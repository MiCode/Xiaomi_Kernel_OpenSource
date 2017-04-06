/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#include "fg-core.h"

void fg_circ_buf_add(struct fg_circ_buf *buf, int val)
{
	buf->arr[buf->head] = val;
	buf->head = (buf->head + 1) % ARRAY_SIZE(buf->arr);
	buf->size = min(++buf->size, (int)ARRAY_SIZE(buf->arr));
}

void fg_circ_buf_clr(struct fg_circ_buf *buf)
{
	memset(buf, 0, sizeof(*buf));
}

int fg_circ_buf_avg(struct fg_circ_buf *buf, int *avg)
{
	s64 result = 0;
	int i;

	if (buf->size == 0)
		return -ENODATA;

	for (i = 0; i < buf->size; i++)
		result += buf->arr[i];

	*avg = div_s64(result, buf->size);
	return 0;
}

int fg_lerp(const struct fg_pt *pts, size_t tablesize, s32 input, s32 *output)
{
	int i;
	s64 temp;

	if (pts == NULL) {
		pr_err("Table is NULL\n");
		return -EINVAL;
	}

	if (tablesize < 1) {
		pr_err("Table has no entries\n");
		return -ENOENT;
	}

	if (tablesize == 1) {
		*output = pts[0].y;
		return 0;
	}

	if (pts[0].x > pts[1].x) {
		pr_err("Table is not in acending order\n");
		return -EINVAL;
	}

	if (input <= pts[0].x) {
		*output = pts[0].y;
		return 0;
	}

	if (input >= pts[tablesize - 1].x) {
		*output = pts[tablesize - 1].y;
		return 0;
	}

	for (i = 1; i < tablesize; i++) {
		if (input >= pts[i].x)
			continue;

		temp = (s64)(pts[i].y - pts[i - 1].y) *
						(s64)(input - pts[i - 1].x);
		temp = div_s64(temp, pts[i].x - pts[i - 1].x);
		*output = temp + pts[i - 1].y;
		return 0;
	}

	return -EINVAL;
}

static struct fg_dbgfs dbgfs_data = {
	.help_msg = {
	.data =
	"FG Debug-FS support\n"
	"\n"
	"Hierarchy schema:\n"
	"/sys/kernel/debug/fg_sram\n"
	"       /help            -- Static help text\n"
	"       /address  -- Starting register address for reads or writes\n"
	"       /count    -- Number of registers to read (only used for reads)\n"
	"       /data     -- Initiates the SRAM read (formatted output)\n"
	"\n",
	},
};

static bool is_usb_present(struct fg_chip *chip)
{
	union power_supply_propval pval = {0, };

	if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("usb");

	if (chip->usb_psy)
		power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_PRESENT, &pval);
	else
		return false;

	return pval.intval != 0;
}

static bool is_dc_present(struct fg_chip *chip)
{
	union power_supply_propval pval = {0, };

	if (!chip->dc_psy)
		chip->dc_psy = power_supply_get_by_name("dc");

	if (chip->dc_psy)
		power_supply_get_property(chip->dc_psy,
				POWER_SUPPLY_PROP_PRESENT, &pval);
	else
		return false;

	return pval.intval != 0;
}

bool is_input_present(struct fg_chip *chip)
{
	return is_usb_present(chip) || is_dc_present(chip);
}

#define EXPONENT_SHIFT		11
#define EXPONENT_OFFSET		-9
#define MANTISSA_SIGN_BIT	10
#define MICRO_UNIT		1000000
s64 fg_float_decode(u16 val)
{
	s8 exponent;
	s32 mantissa;

	/* mantissa bits are shifted out during sign extension */
	exponent = ((s16)val >> EXPONENT_SHIFT) + EXPONENT_OFFSET;
	/* exponent bits are shifted out during sign extension */
	mantissa = sign_extend32(val, MANTISSA_SIGN_BIT) * MICRO_UNIT;

	if (exponent < 0)
		return (s64)mantissa >> -exponent;

	return (s64)mantissa << exponent;
}

