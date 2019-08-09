/*
 * Copyright (C) 2018 MediaTek Inc.
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <asm/mman.h>
#include <linux/mm.h>
#include <linux/dmapool.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/wait.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include "edmc_debug.h"
#include "edmc_dvfs.h"
#include <linux/workqueue.h>
#include <linux/timer.h>

#define DRIVER_NAME "mtk_edmc"

#include <linux/interrupt.h>
#include "edmc_hw_reg.h"
#include "edmc_ioctl.h"
#include <linux/spinlock.h>

#define  DEVICE_NAME "edmcctl"
#define  CLASS_NAME  "edmc"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yu-Ren, Wang");
MODULE_DESCRIPTION("EDMC driver");
MODULE_VERSION("0.1");


static int majorNumber;
static int numberOpens;
static struct class *edmcctlClass;
static struct device *edmcctlDevice;
//static struct platform_device *edmcctlPlatformDevice;

//eDMC variable
static void *edmc_ctrl_top;

u64 g_edmc_seq_job; //wait id
u64 g_edmc_seq_finish;
u64 g_edmc_seq_last; //Hw now id
u64 g_edmc_seq_error;
u64 cmd_list_len; //HW queue
u32 edmc_power_on;
static struct work_struct edmc_error_handle_wq;
static int g_irq_num;

u32 g_edmc_log_level;

struct work_struct edmc_queue;
struct work_struct edmc_power_off_queue;
struct work_struct edmc_polling_queue;

#ifdef ERROR_TRIGGER_TEST
static bool g_trigger_next_error;
static bool g_emdc_irq_disabled;
#endif


static DEFINE_MUTEX(cmd_list_mutex);
static LIST_HEAD(cmd_list);

static DECLARE_WAIT_QUEUE_HEAD(edmc_cmd_queue);
static DECLARE_WAIT_QUEUE_HEAD(service_wait);
//static DEFINE_SPINLOCK(service_lock);

static DEFINE_MUTEX(edmc_mutex);
static DEFINE_SPINLOCK(edmc_lock);

#define GETBIT(x, y) (((x) >> (y)) & 0x1)
#define EDMC_MAX_DESCRIPTOR              4
#define EDMC_DESP_FULL_POLLING_TIME      100 // ms
#define EDMC_DESP_FULL_POLLING_COUNT     10

struct mtk_edmc_drvdata {
	int irq;
	unsigned long mem_start;
	unsigned long mem_end;
	void __iomem *base_addr;
};
struct edmc_command_entry {
	struct list_head list;
	struct ioctl_edmc_descript descript;
	u8 op_mode;
};

static int edmc_probe(struct platform_device *pdev);
static int edmc_remove(struct platform_device *pdev);

static int edmc_open(struct inode *, struct file *);
static int edmc_release(struct inode *, struct file *);
static long edmc_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg);
#ifdef CONFIG_COMPAT
static long compat_edmc_ioctl(struct file *file, unsigned int command,
	unsigned long arg);
#endif
static void edmc_reg_write(u32 value, u32 offset);
static int edmc_wait_command(u64 wait_id);
static int edmc_enable_clk(bool enable);
static int emdc_enable_power(bool enable);

static int edmc_reset(void);
static int edmc_toggle_reset(void);
static void edmc_trigger(void);
static void edmc_dump_edm_reg(void);
static void edmc_dump_desp_reg(void);
static void edmc_dump_reg(u32 write_ptr);
static void edmc_copy(struct ioctl_edmc_descript *pdescript, u8 op_mode,
			u32 write_ptr);
static int edmc_get_write_ptr(u32 *write_ptr);
static int edmc_enque_command(struct ioctl_edmc_descript *descript,
						u8 op_mode);
static int edmc_deque_command(u32 write_ptr);
static bool edmc_is_queue_empty(void);
static bool edmc_is_desp_valid(struct ioctl_edmc_descript *descript);

#ifdef CONFIG_MTK_EDMC_ION
static int edmc_enable_ion(bool enable);
#endif
static bool edmc_get_clk(void);

static int edmc_fifo_in(void);
static int edmc_fifo_out(void);
static int edmc_run_command_async(struct ioctl_edmc_descript *descript,
		u8 op_mode);
static inline bool edmc_is_fifo_empty(void);

static void edmc_start_queue(struct work_struct *work);
static void edmc_start_power_off(struct work_struct *work);

static void edmc_start_polling(struct work_struct *work);

static void edmc_power_timeup(unsigned long data);

#define EDMC_POWEROFF_TIME_DEFAULT 2000 /* ms */
u32 g_edmc_poweroff_time = EDMC_POWEROFF_TIME_DEFAULT;
static DEFINE_TIMER(edmc_power_timer, edmc_power_timeup, 0, 0);

#if 0
static void edmc_get_status(void);
static bool edmc_is_hw_empty(void);
static bool edmc_is_dequeue_ready(void);
static bool edmc_is_hw_free(void);
static bool edmc_is_hw_full(void);
static void edmc_power_off(void);
#endif

static void edmc_clear_cmd_queue(void);
static void edmc_error_handle(struct work_struct *work);

#define EDMC_TAG "[eDMC]"

#define LOG_DBG(fmt, args...) do { if (g_edmc_log_level >= 2) {\
	pr_info(EDMC_TAG " " fmt, ##args); } \
	} while (0)

#define LOG_INF(format, args...) do { if (g_edmc_log_level >= 1) {\
	pr_info(EDMC_TAG " " format, ##args); } \
	} while (0)
#define LOG_WRN(format, args...) pr_info(EDMC_TAG "[warn] " format, ##args)
#define LOG_ERR(format, args...) pr_info(EDMC_TAG "[err] " format, ##args)

#if 0 //Debug for super mode
#define LOG_SUP(format, args...) do { if (g_edmc_log_level == 0) {\
	pr_info(EDMC_TAG " " format, ##args); } \
	} while (0)
#else
#define LOG_SUP(format, args...)
#endif

static const struct file_operations fops = {
	.open = edmc_open,
	.unlocked_ioctl = edmc_ioctl,
	.release = edmc_release,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_edmc_ioctl,
#endif
};

unsigned int edmc_reg_read(u32 offset)
{
	return ioread32(edmc_ctrl_top + offset);
}

static void edmc_reg_write(u32 value, u32 offset)
{
	iowrite32(value, edmc_ctrl_top + offset);
}

static void edmc_start_queue(struct work_struct *work)
{

	u32 write_ptr = 0;

	LOG_INF("[%s]\n", __func__);

	mutex_lock(&cmd_list_mutex);

	while (g_edmc_seq_finish > g_edmc_seq_last) {
		LOG_INF("[%s] finish(%.8llx) last(%.8llx)\n"
				, __func__, g_edmc_seq_finish, g_edmc_seq_last);
		edmc_fifo_out();
		if (!edmc_is_queue_empty()) {
			LOG_DBG("[%s] Queue Ready\n", __func__);
			if (edmc_fifo_in()) {
				if (!edmc_get_write_ptr(&write_ptr))
					edmc_deque_command(write_ptr);
			} else {
				LOG_DBG("[%s] INT is no return\n", __func__);
			}

		}
	}

	if (edmc_is_fifo_empty()) {
		if (g_edmc_poweroff_time) {
			LOG_DBG("%s: start power_timer(%u)\n",
					__func__, g_edmc_poweroff_time);
			mod_timer(&edmc_power_timer,
				jiffies +
				msecs_to_jiffies(g_edmc_poweroff_time));
		}
	}

	LOG_DBG("[%s] All Done\n", __func__);

	mutex_unlock(&cmd_list_mutex);

	wake_up_interruptible(&edmc_cmd_queue);

}

static void edmc_error_handle(struct work_struct *work)
{

	LOG_DBG("[%s] APU_EDM_ERR_STATUS :[%.8x]\n", __func__,
		edmc_reg_read(APU_EDM_ERR_STATUS));
	LOG_DBG("[%s]g_edmc_seq_error = %llu\n", __func__, g_edmc_seq_error);
	LOG_DBG("[%s]g_edmc_seq_finish = %llu\n", __func__, g_edmc_seq_finish);
	LOG_DBG("[%s]g_edmc_seq_job = %llu\n", __func__, g_edmc_seq_job);
	LOG_DBG("[%s]g_edmc_seq_last = %llu\n", __func__, g_edmc_seq_last);
	LOG_DBG("[%s]cmd_list_len = %llu\n", __func__, cmd_list_len);

	//1.reset hardware 2.clear cmd queue
	//3. wake up waiters and return false
	disable_irq(g_irq_num);
	edmc_reset();

	mutex_lock(&cmd_list_mutex);
	edmc_clear_cmd_queue();
	g_edmc_seq_last = g_edmc_seq_finish = g_edmc_seq_error = g_edmc_seq_job;
	cmd_list_len = 0;
	mutex_unlock(&cmd_list_mutex);

	enable_irq(g_irq_num);
	//wake up the waitors
	wake_up_interruptible(&edmc_cmd_queue);

	//schedule_work(&edmc_queue);

	LOG_DBG("[%s] end\n", __func__);
}

