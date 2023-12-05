/**
 * (C)2020 aks
 * https://github.com/akscf/
 **/
#include <whisperd.h>
/*
 * Copyright (c) 2002, Christopher Clark
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
  Credit for primes table: Aaron Krowne
  http://br.endernet.org/~akrowne/
  http://planetmath.org/encyclopedia/GoodHashTablePrimes.html
 */
struct wd_hashtable_entry {
    void                        *k, *v;
    unsigned int                 h;
    wd_hashtable_flag_t        flags;
    wd_hashtable_destructor_t  destructor;
    struct wd_hashtable_entry  *next;
};
struct wd_hashtable_iterator {
    unsigned int                pos;
    struct wd_hashtable_entry  *e;
    struct wd_hashtable        *h;
};
struct wd_hashtable {
    unsigned int                tablelength;
    struct wd_hashtable_entry  **table;
    unsigned int                entrycount;
    unsigned int                loadlimit;
    unsigned int                primeindex;
    unsigned int (*hashfn) (void *k);
    int (*eqfn) (void *k1, void *k2);
};

static const unsigned int primes[] = {
    53, 97, 193, 389,
    769, 1543, 3079, 6151,
    12289, 24593, 49157, 98317,
    196613, 393241, 786433, 1572869,
    3145739, 6291469, 12582917, 25165843,
    50331653, 100663319, 201326611, 402653189,
    805306457, 1610612741
};
const unsigned int prime_table_length = (sizeof (primes) / sizeof (primes[0]));
const float max_load_factor = 0.65f;
typedef struct wd_hashtable wd_hashtable_t;
typedef struct wd_hashtable_iterator wd_hashtable_iterator_t;

#define ht__zmalloc(ptr, len) (void)(assert((ptr = calloc(1, (len)))),ptr)
#define ht__freekey(X) free(X)
#define ht__xmalloc(sz) malloc(sz)
#define ht__xfree(p) free(p)
#define ht__safe_free(it) if (it) {free(it);it=NULL;}
#define ht__assert(x) assert(x)

static inline unsigned int hash(struct wd_hashtable *h, void *k) {
    /* Aim to protect against poor hash functions by adding logic here - logic taken from java 1.4 hashtable source */
    unsigned int i = h->hashfn(k);
    i += ~(i << 9);
    i ^= ((i >> 14) | (i << 18)); /* >>> */
    i += (i << 4);
    i ^= ((i >> 10) | (i << 22)); /* >>> */
    return i;
}

static __inline__ unsigned int indexFor(unsigned int tablelength, unsigned int hashvalue) {
    return (hashvalue % tablelength);
}


/* https://code.google.com/p/stringencoders/wiki/PerformanceAscii
   http://www.azillionmonkeys.com/qed/asmexample.html
 */
static uint32_t c_tolower(uint32_t eax) {
    uint32_t ebx = (0x7f7f7f7ful & eax) + 0x25252525ul;
    ebx = (0x7f7f7f7ful & ebx) + 0x1a1a1a1aul;
    ebx = ((ebx & ~eax) >> 2) & 0x20202020ul;
    return eax + ebx;
}

// ----------------------------------------------------------------------------------------------------------------------------------------------
/**
 **
 **/
wd_status_t wd_hashtable_create(wd_hashtable_t **hp, unsigned int minsize, unsigned int (*hashf) (void*), int (*eqf) (void*, void*)) {
    wd_hashtable_t *h;
    unsigned int pindex, size = primes[0];

    /* Check requested hashtable isn't too large */
    if (minsize > (1u << 30)) {
        *hp = NULL;
        return WD_STATUS_FALSE;
    }
    /* Enforce size as prime */
    for (pindex = 0; pindex < prime_table_length; pindex++) {
        if (primes[pindex] > minsize) {
            size = primes[pindex];
            break;
        }
    }
    h = (wd_hashtable_t *) ht__xmalloc(sizeof (wd_hashtable_t));
    if (h == NULL) {
        syslog(LOG_ERR, "malloc failed [%s:%d]", __FILE__, __LINE__);
        return WD_STATUS_FALSE;
    }

    h->table = (struct wd_hashtable_entry **) ht__xmalloc(sizeof (struct wd_hashtable_entry*) * size);
    if (h->table == NULL) {
        syslog(LOG_ERR, "malloc failed [%s:%d]", __FILE__, __LINE__);
        ht__xfree(h);
        return WD_STATUS_FALSE;
    }

    memset(h->table, 0, size * sizeof (struct wd_hashtable_entry *));
    h->tablelength = size;
    h->primeindex = pindex;
    h->entrycount = 0;
    h->hashfn = hashf;
    h->eqfn = eqf;
    h->loadlimit = (unsigned int) ceil(size * max_load_factor);

    *hp = h;
    return WD_STATUS_SUCCESS;
}

