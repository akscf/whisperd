/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <whisperd.h>
#include <whisperd-file.h>
#include <whisperd-config.h>

extern whisperd_global_t *wd_global;

wd_status_t wd_thread_launch(thrd_start_t func, void *udata) {
    thrd_t thr;
    int err = 0;

    if((err = thrd_create(&thr, func, udata)) != thrd_success) {
        return WD_STATUS_FALSE;
    }

    mtx_lock(wd_global->mutex);
    wd_global->active_threds++;
    mtx_unlock(wd_global->mutex);

    return WD_STATUS_SUCCESS;
}

void wd_thread_finished() {
    mtx_lock(wd_global->mutex);
    if(wd_global->active_threds > 0) wd_global->active_threds--;
    mtx_unlock(wd_global->mutex);
}

wd_status_t wd_dir_create_ifne(char *dir) {
    if(!wd_dir_exists(dir)) {
        return wd_dir_create(dir, true);
    }
    return WD_STATUS_SUCCESS;
}

wd_status_t wd_switch_ug(char *user, char *group) {
    struct passwd *pw = NULL;
    struct group *gr = NULL;
    uid_t uid = 0;
    gid_t gid = 0;

    if (user) {
        if((pw = getpwnam(user)) == NULL) {
            re_fprintf(stderr,"ERROR: Unknown user: %s\n", user);
            return WD_STATUS_FALSE;
        }
        uid = pw->pw_uid;
    }
    if (group) {
        if((gr = getgrnam(group)) == NULL) {
            re_fprintf(stderr,"ERROR: Unknown group: %s\n", group);
            return WD_STATUS_FALSE;
        }
        gid = gr->gr_gid;
    }

    if (uid && getuid() == uid && (!gid || gid == getgid())) {
        return WD_STATUS_SUCCESS;
    }

    // GID
#ifdef HAVE_SETGROUPS
    if (setgroups(0, NULL) < 0) {
        re_fprintf(stderr, "ERROR: setgroups() failed\n");
        return WD_STATUS_FALSE;
    }
#endif
    if (gid) {
        if(setgid(gid) < 0) {
            re_fprintf(stderr, "ERROR: setgid() failed\n");
            return WD_STATUS_FALSE;
        }
    } else {
        if (setgid(pw->pw_gid) < 0) {
            re_fprintf(stderr, "ERROR: setgid() failed\n");
            return WD_STATUS_FALSE;
        }
#ifdef HAVE_INITGROUPS
        if (initgroups(pw->pw_name, pw->pw_gid) < 0) {
            re_fprintf(stderr, "ERROR: initgroups() failed\n");
            return WD_STATUS_FALSE;
        }
#endif
    }

    // UID
    if (setuid(uid) < 0) {
        re_fprintf(stderr, "ERROR: setuid() failed\n");
        return WD_STATUS_FALSE;
    }

    return WD_STATUS_SUCCESS;
}

pid_t wd_pid_read(const char *filename) {
    FILE *f;
    pid_t pid;

    if(!(f = fopen(filename, "r"))) {
        return 0;
    }
    int rd = fscanf(f, "%d", &pid);
    fclose(f);

    return pid;
}

wd_status_t wd_pid_write(const char *filename, pid_t pid) {
    FILE *f;
    int fd;

    if(((fd = open(filename, O_RDWR | O_CREAT, 0644)) == -1) || ((f = fdopen(fd, "r+")) == NULL)) {
        return WD_STATUS_FALSE;
    }
    if(!fprintf(f, "%d\n", pid)) {
        close(fd);
        return WD_STATUS_FALSE;
    }
    fflush(f);
    close(fd);

    return WD_STATUS_SUCCESS;
}

wd_status_t wd_pid_delete(const char *filename) {
    return unlink(filename) == 0 ? WD_STATUS_SUCCESS : WD_STATUS_FALSE;
}

uint64_t time_ms_now() {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (((uint64_t) tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}
