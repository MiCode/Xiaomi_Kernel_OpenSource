/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

/*
 * Per-File-Tagger (PFT).
 *
 * This driver tags enterprise file for encryption/decryption,
 * as part of the Per-File-Encryption (PFE) feature.
 *
 * Enterprise registered applications are identified by their UID.
 *
 * The PFT exposes character-device interface to the user-space application,
 * to handle the following commands:
 * 1. Update registered applications list
 * 2. Encryption (in-place) of a file that was created before.
 * 3. Set State - update the state.
 *
 * The PFT exposes kernel API hooks that are intercepting file operations
 * like create/open/read/write for tagging files and also for access control.
 * It utilizes the existing security framework hooks
 * that calls the selinux hooks.
 *
 * The PFT exposes kernel API to the dm-req-crypt driver to provide the info
 * if a file is tagged or not. The dm-req-crypt driver is doing the
 * actual encryption/decryptiom.
 *
 * Tagging the file:
 * 1. Non-volatile tagging on storage using file extra-attribute (xattr).
 * 2. Volatile tagging on the file's inode, for fast access.
 *
 */

/* Uncomment the line below to enable debug messages */
/* #define DEBUG 1 */

#define pr_fmt(fmt)	"pft [%s]: " fmt, __func__

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/cred.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/bitops.h>
#include <linux/fdtable.h>
#include <linux/selinux.h>

#include <linux/pft.h>
#include <linux/msm_pft.h>

#include "objsec.h"

/* File tagging as encrypted/non-encrypted is valid */
#define PFT_TAG_MAGIC		((u32)(0xABC00000))

/* File tagged as encrypted */
#define PFT_TAG_ENCRYPTED	BIT(16)

#define PFT_TAG_MAGIC_MASK	0xFFF00000
#define PFT_TAG_FLAGS_MASK	0x000F0000
#define PFT_TAG_KEY_MASK	0x0000FFFF

/* The defualt encryption key index */
#define PFT_DEFAULT_KEY_INDEX	1

/* The defualt key index for non-encrypted files */
#define PFT_NO_KEY		0

/* PFT extended attribute name */
#define XATTR_NAME_PFE "security.pfe"

/* PFT driver requested major number */
#define PFT_REQUESTED_MAJOR	213

/* PFT driver name */
#define PFT_DEVICE_NAME	"pft"

/* Maximum registered applications */
#define PFT_MAX_APPS	1000

/* Maximum command size */
#define PFT_MAX_COMMAND_SIZE (PAGE_SIZE)

/* Current Process ID */
#define current_pid() ((u32)(current->pid))

static const char *pft_state_name[PFT_STATE_MAX_INDEX] = {
	"deactivated",
	"deactivating",
	"key_removed",
	"removing_key",
	"key_loaded",
};

/**
 * struct pft_file_info - pft file node info.
 * @file:	pointer to file stucture.
 * @pid:	process ID.
 * @list:	next list item.
 *
 * A node in the list of the current open encrypted files.
 */
struct pft_file_info {
	struct file *file;
	pid_t pid;
	struct list_head list;
};

/**
 * struct pft_device - device state structure.
 *
 * @open_count:	device open count.
 * @major:	device major number.
 * @state:	Per-File-Encryption state.
 * @response:	command response.
 * @pfm_pid:	PFM process id.
 * @inplace_file:	file for in-place encryption.
 * @uid_table:	registered application array (UID).
 * @uid_count:	number of registered applications.
 * @open_file_list:	open encrypted file list.
 * @lock:	lock protect list access.
 *
 * The open_count purpose is to ensure that only one user space
 * application uses this driver.
 * The open_file_list is used to close open encrypted files
 * after the key is removed from the encryption hardware.
 */
struct pft_device {
	int open_count;
	int major;
	enum pft_state state;
	struct pft_command_response response;
	u32 pfm_pid;
	struct file *inplace_file;
	u32 *uid_table;
	u32 uid_count;
	struct list_head open_file_list;
	struct mutex lock;
};

/* Device Driver State */
static struct pft_device *pft_dev;

/**
 * pft_is_ready() - driver is initialized and ready.
 *
 * Return: true if the driver is ready.
 */
static bool pft_is_ready(void)
{
	return  (pft_dev != NULL);
}

/**
 * file_to_filename() - get the filename from file pointer.
 * @filp: file pointer
 *
 * it is used for debug prints.
 *
 * Return: filename string or "unknown".
 */
static char *file_to_filename(struct file *filp)
{
	struct dentry *dentry = NULL;
	char *filename = NULL;

	if (!filp || !filp->f_dentry)
		return "unknown";

	dentry = filp->f_dentry;
	filename = dentry->d_iname;

	return filename;
}

/**
 * inode_to_filename() - get the filename from inode pointer.
 * @inode: inode pointer
 *
 * it is used for debug prints.
 *
 * Return: filename string or "unknown".
 */
static char *inode_to_filename(struct inode *inode)
{
	struct dentry *dentry = NULL;
	char *filename = NULL;

	if (list_empty(&inode->i_dentry))
		return "unknown";

	dentry = list_first_entry(&inode->i_dentry, struct dentry, d_alias);

	filename = dentry->d_iname;

	return filename;
}

/**
 * ptf_set_response() - set response error code.
 *
 * @error_code: The error code to return on response.
 */
static inline void ptf_set_response(u32 error_code)
{
	pft_dev->response.error_code = error_code;
}

/**
 * pft_add_file()- Add the file to the list of opened encrypted
 * files.
 * @filp: file to add.
 *
 * Return: 0 of successful operation, negative value otherwise.
 */
static int pft_add_file(struct file *filp)
{
	struct pft_file_info *node = NULL;

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node) {
		pr_err("malloc failure\n");
		return -ENOMEM;
	}

	node->file = filp;
	INIT_LIST_HEAD(&node->list);

	mutex_lock(&pft_dev->lock);
	list_add(&node->list, &pft_dev->open_file_list);
	pr_debug("adding file %s to open list.\n", file_to_filename(filp));
	mutex_unlock(&pft_dev->lock);

	return 0;
}

/**
 * pft_remove_file()- Remove the given file from the list of
 * open encrypted files.
 * @filp: file to remove.
 *
 * Return: 0 on success, negative value on failure.
 */
