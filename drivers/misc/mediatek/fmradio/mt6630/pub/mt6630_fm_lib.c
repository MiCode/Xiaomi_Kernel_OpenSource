#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "stp_exp.h"
#include "wmt_exp.h"

#include "fm_typedef.h"
#include "fm_dbg.h"
#include "fm_err.h"
#include "fm_interface.h"
#include "fm_stdlib.h"
#include "fm_patch.h"
#include "fm_utils.h"
#include "fm_link.h"
#include "fm_config.h"
#include "fm_private.h"

#include "mt6630_fm_reg.h"
#include "mt6630_fm.h"
#include "mt6630_fm_lib.h"
#include "mt6630_fm_cmd.h"
#include "mt6630_fm_cust_cfg.h"
extern fm_cust_cfg mt6630_fm_config;

static struct fm_patch_tbl mt6630_patch_tbl[5] = {
	{FM_ROM_V1, "/etc/firmware/mt6630/mt6630_fm_v1_patch.bin",
	 "/etc/firmware/mt6630/mt6630_fm_v1_coeff.bin", NULL, NULL},
	{FM_ROM_V2, "/etc/firmware/mt6630/mt6630_fm_v2_patch.bin",
	 "/etc/firmware/mt6630/mt6630_fm_v2_coeff.bin", NULL, NULL},
	{FM_ROM_V3, "/etc/firmware/mt6630/mt6630_fm_v3_patch.bin",
	 "/etc/firmware/mt6630/mt6630_fm_v3_coeff.bin", NULL, NULL},
	{FM_ROM_V4, "/etc/firmware/mt6630/mt6630_fm_v4_patch.bin",
	 "/etc/firmware/mt6630/mt6630_fm_v4_coeff.bin", NULL, NULL},
	{FM_ROM_V5, "/etc/firmware/mt6630/mt6630_fm_v5_patch.bin",
	 "/etc/firmware/mt6630/mt6630_fm_v5_coeff.bin", NULL, NULL},
};

static struct fm_patch_tbl mt6630_patch_tbl_tx[5] = {
	{FM_ROM_V1, "/etc/firmware/mt6630/mt6630_fm_v1_patch_tx.bin",
	 "/etc/firmware/mt6630/mt6630_fm_v1_coeff_tx.bin", NULL, NULL},
	{FM_ROM_V2, "/etc/firmware/mt6630/mt6630_fm_v2_patch_tx.bin",
	 "/etc/firmware/mt6630/mt6630_fm_v2_coeff_tx.bin", NULL, NULL},
	{FM_ROM_V3, "/etc/firmware/mt6630/mt6630_fm_v3_patch_tx.bin",
	 "/etc/firmware/mt6630/mt6630_fm_v3_coeff_tx.bin", NULL, NULL},
	{FM_ROM_V4, "/etc/firmware/mt6630/mt6630_fm_v4_patch_tx.bin",
	 "/etc/firmware/mt6630/mt6630_fm_v4_coeff_tx.bin", NULL, NULL},
	{FM_ROM_V5, "/etc/firmware/mt6630/mt6630_fm_v5_patch_tx.bin",
	 "/etc/firmware/mt6630/mt6630_fm_v5_coeff_tx.bin", NULL, NULL},
};

static struct fm_hw_info mt6630_hw_info = {
	.chip_id = 0x00006630,
	.eco_ver = 0x00000000,
	.rom_ver = 0x00000000,
	.patch_ver = 0x00000000,
	.reserve = 0x00000000,
};

#define PATCH_SEG_LEN 512

static fm_u8 *cmd_buf;
static struct fm_lock *cmd_buf_lock;
static struct fm_callback *fm_cb_op;
static struct fm_res_ctx *mt6630_res;
static fm_u8 fm_packaging = 1;	/*0:QFN,1:WLCSP */
static fm_u32 fm_sant_flag;	/* 1,Short Antenna;  0, Long Antenna */
static fm_s32 mt6630_is_dese_chan(fm_u16 freq);
#if 0
static fm_s32 mt6630_mcu_dese(fm_u16 freq, void *arg);
#endif
static fm_s32 mt6630_gps_dese(fm_u16 freq, void *arg);

static fm_s32 mt6630_I2s_Setting(fm_s32 onoff, fm_s32 mode, fm_s32 sample);
static fm_u16 mt6630_chan_para_get(fm_u16 freq);
static fm_s32 mt6630_desense_check(fm_u16 freq, fm_s32 rssi);
static fm_bool mt6630_TDD_chan_check(fm_u16 freq);
static fm_s32 mt6630_soft_mute_tune(fm_u16 freq, fm_s32 *rssi, fm_bool *valid);
static fm_s32 mt6630_pwron(fm_s32 data)
{
	if (MTK_WCN_BOOL_FALSE == mtk_wcn_wmt_func_on(WMTDRV_TYPE_FM)) {
		FM_LOG_ERR(FM_ERR | CHIP, "WMT turn on FM Fail!\n");
		return -FM_ELINK;
	} else {
		FM_LOG_NTC(FM_NTC | CHIP, "WMT turn on FM OK!\n");
		return 0;
	}
}

static fm_s32 mt6630_pwroff(fm_s32 data)
{
	if (MTK_WCN_BOOL_FALSE == mtk_wcn_wmt_func_off(WMTDRV_TYPE_FM)) {
		FM_LOG_ERR(FM_ERR | CHIP, "WMT turn off FM Fail!\n");
		return -FM_ELINK;
	} else {
		FM_LOG_NTC(FM_NTC | CHIP, "WMT turn off FM OK!\n");
		return 0;
	}
}

static fm_s32 Delayms(fm_u32 data)
{
	FM_LOG_DBG(FM_DBG | CHIP, "delay %dms\n", data);
	msleep(data);
	return 0;
}

static fm_s32 Delayus(fm_u32 data)
{
	FM_LOG_DBG(FM_DBG | CHIP, "delay %dus\n", data);
	udelay(data);
	return 0;
}

fm_s32 mt6630_get_read_result(struct fm_res_ctx *result)
{
	FMR_ASSERT(result);
	mt6630_res = result;

	return 0;
}

static fm_s32 mt6630_read(fm_u8 addr, fm_u16 *val)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6630_get_reg(cmd_buf, TX_BUF_SIZE, addr);
	ret =
	    fm_cmd_tx(cmd_buf, pkt_size, FLAG_FSPI_RD, SW_RETRY_CNT, FSPI_RD_TIMEOUT,
		      mt6630_get_read_result);

	if (!ret && mt6630_res) {
		*val = mt6630_res->fspi_rd;
	}

	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

static fm_s32 mt6630_write(fm_u8 addr, fm_u16 val)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6630_set_reg(cmd_buf, TX_BUF_SIZE, addr, val);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_FSPI_WR, SW_RETRY_CNT, FSPI_WR_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

static fm_s32 mt6630_set_bits(fm_u8 addr, fm_u16 bits, fm_u16 mask)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6630_set_bits_reg(cmd_buf, TX_BUF_SIZE, addr, bits, mask);
	ret = fm_cmd_tx(cmd_buf, pkt_size, (1 << 0x11), SW_RETRY_CNT, FSPI_WR_TIMEOUT, NULL);	/* 0x11 this opcode won't be parsed as an opcode, so set here as spcial case. */
	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

static fm_s32 mt6630_host_read(fm_u32 addr, fm_u32 *val)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6630_host_get_reg(cmd_buf, TX_BUF_SIZE, addr);
	ret =
	    fm_cmd_tx(cmd_buf, pkt_size, FLAG_HOST_READ, SW_RETRY_CNT, FSPI_RD_TIMEOUT,
		      mt6630_get_read_result);

	if (!ret && mt6630_res) {
		*val = mt6630_res->cspi_rd;
	}

	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

static fm_s32 mt6630_host_write(fm_u32 addr, fm_u32 val)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6630_host_set_reg(cmd_buf, TX_BUF_SIZE, addr, val);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_HOST_WRITE, SW_RETRY_CNT, FSPI_WR_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

static fm_u16 mt6630_get_chipid(void)
{
	return 0x6630;
}

/*  MT6630_SetAntennaType - set Antenna type
 *  @type - 1,Short Antenna;  0, Long Antenna
 */
static fm_s32 mt6630_SetAntennaType(fm_s32 type)
{
	fm_u16 dataRead;

	FM_LOG_NTC(FM_NTC | CHIP, "set ana to %s\n", type ? "short" : "long");
	if (fm_packaging == 0) {
		fm_sant_flag = type;
	} else {
		mt6630_read(FM_MAIN_CG2_CTRL, &dataRead);

		if (type) {
			dataRead |= ANTENNA_TYPE;
		} else {
			dataRead &= (~ANTENNA_TYPE);
		}

		mt6630_write(FM_MAIN_CG2_CTRL, dataRead);
	}
	return 0;
}

static fm_s32 mt6630_GetAntennaType(void)
{
	fm_u16 dataRead;

	if (fm_packaging == 0) {
		return fm_sant_flag;
	} else {
		mt6630_read(FM_MAIN_CG2_CTRL, &dataRead);
		FM_LOG_NTC(FM_NTC | CHIP, "get ana type: %s\n",
			   (dataRead & ANTENNA_TYPE) ? "short" : "long");

		if (dataRead & ANTENNA_TYPE)
			return FM_ANA_SHORT;	/* short antenna */
		else
			return FM_ANA_LONG;	/* long antenna */
	}
}


static fm_s32 mt6630_Mute(fm_bool mute)
{
	fm_s32 ret = 0;
	fm_u16 dataRead;

	FM_LOG_NTC(FM_NTC | CHIP, "set %s\n", mute ? "mute" : "unmute");
	mt6630_read(FM_MAIN_CTRL, &dataRead);

	if (mute == 1) 
	{
		ret = mt6630_write(FM_MAIN_CTRL, (dataRead&0xFFDF) | 0x0020);
	} 
	else 
	{
		ret = mt6630_write(FM_MAIN_CTRL, (dataRead&0xFFDF));
	}
    return ret;
}

static fm_s32 mt6630_RampDown(void)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;
	/* fm_u16 tmp; */

	FM_LOG_NTC(FM_NTC | CHIP, "ramp down\n");

	ret = mt6630_write(FM_MAIN_EXTINTRMASK, 0x0000);
	if (ret) {
		FM_LOG_ERR(FM_ERR |CHIP, "ramp down write FM_MAIN_EXTINTRMASK failed\n");
		return ret;
	}

	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6630_rampdown(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_RAMPDOWN, SW_RETRY_CNT, RAMPDOWN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		FM_LOG_ERR(FM_ERR |CHIP, "ramp down failed\n");
		return ret;
	}

	ret = mt6630_write(FM_MAIN_EXTINTRMASK, 0x0021);
	if (ret) {
		FM_LOG_ERR(FM_ERR |CHIP, "ramp down write FM_MAIN_EXTINTRMASK failed\n");
	}

	return ret;
}

