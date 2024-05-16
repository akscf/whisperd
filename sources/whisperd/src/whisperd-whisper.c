/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include <whisperd-whisper.h>
#include <whisperd-config.h>
#include <whisperd-model.h>

extern whisperd_global_t *wd_global;

extern wd_status_t wd_whisper_worker_destroy_obj(wd_whisper_worker_t *worker);
extern wd_status_t wd_whisper_worket_init_obj(wd_whisper_worker_t *worker, wd_model_description_t *model);
extern wd_status_t wd_whisper_worker_transcript(wd_whisper_worker_t *worker, wd_whisper_trans_ctx_t *tctx);

static void destructor__wd_whisper_trans_ctx_t(void *data) {
    wd_whisper_trans_ctx_t *ptr = (wd_whisper_trans_ctx_t *)data;

    mem_deref(ptr->text_buffer);
    mem_deref(ptr->language);
    mem_deref(ptr->model_name);
    mem_deref(ptr->eparams);
    mem_deref(ptr->xx_sim_text);
}

static void destructor__wd_whisper_worker_t(void *data) {
    wd_whisper_worker_t *ptr = (wd_whisper_worker_t *)data;
    if(ptr) {
       wd_whisper_worker_destroy_obj(ptr);
    }
}

static int list_cb_find_worker(int idx, void *obj) {
    wd_whisper_worker_t *worker = (wd_whisper_worker_t *) obj;
    if(!worker->busy) { return true; }
    return false;
}

// ----------------------------------------------------------------------------------------------------------------------------------
// public
// ----------------------------------------------------------------------------------------------------------------------------------
wd_status_t wd_whisper_trans_ctx_alloc(wd_whisper_trans_ctx_t **ctx, char *model_name) {
    wd_status_t status = WD_STATUS_SUCCESS;
    wd_whisper_trans_ctx_t *ctx_local = NULL;

    ctx_local = (wd_whisper_trans_ctx_t *) mem_zalloc(sizeof(wd_whisper_trans_ctx_t), destructor__wd_whisper_trans_ctx_t);
    if(ctx_local == NULL) {
        log_error("mem fail");
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }

    ctx_local->text_buffer = mbuf_alloc(4096);
    if(ctx_local->text_buffer == NULL) {
        log_error("mem fail");
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }
    ctx_local->segments = NULL;
    str_dup(&ctx_local->model_name, model_name);

    *ctx = ctx_local;
out:
    if(status != WD_STATUS_SUCCESS) {
        mem_deref(ctx_local);
    }
    return status;
}

wd_status_t wd_whisper_worker_alloc(wd_whisper_worker_t **worker, wd_model_description_t *model) {
    wd_status_t status = WD_STATUS_SUCCESS;
    wd_whisper_worker_t *w_local = NULL;

    w_local = (wd_whisper_worker_t *) mem_zalloc(sizeof(wd_whisper_worker_t), destructor__wd_whisper_worker_t);
    if(w_local == NULL) {
        log_error("mem fail");
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }
    w_local->id = rand_u32();

    if((status = wd_whisper_worket_init_obj(w_local, model)) != WD_STATUS_SUCCESS) {
        log_error("wd_whisper_worket_init_obj() failed");
        log_mem_fail_goto_status(WD_STATUS_FALSE, out);
    }

    *worker = w_local;
out:
    if(status != WD_STATUS_SUCCESS) {
        mem_deref(w_local);
    }
    return status;
}

wd_status_t wd_whisper_transcript(wd_whisper_trans_ctx_t *tctx) {
    wd_status_t status = WD_STATUS_SUCCESS;
    wd_model_description_t *model_descr = tctx->model_descr_ref;
    wd_whisper_worker_t *worker = NULL;
    wd_list_t *workres = NULL;

    mtx_lock(wd_global->mutex);

    workres = (wd_list_t *) wd_hash_find(wd_global->workers_hash, model_descr->name);
    if(workres == NULL) {
        log_error("workres == NULL");
        wd_goto_status(WD_STATUS_FALSE, done);
    }

    if((worker = wd_list_find(workres, list_cb_find_worker)) == NULL) {
        if(wd_list_get_size(workres) >= WD_WORKERS_PER_MODEL) {
            log_error("Too many active workers for model: %s (max=%i)", model_descr->name, WD_WORKERS_PER_MODEL);
            wd_goto_status(WD_STATUS_FALSE, done);
        }

        if(wd_whisper_worker_alloc(&worker, model_descr) != WD_STATUS_SUCCESS) {
            log_error("mem fail");
            log_mem_fail_goto_status(WD_STATUS_FALSE, done);
        }

        if(wd_list_add_tail(workres, worker) != WD_STATUS_SUCCESS) {
            log_error("wd_list_add() fail");
            log_mem_fail_goto_status(WD_STATUS_FALSE, done);
        }
    }

done:
    mtx_unlock(wd_global->mutex);

    if(worker) {
        status = wd_whisper_worker_transcript(worker, tctx);
    }

    return status;
}