static void edmc_start_polling(struct work_struct *work)
{
	unsigned long flags;
	unsigned int reg_edm_cfg;

	//Polling DMA_BUS_IDLE
	LOG_DBG("[%s]\n", __func__);
	while (1) {
		reg_edm_cfg = edmc_reg_read(APU_EDM_CFG_0);

		if (GETBIT(reg_edm_cfg, 3) == 0) {
			usleep_range(1000, 2000);
			LOG_DBG("[%s] sleep 1ms..\n", __func__);
		} else {
			// DMA finish
			LOG_DBG("[%s] DMA finish\n", __func__);
			spin_lock_irqsave(&edmc_lock, flags);
			//Finish Job Increment
			g_edmc_seq_finish = g_edmc_seq_finish + 1;
			spin_unlock_irqrestore(&edmc_lock, flags);

			schedule_work(&edmc_queue);

			break;
		}

	}



	LOG_DBG("[%s] end\n", __func__);
}
static void edmc_power_timeup(unsigned long data)
{
	schedule_work(&edmc_power_off_queue);
}

static void edmc_start_power_off(struct work_struct *work)
{
	LOG_INF("[%s]\n", __func__);

	mutex_lock(&cmd_list_mutex);
	if (edmc_is_fifo_empty() && edmc_is_queue_empty()) {
		//Disable clk
		edmc_enable_clk(false);
		//Disable power
		emdc_enable_power(false);
		//edmc_power_on = 0;
	}
	mutex_unlock(&cmd_list_mutex);

}

