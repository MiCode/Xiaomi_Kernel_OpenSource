/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
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

#define BIT_WIDTH_4		4
#define MSK_WIDTH_4		0xf
#define VAL_MAX_WDITH_4		0xf
#define VAL_0_WIDTH_4		0x0
#define VAL_1_WIDTH_4		0x1
#define VAL_2_WIDTH_4		0x2
#define VAL_3_WIDTH_4		0x3
#define VAL_4_WIDTH_4		0x4
#define VAL_5_WIDTH_4		0x5
#define VAL_6_WIDTH_4		0x6
#define VAL_7_WIDTH_4		0x7
#define VAL_8_WIDTH_4		0x8
#define VAL_9_WIDTH_4		0x9
#define VAL_A_WIDTH_4		0xa
#define VAL_B_WIDTH_4		0xb
#define VAL_C_WIDTH_4		0xc
#define VAL_D_WIDTH_4		0xd
#define VAL_E_WIDTH_4		0xe
#define VAL_F_WIDTH_4		0xf
#define STRNG_0_WIDTH_4	"0000"
#define STRNG_1_WIDTH_4	"0001"
#define STRNG_2_WIDTH_4	"0010"
#define STRNG_3_WIDTH_4	"0011"
#define STRNG_4_WIDTH_4	"0100"
#define STRNG_5_WIDTH_4	"0101"
#define STRNG_6_WIDTH_4	"0110"
#define STRNG_7_WIDTH_4	"0111"
#define STRNG_8_WIDTH_4	"1000"
#define STRNG_9_WIDTH_4	"1001"
#define STRNG_A_WIDTH_4	"1010"
#define STRNG_B_WIDTH_4	"1011"
#define STRNG_C_WIDTH_4	"1100"
#define STRNG_D_WIDTH_4	"1101"
#define STRNG_E_WIDTH_4	"1110"
#define STRNG_F_WIDTH_4	"1111"

#define BIT_WIDTH_5		5
#define MSK_WIDTH_5		0x1f
#define VAL_MAX_WDITH_5		0x1f
#define VAL_0_WIDTH_5		0x0
#define VAL_1_WIDTH_5		0x1
#define VAL_18_WIDTH_5		0x12
#define VAL_20_WIDTH_5		0x14
#define VAL_30_WIDTH_5		0x1e
#define VAL_31_WIDTH_5		0x1f
#define STRNG_0_WIDTH_5		"00000"
#define STRNG_1_WIDTH_5		"00001"
#define STRNG_18_WIDTH_5	"10010"
#define STRNG_20_WIDTH_5	"10100"
#define STRNG_30_WIDTH_5	"11110"
#define STRNG_31_WIDTH_5	"11111"

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

#define FILE_RG_USB20_INTR_CAL "RG_USB20_INTR_CAL"
#define MSK_RG_USB20_INTR_CAL MSK_WIDTH_5
#define SHFT_RG_USB20_INTR_CAL 19
#define OFFSET_RG_USB20_INTR_CAL 0x4

#define FILE_RG_USB20_DISCTH "RG_USB20_DISCTH"
#define MSK_RG_USB20_DISCTH MSK_WIDTH_4
#define SHFT_RG_USB20_DISCTH 4
#define OFFSET_RG_USB20_DISCTH 0x18

#define FILE_REG_DEBUG "phy_reg"

static struct proc_dir_entry *usb20_phy_procfs_root;
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

