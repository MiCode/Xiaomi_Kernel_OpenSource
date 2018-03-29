/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#ifndef __MT_AFE_AUDIO_DIGITAL_TYPE_H__
#define __MT_AFE_AUDIO_DIGITAL_TYPE_H__

/*
 * ENUM DEFINITION
 */

enum mt_afe_digital_block {
	/* memory interfrace */
	MT_AFE_DIGITAL_BLOCK_MEM_DL1 = 0,
	MT_AFE_DIGITAL_BLOCK_MEM_DL2,
	MT_AFE_DIGITAL_BLOCK_MEM_VUL,
	MT_AFE_DIGITAL_BLOCK_MEM_DAI,
	MT_AFE_DIGITAL_BLOCK_MEM_I2S,
	MT_AFE_DIGITAL_BLOCK_MEM_AWB,
	MT_AFE_DIGITAL_BLOCK_MEM_MOD_DAI,
	MT_AFE_DIGITAL_BLOCK_MEM_DL1_DATA2,
	MT_AFE_DIGITAL_BLOCK_MEM_VUL_DATA2,
	/* connection to int main modem */
	MT_AFE_DIGITAL_BLOCK_MODEM_PCM_1_O,
	/* connection to extrt/int modem */
	MT_AFE_DIGITAL_BLOCK_MODEM_PCM_2_O,
	/* 1st I2S for DAC and ADC */
	MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC,
	MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC,
	MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC2,
	/* 2nd I2S */
	MT_AFE_DIGITAL_BLOCK_I2S_OUT_2,
	MT_AFE_DIGITAL_BLOCK_I2S_IN_2,
	/* HW gain contorl */
	MT_AFE_DIGITAL_BLOCK_HW_GAIN1,
	MT_AFE_DIGITAL_BLOCK_HW_GAIN2,
	/* megrge interface */
	MT_AFE_DIGITAL_BLOCK_MRG_I2S_OUT,
	MT_AFE_DIGITAL_BLOCK_MRG_I2S_IN,
	MT_AFE_DIGITAL_BLOCK_DAI_BT,
	MT_AFE_DIGITAL_BLOCK_HDMI,
	MT_AFE_DIGITAL_BLOCK_HDMI_RAW,
	MT_AFE_DIGITAL_BLOCK_SPDIF,
	MT_AFE_DIGITAL_BLOCK_NUM,
	MT_AFE_MEM_INTERFACE_NUM = MT_AFE_DIGITAL_BLOCK_MEM_VUL_DATA2 + 1
};

enum mt_afe_mem_context {
	MT_AFE_MEM_CTX_DL1 = 0,
	MT_AFE_MEM_CTX_DL2,
	MT_AFE_MEM_CTX_VUL,
	MT_AFE_MEM_CTX_VUL2,
	MT_AFE_MEM_CTX_DAI,
	MT_AFE_MEM_CTX_MOD_DAI,
	MT_AFE_MEM_CTX_AWB,
	MT_AFE_MEM_CTX_HDMI,
	MT_AFE_MEM_CTX_HDMI_RAW,
	MT_AFE_MEM_CTX_SPDIF,
	MT_AFE_MEM_CTX_COUNT
};

enum mt_afe_memif_direction {
	MT_AFE_MEMIF_DIRECTION_OUTPUT,
	MT_AFE_MEMIF_DIRECTION_INPUT
};

enum mt_afe_memif_mono_type {
	L_MONO = 0,
	R_MONO
};

enum mt_afe_interconnection_input {
	INTER_CONN_I00 = 0,
	INTER_CONN_I01,
	INTER_CONN_I02,
	INTER_CONN_I03,
	INTER_CONN_I04,
	INTER_CONN_I05,
	INTER_CONN_I06,
	INTER_CONN_I07,
	INTER_CONN_I08,
	INTER_CONN_I09,
	INTER_CONN_I10,
	INTER_CONN_I11,
	INTER_CONN_I12,
	INTER_CONN_I13,
	INTER_CONN_I14,
	INTER_CONN_I15,
	INTER_CONN_I16,
	INTER_CONN_I17,
	INTER_CONN_I18,
	INTER_CONN_I19,
	INTER_CONN_I20,
	INTER_CONN_INPUT_NUM
};

