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
#include <linux/spinlock.h>

#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/mm.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/printk.h>

#include <mt-plat/sync_write.h>

#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include "i2c-mtk.h"



#include "mtk_ion.h"

#include "ion_drv.h"

#include <linux/iommu.h>


#ifdef CONFIG_MTK_IOMMU
#include "mtk_iommu.h"
#include <dt-bindings/memory/mt6763-larb-port.h>
#else
#include "m4u.h"
#endif


#include <linux/clk.h>

#include "ccu_drv.h"
#include "ccu_cmn.h"
#include "ccu_reg.h"
#include "ccu_n3d_a.h"
#include "ccu_i2c.h"
#include "ccu_i2c_hw.h"

#define CCU_DEV_NAME            "ccu"

struct clk *ccu_clock_ctrl;

struct ccu_device_s *g_ccu_device;
static struct ccu_power_s power;
static struct ccu_platform_info g_ccu_platform_info;

static wait_queue_head_t wait_queue_deque;
static wait_queue_head_t wait_queue_enque;

#ifdef CONFIG_PM_WAKELOCKS
struct wakeup_source ccu_wake_lock;
#else
struct wake_lock ccu_wake_lock;
#endif

static irqreturn_t ccu_isr_callback_xxx(int rrq, void *device_id);

typedef irqreturn_t(*ccu_isr_fp_t) (int, void *);

struct ccu_isr_callback_s {
	ccu_isr_fp_t irq_fp;
	unsigned int int_number;
	char device_name[16];
};

/* int number is got from kernel api */
const struct ccu_isr_callback_s
	ccu_isr_callbacks[CCU_IRQ_NUM_TYPES] = {
	/* The last used be mapping to device node.
	 * Must be the same name with that in device node.
	 */
	{ccu_isr_callback_xxx, 0, "ccu2"}
};

static irqreturn_t ccu_isr_callback_xxx(int irq, void *device_id)
{
	LOG_DBG("%s:0x%x\n", __func__, irq);
	return IRQ_HANDLED;
}


static int ccu_probe(struct platform_device *dev);

static int ccu_remove(struct platform_device *dev);

static int ccu_suspend(struct platform_device *dev, pm_message_t mesg);

static int ccu_resume(struct platform_device *dev);

/*---------------------------------------------------------------------------*/
/* CCU Driver: pm operations                                                 */
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
int ccu_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ccu_suspend(pdev, PMSG_SUSPEND);
}

int ccu_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ccu_resume(pdev);
}

/* extern void mt_irq_set_sens(unsigned int irq, unsigned int sens); */
/* extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity); */
int ccu_pm_restore_noirq(struct device *device)
{
#ifndef CONFIG_OF
	mt_irq_set_sens(CAM0_IRQ_BIT_ID, MT_LEVEL_SENSITIVE);
	mt_irq_set_polarity(CAM0_IRQ_BIT_ID, MT_POLARITY_LOW);
#endif
	return 0;
}
#else
#define ccu_pm_suspend NULL
#define ccu_pm_resume  NULL
#define ccu_pm_restore_noirq NULL
#endif

const struct dev_pm_ops ccu_pm_ops = {
	.suspend = ccu_pm_suspend,
	.resume = ccu_pm_resume,
	.freeze = ccu_pm_suspend,
	.thaw = ccu_pm_resume,
	.poweroff = ccu_pm_suspend,
	.restore = ccu_pm_resume,
	.restore_noirq = ccu_pm_restore_noirq,
};


/*---------------------------------------------------------------------------*/
/* CCU Driver: Prototype                                                     */
/*---------------------------------------------------------------------------*/

static const struct of_device_id ccu_of_ids[] = {
	{.compatible = "mediatek,ccu",},
	{.compatible = "mediatek,ccu_camsys",},
	{.compatible = "mediatek,n3d_ctl_a",},
	{}
};

static struct platform_driver ccu_driver = {
	.probe = ccu_probe,
	.remove = ccu_remove,
	.suspend = ccu_suspend,
	.resume = ccu_resume,
	.driver = {
		   .name = CCU_DEV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = ccu_of_ids,
#endif
#ifdef CONFIG_PM
		   .pm = &ccu_pm_ops,
#endif
	}
};


/*---------------------------------------------------------------------------*/
/* CCU Driver: file operations                                               */
/*---------------------------------------------------------------------------*/
static int ccu_open(struct inode *inode, struct file *flip);

static int ccu_release(struct inode *inode, struct file *flip);

static int ccu_mmap(struct file *flip, struct vm_area_struct *vma);

static long ccu_ioctl(struct file *flip, unsigned int cmd, unsigned long arg);

#ifdef CONFIG_COMPAT
static long ccu_compat_ioctl(
	struct file *flip, unsigned int cmd, unsigned long arg);
#endif

static const struct file_operations ccu_fops = {
	.owner = THIS_MODULE,
	.open = ccu_open,
	.release = ccu_release,
	.mmap = ccu_mmap,
	.unlocked_ioctl = ccu_ioctl,
#ifdef CONFIG_COMPAT
	/*for 32bit usersapce program doing ioctl, compat_ioctl will be called*/
	.compat_ioctl = ccu_compat_ioctl
#endif
};

