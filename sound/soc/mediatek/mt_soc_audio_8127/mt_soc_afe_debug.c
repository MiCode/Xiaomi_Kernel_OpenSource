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

#include "mt_soc_afe_debug.h"
#include "mt_soc_afe_reg.h"
#include "mt_soc_afe_clk.h"
#include "mt_soc_codec_63xx.h"

#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <mt-plat/upmu_common.h>

#define DEBUG_FS_BUFFER_SIZE 4096
static const char const ParSetkeyAfe[] = "Setafereg";
static const char const ParSetkeyAna[] = "Setanareg";
static const char const ParSetkeyCfg[] = "Setcfgreg";
static const char const ParSetkeyApm[] = "Setapmreg";
static const char const PareGetkeyAfe[] = "Getafereg";
static const char const PareGetkeyAna[] = "Getanareg";
static const char const PareGetkeyApm[] = "Getapmreg";


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

	mt_afe_ana_clk_on();
	mt_afe_main_clk_on();

	pr_debug("mt_soc_debug_read\n");

	n = scnprintf(buffer, size - n, "mt_soc_debug_read\n");
/*
	n += scnprintf(buffer + n, size - n, "aud_afe_clk_cntr = 0x%x\n", aud_afe_clk_cntr);
	n += scnprintf(buffer + n, size - n, "aud_dac_clk_cntr = 0x%x\n", aud_dac_clk_cntr);
	n += scnprintf(buffer + n, size - n, "aud_adc_clk_cntr = 0x%x\n", aud_adc_clk_cntr);
	n += scnprintf(buffer + n, size - n, "aud_i2s_clk_cntr = 0x%x\n", aud_i2s_clk_cntr);
	n += scnprintf(buffer + n, size - n, "aud_hdmi_clk_cntr = 0x%x\n", aud_hdmi_clk_cntr);
	n += scnprintf(buffer + n, size - n, "aud_spdif_clk_cntr = 0x%x\n", aud_spdif_clk_cntr);
	n += scnprintf(buffer + n, size - n, "aud_apll1_tuner_cntr = 0x%x\n", aud_apll1_tuner_cntr);
	n += scnprintf(buffer + n, size - n, "aud_apll2_tuner_cntr = 0x%x\n", aud_apll2_tuner_cntr);
	n += scnprintf(buffer + n, size - n, "aud_emi_clk_cntr = 0x%x\n", aud_emi_clk_cntr);*/
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON0  = 0x%x\n",
		       mt_afe_get_reg(AUDIO_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON1  = 0x%x\n",
		       mt_afe_get_reg(AUDIO_TOP_CON1));
	n += scnprintf(buffer+n, size-n, "AUDIO_TOP_CON2  = 0x%x\n", mt_afe_get_reg(AUDIO_TOP_CON2));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON3  = 0x%x\n",
		       mt_afe_get_reg(AUDIO_TOP_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON0  = 0x%x\n", mt_afe_get_reg(AFE_DAC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON1  = 0x%x\n", mt_afe_get_reg(AFE_DAC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON  = 0x%x\n", mt_afe_get_reg(AFE_I2S_CON));
	n += scnprintf(buffer + n, size - n, "AFE_CONN0  = 0x%x\n", mt_afe_get_reg(AFE_CONN0));
	n += scnprintf(buffer + n, size - n, "AFE_CONN1  = 0x%x\n", mt_afe_get_reg(AFE_CONN1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN2  = 0x%x\n", mt_afe_get_reg(AFE_CONN2));
	n += scnprintf(buffer + n, size - n, "AFE_CONN3  = 0x%x\n", mt_afe_get_reg(AFE_CONN3));
	n += scnprintf(buffer + n, size - n, "AFE_CONN4  = 0x%x\n", mt_afe_get_reg(AFE_CONN4));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON1  = 0x%x\n", mt_afe_get_reg(AFE_I2S_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON2  = 0x%x\n", mt_afe_get_reg(AFE_I2S_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_BASE  = 0x%x\n", mt_afe_get_reg(AFE_DL1_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_CUR  = 0x%x\n", mt_afe_get_reg(AFE_DL1_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_END  = 0x%x\n", mt_afe_get_reg(AFE_DL1_END));

	n += scnprintf(buffer+n, size-n, "AFE_I2S_CON3  = 0x%x\n", mt_afe_get_reg(AFE_I2S_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_BASE  = 0x%x\n", mt_afe_get_reg(AFE_DL2_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_CUR  = 0x%x\n", mt_afe_get_reg(AFE_DL2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_END  = 0x%x\n", mt_afe_get_reg(AFE_DL2_END));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_BASE  = 0x%x\n", mt_afe_get_reg(AFE_AWB_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_END  = 0x%x\n", mt_afe_get_reg(AFE_AWB_END));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_CUR  = 0x%x\n", mt_afe_get_reg(AFE_AWB_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_BASE  = 0x%x\n", mt_afe_get_reg(AFE_VUL_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_END  = 0x%x\n", mt_afe_get_reg(AFE_VUL_END));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_CUR  = 0x%x\n", mt_afe_get_reg(AFE_VUL_CUR));

	n += scnprintf(buffer + n, size - n, "MEMIF_MON0 = 0x%x\n", mt_afe_get_reg(AFE_MEMIF_MON0));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON1 = 0x%x\n", mt_afe_get_reg(AFE_MEMIF_MON1));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON2 = 0x%x\n", mt_afe_get_reg(AFE_MEMIF_MON2));
	n += scnprintf(buffer + n, size - n, "MEMIF_MON4 = 0x%x\n", mt_afe_get_reg(AFE_MEMIF_MON4));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SRC2_CON0  = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_DL_SRC2_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SRC2_CON1  = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_DL_SRC2_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_SRC_CON0  = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_UL_SRC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_SRC_CON1  = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_UL_SRC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_TOP_CON0  = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_DL_CON0  = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_UL_DL_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG  = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_SRC_DEBUG));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG_MON0  = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_SRC_DEBUG_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG_MON1  = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_SRC_DEBUG_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_NEWIF_CFG0  = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_NEWIF_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_NEWIF_CFG1  = 0x%x\n",
		       mt_afe_get_reg(AFE_ADDA_NEWIF_CFG1));
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

	n += scnprintf(buffer + n, size - n, "AFE_TOP_CON0 = 0x%x\n", mt_afe_get_reg(AFE_TOP_CON0));

	n += scnprintf(buffer + n, size - n, "HDMI_OUT_CON0  = 0x%x\n",
		       mt_afe_get_reg(AFE_HDMI_OUT_CON0));
	n += scnprintf(buffer + n, size - n, "HDMI_OUT_BASE  = 0x%x\n",
		       mt_afe_get_reg(AFE_HDMI_OUT_BASE));
	n += scnprintf(buffer + n, size - n, "HDMI_OUT_CUR  = 0x%x\n",
		       mt_afe_get_reg(AFE_HDMI_OUT_CUR));
	n += scnprintf(buffer + n, size - n, "HDMI_OUT_END  = 0x%x\n",
		       mt_afe_get_reg(AFE_HDMI_OUT_END));
	n += scnprintf(buffer + n, size - n, "SPDIF_OUT_CON0  = 0x%x\n",
		       mt_afe_get_reg(AFE_SPDIF_OUT_CON0));
	n += scnprintf(buffer + n, size - n, "SPDIF_BASE  = 0x%x\n",
		       mt_afe_get_reg(AFE_SPDIF_BASE));
	n += scnprintf(buffer + n, size - n, "SPDIF_CUR  = 0x%x\n",
		       mt_afe_get_reg(AFE_SPDIF_CUR));
	n += scnprintf(buffer + n, size - n, "SPDIF_END  = 0x%x\n",
		       mt_afe_get_reg(AFE_SPDIF_END));
	n += scnprintf(buffer + n, size - n, "HDMI_CONN0  = 0x%x\n",
		       mt_afe_get_reg(AFE_HDMI_CONN0));
	n += scnprintf(buffer + n, size - n, "8CH_I2S_OUT_CON = 0x%x\n",
		       mt_afe_get_reg(AFE_8CH_I2S_OUT_CON));
	n += scnprintf(buffer + n, size - n, "IEC_CFG  = 0x%x\n", mt_afe_get_reg(AFE_IEC_CFG));
	n += scnprintf(buffer + n, size - n, "IEC_NSNUM  = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC_NSNUM));
	n += scnprintf(buffer + n, size - n, "IEC_BURST_INFO  = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC_BURST_INFO));
	n += scnprintf(buffer + n, size - n, "IEC_BURST_LEN  = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC_BURST_LEN));
	n += scnprintf(buffer + n, size - n, "IEC_NSADR  = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC_NSADR));
	n += scnprintf(buffer + n, size - n, "IEC_CHL_STAT0  = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC_CHL_STAT0));
	n += scnprintf(buffer + n, size - n, "IEC_CHL_STAT1  = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC_CHL_STAT1));
	n += scnprintf(buffer + n, size - n, "IEC_CHR_STAT0  = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC_CHR_STAT0));
	n += scnprintf(buffer + n, size - n, "IEC_CHR_STAT1  = 0x%x\n",
		       mt_afe_get_reg(AFE_IEC_CHR_STAT1));
	n += scnprintf(buffer+n, size-n, "AFE_MOD_PCM_BASE = 0x%x\n", mt_afe_get_reg(AFE_MOD_DAI_BASE));
	n += scnprintf(buffer+n, size-n, "AFE_MOD_PCM_END = 0x%x\n", mt_afe_get_reg(AFE_MOD_DAI_END));
	n += scnprintf(buffer+n, size-n, "AFE_MOD_PCM_CUR = 0x%x\n", mt_afe_get_reg(AFE_MOD_DAI_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CON = 0x%x\n",
		mt_afe_get_reg(AFE_IRQ_MCU_CON));	/* ccc */

	n += scnprintf(buffer + n, size - n, "AFE_IRQ_STATUS = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ_MCU_STATUS));

	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CLR = 0x%x\n", mt_afe_get_reg(AFE_IRQ_MCU_CLR));

	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT1 = 0x%x\n", mt_afe_get_reg(AFE_IRQ_MCU_CNT1));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT2 = 0x%x\n", mt_afe_get_reg(AFE_IRQ_MCU_CNT2));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_MON2 = 0x%x\n", mt_afe_get_reg(AFE_IRQ_MCU_MON2));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT5 = 0x%x\n", mt_afe_get_reg(AFE_IRQ_MCU_CNT5));

	n += scnprintf(buffer + n, size - n, "AFE_IRQ1_EN_CNT_MON = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ1_MCU_EN_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ5_EN_CNT_MON = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ5_MCU_EN_CNT_MON));

	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MAXLEN  = 0x%x\n",
		       mt_afe_get_reg(AFE_MEMIF_MAXLEN));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_PBUF_SIZE  = 0x%x\n",
		       mt_afe_get_reg(AFE_MEMIF_PBUF_SIZE));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON0  = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN1_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON1  = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN1_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON2  = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN1_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON3  = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN1_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CONN  = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN1_CONN));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CUR  = 0x%x\n", mt_afe_get_reg(AFE_GAIN1_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON0  = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN2_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON1  = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN2_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON2  = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN2_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON3  = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN2_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CONN  = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN2_CONN));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CONN2  = 0x%x\n",
		       mt_afe_get_reg(AFE_GAIN2_CONN2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CUR  = 0x%x\n",
		mt_afe_get_reg(AFE_GAIN2_CUR));
	n += scnprintf(buffer+n, size-n, "FPGA_CFG2  = 0x%x\n", mt_afe_get_reg(FPGA_CFG2));
	n += scnprintf(buffer+n, size-n, "FPGA_CFG3  = 0x%x\n", mt_afe_get_reg(FPGA_CFG3));
	n += scnprintf(buffer+n, size-n, "FPGA_CFG0  = 0x%x\n", mt_afe_get_reg(FPGA_CFG0));
	n += scnprintf(buffer+n, size-n, "FPGA_CFG1  = 0x%x\n", mt_afe_get_reg(FPGA_CFG1));
	n += scnprintf(buffer+n, size-n, "FPGA_STC  = 0x%x\n", mt_afe_get_reg(FPGA_STC));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON0  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON0));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON1  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON1));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON2  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON2));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON3  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON3));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON4  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON4));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON5  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON5));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON6  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON6));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON7  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON7));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON8  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON8));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON9  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON9));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON10  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON10));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON11  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON11));
	n += scnprintf(buffer+n, size-n, "PCM_INTF_CON1 = 0x%x\n", mt_afe_get_reg(PCM_INTF_CON1));
	n += scnprintf(buffer+n, size-n, "PCM_INTF_CON2 = 0x%x\n", mt_afe_get_reg(PCM_INTF_CON2));
	n += scnprintf(buffer+n, size-n, "PCM2_INTF_CON = 0x%x\n", mt_afe_get_reg(PCM2_INTF_CON));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON13  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON13));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON14  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON14));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON15  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON15));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON16  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON16));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON17  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON17));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON18  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON18));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON19  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON19));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON20  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON20));
	n += scnprintf(buffer+n, size-n, "AFE_ASRC_CON21  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON21));
	n += scnprintf(buffer + n, size - n, "=PMIC registers=\n");
	/* PMIC Digital Register */
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON0 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON0));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON1 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON1));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON2 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON2));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON3 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON3));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON4 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON4));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON5 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON5));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON6 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON6));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON7 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON7));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON8 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON8));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON9 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON9));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON10 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON10));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON11 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON11));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_STA0 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_STA0));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_STA1 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_STA1));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_STA2 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_STA2));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_UP8X_FIFO_CFG0 = 0x%x\n",
		pmic_get_ana_reg(ABB_AFE_UP8X_FIFO_CFG0));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_UP8X_FIFO_LOG_MON0 = 0x%x\n",
		pmic_get_ana_reg(ABB_AFE_UP8X_FIFO_LOG_MON0));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_UP8X_FIFO_LOG_MON1 = 0x%x\n",
		pmic_get_ana_reg(ABB_AFE_UP8X_FIFO_LOG_MON1));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_PMIC_NEWIF_CFG0 = 0x%x\n",
		pmic_get_ana_reg(ABB_AFE_PMIC_NEWIF_CFG0));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_PMIC_NEWIF_CFG1 = 0x%x\n",
		pmic_get_ana_reg(ABB_AFE_PMIC_NEWIF_CFG1));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_PMIC_NEWIF_CFG2 = 0x%x\n",
		pmic_get_ana_reg(ABB_AFE_PMIC_NEWIF_CFG2));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_PMIC_NEWIF_CFG3 = 0x%x\n",
		pmic_get_ana_reg(ABB_AFE_PMIC_NEWIF_CFG3));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_TOP_CON0 = 0x%x\n",
		pmic_get_ana_reg(ABB_AFE_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_MON_DEBUG0 = 0x%x\n",
		pmic_get_ana_reg(ABB_AFE_MON_DEBUG0));
	/* PMIC Analog Register */
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN0 = 0x%x\n", pmic_get_ana_reg(TOP_CKPDN0));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN0_SET = 0x%x\n", pmic_get_ana_reg(TOP_CKPDN0_SET));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN0_CLR = 0x%x\n", pmic_get_ana_reg(TOP_CKPDN0_CLR));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN1 = 0x%x\n", pmic_get_ana_reg(TOP_CKPDN1));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN1_SET = 0x%x\n", pmic_get_ana_reg(TOP_CKPDN1_SET));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN1_CLR = 0x%x\n", pmic_get_ana_reg(TOP_CKPDN1_CLR));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN2 = 0x%x\n", pmic_get_ana_reg(TOP_CKPDN2));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN2_SET = 0x%x\n", pmic_get_ana_reg(TOP_CKPDN2_SET));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN2_CLR = 0x%x\n", pmic_get_ana_reg(TOP_CKPDN2_CLR));
	n += scnprintf(buffer + n, size - n, "TOP_CKCON1 = 0x%x\n", pmic_get_ana_reg(TOP_CKCON1));
	n += scnprintf(buffer + n, size - n, "SPK_CON0 = 0x%x\n", pmic_get_ana_reg(SPK_CON0));
	n += scnprintf(buffer + n, size - n, "SPK_CON1 = 0x%x\n", pmic_get_ana_reg(SPK_CON1));
	n += scnprintf(buffer + n, size - n, "SPK_CON2 = 0x%x\n", pmic_get_ana_reg(SPK_CON2));
	n += scnprintf(buffer + n, size - n, "SPK_CON6 = 0x%x\n", pmic_get_ana_reg(SPK_CON6));
	n += scnprintf(buffer + n, size - n, "SPK_CON7 = 0x%x\n", pmic_get_ana_reg(SPK_CON7));
	n += scnprintf(buffer + n, size - n, "SPK_CON8 = 0x%x\n", pmic_get_ana_reg(SPK_CON8));
	n += scnprintf(buffer + n, size - n, "SPK_CON9 = 0x%x\n", pmic_get_ana_reg(SPK_CON9));
	n += scnprintf(buffer + n, size - n, "SPK_CON10 = 0x%x\n", pmic_get_ana_reg(SPK_CON10));
	n += scnprintf(buffer + n, size - n, "SPK_CON11 = 0x%x\n", pmic_get_ana_reg(SPK_CON11));
	n += scnprintf(buffer + n, size - n, "SPK_CON12 = 0x%x\n", pmic_get_ana_reg(SPK_CON12));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON0 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON0));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON1 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON1));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON2 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON2));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON3 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON3));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON4 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON4));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON5 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON5));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON6 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON6));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON7 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON7));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON8 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON8));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON9 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON9));

	pr_notice("mt_soc_debug_read len = %d\n", n);


	pr_debug("mt_soc_debug_read len = %d\n", n);
	mt_afe_main_clk_off();
	mt_afe_ana_clk_off();

	return simple_read_from_buffer(buf, count, pos, buffer, n);
}