enum mt_afe_interconnection_output {
	INTER_CONN_O00 = 0,
	INTER_CONN_O01,
	INTER_CONN_O02,
	INTER_CONN_O03,
	INTER_CONN_O04,
	INTER_CONN_O05,
	INTER_CONN_O06,
	INTER_CONN_O07,
	INTER_CONN_O08,
	INTER_CONN_O09,
	INTER_CONN_O10,
	INTER_CONN_O11,
	INTER_CONN_O12,
	INTER_CONN_O13,
	INTER_CONN_O14,
	INTER_CONN_O15,
	INTER_CONN_O16,
	INTER_CONN_O17,
	INTER_CONN_O18,
	INTER_CONN_O19,
	INTER_CONN_O20,
	INTER_CONN_O21,
	INTER_CONN_O22,
	INTER_CONN_OUTPUT_NUM
};

enum mt_afe_hdmi_interconnection_input {
	INTER_CONN_I30 = 30,
	INTER_CONN_I31,
	INTER_CONN_I32,
	INTER_CONN_I33,
	INTER_CONN_I34,
	INTER_CONN_I35,
	INTER_CONN_I36,
	INTER_CONN_I37,
	HDMI_INTER_CONN_INPUT_BASE = INTER_CONN_I30,
	HDMI_INTER_CONN_INPUT_MAX = INTER_CONN_I37,
	HDMI_INTER_CONN_INPUT_NUM = (HDMI_INTER_CONN_INPUT_MAX - HDMI_INTER_CONN_INPUT_BASE + 1)
};

enum mt_afe_hdmi_interconnection_output {
	INTER_CONN_O30 = 30,
	INTER_CONN_O31,
	INTER_CONN_O32,
	INTER_CONN_O33,
	INTER_CONN_O34,
	INTER_CONN_O35,
	INTER_CONN_O36,
	INTER_CONN_O37,
	INTER_CONN_O38,
	INTER_CONN_O39,
	INTER_CONN_O40,
	INTER_CONN_O41,
	HDMI_INTER_CONN_OUTPUT_BASE = INTER_CONN_O30,
	HDMI_INTER_CONN_OUTPUT_MAX = INTER_CONN_O41,
	HDMI_INTER_CONN_OUTPUT_NUM = (HDMI_INTER_CONN_OUTPUT_MAX - HDMI_INTER_CONN_OUTPUT_BASE + 1)
};

enum mt_afe_interconnection_state {
	INTER_DISCONNECT = 0x0,
	INTER_CONNECT = 0x1,
	INTER_CONNECT_SHIFT = 0x2
};

enum mt_afe_irq_mcu_mode {
	MT_AFE_IRQ_MCU_MODE_IRQ1 = 0,
	MT_AFE_IRQ_MCU_MODE_IRQ2,
	MT_AFE_IRQ_MCU_MODE_IRQ3,
	MT_AFE_IRQ_MCU_MODE_IRQ4,
	MT_AFE_IRQ_MCU_MODE_IRQ5,	/* dedicated for HDMI */
	MT_AFE_IRQ_MCU_MODE_IRQ6,	/* dedicated for SPDIF */
	MT_AFE_IRQ_MCU_MODE_IRQ7,
	MT_AFE_IRQ_MCU_MODE_IRQ8,	/* dedicated for SPDIF2 */
	MT_AFE_IRQ_MCU_MODE_NUM
};

enum mt_afe_hw_digital_gain {
	MT_AFE_HW_DIGITAL_GAIN1,
	MT_AFE_HW_DIGITAL_GAIN2
};

enum mt_afe_i2s_in_pad_sel {
	MT_AFE_I2S_IN_FROM_CONNSYS = 0,
	MT_AFE_I2S_IN_FROM_IO_MUX = 1
};

enum mt_afe_lr_swap {
	MT_AFE_LR_SWAP_NO_SWAP = 0,
	MT_AFE_LR_SWAP_LR_DATASWAP = 1
};

enum mt_afe_inv_lrck {
	MT_AFE_INV_LRCK_NO_INVERSE = 0,
	MT_AFE_INV_LRCK_INVESE_LRCK = 1
};

enum mt_afe_inv_bck {
	MT_AFE_BCK_INV_NO_INVERSE = 0,
	MT_AFE_BCK_INV_INVESE_BCK = 1
};