static void usb20_phy_debugfs_write_width4(struct phy *phy, u8 offset, u8 shift,
	char *buf)
{
	u32 set_val = 0;

	pr_debug("s(%s)\n", buf);
	if (!strncmp(buf, STRNG_0_WIDTH_4, BIT_WIDTH_4)) {
		pr_debug("%s case\n", STRNG_0_WIDTH_4);
		set_val = VAL_0_WIDTH_4;
	} else if (!strncmp(buf, STRNG_1_WIDTH_4, BIT_WIDTH_4)) {
		pr_debug("%s case\n", STRNG_1_WIDTH_4);
		set_val = VAL_1_WIDTH_4;
	} else if (!strncmp(buf, STRNG_2_WIDTH_4, BIT_WIDTH_4)) {
		pr_debug("%s case\n", STRNG_2_WIDTH_4);
		set_val = VAL_2_WIDTH_4;
	} else if (!strncmp(buf, STRNG_3_WIDTH_4, BIT_WIDTH_4)) {
		pr_debug("%s case\n", STRNG_3_WIDTH_4);
		set_val = VAL_3_WIDTH_4;
	} else if (!strncmp(buf, STRNG_4_WIDTH_4, BIT_WIDTH_4)) {
		pr_debug("%s case\n", STRNG_4_WIDTH_4);
		set_val = VAL_4_WIDTH_4;
	} else if (!strncmp(buf, STRNG_5_WIDTH_4, BIT_WIDTH_4)) {
		pr_debug("%s case\n", STRNG_5_WIDTH_4);
		set_val = VAL_5_WIDTH_4;
	} else if (!strncmp(buf, STRNG_6_WIDTH_4, BIT_WIDTH_4)) {
		pr_debug("%s case\n", STRNG_6_WIDTH_4);
		set_val = VAL_6_WIDTH_4;
	} else if (!strncmp(buf, STRNG_7_WIDTH_4, BIT_WIDTH_4)) {
		pr_debug("%s case\n", STRNG_7_WIDTH_4);
		set_val = VAL_7_WIDTH_4;
	} else if (!strncmp(buf, STRNG_8_WIDTH_4, BIT_WIDTH_4)) {
		pr_debug("%s case\n", STRNG_8_WIDTH_4);
		set_val = VAL_8_WIDTH_4;
	} else if (!strncmp(buf, STRNG_9_WIDTH_4, BIT_WIDTH_4)) {
		pr_debug("%s case\n", STRNG_9_WIDTH_4);
		set_val = VAL_9_WIDTH_4;
	} else if (!strncmp(buf, STRNG_A_WIDTH_4, BIT_WIDTH_4)) {
		pr_debug("%s case\n", STRNG_A_WIDTH_4);
		set_val = VAL_A_WIDTH_4;
	} else if (!strncmp(buf, STRNG_B_WIDTH_4, BIT_WIDTH_4)) {
		pr_debug("%s case\n", STRNG_B_WIDTH_4);
		set_val = VAL_B_WIDTH_4;
	} else if (!strncmp(buf, STRNG_C_WIDTH_4, BIT_WIDTH_4)) {
		pr_debug("%s case\n", STRNG_C_WIDTH_4);
		set_val = VAL_C_WIDTH_4;
	} else if (!strncmp(buf, STRNG_D_WIDTH_4, BIT_WIDTH_4)) {
		pr_debug("%s case\n", STRNG_D_WIDTH_4);
		set_val = VAL_D_WIDTH_4;
	} else if (!strncmp(buf, STRNG_E_WIDTH_4, BIT_WIDTH_4)) {
		pr_debug("%s case\n", STRNG_E_WIDTH_4);
		set_val = VAL_E_WIDTH_4;
	} else if (!strncmp(buf, STRNG_F_WIDTH_4, BIT_WIDTH_4)) {
		pr_debug("%s case\n", STRNG_F_WIDTH_4);
		set_val = VAL_F_WIDTH_4;
	} else
		return;

	u3phywrite32(phy, offset, MSK_WIDTH_4 << shift, set_val << shift);

}

