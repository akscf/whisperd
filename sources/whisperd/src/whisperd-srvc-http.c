/**
 *
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include <whisperd-srvc-http.h>
#include <whisperd-parser.h>
#include <whisperd-audio.h>
#include <whisperd-whisper.h>

extern whisperd_global_t *wd_global;

typedef struct {
    uint32_t            req_id_seq;
    http_sock_t         *httpsock;
    http_sock_t         *httpssock;
    mtx_t               *mutex;
    uint32_t            active_treads;
} http_service_conf_t;

typedef struct {
    uint32_t                        req_id;
    http_conn_t                     *conn_ref;
    wd_http_form_params_v1_t        *form_params;
} http_req_async_params_t;

static http_service_conf_t *http_service_conf = NULL;

#define DEFAULT_RSP_BUFFER_SIZE    2048

// ----------------------------------------------------------------------------------------------------------------------------------
static void service_active_treads_increase() {
    mtx_lock(http_service_conf->mutex);
    http_service_conf->active_treads++;
    mtx_unlock(http_service_conf->mutex);
}
static void service_active_treads_decrease() {
    mtx_lock(http_service_conf->mutex);
    if(http_service_conf->active_treads > 0) http_service_conf->active_treads--;
    mtx_unlock(http_service_conf->mutex);
}

static void write_json_response_error(http_conn_t *conn, char *err_msg) {
    mbuf_t *mbuf = mbuf_alloc(DEFAULT_RSP_BUFFER_SIZE);
    if(mbuf) {
        mbuf_write_str(mbuf, "{\"error\":{ \"message\": \"");
        mbuf_write_str(mbuf, err_msg);
        mbuf_write_str(mbuf, "\", \"type\": \"invalid_request_error\", \"param\": null, \"code\": null }}");

        if(http_conn_tcp(conn) == 0) {
            log_warn("socket already closed");
            goto out;
        }
        http_reply(conn, 200, "OK",
                    "Content-Type: application/json;charset=UTF-8\r\n"
                    "Content-Length: %zu\r\n"
                    "\r\n"
                    "%b", mbuf->end, mbuf->buf, mbuf->end
        );
    } else {
        if(http_conn_tcp(conn) == 0) {
            log_warn("socket already closed");
            goto out;
        }
        http_reply(conn, 500, "Server Error", NULL);
    }
out:
    mem_deref(mbuf);
}

static void write_json_response_ok(http_conn_t *conn, char *msg) {
    mbuf_t *mbuf = mbuf_alloc(DEFAULT_RSP_BUFFER_SIZE);
    if(mbuf) {
        mbuf_write_str(mbuf, "{\"text\": \"");
        if(msg) mbuf_write_str(mbuf, msg);
        mbuf_write_str(mbuf, "\"}");

        if(http_conn_tcp(conn) == 0) {
            log_warn("socket already closed");
            goto out;
        }
        http_reply(conn, 200, "OK",
                    "Content-Type: application/json;charset=UTF-8\r\n"
                    "Content-Length: %zu\r\n"
                    "\r\n"
                    "%b", mbuf->end, mbuf->buf, mbuf->end
        );
    } else {
        if(http_conn_tcp(conn) == 0) {
            log_warn("socket already closed");
            goto out;
        }
        http_reply(conn, 500, "Server Error", NULL);
    }
out:
    mem_deref(mbuf);
}

static void write_http_response(http_conn_t *conn, int http_code, char *http_msg, char *html) {
    if(html) {
        http_reply(conn, http_code, http_msg,
                    "Content-Type: text/html;charset=UTF-8\r\n"
                    "Content-Length: %u\r\n"
                    "\r\n"
                    "%s", strlen(html), html
        );
    } else {
        http_reply(conn, http_code, http_msg, NULL);
    }
}

/**
 ** async handler
 **/