enum mt_afe_i2s_format {
	MT_AFE_I2S_FORMAT_EIAJ = 0,
	MT_AFE_I2S_FORMAT_I2S = 1
};

enum mt_afe_i2s_src_mode {
	MT_AFE_I2S_SRC_MASTER_MODE = 0,
	MT_AFE_I2S_SRC_SLAVE_MODE = 1
};

enum mt_afe_i2s_hd_en {
	MT_AFE_NORMAL_CLOCK = 0,
	MT_AFE_LOW_JITTER_CLOCK = 1
};

enum mt_afe_i2s_wlen {
	MT_AFE_I2S_WLEN_16BITS = 0,
	MT_AFE_I2S_WLEN_32BITS = 1
};

enum mt_afe_i2s_sample_rate {
	MT_AFE_I2S_SAMPLERATE_8K = 0,
	MT_AFE_I2S_SAMPLERATE_11K = 1,
	MT_AFE_I2S_SAMPLERATE_12K = 2,
	MT_AFE_I2S_SAMPLERATE_16K = 4,
	MT_AFE_I2S_SAMPLERATE_22K = 5,
	MT_AFE_I2S_SAMPLERATE_24K = 6,
	MT_AFE_I2S_SAMPLERATE_32K = 8,
	MT_AFE_I2S_SAMPLERATE_44K = 9,
	MT_AFE_I2S_SAMPLERATE_48K = 10,
	MT_AFE_I2S_SAMPLERATE_88K = 11,
	MT_AFE_I2S_SAMPLERATE_96K = 12,
	MT_AFE_I2S_SAMPLERATE_174K = 13,
	MT_AFE_I2S_SAMPLERATE_192K = 14,
};

enum mt_afe_apll_clock_type {
	MT_AFE_ENGEN = 0,
	MT_AFE_I2S0,
	MT_AFE_I2S1,
	MT_AFE_I2S2,
	MT_AFE_I2S3,
	MT_AFE_SPDIF,
	MT_AFE_SPDIF2,
	MT_AFE_I2S3_BCK,
	MT_AFE_APLL_CLOCK_TYPE_NUM
};

enum mt_afe_apll_source {
	MT_AFE_APLL1 = 0,
	MT_AFE_APLL2
};

enum mt_afe_apll_clok_freq {
	MT_AFE_APLL1_CLOCK_FREQ = (22579200 * 8),
	MT_AFE_APLL2_CLOCK_FREQ = (24576000 * 8),
};

enum mt_afe_tdm_channel_bck_cycles {
	MT_AFE_TDM_16_BCK_CYCLES = 0,
	MT_AFE_TDM_24_BCK_CYCLES,
	MT_AFE_TDM_32_BCK_CYCLES,
};

enum mt_afe_tdm_lrck_inv {
	MT_AFE_TDM_LRCK_NOT_INVERSE = 0,
	MT_AFE_TDM_LRCK_INVERSE,
};

enum mt_afe_tdm_bck_inv {
	MT_AFE_TDM_BCK_NOT_INVERSE = 0,
	MT_AFE_TDM_BCK_INVERSE,
};

enum mt_afe_tdm_delay_data {
	MT_AFE_TDM_0_BCK_CYCLE_DELAY = 0,
	MT_AFE_TDM_1_BCK_CYCLE_DELAY,
};

enum mt_afe_tdm_wlen {
	MT_AFE_TDM_WLLEN_16BIT = 1,
	MT_AFE_TDM_WLLEN_32BIT = 2,
};

enum mt_afe_tdm_left_align {
	MT_AFE_TDM_NOT_ALIGNED_TO_MSB = 0,
	MT_AFE_TDM_ALIGNED_TO_MSB,
};

enum mt_afe_tdm_number_of_channel_for_each_data {
	MT_AFE_TDM_2CH_FOR_EACH_SDATA = 0,
	MT_AFE_TDM_4CH_FOR_EACH_SDATA,
	MT_AFE_TDM_8CH_FOR_EACH_SDATA,
};

enum mt_afe_tdm_st_ch_pair_sout {
	CHANNEL_START_FROM_030_O31 = 0,
	CHANNEL_START_FROM_032_O33,
	CHANNEL_START_FROM_034_O35,
	CHANNEL_START_FROM_036_O37,
	CHANNEL_DATA_IS_ZERO,
};

