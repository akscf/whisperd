/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#ifndef WHISPERD_FILE_H
#define WHISPERD_FILE_H

#include <dirent.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <whisperd.h>

wd_status_t wd_dir_create(const char *dirname, bool recursive);
bool wd_dir_exists(const char *dirname);

bool wd_file_exists(const char *filename);

#endif


