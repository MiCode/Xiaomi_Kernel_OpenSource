/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 */
/*
 * Copyright (c) 2013 Samsung Electronics Co. Ltd
 *   Authors: Daeho Jeong, Woojoong Lee, Seunghwan Hyun,
 *               Sunghwan Yun, Sungjong Seo
 *
 * This program has been developed as a stackable file system based on
 * the WrapFS which written by
 *
 * Copyright (c) 1998-2011 Erez Zadok
 * Copyright (c) 2009     Shrikar Archak
 * Copyright (c) 2003-2011 Stony Brook University
 * Copyright (c) 2003-2011 The Research Foundation of SUNY
 *
 * This file is dual licensed.  It may be redistributed and/or modified
 * under the terms of the Apache 2.0 License OR version 2 of the GNU
 * General Public License.
 */

#define DEBUG 1
#include "sdcardfs.h"
#include <linux/lockdep.h>
#include <linux/dcache.h>
#include <linux/workqueue.h>
#include <linux/wait.h>

struct workqueue_struct *sdcardfs_asyn_wq = NULL;
struct workqueue_struct *sdcardfs_sync_wq = NULL;
struct kmem_cache *sdcardfs_work_cachep = NULL;
DECLARE_WAIT_QUEUE_HEAD(sdcardfs_work_waitq);

struct sdcardfs_work_job {
	int operation;
	struct dentry *entry;
	struct inode *dir;
	umode_t mode;
	bool want_excl;
	int result;
	struct task_struct *owner;
	struct work_struct work;
};

static void permwork_init_once(void *obj);
static void get_derive_permissions_recursive_internal(struct dentry *parent);

int sdcardfs_init_workqueue(void)
{
	int err = 0;

	sdcardfs_asyn_wq = create_singlethread_workqueue("sdcardfs_asyn_wq");
	sdcardfs_sync_wq = create_singlethread_workqueue("sdcardfs_wq");

	if (!sdcardfs_asyn_wq || !sdcardfs_sync_wq) {
		err = -ENOMEM;
		goto out;
	}

	sdcardfs_work_cachep =
		kmem_cache_create("sdcardfs_work_cache",
				  sizeof(struct sdcardfs_work_job), 0,
				  SLAB_RECLAIM_ACCOUNT, permwork_init_once);

	if (!sdcardfs_work_cachep)
		err = -ENOMEM;

out:
	return err;
}

void sdcardfs_destroy_workqueue(void)
{
	if (sdcardfs_sync_wq) {
		flush_workqueue(sdcardfs_sync_wq);
		destroy_workqueue(sdcardfs_sync_wq);
		sdcardfs_sync_wq = NULL;
	}

	if (sdcardfs_asyn_wq) {
		flush_workqueue(sdcardfs_asyn_wq);
		destroy_workqueue(sdcardfs_asyn_wq);
		sdcardfs_asyn_wq = NULL;
	}

	if (sdcardfs_work_cachep) {
		kmem_cache_destroy(sdcardfs_work_cachep);
		sdcardfs_work_cachep = NULL;
	}
}

static inline void sdcardfs_lock_dinode(struct dentry *entry)
{
	mutex_lock(&entry->d_inode->i_mutex);
}

static inline void sdcardfs_unlock_dinode(struct dentry *entry)
{
	mutex_unlock(&entry->d_inode->i_mutex);
}

static int touch(char *abs_path, mode_t mode)
{
	struct file *filp = filp_open(abs_path, O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW, mode);

	if (IS_ERR(filp)) {
		if (PTR_ERR(filp) == -EEXIST)
			return 0;

		pr_err("sdcardfs: failed to open(%s): %ld\n",
					abs_path, PTR_ERR(filp));
		return PTR_ERR(filp);
	}
	filp_close(filp, current->files);
	return 0;
}

