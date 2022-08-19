/*
 * leds-aw22xxx.c   aw22xxx led module
 *
 * Copyright (c) 2017 AWINIC Technology CO., LTD
 *
 *  Author: Nick Li <liweilei@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/leds.h>
#include <linux/leds-aw22xxx.h>
#include <linux/leds-aw22xxx-reg.h>
#include <linux/regulator/consumer.h>
#include <linux/string.h>
/******************************************************
 *
 * Marco
 *
 ******************************************************/
#define AW22XXX_I2C_NAME "aw22xxx_led"

#define AW22XXX_DRIVER_VERSION "v1.2.0"

#define AW_I2C_RETRIES              2
#define AW_I2C_RETRY_DELAY          1
#define AW_READ_CHIPID_RETRIES      2
#define AW_READ_CHIPID_RETRY_DELAY  1


/******************************************************
 *
 * aw22xxx led parameter
 *
 ******************************************************/
#define AW22XXX_CFG_NAME_MAX        64
static char *aw22xxx_fw_name = "aw22xxx_fw.bin";
static char aw22xxx_cfg_name[][AW22XXX_CFG_NAME_MAX] = {
	{"aw22xxx_cfg_led_off.bin"},
	{"aw22xxx_cfg_led_on.bin"},
	{"aw22xxx_cfg_led_breath.bin"},
	{"aw22xxx_cfg_led_collision.bin"},
	{"aw22xxx_cfg_led_skyline.bin"},
	{"aw22xxx_cfg_led_flower.bin"},
	{"aw22xxx_cfg_audio_skyline.bin"},
	{"aw22xxx_cfg_audio_flower.bin"},
	{"aw22xxx_cfg_led_red_13.bin"},
	{"aw22xxx_cfg_led_green_13.bin"},
	{"aw22xxx_cfg_led_blue_13.bin"},
	{"aw22xxx_cfg_led_red_24.bin"},
	{"aw22xxx_cfg_led_green_24.bin"},
	{"aw22xxx_cfg_led_blue_24.bin"},
	{"aw22xxx_cfg_led_red_on.bin"},
	{"aw22xxx_cfg_led_green_on.bin"},
	{"aw22xxx_cfg_led_blue_on.bin"},
	{"aw22xxx_cfg_led_yellow_on.bin"},
	{"aw22xxx_cfg_led_game.bin"},
};

#define AW22XXX_IMAX_NAME_MAX       32
static char aw22xxx_imax_name[][AW22XXX_IMAX_NAME_MAX] = {
	{"AW22XXX_IMAX_2mA"},
	{"AW22XXX_IMAX_3mA"},
	{"AW22XXX_IMAX_4mA"},
	{"AW22XXX_IMAX_6mA"},
	{"AW22XXX_IMAX_9mA"},
	{"AW22XXX_IMAX_10mA"},
	{"AW22XXX_IMAX_15mA"},
	{"AW22XXX_IMAX_20mA"},
	{"AW22XXX_IMAX_30mA"},
	{"AW22XXX_IMAX_40mA"},
	{"AW22XXX_IMAX_45mA"},
	{"AW22XXX_IMAX_60mA"},
	{"AW22XXX_IMAX_75mA"},
};

static char aw22xxx_imax_code[] = {
	AW22XXX_IMAX_2mA,
	AW22XXX_IMAX_3mA,
	AW22XXX_IMAX_4mA,
	AW22XXX_IMAX_6mA,
	AW22XXX_IMAX_9mA,
	AW22XXX_IMAX_10mA,
	AW22XXX_IMAX_15mA,
	AW22XXX_IMAX_20mA,
	AW22XXX_IMAX_30mA,
	AW22XXX_IMAX_40mA,
	AW22XXX_IMAX_45mA,
	AW22XXX_IMAX_60mA,
	AW22XXX_IMAX_75mA,
};

/******************************************************
 *
 * aw22xxx i2c write/read
 *
 ******************************************************/
static int aw22xxx_i2c_write(struct aw22xxx *aw22xxx, unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_write_byte_data(aw22xxx->i2c, reg_addr, reg_data);
		if (ret < 0) {
			pr_err ("%s: i2c_write cnt=%d error=%d\n", __func__, cnt, ret);
		} else {
			break;
		}
		cnt++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}

