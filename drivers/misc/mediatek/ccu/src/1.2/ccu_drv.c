/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
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
#include "ccu_platform_def.h"
#include "ccu_n3d_a.h"
#include "ccu_i2c.h"
#include "ccu_i2c_hw.h"
#include "kd_camera_feature.h"/*for IMGSENSOR_SENSOR_IDX*/

/**************************************************************************
 *
 **************************************************************************/

#define CCU_DEV_NAME            "ccu"

#define CCU_CLK_PWR_NUM 2

static int32_t _user_count;

struct clk *ccu_clk_pwr_ctrl[CCU_CLK_PWR_NUM];

struct ccu_device_s *g_ccu_device;
static struct ccu_power_s power;
static uint32_t ccu_hw_base;

static wait_queue_head_t wait_queue_deque;
static wait_queue_head_t wait_queue_enque;

#ifdef CONFIG_PM_SLEEP
// struct wakeup_source ccu_wake_lock;
#endif
/*static  int g_bWaitLock;*/

static irqreturn_t ccu_isr_callback_xxx(int rrq, void *device_id);

typedef irqreturn_t(*ccu_isr_fp_t) (int, void *);

struct ccu_isr_callback_t {
	ccu_isr_fp_t irq_fp;
	unsigned int int_number;
	char device_name[16];
};

/* int number is got from kernel api */
const struct ccu_isr_callback_t ccu_isr_callbacks[CCU_IRQ_NUM_TYPES] = {
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

static int32_t _clk_count;
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
static long ccu_compat_ioctl(struct file *flip, unsigned int cmd,
	unsigned long arg);
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
m4u_callback_ret_t ccu_m4u_fault_callback(int port, unsigned int mva,
	void *data)
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
	list_add_tail(vlist_link(u, struct ccu_user_s),
		&g_ccu_device->user_list);
	mutex_unlock(&g_ccu_device->user_mutex);

	*user = u;
	return 0;
}


int ccu_push_command_to_queue(struct ccu_user_s *user, struct ccu_cmd_s *cmd)
{
	if (!user) {
		LOG_ERR("empty user");
		return -1;
	}

	LOG_DBG("+:%s\n", __func__);

	mutex_lock(&user->data_mutex);
	list_add_tail(vlist_link(cmd, struct ccu_cmd_s),
		&user->enque_ccu_cmd_list);
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
	long ret;
	struct ccu_cmd_s *cmd;

	/* wait until condition is true */
	ret = wait_event_interruptible_timeout(user->deque_wait,
					!list_empty(&user->deque_ccu_cmd_list),
					msecs_to_jiffies(3 * 1000));

	/* ret == 0, if timeout; ret == -ERESTARTSYS, if signal interrupt */
	if (ret == 0) {
		LOG_ERR("timeout: pop a command! ret=%ld\n", ret);
		*rcmd = NULL;
		return -1;
	} else if (ret < 0) {
		LOG_ERR("interrupted by system signal: %ld\n", ret);

		if (ret == -ERESTARTSYS)
			LOG_ERR("interrupted as -ERESTARTSYS\n");

		return ret;
	}
	mutex_lock(&user->data_mutex);
	/* This part should not be happened */
	if (list_empty(&user->deque_ccu_cmd_list)) {
		mutex_unlock(&user->data_mutex);
		LOG_ERR("pop a command from empty queue! ret=%ld\n", ret);
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

	struct ccu_user_s *user = NULL;

	mutex_lock(&g_ccu_device->dev_mutex);

	LOG_INF_MUST("%s pid:%d tid:%d cnt:%d+\n",
		__func__, current->pid, current->tgid, _user_count);

	ret = ccu_create_user(&user);
	if (IS_ERR_OR_NULL(user)) {
		LOG_ERR("fail to create user\n");
		mutex_unlock(&g_ccu_device->dev_mutex);
		return -ENOMEM;
	}

	flip->private_data = user;
	_user_count++;

	if (_user_count > 1) {
		LOG_INF_MUST("%s clean legacy data flow-\n", __func__);
		ccu_force_powerdown();
	}

	LOG_INF_MUST("%s-\n",
		__func__);

	mutex_unlock(&g_ccu_device->dev_mutex);

	return ret;
}

#ifdef CONFIG_COMPAT
static long ccu_compat_ioctl(struct file *flip, unsigned int cmd,
	unsigned long arg)
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
		LOG_DBG("[IOCTL_DBG] struct ccu_working_buffer_s size: %zu\n",
			sizeof(struct ccu_working_buffer_s));
		LOG_DBG("[IOCTL_DBG] arg: %p\n", (void *)arg);
		LOG_DBG("[IOCTL_DBG] long size: %zu\n", sizeof(long));
		LOG_DBG("[IOCTL_DBG] long long: %zu\n", sizeof(long long));
		LOG_DBG("[IOCTL_DBG] char *size: %zu\n", sizeof(char *));
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
			err |= get_user(uptr_Addr32,
				(&ptr_power32->workBuf.va_log[i]));
			err |= put_user(compat_ptr(uptr_Addr32),
				(&ptr_power64->workBuf.va_log[i]));
			err |= get_user(uint_Data32,
				(&ptr_power32->workBuf.mva_log[i]));
			err |= copy_to_user(
				&(ptr_power64->workBuf.mva_log[i]),
				&uint_Data32,
				sizeof(uint_Data32));
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

		ret = flip->f_op->unlocked_ioctl(flip, cmd,
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
		LOG_ERR("fail, cmd(%d), pid(%d), ",
			cmd, user->open_pid);
		LOG_ERR("(process, pid, tgid)=(%s, %d, %d)\n",
			current->comm, current->pid, current->tgid);
	}
	return ret;
}
#endif

