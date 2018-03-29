/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "mt_afe_debug.h"
#include "mt_afe_reg.h"
#include "mt_afe_clk.h"
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#define DEBUG_FS_BUFFER_SIZE 4096

struct mt_soc_audio_debug_fs {
	struct dentry *audio_dentry;
	char *fs_name;
	const struct file_operations *fops;
};

static ssize_t mt_soc_debug_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	const int size = DEBUG_FS_BUFFER_SIZE;
	char buffer[size];
	int n = 0;

	mt_afe_main_clk_on();

	n = scnprintf(buffer + n, size - n, "AUDIO_TOP_CON0 = 0x%x\n",
		      mt_afe_get_reg(AUDIO_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON1 = 0x%x\n",
		       mt_afe_get_reg(AUDIO_TOP_CON1));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON2 = 0x%x\n",
		       mt_afe_get_reg(AUDIO_TOP_CON2));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON3 = 0x%x\n",
		       mt_afe_get_reg(AUDIO_TOP_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_TOP_CON0 = 0x%x\n", mt_afe_get_reg(AFE_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON0 = 0x%x\n", mt_afe_get_reg(AFE_DAC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON1 = 0x%x\n", mt_afe_get_reg(AFE_DAC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON = 0x%x\n", mt_afe_get_reg(AFE_I2S_CON));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON1 = 0x%x\n", mt_afe_get_reg(AFE_I2S_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON2 = 0x%x\n", mt_afe_get_reg(AFE_I2S_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON3 = 0x%x\n", mt_afe_get_reg(AFE_I2S_CON3));

	n += scnprintf(buffer + n, size - n, "AFE_CONN0 = 0x%x\n", mt_afe_get_reg(AFE_CONN0));
	n += scnprintf(buffer + n, size - n, "AFE_CONN1 = 0x%x\n", mt_afe_get_reg(AFE_CONN1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN2 = 0x%x\n", mt_afe_get_reg(AFE_CONN2));
	n += scnprintf(buffer + n, size - n, "AFE_CONN3 = 0x%x\n", mt_afe_get_reg(AFE_CONN3));
	n += scnprintf(buffer + n, size - n, "AFE_CONN4 = 0x%x\n", mt_afe_get_reg(AFE_CONN4));
	n += scnprintf(buffer + n, size - n, "AFE_CONN5 = 0x%x\n", mt_afe_get_reg(AFE_CONN5));
	n += scnprintf(buffer + n, size - n, "AFE_CONN6 = 0x%x\n", mt_afe_get_reg(AFE_CONN6));
	n += scnprintf(buffer + n, size - n, "AFE_CONN7 = 0x%x\n", mt_afe_get_reg(AFE_CONN7));
	n += scnprintf(buffer + n, size - n, "AFE_CONN8 = 0x%x\n", mt_afe_get_reg(AFE_CONN8));
	n += scnprintf(buffer + n, size - n, "AFE_CONN9 = 0x%x\n", mt_afe_get_reg(AFE_CONN9));
	n += scnprintf(buffer + n, size - n, "AFE_CONN_24BIT = 0x%x\n",
		       mt_afe_get_reg(AFE_CONN_24BIT));

	n += scnprintf(buffer + n, size - n, "AFE_DAIBT_CON0 = 0x%x\n",
		       mt_afe_get_reg(AFE_DAIBT_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_CON = 0x%x\n",
		       mt_afe_get_reg(AFE_MRGIF_CON));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_MON0 = 0x%x\n",
		       mt_afe_get_reg(AFE_MRGIF_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_MON1 = 0x%x\n",
		       mt_afe_get_reg(AFE_MRGIF_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_MON2 = 0x%x\n",
		       mt_afe_get_reg(AFE_MRGIF_MON2));

	n += scnprintf(buffer + n, size - n, "AFE_DL1_BASE = 0x%x\n", mt_afe_get_reg(AFE_DL1_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_CUR = 0x%x\n", mt_afe_get_reg(AFE_DL1_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_END = 0x%x\n", mt_afe_get_reg(AFE_DL1_END));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_BASE = 0x%x\n", mt_afe_get_reg(AFE_DL2_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_CUR = 0x%x\n", mt_afe_get_reg(AFE_DL2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_END = 0x%x\n", mt_afe_get_reg(AFE_DL2_END));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_BASE = 0x%x\n", mt_afe_get_reg(AFE_AWB_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_END = 0x%x\n", mt_afe_get_reg(AFE_AWB_END));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_CUR = 0x%x\n", mt_afe_get_reg(AFE_AWB_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_BASE = 0x%x\n", mt_afe_get_reg(AFE_VUL_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_END = 0x%x\n", mt_afe_get_reg(AFE_VUL_END));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_CUR = 0x%x\n", mt_afe_get_reg(AFE_VUL_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DAI_BASE = 0x%x\n", mt_afe_get_reg(AFE_DAI_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DAI_END = 0x%x\n", mt_afe_get_reg(AFE_DAI_END));
	n += scnprintf(buffer + n, size - n, "AFE_DAI_CUR = 0x%x\n", mt_afe_get_reg(AFE_DAI_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_D2_BASE = 0x%x\n",
		       mt_afe_get_reg(AFE_DL1_D2_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_D2_END = 0x%x\n",
		       mt_afe_get_reg(AFE_DL1_D2_END));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_D2_CUR = 0x%x\n",
		       mt_afe_get_reg(AFE_DL1_D2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_D2_BASE = 0x%x\n",
		       mt_afe_get_reg(AFE_VUL_D2_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_D2_END = 0x%x\n",
		       mt_afe_get_reg(AFE_VUL_D2_END));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_D2_CUR = 0x%x\n",
		       mt_afe_get_reg(AFE_VUL_D2_CUR));

	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MSB = 0x%x\n",
		       mt_afe_get_reg(AFE_MEMIF_MSB));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON0 = 0x%x\n",
		       mt_afe_get_reg(AFE_MEMIF_MON0));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON1 = 0x%x\n",
		       mt_afe_get_reg(AFE_MEMIF_MON1));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON2 = 0x%x\n",
		       mt_afe_get_reg(AFE_MEMIF_MON2));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON4 = 0x%x\n",
		       mt_afe_get_reg(AFE_MEMIF_MON4));

	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SRC2_CON0 = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_DL_SRC2_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SRC2_CON1 = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_DL_SRC2_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_SRC_CON0 = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_UL_SRC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_SRC_CON1 = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_UL_SRC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_TOP_CON0 = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_DL_CON0 = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_UL_DL_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_SRC_DEBUG));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG_MON0 = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_SRC_DEBUG_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG_MON1 = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_SRC_DEBUG_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_NEWIF_CFG0 = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_NEWIF_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_NEWIF_CFG1 = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_NEWIF_CFG1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA2_TOP_CON0 = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA2_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_PREDIS_CON0 = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_PREDIS_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_PREDIS_CON1 = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_PREDIS_CON1));

	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CON = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ_MCU_CON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_STATUS = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ_MCU_STATUS));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CLR = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ_MCU_CLR));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT1 = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ_MCU_CNT1));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT2 = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ_MCU_CNT2));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_EN = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ_MCU_EN));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_MON2 = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ_MCU_MON2));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT5 = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ_MCU_CNT5));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ1_MCU_CNT_MON = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ1_MCU_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ2_MCU_CNT_MON = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ2_MCU_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ1_MCU_EN_CNT_MON = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ1_MCU_EN_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ5_MCU_CNT_MON = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ5_MCU_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MAXLEN = 0x%x\n",
		       mt_afe_get_reg(AFE_MEMIF_MAXLEN));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_PBUF_SIZE = 0x%x\n",
		       mt_afe_get_reg(AFE_MEMIF_PBUF_SIZE));

	n += scnprintf(buffer + n, size - n, "SIDETONE_DEBUG = 0x%x\n",
		       mt_afe_get_reg(AFE_SIDETONE_DEBUG));
	n += scnprintf(buffer + n, size - n, "SIDETONE_MON = 0x%x\n",
		       mt_afe_get_reg(AFE_SIDETONE_MON));
	n += scnprintf(buffer + n, size - n, "SIDETONE_CON0 = 0x%x\n",
		       mt_afe_get_reg(AFE_SIDETONE_CON0));
	n += scnprintf(buffer + n, size - n, "SIDETONE_COEFF = 0x%x\n",
		       mt_afe_get_reg(AFE_SIDETONE_COEFF));
	n += scnprintf(buffer + n, size - n, "SIDETONE_CON1 = 0x%x\n",
		       mt_afe_get_reg(AFE_SIDETONE_CON1));
	n += scnprintf(buffer + n, size - n, "SIDETONE_GAIN = 0x%x\n",
		       mt_afe_get_reg(AFE_SIDETONE_GAIN));
	n += scnprintf(buffer + n, size - n, "SGEN_CON0 = 0x%x\n", mt_afe_get_reg(AFE_SGEN_CON0));
	n += scnprintf(buffer + n, size - n, "SGEN_CON1 = 0x%x\n", mt_afe_get_reg(AFE_SGEN_CON1));

	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON0 = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN1_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON1 = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN1_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON2 = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN1_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON3 = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN1_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CONN = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN1_CONN));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CUR = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN1_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON0 = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN2_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON1 = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN2_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON2 = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN2_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON3 = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN2_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CONN = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN2_CONN));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CUR = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CONN2 = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN2_CONN2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CONN3 = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN2_CONN3));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CONN2 = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN1_CONN2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CONN3 = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN1_CONN3));

	n += scnprintf(buffer + n, size - n, "DCM_CFG = 0x%x\n",
		       mt_afe_topck_get_reg(AUDIO_DCM_CFG));
	n += scnprintf(buffer + n, size - n, "CLK_CFG_4 = 0x%x\n",
		       mt_afe_topck_get_reg(AUDIO_CLK_CFG_4));
	n += scnprintf(buffer + n, size - n, "SCP_AUDIO_PWR_CON = 0x%x\n",
		       mt_afe_spm_get_reg(SCP_AUDIO_PWR_CON));