#if 1
static irqreturn_t edmc_interrupt(int irq, void *dev_id)
{

	unsigned int reg_int_status, reg_err_int_status, reg_err_status;
	unsigned int reg_edm_cfg;
	u8 interrupt_status = 0;
	u8 interrupt_bit = 0;
	unsigned long flags;
	int count = 0;

	LOG_INF("[%s]\n", __func__);

	reg_edm_cfg = edmc_reg_read(APU_EDM_CFG_0);
	reg_err_int_status = edmc_reg_read(APU_EDM_ERR_INT_STATUS);
	while (GETBIT(reg_edm_cfg, 3) == 0) {
		//maybe false alarm
		reg_edm_cfg = edmc_reg_read(APU_EDM_CFG_0);
		count++;
		udelay(5);
		LOG_INF("Error Occur: is_fake\n");
		if (count >= 5000) {
			LOG_ERR("[%s] Polling Fail\n", __func__);
			break;
		}

	}
	reg_int_status = edmc_reg_read(APU_EDM_INT_STATUS);

#ifdef ERROR_TRIGGER_TEST
	if (reg_err_int_status != 0 || g_trigger_next_error) {
		//clear the int , or the interrupt will occur always
		edmc_reg_write(edmc_reg_read(APU_EDM_INT_STATUS),
				APU_EDM_INT_STATUS);
		/**
		 * in real error situation, interrupt will be stopped by
		 * hw when error happen, so we disable irq to simulate it
		 **/
		if (g_emdc_irq_disabled == false) {
			disable_irq_nosync(g_irq_num);
			g_emdc_irq_disabled = true;
		}
		LOG_DBG("[%s] reg_err_int_status=%d,g_trigger_next_error=%d\n",
			__func__, reg_err_int_status, g_trigger_next_error);
#else
	if (reg_err_int_status != 0) { // Error handler
#endif
		LOG_INF("eDMC Error Occur!\n");
		reg_err_status = edmc_reg_read(APU_EDM_ERR_STATUS);
		LOG_DBG("APU_EDM_ERR_STATUS :[%.8x]\n", reg_err_status);

		schedule_work(&edmc_error_handle_wq);
		//edmc_reset();

		//Error Job Update
		//spin_lock_irqsave(&edmc_lock, flags);
		//g_edmc_seq_error = g_edmc_seq_job;
		//spin_unlock_irqrestore(&edmc_lock, flags);

	} else {
		//Normal mode

		edmc_toggle_reset();

		interrupt_status = (reg_int_status >> 16) & 0xF;

		//// clear All DESP DONE IRQ status
		edmc_reg_write(reg_int_status | (interrupt_status << 16),
			       APU_EDM_INT_STATUS);

		interrupt_bit = GETBIT(interrupt_status, 0) +
						GETBIT(interrupt_status, 1) +
						GETBIT(interrupt_status, 2) +
						GETBIT(interrupt_status, 3);

		//Finish Job Increment
		spin_lock_irqsave(&edmc_lock, flags);
		g_edmc_seq_finish = g_edmc_seq_finish + (u64) interrupt_bit;
		spin_unlock_irqrestore(&edmc_lock, flags);

		LOG_DBG("[%s] interrupt_bit :[%.8x]\n"
				, __func__, interrupt_bit);
		LOG_DBG("[%s] g_edmc_seq_finish :[%.8llx]\n"
				, __func__, g_edmc_seq_finish);
		LOG_DBG("[%s] interrupt_status = %.4x\n"
				, __func__,	interrupt_status);

		schedule_work(&edmc_queue);
	}

	return IRQ_HANDLED;
}
#endif

static int edmc_fifo_in(void)
{
	int ret = 0;

	LOG_DBG("[%s]\n", __func__);

	switch (cmd_list_len) {
	case 0:
		cmd_list_len++;
		ret = 1;
		break;
#if 0 //Limit to one descriptor one time for SW work around
	case 1:
		cmd_list_len++;
		ret = 2;
		break;
	case 2:
		cmd_list_len++;
		ret = 3;
		break;
	case 3:
		cmd_list_len++;
		ret = 4;
		break;
#endif
	default:
		ret = 0;
		break;
	}

	LOG_DBG("[%s] cmd_list_len(%.8llx) ret(%.8x)\n",
			__func__, cmd_list_len, ret);
	return ret;
}

static int edmc_fifo_out(void)
{
	int ret = 0;

	LOG_DBG("[%s]\n", __func__);

	if (!cmd_list_len)
		LOG_DBG("[%s] FIFO is already empty\n", __func__);

	g_edmc_seq_last++;
	cmd_list_len--;

	ret = cmd_list_len;

	LOG_DBG("[%s] cmd_list_len(%.8llx) g_edmc_seq_last(%.8llx)\n",
			__func__, cmd_list_len, g_edmc_seq_last);
	return ret;
}

static inline bool edmc_is_fifo_empty(void)
{
	return (cmd_list_len == 0);
}

static bool edmc_is_desp_valid(struct ioctl_edmc_descript *descript)
{
	bool ret = true;

	if ((descript->src_tile_width > SRC_TILE_WIDTH_MAX) ||
		(descript->dst_tile_width > DST_TILE_WIDTH_MAX) ||
		(descript->tile_height > TILE_HEIGHT_MAX)) {
		ret = false;
	}
	if ((descript->user_desp_id != 0) ||
		(descript->despcript_id != 0) ||
		(descript->wait_id != 0)) {
		ret = false;
	}
	if ((descript->dst_tile_width == 0) ||
		(descript->tile_height == 0) ||
		(descript->dst_stride == 0)) {
		ret = false;
	}

	return ret;
}

static int edmc_run_command_async(struct ioctl_edmc_descript *descript,
		u8 op_mode)
{
	u32 write_ptr = 0;

	LOG_DBG("[%s]\n", __func__);

	if (!edmc_is_desp_valid(descript)) {
		LOG_DBG("[%s] Check Format InVaild\n", __func__);
		return -1;
	}

	mutex_lock(&cmd_list_mutex);
	//Update Output wait_id
	descript->wait_id = g_edmc_seq_job;
	descript->user_desp_id = descript->wait_id & 0xFF;
	//update seq_job
	g_edmc_seq_job++;


	//HW is available do copy, or eneque to sw queue
	if (edmc_fifo_in()) {
		if (!edmc_get_write_ptr(&write_ptr))
			edmc_copy(descript, op_mode, write_ptr);
	} else
		edmc_enque_command(descript, op_mode);

	mutex_unlock(&cmd_list_mutex);
	LOG_DBG("[%s]descript->wait_id = %llu\n", __func__, descript->wait_id);
	LOG_DBG("[%s] Done\n", __func__);
	return 0;
}
#if 0
static void edmc_get_status(void)
{
	unsigned int value_APU_EDM_CFG_0 = 0;
	unsigned int write_pointer, num_desp;
	unsigned int int_status;

	value_APU_EDM_CFG_0 = edmc_reg_read(APU_EDM_CFG_0);
	int_status = (edmc_reg_read(APU_EDM_INT_STATUS) >> 16) & 0xF;
	write_pointer = (value_APU_EDM_CFG_0 >> 4) & 0x3;
	num_desp = value_APU_EDM_CFG_0 & 0x7;

	LOG_DBG("[%s] write_pointer(%.8x) num_desp(%.8x) int_status(%.8x)\n"
			, __func__, write_pointer, num_desp, int_status);

}
static int edmc_run_command_sync(struct ioctl_edmc_descript *descript,
		u8 op_mode)
{
	u32 write_ptr = 0;
	unsigned int reg_edm_cfg;

	//if (edmc_fifo_in()) {
		if (!edmc_get_write_ptr(&write_ptr)) {
			edmc_copy(descript, op_mode, write_ptr);
			while (1) {
				reg_edm_cfg = edmc_reg_read(APU_EDM_CFG_0);
				if (GETBIT(reg_edm_cfg, 3) == 0) {
					udelay(5);
				} else {
					// DMA finish
					break;
				}
			}
		//	edmc_fifo_out();
		//}

	}

	return 0;
}

static bool edmc_is_dequeue_ready(void)
{
	bool is_ready = false;

	if ((!edmc_is_queue_empty()) && (!edmc_is_hw_full())) {
		is_ready = true;
		LOG_DBG("[%s] Ready!!\n", __func__);
	} else {
		LOG_DBG("[%s] Not Ready!!\n", __func__);
	}

	return is_ready;
}

//Developing
static void edmc_power_off(void)
{

	if ((edmc_is_hw_free()) && (edmc_is_queue_empty())) {
		LOG_DBG("[%s] HW/SW is free!!\n", __func__);
		//Disable clk
		edmc_enable_clk(false);
	} else {
		LOG_DBG("[%s] HW/SW is busy!!\n", __func__);
	}


}
static bool edmc_is_hw_empty(void)
{
	bool is_empty = true;
	unsigned int value = 0;
	u32 reg_write_ptr = 0, reg_num_descriptor = 0;

	value = edmc_reg_read(APU_EDM_CFG_0);

	//Update write_pointer and num_of_descriptor
	reg_num_descriptor = value & 0x7;
	reg_write_ptr = (value >> 4) & 0x3;

	LOG_DBG("[%s] write_ptr(%u) num_descriptor(%u)\n",
			__func__, reg_write_ptr, reg_num_descriptor);

	if (reg_num_descriptor > 0) {
		LOG_DBG("HW is not empty (%d)  !\n", reg_num_descriptor);
		is_empty = false;
	}

	return is_empty;
}


static bool edmc_is_hw_full(void)
{
	bool is_full = true;
	unsigned int value = 0;
	u32 reg_write_ptr = 0, reg_num_descriptor = 0;

	value = edmc_reg_read(APU_EDM_CFG_0);

	//Update write_pointer and num_of_descriptor
	reg_num_descriptor = value & 0x7;
	reg_write_ptr = (value >> 4) & 0x3;

	LOG_DBG("[%s] write_ptr = %u, num_descriptor = %u !\n",
			reg_write_ptr, reg_num_descriptor);

	if (reg_num_descriptor < EDMC_MAX_DESCRIPTOR) {
		LOG_DBG("Access eDMC OK (%d)  !\n", reg_num_descriptor);
		is_full = false;
	}

	return is_full;
}


static bool edmc_is_hw_free(void)
{
	bool is_free = true;
	unsigned int value = 0;
	u32 reg_write_ptr = 0, reg_num_descriptor = 0, reg_dma_idle = 0;

	value = edmc_reg_read(APU_EDM_CFG_0);

	//Update write_pointer and num_of_descriptor
	reg_num_descriptor = value & 0x7;
	reg_write_ptr = (value >> 4) & 0x3;
	reg_dma_idle = (value >> 6) & 0x1;

	LOG_DBG("[%s] write_ptr(%u) num_descriptor(%u) dma_idle(%u)\n",
			__func__, reg_write_ptr,
			reg_num_descriptor, reg_dma_idle);


	if (reg_num_descriptor > 0) {
		LOG_DBG("[%s] HW not empty [%d]!\n",
				__func__, reg_num_descriptor);
		is_free = false;
	} else if ((reg_num_descriptor == 0) && (reg_dma_idle == 0)) {
		// hw queue = 0 but need to excute current job
		LOG_DBG("[%s] HW still run [%d]  !\n", __func__, reg_dma_idle);
		is_free = false;
	}

	return is_free;
}
#endif
static bool edmc_is_queue_empty(void)
{
	bool is_empty = true;

	if (!list_empty(&cmd_list)) {
		is_empty = false;
		LOG_DBG("Queue eDMC Not empty !\n");
	}

	return is_empty;
}

static int edmc_deque_command(u32 write_ptr)
{
	struct edmc_command_entry *cmd;

	LOG_DBG("[%s]\n", __func__);

	if (!list_empty(&cmd_list))	{
		cmd = list_first_entry(&cmd_list,
				struct edmc_command_entry, list);
		list_del(&cmd->list);
		edmc_copy(&cmd->descript, cmd->op_mode, write_ptr);
		kfree(cmd);
		LOG_DBG("[%s] Deque Done\n", __func__);
	} else {
		LOG_DBG("[%s] empty by list_empty\n", __func__);
	}

	return 0;
}

static int edmc_enque_command(struct ioctl_edmc_descript *descript,
						u8 op_mode)
{
	struct edmc_command_entry *cmd =
		kmalloc(sizeof(struct edmc_command_entry), GFP_KERNEL);

	LOG_DBG("[%s]\n", __func__);

	if (cmd == NULL) {
		LOG_ERR("[%s] kmalloc fail\n", __func__);
		return -1;
	}


	memcpy(&cmd->descript, descript, sizeof(struct ioctl_edmc_descript));

	cmd->op_mode = op_mode;

	//update wait_id
	cmd->descript.user_desp_id = descript->wait_id & 0xFF;
	cmd->descript.wait_id = descript->wait_id;

	list_add_tail(&cmd->list, &cmd_list);

	return 0;
}

static void edmc_clear_cmd_queue(void)
{
	struct list_head *p;
	struct edmc_command_entry *cmd;

	LOG_DBG("edmc_clear_queue\n");
	list_for_each(p, &cmd_list) {
		cmd = list_entry(p, struct edmc_command_entry, list);
		list_del(p);
		kfree(cmd);
	}
	//BUG_ON(!list_empty(&cmd_list));

	LOG_DBG("edmc_clear_queue result, list_empty = %d\n",
		list_empty(&cmd_list));
}

static int edmc_wait_command(u64 wait_id)
{
	long ret = 0;
#if 0 //timeout mode
	u32 wait_event_timeouts = 0;
#endif

	LOG_INF("[%s] enter id:[%llx] g_edmc_seq_finish:[%llx]\n",
		__func__, wait_id, g_edmc_seq_finish);
	if (g_edmc_seq_finish > wait_id)
		LOG_DBG("[%s] g_edmc_seq_finish > wait_id\n", __func__);
	else
		LOG_DBG("[%s] g_edmc_seq_finish <= wait_id\n", __func__);
#if 1
#if 1	//Interrupt mode
	//ret = wait_event_interruptible(edmc_cmd_queue,
	//				g_edmc_seq_finish > wait_id);
	wait_event_interruptible(edmc_cmd_queue, g_edmc_seq_finish > wait_id);

#else //timeout mode
	wait_event_timeouts =
			msecs_to_jiffies(1000);
	ret = wait_event_interruptible_timeout(
			edmc_cmd_queue,
			g_edmc_seq_finish > wait_id, wait_event_timeouts);
#endif
	LOG_DBG("[%s] interrupt exit ret(%ld)\n", __func__, ret);

#else //Polling mode But wait_id is descript_id

	while (1) {
		value_edm_int_status = edmc_reg_read(APU_EDM_INT_STATUS);
		interrupt_status = (value_edm_int_status >> 16) & 0xF;
		LOG_DBG("value_edm_int_status :[%.8x]\n",
				value_edm_int_status);
		LOG_DBG("interrupt_status = %.4x\n",
				interrupt_status);
		if (((interrupt_status >> wait_id) & 0x1) > 0) {
			//// clear DESP DONE IRQ status
			edmc_reg_write(value_edm_int_status |
					(1 << (16 + wait_id)),
					APU_EDM_INT_STATUS);
			value_edm_int_status =
					edmc_reg_read(APU_EDM_INT_STATUS);
			LOG_DBG("value_edm_int_status :[%.8x]\n",
					value_edm_int_status);
			LOG_DBG("[%s] exit\n", __func__);
			goto exit;
		} else {
			mdelay(1);
			count++;
			LOG_DBG("despcript_id = %.4x\n", wait_id);
			LOG_DBG("interrupt_status = %.4x\n",
					interrupt_status);
			if (count > 100) {
				LOG_DBG("count > 100 exit\n");
				ret = -1;
				got exit;
			}
		}

	}
exit:
	LOG_DBG("[%s]  Polling exit\n", __func__);
#endif

	return ret;
}

static bool edmc_get_clk(void)
{
	bool ret = false;
	unsigned int reg_edm_ctl_0 = 0;

	LOG_DBG("[%s]\n", __func__);
	reg_edm_ctl_0 = edmc_reg_read(APU_EDM_CTL_0);

	if ((reg_edm_ctl_0 & CLK_ENABLE) > 0)
		ret = true;

	LOG_DBG("[%s] ret(%d)\n", __func__, ret);
	return ret;
}

static int emdc_enable_power(bool enable)
{
	int ret = 0;

	LOG_INF("[%s] enable(%u)\n", __func__, enable);

	if (enable) {
		if (!edmc_power_on) {
			LOG_DBG("[%s]Power Enable\n", __func__);
			edmc_power_on = 1;

			//Set to top opp
			mdla_opp_check(1, 0, 0);
			ret = mdla_get_power(1);

			if (ret)
				LOG_ERR("[%s] may fail(%u)\n", __func__, ret);
			else
				LOG_DBG("[%s] Success\n", __func__);
		} else {
			LOG_DBG("[%s]Power is already Enable\n", __func__);
		}
	} else {
		if (edmc_power_on) {
			LOG_DBG("[%s]Power Disable\n", __func__);
			edmc_power_on = 0;
			mdla_put_power(1);
			LOG_DBG("[%s]mdla_put_power\n", __func__);
		} else {
			LOG_DBG("[%s]Power is already Disable\n", __func__);
		}
	}
	LOG_INF("[%s] enable(%u) edmc_power_on(%u)Done\n",
			__func__, enable, edmc_power_on);

	return 0;
}
static int edmc_enable_clk(bool enable)
{
	bool clk_on = false;

	LOG_INF("[%s] enable(%u)\n", __func__, enable);

	clk_on = edmc_get_clk();

	if (enable) {
		if (!clk_on) {

			edmc_reg_write(edmc_reg_read(APU_EDM_CTL_0) |
					CLK_ENABLE, APU_EDM_CTL_0);
			edmc_reg_write(edmc_reg_read(APU_EDM_CTL_0) |
					DMA_SW_RST, APU_EDM_CTL_0);
			edmc_reg_write(edmc_reg_read(APU_EDM_CTL_0) &
					~DMA_SW_RST, APU_EDM_CTL_0);
		} else {
			LOG_DBG("[%s]Clk is already Enable\n", __func__);
		}
	} else {
		if (clk_on) {
			//Reset for checking AXI is Done then disable CLK
			edmc_reset();
			edmc_reg_write(edmc_reg_read(APU_EDM_CTL_0) &
					~CLK_ENABLE, APU_EDM_CTL_0);
		} else {
			LOG_DBG("[%s]Clk is already Disable\n", __func__);
		}
	}
	LOG_INF("[%s] Clk_enable(%u) Done\n", __func__, enable);
	return 0;
}



static int edmc_toggle_reset(void)
{
	// Reset
	int ret = 0;

	LOG_INF("[%s]\n", __func__);

	edmc_reg_write(edmc_reg_read(APU_EDM_CTL_0) | DMA_SW_RST,
			APU_EDM_CTL_0);
	edmc_reg_write(edmc_reg_read(APU_EDM_CTL_0) & ~DMA_SW_RST,
			APU_EDM_CTL_0);

#ifdef CONFIG_MTK_EDMC_ION
	edmc_enable_ion(true);
#endif

	return ret;

}

static int edmc_reset(void)
{
	// Reset
	unsigned int value_APU_EDM_CTL_0 = 0;
	int count = 0;
	int ret = 0;

	LOG_INF("[%s]\n", __func__);
	value_APU_EDM_CTL_0 = edmc_reg_read(APU_EDM_CTL_0);
	value_APU_EDM_CTL_0 = value_APU_EDM_CTL_0 & ~MT8163E2_ECO_DISABLE;
	value_APU_EDM_CTL_0 = value_APU_EDM_CTL_0 | RST_PROT_EN;
	value_APU_EDM_CTL_0 = value_APU_EDM_CTL_0 & ~0x7; //Clear NumDescIncr
	edmc_reg_write(value_APU_EDM_CTL_0, APU_EDM_CTL_0);

	edmc_reg_write(edmc_reg_read(APU_EDM_CTL_0) |
			RST_PROT_EN, APU_EDM_CTL_0);
	edmc_reg_write(edmc_reg_read(APU_EDM_CTL_0) |
			AXI_PROT_EN, APU_EDM_CTL_0);

	LOG_DBG("Polling enable reset protection Start\n");
	value_APU_EDM_CTL_0 = 0;
	while (!value_APU_EDM_CTL_0) {
		value_APU_EDM_CTL_0 = ((edmc_reg_read(APU_EDM_CTL_0) >> 8) &
					0x1);
		if (value_APU_EDM_CTL_0 != 0) {
			LOG_DBG("Polling enable reset protection done\n");
		} else {
			LOG_DBG("Polling value_APU_EDM_CTL_0: %.8x\n",
					edmc_reg_read(APU_EDM_CTL_0));
			mdelay(100);
			count++;
			if (count > 1000) {
				LOG_ERR("[%s] count > 1000 exit\n", __func__);
				ret = -1;
				goto exit;
			}
		}

	}

	edmc_reg_write(edmc_reg_read(APU_EDM_CTL_0) | DMA_SW_RST,
			APU_EDM_CTL_0);
	edmc_reg_write(edmc_reg_read(APU_EDM_CTL_0) & ~DMA_SW_RST,
			APU_EDM_CTL_0);
	edmc_reg_write(edmc_reg_read(APU_EDM_CTL_0) & ~AXI_PROT_EN,
			APU_EDM_CTL_0);

#ifdef CONFIG_MTK_EDMC_ION
	edmc_enable_ion(true);
#endif
	LOG_INF("[%s] Done\n", __func__);
exit:
	return ret;

}
static void edmc_trigger(void)
{
	//fire
	unsigned int value_edm_ctl_0 = 0;

	LOG_INF("[%s]\n", __func__);

	value_edm_ctl_0 = edmc_reg_read(APU_EDM_CTL_0);

	//Enque 1 descriptor and Fire
	edmc_reg_write(value_edm_ctl_0 | 0x1, APU_EDM_CTL_0);

	//edmc_dump_edm_reg();
#if 0 //Polling mode for INT fail
	//Trigger polling wq for checking IsDMADone
	schedule_work(&edmc_polling_queue);
#endif
	LOG_DBG("[%s] Done\n", __func__);
}
static void edmc_dump_edm_reg(void)
{

	LOG_DBG("[%s]\n", __func__);
	LOG_DBG("[0x00]: %.8x ",
			edmc_reg_read(APU_EDM_CTL_0));
	LOG_DBG("[0x04]: %.8x ",
			edmc_reg_read(APU_EDM_CFG_0));
	LOG_DBG("[0x08]: %.8x ",
				edmc_reg_read(APU_EDM_INT_MASK));
	LOG_DBG("[0x0C]: %.8x ",
				edmc_reg_read(APU_EDM_DESP_STATUS));
	LOG_DBG("[0x10]: %.8x\n",
				edmc_reg_read(APU_EDM_INT_STATUS));

	LOG_DBG("[%s] Done\n", __func__);

}
static void edmc_dump_desp_reg(void)
{
	LOG_DBG("[%s]\n", __func__);

	LOG_DBG("===[0]===\n");
	LOG_DBG("[0xC00]: %.8x ",
				edmc_reg_read(APU_DESP0_SRC_TILE_WIDTH));
	LOG_DBG("[0xC04]: %.8x ",
		edmc_reg_read(APU_DESP0_DEST_TILE_WIDTH));
	LOG_DBG("[0xC08]: %.8x ",
		edmc_reg_read(APU_DESP0_TILE_HEIGHT));
	LOG_DBG("[0xC0C]: %.8x ",
		edmc_reg_read(APU_DESP0_SRC_STRIDE));
	LOG_DBG("[0xC10]: %.8x\n",
		edmc_reg_read(APU_DESP0_DEST_STRIDE));
	LOG_DBG("[0xC14]: %.8x ",
		edmc_reg_read(APU_DESP0_SRC_ADDR_0));
	LOG_DBG("[0xC1C]: %.8x ",
		edmc_reg_read(APU_DESP0_DEST_ADDR_0));
	LOG_DBG("[0xC24]: %.8x ",
		edmc_reg_read(APU_DESP0_CTL_0));
	LOG_DBG("[0xC28]: %.8x\n",
		edmc_reg_read(APU_DESP0_CTL_1));
	LOG_DBG("[0xC2C]: %.8x ",
		edmc_reg_read(APU_DESP0_FILL_VALUE));
	LOG_DBG("[0xC30]: %.8x ",
		edmc_reg_read(APU_DESP0_ID));
	LOG_DBG("[0xC34]: %.8x ",
		edmc_reg_read(APU_DESP0_RANGE_SCALE));
	LOG_DBG("[0xC38]: %.8x\n",
		edmc_reg_read(APU_DESP0_MIN_FP32));
	LOG_DBG("===[1]===\n");
	LOG_DBG("[0xC40]: %.8x ",
				edmc_reg_read(APU_DESP1_SRC_TILE_WIDTH));
	LOG_DBG("[0xC44]: %.8x ",
		edmc_reg_read(APU_DESP1_DEST_TILE_WIDTH));
	LOG_DBG("[0xC48]: %.8x ",
		edmc_reg_read(APU_DESP1_TILE_HEIGHT));
	LOG_DBG("[0xC4C]: %.8x ",
		edmc_reg_read(APU_DESP1_SRC_STRIDE));
	LOG_DBG("[0xC50]: %.8x\n",
		edmc_reg_read(APU_DESP1_DEST_STRIDE));
	LOG_DBG("[0xC54]: %.8x ",
		edmc_reg_read(APU_DESP1_SRC_ADDR_0));
	LOG_DBG("[0xC5C]: %.8x ",
		edmc_reg_read(APU_DESP1_DEST_ADDR_0));
	LOG_DBG("[0xC64]: %.8x ",
		edmc_reg_read(APU_DESP1_CTL_0));
	LOG_DBG("[0xC68]: %.8x\n",
		edmc_reg_read(APU_DESP1_CTL_1));
	LOG_DBG("[0xC6C]: %.8x ",
		edmc_reg_read(APU_DESP1_FILL_VALUE));
	LOG_DBG("[0xC70]: %.8x ",
		edmc_reg_read(APU_DESP1_ID));
	LOG_DBG("[0xC74]: %.8x ",
		edmc_reg_read(APU_DESP1_RANGE_SCALE));
	LOG_DBG("[0xC78]: %.8x\n",
		edmc_reg_read(APU_DESP1_MIN_FP32));
	LOG_DBG("===[2]===\n");
	LOG_DBG("[0xC80]: %.8x ",
				edmc_reg_read(APU_DESP2_SRC_TILE_WIDTH));
	LOG_DBG("[0xC84]: %.8x ",
		edmc_reg_read(APU_DESP2_DEST_TILE_WIDTH));
	LOG_DBG("[0xC88]: %.8x ",
		edmc_reg_read(APU_DESP2_TILE_HEIGHT));
	LOG_DBG("[0xC8C]: %.8x ",
		edmc_reg_read(APU_DESP2_SRC_STRIDE));
	LOG_DBG("[0xC90]: %.8x\n",
		edmc_reg_read(APU_DESP2_DEST_STRIDE));
	LOG_DBG("[0xC94]: %.8x ",
		edmc_reg_read(APU_DESP2_SRC_ADDR_0));
	LOG_DBG("[0xC9C]: %.8x ",
		edmc_reg_read(APU_DESP2_DEST_ADDR_0));
	LOG_DBG("[0xCA4]: %.8x ",
		edmc_reg_read(APU_DESP2_CTL_0));
	LOG_DBG("[0xCA8]: %.8x\n",
		edmc_reg_read(APU_DESP2_CTL_1));
	LOG_DBG("[0xCAC]: %.8x ",
		edmc_reg_read(APU_DESP2_FILL_VALUE));
	LOG_DBG("[0xCB0]: %.8x ",
		edmc_reg_read(APU_DESP2_ID));
	LOG_DBG("[0xCB4]: %.8x ",
		edmc_reg_read(APU_DESP2_RANGE_SCALE));
	LOG_DBG("[0xCB8]: %.8x\n",
		edmc_reg_read(APU_DESP2_MIN_FP32));
	LOG_DBG("===[3]===\n");
	LOG_DBG("[0xCC0]: %.8x ",
				edmc_reg_read(APU_DESP3_SRC_TILE_WIDTH));
	LOG_DBG("[0xCC4]: %.8x ",
		edmc_reg_read(APU_DESP3_DEST_TILE_WIDTH));
	LOG_DBG("[0xCC8]: %.8x ",
		edmc_reg_read(APU_DESP3_TILE_HEIGHT));
	LOG_DBG("[0xCCC]: %.8x ",
		edmc_reg_read(APU_DESP3_SRC_STRIDE));
	LOG_DBG("[0xCD0]: %.8x\n",
		edmc_reg_read(APU_DESP3_DEST_STRIDE));
	LOG_DBG("[0xCD4]: %.8x ",
		edmc_reg_read(APU_DESP3_SRC_ADDR_0));
	LOG_DBG("[0xCDC]: %.8x ",
		edmc_reg_read(APU_DESP3_DEST_ADDR_0));
	LOG_DBG("[0xCE4]: %.8x ",
		edmc_reg_read(APU_DESP3_CTL_0));
	LOG_DBG("[0xCE8]: %.8x\n",
		edmc_reg_read(APU_DESP3_CTL_1));
	LOG_DBG("[0xCEC]: %.8x ",
		edmc_reg_read(APU_DESP3_FILL_VALUE));
	LOG_DBG("[0xCF0]: %.8x ",
		edmc_reg_read(APU_DESP3_ID));
	LOG_DBG("[0xCF4]: %.8x ",
		edmc_reg_read(APU_DESP3_RANGE_SCALE));
	LOG_DBG("[0xCF8]: %.8x\n",
		edmc_reg_read(APU_DESP3_MIN_FP32));

	LOG_DBG("[%s] Done\n", __func__);

}

static void edmc_dump_reg(u32 write_ptr)
{
	u32 reg_src_tile_width, reg_dst_tile_width, reg_tile_height;
	u32 reg_src_stride, reg_dst_stride;
	u32 reg_src_addr, reg_dst_addr;
	u32 reg_ctl_1, reg_desp_id;
	u32 reg_fill_value;
	u32 reg_range_scale, reg_min_fp32;

	switch (write_ptr) {
	case 0:
		reg_src_tile_width = APU_DESP0_SRC_TILE_WIDTH;
		reg_dst_tile_width = APU_DESP0_DEST_TILE_WIDTH;
		reg_tile_height = APU_DESP0_TILE_HEIGHT;
		reg_src_stride = APU_DESP0_SRC_STRIDE;
		reg_dst_stride = APU_DESP0_DEST_STRIDE;
		reg_src_addr = APU_DESP0_SRC_ADDR_0;
		reg_dst_addr = APU_DESP0_DEST_ADDR_0;
		reg_ctl_1 = APU_DESP0_CTL_1;
		reg_desp_id = APU_DESP0_ID;
		reg_fill_value = APU_DESP0_FILL_VALUE;
		reg_range_scale = APU_DESP0_RANGE_SCALE;
		reg_min_fp32 = APU_DESP0_MIN_FP32;
		break;
	case 1:
		reg_src_tile_width = APU_DESP1_SRC_TILE_WIDTH;
		reg_dst_tile_width = APU_DESP1_DEST_TILE_WIDTH;
		reg_tile_height = APU_DESP1_TILE_HEIGHT;
		reg_src_stride = APU_DESP1_SRC_STRIDE;
		reg_dst_stride = APU_DESP1_DEST_STRIDE;
		reg_src_addr = APU_DESP1_SRC_ADDR_0;
		reg_dst_addr = APU_DESP1_DEST_ADDR_0;
		reg_ctl_1 = APU_DESP1_CTL_1;
		reg_desp_id = APU_DESP1_ID;
		reg_fill_value = APU_DESP1_FILL_VALUE;
		reg_range_scale = APU_DESP1_RANGE_SCALE;
		reg_min_fp32 = APU_DESP1_MIN_FP32;
		break;
	case 2:
		reg_src_tile_width = APU_DESP2_SRC_TILE_WIDTH;
		reg_dst_tile_width = APU_DESP2_DEST_TILE_WIDTH;
		reg_tile_height = APU_DESP2_TILE_HEIGHT;
		reg_src_stride = APU_DESP2_SRC_STRIDE;
		reg_dst_stride = APU_DESP2_DEST_STRIDE;
		reg_src_addr = APU_DESP2_SRC_ADDR_0;
		reg_dst_addr = APU_DESP2_DEST_ADDR_0;
		reg_ctl_1 = APU_DESP2_CTL_1;
		reg_desp_id = APU_DESP2_ID;
		reg_fill_value = APU_DESP2_FILL_VALUE;
		reg_range_scale = APU_DESP2_RANGE_SCALE;
		reg_min_fp32 = APU_DESP2_MIN_FP32;
		break;
	case 3:
		reg_src_tile_width = APU_DESP3_SRC_TILE_WIDTH;
		reg_dst_tile_width = APU_DESP3_DEST_TILE_WIDTH;
		reg_tile_height = APU_DESP3_TILE_HEIGHT;
		reg_src_stride = APU_DESP3_SRC_STRIDE;
		reg_dst_stride = APU_DESP3_DEST_STRIDE;
		reg_src_addr = APU_DESP3_SRC_ADDR_0;
		reg_dst_addr = APU_DESP3_DEST_ADDR_0;
		reg_ctl_1 = APU_DESP3_CTL_1;
		reg_desp_id = APU_DESP3_ID;
		reg_fill_value = APU_DESP3_FILL_VALUE;
		reg_range_scale = APU_DESP3_RANGE_SCALE;
		reg_min_fp32 = APU_DESP3_MIN_FP32;
		break;
	default:
		LOG_DBG("edmc_copy out of range!\n");
		return;
	}
#if 1
	LOG_DBG("reg_src_tile_width: %.8x value = %.8x\n", reg_src_tile_width,
		  edmc_reg_read(reg_src_tile_width));
	LOG_DBG("reg_dst_tile_width: %.8x value = %.8x\n", reg_dst_tile_width,
		  edmc_reg_read(reg_dst_tile_width));
	LOG_DBG("reg_tile_height: %.8x value = %.8x\n", reg_tile_height,
		  edmc_reg_read(reg_tile_height));
	LOG_DBG("reg_src_stride: %.8x value = %.8x\n", reg_src_stride,
		  edmc_reg_read(reg_src_stride));
	LOG_DBG("reg_dst_stride: %.8x value = %.8x\n", reg_dst_stride,
		  edmc_reg_read(reg_dst_stride));
	LOG_DBG("reg_src_addr: %.8x value = %.8x\n", reg_src_addr,
		  edmc_reg_read(reg_src_addr));
	LOG_DBG("reg_dst_addr: %.8x value = %.8x\n", reg_dst_addr,
		  edmc_reg_read(reg_dst_addr));
	LOG_DBG("reg_fill_value: %.8x value = %.8x\n", reg_fill_value,
		  edmc_reg_read(reg_fill_value));
	LOG_DBG("reg_range_scale: %.8x value = %.8x\n", reg_range_scale,
		  edmc_reg_read(reg_range_scale));
	LOG_DBG("reg_min_fp32: %.8x value = %.8x\n", reg_min_fp32,
		  edmc_reg_read(reg_min_fp32));
	LOG_DBG("reg_ctl_1: %.8x value = %.8x\n", reg_ctl_1,
		  edmc_reg_read(reg_ctl_1));
	LOG_DBG("reg_desp_id: %.8x value = %.8x\n", reg_desp_id,
		  edmc_reg_read(reg_desp_id));
//	LOG_DBG("reg_err_int_mask: 0x0014 value = %.8x\n",
			//edmc_reg_read(APU_EDM_ERR_INT_MASK));
	LOG_DBG("reg_err_int_status: 0x001c value = %.8x\n",
		edmc_reg_read(APU_EDM_ERR_INT_STATUS));
#endif

}

static int edmc_get_write_ptr(u32 *write_ptr)
{
	unsigned int value = 0;
	u32 reg_write_ptr = 0, reg_num_descriptor = 0;

	value = edmc_reg_read(APU_EDM_CFG_0);

	//Update write_pointer and num_of_descriptor
	reg_num_descriptor = value & 0x7;
	reg_write_ptr = (value >> 4) & 0x3;

	LOG_DBG("[%s] write_ptr(%u) num_descriptor(%u)\n",
			__func__, reg_write_ptr, reg_num_descriptor);

	//Update output
	*write_ptr = reg_write_ptr;
	LOG_DBG("[%s] reg_write_ptr = %.8x\n", __func__, reg_write_ptr);
	return 0;
}
static void edmc_copy(struct ioctl_edmc_descript *pdescript, u8 op_mode,
		      u32 write_ptr)
{
	u32 reg_src_tile_width, reg_dst_tile_width, reg_tile_height;
	u32 reg_src_stride, reg_dst_stride;
	u32 reg_src_addr, reg_dst_addr;
	u32 reg_ctl_1, reg_desp_id;
	u32 reg_fill_value;
	u32 reg_range_scale, reg_min_fp32;

	LOG_INF("[%s] op_mode(%x) write_ptr(%x)\n",
		__func__, op_mode, write_ptr);

	//Enable power
	emdc_enable_power(true);
	//Enable clk
	edmc_enable_clk(true);


	if (timer_pending(&edmc_power_timer)) {
		LOG_DBG("[%s] del_timer!\n", __func__);
		del_timer(&edmc_power_timer);
	}

#ifdef CONFIG_MTK_EDMC_ION
	edmc_enable_ion(true);
#endif

	switch (write_ptr) {
	case 0:
		reg_src_tile_width = APU_DESP0_SRC_TILE_WIDTH;
		reg_dst_tile_width = APU_DESP0_DEST_TILE_WIDTH;
		reg_tile_height = APU_DESP0_TILE_HEIGHT;
		reg_src_stride = APU_DESP0_SRC_STRIDE;
		reg_dst_stride = APU_DESP0_DEST_STRIDE;
		reg_src_addr = APU_DESP0_SRC_ADDR_0;
		reg_dst_addr = APU_DESP0_DEST_ADDR_0;
		reg_ctl_1 = APU_DESP0_CTL_1;
		reg_desp_id = APU_DESP0_ID;
		reg_fill_value = APU_DESP0_FILL_VALUE;
		reg_range_scale = APU_DESP0_RANGE_SCALE;
		reg_min_fp32 = APU_DESP0_MIN_FP32;
		break;
	case 1:
		reg_src_tile_width = APU_DESP1_SRC_TILE_WIDTH;
		reg_dst_tile_width = APU_DESP1_DEST_TILE_WIDTH;
		reg_tile_height = APU_DESP1_TILE_HEIGHT;
		reg_src_stride = APU_DESP1_SRC_STRIDE;
		reg_dst_stride = APU_DESP1_DEST_STRIDE;
		reg_src_addr = APU_DESP1_SRC_ADDR_0;
		reg_dst_addr = APU_DESP1_DEST_ADDR_0;
		reg_ctl_1 = APU_DESP1_CTL_1;
		reg_desp_id = APU_DESP1_ID;
		reg_fill_value = APU_DESP1_FILL_VALUE;
		reg_range_scale = APU_DESP1_RANGE_SCALE;
		reg_min_fp32 = APU_DESP1_MIN_FP32;
		break;
	case 2:
		reg_src_tile_width = APU_DESP2_SRC_TILE_WIDTH;
		reg_dst_tile_width = APU_DESP2_DEST_TILE_WIDTH;
		reg_tile_height = APU_DESP2_TILE_HEIGHT;
		reg_src_stride = APU_DESP2_SRC_STRIDE;
		reg_dst_stride = APU_DESP2_DEST_STRIDE;
		reg_src_addr = APU_DESP2_SRC_ADDR_0;
		reg_dst_addr = APU_DESP2_DEST_ADDR_0;
		reg_ctl_1 = APU_DESP2_CTL_1;
		reg_desp_id = APU_DESP2_ID;
		reg_fill_value = APU_DESP2_FILL_VALUE;
		reg_range_scale = APU_DESP2_RANGE_SCALE;
		reg_min_fp32 = APU_DESP2_MIN_FP32;
		break;
	case 3:
		reg_src_tile_width = APU_DESP3_SRC_TILE_WIDTH;
		reg_dst_tile_width = APU_DESP3_DEST_TILE_WIDTH;
		reg_tile_height = APU_DESP3_TILE_HEIGHT;
		reg_src_stride = APU_DESP3_SRC_STRIDE;
		reg_dst_stride = APU_DESP3_DEST_STRIDE;
		reg_src_addr = APU_DESP3_SRC_ADDR_0;
		reg_dst_addr = APU_DESP3_DEST_ADDR_0;
		reg_ctl_1 = APU_DESP3_CTL_1;
		reg_desp_id = APU_DESP3_ID;
		reg_fill_value = APU_DESP3_FILL_VALUE;
		reg_range_scale = APU_DESP3_RANGE_SCALE;
		reg_min_fp32 = APU_DESP3_MIN_FP32;
		break;
	default:
		LOG_ERR("[%s]Out of range!\n", __func__);
		goto exit;
	}
	//edmc_dump_reg(write_ptr);

	//write descriptor
	pdescript->despcript_id = write_ptr;

	edmc_reg_write(pdescript->src_tile_width - 1, reg_src_tile_width);
	edmc_reg_write(pdescript->dst_tile_width - 1, reg_dst_tile_width);
	edmc_reg_write(pdescript->tile_height - 1, reg_tile_height);
	edmc_reg_write(pdescript->src_stride - 1, reg_src_stride);
	edmc_reg_write(pdescript->dst_stride - 1, reg_dst_stride);
	edmc_reg_write(pdescript->src_addr, reg_src_addr);
	edmc_reg_write(pdescript->dst_addr, reg_dst_addr);
	edmc_reg_write(pdescript->fill_value, reg_fill_value);
	edmc_reg_write(pdescript->range_scale, reg_range_scale);
	edmc_reg_write(pdescript->min_fp32, reg_min_fp32);
	edmc_reg_write(edmc_reg_read(reg_ctl_1) & (~0xF), reg_ctl_1);
	//Set to op_mode = 0
	edmc_reg_write(edmc_reg_read(reg_ctl_1) | op_mode | (1 << 6),
			reg_ctl_1);

	edmc_reg_write(pdescript->user_desp_id, reg_desp_id);
	LOG_DBG("[%s] user_desp_id(%.8x) wait_id((%.8llx))\n",
			__func__, pdescript->user_desp_id, pdescript->wait_id);
	LOG_DBG("[%s] write done\n", __func__);
	edmc_dump_edm_reg();
	edmc_dump_desp_reg();
	edmc_dump_reg(write_ptr);


	edmc_trigger();
exit:
	//mutex_unlock(&edmc_mutex);
	LOG_INF("[%s] Done\n", __func__);
}
#ifdef CONFIG_MTK_EDMC_ION
static int edmc_enable_ion(bool enable)
{
	unsigned int value = 0;
	int regs[4] = {APU_DESP0_CTL_0, APU_DESP1_CTL_0,
					APU_DESP2_CTL_0, APU_DESP3_CTL_0};
	int i = 0;

	//LOG_DBG("[%s] ,enable=%d\n", __func__, enable);

	if (enable) {
		for (i = 0; i < ARRAY_SIZE(regs); i++) {
			value = edmc_reg_read(regs[i]);
			value = value | (0x8<<4) | (0x8<<9); /*enable ion*/
			edmc_reg_write(value, regs[i]);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(regs); i++) {
			value = edmc_reg_read(regs[i]);
			/*disable ion*/
			value = value & (~((0x8<<4) | (0x8<<9)));
			edmc_reg_write(value, regs[i]);
		}
	}
	return 0;
}
#endif

static int edmc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_edmc_drvdata *lp = NULL;
	int rc = 0, ret = 0;
	struct resource *r_mem; /* IO mem resources */
	struct resource *r_irq; /* Interrupt resources */

	dev_info(dev, "Device Tree Probing\n");

	/* Get iospace for the device */
	r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r_mem) {
		dev_err(dev, "invalid address\n");
		return -ENODEV;
	}

	lp = (struct mtk_edmc_drvdata *)
				kmalloc(sizeof(struct mtk_edmc_drvdata),
					GFP_KERNEL);
	if (!lp) {
		dev_err(dev, "Cound not allocate mtk_edmc device\n");
		return -ENOMEM;
	}

	dev_info(dev, "r_mem start at 0x%08lx end 0x%08lx\n",
		 (unsigned long __force)r_mem->start,
		 (unsigned long __force)r_mem->end);

	dev_info(dev, "set drvdata\n");
	dev_set_drvdata(dev, lp);

	lp->mem_start = r_mem->start;
	lp->mem_end = r_mem->end;

	if (!request_mem_region(lp->mem_start, lp->mem_end - lp->mem_start + 1,
				DRIVER_NAME)) {
		dev_err(dev, "Couldn't lock memory region at %p\n",
				(void *) lp->mem_start);
		rc = -EBUSY;
		return 0;
	}
	dev_info(dev, "request_mem_region\n");
	dev_info(dev, "mem_start at 0x%08lx mem_end 0x%08lx\n",
		 (unsigned long __force)lp->mem_start,
		 (unsigned long __force)lp->mem_end);

	lp->base_addr = ioremap_nocache(lp->mem_start,
					lp->mem_end - lp->mem_start + 1);
	edmc_ctrl_top = lp->base_addr;
	if (!lp->base_addr) {
		dev_err(dev, "mtk_edmc: Could not allocate iomem\n");
		rc = -EIO;
		return 0;
	}
	dev_info(dev, "ioremap_nocache\n");

	/* Get IRQ for the device */
	r_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!r_irq) {
		dev_info(dev, "no IRQ found\n");
		dev_info(dev, "mtk_edmc at 0x%08lx mapped to 0x%08lx\n",
			 (unsigned long __force)lp->mem_start,
			 (unsigned long __force)lp->base_addr);
		return 0;
	}
	dev_info(dev, "platform_get_resource irq: %llu\n", r_irq->start);
	g_irq_num = lp->irq = r_irq->start;
