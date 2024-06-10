/**
 **
 ** (C)2024 aks
 **/
#include <whsd-form-decoder.h>
#include <whsd-http-server.h>
#include <multipartparser.h>

static void destructor__whsd_upload_form_t(void *data) {
    whsd_upload_form_t *form = (whsd_upload_form_t *)data;

    wstk_mem_deref(form->file_encoding);
    wstk_mem_deref(form->body_encoding);
    wstk_mem_deref(form->model_name);
    wstk_mem_deref(form->opts);
    wstk_mem_deref(form->body);
}

static int on_hdr_name_callback(multipartparser *p, const char *data, size_t len) {
    whsd_upload_form_t *from = (whsd_upload_form_t *)p->data;
    wstk_pl_t src = {data, len};

    if(wstk_pl_strcasecmp(&src, "Content-Disposition") == 0) {
        from->_param_id = 1;
    } else if(wstk_pl_strcasecmp(&src, "Content-Type") == 0) {
        from->_param_id = 2;
    }

    return 0;
}

static int on_hdr_value_callback(multipartparser *p, const char *data, size_t len) {
    whsd_upload_form_t *from = (whsd_upload_form_t *)p->data;
    wstk_pl_t src = {data, len};
    wstk_pl_t field_name = { 0 };

    if(from->_param_id == 1) {
        if(wstk_regex(data, len, "form-data; name=\"[^\"]+\"", &field_name) == WSTK_STATUS_SUCCESS) {
            if(wstk_pl_strcasecmp(&field_name, "opts") == 0) {
                from->_param_id = 10;
            } else if(wstk_pl_strcasecmp(&field_name, "model") == 0) {
                from->_param_id = 11;
            } else if(wstk_pl_strcasecmp(&field_name, "file") == 0) {
                if(wstk_regex(data, len, "filename=\"[^\"]+\"", &field_name) == WSTK_STATUS_SUCCESS) {
                    if(wstk_pl_strstr(&field_name, ".mp3")) {
                        wstk_str_dup2(&from->file_encoding, "mp3");
                    } else if(wstk_pl_strstr(&field_name, ".wav")) {
                        wstk_str_dup2(&from->file_encoding, "wav");
                    } else if(wstk_pl_strstr(&field_name, ".txt")) {
                        wstk_str_dup2(&from->file_encoding, "txt");
                    } else {
                        if(field_name.l <= 255) { wstk_pl_strdup(&from->file_encoding, &field_name); }
                    }
                    from->_param_id = 12;
                }
            }
        }
    } else if(from->_param_id == 2) {
        if(wstk_pl_strcasecmp(&src, "text/plain") == 0) {
            wstk_str_dup2(&from->body_encoding, "txt");
        } else if(wstk_pl_strcasecmp(&src, "application/octet-stream") == 0) {
            wstk_str_dup2(&from->body_encoding, "bin");
        } else {
            if(src.l <= 255) { wstk_pl_strdup(&from->body_encoding, &src); }
        }
        from->_param_id = 20; // body
    }

    return 0;
}

static int on_data_callback(multipartparser *p, const char *data, size_t len) {
    whsd_upload_form_t *from = (whsd_upload_form_t *)p->data;
    wstk_pl_t src = {data, len};

    if(from->_param_id == 10) {
        if(src.l <= 16384) { wstk_pl_strdup(&from->opts, &src); }
        from->_param_id = 0;
    } else if(from->_param_id == 11) {
        if(src.l <= 255) { wstk_pl_strdup(&from->model_name, &src); }
        from->_param_id = 0;
    } else if(from->_param_id == 20) {
        wstk_mbuf_write_pl(from->body, &src);
    }

    return 0;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t whsd_upload_form_decode(whsd_upload_form_t **form, wstk_http_msg_t *msg, char *body, size_t body_len) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    whsd_upload_form_t *form_local = NULL;
    multipartparser mp_parser = { 0 };
    multipartparser_callbacks mp_callbacks = { 0 };
    wstk_pl_t boundary = { 0 };
    char *boundary_str = NULL;
    size_t nparts = 0;

    if(wstk_regex(msg->ctype.p, msg->ctype.l, "boundary=[^]*", &boundary) == WSTK_STATUS_SUCCESS) {
        status = wstk_mem_zalloc((void *)&form_local, sizeof(whsd_upload_form_t), destructor__whsd_upload_form_t);
        if(status != WSTK_STATUS_SUCCESS) {
            log_error("Unable to allocate memory");
            goto out;
        }

        if((status = wstk_mbuf_alloc(&form_local->body, msg->clen)) != WSTK_STATUS_SUCCESS) {
            log_error("Unable to allocate memory");
            goto out;
        }

        if((status = wstk_pl_strdup(&boundary_str, &boundary)) != WSTK_STATUS_SUCCESS) {
            log_error("Unable to allocate memory");
            goto out;
        }

        multipartparser_callbacks_init(&mp_callbacks);
        mp_callbacks.on_data = &on_data_callback;
        mp_callbacks.on_header_field = &on_hdr_name_callback;
        mp_callbacks.on_header_value = &on_hdr_value_callback;

        multipartparser_init(&mp_parser, boundary_str);
        mp_parser.data = form_local;

        nparts = multipartparser_execute(&mp_parser, &mp_callbacks, body, body_len);
        status = (nparts > 0 ? WSTK_STATUS_SUCCESS : WSTK_STATUS_FALSE);

    } else {
        status = WSTK_STATUS_FALSE;
    }

out:
    wstk_mem_deref(boundary_str);

    if(status == WSTK_STATUS_SUCCESS) {
        *form = form_local;
    } else {
        wstk_mem_deref(form_local);
    }

    return status;
}
