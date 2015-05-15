/* drivers/input/touchscreen/maxim_sti.c
 *
 * Maxim SmartTouch Imager Touchscreen Driver
 *
 * Copyright (c)2013 Maxim Integrated Products, Inc.
 * Copyright (C) 2013, NVIDIA Corporation.  All Rights Reserved.
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
#include <linux/moduleparam.h>
#include <linux/kmod.h>
#include <linux/kthread.h>
#include <linux/spi/spi.h>
#include <linux/firmware.h>
#include <linux/crc16.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/regulator/consumer.h>
#include <linux/maxim_sti.h>
#include <asm/byteorder.h>  /* MUST include this header to get byte order */


/****************************************************************************\
* Custom features                                                            *
\****************************************************************************/

#define INPUT_DEVICES         1
#define INPUT_ENABLE_DISABLE  1
#define SUSPEND_POWER_OFF     0
#define NV_ENABLE_CPU_BOOST   1
#define HI02_WORKAROUND       1

#if NV_ENABLE_CPU_BOOST
#define INPUT_IDLE_PERIOD     (msecs_to_jiffies(50))
#endif

#if HI02_WORKAROUND
u16 read_value[2] = { 0 };
#endif
/****************************************************************************\
* Device context structure, globals, and macros                              *
\****************************************************************************/

struct dev_data;

struct chip_access_method {
	int (*read)(struct dev_data *dd, u16 address, u8 *buf, u16 len);
	int (*write)(struct dev_data *dd, u16 address, u8 *buf, u16 len);
};

struct dev_data {
	u8                           *tx_buf;
	u8                           *rx_buf;
	u8                           send_fail_count;
	u32                          nl_seq;
	u8                           nl_mc_group_count;
	bool                         nl_enabled;
	bool                         start_fusion;
	bool                         suspended;
	bool                         suspend_in_progress;
	bool                         resume_in_progress;
	bool                         expect_resume_ack;
	bool                         eraser_active;
	bool                         legacy_acceleration;
#if INPUT_ENABLE_DISABLE
	bool                         input_no_deconfig;
#endif
	bool                         irq_registered;
	u16                          nirq_params;
	u16                          irq_param[MAX_IRQ_PARAMS];
	pid_t                        fusion_process;
	char                         input_phys[128];
	struct input_dev             *input_dev[INPUT_DEVICES];
	struct completion            suspend_resume;
	struct chip_access_method    chip;
	struct spi_device            *spi;
	struct genl_family           nl_family;
	struct genl_ops              *nl_ops;
	struct genl_multicast_group  *nl_mc_groups;
	struct sk_buff               *outgoing_skb;
	struct sk_buff_head          incoming_skb_queue;
	struct task_struct           *thread;
	struct sched_param           thread_sched;
	struct list_head             dev_list;
	struct regulator             *reg_avdd;
	struct regulator             *reg_dvdd;
	void                         (*service_irq)(struct dev_data *dd);
#if NV_ENABLE_CPU_BOOST
	unsigned long                last_irq_jiffies;
#endif
	struct kobject               *parent;
	u8                           sysfs_created;
	u16                          chip_id;
	u16                          glove_enabled;
	u16                          gesture_en;
	u16                          charger_mode_en;
	u16                          lcd_fps;
	u16                          tf_ver;
	u16                          drv_ver;
	u16                          fw_ver;
	u16                          fw_protocol;
	u32                          tf_status;
	bool                         idle;
	bool                         resume_reset;
	bool                         compatibility_mode;
	unsigned long int            sysfs_update_type;
	struct completion            sysfs_ack_glove;
	struct completion            sysfs_ack_charger;
	struct completion            sysfs_ack_lcd_fps;
	struct mutex                 sysfs_update_mutex;
};

static unsigned short    panel_id;
static struct list_head  dev_list;
static spinlock_t        dev_lock;

//static u16 int_counter;

static void gesture_wake_detect(struct dev_data *dd);
static irqreturn_t irq_handler(int irq, void *context);
static void service_irq(struct dev_data *dd);
static void service_irq_legacy_acceleration(struct dev_data *dd);

#define ERROR(a, b...) printk(KERN_ERR "%s driver(ERROR:%s:%d): " a "\n", \
			      dd->nl_family.name, __func__, __LINE__, ##b)
#define INFO(a, b...) printk(KERN_INFO "%s driver: " a "\n", \
			     dd->nl_family.name, ##b)

/****************************************************************************\
* Chip access methods                                                        *
\****************************************************************************/

static inline int
spi_read_123(struct dev_data *dd, u16 address, u8 *buf, u16 len, bool add_len)
{
	struct spi_message   message;
	struct spi_transfer  transfer;
	u16                  *tx_buf = (u16 *)dd->tx_buf;
	u16                  *rx_buf = (u16 *)dd->rx_buf;
	u16                  words = len / sizeof(u16), header_len = 1;
	u16                  *ptr2 = rx_buf + 1;
#ifdef __LITTLE_ENDIAN
	u16                  *ptr1 = (u16 *)buf, i;
#endif
	int                  ret;

	if (tx_buf == NULL || rx_buf == NULL)
		return -ENOMEM;

	tx_buf[0] = (address << 1) | 0x0001;
#ifdef __LITTLE_ENDIAN
	tx_buf[0] = (tx_buf[0] << 8) | (tx_buf[0] >> 8);
#endif

	if (add_len) {
		tx_buf[1] = words;
#ifdef __LITTLE_ENDIAN
		tx_buf[1] = (tx_buf[1] << 8) | (tx_buf[1] >> 8);
#endif
		ptr2++;
		header_len++;
	}

	spi_message_init(&message);
	memset(&transfer, 0, sizeof(transfer));

	transfer.len = len + header_len * sizeof(u16);
	transfer.tx_buf = tx_buf;
	transfer.rx_buf = rx_buf;
	spi_message_add_tail(&transfer, &message);

	do {
		ret = spi_sync(dd->spi, &message);
	} while (ret == -EAGAIN);

#ifdef __LITTLE_ENDIAN
	for (i = 0; i < words; i++)
		ptr1[i] = (ptr2[i] << 8) | (ptr2[i] >> 8);
#else
	memcpy(buf, ptr2, len);
#endif
	return ret;
}

static inline int
spi_write_123(struct dev_data *dd, u16 address, u8 *buf, u16 len,
	      bool add_len)
{
	struct maxim_sti_pdata  *pdata = dd->spi->dev.platform_data;
	u16                     *tx_buf = (u16 *)dd->tx_buf;
	u16                     words = len / sizeof(u16), header_len = 1;
#ifdef __LITTLE_ENDIAN
	u16                     i;
#endif
	int  ret;

	if (tx_buf == NULL)
		return -ENOMEM;

	tx_buf[0] = address << 1;
	if (add_len) {
		tx_buf[1] = words;
		header_len++;
	}
	memcpy(tx_buf + header_len, buf, len);
#ifdef __LITTLE_ENDIAN
	for (i = 0; i < (words + header_len); i++)
		tx_buf[i] = (tx_buf[i] << 8) | (tx_buf[i] >> 8);
#endif

	do {
		ret = spi_write(dd->spi, tx_buf,
				len + header_len * sizeof(u16));
	} while (ret == -EAGAIN);

	memset(dd->tx_buf, 0xFF, pdata->tx_buf_size);
	return ret;
}

/* ======================================================================== */

static int
spi_read_1(struct dev_data *dd, u16 address, u8 *buf, u16 len)
{
	return spi_read_123(dd, address, buf, len, true);
}

static int
spi_write_1(struct dev_data *dd, u16 address, u8 *buf, u16 len)
{
	return spi_write_123(dd, address, buf, len, true);
}

/* ======================================================================== */

static inline int
stop_legacy_acceleration(struct dev_data *dd)
{
	u16  value = 0xDEAD, status, i;
	int  ret;

	ret = spi_write_123(dd, 0x0003, (u8 *)&value,
				sizeof(value), false);
	if (ret < 0)
		return -1;
	usleep_range(100, 120);

	for (i = 0; i < 200; i++) {
		ret = spi_read_123(dd, 0x0003, (u8 *)&status, sizeof(status),
				   false);
		if (ret < 0)
			return -1;
		if (status == 0xABCD)
			return 0;
	}

	return -2;
}

static inline int
start_legacy_acceleration(struct dev_data *dd)
{
	u16  value = 0xBEEF;
	int  ret;

	ret = spi_write_123(dd, 0x0003, (u8 *)&value, sizeof(value), false);
	usleep_range(100, 120);

	return ret;
}

static inline int
spi_rw_2_poll_status(struct dev_data *dd)
{
	u16  status, i;
	int  ret;

	for (i = 0; i < 200; i++) {
		ret = spi_read_123(dd, 0x0000, (u8 *)&status, sizeof(status),
				   false);
		if (ret < 0)
			return -1;
		if (status == 0xABCD)
			return 0;
	}

	return -2;
}

static inline int
spi_read_2_page(struct dev_data *dd, u16 address, u8 *buf, u16 len)
{
	u16  request[] = {0xFEDC, (address << 1) | 0x0001, len / sizeof(u16)};
	int  ret;

	/* write read request header */
	ret = spi_write_123(dd, 0x0000, (u8 *)request, sizeof(request),
			    false);
	if (ret < 0)
		return -1;

	/* poll status */
	ret = spi_rw_2_poll_status(dd);
	if (ret < 0)
		return ret;

	/* read data */
	ret = spi_read_123(dd, 0x0004, (u8 *)buf, len, false);
	return ret;
}

static inline int
spi_write_2_page(struct dev_data *dd, u16 address, u8 *buf, u16 len)
{
	u16  page[254];
	int  ret;

	page[0] = 0xFEDC;
	page[1] = address << 1;
	page[2] = len / sizeof(u16);
	memcpy(page + 4, buf, len);

	/* write data with write request header */
	ret = spi_write_123(dd, 0x0000, (u8 *)page, len + 4 * sizeof(u16),
			    false);
	if (ret < 0)
		return -1;

	/* poll status */
	return spi_rw_2_poll_status(dd);
}

static inline int
spi_rw_2(struct dev_data *dd, u16 address, u8 *buf, u16 len,
	 int (*func)(struct dev_data *dd, u16 address, u8 *buf, u16 len))
{
	u16  rx_len, rx_limit = 250 * sizeof(u16), offset = 0;
	int  ret;

	while (len > 0) {
		rx_len = (len > rx_limit) ? rx_limit : len;
		if (dd->legacy_acceleration)
			stop_legacy_acceleration(dd);
		ret = func(dd, address + (offset / sizeof(u16)), buf + offset,
			   rx_len);
		if (dd->legacy_acceleration)
			start_legacy_acceleration(dd);
		if (ret < 0)
			return ret;
		offset += rx_len;
		len -= rx_len;
	}

	return 0;
}

