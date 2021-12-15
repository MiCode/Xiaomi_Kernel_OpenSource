// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */


#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <asoc/core.h>
#include <asoc/pdata.h>
#include "wcd9xxx-utils.h"
#include "wcd9335_registers.h"
#include "wcd9335_irq.h"
#include <asoc/wcd934x_registers.h>
#include "wcd934x/wcd934x_irq.h"

/* wcd9335 interrupt table  */
static const struct intr_data wcd9335_intr_table[] = {
	{WCD9XXX_IRQ_SLIMBUS, false},
	{WCD9335_IRQ_MBHC_SW_DET, true},
	{WCD9335_IRQ_MBHC_BUTTON_PRESS_DET, true},
	{WCD9335_IRQ_MBHC_BUTTON_RELEASE_DET, true},
	{WCD9335_IRQ_MBHC_ELECT_INS_REM_DET, true},
	{WCD9335_IRQ_MBHC_ELECT_INS_REM_LEG_DET, true},
	{WCD9335_IRQ_FLL_LOCK_LOSS, false},
	{WCD9335_IRQ_HPH_PA_CNPL_COMPLETE, false},
	{WCD9335_IRQ_HPH_PA_CNPR_COMPLETE, false},
	{WCD9335_IRQ_EAR_PA_CNP_COMPLETE, false},
	{WCD9335_IRQ_LINE_PA1_CNP_COMPLETE, false},
	{WCD9335_IRQ_LINE_PA2_CNP_COMPLETE, false},
	{WCD9335_IRQ_LINE_PA3_CNP_COMPLETE, false},
	{WCD9335_IRQ_LINE_PA4_CNP_COMPLETE, false},
	{WCD9335_IRQ_HPH_PA_OCPL_FAULT, false},
	{WCD9335_IRQ_HPH_PA_OCPR_FAULT, false},
	{WCD9335_IRQ_EAR_PA_OCP_FAULT, false},
	{WCD9335_IRQ_SOUNDWIRE, false},
	{WCD9335_IRQ_VDD_DIG_RAMP_COMPLETE, false},
	{WCD9335_IRQ_RCO_ERROR, false},
	{WCD9335_IRQ_SVA_ERROR, false},
	{WCD9335_IRQ_MAD_AUDIO, false},
	{WCD9335_IRQ_MAD_BEACON, false},
	{WCD9335_IRQ_SVA_OUTBOX1, true},
	{WCD9335_IRQ_SVA_OUTBOX2, true},
	{WCD9335_IRQ_MAD_ULTRASOUND, false},
	{WCD9335_IRQ_VBAT_ATTACK, false},
	{WCD9335_IRQ_VBAT_RESTORE, false},
};

static const struct intr_data wcd934x_intr_table[] = {
	{WCD9XXX_IRQ_SLIMBUS, false},
	{WCD934X_IRQ_MBHC_SW_DET, true},
	{WCD934X_IRQ_MBHC_BUTTON_PRESS_DET, true},
	{WCD934X_IRQ_MBHC_BUTTON_RELEASE_DET, true},
	{WCD934X_IRQ_MBHC_ELECT_INS_REM_DET, true},
	{WCD934X_IRQ_MBHC_ELECT_INS_REM_LEG_DET, true},
	{WCD934X_IRQ_MISC, false},
	{WCD934X_IRQ_HPH_PA_CNPL_COMPLETE, false},
	{WCD934X_IRQ_HPH_PA_CNPR_COMPLETE, false},
	{WCD934X_IRQ_EAR_PA_CNP_COMPLETE, false},
	{WCD934X_IRQ_LINE_PA1_CNP_COMPLETE, false},
	{WCD934X_IRQ_LINE_PA2_CNP_COMPLETE, false},
	{WCD934X_IRQ_SLNQ_ANALOG_ERROR, false},
	{WCD934X_IRQ_RESERVED_3, false},
	{WCD934X_IRQ_HPH_PA_OCPL_FAULT, false},
	{WCD934X_IRQ_HPH_PA_OCPR_FAULT, false},
	{WCD934X_IRQ_EAR_PA_OCP_FAULT, false},
	{WCD934X_IRQ_SOUNDWIRE, false},
	{WCD934X_IRQ_VDD_DIG_RAMP_COMPLETE, false},
	{WCD934X_IRQ_RCO_ERROR, false},
	{WCD934X_IRQ_CPE_ERROR, false},
	{WCD934X_IRQ_MAD_AUDIO, false},
	{WCD934X_IRQ_MAD_BEACON, false},
	{WCD934X_IRQ_CPE1_INTR, true},
	{WCD934X_IRQ_RESERVED_4, false},
	{WCD934X_IRQ_MAD_ULTRASOUND, false},
	{WCD934X_IRQ_VBAT_ATTACK, false},
	{WCD934X_IRQ_VBAT_RESTORE, false},
};

