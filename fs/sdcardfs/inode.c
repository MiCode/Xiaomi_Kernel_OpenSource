/*
 * fs/sdcardfs/inode.c
 *
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

#include "sdcardfs.h"
#include <linux/lockdep.h>

/* Do not directly use this function. Use OVERRIDE_CRED() instead. */
const struct cred * override_fsids(struct sdcardfs_sb_info* sbi)
{
	struct cred * cred;
	const struct cred * old_cred;

	cred = prepare_creds();
	if (!cred)
		return NULL;

	cred->fsuid = make_kuid(&init_user_ns, sbi->options.fs_low_uid);
	cred->fsgid = make_kgid(&init_user_ns, sbi->options.fs_low_gid);

	old_cred = override_creds(cred);

	return old_cred;
}

/* Do not directly use this function, use REVERT_CRED() instead. */
void revert_fsids(const struct cred * old_cred)
{
	const struct cred * cur_cred;

	cur_cred = current->cred;
	revert_creds(old_cred);
	put_cred(cur_cred);
}

static int sdcardfs_create(struct inode *dir, struct dentry *dentry,
			 umode_t mode, bool want_excl)
{
	if(!check_caller_access_to_name(dir, dentry->d_name.name)) {
		printk(KERN_INFO "%s: need to check the caller's gid in packages.list\n"
						 "  dentry: %s, task:%s\n",
						 __func__, dentry->d_name.name, current->comm);
		return -EACCES;
	}
	return sdcardfs_work_dispatch_create(dir, dentry, mode, want_excl);
}

#if 0
static int sdcardfs_link(struct dentry *old_dentry, struct inode *dir,
		       struct dentry *new_dentry)
{
	struct dentry *lower_old_dentry;
	struct dentry *lower_new_dentry;
	struct dentry *lower_dir_dentry;
	u64 file_size_save;
	int err;
	struct path lower_old_path, lower_new_path;

	OVERRIDE_CRED(SDCARDFS_SB(dir->i_sb));

	file_size_save = i_size_read(old_dentry->d_inode);
	sdcardfs_get_lower_path(old_dentry, &lower_old_path);
	sdcardfs_get_lower_path(new_dentry, &lower_new_path);
	lower_old_dentry = lower_old_path.dentry;
	lower_new_dentry = lower_new_path.dentry;
	lower_dir_dentry = lock_parent(lower_new_dentry);

	err = vfs_link(lower_old_dentry, lower_dir_dentry->d_inode,
		       lower_new_dentry, NULL);
	if (err || !lower_new_dentry->d_inode)
		goto out;

	err = sdcardfs_interpose(new_dentry, dir->i_sb, &lower_new_path);
	if (err)
		goto out;
	fsstack_copy_attr_times(dir, lower_new_dentry->d_inode);
	fsstack_copy_inode_size(dir, lower_new_dentry->d_inode);
	set_nlink(old_dentry->d_inode,
		  sdcardfs_lower_inode(old_dentry->d_inode)->i_nlink);
	i_size_write(new_dentry->d_inode, file_size_save);
out:
	unlock_dir(lower_dir_dentry);
	sdcardfs_put_lower_path(old_dentry, &lower_old_path);
	sdcardfs_put_lower_path(new_dentry, &lower_new_path);
	REVERT_CRED();
	return err;
}
#endif

static int sdcardfs_unlink(struct inode *dir, struct dentry *dentry)
{
	if(!check_caller_access_to_name(dir, dentry->d_name.name)) {
		printk(KERN_INFO "%s: need to check the caller's gid in packages.list\n"
						 "  dentry: %s, task:%s\n",
						 __func__, dentry->d_name.name, current->comm);
		return -EACCES;
	}

	return sdcardfs_work_dispatch_unlink(dir, dentry);
}

