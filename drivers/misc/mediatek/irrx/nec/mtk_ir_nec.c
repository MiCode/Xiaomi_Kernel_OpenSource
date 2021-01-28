/*
 * Copyright (C) 2016 MediaTek Inc.
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
#include <linux/jiffies.h>
#include "../inc/mtk_ir_common.h"
#include "../inc/mtk_ir_core.h"
#include "../inc/mtk_ir_regs.h"
#include "mtk_ir_cus_nec.h"

#define MTK_NEC_1ST_PULSE_REPEAT	(3)
#define MTK_NEC_BITCNT_NORMAL		(33)
#define MTK_NEC_BITCNT_REPEAT		(1)
#define MTK_NEC_BIT8_VERIFY		(0xff)

#define NEC_INFO_TO_BITCNT(u4Info) \
	((u4Info & IRRX_CH_BITCNT_MASK) >> IRRX_CH_BITCNT_BITSFT)
#define NEC_INFO_TO_1STPULSE(u4Info) \
	((u4Info & IRRX_CH_1ST_PULSE_MASK) >> IRRX_CH_1ST_PULSE_BITSFT)
#define NEC_INFO_TO_2NDPULSE(u4Info) \
	((u4Info & IRRX_CH_2ND_PULSE_MASK) >> IRRX_CH_2ND_PULSE_BITSFT)
#define NEC_INFO_TO_3RDPULSE(u4Info) \
	((u4Info & IRRX_CH_3RD_PULSE_MASK) >> IRRX_CH_3RD_PULSE_BITSFT)

#define IR_NEC_DEBUG	1
#define IR_NEC_TAG      "[IRRX_NEC] "
#define IR_NEC_FUN(f) \
	do { \
		if (ir_log_debug_on) \
			pr_err(IR_NEC_TAG"%s++++\n", __func__); \
		else \
			pr_debug(IR_NEC_TAG"%s++++\n", __func__); \
	} while (0)

#define IR_NEC_LOG(fmt, args...) \
	do { \
		if (ir_log_debug_on) \
			pr_err(IR_NEC_TAG fmt, ##args); \
		else if (IR_NEC_DEBUG) \
			pr_debug(IR_NEC_TAG fmt, ##args); \
	} while (0)

#define IR_NEC_ERR(fmt, args...) \
	pr_err(MTK_IR_TAG"%s %d : "fmt, __func__, __LINE__, ##args)

static int mtk_ir_nec_enable_hwirq(int enable);
static int mtk_ir_nec_init_hw(void);
static int mtk_ir_nec_uninit_hw(void);
static u32 mtk_ir_nec_decode(void *preserve);

struct mtk_ir_hw ir_nec_cust;
static struct mtk_ir_hw *hw = &ir_nec_cust;

static u32 _u4Routine_count;
static u32 _u4PrevKey = BTN_NONE;
static u32 _u4Nec_customer_code = MTK_IR_NEC_CUSTOMER_CODE;
#define NEC_REPEAT_MS 200
#define NEC_REPEAT_MS_MIN 60

static struct rc_map_list mtk_nec_map = {
	.map = {
		.scan = mtk_nec_table,
		.size = ARRAY_SIZE(mtk_nec_table),
		.rc_proto = RC_PROTO_NEC,
		.name = RC_MAP_MTK_NEC,
		}
};

static u32 mtk_ir_nec_get_customer_code(void)
{
	return _u4Nec_customer_code;
}

static u32 mtk_ir_nec_set_customer_code(u32 data)
{
	_u4Nec_customer_code = data;
	IR_NEC_LOG("_u4Nec_customer_code = 0x%x\n", _u4Nec_customer_code);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mtk_ir_nec_early_suspend(void *preserve)
{
	IR_NEC_FUN();
}

static void mtk_ir_nec_late_resume(void *preserve)
{
	IR_NEC_FUN();
}

#else

#define mtk_ir_nec_early_suspend NULL
#define mtk_ir_nec_late_resume NULL

#endif


#ifdef CONFIG_PM_SLEEP
static int mtk_ir_nec_suspend(void *preserve)
{
	IR_NEC_FUN();
	/* 0x24 bit[7:0]: set except register
	 * 0000,0001: IRL0 & IRM0
	 * 0000,0010: IRL1 & IRM1
	 * 0000,0100: IRL2 & IRM2
	 * 0000,1000: IRL3 & IRM3
	 * 0001,0000: IRL4 & IRM4
	 * 0010,0000: IRL5 & IRM5
	 * 0100,0000: IRL6 & IRM6
	 * 1000,0000: IRL7 & IRM7
	 */
	IR_WRITE_MASK(IRRX_WAKECLR, IRRX_WAKECLR_MASK,
		IRRX_WAKECLR_OFFSET, 0x1);
	IR_WRITE_MASK(IRRX_WAKEEN, IRRX_WAKEEN_MASK,
		IRRX_WAKEEN_OFFSET, 0x1);
	IR_WRITE_MASK(IRRX_IREXPEN, IRRX_IREXPEN_MASK,
		IRRX_IREXPEN_OFFSET, 0x1);
	IR_WRITE_MASK(IRRX_IREXPEN, IRRX_BCEPEN_MASK,
		IRRX_BCEPEN_OFFSET, 0x1);
	IR_WRITE_MASK(IRRX_EXPBCNT, IRRX_EXP_BITCNT_MASK,
		IRRX_EXP_BITCNT_OFFSET, 0x21);
	IR_WRITE32(IRRX_ENEXP_IRM, MTK_NEC_EXP_IRM_BIT_MASK);
	IR_WRITE32(IRRX_ENEXP_IRL, MTK_NEC_EXP_IRL_BIT_MASK);
	IR_WRITE32(IRRX_EXP_IRM0, MTK_NEC_EXP_POWE_KEY1);
	IR_WRITE32(IRRX_EXP_IRL0, MTK_NEC_EXP_POWE_KEY2);

	return 0;
}

