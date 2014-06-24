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

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/spmi.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/power_supply.h>

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

#define REG_OFFSET_PERP_SUBTYPE	0x05

/* RAM register offsets */
#define RAM_OFFSET		0x400

/* Bit/Mask definitions */
#define FULL_PERCENT		0xFF
#define MAX_TRIES_SOC		5
#define MA_MV_BIT_RES		39
#define MSB_SIGN		BIT(7)
#define IBAT_VBAT_MASK		0x7F

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

/* FG_MEMIF setting index */
enum fg_mem_setting_index {
	FG_MEM_JEITA_SOFT_COLD,
	FG_MEM_JEITA_SOFT_HOT,
	FG_MEM_JEITA_HARD_COLD,
	FG_MEM_JEITA_HARD_HOT,
	FG_MEM_SETTING_MAX,
};

#define SETTING(_idx, _address, _offset, _value)	\
	[FG_MEM_##_idx] = {				\
		.address = _address,			\
		.offset = _offset,			\
		.value = _value,			\
	}						\

static struct fg_mem_setting settings[FG_MEM_SETTING_MAX] = {
	/*       ID                    Address, Offset, Value*/
	SETTING(JEITA_SOFT_COLD,       0x454,   0,      100),
	SETTING(JEITA_SOFT_HOT,        0x454,   1,      400),
	SETTING(JEITA_HARD_COLD,       0x454,   2,      50),
	SETTING(JEITA_HARD_HOT,        0x454,   3,      450),
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
	struct fg_irq		soc_irq[FG_SOC_IRQ_COUNT];
	struct fg_irq		batt_irq[FG_BATT_IRQ_COUNT];
	struct fg_irq		mem_irq[FG_MEM_IF_IRQ_COUNT];
	struct delayed_work	update_jeita_setting;
	struct mutex		rw_lock;
	struct completion	sram_access;
	struct power_supply	bms_psy;
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
		bool burst)
{
	int rc;
	u8 intf_ctl = 0;

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

static int fg_mem_write(struct fg_chip *chip, u8 *val, u16 address,
		unsigned int len, unsigned int offset, bool keep_access)
{
	int rc = 0;
	u8 *wr_data = val;

	if (address < RAM_OFFSET)
		return -EINVAL;

	if (offset > 3)
		return -EINVAL;

	if (!fg_check_sram_access(chip)) {
		rc = fg_req_and_wait_access(chip, MEM_IF_TIMEOUT_MS);
		if (rc)
			return rc;
	}

	mutex_lock(&chip->rw_lock);
	rc = fg_config_access(chip, 1, (len > 4));
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

	if (!keep_access) {
		rc = fg_masked_write(chip, chip->mem_base + MEM_INTF_CFG,
				RIF_MEM_ACCESS_REQ, 0, 1);
		if (rc) {
			pr_err("failed to set mem access bit\n");
			rc = -EIO;
		}
	}

out:
	mutex_unlock(&chip->rw_lock);
	return 0;
}

static int get_prop_capacity(struct fg_chip *chip)
{
	u8 cap[2];
	int rc, capacity = 0, tries = 0;

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

static int get_prop_current_now(struct fg_chip *chip)
{
	s8 ibat_ma;
	int rc, current_now_ua;

	if (!chip->ibat_adc_addr)
		return 0;

	rc = fg_read(chip, &ibat_ma, chip->ibat_adc_addr, 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03x, rc=%d\n",
			chip->ibat_adc_addr, rc);
		return rc;
	}

	/* convert to uA */
	current_now_ua = MA_MV_BIT_RES * ibat_ma * 1000;

	if (fg_debug_mask & FG_POWER_SUPPLY)
		pr_info("current %d uA\n", current_now_ua);
	return current_now_ua;
}

static int get_prop_voltage_now(struct fg_chip *chip)
{
	s8 vbat_mv;
	int rc, voltage_now_uv;

	if (!chip->vbat_adc_addr)
		return 0;

	rc = fg_read(chip, &vbat_mv, chip->vbat_adc_addr, 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03x, rc=%d\n",
			chip->vbat_adc_addr, rc);
		return rc;
	}

	/* convert to uV */
	voltage_now_uv = MA_MV_BIT_RES * vbat_mv * 1000;

	if (vbat_mv & MSB_SIGN)
		voltage_now_uv *= -1;

	if (fg_debug_mask & FG_POWER_SUPPLY)
		pr_info("voltage %d uV\n", voltage_now_uv);

	return voltage_now_uv;
}

