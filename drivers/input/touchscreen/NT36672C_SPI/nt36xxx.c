/*
 * Copyright (C) 2010 - 2021 Novatek, Inc.
 *
 * $Revision: 106431 $
 * $Date: 2022-10-04 09:03:03 +0800 (週二, 04 十月 2022) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include "nt36xxx.h"
#include <linux/hardware_info.h>


#if IS_ENABLED(CONFIG_XIAOMI_PANEL_NOTIFIER)
#include <misc/xiaomi_panel_notifier.h>
#elif defined(CONFIG_DRM_MSM)
#include <linux/msm_drm_notify.h>
#elif defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE)
#include "../xiaomi_touch/xiaomi_touch.h"
#endif

#if IS_ENABLED(CONFIG_XIAOMI_USB_TOUCH_NOTIFIER)
#include <misc/xiaomi_usb_touch_notifier.h>
#endif


#if NVT_TOUCH_ESD_PROTECT
#include <linux/jiffies.h>
#endif /* #if NVT_TOUCH_ESD_PROTECT */

int nvt_charger_flag;

char tp_hardware_info[HARDWARE_MAX_ITEM_LONGTH];
extern void set_tpinfo_gki(char *TPINFO);

extern wait_queue_head_t proximity_switch_queue;
extern  int32_t nvt_set_proximity_switch(uint8_t proximity_switch);
extern int32_t	g_priximity_enable;
int  g_priximity_data = -1;
int32_t g_new_proximity_event_flag;

static int32_t nvt_ts_resume(struct device *dev);
static int32_t nvt_ts_suspend(struct device *dev);

static int32_t nvt_check_palm(uint8_t input_id, uint8_t *data);


#if NVT_TOUCH_ESD_PROTECT
static struct delayed_work nvt_esd_check_work;
static struct workqueue_struct *nvt_esd_check_wq;
static unsigned long irq_timer = 0;
uint8_t esd_check = false;
uint8_t esd_retry = 0;
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#if NVT_TOUCH_EXT_PROC
extern int32_t nvt_extra_proc_init(void);
extern void nvt_extra_proc_deinit(void);
#endif

#if NVT_TOUCH_MP
extern int32_t nvt_mp_proc_init(void);
extern void nvt_mp_proc_deinit(void);
#endif

struct nvt_ts_data *ts;
bool g_gesture_flag;
#if IS_ENABLED(CONFIG_XIAOMI_TOUCH_NOTIFIER)
	static struct xiaomi_touch_notify_data touch_gesture_data;
#endif



#if BOOT_UPDATE_FIRMWARE
static struct workqueue_struct *nvt_fwu_wq;
extern void Boot_Update_Firmware(struct work_struct *work);
#endif

#if IS_ENABLED(CONFIG_XIAOMI_PANEL_NOTIFIER)
static int nvt_xiaomi_panel_notifier_callback(struct notifier_block *self,
											unsigned long event, void *data);
#elif defined(_MSM_DRM_NOTIFY_H_)
static int nvt_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#elif defined(CONFIG_FB)
static int nvt_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void nvt_ts_early_suspend(struct early_suspend *h);
static void nvt_ts_late_resume(struct early_suspend *h);
#endif

uint32_t ENG_RST_ADDR  = 0x7FFF80;
uint32_t SWRST_SIF_ADDR = 0; //read from dtsi
uint32_t SPI_RD_FAST_ADDR = 0;	//read from dtsi

#if TOUCH_KEY_NUM > 0
const uint16_t touch_key_array[TOUCH_KEY_NUM] = {
	KEY_BACK,
	KEY_HOME,
	KEY_MENU
};
#endif

#if WAKEUP_GESTURE
const uint16_t gesture_key_array[] = {
	KEY_POWER,  //GESTURE_WORD_C
	KEY_POWER,  //GESTURE_WORD_W
	KEY_POWER,  //GESTURE_WORD_V
	NVT_KEY_DOUBLE_CLICK,  //GESTURE_DOUBLE_CLICK
	KEY_POWER,  //GESTURE_WORD_Z
	KEY_POWER,  //GESTURE_WORD_M
	KEY_POWER,  //GESTURE_WORD_O
	KEY_POWER,  //GESTURE_WORD_e
	KEY_POWER,  //GESTURE_WORD_S
	KEY_POWER,  //GESTURE_SLIDE_UP
	KEY_POWER,  //GESTURE_SLIDE_DOWN
	KEY_POWER,  //GESTURE_SLIDE_LEFT
	KEY_POWER,  //GESTURE_SLIDE_RIGHT
};
#endif

#ifdef CONFIG_MTK_SPI
const struct mt_chip_conf spi_ctrdata = {
	.setuptime = 25,
	.holdtime = 25,
	.high_time = 5,	/* 10MHz (SPI_SPEED=100M / (high_time+low_time(10ns)))*/
	.low_time = 5,
	.cs_idletime = 2,
	.ulthgh_thrsh = 0,
	.cpol = 0,
	.cpha = 0,
	.rx_mlsb = 1,
	.tx_mlsb = 1,
	.tx_endian = 0,
	.rx_endian = 0,
	.com_mod = DMA_TRANSFER,
	.pause = 0,
	.finish_intr = 1,
	.deassert = 0,
	.ulthigh = 0,
	.tckdly = 0,
};
#endif

#ifdef CONFIG_SPI_MT65XX
const struct mtk_chip_config spi_ctrdata = {
    .rx_mlsb = 1,
    .tx_mlsb = 1,
    .cs_pol = 0,
};
#endif

static uint8_t bTouchIsAwake = 0;

/*******************************************************
Description:
	Novatek touchscreen irq enable/disable function.

return:
	n.a.
*******************************************************/
static void nvt_irq_enable(bool enable)
{
	struct irq_desc *desc;

	if (enable) {
		if (!ts->irq_enabled) {
			enable_irq(ts->client->irq);
			ts->irq_enabled = true;
		}
	} else {
		if (ts->irq_enabled) {
			disable_irq(ts->client->irq);
			ts->irq_enabled = false;
		}
	}

	desc = irq_to_desc(ts->client->irq);
	NVT_LOG("enable=%d, desc->depth=%d\n", enable, desc->depth);
}

static void nvt_enable_irq_wake(bool enable)
{
	struct irq_desc *desc;

	if (enable) {
		if (!ts->irq_enable_wake) {
			enable_irq_wake(ts->client->irq);
			ts->irq_enable_wake = true;
		}
	} else {
		if (ts->irq_enable_wake) {
			disable_irq_wake(ts->client->irq);
			ts->irq_enable_wake = false;
		}
	}

	desc = irq_to_desc(ts->client->irq);
	NVT_LOG("enable=%d, desc->depth=%d\n", enable, desc->depth);
}

