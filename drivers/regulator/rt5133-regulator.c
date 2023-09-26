/*
 *  Copyright (C) 2021 Mediatek Technology Inc.
 *  Jeff_Chang <jeff_chang@richtek.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/crc8.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/driver.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <mt-plat/rt5133_event.h>

#define GENERIC_DEBUGFS	1

#if GENERIC_DEBUGFS
#include <linux/debugfs.h>
#endif /* GENERIC_DEBUGFS */

#define RT5133_DRV_VERSION	"1.0.2_MTK"

#define RT5133_REG_CHIP_INFO		0x00
#define RT5133_REG_RST_CTRL		0x06
#define RT5133_REG_BASE_CTRL		0x09
#define RT5133_REG_GPIO_CTRL		0x0B
#define RT5133_REG_BASE_EVT		0x10
#define RT5133_REG_BASE_MASK		0x16
#define RT5133_REG_LDO_SHDN		0x19
#define RT5133_REG_LDO1_CTRL1		0x20
#define RT5133_REG_LDO1_CTRL2		0x21
#define RT5133_REG_LDO1_CTRL3		0x22
#define RT5133_REG_LDO2_CTRL1		0x24
#define RT5133_REG_LDO2_CTRL2		0x25
#define RT5133_REG_LDO2_CTRL3		0x26
#define RT5133_REG_LDO3_CTRL1		0x28
#define RT5133_REG_LDO3_CTRL2		0x29
#define RT5133_REG_LDO3_CTRL3		0x2A
#define RT5133_REG_LDO4_CTRL1		0x2C
#define RT5133_REG_LDO4_CTRL2		0x2D
#define RT5133_REG_LDO4_CTRL3		0x2E
#define RT5133_REG_LDO5_CTRL1		0x30
#define RT5133_REG_LDO5_CTRL2		0x31
#define RT5133_REG_LDO5_CTRL3		0x32
#define RT5133_REG_LDO6_CTRL1		0x34
#define RT5133_REG_LDO6_CTRL2		0x35
#define RT5133_REG_LDO6_CTRL3		0x36
#define RT5133_REG_LDO7_CTRL1		0x38
#define RT5133_REG_LDO7_CTRL2		0x39
#define RT5133_REG_LDO7_CTRL3		0x3A
#define RT5133_REG_LDO8_CTRL1		0x3C
#define RT5133_REG_LDO8_CTRL2		0x3D
#define RT5133_REG_LDO8_CTRL3		0x3E
#define RT5133_REG_LDO8_CTRL4		0x3F

#define RT5133_LDO_REG_BASE(_id)	(0x20 + ((_id) - 1) * 4)

#define RT5133_VENDOR_ID_MASK		GENMASK(7, 4)
#define RT5133_VENDOR_ID		0x70
#define RT5133_RESET_CODE		0xB1

#define RT5133_FOFF_BASE_MASK		BIT(1)
#define RT5133_OCSHDN_ALL_MASK		BIT(7)
#define RT5133_PGBSHDN_ALL_MASK		BIT(6)

#define RT5133_OCPTSEL_MASK		BIT(5)
#define RT5133_PGBPTSEL_MASK		BIT(4)
#define RT5133_STBTDSEL_MASK		GENMASK(1, 0)

#define RT5133_LDO_ENABLE_MASK		BIT(7)
#define RT5133_LDO_VSEL_MASK		GENMASK(7, 5)
#define RT5133_LDO_AD_MASK		BIT(2)

#define RT5133_GPIO_NR			3
#define RT5133_GPIOEN_MASK(_id)		(BIT(7 - (_id)) | BIT(3 - (_id)))

#define RT5133_LDO_PGB_EVT_MASK		GENMASK(23, 16)
#define RT5133_LDO_PGB_EVT_SHIFT	16
#define RT5133_LDO_OC_EVT_MASK		GENMASK(15, 8)
#define RT5133_LDO_OC_EVT_SHIFT		8
#define RT5133_VREF_EVT_MASK		BIT(6)
#define RT5133_BASE_EVT_MASK		GENMASK(7, 0)
#define RT5133_INTR_CLR_MASK		GENMASK(23, 0)
#define RT5133_INTR_BYTE_NR		3

#define RT5133_MAX_I2C_BLOCK_SIZE	1

#define RT5133_CRC8_POLYNOMIAL		0x7

