/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include <whisperd.h>
#include <whisperd-config.h>
#include <whisperd-audio.h>
#include <whisperd-srvc-http.h>
#include <whisperd-srvc-cluster.h>
#include <whisperd-whisper.h>

whisperd_global_t *wd_global = NULL;

static void signal_handler(int sig) {
    if(wd_global->fl_shutdown)  {
        return;
    }

    switch (sig) {
        case SIGHUP:  break;
        case SIGALRM: break;
        case SIGINT:
        case SIGTERM:
            wd_global->fl_ready = false;
            re_cancel();
            break;
    }
}

static void usage(void) {
    re_fprintf(stderr,"Usage: whisperd [options]\n"
        "options:\n"
        "\t-h <path>        app home (default: %s)\n"
        "\t-a <action>      start | stop | reload\n"
        "\t-u username      start daemon with a specified user\n"
        "\t-g groupname     start daemon with a specified group\n"
        "\t?                this help\n",
        DEFAULT_HOME
    );
    exit(1);
}

// ----------------------------------------------------------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------------------------------------------------------
#define main_goto_err(_status, _label) err = _status; goto _label
int main(int argc, char **argv) {
    daemon_action_e action = D_ACTION_NONE;
    pid_t srv_pid = 0;
    int opt = 0, err = 0, wdc = 0;
    bool fl_del_pid = false;
    char *xuser = NULL, *xgroup = NULL;

    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    sys_coredump_set(true);

    if((err = libre_init()) != LIBRE_SUCCESS) {
        re_fprintf(stderr, "FAIL: libre_init() (%m)\n", err);
        main_goto_err(-1, out);
    }
    if(wd_global_init(&wd_global) != WD_STATUS_SUCCESS) {
        main_goto_err(-1, out);
    }

    while((opt = getopt(argc, argv, "a:h:r:u:g:?")) != -1) {
        switch(opt) {
            case 'h':
                if(str_dup(&wd_global->path_home, optarg) != LIBRE_SUCCESS) {
                    print_mem_fail();
                    main_goto_err(-1, out);
                }
                break;
            case 'u':
                if(str_dup(&xuser, optarg) != LIBRE_SUCCESS) {
                    print_mem_fail();
                    main_goto_err(-1, out);
                }
                break;
            break;
            case 'g':
                if(str_dup(&xgroup, optarg) != LIBRE_SUCCESS) {
                    print_mem_fail();
                    main_goto_err(-1, out);
                }
                break;
            case 'a':
                if(str_casecmp(optarg, "start") == 0) {
                    action = D_ACTION_START;
                } else if(str_casecmp(optarg, "stop") == 0) {
                    action = D_ACTION_STOP;
                } else if(str_casecmp(optarg, "reload") == 0) {
                    action = D_ACTION_RELOAD;
                }
                break;
            case '?':
                action = D_ACTION_NONE;
                break;
        }
    }

    if(action == D_ACTION_NONE) {
        usage();
        main_goto_err(0, out);
    }

    if(xuser) {
        if(wd_switch_ug(xuser, xgroup) != WD_STATUS_SUCCESS) {
            re_fprintf(stderr,"ERROR: Couldn't switch to user/group = %s:%s\n", xuser, xgroup);
            main_goto_err(-1, out);
        }
    }
    mem_deref(xuser);
    mem_deref(xgroup);

    if(wd_global->path_home == NULL) {
        if(re_sdprintf(&wd_global->path_home, DEFAULT_HOME) != LIBRE_SUCCESS) {
            print_mem_fail();
            main_goto_err(-1, out);
        }
    }
    if(re_sdprintf(&wd_global->path_config, "%s/configs", wd_global->path_home) != LIBRE_SUCCESS) {
        print_mem_fail();
        main_goto_err(-1, out);
    }
    if(re_sdprintf(&wd_global->path_models, "%s/models", wd_global->path_home) != LIBRE_SUCCESS) {
        print_mem_fail();
        main_goto_err(-1, out);
    }
    if(re_sdprintf(&wd_global->path_var, "%s/var", wd_global->path_home) != LIBRE_SUCCESS) {
        print_mem_fail();
        main_goto_err(-1, out);
    }
    if(re_sdprintf(&wd_global->path_tmp, "/tmp") != LIBRE_SUCCESS) {
        print_mem_fail();
        main_goto_err(-1, out);
    }
    if(re_sdprintf(&wd_global->file_config, "%s/whisperd-conf.xml", wd_global->path_config) != LIBRE_SUCCESS) {
        print_mem_fail();
        main_goto_err(-1, out);
    }
    if(re_sdprintf(&wd_global->file_pid, "%s/whisperd.pid", wd_global->path_var) != LIBRE_SUCCESS) {
        print_mem_fail();
        main_goto_err(-1, out);
    }

    // D_ACTION_STOP || D_ACTION_RELOAD
    if(action == D_ACTION_STOP || action == D_ACTION_RELOAD)  {
        srv_pid = wd_pid_read(wd_global->file_pid);
        if(srv_pid == 0) {
            re_fprintf(stderr,"ERROR: Server is not running\n");
            main_goto_err(-1, out);
        }
        if(kill(srv_pid, (action == D_ACTION_RELOAD ? SIGHUP : SIGTERM)) != 0) {
            re_fprintf(stderr,"ERROR: Couldn't stop the server (pid=%d)\n", srv_pid);
            wd_pid_delete(wd_global->file_pid);
            main_goto_err(-1, out);
        }
        goto out;
    }

    // D_ACTION_START
    if(wd_dir_create_ifne(wd_global->path_home) != WD_STATUS_SUCCESS) {
        main_goto_err(-1, out);
    }
    if(wd_dir_create_ifne(wd_global->path_config) != WD_STATUS_SUCCESS) {
        main_goto_err(-1, out);
    }
    if(wd_dir_create_ifne(wd_global->path_models) != WD_STATUS_SUCCESS) {
        main_goto_err(-1, out);
    }
    if(wd_dir_create_ifne(wd_global->path_var) != WD_STATUS_SUCCESS) {
        main_goto_err(-1, out);
    }
    if(wd_dir_create_ifne(wd_global->path_tmp) != WD_STATUS_SUCCESS) {
        main_goto_err(-1, out);
    }

    srv_pid = wd_pid_read(wd_global->file_pid);
    if(srv_pid && srv_pid != getpid()) {
        re_fprintf(stderr,"ERROR: Deamon already running (pid=%d)\n", srv_pid);
        main_goto_err(-1, out);
    }

    if((err = sys_daemon()) != LIBRE_SUCCESS) {
        re_fprintf(stderr,"ERROR: Couldn't daemonize! (%m)\n", err);
        main_goto_err(-1, out);
    }

    openlog(SYSLOG_NAME, LOG_PID | LOG_NDELAY, LOG_DAEMON);

    srv_pid = getpid();
    if(wd_pid_write(wd_global->file_pid, srv_pid) != WD_STATUS_SUCCESS) {
        log_error("Couldn't write PID: %s", wd_global->file_pid);
        main_goto_err(-1, out);
    }
    fl_del_pid = true;

    if(wd_config_load(wd_global) != WD_STATUS_SUCCESS) {
        main_goto_err(-1, out);
    }

    /* start services */
    if(wd_audio_init() != WD_STATUS_SUCCESS) {
        main_goto_err(-1, out);
    }
    if(wd_service_http_start() != WD_STATUS_SUCCESS) {
        main_goto_err(-1, out);
    }
    if(wd_service_cluster_start() != WD_STATUS_SUCCESS) {
        main_goto_err(-1, out);
    }

    /* mian loop */
    wd_global->fl_ready = true;
    wd_global->fl_shutdown = false;

    log_notice("whisperd-%s [pid:%d / uid:%d / gid:%d] - started", APP_VERSION, srv_pid, getuid(), getgid());
    log_notice("hw-features: %s", whisper_print_system_info());

    if((err = re_main(signal_handler)) != LIBRE_SUCCESS) {
        log_error("re_main() fail (%i)", err);
        err = -1;
    }

    wd_global->fl_ready = false;
    wd_global->fl_shutdown = true;

    wd_service_http_stop();
    wd_service_cluster_stop();
    wd_audio_shutdown();

    if(wd_global->active_threds > 0) {
        uint8_t fl_wloop = true;

        log_warn("Waiting for termination '%i' threads...", wd_global->active_threds);
        while(fl_wloop) {
            mtx_lock(wd_global->mutex);
            fl_wloop = (wd_global->active_threds > 0);
            mtx_unlock(wd_global->mutex);
            sys_msleep(1000);
        }
    }
out:
    if(fl_del_pid) {
        wd_pid_delete(wd_global->file_pid);
    }

    if(wd_global) {
        mem_deref(wd_global);
    }

#ifdef WD_DEBUG_ENABLE_RE_TMRDEBUG
    tmr_debug();
#endif

    libre_close();

#ifdef WD_DEBUG_ENABLE_RE_MEMDEBUG
    mem_debug();
#endif

    if(action == D_ACTION_START) {
        if(err == 0) {
            log_notice("whisperd-%s [pid:%d] - stopped", APP_VERSION, srv_pid);
        }
    }

    return err;
};