void fill_string(char *str, size_t str_len, u8 *buf, int buf_len)
{
	int pos = 0;
	int i;

	for (i = 0; i < buf_len; i++) {
		pos += scnprintf(str + pos, str_len - pos, "%02x", buf[i]);
		if (i < buf_len - 1)
			pos += scnprintf(str + pos, str_len - pos, " ");
	}
}

void dump_sram(u8 *buf, int addr, int len)
{
	int i;
	char str[16];

	/*
	 * Length passed should be in multiple of 4 as each FG SRAM word
	 * holds 4 bytes. To keep this simple, even if a length which is
	 * not a multiple of 4 bytes or less than 4 bytes is passed, SRAM
	 * registers dumped will be always in multiple of 4 bytes.
	 */
	for (i = 0; i < len; i += 4) {
		str[0] = '\0';
		fill_string(str, sizeof(str), buf + i, 4);
		pr_info("%03d %s\n", addr + (i / 4), str);
	}
}

static inline bool fg_sram_address_valid(u16 address, int len)
{
	if (address > FG_SRAM_ADDRESS_MAX)
		return false;

	if ((address + DIV_ROUND_UP(len, 4)) > FG_SRAM_ADDRESS_MAX + 1)
		return false;

	return true;
}

#define SOC_UPDATE_WAIT_MS	1500
int fg_sram_write(struct fg_chip *chip, u16 address, u8 offset,
			u8 *val, int len, int flags)
{
	int rc = 0;
	bool tried_again = false;
	bool atomic_access = false;

	if (!chip)
		return -ENXIO;

	if (chip->battery_missing)
		return -ENODATA;

	if (!fg_sram_address_valid(address, len))
		return -EFAULT;

	if (!(flags & FG_IMA_NO_WLOCK))
		vote(chip->awake_votable, SRAM_WRITE, true, 0);
	mutex_lock(&chip->sram_rw_lock);

	if ((flags & FG_IMA_ATOMIC) && chip->irqs[SOC_UPDATE_IRQ].irq) {
		/*
		 * This interrupt need to be enabled only when it is
		 * required. It will be kept disabled other times.
		 */
		reinit_completion(&chip->soc_update);
		enable_irq(chip->irqs[SOC_UPDATE_IRQ].irq);
		atomic_access = true;
	} else {
		flags = FG_IMA_DEFAULT;
	}
wait:
	/*
	 * Atomic access mean waiting upon SOC_UPDATE interrupt from
	 * FG_ALG and do the transaction after that. This is to make
	 * sure that there will be no SOC update happening when an
	 * IMA write is happening. SOC_UPDATE interrupt fires every
	 * FG cycle (~1.47 seconds).
	 */
	if (atomic_access) {
		/* Wait for SOC_UPDATE completion */
		rc = wait_for_completion_interruptible_timeout(
			&chip->soc_update,
			msecs_to_jiffies(SOC_UPDATE_WAIT_MS));

		/* If we were interrupted wait again one more time. */
		if (rc == -ERESTARTSYS && !tried_again) {
			tried_again = true;
			goto wait;
		} else if (rc <= 0) {
			pr_err("wait for soc_update timed out rc=%d\n", rc);
			goto out;
		}
	}

	rc = fg_interleaved_mem_write(chip, address, offset, val, len,
			atomic_access);
	if (rc < 0)
		pr_err("Error in writing SRAM address 0x%x[%d], rc=%d\n",
			address, offset, rc);
out:
	if (atomic_access)
		disable_irq_nosync(chip->irqs[SOC_UPDATE_IRQ].irq);

	mutex_unlock(&chip->sram_rw_lock);
	if (!(flags & FG_IMA_NO_WLOCK))
		vote(chip->awake_votable, SRAM_WRITE, false, 0);
	return rc;
}

int fg_sram_read(struct fg_chip *chip, u16 address, u8 offset,
			u8 *val, int len, int flags)
{
	int rc = 0;

	if (!chip)
		return -ENXIO;

	if (chip->battery_missing)
		return -ENODATA;

	if (!fg_sram_address_valid(address, len))
		return -EFAULT;

	if (!(flags & FG_IMA_NO_WLOCK))
		vote(chip->awake_votable, SRAM_READ, true, 0);
	mutex_lock(&chip->sram_rw_lock);

	rc = fg_interleaved_mem_read(chip, address, offset, val, len);
	if (rc < 0)
		pr_err("Error in reading SRAM address 0x%x[%d], rc=%d\n",
			address, offset, rc);

	mutex_unlock(&chip->sram_rw_lock);
	if (!(flags & FG_IMA_NO_WLOCK))
		vote(chip->awake_votable, SRAM_READ, false, 0);
	return rc;
}

