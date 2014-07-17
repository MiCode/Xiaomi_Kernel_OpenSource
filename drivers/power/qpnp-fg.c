/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"FG: %s: " fmt, __func__

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/spmi.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/string_helpers.h>

/* Register offsets */

/* Interrupt offsets */
#define INT_RT_STS(base)			(base + 0x10)

/* SPMI Register offsets */
#define SOC_MONOTONIC_SOC	0x09
#define MEM_INTF_CFG		0x40
#define MEM_INTF_CTL		0x41
#define MEM_INTF_ADDR_LSB	0x42
#define MEM_INTF_ADDR_MSB	0x43
#define MEM_INTF_WR_DATA0	0x48
#define MEM_INTF_WR_DATA1	0x49
#define MEM_INTF_WR_DATA2	0x4A
#define MEM_INTF_WR_DATA3	0x4B
#define MEM_INTF_RD_DATA0	0x4C
#define MEM_INTF_RD_DATA1	0x4D
#define MEM_INTF_RD_DATA2	0x4E
#define MEM_INTF_RD_DATA3	0x4F
#define OTP_CFG1		0xE2
#define SOC_BOOT_MOD		0x50
#define SOC_RESTART		0x51

#define REG_OFFSET_PERP_SUBTYPE	0x05

/* RAM register offsets */
#define RAM_OFFSET		0x400

/* Bit/Mask definitions */
#define FULL_PERCENT		0xFF
#define MAX_TRIES_SOC		5
#define MA_MV_BIT_RES		39
#define MSB_SIGN		BIT(7)
#define IBAT_VBAT_MASK		0x7F
#define NO_OTP_PROF_RELOAD	BIT(6)
#define REDO_FIRST_ESTIMATE	BIT(3)
#define RESTART_GO		BIT(0)

/* SUBTYPE definitions */
#define FG_SOC			0x9
#define FG_BATT			0xA
#define FG_ADC			0xB
#define FG_MEMIF		0xC

#define QPNP_FG_DEV_NAME "qcom,qpnp-fg"
#define MEM_IF_TIMEOUT_MS	2200

/* Debug Flag Definitions */
enum {
	FG_SPMI_DEBUG_WRITES		= BIT(0), /* Show SPMI writes */
	FG_SPMI_DEBUG_READS		= BIT(1), /* Show SPMI reads */
	FG_IRQS				= BIT(2), /* Show interrupts */
	FG_MEM_DEBUG_WRITES		= BIT(3), /* Show SRAM writes */
	FG_MEM_DEBUG_READS		= BIT(4), /* Show SRAM reads */
	FG_POWER_SUPPLY			= BIT(5), /* Show POWER_SUPPLY */
};

struct fg_mem_setting {
	u16	address;
	u8	offset;
	int	value;
};

struct fg_mem_data {
	u16	address;
	u8	offset;
	unsigned int len;
	int	value;
};

/* FG_MEMIF setting index */
enum fg_mem_setting_index {
	FG_MEM_SOFT_COLD = 0,
	FG_MEM_SOFT_HOT,
	FG_MEM_HARD_COLD,
	FG_MEM_HARD_HOT,
	FG_MEM_SETTING_MAX,
};

/* FG_MEMIF data index */
enum fg_mem_data_index {
	FG_DATA_BATT_TEMP = 0,
	FG_DATA_OCV,
	FG_DATA_VOLTAGE,
	FG_DATA_CURRENT,
	FG_DATA_MAX,
};

#define SETTING(_idx, _address, _offset, _value)	\
	[FG_MEM_##_idx] = {				\
		.address = _address,			\
		.offset = _offset,			\
		.value = _value,			\
	}						\

static struct fg_mem_setting settings[FG_MEM_SETTING_MAX] = {
	/*       ID                    Address, Offset, Value*/
	SETTING(SOFT_COLD,       0x454,   0,      100),
	SETTING(SOFT_HOT,        0x454,   1,      400),
	SETTING(HARD_COLD,       0x454,   2,      50),
	SETTING(HARD_HOT,        0x454,   3,      450),
};

#define DATA(_idx, _address, _offset, _length,  _value)	\
	[FG_DATA_##_idx] = {				\
		.address = _address,			\
		.offset = _offset,			\
		.len = _length,			\
		.value = _value,			\
	}						\

static struct fg_mem_data fg_data[FG_DATA_MAX] = {
	/*       ID           Address, Offset, Length, Value*/
	DATA(BATT_TEMP,       0x550,   2,      2,     -EINVAL),
	DATA(OCV,             0x588,   3,      2,     -EINVAL),
	DATA(VOLTAGE,         0x5CC,   1,      2,     -EINVAL),
	DATA(CURRENT,         0x5CC,   3,      2,     -EINVAL),
};

static int fg_debug_mask;
module_param_named(
	debug_mask, fg_debug_mask, int, S_IRUSR | S_IWUSR
);

struct fg_irq {
	int			irq;
	unsigned long		disabled;
};

enum fg_soc_irq {
	HIGH_SOC,
	LOW_SOC,
	FULL_SOC,
	EMPTY_SOC,
	DELTA_SOC,
	FIRST_EST_DONE,
	SW_FALLBK_OCV,
	SW_FALLBK_NEW_BATTRT_STS,
	FG_SOC_IRQ_COUNT,
};

enum fg_batt_irq {
	JEITA_SOFT_COLD,
	JEITA_SOFT_HOT,
	VBATT_LOW,
	BATT_IDENTIFIED,
	BATT_ID_REQ,
	BATT_UNKNOWN,
	BATT_MISSING,
	BATT_MATCH,
	FG_BATT_IRQ_COUNT,
};

enum fg_mem_if_irq {
	FG_MEM_AVAIL,
	TA_RCVRY_SUG,
	FG_MEM_IF_IRQ_COUNT,
};

struct fg_chip {
	struct device		*dev;
	struct spmi_device	*spmi;
	u16			soc_base;
	u16			batt_base;
	u16			mem_base;
	u16			vbat_adc_addr;
	u16			ibat_adc_addr;
	atomic_t		memif_user_cnt;
	bool			fast_access;
	struct fg_irq		soc_irq[FG_SOC_IRQ_COUNT];
	struct fg_irq		batt_irq[FG_BATT_IRQ_COUNT];
	struct fg_irq		mem_irq[FG_MEM_IF_IRQ_COUNT];
	struct completion	sram_access;
	struct power_supply	bms_psy;
	struct mutex		rw_lock;
	struct work_struct	batt_profile_init;
	bool			profile_loaded;
	bool			use_otp_profile;
	struct delayed_work	update_jeita_setting;
	struct delayed_work	update_sram_data;
	char			*batt_profile;
	unsigned int		batt_profile_len;
};

