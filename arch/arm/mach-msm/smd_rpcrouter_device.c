/* arch/arm/mach-msm/smd_rpcrouter_device.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2011, The Linux Foundation. All rights reserved.
 * Author: San Mehat <san@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/msm_rpcrouter.h>

#include <asm/uaccess.h>
#include <asm/byteorder.h>

#include <mach/peripheral-loader.h>
#include "smd_rpcrouter.h"

/* Support 64KB of data plus some space for headers */
#define SAFETY_MEM_SIZE (65536 + sizeof(struct rpc_request_hdr))

/* modem load timeout */
#define MODEM_LOAD_TIMEOUT (10 * HZ)

/* Next minor # available for a remote server */
static int next_minor = 1;
static DEFINE_SPINLOCK(server_cdev_lock);

struct class *msm_rpcrouter_class;
dev_t msm_rpcrouter_devno;

static struct cdev rpcrouter_cdev;
static struct device *rpcrouter_device;

struct rpcrouter_file_info {
	struct msm_rpc_endpoint *ept;
	void *modem_pil;
};

static void msm_rpcrouter_unload_modem(void *pil)
{
	if (pil)
		pil_put(pil);
}

static void *msm_rpcrouter_load_modem(void)
{
	void *pil;
	int rc;

	pil = pil_get("modem");
	if (IS_ERR(pil))
		pr_err("%s: modem load failed\n", __func__);
	else {
		rc = wait_for_completion_interruptible_timeout(
						&rpc_remote_router_up,
						MODEM_LOAD_TIMEOUT);
		if (!rc)
			rc = -ETIMEDOUT;
		if (rc < 0) {
			pr_err("%s: wait for remote router failed %d\n",
			       __func__, rc);
			msm_rpcrouter_unload_modem(pil);
			pil = ERR_PTR(rc);
		}
	}

	return pil;
}

static int rpcrouter_open(struct inode *inode, struct file *filp)
{
	int rc;
	void *pil;
	struct msm_rpc_endpoint *ept;
	struct rpcrouter_file_info *file_info;

	rc = nonseekable_open(inode, filp);
	if (rc < 0)
		return rc;

	file_info = kzalloc(sizeof(*file_info), GFP_KERNEL);
	if (!file_info)
		return -ENOMEM;

	ept = msm_rpcrouter_create_local_endpoint(inode->i_rdev);
	if (!ept) {
		kfree(file_info);
		return -ENOMEM;
	}
	file_info->ept = ept;

	/* if router device, load the modem */
	if (inode->i_rdev == msm_rpcrouter_devno) {
		pil = msm_rpcrouter_load_modem();
		if (IS_ERR(pil)) {
			kfree(file_info);
			msm_rpcrouter_destroy_local_endpoint(ept);
			return PTR_ERR(pil);
		}
		file_info->modem_pil = pil;
	}

	filp->private_data = file_info;
	return 0;
}

static int rpcrouter_release(struct inode *inode, struct file *filp)
{
	struct rpcrouter_file_info *file_info = filp->private_data;
	struct msm_rpc_endpoint *ept;
	static unsigned int rpcrouter_release_cnt;

	ept = (struct msm_rpc_endpoint *) file_info->ept;

	/* A user program with many files open when ends abruptly,
	 * will cause a flood of REMOVE_CLIENT messages to the
	 * remote processor.  This will cause remote processors
	 * internal queue to overflow. Inserting a sleep here
	 * regularly is the effecient option.
	 */
	if (rpcrouter_release_cnt++ % 2)
		msleep(1);

	/* if router device, unload the modem */
	if (inode->i_rdev == msm_rpcrouter_devno)
		msm_rpcrouter_unload_modem(file_info->modem_pil);

	kfree(file_info);
	return msm_rpcrouter_destroy_local_endpoint(ept);
}

static ssize_t rpcrouter_read(struct file *filp, char __user *buf,
			      size_t count, loff_t *ppos)
{
	struct rpcrouter_file_info *file_info = filp->private_data;
	struct msm_rpc_endpoint *ept;
	struct rr_fragment *frag, *next;
	int rc;

	ept = (struct msm_rpc_endpoint *) file_info->ept;

	rc = __msm_rpc_read(ept, &frag, count, -1);
	if (rc < 0)
		return rc;

	count = rc;

	while (frag != NULL) {		
		if (copy_to_user(buf, frag->data, frag->length)) {
			printk(KERN_ERR
			       "rpcrouter: could not copy all read data to user!\n");
			rc = -EFAULT;
		}
		buf += frag->length;
		next = frag->next;
		kfree(frag);
		frag = next;
	}

	return rc;
}

