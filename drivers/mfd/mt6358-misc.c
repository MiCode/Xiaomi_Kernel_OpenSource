/*
 * Copyright (C) 2018 MediaTek, Inc.
 * Author: Wilma Wu <wilma.wu@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/sched/clock.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/irqdomain.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/io.h>
#include <linux/mfd/mt6358/core.h>
#include <linux/mfd/mt6358/rtc_misc.h>
#if defined(CONFIG_MTK_PMIC_CHIP_MT6358)
#include <linux/mfd/mt6358/registers.h>
#elif defined(CONFIG_MTK_PMIC_CHIP_MT6359)
#include <linux/mfd/mt6359/registers.h>
#elif defined(CONFIG_MTK_PMIC_CHIP_MT6359P)
#include <linux/mfd/mt6359p/registers.h>
#endif


#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

/* we map HW YEA 0 (2000) to 1968 not 1970 because 2000 is the leap year */
#define RTC_MIN_YEAR		1968

/*
 * Reset to default date if RTC time is over 2038/1/19 3:14:7
 * Year (YEA)        : 1970 ~ 2037
 * Month (MTH)       : 1 ~ 12
 * Day of Month (DOM): 1 ~ 31
 */

#define RTC_DEFAULT_YEA		2010
#define RTC_DEFAULT_MTH		1
#define RTC_DEFAULT_DOM		1

/* Min, Hour, Dom... register offset to RTC_TC_SEC */
#define RTC_OFFSET_SEC		0
#define RTC_OFFSET_MIN		1
#define RTC_OFFSET_HOUR		2
#define RTC_OFFSET_DOM		3
#define RTC_OFFSET_DOW		4
#define RTC_OFFSET_MTH		5
#define RTC_OFFSET_YEAR		6
#define RTC_OFFSET_COUNT	7



#define RTC_DSN_ID				0x580
#define RTC_BBPU				0x8
#define RTC_IRQ_STA				0xa
#define RTC_IRQ_EN				0xc
#define RTC_AL_MASK				0x10
#define RTC_TC_SEC				0x12
#define RTC_AL_SEC				0x20
#define RTC_AL_MIN				0x22
#define RTC_AL_HOU				0x24
#define RTC_AL_DOM				0x26
#define RTC_AL_DOW				0x28
#define RTC_AL_MTH				0x2a
#define RTC_AL_YEA				0x2c
#define RTC_PDN1				0x34
#define RTC_PDN2				0x36
#define RTC_SPAR0				0x38
#define RTC_SPAR1				0x3a
#define RTC_WRTGR				0x42
#define RTC_OSC32CON			0x2e
#define RTC_POWERKEY1			0x30
#define RTC_POWERKEY2			0x32
#define RTC_PROT				0x3c
#define RTC_CON					0x44

#define RTC_AL_SEC_MASK			0x3f
#define RTC_AL_MIN_MASK			0x3f
#define RTC_AL_HOU_MASK			0x1f
#define RTC_AL_DOM_MASK			0x1f
#define RTC_AL_DOW_MASK			0x7
#define RTC_AL_MTH_MASK			0xf
#define RTC_AL_YEA_MASK			0x7f

#define RTC_PWRON_SEC_MASK		0x3f
#define RTC_PWRON_MIN_MASK		0x3f
#define RTC_PWRON_HOU_MASK		0x7c0
#define RTC_PWRON_DOM_MASK		0xf800
#define RTC_PWRON_MTH_MASK		0xf
#define RTC_PWRON_YEA_MASK		0x7f00
#define RTC_PWRON_SEC_SHIFT		0x0
#define RTC_PWRON_MIN_SHIFT		0x0
#define RTC_PWRON_HOU_SHIFT		0x6
#define RTC_PWRON_DOM_SHIFT		0xb
#define RTC_PWRON_MTH_SHIFT		0x0
#define RTC_PWRON_YEA_SHIFT		0x8

#define	RTC_IRQ_EN_AL			BIT(0)

#define RTC_BBPU_KEY			0x4300
#define RTC_BBPU_CBUSY			BIT(6)
#define RTC_BBPU_RELOAD			BIT(5)
#define RTC_BBPU_CLR			BIT(1)
#define RTC_BBPU_PWREN			BIT(0)

#define RTC_AL_MASK_DOW			BIT(4)

#define RTC_GPIO_USER_MASK		0x1f00
#define RTC_PDN1_PWRON_TIME		BIT(7)
#define RTC_PDN2_PWRON_LOGO		BIT(15)


#define RTC_CON_F32KOB			BIT(5)

