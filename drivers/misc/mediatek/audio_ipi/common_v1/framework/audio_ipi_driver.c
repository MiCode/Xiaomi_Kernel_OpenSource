// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2016 MediaTek Inc.

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
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

#include <audio_ipi_queue.h>

#ifdef CONFIG_MTK_AUDIO_CM4_SUPPORT
#include <scp_helper.h>
#include <scp_ipi.h>
#include <scp_feature_define.h>
#endif

#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
#include <adsp_helper.h>
#include <adsp_ipi.h>
#include <adsp_service.h>
#endif


#include "audio_log.h"
#include "audio_assert.h"

#include <audio_controller_msg_id.h>
#include <audio_messenger_ipi.h>

#include <audio_task_manager.h>

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

#ifdef ipi_dbg
#undef ipi_dbg
#endif

#ifdef DEBUG_IPI
#define ipi_dbg(x...) pr_info(x)
#else
#define ipi_dbg(x...)
#endif

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



/*
 * =============================================================================
 *                     struct
 * =============================================================================
 */

struct audio_ipi_reg_dma_t {
	uint32_t magic_header;
	uint8_t task;
	uint8_t reg_flag; /* 1: register, 0: unregister */
	uint16_t __reserved;

	uint32_t a2d_size;
	uint32_t d2a_size;
	uint32_t magic_footer;
};


struct audio_ipi_reg_feature_t {
	uint16_t reg_flag;
	uint16_t feature_id;
};


struct audio_task_info_t {
	uint32_t dsp_id;            /* dsp_id_t */
	uint8_t  is_dsp_support;    /* dsp_id supported or not */
	uint8_t  is_adsp;           /* adsp(HiFi) or not */
	uint8_t  is_scp;            /* scp(CM4) or not */
	uint8_t  task_ctrl;         /* task controller scene # */
};



/*
 * =============================================================================
 *                     global var
 * =============================================================================
 */

static struct audio_task_info_t g_audio_task_info[TASK_SCENE_SIZE];



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

	phys_addr_t dram_buf = 0;
	void *dram_buf_virt = NULL;

	int retval = 0;


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
	if (ipi_msg.source_layer != AUDIO_IPI_LAYER_FROM_HAL) {
		pr_notice("source_layer %d != %d", ipi_msg.source_layer, AUDIO_IPI_LAYER_FROM_HAL);
		retval = -1;
		goto parsing_exit;
	}
	msg_len = get_message_buf_size(&ipi_msg);
	retval = check_msg_format(&ipi_msg, msg_len);
	if (retval != 0)
		goto parsing_exit;

	ipi_dbg("task %d msg 0x%x", ipi_msg.task_scene, ipi_msg.msg_id);

	/* get dma buf if need */
	dma_info = &ipi_msg.dma_info;
	wb_dram = &dma_info->wb_dram;

	if (data_type == AUDIO_IPI_DMA) {
		/* get hal data & write hal data to DRAM */
		hal_data_size = dma_info->hal_buf.data_size;
		if (hal_data_size > MAX_DSP_DMA_WRITE_SIZE) {
			DUMP_IPI_MSG("hal_data_size error!!", &ipi_msg);
			goto parsing_exit;
		}
		copy_hal_data = vmalloc(hal_data_size);
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
		ipi_dbg(
			"write region copy_hal_data(%p), hal_data_size %d, hal_wb_buf_size %d"
			, copy_hal_data, hal_data_size, hal_wb_buf_size);
		if (hal_wb_buf_size != 0 && hal_wb_buf_addr != NULL) {
			/* alloc a dma for wb */
			audio_ipi_dma_alloc(ipi_msg.task_scene,
					    &dram_buf,
					    &dram_buf_virt,
					    hal_wb_buf_size);

			wb_dram->memory_size = hal_wb_buf_size;
			wb_dram->data_size = 0;
			wb_dram->addr_val = dram_buf;
			ipi_dbg(
				"hal_wb_buf_addr(%p), wb dram_buf(%u), hal_wb_buf_size(%d)"
				, hal_wb_buf_addr, dram_buf, hal_wb_buf_size);

			/* force need ack to get scp info */
			if (ipi_msg.ack_type != AUDIO_IPI_MSG_NEED_ACK) {
				pr_notice("task %d msg 0x%x need ack!!",
					  ipi_msg.task_scene, ipi_msg.msg_id);
				ipi_msg.ack_type = AUDIO_IPI_MSG_NEED_ACK;
			}
		}
#ifdef DEBUG_IPI /* debug only */
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
					 dram_buf_virt,
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
		vfree(copy_hal_data);
		copy_hal_data = NULL;
	}
	if (dram_buf != 0) {
		ipi_dbg(
			"task %d msg 0x%x, free wb buffer %u, hal_wb_buf_size 0x%x"
			, ipi_msg.task_scene, ipi_msg.msg_id,
			dram_buf, hal_wb_buf_size);
		audio_ipi_dma_free(ipi_msg.task_scene,
				   dram_buf, hal_wb_buf_size);
	}
	ipi_dbg("task %d msg 0x%x, retval %d",
		ipi_msg.task_scene, ipi_msg.msg_id, retval);

	return retval;
}