static int aw22xxx_i2c_read(struct aw22xxx *aw22xxx, unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data (aw22xxx->i2c, reg_addr);
		if (ret < 0) {
			pr_err ("%s: i2c_read cnt=%d error=%d\n", __func__, cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		msleep (AW_I2C_RETRY_DELAY);
	}

	return ret;
}

static int aw22xxx_i2c_write_bits (struct aw22xxx *aw22xxx, unsigned char reg_addr, unsigned char mask, unsigned char reg_data)
{
	unsigned char reg_val;

	aw22xxx_i2c_read (aw22xxx, reg_addr, &reg_val);
	reg_val &= mask;
	reg_val |= reg_data;
	aw22xxx_i2c_write (aw22xxx, reg_addr, reg_val);

	return 0;
}

#ifdef AW22XXX_FLASH_I2C_WRITES
static int aw22xxx_i2c_writes (struct aw22xxx *aw22xxx, unsigned char reg_addr, unsigned char *buf, unsigned int len)
{
	int ret = -1;
	unsigned char *data;

	data = kmalloc (len + 1, GFP_KERNEL);
	if (data == NULL) {
		pr_err ("%s: can not allocate memory\n", __func__);
		return -ENOMEM;
	}

	data[0] = reg_addr;
	memcpy (&data[1], buf, len);

	ret = i2c_master_send (aw22xxx->i2c, data, len + 1);
	if (ret < 0) {
		pr_err ("%s: i2c master send error\n", __func__);
	}

	kfree (data);

	return ret;
}
#endif

/*****************************************************
 *
 * aw22xxx led cfg
 *
 *****************************************************/
static int aw22xxx_reg_page_cfg (struct aw22xxx *aw22xxx, unsigned char page)
{
	aw22xxx_i2c_write (aw22xxx, REG_PAGE, page);
	return 0;
}

static int aw22xxx_sys_init(struct aw22xxx *aw22xxx)
{
	unsigned char i;
	pr_info ("%s: enter\n", __func__);
	for(i = 0; i < sizeof(aw22xxx_init_code); i+=2)
	{
		aw22xxx_i2c_write(aw22xxx, aw22xxx_init_code[i], aw22xxx_init_code[i+1]);
	}
	return 0;
}

static int aw22xxx_sw_reset (struct aw22xxx *aw22xxx)
{
	aw22xxx_i2c_write (aw22xxx, REG_SRST, AW22XXX_SRSTW);
	msleep (2);
	return 0;
}

static int aw22xxx_chip_enable (struct aw22xxx *aw22xxx, bool flag)
{
	if (flag) {
		aw22xxx_i2c_write_bits (aw22xxx, REG_GCR, BIT_GCR_CHIPEN_MASK, BIT_GCR_CHIPEN_ENABLE);
	} else {
		aw22xxx_i2c_write_bits (aw22xxx, REG_GCR, BIT_GCR_CHIPEN_MASK, BIT_GCR_CHIPEN_DISABLE);
	}
	msleep (2);
	return 0;
}

static int aw22xxx_mcu_reset (struct aw22xxx *aw22xxx, bool flag)
{
	if (flag) {
		aw22xxx_i2c_write_bits (aw22xxx, REG_MCUCTR, BIT_MCUCTR_MCU_RESET_MASK, BIT_MCUCTR_MCU_RESET_ENABLE);
	} else {
		aw22xxx_i2c_write_bits (aw22xxx, REG_MCUCTR, BIT_MCUCTR_MCU_RESET_MASK, BIT_MCUCTR_MCU_RESET_DISABLE);
	}
	return 0;
}

static int aw22xxx_mcu_enable (struct aw22xxx *aw22xxx, bool flag)
{
	if (flag) {
		aw22xxx_i2c_write_bits (aw22xxx, REG_MCUCTR, BIT_MCUCTR_MCU_WORK_MASK, BIT_MCUCTR_MCU_WORK_ENABLE);
	} else {
		aw22xxx_i2c_write_bits (aw22xxx, REG_MCUCTR, BIT_MCUCTR_MCU_WORK_MASK, BIT_MCUCTR_MCU_WORK_DISABLE);
	}
	return 0;
}

static int aw22xxx_led_task0_cfg (struct aw22xxx *aw22xxx, unsigned char task)
{
	aw22xxx_i2c_write (aw22xxx, REG_TASK0, task);
	return 0;
}

static int aw22xxx_led_task1_cfg (struct aw22xxx *aw22xxx, unsigned char task)
{
	aw22xxx_i2c_write (aw22xxx, REG_TASK1, task);
	return 0;
}

static int aw22xxx_imax_cfg (struct aw22xxx *aw22xxx, unsigned char imax)
{
	if (imax > 0x0f) {
		imax = 0x0f;
	}
	aw22xxx_reg_page_cfg (aw22xxx, AW22XXX_REG_PAGE0);
	aw22xxx_i2c_write (aw22xxx, REG_IMAX, imax);

	return 0;
}

static int aw22xxx_dbgctr_cfg (struct aw22xxx *aw22xxx, unsigned char cfg)
{
	if (cfg >= (AW22XXX_DBGCTR_MAX - 1)) {
		cfg = AW22XXX_DBGCTR_NORMAL;
	}
	aw22xxx_i2c_write (aw22xxx, REG_DBGCTR, cfg);

	return 0;
}

static int aw22xxx_addr_cfg (struct aw22xxx *aw22xxx, unsigned int addr)
{
	aw22xxx_i2c_write (aw22xxx, REG_ADDR1, (unsigned char) ((addr >> 0) & 0xff));
	aw22xxx_i2c_write (aw22xxx, REG_ADDR2, (unsigned char) ((addr >> 8) & 0xff));

	return 0;
}

static int aw22xxx_data_cfg (struct aw22xxx *aw22xxx, unsigned int data)
{
	aw22xxx_i2c_write (aw22xxx, REG_DATA, data);

	return 0;
}

static int aw22xxx_led_display (struct aw22xxx *aw22xxx)
{
	aw22xxx_addr_cfg (aw22xxx, 0x00e1);
	aw22xxx_dbgctr_cfg (aw22xxx, AW22XXX_DBGCTR_SFR);
	aw22xxx_data_cfg (aw22xxx, 0x3d);
	aw22xxx_dbgctr_cfg (aw22xxx, AW22XXX_DBGCTR_NORMAL);
	return 0;
}

static int aw22xxx_led_off (struct aw22xxx *aw22xxx)
{
	aw22xxx_led_task0_cfg (aw22xxx, 0xff);
	aw22xxx_mcu_reset (aw22xxx, true);
	return 0;
}

static void aw22xxx_brightness_work (struct work_struct *work)
{
	struct aw22xxx *aw22xxx = container_of (work, struct aw22xxx, brightness_work);

	pr_info ("%s: enter\n", __func__);

	aw22xxx_led_off (aw22xxx);
	aw22xxx_chip_enable (aw22xxx, false);
	if (aw22xxx->cdev.brightness) {
		aw22xxx_chip_enable (aw22xxx, true);
		aw22xxx_mcu_enable (aw22xxx, true);

		aw22xxx_imax_cfg (aw22xxx, (unsigned char) aw22xxx->imax);
		aw22xxx_led_display (aw22xxx);

		aw22xxx_led_task0_cfg (aw22xxx, 0x82);
		aw22xxx_mcu_reset (aw22xxx, false);
	}
}

static void aw22xxx_set_brightness (struct led_classdev *cdev,
			 enum led_brightness brightness)
{
	struct aw22xxx *aw22xxx = container_of (cdev, struct aw22xxx, cdev);

	aw22xxx->cdev.brightness = brightness;

	schedule_work (&aw22xxx->brightness_work);
}

static void aw22xxx_task_work (struct work_struct *work)
{
	struct aw22xxx *aw22xxx = container_of (work, struct aw22xxx, task_work);

	pr_info ("%s: enter\n", __func__);

	aw22xxx_led_off (aw22xxx);
	aw22xxx_chip_enable (aw22xxx, false);
	if (aw22xxx->task0) {
		aw22xxx_chip_enable (aw22xxx, true);
		aw22xxx_mcu_enable (aw22xxx, true);

		aw22xxx_imax_cfg (aw22xxx, (unsigned char) aw22xxx->imax);
		aw22xxx_led_display (aw22xxx);

		aw22xxx_led_task0_cfg (aw22xxx, aw22xxx->task0);
		aw22xxx_led_task1_cfg (aw22xxx, aw22xxx->task1);
		aw22xxx_mcu_reset (aw22xxx, false);
	}
}

static int aw22xxx_led_init (struct aw22xxx *aw22xxx)
{
	pr_info ("%s: enter\n", __func__);

	aw22xxx_sw_reset (aw22xxx);
	aw22xxx_chip_enable (aw22xxx, true);
	aw22xxx_imax_cfg (aw22xxx, aw22xxx_imax_code[aw22xxx->imax]);
	aw22xxx_chip_enable (aw22xxx, false);
	aw22xxx_sys_init(aw22xxx);

	pr_info ("%s: exit\n", __func__);

	return 0;
}

/*****************************************************
 *
 * firmware/cfg update
 *
 *****************************************************/
static void aw22xxx_cfg_loaded (const struct firmware *cont, void *context)
{
	struct aw22xxx *aw22xxx = context;
	int i = 0;
	unsigned char page = 0;
	unsigned char reg_addr = 0;
	unsigned char reg_val = 0;
	unsigned int time_val = 0;
	unsigned int cnt = 0;

	pr_info ("%s: enter\n", __func__);

	if (!cont) {
		pr_err ("%s: failed to read %s\n", __func__, aw22xxx_cfg_name[aw22xxx->effect]);
		release_firmware (cont);
		return;
	}

	pr_info ("%s: loaded %s - size: %zu\n", __func__, aw22xxx_cfg_name[aw22xxx->effect], cont ? cont->size : 0);

	for (i = 0; i < cont->size; i += 2) {
		if (*(cont->data + i) == 0xff) {
			page = *(cont->data + i + 1);
		}
		if (aw22xxx->cfg == 1) {
			aw22xxx_i2c_write (aw22xxx, *(cont->data + i), *(cont->data + i + 1));
			pr_debug ("%s: addr:0x%02x, data:0x%02x\n", __func__, *(cont->data + i), *(cont->data + i + 1));
		} else {
			if (page == AW22XXX_REG_PAGE1) {
				reg_addr = *(cont->data + i);
				if ((reg_addr < 0x2b) && (reg_addr > 0x0f)) {
					reg_addr -= 0x10;
					reg_val = (unsigned char) (((aw22xxx->rgb[reg_addr / 3]) >> (8 * (2 - reg_addr % 3))) & 0xff);
					aw22xxx_i2c_write (aw22xxx, *(cont->data + i), reg_val);
					pr_debug ("%s: addr:0x%02x, data:0x%02x\n", __func__, *(cont->data + i), reg_val);
				}
				else if(reg_addr > 0x2b && reg_addr < 0xaa){
					if((reg_addr == 0x2f+cnt*7) || (reg_addr == 0x30+cnt*7) || (reg_addr == 0x2e +cnt*7) ||  (reg_addr == 0x31+cnt*7)){
						reg_val = (aw22xxx->frq<64)?aw22xxx->frq:64;
						pr_debug ("%s: addr:0x%02x, data:0x%02x\n", __func__, reg_addr, reg_val);
						aw22xxx_i2c_write(aw22xxx, reg_addr,reg_val);
						time_val++;
						if(time_val>=4){
							cnt++;
							time_val=0;
						}
					}
					else {
						aw22xxx_i2c_write (aw22xxx, *(cont->data + i), *(cont->data + i + 1));
						pr_debug ("%s: addr:0x%02x, data:0x%02x\n", __func__, *(cont->data + i), *(cont->data + i + 1));
					}
				}
				else {
					aw22xxx_i2c_write (aw22xxx, *(cont->data + i), *(cont->data + i + 1));
					pr_debug ("%s: addr:0x%02x, data:0x%02x\n", __func__, *(cont->data + i), *(cont->data + i + 1));
				}
			} else {
				aw22xxx_i2c_write (aw22xxx, *(cont->data + i), *(cont->data + i + 1));
				pr_debug ("%s: addr:0x%02x, data:0x%02x\n", __func__, *(cont->data + i), *(cont->data + i + 1));
			}
		}
		if (page == AW22XXX_REG_PAGE0) {
			reg_addr = *(cont->data + i);
			reg_val = *(cont->data + i + 1);
			/* gcr chip enable delay */
			if ((reg_addr == REG_GCR) && ((reg_val & BIT_GCR_CHIPEN_ENABLE) == BIT_GCR_CHIPEN_ENABLE)) {
				msleep (2);
			}
		}
	}

	release_firmware (cont);

	pr_info ("%s: cfg update complete\n", __func__);
	mutex_unlock (&aw22xxx->cfg_lock);
}

static int aw22xxx_cfg_update (struct aw22xxx *aw22xxx)
{
	pr_info ("%s: enter\n", __func__);

	if (aw22xxx->effect < (sizeof (aw22xxx_cfg_name) / AW22XXX_CFG_NAME_MAX)) {
		pr_info ("%s: cfg name=%s\n", __func__, aw22xxx_cfg_name[aw22xxx->effect]);
	} else {
		pr_err ("%s: effect 0x%02x over max value \n", __func__, aw22xxx->effect);
		return -1;
	}

	if (aw22xxx->fw_flags != AW22XXX_FLAG_FW_OK) {
		pr_err ("%s: fw update error: not compelte \n", __func__);
		return -2;
	}
	mutex_lock (&aw22xxx->cfg_lock);

	return request_firmware_nowait (THIS_MODULE, FW_ACTION_HOTPLUG, aw22xxx_cfg_name[aw22xxx->effect], aw22xxx->dev, GFP_KERNEL, aw22xxx, aw22xxx_cfg_loaded);
}

static int aw22xxx_container_update (struct aw22xxx *aw22xxx, struct aw22xxx_container *aw22xxx_fw)
{
	unsigned int i;
	unsigned char reg_val;
	unsigned int tmp_bist;
#ifdef AW22XXX_FLASH_I2C_WRITES
	unsigned int tmp_len;
#endif

	/* chip enable */
	aw22xxx_reg_page_cfg (aw22xxx, AW22XXX_REG_PAGE0);
	aw22xxx_sw_reset (aw22xxx);
	aw22xxx_chip_enable (aw22xxx, true);
	aw22xxx_mcu_enable (aw22xxx, true);

	/* flash cfg */
	aw22xxx_i2c_write (aw22xxx, 0x80, 0xec);
	aw22xxx_i2c_write (aw22xxx, 0x35, 0x29);
	//aw22xxx_i2c_write(aw22xxx, 0x37, 0xba);
	aw22xxx_i2c_write (aw22xxx, 0x38, aw22xxx_fw->key);

	/* flash erase */
	aw22xxx_i2c_write (aw22xxx, 0x22, 0x00);
	aw22xxx_i2c_write (aw22xxx, 0x21, 0x00);
	aw22xxx_i2c_write (aw22xxx, 0x20, 0x03);
	aw22xxx_i2c_write (aw22xxx, 0x30, 0x03);
	aw22xxx_i2c_write (aw22xxx, 0x23, 0x00);
	msleep (40);
	aw22xxx_i2c_write (aw22xxx, 0x30, 0x00);
	aw22xxx_i2c_write (aw22xxx, 0x22, 0x40);
	aw22xxx_i2c_write (aw22xxx, 0x21, 0x00);
	aw22xxx_i2c_write (aw22xxx, 0x30, 0x02);
	aw22xxx_i2c_write (aw22xxx, 0x23, 0x00);
	msleep (6);
	aw22xxx_i2c_write (aw22xxx, 0x30, 0x00);
	aw22xxx_i2c_write (aw22xxx, 0x22, 0x42);
	aw22xxx_i2c_write (aw22xxx, 0x21, 0x00);
	aw22xxx_i2c_write (aw22xxx, 0x30, 0x02);
	aw22xxx_i2c_write (aw22xxx, 0x23, 0x00);
	msleep (6);
	aw22xxx_i2c_write (aw22xxx, 0x30, 0x00);
	aw22xxx_i2c_write (aw22xxx, 0x22, 0x44);
	aw22xxx_i2c_write (aw22xxx, 0x21, 0x00);
	aw22xxx_i2c_write (aw22xxx, 0x30, 0x02);
	aw22xxx_i2c_write (aw22xxx, 0x23, 0x00);
	msleep (6);
	aw22xxx_i2c_write (aw22xxx, 0x30, 0x00);
	aw22xxx_i2c_write (aw22xxx, 0x20, 0x00);

#ifdef AW22XXX_FLASH_I2C_WRITES
	/* flash writes */
	aw22xxx_i2c_write (aw22xxx, 0x20, 0x03);
	for (i = 0; i < aw22xxx_fw->len; i += tmp_len) {
		aw22xxx_i2c_write (aw22xxx, 0x22, ((i >> 8) & 0xff));
		aw22xxx_i2c_write (aw22xxx, 0x21, ((i >> 0) & 0xff));
		aw22xxx_i2c_write (aw22xxx, 0x11, 0x01);
		aw22xxx_i2c_write (aw22xxx, 0x30, 0x04);
		if ((aw22xxx_fw->len - i) < MAX_FLASH_WRITE_BYTE_SIZE) {
			tmp_len = aw22xxx_fw->len - i;
		} else {
			tmp_len = MAX_FLASH_WRITE_BYTE_SIZE;
		}
		aw22xxx_i2c_writes (aw22xxx, 0x23, &aw22xxx_fw->data[i], tmp_len);
		aw22xxx_i2c_write (aw22xxx, 0x11, 0x00);
		aw22xxx_i2c_write (aw22xxx, 0x30, 0x00);
	}
	aw22xxx_i2c_write (aw22xxx, 0x20, 0x00);
#else
	/* flash write */
	aw22xxx_i2c_write (aw22xxx, 0x20, 0x03);
	for (i = 0; i < aw22xxx_fw->len; i++) {
		aw22xxx_i2c_write (aw22xxx, 0x22, ((i >> 8) & 0xff));
		aw22xxx_i2c_write (aw22xxx, 0x21, ((i >> 0) & 0xff));
		aw22xxx_i2c_write (aw22xxx, 0x30, 0x04);
		aw22xxx_i2c_write (aw22xxx, 0x23, aw22xxx_fw->data[i]);
		aw22xxx_i2c_write (aw22xxx, 0x30, 0x00);
	}
	aw22xxx_i2c_write (aw22xxx, 0x20, 0x00);
#endif

	/* bist check */
	aw22xxx_sw_reset (aw22xxx);
	aw22xxx_chip_enable (aw22xxx, true);
	aw22xxx_mcu_enable (aw22xxx, true);
	aw22xxx_i2c_write (aw22xxx, 0x22, (((aw22xxx_fw->len - 1) >> 8) & 0xff));
	aw22xxx_i2c_write (aw22xxx, 0x21, (((aw22xxx_fw->len - 1) >> 0) & 0xff));
	aw22xxx_i2c_write (aw22xxx, 0x24, 0x07);
	msleep (5);
	aw22xxx_i2c_read (aw22xxx, 0x24, &reg_val);
	if (reg_val == 0x05) {
		aw22xxx_i2c_read (aw22xxx, 0x25, &reg_val);
		tmp_bist = reg_val;
		aw22xxx_i2c_read (aw22xxx, 0x26, &reg_val);
		tmp_bist |= (reg_val << 8);
		if (tmp_bist == aw22xxx_fw->bist) {
			pr_info ("%s: bist check pass, bist=0x%04x\n", __func__, aw22xxx_fw->bist);
		} else {
			pr_err ("%s: bist check fail, bist=0x%04x\n", __func__, aw22xxx_fw->bist);
			pr_err ("%s: fw update failed, please reset phone\n", __func__);
			return -1;
		}
	} else {
		pr_err ("%s: bist check is running, reg0x24=0x%02x\n", __func__, reg_val);
	}
	aw22xxx_i2c_write (aw22xxx, 0x24, 0x00);

	return 0;
}

static void aw22xxx_fw_loaded (const struct firmware *cont, void *context)
{
	struct aw22xxx *aw22xxx = context;
	struct aw22xxx_container *aw22xxx_fw;
	int i = 0;
	int ret = -1;
	char tmp_buf[32] = { 0 };
	unsigned int shift = 0;
	unsigned short check_sum = 0;
	unsigned char reg_val = 0;
	unsigned int tmp_bist = 0;

	pr_info ("%s: enter\n", __func__);

	if (!cont) {
		pr_err ("%s: failed to read %s\n", __func__, aw22xxx_fw_name);
		release_firmware (cont);
		return;
	}

	pr_info ("%s: loaded %s - size: %zu\n", __func__, aw22xxx_fw_name, cont ? cont->size : 0);

	/* check sum */
	for (i = 2; i < cont->size; i++) {
		check_sum += cont->data[i];
	}
	if (check_sum != (unsigned short) ((cont->data[0] << 8) | (cont->data[1]))) {
		pr_err ("%s: check sum err: check_sum=0x%04x\n", __func__, check_sum);
		release_firmware (cont);
		return;
	} else {
		pr_info ("%s: check sum pass : 0x%04x\n", __func__, check_sum);
	}

	/* get fw info */
	aw22xxx_fw = kzalloc (cont->size + 4 * sizeof (unsigned int), GFP_KERNEL);
	if (!aw22xxx_fw) {
		release_firmware (cont);
		pr_err ("%s: Error allocating memory\n", __func__);
		return;
	}
	shift += 2;

	pr_info ("%s: fw chip_id : 0x%02x\n", __func__, cont->data[0 + shift]);
	shift += 1;

	memcpy (tmp_buf, &cont->data[0 + shift], 16);
	pr_info ("%s: fw customer: %s\n", __func__, tmp_buf);
	shift += 16;

	memcpy (tmp_buf, &cont->data[0 + shift], 8);
	pr_info ("%s: fw project: %s\n", __func__, tmp_buf);
	shift += 8;

	aw22xxx_fw->version =
	    (cont->data[0 + shift] << 24) | (cont->data[1 + shift] << 16) | (cont->data[2 + shift] << 8) | (cont->data[3 + shift] << 0);
	pr_info ("%s: fw version : 0x%04x\n", __func__, aw22xxx_fw->version);
	shift += 4;

	// reserved
	shift += 3;

	aw22xxx_fw->bist =
	    (cont->data[0 + shift] << 8) | (cont->data[1 + shift] << 0);
	pr_info ("%s: fw bist : 0x%04x\n", __func__, aw22xxx_fw->bist);
	shift += 2;

	aw22xxx_fw->key = cont->data[0 + shift];
	pr_info ("%s: fw key : 0x%04x\n", __func__, aw22xxx_fw->key);
	shift += 1;

	// reserved
	shift += 1;

	aw22xxx_fw->len =
	    (cont->data[0 + shift] << 8) | (cont->data[1 + shift] << 0);
	pr_info ("%s: fw len : 0x%04x\n", __func__, aw22xxx_fw->len);
	shift += 2;

	memcpy (aw22xxx_fw->data, &cont->data[shift], aw22xxx_fw->len);
	release_firmware (cont);

	/* check version */
	//aw22xxx_get_fw_version(aw22xxx);

	/* bist check */
	aw22xxx_sw_reset (aw22xxx);
	aw22xxx_chip_enable (aw22xxx, true);
	aw22xxx_mcu_enable (aw22xxx, true);
	aw22xxx_i2c_write (aw22xxx, 0x22, (((aw22xxx_fw->len - 1) >> 8) & 0xff));
	aw22xxx_i2c_write (aw22xxx, 0x21, (((aw22xxx_fw->len - 1) >> 0) & 0xff));
	aw22xxx_i2c_write (aw22xxx, 0x24, 0x07);
	msleep (5);
	aw22xxx_i2c_read (aw22xxx, 0x24, &reg_val);
	if (reg_val == 0x05) {
		aw22xxx_i2c_read (aw22xxx, 0x25, &reg_val);
		tmp_bist = reg_val;
		aw22xxx_i2c_read (aw22xxx, 0x26, &reg_val);
		tmp_bist |= (reg_val << 8);
		if (tmp_bist == aw22xxx_fw->bist) {
			pr_info ("%s: bist check pass, bist=0x%04x\n", __func__, aw22xxx_fw->bist);
			if (aw22xxx->fw_update == 0) {
				kfree (aw22xxx_fw);
				aw22xxx_i2c_write (aw22xxx, 0x24, 0x00);
				aw22xxx_led_init (aw22xxx);
				aw22xxx->fw_flags = AW22XXX_FLAG_FW_OK;
				return;
			} else {
				pr_info ("%s: fw version: 0x%04x, force update fw\n", __func__, aw22xxx_fw->version);
			}
		} else {
			pr_info ("%s: bist check fail, fw bist=0x%04x, flash bist=0x%04x\n", __func__, aw22xxx_fw->bist, tmp_bist);
			pr_info ("%s: find new fw: 0x%04x, need update\n", __func__, aw22xxx_fw->version);
		}
	} else {
		pr_err ("%s: bist check is running, reg0x24=0x%02x\n", __func__, reg_val);
		pr_info ("%s: fw need update\n", __func__);
	}
	aw22xxx_i2c_write (aw22xxx, 0x24, 0x00);

	/* fw update */
	ret = aw22xxx_container_update (aw22xxx, aw22xxx_fw);
	if (ret) {
		aw22xxx->fw_flags = AW22XXX_FLAG_FW_FAIL;
	} else {
		aw22xxx->fw_flags = AW22XXX_FLAG_FW_OK;
	}
	kfree (aw22xxx_fw);

	aw22xxx->fw_update = 0;

	aw22xxx_led_init (aw22xxx);

	pr_info ("%s: exit\n", __func__);
}

static int aw22xxx_fw_update (struct aw22xxx *aw22xxx)
{
	pr_info ("%s: enter\n", __func__);

	aw22xxx->fw_flags = AW22XXX_FLAG_FW_UPDATE;

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG, aw22xxx_fw_name, aw22xxx->dev, GFP_KERNEL, aw22xxx, aw22xxx_fw_loaded);
}