#if 0
static int sdcardfs_symlink(struct inode *dir, struct dentry *dentry,
			  const char *symname)
{
	int err;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct path lower_path;

	OVERRIDE_CRED(SDCARDFS_SB(dir->i_sb));

	sdcardfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	err = vfs_symlink(lower_parent_dentry->d_inode, lower_dentry, symname);
	if (err)
		goto out;
	err = sdcardfs_interpose(dentry, dir->i_sb, &lower_path);
	if (err)
		goto out;
	fsstack_copy_attr_times(dir, sdcardfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);

out:
	unlock_dir(lower_parent_dentry);
	sdcardfs_put_lower_path(dentry, &lower_path);
	REVERT_CRED();
	return err;
}
#endif

static int sdcardfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	if(!check_caller_access_to_name(dir, dentry->d_name.name)) {
		printk(KERN_INFO "%s: need to check the caller's gid in packages.list\n"
						 "  dentry: %s, task:%s\n",
						 __func__, dentry->d_name.name, current->comm);
		return -EACCES;
	}
	return sdcardfs_work_dispatch_mkdir(dir, dentry, mode);
}

static int sdcardfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct dentry *lower_dentry;
	struct dentry *lower_dir_dentry;
	int err;
	struct path lower_path;
	const struct cred *saved_cred = NULL;

	if(!check_caller_access_to_name(dir, dentry->d_name.name)) {
		printk(KERN_INFO "%s: need to check the caller's gid in packages.list\n"
						 "  dentry: %s, task:%s\n",
						 __func__, dentry->d_name.name, current->comm);
		err = -EACCES;
		goto out_eacces;
	}

	/* save current_cred and override it */
	OVERRIDE_CRED(SDCARDFS_SB(dir->i_sb), saved_cred);

	/* sdcardfs_get_real_lower(): in case of remove an user's obb dentry
	 * the dentry on the original path should be deleted. */
	sdcardfs_get_real_lower(dentry, &lower_path);

	lower_dentry = lower_path.dentry;
	lower_dir_dentry = lock_parent(lower_dentry);

	err = vfs_rmdir(lower_dir_dentry->d_inode, lower_dentry);
	if (err)
		goto out;

	d_drop(dentry);	/* drop our dentry on success (why not VFS's job?) */
	if (dentry->d_inode)
		clear_nlink(dentry->d_inode);
	fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
	fsstack_copy_inode_size(dir, lower_dir_dentry->d_inode);
	set_nlink(dir, lower_dir_dentry->d_inode->i_nlink);

out:
	unlock_dir(lower_dir_dentry);
	sdcardfs_put_real_lower(dentry, &lower_path);
	REVERT_CRED(saved_cred);
out_eacces:
	return err;
}

#if 0
static int sdcardfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode,
			dev_t dev)
{
	int err;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct path lower_path;

	OVERRIDE_CRED(SDCARDFS_SB(dir->i_sb));

	sdcardfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	err = vfs_mknod(lower_parent_dentry->d_inode, lower_dentry, mode, dev);
	if (err)
		goto out;

	err = sdcardfs_interpose(dentry, dir->i_sb, &lower_path);
	if (err)
		goto out;
	fsstack_copy_attr_times(dir, sdcardfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);

out:
	unlock_dir(lower_parent_dentry);
	sdcardfs_put_lower_path(dentry, &lower_path);
	REVERT_CRED();
	return err;
}
#endif

/*
 * The locking rules in sdcardfs_rename are complex.  We could use a simpler
 * superblock-level name-space lock for renames and copy-ups.
 */
static int sdcardfs_rename(struct inode *old_dir, struct dentry *old_dentry,
			 struct inode *new_dir, struct dentry *new_dentry)
{
	int err = 0;
	struct dentry *lower_old_dentry = NULL;
	struct dentry *lower_new_dentry = NULL;
	struct dentry *lower_old_dir_dentry = NULL;
	struct dentry *lower_new_dir_dentry = NULL;
	struct dentry *trap = NULL;
	struct dentry *new_parent = NULL;
	struct path lower_old_path, lower_new_path;
	const struct cred *saved_cred = NULL;

