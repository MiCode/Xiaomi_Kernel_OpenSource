/*
 * Copyright (C) 2016 MediaTek Inc.
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
#ifndef _MTK_CTT_H_
#define _MTK_CTT_H_

#define CTT_NUM 4

#define CTT_V101 0x400
#define CTT_V127 0x468

/************************************************
 * config enum
 ************************************************/
enum CTT_KEY {
	CTT_CFG,
	CTT_R_CFG,
	CTT_IMAX,
	CTT_R_IMAX,
	CTT_TMAX,
	CTT_R_TMAX,
	CTT_IMAX_DELTA,
	CTT_R_IMAX_DELTA,
	CTT_TMAX_DELTA,
	CTT_R_TMAX_DELTA,

	CTT_R_TARGETSCALEOUT,
	CTT_R_STARTUPDONE,
	CTT_R_STATE,
	CTT_R_CURRENT,
	CTT_R_TEMP,
	CTT_R_LCODE,
	CTT_R,

	NR_CTT,
};

enum CTT_TRIGGER_STAGE {
	CTT_TRIGGER_STAGE_PROBE,
	CTT_TRIGGER_STAGE_SUSPEND,
	CTT_TRIGGER_STAGE_RESUME,

	NR_CTT_TRIGGER_STAGE,
};

struct ctt_class {
	/* CTT_V101 0x0C532400, 2C00, 3400, 3C00 */
	unsigned int safeFreqReqOverride:1;   /* [0] */
	unsigned int globalEventEn:1;         /* [1] */
	unsigned int testMode:4;              /* [5:2] */
	unsigned int cttEn:1;                 /* [6] */
	unsigned int fllSlowReqGateEn:1;      /* [7] */
	unsigned int fllSlowReqEn:1;          /* [8] */
	unsigned int fllClkOutSelect:1;       /* [9] */
	unsigned int fllEn:1;                 /* [10] */
	unsigned int configComplete:1;        /* [11] */
	unsigned int drccGateEn:1;            /* [12] */
	unsigned int samplerEn:1;             /* [13] */
	unsigned int CttV101:18;                 /* [14:31] */

	/* CTT_V127 0x0C532468, 2C68, 3468, 3C68 */
	unsigned int CttTmax:8;               /* [7:0] */
	unsigned int CttImax:13;              /* [20:8] */
	unsigned int CttVoltage:10;           /* [30:21] */
	unsigned int CttV127:1;                  /* [31] */

	/* CTT_V128 0x0C53246C, 2C6C, 346C, 3C6C */
	unsigned int CttDynScale:8;           /* [7:0] */
	unsigned int CttLkgScale:8;           /* [15:8] */
	unsigned int CttV128:16;                 /* [31:16] */

	/* CTT_V129 0x0C532470, 2C70, 3470, 3C70 */
	unsigned int CttVcoef:8;              /* [7:0] */
	unsigned int CttTcoef2:8;             /* [15:8] */
	unsigned int CttTcoef1:8;             /* [23:16] */
	unsigned int CttV129:8;                  /* [31:24] */

	/* CTT_V130 0x0C532474, 2C74, 3474, 3C74 */
	unsigned int CttLkgIrefTrim:4;        /* [3:0] */
	unsigned int CttTcal:8;               /* [11:4] */
	unsigned int CttVcal:10;              /* [21:12] */
	unsigned int CttLcal:10;              /* [31:22] */

	/* CTT_V131 0x0C532478, 2C78, 3478, 3C78 */
	unsigned int CttFllNR:1;              /* [0] */
	unsigned int CttMpmmEn:1;             /* [1] */
	unsigned int CttImaxDisableEn:1;      /* [2] */
	unsigned int CttIRQStatus:2;          /* [4:3] */
	unsigned int CttIRQClear:2;           /* [6:5] */
	unsigned int CttIRQEn:2;              /* [8:7] */
	unsigned int CttVoltageMin:10;        /* [18:9] */
	unsigned int CttTargetScaleMin:7;     /* [25:19] */
	unsigned int CttC131:6;                  /* [31:26] */

	/* CTT_V132 0x0C53247C, 2C7C, 347C, 3C7C */
	unsigned int CttTmaxIncPwr2:5;        /* [4:0] */
	unsigned int CttImaxIncPwr2:5;        /* [9:5] */
	unsigned int CttTmaxDelta:5;          /* [14:10] */
	unsigned int CttImaxDelta:11;         /* [25:15] */
	unsigned int CttV132:6;                  /* [31:26] */

