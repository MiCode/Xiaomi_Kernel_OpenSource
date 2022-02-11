/* SPDX-License-Identifier: GPL-2.0-only */
/* Atlantic Network Driver
 *
 * Copyright (C) 2021 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ATL_DUMP_H_
#define _ATL_DUMP_H_

enum atl_crash_dump_types {
	atl_crash_dump_type_regs     = 0,
	atl_crash_dump_type_fwiface  = 1,
	atl_crash_dump_type_act_res  = 2,
	atl_crash_dump_type_ring     = 3,
};

/** Some of the registers we can not read safely from the device.
 *  Those will not be read, with value 0xFFFFFFFF stored instead.
 */
struct atl_crash_dump_regs {
	u32 type;
	u32 length;
	union {
		u32 regs_data[0x9000/4];
		struct{
			u32 mif[0x1000/4];
			u32 pci[0x1000/4];
			u32 itr[0x1000/4];
			u32 com[0x1000/4];
			u32 mac_phy[0x1000/4];
			union {
				u32 rx[0x2000/4];
				struct{
					u32 res1[0x700/4];
					u32 pb_ctrl;
					u32 res2;
					u32 pb_status;
					u32 res3;
					/* 0x5710 */
					struct{
						u32 pb_reg1;
						u32 pb_reg2;
						u32 pb_reg3;
					} pb[8];
					u32 res4[0x100]; /* TODO */
					/* 0x5B00 */
					struct{
						u32 base_lo;
						u32 base_hi;
						u32 ctrl;
						u32 head;
						u32 tail;
						u32 status;
						u32 size;
						u32 threshold;
					} dma_desc[32];
				} layout;
			};
			u32 tx[0x2000/4];
		} layout;
	};
};

struct atl_crash_dump_fwiface {
	u32 type;
	u32 length;
	/* struct fw_interface_in */
	u32 fw_interface_in[0x1000/4];
	/* struct fw_interface_out */
	u32 fw_interface_out[0x1000/4];
};

/* Record table size of 128 with each entry holding (tag, mask, action) */
#define ATL_ACT_RES_TABLE_SIZE 384
struct atl_crash_dump_act_res {
	u32 type;
	u32 length;
	u32 act_res_data[ATL_ACT_RES_TABLE_SIZE];
};

/* max_size = 'ring_size * (tx + rx) * 16 byte raw descriptor data' */
#define ATL_MAX_RING_DESC_SIZE ATL_MAX_RING_SIZE * 32
struct atl_crash_dump_ring {
	u32 type;
	u32 length;
	u32 index;
	u32 rx_head;
	u32 rx_tail;
	u32 tx_head;
	u32 tx_tail;
	u32 rx_ring_size;
	u32 tx_ring_size;
	u8  ring_data[ATL_MAX_RING_DESC_SIZE];
};

/* This is an extensible structure to collect various debug data from
 * AQC device
 */
struct atl_crash_dump {
	u32 length;		/* total crash structure length, in bytes */
	u32 sections_count;	/* number of sections of type atl_crash_dump* following */
	u8 drv_version[16];
	u8 fw_version[16];
	union {
		struct {
			struct atl_crash_dump_regs regs;
			struct atl_crash_dump_fwiface fwiface;
			struct atl_crash_dump_act_res act_res;
			struct atl_crash_dump_ring ring[ATL_MAX_QUEUES];
		} antigua;
		struct {
			struct atl_crash_dump_regs regs;
			struct atl_crash_dump_ring ring[ATL_MAX_QUEUES];
		} atlantic;
	};
};


#endif
