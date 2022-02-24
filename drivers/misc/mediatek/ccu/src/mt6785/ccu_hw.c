/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/iommu.h>
#include <linux/firmware.h>
#include <crypto/hash.h>
#include <crypto/akcipher.h>

#include "mtk_ion.h"
#include "ion_drv.h"
#include <linux/iommu.h>

#ifdef CONFIG_MTK_IOMMU
#include "mtk_iommu.h"
#include <dt-bindings/memory/mt6785-larb-port.h>
#else
#include "m4u.h"
#endif

#include <linux/io.h> /*for mb();*/

#include "ccu_hw.h"
#include "ccu_fw_pubk.h"
#include "ccu_drv_pubk.h"
#include "ccu_reg.h"
#include "ccu_cmn.h"
#include "ccu_kd_mailbox.h"
#include "ccu_i2c.h"
#include "ccu_mva.h"
#include "ccu_platform_def.h"
#include "kd_camera_feature.h"/*for sensorType in ccu_set_sensor_info*/
#include "ccu_ipc.h"
#include "ccu_imgsensor.h"

static uint64_t camsys_base;
static uint64_t bin_base;
static uint64_t dmem_base;
static uint64_t pmem_base;

static struct ccu_device_s *ccu_dev;
static struct task_struct *enque_task;

struct ccu_mailbox_t *pMailBox[MAX_MAILBOX_NUM];
static struct ccu_msg receivedCcuCmd;
static struct ccu_msg CcuAckCmd;

struct sdesc {
	struct shash_desc shash;
	char ctx[];
};

/*isr work management*/
struct ap_task_manage_t {
	struct workqueue_struct *ApTaskWorkQueue;
	struct mutex ApTaskMutex;
	struct list_head ApTskWorkList;
};

struct ap_task_manage_t ap_task_manage;


static struct CCU_INFO_STRUCT ccuInfo;
static bool bWaitCond;
static bool AFbWaitCond[2];
static unsigned int g_LogBufIdx = 1;
static unsigned int AFg_LogBufIdx[2] = {1, 1};

static int _ccu_powerdown(void);

static int ccu_load_segments(const struct firmware *fw,
	enum CCU_BIN_TYPE type);
static void *ccu_da_to_va(u64 da, uint32_t len);
static int ccu_sanity_check(const struct firmware *fw);
static int ccu_cert_check(const struct firmware *fw,
	uint8_t *pubk, uint32_t key_size);

static inline unsigned int CCU_MsToJiffies(unsigned int Ms)
{
	return ((Ms * HZ + 512) >> 10);
}

static void isr_sp_task(void)
{
	MUINT32 sp_task = ccu_read_reg(ccu_base, CCU_STA_REG_SP_ISR_TASK);

	switch (sp_task) {

	default:
	{
		LOG_DBG("no %s: %x\n", __func__, sp_task);
		break;
	}
	}
}

#define CCU_ISR_WORK_BUFFER_SZIE 16


irqreturn_t ccu_isr_handler(int irq, void *dev_id)
{
	enum mb_result mailboxRet;

	LOG_DBG("+++:%s\n", __func__);

	/*write clear mode*/
	LOG_DBG("write clear mode\n");
	if (!ccuInfo.IsCcuPoweredOn) {
		LOG_DBG_MUST("CCU off no need to service isr (%d)",
			ccuInfo.IsCcuPoweredOn);
		return IRQ_HANDLED;
	}

	ccu_write_reg(ccu_base, EINTC_CLR, 0xFF);
	LOG_DBG("read clear mode\n");
	ccu_read_reg(ccu_base, EINTC_ST);
	/**/

	isr_sp_task();

while (1) {
	mailboxRet = mailbox_receive_cmd(&receivedCcuCmd);

	if (mailboxRet == MAILBOX_QUEUE_EMPTY) {
		LOG_DBG_MUST("MAIL_BOX IS EMPTY");
		goto ISR_EXIT;
	}

	LOG_DBG("receivedCcuCmd.msg_id : 0x%x\n",
		receivedCcuCmd.msg_id);

	switch (receivedCcuCmd.msg_id) {

	case MSG_TO_APMCU_FLUSH_LOG:
	{
		/*for ccu_waitirq();*/
		LOG_DBG
		("got %s:%d, wakeup %s\n",
		 "MSG_TO_APMCU_FLUSH_LOG",
		 receivedCcuCmd.in_data_ptr,
		 "ccuInfo.WaitQueueHead");
		bWaitCond = true;
		g_LogBufIdx = receivedCcuCmd.in_data_ptr;

		wake_up_interruptible(&ccuInfo.WaitQueueHead);
		LOG_DBG("wakeup ccuInfo.WaitQueueHead done\n");
		break;
	}
	case MSG_TO_APMCU_CCU_ASSERT:
	{
		LOG_ERR
		("got %s:%d, wakeup %s\n",
		 "MSG_TO_APMCU_CCU_ASSERT",
		 receivedCcuCmd.in_data_ptr,
		 "ccuInfo.WaitQueueHead");
		LOG_ERR
			("====== AP_ISR_CCU_ASSERT ======\n");
		bWaitCond = true;
		g_LogBufIdx = 0xFFFFFFFF;	/* -1*/

		wake_up_interruptible(&ccuInfo.WaitQueueHead);
		LOG_ERR("wakeup ccuInfo.WaitQueueHead done\n");
		break;
	}
	case MSG_TO_APMCU_CCU_WARNING:
	{
		LOG_ERR
		("got %s:%d, wakeup %s\n",
		 "MSG_TO_APMCU_CCU_WARNING",
		 receivedCcuCmd.in_data_ptr,
		 "ccuInfo.WaitQueueHead");
		LOG_ERR
		("====== AP_ISR_CCU_WARNING ======\n");
		bWaitCond = true;
		g_LogBufIdx = -2;

		wake_up_interruptible(&ccuInfo.WaitQueueHead);
		LOG_ERR("wakeup ccuInfo.WaitQueueHead done\n");
		break;
	}
#ifdef CCU_AF_ENABLE
	case MSG_TO_APMCU_CAM_A_AFO_i:
	{
		LOG_DBG
		       ("AFWaitQueueHead:%d\n",
			receivedCcuCmd.in_data_ptr);
		if (receivedCcuCmd.tg_info == 1) {
			LOG_DBG
		    ("====== AFO_A_done_from_CCU ======\n");
		    AFbWaitCond[0] = true;
			AFg_LogBufIdx[0] = 3;

			wake_up_interruptible(&ccuInfo.AFWaitQueueHead[0]);
			LOG_DBG("wakeup ccuInfo.AFWaitQueueHead done\n");
		} else if (receivedCcuCmd.tg_info == 2) {
			LOG_DBG
		    ("====== AFO_B_done_from_CCU ======\n");
			AFbWaitCond[1] = true;
			AFg_LogBufIdx[1] = 4;

			wake_up_interruptible(&ccuInfo.AFWaitQueueHead[1]);
			LOG_DBG("wakeup ccuInfo.AFBWaitQueueHead done\n");
		} else {
			AFbWaitCond[0] = true;
			AFbWaitCond[1] = true;
			AFg_LogBufIdx[0] = 5;
			AFg_LogBufIdx[1] = 5;
			wake_up_interruptible(&ccuInfo.AFWaitQueueHead[0]);
			LOG_DBG("wakeup ccuInfo.AFWaitQueueHead done\n");
			wake_up_interruptible(&ccuInfo.AFWaitQueueHead[1]);
			LOG_DBG("wakeup ccuInfo.AFBWaitQueueHead done\n");
			LOG_DBG("abort and wakeup\n");
		}
		break;
	}
	case MSG_TO_APMCU_CAM_B_AFO_i:
	{
		LOG_DBG
		("AFBWaitQueueHead:%d\n",
		receivedCcuCmd.in_data_ptr);
		LOG_DBG
		("========== AFO_B_done_from_CCU ==========n");
		AFbWaitCond[1] = true;
		AFg_LogBufIdx[1] = 4;

		wake_up_interruptible(&ccuInfo.AFWaitQueueHead[1]);
		LOG_DBG("wakeup ccuInfo.AFBWaitQueueHead done\n");
		break;
	}
#endif /*CCU_AF_ENABLE*/

	default:
		LOG_DBG_MUST("got msgId: %d, cmd_wait\n",
			receivedCcuCmd.msg_id);
		ccu_memcpy(&CcuAckCmd, &receivedCcuCmd,
			sizeof(struct ccu_msg));
		break;

	}
}

ISR_EXIT:

	LOG_DBG("---:%s\n", __func__);

	/**/
	return IRQ_HANDLED;
}

