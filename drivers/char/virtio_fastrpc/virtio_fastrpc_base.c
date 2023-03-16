// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/virtio_config.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>
#include "virtio_fastrpc_core.h"
#include "virtio_fastrpc_mem.h"
#include "virtio_fastrpc_queue.h"

/* Virtio ID of FASTRPC : 0xC004 */
#define VIRTIO_ID_FASTRPC				49156
/* indicates remote invoke with buffer attributes is supported */
#define VIRTIO_FASTRPC_F_INVOKE_ATTR			1
/* indicates remote invoke with CRC is supported */
#define VIRTIO_FASTRPC_F_INVOKE_CRC			2
/* indicates remote mmap/munmap is supported */
#define VIRTIO_FASTRPC_F_MMAP				3
/* indicates QOS setting is supported */
#define VIRTIO_FASTRPC_F_CONTROL			4
/* indicates version is available in config space */
#define VIRTIO_FASTRPC_F_VERSION			5
/* indicates domain num is available in config space */
#define VIRTIO_FASTRPC_F_DOMAIN_NUM			6
#define VIRTIO_FASTRPC_F_VQUEUE_SETTING			7
/* indicates fastrpc_mmap/fastrpc_munmap is supported */
#define VIRTIO_FASTRPC_F_MEM_MAP			8

#define NUM_CHANNELS			4 /* adsp, mdsp, slpi, cdsp0*/
#define NUM_DEVICES			2 /* adsprpc-smd, adsprpc-smd-secure */

#define INIT_FILELEN_MAX		(2*1024*1024)
#define INIT_MEMLEN_MAX			(8*1024*1024)

#define MAX_FASTRPC_BUF_SIZE		(1024*1024*4)
#define DEF_FASTRPC_BUF_SIZE		(128*1024)
#define DEBUGFS_SIZE			3072

/*
 * FE_MAJOR_VER is used for the FE and BE's version match check,
 * and it MUST be equal to BE_MAJOR_VER, otherwise virtual fastrpc
 * cannot work properly. It increases when fundamental protocol is
 * changed between FE and BE.
 */
#define FE_MAJOR_VER 0x5
/* FE_MINOR_VER is used to track patches in this driver. It does not
 * need to be matched with BE_MINOR_VER. And it will return to 0 when
 * FE_MAJOR_VER is increased.
 */
#define FE_MINOR_VER 0x4
#define FE_VERSION (FE_MAJOR_VER << 16 | FE_MINOR_VER)
#define BE_MAJOR_VER(ver) (((ver) >> 16) & 0xffff)

struct virtio_fastrpc_config {
	u32 version;
	u32 domain_num;
	u32 max_buf_size;
} __packed;


static struct vfastrpc_apps gfa;

static ssize_t vfastrpc_debugfs_read(struct file *filp, char __user *buffer,
					 size_t count, loff_t *position)
{
	struct fastrpc_file *fl = filp->private_data;
	struct vfastrpc_file *vfl = to_vfastrpc_file(fl);
	struct vfastrpc_mmap *map = NULL;
	struct vfastrpc_buf *buf = NULL;
	struct hlist_node *n;
	struct vfastrpc_invoke_ctx *ictx = NULL;
	char *fileinfo = NULL;
	unsigned int len = 0;
	int err = 0;
	char title[] = "=========================";

	/* Only allow read once */
	if (*position != 0)
		goto bail;

	fileinfo = kzalloc(DEBUGFS_SIZE, GFP_KERNEL);
	if (!fileinfo) {
		err = -ENOMEM;
		goto bail;
	}
	if (fl && vfl) {
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
				"\n%s %d %s %d\n", "channel =", vfl->domain,
				"proc_attr =", vfl->procattrs);

		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"\n========%s %s %s========\n", title,
			" LIST OF BUFS ", title);
		spin_lock(&fl->hlock);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%-19s|%-19s|%-19s\n\n",
			"virt", "phys", "size");
		hlist_for_each_entry_safe(buf, n, &fl->cached_bufs, hn) {
			len += scnprintf(fileinfo + len,
				DEBUGFS_SIZE - len,
				"0x%-17lX|0x%-17llX|%-19zu\n",
				(unsigned long)buf->va,
				(uint64_t)page_to_phys(buf->pages[0]),
				buf->size);
		}

		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"\n==%s %s %s==\n", title,
			" LIST OF PENDING CONTEXTS ", title);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%-20s|%-10s|%-10s|%-10s|%-20s\n\n",
			"sc", "pid", "tgid", "size", "handle");
		hlist_for_each_entry_safe(ictx, n, &fl->clst.pending, hn) {
			len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
				"0x%-18X|%-10d|%-10d|%-10zu|0x%-20X\n\n",
				ictx->sc, ictx->pid, ictx->tgid,
				ictx->size, ictx->handle);
		}

		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"\n%s %s %s\n", title,
			" LIST OF INTERRUPTED CONTEXTS ", title);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%-20s|%-10s|%-10s|%-10s|%-20s\n\n",
			"sc", "pid", "tgid", "size", "handle");
		hlist_for_each_entry_safe(ictx, n, &fl->clst.interrupted, hn) {
			len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
				"0x%-18X|%-10d|%-10d|%-10zu|0x%-20X\n\n",
				ictx->sc, ictx->pid, ictx->tgid,
				ictx->size, ictx->handle);
		}
		spin_unlock(&fl->hlock);

		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"\n========%s %s %s========\n", title,
			" LIST OF MAPS ", title);
		len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
			"%-20s|%-20s|%-10s|%-10s|%-10s\n\n",
			"va", "phys", "size", "attr", "refs");
		mutex_lock(&fl->map_mutex);
		hlist_for_each_entry_safe(map, n, &fl->maps, hn) {
			len += scnprintf(fileinfo + len, DEBUGFS_SIZE - len,
				"0x%-18lX|0x%-18llX|%-10zu|0x%-10x|%-10d\n\n",
				map->va, map->phys, map->size,
				map->attr, map->refs);
		}
		mutex_unlock(&fl->map_mutex);
	}

	if (len > DEBUGFS_SIZE)
		len = DEBUGFS_SIZE;

	err = simple_read_from_buffer(buffer, count, position, fileinfo, len);
	kfree(fileinfo);
