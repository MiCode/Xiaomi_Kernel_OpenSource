/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/phy/phy.h>
#include <linux/phy/mediatek/mtk_usb_phy.h>

/* general */
#define BIT_WIDTH_1		1
#define MSK_WIDTH_1		0x1
#define VAL_MAX_WDITH_1	0x1
#define VAL_0_WIDTH_1		0x0
#define VAL_1_WIDTH_1		0x1
#define STRNG_0_WIDTH_1	"0"
#define STRNG_1_WIDTH_1	"1"

#define BIT_WIDTH_2		2
#define MSK_WIDTH_2		0x3
#define VAL_MAX_WDITH_2		0x3
#define VAL_0_WIDTH_2		0x0
#define VAL_1_WIDTH_2		0x1
#define VAL_2_WIDTH_2		0x2
#define VAL_3_WIDTH_2		0x3
#define STRNG_0_WIDTH_2	"00"
#define STRNG_1_WIDTH_2	"01"
#define STRNG_2_WIDTH_2	"10"
#define STRNG_3_WIDTH_2	"11"


#define BIT_WIDTH_3		3
#define MSK_WIDTH_3		0x7
#define VAL_MAX_WDITH_3		0x7
#define VAL_0_WIDTH_3		0x0
#define VAL_1_WIDTH_3		0x1
#define VAL_2_WIDTH_3		0x2
#define VAL_3_WIDTH_3		0x3
#define VAL_4_WIDTH_3		0x4
#define VAL_5_WIDTH_3		0x5
#define VAL_6_WIDTH_3		0x6
#define VAL_7_WIDTH_3		0x7
#define STRNG_0_WIDTH_3	"000"
#define STRNG_1_WIDTH_3	"001"
#define STRNG_2_WIDTH_3	"010"
#define STRNG_3_WIDTH_3	"011"
#define STRNG_4_WIDTH_3	"100"
#define STRNG_5_WIDTH_3	"101"
#define STRNG_6_WIDTH_3	"110"
#define STRNG_7_WIDTH_3	"111"

/* specific */
#define FILE_USB_DRIVING_CAPABILITY "USB_DRIVING_CAPABILITY"

#define FILE_RG_USB20_TERM_VREF_SEL "RG_USB20_TERM_VREF_SEL"
#define MSK_RG_USB20_TERM_VREF_SEL MSK_WIDTH_3
#define SHFT_RG_USB20_TERM_VREF_SEL 8
#define OFFSET_RG_USB20_TERM_VREF_SEL 0x4

#define FILE_RG_USB20_HSTX_SRCTRL "RG_USB20_HSTX_SRCTRL"
#define MSK_RG_USB20_HSTX_SRCTRL MSK_WIDTH_3
#define SHFT_RG_USB20_HSTX_SRCTRL 12
#define OFFSET_RG_USB20_HSTX_SRCTRL 0x14

#define FILE_RG_USB20_VRT_VREF_SEL "RG_USB20_VRT_VREF_SEL"
#define MSK_RG_USB20_VRT_VREF_SEL MSK_WIDTH_3
#define SHFT_RG_USB20_VRT_VREF_SEL 12
#define OFFSET_RG_USB20_VRT_VREF_SEL 0x4

#define FILE_RG_USB20_INTR_EN "RG_USB20_INTR_EN"
#define MSK_RG_USB20_INTR_EN MSK_WIDTH_1
#define SHFT_RG_USB20_INTR_EN 5
#define OFFSET_RG_USB20_INTR_EN 0x0

#define FILE_RG_USB20_PHY_REV6 "RG_USB20_PHY_REV6"
#define MSK_RG_USB20_PHY_REV6 MSK_WIDTH_2
#define SHFT_RG_USB20_PHY_REV6 30
#define OFFSET_RG_USB20_PHY_REV6 0x18

#define FILE_REG_DEBUG "phy_reg"

static struct dentry *usb20_phy_debugfs_root;
static u32 ippc_value, ippc_addr;