static int
spi_read_2(struct dev_data *dd, u16 address, u8 *buf, u16 len)
{
	return spi_rw_2(dd, address, buf, len, spi_read_2_page);
}

static int
spi_write_2(struct dev_data *dd, u16 address, u8 *buf, u16 len)
{
	return spi_rw_2(dd, address, buf, len, spi_write_2_page);
}

/* ======================================================================== */

static int
spi_read_3(struct dev_data *dd, u16 address, u8 *buf, u16 len)
{
	return spi_read_123(dd, address, buf, len, false);
}

static int
spi_write_3(struct dev_data *dd, u16 address, u8 *buf, u16 len)
{
	return spi_write_123(dd, address, buf, len, false);
}

/* ======================================================================== */

static struct chip_access_method chip_access_methods[] = {
	{
		.read = spi_read_1,
		.write = spi_write_1,
	},
	{
		.read = spi_read_2,
		.write = spi_write_2,
	},
	{
		.read = spi_read_3,
		.write = spi_write_3,
	},
};

static int
set_chip_access_method(struct dev_data *dd, u8 method)
{
	if (method == 0 || method > ARRAY_SIZE(chip_access_methods))
		return -1;

	memcpy(&dd->chip, &chip_access_methods[method - 1], sizeof(dd->chip));
	return 0;
}

/* ======================================================================== */

static inline int
stop_legacy_acceleration_canned(struct dev_data *dd)
{
	u16  value = dd->irq_param[18];

	return dd->chip.write(dd, dd->irq_param[16], (u8 *)&value,
			      sizeof(value));
}

static inline int
start_legacy_acceleration_canned(struct dev_data *dd)
{
	u16  value = dd->irq_param[17];

	return dd->chip.write(dd, dd->irq_param[16], (u8 *)&value,
			      sizeof(value));
}

/* ======================================================================== */

#define FLASH_BLOCK_SIZE  64      /* flash write buffer in words */
#define FIRMWARE_SIZE     0xC000  /* fixed 48Kbytes */

static int bootloader_wait_ready(struct dev_data *dd)
{
	u16  status, i;

	for (i = 0; i < 15; i++) {
		if (spi_read_3(dd, 0x00FF, (u8 *)&status,
			       sizeof(status)) != 0)
			return -1;
		if (status == 0xABCC)
			return 0;
		if (i >= 3)
			usleep_range(500, 700);
	}
	ERROR("unexpected status %04X", status);
	return -1;
}

static int bootloader_complete(struct dev_data *dd)
{
	u16  value = 0x5432;

	return spi_write_3(dd, 0x00FF, (u8 *)&value, sizeof(value));
}

static int bootloader_read_data(struct dev_data *dd, u16 *value)
{
	u16  buffer[2];

	if (spi_read_3(dd, 0x00FE, (u8 *)buffer, sizeof(buffer)) != 0)
		return -1;
	if (buffer[1] != 0xABCC)
		return -1;

	*value = buffer[0];
	return bootloader_complete(dd);
}

static int bootloader_write_data(struct dev_data *dd, u16 value)
{
	u16  buffer[2] = {value, 0x5432};

	if (bootloader_wait_ready(dd) != 0)
		return -1;
	return spi_write_3(dd, 0x00FE, (u8 *)buffer, sizeof(buffer));
}

static int bootloader_wait_command(struct dev_data *dd)
{
	u16  value, i;

	for (i = 0; i < 15; i++) {
		if (bootloader_read_data(dd, &value) == 0 && value == 0x003E)
			return 0;
		if (i >= 3)
			usleep_range(500, 700);
	}
	return -1;
}

static int bootloader_enter(struct dev_data *dd)
{
	int i;
	u16 enter[3] = {0x0047, 0x00C7, 0x0007};

	for (i = 0; i < 3; i++) {
		if (spi_write_3(dd, 0x7F00, (u8 *)&enter[i],
				sizeof(enter[i])) != 0)
			return -1;
	}

	if (bootloader_wait_command(dd) != 0)
		return -1;
	return 0;
}

static int bootloader_exit(struct dev_data *dd)
{
	u16  value = 0x0000;

	if (bootloader_write_data(dd, 0x0001) != 0)
		return -1;
	return spi_write_3(dd, 0x7F00, (u8 *)&value, sizeof(value));
}

static int bootloader_get_crc(struct dev_data *dd, u16 *crc16, u16 len)
{
	u16 command[] = {0x0030, 0x0002, 0x0000, 0x0000, len & 0xFF,
			len >> 8}, value[2], i;

	for (i = 0; i < ARRAY_SIZE(command); i++)
		if (bootloader_write_data(dd, command[i]) != 0)
			return -1;
	msleep(200); /* wait 200ms for it to get done */

	for (i = 0; i < 2; i++)
		if (bootloader_read_data(dd, &value[i]) != 0)
			return -1;

	if (bootloader_wait_command(dd) != 0)
		return -1;
	*crc16 = (value[1] << 8) | value[0];
	return 0;
}

static int bootloader_set_byte_mode(struct dev_data *dd)
{
	u16  command[2] = {0x000A, 0x0000}, i;

	for (i = 0; i < ARRAY_SIZE(command); i++)
		if (bootloader_write_data(dd, command[i]) != 0)
			return -1;
	if (bootloader_wait_command(dd) != 0)
		return -1;
	return 0;
}

static int bootloader_erase_flash(struct dev_data *dd)
{
	if (bootloader_write_data(dd, 0x0002) != 0)
		return -1;
	msleep(60); /* wait 60ms */
	if (bootloader_wait_command(dd) != 0)
		return -1;
	return 0;
}

static int bootloader_write_flash(struct dev_data *dd, u16 *image, u16 len)
{
	u16  command[] = {0x00F0, 0x0000, len >> 8, 0x0000, 0x0000};
	u16  i, buffer[FLASH_BLOCK_SIZE];

	for (i = 0; i < ARRAY_SIZE(command); i++)
		if (bootloader_write_data(dd, command[i]) != 0)
			return -1;

	for (i = 0; i < ((len / sizeof(u16)) / FLASH_BLOCK_SIZE); i++) {
		if (bootloader_wait_ready(dd) != 0)
			return -1;
		memcpy(buffer, (void *)(image + i * FLASH_BLOCK_SIZE),
			sizeof(buffer));
		if (spi_write_3(dd, ((i % 2) == 0) ? 0x0000 : 0x0040,
				(u8 *)buffer, sizeof(buffer)) != 0)
			return -1;
		if (bootloader_complete(dd) != 0)
			return -1;
	}

	usleep_range(10000, 11000);
	if (bootloader_wait_command(dd) != 0)
		return -1;
	return 0;
}

static int device_fw_load(struct dev_data *dd, const struct firmware *fw)
{
	u16  fw_crc16, chip_crc16;

	fw_crc16 = crc16(0, fw->data, fw->size);
	INFO("firmware size (%d) CRC16(0x%04X)", fw->size, fw_crc16);
	if (bootloader_enter(dd) != 0) {
		ERROR("failed to enter bootloader");
		return -1;
	}
	if (bootloader_get_crc(dd, &chip_crc16, fw->size) != 0) {
		ERROR("failed to get CRC16 from the chip");
		return -1;
	}
	INFO("chip CRC16(0x%04X)", chip_crc16);
	if (fw_crc16 != chip_crc16) {
		INFO("will reprogram chip");
		if (bootloader_erase_flash(dd) != 0) {
			ERROR("failed to erase chip flash");
			return -1;
		}
		INFO("flash erase OK");
		if (bootloader_set_byte_mode(dd) != 0) {
			ERROR("failed to set byte mode");
			return -1;
		}
		INFO("byte mode OK");
		if (bootloader_write_flash(dd, (u16 *)fw->data,
							fw->size) != 0) {
			ERROR("failed to write flash");
			return -1;
		}
		INFO("flash write OK");
		if (bootloader_get_crc(dd, &chip_crc16, fw->size) != 0) {
			ERROR("failed to get CRC16 from the chip");
			return -1;
		}
		if (fw_crc16 != chip_crc16) {
			ERROR("failed to verify programming! (0x%04X)",
			      chip_crc16);
			return -1;
		}
		INFO("chip programmed successfully, new chip CRC16(0x%04X)",
			chip_crc16);
	}
	if (bootloader_exit(dd) != 0) {
		ERROR("failed to exit bootloader");
		return -1;
	}
	return 0;
}

static int fw_request_load(struct dev_data *dd)
{
	const struct firmware *fw;
	int  ret;

	ret = request_firmware(&fw, "maxim_fp35.bin", &dd->spi->dev);
	if (ret || fw == NULL) {
		ERROR("firmware request failed (%d,%p)", ret, fw);
		return -1;
	}
	if (fw->size != FIRMWARE_SIZE) {
		release_firmware(fw);
		ERROR("incoming firmware is of wrong size (%04X)", fw->size);
		return -1;
	}
	ret = device_fw_load(dd, fw);
	if (ret != 0 && bootloader_exit(dd) != 0)
		ERROR("failed to exit bootloader");
	release_firmware(fw);
	return ret;
}

/* ======================================================================== */

static void stop_idle_scan(struct dev_data *dd)
{
	u16 value;

	value = dd->irq_param[13];
	(void)dd->chip.write(dd, dd->irq_param[19],
				(u8 *)&value, sizeof(value));
}

static void stop_scan_canned(struct dev_data *dd)
{
	u16  value, i;

	if (dd->legacy_acceleration)
		(void)stop_legacy_acceleration_canned(dd);
	value = dd->irq_param[13];
	(void)dd->chip.write(dd, dd->irq_param[12], (u8 *)&value,
			     sizeof(value));
	usleep_range(dd->irq_param[15], dd->irq_param[15] + 1000);
	(void)dd->chip.read(dd, dd->irq_param[0], (u8 *)&value,
			     sizeof(value));
	(void)dd->chip.write(dd, dd->irq_param[0], (u8 *)&value,
			     sizeof(value));
	if (!dd->compatibility_mode) {
		for (i = 28; i < 32; i++) {
			value = dd->irq_param[i];
			(void)dd->chip.write(dd, dd->irq_param[27],
					(u8 *)&value, sizeof(value));
		}
		value = dd->irq_param[33];
		(void)dd->chip.write(dd, dd->irq_param[32],
					(u8 *)&value, sizeof(value));
		usleep_range(500, 1000);
		for (i = 28; i < 32; i++) {
			value = dd->irq_param[i];
			(void)dd->chip.write(dd, dd->irq_param[27],
					(u8 *)&value, sizeof(value));
		}
		value = dd->irq_param[13];
		(void)dd->chip.write(dd, dd->irq_param[32],
					(u8 *)&value, sizeof(value));
	}
}

