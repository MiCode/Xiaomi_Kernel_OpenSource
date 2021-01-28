/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <mtk_spm_vcore_dvfs_ipi.h>
#include <mtk_vcorefs_governor.h>
#include <mtk_swpm_common.h>
#include <mtk_swpm_platform.h>


/****************************************************************************
 *  Macro Definitions
 ****************************************************************************/

/****************************************************************************
 *  Type Definitions
 ****************************************************************************/

/****************************************************************************
 *  Local Variables
 ****************************************************************************/
static struct aphy_pwr_data aphy_def_pwr_tbl[] = {
	[APHY_VCORE_0P7V] = {
		.read_pwr = {
			[DDR_800] = {
				.bw = {768, 1184, 2208, 3328, 5120},
				.coef = {6021, 7460, 8812, 10059, 12059},
			},
			[DDR_1200] = {
				.bw = {768, 1184, 2240, 3328, 7872},
				.coef = {7680, 9168, 12136, 12877, 17375},
			},
			[DDR_1600] = {
				.bw = {768, 1152, 2208, 3360, 9216},
				.coef = {13299, 15269, 19891, 22394, 31044},
			},
			[DDR_1800] = {
				.bw = {768, 1152, 2208, 3360, 9216},
				.coef = {11084, 12554, 16135, 18389, 25918},
			},
		},
		.write_pwr = {
			[DDR_800] = {
				.bw = {768, 1184, 2208, 3328, 5120},
				.coef = {6021, 7460, 8812, 10059, 12059},
			},
			[DDR_1200] = {
				.bw = {768, 1184, 2240, 3328, 7872},
				.coef = {7680, 9168, 12136, 12877, 17375},
			},
			[DDR_1600] = {
				.bw = {768, 1152, 2208, 3360, 9216},
				.coef = {13299, 15269, 19891, 22394, 31044},
			},
			[DDR_1800] = {
				.bw = {768, 1152, 2208, 3360, 9216},
				.coef = {11084, 12554, 16135, 18389, 25918},
			},
		},
		.coef_idle = {4269, 4665, 6785, 5545},
	},
	[APHY_VDDQ_0P6V] = {
		.read_pwr = {
			[DDR_800] = {
				.bw = {64, 320, 640, 1280, 1920},
				.coef = {148, 603, 1025, 1283, 1568},
			},
			[DDR_1200] = {
				.bw = {98, 480, 960, 1920, 2880},
				.coef = {167, 680, 1175, 1352, 1703},
			},
			[DDR_1600] = {
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {98, 350, 673, 770, 988},
			},
			[DDR_1800] = {
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {98, 353, 667, 763, 1032},
			},
		},
		.write_pwr = {
			[DDR_800] = {
				.bw = {64, 640, 1280, 1920, 3200},
				.coef = {305, 2188, 3300, 4327, 6127},
			},
			[DDR_1200] = {
				.bw = {98, 960, 1920, 2880, 4800},
				.coef = {350, 2522, 3857, 5075, 7317},
			},
			[DDR_1600] = {
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {398, 2843, 3227, 5687, 7587},
			},
			[DDR_1800] = {
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {433, 1842, 3458, 6303, 8503},
			},
		},
		.coef_idle = {12, 12, 13, 13},
	},
	[APHY_VM_1P1V] = {
		.read_pwr = {
			[DDR_800] = {
				.bw = {64, 320, 640, 1280, 1920},
				.coef = {107, 325, 491, 602, 674},
			},
			[DDR_1200] = {
				.bw = {98, 480, 960, 1920, 2880},
				.coef = {166, 504, 821, 888, 1017},
			},
			[DDR_1600] = {
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {235, 663, 1095, 1221, 1388},
			},
			[DDR_1800] = {
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {248, 708, 1173, 1291, 1527},
			},
		},
		.write_pwr = {
			[DDR_800] = {
				.bw = {64, 640, 1280, 1920, 3200},
				.coef = {174, 958, 1376, 1673, 2332},
			},
			[DDR_1200] = {
				.bw = {98, 960, 1920, 2880, 4800},
				.coef = {260, 1415, 2011, 2434, 3380},
			},
			[DDR_1600] = {
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {356, 1200, 1905, 2686, 3255},
			},
			[DDR_1800] = {
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {377, 1223, 2045, 2891, 3645},
			},
		},
		.coef_idle = {55, 55, 55, 55},
	},
	[APHY_VIO_1P8V] = {
		.read_pwr = {
			[DDR_800] = {
				.bw = {64, 320, 640, 1280, 1920},
				.coef = {30, 139, 268, 448, 543},
			},
			[DDR_1200] = {
				.bw = {98, 480, 960, 1920, 2880},
				.coef = {33, 153, 296, 457, 575},
			},
			[DDR_1600] = {
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {35, 158, 303, 452, 568},
			},
			[DDR_1800] = {
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {37, 162, 307, 444, 547},
			},
		},
		.write_pwr = {
			[DDR_800] = {
				.bw = {64, 640, 1280, 1920, 3200},
				.coef = {0, 0, 0, 0, 0},
			},
			[DDR_1200] = {
				.bw = {98, 960, 1920, 2880, 4800},
				.coef = {0, 0, 0, 0, 0},
			},
			[DDR_1600] = {
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {0, 0, 0, 0, 0},
			},
			[DDR_1800] = {
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {0, 0, 0, 0, 0},
			},
		},
		.coef_idle = {99, 199, 209, 211},
	},
};

