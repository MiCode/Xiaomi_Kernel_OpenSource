// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>

#include <linux/uaccess.h>

#ifdef CONFIG_MTK_MUSB_PHY
#include <usb20_phy.h>
#endif

#define MUSB_OTG_CSR0 0x102
#include <musb.h>
#include <musb_core.h>
#include <musb_debug.h>
#include <musb_dr.h>

#define PROC_DIR_MTK_USB "mtk_usb"
#define PROC_FILE_REGDUMP "mtk_usb/regdump"
#define PROC_FILE_TESTMODE "mtk_usb/testmode"
#define PROC_FILE_REGW "mtk_usb/regw"
#define PROC_FILE_REGR "mtk_usb/regr"
#define PROC_FILE_SPEED "mtk_usb/speed"

#define PROC_FILE_NUM 5
static struct proc_dir_entry *proc_files[PROC_FILE_NUM] = {
	NULL, NULL, NULL, NULL, NULL};

#define PROC_FILE_MODE "mtk_usb/mode"
#define PROC_FILE_VBUS "mtk_usb/vbus"

#define PROC_FILE_DR_NUM 2
static struct proc_dir_entry *proc_dr_files[PROC_FILE_DR_NUM] = {
	NULL, NULL};

struct musb_register_map {
	char *name;
	unsigned int offset;
	unsigned int size;
};

static const struct musb_register_map musb_regmap[] = {
	{"FAddr", 0x00, 8},
	{"Power", 0x01, 8},
	{"Frame", 0x0c, 16},
	{"Index", 0x0e, 8},
	{"Testmode", 0x0f, 8},
	{"TxMaxPp", 0x10, 16},
	{"TxCSRp", 0x12, 16},
	{"RxMaxPp", 0x14, 16},
	{"RxCSR", 0x16, 16},
	{"RxCount", 0x18, 16},
	{"ConfigData", 0x1f, 8},
	{"DevCtl", 0x60, 8},
	{"MISC", 0x61, 8},
	{"TxFIFOsz", 0x62, 8},
	{"RxFIFOsz", 0x63, 8},
	{"TxFIFOadd", 0x64, 16},
	{"RxFIFOadd", 0x66, 16},
	{"VControl", 0x68, 32},
	{"HWVers", 0x6C, 16},
	{"EPInfo", 0x78, 8},
	{"RAMInfo", 0x79, 8},
	{"LinkInfo", 0x7A, 8},
	{"VPLen", 0x7B, 8},
	{"HS_EOF1", 0x7C, 8},
	{"FS_EOF1", 0x7D, 8},
	{"LS_EOF1", 0x7E, 8},
	{"SOFT_RST", 0x7F, 8},
	{"DMA_CNTLch0", 0x204, 16},
	{"DMA_ADDRch0", 0x208, 32},
	{"DMA_COUNTch0", 0x20C, 32},
	{"DMA_CNTLch1", 0x214, 16},
	{"DMA_ADDRch1", 0x218, 32},
	{"DMA_COUNTch1", 0x21C, 32},
	{"DMA_CNTLch2", 0x224, 16},
	{"DMA_ADDRch2", 0x228, 32},
	{"DMA_COUNTch2", 0x22C, 32},
	{"DMA_CNTLch3", 0x234, 16},
	{"DMA_ADDRch3", 0x238, 32},
	{"DMA_COUNTch3", 0x23C, 32},
	{"DMA_CNTLch4", 0x244, 16},
	{"DMA_ADDRch4", 0x248, 32},
	{"DMA_COUNTch4", 0x24C, 32},
	{"DMA_CNTLch5", 0x254, 16},
	{"DMA_ADDRch5", 0x258, 32},
	{"DMA_COUNTch5", 0x25C, 32},
	{"DMA_CNTLch6", 0x264, 16},
	{"DMA_ADDRch6", 0x268, 32},
	{"DMA_COUNTch6", 0x26C, 32},
	{"DMA_CNTLch7", 0x274, 16},
	{"DMA_ADDRch7", 0x278, 32},
	{"DMA_COUNTch7", 0x27C, 32},
	{}			/* Terminating Entry */
};