static void ccu_ap_task_mgr_init(void)
{
	mutex_init(&ap_task_manage.ApTaskMutex);
}

int ccu_init_hw(struct ccu_device_s *device)
{
	int ret = 0, n;

	ccuInfo.IsCcuPoweredOn = 0;
#ifdef CONFIG_MTK_CHIP
	init_check_sw_ver();
#endif

	/* init waitqueue */
	init_waitqueue_head(&ccuInfo.WaitQueueHead);
	init_waitqueue_head(&ccuInfo.AFWaitQueueHead[0]);
	init_waitqueue_head(&ccuInfo.AFWaitQueueHead[1]);
	/* init atomic task counter */
	/*ccuInfo.taskCount = ATOMIC_INIT(0);*/

	/* Init spinlocks */
	spin_lock_init(&(ccuInfo.SpinLockCcuRef));
	spin_lock_init(&(ccuInfo.SpinLockCcu));
	for (n = 0; n < CCU_IRQ_TYPE_AMOUNT; n++) {
		spin_lock_init(&(ccuInfo.SpinLockIrq[n]));
		spin_lock_init(&(ccuInfo.SpinLockIrqCnt[n]));
	}
	spin_lock_init(&(ccuInfo.SpinLockRTBC));
	spin_lock_init(&(ccuInfo.SpinLockClock));
	spin_lock_init(&(ccuInfo.SpinLockI2cPower));
	ccuInfo.IsI2cPoweredOn = 0;
	ccuInfo.IsI2cPowerDisabling = 0;
	/**/
	ccu_ap_task_mgr_init();

	ccu_base = (uint64_t)device->ccu_base;
	camsys_base = (uint64_t)device->camsys_base;
	bin_base = (uint64_t)device->bin_base;
	dmem_base = (uint64_t)device->dmem_base;
	pmem_base = (uint64_t)device->pmem_base;

	ccu_dev = device;

	LOG_DBG("(0x%llx),(0x%llx),(0x%llx)\n",
		ccu_base, camsys_base, bin_base);


	if (
		request_irq(device->irq_num, ccu_isr_handler,
			IRQF_TRIGGER_NONE, "ccu", NULL)) {
		LOG_ERR("fail to request ccu irq!\n");
		ret = -ENODEV;
		goto out;
	}

out:
	return ret;
}

int ccu_uninit_hw(struct ccu_device_s *device)
{
	ccu_i2c_free_dma_buf_mva_all();

	if (enque_task) {
		kthread_stop(enque_task);
		enque_task = NULL;
	}

	flush_workqueue(ap_task_manage.ApTaskWorkQueue);
	destroy_workqueue(ap_task_manage.ApTaskWorkQueue);

	return 0;
}

int ccu_mmap_hw(struct file *filp, struct vm_area_struct *vma)
{
	return 0;
}

int ccu_memcpy(void *dest, void *src, int length)
{
	int i = 0;

	char *destPtr = (char *)dest;
	char *srcPtr = (char *)src;

	for (i = 0; i < length; i++)
		destPtr[i] = srcPtr[i];

	return length;
}

int ccu_memclr(void *dest, int length)
{
	int i = 0;

	char *destPtr = (char *)dest;

	for (i = 0; i < length; i++)
		destPtr[i] = 0;

	return length;
}