#define RTC_OSC32CON_UNLOCK1	0x1a57
#define RTC_OSC32CON_UNLOCK2	0x2b68
#define RTC_EMBCK_SRC_SEL		BIT(8)

#define RTC_K_EOSC_RSV_0		BIT(8)
#define RTC_K_EOSC_RSV_1		BIT(9)
#define RTC_K_EOSC_RSV_2		BIT(10)

#define RTC_BBPU_2SEC_EN		BIT(8)
#define RTC_BBPU_AUTO_PDN_SEL	BIT(6)

#define RTC_RG_EOSC_CALI_TD_MASK	7
#define RTC_RG_EOSC_CALI_TD_SHIFT	5

#define IPIMB


enum rtc_spare_enum {
	RTC_FGSOC = 0,
	RTC_ANDROID,
	RTC_FAC_RESET,
	RTC_BYPASS_PWR,
	RTC_PWRON_TIME,
	RTC_FAST_BOOT,
	RTC_KPOC,
	RTC_DEBUG,
	RTC_PWRON_AL,
	RTC_UART,
	RTC_AUTOBOOT,
	RTC_PWRON_LOGO,
	RTC_32K_LESS,
	RTC_LP_DET,
	RTC_FG_INIT,
	RTC_SPAR_NUM
};

enum rtc_reg_set {
	RTC_REG,
	RTC_MASK,
	RTC_SHIFT
};

u16 rtc_spare_reg[RTC_SPAR_NUM][3] = {
	{RTC_AL_MTH, 0xff, 8},
	{RTC_PDN1, 0xf, 0},
	{RTC_PDN1, 0x3, 4},
	{RTC_PDN1, 0x1, 6},
	{RTC_PDN1, 0x1, 7},
	{RTC_PDN1, 0x1, 13},
	{RTC_PDN1, 0x1, 14},
	{RTC_PDN1, 0x1, 15},
	{RTC_PDN2, 0x1, 4},
	{RTC_PDN2, 0x3, 5},
	{RTC_PDN2, 0x1, 7},
	{RTC_PDN2, 0x1, 15},
	{RTC_SPAR0, 0x1, 6},
	{RTC_SPAR0, 0x1, 7},
	{RTC_AL_HOU, 0xff, 8}
};

/*
 * RTC_PDN1:
 *     bit 0 - 3  : Android bits
 *     bit 4 - 5  : Recovery bits (0x10: factory data reset)
 *     bit 6      : Bypass PWRKEY bit
 *     bit 7      : Power-On Time bit
 *     bit 8      : RTC_GPIO_USER_WIFI bit
 *     bit 9      : RTC_GPIO_USER_GPS bit
 *     bit 10     : RTC_GPIO_USER_BT bit
 *     bit 11     : RTC_GPIO_USER_FM bit
 *     bit 12     : RTC_GPIO_USER_PMIC bit
 *     bit 13     : Fast Boot
 *     bit 14	  : Kernel Power Off Charging
 *     bit 15     : Debug bit
 */

/*
 * RTC_PDN2:
 *     bit 0 - 3 : MTH in power-on time
 *     bit 4     : Power-On Alarm bit
 *     bit 5 - 6 : UART bits
 *     bit 7     : POWER DROP AUTO BOOT bit
 *     bit 8 - 14: YEA in power-on time
 *     bit 15    : Power-On Logo bit
 */

/*
 * RTC_SPAR0:
 *     bit 0 - 5 : SEC in power-on time
 *     bit 6	 : 32K less bit. True:with 32K, False:Without 32K
 *     bit 7     : Low power detected in preloader
 *     bit 8 - 15: reserved bits
 */

/*
 * RTC_SPAR1:
 *     bit 0 - 5  : MIN in power-on time
 *     bit 6 - 10 : HOU in power-on time
 *     bit 11 - 15: DOM in power-on time
 */


/*
 * RTC_NEW_SPARE0: RTC_AL_HOU bit8~15
 *	   bit 8 ~ 14 : Fuel Gauge
 *     bit 15     : reserved bits
 */

/*
 * RTC_NEW_SPARE1: RTC_AL_DOM bit8~15
 *	   bit 8 ~ 15 : reserved bits
 */

/*
 * RTC_NEW_SPARE2: RTC_AL_DOW bit8~15
 *	   bit 8 ~ 15 : reserved bits
 */

/*
 * RTC_NEW_SPARE3: RTC_AL_MTH bit8~15
 *	   bit 8 ~ 15 : reserved bits
 */

