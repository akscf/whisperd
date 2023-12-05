/**
 * (C)2020 aks
 * https://github.com/akscf/
 **/
#include <whisperd.h>

typedef struct wd_list_item_s {
    void                    *data;
    struct wd_list_item_s   *next;
} wd_list_item_t;

typedef struct wd_list_s {
    size_t                  size;
    wd_list_item_t          *head;
    wd_list_item_t          *tail;
} wd_list_t;

static void destructor__wd_list_t(void *data) {
    wd_list_t *list = (wd_list_t*)data;
    wd_list_item_t *t = NULL, *tt = NULL;

    t = list->head;
    while(t != NULL) {
        tt = t;
        t = t->next;
        mem_deref(tt);
    }
}

static void destructor__wd_list_item_t(void *data) {
    wd_list_item_t *item = (wd_list_item_t*)data;

    mem_deref(item->data);
}


// ----------------------------------------------------------------------------------------------------------------------------------
// public
// ----------------------------------------------------------------------------------------------------------------------------------
wd_status_t wd_list_create(wd_list_t **list) {
    *list = mem_zalloc(sizeof(wd_list_t), destructor__wd_list_t);
    return WD_STATUS_SUCCESS;
}

wd_status_t wd_list_destroy(wd_list_t **list) {
    wd_list_t *tmp_list = NULL;

    if(*list) {
        tmp_list = *list;
        *list = NULL;

        mem_deref(tmp_list);
    }

    return WD_STATUS_SUCCESS;
}

wd_status_t wd_list_add(wd_list_t *list, int pos, void *data) {
    wd_list_item_t *item = NULL, *target = NULL;

    if(!list || !data) {
        return WD_STATUS_FALSE;
    }

    if(list->size <= 0) {
        item = mem_zalloc(sizeof(wd_list_item_t), destructor__wd_list_item_t);
        if(!item) { return WD_STATUS_FALSE; }

        item->data = data;
        item->next = NULL;
        list->head = item;
        list->tail = item;
        list->size = 1;

        return WD_STATUS_SUCCESS;
    }

    // at head
    if(pos == 0) {
        item = mem_zalloc(sizeof(wd_list_item_t), destructor__wd_list_item_t);
        if(!item) { return WD_STATUS_FALSE; }

        item->data = data;
        item->next = list->head;
        list->head = item;
        list->size = (list->size + 1);

        return WD_STATUS_SUCCESS;
    }

    // at tail
    if(pos < 0 || pos >= list->size) {
        item = mem_zalloc(sizeof(wd_list_item_t), destructor__wd_list_item_t);
        if(!item) { return WD_STATUS_FALSE; }

        item->data = data;
        item->next = NULL;
        list->tail->next = item;
        list->tail = item;
        list->size = (list->size + 1);

        return WD_STATUS_SUCCESS;
    }

    // at certain position
    target = list->head;
    for (int i = 0; i < list->size; i++) {
        target = target->next;
        if(pos == (i + 1)) break;
    }
    if(!target) {
        return WD_STATUS_FALSE;
    }

    item = mem_zalloc(sizeof(wd_list_item_t), destructor__wd_list_item_t);
    if(!item) { return WD_STATUS_FALSE; }

    item->data = data;
    item->next = target->next->next;
    target->next = item;
    list->size = (list->size + 1);

    return WD_STATUS_SUCCESS;
}

wd_status_t wd_list_del(wd_list_t *list, int pos) {
    wd_list_item_t *item = NULL, *target = NULL;

    if(!list || pos < 0) {
        return WD_STATUS_FALSE;
    }

    if(list->size == 0) {
        return WD_STATUS_SUCCESS;
    }
    if(pos > list->size) {
        return WD_STATUS_FALSE;
    }

    if(pos == 0) {
        item = list->head;
        list->head = item->next;

        mem_deref(item);

        list->size = (list->size - 1);
        if (!list->size) {
            list->tail = NULL;
            list->head = NULL;
        }
        return WD_STATUS_FALSE;
    }

    target = list->head;
    for(int i = 0; i < list->size; i++) {
        target = target->next;
        if (pos == (i + 1)) break;
    }

    if (!target) {
        return WD_STATUS_FALSE;
    }
    item = target->next;
    target->next = item->next;

    mem_deref(item);

    list->size = (list->size - 1);
    if(list->size == 0) {
        list->tail = NULL;
        list->head = NULL;
    }

    return WD_STATUS_SUCCESS;
}

void *wd_list_get(wd_list_t *list, int pos) {
    wd_list_item_t *target = NULL;

    if(!list || pos < 0) {
        return NULL;
    }

    if(list->size == 0 || pos > list->size) {
        return NULL;
    }

    if(pos == list->size) {
        target = list->tail;
    } else {
        target = list->head;
        for (int i = 0; i < list->size; i++) {
            if (i == pos) break;
            target = target->next;
        }
    }

    return (target ? target->data : NULL);
}

wd_status_t wd_list_foreach(wd_list_t *list, void (*cb)(int, void *)) {
    wd_list_item_t *target = NULL;

    if(!list || cb == NULL) {
        return WD_STATUS_FALSE;
    }
    if(list->size == 0) {
        return WD_STATUS_SUCCESS;
    }

    target = list->head;
    for(int i = 0; i < list->size; i++) {
        cb(i, target->data);
        target = target->next;
    }

    return WD_STATUS_SUCCESS;
}

void *wd_list_find(wd_list_t *list, int (*cb)(int, void *)) {
    wd_list_item_t *target = NULL;
    void *result = NULL;

    if(!list || cb == NULL) {
        return NULL;
    }
    if(list->size == 0) {
        return NULL;
    }

    target = list->head;
    for(int i = 0; i < list->size; i++) {
        if(cb(i, target->data)) {
            result = target->data;
            break;
        }
        target = target->next;
    }

    return result;
}

int wd_list_get_size(wd_list_t *list) {
    if(!list) {
        return -1;
    }
    return list->size;
}
