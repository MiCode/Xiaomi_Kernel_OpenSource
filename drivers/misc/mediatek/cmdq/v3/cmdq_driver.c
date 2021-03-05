// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include "cmdq_driver.h"
#include "cmdq_struct.h"
#include "cmdq_virtual.h"
#include "cmdq_reg.h"
#include "cmdq_mdp_common.h"
#include "cmdq_device.h"

#include "cmdq_helper_ext.h"
#include "cmdq_record.h"
#include "cmdq_device.h"
#include "mdp_ioctl_ex.h"
#include "mdp_def_ex.h"

#ifdef CMDQ_SECURE_PATH_SUPPORT
#include "cmdq_sec.h"
#endif

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
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include <linux/sched/clock.h>
#ifdef CMDQ_USE_LEGACY
#include <mach/mt_boot.h>
#endif

/*
 * @device tree porting note
 * alps/kernel-3.10/arch/arm64/boot/dts/{platform}.dts
 *  - use of_device_id to match driver and device
 *  - use io_map to map and get VA of HW's rgister
 */
static const struct of_device_id cmdq_of_ids[] = {
	{.compatible = "mediatek,gce",},
	{}
};

static dev_t gCmdqDevNo;
static struct cdev *gCmdqCDev;
static struct class *gCMDQClass;

static ssize_t error_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return cmdq_core_print_error(dev, attr, buf);
}

static ssize_t error_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return -EACCES;
}

static DEVICE_ATTR_RW(error);

static ssize_t log_level_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return cmdq_core_write_log_level(dev, attr, buf, size);
}

static ssize_t log_level_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return cmdq_core_print_log_level(dev, attr, buf);
}

static DEVICE_ATTR_RW(log_level);

static ssize_t profile_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return cmdq_core_print_profile_enable(dev, attr, buf);
}

static ssize_t profile_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return cmdq_core_write_profile_enable(dev, attr, buf, size);
}

static DEVICE_ATTR_RW(profile_enable);


static int cmdq_proc_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdq_core_print_status_seq, inode->i_private);
}

static int cmdq_proc_record_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdq_core_print_record_seq, inode->i_private);
}

static const struct file_operations cmdqDebugStatusOp = {
	.owner = THIS_MODULE,
	.open = cmdq_proc_status_open,
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
static ssize_t instruction_count_level_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return cmdqCorePrintInstructionCountLevel(dev, attr, buf);
}

static ssize_t instruction_count_level_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return cmdqCoreWriteInstructionCountLevel(dev, attr, buf, size);
}

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

void cmdq_driver_dump_readback(u32 *ids, u32 *addrs, u32 count, u32 *values)
{}

static int cmdq_open(struct inode *pInode, struct file *pFile)
{
	struct cmdqFileNodeStruct *pNode;

	CMDQ_VERBOSE("CMDQ driver open fd=%p begin\n", pFile);

	pFile->private_data = kzalloc(sizeof(struct cmdqFileNodeStruct),
		GFP_KERNEL);
	if (!pFile->private_data) {
		CMDQ_ERR("Can't allocate memory for CMDQ file node\n");
		return -ENOMEM;
	}

	pNode = (struct cmdqFileNodeStruct *)pFile->private_data;
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

	pNode = (struct cmdqFileNodeStruct *)pFile->private_data;

	if (!pNode) {
		CMDQ_ERR("CMDQ file node NULL\n");
		return -EFAULT;
	}

	spin_lock_irqsave(&pNode->nodeLock, flags);

	/* note that we did not release CMDQ tasks
	 * issued by this file node,
	 * since their HW operation may be pending.
	 */

	spin_unlock_irqrestore(&pNode->nodeLock, flags);

	/* release by mapping job */
	mdp_ioctl_free_job_by_node(pNode);

	/* scan through tasks that created by
	 * this file node and release them
	 */
	cmdq_mdp_release_task_by_file_node((void *)pNode);

	kfree(pFile->private_data);
	pFile->private_data = NULL;

	CMDQ_VERBOSE("CMDQ driver release end\n");

	return 0;
}

static int cmdq_driver_create_reg_address_buffer(
	struct cmdqCommandStruct *pCommand)
{
	int status = 0;
	u32 totalRegCount = 0;
	u32 *regAddrBuf = NULL;

	u32 *kernelRegAddr = NULL;
	u32 kernelRegCount = 0;

	const u32 userRegCount = pCommand->regRequest.count;