#define RT5133_I2C_ADDR_LEN		1
#define RT5133_PREDATA_LEN		2
#define RT5133_I2C_CRC_LEN		1
#define RT5133_REG_ADDR_LEN		1
#define RT5133_I2C_DUMMY_LEN		1

#define I2C_ADDR_XLATE_8BIT(_addr, _rw)	((((_addr) & 0x7F) << 1) | (_rw))

#if GENERIC_DEBUGFS
struct dbg_internal {
	struct dentry *rt_root;
	struct dentry *ic_root;
	bool rt_dir_create;
	struct mutex io_lock;
	u16 reg;
	u16 size;
	u16 data_buffer_size;
	void *data_buffer;
};

struct dbg_info {
	const char *dirname;
	const char *devname;
	const char *typestr;
	void *io_drvdata;
	int (*io_read)(void *drvdata, u16 reg, void *val, u16 size);
	int (*io_write)(void *drvdata, u16 reg, const void *val, u16 size);
	struct dbg_internal internal;
};
#endif /* GENERIC_DEBUGFS */

enum {
	RT5133_REGULATOR_BASE = 0,
	RT5133_REGULATOR_LDO1,
	RT5133_REGULATOR_LDO2,
	RT5133_REGULATOR_LDO3,
	RT5133_REGULATOR_LDO4,
	RT5133_REGULATOR_LDO5,
	RT5133_REGULATOR_LDO6,
	RT5133_REGULATOR_LDO7,
	RT5133_REGULATOR_LDO8,
	RT5133_REGULATOR_MAX
};

struct rt5133_priv {
	struct device *dev;
	struct regmap *regmap;
	struct gpio_desc *enable_gpio;
	struct regulator_dev *rdev[RT5133_REGULATOR_MAX];
	struct gpio_chip gc;
	unsigned int gpio_output_flag;
	u8 crc8_tbls[CRC8_TABLE_SIZE];
#if GENERIC_DEBUGFS
	struct dbg_info dbg_info;
#endif /* GENERIC_DEBUGFS */
};

static struct regulator *regulator[8];
static struct notifier_block rt5133_nb[8];
static void (*rt5133_callback[RT5133_IRQ_MAX])(void);
static int rt5133_callback_enable[RT5133_IRQ_MAX];

#if GENERIC_DEBUGFS
#ifdef CONFIG_DEBUG_FS
/* reg/size/data/bustype */
#define PREALLOC_RBUFFER_SIZE	(32)
#define PREALLOC_WBUFFER_SIZE	(1000)

static int data_debug_show(struct seq_file *s, void *data)
{
	struct dbg_info *di = s->private;
	struct dbg_internal *d = &di->internal;
	void *buffer;
	u8 *pdata;
	int i, ret;

	if (d->data_buffer_size < d->size) {
		buffer = kzalloc(d->size, GFP_KERNEL);
		if (!buffer)
			return -ENOMEM;
		kfree(d->data_buffer);
		d->data_buffer = buffer;
		d->data_buffer_size = d->size;
	}

	/* read transfer */
	if (!di->io_read)
		return -EPERM;
	ret = di->io_read(di->io_drvdata, d->reg, d->data_buffer, d->size);
	if (ret < 0)
		return ret;

	pdata = d->data_buffer;
	seq_puts(s, "0x");
	for (i = 0; i < d->size; i++)
		seq_printf(s, "%02x,", *(pdata + i));
	seq_puts(s, "\n");

	return 0;
}

static int data_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, data_debug_show, inode->i_private);
}

static ssize_t data_debug_write(struct file *file,
				const char __user *user_buf,
				size_t cnt, loff_t *loff)
{
	struct seq_file *seq = file->private_data;
	struct dbg_info *di = seq->private;
	struct dbg_internal *d = &di->internal;
	void *buffer;
	u8 *pdata;
	char buf[PREALLOC_WBUFFER_SIZE + 1], *token, *cur;
	int val_cnt = 0, ret;

	if (cnt > PREALLOC_WBUFFER_SIZE)
		return -ENOMEM;
	if (copy_from_user(buf, user_buf, cnt))
		return -EFAULT;
	buf[cnt] = 0;

	/* buffer size check */
	if (d->data_buffer_size < d->size) {
		buffer = kzalloc(d->size, GFP_KERNEL);
		if (!buffer)
			return -ENOMEM;
		kfree(d->data_buffer);
		d->data_buffer = buffer;
		d->data_buffer_size = d->size;
	}

	/* data parsing */
	cur = buf;
	pdata = d->data_buffer;
	while ((token = strsep(&cur, ",\n")) != NULL) {
		if (!*token)
			break;
		if (val_cnt++ >= d->size)
			break;
		if (kstrtou8(token, 16, pdata++))
			return -EINVAL;
	}
	if (val_cnt != d->size)
		return -EINVAL;

	/* write transfer */
	if (!di->io_write)
		return -EPERM;
	ret = di->io_write(di->io_drvdata, d->reg, d->data_buffer, d->size);
	return (ret < 0) ? ret : cnt;
}

