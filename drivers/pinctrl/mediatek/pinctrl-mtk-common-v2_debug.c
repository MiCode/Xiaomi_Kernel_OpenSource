// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 * Author: Light Hsieh <light.hsieh@mediatek.com>
 *
 */

#include <linux/module.h>

#include <dt-bindings/pinctrl/mt65xx.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <../../gpio/gpiolib.h>
#include <asm-generic/gpio.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "pinctrl-paris.h"

#define PULL_DELAY 50 /* in ms */
#define FUN_3STATE "gpio_get_value_tristate"

static const char *pinctrl_paris_modname = MTK_PINCTRL_DEV;
static struct mtk_pinctrl *g_hw;

static void mtk_gpio_find_mtk_pinctrl_dev(void)
{
	unsigned int pin = ARCH_NR_GPIOS - 1;
	struct gpio_desc *gdesc;
	struct mtk_pinctrl *hw;

	do {
		gdesc = gpio_to_desc(pin);
		if (gdesc
		 && !strncmp(pinctrl_paris_modname,
				gdesc->gdev->chip->label,
				strlen(pinctrl_paris_modname))) {
			hw = gpiochip_get_data(gdesc->gdev->chip);
			if (!strcmp(hw->dev->parent->kobj.name, "soc") ||
			    !strcmp(hw->dev->parent->kobj.name, "platform")) {
				if (hw->soc->bias_get_combo &&
				    hw->soc->bias_set_combo) {
					g_hw = hw;
					return;
				}
			}
		}
		if (gdesc)
			pin = gdesc->gdev->chip->base - 1;
		if (pin == 0 || !gdesc)
			break;
	} while (1);

	pr_notice("[pinctrl]cannot find %s gpiochip\n", pinctrl_paris_modname);
}

int gpio_get_tristate_input(unsigned int pin)
{
	struct mtk_pinctrl *hw = NULL;
	const struct mtk_pin_desc *desc;
	int val, val_up, val_down, ret, pullup, pullen, pull_type;

	if (!g_hw)
		mtk_gpio_find_mtk_pinctrl_dev();
	if (!g_hw)
		return  -ENOTSUPP;
	hw = g_hw;

	if (!hw->soc) {
		pr_notice("invalid gpio chip\n");
		return -EINVAL;
	}

	if (pin < hw->chip.base) {
		pr_notice(FUN_3STATE ": please use virtual pin number\n");
		return -EINVAL;
	}

	if (pin - hw->chip.base >= hw->soc->npins) {
		pr_notice(FUN_3STATE ": invalid pin number: %u\n",
			pin);
		return -EINVAL;
	}

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[pin - hw->chip.base];

	ret = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_MODE, &val);
	if (ret)
		return ret;
	if (val != 0) {
		pr_notice(FUN_3STATE ":GPIO%d in mode %d, not GPIO mode\n",
			pin, val);
		return -EINVAL;
	}

	ret = hw->soc->bias_get_combo(hw, desc, &pullup, &pullen);
	if (ret)
		return ret;
	if (pullen == 0 ||  pullen == MTK_PUPD_SET_R1R0_00) {
		pr_notice(FUN_3STATE ":GPIO%d not pullen, skip floating test\n",
			pin);
		return gpio_get_value(pin);
	}
	if (pullen > MTK_PUPD_SET_R1R0_00)
		pull_type = 1;
	else
		pull_type = 0;

	/* set pullsel as pull-up and get input value */
	pr_notice(FUN_3STATE ":pull up GPIO%d\n", pin);
	ret = hw->soc->bias_set_combo(hw, desc, 1,
		(pull_type ? MTK_PUPD_SET_R1R0_11 : MTK_ENABLE));
	if (ret)
		goto out;
	mdelay(PULL_DELAY);
	val_up = gpio_get_value(pin);
	pr_notice(FUN_3STATE ":GPIO%d input %d\n", pin, val_up);

	/* set pullsel as pull-down and get input value */
	pr_notice(FUN_3STATE ":pull down GPIO%d\n", pin);
	ret = hw->soc->bias_set_combo(hw, desc, 0,
		(pull_type ? MTK_PUPD_SET_R1R0_11 : MTK_ENABLE));
	if (ret)
		goto out;
	mdelay(PULL_DELAY);
	val_down = gpio_get_value(pin);
	pr_notice(FUN_3STATE ":GPIO%d input %d\n", pin, val_down);

	if (val_up && val_down)
		ret = 1;
	else if (!val_up && !val_down)
		ret = 0;
	else if (val_up && !val_down)
		ret = 2;
	else {
		pr_notice(FUN_3STATE ":GPIO%d pull HW is abnormal\n", pin);
		ret = -EINVAL;
	}

