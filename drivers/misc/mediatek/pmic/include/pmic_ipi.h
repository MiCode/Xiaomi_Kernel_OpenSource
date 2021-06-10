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
#ifndef _PMIC_IPI_H_
#define _PMIC_IPI_H_




#define PMIC_IPI_SEND_SLOT_SIZE 0x5
#define PMIC_IPI_ACK_SLOT_SIZE	0x2



struct pmic_ipi_cmds {
	unsigned int cmd[PMIC_IPI_SEND_SLOT_SIZE];
};

struct pmic_ipi_ret_datas {
	unsigned int data[PMIC_IPI_ACK_SLOT_SIZE];
};

extern void aee_rr_rec_set_bit_pmic_ext_buck(int bit, int loc);

#endif /* _PMIC_IPI_H_*/

