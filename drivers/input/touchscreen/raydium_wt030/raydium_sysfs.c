/* drivers/input/touchscreen/raydium_wt030/raydium_sysfs.c
 *
 * Raydium TouchScreen driver.
 *
 * Copyright (c) 2010  Raydium tech Ltd.
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

#include <linux/unistd.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include "raydium_driver.h"

static ssize_t raydium_touch_calibration_show(struct device *dev,
		struct device_attribute *attr,
		char *p_i8_buf)
{
	unsigned char u8_rbuffer[1];
	int i32_ret = -1;
	unsigned char u8_retry = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	if (g_raydium_ts->is_suspend)
		pr_debug("[touch]RAD is_suspend at %s\n", __func__);

	raydium_irq_control(DISABLE);
	mutex_lock(&g_raydium_ts->lock);

	i32_ret = raydium_i2c_pda2_set_page(client,
					g_raydium_ts->is_suspend,
					RAYDIUM_PDA2_PAGE_0);
	if (i32_ret < 0)
		goto exit_i2c_error;

	u8_rbuffer[0] = RAYDIUM_HOST_CMD_CALIBRATION;
	i32_ret = raydium_i2c_pda2_write(client, RAYDIUM_PDA2_HOST_CMD_ADDR,
				     u8_rbuffer, 1);
	if (i32_ret < 0)
		goto exit_i2c_error;

	do {
		if (u8_rbuffer[0] == RAYDIUM_HOST_CMD_NO_OP)
			break;

		msleep(1000);

		i32_ret = raydium_i2c_pda2_read(client,
						RAYDIUM_PDA2_HOST_CMD_ADDR,
						u8_rbuffer, 1);
		if (i32_ret < 0)
			goto exit_i2c_error;

		pr_debug("[touch]RAD %s return 0x%02x!!\n",
				__func__, u8_rbuffer[0]);
	} while (u8_retry++ < (SYN_I2C_RETRY_TIMES * 2));

	i32_ret = scnprintf(p_i8_buf, PAGE_SIZE, "0x%02x\n", u8_rbuffer[0]);

exit_i2c_error:
	mutex_unlock(&g_raydium_ts->lock);
	raydium_irq_control(ENABLE);

	return i32_ret;
}

static ssize_t raydium_i2c_pda_access_show(struct device *dev,
		struct device_attribute *attr,
		char *p_i8_buf)
{
	unsigned char u8_rbuffer[4];
	int i32_ret = -1;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	if (g_u32_length > 4)
		return -EINVAL;

	memset(u8_rbuffer, 0x00, 4);
	mutex_lock(&g_raydium_ts->lock);
	i32_ret = raydium_i2c_pda_read(client,
				g_u32_addr,
				u8_rbuffer,
				g_u32_length);
	mutex_unlock(&g_raydium_ts->lock);
	if (i32_ret < 0)
		return i32_ret;

	return scnprintf(p_i8_buf, PAGE_SIZE, "0x%08X : 0x%02X%02X%02X%02X\n",
		 (unsigned int)g_u32_addr, u8_rbuffer[3], u8_rbuffer[2],
		 u8_rbuffer[1], u8_rbuffer[0]);
}

static ssize_t raydium_check_i2c_show(struct device *dev,
				      struct device_attribute *attr,
				      char *p_i8_buf)
{
	unsigned char u8_rbuffer[4];
	int i32_ret = -1;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	if (g_raydium_ts->is_suspend)
		pr_info("[touch]RAD is_suspend at %s\n", __func__);
	raydium_irq_control(DISABLE);
	mutex_lock(&g_raydium_ts->lock);

	if (g_u8_i2c_mode == PDA2_MODE) {
		i32_ret = raydium_i2c_pda2_set_page(client,
						 g_raydium_ts->is_suspend,
						 RAYDIUM_PDA2_PAGE_0);
		if (i32_ret < 0)
			goto exit_i2c_error;

		/*using byte mode to read 4 bytes*/
		*(unsigned long *)u8_rbuffer = (RAD_I2C_PDA_MODE_ENABLE << 24)
						| ((RAD_CHK_I2C_CMD &
						(~MASK_8BIT)) >> 8);

		i32_ret = raydium_i2c_pda2_write(client,
						RAYDIUM_PDA2_PDA_CFG_ADDR,
						u8_rbuffer,
						4);
		if (i32_ret < 0)
			goto exit_i2c_error;
		i32_ret = raydium_i2c_pda2_set_page(client,
						g_raydium_ts->is_suspend,
						RAYDIUM_PDA2_ENABLE_PDA);
		if (i32_ret < 0)
			goto exit_i2c_error;
		i32_ret = raydium_i2c_pda2_read(client,
				(unsigned char)(RAD_CHK_I2C_CMD & MASK_8BIT),
				u8_rbuffer, 4);
		if (i32_ret < 0)
			goto exit_i2c_error;
	} else {
		i32_ret = raydium_i2c_pda_read(client, RAD_CHK_I2C_CMD,
					   u8_rbuffer, 4);
		if (i32_ret < 0)
			goto exit_i2c_error;
	}

	i32_ret = scnprintf(p_i8_buf, PAGE_SIZE,
			"[touch]RAD Touch check i2c: %02X%02X\n",
			u8_rbuffer[3], u8_rbuffer[2]);

exit_i2c_error:
	mutex_unlock(&g_raydium_ts->lock);
	raydium_irq_control(ENABLE);
	return i32_ret;
}

