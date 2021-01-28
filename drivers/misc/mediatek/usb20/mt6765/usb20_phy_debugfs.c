// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <musb_core.h>
#include <musb_debug.h>
#include <mtk_musb.h>

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
#define VAL_MAX_WDITH_2	0x3
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

static struct dentry *usb20_phy_debugfs_root;

void usb20_phy_debugfs_write_width1(u8 offset, u8 shift, char *buf)
{
	u32 clr_val = 0, set_val = 0;

	pr_notice("MTK_ICUSB [DBG], <%s(), %d> s(%s)\n",
					__func__, __LINE__, buf);
	if (!strncmp(buf, STRNG_0_WIDTH_1, BIT_WIDTH_1)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
					__func__, __LINE__, STRNG_0_WIDTH_1);
		clr_val = VAL_1_WIDTH_1;
	}
	if (!strncmp(buf, STRNG_1_WIDTH_1, BIT_WIDTH_1)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
					__func__, __LINE__, STRNG_1_WIDTH_1);
		set_val = VAL_1_WIDTH_1;
	}

	if (clr_val || set_val) {
		clr_val = VAL_MAX_WDITH_1 - set_val;
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> offset:%x, clr_val:%x, set_val:%x, before shft\n",
					__func__, __LINE__,
					offset, clr_val,
					set_val);
		clr_val <<= shift;
		set_val <<= shift;
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> offset:%x, clr_val:%x, set_val:%x, after shft\n",
					__func__, __LINE__,
					offset, clr_val,
					set_val);

		USBPHY_CLR32(offset, clr_val);
		USBPHY_SET32(offset, set_val);
	} else {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> do nothing\n"
				, __func__, __LINE__);
	}
}

void usb20_phy_debugfs_rev6_write(u8 offset, u8 shift, char *buf)
{
	u8 set_val = 0xFF;

	pr_notice("MTK_ICUSB [DBG], <%s(), %d> s(%s)\n",
				__func__, __LINE__, buf);
	if (!strncmp(buf, STRNG_0_WIDTH_2, BIT_WIDTH_2)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
					__func__, __LINE__, STRNG_0_WIDTH_2);
		set_val = VAL_0_WIDTH_2;
	}
	if (!strncmp(buf, STRNG_1_WIDTH_2, BIT_WIDTH_2)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
					__func__, __LINE__, STRNG_1_WIDTH_2);
		set_val = VAL_1_WIDTH_2;
	}
	if (!strncmp(buf, STRNG_2_WIDTH_2, BIT_WIDTH_2)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
					__func__, __LINE__, STRNG_2_WIDTH_2);
		set_val = VAL_2_WIDTH_2;
	}
	if (!strncmp(buf, STRNG_3_WIDTH_2, BIT_WIDTH_2)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
					__func__, __LINE__, STRNG_3_WIDTH_2);
		set_val = VAL_3_WIDTH_2;
	}

	if (set_val <= VAL_MAX_WDITH_2) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> case offset:%x, set_val:%x, before shft\n",
					__func__, __LINE__, offset, set_val);
		USBPHY_CLR32(offset, (VAL_MAX_WDITH_2<<shift));
		USBPHY_SET32(offset, (set_val<<shift));
	} else {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> do nothing\n",
					__func__, __LINE__);
	}
}

void usb20_phy_debugfs_write_width3(u8 offset, u8 shift, char *buf)
{
	u32 clr_val = 0, set_val = 0;

	pr_notice("MTK_ICUSB [DBG], <%s(), %d> s(%s)\n",
				__func__, __LINE__, buf);
	if (!strncmp(buf, STRNG_0_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_0_WIDTH_3);
		clr_val = VAL_7_WIDTH_3;
	}
	if (!strncmp(buf, STRNG_1_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_1_WIDTH_3);
		set_val = VAL_1_WIDTH_3;
	}
	if (!strncmp(buf, STRNG_2_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_2_WIDTH_3);
		set_val = VAL_2_WIDTH_3;
	}
	if (!strncmp(buf, STRNG_3_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_3_WIDTH_3);
		set_val = VAL_3_WIDTH_3;
	}
	if (!strncmp(buf, STRNG_4_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
					__func__, __LINE__, STRNG_4_WIDTH_3);
		set_val = VAL_4_WIDTH_3;
	}
	if (!strncmp(buf, STRNG_5_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
					__func__, __LINE__, STRNG_5_WIDTH_3);
		set_val = VAL_5_WIDTH_3;
	}
	if (!strncmp(buf, STRNG_6_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
					__func__, __LINE__, STRNG_6_WIDTH_3);
		set_val = VAL_6_WIDTH_3;
	}
	if (!strncmp(buf, STRNG_7_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
					__func__, __LINE__, STRNG_7_WIDTH_3);
		set_val = VAL_7_WIDTH_3;
	}

	if (clr_val || set_val) {
		clr_val = VAL_MAX_WDITH_3 - set_val;
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> offset:%x, clr_val:%x, set_val:%x, before shft\n",
				__func__, __LINE__, offset, clr_val, set_val);
		clr_val <<= shift;
		set_val <<= shift;
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> offset:%x, clr_val:%x, set_val:%x, after shft\n",
				__func__, __LINE__, offset, clr_val, set_val);

		USBPHY_CLR32(offset, clr_val);
		USBPHY_SET32(offset, set_val);
	} else {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> do nothing\n",
					__func__, __LINE__);
	}
}