static fm_s32 mt6630_get_rom_version(void)
{
	fm_u16 tmp;
	fm_s32 ret;

	/* DSP rom code version request enable --- set 0x61 b15=1 */
	mt6630_set_bits(0x61, 0x8000, 0x7FFF);

	/* Release ASIP reset --- set 0x61 b1=1 */
	mt6630_set_bits(0x61, 0x0002, 0xFFFD);

	/* Enable ASIP power --- set 0x61 b0=0 */
	mt6630_set_bits(0x61, 0x0000, 0xFFFE);

	/* Wait DSP code version ready --- wait 1ms */
	do {
		Delayus(1000);
		ret = mt6630_read(0x84, &tmp);
		/* ret=-4 means signal got when control FM. usually get sig 9 to kill FM process. */
		/* now cancel FM power up sequence is recommended. */
		if (ret) {
			return ret;
		}
		FM_LOG_DBG(FM_DBG |CHIP, "0x84=%x\n", tmp);
	} while (tmp != 0x0001);

	/* Get FM DSP code version --- rd 0x83[15:8] */
	mt6630_read(0x83, &tmp);
	FM_LOG_NTC(FM_NTC | CHIP, "DSP ver=0x%x\n", tmp);
	tmp = (tmp >> 8);

	/* DSP rom code version request disable --- set 0x61 b15=0 */
	mt6630_set_bits(0x61, 0x0000, 0x7FFF);

	/* Reset ASIP --- set 0x61[1:0] = 1 */
	mt6630_set_bits(0x61, 0x0001, 0xFFFC);

	/* FM_LOG_NTC(CHIP, "ROM version: v%d\n", (fm_s32)tmp); */
	return (fm_s32) tmp;
}

static fm_s32 mt6630_get_patch_path(fm_s32 ver, const fm_s8 **ppath,
				    struct fm_patch_tbl *patch_tbl)
{
	fm_s32 i;
	fm_s32 max = FM_ROM_MAX;

	/* check if the ROM version is defined or not */
	for (i = 0; i < max; i++) {
		if ((patch_tbl[i].idx == ver) && (fm_file_exist(patch_tbl[i].patch) == 0)) {
			*ppath = patch_tbl[i].patch;
			FM_LOG_NTC(FM_NTC | CHIP, "Get ROM version OK\n");
			return 0;
		}
	}

	/* the ROM version isn't defined, find a latest patch instead */
	for (i = max; i > 0; i--) {
		if (fm_file_exist(patch_tbl[i - 1].patch) == 0) {
			*ppath = patch_tbl[i - 1].patch;
			FM_LOG_ERR(FM_ERR | CHIP, "undefined ROM version\n");
			return 0;
		}
	}

	/* get path failed */
	FM_LOG_ERR(FM_ERR | CHIP, "No valid patch file\n");
	return -FM_EPATCH;
}


static fm_s32 mt6630_get_coeff_path(fm_s32 ver, const fm_s8 **ppath,
				    struct fm_patch_tbl *patch_tbl)
{
	fm_s32 i;
	fm_s32 max = FM_ROM_MAX;

	/* check if the ROM version is defined or not */
	for (i = 0; i < max; i++) {
		if ((patch_tbl[i].idx == ver) && (fm_file_exist(patch_tbl[i].coeff) == 0)) {
			*ppath = patch_tbl[i].coeff;
			FM_LOG_NTC(FM_NTC | CHIP, "Get ROM version OK\n");
			return 0;
		}
	}


	/* the ROM version isn't defined, find a latest patch instead */
	for (i = max; i > 0; i--) {
		if (fm_file_exist(patch_tbl[i - 1].coeff) == 0) {
			*ppath = patch_tbl[i - 1].coeff;
			FM_LOG_ERR(FM_ERR | CHIP, "undefined ROM version\n");
			return 1;
		}
	}

	/* get path failed */
	FM_LOG_ERR(FM_ERR | CHIP, "No valid coeff file\n");
	return -FM_EPATCH;
}

/*
*  mt6630_DspPatch - DSP download procedure
*  @img - source dsp bin code
*  @len - patch length in byte
*  @type - rom/patch/coefficient/hw_coefficient
*/
static fm_s32 mt6630_DspPatch(const fm_u8 *img, fm_s32 len, enum IMG_TYPE type)
{
	fm_u8 seg_num;
	fm_u8 seg_id = 0;
	fm_s32 seg_len;
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	FMR_ASSERT(img);

	if (len <= 0) {
		return -1;
	}

	seg_num = len / PATCH_SEG_LEN + 1;
	FM_LOG_NTC(FM_NTC | CHIP, "binary len:%d, seg num:%d\n", len, seg_num);

	switch (type) {
#if 0
	case IMG_ROM:

		for (seg_id = 0; seg_id < seg_num; seg_id++) {
			seg_len = ((seg_id + 1) < seg_num) ? PATCH_SEG_LEN : (len % PATCH_SEG_LEN);
			FM_LOG_NTC(CHIP, "rom,[seg_id:%d],  [seg_len:%d]\n", seg_id, seg_len);
			if (FM_LOCK(cmd_buf_lock))
				return (-FM_ELOCK);
			pkt_size =
			    mt6630_rom_download(cmd_buf, TX_BUF_SIZE, seg_num, seg_id,
						&img[seg_id * PATCH_SEG_LEN], seg_len);
			FM_LOG_NTC(CHIP, "pkt_size:%d\n", (fm_s32) pkt_size);
			ret =
			    fm_cmd_tx(cmd_buf, pkt_size, FLAG_ROM, SW_RETRY_CNT, ROM_TIMEOUT, NULL);
			FM_UNLOCK(cmd_buf_lock);

			if (ret) {
				FM_LOG_ERR(CHIP, "mt6630_rom_download failed\n");
				return ret;
			}
		}

		break;
#endif
	case IMG_PATCH:

		for (seg_id = 0; seg_id < seg_num; seg_id++) {
			seg_len = ((seg_id + 1) < seg_num) ? PATCH_SEG_LEN : (len % PATCH_SEG_LEN);
			FM_LOG_NTC(FM_NTC | CHIP, "patch,[seg_id:%d],  [seg_len:%d]\n", seg_id, seg_len);
			if (FM_LOCK(cmd_buf_lock))
				return (-FM_ELOCK);
			pkt_size =
			    mt6630_patch_download(cmd_buf, TX_BUF_SIZE, seg_num, seg_id,
						  &img[seg_id * PATCH_SEG_LEN], seg_len);
			FM_LOG_NTC(FM_NTC | CHIP, "pkt_size:%d\n", (fm_s32) pkt_size);
			ret =
			    fm_cmd_tx(cmd_buf, pkt_size, FLAG_PATCH, SW_RETRY_CNT, PATCH_TIMEOUT,
				      NULL);
			FM_UNLOCK(cmd_buf_lock);

			if (ret) {
				FM_LOG_ERR(FM_ERR | CHIP, "mt6630_patch_download failed\n");
				return ret;
			}
		}

		break;
#if 0
	case IMG_HW_COEFFICIENT:

		for (seg_id = 0; seg_id < seg_num; seg_id++) {
			seg_len = ((seg_id + 1) < seg_num) ? PATCH_SEG_LEN : (len % PATCH_SEG_LEN);
			FM_LOG_NTC(CHIP, "hwcoeff,[seg_id:%d],  [seg_len:%d]\n", seg_id, seg_len);
			if (FM_LOCK(cmd_buf_lock))
				return (-FM_ELOCK);
			pkt_size =
			    mt6630_hwcoeff_download(cmd_buf, TX_BUF_SIZE, seg_num, seg_id,
						    &img[seg_id * PATCH_SEG_LEN], seg_len);
			FM_LOG_NTC(CHIP, "pkt_size:%d\n", (fm_s32) pkt_size);
			ret =
			    fm_cmd_tx(cmd_buf, pkt_size, FLAG_HWCOEFF, SW_RETRY_CNT,
				      HWCOEFF_TIMEOUT, NULL);
			FM_UNLOCK(cmd_buf_lock);

			if (ret) {
				FM_LOG_ERR(CHIP, "mt6630_hwcoeff_download failed\n");
				return ret;
			}
		}

		break;
#endif
	case IMG_COEFFICIENT:

		for (seg_id = 0; seg_id < seg_num; seg_id++) {
			seg_len = ((seg_id + 1) < seg_num) ? PATCH_SEG_LEN : (len % PATCH_SEG_LEN);
			FM_LOG_NTC(FM_NTC | CHIP, "coeff,[seg_id:%d],  [seg_len:%d]\n", seg_id, seg_len);
			if (FM_LOCK(cmd_buf_lock))
				return (-FM_ELOCK);
			pkt_size =
			    mt6630_coeff_download(cmd_buf, TX_BUF_SIZE, seg_num, seg_id,
						  &img[seg_id * PATCH_SEG_LEN], seg_len);
			FM_LOG_NTC(FM_NTC | CHIP, "pkt_size:%d\n", (fm_s32) pkt_size);
			ret =
			    fm_cmd_tx(cmd_buf, pkt_size, FLAG_COEFF, SW_RETRY_CNT, COEFF_TIMEOUT,
				      NULL);
			FM_UNLOCK(cmd_buf_lock);

			if (ret) {
				FM_LOG_ERR(FM_ERR | CHIP, "mt6630_coeff_download failed\n");
				return ret;
			}
		}

		break;
	default:
		break;
	}

	return 0;
}

static fm_s32 mt6630_pwrup_top_setting(void)
{
	fm_s32 ret = 0, value = 0;
	/* A0.1 Turn on FM buffer */
	ret = mt6630_host_read(0x8102123c, &value);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x8102123c rd failed\n");
		return ret;
	}
	ret = mt6630_host_write(0x8102123c, value & 0xFFFFFFBF);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x8102123c wr failed\n");
		return ret;
	}
	/* A0.2 Set xtal no off when FM on */
	ret = mt6630_host_read(0x81021134, &value);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81021134 rd failed\n");
		return ret;
	}
	ret = mt6630_host_write(0x81021134, value | 0x80);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81021134 wr failed\n");
		return ret;
	}
	/* A0.3 Set top off always on when FM on */
	ret = mt6630_host_read(0x81020010, &value);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81020010 rd failed\n");
		return ret;
	}
	ret = mt6630_host_write(0x81020010, value & 0xFFFDFFFF);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81020010 wr failed\n");
		return ret;
	}
	/* A0.4 Always enable PALDO when FM on */
	ret = mt6630_host_read(0x81021430, &value);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81021430 rd failed\n");
		return ret;
	}
	ret = mt6630_host_write(0x81021430, value | 0x80000000);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81021430 wr failed\n");
		return ret;
	}
	/* A0.5 */
	Delayus(240);

	/* A0.6 MTCMOS Control */
	ret = mt6630_host_read(0x81020008, &value);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81020008 rd failed\n");
		return ret;
	}
	ret = mt6630_host_write(0x81020008, value | 0x00000030);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81020008 wr failed\n");
		return ret;
	}
	/* A0.7 */
	Delayus(20);

	/* A0.8 release power on reset */
	ret = mt6630_host_read(0x81020008, &value);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81020008 rd failed\n");
		return ret;
	}
	ret = mt6630_host_write(0x81020008, value | 0x00000001);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81020008 wr failed\n");
		return ret;
	}
	/* A0.9 enable fspi_mas_bclk_ck */
	ret = mt6630_host_read(0x80000108, &value);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x80000108 rd failed\n");
		return ret;
	}
	ret = mt6630_host_write(0x80000108, value | 0x00000100);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x80000108 wr failed\n");
		return ret;
	}
	return ret;
}

