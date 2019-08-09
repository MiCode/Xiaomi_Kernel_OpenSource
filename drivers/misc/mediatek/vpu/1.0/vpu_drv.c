/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>

#include <linux/slab.h>

#include <linux/delay.h>
#include <linux/uaccess.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>

#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#ifdef CONFIG_MTK_M4U
#include <m4u.h>
#else
#include "mach/mt_iommu.h"
#endif
#include "vpu_drv.h"
#include "vpu_cmn.h"


/****************************************************************************/

#define VPU_DEV_NAME            "vpu"

static struct vpu_device *vpu_device;
static struct wakeup_source vpu_wake_lock;

static int vpu_probe(struct platform_device *dev);

static int vpu_remove(struct platform_device *dev);

static int vpu_suspend(struct platform_device *dev, pm_message_t mesg);

static int vpu_resume(struct platform_device *dev);

/*---------------------------------------------------------------------------*/
/* VPU Driver: pm operations                                                 */
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
int vpu_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return vpu_suspend(pdev, PMSG_SUSPEND);
}

int vpu_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return vpu_resume(pdev);
}

int vpu_pm_restore_noirq(struct device *device)
{
	return 0;
}
#else
#define vpu_pm_suspend NULL
#define vpu_pm_resume  NULL
#define vpu_pm_restore_noirq NULL
#endif

static const struct dev_pm_ops vpu_pm_ops = {
	.suspend = vpu_pm_suspend,
	.resume = vpu_pm_resume,
	.freeze = vpu_pm_suspend,
	.thaw = vpu_pm_resume,
	.poweroff = vpu_pm_suspend,
	.restore = vpu_pm_resume,
	.restore_noirq = vpu_pm_restore_noirq,
};


/*---------------------------------------------------------------------------*/
/* VPU Driver: Prototype                                                     */
/*---------------------------------------------------------------------------*/


static const struct of_device_id vpu_of_ids[] = {
	{.compatible = "mediatek,ipu",},
	{}
};

static struct platform_driver vpu_driver = {
	.probe   = vpu_probe,
	.remove  = vpu_remove,
	.suspend = vpu_suspend,
	.resume  = vpu_resume,
	.driver  = {
		.name = VPU_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = vpu_of_ids,
#ifdef CONFIG_PM
		.pm = &vpu_pm_ops,
#endif
	}
};


/*---------------------------------------------------------------------------*/
/* VPU Driver: file operations                                               */
/*---------------------------------------------------------------------------*/
static int vpu_open(struct inode *inode, struct file *flip);

static int vpu_release(struct inode *inode, struct file *flip);

static int vpu_mmap(struct file *flip, struct vm_area_struct *vma);

static long vpu_compat_ioctl(struct file *flip, unsigned int cmd,
			     unsigned long arg);

static long vpu_ioctl(struct file *flip, unsigned int cmd, unsigned long arg);

static const struct file_operations vpu_fops = {
	.owner = THIS_MODULE,
	.open = vpu_open,
	.release = vpu_release,
	.mmap = vpu_mmap,
	.compat_ioctl = vpu_compat_ioctl,
	.unlocked_ioctl = vpu_ioctl
};


/*---------------------------------------------------------------------------*/
/* M4U: fault callback                                                       */
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_MTK_M4U
m4u_callback_ret_t vpu_m4u_fault_callback(int port, unsigned int mva,
					  void *data)
{
	LOG_DBG("[m4u] fault callback: port=%d, mva=0x%x", port, mva);
	return M4U_CALLBACK_HANDLED;
}
#else
enum mtk_iommu_callback_ret_t vpu_m4u_fault_callback(int port, unsigned int mva,
					  void *data)
{
	LOG_DBG("[m4u] fault callback: port=%d, mva=0x%x", port, mva);
	return MTK_IOMMU_CALLBACK_HANDLED;
}
#endif
/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static int vpu_num_users;