	if (pCommand->debugRegDump != 0) {
		/* get kernel dump request count */
		status = cmdq_core_reg_dump_begin(pCommand->debugRegDump,
		    &kernelRegCount, &kernelRegAddr);
		if (status != 0) {
			CMDQ_ERR(
				"cmdq_core_reg_dump_begin returns %d ignore kernel reg dump request\n",
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

	if (!totalRegCount) {
		/* no need to dump register */
		pCommand->regRequest.count = 0;
		pCommand->regValue.count = 0;
		pCommand->regRequest.regAddresses = 0;
		pCommand->regValue.regValues = 0;
	} else {
		regAddrBuf = kcalloc(totalRegCount, sizeof(u32), GFP_KERNEL);
		if (!regAddrBuf)
			return -ENOMEM;

		/* collect user space dump request */
		if (userRegCount) {
			if (copy_from_user
			    (regAddrBuf, CMDQ_U32_PTR(
			    pCommand->regRequest.regAddresses),
			     userRegCount * sizeof(u32))) {
				kfree(regAddrBuf);
				return -EFAULT;
			}
		}

		/* collect kernel space dump request,
		 * concatnate after user space request
		 */
		if (kernelRegCount) {
			memcpy(regAddrBuf + userRegCount, kernelRegAddr,
			       kernelRegCount * sizeof(u32));
		}


		/* replace address buffer and value address buffer with
		 * kzalloc memory
		 */
		pCommand->regRequest.regAddresses =
			(cmdqU32Ptr_t)(unsigned long)regAddrBuf;
		pCommand->regRequest.count = totalRegCount;
	}

	return 0;
}

static void cmdq_driver_process_read_address_request(
	struct cmdqReadAddressStruct *req_user)
{
	/* create kernel-space buffer for working */
	u32 *addrs = NULL;
	u32 *values = NULL;
	void *dma_addr;
	void *values_addr;

	CMDQ_MSG("[READ_PA] %s\n", __func__);

	do {
		if (!req_user || !req_user->count ||
			req_user->count > CMDQ_MAX_DUMP_REG_COUNT) {
			CMDQ_ERR("[READ_PA] invalid req_user\n");
			break;
		}

		dma_addr = (void *)CMDQ_U32_PTR(req_user->dmaAddresses);
		values_addr = (void *)CMDQ_U32_PTR(req_user->values);
		if (!dma_addr || !values_addr) {
			CMDQ_ERR("[READ_PA] invalid in/out addr\n");
			break;
		}

		addrs = kcalloc(req_user->count, sizeof(u32), GFP_KERNEL);
		if (!addrs) {
			CMDQ_ERR("[READ_PA] fail to alloc addr buf\n");
			break;
		}

		values = kcalloc(req_user->count, sizeof(u32), GFP_KERNEL);
		if (!values) {
			CMDQ_ERR("[READ_PA] fail to alloc value buf\n");
			break;
		}

		/* copy from user */
		if (copy_from_user(addrs, dma_addr,
			req_user->count * sizeof(u32))) {
			CMDQ_ERR(
				"[READ_PA] fail to copy user dma addr:0x%p\n",
				dma_addr);
			break;
		}

		/* actually read these PA write buffers */
		cmdqCoreReadWriteAddressBatch(addrs, req_user->count, values);

		/* copy value to user */
		if (copy_to_user(values_addr, values,
			req_user->count * sizeof(u32))) {
			CMDQ_ERR("[READ_PA] fail to copy to user value buf\n");
			break;
		}
	} while (0);

	kfree(addrs);
	kfree(values);
}

#define CMDQ_PTR_FREE_NULL(ptr) \
do { \
	vfree(CMDQ_U32_PTR((ptr))); \
	(ptr) = 0; \
} while (0)


static long cmdq_driver_destroy_secure_medadata(
	struct cmdqCommandStruct *pCommand)
{
	u32 i;

	kfree(CMDQ_U32_PTR(pCommand->secData.addrMetadatas));
	pCommand->secData.addrMetadatas = 0;

	for (i = 0; i < ARRAY_SIZE(pCommand->secData.ispMeta.ispBufs); i++)
		CMDQ_PTR_FREE_NULL(pCommand->secData.ispMeta.ispBufs[i].va);

	return 0;
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
static s32 cmdq_driver_copy_meta(void *src, void **dest, size_t copy_size,
	size_t max_size, bool vm)
{
	void *meta_buf;

	if (!copy_size)
		return -EINVAL;

	if (copy_size > max_size) {
		CMDQ_ERR("source size exceed:%zu > %zu", copy_size, max_size);
		return -EFAULT;
	}

	if (vm)
		meta_buf = vzalloc(copy_size);
	else
		meta_buf = kzalloc(copy_size, GFP_KERNEL);
	if (!meta_buf) {
		CMDQ_ERR("allocate size fail:%zu\n", copy_size);
		return -ENOMEM;
	}
	*dest = meta_buf;

	if (copy_from_user(meta_buf, src, copy_size)) {
		CMDQ_ERR("fail to copy user data\n");
		return -EFAULT;
	}

	return 0;
}
#endif

static long cmdq_driver_create_secure_medadata(
	struct cmdqCommandStruct *pCommand)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	u32 length, max_length;
	void *meta_buf;
	s32 ret;
	void *addr_meta = CMDQ_U32_PTR(pCommand->secData.addrMetadatas);
	void *isp_bufs[ARRAY_SIZE(pCommand->secData.ispMeta.ispBufs)] = {0};
	u32 i;

	if (pCommand->secData.is_secure &&
		!pCommand->secData.addrMetadataCount)
		CMDQ_LOG("[warn]secure task without secure handle\n");

	if (pCommand->secData.addrMetadataCount >=
		CMDQ_IWC_MAX_ADDR_LIST_LENGTH) {
		CMDQ_ERR("Metadata %u reach the max allowed number = %u\n",
			 pCommand->secData.addrMetadataCount,
			 CMDQ_IWC_MAX_ADDR_LIST_LENGTH);
		return -EFAULT;
	}

	max_length = CMDQ_IWC_MAX_ADDR_LIST_LENGTH *
		sizeof(struct cmdqSecAddrMetadataStruct);
	length = pCommand->secData.addrMetadataCount *
		sizeof(struct cmdqSecAddrMetadataStruct);

	/* always clear to prevent free unknown memory */
	pCommand->secData.addrMetadatas = 0;
	for (i = 0; i < ARRAY_SIZE(pCommand->secData.ispMeta.ispBufs); i++) {
		isp_bufs[i] = (void *)pCommand->secData.ispMeta.ispBufs[i].va;
		pCommand->secData.ispMeta.ispBufs[i].va = 0;
	}

	/* verify parameter */
	if (!pCommand->secData.is_secure &&
		pCommand->secData.addrMetadataCount) {
		/* normal path with non-zero secure metadata */
		CMDQ_ERR(
			"[secData]mismatch is_secure %d and addrMetadataCount %d\n",
			pCommand->secData.is_secure,
			pCommand->secData.addrMetadataCount);
		return -EFAULT;
	}

	/* revise max count field */
	pCommand->secData.addrMetadataMaxCount =
		pCommand->secData.addrMetadataCount;

	/* bypass 0 metadata case */
	if (!pCommand->secData.addrMetadataCount)
		return 0;

	/* create kernel-space buffer for working */
	meta_buf = NULL;
	ret = cmdq_driver_copy_meta(addr_meta, &meta_buf, length, max_length,
		false);
	if (ret < 0) {
		CMDQ_ERR(
			"[secData]copy meta fail count:%d alloacted size:%d ret:%d\n",
			 pCommand->secData.addrMetadataCount, length, ret);
		/* replace buffer first to ensure that
		 * meta_buf is valid kernel space buffer address when free it
		 * crazy casting to cast 64bit int to 32/64 bit pointer
		 */
		pCommand->secData.addrMetadatas =
			(cmdqU32Ptr_t)(unsigned long)meta_buf;
		/* free secure path metadata */
		cmdq_driver_destroy_secure_medadata(pCommand);
		return ret;
	}
	/* replace buffer with kernel buffer */
	pCommand->secData.addrMetadatas = (cmdqU32Ptr_t)(unsigned long)meta_buf;

	/* check isp data valid */

	for (i = 0; i < ARRAY_SIZE(pCommand->secData.ispMeta.ispBufs); i++) {
		if (!isp_bufs[i])
			continue;
		meta_buf = NULL;
		ret = cmdq_driver_copy_meta(isp_bufs[i], &meta_buf,
			pCommand->secData.ispMeta.ispBufs[i].size,
			isp_iwc_buf_size[i], true);
		pCommand->secData.ispMeta.ispBufs[i].va =
			(cmdqU32Ptr_t)(unsigned long)meta_buf;
		if (ret < 0) {
			CMDQ_ERR(
				"[secData]copy meta %u size:%llu va:0x%llx ret:%d\n",
				i, pCommand->secData.ispMeta.ispBufs[i].size,
				pCommand->secData.ispMeta.ispBufs[i].va,
				ret);
			pCommand->secData.ispMeta.ispBufs[i].size = 0;
		}
	}

#endif
	return 0;
}

static long cmdq_driver_process_command_request(
	struct cmdqCommandStruct *pCommand)
{
	s32 status = 0;
	u32 *userRegValue = NULL;
	u32 userRegCount = 0;

	if (pCommand->regRequest.count > CMDQ_MAX_DUMP_REG_COUNT) {
		CMDQ_ERR("reg request count too much:%u\n",
			pCommand->regRequest.count);
		return -EFAULT;
	}


	if (pCommand->regValue.count > CMDQ_MAX_DUMP_REG_COUNT) {
		CMDQ_ERR("reg value count too much:%u\n",
			pCommand->regValue.count);
		return -EFAULT;
	}

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
	    kzalloc(pCommand->regRequest.count * sizeof(u32), GFP_KERNEL);
	pCommand->regValue.count = pCommand->regRequest.count;
	if (CMDQ_U32_PTR(pCommand->regValue.regValues) == NULL) {
		kfree(CMDQ_U32_PTR(pCommand->regRequest.regAddresses));
		return -ENOMEM;
	}

	/* scenario id fixup */
	cmdq_mdp_fix_command_scenario_for_user_space(pCommand);

	status = cmdq_mdp_flush(pCommand, true);
	if (status < 0) {
		CMDQ_ERR("flush user commands for execution failed:%d\n",
			status);
		cmdq_driver_destroy_secure_medadata(pCommand);

		kfree(CMDQ_U32_PTR(pCommand->regRequest.regAddresses));
		kfree(CMDQ_U32_PTR(pCommand->regValue.regValues));
		return status;
	}

	/* notify kernel space dump callback */
	if (pCommand->debugRegDump) {
		status = cmdq_core_reg_dump_end(pCommand->debugRegDump,
			pCommand->regRequest.count - userRegCount,
			CMDQ_U32_PTR(pCommand->regValue.regValues) +
			userRegCount);
		if (status != 0) {
			/* Error status print */
			CMDQ_ERR("cmdq_core_reg_dump_end returns %d\n",
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
		     userRegCount * sizeof(u32))) {
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

static s32 cmdq_driver_copy_handle_prop_from_user(void *from, u32 size,
	void **to)
{
	void *task_prop = NULL;

	/* considering backward compatible,
	 * we won't return error when argument not available
	 */
	if (from && size && to) {
		task_prop = kzalloc(size, GFP_KERNEL);
		if (!task_prop) {
			CMDQ_ERR("allocate task_prop failed\n");
			return -ENOMEM;
		}

		if (copy_from_user(task_prop, from, size)) {
			CMDQ_ERR(
				"cannot copy task property from user, size=%d\n",
				size);
			kfree(task_prop);
			return -EFAULT;
		}

		*to = task_prop;
	}

	return 0;
}

static void cmdq_release_handle_property(void **prop_addr, u32 *prop_size)
{
	if (!prop_addr || !prop_size)
		return;

	kfree(*prop_addr);
	*prop_addr = NULL;
	*prop_size = 0;
}

s32 cmdq_driver_ioctl_exec_command(struct file *pf, unsigned long param)
{
	struct cmdqCommandStruct command;
	struct task_private desc_private = {0};
	s32 status;

	if (copy_from_user(&command, (void *)param,
		sizeof(struct cmdqCommandStruct)))
		return -EFAULT;

	if (command.regRequest.count > CMDQ_MAX_DUMP_REG_COUNT ||
		!command.blockSize ||
		command.blockSize > CMDQ_MAX_COMMAND_SIZE ||
		command.prop_size > CMDQ_MAX_USER_PROP_SIZE) {
		CMDQ_ERR(
			"invalid input reg count:%u block size:%u prop size:%u\n",
			command.regRequest.count,
			command.blockSize, command.prop_size);
		return -EINVAL;
	}

	status = cmdq_driver_copy_handle_prop_from_user(
		(void *)CMDQ_U32_PTR(command.prop_addr), command.prop_size,
		(void *)CMDQ_U32_PTR(&command.prop_addr));
	if (status < 0) {
		CMDQ_ERR("copy prop_addr failed, err=%d\n", status);
		return status;
	}

	/* insert private_data for resource reclaim */
	desc_private.node_private_data = pf->private_data;
	command.privateData = (cmdqU32Ptr_t)(unsigned long)&desc_private;

	status = cmdq_driver_process_command_request(&command);

	cmdq_release_handle_property(
		(void *)CMDQ_U32_PTR(&command.prop_addr), &command.prop_size);

	if (status < 0)
		return -EFAULT;

	return 0;
}

s32 cmdq_driver_ioctl_query_usage(struct file *pf, unsigned long param)
{
	int count[CMDQ_MAX_ENGINE_COUNT] = {0};

	if (cmdq_mdp_query_usage(count))
		return -EFAULT;

	if (copy_to_user((void *)param, count, sizeof(s32) *
		CMDQ_MAX_ENGINE_COUNT)) {
		CMDQ_ERR("CMDQ_IOCTL_QUERY_USAGE copy_to_user failed\n");
		return -EFAULT;
	}

	return 0;
}

s32 cmdq_driver_ioctl_async_job_exec(struct file *pf,
	unsigned long param)
{
	struct cmdqJobStruct job;
	struct task_private desc_private = {0};
	struct cmdqRecStruct *handle = NULL;
	u32 userRegCount;
	s32 status;

	if (copy_from_user(&job, (void *)param, sizeof(job))) {
		CMDQ_ERR("copy job from user fail\n");
		return -EFAULT;
	}

	if (job.command.regRequest.count > CMDQ_MAX_DUMP_REG_COUNT ||
		!job.command.blockSize ||
		job.command.blockSize > CMDQ_MAX_COMMAND_SIZE ||
		job.command.prop_size > CMDQ_MAX_USER_PROP_SIZE) {
		CMDQ_ERR(
			"invalid input reg count:%u block size:%u prop size:%u\n",
			job.command.regRequest.count,
			job.command.blockSize, job.command.prop_size);
		return -EINVAL;
	}

	/* backup */
	userRegCount = job.command.regRequest.count;

	/* insert private_data for resource reclaim */
	desc_private.node_private_data = pf->private_data;
	job.command.privateData = (cmdqU32Ptr_t)(unsigned long)&desc_private;

	/* create kernel-space address buffer */
	status = cmdq_driver_create_reg_address_buffer(&job.command);
	if (status != 0) {
		CMDQ_ERR("create reg buffer fail:%d\n", status);
		return status;
	}

	/* avoid copy large string */
	if (job.command.userDebugStrLen > CMDQ_MAX_DBG_STR_LEN)
		job.command.userDebugStrLen = CMDQ_MAX_DBG_STR_LEN;

	/* scenario id fixup */
	cmdq_mdp_fix_command_scenario_for_user_space(&job.command);

	/* allocate secure medatata */
	status = cmdq_driver_create_secure_medadata(&job.command);
	if (status != 0) {
		CMDQ_ERR("create secure meta fail:%d\n", status);
		return status;
	}

	status = cmdq_driver_copy_handle_prop_from_user(
			(void *)CMDQ_U32_PTR(job.command.prop_addr),
			job.command.prop_size,
			(void *)CMDQ_U32_PTR(&job.command.prop_addr));

	if (status < 0) {
		CMDQ_ERR("copy prop_addr failed, err=status\n");
		return status;
	}

	status = cmdq_mdp_flush_async(&job.command, true, &handle);

	cmdq_release_handle_property(
		(void *)CMDQ_U32_PTR(&job.command.prop_addr),
		&job.command.prop_size);

	if (status < 0) {
		CMDQ_ERR(
			"CMDQ_IOCTL_ASYNC_JOB_EXEC flush task fail status:%d\n",
			status);
		if (handle) {
			if (handle->thread != CMDQ_INVALID_THREAD)
				cmdq_mdp_unlock_thread(handle);
			cmdq_task_destroy(handle);
		}

		return status;
	}

	/* store user space request count for later retrieval */
	handle->user_reg_count = userRegCount;
	handle->user_token = job.command.debugRegDump;

	/* we don't need regAddress anymore, free it now */
	kfree(CMDQ_U32_PTR(job.command.regRequest.regAddresses));
	job.command.regRequest.regAddresses = 0;

	/* free secure path metadata */
	cmdq_driver_destroy_secure_medadata(&job.command);

	job.hJob = (unsigned long)handle;
	if (copy_to_user((void *)param, (void *)&job, sizeof(job))) {
		CMDQ_ERR("CMDQ_IOCTL_ASYNC_JOB_EXEC copy_to_user failed\n");
		return -EFAULT;
	}

	return 0;
}

s32 cmdq_driver_ioctl_async_job_wait_and_close(unsigned long param)
{
	struct cmdqJobResultStruct jobResult;
	struct cmdqRecStruct *handle;
	u32 *userRegValue = NULL;
	/* backup value after task release */
	s32 status;
	u64 exec_cost = sched_clock();

	if (copy_from_user(&jobResult, (void *)param, sizeof(jobResult))) {
		CMDQ_ERR("copy_from_user jobResult fail\n");
		return -EFAULT;
	}

	/* verify job handle */
	handle = cmdq_mdp_get_valid_handle((unsigned long)jobResult.hJob);
	if (!handle) {
		CMDQ_ERR("job does not exists:0x%016llx\n", jobResult.hJob);
		return -EFAULT;
	}

	if (handle->reg_count > CMDQ_MAX_DUMP_REG_COUNT) {
		CMDQ_ERR("reg count overflow:%u\n", handle->reg_count);
		return -EINVAL;
	}

	CMDQ_VERBOSE("async job wait with handle:0x%p\n", handle);

	/* utility service, fill the engine flag. this is required by MDP. */
	jobResult.engineFlag = handle->engineFlag;

	/* check if reg buffer suffices */
	if (jobResult.regValue.count < handle->user_reg_count) {
		CMDQ_ERR("handle:0x%p insufficient register buffer %u < %u\n",
			handle, jobResult.regValue.count,
			handle->user_reg_count);
		jobResult.regValue.count = handle->user_reg_count;
		if (copy_to_user((void *)param, (void *)&jobResult,
			sizeof(jobResult))) {
			CMDQ_ERR("copy_to_user fail, line:%d\n", __LINE__);
			return -EINVAL;
		}
		return -ENOMEM;
	}

	/* inform client the actual read register count */
	jobResult.regValue.count = handle->user_reg_count;
	/* update user space before replace the regValues pointer. */
	if (copy_to_user((void *)param, (void *)&jobResult,
		sizeof(jobResult))) {
		CMDQ_ERR("copy_to_user fail line:%d\n", __LINE__);
		return -EINVAL;
	}

	/* allocate kernel space result buffer
	 * which contains kernel + user space requests
	 */
	userRegValue = CMDQ_U32_PTR(jobResult.regValue.regValues);
	jobResult.regValue.regValues = (cmdqU32Ptr_t)(unsigned long)(
		kzalloc(handle->reg_count + sizeof(u32), GFP_KERNEL));
	jobResult.regValue.count = handle->reg_count;
	if (CMDQ_U32_PTR(jobResult.regValue.regValues) == NULL) {
		CMDQ_ERR("no reg value buffer\n");
		return -ENOMEM;
	}

	/* wait for task done */
	status = cmdq_mdp_wait(handle, &jobResult.regValue);
	if (status < 0) {
		CMDQ_ERR("wait task result failed:%d handle:0x%p\n",
			status, handle);
		cmdq_task_destroy(handle);
		kfree(CMDQ_U32_PTR(jobResult.regValue.regValues));
		return status;
	}

	/* notify kernel space dump callback */
	if (handle->reg_count > handle->user_reg_count) {
		CMDQ_VERBOSE("kernel space reg dump = %d, %d, %d\n",
			handle->reg_count, handle->user_reg_count,
			handle->user_token);
		status = cmdq_core_reg_dump_end(handle->user_token,
			handle->reg_count - handle->user_reg_count,
			CMDQ_U32_PTR(jobResult.regValue.regValues +
			handle->user_reg_count));
		if (status != 0) {
			/* Error status print */
			CMDQ_ERR("cmdq_core_reg_dump_end returns %d\n",
				status);
		}
	}

	/* copy result to user space */
	if (copy_to_user((void *)userRegValue, (void *)(unsigned long)
		handle->reg_values, handle->user_reg_count * sizeof(u32))) {
		CMDQ_ERR("Copy REGVALUE to user space failed\n");
		kfree(CMDQ_U32_PTR(jobResult.regValue.regValues));
		return -EFAULT;
	}

	if (jobResult.readAddress.count > 0)
		cmdq_driver_process_read_address_request(
			&jobResult.readAddress);

	/* free kernel space result buffer */
	kfree(CMDQ_U32_PTR(jobResult.regValue.regValues));

	exec_cost = div_s64(sched_clock() - exec_cost, 1000);
	if (exec_cost > 150000)
		CMDQ_LOG("[warn]job wait and close cost:%lluus handle:0x%p\n",
			exec_cost, handle);

	/* task now can release */
	cmdq_task_destroy(handle);

	return 0;
}

s32 cmdq_driver_ioctl_alloc_write_address(void *fp, unsigned long param)
{
	struct cmdqWriteAddressStruct addrReq;
	dma_addr_t paStart = 0;
	s32 status;

	if (copy_from_user(&addrReq, (void *)param, sizeof(addrReq))) {
		CMDQ_ERR("%s copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	status = cmdqCoreAllocWriteAddress(addrReq.count, &paStart,
		CMDQ_CLT_MDP);
	if (status != 0) {
		CMDQ_ERR("%s alloc write address failed\n", __func__);
		return status;
	}

	addrReq.startPA = (u32)paStart;
	CMDQ_MSG("%s get 0x%08x\n", __func__, addrReq.startPA);

	if (copy_to_user((void *)param, &addrReq, sizeof(addrReq))) {
		CMDQ_ERR("%s copy_to_user failed\n", __func__);
		return -EFAULT;
	}

	return 0;
}

s32 cmdq_driver_ioctl_free_write_address(unsigned long param)
{
	struct cmdqWriteAddressStruct freeReq;

	CMDQ_MSG("%s\n", __func__);

	if (copy_from_user(&freeReq, (void *)param, sizeof(freeReq))) {
		CMDQ_ERR("%s copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	return cmdqCoreFreeWriteAddress(freeReq.startPA, CMDQ_CLT_MDP);
}

s32 cmdq_driver_ioctl_read_address_value(unsigned long param)
{
	struct cmdqReadAddressStruct readReq;

	CMDQ_MSG("%s\n", __func__);

	if (copy_from_user(&readReq, (void *)param, sizeof(readReq))) {
		CMDQ_ERR("%s copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	/* this will copy result to readReq->values buffer */
	cmdq_driver_process_read_address_request(&readReq);

	return 0;
}

s32 cmdq_driver_ioctl_query_cap_bits(unsigned long param)
{
	int capBits = 0;

	/* support wait and receive event in same tick */
	capBits |= (1L << CMDQ_CAP_WFE);

	if (copy_to_user((void *)param, &capBits, sizeof(int))) {
		CMDQ_ERR("Copy capacity bits to user space failed\n");
		return -EFAULT;
	}

	return 0;
}

s32 cmdq_driver_ioctl_query_dts(unsigned long param)
{
	struct cmdqDTSDataStruct *dts;

	dts = cmdq_core_get_dts_data();

	if (copy_to_user((void *)param, dts, sizeof(*dts))) {
		CMDQ_ERR("Copy dts to user space failed\n");
		return -EFAULT;
	}

	return 0;
}

s32 cmdq_driver_ioctl_notify_engine(unsigned long param)
{
	u64 engineFlag;

	if (copy_from_user(&engineFlag, (void *)param, sizeof(u64))) {
		CMDQ_ERR("%s copy_from_user failed\n", __func__);
		return -EFAULT;
	}
	cmdq_mdp_lock_resource(engineFlag, true);

	return 0;
}

static long cmdq_ioctl(struct file *pf, unsigned int code,
	unsigned long param)
{
	s32 status = 0;

	CMDQ_VERBOSE("%s code:0x%08x f:0x%p\n", __func__, code, pf);

	switch (code) {
#if 0
	case CMDQ_IOCTL_EXEC_COMMAND:
		status = cmdq_driver_ioctl_exec_command(pf, param);
		break;
#endif
	case CMDQ_IOCTL_QUERY_USAGE:
		status = cmdq_driver_ioctl_query_usage(pf, param);
		break;
#if 0
	case CMDQ_IOCTL_ASYNC_JOB_EXEC:
		CMDQ_SYSTRACE_BEGIN("%s_async_job_exec\n", __func__);
		status = cmdq_driver_ioctl_async_job_exec(pf, param);
		CMDQ_SYSTRACE_END();
		break;
	case CMDQ_IOCTL_ASYNC_JOB_WAIT_AND_CLOSE:
		CMDQ_SYSTRACE_BEGIN("%s_async_job_wait_and_close\n", __func__);
		status = cmdq_driver_ioctl_async_job_wait_and_close(param);
		CMDQ_SYSTRACE_END();
		break;
	case CMDQ_IOCTL_ALLOC_WRITE_ADDRESS:
		status = cmdq_driver_ioctl_alloc_write_address(pf, param);
		break;
	case CMDQ_IOCTL_FREE_WRITE_ADDRESS:
		status = cmdq_driver_ioctl_free_write_address(param);
		break;
	case CMDQ_IOCTL_READ_ADDRESS_VALUE:
		status = cmdq_driver_ioctl_read_address_value(param);
		break;
#endif
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
		status = mdp_ioctl_async_exec(pf, param);
		break;
	case CMDQ_IOCTL_ASYNC_WAIT:
		CMDQ_MSG("ioctl CMDQ_IOCTL_ASYNC_WAIT\n");
		status = mdp_ioctl_async_wait(param);
		break;
	case CMDQ_IOCTL_ALLOC_READBACK_SLOTS:
		CMDQ_MSG("ioctl CMDQ_IOCTL_ALLOC_READBACK_SLOTS\n");
		status = mdp_ioctl_alloc_readback_slots(pf, param);
		break;
	case CMDQ_IOCTL_FREE_READBACK_SLOTS:
		CMDQ_MSG("ioctl CMDQ_IOCTL_FREE_READBACK_SLOTS\n");
		status = mdp_ioctl_free_readback_slots(pf, param);
		break;
	case CMDQ_IOCTL_READ_READBACK_SLOTS:
		CMDQ_MSG("ioctl CMDQ_IOCTL_READ_READBACK_SLOTS\n");
		status = mdp_ioctl_read_readback_slots(param);
		break;
	default:
		CMDQ_ERR("unrecognized ioctl 0x%08x\n", code);
		return -ENOIOCTLCMD;
	}

	if (status < 0)
		CMDQ_ERR("ioctl return fail:%d\n", status);

	return status;
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
		/* All ioctl structures should be the same size in
		 * 32-bit and 64-bit linux.
		 */
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
		/* processes had resumed in previous stage
		 * (system resume callback)
		 * resume CMDQ driver to execute.
		 */
		cmdq_core_resume_notifier();
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


static int cmdq_create_debug_entries(void)
{
	struct proc_dir_entry *debugDirEntry = NULL;

	debugDirEntry = proc_mkdir(CMDQ_DRIVER_DEVICE_NAME "_debug", NULL);
	if (debugDirEntry) {
		struct proc_dir_entry *entry = NULL;

		entry = proc_create("status", 0440, debugDirEntry,
			&cmdqDebugStatusOp);
		entry = proc_create("record", 0440, debugDirEntry,
			&cmdqDebugRecordOp);
#ifdef CMDQ_INSTRUCTION_COUNT
		entry = proc_create("instructionCount", 0440, debugDirEntry,
			&cmdqDebugInstructionCountOp);
#endif
	}

	return 0;
}

static int cmdq_probe(struct platform_device *pDevice)
{
	int status;
	struct device *object;

	CMDQ_LOG("CMDQ driver probe begin\n");

	/* Function link */
	cmdq_virtual_function_setting();

	/* init cmdq device related data */
	cmdq_dev_init(pDevice);

	/* init cmdq context */
	cmdq_core_initialize();

	/* init cmdq context */
	cmdq_mdp_init();

	/* register mdp power domain control. */
	pm_runtime_enable(&pDevice->dev);

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

	/* mtk-cmdq-mailbox will register the irq */

	/* proc debug access point */
	cmdq_create_debug_entries();

	/* device attributes for debugging */
	status = device_create_file(&pDevice->dev, &dev_attr_error);
	if (status != 0)
		CMDQ_ERR("%s attr error create fail\n", __func__);
	status = device_create_file(&pDevice->dev, &dev_attr_log_level);
	if (status != 0)
		CMDQ_ERR("%s attr log level create fail\n", __func__);
	status = device_create_file(&pDevice->dev, &dev_attr_profile_enable);
	if (status != 0)
		CMDQ_ERR("%s attr profile create fail\n", __func__);
#ifdef CMDQ_INSTRUCTION_COUNT
	status = device_create_file(&pDevice->dev,
		&dev_attr_instruction_count_level);
	if (status != 0)
		CMDQ_ERR("%s attr inst count create fail\n", __func__);
#endif

	mdp_limit_dev_create(pDevice);
	CMDQ_LOG("CMDQ driver probe end\n");

	return 0;
}


static int cmdq_remove(struct platform_device *pDevice)
{
	/* unregister mdp power domain control. */
	pm_runtime_disable(&pDevice->dev);
	disable_irq(cmdq_dev_get_irq_id());

	device_remove_file(&pDevice->dev, &dev_attr_error);
	device_remove_file(&pDevice->dev, &dev_attr_log_level);
	device_remove_file(&pDevice->dev, &dev_attr_profile_enable);
#ifdef CMDQ_INSTRUCTION_COUNT
	device_remove_file(&pDevice->dev, &dev_attr_instruction_count_level);
#endif
	return 0;
}


static int cmdq_suspend(struct device *pDevice)
{
	CMDQ_LOG("%s ignore\n", __func__);
	return cmdq_core_suspend();
}

static int cmdq_resume(struct device *pDevice)
{
	CMDQ_LOG("%s ignore\n", __func__);
	return cmdq_core_resume();
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
		.of_match_table = cmdq_of_ids,
	}
};

static int __init mdp_late_init(void)
{
	int status;

	CMDQ_LOG("%s begin\n", __func__);
	status = mdp_limit_late_init();
	CMDQ_LOG("%s end\n", __func__);

	return 0;
}
late_initcall(mdp_late_init);

static int __init cmdq_init(void)
{
	int status;
	struct cmdqMDPFuncStruct *mdp_func = cmdq_mdp_get_func();

	CMDQ_LOG("%s CMDQ driver init begin\n", __func__);

	/* MDP function link */
	cmdq_mdp_virtual_function_setting();
	cmdq_mdp_platform_function_setting();

	/* Register VENC callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_VENC, NULL, mdp_func->vEncDumpInfo,
		NULL, NULL);

	/* Register PMQoS */
	cmdq_core_register_task_cycle_cb(CMDQ_GROUP_MDP,
			cmdq_mdp_get_func()->beginTask,
			cmdq_mdp_get_func()->endTask);

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

	CMDQ_LOG("CMDQ driver init end\n");

	return 0;
}

static void __exit cmdq_exit(void)
{
	s32 status;

	CMDQ_LOG("CMDQ driver exit begin\n");

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
	cmdq_core_deinit_group_cb();

	/* De-Initialize cmdq core */
	cmdq_core_deinitialize();

	/* De-Initialize cmdq dev related data */
	cmdq_dev_deinit();
	mdp_limit_dev_destroy();

	CMDQ_LOG("CMDQ driver exit end\n");
}

subsys_initcall(cmdq_init);
module_exit(cmdq_exit);

MODULE_DESCRIPTION("MTK CMDQ driver");
MODULE_AUTHOR("Pablo<pablo.sun@mediatek.com>");
MODULE_LICENSE("GPL");