u8 usb20_phy_debugfs_read_val(u8 offset, u8 shft, u8 msk, u8 width, char *str)
{
	u32 val;
	int i, temp;

	val = USBPHY_READ32(offset);
	pr_notice("MTK_ICUSB [DBG], <%s(), %d> offset:%x, val:%x, shft:%x, msk:%x\n",
				__func__, __LINE__, offset, val, shft, msk);
	val = val >> shft;
	pr_notice("MTK_ICUSB [DBG], <%s(), %d> offset:%x, val:%x, shft:%x, msk:%x\n",
				__func__, __LINE__, offset, val, shft, msk);
	val = val & msk;
	pr_notice("MTK_ICUSB [DBG], <%s(), %d> offset:%x, val:%x, shft:%x, msk:%x\n",
				__func__, __LINE__, offset, val, shft, msk);

	temp = val;
	str[width] = '\0';
	for (i = (width - 1); i >= 0; i--) {
		if (val % 2)
			str[i] = '1';
		else
			str[i] = '0';
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> str[%d]:%c\n\n",
					__func__, __LINE__, i, str[i]);
		val /= 2;
	}
	pr_notice("MTK_ICUSB [DBG], <%s(), %d> str(%s)\n",
					__func__, __LINE__, str);
	return val;
}

static int usb_driving_capability_show(struct seq_file *s, void *unused)
{
	u8 val;
	char str[16];
	u8 combined_val, tmp_val = 0xff;

	val = usb20_phy_debugfs_read_val(OFFSET_RG_USB20_TERM_VREF_SEL,
				SHFT_RG_USB20_TERM_VREF_SEL,
				MSK_RG_USB20_TERM_VREF_SEL, BIT_WIDTH_3, str);
	if (!strncmp(str, STRNG_0_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_0_WIDTH_3);
		tmp_val = VAL_0_WIDTH_3;
	}
	if (!strncmp(str, STRNG_1_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_1_WIDTH_3);
		tmp_val = VAL_1_WIDTH_3;
	}
	if (!strncmp(str, STRNG_2_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_2_WIDTH_3);
		tmp_val = VAL_2_WIDTH_3;
	}
	if (!strncmp(str, STRNG_3_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_3_WIDTH_3);
		tmp_val = VAL_3_WIDTH_3;
	}
	if (!strncmp(str, STRNG_4_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_4_WIDTH_3);
		tmp_val = VAL_4_WIDTH_3;
	}
	if (!strncmp(str, STRNG_5_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_5_WIDTH_3);
		tmp_val = VAL_5_WIDTH_3;
	}
	if (!strncmp(str, STRNG_6_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_6_WIDTH_3);
		tmp_val = VAL_6_WIDTH_3;
	}
	if (!strncmp(str, STRNG_7_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_7_WIDTH_3);
		tmp_val = VAL_7_WIDTH_3;
	}

	combined_val = tmp_val;

	val = usb20_phy_debugfs_read_val(OFFSET_RG_USB20_VRT_VREF_SEL,
					SHFT_RG_USB20_VRT_VREF_SEL,
					MSK_RG_USB20_VRT_VREF_SEL,
					BIT_WIDTH_3, str);
	if (!strncmp(str, STRNG_0_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_0_WIDTH_3);
		tmp_val = VAL_0_WIDTH_3;
	}
	if (!strncmp(str, STRNG_1_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_1_WIDTH_3);
		tmp_val = VAL_1_WIDTH_3;
	}
	if (!strncmp(str, STRNG_2_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_2_WIDTH_3);
		tmp_val = VAL_2_WIDTH_3;
	}
	if (!strncmp(str, STRNG_3_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_3_WIDTH_3);
		tmp_val = VAL_3_WIDTH_3;
	}
	if (!strncmp(str, STRNG_4_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_4_WIDTH_3);
		tmp_val = VAL_4_WIDTH_3;
	}
	if (!strncmp(str, STRNG_5_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_5_WIDTH_3);
		tmp_val = VAL_5_WIDTH_3;
	}
	if (!strncmp(str, STRNG_6_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_6_WIDTH_3);
		tmp_val = VAL_6_WIDTH_3;
	}
	if (!strncmp(str, STRNG_7_WIDTH_3, BIT_WIDTH_3)) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> %s case\n",
				__func__, __LINE__, STRNG_7_WIDTH_3);
		tmp_val = VAL_7_WIDTH_3;
	}

	pr_notice("MTK_ICUSB [DBG], <%s(), %d> combined_val(%d), tmp_val(%d)\n",
				__func__, __LINE__, combined_val, tmp_val);

	if ((tmp_val == (combined_val - 1)) || (tmp_val == combined_val))
		combined_val += tmp_val;
	else
		combined_val = tmp_val * (VAL_MAX_WDITH_3 + 1) + combined_val;

	pr_notice("MTK_ICUSB [DBG], <%s(), %d> combined_val(%d), tmp_val(%d)\n",
				__func__, __LINE__, combined_val, tmp_val);

	seq_printf(s, "%d", combined_val);
	return 0;
}

