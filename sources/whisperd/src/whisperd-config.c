/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include <whisperd-config.h>
#include <whisperd-file.h>
#include <whisperd-model.h>
#include <whisperd-srvc-http.h>
#include <ezxml.h>

static void destructor__whisperd_global_t(void *data) {
    whisperd_global_t *ptr = (whisperd_global_t *)data;

    mem_deref(ptr->file_config);
    mem_deref(ptr->file_pid);

    mem_deref(ptr->path_home);
    mem_deref(ptr->path_config);
    mem_deref(ptr->path_models);
    mem_deref(ptr->path_var);
    mem_deref(ptr->path_tmp);

    if(ptr->whisper_worker) {
        mem_deref(ptr->whisper_worker);
    }

    if(ptr->http_service) {
        mem_deref(ptr->http_service->access_secret);
        mem_deref(ptr->http_service->cert_file);
        mem_deref(ptr->http_service->address);
        mem_deref(ptr->http_service);
    }

    if(ptr->cluster_service) {
        mem_deref(ptr->cluster_service);
    }

    if(ptr->workers_hash) {
        wd_hash_destroy(&ptr->workers_hash);
    }
    if(ptr->models_hash) {
        wd_hash_destroy(&ptr->models_hash);
    }

    mem_deref(ptr->mutex);
}

// ----------------------------------------------------------------------------------------------------------------------------------
// public
// ----------------------------------------------------------------------------------------------------------------------------------
wd_status_t wd_global_init(whisperd_global_t **wd_global) {
    wd_status_t status = WD_STATUS_SUCCESS;
    whisperd_global_t *wd_local = NULL;

    wd_local = mem_zalloc(sizeof(whisperd_global_t), destructor__whisperd_global_t);
    if(wd_local == NULL) {
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }

    if(mutex_alloc(&wd_local->mutex) != LIBRE_SUCCESS) {
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }

    // hashes
    wd_hash_init(&wd_local->models_hash);
    wd_hash_init(&wd_local->workers_hash);

    // whisper worker
    wd_local->whisper_worker = mem_zalloc(sizeof(wd_config_entry_whisper_worker_t), NULL);
    if(!wd_local->whisper_worker) {
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }

    // http config entry
    wd_local->http_service = mem_zalloc(sizeof(wd_config_entry_service_http_t), NULL);
    if(!wd_local->http_service) {
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }
    wd_local->http_service->enabled = false;

    // cluster config entry
    wd_local->cluster_service = mem_zalloc(sizeof(wd_config_entry_service_cluster_t), NULL);
    if(!wd_local->cluster_service) {
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }
    wd_local->cluster_service->enabled = false;


    *wd_global = wd_local;
out:
    if(status != WD_STATUS_SUCCESS) {
        if(wd_local) { mem_deref(wd_local); }
    }
    return status;
}