bail:
	return err;
}

static const struct file_operations debugfs_fops = {
	.open = simple_open,
	.read = vfastrpc_debugfs_read,
};

static inline void get_fastrpc_ioctl_mmap_64(
			struct fastrpc_ioctl_mmap_64 *mmap64,
			struct fastrpc_ioctl_mmap *immap)
{
	immap->fd = mmap64->fd;
	immap->flags = mmap64->flags;
	immap->vaddrin = (uintptr_t)mmap64->vaddrin;
	immap->size = mmap64->size;
}

static inline void put_fastrpc_ioctl_mmap_64(
			struct fastrpc_ioctl_mmap_64 *mmap64,
			struct fastrpc_ioctl_mmap *immap)
{
	mmap64->vaddrout = (uint64_t)immap->vaddrout;
}

static inline void get_fastrpc_ioctl_munmap_64(
			struct fastrpc_ioctl_munmap_64 *munmap64,
			struct fastrpc_ioctl_munmap *imunmap)
{
	imunmap->vaddrout = (uintptr_t)munmap64->vaddrout;
	imunmap->size = munmap64->size;
}

static int vfastrpc_mmap_ioctl(struct vfastrpc_file *vfl,
		unsigned int ioctl_num, union fastrpc_ioctl_param *p,
		void *param)
{
	union {
		struct fastrpc_ioctl_mmap mmap;
		struct fastrpc_ioctl_munmap munmap;
	} i;
	struct vfastrpc_apps *me = vfl->apps;
	int err = 0;

	switch (ioctl_num) {
	case FASTRPC_IOCTL_MEM_MAP:
		if (!me->has_mem_map) {
			dev_err(me->dev, "mem_map is not supported\n");
			return -ENOTTY;
		}
		K_COPY_FROM_USER(err, 0, &p->mem_map, param,
						sizeof(p->mem_map));
		if (err)
			return err;

		VERIFY(err, 0 == (err = vfastrpc_internal_mem_map(vfl,
						&p->mem_map)));
		if (err)
			return err;

		K_COPY_TO_USER(err, 0, param, &p->mem_map, sizeof(p->mem_map));
		if (err)
			return err;
		break;
	case FASTRPC_IOCTL_MEM_UNMAP:
		if (!me->has_mem_map) {
			dev_err(me->dev, "mem_unmap is not supported\n");
			return -ENOTTY;
		}
		K_COPY_FROM_USER(err, 0, &p->mem_unmap, param,
						sizeof(p->mem_unmap));
		if (err)
			return err;

		VERIFY(err, 0 == (err = vfastrpc_internal_mem_unmap(vfl,
						&p->mem_unmap)));
		if (err)
			return err;
		break;
	case FASTRPC_IOCTL_MMAP:
		if (!me->has_mmap) {
			dev_err(me->dev, "mmap is not supported\n");
			return -ENOTTY;
		}

		K_COPY_FROM_USER(err, 0, &p->mmap, param, sizeof(p->mmap));
		if (err)
			return err;

		VERIFY(err, 0 == (err = vfastrpc_internal_mmap(vfl, &p->mmap)));
		if (err)
			return err;

		K_COPY_TO_USER(err, 0, param, &p->mmap, sizeof(p->mmap));
		break;
	case FASTRPC_IOCTL_MUNMAP:
		if (!(me->has_mmap)) {
			dev_err(me->dev, "munmap is not supported\n");
			return -ENOTTY;
		}

		K_COPY_FROM_USER(err, 0, &p->munmap, param, sizeof(p->munmap));
		if (err)
			return err;

		VERIFY(err, 0 == (err = vfastrpc_internal_munmap(vfl, &p->munmap)));
		break;
	case FASTRPC_IOCTL_MMAP_64:
		if (!(me->has_mmap)) {
			dev_err(me->dev, "mmap is not supported\n");
			return -ENOTTY;
		}

		K_COPY_FROM_USER(err, 0, &p->mmap64, param, sizeof(p->mmap64));
		if (err)
			return err;

		get_fastrpc_ioctl_mmap_64(&p->mmap64, &i.mmap);
		VERIFY(err, 0 == (err = vfastrpc_internal_mmap(vfl, &i.mmap)));
		if (err)
			return err;

		put_fastrpc_ioctl_mmap_64(&p->mmap64, &i.mmap);
		K_COPY_TO_USER(err, 0, param, &p->mmap64, sizeof(p->mmap64));
		break;
	case FASTRPC_IOCTL_MUNMAP_64:
		if (!(me->has_mmap)) {
			dev_err(me->dev, "munmap is not supported\n");
			return -ENOTTY;
		}

		K_COPY_FROM_USER(err, 0, &p->munmap64, param,
						sizeof(p->munmap64));
		if (err)
			return err;

		get_fastrpc_ioctl_munmap_64(&p->munmap64, &i.munmap);
		VERIFY(err, 0 == (err = vfastrpc_internal_munmap(vfl, &i.munmap)));
		break;
	case FASTRPC_IOCTL_MUNMAP_FD:
		K_COPY_FROM_USER(err, 0, &p->munmap_fd, param, sizeof(p->munmap_fd));
		if (err)
			return err;

		VERIFY(err, 0 == (err = vfastrpc_internal_munmap_fd(vfl, &p->munmap_fd)));
		break;
	default:
		err = -ENOTTY;
		dev_err(me->dev, "bad ioctl: 0x%x\n", ioctl_num);
		break;
	}
	return err;
}