/* FG_MEMIF DEBUGFS structures */
#define ADDR_LEN	4	/* 3 byte address + 1 space character */
#define CHARS_PER_ITEM	3	/* Format is 'XX ' */
#define ITEMS_PER_LINE	4	/* 4 data items per line */
#define MAX_LINE_LENGTH  (ADDR_LEN + (ITEMS_PER_LINE * CHARS_PER_ITEM) + 1)
#define MAX_REG_PER_TRANSACTION	(8)

static const char *DFS_ROOT_NAME	= "fg_memif";
static const mode_t DFS_MODE = S_IRUSR | S_IWUSR;

/* Log buffer */
struct fg_log_buffer {
	size_t rpos;	/* Current 'read' position in buffer */
	size_t wpos;	/* Current 'write' position in buffer */
	size_t len;	/* Length of the buffer */
	char data[0];	/* Log buffer */
};

/* transaction parameters */
struct fg_trans {
	u32 cnt;	/* Number of bytes to read */
	u16 addr;	/* 12-bit address in SRAM */
	u32 offset;	/* Offset of last read data + byte offset */
	struct fg_chip *chip;
	struct fg_log_buffer *log; /* log buffer */
};

struct fg_dbgfs {
	u32 cnt;
	u32 addr;
	struct fg_chip *chip;
	struct dentry *root;
	struct mutex  lock;
	struct debugfs_blob_wrapper help_msg;
};

static struct fg_dbgfs dbgfs_data = {
	.lock = __MUTEX_INITIALIZER(dbgfs_data.lock),
	.help_msg = {
	.data =
"FG Debug-FS support\n"
"\n"
"Hierarchy schema:\n"
"/sys/kernel/debug/fg_memif\n"
"       /help            -- Static help text\n"
"       /address  -- Starting register address for reads or writes\n"
"       /count    -- Number of registers to read (only used for reads)\n"
"       /data     -- Initiates the SRAM read (formatted output)\n"
"\n",
	},
};

static const struct of_device_id fg_match_table[] = {
	{	.compatible = QPNP_FG_DEV_NAME, },
	{}
};

static char *fg_supplicants[] = {
	"battery"
};

#define DEBUG_PRINT_BUFFER_SIZE 64
static void fill_string(char *str, size_t str_len, u8 *buf, int buf_len)
{
	int pos = 0;
	int i;

	for (i = 0; i < buf_len; i++) {
		pos += scnprintf(str + pos, str_len - pos, "0x%02X", buf[i]);
		if (i < buf_len - 1)
			pos += scnprintf(str + pos, str_len - pos, ", ");
	}
}

static int fg_write(struct fg_chip *chip, u8 *val, u16 addr, int len)
{
	int rc = 0;
	struct spmi_device *spmi = chip->spmi;
	char str[DEBUG_PRINT_BUFFER_SIZE];

	if ((addr & 0xff00) == 0) {
		pr_err("addr cannot be zero base=0x%02x sid=0x%02x rc=%d\n",
			addr, spmi->sid, rc);
		return -EINVAL;
	}

	rc = spmi_ext_register_writel(spmi->ctrl, spmi->sid, addr, val, len);
	if (rc) {
		pr_err("write failed addr=0x%02x sid=0x%02x rc=%d\n",
			addr, spmi->sid, rc);
		return rc;
	}

	if (!rc && (fg_debug_mask & FG_SPMI_DEBUG_WRITES)) {
		str[0] = '\0';
		fill_string(str, DEBUG_PRINT_BUFFER_SIZE, val, len);
		pr_info("write(0x%04X), sid=%d, len=%d; %s\n",
			addr, spmi->sid, len, str);
	}

	return rc;
}

static int fg_read(struct fg_chip *chip, u8 *val, u16 addr, int len)
{
	int rc = 0;
	struct spmi_device *spmi = chip->spmi;
	char str[DEBUG_PRINT_BUFFER_SIZE];

	if ((addr & 0xff00) == 0) {
		pr_err("base cannot be zero base=0x%02x sid=0x%02x rc=%d\n",
			addr, spmi->sid, rc);
		return -EINVAL;
	}

	rc = spmi_ext_register_readl(spmi->ctrl, spmi->sid, addr, val, len);
	if (rc) {
		pr_err("SPMI read failed base=0x%02x sid=0x%02x rc=%d\n", addr,
				spmi->sid, rc);
		return rc;
	}

	if (!rc && (fg_debug_mask & FG_SPMI_DEBUG_READS)) {
		str[0] = '\0';
		fill_string(str, DEBUG_PRINT_BUFFER_SIZE, val, len);
		pr_info("read(0x%04x), sid=%d, len=%d; %s\n",
			addr, spmi->sid, len, str);
	}

	return rc;
}

static int fg_masked_write(struct fg_chip *chip, u16 addr,
		u8 mask, u8 val, int len)
{
	int rc;
	u8 reg;

	rc = fg_read(chip, &reg, addr, len);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n", addr, rc);
		return rc;
	}
	pr_debug("addr = 0x%x read 0x%x\n", addr, reg);

	reg &= ~mask;
	reg |= val & mask;

	pr_debug("Writing 0x%x\n", reg);

	rc = fg_write(chip, &reg, addr, len);
	if (rc) {
		pr_err("spmi write failed: addr=%03X, rc=%d\n", addr, rc);
		return rc;
	}

	return rc;
}

static inline bool fg_check_sram_access(struct fg_chip *chip)
{
	int rc;
	u8 mem_if_sts;

	rc = fg_read(chip, &mem_if_sts, INT_RT_STS(chip->mem_base), 1);
	if (rc) {
		pr_err("failed to read mem status rc=%d\n", rc);
		return 0;
	}

	return !!(mem_if_sts & BIT(FG_MEM_AVAIL));
}