static ssize_t raydium_hw_reset_show(struct device *dev,
				     struct device_attribute *attr,
				     char *p_i8_buf)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	int i32_ret = SUCCESS;

	pr_debug("[touch]HW reset\n");
	g_u8_resetflag = true;
	if ((g_u8_raydium_flag & ENG_MODE) == 0)
		raydium_irq_control(DISABLE);

	/*HW reset*/
	if (gpio_is_valid(g_raydium_ts->rst_gpio)) {
		gpio_set_value(g_raydium_ts->rst_gpio, 1);
		gpio_set_value(g_raydium_ts->rst_gpio, 0);
		msleep(RAYDIUM_RESET_INTERVAL_MSEC);
		gpio_set_value(g_raydium_ts->rst_gpio, 1);
	}

	g_u8_i2c_mode = PDA2_MODE;

	i32_ret = wait_irq_state(client, 1000, 2000);
	if (i32_ret != ERROR)
		msleep(25);

	if ((g_u8_raydium_flag & ENG_MODE) == 0)
		raydium_irq_control(ENABLE);

	pr_debug("Raydium HW Reset : %d\n", i32_ret);
	return scnprintf(p_i8_buf, PAGE_SIZE,
			"Raydium HW Reset : %d\n", i32_ret);
}

static ssize_t raydium_reset_control_store(struct device *dev,
		struct device_attribute *attr,
		const char *p_i8_buf, size_t count)
{
	int i32_ret = 0;
	unsigned char u8_high;

	if (g_raydium_ts->is_suspend)
		pr_debug("[touch]RAD is_suspend at %s\n", __func__);

	/* receive command line arguments string */
	if (count > 2)
		return -EINVAL;

	i32_ret = kstrtou8(p_i8_buf, 16, &u8_high);
	if (i32_ret < 0)
		return i32_ret;

	g_u8_i2c_mode = PDA2_MODE;
	g_u8_resetflag = true;

	if (u8_high) {
		pr_debug("[touch]RAD %s set reset gpio to high!!\n", __func__);
		gpio_set_value(g_raydium_ts->rst_gpio, 1);
	} else {
		pr_debug("[touch]RAD %s set reset gpio to low!!\n",  __func__);
		gpio_set_value(g_raydium_ts->rst_gpio, 0);
	}

	return count;
}

static ssize_t raydium_palm_status_show(struct device *dev,
					struct device_attribute *attr,
					char *p_i8_buf)
{
	unsigned char u8_tp_status[MAX_TCH_STATUS_PACKET_SIZE];
	unsigned char u8_tp_buf[MAX_REPORT_PACKET_SIZE];

	raydium_read_touchdata(u8_tp_status, u8_tp_buf);
	return scnprintf(p_i8_buf, PAGE_SIZE, "[touch] palm_status : %d\n",
				u8_tp_status[POS_GES_STATUS]);
}


static ssize_t raydium_palm_area_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int ret = 0;
	unsigned char w_data[1];
	unsigned char palm_area = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	/* receive command line arguments string */
	if (count > 3)
		return -EINVAL;

	ret = kstrtou8(buf, 16, &palm_area);
	pr_debug("[touch] input palm area = %d\n", palm_area);

	mutex_lock(&g_raydium_ts->lock);
	/* unlock PARM */

	ret = raydium_i2c_pda2_set_page(client, g_raydium_ts->is_suspend,
			RAYDIUM_PDA2_PAGE_0);
	if (ret < 0) {
		mutex_unlock(&g_raydium_ts->lock);
		goto exit_error;
	}

	w_data[0] = palm_area;
	ret = raydium_i2c_pda2_write(client, RAYDIUM_PDA2_PALM_AREA_ADDR,
				w_data, 1);
	if (ret < 0) {
		mutex_unlock(&g_raydium_ts->lock);
		goto exit_error;
	}

	mutex_unlock(&g_raydium_ts->lock);

exit_error:
	return count;
}

static ssize_t raydium_palm_area_show(struct device *dev,
		struct device_attribute *attr,
		char *p_i8_buf)
{
	unsigned char u8_rbuffer[4];
	int i32_ret = -1;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	if (g_raydium_ts->is_suspend)
		pr_debug("[touch]RAD is_suspend at %s\n", __func__);

	raydium_irq_control(DISABLE);
	mutex_lock(&g_raydium_ts->lock);

	i32_ret = raydium_i2c_pda2_set_page(client,
					g_raydium_ts->is_suspend,
					RAYDIUM_PDA2_PAGE_0);
	if (i32_ret < 0)
		goto exit_i2c_error;
	i32_ret = raydium_i2c_pda2_read(client, RAYDIUM_PDA2_PALM_AREA_ADDR,
				    u8_rbuffer, 4);
	if (i32_ret < 0)
		goto exit_i2c_error;
	i32_ret = scnprintf(p_i8_buf, PAGE_SIZE, "RAD Palm area : %02X\n",
					u8_rbuffer[0]);

exit_i2c_error:
	mutex_unlock(&g_raydium_ts->lock);
	raydium_irq_control(ENABLE);

	return i32_ret;
}

static ssize_t raydium_irq_state_show(struct device *dev,
				     struct device_attribute *attr,
				     char *p_i8_buf)
{
	unsigned int u32_irq_value;

	u32_irq_value = gpio_get_value(g_raydium_ts->irq_gpio);

	pr_debug("%d\n", u32_irq_value);
	return scnprintf(p_i8_buf, PAGE_SIZE, "%d", u32_irq_value);
}