	/* CTT_V133 0x0C532480, 2C80, 3480, 3C80 */
	unsigned int CttState:4;              /* [3:0] */
	unsigned int CttLkgCode_r:8;          /* [11:4] */
	unsigned int LKG_INCR:1;              /* [12] */
	unsigned int CttStartupDone:1;        /* [13] */
	unsigned int CttTargetScaleOut:7;     /* [20:14] */
	unsigned int CttLcode:10;             /* [30:21] */
	unsigned int CttV133:1;                  /* [31] */

	/* CTT_V134 0x0C532484, 2C84, 3484, 3C84 */
	unsigned int CttTemp:8;               /* [7:0] */
	unsigned int CttCurrent:14;           /* [21:8] */
	unsigned int CttV134:10;                 /* [31:22] */

	/* CTT_V135 0x0C532488, 2C88, 3488, 3C88 */
	unsigned int CttMpmmGear1:13;         /* [12:0] */
	unsigned int CttMpmmGear0:13;         /* [25:13] */
	unsigned int CttV135:6;                  /* [31:26] */

	/* CTT_V136 0x0C53248C, 2C8C, 348C, 3C8C */
	unsigned int CttMpmmGear2:13;         /* [12:0] */
	unsigned int CttV136:19;                 /* [31:13] */

	/* CTT_V137 0x0C532490, 2C90, 3490, 3C90 */
	unsigned int CttLkgCode_w:8;          /* [7:0] */
	unsigned int CttLkgRst:1;             /* [8] */
	unsigned int CttLkgTestEn:1;          /* [9] */
	unsigned int CttLkgClk:1;             /* [10] */
	unsigned int CttLkgStartup:1;         /* [11] */
	unsigned int CttLkgEnable:1;          /* [12] */
	unsigned int CttLkgControl:16;        /* [28:13] */
	unsigned int CttV137:3;                  /* [31:29] */

	/* CTT_V138 0x0C532494, 2C94, 3494, 3C94 */
	unsigned int CttDacBistCode:8;        /* [7:0] */
	unsigned int CttDacBistSingle:1;      /* [8] */
	unsigned int CttDacBistLoopPwr2:4;    /* [12:9] */
	unsigned int CttIrefBistLoopPwr2:4;   /* [16:13] */
	unsigned int CttCapBistLoopCount:12;  /* [28:17] */
	unsigned int CttV138:3;                  /* [31:29] */

	/* CTT_V139 0x0C532498, 2C98, 3498, 3C98 */
	unsigned int CttDacBistTarget:16;     /* [15:0] */
	unsigned int CttIrefBistTarget:16;    /* [31:16] */

	/* CTT_V140 0x0C53249C, 2C9C, 349C, 3C9C */
	unsigned int CttDacBistZone0Target:8; /* [7:0] */
	unsigned int CttDacBistZone1Target:8; /* [15:8] */
	unsigned int CttDacBistZone2Target:8; /* [23:16] */
	unsigned int CttDacBistZone3Target:8; /* [31:24] */

	/* CTT_V141 0x0C5324A0, 2CA0, 34A0, 3CA0 */
	unsigned int CttBistDone:1;           /* [0] */
	unsigned int CttDacBistFailCount:9;   /* [9:1] */
	unsigned int CttDacBistMaxFail:1;     /* [10] */
	unsigned int CttDacBistZeroFail:1;    /* [11] */
	unsigned int CttDacBistDone:1;        /* [12] */
	unsigned int CttIrefBistTrim:4;       /* [16:13] */
	unsigned int CttIrefBistMaxFail:1;    /* [17] */
	unsigned int CttIrefBistMinFail:1;    /* [18] */
	unsigned int CttIrefBistDone:1;       /* [19] */
	unsigned int CttCapBistMaxFail:1;     /* [20] */
	unsigned int CttCapBistZeroFail:1;    /* [21] */
	unsigned int CttCapBistDone:1;        /* [22] */
	unsigned int CttV141:9;                  /* [31:23] */

	/* CTT_V142 0x0C5324A4, 2CA4, 34A4, 3CA4 */
	unsigned int CttDacBistCount:16;      /* [15:0] */
	unsigned int CttCapBistCount:16;      /* [31:16] */
};

/************************************************
 * Func prototype
 ************************************************/
int ctt_resume(struct platform_device *pdev);
int ctt_suspend(struct platform_device *pdev, pm_message_t state);
int ctt_probe(struct platform_device *pdev);
int ctt_create_procfs(const char *proc_name, struct proc_dir_entry *dir);
int ctt_reserve_memory_dump(char *buf, unsigned long long ptp3_mem_size,
	enum CTT_TRIGGER_STAGE ctt_tri_stage);
void ctt_save_memory_info(char *buf, unsigned long long ptp3_mem_size);
#endif //_MTK_CTT_H_

