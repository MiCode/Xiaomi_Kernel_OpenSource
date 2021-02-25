/**
 * Copyright Elliptic Labs
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/mm.h>


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
/*  includes the file structure, that is, file open read close */
#include <linux/fs.h>

/* include the character device, makes cdev avilable */
#include <linux/cdev.h>
#include <linux/semaphore.h>

/* includes copy_user vice versa */
#include <linux/uaccess.h>

#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/device.h>


#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <linux/errno.h>

#include <elliptic/elliptic_data_io.h>
#include <elliptic/elliptic_device.h>

#include <dsp/apr_elliptic.h>
#include <dsp/q6afe-v2.h>

#define IO_PING_PONG_BUFFER_SIZE 512
#define AFE_MSM_RX_PSEUDOPORT_ID 0x8001
#define AFE_MSM_TX_PSEUDOPORT_ID 0x8002

struct elliptic_msm_io_device {
};

/* static struct elliptic_msm_io_device io_device;*/


int elliptic_data_io_initialize(void)
{
	return 0;
}

int elliptic_data_io_cleanup(void)
{
	return 0;
}
int elliptic_io_open_port(int portid)
{
	if (portid == ULTRASOUND_RX_PORT_ID)
		return afe_start_pseudo_port(AFE_MSM_RX_PSEUDOPORT_ID);
	else
		return afe_start_pseudo_port(AFE_MSM_TX_PSEUDOPORT_ID);
}

int elliptic_io_close_port(int portid)
{
	if (portid == ULTRASOUND_RX_PORT_ID)
		return afe_stop_pseudo_port(AFE_MSM_RX_PSEUDOPORT_ID);
	else
		return afe_stop_pseudo_port(AFE_MSM_TX_PSEUDOPORT_ID);
}

int32_t elliptic_data_io_write(uint32_t message_id, const char *data,
	size_t data_size)
{
	int32_t result = 0;

	/* msm_pcm_routing_acquire_lock(); */

	result = ultrasound_apr_set_parameter(ELLIPTIC_PORT_ID,
		message_id, (u8 *)data,
		(int32_t)data_size);

	/* msm_pcm_routing_release_lock();*/
	return result;
}

int32_t elliptic_data_io_transact(uint32_t message_id, const char *data,
	size_t data_size, char *output_data, size_t output_data_size)
{
	pr_err("%s : unimplemented\n", __func__);
	return -EINVAL;
}
