/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include "cmdq_driver.h"
#include "cmdq_struct.h"
#include "cmdq_core.h"
#include "cmdq_mutex.h"
#include "cmdq_reg.h"
#include "cmdq_mdp.h"
#include "cmdq_device.h"
#include "cmdq_platform.h"
#ifdef CMDQ_SECURE_PATH_SUPPORT
#include "cmdq_sec.h"
#endif

/* #include "camera_isp.h" */
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
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
/* #include <mach/mt_irq.h> */

#ifdef CONFIG_HAS_SBSUSPEND
#include <linux/sbsuspend.h>
#endif


#ifdef CMDQ_OF_SUPPORT
/**
 * @device tree porting note
 * alps/kernel-3.10/arch/arm64/boot/dts/{platform}.dts
 *  - use of_device_id to match driver and device
 *  - use io_map to map and get VA of HW's rgister
**/
static const struct of_device_id cmdq_of_ids[] = {
	{.compatible = "mediatek,mt8173-gce",},
	{.compatible = "mediatek,mt8163-gce",},
	{}
};
#endif

#define CMDQ_MAX_DUMP_REG_COUNT (2048)

static dev_t gCmdqDevNo;
static struct cdev *gCmdqCDev;
static struct class *gCMDQClass;

static ssize_t cmdq_driver_dummy_write(struct device *dev,
				       struct device_attribute *attr, const char *buf, size_t size)
{
	return -EACCES;
}

static DEVICE_ATTR(status, S_IRUSR | S_IWUSR, cmdqCorePrintStatus, cmdq_driver_dummy_write);
static DEVICE_ATTR(error, S_IRUSR | S_IWUSR, cmdqCorePrintError, cmdq_driver_dummy_write);
static DEVICE_ATTR(record, S_IRUSR | S_IWUSR, cmdqCorePrintRecord, cmdq_driver_dummy_write);
static DEVICE_ATTR(log_level, S_IRUSR | S_IWUSR, cmdqCorePrintLogLevel, cmdqCoreWriteLogLevel);
static DEVICE_ATTR(profile_enable, S_IRUSR | S_IWUSR, cmdqCorePrintProfileEnable,
		   cmdqCoreWriteProfileEnable);


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

static const struct file_operations cmdqDebugLevelOp = {
	.owner = THIS_MODULE,
	.read = cmdq_read_debug_level_proc,
	.write = cmdq_write_debug_level_proc
};



static int cmdq_open(struct inode *pInode, struct file *pFile)
{
	struct cmdqFileNodeStruct *pNode;

	CMDQ_VERBOSE("CMDQ driver open fd=%p begin\n", pFile);

	pFile->private_data = kzalloc(sizeof(struct cmdqFileNodeStruct), GFP_KERNEL);
	if (NULL == pFile->private_data) {
		CMDQ_ERR("Can't allocate memory for CMDQ file node\n");
		return -ENOMEM;
	}

	pNode = (struct cmdqFileNodeStruct *)pFile->private_data;
	pNode->userPID = current->pid;
	pNode->userTGID = current->tgid;

	INIT_LIST_HEAD(&(pNode->taskList));
	pNode->mutexFlag = 0;
	spin_lock_init(&pNode->nodeLock);

	CMDQ_VERBOSE("CMDQ driver open end\n");

	return 0;
}


static int cmdq_release(struct inode *pInode, struct file *pFile)
{
	struct cmdqFileNodeStruct *pNode;
	unsigned long flags;
	int32_t index;

	CMDQ_VERBOSE("CMDQ driver release fd=%p begin\n", pFile);

	pNode = (struct cmdqFileNodeStruct *)pFile->private_data;

	if (NULL == pNode) {
		CMDQ_ERR("CMDQ file node NULL\n");
		return -EFAULT;
	}

	spin_lock_irqsave(&pNode->nodeLock, flags);

	if (pNode->mutexFlag) {
		for (index = DISP_MUTEX_MDP_FIRST;
		     index < DISP_MUTEX_MDP_FIRST + DISP_MUTEX_MDP_COUNT; index++) {
			if ((1 << index) & pNode->mutexFlag)
				cmdqMutexRelease(index);

		}

		pNode->mutexFlag = 0;
	}
	/* note that we did not release CMDQ tasks */
	/* issued by this file node, */
	/* since their HW operation may be pending. */

	spin_unlock_irqrestore(&pNode->nodeLock, flags);


	/* scan through tasks that created by this file node and release them */
	cmdq_core_release_task_by_file_node((void *)pNode);

	if (NULL != pFile->private_data) {
		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}

	CMDQ_VERBOSE("CMDQ driver release end\n");

	return 0;
}

static int cmdq_driver_create_reg_address_buffer(struct cmdqCommandStruct *pCommand)
{
	int status = 0;
	uint32_t totalRegCount = 0;
	uint32_t *regAddrBuf = NULL;

	uint32_t *kernelRegAddr = NULL;
	uint32_t kernelRegCount = 0;

	const uint32_t userRegCount = pCommand->regRequest.count;


	if (0 != pCommand->debugRegDump) {
		/* get kernel dump request count */
		status = cmdqCoreDebugRegDumpBegin(pCommand->debugRegDump,
						   &kernelRegCount, &kernelRegAddr);
		if (0 != status) {
			CMDQ_ERR
			    ("cmdqCoreDebugRegDumpBegin returns %d, ignore kernel reg dump request\n",
			     status);
			kernelRegCount = 0;
			kernelRegAddr = NULL;
		}
	}
	/* how many register to dump? */
	if (kernelRegCount > CMDQ_MAX_DUMP_REG_COUNT || userRegCount > CMDQ_MAX_DUMP_REG_COUNT)
		return -EINVAL;
	totalRegCount = kernelRegCount + userRegCount;

	if (0 == totalRegCount) {
		/* no need to dump register */
		pCommand->regRequest.count = 0;
		pCommand->regValue.count = 0;
		pCommand->regRequest.regAddresses = (cmdqU32Ptr_t) (unsigned long)NULL;
		pCommand->regValue.regValues = (cmdqU32Ptr_t) (unsigned long)NULL;
	} else {
		regAddrBuf = kcalloc(totalRegCount, sizeof(uint32_t), GFP_KERNEL);
		if (NULL == regAddrBuf)
			return -ENOMEM;

		/* collect user space dump request */
		if (userRegCount) {
			if (copy_from_user
			    (regAddrBuf, CMDQ_U32_PTR(pCommand->regRequest.regAddresses),
			     userRegCount * sizeof(uint32_t))) {
				kfree(regAddrBuf);
				return -EFAULT;
			}
		}
		/* collect kernel space dump request, concatnate after user space request */
		if (kernelRegCount) {
			memcpy(regAddrBuf + userRegCount, kernelRegAddr,
			       kernelRegCount * sizeof(uint32_t));
		}

		/* replace address buffer and value address buffer with kzalloc memory */
		pCommand->regRequest.regAddresses = (cmdqU32Ptr_t) (unsigned long)(regAddrBuf);
		pCommand->regRequest.count = totalRegCount;
	}

	return 0;
}

