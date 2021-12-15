/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

