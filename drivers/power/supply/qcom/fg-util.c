// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2019 The Linux Foundation. All rights reserved.
 */

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/sort.h>
#include "fg-core.h"
#include "fg-reg.h"

/* 3 byte address + 1 space character */
#define ADDR_LEN			4
/* Format is 'XX ' */
#define CHARS_PER_ITEM			3
/* 4 data items per line */
#define ITEMS_PER_LINE			4
#define MAX_LINE_LENGTH			(ADDR_LEN + (ITEMS_PER_LINE *	\
					CHARS_PER_ITEM) + 1)		\

#define MAX_READ_TRIES		5

#define VOLTAGE_24BIT_MSB_MASK	GENMASK(27, 16)
#define VOLTAGE_24BIT_LSB_MASK	GENMASK(11, 0)
int fg_decode_voltage_24b(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int value)
{
	int msb, lsb, val;

	msb = value & VOLTAGE_24BIT_MSB_MASK;
	lsb = value & VOLTAGE_24BIT_LSB_MASK;
	val = (msb >> 4) | lsb;
	sp[id].value = div_s64((s64)val * sp[id].denmtr, sp[id].numrtr);
	pr_debug("id: %d raw value: %x decoded value: %x\n", id, value,
			sp[id].value);
	return sp[id].value;
}

#define VOLTAGE_15BIT_MASK	GENMASK(14, 0)
int fg_decode_voltage_15b(struct fg_sram_param *sp,
				enum fg_sram_param_id id, int value)
{
	value &= VOLTAGE_15BIT_MASK;
	sp[id].value = div_u64((u64)value * sp[id].denmtr, sp[id].numrtr);
	pr_debug("id: %d raw value: %x decoded value: %x\n", id, value,
		sp[id].value);
	return sp[id].value;
}

#define CURRENT_24BIT_MSB_MASK	GENMASK(27, 16)
#define CURRENT_24BIT_LSB_MASK	GENMASK(11, 0)
int fg_decode_current_24b(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int value)
{
	int msb, lsb, val;

	msb = value & CURRENT_24BIT_MSB_MASK;
	lsb = value & CURRENT_24BIT_LSB_MASK;
	val = (msb >> 4) | lsb;
	val = sign_extend32(val, 23);
	sp[id].value = div_s64((s64)val * sp[id].denmtr, sp[id].numrtr);
	pr_debug("id: %d raw value: %x decoded value: %x\n", id, value,
			sp[id].value);
	return sp[id].value;
}

int fg_decode_current_16b(struct fg_sram_param *sp,
				enum fg_sram_param_id id, int value)
{
	value = sign_extend32(value, 15);
	sp[id].value = div_s64((s64)value * sp[id].denmtr, sp[id].numrtr);
	pr_debug("id: %d raw value: %x decoded value: %d\n", id, value,
		sp[id].value);
	return sp[id].value;
}

int fg_decode_cc_soc(struct fg_sram_param *sp,
				enum fg_sram_param_id id, int value)
{
	sp[id].value = div_s64((s64)value * sp[id].denmtr, sp[id].numrtr);
	sp[id].value = sign_extend32(sp[id].value, 31);
	pr_debug("id: %d raw value: %x decoded value: %x\n", id, value,
		sp[id].value);
	return sp[id].value;
}

int fg_decode_value_16b(struct fg_sram_param *sp,
				enum fg_sram_param_id id, int value)
{
	sp[id].value = div_u64((u64)(u16)value * sp[id].denmtr, sp[id].numrtr);
	pr_debug("id: %d raw value: %x decoded value: %x\n", id, value,
		sp[id].value);
	return sp[id].value;
}

int fg_decode_default(struct fg_sram_param *sp, enum fg_sram_param_id id,
				int value)
{
	sp[id].value = value;
	return sp[id].value;
}

int fg_decode(struct fg_sram_param *sp, enum fg_sram_param_id id,
			int value)
{
	if (!sp[id].decode) {
		pr_err("No decoding function for parameter %d\n", id);
		return -EINVAL;
	}

	return sp[id].decode(sp, id, value);
}

void fg_encode_voltage(struct fg_sram_param *sp,
				enum fg_sram_param_id  id, int val_mv, u8 *buf)
{
	int i, mask = 0xff;
	int64_t temp;

	val_mv += sp[id].offset;
	temp = (int64_t)div_u64((u64)val_mv * sp[id].numrtr, sp[id].denmtr);
	pr_debug("temp: %llx id: %d, val_mv: %d, buf: [ ", temp, id, val_mv);
	for (i = 0; i < sp[id].len; i++) {
		buf[i] = temp & mask;
		temp >>= 8;
		pr_debug("%x ", buf[i]);
	}
	pr_debug("]\n");
}

void fg_encode_current(struct fg_sram_param *sp,
				enum fg_sram_param_id  id, int val_ma, u8 *buf)
{
	int i, mask = 0xff;
	int64_t temp;
	s64 current_ma;