#if !SUSPEND_POWER_OFF
static void start_scan_canned(struct dev_data *dd)
{
	u16  value;

	if (dd->legacy_acceleration) {
		(void)start_legacy_acceleration_canned(dd);
	} else {
		value = dd->irq_param[14];
		(void)dd->chip.write(dd, dd->irq_param[12], (u8 *)&value,
				     sizeof(value));
	}
}
#endif

static void enable_gesture(struct dev_data *dd)
{
	u16  value;

	if (!dd->idle)
		stop_scan_canned(dd);

	if (!dd->compatibility_mode) {
		value = dd->irq_param[13];
		(void)dd->chip.write(dd, dd->irq_param[34],
				     (u8 *)&value, sizeof(value));
		value = dd->irq_param[36];
		(void)dd->chip.write(dd, dd->irq_param[35],
				     (u8 *)&value, sizeof(value));
	}

	value = dd->irq_param[18];
	(void)dd->chip.write(dd, dd->irq_param[16], (u8 *)&value,
			     sizeof(value));

	value = dd->irq_param[20];
	(void)dd->chip.write(dd, dd->irq_param[19], (u8 *)&value,
			     sizeof(value));
	if (!dd->idle) {
		value = dd->irq_param[26];
		(void)dd->chip.write(dd, dd->irq_param[25], (u8 *)&value,
				     sizeof(value));
	}
}

static void finish_gesture(struct dev_data *dd)
{
	struct maxim_sti_pdata *pdata = dd->spi->dev.platform_data;
	u16 value;
	u8  fail_count = 0;
	int ret;

	value = dd->irq_param[21];
	(void)dd->chip.write(dd, dd->irq_param[19], (u8 *)&value,
			     sizeof(value));
	do {
		msleep(20);
		ret = dd->chip.read(dd, dd->irq_param[25], (u8 *)&value,
				    sizeof(value));
		if (ret < 0 || fail_count >= 20) {
			ERROR("failed to read control register (%d)", ret);
			pdata->reset(pdata, 0);
			msleep(20);
			pdata->reset(pdata, 1);
			return;
		}
		INFO("ctrl = 0x%04X", value);
		fail_count++;
	} while (value);
}

static int regulator_control(struct dev_data *dd, bool on)
{
	int ret;

	if (!dd->reg_avdd || !dd->reg_dvdd)
		return 0;

	if (on) {
		ret = regulator_enable(dd->reg_dvdd);
		if (ret < 0) {
			ERROR("Failed to enable regulator dvdd: %d", ret);
			return ret;
		}
		usleep_range(1000, 1020);

		ret = regulator_enable(dd->reg_avdd);
		if (ret < 0) {
			ERROR("Failed to enable regulator avdd: %d", ret);
			regulator_disable(dd->reg_dvdd);
			return ret;
		}
	} else {
		ret = regulator_disable(dd->reg_avdd);
		if (ret < 0) {
			ERROR("Failed to disable regulator avdd: %d", ret);
			return ret;
		}

		ret = regulator_disable(dd->reg_dvdd);
		if (ret < 0) {
			ERROR("Failed to disable regulator dvdd: %d", ret);
			regulator_enable(dd->reg_avdd);
			return ret;
		}
	}

	return 0;
}

static void regulator_init(struct dev_data *dd)
{
	dd->reg_avdd = devm_regulator_get(&dd->spi->dev, "avdd");
	if (IS_ERR(dd->reg_avdd))
		goto err_null_regulator;

	dd->reg_dvdd = devm_regulator_get(&dd->spi->dev, "dvdd");
	if (IS_ERR(dd->reg_dvdd))
		goto err_null_regulator;

	return;

err_null_regulator:
	dd->reg_avdd = NULL;
	dd->reg_dvdd = NULL;
	dev_warn(&dd->spi->dev, "Failed to init regulators\n");
}

/****************************************************************************\
* Suspend/resume processing                                                  *
\****************************************************************************/

#ifdef CONFIG_PM_SLEEP
static int early_suspend(struct device *dev)
{
	struct dev_data  *dd = spi_get_drvdata(to_spi_device(dev));
#if SUSPEND_POWER_OFF
	struct maxim_sti_pdata *pdata = dev->platform_data;
	int ret;
#endif

	if (dd->suspended)
		return 0;

	if (dd->suspend_in_progress)
		return -1;

	dd->suspend_in_progress = true;
	wake_up_process(dd->thread);
	wait_for_completion(&dd->suspend_resume);

#if SUSPEND_POWER_OFF
	/* reset-low and power-down */
	pdata->reset(pdata, 0);
	usleep_range(100, 120);
	ret = regulator_control(dd, false);
	if (ret < 0) {
		pdata->reset(pdata, 1);
		return ret;
	}
#endif

	return 0;
}

static int late_resume(struct device *dev)
{
	struct dev_data  *dd = spi_get_drvdata(to_spi_device(dev));
#if SUSPEND_POWER_OFF
	struct maxim_sti_pdata *pdata = dev->platform_data;
	int ret;
#endif

	if (!dd->suspended)
		return 0;

#if SUSPEND_POWER_OFF
	/* power-up and reset-high */
	ret = regulator_control(dd, true);
	if (ret < 0)
		return ret;

	usleep_range(300, 400);
	pdata->reset(pdata, 1);
#endif

	dd->resume_in_progress = true;
	wake_up_process(dd->thread);
	wait_for_completion(&dd->suspend_resume);
	return 0;
}

static int suspend(struct device *dev)
{
	struct dev_data  *dd = spi_get_drvdata(to_spi_device(dev));

	if (dd->irq_registered) {
		disable_irq(dd->spi->irq);
		enable_irq_wake(dd->spi->irq);
	}

	return 0;
}

static int resume(struct device *dev)
{
	struct dev_data  *dd = spi_get_drvdata(to_spi_device(dev));

	if (dd->irq_registered) {
		disable_irq_wake(dd->spi->irq);
		enable_irq(dd->spi->irq);
	}

	return 0;
}

static const struct dev_pm_ops pm_ops = {
	.suspend = suspend,
	.resume = resume,
};

#if INPUT_ENABLE_DISABLE
static int input_disable(struct input_dev *dev)
{
	struct dev_data *dd = input_get_drvdata(dev);

	return early_suspend(&dd->spi->dev);
}

static int input_enable(struct input_dev *dev)
{
	struct dev_data *dd = input_get_drvdata(dev);

	return late_resume(&dd->spi->dev);
}
#endif
#endif

/****************************************************************************\
* Netlink processing                                                         *
\****************************************************************************/

static inline int
nl_msg_new(struct dev_data *dd, u8 dst)
{
	dd->outgoing_skb = alloc_skb(NL_BUF_SIZE, GFP_KERNEL);
	if (dd->outgoing_skb == NULL)
		return -ENOMEM;
	nl_msg_init(dd->outgoing_skb->data, dd->nl_family.id, dd->nl_seq++,
		    dst);
	if (dd->nl_seq == 0)
		dd->nl_seq++;
	return 0;
}

static int
nl_callback_noop(struct sk_buff *skb, struct genl_info *info)
{
	return 0;
}

