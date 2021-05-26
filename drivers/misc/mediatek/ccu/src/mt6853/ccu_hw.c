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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/iommu.h>
#include <linux/firmware.h>

#include "mtk_ion.h"
#include "ion_drv.h"
#include <linux/iommu.h>

#ifdef CONFIG_MTK_IOMMU_V2
#include "mtk_iommu.h"
#include <dt-bindings/memory/mt6853-larb-port.h>
#else
#include "m4u.h"
#endif

#include <linux/io.h> /*for mb();*/

#include "ccu_hw.h"
#include "ccu_reg.h"
#include "ccu_cmn.h"
#include "ccu_kd_mailbox.h"
#include "ccu_mva.h"
#include "ccu_platform_def.h"
#include "kd_camera_feature.h"/*for sensorType in ccu_set_sensor_info*/
#include "ccu_ipc.h"

static uint64_t camsys_base;
static uint64_t bin_base;
static uint64_t dmem_base;
static uint64_t pmem_base;

static struct ccu_device_s *ccu_dev;
static struct task_struct *enque_task;

static struct mutex cmd_mutex;
static bool cmd_done;

struct ccu_mailbox_t *pMailBox[MAX_MAILBOX_NUM];
static struct ccu_msg receivedCcuCmd;
static struct ccu_msg CcuAckCmd;

/*isr work management*/
struct ap_task_manage_t {
	struct workqueue_struct *ApTaskWorkQueue;
	struct mutex ApTaskMutex;
	struct list_head ApTskWorkList;
};

struct ap_task_manage_t ap_task_manage;


static struct CCU_INFO_STRUCT ccuInfo;
static bool bWaitCond;
static bool AFbWaitCond[IMGSENSOR_SENSOR_IDX_MAX_NUM];
static unsigned int g_LogBufIdx = 1;
static unsigned int AFg_LogBufIdx[IMGSENSOR_SENSOR_IDX_MAX_NUM] = {1};

static int _ccu_powerdown(bool need_check_ccu_stat);
static int ccu_irq_enable(void);
static int ccu_irq_disable(void);
static int ccu_load_segments(const struct firmware *fw,
	enum CCU_BIN_TYPE type);
static void *ccu_da_to_va(u64 da, int len);
static int ccu_sanity_check(const struct firmware *fw);
static inline unsigned int CCU_MsToJiffies(unsigned int Ms)
{
	return ((Ms * HZ + 512) >> 10);
}


static inline void lock_command(void)
{
	mutex_lock(&cmd_mutex);
	cmd_done = false;
}

static inline void unlock_command(void)
{
	mutex_unlock(&cmd_mutex);
}


static void isr_sp_task(void)
{
	LOG_DBG("%s\n", __func__);
}

#define CCU_ISR_WORK_BUFFER_SZIE 16


