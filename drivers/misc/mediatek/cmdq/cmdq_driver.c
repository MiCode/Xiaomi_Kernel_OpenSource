#include "cmdq_driver.h"
#include "cmdq_struct.h"
#include "cmdq_core.h"
#include "cmdq_reg.h"
#include "cmdq_mdp.h"
#include "cmdq_device.h"
#include "cmdq_platform.h"
#include "cmdq_sec.h"

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

#ifndef CMDQ_OF_SUPPORT
#include <mach/mt_irq.h>		// mt_irq.h is not available on device tree enabled platforms
#endif

#ifdef CMDQ_OF_SUPPORT
/**
 * @device tree porting note
 * alps/kernel-3.10/arch/arm64/boot/dts/{platform}.dts
 *  - use of_device_id to match driver and device
 *  - use io_map to map and get VA of HW's rgister
**/
static const struct of_device_id cmdq_of_ids[] = {
	{.compatible = "mediatek,GCE",},
	{}
};
#endif

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

static int cmdq_open(struct inode *pInode, struct file *pFile)
{
	cmdqFileNodeStruct *pNode;

	CMDQ_VERBOSE("CMDQ driver open fd=%p begin\n", pFile);

	pFile->private_data = kzalloc(sizeof(cmdqFileNodeStruct), GFP_KERNEL);
	if (NULL == pFile->private_data) {
		CMDQ_ERR("Can't allocate memory for CMDQ file node\n");
		return -ENOMEM;
	}

	pNode = (cmdqFileNodeStruct *) pFile->private_data;
	pNode->userPID = current->pid;
	pNode->userTGID = current->tgid;

	INIT_LIST_HEAD(&(pNode->taskList));
	spin_lock_init(&pNode->nodeLock);

	CMDQ_VERBOSE("CMDQ driver open end\n");

	return 0;
}


