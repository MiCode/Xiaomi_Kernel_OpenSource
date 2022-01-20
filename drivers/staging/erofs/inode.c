// SPDX-License-Identifier: GPL-2.0
/*
 * linux/drivers/staging/erofs/inode.c
 *
 * Copyright (C) 2017-2018 HUAWEI, Inc.
 *             http://www.huawei.com/
 * Created by Gao Xiang <gaoxiang25@huawei.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of the Linux
 * distribution for more details.
 */
#include "xattr.h"

#include <trace/events/erofs.h>

/*
 * if inode is successfully read, return its inode page (or sometimes
 * the inode payload page if it's an extended inode) in order to fill
 * inline data if possible.
 */
static struct page *read_inode(struct inode *inode, unsigned int *ofs)
{
	struct super_block *sb = inode->i_sb;
	struct erofs_sb_info *sbi = EROFS_SB(sb);
	struct erofs_vnode *vi = EROFS_V(inode);
	const erofs_off_t inode_loc = iloc(sbi, vi->nid);
	erofs_blk_t blkaddr;
	struct page *page;
	struct erofs_inode_v1 *v1;
	struct erofs_inode_v2 *v2, *copied = NULL;
	unsigned int ifmt;
	int err;

	blkaddr = erofs_blknr(inode_loc);
	*ofs = erofs_blkoff(inode_loc);

	debugln("%s, reading inode nid %llu at %u of blkaddr %u",
		__func__, vi->nid, *ofs, blkaddr);

	page = erofs_get_meta_page(sb, blkaddr, false);
	if (IS_ERR(page)) {
		errln("failed to get inode (nid: %llu) page, err %ld",
		      vi->nid, PTR_ERR(page));
		return page;
	}

	v1 = page_address(page) + *ofs;
	ifmt = le16_to_cpu(v1->i_advise);

	if (ifmt & ~EROFS_I_ALL) {
		errln("unsupported i_format %u of nid %llu", ifmt, vi->nid);
		err = -EOPNOTSUPP;
		goto err_out;
	}

	vi->data_mapping_mode = __inode_data_mapping(ifmt);
	if (unlikely(vi->data_mapping_mode >= EROFS_INODE_LAYOUT_MAX)) {
		errln("unknown data mapping mode %u of nid %llu",
			vi->data_mapping_mode, vi->nid);
		err = -EOPNOTSUPP;
		goto err_out;
	}