static fm_s32 mt6630_pwrdown_top_setting(void)
{
	fm_s32 ret = 0, value = 0;
	/* B0.1 disable fspi_mas_bclk_ck */
	ret = mt6630_host_read(0x80000104, &value);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x80000104 rd failed\n");
		return ret;
	}
	ret = mt6630_host_write(0x80000104, value | 0x00000100);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x80000104 wr failed\n");
		return ret;
	}
	/* B0.2 set power off reset */
	ret = mt6630_host_read(0x81020008, &value);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81020008 rd failed\n");
		return ret;
	}
	ret = mt6630_host_write(0x81020008, value & 0xFFFFFFFE);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81020008 wr failed\n");
		return ret;
	}
	/* B0.3 */
	Delayus(20);

	/* B0.4 disable MTCMOS & set Iso_en */
	ret = mt6630_host_read(0x81020008, &value);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81020008 rd failed\n");
		return ret;
	}
	ret = mt6630_host_write(0x81020008, value & 0xFFFFFFEF);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81020008 wr failed\n");
		return ret;
	}
	/* B0.5 Turn off FM buffer */
	ret = mt6630_host_read(0x8102123c, &value);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x8102123c rd failed\n");
		return ret;
	}
	ret = mt6630_host_write(0x8102123c, value | 0x40);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x8102123c wr failed\n");
		return ret;
	}
	/* B0.6 Clear xtal no off when FM off */
	ret = mt6630_host_read(0x81021134, &value);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81021134 rd failed\n");
		return ret;
	}
	ret = mt6630_host_write(0x81021134, value & 0xFFFFFF7F);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81021134 wr failed\n");
		return ret;
	}
	/* B0.7 Clear top off always on when FM off */
	ret = mt6630_host_read(0x81020010, &value);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81020010 rd failed\n");
		return ret;
	}
	ret = mt6630_host_write(0x81020010, value | 0x20000);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81020010 wr failed\n");
		return ret;
	}
	/* B0.9 Disable PALDO when FM off */
	ret = mt6630_host_read(0x81021430, &value);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81021430 rd failed\n");
		return ret;
	}
	ret = mt6630_host_write(0x81021430, value & 0x7FFFFFFF);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " 0x81021430 wr failed\n");
		return ret;
	}

	return ret;
}

static fm_s32 mt6630_pwrup_DSP_download(struct fm_patch_tbl *patch_tbl)
{
#define PATCH_BUF_SIZE 4096*6
	fm_s32 ret = 0;
	const fm_s8 *path_patch = NULL;
	const fm_s8 *path_coeff = NULL;
	fm_s32 patch_len = 0;
	fm_u8 *dsp_buf = NULL;
	fm_u16 tmp_reg = 0;

	mt6630_hw_info.eco_ver = (fm_s32) mtk_wcn_wmt_hwver_get();
	FM_LOG_NTC(CHIP, "ECO version:0x%08x\n", mt6630_hw_info.eco_ver);
	mt6630_hw_info.eco_ver += 1;

	/* FM ROM code version request */
	if ((ret = mt6630_get_rom_version()) >= 0) {
		mt6630_hw_info.rom_ver = ret;
		FM_LOG_NTC(FM_NTC | CHIP, "ROM version: v%d\n", mt6630_hw_info.rom_ver);
	} else {
		FM_LOG_ERR(FM_ERR | CHIP, "get ROM version failed\n");
		/* ret=-4 means signal got when control FM. usually get sig 9 to kill FM process. */
		/* now cancel FM power up sequence is recommended. */
		return ret;
	}

	/* Wholechip FM Power Up: step 3, download patch */
	if (!(dsp_buf = fm_vmalloc(PATCH_BUF_SIZE))) {
		FM_LOG_ERR(FM_ERR | CHIP, "-ENOMEM\n");
		return -ENOMEM;
	}

	ret = mt6630_get_patch_path(mt6630_hw_info.rom_ver, &path_patch, patch_tbl);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " mt6630_get_patch_path failed\n");
		goto out;
	}
	patch_len = fm_file_read(path_patch, dsp_buf, PATCH_BUF_SIZE, 0);
	ret = mt6630_DspPatch((const fm_u8 *)dsp_buf, patch_len, IMG_PATCH);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " DL DSPpatch failed\n");
		goto out;
	}

	ret = mt6630_get_coeff_path(mt6630_hw_info.rom_ver, &path_coeff, patch_tbl);
	if (ret == -FM_EPATCH) {
		FM_LOG_ERR(FM_ERR | CHIP, " mt6630_get_coeff_path failed\n");
		goto out;
	}
	patch_len = fm_file_read(path_coeff, dsp_buf, PATCH_BUF_SIZE, 0);

	mt6630_hw_info.rom_ver += 1;

	tmp_reg = dsp_buf[38] | (dsp_buf[39] << 8);	/* to be confirmed */
	mt6630_hw_info.patch_ver = (fm_s32) tmp_reg;
	FM_LOG_NTC(FM_NTC | CHIP, "Patch version: 0x%08x\n", mt6630_hw_info.patch_ver);

	if (ret == 1) {
		dsp_buf[4] = 0x00;	/* if we found rom version undefined, we should disable patch */
		dsp_buf[5] = 0x00;
	}

	ret = mt6630_DspPatch((const fm_u8 *)dsp_buf, patch_len, IMG_COEFFICIENT);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, " DL DSPcoeff failed\n");
		goto out;
	}
	mt6630_write(0x90, 0x0040);
	mt6630_write(0x90, 0x0000);
 out:
	if (dsp_buf) {
		fm_vfree(dsp_buf);
		dsp_buf = NULL;
	}
	return ret;
}

static fm_s32 mt6630_PowerUp(fm_u16 *chip_id, fm_u16 *device_id)
{
	fm_s32 ret = 0, reg = 0;
	fm_u16 pkt_size;
	fm_u16 tmp_reg = 0;

	FMR_ASSERT(chip_id);
	FMR_ASSERT(device_id);

	FM_LOG_DBG(FM_DBG | CHIP, "pwr on seq......\n");

	ret = mt6630_host_read(0x80021010, &reg);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, "packaging rd failed\n");
	} else {
		fm_packaging = (reg & 0x00008000) >> 15;
		FM_LOG_NTC(FM_NTC | CHIP, "fm_packaging: %d\n", fm_packaging);
	}
	ret = mt6630_pwrup_top_setting();
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, "mt6630_pwrup_top_setting failed\n");
		return ret;
	}
	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6630_pwrup_clock_on(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, "mt6630_pwrup_clock_on failed\n");
		return ret;
	}
	/* read HW version */
	mt6630_read(0x62, &tmp_reg);
	*chip_id = tmp_reg;
	*device_id = tmp_reg;
	mt6630_hw_info.chip_id = (fm_s32) tmp_reg;
	FM_LOG_NTC(FM_NTC | CHIP, "chip_id:0x%04x\n", tmp_reg);

	if (mt6630_hw_info.chip_id != 0x6630) {
		FM_LOG_NTC(FM_NTC | CHIP, "fm sys error!\n");
		return (-FM_EPARA);
	}
	ret = mt6630_pwrup_DSP_download(mt6630_patch_tbl);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, "mt6630_pwrup_DSP_download failed\n");
		return ret;
	}

	if ((mt6630_fm_config.aud_cfg.aud_path == FM_AUD_MRGIF)
	    || (mt6630_fm_config.aud_cfg.aud_path == FM_AUD_I2S)) {
		mt6630_I2s_Setting(FM_I2S_ON, mt6630_fm_config.aud_cfg.i2s_info.mode,
				   mt6630_fm_config.aud_cfg.i2s_info.rate);
		/* mt_combo_audio_ctrl(COMBO_AUDIO_STATE_2); */
		mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X) CMB_STUB_AIF_2);
	}
	/* Wholechip FM Power Up: step 4, FM Digital Init: fm_rgf_maincon */
	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6630_pwrup_digital_init(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, "mt6630_pwrup_digital_init failed\n");
		return ret;
	}

	FM_LOG_NTC(FM_NTC | CHIP, "pwr on seq ok\n");

	return ret;
}

static fm_s32 mt6630_PowerDown(void)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;
	fm_u16 dataRead;

	FM_LOG_DBG(FM_DBG | CHIP, "pwr down seq\n");
	/*SW work around for MCUFA issue.
	 *if interrupt happen before doing rampdown, DSP can't switch MCUFA back well.
	 * In case read interrupt, and clean if interrupt found before rampdown.
	 */
	mt6630_read(FM_MAIN_INTR, &dataRead);

	if (dataRead & 0x1) {
		mt6630_write(FM_MAIN_INTR, dataRead);	/* clear status flag */
	}

	//mt6630_RampDown();

	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6630_pwrdown(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, "mt6630_pwrdown failed\n");
		return ret;
	}

	ret = mt6630_pwrdown_top_setting();
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, "mt6630_pwrdown_top_setting failed\n");
		return ret;
	}

	return ret;
}

/* just for dgb */
static fm_bool mt6630_SetFreq(fm_u16 freq)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;
	fm_u16 chan_para = 0;

	fm_cb_op->cur_freq_set(freq);

#if 0
	/* MCU clock adjust if need */
	if ((ret = mt6630_mcu_dese(freq, NULL)) < 0) {
		WCN_DBG(FM_ERR | MAIN, "mt6630_mcu_dese FAIL:%d\n", ret);
	}

	WCN_DBG(FM_INF | MAIN, "MCU %d\n", ret);
