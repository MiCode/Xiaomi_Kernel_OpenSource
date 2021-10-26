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
#include "mtk_ir_common.h"
#include "mtk_ir_core.h"
#include "mtk_ir_regs.h"
#include "mtk_ir_cus_sirc.h"

#define IRRX_SIRC_BITCNT12 (u32)(0xc)
#define IRRX_SIRC_BITCNT15 (u32)(0xf)
#define IRRX_SIRC_BITCNT20 (u32)(0x14)
#define MTK_SIRC_INFO_TO_BITCNT(u4Info) \
	((u4Info & IRRX_CH_BITCNT_MASK) >> IRRX_CH_BITCNT_BITSFT)

static u32 _u4Sirc_customer_code_12bit = SIRC_CUSTOMER_12BIT;
static u32 _u4Sirc_customer_code_15bit = SIRC_CUSTOMER_15BIT;
static u32 _u4Sirc_customer_code_20bit = SIRC_CUSTOMER_20BIT;
static u32 _u4Sirc_customer_code_20bit_dual = SIRC_CUSTOMER_20BIT_DUAL;
static u32 _u4Sirc_customer_code_20bit_trible = SIRC_CUSTOMER_20BIT_TRIBLE;

#define IR_SIRC_DEBUG	1
#define IR_SIRC_TAG     "[IRRX_SIRC] "
#define IR_SIRC_FUN(f) \
	do { \
		if (ir_log_debug_on) \
			pr_err(IR_SIRC_TAG"%s++++\n", __func__); \
		else \
			pr_debug(IR_SIRC_TAG"%s++++\n", __func__); \
	} while (0)

