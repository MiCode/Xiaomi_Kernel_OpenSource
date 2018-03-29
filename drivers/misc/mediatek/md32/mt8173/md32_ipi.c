/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/interrupt.h>
#include <mach/md32_helper.h>
#include <mach/md32_ipi.h>

#include "md32_irq.h"

#define SHARE_BUF_SIZE 64

struct share_obj {
	enum ipi_id id;
	unsigned int len;
	unsigned char reserve[8];
	unsigned char share_buf[SHARE_BUF_SIZE - 16];
};

struct ipi_desc {
	ipi_handler_t handler;
	const char  *name;
};

struct ipi_desc ipi_desc[MD32_NR_IPI];
struct share_obj *md32_send_obj, *md32_rcv_obj;
/* md32 to AP ipi mutex */
struct mutex md32_ipi_mutex;

static void ipi_md2host(void)
{
	writel(0x1, HOST_TO_MD32_REG);
}

void md32_ipi_handler(void)
{
	if (ipi_desc[md32_rcv_obj->id].handler)
		ipi_desc[md32_rcv_obj->id].handler(md32_rcv_obj->id,
						   md32_rcv_obj->share_buf,
						   md32_rcv_obj->len);
	writel(0x0, MD32_TO_SPM_REG);
}

void md32_ipi_init(void)
{
	mutex_init(&md32_ipi_mutex);
	md32_rcv_obj = (struct share_obj *)MD32_DTCM;
	md32_send_obj = md32_rcv_obj + 1;
	pr_debug("[MD32] md32_rcv_obj = %lx\n", (unsigned long)md32_rcv_obj);
	memset(md32_send_obj, 0, SHARE_BUF_SIZE);
}

/*
  @param id:       IPI ID
  @param handler:  IPI handler
  @param name:     IPI name
*/
enum ipi_status md32_ipi_registration(enum ipi_id id, ipi_handler_t handler,
				      const char *name)
{
	if (id < MD32_NR_IPI) {
		ipi_desc[id].name = name;

		if (handler == NULL)
			return ERROR;

		ipi_desc[id].handler = handler;
		return DONE;
	} else {
		return ERROR;
	}
}

/*
 * @param id:       IPI ID
 * @param buf:      the pointer of data
 * @param len:      data length
 * @param wait:     If true, wait (atomically) until data have been
 *			gotten by Host
 */
enum ipi_status md32_ipi_send(enum ipi_id id, void *buf, unsigned int len,
			      unsigned int wait)
{
	unsigned int sw_rstn;

	sw_rstn = readl(MD32_BASE);

	if (sw_rstn == 0x0) {
		pr_warn("[MD32] md32_ipi_send: MD32 not enabled\n");
		return ERROR;
	}

	if (id < MD32_NR_IPI) {
		if (len > sizeof(md32_send_obj->share_buf) || buf == NULL)
			return ERROR;

		if (readl(HOST_TO_MD32_REG))
			return BUSY;

		mutex_lock(&md32_ipi_mutex);

		memcpy((void *)md32_send_obj->share_buf, buf, len);
		md32_send_obj->len = len;
		md32_send_obj->id = id;
		ipi_md2host();

		if (wait)
			while (readl(HOST_TO_MD32_REG))
				;

		mutex_unlock(&md32_ipi_mutex);
	} else {
		return ERROR;
	}

	return DONE;
}