#endif

	/* GPS clock adjust if need */
	if ((ret = mt6630_gps_dese(freq, NULL)) < 0) {
		WCN_DBG(FM_ERR | MAIN, "mt6630_gps_dese FAIL:%d\n", ret);
	}

	WCN_DBG(FM_INF | MAIN, "GPS %d\n", ret);

	ret = mt6630_write(0x60, 0x0007);
	if (ret) {
		WCN_DBG(FM_ALT | MAIN, "set freq write 0x60 fail\n");
	}
	if (mt6630_TDD_chan_check(freq)) {
		ret = mt6630_set_bits(0x30, 0x0004, 0xFFF9);	/* use TDD solution */
		if (ret) {
			WCN_DBG(FM_ALT | MAIN, "set freq write 0x30 fail\n");
		}
	}
	else {
		ret = mt6630_set_bits(0x30, 0x0000, 0xFFF9);	/* default use FDD solution */
		if (ret) {
			WCN_DBG(FM_ALT | MAIN, "set freq write 0x30 fail\n");
		}
	}
	ret = mt6630_write(0x60, 0x000F);
	if (ret) {
		WCN_DBG(FM_ALT | MAIN, "set freq write 0x60 fail\n");
	}
	
	chan_para = mt6630_chan_para_get(freq);
	FM_LOG_DBG(FM_DBG |CHIP, "%d chan para = %d\n", (fm_s32) freq, (fm_s32) chan_para);

	if (FM_LOCK(cmd_buf_lock))
		return fm_false;
	pkt_size = mt6630_tune(cmd_buf, TX_BUF_SIZE, freq, chan_para);
	ret =
	    fm_cmd_tx(cmd_buf, pkt_size, FLAG_TUNE | FLAG_TUNE_DONE, SW_RETRY_CNT, TUNE_TIMEOUT,
		      NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		FM_LOG_ERR(FM_ERR |CHIP, "mt6630_tune failed\n");
		return fm_false;
	}

	FM_LOG_DBG(FM_DBG |CHIP, "set freq to %d ok\n", freq);
	return fm_true;
}

#define FM_CQI_LOG_PATH "/mnt/sdcard/fmcqilog"

static fm_s32 mt6630_full_cqi_get(fm_s32 min_freq, fm_s32 max_freq, fm_s32 space, fm_s32 cnt)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;
	fm_u16 freq, orig_freq;
	fm_s32 i, j, k;
	fm_s32 space_val, max, min, num;
	struct mt6630_full_cqi *p_cqi;
	fm_u8 *cqi_log_title =
	    "Freq, RSSI, PAMD, PR, FPAMD, MR, ATDC, PRX, ATDEV, SMGain, DltaRSSI\n";
	fm_u8 cqi_log_buf[100] = { 0 };
	fm_s32 pos;
	fm_u8 cqi_log_path[100] = { 0 };

	FM_LOG_NTC(FM_NTC | CHIP, "6630 cqi log start\n");
	/* for soft-mute tune, and get cqi */
	freq = fm_cb_op->cur_freq_get();
	if (0 == fm_get_channel_space(freq)) {
		freq *= 10;
	}
	/* get cqi */
	orig_freq = freq;
	if (0 == fm_get_channel_space(min_freq)) {
		min = min_freq * 10;
	} else {
		min = min_freq;
	}
	if (0 == fm_get_channel_space(max_freq)) {
		max = max_freq * 10;
	} else {
		max = max_freq;
	}
	if (space == 0x0001) {
		space_val = 5;	/* 50Khz */
	} else if (space == 0x0002) {
		space_val = 10;	/* 100Khz */
	} else if (space == 0x0004) {
		space_val = 20;	/* 200Khz */
	} else {
		space_val = 10;
	}
	num = (max - min) / space_val + 1;	/* Eg, (8760 - 8750) / 10 + 1 = 2 */
	for (k = 0; (10000 == orig_freq) && (0xffffffff == g_dbg_level) && (k < cnt); k++) {
		FM_LOG_NTC(FM_NTC | CHIP, "cqi file:%d\n", k + 1);
		freq = min;
		pos = 0;
		fm_memcpy(cqi_log_path, FM_CQI_LOG_PATH, strlen(FM_CQI_LOG_PATH));
		sprintf(&cqi_log_path[strlen(FM_CQI_LOG_PATH)], "%d.txt", k + 1);
		fm_file_write(cqi_log_path, cqi_log_title, strlen(cqi_log_title), &pos);
		for (j = 0; j < num; j++) {
			if (FM_LOCK(cmd_buf_lock))
				return (-FM_ELOCK);
			pkt_size = mt6630_full_cqi_req(cmd_buf, TX_BUF_SIZE, &freq, 1, 1);
			ret =
			    fm_cmd_tx(cmd_buf, pkt_size, FLAG_SM_TUNE, SW_RETRY_CNT,
				      SM_TUNE_TIMEOUT, mt6630_get_read_result);
			FM_UNLOCK(cmd_buf_lock);

			if (!ret && mt6630_res) {
				FM_LOG_NTC(FM_NTC | CHIP, "smt cqi size %d\n", mt6630_res->cqi[0]);
				p_cqi = (struct mt6630_full_cqi *)&mt6630_res->cqi[2];
				for (i = 0; i < mt6630_res->cqi[1]; i++) {
					/* just for debug */
					FM_LOG_NTC(FM_NTC | CHIP,
						   "freq %d, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x\n",
						   p_cqi[i].ch, p_cqi[i].rssi, p_cqi[i].pamd,
						   p_cqi[i].pr, p_cqi[i].fpamd, p_cqi[i].mr,
						   p_cqi[i].atdc, p_cqi[i].prx, p_cqi[i].atdev,
						   p_cqi[i].smg, p_cqi[i].drssi);
					/* format to buffer */
					sprintf(cqi_log_buf,
						"%04d,%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,\n",
						p_cqi[i].ch, p_cqi[i].rssi, p_cqi[i].pamd,
						p_cqi[i].pr, p_cqi[i].fpamd, p_cqi[i].mr,
						p_cqi[i].atdc, p_cqi[i].prx, p_cqi[i].atdev,
						p_cqi[i].smg, p_cqi[i].drssi);
					/* write back to log file */
					fm_file_write(cqi_log_path, cqi_log_buf,
						      strlen(cqi_log_buf), &pos);
				}
			} else {
				FM_LOG_ERR(FM_ERR | CHIP, "smt get CQI failed\n");
				ret = -1;
			}
			freq += space_val;
		}
		fm_cb_op->cur_freq_set(0);	/* avoid run too much times */
	}
	FM_LOG_NTC(FM_NTC | CHIP, "6630 cqi log done\n");

	return ret;
}

/*
 * mt6630_GetCurRSSI - get current freq's RSSI value
 * RS=RSSI
 * If RS>511, then RSSI(dBm)= (RS-1024)/16*6
 *				   else RSSI(dBm)= RS/16*6
 */
static fm_s32 mt6630_GetCurRSSI(fm_s32 *pRSSI)
{
	fm_u16 tmp_reg;

	/* TODO: check reg */
	mt6630_read(FM_RSSI_IND, &tmp_reg);
	tmp_reg = tmp_reg & 0x03ff;

	if (pRSSI) {
		*pRSSI = (tmp_reg > 511) ? (((tmp_reg - 1024) * 6) >> 4) : ((tmp_reg * 6) >> 4);
		FM_LOG_DBG(FM_DBG | CHIP, "rssi:%d, dBm:%d\n", tmp_reg, *pRSSI);
	} else {
		WCN_DBG(FM_ERR | CHIP, "get rssi para error\n");
		return -FM_EPARA;
	}

	return 0;
}

static fm_u16 mt6630_vol_tbl[16] = {
	0x0000, 0x0519, 0x066A, 0x0814,
	0x0A2B, 0x0CCD, 0x101D, 0x1449,
	0x198A, 0x2027, 0x287A, 0x32F5,
	0x4027, 0x50C3, 0x65AD, 0x7FFF
};

static fm_s32 mt6630_SetVol(fm_u8 vol)
{
	fm_s32 ret = 0;

	/* TODO: check reg */
	vol = (vol > 15) ? 15 : vol;
	ret = mt6630_write(0x7D, mt6630_vol_tbl[vol]);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "Set vol=%d Failed\n", vol);
		return ret;
	} else {
		FM_LOG_DBG(FM_DBG | CHIP, "Set vol=%d OK\n", vol);
	}

	if (vol == 10) {
		fm_print_cmd_fifo();	/* just for debug */
		fm_print_evt_fifo();
	}
	return 0;
}

static fm_s32 mt6630_GetVol(fm_u8 *pVol)
{
	int ret = 0;
	fm_u16 tmp;
	fm_s32 i;

	FMR_ASSERT(pVol);

	/* TODO: check reg */
	ret = mt6630_read(0x7D, &tmp);
	if (ret) {
		*pVol = 0;
		WCN_DBG(FM_ERR | CHIP, "Get vol Failed\n");
		return ret;
	}

	for (i = 0; i < 16; i++) {
		if (mt6630_vol_tbl[i] == tmp) {
			*pVol = i;
			break;
		}
	}

	FM_LOG_DBG(FM_DBG | CHIP, "Get vol=%d OK\n", *pVol);
	return 0;
}

static fm_s32 mt6630_dump_reg(void)
{
	fm_s32 i;
	fm_u16 TmpReg;
	for (i = 0; i < 0xff; i++) {
		mt6630_read(i, &TmpReg);
		FM_LOG_NTC(FM_NTC | CHIP, "0x%02x=0x%04x\n", i, TmpReg);
	}
	return 0;
}

static fm_bool mt6630_GetMonoStereo(fm_u16 *pMonoStereo)
{
#define FM_BF_STEREO 0x1000
	fm_u16 TmpReg;

	/* TODO: check reg */
	if (pMonoStereo) {
		mt6630_read(FM_RSSI_IND, &TmpReg);
		*pMonoStereo = (TmpReg & FM_BF_STEREO) >> 12;
	} else {
		WCN_DBG(FM_ERR | CHIP, "MonoStero: para err\n");
		return fm_false;
	}

	FM_LOG_DBG(FM_DBG | CHIP, "MonoStero:0x%04x\n", *pMonoStereo);
	return fm_true;
}

static fm_s32 mt6630_SetMonoStereo(fm_s32 MonoStereo)
{
	fm_s32 ret = 0;
#define FM_FORCE_MS 0x0008

	FM_LOG_DBG(FM_DBG | CHIP, "set to %s\n", MonoStereo ? "mono" : "auto");
	/* TODO: check reg */

	mt6630_write(0x60, 0x3007);

	if (MonoStereo) {
		ret = mt6630_set_bits(0x75, FM_FORCE_MS, ~FM_FORCE_MS);
	} else {
		ret = mt6630_set_bits(0x75, 0x0000, ~FM_FORCE_MS);
	}

	return ret;
}