static void u3phywrite32(struct phy *phy, int offset, int mask, int value)
{
	u32 cur_value;
	u32 new_value;

	cur_value = usb_mtkphy_io_read(phy, offset);
	new_value = (cur_value & (~mask)) | value;
	usb_mtkphy_io_write(phy, new_value, offset);
}

static void usb20_phy_debugfs_write_width1(struct phy *phy, u8 offset, u8 shift,
	char *buf)
{
	u32 set_val = 0;

	pr_debug("s(%s)\n", buf);
	if (!strncmp(buf, STRNG_0_WIDTH_1, BIT_WIDTH_1)) {
		pr_debug("%s case\n", STRNG_0_WIDTH_1);
		set_val = VAL_0_WIDTH_1;
	} else if (!strncmp(buf, STRNG_1_WIDTH_1, BIT_WIDTH_1)) {
		pr_debug("%s case\n", STRNG_1_WIDTH_1);
		set_val = VAL_1_WIDTH_1;
	} else
		return;

	u3phywrite32(phy, offset, MSK_WIDTH_1 << shift, set_val << shift);

}

static void usb20_phy_debugfs_rev6_write(struct phy *phy, u8 offset, u8 shift,
	char *buf)
{
	u32 set_val = 0xFF;

	pr_debug("s(%s)\n", buf);
	if (!strncmp(buf, STRNG_0_WIDTH_2, BIT_WIDTH_2)) {
		pr_debug("%s case\n", STRNG_0_WIDTH_2);
		set_val = VAL_0_WIDTH_2;
	} else if (!strncmp(buf, STRNG_1_WIDTH_2, BIT_WIDTH_2)) {
		pr_debug("%s case\n", STRNG_1_WIDTH_2);
		set_val = VAL_1_WIDTH_2;
	} else if (!strncmp(buf, STRNG_2_WIDTH_2, BIT_WIDTH_2)) {
		pr_debug("%s case\n", STRNG_2_WIDTH_2);
		set_val = VAL_2_WIDTH_2;
	} else if (!strncmp(buf, STRNG_3_WIDTH_2, BIT_WIDTH_2)) {
		pr_debug("%s case\n", STRNG_3_WIDTH_2);
		set_val = VAL_3_WIDTH_2;
	} else
		return;

	u3phywrite32(phy, offset, MSK_WIDTH_2 << shift, set_val << shift);

}


static void usb20_phy_debugfs_write_width3(struct phy *phy, u8 offset, u8 shift,
	char *buf)
{
	u32 set_val = 0;

	pr_debug("s(%s)\n", buf);
	if (!strncmp(buf, STRNG_0_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_0_WIDTH_3);
		set_val = VAL_0_WIDTH_3;
	} else if (!strncmp(buf, STRNG_1_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_1_WIDTH_3);
		set_val = VAL_1_WIDTH_3;
	} else if (!strncmp(buf, STRNG_2_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_2_WIDTH_3);
		set_val = VAL_2_WIDTH_3;
	} else if (!strncmp(buf, STRNG_3_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_3_WIDTH_3);
		set_val = VAL_3_WIDTH_3;
	} else if (!strncmp(buf, STRNG_4_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_4_WIDTH_3);
		set_val = VAL_4_WIDTH_3;
	} else if (!strncmp(buf, STRNG_5_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_5_WIDTH_3);
		set_val = VAL_5_WIDTH_3;
	} else if (!strncmp(buf, STRNG_6_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_6_WIDTH_3);
		set_val = VAL_6_WIDTH_3;
	} else if (!strncmp(buf, STRNG_7_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_7_WIDTH_3);
		set_val = VAL_7_WIDTH_3;
	} else
		return;

	u3phywrite32(phy, offset, MSK_WIDTH_3 << shift, set_val << shift);

}

static u8 usb20_phy_debugfs_read_val(u32 val, u8 width, char *str)
{
	int i, temp;

	temp = val;
	str[width] = '\0';
	for (i = (width - 1); i >= 0; i--) {
		if (val % 2)
			str[i] = '1';
		else
			str[i] = '0';
		pr_debug("str[%d]:%c\n", i, str[i]);
		val /= 2;
	}
	pr_debug("str(%s)\n", str);
	return val;
}