int fg_sram_masked_write(struct fg_chip *chip, u16 address, u8 offset,
			u8 mask, u8 val, int flags)
{
	int rc = 0;
	u8 buf[4];

	rc = fg_sram_read(chip, address, 0, buf, 4, flags);
	if (rc < 0) {
		pr_err("sram read failed: address=%03X, rc=%d\n", address, rc);
		return rc;
	}

	buf[offset] &= ~mask;
	buf[offset] |= val & mask;

	rc = fg_sram_write(chip, address, 0, buf, 4, flags);
	if (rc < 0) {
		pr_err("sram write failed: address=%03X, rc=%d\n", address, rc);
		return rc;
	}

	return rc;
}

int fg_read(struct fg_chip *chip, int addr, u8 *val, int len)
{
	int rc, i;

	if (!chip || !chip->regmap)
		return -ENXIO;

	rc = regmap_bulk_read(chip->regmap, addr, val, len);

	if (rc < 0) {
		dev_err(chip->dev, "regmap_read failed for address %04x rc=%d\n",
			addr, rc);
		return rc;
	}

	if (*chip->debug_mask & FG_BUS_READ) {
		pr_info("length %d addr=%04x\n", len, addr);
		for (i = 0; i < len; i++)
			pr_info("val[%d]: %02x\n", i, val[i]);
	}

	return 0;
}

int fg_write(struct fg_chip *chip, int addr, u8 *val, int len)
{
	int rc, i;
	bool sec_access = false;

	if (!chip || !chip->regmap)
		return -ENXIO;

	mutex_lock(&chip->bus_lock);
	sec_access = (addr & 0x00FF) > 0xD0;
	if (sec_access) {
		rc = regmap_write(chip->regmap, (addr & 0xFF00) | 0xD0, 0xA5);
		if (rc < 0) {
			dev_err(chip->dev, "regmap_write failed for address %x rc=%d\n",
				addr, rc);
			goto out;
		}
	}

	if (len > 1)
		rc = regmap_bulk_write(chip->regmap, addr, val, len);
	else
		rc = regmap_write(chip->regmap, addr, *val);

	if (rc < 0) {
		dev_err(chip->dev, "regmap_write failed for address %04x rc=%d\n",
			addr, rc);
		goto out;
	}

	if (*chip->debug_mask & FG_BUS_WRITE) {
		pr_info("length %d addr=%04x\n", len, addr);
		for (i = 0; i < len; i++)
			pr_info("val[%d]: %02x\n", i, val[i]);
	}
out:
	mutex_unlock(&chip->bus_lock);
	return rc;
}

int fg_masked_write(struct fg_chip *chip, int addr, u8 mask, u8 val)
{
	int rc;
	bool sec_access = false;

	if (!chip || !chip->regmap)
		return -ENXIO;

	mutex_lock(&chip->bus_lock);
	sec_access = (addr & 0x00FF) > 0xD0;
	if (sec_access) {
		rc = regmap_write(chip->regmap, (addr & 0xFF00) | 0xD0, 0xA5);
		if (rc < 0) {
			dev_err(chip->dev, "regmap_write failed for address %x rc=%d\n",
				addr, rc);
			goto out;
		}
	}

	rc = regmap_update_bits(chip->regmap, addr, mask, val);
	if (rc < 0) {
		dev_err(chip->dev, "regmap_update_bits failed for address %04x rc=%d\n",
			addr, rc);
		goto out;
	}

	fg_dbg(chip, FG_BUS_WRITE, "addr=%04x mask: %02x val: %02x\n", addr,
		mask, val);
out:
	mutex_unlock(&chip->bus_lock);
	return rc;
}

