// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   AudDrv_devtree_parser.c
 *
 * Project:
 * --------
 *
 *
 * Description:
 * ------------
 *   AudDrv_devtree_parser
 *
 * Author:
 * -------
 *   Chipeng Chang (mtk02308)
 *
 *-----------------------------------------------------------------------------
 *
 *
 ******************************************************************************
 */
#include "mtk-auddrv-devtree-parser.h"

static int bAuddrv_Dev_Tree_Init;
static struct auddrv_i2s_attribute Auddrv_I2S_Setting[Auddrv_I2S_Num]
						     [Auddrv_I2S_Attribute_Num];
static struct auddrv_i2s_attribute Auddrv_CLK_Setting[Auddrv_Attribute_num];

static void Auddrv_Devtree_PinSet(void);

struct auddrv_i2s_attribute *GetI2SSetting(uint32_t I2S_Number,
					   uint32_t I2S_Setting)
{
	struct auddrv_i2s_attribute *ret = NULL;

	if ((I2S_Number < Auddrv_I2S_Num) &&
	    (I2S_Setting < Auddrv_I2S_Attribute_Num))
		ret = &Auddrv_I2S_Setting[I2S_Number][I2S_Setting];

	return ret;
}

void Auddrv_Devtree_Init(void)
{
	pr_debug("%s\n", __func__);
	if (bAuddrv_Dev_Tree_Init == false) {
		/* do some init routine */
		bAuddrv_Dev_Tree_Init = true;
		memset(&Auddrv_I2S_Setting[0][0], 0,
		       sizeof(struct auddrv_i2s_attribute) * Auddrv_I2S_Num *
			       Auddrv_I2S_Attribute_Num);
		memset(&Auddrv_CLK_Setting[0], 0,
		       sizeof(struct auddrv_i2s_attribute) *
			       Auddrv_Attribute_num);
		Auddrv_DevTree_I2S_Setting("mediatek,mt_soc_pcm_routing");
		Auddrv_Devtree_PinSet();
		Auddrv_Devtree_Dump();
	} else
		pr_debug("%s\n bAuddrv_Dev_Tree_Init = %d", __func__,
			 bAuddrv_Dev_Tree_Init);
}

static void I2S0ConfigParse(struct device_node *node)
{
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S0_CLKGPIO, 0,
		    &(Auddrv_I2S_Setting[Auddrv_I2S0_Setting]
					[Auddrv_I2S_Setting_bck]
						.Gpio_Number))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S0_CLKGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S0_CLKGPIO, 0,
		    &(Auddrv_I2S_Setting[Auddrv_I2S0_Setting]
					[Auddrv_I2S_Setting_bck]
						.Gpio_Mode))) {
		pr_debug("%s %s  not exist!!!\n", __func__,
			 AUDDRV_I2S0_CLKGPIO);
	}

	if (of_property_read_u32_index(
		    node, AUDDRV_I2S0_DATGPIO, 0,
		    &(Auddrv_I2S_Setting[Auddrv_I2S0_Setting]
					[Auddrv_I2S_Setting_D00]
						.Gpio_Number))) {
		pr_debug("%s %s  not exist!!!\n", __func__,
			 AUDDRV_I2S0_DATGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S0_DATGPIO, 1,
		    &(Auddrv_I2S_Setting[Auddrv_I2S0_Setting]
					[Auddrv_I2S_Setting_D00]
						.Gpio_Mode))) {
		pr_debug("%s %s  not exist!!!\n", __func__,
			 AUDDRV_I2S0_DATGPIO);
	}

	if (of_property_read_u32_index(
		    node, AUDDRV_I2S0_DATAINGPIO, 0,
		    &(Auddrv_I2S_Setting[Auddrv_I2S0_Setting]
					[Auddrv_I2S_Setting_I00]
						.Gpio_Number))) {
		pr_debug("%s %s  not exist!!!\n", __func__,
			 AUDDRV_I2S0_DATAINGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S0_DATAINGPIO, 1,
		    &(Auddrv_I2S_Setting[Auddrv_I2S0_Setting]
					[Auddrv_I2S_Setting_I00]
						.Gpio_Mode))) {
		pr_debug("%s %s  not exist!!!\n", __func__,
			 AUDDRV_I2S0_DATAINGPIO);
	}

	if (of_property_read_u32_index(
		    node, AUDDRV_I2S0_MCLKGPIO, 0,
		    &(Auddrv_I2S_Setting[Auddrv_I2S0_Setting]
					[Auddrv_I2S_Setting_Mclk]
						.Gpio_Number))) {
		pr_debug("%s %s  not exist!!!\n", __func__,
			 AUDDRV_I2S0_MCLKGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S0_MCLKGPIO, 1,
		    &(Auddrv_I2S_Setting[Auddrv_I2S0_Setting]
					[Auddrv_I2S_Setting_Mclk]
						.Gpio_Mode))) {
		pr_debug("%s %s  not exist!!!\n", __func__,
			 AUDDRV_I2S0_MCLKGPIO);
	}

	if (of_property_read_u32_index(
		    node, AUDDRV_I2S0_WSGPIO, 0,
		    &(Auddrv_I2S_Setting[Auddrv_I2S0_Setting]
					[Auddrv_I2S_Setting_ws]
						.Gpio_Number))) {
		pr_debug("%s %s  not exist!!!\n", __func__, AUDDRV_I2S0_WSGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S0_WSGPIO, 1,
		    &(Auddrv_I2S_Setting[Auddrv_I2S0_Setting]
					[Auddrv_I2S_Setting_ws]
						.Gpio_Mode))) {
		pr_debug("%s %s  not exist!!!\n", __func__, AUDDRV_I2S0_WSGPIO);
	}
}

