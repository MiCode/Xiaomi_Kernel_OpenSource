#ifndef __MT6630_FM_CMD_H__
#define __MT6630_FM_CMD_H__

#include <linux/types.h>
#include "fm_typedef.h"

/* FM basic-operation's opcode */
#define FM_BOP_BASE (0x80)
enum {
	FM_WRITE_BASIC_OP = (FM_BOP_BASE + 0x00),
	FM_UDELAY_BASIC_OP = (FM_BOP_BASE + 0x01),
	FM_RD_UNTIL_BASIC_OP = (FM_BOP_BASE + 0x02),
	FM_MODIFY_BASIC_OP = (FM_BOP_BASE + 0x03),
	FM_MSLEEP_BASIC_OP = (FM_BOP_BASE + 0x04),
	FM_TOP_WRITE_BASIC_OP = (FM_BOP_BASE + 0x05),
	FM_TOP_RD_UNTIL_BASIC_OP = (FM_BOP_BASE + 0x06),
	FM_TOP_MODIFY_BASIC_OP = (FM_BOP_BASE + 0x07),
	FM_MAX_BASIC_OP = (FM_BOP_BASE + 0x08)
};

/* FM BOP's size */
#define FM_TOP_WRITE_BOP_SIZE      (7)
#define FM_TOP_RD_UNTIL_BOP_SIZE     (11)
#define FM_TOP_MODIFY_BOP_SIZE     (11)

#define FM_WRITE_BASIC_OP_SIZE      (3)
#define FM_UDELAY_BASIC_OP_SIZE     (4)
#define FM_RD_UNTIL_BASIC_OP_SIZE   (5)
#define FM_MODIFY_BASIC_OP_SIZE     (5)
#define FM_MSLEEP_BASIC_OP_SIZE     (4)

fm_s32 mt6630_pwrup_fpga_on(fm_u8 *buf, fm_s32 buf_size);
fm_s32 mt6630_pwrup_clock_on(fm_u8 *buf, fm_s32 buf_size);
fm_s32 mt6630_pwrup_digital_init(fm_u8 *buf, fm_s32 buf_size);
fm_s32 mt6630_pwrdown(fm_u8 *buf, fm_s32 buf_size);
fm_s32 mt6630_rampdown(fm_u8 *buf, fm_s32 buf_size);
fm_s32 mt6630_tune(fm_u8 *buf, fm_s32 buf_size, fm_u16 freq, fm_u16 chan_para);
fm_s32 mt6630_seek(fm_u8 *buf, fm_s32 buf_size, fm_u16 seekdir, fm_u16 space, fm_u16 max_freq,
		   fm_u16 min_freq);
fm_s32 mt6630_scan(fm_u8 *buf, fm_s32 buf_size, fm_u16 scandir, fm_u16 space, fm_u16 max_freq,
		   fm_u16 min_freq);
fm_s32 mt6630_cqi_get(fm_u8 *buf, fm_s32 buf_size);
fm_s32 mt6630_get_reg(fm_u8 *buf, fm_s32 buf_size, fm_u8 addr);
fm_s32 mt6630_set_reg(fm_u8 *buf, fm_s32 buf_size, fm_u8 addr, fm_u16 value);
fm_s32 mt6630_patch_download(fm_u8 *buf, fm_s32 buf_size, fm_u8 seg_num, fm_u8 seg_id,
			     const fm_u8 *src, fm_s32 seg_len);
fm_s32 mt6630_coeff_download(fm_u8 *buf, fm_s32 buf_size, fm_u8 seg_num, fm_u8 seg_id,
			     const fm_u8 *src, fm_s32 seg_len);
#if 0
fm_s32 mt6630_hwcoeff_download(fm_u8 *buf, fm_s32 buf_size, fm_u8 seg_num, fm_u8 seg_id,
			       const fm_u8 *src, fm_s32 seg_len);
fm_s32 mt6630_rom_download(fm_u8 *buf, fm_s32 buf_size, fm_u8 seg_num, fm_u8 seg_id,
			   const fm_u8 *src, fm_s32 seg_len);
#endif
fm_s32 mt6630_full_cqi_req(fm_u8 *buf, fm_s32 buf_size, fm_u16 *freq, fm_s32 cnt, fm_s32 type);
fm_s32 mt6630_top_get_reg(fm_u8 *buf, fm_s32 buf_size, fm_u16 addr);
fm_s32 mt6630_top_set_reg(fm_u8 *buf, fm_s32 buf_size, fm_u16 addr, fm_u32 value);
fm_s32 mt6630_host_get_reg(fm_u8 *buf, fm_s32 buf_size, fm_u32 addr);
fm_s32 mt6630_host_set_reg(fm_u8 *buf, fm_s32 buf_size, fm_u32 addr, fm_u32 value);
fm_s32 mt6630_set_bits_reg(fm_u8 *buf, fm_s32 buf_size, fm_u8 addr, fm_u16 bits, fm_u16 mask);
/*****************Tx***********************/
fm_s32 mt6630_tune_tx(fm_u8 *buf, fm_s32 buf_size, fm_u16 freq, fm_u16 chan_para);
fm_s32 mt6630_pwrup_clock_on_tx(fm_u8 *buf, fm_s32 buf_size);
fm_s32 mt6630_pwrup_tx_deviation(fm_u8 *buf, fm_s32 buf_size);
fm_s32 mt6630_rds_tx(fm_u8 *tx_buf, fm_s32 tx_buf_size, fm_u16 pi, fm_u16 *ps, fm_u16 *other_rds,
		     fm_u8 other_rds_cnt);
fm_s32 mt6630_tx_rdson_deviation(fm_u8 *buf, fm_s32 buf_size);

/*
 * fm_get_channel_space - get the spcace of gived channel
 * @freq - value in 760~1080 or 7600~10800
 *
 * Return 0, if 760~1080; return 1, if 7600 ~ 10800, else err code < 0
 */
extern fm_s32 fm_get_channel_space(int freq);

#endif