static int usb_driving_capability_show(struct seq_file *s, void *unused)
{
	struct phy *phy = s->private;
	u32 val;
	char str[16];
	u8 combined_val, tmp_val = 0xff;

	val = usb_mtkphy_io_read(phy, (OFFSET_RG_USB20_TERM_VREF_SEL));
	val = val >> SHFT_RG_USB20_TERM_VREF_SEL;
	val = val & MSK_RG_USB20_TERM_VREF_SEL;
	val = usb20_phy_debugfs_read_val(val, BIT_WIDTH_3, str);

	if (!strncmp(str, STRNG_0_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_0_WIDTH_3);
		tmp_val = VAL_0_WIDTH_3;
	} else if (!strncmp(str, STRNG_1_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_1_WIDTH_3);
		tmp_val = VAL_1_WIDTH_3;
	} else if (!strncmp(str, STRNG_2_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_2_WIDTH_3);
		tmp_val = VAL_2_WIDTH_3;
	} else if (!strncmp(str, STRNG_3_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_3_WIDTH_3);
		tmp_val = VAL_3_WIDTH_3;
	} else if (!strncmp(str, STRNG_4_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_4_WIDTH_3);
		tmp_val = VAL_4_WIDTH_3;
	} else if (!strncmp(str, STRNG_5_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_5_WIDTH_3);
		tmp_val = VAL_5_WIDTH_3;
	} else if (!strncmp(str, STRNG_6_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_6_WIDTH_3);
		tmp_val = VAL_6_WIDTH_3;
	} else if (!strncmp(str, STRNG_7_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_7_WIDTH_3);
		tmp_val = VAL_7_WIDTH_3;
	}

	combined_val = tmp_val;

	val = usb_mtkphy_io_read(phy, (OFFSET_RG_USB20_VRT_VREF_SEL));
	val = val >> SHFT_RG_USB20_VRT_VREF_SEL;
	val = val & MSK_RG_USB20_VRT_VREF_SEL;
	val = usb20_phy_debugfs_read_val(val, BIT_WIDTH_3, str);

	if (!strncmp(str, STRNG_0_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_0_WIDTH_3);
		tmp_val = VAL_0_WIDTH_3;
	} else if (!strncmp(str, STRNG_1_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_1_WIDTH_3);
		tmp_val = VAL_1_WIDTH_3;
	} else if (!strncmp(str, STRNG_2_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_2_WIDTH_3);
		tmp_val = VAL_2_WIDTH_3;
	} else if (!strncmp(str, STRNG_3_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_3_WIDTH_3);
		tmp_val = VAL_3_WIDTH_3;
	} else if (!strncmp(str, STRNG_4_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_4_WIDTH_3);
		tmp_val = VAL_4_WIDTH_3;
	} else if (!strncmp(str, STRNG_5_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_5_WIDTH_3);
		tmp_val = VAL_5_WIDTH_3;
	} else if (!strncmp(str, STRNG_6_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_6_WIDTH_3);
		tmp_val = VAL_6_WIDTH_3;
	} else if (!strncmp(str, STRNG_7_WIDTH_3, BIT_WIDTH_3)) {
		pr_debug("%s case\n", STRNG_7_WIDTH_3);
		tmp_val = VAL_7_WIDTH_3;
	}

	pr_debug("combined_val(%d), tmp_val(%d)\n", combined_val, tmp_val);
	if ((tmp_val == (combined_val - 1)) || (tmp_val == combined_val))
		combined_val += tmp_val;
	else
		combined_val = tmp_val * (VAL_MAX_WDITH_3 + 1) + combined_val;

	pr_debug("combined_val(%d), tmp_val(%d)\n", combined_val, tmp_val);

	seq_printf(s, "\n%s = %d\n", FILE_USB_DRIVING_CAPABILITY, combined_val);
	return 0;
}