static inline bool
nl_process_driver_msg(struct dev_data *dd, u16 msg_id, u16 msg_len, void *msg)
{
	struct maxim_sti_pdata        *pdata = dd->spi->dev.platform_data;
	struct dr_add_mc_group        *add_mc_group_msg;
	struct dr_echo_request        *echo_msg;
	struct fu_echo_response       *echo_response;
	struct dr_chip_read           *read_msg;
	struct fu_chip_read_result    *read_result;
	struct dr_chip_write          *write_msg;
	struct dr_chip_access_method  *chip_access_method_msg;
	struct dr_delay               *delay_msg;
	struct fu_irqline_status      *irqline_status;
	struct dr_config_irq          *config_irq_msg;
	struct dr_config_input        *config_input_msg;
	struct dr_config_watchdog     *config_watchdog_msg;
	struct dr_input               *input_msg;
	struct dr_legacy_acceleration *legacy_acceleration_msg;
	struct dr_handshake           *handshake_msg;
	struct fu_handshake_response  *handshake_response;
	struct dr_config_fw           *config_fw_msg;
	struct dr_sysfs_ack           *sysfs_ack_msg;
	struct dr_idle                *idle_msg;
	struct dr_tf_status           *tf_status_msg;
	u8                            i, inp;
	u8                            irq_edge;
	int                           ret;

	if (dd->expect_resume_ack && msg_id != DR_DECONFIG &&
	    msg_id != DR_RESUME_ACK)
		return false;

	switch (msg_id) {
	case DR_ADD_MC_GROUP:
		add_mc_group_msg = msg;
		if (add_mc_group_msg->number >= pdata->nl_mc_groups) {
			ERROR("invalid multicast group number %d (%d)",
			      add_mc_group_msg->number, pdata->nl_mc_groups);
			return false;
		}
		if (dd->nl_mc_groups[add_mc_group_msg->number].id != 0)
			return false;
		dd->nl_ops[add_mc_group_msg->number].cmd =
						add_mc_group_msg->number;
		dd->nl_ops[add_mc_group_msg->number].doit = nl_callback_noop;
		ret = genl_register_ops(&dd->nl_family,
				&dd->nl_ops[add_mc_group_msg->number]);
		if (ret < 0)
			ERROR("failed to add multicast group op (%d)", ret);
		GENL_COPY(dd->nl_mc_groups[add_mc_group_msg->number].name,
			  add_mc_group_msg->name);
		ret = genl_register_mc_group(&dd->nl_family,
				&dd->nl_mc_groups[add_mc_group_msg->number]);
		if (ret < 0)
			ERROR("failed to add multicast group (%d)", ret);
		return false;
	case DR_ECHO_REQUEST:
		echo_msg = msg;
		echo_response = nl_alloc_attr(dd->outgoing_skb->data,
					      FU_ECHO_RESPONSE,
					      sizeof(*echo_response));
		if (echo_response == NULL)
			goto alloc_attr_failure;
		echo_response->cookie = echo_msg->cookie;
		return true;
	case DR_CHIP_READ:
		read_msg = msg;
		read_result = nl_alloc_attr(dd->outgoing_skb->data,
				FU_CHIP_READ_RESULT,
				sizeof(*read_result) + read_msg->length);
		if (read_result == NULL)
			goto alloc_attr_failure;
		read_result->address = read_msg->address;
		read_result->length = read_msg->length;
		ret = dd->chip.read(dd, read_msg->address, read_result->data,
				    read_msg->length);
		if (ret < 0)
			ERROR("failed to read from chip (%d)", ret);
		return true;
	case DR_CHIP_WRITE:
		write_msg = msg;
#if HI02_WORKAROUND
		if (write_msg->address == dd->irq_param[12] && write_msg->data[0] == dd->irq_param[13]) {
				ret = dd->chip.write(dd, write_msg->address, write_msg->data,
					     write_msg->length);
			if (ret < 0)
				ERROR("failed to write chip (%d)", ret);
			msleep(15);
			ret = dd->chip.read(dd, dd->irq_param[0], (u8 *)read_value, sizeof(read_value));
			if (ret < 0)
				ERROR("failed to read from chip (%d)", ret);
			ret = dd->chip.write(dd, dd->irq_param[0], (u8 *)read_value, sizeof(read_value));
			return false;
		}
		if (write_msg->address == dd->irq_param[0]) {
			/* assume nothing follows TSISTAT, of course */
			read_value[0] = ((u16 *)write_msg->data)[0];
			ret = dd->chip.write(dd, write_msg->address, (u8 *)read_value, sizeof(read_value));
			return false;
		}
#endif
		ret = dd->chip.write(dd, write_msg->address, write_msg->data,
				     write_msg->length);
		if (ret < 0)
			ERROR("failed to write chip (%d)", ret);
		return false;
	case DR_CHIP_RESET:
		pdata->reset(pdata, ((struct dr_chip_reset *)msg)->state);
		return false;
	case DR_GET_IRQLINE:
		irqline_status = nl_alloc_attr(dd->outgoing_skb->data,
					       FU_IRQLINE_STATUS,
					       sizeof(*irqline_status));
		if (irqline_status == NULL)
			goto alloc_attr_failure;
		irqline_status->status = pdata->irq(pdata);
		return true;
	case DR_DELAY:
		delay_msg = msg;
		if (delay_msg->period > 1000)
			msleep(delay_msg->period / 1000);
		usleep_range(delay_msg->period % 1000,
			    (delay_msg->period % 1000) + 10);
		return false;
	case DR_CHIP_ACCESS_METHOD:
		chip_access_method_msg = msg;
		ret = set_chip_access_method(dd,
					     chip_access_method_msg->method);
		if (ret < 0)
			ERROR("failed to set chip access method (%d) (%d)",
			      ret, chip_access_method_msg->method);
		return false;
	case DR_CONFIG_IRQ:
		config_irq_msg = msg;
		if (config_irq_msg->irq_params > MAX_IRQ_PARAMS) {
			ERROR("too many IRQ parameters");
			return false;
		}
		dd->nirq_params = config_irq_msg->irq_params;
		dd->compatibility_mode = dd->nirq_params <= OLD_NIRQ_PARAMS;
		irq_edge = *((u8 *)msg + msg_len - 1);
		memcpy(dd->irq_param, config_irq_msg->irq_param,
		       config_irq_msg->irq_params * sizeof(dd->irq_param[0]));
		if (dd->irq_registered)
			return false;
		dd->service_irq = service_irq;
		ret = request_irq(dd->spi->irq, irq_handler,
			(irq_edge == DR_IRQ_RISING_EDGE) ?
				IRQF_TRIGGER_RISING : IRQF_TRIGGER_FALLING,
						pdata->nl_family, dd);
		if (ret < 0) {
			ERROR("failed to request IRQ (%d)", ret);
		} else {
			dd->irq_registered = true;
			wake_up_process(dd->thread);
		}
		return false;
	case DR_CONFIG_INPUT:
		config_input_msg = msg;
		for (i = 0; i < INPUT_DEVICES; i++)
			if (dd->input_dev[i] != NULL)
				return false;
		for (i = 0; i < INPUT_DEVICES; i++) {
			dd->input_dev[i] = input_allocate_device();
			if (dd->input_dev[i] == NULL) {
				ERROR("failed to allocate input device");
				continue;
			}
			snprintf(dd->input_phys, sizeof(dd->input_phys),
				 "%s/input%d", dev_name(&dd->spi->dev), i);
			dd->input_dev[i]->name = pdata->nl_family;
			dd->input_dev[i]->phys = dd->input_phys;
			dd->input_dev[i]->id.bustype = BUS_SPI;
#if defined(CONFIG_PM_SLEEP) && INPUT_ENABLE_DISABLE
			if (i == 0) {
				dd->input_dev[i]->enable = input_enable;
				dd->input_dev[i]->disable = input_disable;
				dd->input_dev[i]->enabled = true;
				input_set_drvdata(dd->input_dev[i], dd);
			}
#endif
#if NV_ENABLE_CPU_BOOST
			if (i == 0)
				input_set_capability(dd->input_dev[i], EV_MSC,
						     MSC_ACTIVITY);
#endif
			__set_bit(EV_SYN, dd->input_dev[i]->evbit);
			__set_bit(EV_ABS, dd->input_dev[i]->evbit);
			if (i == (INPUT_DEVICES - 1)) {
				__set_bit(EV_KEY, dd->input_dev[i]->evbit);
				__set_bit(BTN_TOOL_RUBBER,
					  dd->input_dev[i]->keybit);
				__set_bit(KEY_POWER,
					  dd->input_dev[i]->keybit);
			}
			input_set_abs_params(dd->input_dev[i],
					     ABS_MT_POSITION_X, 0,
					     config_input_msg->x_range, 0, 0);
			input_set_abs_params(dd->input_dev[i],
					     ABS_MT_POSITION_Y, 0,
					     config_input_msg->y_range, 0, 0);
			input_set_abs_params(dd->input_dev[i],
					     ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
			input_set_abs_params(dd->input_dev[i],
					     ABS_MT_TRACKING_ID, 0,
					     MAX_INPUT_EVENTS, 0, 0);
			if (i == (INPUT_DEVICES - 1))
				input_set_abs_params(dd->input_dev[i],
						     ABS_MT_TOOL_TYPE, 0,
						     MT_TOOL_MAX, 0, 0);
			else
				input_set_abs_params(dd->input_dev[i],
						     ABS_MT_TOOL_TYPE, 0,
						     MT_TOOL_FINGER, 0, 0);

			ret = input_register_device(dd->input_dev[i]);
			if (ret < 0) {
				input_free_device(dd->input_dev[i]);
				dd->input_dev[i] = NULL;
				ERROR("failed to register input device");
			}
		}
		/* create symlink */
		if (dd->parent == NULL) {
			dd->parent = dd->input_dev[0]->dev.kobj.parent;
			ret = sysfs_create_link(dd->parent, &dd->spi->dev.kobj,
								MAXIM_STI_NAME);
			if (ret)
				ERROR("sysfs_create_link error\n");
		}
		return false;
	case DR_CONFIG_WATCHDOG:
		config_watchdog_msg = msg;
		dd->fusion_process = (pid_t)config_watchdog_msg->pid;
		return false;
	case DR_DECONFIG:
		if (dd->irq_registered) {
			free_irq(dd->spi->irq, dd);
			dd->irq_registered = false;
		}
		stop_scan_canned(dd);
		if (!dd->input_no_deconfig) {
			if (dd->parent != NULL) {
				sysfs_remove_link(
					dd->input_dev[0]->dev.kobj.parent,
					MAXIM_STI_NAME);
				dd->parent = NULL;
			}
			for (i = 0; i < INPUT_DEVICES; i++) {
				if (dd->input_dev[i] == NULL)
					continue;
				input_unregister_device(dd->input_dev[i]);
				dd->input_dev[i] = NULL;
			}
		}
		dd->expect_resume_ack = false;
		dd->eraser_active = false;
		dd->legacy_acceleration = false;
		dd->service_irq = service_irq;
		dd->fusion_process = (pid_t)0;
		dd->sysfs_update_type = DR_SYSFS_UPDATE_NONE;
		return false;
	case DR_INPUT:
		input_msg = msg;
		if (input_msg->events == 0) {
			if (dd->eraser_active) {
				input_report_key(
					dd->input_dev[INPUT_DEVICES - 1],
					BTN_TOOL_RUBBER, 0);
				dd->eraser_active = false;
			}
			for (i = 0; i < INPUT_DEVICES; i++) {
				input_mt_sync(dd->input_dev[i]);
				input_sync(dd->input_dev[i]);
			}
		} else {
			for (i = 0; i < input_msg->events; i++) {
				switch (input_msg->event[i].tool_type) {
				case DR_INPUT_FINGER:
					inp = 0;
					input_report_abs(dd->input_dev[inp],
							 ABS_MT_TOOL_TYPE,
							 MT_TOOL_FINGER);
					break;
				case DR_INPUT_STYLUS:
					inp = INPUT_DEVICES - 1;
					input_report_abs(dd->input_dev[inp],
							 ABS_MT_TOOL_TYPE,
							 MT_TOOL_PEN);
					break;
				case DR_INPUT_ERASER:
					inp = INPUT_DEVICES - 1;
					input_report_key(dd->input_dev[inp],
						BTN_TOOL_RUBBER, 1);
					dd->eraser_active = true;
					break;
				default:
					inp = 0;
					ERROR("invalid input tool type (%d)",
					      input_msg->event[i].tool_type);
					break;
				}
				input_report_abs(dd->input_dev[inp],
						 ABS_MT_TRACKING_ID,
						 input_msg->event[i].id);
				input_report_abs(dd->input_dev[inp],
						 ABS_MT_POSITION_X,
						 input_msg->event[i].x);
				input_report_abs(dd->input_dev[inp],
						 ABS_MT_POSITION_Y,
						 input_msg->event[i].y);
				input_report_abs(dd->input_dev[inp],
						 ABS_MT_PRESSURE,
						 input_msg->event[i].z);
				input_mt_sync(dd->input_dev[inp]);
			}
			for (i = 0; i < INPUT_DEVICES; i++)
				input_sync(dd->input_dev[i]);
		}
		return false;
	case DR_RESUME_ACK:
		dd->expect_resume_ack = false;
		if (dd->irq_registered && !dd->gesture_en)
			enable_irq(dd->spi->irq);
		return false;
	case DR_LEGACY_FWDL:
		ret = fw_request_load(dd);
		if (ret < 0)
			ERROR("firmware download failed (%d)", ret);
		else
			INFO("firmware download OK");
		return false;
	case DR_LEGACY_ACCELERATION:
		legacy_acceleration_msg = msg;
		if (legacy_acceleration_msg->enable) {
			dd->service_irq = service_irq_legacy_acceleration;
			start_legacy_acceleration(dd);
			dd->legacy_acceleration = true;
		} else {
			stop_legacy_acceleration(dd);
			dd->legacy_acceleration = false;
			dd->service_irq = service_irq;
		}
		return false;
	case DR_HANDSHAKE:
		handshake_msg = msg;
		dd->tf_ver = handshake_msg->tf_ver;
		dd->chip_id = handshake_msg->chip_id;
		dd->drv_ver = DRIVER_VERSION_NUM;
		handshake_response = nl_alloc_attr(dd->outgoing_skb->data,
						FU_HANDSHAKE_RESPONSE,
						sizeof(*handshake_response));
		if (handshake_response == NULL)
			goto alloc_attr_failure;
		handshake_response->driver_ver = dd->drv_ver;
		handshake_response->panel_id = panel_id;
		handshake_response->driver_protocol = DRIVER_PROTOCOL;
		return true;
	case DR_CONFIG_FW:
		config_fw_msg = msg;
		dd->fw_ver = config_fw_msg->fw_ver;
		dd->fw_protocol = config_fw_msg->fw_protocol;
		return false;
	case DR_SYSFS_ACK:
		sysfs_ack_msg = msg;
		if (sysfs_ack_msg->type == DR_SYSFS_ACK_GLOVE)
			complete(&dd->sysfs_ack_glove);
		if (sysfs_ack_msg->type == DR_SYSFS_ACK_CHARGER)
			complete(&dd->sysfs_ack_charger);
		if (sysfs_ack_msg->type == DR_SYSFS_ACK_LCD_FPS)
			complete(&dd->sysfs_ack_lcd_fps);
		return false;
	case DR_IDLE:
		idle_msg = msg;
		dd->idle = !!(idle_msg->idle);
		return false;
	case DR_TF_STATUS:
		tf_status_msg = msg;
		dd->tf_status = tf_status_msg->tf_status;
		return false;
	default:
		ERROR("unexpected message %d", msg_id);
		return false;
	}

alloc_attr_failure:
	ERROR("failed to allocate response for msg_id %d", msg_id);
	return false;
}

static int nl_process_msg(struct dev_data *dd, struct sk_buff *skb)
{
	struct nlattr  *attr;
	bool           send_reply = false;
	int            ret = 0, ret2;

	/* process incoming message */
	attr = NL_ATTR_FIRST(skb->data);
	for (; attr < NL_ATTR_LAST(skb->data); attr = NL_ATTR_NEXT(attr)) {
		if (nl_process_driver_msg(dd, attr->nla_type,
					  (attr->nla_len - NLA_HDRLEN),
					  NL_ATTR_VAL(attr, void)))
			send_reply = true;
	}

	/* send back reply if requested */
	if (send_reply) {
		(void)skb_put(dd->outgoing_skb,
			      NL_SIZE(dd->outgoing_skb->data));
		if (NL_SEQ(skb->data) == 0)
			ret = genlmsg_unicast(sock_net(skb->sk),
					      dd->outgoing_skb,
					      NETLINK_CB(skb).pid);
		else
			ret = genlmsg_multicast(dd->outgoing_skb, 0,
					dd->nl_mc_groups[MC_FUSION].id,
					GFP_KERNEL);
		if (ret < 0)
			ERROR("could not reply to fusion (%d)", ret);

		/* allocate new outgoing skb */
		ret2 = nl_msg_new(dd, MC_FUSION);
		if (ret2 < 0)
			ERROR("could not allocate outgoing skb (%d)", ret2);
	}

	/* free incoming message */
	kfree_skb(skb);
	return ret;
}

static int
nl_callback_driver(struct sk_buff *skb, struct genl_info *info)
{
	struct dev_data  *dd;
	struct sk_buff   *skb2;
	unsigned long    flags;

	/* locate device structure */
	spin_lock_irqsave(&dev_lock, flags);
	list_for_each_entry(dd, &dev_list, dev_list)
		if (dd->nl_family.id == NL_TYPE(skb->data))
			break;
	spin_unlock_irqrestore(&dev_lock, flags);
	if (&dd->dev_list == &dev_list)
		return -ENODEV;
	if (!dd->nl_enabled)
		return -EAGAIN;

	/* queue incoming skb and wake up processing thread */
	skb2 = skb_clone(skb, GFP_ATOMIC);
	if (skb2 == NULL) {
		ERROR("failed to clone incoming skb");
		return -ENOMEM;
	} else {
		skb_queue_tail(&dd->incoming_skb_queue, skb2);
		wake_up_process(dd->thread);
		return 0;
	}
}

static int
nl_callback_fusion(struct sk_buff *skb, struct genl_info *info)
{
	struct dev_data  *dd;
	unsigned long    flags;

	/* locate device structure */
	spin_lock_irqsave(&dev_lock, flags);
	list_for_each_entry(dd, &dev_list, dev_list)
		if (dd->nl_family.id == NL_TYPE(skb->data))
			break;
	spin_unlock_irqrestore(&dev_lock, flags);
	if (&dd->dev_list == &dev_list)
		return -ENODEV;
	if (!dd->nl_enabled)
		return -EAGAIN;

	(void)genlmsg_multicast(skb_clone(skb, GFP_ATOMIC), 0,
				dd->nl_mc_groups[MC_FUSION].id, GFP_ATOMIC);
	return 0;
}

/****************************************************************************\
* Interrupt processing                                                       *
\****************************************************************************/

static irqreturn_t irq_handler(int irq, void *context)
{
	struct dev_data  *dd = context;

#if NV_ENABLE_CPU_BOOST
	if (time_after(jiffies, dd->last_irq_jiffies + INPUT_IDLE_PERIOD))
		input_event(dd->input_dev[0], EV_MSC, MSC_ACTIVITY, 1);
	dd->last_irq_jiffies = jiffies;
#endif

	wake_up_process(dd->thread);
	return IRQ_HANDLED;
}

static void gesture_wake_detect(struct dev_data *dd)
{
	u16    status, value;
	int    ret = 0;

	ret = dd->chip.read(dd, dd->irq_param[0],
			    (u8 *)&status, sizeof(status));
	INFO("%s: tsistat = %04X", __func__, status);
	if (status & dd->irq_param[10])
		dd->resume_reset = true;
	if ((status & dd->irq_param[18]) && dd->input_dev[0]) {
		ret = dd->chip.read(dd, dd->irq_param[22],
				    (u8 *)&value, sizeof(value));
		INFO("gesture code = 0x%04X", value);
		if (value == dd->irq_param[24]) {
			INFO("gesture detected");
			input_report_key(dd->input_dev[INPUT_DEVICES - 1], KEY_POWER, 1);
			input_sync(dd->input_dev[INPUT_DEVICES - 1]);
			input_report_key(dd->input_dev[INPUT_DEVICES - 1], KEY_POWER, 0);
			input_sync(dd->input_dev[INPUT_DEVICES - 1]);
		}
		value = dd->irq_param[18];
		ret = dd->chip.write(dd, dd->irq_param[0],
				     (u8 *)&value, sizeof(value));
	} else {
		ret = dd->chip.write(dd, dd->irq_param[0],
				     (u8 *)&status, sizeof(status));
	}
}

static void service_irq_legacy_acceleration(struct dev_data *dd)
{
	struct fu_async_data  *async_data;
	u16                   len, rx_len = 0, offset = 0;
	u16                   buf[255], rx_limit = 250 * sizeof(u16);
	int                   ret = 0, counter = 0;

	async_data = nl_alloc_attr(dd->outgoing_skb->data, FU_ASYNC_DATA,
				   sizeof(*async_data) + dd->irq_param[4] +
				   2 * sizeof(u16));
	if (async_data == NULL) {
		ERROR("can't add data to async IRQ buffer");
		return;
	}
	async_data->length = dd->irq_param[4] + 2 * sizeof(u16);
	len = async_data->length;
	async_data->address = 0;

	while (len > 0) {
		rx_len = (len > rx_limit) ? rx_limit : len;
		ret = spi_read_123(dd, 0x0000, (u8 *)&buf,
					rx_len + 4 * sizeof(u16), false);
		if (ret < 0)
			break;

		if (buf[3] == 0xBABE) {
			dd->legacy_acceleration = false;
			dd->service_irq = service_irq;
			nl_msg_init(dd->outgoing_skb->data, dd->nl_family.id,
				    dd->nl_seq - 1, MC_FUSION);
			return;
		}

		if (rx_limit == rx_len)
			usleep_range(200, 300);

		if (buf[0] == 0x6060) {
			ERROR("data not ready");
			start_legacy_acceleration_canned(dd);
			ret = -EBUSY;
			break;
		} else if (buf[0] == 0x8070) {
			if (buf[1] == dd->irq_param[1] ||
					buf[1] == dd->irq_param[2])
				async_data->address = buf[1];

			if (async_data->address +
					offset / sizeof(u16) != buf[1]) {
				ERROR("sequence number incorrect %04X", buf[1]);
				start_legacy_acceleration_canned(dd);
				ret = -EBUSY;
				break;
			}
		}
		counter++;
		memcpy(async_data->data + offset, buf + 4, rx_len);
		offset += rx_len;
		len -= rx_len;
	}
	async_data->status = *(buf + rx_len / sizeof(u16) + 2);

	if (ret < 0) {
		ERROR("can't read IRQ buffer (%d)", ret);
		nl_msg_init(dd->outgoing_skb->data, dd->nl_family.id,
			    dd->nl_seq - 1, MC_FUSION);
	} else {
		(void)skb_put(dd->outgoing_skb,
			      NL_SIZE(dd->outgoing_skb->data));
		ret = genlmsg_multicast(dd->outgoing_skb, 0,
					dd->nl_mc_groups[MC_FUSION].id,
					GFP_KERNEL);
		if (ret < 0) {
			ERROR("can't send IRQ buffer %d", ret);
			msleep(300);
			if (++dd->send_fail_count >= 10 &&
			    dd->fusion_process != (pid_t)0) {
				(void)kill_pid(
					find_get_pid(dd->fusion_process),
					SIGKILL, 1);
				wake_up_process(dd->thread);
			}
		} else {
			dd->send_fail_count = 0;
		}
		ret = nl_msg_new(dd, MC_FUSION);
		if (ret < 0)
			ERROR("could not allocate outgoing skb (%d)", ret);
	}
}

static void service_irq(struct dev_data *dd)
{
	struct fu_async_data  *async_data;
	u16                   status, test, address[2], xbuf;
	u16                   clear[2] = { 0 };
	bool                  read_buf[2] = {true, false};
	int                   ret, ret2;
	u16                   value;
	//s16 finger_buf[36];
	//int i;
	//static u16 prev_finger_count;

	ret = dd->chip.read(dd, dd->irq_param[0], (u8 *)&status,
			    sizeof(status));
	if (ret < 0) {
		ERROR("can't read IRQ status (%d)", ret);
		return;
	}

	if (dd->resume_reset) {
		read_buf[0] = false;
		clear[0] = dd->irq_param[10];
		status = dd->irq_param[10];
		dd->resume_reset = false;
	} else if (status & dd->irq_param[10]) {
		read_buf[0] = false;
		clear[0] = dd->irq_param[10];
	} else if (status & dd->irq_param[18]) {
#if 1
		//int_counter = 0;
		test = status & (dd->irq_param[6] | dd->irq_param[7]);

		if (test == 0)
			return;
		else if (test == (dd->irq_param[6] | dd->irq_param[7]))
			xbuf = ((status & dd->irq_param[5]) == 0) ? 0 : 1;
		else if (test == dd->irq_param[6])
			xbuf = 0;
		else if (test == dd->irq_param[7])
			xbuf = 1;
		else {
			ERROR("unexpected IRQ handler case 0x%04X ",status);
			return;
		}

		read_buf[1] = true;
		address[1] = xbuf ? dd->irq_param[2] : dd->irq_param[1];
		address[0] = dd->irq_param[3];
		clear[0] = xbuf ? dd->irq_param[7] : dd->irq_param[6];
		clear[0] |= dd->irq_param[8];
		clear[0] |= dd->irq_param[18];

		value = dd->irq_param[17];
		(void)dd->chip.write(dd, dd->irq_param[16], (u8 *)&value,
				     sizeof(value));
#else
		//(void)dd->chip.read(dd, 0x1BDC, (u8 *)finger_buf, sizeof(finger_buf));
		(void)dd->chip.read(dd, 0x1410, (u8 *)finger_buf, sizeof(finger_buf));
		INFO("finger_buf[0] = 0x%04X", finger_buf[0]);
		if (finger_buf[0] == 0xFFFFF1FA) {
			INFO("old baseline score = %d, new baseline score = %d, max = %d, min = %d, above = %d, below = %d",
			     finger_buf[30], finger_buf[31], finger_buf[32], finger_buf[33], finger_buf[34], finger_buf[35]);
			finger_buf[0] = 0;
		}
		if (finger_buf[0] > 4)
			/* shouldn't ever happen */
			finger_buf[0] = 4;
		for (i = 0; i < finger_buf[0]; i++) {
			INFO("id = %d, x = %d, y = %d, z = %d",
			     finger_buf[i * 4 + 1 + 0],
			     800 - finger_buf[i * 4 + 1 + 2] * 800 / (36 * 128),
			     finger_buf[i * 4 + 1 + 1] * 1280 / (52 * 128),
			     finger_buf[i * 4 + 1 + 3] > 2040 ? 255 :
			     finger_buf[i * 4 + 1 + 3] >> 3);
			input_report_abs(dd->input_dev[0],
					 ABS_MT_TRACKING_ID,
					 finger_buf[i * 4 + 1 + 0]);
			input_report_abs(dd->input_dev[0],
					 ABS_MT_POSITION_X,
					 800 - finger_buf[i * 4 + 1 + 2] * 800 / (36 * 128));
			input_report_abs(dd->input_dev[0],
					 ABS_MT_POSITION_Y,
					 finger_buf[i * 4 + 1 + 1] * 1280 / (52 * 128));
			input_report_abs(dd->input_dev[0],
					 ABS_MT_PRESSURE,
					 finger_buf[i * 4 + 1 + 3] > 2040 ? 255 :
					 finger_buf[i * 4 + 1 + 3] >> 3);
			input_mt_sync(dd->input_dev[0]);
		}
		if (finger_buf[0]) {
			for (i = 0; i < 4; i++)
				INFO("peak = %d, pixelcount = %d, energy = %d",
				     finger_buf[i * 3 + 17 + 0],
				     finger_buf[i * 3 + 17 + 1],
				     finger_buf[i * 3 + 17 + 2]);
			printk("\n");
		}
		if (!finger_buf[0] && prev_finger_count)
			input_mt_sync(dd->input_dev[0]);
		if (finger_buf[0] || prev_finger_count)
			input_sync(dd->input_dev[0]);
		if (finger_buf[29]) {
			INFO("double-tap!!\n");
			input_report_key(dd->input_dev[INPUT_DEVICES - 1], KEY_POWER, 1);
			input_sync(dd->input_dev[INPUT_DEVICES - 1]);
			input_report_key(dd->input_dev[INPUT_DEVICES - 1], KEY_POWER, 0);
			input_sync(dd->input_dev[INPUT_DEVICES - 1]);
		}
		prev_finger_count = finger_buf[0];
		clear[0] = dd->irq_param[18];
		(void)dd->chip.write(dd, 0x6001, (u8 *)&clear,
				     sizeof(clear));
		return;
#endif

	} else if (status & dd->irq_param[9]) {
		test = status & (dd->irq_param[6] | dd->irq_param[7]);

		if (test == (dd->irq_param[6] | dd->irq_param[7]))
			xbuf = ((status & dd->irq_param[5]) != 0) ? 0 : 1;
		else if (test == dd->irq_param[6])
			xbuf = 0;
		else if (test == dd->irq_param[7])
			xbuf = 1;
		else {
			ERROR("unexpected IRQ handler case");
			return;
		}
		read_buf[1] = true;
		address[1] = xbuf ? dd->irq_param[2] : dd->irq_param[1];

		address[0] = dd->irq_param[3];
		clear[0] = dd->irq_param[6] | dd->irq_param[7] |
			dd->irq_param[8] | dd->irq_param[9];
		value = dd->irq_param[13];
		(void)dd->chip.write(dd, dd->irq_param[12], (u8 *)&value,
				     sizeof(value));


	} else {
		test = status & (dd->irq_param[6] | dd->irq_param[7]);

		if (test == 0)
			return;
		else if (test == (dd->irq_param[6] | dd->irq_param[7]))
			xbuf = ((status & dd->irq_param[5]) == 0) ? 0 : 1;
		else if (test == dd->irq_param[6])
			xbuf = 0;
		else if (test == dd->irq_param[7])
			xbuf = 1;
		else {
			ERROR("unexpected IRQ handler case 0x%04X ",status);
			return;
		}

		address[0] = xbuf ? dd->irq_param[2] : dd->irq_param[1];
		clear[0] = xbuf ? dd->irq_param[7] : dd->irq_param[6];
		clear[0] |= dd->irq_param[8];
	}

	//if (int_counter < 25)
	//	INFO("interrupt #%d, 0x%04X", int_counter, status);

	async_data = nl_alloc_attr(dd->outgoing_skb->data, FU_ASYNC_DATA,
				   sizeof(*async_data) + dd->irq_param[4]);
	if (async_data == NULL) {
		ERROR("can't add data to async IRQ buffer 1");
		return;
	}

	async_data->status = status;
	if (read_buf[0]) {
		async_data->address = address[0];
		async_data->length = dd->irq_param[4];
		ret = dd->chip.read(dd, address[0], async_data->data,
				    dd->irq_param[4]);
	}

	if (read_buf[1] && ret == 0) {
		async_data = nl_alloc_attr(dd->outgoing_skb->data,
					   FU_ASYNC_DATA,
					   sizeof(*async_data) +
						dd->irq_param[4]);
		if (async_data == NULL) {
			ERROR("can't add data to async IRQ buffer 2");
			nl_msg_init(dd->outgoing_skb->data, dd->nl_family.id,
				    dd->nl_seq - 1, MC_FUSION);
			return;
		}
		async_data->address = address[1];
		async_data->length = dd->irq_param[4];
		async_data->status = status;
		ret = dd->chip.read(dd, address[1], async_data->data,
				    dd->irq_param[4]);
	}

#if HI02_WORKAROUND
	ret2 = dd->chip.write(dd, dd->irq_param[0], (u8 *)clear,
			     sizeof(clear));
#else
	ret2 = dd->chip.write(dd, dd->irq_param[0], (u8 *)clear,
			     sizeof(clear[0]));
#endif
	if (ret2 < 0)
		ERROR("can't clear IRQ status (%d)", ret2);

	if (ret < 0) {
		ERROR("can't read IRQ buffer (%d)", ret);
		nl_msg_init(dd->outgoing_skb->data, dd->nl_family.id,
			    dd->nl_seq - 1, MC_FUSION);
	} else {
		//if (int_counter < 25)
		//	INFO("sending #%d, 0x%04X", int_counter++, status);
		(void)skb_put(dd->outgoing_skb,
			      NL_SIZE(dd->outgoing_skb->data));
		ret = genlmsg_multicast(dd->outgoing_skb, 0,
					dd->nl_mc_groups[MC_FUSION].id,
					GFP_KERNEL);
		if (ret < 0) {
			ERROR("can't send IRQ buffer %d", ret);
			msleep(300);
			if (read_buf[0] == false ||
			    (++dd->send_fail_count >= 10 &&
			     dd->fusion_process != (pid_t)0)) {
				(void)kill_pid(
					find_get_pid(dd->fusion_process),
					SIGKILL, 1);
				wake_up_process(dd->thread);
			}
		} else {
			dd->send_fail_count = 0;
		}
		ret = nl_msg_new(dd, MC_FUSION);
		if (ret < 0)
			ERROR("could not allocate outgoing skb (%d)", ret);
	}
}

static int send_sysfs_info(struct dev_data *dd)
{
	struct fu_sysfs_info       *sysfs_info;
	int                        ret, ret2;

	sysfs_info = nl_alloc_attr(dd->outgoing_skb->data,
					FU_SYSFS_INFO,
					sizeof(*sysfs_info));
	if (sysfs_info == NULL) {
		ERROR("Can't allocate sysfs_info");
		return -1;
	}
	sysfs_info->type = dd->sysfs_update_type;

	sysfs_info->glove_value = dd->glove_enabled;
	sysfs_info->charger_value = dd->charger_mode_en;
	sysfs_info->lcd_fps_value = dd->lcd_fps;

	(void)skb_put(dd->outgoing_skb,
			      NL_SIZE(dd->outgoing_skb->data));
	ret = genlmsg_multicast(dd->outgoing_skb, 0,
			dd->nl_mc_groups[MC_FUSION].id,
			GFP_KERNEL);
	if (ret < 0) {
		ERROR("could not reply to fusion (%d)", ret);
		return -1;
	}

	/* allocate new outgoing skb */
	ret2 = nl_msg_new(dd, MC_FUSION);
	if (ret2 < 0)
		ERROR("could not allocate outgoing skb (%d)", ret2);

	return 0;

}
/****************************************************************************\
* Processing thread                                                          *
\****************************************************************************/

static int processing_thread(void *arg)
{
	struct dev_data         *dd = arg;
	struct maxim_sti_pdata  *pdata = dd->spi->dev.platform_data;
	struct sk_buff          *skb;
	char                    *argv[] = { pdata->touch_fusion, "daemon",
					    pdata->nl_family,
					    pdata->config_file, NULL };
	int                     ret, ret2;
	bool                    fusion_dead;

	sched_setscheduler(current, SCHED_FIFO, &dd->thread_sched);

	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);

		/* ensure that we have outgoing skb */
		if (dd->outgoing_skb == NULL)
			if (nl_msg_new(dd, MC_FUSION) < 0) {
				schedule();
				continue;
			}

		/* priority 1: start up fusion process */
		if (dd->fusion_process != (pid_t)0 && get_pid_task(
					find_get_pid(dd->fusion_process),
					PIDTYPE_PID) == NULL) {
			stop_scan_canned(dd);
			dd->start_fusion = true;
			dd->fusion_process = (pid_t)0;
#if INPUT_ENABLE_DISABLE
			dd->input_no_deconfig = true;
#endif
		}
		if (dd->start_fusion) {
			do {
				ret = call_usermodehelper(argv[0], argv, NULL,
							  UMH_WAIT_EXEC);
				if (ret != 0)
					msleep(100);
			} while (ret != 0 && !kthread_should_stop());

			/* power-up and reset-high */
			ret = regulator_control(dd, true);
			if (ret < 0)
				ERROR("failed to enable regulators");

			usleep_range(300, 400);
			pdata->reset(pdata, 1);

			dd->start_fusion = false;
		}
		if (kthread_should_stop())
			break;

		/* priority 2: process pending Netlink messages */
		while ((skb = skb_dequeue(&dd->incoming_skb_queue)) != NULL) {
			if (kthread_should_stop())
				break;
			if (nl_process_msg(dd, skb) < 0)
				skb_queue_purge(&dd->incoming_skb_queue);
		}
		if (kthread_should_stop())
			break;

		/* priority 3: suspend/resume */
		if (dd->suspend_in_progress && !(dd->tf_status & TF_STATUS_BUSY)) {
			if (dd->irq_registered) {
				disable_irq(dd->spi->irq);
				if (dd->gesture_en) {
					enable_gesture(dd);
					enable_irq(dd->spi->irq);
				} else if (dd->compatibility_mode || !dd->idle)
					stop_scan_canned(dd);
				else
					stop_idle_scan(dd);
			}

			complete(&dd->suspend_resume);
			dd->expect_resume_ack = true;
			dd->resume_reset = false;
			while (!dd->resume_in_progress) {
				dd->suspended = true;
				/* the line below is a MUST */
				set_current_state(TASK_INTERRUPTIBLE);
				if (dd->gesture_en && dd->irq_registered &&
				    pdata->irq(pdata) == 0)
					gesture_wake_detect(dd);
				schedule();
			}
#if !SUSPEND_POWER_OFF
			if (dd->irq_registered) {
				if (dd->gesture_en)
					finish_gesture(dd);
				else if (dd->compatibility_mode || !dd->idle)
					start_scan_canned(dd);
			}
#endif
			dd->suspended = false;
			dd->suspend_in_progress = false;
			dd->resume_in_progress = false;
			complete(&dd->suspend_resume);

			fusion_dead = false;
			dd->send_fail_count = 0;
			do {
				if (dd->fusion_process != (pid_t)0 &&
				    get_pid_task(find_get_pid(
							dd->fusion_process),
						 PIDTYPE_PID) == NULL) {
					fusion_dead = true;
					break;
				}
				ret = nl_add_attr(dd->outgoing_skb->data,
						  FU_RESUME, NULL, 0);
				if (ret < 0) {
					ERROR("can't add data to resume " \
					      "buffer");
					nl_msg_init(dd->outgoing_skb->data,
						    dd->nl_family.id,
						    dd->nl_seq - 1, MC_FUSION);
					msleep(100);
					continue;
				}
				(void)skb_put(dd->outgoing_skb,
					      NL_SIZE(dd->outgoing_skb->data));
				ret = genlmsg_multicast(dd->outgoing_skb, 0,
						dd->nl_mc_groups[MC_FUSION].id,
						GFP_KERNEL);
				if (ret < 0) {
					ERROR("can't send resume message %d",
					      ret);
					msleep(100);
					if (++dd->send_fail_count >= 10) {
						fusion_dead = true;
						ret = 0;
					}
				}
				ret2 = nl_msg_new(dd, MC_FUSION);
				if (ret2 < 0)
					ERROR("could not allocate outgoing " \
					      "skb (%d)", ret2);
			} while (ret != 0);
			if (dd->send_fail_count>= 10 &&
			    dd->fusion_process != (pid_t)0)
			       (void)kill_pid(find_get_pid(dd->fusion_process),
					      SIGKILL, 1);
			dd->send_fail_count = 0;
			if (fusion_dead)
				continue;
		}

		/* priority 4: update /sys/device information */
		if (dd->sysfs_update_type != DR_SYSFS_UPDATE_NONE)
			if (!dd->expect_resume_ack) {
				if (send_sysfs_info(dd) < 0)
					ERROR(
					      "Can not send sysfs update to touch fusion"
					     );
				dd->sysfs_update_type = DR_SYSFS_UPDATE_NONE;
			}

		/* priority 5: service interrupt */
		if (dd->irq_registered && !dd->expect_resume_ack &&
		    (pdata->irq(pdata) == 0 || dd->resume_reset))
			dd->service_irq(dd);
		if (dd->irq_registered && !dd->expect_resume_ack &&
		    pdata->irq(pdata) == 0)
			continue;

		/* nothing more to do; sleep */
		schedule();
	}

	return 0;
}