static int mtk_ir_nec_resume(void *preserve)
{
	IR_NEC_FUN();
	IR_WRITE_MASK(IRRX_IREXPEN, IRRX_IREXPEN_MASK,
		IRRX_IREXPEN_OFFSET, 0x0);
	IR_WRITE_MASK(IRRX_IREXPEN, IRRX_BCEPEN_MASK,
		IRRX_BCEPEN_OFFSET, 0x0);
	IR_WRITE_MASK(IRRX_WAKEEN, IRRX_WAKEEN_MASK,
		IRRX_WAKEEN_OFFSET, 0x0);
	IR_WRITE_MASK(IRRX_WAKECLR, IRRX_WAKECLR_MASK,
		IRRX_WAKECLR_OFFSET, 0x1);

	return 0;
}

#else

#define mtk_ir_nec_suspend NULL
#define mtk_ir_nec_resume NULL

#endif

static int mtk_ir_nec_enable_hwirq(int enable)
{
	IR_NEC_LOG("IRRX enable hwirq: %d\n", enable);
	if (enable) {
		IR_WRITE_MASK(IRRX_IRINT_CLR, IRRX_INTCLR_MASK,
			IRRX_INTCLR_OFFSET, 0x1);
		dsb(sy);
		IR_WRITE_MASK(IRRX_IRCLR, IRRX_IRCLR_MASK,
			IRRX_IRCLR_OFFSET, 0x1);
		dsb(sy);
		IR_WRITE_MASK(IRRX_IRINT_EN, IRRX_INTEN_MASK,
			IRRX_INTEN_OFFSET, 0x1);
		dsb(sy);
	} else {
		IR_WRITE_MASK(IRRX_IRINT_CLR, IRRX_INTCLR_MASK,
			IRRX_INTCLR_OFFSET, 0x1);
		dsb(sy);
		IR_WRITE_MASK(IRRX_IRINT_EN, IRRX_INTEN_MASK,
			IRRX_INTEN_OFFSET, 0x0);
		dsb(sy);
	}
	return 0;
}

/*
 * This timer function is for NEC routine work.
 * You can add stuff you want to do in this function.
 */
static int mtk_ir_nec_timer_func(const char *data)
{
	u32 CHK_CNT_PULSE = 0;

	CHK_CNT_PULSE = (IR_READ32(IRRX_EXPBCNT) >> IRRX_IRCHK_CNT_OFFSET) &
		IRRX_IRCHK_CNT;
	if (CHK_CNT_PULSE == IRRX_IRCHK_CNT) {
		_u4Routine_count++;
		if (data[0]) {
			IR_NEC_ERR("CHK_CNT_PULSE = 0x%x\n", CHK_CNT_PULSE);
			IR_NEC_ERR("_u4Routine_count=%d\n", _u4Routine_count);
		}
		return mtk_ir_nec_enable_hwirq(1);
	}
	return 0;
}

