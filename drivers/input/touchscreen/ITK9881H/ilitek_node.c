/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 */

#include "ilitek.h"

#define USER_STR_BUFF		PAGE_SIZE
#define IOCTL_I2C_BUFF		PAGE_SIZE
#define ILITEK_IOCTL_MAGIC	100
#define ILITEK_IOCTL_MAXNR	21

#define ILITEK_IOCTL_I2C_WRITE_DATA		_IOWR(ILITEK_IOCTL_MAGIC, 0, u8*)
#define ILITEK_IOCTL_I2C_SET_WRITE_LENGTH	_IOWR(ILITEK_IOCTL_MAGIC, 1, int)
#define ILITEK_IOCTL_I2C_READ_DATA		_IOWR(ILITEK_IOCTL_MAGIC, 2, u8*)
#define ILITEK_IOCTL_I2C_SET_READ_LENGTH	_IOWR(ILITEK_IOCTL_MAGIC, 3, int)

#define ILITEK_IOCTL_TP_HW_RESET		_IOWR(ILITEK_IOCTL_MAGIC, 4, int)
#define ILITEK_IOCTL_TP_POWER_SWITCH		_IOWR(ILITEK_IOCTL_MAGIC, 5, int)
#define ILITEK_IOCTL_TP_REPORT_SWITCH		_IOWR(ILITEK_IOCTL_MAGIC, 6, int)
#define ILITEK_IOCTL_TP_IRQ_SWITCH		_IOWR(ILITEK_IOCTL_MAGIC, 7, int)

#define ILITEK_IOCTL_TP_DEBUG_LEVEL		_IOWR(ILITEK_IOCTL_MAGIC, 8, int)
#define ILITEK_IOCTL_TP_FUNC_MODE		_IOWR(ILITEK_IOCTL_MAGIC, 9, int)

#define ILITEK_IOCTL_TP_FW_VER			_IOWR(ILITEK_IOCTL_MAGIC, 10, u8*)
#define ILITEK_IOCTL_TP_PL_VER			_IOWR(ILITEK_IOCTL_MAGIC, 11, u8*)
#define ILITEK_IOCTL_TP_CORE_VER		_IOWR(ILITEK_IOCTL_MAGIC, 12, u8*)
#define ILITEK_IOCTL_TP_DRV_VER			_IOWR(ILITEK_IOCTL_MAGIC, 13, u8*)
#define ILITEK_IOCTL_TP_CHIP_ID			_IOWR(ILITEK_IOCTL_MAGIC, 14, u32*)

#define ILITEK_IOCTL_TP_NETLINK_CTRL		_IOWR(ILITEK_IOCTL_MAGIC, 15, int*)
#define ILITEK_IOCTL_TP_NETLINK_STATUS		_IOWR(ILITEK_IOCTL_MAGIC, 16, int*)

#define ILITEK_IOCTL_TP_MODE_CTRL		_IOWR(ILITEK_IOCTL_MAGIC, 17, u8*)
#define ILITEK_IOCTL_TP_MODE_STATUS		_IOWR(ILITEK_IOCTL_MAGIC, 18, int*)
#define ILITEK_IOCTL_ICE_MODE_SWITCH		_IOWR(ILITEK_IOCTL_MAGIC, 19, int)

#define ILITEK_IOCTL_TP_INTERFACE_TYPE		_IOWR(ILITEK_IOCTL_MAGIC, 20, u8*)
#define ILITEK_IOCTL_TP_DUMP_FLASH		_IOWR(ILITEK_IOCTL_MAGIC, 21, int)

#ifdef CONFIG_COMPAT
#define ILITEK_COMPAT_IOCTL_I2C_WRITE_DATA		_IOWR(ILITEK_IOCTL_MAGIC, 0, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_I2C_SET_WRITE_LENGTH	_IOWR(ILITEK_IOCTL_MAGIC, 1, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_I2C_READ_DATA		_IOWR(ILITEK_IOCTL_MAGIC, 2, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_I2C_SET_READ_LENGTH		_IOWR(ILITEK_IOCTL_MAGIC, 3, compat_uptr_t)

#define ILITEK_COMPAT_IOCTL_TP_HW_RESET			_IOWR(ILITEK_IOCTL_MAGIC, 4, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_POWER_SWITCH		_IOWR(ILITEK_IOCTL_MAGIC, 5, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_REPORT_SWITCH		_IOWR(ILITEK_IOCTL_MAGIC, 6, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_IRQ_SWITCH		_IOWR(ILITEK_IOCTL_MAGIC, 7, compat_uptr_t)

#define ILITEK_COMPAT_IOCTL_TP_DEBUG_LEVEL		_IOWR(ILITEK_IOCTL_MAGIC, 8, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_FUNC_MODE		_IOWR(ILITEK_IOCTL_MAGIC, 9, compat_uptr_t)

#define ILITEK_COMPAT_IOCTL_TP_FW_VER			_IOWR(ILITEK_IOCTL_MAGIC, 10, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_PL_VER			_IOWR(ILITEK_IOCTL_MAGIC, 11, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_CORE_VER			_IOWR(ILITEK_IOCTL_MAGIC, 12, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_DRV_VER			_IOWR(ILITEK_IOCTL_MAGIC, 13, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_CHIP_ID			_IOWR(ILITEK_IOCTL_MAGIC, 14, compat_uptr_t)

#define ILITEK_COMPAT_IOCTL_TP_NETLINK_CTRL		_IOWR(ILITEK_IOCTL_MAGIC, 15, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_NETLINK_STATUS		_IOWR(ILITEK_IOCTL_MAGIC, 16, compat_uptr_t)

#define ILITEK_COMPAT_IOCTL_TP_MODE_CTRL		_IOWR(ILITEK_IOCTL_MAGIC, 17, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_MODE_STATUS		_IOWR(ILITEK_IOCTL_MAGIC, 18, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_ICE_MODE_SWITCH		_IOWR(ILITEK_IOCTL_MAGIC, 19, compat_uptr_t)

#define ILITEK_COMPAT_IOCTL_TP_INTERFACE_TYPE		_IOWR(ILITEK_IOCTL_MAGIC, 20, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_DUMP_FLASH		_IOWR(ILITEK_IOCTL_MAGIC, 21, compat_uptr_t)
#endif

unsigned char g_user_buf[USER_STR_BUFF] = {0};

int str2hex(char *str)
{
	int strlen, result, intermed, intermedtop;
	char *s = str;

	while (*s != 0x0) {
		s++;
	}

	strlen = (int)(s - str);
	s = str;
	if (*s != 0x30) {
		return -EINVAL;
	}

	s++;

	if (*s != 0x78 && *s != 0x58) {
		return -EINVAL;
	}
	s++;

	strlen = strlen - 3;
	result = 0;
	while (*s != 0x0) {
		intermed = *s & 0x0f;
		intermedtop = *s & 0xf0;
		if (intermedtop == 0x60 || intermedtop == 0x40) {
			intermed += 0x09;
		}
		intermed = intermed << (strlen << 2);
		result = result | intermed;
		strlen -= 1;
		s++;
	}
	return result;
}

int katoi(char *str)
{
	int result = 0;
	unsigned int digit;
	int sign;

	if (*str == '-') {
		sign = 1;
		str += 1;
	} else {
		sign = 0;
		if (*str == '+') {
			str += 1;
		}
	}

	for (;; str += 1) {
		digit = *str - '0';
		if (digit > 9)
			break;
		result = (10 * result) + digit;
	}

	if (sign) {
		return -result;
	}
	return result;
}

struct file_buffer {
	char *ptr;
	char file_name[128];
	int32_t file_len;
	int32_t file_max_zise;
};

static int file_write(struct file_buffer *file, bool new_open)
{
	struct file *f = NULL;
	struct filename *vts_name;
	mm_segment_t fs;
	loff_t pos;

	if (file->ptr == NULL) {
		ipio_err("str is invaild\n");
		return -EINVAL;
	}

	if (file->file_name == NULL) {
		ipio_err("file name is invaild\n");
		return -EINVAL;
	}

	if (file->file_len >= file->file_max_zise) {
		ipio_err("The length saved to file is too long !\n");
		return -EINVAL;
	}

	if (new_open) {
		vts_name = getname_kernel(file->file_name);
		f = file_open_name(vts_name, O_WRONLY | O_CREAT | O_TRUNC, 644);
	} else {
		vts_name = getname_kernel(file->file_name);
		f = file_open_name(vts_name, O_WRONLY | O_CREAT | O_APPEND, 644);
	}

	if (ERR_ALLOC_MEM(f)) {
		ipio_err("Failed to open %s file\n", file->file_name);
		return -EINVAL;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(f, file->ptr, file->file_len, &pos);
	set_fs(fs);
	filp_close(f, NULL);
	return 0;
}
/*
static int debug_mode_switch(void)
{
	int r, row = 1024, col = 2048, ret = 0;

	idev->debug_node_open = !idev->debug_node_open;

	if (idev->debug_node_open) {
		idev->debug_buf = (unsigned char **)kzalloc(row * sizeof(unsigned char *), GFP_KERNEL);
		if (ERR_ALLOC_MEM(idev->debug_buf)) {
			ipio_err("Failed to allocate debug_buf memory, %ld\n", PTR_ERR(idev->debug_buf));
			idev->debug_node_open = false;
			ret = -ENOMEM;
			goto out;
		}
		for (r = 0; r < row; ++r) {
			idev->debug_buf[r] = (unsigned char *)kzalloc(col * sizeof(unsigned char), GFP_KERNEL);
			if (ERR_ALLOC_MEM(idev->debug_buf[r])) {
				ipio_err("Failed to allocate debug_buf[%d] memory, %ld\n", r, PTR_ERR(idev->debug_buf[r]));
				idev->debug_node_open = false;
				ret = -ENOMEM;
				goto out;
			}
			//memset(idev->debug_buf[r], 0, sizeof(unsigned char) * col);
		}
	}

out:
	for (r = 0; r < row; ++r)
		ipio_kfree((void **)idev->debug_buf[r]);

	ipio_kfree((void **)idev->debug_buf);

	return ret;
}
*/
static int debug_mode_get_data(struct file_buffer *file, u8 type, u32 frame_count)
{
	int ret;
	int timeout = 50;
	u8 cmd[2] = { 0 }, row, col;
	s16 temp;
	unsigned char *ptr;
	int j;
	u16 write_index = 0;

	idev->debug_node_open = false;
	idev->debug_data_frame = 0;
	row = idev->ych_num;
	col = idev->xch_num;

	mutex_lock(&idev->touch_mutex);
	cmd[0] = 0xFA;
	cmd[1] = type;
	ret = idev->write(cmd, 2);
	idev->debug_node_open = true;
	mutex_unlock(&idev->touch_mutex);
	if (ret < 0) {
		ipio_err("Write 0xFA,0x%x failed\n", type);
		return ret;
	}

	while ((write_index < frame_count) && (timeout > 0)) {
		ipio_info("frame = %d,index = %d,count = %d\n", write_index, write_index % 1024, idev->debug_data_frame);
		if ((write_index % 1024) < idev->debug_data_frame) {
			mutex_lock(&idev->touch_mutex);
			file->file_len = 0;
			memset(file->ptr, 0, file->file_max_zise);
			file->file_len += snprintf(file->ptr + file->file_len, PAGE_SIZE, "\n\nFrame%d,", write_index);
			for (j = 0; j < col; j++)
				file->file_len += snprintf(file->ptr + file->file_len, PAGE_SIZE, "[X%d] ,", j);
			ptr = &idev->debug_buf[write_index%1024][35];
			for (j = 0; j < row * col; j++, ptr += 2) {
				temp = (*ptr << 8) + *(ptr + 1);
				if (j % col == 0)
					file->file_len += snprintf(file->ptr + file->file_len, PAGE_SIZE, "\n[Y%d] ,", (j / col));
				file->file_len += snprintf(file->ptr + file->file_len, PAGE_SIZE, "%d, ", temp);
			}
			file->file_len += snprintf(file->ptr + file->file_len, PAGE_SIZE, "\n[X] ,");
			for (j = 0; j < row + col; j++, ptr += 2) {
				temp = (*ptr << 8) + *(ptr + 1);
				if (j == col)
					file->file_len += snprintf(file->ptr + file->file_len, PAGE_SIZE, "\n[Y] ,");
				file->file_len += snprintf(file->ptr + file->file_len, PAGE_SIZE, "%d, ", temp);
			}
			file_write(file, false);
			write_index++;
			mutex_unlock(&idev->touch_mutex);
			timeout = 50;
		}

		if (write_index % 1024 == 0 && idev->debug_data_frame == 1024)
			idev->debug_data_frame = 0;

		mdelay(100);/*get one frame data taken around 130ms*/
		timeout--;
		if (timeout == 0)
			ipio_err("debug mode get data timeout!\n");
	}
	idev->debug_node_open = false;
	return 0;
}

static int dev_mkdir(char *name, umode_t mode)
{
	struct dentry *dentry;
	struct path path;
	int err;

	ipio_info("mkdir: %s\n", name);

	dentry = kern_path_create(AT_FDCWD, name, &path, LOOKUP_DIRECTORY);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	err = vfs_mkdir(path.dentry->d_inode, dentry, mode);
	done_path_create(&path, dentry);
	return err;
}

static ssize_t ilitek_proc_get_delta_data_read(struct file *pFile, char __user *buf, size_t size, loff_t *pos)
{
	s16 *delta = NULL;
	int row = 0, col = 0,  index = 0;
	int ret, i, x, y;
	int read_length = 0;
	u8 cmd[2] = {0};
	u8 *data = NULL;

	if (*pos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	ilitek_tddi_wq_ctrl(WQ_ESD, DISABLE);
	ilitek_tddi_wq_ctrl(WQ_BAT, DISABLE);
	mutex_lock(&idev->touch_mutex);

	row = idev->ych_num;
	col = idev->xch_num;
	read_length = 4 + 2 * row * col + 1 ;

	ipio_info("read length = %d\n", read_length);

	data = kcalloc(read_length + 1, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(data)) {
		ipio_err("Failed to allocate data mem\n");
		return 0;
	}

	delta = kcalloc(P5_X_DEBUG_MODE_PACKET_LENGTH, sizeof(s32), GFP_KERNEL);
	if (ERR_ALLOC_MEM(delta)) {
		ipio_err("Failed to allocate delta mem\n");
		return 0;
	}

	cmd[0] = 0xB7;
	cmd[1] = 0x1; //get delta
	ret = idev->write(cmd, sizeof(cmd));
	if (ret < 0) {
		ipio_err("Failed to write 0xB7,0x1 command, %d\n", ret);
		goto out;
	}

	msleep(120);

	/* read debug packet header */
	ret = idev->read(data, read_length);
	if (ret < 0) {
		ipio_err("Read debug packet header failed, %d\n", ret);
		goto out;
	}

	cmd[1] = 0x03; //switch to normal mode
	ret = idev->write(cmd, sizeof(cmd));
	if (ret < 0) {
		ipio_err("Failed to write 0xB7,0x3 command, %d\n", ret);
		goto out;
	}

	for (i = 4, index = 0; index < row * col * 2; i += 2, index++)
		delta[index] = (data[i] << 8) + data[i + 1];

	size = snprintf(g_user_buf + size, PAGE_SIZE - size, "======== Deltadata ========\n");
	ipio_info("======== Deltadata ========\n");

	size += snprintf(g_user_buf + size, PAGE_SIZE - size,
		"Header 0x%x ,Type %d, Length %d\n", data[0], data[1], (data[2] << 8) | data[3]);
	ipio_info("Header 0x%x ,Type %d, Length %d\n", data[0], data[1], (data[2] << 8) | data[3]);

	// print delta data
	for (y = 0; y < row; y++) {
		size += snprintf(g_user_buf + size, PAGE_SIZE - size, "[%2d] ", (y+1));
		ipio_info("[%2d] ", (y+1));

		for (x = 0; x < col; x++) {
			int shift = y * col + x;
			size += snprintf(g_user_buf + size, PAGE_SIZE - size, "%5d", delta[shift]);
			printk(KERN_CONT "%5d", delta[shift]);
		}
		size += snprintf(g_user_buf + size, PAGE_SIZE - size, "\n");
		printk(KERN_CONT "\n");
	}

	ret = copy_to_user(buf, g_user_buf, size);
	if (ret < 0) {
		ipio_err("Failed to copy data to user space");
	}

	*pos += size;

out:
	mutex_unlock(&idev->touch_mutex);
	ilitek_tddi_wq_ctrl(WQ_ESD, ENABLE);
	ilitek_tddi_wq_ctrl(WQ_BAT, ENABLE);
	ipio_kfree((void **)&data);
	ipio_kfree((void **)&delta);
	return size;
}

static ssize_t ilitek_proc_fw_get_raw_data_read(struct file *pFile, char __user *buf, size_t size, loff_t *pos)
{
	s16 *rawdata = NULL;
	int row = 0, col = 0,  index = 0;
	int ret, i, x, y;
	int read_length = 0;
	u8 cmd[2] = {0};
	u8 *data = NULL;

	if (*pos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	ilitek_tddi_wq_ctrl(WQ_ESD, DISABLE);
	ilitek_tddi_wq_ctrl(WQ_BAT, DISABLE);
	mutex_lock(&idev->touch_mutex);

	row = idev->ych_num;
	col = idev->xch_num;
	read_length = 4 + 2 * row * col + 1 ;

	ipio_info("read length = %d\n", read_length);

	data = kcalloc(read_length + 1, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(data)) {
			ipio_err("Failed to allocate data mem\n");
			return 0;
	}

	rawdata = kcalloc(P5_X_DEBUG_MODE_PACKET_LENGTH, sizeof(s32), GFP_KERNEL);
	if (ERR_ALLOC_MEM(rawdata)) {
			ipio_err("Failed to allocate rawdata mem\n");
			return 0;
	}

	cmd[0] = 0xB7;
	cmd[1] = 0x2; //get rawdata
	ret = idev->write(cmd, sizeof(cmd));
	if (ret < 0) {
		ipio_err("Failed to write 0xB7,0x2 command, %d\n", ret);
		goto out;
	}

	msleep(120);

	/* read debug packet header */
	ret = idev->read(data, read_length);
	if (ret < 0) {
		ipio_err("Read debug packet header failed, %d\n", ret);
		goto out;
	}

	cmd[1] = 0x03; //switch to normal mode
	ret = idev->write(cmd, sizeof(cmd));
	if (ret < 0) {
		ipio_err("Failed to write 0xB7,0x3 command, %d\n", ret);
		goto out;
	}

	for (i = 4, index = 0; index < row * col * 2; i += 2, index++)
		rawdata[index] = (data[i] << 8) + data[i + 1];

	size = snprintf(g_user_buf, PAGE_SIZE, "======== RawData ========\n");
	ipio_info("======== RawData ========\n");

	size += snprintf(g_user_buf + size, PAGE_SIZE - size,
			"Header 0x%x ,Type %d, Length %d\n", data[0], data[1], (data[2] << 8) | data[3]);
	ipio_info("Header 0x%x ,Type %d, Length %d\n", data[0], data[1], (data[2] << 8) | data[3]);

	// print raw data
	for (y = 0; y < row; y++) {
		size += snprintf(g_user_buf + size, PAGE_SIZE - size, "[%2d] ", (y+1));
		ipio_info("[%2d] ", (y+1));

		for (x = 0; x < col; x++) {
			int shift = y * col + x;
			size += snprintf(g_user_buf + size, PAGE_SIZE - size, "%5d", rawdata[shift]);
			printk(KERN_CONT "%5d", rawdata[shift]);
		}
		size += snprintf(g_user_buf + size, PAGE_SIZE - size, "\n");
		printk(KERN_CONT "\n");
	}

	ret = copy_to_user(buf, g_user_buf, size);
	if (ret < 0) {
		ipio_err("Failed to copy data to user space");
	}

	*pos += size;

out:
	mutex_unlock(&idev->touch_mutex);
	ilitek_tddi_wq_ctrl(WQ_ESD, ENABLE);
	ilitek_tddi_wq_ctrl(WQ_BAT, ENABLE);
	ipio_kfree((void **)&data);
	ipio_kfree((void **)&rawdata);
	return size;
}

static ssize_t ilitek_proc_fw_pc_counter_read(struct file *pFile, char __user *buf, size_t size, loff_t *pos)
{
	u32 pc;

	if (*pos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	pc = ilitek_tddi_ic_get_pc_counter();
	size = snprintf(g_user_buf, PAGE_SIZE, "pc counter = 0x%x\n", pc);
	pc = copy_to_user(buf, g_user_buf, size);
	if (pc < 0)
		ipio_err("Failed to copy data to user space");

	*pos += size;
	return size;
}

u32 rw_reg[5] = {0};
static ssize_t ilitek_proc_rw_tp_reg_read(struct file *pFile, char __user *buf, size_t size, loff_t *pos)
{
	int ret = 0;
	bool mcu_on = 0, read = 0;
	u32 type, addr, read_data, write_data, write_len, stop_mcu;

	if (*pos != 0)
		return 0;

	stop_mcu = rw_reg[0];
	type = rw_reg[1];
	addr = rw_reg[2];
	write_data = rw_reg[3];
	write_len = rw_reg[4];

	ipio_info("stop_mcu = %d\n", rw_reg[0]);

	ilitek_tddi_wq_ctrl(WQ_ESD, DISABLE);
	ilitek_tddi_wq_ctrl(WQ_BAT, DISABLE);
	mutex_lock(&idev->touch_mutex);

	if (stop_mcu == mcu_on) {
		ret = ilitek_ice_mode_ctrl(ENABLE, ON);
		if (ret < 0) {
			ipio_err("Failed to enter ICE mode, ret = %d\n", ret);
			return -EINVAL;
		}
	} else {
		ret = ilitek_ice_mode_ctrl(ENABLE, OFF);
		if (ret < 0) {
			ipio_err("Failed to enter ICE mode, ret = %d\n", ret);
			return -EINVAL;
		}
	}

	if (type == read) {
		if (ilitek_ice_mode_read(addr, &read_data, sizeof(u32)) < 0)
			ipio_err("Read data error\n");
		ipio_info("READ:addr = 0x%06x, read = 0x%08x\n", addr, read_data);
		size = snprintf(g_user_buf, PAGE_SIZE, "READ:addr = 0x%06x, read = 0x%08x\n", addr, read_data);
	} else {
		ilitek_ice_mode_write(addr, write_data, write_len);
		ipio_info("WRITE:addr = 0x%06x, write = 0x%08x, len =%d byte\n", addr, write_data, write_len);
		size = snprintf(g_user_buf, PAGE_SIZE, "WRITE:addr = 0x%06x, write = 0x%08x, len =%d byte\n", addr, write_data, write_len);
	}

	if (stop_mcu == mcu_on)
		ilitek_ice_mode_ctrl(DISABLE, ON);
	else
		ilitek_ice_mode_ctrl(DISABLE, OFF);

	ret = copy_to_user(buf, g_user_buf, size);
	if (ret < 0)
		ipio_err("Failed to copy data to user space");

	*pos += size;
	mutex_unlock(&idev->touch_mutex);
	ilitek_tddi_wq_ctrl(WQ_ESD, ENABLE);
	ilitek_tddi_wq_ctrl(WQ_BAT, ENABLE);
	return size;
}

static ssize_t ilitek_proc_rw_tp_reg_write(struct file *filp, const char *buff, size_t size, loff_t *pos)
{
	int ret = 0;
	char *token = NULL, *cur = NULL;
	char cmd[256] = { 0 };
	u32 count = 0;

	if (buff != NULL) {
		ret = copy_from_user(cmd, buff, size - 1);
		if (ret < 0) {
			ipio_info("copy data from user space, failed\n");
			return -EINVAL;
		}
	}
	token = cur = cmd;
	while ((token = strsep(&cur, ",")) != NULL) {
		rw_reg[count] = str2hex(token);
		ipio_info("rw_reg[%d] = 0x%x\n", count, rw_reg[count]);
		count++;
	}
	return size;
}

static ssize_t ilitek_proc_debug_switch_read(struct file *pFile, char __user *buff, size_t size, loff_t *pos)
{
	int ret = 0;

	if (*pos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));
/*
	ret = debug_mode_switch();
	if (ret < 0)
		ipio_err("Failed switch to debug mode\n");
*/
	idev->debug_node_open = !idev->debug_node_open;
	ipio_info("idev->debug_node_open = %d", idev->debug_node_open);
	ipio_info(" %s debug_flag message = %x\n", idev->debug_node_open ? "Enabled" : "Disabled", idev->debug_node_open);

	size = snprintf(g_user_buf, 50, "debug_node_open : %s\n", idev->debug_node_open ? "Enabled" : "Disabled");

	*pos += size;

	ret = copy_to_user(buff, g_user_buf, size);
	if (ret < 0)
		ipio_err("Failed to copy data to user space");

	return size;
}

static ssize_t ilitek_proc_debug_message_read(struct file *filp, char __user *buff, size_t size, loff_t *pos)
{
	unsigned long p = *pos;
	int i = 0;
	int send_data_len = 0;
	int ret = 0;
	int data_count = 0;
	int one_data_bytes = 0;
	int need_read_data_len = 0;
	int type = 0;
	unsigned char *tmpbuf = NULL;
	unsigned char tmpbufback[128] = {0};

	mutex_lock(&idev->debug_read_mutex);

	while (idev->debug_data_frame <= 0) {
		if (filp->f_flags & O_NONBLOCK) {
			return -EAGAIN;
		}
		wait_event_interruptible(idev->inq, idev->debug_data_frame > 0);
	}

	mutex_lock(&idev->debug_mutex);

	tmpbuf = vmalloc(4096);	/* buf size if even */
	if (ERR_ALLOC_MEM(tmpbuf)) {
		ipio_err("buffer vmalloc error\n");
		send_data_len += snprintf(tmpbufback + send_data_len, 100, "buffer vmalloc error\n");
		ret = copy_to_user(buff, tmpbufback, send_data_len); /*idev->debug_buf[0] */
		goto out;
	}

	if (idev->debug_data_frame > 0) {
		if (idev->debug_buf[0][0] == P5_X_DEMO_PACKET_ID) {
			need_read_data_len = 43;
		} else if (idev->debug_buf[0][0] == P5_X_I2CUART_PACKET_ID) {
			type = idev->debug_buf[0][3] & 0x0F;

			data_count = idev->debug_buf[0][1] * idev->debug_buf[0][2];

			if (type == 0 || type == 1 || type == 6) {
				one_data_bytes = 1;
			} else if (type == 2 || type == 3) {
				one_data_bytes = 2;
			} else if (type == 4 || type == 5) {
				one_data_bytes = 4;
			}
			need_read_data_len = data_count * one_data_bytes + 1 + 5;
		} else if (idev->debug_buf[0][0] == P5_X_DEBUG_PACKET_ID) {
			send_data_len = 0;	/* idev->debug_buf[0][1] - 2; */
			need_read_data_len = 2040;
		}

		for (i = 0; i < need_read_data_len; i++) {
			send_data_len += snprintf(tmpbuf + send_data_len, PAGE_SIZE, "%02X", idev->debug_buf[0][i]);
			if (send_data_len >= 4096) {
				ipio_err("send_data_len = %d set 4096 i = %d\n", send_data_len, i);
				send_data_len = 4096;
				break;
			}
		}

		send_data_len += snprintf(tmpbuf + send_data_len, 100, "\n\n");

		if (p == 5 || size == 4096 || size == 2048) {
			idev->debug_data_frame--;

			if (idev->debug_data_frame < 0)
				idev->debug_data_frame = 0;

			for (i = 1; i <= idev->debug_data_frame; i++)
				memcpy(idev->debug_buf[i - 1], idev->debug_buf[i], 2048);
		}

	} else {
		ipio_err("no data send\n");
		send_data_len += snprintf(tmpbuf + send_data_len, PAGE_SIZE, "no data send\n");
	}

	/* Preparing to send debug data to user */
	if (size == 4096)
		ret = copy_to_user(buff, tmpbuf, send_data_len);
	else
		ret = copy_to_user(buff, tmpbuf + p, send_data_len - p);

	/* ipio_err("send_data_len = %d\n", send_data_len); */
	if (send_data_len <= 0 || send_data_len > 4096) {
		ipio_err("send_data_len = %d set 4096\n", send_data_len);
		send_data_len = 4096;
	}

	if (ret) {
		ipio_err("copy_to_user err\n");
		ret = -EFAULT;
	} else {
		*pos += send_data_len;
		ret = send_data_len;
		ipio_debug("Read %d bytes(s) from %ld\n", send_data_len, p);
	}

out:
	mutex_unlock(&idev->debug_mutex);
	mutex_unlock(&idev->debug_read_mutex);
	ipio_vfree((void **)&tmpbuf);
	return send_data_len;
}

static ssize_t ilitek_proc_get_debug_mode_data_read(struct file *filp, char __user *buff, size_t size, loff_t *pos)
{
	int ret;
	u8 tp_mode;
	struct file_buffer csv;

	if (*pos != 0)
		return 0;

	/* initialize file */
	memset(csv.file_name, 0, sizeof(csv.file_name));
	snprintf(csv.file_name, PAGE_SIZE, "%s", DEBUG_DATA_FILE_PATH);
	csv.file_len = 0;
	csv.file_max_zise = DEBUG_DATA_FILE_SIZE;
	csv.ptr = vmalloc(csv.file_max_zise);

	if (ERR_ALLOC_MEM(csv.ptr)) {
		ipio_err("Failed to allocate CSV mem\n");
		goto out;
	}

	/* save data to csv */
	ipio_info("Get Raw data %d frame\n", idev->raw_count);
	ipio_info("Get Delta data %d frame\n", idev->delta_count);
	csv.file_len += snprintf(csv.ptr + csv.file_len, PAGE_SIZE, "Get Raw data %d frame\n", idev->raw_count);
	csv.file_len += snprintf(csv.ptr + csv.file_len, PAGE_SIZE, "Get Delta data %d frame\n", idev->delta_count);
	file_write(&csv, true);

	/* change to debug mode */
	tp_mode = P5_X_FW_DEBUG_MODE;
	ret = ilitek_tddi_switch_mode(&tp_mode);
	if (ret < 0)
		goto out;

	/* get raw data */
	csv.file_len = 0;
	memset(csv.ptr, 0, csv.file_max_zise);
	csv.file_len += snprintf(csv.ptr + csv.file_len, PAGE_SIZE, "\n\n=======Raw data=======");
	file_write(&csv, false);
	ret = debug_mode_get_data(&csv, P5_X_FW_RAW_DATA_MODE, idev->raw_count);
	if (ret < 0)
		goto out;

	/* get delta data */
	csv.file_len = 0;
	memset(csv.ptr, 0, csv.file_max_zise);
	csv.file_len += snprintf(csv.ptr + csv.file_len, PAGE_SIZE, "\n\n=======Delta data=======");
	file_write(&csv, false);
	ret = debug_mode_get_data(&csv, P5_X_FW_DELTA_DATA_MODE, idev->delta_count);
	if (ret < 0)
		goto out;

	/* change to demo mode */
	tp_mode = P5_X_FW_DEMO_MODE;
	ret = ilitek_tddi_switch_mode(&tp_mode);
	if (ret < 0) {
		ipio_err("ilitek_tddi_switch_mode failed\n");
		goto out;
	}
out:
	ipio_vfree((void **)&csv.ptr);
	return 0;
}

static ssize_t ilitek_proc_get_debug_mode_data_write(struct file *filp, const char *buff, size_t size, loff_t *pos)
{
	int ret = 0;
	char *token = NULL, *cur = NULL;
	char cmd[256] = {0};
	u8 temp[256] = {0}, count = 0;

	if (buff != NULL) {
		ret = copy_from_user(cmd, buff, size - 1);
		if (ret < 0) {
			ipio_info("copy data from user space, failed\n");
			return -EINVAL;
		}
	}

	ipio_info("size = %d, cmd = %s\n", (int)size, cmd);
	token = cur = cmd;
	while ((token = strsep(&cur, ",")) != NULL) {
		temp[count] = str2hex(token);
		ipio_info("temp[%d] = %d\n", count, temp[count]);
		count++;
	}

	idev->raw_count = ((temp[0] << 8) | temp[1]);
	idev->delta_count = ((temp[2] << 8) | temp[3]);
	idev->bg_count = ((temp[4] << 8) | temp[5]);

	ipio_info("Raw_count = %d, Delta_count = %d, BG_count = %d\n", idev->raw_count, idev->delta_count, idev->bg_count);
	return size;
}

static ssize_t ilitek_node_mp_lcm_on_test_read(struct file *filp, char __user *buff, size_t size, loff_t *pos)
{
	int ret = 0;
	char apk_ret[100] = {0};

	ipio_info("Run MP test with LCM on\n");

	if (*pos != 0)
		return 0;

	/* Create the directory for mp_test result */
	ret = dev_mkdir(CSV_LCM_ON_PATH, S_IRUGO | S_IWUSR);
	if (ret != 0)
		ipio_err("Failed to create directory for mp_test\n");

	ilitek_tddi_mp_test_handler(apk_ret, ON);
	ipio_info("MP TEST %s\n", (ret < 0) ? "FAIL" : "PASS");

	ret = copy_to_user((char *)buff, apk_ret, sizeof(apk_ret));
	if (ret < 0)
		ipio_err("Failed to copy data to user space\n");

	return ret;
}

static char tp_lockdown_info[128];

void ilitek_tp_lock_down_info(void)
{
	int i = 0;
	u8 reg[8] = {132, 133, 134, 135, 136, 137, 138, 139};
	u8 page = 13;
	u8 read_reg[8];
	char temp[40] = {0};

	 for (i = 0; i < 8; i++) {
		 ipio_debug("page = %c, reg = %u", page, reg[i]);
		 ilitek_tddi_ic_get_ddi_reg_onepage(page, reg[i]);
		 ipio_debug("reg_data = %u", idev->chip->read_reg_data);
		 read_reg[i] = idev->chip->read_reg_data;
	 }

	snprintf (temp, sizeof(g_user_buf), "%02x%02x%02x%02x%02x%02x%02x%02x",
												read_reg[0], read_reg[1],
												read_reg[2], read_reg[3],
												read_reg[4], read_reg[5],
												read_reg[6], read_reg[7]);
	ipio_debug("temp = %s", temp);
	strlcpy(tp_lockdown_info, temp, 20);

}

static int ilitek_lockdown_proc_show(struct seq_file *file, void *data)
{
	char temp[40] = {0};
	snprintf(temp, 20, "%s", tp_lockdown_info);
	seq_printf(file, "%s\n", temp);

	return 0;
}

static int ilitek_lockdown_proc_open (struct inode *inode, struct file *file)
{
	return single_open(file, ilitek_lockdown_proc_show, inode->i_private);
}

static ssize_t ilitek_proc_android_touch_mptest_read(struct file *filp, char __user *buff, size_t size, loff_t *pos)
{
	int ret = 0;
	char apk_ret[100] = {0};

	if (*pos != 0)
		return 0;

	idev->mp_test_result[RAW_DATA_NO_BK] = 'F';
	idev->mp_test_result[SHORT_TEST_ILI9881] = 'F';
	idev->mp_test_result[NOISE_PEAK_TO_PEAK_WITH_PANEL] = 'F';
	idev->mp_test_result[DOZE_PEAK_TO_PEAK] = 'F';
	idev->mp_test_result[OPEN_TEST_C] = 'F';

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	/* Create the directory for mp_test result */
	ret = dev_mkdir(CSV_LCM_ON_PATH, S_IRUGO | S_IWUSR);
	if (ret != 0)
		ipio_err("Failed to create directory for mp_test\n");

	ilitek_tddi_mp_test_handler(apk_ret, ON);

	size = snprintf(g_user_buf, sizeof(g_user_buf), "0%c-1%c-2%c-3%c-4%c\n",
					idev->mp_test_result[RAW_DATA_NO_BK],
					idev->mp_test_result[SHORT_TEST_ILI9881],
					idev->mp_test_result[NOISE_PEAK_TO_PEAK_WITH_PANEL],
					idev->mp_test_result[DOZE_PEAK_TO_PEAK],
					idev->mp_test_result[OPEN_TEST_C]);

	//size = snprintf(g_user_buf, sizeof(g_user_buf), "0P-1P-2P-3P-4P\n");
	*pos += size;

	ret = copy_to_user((u32 *) buff, g_user_buf, size);
	if (ret < 0)
		ipio_err("Failed to copy data to user space\n");

	return size;
}

static int ilitek_test_result;
static ssize_t ilitek_tp_selftest_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	int temp;
	char tmp[40];
	int ret;
	char apk_ret[100] = {0};

	if (copy_from_user(tmp, buf, count)) {
		ipio_err("copy from user failed!\n");
		ilitek_test_result = RESULT_INVALID;
	}

	if (!strncmp(tmp, "short", 5)) {
		temp = TEST_SHORT;
	} else if (!strncmp(tmp, "open", 4)) {
		temp = TEST_OPEN;
	} else if (!strncmp(tmp, "i2c", 3)) {
		temp = TEST_SPI;
	} else {
		temp = TEST_INVAILD;
	}
	ipio_info("selftest %d\n", temp);
	ret = ilitek_tddi_ic_get_spi_panel_info();
	if (ret < 0) {
		ipio_err("Read _panel info error\n");
		ilitek_test_result = RESULT_NG;
	} else {
		switch (temp) {
		case TEST_SPI:
			ilitek_test_result = RESULT_PASS;
			break;
		case TEST_SHORT:
			idev->mp_test_result[RAW_DATA_NO_BK] = 'F';
			idev->mp_test_result[SHORT_TEST_ILI9881] = 'F';
			idev->mp_test_result[NOISE_PEAK_TO_PEAK_WITH_PANEL] = 'F';
			idev->mp_test_result[DOZE_PEAK_TO_PEAK] = 'F';
			idev->mp_test_result[OPEN_TEST_C] = 'F';

			ret = dev_mkdir(CSV_LCM_ON_PATH, S_IRUGO | S_IWUSR);
			if (ret != 0)
				ipio_err("Failed to create directory for mp_test\n");

			ilitek_tddi_mp_test_handler(apk_ret, ON);

			if (idev->mp_test_result[SHORT_TEST_ILI9881] == 'P') {
				ilitek_test_result = RESULT_PASS;
			} else {
				ilitek_test_result = RESULT_NG;
			}

			break;
		case TEST_OPEN:
			idev->mp_test_result[RAW_DATA_NO_BK] = 'F';
			idev->mp_test_result[SHORT_TEST_ILI9881] = 'F';
			idev->mp_test_result[NOISE_PEAK_TO_PEAK_WITH_PANEL] = 'F';
			idev->mp_test_result[DOZE_PEAK_TO_PEAK] = 'F';
			idev->mp_test_result[OPEN_TEST_C] = 'F';

			ret = dev_mkdir(CSV_LCM_ON_PATH, S_IRUGO | S_IWUSR);
			if (ret != 0)
				ipio_err("Failed to create directory for mp_test\n");

			ilitek_tddi_mp_test_handler(apk_ret, ON);

			if (idev->mp_test_result[OPEN_TEST_C] == 'P') {
				ilitek_test_result = RESULT_PASS;
			} else {
				ilitek_test_result = RESULT_NG;
			}
			break;
		default:
			ilitek_test_result = RESULT_INVALID;
			ipio_err("Invalid teset case!\n");
			break;
		}
	}


	return count;
}
static ssize_t ilitek_tp_selftest_read(struct file *file, char __user *buff, size_t count, loff_t *pos)
{
	char tmp[5];
	int cnt;

	if (*pos != 0) {
		return 0;
	}

	cnt = snprintf(tmp, sizeof(ilitek_test_result), "%d\n", ilitek_test_result);

	if (copy_to_user((u32 *) buff, tmp, strlen(tmp))) {
		return -EFAULT;
	}

	*pos += cnt;
	return cnt;
}

static ssize_t ilitek_node_mp_lcm_off_test_read(struct file *filp, char __user *buff, size_t size, loff_t *pos)
{
	int ret = 0;
	char apk_ret[100] = {0};

	ipio_info("Run MP test with LCM off\n");

	if (*pos != 0)
		return 0;

	/* Create the directory for mp_test result */
	ret = dev_mkdir(CSV_LCM_OFF_PATH, S_IRUGO | S_IWUSR);
	if (ret != 0)
		ipio_err("Failed to create directory for mp_test\n");

	ilitek_tddi_mp_test_handler(apk_ret, OFF);

	ret = copy_to_user((char *)buff, apk_ret, sizeof(apk_ret));
	if (ret < 0)
		ipio_err("Failed to copy data to user space\n");

	return ret;
}

static ssize_t ilitek_proc_fw_process_read(struct file *filp, char __user *buff, size_t size, loff_t *pos)
{
	int ret = 0;
	u32 len = 0;

	if (*pos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	len = snprintf(g_user_buf, 50, "%02d\n", idev->fw_update_stat);

	ipio_info("update status = %d\n", idev->fw_update_stat);

	*pos += len;
	ret = copy_to_user((u32 *) buff, g_user_buf, len);
	if (ret < 0) {
		ipio_err("Failed to copy data to user space\n");
	}

	return len;
}

static ssize_t ilitek_node_fw_upgrade_read(struct file *filp, char __user *buff, size_t size, loff_t *pos)
{
	int ret = 0;
	u32 len = 0;

	ipio_info("Preparing to upgarde firmware\n");

	if (*pos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));
	mutex_lock(&idev->touch_mutex);
	idev->force_fw_update = ENABLE;
	ret = ilitek_tddi_fw_upgrade_handler(NULL);
	len = snprintf(g_user_buf, 50, "upgrade firwmare %s\n", (ret != 0) ? "failed" : "succeed");
	idev->force_fw_update = DISABLE;
	*pos += len;
	ret = copy_to_user((u32 *) buff, g_user_buf, len);
	if (ret < 0)
		ipio_err("Failed to copy data to user space\n");
	mutex_unlock(&idev->touch_mutex);

	return len;
}

static ssize_t ilitek_proc_debug_level_read(struct file *filp, char __user *buff, size_t size, loff_t *pos)
{
	int ret = 0;

	if (*pos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	ipio_debug_level = !ipio_debug_level;

	ipio_info(" %s debug level = %x\n", ipio_debug_level ? "Enable" : "Disable", ipio_debug_level);

	size = snprintf(g_user_buf, 50, "debug level : %s\n", ipio_debug_level ? "Enable" : "Disable");

	*pos += size;

	ret = copy_to_user((u32 *) buff, g_user_buf, size);
	if (ret < 0)
		ipio_err("Failed to copy data to user space\n");

	return size;
}

static ssize_t ilitek_node_ioctl_write(struct file *filp, const char *buff, size_t size, loff_t *pos)
{
	int i, ret = 0, count = 0;
	char cmd[512] = {0};
	char *token = NULL, *cur = NULL;
	u8 temp[256] = {0};
	u32 *data = NULL;
	u8 tp_mode;

	if (buff != NULL) {
		ret = copy_from_user(cmd, buff, size - 1);
		if (ret < 0) {
			ipio_info("copy data from user space, failed\n");
			return -EINVAL;
		}
	}

	ipio_info("size = %d, cmd = %s\n", (int)size, cmd);

	token = cur = cmd;

	data = kcalloc(512, sizeof(u32), GFP_KERNEL);

	while ((token = strsep(&cur, ",")) != NULL) {
		data[count] = str2hex(token);
		ipio_info("data[%d] = %x\n", count, data[count]);
		count++;
	}

	ipio_info("cmd = %s\n", cmd);

	mutex_lock(&idev->touch_mutex);

	if (strcmp(cmd, "hwreset") == 0) {
		ilitek_tddi_reset_ctrl(TP_HW_RST_ONLY);
	} else if (strcmp(cmd, "icwholereset") == 0) {
		ilitek_ice_mode_ctrl(ENABLE, OFF);
		ilitek_tddi_reset_ctrl(TP_IC_WHOLE_RST);
	} else if (strcmp(cmd, "iccodereset") == 0) {
		ilitek_ice_mode_ctrl(ENABLE, OFF);
		ilitek_tddi_reset_ctrl(TP_IC_CODE_RST);
		ilitek_ice_mode_ctrl(DISABLE, OFF);
	} else if (strcmp(cmd, "getinfo") == 0) {
		ilitek_ice_mode_ctrl(ENABLE, OFF);
		ilitek_tddi_ic_get_info();
		ilitek_ice_mode_ctrl(DISABLE, OFF);
		ilitek_tddi_ic_get_protocl_ver();
		ilitek_tddi_ic_get_fw_ver();
		ilitek_tddi_ic_get_core_ver();
		ilitek_tddi_ic_get_tp_info();
		ilitek_tddi_ic_get_panel_info();
		ipio_info("Driver version = %s\n", DRIVER_VERSION);
	} else if (strcmp(cmd, "enableicemode") == 0) {
		if (data[1] == ON)
			ilitek_ice_mode_ctrl(ENABLE, ON);
		else
			ilitek_ice_mode_ctrl(ENABLE, OFF);
	} else if (strcmp(cmd, "disableicemode") == 0) {
		ilitek_ice_mode_ctrl(DISABLE, OFF);
	} else if (strcmp(cmd, "enablewqesd") == 0) {
		ilitek_tddi_wq_ctrl(WQ_ESD, ENABLE);
	} else if (strcmp(cmd, "enablewqbat") == 0) {
		ilitek_tddi_wq_ctrl(WQ_BAT, ENABLE);
	} else if (strcmp(cmd, "disablewqesd") == 0) {
		ilitek_tddi_wq_ctrl(WQ_ESD, DISABLE);
	} else if (strcmp(cmd, "disablewqbat") == 0) {
		ilitek_tddi_wq_ctrl(WQ_BAT, DISABLE);
	} else if (strcmp(cmd, "gesture") == 0) {
		idev->gesture = !idev->gesture;
		ilitek_gesture_flag = !ilitek_gesture_flag;
		ipio_info("gesture = %d\n", idev->gesture);
	} else if (strcmp(cmd, "esdgesture") == 0) {
		ilitek_tddi_gesture_recovery();
	} else if (strcmp(cmd, "esdspi") == 0) {
		ilitek_tddi_spi_recovery();
	} else if (strcmp(cmd, "sleepin") == 0) {
		ilitek_tddi_ic_func_ctrl("sleep", SLEEP_IN);
	} else if (strcmp(cmd, "deepsleepin") == 0) {
		ilitek_tddi_ic_func_ctrl("sleep", DEEP_SLEEP_IN);
	} else if (strcmp(cmd, "iceflag") == 0) {
		if (data[1] == ENABLE)
			atomic_set(&idev->ice_stat, ENABLE);
		else
			atomic_set(&idev->ice_stat, DISABLE);
		ipio_info("ice mode flag = %d\n", atomic_read(&idev->ice_stat));
	} else if (strcmp(cmd, "gesturenormal") == 0) {
		idev->gesture_mode = P5_X_FW_GESTURE_NORMAL_MODE;
		ipio_info("gesture mode = %d\n", idev->gesture_mode);
	} else if (strcmp(cmd, "gestureinfo") == 0) {
		idev->gesture_mode = P5_X_FW_GESTURE_INFO_MODE;
		ipio_info("gesture mode = %d\n", idev->gesture_mode);
	} else if (strcmp(cmd, "netlink") == 0) {
		idev->netlink = !idev->netlink;
		ipio_info("netlink flag= %d\n", idev->netlink);
	} else if (strcmp(cmd, "switchtestmode") == 0) {
		tp_mode = P5_X_FW_TEST_MODE;
		ilitek_tddi_switch_mode(&tp_mode);
	} else if (strcmp(cmd, "switchdebugmode") == 0) {
		tp_mode = P5_X_FW_DEBUG_MODE;
		ilitek_tddi_switch_mode(&tp_mode);
	} else if (strcmp(cmd, "switchdemomode") == 0) {
		tp_mode = P5_X_FW_DEMO_MODE;
		ilitek_tddi_switch_mode(&tp_mode);
	} else if (strcmp(cmd, "dbgflag") == 0) {
		//debug_mode_switch();
		idev->debug_node_open = !idev->debug_node_open;
		ipio_info("idev->debug_node_open = %d", idev->debug_node_open);
		//ipio_info("debug flag message = %d\n", idev->debug_node_open);
	} else if (strcmp(cmd, "iow") == 0) {
		int w_len = 0;
		w_len = data[1];
		ipio_info("w_len = %d\n", w_len);

		for (i = 0; i < w_len; i++) {
			temp[i] = data[2 + i];
			ipio_info("write[%d] = %x\n", i, temp[i]);
		}

		idev->write(temp, w_len);
	} else if (strcmp(cmd, "ior") == 0) {
		int r_len = 0;
		r_len = data[1];
		ipio_info("r_len = %d\n", r_len);
		idev->read(temp, r_len);
		for (i = 0; i < r_len; i++)
			ipio_info("read[%d] = %x\n", i, temp[i]);
	} else if (strcmp(cmd, "iowr") == 0) {
		int delay = 0;
		int w_len = 0, r_len = 0;
		w_len = data[1];
		r_len = data[2];
		delay = data[3];
		ipio_info("w_len = %d, r_len = %d, delay = %d\n", w_len, r_len, delay);

		for (i = 0; i < w_len; i++) {
			temp[i] = data[4 + i];
			ipio_info("write[%d] = %x\n", i, temp[i]);
		}
		idev->write(temp, w_len);
		memset(temp, 0, sizeof(temp));
		mdelay(delay);
		idev->read(temp, r_len);

		for (i = 0; i < r_len; i++)
			ipio_info("read[%d] = %x\n", i, temp[i]);
	} else if (strcmp(cmd, "getddiregdata") == 0) {
		ipio_info("Get ddi reg one page: page = %x, reg = %x\n", data[1], data[2]);
		ilitek_tddi_ic_get_ddi_reg_onepage(data[1], data[2]);
	} else if (strcmp(cmd, "setddiregdata") == 0) {
		ipio_info("Set ddi reg one page: page = %x, reg = %x, data = %x\n", data[1], data[2], data[3]);
		ilitek_tddi_ic_set_ddi_reg_onepage(data[1], data[2], data[3]);
	} else if (strcmp(cmd, "dumpflashdata") == 0) {
		ipio_info("Start = 0x%x, End = 0x%x, Dump Hex path = %s\n", data[1], data[2], DUMP_FLASH_PATH);
		ilitek_tddi_fw_dump_flash_data(data[1], data[2], false);
	} else if (strcmp(cmd, "dumpiramdata") == 0) {
		ipio_info("Start = 0x%x, End = 0x%x, Dump IRAM path = %s\n", data[1], data[2], DUMP_IRAM_PATH);
		ilitek_tddi_fw_dump_iram_data(data[1], data[2]);
	} else if (strcmp(cmd, "edge_palm_ctrl") == 0) {
		ilitek_tddi_ic_func_ctrl("edge_palm", data[1]);
	} else if (strcmp(cmd, "uart_mode_ctrl") == 0) {
		if (data[1] > 1) {
			ipio_info("Unknow cmd, Disable UART mdoe\n");
			data[1] = 0;
		} else {
			ipio_info("UART mode %s\n", data[1] ? "Enable" : "Disable");
		}
		temp[0] = P5_X_I2C_UART;
		temp[1] = 0x3;
		temp[2] = 0;
		temp[3] = data[1];
		idev->write(temp, 4);

		idev->fw_uart_en = data[1] ? ENABLE : DISABLE;
	} else if (strcmp(cmd, "printiram") == 0) {
		ipio_err("print iram 64k bytes for debug start \n");
		ilitek_tddi_reset_ctrl(idev->reset);
		ilitek_ice_mode_ctrl(ENABLE, OFF);
		ilitek_tddi_ic_watch_dog_ctrl(ILI_WRITE, DISABLE);
		ilitek_tddi_fw_print_iram_data();
		ilitek_tddi_reset_ctrl(TP_IC_CODE_RST);
		ilitek_ice_mode_ctrl(DISABLE, OFF);
		ipio_err("print iram 64k bytes for debug end \n");
	} else {
		ipio_err("Unknown command\n");
	}

	ipio_kfree((void **)&data);
	mutex_unlock(&idev->touch_mutex);
	return size;
}

#ifdef CONFIG_COMPAT
static long ilitek_node_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		ipio_err("There's no unlocked_ioctl defined in file\n");
		return -ENOTTY;
	}

	ipio_info("cmd = %d\n", _IOC_NR(cmd));

	switch (cmd) {
	case ILITEK_COMPAT_IOCTL_I2C_WRITE_DATA:
		ipio_info("compat_ioctl: convert i2c/spi write\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_I2C_WRITE_DATA, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_I2C_READ_DATA:
		ipio_info("compat_ioctl: convert i2c/spi read\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_I2C_READ_DATA, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_I2C_SET_WRITE_LENGTH:
		ipio_info("compat_ioctl: convert set write length\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_I2C_SET_WRITE_LENGTH, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_I2C_SET_READ_LENGTH:
		ipio_info("compat_ioctl: convert set read length\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_I2C_SET_READ_LENGTH, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_HW_RESET:
		ipio_info("compat_ioctl: convert hw reset\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_HW_RESET, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_POWER_SWITCH:
		ipio_info("compat_ioctl: convert power switch\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_POWER_SWITCH, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_REPORT_SWITCH:
		ipio_info("compat_ioctl: convert report switch\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_REPORT_SWITCH, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_IRQ_SWITCH:
		ipio_info("compat_ioctl: convert irq switch\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_IRQ_SWITCH, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_DEBUG_LEVEL:
		ipio_info("compat_ioctl: convert debug level\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_DEBUG_LEVEL, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_FUNC_MODE:
		ipio_info("compat_ioctl: convert function mode\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_FUNC_MODE, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_FW_VER:
		ipio_info("compat_ioctl: convert set read length\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_FW_VER, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_PL_VER:
		ipio_info("compat_ioctl: convert fw version\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_PL_VER, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_CORE_VER:
		ipio_info("compat_ioctl: convert core version\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_CORE_VER, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_DRV_VER:
		ipio_info("compat_ioctl: convert driver version\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_DRV_VER, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_CHIP_ID:
		ipio_info("compat_ioctl: convert chip id\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_CHIP_ID, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_NETLINK_CTRL:
		ipio_info("compat_ioctl: convert netlink ctrl\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_NETLINK_CTRL, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_NETLINK_STATUS:
		ipio_info("compat_ioctl: convert netlink status\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_NETLINK_STATUS, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_MODE_CTRL:
		ipio_info("compat_ioctl: convert tp mode ctrl\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_MODE_CTRL, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_MODE_STATUS:
		ipio_info("compat_ioctl: convert tp mode status\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_MODE_STATUS, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_ICE_MODE_SWITCH:
		ipio_info("compat_ioctl: convert tp mode switch\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_ICE_MODE_SWITCH, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_INTERFACE_TYPE:
		ipio_info("compat_ioctl: convert interface type\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_INTERFACE_TYPE, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_DUMP_FLASH:
		ipio_info("compat_ioctl: convert dump flash\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_DUMP_FLASH, (unsigned long)compat_ptr(arg));
		return ret;
	default:
		ipio_err("no ioctl cmd, return ilitek_node_ioctl\n");
		return -ENOIOCTLCMD;
	}
}
#endif

static long ilitek_node_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0, length = 0;
	u8 *szBuf = NULL, if_to_user = 0;
	static u16 i2c_rw_length;
	u32 id_to_user[3] = {0};
	char dbg[10] = { 0 };

	if (_IOC_TYPE(cmd) != ILITEK_IOCTL_MAGIC) {
		ipio_err("The Magic number doesn't match\n");
		return -ENOTTY;
	}

	if (_IOC_NR(cmd) > ILITEK_IOCTL_MAXNR) {
		ipio_err("The number of ioctl doesn't match\n");
		return -ENOTTY;
	}

	ipio_info("cmd = %d\n", _IOC_NR(cmd));

	szBuf = kcalloc(IOCTL_I2C_BUFF, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(szBuf)) {
		ipio_err("Failed to allocate mem\n");
		return -ENOMEM;
	}

	mutex_lock(&idev->touch_mutex);

	switch (cmd) {
	case ILITEK_IOCTL_I2C_WRITE_DATA:
		ipio_info("ioctl: write len = %d\n", i2c_rw_length);
		if (i2c_rw_length > IOCTL_I2C_BUFF) {
			ipio_err("ERROR! write len is largn than ioctl buf (%d, %ld)\n",
					i2c_rw_length, IOCTL_I2C_BUFF);
			ret = -ENOTTY;
			break;
		}
		ret = copy_from_user(szBuf, (u8 *) arg, i2c_rw_length);
		if (ret < 0) {
			ipio_err("Failed to copy data from user space\n");
			break;
		}
		ret = idev->write(&szBuf[0], i2c_rw_length);
		if (ret < 0)
			ipio_err("Failed to write data\n");
		break;
	case ILITEK_IOCTL_I2C_READ_DATA:
		ipio_info("ioctl: read len = %d\n", i2c_rw_length);
		if (i2c_rw_length > IOCTL_I2C_BUFF) {
			ipio_err("ERROR! read len is largn than ioctl buf (%d, %ld)\n",
					i2c_rw_length, IOCTL_I2C_BUFF);
			ret = -ENOTTY;
			break;
		}
		ret = idev->read(szBuf, i2c_rw_length);
		if (ret < 0) {
			ipio_err("Failed to read data\n");
			break;
		}
		ret = copy_to_user((u8 *) arg, szBuf, i2c_rw_length);
		if (ret < 0)
			ipio_err("Failed to copy data to user space\n");
		break;
	case ILITEK_IOCTL_I2C_SET_WRITE_LENGTH:
	case ILITEK_IOCTL_I2C_SET_READ_LENGTH:
		i2c_rw_length = arg;
		break;
	case ILITEK_IOCTL_TP_HW_RESET:
		ipio_info("ioctl: hw reset\n");
		ilitek_tddi_reset_ctrl(idev->reset);
		break;
	case ILITEK_IOCTL_TP_POWER_SWITCH:
		ipio_info("Not implemented yet\n");
		break;
	case ILITEK_IOCTL_TP_REPORT_SWITCH:
		ret = copy_from_user(szBuf, (u8 *) arg, 1);
		if (ret < 0) {
			ipio_err("Failed to copy data from user space\n");
			break;
		}
		ipio_info("ioctl: report switch = %d\n", szBuf[0]);
		if (szBuf[0]) {
			idev->report = ENABLE;
			ipio_info("report is enabled\n");
		} else {
			idev->report = DISABLE;
			ipio_info("report is disabled\n");
		}
		break;
	case ILITEK_IOCTL_TP_IRQ_SWITCH:
		ret = copy_from_user(szBuf, (u8 *) arg, 1);
		if (ret < 0) {
			ipio_err("Failed to copy data from user space\n");
			break;
		}
		ipio_info("ioctl: irq switch = %d\n", szBuf[0]);
		if (szBuf[0])
			ilitek_plat_irq_enable();
		else
			ilitek_plat_irq_disable();
		break;
	case ILITEK_IOCTL_TP_DEBUG_LEVEL:
		ret = copy_from_user(dbg, (u32 *) arg, sizeof(u32));
		if (ret < 0) {
			ipio_err("Failed to copy data from user space\n");
			break;
		}
		ipio_debug_level = !ipio_debug_level;
		ipio_info("ipio_debug_level = %d", ipio_debug_level);
		break;
	case ILITEK_IOCTL_TP_FUNC_MODE:
		ret = copy_from_user(szBuf, (u8 *) arg, 3);
		if (ret < 0) {
			ipio_err("Failed to copy data from user space\n");
			break;
		}
		ipio_info("ioctl: set func mode = %x,%x,%x\n", szBuf[0], szBuf[1], szBuf[2]);
		idev->write(&szBuf[0], 3);
		break;
	case ILITEK_IOCTL_TP_FW_VER:
		ipio_info("ioctl: get fw version\n");
		ret = ilitek_tddi_ic_get_fw_ver();
		if (ret < 0) {
			ipio_err("Failed to get firmware version\n");
			break;
		}
		szBuf[3] = idev->chip->fw_ver & 0xFF;
		szBuf[2] = (idev->chip->fw_ver >> 8) & 0xFF;
		szBuf[1] = (idev->chip->fw_ver >> 16) & 0xFF;
		szBuf[0] = idev->chip->fw_ver >> 24;
		ipio_info("Firmware version = %d.%d.%d.%d\n", szBuf[0], szBuf[1], szBuf[2], szBuf[3]);
		ret = copy_to_user((u8 *) arg, szBuf, 4);
		if (ret < 0)
			ipio_err("Failed to copy firmware version to user space\n");
		break;
	case ILITEK_IOCTL_TP_PL_VER:
		ipio_info("ioctl: get protocl version\n");
		ret = ilitek_tddi_ic_get_protocl_ver();
		if (ret < 0) {
			ipio_err("Failed to get protocol version\n");
			break;
		}
		szBuf[2] = idev->protocol->ver & 0xFF;
		szBuf[1] = (idev->protocol->ver >> 8) & 0xFF;
		szBuf[0] = idev->protocol->ver >> 16;
		ipio_info("Protocol version = %d.%d.%d\n", szBuf[0], szBuf[1], szBuf[2]);
		ret = copy_to_user((u8 *) arg, szBuf, 3);
		if (ret < 0)
			ipio_err("Failed to copy protocol version to user space\n");
		break;
	case ILITEK_IOCTL_TP_CORE_VER:
		ipio_info("ioctl: get core version\n");
		ret = ilitek_tddi_ic_get_core_ver();
		if (ret < 0) {
			ipio_err("Failed to get core version\n");
			break;
		}
		szBuf[3] = idev->chip->core_ver & 0xFF;
		szBuf[2] = (idev->chip->core_ver >> 8) & 0xFF;
		szBuf[1] = (idev->chip->core_ver >> 16) & 0xFF;
		szBuf[0] = idev->chip->core_ver >> 24;
		ipio_info("Core version = %d.%d.%d.%d\n", szBuf[0], szBuf[1], szBuf[2], szBuf[3]);
		ret = copy_to_user((u8 *) arg, szBuf, 4);
		if (ret < 0)
			ipio_err("Failed to copy core version to user space\n");
		break;
	case ILITEK_IOCTL_TP_DRV_VER:
		ipio_info("ioctl: get driver version\n");
		length = snprintf(szBuf, 50, "%s", DRIVER_VERSION);
		ret = copy_to_user((u8 *) arg, szBuf, length);
		if (ret < 0) {
			ipio_err("Failed to copy driver ver to user space\n");
		}
		break;
	case ILITEK_IOCTL_TP_CHIP_ID:
		ipio_info("ioctl: get chip id\n");
		ilitek_ice_mode_ctrl(ENABLE, OFF);
		ret = ilitek_tddi_ic_get_info();
		if (ret < 0) {
			ipio_err("Failed to get chip id\n");
			break;
		}
		id_to_user[0] = idev->chip->pid;
		id_to_user[1] = idev->chip->otp_id;
		id_to_user[2] = idev->chip->ana_id;
		ret = copy_to_user((u32 *) arg, id_to_user, sizeof(id_to_user));
		if (ret < 0)
			ipio_err("Failed to copy chip id to user space\n");
		ilitek_ice_mode_ctrl(DISABLE, OFF);
		break;
	case ILITEK_IOCTL_TP_NETLINK_CTRL:
		ret = copy_from_user(szBuf, (u8 *) arg, 1);
		if (ret < 0) {
			ipio_err("Failed to copy data from user space\n");
			break;
		}
		ipio_info("ioctl: netlink ctrl = %d\n", szBuf[0]);
		if (szBuf[0]) {
			idev->netlink = ENABLE;
			ipio_info("ioctl: Netlink is enabled\n");
		} else {
			idev->netlink = DISABLE;
			ipio_info("ioctl: Netlink is disabled\n");
		}
		break;
	case ILITEK_IOCTL_TP_NETLINK_STATUS:
		ipio_info("ioctl: get netlink stat = %d\n", idev->netlink);
		ret = copy_to_user((int *)arg, &idev->netlink, sizeof(int));
		if (ret < 0)
			ipio_err("Failed to copy chip id to user space\n");
		break;
	case ILITEK_IOCTL_TP_MODE_CTRL:
		ret = copy_from_user(szBuf, (u8 *) arg, 4);
		if (ret < 0) {
			ipio_err("Failed to copy data from user space\n");
			break;
		}
		ipio_info("ioctl: switch fw mode = %d\n", szBuf[0]);
		ret = ilitek_tddi_switch_mode(szBuf);
		if (ret < 0) {
			ipio_info("switch to fw mode (%d) failed\n", szBuf[0]);
		}
		break;
	case ILITEK_IOCTL_TP_MODE_STATUS:
		ipio_info("ioctl: current firmware mode = %d", idev->actual_tp_mode);
		ret = copy_to_user((int *)arg, &idev->actual_tp_mode, sizeof(int));
		if (ret < 0)
			ipio_err("Failed to copy chip id to user space\n");
		break;
	/* It works for host downloado only */
	case ILITEK_IOCTL_ICE_MODE_SWITCH:
		ret = copy_from_user(szBuf, (u8 *) arg, 1);
		if (ret < 0) {
			ipio_err("Failed to copy data from user space\n");
			break;
		}
		ipio_info("ioctl: switch ice mode = %d", szBuf[0]);
		if (szBuf[0]) {
			atomic_set(&idev->ice_stat, ENABLE);
			ipio_info("ioctl: set ice mode enabled\n");
		} else {
			atomic_set(&idev->ice_stat, DISABLE);
			ipio_info("ioctl: set ice mode disabled\n");
		}
		break;
	case ILITEK_IOCTL_TP_INTERFACE_TYPE:
		if_to_user = idev->hwif->bus_type;
		ret = copy_to_user((u8 *) arg, &if_to_user, sizeof(if_to_user));
		if (ret < 0) {
			ipio_err("Failed to copy interface type to user space\n");
		}
		break;
	case ILITEK_IOCTL_TP_DUMP_FLASH:
		ipio_info("ioctl: dump flash data\n");
		ret = ilitek_tddi_fw_dump_flash_data(0, 0, true);
		if (ret < 0) {
			ipio_err("ioctl: Failed to dump flash data\n");
		}
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	ipio_kfree((void **)&szBuf);
	mutex_unlock(&idev->touch_mutex);
	return ret;
}

struct proc_dir_entry *proc_dir_ilitek;
struct proc_dir_entry *proc_android_touch_proc;
struct proc_dir_entry *proc_mp_test_node;
struct proc_dir_entry *proc_create_tp_lock_down;
struct proc_dir_entry *proc_create_tp_selftest;

typedef struct {
	char *name;
	struct proc_dir_entry *node;
	struct file_operations *fops;
	bool isCreated;
} proc_node_t;

struct file_operations proc_mp_lcm_on_test_fops = {
	.read = ilitek_node_mp_lcm_on_test_read,
};

struct file_operations proc_mp_lcm_off_test_fops = {
	.read = ilitek_node_mp_lcm_off_test_read,
};

struct file_operations proc_debug_message_fops = {
	.read = ilitek_proc_debug_message_read,
};

struct file_operations proc_debug_message_switch_fops = {
	.read = ilitek_proc_debug_switch_read,
};

struct file_operations proc_ioctl_fops = {
	.unlocked_ioctl = ilitek_node_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ilitek_node_compat_ioctl,
#endif
	.write = ilitek_node_ioctl_write,
};

struct file_operations proc_tp_lock_down_info_fops = {
	.owner = THIS_MODULE,
	.open = ilitek_lockdown_proc_open,
	.read = seq_read,
};

struct file_operations proc_fw_upgrade_fops = {
	.read = ilitek_node_fw_upgrade_read,
};

struct file_operations proc_fw_process_fops = {
	.read = ilitek_proc_fw_process_read,
};

struct file_operations proc_get_delta_data_fops = {
	.read = ilitek_proc_get_delta_data_read,
};

struct file_operations proc_get_raw_data_fops = {
	.read = ilitek_proc_fw_get_raw_data_read,
};

struct file_operations proc_rw_tp_reg_fops = {
	.read = ilitek_proc_rw_tp_reg_read,
	.write = ilitek_proc_rw_tp_reg_write,
};

struct file_operations proc_fw_pc_counter_fops = {
	.read = ilitek_proc_fw_pc_counter_read,
};

struct file_operations proc_get_debug_mode_data_fops = {
	.read = ilitek_proc_get_debug_mode_data_read,
	.write = ilitek_proc_get_debug_mode_data_write,
};

struct file_operations proc_debug_level_fops = {
	.read = ilitek_proc_debug_level_read,
};

struct file_operations proc_android_touch_mptest_fops = {
	.read = ilitek_proc_android_touch_mptest_read,
};

struct file_operations proc_tp_selftest_fops = {
	.write = ilitek_tp_selftest_write,
	.read = ilitek_tp_selftest_read,
};

proc_node_t proc_table[] = {
	{"ioctl", NULL, &proc_ioctl_fops, false},
	{"fw_process", NULL, &proc_fw_process_fops, false},
	{"fw_upgrade", NULL, &proc_fw_upgrade_fops, false},
	{"debug_level", NULL, &proc_debug_level_fops, false},
	{"mp_lcm_on_test", NULL, &proc_mp_lcm_on_test_fops, false},
	{"mp_lcm_off_test", NULL, &proc_mp_lcm_off_test_fops, false},
	{"debug_message", NULL, &proc_debug_message_fops, false},
	{"debug_message_switch", NULL, &proc_debug_message_switch_fops, false},
	{"fw_pc_counter", NULL, &proc_fw_pc_counter_fops, false},
	{"show_delta_data", NULL, &proc_get_delta_data_fops, false},
	{"show_raw_data", NULL, &proc_get_raw_data_fops, false},
	{"get_debug_mode_data", NULL, &proc_get_debug_mode_data_fops, false},
	{"rw_tp_reg", NULL, &proc_rw_tp_reg_fops, false},
};

#define NETLINK_USER 21
struct sock *netlink_skb;
struct nlmsghdr *netlink_head;
struct sk_buff *skb_out;
int netlink_pid;

void netlink_reply_msg(void *raw, int size)
{
	int ret;
	int msg_size = size;
	u8 *data = (u8 *) raw;

	ipio_info("The size of data being sent to user = %d\n", msg_size);
	ipio_info("pid = %d\n", netlink_pid);
	ipio_info("Netlink is enable = %d\n", idev->netlink);

	if (idev->netlink) {
		skb_out = nlmsg_new(msg_size, 0);

		if (!skb_out) {
			ipio_err("Failed to allocate new skb\n");
			return;
		}

		netlink_head = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
		NETLINK_CB(skb_out).dst_group = 0;	/* not in mcast group */

		/* strncpy(NLMSG_DATA(netlink_head), data, msg_size); */
		ipio_memcpy(nlmsg_data(netlink_head), data, msg_size, size);

		ret = nlmsg_unicast(netlink_skb, skb_out, netlink_pid);
		if (ret < 0)
			ipio_err("Failed to send data back to user\n");
	}
}

static void netlink_recv_msg(struct sk_buff *skb)
{
	netlink_pid = 0;

	ipio_info("Netlink = %d\n", idev->netlink);

	netlink_head = (struct nlmsghdr *)skb->data;

	ipio_info("Received a request from client: %s, %d\n",
		(char *)NLMSG_DATA(netlink_head), (int)strlen((char *)NLMSG_DATA(netlink_head)));

	/* pid of sending process */
	netlink_pid = netlink_head->nlmsg_pid;

	ipio_info("the pid of sending process = %d\n", netlink_pid);

	/* TODO: may do something if there's not receiving msg from user. */
	if (netlink_pid != 0) {
		ipio_err("The channel of Netlink has been established successfully !\n");
		idev->netlink = ENABLE;
	} else {
		ipio_err("Failed to establish the channel between kernel and user space\n");
		idev->netlink = DISABLE;
	}
}

static int netlink_init(void)
{
	int ret = 0;

#if KERNEL_VERSION(3, 4, 0) > LINUX_VERSION_CODE
	netlink_skb = netlink_kernel_create(&init_net, NETLINK_USER, netlink_recv_msg, NULL, THIS_MODULE);
#else
	struct netlink_kernel_cfg cfg = {
		.input = netlink_recv_msg,
	};

	netlink_skb = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
#endif

	ipio_info("Initialise Netlink and create its socket\n");

	if (!netlink_skb) {
		ipio_err("Failed to create nelink socket\n");
		ret = -EFAULT;
	}
	return ret;
}

void ilitek_tddi_node_lock_down_info(void)
{
	ilitek_tp_lock_down_info();
	proc_create_tp_lock_down = proc_create("tp_lockdown_info", 0644, NULL, &proc_tp_lock_down_info_fops);
	if (!proc_create_tp_lock_down) {
		ipio_err("Failed to create ilitek_tp_lockdown_info\n");
	} else {
		ipio_info("Sucess to creat ilitek_tp_lockdown_info /proc");
	}
}

void ilitek_mp_test(void)
{
	proc_android_touch_proc = proc_mkdir("android_touch", NULL);

	proc_mp_test_node = proc_create("self_test", 0664, proc_android_touch_proc, &proc_android_touch_mptest_fops);
	if (proc_mp_test_node == NULL) {
		ipio_err("Failed to create ilitek_mptest failed");
	} else {
		ipio_info("Succeed to create ilitek_mptest success /proc");
	}
}

void ilitek_tp_selftest(void)
{
	proc_create_tp_selftest = proc_create("tp_selftest", 0664, NULL, &proc_tp_selftest_fops);
	if (proc_create_tp_selftest == NULL) {
		ipio_err("Failed to create ilitek_mptest failed");
	} else {
		ipio_info("Succeed to create ilitek_mptest success /proc");
	}
}

void ilitek_tddi_node_init(void)
{
	int i = 0, ret = 0;

	ilitek_tddi_node_lock_down_info();
	ilitek_mp_test();
	ilitek_tp_selftest();

	proc_dir_ilitek = proc_mkdir("ilitek", NULL);

	for (; i < ARRAY_SIZE(proc_table); i++) {
		proc_table[i].node = proc_create(proc_table[i].name, 0644, proc_dir_ilitek, proc_table[i].fops);

		if (proc_table[i].node == NULL) {
			proc_table[i].isCreated = false;
			ipio_err("Failed to create %s under /proc\n", proc_table[i].name);
			ret = -ENODEV;
		} else {
			proc_table[i].isCreated = true;
			ipio_info("Succeed to create %s under /proc\n", proc_table[i].name);
		}
	}

	netlink_init();
}