static void cmdq_driver_process_read_address_request(struct cmdqReadAddressStruct *req_user)
{
	/* create kernel-space buffer for working */
	uint32_t *addrs = NULL;
	uint32_t *values = NULL;
	dma_addr_t pa = 0;
	int i = 0;

	CMDQ_LOG("[READ_PA] cmdq_driver_process_read_address_request()\n");

	do {
		if (NULL == req_user ||
		    0 == req_user->count || NULL == CMDQ_U32_PTR(req_user->values)
		    || NULL == CMDQ_U32_PTR(req_user->dmaAddresses)) {
			CMDQ_ERR("[READ_PA] invalid req_user\n");
			break;
		}

		addrs = kcalloc(req_user->count, sizeof(uint32_t), GFP_KERNEL);
		if (NULL == addrs) {
			CMDQ_ERR("[READ_PA] fail to alloc addr buf\n");
			break;
		}

		values = kcalloc(req_user->count, sizeof(uint32_t), GFP_KERNEL);
		if (NULL == values) {
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
			CMDQ_LOG("[READ_PA] req read dma address 0x%pa\n", &pa);
			values[i] = cmdqCoreReadWriteAddress(pa);
		}

		/* copy value to user */
		if (copy_to_user
		    (CMDQ_U32_PTR(req_user->values), values, req_user->count * sizeof(uint32_t))) {
			CMDQ_ERR("[READ_PA] fail to copy to user value buf\n");
			break;
		}

	} while (0);

	kfree(addrs);
	kfree(values);
}

static long cmdq_driver_destroy_secure_medadata(struct cmdqCommandStruct *pCommand)
{
	if (pCommand->secData.addrMetadatas) {
		kfree(CMDQ_U32_PTR(pCommand->secData.addrMetadatas));
		pCommand->secData.addrMetadatas = (cmdqU32Ptr_t) (unsigned long)NULL;
	}

	return 0;
}

static long cmdq_driver_create_secure_medadata(struct cmdqCommandStruct *pCommand)
{
	void *pAddrMetadatas = NULL;
	const uint32_t length =
	    (pCommand->secData.addrMetadataCount) * sizeof(cmdqSecAddrMetadataStruct);

	/* verify parameter */
	if ((false == pCommand->secData.isSecure) && (0 != pCommand->secData.addrMetadataCount)) {

		/* normal path with non-zero secure metadata */
		CMDQ_ERR
		    ("[secData]mismatch secData.isSecure(%d) and secData.addrMetadataCount(%d)\n",
		     pCommand->secData.isSecure, pCommand->secData.addrMetadataCount);
		return -EFAULT;
	}

	/* revise max count field */
	pCommand->secData.addrMetadataMaxCount = pCommand->secData.addrMetadataCount;

	/* bypass 0 metadata case */
	if (0 == pCommand->secData.addrMetadataCount) {
		pCommand->secData.addrMetadatas = (cmdqU32Ptr_t) (unsigned long)NULL;
		return 0;
	}

	/* create kernel-space buffer for working */
	pAddrMetadatas = kzalloc(length, GFP_KERNEL);
	if (NULL == pAddrMetadatas) {
		CMDQ_ERR("[secData]kzalloc for addrMetadatas failed, count:%d, alloacted_size:%d\n",
			 pCommand->secData.addrMetadataCount, length);
		return -ENOMEM;
	}

	/* copy from user */
	if (copy_from_user(pAddrMetadatas, CMDQ_U32_PTR(pCommand->secData.addrMetadatas), length)) {

		CMDQ_ERR("[secData]fail to copy user addrMetadatas\n");
		/* replace buffer first to ensure that */
		/* addrMetadatas is valid kernel space buffer address when free it */
		pCommand->secData.addrMetadatas = (cmdqU32Ptr_t) (unsigned long)pAddrMetadatas;
		/* free secure path metadata */
		cmdq_driver_destroy_secure_medadata(pCommand);
		return -EFAULT;
	}

	/* replace buffer */
	pCommand->secData.addrMetadatas = (cmdqU32Ptr_t) (unsigned long)pAddrMetadatas;


	return 0;
}

