// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include "cmdq_driver.h"
#include "cmdq_struct.h"
#include "cmdq_core.h"
#include "cmdq_virtual.h"
#include "cmdq_reg.h"
#include "cmdq_mdp_common.h"
#include "cmdq_device.h"
#include "cmdq_sec.h"
#include "mdp_ioctl_ex.h"
#include "mdp_def_ex.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/of_device.h>
#ifdef CMDQ_USE_LEGACY
#include <mt-plat/mtk_boot.h>
#endif
#ifndef CMDQ_OF_SUPPORT
/* mt_irq.h is not available on device tree enabled platforms */
#include <mach/mt_irq.h>
#endif

#ifdef CMDQ_OF_SUPPORT
/**
 * @device tree porting note
 * alps/kernel-3.10/arch/arm64/boot/dts/{platform}.dts
 *  - use of_device_id to match driver and device
 *  - use io_map to map and get VA of HW's rgister
 **/
static const struct of_device_id cmdq_of_ids[] = {
	{.compatible = "mediatek,gce",},
	{.compatible = "mediatek,mt8167-gce",},
	{}
};
#endif

static dev_t gCmdqDevNo;
static struct cdev *gCmdqCDev;
static struct class *gCMDQClass;

void cmdq_driver_dump_readback(u32 *addrs, u32 count, u32 *values)
{}

static DEVICE_ATTR_RO(status);
static DEVICE_ATTR_RO(error);
static DEVICE_ATTR_RO(record);
static DEVICE_ATTR_RW(log_level);
static DEVICE_ATTR_RW(profile_enable);

static int cmdq_proc_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdqCorePrintStatusSeq, inode->i_private);
}

static int cmdq_proc_error_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdqCorePrintErrorSeq, inode->i_private);
}

static int cmdq_proc_record_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdqCorePrintRecordSeq, inode->i_private);
}