out:
	/* restore pullsel */
	hw->soc->bias_set_combo(hw, desc, pullup, pullen);

	return ret;
}

static int mtk_hw_set_value_wrap(struct mtk_pinctrl *hw, unsigned int gpio,
	int value, int field)
{
	const struct mtk_pin_desc *desc;

	if (gpio > hw->soc->npins)
		return -EINVAL;

	if (!strncmp(hw->dev->kobj.name, "mt63", strlen("mt63")))
		return mt63xx_hw_set_value(hw, gpio, field, value);

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio];
	return mtk_hw_set_value(hw, desc, field, value);
}

#define mtk_pctrl_set_pinmux(hw, gpio, val)		\
	mtk_hw_set_value_wrap(hw, gpio, val, PINCTRL_PIN_REG_MODE)

/* MTK HW use 0 as input, 1 for output
 * This interface is for set direct register value,
 * so don't reverse
 */
#define mtk_pctrl_set_direction(hw, gpio, val)		\
	mtk_hw_set_value_wrap(hw, gpio, val, PINCTRL_PIN_REG_DIR)

#define mtk_pctrl_set_out(hw, gpio, val)		\
	mtk_hw_set_value_wrap(hw, gpio, val, PINCTRL_PIN_REG_DO)

#define mtk_pctrl_set_smt(hw, gpio, val)		\
	mtk_hw_set_value_wrap(hw, gpio, val, PINCTRL_PIN_REG_SMT)

#define mtk_pctrl_set_ies(hw, gpio, val)		\
	mtk_hw_set_value_wrap(hw, gpio, val, PINCTRL_PIN_REG_IES)

#define mtk_pctrl_set_drv(hw, gpio, val)		\
	mtk_hw_set_value_wrap(hw, gpio, val, PINCTRL_PIN_REG_DRV)

#define mtk_pctrl_set_pullen(hw, gpio, val)		\
	mtk_hw_set_value_wrap(hw, gpio, val, PINCTRL_PIN_REG_PULLEN)

#define mtk_pctrl_set_pullsel(hw, gpio, val)		\
	mtk_hw_set_value_wrap(hw, gpio, val, PINCTRL_PIN_REG_PULLSEL)

void gpio_dump_regs_range(int start, int end)
{
	struct gpio_chip *chip = NULL;
	struct mtk_pinctrl *hw;
	char buf[96];
	int i;

	if (!g_hw)
		mtk_gpio_find_mtk_pinctrl_dev();
	if (!g_hw)
		return;

	hw = g_hw;
	chip = &hw->chip;

	if (start < 0) {
		start = 0;
		end = chip->ngpio - 1;
	}
	if (end < 0)
		end = chip->ngpio - 1;
	if (end > chip->ngpio - 1)
		end = chip->ngpio - 1;

	pr_notice("PIN: (MODE)(DIR)(DOUT)(DIN)(DRIVE)(SMT)(IES)(PULL_EN)(PULL_SEL)(R1 R0)\n");

	for (i = start; i <= end; i++) {
		(void)mtk_pctrl_show_one_pin(hw, i, buf, 96);
		pr_notice("%s\n", buf);
	}
}
EXPORT_SYMBOL_GPL(gpio_dump_regs_range);

void gpio_dump_regs(void)
{
	gpio_dump_regs_range(-1, -1);
}
EXPORT_SYMBOL_GPL(gpio_dump_regs);

