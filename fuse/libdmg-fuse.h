#ifndef MAIN_FUSE_H
#define MAIN_FUSE_H
#define FUSE_USE_VERSION 26

#include <fuse.h>

static void showHelp(const char* argv0);

int hfs_stat(const char* path, struct stat* stat);
int hfs_readlink(const char* path, char* buf, size_t size);
int hfs_open(const char* path, struct fuse_file_info* info);
int hfs_read(const char* path, char* buf, size_t bytes, off_t offset, struct fuse_file_info* info);
int hfs_release(const char* path, struct fuse_file_info* info);
int hfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* info);
int hfs_releasedir(const char *, struct fuse_file_info *);
#ifdef __APPLE__
  int hfs_getxattr(const char* path, const char* name, char* value, size_t vlen, uint32_t unknown);
#else
  int hfs_getxattr(const char* path, const char* name, char* value, size_t vlen);
#endif
int hfs_listxattr(const char* path, char* buffer, size_t size);

#endif

