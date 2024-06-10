/**
 **
 ** (C)2024 aks
 **/
#ifndef MOD_WHISPER_CPP_H
#define MOD_WHISPER_CPP_H
#include <whisperd.h>
#include <whisper.h>

#define MOD_CFG_VERSION 1
#define MOD_APP_VERSION "1.0.10062024"

typedef struct {
    wstk_mutex_t    *mutex;
    wstk_hash_t     *pools;
    wstk_hash_t     *models;        // name|alias -> whisper_model_descr_t
    char            *models_path;
    const char      *endpoint;      // refs to the server endpoint
    uint32_t        whisper_threads;
    uint32_t        whisper_tokens;
    uint32_t        whisper_gpu_dev;
    bool            whisper_use_gpu;
    bool            whisper_flash_attn;
    bool            unreg_hnd;
    bool            fl_shutdown;
} whisper_cpp_global_t;

typedef struct {
    char    *name;
    char    *alias;
    char    *path;
} whisper_model_descr_t;

typedef struct {
    uint32_t                id;
    whsd_audio_buffer_t     *audio_buffer;
    wstk_mbuf_t             *text_buffer;
    char                    *language;
    uint32_t                wop_max_tokens;
    uint32_t                wop_translate;
    uint32_t                wop_single_segment;
    bool                    do_break;
    bool                    error_rsp;
} whisper_trans_ctx_t;

typedef struct {
    uint32_t                id;
    uint32_t                gpu_dev;
    bool                    use_gpu;
    bool                    flash_attn;
    char                    *model_file;
} whisper_worker_cfg_t;

typedef struct {
    struct whisper_context *wctx;
    bool                   do_break;
    bool                   busy;
} whisper_worker_t;

/* misc.c */
wstk_status_t mod_whisper_global_init(whisper_cpp_global_t **out, const char *endpoint);
wstk_status_t mod_whisper_load_config();

wstk_status_t mod_whisper_model_descr_alloc(whisper_model_descr_t **out, char *name, char *alias, char *path);
wstk_status_t mod_whisper_models_registry_add(whisper_model_descr_t *descr);
whisper_model_descr_t *mod_whisper_models_registry_lookup(char *name);

wstk_status_t mod_whisper_worker_create(whisper_worker_t **out, whisper_worker_cfg_t *cfg);
wstk_status_t mod_whisper_worker_transcribe(whisper_worker_t *wrk, whisper_trans_ctx_t *tctx);

#endif