static int cmdq_release(struct inode *pInode, struct file *pFile)
{
	cmdqFileNodeStruct *pNode;
	unsigned long flags;

	CMDQ_VERBOSE("CMDQ driver release fd=%p begin\n", pFile);

	pNode = (cmdqFileNodeStruct *) pFile->private_data;

	if (NULL == pNode) {
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

	if (NULL != pFile->private_data) {
		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}

	CMDQ_VERBOSE("CMDQ driver release end\n");

	return 0;
}

static int cmdq_driver_create_reg_address_buffer(cmdqCommandStruct *pCommand)
{
	int status = 0;
	uint32_t totalRegCount = 0;
	uint32_t *regAddrBuf = NULL;

	uint32_t *kernelRegAddr = NULL;
	uint32_t kernelRegCount = 0;

	const uint32_t userRegCount = pCommand->regRequest.count;


	if (0 != pCommand->debugRegDump) {
		/* get kernel dump request count */
		status =
		    cmdqCoreDebugRegDumpBegin(pCommand->debugRegDump, &kernelRegCount,
					      &kernelRegAddr);
		if (0 != status) {
			CMDQ_ERR
			    ("cmdqCoreDebugRegDumpBegin returns %d, ignore kernel reg dump request\n",
			     status);
			kernelRegCount = 0;
			kernelRegAddr = NULL;
		}
	}

	/* how many register to dump? */
	totalRegCount = kernelRegCount + userRegCount;

	if (0 == totalRegCount) {
		/* no need to dump register */
		pCommand->regRequest.count = 0;
		pCommand->regValue.count = 0;
		pCommand->regRequest.regAddresses = (cmdqU32Ptr_t)(unsigned long)NULL;
		pCommand->regValue.regValues = (cmdqU32Ptr_t)(unsigned long)NULL;
	} else {
		regAddrBuf = kzalloc(totalRegCount * sizeof(uint32_t), GFP_KERNEL);
		if (NULL == regAddrBuf) {
			return -ENOMEM;
		}

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
		pCommand->regRequest.regAddresses = (cmdqU32Ptr_t)(unsigned long)(regAddrBuf);
		pCommand->regRequest.count = totalRegCount;
	}

	return 0;
}

static void cmdq_driver_process_read_address_request(cmdqReadAddressStruct *req_user)
{
	/* create kernel-space buffer for working */
	uint32_t *addrs = NULL;
	uint32_t *values = NULL;
	dma_addr_t pa = 0;
	int i = 0;

	CMDQ_LOG("[READ_PA] cmdq_driver_process_read_address_request()\n");

	do {
		if (NULL == req_user ||
		    0 == req_user->count ||
		    NULL == CMDQ_U32_PTR(req_user->values) || NULL == CMDQ_U32_PTR(req_user->dmaAddresses)) {
			CMDQ_ERR("[READ_PA] invalid req_user\n");
			break;
		}

		addrs = kzalloc(req_user->count * sizeof(uint32_t), GFP_KERNEL);
		if (NULL == addrs) {
			CMDQ_ERR("[READ_PA] fail to alloc addr buf\n");
			break;
		}

		values = kzalloc(req_user->count * sizeof(uint32_t), GFP_KERNEL);
		if (NULL == values) {
			CMDQ_ERR("[READ_PA] fail to alloc value buf\n");
			break;
		}

		/* copy from user */
		if (copy_from_user
		    (addrs, CMDQ_U32_PTR(req_user->dmaAddresses), req_user->count * sizeof(uint32_t))) {
			CMDQ_ERR("[READ_PA] fail to copy user dmaAddresses\n");
			break;
		}

		/* actually read these PA write buffers */
		for (i = 0; i < req_user->count; ++i) {
			pa = (0xFFFFFFFF & addrs[i]);
			CMDQ_LOG("[READ_PA] req read dma addresss 0x%pa\n", &pa);
			values[i] = cmdqCoreReadWriteAddress(pa);
		}

		/* copy value to user */
		if (copy_to_user(CMDQ_U32_PTR(req_user->values), values, req_user->count * sizeof(uint32_t))) {
			CMDQ_ERR("[READ_PA] fail to copy to user value buf\n");
			break;
		}

	} while (0);


	if (addrs) {
		kfree(addrs);
	}

	if (values) {
		kfree(values);
	}

}

static long cmdq_driver_destroy_secure_medadata(cmdqCommandStruct *pCommand)
{
	if (pCommand->secData.addrMetadatas) {
		kfree(CMDQ_U32_PTR(pCommand->secData.addrMetadatas));
		pCommand->secData.addrMetadatas = (cmdqU32Ptr_t)(unsigned long)NULL;
	}

	return 0;
}

static long cmdq_driver_create_secure_medadata(cmdqCommandStruct *pCommand)
{
	void *pAddrMetadatas = NULL;
	const uint32_t length = (pCommand->secData.addrMetadataCount) * sizeof(cmdqSecAddrMetadataStruct);

	/* verify parameter */
	if ((false == pCommand->secData.isSecure) &&
		(0 != pCommand->secData.addrMetadataCount)) {

		/* normal path with non-zero secure metadata */
		CMDQ_ERR("[secData]mismatch secData.isSecure(%d) and secData.addrMetadataCount(%d)\n",
				pCommand->secData.isSecure,
				pCommand->secData.addrMetadataCount);
		return -EFAULT;
	}

	/* revise max count field */
	pCommand->secData.addrMetadataMaxCount = pCommand->secData.addrMetadataCount;

	/* bypass 0 metadata case*/
	if (0 == pCommand->secData.addrMetadataCount) {
		pCommand->secData.addrMetadatas = (cmdqU32Ptr_t)(unsigned long)NULL;
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
		pCommand->secData.addrMetadatas = (cmdqU32Ptr_t)(unsigned long)pAddrMetadatas;
		/* free secure path metadata */
		cmdq_driver_destroy_secure_medadata(pCommand);
		return -EFAULT;
	}

	/* replace buffer */
	pCommand->secData.addrMetadatas = (cmdqU32Ptr_t)(unsigned long)pAddrMetadatas;

#if 0
	cmdq_core_dump_secure_metadata(&(pCommand->secData));
#endif

	return 0;
}

static long cmdq_driver_process_command_request(cmdqCommandStruct *pCommand)
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
	if (0 != status) {
		return status;
	}

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
	pCommand->regValue.regValues = (cmdqU32Ptr_t)(unsigned long)
		    kzalloc(pCommand->regRequest.count * sizeof(uint32_t), GFP_KERNEL);
	pCommand->regValue.count = pCommand->regRequest.count;
	if (NULL == CMDQ_U32_PTR(pCommand->regValue.regValues)) {
		kfree(CMDQ_U32_PTR(pCommand->regRequest.regAddresses));
		return -ENOMEM;
	}

	/* scenario id fixup */
	cmdq_core_fix_command_desc_scenario_for_user_space_request(pCommand);

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
						 CMDQ_U32_PTR(pCommand->regValue.regValues) + userRegCount);
		if (0 != status) {
			CMDQ_ERR("cmdqCoreDebugRegDumpEnd returns %d\n", status);
		}
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

	if (pCommand->readAddress.count > 0) {
		cmdq_driver_process_read_address_request(&pCommand->readAddress);
	}

	/* free allocated secure metadata */
	cmdq_driver_destroy_secure_medadata(pCommand);

	return 0;
}

