/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 */

#ifndef __QSEECOM_KERNEL_H_
#define __QSEECOM_KERNEL_H_

#include <linux/types.h>
#include <soc/qcom/scm.h>

#define QSEECOM_ALIGN_SIZE	0x40
#define QSEECOM_ALIGN_MASK	(QSEECOM_ALIGN_SIZE - 1)
#define QSEECOM_ALIGN(x)	\
	((x + QSEECOM_ALIGN_MASK) & (~QSEECOM_ALIGN_MASK))

/*
 * struct qseecom_handle -
 *      Handle to the qseecom device for kernel clients
 * @sbuf - shared buffer pointer
 * @sbbuf_len - shared buffer size
 */
struct qseecom_handle {
	void *dev; /* in/out */
	unsigned char *sbuf; /* in/out */
	uint32_t sbuf_len; /* in/out */
};

int qseecom_start_app(struct qseecom_handle **handle,
						char *app_name, uint32_t size);
int qseecom_shutdown_app(struct qseecom_handle **handle);
int qseecom_send_command(struct qseecom_handle *handle, void *send_buf,
			uint32_t sbuf_len, void *resp_buf, uint32_t rbuf_len);
int qseecom_set_bandwidth(struct qseecom_handle *handle, bool high);
int qseecom_process_listener_from_smcinvoke(struct scm_desc *desc);

#endif /* __QSEECOM_KERNEL_H_ */