#define RIF_MEM_ACCESS_REQ	BIT(7)
#define INTF_CTL_BURST		BIT(7)
#define INTF_CTL_WR_EN		BIT(6)
static int fg_config_access(struct fg_chip *chip, bool write,
		bool burst, bool otp)
{
	int rc;
	u8 intf_ctl = 0;

	if (otp) {
		/* Configure OTP access */
		rc = fg_masked_write(chip, chip->mem_base + OTP_CFG1,
				0xFF, 0x00, 1);
		if (rc) {
			pr_err("failed to set OTP cfg\n");
			return -EIO;
		}
	}

	intf_ctl = (write ? INTF_CTL_WR_EN : 0) | (burst ? INTF_CTL_BURST : 0);

	rc = fg_write(chip, &intf_ctl, chip->mem_base + MEM_INTF_CTL, 1);
	if (rc) {
		pr_err("failed to set mem access bit\n");
		return -EIO;
	}

	return rc;
}

static int fg_req_and_wait_access(struct fg_chip *chip, int timeout)
{
	int rc = 0, ret = 0;
	bool tried_again = false;

	if (!fg_check_sram_access(chip)) {
		rc = fg_masked_write(chip, chip->mem_base + MEM_INTF_CFG,
				RIF_MEM_ACCESS_REQ, RIF_MEM_ACCESS_REQ, 1);
		if (rc) {
			pr_err("failed to set mem access bit\n");
			return -EIO;
		}
	}

wait:
	/* Wait for MEM_AVAIL IRQ. */
	ret = wait_for_completion_interruptible_timeout(&chip->sram_access,
			msecs_to_jiffies(timeout));
	/* If we were interrupted wait again one more time. */
	if (ret == -ERESTARTSYS && !tried_again) {
		tried_again = true;
		goto wait;
	} else if (ret <= 0) {
		rc = -ETIMEDOUT;
		pr_err("transaction timed out rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static int fg_set_ram_addr(struct fg_chip *chip, u16 *address)
{
	int rc;

	rc = fg_write(chip, (u8 *) address,
			chip->mem_base + MEM_INTF_ADDR_LSB, 2);
	if (rc) {
		pr_err("spmi write failed: addr=%03X, rc=%d\n",
				chip->mem_base + MEM_INTF_ADDR_LSB, rc);
		return rc;
	}

	return rc;
}

#define BUF_LEN		4
static int fg_mem_read(struct fg_chip *chip, u8 *val, u16 address, int len,
		int offset, bool keep_access)
{
	int rc = 0, user_cnt = 0, total_len = 0;
	u8 *rd_data = val;
	bool otp;
	char str[DEBUG_PRINT_BUFFER_SIZE];

	if (address < RAM_OFFSET)
		otp = 1;

	if (offset > 3) {
		pr_err("offset too large %d\n", offset);
		return -EINVAL;
	}

	user_cnt = atomic_add_return(1, &chip->memif_user_cnt);
	if (fg_debug_mask & FG_MEM_DEBUG_READS)
		pr_info("user_cnt %d\n", user_cnt);
	mutex_lock(&chip->rw_lock);
	if (!fg_check_sram_access(chip)) {
		rc = fg_req_and_wait_access(chip, MEM_IF_TIMEOUT_MS);
		if (rc)
			goto out;
	}

	rc = fg_config_access(chip, 0, (len > 4), otp);
	if (rc)
		goto out;

	rc = fg_set_ram_addr(chip, &address);
	if (rc)
		goto out;

	if (fg_debug_mask & FG_MEM_DEBUG_READS)
		pr_info("length %d addr=%02X keep_access %d\n",
				len, address, keep_access);

	total_len = len;
	while (len > 0) {
		if (!offset) {
			rc = fg_read(chip, rd_data,
					chip->mem_base + MEM_INTF_RD_DATA0,
					(len > BUF_LEN) ? BUF_LEN : len);
		} else {
			rc = fg_read(chip, rd_data,
				chip->mem_base + MEM_INTF_RD_DATA0 + offset,
				BUF_LEN - offset);

			/* manually set address to allow continous reads */
			address += BUF_LEN;

			rc = fg_set_ram_addr(chip, &address);
			if (rc)
				goto out;
		}
		if (rc) {
			pr_err("spmi read failed: addr=%03x, rc=%d\n",
				chip->mem_base + MEM_INTF_RD_DATA0, rc);
			goto out;
		}
		rd_data += (BUF_LEN - offset);
		len -= (BUF_LEN - offset);
		offset = 0;
	}

	if (fg_debug_mask & FG_MEM_DEBUG_READS) {
		fill_string(str, DEBUG_PRINT_BUFFER_SIZE, val, total_len);
		pr_info("data: %s\n", str);
	}

out:
	user_cnt = atomic_sub_return(1, &chip->memif_user_cnt);
	if (fg_debug_mask & FG_MEM_DEBUG_READS)
		pr_info("user_cnt %d\n", user_cnt);

	if (!keep_access && (user_cnt == 0) && !rc) {
		rc = fg_masked_write(chip, chip->mem_base + MEM_INTF_CFG,
				RIF_MEM_ACCESS_REQ, 0, 1);
		if (rc)
			pr_err("failed to set mem access bit\n");
	}

	mutex_unlock(&chip->rw_lock);
	return rc;
}

static int fg_mem_write(struct fg_chip *chip, u8 *val, u16 address,
		unsigned int len, unsigned int offset, bool keep_access)
{
	int rc = 0, user_cnt = 0;
	u8 *wr_data = val;

	if (address < RAM_OFFSET)
		return -EINVAL;

	if (offset > 3)
		return -EINVAL;

	user_cnt = atomic_add_return(1, &chip->memif_user_cnt);
	if (fg_debug_mask & FG_MEM_DEBUG_READS)
		pr_info("user_cnt %d\n", user_cnt);
	mutex_lock(&chip->rw_lock);
	if (!fg_check_sram_access(chip)) {
		rc = fg_req_and_wait_access(chip, MEM_IF_TIMEOUT_MS);
		if (rc)
			goto out;
	}

	rc = fg_config_access(chip, 1, (len > 4), 0);
	if (rc)
		goto out;

	rc = fg_set_ram_addr(chip, &address);
	if (rc)
		goto out;

	if (fg_debug_mask & FG_MEM_DEBUG_WRITES)
		pr_info("length %d addr=%02X\n", len, address);

	while (len > 0) {
		if (offset)
			rc = fg_write(chip, wr_data,
				chip->mem_base + MEM_INTF_WR_DATA0 + offset,
				(len > 4) ? 4 : len);
		else
			rc = fg_write(chip, wr_data,
				chip->mem_base + MEM_INTF_WR_DATA0,
				(len > 4) ? 4 : len);
		if (rc) {
			pr_err("spmi read failed: addr=%03x, rc=%d\n",
				chip->mem_base + MEM_INTF_RD_DATA0, rc);
			goto out;
		}
		if (offset) {
			wr_data += 4-offset;
			if (len >= 4)
				len -= 4-offset;
			else
				len = 0;
		} else {
			wr_data += 4;
			if (len >= 4)
				len -= 4;
			else
				len = 0;
		}
	}

out:
	user_cnt = atomic_sub_return(1, &chip->memif_user_cnt);
	if (fg_debug_mask & FG_MEM_DEBUG_READS)
		pr_info("user_cnt %d\n", user_cnt);
	if (!keep_access && (user_cnt == 0) && !rc) {
		rc = fg_masked_write(chip, chip->mem_base + MEM_INTF_CFG,
				RIF_MEM_ACCESS_REQ, 0, 1);
		if (rc) {
			pr_err("failed to set mem access bit\n");
			rc = -EIO;
		}
	}

	mutex_unlock(&chip->rw_lock);
	return rc;
}

static int fg_mem_masked_write(struct fg_chip *chip, u16 addr,
		u8 mask, u8 val, u8 offset)
{
	int rc = 0;
	u8 reg[4];
	char str[DEBUG_PRINT_BUFFER_SIZE];

	rc = fg_mem_read(chip, reg, addr, 4, 0, 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n", addr, rc);
		return rc;
	}

	reg[offset] &= ~mask;
	reg[offset] |= val & mask;

	str[0] = '\0';
	fill_string(str, DEBUG_PRINT_BUFFER_SIZE, reg, 4);
	pr_debug("Writing %s address %03x, offset %d\n", str, addr, offset);

	rc = fg_mem_write(chip, reg, addr, 4, 0, 0);
	if (rc) {
		pr_err("spmi write failed: addr=%03X, rc=%d\n", addr, rc);
		return rc;
	}

	return rc;
}

#define DEFAULT_CAPACITY 50
static int get_prop_capacity(struct fg_chip *chip)
{
	u8 cap[2];
	int rc, capacity = 0, tries = 0;

	if (!chip->profile_loaded && !chip->use_otp_profile)
		return DEFAULT_CAPACITY;

	while (tries < MAX_TRIES_SOC) {
		rc = fg_read(chip, cap,
				chip->soc_base + SOC_MONOTONIC_SOC, 2);
		if (rc) {
			pr_err("spmi read failed: addr=%03x, rc=%d\n",
				chip->soc_base + SOC_MONOTONIC_SOC, rc);
			return rc;
		}

		if (cap[0] == cap[1])
			break;

		tries++;
	}

	if (tries == MAX_TRIES_SOC) {
		pr_err("shadow registers do not match\n");
		return -EINVAL;
	}

	if (cap[0] > 0)
		capacity = (cap[0] * 100 / FULL_PERCENT);

	if (fg_debug_mask & FG_POWER_SUPPLY)
		pr_info("capacity: %d, raw: 0x%02x\n", capacity, cap[0]);
	return capacity;
}

#define DEFAULT_TEMP_DEGC	250
#define SRAM_DATA_DELAY_MS	1000
static int get_sram_prop_now(struct fg_chip *chip, unsigned int type)
{
	if (fg_debug_mask & FG_POWER_SUPPLY)
		pr_info("addr 0x%02X, offset %d value %d\n",
			fg_data[type].address, fg_data[type].offset,
			fg_data[type].value);

	cancel_delayed_work_sync(
		&chip->update_sram_data);
	schedule_delayed_work(
		&chip->update_sram_data, msecs_to_jiffies(SRAM_DATA_DELAY_MS));

	return fg_data[type].value;
}

#define MIN_TEMP_DEGC	-300
#define MAX_TEMP_DEGC	970
static int get_prop_jeita_temp(struct fg_chip *chip, unsigned int type)
{
	if (fg_debug_mask & FG_POWER_SUPPLY)
		pr_info("addr 0x%02X, offset %d\n", settings[type].address,
			settings[type].offset);

	return settings[type].value;
}

static int set_prop_jeita_temp(struct fg_chip *chip,
				unsigned int type, int decidegc)
{
	int rc = 0;

	if (fg_debug_mask & FG_POWER_SUPPLY)
		pr_info("addr 0x%02X, offset %d temp%d\n",
			settings[type].address,
			settings[type].offset, decidegc);

	settings[type].value = decidegc;

	cancel_delayed_work_sync(
		&chip->update_jeita_setting);
	schedule_delayed_work(
		&chip->update_jeita_setting, 0);

	return rc;
}

#define LSB_16B		153
#define TEMP_LSB_16B	625
#define DECIKELVIN	2730
#define SRAM_PERIOD_UPDATE_MS	30000
static void update_sram_data(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				update_sram_data.work);
	int i, rc = 0;
	u8 reg[2];
	s16 temp;

	for (i = 0; i < FG_DATA_MAX; i++) {
		rc = fg_mem_read(chip, reg, fg_data[i].address,
			fg_data[i].len, fg_data[i].offset,
			(i+1 == FG_DATA_MAX) ? 0 : 1);
		if (rc) {
			pr_err("Failed ro update sram data\n");
			break;
		}

		temp = reg[0] | (reg[1] << 8);

		switch (i) {
		case FG_DATA_BATT_TEMP:
			fg_data[i].value = (temp * TEMP_LSB_16B / 1000)
				- DECIKELVIN;
			break;
		case FG_DATA_OCV:
		case FG_DATA_VOLTAGE:
			fg_data[i].value = ((u16) temp) * LSB_16B;
			break;
		case FG_DATA_CURRENT:
			fg_data[i].value = temp * LSB_16B;
			break;
		};

		if (fg_debug_mask & FG_MEM_DEBUG_READS)
			pr_info("%d %d %d\n", i, temp, fg_data[i].value);
	}

	schedule_delayed_work(
		&chip->update_sram_data,
		msecs_to_jiffies(SRAM_PERIOD_UPDATE_MS));
}

static void update_jeita_setting(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				update_jeita_setting.work);
	u8 reg[4];
	int i, rc;

	for (i = 0; i < 4; i++)
		reg[i] = (settings[FG_MEM_SOFT_COLD + i].value / 10) + 30;

	rc = fg_mem_write(chip, reg, settings[FG_MEM_SOFT_COLD].address,
			4, settings[FG_MEM_SOFT_COLD].offset, 0);
	if (rc)
		pr_err("failed to update JEITA setting rc=%d\n", rc);
}

static enum power_supply_property fg_power_props[] = {
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_COOL_TEMP,
	POWER_SUPPLY_PROP_WARM_TEMP,
};

static int fg_power_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct fg_chip *chip = container_of(psy, struct fg_chip, bms_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = get_prop_capacity(chip);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = get_sram_prop_now(chip, FG_DATA_CURRENT);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = get_sram_prop_now(chip, FG_DATA_VOLTAGE);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		val->intval = get_sram_prop_now(chip, FG_DATA_OCV);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = get_sram_prop_now(chip, FG_DATA_BATT_TEMP);
		break;
	case POWER_SUPPLY_PROP_COOL_TEMP:
		val->intval = get_prop_jeita_temp(chip, FG_MEM_SOFT_COLD);
		break;
	case POWER_SUPPLY_PROP_WARM_TEMP:
		val->intval = get_prop_jeita_temp(chip, FG_MEM_SOFT_HOT);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int fg_power_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct fg_chip *chip = container_of(psy, struct fg_chip, bms_psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_COOL_TEMP:
		rc = set_prop_jeita_temp(chip,
				FG_MEM_SOFT_COLD, val->intval);
		break;
	case POWER_SUPPLY_PROP_WARM_TEMP:
		rc = set_prop_jeita_temp(chip,
				FG_MEM_SOFT_HOT, val->intval);
		break;
	default:
		return -EINVAL;
	};

	return rc;
};

static int fg_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_COOL_TEMP:
	case POWER_SUPPLY_PROP_WARM_TEMP:
		return 1;
	default:
		break;
	}

	return 0;
}