/*******************************************************
Description:
	Novatek touchscreen spi read/write core function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static inline int32_t spi_read_write(struct spi_device *client, uint8_t *buf, size_t len , NVT_SPI_RW rw)
{
	struct spi_message m;
	struct spi_transfer t = {
		.len    = len,
	};

	memset(ts->xbuf, 0, len + DUMMY_BYTES);
	memcpy(ts->xbuf, buf, len);

	switch (rw) {
		case NVTREAD:
			t.tx_buf = ts->xbuf;
			t.rx_buf = ts->rbuf;
			t.len    = (len + DUMMY_BYTES);
			break;

		case NVTWRITE:
			t.tx_buf = ts->xbuf;
			break;
	}

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spi_sync(client, &m);
}

/*******************************************************
Description:
	Novatek touchscreen spi read function.

return:
	Executive outcomes. 2---succeed. -5---I/O error
*******************************************************/
int32_t CTP_SPI_READ(struct spi_device *client, uint8_t *buf, uint16_t len)
{
	int32_t ret = -1;
	int32_t retries = 0;

	mutex_lock(&ts->xbuf_lock);

	buf[0] = SPI_READ_MASK(buf[0]);

	while (retries < 5) {
		ret = spi_read_write(client, buf, len, NVTREAD);
		if (ret == 0) break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		NVT_ERR("read error, ret=%d\n", ret);
		ret = -EIO;
	} else {
		memcpy((buf+1), (ts->rbuf+2), (len-1));
	}

	mutex_unlock(&ts->xbuf_lock);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen spi write function.

return:
	Executive outcomes. 1---succeed. -5---I/O error
*******************************************************/
int32_t CTP_SPI_WRITE(struct spi_device *client, uint8_t *buf, uint16_t len)
{
	int32_t ret = -1;
	int32_t retries = 0;

	mutex_lock(&ts->xbuf_lock);

	buf[0] = SPI_WRITE_MASK(buf[0]);

	while (retries < 5) {
		ret = spi_read_write(client, buf, len, NVTWRITE);
		if (ret == 0)	break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		NVT_ERR("error, ret=%d\n", ret);
		ret = -EIO;
	}

	mutex_unlock(&ts->xbuf_lock);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen set index/page/addr address.

return:
	Executive outcomes. 0---succeed. -5---access fail.
*******************************************************/
int32_t nvt_set_page(uint32_t addr)
{
	uint8_t buf[4] = {0};

	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 15) & 0xFF;
	buf[2] = (addr >> 7) & 0xFF;

	return CTP_SPI_WRITE(ts->client, buf, 3);
}

/*******************************************************
Description:
	Novatek touchscreen write data to specify address.

return:
	Executive outcomes. 0---succeed. -5---access fail.
*******************************************************/
int32_t nvt_write_addr(uint32_t addr, uint8_t data)
{
	int32_t ret = 0;
	uint8_t buf[4] = {0};

	//---set xdata index---
	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 15) & 0xFF;
	buf[2] = (addr >> 7) & 0xFF;
	ret = CTP_SPI_WRITE(ts->client, buf, 3);
	if (ret) {
		NVT_ERR("set page 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	//---write data to index---
	buf[0] = addr & (0x7F);
	buf[1] = data;
	ret = CTP_SPI_WRITE(ts->client, buf, 2);
	if (ret) {
		NVT_ERR("write data to 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen enable hw bld crc function.

return:
	N/A.
*******************************************************/
void nvt_bld_crc_enable(void)
{
	uint8_t buf[4] = {0};

	//---set xdata index to BLD_CRC_EN_ADDR---
	nvt_set_page(ts->mmap->BLD_CRC_EN_ADDR);

	//---read data from index---
	buf[0] = ts->mmap->BLD_CRC_EN_ADDR & (0x7F);
	buf[1] = 0xFF;
	CTP_SPI_READ(ts->client, buf, 2);

	//---write data to index---
	buf[0] = ts->mmap->BLD_CRC_EN_ADDR & (0x7F);
	buf[1] = buf[1] | (0x01 << 7);
	CTP_SPI_WRITE(ts->client, buf, 2);
}

/*******************************************************
Description:
	Novatek touchscreen clear status & enable fw crc function.

return:
	N/A.
*******************************************************/
void nvt_fw_crc_enable(void)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	//---clear fw reset status---
	buf[0] = EVENT_MAP_RESET_COMPLETE & (0x7F);
	buf[1] = 0x00;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = 0x00;
	buf[5] = 0x00;
	buf[6] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 7);

	//---enable fw crc---
	buf[0] = EVENT_MAP_HOST_CMD & (0x7F);
	buf[1] = 0xAE;	//enable fw crc command
	buf[2] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 3);
}

/*******************************************************
Description:
	Novatek touchscreen set boot ready function.

return:
	N/A.
*******************************************************/
void nvt_boot_ready(void)
{
	//---write BOOT_RDY status cmds---
	nvt_write_addr(ts->mmap->BOOT_RDY_ADDR, 1);

	mdelay(5);

	if (!ts->hw_crc) {
		//---write BOOT_RDY status cmds---
		nvt_write_addr(ts->mmap->BOOT_RDY_ADDR, 0);

		//---write POR_CD cmds---
		nvt_write_addr(ts->mmap->POR_CD_ADDR, 0xA0);
	}
}

/*******************************************************
Description:
	Novatek touchscreen enable auto copy mode function.

return:
	N/A.
*******************************************************/
void nvt_tx_auto_copy_mode(void)
{
	//---write TX_AUTO_COPY_EN cmds---
	nvt_write_addr(ts->mmap->TX_AUTO_COPY_EN, 0x69);

	NVT_ERR("tx auto copy mode enable\n");
}

/*******************************************************
Description:
	Novatek touchscreen check spi dma tx info function.

return:
	N/A.
*******************************************************/
int32_t nvt_check_spi_dma_tx_info(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 200;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->SPI_DMA_TX_INFO);

		//---read fw status---
		buf[0] = ts->mmap->SPI_DMA_TX_INFO & 0x7F;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(1000, 1000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen eng reset cmd
    function.

return:
	n.a.
*******************************************************/
void nvt_eng_reset(void)
{
	//---eng reset cmds to ENG_RST_ADDR---
	nvt_write_addr(ENG_RST_ADDR, 0x5A);

	mdelay(1);	//wait tMCU_Idle2TP_REX_Hi after TP_RST
}

/*******************************************************
Description:
	Novatek touchscreen reset MCU
    function.

return:
	n.a.
*******************************************************/
void nvt_sw_reset(void)
{
	//---software reset cmds to SWRST_SIF_ADDR---
	nvt_write_addr(SWRST_SIF_ADDR, 0x55);

	msleep(10);
}

/*******************************************************
Description:
	Novatek touchscreen reset MCU then into idle mode
    function.

return:
	n.a.
*******************************************************/
void nvt_sw_reset_idle(void)
{
	//---MCU idle cmds to SWRST_SIF_ADDR---
	nvt_write_addr(SWRST_SIF_ADDR, 0xAA);

	msleep(15);
}

/*******************************************************
Description:
	Novatek touchscreen reset MCU (boot) function.

return:
	n.a.
*******************************************************/
void nvt_bootloader_reset(void)
{
	//---reset cmds to SWRST_SIF_ADDR---
	nvt_write_addr(SWRST_SIF_ADDR, 0x69);

	mdelay(5);	//wait tBRST2FR after Bootload RST

	if (SPI_RD_FAST_ADDR) {
		/* disable SPI_RD_FAST */
		nvt_write_addr(SPI_RD_FAST_ADDR, 0x00);
	}

	NVT_LOG("end\n");
}

/*******************************************************
Description:
	Novatek touchscreen clear FW status function.

return:
	Executive outcomes. 0---succeed. -1---fail.
*******************************************************/
int32_t nvt_clear_fw_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 20;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---clear fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_SPI_WRITE(ts->client, buf, 2);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen check FW status function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
int32_t nvt_check_fw_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 50;

	usleep_range(20000, 20000);

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_SPI_READ(ts->client, buf, 2);

		if ((buf[1] & 0xF0) == 0xA0)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen check FW reset state function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;
	int32_t retry = 0;
	int32_t retry_max = (check_reset_state == RESET_STATE_INIT) ? 10 : 50;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_RESET_COMPLETE);

	while (1) {
		//---read reset state---
		buf[0] = EVENT_MAP_RESET_COMPLETE;
		buf[1] = 0x00;
		CTP_SPI_READ(ts->client, buf, 6);

		if ((buf[1] >= check_reset_state) && (buf[1] <= RESET_STATE_MAX)) {
			ret = 0;
			break;
		}

		retry++;
		if(unlikely(retry > retry_max)) {
			NVT_ERR("error, retry=%d, buf[1]=0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
				retry, buf[1], buf[2], buf[3], buf[4], buf[5]);
			ret = -1;
			break;
		}

		usleep_range(10000, 10000);
	}

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen get firmware related information
	function.

return:
	Executive outcomes. 0---success. -1---fail.
*******************************************************/
int32_t nvt_get_fw_info(void)
{
	uint8_t buf[64] = {0};
	uint32_t retry_count = 0;
	int32_t ret = 0;

info_retry:
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_FWINFO);

	//---read fw info---
	buf[0] = EVENT_MAP_FWINFO;
	CTP_SPI_READ(ts->client, buf, 39);
	if ((buf[1] + buf[2]) != 0xFF) {
		NVT_ERR("FW info is broken! fw_ver=0x%02X, ~fw_ver=0x%02X\n", buf[1], buf[2]);
		if(retry_count < 3) {
			retry_count++;
			NVT_ERR("retry_count=%d\n", retry_count);
			goto info_retry;
		} else {
			ts->fw_ver = 0;
			ts->abs_x_max = TOUCH_DEFAULT_MAX_WIDTH;
			ts->abs_y_max = TOUCH_DEFAULT_MAX_HEIGHT;
			ts->max_button_num = TOUCH_KEY_NUM;
			NVT_ERR("Set default fw_ver=%d, abs_x_max=%d, abs_y_max=%d, max_button_num=%d!\n",
					ts->fw_ver, ts->abs_x_max, ts->abs_y_max, ts->max_button_num);
			ret = -1;
			goto out;
		}
	}
	ts->fw_ver = buf[1];
	ts->x_num = buf[3];
	ts->y_num = buf[4];
	ts->abs_x_max = (uint16_t)((buf[5] << 8) | buf[6]);
	ts->abs_y_max = (uint16_t)((buf[7] << 8) | buf[8]);
	ts->max_button_num = buf[11];
	ts->nvt_pid = (uint16_t)((buf[36] << 8) | buf[35]);
	if (ts->pen_support) {
		ts->x_gang_num = buf[37];
		ts->y_gang_num = buf[38];
	}
	NVT_LOG("fw_ver=0x%02X, fw_type=0x%02X, PID=0x%04X\n", ts->fw_ver, buf[14], ts->nvt_pid);
	snprintf(tp_hardware_info, HARDWARE_MAX_ITEM_LONGTH, "MODULE=TM,IC=NT36672C,FW:0x%02x",ts->fw_ver);
	NVT_LOG("Nvt_tpfwver_show Tp_name is : %s\n", tp_hardware_info);

	set_tpinfo_gki(tp_hardware_info);

	ret = 0;
out:

	return ret;
}

/*******************************************************
  Create Device Node (Proc Entry)
*******************************************************/
#if NVT_TOUCH_PROC
static struct proc_dir_entry *NVT_proc_entry;
#define DEVICE_NAME	"NVTSPI"

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI read function.

return:
	Executive outcomes. 2---succeed. -5,-14---failed.
*******************************************************/
static ssize_t nvt_flash_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
	uint8_t *str = NULL;
	int32_t ret = 0;
	int32_t retries = 0;
	int8_t spi_wr = 0;
	uint8_t *buf;

	if ((count > NVT_TRANSFER_LEN + 3) || (count < 3)) {
		NVT_ERR("invalid transfer len!\n");
		return -EFAULT;
	}

	/* allocate buffer for spi transfer */
	str = (uint8_t *)kzalloc((count), GFP_KERNEL);
	if(str == NULL) {
		NVT_ERR("kzalloc for buf failed!\n");
		ret = -ENOMEM;
		goto kzalloc_failed;
	}

	buf = (uint8_t *)kzalloc((count), GFP_KERNEL | GFP_DMA);
	if(buf == NULL) {
		NVT_ERR("kzalloc for buf failed!\n");
		ret = -ENOMEM;
		kfree(str);
		str = NULL;
		goto kzalloc_failed;
	}

	if (copy_from_user(str, buff, count)) {
		NVT_ERR("copy from user error\n");
		ret = -EFAULT;
		goto out;
	}

#if NVT_TOUCH_ESD_PROTECT
	/*
	 * stop esd check work to avoid case that 0x77 report righ after here to enable esd check again
	 * finally lead to trigger esd recovery bootloader reset
	 */
	cancel_delayed_work_sync(&nvt_esd_check_work);
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	spi_wr = str[0] >> 7;
	memcpy(buf, str+2, ((str[0] & 0x7F) << 8) | str[1]);

	if (spi_wr == NVTWRITE) {	//SPI write
		while (retries < 20) {
			ret = CTP_SPI_WRITE(ts->client, buf, ((str[0] & 0x7F) << 8) | str[1]);
			if (!ret)
				break;
			else
				NVT_ERR("error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		if (unlikely(retries == 20)) {
			NVT_ERR("error, ret = %d\n", ret);
			ret = -EIO;
			goto out;
		}
	} else if (spi_wr == NVTREAD) {	//SPI read
		while (retries < 20) {
			ret = CTP_SPI_READ(ts->client, buf, ((str[0] & 0x7F) << 8) | str[1]);
			if (!ret)
				break;
			else
				NVT_ERR("error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		memcpy(str+2, buf, ((str[0] & 0x7F) << 8) | str[1]);
		// copy buff to user if spi transfer
		if (retries < 20) {
			if (copy_to_user(buff, str, count)) {
				ret = -EFAULT;
				goto out;
			}
		}

		if (unlikely(retries == 20)) {
			NVT_ERR("error, ret = %d\n", ret);
			ret = -EIO;
			goto out;
		}
	} else {
		NVT_ERR("Call error, str[0]=%d\n", str[0]);
		ret = -EFAULT;
		goto out;
	}

out:
	kfree(str);
    kfree(buf);
kzalloc_failed:
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI open function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
static int32_t nvt_flash_open(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev;

	dev = kmalloc(sizeof(struct nvt_flash_data), GFP_KERNEL);
	if (dev == NULL) {
		NVT_ERR("Failed to allocate memory for nvt flash data\n");
		return -ENOMEM;
	}

	rwlock_init(&dev->lock);
	file->private_data = dev;

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI close function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_flash_close(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev = file->private_data;

	if (dev)
		kfree(dev);

	return 0;
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_flash_fops = {
	.proc_open = nvt_flash_open,
	.proc_release = nvt_flash_close,
	.proc_read = nvt_flash_read,
};
#else
static const struct file_operations nvt_flash_fops = {
	.owner = THIS_MODULE,
	.open = nvt_flash_open,
	.release = nvt_flash_close,
	.read = nvt_flash_read,
};
#endif

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI initial function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
static int32_t nvt_flash_proc_init(void)
{
	NVT_proc_entry = proc_create(DEVICE_NAME, 0444, NULL,&nvt_flash_fops);
	if (NVT_proc_entry == NULL) {
		NVT_ERR("Failed!\n");
		return -ENOMEM;
	} else {
		NVT_LOG("Succeeded!\n");
	}

	NVT_LOG("============================================================\n");
	NVT_LOG("Create /proc/%s\n", DEVICE_NAME);
	NVT_LOG("============================================================\n");

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI deinitial function.

return:
	n.a.
*******************************************************/
static void nvt_flash_proc_deinit(void)
{
	if (NVT_proc_entry != NULL) {
		remove_proc_entry(DEVICE_NAME, NULL);
		NVT_proc_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", DEVICE_NAME);
	}
}
#endif
/* customized gesture id */
#define DATA_PROTOCOL           30

#if WAKEUP_GESTURE
#define GESTURE_WORD_C          12
#define GESTURE_WORD_W          13
#define GESTURE_WORD_V          14
#define GESTURE_DOUBLE_CLICK    15
#define GESTURE_WORD_Z          16
#define GESTURE_WORD_M          17
#define GESTURE_WORD_O          18
#define GESTURE_WORD_e          19
#define GESTURE_WORD_S          20
#define GESTURE_SLIDE_UP        21
#define GESTURE_SLIDE_DOWN      22
#define GESTURE_SLIDE_LEFT      23
#define GESTURE_SLIDE_RIGHT     24

/* function page definition */
#define FUNCPAGE_GESTURE         1

/*******************************************************
Description:
	Novatek touchscreen wake up gesture key report function.

return:
	n.a.
*******************************************************/
void nvt_ts_wakeup_gesture_report(uint8_t gesture_id, uint8_t *data)
{
	uint32_t keycode = 0;
	uint8_t func_type = data[2];
	uint8_t func_id = data[3];

	/* support fw specifal data protocol */
	if ((gesture_id == DATA_PROTOCOL) && (func_type == FUNCPAGE_GESTURE)) {
		gesture_id = func_id;
	} else if (gesture_id > DATA_PROTOCOL) {
		NVT_ERR("gesture_id %d is invalid, func_type=%d, func_id=%d\n", gesture_id, func_type, func_id);
		return;
	}

	NVT_LOG("gesture_id = %d\n", gesture_id);

	switch (gesture_id) {
		case GESTURE_WORD_C:
			NVT_LOG("Gesture : Word-C.\n");
			keycode = gesture_key_array[0];
			break;
		case GESTURE_WORD_W:
			NVT_LOG("Gesture : Word-W.\n");
			keycode = gesture_key_array[1];
			break;
		case GESTURE_WORD_V:
			NVT_LOG("Gesture : Word-V.\n");
			keycode = gesture_key_array[2];
			break;
		case GESTURE_DOUBLE_CLICK:
			NVT_LOG("Gesture : Double Click.\n");
			keycode = gesture_key_array[3];
			break;
		case GESTURE_WORD_Z:
			NVT_LOG("Gesture : Word-Z.\n");
			keycode = gesture_key_array[4];
			break;
		case GESTURE_WORD_M:
			NVT_LOG("Gesture : Word-M.\n");
			keycode = gesture_key_array[5];
			break;
		case GESTURE_WORD_O:
			NVT_LOG("Gesture : Word-O.\n");
			keycode = gesture_key_array[6];
			break;
		case GESTURE_WORD_e:
			NVT_LOG("Gesture : Word-e.\n");
			keycode = gesture_key_array[7];
			break;
		case GESTURE_WORD_S:
			NVT_LOG("Gesture : Word-S.\n");
			keycode = gesture_key_array[8];
			break;
		case GESTURE_SLIDE_UP:
			NVT_LOG("Gesture : Slide UP.\n");
			keycode = gesture_key_array[9];
			break;
		case GESTURE_SLIDE_DOWN:
			NVT_LOG("Gesture : Slide DOWN.\n");
			keycode = gesture_key_array[10];
			break;
		case GESTURE_SLIDE_LEFT:
			NVT_LOG("Gesture : Slide LEFT.\n");
			keycode = gesture_key_array[11];
			break;
		case GESTURE_SLIDE_RIGHT:
			NVT_LOG("Gesture : Slide RIGHT.\n");
			keycode = gesture_key_array[12];
			break;
		default:
			break;
	}

	if (keycode > 0) {
		input_report_key(ts->input_dev, keycode, 1);
		input_sync(ts->input_dev);
		input_report_key(ts->input_dev, keycode, 0);
		input_sync(ts->input_dev);
	}
}
#endif

/*******************************************************
Description:
	Novatek touchscreen parse device tree function.

return:
	n.a.
*******************************************************/
#ifdef CONFIG_OF
static int32_t nvt_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int32_t ret = 0;

#if NVT_TOUCH_SUPPORT_HW_RST
	ts->reset_gpio = of_get_named_gpio_flags(np, "novatek,reset-gpio", 0, &ts->reset_flags);
	NVT_LOG("novatek,reset-gpio=%d\n", ts->reset_gpio);
#endif
	ts->irq_gpio = of_get_named_gpio_flags(np, "novatek,irq-gpio", 0, &ts->irq_flags);
	NVT_LOG("novatek,irq-gpio=%d\n", ts->irq_gpio);

	ts->pen_support = of_property_read_bool(np, "novatek,pen-support");
	NVT_LOG("novatek,pen-support=%d\n", ts->pen_support);

	ts->stylus_resol_double = of_property_read_bool(np, "novatek,stylus-resol-double");
	NVT_LOG("novatek,stylus-resol-double=%d\n", ts->stylus_resol_double);

	ret = of_property_read_u32(np, "novatek,swrst-n8-addr", &SWRST_SIF_ADDR);
	if (ret) {
		NVT_ERR("error reading novatek,swrst-n8-addr. ret=%d\n", ret);
		return ret;
	} else {
		NVT_LOG("SWRST_SIF_ADDR=0x%06X\n", SWRST_SIF_ADDR);
	}

	ret = of_property_read_u32(np, "novatek,spi-rd-fast-addr", &SPI_RD_FAST_ADDR);
	if (ret) {
		NVT_LOG("not support novatek,spi-rd-fast-addr\n");
		SPI_RD_FAST_ADDR = 0;
		ret = 0;
	} else {
		NVT_LOG("SPI_RD_FAST_ADDR=0x%06X\n", SPI_RD_FAST_ADDR);
	}

	return ret;
}
#else
static int32_t nvt_parse_dt(struct device *dev)
{
#if NVT_TOUCH_SUPPORT_HW_RST
	ts->reset_gpio = NVTTOUCH_RST_PIN;
#endif
	ts->irq_gpio = NVTTOUCH_INT_PIN;
	return 0;
}
#endif

/*******************************************************
Description:
	Novatek touchscreen config and request gpio

return:
	Executive outcomes. 0---succeed. not 0---failed.
*******************************************************/
static int nvt_gpio_config(struct nvt_ts_data *ts)
{
	int32_t ret = 0;

#if NVT_TOUCH_SUPPORT_HW_RST
	/* request RST-pin (Output/High) */
	if (gpio_is_valid(ts->reset_gpio)) {
		ret = gpio_request_one(ts->reset_gpio, GPIOF_OUT_INIT_LOW, "NVT-tp-rst");
		if (ret) {
			NVT_ERR("Failed to request NVT-tp-rst GPIO\n");
			goto err_request_reset_gpio;
		}
	}
#endif

	/* request INT-pin (Input) */
	if (gpio_is_valid(ts->irq_gpio)) {
		ret = gpio_request_one(ts->irq_gpio, GPIOF_IN, "NVT-int");
		if (ret) {
			NVT_ERR("Failed to request NVT-int GPIO\n");
			goto err_request_irq_gpio;
		}
	}

	return ret;

err_request_irq_gpio:
#if NVT_TOUCH_SUPPORT_HW_RST
	gpio_free(ts->reset_gpio);
err_request_reset_gpio:
#endif
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen deconfig gpio

return:
	n.a.
*******************************************************/
static void nvt_gpio_deconfig(struct nvt_ts_data *ts)
{
	if (gpio_is_valid(ts->irq_gpio))
		gpio_free(ts->irq_gpio);
#if NVT_TOUCH_SUPPORT_HW_RST
	if (gpio_is_valid(ts->reset_gpio))
		gpio_free(ts->reset_gpio);
#endif
}

static uint8_t nvt_fw_recovery(uint8_t *point_data)
{
	uint8_t i = 0;
	uint8_t detected = true;

	/* check pattern */
	for (i=1 ; i<7 ; i++) {
		if (point_data[i] != 0x77) {
			detected = false;
			break;
		}
	}

	return detected;
}

#if NVT_TOUCH_ESD_PROTECT
void nvt_esd_check_enable(uint8_t enable)
{
	/* update interrupt timer */
	irq_timer = jiffies;
	/* clear esd_retry counter, if protect function is enabled */
	esd_retry = enable ? 0 : esd_retry;
	/* enable/disable esd check flag */
	esd_check = enable;
}

static void nvt_esd_check_func(struct work_struct *work)
{
	unsigned int timer = jiffies_to_msecs(jiffies - irq_timer);

	//NVT_LOG("esd_check = %d (retry %d)\n", esd_check, esd_retry);	//DEBUG

	if ((timer > NVT_TOUCH_ESD_CHECK_PERIOD) && esd_check) {
		mutex_lock(&ts->lock);
		NVT_ERR("do ESD recovery, timer = %d, retry = %d\n", timer, esd_retry);
		/* do esd recovery, reload fw */
		nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME);
		mutex_unlock(&ts->lock);
		/* update interrupt timer */
		irq_timer = jiffies;
		/* update esd_retry counter */
		esd_retry++;
	}

	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
}
#endif /* #if NVT_TOUCH_ESD_PROTECT */

//Porting from xiaomi
int32_t nvt_set_pocket_palm_switch(uint8_t pocket_palm_switch)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;
	NVT_LOG("set pocket palm switch: %d\n", pocket_palm_switch);
	msleep(35);
	/* ---set xdata index to EVENT BUF ADDR--- */
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_set_pocket_palm_switch_out;
	}
	buf[0] = EVENT_MAP_HOST_CMD;
	if (pocket_palm_switch == 0) {
		/* pocket palm disable */
		buf[1] = 0x74;
	} else if (pocket_palm_switch == 1) {
		/* pocket palm enable */
		buf[1] = 0x73;
	} else {
		NVT_ERR("Invalid value! pocket_palm_switch = %d\n", pocket_palm_switch);
		ret = -EINVAL;
		goto nvt_set_pocket_palm_switch_out;
	}
	ret = CTP_SPI_WRITE(ts->client, buf, 2);
	if (ret < 0) {
		NVT_ERR("Write pocket palm switch command fail!\n");
		goto nvt_set_pocket_palm_switch_out;
	}
nvt_set_pocket_palm_switch_out:
	NVT_LOG("%s --\n", __func__);
	return ret;
}

#if IS_ENABLED(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE)
static struct xiaomi_touch_interface xiaomi_touch_interfaces;
int32_t nvt_check_palm(uint8_t input_id, uint8_t *data)
{
	int32_t ret = 0;
	uint8_t func_type = data[2];
	uint8_t palm_state = data[3];
	if ((input_id == DATA_PROTOCOL) && (func_type == FUNCPAGE_PALM)) {
		ret = palm_state;
		if (palm_state == PACKET_PALM_ON) {
			NVT_LOG("get packet palm on event.\n");
			update_palm_sensor_value(1);
		} else if (palm_state == PACKET_PALM_OFF) {
			NVT_LOG("get packet palm off event.\n");
			update_palm_sensor_value(0);
		} else {
			NVT_ERR("invalid palm state %d!\n", palm_state);
			ret = -1;
		}
	} else {
		ret = 0;
	}
	return ret;
}

static int nvt_palm_sensor_write(int value)
{
	int ret = 0;
	NVT_LOG("enter %d %d\n", value, ts->palm_sensor_switch);
	ts->palm_sensor_switch = value;
	if (ts->dev_pm_suspend) {
		NVT_LOG("tp has dev_pm_suspend status\n");
		return 0;
	}
	if (bTouchIsAwake) {
		mutex_lock(&ts->lock);
#if NVT_TOUCH_ESD_PROTECT
		nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
		ret = nvt_set_pocket_palm_switch(value);
		mutex_unlock(&ts->lock);
	}
	return ret;
}

static struct xiaomi_touch_interface xiaomi_touch_interfaces;
static void nvt_init_touchmode_data(void)
{
	int i;
	NVT_LOG("%s,ENTER\n", __func__);
	/* Touch Game Mode Switch */
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_CUR_VALUE] = 0;
	/* Acitve Mode */
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_CUR_VALUE] = 0;
	/* Sensivity */
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_CUR_VALUE] = 0;
	/* Tolerance */
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_CUR_VALUE] = 0;
	/* Panel orientation*/
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE] = 0;
	/* Edge filter area*/
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_CUR_VALUE] = 0;
	/* Resist RF interference*/
	xiaomi_touch_interfaces.touch_mode[Touch_Resist_RF][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Resist_RF][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Resist_RF][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Resist_RF][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Resist_RF][GET_CUR_VALUE] = 0;
	for (i = 0; i < Touch_Mode_NUM; i++) {
		NVT_LOG("mode:%d, set cur:%d, get cur:%d, def:%d min:%d max:%d\n",
			i,
			xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_MIN_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_MAX_VALUE]);
	}
	return;
}

