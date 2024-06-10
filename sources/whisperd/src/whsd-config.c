/**
 **
 ** (C)2024 aks
 **/
#include <whsd-config.h>

static void destructor__whsd_global_t(void *data) {
    whsd_global_t *obj = (whsd_global_t *)data;

    if(!obj) { return; }

    wstk_mem_deref(obj->file_config);
    wstk_mem_deref(obj->file_pid);

    wstk_mem_deref(obj->path_home);
    wstk_mem_deref(obj->path_config);
    wstk_mem_deref(obj->path_modules);
    wstk_mem_deref(obj->path_var);
    wstk_mem_deref(obj->path_tmp);

    if(obj->http_server) {
        wstk_mem_deref(obj->http_server->secret);
        wstk_mem_deref(obj->http_server->address);
        wstk_mem_deref(obj->http_server);
    }

    if(obj->modules) {
        wstk_list_destroy(&obj->modules);
    }

    wstk_mem_deref(obj->mutex);
}

static void destructor__whsd_config_entry_module_t(void *data) {
    whsd_config_entry_module_t *obj = (whsd_config_entry_module_t *)data;

    if(!obj) { return; }

    wstk_mem_deref(obj->name);
    wstk_mem_deref(obj->path);
    wstk_mem_deref(obj->endpoint);
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t whsd_global_init(whsd_global_t **whsd_glb) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    whsd_global_t *whsd_local = NULL;

    status = wstk_mem_zalloc((void *)&whsd_local, sizeof(whsd_global_t), destructor__whsd_global_t);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    status = wstk_mutex_create(&whsd_local->mutex);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    /* http-server */
    status = wstk_mem_zalloc((void *)&whsd_local->http_server, sizeof(whsd_config_entry_http_server_t), NULL);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    /* modules */
    status = wstk_list_create(&whsd_local->modules);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }


    *whsd_glb = whsd_local;
out:
    if(status != WSTK_STATUS_SUCCESS) {
        wstk_mem_deref(whsd_local);
    }
    return status;
}

wstk_status_t whsd_config_load(whsd_global_t *whsd_glb) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;
    const char *conf_version_str = NULL;
    ezxml_t xml, xelem, xparams;

    if(!wstk_file_exists(whsd_glb->file_config)) {
        log_error("File not found: %s", whsd_glb->file_config);
        return WSTK_STATUS_FALSE;
    }

    if((xml = ezxml_parse_file(whsd_glb->file_config)) == NULL) {
        log_error("Unable to parse configuration (%s)", whsd_glb->file_config);
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    if(!wstk_str_equal(ezxml_name(xml), "configuration", false)) {
        log_error("Missing root element: <configuration>");
        wstk_goto_status(WSTK_STATUS_FALSE, out);
    }

    conf_version_str = ezxml_attr(xml, "version");
    if(conf_version_str) {
        int vi = atoi(conf_version_str);
        if(vi < CONFIG_VERSION) {
            log_error("Configuration is outdated (%d) (expected version >= %i)", vi, CONFIG_VERSION);
            wstk_goto_status(WSTK_STATUS_FALSE, out);
        }
    }

    /* http-server */
    if((xelem = ezxml_child(xml, "http-server")) != NULL) {
        if((xparams = ezxml_child(xelem, "settings")) != NULL) {
            for(ezxml_t xparam = ezxml_child(xparams, "param"); xparam; xparam = xparam->next) {
                const char *name = ezxml_attr(xparam, "name");
                const char *val = ezxml_attr(xparam, "value");

                if(wstk_str_is_empty(name)) { continue; }

                if(wstk_str_equal(name, "address", false)) {
                    wstk_str_dup2(&whsd_glb->http_server->address, val);
                }
                else if(wstk_str_equal(name, "secret", false)) {
                    wstk_str_dup2(&whsd_glb->http_server->secret, val);
                }
                else if(wstk_str_equal(name, "port", false)) {
                    whsd_glb->http_server->port = wstk_str_atoi(val);
                }
                else if(wstk_str_equal(name, "max-conns", false)) {
                    whsd_glb->http_server->max_conns = wstk_str_atoi(val);
                }
                else if(wstk_str_equal(name, "max-idle", false)) {
                    whsd_glb->http_server->max_idle = wstk_str_atoi(val);
                }
            }
        }
    }

    /* modules */
    if((xelem = ezxml_child(xml, "modules")) != NULL) {
        for(ezxml_t xparam = ezxml_child(xelem, "module"); xparam; xparam = xparam->next) {
            const char *name = ezxml_attr(xparam, "name");
            const char *enabled = ezxml_attr(xparam, "enabled");
            const char *endpoint = ezxml_attr(xparam, "endpoint");

            if(wstk_str_is_empty(name)) { continue; }

            if(wstk_str_equal(enabled, "true", false)) {
                whsd_config_entry_module_t *entry = NULL;
                char *mod_path = NULL;

                status = wstk_sdprintf(&mod_path, "%s/%s", whsd_glb->path_modules, name);
                if(status != WSTK_STATUS_SUCCESS || !mod_path) { log_error("wstk_sdprintf()"); break; }

                if(!wstk_file_exists(mod_path)) {
                    log_warn("File not found: %s", mod_path);
                    mod_path = wstk_mem_deref(mod_path);
                    continue;
                }

                status = wstk_mem_zalloc((void *)&entry, sizeof(whsd_config_entry_module_t), destructor__whsd_config_entry_module_t);
                if(status != WSTK_STATUS_SUCCESS) { log_error("wstk_mem_zalloc()"); break; }

                entry->path = mod_path;
                entry->name = wstk_str_dup(name);
                entry->endpoint = wstk_str_dup(endpoint);

                wstk_list_add_tail(whsd_glb->modules, entry, destructor__whsd_config_entry_module_t);
            }
        }
        if(status != WSTK_STATUS_SUCCESS) { goto out; }
    }

out:
    if(xml) {
        ezxml_free(xml);
    }
    return status;
}