#if 0
	n += scnprintf(buffer + n, size - n, "FPGA_CFG2  = 0x%x\n", mt_afe_get_reg(FPGA_CFG2));
	n += scnprintf(buffer + n, size - n, "FPGA_CFG3  = 0x%x\n", mt_afe_get_reg(FPGA_CFG3));
	n += scnprintf(buffer + n, size - n, "FPGA_CFG0  = 0x%x\n", mt_afe_get_reg(FPGA_CFG0));
	n += scnprintf(buffer + n, size - n, "FPGA_CFG1  = 0x%x\n", mt_afe_get_reg(FPGA_CFG1));
	n += scnprintf(buffer + n, size - n, "FPGA_STC  = 0x%x\n", mt_afe_get_reg(FPGA_STC));

#endif
	n += scnprintf(buffer + n, size - n, "AFE_MOD_DAI_BASE = 0x%x\n",
		       mt_afe_get_reg(AFE_MOD_DAI_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_MOD_DAI_END = 0x%x\n",
		       mt_afe_get_reg(AFE_MOD_DAI_END));
	n += scnprintf(buffer + n, size - n, "AFE_MOD_DAI_CUR = 0x%x\n",
		       mt_afe_get_reg(AFE_MOD_DAI_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON0  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON1  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON2  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON3  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON4  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON4));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON5  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON5));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON6  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON6));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON7  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON7));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON8  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON8));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON9  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON9));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON10  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON10));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON11  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON11));
	n += scnprintf(buffer + n, size - n, "PCM_INTF_CON1 = 0x%x\n",
		       mt_afe_get_reg(PCM_INTF_CON1));
	n += scnprintf(buffer + n, size - n, "PCM_INTF_CON2 = 0x%x\n",
		       mt_afe_get_reg(PCM_INTF_CON2));
	n += scnprintf(buffer + n, size - n, "PCM2_INTF_CON = 0x%x\n",
		       mt_afe_get_reg(PCM2_INTF_CON));

	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON13  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON13));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON14  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON14));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON15  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON15));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON16  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON16));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON17  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON17));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON18  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON18));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON19  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON19));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON20  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON20));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON21  = 0x%x\n",
		       mt_afe_get_reg(AFE_ASRC_CON21));

	mt_afe_main_clk_off();

	return simple_read_from_buffer(buf, count, pos, buffer, n);
}

