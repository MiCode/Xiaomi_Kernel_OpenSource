/* mt6630_fm_cmd.c
 *
 * (C) Copyright 2009
 * MediaTek <www.MediaTek.com>
 * Hongcheng <hongcheng.xia@MediaTek.com>
 *
 * MT6630 FM Radio Driver --  source file
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/kernel.h>
#include <linux/types.h>

#include "fm_typedef.h"
#include "fm_dbg.h"
#include "fm_err.h"
#include "fm_rds.h"
#include "fm_config.h"
#include "fm_link.h"

#include "mt6630_fm_reg.h"
/* #include "mt6630_fm_link.h" */
#include "mt6630_fm.h"
#include "mt6630_fm_cmd.h"
#include "mt6630_fm_cust_cfg.h"

extern fm_cust_cfg mt6630_fm_config;

static fm_s32 fm_bop_write(fm_u8 addr, fm_u16 value, fm_u8 *buf, fm_s32 size)
{
	if (size < (FM_WRITE_BASIC_OP_SIZE + 2)) {
		return (-1);
	}

	if (buf == NULL) {
		return (-2);
	}

	buf[0] = FM_WRITE_BASIC_OP;
	buf[1] = FM_WRITE_BASIC_OP_SIZE;
	buf[2] = addr;
	buf[3] = (fm_u8) ((value) & 0x00FF);
	buf[4] = (fm_u8) ((value >> 8) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3],
		buf[4]);

	return (FM_WRITE_BASIC_OP_SIZE + 2);
}


static fm_s32 fm_bop_udelay(fm_u32 value, fm_u8 *buf, fm_s32 size)
{
	if (size < (FM_UDELAY_BASIC_OP_SIZE + 2)) {
		return (-1);
	}

	if (buf == NULL) {
		return (-2);
	}

	buf[0] = FM_UDELAY_BASIC_OP;
	buf[1] = FM_UDELAY_BASIC_OP_SIZE;
	buf[2] = (fm_u8) ((value) & 0x000000FF);
	buf[3] = (fm_u8) ((value >> 8) & 0x000000FF);
	buf[4] = (fm_u8) ((value >> 16) & 0x000000FF);
	buf[5] = (fm_u8) ((value >> 24) & 0x000000FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3],
		buf[4], buf[5]);

	return (FM_UDELAY_BASIC_OP_SIZE + 2);
}


static fm_s32 fm_bop_rd_until(fm_u8 addr, fm_u16 mask, fm_u16 value, fm_u8 *buf, fm_s32 size)
{
	if (size < (FM_RD_UNTIL_BASIC_OP_SIZE + 2)) {
		return (-1);
	}

	if (buf == NULL) {
		return (-2);
	}

	buf[0] = FM_RD_UNTIL_BASIC_OP;
	buf[1] = FM_RD_UNTIL_BASIC_OP_SIZE;
	buf[2] = addr;
	buf[3] = (fm_u8) ((mask) & 0x00FF);
	buf[4] = (fm_u8) ((mask >> 8) & 0x00FF);
	buf[5] = (fm_u8) ((value) & 0x00FF);
	buf[6] = (fm_u8) ((value >> 8) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2],
		buf[3], buf[4], buf[5], buf[6]);

	return (FM_RD_UNTIL_BASIC_OP_SIZE + 2);
}


static fm_s32 fm_bop_modify(fm_u8 addr, fm_u16 mask_and, fm_u16 mask_or, fm_u8 *buf, fm_s32 size)
{
	if (size < (FM_MODIFY_BASIC_OP_SIZE + 2)) {
		return (-1);
	}

	if (buf == NULL) {
		return (-2);
	}

	buf[0] = FM_MODIFY_BASIC_OP;
	buf[1] = FM_MODIFY_BASIC_OP_SIZE;
	buf[2] = addr;
	buf[3] = (fm_u8) ((mask_and) & 0x00FF);
	buf[4] = (fm_u8) ((mask_and >> 8) & 0x00FF);
	buf[5] = (fm_u8) ((mask_or) & 0x00FF);
	buf[6] = (fm_u8) ((mask_or >> 8) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2],
		buf[3], buf[4], buf[5], buf[6]);

	return (FM_MODIFY_BASIC_OP_SIZE + 2);
}

