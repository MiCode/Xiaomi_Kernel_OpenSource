/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
#ifndef __VM_BMS_H__
#define __VM_BMS_H__

#define VM_BMS_DEVICE			"/dev/vm_bms"
#define MAX_FIFO_REGS			8

/**
 * struct qpnp_vm_bms_data - vm-bms data (passed to usersapce)
 * @data_type:			type of data filled up
 * @num_fifo:			count of valid fifo averages
 * @fifo_uv:			array of fifo averages in uv
 * @sample_interval		sample interval of the fifo data in ms
 * @sample_count		total samples in one fifo
 * @acc_uv			averaged accumulator value in uv
 * @acc_count			num of accumulated samples
 * @seq_num			sequence number of the data
 */
struct qpnp_vm_bms_data {
	unsigned int num_fifo;
	unsigned int fifo_uv[MAX_FIFO_REGS];
	unsigned int sample_interval_ms;
	unsigned int sample_count;
	unsigned int acc_uv;
	unsigned int acc_count;
	unsigned int seq_num;
};

enum vmbms_power_usecase {
	VMBMS_IGNORE_ALL_BIT = 1,
	VMBMS_VOICE_CALL_BIT = (1 << 4),
	VMBMS_STATIC_DISPLAY_BIT = (1 << 5),
};

#endif	/* __VM_BMS_H__ */
