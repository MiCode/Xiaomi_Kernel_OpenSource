#ifndef __GP_DEVICE_LOCAL_H_INCLUDED__
#define __GP_DEVICE_LOCAL_H_INCLUDED__

#include "gp_device_global.h"
/*
#define _REG_GP_SWITCH_IF_ADDR				0x00
#define _REG_GP_SWITCH_DMA_ADDR				0x04
#define _REG_GP_SWITCH_GDC_ADDR				0x08
*/
/*
#define _REG_GP_SYNGEN_ENABLE_ADDR			0x0C
#define _REG_GP_SYNGEN_NR_PIX_ADDR			0x10
#define _REG_GP_SYNGEN_NR_LINES_ADDR		0x14
#define _REG_GP_SYNGEN_HBLANK_CYCLES_ADDR	0x18
#define _REG_GP_SYNGEN_VBLANK_CYCLES_ADDR	0x1C
#define _REG_GP_ISEL_SOF_ADDR				0x20
#define _REG_GP_ISEL_EOF_ADDR				0x24
#define _REG_GP_ISEL_SOL_ADDR				0x28
#define _REG_GP_ISEL_EOL_ADDR				0x2C
#define _REG_GP_ISEL_LFSR_ENABLE_ADDR		0x30
#define _REG_GP_ISEL_LFSR_ENABLE_B_ADDR		0x34
#define _REG_GP_ISEL_LFSR_RESET_VALUE_ADDR	0x38
#define _REG_GP_ISEL_TPG_ENABLE_ADDR		0x3C
#define _REG_GP_ISEL_TPG_ENABLE_B_ADDR		0x40
#define _REG_GP_ISEL_HOR_CNT_MASK_ADDR		0x44
#define _REG_GP_ISEL_VER_CNT_MASK_ADDR		0x48
#define _REG_GP_ISEL_XY_CNT_MASK_ADDR		0x4C
#define _REG_GP_ISEL_HOR_CNT_DELTA_ADDR		0x50
#define _REG_GP_ISEL_VER_CNT_DELTA_ADDR		0x54
#define _REG_GP_ISEL_CH_ID_ADDR				0x58
#define _REG_GP_ISEL_FMT_TYPE_ADDR			0x5C
#define _REG_GP_ISEL_DATA_SEL_ADDR			0x60
#define _REG_GP_ISEL_SBAND_SEL_ADDR			0x64
#define _REG_GP_ISEL_SYNC_SEL_ADDR			0x68
#define _REG_GP_INP_SWI_LUT_REG_0_ADDR		0x6C
#define _REG_GP_INP_SWI_LUT_REG_1_ADDR		0x70
#define _REG_GP_INP_SWI_LUT_REG_2_ADDR		0x74
#define _REG_GP_INP_SWI_LUT_REG_3_ADDR		0x78
#define _REG_GP_INP_SWI_LUT_REG_4_ADDR		0x7C
#define _REG_GP_INP_SWI_LUT_REG_5_ADDR		0x80
#define _REG_GP_INP_SWI_LUT_REG_6_ADDR		0x84
#define _REG_GP_INP_SWI_LUT_REG_7_ADDR		0x88
#define _REG_GP_INP_SWI_FSYNC_LUT_REG_ADDR	0x8C
#define _REG_GP_SDRAM_WAKEUP_ADDR			0x90
#define _REG_GP_IDLE_ADDR					0x94
*/
/* #define _REG_GP_IRQ_REQUEST_ADDR			0x98 */
#define _REG_GP_MIPI_DWORD_FULL_ADDR		0x9C
/* #define _REG_GP_MIPI_USED_DWORD_ADDR		0xA0 */

struct gp_device_state_s {
	int switch_if;
	int switch_dma;
	int switch_gdc;
	int syngen_enable;
	int syngen_nr_pix;
	int syngen_nr_lines;
	int syngen_hblank_cycles;
	int syngen_vblank_cycles;
	int isel_sof;
	int isel_eof;
	int isel_sol;
	int isel_eol;
	int isel_lfsr_enable;
	int isel_lfsr_enable_b;
	int isel_lfsr_reset_value;
	int isel_tpg_enable;
	int isel_tpg_enable_b;
	int isel_hor_cnt_mask;
	int isel_ver_cnt_mask;
	int isel_xy_cnt_mask;
	int isel_hor_cnt_delta;
	int isel_ver_cnt_delta;
	int isel_ch_id;
	int isel_fmt_type;
	int isel_data_sel;
	int isel_sband_sel;
	int isel_sync_sel;
	int inp_swi_lut_reg_0;
	int inp_swi_lut_reg_1;
	int inp_swi_lut_reg_2;
	int inp_swi_lut_reg_3;
	int inp_swi_lut_reg_4;
	int inp_swi_lut_reg_5;
	int inp_swi_lut_reg_6;
	int inp_swi_lut_reg_7;
	int inp_swi_fsync_lut_reg;
	int sdram_wakeup;
	int idle;
	int irq_request;
	int mipi_dword_full;
	int mipi_used_dword;
};

#endif /* __GP_DEVICE_LOCAL_H_INCLUDED__ */