static int rg_usb20_term_vref_sel_show(struct seq_file *s, void *unused)
{
	u8 val;
	char str[16];

	val =
	    usb20_phy_debugfs_read_val(OFFSET_RG_USB20_TERM_VREF_SEL,
					SHFT_RG_USB20_TERM_VREF_SEL,
					MSK_RG_USB20_TERM_VREF_SEL,
					BIT_WIDTH_3, str);
	seq_printf(s, "%s", str);
	return 0;
}

static int rg_usb20_hstx_srctrl_show(struct seq_file *s, void *unused)
{
	u8 val;
	char str[16];

	val =
	    usb20_phy_debugfs_read_val(OFFSET_RG_USB20_HSTX_SRCTRL,
			SHFT_RG_USB20_HSTX_SRCTRL,
			MSK_RG_USB20_HSTX_SRCTRL, BIT_WIDTH_3, str);
	seq_printf(s, "%s", str);
	return 0;
}

static int rg_usb20_vrt_vref_sel_show(struct seq_file *s, void *unused)
{
	u8 val;
	char str[16];

	val =
	    usb20_phy_debugfs_read_val(OFFSET_RG_USB20_VRT_VREF_SEL,
				SHFT_RG_USB20_VRT_VREF_SEL,
				MSK_RG_USB20_VRT_VREF_SEL, BIT_WIDTH_3, str);
	seq_printf(s, "%s", str);
	return 0;
}

static int rg_usb20_intr_en_show(struct seq_file *s, void *unused)
{
	u8 val;
	char str[16];

	val =
	    usb20_phy_debugfs_read_val(OFFSET_RG_USB20_INTR_EN,
					SHFT_RG_USB20_INTR_EN,
					MSK_RG_USB20_INTR_EN, BIT_WIDTH_1, str);
	seq_printf(s, "%s", str);
	return 0;
}

static int rg_usb20_rev6_show(struct seq_file *s, void *unused)
{
	u8 val;
	char str[16];

	val =
	    usb20_phy_debugfs_read_val(OFFSET_RG_USB20_PHY_REV6,
				SHFT_RG_USB20_PHY_REV6,
				MSK_RG_USB20_PHY_REV6, BIT_WIDTH_2, str);

	seq_printf(s, "%s", str);
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

void val_to_bstring_width3(u8 val, char *str)
{

	if (val == VAL_0_WIDTH_3)
		memcpy(str, STRNG_0_WIDTH_3, BIT_WIDTH_3 + 1);
	if (val == VAL_1_WIDTH_3)
		memcpy(str, STRNG_1_WIDTH_3, BIT_WIDTH_3 + 1);
	if (val == VAL_2_WIDTH_3)
		memcpy(str, STRNG_2_WIDTH_3, BIT_WIDTH_3 + 1);
	if (val == VAL_3_WIDTH_3)
		memcpy(str, STRNG_3_WIDTH_3, BIT_WIDTH_3 + 1);
	if (val == VAL_4_WIDTH_3)
		memcpy(str, STRNG_4_WIDTH_3, BIT_WIDTH_3 + 1);
	if (val == VAL_5_WIDTH_3)
		memcpy(str, STRNG_5_WIDTH_3, BIT_WIDTH_3 + 1);
	if (val == VAL_6_WIDTH_3)
		memcpy(str, STRNG_6_WIDTH_3, BIT_WIDTH_3 + 1);
	if (val == VAL_7_WIDTH_3)
		memcpy(str, STRNG_7_WIDTH_3, BIT_WIDTH_3 + 1);

	pr_notice("MTK_ICUSB [DBG], <%s(), %d> val(%d), str(%s)\n",
				__func__, __LINE__, val, str);
}

static ssize_t usb_driving_capability_write(struct file *file,
					    const char __user *ubuf,
					    size_t count, loff_t *ppos)
{
	char buf[18];
	u8 val, tmp_val;
	char str_rg_usb20_term_vref_sel[18], str_rg_usb20_vrt_vref_sel[18];

	memset(buf, 0x00, sizeof(buf));
	pr_notice("\n");
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtol(buf, 10, (long *)&val) != 0) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> kstrtol, err(%d)\n)\n",
			__func__, __LINE__, kstrtol(buf, 10, (long *)&val));
		return count;
	}
	pr_notice("MTK_ICUSB [DBG], <%s(), %d> kstrtol, val(%d)\n)\n",
					__func__, __LINE__, val);

	if (val > VAL_7_WIDTH_3 * 2) {
		pr_notice("MTK_ICUSB [DBG], <%s(), %d> wrong val set(%d), direct return\n",
					__func__, __LINE__, val);
		return count;
	}
	tmp_val = val;
	val /= 2;

	pr_notice("MTK_ICUSB [DBG], <%s(), %d> val(%d), tmp_val(%d)\n",
				__func__, __LINE__, val, tmp_val);

	val_to_bstring_width3(tmp_val - val, str_rg_usb20_term_vref_sel);
	val_to_bstring_width3(val, str_rg_usb20_vrt_vref_sel);
	pr_notice("MTK_ICUSB [DBG], <%s(), %d> Config TERM_VREF_SEL %s\n",
				__func__, __LINE__, str_rg_usb20_term_vref_sel);
	usb20_phy_debugfs_write_width3(OFFSET_RG_USB20_TERM_VREF_SEL,
						SHFT_RG_USB20_TERM_VREF_SEL,
						str_rg_usb20_term_vref_sel);
	pr_notice("MTK_ICUSB [DBG], <%s(), %d> Config VRT_VREF_SEL %s\n\n",
				__func__, __LINE__, str_rg_usb20_vrt_vref_sel);
	usb20_phy_debugfs_write_width3(OFFSET_RG_USB20_VRT_VREF_SEL,
						SHFT_RG_USB20_VRT_VREF_SEL,
						str_rg_usb20_vrt_vref_sel);
	return count;
}

