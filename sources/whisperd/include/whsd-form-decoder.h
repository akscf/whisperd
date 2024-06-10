/**
 **
 ** (C)2024 aks
 **/
#ifndef WHSD_FORM_DECODER_H
#define WHSD_FORM_DECODER_H
#include <whsd-core.h>

typedef struct {
    char        *opts;              // json extra options
    char        *model_name;        // whisper model name
    char        *file_encoding;     // mp3, wav, l16, f32
    char        *body_encoding;     // octet-stream, base64
    wstk_mbuf_t *body;              // the whole decoded file body
    uint32_t    _param_id;          // private
} whsd_upload_form_t;

wstk_status_t whsd_upload_form_decode(whsd_upload_form_t **form, wstk_http_msg_t *msg, char *body, size_t body_len);

#endif
