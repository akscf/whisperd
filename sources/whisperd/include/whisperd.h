/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#ifndef WHISPERD_H
#define WHISPERD_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <whisperd-re.h>

#ifndef true
 #define true 1
#endif
#ifndef false
 #define false 0
#endif

#define wd_goto_status(_status, _label) status = _status; goto _label

#define log_notice(fmt, ...) do{syslog(LOG_NOTICE, "NOTICE [%s:%d]: " fmt, __FILE__, __LINE__, ##__VA_ARGS__);} while (0)
#define log_debug(fmt, ...) do{syslog(LOG_DEBUG, "DEBUG [%s:%d]: " fmt, __FILE__, __LINE__, ##__VA_ARGS__);} while (0)
#define log_error(fmt, ...) do{syslog(LOG_ERR, "ERROR [%s:%d]: " fmt, __FILE__, __LINE__, ##__VA_ARGS__);} while (0)
#define log_warn(fmt, ...) do{syslog(LOG_WARNING, "WARN [%s:%d]: " fmt, __FILE__, __LINE__, ##__VA_ARGS__);} while (0)
#define log_mem_fail() do{syslog(LOG_ERR, "FAIL [%s:%d]: mem fail!", __FILE__, __LINE__);} while (0)
#define log_mem_fail_goto_status(_status, _label) do{syslog(LOG_ERR, "FAIL [%s:%d]: mem fail!", __FILE__, __LINE__); status = _status; goto _label; } while (0)
#define print_mem_fail() do{re_fprintf(stderr, "FAIL [%s:%d]: mem fail!\n", __FILE__, __LINE__);} while (0)

#define DEFAULT_HOME    "/opt/whisperd"
#define APP_VERSION     "1.0 (a01)"
#define SYSLOG_NAME     "whisperd"

//#define WD_DEBUG_ENABLE_RE_MEMDEBUG
//#define WD_DEBUG_ENABLE_RE_TMRDEBUG
#define WD_DEBUG_ENABLE_REQ_FORM

typedef enum { WD_STATUS_SUCCESS = 0, WD_STATUS_FALSE, WD_STATUS_NOT_FOUND, WD_STATUS_ALREADY_EXISTS } wd_status_t;
typedef enum { D_ACTION_NONE = 0, D_ACTION_START, D_ACTION_STOP, D_ACTION_RELOAD } daemon_action_e;

/* whisperd-hashtable.c */
typedef enum {
    WD_HASHTABLE_FLAG_NONE = 0,
    WD_HASHTABLE_FLAG_FREE_KEY = (1 << 0),
    WD_HASHTABLE_FLAG_FREE_VALUE = (1 << 1),
    WD_HASHTABLE_DUP_CHECK = (1 << 2)
} wd_hashtable_flag_t;

typedef struct wd_hashtable wd_hash_t;
typedef struct wd_hashtable wd_inthash_t;
typedef struct wd_hashtable_iterator wd_hash_index_t;
typedef void (*wd_hashtable_destructor_t)(void *ptr);

#define wd_hash_init(_hash) wd_hash_init_case(_hash, true)
#define wd_hash_init_nocase(_hash) wd_hash_init_case(_hash, false)
#define wd_hash_insert(_h, _k, _d) wd_hash_insert_destructor(_h, _k, _d, NULL)

wd_status_t wd_hash_init_case(wd_hash_t **hash, bool case_sensitive);
wd_status_t wd_hash_destroy(wd_hash_t **hash);
wd_status_t wd_hash_insert_destructor(wd_hash_t *hash, const char *key, const void *data, wd_hashtable_destructor_t destructor);

unsigned int wd_hash_size(wd_hash_t *hash);
void *wd_hash_delete(wd_hash_t *hash, const char *key);
void *wd_hash_find(wd_hash_t *hash, const char *key);
bool wd_hash_empty(wd_hash_t *hash);
wd_hash_index_t *wd_hash_first(wd_hash_t *hash);
wd_hash_index_t *wd_hash_first_iter(wd_hash_t *hash, wd_hash_index_t *hi);
wd_hash_index_t *wd_hash_next(wd_hash_index_t **hi);
void wd_hash_this(wd_hash_index_t *hi, const void **key, size_t *klen, void **val);
void wd_hash_this_val(wd_hash_index_t *hi, void *val);

wd_status_t wd_inthash_init(wd_inthash_t **hash);
wd_status_t wd_inthash_destroy(wd_inthash_t **hash);
wd_status_t wd_inthash_insert(wd_inthash_t *hash, uint32_t key, const void *data);
void *wd_core_inthash_delete(wd_inthash_t *hash, uint32_t key);
void *wd_core_inthash_find(wd_inthash_t *hash, uint32_t key);


/* whisperd-list.c */
typedef struct wd_list_s  wd_list_t;

#define wd_list_add_head(l, data) wd_list_add(l, 0, data)
#define wd_list_add_tail(l, data) wd_list_add(l, -1, data)

wd_status_t wd_list_create(wd_list_t **list);
wd_status_t wd_list_destroy(wd_list_t **list);
wd_status_t wd_list_add(wd_list_t *list, int pos, void *data);
wd_status_t wd_list_del(wd_list_t *list, int pos);
wd_status_t wd_list_foreach(wd_list_t *list, void (*cb)(int, void *));
void *wd_list_find(wd_list_t *list, int (*cb)(int, void *));
int wd_list_get_size(wd_list_t *list);
void *wd_list_get(wd_list_t *list, int pos);


/* whisperd-misc.c */
uint64_t time_ms_now();

wd_status_t wd_dir_create_ifne(char *dir);
wd_status_t wd_switch_ug(char *user, char *group);

wd_status_t wd_pid_delete(const char *filename);
wd_status_t wd_pid_write(const char *filename, pid_t pid);
pid_t wd_pid_read(const char *filename);

wd_status_t wd_thread_launch(thrd_start_t func, void *udata);
void wd_thread_finished();


#endif