static fm_s32 mt6630_GetCapArray(fm_s32 *ca)
{
	fm_u16 dataRead;
	fm_u16 tmp = 0;

	/* TODO: check reg */
	FMR_ASSERT(ca);
	mt6630_read(0x60, &tmp);
	mt6630_write(0x60, tmp & 0xFFF7);	/* 0x60 D3=0 */

	mt6630_read(0x26, &dataRead);
	*ca = dataRead;

	mt6630_write(0x60, tmp);	/* 0x60 D3=1 */
	return 0;
}

/*
 * mt6630_GetCurPamd - get current freq's PAMD value
 * PA=PAMD
 * If PA>511 then PAMD(dB)=  (PA-1024)/16*6,
 *				else PAMD(dB)=PA/16*6
 */
static fm_bool mt6630_GetCurPamd(fm_u16 *pPamdLevl)
{
	fm_u16 tmp_reg;
	fm_u16 dBvalue, valid_cnt = 0;
	int i, total = 0;
	for (i = 0; i < 8; i++) {
		/* TODO: check reg */
		if (mt6630_read(FM_ADDR_PAMD, &tmp_reg)) {
			*pPamdLevl = 0;
			return fm_false;
		}

		tmp_reg &= 0x03FF;
		dBvalue = (tmp_reg > 256) ? ((512 - tmp_reg) * 6 / 16) : 0;
		if (dBvalue != 0) {
			total += dBvalue;
			valid_cnt++;
			FM_LOG_DBG(FM_DBG | CHIP, "[%d]PAMD=%d\n", i, dBvalue);
		}
		Delayms(3);
	}
	if (valid_cnt != 0) {
		*pPamdLevl = total / valid_cnt;
	} else {
		*pPamdLevl = 0;
	}
	FM_LOG_NTC(FM_NTC | CHIP, "PAMD=%d\n", *pPamdLevl);
	return fm_true;
}

static fm_s32 MT6630_FMOverBT(fm_bool enable)
{
	fm_s32 ret = 0;

	WCN_DBG(FM_NTC | CHIP, "+%s():\n", __func__);

	if (enable == fm_true) {
		/* change I2S to slave mode and 48K sample rate */
		if (mt6630_I2s_Setting(FM_I2S_ON, FM_I2S_SLAVE, FM_I2S_48K))
			goto out;
		WCN_DBG(FM_NTC | CHIP, "set FM via BT controller\n");
	} else if (enable == fm_false) {
		/* change I2S to master mode and 44.1K sample rate */
		if (mt6630_I2s_Setting(FM_I2S_ON, FM_I2S_MASTER, FM_I2S_44K))
			goto out;
		WCN_DBG(FM_NTC | CHIP, "set FM via Host\n");
	} else {
		WCN_DBG(FM_ERR | CHIP, "%s()\n", __func__);
		ret = -FM_EPARA;
		goto out;
	}
 out:
	WCN_DBG(FM_NTC | CHIP, "-%s():[ret=%d]\n", __func__, ret);
	return ret;
}


/*
 * mt6630_I2s_Setting - set the I2S state on MT6630
 * @onoff - I2S on/off
 * @mode - I2S mode: Master or Slave
 *
 * Return:0, if success; error code, if failed
 */
static fm_s32 mt6630_I2s_Setting(fm_s32 onoff, fm_s32 mode, fm_s32 sample)
{
	fm_u16 tmp_state = 0;
	fm_u16 tmp_mode = 0;
	fm_u16 tmp_sample = 0;
	fm_s32 ret = 0;

	if (onoff == FM_I2S_ON) {
		tmp_state = 0x0003;	/* I2S enable and standard I2S mode, 0x9B D0,D1=1 */
		mt6630_fm_config.aud_cfg.i2s_info.status = FM_I2S_ON;
	} else if (onoff == FM_I2S_OFF) {
		tmp_state = 0x0000;	/* I2S  off, 0x9B D0,D1=0 */
		mt6630_fm_config.aud_cfg.i2s_info.status = FM_I2S_OFF;
	} else {
		WCN_DBG(FM_ERR | CHIP, "%s():[onoff=%d]\n", __func__, onoff);
		ret = -FM_EPARA;
		goto out;
	}

	if (mode == FM_I2S_MASTER) {
		tmp_mode = 0x0000;	/* 6630 as I2S master, set 0x9B D3=0 */
		mt6630_fm_config.aud_cfg.i2s_info.mode = FM_I2S_MASTER;
	} else if (mode == FM_I2S_SLAVE) {
		tmp_mode = 0x0008;	/* 6630 as I2S slave, set 0x9B D3=1 */
		mt6630_fm_config.aud_cfg.i2s_info.mode = FM_I2S_SLAVE;
	} else {
		WCN_DBG(FM_ERR | CHIP, "%s():[mode=%d]\n", __func__, mode);
		ret = -FM_EPARA;
		goto out;
	}

	if (sample == FM_I2S_32K) {
		tmp_sample = 0x0000;	/* 6630 I2S 32KHz sample rate, 0x5F D11~12 */
		mt6630_fm_config.aud_cfg.i2s_info.rate = FM_I2S_32K;
	} else if (sample == FM_I2S_44K) {
		tmp_sample = 0x0800;	/* 6630 I2S 44.1KHz sample rate */
		mt6630_fm_config.aud_cfg.i2s_info.rate = FM_I2S_44K;
	} else if (sample == FM_I2S_48K) {
		tmp_sample = 0x1000;	/* 6630 I2S 48KHz sample rate */
		mt6630_fm_config.aud_cfg.i2s_info.rate = FM_I2S_48K;
	} else {
		WCN_DBG(FM_ERR | CHIP, "%s():[sample=%d]\n", __func__, sample);
		ret = -FM_EPARA;
		goto out;
	}

	ret = mt6630_write(0x60, 0x7);
    if (ret)
    {
        goto out;
    }

	ret = mt6630_set_bits(0x5F, tmp_sample, 0xE7FF);
	if (ret) {
		goto out;
	}
	ret = mt6630_set_bits(0x9B, tmp_mode, 0xFFF7);
	if (ret) {
		goto out;
	}
	ret = mt6630_set_bits(0x9B, tmp_state, 0xFFFC);
	if (ret) {
		goto out;
	}
	/* F0.4    enable ft */
	ret = mt6630_set_bits(0x56, 0x1, 0xFFFE);
	if (ret) {
		goto out;
	}

	ret = mt6630_write(0x60, 0xf);
    if (ret)
    {
        goto out;
    }

	FM_LOG_NTC(FM_NTC | CHIP, "[onoff=%s][mode=%s][sample=%d](0)33KHz,(1)44.1KHz,(2)48KHz\n",
		   (onoff == FM_I2S_ON) ? "On" : "Off",
		   (mode == FM_I2S_MASTER) ? "Master" : "Slave", sample);
 out:
	return ret;
}

static fm_s32 mt6630fm_get_audio_info(fm_audio_info_t *data)
{
	memcpy(data, &mt6630_fm_config.aud_cfg, sizeof(fm_audio_info_t));
	return 0;
}

static fm_s32 mt6630_i2s_info_get(fm_s32 *ponoff, fm_s32 *pmode, fm_s32 *psample)
{
	*ponoff = mt6630_fm_config.aud_cfg.i2s_info.status;
	*pmode = mt6630_fm_config.aud_cfg.i2s_info.mode;
	*psample = mt6630_fm_config.aud_cfg.i2s_info.rate;

	return 0;
}

static fm_s32 mt6630_hw_info_get(struct fm_hw_info *req)
{
	FMR_ASSERT(req);

	req->chip_id = mt6630_hw_info.chip_id;
	req->eco_ver = mt6630_hw_info.eco_ver;
	req->patch_ver = mt6630_hw_info.patch_ver;
	req->rom_ver = mt6630_hw_info.rom_ver;

	return 0;
}

static fm_s32 mt6630_pre_search(void)
{
	mt6630_RampDown();
	return 0;
}

static fm_s32 mt6630_restore_search(void)
{
	mt6630_RampDown();
	return 0;
}

/*
freq: 8750~10800
valid: fm_true-valid channel,fm_false-invalid channel
return: fm_true- smt success, fm_false-smt fail
*/
static fm_s32 mt6630_soft_mute_tune(fm_u16 freq, fm_s32 *rssi, fm_bool *valid)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;
	/* fm_u16 freq;//, orig_freq; */
	struct mt6630_full_cqi *p_cqi;
	fm_s32 RSSI = 0, PAMD = 0, MR = 0, ATDC = 0;
	fm_u32 PRX = 0, ATDEV = 0;
	fm_u16 softmuteGainLvl = 0;

	ret = mt6630_chan_para_get(freq);
	if (ret == 2) {
		ret = mt6630_set_bits(FM_CHANNEL_SET, 0x2000, 0x0FFF);	/* mdf HiLo */
	} else {
		ret = mt6630_set_bits(FM_CHANNEL_SET, 0x0000, 0x0FFF);	/* clear FA/HL/ATJ */
	}
#if 0
	mt6630_write(0x60, 0x0007);
	if (mt6630_TDD_chan_check(freq))
		mt6630_set_bits(0x30, 0x0004, 0xFFF9);	/* use TDD solution */
	else
		mt6630_set_bits(0x30, 0x0000, 0xFFF9);	/* default use FDD solution */
	mt6630_write(0x60, 0x000F);