static int vfastrpc_setmode_ioctl(unsigned long ioctl_param,
		struct vfastrpc_file *vfl)
{
	struct vfastrpc_apps *me = vfl->apps;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);
	int err = 0;

	switch ((uint32_t)ioctl_param) {
	case FASTRPC_MODE_PARALLEL:
	case FASTRPC_MODE_SERIAL:
		fl->mode = (uint32_t)ioctl_param;
		break;
	case FASTRPC_MODE_SESSION:
		err = -ENOTTY;
		dev_err(me->dev, "session mode is not supported\n");
		break;
	case FASTRPC_MODE_PROFILE:
		fl->profile = (uint32_t)ioctl_param;
		break;
	default:
		err = -ENOTTY;
		break;
	}
	return err;
}

int fastrpc_setmode(unsigned long ioctl_param, struct fastrpc_file *fl)
{
	struct vfastrpc_file *vfl = to_vfastrpc_file(fl);

	return vfastrpc_setmode_ioctl(ioctl_param, vfl);
}

static int vfastrpc_control_ioctl(struct fastrpc_ioctl_control *cp,
		void *param, struct vfastrpc_file *vfl)
{
	int err = 0;

	K_COPY_FROM_USER(err, 0, cp, param,
			sizeof(*cp));
	if (err)
		return err;

	VERIFY(err, 0 == (err = vfastrpc_internal_control(vfl, cp)));
	if (err)
		return err;

	if (cp->req == FASTRPC_CONTROL_KALLOC)
		K_COPY_TO_USER(err, 0, param, cp, sizeof(*cp));

	return err;
}

static int vfastrpc_get_info_ioctl(void *param, struct vfastrpc_file *vfl)
{
	int err = 0;
	uint32_t info;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);

	K_COPY_FROM_USER(err, fl->is_compat, &info, param, sizeof(info));
	if (err)
		return err;

	VERIFY(err, 0 == (err = vfastrpc_internal_get_info(vfl, &info)));
	if (err)
		return err;

	K_COPY_TO_USER(err, fl->is_compat, param, &info, sizeof(info));
	return err;
}

int fastrpc_get_info(struct fastrpc_file *fl, uint32_t *info)
{
	struct vfastrpc_file *vfl = to_vfastrpc_file(fl);

	return vfastrpc_get_info_ioctl(info, vfl);
}

static int vfastrpc_init_ioctl(struct fastrpc_ioctl_init_attrs *init,
		void *param, struct vfastrpc_file *vfl)
{
	int err = 0;
	struct fastrpc_file *fl = to_fastrpc_file(vfl);

	K_COPY_FROM_USER(err, fl->is_compat, init, param, sizeof(*init));
	if (err)
		return err;

	VERIFY(err, init->init.filelen >= 0 &&
		init->init.filelen < INIT_FILELEN_MAX);
	if (err)
		return err;

	VERIFY(err, init->init.memlen >= 0 &&
		init->init.memlen < INIT_MEMLEN_MAX);
	if (err)
		return err;

	VERIFY(err, 0 == (err = vfastrpc_internal_init_process(vfl, init)));
	return err;
}