static int rg_usb20_term_vref_sel_show(struct seq_file *s, void *unused)
{
	struct phy *phy = s->private;
	u32 val;
	char str[16];

	val = usb_mtkphy_io_read(phy, (OFFSET_RG_USB20_TERM_VREF_SEL));
	val = val >> SHFT_RG_USB20_TERM_VREF_SEL;
	val = val & MSK_RG_USB20_TERM_VREF_SEL;
	val = usb20_phy_debugfs_read_val(val, BIT_WIDTH_3, str);

	seq_printf(s, "\n%s = %s\n", FILE_RG_USB20_TERM_VREF_SEL, str);
	return 0;
}

static int rg_usb20_hstx_srctrl_show(struct seq_file *s, void *unused)
{
	struct phy *phy = s->private;
	u32 val;
	char str[16];

	val = usb_mtkphy_io_read(phy, (OFFSET_RG_USB20_HSTX_SRCTRL));
	val = val >> SHFT_RG_USB20_HSTX_SRCTRL;
	val = val & MSK_RG_USB20_HSTX_SRCTRL;
	val = usb20_phy_debugfs_read_val(val, BIT_WIDTH_3, str);

	seq_printf(s, "\n%s = %s\n", FILE_RG_USB20_HSTX_SRCTRL, str);
	return 0;
}

static int rg_usb20_vrt_vref_sel_show(struct seq_file *s, void *unused)
{
	struct phy *phy = s->private;
	u32 val;
	char str[16];

	val = usb_mtkphy_io_read(phy, (OFFSET_RG_USB20_VRT_VREF_SEL));
	val = val >> SHFT_RG_USB20_VRT_VREF_SEL;
	val = val & MSK_RG_USB20_VRT_VREF_SEL;
	val = usb20_phy_debugfs_read_val(val, BIT_WIDTH_3, str);

	seq_printf(s, "\n%s = %s\n", FILE_RG_USB20_VRT_VREF_SEL, str);
	return 0;
}

static int rg_usb20_intr_en_show(struct seq_file *s, void *unused)
{
	struct phy *phy = s->private;
	u32 val;
	char str[16];

	val = usb_mtkphy_io_read(phy, (OFFSET_RG_USB20_INTR_EN));
	val = val >> SHFT_RG_USB20_INTR_EN;
	val = val & MSK_RG_USB20_INTR_EN;
	val = usb20_phy_debugfs_read_val(val, BIT_WIDTH_1, str);

	seq_printf(s, "\n%s = %s\n", FILE_RG_USB20_INTR_EN, str);
	return 0;
}

static int rg_usb20_rev6_show(struct seq_file *s, void *unused)
{
	struct phy *phy = s->private;
	u32 val;
	char str[16];

	val = usb_mtkphy_io_read(phy, (OFFSET_RG_USB20_PHY_REV6));
	val = val >> SHFT_RG_USB20_PHY_REV6;
	val = val & MSK_RG_USB20_PHY_REV6;
	val = usb20_phy_debugfs_read_val(val, BIT_WIDTH_2, str);

	seq_printf(s, "\n%s = %s\n", FILE_RG_USB20_PHY_REV6, str);
	return 0;
}

static int phy_rw_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "\nphy[0x%x] = 0x%x\n", ippc_addr, ippc_value);

	return 0;
}

static int usb_driving_capability_open(struct inode *inode, struct file *file)
{
	return single_open(file, usb_driving_capability_show, inode->i_private);
}

static int rg_usb20_term_vref_sel_open(struct inode *inode, struct file *file)
{
	return single_open(file, rg_usb20_term_vref_sel_show, inode->i_private);
}

static int rg_usb20_hstx_srctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, rg_usb20_hstx_srctrl_show, inode->i_private);
}

static int rg_usb20_vrt_vref_sel_open(struct inode *inode, struct file *file)
{
	return single_open(file, rg_usb20_vrt_vref_sel_show, inode->i_private);
}

static int rg_usb20_intr_en_open(struct inode *inode, struct file *file)
{
	return single_open(file, rg_usb20_intr_en_show, inode->i_private);
}

static int rg_usb20_rev6_open(struct inode *inode, struct file *file)
{
	return single_open(file, rg_usb20_rev6_show, inode->i_private);
}

static int phy_rw_open(struct inode *inode, struct file *file)
{
	return single_open(file, phy_rw_show, inode->i_private);
}