/*
 * wcd9335_bring_down: Bringdown WCD Codec
 *
 * @wcd9xxx: Pointer to wcd9xxx structure
 *
 * Returns 0 for success or negative error code for failure
 */
static int wcd9335_bring_down(struct wcd9xxx *wcd9xxx)
{
	if (!wcd9xxx || !wcd9xxx->regmap)
		return -EINVAL;

	regmap_write(wcd9xxx->regmap, WCD9335_CODEC_RPM_PWR_CDC_DIG_HM_CTL,
		     0x04);

	return 0;
}

/*
 * wcd9335_bring_up: Bringup WCD Codec
 *
 * @wcd9xxx: Pointer to the wcd9xxx structure
 *
 * Returns 0 for success or negative error code for failure
 */
static int wcd9335_bring_up(struct wcd9xxx *wcd9xxx)
{
	int ret = 0;
	int val, byte0;
	struct regmap *wcd_regmap;

	if (!wcd9xxx)
		return -EINVAL;

	if (!wcd9xxx->regmap) {
		dev_err(wcd9xxx->dev, "%s: wcd9xxx regmap is null!\n",
			__func__);
		return -EINVAL;
	}
	wcd_regmap = wcd9xxx->regmap;

	regmap_read(wcd_regmap, WCD9335_CHIP_TIER_CTRL_EFUSE_VAL_OUT0, &val);
	regmap_read(wcd_regmap, WCD9335_CHIP_TIER_CTRL_CHIP_ID_BYTE0, &byte0);

	if ((val < 0) || (byte0 < 0)) {
		dev_err(wcd9xxx->dev, "%s: tasha codec version detection fail!\n",
			__func__);
		return -EINVAL;
	}
	if ((val & 0x80) && (byte0 == 0x0)) {
		dev_info(wcd9xxx->dev, "%s: wcd9335 codec version is v1.1\n",
			 __func__);
		regmap_write(wcd_regmap, WCD9335_CODEC_RPM_RST_CTL, 0x01);
		regmap_write(wcd_regmap, WCD9335_SIDO_SIDO_CCL_2, 0xFC);
		regmap_write(wcd_regmap, WCD9335_SIDO_SIDO_CCL_4, 0x21);
		regmap_write(wcd_regmap, WCD9335_CODEC_RPM_PWR_CDC_DIG_HM_CTL,
			     0x5);
		regmap_write(wcd_regmap, WCD9335_CODEC_RPM_PWR_CDC_DIG_HM_CTL,
			     0x7);
		regmap_write(wcd_regmap, WCD9335_CODEC_RPM_PWR_CDC_DIG_HM_CTL,
			     0x3);
		regmap_write(wcd_regmap, WCD9335_CODEC_RPM_RST_CTL, 0x3);
	} else if (byte0 == 0x1) {
		dev_info(wcd9xxx->dev, "%s: wcd9335 codec version is v2.0\n",
			 __func__);
		regmap_write(wcd_regmap, WCD9335_CODEC_RPM_RST_CTL, 0x01);
		regmap_write(wcd_regmap, WCD9335_SIDO_SIDO_TEST_2, 0x00);
		regmap_write(wcd_regmap, WCD9335_SIDO_SIDO_CCL_8, 0x6F);
		regmap_write(wcd_regmap, WCD9335_BIAS_VBG_FINE_ADJ, 0x65);
		regmap_write(wcd_regmap, WCD9335_CODEC_RPM_PWR_CDC_DIG_HM_CTL,
			     0x5);
		regmap_write(wcd_regmap, WCD9335_CODEC_RPM_PWR_CDC_DIG_HM_CTL,
			     0x7);
		regmap_write(wcd_regmap, WCD9335_CODEC_RPM_PWR_CDC_DIG_HM_CTL,
			     0x3);
		regmap_write(wcd_regmap, WCD9335_CODEC_RPM_RST_CTL, 0x3);
	} else if ((byte0 == 0) && (!(val & 0x80))) {
		dev_info(wcd9xxx->dev, "%s: wcd9335 codec version is v1.0\n",
			 __func__);
		regmap_write(wcd_regmap, WCD9335_CODEC_RPM_RST_CTL, 0x01);
		regmap_write(wcd_regmap, WCD9335_SIDO_SIDO_CCL_2, 0xFC);
		regmap_write(wcd_regmap, WCD9335_SIDO_SIDO_CCL_4, 0x21);
		regmap_write(wcd_regmap, WCD9335_CODEC_RPM_PWR_CDC_DIG_HM_CTL,
			     0x3);
		regmap_write(wcd_regmap, WCD9335_CODEC_RPM_RST_CTL, 0x3);
	} else {
		dev_err(wcd9xxx->dev, "%s: tasha codec version unknown\n",
			__func__);
		ret = -EINVAL;
	}

	return ret;
}