#ifdef AWINIC_FW_UPDATE_DELAY
static enum hrtimer_restart aw22xxx_fw_timer_func (struct hrtimer *timer) 
{
	struct aw22xxx *aw22xxx = container_of (timer, struct aw22xxx, fw_timer);

	pr_info ("%s: enter\n", __func__);

	schedule_work (&aw22xxx->fw_work);

	return HRTIMER_NORESTART;
}
#endif

static void aw22xxx_fw_work_routine (struct work_struct *work)
{
	struct aw22xxx *aw22xxx = container_of (work, struct aw22xxx, fw_work);

	pr_info ("%s: enter\n", __func__);

	aw22xxx_fw_update (aw22xxx);

}

static void aw22xxx_cfg_work_routine (struct work_struct *work)
{
	struct aw22xxx *aw22xxx = container_of (work, struct aw22xxx, cfg_work);

	pr_info ("%s: enter\n", __func__);

	aw22xxx_cfg_update (aw22xxx);

}

static int aw22xxx_fw_init (struct aw22xxx *aw22xxx)
{
#ifdef AWINIC_FW_UPDATE_DELAY
	int fw_timer_val = 10000;

	hrtimer_init (&aw22xxx->fw_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw22xxx->fw_timer.function = aw22xxx_fw_timer_func;
	INIT_WORK (&aw22xxx->fw_work, aw22xxx_fw_work_routine);
	INIT_WORK (&aw22xxx->cfg_work, aw22xxx_cfg_work_routine);
	hrtimer_start (&aw22xxx->fw_timer, ktime_set (fw_timer_val / 1000, (fw_timer_val % 1000) * 1000000), HRTIMER_MODE_REL);
#else
	INIT_WORK (&aw22xxx->fw_work, aw22xxx_fw_work_routine);
	INIT_WORK (&aw22xxx->cfg_work, aw22xxx_cfg_work_routine);
	schedule_work (&aw22xxx->fw_work);
#endif
	return 0;
}

/*****************************************************
 *
 * device tree
 *
 *****************************************************/
static int aw22xxx_parse_dt (struct device *dev, struct aw22xxx *aw22xxx, struct device_node *np)
{
	aw22xxx->reset_gpio = of_get_named_gpio (np, "reset-gpio", 0);
	if (aw22xxx->reset_gpio < 0) {
		dev_err (dev, "%s: no reset gpio provided, will not HW reset device\n", __func__);
		return -1;
	} else {
		dev_info (dev, "%s: reset gpio provided ok\n", __func__);
	}

	return 0;
}

static int aw22xxx_power_init (struct aw22xxx *aw22xxx)
{
	int ret = -1;
	struct regulator *vreg;
	struct device *dev = aw22xxx->dev;

	vreg = regulator_get (dev, "aw22xxx_vreg");
	if (vreg == NULL) {
		pr_err ("aw22xxx_vreg regulator get failed!\n");
		goto rg_err;
	}

	if (regulator_is_enabled (vreg)) {
		pr_info ("aw22xxx_vreg is already enabled!\n");
	} else {
		ret = regulator_enable (vreg);
		if (ret) {
			pr_err ("error enabling aw22xxx_vreg!\n");
			vreg = NULL;
			goto rg_err;
		}
	}

	ret = regulator_get_voltage (vreg);
	pr_info ("%s regulator_value %d!\n", __func__, ret);

	pr_info ("power init successful");
	return 0;

rg_err:
	return -1;
}

static int aw22xxx_hw_reset (struct aw22xxx *aw22xxx)
{
	pr_info ("%s: enter\n", __func__);

	if (aw22xxx && gpio_is_valid (aw22xxx->reset_gpio)) {
		gpio_set_value_cansleep (aw22xxx->reset_gpio, 0);
		msleep (1);
		gpio_set_value_cansleep (aw22xxx->reset_gpio, 1);
		msleep (1);
	} else {
		dev_err (aw22xxx->dev, "%s:  failed\n", __func__);
	}
	return 0;
}

static int aw22xxx_hw_off (struct aw22xxx *aw22xxx)
{
	pr_info ("%s: enter\n", __func__);

	if (aw22xxx && gpio_is_valid (aw22xxx->reset_gpio)) {
		gpio_set_value_cansleep (aw22xxx->reset_gpio, 0);
		msleep (1);
	} else {
		dev_err (aw22xxx->dev, "%s:  failed\n", __func__);
	}
	return 0;
}

/*****************************************************
 *
 * check chip id
 *
 *****************************************************/
static int aw22xxx_read_chipid (struct aw22xxx *aw22xxx)
{
	int ret = -1;
	unsigned char cnt = 0;
	unsigned char reg_val = 0;

	aw22xxx_reg_page_cfg (aw22xxx, AW22XXX_REG_PAGE0);
	aw22xxx_sw_reset (aw22xxx);

	while (cnt < AW_READ_CHIPID_RETRIES) {
		ret = aw22xxx_i2c_read (aw22xxx, REG_SRST, &reg_val);
		if (ret < 0) {
			dev_err (aw22xxx->dev, "%s: failed to read register AW22XXX_REG_ID: %d\n", __func__, ret);
			return -EIO;
		}
		switch (reg_val) {
		case AW22XXX_SRSTR:
			pr_info ("%s aw22xxx detected\n", __func__);
			//aw22xxx->flags |= AW22XXX_FLAG_SKIP_INTERRUPTS;
			aw22xxx_i2c_read (aw22xxx, REG_CHIPID, &reg_val);
			switch (reg_val) {
			case AW22118_CHIPID:
				aw22xxx->chipid = AW22118;
				pr_info ("%s: chipid: aw22118\n", __func__);
				break;
			case AW22127_CHIPID:
				aw22xxx->chipid = AW22127;
				pr_info ("%s: chipid: aw22127\n", __func__);
				break;
			default:
				pr_err ("%s: unknown id=0x%02x\n", __func__, reg_val);
				break;
			}
			return 0;
		default:
			pr_info ("%s unsupported device revision (0x%x)\n", __func__, reg_val);
			break;
		}
		cnt++;

		msleep (AW_READ_CHIPID_RETRY_DELAY);
	}

	return -EINVAL;
}

/******************************************************
 *
 * frq match time , rase_time_ms = hold_time_ms = fall_tme_ms = off_time_ms = time /2
 *
 ******************************************************/
unsigned char find_closest_value(uint16_t time)
{
	unsigned char led;
	uint16_t ref[] = {	0,64,128,192,256,320,384,448,512,576,640,704,768,832,896,960,
						1024,1088,1152,1216,1280,1344,1408,1472,1536,1600,1664,1728,1792,1856,1920,1984,
						2048,2112,2176,2240,2304,2368,2432,2496,2560,2624,2688,2752,2816,2880,2944,3008,3072,
						3136,3200,3264,3328,3392,3456,3520,3584,3648,3712,3776,3840,3904,3968,4032};

	for(led=0; led<sizeof(ref)/sizeof(uint16_t); led++)
	{
		if(time <= ref[0]){
			return 0;
		}
		else if(time > ref[63]){
			return 63;
		}
		else if((time > ref[led]) && (time<=ref[led+1])){
			if((time - ref[led]) <= (ref[led+1]-time)){
			return led;
		}
		else
			return led+1;
		}
	}
	return 0;
}

/******************************************************
 *
 * sys group attribute: reg
 *
 ******************************************************/
static ssize_t aw22xxx_reg_store (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata (dev);
	struct aw22xxx *aw22xxx = container_of (led_cdev, struct aw22xxx, cdev);

	unsigned int databuf[2] = { 0, 0 };

	if (2 == sscanf (buf, "%x %x", &databuf[0], &databuf[1])) {
		aw22xxx_i2c_write (aw22xxx, databuf[0], databuf[1]);
	}

	return count;
}

static ssize_t aw22xxx_reg_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata (dev);
	struct aw22xxx *aw22xxx = container_of (led_cdev, struct aw22xxx, cdev);
	ssize_t len = 0;
	unsigned int i = 0;
	unsigned char reg_val = 0;
	unsigned char reg_page = 0;
	aw22xxx_i2c_read (aw22xxx, REG_PAGE, &reg_page);
	for (i = 0; i < AW22XXX_REG_MAX; i++) {
		if (!reg_page) {
			if (!(aw22xxx_reg_access[i] & REG_RD_ACCESS))
				continue;
		}
		aw22xxx_i2c_read (aw22xxx, i, &reg_val);
		len +=
		    snprintf (buf + len, PAGE_SIZE - len, "reg:0x%02x=0x%02x \n", i, reg_val);
	}
	return len;
}

