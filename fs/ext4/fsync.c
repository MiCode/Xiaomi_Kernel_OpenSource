// SPDX-License-Identifier: GPL-2.0
/*
 *  linux/fs/ext4/fsync.c
 *
 *  Copyright (C) 1993  Stephen Tweedie (sct@redhat.com)
 *  from
 *  Copyright (C) 1992  Remy Card (card@masi.ibp.fr)
 *                      Laboratoire MASI - Institut Blaise Pascal
 *                      Universite Pierre et Marie Curie (Paris VI)
 *  from
 *  linux/fs/minix/truncate.c   Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  ext4fs fsync primitive
 *
 *  Big-endian to little-endian byte-swapping/bitmaps by
 *        David S. Miller (davem@caip.rutgers.edu), 1995
 *
 *  Removed unnecessary code duplication for little endian machines
 *  and excessive __inline__s.
 *        Andi Kleen, 1997
 *
 * Major simplications and cleanup - we only need to do the metadata, because
 * we can depend on generic_block_fdatasync() to sync the data blocks.
 */

#include <linux/time.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/blkdev.h>
#include <linux/cred.h>
#include <linux/uidgid.h>
#include <linux/dax.h>

#include "ext4.h"
#include "ext4_jbd2.h"

#include <trace/events/ext4.h>

/*
 * If we're not journaling and this is a just-created file, we have to
 * sync our parent directory (if it was freshly created) since
 * otherwise it will only be written by writeback, leaving a huge
 * window during which a crash may lose the file.  This may apply for
 * the parent directory's parent as well, and so on recursively, if
 * they are also freshly created.
 */
static int ext4_sync_parent(struct inode *inode)
{
	struct dentry *dentry, *next;
	int ret = 0;

	if (!ext4_test_inode_state(inode, EXT4_STATE_NEWENTRY))
		return 0;
	dentry = d_find_any_alias(inode);
	if (!dentry)
		return 0;
	while (ext4_test_inode_state(inode, EXT4_STATE_NEWENTRY)) {
		ext4_clear_inode_state(inode, EXT4_STATE_NEWENTRY);

		next = dget_parent(dentry);
		dput(dentry);
		dentry = next;
		inode = dentry->d_inode;

		/*
		 * The directory inode may have gone through rmdir by now. But
		 * the inode itself and its blocks are still allocated (we hold
		 * a reference to the inode via its dentry), so it didn't go
		 * through ext4_evict_inode()) and so we are safe to flush
		 * metadata blocks and the inode.
		 */
		ret = sync_mapping_buffers(inode->i_mapping);
		if (ret)
			break;
		ret = sync_inode_metadata(inode, 1);
		if (ret)
			break;
	}
	dput(dentry);
	return ret;
}

/*
 * akpm: A new design for ext4_sync_file().
 *
 * This is only called from sys_fsync(), sys_fdatasync() and sys_msync().
 * There cannot be a transaction open by this task.
 * Another task could have dirtied this inode.  Its data can be in any
 * state in the journalling system.
 *
 * What we do is just kick off a commit and wait on it.  This will snapshot the
 * inode to disk.
 */