wd_status_t wd_config_load(whisperd_global_t *wd_global) {
    wd_status_t status = WD_STATUS_SUCCESS;
    const char *conf_version_str = NULL;
    ezxml_t xml, xelem, xparams;

    if(!wd_file_exists(wd_global->file_config)) {
        log_error("File not found: %s", wd_global->file_config);
        return WD_STATUS_NOT_FOUND;
    }

    if((xml = ezxml_parse_file(wd_global->file_config)) == NULL) {
        log_error("Couldn't parese XML");
        wd_goto_status(WD_STATUS_FALSE, out);
    }

    conf_version_str = ezxml_attr(xml, "version");
    if(conf_version_str) {
        int vi = atoi(conf_version_str);
        if(vi > CONFIG_VERSION) {
            log_error("Incorrect configuration version (require <= %i)", CONFIG_VERSION);
            wd_goto_status(WD_STATUS_FALSE, out);
        }
    }

    if(str_casecmp(ezxml_name(xml), "configuration") != 0) {
        log_error("Missing root element: <configuration>");
        wd_goto_status(WD_STATUS_FALSE, out);
    }

    // http-service
    if((xelem = ezxml_child(xml, "http-service")) != NULL) {
        const char *enable = ezxml_attr(xelem, "enabled");
        if(str_casecmp(enable, "true") == 0) {
            wd_global->http_service->enabled = true;

            if((xparams = ezxml_child(xelem, "settings")) != NULL) {
                for(ezxml_t xparam = ezxml_child(xparams, "param"); xparam; xparam = xparam->next) {
                    const char *name = ezxml_attr(xparam, "name");
                    const char *val = ezxml_attr(xparam, "value");

                    if(!name) { continue; }

                    if(str_casecmp(name, "address") == 0) {
                        if(val) str_dup(&wd_global->http_service->address, val);
                    } else if(str_casecmp(name, "cert-file") == 0) {
                        if(val) re_sdprintf(&wd_global->http_service->cert_file, "%s/%s", wd_global->path_config, val);
                    } else if(str_casecmp(name, "access-secret") == 0) {
                        if(val) str_dup(&wd_global->http_service->access_secret, val);
                    } else if(str_casecmp(name, "port") == 0) {
                        if(val) wd_global->http_service->port_plain = atoi(val);
                    } else if(str_casecmp(name, "ssl-port") == 0) {
                        if(val) wd_global->http_service->port_ssl = atoi(val);
                    } else if(str_casecmp(name, "max-threads") == 0) {
                        if(val) wd_global->http_service->max_threads = atoi(val);
                    } else if(str_casecmp(name, "max-content-length") == 0) {
                        if(val) wd_global->http_service->max_content_length = atoi(val);
                    } else if(str_casecmp(name, "min-content-length") == 0) {
                        if(val) wd_global->http_service->min_content_length = atoi(val);
                    } else if(str_casecmp(name, "max-websoc-connections") == 0) {
                        if(val) wd_global->http_service->max_websoc_connections = atoi(val);
                    } else if(str_casecmp(name, "idle-timeout") == 0) {
                        if(val) wd_global->http_service->idle_timeout = atoi(val);
                    }
                }
            }
        }
    }

    // cluster-service
    if((xelem = ezxml_child(xml, "cluster-service")) != NULL) {
        const char *enable = ezxml_attr(xelem, "enabled");
        if(str_casecmp(enable, "true") == 0) {
            wd_global->cluster_service->enabled = true;

            if((xparams = ezxml_child(xelem, "settings")) != NULL) {
                for(ezxml_t xparam = ezxml_child(xparams, "param"); xparam; xparam = xparam->next) {
                    const char *name = ezxml_attr(xparam, "name");
                    const char *val = ezxml_attr(xparam, "value");

                    if(!name) { continue; }
                    //log_notice("cluster: %s=%s", name, val);
                }
            }
        }
    }

    // whisper-worker
    if((xelem = ezxml_child(xml, "whisper-worker")) != NULL) {
        if((xparams = ezxml_child(xelem, "settings")) != NULL) {
            for(ezxml_t xparam = ezxml_child(xparams, "param"); xparam; xparam = xparam->next) {
                const char *name = ezxml_attr(xparam, "name");
                const char *val = ezxml_attr(xparam, "value");

                if(!name) { continue; }
                 else if(str_casecmp(name, "max-threads") == 0) {
                    if(val) wd_global->whisper_worker->max_threads = atoi(val);
                } else if(str_casecmp(name, "max-tokens") == 0) {
                    if(val) wd_global->whisper_worker->max_tokens= atoi(val);
                } else if(str_casecmp(name, "sim-enabled") == 0) {
                    if(val && str_casecmp(val, "true") == 0) wd_global->fl_sim_enabled = true;
                }
            }
        }
    }

    // models
    if((xelem = ezxml_child(xml, "models")) != NULL) {
        for(ezxml_t xparam = ezxml_child(xelem, "model"); xparam; xparam = xparam->next) {
            const char *name = ezxml_attr(xparam, "name");
            const char *file = ezxml_attr(xparam, "file");
            const char *alias = ezxml_attr(xparam, "alias");
            const char *language = ezxml_attr(xparam, "language");
            wd_model_description_t *model_descr = NULL;
            wd_list_t *wlist = NULL;

            if(!name || !file) { continue; }

            if(wd_model_description_alloc(&model_descr) == WD_STATUS_SUCCESS) {
                re_sdprintf(&model_descr->path, "%s/%s", wd_global->path_models, file);
                str_dup(&model_descr->name, name);
                str_dup(&model_descr->alias, alias);
                str_dup(&model_descr->language, language);

                if(wd_model_register(model_descr) == WD_STATUS_SUCCESS) {
                    if(wd_hash_find(wd_global->workers_hash, name) == NULL) {
                        if(wd_list_create(&wlist) == WD_STATUS_SUCCESS) {
                            wd_hash_insert(wd_global->workers_hash, name, wlist);
                        } else {
                            log_warn("Couldn't create workers pool for model: %s", name);
                        }
                    }
                } else {
                    mem_deref(model_descr);
                    log_warn("Couldn't register model: %s", name);
                }
            }
        }
    }

out:
    if(xml) {
        ezxml_free(xml);
    }

    return status;
}
