/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#ifndef WHISPERD_WHISPER_H
#define WHISPERD_WHISPER_H
#include <whisperd.h>
#include <whisperd-audio.h>
#include <whisperd-model.h>

#include <whisper.h>
#define WD_WHISPER_SAMPLERATE   WHISPER_SAMPLE_RATE
#define WD_WORKERS_PER_MODEL    8

typedef struct {
    wd_audio_buffer_t       *audio_buffer_ref;
    wd_model_description_t  *model_descr_ref;
    mbuf_t                  *text_buffer;
    char                    *language;
    char                    *model_name;
    char                    *eparams;
    wd_list_t               *segments;     // not used
    uint32_t                wop_max_tokens;
    uint32_t                wop_speed_up;
    uint32_t                wop_translate;
    uint32_t                wop_single_segment;
    uint32_t                fl_destroyed;
    uint32_t                fl_abort;
} wd_whisper_trans_ctx_t;

typedef struct {
    void        *wobj;
    uint32_t    busy;
    uint32_t    id;
} wd_whisper_worker_t;

wd_status_t wd_whisper_worker_alloc(wd_whisper_worker_t **worker, wd_model_description_t *model);
wd_status_t wd_whisper_trans_ctx_alloc(wd_whisper_trans_ctx_t **ctx, char *model_name);
wd_status_t wd_whisper_transcript(wd_whisper_trans_ctx_t *tctx);

#endif