int ccu_power(struct ccu_power_s *power)
{
	int ret = 0;
	int32_t timeout = 10;

	LOG_DBG("+:%s,(0x%llx)(0x%llx)\n", __func__, ccu_base, camsys_base);
	LOG_DBG("power->bON: %d\n", power->bON);

	if (power->bON == 1) {
		/*CCU power on sequence*/
		ccu_clock_enable();

		/*0. Set CCU_A_RESET. CCU_HW_RST=1*/
		/*TSF be affected.*/
		ccu_write_reg(ccu_base, RESET, 0xFF3FFCFF);
		/*CCU_HW_RST.*/
		ccu_write_reg(ccu_base, RESET, 0x00010000);
		LOG_DBG("reset wrote\n");
		/*ccu_write_reg_bit(ccu_base, RESET, CCU_HW_RST, 1);*/

		/*1. Enable CCU CAMSYS_CG_CON bit12 CCU_CGPDN=0*/
		LOG_DBG("CCU CG released\n");

		/*use user space buffer*/
		ccu_write_reg(ccu_base, CCU_DATA_REG_LOG_BUF0,
			power->workBuf.mva_log[0]);
		ccu_write_reg(ccu_base, CCU_DATA_REG_LOG_BUF1,
			power->workBuf.mva_log[1]);

		LOG_DBG("LogBuf_mva[0](0x%x)\n", power->workBuf.mva_log[0]);
		LOG_DBG("LogBuf_mva[1](0x%x)\n", power->workBuf.mva_log[1]);

		ccuInfo.IsI2cPoweredOn = 1;
		ccuInfo.IsCcuPoweredOn = 1;

	} else if (power->bON == 0) {
		/*CCU Power off*/
		if (ccuInfo.IsCcuPoweredOn)
			ret = _ccu_powerdown();
		else
			LOG_DBG_MUST("ccu not power on yet\n");

	} else if (power->bON == 2) {
		/*Restart CCU, no need to release CG*/

		/*0. Set CCU_A_RESET. CCU_HW_RST=1*/
		/*TSF be affected.*/
		ccu_write_reg(ccu_base, RESET, 0xFF3FFCFF);
		/*CCU_HW_RST.*/
		ccu_write_reg(ccu_base, RESET, 0x00010000);
		LOG_DBG("reset wrote\n");
		/*ccu_write_reg_bit(ccu_base, RESET, CCU_HW_RST, 1);*/

		/*use user space buffer*/
		ccu_write_reg(ccu_base, CCU_DATA_REG_LOG_BUF0,
			power->workBuf.mva_log[0]);
		ccu_write_reg(ccu_base, CCU_DATA_REG_LOG_BUF1,
			power->workBuf.mva_log[1]);

		LOG_DBG("LogBuf_mva[0](0x%x)\n", power->workBuf.mva_log[0]);
		LOG_DBG("LogBuf_mva[1](0x%x)\n", power->workBuf.mva_log[1]);
	} else if (power->bON == 3) {
		/*Pause CCU, but don't pullup CG*/

		/*Check CCU halt status*/
		while (
		(ccu_read_reg(ccu_base, CCU_STA_REG_SW_INIT_DONE) !=
			CCU_STATUS_INIT_DONE_2)
		&& (timeout >= 0)) {
			mdelay(1);
			LOG_DBG("wait ccu halt done\n");
			// LOG_DBG("ccu halt stat: %x\n",
			// ccu_read_reg_bit(ccu_base, DONE_ST, CCU_HALT));
			timeout = timeout - 1;
		}

		if (timeout <= 0) {
			LOG_ERR("ccu_pause timeout\n");
			return -ETIMEDOUT;
		}

		/*Set CCU_A_RESET. CCU_HW_RST=1*/
		ccu_write_reg_bit(ccu_base, RESET, CCU_HW_RST, 1);

		ccuInfo.IsCcuPoweredOn = 0;

	} else if (power->bON == 4) {
		/*CCU boot fail, just enable CG*/
		if (ccuInfo.IsCcuPoweredOn == 1) {
			ccu_clock_disable();
			ccuInfo.IsCcuPoweredOn = 0;
		}

	} else {
		LOG_ERR("invalid power option: %d\n", power->bON);
	}

	LOG_DBG("-:%s\n", __func__);
	return ret;
}

int ccu_force_powerdown(void)
{
	int ret = 0;

	if (ccuInfo.IsCcuPoweredOn == 1) {
		LOG_WARN(
		"CCU kernel drv released on CCU running, try to force shutdown\n");

		/*Set special isr task to MSG_TO_CCU_SHUTDOWN*/
		ccu_write_reg(ccu_base, CCU_INFO29, MSG_TO_CCU_SHUTDOWN);
		/*Interrupt to CCU*/
		/* MCU write this field to trigger ccu interrupt pulse */
		ccu_write_reg_bit(ccu_base, CTL_CCU_INT, INT_CTL_CCU, 1);

		ret = _ccu_powerdown();
		mdelay(60);

		if (ret < 0)
			return ret;

		LOG_WARN("CCU force shutdown success\n");
	}

	return 0;
}

static int _ccu_powerdown(void)
{
	int32_t timeout = 10;
	unsigned long flags;

	if (ccu_read_reg_bit(ccu_base, RESET, CCU_HW_RST) == 1) {
		LOG_INF_MUST("ccu reset is up, skip halt checking.\n");
	} else {
		while ((ccu_read_reg_bit(ccu_base, CCU_ST, CCU_SYS_HALT) == 0)
			&& timeout > 0) {
			mdelay(1);
			LOG_DBG("wait ccu shutdown done\n");
			// LOG_DBG("ccu shutdown stat: %x\n",
			// ccu_read_reg_bit(ccu_base, DONE_ST, CCU_HALT));
			timeout = timeout - 1;
		}

		if (timeout <= 0) {
			LOG_ERR("%s timeout\n", __func__);
/*Even timed-out, clock disable is still necessary, DO NOT return here.*/
		}
	}

	/*Set CCU_A_RESET. CCU_HW_RST=1*/
	ccu_write_reg_bit(ccu_base, RESET, CCU_HW_RST, 1);
	/*CCF*/
	ccu_clock_disable();

	spin_lock_irqsave(&ccuInfo.SpinLockI2cPower, flags);
	ccuInfo.IsI2cPowerDisabling = 1;
	spin_unlock_irqrestore(&ccuInfo.SpinLockI2cPower, flags);

	ccu_i2c_controller_uninit_all();
	ccu_i2c_free_dma_buf_mva_all();
	ccuInfo.IsI2cPoweredOn = 0;
	ccuInfo.IsI2cPowerDisabling = 0;
	ccuInfo.IsCcuPoweredOn = 0;

	return 0;
}