static ssize_t raydium_touch_lock_store(struct device *dev,
					struct device_attribute *attr,
					const char *p_i8_buf, size_t count)
{
	int i32_ret = 0;
	unsigned char u8_mode;
	unsigned char u8_wbuffer[1];

	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	if (g_raydium_ts->is_suspend)
		pr_debug("[touch]RAD is_suspend at %s\n", __func__);

	/* receive command line arguments string */
	if (count > 2)
		return -EINVAL;

	i32_ret = kstrtou8(p_i8_buf, 16, &u8_mode);
	if (i32_ret < 0)
		return i32_ret;
	raydium_irq_control(DISABLE);
	mutex_lock(&g_raydium_ts->lock);

	switch (u8_mode) {
	case 0: /* Disable Touch lock */

		if (g_raydium_ts->is_sleep != 1)
			break;
		g_u8_resetflag = true;
		if (gpio_is_valid(g_raydium_ts->rst_gpio)) {
			gpio_set_value(g_raydium_ts->rst_gpio, 1);
			gpio_set_value(g_raydium_ts->rst_gpio, 0);
			msleep(RAYDIUM_RESET_INTERVAL_MSEC);/*5ms*/
			gpio_set_value(g_raydium_ts->rst_gpio, 1);
			msleep(RAYDIUM_RESET_DELAY_MSEC);/*100ms*/
		}
		pr_debug("[touch]RAD %s disable touch lock!!\n", __func__);

		g_raydium_ts->is_sleep = 0;
		break;

	case 1: /* Enable Touch lock */

		if (g_raydium_ts->is_sleep == 1)
			break;
		i32_ret = raydium_i2c_pda2_set_page(client,
						 g_raydium_ts->is_suspend,
						 RAYDIUM_PDA2_PAGE_0);
		if (i32_ret < 0)
			goto exit_i2c_error;
		/*fw enter sleep mode*/
		u8_wbuffer[0] = RAYDIUM_HOST_CMD_PWR_SLEEP;
		i32_ret = raydium_i2c_pda2_write(client,
						RAYDIUM_PDA2_HOST_CMD_ADDR,
						u8_wbuffer,
						1);
		if (i32_ret < 0)
			goto exit_i2c_error;

		pr_debug("[touch]RAD %s enable touch lock!!\n", __func__);
		g_raydium_ts->is_sleep = 1;
		break;
	}

exit_i2c_error:
	mutex_unlock(&g_raydium_ts->lock);
	raydium_irq_control(ENABLE);
	return count;
}

static ssize_t raydium_check_driver_version_show(struct device *dev,
		struct device_attribute *attr,
		char *p_i8_buf)
{
	return scnprintf(p_i8_buf, PAGE_SIZE, "RAD Driver Ver: 0x%X\n",
			g_u32_driver_version);
}

static ssize_t raydium_check_fw_version_show(struct device *dev,
		struct device_attribute *attr,
		char *p_i8_buf)
{
	unsigned char u8_rbuffer[4];
	int i32_ret = -1;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	unsigned int fw_version;
#ifdef FW_UPDATE_EN
	unsigned int image_version;
#endif
	if (g_raydium_ts->is_suspend)
		pr_debug("[touch]RAD is_suspend at %s\n", __func__);

	raydium_irq_control(DISABLE);
	mutex_lock(&g_raydium_ts->lock);

	i32_ret = raydium_i2c_pda2_set_page(client,
					g_raydium_ts->is_suspend,
					RAYDIUM_PDA2_PAGE_0);
	if (i32_ret < 0)
		goto exit_i2c_error;
	i32_ret = raydium_i2c_pda2_read(client, RAYDIUM_PDA2_FW_VERSION_ADDR,
				    u8_rbuffer, 4);
	if (i32_ret < 0)
		goto exit_i2c_error;
	i32_ret = scnprintf(p_i8_buf, PAGE_SIZE,
			"RAD Touch FW Ver : %02X%02X%02X%02X\n",
			u8_rbuffer[0], u8_rbuffer[1],
			u8_rbuffer[2], u8_rbuffer[3]);

	fw_version = (u8_rbuffer[0] << 24)
		| (u8_rbuffer[1] << 16)
		| (u8_rbuffer[2] << 8)
		| u8_rbuffer[3];
	pr_debug("[touch]RAD FW ver : 0x%x\n", fw_version);

#ifdef FW_UPDATE_EN
	image_version = (g_rad_para_image[0x0004] << 24) |
			(g_rad_para_image[0x0005] << 16) |
			(g_rad_para_image[0x0006] << 8) |
			g_rad_para_image[0x0007];

	pr_debug("[touch]RAD Image FW ver : 0x%x\n", image_version);

	if (fw_version != image_version)
		pr_debug("[touch]%s, FW need upgrade.\n", __func__);
#endif

exit_i2c_error:
	mutex_unlock(&g_raydium_ts->lock);
	raydium_irq_control(ENABLE);

	return i32_ret;
}

static ssize_t raydium_check_panel_version_show(struct device *dev,
		struct device_attribute *attr,
		char *p_i8_buf)
{
	unsigned char u8_rbuffer[8];
	int i32_ret = -1;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	if (g_raydium_ts->is_suspend)
		pr_debug("[touch]RAD is_suspend at %s\n", __func__);
	raydium_irq_control(DISABLE);
	mutex_lock(&g_raydium_ts->lock);

	i32_ret = raydium_i2c_pda2_set_page(client,
					g_raydium_ts->is_suspend,
					RAYDIUM_PDA2_PAGE_0);
	if (i32_ret < 0)
		goto exit_i2c_error;
	i32_ret = raydium_i2c_pda2_read(client,
					RAYDIUM_PDA2_PANEL_VERSION_ADDR,
					u8_rbuffer, 8);
	if (i32_ret < 0)
		goto exit_i2c_error;
	i32_ret = scnprintf(p_i8_buf, PAGE_SIZE,
			"RAD Touch Panel Version : %02X%02X%02X%02X%02X%02X\n",
			u8_rbuffer[0], u8_rbuffer[1], u8_rbuffer[2],
			u8_rbuffer[3], u8_rbuffer[4], u8_rbuffer[5]);

exit_i2c_error:
	mutex_unlock(&g_raydium_ts->lock);
	raydium_irq_control(ENABLE);
	return i32_ret;
}

#ifdef FW_UPDATE_EN
static ssize_t raydium_fw_upgrade_store(struct device *dev,
					struct device_attribute *attr,
					const char *p_i8_buf, size_t count)
{
	int i32_ret = 0;

	/* receive command line arguments string */
	if (count > 2)
		return -EINVAL;

	i32_ret = kstrtou8(p_i8_buf, 16, &g_u8_upgrade_type);
	if (i32_ret < 0)
		return i32_ret;

	return count;
}