int ext4_sync_file(struct file *file, loff_t start, loff_t end, int datasync)
{
	struct inode *inode = file->f_mapping->host;
	struct ext4_inode_info *ei = EXT4_I(inode);
	journal_t *journal = EXT4_SB(inode->i_sb)->s_journal;
	int ret = 0, err;
	tid_t commit_tid;
	bool needs_barrier = false;
	bool async_fsync = false;
	bool commit_need_wait = false;

	if (unlikely(ext4_forced_shutdown(EXT4_SB(inode->i_sb))))
		return -EIO;

	J_ASSERT(ext4_journal_current_handle() == NULL);

	trace_ext4_sync_file_enter(file, datasync);

	if (sb_rdonly(inode->i_sb)) {
		/* Make sure that we read updated s_mount_flags value */
		smp_rmb();
		if (EXT4_SB(inode->i_sb)->s_mount_flags & EXT4_MF_FS_ABORTED)
			ret = -EROFS;
		goto out;
	}

	if (!journal) {
		ret = __generic_file_fsync(file, start, end, datasync);
		if (!ret)
			ret = ext4_sync_parent(inode);
		if (test_opt(inode->i_sb, BARRIER))
			goto issue_flush;
		goto out;
	}

	/*
	 * We split filemap_write_and_wait_range into two parts, the aim is to
	 * overlap device IO operations with computation parts in the fsync path.
	 *
	 * Hopefully it can reduce fsync latency especially when fsynced file is large
	 * and there are many dirty pages needed to be written back to the underlying device.
	 * At this time, the filemap_write_and_wait_range may take long time to finish,
	 * it has enough windows for other journal handles joined into the current
	 * running transaction which can increase fsync latency.
	 *
	 * The idea is from paper "Asynchronous I/O Stack: A Low-latency Kernel IO
	 * Stack for Ultra-Low Latency SSDs"
	 */
	if ((!dax_mapping(inode->i_mapping) && inode->i_mapping->nrpages) ||
	    (dax_mapping(inode->i_mapping) && inode->i_mapping->nrexceptional)) {
		ret = filemap_fdatawrite_range(inode->i_mapping, start, end);
		if (ret && ret != -EIO)
			filemap_fdatawait_range(inode->i_mapping, start, end);
	} else {
		ret = filemap_check_errors(inode->i_mapping);
	}

	if (ret)
		return ret;

	/*
	 * data=writeback,ordered:
	 *  The caller's filemap_fdatawrite()/wait will sync the data.
	 *  Metadata is in the journal, we wait for proper transaction to
	 *  commit here.
	 *
	 * data=journal:
	 *  filemap_fdatawrite won't do anything (the buffers are clean).
	 *  ext4_force_commit will write the file data into the journal and
	 *  will wait on that.
	 *  filemap_fdatawait() will encounter a ton of newly-dirtied pages
	 *  (they were dirtied by commit).  But that's OK - the blocks are
	 *  safe in-journal, which is all fsync() needs to ensure.
	 */
	if (ext4_should_journal_data(inode)) {
		/*
		 * As stated above, if data=journal, filemap_fdatawrite_range won't
		 * do anything, but we still call filemap_fdatawait_range here because
		 * the original design will call it inside filemap_write_and_wait_range.
		 * This may be removed later.
		 */
		ret = filemap_fdatawait_range(inode->i_mapping, start, end);
		if (!ret)
			ret = ext4_force_commit(inode->i_sb);
		goto out;
	}

	commit_tid = datasync ? ei->i_datasync_tid : ei->i_sync_tid;
	if (journal->j_flags & JBD2_BARRIER &&
	    !jbd2_trans_will_send_data_barrier(journal, commit_tid))
		needs_barrier = true;

	commit_need_wait = jbd2_transaction_need_wait(journal, commit_tid);
	if (test_opt(inode->i_sb, ASYNC_FSYNC)) {
		ext4_debug("ext4_sync_file: total sync is %u, async sync is %u\n",
				  atomic_read(&EXT4_SB(inode->i_sb)->s_total_fsync),
				  atomic_read(&EXT4_SB(inode->i_sb)->s_async_fsync));

		atomic_inc(&EXT4_SB(inode->i_sb)->s_total_fsync);
		/*
		 * If current process is neither root process nor system process and
		 * current process is not in system group, we don't want to wait
		 * corresponding transcation to complete.
		 */
#define AID_SYSTEM 1000 /* system server */
		if (!uid_eq(GLOBAL_ROOT_UID, current_fsuid()) &&
			  !(in_group_p(make_kgid(current_user_ns(), AID_SYSTEM)))) {
			atomic_inc(&EXT4_SB(inode->i_sb)->s_async_fsync);
			/* Start committing transaction */
			if (commit_need_wait)
				jbd2_log_start_commit(journal, commit_tid);

			ext4_debug("comm: %s: (uid %u, gid %u): don't wait transaction finish\n",
				  current->comm, current_fsuid(), current_fsgid());
			async_fsync = true;
			goto fdata_wait;
		}
	}

	if (commit_need_wait)
		jbd2_log_start_commit(journal, commit_tid);

fdata_wait:
	ret = filemap_fdatawait_range(inode->i_mapping, start, end);
	/* Wait for the transaction to finish */
	if (!async_fsync && commit_need_wait) {
		if (!ret)
			ret = jbd2_log_wait_commit(journal, commit_tid);
	}

	if (!async_fsync && needs_barrier) {
	issue_flush:
		err = blkdev_issue_flush(inode->i_sb->s_bdev, GFP_KERNEL, NULL);
		if (!ret)
			ret = err;
	}
out:
	err = file_check_and_advance_wb_err(file);
	if (ret == 0)
		ret = err;
	trace_ext4_sync_file_exit(inode, ret);
	return ret;
}