static long cmdq_ioctl(struct file *pFile, unsigned int code, unsigned long param)
{
	struct cmdqCommandStruct command;
	struct cmdqJobStruct job;
	int count[CMDQ_MAX_ENGINE_COUNT];
	struct TaskStruct *pTask;
	int32_t status;
	struct cmdqJobResultStruct jobResult;
	uint32_t *userRegValue = NULL;
	uint32_t userRegCount = 0;
	/* backup value after task release */
	uint32_t regCount = 0, regCountUserSpace = 0, regUserToken = 0;

	switch (code) {
	case CMDQ_IOCTL_EXEC_COMMAND:
		if (copy_from_user(&command, (void *)param, sizeof(cmdqCommandStruct))) {
			return -EFAULT;
		}

		/* insert private_data for resource reclaim */
		command.privateData = (void *)pFile->private_data;

		if (cmdq_driver_process_command_request(&command)) {
			return -EFAULT;
		}
		break;
	case CMDQ_IOCTL_QUERY_USAGE:
		if (cmdqCoreQueryUsage(count)) {
			return -EFAULT;
		}

		if (copy_to_user((void *)param, count, sizeof(int32_t) * CMDQ_MAX_ENGINE_COUNT)) {
			CMDQ_ERR("CMDQ_IOCTL_QUERY_USAGE copy_to_user failed\n");
			return -EFAULT;
		}
		break;
	case CMDQ_IOCTL_ASYNC_JOB_EXEC:
		if (copy_from_user(&job, (void *)param, sizeof(cmdqJobStruct))) {
			return -EFAULT;
		}

		/* not support secure path for async ioctl yet */
		if (true == job.command.secData.isSecure) {
			CMDQ_ERR("not support secure path for CMDQ_IOCTL_ASYNC_JOB_EXEC\n");
			return -EFAULT;
		}

		/* backup */
		userRegCount = job.command.regRequest.count;

		/* insert private_data for resource reclaim */
		job.command.privateData = (cmdqU32Ptr_t)(unsigned long)(pFile->private_data);

		/* create kernel-space address buffer */
		status = cmdq_driver_create_reg_address_buffer(&job.command);
		if (0 != status) {
			return status;
		}

		/* scenario id fixup */
		cmdq_core_fix_command_desc_scenario_for_user_space_request(&job.command);

		status = cmdqCoreSubmitTaskAsync(&job.command, NULL, 0, &pTask);

		/* store user space request count in TaskStruct */
		/* for later retrieval */
		if (pTask) {
			pTask->regCountUserSpace = userRegCount;
			pTask->regUserToken = job.command.debugRegDump;
		}

		/* we don't need regAddress anymore, free it now */
		kfree(CMDQ_U32_PTR(job.command.regRequest.regAddresses));
		job.command.regRequest.regAddresses = (cmdqU32Ptr_t)(unsigned long)(NULL);

		if (status >= 0) {
			job.hJob = (unsigned long)pTask;
			if (copy_to_user((void *)param, (void *)&job, sizeof(cmdqJobStruct))) {
				CMDQ_ERR("CMDQ_IOCTL_ASYNC_JOB_EXEC copy_to_user failed\n");
				return -EFAULT;
			}
		} else {
			job.hJob = (unsigned long)NULL;
			return -EFAULT;
		}
		break;
	case CMDQ_IOCTL_ASYNC_JOB_WAIT_AND_CLOSE:
		if (copy_from_user(&jobResult, (void *)param, sizeof(jobResult))) {
			CMDQ_ERR("copy_from_user jobResult fail\n");
			return -EFAULT;
		}

		/* verify job handle */
		if (!cmdqIsValidTaskPtr((TaskStruct *)(unsigned long)jobResult.hJob)) {
			CMDQ_ERR("invalid task ptr = 0x%llx\n", jobResult.hJob);
			return -EFAULT;
		}
		pTask = (TaskStruct *)(unsigned long)jobResult.hJob;

		/* utility service, fill the engine flag. */
		/* this is required by MDP. */
		jobResult.engineFlag = pTask->engineFlag;

		/* check if reg buffer suffices */
		if (jobResult.regValue.count < pTask->regCountUserSpace) {
			jobResult.regValue.count = pTask->regCountUserSpace;
			if (copy_to_user((void *)param, (void *)&jobResult, sizeof(jobResult))) {
				CMDQ_ERR("copy_to_user fail, line=%d\n", __LINE__);
				return -EINVAL;
			}
			CMDQ_ERR("insufficient register buffer\n");
			return -ENOMEM;
		}

		/* inform client the actual read register count */
		jobResult.regValue.count = pTask->regCountUserSpace;
		/* update user space before we replace the regValues pointer. */
		if (copy_to_user((void *)param, (void *)&jobResult, sizeof(jobResult))) {
			CMDQ_ERR("copy_to_user fail line=%d\n", __LINE__);
			return -EINVAL;
		}

		/* allocate kernel space result buffer */
		/* which contains kernel + user space requests */
		userRegValue = CMDQ_U32_PTR(jobResult.regValue.regValues);
		jobResult.regValue.regValues = 
			(cmdqU32Ptr_t)(unsigned long)(kzalloc(pTask->regCount * sizeof(uint32_t), GFP_KERNEL));
		jobResult.regValue.count = pTask->regCount;
		if (NULL == CMDQ_U32_PTR(jobResult.regValue.regValues)) {
			CMDQ_ERR("no reg value buffer\n");
			return -ENOMEM;
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
			return status;
		}

		/* pTask is released, do not access it any more */
		pTask = NULL;

		/* notify kernel space dump callback */
		if (regCount > regCountUserSpace) {
			CMDQ_VERBOSE("kernel space reg dump = %d, %d, %d\n", regCount,
				     regCountUserSpace, regUserToken);
			status = cmdqCoreDebugRegDumpEnd(
						regUserToken,
						regCount - regCountUserSpace,
						CMDQ_U32_PTR(jobResult.regValue.regValues + regCountUserSpace));
			if (0 != status) {
				CMDQ_ERR("cmdqCoreDebugRegDumpEnd returns %d\n", status);
			}
		}

		/* copy result to user space */
		if (copy_to_user
		    ((void *)userRegValue, (void *)(unsigned long)jobResult.regValue.regValues,
		     regCountUserSpace * sizeof(uint32_t))) {
			CMDQ_ERR("Copy REGVALUE to user space failed\n");
			return -EFAULT;
		}

		if (jobResult.readAddress.count > 0) {
			cmdq_driver_process_read_address_request(&jobResult.readAddress);
		}

		/* free kernel space result buffer */
		kfree(CMDQ_U32_PTR(jobResult.regValue.regValues));
		break;
	case CMDQ_IOCTL_ALLOC_WRITE_ADDRESS:
		do {
			cmdqWriteAddressStruct addrReq;
			dma_addr_t paStart = 0;

			CMDQ_LOG("CMDQ_IOCTL_ALLOC_WRITE_ADDRESS\n");

			if (copy_from_user(&addrReq, (void *)param, sizeof(addrReq))) {
				CMDQ_ERR("CMDQ_IOCTL_ALLOC_WRITE_ADDRESS copy_from_user failed\n");
				return -EFAULT;
			}

			status = cmdqCoreAllocWriteAddress(addrReq.count, &paStart);
			if (0 != status) {
				CMDQ_ERR
				    ("CMDQ_IOCTL_ALLOC_WRITE_ADDRESS cmdqCoreAllocWriteAddress() failed\n");
				return status;
			}


			addrReq.startPA = (uint32_t) paStart;
			CMDQ_LOG("CMDQ_IOCTL_ALLOC_WRITE_ADDRESS get 0x%08x\n", addrReq.startPA);

			if (copy_to_user((void *)param, &addrReq, sizeof(addrReq))) {
				CMDQ_ERR("CMDQ_IOCTL_ALLOC_WRITE_ADDRESS copy_to_user failed\n");
				return -EFAULT;
			}
			status = 0;
		} while (0);
		break;
	case CMDQ_IOCTL_FREE_WRITE_ADDRESS:
		do {
			cmdqWriteAddressStruct freeReq;

			CMDQ_LOG("CMDQ_IOCTL_FREE_WRITE_ADDRESS\n");

			if (copy_from_user(&freeReq, (void *)param, sizeof(freeReq))) {
				CMDQ_ERR("CMDQ_IOCTL_FREE_WRITE_ADDRESS copy_from_user failed\n");
				return -EFAULT;
			}

			status = cmdqCoreFreeWriteAddress(freeReq.startPA);
			if (0 != status) {
				return status;
			}
			status = 0;
		} while (0);
		break;
	case CMDQ_IOCTL_READ_ADDRESS_VALUE:
		do {
			cmdqReadAddressStruct readReq;

			CMDQ_LOG("CMDQ_IOCTL_READ_ADDRESS_VALUE\n");

			if (copy_from_user(&readReq, (void *)param, sizeof(readReq))) {
				CMDQ_ERR("CMDQ_IOCTL_READ_ADDRESS_VALUE copy_from_user failed\n");
				return -EFAULT;
			}

			/* this will copy result to readReq->values buffer */
			cmdq_driver_process_read_address_request(&readReq);

			status = 0;

		} while (0);
		break;
	case CMDQ_IOCTL_QUERY_CAP_BITS:
		do {
			int capBits = 0;
			if (cmdq_core_support_wait_and_receive_event_in_same_tick()) {
				capBits |= (1L << CMDQ_CAP_WFE);
			} else {
				capBits &= ~(1L << CMDQ_CAP_WFE);
			}

			if (copy_to_user((void *)param, &capBits, sizeof(int))) {
				CMDQ_ERR("Copy capacity bits to user space failed\n");
				return -EFAULT;
			}
		} while (0);
		break;
	default:
		CMDQ_ERR("unrecognized ioctl 0x%08x\n", code);
		break;
	}

	return 0;
}