static const struct file_operations data_debug_fops = {
	.open = data_debug_open,
	.read = seq_read,
	.write = data_debug_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int type_debug_show(struct seq_file *s, void *data)
{
	struct dbg_info *di = s->private;

	seq_printf(s, "%s,%s\n", di->typestr, di->devname);
	return 0;
}

static int type_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, type_debug_show, inode->i_private);
}

static const struct file_operations type_debug_fops = {
	.open = type_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static ssize_t lock_debug_read(struct file *file,
		char __user *user_buf, size_t cnt, loff_t *loff)
{
	struct dbg_info *di = file->private_data;
	struct dbg_internal *d = &di->internal;
	char buf[10];
	int ret;

	ret = snprintf(buf, sizeof(buf), "%d\n", mutex_is_locked(&d->io_lock));
	if (ret < 0)
		return ret;

	return simple_read_from_buffer(user_buf, cnt, loff, buf, strlen(buf));
}

static ssize_t lock_debug_write(struct file *file,
		const char __user *user_buf,
		size_t cnt, loff_t *loff)
{
	struct dbg_info *di = file->private_data;
	struct dbg_internal *d = &di->internal;
	u32 lock;
	int ret;

	ret = kstrtou32_from_user(user_buf, cnt, 0, &lock);
	if (ret < 0)
		return ret;

	lock ? mutex_lock(&d->io_lock) : mutex_unlock(&d->io_lock);
	return cnt;
}

static const struct file_operations lock_debug_fops = {
	.open = simple_open,
	.read = lock_debug_read,
	.write = lock_debug_write,
};

static int generic_debugfs_init(struct dbg_info *di)
{
	struct dbg_internal *d = &di->internal;

	/* valid check */
	if (!di->dirname || !di->devname || !di->typestr)
		return -EINVAL;
	d->data_buffer_size = PREALLOC_RBUFFER_SIZE;
	d->data_buffer = kzalloc(PREALLOC_RBUFFER_SIZE, GFP_KERNEL);
	if (!d->data_buffer)
		return -ENOMEM;

	/* create debugfs */
	d->rt_root = debugfs_lookup("ext_dev_io", NULL);
	if (!d->rt_root) {
		d->rt_root = debugfs_create_dir("ext_dev_io", NULL);
		if (!d->rt_root)
			return -ENODEV;
		d->rt_dir_create = true;
	}

	d->ic_root = debugfs_create_dir(di->dirname, d->rt_root);
	if (!d->ic_root)
		goto err_cleanup_rt;
	if (!debugfs_create_u16("reg", 0644, d->ic_root, &d->reg))
		goto err_cleanup_ic;
	if (!debugfs_create_u16("size", 0644, d->ic_root, &d->size))
		goto err_cleanup_ic;
	if (!debugfs_create_file("data", 0644,
				d->ic_root, di, &data_debug_fops))
		goto err_cleanup_ic;
	if (!debugfs_create_file("type", 0444,
				d->ic_root, di, &type_debug_fops))
		goto err_cleanup_ic;
	if (!debugfs_create_file("lock", 0644,
				d->ic_root, di, &lock_debug_fops))
		goto err_cleanup_ic;
	mutex_init(&d->io_lock);
	return 0;

err_cleanup_ic:
	debugfs_remove_recursive(d->ic_root);
err_cleanup_rt:
	if (d->rt_dir_create)
		debugfs_remove_recursive(d->rt_root);
	kfree(d->data_buffer);
	return -ENODEV;
}

static void generic_debugfs_exit(struct dbg_info *di)
{
	struct dbg_internal *d = &di->internal;

	mutex_destroy(&d->io_lock);
	debugfs_remove_recursive(d->ic_root);
	if (d->rt_dir_create)
		debugfs_remove_recursive(d->rt_root);
	kfree(d->data_buffer);
}
#else
static inline int generic_debugfs_init(struct dbg_info *di)
{
	return 0;
}

static inline void generic_debugfs_exit(struct dbg_info *di) {}
#endif
#endif /* GENERIC_DEBUGFS */

static const unsigned int vout_type1_tables[] = {
	1800000, 2500000, 2700000, 2800000, 2900000, 3000000, 3100000, 3200000
};

static const unsigned int vout_type2_tables[] = {
	1700000, 1800000, 1900000, 2500000, 2700000, 2800000, 2900000, 3000000
};

static const unsigned int vout_type3_tables[] = {
	900000, 950000, 1000000, 1050000, 1100000, 1150000, 1200000, 1800000
};

static const struct regulator_ops rt5133_regulator_ops = {
	.list_voltage = regulator_list_voltage_table,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_active_discharge = regulator_set_active_discharge_regmap,
};

static const struct regulator_ops rt5133_base_regulator_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
};

