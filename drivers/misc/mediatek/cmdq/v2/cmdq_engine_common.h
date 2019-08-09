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

#ifndef __CMDQ_ENGINE_COMMON_H__
#define __CMDQ_ENGINE_COMMON_H__

enum CMDQ_ENG_ENUM {
	/* ISP */
	CMDQ_ENG_ISP_IMGI = 0,
	CMDQ_ENG_ISP_IMGO,  /* 1 */
	CMDQ_ENG_ISP_IMG2O, /* 2 */

	/* MDP */
	CMDQ_ENG_MDP_CAMIN,  /* 3 */
	CMDQ_ENG_MDP_RDMA0,  /* 4 */
	CMDQ_ENG_MDP_RDMA1,  /* 5 */
	CMDQ_ENG_MDP_RSZ0,   /* 6 */
	CMDQ_ENG_MDP_RSZ1,   /* 7 */
	CMDQ_ENG_MDP_RSZ2,   /* 8 */
	CMDQ_ENG_MDP_TDSHP0, /* 9 */
	CMDQ_ENG_MDP_TDSHP1, /* 10 */
	CMDQ_ENG_MDP_COLOR0, /* 11 */
	CMDQ_ENG_MDP_MOUT0,  /* 12 */
	CMDQ_ENG_MDP_MOUT1,  /* 13 */
	CMDQ_ENG_MDP_WROT0,  /* 14 */
	CMDQ_ENG_MDP_WROT1,  /* 15 */
	CMDQ_ENG_MDP_WDMA,   /* 16 */

	/* JPEG & VENC */
	CMDQ_ENG_JPEG_ENC,   /* 17 */
	CMDQ_ENG_VIDEO_ENC,  /* 18 */
	CMDQ_ENG_JPEG_DEC,   /* 19 */
	CMDQ_ENG_JPEG_REMDC, /* 20 */

	/* DISP */
	CMDQ_ENG_DISP_UFOE,     /* 21 */
	CMDQ_ENG_DISP_AAL,      /* 22 */
	CMDQ_ENG_DISP_COLOR0,   /* 23 */
	CMDQ_ENG_DISP_COLOR1,   /* 24 */
	CMDQ_ENG_DISP_RDMA0,    /* 25 */
	CMDQ_ENG_DISP_RDMA1,    /* 26 */
	CMDQ_ENG_DISP_RDMA2,    /* 27 */
	CMDQ_ENG_DISP_WDMA0,    /* 28 */
	CMDQ_ENG_DISP_WDMA1,    /* 29 */
	CMDQ_ENG_DISP_OVL0,     /* 30 */
	CMDQ_ENG_DISP_OVL1,     /* 31 */
	CMDQ_ENG_DISP_OVL2,     /* 32 */
	CMDQ_ENG_DISP_2L_OVL0,  /* 33 */
	CMDQ_ENG_DISP_2L_OVL1,  /* 34 */
	CMDQ_ENG_DISP_2L_OVL2,  /* 35 */
	CMDQ_ENG_DISP_GAMMA,    /* 36 */
	CMDQ_ENG_DISP_MERGE,    /* 37 */
	CMDQ_ENG_DISP_SPLIT0,   /* 38 */
	CMDQ_ENG_DISP_SPLIT1,   /* 39 */
	CMDQ_ENG_DISP_DSI0_VDO, /* 40 */
	CMDQ_ENG_DISP_DSI1_VDO, /* 41 */
	CMDQ_ENG_DISP_DSI0_CMD, /* 42 */
	CMDQ_ENG_DISP_DSI1_CMD, /* 43 */
	CMDQ_ENG_DISP_DSI0,     /* 44 */
	CMDQ_ENG_DISP_DSI1,     /* 45 */
	CMDQ_ENG_DISP_DPI,      /* 46 */

	/* DPE */
	CMDQ_ENG_DPE, /* 47 */

	/* temp: CMDQ internal usage */
	CMDQ_ENG_CMDQ,
	CMDQ_ENG_DISP_MUTEX,
	CMDQ_ENG_MMSYS_CONFIG,

	CMDQ_MAX_ENGINE_COUNT /* ALWAYS keep at the end */
};

#endif /* __CMDQ_ENGINE_COMMON_H__ */