int vpu_create_user(struct vpu_user **user)
{
	struct vpu_user *u;

	u = kzalloc(sizeof(vlist_type(struct vpu_user)), GFP_KERNEL);
	if (!u)
		return -ENOMEM;

	mutex_init(&u->data_mutex);
	u->id = ++vpu_num_users;
	u->open_pid = current->pid;
	u->open_tgid = current->tgid;
	INIT_LIST_HEAD(&u->enque_list);
	INIT_LIST_HEAD(&u->deque_list);
	init_waitqueue_head(&u->deque_wait);

	u->power_mode = VPU_POWER_MODE_DYNAMIC;
	u->power_opp = VPU_POWER_OPP_UNREQUEST;

	mutex_lock(&vpu_device->user_mutex);
	list_add_tail(vlist_link(u, struct vpu_user), &vpu_device->user_list);
	mutex_unlock(&vpu_device->user_mutex);

	LOG_DBG("created user[%d]\n", u->id);
	*user = u;
	return 0;
}

static int vpu_renew_power_operation(void)
{
	int ret;
	struct vpu_user *u;
	struct list_head *head;
	uint8_t power_mode = VPU_POWER_MODE_DYNAMIC;
	uint8_t power_opp = VPU_POWER_OPP_UNREQUEST;

	mutex_lock(&vpu_device->user_mutex);
	list_for_each(head, &vpu_device->user_list)
	{
		u = vlist_node_of(head, struct vpu_user);
		if (u->power_mode == VPU_POWER_MODE_ON)
			power_mode = VPU_POWER_MODE_ON;
		if (u->power_opp < power_opp)
			power_opp = u->power_opp;
	}
	mutex_unlock(&vpu_device->user_mutex);

	ret = vpu_change_power_opp(power_opp);
	CHECK_RET("fail to renew power opp:%d\n", power_opp);

	ret = vpu_change_power_mode(power_mode);
	CHECK_RET("fail to renew power mode:%d\n", power_mode);

	LOG_DBG("changed power mode:%d opp:%d\n", power_mode, power_opp);
out:
	return ret;

}
int vpu_set_power(struct vpu_user *user, struct vpu_power *power)
{
	LOG_DBG("set power mode:%d opp:%d, pid=%d, tid=%d\n",
			power->mode, power->opp,
			user->open_pid, user->open_tgid);

	user->power_mode = power->mode;
	user->power_opp = power->opp;

	return vpu_renew_power_operation();
}

static int vpu_write_register(struct vpu_reg_values *regs)
{
	return 0;
}

int vpu_push_request_to_queue(struct vpu_user *user, struct vpu_request *req)
{
	if (!user) {
		LOG_ERR("empty user\n");
		return -EINVAL;
	}

	if (user->deleting) {
		LOG_WRN("push a request while deleting the user\n");
		return -ENONET;
	}

	mutex_lock(&user->data_mutex);
	list_add_tail(vlist_link(req, struct vpu_request), &user->enque_list);
	mutex_unlock(&user->data_mutex);

	wake_up(&vpu_device->req_wait);

	return 0;
}

int vpu_flush_requests_from_queue(struct vpu_user *user)
{
	struct list_head *head, *temp;
	struct vpu_request *req;

	mutex_lock(&user->data_mutex);

	if (!user->running && list_empty(&user->enque_list)) {
		mutex_unlock(&user->data_mutex);
		return 0;
	}

	user->flushing = true;
	mutex_unlock(&user->data_mutex);

	/* the running request will add to the deque before interrupt */
	wait_event_interruptible(user->deque_wait, !user->running);

	while (user->running)
		ndelay(1000);

	mutex_lock(&user->data_mutex);
	/* push the remaining enque to the deque */
	list_for_each_safe(head, temp, &user->enque_list) {
		req = vlist_node_of(head, struct vpu_request);
		req->status = VPU_REQ_STATUS_FLUSH;
		list_del_init(head);
		list_add_tail(head, &user->deque_list);
	}

	user->flushing = false;
	LOG_DBG("flushed queue, user:%d\n", user->id);

	mutex_unlock(&user->data_mutex);

	return 0;
}