static int pft_remove_file(struct file *filp)
{
	int ret = -ENOENT;
	struct pft_file_info *tmp = NULL;
	struct list_head *pos = NULL;
	bool found = false;

	mutex_lock(&pft_dev->lock);
	list_for_each(pos, &pft_dev->open_file_list) {
		tmp = list_entry(pos, struct pft_file_info, list);
		if (filp == tmp->file) {
			found = true;
			break;
		}
	}

	if (found) {
		pr_debug("remove file %s. from open list.\n ",
			 file_to_filename(filp));
		list_del(&tmp->list);
		kfree(tmp);
		ret = 0;
	}
	mutex_unlock(&pft_dev->lock);

	return ret;
}

/**
 * pft_is_current_process_registered()- Check if current process
 * is registered.
 *
 * Return: true if current process is registered.
 */
static bool pft_is_current_process_registered(void)
{
	int is_registered = false;
	int i;
	u32 uid = current_uid();

	mutex_lock(&pft_dev->lock);
	for (i = 0; i < pft_dev->uid_count; i++) {
		if (pft_dev->uid_table[i] == uid) {
			pr_debug("current UID [%u] is registerd.\n", uid);
			is_registered = true;
			break;
		}
	}
	mutex_unlock(&pft_dev->lock);

	return is_registered;
}

/**
 * pft_is_xattr_supported() - Check if the filesystem supports
 * extended attributes.
 * @indoe: pointer to the file inode
 *
 * Return: true if supported, false if not.
 */
static bool pft_is_xattr_supported(struct inode *inode)
{
	if (inode == NULL) {
		pr_err("invalid argument inode passed as NULL");
		return false;
	}

	if (inode->i_security == NULL) {
		pr_debug("i_security is NULL, not ready yet\n");
		return false;
	}

	if (inode->i_op == NULL) {
		pr_debug("i_op is NULL\n");
		return false;
	}

	if (inode->i_op->getxattr == NULL) {
		pr_debug_once("getxattr() not supported , filename=%s\n",
			      inode_to_filename(inode));
		return false;
	}

	if (inode->i_op->setxattr == NULL) {
		pr_debug("setxattr() not supported\n");
		return false;
	}

	return true;
}

/**
 * pft_get_inode_tag() - get the file tag.
 * @indoe: pointer to the file inode
 *
 * Return: tag
 */
static u32 pft_get_inode_tag(struct inode *inode)
{
	struct inode_security_struct *isec = inode->i_security;

	if (isec == NULL)
		return 0;

	return isec->tag;
}

/**
 * pft_get_inode_key_index() - get the file key.
 * @indoe: pointer to the file inode
 *
 * Return: key index
 */
static inline u32 pft_get_inode_key_index(struct inode *inode)
{
	return pft_get_inode_tag(inode) & PFT_TAG_KEY_MASK;
}

/**
 * pft_is_tag_valid() - is the tag valid
 * @indoe: pointer to the file inode
 *
 * The tagging is set to valid when an enterprise file is created
 * or when an file is opened first time after power up and the
 * xattr was checked to see if the file is encrypted or not.
 *
 * Return: true if the tag is valid.
 */
static inline bool pft_is_tag_valid(struct inode *inode)
{
	struct inode_security_struct *isec = inode->i_security;

	return ((isec->tag & PFT_TAG_MAGIC_MASK) == PFT_TAG_MAGIC) ?
		true : false;
}

/**
 * pft_is_file_encrypted() - is inode tagged as encrypted.
 *
 * @tag: holds the key index and tagging flags.
 *
 * Return: true if the file is encrypted.
 */
static inline bool pft_is_file_encrypted(u32 tag)
{
	return (tag & PFT_TAG_ENCRYPTED) ? true : false;
}

/**
 * pft_tag_inode_non_encrypted() - Tag the inode as
 * non-encrypted.
 * @indoe: pointer to the file inode
 *
 * Tag file as non-encrypted, only the valid bit is set,
 * the encrypted bit is not set.
 */
static inline void pft_tag_inode_non_encrypted(struct inode *inode)
{
	struct inode_security_struct *isec = inode->i_security;

	isec->tag = (u32)(PFT_TAG_MAGIC);
}

/**
 * pft_tag_inode_encrypted() - Tag the inode as encrypted.
 * @indoe: pointer to the file inode
 *
 * Set the valid bit, the encrypted bit, and the key index.
 */
static void pft_tag_inode_encrypted(struct inode *inode, u32 key_index)
{
	struct inode_security_struct *isec = inode->i_security;

	isec->tag = key_index | PFT_TAG_ENCRYPTED | PFT_TAG_MAGIC;
}

/**
 * pft_get_file_tag()- get the file tag.
 * @dentry:	pointer to file dentry.
 * @tag_ptr:	pointer to tag.
 *
 * This is the major function for detecting tag files.
 * Get the tag from the inode if tag is valid,
 * or from the xattr if this is the 1st time after power up.
 *
 * Return: 0 on successe, negative value on failure.
 */
static int pft_get_file_tag(struct dentry *dentry, u32 *tag_ptr)
{
	ssize_t size = 0;
	struct inode *inode;
	const char *xattr_name = XATTR_NAME_PFE;
	u32 key;

	if (!dentry || !dentry->d_inode || !tag_ptr) {
		pr_err("invalid param");
		return -EINVAL;
	}

	inode = dentry->d_inode;
	if (pft_is_tag_valid(inode)) {
		*tag_ptr = pft_get_inode_tag(inode);
		return 0;
	}

	/*
	 * For the first time reading the tag, the tag is not valid, hence
	 * get xattr.
	 */
	size = inode->i_op->getxattr(dentry, xattr_name, &key, sizeof(key));

	if (size == -ENODATA || size == -EOPNOTSUPP) {
		pft_tag_inode_non_encrypted(inode);
		*tag_ptr = pft_get_inode_tag(inode);
	} else if (size > 0) {
		pr_debug("First time file %s opened, found xattr = %u.\n",
		       inode_to_filename(inode), key);
		pft_tag_inode_encrypted(inode, key);
		*tag_ptr = pft_get_inode_tag(inode);
	} else {
		pr_err("getxattr() failure, ret=%d.\n", size);
		return -EINVAL;
	}

	return 0;
}