static ssize_t aw22xxx_hwen_store (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata (dev);
	struct aw22xxx *aw22xxx = container_of (led_cdev, struct aw22xxx, cdev);

	unsigned int databuf[1] = { 0 };

	if (1 == sscanf (buf, "%x", &databuf[0])) {
		if (1 == databuf[0]) {
			aw22xxx_hw_reset (aw22xxx);
			aw22xxx_sys_init (aw22xxx);
		} else {
			aw22xxx_hw_off (aw22xxx);
		}
	}

	return count;
}

static ssize_t aw22xxx_hwen_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata (dev);
	struct aw22xxx *aw22xxx = container_of (led_cdev, struct aw22xxx, cdev);
	ssize_t len = 0;
	len += snprintf (buf + len, PAGE_SIZE - len, "hwen=%d\n", gpio_get_value (aw22xxx->reset_gpio));

	return len;
}

static ssize_t aw22xxx_fw_store (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata (dev);
	struct aw22xxx *aw22xxx = container_of (led_cdev, struct aw22xxx, cdev);

	unsigned int databuf[1] = { 0 };

	if (1 == sscanf (buf, "%x", &databuf[0])) {
		aw22xxx->fw_update = databuf[0];
		if (1 == databuf[0]) {
			schedule_work (&aw22xxx->fw_work);
		}
	}

	return count;
}