/*
 * mt6630_pwrup_clock_on - Wholechip FM Power Up: step 1, FM Digital Clock enable
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
fm_s32 mt6630_pwrup_clock_on(fm_u8 *buf, fm_s32 buf_size)
{
	fm_s32 pkt_size = 0;
	fm_u16 de_emphasis;
	/* fm_u16 osc_freq; */

	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	de_emphasis = mt6630_fm_config.rx_cfg.deemphasis;	/* MT6630fm_cust_config_fetch(FM_CFG_RX_DEEMPHASIS); */
	de_emphasis &= 0x0001;	/* rang 0~1 */
	/* osc_freq = mt6630_fm_config.rx_cfg.osc_freq;//MT6628fm_cust_config_fetch(FM_CFG_RX_OSC_FREQ); */
	/* osc_freq &= 0x0007; //rang 0~5 */

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_ENABLE_OPCODE;
	pkt_size = 4;

	/* B1.1    Enable digital OSC */
	pkt_size += fm_bop_write(0x60, 0x0003, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 3 */
	pkt_size += fm_bop_udelay(100, &buf[pkt_size], buf_size - pkt_size);	/* delay 100us */
	/* B1.3    Release HW clock gating */
	pkt_size += fm_bop_write(0x60, 0x0007, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 7 */
	/* B1.4    Set FM long/short antenna:1: short_antenna  0: long antenna(default) */
	pkt_size += fm_bop_modify(0x61, 0xFFEF, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	/* B1.5    Set audio output mode (lineout/I2S) 0:lineout,  1:I2S */
	if (mt6630_fm_config.aud_cfg.aud_path == FM_AUD_ANALOG) {
		pkt_size +=
		    fm_bop_modify(0x61, 0xFF7F, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	} else {
		pkt_size +=
		    fm_bop_modify(0x61, 0xFF7F, 0x0080, &buf[pkt_size], buf_size - pkt_size);
	}
	/* B1.6    Set deemphasis setting */
	pkt_size +=
	    fm_bop_modify(0x61, ~DE_EMPHASIS, (de_emphasis << 12), &buf[pkt_size],
			  buf_size - pkt_size);

	/* pkt_size += fm_bop_modify(0x60, OSC_FREQ_MASK, (osc_freq << 4), &buf[pkt_size], buf_size - pkt_size); */

	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);

	return pkt_size;
}


/*
 * mt6630_patch_download - Wholechip FM Power Up: step 3, download patch to f/w,
 * @buf - target buf
 * @buf_size - buffer size
 * @seg_num - total segments that this patch divided into
 * @seg_id - No. of Segments: segment that will now be sent
 * @src - patch source buffer
 * @seg_len - segment size: segment that will now be sent
 * return package size
 */
fm_s32 mt6630_patch_download(fm_u8 *buf, fm_s32 buf_size, fm_u8 seg_num, fm_u8 seg_id,
			     const fm_u8 *src, fm_s32 seg_len)
{
	fm_s32 pkt_size = 0;
	fm_u8 *dst = NULL;

	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_PATCH_DOWNLOAD_OPCODE;
	pkt_size = 4;

	buf[pkt_size++] = seg_num;
	buf[pkt_size++] = seg_id;

	if (seg_len > (buf_size - pkt_size)) {
		return -1;
	}

	dst = &buf[pkt_size];
	pkt_size += seg_len;

	/* copy patch to tx buffer */
	while (seg_len--) {
		*dst = *src;
		/* pr_debug("%02x ", *dst); */
		src++;
		dst++;
	}

	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);
	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2],
		buf[3], buf[4], buf[5], buf[6]);

	return pkt_size;
}


/*
 * mt6630_coeff_download - Wholechip FM Power Up: step 3,download coeff to f/w,
 * @buf - target buf
 * @buf_size - buffer size
 * @seg_num - total segments that this patch divided into
 * @seg_id - No. of Segments: segment that will now be sent
 * @src - patch source buffer
 * @seg_len - segment size: segment that will now be sent
 * return package size
 */
fm_s32 mt6630_coeff_download(fm_u8 *buf, fm_s32 buf_size, fm_u8 seg_num, fm_u8 seg_id,
			     const fm_u8 *src, fm_s32 seg_len)
{
	fm_s32 pkt_size = 0;
	fm_u8 *dst = NULL;

	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_COEFF_DOWNLOAD_OPCODE;
	pkt_size = 4;

	buf[pkt_size++] = seg_num;
	buf[pkt_size++] = seg_id;

	if (seg_len > (buf_size - pkt_size)) {
		return -1;
	}

	dst = &buf[pkt_size];
	pkt_size += seg_len;

	/* copy patch to tx buffer */
	while (seg_len--) {
		*dst = *src;
		/* pr_debug("%02x ", *dst); */
		src++;
		dst++;
	}

	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);
	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2],
		buf[3], buf[4], buf[5], buf[6]);

	return pkt_size;
}

/*
 * mt6630_pwrup_digital_init - Wholechip FM Power Up: step 4, FM Digital Init: fm_rgf_maincon
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
fm_s32 mt6630_pwrup_digital_init(fm_u8 *buf, fm_s32 buf_size)
{
	fm_s32 pkt_size = 0;

	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_ENABLE_OPCODE;
	pkt_size = 4;

	//update FM ADPLL fast tracking mode gain
	pkt_size += fm_bop_modify(0xF, 0xF800, 0x0455, &buf[pkt_size], buf_size - pkt_size);
	/* F1.4    Set appropriate interrupt mask behavior as desired(RX) */
	/* pkt_size += fm_bop_write(0x6A, 0x0021, &buf[pkt_size], buf_size - pkt_size);//wr 6A 0021 */
	pkt_size += fm_bop_write(0x6B, 0x0021, &buf[pkt_size], buf_size - pkt_size);	/* wr 6B 0021 */
	/* F1.9    Enable HW auto control */
	pkt_size += fm_bop_write(0x60, 0x000F, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 f */
	/* F1.10   Release ASIP reset */
	pkt_size += fm_bop_modify(0x61, 0xFFFD, 0x0002, &buf[pkt_size], buf_size - pkt_size);	/* wr 61 D1=1 */
	/* F1.11   Enable ASIP power */
	pkt_size += fm_bop_modify(0x61, 0xFFFE, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 61 D0=0 */
	pkt_size += fm_bop_udelay(100000, &buf[pkt_size], buf_size - pkt_size);	/* delay 100ms */
	/* F1.13   Check HW intitial complete */
	pkt_size += fm_bop_rd_until(0x64, 0x001F, 0x0002, &buf[pkt_size], buf_size - pkt_size);	/* Poll 64[0~4] = 2 */

	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);

	return pkt_size;
}