int64_t twos_compliment_extend(int64_t val, int sign_bit_pos)
{
	int i, nbytes = DIV_ROUND_UP(sign_bit_pos, 8);
	int64_t mask, val_out;

	val_out = val;
	mask = 1 << sign_bit_pos;
	if (val & mask) {
		for (i = 8; i > nbytes; i--) {
			mask = 0xFFLL << ((i - 1) * 8);
			val_out |= mask;
		}

		if ((nbytes * 8) - 1 > sign_bit_pos) {
			mask = 1 << sign_bit_pos;
			for (i = 1; i <= (nbytes * 8) - sign_bit_pos; i++)
				val_out |= mask << i;
		}
	}

	pr_debug("nbytes: %d val: %llx val_out: %llx\n", nbytes, val, val_out);
	return val_out;
}

/* All the debugfs related functions are defined below */
static int fg_sram_dfs_open(struct inode *inode, struct file *file)
{
	struct fg_log_buffer *log;
	struct fg_trans *trans;
	u8 *data_buf;

	size_t logbufsize = SZ_4K;
	size_t databufsize = SZ_4K;

	if (!dbgfs_data.chip) {
		pr_err("Not initialized data\n");
		return -EINVAL;
	}

	/* Per file "transaction" data */
	trans = devm_kzalloc(dbgfs_data.chip->dev, sizeof(*trans), GFP_KERNEL);
	if (!trans)
		return -ENOMEM;

	/* Allocate log buffer */
	log = devm_kzalloc(dbgfs_data.chip->dev, logbufsize, GFP_KERNEL);
	if (!log)
		return -ENOMEM;

	log->rpos = 0;
	log->wpos = 0;
	log->len = logbufsize - sizeof(*log);

	/* Allocate data buffer */
	data_buf = devm_kzalloc(dbgfs_data.chip->dev, databufsize, GFP_KERNEL);
	if (!data_buf)
		return -ENOMEM;

	trans->log = log;
	trans->data = data_buf;
	trans->cnt = dbgfs_data.cnt;
	trans->addr = dbgfs_data.addr;
	trans->chip = dbgfs_data.chip;
	trans->offset = trans->addr;
	mutex_init(&trans->fg_dfs_lock);

	file->private_data = trans;
	return 0;
}