void val_to_bstring_width3(u8 val, char *str)
{
	switch (val) {
	case VAL_0_WIDTH_3:
		memcpy(str, STRNG_0_WIDTH_3, BIT_WIDTH_3 + 1);
		break;
	case VAL_1_WIDTH_3:
		memcpy(str, STRNG_1_WIDTH_3, BIT_WIDTH_3 + 1);
		break;
	case VAL_2_WIDTH_3:
		memcpy(str, STRNG_2_WIDTH_3, BIT_WIDTH_3 + 1);
		break;
	case VAL_3_WIDTH_3:
		memcpy(str, STRNG_3_WIDTH_3, BIT_WIDTH_3 + 1);
		break;
	case VAL_4_WIDTH_3:
		memcpy(str, STRNG_4_WIDTH_3, BIT_WIDTH_3 + 1);
		break;
	case VAL_5_WIDTH_3:
		memcpy(str, STRNG_5_WIDTH_3, BIT_WIDTH_3 + 1);
		break;
	case VAL_6_WIDTH_3:
		memcpy(str, STRNG_6_WIDTH_3, BIT_WIDTH_3 + 1);
		break;
	case VAL_7_WIDTH_3:
		memcpy(str, STRNG_7_WIDTH_3, BIT_WIDTH_3 + 1);
		break;
	}

	pr_debug("val(%d), str(%s)\n", val, str);
}

static ssize_t usb_driving_capability_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct phy *phy = s->private;
	char buf[18];
	u8 val, tmp_val;
	char str_rg_usb20_term_vref_sel[18], str_rg_usb20_vrt_vref_sel[18];

	memset(buf, 0x00, sizeof(buf));
	pr_debug("\n");
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtou8(buf, 10, &val) != 0) {
		pr_debug("kstrtou8, err(%d)\n", kstrtou8(buf, 10, &val));
		return count;
	}
	pr_debug("kstrtol, val(%d)\n", val);

	if (val > VAL_7_WIDTH_3 * 2) {
		pr_debug("wrong val set(%d), direct return\n", val);
		return count;
	}
	tmp_val = val;
	val /= 2;

	pr_debug("val(%d), tmp_val(%d)\n", val, tmp_val);
	val_to_bstring_width3(tmp_val - val, str_rg_usb20_term_vref_sel);
	val_to_bstring_width3(val, str_rg_usb20_vrt_vref_sel);
	pr_debug("Config TERM_VREF_SEL %s\n", str_rg_usb20_term_vref_sel);
	usb20_phy_debugfs_write_width3(phy, OFFSET_RG_USB20_TERM_VREF_SEL,
		SHFT_RG_USB20_TERM_VREF_SEL, str_rg_usb20_term_vref_sel);
	pr_debug("Config VRT_VREF_SEL %s\n", str_rg_usb20_vrt_vref_sel);
	usb20_phy_debugfs_write_width3(phy, OFFSET_RG_USB20_VRT_VREF_SEL,
		SHFT_RG_USB20_VRT_VREF_SEL, str_rg_usb20_vrt_vref_sel);
	return count;
}

static ssize_t rg_usb20_term_vref_sel_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct phy *phy = s->private;
	char buf[18];

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;
	usb20_phy_debugfs_write_width3(phy, OFFSET_RG_USB20_TERM_VREF_SEL,
		SHFT_RG_USB20_TERM_VREF_SEL, buf);
	return count;
}

static ssize_t rg_usb20_hstx_srctrl_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct phy *phy = s->private;
	char buf[18];

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;
	usb20_phy_debugfs_write_width3(phy, OFFSET_RG_USB20_HSTX_SRCTRL,
		SHFT_RG_USB20_HSTX_SRCTRL, buf);
	return count;
}

static ssize_t rg_usb20_vrt_vref_sel_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct phy *phy = s->private;
	char buf[18];

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;
	usb20_phy_debugfs_write_width3(phy, OFFSET_RG_USB20_VRT_VREF_SEL,
		SHFT_RG_USB20_VRT_VREF_SEL, buf);
	return count;
}