static ssize_t raydium_fw_upgrade_show(struct device *dev,
				       struct device_attribute *attr,
				       char *p_i8_buf)
{
	int i32_ret = 0, i32_result = FAIL;
	unsigned char u8_mode_change;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	raydium_irq_control(DISABLE);

	pr_debug("[touch]RAD burn type is %d\n", g_u8_upgrade_type);

	if ((g_u8_raydium_flag & ENG_MODE) == 0) {
		g_u8_raydium_flag |= ENG_MODE;
		u8_mode_change = 1;
	}

	if ((g_u8_table_setting == 1) && (g_u8_table_init == 1))
		raydium_mem_table_setting();

	if (g_u8_upgrade_type == 1) {
		i32_ret = raydium_burn_fw(client);
		if (i32_ret < 0)
			goto exit_upgrade;

		i32_result = SUCCESS;
	} else if (g_u8_upgrade_type == 2) {
		i32_ret = raydium_burn_comp(client);
		if (i32_ret < 0)
			goto exit_upgrade;

		i32_result = SUCCESS;
	} else if (g_u8_upgrade_type == 4) {
		i32_ret = raydium_load_test_fw(client);
		if (i32_ret < 0)
			goto exit_upgrade;

		i32_result = SUCCESS;
	}

exit_upgrade:
	if (u8_mode_change) {
		pr_debug("[touch]g_u8_raydium_flag : %d", g_u8_raydium_flag);
		g_u8_raydium_flag &= ~ENG_MODE;
		u8_mode_change = 0;
	}
	raydium_irq_control(ENABLE);
	g_u8_upgrade_type = 0;

	return scnprintf(p_i8_buf, PAGE_SIZE,
			"FW Upgrade result : %d\n", i32_result);
}
#endif

static ssize_t raydium_i2c_pda2_page_store(struct device *dev,
		struct device_attribute *attr,
		const char *p_i8_buf, size_t count)
{
	int i32_ret = 0;
	unsigned char u8_page = 0;
	char *temp_buf, *token;
	const char *delim = " ,";
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	/* receive command line arguments string */
	if (count < 2)
		return -EINVAL;

	temp_buf = kzalloc(count + 1, GFP_KERNEL);
	if (temp_buf == NULL)
		return -ENOMEM;

	token = kzalloc(count + 1, GFP_KERNEL);
	if (token == NULL) {
		kfree(temp_buf);
		return -ENOMEM;
	}

	strlcpy(temp_buf, p_i8_buf, count);

	token = strsep(&temp_buf, delim);

	if (temp_buf) {
		pr_err("[touch]input error, extra auguments!n");
		i32_ret = -EINVAL;
		goto exit_error;
	}
	i32_ret = kstrtou8(token, 16, &u8_page);

	if (i32_ret < 0)
		goto exit_error;

	mutex_lock(&g_raydium_ts->lock);

	i32_ret = raydium_i2c_pda2_set_page(client, g_raydium_ts->is_suspend,
						u8_page);
	if (i32_ret < 0)
		goto exit_set_error;

	/* TODO: Page check, Due to ISR will change page back to Page_0.
	 *  Or disable IRQ during PDA2 access period
	 */

exit_set_error:
	mutex_unlock(&g_raydium_ts->lock);

exit_error:
	kfree(token);
	kfree(temp_buf);
	return count;
}

static ssize_t raydium_flag_show(struct device *dev,
				 struct device_attribute *attr,
				 char *p_i8_buf)
{
	pr_debug("[touch]RAD flag : %d\n", g_u8_raydium_flag);
	return scnprintf(p_i8_buf, PAGE_SIZE, "%d", g_u8_raydium_flag);
}

static ssize_t raydium_flag_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *p_i8_buf, size_t count)
{
	int i32_ret = 0;
	unsigned char u8_flag = 0;

	/* receive command line arguments string */
	if (count > 2)
		return -EINVAL;

	i32_ret = kstrtou8(p_i8_buf, 16, &u8_flag);
	if (i32_ret < 0)
		return i32_ret;

	g_u8_raydium_flag = u8_flag;
	return count;
}

#ifdef FW_UPDATE_EN
static ssize_t raydium_mem_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *p_i8_buf, size_t count)
{
	int i32_ret = 0;
	unsigned char u8_type = 0;
	unsigned int u32_image_version;

	/* receive command line arguments string */
	if (count > 2)
		return -EINVAL;

	pr_debug("[touch]%s\n", __func__);

	i32_ret = kstrtou8(p_i8_buf, 16, &u8_type);
	if (i32_ret < 0)
		return i32_ret;

	if (u8_type > 3) {
		pr_debug("[touch]Input invalid value!!\n");
		return ERROR;
	}

	if (g_rad_boot_image != NULL)
		kfree(g_rad_boot_image);
	if (g_rad_init_image != NULL)
		kfree(g_rad_init_image);
	if (g_rad_fw_image != NULL)
		kfree(g_rad_fw_image);
	if (g_rad_para_image != NULL)
		kfree(g_rad_para_image);
	if (g_rad_testfw_image != NULL)
		kfree(g_rad_testfw_image);
	if (g_rad_testpara_image != NULL)
		kfree(g_rad_testpara_image);

	if (!raydium_id_init(u8_type)) {
		pr_debug("[touch]Set Raydium id failed!\n");
		return count;
	}

	raydium_mem_table_init(g_raydium_ts->id);
	if (raydium_mem_table_setting()) {
		u32_image_version = (g_rad_para_image[0x0004] << 24) |
			(g_rad_para_image[0x0005] << 16) |
			(g_rad_para_image[0x0006] << 8) |
			g_rad_para_image[0x0007];

		pr_debug("[touch]RAD Image FW ver : 0x%x\n", u32_image_version);
	} else
		pr_debug("[touch]Mem init failed!\n");

	return count;
}
#endif

