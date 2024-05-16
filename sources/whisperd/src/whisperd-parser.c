/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include <whisperd-parser.h>

static int cb_on_header_name(multipartparser *p, const char *data, size_t len) {
    wd_http_form_params_v1_t *params = (wd_http_form_params_v1_t *)p->data;
    pl_t src = {data, len};

    if(pl_strcasecmp(&src, "Content-Disposition") == 0) {
        params->_param_id = 1;
    } else if(pl_strcasecmp(&src, "Content-Type") == 0) {
        params->_param_id = 2;
    }

    return 0;
}

static int cb_on_header_value(multipartparser *p, const char *data, size_t len) {
    wd_http_form_params_v1_t *params = (wd_http_form_params_v1_t *)p->data;
    pl_t src = {data, len};
    pl_t field_name = { 0 };

    if(params->_param_id == 1) {
        if(re_regex(data, len, "form-data; name=\"[^\"]+\"", &field_name) == 0) {
            if(pl_strcasecmp(&field_name, "opts") == 0) {
                params->_param_id = 10;
            } else if(pl_strcasecmp(&field_name, "model") == 0) {
                params->_param_id = 11;
            } else if(pl_strcasecmp(&field_name, "file") == 0) {
                if(re_regex(data, len, "filename=\"[^\"]+\"", &field_name) == 0) {
                    if(pl_strstr(&field_name, ".mp3")) {
                        str_dup(&params->file_encoding, "mp3");
                    } else if(pl_strstr(&field_name, ".wav")) {
                        str_dup(&params->file_encoding, "wav");
                    } else if(pl_strstr(&field_name, ".txt")) {
                        str_dup(&params->file_encoding, "txt");
                    } else {
                        if(field_name.l <= 255) { pl_strdup(&params->file_encoding, &field_name); }
                    }
                    params->_param_id = 12;
                }
            }
        }
    } else if(params->_param_id == 2) {
        if(pl_strcasecmp(&src, "text/plain") == 0) {
            str_dup(&params->body_encoding, "txt");
        } else if(pl_strcasecmp(&src, "application/octet-stream") == 0) {
            str_dup(&params->body_encoding, "bin");
        } else {
            if(src.l <= 255) { pl_strdup(&params->body_encoding, &src); }
        }
        params->_param_id = 20; // body
    }

    return 0;
}

static int cb_on_part_data(multipartparser *p, const char *data, size_t len) {
    wd_http_form_params_v1_t *params = (wd_http_form_params_v1_t *)p->data;
    pl_t src = {data, len};

    if(params->_param_id == 10) {
        if(src.l <= 16384) { pl_strdup(&params->opts, &src); }
        params->_param_id = 0;
    } else if(params->_param_id == 11) {
        if(src.l <= 255) { pl_strdup(&params->model_name, &src); }
        params->_param_id = 0;
    } else if(params->_param_id == 20) {
        mbuf_write_pl(params->body, &src);
    }

    return 0;
}

static void destructor_wd_http_form_params_v1_t(void *data) {
    wd_http_form_params_v1_t *ptr = (wd_http_form_params_v1_t *)data;

    mem_deref(ptr->opts);
    mem_deref(ptr->file_encoding);
    mem_deref(ptr->body_encoding);
    mem_deref(ptr->model_name);
    mem_deref(ptr->body);
}

// ----------------------------------------------------------------------------------------------------------------------------------
// public
// ----------------------------------------------------------------------------------------------------------------------------------
/**
 ** fill-in whisper params by the form
 **/
wd_status_t wd_parse_form_openai(wd_http_form_params_v1_t **params, const http_msg_t *msg, pl_t *boundary) {
    wd_status_t status = WD_STATUS_SUCCESS;
    multipartparser_callbacks callbacks = { 0 };
    multipartparser parser = { 0 };
    wd_http_form_params_v1_t *result = NULL;
    char *boundary_str = NULL;
    char *body = (char *)(msg->mb->buf + msg->mb->pos);
    uint32_t body_len = msg->clen;
    size_t nparsed = 0;

    result = mem_zalloc(sizeof(wd_http_form_params_v1_t), destructor_wd_http_form_params_v1_t);
    if(!result) {
        log_error("mem fail");
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }

    result->body = mbuf_alloc(body_len);
    if(!result->body) {
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }

    pl_strdup(&boundary_str, boundary);

    multipartparser_callbacks_init(&callbacks);
    callbacks.on_data = &cb_on_part_data;
    callbacks.on_header_field = &cb_on_header_name;
    callbacks.on_header_value = &cb_on_header_value;

    multipartparser_init(&parser, boundary_str);
    parser.data = result;

    nparsed = multipartparser_execute(&parser, &callbacks, body, body_len);
    status = (nparsed == body_len ? WD_STATUS_SUCCESS : WD_STATUS_FALSE);

out:
    if(status != WD_STATUS_SUCCESS) {
        mem_deref(result);
        *params = NULL;
    } else {
        *params = result;
    }
    mem_deref(boundary_str);
    return status;
}