/**
 * pft_tag_file() - Tag the file saving the key_index.
 * @dentry:	file dentry.
 * @key_index:	encryption key index.
 *
 * This is the major fuction for tagging a file.
 * Tag the file on both the xattr and the inode.
 *
 * Return: 0 on successe, negative value on failure.
 */
static int pft_tag_file(struct dentry *dentry, u32 key_index)
{
	int size = 0;
	const char *xattr_name = XATTR_NAME_PFE;

	if (!dentry || !dentry->d_inode) {
		pr_err("invalid NULL param");
		return -EINVAL;
	}

	if (!pft_is_xattr_supported(dentry->d_inode)) {
		pr_err("set xattr for file %s is not support.\n",
		       dentry->d_iname);
		return -EINVAL;
	}

	size = dentry->d_inode->i_op->setxattr(dentry, xattr_name, &key_index,
					       sizeof(key_index), 0);
	if (size < 0) {
		pr_err("failed to set xattr for file %s, ret =%d.\n",
		       dentry->d_iname, size);
		return -EFAULT;
	}

	pft_tag_inode_encrypted(dentry->d_inode, key_index);
	pr_debug("file %s tagged encrypted\n", dentry->d_iname);

	return 0;
}

/**
 * pft_get_app_key_index() - get the application key index.
 * @uid: registered application UID
 *
 * Get key index based on the given registered application UID.
 * Currently only one key is supported.
 *
 * Return: encryption key index.
 */
static inline u32 pft_get_app_key_index(u32 uid)
{
	return PFT_DEFAULT_KEY_INDEX;
}

/**
 * pft_is_encrypted_file() - is the file encrypted.
 * @dentry: file pointer.
 *
 * Return: true if the file is encrypted, false otherwise.
 */
static bool pft_is_encrypted_file(struct dentry *dentry)
{
	int rc;
	u32 tag;

	if (!pft_is_ready())
		return false;

	if (!pft_is_xattr_supported(dentry->d_inode))
		return false;

	rc = pft_get_file_tag(dentry, &tag);
	if (rc < 0)
		return false;

	return pft_is_file_encrypted(tag);
}

/**
 * pft_is_encrypted_inode() - is the file encrypted.
 * @inode: inode of file to check.
 *
 * Return: true if the file is encrypted, false otherwise.
 */
static bool pft_is_encrypted_inode(struct inode *inode)
{
	u32 tag;

	if (!pft_is_ready())
		return false;

	if (!pft_is_xattr_supported(inode))
		return false;

	tag = pft_get_inode_tag(inode);

	return pft_is_file_encrypted(tag);
}

/**
 * pft_is_inplace_inode() - is this the inode of file for
 * in-place encryption.
 * @inode: inode of file to check.
 *
 * Return: true if this file is being encrypted, false
 * otherwise.
 */
static bool pft_is_inplace_inode(struct inode *inode)
{
	if (!pft_dev->inplace_file || !pft_dev->inplace_file->f_path.dentry)
		return false;

	return (pft_dev->inplace_file->f_path.dentry->d_inode == inode);
}

/**
 * pft_is_inplace_file() - is this the file for in-place
 * encryption.
 * @filp: file to check.
 *
 * A file struct might be allocated per process, inode should be
 * only one.
 *
 * Return: true if this file is being encrypted, false
 * otherwise.
 */
static inline bool pft_is_inplace_file(struct file *filp)
{
	if (!filp || !filp->f_path.dentry || !filp->f_path.dentry->d_inode)
		return false;

	return pft_is_inplace_inode(filp->f_path.dentry->d_inode);
}

/**
 * pft_get_key_index() - get the key index and other indications
 * @inode:	Pointer to inode struct
 * @key_index:	Pointer to the return value of key index
 * @is_encrypted:	Pointer to the return value.
 * @is_inplace:	Pointer to the return value.
 *
 * Provides the given inode's encryption key index, and well as
 * indications whether the file is encrypted or is it currently
 * being in-placed encrypted.
 * This API is called by the dm-req-crypt to decide if to
 * encrypt/decrypt the file.
 * File tagging depends on the hooks to be called from selinux,
 * so if selinux is disabled then tagging is also not
 * valid.
 *
 * Return: 0 on successe, negative value on failure.
 */
int pft_get_key_index(struct inode *inode, u32 *key_index,
		      bool *is_encrypted, bool *is_inplace)
{
	u32 tag = 0;

	if (!pft_is_ready())
		return -ENODEV;

	if (!selinux_is_enabled())
		return -ENODEV;

	if (!inode)
		return -EPERM;

	if (!is_encrypted) {
		pr_err("is_encrypted is NULL\n");
		return -EPERM;
	}
	if (!is_inplace) {
		pr_err("is_inplace is NULL\n");
		return -EPERM;
	}
	if (!key_index) {
		pr_err("key_index is NULL\n");
		return -EPERM;
	}

	if (!pft_is_tag_valid(inode)) {
		pr_debug("file %s, Tag not valid\n", inode_to_filename(inode));
		return -EINVAL;
	}

	if (!pft_is_xattr_supported(inode)) {
		*is_encrypted = false;
		*is_inplace = false;
		*key_index = 0;
		return 0;
	}

	tag = pft_get_inode_tag(inode);

	*is_encrypted = pft_is_file_encrypted(tag);
	*key_index = pft_get_inode_key_index(inode);
	*is_inplace = pft_is_inplace_inode(inode);

	if (*is_encrypted)
		pr_debug("file %s is encrypted\n", inode_to_filename(inode));

	return 0;
}
EXPORT_SYMBOL(pft_get_key_index);

/**
 * pft_bio_get_inode() - get the inode from a bio.
 * @bio: Pointer to BIO structure.
 *
 * Walk the bio struct links to get the inode.
 *
 * Return: pointer to the inode struct if successful, or NULL otherwise.
 */
static struct inode *pft_bio_get_inode(struct bio *bio)
{
	if (!bio || !bio->bi_io_vec || !bio->bi_io_vec->bv_page ||
	    !bio->bi_io_vec->bv_page->mapping)
		return NULL;

	return bio->bi_io_vec->bv_page->mapping->host;
}