	switch (__inode_version(ifmt)) {
	case EROFS_INODE_LAYOUT_V2:
		vi->inode_isize = sizeof(struct erofs_inode_v2);
		/* check if the inode acrosses page boundary */
		if (*ofs + vi->inode_isize <= PAGE_SIZE) {
			*ofs += vi->inode_isize;
			v2 = (struct erofs_inode_v2 *)v1;
		} else {
			const unsigned int gotten = PAGE_SIZE - *ofs;

			copied = kmalloc(vi->inode_isize, GFP_NOFS);
			if (!copied) {
				err = -ENOMEM;
				goto err_out;
			}
			memcpy(copied, v1, gotten);
			unlock_page(page);
			put_page(page);

			page = erofs_get_meta_page(sb, blkaddr + 1, false);
			if (IS_ERR(page)) {
				errln("failed to get inode payload page (nid: %llu), err %ld",
				      vi->nid, PTR_ERR(page));
				kfree(copied);
				return page;
			}
			*ofs = vi->inode_isize - gotten;
			memcpy((u8 *)copied + gotten, page_address(page), *ofs);
			v2 = copied;
		}
		vi->xattr_isize = ondisk_xattr_ibody_size(v2->i_xattr_icount);

		inode->i_mode = le16_to_cpu(v2->i_mode);
		if (S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode) ||
						S_ISLNK(inode->i_mode)) {
			vi->raw_blkaddr = le32_to_cpu(v2->i_u.raw_blkaddr);
		} else if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode)) {
			inode->i_rdev =
				new_decode_dev(le32_to_cpu(v2->i_u.rdev));
		} else if (S_ISFIFO(inode->i_mode) || S_ISSOCK(inode->i_mode)) {
			inode->i_rdev = 0;
		} else {
			goto bogusimode;
		}

		i_uid_write(inode, le32_to_cpu(v2->i_uid));
		i_gid_write(inode, le32_to_cpu(v2->i_gid));
		set_nlink(inode, le32_to_cpu(v2->i_nlink));

		/* extended inode has its own timestamp */
		inode->i_ctime.tv_sec = le64_to_cpu(v2->i_ctime);
		inode->i_ctime.tv_nsec = le32_to_cpu(v2->i_ctime_nsec);

		inode->i_size = le64_to_cpu(v2->i_size);
		kfree(copied);
		break;
	case EROFS_INODE_LAYOUT_V1:
		vi->inode_isize = sizeof(struct erofs_inode_v1);
		*ofs += vi->inode_isize;
		vi->xattr_isize = ondisk_xattr_ibody_size(v1->i_xattr_icount);

		inode->i_mode = le16_to_cpu(v1->i_mode);
		if (S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode) ||
						S_ISLNK(inode->i_mode)) {
			vi->raw_blkaddr = le32_to_cpu(v1->i_u.raw_blkaddr);
		} else if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode)) {
			inode->i_rdev =
				new_decode_dev(le32_to_cpu(v1->i_u.rdev));
		} else if (S_ISFIFO(inode->i_mode) || S_ISSOCK(inode->i_mode)) {
			inode->i_rdev = 0;
		} else {
			goto bogusimode;
		}

		i_uid_write(inode, le16_to_cpu(v1->i_uid));
		i_gid_write(inode, le16_to_cpu(v1->i_gid));
		set_nlink(inode, le16_to_cpu(v1->i_nlink));

		/* use build time for compact inodes */
		inode->i_ctime.tv_sec = sbi->build_time;
		inode->i_ctime.tv_nsec = sbi->build_time_nsec;

		inode->i_size = le32_to_cpu(v1->i_size);
		break;
	default:
		errln("unsupported on-disk inode version %u of nid %llu",
		      __inode_version(ifmt), vi->nid);
		err = -EOPNOTSUPP;
		goto err_out;
	}

	inode->i_mtime.tv_sec = inode->i_ctime.tv_sec;
	inode->i_atime.tv_sec = inode->i_ctime.tv_sec;
	inode->i_mtime.tv_nsec = inode->i_ctime.tv_nsec;
	inode->i_atime.tv_nsec = inode->i_ctime.tv_nsec;

	/* measure inode.i_blocks as the generic filesystem */
	inode->i_blocks = ((inode->i_size - 1) >> 9) + 1;
	return page;
bogusimode:
	errln("bogus i_mode (%o) @ nid %llu", inode->i_mode, vi->nid);
	err = -EIO;
err_out:
	DBG_BUGON(1);
	kfree(copied);
	unlock_page(page);
	put_page(page);
	return ERR_PTR(err);
}

/*
 * try_lock can be required since locking order is:
 *   file data(fs_inode)
 *        meta(bd_inode)
 * but the majority of the callers is "iget",
 * in that case we are pretty sure no deadlock since
 * no data operations exist. However I tend to
 * try_lock since it takes no much overhead and
 * will success immediately.
 */
static int fill_inline_data(struct inode *inode, void *data, unsigned m_pofs)
{
	struct erofs_vnode *vi = EROFS_V(inode);
	struct erofs_sb_info *sbi = EROFS_I_SB(inode);
	int mode = vi->data_mapping_mode;

	DBG_BUGON(mode >= EROFS_INODE_LAYOUT_MAX);

	/* should be inode inline C */
	if (mode != EROFS_INODE_LAYOUT_INLINE)
		return 0;

	/* fast symlink (following ext4) */
	if (S_ISLNK(inode->i_mode) && inode->i_size < PAGE_SIZE) {
		char *lnk = erofs_kmalloc(sbi, inode->i_size + 1, GFP_KERNEL);

		if (unlikely(lnk == NULL))
			return -ENOMEM;

		m_pofs += vi->xattr_isize;

		/* inline symlink data shouldn't across page boundary as well */
		if (unlikely(m_pofs + inode->i_size > PAGE_SIZE)) {
			DBG_BUGON(1);
			kfree(lnk);
			return -EIO;
		}

		/* get in-page inline data */
		memcpy(lnk, data + m_pofs, inode->i_size);
		lnk[inode->i_size] = '\0';

		inode->i_link = lnk;
		set_inode_fast_symlink(inode);
	}
	return -EAGAIN;
}