/*
 * wcd9335_get_cdc_info: Get codec specific information
 *
 * @wcd9xxx: pointer to wcd9xxx structure
 * @wcd_type: pointer to wcd9xxx_codec_type structure
 *
 * Returns 0 for success or negative error code for failure
 */
static int wcd9335_get_cdc_info(struct wcd9xxx *wcd9xxx,
			   struct wcd9xxx_codec_type *wcd_type)
{
	u16 id_minor, id_major;
	struct regmap *wcd_regmap;
	int rc, val, version = 0;

	if (!wcd9xxx || !wcd_type)
		return -EINVAL;

	if (!wcd9xxx->regmap) {
		dev_err(wcd9xxx->dev, "%s: wcd9xxx regmap is null!\n",
			__func__);
		return -EINVAL;
	}
	wcd_regmap = wcd9xxx->regmap;

	rc = regmap_bulk_read(wcd_regmap, WCD9335_CHIP_TIER_CTRL_CHIP_ID_BYTE0,
			(u8 *)&id_minor, sizeof(u16));
	if (rc)
		return -EINVAL;

	rc = regmap_bulk_read(wcd_regmap, WCD9335_CHIP_TIER_CTRL_CHIP_ID_BYTE2,
			      (u8 *)&id_major, sizeof(u16));
	if (rc)
		return -EINVAL;

	dev_info(wcd9xxx->dev, "%s: wcd9xxx chip id major 0x%x, minor 0x%x\n",
		 __func__, id_major, id_minor);

	/* Version detection */
	if (id_major == TASHA_MAJOR) {
		regmap_read(wcd_regmap, WCD9335_CHIP_TIER_CTRL_EFUSE_VAL_OUT0,
			    &val);
		version = ((u8)val & 0x80) >> 7;
	} else if (id_major == TASHA2P0_MAJOR)
		version = 2;
	else
		dev_err(wcd9xxx->dev, "%s: wcd9335 version unknown (major 0x%x, minor 0x%x)\n",
			__func__, id_major, id_minor);

