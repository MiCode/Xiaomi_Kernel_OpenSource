/* arch/arm/mach-msm/htc_acoustic.c
 *
 * Copyright (C) 2007-2008 HTC Corporation
 * Author: Laurence Chen <Laurence_Chen@htc.com>
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
#include <linux/sched.h>

#include <mach/msm_smd.h>
#include <mach/msm_rpcrouter.h>
#include <mach/msm_iomap.h>

#include "smd_private.h"

#define ACOUSTIC_IOCTL_MAGIC 'p'
#define ACOUSTIC_ARM11_DONE	_IOW(ACOUSTIC_IOCTL_MAGIC, 22, unsigned int)

#define HTCRPOG 0x30100002
#define HTCVERS 0
#define ONCRPC_SET_MIC_BIAS_PROC       (1)
#define ONCRPC_ACOUSTIC_INIT_PROC      (5)
#define ONCRPC_ALLOC_ACOUSTIC_MEM_PROC (6)

#define HTC_ACOUSTIC_TABLE_SIZE        (0x10000)

#define D(fmt, args...) printk(KERN_INFO "htc-acoustic: "fmt, ##args)
#define E(fmt, args...) printk(KERN_ERR "htc-acoustic: "fmt, ##args)

struct set_smem_req {
	struct rpc_request_hdr hdr;
	uint32_t size;
};

struct set_smem_rep {
	struct rpc_reply_hdr hdr;
	int n;
};

struct set_acoustic_req {
	struct rpc_request_hdr hdr;
};

struct set_acoustic_rep {
	struct rpc_reply_hdr hdr;
	int n;
};

static uint32_t htc_acoustic_vir_addr;
static struct msm_rpc_endpoint *endpoint;
static struct mutex api_lock;

static int acoustic_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long pgoff, delta;
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
	delta = PAGE_ALIGN(pgoff) - pgoff;

	if (size + delta > HTC_ACOUSTIC_TABLE_SIZE) {
		E("mmap failed: size %d\n", size);
		goto done;
	}

	pgoff += delta;
	vma->vm_flags |= VM_IO | VM_RESERVED;

	rc = io_remap_pfn_range(vma, vma->vm_start, pgoff >> PAGE_SHIFT,
		      size, vma->vm_page_prot);

	if (rc < 0)
		E("mmap failed: remap error %d\n", rc);

done:	mutex_unlock(&api_lock);
	return rc;
}

static int acoustic_open(struct inode *inode, struct file *file)
{
	int rc = -EIO;
	struct set_smem_req req_smem;
	struct set_smem_rep rep_smem;

	D("open\n");

	mutex_lock(&api_lock);

	if (!htc_acoustic_vir_addr) {
		if (endpoint == NULL) {
			endpoint = msm_rpc_connect(HTCRPOG, HTCVERS, 0);
			if (IS_ERR(endpoint)) {
				E("init rpc failed! rc = %ld\n",
					PTR_ERR(endpoint));
				endpoint = NULL;
				goto done;
			}
		}

		req_smem.size = cpu_to_be32(HTC_ACOUSTIC_TABLE_SIZE);
		rc = msm_rpc_call_reply(endpoint,
					ONCRPC_ALLOC_ACOUSTIC_MEM_PROC,
					&req_smem, sizeof(req_smem),
					&rep_smem, sizeof(rep_smem),
					5 * HZ);

		if (rep_smem.n != 0 || rc < 0) {
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
	struct set_acoustic_req req;
	struct set_acoustic_rep rep;

	D("ioctl\n");

	mutex_lock(&api_lock);

	switch (cmd) {
	case ACOUSTIC_ARM11_DONE:
		D("ioctl: ACOUSTIC_ARM11_DONE called %d.\n", current->pid);
		rc = msm_rpc_call_reply(endpoint,
					ONCRPC_ACOUSTIC_INIT_PROC, &req,
					sizeof(req), &rep, sizeof(rep),
					5 * HZ);

		reply_value = be32_to_cpu(rep.n);
		if (reply_value != 0 || rc < 0) {
			E("ioctl failed: ONCRPC_ACOUSTIC_INIT_PROC "\
				"error %d.\n", rc);
			if (rc >= 0)
				rc = -EIO;
			break;
		}
		D("ioctl: ONCRPC_ACOUSTIC_INIT_PROC success.\n");
		break;
	default:
		E("ioctl: invalid command\n");
		rc = -EINVAL;
	}

	mutex_unlock(&api_lock);
	return 0;
}


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
	return misc_register(&acoustic_misc);
}

static void __exit acoustic_exit(void)
{
	misc_deregister(&acoustic_misc);
}

module_init(acoustic_init);
module_exit(acoustic_exit);

MODULE_AUTHOR("Laurence Chen <Laurence_Chen@htc.com>");
MODULE_DESCRIPTION("HTC acoustic driver");
MODULE_LICENSE("GPL");