/****************************************************************************\
* SYSFS interface                                                            *
\****************************************************************************/

static ssize_t chip_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "%04X\n", dd->chip_id);
}

static ssize_t fw_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{

	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "%04X\n", dd->fw_ver);
}

static ssize_t driver_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{

	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "%04X\n", dd->drv_ver);
}

static ssize_t tf_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "%04X\n", dd->tf_ver);
}

static ssize_t panel_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%04X\n", panel_id);
}

static ssize_t glove_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "%u\n", dd->glove_enabled);
}

static ssize_t glove_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	u32 temp;
	unsigned long           time_left;

	mutex_lock(&dd->sysfs_update_mutex);

	if (sscanf(buf, "%u", &temp) != 1) {
		ERROR("Invalid (%s)", buf);
		return -EINVAL;
	}
	dd->glove_enabled = !!temp;
	set_bit(DR_SYSFS_UPDATE_BIT_GLOVE, &(dd->sysfs_update_type));
	wake_up_process(dd->thread);
	time_left = wait_for_completion_timeout(&dd->sysfs_ack_glove, 3*HZ);

	mutex_unlock(&dd->sysfs_update_mutex);

	if (time_left > 0)
		return strnlen(buf, PAGE_SIZE);
	else
		return -EAGAIN;
}