static int ccu_alloc_command(struct ccu_cmd_s **rcmd)
{
	struct ccu_cmd_s *cmd;

	cmd = kzalloc(sizeof(vlist_type(struct ccu_cmd_s)), GFP_KERNEL);
	if (cmd == NULL) {
		LOG_ERR("%s(), node=0x%p\n", __func__, cmd);
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

	LOG_DBG_MUST("%s %d.\n", __func__, _clk_count);
	mutex_lock(&g_ccu_device->clk_mutex);

	_clk_count++;
	ret = clk_prepare_enable(ccu_clk_pwr_ctrl[0]);
	if (ret)
		LOG_ERR("CAM_PWR enable fail.\n");
	ret = clk_prepare_enable(ccu_clk_pwr_ctrl[1]);
	if (ret)
		LOG_ERR("CCU_CLK_CAM_CCU enable fail.\n");

	mutex_unlock(&g_ccu_device->clk_mutex);
	if (ret)
		LOG_ERR("clock enable fail.\n");

	return ret;
}

void ccu_clock_disable(void)
{
	LOG_DBG_MUST("%s %d.\n", __func__, _clk_count);
	mutex_lock(&g_ccu_device->clk_mutex);
	if (_clk_count > 0) {
		clk_disable_unprepare(ccu_clk_pwr_ctrl[1]);
		clk_disable_unprepare(ccu_clk_pwr_ctrl[0]);
		_clk_count--;
	}
	mutex_unlock(&g_ccu_device->clk_mutex);
}

static MBOOL _is_fast_cmd(enum ccu_msg_id msg_id)
{
	if ((msg_id == MSG_TO_CCU_SET_SKIP_ALGO) ||
		(msg_id == MSG_TO_CCU_SET_AP_AE_FRAME_SYNC_DATA) ||
		(msg_id == MSG_TO_CCU_GET_CCU_OUTPUT) ||
		(msg_id == MSG_TO_CCU_SET_AP_AE_OUTPUT) ||
		(msg_id == MSG_TO_CCU_SET_AE_ROI) ||
		(msg_id == MSG_TO_CCU_SET_AE_EV)) {
		return MTRUE;
	} else {
		return MFALSE;
	}

}

static long ccu_ioctl(struct file *flip, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int i = 0;
	struct CCU_WAIT_IRQ_STRUCT IrqInfo;
	struct ccu_user_s *user = flip->private_data;

	LOG_DBG("%s+, cmd:%d\n", __func__, cmd);

	if (cmd != CCU_IOCTL_WAIT_IRQ)
		mutex_lock(&g_ccu_device->dev_mutex);

	if ((cmd != CCU_IOCTL_SET_POWER) &&
		(cmd != CCU_IOCTL_FLUSH_LOG) &&
		(cmd != CCU_IOCTL_WAIT_IRQ)) {
		ret = ccu_query_power_status();
		if (ret == 0) {
			LOG_WARN("ccuk: ioctl without powered on\n");
			mutex_unlock(&g_ccu_device->dev_mutex);
			return -EFAULT;
		}
	}

	switch (cmd) {
	case CCU_IOCTL_SET_POWER:
		{
			LOG_DBG("ccuk: ioctl set powerk+\n");
			ret = copy_from_user(&power, (void *)arg,
				sizeof(struct ccu_power_s));
			if (ret != 0) {
				LOG_ERR(
					"[%s] copy_from_user failed, ret=%d\n",
					"SET_POWER", ret);
				mutex_unlock(&g_ccu_device->dev_mutex);
				return -EFAULT;
			}
			ret = ccu_set_power(&power);
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

			/*allocate ccu_cmd_st_list instead of struct ccu_cmd_s*/
			ret = ccu_alloc_command(&cmd);
			if (ret != 0) {
				LOG_ERR(
					"[%s] ccu_alloc_command failed, ret=%d\n",
					"ENQUE_COMMAND", ret);
				mutex_unlock(&g_ccu_device->dev_mutex);
				return -EFAULT;
			}
			ret = copy_from_user(cmd, (void *)arg,
				sizeof(struct ccu_cmd_s));
			if (ret != 0) {
				LOG_ERR(
					"[%s] copy_from_user failed, ret=%d\n",
					"ENQUE_COMMAND", ret);
				mutex_unlock(&g_ccu_device->dev_mutex);
				return -EFAULT;
			}
			if (_is_fast_cmd(cmd->task.msg_id) == MTRUE) {
				LOG_DBG("CCU_FAST_CMD_E.%d\n",
					cmd->task.msg_id);
				ret = ccu_kenrel_fast_cmd_enque(cmd);
			} else {
				ret = ccu_push_command_to_queue(user, cmd);
			}
			break;
		}
	case CCU_IOCTL_DEQUE_COMMAND:
		{
			struct ccu_cmd_s *cmd = 0;

			cmd = ccu_kenrel_fast_cmd_deque();
			if (cmd != NULL) {
				LOG_DBG("CCU_FAST_CMD_D.%d\n",
					cmd->task.msg_id);
				ret = 0;
			} else {
				ret = ccu_pop_command_from_queue(user, &cmd);
			}
			if (ret != 0) {
				LOG_ERR(
					"[%s] pop command failed, ret=%d\n",
					"DEQUE_COMMAND", ret);
				mutex_unlock(&g_ccu_device->dev_mutex);
				return -EFAULT;
			}

			ret = copy_to_user((void *)arg, cmd,
				sizeof(struct ccu_cmd_s));
			if (ret != 0) {
				LOG_ERR(
					"[%s] copy_to_user failed, ret=%d\n",
					"DEQUE_COMMAND", ret);
				ret = -EFAULT;
			}

			ccu_free_command(cmd);
			break;
		}
	case CCU_IOCTL_FLUSH_COMMAND:
		{
			ret = ccu_flush_commands_from_queue(user);
			if (ret != 0) {
				LOG_ERR(
					"[%s] flush command failed, ret=%d\n",
					"FLUSH_COMMAND", ret);
				mutex_unlock(&g_ccu_device->dev_mutex);
				return -EFAULT;
			}

			break;
		}
	case CCU_IOCTL_WAIT_IRQ:
		{
			if (copy_from_user(&IrqInfo, (void *)arg,
				sizeof(struct CCU_WAIT_IRQ_STRUCT)) == 0) {
				if ((IrqInfo.Type >= CCU_IRQ_TYPE_AMOUNT) ||
					(IrqInfo.Type < 0)) {
					ret = -EFAULT;
					LOG_ERR("invalid type(%d)\n",
						IrqInfo.Type);
					goto EXIT;
				}

				LOG_DBG("IRQ type(%d), userKey(%d), ",
					IrqInfo.Type,
					IrqInfo.EventInfo.UserKey);
				LOG_DBG("timeout(%d), sttype(%d), st(%d)\n",
					IrqInfo.EventInfo.Timeout,
					IrqInfo.EventInfo.St_type,
					IrqInfo.EventInfo.Status);

				ret = ccu_waitirq(&IrqInfo);

				if (copy_to_user((void *)arg, &IrqInfo,
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
	case CCU_IOCTL_FLUSH_LOG:
		{
			ccu_flushLog(0, NULL);
			break;
		}
	case CCU_IOCTL_GET_I2C_DMA_BUF_ADDR:
		{
			uint32_t addr[4];

			ret = ccu_get_i2c_dma_buf_addr(&addr[0], &addr[1],
				&addr[2], &addr[3]);

			if (ret != 0) {
				LOG_DBG(
					"ccu_get_i2c_dma_buf_addr fail: %d\n",
					ret);
				break;
			}

			ret = copy_to_user((void *)arg,
				addr, sizeof(uint32_t) * 4);

			break;
		}
	case CCU_IOCTL_SET_I2C_MODE:
		{
			struct ccu_i2c_arg i2c_arg;

			ret = copy_from_user(&i2c_arg, (void *)arg,
				sizeof(struct ccu_i2c_arg));

			ret = ccu_i2c_ctrl(i2c_arg.i2c_write_id,
				i2c_arg.transfer_len);

			break;
		}
	case CCU_IOCTL_SET_I2C_CHANNEL:
	{
		uint32_t sensor_idx;

		ret = copy_from_user(&sensor_idx, (void *)arg,
			sizeof(uint32_t));

		ret = ccu_i2c_set_channel(sensor_idx);

		if (ret < 0)
			LOG_ERR("invalid i2c sensor_idx: %d\n", sensor_idx);

		break;
	}
	case CCU_IOCTL_GET_CURRENT_FPS:
		{
			int32_t current_fps = ccu_get_current_fps();

			ret = copy_to_user((void *)arg,
				&current_fps, sizeof(int32_t));

			break;
		}
	case CCU_IOCTL_GET_SENSOR_I2C_SLAVE_ADDR:
	{
		struct ccu_i2c_info sensor_info[IMGSENSOR_SENSOR_IDX_MAX_NUM];

		ccu_get_sensor_i2c_info(&sensor_info[0]);

		ret = copy_to_user((void *)arg, &sensor_info,
			sizeof(struct ccu_i2c_info) *
			IMGSENSOR_SENSOR_IDX_MAX_NUM);

		break;
	}

	case CCU_IOCTL_GET_SENSOR_NAME:
	{
		#define SENSOR_NAME_MAX_LEN 32

		char *sensor_names[IMGSENSOR_SENSOR_IDX_MAX_NUM];

		ccu_get_sensor_name(sensor_names);
		for (i = IMGSENSOR_SENSOR_IDX_MIN_NUM;
			i < IMGSENSOR_SENSOR_IDX_MAX_NUM ; i++){
			if (sensor_names[i] != NULL) {
				ret = copy_to_user(((char *)arg+
					SENSOR_NAME_MAX_LEN*i),
				sensor_names[i], strlen(sensor_names[i])+1);
				if (ret != 0) {
					LOG_ERR(
					"copy_to_user failed: %d\n", ret);
					break;
				}
			}
		}
		#undef SENSOR_NAME_MAX_LEN
		break;
	}

	case CCU_IOCTL_PRINT_REG:
	{
		uint32_t *Reg;

		Reg = kzalloc(sizeof(uint8_t)*
			(CCU_HW_DUMP_SIZE+CCU_DMEM_SIZE+CCU_PMEM_SIZE),
			GFP_KERNEL);
		if (!Reg) {
			LOG_ERR(
			"CCU_IOCTL_PRINT_REG alloc failed\n");
			break;
		}
		ccu_print_reg(Reg);
		ret = copy_to_user((char *)arg,
			Reg, sizeof(uint8_t)*
			(CCU_HW_DUMP_SIZE+CCU_DMEM_SIZE+CCU_PMEM_SIZE));
		if (ret != 0) {
			LOG_ERR(
			"CCU_IOCTL_PRINT_REG copy_to_user failed: %d\n", ret);
		}
		kfree(Reg);
		break;
	}

	case CCU_IOCTL_PRINT_SRAM_LOG:
	{
		char *sram_log;

		sram_log = kzalloc(sizeof(char)*
			(CCU_LOG_SIZE*2+CCU_ISR_LOG_SIZE),
			GFP_KERNEL);
		if (!sram_log) {
			LOG_ERR(
			"CCU_IOCTL_PRINT_SRAM_LOG alloc failed\n");
			break;
		}
		ccu_print_sram_log(sram_log);
		ret = copy_to_user((char *)arg,
			sram_log, sizeof(char)*
			(CCU_LOG_SIZE*2+CCU_ISR_LOG_SIZE));
		if (ret != 0) {
			LOG_ERR(
			"CCU_IOCTL_PRINT_SRAM_LOG copy_to_user failed: %d\n", ret);
		}
		kfree(sram_log);
		break;
	}

	case CCU_IOCTL_LOAD_CCU_BIN:
	{
		ret = ccu_load_bin(g_ccu_device);
		break;
	}

	default:
		LOG_WARN("ioctl:No such command!\n");
		ret = -EINVAL;
		break;
	}

EXIT:
	if (ret != 0) {
		LOG_ERR("fail, cmd(%d), pid(%d), ",
			cmd, user->open_pid);
		LOG_ERR("(process, pid, tgid)=(%s, %d, %d)\n",
			current->comm, current->pid, current->tgid);
	}
	if (cmd != CCU_IOCTL_WAIT_IRQ)
		mutex_unlock(&g_ccu_device->dev_mutex);
	return ret;
}

static int ccu_release(struct inode *inode, struct file *flip)
{
	struct ccu_user_s *user = flip->private_data;

	mutex_lock(&g_ccu_device->dev_mutex);

	LOG_INF_MUST("%s pid:%d tid:%d cnt:%d+\n",
		__func__, user->open_pid, user->open_tgid, _user_count);
	ccu_delete_user(user);
	_user_count--;

	if (_user_count > 0) {
		LOG_INF_MUST("%s bypass release flow-", __func__);
		mutex_unlock(&g_ccu_device->dev_mutex);
		return 0;
	}

	ccu_force_powerdown();

	LOG_INF_MUST("%s -", __func__);

	mutex_unlock(&g_ccu_device->dev_mutex);

	return 0;
}


/**************************************************************************
 *
 **************************************************************************/
static int ccu_mmap(struct file *flip, struct vm_area_struct *vma)
{
	unsigned long length = 0;
	unsigned long pfn = 0x0;

	length = (vma->vm_end - vma->vm_start);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	pfn = vma->vm_pgoff << PAGE_SHIFT;

	LOG_DBG("CCU_mmap: vm_pgoff(0x%lx),pfn(0x%lx),phy(0x%lx), ",
		vma->vm_pgoff, pfn, vma->vm_pgoff << PAGE_SHIFT);
	LOG_DBG("vm_start(0x%lx),vm_end(0x%lx),length(0x%lx)\n",
		vma->vm_start, vma->vm_end, length);

	/*if (pfn >= CCU_REG_BASE_HW) {*/

	if (pfn == (ccu_hw_base - CCU_HW_OFFSET)) {
		if (length > PAGE_SIZE) {
			LOG_ERR("mmap range error :module(0x%lx), ",
				pfn);
			LOG_ERR
			    ("length(0x%lx), CCU_HW_BASE(0x%x)!\n",
			    length, 0x4000);
			return -EAGAIN;
		}
	} else if (pfn == CCU_CAMSYS_BASE) {
		if (length > CCU_CAMSYS_SIZE) {
			LOG_ERR("mmap range error :module(0x%lx), ",
				pfn);
			LOG_ERR
			    ("length(0x%lx), %s(0x%x)!\n",
			    length, "CCU_CAMSYS_BASE_HW", 0x4000);
			return -EAGAIN;
		}
	} else if (pfn == CCU_PMEM_BASE) {
		if (length > CCU_PMEM_SIZE) {
			LOG_ERR("mmap range error :module(0x%lx), ",
				pfn);
			LOG_ERR
			    ("length(0x%lx), CCU_PMEM_BASE_HW(0x%x)!\n",
			    length, 0x4000);
			return -EAGAIN;
		}
	} else if (pfn == CCU_DMEM_BASE) {
		if (length > CCU_DMEM_SIZE) {
			LOG_ERR("mmap range error :module(0x%lx), ",
				pfn);
			LOG_ERR
			    ("length(0x%lx), CCU_PMEM_BASE_HW(0x%x)!\n",
			    length, 0x4000);
			return -EAGAIN;
		}
	} else {
		LOG_ERR("Illegal starting HW addr for mmap!\n");
		return -EAGAIN;
	}

	if (remap_pfn_range
	    (vma, vma->vm_start, vma->vm_pgoff,
		 vma->vm_end - vma->vm_start,
	     vma->vm_page_prot)) {
		LOG_ERR("remap_pfn_range\n");
		return -EAGAIN;
	}
	LOG_DBG("map_check_1\n");

	/*
	 * } else {
	 *	LOG_DBG("map_check_2\n");
	 *	return ccu_mmap_hw(flip, vma);
	 *}
	 */

	return 0;
}

/**************************************************************************
 *
 **************************************************************************/
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

/**************************************************************************
 * platform_driver
 **************************************************************************/

static int ccu_read_platform_info_from_dt(struct device_node *node)
{
	uint32_t reg[4] = {0, 0, 0, 0};
	int ret = 0;

	ret = of_property_read_u32_array(node, "reg", reg, 4);
	if (ret < 0)
		LOG_ERR("of_property_read_u32_array ERR : %d\n", ret);

	ccu_hw_base = reg[1];

	LOG_DBG("ccu read dt property ccu_hw_base = %x\n", ccu_hw_base);
	LOG_DBG("CCU_HW_OFFSET = %x\n", CCU_HW_OFFSET);
	LOG_DBG("CCU_PMEM_BASE = %x\n", CCU_PMEM_BASE);
	LOG_DBG("CCU_PMEM_SIZE = %x\n", CCU_PMEM_SIZE);
	LOG_DBG("CCU_DMEM_BASE = %x\n", CCU_DMEM_BASE);
	LOG_DBG("CCU_DMEM_SIZE = %x\n", CCU_DMEM_SIZE);
	LOG_DBG("CCU_DMEM_OFFSET = %x\n", CCU_DMEM_OFFSET);
	LOG_DBG("CCU_LOG_BASE = %x\n", CCU_LOG_BASE);
	LOG_DBG("CCU_LOG_SIZE = %x\n", CCU_LOG_SIZE);
	LOG_DBG("CCU_HW_DUMP_SIZE = %x\n", CCU_HW_DUMP_SIZE);
	LOG_DBG("CCU_CAMSYS_BASE = %x\n", CCU_CAMSYS_BASE);
	LOG_DBG("CCU_CAMSYS_SIZE = %x\n", CCU_CAMSYS_SIZE);
	LOG_DBG("CCU_N3D_A_BASE = %x\n", CCU_N3D_A_BASE);
	LOG_DBG("CCU_N3D_A_SIZE = %x\n", CCU_N3D_A_SIZE);
	LOG_DBG("CCU_SENSOR_PM_SIZE = %x\n", CCU_SENSOR_PM_SIZE);
	LOG_DBG("CCU_SENSOR_DM_SIZE = %x\n", CCU_SENSOR_DM_SIZE);

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

		/* get physical address of pmem  */
		/* ioremap_wc() has no access 4 bytes alignment limitation
		 * as of_iomap() does?
		 * https://forums.xilinx.com/xlnx/attachments/
		 * xlnx/ELINUX/11158/1/Linux%20CPU%20to%20PL%20Access.pdf
		 */
		{
			/*remap ccu_base*/
			phy_addr = ccu_hw_base;
			phy_size = 0x1000;
			g_ccu_device->ccu_base =
				(unsigned long)ioremap_wc(phy_addr, phy_size);
			LOG_INF("ccu_base pa: 0x%x, size: 0x%x\n",
				phy_addr, phy_size);
			LOG_INF("ccu_base va: 0x%lx\n",
				g_ccu_device->ccu_base);

			/*remap dmem_base*/
			phy_addr = CCU_DMEM_BASE;
			phy_size = CCU_DMEM_SIZE;
			g_ccu_device->dmem_base =
				(unsigned long)ioremap_wc(phy_addr, phy_size);
			LOG_INF("dmem_base pa: 0x%x, size: 0x%x\n",
				phy_addr, phy_size);
			LOG_INF("dmem_base va: 0x%lx\n",
				g_ccu_device->dmem_base);

			/*remap pmem_base*/
			phy_addr = CCU_PMEM_BASE;
			phy_size = CCU_PMEM_SIZE;
			g_ccu_device->pmem_base =
				(unsigned long)ioremap_wc(phy_addr, phy_size);
			LOG_INF("pmem_base pa: 0x%x, size: 0x%x\n",
				phy_addr, phy_size);
			LOG_INF("pmem_base va: 0x%lx\n",
				g_ccu_device->pmem_base);

			/*remap camsys_base*/
			phy_addr = CCU_CAMSYS_BASE;
			phy_size = CCU_CAMSYS_SIZE;
			g_ccu_device->camsys_base =
				(unsigned long)ioremap_wc(phy_addr, phy_size);
			LOG_INF("camsys_base pa: 0x%x, size: 0x%x\n",
				phy_addr, phy_size);
			LOG_INF("camsys_base va: 0x%lx\n",
				g_ccu_device->camsys_base);

			/*remap n3d_a_base*/
			phy_addr = CCU_N3D_A_BASE;
			phy_size = CCU_N3D_A_SIZE;
			g_ccu_device->n3d_a_base =
				(unsigned long)ioremap_wc(phy_addr, phy_size);
			LOG_INF("n3d_a_base pa: 0x%x, size: 0x%x\n",
				phy_addr, phy_size);
			LOG_INF("n3d_a_base va: 0x%lx\n",
				g_ccu_device->n3d_a_base);

		}
		/* get Clock control from device tree.  */
		{
			ccu_clk_pwr_ctrl[0] =
				devm_clk_get(g_ccu_device->dev,
					"CAM_PWR");
			if (ccu_clk_pwr_ctrl[0] == NULL)
				LOG_ERR("Get CAM_PWR fail.\n");
			ccu_clk_pwr_ctrl[1] =
				devm_clk_get(g_ccu_device->dev,
					"CCU_CLK_CAM_CCU");
			if (ccu_clk_pwr_ctrl[1] == NULL)
				LOG_ERR("Get CCU_CLK_CAM_CCU fail.\n");
		}
		/**/
		g_ccu_device->irq_num = irq_of_parse_and_map(node, 0);
		LOG_DBG("probe 1, ccu_base: 0x%lx, bin_base: 0x%lx, ",
			g_ccu_device->ccu_base, g_ccu_device->bin_base);
		LOG_DBG("irq_num: %d, pdev: %p\n",
			g_ccu_device->irq_num, g_ccu_device->dev);

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
			LOG_DBG("No IRQ!!: ccu_num_devs=%d, ",
				ccu_num_devs);
			LOG_DBG("devnode(%s), irq=%d\n",
				g_ccu_device->dev->of_node->name,
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
				LOG_ERR(
					"Unable to create class, err = %d\n",
					ret);
				goto EXIT;
			}

			dev = device_create(ccu_class,
				NULL, ccu_devt, NULL, CCU_DEV_NAME);
			if (IS_ERR(dev)) {
				ret = PTR_ERR(dev);
				LOG_DERR(g_ccu_device->dev,
					"Failed to create device: /dev/%s, err = %d",
					CCU_DEV_NAME, ret);
				goto EXIT;
			}
#ifdef CONFIG_PM_SLEEP
// wakeup_source_init(&ccu_wake_lock, "ccu_lock_wakelock");
#endif

			/* enqueue/dequeue control in ihalpipe wrapper */
			init_waitqueue_head(&wait_queue_deque);
			init_waitqueue_head(&wait_queue_enque);

			/*for (i = 0; i < CCU_IRQ_NUM_TYPES; i++) {*/
			/*      tasklet_init(ccu_tasklet[i].pCCU_tkt,*/
			/*			ccu_tasklet[i].tkt_cb, 0);*/
			/*}*/

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

		LOG_INF_MUST("ccu probe cuccess...\n");

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

/**************************************************************************
 *
 **************************************************************************/
static int __init CCU_INIT(void)
{
	int ret = 0;
	/*struct device_node *node = NULL;*/

	g_ccu_device = kzalloc(sizeof(struct ccu_device_s), GFP_KERNEL);
	/*g_ccu_device = dma_cache_coherent();*/

	INIT_LIST_HEAD(&g_ccu_device->user_list);
	mutex_init(&g_ccu_device->user_mutex);
	mutex_init(&g_ccu_device->dev_mutex);
	mutex_init(&g_ccu_device->clk_mutex);
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


/**************************************************************************
 *
 **************************************************************************/
module_init(CCU_INIT);
module_exit(CCU_EXIT);
MODULE_DESCRIPTION("MTK CCU Driver");
MODULE_AUTHOR("SW1");
MODULE_LICENSE("GPL");
