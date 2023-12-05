/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include <whisperd-model.h>
#include <whisperd-config.h>
#include <whisperd-file.h>

extern whisperd_global_t *wd_global;

static void destructor__wd_model_description_t(void *data) {
    wd_model_description_t *ptr = (wd_model_description_t *)data;

    mem_deref(ptr->name);
    mem_deref(ptr->alias);
    mem_deref(ptr->language);
    mem_deref(ptr->path);
}

// ----------------------------------------------------------------------------------------------------------------------------------
// public
// ----------------------------------------------------------------------------------------------------------------------------------
wd_status_t wd_model_description_alloc(wd_model_description_t **descr) {
    wd_status_t status = WD_STATUS_SUCCESS;
    wd_model_description_t *descr_local = NULL;

    descr_local = mem_zalloc(sizeof(wd_model_description_t), destructor__wd_model_description_t);
    if(descr_local == NULL) {
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }

    *descr = descr_local;
out:
    return status;
}

wd_status_t wd_model_register(wd_model_description_t *descr) {
    wd_status_t status = WD_STATUS_SUCCESS;

    if(!descr || !descr->name || !descr->path) {
        return WD_STATUS_FALSE;
    }

    if(!wd_file_exists(descr->path)) {
        log_warn("File not found: %s", descr->path);
        return WD_STATUS_FALSE;
    }

    mtx_lock(wd_global->mutex);

    if(wd_hash_find(wd_global->models_hash, descr->name)) {
        log_warn("Model '%s' already registered", descr->name);
        wd_goto_status(WD_STATUS_FALSE, out);
    }
    wd_hash_insert(wd_global->models_hash, descr->name, descr);

    if(descr->alias && !wd_hash_find(wd_global->models_hash, descr->alias)) {
        wd_hash_insert(wd_global->models_hash, descr->alias, descr);
    }

out:
    mtx_unlock(wd_global->mutex);
    return status;
}

wd_model_description_t *wd_model_lookup(const char *name) {
    wd_model_description_t *descr = NULL;

    if(!name) {
        return NULL;
    }

    mtx_lock(wd_global->mutex);
    descr = wd_hash_find(wd_global->models_hash, name);
    mtx_unlock(wd_global->mutex);

    return descr;
}