static long cmdq_ioctl_compat(struct file *pFile, unsigned int code, unsigned long param)
{
#ifdef CONFIG_COMPAT
    switch(code)
    {
    case CMDQ_IOCTL_QUERY_USAGE:
    case CMDQ_IOCTL_EXEC_COMMAND:
    case CMDQ_IOCTL_ASYNC_JOB_EXEC:
    case CMDQ_IOCTL_ASYNC_JOB_WAIT_AND_CLOSE:
    case CMDQ_IOCTL_ALLOC_WRITE_ADDRESS:
    case CMDQ_IOCTL_FREE_WRITE_ADDRESS:
    case CMDQ_IOCTL_READ_ADDRESS_VALUE:
    case CMDQ_IOCTL_QUERY_CAP_BITS:
        // All ioctl structures should be the same size in 32-bit and 64-bit linux.
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
#endif

    return -ENOIOCTLCMD;
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
	case PM_SUSPEND_PREPARE:/* Going to suspend the system */
		/* The next stage is freeze process. */
		/* We will queue all request in suspend callback, */
		/* so don't care this stage*/
		return NOTIFY_DONE; /* don't care this event */
	case PM_POST_SUSPEND:
		/* processes had resumed in previous stage (system resume callback) */
		/* resume CMDQ driver to execute. */
		cmdqCoreResumedNotifier();
		return NOTIFY_OK; /* process done */
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
				if (irqStatus & (1 << index)) {
					continue;
				}
				/* so we mark irqStatus to 1 to denote finished processing */
				/* and we can early-exit if no more threads being asserted */
				irqStatus |= (1 << index);

				cmdqCoreHandleIRQ(index);
				handled = true;
			}
		} else if (cmdq_dev_get_irq_secure_id() == IRQ){
			CMDQ_ERR("receive secure IRQ %d in NWD\n", IRQ);
		}
	} while (0);

	if (handled) {
		cmdq_core_add_consume_task();
		return IRQ_HANDLED;
	} else {
		/* allow CQ-DMA to process this IRQ bit */
		return IRQ_NONE;
	}
}