static ssize_t gesture_wakeup_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "%u\n", dd->gesture_en);
}

static ssize_t gesture_wakeup_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	u32 temp;

	if (sscanf(buf, "%u", &temp) != 1) {
		ERROR("Invalid (%s)", buf);
		return -EINVAL;
	}
	dd->gesture_en = temp;

	return strnlen(buf, PAGE_SIZE);
}

static ssize_t charger_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "%u\n", dd->charger_mode_en);
}

static ssize_t charger_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	u32 temp;
	unsigned long           time_left;

	mutex_lock(&dd->sysfs_update_mutex);

	if (sscanf(buf, "%u", &temp) != 1) {
		ERROR("Invalid (%s)", buf);
		return -EINVAL;
	}
	if (temp > DR_WIRELESS_CHARGER) {
		ERROR("Charger Mode Value Out of Range");
		return -EINVAL;
	}
	dd->charger_mode_en = temp;
	set_bit(DR_SYSFS_UPDATE_BIT_CHARGER, &(dd->sysfs_update_type));
	wake_up_process(dd->thread);
	time_left = wait_for_completion_timeout(&dd->sysfs_ack_charger, 3*HZ);

	mutex_unlock(&dd->sysfs_update_mutex);

	if (time_left > 0)
		return strnlen(buf, PAGE_SIZE);
	else
		return -EAGAIN;
}