#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
/* ADSP reboot */
#ifdef CFG_RECOVERY_SUPPORT
static int audio_ctrl_event_receive(
	struct notifier_block *this,
	unsigned long event,
	void *ptr)
{
	struct ipi_queue_handler_t *handler = NULL;
	uint8_t scene = 0;
	uint32_t dsp_id = 0;

	switch (event) {
	case ADSP_EVENT_STOP:
		for (scene = 0; scene < TASK_SCENE_SIZE; scene++) {
			if (is_audio_use_adsp(audio_get_dsp_id(scene))) {
				handler = get_ipi_queue_handler(scene);
				if (handler != NULL)
					flush_ipi_queue_handler(handler);
			}
		}
		break;
	case ADSP_EVENT_READY:
		for (dsp_id = 0; dsp_id < NUM_OPENDSP_TYPE; dsp_id++) {
			if (is_audio_use_adsp(dsp_id))
				audio_ipi_dma_init_dsp(dsp_id);

		}
		break;
	default:
		pr_info("event %lu err", event);
	}
	return 0;
}


static struct notifier_block audio_ctrl_notifier = {
	.notifier_call = audio_ctrl_event_receive,
	.priority = AUDIO_CONTROLLER_FEATURE_PRI,
};
#endif /* end of CFG_RECOVERY_SUPPORT */

/* HAL reboot */
static int audio_ipi_init_dsp_hifi3(const uint32_t dsp_id)
{
	static bool init_flag[NUM_OPENDSP_TYPE];

	uint8_t task = TASK_SCENE_INVALID;

	struct ipi_msg_t ipi_msg;
	int ret = 0;

	if (dsp_id >= NUM_OPENDSP_TYPE) {
		pr_info("dsp_id(%u) invalid!!!", dsp_id);
		return -ENODEV;
	}
	if (!is_audio_use_adsp(dsp_id)) {
		pr_info("dsp_id(%u) not for adsp!!!", dsp_id);
		return -ENODEV;
	}

	task = get_audio_controller_task(dsp_id);
	if (task == TASK_SCENE_INVALID) {
		pr_info("task(%d) invalid!!!", task);
		return -ENODEV;
	}

	/* check phone boot */
	if (init_flag[dsp_id] == false) {
		init_flag[dsp_id] = true;
		pr_info("phone init");
		audio_ipi_dma_init_dsp(dsp_id);
		return 0;
	}
	/* wake up adsp */
	adsp_register_feature(AUDIO_CONTROLLER_FEATURE_ID);

	/* not first init => HAL reboot */
	pr_info("audio hal reinit");


	/* TODO: do something before ADSP process hal reboot here */





	/* ADSP process hal reboot here */
	ret = audio_send_ipi_msg(
		      &ipi_msg,
		      task,
		      AUDIO_IPI_LAYER_TO_DSP,
		      AUDIO_IPI_MSG_ONLY,
		      AUDIO_IPI_MSG_NEED_ACK,
		      AUD_CTL_MSG_A2D_HAL_REBOOT,
		      0, /* 0: audio HAL */
		      0,
		      NULL);

	/* release DMA */
	audio_ipi_dma_free_region_all_task(dsp_id);


	/* allow adsp to sleep */
	adsp_deregister_feature(AUDIO_CONTROLLER_FEATURE_ID);


	/* TODO: clear feature register count here? */
	reset_hal_feature_table();
	return ret;
}
#endif