static int http_request_handle_async_thread(void *udata) {
    http_req_async_params_t *async_params = (http_req_async_params_t *)udata;
    wd_http_form_params_v1_t *form_params = async_params->form_params;
    wd_model_description_t *model_descr = NULL;
    wd_audio_buffer_t *waudio_buffer = NULL;
    wd_whisper_trans_ctx_t *trans_ctx = NULL;
    uint32_t req_id = async_params->req_id;
    char *response_erro_msg = NULL;
    char *response_ok_msg = NULL;

#ifdef WD_DEBUG_ENABLE_REQ_FORM
    log_debug("wparam.model............[%s]", form_params->model_name);
    log_debug("wparam.file_encoding....[%s]", form_params->file_encoding);
    log_debug("wparam.body_encoding....[%s]", form_params->body_encoding);
    log_debug("wparam.options..........[%s]", form_params->opts);
    log_debug("wparam.body_len.........[%i]", (uint32_t)form_params->body->pos);
#endif // WD_DEBUG_ENABLE_REQ_FORM

    if(!form_params->model_name) {
        re_sdprintf(&response_erro_msg, "Malformed param: model");
        goto rsp;
    }
    if(!form_params->file_encoding) {
        re_sdprintf(&response_erro_msg, "Malformed param: file");
        goto rsp;
    }

    if((model_descr = wd_model_lookup(form_params->model_name)) == NULL) {
        re_sdprintf(&response_erro_msg, "Unknown model: %s", form_params->model_name);
        goto rsp;
    }

    if(str_casecmp(form_params->file_encoding, "mp3") == 0) {
        if(wd_audio_mp3_decode(&waudio_buffer, form_params->body) != WD_STATUS_SUCCESS) {
            re_sdprintf(&response_erro_msg, "Couldn't decode file (mp3)");
            goto rsp;
        }
    } else if(str_casecmp(form_params->file_encoding, "wav") == 0) {
        if(wd_audio_wav_decode(&waudio_buffer, form_params->body) != WD_STATUS_SUCCESS) {
            re_sdprintf(&response_erro_msg, "Couldn't decode file (wav)");
            goto rsp;
        }
    } else {
        log_error("Unsupported file type (%s)", form_params->file_encoding);
        re_sdprintf(&response_erro_msg, "Unsupported media type (%s)", form_params->file_encoding);
    }

    if(waudio_buffer) {
        if(wd_whisper_trans_ctx_alloc(&trans_ctx, form_params->model_name) != WD_STATUS_SUCCESS) {
            re_sdprintf(&response_erro_msg, "Internal error");
            log_error("mem fail"); goto rsp;
        }

        if(form_params->opts) {
            odict_t *odict = NULL;
            const odict_entry_t *dopt = NULL;

            if(json_decode_odict(&odict, 16, form_params->opts, strlen(form_params->opts), 8) == LIBRE_SUCCESS) {
                dopt = odict_lookup(odict, "language");
                if(odict_entry_type(dopt) == ODICT_STRING && str_len(odict_entry_str(dopt)) <= 16) { str_dup(&trans_ctx->language, odict_entry_str(dopt)); }

                dopt = odict_lookup(odict, "max-tokens");
                if(odict_entry_type(dopt) == ODICT_INT) { trans_ctx->wop_max_tokens = odict_entry_int(dopt); }

                dopt = odict_lookup(odict, "translate");
                if(odict_entry_type(dopt) == ODICT_BOOL) { trans_ctx->wop_translate = odict_entry_boolean(dopt); }

                /*dopt = odict_lookup(odict, "markers");
                if(odict_entry_type(dopt) == ODICT_ARRAY) { }
                */
            } else {
                log_warn("Couldn't parse opts");
            }
            mem_deref(odict);
        }

        trans_ctx->model_descr_ref = model_descr;
        trans_ctx->audio_buffer_ref = waudio_buffer;

        if(wd_whisper_transcript(trans_ctx) == WD_STATUS_SUCCESS) {
            uint32_t tlen = trans_ctx->text_buffer->pos;
            if(tlen) {
                trans_ctx->text_buffer->pos = 0;
                mbuf_strdup(trans_ctx->text_buffer, &response_ok_msg, tlen);
            }
        } else {
            re_sdprintf(&response_erro_msg, "Transcription failed (sys_err)");
        }
    }
rsp:
    if(response_erro_msg) {
        write_json_response_error(async_params->conn_ref, response_erro_msg);
    } else {
        write_json_response_ok(async_params->conn_ref, response_ok_msg);
    }
out:
    mem_deref(waudio_buffer);
    mem_deref(trans_ctx);

    mem_deref(response_erro_msg);
    mem_deref(response_ok_msg);

    mem_deref(async_params->form_params);
    mem_deref(async_params);

    service_active_treads_decrease();
    wd_thread_finished();

    return 0;
}

