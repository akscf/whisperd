/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#ifndef WHISPERD_RE_H
#define WHISPERD_RE_H
#include <re/re.h>

#define LIBRE_SUCCESS           0

typedef struct mbuf mbuf_t;
typedef struct sa sa_t;
typedef struct http_sock http_sock_t;
typedef struct http_conn http_conn_t;
typedef struct http_msg http_msg_t;
typedef struct websock websock_t;
typedef struct http_hdr http_hdr_t;
typedef struct pl pl_t;
typedef struct odict odict_t;
typedef struct odict_entry odict_entry_t;

#define mbuf_clean(mb) mb->pos=0; mbuf_fill(mb, 0x0, mb->end); mb->end=mb->pos=0;



#endif