static ssize_t mt_soc_debug_write(struct file *file, const char __user *user_buf,
			size_t count, loff_t *pos)
{
	int ret = 0;
	char InputString[256];
	char *token1 = NULL;
	char *token2 = NULL;
	char *token3 = NULL;
	char *token4 = NULL;
	char *token5 = NULL;
	char *temp = NULL;

	unsigned long int regaddr = 0;
	unsigned long int regvalue = 0;
	char delim[] = " ,";

	memset((void *)InputString, 0, 256);
	if (copy_from_user((InputString), user_buf, count)) {
		pr_debug("copy_from_user mt_soc_debug_write count = %zu temp = %s\n",
		count, InputString);
	}
	temp = kstrdup(InputString, GFP_KERNEL);
	pr_debug("copy_from_user mt_soc_debug_write count = %zu temp = %s pointer = %p\n",
	count, InputString, InputString);
	token1 = strsep(&temp, delim);
	pr_debug("token1 = %s\n", token1);
	token2 = strsep(&temp, delim);
	pr_debug("token2 = %s\n", token2);
	token3 = strsep(&temp, delim);
	pr_debug("token3 = %s\n", token3);
	token4 = strsep(&temp, delim);
	pr_debug("token4 = %s\n", token4);
	token5 = strsep(&temp, delim);
	pr_debug("token5 = %s\n", token5);

	mt_afe_ana_clk_on();
	mt_afe_main_clk_on();


	if (strcmp(token1, ParSetkeyAfe) == 0) {
		pr_debug("strcmp (token1,ParSetkeyAfe)\n");
		ret = kstrtol(token3, 16, &regaddr);
		ret = kstrtol(token5, 16, &regvalue);
		pr_debug("%s regaddr = 0x%lx regvalue = 0x%lx\n", ParSetkeyAfe, regaddr, regvalue);
		mt_afe_set_reg(regaddr, regvalue, 0xffffffff);
		regvalue = mt_afe_get_reg(regaddr);
		pr_debug("%s regaddr = 0x%lx regvalue = 0x%lx\n", ParSetkeyAfe, regaddr, regvalue);
	}
	if (strcmp(token1, ParSetkeyAna) == 0) {
		pr_debug("strcmp (token1,ParSetkeyAna)\n");
		ret = kstrtol(token3, 16, &regaddr);
		ret = kstrtol(token5, 16, &regvalue);
		pr_debug("%s regaddr = 0x%lx regvalue = 0x%lx\n", ParSetkeyAna, regaddr, regvalue);
		pmic_set_ana_reg(regaddr, regvalue, 0xffffffff);
		regvalue = pmic_get_ana_reg(regaddr);
		pr_debug("%s regaddr = 0x%lx regvalue = 0x%lx\n", ParSetkeyAna, regaddr, regvalue);
	}
	if (strcmp(token1, ParSetkeyCfg) == 0) {
		pr_debug("strcmp (token1,ParSetkeyCfg)\n");
		ret = kstrtol(token3, 16, &regaddr);
		ret = kstrtol(token5, 16, &regvalue);
		pr_debug("%s regaddr = 0x%lx regvalue = 0x%lx\n", ParSetkeyCfg, regaddr, regvalue);
		mt_afe_topck_set_reg(regaddr, regvalue, 0xffffffff);
		regvalue = mt_afe_topck_get_reg(regaddr);
		pr_debug("%s regaddr = 0x%lx regvalue = 0x%lx\n", ParSetkeyCfg, regaddr, regvalue);
	}
	if (strcmp(token1, ParSetkeyApm) == 0) {
		pr_debug("strcmp (token1,ParSetkeyApm)\n");
		ret = kstrtol(token3, 16, &regaddr);
		ret = kstrtol(token5, 16, &regvalue);
		pr_debug("%s regaddr = 0x%lx regvalue = 0x%lx\n", ParSetkeyApm, regaddr, regvalue);
		mt_afe_pll_set_reg(regaddr, regvalue, 0xffffffff);
		regvalue = mt_afe_pll_get_reg(regaddr);
		pr_debug("%s regaddr = 0x%lx regvalue = 0x%lx\n", ParSetkeyApm, regaddr, regvalue);
	}

	if (strcmp(token1, PareGetkeyAfe) == 0) {
		pr_debug("strcmp (token1,PareGetkeyAfe)\n");
		ret = kstrtol(token3, 16, &regaddr);
		regvalue = mt_afe_get_reg(regaddr);
		pr_debug("%s regaddr = 0x%lx regvalue = 0x%lx\n", PareGetkeyAfe, regaddr, regvalue);
	}
	if (strcmp(token1, PareGetkeyAna) == 0) {
		pr_debug("strcmp (token1,PareGetkeyAna)\n");
		ret = kstrtol(token3, 16, &regaddr);
		regvalue = pmic_get_ana_reg(regaddr);
		pr_debug("%s regaddr = 0x%lx regvalue = 0x%lx\n", PareGetkeyAna, regaddr, regvalue);
	}
	if (strcmp(token1, PareGetkeyApm) == 0) {
		pr_debug("strcmp (token1,PareGetkeyApm)\n");
		ret = kstrtol(token3, 16, &regaddr);
		regvalue = mt_afe_pll_get_reg(regaddr);
		pr_debug("%s regaddr = 0x%lx regvalue = 0x%lx\n", PareGetkeyApm, regaddr, regvalue);
	}

	mt_afe_main_clk_off();
	mt_afe_ana_clk_off();

	return count;
}