int vpu_pop_request_from_queue(struct vpu_user *user, struct vpu_request **rreq)
{
	int ret;
	struct vpu_request *req;

	/* wait until condition is true */
	ret = wait_event_interruptible(
		user->deque_wait,
		!list_empty(&user->deque_list));

	/* ret == -ERESTARTSYS, if signal interrupt */
	if (ret < 0) {
		LOG_ERR("interrupt by signal, while pop a request, ret=%d\n",
			ret);
		*rreq = NULL;
		return -EINTR;
	}

	mutex_lock(&user->data_mutex);
	/* This part should not be happened */
	if (list_empty(&user->deque_list)) {
		mutex_unlock(&user->data_mutex);
		LOG_ERR("pop a request from empty queue! ret=%d\n", ret);
		*rreq = NULL;
		return -ENODATA;
	};

	/* get first node from deque list */
	req = vlist_node_of(user->deque_list.next, struct vpu_request);

	list_del_init(vlist_link(req, struct vpu_request));
	mutex_unlock(&user->data_mutex);

	*rreq = req;
	return 0;
}


int vpu_delete_user(struct vpu_user *user)
{
	struct list_head *head, *temp;
	struct vpu_request *req;

	if (!user) {
		LOG_ERR("delete empty user!\n");
		return -EINVAL;
	}

	user->deleting = true;
	vpu_flush_requests_from_queue(user);

	/* clear the list of deque */
	mutex_lock(&user->data_mutex);
	list_for_each_safe(head, temp, &user->deque_list) {
		req = vlist_node_of(head, struct vpu_request);
		list_del(head);
		vpu_free_request(req);
	}
	mutex_unlock(&user->data_mutex);

	/* confirm the lock has released */
	if (user->locked)
		vpu_hw_unlock(user);

	mutex_lock(&vpu_device->user_mutex);
	list_del(vlist_link(user, struct vpu_user));
	mutex_unlock(&vpu_device->user_mutex);

	vpu_renew_power_operation();

	LOG_DBG("deleted user[%d]\n", user->id);
	kfree(user);

	return 0;
}

int vpu_dump_user(struct seq_file *s)
{
	struct vpu_user *user;
	struct list_head *head_user;
	struct list_head *head_req;
	uint32_t cnt_enq, cnt_deq;

#define LINE_BAR "  +------+------+------+-------+-------+-------+-------+\n"
	vpu_print_seq(s, LINE_BAR);
	vpu_print_seq(s, "  |%-6s|%-6s|%-6s|%-7s|%-7s|%-7s|%-7s|\n", "Id",
		      "Pid", "Tid", "Enque", "Running", "Deque", "Locked");
	vpu_print_seq(s, LINE_BAR);

	mutex_lock(&vpu_device->user_mutex);
	list_for_each(head_user, &vpu_device->user_list)
	{
		user = vlist_node_of(head_user, struct vpu_user);
		cnt_enq = cnt_deq = 0;

		list_for_each(head_req, &user->enque_list)
		{
			cnt_enq++;
		}

		list_for_each(head_req, &user->deque_list)
		{
			cnt_deq++;
		}

		vpu_print_seq(s, "  |%-6d|%-6d|%-6d|%-7d|%-7d|%-7d|%-7d|\n",
			      user->id,
			      user->open_pid,
			      user->open_tgid,
			      cnt_enq,
			      user->running,
			      cnt_deq,
			      user->locked);
		vpu_print_seq(s, LINE_BAR);
	}
	mutex_unlock(&vpu_device->user_mutex);
	vpu_print_seq(s, "\n");

#undef LINE_BAR

	return 0;
}




/*---------------------------------------------------------------------------*/
/* IOCTL: implementation                                                     */
/*---------------------------------------------------------------------------*/

static int vpu_open(struct inode *inode, struct file *flip)
{
	int ret = 0;

	struct vpu_user *user;

	vpu_create_user(&user);
	if (IS_ERR_OR_NULL(user)) {
		LOG_ERR("fail to create user\n");
		return -ENOMEM;
	}

	flip->private_data = user;

	return ret;
}