#if defined(CONFIG_MTK_AUDIO_CM4_SUPPORT)
/* SCP reboot */
#if defined(SCP_RECOVERY_SUPPORT)
static int audio_ctrl_event_receive_scp(
	struct notifier_block *this,
	unsigned long event,
	void *ptr)
{
	struct ipi_queue_handler_t *handler = NULL;
	uint8_t scene = 0;

	switch (event) {
	case SCP_EVENT_STOP:
		for (scene = 0; scene < TASK_SCENE_SIZE; scene++) {
			if (audio_get_dsp_id(scene) ==
			    AUDIO_OPENDSP_USE_CM4_A) {
				handler = get_ipi_queue_handler(scene);
				if (handler != NULL)
					flush_ipi_queue_handler(handler);
			}
		}
		break;
#if defined(CONFIG_MTK_AUDIO_CM4_DMA_SUPPORT)
	case SCP_EVENT_READY:
		audio_ipi_dma_init_dsp(AUDIO_OPENDSP_USE_CM4_A);
		break;
#endif
	default:
		pr_info("event %lu err", event);
	}
	return 0;
}

static struct notifier_block audio_ctrl_notifier_scp = {
	.notifier_call = audio_ctrl_event_receive_scp,
};
#endif /* end of SCP_RECOVERY_SUPPORT */

static int audio_ipi_init_dsp_cm4(void)
{
	int ret = 0;

#if defined(CONFIG_MTK_AUDIO_CM4_DMA_SUPPORT)
	struct ipi_msg_t ipi_msg;
	static bool init_flag;

	/* check phone boot */
	if (init_flag == false) {
		init_flag = true;
		pr_info("phone init");
		audio_ipi_dma_init_dsp(AUDIO_OPENDSP_USE_CM4_A);
		return 0;
	}

	/* not first init => HAL reboot */
	pr_info("audio hal reinit");

	/* TODO: do something before SCP process hal reboot here */





	/* SCP process hal reboot here */
	ret = audio_send_ipi_msg(
		      &ipi_msg,
		      TASK_SCENE_AUDIO_CONTROLLER_CM4,
		      AUDIO_IPI_LAYER_TO_DSP,
		      AUDIO_IPI_MSG_ONLY,
		      AUDIO_IPI_MSG_NEED_ACK,
		      AUD_CTL_MSG_A2D_HAL_REBOOT,
		      0, /* 0: audio HAL */
		      0,
		      NULL);

	/* release DMA */
	audio_ipi_dma_free_region_all_task(AUDIO_OPENDSP_USE_CM4_A);
#endif

	return ret;
}
#endif /* end of CONFIG_MTK_AUDIO_CM4_SUPPORT */

static long audio_ipi_driver_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
	uint32_t dsp_id = 0;