static int musb_regdump_show(struct seq_file *s, void *unused)
{
	struct musb *musb = s->private;
	unsigned int i;

	seq_puts(s, "MUSB (M)HDRC Register Dump\n");

	for (i = 0; i < ARRAY_SIZE(musb_regmap); i++) {
		switch (musb_regmap[i].size) {
		case 8:
			seq_printf(s, "%-12s: %02x\n", musb_regmap[i].name,
				musb_readb(musb->mregs, musb_regmap[i].offset));
			break;
		case 16:
			seq_printf(s, "%-12s: %04x\n", musb_regmap[i].name,
				musb_readw(musb->mregs, musb_regmap[i].offset));
			break;
		case 32:
			seq_printf(s, "%-12s: %08x\n", musb_regmap[i].name,
				musb_readl(musb->mregs, musb_regmap[i].offset));
			break;
		}
	}

	return 0;
}

static int musb_regdump_open(struct inode *inode, struct file *file)
{
	return single_open(file, musb_regdump_show, PDE_DATA(inode));
}

static int musb_test_mode_show(struct seq_file *s, void *unused)
{
	struct musb *musb = s->private;
	unsigned int test;

	test = musb_readb(musb->mregs, MUSB_TESTMODE);

	if (test & MUSB_TEST_FORCE_HOST)
		seq_puts(s, "force host\n");

	if (test & MUSB_TEST_FIFO_ACCESS)
		seq_puts(s, "fifo access\n");

	if (test & MUSB_TEST_FORCE_FS)
		seq_puts(s, "force full-speed\n");

	if (test & MUSB_TEST_FORCE_HS)
		seq_puts(s, "force high-speed\n");

	if (test & MUSB_TEST_PACKET)
		seq_puts(s, "test packet\n");

	if (test & MUSB_TEST_K)
		seq_puts(s, "test K\n");

	if (test & MUSB_TEST_J)
		seq_puts(s, "test J\n");

	if (test & MUSB_TEST_SE0_NAK)
		seq_puts(s, "test SE0 NAK\n");

	return 0;
}

static const struct file_operations musb_regdump_fops = {
	.open = musb_regdump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int musb_test_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, musb_test_mode_show, PDE_DATA(inode));
}

void musbdebugfs_otg_write_fifo(u16 len, u8 *buf, struct musb *mtk_musb)
{
	int i;

	DBG(0, "musb_otg_write_fifo, len=%d\n", len);
	for (i = 0; i < len; i++)
		musb_writeb(mtk_musb->mregs, 0x20, *(buf + i));
}

void musbdebugfs_h_setup(struct usb_ctrlrequest *setup, struct musb *mtk_musb)
{
	unsigned short csr0;

	DBG(0, "musb_h_setup++\n");
	musbdebugfs_otg_write_fifo
		(sizeof(struct usb_ctrlrequest), (u8 *)setup, mtk_musb);
	csr0 = musb_readw(mtk_musb->mregs, MUSB_OTG_CSR0);
	DBG(0, "musb_h_setup,csr0=0x%x\n", csr0);
	csr0 |= MUSB_CSR0_H_SETUPPKT | MUSB_CSR0_TXPKTRDY;
	musb_writew(mtk_musb->mregs, MUSB_OTG_CSR0, csr0);

	DBG(0, "musb_h_setup--\n");
}

static ssize_t musb_test_mode_write(struct file *file,
			const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct musb *musb = s->private;
	u8 test = 0;
	char			buf[20];
	unsigned char power;
	struct usb_ctrlrequest setup_packet;

	setup_packet.bRequestType =
		USB_DIR_IN|USB_TYPE_STANDARD|USB_RECIP_DEVICE;
	setup_packet.bRequest = USB_REQ_GET_DESCRIPTOR;
	setup_packet.wIndex = 0;
	setup_packet.wValue = 0x0100;
	setup_packet.wLength = 0x40;

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (!strncmp(buf, "force host", 9))
		test = MUSB_TEST_FORCE_HOST;

	if (!strncmp(buf, "fifo access", 11))
		test = MUSB_TEST_FIFO_ACCESS;

	if (!strncmp(buf, "force full-speed", 15))
		test = MUSB_TEST_FORCE_FS;

	if (!strncmp(buf, "force high-speed", 15))
		test = MUSB_TEST_FORCE_HS;

	if (!strncmp(buf, "test packet", 10)) {
		test = MUSB_TEST_PACKET;
		musb_load_testpacket(musb);
	}

	if (!strncmp(buf, "test suspend_resume", 18)) {
		DBG(0, "HS_HOST_PORT_SUSPEND_RESUME\n");
		msleep(5000); /* the host must continue sending SOFs for 15s */
		DBG(0, "please begin to trigger suspend!\n");
		msleep(10000);
		power = musb_readb(musb->mregs, MUSB_POWER);
		power |= MUSB_POWER_SUSPENDM | MUSB_POWER_ENSUSPEND;
		musb_writeb(musb->mregs, MUSB_POWER, power);
		msleep(5000);
		DBG(0, "please begin to trigger resume!\n");
		msleep(10000);
		power &= ~MUSB_POWER_SUSPENDM;
		power |= MUSB_POWER_RESUME;
		musb_writeb(musb->mregs, MUSB_POWER, power);
		mdelay(25);
		power &= ~MUSB_POWER_RESUME;
		musb_writeb(musb->mregs, MUSB_POWER, power);
		/* SOF continue */
		musbdebugfs_h_setup(&setup_packet, musb);
		return count;
	}

	if (!strncmp(buf, "test get_descripter", 18)) {
		DBG(0, "SINGLE_STEP_GET_DEVICE_DESCRIPTOR\n");
		/* the host issues SOFs for 15s allowing the test engineer
		 * to raise the scope trigger just above the SOF voltage level.
		 */
		msleep(15000);
		musbdebugfs_h_setup(&setup_packet, musb);
		return count;
	}

	if (!strncmp(buf, "test K", 6))
		test = MUSB_TEST_K;

	if (!strncmp(buf, "test J", 6))
		test = MUSB_TEST_J;

	if (!strncmp(buf, "test SE0 NAK", 12))
		test = MUSB_TEST_SE0_NAK;

	musb_writeb(musb->mregs, MUSB_TESTMODE, test);

	return count;
}