static long vpu_compat_ioctl(struct file *flip, unsigned int cmd,
			     unsigned long arg)
{
	switch (cmd) {
	case VPU_IOCTL_SET_POWER:
	case VPU_IOCTL_ENQUE_REQUEST:
	case VPU_IOCTL_DEQUE_REQUEST:
	case VPU_IOCTL_GET_ALGO_INFO:
	case VPU_IOCTL_REG_WRITE:
	case VPU_IOCTL_REG_READ:
	case VPU_IOCTL_LOAD_ALG:
	{
		void *ptr = compat_ptr(arg);

		return vpu_ioctl(flip, cmd, (unsigned long) ptr);
	}
	case VPU_IOCTL_LOCK:
	case VPU_IOCTL_UNLOCK:
	default:
		return vpu_ioctl(flip, cmd, arg);
	}
}

static long vpu_ioctl(struct file *flip, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct vpu_user *user = flip->private_data;

	switch (cmd) {
	case VPU_IOCTL_SET_POWER:
	{
		struct vpu_power power;

		ret = copy_from_user(&power, (void *)arg,
				     sizeof(struct vpu_power));
		CHECK_RET("[SET_POWER] copy 'struct power' failed, ret=%d\n",
			       ret);

		ret = vpu_set_power(user, &power);
		CHECK_RET("[SET_POWER] set power failed, ret=%d\n", ret);

		break;
	}
	case VPU_IOCTL_ENQUE_REQUEST:
	{
		struct vpu_request *req;
		struct vpu_request *u_req;

		ret = vpu_alloc_request(&req);
		CHECK_RET("[ENQUE alloc request failed, ret=%d\n", ret);

		u_req = (struct vpu_request *) arg;
		ret = get_user(req->algo_id, &u_req->algo_id);
		ret |= get_user(req->status, &u_req->status);
		ret |= get_user(req->buffer_count, &u_req->buffer_count);
		ret |= get_user(req->sett_ptr, &u_req->sett_ptr);
		ret |= get_user(req->sett_length, &u_req->sett_length);
		ret |= get_user(req->priv, &u_req->priv);

		if (ret)
			LOG_ERR("[ENQUE] get params failed, ret=%d\n", ret);
		else if (req->buffer_count > VPU_MAX_NUM_PORTS)
			LOG_ERR("[ENQUE] wrong buffer count, count=%d\n",
				req->buffer_count);
		else if (copy_from_user(req->buffers, u_req->buffers,
					req->buffer_count *
						sizeof(struct vpu_buffer)))
			LOG_ERR("[ENQUE] copy 'struct buffer' failed, ret=%d\n",
				ret);
		else if (vpu_push_request_to_queue(user, req))
			LOG_ERR("[ENQUE] push to user's queue failed, ret=%d\n",
				ret);
		else
			break;

		/* free the request, error happened here*/
		vpu_free_request(req);
		ret = -EFAULT;
		break;
	}
	case VPU_IOCTL_DEQUE_REQUEST:
	{
		struct vpu_request *req;
		struct vpu_request *u_req;

		u_req = (struct vpu_request *) arg;

		ret = vpu_pop_request_from_queue(user, &req);
		CHECK_RET("[DEQUE] pop request failed, ret=%d\n", ret);

		ret = put_user(req->status, &u_req->status);
		if (ret)
			LOG_ERR("[DEQUE] update status failed, ret=%d\n", ret);

		ret = vpu_free_request(req);
		CHECK_RET("[DEQUE] free request, ret=%d\n", ret);

		break;
	}
	case VPU_IOCTL_FLUSH_REQUEST:
	{
		ret = vpu_flush_requests_from_queue(user);
		CHECK_RET("[FLUSH] flush request failed, ret=%d\n", ret);

		break;
	}
	case VPU_IOCTL_GET_ALGO_INFO:
	{
		char name[VPU_NAME_SIZE];
		struct vpu_algo *algo;
		struct vpu_algo *u_algo;
		uint64_t u_info_ptr;
		uint32_t u_info_length;

		u_algo = (struct vpu_algo *)arg;
		ret = copy_from_user(name, u_algo->name,
				     sizeof(char[VPU_NAME_SIZE]));
		CHECK_RET("[GET_ALGO] copy 'name' failed, ret=%d\n", ret);
		name[VPU_NAME_SIZE - 1] = '\0';

		/* 1. find algo by name */
		ret = vpu_find_algo_by_name(name, &algo);
		CHECK_RET("[GET_ALGO] can not find algo, name=%s\n", name);

		/* 2. write data to user */
		/* 2-1. write port */
		ret = put_user(algo->port_count, &u_algo->port_count);
		ret |= copy_to_user(u_algo->ports, algo->ports,
				sizeof(struct vpu_port) * algo->port_count);
		CHECK_RET("[GET_ALGO] update ports failed, ret=%d\n", ret);

		/* 2-2. write id */
		ret = put_user(algo->id, &u_algo->id);
		CHECK_RET("[GET_ALGO] update id failed, ret=%d\n", ret);

		/* 2-3. write setting desc */
		ret = put_user(algo->sett_desc_count, &u_algo->sett_desc_count);
		ret |= copy_to_user(u_algo->sett_descs, algo->sett_descs,
				    algo->sett_desc_count *
					    sizeof(struct vpu_prop_desc));
		CHECK_RET("[GET_ALGO] update setting desc failed, ret=%d\n",
			  ret);

		/* 2-4. write info desc */
		ret = put_user(algo->info_desc_count, &u_algo->info_desc_count);
		ret |= copy_to_user(u_algo->info_descs, algo->info_descs,
				    algo->info_desc_count *
					    sizeof(struct vpu_prop_desc));
		CHECK_RET("[GET_ALGO] update info desc failed, ret=%d\n", ret);

		/* 2-5. write info data */
		ret = get_user(u_info_ptr, &u_algo->info_ptr);
		ret |= get_user(u_info_length, &u_algo->info_length);
		CHECK_RET("[GET_ALGO] get info ptr/length failed, ret=%d\n",
			  ret);
		ret = (u_info_length < algo->info_length) ? -EINVAL : 0;
		CHECK_RET("[GET_ALGO] the size of info data is not enough!");
		ret = copy_to_user((void *)(u_info_ptr), (void *)algo->info_ptr,
				   algo->info_length);
		CHECK_RET("[GET_ALGO] update info data failed, ret=%d\n", ret);

		break;
	}
	case VPU_IOCTL_REG_WRITE: {
		struct vpu_reg_values regs;

		ret = copy_from_user(&regs, (void *)arg,
				     sizeof(struct vpu_reg_values));
		CHECK_RET("[REG] copy 'struct reg' failed,%d\n", ret);

		ret = vpu_write_register(&regs);
		CHECK_RET("[REG] write reg failed,%d\n", ret);

		break;
	}
	case VPU_IOCTL_LOCK:
	{
		vpu_hw_lock(user);
		break;
	}
	case VPU_IOCTL_UNLOCK:
	{
		vpu_hw_unlock(user);
		break;
	}
	case VPU_IOCTL_LOAD_ALG:
	{
		vpu_id_t id;
		struct vpu_algo *algo;

		if (!user->locked)
			ret = -ENOTBLK;
		CHECK_RET("[LOAD_ALGO] should lock device");

		ret = copy_from_user(&id, (void *) arg, sizeof(vpu_id_t));
		CHECK_RET("[LOAD_ALGO] copy_from_user failed, ret=%d\n", ret);

		ret = vpu_find_algo_by_id(id, &algo);
		CHECK_RET("[LOAD_ALGO] can not find algo, id=%d\n", id);

		ret = vpu_hw_load_algo(algo);
		CHECK_RET("[LOAD_ALGO] fail to load kernel, id=%d\n", id);

		break;
	}
	default:
		LOG_WRN("ioctl: no such command!\n");
		ret = -EINVAL;
		break;
	}

out:
	if (ret) {
		LOG_ERR("fail, cmd_pid(%d_%d),(process_pid_tgid)=(%s_%d_%d)\n",
			    cmd, user->open_pid, current->comm, current->pid,
			    current->tgid);
	}

	return ret;
}

