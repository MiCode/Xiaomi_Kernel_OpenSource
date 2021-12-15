/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "audio_ipi_client_phone_call.h"

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
#include <linux/kthread.h>


#include <linux/io.h>

#include <linux/wait.h>
#include <linux/time.h>

#include "audio_log.h"
#include "audio_assert.h"

#include "audio_task_manager.h"

#include <audio_ipi_dma.h>
#include "audio_speech_msg_id.h"


void phone_call_recv_message(struct ipi_msg_t *p_ipi_msg)
{
	DUMP_IPI_MSG("phone call d2k msg", p_ipi_msg);
}


void phone_call_task_unloaded(void)
{
	pr_debug("%s()\n", __func__);
}


void audio_ipi_client_phone_call_init(void)
{
	pr_debug("%s()\n", __func__);

	audio_task_register_callback(
		TASK_SCENE_PHONE_CALL,
		phone_call_recv_message,
		phone_call_task_unloaded);
}


void audio_ipi_client_phone_call_deinit(void)
{
	pr_debug("%s()\n", __func__);
}