/*
 * mt6630_pwrdown - Wholechip FM Power down: Digital Modem Power Down
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
fm_s32 mt6630_pwrdown(fm_u8 *buf, fm_s32 buf_size)
{
	fm_s32 pkt_size = 0;

	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_ENABLE_OPCODE;
	pkt_size = 4;

	/* Disable HW clock control */
	pkt_size += fm_bop_write(0x60, 0x0107, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 107 */
	/* Reset ASIP */
	pkt_size += fm_bop_write(0x61, 0x0001, &buf[pkt_size], buf_size - pkt_size);	/* wr 61 0001 */
	/* digital core + digital rgf reset */
	pkt_size += fm_bop_modify(0x6E, 0xFFF8, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 6E[0~2] 0 */
	pkt_size += fm_bop_modify(0x6E, 0xFFF8, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 6E[0~2] 0 */
	pkt_size += fm_bop_modify(0x6E, 0xFFF8, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 6E[0~2] 0 */
	pkt_size += fm_bop_modify(0x6E, 0xFFF8, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 6E[0~2] 0 */
	/* Disable all clock */
	pkt_size += fm_bop_write(0x60, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 0000 */
	/* Reset rgfrf */
	pkt_size += fm_bop_write(0x60, 0x4000, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 4000 */
	pkt_size += fm_bop_write(0x60, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 0000 */

	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);

	return pkt_size;
}


/*
 * mt6630_rampdown - f/w will wait for STC_DONE interrupt
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
fm_s32 mt6630_rampdown(fm_u8 *buf, fm_s32 buf_size)
{
	fm_s32 pkt_size = 0;

	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_RAMPDOWN_OPCODE;
	pkt_size = 4;

	/* Clear DSP state */
	pkt_size += fm_bop_modify(FM_MAIN_CTRL, 0xFFF0, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 63[3:0] = 0 */
	/* Set DSP ramp down state */
	pkt_size += fm_bop_modify(FM_MAIN_CTRL, 0xFFFF, RAMP_DOWN, &buf[pkt_size], buf_size - pkt_size);	/* wr 63[8] = 1 */
	/* @Wait for STC_DONE interrupt@ */
	pkt_size += fm_bop_rd_until(FM_MAIN_INTR, FM_INTR_STC_DONE, FM_INTR_STC_DONE, &buf[pkt_size], buf_size - pkt_size);	/* Poll 69[0] = b'1 */
	/* Clear DSP ramp down state */
	pkt_size += fm_bop_modify(FM_MAIN_CTRL, (~RAMP_DOWN), 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 63[8] = 0 */
	/* Write 1 clear the STC_DONE interrupt status flag */
	pkt_size += fm_bop_modify(FM_MAIN_INTR, 0xFFFF, FM_INTR_STC_DONE, &buf[pkt_size], buf_size - pkt_size);	/* wr 69[0] = 1 */

	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);

	return pkt_size;
}


/*
 * mt6630_tune - execute tune action,
 * @buf - target buf
 * @buf_size - buffer size
 * @freq - 760 ~ 1080, 100KHz unit
 * return package size
 */
fm_s32 mt6630_tune(fm_u8 *buf, fm_s32 buf_size, fm_u16 freq, fm_u16 chan_para)
{
	/* #define FM_TUNE_USE_POLL */
	fm_s32 pkt_size = 0;

	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	if (0 == fm_get_channel_space(freq)) {
		freq *= 10;
	}

	freq = (freq - 6400) * 2 / 10;

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_TUNE_OPCODE;
	pkt_size = 4;

	/* Set desired channel & channel parameter */
#ifdef FM_TUNE_USE_POLL
	pkt_size += fm_bop_write(0x6A, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	pkt_size += fm_bop_write(0x6B, 0x0000, &buf[pkt_size], buf_size - pkt_size);
#endif
	pkt_size += fm_bop_modify(FM_CHANNEL_SET, 0xFC00, freq, &buf[pkt_size], buf_size - pkt_size);	/* set 0x65[9:0] = 0x029e, => ((97.5 - 64) * 20) */
	/* channel para setting, D15~D12, D15: ATJ, D13: HL, D12: FA */
	pkt_size +=
	    fm_bop_modify(FM_CHANNEL_SET, 0x0FFF, (chan_para << 12), &buf[pkt_size],
			  buf_size - pkt_size);
	/* Enable hardware controlled tuning sequence */
	pkt_size += fm_bop_modify(FM_MAIN_CTRL, 0xFFF8, TUNE, &buf[pkt_size], buf_size - pkt_size);	/* set 0x63[0] = 1 */
	/* Wait for STC_DONE interrupt */
#ifdef FM_TUNE_USE_POLL
	pkt_size += fm_bop_rd_until(FM_MAIN_INTR, FM_INTR_STC_DONE, FM_INTR_STC_DONE, &buf[pkt_size], buf_size - pkt_size);	/* Poll 69[0] = b'1 */
	/* Write 1 clear the STC_DONE interrupt status flag */
	pkt_size += fm_bop_modify(FM_MAIN_INTR, 0xFFFF, FM_INTR_STC_DONE, &buf[pkt_size], buf_size - pkt_size);	/* wr 69[0] = 1 */
#endif
	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);

	return pkt_size;
}


/*
 * mt6630_full_cqi_req - execute request cqi info action,
 * @buf - target buf
 * @buf_size - buffer size
 * @freq - 7600 ~ 10800, freq array
 * @cnt - channel count
 * @type - request type, 1: a single channel; 2: multi channel; 3:multi channel with 100Khz step; 4: multi channel with 50Khz step
 *
 * return package size
 */
fm_s32 mt6630_full_cqi_req(fm_u8 *buf, fm_s32 buf_size, fm_u16 *freq, fm_s32 cnt, fm_s32 type)
{
	fm_s32 pkt_size = 0;

	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_SOFT_MUTE_TUNE_OPCODE;
	pkt_size = 4;

	switch (type) {
	case 1:
		buf[pkt_size] = 0x0001;
		pkt_size++;
		buf[pkt_size] = (fm_u8) ((*freq) & 0x00FF);
		pkt_size++;
		buf[pkt_size] = (fm_u8) ((*freq >> 8) & 0x00FF);
		pkt_size++;
		break;
	case 2:
		buf[pkt_size] = 0x0002;
		pkt_size++;
		break;
	case 3:
		buf[pkt_size] = 0x0003;
		pkt_size++;
		break;
	case 4:
		buf[pkt_size] = 0x0004;
		pkt_size++;
		break;
	default:
		buf[pkt_size] = (fm_u16) type;
		pkt_size++;
		break;
	}

	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);

	return pkt_size;
}


/*
 * mt6630_seek - execute seek action,
 * @buf - target buf
 * @buf_size - buffer size
 * @seekdir - 0=seek up, 1=seek down
 * @space - step, 50KHz:001, 100KHz:010, 200KHz:100
 * @max_freq - upper bound
 * @min_freq - lower bound
 * return package size
 */
fm_s32 mt6630_seek(fm_u8 *buf, fm_s32 buf_size, fm_u16 seekdir, fm_u16 space, fm_u16 max_freq,
		   fm_u16 min_freq)
{
	fm_s32 pkt_size = 0;

	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	if (0 == fm_get_channel_space(max_freq)) {
		max_freq *= 10;
	}

	if (0 == fm_get_channel_space(min_freq)) {
		min_freq *= 10;
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_SEEK_OPCODE;
	pkt_size = 4;

	/* Program seek direction */
	if (seekdir == 0) {
		pkt_size += fm_bop_modify(FM_MAIN_CFG1, 0xFBFF, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* 0x66[10] = 0, seek up */
	} else {
		pkt_size += fm_bop_modify(FM_MAIN_CFG1, 0xFBFF, 0x0400, &buf[pkt_size], buf_size - pkt_size);	/* 0x66[10] = 1, seek down */
	}

	/* Program scan channel spacing */
	if (space == 1) {
		pkt_size += fm_bop_modify(FM_MAIN_CFG1, 0x8FFF, 0x1000, &buf[pkt_size], buf_size - pkt_size);	/* clear 0x66[14:12] then 0x66[14:12]=001 */
	} else if (space == 2) {
		pkt_size += fm_bop_modify(FM_MAIN_CFG1, 0x8FFF, 0x2000, &buf[pkt_size], buf_size - pkt_size);	/* clear 0x66[14:12] then 0x66[14:12]=010 */
	} else if (space == 4) {
		pkt_size += fm_bop_modify(FM_MAIN_CFG1, 0x8FFF, 0x4000, &buf[pkt_size], buf_size - pkt_size);	/* clear 0x66[14:12] then 0x66[14:12]=100 */
	}
	/* enable wrap , if it is not auto scan function, 0x66[11] 0=no wrarp, 1=wrap */
	pkt_size += fm_bop_modify(FM_MAIN_CFG1, 0xF7FF, 0x0800, &buf[pkt_size], buf_size - pkt_size);	/* 0x66[11] = 1, wrap */
	/* 0x66[9:0] freq upper bound */

	max_freq = (max_freq - 6400) * 2 / 10;

	pkt_size +=
	    fm_bop_modify(FM_MAIN_CFG1, 0xFC00, max_freq, &buf[pkt_size], buf_size - pkt_size);
	/* 0x67[9:0] freq lower bound */

	min_freq = (min_freq - 6400) * 2 / 10;

	pkt_size +=
	    fm_bop_modify(FM_MAIN_CFG2, 0xFC00, min_freq, &buf[pkt_size], buf_size - pkt_size);
	/* Enable hardware controlled seeking sequence */
	pkt_size += fm_bop_modify(FM_MAIN_CTRL, 0xFFF8, SEEK, &buf[pkt_size], buf_size - pkt_size);	/* 0x63[1] = 1 */
	/* Wait for STC_DONE interrupt */
	/* pkt_size += fm_bop_rd_until(FM_MAIN_INTR, FM_INTR_STC_DONE, FM_INTR_STC_DONE, &buf[pkt_size], buf_size - pkt_size);//Poll 69[0] = b'1 */
	/* Write 1 clear the STC_DONE interrupt status flag */
	/* pkt_size += fm_bop_modify(FM_MAIN_INTR, 0xFFFF, FM_INTR_STC_DONE, &buf[pkt_size], buf_size - pkt_size);//wr 69[0] = 1 */

	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);

	return pkt_size;
}


/*
 * mt6630_scan - execute scan action,
 * @buf - target buf
 * @buf_size - buffer size
 * @scandir - 0=seek up, 1=seek down
 * @space - step, 50KHz:001, 100KHz:010, 200KHz:100
 * @max_freq - upper bound
 * @min_freq - lower bound
 * return package size
 */
fm_s32 mt6630_scan(fm_u8 *buf, fm_s32 buf_size, fm_u16 scandir, fm_u16 space, fm_u16 max_freq,
		   fm_u16 min_freq)
{
	fm_s32 pkt_size = 0;

	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	if (0 == fm_get_channel_space(max_freq)) {
		max_freq *= 10;
	}
	if (0 == fm_get_channel_space(min_freq)) {
		min_freq *= 10;
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_SCAN_OPCODE;
	pkt_size = 4;

	/* Program seek direction */
	if (scandir == 0) {
		pkt_size += fm_bop_modify(FM_MAIN_CFG1, 0xFBFF, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* 0x66[10] = 0, seek up */
	} else {
		pkt_size += fm_bop_modify(FM_MAIN_CFG1, 0xFFFF, 0x0400, &buf[pkt_size], buf_size - pkt_size);	/* 0x66[10] = 1, seek down */
	}

	/* Program scan channel spacing */
	if (space == 1) {
		pkt_size += fm_bop_modify(FM_MAIN_CFG1, 0x8FFF, 0x1000, &buf[pkt_size], buf_size - pkt_size);	/* clear 0x66[14:12] then 0x66[14:12]=001 */
	} else if (space == 2) {
		pkt_size += fm_bop_modify(FM_MAIN_CFG1, 0x8FFF, 0x2000, &buf[pkt_size], buf_size - pkt_size);	/* clear 0x66[14:12] then 0x66[14:12]=010 */
	} else if (space == 4) {
		pkt_size += fm_bop_modify(FM_MAIN_CFG1, 0x8FFF, 0x4000, &buf[pkt_size], buf_size - pkt_size);	/* clear 0x66[14:12] then 0x66[14:12]=100 */
	}
	/* disable wrap , if it is auto scan function, 0x66[11] 0=no wrarp, 1=wrap */
	pkt_size += fm_bop_modify(FM_MAIN_CFG1, 0xF7FF, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* 0x66[11] = 0, no wrap */
	/* 0x66[9:0] freq upper bound */

	max_freq = (max_freq - 6400) * 2 / 10;

	pkt_size +=
	    fm_bop_modify(FM_MAIN_CFG1, 0xFC00, max_freq, &buf[pkt_size], buf_size - pkt_size);
	/* 0x67[9:0] freq lower bound */

	min_freq = (min_freq - 6400) * 2 / 10;

	pkt_size +=
	    fm_bop_modify(FM_MAIN_CFG2, 0xFC00, min_freq, &buf[pkt_size], buf_size - pkt_size);
	/* Enable hardware controlled scanning sequence */
	pkt_size += fm_bop_modify(FM_MAIN_CTRL, 0xFFF8, SCAN, &buf[pkt_size], buf_size - pkt_size);	/* 0x63[1] = 1 */
	/* Wait for STC_DONE interrupt */
	/* pkt_size += fm_bop_rd_until(FM_MAIN_INTR, FM_INTR_STC_DONE, FM_INTR_STC_DONE, &buf[pkt_size], buf_size - pkt_size);//Poll 69[0] = b'1 */
	/* Write 1 clear the STC_DONE interrupt status flag */
	/* pkt_size += fm_bop_modify(FM_MAIN_INTR, 0xFFFF, FM_INTR_STC_DONE, &buf[pkt_size], buf_size - pkt_size);//wr 69[0] = 1 */

	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);

	return pkt_size;
}


fm_s32 mt6630_cqi_get(fm_u8 *buf, fm_s32 buf_size)
{
	fm_s32 pkt_size = 0;

	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_SCAN_OPCODE;
	pkt_size = 4;

	pkt_size += fm_bop_modify(FM_MAIN_CTRL, 0xFFF0, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr 63 bit0~2 0 */
	pkt_size += fm_bop_modify(FM_MAIN_CTRL, ~CQI_READ, CQI_READ, &buf[pkt_size], buf_size - pkt_size);	/* wr 63 bit3 1 */

	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);

	return pkt_size;
}


fm_s32 mt6630_get_reg(fm_u8 *buf, fm_s32 buf_size, fm_u8 addr)
{
	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FSPI_READ_OPCODE;
	buf[2] = 0x01;
	buf[3] = 0x00;
	buf[4] = addr;

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3],
		buf[4]);
	return 5;
}


fm_s32 mt6630_set_reg(fm_u8 *buf, fm_s32 buf_size, fm_u8 addr, fm_u16 value)
{
	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FSPI_WRITE_OPCODE;
	buf[2] = 0x03;
	buf[3] = 0x00;
	buf[4] = addr;
	buf[5] = (fm_u8) ((value) & 0x00FF);
	buf[6] = (fm_u8) ((value >> 8) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2],
		buf[3], buf[4], buf[5], buf[6]);
	return 7;
}


fm_s32 mt6630_set_bits_reg(fm_u8 *buf, fm_s32 buf_size, fm_u8 addr, fm_u16 bits, fm_u16 mask)
{
	fm_s32 pkt_size = 0;
	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = 0x11;		/* 0x11 this opcode won't be parsed as an opcode, so set here as spcial case. */
	pkt_size = 4;
	pkt_size += fm_bop_modify(addr, mask, bits, &buf[pkt_size], buf_size - pkt_size);

	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);

	return pkt_size;
}

/*top register read*/
fm_s32 mt6630_top_get_reg(fm_u8 *buf, fm_s32 buf_size, fm_u16 addr)
{
	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = CSPI_READ_OPCODE;
	buf[2] = 0x03;
	buf[3] = 0x00;
	buf[4] = 0x04;		/* top 04,fm 02 */
	buf[5] = (fm_u8) ((addr) & 0x00FF);
	buf[6] = (fm_u8) ((addr >> 8) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2],
		buf[3], buf[4], buf[5], buf[6]);
	return 7;
}


fm_s32 mt6630_top_set_reg(fm_u8 *buf, fm_s32 buf_size, fm_u16 addr, fm_u32 value)
{
	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = CSPI_WRITE_OPCODE;
	buf[2] = 0x07;
	buf[3] = 0x00;
	buf[4] = 0x04;		/* top 04,fm 02 */
	buf[5] = (fm_u8) ((addr) & 0x00FF);
	buf[6] = (fm_u8) ((addr >> 8) & 0x00FF);
	buf[7] = (fm_u8) ((value) & 0x00FF);
	buf[8] = (fm_u8) ((value >> 8) & 0x00FF);
	buf[9] = (fm_u8) ((value >> 16) & 0x00FF);
	buf[10] = (fm_u8) ((value >> 24) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", buf[0],
		buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10]);
	return 11;
}

/*host register read*/
fm_s32 mt6630_host_get_reg(fm_u8 *buf, fm_s32 buf_size, fm_u32 addr)
{
	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_HOST_READ_OPCODE;
	buf[2] = 0x04;
	buf[3] = 0x00;
	buf[4] = (fm_u8) ((addr) & 0x00FF);
	buf[5] = (fm_u8) ((addr >> 8) & 0x00FF);
	buf[6] = (fm_u8) ((addr >> 16) & 0x00FF);
	buf[7] = (fm_u8) ((addr >> 24) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2],
		buf[3], buf[4], buf[5], buf[6], buf[7]);
	return 8;
}


fm_s32 mt6630_host_set_reg(fm_u8 *buf, fm_s32 buf_size, fm_u32 addr, fm_u32 value)
{
	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_HOST_WRITE_OPCODE;
	buf[2] = 0x08;
	buf[3] = 0x00;
	buf[4] = (fm_u8) ((addr) & 0x00FF);
	buf[5] = (fm_u8) ((addr >> 8) & 0x00FF);
	buf[6] = (fm_u8) ((addr >> 16) & 0x00FF);
	buf[7] = (fm_u8) ((addr >> 24) & 0x00FF);
	buf[8] = (fm_u8) ((value) & 0x00FF);
	buf[9] = (fm_u8) ((value >> 8) & 0x00FF);
	buf[10] = (fm_u8) ((value >> 16) & 0x00FF);
	buf[11] = (fm_u8) ((value >> 24) & 0x00FF);

	WCN_DBG(FM_DBG | CHIP, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9],
		buf[10], buf[11]);
	return 12;
}

