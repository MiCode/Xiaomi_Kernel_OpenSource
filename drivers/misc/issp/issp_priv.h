/*
 * Copyright (c) 2006-2013, Cypress Semiconductor Corporation
 * All rights reserved.
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _ISSP_PRIV_H_
#define _ISSP_PRIV_H_

#include <linux/kernel.h>
#include <linux/issp.h>

struct issp_host {
	struct platform_device *pdev;
	struct issp_platform_data *pdata;
	const struct firmware *fw;
	const struct ihex_binrec *security_rec;
	uint16_t checksum_fw;
	uint8_t version_fw;
	uint8_t si_id[4];

	/* context to get fw data */
	const struct ihex_binrec *cur_rec;
	int cur_idx;
};

#define ISSP_FW_SECURITY_ADDR	0x00100000
#define ISSP_FW_CHECKSUM_ADDR	0x00200000

/* let uC go to programing state */
int issp_uc_program(struct issp_host *host);
/* let uC go to normal state */
int issp_uc_run(struct issp_host *host);

int issp_get_checksum(struct issp_host *host, uint16_t *checksum);
int issp_program(struct issp_host *host);
int issp_read_block(struct issp_host *host, uint8_t block_idx, uint8_t addr,
			uint8_t *buf, int len);

/* We only support fw that stores data from low address to high address */
void issp_fw_rewind(struct issp_host *host);
void issp_fw_seek_security(struct issp_host *host);
uint8_t issp_fw_get_byte(struct issp_host *host);

#endif
