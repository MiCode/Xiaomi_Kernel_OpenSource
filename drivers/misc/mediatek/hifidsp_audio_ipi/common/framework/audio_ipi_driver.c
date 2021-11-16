/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by kmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/io.h>

#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
#include <adsp_helper.h>
#include <adsp_ipi.h>
#endif

#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
#include <adsp_ipi.h>
#include "audio_memory.h"
#endif

#include "audio_log.h"
#include "audio_assert.h"

#include "audio_task_manager.h"

#include <audio_ipi_dma.h>
#include <audio_ipi_platform.h>

#include <adsp_ipi_queue.h>


#ifdef CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT
#include <audio_ipi_client_phone_call.h>
#endif


#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
#include "audio_ipi_client_playback.h"
#endif



/*
 * =============================================================================
 *                     log
 * =============================================================================
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[IPI][DRV] %s(), " fmt "\n", __func__



/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */

#define AUDIO_IPI_DEVICE_NAME "audio_ipi"
#define AUDIO_IPI_IOC_MAGIC 'i'

#define AUDIO_IPI_IOCTL_SEND_MSG_ONLY _IOW(AUDIO_IPI_IOC_MAGIC, 0, unsigned int)
#define AUDIO_IPI_IOCTL_SEND_PAYLOAD  _IOW(AUDIO_IPI_IOC_MAGIC, 1, unsigned int)
#define AUDIO_IPI_IOCTL_SEND_DRAM     _IOW(AUDIO_IPI_IOC_MAGIC, 2, unsigned int)

#define AUDIO_IPI_IOCTL_LOAD_SCENE   _IOW(AUDIO_IPI_IOC_MAGIC, 10, unsigned int)

#define AUDIO_IPI_IOCTL_INIT_DSP     _IOW(AUDIO_IPI_IOC_MAGIC, 20, unsigned int)
#define AUDIO_IPI_IOCTL_REG_DMA      _IOW(AUDIO_IPI_IOC_MAGIC, 21, unsigned int)
#define AUDIO_IPI_IOCTL_ADSP_REG_FEA _IOW(AUDIO_IPI_IOC_MAGIC, 22, unsigned int)



/*
 * =============================================================================
 *                     struct
 * =============================================================================
 */

#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
struct audio_ipi_reg_dma_t {
	uint8_t task;
	uint8_t reg_flag; /* 1: register, 0: unregister */
	uint16_t __reserved;

	uint32_t a2d_size;
	uint32_t d2a_size;
};

struct audio_ipi_reg_feature_t {
	uint16_t reg_flag;
	uint16_t feature_id;
};
#endif


/*
 * =============================================================================
 *                     implementation
 * =============================================================================
 */

inline uint32_t msg_len_of_type(const uint8_t data_type)
{
	uint32_t msg_len = 0;

	switch (data_type) {
	case AUDIO_IPI_MSG_ONLY:
		msg_len = IPI_MSG_HEADER_SIZE;
		break;
	case AUDIO_IPI_PAYLOAD:
		msg_len = MAX_IPI_MSG_BUF_SIZE;
		break;
	case AUDIO_IPI_DMA:
		msg_len = IPI_MSG_HEADER_SIZE + IPI_MSG_DMA_INFO_SIZE;
		break;
	default:
		pr_info("%d not support!!", data_type);
		msg_len = IPI_MSG_HEADER_SIZE;
	}

	return msg_len;
}


