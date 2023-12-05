/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include <whisperd-whisper.h>
#include <whisperd-config.h>
#include <time.h>
#include <whisper.h>
#include <stdexcept>

extern whisperd_global_t *wd_global;

class WhisperWorker;
class WhisperWorker {

    public:
        WhisperWorker(uint32_t id, wd_model_description_t *mdescr) {
            if(!mdescr) {
                throw std::runtime_error("model == NULL");
            }
            wid = id;
            model_descr = mdescr;

            whisper_ctx = whisper_init_from_file(mdescr->path);
            if(whisper_ctx == NULL) {
                throw std::runtime_error("whisper_ctx == NULL");
            }
        }

        ~WhisperWorker() {
            if(whisper_ctx) {
                whisper_free(whisper_ctx);
            }

            fl_decoder_active = false;
        }

        wd_status_t transcript(wd_whisper_trans_ctx_t *tctx) {
            wd_status_t status = WD_STATUS_SUCCESS;
            uint64_t tm_start=0, tm_end=0;
            whisper_full_params wparams;
            char *lang = NULL;
            int segments = 0;

            if(!tctx || !tctx->audio_buffer_ref || !tctx->text_buffer) {
                log_error("Invalid arguments");
                return WD_STATUS_FALSE;
            }

            fl_decoder_active = true;

            if(tctx->language) {
                lang = tctx->language;
                if(model_descr->language && str_casecmp(lang, model_descr->language) != 0) {
                    lang = model_descr->language;
                }
            } else {
                lang = model_descr->language;
            }

            log_debug("[%x] transcript: model=%s, lang=%s, samples=%i, wop_single_segment=%i, wop_translate=%i, wop_max_tokens=%i",
                      wid, model_descr->name, lang, tctx->audio_buffer_ref->samples, tctx->wop_single_segment, tctx->wop_translate, tctx->wop_max_tokens
            );

            wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
            wparams.print_progress   = false;
            wparams.print_special    = false;
            wparams.print_realtime   = false;
            wparams.print_timestamps = false;
            wparams.translate        = tctx->wop_translate;
            wparams.single_segment   = tctx->wop_single_segment;
            wparams.speed_up         = tctx->wop_speed_up;
            wparams.n_threads        = wd_global->whisper_worker->max_threads;
            wparams.max_tokens       = (tctx->wop_max_tokens != 0 ? tctx->wop_max_tokens : wd_global->whisper_worker->max_tokens);
            wparams.language         = lang;
            wparams.audio_ctx        = 0;

            wparams.encoder_begin_callback_user_data = tctx;
            wparams.encoder_begin_callback = (whisper_encoder_begin_callback) WhisperWorker::wcb_encoder_begin;


            tm_start = time_ms_now();
            log_debug("[%x] --> transcription-start", wid);

            if(tctx->segments) {
                log_warn("not yet implemenetd");
                status = WD_STATUS_FALSE; goto done;
            } else {
                if(whisper_full(whisper_ctx, wparams, tctx->audio_buffer_ref->data, tctx->audio_buffer_ref->samples) != 0) {
                    log_error("whisper_full() fail");
                    status = WD_STATUS_FALSE; goto done;
                }
            }

            tm_end = time_ms_now();
            log_debug("[%x] --> transcription-end (time=%u)", wid, (uint32_t)(tm_end - tm_start));

            if(tctx->fl_abort || wd_global->fl_shutdown) {
                goto done;
            }

            if((segments = whisper_full_n_segments(whisper_ctx))) {
                for(uint32_t i = 0; i < segments; ++i) {
                    const char *text = whisper_full_get_segment_text(whisper_ctx, i);
                    mbuf_write_str(tctx->text_buffer, text);
                }
            }

        done:
            fl_decoder_active = false;
            return status;
        }

    private:
        wd_model_description_t  *model_descr;
        struct whisper_context  *whisper_ctx = NULL;
        bool fl_decoder_active = false;
        uint32_t wid = 0;
        uint32_t refs = 0;

        uint64_t time_ms_now() {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            return (((uint64_t) tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
        }

        static bool wcb_encoder_begin(struct whisper_context *ctx, struct whisper_state *state, void *udata) {
            wd_whisper_trans_ctx_t *tctx = (wd_whisper_trans_ctx_t *)udata;
            return (tctx->fl_abort || wd_global->fl_shutdown ? false : true);
        }

};

// -------------------------------------------------------------------------------------------------------------------------------------------------------------
extern "C" {
    wd_status_t wd_whisper_worket_init_obj(wd_whisper_worker_t *worker, wd_model_description_t *model) {
        wd_status_t status = WD_STATUS_SUCCESS;
        try {
            worker->wobj = new WhisperWorker(worker->id, model);
        } catch (std::exception& e) {
            status = WD_STATUS_FALSE;
        }
        return status;
    }

    wd_status_t wd_whisper_worker_destroy_obj(wd_whisper_worker_t *worker) {
        wd_status_t status = WD_STATUS_SUCCESS;
        if(worker) {
            WhisperWorker *wobj = (WhisperWorker *) worker->wobj;
            delete wobj;
        }
        return status;
    }

    wd_status_t wd_whisper_worker_transcript(wd_whisper_worker_t *worker, wd_whisper_trans_ctx_t *tctx) {
        wd_status_t status = WD_STATUS_SUCCESS;
        WhisperWorker *wobj = (WhisperWorker *) worker->wobj;
        if(worker->wobj) {
            worker->busy = true;
            status = wobj->transcript(tctx);
            worker->busy = false;
        }
        return status;
    }
}