static irqreturn_t fg_mem_avail_irq_handler(int irq, void *_chip)
{
	struct fg_chip *chip = _chip;
	u8 mem_if_sts, reg;
	int rc;

	rc = fg_read(chip, &mem_if_sts, INT_RT_STS(chip->mem_base), 1);
	if (rc) {
		pr_err("failed to read mem status rc=%d\n", rc);
		return 0;
	}

	if (fg_check_sram_access(chip)) {
		if (fg_debug_mask & FG_IRQS)
			pr_info("sram access granted\n");
		if (chip->fast_access) {
			reg = REDO_FIRST_ESTIMATE | RESTART_GO;
			rc = fg_masked_write(chip, chip->soc_base + SOC_RESTART,
				       reg, reg, 1);
			if (rc)
				pr_err("failed to set low latency bit\n");
		}
		complete_all(&chip->sram_access);
	} else {
		if (fg_debug_mask & FG_IRQS)
			pr_info("sram access revoked\n");
		INIT_COMPLETION(chip->sram_access);
	}

	if (!rc && (fg_debug_mask & FG_IRQS))
		pr_info("mem_if sts 0x%02x\n", mem_if_sts);

	return IRQ_HANDLED;
}

static irqreturn_t fg_soc_irq_handler(int irq, void *_chip)
{
	struct fg_chip *chip = _chip;
	u8 soc_rt_sts;
	int rc;

	rc = fg_read(chip, &soc_rt_sts, INT_RT_STS(chip->soc_base), 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				INT_RT_STS(chip->soc_base), rc);
	}

	if (fg_debug_mask & FG_IRQS)
		pr_info("triggered 0x%x\n", soc_rt_sts);

	power_supply_changed(&chip->bms_psy);
	return IRQ_HANDLED;
}