/**
 **
 **/
static int hashtable_expand(wd_hashtable_t *h) {
    /* Double the size of the table to accommodate more entries */
    struct wd_hashtable_entry **newtable;
    struct wd_hashtable_entry *e;
    struct wd_hashtable_entry **pE;
    unsigned int newsize, i, index;

    /* Check we're not hitting max capacity */
    if (h->primeindex == (prime_table_length - 1)) return 0;
    newsize = primes[++(h->primeindex)];
    newtable = (struct wd_hashtable_entry **) ht__xmalloc(sizeof (struct wd_hashtable_entry*) * newsize);

    if (newtable != NULL) {
        memset(newtable, 0, newsize * sizeof (struct wd_hashtable_entry *));
        /* This algorithm is not 'stable'. ie. it reverses the list when it transfers entries between the tables */
        for (i = 0; i < h->tablelength; i++) {
            while (NULL != (e = h->table[i])) {
                h->table[i] = e->next;
                index = indexFor(newsize, e->h);
                e->next = newtable[index];
                newtable[index] = e;
            }
        }
        ht__safe_free(h->table);
        h->table = newtable;
    } else {
        /* Plan B: realloc instead */
        newtable = (struct wd_hashtable_entry **) realloc(h->table, newsize * sizeof (struct wd_hashtable_entry *));
        if (NULL == newtable) {
            (h->primeindex)--;
            return 0;
        }
        h->table = newtable;
        memset(newtable[h->tablelength], 0, newsize - h->tablelength);
        for (i = 0; i < h->tablelength; i++) {
            for (pE = &(newtable[i]), e = *pE; e != NULL; e = *pE) {
                index = indexFor(newsize, e->h);
                if (index == i) {
                    pE = &(e->next);
                } else {
                    *pE = e->next;
                    e->next = newtable[index];
                    newtable[index] = e;
                }
            }
        }
    }
    h->tablelength = newsize;
    h->loadlimit = (unsigned int) ceil(newsize * max_load_factor);
    return -1;
}

/**
 **
 **/
unsigned int wd_hashtable_count(wd_hashtable_t *h) {
    return h->entrycount;
}

/**
 **
 **/
static void * _wd_hashtable_remove(wd_hashtable_t *h, void *k, unsigned int hashvalue, unsigned int index) {
    /* TODO: consider compacting the table when the load factor drops enough, or provide a 'compact' method. */

    struct wd_hashtable_entry *e;
    struct wd_hashtable_entry **pE;
    void *v;

    pE = &(h->table[index]);
    e = *pE;
    while (NULL != e) {
        /* Check hash value to short circuit heavier comparison */
        if ((hashvalue == e->h) && (h->eqfn(k, e->k))) {
            *pE = e->next;
            h->entrycount--;
            v = e->v;
            if (e->flags & WD_HASHTABLE_FLAG_FREE_KEY) {
                ht__freekey(e->k);
            }
            if (e->flags & WD_HASHTABLE_FLAG_FREE_VALUE) {
                ht__safe_free(e->v);
                v = NULL;
            } else if (e->destructor) {
                e->destructor(e->v);
                v = e->v = NULL;
            }
            ht__safe_free(e);
            return v;
        }
        pE = &(e->next);
        e = e->next;
    }
    return NULL;
}

/**
 **
 **/
int wd_hashtable_insert_destructor(wd_hashtable_t *h, void *k, void *v, wd_hashtable_flag_t flags, wd_hashtable_destructor_t destructor) {
    struct wd_hashtable_entry *e;
    unsigned int hashvalue = hash(h, k);
    unsigned index = indexFor(h->tablelength, hashvalue);

    if (flags & WD_HASHTABLE_DUP_CHECK) {
        _wd_hashtable_remove(h, k, hashvalue, index);
    }

    if (++(h->entrycount) > h->loadlimit) {
        /* Ignore the return value. If expand fails, we should
         * still try cramming just this value into the existing table
         * -- we may not have memory for a larger table, but one more
         * element may be ok. Next time we insert, we'll try expanding again.*/
        hashtable_expand(h);
        index = indexFor(h->tablelength, hashvalue);
    }
    e = (struct wd_hashtable_entry *) ht__xmalloc(sizeof (struct wd_hashtable_entry));
    if (e == NULL) {
        --(h->entrycount); /*oom*/
        return 0;
    }
    e->h = hashvalue;
    e->k = k;
    e->v = v;
    e->flags = flags;
    e->destructor = destructor;
    e->next = h->table[index];
    h->table[index] = e;
    return -1;
}

