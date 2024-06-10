/**
 **
 ** (C)2024 aks
 **/
#include <whsd-core.h>
#include <pwd.h>
#include <grp.h>

wstk_status_t whsd_dir_create_ifne(char *dir) {
    if(!wstk_dir_exists(dir)) {
        return wstk_dir_create(dir, true);
    }
    return WSTK_STATUS_SUCCESS;
}

wstk_status_t whsd_switch_ug(char *user, char *group) {
    struct passwd *pw = NULL;
    struct group *gr = NULL;
    uid_t uid = 0;
    gid_t gid = 0;

    if(user) {
        if((pw = getpwnam(user)) == NULL) {
            log_error("Unknown user: %s", user);
            return WSTK_STATUS_FALSE;
        }
        uid = pw->pw_uid;
    }
    if(group) {
        if((gr = getgrnam(group)) == NULL) {
            log_error("Unknown group: %s", user);
            return WSTK_STATUS_FALSE;
        }
        gid = gr->gr_gid;
    }

    if(uid && getuid() == uid && (!gid || gid == getgid())) {
        return WSTK_STATUS_SUCCESS;
    }

    if(gid) {
        if(setgid(gid) < 0) {
            log_error("Unable to perform: setgid()");
            return WSTK_STATUS_FALSE;
        }
    } else {
        if(setgid(pw->pw_gid) < 0) {
            log_error("Unable to perform: setgid()");
            return WSTK_STATUS_FALSE;
        }
    }

    if(setuid(uid) < 0) {
        log_error("Unable to perform: setuid()\n");
        return WSTK_STATUS_FALSE;
    }

    return WSTK_STATUS_SUCCESS;
}
