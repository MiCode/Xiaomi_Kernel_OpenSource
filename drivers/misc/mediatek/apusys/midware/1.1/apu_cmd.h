// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_APU_CMD_H__
#define __APUSYS_APU_CMD_H__

#define APUSYS_MAGIC_NUMBER 0x3d2070ece309c231
#define APUSYS_CMD_VERSION 0x1

#define FLAG_SUBGRAPH_FD_MAP (1UL << 31)

#define HDR_FLAG_MASK_POWER_SAVE (1ULL << 0)
#define HDR_FLAG_MASK_FENCE_EXEC (1ULL << 1)
#define HDR_FLAG_MASK_MULTI (3ULL << 62)
#define HDR_FLAG_MASK_STATUS_OFS (2)
#define HDR_FlAG_MASK_STATUS_BMP (0xFULL << HDR_FLAG_MASK_STATUS_OFS)

enum {
	HDR_FLAG_MULTI_SCHED = 0,
	HDR_FLAG_MULTI_SINGLE = 1,
	HDR_FLAG_MULTI_MULTI = 2,
};

enum {
	HDR_FLAG_EXEC_STATUS_OK = 0,
	HDR_FLAG_EXEC_STATUS_ABORT = 1,
	HDR_FLAG_EXEC_STATUS_HWERROR = 2,
};

#define VALUE_SUBGRAPH_PACK_ID_NONE      0
#define VALUE_SUBGRAPH_CTX_ID_NONE       0
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

struct apu_cmd_hdr {
	unsigned long long magic;
	unsigned long long uid;
	unsigned char version;
	unsigned char priority;
	unsigned short hard_limit;
	unsigned short soft_limit;
	unsigned short pid;
	unsigned long long flags;
	unsigned int num_sc;
	unsigned int ofs_scr_list;     // successor list offset
	unsigned int ofs_pdr_cnt_list; // predecessor count list offset
	unsigned int scofs_list_entry; // subcmd offset's list offset
} __attribute__((__packed__));

struct apu_sc_hdr_cmn {
	unsigned int type;
	unsigned int driver_time;
	unsigned int ip_time;
	unsigned int suggest_time;
	unsigned int bandwidth;
	unsigned int tcm_usage;
	unsigned char tcm_force;
	unsigned char boost_val;
	unsigned char pack_id;
	unsigned char reserved;
	unsigned int mem_ctx;
	unsigned int cb_info_size; // codebuf info size
	unsigned int ofs_cb_info;  // codebuf info offset
} __attribute__((__packed__));

struct apu_fence_hdr {
	unsigned long long fd;
	unsigned int total_time;
	unsigned int status;
} __attribute__((__packed__));

struct apu_mdla_hdr {
	unsigned int ofs_codebuf_info_dual0;
	unsigned int ofs_codebuf_info_dual1;
	unsigned int ofs_pmu_info;
} __attribute__((__packed__));

#endif