static u16 rtc_alarm_reg[RTC_OFFSET_COUNT][3] = {
	{RTC_AL_SEC, RTC_AL_SEC_MASK, 0},
	{RTC_AL_MIN, RTC_AL_MIN_MASK, 0},
	{RTC_AL_HOU, RTC_AL_HOU_MASK, 0},
	{RTC_AL_DOM, RTC_AL_DOM_MASK, 0},
	{RTC_AL_DOW, RTC_AL_DOW_MASK, 0},
	{RTC_AL_MTH, RTC_AL_MTH_MASK, 0},
	{RTC_AL_YEA, RTC_AL_YEA_MASK, 0},
};

static u16 rtc_pwron_reg[RTC_OFFSET_COUNT][3] = {
	{RTC_SPAR0, RTC_PWRON_SEC_MASK, RTC_PWRON_SEC_SHIFT},
	{RTC_SPAR1, RTC_PWRON_MIN_MASK, RTC_PWRON_MIN_SHIFT},
	{RTC_SPAR1, RTC_PWRON_HOU_MASK, RTC_PWRON_HOU_SHIFT},
	{RTC_SPAR1, RTC_PWRON_DOM_MASK, RTC_PWRON_DOM_SHIFT},
	{0, 0, 0},
	{RTC_PDN2, RTC_PWRON_MTH_MASK, RTC_PWRON_MTH_SHIFT},
	{RTC_PDN2, RTC_PWRON_YEA_MASK, RTC_PWRON_YEA_SHIFT},
};

struct mtk_rtc_compat_data {
	void (*enable_eosc_cali)(void);
};

struct mt6358_misc {
	struct device		*dev;
	spinlock_t	lock;
	struct regmap		*regmap;
	u32			addr_base;
	const struct mtk_rtc_compat_data	*variant;
};

static struct mt6358_misc *rtc_misc;
#ifdef IPIMB
struct regmap *pmic_regmap;
#endif

static void mtk_rtc_enable_k_eosc_revised(void);

static const struct mtk_rtc_compat_data mt6359p_cdata = {
	.enable_eosc_cali = mtk_rtc_enable_k_eosc_revised,
};

static int rtc_eosc_cali_td;
static int dcxo_switch;
module_param(rtc_eosc_cali_td, int, 0664);


static int rtc_read(unsigned int reg, unsigned int *val)
{
	return regmap_read(rtc_misc->regmap, rtc_misc->addr_base + reg, val);
}

static int rtc_write(unsigned int reg, unsigned int val)
{
	return regmap_write(rtc_misc->regmap, rtc_misc->addr_base + reg, val);
}

static int rtc_update_bits(unsigned int reg,
		       unsigned int mask, unsigned int val)
{
	return regmap_update_bits(rtc_misc->regmap,
			rtc_misc->addr_base + reg, mask, val);
}

static int rtc_field_read(unsigned int reg,
		       unsigned int mask, unsigned int shift, unsigned int *val)
{
	int ret;
	unsigned int reg_val = 0;

	ret = rtc_read(reg, &reg_val);
	if (ret != 0)
		return ret;

	reg_val &= mask;
	reg_val >>= shift;
	*val = reg_val;

	return ret;
}

static int rtc_busy_wait(void)
{
	unsigned long long timeout = sched_clock() + 500000000;
	int ret;
	unsigned int bbpu = 0;
	u32 pwrkey1 = 0, pwrkey2 = 0, sec = 0;

	do {
		ret = rtc_read(RTC_BBPU, &bbpu);
		if (ret < 0)
			break;
		if ((bbpu & RTC_BBPU_CBUSY) == 0)
			break;
		else if (sched_clock() > timeout) {
			rtc_read(RTC_BBPU, &bbpu);
			rtc_read(RTC_POWERKEY1, &pwrkey1);
			rtc_read(RTC_POWERKEY2, &pwrkey2);
			rtc_read(RTC_TC_SEC, &sec);
			pr_err("%s, wait cbusy timeout, %x, %x, %x, %d\n",
				__func__, bbpu, pwrkey1, pwrkey2, sec);
			ret = -ETIMEDOUT;
			break;
		}
	} while (1);

	return ret;
}

static int rtc_write_trigger(void)
{
	int ret;

	ret = rtc_write(RTC_WRTGR, 1);
	if (ret < 0)
		return ret;

	return rtc_busy_wait();
}