/*---------------------------------------------------------------------------*/
/* M4U: fault callback                                                       */
/*---------------------------------------------------------------------------*/
m4u_callback_ret_t ccu_m4u_fault_callback(
	int port, unsigned int mva, void *data)
{
	LOG_DBG("[m4u] fault callback: port=%d, mva=0x%x", port, mva);
	return 0/*M4U_CALLBACK_HANDLED*/;
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static int ccu_num_users;

int ccu_create_user(struct ccu_user_s **user)
{
	struct ccu_user_s *u;

	u = kmalloc(sizeof(vlist_type(struct ccu_user_s)), GFP_ATOMIC);
	if (!u)
		return -1;

	mutex_init(&u->data_mutex);
	/*mutex_lock(&u->data_mutex);*/
	u->id = ++ccu_num_users;
	u->open_pid = current->pid;
	u->open_tgid = current->tgid;
	u->running = false;
	u->flush = false;
	INIT_LIST_HEAD(&u->enque_ccu_cmd_list);
	INIT_LIST_HEAD(&u->deque_ccu_cmd_list);
	init_waitqueue_head(&u->deque_wait);
	/*mutex_unlock(&u->data_mutex);*/

	mutex_lock(&g_ccu_device->user_mutex);
	list_add_tail(
		vlist_link(u, struct ccu_user_s), &g_ccu_device->user_list);
	mutex_unlock(&g_ccu_device->user_mutex);

	*user = u;
	return 0;
}


int ccu_push_command_to_queue(struct ccu_user_s *user, struct ccu_cmd_s *cmd)
{
	LOG_DBG("+:%s\n", __func__);
	/*Accuire user_mutex to ensure drv. release func. concurrency*/
	mutex_lock(&g_ccu_device->user_mutex);
	if (user == NULL) {
		LOG_ERR("empty user");
		return -1;
	}
	mutex_unlock(&g_ccu_device->user_mutex);

	mutex_lock(&user->data_mutex);
	list_add_tail(
		vlist_link(cmd, struct ccu_cmd_s), &user->enque_ccu_cmd_list);
	mutex_unlock(&user->data_mutex);

	spin_lock(&g_ccu_device->cmd_wait.lock);
	wake_up_locked(&g_ccu_device->cmd_wait);
	spin_unlock(&g_ccu_device->cmd_wait.lock);

	return 0;
}

int ccu_flush_commands_from_queue(struct ccu_user_s *user)
{

	struct list_head *head, *temp;
	struct ccu_cmd_s *cmd;

	mutex_lock(&user->data_mutex);

	if (!user->running && list_empty(&user->enque_ccu_cmd_list)
	    && list_empty(&user->deque_ccu_cmd_list)) {
		mutex_unlock(&user->data_mutex);
		return 0;
	}

	user->flush = true;
	mutex_unlock(&user->data_mutex);

	/* the running command will add to the deque before interrupt */
	wait_event_interruptible(user->deque_wait, !user->running);

	mutex_lock(&user->data_mutex);
	/* push the remaining enque to the deque */
	list_for_each_safe(head, temp, &user->enque_ccu_cmd_list) {
		cmd = vlist_node_of(head, struct ccu_cmd_s);
		cmd->status = CCU_ENG_STATUS_FLUSH;
		list_del_init(vlist_link(cmd, struct ccu_cmd_s));
		list_add_tail(vlist_link(cmd, struct ccu_cmd_s),
			&user->deque_ccu_cmd_list);
	}

	user->flush = false;
	mutex_unlock(&user->data_mutex);
	return 0;
}

int ccu_pop_command_from_queue(struct ccu_user_s *user, struct ccu_cmd_s **rcmd)
{
	int ret;
	struct ccu_cmd_s *cmd;

	LOG_DBG("+:%s\n", __func__);

	/*Accuire user_mutex to ensure drv. release func. concurrency*/
	mutex_lock(&g_ccu_device->user_mutex);
	if (user == NULL) {
		LOG_ERR("empty user");
		return -1;
	}
	mutex_unlock(&g_ccu_device->user_mutex);

	/* wait until condition is true */
	ret = wait_event_interruptible_timeout(user->deque_wait,
					!list_empty(&user->deque_ccu_cmd_list),
					msecs_to_jiffies(3 * 1000));

	/* ret == 0, if timeout; ret == -ERESTARTSYS, if signal interrupt */
	if (ret == 0) {
		LOG_ERR("timeout: pop a command! ret=%d\n", ret);
		*rcmd = NULL;
		return -1;
	} else if (ret < 0) {
		LOG_ERR("interrupted by system signal: %d\n", ret);

		if (ret == -ERESTARTSYS)
			LOG_ERR("interrupted as -ERESTARTSYS\n");

		return ret;
	}
	mutex_lock(&user->data_mutex);
	/* This part should not be happened */
	if (list_empty(&user->deque_ccu_cmd_list)) {
		mutex_unlock(&user->data_mutex);
		LOG_ERR("pop a command from empty queue! ret=%d\n", ret);
		*rcmd = NULL;
		return -1;
	};

	/* get first node from deque list */
	cmd = vlist_node_of(user->deque_ccu_cmd_list.next, struct ccu_cmd_s);
	list_del_init(vlist_link(cmd, struct ccu_cmd_s));

	mutex_unlock(&user->data_mutex);

	*rcmd = cmd;
	return 0;
}


int ccu_delete_user(struct ccu_user_s *user)
{

	if (!user) {
		LOG_ERR("delete empty user!\n");
		return -1;
	}
	/* TODO: notify dropeed command to user?*/
	/* ccu_dropped_command_notify(user, command);*/
	ccu_flush_commands_from_queue(user);

	mutex_lock(&g_ccu_device->user_mutex);
	list_del(vlist_link(user, struct ccu_user_s));
	mutex_unlock(&g_ccu_device->user_mutex);

	kfree(user);

	return 0;
}

int ccu_lock_user_mutex(void)
{
	mutex_lock(&g_ccu_device->user_mutex);
	return 0;
}

int ccu_unlock_user_mutex(void)
{
	mutex_unlock(&g_ccu_device->user_mutex);
	return 0;
}

/*---------------------------------------------------------------------------*/
/* IOCTL: implementation                                                     */
/*---------------------------------------------------------------------------*/
int ccu_set_power(struct ccu_power_s *power)
{
	return ccu_power(power);
}

static int ccu_open(struct inode *inode, struct file *flip)
{
	int ret = 0;

	struct ccu_user_s *user;

	ccu_create_user(&user);
	if (IS_ERR_OR_NULL(user)) {
		LOG_ERR("fail to create user\n");
		return -ENOMEM;
	}

	flip->private_data = user;

	return ret;
}

#ifdef CONFIG_COMPAT
static long ccu_compat_ioctl(
	struct file *flip, unsigned int cmd, unsigned long arg)
{
	/*<<<<<<<<<< debug 32/64 compat check*/
	struct compat_ccu_power_s __user *ptr_power32;
	struct ccu_power_s __user *ptr_power64;

	compat_uptr_t uptr_Addr32;
	compat_uint_t uint_Data32;

	int err;
	int i;
	/*>>>>>>>>>> debug 32/64 compat check*/

	int ret = 0;
	struct ccu_user_s *user = flip->private_data;

	LOG_DBG("+, cmd: %d\n", cmd);

	switch (cmd) {
	case CCU_IOCTL_SET_POWER:
	{
		LOG_DBG("CCU_IOCTL_SET_POWER+\n");

		/*<<<<<<<<<< debug 32/64 compat check*/
		LOG_DBG("[IOCTL_DBG] struct ccu_power_s size: %zu\n",
			sizeof(struct ccu_power_s));
		LOG_DBG("[IOCTL_DBG] ccu_working_buffer_t size: %zu\n",
			sizeof(ccu_working_buffer_t));
		LOG_DBG("[IOCTL_DBG] arg: %p\n", (void *)arg);
		LOG_DBG("[IOCTL_DBG] long size: %zu\n", sizeof(long));
		LOG_DBG("[IOCTL_DBG] long long: %zu\n",
			sizeof(long long));
		LOG_DBG("[IOCTL_DBG] char *size: %zu\n",
			sizeof(char *));
		LOG_DBG("[IOCTL_DBG] power.workBuf.va_log[0]: %p\n",
			power.workBuf.va_log[0]);

		ptr_power32 = compat_ptr(arg);
		ptr_power64 = compat_alloc_user_space(sizeof(*ptr_power64));
		if (ptr_power64 == NULL)
			return -EFAULT;

		LOG_DBG("[IOCTL_DBG] (void *)arg: %p\n", (void *)arg);
		LOG_DBG("[IOCTL_DBG] ptr_power32: %p\n", ptr_power32);
		LOG_DBG("[IOCTL_DBG] ptr_power64: %p\n", ptr_power64);
		LOG_DBG("[IOCTL_DBG] *ptr_power32 size: %zu\n",
			sizeof(*ptr_power32));
		LOG_DBG("[IOCTL_DBG] *ptr_power64 size: %zu\n",
			sizeof(*ptr_power64));

		err = 0;
		err |= get_user(uint_Data32, &(ptr_power32->bON));
		err |= put_user(uint_Data32, &(ptr_power64->bON));

		for (i = 0; i < MAX_LOG_BUF_NUM; i++) {
			err |=
			get_user(
			uptr_Addr32,
			(&ptr_power32->workBuf.va_log[i]));
			err |=
			put_user(compat_ptr(uptr_Addr32),
				(&ptr_power64->workBuf.va_log[i]));
			err |=
			get_user(
			uint_Data32,
			(&ptr_power32->workBuf.mva_log[i]));
			err |=
			copy_to_user(&(ptr_power64->workBuf.mva_log[i]),
				&uint_Data32, sizeof(uint_Data32));
		}

		LOG_DBG("[IOCTL_DBG] err: %d\n", err);
		LOG_DBG("[IOCTL_DBG] ptr_power32->workBuf.va_pool: %x\n",
			ptr_power32->workBuf.va_pool);
		LOG_DBG("[IOCTL_DBG] ptr_power64->workBuf.va_pool: %p\n",
			ptr_power64->workBuf.va_pool);
		LOG_DBG("[IOCTL_DBG] ptr_power32->workBuf.va_log: %x\n",
			ptr_power32->workBuf.va_log[0]);
		LOG_DBG("[IOCTL_DBG] ptr_power64->workBuf.va_log: %p\n",
			ptr_power64->workBuf.va_log[0]);
		LOG_DBG("[IOCTL_DBG] ptr_power32->workBuf.mva_log: %x\n",
			ptr_power32->workBuf.mva_log[0]);
		LOG_DBG("[IOCTL_DBG] ptr_power64->workBuf.mva_log: %x\n",
			ptr_power64->workBuf.mva_log[0]);

		ret =
		flip->f_op->unlocked_ioctl(
		flip,
		cmd,
		(unsigned long)ptr_power64);
		/*>>>>>>>>>> debug 32/64 compat check*/

		LOG_DBG("CCU_IOCTL_SET_POWER-");
		break;
	}
	default:
		ret = flip->f_op->unlocked_ioctl(flip, cmd, arg);
		break;
	}

	if (ret != 0) {
	LOG_ERR(
	"fail,cmd(%d),pid(%d),(process,pid,tgid)=(%s,%d,%d)\n",
	cmd,
	user->open_pid,
	current->comm,
	current->pid,
	current->tgid);
	}

	return ret;
}
#endif

static int ccu_alloc_command(struct ccu_cmd_s **rcmd)
{
	struct ccu_cmd_s *cmd;

	cmd = kzalloc(sizeof(vlist_type(struct ccu_cmd_s)), GFP_KERNEL);
	if (cmd == NULL) {
		LOG_ERR("%s, node=0x%p\n", __func__, cmd);
		return -ENOMEM;
	}

	*rcmd = cmd;

	return 0;
}


static int ccu_free_command(struct ccu_cmd_s *cmd)
{
	kfree(cmd);
	return 0;
}

int ccu_clock_enable(void)
{
	int ret;

	LOG_DBG_MUST("%s.\n", __func__);

	ret = clk_prepare_enable(ccu_clock_ctrl);
	if (ret)
		LOG_ERR("clock enable fail.\n");

	return ret;
}

void ccu_clock_disable(void)
{
	LOG_DBG_MUST("%s.\n", __func__);
	clk_disable_unprepare(ccu_clock_ctrl);
}

static long ccu_ioctl(struct file *flip, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int powerStat;
	struct CCU_WAIT_IRQ_STRUCT IrqInfo;
	struct ccu_user_s *user = flip->private_data;

	LOG_DBG("%s+, cmd:%d\n", __func__, cmd);

	if ((cmd == CCU_IOCTL_SEND_CMD) ||
		(cmd == CCU_IOCTL_ENQUE_COMMAND) ||
		(cmd == CCU_IOCTL_DEQUE_COMMAND) ||
		(cmd == CCU_IOCTL_WAIT_IRQ) ||
		(cmd == CCU_READ_REGISTER)) {
		powerStat = ccu_query_power_status();
		if (powerStat == 0) {
			LOG_WRN("ccuk: ioctl without powered on\n");
			return -EFAULT;
		}
	}

	switch (cmd) {
	case CCU_IOCTL_SET_POWER:
	{
		LOG_DBG("ccuk: ioctl set powerk+: %p\n", (void *)arg);
		ret = copy_from_user(
			&power, (void *)arg, sizeof(struct ccu_power_s));
		if (!ret) {
		LOG_ERR(
		"[SET_POWER] copy_from_user failed, ret=%d\n", ret);
		return -EFAULT;
		}
		/* to prevent invalid operation, check is arguments reasonable
		 * CCU can only be powered on when power is currently off,
		 * and powered off when power is currently on
		 */
		powerStat = ccu_query_power_status();
		if (((power.bON == 1) && (powerStat == 0)) ||
			((power.bON == 0) && (powerStat == 1))) {
			ret = ccu_set_power(&power);
		} else {
			LOG_WRN(
			"ccuk: set powe invalid,Stat:0x%x,Arg:0x%x",
			powerStat, power.bON);
			return -EFAULT;
		}

		LOG_DBG("ccuk: ioctl set powerk-\n");
		break;
	}
	case CCU_IOCTL_SET_RUN:
	{
		ret = ccu_run();
		break;
	}
	case CCU_IOCTL_ENQUE_COMMAND:
	{
		struct ccu_cmd_s *cmd = 0;

		/*allocate struct ccu_cmd_s_list instead of struct ccu_cmd_s*/
		ccu_alloc_command(&cmd);
		ret = copy_from_user(
			cmd, (void *)arg, sizeof(struct ccu_cmd_s));
		if (!ret) {
		LOG_ERR(
		"[ENQUE_COMMAND] copy_from_user failed,ret=%d\n", ret);
		return -EFAULT;
		}
		ret = ccu_push_command_to_queue(user, cmd);
		break;
	}
	case CCU_IOCTL_DEQUE_COMMAND:
	{
		struct ccu_cmd_s *cmd = 0;

		ret = ccu_pop_command_from_queue(user, &cmd);
		if (!ret) {
		LOG_ERR(
		"[DEQUE_COMMAND]pop command failed,ret=%d\n", ret);
		return -EFAULT;
		}
		ret = copy_to_user((void *)arg, cmd, sizeof(struct ccu_cmd_s));
		if (!ret) {
		LOG_ERR(
		"[DEQUE_COMMAND]copy_to_user failed,ret=%d\n", ret);
		return -EFAULT;
		}
		ret = ccu_free_command(cmd);
		if (!ret) {
		LOG_ERR(
		"[DEQUE_COMMAND]free command,ret=%d\n", ret);
		}
		break;
	}
	case CCU_IOCTL_FLUSH_COMMAND:
	{
		ret = ccu_flush_commands_from_queue(user);
		if (!ret) {
		LOG_ERR(
		"[FLUSH_COMMAND]failed,ret=%d\n", ret);
		}
		break;
	}
	case CCU_IOCTL_WAIT_IRQ:
	{
		if (
		copy_from_user(
		&IrqInfo,
		(void *)arg,
		sizeof(struct CCU_WAIT_IRQ_STRUCT)) == 0) {
			if (
			(IrqInfo.Type >= CCU_IRQ_TYPE_AMOUNT)
			|| (IrqInfo.Type < 0)) {
				ret = -EFAULT;
				LOG_ERR("invalid type(%d)\n", IrqInfo.Type);
				goto EXIT;
			}

			LOG_DBG(
			"IRQ type(%d),Key(%d),to(%d),sttype(%d),st(%d)\n",
			IrqInfo.Type,
			IrqInfo.EventInfo.UserKey,
			IrqInfo.EventInfo.Timeout,
			IrqInfo.EventInfo.St_type,
			IrqInfo.EventInfo.Status);

			ret = ccu_waitirq(&IrqInfo);

			if (
			copy_to_user(
			(void *)arg,
			&IrqInfo,
			sizeof(struct CCU_WAIT_IRQ_STRUCT))
			    != 0) {
				LOG_ERR("copy_to_user failed\n");
				ret = -EFAULT;
			}
		} else {
			LOG_ERR("copy_from_user failed\n");
			ret = -EFAULT;
		}

		break;
	}
	case CCU_IOCTL_SEND_CMD:	/*--todo: not used for now, remove it*/
	{
		struct ccu_cmd_s cmd;

		ret = copy_from_user(
			&cmd, (void *)arg, sizeof(struct ccu_cmd_s));
		if (!ret) {
		LOG_ERR(
		"[CCU_IOCTL_SEND_CMD] copy_from_user failed,ret=%d\n",
		ret);
		return -EFAULT;
		}
		ccu_send_command(&cmd);
		break;
	}
	case CCU_IOCTL_FLUSH_LOG:
	{
		ccu_flushLog(0, NULL);
		break;
	}
	case CCU_IOCTL_GET_I2C_DMA_BUF_ADDR:
	{
		uint32_t mva;

		ret = ccu_get_i2c_dma_buf_addr(&mva);

		if (ret != 0) {
			LOG_DBG("ccu_get_i2c_dma_buf_addr fail: %d\n", ret);
			break;
		}

		ret = copy_to_user((void *)arg, &mva, sizeof(uint32_t));

		break;
	}
	case CCU_IOCTL_SET_I2C_MODE:
	{
		struct ccu_i2c_arg i2c_arg;

		ret =
		copy_from_user(
		&i2c_arg,
		(void *)arg,
		sizeof(struct ccu_i2c_arg));

		ret = ccu_i2c_ctrl(i2c_arg.i2c_write_id, i2c_arg.transfer_len);

		break;
	}
	case CCU_IOCTL_SET_I2C_CHANNEL:
	{
		enum CCU_I2C_CHANNEL channel;

		ret =
		copy_from_user(
		&channel,
		(void *)arg,
		sizeof(enum CCU_I2C_CHANNEL));

		ret = ccu_i2c_set_channel(channel);

		if (ret < 0)
			LOG_ERR("invalid i2c channel: %d\n", channel);

		break;
	}
	case CCU_IOCTL_GET_CURRENT_FPS:
	{
		int32_t current_fps = ccu_get_current_fps();

		ret = copy_to_user((void *)arg, &current_fps, sizeof(int32_t));

		break;
	}
	case CCU_IOCTL_GET_SENSOR_I2C_SLAVE_ADDR:
	{
		int32_t sensorI2cSlaveAddr[3];

		ccu_get_sensor_i2c_slave_addr(&sensorI2cSlaveAddr[0]);

		ret =
		copy_to_user(
		(void *)arg,
		&sensorI2cSlaveAddr,
		sizeof(int32_t) * 3);

		break;
	}

	case CCU_IOCTL_GET_SENSOR_NAME:
	{
		#define SENSOR_NAME_MAX_LEN 32

		char *sensor_names[3];

		ccu_get_sensor_name(sensor_names);

		if (sensor_names[0] != NULL) {
			ret =
			copy_to_user(
			(char *)arg,
			sensor_names[0],
			strlen(sensor_names[0])+1);
			if (ret != 0) {
				LOG_ERR("copy_to_user 1 failed: %d\n", ret);
				break;
			}
		}

		if (sensor_names[1] != NULL) {
			ret = copy_to_user(((char *)arg+SENSOR_NAME_MAX_LEN),
				sensor_names[1], strlen(sensor_names[1])+1);
			if (ret != 0) {
				LOG_ERR("copy_to_user 2 failed: %d\n", ret);
				break;
			}
		}

		if (sensor_names[2] != NULL) {
			ret = copy_to_user(((char *)arg+SENSOR_NAME_MAX_LEN*2),
				sensor_names[2], strlen(sensor_names[2])+1);
			if (ret != 0) {
				LOG_ERR("copy_to_user 3 failed: %d\n", ret);
				break;
			}
		}

		#undef SENSOR_NAME_MAX_LEN
		break;
	}

	case CCU_IOCTL_GET_PLATFORM_INFO:
	{
		ret =
		copy_to_user(
		(void *)arg,
		&g_ccu_platform_info,
		sizeof(g_ccu_platform_info));
		break;
	}

	case CCU_READ_REGISTER:
		{
			int regToRead = (int)arg;

			return ccu_read_info_reg(regToRead);
		}
	default:
		LOG_WRN("ioctl:No such command: %d!\n", cmd);
		ret = -EINVAL;
		break;
	}

EXIT:
	if (ret != 0) {
		LOG_ERR(
		"fail,cmd(%d),pid(%d),(process,pid,tgid)=(%s,%d,%d)\n",
		cmd,
		user->open_pid,
		current->comm,
		current->pid,
		current->tgid);
	}
	return ret;
}

static int ccu_release(struct inode *inode, struct file *flip)
{
	struct ccu_user_s *user = flip->private_data;

	LOG_INF_MUST("%s +", __func__);

	ccu_delete_user(user);

	LOG_INF_MUST("+:%s, delete_user done.\n", __func__);

	ccu_force_powerdown();

	LOG_INF_MUST("%s -", __func__);

	return 0;
}

static int ccu_mmap(struct file *flip, struct vm_area_struct *vma)
{
	unsigned long length = 0;
	unsigned int pfn = 0x0;

	length = (vma->vm_end - vma->vm_start);
	/*  */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	pfn = vma->vm_pgoff << PAGE_SHIFT;

	LOG_DBG(
	"CCU_mmap: vm_pgoff(0x%lx),pfn(0x%x),phy(0x%lx)",
	vma->vm_pgoff,
	pfn,
	vma->vm_pgoff << PAGE_SHIFT);
	LOG_DBG(
	"vm_start(0x%lx), vm_end(0x%lx),length(0x%lx)\n",
	vma->vm_start,
	vma->vm_end,
	length);

	/*if (pfn >= CCU_REG_BASE_HW) {*/

	if (pfn ==
	(g_ccu_platform_info.ccu_hw_base -
	g_ccu_platform_info.ccu_hw_offset)) {
		if (length > CCU_REG_RANGE) {
			LOG_ERR(
			"mmap err:mod(0x%x),len(0x%lx),CCU_A(0x%x)!\n",
			pfn, length, 0x4000);
			return -EAGAIN;
		}
	} else if (pfn == g_ccu_platform_info.ccu_camsys_base) {
		if (length > g_ccu_platform_info.ccu_camsys_size) {
			LOG_ERR(
			"mmap err:mod(0x%x),len(0x%lx),CAMSYS(0x%x)!\n",
			pfn, length, 0x4000);
			return -EAGAIN;
		}
	} else if (pfn == g_ccu_platform_info.ccu_pmem_base) {
		if (length > g_ccu_platform_info.ccu_pmem_size) {
			LOG_ERR(
			"mmap err:mod(0x%x),len(0x%lx),PMEM(0x%x)!\n",
			pfn, length, 0x4000);
			return -EAGAIN;
		}
	} else if (pfn == g_ccu_platform_info.ccu_dmem_base) {
		if (length > g_ccu_platform_info.ccu_dmem_size) {
			LOG_ERR(
			"mmap err:mod(0x%x),len(0x%lx),PMEM(0x%x)!\n",
			pfn, length, 0x4000);
			return -EAGAIN;
		}
	} else {
		LOG_ERR("Illegal starting HW addr for mmap!\n");
		return -EAGAIN;
	}

	if (remap_pfn_range
		(vma, vma->vm_start, vma->vm_pgoff, vma->vm_end - vma->vm_start,
		vma->vm_page_prot)) {
		LOG_ERR("remap_pfn_range\n");
		return -EAGAIN;
	}
	LOG_DBG("map_check_1\n");

	return 0;
}

static dev_t ccu_devt;
static struct cdev *ccu_chardev;
static struct class *ccu_class;
static int ccu_num_devs;

static inline void ccu_unreg_chardev(void)
{
	/* Release char driver */
	if (ccu_chardev != NULL) {
		cdev_del(ccu_chardev);
		ccu_chardev = NULL;
	}
	unregister_chrdev_region(ccu_devt, 1);
}

static inline int ccu_reg_chardev(void)
{
	int ret = 0;

	ret = alloc_chrdev_region(&ccu_devt, 0, 1, CCU_DEV_NAME);
	if ((ret) < 0) {
		LOG_ERR("alloc_chrdev_region failed, %d\n", ret);
		return ret;
	}
	/* Allocate driver */
	ccu_chardev = cdev_alloc();
	if (ccu_chardev == NULL) {
		LOG_ERR("cdev_alloc failed\n");
		ret = -ENOMEM;
		goto EXIT;
	}

	/* Attatch file operation. */
	cdev_init(ccu_chardev, &ccu_fops);

	ccu_chardev->owner = THIS_MODULE;

	/* Add to system */
	ret = cdev_add(ccu_chardev, ccu_devt, 1);
	if ((ret) < 0) {
		LOG_ERR("Attatch file operation failed, %d\n", ret);
		goto EXIT;
	}

EXIT:
	if (ret < 0)
		ccu_unreg_chardev();

	return ret;
}

static int ccu_read_platform_info_from_dt(struct device_node *node)
{
	uint32_t reg[4] = {0, 0, 0, 0};
	int ret = 0;

	ret = of_property_read_u32_array(node, "reg", reg, 4);
	if (ret < 0)
		LOG_ERR("of_property_read_u32_array ERR : %d\n", ret);

	g_ccu_platform_info.ccu_hw_base = reg[1];
	of_property_read_u32(
		node, "ccu_hw_offset",
		&(g_ccu_platform_info.ccu_hw_offset));
	of_property_read_u32(
		node, "ccu_pmem_base",
		&(g_ccu_platform_info.ccu_pmem_base));
	of_property_read_u32(
		node, "ccu_pmem_size",
		&(g_ccu_platform_info.ccu_pmem_size));
	of_property_read_u32(
		node, "ccu_dmem_base",
		&(g_ccu_platform_info.ccu_dmem_base));
	of_property_read_u32(
		node, "ccu_dmem_size",
		&(g_ccu_platform_info.ccu_dmem_size));
	of_property_read_u32(
		node, "ccu_dmem_offset",
		&(g_ccu_platform_info.ccu_dmem_offset));
	of_property_read_u32(
		node, "ccu_log_base",
		&(g_ccu_platform_info.ccu_log_base));
	of_property_read_u32(
		node, "ccu_log_size",
		&(g_ccu_platform_info.ccu_log_size));
	of_property_read_u32(
		node, "ccu_hw_dump_size",
		&(g_ccu_platform_info.ccu_hw_dump_size));
	of_property_read_u32(
		node, "ccu_camsys_base",
		&(g_ccu_platform_info.ccu_camsys_base));
	of_property_read_u32(
		node, "ccu_camsys_size",
		&(g_ccu_platform_info.ccu_camsys_size));
	of_property_read_u32(
		node, "ccu_n3d_a_base",
		&(g_ccu_platform_info.ccu_n3d_a_base));
	of_property_read_u32(
		node, "ccu_n3d_a_size",
		&(g_ccu_platform_info.ccu_n3d_a_size));
	of_property_read_u32(
		node, "ccu_sensor_pm_size",
		&(g_ccu_platform_info.ccu_sensor_pm_size));
	of_property_read_u32(
		node, "ccu_sensor_dm_size",
		&(g_ccu_platform_info.ccu_sensor_dm_size));

	LOG_DBG(
		"ccu read dt property ccu_hw_base = %x\n",
		g_ccu_platform_info.ccu_hw_base);
	LOG_DBG(
		"ccu read dt property ccu_hw_offset = %x\n",
		g_ccu_platform_info.ccu_hw_offset);
	LOG_DBG(
		"ccu read dt property ccu_pmem_base = %x\n",
		g_ccu_platform_info.ccu_pmem_base);
	LOG_DBG(
		"ccu read dt property ccu_pmem_size = %x\n",
		g_ccu_platform_info.ccu_pmem_size);
	LOG_DBG(
		"ccu read dt property ccu_dmem_base = %x\n",
		g_ccu_platform_info.ccu_dmem_base);
	LOG_DBG(
		"ccu read dt property ccu_dmem_size = %x\n",
		g_ccu_platform_info.ccu_dmem_size);
	LOG_DBG(
		"ccu read dt property ccu_dmem_offset = %x\n",
		g_ccu_platform_info.ccu_dmem_offset);
	LOG_DBG(
		"ccu read dt property ccu_log_base = %x\n",
		g_ccu_platform_info.ccu_log_base);
	LOG_DBG(
		"ccu read dt property ccu_log_size = %x\n",
		g_ccu_platform_info.ccu_log_size);
	LOG_DBG(
		"ccu read dt property ccu_hw_dump_size = %x\n",
		g_ccu_platform_info.ccu_hw_dump_size);
	LOG_DBG(
		"ccu read dt property ccu_camsys_base = %x\n",
		g_ccu_platform_info.ccu_camsys_base);
	LOG_DBG(
		"ccu read dt property ccu_camsys_size = %x\n",
		g_ccu_platform_info.ccu_camsys_size);
	LOG_DBG(
		"ccu read dt property ccu_n3d_a_base = %x\n",
		g_ccu_platform_info.ccu_n3d_a_base);
	LOG_DBG(
		"ccu read dt property ccu_n3d_a_size = %x\n",
		g_ccu_platform_info.ccu_n3d_a_size);
	LOG_DBG(
		"ccu read dt property ccu_sensor_pm_size = %x\n",
		g_ccu_platform_info.ccu_sensor_pm_size);
	LOG_DBG(
		"ccu read dt property ccu_sensor_dm_size = %x\n",
		g_ccu_platform_info.ccu_sensor_dm_size);

	return ret;
}

static int ccu_probe(struct platform_device *pdev)
{
#ifdef CONFIG_OF
	struct device *dev;
	struct device_node *node;
	int ret = 0;
	uint32_t phy_addr;
	uint32_t phy_size;


	node = pdev->dev.of_node;
	g_ccu_device->dev = &pdev->dev;
	LOG_DBG("probe 0, pdev id = %d name = %s\n", pdev->id, pdev->name);

	ccu_read_platform_info_from_dt(node);

#ifdef MTK_CCU_EMULATOR
	/* emulator will fill ccu_base and bin_base */
	/*ccu_init_emulator(g_ccu_device);*/
#else
	/* get register address */
	if ((strcmp("ccu", g_ccu_device->dev->of_node->name) == 0)) {
		{
			/*remap ccu_base*/
			phy_addr =
			g_ccu_platform_info.ccu_hw_base;
			phy_size =
			0x1000;
			g_ccu_device->ccu_base =
			(unsigned long)ioremap_wc(phy_addr, phy_size);
			LOG_INF(
				"ccu_base pa: 0x%x, size: 0x%x\n",
				phy_addr, phy_size);
			LOG_INF(
				"ccu_base va: 0x%lx\n",
				g_ccu_device->ccu_base);

			/*remap dmem_base*/
			phy_addr =
			g_ccu_platform_info.ccu_dmem_base;
			phy_size =
			g_ccu_platform_info.ccu_dmem_size;
			g_ccu_device->dmem_base =
			(unsigned long)ioremap_wc(phy_addr, phy_size);
			LOG_INF(
				"dmem_base pa: 0x%x, size: 0x%x\n",
				phy_addr, phy_size);
			LOG_INF(
				"dmem_base va: 0x%lx\n",
				g_ccu_device->dmem_base);

			/*remap camsys_base*/
			phy_addr =
			g_ccu_platform_info.ccu_camsys_base;
			phy_size =
			g_ccu_platform_info.ccu_camsys_size;
			g_ccu_device->camsys_base =
			(unsigned long)ioremap_wc(phy_addr, phy_size);
			LOG_INF(
				"camsys_base pa: 0x%x, size: 0x%x\n",
				phy_addr, phy_size);
			LOG_INF(
				"camsys_base va: 0x%lx\n",
				g_ccu_device->camsys_base);

			/*remap n3d_a_base*/
			phy_addr =
			g_ccu_platform_info.ccu_n3d_a_base;
			phy_size =
			g_ccu_platform_info.ccu_n3d_a_size;
			g_ccu_device->n3d_a_base =
			(unsigned long)ioremap_wc(phy_addr, phy_size);
			LOG_INF(
				"n3d_a_base pa: 0x%x, size: 0x%x\n",
				phy_addr, phy_size);
			LOG_INF(
				"n3d_a_base va: 0x%lx\n",
				g_ccu_device->n3d_a_base);

		}
		/* get Clock control from device tree.  */
		{
			ccu_clock_ctrl =
			devm_clk_get(
				g_ccu_device->dev,
				"CCU_CLK_CAM_CCU");
			if (ccu_clock_ctrl == NULL)
				LOG_ERR("Get ccu clock ctrl fail.\n");
		}
		/**/
		g_ccu_device->irq_num = irq_of_parse_and_map(node, 0);
		LOG_DBG(
		"probe 1,ccu_base:0x%lx,bin_base:0x%lx,irq_num:%d,pdev:%p\n",
		g_ccu_device->ccu_base,
		g_ccu_device->bin_base,
		g_ccu_device->irq_num,
		g_ccu_device->dev);

		if (g_ccu_device->irq_num > 0) {
			/* get IRQ flag from device node */
			unsigned int irq_info[3];

			if (of_property_read_u32_array
			(node, "interrupts", irq_info,
			ARRAY_SIZE(irq_info))) {
			LOG_DERR(g_ccu_device->dev,
			"get irq flags from DTS fail!\n");
			return -ENODEV;
			}
		} else {
			LOG_DBG(
			"No IRQ!!: ccu_num_devs=%d, devnode(%s), irq=%d\n",
			ccu_num_devs, g_ccu_device->dev->of_node->name,
			g_ccu_device->irq_num);
		}

		/* Only register char driver in the 1st time */
		if (++ccu_num_devs == 1) {

			/* Register char driver */
			ret = ccu_reg_chardev();
			if (ret) {
				LOG_DERR(g_ccu_device->dev,
					"register char failed");
				return ret;
			}

			/* Create class register */
			ccu_class = class_create(THIS_MODULE, "ccudrv");
			if (IS_ERR(ccu_class)) {
				ret = PTR_ERR(ccu_class);
				LOG_ERR("Unable to create class, err = %d\n",
					ret);
				goto EXIT;
			}

			dev = device_create(ccu_class, NULL, ccu_devt,
				NULL, CCU_DEV_NAME);
			if (IS_ERR(dev)) {
				ret = PTR_ERR(dev);
				LOG_DERR(g_ccu_device->dev,
					"Failed to create device: /dev/%s, err = %d",
					CCU_DEV_NAME, ret);
				goto EXIT;
			}
#ifdef CONFIG_PM_WAKELOCKS
			wakeup_source_init(&ccu_wake_lock, "ccu_lock_wakelock");
#else
			wake_lock_init(&ccu_wake_lock,
				WAKE_LOCK_SUSPEND, "ccu_lock_wakelock");
#endif

			/* enqueue/dequeue control in ihalpipe wrapper */
			init_waitqueue_head(&wait_queue_deque);
			init_waitqueue_head(&wait_queue_enque);

			/*register i2c driver callback*/
			ret = ccu_i2c_register_driver();
			if (ret < 0)
				goto EXIT;
			ret = ccu_i2c_set_n3d_base(g_ccu_device->n3d_a_base);
			if (ret < 0)
				goto EXIT;

EXIT:
			if (ret < 0)
				ccu_unreg_chardev();
		}

		ccu_init_hw(g_ccu_device);

		LOG_ERR("ccu probe cuccess...\n");

	}
#endif
#endif

	LOG_DBG("- X. CCU driver probe.\n");

	return ret;
}


static int ccu_remove(struct platform_device *pDev)
{
	/*    struct resource *pRes; */
	int irq_num;

	/*  */
	LOG_DBG("- E.");

	/*uninit hw*/
	ccu_uninit_hw(g_ccu_device);

	/* unregister char driver. */
	ccu_unreg_chardev();

	/*ccu_i2c_del_drivers();*/
	ccu_i2c_delete_driver();

	/* Release IRQ */
	disable_irq(g_ccu_device->irq_num);
	irq_num = platform_get_irq(pDev, 0);
	free_irq(irq_num, (void *)ccu_chardev);

	/* kill tasklet */
	/*for (i = 0; i < CCU_IRQ_NUM_TYPES; i++) {*/
	/*      tasklet_kill(ccu_tasklet[i].p_ccu_tkt);*/
	/*}*/

	/*  */
	device_destroy(ccu_class, ccu_devt);
	/*  */
	class_destroy(ccu_class);
	ccu_class = NULL;
	/*  */
	return 0;
}

static int ccu_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int ccu_resume(struct platform_device *pdev)
{
	return 0;
}

static int __init CCU_INIT(void)
{
	int ret = 0;
	/*struct device_node *node = NULL;*/

	g_ccu_device = kzalloc(sizeof(struct ccu_device_s), GFP_KERNEL);
	/*g_ccu_device = dma_cache_coherent();*/

	INIT_LIST_HEAD(&g_ccu_device->user_list);
	mutex_init(&g_ccu_device->user_mutex);
	init_waitqueue_head(&g_ccu_device->cmd_wait);

	/* Register M4U callback */
	LOG_DBG("register m4u callback");

#ifdef CONFIG_MTK_IOMMU
	mtk_iommu_register_fault_callback(CCUI_OF_M4U_PORT,
		(mtk_iommu_fault_callback_t *)ccu_m4u_fault_callback, 0);
	mtk_iommu_register_fault_callback(CCUO_OF_M4U_PORT,
		(mtk_iommu_fault_callback_t *)ccu_m4u_fault_callback, 0);
	mtk_iommu_register_fault_callback(CCUG_OF_M4U_PORT,
		(mtk_iommu_fault_callback_t *)ccu_m4u_fault_callback, 0);

#elif defined(CONFIG_MTK_M4U)
	m4u_register_fault_callback(CCUI_OF_M4U_PORT,
		ccu_m4u_fault_callback, NULL);
	m4u_register_fault_callback(CCUO_OF_M4U_PORT,
		ccu_m4u_fault_callback, NULL);
	m4u_register_fault_callback(CCUG_OF_M4U_PORT,
		ccu_m4u_fault_callback, NULL);
#endif

	LOG_DBG("platform_driver_register start\n");
	if (platform_driver_register(&ccu_driver)) {
		LOG_ERR("failed to register CCU driver");
		return -ENODEV;
	}

	LOG_DBG("platform_driver_register finsish\n");

	return ret;
}


static void __exit CCU_EXIT(void)
{
	platform_driver_unregister(&ccu_driver);

	kfree(g_ccu_device);

	/* Un-Register M4U callback */
	LOG_DBG("un-register m4u callback");

#ifdef CONFIG_MTK_IOMMU
	mtk_iommu_unregister_fault_callback(CCUI_OF_M4U_PORT);
	mtk_iommu_unregister_fault_callback(CCUO_OF_M4U_PORT);
	mtk_iommu_unregister_fault_callback(CCUG_OF_M4U_PORT);

#elif defined(CONFIG_MTK_M4U)
	m4u_unregister_fault_callback(CCUI_OF_M4U_PORT);
	m4u_unregister_fault_callback(CCUO_OF_M4U_PORT);
	m4u_unregister_fault_callback(CCUG_OF_M4U_PORT);
#endif

}


module_init(CCU_INIT);
module_exit(CCU_EXIT);
MODULE_DESCRIPTION("MTK CCU Driver");
MODULE_AUTHOR("SW1");
MODULE_LICENSE("GPL");
