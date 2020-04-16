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
#include "mtk_ir_cus_rc5.h"

#define MTK_RC5_BITCNT   0x14
#define MTK_RC5_TOGGLE0  0x0
#define MTK_RC5_TOGGLE1  0x2

#define RC5_INFO_TO_BITCNT(u4Info) \
	((u4Info & IRRX_CH_BITCNT_MASK) >> IRRX_CH_BITCNT_BITSFT)
#define MTK_RC5_GET_TOGGLE(bdata0) ((bdata0 & 0xc) >> 2)
#define MTK_RC5_GET_KEYCODE(bdata0, bdata1) \
	(((bdata0 & 0xF) << 4) | ((bdata1 & 0xF0) >> 4))

#define IR_RC5_DEBUG    1
#define IR_RC5_TAG      "[IRRX_RC5] "
#define IR_RC5_FUN(f) \
	do { \
		if (ir_log_debug_on) \
			pr_err(IR_RC5_TAG"%s++++\n", __func__); \
		else \
			pr_debug(IR_RC5_TAG"%s++++\n", __func__); \
	} while (0)

#define IR_RC5_LOG(fmt, args...) \
	do { \
		if (ir_log_debug_on) \
			pr_err(IR_RC5_TAG fmt, ##args); \
		else if (IR_RC5_DEBUG) \
			pr_debug(IR_RC5_TAG fmt, ##args); \
	} while (0)

#define IR_RC5_ERR(fmt, args...) \
	pr_err(MTK_IR_TAG"%s %d : "fmt, __func__, __LINE__, ##args)

static int mtk_ir_rc5_enable_hwirq(int enable);
static int mtk_ir_rc5_init_hw(void);
static int mtk_ir_rc5_uninit_hw(void);
static u32 mtk_ir_rc5_decode(void *preserve);

struct mtk_ir_hw ir_rc5_cust;
static struct mtk_ir_hw *hw = &ir_rc5_cust;

static u32 _u4Routine_count;
static u32 _u4PrevKey = BTN_NONE;
static u32 _u4Rc5_customer_code = MTK_IR_RC5_CUSTOMER_CODE;
#define RC5_REPEAT_MS 200
#define RC5_REPEAT_MS_MIN 60

static struct rc_map_list mtk_rc5_map = {
	.map = {
		.scan = mtk_rc5_table,
		.size = ARRAY_SIZE(mtk_rc5_table),
		.rc_proto = RC_PROTO_RC5,
		.name = RC_MAP_MTK_RC5,
		}
};

static u32 mtk_ir_rc5_get_customer_code(void)
{
	return _u4Rc5_customer_code;
}

static u32 mtk_ir_rc5_set_customer_code(u32 data)
{
	_u4Rc5_customer_code = data;
	IR_RC5_LOG("_u4Rc5_customer_code = 0x%x\n", _u4Rc5_customer_code);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mtk_ir_rc5_early_suspend(void *preserve)
{
	IR_RC5_FUN();
}

static void mtk_ir_rc5_late_resume(void *preserve)
{
	IR_RC5_FUN();
}

#else

#define mtk_ir_rc5_early_suspend NULL
#define mtk_ir_rc5_late_resume NULL

#endif


#ifdef CONFIG_PM_SLEEP
static int mtk_ir_rc5_suspend(void *preserve)
{
	IR_RC5_FUN();
	IR_WRITE_MASK(IRRX_WAKECLR, IRRX_WAKECLR_MASK,
		IRRX_WAKECLR_OFFSET, 0x1);
	IR_WRITE_MASK(IRRX_WAKEEN, IRRX_WAKEEN_MASK,
		IRRX_WAKEEN_OFFSET, 0x1);
	IR_WRITE_MASK(IRRX_IREXPEN, IRRX_IREXPEN_MASK,
		IRRX_IREXPEN_OFFSET, 0x1);
	IR_WRITE_MASK(IRRX_IREXPEN, IRRX_BCEPEN_MASK,
		IRRX_BCEPEN_OFFSET, 0x1);
	IR_WRITE_MASK(IRRX_EXPBCNT, IRRX_EXP_BITCNT_MASK,
		IRRX_EXP_BITCNT_OFFSET, 0x14);
	IR_WRITE32(IRRX_ENEXP_IRM, MTK_RC5_EXP_IRM_BIT_MASK);
	IR_WRITE32(IRRX_ENEXP_IRL, MTK_RC5_EXP_IRL_BIT_MASK);
	IR_WRITE32(IRRX_EXP_IRM0, MTK_RC5_EXP_POWE_KEY1);
	IR_WRITE32(IRRX_EXP_IRL0, MTK_RC5_EXP_POWE_KEY2);
	return 0;
}

static int mtk_ir_rc5_resume(void *preserve)
{
	IR_RC5_FUN();
	IR_WRITE_MASK(IRRX_IREXPEN, IRRX_IREXPEN_MASK,
		IRRX_IREXPEN_OFFSET, 0x0);
	IR_WRITE_MASK(IRRX_IREXPEN, IRRX_BCEPEN_MASK,
		IRRX_BCEPEN_OFFSET, 0x0);
	IR_WRITE_MASK(IRRX_WAKECLR, IRRX_WAKECLR_MASK,
		IRRX_WAKECLR_OFFSET, 0x1);
	return 0;
}

#else

#define mtk_ir_rc5_suspend NULL
#define mtk_ir_rc5_resume NULL

#endif

static int mtk_ir_rc5_enable_hwirq(int enable)
{
	IR_RC5_LOG("IRRX enable hwirq: %d\n", enable);
	if (enable) {
		IR_WRITE_MASK(IRRX_IRCLR, IRRX_IRCLR_MASK,
			IRRX_IRCLR_OFFSET, 0x1);
		dsb(sy);
		IR_WRITE_MASK(IRRX_IRINT_CLR, IRRX_INTCLR_MASK,
			IRRX_INTCLR_OFFSET, 0x1);
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
 * This timer function is for RC5 routine work.
 * You can add stuff you want to do in this function.
 */
static int mtk_ir_rc5_timer_func(const char *data)
{
	u32 CHK_CNT_PULSE = 0;

	CHK_CNT_PULSE = (IR_READ32(IRRX_EXPBCNT) >> IRRX_IRCHK_CNT_OFFSET) &
		IRRX_IRCHK_CNT;
	if (CHK_CNT_PULSE == IRRX_IRCHK_CNT) {
		_u4Routine_count++;
		if (data[0]) {
			IR_RC5_ERR("CHK_CNT_PULSE = 0x%x\n", CHK_CNT_PULSE);
			IR_RC5_ERR("_u4Routine_count=%d\n", _u4Routine_count);
		}
		return mtk_ir_rc5_enable_hwirq(1);
	}
	return 0;
}

static struct mtk_ir_core_platform_data mtk_ir_pdata_rc5 = {
	.device_name = MTK_INPUT_DEVICE_NAME,
	.p_map_list = &mtk_rc5_map,
	.i4_keypress_timeout = MTK_IR_RC5_KEYPRESS_TIMEOUT,
	.enable_hwirq = mtk_ir_rc5_enable_hwirq,
	.init_hw = mtk_ir_rc5_init_hw,
	.uninit_hw = mtk_ir_rc5_uninit_hw,
	.ir_hw_decode = mtk_ir_rc5_decode,
	.get_customer_code = mtk_ir_rc5_get_customer_code,
	.set_customer_code = mtk_ir_rc5_set_customer_code,
	.timer_func = mtk_ir_rc5_timer_func,
	.mouse_step = {
		MOUSE_SMALL_X_STEP,
		MOUSE_SMALL_Y_STEP,
		MOUSE_LARGE_X_STEP,
		MOUSE_LARGE_Y_STEP
	},
#ifdef CONFIG_HAS_EARLYSUSPEND
	.early_suspend = mtk_ir_rc5_early_suspend,
	.late_resume = mtk_ir_rc5_late_resume,
#endif

#ifdef CONFIG_PM_SLEEP
	.suspend = mtk_ir_rc5_suspend,
	.resume = mtk_ir_rc5_resume,
#endif
};

static int mtk_ir_rc5_uninit_hw(void)
{
	mtk_ir_rc5_enable_hwirq(0);
	rc_map_unregister(&mtk_rc5_map);
	return 0;
}

static int mtk_ir_rc5_init_hw(void)
{
	enum MTK_IR_MODE ir_mode = mtk_ir_core_getmode();
	struct rc_map_table *p_table = NULL;
	int size = 0;
	int i = 0;

	IR_RC5_FUN();
	IR_RC5_LOG(" ir_mode = %d\n", ir_mode);

	if (ir_mode == MTK_IR_FACTORY) {
		mtk_rc5_map.map.scan = mtk_rc5_factory_table;
		mtk_rc5_map.map.size = ARRAY_SIZE(mtk_rc5_factory_table);
	} else {
		mtk_rc5_map.map.scan = mtk_rc5_table;
		mtk_rc5_map.map.size = ARRAY_SIZE(mtk_rc5_table);
		p_table = mtk_rc5_map.map.scan;
		size = mtk_rc5_map.map.size;

		memset(&(mtk_ir_pdata_rc5.mouse_code), 0xff,
			sizeof(mtk_ir_pdata_rc5.mouse_code));
		for (; i < size; i++) {
			if (p_table[i].keycode == KEY_LEFT)
				mtk_ir_pdata_rc5.mouse_code.scanleft =
					p_table[i].scancode;
			else if (p_table[i].keycode == KEY_RIGHT)
				mtk_ir_pdata_rc5.mouse_code.scanright =
					p_table[i].scancode;
			else if (p_table[i].keycode == KEY_UP)
				mtk_ir_pdata_rc5.mouse_code.scanup =
					p_table[i].scancode;
			else if (p_table[i].keycode == KEY_DOWN)
				mtk_ir_pdata_rc5.mouse_code.scandown =
					p_table[i].scancode;
			else if (p_table[i].keycode == KEY_ENTER)
				mtk_ir_pdata_rc5.mouse_code.scanenter =
					p_table[i].scancode;
		}
		mtk_ir_pdata_rc5.mouse_support = MTK_IR_SUPPORT_MOUSE_INPUT;
		mtk_ir_pdata_rc5.mouse_code.scanswitch =
			MTK_IR_MOUSE_RC5_SWITCH_CODE;
		mtk_ir_pdata_rc5.mousename[0] = '\0';
		strcat(mtk_ir_pdata_rc5.mousename,
			mtk_ir_pdata_rc5.device_name);
		strcat(mtk_ir_pdata_rc5.mousename, "_Mouse");
		mtk_ir_mouse_set_device_mode(MTK_IR_MOUSE_MODE_DEFAULT);
	}

	rc_map_register(&mtk_rc5_map);
	IR_RC5_LOG(" rc_map_register ok.\n");

	/* disable interrupt */
	mtk_ir_rc5_enable_hwirq(0);
	IR_WRITE32(IRRX_CONFIG_HIGH_REG, MTK_RC5_CONFIG);
	IR_WRITE32(IRRX_CONFIG_LOW_REG, MTK_RC5_SAPERIOD);
	IR_WRITE32(IRRX_THRESHOLD_REG, MTK_RC5_THRESHOLD);
	IR_RC5_LOG("config: 0x%08x %08x, threshold: 0x%08x\n",
		IR_READ32(IRRX_CONFIG_HIGH_REG), IR_READ32(IRRX_CONFIG_LOW_REG),
		IR_READ32(IRRX_THRESHOLD_REG));

	IR_RC5_LOG("%s----\n", __func__);
	return 0;
}

u32 mtk_ir_rc5_decode(void *preserve)
{
	u32 _au4IrRxData[2];
	u32 u4RC5BitCnt, u2RC5Toggle, u4RC5key;
	static unsigned long last_jiffers;
	unsigned long current_jiffers = jiffies;
	char *pu1Data = (char *)_au4IrRxData;
	u32 _u4Info = IR_READ32(IRRX_COUNT_HIGH_REG);

	_au4IrRxData[0] = IR_READ32(IRRX_COUNT_MID_REG);
	_au4IrRxData[1] = IR_READ32(IRRX_COUNT_LOW_REG);

	u4RC5BitCnt = RC5_INFO_TO_BITCNT(_u4Info);
	u2RC5Toggle = MTK_RC5_GET_TOGGLE(pu1Data[0]);
	u4RC5key = MTK_RC5_GET_KEYCODE(pu1Data[0], pu1Data[1]);

	if ((_au4IrRxData[0] != 0) ||
		(_au4IrRxData[1] != 0) ||
		(_u4Info != 0)) {
		IR_RC5_LOG("IRRX Info:0x%08x data:0x%08x %08x\n", _u4Info,
			   _au4IrRxData[1], _au4IrRxData[0]);
	} else {
		IR_RC5_ERR("invalid key!!!\n");
		return BTN_INVALID_KEY;
	}

	IR_RC5_LOG("Bitcnt: 0x%02x, Toggle: 0x%02x, Keycode; 0x%02x\n",
			 u4RC5BitCnt, u2RC5Toggle, u4RC5key);

	/* Check invalid pulse */
	if (u4RC5BitCnt != MTK_RC5_BITCNT) {
		IR_RC5_ERR("u4RC5BitCnt(%d), should be(%d)!!!\n",
			u4RC5BitCnt, MTK_RC5_BITCNT);
		_u4PrevKey = BTN_NONE;
		goto end;
	}

	if ((u2RC5Toggle != MTK_RC5_TOGGLE0) ||
		(u2RC5Toggle != MTK_RC5_TOGGLE1)) {
		IR_RC5_ERR("u2RC5Toggle(%d), should be(%d) or (%d)!!!\n",
			u2RC5Toggle, MTK_RC5_TOGGLE0, MTK_RC5_TOGGLE1);
		_u4PrevKey = BTN_NONE;
		goto end;
	}

	_u4PrevKey = u4RC5key;

end:
	IR_RC5_LOG("repeat_out=%dms\n", jiffies_to_msecs(current_jiffers -
		last_jiffers));
	last_jiffers = current_jiffers;

	IR_RC5_LOG("Key:0x%08x\n", _u4PrevKey);
	return _u4PrevKey;
}

static int ir_rc5_local_init(void)
{
	int err = 0;

	IR_RC5_FUN();
	err += mtk_ir_register_ctl_data_path(&mtk_ir_pdata_rc5, hw,
		RC_PROTO_RC5);
	IR_RC5_LOG("%s(%d)----\n", __func__, err);
	return err;
}

static int ir_rc5_remove(void)
{
	int err = 0;
	return err;
}

static struct mtk_ir_init_info ir_rc5_init_info = {
	.name = "ir_rc5",
	.init = ir_rc5_local_init,
	.uninit = ir_rc5_remove,
};

static int __init ir_rc5_init(void)
{
	IR_RC5_FUN();
	hw = get_mtk_ir_cus_dts(hw);
	if (hw == NULL)
		IR_RC5_ERR("get dts info fail\n");

	mtk_ir_driver_add(&ir_rc5_init_info);
	IR_RC5_LOG("%s----\n", __func__);
	return 0;
}

static void __exit ir_rc5_exit(void)
{
	IR_RC5_FUN();
}

module_init(ir_rc5_init);
module_exit(ir_rc5_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("mtk ir_rc5 driver");
MODULE_AUTHOR("Zhimin.Tang@mediatek.com");