#define PIN_DBG_BUF_SZ 96
static int mtk_gpio_proc_show(struct seq_file *file, void *v)
{
	struct mtk_pinctrl *hw = (struct mtk_pinctrl *)file->private;
	struct gpio_chip *chip;
	unsigned int i = 0, start;
	int mt63xx;

	if (!hw) {
		pr_debug("[pinctrl] Err: NULL pointer!\n");
		return 0;
	}

	if (!strncmp(hw->dev->kobj.name, "mt63", strlen("mt63")))
		mt63xx = 1;
	else
		mt63xx = 0;

	chip = &hw->chip;

	seq_printf(file, "pins base: %d\n", chip->base);
	seq_puts(file, "PIN: (MODE)(DIR)(DOUT)(DIN)(DRIVE)(SMT)(IES)(PULL_EN)(PULL_SEL)(R1 R0)\n");

	if (hw->soc->capability_flags & FLAG_GPIO_START_IDX_1)
		start = 1;
	else
		start = 0;
	for (i = start; i < chip->ngpio; i++) {
		if (!mt63xx && mtk_is_virt_gpio(hw, i))
			continue;
		hw->pctrl->desc->pctlops->pin_dbg_show(hw->pctrl, file, i);
		seq_puts(file, "\n");
	}

	return 0;
}

