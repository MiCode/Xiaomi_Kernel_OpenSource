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

#ifndef __MD32_IPI_H
#define __MD32_IPI_H

enum ipi_id {
	IPI_WDT = 0,
	IPI_TEST1,
	IPI_TEST2,
	IPI_LOGGER,
	IPI_SWAP,
	IPI_ANC,
	IPI_SPK_PROTECT,
	IPI_THERMAL,
	IPI_SPM,
	IPI_DVT_TEST,
	IPI_BUF_FULL,
	IPI_VCORE_DVFS,
	MD32_NR_IPI,
};

enum ipi_status {
	ERROR = -1,
	DONE,
	BUSY,
};

typedef void (*ipi_handler_t) (int id, void *data, unsigned int len);

enum ipi_status md32_ipi_registration(enum ipi_id id, ipi_handler_t handler,
				      const char *name);
enum ipi_status md32_ipi_send(enum ipi_id id, void *buf, unsigned int  len,
			      unsigned int wait);
#endif	/* __MD32_IPI_H */
