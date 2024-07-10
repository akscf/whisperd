/**
 **
 ** (C)2024 aks
 **/
#ifndef WHSD_CORE_H
#define WHSD_CORE_H
#include <wstk.h>

#define APP_DEFAULT_HOME    "/opt/whisperd"
#define APP_SYSLOG_NAME     "whisperd"
#define APP_VERSION_STR     "2.0.1"

#define WHISPERD_DEBUG

typedef enum {
    D_ACTION_NONE = 0,
    D_ACTION_START,
    D_ACTION_STOP,
    D_ACTION_RELOAD
} daemon_action_e;

/* whsd-main.c */
int whsd_main(int argc, char **argv);
const char *whsd_core_get_path_home();
const char *whsd_core_get_path_config();
const char *whsd_core_get_path_tmp();
const char *whsd_core_get_path_var();

/* whsd-misc.c */
wstk_status_t whsd_switch_ug(char *user, char *group);
wstk_status_t whsd_dir_create_ifne(char *dir);

#endif