static ssize_t mtk_gpio_write(struct file *file, const char __user *ubuf,
	size_t count, loff_t *ppos)
{
	const struct mtk_pin_desc *desc;
	struct mtk_pinctrl *hw;
	struct gpio_chip *chip;
	int i, gpio, val, val2, vals[12];
	int pullup = 0, pullen = 0;
	char attrs[12], buf[64];
	int r1r0_en[4] = {MTK_PUPD_SET_R1R0_00, MTK_PUPD_SET_R1R0_01,
			  MTK_PUPD_SET_R1R0_10, MTK_PUPD_SET_R1R0_11};
	int mt63xx;

	hw = (struct mtk_pinctrl *)
		(((struct seq_file *)file->private_data)->private);

	if (!hw || !hw->soc) {
		pr_notice("[pinctrl]cannot find %s device\n",
			pinctrl_paris_modname);
		return count;
	}

	if (!strncmp(hw->dev->kobj.name, "mt63", strlen("mt63")))
		mt63xx = 1;
	else
		mt63xx = 0;

	chip = &hw->chip;

	if (count == 0)
		return -1;
	if (count > 63)
		count = 63;

	if (copy_from_user(buf, ubuf, count))
		return -1;

	if (!strncmp(buf, "mode", 4)
		&& (sscanf(buf+4, "%d %d", &gpio, &val) == 2)) {
		mtk_pctrl_set_pinmux(hw, gpio, val);
	} else if (!strncmp(buf, "dir", 3)
		&& (sscanf(buf+3, "%d %d", &gpio, &val) == 2)) {
		mtk_pctrl_set_direction(hw, gpio, val);
	} else if (!strncmp(buf, "out", 3)
		&& (sscanf(buf+3, "%d %d", &gpio, &val) == 2)) {
		mtk_pctrl_set_direction(hw, gpio, 1);
		mtk_pctrl_set_out(hw, gpio, val);
	} else if (!strncmp(buf, "pullen", 6)
		&& (sscanf(buf+6, "%d %d", &gpio, &val) == 2)) {
		if (gpio < 0 || gpio >= hw->soc->npins) {
			pr_notice("invalid pin number\n");
			goto out;
		}
		if (!mt63xx) {
			desc = (const struct mtk_pin_desc *)
				&hw->soc->pins[gpio];
			hw->soc->bias_get_combo(hw, desc, &pullup, &pullen);
			if (pullen < MTK_PUPD_SET_R1R0_00) {
				pullen = !!val;
				if (pullup == 2)
					pullup = 0;
			} else if (pullen >= MTK_PUPD_SET_R1R0_00 &&
				   pullen <= MTK_PUPD_SET_R1R0_11) {
				if (val < 0)
					val = 0;
				else if (val > 3)
					val = 3;
				pullen = r1r0_en[val];
			} else if (pullen >= MTK_PULL_SET_RSEL_000 &&
				   pullen <= MTK_PULL_SET_RSEL_MAX) {
				/* don't support to change rsel via pullen */
				pullen = !!val;
				if (pullup == 2)
					pullup = 0;
			} else {
				goto out;
			}
			hw->soc->bias_set_combo(hw, desc, pullup, pullen);
		} else
			mtk_pctrl_set_pullen(hw, gpio, !!val);
	} else if ((!strncmp(buf, "pullsel", 7))
		&& (sscanf(buf+7, "%d %d", &gpio, &val) == 2)) {
		if (gpio < 0 || gpio >= hw->soc->npins) {
			pr_notice("invalid pin number\n");
			goto out;
		}
		desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio];
		if (!mt63xx) {
			hw->soc->bias_get_combo(hw, desc, &pullup, &pullen);
			if (pullen == 0)
				pullen = 1;
			hw->soc->bias_set_combo(hw, desc, !!val, pullen);
		} else
			mtk_pctrl_set_pullsel(hw, gpio, !!val);
	} else if ((!strncmp(buf, "ies", 3))
		&& (sscanf(buf+3, "%d %d", &gpio, &val) == 2)) {
		mtk_pctrl_set_ies(hw, gpio, val);
	} else if ((!strncmp(buf, "smt", 3))
		&& (sscanf(buf+3, "%d %d", &gpio, &val) == 2)) {
		mtk_pctrl_set_smt(hw, gpio, val);
	} else if ((!strncmp(buf, "driving", 7))
		&& (sscanf(buf+7, "%d %d", &gpio, &val) == 2)) {
		if (hw->soc->drive_set) {
			if (gpio < 0 || gpio >= hw->soc->npins) {
				pr_notice("invalid pin number\n");
				goto out;
			}
			desc = (const struct mtk_pin_desc *)
				&hw->soc->pins[gpio];
			hw->soc->drive_set(hw, desc, val);
		} else
			mtk_pctrl_set_drv(hw, gpio, val);
	} else if ((!strncmp(buf, "r1r0", 4))
		&& (sscanf(buf+4, "%d %d %d", &gpio, &val, &val2) == 3)) {
		if (gpio < 0 || gpio >= hw->soc->npins) {
			pr_notice("invalid pin number\n");
			goto out;
		}
		desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio];
		if (mt63xx) {
			pr_notice("[pinctrl] r1r0 not supported\n");
			goto out;
		}
		hw->soc->bias_get_combo(hw, desc, &pullup, &pullen);
		pullen = r1r0_en[(((!!val) << 1) + !!val2)];
		hw->soc->bias_set_combo(hw, desc, pullup, pullen);
	} else if ((!strncmp(buf, "rsel", 4))
		&& (sscanf(buf+4, "%d %d", &gpio, &val) == 2)) {
		if (gpio < 0 || gpio >= hw->soc->npins) {
			pr_notice("invalid pin number\n");
			goto out;
		}
		desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio];
		if (mt63xx) {
			pr_notice("[pinctrl] rsel not supported\n");
			goto out;
		}
		hw->soc->bias_get_combo(hw, desc, &pullup, &pullen);
		hw->soc->bias_set_combo(hw, desc, pullup,
			val + MTK_PULL_SET_RSEL_000);
	} else if (!strncmp(buf, "set", 3)) {
		if (!mt63xx) {
			val = sscanf(buf+3, "%d %c%c%c%c%c%c%c%c%c%c %c%c",
				&gpio,
				&attrs[0], &attrs[1], &attrs[2], &attrs[3],
				&attrs[4], &attrs[5], &attrs[6], &attrs[7],
				&attrs[8], &attrs[9], &attrs[10], &attrs[11]);
			if (val < 11) {
				pr_notice("invalid input count %d\n", val);
				goto out;
			}
		} else {
			val = sscanf(buf+3, "%d %c%c%c%c%c%c%c%c%c",
				&gpio,
				&attrs[0], &attrs[1], &attrs[2], &attrs[3],
				&attrs[4], &attrs[5], &attrs[6],
				&attrs[8], &attrs[9]);
			if (val < 10) {
				pr_notice("invalid input count %d\n", val);
				goto out;
			}
		}
		for (i = 0; i < ARRAY_SIZE(attrs); i++) {
			if ((attrs[i] >= '0') && (attrs[i] <= '9'))
				vals[i] = attrs[i] - '0';
			else
				vals[i] = 0;
		}
		if (gpio < 0 || gpio >= hw->soc->npins) {
			pr_notice("invalid pin number\n");
			goto out;
		}
		desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio];
		/* MODE */
		mtk_pctrl_set_pinmux(hw, gpio, vals[0]);
		/* DIR */
		mtk_pctrl_set_direction(hw, gpio, !!vals[1]);
		/* DOUT */
		if (vals[1])
			mtk_pctrl_set_out(hw, gpio, !!vals[2]);
		/* DRIVING */
		if (hw->soc->drive_set)
			hw->soc->drive_set(hw, desc, vals[4]*10 + vals[5]);
		else
			mtk_pctrl_set_drv(hw, gpio, vals[4]*10 + vals[5]);
		/* SMT */
		mtk_pctrl_set_smt(hw, gpio, vals[6]);
		/* IES */
		if (!mt63xx)
			mtk_pctrl_set_ies(hw, gpio, vals[7]);
		/* PULL */
		if (!mt63xx) {
			hw->soc->bias_get_combo(hw, desc, &pullup, &pullen);
			if ((pullen < MTK_PUPD_SET_R1R0_00)
			 || (pullen >= MTK_PULL_SET_RSEL_000 &&
			     pullen <= MTK_PULL_SET_RSEL_MAX)){
				hw->soc->bias_set_combo(hw, desc, !!vals[9],
					!!vals[8]);
			} else if (pullen >= MTK_PUPD_SET_R1R0_00 &&
				   pullen <= MTK_PUPD_SET_R1R0_11) {
				val = (((!!vals[10]) << 1) + !!vals[11]);
				pullen = r1r0_en[val];
				hw->soc->bias_set_combo(hw, desc, !!vals[9],
					pullen);
			}
		} else {
			mtk_pctrl_set_pullen(hw, gpio, !!vals[8]);
			mtk_pctrl_set_pullsel(hw, gpio, !!vals[9]);
		}
	}

