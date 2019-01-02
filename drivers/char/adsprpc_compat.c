/*
 * Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/msm_ion.h>

#include "adsprpc_compat.h"
#include "adsprpc_shared.h"

#define COMPAT_FASTRPC_IOCTL_INVOKE \
		_IOWR('R', 1, struct compat_fastrpc_ioctl_invoke)
#define COMPAT_FASTRPC_IOCTL_MMAP \
		_IOWR('R', 2, struct compat_fastrpc_ioctl_mmap)
#define COMPAT_FASTRPC_IOCTL_MUNMAP \
		_IOWR('R', 3, struct compat_fastrpc_ioctl_munmap)
#define COMPAT_FASTRPC_IOCTL_INVOKE_FD \
		_IOWR('R', 4, struct compat_fastrpc_ioctl_invoke_fd)
#define COMPAT_FASTRPC_IOCTL_INIT \
		_IOWR('R', 6, struct compat_fastrpc_ioctl_init)
#define COMPAT_FASTRPC_IOCTL_MMAP_64 \
		_IOWR('R', 14, struct compat_fastrpc_ioctl_mmap_64)
#define COMPAT_FASTRPC_IOCTL_MUNMAP_64 \
		_IOWR('R', 15, struct compat_fastrpc_ioctl_munmap_64)
#define COMPAT_FASTRPC_IOCTL_CONTROL \
		_IOWR('R', 12, struct compat_fastrpc_ioctl_control)

struct compat_remote_buf {
	compat_uptr_t pv;	/* buffer pointer */
	compat_size_t len;	/* length of buffer */
};

union compat_remote_arg {
	struct compat_remote_buf buf;
	compat_uint_t h;
};

struct compat_fastrpc_ioctl_invoke {
	compat_uint_t handle;	/* remote handle */
	compat_uint_t sc;	/* scalars describing the data */
	compat_uptr_t pra;	/* remote arguments list */
};

struct compat_fastrpc_ioctl_invoke_fd {
	struct compat_fastrpc_ioctl_invoke inv;
	compat_uptr_t fds;	/* fd list */
};

struct compat_fastrpc_ioctl_mmap {
	compat_int_t fd;	/* ion fd */
	compat_uint_t flags;	/* flags for dsp to map with */
	compat_uptr_t vaddrin;	/* optional virtual address */
	compat_size_t size;	/* size */
	compat_uptr_t vaddrout;	/* dsps virtual address */
};

struct compat_fastrpc_ioctl_mmap_64 {
	compat_int_t fd;	/* ion fd */
	compat_uint_t flags;	/* flags for dsp to map with */
	compat_u64 vaddrin;	/* optional virtual address */
	compat_size_t size;	/* size */
	compat_u64 vaddrout;	/* dsps virtual address */
};

struct compat_fastrpc_ioctl_munmap {
	compat_uptr_t vaddrout;	/* address to unmap */
	compat_size_t size;	/* size */
};

struct compat_fastrpc_ioctl_munmap_64 {
	compat_u64 vaddrout;	/* address to unmap */
	compat_size_t size;	/* size */
};

struct compat_fastrpc_ioctl_init {
	compat_uint_t flags;	/* one of FASTRPC_INIT_* macros */
	compat_uptr_t file;	/* pointer to elf file */
	compat_int_t filelen;	/* elf file length */
	compat_int_t filefd;	/* ION fd for the file */
	compat_uptr_t mem;	/* mem for the PD */
	compat_int_t memlen;	/* mem length */
	compat_int_t memfd;	/* ION fd for the mem */
};

#define FASTRPC_CONTROL_LATENCY		(1)
struct compat_fastrpc_ctrl_latency {
	compat_uint_t enable;		/* !latency control enable */
	compat_uint_t level;		/* !level of control */
};

#define FASTRPC_CONTROL_SMMU		(2)
struct compat_fastrpc_ctrl_smmu {
	compat_uint_t sharedcb;
};

#define FASTRPC_CONTROL_KALLOC		(3)
struct compat_fastrpc_ctrl_kalloc {
	compat_uint_t kalloc_support; /* Remote memory allocation from kernel */
};

