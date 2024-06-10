/**
 **
 ** (C)2024 aks
 **/
#include <mod-whisper-cpp.h>

extern whisper_cpp_global_t *globals;

static void destructor__whisper_cpp_global_t(void *data) {
    whisper_cpp_global_t *obj = (whisper_cpp_global_t *)data;
    bool floop = true;

    if(!obj || obj->fl_shutdown) { return; }
    obj->fl_shutdown = true;

    log_debug("Clearing modume resouces...");

    if(obj->unreg_hnd) {
        whsd_http_ep_unregister(obj->endpoint);
    }
    if(obj->pools) {
        wstk_hash_destroy(&obj->pools);
    }
    if(obj->models) {
        wstk_hash_destroy(&obj->models);
    }

    obj->models_path = wstk_mem_deref(obj->models_path);
    obj->mutex = wstk_mem_deref(obj->mutex);

    log_debug("Module resources cleared");
}

static void destructor__whisper_model_descr_t(void *data) {
    whisper_model_descr_t *obj = (whisper_model_descr_t *)data;

    log_debug("Destroying description: %p (name=%s)", obj, obj->name);

    wstk_mem_deref(obj->alias);
    wstk_mem_deref(obj->name);
    wstk_mem_deref(obj->path);

    log_debug("Description destroyed: %p", obj);
}

static void destructor__whisper_worker_t(void *ptr) {
    whisper_worker_t *obj = (whisper_worker_t *)ptr;

    log_debug("Destroying worker: %p (wctx=%p)", obj, obj->wctx);

    if(obj->wctx) {
        whisper_free(obj->wctx);
    }

    log_debug("Worker destroyed: %p", obj);
}

static void destructor__hashtable_values(void *ptr) {
    wstk_mem_deref(ptr);
}

static bool whisper_worker_encoder_begin_callback(struct whisper_context *ctx, struct whisper_state *state, void *udata) {
    whisper_worker_t *wrk = (whisper_worker_t *)udata;
    return (wrk->do_break ? false : true);
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t mod_whisper_model_descr_alloc(whisper_model_descr_t **out, char *name, char *alias, char *path) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    whisper_model_descr_t *descr = NULL;

    status = wstk_mem_zalloc((void *)&descr, sizeof(whisper_model_descr_t), destructor__whisper_model_descr_t);
    if(status != WSTK_STATUS_SUCCESS) {  goto out; }

    descr->name = name;
    descr->path = path;
    descr->alias = alias;

    *out = descr;
out:
    return status;
}

wstk_status_t mod_whisper_models_registry_add(whisper_model_descr_t *descr) {
    wstk_status_t status = 0;

    if(!descr) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(globals->fl_shutdown) {
        return WSTK_STATUS_DESTROYED;
    }

    wstk_mutex_lock(globals->mutex);
    if(wstk_hash_find(globals->models, descr->name)) {
        status = WSTK_STATUS_ALREADY_EXISTS;
    } else {
       status = wstk_hash_insert_destructor(globals->models, descr->name, descr, destructor__hashtable_values);
       if(status == WSTK_STATUS_SUCCESS) {
            if(!wstk_str_is_empty(descr->alias)) {
                if(!wstk_hash_find(globals->models, descr->alias)) {
                    status = wstk_hash_insert_destructor(globals->models, descr->alias, descr, destructor__hashtable_values);
                    if(status == WSTK_STATUS_SUCCESS) {
                        wstk_mem_ref(descr);
                    }
                }
            }
       }
    }
    wstk_mutex_unlock(globals->mutex);

    if(status == WSTK_STATUS_ALREADY_EXISTS) {
        log_warn("Model already registered: %s", descr->name);
    } else if(status == WSTK_STATUS_SUCCESS) {
        log_debug("Model registered: %s (alias=%s, path=%s)", descr->name, descr->alias, descr->path);
    }
    return status;
}

whisper_model_descr_t *mod_whisper_models_registry_lookup(char *name) {
    whisper_model_descr_t *descr = NULL;

    if(!name) {
        return NULL;
    }
    if(globals->fl_shutdown) {
        return NULL;
    }

    wstk_mutex_lock(globals->mutex);
    descr = wstk_hash_find(globals->models, name);
    wstk_mutex_unlock(globals->mutex);

    return descr;
}