static ssize_t raydium_i2c_raw_data_store(struct device *dev,
		struct device_attribute *attr,
		const char *p_i8_buf, size_t count)
{
	int i32_ret = 0;
	char *temp_buf, *token;
	const char *delim = " ,";
	unsigned char u8_w_data[RAD_FT_CMD_LENGTH];

	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	/* receive command line arguments string */
	if (count < 2)
		return -EINVAL;

	temp_buf = kzalloc(count + 1, GFP_KERNEL);
	if (temp_buf  == NULL)
		return -ENOMEM;

	token = kzalloc(count + 1, GFP_KERNEL);
	if (token  == NULL) {
		kfree(temp_buf);
		return -ENOMEM;
	}

	strlcpy(temp_buf, p_i8_buf, count);

	token = strsep(&temp_buf, delim);

	i32_ret = kstrtou8(token, 16, &g_u8_raw_data_type);

	token = strsep(&temp_buf, delim);
	if (token) {
		i32_ret = kstrtouint(token, 16, &g_u32_raw_data_len);
		if (i32_ret < 0)
			goto exit_error;


	} else { /* without length info*/
		i32_ret = -EINVAL;
		goto exit_error;
	}

	if (temp_buf) { /* too much arguments*/
		i32_ret = -E2BIG;
		goto exit_error;
	}

	memset(u8_w_data, 0x00, RAD_FT_CMD_LENGTH);

	mutex_lock(&g_raydium_ts->lock);
	i32_ret = raydium_i2c_pda2_set_page(client,
					g_raydium_ts->is_suspend,
					RAYDIUM_PDA2_PAGE_0);
	if (i32_ret < 0) {
		mutex_unlock(&g_raydium_ts->lock);
		goto exit_error;
	}

	g_u8_resetflag = true;

	u8_w_data[RAD_HOST_CMD_POS] = RAYDIUM_HOST_CMD_NO_OP;
	u8_w_data[RAD_FT_CMD_POS] = g_u8_raw_data_type;

	i32_ret = raydium_i2c_pda2_write(client, RAYDIUM_PDA2_HOST_CMD_ADDR,
						u8_w_data, RAD_FT_CMD_LENGTH);
	mutex_unlock(&g_raydium_ts->lock);
	if (i32_ret < 0)
		goto exit_error;

	if (g_u8_raw_data_type == 0) {
		usleep_range(10000, 20000);
		g_u8_resetflag = false;
	}
exit_error:
	kfree(token);
	kfree(temp_buf);

	return count;
}
static ssize_t raydium_i2c_raw_data_show(struct device *dev,
		struct device_attribute *attr,
		char *p_i8_buf)
{
	unsigned char u8_rbuffer[MAX_READ_PACKET_SIZE];
	unsigned int u32_target_addr;
	unsigned int u32_offset;
	unsigned short u16_read_length;

	int i32_ret = -1;
	int i32_retry = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct raydium_ts_data *ts =
		(struct raydium_ts_data *)i2c_get_clientdata(client);
	unsigned char u8_retry_limit = (ts->is_suspend) ? 30 : 3;

	memset(u8_rbuffer, 0x00, MAX_READ_PACKET_SIZE);

	/* make sure update flag was set*/
	for (i32_retry = 0; i32_retry < u8_retry_limit; i32_retry++) {
		mutex_lock(&ts->lock);
		i32_ret = raydium_i2c_pda2_set_page(client,
						ts->is_suspend,
						RAYDIUM_PDA2_PAGE_0);
		if (i32_ret < 0)
			goto exit_i2c_error;

		i32_ret = raydium_i2c_pda2_read(client,
						RAYDIUM_PDA2_HOST_CMD_ADDR,
						u8_rbuffer,
						RAD_FT_CMD_LENGTH);
		mutex_unlock(&ts->lock);
		if (i32_ret < 0)
			goto exit_flag_error;

		if ((u8_rbuffer[RAD_FT_CMD_POS] & RAYDIUM_FT_UPDATE)
				== RAYDIUM_FT_UPDATE)
			break;

		usleep_range(4500, 5500);
	}

	if (i32_retry == u8_retry_limit) {
		i32_ret = -EAGAIN;
		goto exit_flag_error;
	}

	u32_offset = 0;
	u16_read_length = 0;
	while (u32_offset < g_u32_raw_data_len) {
		if ((u32_offset + MAX_READ_PACKET_SIZE) <
				g_u32_raw_data_len)
			u16_read_length = MAX_READ_PACKET_SIZE;
		else
			u16_read_length =
				(unsigned short)(g_u32_raw_data_len
							- u32_offset);

		u32_target_addr = RAD_READ_FT_DATA_CMD + u32_offset;

		mutex_lock(&(ts->lock));
		i32_ret = raydium_i2c_pda2_set_page(client,
						ts->is_suspend,
						RAYDIUM_PDA2_PAGE_0);
		if (i32_ret < 0)
			goto exit_i2c_error;

		*(unsigned int *)u8_rbuffer = (RAD_I2C_PDA_MODE_ENABLE << 24)
						| ((u32_target_addr
						& (~MASK_8BIT)) >> 8);

		/*using byte mode to read 4 bytes*/
		i32_ret = raydium_i2c_pda2_write(client,
				RAYDIUM_PDA2_PDA_CFG_ADDR, u8_rbuffer, 4);
		if (i32_ret < 0)
			goto exit_i2c_error;

		i32_ret = raydium_i2c_pda2_set_page(client,
				ts->is_suspend,
				RAYDIUM_PDA2_ENABLE_PDA);
		if (i32_ret < 0)
			goto exit_i2c_error;

		i32_ret = raydium_i2c_pda2_read(client,
				(unsigned char)(u32_target_addr & MASK_8BIT),
				u8_rbuffer,
				u16_read_length);

		mutex_unlock(&(ts->lock));
		if (i32_ret < 0)
			goto exit_flag_error;

		memcpy((p_i8_buf + u32_offset), u8_rbuffer, u16_read_length);

		u32_offset += u16_read_length;
	}

	/* clear update flag to get next one*/
	u8_rbuffer[RAD_HOST_CMD_POS] = RAYDIUM_HOST_CMD_NO_OP;
	u8_rbuffer[RAD_FT_CMD_POS] = g_u8_raw_data_type;
	mutex_lock(&ts->lock);
	i32_ret = raydium_i2c_pda2_write(client, RAYDIUM_PDA2_HOST_CMD_ADDR,
				     u8_rbuffer, RAD_FT_CMD_LENGTH);
	mutex_unlock(&ts->lock);
	if (i32_ret < 0)
		goto exit_flag_error;

	return g_u32_raw_data_len;
exit_i2c_error:
	mutex_unlock(&(ts->lock));
exit_flag_error:
	return i32_ret;
}
static ssize_t raydium_i2c_pda_access_store(struct device *dev,
		struct device_attribute *attr,
		const char *p_i8_buf, size_t count)
{
	int i32_ret = 0;
	char *temp_buf, *token;
	const char *delim = " ,";
	unsigned char u8_w_data[MAX_WRITE_PACKET_SIZE];
	unsigned int u32_data_count = 0;
	unsigned int u32_data_index = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	/* receive command line arguments string */
	if (count < 2)
		return -EINVAL;

	temp_buf = kzalloc(count + 1, GFP_KERNEL);
	if (temp_buf == NULL)
		return -ENOMEM;

	token = kzalloc(count + 1, GFP_KERNEL);
	if (token == NULL) {
		kfree(temp_buf);
		return -ENOMEM;
	}

	strlcpy(temp_buf, p_i8_buf, count);

	token = strsep(&temp_buf, delim);

	i32_ret = kstrtoul(token, 16, &g_u32_addr);

	token = strsep(&temp_buf, delim);
	if (token)
		i32_ret = kstrtouint(token, 16, &u32_data_count);
	else
		goto exit_error;

	if (g_u32_length > MAX_WRITE_PACKET_SIZE)
		goto exit_error;

	g_u32_length = u32_data_count;

	memset(u8_w_data, 0x00, MAX_WRITE_PACKET_SIZE);

	if (temp_buf && u32_data_count) {
		u32_data_index = 0;
		while (u32_data_count) {
			token = strsep(&temp_buf, delim);
			i32_ret = kstrtou8(token, 16,
					&u8_w_data[u32_data_index++]);
			if (i32_ret < 0)
				goto exit_error;
			u32_data_count--;
		}
		mutex_lock(&g_raydium_ts->lock);
		i32_ret = raydium_i2c_pda_write(client, g_u32_addr,
					       u8_w_data, g_u32_length);
		mutex_unlock(&g_raydium_ts->lock);
	}

exit_error:
	kfree(token);
	kfree(temp_buf);
	return count;
}