static irqreturn_t fg_first_soc_irq_handler(int irq, void *_chip)
{
	struct fg_chip *chip = _chip;

	if (fg_debug_mask & FG_IRQS)
		pr_info("triggered\n");

	power_supply_changed(&chip->bms_psy);
	return IRQ_HANDLED;
}

#define OF_READ_SETTING(type, qpnp_dt_property, retval, optional)	\
do {									\
	if (retval)							\
		break;							\
									\
	retval = of_property_read_u32(chip->spmi->dev.of_node,		\
					"qcom," qpnp_dt_property,	\
					&settings[type].value);		\
									\
	if ((retval == -EINVAL) && optional)				\
		retval = 0;						\
	else if (retval)						\
		pr_err("Error reading " #qpnp_dt_property		\
				" property rc = %d\n", rc);		\
} while (0)

#define LOW_LATENCY	BIT(6)
static int fg_batt_profile_init(struct fg_chip *chip)
{
	int rc = 0;
	int len;
	struct device_node *node = chip->spmi->dev.of_node;
	struct device_node *batt_node;
	const char *data;
	u8 reg = 0;

	batt_node = of_find_node_by_name(node, "qcom,battery-data");
	if (!batt_node) {
		pr_warn("No available batterydata, using OTP defaults\n");
		return 0;
	}

	for_each_child_of_node(batt_node, node) {
		data = of_get_property(node, "qcom,fg-profile-data", &len);
		if (!data) {
			pr_err("no battery profile loaded\n");
			return 0;
		} else {
			break;
		}
	}

	chip->batt_profile = devm_kzalloc(chip->dev,
			sizeof(char) * len, GFP_KERNEL);
	if (!chip->batt_profile)
		return -ENOMEM;

	memcpy(chip->batt_profile, data, len);

	chip->batt_profile_len = len;

	if (fg_debug_mask & FG_MEM_DEBUG_WRITES)
		print_hex_dump(KERN_ERR, "profile: ", DUMP_PREFIX_NONE, 16, 1,
			chip->batt_profile, chip->batt_profile_len, false);

	reg = NO_OTP_PROF_RELOAD;
	rc = fg_masked_write(chip, chip->soc_base + SOC_BOOT_MOD, reg, reg, 1);
	if (rc) {
		pr_err("failed to set low latency access bit\n");
		return -EIO;
	}

	reg = LOW_LATENCY;
	rc = fg_write(chip, &reg, chip->mem_base + MEM_INTF_CTL, 1);
	if (rc) {
		pr_err("failed to set low latency access bit\n");
		return -EIO;
	}
	chip->fast_access = true;

	rc = fg_mem_write(chip, chip->batt_profile, 0x4C0,
			chip->batt_profile_len, 0, 1);
	if (rc)
		pr_err("failed to write profile rc=%d\n", rc);

	rc = fg_mem_masked_write(chip, 0x53C, 0x1, 0x1, 0);
	if (rc)
		pr_err("failed to write profile rc=%d\n", rc);

	reg = 0;
	rc = fg_write(chip, &reg, chip->mem_base + MEM_INTF_CTL, 1);
	if (rc) {
		pr_err("failed to set low latency access bit\n");
		return -EIO;
	}

	reg = 0;
	rc = fg_write(chip, &reg, chip->soc_base + SOC_RESTART, 1);
	if (rc) {
		pr_err("failed to set low latency access bit\n");
		return -EIO;
	}

	chip->fast_access = false;

	/* wait for 2 seconds prior to restart the fuel gauge */
	msleep(2000);
	reg = 0x19;
	rc = fg_write(chip, &reg, chip->soc_base + SOC_RESTART, 1);
	if (rc) {
		pr_err("failed to set low latency access bit\n");
		return -EIO;
	}

	chip->profile_loaded = true;
	return rc;
}