static struct dram_pwr_conf dram_def_pwr_conf[] = {
	[DRAM_VDD1_1P8V] = {
		.i_dd0 = 10500,
		.i_dd2p = 600,
		.i_dd2n = 900,
		.i_dd4r = 14700,
		.i_dd4w = 16000,
		.i_dd5 = 4100,
		.i_dd6 = 900,
	},
	[DRAM_VDD2_1P1V] = {
		.i_dd0 = 48400,
		.i_dd2p = 600,
		.i_dd2n = 21100,
		.i_dd4r = 125700,
		.i_dd4w = 171500,
		.i_dd5 = 27100,
		.i_dd6 = 1100,
	},
	[DRAM_VDDQ_0P6V] = {
		.i_dd0 = 200,
		.i_dd2p = 200,
		.i_dd2n = 200,
		.i_dd4r = 153100,
		.i_dd4w = 600,
		.i_dd5 = 200,
		.i_dd6 = 100,
	},
};

/****************************************************************************
 *  Global Variables
 ****************************************************************************/

/****************************************************************************
 *  Static Function
 ****************************************************************************/

/***************************************************************************
 *  API
 ***************************************************************************/
char *swpm_power_rail_to_string(enum power_rail p)
{
	char *s;

	switch (p) {
	case VPROC12:
		s = "VPROC12";
		break;
	case VPROC11:
		s = "VPROC11";
		break;
	case VGPU:
		s = "VGPU";
		break;
	case VCORE:
		s = "VCORE";
		break;
	case VDRAM1:
		s = "VDRAM1";
		break;
	case VIO18_DDR:
		s = "VIO18_DDR";
		break;
	case VIO18_DRAM:
		s = "VIO18_DRAM";
		break;
	default:
		s = "None";
		break;
	}

	return s;
}

int swpm_platform_init(void)
{
	int ret = 0;

	/* copy pwr data */
	memcpy(swpm_info_ref->aphy_pwr_tbl, aphy_def_pwr_tbl,
		sizeof(aphy_def_pwr_tbl));
	memcpy(swpm_info_ref->dram_conf, dram_def_pwr_conf,
		sizeof(dram_def_pwr_conf));

	swpm_info("copy pwr data (size: aphy/dram = %ld/%ld) done!\n",
		(unsigned long)sizeof(aphy_def_pwr_tbl),
		(unsigned long)sizeof(dram_def_pwr_conf));

	return ret;
}

void swpm_send_init_ipi(unsigned int addr, unsigned int size,
	unsigned int ch_num)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	struct qos_data qos_d;

	qos_d.cmd = QOS_IPI_SWPM_INIT;
	qos_d.u.swpm_init.dram_addr = addr;
	qos_d.u.swpm_init.dram_size = size;
	qos_d.u.swpm_init.dram_ch_num = ch_num;
	qos_ipi_to_sspm_command(&qos_d, 4);
#endif
}

void swpm_set_enable(unsigned int type, unsigned int enable)
{
}

void swpm_set_update_cnt(unsigned int type, unsigned int cnt)
{
}

void swpm_update_lkg_table(void)
{
}