	if(!check_caller_access_to_name(old_dir, old_dentry->d_name.name) ||
		!check_caller_access_to_name(new_dir, new_dentry->d_name.name)) {
		printk(KERN_INFO "%s: need to check the caller's gid in packages.list\n"
						 "  new_dentry: %s, task:%s\n",
						 __func__, new_dentry->d_name.name, current->comm);
		err = -EACCES;
		goto out_eacces;
	}

	/* save current_cred and override it */
	OVERRIDE_CRED(SDCARDFS_SB(old_dir->i_sb), saved_cred);

	sdcardfs_get_real_lower(old_dentry, &lower_old_path);
	sdcardfs_get_lower_path(new_dentry, &lower_new_path);
	lower_old_dentry = lower_old_path.dentry;
	lower_new_dentry = lower_new_path.dentry;
	lower_old_dir_dentry = dget_parent(lower_old_dentry);
	lower_new_dir_dentry = dget_parent(lower_new_dentry);

	trap = lock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
	/* source should not be ancestor of target */
	if (trap == lower_old_dentry) {
		err = -EINVAL;
		goto out;
	}
	/* target should not be ancestor of source */
	if (trap == lower_new_dentry) {
		err = -ENOTEMPTY;
		goto out;
	}

	err = vfs_rename(lower_old_dir_dentry->d_inode, lower_old_dentry,
			 lower_new_dir_dentry->d_inode, lower_new_dentry,
			 NULL, 0);
	if (err)
		goto out;

	/* Copy attrs from lower dir, but i_uid/i_gid */
	sdcardfs_copy_and_fix_attrs(new_dir, lower_new_dir_dentry->d_inode);
	fsstack_copy_inode_size(new_dir, lower_new_dir_dentry->d_inode);

	if (new_dir != old_dir) {
		sdcardfs_copy_and_fix_attrs(old_dir, lower_old_dir_dentry->d_inode);
		fsstack_copy_inode_size(old_dir, lower_old_dir_dentry->d_inode);
		unlock_rename(lower_old_dir_dentry, lower_new_dir_dentry);

		/* update the derived permission of the old_dentry
		 * with its new parent
		 */
		new_parent = dget_parent(new_dentry);
		if(new_parent) {
			if(old_dentry->d_inode) {
				update_derived_permission(old_dentry);
			}
			dput(new_parent);
		}
	} else {
		unlock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
	}

	/* At this point, not all dentry information has been moved, so
	 * we pass along new_dentry for the name.*/
	get_derived_permission_new(new_dentry->d_parent, old_dentry, new_dentry);
	fix_derived_permission(old_dentry->d_inode);
	get_derive_permissions_recursive(old_dentry);
	goto out_success;

out:
	unlock_rename(lower_old_dir_dentry, lower_new_dir_dentry);

out_success:
	dput(lower_old_dir_dentry);
	dput(lower_new_dir_dentry);
	sdcardfs_put_real_lower(old_dentry, &lower_old_path);
	sdcardfs_put_lower_path(new_dentry, &lower_new_path);
	REVERT_CRED(saved_cred);

out_eacces:
	return err;
}

#if 0
static int sdcardfs_readlink(struct dentry *dentry, char __user *buf, int bufsiz)
{
	int err;
	struct dentry *lower_dentry;
	struct path lower_path;
	/* XXX readlink does not requires overriding credential */

	sdcardfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	if (!lower_dentry->d_inode->i_op ||
	    !lower_dentry->d_inode->i_op->readlink) {
		err = -EINVAL;
		goto out;
	}

	err = lower_dentry->d_inode->i_op->readlink(lower_dentry,
						    buf, bufsiz);
	if (err < 0)
		goto out;
	fsstack_copy_attr_atime(dentry->d_inode, lower_dentry->d_inode);

out:
	sdcardfs_put_lower_path(dentry, &lower_path);
	return err;
}
#endif

