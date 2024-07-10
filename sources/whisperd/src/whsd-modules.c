/**
 **
 ** (C)2024 aks
 **/
#include <whsd-modules.h>
#include <whsd-config.h>

typedef struct {
    wstk_mutex_t                *mutex;
    wstk_hash_t                 *modules;       // name => module_entry_t
    whsd_global_t               *whsd_glb;
    bool                        fl_destroyed;
} whsd_modules_manager_t;

typedef struct {
    wstk_dlo_t                  *dlo;
    whsd_module_interface_t     *api;
    char                        *name;          // refs to the list entry
    bool                        success_load;
} module_entry_t;

static whsd_modules_manager_t *manager = NULL;
static module_entry_t *module_lookup(const char *name);

// --------------------------------------------------------------------------------------------------------------
static void destructor__whsd_modules_manager_t(void *data) {
    whsd_modules_manager_t *obj = (whsd_modules_manager_t *)data;

    if(!obj || obj->fl_destroyed) { return; }
    obj->fl_destroyed = true;

    if(obj->modules) {
        wstk_mutex_lock(obj->mutex);
        wstk_mem_deref(obj->modules);
        wstk_mutex_unlock(obj->mutex);
    }

    wstk_mem_deref(obj->mutex);
}

static void destructor__module_entry_t(void *data) {
    module_entry_t *obj = (module_entry_t *)data;

#ifdef WHISPERD_DEBUG
    log_debug("Unloading module: (%s)", obj->name);
#endif

    if(obj->success_load) {
        if(obj->api) {
            obj->api->unload();
        }
    }

    if(obj->dlo) {
        wstk_dlo_close(&obj->dlo);
    }
}

static void modlist_foreach_cb_onload(uint32_t idx, void *data, void *udata) {
    whsd_config_entry_module_t *descr = (whsd_config_entry_module_t *)data;
    module_entry_t *entry = NULL;
    whsd_module_interface_t *api = NULL;
    wstk_dlo_t *dlo = NULL;
    wstk_status_t st = WSTK_STATUS_FALSE;
    int err = 0;

    st = wstk_dlo_open(&dlo, descr->path);
    if(st != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    st = wstk_dlo_sym(dlo, "whisperd_module_interface_v1", (void *)&api);
    if(st != WSTK_STATUS_SUCCESS) { goto out; }

    if(wstk_str_is_empty(api->name)) {
        log_warn("Empty module name (NULL)");
        st = WSTK_STATUS_INVALID_PARAM;
        goto out;
    }

    if(module_lookup(api->name)) {
        log_warn("Module already registered (%s)", api->name);
        st = WSTK_STATUS_ALREADY_EXISTS;
        goto out;
    }

    st = wstk_mem_zalloc((void *)&entry, sizeof(module_entry_t), destructor__module_entry_t);
    if(st != WSTK_STATUS_SUCCESS) {
        log_error("Unable to allocate memeory");
        goto out;
    }

    entry->dlo = dlo;
    entry->api = api;
    entry->name = descr->name;

    if((err = api->load(descr->endpoint)) == 0) {
        entry->success_load = true;

        wstk_mutex_lock(manager->mutex);
        st = wstk_hash_insert_ex(manager->modules, api->name, entry, true);
        wstk_mutex_unlock(manager->mutex);
    } else {
        log_warn("Unable to load module (err=%i)", err);
        st = WSTK_STATUS_FALSE;
    }

out:
    if(st != WSTK_STATUS_SUCCESS) {
        if(entry) {
            wstk_mem_deref(entry);
        } else {
            if(dlo) { wstk_dlo_close(&dlo); }
        }
        log_warn("Unable to load module (%s)", descr->name);
    } else {
        log_debug("Module loaded (%s)", descr->name);
    }
}

static module_entry_t *module_lookup(const char *name) {
    module_entry_t *entry = NULL;

    if(!manager || !name) {
        return NULL;
    }

    wstk_mutex_lock(manager->mutex);
    entry = wstk_hash_find(manager->modules, name);
    wstk_mutex_unlock(manager->mutex);

    return entry;
}


// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
wstk_status_t whsd_modules_manager_init(whsd_global_t *whsd_glb) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;


    status = wstk_mem_zalloc((void *)&manager, sizeof(whsd_modules_manager_t), destructor__whsd_modules_manager_t);
    if(status != WSTK_STATUS_SUCCESS) { goto out; }

    if((status = wstk_mutex_create(&manager->mutex)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    if((status = wstk_hash_init(&manager->modules)) != WSTK_STATUS_SUCCESS) {
        goto out;
    }

    manager->whsd_glb = whsd_glb;

#ifdef WHISPERD_DEBUG
    log_debug("Loading modules");
#endif

    wstk_list_foreach(whsd_glb->modules, modlist_foreach_cb_onload, NULL);

out:
    return status;
}

wstk_status_t whsd_modules_manager_shutdown() {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    if(!manager) {
        return WSTK_STATUS_INVALID_PARAM;
    }
    if(manager->fl_destroyed) {
        return WSTK_STATUS_SUCCESS;
    }

    wstk_mem_deref(manager);

    return status;
}
