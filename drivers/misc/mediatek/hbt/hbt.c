// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/security.h>
#include <linux/compat.h>
#include <linux/thread_info.h>
#include <linux/dirent.h>
#include "hbt.h"

static bool is_32bit(void)
{
	return test_thread_flag(TIF_32BIT);
}

static void set_32bit(bool val)
{
	if (val)
		set_thread_flag(TIF_32BIT);
	else
		clear_thread_flag(TIF_32BIT);
}

static long hbt_get_version(struct hbt_abi_version __user *argp)
{
	bool compat;
	struct hbt_abi_version abi = { .major = HBT_ABI_MAJOR,
					   .minor = HBT_ABI_MINOR };

	set_32bit(true);
	compat = in_compat_syscall();
	set_32bit(false);

	if (!compat)
		return -EIO;

	if (copy_to_user(argp, &abi, sizeof(abi)))
		return -EFAULT;

	return 0;
}

static int validate_mm_fields(const struct hbt_mm *mm_fields)
{
	unsigned long mmap_max_addr = TASK_SIZE;
	unsigned long mmap_min_addr = PAGE_SIZE;
	long retval = -EINVAL, i;

	static const unsigned char offsets[] = {
		offsetof(struct hbt_mm, start_code),
		offsetof(struct hbt_mm, end_code),
		offsetof(struct hbt_mm, start_data),
		offsetof(struct hbt_mm, end_data),
		offsetof(struct hbt_mm, start_brk),
		offsetof(struct hbt_mm, brk),
		offsetof(struct hbt_mm, start_stack),
		offsetof(struct hbt_mm, arg_start),
		offsetof(struct hbt_mm, arg_end),
		offsetof(struct hbt_mm, env_start),
		offsetof(struct hbt_mm, env_end),
	};

	for (i = 0; i < ARRAY_SIZE(offsets); i++) {
		u64 val = *(u64 *)((char *)mm_fields + offsets[i]);

		if ((unsigned long)val >= mmap_max_addr ||
		    (unsigned long)val < mmap_min_addr)
			goto out;
	}

#define __prctl_check_order(__m1, __op, __m2)                                  \
	(((unsigned long)mm_fields->__m1 __op(unsigned long) mm_fields->__m2) ? \
		0 :                                                            \
		-EINVAL)
	retval = __prctl_check_order(start_code, <, end_code);
	retval |= __prctl_check_order(start_data, <=, end_data);
	retval |= __prctl_check_order(start_brk, <=, brk);
	retval |= __prctl_check_order(arg_start, <=, arg_end);
	retval |= __prctl_check_order(env_start, <=, env_end);
	if (retval)
		goto out;
#undef __prctl_check_order

	retval = -EINVAL;

	if (check_data_rlimit(rlimit(RLIMIT_DATA), mm_fields->brk,
			      mm_fields->start_brk, mm_fields->end_data,
			      mm_fields->start_data))
		goto out;

	retval = 0;
out:
	return retval;
}

static long hbt_set_mm(struct hbt_mm __user *argp)
{
	struct mm_struct *mm = current->mm;
	unsigned long user_auxv[AT_VECTOR_SIZE];
	struct hbt_mm mm_fields;
	long retval;

	BUILD_BUG_ON(sizeof(user_auxv) != sizeof(mm->saved_auxv));

	if (copy_from_user(&mm_fields, argp, sizeof(mm_fields)))
		return -EFAULT;

	/*
	 * Sanity-check the input values.
	 */
	retval = validate_mm_fields(&mm_fields);
	if (retval)
		return retval;

	if (mm_fields.auxv_size) {
		if (!mm_fields.auxv ||
		    mm_fields.auxv_size > sizeof(mm->saved_auxv))
			return -EINVAL;

		memset(user_auxv, 0, sizeof(user_auxv));
		if (copy_from_user(user_auxv,
				   (const void __user *)mm_fields.auxv,
				   mm_fields.auxv_size))
			return -EFAULT;

		/* Last entry must be AT_NULL as specification requires */
		user_auxv[AT_VECTOR_SIZE - 2] = AT_NULL;
		user_auxv[AT_VECTOR_SIZE - 1] = AT_NULL;
	}

	if (mmap_read_lock_killable(mm))
		return -EINTR;

	spin_lock(&mm->arg_lock);
	mm->start_code = mm_fields.start_code;
	mm->end_code = mm_fields.end_code;
	mm->start_data = mm_fields.start_data;
	mm->end_data = mm_fields.end_data;
	mm->start_brk = mm_fields.start_brk;
	mm->brk = mm_fields.brk;
	mm->start_stack = mm_fields.start_stack;
	mm->arg_start = mm_fields.arg_start;
	mm->arg_end = mm_fields.arg_end;
	mm->env_start = mm_fields.env_start;
	mm->env_end = mm_fields.env_end;
	spin_unlock(&mm->arg_lock);

	if (mm_fields.auxv_size)
		memcpy(mm->saved_auxv, user_auxv, sizeof(user_auxv));

	mmap_read_unlock(mm);

	return retval;
}

