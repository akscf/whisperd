/**
 **
 ** (C)2024 aks
 **/
#include <whsd-http-server.h>
#include <whsd-form-decoder.h>
#include <whsd-config.h>

#define WHSD_CHARSET            "UTF-8"
#define WHSD_MAX_CONTENT_LEN    1048510

typedef struct {
    whsd_global_t                   *whsd_glb;
    wstk_httpd_t                    *httpd;
    char                            *json_ctype;
    wstk_sockaddr_t                 laddr;
    bool                            fl_destroyed;
} whsd_http_server_manager_t;

static whsd_http_server_manager_t *manager = NULL;

static char *pls2str(char *str, size_t str_len);
static void json_response_ok(wstk_http_conn_t *conn, char *msg, size_t msg_len);
static void json_response_error(wstk_http_conn_t *conn, char *msg, size_t msg_len);

// --------------------------------------------------------------------------------------------------------------
static void destructor__whsd_http_server_manager_t(void *data) {
    whsd_http_server_manager_t *obj = (whsd_http_server_manager_t *)data;

    if(!obj || obj->fl_destroyed) {
        return;
    }
    obj->fl_destroyed = true;

#ifdef WHISPERD_DEBUG
    log_debug("stopping web-service...");
#endif

    if(obj->httpd) {
        obj->httpd = wstk_mem_deref(obj->httpd);
    }

    wstk_mem_deref(obj->json_ctype);
}

/* validate bearer token */
static void whsd_auth_handler(wstk_httpd_auth_request_t *req, wstk_httpd_auth_response_t *rsp) {
    bool permitted = false;

    if(!manager || manager->fl_destroyed)  {
        return;
    }

    if(wstk_str_is_empty(manager->whsd_glb->http_server->secret)) {
        permitted = true;
    } else {
        permitted = (req->token && wstk_str_equal(manager->whsd_glb->http_server->secret, req->token, true));
    }

    rsp->permitted = permitted;
}