static ssize_t info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "chip id: 0x%04X driver ver: 0x%04X "
					"fw ver: 0x%04X panel id: 0x%04X "
					"tf ver: 0x%04X\n",
					dd->chip_id, dd->drv_ver, dd->fw_ver,
					panel_id, dd->tf_ver);
}

static ssize_t screen_status_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	u32 temp;

	if (sscanf(buf, "%u", &temp) != 1) {
		ERROR("Invalid (%s)", buf);
		return -EINVAL;
	}
	if (temp)
		late_resume(dev);
	else
		early_suspend(dev);

	return strnlen(buf, PAGE_SIZE);
}

static ssize_t tf_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "0x%08X", dd->tf_status);
}

static ssize_t default_loaded_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "%d",
			(dd->tf_status & TF_STATUS_DEFAULT_LOADED));
}

static ssize_t lcd_fps_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	return snprintf(buf, PAGE_SIZE, "%u\n", dd->lcd_fps);
}

static ssize_t lcd_fps_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct spi_device       *spi = to_spi_device(dev);
	struct dev_data         *dd = spi_get_drvdata(spi);
	u32 temp;
	unsigned long           time_left;

	mutex_lock(&dd->sysfs_update_mutex);

	if (sscanf(buf, "%u", &temp) != 1) {
		ERROR("Invalid (%s)", buf);
		return -EINVAL;
	}

	dd->lcd_fps = temp;
	set_bit(DR_SYSFS_UPDATE_BIT_LCD_FPS, &(dd->sysfs_update_type));
	wake_up_process(dd->thread);
	time_left = wait_for_completion_timeout(&dd->sysfs_ack_lcd_fps, 3*HZ);

	mutex_unlock(&dd->sysfs_update_mutex);

	if (time_left > 0)
		return strnlen(buf, PAGE_SIZE);
	else
		return -EAGAIN;
}

static struct device_attribute dev_attrs[] = {
	__ATTR(fw_ver, S_IRUGO, fw_ver_show, NULL),
	__ATTR(chip_id, S_IRUGO, chip_id_show, NULL),
	__ATTR(tf_ver, S_IRUGO, tf_ver_show, NULL),
	__ATTR(driver_ver, S_IRUGO, driver_ver_show, NULL),
	__ATTR(panel_id, S_IRUGO, panel_id_show, NULL),
	__ATTR(glove_en, S_IRUGO | S_IWUSR, glove_show, glove_store),
	__ATTR(gesture_wakeup, S_IRUGO | S_IWUSR, gesture_wakeup_show, gesture_wakeup_store),
	__ATTR(charger_mode, S_IRUGO | S_IWUSR, charger_mode_show, charger_mode_store),
	__ATTR(info, S_IRUGO, info_show, NULL),
	__ATTR(screen_status, S_IWUSR, NULL, screen_status_store),
	__ATTR(tf_status, S_IRUGO, tf_status_show, NULL),
	__ATTR(default_loaded, S_IRUGO, default_loaded_show, NULL),
	__ATTR(lcd_fps, S_IRUGO | S_IWUSR, lcd_fps_show, lcd_fps_store),
};