out:
	return count;
}

static int mtk_gpio_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_gpio_proc_show, PDE_DATA(inode));
}

static const struct proc_ops mtk_gpio_fops = {
	.proc_open		= mtk_gpio_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= seq_release,
	.proc_write		= mtk_gpio_write,
};

static struct proc_dir_entry *mtk_gpio_debug_root;

static int mtk_gpio_init_procfs(void)
{
	struct proc_dir_entry *proc_entry;
	struct mtk_pinctrl *hw = NULL;
	struct gpio_desc *gdesc;
	unsigned int pin = ARCH_NR_GPIOS - 1;
	kuid_t uid;
	kgid_t gid;

	uid = make_kuid(&init_user_ns, 0);
	gid = make_kgid(&init_user_ns, 1001);

	mtk_gpio_debug_root = proc_mkdir("mtk_gpio", NULL);

	do {
		gdesc = gpio_to_desc(pin);
		if (!gdesc)
			break;

		if (!strncmp(pinctrl_paris_modname,
				gdesc->gdev->chip->label,
				strlen(pinctrl_paris_modname))) {
			hw = gpiochip_get_data(gdesc->gdev->chip);
			if (!hw || !hw->soc || !hw->dev) {
				pr_notice("invalid gpio chip\n");
				return -EINVAL;
			}

			if (!strcmp(hw->dev->parent->kobj.name, "soc") ||
			    !strcmp(hw->dev->parent->kobj.name, "platform")) {
				if (hw->soc->bias_get_combo &&
				    hw->soc->bias_set_combo) {
					proc_entry = proc_create_data("soc.pinctrl",
						S_IFREG | 0444, mtk_gpio_debug_root,
						&mtk_gpio_fops, hw);
				}
			} else
				proc_entry = proc_create_data(hw->dev->kobj.name,
					S_IFREG | 0444, mtk_gpio_debug_root,
					&mtk_gpio_fops, hw);

			if (proc_entry) {
				proc_set_user(proc_entry, uid, gid);
			} else {
				pr_notice("[pinctrl]error create mtk_gpio\n");
				return -1;
			}

		}

		pin = gdesc->gdev->chip->base - 1;
	} while (pin > 0);

	return 0;
}

static int __init pinctrl_mtk_debug_v2_init(void)
{
	return mtk_gpio_init_procfs();
}

late_initcall(pinctrl_mtk_debug_v2_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek Pinctrl DEBUG Driver");