static int nvt_touchfeature_cmd_xsfer(uint8_t *touchfeature)
{
	int ret;
	uint8_t buf[8] = {0};
	NVT_LOG("++\n");
	NVT_LOG("cmd xsfer:%02x, %02x", touchfeature[0], touchfeature[1]);
	/* ---set xdata index to EVENT BUF ADDR--- */
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_touchfeature_cmd_xsfer_out;
	}
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = touchfeature[0];
	buf[2] = touchfeature[1];
	ret = CTP_SPI_WRITE(ts->client, buf, 3);
	if (ret < 0) {
		NVT_ERR("Write sensitivity switch command fail!\n");
		goto nvt_touchfeature_cmd_xsfer_out;
	}
	nvt_touchfeature_cmd_xsfer_out:
	NVT_LOG("--\n");
	return ret;
}

static int nvt_touchfeature_set(uint8_t *touchfeature)
{
	int ret;
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}
#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
	ret = nvt_touchfeature_cmd_xsfer(touchfeature);
	if (ret < 0)
		NVT_ERR("send cmd via SPI failed, errno:%d", ret);
	mutex_unlock(&ts->lock);
	msleep(35);
	return ret;
}

static int nvt_set_cur_value(int nvt_mode, int nvt_value)
{
	bool skip = false;
	uint8_t nvt_game_value[2] = {0};
	uint8_t temp_value = 0;
	uint8_t ret = 0;
	if (nvt_mode >= Touch_Mode_NUM && nvt_mode < 0) {
		NVT_ERR("%s, nvt mode is error:%d", __func__, nvt_mode);
		return -EINVAL;
	}
	if (nvt_mode == Touch_Doubletap_Mode && ts && nvt_value >= 0) {
		ts-> db_wakeup = nvt_value;
		schedule_work(&ts->switch_mode_work);
	}
	xiaomi_touch_interfaces.touch_mode[nvt_mode][SET_CUR_VALUE] = nvt_value;
	if (xiaomi_touch_interfaces.touch_mode[nvt_mode][SET_CUR_VALUE] >
			xiaomi_touch_interfaces.touch_mode[nvt_mode][GET_MAX_VALUE]) {
		xiaomi_touch_interfaces.touch_mode[nvt_mode][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[nvt_mode][GET_MAX_VALUE];
	} else if (xiaomi_touch_interfaces.touch_mode[nvt_mode][SET_CUR_VALUE] <
			xiaomi_touch_interfaces.touch_mode[nvt_mode][GET_MIN_VALUE]) {
		xiaomi_touch_interfaces.touch_mode[nvt_mode][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[nvt_mode][GET_MIN_VALUE];
	}
	switch (nvt_mode) {
	case Touch_Game_Mode:
			break;
	case Touch_Active_MODE:
			break;
	case Touch_UP_THRESHOLD:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE];
			nvt_game_value[0] = 0x71;
			nvt_game_value[1] = temp_value;
			break;
	case Touch_Tolerance:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE];
			nvt_game_value[0] = 0x70;
			nvt_game_value[1] = temp_value;
			break;
	case Touch_Edge_Filter:
			/* filter 0,1,2,3 = default,1,2,3 level*/
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE];
			nvt_game_value[0] = 0x72;
			nvt_game_value[1] = temp_value;
			break;
	case Touch_Panel_Orientation:
			/* 0,1,2,3 = 0, 90, 180,270 Counter-clockwise*/
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE];
			if (temp_value == 0 || temp_value == 2) {
				nvt_game_value[0] = 0xBA;
			} else if (temp_value == 1) {
				nvt_game_value[0] = 0xBC;
			} else if (temp_value == 3) {
				nvt_game_value[0] = 0xBB;
			}
			nvt_game_value[1] = 0;
			break;
	case Touch_Resist_RF:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Resist_RF][SET_CUR_VALUE];
			if (temp_value == 0) {
				nvt_game_value[0] = 0x76;
			} else if (temp_value == 1) {
				nvt_game_value[0] = 0x75;
			}
			nvt_game_value[1] = 0;
			break;
	default:
			/* Don't support */
			skip = true;
			break;
	};
	NVT_LOG("mode:%d, value:%d,temp_value:%d,game value:0x%x,0x%x", nvt_mode, nvt_value, temp_value, nvt_game_value[0], nvt_game_value[1]);
	if (!skip) {
		xiaomi_touch_interfaces.touch_mode[nvt_mode][GET_CUR_VALUE] =
						xiaomi_touch_interfaces.touch_mode[nvt_mode][SET_CUR_VALUE];
		ret = nvt_touchfeature_set(nvt_game_value);
		if (ret < 0) {
			NVT_ERR("change game mode fail");
		}
	} else {
		NVT_LOG("Cmd is not support,skip!");
	}
	return 0;
}