static int check_invoke_supported(struct vfastrpc_file *vfl,
		struct fastrpc_ioctl_invoke_async *inv)
{
	int err = 0;
	struct vfastrpc_apps *me = vfl->apps;

	if (inv->attrs && !(me->has_invoke_attr)) {
		dev_err(me->dev, "invoke attr is not supported\n");
		return -ENOTTY;
	}

	if (inv->crc && !(me->has_invoke_crc)) {
		dev_err(me->dev, "invoke crc is not supported\n");
		err = -ENOTTY;
	}
	return err;
}

int fastrpc_internal_invoke(struct fastrpc_file *fl, uint32_t mode,
				   uint32_t kernel,
				   struct fastrpc_ioctl_invoke_async *inv)
{
	struct vfastrpc_file *vfl = to_vfastrpc_file(fl);

	return vfastrpc_internal_invoke(vfl, mode, inv);
}

int fastrpc_internal_invoke2(struct fastrpc_file *fl,
				struct fastrpc_ioctl_invoke2 *inv2)
{
	struct vfastrpc_file *vfl = to_vfastrpc_file(fl);

	return vfastrpc_internal_invoke2(vfl, inv2);
}

int fastrpc_internal_munmap(struct fastrpc_file *fl,
				   struct fastrpc_ioctl_munmap *ud)
{
	struct vfastrpc_file *vfl = to_vfastrpc_file(fl);

	return vfastrpc_internal_munmap(vfl, ud);
}

int fastrpc_internal_mmap(struct fastrpc_file *fl,
				 struct fastrpc_ioctl_mmap *ud)
{
	struct vfastrpc_file *vfl = to_vfastrpc_file(fl);

	return vfastrpc_internal_mmap(vfl, ud);
}

int fastrpc_init_process(struct fastrpc_file *fl,
				struct fastrpc_ioctl_init_attrs *uproc)
{
	struct vfastrpc_file *vfl = to_vfastrpc_file(fl);

	return vfastrpc_internal_init_process(vfl, uproc);
}

int fastrpc_internal_control(struct fastrpc_file *fl,
					struct fastrpc_ioctl_control *cp)
{
	struct vfastrpc_file *vfl = to_vfastrpc_file(fl);

	return vfastrpc_internal_control(vfl, cp);
}

int fastrpc_get_info_from_kernel(
		struct fastrpc_ioctl_capability *cap,
		struct fastrpc_file *fl)
{
	struct vfastrpc_file *vfl = to_vfastrpc_file(fl);

	return vfastrpc_get_info_from_kernel(cap, vfl);
}

int fastrpc_dspsignal_cancel_wait(struct fastrpc_file *fl,
				  struct fastrpc_ioctl_dspsignal_cancel_wait *cancel)
{
	return -ENOTTY;
}

int fastrpc_dspsignal_wait(struct fastrpc_file *fl,
			   struct fastrpc_ioctl_dspsignal_wait *wait)
{
	return -ENOTTY;
}

int fastrpc_dspsignal_signal(struct fastrpc_file *fl,
			     struct fastrpc_ioctl_dspsignal_signal *sig)
{
	return -ENOTTY;
}

int fastrpc_dspsignal_create(struct fastrpc_file *fl,
			     struct fastrpc_ioctl_dspsignal_create *create)
{
	return -ENOTTY;
}

int fastrpc_dspsignal_destroy(struct fastrpc_file *fl,
			      struct fastrpc_ioctl_dspsignal_destroy *destroy)
{
	return -ENOTTY;
}

int fastrpc_internal_mem_unmap(struct fastrpc_file *fl,
				struct fastrpc_ioctl_mem_unmap *ud)
{
	struct vfastrpc_file *vfl = to_vfastrpc_file(fl);

	return vfastrpc_internal_mem_unmap(vfl, ud);
}

int fastrpc_internal_mem_map(struct fastrpc_file *fl,
				struct fastrpc_ioctl_mem_map *ud)
{
	struct vfastrpc_file *vfl = to_vfastrpc_file(fl);

	return vfastrpc_internal_mem_map(vfl, ud);
}