static void sdcardfs_work_handle_create(struct sdcardfs_work_job *pw)
{
	int err;
	struct dentry *dentry = pw->entry;
	struct inode *dir = pw->dir;
	umode_t mode = pw->mode;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct path lower_path;
	const struct cred *saved_cred = NULL;

	if (!dentry->d_fsdata) {
		pr_err("sdcardfs: %s: dentry %p d_fsdata absent\n", __func__, dentry);
		err = -ENOENT;
		goto out_cred;
	}

	/* save current_cred and override it */
	saved_cred = override_fsids(SDCARDFS_SB(dir->i_sb));
	if (!saved_cred) {
		err = -ENOMEM;
		goto out_cred;
	}

	sdcardfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	/* set last 16bytes of mode field to 0664 */
	mode = (mode & S_IFMT) | 00664;
	err = vfs_create(lower_parent_dentry->d_inode, lower_dentry, mode, pw->want_excl);
	if (err)
		goto out;

	err = sdcardfs_interpose(dentry, dir->i_sb, &lower_path, SDCARDFS_I(dir)->userid);
	if (err)
		goto out;
	fsstack_copy_attr_times(dir, sdcardfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);

out:
	unlock_dir(lower_parent_dentry);
	sdcardfs_put_lower_path(dentry, &lower_path);
	REVERT_CRED(saved_cred);
out_cred:
	pw->result = err;
	pw->operation = SDCARDFS_WQOP_DONE;
	wake_up(&sdcardfs_work_waitq);
}

static void sdcardfs_work_handle_mkdir(struct sdcardfs_work_job *pw)
{
	int err;
	struct dentry *dentry = pw->entry;
	struct inode *dir = pw->dir;
	umode_t mode = pw->mode;
	int make_nomedia_in_obb = 0;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct path lower_path;
	struct sdcardfs_sb_info *sbi = SDCARDFS_SB(dentry->d_sb);
	const struct cred *saved_cred = NULL;
	struct sdcardfs_inode_info *pi = SDCARDFS_I(dir);
	char *page_buf;
	char *nomedia_dir_name;
	char *nomedia_fullpath;
	int fullpath_namelen;
	int touch_err = 0;

	/* save current_cred and override it */
	saved_cred = override_fsids(SDCARDFS_SB(dir->i_sb));
	if (!saved_cred) {
		err = -ENOMEM;
		goto out_cred;
	}

	/* check disk space */
	if (!check_min_free_space(dentry, 0, 1)) {
		pr_info("sdcardfs: No minimum free space.\n");
		err = -ENOSPC;
		goto out_revert;
	}

	/* the lower_dentry is negative here */
	sdcardfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	/* set last 16bytes of mode field to 0775 */
	mode = (mode & S_IFMT) | 00775;
	err = vfs_mkdir(lower_parent_dentry->d_inode, lower_dentry, mode);

	if (err)
		goto out;

	/* if it is a local obb dentry, setup it with the base obbpath */
	if (need_graft_path(dentry)) {
		unlock_dir(lower_parent_dentry);
		err = setup_obb_dentry(dentry, &lower_path);
		lower_parent_dentry = lock_parent(lower_dentry);
		if (err) {
			/* if the sbi->obbpath is not available, the lower_path won't be
			 * changed by setup_obb_dentry() but the lower path is saved to
			 * its orig_path. this dentry will be revalidated later.
			 * but now, the lower_path should be NULL
			 */
			sdcardfs_put_reset_lower_path(dentry);

			/* the newly created lower path which saved to its orig_path or
			 * the lower_path is the base obbpath.
			 * therefore, an additional path_get is required
			 */
			path_get(&lower_path);
		} else
			make_nomedia_in_obb = 1;
	}

	err = sdcardfs_interpose(dentry, dir->i_sb, &lower_path, pi->userid);
	if (err)
		goto out;

	fsstack_copy_attr_times(dir, sdcardfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);
	/* update number of links on parent directory */
	set_nlink(dir, sdcardfs_lower_inode(dir)->i_nlink);

	if ((!sbi->options.multiuser) && (!strcasecmp(dentry->d_name.name, "obb"))
		&& (pi->perm == PERM_ANDROID) && (pi->userid == 0))
		make_nomedia_in_obb = 1;

	/* When creating /Android/data and /Android/obb, mark them as .nomedia */
	if (make_nomedia_in_obb ||
		((pi->perm == PERM_ANDROID) && (!strcasecmp(dentry->d_name.name, "data")))) {

		page_buf = (char *)__get_free_page(GFP_KERNEL);
		if (!page_buf) {
			pr_err("sdcardfs: failed to allocate page buf\n");
			goto out;
		}

		nomedia_dir_name = d_absolute_path(&lower_path, page_buf, PAGE_SIZE);
		if (IS_ERR(nomedia_dir_name)) {
			free_page((unsigned long)page_buf);
			pr_err("sdcardfs: failed to get .nomedia dir name\n");
			goto out;
		}

		fullpath_namelen = page_buf + PAGE_SIZE - nomedia_dir_name - 1;
		fullpath_namelen += strlen("/.nomedia");
		nomedia_fullpath = kzalloc(fullpath_namelen + 1, GFP_KERNEL);
		if (!nomedia_fullpath) {
			free_page((unsigned long)page_buf);
			pr_err("sdcardfs: failed to allocate .nomedia fullpath buf\n");
			goto out;
		}

		strncpy(nomedia_fullpath, nomedia_dir_name, fullpath_namelen + 1);
		free_page((unsigned long)page_buf);
		strcat(nomedia_fullpath, "/.nomedia");
		unlock_dir(lower_parent_dentry);
		touch_err = touch(nomedia_fullpath, 0664);
		if (touch_err) {
			pr_err("sdcardfs: failed to touch(%s): %d\n",
							nomedia_fullpath, touch_err);
			kfree(nomedia_fullpath);
			goto out_touch;
		}
		kfree(nomedia_fullpath);
		goto out_touch;
	}
out:
	unlock_dir(lower_parent_dentry);
out_touch:
	sdcardfs_put_lower_path(dentry, &lower_path);
out_revert:
	REVERT_CRED(saved_cred);
out_cred:
	pw->result = err;
	pw->operation = SDCARDFS_WQOP_DONE;
	wake_up(&sdcardfs_work_waitq);
}