int ccu_run(struct ccu_run_s *info)
{
	int32_t timeout = 10000;
	struct ccu_mailbox_t *ccuMbPtr = NULL;
	struct ccu_mailbox_t *apMbPtr = NULL;
	uint32_t status;
	MUINT32 remapOffset;
	struct CcuMemInfo *bin_mem = ccu_get_binary_memory();

	LOG_DBG("+:%s\n", __func__);
	if (bin_mem == NULL) {
		LOG_ERR("CCU RUN failed, bin_mem NULL\n");
		return -EINVAL;
	}


	remapOffset = bin_mem->mva - CCU_CACHE_BASE;
	ccu_write_reg(ccu_base, CCU_AXI_REMAP, remapOffset);
	LOG_INF_MUST("set CCU remap offset: %x\n", remapOffset);
	ccu_write_reg(ccu_base, CCU_INFO20, info->log_level);


	/*smp_inner_dcache_flush_all();*/
	/*LOG_DBG("cache flushed 2\n");*/
	/*3. Set CCU_A_RESET. CCU_HW_RST=0*/
	/*ccu_write_reg_bit(ccu_base, RESET, CCU_HW_RST, 0);*/
	ccu_write_reg_bit(ccu_base, CCU_CTL, CCU_RUN_REQ, 1);

	do {
		status = ccu_read_reg(ccu_base, CCU_ST);
		LOG_DBG("wait ccu run : %x\n", status);
	} while (!(status&0x10));

	ccu_write_reg_bit(ccu_base, CCU_CTL, CCU_RUN_REQ, 0);

	LOG_DBG("released CCU reset, wait for initial done, %x\n",
		ccu_read_reg(ccu_base, RESET));
	LOG_DBG("CCU reset: %x\n", ccu_read_reg(ccu_base, RESET));

	/*4. Pulling CCU init done spare register*/
	while (
	(ccu_read_reg(ccu_base, CCU_STA_REG_SW_INIT_DONE)
		!= CCU_STATUS_INIT_DONE)
	&& (timeout >= 0)) {
		udelay(100);
		LOG_DBG("wait ccu initial done\n");
		LOG_DBG("ccu initial stat: %x\n",
			ccu_read_reg(ccu_base, CCU_STA_REG_SW_INIT_DONE));
		timeout = timeout - 1;
	}

	if (timeout <= 0) {
		LOG_ERR("CCU init timeout\n");
		LOG_ERR("ccu initial debug info: %x\n",
			ccu_read_reg(ccu_base, CCU_INFO28));
		return -ETIMEDOUT;
	}

	LOG_DBG_MUST("ccu initial done\n");
	LOG_DBG_MUST("ccu initial stat: %x\n",
		ccu_read_reg(ccu_base, CCU_STA_REG_SW_INIT_DONE));
	LOG_DBG_MUST("ccu initial debug info: %x\n",
		ccu_read_reg(ccu_base, CCU_INFO29));
	LOG_DBG_MUST("ccu initial debug info00: %x\n",
		ccu_read_reg(ccu_base, CCU_INFO00));
	LOG_DBG_MUST("ccu initial debug info01: %x\n",
		ccu_read_reg(ccu_base, CCU_INFO01));

/*
 * 20160930
 * Due to AHB2GMC HW Bug, mailbox use SRAM
 * Driver wait CCU main initialize done and
 * query INFO00 & INFO01 as mailbox address
 */
	pMailBox[MAILBOX_SEND] =
		(struct ccu_mailbox_t *)(uintptr_t)(dmem_base +
			ccu_read_reg(ccu_base, CCU_DATA_REG_MAILBOX_CCU));
	pMailBox[MAILBOX_GET] =
		(struct ccu_mailbox_t *)(uintptr_t)(dmem_base +
			ccu_read_reg(ccu_base, CCU_DATA_REG_MAILBOX_APMCU));


	ccuMbPtr = (struct ccu_mailbox_t *) pMailBox[MAILBOX_SEND];
	apMbPtr = (struct ccu_mailbox_t *) pMailBox[MAILBOX_GET];

	mailbox_init(apMbPtr, ccuMbPtr);

	/*tell ccu that driver has initialized mailbox*/
	ccu_write_reg(ccu_base, CCU_STA_REG_SW_INIT_DONE, 0);

	timeout = 100;
	while (
	(ccu_read_reg(ccu_base, CCU_STA_REG_SW_INIT_DONE)
		!= CCU_STATUS_INIT_DONE_2)
	&& (timeout >= 0)) {
		udelay(100);
		LOG_DBG_MUST("wait ccu log test\n");
		timeout = timeout - 1;
	}

	if (timeout <= 0) {
		LOG_ERR("CCU init timeout 2\n");
		LOG_ERR("ccu initial debug info: %x\n",
			ccu_read_reg(ccu_base, CCU_INFO28));
		return -ETIMEDOUT;
	}

	LOG_DBG_MUST("ccu log test done\n");
	LOG_DBG_MUST("ccu log test stat: %x\n",
			ccu_read_reg(ccu_base, CCU_STA_REG_SW_INIT_DONE));
	LOG_DBG_MUST("ccu log test debug info: %x\n",
		ccu_read_reg(ccu_base, CCU_INFO29));

	ccu_ipc_init((uint32_t *)dmem_base, (uint32_t *)ccu_base);

	LOG_DBG_MUST("-:%s\n", __func__);

	return 0;
}