static long vfastrpc_ioctl(struct file *file, unsigned int ioctl_num,
				 unsigned long ioctl_param)
{
	union fastrpc_ioctl_param p;
	void *param = (char *)ioctl_param;
	struct fastrpc_file *fl = (struct fastrpc_file *)file->private_data;
	struct vfastrpc_file *vfl = to_vfastrpc_file(fl);
	struct vfastrpc_apps *me = &gfa;
	int size = 0, err = 0;

	p.inv.fds = NULL;
	p.inv.attrs = NULL;
	p.inv.crc = NULL;
	p.inv.perf_kernel = NULL;
	p.inv.perf_dsp = NULL;
	p.inv.job = NULL;

	spin_lock(&fl->hlock);
	if (fl->file_close == 1) {
		err = -EBADF;
		dev_warn(me->dev, "fastrpc_device_release is happening, So not sending any new requests to DSP\n");
		spin_unlock(&fl->hlock);
		goto bail;
	}
	spin_unlock(&fl->hlock);

	switch (ioctl_num) {
	case FASTRPC_IOCTL_INVOKE:
		size = sizeof(struct fastrpc_ioctl_invoke);
		fallthrough;
	case FASTRPC_IOCTL_INVOKE_FD:
		if (!size)
			size = sizeof(struct fastrpc_ioctl_invoke_fd);
		fallthrough;
	case FASTRPC_IOCTL_INVOKE_ATTRS:
		if (!size)
			size = sizeof(struct fastrpc_ioctl_invoke_attrs);
		fallthrough;
	case FASTRPC_IOCTL_INVOKE_CRC:
		if (!size)
			size = sizeof(struct fastrpc_ioctl_invoke_crc);
		fallthrough;
	case FASTRPC_IOCTL_INVOKE_PERF:
		if (!size)
			size = sizeof(struct fastrpc_ioctl_invoke_perf);
		K_COPY_FROM_USER(err, 0, &p.inv, param, size);
		if (err)
			goto bail;

		err = check_invoke_supported(vfl, &p.inv);
		if (err)
			goto bail;

		err = vfastrpc_internal_invoke(vfl, fl->mode, &p.inv);
		break;
	case FASTRPC_IOCTL_INVOKE2:
		K_COPY_FROM_USER(err, 0, &p.inv2, param,
				sizeof(struct fastrpc_ioctl_invoke2));
		if (err) {
			err = -EFAULT;
			goto bail;
		}
		err = vfastrpc_internal_invoke2(vfl, &p.inv2);
		break;
	case FASTRPC_IOCTL_MEM_MAP:
	case FASTRPC_IOCTL_MEM_UNMAP:
	case FASTRPC_IOCTL_MMAP:
	case FASTRPC_IOCTL_MUNMAP:
	case FASTRPC_IOCTL_MMAP_64:
	case FASTRPC_IOCTL_MUNMAP_64:
	case FASTRPC_IOCTL_MUNMAP_FD:
		err = vfastrpc_mmap_ioctl(vfl, ioctl_num, &p, param);
		break;
	case FASTRPC_IOCTL_SETMODE:
		err = vfastrpc_setmode_ioctl(ioctl_param, vfl);
		break;
	case FASTRPC_IOCTL_CONTROL:
		err = vfastrpc_control_ioctl(&p.cp, param, vfl);
		break;
	case FASTRPC_IOCTL_GETINFO:
		err = vfastrpc_get_info_ioctl(param, vfl);
		break;
	case FASTRPC_IOCTL_INIT:
		p.init.attrs = 0;
		p.init.siglen = 0;
		size = sizeof(struct fastrpc_ioctl_init);
		fallthrough;
	case FASTRPC_IOCTL_INIT_ATTRS:
		err = vfastrpc_init_ioctl(&p.init, param, vfl);
		break;
	case FASTRPC_IOCTL_GET_DSP_INFO:
		err = vfastrpc_internal_get_dsp_info(&p.cap, param, vfl);
		break;
	default:
		err = -ENOTTY;
		dev_err(me->dev, "bad ioctl: 0x%x\n", ioctl_num);
		break;
	}
 bail:
	return err;
}

static int vfastrpc_open(struct inode *inode, struct file *filp)
{
	int err = 0;
	struct vfastrpc_file *vfl = NULL;
	struct fastrpc_file *fl = NULL;
	struct vfastrpc_apps *me = &gfa;

	/*
	 * Indicates the device node opened
	 * MINOR_NUM_DEV or MINOR_NUM_SECURE_DEV
	 */
	int dev_minor = MINOR(inode->i_rdev);

	VERIFY(err, ((dev_minor == MINOR_NUM_DEV) ||
			(dev_minor == MINOR_NUM_SECURE_DEV)));
	if (err) {
		dev_err(me->dev, "Invalid dev minor num %d\n", dev_minor);
		return err;
	}

	VERIFY(err, NULL != (vfl = vfastrpc_file_alloc()));
	if (err) {
		dev_err(me->dev, "Allocate vfastrpc_file failed %d\n", dev_minor);
		return err;
	}

	fl = to_fastrpc_file(vfl);
	fl->dev_minor = dev_minor;
	vfl->apps = me;

	filp->private_data = fl;
	return 0;
}

static int vfastrpc_release(struct inode *inode, struct file *file)
{
	struct fastrpc_file *fl = (struct fastrpc_file *)file->private_data;
	struct vfastrpc_file *vfl = to_vfastrpc_file(fl);

	if (vfl) {
		vfastrpc_file_free(vfl);
		file->private_data = NULL;
	}
	return 0;
}

static const struct file_operations fops = {
	.open = vfastrpc_open,
	.release = vfastrpc_release,
	.unlocked_ioctl = vfastrpc_ioctl,
	.compat_ioctl = compat_fastrpc_device_ioctl,
};