#if 0
static void *sdcardfs_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	char *buf;
	int len = PAGE_SIZE, err;
	mm_segment_t old_fs;

	/* This is freed by the put_link method assuming a successful call. */
	buf = kmalloc(len, GFP_KERNEL);
	if (!buf) {
		buf = ERR_PTR(-ENOMEM);
		goto out;
	}

	/* read the symlink, and then we will follow it */
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	err = sdcardfs_readlink(dentry, buf, len);
	set_fs(old_fs);
	if (err < 0) {
		kfree(buf);
		buf = ERR_PTR(err);
	} else {
		buf[err] = '\0';
	}
out:
	nd_set_link(nd, buf);
	return NULL;
}
#endif

static int sdcardfs_permission(struct inode *inode, int mask)
{
	int err;

	/*
	 * Permission check on sdcardfs inode.
	 * Calling process should have AID_SDCARD_RW permission
	 */
	err = generic_permission(inode, mask);

	/* XXX
	 * Original sdcardfs code calls inode_permission(lower_inode,.. )
	 * for checking inode permission. But doing such things here seems
	 * duplicated work, because the functions called after this func,
	 * such as vfs_create, vfs_unlink, vfs_rename, and etc,
	 * does exactly same thing, i.e., they calls inode_permission().
	 * So we just let they do the things.
	 * If there are any security hole, just uncomment following if block.
	 */
#if 0
	if (!err) {
		/*
		 * Permission check on lower_inode(=EXT4).
		 * we check it with AID_MEDIA_RW permission
		 */
		struct inode *lower_inode;
		OVERRIDE_CRED(SDCARDFS_SB(inode->sb));

		lower_inode = sdcardfs_lower_inode(inode);
		err = inode_permission(lower_inode, mask);

		REVERT_CRED();
	}
#endif
	return err;

}

static int sdcardfs_setattr(struct dentry *dentry, struct iattr *ia)
{
	int err;
	struct dentry *lower_dentry;
	struct inode *inode;
	struct inode *lower_inode;
	struct path lower_path;
	struct iattr lower_ia;
	struct dentry *parent;

	inode = dentry->d_inode;

	/*
	 * Check if user has permission to change inode.  We don't check if
	 * this user can change the lower inode: that should happen when
	 * calling notify_change on the lower inode.
	 */
	err = inode_change_ok(inode, ia);

	/* no vfs_XXX operations required, cred overriding will be skipped. wj*/
	if (!err) {
		/* check the Android group ID */
		parent = dget_parent(dentry);
		if(!check_caller_access_to_name(parent->d_inode, dentry->d_name.name)) {
			printk(KERN_INFO "%s: need to check the caller's gid in packages.list\n"
							 "  dentry: %s, task:%s\n",
							 __func__, dentry->d_name.name, current->comm);
			err = -EACCES;
		}
		dput(parent);
	}

	if (err)
		goto out_err;

	sdcardfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_inode = sdcardfs_lower_inode(inode);

	/* prepare our own lower struct iattr (with the lower file) */
	memcpy(&lower_ia, ia, sizeof(lower_ia));
	if (ia->ia_valid & ATTR_FILE)
		lower_ia.ia_file = sdcardfs_lower_file(ia->ia_file);

	lower_ia.ia_valid &= ~(ATTR_UID | ATTR_GID | ATTR_MODE);

	/*
	 * If shrinking, first truncate upper level to cancel writing dirty
	 * pages beyond the new eof; and also if its' maxbytes is more
	 * limiting (fail with -EFBIG before making any change to the lower
	 * level).  There is no need to vmtruncate the upper level
	 * afterwards in the other cases: we fsstack_copy_inode_size from
	 * the lower level.
	 */
	lockdep_off();
	if (current->mm)
		down_write(&current->mm->mmap_sem);
	if (ia->ia_valid & ATTR_SIZE) {
		err = inode_newsize_ok(inode, ia->ia_size);
		if (err) {
			if (current->mm)
				up_write(&current->mm->mmap_sem);
			lockdep_on();
			goto out;
		}
		truncate_setsize(inode, ia->ia_size);
	}

	/*
	 * mode change is for clearing setuid/setgid bits. Allow lower fs
	 * to interpret this in its own way.
	 */
	if (lower_ia.ia_valid & (ATTR_KILL_SUID | ATTR_KILL_SGID))
		lower_ia.ia_valid &= ~ATTR_MODE;

	/* notify the (possibly copied-up) lower inode */
	/*
	 * Note: we use lower_dentry->d_inode, because lower_inode may be
	 * unlinked (no inode->i_sb and i_ino==0.  This happens if someone
	 * tries to open(), unlink(), then ftruncate() a file.
	 */
	mutex_lock(&lower_dentry->d_inode->i_mutex);
	err = notify_change(lower_dentry, &lower_ia, /* note: lower_ia */
			NULL);
	mutex_unlock(&lower_dentry->d_inode->i_mutex);
	if (current->mm)
		up_write(&current->mm->mmap_sem);
	lockdep_on();
	if (err)
		goto out;

	/* get attributes from the lower inode and update derived permissions */
	sdcardfs_copy_and_fix_attrs(inode, lower_inode);

	/*
	 * Not running fsstack_copy_inode_size(inode, lower_inode), because
	 * VFS should update our inode size, and notify_change on
	 * lower_inode should update its size.
	 */

out:
	sdcardfs_put_lower_path(dentry, &lower_path);
out_err:
	return err;
}