static const struct file_operations musb_test_mode_fops = {
	.open = musb_test_mode_open,
	.write = musb_test_mode_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static inline int my_isspace(char c)
{
	return (c == ' ' || c == '\t' || c == '\n' || c == '\12');
}

static inline int my_isupper(char c)
{
	return (c >= 'A' && c <= 'Z');
}

static inline int my_isalpha(char c)
{
	return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}

static inline int my_isdigit(char c)
{
	return (c >= '0' && c <= '9');
}

static unsigned my_strtoul
	(const char *nptr, char **endptr, unsigned int base)
{
	const char *s = nptr;
	unsigned long acc;
	int c;
	unsigned long cutoff;
	int neg = 0, any, cutlim;

	do {
		c = *s++;
	} while (my_isspace(c));
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else if (c == '+')
		c = *s++;

	if ((base == 0 || base == 16) &&
		c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	} else if ((base == 0 || base == 2) &&
			c == '0' && (*s == 'b' || *s == 'B')) {
		c = s[1];
		s += 2;
		base = 2;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;

	cutoff = (unsigned long)ULONG_MAX / (unsigned long)base;
	cutlim = (unsigned long)ULONG_MAX % (unsigned long)base;

	for (acc = 0, any = 0;; c = *s++) {
		if (my_isdigit(c))
			c -= '0';
		else if (my_isalpha(c))
			c -= my_isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;

		if (c >= base)
			break;
		if ((any < 0 || acc > cutoff || acc == cutoff) && c > cutlim)
			any = -1;
		else {
			any = 1;
			acc *= base;
			acc += c;
		}
	}
	if (any < 0)
		acc = ULONG_MAX;
	else if (neg)
		acc = -acc;

	if (endptr != 0)
		*endptr = (char *)(any ? s - 1 : nptr);

	return acc;
}

static int musb_regw_show(struct seq_file *s, void *unused)
{
	DBG(0, "%s -> Called\n", __func__);

	pr_notice("Uage:\n");
	pr_notice("Mac Write: echo mac:addr:data > regw\n");
	pr_notice("Phy Write: echo phy:addr:data > regw\n");

	return 0;
}

static int musb_regw_open(struct inode *inode, struct file *file)
{
	return single_open(file, musb_regw_show, PDE_DATA(inode));
}

static ssize_t musb_regw_mode_write
	(struct file *file,	const char __user *ubuf
	, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct musb *musb = s->private;
	char			buf[20];
	u8 is_mac = 0;
	char *tmp1 = NULL;
	char *tmp2 = NULL;
	unsigned int offset = 0;
	u8 data = 0;

	memset(buf, 0x00, sizeof(buf));

	pr_notice("%s -> Called\n", __func__);

	if (copy_from_user(buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	buf[19] = '\0';

	if ((!strncmp(buf, "MAC", 3)) || (!strncmp(buf, "mac", 3)))
		is_mac = 1;
	else if ((!strncmp(buf, "PHY", 3)) || (!strncmp(buf, "phy", 3)))
		is_mac = 0;
	else
		return -EFAULT;

	tmp1 = strchr(buf, ':');
	if (tmp1 == NULL)
		return -EFAULT;
	tmp1++;
	if (strlen(tmp1) == 0)
		return -EFAULT;

	tmp2 = strrchr(buf, ':');
	if (tmp2 == NULL)
		return -EFAULT;
	tmp2++;
	if (strlen(tmp2) == 0)
		return -EFAULT;


	offset = my_strtoul(tmp1, NULL, 0);
	data = my_strtoul(tmp2, NULL, 0);

	if (is_mac == 1) {
		pr_notice("Mac base adddr 0x%lx, Write 0x%x[0x%x]\n",
			(unsigned long)musb->mregs, offset, data);
		musb_writeb(musb->mregs, offset, data);
	} else {
		if ((offset % 4) != 0) {
			pr_notice("Must use 32bits alignment address\n");
			return count;
		}
#ifdef CONFIG_MTK_MUSB_PHY
		pr_notice("Phy base adddr 0x%lx, Write 0x%x[0x%x]\n",
		(unsigned long)((void __iomem *)
		(((unsigned long)musb->xceiv->io_priv)
		+ 0x800)), offset, data);
		USBPHY_WRITE32(offset, data);
#endif
	}

	return count;
}

static const struct file_operations musb_regw_fops = {
	.open = musb_regw_open,
	.write = musb_regw_mode_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int musb_regr_show(struct seq_file *s, void *unused)
{
	DBG(0, "%s -> Called\n"
			"Uage:\n"
			"Mac Read: echo mac:addr > regr\n"
			"Phy Read: echo phy:addr > regr\n", __func__);

	return 0;
}

static int musb_regr_open(struct inode *inode, struct file *file)
{
	return single_open(file, musb_regr_show, PDE_DATA(inode));
}

static ssize_t musb_regr_mode_write(struct file *file,
				    const char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct musb *musb = s->private;
	char			buf[20];
	u8 is_mac = 0;
	char *tmp = NULL;
	unsigned int offset = 0;

	memset(buf, 0x00, sizeof(buf));

	pr_notice("%s -> Called\n", __func__);

	if (copy_from_user(buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	buf[19] = '\0';

	if ((!strncmp(buf, "MAC", 3)) || (!strncmp(buf, "mac", 3)))
		is_mac = 1;
	else if ((!strncmp(buf, "PHY", 3)) || (!strncmp(buf, "phy", 3)))
		is_mac = 0;
	else
		return -EFAULT;

	tmp = strrchr(buf, ':');

	if (tmp == NULL)
		return -EFAULT;

	tmp++;

	if (strlen(tmp) == 0)
		return -EFAULT;

	offset = my_strtoul(tmp, NULL, 0);

	if (is_mac == 1)
		pr_notice("Read Mac base adddr 0x%lx, Read 0x%x[0x%x]\n",
			(unsigned long)musb->mregs, offset
			, musb_readb(musb->mregs, offset));
	else {
		if ((offset % 4) != 0) {
			pr_notice("Must use 32bits alignment address\n");
			return count;
		}
#ifdef CONFIG_MTK_MUSB_PHY
		pr_notice("Read Phy base adddr 0x%lx, Read 0x%x[0x%x]\n",
			(unsigned long)((void __iomem *)
			(((unsigned long)musb->xceiv->io_priv)
			+ 0x800)), offset,
			USBPHY_READ32(offset));
#endif
	}

	return count;
}

static const struct file_operations musb_regr_fops = {
	.open = musb_regr_open,
	.write = musb_regr_mode_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int musb_speed_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "musb_speed = %d\n", musb_speed);
	return 0;
}

static int musb_speed_open(struct inode *inode, struct file *file)
{
	return single_open(file, musb_speed_show, PDE_DATA(inode));
}

static ssize_t musb_speed_write(struct file *file,
			const char __user *ubuf, size_t count, loff_t *ppos)
{
	char buf[20];
	unsigned int val;

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 10, &val) == 0 && val >= 0 && val <= 1)
		musb_speed = val;
	else
		return -EINVAL;

	return count;
}

static const struct file_operations musb_speed_fops = {
	.open = musb_speed_open,
	.write = musb_speed_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int musb_init_debugfs(struct musb *musb)
{
	int ret, idx = 0;

	proc_mkdir(PROC_DIR_MTK_USB, NULL);

	proc_files[idx] = proc_create_data(PROC_FILE_REGDUMP, 0444,
			NULL, &musb_regdump_fops, musb);
	if (!proc_files[idx]) {
		ret = -ENOMEM;
		goto err1;
	}
	idx++;

	proc_files[idx] = proc_create_data(PROC_FILE_TESTMODE, 0644,
			NULL, &musb_test_mode_fops, musb);
	if (!proc_files[idx]) {
		ret = -ENOMEM;
		goto err1;
	}
	idx++;

	proc_files[idx] = proc_create_data(PROC_FILE_REGW, 0644,
			NULL, &musb_regw_fops, musb);
	if (!proc_files[idx]) {
		ret = -ENOMEM;
		goto err1;
	}
	idx++;

	proc_files[idx] = proc_create_data(PROC_FILE_REGR, 0644,
			NULL, &musb_regr_fops, musb);
	if (!proc_files[idx]) {
		ret = -ENOMEM;
		goto err1;
	}
	idx++;

	proc_files[idx] = proc_create_data(PROC_FILE_SPEED, 0644,
			NULL, &musb_speed_fops, musb);
	if (!proc_files[idx]) {
		ret = -ENOMEM;
		goto err1;
	}

	return 0;

err1:
	for (; idx >= 0; idx--) {
		if (proc_files[idx]) {
			proc_remove(proc_files[idx]);
			proc_files[idx] = NULL;
		}
	}

	return ret;
}

void /* __init_or_exit */ musb_exit_debugfs(struct musb *musb)
{
	int idx = 0;

	for (; idx < PROC_FILE_NUM; idx++) {
		if (proc_files[idx]) {
			proc_remove(proc_files[idx]);
			proc_files[idx] = NULL;
		}
	}
}

static int musb_mode_show(struct seq_file *sf, void *unused)
{
	struct musb *musb = sf->private;
	struct mt_usb_glue *glue =
		container_of(&musb, struct mt_usb_glue, mtk_musb);

	seq_printf(sf, "current mode: %s(%s drd)\n(echo device/host)\n",
		   musb->is_host ? "host" : "device",
		   glue->otg_sx.manual_drd_enabled ? "manual" : "auto");

	return 0;
}

static int musb_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, musb_mode_show, PDE_DATA(inode));
}

static ssize_t musb_mode_write(struct file *file, const char __user *ubuf,
				size_t count, loff_t *ppos)
{
	struct seq_file *sf = file->private_data;
	struct musb *musb = sf->private;
	char buf[16];

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (!strncmp(buf, "host", 4) && !musb->is_host) {
		mt_usb_mode_switch(musb, 1);
	} else if (!strncmp(buf, "device", 6) && musb->is_host) {
		mt_usb_mode_switch(musb, 0);
	} else {
		dev_err(musb->controller, "wrong or duplicated setting\n");
		return -EINVAL;
	}

	return count;
}

static const struct file_operations musb_mode_fops = {
	.open = musb_mode_open,
	.write = musb_mode_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int musb_vbus_show(struct seq_file *sf, void *unused)
{
	struct musb *musb = sf->private;
	struct mt_usb_glue *glue =
		container_of(&musb, struct mt_usb_glue, mtk_musb);
	struct otg_switch_mtk *otg_sx = &glue->otg_sx;

	seq_printf(sf, "vbus state: %s\n(echo on/off)\n",
		   regulator_is_enabled(otg_sx->vbus) ? "on" : "off");

	return 0;
}

static int musb_vbus_open(struct inode *inode, struct file *file)
{
	return single_open(file, musb_vbus_show, PDE_DATA(inode));
}

static ssize_t musb_vbus_write(struct file *file, const char __user *ubuf,
				size_t count, loff_t *ppos)
{
	struct seq_file *sf = file->private_data;
	struct musb *musb = sf->private;
	struct mt_usb_glue *glue =
		container_of(&musb, struct mt_usb_glue, mtk_musb);
	struct otg_switch_mtk *otg_sx = &glue->otg_sx;
	char buf[16];
	bool enable;

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtobool(buf, &enable)) {
		dev_err(musb->controller, "wrong setting\n");
		return -EINVAL;
	}

	mt_usb_set_vbus(otg_sx, enable);

	return count;
}

static const struct file_operations musb_vbus_fops = {
	.open = musb_vbus_open,
	.write = musb_vbus_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void musb_dr_debugfs_init(struct musb *musb)
{
	int idx = 0;

	proc_mkdir(PROC_DIR_MTK_USB, NULL);

	proc_dr_files[idx++] = proc_create_data(PROC_FILE_MODE, 0644,
			NULL, &musb_mode_fops, musb);

	proc_dr_files[idx++] = proc_create_data(PROC_FILE_VBUS, 0644,
			NULL, &musb_vbus_fops, musb);
}