static int nvt_get_mode_value(int mode, int value_type)
{
	int value = -1;
	if (mode < Touch_Mode_NUM && mode >= 0)
		value = xiaomi_touch_interfaces.touch_mode[mode][value_type];
	else
		NVT_ERR("%s, don't support\n", __func__);
	return value;
}
static int nvt_get_mode_all(int mode, int *value)
{
	if (mode < Touch_Mode_NUM && mode >= 0) {
		value[0] = xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE];
		value[1] = xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		value[2] = xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE];
		value[3] = xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE];
	} else {
		NVT_ERR("%s, don't support\n",  __func__);
	}
	NVT_LOG("%s, mode:%d, value:%d:%d:%d:%d\n", __func__, mode, value[0],
					value[1], value[2], value[3]);
	return 0;
}

static int nvt_reset_mode(int mode)
{
	int i = 0;
	NVT_LOG("nvt_reset_mode enter\n");
	if (mode < Touch_Report_Rate && mode > 0) {
		xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		nvt_set_cur_value(mode, xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE]);
	} else if (mode == 0) {
		for (i = 0; i <= Touch_Report_Rate; i++) {
			if (i == Touch_Panel_Orientation) {
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE];
			} else {
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE];
			}
			nvt_set_cur_value(i, xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE]);
		}
	} else {
		NVT_ERR("%s, don't support\n",  __func__);
	}
	NVT_LOG("%s, mode:%d\n",  __func__, mode);
	return 0;
}
#endif
/*end porting xiaomi codes*/

#define PEN_DATA_LEN 14
#if CHECK_PEN_DATA_CHECKSUM
static int32_t nvt_ts_pen_data_checksum(uint8_t *buf, uint8_t length)
{
	uint8_t checksum = 0;
	int32_t i = 0;

	// Calculate checksum
	for (i = 0; i < length - 1; i++) {
		checksum += buf[i];
	}
	checksum = (~checksum + 1);

	// Compare ckecksum and dump fail data
	if (checksum != buf[length - 1]) {
		NVT_ERR("pen packet checksum not match. (buf[%d]=0x%02X, checksum=0x%02X)\n",
			length - 1, buf[length - 1], checksum);
		//--- dump pen buf ---
		for (i = 0; i < length; i++) {
			printk("%02X ", buf[i]);
		}
		printk("\n");

		return -1;
	}

	return 0;
}
#endif // #if CHECK_PEN_DATA_CHECKSUM

int32_t nvt_check_proximity(uint8_t input_id, uint8_t *data)
{
	int32_t ret = 0;
	uint8_t func_type = data[2];
	//uint8_t proximity_state = data[3];
	uint8_t proximity_level = data[4];
	int proximity_state = -1;

	if ((input_id == DATA_PROTOCOL) && (func_type == FUNCPAGE_PROXIMITY)) {
		/*ret = proximity_state;
		if (proximity_state == PROXIMITY_ON) {
			g_priximity_data = 0;
			NVT_LOG("get proximity on event.\n");
		} else if (proximity_state == PROXIMITY_OFF) {
			g_priximity_data = 1;
			NVT_LOG("get proximity off event.\n");
		} else {
			NVT_ERR("invalid proximity state %d!\n", proximity_state);
			ret = -1;
		}*/
		if (proximity_level == NVT_PROXIMITY_LEVEL1 || proximity_level == NVT_PROXIMITY_LEVEL2
			|| proximity_level == NVT_PROXIMITY_LEVEL3 || proximity_level == NVT_PROXIMITY_LEVEL4) {
			proximity_state = proximity_level;
		} else if (proximity_level == NVT_PROXIMITY_LEVEL0 || proximity_level == NVT_PROXIMITY_LEVEL5) {
			proximity_state = NVT_PROXIMITY_LEVEL0;
		} else {
                  	proximity_state = NVT_PROXIMITY_LEVEL0;
			ret = -1;
		}
		//NVT_LOG("proximity_state is %d\n",proximity_state);
		if (proximity_state != g_priximity_data) {
			NVT_LOG(" proximity state is %d, get proximity event.\n",proximity_state);
			g_priximity_data = proximity_state;
			g_new_proximity_event_flag = 1;
			wake_up(&proximity_switch_queue);
			return 0;
		}
	} else {
	ret = -1;
	}

	return ret;
}

#if NVT_TOUCH_WDT_RECOVERY
static uint8_t recovery_cnt = 0;
static uint8_t nvt_wdt_fw_recovery(uint8_t *point_data)
{
   uint32_t recovery_cnt_max = 10;
   uint8_t recovery_enable = false;
   uint8_t i = 0;

   recovery_cnt++;

   /* check pattern */
   for (i=1 ; i<7 ; i++) {
       if ((point_data[i] != 0xFD) && (point_data[i] != 0xFE)) {
           recovery_cnt = 0;
           break;
       }
   }

   if (recovery_cnt > recovery_cnt_max){
       recovery_enable = true;
       recovery_cnt = 0;
   }

   return recovery_enable;
}

void nvt_read_fw_history(uint32_t fw_history_addr)
{
    uint8_t i = 0;
    uint8_t buf[65];
    char str[128];

	if (fw_history_addr == 0)
		return;

    nvt_set_page(fw_history_addr);

    buf[0] = (uint8_t) (fw_history_addr & 0x7F);
    CTP_SPI_READ(ts->client, buf, 64+1);	//read 64bytes history

    //print all data
    NVT_LOG("fw history 0x%x: \n", fw_history_addr);
    for (i = 0; i < 4; i++) {
        snprintf(str, sizeof(str),
				"%02X %02X %02X %02X %02X %02X %02X %02X  "
				"%02X %02X %02X %02X %02X %02X %02X %02X\n",
                 buf[1+i*16], buf[2+i*16], buf[3+i*16], buf[4+i*16],
                 buf[5+i*16], buf[6+i*16], buf[7+i*16], buf[8+i*16],
                 buf[9+i*16], buf[10+i*16], buf[11+i*16], buf[12+i*16],
                 buf[13+i*16], buf[14+i*16], buf[15+i*16], buf[16+i*16]);
        NVT_LOG("%s", str);
    }

	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);
}
#endif	/* #if NVT_TOUCH_WDT_RECOVERY */

#if POINT_DATA_CHECKSUM
static int32_t nvt_ts_point_data_checksum(uint8_t *buf, uint8_t length)
{
   uint8_t checksum = 0;
   int32_t i = 0;

   // Generate checksum
   for (i = 0; i < length - 1; i++) {
       checksum += buf[i + 1];
   }
   checksum = (~checksum + 1);

   // Compare ckecksum and dump fail data
   if (checksum != buf[length]) {
       NVT_ERR("i2c/spi packet checksum not match. (point_data[%d]=0x%02X, checksum=0x%02X)\n",
               length, buf[length], checksum);

       for (i = 0; i < 10; i++) {
           NVT_LOG("%02X %02X %02X %02X %02X %02X\n",
                   buf[1 + i*6], buf[2 + i*6], buf[3 + i*6], buf[4 + i*6], buf[5 + i*6], buf[6 + i*6]);
       }

       NVT_LOG("%02X %02X %02X %02X %02X\n", buf[61], buf[62], buf[63], buf[64], buf[65]);

       return -1;
   }

   return 0;
}
#endif /* POINT_DATA_CHECKSUM */

//#define POINT_DATA_LEN 65
/*******************************************************
Description:
	Novatek touchscreen work function.

return:
	n.a.
*******************************************************/
static irqreturn_t nvt_ts_work_func(int irq, void *data)
{
	int32_t ret = -1;
	uint8_t point_data[POINT_DATA_LEN + PEN_DATA_LEN + 1 + DUMMY_BYTES] = {0};
	uint32_t position = 0;
	uint32_t input_x = 0;
	uint32_t input_y = 0;
	uint32_t input_w = 0;
	uint32_t input_p = 0;
	uint8_t input_id = 0;
#if MT_PROTOCOL_B
	uint8_t press_id[TOUCH_MAX_FINGER_NUM] = {0};
#endif /* MT_PROTOCOL_B */
	int32_t i = 0;
	int32_t finger_cnt = 0;
	uint8_t pen_format_id = 0;
	uint32_t pen_x = 0;
	uint32_t pen_y = 0;
	uint32_t pen_pressure = 0;
	uint32_t pen_distance = 0;
	int8_t pen_tilt_x = 0;
	int8_t pen_tilt_y = 0;
	uint32_t pen_btn1 = 0;
	uint32_t pen_btn2 = 0;
	uint32_t pen_battery = 0;

#if WAKEUP_GESTURE
	if (bTouchIsAwake == 0) {
		pm_wakeup_event(&ts->input_dev->dev, 5000);
	}
#endif

	mutex_lock(&ts->lock);

	if (ts->pen_support)
		ret = CTP_SPI_READ(ts->client, point_data, POINT_DATA_LEN + PEN_DATA_LEN + 1);
	else
		ret = CTP_SPI_READ(ts->client, point_data, POINT_DATA_LEN + 1);
	if (ret < 0) {
		NVT_ERR("CTP_SPI_READ failed.(%d)\n", ret);
		goto XFER_ERROR;
	}
/*
	//--- dump SPI buf ---
	for (i = 0; i < 10; i++) {
		printk("%02X %02X %02X %02X %02X %02X  ",
			point_data[1+i*6], point_data[2+i*6], point_data[3+i*6], point_data[4+i*6], point_data[5+i*6], point_data[6+i*6]);
	}
	printk("\n");
*/

#if NVT_TOUCH_WDT_RECOVERY
	/* ESD protect by WDT */
	if (nvt_wdt_fw_recovery(point_data)) {
		NVT_ERR("Recover for fw reset, %02X\n", point_data[1]);
		if (point_data[1] == 0xFE) {
			nvt_sw_reset_idle();
		}
		nvt_read_fw_history(ts->mmap->MMAP_HISTORY_EVENT0);
		nvt_read_fw_history(ts->mmap->MMAP_HISTORY_EVENT1);
		nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME);
		goto XFER_ERROR;
	}