int ccu_waitirq(struct CCU_WAIT_IRQ_STRUCT *WaitIrq)
{
	signed int ret = 0, Timeout = WaitIrq->EventInfo.Timeout;

	LOG_DBG("Clear(%d),bWaitCond(%d),Timeout(%d)\n",
		WaitIrq->EventInfo.Clear, bWaitCond, Timeout);
	LOG_DBG("arg is struct CCU_WAIT_IRQ_STRUCT, size:%zu\n",
		sizeof(struct CCU_WAIT_IRQ_STRUCT));

	if (Timeout != 0) {
		/* 2. start to wait signal */
		LOG_DBG("+:wait_event_interruptible_timeout\n");
		Timeout = wait_event_interruptible_timeout(
			ccuInfo.WaitQueueHead,
			bWaitCond,
			CCU_MsToJiffies(WaitIrq->EventInfo.Timeout));
		bWaitCond = false;
		LOG_DBG("-:wait_event_interruptible_timeout\n");
	} else {
		LOG_DBG("+:ccu wait_event_interruptible\n");
		/*task_count_temp = atomic_read(&(ccuInfo.taskCount))*/
		/*if(task_count_temp == 0)*/
		/*{*/

		mutex_unlock(&ap_task_manage.ApTaskMutex);
		LOG_DBG("unlock ApTaskMutex\n");
		wait_event_interruptible(ccuInfo.WaitQueueHead, bWaitCond);
		LOG_DBG("accuiring ApTaskMutex\n");
		mutex_lock(&ap_task_manage.ApTaskMutex);
		LOG_DBG("got ApTaskMutex\n");
		/*}*/
		/*else*/
		/*{*/
		/*LOG_DBG(*/
		/*"ccuInfo.taskCount is not zero: %d\n",*/
		/*task_count_temp);*/
		/*}*/
		bWaitCond = false;
		LOG_DBG("-:ccu wait_event_interruptible\n");
	}

	if (Timeout > 0) {
		LOG_DBG("remain timeout:%d, task: %d\n",
			Timeout, g_LogBufIdx);
		/*send to user if not timeout*/
		WaitIrq->EventInfo.TimeInfo.passedbySigcnt = (int)g_LogBufIdx;
	}
	/*EXIT:*/

	return ret;
}

int ccu_AFwaitirq(struct CCU_WAIT_IRQ_STRUCT *WaitIrq, int tg_num)
{
	signed int ret = 0, Timeout = WaitIrq->EventInfo.Timeout;

	LOG_DBG("Clear(%d),AFbWaitCond(%d),Timeout(%d)\n",
		WaitIrq->EventInfo.Clear, AFbWaitCond[tg_num-1], Timeout);
	LOG_DBG("arg is struct CCU_WAIT_IRQ_STRUCT, size:%zu\n",
		sizeof(struct CCU_WAIT_IRQ_STRUCT));

	if (Timeout != 0) {
		/* 2. start to wait signal */
		LOG_DBG("+:wait_event_interruptible_timeout\n");
	AFbWaitCond[tg_num-1] = false;
		Timeout = wait_event_interruptible_timeout(
			ccuInfo.AFWaitQueueHead[tg_num-1],
			AFbWaitCond[tg_num-1],
			CCU_MsToJiffies(WaitIrq->EventInfo.Timeout));

		LOG_DBG("-:wait_event_interruptible_timeout\n");
	} else {
		LOG_DBG("+:ccu wait_event_interruptible\n");
		/*task_count_temp = atomic_read(&(ccuInfo.taskCount))*/
		/*if(task_count_temp == 0)*/
		/*{*/

		mutex_unlock(&ap_task_manage.ApTaskMutex);
		LOG_DBG("unlock ApTaskMutex\n");
		wait_event_interruptible(ccuInfo.AFWaitQueueHead[tg_num-1],
			AFbWaitCond[tg_num-1]);
		LOG_DBG("accuiring ApTaskMutex\n");
		mutex_lock(&ap_task_manage.ApTaskMutex);
		LOG_DBG("got ApTaskMutex\n");
		/*}*/
		/*else*/
		/*{*/
		/*LOG_DBG("ccuInfo.taskCount is not zero: %d\n",*/
		/* task_count_temp);*/
		/*}*/
		AFbWaitCond[tg_num-1] = false;
		LOG_DBG("-:ccu wait_event_interruptible\n");
	}

	if (Timeout > 0) {
		LOG_DBG("remain timeout:%d, task: %d\n", Timeout,
			AFg_LogBufIdx[tg_num-1]);
		/*send to user if not timeout*/
		WaitIrq->EventInfo.TimeInfo.passedbySigcnt =
		 (int)AFg_LogBufIdx[tg_num-1];
	}
	/*EXIT:*/

	return ret;
}

int ccu_flushLog(int argc, int *argv)
{
	LOG_DBG("bWaitCond(%d)\n", bWaitCond);

	bWaitCond = true;

	wake_up_interruptible(&ccuInfo.WaitQueueHead);

	LOG_DBG("bWaitCond(%d)\n", bWaitCond);
	return 0;
}

int ccu_read_info_reg(int regNo)
{
	int *offset;

	if (regNo < 0 || regNo >= 32) {
		LOG_ERR("invalid regNo");
		return 0;
	}

	offset = (int *)(uintptr_t)(ccu_base + 0x60 + regNo * 4);

	LOG_DBG("%s: %x\n", __func__, (unsigned int)(*offset));

	return *offset;
}

void ccu_print_reg(uint32_t *Reg)
{
	int i;
	uint32_t offset = 0;
	uint32_t *ccuCtrlPtr = Reg;
	uint32_t *ccuDmPtr = Reg + (CCU_HW_DUMP_SIZE>>2);
	uint32_t *ccuPmPtr = Reg + (CCU_HW_DUMP_SIZE>>2) + (CCU_DMEM_SIZE>>2);

	for (i = 0 ; i < CCU_HW_DUMP_SIZE ; i += 16) {
		*(ccuCtrlPtr+offset) = *(uint32_t *)(ccu_base + i);
		*(ccuCtrlPtr+offset + 1) = *(uint32_t *)(ccu_base + i + 4);
		*(ccuCtrlPtr+offset + 2) = *(uint32_t *)(ccu_base + i + 8);
		*(ccuCtrlPtr+offset + 3) = *(uint32_t *)(ccu_base + i + 12);
		offset += 4;
	}
	offset = 0;
	for (i = 0 ; i < CCU_DMEM_SIZE ; i += 16) {
		*(ccuDmPtr+offset) = *(uint32_t *)(dmem_base + i);
		*(ccuDmPtr+offset + 1) = *(uint32_t *)(dmem_base + i + 4);
		*(ccuDmPtr+offset + 2) = *(uint32_t *)(dmem_base + i + 8);
		*(ccuDmPtr+offset + 3) = *(uint32_t *)(dmem_base + i + 12);
		offset += 4;
	}
	offset = 0;
	for (i = 0 ; i < CCU_PMEM_SIZE ; i += 16) {
		*(ccuPmPtr+offset) = *(uint32_t *)(pmem_base + i);
		*(ccuPmPtr+offset + 1) = *(uint32_t *)(pmem_base + i + 4);
		*(ccuPmPtr+offset + 2) = *(uint32_t *)(pmem_base + i + 8);
		*(ccuPmPtr+offset + 3) = *(uint32_t *)(pmem_base + i + 12);
		offset += 4;
	}
}