static int rt5133_of_parse_cb(struct device_node *node,
			      const struct regulator_desc *desc,
			      struct regulator_config *config)
{
	struct rt5133_priv *priv = config->driver_data;
	unsigned int base_addr = 0;

	const struct {
		const char *prop_name;
		unsigned int min;
		unsigned int max;
		unsigned int def_val;
		unsigned int addr_offset;
		unsigned int mask;
	} base_props[] = {
		{ "oc_shutdown_all", 0, 1, 0,
			RT5133_REG_LDO_SHDN, RT5133_OCSHDN_ALL_MASK },
		{ "pgb_shutdown_all", 0, 1, 0,
			RT5133_REG_LDO_SHDN, RT5133_PGBSHDN_ALL_MASK }
	}, ldo_props[] = {
		{ "oc_ptsel", 0, 1, 0, 0, RT5133_OCPTSEL_MASK },
		{ "pgb_ptsel", 0, 1, 0, 0, RT5133_PGBPTSEL_MASK },
		{ "soft_start_time_sel", 0, 3, 1, 0, RT5133_STBTDSEL_MASK }
	}, *props;
	int i, props_size;

	if (desc->id == RT5133_REGULATOR_BASE) {
		props = base_props;
		props_size = ARRAY_SIZE(base_props);
	} else {
		props = ldo_props;
		props_size = ARRAY_SIZE(ldo_props);
		base_addr = RT5133_LDO_REG_BASE(desc->id);
	}

	for (i = 0; i < props_size; i++) {
		int shift = ffs(props[i].mask) - 1, ret;
		unsigned int val = 0;

		ret = of_property_read_u32(node, props[i].prop_name, &val);
		if (ret)
			val = props[i].def_val;

		if (val > props[i].max)
			val = props[i].max;

		ret = regmap_update_bits(priv->regmap,
					 base_addr + props[i].addr_offset,
					 props[i].mask, val << shift);
		if (ret)
			return ret;
	}

	return 0;
}