static void sdcardfs_work_handle_unlink(struct sdcardfs_work_job *pw)
{
	int err;
	struct dentry *dentry = pw->entry;
	struct inode *dir = pw->dir;
	struct dentry *lower_dentry;
	struct inode *lower_dir_inode = sdcardfs_lower_inode(dir);
	struct dentry *lower_dir_dentry;
	struct path lower_path;
	const struct cred *saved_cred = NULL;

	/* save current_cred and override it */
	saved_cred = override_fsids(SDCARDFS_SB(dir->i_sb));
	if (!saved_cred) {
		err = -ENOMEM;
		goto out_cred;
	}

	sdcardfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	dget(lower_dentry);
	lower_dir_dentry = lock_parent(lower_dentry);

	err = vfs_unlink(lower_dir_inode, lower_dentry, NULL);

	/*
	 * Note: unlinking on top of NFS can cause silly-renamed files.
	 * Trying to delete such files results in EBUSY from NFS
	 * below.  Silly-renamed files will get deleted by NFS later on, so
	 * we just need to detect them here and treat such EBUSY errors as
	 * if the upper file was successfully deleted.
	 */
	if (err == -EBUSY && lower_dentry->d_flags & DCACHE_NFSFS_RENAMED)
		err = 0;
	if (err)
		goto out;
	fsstack_copy_attr_times(dir, lower_dir_inode);
	fsstack_copy_inode_size(dir, lower_dir_inode);
	set_nlink(dentry->d_inode,
		  sdcardfs_lower_inode(dentry->d_inode)->i_nlink);
	dentry->d_inode->i_ctime = dir->i_ctime;
	d_drop(dentry); /* this is needed, else LTP fails (VFS won't do it) */
out:
	unlock_dir(lower_dir_dentry);
	dput(lower_dentry);
	sdcardfs_put_lower_path(dentry, &lower_path);
	REVERT_CRED(saved_cred);
out_cred:
	pw->result = err;
	pw->operation = SDCARDFS_WQOP_DONE;
	wake_up(&sdcardfs_work_waitq);
}

static void get_derive_permissions_recursive_internal(struct dentry *parent)
{
	struct dentry *dentry;

	list_for_each_entry(dentry, &parent->d_subdirs, d_child) {
		if (dentry && dentry->d_inode) {
			if (dentry == parent)
				continue;
			sdcardfs_lock_dinode(dentry);
			get_derived_permission(parent, dentry);
			fix_derived_permission(dentry->d_inode);
			if (S_ISDIR(dentry->d_inode->i_mode))
				get_derive_permissions_recursive_internal(dentry);
			sdcardfs_unlock_dinode(dentry);
		}
	}
}