void ccu_print_sram_log(char *sram_log)
{
	int i;
	uint32_t offset = ccu_read_reg(ccu_base, CCU_INFO25);
	char *ccuLogPtr_1 = (char *)dmem_base + offset;
	char *ccuLogPtr_2 = (char *)dmem_base + offset + CCU_LOG_SIZE;
	char *isrLogPtr = (char *)dmem_base + offset + (CCU_LOG_SIZE * 2);

	MUINT32 *from_sram;
	MUINT32 *to_dram;

	from_sram = (MUINT32 *)ccuLogPtr_1;
	to_dram = (MUINT32 *)sram_log;
	for (i = 0; i < CCU_LOG_SIZE/4-1; i++)
		*(to_dram+i) = *(from_sram+i);
	from_sram = (MUINT32 *)ccuLogPtr_2;
	to_dram = (MUINT32 *)(sram_log + CCU_LOG_SIZE);
	for (i = 0; i < CCU_LOG_SIZE/4-1; i++)
		*(to_dram+i) = *(from_sram+i);
	from_sram = (MUINT32 *)isrLogPtr;
	to_dram = (MUINT32 *)(sram_log + (CCU_LOG_SIZE * 2));
	for (i = 0; i < CCU_ISR_LOG_SIZE/4-1; i++)
		*(to_dram+i) = *(from_sram+i);
}

int ccu_read_data(uint32_t *buf, uint32_t ccu_da, uint32_t size)
{
	int i;
	uint32_t *ptr = ccu_da_to_va(ccu_da, size*sizeof(uint32_t));

	LOG_DBG("%s: %x(%x)\n", __func__, ccu_da, size);
	if (ccu_da%4)
		return -EINVAL;
	if (ptr == NULL) {
		LOG_ERR("%s: ptr null\n", __func__);
		return -EINVAL;
	}
	for (i = 0; i < size; i++)
		buf[i] = ptr[i];
	return 0;
}

int ccu_query_power_status(void)
{
	return ccuInfo.IsCcuPoweredOn;
}

CCU_FW_PUBK;
CCU_DRV_PUBK;

int ccu_load_bin(struct ccu_device_s *device, struct ccu_bin_info_s *bin_info)
{
	const struct firmware *firmware_p;
	int ret = 0;

	ret = request_firmware(&firmware_p, bin_info->name, device->dev);
	if (ret < 0) {
		LOG_ERR("request_firmware failed: %d\n", ret);
		goto EXIT;
	}

	ret = ccu_sanity_check(firmware_p);
	if (ret < 0) {
		LOG_ERR("sanity check failed: %d\n", ret);
		goto EXIT;
	}

	if (bin_info->type == CCU_DRIVER_BIN) {
		ret = ccu_cert_check(firmware_p, g_ccu_drv_pubk,
			CCU_DRV_PUBK_SZ);
	} else {
		ret = ccu_cert_check(firmware_p, g_ccu_pubk,
			CCU_FW_PUBK_SZ);
	}
	if (ret < 0) {
		LOG_ERR("cert check failed: %d\n", ret);
		goto EXIT;
	}


	ret = ccu_load_segments(firmware_p, bin_info->type);
	if (ret < 0)
		LOG_ERR("load segments failed: %d\n", ret);
EXIT:
	release_firmware(firmware_p);
	return ret;
}

struct tcrypt_result {
	struct completion completion;
	int err;
};

static void tcrypt_complete(struct crypto_async_request *req, int err)
{
	struct tcrypt_result *res = (struct tcrypt_result *) req->data;

	if (err == -EINPROGRESS)
		return;

	res->err = err;
	complete(&res->completion);
}

static int wait_async_op(struct tcrypt_result *tr, int ret)
{
	if (ret == -EINPROGRESS || ret == -EBUSY) {
		wait_for_completion(&tr->completion);
		reinit_completion(&tr->completion);
		ret = tr->err;
	}

	return ret;
}