/**
 ** returns value associated with key
 **/
void *wd_hashtable_search(wd_hashtable_t *h, void *k) {
    struct wd_hashtable_entry *e;
    unsigned int hashvalue, index;
    hashvalue = hash(h, k);
    index = indexFor(h->tablelength, hashvalue);
    e = h->table[index];
    while (NULL != e) {
        /* Check hash value to short circuit heavier comparison */
        if ((hashvalue == e->h) && (h->eqfn(k, e->k))) return e->v;
        e = e->next;
    }
    return NULL;
}

/**
 ** returns value associated with key
 **/
void *wd_hashtable_remove(wd_hashtable_t *h, void *k) {
    unsigned int hashvalue = hash(h, k);
    return _wd_hashtable_remove(h, k, hashvalue, indexFor(h->tablelength, hashvalue));
}

/**
 ** destroy
 **/
void wd_hashtable_destroy(wd_hashtable_t **h) {
    unsigned int i;
    struct wd_hashtable_entry *e, *f;
    struct wd_hashtable_entry **table = (*h)->table;

    for (i = 0; i < (*h)->tablelength; i++) {
        e = table[i];
        while (NULL != e) {
            f = e;
            e = e->next;

            if (f->flags & WD_HASHTABLE_FLAG_FREE_KEY) {
                ht__freekey(f->k);
            }

            if (f->flags & WD_HASHTABLE_FLAG_FREE_VALUE) {
                ht__safe_free(f->v);
            } else if (f->destructor) {
                f->destructor(f->v);
                f->v = NULL;
            }
            ht__safe_free(f);
        }
    }

    ht__safe_free((*h)->table);
    free(*h);
    *h = NULL;
}

/**
 **
 **/
wd_hashtable_iterator_t *wd_hashtable_next(wd_hashtable_iterator_t **iP) {
    wd_hashtable_iterator_t *i = *iP;

    if (i->e) {
        if ((i->e = i->e->next) != 0) {
            return i;
        } else {
            i->pos++;
        }
    }
    while (i->pos < i->h->tablelength && !i->h->table[i->pos]) {
        i->pos++;
    }
    if (i->pos >= i->h->tablelength) {
        goto end;
    }
    if ((i->e = i->h->table[i->pos]) != 0) {
        return i;
    }

end:
    free(i);
    *iP = NULL;

    return NULL;
}

/**
 **
 **/
wd_hashtable_iterator_t *wd_hashtable_first_iter(wd_hashtable_t *h, wd_hashtable_iterator_t *it) {
    wd_hashtable_iterator_t *iterator;

    if (it) {
        iterator = it;
    } else {
        ht__zmalloc(iterator, sizeof (*iterator));
    }

    ht__assert(iterator);

    iterator->pos = 0;
    iterator->e = NULL;
    iterator->h = h;

    return wd_hashtable_next(&iterator);
}

/**
 **
 **/
void wd_hashtable_this_val(wd_hashtable_iterator_t *i, void *val) {
    if (i->e) {
        i->e->v = val;
    }
}

/**
 **
 **/
void wd_hashtable_this(wd_hashtable_iterator_t *i, const void **key, size_t *klen, void **val) {
    if (i->e) {
        if (key) {
            *key = i->e->k;
        }
        if (klen) {
            *klen = (int) strlen(i->e->k);
        }
        if (val) {
            *val = i->e->v;
        }
    } else {
        if (key) {
            *key = NULL;
        }
        if (klen) {
            *klen = 0;
        }
        if (val) {
            *val = NULL;
        }
    }
}

// ===================================================================================================================================================================
// PUB
// ===================================================================================================================================================================

static inline uint32_t wd_hash_default_int(void *ky) {
    uint32_t x = *((uint32_t *) ky);
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x);
    return x;
}

static inline int wd_hash_equalkeys_int(void *k1, void *k2) {
    return *(uint32_t *) k1 == *(uint32_t *) k2;
}

static inline int wd_hash_equalkeys(void *k1, void *k2) {
    return strcmp((char *) k1, (char *) k2) ? 0 : 1;
}