static ssize_t aw22xxx_fw_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf (buf + len, PAGE_SIZE - len, "firmware name = %s\n", aw22xxx_fw_name);

	return len;
}

static ssize_t aw22xxx_cfg_store (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata (dev);
	struct aw22xxx *aw22xxx = container_of (led_cdev, struct aw22xxx, cdev);

	unsigned int databuf[1] = { 0 };

	if (1 == sscanf (buf, "%d", &databuf[0])) {
		pr_info ("%s: enter, cfg value is %d \n", __func__, databuf[0]);
		aw22xxx->cfg = databuf[0];
		if (aw22xxx->cfg) {
			schedule_work (&aw22xxx->cfg_work);
		}
	}

	return count;
}

static ssize_t aw22xxx_cfg_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	unsigned int i;
	struct led_classdev *led_cdev = dev_get_drvdata (dev);
	struct aw22xxx *aw22xxx = container_of (led_cdev, struct aw22xxx, cdev);

	for (i = 0; i < sizeof (aw22xxx_cfg_name) / AW22XXX_CFG_NAME_MAX; i++) {
		len += snprintf (buf + len, PAGE_SIZE - len, "cfg[%x] = %s\n", i, aw22xxx_cfg_name[i]);
	}
	len += snprintf (buf + len, PAGE_SIZE - len, "current cfg = %s\n", aw22xxx_cfg_name[aw22xxx->effect]);

	return len;
}