static ssize_t rg_usb20_term_vref_sel_write(struct file *file,
					    const char __user *ubuf,
					    size_t count, loff_t *ppos)
{
	char buf[18];

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;
	usb20_phy_debugfs_write_width3(OFFSET_RG_USB20_TERM_VREF_SEL,
					SHFT_RG_USB20_TERM_VREF_SEL, buf);
	return count;
}

static ssize_t rg_usb20_hstx_srctrl_write(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	char buf[18];

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;
	usb20_phy_debugfs_write_width3(OFFSET_RG_USB20_HSTX_SRCTRL,
						SHFT_RG_USB20_HSTX_SRCTRL, buf);
	return count;
}

static ssize_t rg_usb20_vrt_vref_sel_write(struct file *file,
					   const char __user *ubuf,
					   size_t count, loff_t *ppos)
{
	char buf[18];

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;
	usb20_phy_debugfs_write_width3(OFFSET_RG_USB20_VRT_VREF_SEL,
					SHFT_RG_USB20_VRT_VREF_SEL, buf);
	return count;
}

static ssize_t rg_usb20_intr_en_write(struct file *file,
				      const char __user *ubuf,
				      size_t count, loff_t *ppos)
{
	char buf[18];

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;
	usb20_phy_debugfs_write_width1(OFFSET_RG_USB20_INTR_EN,
						SHFT_RG_USB20_INTR_EN, buf);
	return count;
}

static ssize_t rg_usb20_rev6_write(struct file *file,
				      const char __user *ubuf, size_t count,
				      loff_t *ppos)
{
	char buf[18];

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;
	usb20_phy_debugfs_rev6_write(OFFSET_RG_USB20_PHY_REV6,
						SHFT_RG_USB20_PHY_REV6, buf);
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

int usb20_phy_init_debugfs(void)
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
				   root, NULL, &usb_driving_capability_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
	file = debugfs_create_file(FILE_RG_USB20_TERM_VREF_SEL, 0644,
				   root, NULL, &rg_usb20_term_vref_sel_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
	file = debugfs_create_file(FILE_RG_USB20_HSTX_SRCTRL, 0644,
				   root, NULL, &rg_usb20_hstx_srctrl_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
	file = debugfs_create_file(FILE_RG_USB20_VRT_VREF_SEL, 0644,
				   root, NULL, &rg_usb20_vrt_vref_sel_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
	file = debugfs_create_file(FILE_RG_USB20_INTR_EN, 0644,
				   root, NULL, &rg_usb20_intr_en_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
	file = debugfs_create_file(FILE_RG_USB20_PHY_REV6, 0644,
					root, NULL, &rg_usb20_rev6_fops);
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

void /* __init_or_exit */ usb20_phy_exit_debugfs(struct musb *musb)
{
	debugfs_remove_recursive(usb20_phy_debugfs_root);
}