enum mt_afe_daibt_input {
	MT_AFE_DAIBT_INPUT_FROM_BT = 0,
	MT_AFE_DAIBT_INPUT_FROM_MGRIF
};

enum mt_afe_daibt_mode {
	MT_AFE_DAIBT_MODE_8K = 0,
	MT_AFE_DAIBT_MODE_16K
};

enum mt_afe_dai_sel {
	MT_AFE_DAI_SEL_HIGHWORD = 0,
	MT_AFE_DAI_SEL_LOWWORD
};

enum mt_afe_btsync {
	MT_AFE_BTSYNC_SHORT_SYNC = 0,
	MT_AFE_BTSYNC_LONG_SYNC
};

enum mt_afe_merge_interface_i2s_sample_rate {
	MT_AFE_MRGIF_I2S_SAMPLERATE_8K = 0,
	MT_AFE_MRGIF_I2S_SAMPLERATE_11K = 1,
	MT_AFE_MRGIF_I2S_SAMPLERATE_12K = 2,
	MT_AFE_MRGIF_I2S_SAMPLERATE_16K = 4,
	MT_AFE_MRGIF_I2S_SAMPLERATE_22K = 5,
	MT_AFE_MRGIF_I2S_SAMPLERATE_24K = 6,
	MT_AFE_MRGIF_I2S_SAMPLERATE_32K = 8,
	MT_AFE_MRGIF_I2S_SAMPLERATE_44K = 9,
	MT_AFE_MRGIF_I2S_SAMPLERATE_48K = 10
};

enum mt_afe_memif_format {
	MT_AFE_MEMIF_16_BIT = 0,
	MT_AFE_MEMIF_32_BIT_ALIGN_8BIT_0_24BIT_DATA = 1,
	MT_AFE_MEMIF_32_BIT_ALIGN_24BIT_DATA_8BIT_0 = 3,
};

enum mt_afe_conn_output_format {
	MT_AFE_CONN_OUTPUT_16BIT = 0,
	MT_AFE_CONN_OUTPUT_24BIT
};

enum mt_afe_apll_divider_group {
	MT_AFE_APLL1_DIV0 = 8,
	MT_AFE_APLL1_DIV1 = 9,
	MT_AFE_APLL1_DIV2 = 10,
	MT_AFE_APLL1_DIV3 = 11,
	MT_AFE_APLL1_DIV4 = 12,
	MT_AFE_APLL1_DIV5 = 13,
	MT_AFE_SPDIF_DIV = 14,
	MT_AFE_SPDIF2_DIV = 15,
	MT_AFE_APLL2_DIV0 = 16,
	MT_AFE_APLL2_DIV1 = 17,
	MT_AFE_APLL2_DIV2 = 18,
	MT_AFE_APLL2_DIV3 = 19,
	MT_AFE_APLL2_DIV4 = 20,
	MT_AFE_APLL2_DIV5 = 21,
	MT_AFE_APLL_DIV_COUNT = (MT_AFE_APLL2_DIV5 - MT_AFE_APLL1_DIV0 + 1),
};

enum mt_afe_irq_mcu_status {
	MT_AFE_IRQ1_MCU = (1 << 0),
	MT_AFE_IRQ2_MCU = (1 << 1),
	MT_AFE_IRQ3_MCU = (1 << 2),
	MT_AFE_IRQ4_MCU = (1 << 3),
	MT_AFE_IRQ5_MCU = (1 << 4),
	MT_AFE_IRQ6_MCU = (1 << 5),
	MT_AFE_IRQ7_MCU = (1 << 6),
	MT_AFE_IRQ8_MCU = (1 << 7),
	MT_AFE_IRQ_MCU_STATUS_COUNT = 8,
};

enum pcm_fmat {
	PCM_I2S   = 0x0,
	PCM_EIAJ  = 0x1,
	PCM_MODEA = 0x2,
	PCM_MODEB = 0x3,
};

enum pcm_mode {
	PCM_8K  = 0x0,
	PCM_16K = 0x1,
	PCM_32K = 0x2,
};

enum pcm_wlen {
	PCM_32BCK = 0x0,
	PCM_64BCK = 0x1,
};

enum pcm_slave {
	PCM_MASTER = 0x0,
	PCM_SLAVE = 0x1,
};