static void usb20_phy_debugfs_write_width5(struct phy *phy, u8 offset, u8 shift,
	char *buf)
{
	u32 set_val = 0;

	pr_debug("s(%s)\n", buf);
	if (!strncmp(buf, STRNG_0_WIDTH_5, BIT_WIDTH_5)) {
		pr_debug("%s case\n", STRNG_0_WIDTH_5);
		set_val = VAL_0_WIDTH_5;
	} else if (!strncmp(buf, STRNG_1_WIDTH_5, BIT_WIDTH_5)) {
		pr_debug("%s case\n", STRNG_1_WIDTH_5);
		set_val = VAL_1_WIDTH_5;
	} else if (!strncmp(buf, STRNG_18_WIDTH_5, BIT_WIDTH_5)) {
		pr_debug("%s case\n", STRNG_18_WIDTH_5);
		set_val = VAL_18_WIDTH_5;
	} else if (!strncmp(buf, STRNG_20_WIDTH_5, BIT_WIDTH_5)) {
		pr_debug("%s case\n", STRNG_20_WIDTH_5);
		set_val = VAL_20_WIDTH_5;
	} else if (!strncmp(buf, STRNG_30_WIDTH_5, BIT_WIDTH_5)) {
		pr_debug("%s case\n", STRNG_30_WIDTH_5);
		set_val = VAL_30_WIDTH_5;
	} else if (!strncmp(buf, STRNG_31_WIDTH_5, BIT_WIDTH_5)) {
		pr_debug("%s case\n", STRNG_31_WIDTH_5);
		set_val = VAL_31_WIDTH_5;
	} else
		return;

	u3phywrite32(phy, offset, MSK_WIDTH_5 << shift, set_val << shift);

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

	seq_printf(s, "%s = %d\n", FILE_USB_DRIVING_CAPABILITY, combined_val);
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

	seq_printf(s, "%s= %s\n", FILE_RG_USB20_TERM_VREF_SEL, str);
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

	seq_printf(s, "%s = %s\n", FILE_RG_USB20_HSTX_SRCTRL, str);
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

	seq_printf(s, "%s = %s\n", FILE_RG_USB20_VRT_VREF_SEL, str);
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

	seq_printf(s, "%s = %s\n", FILE_RG_USB20_INTR_EN, str);
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

	seq_printf(s, "%s = %s\n", FILE_RG_USB20_PHY_REV6, str);
	return 0;
}

static int rg_usb20_intr_cal_show(struct seq_file *s, void *unused)
{
	struct phy *phy = s->private;
	u32 val;
	char str[16];

	val = usb_mtkphy_io_read(phy, (OFFSET_RG_USB20_INTR_CAL));
	val = val >> SHFT_RG_USB20_INTR_CAL;
	val = val & MSK_RG_USB20_INTR_CAL;
	val = usb20_phy_debugfs_read_val(val, BIT_WIDTH_5, str);

	seq_printf(s, "%s = %s\n", FILE_RG_USB20_INTR_CAL, str);
	return 0;
}

static int rg_usb20_discth_show(struct seq_file *s, void *unused)
{
	struct phy *phy = s->private;
	u32 val;
	char str[16];

	val = usb_mtkphy_io_read(phy, (OFFSET_RG_USB20_DISCTH));
	val = val >> SHFT_RG_USB20_DISCTH;
	val = val & MSK_RG_USB20_DISCTH;
	val = usb20_phy_debugfs_read_val(val, BIT_WIDTH_4, str);

	seq_printf(s, "%s = %s\n", FILE_RG_USB20_DISCTH, str);
	return 0;
}

static int phy_rw_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "phy[0x%x] = 0x%x\n", ippc_addr, ippc_value);

	return 0;
}

static int usb_driving_capability_open(struct inode *inode, struct file *file)
{
	return single_open(file, usb_driving_capability_show, PDE_DATA(inode));
}

static int rg_usb20_term_vref_sel_open(struct inode *inode, struct file *file)
{
	return single_open(file, rg_usb20_term_vref_sel_show, PDE_DATA(inode));
}

static int rg_usb20_hstx_srctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, rg_usb20_hstx_srctrl_show, PDE_DATA(inode));
}