static void I2S1ConfigParse(struct device_node *node)
{
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S1_CLKGPIO, 0,
		    &(Auddrv_I2S_Setting[Auddrv_I2S1_Setting]
					[Auddrv_I2S_Setting_bck]
						.Gpio_Number))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S1_CLKGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S1_CLKGPIO, 1,
		    &(Auddrv_I2S_Setting[Auddrv_I2S1_Setting]
					[Auddrv_I2S_Setting_bck]
						.Gpio_Mode))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S1_CLKGPIO);
	}

	if (of_property_read_u32_index(
		    node, AUDDRV_I2S1_DATGPIO, 0,
		    &(Auddrv_I2S_Setting[Auddrv_I2S1_Setting]
					[Auddrv_I2S_Setting_D00]
						.Gpio_Number))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S1_DATGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S1_DATGPIO, 1,
		    &(Auddrv_I2S_Setting[Auddrv_I2S1_Setting]
					[Auddrv_I2S_Setting_D00]
						.Gpio_Mode))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S1_DATGPIO);
	}

	if (of_property_read_u32_index(
		    node, AUDDRV_I2S1_MCLKGPIO, 0,
		    &(Auddrv_I2S_Setting[Auddrv_I2S1_Setting]
					[Auddrv_I2S_Setting_Mclk]
						.Gpio_Number))) {
		pr_debug("%s %s not exist!!!\n", __func__,
			 AUDDRV_I2S1_MCLKGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S1_MCLKGPIO, 1,
		    &(Auddrv_I2S_Setting[Auddrv_I2S1_Setting]
					[Auddrv_I2S_Setting_Mclk]
						.Gpio_Mode))) {
		pr_debug("%s %s not exist!!!\n", __func__,
			 AUDDRV_I2S1_MCLKGPIO);
	}

	if (of_property_read_u32_index(
		    node, AUDDRV_I2S1_WSGPIO, 0,
		    &(Auddrv_I2S_Setting[Auddrv_I2S1_Setting]
					[Auddrv_I2S_Setting_ws]
						.Gpio_Number))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S1_WSGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S1_WSGPIO, 1,
		    &(Auddrv_I2S_Setting[Auddrv_I2S1_Setting]
					[Auddrv_I2S_Setting_ws]
						.Gpio_Mode))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S1_WSGPIO);
	}
}