static struct mtk_ir_core_platform_data mtk_ir_pdata_nec = {
	.device_name = MTK_INPUT_DEVICE_NAME,
	.p_map_list = &mtk_nec_map,
	.i4_keypress_timeout = MTK_IR_NEC_KEYPRESS_TIMEOUT,
	.enable_hwirq = mtk_ir_nec_enable_hwirq,
	.init_hw = mtk_ir_nec_init_hw,
	.uninit_hw = mtk_ir_nec_uninit_hw,
	.ir_hw_decode = mtk_ir_nec_decode,
	.get_customer_code = mtk_ir_nec_get_customer_code,
	.set_customer_code = mtk_ir_nec_set_customer_code,
	.timer_func = mtk_ir_nec_timer_func,
	.mouse_step = {
		MOUSE_SMALL_X_STEP,
		MOUSE_SMALL_Y_STEP,
		MOUSE_LARGE_X_STEP,
		MOUSE_LARGE_Y_STEP
	},
#ifdef CONFIG_HAS_EARLYSUSPEND
	.early_suspend = mtk_ir_nec_early_suspend,
	.late_resume = mtk_ir_nec_late_resume,
#endif

#ifdef CONFIG_PM_SLEEP
	.suspend = mtk_ir_nec_suspend,
	.resume = mtk_ir_nec_resume,
#endif
};

static int mtk_ir_nec_uninit_hw(void)
{
	mtk_ir_nec_enable_hwirq(0);
	rc_map_unregister(&mtk_nec_map);
	return 0;
}

static int mtk_ir_nec_init_hw(void)
{
	enum MTK_IR_MODE ir_mode = mtk_ir_core_getmode();
	struct rc_map_table *p_table = NULL;
	int size = 0;
	int i = 0;

	IR_NEC_FUN();
	IR_NEC_LOG(" ir_mode = %d\n", ir_mode);

	if (ir_mode == MTK_IR_FACTORY) {
		mtk_nec_map.map.scan = mtk_nec_factory_table;
		mtk_nec_map.map.size = ARRAY_SIZE(mtk_nec_factory_table);
	} else {
		mtk_nec_map.map.scan = mtk_nec_table;
		mtk_nec_map.map.size = ARRAY_SIZE(mtk_nec_table);
		p_table = mtk_nec_map.map.scan;
		size = mtk_nec_map.map.size;

		memset(&(mtk_ir_pdata_nec.mouse_code), 0xff,
			sizeof(mtk_ir_pdata_nec.mouse_code));
		for (; i < size; i++) {
			if (p_table[i].keycode == KEY_LEFT)
				mtk_ir_pdata_nec.mouse_code.scanleft =
					p_table[i].scancode;
			else if (p_table[i].keycode == KEY_RIGHT)
				mtk_ir_pdata_nec.mouse_code.scanright =
					p_table[i].scancode;
			else if (p_table[i].keycode == KEY_UP)
				mtk_ir_pdata_nec.mouse_code.scanup =
					p_table[i].scancode;
			else if (p_table[i].keycode == KEY_DOWN)
				mtk_ir_pdata_nec.mouse_code.scandown =
					p_table[i].scancode;
			else if (p_table[i].keycode == KEY_ENTER)
				mtk_ir_pdata_nec.mouse_code.scanenter =
					p_table[i].scancode;
		}
		mtk_ir_pdata_nec.mouse_support = MTK_IR_SUPPORT_MOUSE_INPUT;
		mtk_ir_pdata_nec.mouse_code.scanswitch =
			MTK_IR_MOUSE_NEC_SWITCH_CODE;
		mtk_ir_pdata_nec.mousename[0] = '\0';
		strcat(mtk_ir_pdata_nec.mousename,
			mtk_ir_pdata_nec.device_name);
		strcat(mtk_ir_pdata_nec.mousename, "_Mouse");
		mtk_ir_mouse_set_device_mode(MTK_IR_MOUSE_MODE_DEFAULT);
	}

	rc_map_register(&mtk_nec_map);
	IR_NEC_LOG(" rc_map_register ok.\n");

	/* disable interrupt */
	mtk_ir_nec_enable_hwirq(0);
	IR_WRITE32(IRRX_CONFIG_HIGH_REG, MTK_NEC_CONFIG);
	IR_WRITE32(IRRX_CONFIG_LOW_REG, MTK_NEC_SAPERIOD);
	IR_WRITE32(IRRX_THRESHOLD_REG, MTK_NEC_THRESHOLD);
	IR_NEC_LOG("config: 0x%08x %08x, threshold: 0x%08x\n",
		IR_READ32(IRRX_CONFIG_HIGH_REG), IR_READ32(IRRX_CONFIG_LOW_REG),
		IR_READ32(IRRX_THRESHOLD_REG));

	IR_NEC_LOG("%s----\n", __func__);
	return 0;
}