wstk_status_t mod_whisper_global_init(whisper_cpp_global_t **out, const char *endpoint) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    whisper_cpp_global_t *glb = NULL;

    status = wstk_mem_zalloc((void *)&glb, sizeof(whisper_cpp_global_t), destructor__whisper_cpp_global_t);
    if(status != WSTK_STATUS_SUCCESS) {
        log_error("wstk_mem_zalloc()");
        goto out;
    }

    if((status = wstk_mutex_create(&glb->mutex)) != WSTK_STATUS_SUCCESS) {
        log_error("wstk_mutex_create()");
        goto out;
    }
    if((status = wstk_hash_init(&glb->pools)) != WSTK_STATUS_SUCCESS) {
        log_error("wstk_hash_init()");
        goto out;
    }
    if((status = wstk_hash_init(&glb->models)) != WSTK_STATUS_SUCCESS) {
        log_error("wstk_hash_init()");
        goto out;
    }

out:
    if(status == WSTK_STATUS_SUCCESS) {
        *out = glb;
    } else {
        wstk_mem_deref(glb);
    }
    return status;
}

wstk_status_t mod_whisper_load_config() {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    char *config_path = NULL;
    const char *conf_version_str = NULL;
    ezxml_t xml, xelem, xparams;

    wstk_sdprintf(&config_path, "%s/mod-whisper-cpp-conf.xml", whsd_core_get_path_config());

    if(!wstk_file_exists(config_path)) {
        log_error("File not found: %s", config_path);
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if((xml = ezxml_parse_file(config_path)) == NULL) {
        log_error("Unable to parse configuration (%s)", config_path);
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if(!wstk_str_equal(ezxml_name(xml), "configuration", false)) {
        log_error("Missing root element: <configuration>");
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    conf_version_str = ezxml_attr(xml, "version");
    if(conf_version_str) {
        int vi = atoi(conf_version_str);
        if(vi < MOD_CFG_VERSION) {
            log_error("Configuration is outdate (%d) (expected version >= %i)", vi, MOD_CFG_VERSION);
            wstk_goto_status(WSTK_STATUS_FALSE, out);
        }
    }

    if((xelem = ezxml_child(xml, "whisper")) != NULL) {
        if((xparams = ezxml_child(xelem, "settings")) != NULL) {
            for(ezxml_t xparam = ezxml_child(xparams, "param"); xparam; xparam = xparam->next) {
                const char *name = ezxml_attr(xparam, "name");
                const char *val = ezxml_attr(xparam, "value");

                if(wstk_str_is_empty(name)) { continue; }

                if(wstk_str_equal(name, "max-threads", false)) {
                    globals->whisper_threads = wstk_str_atoi(val);
                }
                else if(wstk_str_equal(name, "max-tokens", false)) {
                    globals->whisper_tokens = wstk_str_atoi(val);
                }
                else if(wstk_str_equal(name, "gpu-dev", false)) {
                    globals->whisper_gpu_dev = wstk_str_atoi(val);
                }
                else if(wstk_str_equal(name, "use-gpu", false)) {
                    globals->whisper_use_gpu = wstk_str_atob(val);
                }
                else if(wstk_str_equal(name, "flash-attn", false)) {
                    globals->whisper_flash_attn = wstk_str_atob(val);
                }
            }
        }
    }

    if((xelem = ezxml_child(xml, "models")) != NULL) {
        const char *mbase_path = ezxml_attr(xelem, "path");

        if(wstk_str_is_empty(mbase_path)) {
            log_error("Missing attribute: models::path");
            wstk_goto_status(WSTK_STATUS_FALSE, out);
        }
        if(!wstk_dir_exists(mbase_path)) {
            log_error("Directory not found (%s)", mbase_path);
            wstk_goto_status(WSTK_STATUS_FALSE, out);
        }
        if(wstk_str_dup2(&globals->models_path, mbase_path) != WSTK_STATUS_SUCCESS) {
            log_error("wstk_str_dup2()");
            wstk_goto_status(WSTK_STATUS_FALSE, out);
        }

        for(ezxml_t xparam = ezxml_child(xelem, "model"); xparam; xparam = xparam->next) {
            const char *name = ezxml_attr(xparam, "name");
            const char *file = ezxml_attr(xparam, "file");
            const char *alias = ezxml_attr(xparam, "alias");
            whisper_model_descr_t *descr = NULL;
            char *file_path = NULL;

            if(wstk_str_is_empty(name) || wstk_str_is_empty(file)) {
                continue;
            }

            wstk_sdprintf(&file_path, "%s/%s", globals->models_path, file);
            if(!wstk_file_exists(file_path)) {
                log_error("File not found (%s) [mode=%s]", file_path, name);
                wstk_mem_deref(file_path);
                continue;
            }

            status = mod_whisper_model_descr_alloc(&descr, wstk_str_dup(name), wstk_str_dup(alias), file_path);
            if(status != WSTK_STATUS_SUCCESS) {
                log_error("whisper_model_descr_alloc()");
                goto out;
            }

            if(mod_whisper_models_registry_add(descr) != WSTK_STATUS_SUCCESS) {
                log_warn("Unable to register model (%s)", descr->name);
                wstk_mem_deref(descr);
            }
        }
    }

    if((xelem = ezxml_child(xml, "pools")) != NULL) {
        //
        // todo
        //
    }

out:
    wstk_mem_deref(config_path);
    if(xml) { ezxml_free(xml); }
    return status;
}


wstk_status_t mod_whisper_worker_create(whisper_worker_t **out, whisper_worker_cfg_t *cfg) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    struct whisper_context_params cparams = {0};
    whisper_worker_t *wrk = NULL;

    if(!out || !cfg) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    status = wstk_mem_zalloc((void *)&wrk, sizeof(whisper_worker_t), destructor__whisper_worker_t);
    if(status != WSTK_STATUS_SUCCESS) {
        log_error("wstk_mem_zalloc()");
        goto out;
    }

    cparams = whisper_context_default_params();
    cparams.use_gpu = cfg->use_gpu;
    cparams.gpu_device = cfg->gpu_dev;
    cparams.flash_attn = cfg->flash_attn;

    wrk->wctx = whisper_init_from_file_with_params(cfg->model_file, cparams);
    if(!wrk->wctx) {
        log_error("whisper_init_from_file_with_params()");
        goto out;
    }

#ifdef WHISPERD_DEBUG
    log_debug("worker created: %p (wctx=%p, model=%s)", wrk, wrk->wctx, cfg->model_file);
#endif
out:
    if(status == WSTK_STATUS_SUCCESS) {
        *out = wrk;
    } else {
        wstk_mem_deref(wrk);
    }
    return status;
}

wstk_status_t mod_whisper_worker_transcribe(whisper_worker_t *wrk, whisper_trans_ctx_t *tctx) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    struct whisper_full_params wparams = {0};
    uint64_t tm_start=0, tm_end=0;
    int segments = 0;

    if(!wrk || !tctx) {
        return WSTK_STATUS_INVALID_PARAM;
    }

    wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress   = false;
    wparams.print_special    = false;
    wparams.print_realtime   = false;
    wparams.print_timestamps = false;
    wparams.translate        = tctx->wop_translate;
    wparams.single_segment   = tctx->wop_single_segment;
    wparams.max_tokens       = tctx->wop_max_tokens;
    wparams.language         = tctx->language ? tctx->language : "en";
    wparams.n_threads        = globals->whisper_threads;
    wparams.audio_ctx        = 0;

    wparams.encoder_begin_callback_user_data = wrk;
    wparams.encoder_begin_callback = (whisper_encoder_begin_callback) whisper_worker_encoder_begin_callback;

    tm_start = wstk_time_micro_now();

    if(whisper_full(wrk->wctx, wparams, tctx->audio_buffer->data, tctx->audio_buffer->samples) != 0) {
        log_error("whisper_full() failed");
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    tm_end = wstk_time_micro_now();
#ifdef WHISPERD_DEBUG
    log_debug("elapsed time: %u", (uint32_t)(tm_end - tm_start));
#endif

    if(tctx->do_break) {
        goto out;
    }

    if((segments = whisper_full_n_segments(wrk->wctx))) {
        for(uint32_t i = 0; i < segments; ++i) {
            const char *text = whisper_full_get_segment_text(wrk->wctx, i);

            if(text && text[0] == ' ') {
                wstk_mbuf_write_str(tctx->text_buffer, (char *)(text + 1));
            } else {
                wstk_mbuf_write_str(tctx->text_buffer, text);
            }
        }
    }

out:
    return status;
}