static int vpu_release(struct inode *inode, struct file *flip)
{
	struct vpu_user *user = flip->private_data;

	vpu_delete_user(user);

	return 0;
}


/*******************************************************************
 *******************************************************************
 */
static int vpu_mmap(struct file *flip, struct vm_area_struct *vma)
{
	unsigned long length = 0;
	unsigned int pfn = 0x0;

	length = (vma->vm_end - vma->vm_start);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	pfn = vma->vm_pgoff << PAGE_SHIFT;

	LOG_INF("vpu_mmap: vm_pgoff(0x%lx),pfn(0x%x),phy(0x%lx)",
			vma->vm_pgoff, pfn, vma->vm_pgoff << PAGE_SHIFT);
	LOG_INF("vpu_mmap: vm_start(0x%lx), vm_end(0x%lx), length(0x%lx)\n",
			vma->vm_start, vma->vm_end, length);

	switch (pfn) {

	default:
		LOG_ERR("illegal hw addr for mmap!\n");
		return -EAGAIN;
	}
}


/********************************************************************
 ********************************************************************
 */
static dev_t vpu_devt;
static struct cdev *vpu_chardev;
static struct class *vpu_class;
static int vpu_num_devs;

static inline void vpu_unreg_chardev(void)
{
	/* Release char driver */
	if (vpu_chardev != NULL) {
		cdev_del(vpu_chardev);
		vpu_chardev = NULL;
	}
	unregister_chrdev_region(vpu_devt, 1);
}