static ssize_t aw22xxx_effect_store (struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	unsigned int databuf[1];
	struct led_classdev *led_cdev = dev_get_drvdata (dev);
	struct aw22xxx *aw22xxx = container_of (led_cdev, struct aw22xxx, cdev);

	sscanf (buf, "%d", &databuf[0]);
	pr_info ("%s: enter, effect value is %d \n", __func__, databuf[0]);

	aw22xxx->effect = databuf[0];

	return len;
}

static ssize_t aw22xxx_effect_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct led_classdev *led_cdev = dev_get_drvdata (dev);
	struct aw22xxx *aw22xxx = container_of (led_cdev, struct aw22xxx, cdev);

	len += snprintf (buf + len, PAGE_SIZE - len, "effect = 0x%02x\n", aw22xxx->effect);

	return len;
}

static ssize_t aw22xxx_imax_store (struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	unsigned int databuf[1];
	struct led_classdev *led_cdev = dev_get_drvdata (dev);
	struct aw22xxx *aw22xxx = container_of (led_cdev, struct aw22xxx, cdev);

	sscanf (buf, "%d", &databuf[0]);
	pr_info ("%s: enter, imax index value is %d, imax value is %s\n", __func__, databuf[0], aw22xxx_imax_name[databuf[0]]);

	if (databuf[0] > AW22XXX_IMAX_LIMIT) {
		pr_err ("aw22xxx imax set: More than AW22XXX_IMAX_LIMIT, Reset IMAX to 20mA \n");
		databuf[0] = AW22XXX_IMAX_LIMIT; //LIMIT IMAX 20MA
	}

	aw22xxx->imax = databuf[0];

	aw22xxx_imax_cfg (aw22xxx, aw22xxx_imax_code[aw22xxx->imax]);

	return len;
}

static ssize_t aw22xxx_imax_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	unsigned int i;
	struct led_classdev *led_cdev = dev_get_drvdata (dev);
	struct aw22xxx *aw22xxx = container_of (led_cdev, struct aw22xxx, cdev);

	for (i = 0; i < sizeof (aw22xxx_imax_name) / AW22XXX_IMAX_NAME_MAX; i++) {
		len += snprintf (buf + len, PAGE_SIZE - len, "imax[%x] = %s\n", i, aw22xxx_imax_name[i]);
	}
	len += snprintf (buf + len, PAGE_SIZE - len, "current id = 0x%02x, imax = %s\n", aw22xxx->imax, aw22xxx_imax_name[aw22xxx->imax]);

	return len;
}

static ssize_t aw22xxx_rgb_store (struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	unsigned int databuf[2];
	struct led_classdev *led_cdev = dev_get_drvdata (dev);
	struct aw22xxx *aw22xxx = container_of (led_cdev, struct aw22xxx, cdev);

	sscanf (buf, "%x %x", &databuf[0], &databuf[1]);
	pr_info ("%s: enter, rgb index value is %d , rgb value is %x\n", __func__, databuf[0], databuf[1]);
	aw22xxx->rgb[databuf[0]] = databuf[1];

	return len;
}

static ssize_t aw22xxx_rgb_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	unsigned int i;
	struct led_classdev *led_cdev = dev_get_drvdata (dev);
	struct aw22xxx *aw22xxx = container_of (led_cdev, struct aw22xxx, cdev);

	for (i = 0; i < AW22XXX_RGB_MAX; i++) {
		len += snprintf (buf + len, PAGE_SIZE - len, "rgb[%d] = 0x%06x\n", i, aw22xxx->rgb[i]);
	}
	return len;
}

static ssize_t aw22xxx_task0_store (struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	unsigned int databuf[1];
	struct led_classdev *led_cdev = dev_get_drvdata (dev);
	struct aw22xxx *aw22xxx = container_of (led_cdev, struct aw22xxx, cdev);

	sscanf (buf, "%x", &databuf[0]);
	aw22xxx->task0 = databuf[0];
	schedule_work (&aw22xxx->task_work);

	return len;
}

static ssize_t aw22xxx_task0_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct led_classdev *led_cdev = dev_get_drvdata (dev);
	struct aw22xxx *aw22xxx = container_of (led_cdev, struct aw22xxx, cdev);

	len += snprintf(buf + len, PAGE_SIZE - len, "task0 = 0x%02x\n", aw22xxx->task0);

	return len;
}

static ssize_t aw22xxx_task1_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	unsigned int databuf[1];
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw22xxx *aw22xxx = container_of (led_cdev, struct aw22xxx, cdev);

	sscanf (buf, "%x", &databuf[0]);
	aw22xxx->task1 = databuf[0];

	return len;
}

static ssize_t aw22xxx_task1_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct led_classdev *led_cdev = dev_get_drvdata (dev);
	struct aw22xxx *aw22xxx = container_of (led_cdev, struct aw22xxx, cdev);

	len += snprintf (buf + len, PAGE_SIZE - len, "task1 = 0x%02x\n", aw22xxx->task1);

	return len;
}


static ssize_t aw22xxx_frq_store(struct device* dev, struct device_attribute *attr, const char* buf, size_t len)
{
	uint16_t databuf[1];
	unsigned char frq_value = 0;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

	sscanf(buf,"%d",&databuf[0]);
	frq_value = find_closest_value(databuf[0]);
	pr_info ("%s: enter, frq value is %d , frq_index is %d\n", __func__, databuf[0], frq_value);

	aw22xxx->frq = frq_value;

	return len;
}

static ssize_t aw22xxx_frq_show(struct device* dev,struct device_attribute *attr, char* buf)
{
	ssize_t len = 0;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw22xxx *aw22xxx = container_of(led_cdev, struct aw22xxx, cdev);

	len += snprintf(buf+len, PAGE_SIZE-len, "frq = 0x%02x,effect = 0x%02x\n", aw22xxx->frq, aw22xxx->effect);

	return len;
}