static void batt_profile_init(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				batt_profile_init);

	if (fg_batt_profile_init(chip))
		pr_err("failed to update JEITA setting\n");
}

static int fg_of_init(struct fg_chip *chip)
{
	int rc = 0;

	OF_READ_SETTING(FG_MEM_SOFT_HOT, "warm-bat-decidegc", rc, 1);
	OF_READ_SETTING(FG_MEM_SOFT_COLD, "cool-bat-decidegc", rc, 1);
	OF_READ_SETTING(FG_MEM_HARD_HOT, "hot-bat-decidegc", rc, 1);
	OF_READ_SETTING(FG_MEM_HARD_COLD, "cold-bat-decidegc", rc, 1);

	/* Get the use-otp-profile property */
	chip->use_otp_profile = of_property_read_bool(
			chip->spmi->dev.of_node,
			"qcom,use-otp-profile");

	return rc;
}

static int fg_init_irqs(struct fg_chip *chip)
{
	int rc = 0;
	struct resource *resource;
	struct spmi_resource *spmi_resource;
	u8 subtype;
	struct spmi_device *spmi = chip->spmi;

	spmi_for_each_container_dev(spmi_resource, spmi) {
		if (!spmi_resource) {
			pr_err("fg: spmi resource absent\n");
			return rc;
		}

		resource = spmi_get_resource(spmi, spmi_resource,
						IORESOURCE_MEM, 0);
		if (!(resource && resource->start)) {
			pr_err("node %s IO resource absent!\n",
				spmi->dev.of_node->full_name);
			return rc;
		}

		if ((resource->start == chip->vbat_adc_addr) ||
				(resource->start == chip->ibat_adc_addr))
			continue;

		rc = fg_read(chip, &subtype,
				resource->start + REG_OFFSET_PERP_SUBTYPE, 1);
		if (rc) {
			pr_err("Peripheral subtype read failed rc=%d\n", rc);
			return rc;
		}

		switch (subtype) {
		case FG_SOC:
			chip->soc_irq[FULL_SOC].irq = spmi_get_irq_byname(
					chip->spmi, spmi_resource, "full-soc");
			if (chip->soc_irq[FULL_SOC].irq < 0) {
				pr_err("Unable to get full-soc irq\n");
				return rc;
			}
			chip->soc_irq[EMPTY_SOC].irq = spmi_get_irq_byname(
					chip->spmi, spmi_resource, "empty-soc");
			if (chip->soc_irq[EMPTY_SOC].irq < 0) {
				pr_err("Unable to get low-soc irq\n");
				return rc;
			}
			chip->soc_irq[DELTA_SOC].irq = spmi_get_irq_byname(
					chip->spmi, spmi_resource, "delta-soc");
			if (chip->soc_irq[DELTA_SOC].irq < 0) {
				pr_err("Unable to get delta-soc irq\n");
				return rc;
			}
			chip->soc_irq[FIRST_EST_DONE].irq = spmi_get_irq_byname(
				chip->spmi, spmi_resource, "first-est-done");
			if (chip->soc_irq[FIRST_EST_DONE].irq < 0) {
				pr_err("Unable to get first-est-done irq\n");
				return rc;
			}

			rc |= devm_request_irq(chip->dev,
				chip->soc_irq[FULL_SOC].irq,
				fg_soc_irq_handler, IRQF_TRIGGER_RISING,
				"full-soc", chip);
			if (rc < 0) {
				pr_err("Can't request %d full-soc: %d\n",
					chip->soc_irq[FULL_SOC].irq, rc);
				return rc;
			}
			rc |= devm_request_irq(chip->dev,
				chip->soc_irq[EMPTY_SOC].irq,
				fg_soc_irq_handler, IRQF_TRIGGER_RISING,
				"empty-soc", chip);
			if (rc < 0) {
				pr_err("Can't request %d empty-soc: %d\n",
					chip->soc_irq[EMPTY_SOC].irq, rc);
				return rc;
			}
			rc |= devm_request_irq(chip->dev,
				chip->soc_irq[DELTA_SOC].irq,
				fg_soc_irq_handler, IRQF_TRIGGER_RISING,
				"delta-soc", chip);
			if (rc < 0) {
				pr_err("Can't request %d delta-soc: %d\n",
					chip->soc_irq[DELTA_SOC].irq, rc);
				return rc;
			}
			rc |= devm_request_irq(chip->dev,
				chip->soc_irq[FIRST_EST_DONE].irq,
				fg_first_soc_irq_handler, IRQF_TRIGGER_RISING,
				"first-est-done", chip);
			if (rc < 0) {
				pr_err("Can't request %d delta-soc: %d\n",
					chip->soc_irq[FIRST_EST_DONE].irq, rc);
				return rc;
			}

			enable_irq_wake(chip->soc_irq[FULL_SOC].irq);
			enable_irq_wake(chip->soc_irq[EMPTY_SOC].irq);
			break;
		case FG_MEMIF:
			chip->mem_irq[FG_MEM_AVAIL].irq = spmi_get_irq_byname(
					chip->spmi, spmi_resource, "mem-avail");
			if (chip->mem_irq[FG_MEM_AVAIL].irq < 0) {
				pr_err("Unable to get mem-avail irq\n");
				return rc;
			}
			rc |= devm_request_irq(chip->dev,
					chip->mem_irq[FG_MEM_AVAIL].irq,
					fg_mem_avail_irq_handler,
					IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING,
					"mem-avail", chip);
			if (rc < 0) {
				pr_err("Can't request %d mem-avail: %d\n",
					chip->mem_irq[FG_MEM_AVAIL].irq, rc);
				return rc;
			}
			break;
		case FG_BATT:
		case FG_ADC:
			break;
		default:
			pr_err("subtype %d\n", subtype);
			return -EINVAL;
		}
	}

	return rc;
}