/********************************Tx function***********************************************/
/*
 * mt6630_pwrup_clock_on_tx - FM tx Digital Clock enable
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
fm_s32 mt6630_pwrup_clock_on_tx(fm_u8 *buf, fm_s32 buf_size)
{
	fm_s32 pkt_size = 0;

	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_ENABLE_OPCODE;
	pkt_size = 4;

	/* B1.0    Enable digital OSC */
	pkt_size += fm_bop_write(0x60, 0x0003, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 3 */
	pkt_size += fm_bop_udelay(100, &buf[pkt_size], buf_size - pkt_size);	/* delay 100us */
	/* B1.2    Release HW clock gating */
	pkt_size += fm_bop_write(0x60, 0x0007, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 7 */
	if (mt6630_fm_config.aud_cfg.aud_path == FM_AUD_ANALOG) {
		pkt_size +=
		    fm_bop_modify(0x61, 0xFF7F, 0x0000, &buf[pkt_size], buf_size - pkt_size);
	} else {
		pkt_size +=
		    fm_bop_modify(0x61, 0xFF7F, 0x0080, &buf[pkt_size], buf_size - pkt_size);
	}
	/* B1.4    set TX mode: 0909 sequence */
	pkt_size += fm_bop_write(0xC7, 0x8286, &buf[pkt_size], buf_size - pkt_size);	/* wr C7 8286 */

	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);

	return pkt_size;
}