static int  aw22xxx_brightness_store (struct led_classdev * cdv,enum led_brightness  value)
{

	unsigned char i;
	struct aw22xxx *aw22xxx = NULL;

	pr_info ("%s: enter, led_brightness value is %d \n", __func__, value);

	if(!strcmp(cdv->name,"red")){
		pr_info ("aw22xxx set brightness of red \n");
		aw22xxx = container_of (cdv, struct aw22xxx, red_cdev);

		aw22xxx_led_rgb_code[AW22XXX_RGB_INDEX] = AW22XXX_RED;
		aw22xxx_led_rgb_code[AW22XXX_BRIGHTNESS_INDEX] = (unsigned char)value;
		aw22xxx->red_cdev.brightness = (unsigned char)value;
		for(i = 0; i < sizeof(aw22xxx_led_rgb_code); i+=2)
		{
			aw22xxx_i2c_write(aw22xxx, aw22xxx_led_rgb_code[i], aw22xxx_led_rgb_code[i+1]);
		}
	}

	else if(!strcmp(cdv->name,"green")){
		pr_info ("aw22xxx set brightness of green \n");
		aw22xxx = container_of (cdv, struct aw22xxx, green_cdev);

		aw22xxx_led_rgb_code[AW22XXX_RGB_INDEX] = AW22XXX_GREEN;
		aw22xxx_led_rgb_code[AW22XXX_BRIGHTNESS_INDEX] = (unsigned char)value;
		aw22xxx->green_cdev.brightness = (unsigned char)value;
		for(i = 0; i < sizeof(aw22xxx_led_rgb_code); i+=2)
		{
			aw22xxx_i2c_write(aw22xxx, aw22xxx_led_rgb_code[i], aw22xxx_led_rgb_code[i+1]);
		}
	}
	else if(!strcmp(cdv->name,"blue")){
		pr_info ("aw22xxx set brightness of blue \n");
		aw22xxx = container_of (cdv, struct aw22xxx, blue_cdev);

		aw22xxx_led_rgb_code[AW22XXX_RGB_INDEX] = AW22XXX_BLUE;
		aw22xxx_led_rgb_code[AW22XXX_BRIGHTNESS_INDEX] = (unsigned char)value;
		aw22xxx->blue_cdev.brightness = (unsigned char)value;
		for(i = 0; i < sizeof(aw22xxx_led_rgb_code); i+=2)
		{
			aw22xxx_i2c_write(aw22xxx, aw22xxx_led_rgb_code[i], aw22xxx_led_rgb_code[i+1]);
		}
	}
	return 0;
}

enum led_brightness aw22xxx_brightness_show(struct led_classdev *cdv)
{
	struct aw22xxx *aw22xxx = NULL;

	if(!strcmp(cdv->name,"red")){
		aw22xxx = container_of (cdv, struct aw22xxx, red_cdev);
		return aw22xxx->red_cdev.brightness;
	}

	else if(!strcmp(cdv->name,"green")){
		aw22xxx = container_of (cdv, struct aw22xxx, green_cdev);
		return aw22xxx->green_cdev.brightness;
	}
	else if(!strcmp(cdv->name,"blue")){
		aw22xxx = container_of (cdv, struct aw22xxx, blue_cdev);
		return aw22xxx->blue_cdev.brightness;
	}
	return 0;
}

static DEVICE_ATTR (reg, S_IWUSR | S_IRUGO, aw22xxx_reg_show, aw22xxx_reg_store);
static DEVICE_ATTR (hwen, S_IWUSR | S_IRUGO, aw22xxx_hwen_show, aw22xxx_hwen_store);
static DEVICE_ATTR (fw, S_IWUSR | S_IRUGO, aw22xxx_fw_show, aw22xxx_fw_store);
static DEVICE_ATTR (cfg, S_IWUSR | S_IRUGO, aw22xxx_cfg_show, aw22xxx_cfg_store);
static DEVICE_ATTR (effect, S_IWUSR | S_IRUGO, aw22xxx_effect_show, aw22xxx_effect_store);
static DEVICE_ATTR (imax, S_IWUSR | S_IRUGO, aw22xxx_imax_show, aw22xxx_imax_store);
static DEVICE_ATTR (rgb, S_IWUSR | S_IRUGO, aw22xxx_rgb_show, aw22xxx_rgb_store);
static DEVICE_ATTR (task0, S_IWUSR | S_IRUGO, aw22xxx_task0_show, aw22xxx_task0_store);
static DEVICE_ATTR (task1, S_IWUSR | S_IRUGO, aw22xxx_task1_show, aw22xxx_task1_store);
static DEVICE_ATTR(frq, S_IWUSR | S_IRUGO, aw22xxx_frq_show, aw22xxx_frq_store);

static struct attribute *aw22xxx_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_hwen.attr,
	&dev_attr_fw.attr,
	&dev_attr_cfg.attr,
	&dev_attr_effect.attr,
	&dev_attr_imax.attr,
	&dev_attr_rgb.attr,
	&dev_attr_task0.attr,
	&dev_attr_task1.attr,
	&dev_attr_frq.attr,
	NULL
};

static struct attribute_group aw22xxx_attribute_group = {
	.attrs = aw22xxx_attributes
};


/******************************************************
 *
 * led class dev
 *
 ******************************************************/
static int aw22xxx_parse_led_cdev (struct aw22xxx *aw22xxx, struct device_node *np)
{
	struct device_node *temp;
	int ret = -1;

	pr_info ("%s: enter\n", __func__);

	for_each_child_of_node (np, temp) {
		ret = of_property_read_string (temp, "aw22xxx,name", &aw22xxx->cdev.name);
		if (ret < 0) {
			dev_err (aw22xxx->dev, "Failure reading led name, ret = %d\n", ret);
			goto free_pdata;
		}
		ret = of_property_read_u32 (temp, "aw22xxx,imax", &aw22xxx->imax);
		if (ret < 0) {
			dev_err (aw22xxx->dev, "Failure reading imax, ret = %d\n", ret);
			goto free_pdata;
		}
		ret = of_property_read_u32 (temp, "aw22xxx,brightness", &aw22xxx->cdev.brightness);
		if (ret < 0) {
			dev_err (aw22xxx->dev, "Failure reading brightness, ret = %d\n", ret);
			goto free_pdata;
		}
		ret = of_property_read_u32 (temp, "aw22xxx,max_brightness", &aw22xxx->cdev.max_brightness);
		if (ret < 0) {
			dev_err (aw22xxx->dev, "Failure reading max brightness, ret = %d\n", ret);
			goto free_pdata;
		}
	}

	INIT_WORK (&aw22xxx->brightness_work, aw22xxx_brightness_work);
	INIT_WORK (&aw22xxx->task_work, aw22xxx_task_work);

	aw22xxx->cdev.brightness_set = aw22xxx_set_brightness;
	ret = led_classdev_register (aw22xxx->dev, &aw22xxx->cdev);
	if (ret) {
		dev_err (aw22xxx->dev, "unable to register led ret=%d\n", ret);
		goto free_pdata;
	}

	ret = sysfs_create_group (&aw22xxx->cdev.dev->kobj, &aw22xxx_attribute_group);
	if (ret) {
		dev_err (aw22xxx->dev, "led sysfs ret: %d\n", ret);
		goto free_class;
	}

	return 0;

free_class:
	led_classdev_unregister (&aw22xxx->cdev);
free_pdata:
	return ret;
}

