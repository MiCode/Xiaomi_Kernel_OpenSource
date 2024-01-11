// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 InvenSense, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/sysfs.h>
#include <linux/types.h>

#include "ch101_sysfs.h"
#include "ch101_data.h"
#include "src/ch101_gpr.h"
#include "src/ch201_gprmt.h"

static int current_part;

static ssize_t ch101_fw_read(char *buf, loff_t off, size_t count,
	u8 *addr, u8 *end_addr)
{
	int read_count = 0;
	int i = 0;

	while (count > 0 && addr < end_addr) {
		if (count > MAX_DMP_READ_SIZE)
			read_count = MAX_DMP_READ_SIZE;
		else
			read_count = count;

		if (read_count > end_addr - addr)
			read_count = end_addr - addr;

		pr_info(TAG "%s: %d, %p, %p, %d", __func__,
			i, &buf[i], addr, read_count);
		memcpy((void *)&buf[i], (const void *)addr, read_count);

		addr += read_count;
		i += read_count;
		count -= read_count;
	}
	pr_info(TAG "%s: %d", __func__, read_count);

	return read_count;
}

/*
 * ch101_firmware_write() -  calling this function will load the firmware.
 */
static ssize_t ch101_firmware_write(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t pos, size_t size)
{
	struct iio_dev *indio_dev =
		dev_get_drvdata(container_of(kobj, struct device, kobj));
	struct ch101_data *data = iio_priv(indio_dev);
	ssize_t len;
	u16 ram_addr;
	u8  ram_size;

	if (current_part == CH101_PART_NUMBER)
		len = sizeof(ch101_gpr_fw);
	else
		len = sizeof(ch201_gprmt_fw);

	data->fw_initialized = 0;
	if (size > len - pos) {
		pr_err("firmware load failed\n");
		return -EFBIG;
	}

	pr_info(TAG "%s: %d, %d, %d", __func__,
		pos, size, len);

	if (current_part == CH101_PART_NUMBER) {

		pr_info(TAG "write CH101");
		memcpy(ch101_gpr_fw + pos, buf, len - pos);

		if ((len - pos) == size) {
			ram_size = (u8)ch101_gpr_fw[CH101_FW_SIZE];
			if (ram_size > 29)
				ram_size = 29;

			ram_addr = (u8)ch101_gpr_fw[CH101_FW_SIZE + 2];
			ram_addr <<= 8;
			ram_addr += (u8)ch101_gpr_fw[CH101_FW_SIZE + 1];

			set_ch101_gpr_fw_ram_init_addr(ram_addr);
			set_ch101_gpr_fw_ram_write_size(ram_size);

			memcpy(get_ram_ch101_gpr_init_ptr(),
				&ch101_gpr_fw[CH101_FW_SIZE + 3], ram_size);
		}
	} else {
		pr_info(TAG "write CH201");
		memcpy(ch201_gprmt_fw + pos, buf, len - pos);

		if ((len - pos) == size) {
			ram_size = (u8)ch201_gprmt_fw[CH101_FW_SIZE];
			if (ram_size > 29)
				ram_size = 29;

			ram_addr = (u8)ch201_gprmt_fw[CH101_FW_SIZE + 2];
			ram_addr <<= 8;
			ram_addr += (u8)ch201_gprmt_fw[CH101_FW_SIZE + 1];

			set_ch201_gpr_fw_ram_init_addr(ram_addr);
			set_ch201_gpr_fw_ram_write_size(ram_size);

			memcpy(get_ram_ch201_gprmt_init_ptr(),
				&ch201_gprmt_fw[CH201_FW_SIZE + 3], ram_size);
		}
	}
	return len;
}

static ssize_t ch101_firmware_read(struct file *filp,
			struct kobject *kobj,
			struct bin_attribute *bin_attr,
			char *buf, loff_t off, size_t count)
{
	u8 *addr = (u8 *)ch101_gpr_fw + off;
	u8 *end_addr = (u8 *)ch101_gpr_fw + CH101_FW_SIZE;


	return ch101_fw_read(buf, off, count, addr, end_addr);
}

/*
 * ch101_firmware_write_vers() -  calling this function will load the firmware.
 */
static ssize_t ch101_firmware_write_vers(struct file *fp, struct kobject *kobj,
			struct bin_attribute *attr, char *buf,
			loff_t pos, size_t size)
{
	ssize_t len = sizeof(ch101_gpr_version) - 2;
	char ch201_fw_name[] = "ch201";
	char input_file[] = "ch201";
	int res;

	if (size > len - pos) {
		pr_err("firmware version load failed\n");
		return -EFBIG;
	}

	pr_info(TAG "%s: %d, %d, %d", __func__,
		pos, size, len);

	memcpy(input_file, buf, sizeof(input_file));
	pr_info("size=%d, size2=%d, %s\n",
		sizeof(input_file), sizeof(ch201_fw_name), input_file);
	res = memcmp(buf, ch201_fw_name, sizeof(ch201_fw_name)-1);

	pr_info(TAG "%d, %s\n", res, buf);

	if (res) {
		memcpy(ch101_gpr_version + pos, buf, len - pos);
		current_part = CH101_PART_NUMBER;
	} else {
		memcpy(ch201_gprmt_version + pos, buf, len - pos);
		current_part = CH201_PART_NUMBER;
	}
	return len;
}

static ssize_t ch101_firmware_read_vers(struct file *filp,
			struct kobject *kobj,
			struct bin_attribute *bin_attr,
			char *buf, loff_t off, size_t count)
{
	u8 *addr = (u8 *)ch101_gpr_version + off;
	u8 *end_addr = (u8 *)ch101_gpr_version + CH101_FW_VERS_SIZE;

	ch101_fw_read(buf, off, count, addr, end_addr);

	return strlen(buf);
}

static struct bin_attribute dmp_firmware = {
	.attr = {
		.name = "misc_bin_dmp_firmware",
		.mode = 0666},
	.read = ch101_firmware_read,
	.write = ch101_firmware_write,
};

static struct bin_attribute dmp_firmware_vers = {
	.attr = {
		.name = "misc_bin_dmp_firmware_vers",
		.mode = 0666},
	.read = ch101_firmware_read_vers,
	.write = ch101_firmware_write_vers,
};

int ch101_create_dmp_sysfs(struct iio_dev *indio_dev)
{
	int result = 0;
	struct ch101_data *data = iio_priv(indio_dev);
	struct device *dev = data->dev;

	dev_info(dev, "%s:\n", __func__);

	dmp_firmware.size = CH101_FW_SIZE+32;
	result = sysfs_create_bin_file(&indio_dev->dev.kobj, &dmp_firmware);
	if (result)
		return result;

	dmp_firmware_vers.size = CH101_FW_VERS_SIZE;
	result = sysfs_create_bin_file(&indio_dev->dev.kobj,
		&dmp_firmware_vers);
	if (result)
		return result;

	return result;
}