	current_ma = val_ma;
	temp = (int64_t)div_s64(current_ma * sp[id].numrtr, sp[id].denmtr);
	pr_debug("temp: %llx id: %d, val: %d, buf: [ ", temp, id, val_ma);
	for (i = 0; i < sp[id].len; i++) {
		buf[i] = temp & mask;
		temp >>= 8;
		pr_debug("%x ", buf[i]);
	}
	pr_debug("]\n");
}

void fg_encode_default(struct fg_sram_param *sp,
				enum fg_sram_param_id  id, int val, u8 *buf)
{
	int i, mask = 0xff;
	int64_t temp;

	temp = (int64_t)div_s64((s64)val * sp[id].numrtr, sp[id].denmtr);
	pr_debug("temp: %llx id: %d, val: %d, buf: [ ", temp, id, val);
	for (i = 0; i < sp[id].len; i++) {
		buf[i] = temp & mask;
		temp >>= 8;
		pr_debug("%x ", buf[i]);
	}
	pr_debug("]\n");
}

void fg_encode(struct fg_sram_param *sp, enum fg_sram_param_id id,
			int val, u8 *buf)
{
	if (!sp[id].encode) {
		pr_err("No encoding function for parameter %d\n", id);
		return;
	}

	sp[id].encode(sp, id, val, buf);
}

/*
 * Please make sure *_sram_params table has the entry for the parameter
 * obtained through this function. In addition to address, offset,
 * length from where this SRAM parameter is read, a decode function
 * need to be specified.
 */
int fg_get_sram_prop(struct fg_dev *fg, enum fg_sram_param_id id,
				int *val)
{
	int temp, rc, i;
	u8 buf[4];

	if (id < 0 || id > FG_SRAM_MAX || fg->sp[id].len > sizeof(buf))
		return -EINVAL;

	if (fg->battery_missing)
		return 0;

	rc = fg_sram_read(fg, fg->sp[id].addr_word, fg->sp[id].addr_byte,
		buf, fg->sp[id].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error reading address %d[%d] rc=%d\n",
			fg->sp[id].addr_word, fg->sp[id].addr_byte, rc);
		return rc;
	}

	for (i = 0, temp = 0; i < fg->sp[id].len; i++)
		temp |= buf[i] << (8 * i);

	*val = fg_decode(fg->sp, id, temp);
	return 0;
}

void fg_circ_buf_add(struct fg_circ_buf *buf, int val)
{
	buf->arr[buf->head] = val;
	buf->head = (buf->head + 1) % ARRAY_SIZE(buf->arr);
	buf->size = min(++buf->size, (int)ARRAY_SIZE(buf->arr));
}