static void I2S2ConfigParse(struct device_node *node)
{
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S2_CLKGPIO, 0,
		    &(Auddrv_I2S_Setting[Auddrv_I2S2_Setting]
					[Auddrv_I2S_Setting_bck]
						.Gpio_Number))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S2_CLKGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S2_CLKGPIO, 1,
		    &(Auddrv_I2S_Setting[Auddrv_I2S2_Setting]
					[Auddrv_I2S_Setting_bck]
						.Gpio_Mode))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S2_CLKGPIO);
	}

	if (of_property_read_u32_index(
		    node, AUDDRV_I2S2_DATGPIO, 0,
		    &(Auddrv_I2S_Setting[Auddrv_I2S2_Setting]
					[Auddrv_I2S_Setting_D00]
						.Gpio_Number))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S2_DATGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S2_DATGPIO, 1,
		    &(Auddrv_I2S_Setting[Auddrv_I2S2_Setting]
					[Auddrv_I2S_Setting_D00]
						.Gpio_Mode))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S2_DATGPIO);
	}

	if (of_property_read_u32_index(
		    node, AUDDRV_I2S2_MCLKGPIO, 0,
		    &(Auddrv_I2S_Setting[Auddrv_I2S2_Setting]
					[Auddrv_I2S_Setting_Mclk]
						.Gpio_Number))) {
		pr_debug("%s %s not exist!!!\n", __func__,
			 AUDDRV_I2S2_MCLKGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S2_MCLKGPIO, 1,
		    &(Auddrv_I2S_Setting[Auddrv_I2S2_Setting]
					[Auddrv_I2S_Setting_Mclk]
						.Gpio_Mode))) {
		pr_debug("%s %s not exist!!!\n", __func__,
			 AUDDRV_I2S2_MCLKGPIO);
	}

	if (of_property_read_u32_index(
		    node, AUDDRV_I2S2_WSGPIO, 0,
		    &(Auddrv_I2S_Setting[Auddrv_I2S2_Setting]
					[Auddrv_I2S_Setting_ws]
						.Gpio_Number))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S2_WSGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S2_WSGPIO, 1,
		    &(Auddrv_I2S_Setting[Auddrv_I2S2_Setting]
					[Auddrv_I2S_Setting_ws]
						.Gpio_Mode))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S2_WSGPIO);
	}
}

static void I2S3ConfigParse(struct device_node *node)
{
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S3_CLKGPIO, 0,
		    &(Auddrv_I2S_Setting[Auddrv_I2S3_Setting]
					[Auddrv_I2S_Setting_bck]
						.Gpio_Number))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S3_CLKGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S3_CLKGPIO, 1,
		    &(Auddrv_I2S_Setting[Auddrv_I2S3_Setting]
					[Auddrv_I2S_Setting_bck]
						.Gpio_Mode))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S3_CLKGPIO);
	}

	if (of_property_read_u32_index(
		    node, AUDDRV_I2S3_DATGPIO, 0,
		    &(Auddrv_I2S_Setting[Auddrv_I2S3_Setting]
					[Auddrv_I2S_Setting_D00]
						.Gpio_Number))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S3_DATGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S3_DATGPIO, 1,
		    &(Auddrv_I2S_Setting[Auddrv_I2S3_Setting]
					[Auddrv_I2S_Setting_D00]
						.Gpio_Mode))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S3_DATGPIO);
	}

	if (of_property_read_u32_index(
		    node, AUDDRV_I2S3_MCLKGPIO, 0,
		    &(Auddrv_I2S_Setting[Auddrv_I2S3_Setting]
					[Auddrv_I2S_Setting_Mclk]
						.Gpio_Number))) {
		pr_debug("%s %s not exist!!!\n", __func__,
			 AUDDRV_I2S3_MCLKGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S3_MCLKGPIO, 1,
		    &(Auddrv_I2S_Setting[Auddrv_I2S3_Setting]
					[Auddrv_I2S_Setting_Mclk]
						.Gpio_Mode))) {
		pr_debug("%s %s not exist!!!\n", __func__,
			 AUDDRV_I2S3_MCLKGPIO);
	}

	if (of_property_read_u32_index(
		    node, AUDDRV_I2S3_WSGPIO, 0,
		    &(Auddrv_I2S_Setting[Auddrv_I2S3_Setting]
					[Auddrv_I2S_Setting_ws]
						.Gpio_Number))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S3_WSGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_I2S3_WSGPIO, 1,
		    &(Auddrv_I2S_Setting[Auddrv_I2S3_Setting]
					[Auddrv_I2S_Setting_ws]
						.Gpio_Mode))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_I2S3_WSGPIO);
	}
}

static void MtkInterfaceConfigParse(struct device_node *node)
{
	if (of_property_read_u32_index(
		    node, AUDDRV_AUD_CLKGPIO, 0,
		    &(Auddrv_CLK_Setting[Auddrv_CLK_Mosi].Gpio_Number))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_AUD_CLKGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_AUD_CLKGPIO, 1,
		    &(Auddrv_CLK_Setting[Auddrv_CLK_Mosi].Gpio_Mode))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_AUD_CLKGPIO);
	}

	if (of_property_read_u32_index(
		    node, AUDDRV_AUD_DATIGPIO, 0,
		    &(Auddrv_CLK_Setting[Auddrv_DataIn1_Mosi].Gpio_Number))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_AUD_DATIGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_AUD_DATIGPIO, 1,
		    &(Auddrv_CLK_Setting[Auddrv_DataIn1_Mosi].Gpio_Mode))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_AUD_DATIGPIO);
	}

	if (of_property_read_u32_index(
		    node, AUDDRV_AUD_DATOGPIO, 0,
		    &(Auddrv_CLK_Setting[Auddrv_DataOut1_Mosi].Gpio_Number))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_AUD_DATOGPIO);
	}
	if (of_property_read_u32_index(
		    node, AUDDRV_AUD_DATOGPIO, 1,
		    &(Auddrv_CLK_Setting[Auddrv_DataOut1_Mosi].Gpio_Mode))) {
		pr_debug("%s %s not exist!!!\n", __func__, AUDDRV_AUD_DATOGPIO);
	}
}