#define RT5133_REGULATOR_DESC(_name, vtables, _supply) \
{\
	.name = #_name,\
	.id = RT5133_REGULATOR_##_name,\
	.of_match = of_match_ptr(#_name),\
	.regulators_node = of_match_ptr("regulators"),\
	.supply_name = _supply,\
	.of_parse_cb = rt5133_of_parse_cb,\
	.type = REGULATOR_VOLTAGE,\
	.owner = THIS_MODULE,\
	.ops = &rt5133_regulator_ops,\
	.n_voltages = ARRAY_SIZE(vtables),\
	.volt_table = vtables,\
	.enable_reg = RT5133_REG_##_name##_CTRL1,\
	.enable_mask = RT5133_LDO_ENABLE_MASK,\
	.vsel_reg = RT5133_REG_##_name##_CTRL2,\
	.vsel_mask = RT5133_LDO_VSEL_MASK,\
	.active_discharge_reg = RT5133_REG_##_name##_CTRL3,\
	.active_discharge_mask = RT5133_LDO_AD_MASK,\
}

static const struct regulator_desc rt5133_regulators[] = {
	/* For digital part, base current control */
	{
		.name = "rt5133,base",
		.id = RT5133_REGULATOR_BASE,
		.of_match = of_match_ptr("BASE"),
		.regulators_node = of_match_ptr("regulators"),
		.of_parse_cb = rt5133_of_parse_cb,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
		.ops = &rt5133_base_regulator_ops,
		.enable_reg = RT5133_REG_BASE_CTRL,
		.enable_mask = RT5133_FOFF_BASE_MASK,
		.enable_is_inverted = true,
	},
	RT5133_REGULATOR_DESC(LDO1, vout_type1_tables, "rt5133,base"),
	RT5133_REGULATOR_DESC(LDO2, vout_type1_tables, "rt5133,base"),
	RT5133_REGULATOR_DESC(LDO3, vout_type2_tables, "rt5133,base"),
	RT5133_REGULATOR_DESC(LDO4, vout_type2_tables, "rt5133,base"),
	RT5133_REGULATOR_DESC(LDO5, vout_type2_tables, "rt5133,base"),
	RT5133_REGULATOR_DESC(LDO6, vout_type2_tables, "rt5133,base"),
	RT5133_REGULATOR_DESC(LDO7, vout_type3_tables, "rt5133-ldo1"),
	RT5133_REGULATOR_DESC(LDO8, vout_type3_tables, "rt5133-ldo1"),
};

static int rt5133_gpio_direction_output(struct gpio_chip *gpio,
					unsigned int offset, int value)
{
	struct rt5133_priv *priv = gpiochip_get_data(gpio);

	return regmap_update_bits(priv->regmap, RT5133_REG_GPIO_CTRL,
				  BIT(7-offset)|BIT(3-offset),
				  value ? BIT(7-offset)|BIT(3-offset) : 0);
}

static int rt5133_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	struct rt5133_priv *priv = gpiochip_get_data(chip);

	return !!(priv->gpio_output_flag & BIT(offset));
}

static void rt5133_gpio_set(struct gpio_chip *chip, unsigned int offset,
			    int set_val)
{
	struct rt5133_priv *priv = gpiochip_get_data(chip);
	unsigned int mask = RT5133_GPIOEN_MASK(offset);
	unsigned int val = set_val ? mask : 0;
	unsigned int next_flag = priv->gpio_output_flag;
	int ret;

	if (set_val)
		next_flag |= BIT(offset);
	else
		next_flag &= ~BIT(offset);

	ret = regmap_update_bits(priv->regmap, RT5133_REG_GPIO_CTRL, mask, val);
	if (ret) {
		dev_err(priv->dev, "Failed to set gpio [%d] val %d\n", offset,
			set_val);
		return;
	}

	priv->gpio_output_flag = next_flag;
}

static irqreturn_t rt5133_intr_handler(int irq_number, void *data)
{
	struct rt5133_priv *priv = data;
	u32 intr_evts = 0, handle_evts;
	int i, ret;

	ret = regmap_bulk_read(priv->regmap, RT5133_REG_BASE_EVT, &intr_evts,
			       RT5133_INTR_BYTE_NR);
	if (ret)
		goto out_intr_handler;

	handle_evts = intr_evts & RT5133_BASE_EVT_MASK;
	/*
	 * VREF_EVT is a special case, if base off
	 * this event will also be trigger. Skip it
	 */
	if (handle_evts & ~RT5133_VREF_EVT_MASK)
		dev_info(priv->dev, "base event occurred [0x%02x]\n",
			 handle_evts);

	handle_evts = (intr_evts & RT5133_LDO_OC_EVT_MASK) >>
		RT5133_LDO_OC_EVT_SHIFT;
	for (i = RT5133_REGULATOR_LDO1; i < RT5133_REGULATOR_MAX && handle_evts; i++) {
		if (!(handle_evts & BIT(i - 1)))
			continue;
		regulator_notifier_call_chain(priv->rdev[i],
					      REGULATOR_EVENT_OVER_CURRENT,
					      &i);
	}

	handle_evts = (intr_evts & RT5133_LDO_PGB_EVT_MASK) >>
		RT5133_LDO_PGB_EVT_SHIFT;
	for (i = RT5133_REGULATOR_LDO1; i < RT5133_REGULATOR_MAX && handle_evts; i++) {
		if (!(handle_evts & BIT(i - 1)))
			continue;
		regulator_notifier_call_chain(priv->rdev[i],
					      REGULATOR_EVENT_FAIL, &i);
	}

	ret = regmap_bulk_write(priv->regmap, RT5133_REG_BASE_EVT, &intr_evts,
				RT5133_INTR_BYTE_NR);
	if (ret)
		goto out_intr_handler;

	return IRQ_HANDLED;

out_intr_handler:
	return IRQ_NONE;
}