static inline int vpu_reg_chardev(void)
{
	int ret = 0;

	ret = alloc_chrdev_region(&vpu_devt, 0, 1, VPU_DEV_NAME);
	if ((ret) < 0) {
		LOG_ERR("alloc_chrdev_region failed, %d\n", ret);
		return ret;
	}
	/* Allocate driver */
	vpu_chardev = cdev_alloc();
	if (vpu_chardev == NULL) {
		LOG_ERR("cdev_alloc failed\n");
		ret = -ENOMEM;
		goto out;
	}

	/* Attatch file operation. */
	cdev_init(vpu_chardev, &vpu_fops);

	vpu_chardev->owner = THIS_MODULE;

	/* Add to system */
	ret = cdev_add(vpu_chardev, vpu_devt, 1);
	if ((ret) < 0) {
		LOG_ERR("Attatch file operation failed, %d\n", ret);
		goto out;
	}

out:
	if (ret < 0)
		vpu_unreg_chardev();

	return ret;
}

/*****************************************************************/
/* platform_driver */
/*****************************************************************/

static int vpu_probe(struct platform_device *pdev)
{
	int ret = 0;

	struct device *dev;
	struct device_node *node;

	node = pdev->dev.of_node;
	vpu_device->dev = &pdev->dev;
	LOG_INF("probe 0, pdev id = %d name = %s\n", pdev->id, pdev->name);

#ifdef MTK_VPU_EMULATOR
	/* emulator will fill vpu_base and bin_base */
	vpu_init_emulator(vpu_device);
#else
	/* get register address */
	vpu_device->vpu_base = (unsigned long) of_iomap(node, 0);
	/* get physical address of binary data loaded by LK */
	{
		uint32_t phy_addr;
		uint32_t phy_size;

		if (of_property_read_u32(node, "bin-phy-addr", &phy_addr) ||
			of_property_read_u32(node, "bin-size", &phy_size)) {
			LOG_ERR("fail to get PhyAddress of vpu binary!\n");
			return -ENODEV;
		}

		LOG_INF("probe 1, phy_addr: 0x%x, phy_size: 0x%x\n",
				phy_addr, phy_size);
		vpu_device->bin_base = (uint64_t)ioremap_wc(phy_addr, phy_size);
		vpu_device->bin_pa = phy_addr;
		vpu_device->bin_size = phy_size;
	}
#endif

	vpu_device->irq_num = irq_of_parse_and_map(node, 0);
	LOG_INF("probe2, vpu_base_0x%lx, bin_base_0x%lx, irq_num,pdev: %d_%p\n",
		 vpu_device->vpu_base,  vpu_device->bin_base,
		 vpu_device->irq_num, vpu_device->dev);

	vpu_init_algo(vpu_device);
	vpu_init_debug(vpu_device);
	vpu_init_hw(vpu_device);
	vpu_init_reg(vpu_device);

	/* Only register char driver in the 1st time */
	if (++vpu_num_devs == 1) {
		/* Register char driver */
		ret = vpu_reg_chardev();
		if (ret) {
			dev_err(vpu_device->dev, "register char failed");
			return ret;
		}

		/* Create class register */
		vpu_class = class_create(THIS_MODULE, "vpudrv");
		if (IS_ERR(vpu_class)) {
			ret = PTR_ERR(vpu_class);
			LOG_ERR("Unable to create class, err = %d\n", ret);
			goto out;
		}

		dev = device_create(vpu_class, NULL, vpu_devt, NULL,
				    VPU_DEV_NAME);
		if (IS_ERR(dev)) {
			ret = PTR_ERR(dev);
			dev_err(vpu_device->dev,
				"Failed to create device: /dev/%s, err = %d",
				VPU_DEV_NAME, ret);
			goto out;
		}

		wakeup_source_init(&vpu_wake_lock, "vpu_lock_wakelock");

out:
		if (ret < 0)
			vpu_unreg_chardev();
	}

	LOG_INF("probe vpu driver\n");
	return ret;
}


