/**
 * Copyright Elliptic Labs
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

#include "elliptic_data_io.h"
#include "elliptic_device.h"

#include <sound/apr_elliptic.h>
#define IO_PING_PONG_BUFFER_SIZE 512

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

void elliptic_data_io_cancel(struct elliptic_data *elliptic_data)
{
	atomic_set(&elliptic_data->abort_io, 1);
	wake_up_interruptible(&elliptic_data->fifo_usp_not_empty);

}

int32_t elliptic_data_io_write(uint32_t message_id, const char *data,
	size_t data_size)
{
	uint32_t port_id;

	port_id = ELLIPTIC_PORT_ID;
	return ultrasound_apr_set(port_id, &message_id, (u8 *)data,
		(int32_t)data_size);
}

int32_t elliptic_data_io_transact(uint32_t message_id, const char *data,
	size_t data_size, char *output_data, size_t output_data_size)
{
	pr_err("%s : unimplemented\n", __func__);
	return -EINVAL;
}