irqreturn_t ccu_isr_handler(int irq, void *dev_id)
{
	int n;
	enum mb_result mailboxRet;

	LOG_DBG("+++:%s\n", __func__);

	/*write clear mode*/
	LOG_DBG("write clear mode\n");
	ccu_write_reg(ccu_base, EINTC_CLR, 0xFF);
	LOG_DBG("read clear mode\n");
	ccu_read_reg(ccu_base, EINTC_ST);
	/**/

	isr_sp_task();

	while (1) {
		mailboxRet = mailbox_receive_cmd(&receivedCcuCmd);

		if (mailboxRet == MAILBOX_QUEUE_EMPTY) {
			LOG_DBG("MAIL_BOX IS EMPTY");
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
			("===== AP_ISR_CCU_ASSERT =====\n");
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
			("===== AP_ISR_CCU_WARNING =====\n");
			bWaitCond = true;
			g_LogBufIdx = -2;

			wake_up_interruptible(&ccuInfo.WaitQueueHead);
			LOG_ERR("wakeup ccuInfo.WaitQueueHead done\n");
			break;
		}
#ifdef CCU_AF_ENABLE
		case MSG_TO_APMCU_CAM_AFO_i:
		{
			LOG_DBG
			       ("AFWaitQueueHead:%d\n",
				receivedCcuCmd.in_data_ptr);
			if ((receivedCcuCmd.sensor_idx >=
				IMGSENSOR_SENSOR_IDX_MIN_NUM) &&
				(receivedCcuCmd.sensor_idx <
					IMGSENSOR_SENSOR_IDX_MAX_NUM)) {
				LOG_DBG("==== AFO_%d_done_from_CCU =====\n",
					receivedCcuCmd.sensor_idx);

				AFbWaitCond[receivedCcuCmd.sensor_idx] = true;
				AFg_LogBufIdx[receivedCcuCmd.sensor_idx] =
					receivedCcuCmd.sensor_idx;

				wake_up_interruptible(
					&ccuInfo.AFWaitQueueHead[
						receivedCcuCmd.sensor_idx]);

				LOG_DBG(
					"wakeup ccuInfo.AFWaitQueueHead done\n");
			} else if (receivedCcuCmd.sensor_idx ==
				IMGSENSOR_SENSOR_IDX_MAX_NUM) {
				for (n = 0;
					n < IMGSENSOR_SENSOR_IDX_MAX_NUM; n++) {

					AFbWaitCond[n] = true;
					AFg_LogBufIdx[n] =
						receivedCcuCmd.sensor_idx;
					wake_up_interruptible(
						&ccuInfo.AFWaitQueueHead[n]);
			LOG_DBG("wakeup ccuInfo.AFWaitQueueHead[%d] done\n", n);
					LOG_DBG_MUST("abort and wakeup\n");
				}
			} else {
				LOG_DBG_MUST(
					"unknown interrupt (%d)(MSG_TO_APMCU_CAM_AFO_i)\n",
					receivedCcuCmd.sensor_idx);
			}
			break;
		}

#endif /*CCU_AF_ENABLE*/

		default:
			LOG_DBG("got msgId: %d, cmd_wait\n",
				receivedCcuCmd.msg_id);
			ccu_memcpy(&CcuAckCmd, &receivedCcuCmd,
				sizeof(struct ccu_msg));
			cmd_done = true;
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

#ifdef CONFIG_MTK_CHIP
	init_check_sw_ver();
#endif

	/* init mutex */
	mutex_init(&cmd_mutex);
	/* init waitqueue */
	init_waitqueue_head(&ccuInfo.WaitQueueHead);
	for (n = 0; n < IMGSENSOR_SENSOR_IDX_MAX_NUM; n++)
		init_waitqueue_head(&ccuInfo.AFWaitQueueHead[n]);

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


	if (request_irq(device->irq_num,
		ccu_isr_handler, IRQF_TRIGGER_NONE, "ccu", NULL)) {
		LOG_ERR("fail to request ccu irq!\n");
		ret = -ENODEV;
		goto out;
	}

out:
	return ret;
}

int ccu_uninit_hw(struct ccu_device_s *device)
{
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

		#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
		(CONFIG_MTK_IOMMU_PGTABLE_EXT == 34)
		ccu_write_reg_bit(ccu_base, CTRL, H2X_MSB, 1);
		#endif

		/*use user space buffer*/
		ccu_write_reg(ccu_base, SPREG_02_LOG_DRAM_ADDR1,
			power->workBuf.mva_log[0]);
		ccu_write_reg(ccu_base, SPREG_03_LOG_DRAM_ADDR2,
			power->workBuf.mva_log[1]);

		LOG_DBG("LogBuf_mva[0](0x%x)(0x%x)\n",
			power->workBuf.mva_log[0],
			ccu_read_reg(ccu_base, SPREG_02_LOG_DRAM_ADDR1));
		LOG_DBG("LogBuf_mva[1](0x%x)(0x%x)\n",
			power->workBuf.mva_log[1],
			ccu_read_reg(ccu_base, SPREG_03_LOG_DRAM_ADDR2));

		ccuInfo.IsCcuPoweredOn = 1;

	} else if (power->bON == 0) {
		/*CCU Power off*/
		ccu_sw_hw_reset();
		if (ccuInfo.IsCcuPoweredOn == 1)
			ret = _ccu_powerdown(true);

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
		ccu_write_reg(ccu_base, SPREG_02_LOG_DRAM_ADDR1,
			power->workBuf.mva_log[0]);
		ccu_write_reg(ccu_base, SPREG_03_LOG_DRAM_ADDR2,
			power->workBuf.mva_log[1]);

		LOG_DBG("LogBuf_mva[0](0x%x)\n", power->workBuf.mva_log[0]);
		LOG_DBG("LogBuf_mva[1](0x%x)\n", power->workBuf.mva_log[1]);
	} else if (power->bON == 3) {
		/*Pause CCU, but don't pullup CG*/

		/*Check CCU halt status*/
		while ((ccu_read_reg(ccu_base, SPREG_08_CCU_INIT_CHECK)
			!= CCU_STATUS_INIT_DONE_2)
			&& (timeout >= 0)) {
			mdelay(1);
			LOG_DBG("wait ccu halt done\n");
			LOG_DBG("ccu halt stat: %x\n",
			ccu_read_reg_bit(ccu_base, CCU_ST, CCU_SYS_HALT));
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
			ccu_irq_disable();
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
	struct clk *cam_clk;

	if (ccuInfo.IsCcuPoweredOn == 1) {
		LOG_WARN("CCU kernel released on CCU running, shutdown\n");

		cam_clk = __clk_lookup("PG_CAM"); /* check cam power */

		LOG_WARN("current cam power is [%-17s: %3d]\n",
			__clk_get_name(cam_clk),
			__clk_get_enable_count(cam_clk));

		if (__clk_get_enable_count(cam_clk) > 0) {
			/*Set special isr task to MSG_TO_CCU_SHUTDOWN*/
			ccu_write_reg(ccu_base,
				SPREG_09_FORCE_PWR_DOWN, MSG_TO_CCU_SHUTDOWN);
			/*Interrupt to CCU*/
			ccu_write_reg_bit(ccu_base,
				EXT2CCU_INT_CCU, EXT2CCU_INT_CCU, 1);
			ret = _ccu_powerdown(true);
		} else {
			/*do not touch CCU HW regs & mems*/
			/* if cam power is already down*/
			ret = _ccu_powerdown(false);
		}

		if (ret < 0)
			return ret;

		mdelay(60);
		LOG_WARN("CCU force shutdown success\n");
	}

	return 0;
}

static int _ccu_powerdown(bool need_check_ccu_stat)
{
	int32_t timeout = 10;
	int32_t ccu_halt = 0;
	int32_t ccu_sleep = 0;

	if (!need_check_ccu_stat)
		goto  CCU_PWDN_SKIP_STAT_CHK;

	if (ccu_read_reg_bit(ccu_base, RESET, CCU_HW_RST) == 1) {
		LOG_INF_MUST("ccu reset is up, skip halt check\n");
	} else {
		while (timeout > 0 && !(ccu_halt | ccu_sleep)) {
			udelay(100);
			ccu_halt =
			 ccu_read_reg_bit(ccu_base, CCU_ST, CCU_SYS_HALT);
			ccu_sleep =
			 ccu_read_reg_bit(ccu_base, CCU_ST, CCU_SYS_SLEEP);
			LOG_DBG("wait ccu shutdown done\n");
			LOG_DBG("ccu shutdown stat: %x\n",
			ccu_halt,
			ccu_sleep);
			timeout = timeout - 1;
		}

		if (timeout <= 0) {
			LOG_ERR("%s timeout(%d)(%x)(%x)\n", __func__,
			ccu_read_reg(ccu_base, SPREG_09_FORCE_PWR_DOWN),
			ccu_halt,
			ccu_sleep);

			ccu_write_reg_bit(ccu_base, RESET, CCU_HW_RST, 1);
			/*Even timed-out, clock disable is still necessary,*/
			/*DO NOT return here.*/
		} else {
			LOG_INF_MUST("shutdown success(%x)(%x)(%x)\n",
			ccu_read_reg(ccu_base, SPREG_09_FORCE_PWR_DOWN),
			ccu_halt,
			ccu_sleep);
		}
	}
	/*Set CCU_A_RESET. CCU_HW_RST=1*/

CCU_PWDN_SKIP_STAT_CHK:
	udelay(100);
	/*CCF & i2c uninit*/
	ccu_irq_disable();
	ccu_clock_disable();
	ccuInfo.IsCcuPoweredOn = 0;

	return 0;
}

int ccu_run(struct ccu_run_s *info)
{
	int32_t timeout = 100000;
	struct ccu_mailbox_t *ccuMbPtr = NULL;
	struct ccu_mailbox_t *apMbPtr = NULL;
	uint32_t status;
	uint32_t mmu_enable_reg;
	uint32_t ccu_H2X_MSB;
	struct CcuMemInfo *bin_mem = ccu_get_binary_memory();
	MUINT32 remapOffset;
	struct shared_buf_map *sb_map_ptr = (struct shared_buf_map *)
		(dmem_base + OFFSET_CCU_SHARED_BUF_MAP_BASE);

	LOG_DBG("+:%s\n", __func__);
	if (bin_mem == NULL) {
		LOG_ERR("CCU RUN failed, bin_mem NULL\n");
		return -EINVAL;
	}
	remapOffset = bin_mem->mva - CCU_CACHE_BASE;
	ccu_irq_enable();
	ccu_H2X_MSB = ccu_read_reg_bit(ccu_base, CTRL, H2X_MSB);
	ccu_write_reg(ccu_base, AXI_REMAP, remapOffset);
	LOG_INF_MUST("set CCU remap offset: %x\n", remapOffset);
	ccu_write_reg(ccu_base, SPREG_04_LOG_LEVEL, info->log_level);
	ccu_write_reg(ccu_base, SPREG_05_LOG_TAGLEVEL, info->log_taglevel);
	ccu_write_reg(ccu_base, SPREG_06_CPUREF_BUF_ADDR,
	(info->CpuRefBufMva - remapOffset) | (info->CpuRefBufSz & 0xFF));
	ccu_write_reg(ccu_base, SPREG_21_CTRL_BUF_ADDR, info->CtrlBufMva);
	LOG_INF_MUST("set CCU CtrlBufMva: %x\n", info->CtrlBufMva);
	LOG_INF_MUST("CPU Ref Buf MVA %x(%x-%x), sz %dMB\n",
	info->CpuRefBufMva, info->CpuRefBufMva, remapOffset, info->CpuRefBufSz);

	sb_map_ptr->bkdata_ddr_buf_mva = info->bkdata_ddr_buf_mva;

	if (ccu_H2X_MSB) {
		ccu_config_m4u_port();
		LOG_INF_MUST("CCU 34bits support: %x\n", ccu_H2X_MSB);
	} else
		LOG_INF_MUST("CCU 32bits support: %x\n", ccu_H2X_MSB);

	mmu_enable_reg = ccu_read_reg(ccu_base, H2X_CFG);
	ccu_write_reg(ccu_base, H2X_CFG, (mmu_enable_reg | MMU_ENABLE_BIT));

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
	while ((ccu_read_reg(ccu_base, SPREG_08_CCU_INIT_CHECK)
		!= CCU_STATUS_INIT_DONE) && (timeout >= 0)) {
		usleep_range(50, 100);
		LOG_DBG("wait ccu initial done\n");
		LOG_DBG("ccu initial stat: %x\n",
			ccu_read_reg(ccu_base, SPREG_08_CCU_INIT_CHECK));
		timeout = timeout - 1;
	}

	if (timeout <= 0) {
		LOG_ERR("CCU init timeout\n");
		LOG_ERR("ccu initial debug info: %x\n",
			ccu_read_reg(ccu_base, SPREG_08_CCU_INIT_CHECK));
		return -ETIMEDOUT;
	}

	LOG_DBG_MUST("ccu initial done\n");
	LOG_DBG_MUST("ccu initial stat: %x\n",
		ccu_read_reg(ccu_base, SPREG_08_CCU_INIT_CHECK));
	LOG_DBG_MUST("ccu initial debug mb_ap2ccu: %x\n",
		ccu_read_reg(ccu_base, SPREG_00_MB_CCU2AP));
	LOG_DBG_MUST("ccu initial debug mb_ccu2ap: %x\n",
		ccu_read_reg(ccu_base, SPREG_01_MB_AP2CCU));

	/*
	 * 20160930
	 * Due to AHB2GMC HW Bug, mailbox use SRAM
	 * Driver wait CCU main initialize done and
	 * query INFO00 & INFO01 as mailbox address
	 */
	pMailBox[MAILBOX_SEND] =
		(struct ccu_mailbox_t *)(uintptr_t)(dmem_base +
			ccu_read_reg(ccu_base, SPREG_01_MB_AP2CCU));
	pMailBox[MAILBOX_GET] =
		(struct ccu_mailbox_t *)(uintptr_t)(dmem_base +
			ccu_read_reg(ccu_base, SPREG_00_MB_CCU2AP));


	ccuMbPtr = (struct ccu_mailbox_t *) pMailBox[MAILBOX_SEND];
	apMbPtr = (struct ccu_mailbox_t *) pMailBox[MAILBOX_GET];

	mailbox_init(apMbPtr, ccuMbPtr);

	/*tell ccu that driver has initialized mailbox*/
	ccu_write_reg(ccu_base, SPREG_08_CCU_INIT_CHECK, 0);

	timeout = 100000;
	while ((ccu_read_reg(ccu_base, SPREG_08_CCU_INIT_CHECK)
		!= CCU_STATUS_INIT_DONE_2) && (timeout >= 0)) {
		udelay(100);
		LOG_DBG_MUST("wait ccu log test\n");
		timeout = timeout - 1;
	}

	if (timeout <= 0) {
		LOG_ERR("CCU init timeout 2\n");
		LOG_ERR("ccu initial debug info: %x\n",
			ccu_read_reg(ccu_base, SPREG_08_CCU_INIT_CHECK));
		return -ETIMEDOUT;
	}

	LOG_DBG_MUST("ccu log test done\n");
	LOG_DBG_MUST("ccu log test stat: %x\n",
			ccu_read_reg(ccu_base, SPREG_08_CCU_INIT_CHECK));

	ccu_ipc_init((uint32_t *)dmem_base, (uint32_t *)ccu_base);

	LOG_DBG_MUST("-:%s(0218)\n", __func__);

	return 0;
}


int ccu_waitirq(struct CCU_WAIT_IRQ_STRUCT *WaitIrq)
{
	signed int ret = 0, Timeout = WaitIrq->EventInfo.Timeout;

	LOG_DBG("Clear(%d),bWaitCond(%d),Timeout(%d)(%d)\n",
		WaitIrq->EventInfo.Clear, bWaitCond, Timeout, ccu_dev->irq_num);
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
		/*LOG_DBG("ccuInfo.taskCount is not zero: %d\n",*/
		/* task_count_temp);*/
		/*}*/
		bWaitCond = false;
		LOG_DBG("-:ccu wait_event_interruptible\n");
	}

	if (Timeout > 0) {
		LOG_DBG("remain timeout:%d, task: %d\n", Timeout, g_LogBufIdx);
		/*send to user if not timeout*/
		WaitIrq->EventInfo.TimeInfo.passedbySigcnt = (int)g_LogBufIdx;
	}
	/*EXIT:*/

	return ret;
}

int ccu_AFwaitirq(struct CCU_WAIT_IRQ_STRUCT *WaitIrq, int sensoridx)
{
	signed int ret = 0, Timeout = WaitIrq->EventInfo.Timeout;

	LOG_DBG("Clear(%d),AFbWaitCond(%d),Timeout(%d)\n",
		WaitIrq->EventInfo.Clear,
		AFbWaitCond[sensoridx], Timeout);
	LOG_DBG("arg is struct CCU_WAIT_IRQ_STRUCT, size:%zu\n",
		sizeof(struct CCU_WAIT_IRQ_STRUCT));

	if (Timeout != 0) {
		/* 2. start to wait signal */
		LOG_DBG("+:wait_event_interruptible_timeout\n");
		AFbWaitCond[sensoridx] = false;
		Timeout = wait_event_interruptible_timeout(
				ccuInfo.AFWaitQueueHead[sensoridx],
				AFbWaitCond[sensoridx],
				CCU_MsToJiffies(WaitIrq->EventInfo.Timeout));

		LOG_DBG("-:wait_event_interruptible_timeout\n");
	} else {
		LOG_DBG("+:ccu wait_event_interruptible\n");

		mutex_unlock(&ap_task_manage.ApTaskMutex);
		LOG_DBG("unlock ApTaskMutex\n");
		wait_event_interruptible(
			ccuInfo.AFWaitQueueHead[sensoridx],
			AFbWaitCond[sensoridx]);
		LOG_DBG("accuiring ApTaskMutex\n");
		mutex_lock(&ap_task_manage.ApTaskMutex);
		LOG_DBG("got ApTaskMutex\n");

		AFbWaitCond[sensoridx] = false;
		LOG_DBG("-:ccu wait_event_interruptible\n");
	}

	if (Timeout > 0) {
		LOG_DBG("remain timeout:%d, task: %d\n",
			Timeout, AFg_LogBufIdx[sensoridx]);
		/*send to user if not timeout*/
		WaitIrq->EventInfo.TimeInfo.passedbySigcnt =
			(int)AFg_LogBufIdx[sensoridx];
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
	int *offset = (int *)(uintptr_t)(ccu_base + 0x80 + regNo * 4);

	LOG_DBG("%s: %x\n", __func__, (unsigned int)(*offset));

	return *offset;
}

void ccu_write_info_reg(int regNo, int val)
{
	int *offset = (int *)(uintptr_t)(ccu_base + 0x80 + regNo * 4);
	*offset = val;
	LOG_DBG("%s: %x\n", __func__, (unsigned int)(*offset));
}

void ccu_read_struct_size(uint32_t *structSizes, uint32_t structCnt)
{
	int i;
	int offset = ccu_read_reg(ccu_base, SPREG_10_STRUCT_SIZE_CHECK);
	uint32_t *ptr = ccu_da_to_va(offset, structCnt*sizeof(uint32_t));
	if (ptr == NULL) {
		LOG_ERR("%s: ptr null\n", __func__);
		return;
	}
	for (i = 0; i < structCnt; i++)
		structSizes[i] = ptr[i];
	LOG_DBG("%s: %x\n", __func__, offset);
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
	uint32_t offset = ccu_read_reg(ccu_base, SPREG_07_LOG_SRAM_ADDR);
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

int ccu_query_power_status(void)
{
	return ccuInfo.IsCcuPoweredOn;
}

int ccu_irq_enable(void)
{
	int ret = 0;

	LOG_DBG_MUST("%s+.\n", __func__);

	ccu_write_reg(ccu_base, EINTC_CLR, 0xFF);
	ccu_read_reg(ccu_base, EINTC_ST);
	if (request_irq(ccu_dev->irq_num, ccu_isr_handler,
		IRQF_TRIGGER_NONE, "ccu", NULL)) {
		LOG_ERR("fail to request ccu irq!\n");
		ret = -ENODEV;
	}

	return 0;
}

int ccu_irq_disable(void)
{
	LOG_DBG_MUST("%s+.\n", __func__);

	free_irq(ccu_dev->irq_num, NULL);
	ccu_write_reg(ccu_base, EINTC_CLR, 0xFF);
	ccu_read_reg(ccu_base, EINTC_ST);

	return 0;
}

int ccu_load_bin(struct ccu_device_s *device, enum CCU_BIN_TYPE type)
{
	const struct firmware *firmware_p;
	int ret = 0;

	ret = request_firmware(&firmware_p, "lib3a.ccu", device->dev);
	if (ret < 0) {
		LOG_ERR("request_firmware failed: %d\n", ret);
		goto EXIT;
	}

	ret = ccu_sanity_check(firmware_p);
	if (ret < 0) {
		LOG_ERR("sanity check failed: %d\n", ret);
		goto EXIT;
	}

	ret = ccu_load_segments(firmware_p, type);
	if (ret < 0)
		LOG_ERR("load segments failed: %d\n", ret);
EXIT:
	release_firmware(firmware_p);
	return ret;
}

int ccu_sanity_check(const struct firmware *fw)
{
	// const char *name = rproc->firmware;
	struct elf32_hdr *ehdr;
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

	if (ehdr->e_phnum == 0) {
		LOG_ERR("No loadable segments\n");
		return -EINVAL;
	}

	if (ehdr->e_phoff > fw->size) {
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
		ccu_write_reg(ccu_base, RESET, 0xFF3FFCFF);
		ccu_write_reg(ccu_base, RESET, 0x00010000);
		ccu_write_reg(ccu_base, RESET, 0x0);
		udelay(10);
		ccu_write_reg(ccu_base, RESET, 0x00010000);
		udelay(10);
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

		switch (type) {
		case CCU_DP_BIN:
		{
			if (da < CCU_CORE_DMEM_BASE && da > CCU_CACHE_BASE)
				continue;
			break;
		}
		case CCU_DDR_BIN:
		{
			if (da >= CCU_CORE_DMEM_BASE || da < CCU_CACHE_BASE)
				continue;
			break;
		}
		default:
		{
			LOG_ERR("binary type error %d\n",
				type);
			return -EFAULT;
		}

		}
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

void *ccu_da_to_va(u64 da, int len)
{
	int offset;
	struct CcuMemInfo *bin_mem = ccu_get_binary_memory();

	if (bin_mem == NULL) {
		LOG_ERR("failed lookup da(%x), bin_mem NULL", da);
		return NULL;
	}
	if (da < CCU_CACHE_BASE) {
		offset = da;
		if ((offset >= 0) && ((offset + len) < CCU_PMEM_SIZE)) {
			LOG_INF_MUST("da(0x%lx) to va(0x%lx)",
				da, pmem_base + offset);
			return (uint32_t *)(pmem_base + offset);
		}
	} else if (da >= CCU_CORE_DMEM_BASE) {
		offset = da - CCU_CORE_DMEM_BASE;
		if ((offset >= 0) && ((offset + len) < CCU_DMEM_SIZE)) {
			LOG_INF_MUST("da(0x%lx) to va(0x%lx)",
				da, dmem_base + offset);
			return (uint32_t *)(dmem_base + offset);
		}
	} else {
		offset = da - CCU_CACHE_BASE;
		if ((offset >= 0) &&
		((offset + len) < CCU_CTRL_BUF_TOTAL_SIZE)) {
			LOG_INF_MUST("da(0x%lx) to va(0x%lx)",
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