u32 mtk_ir_nec_decode(void *preserve)
{
	u32 _au4IrRxData[2];
	u32 _u4Info = IR_READ32(IRRX_COUNT_HIGH_REG);
	u32 u4BitCnt = NEC_INFO_TO_BITCNT(_u4Info);
	u32 u4GroupID = 0;
	u32 u41stPulse, u42ndPulse, u43rdPulse;
	char *pu1Data = (char *)_au4IrRxData;
	static unsigned long _lRepeatKeyCnt;
	static unsigned long last_jiffers;
	unsigned long current_jiffers = jiffies;

	_au4IrRxData[0] = IR_READ32(IRRX_COUNT_MID_REG);
	_au4IrRxData[1] = IR_READ32(IRRX_COUNT_LOW_REG);
	u41stPulse = NEC_INFO_TO_1STPULSE(_u4Info);
	u42ndPulse = NEC_INFO_TO_2NDPULSE(_u4Info);
	u43rdPulse = NEC_INFO_TO_3RDPULSE(_u4Info);

	if ((_au4IrRxData[0] != 0) ||
		(_au4IrRxData[1] != 0) ||
		(_u4Info != 0)) {
		IR_NEC_LOG("IRRX Info:0x%08x data:0x%08x %08x\n", _u4Info,
			   _au4IrRxData[1], _au4IrRxData[0]);
	} else {
		IR_NEC_ERR("invalid key!!!\n");
		return BTN_INVALID_KEY;
	}

	/* Check repeat key */
	if (u4BitCnt == MTK_NEC_BITCNT_REPEAT) {
		_lRepeatKeyCnt++;
		if (((u41stPulse == MTK_NEC_1ST_PULSE_REPEAT) ||
			(u41stPulse == MTK_NEC_1ST_PULSE_REPEAT - 1) ||
			(u41stPulse == MTK_NEC_1ST_PULSE_REPEAT + 1)) &&
			(u42ndPulse == 0) && (u43rdPulse == 0)) {

			/* repeat key ,may be disturbed waveform */
		} else {
			IR_NEC_ERR("invalid repeat key!!!\n");
			_u4PrevKey = BTN_NONE;
		}

		if (time_after(current_jiffers, last_jiffers) > 0) {
			if (jiffies_to_msecs(current_jiffers - last_jiffers) >
				NEC_REPEAT_MS) {
				IR_NEC_ERR
				    ("ng repeat key repeat_out=%dms!!!\n",
				     jiffies_to_msecs(current_jiffers -
				     last_jiffers));
				_u4PrevKey = BTN_NONE;
			}
		}
		goto end;
	} else
		_lRepeatKeyCnt = 0;

	/* Check invalid pulse */
	if (u4BitCnt != MTK_NEC_BITCNT_NORMAL) {
		IR_NEC_ERR("u4BitCnt(%d), should be(%d)!!!\n",
			u4BitCnt, MTK_NEC_BITCNT_NORMAL);
		_u4PrevKey = BTN_NONE;
		goto end;
	}
	/* Check invalid key */
	if ((pu1Data[2] + pu1Data[3]) != MTK_NEC_BIT8_VERIFY) {
		IR_NEC_ERR("invalid nec key code!!!\n");
		_u4PrevKey = BTN_NONE;
		goto end;
	}

	u4GroupID = ((pu1Data[1] << 8) + pu1Data[0]);

	/* Check GroupId */
	if (u4GroupID != _u4Nec_customer_code) {
		IR_NEC_ERR("invalid customer code 0x%x!!!\n", u4GroupID);
		_u4PrevKey = BTN_NONE;
		goto end;
	}

	_u4PrevKey = pu1Data[2];

end:

	IR_NEC_LOG("repeat_out=%dms\n", jiffies_to_msecs(current_jiffers -
		last_jiffers));
	last_jiffers = current_jiffers;

	MTK_IR_KEY_LOG("ID:0x%08x, Key:0x%08x\n", u4GroupID, _u4PrevKey);
	return _u4PrevKey;

}

static int ir_nec_local_init(void)
{
	int err = 0;

	IR_NEC_FUN();
	err += mtk_ir_register_ctl_data_path(&mtk_ir_pdata_nec,
		hw, RC_PROTO_NEC);
	IR_NEC_LOG("%s(%d)----\n", __func__, err);
	return err;
}

static int ir_nec_remove(void)
{
	int err = 0;
	/* err += mtk_ir_nec_uninit_hw(); */
	return err;
}

static struct mtk_ir_init_info ir_nec_init_info = {
	.name = "ir_nec",
	.init = ir_nec_local_init,
	.uninit = ir_nec_remove,
};

static int __init ir_nec_init(void)
{
	IR_NEC_FUN();
	hw = get_mtk_ir_cus_dts(hw);
	if (hw == NULL)
		IR_NEC_ERR("get dts info fail\n");

	mtk_ir_driver_add(&ir_nec_init_info);
	IR_NEC_LOG("%s----\n", __func__);
	return 0;
}

static void __exit ir_nec_exit(void)
{
	IR_NEC_FUN();
}

module_init(ir_nec_init);
module_exit(ir_nec_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("mtk ir_nec driver");
MODULE_AUTHOR("Zhimin.Tang@mediatek.com");