#if 1 //IRQ Disable
	rc = request_irq(lp->irq, edmc_interrupt, IRQF_TRIGGER_LOW,
			DRIVER_NAME, dev);
	dev_info(dev, "request_irq\n");
	if (rc) {
		dev_err(dev, "mtk_edmc: Could not allocate interrupt %d.\n",
			lp->irq);
		return 0;
	}
#endif
	dev_info(dev, "mtk_edmc at 0x%08lx mapped to 0x%08lx, irq=%d\n",
		 (unsigned long __force)lp->mem_start,
		 (unsigned long __force)lp->base_addr,
		 lp->irq);

#if 0
	edmc_enable_clk(true);
#ifdef CONFIG_MTK_EDMC_ION
	edmc_enable_ion(true);
#endif
#endif
	return ret;

}
static int edmc_remove(struct platform_device *pdev)
{
	unsigned long flags;
	struct device *dev = &pdev->dev;
	struct mtk_edmc_drvdata *lp = dev_get_drvdata(dev);

	LOG_DBG("[%s]\n", __func__);

	edmc_reset();

	mutex_lock(&cmd_list_mutex);
	g_edmc_seq_job = 0;
	g_edmc_seq_last = 0;
	cmd_list_len = 0;
	edmc_clear_cmd_queue();
	mutex_unlock(&cmd_list_mutex);

	spin_lock_irqsave(&edmc_lock, flags);
	g_edmc_seq_finish = 0;
	g_edmc_seq_error = 0;
	spin_unlock_irqrestore(&edmc_lock, flags);
#if 1 //IRQ Disable
	free_irq(lp->irq, dev);
#endif
	release_mem_region(lp->mem_start, lp->mem_end - lp->mem_start + 1);
	kfree(lp);

	dev_set_drvdata(dev, NULL);
	platform_set_drvdata(pdev, NULL);

	LOG_DBG("[%s] Done\n", __func__);
	return 0;
}
static const struct of_device_id edmc_of_ids[] = {
	{ .compatible = "mediatek,edmc", },
	{ .compatible = "mtk,edmc", },
	{ /* end of list */},
};

