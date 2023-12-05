/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#ifndef WHISPERD_PARSER_H
#define WHISPERD_PARSER_H
#include <whisperd.h>
#include <multipartparser.h>

typedef struct {
    uint32_t    _param_id;
    char        *opts;              // extra options
    char        *model_name;        // whisper model name
    char        *file_encoding;     // mp3, wav, l16, f32
    char        *body_encoding;     // octet-stream, base64
    mbuf_t      *body;
} wd_http_form_params_v1_t;

wd_status_t wd_parse_form_openai(wd_http_form_params_v1_t **params, const http_msg_t *msg, pl_t *boundary);

#endif

