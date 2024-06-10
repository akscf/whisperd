/**
 **
 ** (C)2024 aks
 **/
#include <whisperd.h>
#include <mod-whisper-cpp.h>

whisper_cpp_global_t *globals;

/* http handler */
static void req_handler(whsd_endpoint_request_t *req, whsd_endpoint_response_t *rsp) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    whsd_audio_buffer_t *audio_buf = NULL;

    if(globals->fl_shutdown) {
        return;
    }

    if(wstk_str_equal(req->format, "wav", false)) {
        if((status = whsd_codecs_decode_wav(&audio_buf, WHISPER_SAMPLE_RATE, req->buffer)) != WSTK_STATUS_SUCCESS) {
            log_error("Unable to decode audio (status=%d)", (int)status);
            wstk_mbuf_printf(rsp->text, "Unable to decode audio");
            rsp->error = true; goto out;
        }
    } else if(wstk_str_equal(req->format, "mp3", false)) {
        if((status = whsd_codecs_decode_mp3(&audio_buf, WHISPER_SAMPLE_RATE, req->buffer)) != WSTK_STATUS_SUCCESS) {
            log_error("Unable to decode audio (status=%d)", (int)status);
            wstk_mbuf_printf(rsp->text, "Unable to decode audio");
            rsp->error = true; goto out;
        }
    } else {
        log_error("Unsupportd format: %s", req->format);
        wstk_mbuf_printf(rsp->text, "Unsupported format (%s)", req->format);
        rsp->error = true; goto out;
    }

#ifdef WHISPERD_DEBUG
    log_debug("audio: samplerate=%d, channels=%d, samples=%d", audio_buf->samplerate, audio_buf->channels, audio_buf->samples);
#endif

    if(true) {
        whisper_trans_ctx_t tctx = {0};
        whisper_worker_cfg_t wcfg = {0};
        whisper_worker_t *worker = NULL;
        whisper_model_descr_t *model_descr = NULL;
        char *model_name = NULL;

        model_name = whsd_http_ep_req_param_get(req, "model");
        if(wstk_str_is_empty(model_name)) {
            wstk_mbuf_printf(rsp->text, "Missing parameter: model");
            rsp->error = true; goto out;
        }

        model_descr = mod_whisper_models_registry_lookup(model_name);
        if(!model_descr) {
            log_error("Unknown model: %s", model_name);
            wstk_mbuf_printf(rsp->text, "Unknonw model: %s", model_name);
            rsp->error = true; goto out;
        }

        if(wstk_hash_is_empty(globals->pools)) {
            wcfg.id = 0;
            wcfg.gpu_dev = globals->whisper_gpu_dev;
            wcfg.use_gpu = globals->whisper_use_gpu;
            wcfg.flash_attn = globals->whisper_flash_attn;
            wcfg.model_file = model_descr->path;

            if((status = mod_whisper_worker_create(&worker, &wcfg)) != WSTK_STATUS_SUCCESS) {
                wstk_mbuf_printf(rsp->text, "Unable to create worker");
                rsp->error = true; goto out;
            }

            tctx.text_buffer = rsp->text;
            tctx.audio_buffer = audio_buf;
            tctx.language = whsd_http_ep_req_param_get(req, "language");
            tctx.wop_max_tokens = wstk_str_atoi(whsd_http_ep_req_param_get(req, "tokens"));
            tctx.wop_translate = wstk_str_atob(whsd_http_ep_req_param_get(req, "translate"));
            tctx.wop_single_segment = wstk_str_atob(whsd_http_ep_req_param_get(req, "single"));

            mod_whisper_worker_transcribe(worker, &tctx);
            wstk_mem_deref(worker);

        } else {
            //
            // todo
            //
        }
    }
out:
    wstk_mem_deref(audio_buf);
}

/* on module load */
static wstk_status_t mod_load(const char *endpoint) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if((status = mod_whisper_global_init(&globals, endpoint)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = mod_whisper_load_config()) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    status = whsd_http_ep_register(endpoint, req_handler);
    globals->unreg_hnd = (status == WSTK_STATUS_SUCCESS);

    if(status == WSTK_STATUS_SUCCESS) {
        log_notice("Module version (%s)", MOD_APP_VERSION);
        log_notice("whisper-features (%s)", whisper_print_system_info());
    }

out:
    return status;
}

/* on module unload */
static void mod_unload() {
    if(globals)  {
        wstk_mem_deref(globals);
    }
}

WHISPERD_MODULE_DEFINITION(mod_whisper_cpp, mod_load, mod_unload);