MODULE_DEVICE_TABLE(of, edmc_of_ids);

static struct platform_driver edmc_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = edmc_of_ids,
	},
	.probe = edmc_probe,
	.remove = edmc_remove,
};

static int edmcctl_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&edmc_driver);
	if (ret != 0)
		return ret;

	numberOpens = 0;
	g_edmc_seq_job = 0;
	g_edmc_seq_finish = 0;
	g_edmc_seq_error = 0;
	g_edmc_seq_last = 0;
	cmd_list_len = 0;
	edmc_power_on = 0;

	g_edmc_log_level = EDMC_LOG_LEVEL_DEFAULT;
#ifdef ERROR_TRIGGER_TEST
	g_trigger_next_error = false;
	g_emdc_irq_disabled = false;
#endif

	// Try to dynamically allocate a major number for the device
	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if (majorNumber < 0) {
		pr_warn("EDMC failed to register a major number\n");
		return majorNumber;
	}
	LOG_INF("EDMC: registered correctly with major number %d\n",
			majorNumber);

	// Register the device class
	edmcctlClass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(edmcctlClass)) {
		// Check for error and clean up if there is
		unregister_chrdev(majorNumber, DEVICE_NAME);
		pr_warn("ailed to register device class\n");
		return PTR_ERR(edmcctlClass);
	}
	// Register the device driver
	edmcctlDevice = device_create(edmcctlClass, NULL,
					MKDEV(majorNumber, 0), NULL,
					DEVICE_NAME);
	if (IS_ERR(edmcctlDevice)) {
		// Clean up if there is an error
		unregister_chrdev(majorNumber, DEVICE_NAME);
		pr_warn("ailed to create the device\n");
		return PTR_ERR(edmcctlDevice);
	}

	INIT_WORK(&edmc_error_handle_wq, edmc_error_handle);
	INIT_WORK(&edmc_queue, edmc_start_queue);
	INIT_WORK(&edmc_power_off_queue, edmc_start_power_off);
	INIT_WORK(&edmc_polling_queue, edmc_start_polling);

	if (edmc_debugfs_init())
		LOG_ERR("EDMC: edmc_debugfs_init FAIL!!!\n");

	return 0;
}

