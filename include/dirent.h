/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __DIRENT_H
#define __DIRENT_H

#include <linux/list.h>

struct dirent {
	char d_name[256];
};

typedef struct dir {
	struct device *dev;
	struct fs_driver *fsdrv;
	struct dirent d;
	void *priv; /* private data for the fs driver */
	int fd;
	struct list_head entries;
} DIR;

DIR *opendir(const char *pathname);
DIR *fdopendir(int fd);
struct dirent *readdir(DIR *dir);
int unreaddir(DIR *dir, const struct dirent *d);
int rewinddir(DIR *dir);
int countdir(DIR *dir);
int closedir(DIR *dir);

#endif /* __DIRENT_H */
