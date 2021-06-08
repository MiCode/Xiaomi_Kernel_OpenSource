/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#ifndef __QBG_SDAM_H__
#define __QBG_SDAM_H__

#define QBG_SDAM_PBS_STATUS_OFFSET		0x45
#define QBG_SDAM_FIFO_COUNT_OFFSET		0x46
#define QBG_ESSENTIAL_PARAMS_START_OFFSET	0x47
#define QBG_SDAM_START_OFFSET			0x4b
#define QBG_SDAM_BHARGER_OCV_HDRM_OFFSET	0x5b
#define QBG_SDAM_INT_TEST1			0xe0
#define QBG_SDAM_INT_TEST_VAL			0xe1
#define QBG_SDAM_DATA_PUSH_COUNTER_OFFSET	0x46

#define QBG_SDAM_TEST_VAL_1_SET			0x2
#define QBG_SDAM_ONE_FIFO_REGION_SIZE		117 /* 117 bytes for FIFO starting from 0x4B */
#define QBG_SDAM_ESR_PULSE_FIFO_INDEX		11

#define QBG_SDAM_DATA_START_OFFSET(chip, index)	\
	(chip->sdam_base + (index * 0x100) + QBG_SDAM_START_OFFSET)

int qbg_sdam_read(struct qti_qbg *chip, int offset, u8 *data, int length);
int qbg_sdam_write(struct qti_qbg *chip, int offset, u8 *data, int length);
int qbg_sdam_get_fifo_data(struct qti_qbg *chip, struct fifo_data *fifo,
				u32 fifo_count);
int qbg_sdam_get_essential_params(struct qti_qbg *chip, u8 *params);
int qbg_sdam_set_essential_params(struct qti_qbg *chip, u8 *params);

#endif /* __QBG_SDAM_H__ */