#endif

	struct audio_ipi_reg_dma_t dma_reg;
	int retval = 0;
	uint32_t check_sum = 0;

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
	case AUDIO_IPI_IOCTL_INIT_DSP: {
		pr_debug("AUDIO_IPI_IOCTL_INIT_DSP");
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
		for (dsp_id = 0; dsp_id < NUM_OPENDSP_TYPE; dsp_id++) {
			if (is_audio_use_adsp(dsp_id))
				audio_ipi_init_dsp_hifi3(dsp_id);

		}
#endif
#if defined(CONFIG_MTK_AUDIO_CM4_SUPPORT)
		audio_ipi_init_dsp_cm4();
#endif
		/* copy g_audio_task_info to HAL */
		if (((void __user *)arg) == NULL)
			retval = -EINVAL;
		else {
			retval = copy_to_user((void __user *)arg,
					      g_audio_task_info,
					      sizeof(g_audio_task_info));
		}
		if (retval) {
			pr_info("task info copy_to_user err");
			retval = -EFAULT;
		}
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
		check_sum = dma_reg.magic_footer + dma_reg.magic_header;
		if (check_sum != 0xFFFFFFFF) {
			pr_notice("dma reg check fail! header(0x%x) footer(0x%x)",
				  dma_reg.magic_header,
				  dma_reg.magic_footer);
			retval = -1;
			break;
		}

		if (dma_reg.reg_flag)
			retval = audio_ipi_dma_alloc_region(dma_reg.task,
							    dma_reg.a2d_size,
							    dma_reg.d2a_size);
		else
			retval = audio_ipi_dma_free_region(dma_reg.task);

		break;
	}
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
	return audio_ipi_dma_msg_read(buf, count);
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

	uint32_t dsp_id = 0;

	struct audio_task_info_t *task_info = NULL;
	uint8_t task_id = 0;

	for (dsp_id = 0; dsp_id < NUM_OPENDSP_TYPE; dsp_id++) {
		if (is_audio_dsp_support(dsp_id))
			scp_ipi_queue_init(dsp_id);
	}

	audio_task_manager_init();
	audio_messenger_ipi_init();

	for (dsp_id = 0; dsp_id < NUM_OPENDSP_TYPE; dsp_id++) {
		if (is_audio_dsp_support(dsp_id))
			init_audio_ipi_dma(dsp_id);
	}

#if defined(CONFIG_MTK_AUDIODSP_SUPPORT) && defined(CFG_RECOVERY_SUPPORT)
	adsp_A_register_notify(&audio_ctrl_notifier);
#endif

#if defined(CONFIG_MTK_AUDIO_CM4_SUPPORT) && defined(SCP_RECOVERY_SUPPORT)
	scp_A_register_notify(&audio_ctrl_notifier_scp);
#endif

#if defined(CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT)
	audio_ipi_client_phone_call_init();
#endif

	for (task_id = 0; task_id < TASK_SCENE_SIZE; task_id++) {
		task_info = &g_audio_task_info[task_id];

		dsp_id = audio_get_dsp_id(task_id);

		task_info->dsp_id = dsp_id;
		task_info->is_dsp_support = is_audio_dsp_support(dsp_id);
		task_info->is_adsp = is_audio_use_adsp(dsp_id);
		task_info->is_scp = is_audio_use_scp(dsp_id);
		task_info->task_ctrl = get_audio_controller_task(dsp_id);
	}

	ret = misc_register(&audio_ipi_device);
	if (unlikely(ret != 0)) {
		pr_notice("misc register failed");
		return ret;
	}

	return ret;
}


static void __exit audio_ipi_driver_exit(void)
{
	int dsp_id = 0;

#if defined(CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT)
	audio_ipi_client_phone_call_deinit();
#endif
	audio_task_manager_deinit();
	for (dsp_id = 0; dsp_id < NUM_OPENDSP_TYPE; dsp_id++) {
		if (is_audio_dsp_support(dsp_id))
			deinit_audio_ipi_dma(dsp_id);
	}
	misc_deregister(&audio_ipi_device);
}


module_init(audio_ipi_driver_init);
module_exit(audio_ipi_driver_exit);