/**
 * pft_allow_merge_bio()- Check if 2 BIOs can be merged.
 * @bio1:	Pointer to first BIO structure.
 * @bio2:	Pointer to second BIO structure.
 *
 * Prevent merging of BIOs from encrypted and non-encrypted
 * files, or files encrypted with different key.
 * This API is called by the file system block layer.
 *
 * Return: true if the BIOs allowed to be merged, false
 * otherwise.
 */
bool pft_allow_merge_bio(struct bio *bio1, struct bio *bio2)
{
	u32 key_index1 = 0, key_index2 = 0;
	bool is_encrypted1 = false, is_encrypted2 = false;
	bool allow = false;
	bool is_inplace = false; /* N.A. */
	int ret;

	ret = pft_get_key_index(pft_bio_get_inode(bio1), &key_index1,
				&is_encrypted1, &is_inplace);
	if (ret)
		is_encrypted1 = false;

	ret = pft_get_key_index(pft_bio_get_inode(bio2), &key_index2,
				&is_encrypted2, &is_inplace);
	if (ret)
		is_encrypted2 = false;

	allow = ((is_encrypted1 == is_encrypted2) &&
		 (key_index1 == key_index2));

	return allow;
}
EXPORT_SYMBOL(pft_allow_merge_bio);

/**
 * pft_inode_create() - file creation callback.
 * @dir:	directory inode pointer
 * @dentry:	file dentry pointer
 * @mode:	flags
 *
 * This hook is called when file is created by VFS.
 * This hook is called from the selinux driver.
 * This hooks check file creation permission for enterprise
 * applications.
 * Call path:
 * vfs_create()->security_inode_create()->selinux_inode_create()
 *
 * Return: 0 on successe, negative value on failure.
 */
int pft_inode_create(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	if (!dir || !dentry)
		return 0;

	if (!pft_is_ready())
		return 0;

	switch (pft_dev->state) {
	case PFT_STATE_DEACTIVATED:
	case PFT_STATE_KEY_LOADED:
		break;
	case PFT_STATE_KEY_REMOVED:
	case PFT_STATE_DEACTIVATING:
	case PFT_STATE_REMOVING_KEY:
		/* At this state no new encrypted files can be created */
		if (pft_is_current_process_registered()) {
			pr_debug("key removed, registered uid %u is denied from creating new file %s\n",
				current_uid(), dentry->d_iname);
			return -EACCES;
		}
		break;
	default:
		BUG(); /* State is set by "set state" command */
		break;
	}

	return 0;

}
EXPORT_SYMBOL(pft_inode_create);

/**
 * pft_inode_post_create() - file creation callback.
 * @dir:	directory inode pointer
 * @dentry:	file dentry pointer
 * @mode:	flags
 *
 * This hook is called when file is created by VFS.
 * This hook is called from the selinux driver.
 * This hooks tags new files as encrypted when created by
 * enterprise applications.
 * Call path:
 * vfs_create()->security_inode_post_create()->selinux_inode_post_create()
 *
 * Return: 0 on successe, negative value on failure.
 */
int pft_inode_post_create(struct inode *dir, struct dentry *dentry,
			  umode_t mode)
{
	int ret;

	if (!dir || !dentry)
		return 0;

	if (!pft_is_ready())
		return 0;

	switch (pft_dev->state) {
	case PFT_STATE_DEACTIVATED:
	case PFT_STATE_KEY_REMOVED:
	case PFT_STATE_DEACTIVATING:
	case PFT_STATE_REMOVING_KEY:
		break;
	case PFT_STATE_KEY_LOADED:
		/* Check whether the new file should be encrypted */
		if (pft_is_current_process_registered()) {
			u32 key_index = pft_get_app_key_index(current_uid());
			ret = pft_tag_file(dentry, key_index);
			if (ret == 0)
				pr_debug("key loaded, pid [%u] uid [%d] is creating file %s\n",
					 current_pid(), current_uid(),
					 dentry->d_iname);
			else {
				pr_err("Failed to tag file %s by pid %d\n",
					dentry->d_iname, current_pid());
				return -EFAULT;
			}
		}
		break;
	default:
		BUG(); /* State is set by "set state" command */
		break;
	}

	return 0;
}
EXPORT_SYMBOL(pft_inode_post_create);

/**
 * pft_inode_mknod() - mknode file hook (callback)
 * @dir:	directory inode pointer
 * @dentry:	file dentry pointer
 * @mode:	flags
 * @dev:
 *
 * This hook checks encrypted file access permission by
 * enterprise application.
 * Call path:
 * vfs_mknod()->security_inode_mknod()->selinux_inode_mknod()->pft_inode_mknod()
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
int pft_inode_mknod(struct inode *dir, struct dentry *dentry,
		    umode_t mode, dev_t dev)
{
	int rc;

	/* Check if allowed to create new encrypted files */
	rc = pft_inode_create(dir, dentry, mode);

	return rc;
}
EXPORT_SYMBOL(pft_inode_mknod);

/**
 * pft_inode_symlink() - symlink file hook (callback)
 * @dir:	directory inode pointer
 * @dentry:	file dentry pointer
 * @name:	Old file name
 *
 * Allow only enterprise app to create symlink to enterprise
 * file.
 * Call path:
 * vfs_symlink()->security_inode_symlink()->selinux_inode_symlink()
 *
 * Return: 0 on allowed operation, negative value otherwise.
 */