static ssize_t rpcrouter_write(struct file *filp, const char __user *buf,
				size_t count, loff_t *ppos)
{
	struct rpcrouter_file_info *file_info = filp->private_data;
	struct msm_rpc_endpoint	*ept;
	int rc = 0;
	void *k_buffer;

	ept = (struct msm_rpc_endpoint *) file_info->ept;

	/* A check for safety, this seems non-standard */
	if (count > SAFETY_MEM_SIZE)
		return -EINVAL;

	k_buffer = kmalloc(count, GFP_KERNEL);
	if (!k_buffer)
		return -ENOMEM;

	if (copy_from_user(k_buffer, buf, count)) {
		rc = -EFAULT;
		goto write_out_free;
	}

	rc = msm_rpc_write(ept, k_buffer, count);
	if (rc < 0)
		goto write_out_free;

	rc = count;
write_out_free:
	kfree(k_buffer);
	return rc;
}

/* RPC VFS Poll Implementation
 *
 * POLLRDHUP - restart in progress
 * POLLOUT - writes accepted (without blocking)
 * POLLIN - data ready to read
 *
 * The restart state consists of several different phases including a client
 * notification and a server restart.  If the server has been restarted, then
 * reads and writes can be performed and the POLLOUT bit will be set.  If a
 * restart is in progress, but the server hasn't been restarted, then only the
 * POLLRDHUP is active and reads and writes will block.  See the table
 * below for a summary.  POLLRDHUP is cleared once a call to msm_rpc_write_pkt
 * or msm_rpc_read_pkt returns ENETRESET.
 *
 * POLLOUT	POLLRDHUP
 *    1         0       Normal operation
 *    0         1       Restart in progress and server hasn't restarted yet
 *    1         1       Server has been restarted, but client has
 *                      not been notified of a restart by a return code
 *                      of ENETRESET from msm_rpc_write_pkt or
 *                      msm_rpc_read_pkt.
 */
static unsigned int rpcrouter_poll(struct file *filp,
				   struct poll_table_struct *wait)
{
	struct rpcrouter_file_info *file_info = filp->private_data;
	struct msm_rpc_endpoint *ept;
	unsigned mask = 0;

	ept = (struct msm_rpc_endpoint *) file_info->ept;

	poll_wait(filp, &ept->wait_q, wait);
	poll_wait(filp, &ept->restart_wait, wait);

	if (!list_empty(&ept->read_q))
		mask |= POLLIN;
	if (!(ept->restart_state & RESTART_PEND_SVR))
		mask |= POLLOUT;
	if (ept->restart_state != 0)
		mask |= POLLRDHUP;

	return mask;
}