	/* Fill codec type info */
	wcd_type->id_major = id_major;
	wcd_type->id_minor = id_minor;
	wcd_type->num_irqs = WCD9335_NUM_IRQS;
	wcd_type->version = version;
	wcd_type->slim_slave_type = WCD9XXX_SLIM_SLAVE_ADDR_TYPE_1;
	wcd_type->i2c_chip_status = 0x01;
	wcd_type->intr_tbl = wcd9335_intr_table;
	wcd_type->intr_tbl_size = ARRAY_SIZE(wcd9335_intr_table);

	wcd_type->intr_reg[WCD9XXX_INTR_STATUS_BASE] =
						WCD9335_INTR_PIN1_STATUS0;
	wcd_type->intr_reg[WCD9XXX_INTR_CLEAR_BASE] =
						WCD9335_INTR_PIN1_CLEAR0;
	wcd_type->intr_reg[WCD9XXX_INTR_MASK_BASE] =
						WCD9335_INTR_PIN1_MASK0;
	wcd_type->intr_reg[WCD9XXX_INTR_LEVEL_BASE] =
						WCD9335_INTR_LEVEL0;
	wcd_type->intr_reg[WCD9XXX_INTR_CLR_COMMIT] =
						WCD9335_INTR_CLR_COMMIT;

	return rc;
}

/*
 * wcd934x_bring_down: Bringdown WCD Codec
 *
 * @wcd9xxx: Pointer to wcd9xxx structure
 *
 * Returns 0 for success or negative error code for failure
 */
static int wcd934x_bring_down(struct wcd9xxx *wcd9xxx)
{
	if (!wcd9xxx || !wcd9xxx->regmap)
		return -EINVAL;

	regmap_write(wcd9xxx->regmap, WCD934X_CODEC_RPM_PWR_CDC_DIG_HM_CTL,
		     0x04);

	return 0;
}

/*
 * wcd934x_bring_up: Bringup WCD Codec
 *
 * @wcd9xxx: Pointer to the wcd9xxx structure
 *
 * Returns 0 for success or negative error code for failure
 */
static int wcd934x_bring_up(struct wcd9xxx *wcd9xxx)
{
	struct regmap *wcd_regmap;

	if (!wcd9xxx)
		return -EINVAL;

	if (!wcd9xxx->regmap) {
		dev_err(wcd9xxx->dev, "%s: wcd9xxx regmap is null!\n",
			__func__);
		return -EINVAL;
	}
	wcd_regmap = wcd9xxx->regmap;

	regmap_write(wcd_regmap, WCD934X_CODEC_RPM_RST_CTL, 0x01);
	regmap_write(wcd_regmap, WCD934X_SIDO_NEW_VOUT_A_STARTUP, 0x19);
	regmap_write(wcd_regmap, WCD934X_SIDO_NEW_VOUT_D_STARTUP, 0x15);
	/* Add 1msec delay for VOUT to settle */
	usleep_range(1000, 1100);
	regmap_write(wcd_regmap, WCD934X_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x5);
	regmap_write(wcd_regmap, WCD934X_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x7);
	regmap_write(wcd_regmap, WCD934X_CODEC_RPM_RST_CTL, 0x3);
	regmap_write(wcd_regmap, WCD934X_CODEC_RPM_RST_CTL, 0x7);
	regmap_write(wcd_regmap, WCD934X_CODEC_RPM_PWR_CDC_DIG_HM_CTL, 0x3);

	return 0;
}

/*
 * wcd934x_get_cdc_info: Get codec specific information
 *
 * @wcd9xxx: pointer to wcd9xxx structure
 * @wcd_type: pointer to wcd9xxx_codec_type structure
 *
 * Returns 0 for success or negative error code for failure
 */
static int wcd934x_get_cdc_info(struct wcd9xxx *wcd9xxx,
			   struct wcd9xxx_codec_type *wcd_type)
{
	u16 id_minor, id_major;
	struct regmap *wcd_regmap;
	int rc, version = -1;

	if (!wcd9xxx || !wcd_type)
		return -EINVAL;