int pft_inode_symlink(struct inode *dir, struct dentry *dentry,
		      const char *name)
{
	struct inode *inode;

	if (!dir) {
		pr_err("dir is NULL.\n");
		return 0;
	}
	if (!dentry) {
		pr_err("dentry is NULL.\n");
		return 0;
	}
	if (!name) {
		pr_err("name is NULL.\n");
		return 0;
	}

	pr_debug("symlink for file [%s] dir [%s] dentry [%s] started! ....\n",
		 name, inode_to_filename(dir), dentry->d_iname);
	inode = dentry->d_inode;

	if (!dentry->d_inode) {
		pr_debug("d_inode is NULL.\n");
		return 0;
	}

	if (!pft_is_ready())
		return 0;

	/* do nothing for non-encrypted files */
	if (!pft_is_encrypted_inode(inode))
		return 0;

	/*
	 * Only PFM allowed to access in-place-encryption-file
	 * during in-place-encryption process
	 */
	if (pft_is_inplace_inode(inode)) {
		pr_err("symlink for in-place-encryption file %s by pid %d is blocked.\n",
			 inode_to_filename(inode), current_pid());
		return -EACCES;
	}

	switch (pft_dev->state) {
	case PFT_STATE_DEACTIVATED:
	case PFT_STATE_KEY_REMOVED:
	case PFT_STATE_DEACTIVATING:
	case PFT_STATE_REMOVING_KEY:
		/* Block any access for encrypted files when key not loaded */
		pr_debug("key not loaded. uid (%u) can not access file %s\n",
			 current_uid(), inode_to_filename(inode));
		return -EACCES;
	case PFT_STATE_KEY_LOADED:
		 /* Only registered apps may access encrypted files. */
		if (!pft_is_current_process_registered()) {
			pr_err("unregistered app uid %u pid %u is trying to access encrypted file %s\n",
			       current_uid(), current_pid(), name);
			return -EACCES;
		}
		break;
	default:
		BUG(); /* State is set by "set state" command */
		break;
	}

	pr_debug("symlink for file %s ok.\n", name);

	return 0;

}
EXPORT_SYMBOL(pft_inode_symlink);

/**
 * pft_inode_rename() - file rename hook.
 * @inode:	directory inode
 * @dentry:	file dentry
 * @new_inode
 * @new_dentry
 *
 * Block attempt to rename enterprise file.
 *
 * Return: 0 on allowed operation, negative value otherwise.
 */
int pft_inode_rename(struct inode *inode, struct dentry *dentry,
		     struct inode *new_inode, struct dentry *new_dentry)
{
	if (!inode || !dentry || !new_inode || !new_dentry || !dentry->d_inode)
		return 0;

	if (!pft_is_ready())
		return 0;

	/* do nothing for non-encrypted files */
	if (!pft_is_encrypted_file(dentry))
		return 0;

	pr_debug("attempt to rename encrypted file [%s]\n", dentry->d_iname);

	if (pft_is_inplace_inode(dentry->d_inode)) {
		pr_err("access in-place-encryption file %s by uid [%d] pid [%d] is blocked.\n",
		       inode_to_filename(inode), current_uid(), current_pid());
		return -EACCES;
	}

	if (!pft_is_current_process_registered()) {
		pr_err("unregistered app (uid %u pid %u) is trying to access encrypted file %s\n",
		       current_uid(), current_pid(), dentry->d_iname);
		return -EACCES;
	} else
		pr_debug("rename file %s\n", dentry->d_iname);

	return 0;
}
EXPORT_SYMBOL(pft_inode_rename);

/**
 * pft_file_open() - file open hook (callback).
 * @filp:	file pointer
 * @cred:	credentials pointer
 *
 * This hook is called when file is opened by VFS.
 * It is called from the selinux driver.
 * It checks enterprise file xattr when first opened.
 * It adds encrypted file to the list of open files.
 * Call path:
 * do_filp_open()->security_dentry_open()->selinux_dentry_open()
 *
 * Return: 0 on successe, negative value on failure.
 */
int pft_file_open(struct file *filp, const struct cred *cred)
{
	if (!filp || !filp->f_path.dentry)
		return 0;

	if (!pft_is_ready())
		return 0;

	/* do nothing for non-encrypted files */
	if (!pft_is_encrypted_file(filp->f_dentry))
		return 0;

	/*
	 * Only PFM allowed to access in-place-encryption-file
	 * during in-place-encryption process
	 */
	if (pft_is_inplace_file(filp) && current_pid() != pft_dev->pfm_pid) {
		pr_err("Access in-place-encryption file %s by uid %d pid %d is blocked.\n",
			 file_to_filename(filp), current_uid(), current_pid());
		return -EACCES;
	}

	switch (pft_dev->state) {
	case PFT_STATE_DEACTIVATED:
	case PFT_STATE_KEY_REMOVED:
	case PFT_STATE_DEACTIVATING:
	case PFT_STATE_REMOVING_KEY:
		/* Block any access for encrypted files when key not loaded */
		pr_debug("key not loaded. uid (%u) can not access file %s\n",
			 current_uid(), file_to_filename(filp));
		return -EACCES;
	case PFT_STATE_KEY_LOADED:
		 /* Only registered apps may access encrypted files. */
		if (!pft_is_current_process_registered()) {
			pr_err("unregistered app (uid %u pid %u) is trying to access encrypted file %s\n",
			       current_uid(), current_pid(),
			       file_to_filename(filp));
			return -EACCES;
		}

		pft_add_file(filp);
		break;
	default:
		BUG(); /* State is set by "set state" command */
		break;
	}

	return 0;
}
EXPORT_SYMBOL(pft_file_open);

/**
 * pft_file_permission() - check file access permission.
 * @filp:	file pointer
 * @mask:	flags
 *
 * This hook is called when file is read/write by VFS.
 * This hook is called from the selinux driver.
 * This hook checks encrypted file access permission by
 * enterprise application.
 * Call path:
 * vfs_read()->security_file_permission()->selinux_file_permission()
 *
 * Return: 0 on successe, negative value on failure.
 */
int pft_file_permission(struct file *filp, int mask)
{
	if (!filp)
		return 0;

	if (!pft_is_ready())
		return 0;

	/* do nothing for non-encrypted files */
	if (!pft_is_encrypted_file(filp->f_dentry))
		return 0;

	/*
	 * Only PFM allowed to access in-place-encryption-file
	 * during in-place encryption process
	 */
	if (pft_is_inplace_file(filp)) {
		if (current_pid() == pft_dev->pfm_pid) {
			/* mask MAY_WRITE=2 / MAY_READ=4 */
			pr_debug("r/w [mask 0x%x] in-place-encryption file %s by PFM (UID %d, PID %d).\n",
				 mask, file_to_filename(filp),
				 current_uid(), current_pid());
			return 0;
		} else {
			pr_err("Access in-place-encryption file %s by App (UID %d, PID %d) is blocked.\n",
			       file_to_filename(filp),
			       current_uid(), current_pid());
			return -EACCES;
		}
	}

	switch (pft_dev->state) {
	case PFT_STATE_DEACTIVATED:
	case PFT_STATE_KEY_REMOVED:
	case PFT_STATE_DEACTIVATING:
	case PFT_STATE_REMOVING_KEY:
		/* Block any access for encrypted files when key not loaded */
		pr_debug("key not loaded. uid (%u) can not access file %s\n",
			 current_uid(), file_to_filename(filp));
		return -EACCES;
	case PFT_STATE_KEY_LOADED:
		 /* Only registered apps can access encrypted files. */
		if (!pft_is_current_process_registered()) {
			pr_err("unregistered app (uid %u pid %u) is trying to access encrypted file %s\n",
			       current_uid(), current_pid(),
			       file_to_filename(filp));
			return -EACCES;
		}
		break;
	default:
		BUG(); /* State is set by "set state" command */
		break;
	}

	return 0;
}
EXPORT_SYMBOL(pft_file_permission);

