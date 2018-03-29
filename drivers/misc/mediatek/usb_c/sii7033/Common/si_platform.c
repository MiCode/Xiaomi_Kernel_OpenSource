/*
*Copyright (C) 2013-2014 Silicon Image, Inc.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation version 2.
* This program is distributed AS-IS WITHOUT ANY WARRANTY of any
* kind, whether express or implied; INCLUDING without the implied warranty
* of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
* See the GNU General Public License for more details at
* http://www.gnu.org/licenses/gpl-2.0.html.
*/
#include <Wrap.h>
#include "si_usbpd_main.h"

#include <linux/of_irq.h>
#include <typec.h>

void sii_platform_block_write8(const uint8_t addr, const uint8_t *p_data, uint16_t size)
{
	int ret = 0;

	ret = sii_write_i2c_block(addr, size, p_data);
	if (ret < 0)
		pr_debug("I2C_Write_block failed");

	/*Note: Customer can implement their error
	   handling. Below is an example */
}

void sii_platform_block_read8(const uint8_t addr, uint8_t *p_data, uint16_t size)
{
	int ret = 0;
	int i = 0;
	uint8_t tmp_addr = addr;
	uint8_t *tmp_buf = p_data;

	for (i = 0; i < size; i++) {
		ret = sii_read_i2c_block(tmp_addr, 1, tmp_buf);
		tmp_addr++;
		tmp_buf++;
		if (ret < 0) {
			pr_err("I2C_Read_block failed");
			return;
		}
	}
#if 0
	ret = sii_read_i2c_block(addr, size, p_data);
	if (ret < 0)
		pr_debug("I2C_Read_block failed");
#endif
}

void sii_platform_wr_reg8(const uint8_t addr, const uint8_t val)
{
	sii_platform_block_write8(addr, &val, 1);
}

uint8_t sii_platform_rd_reg8(const uint8_t addr)
{
	uint8_t val = 0;

	sii_platform_block_read8(addr, &val, 1);
	return val;
}

void sii_platform_set_bit8(const uint8_t addr, const uint8_t mask)
{
	uint8_t val;

	val = sii_platform_rd_reg8(addr);
	/*pr_debug("reading:  %X writing: ",val); */
	val = (val & ((uint8_t) ~mask)) | mask;
	/*pr_debug("%X\n",val); */
	sii_platform_wr_reg8(addr, val);
}

void sii_platform_clr_bit8(const uint8_t addr, const uint8_t mask)
{
	uint8_t val;

	val = sii_platform_rd_reg8(addr);
	val = (val & ((uint8_t) ~mask));
	sii_platform_wr_reg8(addr, val);
}

void sii_platform_put_bit8(const uint8_t addr, const uint8_t mask, const uint8_t val)
{
	uint8_t temp;
	uint8_t write_val;

	temp = sii_platform_rd_reg8(addr);
	temp = (temp & ((uint8_t) ~mask));
	write_val = temp | (mask & val);
	sii_platform_wr_reg8(addr, write_val);
}

void sii_platform_fifo_read8(const uint8_t addr, uint8_t *p_data, uint16_t size)
{
	uint16_t i;

	for (i = 0; i < size; i++) {
		p_data[i] = sii_platform_rd_reg8(addr);
		;
	}
}

void sii_platform_fifo_write8(const uint8_t addr, const uint8_t *p_data, uint16_t size)
{
	uint16_t i;

	for (i = 0; i < size; i++) {
		sii_platform_wr_reg8(addr, *(p_data + i));
		;
	}
}

void sii_platform_read_70xx_gpio(struct sii70xx_drv_context *drv_context)
{
	uint8_t gpio_status = 0;

	gpio_status = gpio_get_value(drv_context->sii_gpio[VBUS_SRC].gpio);
	pr_info("VBUS SOURCE STATUS = %x\n", gpio_status);

	gpio_status = gpio_get_value(drv_context->sii_gpio[VBUS_SNK].gpio);
	pr_info("VBUS SINK STATUS = %x\n", gpio_status);
}

void sii_platform_vbus_control(struct sii70xx_drv_context *drv_context, uint8_t vbus_type)
{
	sii_platform_vbus_gpio_control(drv_context, vbus_type);
}

void sii_platform_vbus_gpio_control(struct sii70xx_drv_context *drv_context, uint8_t vbus_type)
{
#if 0
	if (vbus_type == VBUS_SRC) {
		gpio_set_value(drv_context->sii_gpio[VBUS_SNK].gpio, 0);
		gpio_set_value(drv_context->sii_gpio[VBUS_SRC].gpio, 1);
		pr_info("\n GPIO control source\n");
	} else if (vbus_type == VBUS_SNK) {
		gpio_set_value(drv_context->sii_gpio[VBUS_SRC].gpio, 0);
		gpio_set_value(drv_context->sii_gpio[VBUS_SNK].gpio, 1);
		pr_info("\n GPIO control sink\n");
	} else {
		gpio_set_value(drv_context->sii_gpio[VBUS_SRC].gpio, 0);
		gpio_set_value(drv_context->sii_gpio[VBUS_SNK].gpio, 0);
		pr_info("\n GPIO control default\n");
	}
#endif
}

void sii70xx_platform_reset(struct sii70xx_drv_context *drv_context)
{
	struct usbtypc *typec;

	typec = g_exttypec;
	pinctrl_select_state(typec->pinctrl, typec->pin_cfg->sii7033_rst_low);
	msleep(20);
	pinctrl_select_state(typec->pinctrl, typec->pin_cfg->sii7033_rst_high);

	pr_err("rst_n=0x%x, out=%d\n", 251, gpio_get_value(251));
#if 0
	/*TODO... */
	int retval = 0;
	int rst_n = GPIO_SIL7033_RST_PIN;

	retval |= mt_set_gpio_mode(rst_n, GPIO_MODE_GPIO);
	retval |= mt_set_gpio_dir(rst_n, GPIO_DIR_OUT);
	retval |= mt_set_gpio_out(rst_n, GPIO_OUT_ZERO);

	msleep(20);

	retval |= mt_set_gpio_out(rst_n, GPIO_OUT_ONE);

	pr_err("rst_n=0x%x, mode=%d, out=%d\n", rst_n,
	       mt_get_gpio_mode(rst_n), mt_get_gpio_out(rst_n));
#endif
#if 0
	gpio_set_value(drv_context->sii_gpio[RESET_CTRL].gpio, 0);
	msleep(20);
	gpio_set_value(drv_context->sii_gpio[RESET_CTRL].gpio, 1);
#endif
	pr_info("Platform Reset\n");
}

void SiiPlatformDebugAssert(const char *pFileName, uint32_t lineNumber,
			    uint32_t expressionEvaluation, const char *pConditionText)
{
	pr_info("Asserting with failure in %s Line %d\n", pFileName, (int)lineNumber);
	for (;;) {
		;
	};			/* Note: just an example */
}
