/*
 *  Copyright (C) 2012-2013 Samsung Electronics Co., Ltd.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/************************************************************************/
/*                                                                      */
/*  PROJECT : exFAT & FAT12/16/32 File System                           */
/*  FILE    : xattr.c                                                   */
/*  PURPOSE : sdFAT code for supporting xattr(Extended File Attributes) */
/*                                                                      */
/*----------------------------------------------------------------------*/
/*  NOTES                                                               */
/*                                                                      */
/*                                                                      */
/************************************************************************/

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/xattr.h>
#include <linux/dcache.h>
#include "sdfat.h"

#ifndef CONFIG_SDFAT_VIRTUAL_XATTR_SELINUX_LABEL
#define CONFIG_SDFAT_VIRTUAL_XATTR_SELINUX_LABEL	("undefined")
#endif

static const char default_xattr[] = CONFIG_SDFAT_VIRTUAL_XATTR_SELINUX_LABEL;

static int can_support(const char *name)
{
	if (!name || strcmp(name, "security.selinux"))
		return -1;
	return 0;
}

ssize_t sdfat_listxattr(struct dentry *dentry, char *list, size_t size)
{
	return 0;
}


/*************************************************************************
 * INNER FUNCTIONS WHICH HAS KERNEL VERSION DEPENDENCY
 *************************************************************************/
static int __sdfat_xattr_check_support(const char *name)
{
	if (can_support(name))
		return -EOPNOTSUPP;

	return 0;
}

ssize_t __sdfat_getxattr(const char *name, void *value, size_t size)
{
	if (can_support(name))
		return -EOPNOTSUPP;

	if ((size > strlen(default_xattr)+1) && value)
		strcpy(value, default_xattr);

	return strlen(default_xattr);
}


/*************************************************************************
 * FUNCTIONS WHICH HAS KERNEL VERSION DEPENDENCY
 *************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
static int sdfat_xattr_get(const struct xattr_handler *handler,
		struct dentry *dentry, struct inode *inode,
		const char *name, void *buffer, size_t size)
{
	return __sdfat_getxattr(name, buffer, size);
}

static int sdfat_xattr_set(const struct xattr_handler *handler,
		struct dentry *dentry, struct inode *inode,
		const char *name, const void *value, size_t size,
		int flags)
{
	return __sdfat_xattr_check_support(name);
}

const struct xattr_handler sdfat_xattr_handler = {
	.prefix = "",  /* match anything */
	.get = sdfat_xattr_get,
	.set = sdfat_xattr_set,
};

const struct xattr_handler *sdfat_xattr_handlers[] = {
	&sdfat_xattr_handler,
	NULL
};

void setup_sdfat_xattr_handler(struct super_block *sb)
{
	sb->s_xattr = sdfat_xattr_handlers;
}
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0) */
int sdfat_setxattr(struct dentry *dentry, const char *name, const void *value, size_t size, int flags)
{
	return __sdfat_xattr_check_support(name);
}

ssize_t sdfat_getxattr(struct dentry *dentry, const char *name, void *value, size_t size)
{
	return __sdfat_getxattr(name, value, size);
}

int sdfat_removexattr(struct dentry *dentry, const char *name)
{
	return __sdfat_xattr_check_support(name);
}

void setup_sdfat_xattr_handler(struct super_block *sb)
{
	/* DO NOTHING */
}
#endif