static int edmc_open(struct inode *inodep, struct file *filep)
{
	numberOpens++;
	LOG_DBG("EDMC: Device has been opened %d time(s)\n", numberOpens);
	return 0;
}

static int edmc_release(struct inode *inodep, struct file *filep)
{
	LOG_DBG("EDMC: Device successfully closed\n");

	return 0;
}

#ifdef CONFIG_COMPAT
static long compat_edmc_ioctl(struct file *file, unsigned int
					command, unsigned long arg)
{
	long ret = 0;

	LOG_DBG("[%s]!!\n", __func__);
	ret = file->f_op->unlocked_ioctl(file, command,
				(unsigned long)compat_ptr(arg));
	return ret;
}
#endif
static long edmc_ioctl(struct file *filp, unsigned int command,
		       unsigned long arg)
{
	long retval = 0;
	struct ioctl_edmc_descript edmc_descript;
	u64 wait_id;
#ifdef ERROR_TRIGGER_TEST
	int trigger_error = 0;
#endif

	//LOG_DBG("[%s] command=0x%x, arg=0x%lx\n", __func__, command, arg);

	switch (command) {
	case IOCTL_EDMC_COPY:
		if (copy_from_user(&edmc_descript, (void *)arg,
				   sizeof(edmc_descript))) {
			retval = -EFAULT;
			return retval;
		}
		if (edmc_run_command_async(&edmc_descript, MODE_NORMAL)) {
			retval = -EINVAL;
			return retval;
		}
		if (copy_to_user((void *) arg, &edmc_descript,
				 sizeof(edmc_descript))) {
			retval = -EFAULT;
			return retval;
		}
		break;
	case IOCTL_EDMC_FILL:
		if (copy_from_user(&edmc_descript, (void *)arg,
				   sizeof(edmc_descript))) {
			retval = -EFAULT;
			return retval;
		}
		if (edmc_run_command_async(&edmc_descript, MODE_FILL)) {
			retval = -EINVAL;
			return retval;
		}
		if (copy_to_user((void *) arg, &edmc_descript,
				 sizeof(edmc_descript))) {
			retval = -EFAULT;
			return retval;
		}
		break;
	case IOCTL_EDMC_RGBTORGBA:
		if (copy_from_user(&edmc_descript, (void *)arg,
				   sizeof(edmc_descript))) {
			retval = -EFAULT;
			return retval;
		}
		if (edmc_run_command_async(&edmc_descript, MODE_RGATORGBA)) {
			retval = -EINVAL;
			return retval;
		}
		if (copy_to_user((void *) arg, &edmc_descript,
				 sizeof(edmc_descript))) {
			retval = -EFAULT;
			return retval;
		}
		break;
	case IOCTL_EDMC_FP32TOFIX8:
		if (copy_from_user(&edmc_descript, (void *)arg,
				   sizeof(edmc_descript)))	{
			retval = -EFAULT;
			return retval;
		}
		if (edmc_run_command_async(&edmc_descript, MODE_FP32TOFIX8)) {
			retval = -EINVAL;
			return retval;
		}
		if (copy_to_user((void *) arg, &edmc_descript,
				 sizeof(edmc_descript))) {
			retval = -EFAULT;
			return retval;
		}
		break;
	case IOCTL_EDMC_FIX8TOFP32:
		if (copy_from_user(&edmc_descript, (void *)arg,
				   sizeof(edmc_descript))) {
			retval = -EFAULT;
			return retval;
		}
		if (edmc_run_command_async(&edmc_descript, MODE_FIX8TOFP32)) {
			retval = -EINVAL;
			return retval;
		}
		if (copy_to_user((void *) arg, &edmc_descript,
				 sizeof(edmc_descript))) {
			retval = -EFAULT;
			return retval;
		}
		break;
	case IOCTL_EDMC_STATUS:
		//Not Support
		//edmc_get_status();
		break;
	case IOCTL_EDMC_RESET:
		//edmc_error_handle(NULL);
		//edmc_reset();
		break;
	case IOCTL_EDMC_WAIT:
		if (copy_from_user(&wait_id, (void *) arg, sizeof(u64))) {
			retval = -EFAULT;
			return retval;
		}
		retval = edmc_wait_command(wait_id);

		if (wait_id < g_edmc_seq_error) {
			LOG_DBG("[%s],IOCTL_EDMC_WAIT error hanppened!!!",
				__func__);
			LOG_DBG("[%s] pid[%d],wait_id=%llu, !!\n",
				__func__, current->pid, wait_id);
			LOG_DBG("[%s]seq_error=%llu\n",
					__func__, g_edmc_seq_error);
			return -EFAULT;
		}
		break;
#ifdef ERROR_TRIGGER_TEST
	case IOCTL_EDMC_TEST_TRIGGER_ERROR:
		if (copy_from_user(&trigger_error, (void *)arg,
				sizeof(trigger_error))) {
			retval = -EFAULT;
			return retval;
		}
		g_trigger_next_error = (bool)trigger_error;

		if (g_trigger_next_error == false
					&& g_emdc_irq_disabled == true) {
			g_emdc_irq_disabled = false;
			enable_irq(g_irq_num);
		}
		break;
#endif
	default:
		LOG_INF("[%s] IOCTL command not found\n", __func__);
		break;
	}
	return retval;
}

static void edmcctl_exit(void)
{
	platform_driver_unregister(&edmc_driver);
	device_destroy(edmcctlClass, MKDEV(majorNumber, 0));
	class_destroy(edmcctlClass);
	unregister_chrdev(majorNumber, DEVICE_NAME);
	edmc_debugfs_exit();
	LOG_DBG("[%s]\n", __func__);
}

module_init(edmcctl_init);
module_exit(edmcctl_exit);