static long hbt_set_mmap_base(unsigned long arg)
{
	unsigned long mmap_max_addr = TASK_SIZE;
	/* mmap_min_addr is not exported to modules, pick a safe default. */
	unsigned long mmap_min_addr = 0x10000000;
	struct mm_struct *mm = current->mm;

	if (arg >= mmap_max_addr || arg <= mmap_min_addr)
		return -EINVAL;

	if (mmap_write_lock_killable(mm))
		return -EINTR;

	mm->mmap_base = arg;

	mmap_write_unlock(mm);

	return 0;
}

static long hbt_compat_ioctl(struct hbt_compat_ioctl __user *argp)
{
	struct hbt_compat_ioctl args;
	struct fd f;
	long retval;

	if (copy_from_user(&args, argp, sizeof(args)))
		return -EFAULT;

	f = fdget(args.fd);
	if (!f.file)
		return -EBADF;

	/*
	 * Pretend to be a 32-bit task for the duration of this syscall.
	 */
	set_32bit(true);

	retval = security_file_ioctl(f.file, args.cmd, args.arg);
	if (retval)
		goto out;

	retval = -ENOIOCTLCMD;
	if (f.file->f_op->compat_ioctl)
		retval = f.file->f_op->compat_ioctl(f.file, args.cmd, args.arg);

out:
	set_32bit(false);
	fdput(f);

	return retval;
}

static long
hbt_compat_set_robust_list(struct hbt_compat_robust_list __user *argp)
{
	struct hbt_compat_robust_list args;

	if (copy_from_user(&args, argp, sizeof(args)))
		return -EFAULT;

	if (args.len != sizeof(*current->compat_robust_list))
		return -EINVAL;

	current->compat_robust_list = compat_ptr(args.head);

	return 0;
}

static long
hbt_compat_get_robust_list(struct hbt_compat_robust_list __user *argp)
{
	struct hbt_compat_robust_list out;

	out.head = ptr_to_compat(current->compat_robust_list);
	out.len = sizeof(*current->compat_robust_list);

	if (copy_to_user(argp, &out, sizeof(out)))
		return -EFAULT;

	return 0;
}

#define unsafe_copy_dirent_name(_dst, _src, _len, label)                       \
	do {                                                                   \
		char __user *dst = (_dst);                                     \
		const char *src = (_src);                                      \
		size_t len = (_len);                                           \
		unsafe_put_user(0, dst + len, label);                          \
		unsafe_copy_to_user(dst, src, len, label);                     \
	} while (0)

static int verify_dirent_name(const char *name, int len)
{
	if (len <= 0 || len >= PATH_MAX)
		return -EIO;
	if (memchr(name, '/', len))
		return -EIO;
	return 0;
}

struct getdents_callback64 {
	struct dir_context ctx;
	struct linux_dirent64 __user *current_dir;
	int prev_reclen;
	int count;
	int error;
};