static int recv_single(struct virt_msg_hdr *rsp, unsigned int len)
{
	struct vfastrpc_apps *me = &gfa;
	struct virt_fastrpc_msg *msg;

	if (len != rsp->len) {
		dev_err(me->dev, "msg %u len mismatch,expected %u but %d found\n",
				rsp->cmd, rsp->len, len);
		return -EINVAL;
	}
	spin_lock(&me->msglock);
	msg = me->msgtable[rsp->msgid];
	spin_unlock(&me->msglock);

	if (!msg) {
		dev_err(me->dev, "msg %u already free in table[%u]\n",
				rsp->cmd, rsp->msgid);
		return -EINVAL;
	}
	msg->rxbuf = (void *)rsp;

	if (msg->ctx && msg->ctx->asyncjob.isasyncjob)
		vfastrpc_queue_completed_async_job(msg->ctx);
	else
		complete(&msg->work);

	return 0;
}

static void recv_done(struct virtqueue *rvq)
{

	struct vfastrpc_apps *me = &gfa;
	struct virt_msg_hdr *rsp;
	unsigned int len, msgs_received = 0;
	int err;
	unsigned long flags;

	spin_lock_irqsave(&me->rvq.vq_lock, flags);
	rsp = virtqueue_get_buf(rvq, &len);
	if (!rsp) {
		spin_unlock_irqrestore(&me->rvq.vq_lock, flags);
		dev_err(me->dev, "incoming signal, but no used buffer\n");
		return;
	}
	spin_unlock_irqrestore(&me->rvq.vq_lock, flags);

	while (rsp) {
		err = recv_single(rsp, len);
		if (err)
			break;

		msgs_received++;

		spin_lock_irqsave(&me->rvq.vq_lock, flags);
		rsp = virtqueue_get_buf(rvq, &len);
		spin_unlock_irqrestore(&me->rvq.vq_lock, flags);
	}
}

static void virt_init_vq(struct virt_fastrpc_vq *fastrpc_vq,
				struct virtqueue *vq)
{
	spin_lock_init(&fastrpc_vq->vq_lock);
	fastrpc_vq->vq = vq;
}

static int init_vqs(struct vfastrpc_apps *me)
{
	struct virtqueue *vqs[2];
	static const char * const names[] = { "output", "input" };
	vq_callback_t *cbs[] = { NULL, recv_done };
	int err, i;

	err = virtio_find_vqs(me->vdev, 2, vqs, cbs, names, NULL);
	if (err)
		return err;

	virt_init_vq(&me->svq, vqs[0]);
	virt_init_vq(&me->rvq, vqs[1]);


	/* we expect symmetric tx/rx vrings */
	if (virtqueue_get_vring_size(me->rvq.vq) !=
			virtqueue_get_vring_size(me->svq.vq)) {
		dev_err(&me->vdev->dev, "tx/rx vring size does not match\n");
			err = -EINVAL;
		goto vqs_del;
	}

	me->num_bufs = virtqueue_get_vring_size(me->rvq.vq);
	me->rbufs = kcalloc(me->num_bufs, sizeof(void *), GFP_KERNEL);
	if (!me->rbufs) {
		err = -ENOMEM;
		goto vqs_del;
	}
	me->sbufs = kcalloc(me->num_bufs, sizeof(void *), GFP_KERNEL);
	if (!me->sbufs) {
		err = -ENOMEM;
		kfree(me->rbufs);
		goto vqs_del;
	}

	me->order = get_order(me->buf_size);

	for (i = 0; i < me->num_bufs; i++) {
		me->rbufs[i] = (void *)__get_free_pages(GFP_KERNEL, me->order);
		if (!me->rbufs[i]) {
			err = -ENOMEM;
			goto rbuf_del;
		}
	}

	for (i = 0; i < me->num_bufs; i++) {
		me->sbufs[i] = (void *)__get_free_pages(GFP_KERNEL, me->order);
		if (!me->sbufs[i]) {
			err = -ENOMEM;
			goto sbuf_del;
		}
	}
	return 0;

sbuf_del:
	for (i = 0; i < me->num_bufs; i++) {
		if (me->sbufs[i])
			free_pages((unsigned long)me->sbufs[i], me->order);
	}

rbuf_del:
	for (i = 0; i < me->num_bufs; i++) {
		if (me->rbufs[i])
			free_pages((unsigned long)me->rbufs[i], me->order);
	}
	kfree(me->sbufs);
	kfree(me->rbufs);
vqs_del:
	me->vdev->config->del_vqs(me->vdev);
	return err;
}

static int vfastrpc_channel_init(struct vfastrpc_apps *me)
{
	int i;

	me->channel = kcalloc(me->num_channels,
			sizeof(struct vfastrpc_channel_ctx), GFP_KERNEL);
	if (!me->channel)
		return -ENOMEM;
	for (i = 0; i < me->num_channels; i++) {
		/* All channels are secure by default except CDSP and CDSP1*/
		if (i == CDSP_DOMAIN_ID || i == CDSP1_DOMAIN_ID) {
			me->channel[i].secure = false;
			me->channel[i].unsigned_support = true;
		} else {
			me->channel[i].secure = true;
			me->channel[i].unsigned_support = false;
		}
	}
	return 0;
}