static inline int wd_hash_equalkeys_ci(void *k1, void *k2) {
    return strcasecmp((char *) k1, (char *) k2) ? 0 : 1;
}

static inline uint32_t wd_hash_default(void *ky) {
    unsigned char *str = (unsigned char *) ky;
    uint32_t hash = 0;
    int c;

    while ((c = *str)) {
        str++;
        hash = c + (hash << 6) + (hash << 16) - hash;
    }

    return hash;
}

static inline uint32_t wd_hash_default_ci(void *ky) {
    unsigned char *str = (unsigned char *) ky;
    uint32_t hash = 0;
    int c;

    while ((c = c_tolower(*str))) {
        str++;
        hash = c + (hash << 6) + (hash << 16) - hash;
    }
    return hash;
}

// ===================================================================================================================================================================

wd_status_t wd_hash_init_case(wd_hash_t **hash, bool case_sensitive) {
    if (case_sensitive) {
        return wd_hashtable_create(hash, 16, wd_hash_default, wd_hash_equalkeys);
    } else {
        return wd_hashtable_create(hash, 16, wd_hash_default_ci, wd_hash_equalkeys_ci);
    }
}

wd_status_t wd_hash_destroy(wd_hash_t **hash) {
    ht__assert(hash != NULL && *hash != NULL);
    wd_hashtable_destroy(hash);
    return WD_STATUS_SUCCESS;
}

wd_status_t wd_hash_insert_destructor(wd_hash_t *hash, const char *key, const void *data, wd_hashtable_destructor_t destructor) {
    int r = 0;
    r = wd_hashtable_insert_destructor(hash, strdup(key), (void *) data, WD_HASHTABLE_FLAG_FREE_KEY | WD_HASHTABLE_DUP_CHECK, destructor);
    return (r ? WD_STATUS_SUCCESS : WD_STATUS_FALSE);
}

void *wd_hash_delete(wd_hash_t *hash, const char *key) {
    return wd_hashtable_remove(hash, (void *) key);
}

unsigned int wd_hash_size(wd_hash_t *hash) {
    return wd_hashtable_count(hash);
}

void *wd_hash_find(wd_hash_t *hash, const char *key) {
    return wd_hashtable_search(hash, (void *) key);
}

bool wd_hash_empty(wd_hash_t *hash) {
    wd_hash_index_t *hi = wd_hash_first(hash);
    if (hi) {
        ht__safe_free(hi);
        return false;
    }
    return true;
}

wd_hash_index_t *wd_hash_first(wd_hash_t *hash) {
    return wd_hashtable_first_iter(hash, NULL);
}

wd_hash_index_t *wd_hash_first_iter(wd_hash_t *hash, wd_hash_index_t *hi) {
    return wd_hashtable_first_iter(hash, hi);
}

wd_hash_index_t *wd_hash_next(wd_hash_index_t **hi) {
    return wd_hashtable_next(hi);
}

void wd_hash_this(wd_hash_index_t *hi, const void **key, size_t *klen, void **val) {
    wd_hashtable_this(hi, key, klen, val);
}

void wd_hash_this_val(wd_hash_index_t *hi, void *val) {
    wd_hashtable_this_val(hi, val);
}

// ------------------------------------------------------------------------------------

wd_status_t wd_inthash_init(wd_inthash_t **hash) {
    return wd_hashtable_create(hash, 16, wd_hash_default_int, wd_hash_equalkeys_int);
}

wd_status_t wd_inthash_destroy(wd_inthash_t **hash) {
    wd_hashtable_destroy(hash);
    return WD_STATUS_SUCCESS;
}

wd_status_t wd_inthash_insert(wd_inthash_t *hash, uint32_t key, const void *data) {
    uint32_t *k = NULL;
    int r = 0;

    ht__zmalloc(k, sizeof (*k));
    *k = key;
    r = wd_hashtable_insert_destructor(hash, k, (void *) data, WD_HASHTABLE_FLAG_FREE_KEY | WD_HASHTABLE_DUP_CHECK, NULL);
    return (r ? WD_STATUS_SUCCESS : WD_STATUS_FALSE);
}

void *wd_core_inthash_delete(wd_inthash_t *hash, uint32_t key) {
    return wd_hashtable_remove(hash, (void *) &key);
}

void *wd_core_inthash_find(wd_inthash_t *hash, uint32_t key) {
    return wd_hashtable_search(hash, (void *) &key);
}
