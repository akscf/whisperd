/**
 **
 **
 ** (C)2024 aks
 **/
#include <whisperd.h>

//
// todo
//

static wstk_status_t mod_load(const char *endpoint) {
    wstk_status_t status = WSTK_STATUS_SUCCESS;

    return status;
}

static void mod_unload() {

}

WHISPERD_MODULE_DEFINITION(mod_whisper_fast, mod_load, mod_unload);
