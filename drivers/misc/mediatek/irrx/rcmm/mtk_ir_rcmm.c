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
#include "mtk_ir_cus_rcmm.h"

#define MTK_RCMM_BITCNT_NORMAL		(17)
#define RCMM_INFO_TO_BITCNT(u4Info)	\
	((u4Info & IRRX_CH_BITCNT_MASK) >> IRRX_CH_BITCNT_BITSFT)
#define RCMM_INFO_TO_1STPULSE(u4Info) \
	((u4Info & IRRX_CH_1ST_PULSE_MASK) >> IRRX_CH_1ST_PULSE_BITSFT)
#define RCMM_INFO_TO_2NDPULSE(u4Info) \
	((u4Info & IRRX_CH_2ND_PULSE_MASK) >> IRRX_CH_2ND_PULSE_BITSFT)
#define RCMM_INFO_TO_3RDPULSE(u4Info) \
	((u4Info & IRRX_CH_3RD_PULSE_MASK) >> IRRX_CH_3RD_PULSE_BITSFT)

#define IR_RCMM_DEBUG	1
#define IR_RCMM_TAG     "[IRRX_RCMM] "
#define IR_RCMM_FUN(f) \
	do { \
		if (ir_log_debug_on) \
			pr_err(IR_RCMM_TAG"%s++++\n", __func__); \
		else \
			pr_debug(IR_RCMM_TAG"%s++++\n", __func__); \
	} while (0)

