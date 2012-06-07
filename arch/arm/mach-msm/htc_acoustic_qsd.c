/* arch/arm/mach-msm/htc_acoustic_qsd.c
 *
 * Copyright (C) 2009 HTC Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/gpio.h>

#include <mach/msm_smd.h>
#include <mach/msm_rpcrouter.h>
#include <mach/msm_iomap.h>
#include <mach/htc_acoustic_qsd.h>
#include <mach/msm_qdsp6_audio.h>

#include "smd_private.h"

#define ACOUSTIC_IOCTL_MAGIC 'p'
#define ACOUSTIC_UPDATE_ADIE \
	_IOW(ACOUSTIC_IOCTL_MAGIC, 24, unsigned int)

#define HTCACOUSTICPROG 0x30100003
#define HTCACOUSTICVERS 0
#define ONCRPC_ALLOC_ACOUSTIC_MEM_PROC		(1)
#define ONCRPC_UPDATE_ADIE_PROC			(2)
#define ONCRPC_ENABLE_AUX_PGA_LOOPBACK_PROC	(3)
#define ONCRPC_FORCE_HEADSET_SPEAKER_PROC	(4)

#define HTC_ACOUSTIC_TABLE_SIZE        (0x20000)

#define D(fmt, args...) printk(KERN_INFO "htc-acoustic: "fmt, ##args)
#define E(fmt, args...) printk(KERN_ERR "htc-acoustic: "fmt, ##args)

static uint32_t htc_acoustic_vir_addr;
static struct msm_rpc_endpoint *endpoint;
static struct mutex api_lock;
static struct mutex rpc_connect_lock;
static struct qsd_acoustic_ops *the_ops;

void acoustic_register_ops(struct qsd_acoustic_ops *ops)
{
	the_ops = ops;
}

static int is_rpc_connect(void)
{
	mutex_lock(&rpc_connect_lock);
	if (endpoint == NULL) {
		endpoint = msm_rpc_connect(HTCACOUSTICPROG,
				HTCACOUSTICVERS, 0);
		if (IS_ERR(endpoint)) {
			pr_err("%s: init rpc failed! rc = %ld\n",
				__func__, PTR_ERR(endpoint));
			mutex_unlock(&rpc_connect_lock);
			return -1;
		}
	}
	mutex_unlock(&rpc_connect_lock);
	return 0;
}

int turn_mic_bias_on(int on)
{
	D("%s called %d\n", __func__, on);
	if (the_ops->enable_mic_bias)
		the_ops->enable_mic_bias(on);

	return 0;
}
EXPORT_SYMBOL(turn_mic_bias_on);

int force_headset_speaker_on(int enable)
{
	struct speaker_headset_req {
		struct rpc_request_hdr hdr;
		uint32_t enable;
	} spkr_req;

	D("%s called %d\n", __func__, enable);

	if (is_rpc_connect() == -1)
		return -1;

	spkr_req.enable = cpu_to_be32(enable);
	return  msm_rpc_call(endpoint,
		ONCRPC_FORCE_HEADSET_SPEAKER_PROC,
		&spkr_req, sizeof(spkr_req), 5 * HZ);
}
EXPORT_SYMBOL(force_headset_speaker_on);

int enable_aux_loopback(uint32_t enable)
{
	struct aux_loopback_req {
		struct rpc_request_hdr hdr;
		uint32_t enable;
	} aux_req;

	D("%s called %d\n", __func__, enable);

	if (is_rpc_connect() == -1)
		return -1;

	aux_req.enable = cpu_to_be32(enable);
	return  msm_rpc_call(endpoint,
		ONCRPC_ENABLE_AUX_PGA_LOOPBACK_PROC,
		&aux_req, sizeof(aux_req), 5 * HZ);
}
EXPORT_SYMBOL(enable_aux_loopback);

static int acoustic_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long pgoff;
	int rc = -EINVAL;
	size_t size;

	D("mmap\n");

	mutex_lock(&api_lock);

	size = vma->vm_end - vma->vm_start;

	if (vma->vm_pgoff != 0) {
		E("mmap failed: page offset %lx\n", vma->vm_pgoff);
		goto done;
	}

	if (!htc_acoustic_vir_addr) {
		E("mmap failed: smem region not allocated\n");
		rc = -EIO;
		goto done;
	}

	pgoff = MSM_SHARED_RAM_PHYS +
		(htc_acoustic_vir_addr - (uint32_t)MSM_SHARED_RAM_BASE);
	pgoff = ((pgoff + 4095) & ~4095);
	htc_acoustic_vir_addr = ((htc_acoustic_vir_addr + 4095) & ~4095);

	if (pgoff <= 0) {
		E("pgoff wrong. %ld\n", pgoff);
		goto done;
	}

	if (size <= HTC_ACOUSTIC_TABLE_SIZE) {
		pgoff = pgoff >> PAGE_SHIFT;
	} else {
		E("size > HTC_ACOUSTIC_TABLE_SIZE  %d\n", size);
		goto done;
	}

	vma->vm_flags |= VM_IO | VM_RESERVED;
	rc = io_remap_pfn_range(vma, vma->vm_start, pgoff,
				size, vma->vm_page_prot);

	if (rc < 0)
		E("mmap failed: remap error %d\n", rc);

done:	mutex_unlock(&api_lock);
	return rc;
}

static int acoustic_open(struct inode *inode, struct file *file)
{
	int reply_value;
	int rc = -EIO;
	struct set_smem_req {
		struct rpc_request_hdr hdr;
		uint32_t size;
	} req_smem;

	struct set_smem_rep {
		struct rpc_reply_hdr hdr;
		int n;
	} rep_smem;

	D("open\n");

	mutex_lock(&api_lock);

	if (!htc_acoustic_vir_addr) {
		if (is_rpc_connect() == -1)
			goto done;

		req_smem.size = cpu_to_be32(HTC_ACOUSTIC_TABLE_SIZE);
		rc = msm_rpc_call_reply(endpoint,
					ONCRPC_ALLOC_ACOUSTIC_MEM_PROC,
					&req_smem, sizeof(req_smem),
					&rep_smem, sizeof(rep_smem),
					5 * HZ);

		reply_value = be32_to_cpu(rep_smem.n);
		if (reply_value != 0 || rc < 0) {
			E("open failed: ALLOC_ACOUSTIC_MEM_PROC error %d.\n",
			rc);
			goto done;
		}
		htc_acoustic_vir_addr =
			(uint32_t)smem_alloc(SMEM_ID_VENDOR1,
					HTC_ACOUSTIC_TABLE_SIZE);
		if (!htc_acoustic_vir_addr) {
			E("open failed: smem_alloc error\n");
			goto done;
		}
	}

	rc = 0;
done:
	mutex_unlock(&api_lock);
	return rc;
}

static int acoustic_release(struct inode *inode, struct file *file)
{
	D("release\n");
	return 0;
}

static long acoustic_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	int rc, reply_value;

	D("ioctl\n");

	mutex_lock(&api_lock);

	switch (cmd) {
	case ACOUSTIC_UPDATE_ADIE: {
		struct update_adie_req {
			struct rpc_request_hdr hdr;
			int id;
		} adie_req;

		struct update_adie_rep {
			struct rpc_reply_hdr hdr;
			int ret;
		} adie_rep;

		D("ioctl: ACOUSTIC_UPDATE_ADIE called %d.\n", current->pid);

		adie_req.id = cpu_to_be32(-1); /* update all codecs */
		rc = msm_rpc_call_reply(endpoint,
					ONCRPC_UPDATE_ADIE_PROC, &adie_req,
					sizeof(adie_req), &adie_rep,
					sizeof(adie_rep), 5 * HZ);

		reply_value = be32_to_cpu(adie_rep.ret);
		if (reply_value != 0 || rc < 0) {
			E("ioctl failed: ONCRPC_UPDATE_ADIE_PROC "\
				"error %d.\n", rc);
			if (rc >= 0)
				rc = -EIO;
			break;
		}
		D("ioctl: ONCRPC_UPDATE_ADIE_PROC success.\n");
		break;
	}
	default:
		E("ioctl: invalid command\n");
		rc = -EINVAL;
	}

	mutex_unlock(&api_lock);
	return rc;
}

struct rpc_set_uplink_mute_args {
	int mute;
};

static struct file_operations acoustic_fops = {
	.owner = THIS_MODULE,
	.open = acoustic_open,
	.release = acoustic_release,
	.mmap = acoustic_mmap,
	.unlocked_ioctl = acoustic_ioctl,
};

static struct miscdevice acoustic_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "htc-acoustic",
	.fops = &acoustic_fops,
};

static int __init acoustic_init(void)
{
	mutex_init(&api_lock);
	mutex_init(&rpc_connect_lock);
	return misc_register(&acoustic_misc);
}

static void __exit acoustic_exit(void)
{
	misc_deregister(&acoustic_misc);
}

module_init(acoustic_init);
module_exit(acoustic_exit);