static long rpcrouter_ioctl(struct file *filp, unsigned int cmd,
			    unsigned long arg)
{
	struct rpcrouter_file_info *file_info = filp->private_data;
	struct msm_rpc_endpoint *ept;
	struct rpcrouter_ioctl_server_args server_args;
	int rc = 0;
	uint32_t n;

	ept = (struct msm_rpc_endpoint *) file_info->ept;
	switch (cmd) {

	case RPC_ROUTER_IOCTL_GET_VERSION:
		n = RPC_ROUTER_VERSION_V1;
		rc = put_user(n, (unsigned int *) arg);
		break;

	case RPC_ROUTER_IOCTL_GET_MTU:
		/* the pacmark word reduces the actual payload
		 * possible per message
		 */
		n = RPCROUTER_MSGSIZE_MAX - sizeof(uint32_t);
		rc = put_user(n, (unsigned int *) arg);
		break;

	case RPC_ROUTER_IOCTL_REGISTER_SERVER:
		rc = copy_from_user(&server_args, (void *) arg,
				    sizeof(server_args));
		if (rc < 0)
			break;
		msm_rpc_register_server(ept,
					server_args.prog,
					server_args.vers);
		break;

	case RPC_ROUTER_IOCTL_UNREGISTER_SERVER:
		rc = copy_from_user(&server_args, (void *) arg,
				    sizeof(server_args));
		if (rc < 0)
			break;

		msm_rpc_unregister_server(ept,
					  server_args.prog,
					  server_args.vers);
		break;

	case RPC_ROUTER_IOCTL_CLEAR_NETRESET:
		msm_rpc_clear_netreset(ept);
		break;

	case RPC_ROUTER_IOCTL_GET_CURR_PKT_SIZE:
		rc = msm_rpc_get_curr_pkt_size(ept);
		break;

	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static struct file_operations rpcrouter_server_fops = {
	.owner	 = THIS_MODULE,
	.open	 = rpcrouter_open,
	.release = rpcrouter_release,
	.read	 = rpcrouter_read,
	.write	 = rpcrouter_write,
	.poll    = rpcrouter_poll,
	.unlocked_ioctl	 = rpcrouter_ioctl,
};

static struct file_operations rpcrouter_router_fops = {
	.owner	 = THIS_MODULE,
	.open	 = rpcrouter_open,
	.release = rpcrouter_release,
	.read	 = rpcrouter_read,
	.write	 = rpcrouter_write,
	.poll    = rpcrouter_poll,
	.unlocked_ioctl = rpcrouter_ioctl,
};

int msm_rpcrouter_create_server_cdev(struct rr_server *server)
{
	int rc;
	uint32_t dev_vers;
	unsigned long flags;

	spin_lock_irqsave(&server_cdev_lock, flags);
	if (next_minor == RPCROUTER_MAX_REMOTE_SERVERS) {
		spin_unlock_irqrestore(&server_cdev_lock, flags);
		printk(KERN_ERR
		       "rpcrouter: Minor numbers exhausted - Increase "
		       "RPCROUTER_MAX_REMOTE_SERVERS\n");
		return -ENOBUFS;
	}

	/* Servers with bit 31 set are remote msm servers with hashkey version.
	 * Servers with bit 31 not set are remote msm servers with
	 * backwards compatible version type in which case the minor number
	 * (lower 16 bits) is set to zero.
	 *
	 */
	if ((server->vers & 0x80000000))
		dev_vers = server->vers;
	else
		dev_vers = server->vers & 0xffff0000;

	server->device_number =
		MKDEV(MAJOR(msm_rpcrouter_devno), next_minor++);
	spin_unlock_irqrestore(&server_cdev_lock, flags);

	server->device =
		device_create(msm_rpcrouter_class, rpcrouter_device,
			      server->device_number, NULL, "%.8x:%.8x",
			      server->prog, dev_vers);
	if (IS_ERR(server->device)) {
		printk(KERN_ERR
		       "rpcrouter: Unable to create device (%ld)\n",
		       PTR_ERR(server->device));
		return PTR_ERR(server->device);;
	}

	cdev_init(&server->cdev, &rpcrouter_server_fops);
	server->cdev.owner = THIS_MODULE;

	rc = cdev_add(&server->cdev, server->device_number, 1);
	if (rc < 0) {
		printk(KERN_ERR
		       "rpcrouter: Unable to add chrdev (%d)\n", rc);
		device_destroy(msm_rpcrouter_class, server->device_number);
		return rc;
	}
	return 0;
}

/* for backward compatible version type (31st bit cleared)
 * clearing minor number (lower 16 bits) in device name
 * is neccessary for driver binding
 */
int msm_rpcrouter_create_server_pdev(struct rr_server *server)
{
	server->p_device.base.id = (server->vers & RPC_VERSION_MODE_MASK) ?
				   server->vers :
				   (server->vers & RPC_VERSION_MAJOR_MASK);
	server->p_device.base.name = server->pdev_name;

	server->p_device.prog = server->prog;
	server->p_device.vers = server->vers;

	platform_device_register(&server->p_device.base);
	return 0;
}

int msm_rpcrouter_init_devices(void)
{
	int rc;
	int major;

	/* Create the device nodes */
	msm_rpcrouter_class = class_create(THIS_MODULE, "oncrpc");
	if (IS_ERR(msm_rpcrouter_class)) {
		rc = -ENOMEM;
		printk(KERN_ERR
		       "rpcrouter: failed to create oncrpc class\n");
		goto fail;
	}

	rc = alloc_chrdev_region(&msm_rpcrouter_devno, 0,
				 RPCROUTER_MAX_REMOTE_SERVERS + 1,
				 "oncrpc");
	if (rc < 0) {
		printk(KERN_ERR
		       "rpcrouter: Failed to alloc chardev region (%d)\n", rc);
		goto fail_destroy_class;
	}

	major = MAJOR(msm_rpcrouter_devno);
	rpcrouter_device = device_create(msm_rpcrouter_class, NULL,
					 msm_rpcrouter_devno, NULL, "%.8x:%d",
					 0, 0);
	if (IS_ERR(rpcrouter_device)) {
		rc = -ENOMEM;
		goto fail_unregister_cdev_region;
	}

	cdev_init(&rpcrouter_cdev, &rpcrouter_router_fops);
	rpcrouter_cdev.owner = THIS_MODULE;

	rc = cdev_add(&rpcrouter_cdev, msm_rpcrouter_devno, 1);
	if (rc < 0)
		goto fail_destroy_device;

	return 0;

fail_destroy_device:
	device_destroy(msm_rpcrouter_class, msm_rpcrouter_devno);
fail_unregister_cdev_region:
	unregister_chrdev_region(msm_rpcrouter_devno,
				 RPCROUTER_MAX_REMOTE_SERVERS + 1);
fail_destroy_class:
	class_destroy(msm_rpcrouter_class);
fail:
	return rc;
}

void msm_rpcrouter_exit_devices(void)
{
	cdev_del(&rpcrouter_cdev);
	device_destroy(msm_rpcrouter_class, msm_rpcrouter_devno);
	unregister_chrdev_region(msm_rpcrouter_devno,
				 RPCROUTER_MAX_REMOTE_SERVERS + 1);
	class_destroy(msm_rpcrouter_class);
}

