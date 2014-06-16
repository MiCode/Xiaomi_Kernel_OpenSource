/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef PFT_H_
#define PFT_H_

#include <linux/types.h>
#include <linux/fs.h>

#ifdef CONFIG_PFT

/* dm-req-crypt API */
int pft_get_key_index(struct inode *inode, u32 *key_index,
		      bool *is_encrypted, bool *is_inplace);

/* block layer API */
bool pft_allow_merge_bio(struct bio *bio1, struct bio *bio2);

/* --- security hooks , called from selinux --- */
int pft_inode_create(struct inode *dir, struct dentry *dentry, umode_t mode);

int pft_inode_post_create(struct inode *dir, struct dentry *dentry,
			  umode_t mode);

int pft_file_open(struct file *filp, const struct cred *cred);

int pft_file_permission(struct file *file, int mask);

int pft_file_close(struct file *filp);

int pft_inode_unlink(struct inode *dir, struct dentry *dentry);

int pft_inode_mknod(struct inode *dir, struct dentry *dentry, umode_t mode,
		    dev_t dev);

int pft_inode_rename(struct inode *inode, struct dentry *dentry,
		     struct inode *new_inode, struct dentry *new_dentry);

int pft_inode_set_xattr(struct dentry *dentry, const char *name);


#else
static inline int pft_get_key_index(struct inode *inode, u32 *key_index,
				    bool *is_encrypted, bool *is_inplace)
{ return -ENODEV; }

static inline bool pft_allow_merge_bio(struct bio *bio1, struct bio *bio2)
{ return true; }

static inline int pft_file_permission(struct file *file, int mask)
{ return 0; }

static inline int pft_inode_create(
	struct inode *dir, struct dentry *dentry, umode_t mode)
{ return 0; }

static inline int pft_inode_post_create(
	struct inode *dir, struct dentry *dentry, umode_t mode)
{ return 0; }

static inline int pft_file_open(struct file *filp, const struct cred *cred)
{ return 0; }

static inline int pft_file_close(struct file *filp)
{ return 0; }

static inline int pft_inode_unlink(struct inode *dir, struct dentry *dentry)
{ return 0; }

static inline int pft_inode_mknod(struct inode *dir, struct dentry *dentry,
				  umode_t mode, dev_t dev)
{ return 0; }

static inline int pft_inode_rename(struct inode *inode, struct dentry *dentry,
		     struct inode *new_inode, struct dentry *new_dentry)
{ return 0; }

static inline int pft_inode_set_xattr(struct dentry *dentry, const char *name)
{ return 0; }

#endif /* CONFIG_PFT */

#endif /* PFT_H */