static int mtk_rtc_get_spare_register(enum rtc_spare_enum cmd)
{
	unsigned int tmp_val = 0;
	int ret = -EINVAL;

	if (cmd >= 0 && cmd < RTC_SPAR_NUM) {

		ret = rtc_field_read(rtc_spare_reg[cmd][RTC_REG],
				rtc_spare_reg[cmd][RTC_MASK]
					<< rtc_spare_reg[cmd][RTC_SHIFT],
				rtc_spare_reg[cmd][RTC_SHIFT],
				&tmp_val);

		if (ret < 0)
			goto exit;

		pr_notice("%s: cmd[%d], get rg[0x%x, 0x%x , %d] = 0x%x\n",
			      __func__, cmd,
				  rtc_spare_reg[cmd][RTC_REG],
			      rtc_spare_reg[cmd][RTC_MASK],
			      rtc_spare_reg[cmd][RTC_SHIFT], tmp_val);

		return tmp_val;
	}

exit:
	return ret;
}

static void mtk_rtc_set_spare_register(enum rtc_spare_enum cmd, u16 val)
{
	u32 tmp_val = 0;
	int ret;

	if (cmd >= 0 && cmd < RTC_SPAR_NUM) {

		pr_notice("%s: cmd[%d], set rg[0x%x, 0x%x , %d] = 0x%x\n",
				  __func__, cmd,
			      rtc_spare_reg[cmd][RTC_REG],
			      rtc_spare_reg[cmd][RTC_MASK],
			      rtc_spare_reg[cmd][RTC_SHIFT], val);

		tmp_val = ((val & rtc_spare_reg[cmd][RTC_MASK])
					<< rtc_spare_reg[cmd][RTC_SHIFT]);
		ret = rtc_update_bits(rtc_spare_reg[cmd][RTC_REG],
				rtc_spare_reg[cmd][RTC_MASK]
					<< rtc_spare_reg[cmd][RTC_SHIFT],
				tmp_val);
		if (ret < 0)
			goto exit;
		ret = rtc_write_trigger();
		if (ret < 0)
			goto exit;
	}
	return;
exit:
	pr_err("%s error\n", __func__);
}

int get_rtc_spare_fg_value(void)
{
	/* RTC_AL_HOU bit8~14 */
	u16 temp;
	unsigned long flags;

	spin_lock_irqsave(&rtc_misc->lock, flags);
	temp = mtk_rtc_get_spare_register(RTC_FGSOC);
	spin_unlock_irqrestore(&rtc_misc->lock, flags);

	return temp;
}

int set_rtc_spare_fg_value(int val)
{
	/* RTC_AL_HOU bit8~14 */
	unsigned long flags;

	spin_lock_irqsave(&rtc_misc->lock, flags);
	mtk_rtc_set_spare_register(RTC_FGSOC, val);
	spin_unlock_irqrestore(&rtc_misc->lock, flags);

	return 0;
}

int get_rtc_spare0_fg_value(void)
{
	u16 temp;
	unsigned long flags;

	spin_lock_irqsave(&rtc_misc->lock, flags);
	temp = mtk_rtc_get_spare_register(RTC_FG_INIT);
	spin_unlock_irqrestore(&rtc_misc->lock, flags);

	return temp;
}

int set_rtc_spare0_fg_value(int val)
{
	unsigned long flags;

	spin_lock_irqsave(&rtc_misc->lock, flags);
	mtk_rtc_set_spare_register(RTC_FG_INIT, val);
	spin_unlock_irqrestore(&rtc_misc->lock, flags);

	return 0;
}