static int rg_usb20_vrt_vref_sel_open(struct inode *inode, struct file *file)
{
	return single_open(file, rg_usb20_vrt_vref_sel_show, PDE_DATA(inode));
}

static int rg_usb20_intr_en_open(struct inode *inode, struct file *file)
{
	return single_open(file, rg_usb20_intr_en_show, PDE_DATA(inode));
}

static int rg_usb20_rev6_open(struct inode *inode, struct file *file)
{
	return single_open(file, rg_usb20_rev6_show, PDE_DATA(inode));
}

static int rg_usb20_intr_cal_open(struct inode *inode, struct file *file)
{
	return single_open(file, rg_usb20_intr_cal_show, PDE_DATA(inode));
}

static int rg_usb20_discth_open(struct inode *inode, struct file *file)
{
	return single_open(file, rg_usb20_discth_show, PDE_DATA(inode));
}

static int phy_rw_open(struct inode *inode, struct file *file)
{
	return single_open(file, phy_rw_show, PDE_DATA(inode));
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

	if (kstrtol(buf, 10, (long *)&val) != 0) {
		pr_debug("kstrtol, err(%d)\n", kstrtol(buf, 10, (long *)&val));
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

static ssize_t rg_usb20_intr_cal_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct phy *phy = s->private;
	char buf[18];

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;
	usb20_phy_debugfs_write_width5(phy, OFFSET_RG_USB20_INTR_CAL,
		SHFT_RG_USB20_INTR_CAL, buf);
	return count;
}

static ssize_t rg_usb20_discth_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct phy *phy = s->private;
	char buf[18];

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;
	usb20_phy_debugfs_write_width4(phy, OFFSET_RG_USB20_DISCTH,
		SHFT_RG_USB20_DISCTH, buf);
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

static const struct file_operations rg_usb20_intr_cal_fops = {
	.open = rg_usb20_intr_cal_open,
	.write = rg_usb20_intr_cal_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations rg_usb20_discth_fops = {
	.open = rg_usb20_discth_open,
	.write = rg_usb20_discth_write,
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
	struct proc_dir_entry *root;
	struct proc_dir_entry *file;
	int ret;

	proc_mkdir("mtk_usb", NULL);

	root = proc_mkdir("mtk_usb/usb20_phy", NULL);
	if (!root) {
		ret = -ENOMEM;
		goto err0;
	}

	file = proc_create_data(FILE_USB_DRIVING_CAPABILITY, 0644,
				   root, &usb_driving_capability_fops, phy);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
	file = proc_create_data(FILE_RG_USB20_TERM_VREF_SEL, 0644,
				   root, &rg_usb20_term_vref_sel_fops, phy);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
	file = proc_create_data(FILE_RG_USB20_HSTX_SRCTRL, 0644,
				   root, &rg_usb20_hstx_srctrl_fops, phy);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
	file = proc_create_data(FILE_RG_USB20_VRT_VREF_SEL, 0644,
				   root, &rg_usb20_vrt_vref_sel_fops, phy);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
	file = proc_create_data(FILE_RG_USB20_INTR_EN, 0644,
				   root, &rg_usb20_intr_en_fops, phy);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
	file = proc_create_data(FILE_RG_USB20_PHY_REV6, 0644,
				   root, &rg_usb20_rev6_fops, phy);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
	file = proc_create_data(FILE_RG_USB20_INTR_CAL, 0644,
				   root, &rg_usb20_intr_cal_fops, phy);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
	file = proc_create_data(FILE_RG_USB20_DISCTH, 0644,
				   root, &rg_usb20_discth_fops, phy);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
	file = proc_create_data(FILE_REG_DEBUG, 0644,
				   root, &phy_rw_fops, phy);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}

	usb20_phy_procfs_root = root;
	return 0;

err1:
	proc_remove(root);

err0:
	return ret;
}

int mtu3_phy_exit_debugfs(void)
{
	proc_remove(usb20_phy_procfs_root);
	return 0;
}
