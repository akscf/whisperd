/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include <whisperd-file.h>

static wd_status_t l_dir_create_recursive(const char *dirname) {
    wd_status_t st = WD_STATUS_SUCCESS;
    char tmp[MAXNAMLEN];
    char *p = NULL;
    size_t len;
    int err;

    snprintf(tmp, sizeof(tmp), "%s", dirname);
    len = str_len((char *)tmp);

    if(tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for(p = tmp + 1; *p; p++) {
        if(*p == '/') {
            *p = 0;
            err = mkdir(tmp, S_IRWXU);
            if(err && errno != EEXIST) {
                st = (errno == 0 ? WD_STATUS_SUCCESS : errno);
                break;
            }
            *p = '/';
        }
    }

    if(st == WD_STATUS_SUCCESS) {
        err = mkdir(tmp, S_IRWXU);
        if(err && errno != EEXIST) {
            st = (errno == 0 ? WD_STATUS_SUCCESS : errno);
        }
    }

    return st;
}


// ---------------------------------------------------------------------------------------------------------------------------------------------
// dir
// ---------------------------------------------------------------------------------------------------------------------------------------------
bool wd_dir_exists(const char *dirname) {
    struct stat st = {0};

    if(!dirname) {
        return false;
    }
    if(stat(dirname, &st) == -1) {
        return false;
    }

    return(st.st_mode & S_IFDIR ? true : false);
}

wd_status_t wd_dir_create(const char *dirname, bool recursive) {
    if(!dirname) {
        return WD_STATUS_FALSE;
    }

    if(!recursive) {
        if(mkdir(dirname, S_IRWXU) == 0) {
            return WD_STATUS_SUCCESS;
        }
        if(errno == EEXIST) {
            return WD_STATUS_SUCCESS;
        }
        return errno;
    }

    return l_dir_create_recursive(dirname);
}


// ---------------------------------------------------------------------------------------------------------------------------------------------
// file
// ---------------------------------------------------------------------------------------------------------------------------------------------
bool wd_file_exists(const char *filename) {
    struct stat st = {0};

    if(!filename) {
        return false;
    }
    if(stat(filename, &st) == -1) {
        return false;
    }

    return(st.st_mode & S_IFREG ? true : false);
}