#define IR_SIRC_LOG(fmt, args...) \
	do { \
		if (ir_log_debug_on) \
			pr_err(IR_SIRC_TAG fmt, ##args); \
		else if (IR_SIRC_DEBUG) \
			pr_debug(IR_SIRC_TAG fmt, ##args); \
	} while (0)

#define IR_SIRC_ERR(fmt, args...) \
	pr_err(MTK_IR_TAG"%s %d : "fmt, __func__, __LINE__, ##args)

static int mtk_ir_sirc_enable_hwirq(int enable);
static int mtk_ir_sirc_init_hw(void);
static int mtk_ir_sirc_uninit_hw(void);
static u32 mtk_ir_sirc_decode(void *preserve);
static char Reverse1Byte(char ucSrc);

struct mtk_ir_hw ir_sirc_cust;
static struct mtk_ir_hw *hw = &ir_sirc_cust;

static u32 _u4Routine_count;
static u32 _u4PrevKey = BTN_NONE;
#define SIRC_REPEAT_MS 200
#define SIRC_REPEAT_MS_MIN 60

static struct rc_map_list mtk_sirc_map = {
	.map = {
		.scan = mtk_sirc_table,
		.size = ARRAY_SIZE(mtk_sirc_table),
		.rc_proto = RC_PROTO_SONY20,
		.name = RC_MAP_MTK_SIRC,
		}
};

static char Reverse1Byte(char ucSrc)
{
	char ucRet = 0;
	int i;
	char ucTemp;

	for (i = 0; i < 8; i++) {
		ucTemp = 1<<i;
		if (ucSrc & ucTemp)
			ucRet += 1<<(7-i);
	}

	return ucRet;
}

static u32 mtk_ir_sirc_get_customer_code(void)
{
	IR_SIRC_LOG("_u4Sirc_customer_code_20 = 0x%x\n",
		_u4Sirc_customer_code_20bit);
	return _u4Sirc_customer_code_20bit;
}

static u32 mtk_ir_sirc_set_customer_code(u32 data)
{
	_u4Sirc_customer_code_20bit = data;
	IR_SIRC_LOG("_u4Sirc_customer_code_20 = 0x%x\n",
		_u4Sirc_customer_code_20bit);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mtk_ir_sirc_early_suspend(void *preserve)
{
	IR_SIRC_FUN();
}

static void mtk_ir_sirc_late_resume(void *preserve)
{
	IR_SIRC_FUN();
}

#else

#define mtk_ir_sirc_early_suspend NULL
#define mtk_ir_sirc_late_resume NULL

#endif


#ifdef CONFIG_PM_SLEEP
static int mtk_ir_sirc_suspend(void *preserve)
{
	IR_SIRC_FUN();
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
		IRRX_EXP_BITCNT_OFFSET, 0xC);
#ifdef USE_OLD_IRRX_CODA
	IR_WRITE_MASK(IRRX_PDWNOUT, IRRX_PDWNOUT_MASK,
		IRRX_PDWNOUT_OFFSET, 0x1);
#endif
	IR_WRITE32(IRRX_ENEXP_IRM, MTK_SIRC_EXP_IRM_BIT_MASK);
	IR_WRITE32(IRRX_ENEXP_IRL, MTK_SIRC_EXP_IRL_BIT_MASK);
	IR_WRITE32(IRRX_EXP_IRM0, MTK_SIRC_EXP_POWE_KEY1);
	IR_WRITE32(IRRX_EXP_IRL0, MTK_SIRC_EXP_POWE_KEY2);

	return 0;
}

static int mtk_ir_sirc_resume(void *preserve)
{
	IR_SIRC_FUN();
	IR_WRITE_MASK(IRRX_IREXPEN, IRRX_IREXPEN_MASK,
		IRRX_IREXPEN_OFFSET, 0x0);
	IR_WRITE_MASK(IRRX_IREXPEN, IRRX_BCEPEN_MASK,
		IRRX_BCEPEN_OFFSET, 0x0);
	IR_WRITE_MASK(IRRX_WAKEEN, IRRX_WAKEEN_MASK,
		IRRX_WAKEEN_OFFSET, 0x0);
#ifdef USE_OLD_IRRX_CODA
	IR_WRITE_MASK(IRRX_PDWNOUT, IRRX_PDWNOUT_MASK,
		IRRX_PDWNOUT_OFFSET, 0x0);
#endif
	IR_WRITE_MASK(IRRX_WAKECLR, IRRX_WAKECLR_MASK,
		IRRX_WAKECLR_OFFSET, 0x1);

	return 0;
}

#else

#define mtk_ir_sirc_suspend NULL
#define mtk_ir_sirc_resume NULL

#endif

static int mtk_ir_sirc_enable_hwirq(int enable)
{
	IR_SIRC_LOG("IRRX enable hwirq: %d\n", enable);
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
 * This timer function is for SIRC routine work.
 * You can add stuff you want to do in this function.
 */
static int mtk_ir_sirc_timer_func(const char *data)
{
	u32 CHK_CNT_PULSE = 0;

	CHK_CNT_PULSE = (IR_READ32(IRRX_EXPBCNT) >> IRRX_IRCHK_CNT_OFFSET) &
		IRRX_IRCHK_CNT;
	if (CHK_CNT_PULSE == IRRX_IRCHK_CNT) {
		_u4Routine_count++;
		if (data[0]) {
			IR_SIRC_ERR("CHK_CNT_PULSE = 0x%x\n", CHK_CNT_PULSE);
			IR_SIRC_ERR("_u4Routine_count=%d\n", _u4Routine_count);
		}
		return mtk_ir_sirc_enable_hwirq(1);
	}
	return 0;
}

static struct mtk_ir_core_platform_data mtk_ir_pdata_sirc = {
	.device_name = MTK_INPUT_DEVICE_NAME,
	.p_map_list = &mtk_sirc_map,
	.i4_keypress_timeout = MTK_IR_SIRC_KEYPRESS_TIMEOUT,
	.enable_hwirq = mtk_ir_sirc_enable_hwirq,
	.init_hw = mtk_ir_sirc_init_hw,
	.uninit_hw = mtk_ir_sirc_uninit_hw,
	.ir_hw_decode = mtk_ir_sirc_decode,
	.get_customer_code = mtk_ir_sirc_get_customer_code,
	.set_customer_code = mtk_ir_sirc_set_customer_code,
	.timer_func = mtk_ir_sirc_timer_func,
	.mouse_step = {
		MOUSE_SMALL_X_STEP,
		MOUSE_SMALL_Y_STEP,
		MOUSE_LARGE_X_STEP,
		MOUSE_LARGE_Y_STEP
	},
#ifdef CONFIG_HAS_EARLYSUSPEND
	.early_suspend = mtk_ir_sirc_early_suspend,
	.late_resume = mtk_ir_sirc_late_resume,
#endif

#ifdef CONFIG_PM_SLEEP
	.suspend = mtk_ir_sirc_suspend,
	.resume = mtk_ir_sirc_resume,
#endif
};

static int mtk_ir_sirc_uninit_hw(void)
{
	mtk_ir_sirc_enable_hwirq(0);
	rc_map_unregister(&mtk_sirc_map);
	return 0;
}

static int mtk_ir_sirc_init_hw(void)
{
	enum MTK_IR_MODE ir_mode = mtk_ir_core_getmode();
	struct rc_map_table *p_table = NULL;
	int size = 0;
	int i = 0;

	IR_SIRC_FUN();
	IR_SIRC_LOG(" ir_mode = %d\n", ir_mode);

	if (ir_mode == MTK_IR_FACTORY) {
		mtk_sirc_map.map.scan = mtk_sirc_factory_table;
		mtk_sirc_map.map.size = ARRAY_SIZE(mtk_sirc_factory_table);
	} else {
		mtk_sirc_map.map.scan = mtk_sirc_table;
		mtk_sirc_map.map.size = ARRAY_SIZE(mtk_sirc_table);
		p_table = mtk_sirc_map.map.scan;
		size = mtk_sirc_map.map.size;

		memset(&(mtk_ir_pdata_sirc.mouse_code), 0xff,
			sizeof(mtk_ir_pdata_sirc.mouse_code));
		for (; i < size; i++) {
			if (p_table[i].keycode == KEY_LEFT)
				mtk_ir_pdata_sirc.mouse_code.scanleft =
				p_table[i].scancode;
			else if (p_table[i].keycode == KEY_RIGHT)
				mtk_ir_pdata_sirc.mouse_code.scanright =
				p_table[i].scancode;
			else if (p_table[i].keycode == KEY_UP)
				mtk_ir_pdata_sirc.mouse_code.scanup =
				p_table[i].scancode;
			else if (p_table[i].keycode == KEY_DOWN)
				mtk_ir_pdata_sirc.mouse_code.scandown =
				p_table[i].scancode;
			else if (p_table[i].keycode == KEY_ENTER)
				mtk_ir_pdata_sirc.mouse_code.scanenter =
				p_table[i].scancode;
		}
		mtk_ir_pdata_sirc.mouse_support = MTK_IR_SUPPORT_MOUSE_INPUT;
		mtk_ir_pdata_sirc.mouse_code.scanswitch =
			MTK_IR_MOUSE_SIRC_SWITCH_CODE;
		mtk_ir_pdata_sirc.mousename[0] = '\0';
		strcat(mtk_ir_pdata_sirc.mousename,
			mtk_ir_pdata_sirc.device_name);
		strcat(mtk_ir_pdata_sirc.mousename, "_Mouse");
		mtk_ir_mouse_set_device_mode(MTK_IR_MOUSE_MODE_DEFAULT);
	}

	rc_map_register(&mtk_sirc_map);
	IR_SIRC_LOG(" rc_map_register ok.\n");

	/* disable interrupt */
	mtk_ir_sirc_enable_hwirq(0);
	IR_WRITE32(IRRX_CONFIG_HIGH_REG, MTK_SIRC_CONFIG);
	IR_WRITE32(IRRX_CONFIG_LOW_REG, MTK_SIRC_SAPERIOD);
	IR_WRITE32(IRRX_THRESHOLD_REG, MTK_SIRC_THRESHOLD);
	IR_SIRC_LOG(" config: 0x%08x %08x, threshold: 0x%08x\n",
		IR_READ32(IRRX_CONFIG_HIGH_REG), IR_READ32(IRRX_CONFIG_LOW_REG),
		IR_READ32(IRRX_THRESHOLD_REG));

	IR_SIRC_LOG("%s----\n", __func__);
	return 0;
}

u32 mtk_ir_sirc_decode(void *preserve)
{
	u32 _au4IrRxData[2];
	u32 u1Command, u1Device, u1Extended, sirc_key;
	u32 _u4Info = IR_READ32(IRRX_COUNT_HIGH_REG);
	u32 u4BitCnt = MTK_SIRC_INFO_TO_BITCNT(_u4Info);
	char *pu1Data = (char *)_au4IrRxData;
	static unsigned long last_jiffers;
	unsigned long current_jiffers = jiffies;

	_au4IrRxData[0] = IR_READ32(IRRX_COUNT_MID_REG);
	_au4IrRxData[1] = IR_READ32(IRRX_COUNT_LOW_REG);

	if ((_au4IrRxData[0] != 0) ||
		(_au4IrRxData[1] != 0) ||
		(_u4Info != 0)) {
		IR_SIRC_LOG("IRRX Info:0x%08x data:0x%08x %08x\n", _u4Info,
			   _au4IrRxData[1], _au4IrRxData[0]);
	} else {
		IR_SIRC_ERR("invalid key!!!\n");
		return BTN_INVALID_KEY;
	}

	switch (u4BitCnt) {
	case IRRX_SIRC_BITCNT12:
		u1Command = (pu1Data[0] >> 1);
		u1Command = Reverse1Byte(u1Command<<1);

		u1Device = ((pu1Data[0] & 0x01) << 4) |
			((pu1Data[1] & 0xF0) >> 4);
		u1Device = Reverse1Byte(u1Device<<3);
		sirc_key = SIRC_KEY_CODE(SIRC_LENGTH_12, u1Device, u1Command);
		_u4PrevKey = sirc_key;
		IR_SIRC_LOG("SIRC 12B: 0x%02x, Device = 0x%02x\n",
			u1Command, u1Device);
		IR_SIRC_LOG("SIRC 12B: sirc_key = 0x%08x\n", sirc_key);

		/* Check GroupId */
		if (u1Device != _u4Sirc_customer_code_12bit) {
			IR_SIRC_ERR("SIRC 12B :invalid customer code 0x%x!!!\n",
				u1Device);
			_u4PrevKey = BTN_NONE;
		}

		break;

	case IRRX_SIRC_BITCNT15:
		u1Command = (pu1Data[0] >> 1);
		u1Command = Reverse1Byte(u1Command<<1);

		u1Device = ((pu1Data[0] & 0x01) << 7) |
			((pu1Data[1] & 0xFE) >> 1);
		u1Device = Reverse1Byte(u1Device);
		sirc_key = SIRC_KEY_CODE(SIRC_LENGTH_15, u1Device, u1Command);
		_u4PrevKey = sirc_key;
		IR_SIRC_LOG("SIRC 15B: 0x%02x, u1Device = 0x%02x\n",
			u1Command, u1Device);
		IR_SIRC_LOG("SIRC 15B: sirc_key = 0x%08x\n", sirc_key);

		/* Check GroupId */
		if (u1Device != _u4Sirc_customer_code_15bit) {
			IR_SIRC_ERR("SIRC 15B: invalid customer code 0x%x!!!\n",
				u1Device);
			_u4PrevKey = BTN_NONE;
		}

		break;

	case IRRX_SIRC_BITCNT20:
		u1Command = (pu1Data[0] >> 1);
		u1Command = Reverse1Byte(u1Command<<1);

		u1Device = ((pu1Data[0] & 0x01) << 4) |
			((pu1Data[1] & 0xF0) >> 4);
		u1Device = Reverse1Byte(u1Device<<3);

		u1Extended = ((pu1Data[1] & 0x0F) << 4) |
			((pu1Data[2] & 0xF0) >> 4);
		u1Extended = Reverse1Byte(u1Extended);
		IR_SIRC_LOG("SIRC 20B: Device = 0x%x, u1Extended: 0x%x\n",
			u1Device, u1Extended);
		u1Device = ((u1Extended&0xff) << 5) + ((u1Device)&0x1f);
		sirc_key = SIRC_KEY_CODE(SIRC_LENGTH_20, u1Device, u1Command);
		_u4PrevKey = sirc_key;

		IR_SIRC_LOG("SIRC 20B: 0x%x, Device = 0x%x\n",
			u1Command, u1Device);
		IR_SIRC_LOG("SIRC 20B: sirc_key = 0x%08x\n", sirc_key);

		/* Check GroupId */
		if ((u1Device != _u4Sirc_customer_code_20bit) &&
			(u1Device != _u4Sirc_customer_code_20bit_dual) &&
			(u1Device != _u4Sirc_customer_code_20bit_trible)) {
			IR_SIRC_ERR("SIRC 20B: invalid customer code 0x%x!!!\n",
				u1Device);
			_u4PrevKey = BTN_NONE;
		}

		break;

	default:
		IR_SIRC_ERR("BITCNT unmatch:not sirc key\n");
		_u4PrevKey = BTN_NONE;
		break;
	}

	IR_SIRC_LOG("repeat_out = %dms\n", jiffies_to_msecs(current_jiffers -
		last_jiffers));
	last_jiffers = current_jiffers;

	MTK_IR_KEY_LOG("Key:0x%08x\n", _u4PrevKey);
	return _u4PrevKey;
}

static int ir_sirc_local_init(void)
{
	int err = 0;

	IR_SIRC_FUN();
	err += mtk_ir_register_ctl_data_path(&mtk_ir_pdata_sirc, hw,
		RC_PROTO_SONY20);
	IR_SIRC_LOG("%s(%d)----\n", __func__, err);
	return err;
}

static int ir_sirc_remove(void)
{
	int err = 0;
	/* err += mtk_ir_sirc_uninit_hw(); */
	return err;
}

static struct mtk_ir_init_info ir_sirc_init_info = {
	.name = "ir_sirc",
	.init = ir_sirc_local_init,
	.uninit = ir_sirc_remove,
};

static int __init ir_sirc_init(void)
{
	IR_SIRC_FUN();
	hw = get_mtk_ir_cus_dts(hw);
	if (hw == NULL)
		IR_SIRC_ERR("get dts info fail\n");

	mtk_ir_driver_add(&ir_sirc_init_info);
	IR_SIRC_LOG("%s----\n", __func__);
	return 0;
}

static void __exit ir_sirc_exit(void)
{
	IR_SIRC_FUN();
}

module_init(ir_sirc_init);
module_exit(ir_sirc_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("mtk ir_sirc driver");
MODULE_AUTHOR("Zhimin.Tang@mediatek.com");