static int fill_inode(struct inode *inode, int isdir)
{
	struct page *page;
	unsigned int ofs;
	int err = 0;

	trace_erofs_fill_inode(inode, isdir);

	/* read inode base data from disk */
	page = read_inode(inode, &ofs);
	if (IS_ERR(page)) {
		return PTR_ERR(page);
	} else {
		/* setup the new inode */
		if (S_ISREG(inode->i_mode)) {
#ifdef CONFIG_EROFS_FS_XATTR
			inode->i_op = &erofs_generic_xattr_iops;
#endif
			inode->i_fop = &generic_ro_fops;
		} else if (S_ISDIR(inode->i_mode)) {
			inode->i_op =
#ifdef CONFIG_EROFS_FS_XATTR
				&erofs_dir_xattr_iops;
#else
				&erofs_dir_iops;
#endif
			inode->i_fop = &erofs_dir_fops;
		} else if (S_ISLNK(inode->i_mode)) {
			/* by default, page_get_link is used for symlink */
			inode->i_op =
#ifdef CONFIG_EROFS_FS_XATTR
				&erofs_symlink_xattr_iops,
#else
				&page_symlink_inode_operations;
#endif
			inode_nohighmem(inode);
		} else if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode) ||
			S_ISFIFO(inode->i_mode) || S_ISSOCK(inode->i_mode)) {
#ifdef CONFIG_EROFS_FS_XATTR
			inode->i_op = &erofs_special_inode_operations;
#endif
			init_special_inode(inode, inode->i_mode, inode->i_rdev);
		} else {
			err = -EIO;
			goto out_unlock;
		}

		if (is_inode_layout_compression(inode)) {
#ifdef CONFIG_EROFS_FS_ZIP
			inode->i_mapping->a_ops =
				&z_erofs_vle_normalaccess_aops;
#else
			err = -ENOTSUPP;
#endif
			goto out_unlock;
		}

		inode->i_mapping->a_ops = &erofs_raw_access_aops;

		/* fill last page if inline data is available */
		fill_inline_data(inode, page_address(page), ofs);
	}

out_unlock:
	unlock_page(page);
	put_page(page);
	return err;
}

struct inode *erofs_iget(struct super_block *sb,
	erofs_nid_t nid, bool isdir)
{
	struct inode *inode = iget_locked(sb, nid);

	if (unlikely(inode == NULL))
		return ERR_PTR(-ENOMEM);

	if (inode->i_state & I_NEW) {
		int err;
		struct erofs_vnode *vi = EROFS_V(inode);
		vi->nid = nid;

		err = fill_inode(inode, isdir);
		if (likely(!err))
			unlock_new_inode(inode);
		else {
			iget_failed(inode);
			inode = ERR_PTR(err);
		}
	}
	return inode;
}

#ifdef CONFIG_EROFS_FS_XATTR
const struct inode_operations erofs_generic_xattr_iops = {
	.listxattr = erofs_listxattr,
};
#endif

#ifdef CONFIG_EROFS_FS_XATTR
const struct inode_operations erofs_symlink_xattr_iops = {
	.get_link = page_get_link,
	.listxattr = erofs_listxattr,
};
#endif

const struct inode_operations erofs_special_inode_operations = {
#ifdef CONFIG_EROFS_FS_XATTR
	.listxattr = erofs_listxattr,
#endif
};

#ifdef CONFIG_EROFS_FS_XATTR
const struct inode_operations erofs_fast_symlink_xattr_iops = {
	.get_link = simple_get_link,
	.listxattr = erofs_listxattr,
};
#endif