#endif
	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6630_full_cqi_req(cmd_buf, TX_BUF_SIZE, &freq, 1, 1);
	ret =
	    fm_cmd_tx(cmd_buf, pkt_size, FLAG_SM_TUNE, SW_RETRY_CNT, SM_TUNE_TIMEOUT,
		      mt6630_get_read_result);
	FM_UNLOCK(cmd_buf_lock);

	if (!ret && mt6630_res) {
		FM_LOG_NTC(FM_NTC | CHIP, "smt cqi size %d\n", mt6630_res->cqi[0]);
		p_cqi = (struct mt6630_full_cqi *)&mt6630_res->cqi[2];
		/* just for debug */
		FM_LOG_NTC(FM_NTC | CHIP,
			   "freq %d, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x\n",
			   p_cqi->ch, p_cqi->rssi, p_cqi->pamd, p_cqi->pr, p_cqi->fpamd, p_cqi->mr,
			   p_cqi->atdc, p_cqi->prx, p_cqi->atdev, p_cqi->smg, p_cqi->drssi);
		RSSI =
		    ((p_cqi->rssi & 0x03FF) >=
		     512) ? ((p_cqi->rssi & 0x03FF) - 1024) : (p_cqi->rssi & 0x03FF);
		PAMD =
		    ((p_cqi->pamd & 0x1FF) >=
		     256) ? ((p_cqi->pamd & 0x01FF) - 512) : (p_cqi->pamd & 0x01FF);
		MR = ((p_cqi->mr & 0x01FF) >=
		      256) ? ((p_cqi->mr & 0x01FF) - 512) : (p_cqi->mr & 0x01FF);
		ATDC = (p_cqi->atdc >= 32768) ? (65536 - p_cqi->atdc) : (p_cqi->atdc);
		if (ATDC < 0) {
			ATDC = (~(ATDC)) - 1;	/* Get abs value of ATDC */
		}
		PRX = (p_cqi->prx & 0x00FF);
		ATDEV = p_cqi->atdev;
		softmuteGainLvl = p_cqi->smg;
		/* check if the channel is valid according to each CQIs */
		if ((RSSI >= mt6630_fm_config.rx_cfg.long_ana_rssi_th)
		    && (PAMD <= mt6630_fm_config.rx_cfg.pamd_th)
		    && (ATDC <= mt6630_fm_config.rx_cfg.atdc_th)
		    && (MR >= mt6630_fm_config.rx_cfg.mr_th)
		    && (PRX >= mt6630_fm_config.rx_cfg.prx_th)
		    && (ATDEV >= ATDC)	/* sync scan algorithm */
		    &&(softmuteGainLvl >= mt6630_fm_config.rx_cfg.smg_th)) {
			*valid = fm_true;
		} else {
			*valid = fm_false;
		}
		*rssi = RSSI;
/*		if(RSSI < -296)
			FM_LOG_NTC(FM_NTC | CHIP, "rssi\n");
		else if(PAMD > -12)
			FM_LOG_NTC(FM_NTC | CHIP, "PAMD\n");
		else if(ATDC > 3496)
			FM_LOG_NTC(FM_NTC | CHIP, "ATDC\n");
		else if(MR < -67)
			FM_LOG_NTC(FM_NTC | CHIP, "MR\n");
		else if(PRX < 80)
			FM_LOG_NTC(FM_NTC | CHIP, "PRX\n");
		else if(ATDEV < ATDC)
			FM_LOG_NTC(FM_NTC | CHIP, "ATDEV\n");
		else if(softmuteGainLvl < 16421)
			FM_LOG_NTC(FM_NTC | CHIP, "softmuteGainLvl\n");
			*/
	} else {
		FM_LOG_ERR(FM_ERR | CHIP, "smt get CQI failed\n");
		return fm_false;
	}
	FM_LOG_NTC(FM_NTC | CHIP, "valid=%d\n", *valid);
	return fm_true;
}

/*
parm:
	parm.th_type: 0, RSSI. 1,desense RSSI. 2,SMG.
    parm.th_val: threshold value
*/
static fm_s32 mt6630_set_search_th(fm_s32 idx, fm_s32 val, fm_s32 reserve)
{
	switch (idx) {
	case 0:
		{
			mt6630_fm_config.rx_cfg.long_ana_rssi_th = val;
			WCN_DBG(FM_NTC | CHIP, "set rssi th =%d\n", val);
			break;
		}
	case 1:
		{
			mt6630_fm_config.rx_cfg.desene_rssi_th = val;
			WCN_DBG(FM_NTC | CHIP, "set desense rssi th =%d\n", val);
			break;
		}
	case 2:
		{
			mt6630_fm_config.rx_cfg.smg_th = val;
			WCN_DBG(FM_NTC | CHIP, "set smg th =%d\n", val);
			break;
		}
	default:
		break;
	}
	return 0;
}

#if 0
static const fm_u16 mt6630_mcu_dese_list[] = {
	0			/* 7630, 7800, 7940, 8320, 9260, 9600, 9710, 9920, 10400, 10410 */
};

static const fm_u16 mt6630_gps_dese_list[] = {
	0			/* 7850, 7860 */
};
#endif

static const fm_s8 mt6630_chan_para_map[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 6500~6595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 6600~6695 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0,	/* 6700~6795 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 6800~6895 */
	0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 6900~6995 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7000~7095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7100~7195 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7200~7295 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7300~7395 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7400~7495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7500~7595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,	/* 7600~7695 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7700~7795 */
	8, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7800~7895 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7900~7995 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8000~8095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8100~8195 */
	0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8200~8295 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0,	/* 8300~8395 */
	0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8400~8495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8500~8595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8600~8695 */
	0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8700~8795 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8800~8895 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8900~8995 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9000~9095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9100~9195 */
	0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9200~9295 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9300~9395 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9400~9495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9500~9595 */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9600~9695 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9700~9795 */
	0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9800~9895 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,	/* 9900~9995 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10000~10095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10100~10195 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0,	/* 10200~10295 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0,	/* 10300~10395 */
	8, 0, 2, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10400~10495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0,	/* 10500~10595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10600~10695 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0,	/* 10700~10795 */
	0			/* 10800 */
};

static const fm_u16 mt6630_TDD_list[] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 6500~6595 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 6600~6695 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 6700~6795 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 6800~6895 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 6900~6995 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7000~7095 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7100~7195 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7200~7295 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7300~7395 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7400~7495 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7500~7595 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7600~7695 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7700~7795 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7800~7895 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7900~7995 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8000~8095 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8100~8195 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8200~8295 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8300~8395 */
	0x0101, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8400~8495 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8500~8595 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8600~8695 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8700~8795 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8800~8895 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8900~8995 */
	0x0000, 0x0000, 0x0101, 0x0101, 0x0101,	/* 9000~9095 */
	0x0101, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9100~9195 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9200~9295 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9300~9395 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9400~9495 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9500~9595 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9600~9695 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0100,	/* 9700~9795 */
	0x0101, 0x0101, 0x0101, 0x0101, 0x0101,	/* 9800~9895 */
	0x0101, 0x0101, 0x0001, 0x0000, 0x0000,	/* 9900~9995 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 10000~10095 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 10100~10195 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 10200~10295 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 10300~10395 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 10400~10495 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 10500~10595 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0100,	/* 10600~10695 */
	0x0101, 0x0101, 0x0101, 0x0101, 0x0101,	/* 10700~10795 */
	0x0001			/* 10800 */
};

static const fm_u16 mt6630_TDD_Mask[] = {
	0x0001, 0x0010, 0x0100, 0x1000
};

static const fm_u16 mt6630_scan_dese_list[] = {
	7800, 9210, 9220, 9600, 9980, 10400, 10750, 10760
};

/* return value: 0, not a de-sense channel; 1, this is a de-sense channel; else error no */
static fm_s32 mt6630_is_dese_chan(fm_u16 freq)
{
	fm_s32 size;

	/* return 0;//HQA only :skip desense channel check. */
	size = sizeof(mt6630_scan_dese_list) / sizeof(mt6630_scan_dese_list[0]);

	if (0 == fm_get_channel_space(freq)) {
		freq *= 10;
	}

	while (size) {
		if (mt6630_scan_dese_list[size - 1] == freq)
			return 1;

		size--;
	}

	return 0;
}

static fm_bool mt6630_TDD_chan_check(fm_u16 freq)
{
	fm_u32 i = 0;
	fm_u16 freq_tmp = freq;
	fm_s32 ret = 0;

	ret = fm_get_channel_space(freq_tmp);
	if (0 == ret) {
		freq_tmp *= 10;
	} else if (-1 == ret)
		return fm_false;

	i = (freq_tmp - 6500) / 5;

	/* WCN_DBG(FM_NTC | CHIP, "Freq %d is 0x%4x, mask is 0x%4x\n", freq,(mt6630_TDD_list[i/4]),mt6630_TDD_Mask[i%4]); */
	if (mt6630_TDD_list[i / 4] & mt6630_TDD_Mask[i % 4]) {
		WCN_DBG(FM_DBG | CHIP, "Freq %d use TDD solution\n", freq);
		return fm_true;
	} else
		return fm_false;
}


/*  return value:
1, is desense channel and rssi is less than threshold;
0, not desense channel or it is but rssi is more than threshold.*/
static fm_s32 mt6630_desense_check(fm_u16 freq, fm_s32 rssi)
{
	if (mt6630_is_dese_chan(freq)) {
		if (rssi < mt6630_fm_config.rx_cfg.desene_rssi_th) {
			return 1;
		}
		FM_LOG_DBG(FM_DBG | CHIP, "desen_rssi %d th:%d\n", rssi,
			   mt6630_fm_config.rx_cfg.desene_rssi_th);
	}
	return 0;
}

/* get channel parameter, HL side/ FA / ATJ */
static fm_u16 mt6630_chan_para_get(fm_u16 freq)
{
	fm_s32 pos, size;

	/* return 0;//for HQA only: skip FA/HL/ATJ */
	if (0 == fm_get_channel_space(freq)) {
		freq *= 10;
	}
	if (freq < 6500) {
		return 0;
	}
	pos = (freq - 6500) / 5;

	size = sizeof(mt6630_chan_para_map) / sizeof(mt6630_chan_para_map[0]);

	pos = (pos < 0) ? 0 : pos;
	pos = (pos > (size - 1)) ? (size - 1) : pos;

	return mt6630_chan_para_map[pos];
}

static fm_s32 mt6630_gps_dese(fm_u16 freq, void *arg)
{
	fm_gps_desense_t state = FM_GPS_DESE_DISABLE;

	if (0 == fm_get_channel_space(freq)) {
		freq *= 10;
	}

	WCN_DBG(FM_DBG | CHIP, "%s, [freq=%d]\n", __func__, (int)freq);

	if (state != FM_GPS_DESE_ENABLE) {
		if ((freq >= 7800) && (freq <= 8000)) {
			state = FM_GPS_DESE_ENABLE;
		}
	}
	/* request 6630 GPS change clk */
	if (state == FM_GPS_DESE_DISABLE) {
		if (!mtk_wcn_wmt_dsns_ctrl(WMTDSNS_FM_GPS_DISABLE)) {
			return -1;
		}
		return 0;
	} else {
		if (!mtk_wcn_wmt_dsns_ctrl(WMTDSNS_FM_GPS_ENABLE)) {
			return -1;
		}
		return 1;
	}
}

/******************************Tx function********************************************/
static fm_s32 mt6630_Tx_Support(fm_s32 *sup)
{
	*sup = 1;
	return 0;
}

static fm_s32 mt6630_rdsTx_Support(fm_s32 *sup)
{
	*sup = 1;
	return 0;
}

static fm_s32 MT6630_Rds_Tx_Enable(void)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock)) {
		return (-FM_ELOCK);
	}
	pkt_size = mt6630_tx_rdson_deviation(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_RDS_TX, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6630_tx_rdson_deviation failed\n");
		return ret;
	}
	mt6630_set_bits(0xC7, 0x0800, 0xF7FF);
	return 0;
}

static fm_s32 MT6630_Rds_Tx_Disable(void)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock)) {
		return (-FM_ELOCK);
	}
	pkt_size = mt6630_pwrup_tx_deviation(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6630_pwrup_tx_deviation failed\n");
		return ret;
	}

	mt6630_set_bits(0xC7, 0x0000, 0xF7FF);

	return 0;
}