static void vfastrpc_channel_deinit(struct vfastrpc_apps *me)
{
	kfree(me->channel);
	me->channel = NULL;
}

static int virt_fastrpc_probe(struct virtio_device *vdev)
{
	struct vfastrpc_apps *me = &gfa;
	struct device *dev = NULL;
	struct device *secure_dev = NULL;
	struct virtio_fastrpc_config config;
	int err, i;

	if (!virtio_has_feature(vdev, VIRTIO_F_VERSION_1))
		return -ENODEV;

	memset(&config, 0x0, sizeof(config));
	if (virtio_has_feature(vdev, VIRTIO_FASTRPC_F_VERSION)) {
		virtio_cread(vdev, struct virtio_fastrpc_config, version, &config.version);
		if (BE_MAJOR_VER(config.version) != FE_MAJOR_VER) {
			dev_err(&vdev->dev, "vdev major version does not match 0x%x:0x%x\n",
					FE_VERSION, config.version);
			return -ENODEV;
		}
	}
	dev_info(&vdev->dev, "virtio fastrpc version 0x%x:0x%x\n",
			FE_VERSION, config.version);

	memset(me, 0, sizeof(*me));
	spin_lock_init(&me->msglock);

	if (virtio_has_feature(vdev, VIRTIO_FASTRPC_F_INVOKE_ATTR))
		me->has_invoke_attr = true;

	if (virtio_has_feature(vdev, VIRTIO_FASTRPC_F_INVOKE_CRC))
		me->has_invoke_crc = true;

	if (virtio_has_feature(vdev, VIRTIO_FASTRPC_F_MMAP))
		me->has_mmap = true;

	if (virtio_has_feature(vdev, VIRTIO_FASTRPC_F_CONTROL))
		me->has_control = true;

	if (virtio_has_feature(vdev, VIRTIO_FASTRPC_F_MEM_MAP))
		me->has_mem_map = true;

	vdev->priv = me;
	me->vdev = vdev;
	me->dev = vdev->dev.parent;

	if (virtio_has_feature(vdev, VIRTIO_FASTRPC_F_VQUEUE_SETTING)) {
		virtio_cread(vdev, struct virtio_fastrpc_config, max_buf_size,
				&config.max_buf_size);
		if (config.max_buf_size > MAX_FASTRPC_BUF_SIZE) {
			dev_err(&vdev->dev, "buffer size 0x%x is exceed to maximum limit 0x%x\n",
					config.max_buf_size, MAX_FASTRPC_BUF_SIZE);
			return -EINVAL;
		}

		me->buf_size = config.max_buf_size;
		dev_info(&vdev->dev, "set buf_size to 0x%x\n", me->buf_size);
	} else {
		dev_info(&vdev->dev, "set buf_size to default value\n");
		me->buf_size = DEF_FASTRPC_BUF_SIZE;
	}

	err = init_vqs(me);
	if (err) {
		dev_err(&vdev->dev, "failed to initialized virtqueue\n");
		return err;
	}

	if (virtio_has_feature(vdev, VIRTIO_FASTRPC_F_DOMAIN_NUM)) {
		virtio_cread(vdev, struct virtio_fastrpc_config, domain_num,
				&config.domain_num);
		dev_info(&vdev->dev, "get domain_num %d from config space\n",
				config.domain_num);
		me->num_channels = config.domain_num;
	} else if (of_get_property(me->dev->of_node, "qcom,domain_num", NULL) != NULL) {
		err = of_property_read_u32(me->dev->of_node, "qcom,domain_num",
					&me->num_channels);
		if (err) {
			dev_err(&vdev->dev, "failed to read domain_num %d\n", err);
			goto alloc_channel_bail;
		}
	} else {
		dev_dbg(&vdev->dev, "set domain_num to default value\n");
		me->num_channels = NUM_CHANNELS;
	}

	err = vfastrpc_channel_init(me);
	if (err) {
		dev_err(&vdev->dev, "failed to init channel context %d\n", err);
		goto alloc_channel_bail;
	}

	me->debugfs_root = debugfs_create_dir("adsprpc", NULL);
	if (IS_ERR_OR_NULL(me->debugfs_root)) {
		dev_warn(&vdev->dev, "%s: %s: failed to create debugfs root dir\n",
			current->comm, __func__);
		me->debugfs_root = NULL;
	}

	me->debugfs_fops = &debugfs_fops;
	err = alloc_chrdev_region(&me->dev_no, 0, me->num_channels, DEVICE_NAME);
	if (err)
		goto alloc_chrdev_bail;

	cdev_init(&me->cdev, &fops);
	me->cdev.owner = THIS_MODULE;
	err = cdev_add(&me->cdev, MKDEV(MAJOR(me->dev_no), 0), NUM_DEVICES);
	if (err)
		goto cdev_init_bail;

	me->class = class_create(THIS_MODULE, "fastrpc");
	if (IS_ERR(me->class))
		goto class_create_bail;

	/*
	 * Create devices and register with sysfs
	 * Create first device with minor number 0
	 */
	dev = device_create(me->class, NULL,
				MKDEV(MAJOR(me->dev_no), MINOR_NUM_DEV),
				NULL, DEVICE_NAME);
	if (IS_ERR_OR_NULL(dev))
		goto device_create_bail;

	/* Create secure device with minor number for secure device */
	secure_dev = device_create(me->class, NULL,
				MKDEV(MAJOR(me->dev_no), MINOR_NUM_SECURE_DEV),
				NULL, DEVICE_NAME_SECURE);
	if (IS_ERR_OR_NULL(secure_dev))
		goto device_create_bail;

	virtio_device_ready(vdev);

	/* set up the receive buffers */
	for (i = 0; i < me->num_bufs; i++) {
		struct scatterlist sg;
		void *cpu_addr = me->rbufs[i];

		sg_init_one(&sg, cpu_addr, me->buf_size);
		err = virtqueue_add_inbuf(me->rvq.vq, &sg, 1, cpu_addr,
				GFP_KERNEL);
		WARN_ON(err); /* sanity check; this can't really happen */
	}

	/* suppress "tx-complete" interrupts */
	virtqueue_disable_cb(me->svq.vq);

	virtqueue_enable_cb(me->rvq.vq);
	virtqueue_kick(me->rvq.vq);

	dev_info(&vdev->dev, "Registered virtio fastrpc device\n");
	return 0;
device_create_bail:
	if (!IS_ERR_OR_NULL(dev))
		device_destroy(me->class, MKDEV(MAJOR(me->dev_no),
						MINOR_NUM_DEV));
	if (!IS_ERR_OR_NULL(secure_dev))
		device_destroy(me->class, MKDEV(MAJOR(me->dev_no),
						MINOR_NUM_SECURE_DEV));
	class_destroy(me->class);
class_create_bail:
	cdev_del(&me->cdev);
cdev_init_bail:
	unregister_chrdev_region(me->dev_no, me->num_channels);
alloc_chrdev_bail:
	debugfs_remove_recursive(me->debugfs_root);
	vfastrpc_channel_deinit(me);
alloc_channel_bail:
	vdev->config->del_vqs(vdev);
	return err;
}