/* preprocessing request and call the module handler */
static void endpont_pre_handler(wstk_http_conn_t *conn, wstk_http_msg_t *msg, void *udata) {
    whsd_http_endpoint_handler_t handler = (whsd_http_endpoint_handler_t )udata;
    wstk_status_t status = 0;
    whsd_endpoint_request_t req = {0};
    whsd_endpoint_response_t rsp = {0};
    wstk_httpd_sec_ctx_t sec_ctx = {0};
    whsd_upload_form_t *upload_form = NULL;
    wstk_mbuf_t *body_tmp_buf = NULL;
    wstk_mbuf_t *rsp_buf = NULL;
    wstk_hash_t *params_map = NULL;
    char *body_ptr = NULL;
    size_t body_len = 0;
    bool sys_erro = false;

    if(!handler) {
        log_error("oops! (handler == null)");
        return;
    }
    if(!manager || manager->fl_destroyed)  {
        return;
    }

    if(msg->clen > WHSD_MAX_CONTENT_LEN) {
        wstk_httpd_ereply(conn, 400, "Content is too big");
        return;
    }
    if(wstk_pl_strcasecmp(&msg->method, "post") != 0) {
        wstk_httpd_ereply(conn, 400, "Bad request (expected method: POST)");
        return;
    }
    if(!wstk_pl_strstr(&msg->ctype, "multipart/form-data")) {
        wstk_httpd_ereply(conn, 400, "Bad request (expected type: multipart/form-data)");
        return;
    }

    /* authenticate */
    wstk_httpd_autheticate(conn, msg, &sec_ctx);
    if(!sec_ctx.permitted) {
        wstk_httpd_ereply(conn, 401, "Access not allowed");
        return;
    }

    /* load the rest of the content */
    if(msg->clen > wstk_mbuf_left(conn->buffer)) {
        if(wstk_mbuf_alloc(&body_tmp_buf, msg->clen) != WSTK_STATUS_SUCCESS) {
            log_error("Unable to allocate memory");
            wstk_httpd_ereply(conn, 500, NULL);
            goto out;
        }
        if(wstk_httpd_conn_rdlock(conn, true) == WSTK_STATUS_SUCCESS) {
            while(true) {
                status = wstk_httpd_read(conn, body_tmp_buf, WSTK_RD_TIMEOUT(msg->clen));
                if(manager->fl_destroyed || body_tmp_buf->pos >= msg->clen)  {
                    break;
                }
                if(status == WSTK_STATUS_CONN_DISCON || status == WSTK_STATUS_CONN_EXPIRE)  {
                    break;
                }
            }
            wstk_httpd_conn_rdlock(conn, false);
        } else {
            status = WSTK_STATUS_LOCK_FAIL;
            log_error("Unable to lock connection (rdlock)");
        }
        if(!WSTK_RW_ACCEPTABLE(status)) {
            log_error("Unable to read the whole body (status=%d)", (int)status);
            wstk_httpd_ereply(conn, 500, NULL);
            goto out;
        }
        body_ptr = (char *)body_tmp_buf->buf;
        body_len = body_tmp_buf->end;
    } else {
        body_ptr = (char *)wstk_mbuf_buf(conn->buffer);
        body_len = wstk_mbuf_left(conn->buffer);
    }

    if(!body_len) {
        log_error("Empty content (body_len == 0)");
        wstk_httpd_ereply(conn, 400, "Empty content");
        goto out;
    }

    /* parse form */
    if(whsd_upload_form_decode(&upload_form, msg, body_ptr, body_len) != WSTK_STATUS_SUCCESS) {
        log_error("Unable to decode upload from");
        wstk_httpd_ereply(conn, 400, "Unable to decode upload from");
        goto out;
    }
    if(body_tmp_buf) {
        body_tmp_buf = wstk_mem_deref(body_tmp_buf);
    }

    /* request params */
    if(wstk_hash_init(&params_map) != WSTK_STATUS_SUCCESS) {
        log_error("Unable to decode upload from");
        wstk_httpd_ereply(conn, 400, "Unable to decode upload from");
        goto out;
    }
    if(upload_form->opts) {
        cJSON *jparams = cJSON_Parse(upload_form->opts);
        if(jparams) {
            cJSON *item = jparams->child;
            while(item) {
                if(item->string) {
                    if(cJSON_IsString(item)) {
                        char *val = wstk_str_dup(cJSON_GetStringValue(item));
                        wstk_hash_insert_ex(params_map, item->string, val, true);
                    } else if(cJSON_IsNumber(item)) {
                        char *val = NULL;
                        wstk_sdprintf(&val, "%d", (int)(cJSON_GetNumberValue(item)));
                        wstk_hash_insert_ex(params_map, item->string, val, true);
                    } else if(cJSON_IsTrue(item)) {
                        char *val = wstk_str_dup("true");
                        wstk_hash_insert_ex(params_map, item->string, val, true);
                    } else if(cJSON_IsFalse(item)) {
                        char *val = wstk_str_dup("false");
                        wstk_hash_insert_ex(params_map, item->string, val, true);
                    }
                }
                item = item->next;
            }
        }
        cJSON_Delete(jparams);
    }

    if(upload_form->model_name) {
        char *val = wstk_str_dup(upload_form->model_name);
        wstk_hash_insert_ex(params_map, "model", val, true);
    }

#ifdef WHISPERD_DEBUG
    log_debug("from.opts............[%s]", upload_form->opts);
    log_debug("from.model...........[%s]", upload_form->model_name);
    log_debug("from.file_encoding...[%s]", upload_form->file_encoding);
    log_debug("from.body_encoding...[%s]", upload_form->body_encoding);
    log_debug("from.body_len........[%d]", upload_form->body->end);
#endif

    /* call handler */
    if(wstk_mbuf_alloc(&rsp_buf, 2048) != WSTK_STATUS_SUCCESS) {
        log_error("Unable to allocate memory");
        sys_erro = true; goto out;
    }

    rsp.text = rsp_buf;
    rsp.error = false;
    req.params = params_map;
    req.buffer = upload_form->body;
    req.format = upload_form->file_encoding;
    upload_form->body->pos = 0;

    handler(&req, &rsp);

    if(!rsp.error) {
        json_response_ok(conn, rsp_buf->buf, rsp_buf->end);
    } else {
        json_response_error(conn, rsp_buf->buf, rsp_buf->end);
    }
out:
    wstk_mem_deref(rsp_buf);
    wstk_mem_deref(body_tmp_buf);
    wstk_mem_deref(upload_form);
    wstk_mem_deref(params_map);
    wstk_httpd_sec_ctx_clean(&sec_ctx);
}

static char *pls2str(char *str, size_t str_len) {
    char *jstr = NULL;

    if(str && str_len) {
        char *cstr  = wstk_str_ndup(str, str_len);
        if(cstr) {
            cJSON *obj = cJSON_CreateString(cstr);
            if(obj) {
                char *xstr = cJSON_PrintUnformatted(obj);
                if(xstr) { jstr = wstk_str_wrap(xstr); }
                cJSON_Delete(obj);
            }
        }
        wstk_mem_deref(cstr);
    }

    return jstr;
}