/**
 * pft_sync_file() - sync the file.
 * @filp:	file pointer
 *
 * Complete writting any pending write request of encrypted data
 * before key is removed, to avoid writting garbage to
 * enterprise files.
 */
static void pft_sync_file(struct file *filp)
{
	int ret;

	ret = vfs_fsync(filp, false);

	if (ret)
		pr_debug("failed to sync file %s, ret = %d.\n",
			 file_to_filename(filp), ret);
	else
		pr_debug("Sync file %s ok.\n",  file_to_filename(filp));

}

/**
 * pft_file_close()- handle file close event
 * @filp:	file pointer
 *
 * This hook is called when file is closed by VFS.
 * This hook is called from the selinux driver.
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
int pft_file_close(struct file *filp)
{
	if (!filp)
		return 0;

	if (!pft_is_ready())
		return 0;

	/* do nothing for non-encrypted files */
	if (!pft_is_encrypted_file(filp->f_dentry))
		return 0;

	if (pft_is_inplace_file(filp)) {
		pr_debug("pid [%u] uid [%u] is closing in-place-encryption file %s\n",
			 current_pid(), current_uid(), file_to_filename(filp));
		pft_dev->inplace_file = NULL;
	}

	pft_sync_file(filp);
	pft_remove_file(filp);

	return 0;
}
EXPORT_SYMBOL(pft_file_close);

/**
 * pft_inode_unlink() - Delete file hook.
 * @dir:	directory inode pointer
 * @dentry:	file dentry pointer
 *
 * call path: vfs_unlink()->security_inode_unlink().
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
int pft_inode_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = NULL;

	if (!dir || !dentry || !dentry->d_inode)
		return 0;

	if (!pft_is_ready())
		return 0;

	inode = dentry->d_inode;

	/* do nothing for non-encrypted files */
	if (!pft_is_encrypted_file(dentry))
		return 0;

	if (pft_is_inplace_inode(inode)) {
		pr_err("block delete in-place-encryption file %s by uid [%d] pid [%d], while encryption in progress.\n",
		       inode_to_filename(inode), current_uid(), current_pid());
		return -EACCES;
	}

	if (!pft_is_current_process_registered()) {
		pr_err("unregistered app (uid %u pid %u) is trying to access encrypted file %s\n",
		       current_uid(), current_pid(), inode_to_filename(inode));
		return -EACCES;
	} else
		pr_debug("delete file %s\n", inode_to_filename(inode));

	return 0;
}
EXPORT_SYMBOL(pft_inode_unlink);

/**
 * pft_inode_set_xattr() - set/remove xattr callback.
 * @dentry:	file dentry pointer
 * @name:	xattr name.
 *
 * This hook checks attempt to set/remove PFE xattr.
 * Only this kernel driver allows to set the PFE xattr, so block
 * any attempt to do it from user space. Allow access for other
 * xattr.
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
int pft_inode_set_xattr(struct dentry *dentry, const char *name)
{
	struct inode *inode = NULL;

	if (!dentry || !dentry->d_inode)
		return 0;

	inode = dentry->d_inode;

	if (strcmp(name, XATTR_NAME_PFE) != 0) {
		pr_debug("xattr name=%s file %s\n", name,
		       inode_to_filename(inode));
		return 0; /* Not PFE xattr so it is ok */
	}

	pr_err("Attemp to set/remove PFE xattr for file %s\n",
	       inode_to_filename(inode));

	/* Only PFT kernel driver allows to set the PFE xattr */
	return -EACCES;
}
EXPORT_SYMBOL(pft_inode_set_xattr);

/**
 * pft_close_opened_enc_files() - Close all the currently open
 * encrypted files
 *
 * Close all open encrypted file when removing key or
 * deactivating.
 */
static void pft_close_opened_enc_files(void)
{
	struct pft_file_info *tmp = NULL;
	struct list_head *pos = NULL;

	mutex_lock(&pft_dev->lock);
	list_for_each(pos, &pft_dev->open_file_list) {
		struct file *filp;
		tmp = list_entry(pos, struct pft_file_info, list);
		filp = tmp->file;
		pr_debug("file %s\n is being closed",
			 file_to_filename(filp));
		pft_sync_file(filp);
		filp_close(filp, NULL);
		list_del(&tmp->list);
		kfree(tmp);
	}
	mutex_unlock(&pft_dev->lock);
}

/**
 * pft_set_state() - Handle "Set State" command
 * @command:	command buffer.
 * @size:	size of command buffer.
 *
 * The command execution status is reported by the response.
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
static int pft_set_state(struct pft_command *command, int size)
{
	u32 state = command->set_state.state;
	int expected_size = sizeof(command->opcode) +
		sizeof(command->set_state);

	if (size != expected_size) {
		pr_err("Invalid buffer size\n");
		ptf_set_response(PFT_CMD_RESP_INVALID_CMD_PARAMS);
		return -EINVAL;
	}

	if (state >= PFT_STATE_MAX_INDEX) {
		pr_err("Invalid state %d\n", command->set_state.state);
		ptf_set_response(PFT_CMD_RESP_INVALID_STATE);
		return 0;
	}

	pr_debug("Set State %d [%s].\n", state, pft_state_name[state]);

	switch (command->set_state.state) {
	case PFT_STATE_DEACTIVATING:
	case PFT_STATE_REMOVING_KEY:
		pft_close_opened_enc_files();
		/* Fall through */
	case PFT_STATE_DEACTIVATED:
	case PFT_STATE_KEY_LOADED:
	case PFT_STATE_KEY_REMOVED:
		pft_dev->state = command->set_state.state;
		ptf_set_response(PFT_CMD_RESP_SUCCESS);
		break;
	default:
		pr_err("Invalid state %d\n", command->set_state.state);
		ptf_set_response(PFT_CMD_RESP_INVALID_STATE);
		break;
	}

	return 0;
}