static ssize_t raydium_i2c_pda2_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *p_i8_buf, size_t count)
{
	int i32_ret = 0;
	unsigned char u8_mode;

	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	if (g_raydium_ts->is_suspend)
		pr_debug("[touch]RAD is_suspend at %s\n", __func__);

	/* receive command line arguments string */
	if (count > 2)
		return -EINVAL;

	i32_ret = kstrtou8(p_i8_buf, 16, &u8_mode);
	if (i32_ret < 0)
		return i32_ret;

	i32_ret = raydium_i2c_mode_control(client, u8_mode);
	if (i32_ret < 0)
		return i32_ret;

	return count;
}

static ssize_t raydium_i2c_pda2_access_show(struct device *dev,
		struct device_attribute *attr,
		char *p_i8_buf)
{
	unsigned char u8_rbuffer[4];
	int i32_ret = -1;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	if (g_u32_length > 4)
		return -EINVAL;

	memset(u8_rbuffer, 0x00, 4);

	mutex_lock(&g_raydium_ts->lock);
	i32_ret = raydium_i2c_pda2_read(client, g_u8_addr,
					u8_rbuffer, g_u32_length);
	mutex_unlock(&g_raydium_ts->lock);
	if (i32_ret < 0)
		return i32_ret;

	return scnprintf(p_i8_buf, PAGE_SIZE, "0x%04X : 0x%02X%02X%02X%02X\n",
			g_u8_addr, u8_rbuffer[3], u8_rbuffer[2],
			u8_rbuffer[1], u8_rbuffer[0]);
}

static ssize_t raydium_i2c_pda2_access_store(struct device *dev,
		struct device_attribute *attr,
		const char *p_i8_buf, size_t count)
{
	int i32_ret = 0;
	char *temp_buf, *token;
	const char *delim = " ,";
	unsigned char u8_w_data[MAX_WRITE_PACKET_SIZE];
	unsigned int u32_data_count = 0;
	unsigned int u32_data_index = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	/* receive command line arguments string */
	if (count < 2)
		return -EINVAL;

	temp_buf = kzalloc(count + 1, GFP_KERNEL);
	if (temp_buf  == NULL)
		return -ENOMEM;

	token = kzalloc(count + 1, GFP_KERNEL);
	if (token  == NULL) {
		kfree(temp_buf);
		return -ENOMEM;
	}

	strlcpy(temp_buf, p_i8_buf, count);

	token = strsep(&temp_buf, delim);

	i32_ret = kstrtou8(token, 16, &g_u8_addr);

	token = strsep(&temp_buf, delim);
	if (token)
		i32_ret = kstrtouint(token, 16, &u32_data_count);
	else {
		i32_ret = -EINVAL;
		goto exit_error;
	}

	if (u32_data_count > MAX_WRITE_PACKET_SIZE) {
		i32_ret = -EINVAL;
		goto exit_error;
	}

	memset(u8_w_data, 0x00, MAX_WRITE_PACKET_SIZE);

	g_u32_length = u32_data_count;

	if (temp_buf && u32_data_count) {
		u32_data_index = 0;
		while (u32_data_count) {
			token = strsep(&temp_buf, delim);
			i32_ret = kstrtou8(token, 16,
					 &u8_w_data[u32_data_index++]);
			if (i32_ret < 0)
				goto exit_error;
			u32_data_count--;
		}

		mutex_lock(&g_raydium_ts->lock);
		i32_ret = raydium_i2c_pda2_write(client, g_u8_addr,
						u8_w_data, g_u32_length);
		mutex_unlock(&g_raydium_ts->lock);
		if (i32_ret < 0)
			goto exit_error;
	}

exit_error:
	kfree(token);
	kfree(temp_buf);
	return count;
}