enum pcm_byp_asrc {
	PCM_GO_ASRC  = 0x0,         /* (ASRC)       Set to 0 when source & destination uses different crystal*/
	PCM_GO_ASYNC_FIFO = 0x1,    /*(Async FIFO) Set to 1 when source & destination uses same crystal*/
};

enum pcm_vbit_16k_mode {
	PCM_VBT_16K_MODE_DISABLE = 0x0,
	PCM_VBT_16K_MODE_ENABLE = 0x1,
};

enum pcm_24bit {
	PCM_16BIT = 0x0,
	PCM_32BIT = 0x1,
	PCM_24BIT = 0x2,
};

enum pcm_ext_mode_sel {
	PCM_INT_MD = 0x0,
	PCM_EXT_MD = 0x1,
};

enum pcm_sync_inv {
	Soc_Aud_INV_SYNC_NO_INVERSE = 0,
	Soc_Aud_INV_SYNC_INVERSE = 1,
};

enum pcm_bck_inv {
	Soc_Aud_INV_BCK_NO_INVERSE = 0,
	Soc_Aud_INV_BCK_INVERSE = 1,
};

enum pcm_tx_lch_rpt {
	Soc_Aud_TX_LCH_RPT_TX_LCH_NO_REPEAT = 0,
	Soc_Aud_TX_LCH_RPT_TX_LCH_REPEAT = 1,
};

enum pcm_sync_type {
	Soc_Aud_PCM_SYNC_TYPE_BCK_CYCLE_SYNC = 0,	/* bck sync length = 1 */
	Soc_Aud_PCM_SYNC_TYPE_EXTEND_BCK_CYCLE_SYNC = 1,	/* bck sync length = PCM_INTF_CON[9:13] */
};

enum pcm_bt_mode {
	Soc_Aud_BT_MODE_DUAL_MIC_ON_TX = 0,
	Soc_Aud_BT_MODE_SINGLE_MIC_ON_TX = 1,
};

/*
 * STRUCT DEFINITION
 */

struct mt_afe_digital_i2s {
	bool lr_swap;
	bool i2s_slave;
	uint32_t i2s_sample_rate;
	bool inv_lrck;
	bool i2s_fmt;
	bool i2s_wlen;
	/* here for ADC usage , DAC will not use this */
	int buffer_update_word;
	bool loopback;
	bool fpga_bit;
	bool fpga_bit_test;
};

struct mt_afe_digital_dai_bt {
	bool use_mrgif_input;
	bool dai_bt_mode;
	bool dai_del;
	int bt_len;
	bool data_rdy;
	bool bt_sync;
	bool bt_on;
	bool dai_bt_on;
};

struct mt_afe_pcm_info {
	enum pcm_fmat fmt;
	enum pcm_mode mode;
	enum pcm_slave slave;
	enum pcm_byp_asrc byp_asrc;
	enum pcm_bt_mode bt_mode;
	enum pcm_sync_type sync_type;
	unsigned int sync_length;
	enum pcm_wlen wlen;
	enum pcm_24bit bit24;
	enum pcm_ext_mode_sel ext_modem;
	enum pcm_vbit_16k_mode vbat_16k_mode;
	enum pcm_tx_lch_rpt tx_lch_rpt;
	enum pcm_bck_inv bck_in_inv;
	enum pcm_sync_inv sync_in_inv;
	enum pcm_bck_inv bck_out_inv;
	enum pcm_sync_inv sync_out_inv;
};

struct mt_afe_block_t {
	unsigned int phy_buf_addr;
	unsigned char *virtual_buf_addr;
	int buffer_size;
	int data_remained;
	int write_index;
	int read_index;
	unsigned int iec_nsadr;
};

struct mt_afe_mem_control_t {
	struct mt_afe_block_t block;
	struct snd_pcm_substream *substream;
};

struct mt_afe_merge_interface {
	bool mergeif_i2s_enable;
	int mrg_i2s_sample_rate;
	bool mrgif_en;
};

struct mt_afe_irq_status {
	bool status;
	unsigned int irq_mcu_counter;
	unsigned int sample_rate;
};

struct mt_afe_mem_if_attribute {
	int format;
	unsigned int sample_rate;
	unsigned int channels;
	unsigned int state;
	unsigned int fetch_format_per_sample;
	int user_count;
};