#endif /* #if NVT_TOUCH_WDT_RECOVERY */

	/* ESD protect by FW handshake */
	if (nvt_fw_recovery(point_data)) {
#if NVT_TOUCH_ESD_PROTECT
		nvt_esd_check_enable(true);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
		goto XFER_ERROR;
	}

	input_id = (uint8_t)(point_data[1] >> 3);
	if (nvt_check_proximity(input_id, point_data) == 0) {
		goto XFER_ERROR; /* to skip point data parsing */
}

	input_id = (uint8_t)(point_data[1] >> 3);
	if (nvt_check_palm(input_id, point_data)) {
		goto XFER_ERROR; /* to skip point data parsing */
	}

#if POINT_DATA_CHECKSUM
   if (POINT_DATA_LEN >= POINT_DATA_CHECKSUM_LEN) {
       ret = nvt_ts_point_data_checksum(point_data, POINT_DATA_CHECKSUM_LEN);
       if (ret) {
           goto XFER_ERROR;
       }
   }
#endif /* POINT_DATA_CHECKSUM */

#if WAKEUP_GESTURE
	if (bTouchIsAwake == 0) {
		input_id = (uint8_t)(point_data[1] >> 3);
		nvt_ts_wakeup_gesture_report(input_id, point_data);
		mutex_unlock(&ts->lock);
		return IRQ_HANDLED;
	}
#endif

	finger_cnt = 0;

	for (i = 0; i < ts->max_touch_num; i++) {
		position = 1 + 6 * i;
		input_id = (uint8_t)(point_data[position + 0] >> 3);
		if ((input_id == 0) || (input_id > ts->max_touch_num))
			continue;

		if (((point_data[position] & 0x07) == 0x01) || ((point_data[position] & 0x07) == 0x02)) {	//finger down (enter & moving)
#if NVT_TOUCH_ESD_PROTECT
			/* update interrupt timer */
			irq_timer = jiffies;
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#if NVT_SUPER_RESOLUTION_N
						input_x = (uint32_t)(point_data[position + 1] << 8) + (uint32_t) (point_data[position + 2]);
						input_y = (uint32_t)(point_data[position + 3] << 8) + (uint32_t) (point_data[position + 4]);
						if ((input_x < 0) || (input_y < 0))
							continue;
						if ((input_x > ts->abs_x_max * NVT_SUPER_RESOLUTION_N - 1) || (input_y > ts->abs_y_max * NVT_SUPER_RESOLUTION_N - 1))
							continue;
						input_w = (uint32_t)(point_data[position + 5]);
						if (input_w == 0)
							input_w = 1;
						input_p = (uint32_t)(point_data[1 + 98 + i]);
						if (input_p == 0)
							input_p = 1;
#else /* #if NVT_SUPER_RESOLUTION_N */

			input_x = (uint32_t)(point_data[position + 1] << 4) + (uint32_t) (point_data[position + 3] >> 4);
			input_y = (uint32_t)(point_data[position + 2] << 4) + (uint32_t) (point_data[position + 3] & 0x0F);
			if ((input_x < 0) || (input_y < 0))
				continue;
			if ((input_x > ts->abs_x_max-1) || (input_y > ts->abs_y_max-1))
				continue;
			input_w = (uint32_t)(point_data[position + 4]);
			if (input_w == 0)
				input_w = 1;
			if (i < 2) {
				input_p = (uint32_t)(point_data[position + 5]) + (uint32_t)(point_data[i + 63] << 8);
				if (input_p > TOUCH_FORCE_NUM)
					input_p = TOUCH_FORCE_NUM;
			} else {
				input_p = (uint32_t)(point_data[position + 5]);
			}
			if (input_p == 0)
				input_p = 1;
#endif /* #if NVT_SUPER_RESOLUTION_N */
                  
#if MT_PROTOCOL_B
			press_id[input_id - 1] = 1;
			input_mt_slot(ts->input_dev, input_id - 1);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
#else /* MT_PROTOCOL_B */
			input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, input_id - 1);
			input_report_key(ts->input_dev, BTN_TOUCH, 1);
#endif /* MT_PROTOCOL_B */

			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, input_x);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, input_y);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, input_w);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, input_p);

#if MT_PROTOCOL_B
#else /* MT_PROTOCOL_B */
			input_mt_sync(ts->input_dev);
#endif /* MT_PROTOCOL_B */

			finger_cnt++;
		}
	}

#if MT_PROTOCOL_B
	for (i = 0; i < ts->max_touch_num; i++) {
		if (press_id[i] != 1) {
			input_mt_slot(ts->input_dev, i);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
		}
	}

	input_report_key(ts->input_dev, BTN_TOUCH, (finger_cnt > 0));
#else /* MT_PROTOCOL_B */
	if (finger_cnt == 0) {
		input_report_key(ts->input_dev, BTN_TOUCH, 0);
		input_mt_sync(ts->input_dev);
	}
#endif /* MT_PROTOCOL_B */

#if TOUCH_KEY_NUM > 0
	if (point_data[61] == 0xF8) {
#if NVT_TOUCH_ESD_PROTECT
		/* update interrupt timer */
		irq_timer = jiffies;
#endif /* #if NVT_TOUCH_ESD_PROTECT */
		for (i = 0; i < ts->max_button_num; i++) {
			input_report_key(ts->input_dev, touch_key_array[i], ((point_data[62] >> i) & 0x01));
		}
	} else {
		for (i = 0; i < ts->max_button_num; i++) {
			input_report_key(ts->input_dev, touch_key_array[i], 0);
		}
	}
#endif

	input_sync(ts->input_dev);

	if (ts->pen_support) {
/*
		//--- dump pen buf ---
		printk("%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
			point_data[66], point_data[67], point_data[68], point_data[69], point_data[70],
			point_data[71], point_data[72], point_data[73], point_data[74], point_data[75],
			point_data[76], point_data[77], point_data[78], point_data[79]);
*/
#if CHECK_PEN_DATA_CHECKSUM
		if (nvt_ts_pen_data_checksum(&point_data[66], PEN_DATA_LEN)) {
			// pen data packet checksum not match, skip it
			goto XFER_ERROR;
		}
#endif // #if CHECK_PEN_DATA_CHECKSUM

		// parse and handle pen report
		pen_format_id = point_data[66];
		if (pen_format_id != 0xFF) {
			if (pen_format_id == 0x01) {
				// report pen data
				pen_x = (uint32_t)(point_data[67] << 8) + (uint32_t)(point_data[68]);
				pen_y = (uint32_t)(point_data[69] << 8) + (uint32_t)(point_data[70]);
				pen_pressure = (uint32_t)(point_data[71] << 8) + (uint32_t)(point_data[72]);
				pen_tilt_x = (int32_t)point_data[73];
				pen_tilt_y = (int32_t)point_data[74];
				pen_distance = (uint32_t)(point_data[75] << 8) + (uint32_t)(point_data[76]);
				pen_btn1 = (uint32_t)(point_data[77] & 0x01);
				pen_btn2 = (uint32_t)((point_data[77] >> 1) & 0x01);
				pen_battery = (uint32_t)point_data[78];
//				printk("x=%d,y=%d,p=%d,tx=%d,ty=%d,d=%d,b1=%d,b2=%d,bat=%d\n", pen_x, pen_y, pen_pressure,
//						pen_tilt_x, pen_tilt_y, pen_distance, pen_btn1, pen_btn2, pen_battery);

				input_report_abs(ts->pen_input_dev, ABS_X, pen_x);
				input_report_abs(ts->pen_input_dev, ABS_Y, pen_y);
				input_report_abs(ts->pen_input_dev, ABS_PRESSURE, pen_pressure);
				input_report_key(ts->pen_input_dev, BTN_TOUCH, !!pen_pressure);
				input_report_abs(ts->pen_input_dev, ABS_TILT_X, pen_tilt_x);
				input_report_abs(ts->pen_input_dev, ABS_TILT_Y, pen_tilt_y);
				input_report_abs(ts->pen_input_dev, ABS_DISTANCE, pen_distance);
				input_report_key(ts->pen_input_dev, BTN_TOOL_PEN, !!pen_distance || !!pen_pressure);
				input_report_key(ts->pen_input_dev, BTN_STYLUS, pen_btn1);
				input_report_key(ts->pen_input_dev, BTN_STYLUS2, pen_btn2);
				// TBD: pen battery event report
				// NVT_LOG("pen_battery=%d\n", pen_battery);
			} else if (pen_format_id == 0xF0) {
				// report Pen ID
			} else {
				NVT_ERR("Unknown pen format id!\n");
				goto XFER_ERROR;
			}
		} else { // pen_format_id = 0xFF, i.e. no pen present
			input_report_abs(ts->pen_input_dev, ABS_X, 0);
			input_report_abs(ts->pen_input_dev, ABS_Y, 0);
			input_report_abs(ts->pen_input_dev, ABS_PRESSURE, 0);
			input_report_abs(ts->pen_input_dev, ABS_TILT_X, 0);
			input_report_abs(ts->pen_input_dev, ABS_TILT_Y, 0);
			input_report_abs(ts->pen_input_dev, ABS_DISTANCE, 0);
			input_report_key(ts->pen_input_dev, BTN_TOUCH, 0);
			input_report_key(ts->pen_input_dev, BTN_TOOL_PEN, 0);
			input_report_key(ts->pen_input_dev, BTN_STYLUS, 0);
			input_report_key(ts->pen_input_dev, BTN_STYLUS2, 0);
		}

		input_sync(ts->pen_input_dev);
	} /* if (ts->pen_support) */

XFER_ERROR:

	mutex_unlock(&ts->lock);

	return IRQ_HANDLED;
}


/*******************************************************
Description:
	Novatek touchscreen check chip version trim function.

return:
	Executive outcomes. 0---NVT IC. -1---not NVT IC.
*******************************************************/
static int8_t nvt_ts_check_chip_ver_trim(uint32_t chip_ver_trim_addr)
{
	uint8_t buf[8] = {0};
	int32_t retry = 0;
	int32_t list = 0;
	int32_t i = 0;
	int32_t found_nvt_chip = 0;
	int32_t ret = -1;

	//---Check for 5 times---
	for (retry = 5; retry > 0; retry--) {

		nvt_bootloader_reset();

		nvt_set_page(chip_ver_trim_addr);

		buf[0] = chip_ver_trim_addr & 0x7F;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		buf[6] = 0x00;
		CTP_SPI_READ(ts->client, buf, 7);
		NVT_LOG("buf[1]=0x%02X, buf[2]=0x%02X, buf[3]=0x%02X, buf[4]=0x%02X, buf[5]=0x%02X, buf[6]=0x%02X\n",
			buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);

		// compare read chip id on supported list
		for (list = 0; list < (sizeof(trim_id_table) / sizeof(struct nvt_ts_trim_id_table)); list++) {
			found_nvt_chip = 0;

			// compare each byte
			for (i = 0; i < NVT_ID_BYTE_MAX; i++) {
				if (trim_id_table[list].mask[i]) {
					if (buf[i + 1] != trim_id_table[list].id[i])
						break;
				}
			}

			if (i == NVT_ID_BYTE_MAX) {
				found_nvt_chip = 1;
			}

			if (found_nvt_chip) {
				NVT_LOG("This is NVT touch IC\n");
				ts->mmap = trim_id_table[list].mmap;
				ts->hw_crc = trim_id_table[list].hwinfo->hw_crc;
				ret = 0;
				goto out;
			} else {
				ts->mmap = NULL;
				ret = -1;
			}
		}

		msleep(10);
	}

out:
	return ret;
}