#define IR_RCMM_LOG(fmt, args...) \
	do { \
		if (ir_log_debug_on) \
			pr_err(IR_RCMM_TAG fmt, ##args); \
		else if (IR_RCMM_DEBUG) \
			pr_debug(IR_RCMM_TAG fmt, ##args); \
	} while (0)

#define IR_RCMM_ERR(fmt, args...) \
	pr_err(MTK_IR_TAG"%s %d : "fmt, __func__, __LINE__, ##args)

static int mtk_ir_rcmm_enable_hwirq(int enable);
static int mtk_ir_rcmm_init_hw(void);
static int mtk_ir_rcmm_uninit_hw(void);
static u32 mtk_ir_rcmm_decode(void *preserve);

struct mtk_ir_hw ir_rcmm_cust;
static struct mtk_ir_hw *hw = &ir_rcmm_cust;

static u32 _u4Routine_count;
static u32 _u4PrevKey = BTN_NONE;
static u32 _u4Rcmm_customer_code = MTK_IR_RCMM_CUSTOMER_CODE;
#define RCMM_REPEAT_MS 200
#define RCMM_REPEAT_MS_MIN 60

static struct rc_map_list mtk_rcmm_map = {
	.map = {
		.scan = mtk_rcmm_table,
		.size = ARRAY_SIZE(mtk_rcmm_table),
		.rc_proto = RC_PROTO_RCMM,
		.name = RC_MAP_MTK_RCMM,
		}
};

static u32 mtk_ir_rcmm_get_customer_code(void)
{
	return _u4Rcmm_customer_code;
}

static u32 mtk_ir_rcmm_set_customer_code(u32 data)
{
	_u4Rcmm_customer_code = data;
	IR_RCMM_LOG("_u4Rcmm_customer_code = 0x%x\n", _u4Rcmm_customer_code);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mtk_ir_rcmm_early_suspend(void *preserve)
{
	IR_RCMM_FUN();
}

static void mtk_ir_rcmm_late_resume(void *preserve)
{
	IR_RCMM_FUN();
}

#else

#define mtk_ir_rcmm_early_suspend NULL
#define mtk_ir_rcmm_late_resume NULL

#endif


#ifdef CONFIG_PM_SLEEP
static int mtk_ir_rcmm_suspend(void *preserve)
{
	IR_RCMM_FUN();
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
		IRRX_EXP_BITCNT_OFFSET, 0x11);
	IR_WRITE32(IRRX_ENEXP_IRM, MTK_RCMM_EXP_IRM_BIT_MASK);
	IR_WRITE32(IRRX_ENEXP_IRL, MTK_RCMM_EXP_IRL_BIT_MASK);
	IR_WRITE32(IRRX_EXP_IRM0, MTK_RCMM_EXP_POWE_KEY1);
	IR_WRITE32(IRRX_EXP_IRL0, MTK_RCMM_EXP_POWE_KEY2);
	return 0;
}

static int mtk_ir_rcmm_resume(void *preserve)
{
	IR_RCMM_FUN();
	IR_WRITE_MASK(IRRX_IREXPEN, IRRX_IREXPEN_MASK,
		IRRX_IREXPEN_OFFSET, 0x0);
	IR_WRITE_MASK(IRRX_IREXPEN, IRRX_BCEPEN_MASK,
		IRRX_BCEPEN_OFFSET, 0x0);
	IR_WRITE_MASK(IRRX_WAKECLR, IRRX_WAKECLR_MASK,
		IRRX_WAKECLR_OFFSET, 0x1);
	return 0;
}

#else

#define mtk_ir_rcmm_suspend NULL
#define mtk_ir_rcmm_resume NULL

#endif

static int mtk_ir_rcmm_enable_hwirq(int enable)
{
	IR_RCMM_LOG("IRRX enable hwirq: %d\n", enable);
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
 * This timer function is for RCMM routine work.
 * You can add stuff you want to do in this function.
 */
static int mtk_ir_rcmm_timer_func(const char *data)
{
	u32 CHK_CNT_PULSE = 0;

	CHK_CNT_PULSE = (IR_READ32(IRRX_EXPBCNT) >> IRRX_IRCHK_CNT_OFFSET) &
		IRRX_IRCHK_CNT;
	if (CHK_CNT_PULSE == IRRX_IRCHK_CNT) {
		_u4Routine_count++;
		if (data[0]) {
			IR_RCMM_ERR("CHK_CNT_PULSE = 0x%x\n", CHK_CNT_PULSE);
			IR_RCMM_ERR("_u4Routine_count=%d\n", _u4Routine_count);
		}
		return mtk_ir_rcmm_enable_hwirq(1);
	}
	return 0;
}

static struct mtk_ir_core_platform_data mtk_ir_pdata_rcmm = {
	.device_name = MTK_INPUT_DEVICE_NAME,
	.p_map_list = &mtk_rcmm_map,
	.i4_keypress_timeout = MTK_IR_RCMM_KEYPRESS_TIMEOUT,
	.enable_hwirq = mtk_ir_rcmm_enable_hwirq,
	.init_hw = mtk_ir_rcmm_init_hw,
	.uninit_hw = mtk_ir_rcmm_uninit_hw,
	.ir_hw_decode = mtk_ir_rcmm_decode,
	.get_customer_code = mtk_ir_rcmm_get_customer_code,
	.set_customer_code = mtk_ir_rcmm_set_customer_code,
	.timer_func = mtk_ir_rcmm_timer_func,
	.mouse_step = {
		MOUSE_SMALL_X_STEP,
		MOUSE_SMALL_Y_STEP,
		MOUSE_LARGE_X_STEP,
		MOUSE_LARGE_Y_STEP
	},
#ifdef CONFIG_HAS_EARLYSUSPEND
	.early_suspend = mtk_ir_rcmm_early_suspend,
	.late_resume = mtk_ir_rcmm_late_resume,
#endif

#ifdef CONFIG_PM_SLEEP
	.suspend = mtk_ir_rcmm_suspend,
	.resume = mtk_ir_rcmm_resume,
#endif
};

static int mtk_ir_rcmm_uninit_hw(void)
{
	mtk_ir_rcmm_enable_hwirq(0);
	rc_map_unregister(&mtk_rcmm_map);
	return 0;
}


static int mtk_ir_rcmm_init_hw(void)
{
	enum MTK_IR_MODE ir_mode = mtk_ir_core_getmode();
	struct rc_map_table *p_table = NULL;
	int size = 0;
	int i = 0;

	IR_RCMM_FUN();
	IR_RCMM_LOG(" ir_mode = %d\n", ir_mode);

	if (ir_mode == MTK_IR_FACTORY) {
		mtk_rcmm_map.map.scan = mtk_rcmm_factory_table;
		mtk_rcmm_map.map.size = ARRAY_SIZE(mtk_rcmm_factory_table);
	} else {
		mtk_rcmm_map.map.scan = mtk_rcmm_table;
		mtk_rcmm_map.map.size = ARRAY_SIZE(mtk_rcmm_table);
		p_table = mtk_rcmm_map.map.scan;
		size = mtk_rcmm_map.map.size;

		memset(&(mtk_ir_pdata_rcmm.mouse_code), 0xff,
			sizeof(mtk_ir_pdata_rcmm.mouse_code));
		for (; i < size; i++) {
			if (p_table[i].keycode == KEY_LEFT)
				mtk_ir_pdata_rcmm.mouse_code.scanleft =
				p_table[i].scancode;
			else if (p_table[i].keycode == KEY_RIGHT)
				mtk_ir_pdata_rcmm.mouse_code.scanright =
				p_table[i].scancode;
			else if (p_table[i].keycode == KEY_UP)
				mtk_ir_pdata_rcmm.mouse_code.scanup =
				p_table[i].scancode;
			else if (p_table[i].keycode == KEY_DOWN)
				mtk_ir_pdata_rcmm.mouse_code.scandown =
				p_table[i].scancode;
			else if (p_table[i].keycode == KEY_ENTER)
				mtk_ir_pdata_rcmm.mouse_code.scanenter =
				p_table[i].scancode;
		}
		mtk_ir_pdata_rcmm.mouse_support = MTK_IR_SUPPORT_MOUSE_INPUT;
		mtk_ir_pdata_rcmm.mouse_code.scanswitch =
			MTK_IR_MOUSE_RCMM_SWITCH_CODE;
		mtk_ir_pdata_rcmm.mousename[0] = '\0';
		strcat(mtk_ir_pdata_rcmm.mousename,
			mtk_ir_pdata_rcmm.device_name);
		strcat(mtk_ir_pdata_rcmm.mousename, "_Mouse");
		mtk_ir_mouse_set_device_mode(MTK_IR_MOUSE_MODE_DEFAULT);
	}

	rc_map_register(&mtk_rcmm_map);
	IR_RCMM_LOG(" rc_map_register ok.\n");

	/* disable interrupt */
	mtk_ir_rcmm_enable_hwirq(0);
	IR_WRITE32(IRRX_CONFIG_HIGH_REG, MTK_RCMM_CONFIG);
	IR_WRITE32(IRRX_CONFIG_LOW_REG, MTK_RCMM_SAPERIOD);
	IR_WRITE32(IRRX_THRESHOLD_REG, MTK_RCMM_THRESHOLD);
	IR_WRITE32(IRRX_RCMM_THD_REG,  MTK_RCMM_THRESHOLD_REG);
	IR_WRITE32(IRRX_RCMM_THD_REG0, MTK_RCMM_THRESHOLD_REG_0);
	IR_RCMM_LOG(" config:0x%08x %08x, threshold:0x%08x\n",
		IR_READ32(IRRX_CONFIG_HIGH_REG), IR_READ32(IRRX_CONFIG_LOW_REG),
		IR_READ32(IRRX_THRESHOLD_REG));
	IR_RCMM_LOG("rcmm_threshold_reg: 0x%08x, rcmm_threshold_reg0: 0x%08x\n",
		IR_READ32(IRRX_RCMM_THD_REG), IR_READ32(IRRX_RCMM_THD_REG0));

	IR_RCMM_LOG("%s----\n", _func__);
	return 0;
}

u32 mtk_ir_rcmm_decode(void *preserve)
{
	u32 _au4IrRxData[2];
	u32 _u4Info = IR_READ32(IRRX_COUNT_HIGH_REG);
	u32 u4BitCnt = RCMM_INFO_TO_BITCNT(_u4Info);
	u32 u4GroupID = 0;
	char *pu1Data = (char *)_au4IrRxData;
	static unsigned long last_jiffers;
	unsigned long current_jiffers = jiffies;

	_au4IrRxData[0] = IR_READ32(IRRX_COUNT_MID_REG);
	_au4IrRxData[1] = IR_READ32(IRRX_COUNT_LOW_REG);

	if ((_au4IrRxData[0] != 0) ||
		(_au4IrRxData[1] != 0) ||
		(_u4Info != 0)) {
		IR_RCMM_LOG("IRRX Info:0x%08x data:0x%08x %08x\n", _u4Info,
			_au4IrRxData[1], _au4IrRxData[0]);
	} else {
		IR_RCMM_ERR("invalid key!!!\n");
		return BTN_INVALID_KEY;
	}

	/* Check invalid pulse */
	if (u4BitCnt != MTK_RCMM_BITCNT_NORMAL) {
		IR_RCMM_ERR("u4BitCnt(%d), should be(%d)!!!\n", u4BitCnt,
			MTK_RCMM_BITCNT_NORMAL);
		_u4PrevKey = BTN_NONE;
		goto end;
	}

	/* Check GroupId */
	u4GroupID = (pu1Data[0] & 0xf0);
	if (u4GroupID != _u4Rcmm_customer_code) {
		IR_RCMM_ERR("invalid customer code 0x%x!!!\n", u4GroupID);
		_u4PrevKey = BTN_NONE;
		goto end;
	}

	_u4PrevKey = MTK_IR_RCMM_GET_KEYCODE(pu1Data[3]);

end:

	IR_RCMM_LOG("repeat_out=%dms\n", jiffies_to_msecs(current_jiffers -
		last_jiffers));
	last_jiffers = current_jiffers;

	MTK_IR_KEY_LOG("ID:0x%08x, Key:0x%08x\n", u4GroupID, _u4PrevKey);
	return _u4PrevKey;

}

static int ir_rcmm_local_init(void)
{
	int err = 0;

	IR_RCMM_FUN();
	err += mtk_ir_register_ctl_data_path(&mtk_ir_pdata_rcmm, hw,
		RC_PROTO_RCMM);
	IR_RCMM_LOG("%s(%d)----\n", __func__, err);
	return err;
}

static int ir_rcmm_remove(void)
{
	int err = 0;
	/* err += mtk_ir_rcmm_uninit_hw(); */
	return err;
}

static struct mtk_ir_init_info ir_rcmm_init_info = {
	.name = "ir_rcmm",
	.init = ir_rcmm_local_init,
	.uninit = ir_rcmm_remove,
};

static int __init ir_rcmm_init(void)
{
	IR_RCMM_FUN();
	hw = get_mtk_ir_cus_dts(hw);
	if (hw == NULL)
		IR_RCMM_ERR("get dts info fail\n");

	mtk_ir_driver_add(&ir_rcmm_init_info);
	IR_RCMM_LOG("%s----\n", __func__);
	return 0;
}

static void __exit ir_rcmm_exit(void)
{
	IR_RCMM_FUN();
}

module_init(ir_rcmm_init);
module_exit(ir_rcmm_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("mtk ir_rcmm driver");
MODULE_AUTHOR("Zhimin.Tang@mediatek.com");