struct compat_fastrpc_ioctl_control {
	compat_uint_t req;
	union {
		struct compat_fastrpc_ctrl_latency lp;
		struct compat_fastrpc_ctrl_smmu smmu;
		struct compat_fastrpc_ctrl_kalloc kalloc;
	};
};
static int compat_get_fastrpc_ioctl_invoke(
			struct compat_fastrpc_ioctl_invoke_fd __user *inv32,
			struct fastrpc_ioctl_invoke_fd __user **inva,
			unsigned int cmd)
{
	compat_uint_t u, sc;
	compat_size_t s;
	compat_uptr_t p;
	struct fastrpc_ioctl_invoke_fd *inv;
	union compat_remote_arg *pra32;
	union remote_arg *pra;
	int err, len, num, j;

	err = get_user(sc, &inv32->inv.sc);
	if (err)
		return err;

	len = REMOTE_SCALARS_LENGTH(sc);
	VERIFY(err, NULL != (inv = compat_alloc_user_space(
				sizeof(*inv) + len * sizeof(*pra))));
	if (err)
		return -EFAULT;

	pra = (union remote_arg *)(inv + 1);
	err = put_user(pra, &inv->inv.pra);
	err |= put_user(sc, &inv->inv.sc);
	err |= get_user(u, &inv32->inv.handle);
	err |= put_user(u, &inv->inv.handle);
	err |= get_user(p, &inv32->inv.pra);
	if (err)
		return err;

	pra32 = compat_ptr(p);
	pra = (union remote_arg *)(inv + 1);
	num = REMOTE_SCALARS_INBUFS(sc) + REMOTE_SCALARS_OUTBUFS(sc);
	for (j = 0; j < num; j++) {
		err |= get_user(p, &pra32[j].buf.pv);
		err |= put_user(p, (uintptr_t *)&pra[j].buf.pv);
		err |= get_user(s, &pra32[j].buf.len);
		err |= put_user(s, &pra[j].buf.len);
	}
	for (j = 0; j < REMOTE_SCALARS_INHANDLES(sc); j++) {
		err |= get_user(u, &pra32[num + j].h);
		err |= put_user(u, &pra[num + j].h);
	}

	err |= put_user(NULL, &inv->fds);
	if (cmd == COMPAT_FASTRPC_IOCTL_INVOKE_FD) {
		err |= get_user(p, &inv32->fds);
		err |= put_user(p, (compat_uptr_t *)&inv->fds);
	}

	*inva = inv;
	return err;
}

static int compat_put_fastrpc_ioctl_invoke(
			struct compat_fastrpc_ioctl_invoke_fd __user *inv32,
			struct fastrpc_ioctl_invoke_fd __user *inv)
{
	compat_uptr_t p;
	compat_uint_t u, h;
	union compat_remote_arg *pra32;
	union remote_arg *pra;
	int err, i, num;

	err = get_user(u, &inv32->inv.sc);
	err |= get_user(p, &inv32->inv.pra);
	if (err)
		return err;

	pra32 = compat_ptr(p);
	pra = (union remote_arg *)(inv + 1);
	num = REMOTE_SCALARS_INBUFS(u) + REMOTE_SCALARS_OUTBUFS(u)
		+ REMOTE_SCALARS_INHANDLES(u);
	for (i = 0;  i < REMOTE_SCALARS_OUTHANDLES(u); i++) {
		err |= get_user(h, &pra[num + i].h);
		err |= put_user(h, &pra32[num + i].h);
	}

	return err;
}

static int compat_get_fastrpc_ioctl_mmap(
			struct compat_fastrpc_ioctl_mmap __user *map32,
			struct fastrpc_ioctl_mmap __user *map)
{
	compat_uint_t u;
	compat_int_t i;
	compat_size_t s;
	compat_uptr_t p;
	int err;

	err = get_user(i, &map32->fd);
	err |= put_user(i, &map->fd);
	err |= get_user(u, &map32->flags);
	err |= put_user(u, &map->flags);
	err |= get_user(p, &map32->vaddrin);
	err |= put_user(p, (uintptr_t *)&map->vaddrin);
	err |= get_user(s, &map32->size);
	err |= put_user(s, &map->size);

	return err;
}

static int compat_get_fastrpc_ioctl_mmap_64(
			struct compat_fastrpc_ioctl_mmap_64 __user *map32,
			struct fastrpc_ioctl_mmap __user *map)
{
	compat_uint_t u;
	compat_int_t i;
	compat_size_t s;
	compat_u64 p;
	int err;

	err = get_user(i, &map32->fd);
	err |= put_user(i, &map->fd);
	err |= get_user(u, &map32->flags);
	err |= put_user(u, &map->flags);
	err |= get_user(p, &map32->vaddrin);
	err |= put_user(p, &map->vaddrin);
	err |= get_user(s, &map32->size);
	err |= put_user(s, &map->size);

	return err;
}

static int compat_put_fastrpc_ioctl_mmap(
			struct compat_fastrpc_ioctl_mmap __user *map32,
			struct fastrpc_ioctl_mmap __user *map)
{
	compat_uptr_t p;
	int err;