static int fg_sram_dfs_close(struct inode *inode, struct file *file)
{
	struct fg_trans *trans = file->private_data;

	if (trans && trans->log && trans->data) {
		file->private_data = NULL;
		mutex_destroy(&trans->fg_dfs_lock);
		devm_kfree(trans->chip->dev, trans->log);
		devm_kfree(trans->chip->dev, trans->data);
		devm_kfree(trans->chip->dev, trans);
	}

	return 0;
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
static int print_to_log(struct fg_log_buffer *log, const char *fmt, ...)
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
 * @trans: Pointer to SRAM transaction data.
 * @offset: SRAM address offset to start reading from.
 * @pcnt: Pointer to 'cnt' variable.  Indicates the number of bytes to read.
 *
 * The 'offset' is a 12-bit SRAM address.
 *
 * On a successful read, the pcnt is decremented by the number of data
 * bytes read from the SRAM.  When the cnt reaches 0, all requested bytes have
 * been read.
 */
static int write_next_line_to_log(struct fg_trans *trans, int offset,
				size_t *pcnt)
{
	int i;
	u8 data[ITEMS_PER_LINE];
	u16 address;
	struct fg_log_buffer *log = trans->log;
	int cnt = 0;
	int items_to_read = min(ARRAY_SIZE(data), *pcnt);
	int items_to_log = min(ITEMS_PER_LINE, items_to_read);

	/* Buffer needs enough space for an entire line */
	if ((log->len - log->wpos) < MAX_LINE_LENGTH)
		goto done;

	memcpy(data, trans->data + (offset - trans->addr), items_to_read);
	*pcnt -= items_to_read;

	/* address is in word now and it increments by 1. */
	address = trans->addr + ((offset - trans->addr) / ITEMS_PER_LINE);
	cnt = print_to_log(log, "%3.3d ", address & 0xfff);
	if (cnt == 0)
		goto done;

	/* Log the data items */
	for (i = 0; i < items_to_log; ++i) {
		cnt = print_to_log(log, "%2.2X ", data[i]);
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
 * get_log_data - reads data from SRAM and saves to the log buffer
 * @trans: Pointer to SRAM transaction data.
 *
 * Returns the number of "items" read or SPMI error code for read failures.
 */
static int get_log_data(struct fg_trans *trans)
{
	int cnt, rc;
	int last_cnt;
	int items_read;
	int total_items_read = 0;
	u32 offset = trans->offset;
	size_t item_cnt = trans->cnt;
	struct fg_log_buffer *log = trans->log;

	if (item_cnt == 0)
		return 0;

	if (item_cnt > SZ_4K) {
		pr_err("Reading too many bytes\n");
		return -EINVAL;
	}

	pr_debug("addr: %d offset: %d count: %d\n", trans->addr, trans->offset,
		trans->cnt);
	rc = fg_sram_read(trans->chip, trans->addr, 0,
			trans->data, trans->cnt, 0);
	if (rc < 0) {
		pr_err("SRAM read failed: rc = %d\n", rc);
		return rc;
	}
	/* Reset the log buffer 'pointers' */
	log->wpos = log->rpos = 0;

	/* Keep reading data until the log is full */
	do {
		last_cnt = item_cnt;
		cnt = write_next_line_to_log(trans, offset, &item_cnt);
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
 * fg_sram_dfs_reg_read: reads value(s) from SRAM and fills user's buffer a
 *  byte array (coded as string)
 * @file: file pointer
 * @buf: where to put the result
 * @count: maximum space available in @buf
 * @ppos: starting position
 * @return number of user bytes read, or negative error value
 */
static ssize_t fg_sram_dfs_reg_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	struct fg_trans *trans = file->private_data;
	struct fg_log_buffer *log = trans->log;
	size_t ret;
	size_t len;

	mutex_lock(&trans->fg_dfs_lock);
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
		pr_err("error copy sram register values to user\n");
		len = -EFAULT;
		goto unlock_mutex;
	}

	/* 'ret' is the number of bytes not copied */
	len -= ret;

	*ppos += len;
	log->rpos += len;

unlock_mutex:
	mutex_unlock(&trans->fg_dfs_lock);
	return len;
}

/**
 * fg_sram_dfs_reg_write: write user's byte array (coded as string) to SRAM.
 * @file: file pointer
 * @buf: user data to be written.
 * @count: maximum space available in @buf
 * @ppos: starting position
 * @return number of user byte written, or negative error value
 */
static ssize_t fg_sram_dfs_reg_write(struct file *file, const char __user *buf,
			size_t count, loff_t *ppos)
{
	int bytes_read;
	int data;
	int pos = 0;
	int cnt = 0;
	u8  *values;
	char *kbuf;
	size_t ret = 0;
	struct fg_trans *trans = file->private_data;
	u32 address = trans->addr;

	mutex_lock(&trans->fg_dfs_lock);
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
	while ((pos < count) &&
		sscanf(kbuf + pos, "%i%n", &data, &bytes_read) == 1) {
		/*
		 * We shouldn't be receiving a string of characters that
		 * exceeds a size of 5 to keep this functionally correct.
		 * Also, we should make sure that pos never gets overflowed
		 * beyond the limit.
		 */
		if (bytes_read > 5 || bytes_read > INT_MAX - pos) {
			cnt = 0;
			ret = -EINVAL;
			break;
		}
		pos += bytes_read;
		values[cnt++] = data & 0xff;
	}

	if (!cnt)
		goto free_buf;

	pr_debug("address %d, count %d\n", address, cnt);
	/* Perform the write(s) */

	ret = fg_sram_write(trans->chip, address, 0, values, cnt, 0);
	if (ret) {
		pr_err("SRAM write failed, err = %zu\n", ret);
	} else {
		ret = count;
		trans->offset += cnt > 4 ? 4 : cnt;
	}

free_buf:
	kfree(kbuf);
unlock_mutex:
	mutex_unlock(&trans->fg_dfs_lock);
	return ret;
}

static const struct file_operations fg_sram_dfs_reg_fops = {
	.open		= fg_sram_dfs_open,
	.release	= fg_sram_dfs_close,
	.read		= fg_sram_dfs_reg_read,
	.write		= fg_sram_dfs_reg_write,
};

/*
 * fg_debugfs_create: adds new fg_sram debugfs entry
 * @return zero on success
 */
static int fg_sram_debugfs_create(struct fg_chip *chip)
{
	struct dentry *dfs_sram;
	struct dentry *file;
	mode_t dfs_mode = S_IRUSR | S_IWUSR;

	pr_debug("Creating FG_SRAM debugfs file-system\n");
	dfs_sram = debugfs_create_dir("sram", chip->dfs_root);
	if (!dfs_sram) {
		pr_err("error creating fg sram dfs rc=%ld\n",
		       (long)dfs_sram);
		return -ENOMEM;
	}

	dbgfs_data.help_msg.size = strlen(dbgfs_data.help_msg.data);
	file = debugfs_create_blob("help", S_IRUGO, dfs_sram,
					&dbgfs_data.help_msg);
	if (!file) {
		pr_err("error creating help entry\n");
		goto err_remove_fs;
	}

	dbgfs_data.chip = chip;

	file = debugfs_create_u32("count", dfs_mode, dfs_sram,
					&(dbgfs_data.cnt));
	if (!file) {
		pr_err("error creating 'count' entry\n");
		goto err_remove_fs;
	}

	file = debugfs_create_x32("address", dfs_mode, dfs_sram,
					&(dbgfs_data.addr));
	if (!file) {
		pr_err("error creating 'address' entry\n");
		goto err_remove_fs;
	}

	file = debugfs_create_file("data", dfs_mode, dfs_sram, &dbgfs_data,
					&fg_sram_dfs_reg_fops);
	if (!file) {
		pr_err("error creating 'data' entry\n");
		goto err_remove_fs;
	}

	return 0;

err_remove_fs:
	debugfs_remove_recursive(dfs_sram);
	return -ENOMEM;
}

static int fg_alg_flags_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t fg_alg_flags_read(struct file *file, char __user *userbuf,
				 size_t count, loff_t *ppos)
{
	struct fg_chip *chip = file->private_data;
	char buf[512];
	u8 alg_flags = 0;
	int rc, i, len;

	rc = fg_sram_read(chip, chip->sp[FG_SRAM_ALG_FLAGS].addr_word,
			  chip->sp[FG_SRAM_ALG_FLAGS].addr_byte, &alg_flags, 1,
			  FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("failed to read algorithm flags rc=%d\n", rc);
		return -EFAULT;
	}

	len = 0;
	for (i = 0; i < ALG_FLAG_MAX; ++i) {
		if (len > ARRAY_SIZE(buf) - 1)
			return -EFAULT;
		if (chip->alg_flags[i].invalid)
			continue;

		len += snprintf(buf + len, sizeof(buf) - sizeof(*buf) * len,
				"%s = %d\n", chip->alg_flags[i].name,
				(bool)(alg_flags & chip->alg_flags[i].bit));
	}

	return simple_read_from_buffer(userbuf, count, ppos, buf, len);
}

static const struct file_operations fg_alg_flags_fops = {
	.open = fg_alg_flags_open,
	.read = fg_alg_flags_read,
};

int fg_debugfs_create(struct fg_chip *chip)
{
	int rc;

	pr_debug("Creating debugfs file-system\n");
	chip->dfs_root = debugfs_create_dir("fg", NULL);
	if (IS_ERR_OR_NULL(chip->dfs_root)) {
		if (PTR_ERR(chip->dfs_root) == -ENODEV)
			pr_err("debugfs is not enabled in the kernel\n");
		else
			pr_err("error creating fg dfs root rc=%ld\n",
			       (long)chip->dfs_root);
		return -ENODEV;
	}

	rc = fg_sram_debugfs_create(chip);
	if (rc < 0) {
		pr_err("failed to create sram dfs rc=%d\n", rc);
		goto err_remove_fs;
	}

	if (!debugfs_create_file("alg_flags", S_IRUSR, chip->dfs_root, chip,
				 &fg_alg_flags_fops)) {
		pr_err("failed to create alg_flags file\n");
		goto err_remove_fs;
	}

	return 0;

err_remove_fs:
	debugfs_remove_recursive(chip->dfs_root);
	return -ENOMEM;
}
