/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#ifndef WHISPERD_MODEL_H
#define WHISPERD_MODEL_H
#include <whisperd.h>

typedef struct {
    char        *name;
    char        *alias;
    char        *path;
    char        *language; // force
} wd_model_description_t;

wd_status_t wd_model_description_alloc(wd_model_description_t **descr);
wd_status_t wd_model_register(wd_model_description_t *descr);
wd_model_description_t *wd_model_lookup(const char *name);

#endif