/*
pi: pi code,
ps: block B,C,D
other_rds: NULL now
other_rds_cnt:0 now
*/
static fm_s32 MT6630_Rds_Tx(fm_u16 pi, fm_u16 *ps, fm_u16 *other_rds, fm_u8 other_rds_cnt)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size = 0;

	WCN_DBG(FM_NTC | RDSC,
		"+%s():PI=0x%04x, PS=0x%04x/0x%04x/0x%04x/0x%04x, other_rds_cnt=%d\n", __func__,
		pi, ps[0], ps[1], ps[2], ps[3], other_rds_cnt);
	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6630_rds_tx(cmd_buf, TX_BUF_SIZE, pi, ps, other_rds, other_rds_cnt);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_RDS_TX, SW_RETRY_CNT, RDS_TX_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

/*
freq: 8750~10800
valid: fm_true-valid channel,fm_false-invalid channel
return: fm_true- smt success, fm_false-smt fail
*/
static fm_s32 mt6630_soft_mute_tune_Tx(fm_u16 freq, fm_s32 *rssi, fm_bool *valid)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;
	struct mt6630_full_cqi *p_cqi;
	fm_s32 RSSI = 0, PAMD = 0, MR = 0, ATDC = 0;
	fm_u32 PRX = 0, ATDEV = 0;
	fm_u16 softmuteGainLvl = 0;

	ret = mt6630_chan_para_get(freq);
	if (ret == 2) {
		ret = mt6630_set_bits(FM_CHANNEL_SET, 0x2000, 0x0FFF);	/* mdf HiLo */
	} else {
		ret = mt6630_set_bits(FM_CHANNEL_SET, 0x0000, 0x0FFF);	/* clear FA/HL/ATJ */
	}

	mt6630_write(0x60, 0x0007);
	if (mt6630_TDD_chan_check(freq))
		mt6630_set_bits(0x30, 0x0004, 0xFFF9);	/* use TDD solution */
	else
		mt6630_set_bits(0x30, 0x0000, 0xFFF9);	/* default use FDD solution */
	mt6630_write(0x60, 0x000F);

	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6630_full_cqi_req(cmd_buf, TX_BUF_SIZE, &freq, 1, 1);
	ret =
	    fm_cmd_tx(cmd_buf, pkt_size, FLAG_SM_TUNE, SW_RETRY_CNT, SM_TUNE_TIMEOUT,
		      mt6630_get_read_result);
	FM_UNLOCK(cmd_buf_lock);

	if (!ret && mt6630_res) {
		FM_LOG_NTC(FM_NTC | CHIP, "smt cqi size %d\n", mt6630_res->cqi[0]);
		p_cqi = (struct mt6630_full_cqi *)&mt6630_res->cqi[2];
		/* just for debug */
		FM_LOG_NTC(FM_NTC | CHIP,
			   "freq %d, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x\n",
			   p_cqi->ch, p_cqi->rssi, p_cqi->pamd, p_cqi->pr, p_cqi->fpamd, p_cqi->mr,
			   p_cqi->atdc, p_cqi->prx, p_cqi->atdev, p_cqi->smg, p_cqi->drssi);
		RSSI =
		    ((p_cqi->rssi & 0x03FF) >=
		     512) ? ((p_cqi->rssi & 0x03FF) - 1024) : (p_cqi->rssi & 0x03FF);
		PAMD =
		    ((p_cqi->pamd & 0x1FF) >=
		     256) ? ((p_cqi->pamd & 0x01FF) - 512) : (p_cqi->pamd & 0x01FF);
		MR = ((p_cqi->mr & 0x01FF) >=
		      256) ? ((p_cqi->mr & 0x01FF) - 512) : (p_cqi->mr & 0x01FF);
		ATDC = (p_cqi->atdc >= 32768) ? (65536 - p_cqi->atdc) : (p_cqi->atdc);
		if (ATDC < 0) {
			ATDC = (~(ATDC)) - 1;	/* Get abs value of ATDC */
		}
		PRX = (p_cqi->prx & 0x00FF);
		ATDEV = p_cqi->atdev;
		softmuteGainLvl = p_cqi->smg;
		/* check if the channel is valid according to each CQIs */
		if ((PAMD > mt6630_fm_config.tx_cfg.pamd_th)
		    && (MR <= mt6630_fm_config.tx_cfg.mr_th)
		    && (softmuteGainLvl < mt6630_fm_config.tx_cfg.smg_th)) {
			*valid = fm_true;
		} else {
			*valid = fm_false;
		}
		*rssi = RSSI;
	} else {
		FM_LOG_ERR(FM_ERR | CHIP, "smt get CQI failed\n");
		return fm_false;
	}
	FM_LOG_NTC(FM_NTC | CHIP, "valid=%d\n", *valid);
	return fm_true;
}

#define	TX_ABANDON_BAND_LOW1	7320
#define	TX_ABANDON_BAND_HIGH1	7450
#define	TX_ABANDON_BAND_LOW2	9760
#define	TX_ABANDON_BAND_HIGH2	9940
static fm_s32 mt6630_TxScan(fm_u16 min_freq,
			    fm_u16 max_freq,
			    fm_u16 *pFreq,
			    fm_u16 *pScanTBL, fm_u16 *ScanTBLsize, fm_u16 scandir, fm_u16 space)
{
	fm_s32 i = 0, ret = 0;
	fm_u16 freq = *pFreq;
	fm_u16 scan_cnt = *ScanTBLsize;
	fm_u16 cnt = 0;
	fm_s32 rssi = 0;
	fm_s32 step;
	fm_bool valid = fm_false;
	fm_s32 total_no = 0;

	WCN_DBG(FM_NTC | CHIP, "+%s():\n", __func__);

	if ((!pScanTBL) || (*ScanTBLsize < FM_TX_SCAN_MIN) || (*ScanTBLsize > FM_TX_SCAN_MAX)) {
		WCN_DBG(FM_ERR | CHIP, "invalid scan table\n");
		ret = -FM_EPARA;
		return 1;
	}
	if (0 == fm_get_channel_space(freq)) {
		*pFreq *= 10;
	}
	if (0 == fm_get_channel_space(max_freq)) {
		max_freq *= 10;
	}
	if (0 == fm_get_channel_space(min_freq)) {
		min_freq *= 10;
	}

	WCN_DBG(FM_NTC | CHIP,
		"[freq=%d], [max_freq=%d],[min_freq=%d],[scan BTL size=%d],[scandir=%d],[space=%d]\n",
		*pFreq, max_freq, min_freq, *ScanTBLsize, scandir, space);

	cnt = 0;
	if (space == FM_SPACE_200K) {
		step = 20;
	} else if (space == FM_SPACE_50K) {
		step = 5;
	} else {
		step = 10;
	}
	total_no = (max_freq - min_freq) / step + 1;
	if (scandir == FM_TX_SCAN_UP) {
		for (i = ((*pFreq - min_freq) / step); i < total_no; i++) {
			freq = min_freq + step * i;

			//FM desense GPS 
			if((freq>=TX_ABANDON_BAND_LOW1) && (freq<=TX_ABANDON_BAND_HIGH1))
			{
				freq = TX_ABANDON_BAND_HIGH1 + 10;
				i = (freq-min_freq)/step;
			}
				
			if((freq>=TX_ABANDON_BAND_LOW2) && (freq<=TX_ABANDON_BAND_HIGH2))
			{
				freq = TX_ABANDON_BAND_HIGH2 + 10;				
				i = (freq-min_freq)/step;
			}

			ret = mt6630_soft_mute_tune_Tx(freq, &rssi, &valid);
			if (ret == fm_false) {
				WCN_DBG(FM_ERR | CHIP, "mt6630_soft_mute_tune failed\n");
				return 1;
			}

			if (valid == fm_true) {
				*(pScanTBL + cnt) = freq;	/* strore the valid empty channel */
				cnt++;
				WCN_DBG(FM_NTC | CHIP, "empty channel:[freq=%d] [cnt=%d]\n", freq,
					cnt);
			}
			if (cnt >= scan_cnt) {
				break;
			}
		}

		if (cnt < scan_cnt) {
			for (i = 0; i < ((*pFreq - min_freq) / step); i++) {
				freq = min_freq + step * i;

				//FM desense GPS 
				if((freq>=TX_ABANDON_BAND_LOW1) && (freq<=TX_ABANDON_BAND_HIGH1))
				{
					freq = TX_ABANDON_BAND_HIGH1 + 10;
					i = (freq-min_freq)/step;
				}
					
				if((freq>=TX_ABANDON_BAND_LOW2) && (freq<=TX_ABANDON_BAND_HIGH2))
				{
					freq = TX_ABANDON_BAND_HIGH2 + 10;				
					i = (freq-min_freq)/step;
				}

				if(i >= ((*pFreq-min_freq)/step))
					break;

				ret = mt6630_soft_mute_tune_Tx(freq, &rssi, &valid);
				if (ret == fm_false) {
					WCN_DBG(FM_ERR | CHIP, "mt6630_soft_mute_tune failed\n");
					return 1;
				}

				if (valid == fm_true) {
					*(pScanTBL + cnt) = freq;	/* strore the valid empty channel */
					cnt++;
					WCN_DBG(FM_NTC | CHIP, "empty channel:[freq=%d] [cnt=%d]\n",
						freq, cnt);
				}
				if (cnt >= scan_cnt) {
					break;
				}
			}
		}
	} else {
		for (i = ((*pFreq - min_freq) / step - 1); i >= 0; i--) {
			freq = min_freq + step * i;

			//FM desense GPS 
			if((freq>=TX_ABANDON_BAND_LOW1) && (freq<=TX_ABANDON_BAND_HIGH1))
			{
				freq = TX_ABANDON_BAND_LOW1 - 10;
				i = (freq-min_freq)/step;
			}
				
			if((freq>=TX_ABANDON_BAND_LOW2) && (freq<=TX_ABANDON_BAND_HIGH2))
			{
				freq = TX_ABANDON_BAND_LOW2 - 10;				
				i = (freq-min_freq)/step;
			}

			ret = mt6630_soft_mute_tune_Tx(freq, &rssi, &valid);
			if (ret == fm_false) {
				WCN_DBG(FM_ERR | CHIP, "mt6630_soft_mute_tune failed\n");
				return 1;
			}

			if (valid == fm_true) {
				*(pScanTBL + cnt) = freq;	/* strore the valid empty channel */
				cnt++;
				WCN_DBG(FM_NTC | CHIP, "empty channel:[freq=%d] [cnt=%d]\n", freq,
					cnt);
			}
			if (cnt >= scan_cnt) {
				break;
			}
		}
		if (cnt < scan_cnt) {
			for (i = (total_no - 1); i > ((*pFreq - min_freq) / step); i--) {
				freq = min_freq + step * i;

				//FM desense GPS 
				if((freq>=TX_ABANDON_BAND_LOW1) && (freq<=TX_ABANDON_BAND_HIGH1))
				{
					freq = TX_ABANDON_BAND_LOW1 - 10;
					i = (freq-min_freq)/step;
				}
					
				if((freq>=TX_ABANDON_BAND_LOW2) && (freq<=TX_ABANDON_BAND_HIGH2))
				{
					freq = TX_ABANDON_BAND_LOW2 - 10;				
					i = (freq-min_freq)/step;
				}

				if(i <= ((*pFreq-min_freq)/step))
					break;

				ret = mt6630_soft_mute_tune_Tx(freq, &rssi, &valid);
				if (ret == fm_false) {
					WCN_DBG(FM_ERR | CHIP, "mt6630_soft_mute_tune failed\n");
					return 1;
				}

				if (valid == fm_true) {
					*(pScanTBL + cnt) = freq;	/* strore the valid empty channel */
					cnt++;
					WCN_DBG(FM_NTC | CHIP, "empty channel:[freq=%d] [cnt=%d]\n",
						freq, cnt);
				}
				if (cnt >= scan_cnt) {
					break;
				}
			}
		}
	}

	*ScanTBLsize = cnt;
	WCN_DBG(FM_NTC | CHIP, "completed, [cnt=%d],[freq=%d]\n", cnt, freq);
	/* return 875~1080 */
	for (i = 0; i < cnt; i++) {
		if (1 == fm_get_channel_space(*(pScanTBL + i))) {
			*(pScanTBL + i) = *(pScanTBL + i) / 10;
		}
	}
	WCN_DBG(FM_NTC | CHIP, "-%s():[ret=%d]\n", __func__, ret);
	return 0;
}