static ssize_t mt_soc_debug_write(struct file *file, const char __user *user_buf,
			size_t count, loff_t *pos)
{
	char buf[64];
	size_t buf_size;
	char *start = buf;
	char *reg_str;
	char *value_str;
	const char delim[] = " ,";
	unsigned long reg, value;

	buf_size = min(count, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	buf[buf_size] = 0;

	reg_str = strsep(&start, delim);
	if (!reg_str || !strlen(reg_str))
		return -EINVAL;

	value_str = strsep(&start, delim);
	if (!value_str || !strlen(value_str))
		return -EINVAL;

	if (kstrtoul(reg_str, 16, &reg))
		return -EINVAL;

	if (kstrtoul(value_str, 16, &value))
		return -EINVAL;

	mt_afe_main_clk_on();
	mt_afe_set_reg(reg, value, 0xffffffff);
	mt_afe_main_clk_off();

	return buf_size;
}

static ssize_t mt_soc_hdmi_debug_read(struct file *file, char __user *buf,
				size_t count, loff_t *pos)
{
	const int size = DEBUG_FS_BUFFER_SIZE;
	char buffer[size];
	int n = 0;

	mt_afe_main_clk_on();

	n = scnprintf(buffer + n, size - n, "AUDIO_TOP_CON0 = 0x%x\n",
		       mt_afe_get_reg(AUDIO_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON3 = 0x%x\n",
		       mt_afe_get_reg(AUDIO_TOP_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON0 = 0x%x\n",
		       mt_afe_get_reg(AFE_DAC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_HDMI_OUT_CON0 = 0x%x\n",
		       mt_afe_get_reg(AFE_HDMI_OUT_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_HDMI_OUT_BASE = 0x%x\n",
		       mt_afe_get_reg(AFE_HDMI_OUT_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_HDMI_OUT_CUR = 0x%x\n",
		       mt_afe_get_reg(AFE_HDMI_OUT_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_HDMI_OUT_END = 0x%x\n",
		       mt_afe_get_reg(AFE_HDMI_OUT_END));
	n += scnprintf(buffer + n, size - n, "AFE_HDMI_CONN0 = 0x%x\n",
		       mt_afe_get_reg(AFE_HDMI_CONN0));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT5 = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ_MCU_CNT5));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CON = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ_MCU_CON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_STATUS = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ_MCU_STATUS));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_PBUF_SIZE = 0x%x\n",
		       mt_afe_get_reg(AFE_MEMIF_PBUF_SIZE));
	n += scnprintf(buffer + n, size - n, "AFE_APLL1_TUNER_CFG = 0x%x\n",
		       mt_afe_get_reg(AFE_APLL1_TUNER_CFG));
	n += scnprintf(buffer + n, size - n, "AFE_APLL2_TUNER_CFG = 0x%x\n",
		       mt_afe_get_reg(AFE_APLL2_TUNER_CFG));
	n += scnprintf(buffer + n, size - n, "AFE_TDM_CON1 = 0x%x\n",
		       mt_afe_get_reg(AFE_TDM_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_TDM_CON2 = 0x%x\n",
		       mt_afe_get_reg(AFE_TDM_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MSB = 0x%x\n",
		       mt_afe_get_reg(AFE_MEMIF_MSB));

	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_CFG_6 = 0x%x\n",
		       mt_afe_topck_get_reg(AUDIO_CLK_CFG_6));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_CFG_7 = 0x%x\n",
		       mt_afe_topck_get_reg(AUDIO_CLK_CFG_7));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUDDIV_0 = 0x%x\n",
		       mt_afe_topck_get_reg(AUDIO_CLK_AUDDIV_0));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUDDIV_1 = 0x%x\n",
		       mt_afe_topck_get_reg(AUDIO_CLK_AUDDIV_1));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUDDIV_2 = 0x%x\n",
		       mt_afe_topck_get_reg(AUDIO_CLK_AUDDIV_2));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUDDIV_3 = 0x%x\n",
		       mt_afe_topck_get_reg(AUDIO_CLK_AUDDIV_3));

	n += scnprintf(buffer + n, size - n, "AP_PLL_CON5 = 0x%x\n",
		       mt_afe_pll_get_reg(AP_PLL_CON5));
	n += scnprintf(buffer + n, size - n, "APLL1_CON0 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDIO_APLL1_CON0));
	n += scnprintf(buffer + n, size - n, "APLL1_CON1 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDIO_APLL1_CON1));
	n += scnprintf(buffer + n, size - n, "APLL1_CON2 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDIO_APLL1_CON2));
	n += scnprintf(buffer + n, size - n, "APLL1_PWR_CON0 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDIO_APLL1_PWR_CON0));
	n += scnprintf(buffer + n, size - n, "APLL2_CON0 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDIO_APLL2_CON0));
	n += scnprintf(buffer + n, size - n, "APLL2_CON1 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDIO_APLL2_CON1));
	n += scnprintf(buffer + n, size - n, "APLL2_CON2 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDIO_APLL2_CON2));
	n += scnprintf(buffer + n, size - n, "APLL2_PWR_CON0 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDIO_APLL2_PWR_CON0));

	mt_afe_main_clk_off();

	return simple_read_from_buffer(buf, count, pos, buffer, n);
}

static ssize_t mt_soc_spdif_debug_read(struct file *file, char __user *buf,
				size_t count, loff_t *pos)
{
	const int size = DEBUG_FS_BUFFER_SIZE;
	char buffer[size];
	int n = 0;

	mt_afe_main_clk_on();

	n = scnprintf(buffer + n, size - n, "AUDIO_TOP_CON0 = 0x%x\n",
		       mt_afe_get_reg(AUDIO_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON3 = 0x%x\n",
		       mt_afe_get_reg(AUDIO_TOP_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON0 = 0x%x\n",
		       mt_afe_get_reg(AFE_DAC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_HDMI_CONN0 = 0x%x\n",
		       mt_afe_get_reg(AFE_HDMI_CONN0));
	n += scnprintf(buffer + n, size - n, "AFE_HDMI_CONN1 = 0x%x\n",
		       mt_afe_get_reg(AFE_HDMI_CONN1));

	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CON = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ_MCU_CON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_STATUS = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ_MCU_STATUS));

	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_PBUF_SIZE = 0x%x\n",
		       mt_afe_get_reg(AFE_MEMIF_PBUF_SIZE));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_PBUF2_SIZE = 0x%x\n",
		       mt_afe_get_reg(AFE_MEMIF_PBUF2_SIZE));
	n += scnprintf(buffer + n, size - n, "AFE_APLL1_TUNER_CFG = 0x%x\n",
		       mt_afe_get_reg(AFE_APLL1_TUNER_CFG));
	n += scnprintf(buffer + n, size - n, "AFE_APLL2_TUNER_CFG = 0x%x\n",
		       mt_afe_get_reg(AFE_APLL2_TUNER_CFG));

	n += scnprintf(buffer + n, size - n, "AFE_SPDIF_OUT_CON0 = 0x%x\n",
		       mt_afe_get_reg(AFE_SPDIF_OUT_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_SPDIF_BASE = 0x%x\n",
		       mt_afe_get_reg(AFE_SPDIF_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_SPDIF_CUR = 0x%x\n",
		       mt_afe_get_reg(AFE_SPDIF_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_SPDIF_END = 0x%x\n",
		       mt_afe_get_reg(AFE_SPDIF_END));

	n += scnprintf(buffer + n, size - n, "AFE_IEC_CFG = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC_CFG));
	n += scnprintf(buffer + n, size - n, "AFE_IEC_NSNUM = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC_NSNUM));
	n += scnprintf(buffer + n, size - n, "AFE_IEC_BURST_INFO = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC_BURST_INFO));
	n += scnprintf(buffer + n, size - n, "AFE_IEC_BURST_LEN = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC_BURST_LEN));
	n += scnprintf(buffer + n, size - n, "AFE_IEC_NSADR = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC_NSADR));
	n += scnprintf(buffer + n, size - n, "AFE_IEC_CHL_STAT0 = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC_CHL_STAT0));
	n += scnprintf(buffer + n, size - n, "AFE_IEC_CHL_STAT1 = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC_CHL_STAT1));
	n += scnprintf(buffer + n, size - n, "AFE_IEC_CHR_STAT0 = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC_CHR_STAT0));
	n += scnprintf(buffer + n, size - n, "AFE_IEC_CHR_STAT1 = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC_CHR_STAT1));

	n += scnprintf(buffer + n, size - n, "AFE_SPDIF2_OUT_CON0 = 0x%x\n",
		       mt_afe_get_reg(AFE_SPDIF2_OUT_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_SPDIF2_BASE = 0x%x\n",
		       mt_afe_get_reg(AFE_SPDIF2_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_SPDIF2_CUR = 0x%x\n",
		       mt_afe_get_reg(AFE_SPDIF2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_SPDIF2_END = 0x%x\n",
		       mt_afe_get_reg(AFE_SPDIF2_END));

	n += scnprintf(buffer + n, size - n, "AFE_IEC2_CFG = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC2_CFG));
	n += scnprintf(buffer + n, size - n, "AFE_IEC2_NSNUM = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC2_NSNUM));
	n += scnprintf(buffer + n, size - n, "AFE_IEC2_BURST_INFO = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC2_BURST_INFO));
	n += scnprintf(buffer + n, size - n, "AFE_IEC2_BURST_LEN = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC2_BURST_LEN));
	n += scnprintf(buffer + n, size - n, "AFE_IEC2_NSADR = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC2_NSADR));
	n += scnprintf(buffer + n, size - n, "AFE_IEC2_CHL_STAT0 = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC2_CHL_STAT0));
	n += scnprintf(buffer + n, size - n, "AFE_IEC2_CHL_STAT1 = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC2_CHL_STAT1));
	n += scnprintf(buffer + n, size - n, "AFE_IEC2_CHR_STAT0 = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC2_CHR_STAT0));
	n += scnprintf(buffer + n, size - n, "AFE_IEC2_CHR_STAT1 = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC2_CHR_STAT1));

	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MSB = 0x%x\n",
		       mt_afe_get_reg(AFE_MEMIF_MSB));

	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_CFG_6 = 0x%x\n",
		       mt_afe_topck_get_reg(AUDIO_CLK_CFG_6));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_CFG_7 = 0x%x\n",
		       mt_afe_topck_get_reg(AUDIO_CLK_CFG_7));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUDDIV_0 = 0x%x\n",
		       mt_afe_topck_get_reg(AUDIO_CLK_AUDDIV_0));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUDDIV_3 = 0x%x\n",
		       mt_afe_topck_get_reg(AUDIO_CLK_AUDDIV_3));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUDDIV_4 = 0x%x\n",
		       mt_afe_topck_get_reg(AUDIO_CLK_AUDDIV_4));

	n += scnprintf(buffer + n, size - n, "AP_PLL_CON5 = 0x%x\n",
		       mt_afe_pll_get_reg(AP_PLL_CON5));
	n += scnprintf(buffer + n, size - n, "APLL1_CON0 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDIO_APLL1_CON0));
	n += scnprintf(buffer + n, size - n, "APLL1_CON1 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDIO_APLL1_CON1));
	n += scnprintf(buffer + n, size - n, "APLL1_CON2 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDIO_APLL1_CON2));
	n += scnprintf(buffer + n, size - n, "APLL1_PWR_CON0 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDIO_APLL1_PWR_CON0));
	n += scnprintf(buffer + n, size - n, "APLL2_CON0 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDIO_APLL2_CON0));
	n += scnprintf(buffer + n, size - n, "APLL2_CON1 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDIO_APLL2_CON1));
	n += scnprintf(buffer + n, size - n, "APLL2_CON2 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDIO_APLL2_CON2));
	n += scnprintf(buffer + n, size - n, "APLL2_PWR_CON0 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDIO_APLL2_PWR_CON0));

	mt_afe_main_clk_off();
	return simple_read_from_buffer(buf, count, pos, buffer, n);
}

static const struct file_operations mtaudio_debug_ops = {
	.open = simple_open,
	.read = mt_soc_debug_read,
	.write = mt_soc_debug_write,
	.llseek = default_llseek,
};

static const struct file_operations mtaudio_hdmi_debug_ops = {
	.open = simple_open,
	.read = mt_soc_hdmi_debug_read,
	.llseek = default_llseek,
};

static const struct file_operations mtaudio_spdif_debug_ops = {
	.open = simple_open,
	.read = mt_soc_spdif_debug_read,
	.llseek = default_llseek,
};

static struct mt_soc_audio_debug_fs audio_debug_fs[] = {
	{NULL, "mtksocaudio", &mtaudio_debug_ops},
	{NULL, "mtksochdmiaudio", &mtaudio_hdmi_debug_ops},
	{NULL, "mtksocspdifaudio", &mtaudio_spdif_debug_ops},
};

void mt_afe_debug_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(audio_debug_fs); i++) {
		audio_debug_fs[i].audio_dentry = debugfs_create_file(audio_debug_fs[i].fs_name,
							0644, NULL, NULL,
							audio_debug_fs[i].fops);
		if (!audio_debug_fs[i].audio_dentry)
			pr_warn("%s failed to create %s debugfs file\n", __func__,
				audio_debug_fs[i].fs_name);
	}
}

void mt_afe_debug_deinit(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(audio_debug_fs); i++)
		debugfs_remove(audio_debug_fs[i].audio_dentry);
}