static long cmdq_driver_process_command_request(struct cmdqCommandStruct *pCommand)
{
	int32_t status = 0;
	uint32_t *userRegValue = NULL;
	uint32_t userRegCount = 0;

	if (pCommand->regRequest.count != pCommand->regValue.count) {
		CMDQ_ERR("mismatch regRequest and regValue\n");
		return -EFAULT;
	}

	/* allocate secure medatata */
	status = cmdq_driver_create_secure_medadata(pCommand);
	if (0 != status)
		return status;


	/* backup since we are going to replace these */
	userRegValue = CMDQ_U32_PTR(pCommand->regValue.regValues);
	userRegCount = pCommand->regValue.count;

	/* create kernel-space address buffer */
	status = cmdq_driver_create_reg_address_buffer(pCommand);
	if (0 != status) {
		/* free secure path metadata */
		cmdq_driver_destroy_secure_medadata(pCommand);
		return status;
	}

	/* create kernel-space value buffer */
	pCommand->regValue.regValues = (cmdqU32Ptr_t) (unsigned long)
	    kzalloc(pCommand->regRequest.count * sizeof(uint32_t), GFP_KERNEL);
	pCommand->regValue.count = pCommand->regRequest.count;
	if (NULL == CMDQ_U32_PTR(pCommand->regValue.regValues)) {
		kfree(CMDQ_U32_PTR(pCommand->regRequest.regAddresses));
		return -ENOMEM;
	}

	/* scenario id fixup */
	if ((CMDQ_SCENARIO_USER_DISP_COLOR == pCommand->scenario)
	    || (CMDQ_SCENARIO_USER_MDP == pCommand->scenario)) {
		CMDQ_VERBOSE("user space request, scenario:%d\n", pCommand->scenario);
	} else {
		CMDQ_VERBOSE("[WARNING]fix user space request to CMDQ_SCENARIO_USER_SPACE\n");
		pCommand->scenario = CMDQ_SCENARIO_USER_SPACE;
	}

	if (CMDQ_SCENARIO_USER_MDP == pCommand->scenario)
		CMDQ_MSG("srcHandle=0x%08x, dstHandle=0x%08x\n",
			 pCommand->secData.srcHandle, pCommand->secData.dstHandle);

	status = cmdqCoreSubmitTask(pCommand);
	if (0 > status) {
		CMDQ_ERR("Submit user commands for execution failed = %d\n", status);
		cmdq_driver_destroy_secure_medadata(pCommand);
		kfree(CMDQ_U32_PTR(pCommand->regRequest.regAddresses));
		kfree(CMDQ_U32_PTR(pCommand->regValue.regValues));
		return -EFAULT;
	}

	/* notify kernel space dump callback */
	if (0 != pCommand->debugRegDump) {
		status = cmdqCoreDebugRegDumpEnd(pCommand->debugRegDump,
						 pCommand->regRequest.count - userRegCount,
						 CMDQ_U32_PTR(pCommand->regValue.regValues) +
						 userRegCount);
		if (0 != status)
			CMDQ_ERR("cmdqCoreDebugRegDumpEnd returns %d\n", status);

	}

	/* copy back to user space buffer */
	if (userRegValue && userRegCount) {
		/* copy results back to user space */
		CMDQ_VERBOSE("regValue[0] is %d\n", CMDQ_U32_PTR(pCommand->regValue.regValues)[0]);
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
		cmdq_driver_process_read_address_request(&pCommand->readAddress);


	/* free allocated secure metadata */
	cmdq_driver_destroy_secure_medadata(pCommand);

	return 0;
}

static long cmdq_ioctl(struct file *pFile, unsigned int code, unsigned long param)
{
	int mutex;
	struct cmdqCommandStruct command;
	struct cmdqJobStruct job;
	struct cmdqFileNodeStruct *pNode;
	unsigned long flags;
	int count[CMDQ_MAX_ENGINE_COUNT];
	struct TaskStruct *pTask;
	int32_t status;
	struct cmdqJobResultStruct jobResult;
	uint32_t *userRegValue = NULL;
	uint32_t userRegCount = 0;
	/* backup value after task release */
	uint32_t regCount = 0, regCountUserSpace = 0, regUserToken = 0;

	switch (code) {
	case CMDQ_IOCTL_LOCK_MUTEX:
		mutex = cmdqMutexAcquire();
		if (-1 == mutex)
			return -IOCTL_RET_LOCK_MUTEX_FAIL;


		if (copy_to_user((void *)param, &mutex, sizeof(int))) {
			CMDQ_ERR("Copy mutex number to user failed\n");
			cmdqMutexRelease(mutex);
			return -IOCTL_RET_COPY_MUTEX_NUM_TO_USER_FAIL;
		}
		/* register mutex into file node data */
		pNode = (struct cmdqFileNodeStruct *)pFile->private_data;
		if (pNode) {
			spin_lock_irqsave(&pNode->nodeLock, flags);
			pNode->mutexFlag |= (1 << mutex);
			spin_unlock_irqrestore(&pNode->nodeLock, flags);
		}
		break;
	case CMDQ_IOCTL_UNLOCK_MUTEX:
		if (copy_from_user(&mutex, (void *)param, sizeof(int))) {
			CMDQ_ERR("Copy mutex number from user failed\n");
			return -IOCLT_RET_COPY_MUTEX_NUM_FROM_USER_FAIL;
		}

		status = cmdqMutexRelease(mutex);
		if (status < 0)
			return -IOCTL_RET_RELEASE_MUTEX_FAIL;


		if (-1 != mutex) {
			pNode = (struct cmdqFileNodeStruct *)pFile->private_data;
			spin_lock_irqsave(&pNode->nodeLock, flags);
			pNode->mutexFlag &= ~(1 << mutex);
			spin_unlock_irqrestore(&pNode->nodeLock, flags);
		}

		break;
	case CMDQ_IOCTL_EXEC_COMMAND:
		if (copy_from_user(&command, (void *)param, sizeof(struct cmdqCommandStruct)))
			return -IOCTL_RET_COPY_EXEC_CMD_FROM_USER_FAIL;

		if (cmdqCoreIsEarlySuspended() && (CMDQ_SCENARIO_USER_MDP == command.scenario))
			return -IOCTL_RET_IS_SUSPEND_WHEN_EXEC_CMD;


		/* insert private_data for resource reclaim */
		command.privateData = (cmdqU32Ptr_t) pFile->private_data;

		if (cmdq_driver_process_command_request(&command))
			return -IOCTL_RET_PROCESS_CMD_REQUEST_FAIL;


		break;
	case CMDQ_IOCTL_QUERY_USAGE:
		if (cmdqCoreQueryUsage(count))
			return -IOCLT_RET_QUERY_USAGE_FAIL;


		if (copy_to_user((void *)param, count, sizeof(int32_t) * CMDQ_MAX_ENGINE_COUNT)) {
			CMDQ_ERR("CMDQ_IOCTL_QUERY_USAGE copy_to_user failed\n");
			return -IOCTL_RET_COPY_USAGE_TO_USER_FAIL;
		}
		break;
	case CMDQ_IOCTL_ASYNC_JOB_EXEC:
		if (copy_from_user(&job, (void *)param, sizeof(struct cmdqJobStruct)))
			return -IOCTL_RET_COPY_ASYNC_JOB_EXEC_FROM_USER_FAIL;

		if (cmdqCoreIsEarlySuspended() && (CMDQ_SCENARIO_USER_MDP == job.command.scenario)) {
			CMDQ_ERR("CMDQ_IOCTL_ASYNC_JOB_EXEC suspended, return\n");
			return -IOCTL_RET_IS_SUSPEND_WHEN_ASYNC_JOB_EXEC;
		}

		/* not support secure path for async ioctl yet */
		if (true == job.command.secData.isSecure) {
			CMDQ_ERR("not support secure path for CMDQ_IOCTL_ASYNC_JOB_EXEC\n");
			return -IOCTL_RET_NOT_SUPPORT_SEC_PATH_FOR_ASYNC_JOB_EXEC;
		}

		/* backup */
		userRegCount = job.command.regRequest.count;

		/* insert private_data for resource reclaim */
		job.command.privateData = (cmdqU32Ptr_t) (unsigned long)(pFile->private_data);

		/* create kernel-space address buffer */
		status = cmdq_driver_create_reg_address_buffer(&job.command);
		if (0 != status)
			return -IOCTL_RET_CREATE_REG_ADDR_BUF_FAIL;

		/* scenario id fixup */
		if ((CMDQ_SCENARIO_USER_DISP_COLOR == job.command.scenario)
		    || (CMDQ_SCENARIO_USER_MDP == job.command.scenario)) {
			CMDQ_VERBOSE("user space request, scenario:%d\n", job.command.scenario);
		} else {
			CMDQ_VERBOSE("[WARNING]fix user space request to CMDQ_SCENARIO_USER_SPACE\n");
			job.command.scenario = CMDQ_SCENARIO_USER_SPACE;
		}
		status = cmdqCoreSubmitTaskAsync(&job.command, NULL, 0, &pTask);

		/* store user space request count in struct TaskStruct */
		/* for later retrieval */
		if (pTask) {
			pTask->regCountUserSpace = userRegCount;
			pTask->regUserToken = job.command.debugRegDump;
		}
		/* we don't need regAddress anymore, free it now */
		kfree(CMDQ_U32_PTR(job.command.regRequest.regAddresses));
		job.command.regRequest.regAddresses = (cmdqU32Ptr_t) (unsigned long)(NULL);

		if (status >= 0) {
			job.hJob = (unsigned long)pTask;
			if (copy_to_user((void *)param, (void *)&job, sizeof(struct cmdqJobStruct))) {
				CMDQ_ERR("CMDQ_IOCTL_ASYNC_JOB_EXEC copy_to_user failed\n");
				return -IOCLT_RET_COPY_ASYNC_JOB_EXEC_TO_USER_FAIL;
			}
		} else {
			job.hJob = (unsigned long)NULL;
			return -IOCTL_RET_SUBMIT_TASK_ASYNC_FAILED;
		}
		break;
	case CMDQ_IOCTL_ASYNC_JOB_WAIT_AND_CLOSE:
		if (copy_from_user(&jobResult, (void *)param, sizeof(jobResult))) {
			CMDQ_ERR("copy_from_user jobResult fail\n");
			return -IOCTL_RET_COPY_ASYNC_JOB_WAIT_AND_CLOSE_FROM_USER_FAIL;
		}

		if (cmdqCoreIsEarlySuspended() && (CMDQ_SCENARIO_USER_MDP == job.command.scenario)) {
			CMDQ_ERR("CMDQ_IOCTL_ASYNC_JOB_WAIT_AND_CLOSE suspended, return\n");
			return -IOCTL_RET_IS_SUSPEND_WHEN_ASYNC_JOB_WAIT_AND_CLOSE;
		}

		/* verify job handle */
		if (!cmdqIsValidTaskPtr((struct TaskStruct *)(unsigned long)jobResult.hJob)) {
			CMDQ_ERR("invalid task ptr = 0x%llx\n", jobResult.hJob);
			return -IOCTL_RET_INVALID_TASK_PTR;
		}
		pTask = (struct TaskStruct *)(unsigned long)jobResult.hJob;

		/* utility service, fill the engine flag. */
		/* this is required by MDP. */
		jobResult.engineFlag = pTask->engineFlag;

		/* check if reg buffer suffices */
		if (jobResult.regValue.count < pTask->regCountUserSpace) {
			jobResult.regValue.count = pTask->regCountUserSpace;
			if (copy_to_user((void *)param, (void *)&jobResult, sizeof(jobResult))) {
				CMDQ_ERR("copy_to_user fail, line=%d\n", __LINE__);
				return -IOCTL_RET_COPY_JOB_RESULT_TO_USER1_FAIL;
			}
			CMDQ_ERR("insufficient register buffer\n");
			return -IOCTL_RET_NOT_ENOUGH_REGISTER_BUFFER;
		}
		/* inform client the actual read register count */
		jobResult.regValue.count = pTask->regCountUserSpace;
		/* update user space before we replace the regValues pointer. */
		if (copy_to_user((void *)param, (void *)&jobResult, sizeof(jobResult))) {
			CMDQ_ERR("copy_to_user fail line=%d\n", __LINE__);
			return -IOCTL_RET_COPY_JOB_RESULT_TO_USER2_FAIL;
		}
		/* allocate kernel space result buffer */
		/* which contains kernel + user space requests */
		userRegValue = CMDQ_U32_PTR(jobResult.regValue.regValues);
		jobResult.regValue.regValues =
		    (cmdqU32Ptr_t) (unsigned
				    long)(kzalloc(pTask->regCount * sizeof(uint32_t), GFP_KERNEL));
		jobResult.regValue.count = pTask->regCount;
		if (NULL == CMDQ_U32_PTR(jobResult.regValue.regValues)) {
			CMDQ_ERR("no reg value buffer\n");
			return -IOCTL_RET_NO_REG_VAL_BUFFER;
		}
		/* backup value after task release */
		regCount = pTask->regCount;
		regCountUserSpace = pTask->regCountUserSpace;
		regUserToken = pTask->regUserToken;

		/* make sure the task is running and wait for it */
		status = cmdqCoreWaitResultAndReleaseTask(pTask,
							  &jobResult.regValue,
							  msecs_to_jiffies
							  (CMDQ_DEFAULT_TIMEOUT_MS));
		if (status < 0) {
			CMDQ_ERR("waitResultAndReleaseTask fail=%d\n", status);
			/* free kernel space result buffer */
			kfree(CMDQ_U32_PTR(jobResult.regValue.regValues));
			return -IOCTL_RET_WAIT_RESULT_AND_RELEASE_TASK_FAIL;
		}
		/* pTask is released, do not access it any more */
		pTask = NULL;

		/* notify kernel space dump callback */
		if (regCount > regCountUserSpace) {
			CMDQ_VERBOSE("kernel space reg dump = %d, %d, %d\n", regCount,
				     regCountUserSpace, regUserToken);
			status = cmdqCoreDebugRegDumpEnd(regUserToken,
							 regCount - regCountUserSpace,
							 CMDQ_U32_PTR(jobResult.regValue.regValues +
								      regCountUserSpace));
			if (0 != status)
				CMDQ_ERR("cmdqCoreDebugRegDumpEnd returns %d\n", status);

		}
		/* copy result to user space */
		if (copy_to_user
		    ((void *)userRegValue, (void *)(unsigned long)jobResult.regValue.regValues,
		     regCountUserSpace * sizeof(uint32_t))) {
			CMDQ_ERR("Copy REGVALUE to user space failed\n");
			return -IOCLT_RET_COPY_REG_VALUE_TO_USER_FAIL;
		}

		if (jobResult.readAddress.count > 0)
			cmdq_driver_process_read_address_request(&jobResult.readAddress);

		/* free kernel space result buffer */
		kfree(CMDQ_U32_PTR(jobResult.regValue.regValues));
		break;
	case CMDQ_IOCTL_ALLOC_WRITE_ADDRESS:
		do {
			struct cmdqWriteAddressStruct addrReq;
			dma_addr_t paStart = 0;

			CMDQ_LOG("CMDQ_IOCTL_ALLOC_WRITE_ADDRESS\n");

			if (copy_from_user(&addrReq, (void *)param, sizeof(addrReq))) {
				CMDQ_ERR("CMDQ_IOCTL_ALLOC_WRITE_ADDRESS copy_from_user failed\n");
				return -IOCTL_RET_COPY_ALLOC_WRITE_ADDR_FROM_USER_FAIL;
			}

			status = cmdqCoreAllocWriteAddress(addrReq.count, &paStart);
			if (0 != status) {
				CMDQ_ERR
				    ("CMDQ_IOCTL_ALLOC_WRITE_ADDRESS cmdqCoreAllocWriteAddress() failed\n");
				return -IOCTL_RET_ALLOC_WRITE_ADDR_FAIL;
			}


			addrReq.startPA = (uint32_t) paStart;
			CMDQ_LOG("CMDQ_IOCTL_ALLOC_WRITE_ADDRESS get 0x%08x\n", addrReq.startPA);

			if (copy_to_user((void *)param, &addrReq, sizeof(addrReq))) {
				CMDQ_ERR("CMDQ_IOCTL_ALLOC_WRITE_ADDRESS copy_to_user failed\n");
				return -IOCTL_RET_COPY_ALLOC_WRITE_ADDR_TO_USER_FAIL;
			}
			status = 0;
		} while (0);
		break;
	case CMDQ_IOCTL_FREE_WRITE_ADDRESS:
		do {
			struct cmdqWriteAddressStruct freeReq;

			CMDQ_LOG("CMDQ_IOCTL_FREE_WRITE_ADDRESS\n");

			if (copy_from_user(&freeReq, (void *)param, sizeof(freeReq))) {
				CMDQ_ERR("CMDQ_IOCTL_FREE_WRITE_ADDRESS copy_from_user failed\n");
				return -IOCTL_RET_COPY_FREE_WRITE_ADDR_FROM_USER_FAIL;
			}

			status = cmdqCoreFreeWriteAddress(freeReq.startPA);
			if (0 != status)
				return -IOCTL_RET_FREE_WRITE_ADDR_FAIL;

			status = 0;
		} while (0);
		break;
	case CMDQ_IOCTL_READ_ADDRESS_VALUE:
		do {
			struct cmdqReadAddressStruct readReq;

			CMDQ_LOG("CMDQ_IOCTL_READ_ADDRESS_VALUE\n");

			if (copy_from_user(&readReq, (void *)param, sizeof(readReq))) {
				CMDQ_ERR("CMDQ_IOCTL_READ_ADDRESS_VALUE copy_from_user failed\n");
				return -IOCTL_RET_COPY_READ_ADDR_VAL_FROM_USER_FAIL;
			}
			/* this will copy result to readReq->values buffer */
			cmdq_driver_process_read_address_request(&readReq);

			status = 0;

		} while (0);
		break;
	case CMDQ_IOCTL_QUERY_CAP_BITS:
		do {
			int capBits = 0;

			if (cmdq_core_support_wait_and_receive_event_in_same_tick())
				capBits |= (1L << CMDQ_CAP_WFE);
			else
				capBits &= ~(1L << CMDQ_CAP_WFE);


			if (copy_to_user((void *)param, &capBits, sizeof(int))) {
				CMDQ_ERR("Copy capacity bits to user space failed\n");
				return -IOCTL_RET_COPY_CAP_BITS_TO_USER_FAIL;
			}
		} while (0);
		break;
	case CMDQ_IOCTL_SYNC_BUF_HDCP_VERSION:
#ifdef CMDQ_SECURE_PATH_SUPPORT
		do {
			struct cmdqSyncHandleHdcpStruct syncHandle;

			if (copy_from_user(&syncHandle, (void *)param, sizeof(syncHandle))) {
				CMDQ_ERR
				    ("CMDQ_IOCTL_SYNC_BUF_HDCP_VERSION copy_from_user failed\n");
				return -IOCTL_RET_COPY_HDCP_VERSION_FROM_USER_FAIL;
			}
			status = cmdq_sec_sync_handle_hdcp_unlock(syncHandle);
			if (0 != status)
				CMDQ_ERR("cmdq_sec_sync_handle_hdcp_unlock returns %d\n", status);

		} while (0);
#else
		CMDQ_ERR("SVP not supported\n");
		return -IOCTL_RET_SVP_NOT_SUPPORT;
#endif
		break;
	default:
		CMDQ_ERR("unrecognized ioctl 0x%08x\n", code);
		CMDQ_ERR("CMDQ_IOCTL_LOCK_MUTEX:0x%08lx sizeof(int) = %ld\n",
			CMDQ_IOCTL_LOCK_MUTEX, sizeof(int));
		CMDQ_ERR("CMDQ_IOCTL_UNLOCK_MUTEX:0x%08lx sizeof(int) = %ld\n",
			CMDQ_IOCTL_UNLOCK_MUTEX, sizeof(int));
		CMDQ_ERR("CMDQ_IOCTL_EXEC_COMMAND:0x%08lx sizeof(struct cmdqCommandStruct) = %ld\n",
			CMDQ_IOCTL_EXEC_COMMAND, sizeof(struct cmdqCommandStruct));
		CMDQ_ERR("CMDQ_IOCTL_QUERY_USAGE:0x%08lx sizeof(struct cmdqUsageInfoStruct) = %ld\n",
			CMDQ_IOCTL_QUERY_USAGE, sizeof(struct cmdqUsageInfoStruct));
		CMDQ_ERR("CMDQ_IOCTL_ASYNC_JOB_EXEC:0x%08lx sizeof(struct cmdqJobStruct) = %ld\n",
			CMDQ_IOCTL_ASYNC_JOB_EXEC, sizeof(struct cmdqJobStruct));
		CMDQ_ERR("CMDQ_IOCTL_ASYNC_JOB_WAIT_AND_CLOSE:0x%08lx sizeof(struct cmdqJobResultStruct) = %ld\n",
			CMDQ_IOCTL_ASYNC_JOB_WAIT_AND_CLOSE, sizeof(struct cmdqJobResultStruct));
		CMDQ_ERR("CMDQ_IOCTL_ALLOC_WRITE_ADDRESS:0x%08lx sizeof(struct cmdqWriteAddressStruct) = %ld\n",
			CMDQ_IOCTL_ALLOC_WRITE_ADDRESS, sizeof(struct cmdqWriteAddressStruct));
		CMDQ_ERR("CMDQ_IOCTL_FREE_WRITE_ADDRESS:0x%08lx sizeof(struct cmdqWriteAddressStruct) = %ld\n",
			CMDQ_IOCTL_FREE_WRITE_ADDRESS, sizeof(struct cmdqWriteAddressStruct));
		CMDQ_ERR("CMDQ_IOCTL_READ_ADDRESS_VALUE:0x%08lx sizeof(struct cmdqReadAddressStruct) = %ld\n",
			CMDQ_IOCTL_READ_ADDRESS_VALUE, sizeof(struct cmdqReadAddressStruct));
		CMDQ_ERR("CMDQ_IOCTL_QUERY_CAP_BITS:0x%08lx sizeof(int) = %ld\n",
			CMDQ_IOCTL_QUERY_CAP_BITS, sizeof(int));
		CMDQ_ERR("CMDQ_IOCTL_SYNC_BUF_HDCP_VERSION:0x%08lx sizeof(struct cmdqSyncHandleHdcpStruct) = %ld\n",
			CMDQ_IOCTL_SYNC_BUF_HDCP_VERSION, sizeof(struct cmdqSyncHandleHdcpStruct));
		return -IOCTL_RET_UNRECOGNIZED_IOCTL;
	}

	return IOCTL_RET_SUCCESS;
}

static long cmdq_ioctl_compat(struct file *pFile, unsigned int code, unsigned long param)
{
#ifdef CONFIG_COMPAT
	switch (code) {
	case CMDQ_IOCTL_QUERY_USAGE:
	case CMDQ_IOCTL_EXEC_COMMAND:
	case CMDQ_IOCTL_ASYNC_JOB_EXEC:
	case CMDQ_IOCTL_ASYNC_JOB_WAIT_AND_CLOSE:
	case CMDQ_IOCTL_ALLOC_WRITE_ADDRESS:
	case CMDQ_IOCTL_FREE_WRITE_ADDRESS:
	case CMDQ_IOCTL_READ_ADDRESS_VALUE:
	case CMDQ_IOCTL_QUERY_CAP_BITS:
		/* All ioctl structures should be the same size in 32-bit and 64-bit linux. */
		return cmdq_ioctl(pFile, code, param);
	case CMDQ_IOCTL_LOCK_MUTEX:
		CMDQ_ERR("[COMPAT]deprecated ioctl 0x%08x\n", code);
		return -IOCTL_RET_DEPRECATE_LOCK_MUTEX;
	case CMDQ_IOCTL_UNLOCK_MUTEX:
		CMDQ_ERR("[COMPAT]deprecated ioctl 0x%08x\n", code);
		return -IOCTL_RET_DEPRECATE_UNLOCK_MUTEX;
	default:
		CMDQ_ERR("[COMPAT]unrecognized ioctl 0x%08x\n", code);
		CMDQ_ERR("CMDQ_IOCTL_LOCK_MUTEX:0x%08lx sizeof(int) = %ld\n",
			CMDQ_IOCTL_LOCK_MUTEX, sizeof(int));
		CMDQ_ERR("CMDQ_IOCTL_UNLOCK_MUTEX:0x%08lx sizeof(int) = %ld\n",
			CMDQ_IOCTL_UNLOCK_MUTEX, sizeof(int));
		CMDQ_ERR("CMDQ_IOCTL_EXEC_COMMAND:0x%08lx sizeof(struct cmdqCommandStruct) = %ld\n",
			CMDQ_IOCTL_EXEC_COMMAND, sizeof(struct cmdqCommandStruct));
		CMDQ_ERR("CMDQ_IOCTL_QUERY_USAGE:0x%08lx sizeof(struct cmdqUsageInfoStruct) = %ld\n",
			CMDQ_IOCTL_QUERY_USAGE, sizeof(struct cmdqUsageInfoStruct));
		CMDQ_ERR("CMDQ_IOCTL_ASYNC_JOB_EXEC:0x%08lx sizeof(struct cmdqJobStruct) = %ld\n",
			CMDQ_IOCTL_ASYNC_JOB_EXEC, sizeof(struct cmdqJobStruct));
		CMDQ_ERR("CMDQ_IOCTL_ASYNC_JOB_WAIT_AND_CLOSE:0x%08lx sizeof(struct cmdqJobResultStruct) = %ld\n",
			CMDQ_IOCTL_ASYNC_JOB_WAIT_AND_CLOSE, sizeof(struct cmdqJobResultStruct));
		CMDQ_ERR("CMDQ_IOCTL_ALLOC_WRITE_ADDRESS:0x%08lx sizeof(struct cmdqWriteAddressStruct) = %ld\n",
			CMDQ_IOCTL_ALLOC_WRITE_ADDRESS, sizeof(struct cmdqWriteAddressStruct));
		CMDQ_ERR("CMDQ_IOCTL_FREE_WRITE_ADDRESS:0x%08lx sizeof(struct cmdqWriteAddressStruct) = %ld\n",
			CMDQ_IOCTL_FREE_WRITE_ADDRESS, sizeof(struct cmdqWriteAddressStruct));
		CMDQ_ERR("CMDQ_IOCTL_READ_ADDRESS_VALUE:0x%08lx sizeof(struct cmdqReadAddressStruct) = %ld\n",
			CMDQ_IOCTL_READ_ADDRESS_VALUE, sizeof(struct cmdqReadAddressStruct));
		CMDQ_ERR("CMDQ_IOCTL_QUERY_CAP_BITS:0x%08lx sizeof(int) = %ld\n",
			CMDQ_IOCTL_QUERY_CAP_BITS, sizeof(int));
		CMDQ_ERR("CMDQ_IOCTL_SYNC_BUF_HDCP_VERSION:0x%08lx sizeof(struct cmdqSyncHandleHdcpStruct) = %ld\n",
			CMDQ_IOCTL_SYNC_BUF_HDCP_VERSION, sizeof(struct cmdqSyncHandleHdcpStruct));
		return -IOCTL_RET_UNRECOGNIZED_COMPAT_IOCTL;
	}

#else
	return -IOCTL_RET_CONFIG_COMPAT_NOT_OPEN;
#endif
}

static const struct file_operations cmdqOP = {
	.owner = THIS_MODULE,
	.open = cmdq_open,
	.release = cmdq_release,
	.unlocked_ioctl = cmdq_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = cmdq_ioctl_compat,
#endif
};

static int cmdq_pm_notifier_cb(struct notifier_block *nb, unsigned long event, void *ptr)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:	/* Going to suspend the system */
		/* The next stage is freeze process. */
		/* We will queue all request in suspend callback, */
		/* so don't care this stage */
		return NOTIFY_DONE;	/* don't care this event */
	case PM_POST_SUSPEND:
		/* processes had resumed in previous stage (system resume callback) */
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
			irqStatus = CMDQ_REG_GET32(CMDQ_CURR_IRQ_STATUS) & 0x0FFFF;
			for (index = 0; (irqStatus != 0xFFFF) && index < CMDQ_MAX_THREAD_COUNT;
			     index++) {
				/* STATUS bit set to 0 means IRQ asserted */
				if (irqStatus & (1 << index))
					continue;

				/* so we mark irqStatus to 1 to denote finished processing */
				/* and we can early-exit if no more threads being asserted */
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

#if 0
static irqreturn_t cmdq_sec_irq_handler(int IRQ, void *pDevice)
{
	int index;
	uint32_t irqStatus;
	bool handled = false;	/* we share IRQ bit with CQ-DMA, */
	/* so it is possible that this handler */
	/* is called but GCE does not have IRQ flag. */
	do {
		if (cmdq_dev_get_irq_id() == IRQ) {
			irqStatus = CMDQ_REG_GET32(CMDQ_CURR_IRQ_STATUS) & 0x0FFFF;
			for (index = 0; (irqStatus != 0xFFFF) && index < CMDQ_MAX_THREAD_COUNT;
			     index++) {
				/* STATUS bit set to 0 means IRQ asserted */
				if (irqStatus & (1 << index))
					continue;

				/* so we mark irqStatus to 1 to denote finished processing */
				/* and we can early-exit if no more threads being asserted */
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
#endif


static int cmdq_create_debug_entries(void)
{
	struct proc_dir_entry *debugDirEntry = NULL;

	debugDirEntry = proc_mkdir(CMDQ_DRIVER_DEVICE_NAME "_debug", NULL);
	if (debugDirEntry) {
		struct proc_dir_entry *entry = NULL;

		entry = proc_create("status", 0440, debugDirEntry, &cmdqDebugStatusOp);
		entry = proc_create("error", 0440, debugDirEntry, &cmdqDebugErrorOp);
		entry = proc_create("record", 0440, debugDirEntry, &cmdqDebugRecordOp);
		entry = proc_create("log_level", 0660, debugDirEntry, &cmdqDebugLevelOp);
	}

	return 0;
}


static int cmdq_probe(struct platform_device *pDevice)
{
	int status;
	struct device *object;

	CMDQ_MSG("CMDQ driver probe begin\n");

	/* init cmdq device related data */
	cmdq_dev_init(pDevice);

	/* init cmdq context */
	cmdqCoreInitialize();

	status = alloc_chrdev_region(&gCmdqDevNo, 0, 1, CMDQ_DRIVER_DEVICE_NAME);
	if (status != 0)
		CMDQ_ERR("Get CMDQ device major number(%d) failed(%d)\n", gCmdqDevNo, status);
	else
		CMDQ_MSG("Get CMDQ device major number(%d) success(%d)\n", gCmdqDevNo, status);


	/* ioctl access point (/dev/mtk_cmdq) */
	gCmdqCDev = cdev_alloc();
	gCmdqCDev->owner = THIS_MODULE;
	gCmdqCDev->ops = &cmdqOP;

	status = cdev_add(gCmdqCDev, gCmdqDevNo, 1);

	gCMDQClass = class_create(THIS_MODULE, CMDQ_DRIVER_DEVICE_NAME);
	object = device_create(gCMDQClass, NULL, gCmdqDevNo, NULL, CMDQ_DRIVER_DEVICE_NAME);

	CMDQ_LOG("register IRQ:%d\n", cmdq_dev_get_irq_id());
	status =
	    request_irq(cmdq_dev_get_irq_id(), cmdq_irq_handler, IRQF_TRIGGER_LOW,
			CMDQ_DRIVER_DEVICE_NAME, gCmdqCDev);
	if (status != 0) {
		CMDQ_ERR("Register cmdq driver irq handler(%d) failed(%d)\n", gCmdqDevNo, status);
		return -EFAULT;
	}
#if 0				/* remove register secure IRQ in Normal world . TZ register instead */
	/* although secusre CMDQ driver is responsible for handle secure IRQ, */
	/* MUST registet secure IRQ to GIC in normal world to ensure it will be initialize correctly */
	/* (that's because t-base does not support GIC init IRQ in secure world...) */
	CMDQ_LOG("register sec IRQ:%d\n", cmdq_dev_get_irq_secure_id());
	status =
	    request_irq(cmdq_dev_get_irq_secure_id(), cmdq_sec_irq_handler, IRQF_TRIGGER_LOW,
			"TEE IRQ", gCmdqCDev);
	if (status != 0) {
		CMDQ_ERR("Register cmdq driver secure irq handler(%d) failed(%d)\n", gCmdqDevNo,
			 status);
		return -EFAULT;
	}
#endif
	/* CMDQ_ERR("prepare to create device mtk_cmdq\n"); */
	/* global ioctl access point (/proc/mtk_cmdq) */
	if (NULL == proc_create(CMDQ_DRIVER_DEVICE_NAME, 0644, NULL, &cmdqOP)) {
		CMDQ_ERR("CMDQ procfs node create failed\n");
		return -EFAULT;
	}
#ifdef CMDQ_OF_SUPPORT
	/* CCF - Common Clock Framework */
	cmdq_core_get_clk_map(pDevice);
#endif
	/* proc debug access point */
	cmdq_create_debug_entries();

	/* device attributes for debugging */
	device_create_file(&pDevice->dev, &dev_attr_status);
	device_create_file(&pDevice->dev, &dev_attr_error);
	device_create_file(&pDevice->dev, &dev_attr_record);
	device_create_file(&pDevice->dev, &dev_attr_log_level);
	device_create_file(&pDevice->dev, &dev_attr_profile_enable);

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
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cmdq_earlysuspend(struct early_suspend *h)
{
	cmdqCoreEarlySuspend();
}


static void cmdq_lateresume(struct early_suspend *h)
{
	cmdqCoreLateResume();
}
#endif

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

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend cmdq_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1,
	.suspend = cmdq_earlysuspend,
	.resume = cmdq_lateresume,
};
#endif
static int __init cmdq_init(void)
{
	int status;

	CMDQ_MSG("CMDQ driver init begin\n");

	/* Initialize mutex */
	cmdqMutexInitialize();

	/* Initialize group callback */
	cmdqCoreInitGroupCB();

	/* Register MDP callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_MDP, cmdqMdpClockOn, cmdqMdpDumpInfo, cmdqMdpResetEng,
			   cmdqMdpClockOff);

	/* Register VENC callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_VENC, NULL, cmdqVEncDumpInfo, NULL, NULL);

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&cmdq_early_suspend_handler);
#endif

	status = platform_driver_register(&gCmdqDriver);
	if (0 != status) {
		CMDQ_ERR("Failed to register the CMDQ driver(%d)\n", status);
		return -ENODEV;
	}

	/* register pm notifier */
	status = register_pm_notifier(&cmdq_pm_notifier_block);
	if (0 != status) {
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

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&cmdq_early_suspend_handler);
#endif

	/* register pm notifier */
	status = unregister_pm_notifier(&cmdq_pm_notifier_block);
	if (0 != status)
		CMDQ_ERR("Failed to unregister_pm_notifier(%d)\n", status);


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

	/* De-Initialize mutex */
	cmdqMutexDeInitialize();

	CMDQ_MSG("CMDQ driver exit end\n");
}

static int __init cmdq_driver_init_secure_path(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	cmdq_sec_init_secure_path();
#endif
	return 0;
}

subsys_initcall(cmdq_init);
late_initcall(cmdq_driver_init_secure_path);
module_exit(cmdq_exit);

MODULE_DESCRIPTION("MTK CMDQ driver");
MODULE_AUTHOR("Pablo<pablo.sun@mediatek.com>");
MODULE_LICENSE("GPL");