int ccu_cert_check(const struct firmware *fw, uint8_t *pubk, uint32_t key_size)
{
	uint8_t hash[32];
	uint8_t *cert = NULL;
	uint8_t *sign = NULL;
	uint8_t *digest = NULL;
	int cert_len = 0x110;
	int block_len = 0x100;
	struct crypto_shash *alg = NULL;
	struct crypto_akcipher *rsa_alg = NULL;
	struct akcipher_request *req = NULL;
	struct tcrypt_result result;
	struct sdesc *sdesc = NULL;
	struct scatterlist sg_in;
	struct scatterlist sg_out;
	uint32_t firmware_size, size;
	int ret, i;

	LOG_DBG_MUST("%s+\n", __func__);
	if (fw->size < cert_len) {
		LOG_ERR("firmware size small than cert\n");
		return -EINVAL;
	}

	cert = (uint8_t *)fw->data + fw->size - cert_len;

	alg = crypto_alloc_shash("sha256", 0, 0);
	if (IS_ERR(alg)) {
		LOG_ERR("can't alloc alg sha256\n");
		ret = -EINVAL;
		goto free_req;
	}
	size = sizeof(struct shash_desc) + crypto_shash_descsize(alg);
	sdesc = kmalloc(size, GFP_KERNEL);
	if (!sdesc) {
		LOG_ERR("can't alloc sdesc\n");
		ret = -ENOMEM;
		goto free_req;
	}
	digest = kmalloc(block_len, GFP_KERNEL);
	if (!digest) {
		LOG_ERR("can't alloc sdesc\n");
		ret = -ENOMEM;
		goto free_req;
	}
	sign = kmalloc(block_len, GFP_KERNEL);
	if (!sign) {
		LOG_ERR("can't alloc sdesc\n");
		ret = -ENOMEM;
		goto free_req;
	}

	firmware_size = *(uint32_t *)(cert);
	sdesc->shash.tfm = alg;
	ret = crypto_shash_digest(&sdesc->shash, fw->data, firmware_size, hash);

	memcpy(sign, cert + 0x10, block_len);
	rsa_alg = crypto_alloc_akcipher("rsa", 0, 0);
	if (IS_ERR(rsa_alg)) {
		LOG_ERR("can't alloc alg %ld\n", PTR_ERR(rsa_alg));
		goto free_req;
	}

	req = akcipher_request_alloc(rsa_alg, GFP_KERNEL);
	if (!req) {
		LOG_ERR("can't request alg rsa\n");
		goto free_req;
	}

	ret = crypto_akcipher_set_pub_key(rsa_alg, pubk, key_size);
	if (ret) {
		LOG_ERR("set pubkey err %d %d\n", ret, key_size);
		goto free_req;
	}

	sg_init_one(&sg_in, sign, block_len);
	sg_init_one(&sg_out, digest, block_len);

	akcipher_request_set_crypt(req, &sg_in, &sg_out, block_len, block_len);
	init_completion(&result.completion);

	akcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
		tcrypt_complete, &result);
	ret = wait_async_op(&result, crypto_akcipher_verify(req));
	if (ret) {
		LOG_ERR("verify err %d\n", ret);
		goto free_req;
	}

	if (memcmp(digest + 0xE0, hash, 0x20)) {
		LOG_ERR("firmware is corrupted\n");
		LOG_ERR("digest:\n");
		for (i = 0xE0; i < 0x100; i += 8) {
			LOG_ERR("%02x%02x%02x%02x%02x%02x%02x%02x\n",
			digest[i], digest[i+1], digest[i+2], digest[i+3],
			digest[i+4], digest[i+5], digest[i+6], digest[i+7]);
		}
		LOG_ERR("hash:\n");
		for (i = 0; i < 32; i += 8) {
			LOG_INF_MUST("%02x%02x%02x%02x%02x%02x%02x%02x\n",
			hash[i], hash[i+1], hash[i+2], hash[i+3],
			hash[i+4], hash[i+5], hash[i+6], hash[i+7]);
		}
		LOG_ERR("cert:\n");
		for (i = 0; i < 32; i += 8) {
			LOG_INF_MUST("%02x%02x%02x%02x%02x%02x%02x%02x\n",
			cert[i], cert[i+1], cert[i+2], cert[i+3],
			cert[i+4], cert[i+5], cert[i+6], cert[i+7]);
		}
		ret = -EINVAL;
	}

free_req:
	if (rsa_alg)
		crypto_free_akcipher(rsa_alg);
	if (req)
		akcipher_request_free(req);
	if (alg)
		crypto_free_shash(alg);
	kfree(sdesc);
	LOG_DBG_MUST("%s-\n", __func__);
	return ret;
}

int ccu_sanity_check(const struct firmware *fw)
{
	// const char *name = rproc->firmware;
	struct elf32_hdr *ehdr;
	uint32_t phdr_offset;
	char class;

	if (!fw) {
		LOG_ERR("failed to load ccu_bin\n");
		return -EINVAL;
	}

	if (fw->size < sizeof(struct elf32_hdr)) {
		LOG_ERR("Image is too small\n");
		return -EINVAL;
	}

	ehdr = (struct elf32_hdr *)fw->data;

	/* We only support ELF32 at this point */
	class = ehdr->e_ident[EI_CLASS];
	if (class != ELFCLASS32) {
		LOG_ERR("Unsupported class: %d\n", class);
		return -EINVAL;
	}

	/* We assume the firmware has the same endianness as the host */
# ifdef __LITTLE_ENDIAN
	if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
# else /* BIG ENDIAN */
	if (ehdr->e_ident[EI_DATA] != ELFDATA2MSB) {
# endif
		LOG_ERR("Unsupported firmware endianness\n");
		return -EINVAL;
	}

	if (fw->size < ehdr->e_shoff + sizeof(struct elf32_shdr)) {
		LOG_ERR("Image is too small\n");
		return -EINVAL;
	}

	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) {
		LOG_ERR("Image is corrupted (bad magic)\n");
		return -EINVAL;
	}

	if ((ehdr->e_phnum == 0) || (ehdr->e_phnum > CCU_HEADER_NUM)) {
		LOG_ERR("loadable segments is invalid: %x\n", ehdr->e_phnum);
		return -EINVAL;
	}

	phdr_offset = ehdr->e_phoff + sizeof(struct elf32_phdr) * ehdr->e_phnum;
	if (phdr_offset > fw->size) {
		LOG_ERR("Firmware size is too small\n");
		return -EINVAL;
	}

	return 0;
}

static void ccu_load_memcpy(void *dst, const void *src, uint32_t len)
{
	int i;

	for (i = 0; i < len/4; ++i)
		writel(*((uint32_t *)src+i), (uint32_t *)dst+i);
}

static void ccu_load_memclr(void *dst, uint32_t len)
{
	int i = 0;

	for (i = 0; i < len/4; ++i)
		writel(0, (uint32_t *)dst+i);
}

int ccu_load_segments(const struct firmware *fw, enum CCU_BIN_TYPE type)
{
	struct elf32_hdr *ehdr;
	struct elf32_phdr *phdr;
	int i, ret = 0;
	int timeout = 10;
	unsigned int status;
	const u8 *elf_data = fw->data;

	/*0. Set CCU_A_RESET. CCU_HW_RST=1*/
	if (type == CCU_DP_BIN) {
		ccu_write_reg(ccu_base, RESET, 0x0);
		udelay(10);
		status = ccu_read_reg(ccu_base, CCU_ST);
		while (!(status & 0x100)) {
			status = ccu_read_reg(ccu_base, CCU_ST);
			udelay(300);
			if (timeout < 0 && !(status & 0x100)) {
				LOG_ERR("ccu halt before load bin, timeout");
				return -EFAULT;
			}
			timeout--;
		}
	}
	ehdr = (struct elf32_hdr *)elf_data;
	phdr = (struct elf32_phdr *)(elf_data + ehdr->e_phoff);
	// dev_info(dev, "ehdr->e_phnum %d\n", ehdr->e_phnum);
	/* go through the available ELF segments */
	for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
		u32 da = phdr->p_paddr;
		u32 memsz = phdr->p_memsz;
		u32 filesz = phdr->p_filesz;
		u32 offset = phdr->p_offset;
		void *ptr;

		if (phdr->p_type != PT_LOAD)
			continue;

		LOG_ERR("phdr: type %d da 0x%x memsz 0x%x filesz 0x%x\n",
			phdr->p_type, da, memsz, filesz);

		if (filesz > memsz) {
			LOG_ERR("bad phdr filesz 0x%x memsz 0x%x\n",
				filesz, memsz);
			ret = -EINVAL;
			break;
		}

		if (offset + filesz > fw->size) {
			LOG_ERR("truncated fw: need 0x%x avail 0x%zx\n",
				offset + filesz, fw->size);
			ret = -EINVAL;
			break;
		}

		/* grab the kernel address for this device address */
		ptr = ccu_da_to_va(da, memsz);
		if (!ptr) {
			LOG_ERR("bad phdr da 0x%x mem 0x%x\n", da, memsz);
			// ret = -EINVAL;
			continue;
		}

		/* put the segment where the remote processor expects it */
		if (phdr->p_filesz) {
			ccu_load_memcpy(ptr,
				(void *)elf_data + phdr->p_offset, filesz);
		}

		/*
		 * Zero out remaining memory for this segment.
		 *
		 * This isn't strictly required since dma_alloc_coherent already
		 * did this for us. albeit harmless, we may consider removing
		 * this.
		 */
		if (memsz > filesz)
			ccu_load_memclr(ptr + filesz, memsz - filesz);
	}

	return ret;
}