static const struct file_operations cmdqDebugStatusOp = {
	.owner = THIS_MODULE,
	.open = cmdq_proc_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations cmdqDebugErrorOp = {
	.owner = THIS_MODULE,
	.open = cmdq_proc_error_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations cmdqDebugRecordOp = {
	.owner = THIS_MODULE,
	.open = cmdq_proc_record_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#ifdef CMDQ_INSTRUCTION_COUNT
static DEVICE_ATTR_RW(instruction_count_level);

static int cmdq_proc_instruction_count_open(struct inode *inode,
	struct file *file)
{
	return single_open(file, cmdqCorePrintInstructionCountSeq,
		inode->i_private);
}

static const struct file_operations cmdqDebugInstructionCountOp = {
	.owner = THIS_MODULE,
	.open = cmdq_proc_instruction_count_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

static int cmdq_open(struct inode *pInode, struct file *pFile)
{
	struct cmdqFileNodeStruct *pNode;

	CMDQ_VERBOSE("CMDQ driver open fd=%p begin\n", pFile);

	pFile->private_data = kzalloc(sizeof(struct cmdqFileNodeStruct),
		GFP_KERNEL);
	if (pFile->private_data == NULL) {
		CMDQ_ERR("Can't allocate memory for CMDQ file node\n");
		return -ENOMEM;
	}

	pNode = (struct cmdqFileNodeStruct *) pFile->private_data;
	pNode->userPID = current->pid;
	pNode->userTGID = current->tgid;

	INIT_LIST_HEAD(&(pNode->taskList));
	spin_lock_init(&pNode->nodeLock);

	CMDQ_VERBOSE("CMDQ driver open end\n");

	return 0;
}


static int cmdq_release(struct inode *pInode, struct file *pFile)
{
	struct cmdqFileNodeStruct *pNode;
	unsigned long flags;

	CMDQ_VERBOSE("CMDQ driver release fd=%p begin\n", pFile);

	pNode = (struct cmdqFileNodeStruct *) pFile->private_data;

	if (pNode == NULL) {
		CMDQ_ERR("CMDQ file node NULL\n");
		return -EFAULT;
	}

	spin_lock_irqsave(&pNode->nodeLock, flags);

	/* note that we did not release CMDQ tasks */
	/* issued by this file node, */
	/* since their HW operation may be pending. */

	spin_unlock_irqrestore(&pNode->nodeLock, flags);

	/* scan through tasks that created by this file node and release them */
	cmdq_core_release_task_by_file_node((void *)pNode);

	if (pFile->private_data != NULL) {
		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}

	CMDQ_VERBOSE("CMDQ driver release end\n");

	return 0;
}

static int cmdq_driver_create_reg_address_buffer(
	struct cmdqCommandStruct *pCommand)
{
	int status = 0;
	uint32_t totalRegCount = 0;
	uint32_t *regAddrBuf = NULL;

	uint32_t *kernelRegAddr = NULL;
	uint32_t kernelRegCount = 0;

	const uint32_t userRegCount = pCommand->regRequest.count;


	if (pCommand->debugRegDump != 0) {
		/* get kernel dump request count */
		status =
		    cmdqCoreDebugRegDumpBegin(pCommand->debugRegDump,
			&kernelRegCount,
			&kernelRegAddr);
		if (status != 0) {
			CMDQ_ERR
			("cmdqCoreDebugRegDumpBegin returns %d\n",
				status);
			kernelRegCount = 0;
			kernelRegAddr = NULL;
		}
	}

	/* how many register to dump? */
	if (kernelRegCount > CMDQ_MAX_DUMP_REG_COUNT ||
		userRegCount > CMDQ_MAX_DUMP_REG_COUNT)
		return -EINVAL;
	totalRegCount = kernelRegCount + userRegCount;

	if (totalRegCount == 0) {
		/* no need to dump register */
		pCommand->regRequest.count = 0;
		pCommand->regValue.count = 0;
		pCommand->regRequest.regAddresses =
			(cmdqU32Ptr_t) (unsigned long)NULL;
		pCommand->regValue.regValues =
			(cmdqU32Ptr_t) (unsigned long)NULL;
	} else {
		regAddrBuf = kcalloc(totalRegCount, sizeof(uint32_t),
			GFP_KERNEL);
		if (regAddrBuf == NULL)
			return -ENOMEM;

		/* collect user space dump request */
		if (userRegCount) {
			if (copy_from_user(regAddrBuf,
			    CMDQ_U32_PTR(pCommand->regRequest.regAddresses),
			     userRegCount * sizeof(uint32_t))) {
				kfree(regAddrBuf);
				return -EFAULT;
			}
		}

		/* collect kernel space dump request, */
		/* concatnate after user space request */
		if (kernelRegCount) {
			memcpy(regAddrBuf + userRegCount, kernelRegAddr,
			       kernelRegCount * sizeof(uint32_t));
		}


		/* replace address buffer and value */
		/* address buffer with kzalloc memory */
		pCommand->regRequest.regAddresses =
			(cmdqU32Ptr_t) (unsigned long)(regAddrBuf);
		pCommand->regRequest.count = totalRegCount;
	}

	return 0;
}

static void cmdq_driver_process_read_address_request(
	struct cmdqReadAddressStruct *req_user)
{
	/* create kernel-space buffer for working */
	uint32_t *addrs = NULL;
	uint32_t *values = NULL;
	dma_addr_t pa = 0;
	int i = 0;

	CMDQ_MSG("[READ_PA] %s()\n", __func__);

	do {
		if (req_user == NULL ||
		    req_user->count == 0 ||
		    req_user->count > CMDQ_MAX_DUMP_REG_COUNT ||
		    CMDQ_U32_PTR(req_user->values) == NULL ||
		    CMDQ_U32_PTR(req_user->dmaAddresses) == NULL) {
			CMDQ_ERR("[READ_PA] invalid req_user\n");
			break;
		}

		addrs = kcalloc(req_user->count, sizeof(uint32_t), GFP_KERNEL);
		if (addrs == NULL) {
			CMDQ_ERR("[READ_PA] fail to alloc addr buf\n");
			break;
		}

		values = kcalloc(req_user->count, sizeof(uint32_t), GFP_KERNEL);
		if (values == NULL) {
			CMDQ_ERR("[READ_PA] fail to alloc value buf\n");
			break;
		}

		/* copy from user */
		if (copy_from_user
		    (addrs, CMDQ_U32_PTR(req_user->dmaAddresses),
		     req_user->count * sizeof(uint32_t))) {
			CMDQ_ERR("[READ_PA] fail to copy user dmaAddresses\n");
			break;
		}

		/* actually read these PA write buffers */
		for (i = 0; i < req_user->count; ++i) {
			pa = (0xFFFFFFFF & addrs[i]);
			CMDQ_MSG("[READ_PA] req read dma address 0x%pa\n", &pa);
			values[i] = cmdqCoreReadWriteAddress(pa);
		}

		/* copy value to user */
		if (copy_to_user
		    (CMDQ_U32_PTR(req_user->values), values,
			req_user->count * sizeof(uint32_t))) {
			CMDQ_ERR("[READ_PA] fail to copy to user value buf\n");
			break;
		}

	} while (0);


	kfree(addrs);
	addrs = NULL;

	kfree(values);
	values = NULL;

}

static long cmdq_driver_destroy_secure_medadata(
	struct cmdqCommandStruct *pCommand)
{
	if (pCommand->secData.addrMetadatas) {
		kfree(CMDQ_U32_PTR(pCommand->secData.addrMetadatas));
		pCommand->secData.addrMetadatas =
			(cmdqU32Ptr_t) (unsigned long)NULL;
	}

	return 0;
}

static long cmdq_driver_create_secure_medadata(
	struct cmdqCommandStruct *pCommand)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	void *pAddrMetadatas = NULL;
	u32 length;

	if (pCommand->secData.addrMetadataCount >=
		CMDQ_IWC_MAX_ADDR_LIST_LENGTH) {
		CMDQ_ERR("Metadata %u reach the max allowed number = %u\n",
			 pCommand->secData.addrMetadataCount,
			 CMDQ_IWC_MAX_ADDR_LIST_LENGTH);
		return -EFAULT;
	}

	length = pCommand->secData.addrMetadataCount *
			sizeof(struct cmdqSecAddrMetadataStruct);

	/* verify parameter */
	if ((pCommand->secData.is_secure == false) &&
		(pCommand->secData.addrMetadataCount != 0)) {

		/* normal path with non-zero secure metadata */
		CMDQ_ERR
		    ("[secData]mismatch is_secure(%d) and count(%d)\n",
		     pCommand->secData.is_secure,
		     pCommand->secData.addrMetadataCount);
		return -EFAULT;
	}

	/* revise max count field */
	pCommand->secData.addrMetadataMaxCount =
		pCommand->secData.addrMetadataCount;

	/* bypass 0 metadata case */
	if (pCommand->secData.addrMetadataCount == 0) {
		pCommand->secData.addrMetadatas =
			(cmdqU32Ptr_t) (unsigned long)NULL;
		return 0;
	}

	/* create kernel-space buffer for working */
	pAddrMetadatas = kzalloc(length, GFP_KERNEL);
	if (pAddrMetadatas == NULL) {
		CMDQ_ERR("[secData]kzalloc failed, count:%d, size:%d\n",
			 pCommand->secData.addrMetadataCount, length);
		return -ENOMEM;
	}

	/* copy from user */
	if (copy_from_user(pAddrMetadatas,
		CMDQ_U32_PTR(pCommand->secData.addrMetadatas), length)) {

		CMDQ_ERR("[secData]fail to copy user addrMetadatas\n");

		/* replace buffer first to ensure that */
		/* addrMetadatas is valid kernel space */
		/* buffer address when free it */
		pCommand->secData.addrMetadatas =
			(cmdqU32Ptr_t) (unsigned long)pAddrMetadatas;
		/* free secure path metadata */
		cmdq_driver_destroy_secure_medadata(pCommand);
		return -EFAULT;
	}

	/* replace buffer */
	pCommand->secData.addrMetadatas =
		(cmdqU32Ptr_t) (unsigned long)pAddrMetadatas;

#endif
	return 0;
}

long cmdq_driver_process_command_request(
	struct cmdqCommandStruct *pCommand)
{
	int32_t status = 0;
	uint32_t *userRegValue = NULL;
	uint32_t userRegCount = 0;

	if (pCommand->regRequest.count != pCommand->regValue.count) {
		CMDQ_ERR("mismatch regRequest and regValue\n");
		return -EFAULT;
	}

	/* avoid copy large string */
	if (pCommand->userDebugStrLen > CMDQ_MAX_DBG_STR_LEN)
		pCommand->userDebugStrLen = CMDQ_MAX_DBG_STR_LEN;

	/* allocate secure medatata */
	status = cmdq_driver_create_secure_medadata(pCommand);
	if (status != 0)
		return status;

	/* backup since we are going to replace these */
	userRegValue = CMDQ_U32_PTR(pCommand->regValue.regValues);
	userRegCount = pCommand->regValue.count;

	/* create kernel-space address buffer */
	status = cmdq_driver_create_reg_address_buffer(pCommand);
	if (status != 0) {
		/* free secure path metadata */
		cmdq_driver_destroy_secure_medadata(pCommand);
		return status;
	}

	/* create kernel-space value buffer */
	pCommand->regValue.regValues = (cmdqU32Ptr_t) (unsigned long)
	    kzalloc(pCommand->regRequest.count * sizeof(uint32_t), GFP_KERNEL);
	pCommand->regValue.count = pCommand->regRequest.count;
	if (CMDQ_U32_PTR(pCommand->regValue.regValues) == NULL) {
		kfree(CMDQ_U32_PTR(pCommand->regRequest.regAddresses));
		return -ENOMEM;
	}

	/* scenario id fixup */
	cmdq_core_fix_command_scenario_for_user_space(pCommand);

	status = cmdqCoreSubmitTask(pCommand);
	if (status < 0) {
		CMDQ_ERR("Submit user commands for execution failed = %d\n",
			status);
		cmdq_driver_destroy_secure_medadata(pCommand);

		kfree(CMDQ_U32_PTR(pCommand->regRequest.regAddresses));
		kfree(CMDQ_U32_PTR(pCommand->regValue.regValues));
		return -EFAULT;
	}

	/* notify kernel space dump callback */
	if (pCommand->debugRegDump != 0) {
		status = cmdqCoreDebugRegDumpEnd(pCommand->debugRegDump,
			pCommand->regRequest.count - userRegCount,
			CMDQ_U32_PTR(pCommand->regValue.regValues) +
			userRegCount);
		if (status != 0) {
			/* Error status print */
			CMDQ_ERR("cmdqCoreDebugRegDumpEnd returns %d\n",
				status);
		}
	}

	/* copy back to user space buffer */
	if (userRegValue && userRegCount) {
		/* copy results back to user space */
		CMDQ_VERBOSE("regValue[0] is %d\n",
			CMDQ_U32_PTR(pCommand->regValue.regValues)[0]);
		if (copy_to_user
		    (userRegValue, CMDQ_U32_PTR(pCommand->regValue.regValues),
		     userRegCount * sizeof(uint32_t))) {
			CMDQ_ERR("Copy REGVALUE to user space failed\n");
		}
	}

	/* free allocated kernel buffers */
	kfree(CMDQ_U32_PTR(pCommand->regRequest.regAddresses));
	kfree(CMDQ_U32_PTR(pCommand->regValue.regValues));

	if (pCommand->readAddress.count > 0)
		cmdq_driver_process_read_address_request(
			&pCommand->readAddress);

	/* free allocated secure metadata */
	cmdq_driver_destroy_secure_medadata(pCommand);

	return 0;
}

bool cmdq_driver_support_wait_and_receive_event_in_same_tick(void)
{
#ifdef CMDQ_USE_LEGACY
	const unsigned int code = mt_get_chip_hw_code();
	enum chip_sw_ver ver = mt_get_chip_sw_ver();
	bool support = false;

	if (code == 0x6795) {
		support = true;
	} else if (ver >= CHIP_SW_VER_02) {
		/* SW V2 */
		support = true;
	} else if (ver >= CHIP_SW_VER_01) {
		support = false;
	}

	return support;
#else
	return true;
#endif
}

s32 cmdq_driver_ioctl_query_usage(struct file *pf, unsigned long param)
{
	int count[CMDQ_MAX_ENGINE_COUNT] = {0};

	if (cmdqCoreQueryUsage(count))
		return -EFAULT;

	if (copy_to_user((void *)param, count,
		sizeof(int32_t) * CMDQ_MAX_ENGINE_COUNT)) {
		CMDQ_ERR("CMDQ_IOCTL_QUERY_USAGE copy_to_user failed\n");
		return -EFAULT;
	}
	return 0;
}

s32 cmdq_driver_ioctl_query_cap_bits(unsigned long param)
{
	int capBits = 0;

	if (cmdq_driver_support_wait_and_receive_event_in_same_tick())
		capBits |= (1L << CMDQ_CAP_WFE);
	else
		capBits &= ~(1L << CMDQ_CAP_WFE);

	if (copy_to_user((void *)param, &capBits, sizeof(int))) {
		CMDQ_ERR("Copy capacity bits to user space failed\n");
		return -EFAULT;
	}
	return 0;
}

s32 cmdq_driver_ioctl_query_dts(unsigned long param)
{
	struct cmdqDTSDataStruct *pDtsData;

	pDtsData = cmdq_core_get_whole_DTS_Data();

	if (copy_to_user((void *)param, pDtsData,
		sizeof(struct cmdqDTSDataStruct))) {
		CMDQ_ERR("Copy device tree to user space failed\n");
		return -EFAULT;
	}
	return 0;
}

s32 cmdq_driver_ioctl_notify_engine(unsigned long param)
{
	uint64_t engineFlag;

	if (copy_from_user(&engineFlag, (void *)param, sizeof(uint64_t))) {
		CMDQ_ERR("CMDQ_IOCTL_NOTIFY_ENGINE copy_from_user failed\n");
		return -EFAULT;
	}
	cmdqCoreLockResource(engineFlag, true);
	return true;
}

long cmdq_ioctl(struct file *pFile, unsigned int code,
	unsigned long param)
{
	int32_t status;

	switch (code) {
	case CMDQ_IOCTL_QUERY_USAGE:
		status = cmdq_driver_ioctl_query_usage(pFile, param);
		break;
	case CMDQ_IOCTL_QUERY_CAP_BITS:
		status = cmdq_driver_ioctl_query_cap_bits(param);
		break;
	case CMDQ_IOCTL_QUERY_DTS:
		status = cmdq_driver_ioctl_query_dts(param);
		break;
	case CMDQ_IOCTL_NOTIFY_ENGINE:
		status = cmdq_driver_ioctl_notify_engine(param);
		break;
	case CMDQ_IOCTL_ASYNC_EXEC:
		CMDQ_MSG("ioctl CMDQ_IOCTL_ASYNC_EXEC\n");
		status = mdp_ioctl_async_exec(pFile, param);
		break;
	case CMDQ_IOCTL_ASYNC_WAIT:
		CMDQ_MSG("ioctl CMDQ_IOCTL_ASYNC_WAIT\n");
		status = mdp_ioctl_async_wait(param);
		break;
	case CMDQ_IOCTL_ALLOC_READBACK_SLOTS:
		CMDQ_MSG("ioctl CMDQ_IOCTL_ALLOC_READBACK_SLOTS\n");
		status = mdp_ioctl_alloc_readback_slots(pFile, param);
		break;
	case CMDQ_IOCTL_FREE_READBACK_SLOTS:
		CMDQ_MSG("ioctl CMDQ_IOCTL_FREE_READBACK_SLOTS\n");
		status = mdp_ioctl_free_readback_slots(pFile, param);
		break;
	case CMDQ_IOCTL_READ_READBACK_SLOTS:
		CMDQ_MSG("ioctl CMDQ_IOCTL_READ_READBACK_SLOTS\n");
		status = mdp_ioctl_read_readback_slots(param);
		break;
	default:
		CMDQ_ERR("unrecognized ioctl 0x%08x\n", code);
		return -ENOIOCTLCMD;
	}

	return 0;
}

#ifdef CONFIG_COMPAT
static long cmdq_ioctl_compat(struct file *pFile, unsigned int code,
	unsigned long param)
{
	switch (code) {
	case CMDQ_IOCTL_QUERY_USAGE:
	case CMDQ_IOCTL_EXEC_COMMAND:
	case CMDQ_IOCTL_ASYNC_JOB_EXEC:
	case CMDQ_IOCTL_ASYNC_JOB_WAIT_AND_CLOSE:
	case CMDQ_IOCTL_ALLOC_WRITE_ADDRESS:
	case CMDQ_IOCTL_FREE_WRITE_ADDRESS:
	case CMDQ_IOCTL_READ_ADDRESS_VALUE:
	case CMDQ_IOCTL_QUERY_CAP_BITS:
	case CMDQ_IOCTL_QUERY_DTS:
	case CMDQ_IOCTL_NOTIFY_ENGINE:
	case CMDQ_IOCTL_ASYNC_EXEC:
	case CMDQ_IOCTL_ASYNC_WAIT:
	case CMDQ_IOCTL_ALLOC_READBACK_SLOTS:
	case CMDQ_IOCTL_FREE_READBACK_SLOTS:
	case CMDQ_IOCTL_READ_READBACK_SLOTS:
		/* All ioctl structures should be */
		/* the same size in 32-bit and 64-bit linux. */
		return cmdq_ioctl(pFile, code, param);
	case CMDQ_IOCTL_LOCK_MUTEX:
	case CMDQ_IOCTL_UNLOCK_MUTEX:
		CMDQ_ERR("[COMPAT]deprecated ioctl 0x%08x\n", code);
		return -ENOIOCTLCMD;
	default:
		CMDQ_ERR("[COMPAT]unrecognized ioctl 0x%08x\n", code);
		return -ENOIOCTLCMD;
	}

	CMDQ_ERR("[COMPAT]unrecognized ioctl 0x%08x\n", code);
	return -ENOIOCTLCMD;
}
#endif


static const struct file_operations cmdqOP = {
	.owner = THIS_MODULE,
	.open = cmdq_open,
	.release = cmdq_release,
	.unlocked_ioctl = cmdq_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = cmdq_ioctl_compat,
#endif
};

static int cmdq_pm_notifier_cb(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:	/* Going to suspend the system */
		/* The next stage is freeze process. */
		/* We will queue all request in suspend callback, */
		/* so don't care this stage */
		return NOTIFY_DONE;	/* don't care this event */
	case PM_POST_SUSPEND:
		/* processes had resumed in previous */
		/*  stage (system resume callback) */
		/* resume CMDQ driver to execute. */
		cmdqCoreResumedNotifier();
		return NOTIFY_OK;	/* process done */
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_DONE;
}

/* Hibernation and suspend events */
static struct notifier_block cmdq_pm_notifier_block = {
	.notifier_call = cmdq_pm_notifier_cb,
	.priority = 5,
};

static irqreturn_t cmdq_irq_handler(int IRQ, void *pDevice)
{
	int index;
	uint32_t irqStatus;
	bool handled = false;	/* we share IRQ bit with CQ-DMA, */
	/* so it is possible that this handler */
	/* is called but GCE does not have IRQ flag. */
	do {
		if (cmdq_dev_get_irq_id() == IRQ) {
			if (!cmdq_core_is_clock_enabled()) {
				CMDQ_ERR("Got IRQ when clock is disabled\n");
				break;
			}
			irqStatus = CMDQ_REG_GET32(CMDQ_CURR_IRQ_STATUS) &
				0x0FFFF;
			for (index = 0; (irqStatus != 0xFFFF) &&
				index < CMDQ_MAX_THREAD_COUNT;
			     index++) {
				/* STATUS bit set to 0 means IRQ asserted */
				if (irqStatus & (1 << index))
					continue;

				/* so we mark irqStatus to 1 to */
				/* denote finished processing */
				/* and we can early-exit if no more */
				/* threads being asserted */
				irqStatus |= (1 << index);

				cmdqCoreHandleIRQ(index);
				handled = true;
			}
		} else if (cmdq_dev_get_irq_secure_id() == IRQ) {
			CMDQ_ERR("receive secure IRQ %d in NWD\n", IRQ);
		}
	} while (0);

	if (handled) {
		cmdq_core_add_consume_task();
		return IRQ_HANDLED;
	}
	/* allow CQ-DMA to process this IRQ bit */
	return IRQ_NONE;
}

static int cmdq_create_debug_entries(void)
{
	struct proc_dir_entry *debugDirEntry = NULL;

	debugDirEntry = proc_mkdir(CMDQ_DRIVER_DEVICE_NAME "_debug", NULL);
	if (debugDirEntry) {
		struct proc_dir_entry *entry = NULL;

		entry = proc_create("status", 0440, debugDirEntry,
			&cmdqDebugStatusOp);
		entry = proc_create("error", 0440, debugDirEntry,
			&cmdqDebugErrorOp);
		entry = proc_create("record", 0440, debugDirEntry,
			&cmdqDebugRecordOp);
#ifdef CMDQ_INSTRUCTION_COUNT
		entry =
		    proc_create("instructionCount", 0440, debugDirEntry,
				&cmdqDebugInstructionCountOp);
#endif
	}

	return 0;
}

static int cmdq_probe(struct platform_device *pDevice)
{
	int status;
	struct device *object;

	CMDQ_MSG("CMDQ driver probe begin\n");

	/* Function link */
	cmdq_virtual_function_setting();

	/* init cmdq device related data */
	cmdq_dev_init(pDevice);

	/* init cmdq context */
	cmdqCoreInitialize();

	status = alloc_chrdev_region(&gCmdqDevNo, 0, 1,
		CMDQ_DRIVER_DEVICE_NAME);
	if (status != 0) {
		/* Cannot get CMDQ device major number */
		CMDQ_ERR("Get CMDQ device major number(%d) failed(%d)\n",
			gCmdqDevNo, status);
	} else {
		/* Get CMDQ device major number successfully */
		CMDQ_MSG("Get CMDQ device major number(%d) success(%d)\n",
			gCmdqDevNo, status);
	}

	/* ioctl access point (/dev/mtk_cmdq) */
	gCmdqCDev = cdev_alloc();
	gCmdqCDev->owner = THIS_MODULE;
	gCmdqCDev->ops = &cmdqOP;

	status = cdev_add(gCmdqCDev, gCmdqDevNo, 1);

	gCMDQClass = class_create(THIS_MODULE, CMDQ_DRIVER_DEVICE_NAME);
	object = device_create(gCMDQClass, NULL, gCmdqDevNo, NULL,
			CMDQ_DRIVER_DEVICE_NAME);

	status =
	    request_irq(cmdq_dev_get_irq_id(), cmdq_irq_handler,
			IRQF_TRIGGER_LOW | IRQF_SHARED,
			CMDQ_DRIVER_DEVICE_NAME, gCmdqCDev);
	if (status != 0) {
		CMDQ_ERR("Register cmdq driver irq handler(%d) failed(%d)\n",
			gCmdqDevNo, status);
		return -EFAULT;
	}

	/* although secusre CMDQ driver is responsible for handle secure IRQ, */
	/* MUST registet secure IRQ to GIC in normal */
	/* world to ensure it will be initialize correctly */
	/* (that's because t-base does not */
	/* support GIC init IRQ in secure world...) */
#if defined(CMDQ_SECURE_PATH_SUPPORT) && defined(CMDQ_SECURE_PATH_NORMAL_IRQ)
	status = request_irq(cmdq_dev_get_irq_secure_id(),
		cmdq_irq_handler,
		IRQF_TRIGGER_LOW,
		CMDQ_DRIVER_DEVICE_NAME,
		gCmdqCDev);
	CMDQ_MSG("register sec IRQ:%d\n", cmdq_dev_get_irq_secure_id());
	if (status != 0) {
		CMDQ_ERR("Register secure irq handler(%d) failed(%d)\n",
			gCmdqDevNo,
			status);
		return -EFAULT;
	}
#endif

	/* proc debug access point */
	cmdq_create_debug_entries();

	/* device attributes for debugging */
	device_create_file(&pDevice->dev, &dev_attr_status);
	device_create_file(&pDevice->dev, &dev_attr_error);
	device_create_file(&pDevice->dev, &dev_attr_record);
	device_create_file(&pDevice->dev, &dev_attr_log_level);
	device_create_file(&pDevice->dev, &dev_attr_profile_enable);
#ifdef CMDQ_INSTRUCTION_COUNT
	device_create_file(&pDevice->dev, &dev_attr_instruction_count_level);
#endif

	mdp_limit_dev_create(pDevice);
	CMDQ_MSG("CMDQ driver probe end\n");

	return 0;
}


static int cmdq_remove(struct platform_device *pDevice)
{
	disable_irq(cmdq_dev_get_irq_id());

	device_remove_file(&pDevice->dev, &dev_attr_status);
	device_remove_file(&pDevice->dev, &dev_attr_error);
	device_remove_file(&pDevice->dev, &dev_attr_record);
	device_remove_file(&pDevice->dev, &dev_attr_log_level);
	device_remove_file(&pDevice->dev, &dev_attr_profile_enable);
#ifdef CMDQ_INSTRUCTION_COUNT
	device_remove_file(&pDevice->dev, &dev_attr_instruction_count_level);
#endif
	return 0;
}


static int cmdq_suspend(struct device *pDevice)
{
	return cmdqCoreSuspend();
}


static int cmdq_resume(struct device *pDevice)
{
	return cmdqCoreResume();
}

static int cmdq_pm_restore_noirq(struct device *pDevice)
{
	return 0;
}

static const struct dev_pm_ops cmdq_pm_ops = {
	.suspend = cmdq_suspend,
	.resume = cmdq_resume,
	.freeze = NULL,
	.thaw = NULL,
	.poweroff = NULL,
	.restore = NULL,
	.restore_noirq = cmdq_pm_restore_noirq,
};


static struct platform_driver gCmdqDriver = {
	.probe = cmdq_probe,
	.remove = cmdq_remove,
	.driver = {
		   .name = CMDQ_DRIVER_DEVICE_NAME,
		   .owner = THIS_MODULE,
		   .pm = &cmdq_pm_ops,
#ifdef CMDQ_OF_SUPPORT
		   .of_match_table = cmdq_of_ids,
#endif
	}
};

static int __init cmdq_init(void)
{
	int status;

	CMDQ_MSG("CMDQ driver init begin\n");

	/* Initialize group callback */
	cmdqCoreInitGroupCB();

	/* MDP function link */
	cmdq_mdp_virtual_function_setting();
	cmdq_mdp_platform_function_setting();

	/* Register MDP callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_MDP,
			   cmdq_mdp_get_func()->mdpClockOn,
			   cmdq_mdp_get_func()->mdpDumpInfo,
			   cmdq_mdp_get_func()->mdpResetEng,
			   cmdq_mdp_get_func()->mdpClockOff);

	/* Register module dispatch callback */
	cmdqCoreRegisterDispatchModCB(CMDQ_GROUP_MDP,
			   cmdq_mdp_get_func()->dispatchModule);

	/* Register restore task */
	cmdqCoreRegisterTrackTaskCB(CMDQ_GROUP_MDP,
			   cmdq_mdp_get_func()->trackTask);

	/* Register VENC callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_VENC, NULL,
		cmdq_mdp_get_func()->vEncDumpInfo, NULL, NULL);

	status = platform_driver_register(&gCmdqDriver);
	if (status != 0) {
		CMDQ_ERR("Failed to register the CMDQ driver(%d)\n", status);
		return -ENODEV;
	}

	/* register pm notifier */
	status = register_pm_notifier(&cmdq_pm_notifier_block);
	if (status != 0) {
		CMDQ_ERR("Failed to register_pm_notifier(%d)\n", status);
		return -ENODEV;
	}

	CMDQ_MSG("CMDQ driver init end\n");

	return 0;
}

static void __exit cmdq_exit(void)
{
	int32_t status;

	CMDQ_MSG("CMDQ driver exit begin\n");

	device_destroy(gCMDQClass, gCmdqDevNo);

	class_destroy(gCMDQClass);

	cdev_del(gCmdqCDev);

	gCmdqCDev = NULL;

	unregister_chrdev_region(gCmdqDevNo, 1);

	platform_driver_unregister(&gCmdqDriver);

	/* register pm notifier */
	status = unregister_pm_notifier(&cmdq_pm_notifier_block);
	if (status != 0) {
		/* Failed to unregister_pm_notifier */
		CMDQ_ERR("Failed to unregister_pm_notifier(%d)\n", status);
	}

	/* Unregister MDP callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_MDP, NULL, NULL, NULL, NULL);

	/* Unregister VENC callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_VENC, NULL, NULL, NULL, NULL);

	/* De-Initialize group callback */
	cmdqCoreDeinitGroupCB();

	/* De-Initialize cmdq core */
	cmdqCoreDeInitialize();

	/* De-Initialize cmdq dev related data */
	cmdq_dev_deinit();
	mdp_limit_dev_destroy();

	CMDQ_MSG("CMDQ driver exit end\n");
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
static int __init cmdq_init_allocate_WSM(void)
{
	int status;

	CMDQ_MSG("CMDQ driver late init begin\n");

	status = cmdqCoreLateInitialize();
	status = mdp_limit_late_init();

	CMDQ_MSG("CMDQ driver late init end\n");

	return 0;
}
late_initcall(cmdq_init_allocate_WSM);
#else
static int __init mdp_late_init(void)
{
	int status;

	CMDQ_LOG("%s begin\n", __func__);
	status = mdp_limit_late_init();
	CMDQ_LOG("%s end\n", __func__);

	return 0;
}
late_initcall(mdp_late_init);
#endif

subsys_initcall(cmdq_init);
module_exit(cmdq_exit);

MODULE_DESCRIPTION("MTK CMDQ driver");
MODULE_AUTHOR("Pablo<pablo.sun@mediatek.com>");
MODULE_LICENSE("GPL");