static int vpu_remove(struct platform_device *pDev)
{
	/*    struct resource *pRes;*/
	int irq_num;

	/*  */
	LOG_DBG("remove vpu driver");
	/* unregister char driver. */
	vpu_unreg_chardev();

	/* Release IRQ */
	disable_irq(vpu_device->irq_num);
	irq_num = platform_get_irq(pDev, 0);
	free_irq(irq_num, (void *) vpu_chardev);

	device_destroy(vpu_class, vpu_devt);
	class_destroy(vpu_class);
	vpu_class = NULL;
	return 0;
}

static int vpu_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int vpu_resume(struct platform_device *pdev)
{
	return 0;
}

/*******************************************************************************
 *
 */
static int __init VPU_INIT(void)
{
	int ret = 0;

	vpu_device = kzalloc(sizeof(struct vpu_device), GFP_KERNEL);

	INIT_LIST_HEAD(&vpu_device->user_list);
	mutex_init(&vpu_device->user_mutex);
	init_waitqueue_head(&vpu_device->req_wait);

	/* Register M4U callback */
	LOG_DBG("register m4u callback");
#ifdef CONFIG_MTK_M4U
	m4u_register_fault_callback(VPU_PORT_OF_IOMMU, vpu_m4u_fault_callback,
				    NULL);
#else
	mtk_iommu_register_fault_callback(VPU_PORT_OF_IOMMU,
					  vpu_m4u_fault_callback,
					  NULL);
#endif
	LOG_DBG("platform_driver_register start\n");
	if (platform_driver_register(&vpu_driver)) {
		LOG_ERR("failed to register VPU driver");
		return -ENODEV;
	}
	LOG_DBG("platform_driver_register finsish\n");

	return ret;
}


static void __exit VPU_EXIT(void)
{
	platform_driver_unregister(&vpu_driver);

	kfree(vpu_device);
	/* Un-Register M4U callback */
	LOG_DBG("un-register m4u callback");
#ifdef CONFIG_MTK_M4U
	m4u_unregister_fault_callback(VPU_PORT_OF_IOMMU);
#else
	mtk_iommu_unregister_fault_callback(VPU_PORT_OF_IOMMU);
#endif
}


/*******************************************************************************
 *
 */
module_init(VPU_INIT);
module_exit(VPU_EXIT);
MODULE_DESCRIPTION("MTK VPU Driver");
MODULE_AUTHOR("SW6");
MODULE_LICENSE("GPL");