static fm_s32 mt6630_PowerUpTx(void)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;
	fm_u16 dataRead;

	FM_LOG_NTC(FM_NTC | CHIP, "pwr on Tx seq......\n");

	ret = mt6630_pwrup_top_setting();
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, "mt6630_pwrup_top_setting failed\n");
		return ret;
	}
	if (FM_LOCK(cmd_buf_lock)) {
		return (-FM_ELOCK);
	}
	pkt_size = mt6630_pwrup_clock_on_tx(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6630_pwrup_clock_on_tx failed\n");
		return ret;
	}

	mt6630_read(0x62, &dataRead);
	WCN_DBG(FM_NTC | CHIP, "Tx on chipid=%x\n", dataRead);

	ret = mt6630_pwrup_DSP_download(mt6630_patch_tbl_tx);
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, "mt6630_pwrup_DSP_download failed\n");
		return ret;
	}

	if ((mt6630_fm_config.aud_cfg.aud_path == FM_AUD_MRGIF)
	    || (mt6630_fm_config.aud_cfg.aud_path == FM_AUD_I2S)) {
		mt6630_I2s_Setting(FM_I2S_ON, mt6630_fm_config.aud_cfg.i2s_info.mode,
				   mt6630_fm_config.aud_cfg.i2s_info.rate);
		/* mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X)CMB_STUB_AIF_2);//no need to do? */
		WCN_DBG(FM_NTC | CHIP, "pwron set I2S on ok\n");
	}

	if (FM_LOCK(cmd_buf_lock)) {
		return (-FM_ELOCK);
	}
	pkt_size = mt6630_pwrup_digital_init(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6630_dig_init failed\n");
		return ret;
	}

	if (FM_LOCK(cmd_buf_lock)) {
		return (-FM_ELOCK);
	}
	pkt_size = mt6630_pwrup_tx_deviation(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6630_pwrup__tx_deviation failed\n");
		return ret;
	}

	WCN_DBG(FM_DBG | CHIP, "pwr on tx seq ok\n");
	return ret;
}

static fm_s32 mt6630_PowerDownTx(void)
{
	fm_s32 ret = 0;

	ret = mt6630_PowerDown();
	if (ret) {
		FM_LOG_ERR(FM_ERR | CHIP, "mt6630_PowerDownTx failed\n");
		return ret;
	}

	return ret;
}

static fm_u16 mt6630_Hside_list_Tx[] = { 7720, 8045 };

static fm_bool mt6630_HiSide_chan_check_Tx(fm_u16 freq)
{
	//fm_s32 pos, size;
	fm_u32 i = 0, count = 0;

	/* return 0;//for HQA only: skip FA/HL/ATJ */
	if (0 == fm_get_channel_space(freq)) {
		freq *= 10;
	}
	if (freq < 6500) {
		return fm_false;
	}

	count = sizeof(mt6630_Hside_list_Tx) / sizeof(mt6630_Hside_list_Tx[0]);
	for (i = 0; i < count; i++) {
		if (freq == mt6630_Hside_list_Tx[i])
			return fm_true;
	}

	return fm_false;
}

static fm_bool MT6630_SetFreq_Tx(fm_u16 freq)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;
	fm_u16 chan_para = 0;

	ret = mt6630_RampDown();
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6630_RampDown failed\n");
		return ret;
	}

	if (fm_true == mt6630_HiSide_chan_check_Tx(freq)) {
		FM_LOG_DBG(FM_DBG | CHIP, "%d chan para = %d\n", (fm_s32) freq, (fm_s32) chan_para);
		ret = mt6630_set_bits(FM_CHANNEL_SET, 0x2000, 0x0FFF);	/* mdf HiLo */
	} else
		ret = mt6630_set_bits(FM_CHANNEL_SET, 0x0000, 0xEFFF);	/* clear HiLo */


	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6630_set_bits failed\n");
		return ret;
	}
	/* fm_cb_op->cur_freq_set(freq); */
	/* start tune */
	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6630_tune_tx(cmd_buf, TX_BUF_SIZE, freq, 0);
	ret =
	    fm_cmd_tx(cmd_buf, pkt_size, FLAG_TUNE | FLAG_TUNE_DONE, SW_RETRY_CNT, TUNE_TIMEOUT,
		      NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6630_tune_tx failed\n");
		return ret;
	}

	WCN_DBG(FM_DBG | CHIP, "mt6630_tune_tx to %d ok\n", freq);

	return fm_true;
}


fm_s32 MT6630fm_low_ops_register(struct fm_lowlevel_ops *ops)
{
	fm_s32 ret = 0;
	/* Basic functions. */

	FMR_ASSERT(ops);
	FMR_ASSERT(ops->cb.cur_freq_get);
	FMR_ASSERT(ops->cb.cur_freq_set);
	fm_cb_op = &ops->cb;

	ops->bi.pwron = mt6630_pwron;
	ops->bi.pwroff = mt6630_pwroff;
	ops->bi.msdelay = Delayms;
	ops->bi.usdelay = Delayus;
	ops->bi.read = mt6630_read;
	ops->bi.write = mt6630_write;
	/* ops->bi.top_read = mt6630_top_read; */
	/* ops->bi.top_write = mt6630_top_write; */
	ops->bi.host_read = mt6630_host_read;
	ops->bi.host_write = mt6630_host_write;
	ops->bi.setbits = mt6630_set_bits;
	ops->bi.chipid_get = mt6630_get_chipid;
	ops->bi.mute = mt6630_Mute;
	ops->bi.rampdown = mt6630_RampDown;
	ops->bi.pwrupseq = mt6630_PowerUp;
	ops->bi.pwrdownseq = mt6630_PowerDown;
	ops->bi.setfreq = mt6630_SetFreq;
	/* ops->bi.low_pwr_wa = MT6630fm_low_power_wa_default; */
	ops->bi.i2s_set = mt6630_I2s_Setting;
	ops->bi.rssiget = mt6630_GetCurRSSI;
	ops->bi.volset = mt6630_SetVol;
	ops->bi.volget = mt6630_GetVol;
	ops->bi.dumpreg = mt6630_dump_reg;
	ops->bi.msget = mt6630_GetMonoStereo;
	ops->bi.msset = mt6630_SetMonoStereo;
	ops->bi.pamdget = mt6630_GetCurPamd;
	/* ops->bi.em = mt6630_em_test; */
	ops->bi.anaswitch = mt6630_SetAntennaType;
	ops->bi.anaget = mt6630_GetAntennaType;
	ops->bi.caparray_get = mt6630_GetCapArray;
	ops->bi.hwinfo_get = mt6630_hw_info_get;
	ops->bi.fm_via_bt = MT6630_FMOverBT;
	ops->bi.i2s_get = mt6630_i2s_info_get;
	ops->bi.is_dese_chan = mt6630_is_dese_chan;
	ops->bi.softmute_tune = mt6630_soft_mute_tune;
	ops->bi.desense_check = mt6630_desense_check;
	ops->bi.cqi_log = mt6630_full_cqi_get;
	ops->bi.pre_search = mt6630_pre_search;
	ops->bi.restore_search = mt6630_restore_search;
	ops->bi.set_search_th = mt6630_set_search_th;
	ops->bi.get_aud_info = mt6630fm_get_audio_info;
	/*****tx function****/
	ops->ri.rdstx_support = mt6630_rdsTx_Support;
	ops->bi.tx_support = mt6630_Tx_Support;
	ops->bi.pwrupseq_tx = mt6630_PowerUpTx;
	ops->bi.tune_tx = MT6630_SetFreq_Tx;
	ops->bi.pwrdownseq_tx = mt6630_PowerDownTx;
	ops->bi.tx_scan = mt6630_TxScan;
	ops->ri.rds_tx = MT6630_Rds_Tx;
	ops->ri.rds_tx_enable = MT6630_Rds_Tx_Enable;
	ops->ri.rds_tx_disable = MT6630_Rds_Tx_Disable;
	/* ops->bi.tx_pwr_ctrl = MT6630_TX_PWR_CTRL; */
	/* ops->bi.rtc_drift_ctrl = MT6630_RTC_Drift_CTRL; */
	/* ops->bi.tx_desense_wifi = MT6630_TX_DESENSE; */

	cmd_buf_lock = fm_lock_create("30_cmd");
	ret = fm_lock_get(cmd_buf_lock);

	cmd_buf = fm_zalloc(TX_BUF_SIZE + 1);

	if (!cmd_buf) {
		FM_LOG_ERR(FM_ERR | CHIP, "6630 fm lib alloc tx buf failed\n");
		ret = -1;
	}

	return ret;
}

fm_s32 MT6630fm_low_ops_unregister(struct fm_lowlevel_ops *ops)
{
	fm_s32 ret = 0;
	/* Basic functions. */
	FMR_ASSERT(ops);

	if (cmd_buf) {
		fm_free(cmd_buf);
		cmd_buf = NULL;
	}

	ret = fm_lock_put(cmd_buf_lock);
	fm_memset(&ops->bi, 0, sizeof(struct fm_basic_interface));
	return ret;
}