struct mt_afe_suspend_reg {
	uint32_t reg_AUDIO_TOP_CON0;
	uint32_t reg_AUDIO_TOP_CON1;
	uint32_t reg_AUDIO_TOP_CON2;
	uint32_t reg_AUDIO_TOP_CON3;
	uint32_t reg_AFE_DAC_CON0;
	uint32_t reg_AFE_DAC_CON1;
	uint32_t reg_AFE_I2S_CON;
	uint32_t reg_AFE_DAIBT_CON0;

	uint32_t reg_AFE_CONN0;
	uint32_t reg_AFE_CONN1;
	uint32_t reg_AFE_CONN2;
	uint32_t reg_AFE_CONN3;
	uint32_t reg_AFE_CONN4;
	uint32_t reg_AFE_CONN5;
	uint32_t reg_AFE_CONN6;
	uint32_t reg_AFE_CONN7;
	uint32_t reg_AFE_CONN8;
	uint32_t reg_AFE_CONN9;
	uint32_t reg_AFE_CONN_24BIT;
	uint32_t reg_AFE_I2S_CON1;
	uint32_t reg_AFE_I2S_CON2;
	uint32_t reg_AFE_I2S_CON3;
	uint32_t reg_AFE_MRGIF_CON;

	uint32_t reg_AFE_DL1_BASE;
	uint32_t reg_AFE_DL1_CUR;
	uint32_t reg_AFE_DL1_END;
	uint32_t reg_AFE_DL2_BASE;
	uint32_t reg_AFE_DL2_CUR;
	uint32_t reg_AFE_DL2_END;
	uint32_t reg_AFE_AWB_BASE;
	uint32_t reg_AFE_AWB_CUR;
	uint32_t reg_AFE_AWB_END;
	uint32_t reg_AFE_VUL_BASE;
	uint32_t reg_AFE_VUL_CUR;
	uint32_t reg_AFE_VUL_END;
	uint32_t reg_AFE_VUL_D2_BASE;
	uint32_t reg_AFE_VUL_D2_CUR;
	uint32_t reg_AFE_VUL_D2_END;
	uint32_t reg_AFE_DAI_BASE;
	uint32_t reg_AFE_DAI_CUR;
	uint32_t reg_AFE_DAI_END;
	uint32_t reg_AFE_MEMIF_MSB;

	uint32_t reg_AFE_ADDA_DL_SRC2_CON0;
	uint32_t reg_AFE_ADDA_DL_SRC2_CON1;
	uint32_t reg_AFE_ADDA_UL_SRC_CON0;
	uint32_t reg_AFE_ADDA_UL_SRC_CON1;
	uint32_t reg_AFE_ADDA_TOP_CON0;
	uint32_t reg_AFE_ADDA_UL_DL_CON0;
	uint32_t reg_AFE_ADDA_NEWIF_CFG0;
	uint32_t reg_AFE_ADDA_NEWIF_CFG1;
	uint32_t reg_AFE_ADDA2_TOP_CON0;

	uint32_t reg_AFE_SIDETONE_CON0;
	uint32_t reg_AFE_SIDETONE_COEFF;
	uint32_t reg_AFE_SIDETONE_CON1;
	uint32_t reg_AFE_SIDETONE_GAIN;
	uint32_t reg_AFE_SGEN_CON0;
	uint32_t reg_AFE_SGEN_CON1;
	uint32_t reg_AFE_TOP_CON0;
	uint32_t reg_AFE_ADDA_PREDIS_CON0;
	uint32_t reg_AFE_ADDA_PREDIS_CON1;

	uint32_t reg_AFE_MOD_DAI_BASE;
	uint32_t reg_AFE_MOD_DAI_END;
	uint32_t reg_AFE_MOD_DAI_CUR;
	uint32_t reg_AFE_HDMI_OUT_CON0;
	uint32_t reg_AFE_HDMI_OUT_BASE;
	uint32_t reg_AFE_HDMI_OUT_CUR;
	uint32_t reg_AFE_HDMI_OUT_END;
	uint32_t reg_AFE_SPDIF_OUT_CON0;
	uint32_t reg_AFE_SPDIF_BASE;
	uint32_t reg_AFE_SPDIF_CUR;
	uint32_t reg_AFE_SPDIF_END;
	uint32_t reg_AFE_SPDIF2_OUT_CON0;
	uint32_t reg_AFE_SPDIF2_BASE;
	uint32_t reg_AFE_SPDIF2_CUR;
	uint32_t reg_AFE_SPDIF2_END;
	uint32_t reg_AFE_HDMI_CONN0;