static void json_response_error(wstk_http_conn_t *conn, char *msg, size_t msg_len) {
    char *jstr = NULL;

    jstr = pls2str(msg, msg_len);
    if(jstr) {
        wstk_httpd_creply(conn, 200, NULL, manager->json_ctype, "{\"error\": %s }", jstr);
    } else {
        wstk_httpd_creply(conn, 200, NULL, manager->json_ctype, "{\"error\": \"undefined error\" }");
    }

    wstk_mem_deref(jstr);
}


static void json_response_ok(wstk_http_conn_t *conn, char *msg, size_t msg_len) {
    char *jstr = NULL;

    jstr = pls2str(msg, msg_len);
    if(jstr) {
        wstk_httpd_creply(conn, 200, NULL, manager->json_ctype, "{\"text\": %s }", jstr);
    } else {
        wstk_httpd_creply(conn, 200, NULL, manager->json_ctype, "{\"text\": \"\" }");
    }

    wstk_mem_deref(jstr);
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t whsd_http_server_init(whsd_global_t *whsd_glb) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    const whsd_config_entry_http_server_t *http_server = whsd_glb->http_server;

    status = wstk_mem_zalloc((void *)&manager, sizeof(whsd_http_server_manager_t), destructor__whsd_http_server_manager_t);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    manager->whsd_glb = whsd_glb;

#ifdef WHISPERD_DEBUG
    log_debug("configuring web-service...");
#endif

    if(wstk_str_is_empty(http_server->address)) {
        log_error("Invalid web-service setting: address (%s)", http_server->address);
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }
    if(http_server->port <= 0) {
        log_error("Invalid web-service setting: port (%d)", http_server->port);
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    status = wstk_sa_set_str(&manager->laddr, http_server->address, http_server->port);
    if(status != WSTK_STATUS_SUCCESS) {
        log_error("Unbale to parse listening address");
        goto out;
    }

    status = wstk_httpd_create(&manager->httpd, &manager->laddr, http_server->max_conns, http_server->max_idle, WHSD_CHARSET, NULL, NULL, false);
    if(status != WSTK_STATUS_SUCCESS) {
        log_error("Unable to create httpd instance (%d)", (int)status);
        goto out;
    }

    status = wstk_httpd_set_ident(manager->httpd, "whsd/1.x");
    if(status != WSTK_STATUS_SUCCESS) {
        log_error("Unable to set server ident");
        goto out;
    }

    status = wstk_httpd_set_authenticator(manager->httpd, whsd_auth_handler, true);
    if(status != WSTK_STATUS_SUCCESS) {
        log_error("Unable to set authenticator");
        goto out;
    }

    status = wstk_sdprintf(&manager->json_ctype, "application/json;charset=%s", WHSD_CHARSET);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    if(status == WSTK_STATUS_SUCCESS) {
        log_notice("Services available on: %s:%d", http_server->address, http_server->port);
    }

out:
    return status;
}

wstk_status_t whsd_http_server_start() {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!manager || manager->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

#ifdef WHISPERD_DEBUG
    log_debug("starting web-service...");
#endif

    status = wstk_httpd_start(manager->httpd);
    if(status != WSTK_STATUS_SUCCESS) {
        log_error("Unable to start httpd (%d)", (int)status);
    }

    return status;
}

wstk_status_t whsd_http_server_shutdown() {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!manager || manager->fl_destroyed) {
        return WSTK_STATUS_SUCCESS;
    }

    wstk_mem_deref(manager);

    return status;
}

wstk_status_t whsd_http_ep_register(const char *name, whsd_http_endpoint_handler_t handler) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!name || !handler) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(!manager || manager->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    status = wstk_httpd_register_servlet(manager->httpd, name, endpont_pre_handler, handler, false);

#ifdef WHISPERD_DEBUG
    if(status == WSTK_STATUS_SUCCESS) {
        log_debug("Endpoint registered: %s (handler=%p)", name, handler);
    } else {
        log_debug("Unble to register endpoint: %d", (int)status);
    }
#endif

    return status;
}

wstk_status_t whsd_http_ep_unregister(const char *name) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!name) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(!manager || manager->fl_destroyed) {
        return WSTK_STATUS_DESTROYED;
    }

    status = wstk_httpd_unregister_servlet(manager->httpd, name);

#ifdef WHISPERD_DEBUG
    log_debug("Endpoint unregistered: %s", name);
#endif

    return status;
}

char *whsd_http_ep_req_param_get(whsd_endpoint_request_t *req, const char *name) {
    if(!req || !name) {
        return NULL;
    }
    if(!req->params) {
        return NULL;
    }
    return wstk_hash_find(req->params, name);
}