	err = get_user(p, &map->vaddrout);
	err |= put_user(p, &map32->vaddrout);

	return err;
}

static int compat_put_fastrpc_ioctl_mmap_64(
			struct compat_fastrpc_ioctl_mmap_64 __user *map32,
			struct fastrpc_ioctl_mmap __user *map)
{
	compat_u64 p;
	int err;

	err = get_user(p, &map->vaddrout);
	err |= put_user(p, &map32->vaddrout);

	return err;
}

static int compat_get_fastrpc_ioctl_munmap(
			struct compat_fastrpc_ioctl_munmap __user *unmap32,
			struct fastrpc_ioctl_munmap __user *unmap)
{
	compat_uptr_t p;
	compat_size_t s;
	int err;

	err = get_user(p, &unmap32->vaddrout);
	err |= put_user(p, &unmap->vaddrout);
	err |= get_user(s, &unmap32->size);
	err |= put_user(s, &unmap->size);

	return err;
}

static int compat_get_fastrpc_ioctl_munmap_64(
			struct compat_fastrpc_ioctl_munmap_64 __user *unmap32,
			struct fastrpc_ioctl_munmap __user *unmap)
{
	compat_u64 p;
	compat_size_t s;
	int err;

	err = get_user(p, &unmap32->vaddrout);
	err |= put_user(p, &unmap->vaddrout);
	err |= get_user(s, &unmap32->size);
	err |= put_user(s, &unmap->size);

	return err;
}

static int compat_get_fastrpc_ioctl_control(
			struct compat_fastrpc_ioctl_control __user *ctrl32,
			struct fastrpc_ioctl_control __user *ctrl)
{
	compat_uptr_t p;
	int err;

	err = get_user(p, &ctrl32->req);
	err |= put_user(p, &ctrl->req);

	return err;
}

static int compat_get_fastrpc_ioctl_init(
			struct compat_fastrpc_ioctl_init __user *init32,
			struct fastrpc_ioctl_init __user *init)
{
	compat_uint_t u;
	compat_uptr_t p;
	compat_int_t i;
	int err;

	err = get_user(u, &init32->flags);
	err |= put_user(u, &init->flags);
	err |= get_user(p, &init32->file);
	err |= put_user(p, &init->file);
	err |= get_user(i, &init32->filelen);
	err |= put_user(i, &init->filelen);
	err |= get_user(i, &init32->filefd);
	err |= put_user(i, &init->filefd);
	err |= get_user(p, &init32->mem);
	err |= put_user(p, &init->mem);
	err |= get_user(i, &init32->memlen);
	err |= put_user(i, &init->memlen);
	err |= get_user(i, &init32->memfd);
	err |= put_user(i, &init->memfd);

	return err;
}