/**
 * pft_get_process_open_file() - get file pointer using file
 * descriptor index.
 * @index: file descriptor index.
 *
 * Return: file pointer on success, NULL on failure.
 */
static struct file *pft_get_process_open_file(int index)
{
	struct fdtable *files_table;

	files_table = files_fdtable(current->files);
	if (files_table == NULL)
		return NULL;

	if (index >= files_table->max_fds)
		return NULL;
	else
		return files_table->fd[index];
}

/**
 *  pft_set_inplace_file() - handle "inplace file encryption"
 *  command.
 * @command:	command buffer.
 * @size:	size of command buffer.
 *
 * The command execution status is reported by the response.
 *
 * Return: 0 if command is valid, negative value otherwise.
 */
static int pft_set_inplace_file(struct pft_command *command, int size)
{
	int expected_size;
	u32 fd;
	int rc;
	struct file *filp = NULL;
	struct inode *inode = NULL;
	int writecount;

	expected_size = sizeof(command->opcode) +
		sizeof(command->preform_in_place_file_enc.file_descriptor);

	if (size != expected_size) {
		pr_err("invalid command size %d expected %d.\n",
		       size, expected_size);
		ptf_set_response(PFT_CMD_RESP_INVALID_CMD_PARAMS);
		return -EINVAL;
	}

	if (pft_dev->state != (u32) PFT_STATE_KEY_LOADED) {
		pr_err("Key not loaded, state [%d], In-place-encryption is not allowed.\n",
		       pft_dev->state);
		ptf_set_response(PFT_CMD_RESP_GENERAL_ERROR);
		return 0;
	}

	/* allow only one in-place file encryption at a time */
	if (pft_dev->inplace_file != NULL) {
		pr_err("file %s in-place-encryption in progress.\n",
		       file_to_filename(pft_dev->inplace_file));
		/* @todo - use new error code */
		ptf_set_response(PFT_CMD_RESP_INPLACE_FILE_IS_OPEN);
		return 0;
	}

	fd = command->preform_in_place_file_enc.file_descriptor;
	filp = pft_get_process_open_file(fd);

	if (filp == NULL) {
		pr_err("failed to find file by fd %d.\n", fd);
		ptf_set_response(PFT_CMD_RESP_GENERAL_ERROR);
		return 0;
	}

	/* Verify the file is not already open by other than PFM */
	if (!filp->f_path.dentry || !filp->f_path.dentry->d_inode) {
		pr_err("failed to get inode of inplace-file.\n");
		ptf_set_response(PFT_CMD_RESP_GENERAL_ERROR);
		return 0;
	}

	inode = filp->f_path.dentry->d_inode;
	writecount = atomic_read(&inode->i_writecount);
	if (writecount > 1) {
		pr_err("file %s is opened %d times for write.\n",
		       file_to_filename(filp), writecount);
		ptf_set_response(PFT_CMD_RESP_GENERAL_ERROR);
		return 0;
	}

	/*
	 * Check if the file was already encryprted.
	 * In practice, it is unlikely to happen,
	 * because PFM is not an enterprise application
	 * it won't be able to open encrypted file.
	 */
	if (pft_is_encrypted_file(filp->f_dentry)) {
		pr_err("file %s is already encrypted.\n",
		       file_to_filename(filp));
		ptf_set_response(PFT_CMD_RESP_GENERAL_ERROR);
		return 0;
	}


	/* Update the current in-place-encryption file */
	pft_dev->inplace_file = filp;

	/*
	 * Now, any new access to this file is allowed only to PFM.
	 * Lets make sure that all pending writes are completed
	 * before encrypting the file.
	 */
	pft_sync_file(filp);

	rc = pft_tag_file(pft_dev->inplace_file->f_dentry,
			  pft_get_app_key_index(current_uid()));

	if (!rc) {
		pr_debug("tagged file %s to be encrypted.\n",
			 file_to_filename(pft_dev->inplace_file));
		ptf_set_response(PFT_CMD_RESP_SUCCESS);
	} else {
		pr_err("failed to tag file %s for encryption.\n",
			file_to_filename(pft_dev->inplace_file));
		ptf_set_response(PFT_CMD_RESP_GENERAL_ERROR);
	}

	return 0;
}

/**
 * pft_update_reg_apps() - Update the registered application
 * list.
 * @command:	command buffer.
 * @size:	size of command buffer.
 *
 * The command execution status is reported by the response.
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
static int pft_update_reg_apps(struct pft_command *command, int size)
{
	int i;
	int expected_size;
	void *buf;
	int buf_size;
	u32 items_count = command->update_app_list.items_count;

	if (items_count > PFT_MAX_APPS) {
		pr_err("Number of apps [%d] > max apps [%d]\n",
		       items_count , PFT_MAX_APPS);
		ptf_set_response(PFT_CMD_RESP_INVALID_CMD_PARAMS);
		return -EINVAL;
	}

	expected_size =
		sizeof(command->opcode) +
		sizeof(command->update_app_list.items_count) +
		(command->update_app_list.items_count * sizeof(u32));

	if (size != expected_size) {
		pr_err("invalid command size %d expected %d.\n",
		       size, expected_size);
		ptf_set_response(PFT_CMD_RESP_INVALID_CMD_PARAMS);
		return -EINVAL;
	}

	mutex_lock(&pft_dev->lock);

	/* Free old table */
	kfree(pft_dev->uid_table);
	pft_dev->uid_table = NULL;
	pft_dev->uid_count = 0;

	if (items_count == 0) {
		pr_info("empty app list - clear list.\n");
		mutex_unlock(&pft_dev->lock);
		return 0;
	}

	buf_size = command->update_app_list.items_count * sizeof(u32);
	buf = kzalloc(buf_size, GFP_KERNEL);

	if (!buf) {
		pr_err("malloc failure\n");
		ptf_set_response(PFT_CMD_RESP_GENERAL_ERROR);
		mutex_unlock(&pft_dev->lock);
		return 0;
	}

	pft_dev->uid_table = buf;
	pft_dev->uid_count = command->update_app_list.items_count;
	pr_debug("uid_count = %d\n", pft_dev->uid_count);
	for (i = 0; i < pft_dev->uid_count; i++)
		pft_dev->uid_table[i] = command->update_app_list.table[i];
	ptf_set_response(PFT_CMD_RESP_SUCCESS);
	mutex_unlock(&pft_dev->lock);

	return 0;
}