static ssize_t rg_usb20_intr_en_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct phy *phy = s->private;
	char buf[18];

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;
	usb20_phy_debugfs_write_width1(phy, OFFSET_RG_USB20_INTR_EN,
		SHFT_RG_USB20_INTR_EN, buf);
	return count;
}

static ssize_t rg_usb20_rev6_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct phy *phy = s->private;
	char buf[18];

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;
	usb20_phy_debugfs_rev6_write(phy, OFFSET_RG_USB20_PHY_REV6,
		SHFT_RG_USB20_PHY_REV6, buf);
	return count;
}

static ssize_t phy_rw_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct phy *phy = s->private;
	char buf[40];

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (!strncmp(buf, "w", 1)) {
		u32 offset;
		u32 value;
		u32 shift;
		u32 mask;

		if (sscanf(buf, "w32 0x%x:%d:0x%x:0x%x",
			&offset, &shift, &mask, &value) == 4) {
			if ((offset % 4) != 0) {
				pr_notice("Must use 32bits alignment address\n");
				return count;
			}
			u3phywrite32(phy, offset,
				mask << shift, value << shift);
		} else
			return -EFAULT;
	}

	if (!strncmp(buf, "r", 1)) {
		u32 offset;

		if (sscanf(buf, "r32 0x%x", &offset) == 1) {
			if ((offset % 4) != 0) {
				pr_notice("Must use 32bits alignment address\n");
				return count;
			}
			ippc_addr = offset;
			ippc_value = usb_mtkphy_io_read(phy, ippc_addr);
		}
	}

	return count;
}



static const struct file_operations usb_driving_capability_fops = {
	.open = usb_driving_capability_open,
	.write = usb_driving_capability_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations rg_usb20_term_vref_sel_fops = {
	.open = rg_usb20_term_vref_sel_open,
	.write = rg_usb20_term_vref_sel_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations rg_usb20_hstx_srctrl_fops = {
	.open = rg_usb20_hstx_srctrl_open,
	.write = rg_usb20_hstx_srctrl_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations rg_usb20_vrt_vref_sel_fops = {
	.open = rg_usb20_vrt_vref_sel_open,
	.write = rg_usb20_vrt_vref_sel_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations rg_usb20_intr_en_fops = {
	.open = rg_usb20_intr_en_open,
	.write = rg_usb20_intr_en_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations rg_usb20_rev6_fops = {
	.open = rg_usb20_rev6_open,
	.write = rg_usb20_rev6_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations phy_rw_fops = {
	.open = phy_rw_open,
	.write = phy_rw_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int mtu3_phy_init_debugfs(struct phy *phy)
{
	struct dentry *root;
	struct dentry *file;
	int ret;

	root = debugfs_create_dir("usb20_phy", NULL);
	if (!root) {
		ret = -ENOMEM;
		goto err0;
	}

	file = debugfs_create_file(FILE_USB_DRIVING_CAPABILITY, 0644,
				   root, phy, &usb_driving_capability_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
	file = debugfs_create_file(FILE_RG_USB20_TERM_VREF_SEL, 0644,
				   root, phy, &rg_usb20_term_vref_sel_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
	file = debugfs_create_file(FILE_RG_USB20_HSTX_SRCTRL, 0644,
				   root, phy, &rg_usb20_hstx_srctrl_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
	file = debugfs_create_file(FILE_RG_USB20_VRT_VREF_SEL, 0644,
				   root, phy, &rg_usb20_vrt_vref_sel_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
	file = debugfs_create_file(FILE_RG_USB20_INTR_EN, 0644,
				   root, phy, &rg_usb20_intr_en_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
	file = debugfs_create_file(FILE_RG_USB20_PHY_REV6, 0644,
				   root, phy, &rg_usb20_rev6_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}

	file = debugfs_create_file(FILE_REG_DEBUG, 0644,
				   root, phy, &phy_rw_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}

	usb20_phy_debugfs_root = root;
	return 0;

err1:
	debugfs_remove_recursive(root);

err0:
	return ret;
}

int mtu3_phy_exit_debugfs(void)
{
	debugfs_remove_recursive(usb20_phy_debugfs_root);
	return 0;
}