#ifdef FW_UPDATE_EN
static ssize_t raydium_receive_fw_store(struct device *dev,
					struct device_attribute *attr,
					const char *p_i8_buf, size_t count)
{
	int i32_ret = 0;
	const char *delim = " ,";
	char *token, *temp_buf;
	static unsigned char *p_u8_firmware_data;

	unsigned char u8_cmd;
	unsigned long u32_len;
	static unsigned char u8_type;
	static unsigned int u32_index;

	if (count == 20) { /*check FW type*/
		temp_buf = kzalloc(32, GFP_KERNEL);
		if (temp_buf  == NULL) {
			pr_err("[touch]kzalloc temp_buf failed\n");
			return -ENOMEM;
		}

		token = kzalloc(32, GFP_KERNEL);
		if (token == NULL) {
			kfree(temp_buf);
			return -ENOMEM;
		}

		snprintf(temp_buf, PAGE_SIZE, "%s", p_i8_buf);
		token = strsep(&temp_buf, delim);
		i32_ret = kstrtou8(token, 16, &u8_cmd);
		if (i32_ret < 0) {
			pr_err("[touch]kstrtou8 failed\n");
			goto exit_error;
		}

		token = strsep(&temp_buf, delim);
		i32_ret = kstrtou8(token, 16, &u8_type);
		if (i32_ret < 0) {
			pr_err("[touch]kstrtou8 failed\n");
			goto exit_error;
		}

		token = strsep(&temp_buf, delim);
		i32_ret = kstrtoul(token, 16, &u32_len);
		if (i32_ret < 0) {
			pr_err("[touch]kstrtou8 failed\n");
			goto exit_error;
		}

		pr_debug("[touch]uc_cmd=0x%x, uc_type=0x%x, u16_len=0x%x\n",
			u8_cmd, u8_type, (unsigned int)u32_len);

		if (u8_cmd == RAD_CMD_UPDATE_BIN) { /*check FW length*/
			u32_index = 0;
			if (u8_type == RAYDIUM_BOOTLOADER) {
				memset(g_rad_boot_image, 0, u32_len);
				p_u8_firmware_data = g_rad_boot_image;
			} else if (u8_type == RAYDIUM_INIT) {
				memset(g_rad_init_image, 0, u32_len);
				p_u8_firmware_data = g_rad_init_image;
			} else if (u8_type == RAYDIUM_PARA) {
				memset(g_rad_para_image, 0, u32_len);
				p_u8_firmware_data = g_rad_para_image;
			} else if (u8_type == RAYDIUM_FIRMWARE) {
				memset(g_rad_fw_image, 0, u32_len);
				p_u8_firmware_data = g_rad_fw_image;
			} else if (u8_type == RAYDIUM_TEST_PARA) {
				memset(g_rad_testpara_image, 0, u32_len);
				p_u8_firmware_data = g_rad_testpara_image;
			} else if (u8_type == RAYDIUM_TEST_FW) {
				memset(g_rad_testfw_image, 0, u32_len);
				p_u8_firmware_data = g_rad_testfw_image;
			}

		} else if (u8_cmd == RAD_CMD_UPDATE_END) { /*set buffer finish*/
			if (((g_raydium_ts->id & 0x2000) == 0x2000)
					&& (u8_type == RAYDIUM_TEST_FW)) {
				memcpy((g_rad_testfw_image + RAD_FW_2X_SIZE),
						g_rad_testpara_image,
						RAD_PARA_2X_SIZE + 4);
			}

			u32_index = 0;
			g_u8_table_setting = 0;

		} else if (u8_cmd == RAD_CMD_BURN_FINISH) { /*free buffer*/
			u8_type = 0;
			u32_index = 0;
			g_u8_table_setting = 1;
		}

	} else  if (count > 10) {	/*start copy FW to array*/
		memcpy((p_u8_firmware_data + u32_index), p_i8_buf, count);
		u32_index += count;
	} else
		pr_debug("[touch]other case, count=%d\n", count);

exit_error:
	kfree(temp_buf);
	kfree(token);
	return count;
}
#endif

#ifdef RAD_SELFTEST
static ssize_t raydium_selftest_show(struct device *dev,
				     struct device_attribute *attr,
				     char *p_i8_buf)
{
	int i32_ret = SUCCESS;

	pr_debug("[touch]do selftest\n");

	i32_ret = raydium_do_selftest();

	return scnprintf(p_i8_buf, PAGE_SIZE,
			"Raydium do selftest : %d\n", i32_ret);
}

#endif
/* panel calibration cmd (R)
 *  example:cat raydium_ic_verion
 */
static DEVICE_ATTR(raydium_touch_calibration, 0644,
		   raydium_touch_calibration_show,
		   NULL);

/* check the i2c (R)
 *  example:cat raydium_check_i2c
 */
static DEVICE_ATTR(raydium_check_i2c, 0644,
		   raydium_check_i2c_show,
		   NULL);

#ifdef FW_UPDATE_EN
/* upgrade configurate and algo firmware from app.bin (W)
 *  example:echo "offset num_of_bin length *_app.bin [length *_app.bin]"
 *  > raydium_fw_upgrade_mode
 */
static DEVICE_ATTR(raydium_fw_upgrade, 0644,
		   raydium_fw_upgrade_show,
		   raydium_fw_upgrade_store);
#endif

/* change I2C communication mode (W)
 *  example:echo 1 > raydium_i2c_pda2_mode ==> enable pda2 mode
 *        echo 0 > raydium_i2c_pda2_mode ==> disable pda2 mode
 */
static DEVICE_ATTR(raydium_i2c_pda2_mode, 0644,
		   NULL,
		   raydium_i2c_pda2_mode_store);