static void nvt_resume_work(struct work_struct *work)
{
	NVT_LOG("start\n");
	nvt_ts_resume(&ts->client->dev);
	NVT_LOG("end\n");
}

/*static void nvt_suspend_work(struct work_struct *work)
{
	NVT_LOG("start\n");
	nvt_ts_suspend(&ts->client->dev);
	NVT_LOG("end\n");
}*/

/*******************************************************
Description:
	Novatek touchscreen driver probe function.

return:
	Executive outcomes. 0---succeed. negative---failed
*******************************************************/
#if WAKEUP_GESTURE
int nvt_gesture_switch(struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
	if (type == EV_SYN && code == SYN_CONFIG) {
		if (value == WAKEUP_OFF) {
			g_gesture_flag = false;
#if IS_ENABLED(CONFIG_XIAOMI_TOUCH_NOTIFIER)
			touch_gesture_data.gesture_enable = XIAOMI_TOUCH_GESTURE_DISABLE;
			xiaomi_touch_notifier_call_chain(XIAOMI_TOUCH_GESTURE_EVENT_SWITCH, &touch_gesture_data);
#endif
			NVT_LOG("gesture disabled:%d", g_gesture_flag);
		} else if (value == WAKEUP_ON) {
			g_gesture_flag = true;
#if IS_ENABLED(CONFIG_XIAOMI_TOUCH_NOTIFIER)
			touch_gesture_data.gesture_enable = XIAOMI_TOUCH_GESTURE_ENABLE;
			xiaomi_touch_notifier_call_chain(XIAOMI_TOUCH_GESTURE_EVENT_SWITCH, &touch_gesture_data);
#endif
			NVT_LOG("gesture enabled:%d", g_gesture_flag);
		}
	}
	return 0;
}
#endif

void nvt_charger_mode_work(struct work_struct *work)
{
	u8 buf[8] = {0};
	if (nvt_charger_flag == XIAOMI_USB_ENABLE) {
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x53; /*USB IN*/
		CTP_SPI_WRITE(ts->client, buf, 2);
		NVT_LOG("nvt in charger mode\n");
	} else if (nvt_charger_flag == XIAOMI_USB_DISABLE){
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x51; /*USB OUT*/
		CTP_SPI_WRITE(ts->client, buf, 2);
		NVT_LOG("nvt out charger mode\n");
	}
}

static int nvt_detect_charger_notifier_callback(struct notifier_block *self,
                                unsigned long event, void *data)
{
	int ret;
	struct xiaomi_usb_notify_data  *evdata = data;

	NVT_LOG("nvt_detect_charger_notifier_callback start\n");

	nvt_charger_flag = evdata->usb_touch_enable;
	ret = queue_work(ts->nvt_charger_detect_workqueue, &ts->nvt_charger_detect_work);
	NVT_LOG("nvt_detect_charger_notifier_callback end\n");
	return 0;
}

static int32_t nvt_ts_probe(struct spi_device *client)
{
	int32_t ret = 0;

#if ((TOUCH_KEY_NUM > 0) || WAKEUP_GESTURE)
	int32_t retry = 0;
#endif

	NVT_LOG("start\n");

	ts = (struct nvt_ts_data *)kzalloc(sizeof(struct nvt_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		NVT_ERR("failed to allocated memory for nvt ts data\n");
		return -ENOMEM;
	}

	ts->xbuf = (uint8_t *)kzalloc(NVT_XBUF_LEN, GFP_KERNEL);
	if (ts->xbuf == NULL) {
		NVT_ERR("kzalloc for xbuf failed!\n");
		ret = -ENOMEM;
		goto err_malloc_xbuf;
	}

	ts->rbuf = (uint8_t *)kzalloc(NVT_READ_LEN, GFP_KERNEL);
	if(ts->rbuf == NULL) {
		NVT_ERR("kzalloc for rbuf failed!\n");
		ret = -ENOMEM;
		goto err_malloc_rbuf;
	}

    if (ts->ts_tp_class == NULL)
#if IS_ENABLED(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE)
		ts->ts_tp_class = get_xiaomi_touch_class();
#else
		ts->ts_tp_class = class_create(THIS_MODULE, "touch");
#endif
	ts->ts_touch_dev =
		device_create(ts->ts_tp_class, NULL, 0x72, ts, "tp_dev");
	if (IS_ERR(ts->ts_touch_dev)) {
			printk("%s ERROR: Failed to create device for the sysfs!\n",__func__);
			goto err_create_class;
	}

	ts->client = client;
	spi_set_drvdata(client, ts);

	//---prepare for spi parameter---
	if (ts->client->master->flags & SPI_MASTER_HALF_DUPLEX) {
		NVT_ERR("Full duplex not supported by master\n");
		ret = -EIO;
		goto err_ckeck_full_duplex;
	}
	ts->client->bits_per_word = 8;
	ts->client->mode = SPI_MODE_0;

	ret = spi_setup(ts->client);
	if (ret < 0) {
		NVT_ERR("Failed to perform SPI setup\n");
		goto err_spi_setup;
	}

#ifdef CONFIG_MTK_SPI
    /* old usage of MTK spi API */
    memcpy(&ts->spi_ctrl, &spi_ctrdata, sizeof(struct mt_chip_conf));
    ts->client->controller_data = (void *)&ts->spi_ctrl;
#endif

#ifdef CONFIG_SPI_MT65XX
    /* new usage of MTK spi API */
    memcpy(&ts->spi_ctrl, &spi_ctrdata, sizeof(struct mtk_chip_config));
    ts->client->controller_data = (void *)&ts->spi_ctrl;
#endif

	NVT_LOG("mode=%d, max_speed_hz=%d\n", ts->client->mode, ts->client->max_speed_hz);

	//---parse dts---
	ret = nvt_parse_dt(&client->dev);
	if (ret) {
		NVT_ERR("parse dt error\n");
		goto err_spi_setup;
	}

	//---request and config GPIOs---
	ret = nvt_gpio_config(ts);
	if (ret) {
		NVT_ERR("gpio config error!\n");
		goto err_gpio_config_failed;
	}

	mutex_init(&ts->lock);
	mutex_init(&ts->xbuf_lock);

	//---eng reset before TP_RESX high
	nvt_eng_reset();

#if NVT_TOUCH_SUPPORT_HW_RST
	gpio_set_value(ts->reset_gpio, 1);
#endif

	// need 10ms delay after POR(power on reset)
	msleep(10);

	//---check chip version trim---
	ret = nvt_ts_check_chip_ver_trim(CHIP_VER_TRIM_ADDR);
	if (ret) {
		NVT_LOG("try to check from old chip ver trim address\n");
		ret = nvt_ts_check_chip_ver_trim(CHIP_VER_TRIM_OLD_ADDR);
		if (ret) {
			NVT_ERR("chip is not identified\n");
			ret = -EINVAL;
			goto err_chipvertrim_failed;
		}
	}

	ts->abs_x_max = TOUCH_DEFAULT_MAX_WIDTH;
	ts->abs_y_max = TOUCH_DEFAULT_MAX_HEIGHT;

	//---allocate input device---
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		NVT_ERR("allocate input device failed\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

	ts->max_touch_num = TOUCH_MAX_FINGER_NUM;

#if TOUCH_KEY_NUM > 0
	ts->max_button_num = TOUCH_KEY_NUM;
#endif

	ts->int_trigger_type = INT_TRIGGER_TYPE;


	//---set input device info.---
	ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	ts->input_dev->propbit[0] = BIT(INPUT_PROP_DIRECT);

#if MT_PROTOCOL_B
	input_mt_init_slots(ts->input_dev, ts->max_touch_num, 0);
#endif

	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, TOUCH_FORCE_NUM, 0, 0);    //pressure = TOUCH_FORCE_NUM

#if TOUCH_MAX_FINGER_NUM > 1
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);    //area = 255

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->abs_y_max, 0, 0);
#if MT_PROTOCOL_B
	// no need to set ABS_MT_TRACKING_ID, input_mt_init_slots() already set it
#else
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, ts->max_touch_num, 0, 0);
#endif //MT_PROTOCOL_B
#endif //TOUCH_MAX_FINGER_NUM > 1

#if TOUCH_KEY_NUM > 0
	for (retry = 0; retry < ts->max_button_num; retry++) {
		input_set_capability(ts->input_dev, EV_KEY, touch_key_array[retry]);
	}
#endif

#if WAKEUP_GESTURE
	for (retry = 0; retry < (sizeof(gesture_key_array) / sizeof(gesture_key_array[0])); retry++) {
		input_set_capability(ts->input_dev, EV_KEY, gesture_key_array[retry]);
	}
	ts->input_dev->event = nvt_gesture_switch;
#endif

	sprintf(ts->phys, "input/ts");
	ts->input_dev->name = NVT_TS_NAME;
	ts->input_dev->phys = ts->phys;
	ts->input_dev->id.bustype = BUS_SPI;

	//---register input device---
	ret = input_register_device(ts->input_dev);
	if (ret) {
		NVT_ERR("register input device (%s) failed. ret=%d\n", ts->input_dev->name, ret);
		goto err_input_register_device_failed;
	}

	if (ts->pen_support) {
		//---allocate pen input device---
		ts->pen_input_dev = input_allocate_device();
		if (ts->pen_input_dev == NULL) {
			NVT_ERR("allocate pen input device failed\n");
			ret = -ENOMEM;
			goto err_pen_input_dev_alloc_failed;
		}

		//---set pen input device info.---
		ts->pen_input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
		ts->pen_input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
		ts->pen_input_dev->keybit[BIT_WORD(BTN_TOOL_PEN)] |= BIT_MASK(BTN_TOOL_PEN);
		//ts->pen_input_dev->keybit[BIT_WORD(BTN_TOOL_RUBBER)] |= BIT_MASK(BTN_TOOL_RUBBER);
		ts->pen_input_dev->keybit[BIT_WORD(BTN_STYLUS)] |= BIT_MASK(BTN_STYLUS);
		ts->pen_input_dev->keybit[BIT_WORD(BTN_STYLUS2)] |= BIT_MASK(BTN_STYLUS2);
		ts->pen_input_dev->propbit[0] = BIT(INPUT_PROP_DIRECT);

		if (ts->stylus_resol_double) {
			input_set_abs_params(ts->pen_input_dev, ABS_X, 0, ts->abs_x_max * 2, 0, 0);
			input_set_abs_params(ts->pen_input_dev, ABS_Y, 0, ts->abs_y_max * 2, 0, 0);
		} else {
			input_set_abs_params(ts->pen_input_dev, ABS_X, 0, ts->abs_x_max, 0, 0);
			input_set_abs_params(ts->pen_input_dev, ABS_Y, 0, ts->abs_y_max, 0, 0);
		}
		input_set_abs_params(ts->pen_input_dev, ABS_PRESSURE, 0, PEN_PRESSURE_MAX, 0, 0);
		input_set_abs_params(ts->pen_input_dev, ABS_DISTANCE, 0, PEN_DISTANCE_MAX, 0, 0);
		input_set_abs_params(ts->pen_input_dev, ABS_TILT_X, PEN_TILT_MIN, PEN_TILT_MAX, 0, 0);
		input_set_abs_params(ts->pen_input_dev, ABS_TILT_Y, PEN_TILT_MIN, PEN_TILT_MAX, 0, 0);

		sprintf(ts->pen_phys, "input/pen");
		ts->pen_input_dev->name = NVT_PEN_NAME;
		ts->pen_input_dev->phys = ts->pen_phys;
		ts->pen_input_dev->id.bustype = BUS_SPI;

		//---register pen input device---
		ret = input_register_device(ts->pen_input_dev);
		if (ret) {
			NVT_ERR("register pen input device (%s) failed. ret=%d\n", ts->pen_input_dev->name, ret);
			goto err_pen_input_register_device_failed;
		}
	} /* if (ts->pen_support) */

	//---set int-pin & request irq---
	client->irq = gpio_to_irq(ts->irq_gpio);
	if (client->irq) {
		NVT_LOG("int_trigger_type=%d\n", ts->int_trigger_type);
		ts->irq_enabled = true;
		ret = request_threaded_irq(client->irq, NULL, nvt_ts_work_func,
				ts->int_trigger_type | IRQF_ONESHOT, NVT_SPI_NAME, ts);
		if (ret != 0) {
			NVT_ERR("request irq failed. ret=%d\n", ret);
			goto err_int_request_failed;
		} else {
			nvt_irq_enable(false);
			NVT_LOG("request irq %d succeed\n", client->irq);
		}
	}