static int sdcardfs_getattr(struct vfsmount *mnt, struct dentry *dentry,
		 struct kstat *stat)
{
	struct dentry *lower_dentry;
	struct inode *inode;
	struct inode *lower_inode;
	struct path lower_path;
	struct dentry *parent;

	parent = dget_parent(dentry);
	if(!check_caller_access_to_name(parent->d_inode, dentry->d_name.name)) {
		printk(KERN_INFO "%s: need to check the caller's gid in packages.list\n"
						 "  dentry: %s, task:%s\n",
						 __func__, dentry->d_name.name, current->comm);
		dput(parent);
		return -EACCES;
	}
	dput(parent);

	inode = dentry->d_inode;

	sdcardfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_inode = sdcardfs_lower_inode(inode);


	sdcardfs_copy_and_fix_attrs(inode, lower_inode);
	fsstack_copy_inode_size(inode, lower_inode);


	generic_fillattr(inode, stat);
	sdcardfs_put_lower_path(dentry, &lower_path);
	return 0;
}

const struct inode_operations sdcardfs_symlink_iops = {
	.permission	= sdcardfs_permission,
	.setattr	= sdcardfs_setattr,
	/* XXX Following operations are implemented,
	 *     but FUSE(sdcard) or FAT does not support them
	 *     These methods are *NOT* perfectly tested.
	.readlink	= sdcardfs_readlink,
	.follow_link	= sdcardfs_follow_link,
	.put_link	= kfree_put_link,
	 */
};

const struct inode_operations sdcardfs_dir_iops = {
	.create		= sdcardfs_create,
	.lookup		= sdcardfs_lookup,
#if 0
	.permission	= sdcardfs_permission,
#endif
	.unlink		= sdcardfs_unlink,
	.mkdir		= sdcardfs_mkdir,
	.rmdir		= sdcardfs_rmdir,
	.rename		= sdcardfs_rename,
	.setattr	= sdcardfs_setattr,
	.getattr	= sdcardfs_getattr,
	/* XXX Following operations are implemented,
	 *     but FUSE(sdcard) or FAT does not support them
	 *     These methods are *NOT* perfectly tested.
	.symlink	= sdcardfs_symlink,
	.link		= sdcardfs_link,
	.mknod		= sdcardfs_mknod,
	 */
};

const struct inode_operations sdcardfs_main_iops = {
	.create		= sdcardfs_create,
	.unlink		= sdcardfs_unlink,
	.permission	= sdcardfs_permission,
	.setattr	= sdcardfs_setattr,
	.getattr	= sdcardfs_getattr,
};