/* I2C pda mode (R/W)
 *  example:    cat raydium_i2c_pda_access ==> read pda address provided by the
 *		following cmd
 *		echo ADDRinHEX [DATAinHEX] > raydium_i2c_pda_access ==> write
 *		pda address [data]
 */
static DEVICE_ATTR(raydium_i2c_pda_access, 0644,
		   raydium_i2c_pda_access_show,
		   raydium_i2c_pda_access_store);

/* I2C pda2 mode (R/W)
 *  example:    cat raydium_i2c_pda2_access ==> read pda2 address provided by
 *		the following cmd
 *		echo ADDRinHEX [DATAinHEX] > raydium_i2c_pda2_access ==>
 *		write pda2 address [data]
 */
static DEVICE_ATTR(raydium_i2c_pda2_access, 0644,
		   raydium_i2c_pda2_access_show,
		   raydium_i2c_pda2_access_store);

/* I2C pda2 mode page (W)
 *  example:    echo PAGEinHEX > raydium_i2c_pda2_page ==> write pda2 page
 */
static DEVICE_ATTR(raydium_i2c_pda2_page, 0644,
		   NULL,
		   raydium_i2c_pda2_page_store);

/* I2C read/set FT raw data (R/W)
 *  example:    cat raydium_i2c_raw_data ==> read raw data with specific length
 *		of corresponding type provided by the following cmd
 *              echo DataTypeinHEX RawDataLengthinHEX > raydium_i2c_raw_data
 *		==> set raw data type and its length
 */
static DEVICE_ATTR(raydium_i2c_raw_data, 0644,
		   raydium_i2c_raw_data_show,
		   raydium_i2c_raw_data_store);

/* Read interrupt flag cmd (R)
 *  example:cat raydium_flag
 */
static DEVICE_ATTR(raydium_flag, 0644,
		   raydium_flag_show,
		   raydium_flag_store);

/* Read interrupt flag cmd (R)
 *  example:cat raydium_int_flag
 */
static DEVICE_ATTR(raydium_int_flag, 0644,
		   raydium_flag_show,
		   raydium_flag_store);

/* Read selftest flag cmd (R)
 *  example:cat raydium_int_flag
 */
static DEVICE_ATTR(raydium_selftest_flag, 0644,
		   raydium_flag_show,
		   raydium_flag_store);

/* Touch lock (W)
 *  example:    echo 1 > raydium_i2c_touch_lock ==> enable touch lock
 *            echo 0 > raydium_i2c_touch_lock ==> disable touch lock
 */
static DEVICE_ATTR(raydium_i2c_touch_lock, 0644,
		   NULL,
		   raydium_touch_lock_store);

/* show the fw version (R)
 *  example:cat raydium_fw_version
 */
static DEVICE_ATTR(raydium_check_fw_version, 0644,
		   raydium_check_fw_version_show,
		   NULL);

/* show the driver version (R)
 *  example:cat raydium_check_driver_version
 */
static DEVICE_ATTR(raydium_check_driver_version, 0644,
		   raydium_check_driver_version_show,
		   NULL);
/* show the panel version (R)
 *  example:cat raydium_panel_version
 */
static DEVICE_ATTR(raydium_check_panel_version, 0644,
		   raydium_check_panel_version_show,
		   NULL);

static DEVICE_ATTR(raydium_hw_reset, 0644,
		   raydium_hw_reset_show,
		   NULL);

static DEVICE_ATTR(raydium_irq_state, 0644,
		   raydium_irq_state_show,
		   NULL);
static DEVICE_ATTR(raydium_palm_status, 0644,
		   raydium_palm_status_show,
		   NULL);
static DEVICE_ATTR(raydium_palm_area, 0644,
		   raydium_palm_area_show,
		   raydium_palm_area_store);
static DEVICE_ATTR(raydium_reset_control, 0644,
		   NULL,
		   raydium_reset_control_store);
#ifdef FW_UPDATE_EN
static DEVICE_ATTR(raydium_receive_fw_control, 0644,
		   NULL,
		   raydium_receive_fw_store);
static DEVICE_ATTR(raydium_mem_setting, 0644,
		   NULL,
		   raydium_mem_store);
#endif
#ifdef RAD_SELFTEST
static DEVICE_ATTR(raydium_do_selftest, 0644,
		   raydium_selftest_show,
		   NULL);
#endif
/*add your attr in here*/
struct attribute *raydium_attributes[] = {
	&dev_attr_raydium_touch_calibration.attr,
	&dev_attr_raydium_check_i2c.attr,
	&dev_attr_raydium_i2c_pda2_mode.attr,
	&dev_attr_raydium_i2c_pda_access.attr,
	&dev_attr_raydium_i2c_pda2_access.attr,
	&dev_attr_raydium_i2c_pda2_page.attr,
	&dev_attr_raydium_i2c_raw_data.attr,
	&dev_attr_raydium_flag.attr,
	&dev_attr_raydium_i2c_touch_lock.attr,
	&dev_attr_raydium_check_fw_version.attr,
	&dev_attr_raydium_check_panel_version.attr,
	&dev_attr_raydium_hw_reset.attr,
	&dev_attr_raydium_irq_state.attr,
	&dev_attr_raydium_palm_status.attr,
	&dev_attr_raydium_palm_area.attr,
	&dev_attr_raydium_int_flag.attr,
	&dev_attr_raydium_selftest_flag.attr,
	&dev_attr_raydium_check_driver_version.attr,
	&dev_attr_raydium_reset_control.attr,
#ifdef FW_UPDATE_EN
	&dev_attr_raydium_fw_upgrade.attr,
	&dev_attr_raydium_receive_fw_control.attr,
	&dev_attr_raydium_mem_setting.attr,
#endif
#ifdef RAD_SELFTEST
	&dev_attr_raydium_do_selftest.attr,
#endif
	NULL
};

MODULE_AUTHOR("Raydium");
MODULE_DESCRIPTION("Raydium TouchScreen driver");
MODULE_LICENSE("GPL");
