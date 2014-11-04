#ifndef __GP_DEVICE_GLOBAL_H_INCLUDED__
#define __GP_DEVICE_GLOBAL_H_INCLUDED__

#define IS_GP_DEVICE_VERSION_1

/* The SP configures FIFO switches in these registers */
#define _REG_GP_SWITCH_IF_ADDR				0x00
#define _REG_GP_SWITCH_DMA_ADDR				0x04
#define _REG_GP_SWITCH_GDC_ADDR				0x08

/* The SP sends SW interrupt info to this register */
#define _REG_GP_IRQ_REQUEST_ADDR			0x98

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

#define _REG_GP_MIPI_USED_DWORD_ADDR		0xA0

#define _REG_GP_IFMT_input_switch_lut_reg0			_REG_GP_INP_SWI_LUT_REG_0_ADDR
#define _REG_GP_IFMT_input_switch_lut_reg1			_REG_GP_INP_SWI_LUT_REG_1_ADDR
#define _REG_GP_IFMT_input_switch_lut_reg2			_REG_GP_INP_SWI_LUT_REG_2_ADDR
#define _REG_GP_IFMT_input_switch_lut_reg3			_REG_GP_INP_SWI_LUT_REG_3_ADDR
#define _REG_GP_IFMT_input_switch_lut_reg4			_REG_GP_INP_SWI_LUT_REG_4_ADDR
#define _REG_GP_IFMT_input_switch_lut_reg5			_REG_GP_INP_SWI_LUT_REG_5_ADDR
#define _REG_GP_IFMT_input_switch_lut_reg6			_REG_GP_INP_SWI_LUT_REG_6_ADDR
#define _REG_GP_IFMT_input_switch_lut_reg7			_REG_GP_INP_SWI_LUT_REG_7_ADDR
#define _REG_GP_IFMT_input_switch_fsync_lut			_REG_GP_INP_SWI_FSYNC_LUT_REG_ADDR
#define _REG_GP_IFMT_input_switch_ch_id_fmt_type	0xffffffff

#endif /* __GP_DEVICE_GLOBAL_H_INCLUDED__ */