long compat_fastrpc_device_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	int err = 0;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_FASTRPC_IOCTL_INVOKE:
	case COMPAT_FASTRPC_IOCTL_INVOKE_FD:
	{
		struct compat_fastrpc_ioctl_invoke_fd __user *inv32;
		struct fastrpc_ioctl_invoke_fd __user *inv;
		long ret;

		inv32 = compat_ptr(arg);
		VERIFY(err, 0 == compat_get_fastrpc_ioctl_invoke(inv32,
							&inv, cmd));
		if (err)
			return err;
		ret = filp->f_op->unlocked_ioctl(filp, FASTRPC_IOCTL_INVOKE_FD,
							(unsigned long)inv);
		if (ret)
			return ret;
		VERIFY(err, 0 == compat_put_fastrpc_ioctl_invoke(inv32, inv));
		return err;
	}
	case COMPAT_FASTRPC_IOCTL_MMAP:
	{
		struct compat_fastrpc_ioctl_mmap __user *map32;
		struct fastrpc_ioctl_mmap __user *map;
		long ret;

		map32 = compat_ptr(arg);
		VERIFY(err, NULL != (map = compat_alloc_user_space(
							sizeof(*map))));
		if (err)
			return -EFAULT;
		VERIFY(err, 0 == compat_get_fastrpc_ioctl_mmap(map32, map));
		if (err)
			return err;
		ret = filp->f_op->unlocked_ioctl(filp, FASTRPC_IOCTL_MMAP,
							(unsigned long)map);
		if (ret)
			return ret;
		VERIFY(err, 0 == compat_put_fastrpc_ioctl_mmap(map32, map));
		return err;
	}
	case COMPAT_FASTRPC_IOCTL_MMAP_64:
	{
		struct compat_fastrpc_ioctl_mmap_64  __user *map32;
		struct fastrpc_ioctl_mmap __user *map;
		long ret;

		map32 = compat_ptr(arg);
		VERIFY(err, NULL != (map = compat_alloc_user_space(
							sizeof(*map))));
		if (err)
			return -EFAULT;
		VERIFY(err, 0 == compat_get_fastrpc_ioctl_mmap_64(map32, map));
		if (err)
			return err;
		ret = filp->f_op->unlocked_ioctl(filp, FASTRPC_IOCTL_MMAP_64,
							(unsigned long)map);
		if (ret)
			return ret;
		VERIFY(err, 0 == compat_put_fastrpc_ioctl_mmap_64(map32, map));
		return err;
	}
	case COMPAT_FASTRPC_IOCTL_MUNMAP:
	{
		struct compat_fastrpc_ioctl_munmap __user *unmap32;
		struct fastrpc_ioctl_munmap __user *unmap;

		unmap32 = compat_ptr(arg);
		VERIFY(err, NULL != (unmap = compat_alloc_user_space(
							sizeof(*unmap))));
		if (err)
			return -EFAULT;
		VERIFY(err, 0 == compat_get_fastrpc_ioctl_munmap(unmap32,
							unmap));
		if (err)
			return err;
		return filp->f_op->unlocked_ioctl(filp, FASTRPC_IOCTL_MUNMAP,
							(unsigned long)unmap);
	}
	case COMPAT_FASTRPC_IOCTL_MUNMAP_64:
	{
		struct compat_fastrpc_ioctl_munmap_64 __user *unmap32;
		struct fastrpc_ioctl_munmap __user *unmap;

		unmap32 = compat_ptr(arg);
		VERIFY(err, NULL != (unmap = compat_alloc_user_space(
							sizeof(*unmap))));
		if (err)
			return -EFAULT;
		VERIFY(err, 0 == compat_get_fastrpc_ioctl_munmap_64(unmap32,
							unmap));
		if (err)
			return err;
		return filp->f_op->unlocked_ioctl(filp, FASTRPC_IOCTL_MUNMAP_64,
							(unsigned long)unmap);
	}
	case COMPAT_FASTRPC_IOCTL_INIT:
	{
		struct compat_fastrpc_ioctl_init __user *init32;
		struct fastrpc_ioctl_init __user *init;

		init32 = compat_ptr(arg);
		VERIFY(err, NULL != (init = compat_alloc_user_space(
							sizeof(*init))));
		if (err)
			return -EFAULT;
		VERIFY(err, 0 == compat_get_fastrpc_ioctl_init(init32,
							init));
		if (err)
			return err;
		return filp->f_op->unlocked_ioctl(filp, FASTRPC_IOCTL_INIT,
							(unsigned long)init);
	}
	case FASTRPC_IOCTL_GETINFO:
	{
		compat_uptr_t __user *info32;
		uint32_t __user *info;
		compat_uint_t u;
		long ret;

		info32 = compat_ptr(arg);
		VERIFY(err, NULL != (info = compat_alloc_user_space(
							sizeof(*info))));
		if (err)
			return -EFAULT;
		ret = filp->f_op->unlocked_ioctl(filp, FASTRPC_IOCTL_GETINFO,
							(unsigned long)info);
		if (ret)
			return ret;
		err = get_user(u, info);
		err |= put_user(u, info32);
		return err;
	}
	case FASTRPC_IOCTL_SETMODE:
		return filp->f_op->unlocked_ioctl(filp, cmd,
						(unsigned long)compat_ptr(arg));
	case COMPAT_FASTRPC_IOCTL_CONTROL:
	{
		struct compat_fastrpc_ioctl_control __user *ctrl32;
		struct fastrpc_ioctl_control __user *ctrl;
		compat_uptr_t p;

		ctrl32 = compat_ptr(arg);
		VERIFY(err, NULL != (ctrl = compat_alloc_user_space(
							sizeof(*ctrl))));
		if (err)
			return -EFAULT;
		VERIFY(err, 0 == compat_get_fastrpc_ioctl_control(ctrl32,
							ctrl));
		if (err)
			return err;
		err = filp->f_op->unlocked_ioctl(filp, FASTRPC_IOCTL_CONTROL,
							(unsigned long)ctrl);
		if (err)
			return err;
		err = get_user(p, &ctrl32->req);
		if (err)
			return err;
		if (p == FASTRPC_CONTROL_KALLOC) {
			err = get_user(p, &ctrl->kalloc.kalloc_support);
			err |= put_user(p, &ctrl32->kalloc.kalloc_support);
		}
		return err;
	}
	default:
		return -ENOIOCTLCMD;
	}
}
