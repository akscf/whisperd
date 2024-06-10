/**
 **
 ** (C)2024 aks
 **/
#ifndef WHSD_HTTP_SERVER_H
#define WHSD_HTTP_SERVER_H
#include <whsd-core.h>
#include <whsd-config.h>

typedef struct {
    const char      *format;    // content type: mp3/vaw/...
    wstk_mbuf_t     *buffer;    // audio content
    wstk_hash_t     *params;    // params (name => val)
} whsd_endpoint_request_t;

typedef struct {
    wstk_mbuf_t     *text;      // response or error text
    bool            error;      // true if error
} whsd_endpoint_response_t;

typedef void (*whsd_http_endpoint_handler_t)(whsd_endpoint_request_t *req, whsd_endpoint_response_t *rsp);

wstk_status_t whsd_http_ep_register(const char *name, whsd_http_endpoint_handler_t handler);
wstk_status_t whsd_http_ep_unregister(const char *name);
char *whsd_http_ep_req_param_get(whsd_endpoint_request_t *req, const char *name);

wstk_status_t whsd_http_server_init(whsd_global_t *whsd_glb);
wstk_status_t whsd_http_server_shutdown();
wstk_status_t whsd_http_server_start();

#endif