static int rt5133_enable_interrupts(int irq_no, struct rt5133_priv *priv)
{
	u32 mask = RT5133_INTR_CLR_MASK;
	int ret;

	/* Force to write clear all events */
	ret = regmap_bulk_write(priv->regmap, RT5133_REG_BASE_EVT, &mask,
				RT5133_INTR_BYTE_NR);
	if (ret) {
		dev_err(priv->dev, "Failed to clear all interrupts\n");
		return ret;
	}

	/* Unmask all interrupts */
	mask = 0;
	ret = regmap_bulk_write(priv->regmap, RT5133_REG_BASE_MASK, &mask,
				RT5133_INTR_BYTE_NR);
	if (ret) {
		dev_err(priv->dev, "Failed to unmask all interrupts\n");
		return ret;
	}

	return devm_request_threaded_irq(priv->dev, irq_no, NULL,
					 rt5133_intr_handler, IRQF_ONESHOT,
					 dev_name(priv->dev), priv);
}

#if GENERIC_DEBUGFS
static int rt5133_dbg_io_read(void *drvdata, u16 reg, void *val, u16 size)
{
	return regmap_bulk_read((struct regmap *)drvdata, reg, val, size);
}

static int rt5133_dbg_io_write(void *drvdata, u16 reg,
			       const void *val, u16 size)
{
	return regmap_bulk_write((struct regmap *)drvdata, reg, val, size);
}
#endif /* GENERIC_DEBUGFS */

static int rt5133_regmap_hw_read(void *context, const void *reg_buf,
				 size_t reg_size, void *val_buf,
				 size_t val_size)
{
	struct rt5133_priv *priv = context;
	struct i2c_client *client = to_i2c_client(priv->dev);
	u8 reg = *(u8 *)reg_buf, crc;
	u8 *buf;
	int buf_len = RT5133_PREDATA_LEN + val_size + RT5133_I2C_CRC_LEN;
	int read_len, ret;

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf[0] = I2C_ADDR_XLATE_8BIT(client->addr, I2C_SMBUS_READ);
	buf[1] = reg;

	read_len = val_size + RT5133_I2C_CRC_LEN;
	ret = i2c_smbus_read_i2c_block_data(client, reg, read_len,
					    buf + RT5133_PREDATA_LEN);

	if (ret < 0)
		goto out_read_err;
	else if (ret != read_len) {
		ret = -EIO;
		goto out_read_err;
	}

	crc = crc8(priv->crc8_tbls, buf, RT5133_PREDATA_LEN + val_size, 0);
	if (crc != buf[RT5133_PREDATA_LEN + val_size]) {
		ret = -EIO;
		goto out_read_err;
	}

	memcpy(val_buf, buf + RT5133_PREDATA_LEN, val_size);

out_read_err:
	kfree(buf);
	return (ret < 0) ? ret : 0;
}

static int rt5133_regmap_hw_write(void *context, const void *data, size_t count)
{
	struct rt5133_priv *priv = context;
	struct i2c_client *client = to_i2c_client(priv->dev);
	u8 reg = *(u8 *)data, crc;
	u8 *buf;
	int buf_len = RT5133_I2C_ADDR_LEN + count + RT5133_I2C_CRC_LEN +
		RT5133_I2C_DUMMY_LEN;
	int write_len, ret;

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf[0] = I2C_ADDR_XLATE_8BIT(client->addr, I2C_SMBUS_WRITE);
	buf[1] = reg;
	memcpy(buf + RT5133_PREDATA_LEN, data + RT5133_REG_ADDR_LEN,
	       count - RT5133_REG_ADDR_LEN);

	crc = crc8(priv->crc8_tbls, buf, RT5133_I2C_ADDR_LEN + count, 0);
	buf[RT5133_I2C_ADDR_LEN + count] = crc;

	write_len = count - RT5133_REG_ADDR_LEN + RT5133_I2C_CRC_LEN +
		RT5133_I2C_DUMMY_LEN;
	ret = i2c_smbus_write_i2c_block_data(client, reg, write_len,
					     buf + RT5133_PREDATA_LEN);

	kfree(buf);
	return ret;
}

static const struct regmap_bus rt5133_regmap_bus = {
	.read = rt5133_regmap_hw_read,
	.write = rt5133_regmap_hw_write,
	/* Due to crc, the block read/write length has the limit */
	.max_raw_read = RT5133_MAX_I2C_BLOCK_SIZE,
	.max_raw_write = RT5133_MAX_I2C_BLOCK_SIZE,
};

