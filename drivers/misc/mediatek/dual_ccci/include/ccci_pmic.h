/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __CCCI_PMIC_H__
#define __CCCI_PMIC_H__

enum pmic6326_ccci_op {
	PMIC6326_VSIM_ENABLE = 0,
	PMIC6326_VSIM_SET_AND_ENABLE = 1,
	PMIC6236_LOCK = 2,
	PMIC6326_UNLOCK = 3,
	PMIC6326_VSIM2_ENABLE = 4,
	PMIC6326_VSIM2_SET_AND_ENABLE = 5,
	PMIC6326_MAX
};

enum pmic6326_ccci_type {
	PMIC6326_REQ = 0,	/*  Local side send request to remote side */
	PMIC6326_RES = 1	/*  Remote side send response to local side */
};

struct pmic6326_ccci_msg {
	unsigned short pmic6326_op;	/*  Operation */
	unsigned short pmic6326_type;	/*  message type: Request or Response */
	unsigned short pmic6326_param1;
	unsigned short pmic6326_param2;
};

struct pmic6326_ccci_msg_info {
	unsigned int pmic6326_exec_time;	/*  Operation execution time (In ms) */
	unsigned short pmic6326_param1;
	unsigned short pmic6326_param2;
};

/*
    PMIC share memory
    (MSB)                                                   (LSB)
    |  1 byte        | 1 byte        | 1 byte        | 1 byte   |
       Param2          Param1          Type            Op
    |  1 byte        | 1 byte        | 2 bytes                  |
       Param2          Param1          Exec_time
*/

struct pmic6326_share_mem_info {
	pmic6326_ccci_msg ccci_msg;
	pmic6326_ccci_msg_info ccci_msg_info;
};

struct shared_mem_pmic_t {
	struct pmic6326_ccci_msg ccci_msg;
	struct pmic6326_ccci_msg_info ccci_msg_info;
};

int __init ccci_pmic_init(void);
void __exit ccci_pmic_exit(void);

#define CCCI_PMIC_SMEM_SIZE sizeof(struct shared_mem_pmic_t)
#endif				/*  __CCCI_PMIC_H__ */