void fg_circ_buf_clr(struct fg_circ_buf *buf)
{
	buf->size = 0;
	buf->head = 0;
	memset(buf->arr, 0, sizeof(buf->arr));
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

static int cmp_int(const void *a, const void *b)
{
	return *(int *)a - *(int *)b;
}

int fg_circ_buf_median(struct fg_circ_buf *buf, int *median)
{
	int *temp;

	if (buf->size == 0)
		return -ENODATA;

	if (buf->size == 1) {
		*median = buf->arr[0];
		return 0;
	}

	temp = kmalloc_array(buf->size, sizeof(*temp), GFP_KERNEL);
	if (!temp)
		return -ENOMEM;

	memcpy(temp, buf->arr, buf->size * sizeof(*temp));
	sort(temp, buf->size, sizeof(*temp), cmp_int, NULL);

	if (buf->size % 2)
		*median = temp[buf->size / 2];
	else
		*median = (temp[buf->size / 2 - 1] + temp[buf->size / 2]) / 2;

	kfree(temp);
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

bool usb_psy_initialized(struct fg_dev *fg)
{
	if (fg->usb_psy)
		return true;

	fg->usb_psy = power_supply_get_by_name("usb");
	if (!fg->usb_psy)
		return false;

	return true;
}

static bool is_usb_present(struct fg_dev *fg)
{
	union power_supply_propval pval = {0, };
	int rc;

	if (!usb_psy_initialized(fg))
		return false;

	rc = power_supply_get_property(fg->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &pval);
	if (rc < 0)
		return false;

	return pval.intval != 0;
}

bool dc_psy_initialized(struct fg_dev *fg)
{
	if (fg->dc_psy)
		return true;

	fg->dc_psy = power_supply_get_by_name("dc");
	if (!fg->dc_psy)
		return false;

	return true;
}

static bool is_dc_present(struct fg_dev *fg)
{
	union power_supply_propval pval = {0, };
	int rc;

	if (!dc_psy_initialized(fg))
		return false;

	rc = power_supply_get_property(fg->dc_psy,
			POWER_SUPPLY_PROP_PRESENT, &pval);
	if (rc < 0)
		return false;

	return pval.intval != 0;
}

bool is_input_present(struct fg_dev *fg)
{
	return is_usb_present(fg) || is_dc_present(fg);
}

void fg_notify_charger(struct fg_dev *fg)
{
	union power_supply_propval prop = {0, };
	int rc;

	if (!fg->batt_psy)
		return;

	if (!fg->profile_available)
		return;

	if (fg->bp.float_volt_uv > 0) {
		prop.intval = fg->bp.float_volt_uv;
		rc = power_supply_set_property(fg->batt_psy,
				POWER_SUPPLY_PROP_VOLTAGE_MAX, &prop);
		if (rc < 0) {
			pr_err("Error in setting voltage_max property on batt_psy, rc=%d\n",
				rc);
			return;
		}
	}

	if (fg->bp.fastchg_curr_ma > 0) {
		prop.intval = fg->bp.fastchg_curr_ma * 1000;
		rc = power_supply_set_property(fg->batt_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
				&prop);
		if (rc < 0) {
			pr_err("Error in setting constant_charge_current_max property on batt_psy, rc=%d\n",
				rc);
			return;
		}
	}
}

bool batt_psy_initialized(struct fg_dev *fg)
{
	if (fg->batt_psy)
		return true;

	fg->batt_psy = power_supply_get_by_name("battery");
	if (!fg->batt_psy)
		return false;

	/* batt_psy is initialized, set the fcc and fv */
	fg_notify_charger(fg);

	return true;
}

bool is_qnovo_en(struct fg_dev *fg)
{
	union power_supply_propval pval = {0, };
	int rc;

	if (!batt_psy_initialized(fg))
		return false;

	rc = power_supply_get_property(fg->batt_psy,
			POWER_SUPPLY_PROP_CHARGE_QNOVO_ENABLE, &pval);
	if (rc < 0)
		return false;

	return pval.intval != 0;
}

bool pc_port_psy_initialized(struct fg_dev *fg)
{
	if (fg->pc_port_psy)
		return true;

	fg->pc_port_psy = power_supply_get_by_name("pc_port");
	if (!fg->pc_port_psy)
		return false;

	return true;
}

bool is_parallel_charger_available(struct fg_dev *fg)
{
	if (!fg->parallel_psy)
		fg->parallel_psy = power_supply_get_by_name("parallel");

	if (!fg->parallel_psy)
		return false;

	return true;
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

void dump_sram(struct fg_dev *fg, u8 *buf, int addr, int len)
{
	int i;
	char str[16];

	/*
	 * Length passed should be in multiple of 4 as each GEN3 FG SRAM word
	 * holds 4 bytes and GEN4 FG SRAM word holds 2 bytes. To keep this
	 * simple, even if a length which is not a multiple of 4 bytes or less
	 * than 4 bytes is passed, SRAM registers dumped will be always in
	 * multiple of 4 bytes.
	 */
	for (i = 0; i < len; i += 4) {
		str[0] = '\0';
		fill_string(str, sizeof(str), buf + i, 4);

		/*
		 * We still print 4 bytes per line. However, the address
		 * should be incremented by 2 for GEN4 FG as each word holds
		 * 2 bytes.
		 */
		if (fg->version == GEN3_FG)
			pr_info("%03d %s\n", addr + (i / 4), str);
		else
			pr_info("%03d %s\n", addr + (i / 2), str);
	}
}

static inline bool fg_sram_address_valid(struct fg_dev *fg, u16 address,
					int len)
{
	if (address > fg->sram.address_max || !fg->sram.num_bytes_per_word)
		return false;

	if ((address + DIV_ROUND_UP(len, fg->sram.num_bytes_per_word))
			> fg->sram.address_max + 1)
		return false;

	return true;
}

#define SOC_UPDATE_WAIT_MS	1500
int fg_sram_write(struct fg_dev *fg, u16 address, u8 offset,
			u8 *val, int len, int flags)
{
	int rc = 0, tries = 0;
	bool atomic_access = false;

	if (!fg)
		return -ENXIO;

	if (fg->battery_missing)
		return 0;

	if (!fg_sram_address_valid(fg, address, len))
		return -EFAULT;

	if (!(flags & FG_IMA_NO_WLOCK))
		vote(fg->awake_votable, SRAM_WRITE, true, 0);

	if (flags & FG_IMA_ATOMIC)
		atomic_access = true;

	/* With DMA granted, SRAM transaction is already atomic */
	if (fg->use_dma)
		atomic_access = false;

	mutex_lock(&fg->sram_rw_lock);

	if (atomic_access && fg->irqs[SOC_UPDATE_IRQ].irq) {
		/*
		 * This interrupt need to be enabled only when it is
		 * required. It will be kept disabled other times.
		 */
		reinit_completion(&fg->soc_update);
		enable_irq(fg->irqs[SOC_UPDATE_IRQ].irq);
	}

	/*
	 * Atomic access mean waiting upon SOC_UPDATE interrupt from
	 * FG_ALG and do the transaction after that. This is to make
	 * sure that there will be no SOC update happening when an
	 * IMA write is happening. SOC_UPDATE interrupt fires every
	 * FG cycle (~1.47 seconds).
	 */
	if (atomic_access) {
		for (tries = 0; tries < 2; tries++) {
			/* Wait for SOC_UPDATE completion */
			rc = wait_for_completion_interruptible_timeout(
				&fg->soc_update,
				msecs_to_jiffies(SOC_UPDATE_WAIT_MS));
			if (rc > 0) {
				rc = 0;
				break;
			} else if (!rc) {
				rc = -ETIMEDOUT;
			}
		}

		if (rc < 0) {
			pr_err("wait for soc_update timed out rc=%d\n", rc);
			goto out;
		}
	}

	if (fg->use_dma)
		rc = fg_direct_mem_write(fg, address, offset, val, len,
				false);
	else
		rc = fg_interleaved_mem_write(fg, address, offset, val, len,
				atomic_access);

	if (rc < 0)
		pr_err("Error in writing SRAM address 0x%x[%d], rc=%d\n",
			address, offset, rc);

out:
	if (atomic_access && fg->irqs[SOC_UPDATE_IRQ].irq)
		disable_irq_nosync(fg->irqs[SOC_UPDATE_IRQ].irq);

	mutex_unlock(&fg->sram_rw_lock);
	if (!(flags & FG_IMA_NO_WLOCK))
		vote(fg->awake_votable, SRAM_WRITE, false, 0);
	return rc;
}

int fg_sram_read(struct fg_dev *fg, u16 address, u8 offset,
			u8 *val, int len, int flags)
{
	int rc = 0;

	if (!fg)
		return -ENXIO;

	if (fg->battery_missing)
		return 0;

	if (!fg_sram_address_valid(fg, address, len))
		return -EFAULT;

	if (!(flags & FG_IMA_NO_WLOCK))
		vote(fg->awake_votable, SRAM_READ, true, 0);

	mutex_lock(&fg->sram_rw_lock);

	if (fg->use_dma)
		rc = fg_direct_mem_read(fg, address, offset, val, len);
	else
		rc = fg_interleaved_mem_read(fg, address, offset, val, len);

	if (rc < 0)
		pr_err("Error in reading SRAM address 0x%x[%d], rc=%d\n",
			address, offset, rc);

	mutex_unlock(&fg->sram_rw_lock);
	if (!(flags & FG_IMA_NO_WLOCK))
		vote(fg->awake_votable, SRAM_READ, false, 0);
	return rc;
}

int fg_sram_masked_write(struct fg_dev *fg, u16 address, u8 offset,
			u8 mask, u8 val, int flags)
{
	int rc = 0, length = 4;
	u8 buf[4];

	if (fg->version == GEN4_FG)
		length = 2;

	rc = fg_sram_read(fg, address, 0, buf, length, flags);
	if (rc < 0) {
		pr_err("sram read failed: address=%03X, rc=%d\n", address, rc);
		return rc;
	}

	buf[offset] &= ~mask;
	buf[offset] |= val & mask;

	rc = fg_sram_write(fg, address, 0, buf, length, flags);
	if (rc < 0) {
		pr_err("sram write failed: address=%03X, rc=%d\n", address, rc);
		return rc;
	}

	return rc;
}

int fg_read(struct fg_dev *fg, int addr, u8 *val, int len)
{
	int rc, i;

	if (!fg || !fg->regmap)
		return -ENXIO;

	rc = regmap_bulk_read(fg->regmap, addr, val, len);

	if (rc < 0) {
		dev_err(fg->dev, "regmap_read failed for address %04x rc=%d\n",
			addr, rc);
		return rc;
	}

	if (*fg->debug_mask & FG_BUS_READ) {
		pr_info("length %d addr=%04x\n", len, addr);
		for (i = 0; i < len; i++)
			pr_info("val[%d]: %02x\n", i, val[i]);
	}

	return 0;
}

static inline bool is_sec_access(struct fg_dev *fg, int addr)
{
	if (fg->version != GEN3_FG)
		return false;

	return ((addr & 0x00FF) > 0xD0);
}

int fg_write(struct fg_dev *fg, int addr, u8 *val, int len)
{
	int rc, i;

	if (!fg || !fg->regmap)
		return -ENXIO;

	mutex_lock(&fg->bus_lock);
	if (is_sec_access(fg, addr)) {
		rc = regmap_write(fg->regmap, (addr & 0xFF00) | 0xD0, 0xA5);
		if (rc < 0) {
			dev_err(fg->dev, "regmap_write failed for address %x rc=%d\n",
				addr, rc);
			goto out;
		}
	}

	if (len > 1)
		rc = regmap_bulk_write(fg->regmap, addr, val, len);
	else
		rc = regmap_write(fg->regmap, addr, *val);

	if (rc < 0) {
		dev_err(fg->dev, "regmap_write failed for address %04x rc=%d\n",
			addr, rc);
		goto out;
	}

	if (*fg->debug_mask & FG_BUS_WRITE) {
		pr_info("length %d addr=%04x\n", len, addr);
		for (i = 0; i < len; i++)
			pr_info("val[%d]: %02x\n", i, val[i]);
	}
out:
	mutex_unlock(&fg->bus_lock);
	return rc;
}

int fg_masked_write(struct fg_dev *fg, int addr, u8 mask, u8 val)
{
	int rc;

	if (!fg || !fg->regmap)
		return -ENXIO;

	mutex_lock(&fg->bus_lock);
	if (is_sec_access(fg, addr)) {
		rc = regmap_write(fg->regmap, (addr & 0xFF00) | 0xD0, 0xA5);
		if (rc < 0) {
			dev_err(fg->dev, "regmap_write failed for address %x rc=%d\n",
				addr, rc);
			goto out;
		}
	}

	rc = regmap_update_bits(fg->regmap, addr, mask, val);
	if (rc < 0) {
		dev_err(fg->dev, "regmap_update_bits failed for address %04x rc=%d\n",
			addr, rc);
		goto out;
	}

	fg_dbg(fg, FG_BUS_WRITE, "addr=%04x mask: %02x val: %02x\n", addr,
		mask, val);
out:
	mutex_unlock(&fg->bus_lock);
	return rc;
}

int fg_dump_regs(struct fg_dev *fg)
{
	int i, rc;
	u8 buf[256];

	if (!fg)
		return -EINVAL;

	rc = fg_read(fg, fg->batt_soc_base, buf, sizeof(buf));
	if (rc < 0)
		return rc;

	pr_info("batt_soc_base registers:\n");
	for (i = 0; i < sizeof(buf); i++)
		pr_info("%04x:%02x\n", fg->batt_soc_base + i, buf[i]);

	rc = fg_read(fg, fg->mem_if_base, buf, sizeof(buf));
	if (rc < 0)
		return rc;

	pr_info("mem_if_base registers:\n");
	for (i = 0; i < sizeof(buf); i++)
		pr_info("%04x:%02x\n", fg->mem_if_base + i, buf[i]);

	return 0;
}

int fg_restart(struct fg_dev *fg, int wait_time_ms)
{
	union power_supply_propval pval = {0, };
	int rc;
	bool tried_again = false;

	if (!fg->fg_psy)
		return -ENODEV;

	rc = power_supply_get_property(fg->fg_psy, POWER_SUPPLY_PROP_CAPACITY,
					&pval);
	if (rc < 0) {
		pr_err("Error in getting capacity, rc=%d\n", rc);
		return rc;
	}

	fg->last_soc = pval.intval;
	fg->fg_restarting = true;
	reinit_completion(&fg->soc_ready);
	rc = fg_masked_write(fg, BATT_SOC_RESTART(fg), RESTART_GO_BIT,
			RESTART_GO_BIT);
	if (rc < 0) {
		pr_err("Error in writing to %04x, rc=%d\n",
			BATT_SOC_RESTART(fg), rc);
		goto out;
	}

wait:
	rc = wait_for_completion_interruptible_timeout(&fg->soc_ready,
		msecs_to_jiffies(wait_time_ms));

	/* If we were interrupted wait again one more time. */
	if (rc == -ERESTARTSYS && !tried_again) {
		tried_again = true;
		goto wait;
	} else if (rc <= 0) {
		pr_err("wait for soc_ready timed out rc=%d\n", rc);
	}

	rc = fg_masked_write(fg, BATT_SOC_RESTART(fg), RESTART_GO_BIT, 0);
	if (rc < 0) {
		pr_err("Error in writing to %04x, rc=%d\n",
			BATT_SOC_RESTART(fg), rc);
		goto out;
	}
out:
	fg->fg_restarting = false;
	return rc;
}

/* All fg_get_* , fg_set_* functions here */

int fg_get_msoc_raw(struct fg_dev *fg, int *val)
{
	u8 cap[2];
	int rc, tries = 0;

	while (tries < MAX_READ_TRIES) {
		rc = fg_read(fg, BATT_SOC_FG_MONOTONIC_SOC(fg), cap, 2);
		if (rc < 0) {
			pr_err("failed to read addr=0x%04x, rc=%d\n",
				BATT_SOC_FG_MONOTONIC_SOC(fg), rc);
			return rc;
		}

		if (cap[0] == cap[1])
			break;

		tries++;
	}

	if (tries == MAX_READ_TRIES) {
		pr_err("MSOC: shadow registers do not match\n");
		return -EINVAL;
	}

	fg_dbg(fg, FG_POWER_SUPPLY, "raw: 0x%02x\n", cap[0]);
	*val = cap[0];
	return 0;
}

int fg_get_msoc(struct fg_dev *fg, int *msoc)
{
	int rc;

	rc = fg_get_msoc_raw(fg, msoc);
	if (rc < 0)
		return rc;

	/*
	 * To have better endpoints for 0 and 100, it is good to tune the
	 * calculation discarding values 0 and 255 while rounding off. Rest
	 * of the values 1-254 will be scaled to 1-99. DIV_ROUND_UP will not
	 * be suitable here as it rounds up any value higher than 252 to 100.
	 */
	if (*msoc == FULL_SOC_RAW)
		*msoc = 100;
	else if (*msoc == 0)
		*msoc = 0;
	else
		*msoc = DIV_ROUND_CLOSEST((*msoc - 1) * (FULL_CAPACITY - 2),
				FULL_SOC_RAW - 2) + 1;
	return 0;
}

#define DEFAULT_BATT_TYPE	"Unknown Battery"
#define MISSING_BATT_TYPE	"Missing Battery"
#define LOADING_BATT_TYPE	"Loading Battery"
#define SKIP_BATT_TYPE		"Skipped loading battery"
const char *fg_get_battery_type(struct fg_dev *fg)
{
	switch (fg->profile_load_status) {
	case PROFILE_MISSING:
		return DEFAULT_BATT_TYPE;
	case PROFILE_SKIPPED:
		return SKIP_BATT_TYPE;
	case PROFILE_LOADED:
		if (fg->bp.batt_type_str)
			return fg->bp.batt_type_str;
		break;
	case PROFILE_NOT_LOADED:
		return MISSING_BATT_TYPE;
	default:
		break;
	};

	if (fg->battery_missing)
		return MISSING_BATT_TYPE;

	if (fg->profile_available)
		return LOADING_BATT_TYPE;

	return DEFAULT_BATT_TYPE;
}

int fg_get_battery_resistance(struct fg_dev *fg, int *val)
{
	int rc, esr_uohms, rslow_uohms;

	rc = fg_get_sram_prop(fg, FG_SRAM_ESR, &esr_uohms);
	if (rc < 0) {
		pr_err("failed to get ESR, rc=%d\n", rc);
		return rc;
	}

	rc = fg_get_sram_prop(fg, FG_SRAM_RSLOW, &rslow_uohms);
	if (rc < 0) {
		pr_err("failed to get Rslow, rc=%d\n", rc);
		return rc;
	}

	*val = esr_uohms + rslow_uohms;
	return 0;
}

#define BATT_CURRENT_NUMR	488281
#define BATT_CURRENT_DENR	1000
int fg_get_battery_current(struct fg_dev *fg, int *val)
{
	int rc = 0, tries = 0;
	int64_t temp = 0;
	u8 buf[2], buf_cp[2];

	while (tries++ < MAX_READ_TRIES) {
		rc = fg_read(fg, BATT_INFO_IBATT_LSB(fg), buf, 2);
		if (rc < 0) {
			pr_err("failed to read addr=0x%04x, rc=%d\n",
				BATT_INFO_IBATT_LSB(fg), rc);
			return rc;
		}

		rc = fg_read(fg, BATT_INFO_IBATT_LSB_CP(fg), buf_cp, 2);
		if (rc < 0) {
			pr_err("failed to read addr=0x%04x, rc=%d\n",
				BATT_INFO_IBATT_LSB_CP(fg), rc);
			return rc;
		}

		if (buf[0] == buf_cp[0] && buf[1] == buf_cp[1])
			break;
	}

	if (tries == MAX_READ_TRIES) {
		pr_err("IBATT: shadow registers do not match\n");
		return -EINVAL;
	}

	if (fg->wa_flags & PMI8998_V1_REV_WA)
		temp = buf[0] << 8 | buf[1];
	else
		temp = buf[1] << 8 | buf[0];

	pr_debug("buf: %x %x temp: %llx\n", buf[0], buf[1], temp);
	/* Sign bit is bit 15 */
	temp = sign_extend32(temp, 15);
	*val = div_s64((s64)temp * BATT_CURRENT_NUMR, BATT_CURRENT_DENR);
	return 0;
}

#define BATT_VOLTAGE_NUMR	122070
#define BATT_VOLTAGE_DENR	1000
int fg_get_battery_voltage(struct fg_dev *fg, int *val)
{
	int rc = 0, tries = 0;
	u16 temp = 0;
	u8 buf[2], buf_cp[2];

	while (tries++ < MAX_READ_TRIES) {
		rc = fg_read(fg, BATT_INFO_VBATT_LSB(fg), buf, 2);
		if (rc < 0) {
			pr_err("failed to read addr=0x%04x, rc=%d\n",
				BATT_INFO_VBATT_LSB(fg), rc);
			return rc;
		}

		rc = fg_read(fg, BATT_INFO_VBATT_LSB_CP(fg), buf_cp, 2);
		if (rc < 0) {
			pr_err("failed to read addr=0x%04x, rc=%d\n",
				BATT_INFO_VBATT_LSB_CP(fg), rc);
			return rc;
		}

		if (buf[0] == buf_cp[0] && buf[1] == buf_cp[1])
			break;
	}

	if (tries == MAX_READ_TRIES) {
		pr_err("VBATT: shadow registers do not match\n");
		return -EINVAL;
	}

	if (fg->wa_flags & PMI8998_V1_REV_WA)
		temp = buf[0] << 8 | buf[1];
	else
		temp = buf[1] << 8 | buf[0];

	pr_debug("buf: %x %x temp: %x\n", buf[0], buf[1], temp);
	*val = div_u64((u64)temp * BATT_VOLTAGE_NUMR, BATT_VOLTAGE_DENR);
	return 0;
}

int fg_set_constant_chg_voltage(struct fg_dev *fg, int volt_uv)
{
	u8 buf[2];
	int rc;

	if (volt_uv <= 0 || volt_uv > 15590000) {
		pr_err("Invalid voltage %d\n", volt_uv);
		return -EINVAL;
	}

	fg_encode(fg->sp, FG_SRAM_VBATT_FULL, volt_uv, buf);

	rc = fg_sram_write(fg, fg->sp[FG_SRAM_VBATT_FULL].addr_word,
		fg->sp[FG_SRAM_VBATT_FULL].addr_byte, buf,
		fg->sp[FG_SRAM_VBATT_FULL].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing vbatt_full, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

int fg_set_esr_timer(struct fg_dev *fg, int cycles_init,
				int cycles_max, bool charging, int flags)
{
	u8 buf[2];
	int rc, timer_max, timer_init;

	if (cycles_init < 0 || cycles_max < 0)
		return 0;

	if (charging) {
		timer_max = FG_SRAM_ESR_TIMER_CHG_MAX;
		timer_init = FG_SRAM_ESR_TIMER_CHG_INIT;
	} else {
		timer_max = FG_SRAM_ESR_TIMER_DISCHG_MAX;
		timer_init = FG_SRAM_ESR_TIMER_DISCHG_INIT;
	}

	fg_encode(fg->sp, timer_max, cycles_max, buf);
	rc = fg_sram_write(fg,
			fg->sp[timer_max].addr_word,
			fg->sp[timer_max].addr_byte, buf,
			fg->sp[timer_max].len, flags);
	if (rc < 0) {
		pr_err("Error in writing esr_timer_dischg_max, rc=%d\n",
			rc);
		return rc;
	}

	fg_encode(fg->sp, timer_init, cycles_init, buf);
	rc = fg_sram_write(fg,
			fg->sp[timer_init].addr_word,
			fg->sp[timer_init].addr_byte, buf,
			fg->sp[timer_init].len, flags);
	if (rc < 0) {
		pr_err("Error in writing esr_timer_dischg_init, rc=%d\n",
			rc);
		return rc;
	}

	fg_dbg(fg, FG_STATUS, "esr_%s_timer set to %d/%d\n",
		charging ? "charging" : "discharging", cycles_init, cycles_max);
	return 0;
}

static int fg_get_irq_index_byname(struct fg_dev *fg, const char *name,
					int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (!fg->irqs[i].name)
			continue;

		if (strcmp(fg->irqs[i].name, name) == 0)
			return i;
	}

	pr_err("%s is not in irq list\n", name);
	return -ENOENT;
}

int fg_register_interrupts(struct fg_dev *fg, int size)
{
	struct device_node *child, *node = fg->dev->of_node;
	struct property *prop;
	const char *name;
	int rc, irq, irq_index;

	for_each_available_child_of_node(node, child) {
		of_property_for_each_string(child, "interrupt-names", prop,
						name) {
			irq = of_irq_get_byname(child, name);
			if (irq < 0) {
				dev_err(fg->dev, "failed to get irq %s irq:%d\n",
					name, irq);
				return irq;
			}

			irq_index = fg_get_irq_index_byname(fg, name, size);
			if (irq_index < 0)
				return irq_index;

			rc = devm_request_threaded_irq(fg->dev, irq, NULL,
					fg->irqs[irq_index].handler,
					IRQF_ONESHOT, name, fg);
			if (rc < 0) {
				dev_err(fg->dev, "failed to register irq handler for %s rc:%d\n",
					name, rc);
				return rc;
			}

			fg->irqs[irq_index].irq = irq;
			if (fg->irqs[irq_index].wakeable)
				enable_irq_wake(fg->irqs[irq_index].irq);
		}
	}

	return 0;
}

void fg_unregister_interrupts(struct fg_dev *fg, void *data, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (fg->irqs[i].irq)
			devm_free_irq(fg->dev, fg->irqs[i].irq, data);
	}

}

/* All the debugfs related functions are defined below */

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

static int fg_sram_dfs_open(struct inode *inode, struct file *file)
{
	struct fg_log_buffer *log;
	struct fg_trans *trans;
	u8 *data_buf;

	size_t logbufsize = SZ_4K;
	size_t databufsize = SZ_4K;

	if (!dbgfs_data.fg) {
		pr_err("Not initialized data\n");
		return -EINVAL;
	}

	/* Per file "transaction" data */
	trans = devm_kzalloc(dbgfs_data.fg->dev, sizeof(*trans), GFP_KERNEL);
	if (!trans)
		return -ENOMEM;

	/* Allocate log buffer */
	log = devm_kzalloc(dbgfs_data.fg->dev, logbufsize, GFP_KERNEL);
	if (!log)
		return -ENOMEM;

	log->rpos = 0;
	log->wpos = 0;
	log->len = logbufsize - sizeof(*log);

	/* Allocate data buffer */
	data_buf = devm_kzalloc(dbgfs_data.fg->dev, databufsize, GFP_KERNEL);
	if (!data_buf)
		return -ENOMEM;

	trans->log = log;
	trans->data = data_buf;
	trans->cnt = dbgfs_data.cnt;
	trans->addr = dbgfs_data.addr;
	trans->fg = dbgfs_data.fg;
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
		devm_kfree(trans->fg->dev, trans->log);
		devm_kfree(trans->fg->dev, trans->data);
		devm_kfree(trans->fg->dev, trans);
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

	if (trans->fg->version == GEN4_FG) {
		/*
		 * For GEN4 FG, address is in word and it increments by 1.
		 * Each word holds 2 bytes. To keep the SRAM dump format
		 * compatible, print 4 bytes per line which holds 2 words.
		 */
		address = trans->addr + ((offset - trans->addr) * 2 /
				ITEMS_PER_LINE);
	} else {
		/*
		 * For GEN3 FG, address is in word and it increments by 1.
		 * Each word holds 4 bytes.
		 */
		address = trans->addr + ((offset - trans->addr) /
			ITEMS_PER_LINE);
	}

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
	rc = fg_sram_read(trans->fg, trans->addr, 0,
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

	ret = fg_sram_write(trans->fg, address, 0, values, cnt, 0);
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

static int fg_sram_debugfs_create(struct fg_dev *fg)
{
	struct dentry *dfs_sram;
	struct dentry *file;
	mode_t dfs_mode = 0600;

	pr_debug("Creating FG_SRAM debugfs file-system\n");
	dfs_sram = debugfs_create_dir("sram", fg->dfs_root);
	if (!dfs_sram) {
		pr_err("error creating fg sram dfs rc=%ld\n",
		       (long)dfs_sram);
		return -ENOMEM;
	}

	dbgfs_data.help_msg.size = strlen(dbgfs_data.help_msg.data);
	file = debugfs_create_blob("help", 0444, dfs_sram,
					&dbgfs_data.help_msg);
	if (!file) {
		pr_err("error creating help entry\n");
		goto err_remove_fs;
	}

	dbgfs_data.fg = fg;

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
	struct fg_dev *fg = file->private_data;
	char buf[512];
	u8 alg_flags = 0;
	int rc, i, len;

	rc = fg_sram_read(fg, fg->sp[FG_SRAM_ALG_FLAGS].addr_word,
			  fg->sp[FG_SRAM_ALG_FLAGS].addr_byte, &alg_flags, 1,
			  FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("failed to read algorithm flags rc=%d\n", rc);
		return -EFAULT;
	}

	len = 0;
	for (i = 0; i < ALG_FLAG_MAX; ++i) {
		if (len > ARRAY_SIZE(buf) - 1)
			return -EFAULT;
		if (fg->alg_flags[i].invalid)
			continue;

		len += snprintf(buf + len, sizeof(buf) - sizeof(*buf) * len,
				"%s = %d\n", fg->alg_flags[i].name,
				(bool)(alg_flags & fg->alg_flags[i].bit));
	}

	return simple_read_from_buffer(userbuf, count, ppos, buf, len);
}

static const struct file_operations fg_alg_flags_fops = {
	.open = fg_alg_flags_open,
	.read = fg_alg_flags_read,
};

/*
 * fg_debugfs_create: adds new fg_sram debugfs entry
 * @return zero on success
 */
int fg_debugfs_create(struct fg_dev *fg)
{
	int rc;
	struct dentry *file;

	pr_debug("Creating debugfs file-system\n");
	fg->dfs_root = debugfs_create_dir("fg", NULL);
	if (IS_ERR_OR_NULL(fg->dfs_root)) {
		if (PTR_ERR(fg->dfs_root) == -ENODEV)
			pr_err("debugfs is not enabled in the kernel\n");
		else
			pr_err("error creating fg dfs root rc=%ld\n",
			       (long)fg->dfs_root);
		return -ENODEV;
	}

	file = debugfs_create_u32("debug_mask", 0600, fg->dfs_root,
			fg->debug_mask);
	if (IS_ERR_OR_NULL(file)) {
		pr_err("failed to create debug_mask\n");
		goto err_remove_fs;
	}

	rc = fg_sram_debugfs_create(fg);
	if (rc < 0) {
		pr_err("failed to create sram dfs rc=%d\n", rc);
		goto err_remove_fs;
	}

	if (fg->alg_flags) {
		if (!debugfs_create_file("alg_flags", 0400, fg->dfs_root, fg,
					 &fg_alg_flags_fops)) {
			pr_err("failed to create alg_flags file\n");
			goto err_remove_fs;
		}
	}

	return 0;

err_remove_fs:
	debugfs_remove_recursive(fg->dfs_root);
	return -ENOMEM;
}
