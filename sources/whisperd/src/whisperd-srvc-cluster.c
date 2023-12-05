/**
 *
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include <whisperd-srvc-cluster.h>

extern whisperd_global_t *wd_global;

typedef struct {
    mtx_t               *mutex;
    uint32_t            node_id;

} cluster_service_conf_t;


static cluster_service_conf_t *cluster_service_conf = NULL;

// ----------------------------------------------------------------------------------------------------------------------------------
// private
// ----------------------------------------------------------------------------------------------------------------------------------



// ----------------------------------------------------------------------------------------------------------------------------------
// public
// ----------------------------------------------------------------------------------------------------------------------------------
wd_status_t wd_service_cluster_start() {
    wd_status_t status = WD_STATUS_SUCCESS;
    const wd_config_entry_service_cluster_t *cluster_config_entry = wd_global->cluster_service;

    cluster_service_conf = mem_zalloc(sizeof(cluster_service_conf_t), NULL);
    if(!cluster_service_conf) {
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }
    if(mutex_alloc(&cluster_service_conf->mutex) != LIBRE_SUCCESS) {
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }

    //
    // todo
    //

out:
    return status;
}

wd_status_t wd_service_cluster_stop() {
    if(cluster_service_conf) {

        mem_deref(cluster_service_conf->mutex);
        mem_deref(cluster_service_conf);
    }
    return WD_STATUS_SUCCESS;
}