static int filldir64(struct dir_context *ctx, const char *name, int namlen,
		     loff_t offset, u64 ino, unsigned int d_type)
{
	struct linux_dirent64 __user *dirent, *prev;
	struct getdents_callback64 *buf =
		container_of(ctx, struct getdents_callback64, ctx);
	int reclen = ALIGN(offsetof(struct linux_dirent64, d_name) + namlen + 1,
			   sizeof(u64));
	int prev_reclen;

	buf->error = verify_dirent_name(name, namlen);
	if (unlikely(buf->error))
		return buf->error;
	buf->error = -EINVAL; /* only used if we fail.. */
	if (reclen > buf->count)
		return -EINVAL;
	prev_reclen = buf->prev_reclen;
	if (prev_reclen && signal_pending(current))
		return -EINTR;
	dirent = buf->current_dir;
	prev = (void __user *)dirent - prev_reclen;
	if (!user_write_access_begin(prev, reclen + prev_reclen))
		goto efault;

	unsafe_put_user(offset, &prev->d_off, efault_end);
	unsafe_put_user(ino, &dirent->d_ino, efault_end);
	unsafe_put_user(reclen, &dirent->d_reclen, efault_end);
	unsafe_put_user(d_type, &dirent->d_type, efault_end);
	unsafe_copy_dirent_name(dirent->d_name, name, namlen, efault_end);
	user_write_access_end();

	buf->prev_reclen = reclen;
	buf->current_dir = (void __user *)dirent + reclen;
	buf->count -= reclen;
	return 0;

efault_end:
	user_write_access_end();
efault:
	buf->error = -EFAULT;
	return -EFAULT;
}

static struct fd my_fdget_pos(unsigned int fd)
{
	struct fd f = fdget(fd);

	if (f.file && (f.file->f_mode & FMODE_ATOMIC_POS)) {
		if (file_count(f.file) > 1) {
			f.flags |= FDPUT_POS_UNLOCK;
			mutex_lock(&f.file->f_pos_lock);
		}
	}
	return f;
}
static void my_fdput_pos(struct fd fd)
{
	if (fd.flags & FDPUT_POS_UNLOCK)
		mutex_unlock(&fd.file->f_pos_lock);
	fdput(fd);
}

static long
hbt_compat_getdents64(struct hbt_compat_getdents64 __user *argp)
{
	struct hbt_compat_getdents64 args;
	struct fd f;
	struct getdents_callback64 buf = {};
	int error;

	if (copy_from_user(&args, argp, sizeof(args)))
		return -EFAULT;

	f = my_fdget_pos(args.fd);
	if (!f.file)
		return -EBADF;

	set_32bit(true);

	buf.ctx.actor = filldir64;
	buf.count = args.count;
	buf.current_dir = (struct linux_dirent64 __user *)args.dirp;

	error = iterate_dir(f.file, &buf.ctx);
	if (error >= 0)
		error = buf.error;
	if (buf.prev_reclen) {
		struct linux_dirent64 __user *lastdirent;
		typeof(lastdirent->d_off) d_off = buf.ctx.pos;

		lastdirent = (void __user *)buf.current_dir - buf.prev_reclen;
		if (put_user(d_off, &lastdirent->d_off))
			error = -EFAULT;
		else
			error = args.count - buf.count;
	}

	set_32bit(false);
	my_fdput_pos(f);
	return error;
}

static long hbt_compat_lseek(struct hbt_compat_lseek __user *argp)
{
	struct hbt_compat_lseek args;
	off_t retval;
	struct fd f;

	if (copy_from_user(&args, argp, sizeof(args)))
		return -EFAULT;

	f = my_fdget_pos(args.fd);
	if (!f.file)
		return -EBADF;

	set_32bit(true);

	retval = -EINVAL;
	if (args.whence <= SEEK_MAX)
		retval = vfs_llseek(f.file, args.offset, args.whence);

	set_32bit(false);
	my_fdput_pos(f);
	return retval;
}

static long hbt_ioctl(struct file *filp, unsigned int cmd,
			  unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	if (is_32bit())
		return -EINVAL;

	switch (cmd) {
	case HBT_GET_VERSION:
		return hbt_get_version(argp);
	case HBT_SET_MM:
		return hbt_set_mm(argp);
	case HBT_SET_MMAP_BASE:
		return hbt_set_mmap_base(arg);
	case HBT_COMPAT_IOCTL:
		return hbt_compat_ioctl(argp);
	case HBT_COMPAT_SET_ROBUST_LIST:
		return hbt_compat_set_robust_list(argp);
	case HBT_COMPAT_GET_ROBUST_LIST:
		return hbt_compat_get_robust_list(argp);
	case HBT_COMPAT_GETDENTS64:
		return hbt_compat_getdents64(argp);
	case HBT_COMPAT_LSEEK:
		return hbt_compat_lseek(argp);
	}

	return -ENOIOCTLCMD;
}

static const struct file_operations hbt_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = hbt_ioctl,
};

static struct miscdevice hbt_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "hbt",
	.fops = &hbt_fops,
	.mode = 0666,
};

module_misc_device(hbt_device);

MODULE_LICENSE("GPL");
