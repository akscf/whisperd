/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#ifndef WHISPERD_CONFIG_H
#define WHISPERD_CONFIG_H
#include <whisperd.h>

#define CONFIG_VERSION     0x1

typedef struct {
    char        *cert_file;
    char        *access_secret;
    char        *address;
    uint32_t    port_plain;
    uint32_t    port_ssl;
    uint32_t    max_threads;
    uint32_t    max_content_length;
    uint32_t    min_content_length;
    uint32_t    max_websoc_connections;
    uint32_t    idle_timeout;
    uint32_t    enabled;
} wd_config_entry_service_http_t;

typedef struct {
    uint32_t    max_threads;
    uint32_t    max_tokens;
} wd_config_entry_whisper_worker_t;

typedef struct {
    uint32_t    enabled;
} wd_config_entry_service_cluster_t;

typedef struct {
    mtx_t       *mutex;
    wd_hash_t   *models_hash;       // name|alias -> description
    wd_hash_t   *workers_hash;      // modelName -> list
    char        *path_home;         // $home
    char        *path_config;       // $home/configs
    char        *path_var;          // $home/var
    char        *path_models;       // $home/models
    char        *path_tmp;          // /tmp
    char        *file_config;       // $home/configs/whisperd-conf.xml
    char        *file_pid;          // $home/run/whisperd.pid
    uint32_t    active_threds;
    bool        fl_ready;
    bool        fl_shutdown;
    //
    wd_config_entry_whisper_worker_t    *whisper_worker;
    wd_config_entry_service_http_t      *http_service;
    wd_config_entry_service_cluster_t   *cluster_service;

} whisperd_global_t;

wd_status_t wd_global_init(whisperd_global_t **wd_global);
wd_status_t wd_config_load(whisperd_global_t *wd_global);

#endif