static const struct regmap_config rt5133_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = RT5133_REG_LDO8_CTRL4,
};

static int rt5133_chip_reset(struct rt5133_priv *priv)
{
	int ret;

	ret = regmap_write(priv->regmap, RT5133_REG_RST_CTRL,
			   RT5133_RESET_CODE);
	if (ret)
		return ret;

	/* Wait for register reset to take effect */
	udelay(2);

	return 0;
}

static int rt5133_validate_vendor_info(struct rt5133_priv *priv)
{
	unsigned int val = 0;
	int ret;

	ret = regmap_read(priv->regmap, RT5133_REG_CHIP_INFO, &val);
	if (ret)
		return ret;

	if ((val & RT5133_VENDOR_ID_MASK) != RT5133_VENDOR_ID)
		return -ENODEV;

	return 0;
}

void rt5133_register_interrupt_callback(enum RT5133_IRQ_NUM intno,
					RT5133_IRQ_FUNC_PTR IRQ_FUNC_PTR)
{
	if (intno < RT5133_IRQ_MAX && intno >= 0) {
		rt5133_callback[intno] = IRQ_FUNC_PTR;
		rt5133_callback[intno]();
	}
}
EXPORT_SYMBOL(rt5133_register_interrupt_callback);

void rt5133_enable_interrupt(enum RT5133_IRQ_NUM intno, int en)
{
	rt5133_callback_enable[intno] = en ? 1 : 0;
}
EXPORT_SYMBOL(rt5133_enable_interrupt);

static int rt5133_regulator_notify(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	int idx;

	if (event != REGULATOR_EVENT_OVER_CURRENT &&
	    event != REGULATOR_EVENT_FAIL)
		goto out;

	if (data == NULL) {
		pr_info("%s: data gets null pointer\n", __func__);
		goto out;
	}

	idx = *(int *)data;
	pr_info("%s, ldo(%d), event = %d\n", __func__, idx, (int)event);

	idx = idx - 1;
	if (idx < 0)
		goto out;

	switch (event) {
	case REGULATOR_EVENT_OVER_CURRENT:
		if (rt5133_callback[idx*2] && rt5133_callback_enable[idx*2])
			rt5133_callback[idx*2]();
		break;
	case REGULATOR_EVENT_FAIL:
		if (rt5133_callback[idx*2+1] && rt5133_callback_enable[idx*2+1])
			rt5133_callback[idx*2+1]();
		break;
	default:
		break;
	}

out:
	return NOTIFY_OK;
}

static int rt5133_register_notifier(struct rt5133_priv *priv)
{
	int i = 0;
	const char **regulator_name;
	struct device_node *np = priv->dev->of_node;
	int ret;

	regulator_name = kcalloc(8, sizeof(char *),  GFP_KERNEL);
	if (of_property_read_string_array(np, "regulator_nb", regulator_name, 8) < 0)
		goto err_read_property;

	for (i = 0; i < 8; i++) {
		rt5133_callback[i] = rt5133_callback[i+1] = NULL;
		regulator[i] = devm_regulator_get(priv->dev,
							regulator_name[i]);
		if (IS_ERR(regulator[i])) {
			dev_err(priv->dev, "get regulator %s fail\n",
				regulator_name[i]);
			goto err_get_regulator;
		}

		rt5133_nb[i].notifier_call = rt5133_regulator_notify;
		ret = devm_regulator_register_notifier(regulator[i],
						       &rt5133_nb[i]);
		if (ret < 0)
			goto err_get_regulator;
	}

	kfree(regulator_name);
	return 0;

err_get_regulator:
	if (i > 0) {
		for (; i > 0; i--) {
			devm_regulator_put(regulator[i]);
			devm_regulator_unregister_notifier(regulator[i],
							   &rt5133_nb[i]);
		}
	}

err_read_property:
	kfree(regulator_name);
	return -EINVAL;
}