	uint32_t reg_AFE_IRQ_MCU_CON;
	uint32_t reg_AFE_IRQ_MCU_CNT1;
	uint32_t reg_AFE_IRQ_MCU_CNT2;
	uint32_t reg_AFE_IRQ_MCU_EN;
	uint32_t reg_AFE_IRQ_MCU_CNT5;
	uint32_t reg_AFE_MEMIF_MAXLEN;
	uint32_t reg_AFE_MEMIF_PBUF_SIZE;
	uint32_t reg_AFE_MEMIF_PBUF2_SIZE;
	uint32_t reg_AFE_APLL1_TUNER_CFG;
	uint32_t reg_AFE_APLL2_TUNER_CFG;

	uint32_t reg_AFE_GAIN1_CON0;
	uint32_t reg_AFE_GAIN1_CON1;
	uint32_t reg_AFE_GAIN1_CON2;
	uint32_t reg_AFE_GAIN1_CON3;
	uint32_t reg_AFE_GAIN1_CONN;
	uint32_t reg_AFE_GAIN1_CUR;
	uint32_t reg_AFE_GAIN2_CON0;
	uint32_t reg_AFE_GAIN2_CON1;
	uint32_t reg_AFE_GAIN2_CON2;
	uint32_t reg_AFE_GAIN2_CON3;
	uint32_t reg_AFE_GAIN2_CONN;
	uint32_t reg_AFE_GAIN2_CUR;

	uint32_t reg_AFE_IEC_CFG;
	uint32_t reg_AFE_IEC_NSNUM;
	uint32_t reg_AFE_IEC_BURST_INFO;
	uint32_t reg_AFE_IEC_BURST_LEN;
	uint32_t reg_AFE_IEC_NSADR;
	uint32_t reg_AFE_IEC_CHL_STAT0;
	uint32_t reg_AFE_IEC_CHL_STAT1;
	uint32_t reg_AFE_IEC_CHR_STAT0;
	uint32_t reg_AFE_IEC_CHR_STAT1;
	uint32_t reg_AFE_IEC2_CFG;
	uint32_t reg_AFE_IEC2_NSNUM;
	uint32_t reg_AFE_IEC2_BURST_INFO;
	uint32_t reg_AFE_IEC2_BURST_LEN;
	uint32_t reg_AFE_IEC2_NSADR;
	uint32_t reg_AFE_IEC2_CHL_STAT0;
	uint32_t reg_AFE_IEC2_CHL_STAT1;
	uint32_t reg_AFE_IEC2_CHR_STAT0;
	uint32_t reg_AFE_IEC2_CHR_STAT1;

	uint32_t reg_AFE_ASRC_CON0;
	uint32_t reg_AFE_ASRC_CON1;
	uint32_t reg_AFE_ASRC_CON2;
	uint32_t reg_AFE_ASRC_CON3;
	uint32_t reg_AFE_ASRC_CON4;
	uint32_t reg_AFE_ASRC_CON5;
	uint32_t reg_AFE_ASRC_CON6;
	uint32_t reg_AFE_ASRC_CON7;
	uint32_t reg_AFE_ASRC_CON8;
	uint32_t reg_AFE_ASRC_CON9;
	uint32_t reg_AFE_ASRC_CON10;
	uint32_t reg_AFE_ASRC_CON11;
	uint32_t reg_AFE_ASRC_CON13;
	uint32_t reg_AFE_ASRC_CON14;
	uint32_t reg_AFE_ASRC_CON15;
	uint32_t reg_AFE_ASRC_CON16;
	uint32_t reg_AFE_ASRC_CON17;
	uint32_t reg_AFE_ASRC_CON18;
	uint32_t reg_AFE_ASRC_CON19;
	uint32_t reg_AFE_ASRC_CON20;
	uint32_t reg_AFE_ASRC_CON21;
	uint32_t reg_PCM_INTF_CON1;
	uint32_t reg_PCM_INTF_CON2;
	uint32_t reg_PCM2_INTF_CON;
	uint32_t reg_AFE_TDM_CON1;
	uint32_t reg_AFE_TDM_CON2;
};

#endif