	if (!wcd9xxx->regmap) {
		dev_err(wcd9xxx->dev, "%s: wcd9xxx regmap is null\n", __func__);
		return -EINVAL;
	}
	wcd_regmap = wcd9xxx->regmap;

	rc = regmap_bulk_read(wcd_regmap, WCD934X_CHIP_TIER_CTRL_CHIP_ID_BYTE0,
			      (u8 *)&id_minor, sizeof(u16));
	if (rc)
		return -EINVAL;

	rc = regmap_bulk_read(wcd_regmap, WCD934X_CHIP_TIER_CTRL_CHIP_ID_BYTE2,
			      (u8 *)&id_major, sizeof(u16));
	if (rc)
		return -EINVAL;

	dev_info(wcd9xxx->dev, "%s: wcd9xxx chip id major 0x%x, minor 0x%x\n",
		 __func__, id_major, id_minor);

	if (id_major != TAVIL_MAJOR)
		goto version_unknown;

	/*
	 * As fine version info cannot be retrieved before tavil probe.
	 * Assign coarse versions for possible future use before tavil probe.
	 */
	if (id_minor == cpu_to_le16(0))
		version = TAVIL_VERSION_1_0;
	else if (id_minor == cpu_to_le16(0x01))
		version = TAVIL_VERSION_1_1;

version_unknown:
	if (version < 0)
		dev_err(wcd9xxx->dev, "%s: wcd934x version unknown\n",
			__func__);

	/* Fill codec type info */
	wcd_type->id_major = id_major;
	wcd_type->id_minor = id_minor;
	wcd_type->num_irqs = WCD934X_NUM_IRQS;
	wcd_type->version = version;
	wcd_type->slim_slave_type = WCD9XXX_SLIM_SLAVE_ADDR_TYPE_1;
	wcd_type->i2c_chip_status = 0x01;
	wcd_type->intr_tbl = wcd934x_intr_table;
	wcd_type->intr_tbl_size = ARRAY_SIZE(wcd934x_intr_table);

	wcd_type->intr_reg[WCD9XXX_INTR_STATUS_BASE] =
						WCD934X_INTR_PIN1_STATUS0;
	wcd_type->intr_reg[WCD9XXX_INTR_CLEAR_BASE] =
						WCD934X_INTR_PIN1_CLEAR0;
	wcd_type->intr_reg[WCD9XXX_INTR_MASK_BASE] =
						WCD934X_INTR_PIN1_MASK0;
	wcd_type->intr_reg[WCD9XXX_INTR_LEVEL_BASE] =
						WCD934X_INTR_LEVEL0;
	wcd_type->intr_reg[WCD9XXX_INTR_CLR_COMMIT] =
						WCD934X_INTR_CLR_COMMIT;

	return rc;
}

codec_bringdown_fn wcd9xxx_bringdown_fn(int type)
{
	codec_bringdown_fn cdc_bdown_fn;

	switch (type) {
	case WCD934X:
		cdc_bdown_fn = wcd934x_bring_down;
		break;
	case WCD9335:
		cdc_bdown_fn = wcd9335_bring_down;
		break;
	default:
		cdc_bdown_fn = NULL;
		break;
	}

	return cdc_bdown_fn;
}

codec_bringup_fn wcd9xxx_bringup_fn(int type)
{
	codec_bringup_fn cdc_bup_fn;

	switch (type) {
	case WCD934X:
		cdc_bup_fn = wcd934x_bring_up;
		break;
	case WCD9335:
		cdc_bup_fn = wcd9335_bring_up;
		break;
	default:
		cdc_bup_fn = NULL;
		break;
	}

	return cdc_bup_fn;
}

codec_type_fn wcd9xxx_get_codec_info_fn(int type)
{
	codec_type_fn cdc_type_fn;

	switch (type) {
	case WCD934X:
		cdc_type_fn = wcd934x_get_cdc_info;
		break;
	case WCD9335:
		cdc_type_fn = wcd9335_get_cdc_info;
		break;
	default:
		cdc_type_fn = NULL;
		break;
	}

	return cdc_type_fn;
}