static int parsing_ipi_msg_from_user_space(
	void __user *user_data_ptr,
	uint8_t data_type)
{
	struct ipi_msg_t ipi_msg;
	struct ipi_msg_dma_info_t *dma_info = NULL;
	struct aud_data_t *wb_dram = NULL;

	uint32_t msg_len = 0;

	uint32_t hal_data_size = 0;
	void *copy_hal_data = NULL;

	uint32_t hal_wb_buf_size = 0;
	void __user *hal_wb_buf_addr = NULL;

	struct aud_ptr_t dram_buf;


	int retval = 0;


	/* init var */
	memset(&dram_buf, 0, sizeof(struct aud_ptr_t));


	/* get message size to read */
	msg_len = msg_len_of_type(data_type);
	if (msg_len > sizeof(struct ipi_msg_t))  {
		pr_notice("msg_len %u > %zu!!",
			  msg_len, sizeof(struct ipi_msg_t));
		retval = -1;
		goto parsing_exit;
	}

	memset(&ipi_msg, 0, sizeof(struct ipi_msg_t));
	retval = copy_from_user(&ipi_msg, user_data_ptr, msg_len);
	if (retval != 0) {
		pr_notice("msg copy_from_user retval %d", retval);
		goto parsing_exit;
	}
	if (ipi_msg.data_type != data_type) { /* double check */
		pr_notice("data_type %d != %d", ipi_msg.data_type, data_type);
		retval = -1;
		goto parsing_exit;
	}
	msg_len = get_message_buf_size(&ipi_msg);
	retval = check_msg_format(&ipi_msg, msg_len);
	if (retval != 0)
		goto parsing_exit;


	/* get dma buf if need */
	dma_info = &ipi_msg.dma_info;
	wb_dram = &dma_info->wb_dram;

	if (data_type == AUDIO_IPI_DMA) {
		/* get hal data & write hal data to DRAM */
		hal_data_size = dma_info->hal_buf.data_size;
		copy_hal_data = kmalloc(hal_data_size, GFP_KERNEL);
		if (copy_hal_data == NULL) {
			retval = -ENOMEM;
			goto parsing_exit;
		}
		retval = copy_from_user(
				 copy_hal_data,
				 (void __user *)dma_info->hal_buf.addr,
				 hal_data_size);
		if (retval != 0) {
			pr_notice("dma copy_from_user retval %d", retval);
			goto parsing_exit;
		}

		dma_info->data_size = hal_data_size;
		retval = audio_ipi_dma_write_region(
				 ipi_msg.task_scene,
				 copy_hal_data,
				 hal_data_size,
				 &dma_info->rw_idx);
		if (retval != 0) {
			pr_notice("dma write region error!!");
			goto parsing_exit;
		}


		/* write back result to hal later, like get parameter */
		hal_wb_buf_size = dma_info->hal_buf.memory_size;
		hal_wb_buf_addr = (void __user *)dma_info->hal_buf.addr;
		if (hal_wb_buf_size != 0 && hal_wb_buf_addr != NULL) {
			/* alloc a dma for wb */
			audio_ipi_dma_alloc(&dram_buf, hal_wb_buf_size);

			wb_dram->memory_size = hal_wb_buf_size;
			wb_dram->data_size = 0;
			wb_dram->addr_val = dram_buf.addr_val;

			/* force need ack to get scp info */
			if (ipi_msg.ack_type != AUDIO_IPI_MSG_NEED_ACK) {
				pr_notice("task %d msg 0x%x need ack!!",
					  ipi_msg.task_scene, ipi_msg.msg_id);
				ipi_msg.ack_type = AUDIO_IPI_MSG_NEED_ACK;
			}
		}
#if 0 /* debug only */
		DUMP_IPI_MSG("dma", &ipi_msg);
#endif
	}

	/* sent message */
	retval = audio_send_ipi_filled_msg(&ipi_msg);
	if (retval != 0) {
		pr_notice("audio_send_ipi_filled_msg error!!");
		goto parsing_exit;
	}


	/* write back data to hal */
	if (data_type == AUDIO_IPI_DMA &&
	    hal_wb_buf_size != 0 &&
	    hal_wb_buf_addr != NULL &&
	    wb_dram != NULL &&
	    wb_dram->addr_val != 0 &&
	    ipi_msg.scp_ret == 1) {
		if (wb_dram->data_size > hal_wb_buf_size) {
			pr_notice("wb_dram->data_size %u > hal_wb_buf_size %u!!",
				  wb_dram->data_size,
				  hal_wb_buf_size);
			ipi_msg.scp_ret = 0;
		} else if (wb_dram->data_size == 0) {
			pr_notice("ipi wb data sz = 0!! check adsp write");
			ipi_msg.scp_ret = 0;
		} else {
			retval = copy_to_user(
					 hal_wb_buf_addr,
					 get_audio_ipi_dma_vir_addr(
						 wb_dram->addr_val),
					 wb_dram->data_size);
			if (retval) {
				pr_info("copy_to_user dma err, id = 0x%x",
					ipi_msg.msg_id);
				ipi_msg.scp_ret = 0;
			}
		}
	}


	/* write back ipi msg to hal */
	if (data_type == AUDIO_IPI_DMA) /* clear sensitive addr info */
		memset(&ipi_msg.dma_info, 0, IPI_MSG_DMA_INFO_SIZE);

	retval = copy_to_user(user_data_ptr,
			      &ipi_msg,
			      sizeof(struct ipi_msg_t));
	if (retval) {
		pr_info("copy_to_user err, id = 0x%x", ipi_msg.msg_id);
		retval = -EFAULT;
	}


parsing_exit:
	if (copy_hal_data != NULL) {
		kfree(copy_hal_data);
		copy_hal_data = NULL;
	}
	if (dram_buf.addr_val != 0)
		audio_ipi_dma_free(&dram_buf, hal_wb_buf_size);


	return retval;
}


/* trigger scp driver to dump scp_log to sdcard */
/*extern void scp_get_log(int save);*/ /* TODO: ask scp driver to add in .h */


static long audio_ipi_driver_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
	struct audio_ipi_reg_dma_t dma_reg;
	struct audio_ipi_reg_feature_t feat_reg;