bool crystal_exist_status(void)
{
	unsigned long flags;
	u16 ret;

	spin_lock_irqsave(&rtc_misc->lock, flags);
	ret = mtk_rtc_get_spare_register(RTC_32K_LESS);
	spin_unlock_irqrestore(&rtc_misc->lock, flags);

	if (ret)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(crystal_exist_status);


static void mtk_rtc_set_gpio_32k_status(u16 user, bool enable)
{
	unsigned int pdn1 = 0;
	int ret;

	if (enable)
		pdn1 = (1U << user);
	ret = rtc_update_bits(RTC_PDN1, (1U << user), pdn1);
	if (ret < 0)
		goto exit;
	ret = rtc_write_trigger();
	if (ret < 0)
		goto exit;

	if (enable) {
		ret = rtc_update_bits(RTC_CON, RTC_CON_F32KOB, 0);
		if (ret < 0)
			goto exit;
	} else {
		ret = rtc_field_read(RTC_PDN1,
				RTC_GPIO_USER_MASK, RTC_GPIO_USER_WIFI, &pdn1);
		if (ret < 0)
			goto exit;
		/* disable 32K export if there are no RTC_GPIO users */
		if (!pdn1) {
			ret = rtc_update_bits(RTC_CON,
						RTC_CON_F32KOB,	RTC_CON_F32KOB);
			if (ret < 0)
				goto exit;
		}
	}

	pr_notice("RTC_GPIO user %d enable = %d 32k (0x%x)\n",
		user, enable, pdn1);
	return;

exit:
	pr_err("%s error\n", __func__);
}

void rtc_gpio_enable_32k(enum rtc_gpio_user_t user)
{
	unsigned long flags;

	pr_notice("%s: user = %d\n", __func__, user);

	if (user < RTC_GPIO_USER_WIFI || user > RTC_GPIO_USER_PMIC)
		return;

	spin_lock_irqsave(&rtc_misc->lock, flags);
	mtk_rtc_set_gpio_32k_status(user, true);
	spin_unlock_irqrestore(&rtc_misc->lock, flags);
}
EXPORT_SYMBOL(rtc_gpio_enable_32k);

void rtc_gpio_disable_32k(enum rtc_gpio_user_t user)
{
	unsigned long flags;

	pr_notice("%s: user = %d\n", __func__, user);

	if (user < RTC_GPIO_USER_WIFI || user > RTC_GPIO_USER_PMIC)
		return;

	spin_lock_irqsave(&rtc_misc->lock, flags);
	mtk_rtc_set_gpio_32k_status(user, false);
	spin_unlock_irqrestore(&rtc_misc->lock, flags);
}
EXPORT_SYMBOL(rtc_gpio_disable_32k);

static void mtk_rtc_clear_pwron_alarm(void)
{
	u16 data[RTC_OFFSET_COUNT];
	int ret, i;

	pr_err("%s\n", __func__);

	data[RTC_OFFSET_SEC] = 0;
	data[RTC_OFFSET_MIN] = 0;
	data[RTC_OFFSET_HOUR] = 0;
	data[RTC_OFFSET_DOM] =
		((RTC_DEFAULT_DOM << RTC_PWRON_DOM_SHIFT) & RTC_PWRON_DOM_MASK);
	data[RTC_OFFSET_MTH] =
		((RTC_DEFAULT_MTH << RTC_PWRON_MTH_SHIFT) & RTC_PWRON_MTH_MASK);
	data[RTC_OFFSET_YEAR] =
		(((RTC_DEFAULT_YEA - RTC_MIN_YEAR) << RTC_PWRON_YEA_SHIFT)
		& RTC_PWRON_YEA_MASK);

	for (i = RTC_OFFSET_SEC; i < RTC_OFFSET_COUNT; i++) {
		if (i == RTC_OFFSET_DOW)
			continue;
		ret = rtc_update_bits(rtc_pwron_reg[i][RTC_REG],
					rtc_pwron_reg[i][RTC_MASK], data[i]);
		if (ret < 0)
			goto exit;
		ret = rtc_write_trigger();
		if (ret < 0)
			goto exit;
	}

	ret = rtc_update_bits(RTC_PDN1, RTC_PDN1_PWRON_TIME, 0);
	if (ret < 0)
		goto exit;
	ret = rtc_update_bits(RTC_PDN2, RTC_PDN2_PWRON_LOGO, 0);
	if (ret < 0)
		goto exit;

	ret = rtc_write_trigger();
	if (ret < 0)
		goto exit;

	return;

exit:
	pr_err("%s error\n", __func__);
}

static void mtk_rtc_clear_alarm(void)
{
	unsigned int irqsta;
	u16 data[RTC_OFFSET_COUNT];
	int i, ret;

	ret = rtc_update_bits(RTC_IRQ_EN, RTC_IRQ_EN_AL, 0);
	if (ret < 0)
		goto exit;
	ret = rtc_write_trigger();
	if (ret < 0)
		goto exit;

	ret = rtc_read(RTC_IRQ_STA, &irqsta);	/* read clear */
	if (ret < 0)
		goto exit;

	data[RTC_OFFSET_SEC] = 0;
	data[RTC_OFFSET_MIN] = 0;
	data[RTC_OFFSET_HOUR] = 0;
	data[RTC_OFFSET_DOM] = RTC_DEFAULT_DOM & RTC_AL_DOM_MASK;
	data[RTC_OFFSET_MTH] = RTC_DEFAULT_MTH & RTC_AL_MTH_MASK;
	data[RTC_OFFSET_YEAR] =
		((RTC_DEFAULT_YEA - RTC_MIN_YEAR) & RTC_AL_YEA_MASK);

	for (i = RTC_OFFSET_SEC; i < RTC_OFFSET_COUNT; i++) {
		if (i == RTC_OFFSET_DOW)
			continue;
		ret = rtc_update_bits(rtc_alarm_reg[i][RTC_REG],
					rtc_alarm_reg[i][RTC_MASK], data[i]);
		if (ret < 0)
			goto exit;
	}

	ret = rtc_write_trigger();
	if (ret < 0)
		goto exit;

	return;
exit:
	pr_err("%s error\n", __func__);
}

void rtc_mark_recovery(void)
{
	unsigned long flags;

	pr_notice("%s\n", __func__);

	spin_lock_irqsave(&rtc_misc->lock, flags);
	mtk_rtc_set_spare_register(RTC_FAC_RESET, 0x1);
	/* Clear alarm setting when doing factory recovery. */
	mtk_rtc_clear_pwron_alarm();
	mtk_rtc_clear_alarm();
	spin_unlock_irqrestore(&rtc_misc->lock, flags);
}

void rtc_mark_kpoc(void)
{
	unsigned long flags;

	pr_notice("%s\n", __func__);

	spin_lock_irqsave(&rtc_misc->lock, flags);
	mtk_rtc_set_spare_register(RTC_KPOC, 0x1);
	spin_unlock_irqrestore(&rtc_misc->lock, flags);
}

void rtc_mark_fast(void)
{
	unsigned long flags;

	pr_notice("%s\n", __func__);

	spin_lock_irqsave(&rtc_misc->lock, flags);
	mtk_rtc_set_spare_register(RTC_FAST_BOOT, 0x1);
	spin_unlock_irqrestore(&rtc_misc->lock, flags);
}

static int pmic_config_interface(unsigned int RegNum, unsigned int val,
			    unsigned int MASK, unsigned int SHIFT)
{
	int ret = 0;

#ifdef IPIMB
	ret = regmap_update_bits(pmic_regmap, RegNum,
				(MASK << SHIFT), (val << SHIFT));
#else
	ret = regmap_update_bits(rtc_misc->regmap, RegNum,
				(MASK << SHIFT), (val << SHIFT));
#endif
	if (ret) {
		pr_notice("[%s]ret=%d Reg=0x%x val=0x%x MASK=0x%x SHIFT=%d\n",
			__func__, ret, RegNum, val, MASK, SHIFT);
		return ret;
	}

	return ret;
}

static void mtk_rtc_enable_k_eosc_revised(void)
{
	u32 td;
	int ret;

	pr_notice("%s\n", __func__);

	/* Truning on eosc cali mode clock */
	pmic_config_interface(PMIC_SCK_TOP_CKPDN_CON0_CLR_ADDR, 1,
					PMIC_RG_RTC_EOSC32_CK_PDN_MASK,
					PMIC_RG_RTC_EOSC32_CK_PDN_SHIFT);

	if (rtc_eosc_cali_td) {
		pr_notice("%s: rtc_eosc_cali_td = %d\n",
						__func__, rtc_eosc_cali_td);
		switch (rtc_eosc_cali_td) {
		case 1:
			td = 0x3;
			break;
		case 2:
			td = 0x4;
			break;
		case 4:
			td = 0x5;
			break;
		case 16:
			td = 0x7;
			break;
		default:
			td = 0x6;
			break;
		}
		ret = rtc_update_bits(RTC_AL_DOW,
			(RTC_RG_EOSC_CALI_TD_MASK << RTC_RG_EOSC_CALI_TD_SHIFT),
			(td << RTC_RG_EOSC_CALI_TD_SHIFT));
		if (ret < 0)
			goto exit;
		ret = rtc_write_trigger();
		if (ret < 0)
			goto exit;
	}
	return;
exit:
	pr_err("%s error\n", __func__);
}

static void mtk_rtc_enable_k_eosc(void)
{
	pr_notice("%s\n", __func__);

	/* Truning on eosc cali mode clock */
	pmic_config_interface(PMIC_SCK_TOP_CKPDN_CON0_CLR_ADDR, 1,
					PMIC_RG_RTC_EOSC32_CK_PDN_MASK,
					PMIC_RG_RTC_EOSC32_CK_PDN_SHIFT);

	if (rtc_eosc_cali_td) {
		pr_notice("%s: rtc_eosc_cali_td = %d\n",
						__func__, rtc_eosc_cali_td);
		switch (rtc_eosc_cali_td) {
		case 1:
			pmic_config_interface(PMIC_EOSC_CALI_TD_ADDR, 0x3,
						PMIC_EOSC_CALI_TD_MASK,
						PMIC_EOSC_CALI_TD_SHIFT);
			break;
		case 2:
			pmic_config_interface(PMIC_EOSC_CALI_TD_ADDR, 0x4,
						PMIC_EOSC_CALI_TD_MASK,
						PMIC_EOSC_CALI_TD_SHIFT);
			break;
		case 4:
			pmic_config_interface(PMIC_EOSC_CALI_TD_ADDR, 0x5,
						PMIC_EOSC_CALI_TD_MASK,
						PMIC_EOSC_CALI_TD_SHIFT);
			break;
		case 16:
			pmic_config_interface(PMIC_EOSC_CALI_TD_ADDR, 0x7,
						PMIC_EOSC_CALI_TD_MASK,
						PMIC_EOSC_CALI_TD_SHIFT);
			break;
		default:
			pmic_config_interface(PMIC_EOSC_CALI_TD_ADDR, 0x6,
						PMIC_EOSC_CALI_TD_MASK,
						PMIC_EOSC_CALI_TD_SHIFT);
			break;
		}
	}
	/*
	 * Switch the DCXO from 32k-less mode to RTC mode,
	 * otherwise, EOSC cali will fail
	 * RTC mode will have only OFF mode and FPM
	 */
	if (dcxo_switch) {
		pr_notice("%s: dcxo_switch\n", __func__);
		pmic_config_interface(PMIC_XO_EN32K_MAN_ADDR, 0,
				     PMIC_XO_EN32K_MAN_MASK,
					 PMIC_XO_EN32K_MAN_SHIFT);
	}
}

static void mtk_rtc_spar_alarm_clear_wait(void)
{
	unsigned long long timeout = sched_clock() + 500000000;
	u32 bbpu = 0;
	int ret;

	do {
		ret = rtc_read(RTC_BBPU, &bbpu);
		if (ret < 0)
			break;
		if ((bbpu & RTC_BBPU_CLR) == 0)
			break;
		else if (sched_clock() > timeout) {
			pr_err("%s, spar/alarm clear time out,\n", __func__);
			break;
		}
	} while (1);
}

void mt_power_off(void)
{
	unsigned long flags;
	u32 pdn1 = 0, al_mask = 0, irq_en = 0;
	int ret;

	pr_notice("%s\n", __func__);
	dump_stack();

	spin_lock_irqsave(&rtc_misc->lock, flags);

	ret = rtc_field_read(RTC_PDN1,
			RTC_GPIO_USER_MASK, RTC_GPIO_USER_WIFI, &pdn1);
	if (ret < 0)
		goto exit;
	/* disable 32K export if there are no RTC_GPIO users */
	if (!pdn1) {
		ret = rtc_update_bits(RTC_CON, RTC_CON_F32KOB, RTC_CON_F32KOB);
		if (ret < 0)
			goto exit;
		ret = rtc_write_trigger();
		if (ret < 0)
			goto exit;
	}

	/* lpsd */
	pr_notice("clear lpsd solution\n");
	ret = rtc_write(RTC_BBPU, RTC_BBPU_KEY | RTC_BBPU_CLR | RTC_BBPU_PWREN);
	if (ret < 0)
		goto exit;
	ret = rtc_write(RTC_AL_MASK, RTC_AL_MASK_DOW);	/* mask DOW */
	if (ret < 0)
		goto exit;
	ret = rtc_write_trigger();
	if (ret < 0)
		goto exit;
	mtk_rtc_spar_alarm_clear_wait();

	ret = rtc_update_bits(RTC_BBPU,
					(RTC_BBPU_KEY | RTC_BBPU_RELOAD),
					(RTC_BBPU_KEY | RTC_BBPU_RELOAD));
	if (ret < 0)
		goto exit;
	ret = rtc_write_trigger();
	if (ret < 0)
		goto exit;

	ret = rtc_read(RTC_AL_MASK, &al_mask);
	if (ret < 0)
		goto exit;
	ret = rtc_read(RTC_IRQ_EN, &irq_en);
	if (ret < 0)
		goto exit;
	pr_notice("%s: RTC_AL_MASK= 0x%x RTC_IRQ_EN= 0x%x\n",
				__func__, al_mask, irq_en);

	spin_unlock_irqrestore(&rtc_misc->lock, flags);

	//wk_pmic_enable_sdn_delay();
	pmic_config_interface(PMIC_TMA_KEY_ADDR, 0x9CA7,
			PMIC_TMA_KEY_MASK, PMIC_TMA_KEY_SHIFT);
	pmic_config_interface(PMIC_RG_SDN_DLY_ENB_ADDR, 0,
			PMIC_RG_SDN_DLY_ENB_MASK, PMIC_RG_SDN_DLY_ENB_SHIFT);
	pmic_config_interface(PMIC_TMA_KEY_ADDR, 0,
			PMIC_TMA_KEY_MASK, PMIC_TMA_KEY_SHIFT);


	pmic_config_interface(PMIC_RG_PWRHOLD_ADDR, 0,
			PMIC_RG_PWRHOLD_MASK, PMIC_RG_PWRHOLD_SHIFT);

	return;

exit:
	spin_unlock_irqrestore(&rtc_misc->lock, flags);
	pr_err("%s error\n", __func__);

}

static void mtk_rtc_lpsd_restore_al_mask(void)
{
	int ret;
	u32 val = 0;

	ret = rtc_update_bits(RTC_BBPU,
					(RTC_BBPU_KEY | RTC_BBPU_RELOAD),
					(RTC_BBPU_KEY | RTC_BBPU_RELOAD));
	if (ret < 0)
		goto exit;
	ret = rtc_write_trigger();
	if (ret < 0)
		goto exit;

	ret = rtc_read(RTC_AL_MASK, &val);
	if (ret < 0)
		goto exit;
	pr_notice("%s: 1st RTC_AL_MASK = 0x%x\n", __func__, val);

	/* mask DOW */
	ret = rtc_write(RTC_AL_MASK, RTC_AL_MASK_DOW);
	if (ret < 0)
		goto exit;
	ret = rtc_write_trigger();
	if (ret < 0)
		goto exit;

	ret = rtc_read(RTC_AL_MASK, &val);
	if (ret < 0)
		goto exit;
	pr_notice("%s: 2nd RTC_AL_MASK = 0x%x\n", __func__, val);

	return;
exit:
	pr_err("%s error\n", __func__);
}

static void mt6358_misc_shutdown(struct platform_device *pdev)
{
	if (rtc_misc->variant && rtc_misc->variant->enable_eosc_cali)
		rtc_misc->variant->enable_eosc_cali();
	else
		mtk_rtc_enable_k_eosc();
}

static int mt6358_misc_probe(struct platform_device *pdev)
{
	struct mt6358_chip *mt6358_chip = dev_get_drvdata(pdev->dev.parent);
	struct mt6358_misc *misc;
	unsigned long flags;

	misc = devm_kzalloc(&pdev->dev, sizeof(struct mt6358_misc), GFP_KERNEL);
	if (!misc)
		return -ENOMEM;

#ifdef IPIMB
	pmic_regmap = mt6358_chip->regmap;
	misc->regmap = dev_get_regmap(pdev->dev.parent->parent, NULL);
#else
	misc->regmap = mt6358_chip->regmap;
#endif
	if (!misc->regmap) {
		pr_notice("get regmap failed\n");
		return -ENODEV;
	}

	misc->dev = &pdev->dev;
	spin_lock_init(&misc->lock);
	rtc_misc = misc;
	platform_set_drvdata(pdev, misc);

	misc->variant =
	(struct mtk_rtc_compat_data *)of_device_get_match_data(misc->dev);
	if (misc->variant)
		dev_err(misc->dev, "no match data\n");

	if (of_property_read_u32(pdev->dev.of_node, "base",
							&rtc_misc->addr_base))
		rtc_misc->addr_base = RTC_DSN_ID;
	pr_notice("%s: rtc_misc->addr_base =0x%x\n",
				__func__, rtc_misc->addr_base);

	if (of_property_read_bool(pdev->dev.of_node, "apply-lpsd-solution")) {
		spin_lock_irqsave(&misc->lock, flags);
		mtk_rtc_lpsd_restore_al_mask();
		spin_unlock_irqrestore(&misc->lock, flags);

		pm_power_off = mt_power_off;
	}

	if (of_property_read_bool(pdev->dev.of_node, "dcxo-switch"))
		dcxo_switch = 1;

	pr_notice("%s done\n", __func__);

	return 0;
}

static const struct of_device_id mt6358_misc_of_match[] = {
	{ .compatible = "mediatek,mt6358-misc", },
	{ .compatible = "mediatek,mt6359-misc", },
	{ .compatible = "mediatek,mt6359p-misc", .data = &mt6359p_cdata},
	{ }
};
MODULE_DEVICE_TABLE(of, mt6358_misc_of_match);

static struct platform_driver mt6358_misc_driver = {
	.driver = {
		.name = "mt6358-misc",
		.of_match_table = mt6358_misc_of_match,
	},
	.probe = mt6358_misc_probe,
	.shutdown = mt6358_misc_shutdown,
};

module_platform_driver(mt6358_misc_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Wilma Wu <wilma.wu@mediatek.com>");
MODULE_DESCRIPTION("Misc Driver for MediaTek MT6358 PMIC");