#if WAKEUP_GESTURE
	device_init_wakeup(&ts->input_dev->dev, 1);
#endif

#if BOOT_UPDATE_FIRMWARE
	nvt_fwu_wq = alloc_workqueue("nvt_fwu_wq", WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!nvt_fwu_wq) {
		NVT_ERR("nvt_fwu_wq create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_nvt_fwu_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->nvt_fwu_work, Boot_Update_Firmware);
	// please make sure boot update start after display reset(RESX) sequence
	queue_delayed_work(nvt_fwu_wq, &ts->nvt_fwu_work, msecs_to_jiffies(14000));
#endif

	NVT_LOG("NVT_TOUCH_ESD_PROTECT is %d\n", NVT_TOUCH_ESD_PROTECT);
#if NVT_TOUCH_ESD_PROTECT
	INIT_DELAYED_WORK(&nvt_esd_check_work, nvt_esd_check_func);
	nvt_esd_check_wq = alloc_workqueue("nvt_esd_check_wq", WQ_MEM_RECLAIM, 1);
	if (!nvt_esd_check_wq) {
		NVT_ERR("nvt_esd_check_wq create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_nvt_esd_check_wq_failed;
	}
	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	//---set device node---
#if NVT_TOUCH_PROC
	ret = nvt_flash_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt flash proc init failed. ret=%d\n", ret);
		goto err_flash_proc_init_failed;
	}
#endif

#if NVT_TOUCH_EXT_PROC
	ret = nvt_extra_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt extra proc init failed. ret=%d\n", ret);
		goto err_extra_proc_init_failed;
	}
#endif

#if NVT_TOUCH_MP
	ret = nvt_mp_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt mp proc init failed. ret=%d\n", ret);
		goto err_mp_proc_init_failed;
	}
#endif
    ts->wait_workqueue = create_singlethread_workqueue("nvt_suspend_resume_wq");
    if(!ts->wait_workqueue) {
        NVT_ERR("create nvt resume workqueue fail");
	}

    if(ts->wait_workqueue) {
        INIT_DELAYED_WORK(&ts->resume_work,nvt_resume_work);
        //INIT_DELAYED_WORK(&ts->suspend_work,nvt_suspend_work);
    }


	ts->nvt_charger_detect_workqueue = create_singlethread_workqueue("nvt_charger_detect");
	if(!ts->nvt_charger_detect_workqueue) {
        NVT_ERR("create charger workqueue fail");
	}

	if(ts->nvt_charger_detect_workqueue) {
		INIT_WORK(&ts->nvt_charger_detect_work, nvt_charger_mode_work);
	}

	proc_node_init();
#if IS_ENABLED(CONFIG_XIAOMI_PANEL_NOTIFIER)
	ts->xiaomi_panel_notif.notifier_call = nvt_xiaomi_panel_notifier_callback;
	ret = xiaomi_panel_notifier_register_client(&ts->xiaomi_panel_notif);
	if (ret) {
		NVT_ERR("register xiaomi_panel_notif failed. ret=%d\n", ret);
		goto err_register_xiaomi_panel_notif_failed;
	} else {
		NVT_ERR("register xiaomi_panel_notif successfully. ret=%d\n", ret);
	}
#elif defined(_MSM_DRM_NOTIFY_H_)
	ts->drm_notif.notifier_call = nvt_drm_notifier_callback;
	ret = msm_drm_register_client(&ts->drm_notif);
	if(ret) {
		NVT_ERR("register drm_notifier failed. ret=%d\n", ret);
		goto err_register_drm_notif_failed;
	}
#elif defined(CONFIG_FB)
	ts->fb_notif.notifier_call = nvt_fb_notifier_callback;
	ret = fb_register_client(&ts->fb_notif);
	if(ret) {
		NVT_ERR("register fb_notifier failed. ret=%d\n", ret);
		goto err_register_fb_notif_failed;
	}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = nvt_ts_early_suspend;
	ts->early_suspend.resume = nvt_ts_late_resume;
	ret = register_early_suspend(&ts->early_suspend);
	if(ret) {
		NVT_ERR("register early suspend failed. ret=%d\n", ret);
		goto err_register_early_suspend_failed;
	}
#endif

	ts->nvt_charger_detect_notif.notifier_call = nvt_detect_charger_notifier_callback;
	ret = xiaomi_usb_touch_notifier_register_client(&ts->nvt_charger_detect_notif);
	if(ret) {
		NVT_ERR("register charger_detect_notifier failed. ret=%d\n", ret);
		goto err_register_charger_nofif_failed;
	}

#if IS_ENABLED(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE)
	//xiaomi_touch_interfaces.touch_vendor_read = nvt_touch_vendor_read;
	//xiaomi_touch_interfaces.panel_display_read = nvt_panel_display_read;
	//xiaomi_touch_interfaces.panel_vendor_read = nvt_panel_vendor_read;
	//xiaomi_touch_interfaces.panel_color_read = nvt_panel_color_read;
	xiaomi_touch_interfaces.getModeValue = nvt_get_mode_value;
	xiaomi_touch_interfaces.setModeValue = nvt_set_cur_value;
	xiaomi_touch_interfaces.resetMode = nvt_reset_mode;
	xiaomi_touch_interfaces.getModeAll = nvt_get_mode_all;
	xiaomi_touch_interfaces.palm_sensor_write = nvt_palm_sensor_write;
	nvt_init_touchmode_data();
	xiaomitouch_register_modedata(&xiaomi_touch_interfaces);
#endif

	bTouchIsAwake = 1;
	NVT_LOG("end\n");

	nvt_irq_enable(true);

	return 0;

#if IS_ENABLED(CONFIG_XIAOMI_PANEL_NOTIFIER)
err_register_xiaomi_panel_notif_failed:
#elif defined(_MSM_DRM_NOTIFY_H_)
	if (msm_drm_unregister_client(&ts->drm_notif))
		NVT_ERR("Error occurred while unregistering drm_notifier.\n");
err_register_drm_notif_failed:
#elif defined(CONFIG_FB)
	if (fb_unregister_client(&ts->fb_notif))
		NVT_ERR("Error occurred while unregistering fb_notifier.\n");
err_register_fb_notif_failed:
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts->early_suspend);
err_register_early_suspend_failed:
#endif
err_register_charger_nofif_failed:
	if(xiaomi_usb_touch_notifier_unregister_client(&ts->nvt_charger_detect_notif))
		NVT_ERR("Error occurred while unregistering nvt_charger_detect_notifier.\n");
#if NVT_TOUCH_MP
	nvt_mp_proc_deinit();
err_mp_proc_init_failed:
#endif
#if NVT_TOUCH_EXT_PROC
	nvt_extra_proc_deinit();
err_extra_proc_init_failed:
#endif
#if NVT_TOUCH_PROC
	nvt_flash_proc_deinit();
err_flash_proc_init_failed:
#endif
#if NVT_TOUCH_ESD_PROTECT
	if (nvt_esd_check_wq) {
		cancel_delayed_work_sync(&nvt_esd_check_work);
		destroy_workqueue(nvt_esd_check_wq);
		nvt_esd_check_wq = NULL;
	}
err_create_nvt_esd_check_wq_failed:
#endif
#if BOOT_UPDATE_FIRMWARE
	if (nvt_fwu_wq) {
		cancel_delayed_work_sync(&ts->nvt_fwu_work);
		destroy_workqueue(nvt_fwu_wq);
		nvt_fwu_wq = NULL;
	}
err_create_nvt_fwu_wq_failed:
#endif
#if WAKEUP_GESTURE
	device_init_wakeup(&ts->input_dev->dev, 0);
#endif
	free_irq(client->irq, ts);
err_int_request_failed:
	if (ts->pen_support) {
		input_unregister_device(ts->pen_input_dev);
		ts->pen_input_dev = NULL;
	}
err_pen_input_register_device_failed:
	if (ts->pen_support) {
		if (ts->pen_input_dev) {
			input_free_device(ts->pen_input_dev);
			ts->pen_input_dev = NULL;
		}
	}
err_pen_input_dev_alloc_failed:
	input_unregister_device(ts->input_dev);
	ts->input_dev = NULL;
err_input_register_device_failed:
	if (ts->input_dev) {
		input_free_device(ts->input_dev);
		ts->input_dev = NULL;
	}
err_input_dev_alloc_failed:
err_chipvertrim_failed:
	mutex_destroy(&ts->xbuf_lock);
	mutex_destroy(&ts->lock);
	nvt_gpio_deconfig(ts);
err_gpio_config_failed:
err_spi_setup:
err_ckeck_full_duplex:
	spi_set_drvdata(client, NULL);
	if (ts->rbuf) {
		kfree(ts->rbuf);
		ts->rbuf = NULL;
	}

err_create_class:
	device_destroy(ts->ts_tp_class, 0x72);

err_malloc_rbuf:
	if (ts->xbuf) {
		kfree(ts->xbuf);
		ts->xbuf = NULL;
	}