static int fg_remove(struct spmi_device *spmi)
{
	struct fg_chip *chip = dev_get_drvdata(&spmi->dev);

	mutex_destroy(&chip->rw_lock);
	cancel_delayed_work_sync(&chip->update_jeita_setting);
	cancel_work_sync(&chip->batt_profile_init);
	power_supply_unregister(&chip->bms_psy);
	dev_set_drvdata(&spmi->dev, NULL);
	return 0;
}

static int fg_memif_data_open(struct inode *inode, struct file *file)
{
	struct fg_log_buffer *log;
	struct fg_trans *trans;

	size_t logbufsize = SZ_4K;

	if (!dbgfs_data.chip) {
		pr_err("Not initialized data\n");
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
	trans->cnt = dbgfs_data.cnt;
	trans->addr = dbgfs_data.addr;
	trans->chip = dbgfs_data.chip;
	trans->offset = trans->addr;

	file->private_data = trans;
	return 0;
}

static int fg_memif_dfs_close(struct inode *inode, struct file *file)
{
	struct fg_trans *trans = file->private_data;

	if (trans && trans->log) {
		file->private_data = NULL;
		kfree(trans->log);
		kfree(trans);
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
static int
write_next_line_to_log(struct fg_trans *trans, int offset, size_t *pcnt)
{
	int i, j, rc = 0;
	u8 data[ITEMS_PER_LINE];
	struct fg_log_buffer *log = trans->log;

	int cnt = 0;
	int padding = offset % ITEMS_PER_LINE;
	int items_to_read = min(ARRAY_SIZE(data) - padding, *pcnt);
	int items_to_log = min(ITEMS_PER_LINE, padding + items_to_read);

	/* Buffer needs enough space for an entire line */
	if ((log->len - log->wpos) < MAX_LINE_LENGTH)
		goto done;

	/* Read the desired number of "items" */
	rc = fg_mem_read(trans->chip, data, offset, items_to_read, 0,
				*pcnt > items_to_read ? 1 : 0);
	if (rc)
		return -EINVAL;

	*pcnt -= items_to_read;

	/* Each line starts with the aligned offset (12-bit address) */
	cnt = print_to_log(log, "%3.3X ", offset & 0xfff);
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
 * get_log_data - reads data from SRAM and saves to the log buffer
 * @trans: Pointer to SRAM transaction data.
 *
 * Returns the number of "items" read or SPMI error code for read failures.
 */
static int get_log_data(struct fg_trans *trans)
{
	int cnt;
	int last_cnt;
	int items_read;
	int total_items_read = 0;
	u32 offset = trans->offset;
	size_t item_cnt = trans->cnt;
	struct fg_log_buffer *log = trans->log;

	if (item_cnt == 0)
		return 0;

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
 * fg_memif_dfs_reg_read: reads value(s) from SRAM and fills user's buffer a
 *  byte array (coded as string)
 * @file: file pointer
 * @buf: where to put the result
 * @count: maximum space available in @buf
 * @ppos: starting position
 * @return number of user bytes read, or negative error value
 */
static ssize_t fg_memif_dfs_reg_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	struct fg_trans *trans = file->private_data;
	struct fg_log_buffer *log = trans->log;
	size_t ret;
	size_t len;

	/* Is the the log buffer empty */
	if (log->rpos >= log->wpos) {
		if (get_log_data(trans) <= 0)
			return 0;
	}

	len = min(count, log->wpos - log->rpos);

	ret = copy_to_user(buf, &log->data[log->rpos], len);
	if (ret == len) {
		pr_err("error copy SPMI register values to user\n");
		return -EFAULT;
	}

	/* 'ret' is the number of bytes not copied */
	len -= ret;

	*ppos += len;
	log->rpos += len;
	return len;
}

/**
 * fg_memif_dfs_reg_write: write user's byte array (coded as string) to SRAM.
 * @file: file pointer
 * @buf: user data to be written.
 * @count: maximum space available in @buf
 * @ppos: starting position
 * @return number of user byte written, or negative error value
 */
static ssize_t fg_memif_dfs_reg_write(struct file *file, const char __user *buf,
			size_t count, loff_t *ppos)
{
	int bytes_read;
	int data;
	int pos = 0;
	int cnt = 0;
	u8  *values;
	size_t ret = 0;

	struct fg_trans *trans = file->private_data;
	u32 offset = trans->offset;

	/* Make a copy of the user data */
	char *kbuf = kmalloc(count + 1, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

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

	pr_info("address %x, count %d\n", offset, cnt);
	/* Perform the write(s) */

	ret = fg_mem_write(trans->chip, values, offset,
				cnt, 0, 0);
	if (ret) {
		pr_err("SPMI write failed, err = %zu\n", ret);
	} else {
		ret = count;
		trans->offset += cnt > 4 ? 4 : cnt;
	}

free_buf:
	kfree(kbuf);
	return ret;
}

static const struct file_operations fg_memif_dfs_reg_fops = {
	.open		= fg_memif_data_open,
	.release	= fg_memif_dfs_close,
	.read		= fg_memif_dfs_reg_read,
	.write		= fg_memif_dfs_reg_write,
};

/**
 * fg_dfs_create_fs: create debugfs file system.
 * @return pointer to root directory or NULL if failed to create fs
 */
static struct dentry *fg_dfs_create_fs(void)
{
	struct dentry *root, *file;

	pr_debug("Creating FG_MEM debugfs file-system\n");
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
 * fg_dfs_get_root: return a pointer to FG debugfs root directory.
 * @return a pointer to the existing directory, or if no root
 * directory exists then create one. Directory is created with file that
 * configures SRAM transaction, namely: address, and count.
 * @returns valid pointer on success or NULL
 */
struct dentry *fg_dfs_get_root(void)
{
	if (dbgfs_data.root)
		return dbgfs_data.root;

	if (mutex_lock_interruptible(&dbgfs_data.lock) < 0)
		return NULL;
	/* critical section */
	if (!dbgfs_data.root) { /* double checking idiom */
		dbgfs_data.root = fg_dfs_create_fs();
	}
	mutex_unlock(&dbgfs_data.lock);
	return dbgfs_data.root;
}

/*
 * fg_dfs_create: adds new fg_mem if debugfs entry
 * @return zero on success
 */
int fg_dfs_create(struct fg_chip *chip)
{
	struct dentry *root;
	struct dentry *file;

	root = fg_dfs_get_root();
	if (!root)
		return -ENOENT;

	dbgfs_data.chip = chip;

	file = debugfs_create_u32("count", DFS_MODE, root, &(dbgfs_data.cnt));
	if (!file) {
		pr_err("error creating 'count' entry\n");
		goto err_remove_fs;
	}

	file = debugfs_create_x32("address", DFS_MODE,
			root, &(dbgfs_data.addr));
	if (!file) {
		pr_err("error creating 'address' entry\n");
		goto err_remove_fs;
	}

	file = debugfs_create_file("data", DFS_MODE, root, &dbgfs_data,
							&fg_memif_dfs_reg_fops);
	if (!file) {
		pr_err("error creating 'data' entry\n");
		goto err_remove_fs;
	}

	return 0;

err_remove_fs:
	debugfs_remove_recursive(root);
	return -ENOMEM;
}

#define INIT_JEITA_DELAY_MS 1000
static int fg_probe(struct spmi_device *spmi)
{
	struct device *dev = &(spmi->dev);
	struct fg_chip *chip;
	struct spmi_resource *spmi_resource;
	struct resource *resource;
	u8 subtype;
	int rc = 0;

	if (!spmi->dev.of_node) {
		pr_err("device node missing\n");
		return -ENODEV;
	}

	if (!spmi) {
		pr_err("no valid spmi pointer\n");
		return -ENODEV;
	}

	chip = devm_kzalloc(dev, sizeof(struct fg_chip), GFP_KERNEL);
	if (chip == NULL) {
		pr_err("Can't allocate fg_chip\n");
		return -ENOMEM;
	}

	chip->spmi = spmi;
	chip->dev = &(spmi->dev);

	mutex_init(&chip->rw_lock);
	INIT_DELAYED_WORK(&chip->update_jeita_setting,
			update_jeita_setting);
	INIT_DELAYED_WORK(&chip->update_sram_data, update_sram_data);
	INIT_WORK(&chip->batt_profile_init,
			batt_profile_init);
	init_completion(&chip->sram_access);

	spmi_for_each_container_dev(spmi_resource, spmi) {
		if (!spmi_resource) {
			pr_err("qpnp_chg: spmi resource absent\n");
			rc = -ENXIO;
			goto of_init_fail;
		}

		resource = spmi_get_resource(spmi, spmi_resource,
						IORESOURCE_MEM, 0);
		if (!(resource && resource->start)) {
			pr_err("node %s IO resource absent!\n",
				spmi->dev.of_node->full_name);
			rc = -ENXIO;
			goto of_init_fail;
		}

		if (strcmp("qcom,fg-adc-vbat",
					spmi_resource->of_node->name) == 0) {
			chip->vbat_adc_addr = resource->start;
			continue;
		} else if (strcmp("qcom,fg-adc-ibat",
					spmi_resource->of_node->name) == 0) {
			chip->ibat_adc_addr = resource->start;
			continue;
		}

		rc = fg_read(chip, &subtype,
				resource->start + REG_OFFSET_PERP_SUBTYPE, 1);
		if (rc) {
			pr_err("Peripheral subtype read failed rc=%d\n", rc);
			goto of_init_fail;
		}

		switch (subtype) {
		case FG_SOC:
			chip->soc_base = resource->start;
			break;
		case FG_MEMIF:
			chip->mem_base = resource->start;
			break;
		default:
			pr_err("Invalid peripheral subtype=0x%x\n", subtype);
			rc = -EINVAL;
		}
	}

	rc = fg_of_init(chip);
	if (rc) {
		pr_err("failed to parse devicetree rc%d\n", rc);
		goto of_init_fail;
	}

	chip->bms_psy.name = "bms";
	chip->bms_psy.type = POWER_SUPPLY_TYPE_BMS;
	chip->bms_psy.properties = fg_power_props;
	chip->bms_psy.num_properties = ARRAY_SIZE(fg_power_props);
	chip->bms_psy.get_property = fg_power_get_property;
	chip->bms_psy.set_property = fg_power_set_property;
	chip->bms_psy.supplied_to = fg_supplicants;
	chip->bms_psy.num_supplicants = ARRAY_SIZE(fg_supplicants);
	chip->bms_psy.property_is_writeable = fg_property_is_writeable;

	rc = power_supply_register(chip->dev, &chip->bms_psy);
	if (rc < 0) {
		pr_err("batt failed to register rc = %d\n", rc);
		goto of_init_fail;
	}

	rc = fg_init_irqs(chip);
	if (rc) {
		pr_err("failed to request interrupts %d\n", rc);
		goto power_supply_unregister;
	}

	if (chip->mem_base) {
		rc = fg_dfs_create(chip);
		if (rc < 0) {
			pr_err("failed to create debugfs rc = %d\n", rc);
			goto power_supply_unregister;
		}
	}

	schedule_delayed_work(
		&chip->update_jeita_setting,
		msecs_to_jiffies(INIT_JEITA_DELAY_MS));
	if (!chip->use_otp_profile)
		schedule_work(&chip->batt_profile_init);

	pr_info("probe success\n");

	return rc;

power_supply_unregister:
	power_supply_unregister(&chip->bms_psy);
of_init_fail:
	mutex_destroy(&chip->rw_lock);
	return rc;
}

static struct spmi_driver fg_driver = {
	.driver		= {
		.name	= QPNP_FG_DEV_NAME,
		.of_match_table	= fg_match_table,
	},
	.probe		= fg_probe,
	.remove		= fg_remove,
};

static int __init fg_init(void)
{
	return spmi_driver_register(&fg_driver);
}

static void __exit fg_exit(void)
{
	return spmi_driver_unregister(&fg_driver);
}

module_init(fg_init);
module_exit(fg_exit);

MODULE_DESCRIPTION("QPNP Fuel Gauge Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" QPNP_FG_DEV_NAME);