/* base on devtree name to pares dev tree. */
void Auddrv_DevTree_I2S_Setting(const char *DevTreeName)
{
	struct device_node *node = NULL;

	pr_debug("%s\n", __func__);
	node = of_find_compatible_node(NULL, NULL, DevTreeName);

	if (node != NULL) {
		I2S0ConfigParse(node);
		I2S1ConfigParse(node);
		I2S2ConfigParse(node);
		I2S3ConfigParse(node);
		MtkInterfaceConfigParse(node);
	}
}

/* base on devtree name to pares dev tree. */
static void Auddrv_Devtree_PinSet(void)
{
	int I2S_Num = 0;
	int I2S_Attribute_Num = 0;

	pr_debug("+%s\n", __func__);
	for (I2S_Num = 0; I2S_Num < Auddrv_I2S_Num; I2S_Num++) {
		for (I2S_Attribute_Num = 0;
		     I2S_Attribute_Num < Auddrv_I2S_Attribute_Num;
		     I2S_Attribute_Num++) {
			pr_debug(
				"Auddrv_I2S_Setting[%d][%d] gpio_num = %d gpio_mode = %d\n",
				I2S_Num, I2S_Attribute_Num,
				Auddrv_I2S_Setting[I2S_Num][I2S_Attribute_Num]
					.Gpio_Number,
				Auddrv_I2S_Setting[I2S_Num][I2S_Attribute_Num]
					.Gpio_Mode);
			if (Auddrv_I2S_Setting[I2S_Num][I2S_Attribute_Num]
				    .Gpio_Number) {
				Auddrv_I2S_Setting[I2S_Num][I2S_Attribute_Num]
					.Gpio_Number |= 0x80000000;
			}
		}
	}
	for (I2S_Attribute_Num = 0; I2S_Attribute_Num < Auddrv_Attribute_num;
	     I2S_Attribute_Num++) {
		pr_debug(
			"Auddrv_CLK_Setting[%d] gpio_num = %d gpio_mode = %d\n",
			I2S_Attribute_Num,
			Auddrv_CLK_Setting[I2S_Attribute_Num].Gpio_Number,
			Auddrv_CLK_Setting[I2S_Attribute_Num].Gpio_Mode);
		if (Auddrv_CLK_Setting[I2S_Attribute_Num].Gpio_Number)
			Auddrv_CLK_Setting[I2S_Attribute_Num].Gpio_Number |=
				0x80000000;
	}
	pr_debug("-%s\n", __func__);
}

/* base on devtree name to pares dev tree. */
void Auddrv_Devtree_Dump(void)
{
	int I2S_Num = 0;
	int I2S_Attribute_Num = 0;

	pr_debug("+%s\n", __func__);
	for (I2S_Num = 0; I2S_Num < Auddrv_I2S_Num; I2S_Num++) {
		for (I2S_Attribute_Num = 0;
		     I2S_Attribute_Num < Auddrv_I2S_Attribute_Num;
		     I2S_Attribute_Num++) {
			pr_debug(
				"Auddrv_I2S_Setting[%d][%d] gpio_num = %d gpio_mode = %d\n",
				I2S_Num, I2S_Attribute_Num,
				Auddrv_I2S_Setting[I2S_Num][I2S_Attribute_Num]
					.Gpio_Number,
				Auddrv_I2S_Setting[I2S_Num][I2S_Attribute_Num]
					.Gpio_Mode);
		}
	}
	for (I2S_Attribute_Num = 0; I2S_Attribute_Num < Auddrv_Attribute_num;
	     I2S_Attribute_Num++) {
		pr_debug(
			"Auddrv_CLK_Setting[%d] gpio_num = %d gpio_mode = %d\n",
			I2S_Attribute_Num,
			Auddrv_CLK_Setting[I2S_Attribute_Num].Gpio_Number,
			Auddrv_CLK_Setting[I2S_Attribute_Num].Gpio_Mode);
	}
	pr_debug("-%s\n", __func__);
}