#define MIN_TEMP_DEGC	-300
#define MAX_TEMP_DEGC	970
static int get_prop_jeita_temp(struct fg_chip *chip, unsigned int type)
{
	return settings[type].value;
}

static int set_prop_jeita_temp(struct fg_chip *chip,
				unsigned int type, int decidegc)
{
	int rc = 0;

	pr_debug("addr 0x%02X, offset %d temp%d\n", settings[type].address,
			settings[type].offset, decidegc);

	settings[type].value = decidegc;

	cancel_delayed_work_sync(
		&chip->update_jeita_setting);
	schedule_delayed_work(
		&chip->update_jeita_setting, 0);

	return rc;
}

static void update_jeita_setting(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				update_jeita_setting.work);
	u8 reg[4];
	int i, rc;

	for (i = 0; i < 4; i++)
		reg[i] = (settings[JEITA_SOFT_COLD + i].value / 10) + 30;

	rc = fg_mem_write(chip, reg, settings[JEITA_SOFT_COLD].address, 4,
			settings[JEITA_SOFT_COLD].offset, 0);
	if (rc)
		pr_err("failed to update JEITA setting rc=%d\n", rc);
}

static enum power_supply_property fg_power_props[] = {
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
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
		val->intval = get_prop_current_now(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = get_prop_voltage_now(chip);
		break;
	case POWER_SUPPLY_PROP_COOL_TEMP:
		val->intval = get_prop_jeita_temp(chip, FG_MEM_JEITA_SOFT_COLD);
		break;
	case POWER_SUPPLY_PROP_WARM_TEMP:
		val->intval = get_prop_jeita_temp(chip, FG_MEM_JEITA_SOFT_HOT);
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
				FG_MEM_JEITA_SOFT_COLD, val->intval);
		break;
	case POWER_SUPPLY_PROP_WARM_TEMP:
		rc = set_prop_jeita_temp(chip,
				FG_MEM_JEITA_SOFT_HOT, val->intval);
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
	u8 mem_if_sts;
	int rc;

	rc = fg_read(chip, &mem_if_sts, INT_RT_STS(chip->mem_base), 1);
	if (rc) {
		pr_err("failed to read mem status rc=%d\n", rc);
		return 0;
	}

	if (fg_check_sram_access(chip)) {
		if (fg_debug_mask & FG_IRQS)
			pr_info("sram access granted\n");
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

static int fg_of_init(struct fg_chip *chip)
{
	int rc = 0;

	OF_READ_SETTING(FG_MEM_JEITA_SOFT_HOT, "warm-bat-decidegc", rc, 1);
	OF_READ_SETTING(FG_MEM_JEITA_SOFT_COLD, "cool-bat-decidegc", rc, 1);
	OF_READ_SETTING(FG_MEM_JEITA_HARD_HOT, "hot-bat-decidegc", rc, 1);
	OF_READ_SETTING(FG_MEM_JEITA_HARD_COLD, "cold-bat-decidegc", rc, 1);

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
	power_supply_unregister(&chip->bms_psy);
	dev_set_drvdata(&spmi->dev, NULL);
	return 0;
}

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
	init_completion(&chip->sram_access);

	rc = fg_of_init(chip);
	if (rc) {
		pr_err("failed to parse devicetree rc%d\n", rc);
		goto of_init_fail;
	}

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

	pr_info("probe success SOC %d\n", get_prop_capacity(chip));

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