static int create_sysfs_entries(struct dev_data *dd)
{
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(dev_attrs); i++) {
		ret = device_create_file(&dd->spi->dev, &dev_attrs[i]);
		if (ret) {
			for (; i >= 0; --i) {
				device_remove_file(&dd->spi->dev,
							&dev_attrs[i]);
				dd->sysfs_created--;
			}
			break;
		}
		dd->sysfs_created++;
	}
	return ret;
}

static void remove_sysfs_entries(struct dev_data *dd)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dev_attrs); i++)
		if (dd->sysfs_created && dd->sysfs_created--)
			device_remove_file(&dd->spi->dev, &dev_attrs[i]);
}

/****************************************************************************\
* Driver initialization                                                      *
\****************************************************************************/

static int probe(struct spi_device *spi)
{
	struct maxim_sti_pdata  *pdata = spi->dev.platform_data;
	struct dev_data         *dd;
	unsigned long           flags;
	int                     ret, i;
	void                    *ptr;

	/* validate platform data */
	if (pdata == NULL || pdata->init == NULL || pdata->reset == NULL ||
		pdata->irq == NULL || pdata->touch_fusion == NULL ||
		pdata->config_file == NULL || pdata->nl_family == NULL ||
		GENL_CHK(pdata->nl_family) ||
		pdata->nl_mc_groups < MC_REQUIRED_GROUPS ||
		pdata->chip_access_method == 0 ||
		pdata->chip_access_method > ARRAY_SIZE(chip_access_methods) ||
		pdata->default_reset_state > 1)
			return -EINVAL;

	/* device context: allocate structure */
	dd = kzalloc(sizeof(*dd) + pdata->tx_buf_size + pdata->rx_buf_size +
		     sizeof(*dd->nl_ops) * pdata->nl_mc_groups +
		     sizeof(*dd->nl_mc_groups) * pdata->nl_mc_groups,
		     GFP_KERNEL);
	if (dd == NULL)
		return -ENOMEM;

	/* device context: set up dynamic allocation pointers */
	ptr = (void *)dd + sizeof(*dd);
	if (pdata->tx_buf_size > 0) {
		dd->tx_buf = ptr;
		ptr += pdata->tx_buf_size;
	}
	if (pdata->rx_buf_size > 0) {
		dd->rx_buf = ptr;
		ptr += pdata->rx_buf_size;
	}
	dd->nl_ops = ptr;
	ptr += sizeof(*dd->nl_ops) * pdata->nl_mc_groups;
	dd->nl_mc_groups = ptr;

	/* device context: initialize structure members */
	spi_set_drvdata(spi, dd);
	dd->spi = spi;
	dd->nl_seq = 1;
	init_completion(&dd->suspend_resume);
	memset(dd->tx_buf, 0xFF, pdata->tx_buf_size);
	(void)set_chip_access_method(dd, pdata->chip_access_method);

	/* initialize regulators */
	regulator_init(dd);

	/* initialize platform */
	ret = pdata->init(pdata, true);
	if (ret < 0)
		goto platform_failure;

	/* Netlink: initialize incoming skb queue */
	skb_queue_head_init(&dd->incoming_skb_queue);

	/* start processing thread */
	dd->thread_sched.sched_priority = MAX_USER_RT_PRIO / 2;
	dd->thread = kthread_run(processing_thread, dd, pdata->nl_family);
	if (IS_ERR(dd->thread)) {
		ret = PTR_ERR(dd->thread);
		goto platform_failure;
	}

	/* Netlink: register GENL family */
	dd->nl_family.id      = GENL_ID_GENERATE;
	dd->nl_family.version = NL_FAMILY_VERSION;
	GENL_COPY(dd->nl_family.name, pdata->nl_family);
	ret = genl_register_family(&dd->nl_family);
	if (ret < 0)
		goto nl_family_failure;

	/* Netlink: register family ops */
	for (i = 0; i < MC_REQUIRED_GROUPS; i++) {
		dd->nl_ops[i].cmd = i;
		dd->nl_ops[i].doit = nl_callback_noop;
	}
	dd->nl_ops[MC_DRIVER].doit = nl_callback_driver;
	dd->nl_ops[MC_FUSION].doit = nl_callback_fusion;
	for (i = 0; i < MC_REQUIRED_GROUPS; i++) {
		ret = genl_register_ops(&dd->nl_family, &dd->nl_ops[i]);
		if (ret < 0)
			goto nl_failure;
	}

	/* Netlink: register family multicast groups */
	GENL_COPY(dd->nl_mc_groups[MC_DRIVER].name, MC_DRIVER_NAME);
	GENL_COPY(dd->nl_mc_groups[MC_FUSION].name, MC_FUSION_NAME);
	for (i = 0; i < MC_REQUIRED_GROUPS; i++) {
		ret = genl_register_mc_group(&dd->nl_family,
					     &dd->nl_mc_groups[i]);
		if (ret < 0)
			goto nl_failure;
	}
	dd->nl_mc_group_count = MC_REQUIRED_GROUPS;

	/* Netlink: pre-allocate outgoing skb */
	ret = nl_msg_new(dd, MC_FUSION);
	if (ret < 0)
		goto nl_failure;

	/* Netlink: ready to start processing incoming messages */
	dd->nl_enabled = true;

	/* No sysfs update initially */
	dd->sysfs_update_type = DR_SYSFS_UPDATE_NONE;
	init_completion(&dd->sysfs_ack_glove);
	init_completion(&dd->sysfs_ack_charger);
	init_completion(&dd->sysfs_ack_lcd_fps);
	mutex_init(&dd->sysfs_update_mutex);

	/* add us to the devices list */
	spin_lock_irqsave(&dev_lock, flags);
	list_add_tail(&dd->dev_list, &dev_list);
	spin_unlock_irqrestore(&dev_lock, flags);

#if NV_ENABLE_CPU_BOOST
	dd->last_irq_jiffies = jiffies;
#endif
	ret = create_sysfs_entries(dd);
	if (ret) {
		INFO("failed to create sysfs file");
		goto sysfs_remove_group;
	}

	/* start up Touch Fusion */
	dd->start_fusion = true;
	wake_up_process(dd->thread);
	INFO("driver loaded; version %s; release date %s", DRIVER_VERSION,
	     DRIVER_RELEASE);

	return 0;

sysfs_remove_group:
	remove_sysfs_entries(dd);
nl_failure:
	genl_unregister_family(&dd->nl_family);
nl_family_failure:
	(void)kthread_stop(dd->thread);
platform_failure:
	pdata->init(pdata, false);
	kfree(dd);
	return ret;
}

static int remove(struct spi_device *spi)
{
	struct maxim_sti_pdata  *pdata = spi->dev.platform_data;
	struct dev_data         *dd = spi_get_drvdata(spi);
	unsigned long           flags;
	u8                      i;

	if (dd->fusion_process != (pid_t)0)
		(void)kill_pid(find_get_pid(dd->fusion_process), SIGKILL, 1);

	if (dd->parent != NULL)
		sysfs_remove_link(dd->input_dev[0]->dev.kobj.parent,
						MAXIM_STI_NAME);
	remove_sysfs_entries(dd);

	/* BEWARE: tear-down sequence below is carefully staged:            */
	/* 1) first the feeder of Netlink messages to the processing thread */
	/*    is turned off                                                 */
	/* 2) then the thread itself is shut down                           */
	/* 3) then Netlink family is torn down since no one would be using  */
	/*    it at this point                                              */
	/* 4) above step (3) insures that all Netlink senders are           */
	/*    definitely gone and it is safe to free up outgoing skb buffer */
	/*    and incoming skb queue                                        */
	dd->nl_enabled = false;
	(void)kthread_stop(dd->thread);
	genl_unregister_family(&dd->nl_family);
	kfree_skb(dd->outgoing_skb);
	skb_queue_purge(&dd->incoming_skb_queue);

	for (i = 0; i < INPUT_DEVICES; i++)
		if (dd->input_dev[i])
			input_unregister_device(dd->input_dev[i]);

	if (dd->irq_registered)
		free_irq(dd->spi->irq, dd);

	stop_scan_canned(dd);

	spin_lock_irqsave(&dev_lock, flags);
	list_del(&dd->dev_list);
	spin_unlock_irqrestore(&dev_lock, flags);

	pdata->reset(pdata, 0);
	usleep_range(100, 120);
	regulator_control(dd, false);
	pdata->init(pdata, false);

	kfree(dd);

	INFO("driver unloaded");
	return 0;
}

static void shutdown(struct spi_device *spi)
{
	struct maxim_sti_pdata  *pdata = spi->dev.platform_data;
	struct dev_data         *dd = spi_get_drvdata(spi);

	if (dd->parent != NULL)
		sysfs_remove_link(dd->input_dev[0]->dev.kobj.parent,
						MAXIM_STI_NAME);
	remove_sysfs_entries(dd);

	pdata->reset(pdata, 0);
	usleep_range(100, 120);
	regulator_control(dd, false);
}

/****************************************************************************\
* Module initialization                                                      *
\****************************************************************************/

static const struct spi_device_id id[] = {
	{ MAXIM_STI_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(spi, id);

static struct spi_driver driver = {
	.probe          = probe,
	.remove         = remove,
	.shutdown       = shutdown,
	.id_table       = id,
	.driver = {
		.name   = MAXIM_STI_NAME,
		.owner  = THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		.pm     = &pm_ops,
#endif
	},
};

static int __devinit maxim_sti_init(void)
{
	INIT_LIST_HEAD(&dev_list);
	spin_lock_init(&dev_lock);
	return spi_register_driver(&driver);
}

static void __exit maxim_sti_exit(void)
{
	spi_unregister_driver(&driver);
}

module_param(panel_id, ushort, S_IRUGO);
MODULE_PARM_DESC(panel_id, "Touch Panel ID Configuration");

module_init(maxim_sti_init);
module_exit(maxim_sti_exit);

MODULE_AUTHOR("Maxim Integrated Products, Inc.");
MODULE_DESCRIPTION("Maxim SmartTouch Imager Touchscreen Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRIVER_VERSION);