static ssize_t mt_soc_ana_debug_read(struct file *file, char __user *buf,
				     size_t count, loff_t *pos)
{
	const int size = DEBUG_FS_BUFFER_SIZE;
	char buffer[size];
	int n = 0;

	pr_debug("mt_soc_ana_debug_read count = %zu\n", count);
	mt_afe_ana_clk_on();
	mt_afe_main_clk_on();

	n += scnprintf(buffer + n, size - n, "=PMIC registers=\n");
    /* PMIC Digital Register */
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON0 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON0));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON1 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON1));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON2 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON2));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON3 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON3));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON4 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON4));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON5 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON5));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON6 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON6));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON7 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON7));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON8 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON8));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON9 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON9));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON10 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON10));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON11 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_CON11));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_STA0 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_STA0));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_STA1 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_STA1));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_STA2 = 0x%x\n", pmic_get_ana_reg(ABB_AFE_STA2));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_UP8X_FIFO_CFG0 = 0x%x\n",
		pmic_get_ana_reg(ABB_AFE_UP8X_FIFO_CFG0));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_UP8X_FIFO_LOG_MON0 = 0x%x\n",
		pmic_get_ana_reg(ABB_AFE_UP8X_FIFO_LOG_MON0));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_UP8X_FIFO_LOG_MON1 = 0x%x\n",
		pmic_get_ana_reg(ABB_AFE_UP8X_FIFO_LOG_MON1));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_PMIC_NEWIF_CFG0 = 0x%x\n",
		pmic_get_ana_reg(ABB_AFE_PMIC_NEWIF_CFG0));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_PMIC_NEWIF_CFG1 = 0x%x\n",
		pmic_get_ana_reg(ABB_AFE_PMIC_NEWIF_CFG1));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_PMIC_NEWIF_CFG2 = 0x%x\n",
		pmic_get_ana_reg(ABB_AFE_PMIC_NEWIF_CFG2));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_PMIC_NEWIF_CFG3 = 0x%x\n",
		pmic_get_ana_reg(ABB_AFE_PMIC_NEWIF_CFG3));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_TOP_CON0 = 0x%x\n",
		pmic_get_ana_reg(ABB_AFE_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_MON_DEBUG0 = 0x%x\n",
		pmic_get_ana_reg(ABB_AFE_MON_DEBUG0));
	/* PMIC Analog Register */
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN0 = 0x%x\n", pmic_get_ana_reg(TOP_CKPDN0));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN0_SET = 0x%x\n", pmic_get_ana_reg(TOP_CKPDN0_SET));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN0_CLR = 0x%x\n", pmic_get_ana_reg(TOP_CKPDN0_CLR));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN1 = 0x%x\n", pmic_get_ana_reg(TOP_CKPDN1));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN1_SET = 0x%x\n", pmic_get_ana_reg(TOP_CKPDN1_SET));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN1_CLR = 0x%x\n", pmic_get_ana_reg(TOP_CKPDN1_CLR));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN2 = 0x%x\n", pmic_get_ana_reg(TOP_CKPDN2));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN2_SET = 0x%x\n", pmic_get_ana_reg(TOP_CKPDN2_SET));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN2_CLR = 0x%x\n", pmic_get_ana_reg(TOP_CKPDN2_CLR));
	n += scnprintf(buffer + n, size - n, "TOP_CKCON1 = 0x%x\n", pmic_get_ana_reg(TOP_CKCON1));
	n += scnprintf(buffer + n, size - n, "SPK_CON0 = 0x%x\n", pmic_get_ana_reg(SPK_CON0));
	n += scnprintf(buffer + n, size - n, "SPK_CON1 = 0x%x\n", pmic_get_ana_reg(SPK_CON1));
	n += scnprintf(buffer + n, size - n, "SPK_CON2 = 0x%x\n", pmic_get_ana_reg(SPK_CON2));
	n += scnprintf(buffer + n, size - n, "SPK_CON6 = 0x%x\n", pmic_get_ana_reg(SPK_CON6));
	n += scnprintf(buffer + n, size - n, "SPK_CON7 = 0x%x\n", pmic_get_ana_reg(SPK_CON7));
	n += scnprintf(buffer + n, size - n, "SPK_CON8 = 0x%x\n", pmic_get_ana_reg(SPK_CON8));
	n += scnprintf(buffer + n, size - n, "SPK_CON9 = 0x%x\n", pmic_get_ana_reg(SPK_CON9));
	n += scnprintf(buffer + n, size - n, "SPK_CON10 = 0x%x\n", pmic_get_ana_reg(SPK_CON10));
	n += scnprintf(buffer + n, size - n, "SPK_CON11 = 0x%x\n", pmic_get_ana_reg(SPK_CON11));
	n += scnprintf(buffer + n, size - n, "SPK_CON12 = 0x%x\n", pmic_get_ana_reg(SPK_CON12));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON0 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON0));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON1 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON1));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON2 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON2));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON3 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON3));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON4 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON4));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON5 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON5));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON6 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON6));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON7 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON7));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON8 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON8));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON9 = 0x%x\n", pmic_get_ana_reg(AUDTOP_CON9));

	pr_notice("mt_soc_ana_debug_read len = %d\n", n);

	mt_afe_main_clk_off();
	mt_afe_ana_clk_off();

	return simple_read_from_buffer(buf, count, pos, buffer, n);
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
	n += scnprintf(buffer + n , size - n, "AFE_HDMI_CONN0      = 0x%x\n",
				mt_afe_get_reg(AFE_HDMI_CONN0));
	n += scnprintf(buffer + n , size - n, "AFE_8CH_I2S_OUT_CON = 0x%x\n",
				mt_afe_get_reg(AFE_8CH_I2S_OUT_CON));
	n += scnprintf(buffer + n , size - n, "AFE_HDMI_CONN1      = 0x%x\n",
				mt_afe_get_reg(AFE_HDMI_CONN1));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT5 = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ_MCU_CNT5));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CON = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ_MCU_CON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_STATUS = 0x%x\n",
		       mt_afe_get_reg(AFE_IRQ_MCU_STATUS));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_PBUF_SIZE = 0x%x\n",
		       mt_afe_get_reg(AFE_MEMIF_PBUF_SIZE));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MSB = 0x%x\n",
		       mt_afe_get_reg(AFE_MEMIF_MSB));

	n += scnprintf(buffer + n, size - n, "=APMIXED registers=\n");
	n += scnprintf(buffer + n, size - n, "AUDPLL_CON0 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDPLL_CON0));
	n += scnprintf(buffer + n, size - n, "AUDPLL_CON1 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDPLL_CON1));
	n += scnprintf(buffer + n, size - n, "AUDPLL_CON2 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDPLL_CON2));
	n += scnprintf(buffer + n, size - n, "AUDPLL_CON3 = 0x%x\n",
		       mt_afe_pll_get_reg(AUDPLL_CON3));
	n += scnprintf(buffer + n, size - n, "=TOPCKGEN registers=\n");
	n += scnprintf(buffer + n, size - n, "CLK_CFG_5 = 0x%x\n",
		       mt_afe_topck_get_reg(CLK_CFG_5));

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
	n += sprintf(buf + n , "HDMI_OUT_CON0   = 0x%x\n", mt_afe_get_reg(AFE_HDMI_OUT_CON0));
	n += sprintf(buf + n , "HDMI_OUT_BASE   = 0x%x\n", mt_afe_get_reg(AFE_HDMI_OUT_BASE));
	n += sprintf(buf + n , "HDMI_OUT_CUR    = 0x%x\n", mt_afe_get_reg(AFE_HDMI_OUT_CUR));
	n += sprintf(buf + n , "HDMI_OUT_END    = 0x%x\n", mt_afe_get_reg(AFE_HDMI_OUT_END));
	n += sprintf(buf + n , "SPDIF_OUT_CON0  = 0x%x\n", mt_afe_get_reg(AFE_SPDIF_OUT_CON0));
	n += sprintf(buf + n , "SPDIF_BASE      = 0x%x\n", mt_afe_get_reg(AFE_SPDIF_BASE));
	n += sprintf(buf + n , "SPDIF_CUR       = 0x%x\n", mt_afe_get_reg(AFE_SPDIF_CUR));
	n += sprintf(buf + n , "SPDIF_END       = 0x%x\n", mt_afe_get_reg(AFE_SPDIF_END));
	n += sprintf(buf + n , "HDMI_CONN0      = 0x%x\n", mt_afe_get_reg(AFE_HDMI_CONN0));
	n += sprintf(buf + n , "8CH_I2S_OUT_CON = 0x%x\n", mt_afe_get_reg(AFE_8CH_I2S_OUT_CON));
	n += sprintf(buf + n , "IEC_CFG         = 0x%x\n", mt_afe_get_reg(AFE_IEC_CFG));
	n += sprintf(buf + n , "IEC_NSNUM       = 0x%x\n", mt_afe_get_reg(AFE_IEC_NSNUM));
	n += sprintf(buf + n , "IEC_BURST_INFO  = 0x%x\n", mt_afe_get_reg(AFE_IEC_BURST_INFO));
	n += sprintf(buf + n , "IEC_BURST_LEN   = 0x%x\n", mt_afe_get_reg(AFE_IEC_BURST_LEN));
	n += sprintf(buf + n , "IEC_NSADR       = 0x%x\n", mt_afe_get_reg(AFE_IEC_NSADR));
	n += sprintf(buf + n , "IEC_CHL_STAT0   = 0x%x\n", mt_afe_get_reg(AFE_IEC_CHL_STAT0));
	n += sprintf(buf + n , "IEC_CHL_STAT1   = 0x%x\n", mt_afe_get_reg(AFE_IEC_CHL_STAT1));
	n += sprintf(buf + n , "IEC_CHR_STAT0   = 0x%x\n", mt_afe_get_reg(AFE_IEC_CHR_STAT0));
	n += sprintf(buf + n , "IEC_CHR_STAT1   = 0x%x\n", mt_afe_get_reg(AFE_IEC_CHR_STAT1));

	mt_afe_main_clk_off();
	return simple_read_from_buffer(buf, count, pos, buffer, n);
}

static const struct file_operations mtaudio_debug_ops = {
	.open = simple_open,
	.read = mt_soc_debug_read,
	.write = mt_soc_debug_write,
	.llseek = default_llseek,
};

static const struct file_operations mtaudio_ana_debug_ops = {
	.open = simple_open,
	.read = mt_soc_ana_debug_read,
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
	{NULL, "mtksocanaaudio", &mtaudio_ana_debug_ops},
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