static int cmdq_create_debug_entries(void)
{
	struct proc_dir_entry *debugDirEntry = NULL;
	debugDirEntry = proc_mkdir(CMDQ_DRIVER_DEVICE_NAME "_debug", NULL);
	if (debugDirEntry) {
		struct proc_dir_entry *entry = NULL;
		entry = proc_create("status", 0440, debugDirEntry, &cmdqDebugStatusOp);
		entry = proc_create("error", 0440, debugDirEntry, &cmdqDebugErrorOp);
		entry = proc_create("record", 0440, debugDirEntry, &cmdqDebugRecordOp);
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
	if (status != 0) {
		CMDQ_ERR("Get CMDQ device major number(%d) failed(%d)\n", gCmdqDevNo, status);
	} else {
		CMDQ_MSG("Get CMDQ device major number(%d) success(%d)\n", gCmdqDevNo, status);
	}

	/* ioctl access point (/dev/mtk_cmdq) */
	gCmdqCDev = cdev_alloc();
	gCmdqCDev->owner = THIS_MODULE;
	gCmdqCDev->ops = &cmdqOP;

	status = cdev_add(gCmdqCDev, gCmdqDevNo, 1);

	gCMDQClass = class_create(THIS_MODULE, CMDQ_DRIVER_DEVICE_NAME);
	object = device_create(gCMDQClass, NULL, gCmdqDevNo, NULL, CMDQ_DRIVER_DEVICE_NAME);

	status =
	    request_irq(cmdq_dev_get_irq_id(), cmdq_irq_handler, IRQF_TRIGGER_LOW | IRQF_SHARED,
			CMDQ_DRIVER_DEVICE_NAME, gCmdqCDev);
	if (status != 0) {
		CMDQ_ERR("Register cmdq driver irq handler(%d) failed(%d)\n", gCmdqDevNo, status);
		return -EFAULT;
	}

	/* although secusre CMDQ driver is responsible for handle secure IRQ, */
	/* MUST registet secure IRQ to GIC in normal world to ensure it will be initialize correctly */
	/* (that's because t-base does not support GIC init IRQ in secure world...) */
#ifdef CMDQ_SECURE_PATH_SUPPORT	
	status =
	    request_irq(cmdq_dev_get_irq_secure_id(), cmdq_irq_handler, IRQF_TRIGGER_LOW,
			CMDQ_DRIVER_DEVICE_NAME, gCmdqCDev);
	CMDQ_MSG("register sec IRQ:%d\n", cmdq_dev_get_irq_secure_id());
	if (status != 0) {
		CMDQ_ERR("Register cmdq driver secure irq handler(%d) failed(%d)\n", gCmdqDevNo, status);
		return -EFAULT;
	}
#endif

	/* global ioctl access point (/proc/mtk_cmdq) */
	if (NULL == proc_create(CMDQ_DRIVER_DEVICE_NAME, 0644, NULL, &cmdqOP)) {
		CMDQ_ERR("CMDQ procfs node create failed\n");
		return -EFAULT;
	}

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

static struct dev_pm_ops cmdq_pm_ops = {
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

	/* Register MDP callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_MDP,
			   cmdqMdpClockOn, cmdqMdpDumpInfo, cmdqMdpResetEng, cmdqMdpClockOff);

	/* Register VENC callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_VENC, NULL, cmdqVEncDumpInfo, NULL, NULL);

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

	/* register pm notifier */
	status = unregister_pm_notifier(&cmdq_pm_notifier_block);
	if (0 != status) {
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

	CMDQ_MSG("CMDQ driver exit end\n");
}


subsys_initcall(cmdq_init);
module_exit(cmdq_exit);

MODULE_DESCRIPTION("MTK CMDQ driver");
MODULE_AUTHOR("Pablo<pablo.sun@mediatek.com>");
MODULE_LICENSE("GPL");