/**
 * pft_handle_command() - Handle user space app commands.
 * @buf:	command buffer.
 * @buf_size:	command buffer size.
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
static int pft_handle_command(void *buf, int buf_size)
{
	size_t ret = 0;
	struct pft_command *command = NULL;

	/* opcode field is the minimum length of command */
	if (buf_size < sizeof(command->opcode)) {
		pr_err("Invalid argument used buffer size\n");
		return -EINVAL;
	}

	command = (struct pft_command *)buf;

	pft_dev->response.command_id = command->opcode;

	switch (command->opcode) {
	case PFT_CMD_OPCODE_SET_STATE:
		ret = pft_set_state(command, buf_size);
		break;
	case PFT_CMD_OPCODE_UPDATE_REG_APP_UID:
		ret = pft_update_reg_apps(command, buf_size);
		break;
	case PFT_CMD_OPCODE_PERFORM_IN_PLACE_FILE_ENC:
		ret = pft_set_inplace_file(command, buf_size);
		break;
	default:
		pr_err("Invalid command_op_code %u\n", command->opcode);
		ptf_set_response(PFT_CMD_RESP_INVALID_COMMAND);
		return 0;
	}

	return ret;
}

static int pft_device_open(struct inode *inode, struct file *file)
{
	int ret;

	mutex_lock(&pft_dev->lock);
	if (pft_dev->open_count > 0) {
		pr_err("PFT device is already opened (%d)\n",
		       pft_dev->open_count);
		ret = -EBUSY;
	} else {
		pft_dev->open_count++;
		pft_dev->pfm_pid = current_pid();
		pr_debug("PFT device opened by %d (%d)\n",
			 pft_dev->pfm_pid, pft_dev->open_count);
		ret = 0;
	}
	mutex_unlock(&pft_dev->lock);

	pr_debug("device opened, count %d\n", pft_dev->open_count);

	return ret;
}

static int pft_device_release(struct inode *inode, struct file *file)
{
	mutex_lock(&pft_dev->lock);
	if (0 < pft_dev->open_count)
		pft_dev->open_count--;
	pft_dev->pfm_pid = UINT_MAX;
	mutex_unlock(&pft_dev->lock);

	pr_debug("device released, count %d\n", pft_dev->open_count);

	return 0;
}

/**
 * pft_device_write() - Get commands from user sapce.
 *
 * Return: number of bytes to write on success to get the
 * command buffer, negative value on failure.
 * The error code for handling the command should be retrive by
 * reading the response.
 * Note: any reurn value of 0..size-1 will cause retry by the
 * OS, so avoid it.
 */
static ssize_t pft_device_write(struct file *filp, const char __user *user_buff,
				size_t size, loff_t *f_pos)
{
	int ret;
	char *cmd_buf;

	if (size > PFT_MAX_COMMAND_SIZE || !user_buff || !f_pos) {
		pr_err("inavlid parameters.\n");
		return -EINVAL;
	}

	cmd_buf = kzalloc(size, GFP_KERNEL);
	if (cmd_buf == NULL) {
		pr_err("malloc failure for command buffer\n");
		return -ENOMEM;
	}

	ret = copy_from_user(cmd_buf, user_buff, size);
	if (ret) {
		pr_err("Unable to copy from user (err %d)\n", ret);
		kfree(cmd_buf);
		return -EFAULT;
	}

	ret = pft_handle_command(cmd_buf, size);
	if (ret) {
		kfree(cmd_buf);
		return -EFAULT;
	}

	kfree(cmd_buf);

	return size;
}

/**
 * pft_device_read() - return response of last command.
 *
 * Return: number of bytes to read on success, negative value on
 * failure.
 */
static ssize_t pft_device_read(struct file *filp, char __user *buffer,
			       size_t length, loff_t *f_pos)
{
	int ret = 0;

	if (!buffer || !f_pos || length < sizeof(pft_dev->response)) {
		pr_err("inavlid parameters.\n");
		return -EFAULT;
	}

	ret = copy_to_user(buffer, &(pft_dev->response),
			   sizeof(pft_dev->response));
	if (ret) {
		pr_err("Unable to copy to user, err = %d.\n", ret);
		return -EINVAL;
	}

	return sizeof(pft_dev->response);
}


static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = pft_device_read,
	.write = pft_device_write,
	.open = pft_device_open,
	.release = pft_device_release,
};

static void __exit pft_exit(void)
{
	if (pft_dev == NULL)
		return;

	unregister_chrdev(pft_dev->major, PFT_DEVICE_NAME);

	kfree(pft_dev->uid_table);
	kfree(pft_dev);
}

static int __init pft_init(void)
{
	struct pft_device *dev = NULL;

	dev = kzalloc(sizeof(struct pft_device), GFP_KERNEL);
	if (dev == NULL) {
		pr_err("No memory for device structr\n");
		return -ENOMEM;
	}

	dev->state = PFT_STATE_DEACTIVATED;
	INIT_LIST_HEAD(&dev->open_file_list);
	mutex_init(&dev->lock);

	dev->major = register_chrdev(PFT_REQUESTED_MAJOR, PFT_DEVICE_NAME,
				     &fops);
	if (IS_ERR_VALUE(dev->major)) {
		pr_err("Registering the character device with major %d failed with %d\n",
		       PFT_REQUESTED_MAJOR, dev->major);
		goto fail;
	}
	pft_dev = dev;

	pr_info("Drivr initialized successfully %s %s.n", __DATE__, __TIME__);

	return 0;

fail:
	pr_err("Failed to init driver.\n");
	kfree(dev);

	return -ENODEV;
}

module_init(pft_init);
module_exit(pft_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Per-File-Tagger driver");
