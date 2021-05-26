/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __APUSYS_CMD_FORMAT_H__
#define __APUSYS_CMD_FORMAT_H__

#define APUSYS_MAGIC_NUMBER 0x3d2070ece309c231
#define APUSYS_CMD_VERSION 0x1

#define VALUE_SUBGRAPH_PACK_ID_NONE      0xFFFFFFFF
#define VALUE_SUBGRAPH_CTX_ID_NONE       0xFFFFFFFF
#define VALUE_SUBGRAPH_BOOST_NONE       0xFF

#define TYPE_SUBGRAPH_SCOFS_ELEMENT unsigned int
#define TYPE_CMD_SUCCESSOR_ELEMENT unsigned int
#define TYPE_CMD_PREDECCESSOR_CMNT_ELEMENT unsigned int

#define SIZE_SUBGRAPH_SCOFS_ELEMENT \
	sizeof(TYPE_SUBGRAPH_SCOFS_ELEMENT)
#define SIZE_CMD_SUCCESSOR_ELEMENT \
	sizeof(TYPE_CMD_SUCCESSOR_ELEMENT)
#define SIZE_CMD_PREDECCESSOR_CMNT_ELEMENT \
	sizeof(TYPE_CMD_PREDECCESSOR_CMNT_ELEMENT)

/* flag bitmap of apusys cmd */
enum {
	CMD_FLAG_BITMAP_POWERSAVE = 0,
	CMD_FLAG_BITMAP_MULTI0 = 62,
	CMD_FLAG_BITMAP_MULTI1 = 63,
};

/* fd flag of codebuf info offset */
enum {
	SUBGRAPH_CODEBUF_INFO_BIT_FD = 31,
};

struct apusys_cmd_hdr {
	unsigned long long magic;
	unsigned long long uid;
	unsigned char version;
	unsigned char priority;
	unsigned short hard_limit;
	unsigned short soft_limit;
	unsigned short preserved;
	unsigned long long flag_bitmap;
	unsigned int num_sc;
	unsigned int ofs_scr_list;     // successor list offset
	unsigned int ofs_pdr_cnt_list; // predecessor count list offset
	unsigned int scofs_list_entry; // subcmd offset's list offset
} __attribute__((__packed__));

struct apusys_sc_hdr_cmn {
	unsigned int dev_type;
	unsigned int driver_time;
	unsigned int ip_time;
	unsigned int suggest_time;
	unsigned int bandwidth;
	unsigned int tcm_usage;
	unsigned char tcm_force;
	unsigned char boost_val;
	unsigned short reserved;
	unsigned int mem_ctx;
	unsigned int cb_info_size; // codebuf info size
	unsigned int ofs_cb_info;  // codebuf info offset
} __attribute__((__packed__));

struct apusys_sc_hdr_sample {
	unsigned int pack_idx;
} __attribute__((__packed__));

struct apusys_sc_hdr_mdla {
	unsigned int ofs_cb_info_dual0;
	unsigned int ofs_cb_info_dual1;
	unsigned int ofs_pmu_info;
} __attribute__((__packed__));

struct apusys_sc_hdr_vpu {
	unsigned int pack_idx;
} __attribute__((__packed__));

struct apusys_sc_hdr {
	struct apusys_sc_hdr_cmn cmn;
	union {
		struct apusys_sc_hdr_sample sample;
		struct apusys_sc_hdr_mdla mdla;
		struct apusys_sc_hdr_vpu vpu;
	};
} __attribute__((__packed__));

#endif
