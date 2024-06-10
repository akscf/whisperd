/**
 **
 ** (C)2024 aks
 **/
#ifndef WHSD_CONFIG_H
#define WHSD_CONFIG_H
#include <whsd-core.h>

#define CONFIG_VERSION     0x1

typedef struct {
    char            *secret;
    char            *address;
    uint32_t        port;
    uint32_t        max_conns;
    uint32_t        max_idle;
} whsd_config_entry_http_server_t;

typedef struct {
    char            *name;
    char            *path;
    char            *endpoint;
} whsd_config_entry_module_t;

typedef struct {
    wstk_mutex_t    *mutex;
    wstk_list_t     *modules;           // list of whsd_config_entry_module_t
    char            *path_home;         // $home
    char            *path_config;       // $home/configs
    char            *path_modules;      // $home/lib/mods
    char            *path_var;          // $home/var
    char            *path_tmp;          // /tmp
    char            *file_config;       // $home/configs/whisperd-conf.xml
    char            *file_pid;          // $home/var/whisperd.pid
    bool            fl_ready;
    bool            fl_shutdown;
    bool            fl_debug_mode;
    //
    whsd_config_entry_http_server_t     *http_server;
} whsd_global_t;

wstk_status_t whsd_global_init(whsd_global_t **whsd_glb);
wstk_status_t whsd_config_load(whsd_global_t *whsd_glb);

#endif