#endif
	int retval = 0;

	AUD_LOG_V("cmd = %u, arg = %lu", cmd, arg);

	switch (cmd) {
	case AUDIO_IPI_IOCTL_SEND_MSG_ONLY: {
		retval = parsing_ipi_msg_from_user_space(
				 (void __user *)arg, AUDIO_IPI_MSG_ONLY);
		break;
	}
	case AUDIO_IPI_IOCTL_SEND_PAYLOAD: {
		retval = parsing_ipi_msg_from_user_space(
				 (void __user *)arg, AUDIO_IPI_PAYLOAD);
		break;
	}
	case AUDIO_IPI_IOCTL_SEND_DRAM: {
		retval = parsing_ipi_msg_from_user_space(
				 (void __user *)arg, AUDIO_IPI_DMA);
		break;
	}
	case AUDIO_IPI_IOCTL_LOAD_SCENE: {
		pr_debug("AUDIO_IPI_IOCTL_LOAD_SCENE(%d)", (uint8_t)arg);
		audio_load_task((uint8_t)arg);
		break;
	}
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
	case AUDIO_IPI_IOCTL_INIT_DSP: {
		pr_debug("AUDIO_IPI_IOCTL_INIT_DSP(%d)", (uint8_t)arg);
		audio_ipi_dma_init_dsp();
		break;
	}
	case AUDIO_IPI_IOCTL_REG_DMA: {
		if (((void __user *)arg) == NULL) {
			retval = -1;
			break;
		}
		retval = copy_from_user(
				 &dma_reg,
				 (void __user *)arg,
				 sizeof(struct audio_ipi_reg_dma_t));
		if (retval != 0) {
			pr_notice("dma reg copy_from_user retval %d", retval);
			break;
		}

		pr_debug("AUDIO_IPI_IOCTL_REG_DMA(%d,%d,0x%x,0x%x)",
			 dma_reg.task,
			 dma_reg.reg_flag,
			 dma_reg.a2d_size,
			 dma_reg.d2a_size);
		if (dma_reg.reg_flag)
			retval = audio_ipi_dma_alloc_region(dma_reg.task,
							    dma_reg.a2d_size,
							    dma_reg.d2a_size);
		else
			retval = audio_ipi_dma_free_region(dma_reg.task);

		break;
	}
	case AUDIO_IPI_IOCTL_ADSP_REG_FEA: {
		if (((void __user *)arg) == NULL) {
			retval = -1;
			break;
		}
		retval = copy_from_user(
				 &feat_reg,
				 (void __user *)arg,
				 sizeof(struct audio_ipi_reg_feature_t));
		if (retval != 0) {
			pr_notice("feature reg copy_from_user retval %d",
				  retval);
			break;
		}
		pr_debug("AUDIO_IPI_IOCTL_ADSP_REG_FEA(%s,%d)",
			 feat_reg.reg_flag ? "enable" : "disable",
			 feat_reg.feature_id);

		if (feat_reg.reg_flag)
			retval = adsp_register_feature(feat_reg.feature_id);
		else
			retval = adsp_deregister_feature(feat_reg.feature_id);

		break;
	}
#endif
	default:
		retval = -ENOIOCTLCMD;
		break;
	}
	return retval;
}

#ifdef CONFIG_COMPAT
static long audio_ipi_driver_compat_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	if (!file->f_op || !file->f_op->unlocked_ioctl) {
		pr_notice("op null");
		return -ENOTTY;
	}
	return file->f_op->unlocked_ioctl(file, cmd, arg);
}
#endif


static ssize_t audio_ipi_driver_read(
	struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
	return audio_ipi_dma_msg_read(buf, count);
#else
	return 0;
#endif
}



static const struct file_operations audio_ipi_driver_ops = {
	.owner          = THIS_MODULE,
	.read           = audio_ipi_driver_read,
	.unlocked_ioctl = audio_ipi_driver_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = audio_ipi_driver_compat_ioctl,
#endif
};


static struct miscdevice audio_ipi_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = AUDIO_IPI_DEVICE_NAME,
	.fops = &audio_ipi_driver_ops
};


static int __init audio_ipi_driver_init(void)
{
	int ret = 0;

#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
	scp_ipi_queue_init(AUDIO_OPENDSP_USE_HIFI3);
#endif

#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
	scp_ipi_queue_init(AUDIO_OPENDSP_USE_HIFI4);
#endif
	audio_task_manager_init();
	audio_messenger_ipi_init();

#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
	init_audio_ipi_dma();
#endif

#ifdef CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT
	audio_ipi_client_phone_call_init();
#endif

	ret = misc_register(&audio_ipi_device);
	if (unlikely(ret != 0)) {
		pr_notice("misc register failed");
		return ret;
	}

	return ret;
}


static void __exit audio_ipi_driver_exit(void)
{
#ifdef CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT
	audio_ipi_client_phone_call_deinit();
#endif

	audio_task_manager_deinit();

	deinit_audio_ipi_dma();

	misc_deregister(&audio_ipi_device);
}


module_init(audio_ipi_driver_init);
module_exit(audio_ipi_driver_exit);