static void virt_fastrpc_remove(struct virtio_device *vdev)
{
	struct vfastrpc_apps *me = &gfa;
	int i;

	device_destroy(me->class, MKDEV(MAJOR(me->dev_no), MINOR_NUM_DEV));
	device_destroy(me->class, MKDEV(MAJOR(me->dev_no),
					MINOR_NUM_SECURE_DEV));
	class_destroy(me->class);
	cdev_del(&me->cdev);
	unregister_chrdev_region(me->dev_no, me->num_channels);
	debugfs_remove_recursive(me->debugfs_root);

	vfastrpc_channel_deinit(me);
	vdev->config->reset(vdev);
	vdev->config->del_vqs(vdev);

	for (i = 0; i < me->num_bufs; i++)
		free_pages((unsigned long)me->rbufs[i], me->order);
	for (i = 0; i < me->num_bufs; i++)
		free_pages((unsigned long)me->sbufs[i], me->order);

	kfree(me->sbufs);
	kfree(me->rbufs);
}

const struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_FASTRPC, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static unsigned int features[] = {
	VIRTIO_FASTRPC_F_INVOKE_ATTR,
	VIRTIO_FASTRPC_F_INVOKE_CRC,
	VIRTIO_FASTRPC_F_MMAP,
	VIRTIO_FASTRPC_F_CONTROL,
	VIRTIO_FASTRPC_F_VERSION,
	VIRTIO_FASTRPC_F_DOMAIN_NUM,
	VIRTIO_FASTRPC_F_VQUEUE_SETTING,
	VIRTIO_FASTRPC_F_MEM_MAP,
};

static struct virtio_driver virtio_fastrpc_driver = {
	.feature_table		= features,
	.feature_table_size	= ARRAY_SIZE(features),
	.driver.name		= KBUILD_MODNAME,
	.driver.owner		= THIS_MODULE,
	.id_table		= id_table,
	.probe			= virt_fastrpc_probe,
	.remove			= virt_fastrpc_remove,
};

static int __init virtio_fastrpc_init(void)
{
	return register_virtio_driver(&virtio_fastrpc_driver);
}

static void __exit virtio_fastrpc_exit(void)
{
	unregister_virtio_driver(&virtio_fastrpc_driver);
}
module_init(virtio_fastrpc_init);
module_exit(virtio_fastrpc_exit);

MODULE_DEVICE_TABLE(virtio, id_table);
MODULE_DESCRIPTION("Virtio fastrpc driver");
MODULE_LICENSE("GPL v2");