static void sdcardfs_work_handle_permissions(struct sdcardfs_work_job *pw)
{
	struct dentry *entry = pw->entry;

	if (entry && entry->d_inode) {
		lockdep_off();
		sdcardfs_lock_dinode(entry);
		get_derive_permissions_recursive_internal(entry);
		sdcardfs_unlock_dinode(entry);
		lockdep_on();
	}
	dput(entry);
	kmem_cache_free(sdcardfs_work_cachep, pw);
}

static void sdcardfs_work_handle(struct work_struct *w)
{
	struct sdcardfs_work_job *pw;

	pw = container_of(w, struct sdcardfs_work_job, work);

	if (pw->operation == SDCARDFS_WQOP_CREATE)
		sdcardfs_work_handle_create(pw);
	else if (pw->operation == SDCARDFS_WQOP_MKDIR)
		sdcardfs_work_handle_mkdir(pw);
	else if (pw->operation == SDCARDFS_WQOP_UNLINK)
		sdcardfs_work_handle_unlink(pw);
	else if (pw->operation == SDCARDFS_WQOP_RECURSIVE_PERM)
		sdcardfs_work_handle_permissions(pw);
}

/* sdcardfs permwork constructor */
static void permwork_init_once(void *obj)
{
	struct sdcardfs_work_job *pw = obj;

	INIT_WORK(&pw->work, sdcardfs_work_handle);
}

/* dispatch asynchronous job to update permissions recursively */
int sdcardfs_work_dispatch_permissions(struct dentry *entry)
{
	struct sdcardfs_work_job *pw;
	bool ret;

	if (!entry)
		return 0;

	pw = kmem_cache_alloc(sdcardfs_work_cachep, GFP_KERNEL);

	if (!pw)
		return -ENOMEM;

	dget(entry);
	pw->operation = SDCARDFS_WQOP_RECURSIVE_PERM;
	pw->entry = entry;
	pw->owner = current;

	ret = queue_work(sdcardfs_asyn_wq, &pw->work);

	if (!ret)
		pr_err("sdcardfs: %s: failed %d\n", __func__, ret);

	return 0;
}

/* dispatch synchronous job for create, unlink, and mkdir */
int sdcardfs_work_dispatch_syncjob(int operation,
	struct inode *dir, struct dentry *dentry, umode_t mode, bool want_excl)
{
	int ret;
	struct sdcardfs_work_job *pw;

	pw = kmem_cache_alloc(sdcardfs_work_cachep, GFP_KERNEL);
	if (!pw)
		return -ENOMEM;

	dget(dentry);
	pw->operation = operation;
	pw->owner = current;
	pw->dir = dir;
	pw->entry = dentry;
	pw->mode = mode;
	pw->want_excl = want_excl;
	ret = queue_work(sdcardfs_sync_wq, &pw->work);

	if (!ret) {
		pr_err("sdcardfs: %s: failed %d\n", __func__, ret);
		goto out;
	}

	if (wait_event_interruptible(sdcardfs_work_waitq, pw->operation == SDCARDFS_WQOP_DONE)) {
		pr_warn("sdcardfs: %s: cancel work %d\n", __func__, operation);
		cancel_work_sync(&pw->work);
		ret = -EINTR;
	}
	else
		ret = pw->result;
out:
	dput(dentry);
	kmem_cache_free(sdcardfs_work_cachep, pw);
	return ret;
}

int sdcardfs_work_dispatch_create(struct inode *dir, struct dentry *dentry,
			 umode_t mode, bool want_excl)
{
	return sdcardfs_work_dispatch_syncjob(SDCARDFS_WQOP_CREATE, dir, dentry, mode, want_excl);
}

int sdcardfs_work_dispatch_unlink(struct inode *dir, struct dentry *dentry)
{
	return sdcardfs_work_dispatch_syncjob(SDCARDFS_WQOP_UNLINK, dir, dentry, 0, 0);
}

int sdcardfs_work_dispatch_mkdir(struct inode *dir, struct dentry *dentry,
	umode_t mode)
{
	return sdcardfs_work_dispatch_syncjob(SDCARDFS_WQOP_MKDIR, dir, dentry, mode, 0);
}