err_malloc_xbuf:
	if (ts) {
		kfree(ts);
		ts = NULL;
	}
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen driver release function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_ts_remove(struct spi_device *client)
{
	NVT_LOG("Removing driver...\n");

#if IS_ENABLED(CONFIG_XIAOMI_PANEL_NOTIFIER)
	if (xiaomi_panel_notifier_unregister_client(&ts->xiaomi_panel_notif))
		NVT_ERR("Error occurred while unregistering xiaomi_panel_notif.\n");
#elif defined(_MSM_DRM_NOTIFY_H_)
	if (msm_drm_unregister_client(&ts->drm_notif))
		NVT_ERR("Error occurred while unregistering drm_notifier.\n");
#elif defined(CONFIG_FB)
	if (fb_unregister_client(&ts->fb_notif))
		NVT_ERR("Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts->early_suspend);
#endif

#if NVT_TOUCH_MP
	nvt_mp_proc_deinit();
#endif
#if NVT_TOUCH_EXT_PROC
	nvt_extra_proc_deinit();
#endif
#if NVT_TOUCH_PROC
	nvt_flash_proc_deinit();
#endif
	proc_node_deinit();

    if (ts->wait_workqueue) {
        cancel_delayed_work_sync(&ts->resume_work);
        //cancel_delayed_work_sync(&ts->suspend_work);
        destroy_workqueue(ts->wait_workqueue);
    }
#if NVT_TOUCH_ESD_PROTECT
	if (nvt_esd_check_wq) {
		cancel_delayed_work_sync(&nvt_esd_check_work);
		nvt_esd_check_enable(false);
		destroy_workqueue(nvt_esd_check_wq);
		nvt_esd_check_wq = NULL;
	}
#endif

#if BOOT_UPDATE_FIRMWARE
	if (nvt_fwu_wq) {
		cancel_delayed_work_sync(&ts->nvt_fwu_work);
		destroy_workqueue(nvt_fwu_wq);
		nvt_fwu_wq = NULL;
	}
#endif

#if WAKEUP_GESTURE
	device_init_wakeup(&ts->input_dev->dev, 0);
#endif

	nvt_irq_enable(false);
	free_irq(client->irq, ts);

	mutex_destroy(&ts->xbuf_lock);
	mutex_destroy(&ts->lock);

	nvt_gpio_deconfig(ts);

	if (ts->pen_support) {
		if (ts->pen_input_dev) {
			input_unregister_device(ts->pen_input_dev);
			ts->pen_input_dev = NULL;
		}
	}

	if (ts->input_dev) {
		input_unregister_device(ts->input_dev);
		ts->input_dev = NULL;
	}

	spi_set_drvdata(client, NULL);

	if (ts->xbuf) {
		kfree(ts->xbuf);
		ts->xbuf = NULL;
	}

	if (ts) {
		kfree(ts);
		ts = NULL;
	}

	return 0;
}

static void nvt_ts_shutdown(struct spi_device *client)
{
	NVT_LOG("Shutdown driver...\n");

	nvt_irq_enable(false);

#if IS_ENABLED(CONFIG_XIAOMI_PANEL_NOTIFIER)
	if (xiaomi_panel_notifier_unregister_client(&ts->xiaomi_panel_notif))
		NVT_ERR("Error occurred while unregistering xiaomi_panel_notif.\n");
#elif defined(_MSM_DRM_NOTIFY_H_)
	if (msm_drm_unregister_client(&ts->drm_notif))
		NVT_ERR("Error occurred while unregistering drm_notifier.\n");
#elif defined(CONFIG_FB)
	if (fb_unregister_client(&ts->fb_notif))
		NVT_ERR("Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts->early_suspend);
#endif

#if NVT_TOUCH_MP
	nvt_mp_proc_deinit();
#endif
#if NVT_TOUCH_EXT_PROC
	nvt_extra_proc_deinit();
#endif
#if NVT_TOUCH_PROC
	nvt_flash_proc_deinit();
#endif

#if NVT_TOUCH_ESD_PROTECT
	if (nvt_esd_check_wq) {
		cancel_delayed_work_sync(&nvt_esd_check_work);
		nvt_esd_check_enable(false);
		destroy_workqueue(nvt_esd_check_wq);
		nvt_esd_check_wq = NULL;
	}
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#if BOOT_UPDATE_FIRMWARE
	if (nvt_fwu_wq) {
		cancel_delayed_work_sync(&ts->nvt_fwu_work);
		destroy_workqueue(nvt_fwu_wq);
		nvt_fwu_wq = NULL;
	}
#endif

#if WAKEUP_GESTURE
	device_init_wakeup(&ts->input_dev->dev, 0);
#endif
}

/*******************************************************
Description:
	Novatek touchscreen driver suspend function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_ts_suspend(struct device *dev)
{
	uint8_t buf[4] = {0};
#if MT_PROTOCOL_B
	uint32_t i = 0;
#endif

	if (!bTouchIsAwake) {
		NVT_LOG("Touch is already suspend\n");
		return 0;
	}

#if WAKEUP_GESTURE
	if (g_gesture_flag == false)
		nvt_irq_enable(false);
#else
	nvt_irq_enable(false);
#endif


#if NVT_TOUCH_ESD_PROTECT
	NVT_LOG("cancel delayed work sync\n");
	cancel_delayed_work_sync(&nvt_esd_check_work);
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	mutex_lock(&ts->lock);

	NVT_LOG("start\n");

	bTouchIsAwake = 0;

#if IS_ENABLED(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE)
	update_palm_sensor_value(0);
	if (ts->palm_sensor_switch) {
		NVT_LOG("palm sensor on status, switch to off\n");
		nvt_set_pocket_palm_switch(false);
		ts->palm_sensor_switch = false;
	}
#endif

#if WAKEUP_GESTURE
	//---write command to enter "wakeup gesture mode"---
	if (g_gesture_flag == true) {
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x13;
		CTP_SPI_WRITE(ts->client, buf, 2);

		//enable_irq_wake(ts->client->irq);
		nvt_enable_irq_wake(true);

		NVT_LOG("Enabled touch wakeup gesture\n");
	} else { // WAKEUP_GESTURE
		//---write command to enter "deep sleep mode"---
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x11;
		CTP_SPI_WRITE(ts->client, buf, 2);
	}
#endif // WAKEUP_GESTURE

	mutex_unlock(&ts->lock);

	/* release all touches */
#if MT_PROTOCOL_B
	for (i = 0; i < ts->max_touch_num; i++) {
		input_mt_slot(ts->input_dev, i);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
	}
#endif
	input_report_key(ts->input_dev, BTN_TOUCH, 0);
#if !MT_PROTOCOL_B
	input_mt_sync(ts->input_dev);
#endif
	input_sync(ts->input_dev);

	/* release pen event */
	if (ts->pen_support) {
		input_report_abs(ts->pen_input_dev, ABS_X, 0);
		input_report_abs(ts->pen_input_dev, ABS_Y, 0);
		input_report_abs(ts->pen_input_dev, ABS_PRESSURE, 0);
		input_report_abs(ts->pen_input_dev, ABS_TILT_X, 0);
		input_report_abs(ts->pen_input_dev, ABS_TILT_Y, 0);
		input_report_abs(ts->pen_input_dev, ABS_DISTANCE, 0);
		input_report_key(ts->pen_input_dev, BTN_TOUCH, 0);
		input_report_key(ts->pen_input_dev, BTN_TOOL_PEN, 0);
		input_report_key(ts->pen_input_dev, BTN_STYLUS, 0);
		input_report_key(ts->pen_input_dev, BTN_STYLUS2, 0);
		input_sync(ts->pen_input_dev);
	}

	msleep(50);

	NVT_LOG("end\n");

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen driver resume function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_ts_resume(struct device *dev)
{
	u8 buf[8] = {0};
	/*if (bTouchIsAwake) {
		NVT_LOG("Touch is already resume\n");
		return 0;
	}*/

	mutex_lock(&ts->lock);

	NVT_LOG("start\n");

	// please make sure display reset(RESX) sequence and mipi dsi cmds sent before this
#if NVT_TOUCH_SUPPORT_HW_RST
	gpio_set_value(ts->reset_gpio, 1);
#endif
	if (nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME)) {
		NVT_ERR("download firmware failed, ignore check fw state\n");
	} else {
		nvt_check_fw_reset_state(RESET_STATE_REK);
	}



	if (g_gesture_flag == true) {
		nvt_enable_irq_wake(false);
	}

	nvt_irq_enable(true);


#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	bTouchIsAwake = 1;

	mutex_unlock(&ts->lock);

	msleep(50);

	if(g_priximity_enable){
		NVT_LOG("g_priximity_enable is %d\n",g_priximity_enable);
		nvt_set_proximity_switch(1);
	}
#if IS_ENABLED(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE)
	mutex_lock(&ts->lock);
	NVT_LOG("palm_sensor_switch=%d", ts->palm_sensor_switch);
	if (ts->palm_sensor_switch) {
		NVT_LOG("palm sensor on status, switch to on\n");
		nvt_set_pocket_palm_switch(true);
	}
	mutex_unlock(&ts->lock);
#endif

	if (nvt_charger_flag == XIAOMI_USB_ENABLE) {
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x53;
		CTP_SPI_WRITE(ts->client, buf, 2);
		NVT_LOG("nvt in charger mode\n");
    } else if (nvt_charger_flag == XIAOMI_USB_DISABLE){
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x51;
		CTP_SPI_WRITE(ts->client, buf, 2);
		NVT_LOG("nvt out charger mode\n");
	}

	NVT_LOG("end\n");

	return 0;
}

#if IS_ENABLED(CONFIG_XIAOMI_PANEL_NOTIFIER)
static int nvt_xiaomi_panel_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct xiaomi_panel_notify_data *evdata = data;

	struct nvt_ts_data *ts_data =
		container_of(self, struct nvt_ts_data, xiaomi_panel_notif);

	NVT_ERR("%s: event = %lu\n", __func__, event);

	if (!evdata)
		return 0;

	NVT_ERR("%s: blank = %d\n", __func__, evdata->blank);

	if (ts_data) {
		if (event == XIAOMI_PANEL_EARLY_EVENT_BLANK) {
			if (evdata->blank == XIAOMI_PANEL_BLANK_POWERDOWN) {
				if(!g_priximity_enable){
				NVT_LOG("g_priximity_enable is %d\n",g_priximity_enable);
				nvt_ts_suspend(&ts->client->dev);
				//queue_delayed_work(ts->wait_workqueue,&ts->suspend_work,msecs_to_jiffies(0));
				}
			}
		} else if (event == XIAOMI_PANEL_NORMAL_EVENT_BLANK) {
			if (evdata->blank == XIAOMI_PANEL_BLANK_UNBLANK) {
				//nvt_ts_resume(&ts->client->dev);
				queue_delayed_work(ts->wait_workqueue,&ts->resume_work,msecs_to_jiffies(40));
			}
		}
	}

	return 0;
}
#elif defined(_MSM_DRM_NOTIFY_H_)
static int nvt_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
	struct msm_drm_notifier *evdata = data;
	int *blank;
	struct nvt_ts_data *ts =
		container_of(self, struct nvt_ts_data, drm_notif);

	if (!evdata || (evdata->id != 0))
		return 0;

	if (evdata->data && ts) {
		blank = evdata->data;
		if (event == MSM_DRM_EARLY_EVENT_BLANK) {
			if (*blank == MSM_DRM_BLANK_POWERDOWN) {
				NVT_LOG("event=%lu, *blank=%d\n", event, *blank);
				nvt_ts_suspend(&ts->client->dev);
			}
		} else if (event == MSM_DRM_EVENT_BLANK) {
			if (*blank == MSM_DRM_BLANK_UNBLANK) {
				NVT_LOG("event=%lu, *blank=%d\n", event, *blank);
				nvt_ts_resume(&ts->client->dev);
			}
		}
	}

	return 0;
}
#elif defined(CONFIG_FB)
static int nvt_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct nvt_ts_data *ts =
		container_of(self, struct nvt_ts_data, fb_notif);

	if (evdata && evdata->data && event == FB_EARLY_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_POWERDOWN) {
			NVT_LOG("event=%lu, *blank=%d\n", event, *blank);
			nvt_ts_suspend(&ts->client->dev);
		}
	} else if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
			NVT_LOG("event=%lu, *blank=%d\n", event, *blank);
			nvt_ts_resume(&ts->client->dev);
		}
	}

	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
/*******************************************************
Description:
	Novatek touchscreen driver early suspend function.

return:
	n.a.
*******************************************************/
static void nvt_ts_early_suspend(struct early_suspend *h)
{
	nvt_ts_suspend(ts->client, PMSG_SUSPEND);
}

/*******************************************************
Description:
	Novatek touchscreen driver late resume function.

return:
	n.a.
*******************************************************/
static void nvt_ts_late_resume(struct early_suspend *h)
{
	nvt_ts_resume(ts->client);
}
#endif

static const struct spi_device_id nvt_ts_id[] = {
	{ NVT_SPI_NAME, 0 },
	{ }
};

#ifdef CONFIG_OF
static struct of_device_id nvt_match_table[] = {
	{ .compatible = "novatek,NVT-ts-spi",},
	{ },
};
#endif

static struct spi_driver nvt_spi_driver = {
	.probe		= nvt_ts_probe,
	.remove		= nvt_ts_remove,
	.shutdown	= nvt_ts_shutdown,
	.id_table	= nvt_ts_id,
	.driver = {
		.name	= NVT_SPI_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = nvt_match_table,
#endif
	},
};

/*******************************************************
Description:
	Driver Install function.

return:
	Executive Outcomes. 0---succeed. not 0---failed.
********************************************************/
static int32_t __init nvt_driver_init(void)
{
	int32_t ret = 0;

	NVT_LOG("start\n");

	//---add spi driver---
	ret = spi_register_driver(&nvt_spi_driver);
	if (ret) {
		NVT_ERR("failed to add spi driver");
		goto err_driver;
	}

	NVT_LOG("finished\n");

err_driver:
	return ret;
}

/*******************************************************
Description:
	Driver uninstall function.

return:
	n.a.
********************************************************/
static void __exit nvt_driver_exit(void)
{
	spi_unregister_driver(&nvt_spi_driver);
}

#if IS_ENABLED(CONFIG_XIAOMI_PANEL_NOTIFIER)
late_initcall(nvt_driver_init);
#else
module_init(nvt_driver_init);
#endif
module_exit(nvt_driver_exit);

MODULE_DESCRIPTION("Novatek Touchscreen Driver");
MODULE_LICENSE("GPL");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif
