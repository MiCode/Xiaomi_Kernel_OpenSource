/* Copyright (c) 2012-2014, 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/**
 * SPMI Debug-fs support.
 *
 * Hierarchy schema:
 * /sys/kernel/debug/spmi
 *        /help			-- static help text
 *        /spmi-0
 *        /spmi-0/address	-- Starting register address for reads or writes
 *        /spmi-0/count		-- number of registers to read (only on read)
 *        /spmi-0/data		-- Triggers the SPMI formatted read.
 *        /spmi-0/data_raw	-- Triggers the SPMI raw read or write
 *        /spmi-#
 */

#define pr_fmt(fmt) "%s:%d: " fmt, __func__, __LINE__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/debugfs.h>
#include <linux/spmi.h>
#include <linux/ctype.h>
#include "spmi-dbgfs.h"

#define ADDR_LEN	 6	/* 5 byte address + 1 space character */
#define CHARS_PER_ITEM   3	/* Format is 'XX ' */
#define ITEMS_PER_LINE	16	/* 16 data items per line */
#define MAX_LINE_LENGTH  (ADDR_LEN + (ITEMS_PER_LINE * CHARS_PER_ITEM) + 1)
#define MAX_REG_PER_TRANSACTION	(8)

static const char *DFS_ROOT_NAME	= "spmi";
static const mode_t DFS_MODE = S_IRUSR | S_IWUSR;

/* Log buffer */
struct spmi_log_buffer {
	size_t rpos;	/* Current 'read' position in buffer */
	size_t wpos;	/* Current 'write' position in buffer */
	size_t len;	/* Length of the buffer */
	char data[0];	/* Log buffer */
};

/* SPMI controller specific data */
struct spmi_ctrl_data {
	u32 cnt;
	u32 addr;
	struct dentry *dir;
	struct list_head node;
	struct spmi_controller *ctrl;
};

/* SPMI transaction parameters */
struct spmi_trans {
	u32 cnt;	/* Number of bytes to read */
	u32 addr;	/* 20-bit address: SID + PID + Register offset */
	u32 offset;	/* Offset of last read data */
	bool raw_data;	/* Set to true for raw data dump */
	struct mutex spmi_dfs_lock; /* Prevent thread concurrency */
	struct spmi_controller *ctrl;
	struct spmi_log_buffer *log; /* log buffer */
};

struct spmi_dbgfs {
	struct dentry *root;
	struct mutex  lock;
	struct list_head ctrl; /* List of spmi_ctrl_data nodes */
	struct debugfs_blob_wrapper help_msg;
};

static struct spmi_dbgfs dbgfs_data = {
	.lock = __MUTEX_INITIALIZER(dbgfs_data.lock),
	.ctrl = LIST_HEAD_INIT(dbgfs_data.ctrl),
	.help_msg = {
	.data =
"SPMI Debug-FS support\n"
"\n"
"Hierarchy schema:\n"
"/sys/kernel/debug/spmi\n"
"       /help            -- Static help text\n"
"       /spmi-0          -- Directory for SPMI bus 0\n"
"       /spmi-0/address  -- Starting register address for reads or writes\n"
"       /spmi-0/count    -- Number of registers to read (only used for reads)\n"
"       /spmi-0/data     -- Initiates the SPMI read (formatted output)\n"
"       /spmi-0/data_raw -- Initiates the SPMI raw read or write\n"
"       /spmi-n          -- Directory for SPMI bus n\n"
"\n"
"To perform SPMI read or write transactions, you need to first write the\n"
"address of the slave device register to the 'address' file.  For read\n"
"transactions, the number of bytes to be read needs to be written to the\n"
"'count' file.\n"
"\n"
"The 'address' file specifies the 20-bit address of a slave device register.\n"
"The upper 4 bits 'address[19..16]' specify the slave identifier (SID) for\n"
"the slave device.  The lower 16 bits specify the slave register address.\n"
"\n"
"Reading from the 'data' file will initiate a SPMI read transaction starting\n"
"from slave register 'address' for 'count' number of bytes.\n"
"\n"
"Writing to the 'data' file will initiate a SPMI write transaction starting\n"
"from slave register 'address'.  The number of registers written to will\n"
"match the number of bytes written to the 'data' file.\n"
"\n"
"Example: Read 4 bytes starting at register address 0x1234 for SID 2\n"
"\n"
"echo 0x21234 > address\n"
"echo 4 > count\n"
"cat data\n"
"\n"
"Example: Write 3 bytes starting at register address 0x1008 for SID 1\n"
"\n"
"echo 0x11008 > address\n"
"echo 0x01 0x02 0x03 > data\n"
"\n"
"Note that the count file is not used for writes.  Since 3 bytes are\n"
"written to the 'data' file, then 3 bytes will be written across the\n"
"SPMI bus.\n\n",
	},
};

static int spmi_dfs_open(struct spmi_ctrl_data *ctrl_data, struct file *file)
{
	struct spmi_log_buffer *log;
	struct spmi_trans *trans;

	size_t logbufsize = SZ_4K;

	if (!ctrl_data) {
		pr_err("No SPMI controller data\n");
		return -EINVAL;
	}

	/* Per file "transaction" data */
	trans = kzalloc(sizeof(*trans), GFP_KERNEL);

	if (!trans) {
		pr_err("Unable to allocate memory for transaction data\n");
		return -ENOMEM;
	}

	/* Allocate log buffer */
	log = kzalloc(logbufsize, GFP_KERNEL);

	if (!log) {
		kfree(trans);
		pr_err("Unable to allocate memory for log buffer\n");
		return -ENOMEM;
	}

	log->rpos = 0;
	log->wpos = 0;
	log->len = logbufsize - sizeof(*log);

	trans->log = log;
	trans->cnt = ctrl_data->cnt;
	trans->addr = ctrl_data->addr;
	trans->ctrl = ctrl_data->ctrl;
	trans->offset = trans->addr;
	mutex_init(&trans->spmi_dfs_lock);

	file->private_data = trans;
	return 0;
}

static int spmi_dfs_data_open(struct inode *inode, struct file *file)
{
	struct spmi_ctrl_data *ctrl_data = inode->i_private;
	return spmi_dfs_open(ctrl_data, file);
}

static int spmi_dfs_raw_data_open(struct inode *inode, struct file *file)
{
	int rc;
	struct spmi_trans *trans;
	struct spmi_ctrl_data *ctrl_data = inode->i_private;

	rc = spmi_dfs_open(ctrl_data, file);
	trans = file->private_data;
	trans->raw_data = true;
	return rc;
}

static int spmi_dfs_close(struct inode *inode, struct file *file)
{
	struct spmi_trans *trans = file->private_data;

	if (trans && trans->log) {
		file->private_data = NULL;
		mutex_destroy(&trans->spmi_dfs_lock);
		kfree(trans->log);
		kfree(trans);
	}

	return 0;
}

/**
 * spmi_read_data: reads data across the SPMI bus
 * @ctrl: The SPMI controller
 * @buf: buffer to store the data read.
 * @offset: SPMI address offset to start reading from.
 * @cnt: The number of bytes to read.
 *
 * Returns 0 on success, otherwise returns error code from SPMI driver.
 */
static int
spmi_read_data(struct spmi_controller *ctrl, uint8_t *buf, int offset, int cnt)
{
	int ret = 0;
	int len;
	uint8_t sid;
	uint16_t addr;

	while (cnt > 0) {
		sid = (offset >> 16) & 0xF;
		addr = offset & 0xFFFF;
		len = min(cnt, MAX_REG_PER_TRANSACTION);

		ret = spmi_ext_register_readl(ctrl, sid, addr, buf, len);
		if (ret < 0) {
			pr_err("SPMI read failed, err = %d\n", ret);
			goto done;
		}

		cnt -= len;
		buf += len;
		offset += len;
	}

done:
	return ret;
}

/**
 * spmi_write_data: writes data across the SPMI bus
 * @ctrl: The SPMI controller
 * @buf: data to be written.
 * @offset: SPMI address offset to start writing to.
 * @cnt: The number of bytes to write.
 *
 * Returns 0 on success, otherwise returns error code from SPMI driver.
 */
static int
spmi_write_data(struct spmi_controller *ctrl, uint8_t *buf, int offset, int cnt)
{
	int ret = 0;
	int len;
	uint8_t sid;
	uint16_t addr;

	while (cnt > 0) {
		sid = (offset >> 16) & 0xF;
		addr = offset & 0xFFFF;
		len = min(cnt, MAX_REG_PER_TRANSACTION);

		ret = spmi_ext_register_writel(ctrl, sid, addr, buf, len);
		if (ret < 0) {
			pr_err("SPMI write failed, err = %d\n", ret);
			goto done;
		}

		cnt -= len;
		buf += len;
		offset += len;
	}

done:
	return ret;
}

/**
 * print_to_log: format a string and place into the log buffer
 * @log: The log buffer to place the result into.
 * @fmt: The format string to use.
 * @...: The arguments for the format string.
 *
 * The return value is the number of characters written to @log buffer
 * not including the trailing '\0'.
 */
static int print_to_log(struct spmi_log_buffer *log, const char *fmt, ...)
{
	va_list args;
	int cnt;
	char *buf = &log->data[log->wpos];
	size_t size = log->len - log->wpos;

	va_start(args, fmt);
	cnt = vscnprintf(buf, size, fmt, args);
	va_end(args);

	log->wpos += cnt;
	return cnt;
}

/**
 * write_next_line_to_log: Writes a single "line" of data into the log buffer
 * @trans: Pointer to SPMI transaction data.
 * @offset: SPMI address offset to start reading from.
 * @pcnt: Pointer to 'cnt' variable.  Indicates the number of bytes to read.
 *
 * The 'offset' is a 20-bits SPMI address which includes a 4-bit slave id (SID),
 * an 8-bit peripheral id (PID), and an 8-bit peripheral register address.
 *
 * On a successful read, the pcnt is decremented by the number of data
 * bytes read across the SPMI bus.  When the cnt reaches 0, all requested
 * bytes have been read.
 */
static int
write_next_line_to_log(struct spmi_trans *trans, int offset, size_t *pcnt)
{
	int i, j;
	u8  data[ITEMS_PER_LINE];
	struct spmi_log_buffer *log = trans->log;

	int cnt = 0;
	int padding = offset % ITEMS_PER_LINE;
	int items_to_read = min(ARRAY_SIZE(data) - padding, *pcnt);
	int items_to_log = min(ITEMS_PER_LINE, padding + items_to_read);

	/* Buffer needs enough space for an entire line */
	if ((log->len - log->wpos) < MAX_LINE_LENGTH)
		goto done;

	/* Read the desired number of "items" */
	if (spmi_read_data(trans->ctrl, data, offset, items_to_read))
		goto done;

	*pcnt -= items_to_read;

	/* Each line starts with the aligned offset (20-bit address) */
	cnt = print_to_log(log, "%5.5X ", offset & 0xffff0);
	if (cnt == 0)
		goto done;

	/* If the offset is unaligned, add padding to right justify items */
	for (i = 0; i < padding; ++i) {
		cnt = print_to_log(log, "-- ");
		if (cnt == 0)
			goto done;
	}

	/* Log the data items */
	for (j = 0; i < items_to_log; ++i, ++j) {
		cnt = print_to_log(log, "%2.2X ", data[j]);
		if (cnt == 0)
			goto done;
	}

	/* If the last character was a space, then replace it with a newline */
	if (log->wpos > 0 && log->data[log->wpos - 1] == ' ')
		log->data[log->wpos - 1] = '\n';

done:
	return cnt;
}

/**
 * write_raw_data_to_log: Writes a single "line" of data into the log buffer
 * @trans: Pointer to SPMI transaction data.
 * @offset: SPMI address offset to start reading from.
 * @pcnt: Pointer to 'cnt' variable.  Indicates the number of bytes to read.
 *
 * The 'offset' is a 20-bits SPMI address which includes a 4-bit slave id (SID),
 * an 8-bit peripheral id (PID), and an 8-bit peripheral register address.
 *
 * On a successful read, the pcnt is decremented by the number of data
 * bytes read across the SPMI bus.  When the cnt reaches 0, all requested
 * bytes have been read.
 */
static int
write_raw_data_to_log(struct spmi_trans *trans, int offset, size_t *pcnt)
{
	u8  data[16];
	struct spmi_log_buffer *log = trans->log;

	int i;
	int cnt = 0;
	int items_to_read = min(ARRAY_SIZE(data), *pcnt);

	/* Buffer needs enough space for an entire line */
	if ((log->len - log->wpos) < 80)
		goto done;

	/* Read the desired number of "items" */
	if (spmi_read_data(trans->ctrl, data, offset, items_to_read))
		goto done;

	*pcnt -= items_to_read;

	/* Log the data items */
	for (i = 0; i < items_to_read; ++i) {
		cnt = print_to_log(log, "0x%2.2X ", data[i]);
		if (cnt == 0)
			goto done;
	}

	/* If the last character was a space, then replace it with a newline */
	if (log->wpos > 0 && log->data[log->wpos - 1] == ' ')
		log->data[log->wpos - 1] = '\n';

done:
	return cnt;
}

/**
 * get_log_data - reads data across the SPMI bus and saves to the log buffer
 * @trans: Pointer to SPMI transaction data.
 *
 * Returns the number of "items" read or SPMI error code for read failures.
 */
static int get_log_data(struct spmi_trans *trans)
{
	int cnt;
	int last_cnt;
	int items_read;
	int total_items_read = 0;
	u32 offset = trans->offset;
	size_t item_cnt = trans->cnt;
	struct spmi_log_buffer *log = trans->log;
	int (*write_to_log)(struct spmi_trans *, int, size_t *);

	if (item_cnt == 0)
		return 0;

	if (trans->raw_data)
		write_to_log = write_raw_data_to_log;
	else
		write_to_log = write_next_line_to_log;

	/* Reset the log buffer 'pointers' */
	log->wpos = log->rpos = 0;

	/* Keep reading data until the log is full */
	do {
		last_cnt = item_cnt;
		cnt = write_to_log(trans, offset, &item_cnt);
		items_read = last_cnt - item_cnt;
		offset += items_read;
		total_items_read += items_read;
	} while (cnt && item_cnt > 0);

	/* Adjust the transaction offset and count */
	trans->cnt = item_cnt;
	trans->offset += total_items_read;

	return total_items_read;
}

/**
 * spmi_dfs_reg_write: write user's byte array (coded as string) over SPMI.
 * @file: file pointer
 * @buf: user data to be written.
 * @count: maximum space available in @buf
 * @ppos: starting position
 * @return number of user byte written, or negative error value
 */
static ssize_t spmi_dfs_reg_write(struct file *file, const char __user *buf,
			size_t count, loff_t *ppos)
{
	int bytes_read;
	int data;
	int pos = 0;
	int cnt = 0;
	u8  *values;
	size_t ret = 0;
	u32 offset;
	char *kbuf;
	struct spmi_trans *trans = file->private_data;

	mutex_lock(&trans->spmi_dfs_lock);

	trans = file->private_data;
	offset = trans->offset;

	/* Make a copy of the user data */
	kbuf = kmalloc(count + 1, GFP_KERNEL);
	if (!kbuf) {
		ret = -ENOMEM;
		goto unlock_mutex;
	}

	ret = copy_from_user(kbuf, buf, count);
	if (ret == count) {
		pr_err("failed to copy data from user\n");
		ret = -EFAULT;
		goto free_buf;
	}

	count -= ret;
	*ppos += count;
	kbuf[count] = '\0';

	/* Override the text buffer with the raw data */
	values = kbuf;

	/* Parse the data in the buffer.  It should be a string of numbers */
	while (sscanf(kbuf + pos, "%i%n", &data, &bytes_read) == 1) {
		pos += bytes_read;
		values[cnt++] = data & 0xff;
	}

	if (!cnt)
		goto free_buf;

	/* Perform the SPMI write(s) */
	ret = spmi_write_data(trans->ctrl, values, offset, cnt);

	if (ret) {
		pr_err("SPMI write failed, err = %zu\n", ret);
	} else {
		ret = count;
		trans->offset += cnt;
	}

free_buf:
	kfree(kbuf);
unlock_mutex:
	mutex_unlock(&trans->spmi_dfs_lock);
	return ret;
}

/**
 * spmi_dfs_reg_read: reads value(s) over SPMI and fill user's buffer a
 *  byte array (coded as string)
 * @file: file pointer
 * @buf: where to put the result
 * @count: maximum space available in @buf
 * @ppos: starting position
 * @return number of user bytes read, or negative error value
 */
static ssize_t spmi_dfs_reg_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	struct spmi_trans *trans = file->private_data;
	struct spmi_log_buffer *log = trans->log;
	size_t ret;
	size_t len;

	mutex_lock(&trans->spmi_dfs_lock);
	/* Is the the log buffer empty */
	if (log->rpos >= log->wpos) {
		if (get_log_data(trans) <= 0) {
			len = 0;
			goto unlock_mutex;
		}
	}

	len = min(count, log->wpos - log->rpos);

	ret = copy_to_user(buf, &log->data[log->rpos], len);
	if (ret == len) {
		pr_err("error copy SPMI register values to user\n");
		len = -EFAULT;
		goto unlock_mutex;
	}

	/* 'ret' is the number of bytes not copied */
	len -= ret;

	*ppos += len;
	log->rpos += len;

unlock_mutex:
	mutex_unlock(&trans->spmi_dfs_lock);
	return len;
}

static const struct file_operations spmi_dfs_reg_fops = {
	.open		= spmi_dfs_data_open,
	.release	= spmi_dfs_close,
	.read		= spmi_dfs_reg_read,
	.write		= spmi_dfs_reg_write,
};

static const struct file_operations spmi_dfs_raw_data_fops = {
	.open		= spmi_dfs_raw_data_open,
	.release	= spmi_dfs_close,
	.read		= spmi_dfs_reg_read,
	.write		= spmi_dfs_reg_write,
};

/**
 * spmi_dfs_create_fs: create debugfs file system.
 * @return pointer to root directory or NULL if failed to create fs
 */
static struct dentry *spmi_dfs_create_fs(void)
{
	struct dentry *root, *file;

	pr_debug("Creating SPMI debugfs file-system\n");
	root = debugfs_create_dir(DFS_ROOT_NAME, NULL);
	if (IS_ERR_OR_NULL(root)) {
		pr_err("Error creating top level directory err:%ld",
			(long)root);
		if (PTR_ERR(root) == -ENODEV)
			pr_err("debugfs is not enabled in the kernel");
		return NULL;
	}

	dbgfs_data.help_msg.size = strlen(dbgfs_data.help_msg.data);

	file = debugfs_create_blob("help", S_IRUGO, root, &dbgfs_data.help_msg);
	if (!file) {
		pr_err("error creating help entry\n");
		goto err_remove_fs;
	}
	return root;

err_remove_fs:
	debugfs_remove_recursive(root);
	return NULL;
}

/**
 * spmi_dfs_get_root: return a pointer to SPMI debugfs root directory.
 * @brief return a pointer to the existing directory, or if no root
 * directory exists then create one. Directory is created with file that
 * configures SPMI transaction, namely: sid, address, and count.
 * @returns valid pointer on success or NULL
 */
struct dentry *spmi_dfs_get_root(void)
{
	if (dbgfs_data.root)
		return dbgfs_data.root;

	if (mutex_lock_interruptible(&dbgfs_data.lock) < 0)
		return NULL;
	/* critical section */
	if (!dbgfs_data.root) { /* double checking idiom */
		dbgfs_data.root = spmi_dfs_create_fs();
	}
	mutex_unlock(&dbgfs_data.lock);
	return dbgfs_data.root;
}

/*
 * spmi_dfs_add_controller: adds new spmi controller entry
 * @return zero on success
 */
int spmi_dfs_add_controller(struct spmi_controller *ctrl)
{
	struct dentry *dir;
	struct dentry *root;
	struct dentry *file;
	struct spmi_ctrl_data *ctrl_data;

	pr_debug("Adding controller %s\n", ctrl->dev.kobj.name);
	root = spmi_dfs_get_root();
	if (!root)
		return -ENOENT;

	/* Allocate transaction data for the controller */
	ctrl_data = kzalloc(sizeof(*ctrl_data), GFP_KERNEL);
	if (!ctrl_data)
		return -ENOMEM;

	dir = debugfs_create_dir(ctrl->dev.kobj.name, root);
	if (!dir) {
		pr_err("Error creating entry for spmi controller %s\n",
						ctrl->dev.kobj.name);
		goto err_create_dir_failed;
	}

	ctrl_data->cnt  = 1;
	ctrl_data->dir  = dir;
	ctrl_data->ctrl = ctrl;

	file = debugfs_create_u32("count", DFS_MODE, dir, &ctrl_data->cnt);
	if (!file) {
		pr_err("error creating 'count' entry\n");
		goto err_remove_fs;
	}

	file = debugfs_create_x32("address", DFS_MODE, dir, &ctrl_data->addr);
	if (!file) {
		pr_err("error creating 'address' entry\n");
		goto err_remove_fs;
	}

	file = debugfs_create_file("data", DFS_MODE, dir, ctrl_data,
							&spmi_dfs_reg_fops);
	if (!file) {
		pr_err("error creating 'data' entry\n");
		goto err_remove_fs;
	}

	file = debugfs_create_file("data_raw", DFS_MODE, dir, ctrl_data,
						&spmi_dfs_raw_data_fops);
	if (!file) {
		pr_err("error creating 'data' entry\n");
		goto err_remove_fs;
	}

	list_add(&ctrl_data->node, &dbgfs_data.ctrl);
	return 0;

err_remove_fs:
	debugfs_remove_recursive(dir);
err_create_dir_failed:
	kfree(ctrl_data);
	return -ENOMEM;
}

/*
 * spmi_dfs_del_controller: deletes spmi controller entry
 * @return zero on success
 */
int spmi_dfs_del_controller(struct spmi_controller *ctrl)
{
	int rc;
	struct list_head *pos, *tmp;
	struct spmi_ctrl_data *ctrl_data;

	pr_debug("Deleting controller %s\n", ctrl->dev.kobj.name);

	rc = mutex_lock_interruptible(&dbgfs_data.lock);
	if (rc)
		return rc;

	list_for_each_safe(pos, tmp, &dbgfs_data.ctrl) {
		ctrl_data = list_entry(pos, struct spmi_ctrl_data, node);

		if (ctrl_data->ctrl == ctrl) {
			debugfs_remove_recursive(ctrl_data->dir);
			list_del(pos);
			kfree(ctrl_data);
			rc = 0;
			goto done;
		}
	}
	rc = -EINVAL;
	pr_debug("Unknown controller %s\n", ctrl->dev.kobj.name);

done:
	mutex_unlock(&dbgfs_data.lock);
	return rc;
}

/*
 * spmi_dfs_create_file: creates a new file in the SPMI debugfs
 * @returns valid dentry pointer on success or NULL
 */
struct dentry *spmi_dfs_create_file(struct spmi_controller *ctrl,
					const char *name, void *data,
					const struct file_operations *fops)
{
	struct spmi_ctrl_data *ctrl_data;

	list_for_each_entry(ctrl_data, &dbgfs_data.ctrl, node) {
		if (ctrl_data->ctrl == ctrl)
			return debugfs_create_file(name,
					DFS_MODE, ctrl_data->dir, data, fops);
	}

	return NULL;
}

static void __exit spmi_dfs_delete_all_ctrl(struct list_head *head)
{
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, head) {
		struct spmi_ctrl_data *ctrl_data;

		ctrl_data = list_entry(pos, struct spmi_ctrl_data, node);
		list_del(pos);
		kfree(ctrl_data);
	}
}

static void __exit spmi_dfs_destroy(void)
{
	pr_debug("de-initializing spmi debugfs ...");
	if (mutex_lock_interruptible(&dbgfs_data.lock) < 0)
		return;
	if (dbgfs_data.root) {
		debugfs_remove_recursive(dbgfs_data.root);
		dbgfs_data.root = NULL;
		spmi_dfs_delete_all_ctrl(&dbgfs_data.ctrl);
	}
	mutex_unlock(&dbgfs_data.lock);
}

module_exit(spmi_dfs_destroy);

MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:spmi_debug_fs");