/**
 ** blocking handler
 **/
static void http_request_handler(struct http_conn *conn, const struct http_msg *msg, void *arg) {
    wd_status_t status = WD_STATUS_SUCCESS;
    const wd_config_entry_service_http_t *http_config_entry = wd_global->http_service;
    const http_hdr_t *hdr_ctype = NULL;
    const http_hdr_t *hdr_auth = NULL;
    bool auth_success = false;

    if(http_msg_xhdr_has_value(msg, "Sec-WebSocket-Protocol", "xaudio")) {
        //
        // todo, websocket stream
        //
        write_http_response(conn, 400, "Bad request", "<html><body>Not yet implemented</body></html>");
        goto out;
    }

    if(http_config_entry->max_threads && http_service_conf->active_treads >= http_config_entry->max_threads) {
        log_warn("Threads limit reached (%i)", http_service_conf->active_treads);
        http_reply(conn, 503, "Server busy", NULL);
        goto out;
    }
    if(http_config_entry->max_content_length && msg->clen >= http_config_entry->max_content_length) {
        http_reply(conn, 400, "Content length is too huge", NULL);
        goto out;
    }
    if((http_config_entry->min_content_length && msg->clen < http_config_entry->min_content_length) || (msg->clen <= 0)) {
        http_reply(conn, 400, "Content length is too small", NULL);
        goto out;
    }
    if(pl_strcasecmp(&msg->met, "post") != 0) {
        http_reply(conn, 400, "Bad request", NULL);
        goto out;
    }

    hdr_ctype = http_msg_hdr(msg, HTTP_HDR_CONTENT_TYPE);
    if(!hdr_ctype) {
        http_reply(conn, 400, "Bad request", NULL);
        goto out;
    }

    hdr_auth = http_msg_hdr(msg, HTTP_HDR_AUTHORIZATION);
    if(hdr_auth) {
        pl_t secret = { 0 };
        if(http_config_entry->access_secret) {
            if(re_regex(hdr_auth->val.p, hdr_auth->val.l, "Bearer [^]+", &secret) == 0) {
                auth_success = (pl_strcasecmp(&secret, http_config_entry->access_secret) == 0 ? true : false);
            }
        } else {
            auth_success = true;
        }
    }

    if(!auth_success) {
        const sa_t *peer_sa = NULL;
        char peer_addr[128] = { 0 };

        peer_sa = http_conn_peer(conn);
        sa_ntop(peer_sa, peer_addr, sizeof(peer_addr));

        log_warn("Authentication faild (remote-ip: %s)", (char *) peer_addr);

        write_json_response_error(conn, "Invalid API key");
        goto out;
    }

    /* simulate OpenAI api */
    if(pl_strcasecmp(&msg->path, "/v1/audio/transcriptions") == 0) {
        wd_http_form_params_v1_t *form_params = NULL;
        pl_t boundary = { 0 };

        if(pl_strstr(&hdr_ctype->val, "multipart/form-data")) {
            if(re_regex(hdr_ctype->val.p, hdr_ctype->val.l, "boundary=[^]*", &boundary) == 0) {
                if(wd_parse_form_openai(&form_params, msg, &boundary) == WD_STATUS_SUCCESS) {
                    http_req_async_params_t *async_params = NULL;

                    async_params = mem_zalloc(sizeof(http_req_async_params_t), NULL);
                    if(!async_params) {
                        mem_deref(form_params);
                        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
                    }

                    async_params->req_id = http_service_conf->req_id_seq++;
                    async_params->conn_ref = conn;
                    async_params->form_params = form_params;

                    // increase the refs to be able to catch socket close
                    // at first sight it should workout fine but...
                    mem_ref(conn);

                    service_active_treads_increase();
                    if(wd_thread_launch(http_request_handle_async_thread, async_params) != WD_STATUS_SUCCESS) {
                        service_active_treads_decrease();

                        mem_deref(async_params);
                        mem_deref(form_params);

                        write_json_response_error(conn, "Couldn't processing request (sys_error)");
                    }
                    goto out;
                }
            }
        }
        write_json_response_error(conn, "Malformed request");
    } else {
        write_http_response(conn, 404, "Not found", "<html><body>Service not found</body></html>");
    }
out:
    return;
}