static int aw22xxx_blink_set(struct led_classdev *cdv, unsigned long *delay_on, unsigned long *delay_off)
{
	unsigned char i;
	struct aw22xxx *aw22xxx = NULL;

	pr_info ("%s: enter, delay_on value is %d, delay_off value is %d \n", __func__, *delay_on, *delay_off);

	aw22xxx_led_blink_code[AW22XXX_DELAY_ON_LOW]   = (unsigned char) *delay_on&0xff;
	aw22xxx_led_blink_code[AW22XXX_DELAY_ON_HIGH]  = (unsigned char) (*delay_on>>8)&0xff;
	aw22xxx_led_blink_code[AW22XXX_DELAY_OFF_LOW]  = (unsigned char)*delay_off&0xff;
	aw22xxx_led_blink_code[AW22XXX_DELAY_OFF_HIGH] = (unsigned char) (*delay_off>>8)&0xff;

	if(!strcmp(cdv->name,"red")){
		pr_info ("aw22xxx set blink of red \n");
		aw22xxx = container_of (cdv, struct aw22xxx, red_cdev);
		for(i = 0; i < sizeof(aw22xxx_led_blink_code); i+=2)
		{
			aw22xxx_i2c_write(aw22xxx, aw22xxx_led_blink_code[i], aw22xxx_led_blink_code[i+1]);
		}
	}
	else if(!strcmp(cdv->name,"green")){
		pr_info ("aw22xxx set blink of green \n");
		aw22xxx = container_of (cdv, struct aw22xxx, green_cdev);
		for(i = 0; i < sizeof(aw22xxx_led_blink_code); i+=2)
		{
			aw22xxx_i2c_write(aw22xxx, aw22xxx_led_blink_code[i], aw22xxx_led_blink_code[i+1]);
		}
	}
	else if(!strcmp(cdv->name,"blue")){
		pr_info ("aw22xxx set blink of blue \n");
		aw22xxx = container_of (cdv, struct aw22xxx, blue_cdev);
		for(i = 0; i < sizeof(aw22xxx_led_blink_code); i+=2)
		{
			aw22xxx_i2c_write(aw22xxx, aw22xxx_led_blink_code[i], aw22xxx_led_blink_code[i+1]);
		}
	}
	return 0;
}


/******************************************************
 *
 * i2c driver
 *
 ******************************************************/
static int aw22xxx_i2c_probe (struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct aw22xxx *aw22xxx;
	struct device_node *np = i2c->dev.of_node;
	int ret;
	//int irq_flags;

	pr_info ("%s: enter\n", __func__);

	if (!i2c_check_functionality (i2c->adapter, I2C_FUNC_I2C)) {
		dev_err (&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	aw22xxx = devm_kzalloc (&i2c->dev, sizeof (struct aw22xxx), GFP_KERNEL);
	if (aw22xxx == NULL)
		return -ENOMEM;

	aw22xxx->dev = &i2c->dev;
	aw22xxx->i2c = i2c;

	i2c_set_clientdata (i2c, aw22xxx);

	mutex_init (&aw22xxx->cfg_lock);

	/* aw22xxx rst & int */
	if (np) {
		ret = aw22xxx_parse_dt (&i2c->dev, aw22xxx, np);
		if (ret) {
			dev_err (&i2c->dev, "%s: failed to parse device tree node\n", __func__);
			goto err_parse_dt;
		}
	} else {
		aw22xxx->reset_gpio = -1;
		aw22xxx->irq_gpio = -1;
	}

	if (gpio_is_valid (aw22xxx->reset_gpio)) {
		ret = devm_gpio_request_one (&i2c->dev, aw22xxx->reset_gpio, GPIOF_OUT_INIT_LOW, "aw22xxx_rst");
		if (ret) {
			dev_err (&i2c->dev, "%s: rst request failed\n", __func__);
			goto err_gpio_request;
		}
	}

	ret = aw22xxx_power_init (aw22xxx);
	if (ret) {
		pr_err ("aw22xxx_power_init failed!");
	}

	/* hardware reset */
	aw22xxx_hw_reset (aw22xxx);

	/* aw22xxx chip id */
	ret = aw22xxx_read_chipid (aw22xxx);
	if (ret < 0) {
		dev_err (&i2c->dev, "%s: aw22xxx_read_chipid failed ret=%d\n", __func__, ret);
		goto err_id;
	}

	dev_set_drvdata (&i2c->dev, aw22xxx);

	aw22xxx_parse_led_cdev (aw22xxx, np);
	if (ret < 0) {
		dev_err (&i2c->dev, "%s error creating led class dev\n", __func__);
		goto err_sysfs;
	}

	//red register
	aw22xxx->red_cdev.name = "red";
	aw22xxx->red_cdev.brightness_set_blocking = aw22xxx_brightness_store;
	aw22xxx->red_cdev.brightness_get = aw22xxx_brightness_show;
	aw22xxx->red_cdev.blink_set = aw22xxx_blink_set;

	ret = led_classdev_register (aw22xxx->dev, &aw22xxx->red_cdev);
	if (ret) {
		dev_err (aw22xxx->dev, "unable to register led ret=%d\n", ret);
		goto err_register_led;
	}

	//blue register
	aw22xxx->blue_cdev.name = "blue";
	aw22xxx->blue_cdev.brightness_set_blocking = aw22xxx_brightness_store;
	aw22xxx->blue_cdev.brightness_get = aw22xxx_brightness_show;
	aw22xxx->blue_cdev.blink_set = aw22xxx_blink_set;

	ret = led_classdev_register (aw22xxx->dev, &aw22xxx->blue_cdev);
	if (ret) {
		dev_err (aw22xxx->dev, "unable to register led ret=%d\n", ret);
		goto err_register_led;
	}

	//green register
	aw22xxx->green_cdev.name = "green";
	aw22xxx->green_cdev.brightness_set_blocking = aw22xxx_brightness_store;
	aw22xxx->green_cdev.brightness_get = aw22xxx_brightness_show;
	aw22xxx->green_cdev.blink_set = aw22xxx_blink_set;

	ret = led_classdev_register (aw22xxx->dev, &aw22xxx->green_cdev);
	if (ret) {
		dev_err (aw22xxx->dev, "unable to register led ret=%d\n", ret);
		goto err_register_led;
	}

	aw22xxx_fw_init (aw22xxx);

	pr_info ("%s probe completed successfully!\n", __func__);

	return 0;

err_sysfs:
err_id:
	devm_gpio_free (&i2c->dev, aw22xxx->reset_gpio);
err_gpio_request:
err_parse_dt:
err_register_led:
	devm_kfree (&i2c->dev, aw22xxx);
	aw22xxx = NULL;
	return ret;
}

static int aw22xxx_i2c_remove (struct i2c_client *i2c)
{
	struct aw22xxx *aw22xxx = i2c_get_clientdata (i2c);

	pr_info ("%s: enter\n", __func__);
	sysfs_remove_group (&aw22xxx->cdev.dev->kobj, &aw22xxx_attribute_group);
	led_classdev_unregister (&aw22xxx->cdev);

	if (gpio_is_valid (aw22xxx->reset_gpio))
		devm_gpio_free (&i2c->dev, aw22xxx->reset_gpio);

	devm_kfree (&i2c->dev, aw22xxx);
	aw22xxx = NULL;

	return 0;
}

static const struct i2c_device_id aw22xxx_i2c_id[] = {
	{AW22XXX_I2C_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE (i2c, aw22xxx_i2c_id);

static struct of_device_id aw22xxx_dt_match[] = {
	{.compatible = "awinic,aw22xxx_led"},
	{},
};

static struct i2c_driver aw22xxx_i2c_driver = {
	.driver = {
		.name = AW22XXX_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = aw22xxx_dt_match,
	},
	.probe = aw22xxx_i2c_probe,
	.remove = aw22xxx_i2c_remove,
	.id_table = aw22xxx_i2c_id,
};

static int __init aw22xxx_i2c_init (void)
{
	int ret = 0;

	pr_info ("aw22xxx driver version %s\n", AW22XXX_DRIVER_VERSION);

	ret = i2c_add_driver (&aw22xxx_i2c_driver);
	if (ret) {
		pr_err ("fail to add aw22xxx device into i2c\n");
		return ret;
	}

	return 0;
}

module_init (aw22xxx_i2c_init);

static void __exit aw22xxx_i2c_exit (void)
{
	i2c_del_driver (&aw22xxx_i2c_driver);
}

module_exit (aw22xxx_i2c_exit);

MODULE_DESCRIPTION ("AW22XXX LED Driver");
MODULE_LICENSE ("GPL v2");