static int rt5133_probe(struct i2c_client *i2c)
{
	struct rt5133_priv *priv;
	struct regulator_config config = {0};
	int i, ret;

	dev_info(&i2c->dev, "%s start(%s)\n", __func__, RT5133_DRV_VERSION);
	priv = devm_kzalloc(&i2c->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &i2c->dev;
	crc8_populate_msb(priv->crc8_tbls, RT5133_CRC8_POLYNOMIAL);

	priv->enable_gpio = devm_gpiod_get_optional(&i2c->dev, "enable",
						    GPIOD_OUT_HIGH);
	if (IS_ERR(priv->enable_gpio)) {
		dev_err(&i2c->dev, "Failed to request HWEN gpio\n");
		return PTR_ERR(priv->enable_gpio);
	}

	priv->regmap = devm_regmap_init(&i2c->dev, &rt5133_regmap_bus, priv,
					&rt5133_regmap_config);
	if (IS_ERR(priv->regmap)) {
		dev_err(&i2c->dev, "Failed to register regmap\n");
		return PTR_ERR(priv->regmap);
	}

#if GENERIC_DEBUGFS
	priv->dbg_info.dirname = devm_kasprintf(&i2c->dev,
			GFP_KERNEL, "RT5133.%s",
			dev_name(&i2c->dev));
	priv->dbg_info.devname = dev_name(&i2c->dev);
	priv->dbg_info.typestr = devm_kasprintf(&i2c->dev,
			GFP_KERNEL, "I2C,RT5133");
	priv->dbg_info.io_drvdata = priv->regmap;
	priv->dbg_info.io_read = rt5133_dbg_io_read;
	priv->dbg_info.io_write = rt5133_dbg_io_write;

	ret = generic_debugfs_init(&priv->dbg_info);
	if (ret < 0)
		return ret;
#endif /* GENERIC_DEBUGFS*/

	ret = rt5133_validate_vendor_info(priv);
	if (ret) {
		dev_err(&i2c->dev, "Failed to check vendor info [%d]\n", ret);
		return ret;
	}

	ret = rt5133_chip_reset(priv);
	if (ret) {
		dev_err(&i2c->dev, "Failed to execute sw reset\n");
		return ret;
	}

	config.dev = &i2c->dev;
	config.driver_data = priv;
	config.regmap = priv->regmap;

	for (i = 0; i < RT5133_REGULATOR_MAX; i++) {
		priv->rdev[i] = devm_regulator_register(&i2c->dev,
							rt5133_regulators + i,
							&config);
		if (IS_ERR(priv->rdev[i])) {
			dev_err(&i2c->dev,
				"Failed to register [%d] regulator\n", i);
			return PTR_ERR(priv->rdev[i]);
		}
	}

	priv->gc.label = dev_name(&i2c->dev);
	priv->gc.parent = &i2c->dev;
	priv->gc.base = -1;
	priv->gc.ngpio = RT5133_GPIO_NR;
	priv->gc.set = rt5133_gpio_set;
	priv->gc.get = rt5133_gpio_get;
	priv->gc.direction_output = rt5133_gpio_direction_output;

	ret = devm_gpiochip_add_data(&i2c->dev, &priv->gc, priv);
	if (ret)
		return ret;

	ret = rt5133_enable_interrupts(i2c->irq, priv);
	if (ret) {
		dev_err(&i2c->dev, "enable interrupt failed\n");
		return ret;
	}

	ret = rt5133_register_notifier(priv);
	if (ret) {
		dev_err(&i2c->dev, "register regulator notifier failed\n");
		return ret;
	}

	dev_info(&i2c->dev, "%s done.\n", __func__);
	return ret;
}

static const struct of_device_id __maybe_unused rt5133_ofid_tbls[] = {
	{ .compatible = "richtek,rt5133", },
	{ }
};
MODULE_DEVICE_TABLE(of, rt5133_ofid_tbls);

static struct i2c_driver rt5133_driver = {
	.driver = {
		.name = "rt5133",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rt5133_ofid_tbls),
	},
	.probe_new = rt5133_probe,
};
module_i2c_driver(rt5133_driver);

MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_DESCRIPTION("RT5133 Regulator Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(RT5133_DRV_VERSION);
/*
 * Release Note
 * 1.0.2
 * (1) Free regulator_name when read of_property failed to avoid memory leak
 * (2) Check snprintf error return
 * (3) Call rt5133_callback only when intno is in the valid range
 * (4) Initialize idx and handle data == NULL or idx < 0 in rt5133_regulator_notify
 * (5) Initialize val of regmap_read before using it
 *
 * 1.0.1
 * (1) Add driver version description
 * (2) Remove the force disabling of Base current at initialization
 *
 * 1.0.0
 * (1) Initial released
 */
