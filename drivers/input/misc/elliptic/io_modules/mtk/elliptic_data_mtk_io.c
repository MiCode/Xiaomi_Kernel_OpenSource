/**
 * Copyright Elliptic Labs
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <linux/errno.h>
#include <linux/wakelock.h>

#include <scp_ipi.h>
#include <elliptic/elliptic_data_io.h>
#include "elliptic_device.h"

#define ELLIPTIC_HOST_MESSAGE_HEADER_SIZE \
		(sizeof(struct elliptic_ipi_host_message_header))
#define ELLIPTIC_HOST_MESSAGE_MAX_DATA_SIZE \
		((SHARE_BUF_DATA_SIZE) - (ELLIPTIC_HOST_MESSAGE_HEADER_SIZE))

struct elliptic_ipi_host_message_header {
	uint32_t elliptic_ipi_message_id;
	uint32_t data_size;
};

struct elliptic_ipi_host_message {
	elliptic_ipi_host_message_header_t header;
	uint8_t data[ELLIPTIC_HOST_MESSAGE_MAX_DATA_SIZE];
};

static int elliptic_store_data(
	const char *buffer,
	size_t buffer_size,
)
{
	int ret = 0;
	int id = 0;

	switch (id) {
	case ELLIPTIC_ULTRASOUND_PARAM_ID_ENGINE_VERSION:
		if (buffer_size >= ELLIPTIC_VERSION_INFO_SIZE) {
			pr_debug("[ELUS]: elliptic_version copied to local AP cache");
			data_block =
			elliptic_get_shared_obj(
				ELLIPTIC_OBJ_ID_VERSION_INFO);
			copy_size = min_t(size_t, data_block->size,
				(size_t)ELLIPTIC_VERSION_INFO_SIZE);

			memcpy((u8 *)data_block->buffer,
				&payload[3], copy_size);
			ret = (int32_t)copy_size;
		}
	break;
	default:
		break;
	}
	return ret;
}

/* Will be called from MTK SCP IPI driver when data arrives from DSP */
void elliptic_data_io_ipi_handler(int id, void *data, unsigned int len)
{
	const size_t max_len = min_t(size_t, len, ELLIPTIC_MSG_BUF_SIZE);

	elliptic_data_push(ELLIPTIC_ALL_DEVICES, (const char *)data,
		max_len, ELLIPTIC_DATA_PUSH_FROM_KERNEL);
}

int elliptic_data_io_initialize(void)
{
	ipi_status ipi_registration_result;

	ipi_registration_result = scp_ipi_registration(IPI_USND,
	elliptic_data_io_ipi_handler, "usnd");
	if (ipi_registration_result != 0) {
		EL_PRINT_E("failed to register IPI callback");
		return -EINVAL;
	}

	return 0;
}

int32_t elliptic_data_io_write(uint32_t message_id, const char *data,
	size_t data_size)
{
	static struct elliptic_ipi_host_message host_message;
	ipi_status send_status;

	host_message.header.elliptic_ipi_message_id = message_id;

	host_message.header.data_size =
		min_t(size_t, data_size, ELLIPTIC_HOST_MESSAGE_MAX_DATA_SIZE);
	memcpy(host_message.data, data, host_message.header.data_size);

	send_status = scp_ipi_send(IPI_USND, &host_message,
					sizeof(host_message), 1);

	if (send_status != DONE) {
		pr_err("[ELUS]: elliptic_data_io_write failed to send\n");
		return 0;
	}
	return (int32_t)data_size;

	return 0;
}


int elliptic_data_io_cleanup(void)
{
	EL_PRINT_I("Unimplemented");
	return 0;
}