/*
 * mt6630_pwrup_tx_deviation - default deviation (RDS off)
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
fm_s32 mt6630_pwrup_tx_deviation(fm_u8 *buf, fm_s32 buf_size)
{
	fm_s32 pkt_size = 0;

	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_ENABLE_OPCODE;
	pkt_size = 4;

	/* A1  switch to host control */
	pkt_size += fm_bop_write(0x60, 0x0007, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 0007 */
	/* set rgf_tx_beta_sum */
	pkt_size += fm_bop_write(0xCD, 0x72D2, &buf[pkt_size], buf_size - pkt_size);	/* wr CD 72D2 */
	/* set rgf_tx_beta_diff */
	pkt_size += fm_bop_write(0xCF, 0x787B, &buf[pkt_size], buf_size - pkt_size);	/* wr CF 787B */
	/* set rgf_tx_beta_rds */
	pkt_size += fm_bop_write(0xCE, 0x0785, &buf[pkt_size], buf_size - pkt_size);	/* wr CE 785 */
	/* set rgf_tx_beta_pilot */
	pkt_size += fm_bop_write(0xCC, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr CC 0 */
	/* set  rgf_phase_gen_rsh */
	pkt_size += fm_bop_modify(0xAD, 0xFFE8, 0x0001, &buf[pkt_size], buf_size - pkt_size);	/* wr AD D4 D2:D0=1 */
	/* set rgf_phase_gen_wb */
	pkt_size += fm_bop_modify(0xA8, 0xF000, 0x0F16, &buf[pkt_size], buf_size - pkt_size);	/* wr A8 D11:D0=F16 */
	/* set agc */
	pkt_size += fm_bop_modify(0xAE, 0xFC00, 0x020B, &buf[pkt_size], buf_size - pkt_size);	/* wr AE D9:D0=20B */
	/* set rgf_beta_fm */
	pkt_size += fm_bop_write(0xEE, 0x623D, &buf[pkt_size], buf_size - pkt_size);	/* wr EE 623D */
	/* switch to DSP control */
	pkt_size += fm_bop_write(0x60, 0x000F, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 000F */

	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);

	return pkt_size;
}

/*
 * mt6630_tx_rdsoff_deviation -  deviation (RDS on)
 * @buf - target buf
 * @buf_size - buffer size
 * return package size
 */
fm_s32 mt6630_tx_rdson_deviation(fm_u8 *buf, fm_s32 buf_size)
{
	fm_s32 pkt_size = 0;

	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = RDS_TX_OPCODE;
	pkt_size = 4;

	/* A1  switch to host control */
	pkt_size += fm_bop_write(0x60, 0x0007, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 0007 */
	/* set rgf_tx_beta_sum */
	pkt_size += fm_bop_write(0xCD, 0x70E3, &buf[pkt_size], buf_size - pkt_size);	/* wr CD 70E3 */
	/* set rgf_tx_beta_diff */
	pkt_size += fm_bop_write(0xCF, 0x7675, &buf[pkt_size], buf_size - pkt_size);	/* wr CF 7675 */
	/* set rgf_tx_beta_rds:0909 sequence */
	pkt_size += fm_bop_write(0xCC, 0x0227, &buf[pkt_size], buf_size - pkt_size);	/* wr CC 227 */
	/* set rgf_tx_beta_pilot :0909 sequence */
	pkt_size += fm_bop_write(0xCE, 0x0764, &buf[pkt_size], buf_size - pkt_size);	/* wr CE 764 */
	/* set  rgf_phase_gen_rsh */
	pkt_size += fm_bop_modify(0xAD, 0xFFEF, 0x0000, &buf[pkt_size], buf_size - pkt_size);	/* wr AD D4 =0 */
	pkt_size += fm_bop_modify(0xAD, 0xFFF8, 0x0001, &buf[pkt_size], buf_size - pkt_size);	/* wr AD D2:D0=1 */
	/* set rgf_phase_gen_wb */
	pkt_size += fm_bop_modify(0xA8, 0xF000, 0x0222, &buf[pkt_size], buf_size - pkt_size);	/* wr A8 D11:D0=222 */
	/* set agc */
	pkt_size += fm_bop_modify(0xAE, 0xFC00, 0x0203, &buf[pkt_size], buf_size - pkt_size);	/* wr AE D9:D0=203 */
	/* set rgf_beta_fm */
	pkt_size += fm_bop_write(0xEE, 0x63EB, &buf[pkt_size], buf_size - pkt_size);	/* wr EE 63EB */
	/* switch to DSP control */
	pkt_size += fm_bop_write(0x60, 0x000F, &buf[pkt_size], buf_size - pkt_size);	/* wr 60 000F */

	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);

	return pkt_size;
}

/*
 * mt6630_tune_tx - execute tx tune action,
 * @buf - target buf
 * @buf_size - buffer size
 * @freq - 760 ~ 1080, 100KHz unit
 * return package size
 */
fm_s32 mt6630_tune_tx(fm_u8 *buf, fm_s32 buf_size, fm_u16 freq, fm_u16 chan_para)
{
	/* #define FM_TUNE_USE_POLL */
	fm_s32 pkt_size = 0;

	if (buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	if (0 == fm_get_channel_space(freq)) {
		freq *= 10;
	}

	freq = (freq - 6400) * 2 / 10;

	buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	buf[1] = FM_TUNE_OPCODE;
	pkt_size = 4;

	/* Set desired channel & channel parameter */
#ifdef FM_TUNE_USE_POLL
	pkt_size += fm_bop_write(0x6B, 0x0000, &buf[pkt_size], buf_size - pkt_size);
#endif
	/* sequence 09/16:0x65 D12=1 for iq switch */
	pkt_size += fm_bop_modify(FM_CHANNEL_SET, 0xEC00, freq | 0x1000, &buf[pkt_size], buf_size - pkt_size);	/* set 0x65[9:0] = 0x029e, => ((97.5 - 64) * 20) */
	/* set iq switch, D12 */
	/* pkt_size += fm_bop_modify(FM_CHANNEL_SET, 0x0FFF, (chan_para << 12), &buf[pkt_size], buf_size - pkt_size); */
	/* Enable hardware controlled tuning sequence */
	pkt_size += fm_bop_modify(FM_MAIN_CTRL, 0xFFF8, TUNE, &buf[pkt_size], buf_size - pkt_size);	/* set 0x63[0] = 1 */
	/* Wait for STC_DONE interrupt */
#ifdef FM_TUNE_USE_POLL
	pkt_size += fm_bop_rd_until(FM_MAIN_INTR, FM_INTR_STC_DONE, FM_INTR_STC_DONE, &buf[pkt_size], buf_size - pkt_size);	/* Poll 69[0] = b'1 */
	/* Write 1 clear the STC_DONE interrupt status flag */
	pkt_size += fm_bop_modify(FM_MAIN_INTR, 0xFFFF, FM_INTR_STC_DONE, &buf[pkt_size], buf_size - pkt_size);	/* wr 69[0] = 1 */
#endif
	buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);

	return pkt_size;
}

/*
pi: pi code
ps: block B,C,D
other_rds: unused
other_rds_cnt: unused
*/
fm_s32 mt6630_rds_tx(fm_u8 *tx_buf, fm_s32 tx_buf_size, fm_u16 pi, fm_u16 *ps, fm_u16 *other_rds,
		     fm_u8 other_rds_cnt)
{
	fm_s32 pkt_size = 0;
	fm_s32 i;

	if (tx_buf_size < TX_BUF_SIZE) {
		return (-1);
	}

	tx_buf[0] = FM_TASK_COMMAND_PKT_TYPE;
	tx_buf[1] = RDS_TX_OPCODE;
	pkt_size = 4;

	/* set repeat mode */
	pkt_size += fm_bop_modify(0x88, 0xFFFE, 0x0001, &tx_buf[pkt_size], TX_BUF_SIZE - pkt_size);	/* wr 88[0] = b'1, repeat mode */
	pkt_size += fm_bop_modify(0x88, 0xFFFB, 0x0004, &tx_buf[pkt_size], TX_BUF_SIZE - pkt_size);	/* wr 88[2] = b'1, PI_reg mode */
	pkt_size += fm_bop_write(0x8A, pi, &tx_buf[pkt_size], tx_buf_size - pkt_size);	/* write PI to PI_reg */

	pkt_size += fm_bop_modify(0x88, 0xFFFD, 0x0002, &tx_buf[pkt_size], TX_BUF_SIZE - pkt_size);	/* wr 88[1] = b'1, addr from host */
	for (i = 0; i < 12; i++) {
		pkt_size += fm_bop_write(0x8B, (0x0063 + i), &tx_buf[pkt_size], tx_buf_size - pkt_size);	/* 8B = mem_addr */
		pkt_size += fm_bop_write(0x8C, ps[i], &tx_buf[pkt_size], tx_buf_size - pkt_size);	/* 8C = RDS Tx data */
	}
	pkt_size += fm_bop_modify(0x88, 0xFFFD, 0x0000, &tx_buf[pkt_size], TX_BUF_SIZE - pkt_size);	/* wr 88[1] = b'0, clear mem_addr */
	pkt_size += fm_bop_modify(0x88, 0xFFEF, 0x0010, &tx_buf[pkt_size], TX_BUF_SIZE - pkt_size);	/* wr 88[4] = b'1, switch to ps buf */
	/* work around: write at leat one group to normal buffer, otherwise ps buffer can be sent out. */
	pkt_size += fm_bop_write(0x8C, 0, &tx_buf[pkt_size], tx_buf_size - pkt_size);
	pkt_size += fm_bop_write(0x8C, 0, &tx_buf[pkt_size], tx_buf_size - pkt_size);
	pkt_size += fm_bop_write(0x8C, 0, &tx_buf[pkt_size], tx_buf_size - pkt_size);
	pkt_size += fm_bop_write(0x8C, 0, &tx_buf[pkt_size], tx_buf_size - pkt_size);
	pkt_size += fm_bop_modify(0x88, 0xFFDF, 0x0020, &tx_buf[pkt_size], TX_BUF_SIZE - pkt_size);	/* wr 88[5] = b'1,clear in_ptr */
	pkt_size += fm_bop_modify(0x88, 0xFFDF, 0x0000, &tx_buf[pkt_size], TX_BUF_SIZE - pkt_size);	/* wr 88[5] = b'0,clear in_ptr */

	tx_buf[2] = (fm_u8) ((pkt_size - 4) & 0x00FF);
	tx_buf[3] = (fm_u8) (((pkt_size - 4) >> 8) & 0x00FF);

	return pkt_size;
}
