/**
 **
 ** (C)2024 aks
 **/
#ifndef WHSD_MODULES_H
#define WHSD_MODULES_H
#include <whsd-core.h>
#include <whsd-config.h>

wstk_status_t whsd_modules_manager_init(whsd_global_t *whsd_glb);
wstk_status_t whsd_modules_manager_shutdown();

typedef void (whsd_mod_unload_t)(void);
typedef wstk_status_t (whsd_mod_load_t)(const char *endpoint);

typedef struct {
    const char          *name;
    whsd_mod_load_t     *load;
    whsd_mod_unload_t   *unload;
} whsd_module_interface_t;

#define WHISPERD_MODULE_DEFINITION(name, load, unload) \
 whsd_module_interface_t whisperd_module_interface_v1 = { #name, load, unload }


#endif