void *ccu_da_to_va(u64 da, uint32_t len)
{
	int offset;
	struct CcuMemInfo *bin_mem = ccu_get_binary_memory();

	if (bin_mem == NULL) {
		LOG_ERR("failed lookup da(%x), bin_mem NULL", da);
		return NULL;
	}
	if (da < CCU_CACHE_BASE) {
		offset = da;
		if ((len & 0x3) || (da & 0x3)) {
			LOG_ERR("[%s] align violation: da(0x%x) size(0x%x)\n",
				__func__, da, len);
			return NULL;
		} else if ((offset >= 0) && ((offset + len) < CCU_PMEM_SIZE)) {
			LOG_DBG("da(0x%lx) to va(0x%lx)",
				da, pmem_base + offset);
			return (uint32_t *)(pmem_base + offset);
		}
	} else if (da >= CCU_CORE_DMEM_BASE) {
		offset = da - CCU_CORE_DMEM_BASE;
		if ((len & 0x3) || (da & 0x3)) {
			LOG_ERR("[%s] align violation: da(0x%x) size(0x%x)\n",
				__func__, da, len);
			return NULL;
		} else if ((offset >= 0) && ((offset + len) < CCU_DMEM_SIZE)) {
			LOG_DBG("da(0x%lx) to va(0x%lx)",
				da, dmem_base + offset);
			return (uint32_t *)(dmem_base + offset);
		}
	} else {
		offset = da - CCU_CACHE_BASE;
		if ((offset >= 0) &&
		((offset + len) < bin_mem->size)) {
			LOG_DBG("da(0x%lx) to va(0x%lx)",
				da, bin_mem->va + offset);
			return (uint32_t *)(bin_mem->va + offset);
		}
	}

	LOG_ERR("failed lookup da(%x) len(%x) to va, offset(%x)", da, offset);
	return NULL;
}

int ccu_sw_hw_reset(void)
{
	uint32_t duration = 0;
	uint32_t ccu_status;
	uint32_t ccu_reset;
	//check halt is up

	ccu_status = ccu_read_reg(ccu_base, CCU_ST);
	LOG_INF_MUST("[%s] polling CCU halt(0x%08x)\n", __func__, ccu_status);
	duration = 0;
	while ((ccu_status & 0x100) != 0x100) {
		duration++;
		if (duration > 1000) {
			LOG_ERR("[%s] polling halt, 1ms timeout: (0x%08x)\n",
				__func__, ccu_status);
			break;
		}
		udelay(10);
		ccu_status = ccu_read_reg(ccu_base, CCU_ST);
	}
	LOG_INF_MUST("[%s] polling CCU halt done(0x%08x)\n",
		__func__, ccu_status);

	//do SW reset
	LOG_INF_MUST("[%s] CCU SW reset: before(0x%08x)\n",
		__func__, ccu_status);
	ccu_reset = ccu_read_reg(ccu_base, RESET);
	ccu_write_reg(ccu_base, RESET, ccu_reset | 0x700);
	LOG_INF_MUST("[%s] CCU SW reset: after(0x%08x)\n",
		__func__, ccu_status);

	LOG_INF_MUST("[%s] polling CCU SW reset(0x%08x)\n",
		__func__, ccu_status);
	duration = 0;
	ccu_reset = ccu_read_reg(ccu_base, RESET);
	while ((ccu_reset & 0x7) != 0x7) {
		duration++;
		if (duration > 1000) {
			LOG_ERR("[%s] polling reset, 1ms timeout: (0x%08x)\n",
				__func__, ccu_reset);
			break;
		}
		udelay(10);
		ccu_reset = ccu_read_reg(ccu_base, RESET);
	}
	LOG_INF_MUST("[%s] polling CCU SW reset done(0x%08x)\n",
		__func__, ccu_reset);
	LOG_INF_MUST("[%s] release CCU SW reset: before(0x%08x)\n",
		__func__, ccu_reset);
	ccu_write_reg(ccu_base, RESET, ccu_reset & (~0x700));
	ccu_reset = ccu_read_reg(ccu_base, RESET);

	LOG_INF_MUST("[%s] release CCU SW reset: after(0x%08x)\n",
		__func__, ccu_reset);

	//do HW reset
	LOG_INF_MUST("[%s] CCU HW reset: before(0x%08x)\n",
		__func__, ccu_reset);
	ccu_write_reg(ccu_base, RESET, ccu_reset | 0xFF0000);
	ccu_reset = ccu_read_reg(ccu_base, RESET);

	LOG_INF_MUST("[%s] CCU HW reset: after(0x%08x)\n",
		__func__, ccu_reset);
	LOG_INF_MUST("[%s] release CCU HW reset: before(0x%08x)\n",
		__func__, ccu_reset);
	ccu_write_reg(ccu_base, RESET, ccu_reset & (~0xFF0000));
	ccu_reset = ccu_read_reg(ccu_base, RESET);

	LOG_INF_MUST("[%s] release CCU HW reset: after(0x%08x)\n",
		__func__, ccu_reset);

	return true;

}