// ----------------------------------------------------------------------------------------------------------------------------------
// public
// ----------------------------------------------------------------------------------------------------------------------------------
wd_status_t wd_service_http_start() {
    wd_status_t status = WD_STATUS_SUCCESS;
    const wd_config_entry_service_http_t *http_config_entry = wd_global->http_service;

    http_service_conf = mem_zalloc(sizeof(http_service_conf_t), NULL);
    if(!http_service_conf) {
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }
    if(mutex_alloc(&http_service_conf->mutex) != LIBRE_SUCCESS) {
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }

    if(!http_config_entry->access_secret) {
        log_warn("Pay attention that 'access_secret' is epmty!");
    }

    if(http_config_entry->port_plain > 0) {
        int err = 0;
        sa_t saddr;

        sa_set_str(&saddr, http_config_entry->address, http_config_entry->port_plain);
        err = http_listen(&http_service_conf->httpsock, &saddr, http_request_handler, NULL);
        if(err != LIBRE_SUCCESS) {
            log_error("Couldn't start http listener (err: %i)\n", err);
            wd_goto_status(WD_STATUS_FALSE, out);
        }

        log_notice("added connector: %s:%i (http)", http_config_entry->address, http_config_entry->port_plain);
    }

    if(http_config_entry->port_ssl > 0) {
        int err = 0;
        sa_t saddr;

        sa_set_str(&saddr, http_config_entry->address, http_config_entry->port_ssl);
        err = https_listen(&http_service_conf->httpssock, &saddr, http_config_entry->cert_file, http_request_handler, NULL);
        if(err != LIBRE_SUCCESS) {
            log_error("Couldn't start https listener (err: %i)\n", err);
            wd_goto_status(WD_STATUS_FALSE, out);
        }

        log_notice("added connector: %s:%i (https)", http_config_entry->address, http_config_entry->port_ssl);
    }

out:
    return status;
}

wd_status_t wd_service_http_stop() {
    if(http_service_conf) {
        if(http_service_conf->active_treads > 0) {
            uint8_t fl_wloop = true;

            log_warn("Waiting for termination '%i' threads...", http_service_conf->active_treads);
            while(fl_wloop) {
                mtx_lock(http_service_conf->mutex);
                fl_wloop = (http_service_conf->active_treads > 0);
                mtx_unlock(http_service_conf->mutex);
                sys_msleep(1000);
            }
        }

        if(http_service_conf->httpsock) {
            mem_deref(http_service_conf->httpsock);
        }
        if(http_service_conf->httpssock) {
            mem_deref(http_service_conf->httpssock);
        }

        mem_deref(http_service_conf->mutex);
        mem_deref(http_service_conf);
    }
    return WD_STATUS_SUCCESS;
}
