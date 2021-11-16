/*
 * MUSB OTG driver debugfs support
 *
 * Copyright 2010 Nokia Corporation
 * Contact: Felipe Balbi <felipe.balbi@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <linux/uaccess.h>

#include "musb_core.h"
#include "musb_debug.h"
#include "mu3d_hal_phy.h"
#include "mu3d_hal_usb_drv.h"
#include "mu3d_hal_hw.h"
#include "mu3d_hal_qmu_drv.h"

#ifdef CONFIG_PROJECT_PHY
#include "mtk-phy-asic.h"
#endif

struct musb_register_map {
	char *name;
	unsigned offset;
	unsigned size;
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
	unsigned i;

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
	return single_open(file, musb_regdump_show, inode->i_private);
}

static const struct file_operations musb_regdump_fops = {
	.open = musb_regdump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int musb_test_mode_show(struct seq_file *s, void *unused)
{
	struct musb *musb = s->private;
	unsigned test;

	test = musb_readb(musb->mregs, MUSB_TESTMODE);

	if (test & FORCE_HOST)
		seq_puts(s, "force host\n");

	if (test & FIFO_ACCESS)
		seq_puts(s, "fifo access\n");

	if (test & FORCE_FS)
		seq_puts(s, "force full-speed\n");

	if (test & FORCE_HS)
		seq_puts(s, "force high-speed\n");

	if (test & TEST_PACKET_MODE)
		seq_puts(s, "test packet\n");

	if (test & TEST_K_MODE)
		seq_puts(s, "test K\n");

	if (test & TEST_J_MODE)
		seq_puts(s, "test J\n");

	if (test & TEST_SE0_NAK_MODE)
		seq_puts(s, "test SE0 NAK\n");

	return 0;
}

static int musb_test_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, musb_test_mode_show, inode->i_private);
}

static ssize_t musb_test_mode_write(struct file *file,
				    const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct musb *musb = s->private;
	u8 test = 0;
	char buf[18];

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (!strncmp(buf, "force host", 9))
		test = FORCE_HOST;

	if (!strncmp(buf, "fifo access", 11))
		test = FIFO_ACCESS;

	if (!strncmp(buf, "force full-speed", 15))
		test = FORCE_FS;

	if (!strncmp(buf, "force high-speed", 15))
		test = FORCE_HS;

	if (!strncmp(buf, "test packet", 10)) {
		test = TEST_PACKET_MODE;
		musb_load_testpacket(musb);
	}

	if (!strncmp(buf, "test K", 6)) {
		test = TEST_K_MODE;
#ifdef CONFIG_PROJECT_PHY
	usb20_rev6_setting(0, false);
#endif
	}
	if (!strncmp(buf, "test J", 6)) {
		test = TEST_J_MODE;
#ifdef CONFIG_PROJECT_PHY
	usb20_rev6_setting(0, false);
#endif
	}
	if (!strncmp(buf, "test SE0 NAK", 12))
		test = TEST_SE0_NAK_MODE;

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

#ifdef USE_SSUSB_QMU

static int musb_usb_reg_show(struct seq_file *s, void *unused)
{
	int i;

	for (i = 1; i < 9; ++i) {
		unsigned int *addr = 0;
		uintptr_t tmp;

		seq_printf(s, "EP[%d] CSR0=%08x, CSR1=%08x, CSR2=%08x\n", i,
			   (unsigned int)USB_ReadCsr32(U3D_TX1CSR0, i),
			   (unsigned int)USB_ReadCsr32(U3D_TX1CSR1, i),
			   (unsigned int)USB_ReadCsr32(U3D_TX1CSR2, i));

		seq_printf(s, "TxQ CPR=%08x, CSR=%08x\n",
			   (unsigned int)os_readl(USB_QMU_TQCPR(i)),
			   (unsigned int)os_readl(USB_QMU_TQCSR(i)));

		tmp = (uintptr_t) os_readl(USB_QMU_TQCPR(i));	/* QMU GPD address --> CPU DMA address */

		if (tmp != 0) {
			/* addr = (unsigned int *)phys_to_virt(tmp); */
			addr = (unsigned int *)gpd_phys_to_virt((void *)tmp, USB_TX, i);

			/*seq_printf(s, "GPD[%08x] %08x, %08x, %08x, %08x\n",
			 *  addr, (*(unsigned int*)addr),
			 *  (*(unsigned int*)(addr+1)),
			 *  (*(unsigned int*)(addr+2)),
			 *  (*(unsigned int*)(addr+3)));
			 */

			seq_printf(s, "GPD[%p] HWO=%d, BPD=%d, Next_GPD=%lx, DataBuffer=%lx, BufferLen=%d\n",
				addr, (u32) TGPD_GET_FLAG(addr),
				(u32) TGPD_GET_FORMAT(addr), (uintptr_t) TGPD_GET_NEXT(addr),
				(uintptr_t) TGPD_GET_DATA(addr), (u32) TGPD_GET_BUF_LEN(addr));
		}

		seq_printf(s, "EP[%d] CSR0=%08x, CSR1=%08x, CSR2=%08x, CSR3=%08x\n", i,
			   (unsigned int)USB_ReadCsr32(U3D_RX1CSR0, i),
			   (unsigned int)USB_ReadCsr32(U3D_RX1CSR1, i),
			   (unsigned int)USB_ReadCsr32(U3D_RX1CSR2, i),
			   (unsigned int)USB_ReadCsr32(U3D_RX1CSR3, i));

		seq_printf(s, "RxQ CPR=%08x, CSR=%08x, LDPR=%08x\n",
			   (unsigned int)os_readl(USB_QMU_RQCPR(i)),
			   (unsigned int)os_readl(USB_QMU_RQCSR(i)),
			   (unsigned int)os_readl(USB_QMU_RQLDPR(i)));

		tmp = (uintptr_t) os_readl(USB_QMU_RQCPR(i));

		if (tmp != 0) {
			/* addr = (unsigned int *)phys_to_virt(tmp); */
			addr = (unsigned int *)gpd_phys_to_virt((void *)tmp, USB_RX, i);

			/*seq_printf(s, "GPD[%08x] %08x, %08x, %08x, %08x\n",
			 *  addr, (*(unsigned int*)addr),
			 *  (*(unsigned int*)(addr+1)),
			 *  (*(unsigned int*)(addr+2)),
			 *  (*(unsigned int*)(addr+3)));
			 */

			seq_printf(s, "GPD[%p] HWO=%d, Next_GPD=%lx ,DataBufLen=%d, DataBuf=%lx, RecvLen=%d, Endpoint=%d\n",
					 addr, (u32) TGPD_GET_FLAG(addr), (uintptr_t) TGPD_GET_NEXT(addr),
				   (u32) TGPD_GET_DataBUF_LEN(addr),
				   (uintptr_t) TGPD_GET_DATA(addr), (u32) TGPD_GET_BUF_LEN(addr),
				   (u32) TGPD_GET_EPaddr(addr));
		}

		seq_puts(s, "---------------\n\n");
	}

	return 0;
}

static int musb_usb_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, musb_usb_reg_show, inode->i_private);
}

static ssize_t musb_usb_reg_write(struct file *file,
				  const char __user *ubuf, size_t count, loff_t *ppos)
{
	char buf[18];
	char dir;
	int ep;

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;
	if (sscanf(buf, "resume %c %d", &dir, &ep) == 2) {
		pr_debug("%s %c %d\n", __func__, dir, ep);

		if (dir == 'R' || dir == 'r') {
			pr_debug("%s resume EP[%d]-R\n", __func__, ep);
			os_writel(USB_QMU_RQCSR(ep), QMU_Q_RESUME);
			/* mu3d_hal_resume_qmu(ep, USB_RX); */
		} else {
			pr_debug("%s resume EP[%d]-T\n", __func__, ep);
			os_writel(USB_QMU_TQCSR(ep), QMU_Q_RESUME);
			/* mu3d_hal_resume_qmu(ep, USB_TX); */
		}
	}
	return count;
}


static const struct file_operations musb_usb_reg_fops = {
	.open = musb_usb_reg_open,
	.write = musb_usb_reg_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#ifdef CONFIG_FPGA_EARLY_PORTING
static int musb_scan_phase_show(struct seq_file *s, void *unused)
{
	seq_puts(s, "#echo scan l=[0|1] d=[0~3] > scan_phase\n");
	seq_puts(s, "#echo linkup[0|1] > scan_phase\n");

	return 0;
}


static int musb_scan_phase(struct seq_file *s, int latch, int driving)
{
	struct musb *musb = s->private;

	disable_irq(musb->nIrq);

	os_printk(K_INFO, "Init PHY\n");
	u3phy_ops->init(u3phy);

	os_printk(K_INFO, "Plug in the USB cable 3\n");
	mdelay(1000);
	os_printk(K_INFO, "Plug in the USB cable 2\n");
	mdelay(1000);
	os_printk(K_INFO, "Plug in the USB cable 1\n");
	mdelay(1000);

	os_printk(K_INFO, "PHY SCAN latch=%d, drivind=%d\n", latch, driving);
	mu3d_hal_phy_scan(latch, driving);
	seq_puts(s, "Finish--\n");

	return 0;
}

static int musb_scan_phase_open(struct inode *inode, struct file *file)
{
	return single_open(file, musb_scan_phase_show, inode->i_private);
}

static ssize_t musb_scan_phase_write(struct file *file,
				     const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	char buf[18];

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (!strncmp(buf, "linkup1", 7)) {
		int status = RET_SUCCESS;
		int cnt = 10;

		mu3d_hal_link_up(1);
		mdelay(500);
		do {
			if ((os_readl(U3D_LINK_STATE_MACHINE) & LTSSM) != STATE_U0_STATE) {
				status = RET_FAIL;
				break;
			}
			mdelay(50);
		} while (cnt--);

		if (status != RET_SUCCESS)
			pr_debug("&&&&&& LINK UP FAIL !!&&&&&&\n");
		else
			pr_debug("&&&&&& LINK UP PASS !!&&&&&&\n");

	}

	if (!strncmp(buf, "linkup0", 7)) {
		int status = RET_SUCCESS;
		int cnt = 10;

		mu3d_hal_link_up(0);
		mdelay(500);
		do {
			if ((os_readl(U3D_LINK_STATE_MACHINE) & LTSSM) != STATE_U0_STATE) {
				status = RET_FAIL;
				break;
			}
			mdelay(50);
		} while (cnt--);

		if (status != RET_SUCCESS)
			os_printk(K_INFO, "&&&&&& LINK UP FAIL !!&&&&&&\n");
		else
			os_printk(K_INFO, "&&&&&& LINK UP PASS !!&&&&&&\n");
	}

	if (strncmp(buf, "scan", 4) == 0) {
		unsigned latch, driving;

		if (sscanf(buf, "scan l=%u d=%u", &latch, &driving) == 2) {
			os_printk(K_INFO, "%s latch=%d, driving=%d\n", __func__, latch, driving);
			musb_scan_phase(s, latch, driving);
		} else {
			os_printk(K_INFO, "%s Can not match\n", __func__);
		}
	}

	return count;
}

static const struct file_operations musb_scan_phase_fops = {
	.open = musb_scan_phase_open,
	.write = musb_scan_phase_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int musb_phy_reg_show(struct seq_file *s, void *unused)
{
	seq_printf(s,
		   "#echo w/w8/w32 [ADDR] [VAL] > phy_reg (ex: #echo w 0x2000e4 0x1 > phy_reg)\n");
	seq_puts(s, "#echo r/r8/r32 [ADDR] > phy_reg (ex: #echo r32 0x2000e4 > phy_reg)\n");

	return 0;
}

static int musb_phy_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, musb_phy_reg_show, inode->i_private);
}

static ssize_t musb_phy_rege_write(struct file *file,
				   const char __user *ubuf, size_t count, loff_t *ppos)
{
	/* warning: unused variable 's' [-Wunused-variable] */
	/* struct seq_file              *s = file->private_data; */
	char buf[18];

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
	if (!strncmp(buf, "w", 1)) {
		unsigned int address = 0;
		unsigned int value = 0;

		if (sscanf(buf, "w32 0x%x 0x%x", &address, &value) == 2) {
			value = value & 0xff;
			os_printk(K_INFO, "%s write32 address=0x%x, value=0x%x\n", __func__,
				  address, value);
			U3PhyWriteReg32(address, value);
			mdelay(10);
			value = U3PhyReadReg32(address);
			os_printk(K_INFO, "%s result=0x%x\n", __func__, value);
		} else if (sscanf(buf, "w8 0x%x 0x%x", &address, &value) == 2) {
			value = value & 0xff;
			os_printk(K_INFO, "%s write8 address=0x%x, value=0x%x\n", __func__, address,
				  value);
			U3PhyWriteReg8(address, value);
			mdelay(10);
			value = U3PhyReadReg8(address);
			os_printk(K_INFO, "%s result=0x%x\n", __func__, value);
		} else if (sscanf(buf, "w 0x%x 0x%x", &address, &value) == 2) {
			value = value & 0xff;
			pr_debug("%s write address=0x%x, value=0x%x\n", __func__, address, value);
			_U3Write_Reg(address, value);
			mdelay(10);
			value = _U3Read_Reg(address);
			os_printk(K_INFO, "%s result=0x%x\n", __func__, value);
		} else {
			os_printk(K_INFO, "%s Can not match\n", __func__);
		}
	}

	if (!strncmp(buf, "r", 1)) {
		unsigned int address = 0;
		unsigned int value = 0;

		if (sscanf(buf, "r32 0x%x", &address) == 1) {
			os_printk(K_INFO, "%s read32 address=0x%x\n", __func__, address);
			value = U3PhyReadReg32(address);
			os_printk(K_INFO, "%s result=0x%x\n", __func__, value);
		} else if (sscanf(buf, "r8 0x%x", &address) == 1) {
			os_printk(K_INFO, "%s read8 address=0x%x\n", __func__, address);
			value = U3PhyReadReg8(address);
			os_printk(K_INFO, "%s result=0x%x\n", __func__, value);
		} else if (sscanf(buf, "r 0x%x", &address) == 1) {
			os_printk(K_INFO, "%s read address=0x%x\n", __func__, address);
			value = _U3Read_Reg(address);
			os_printk(K_INFO, "%s result=0x%x\n", __func__, value);
		} else {
			os_printk(K_INFO, "%s Can not match\n", __func__);
		}
	}
#endif

	return count;
}


static const struct file_operations musb_phy_reg_fops = {
	.open = musb_phy_reg_open,
	.write = musb_phy_rege_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#ifdef CONFIG_U3_PHY_SMT_LOOP_BACK_SUPPORT
static int smt_err_count_get(void *data, u64 *val)
{
	*val = u3_loop_back_test();

	os_printk(K_DEBUG, "smt_err_count_get %llu\n", *val);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(smt_err_count, smt_err_count_get, NULL, "%llu\n");
#endif

int musb_init_debugfs(struct musb *musb)
{
	struct dentry *root;
	struct dentry *file;
	int ret;

	root = debugfs_create_dir(dev_name(musb->controller), NULL);
	if (!root) {
		ret = -ENOMEM;
		goto err0;
	}

	file = debugfs_create_file("regdump", S_IRUGO, root, musb, &musb_regdump_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}

	file = debugfs_create_file("testmode", S_IRUGO | S_IWUSR, root, musb, &musb_test_mode_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
#ifdef USE_SSUSB_QMU
	file = debugfs_create_file("usb_reg", S_IRUGO | S_IWUSR, root, musb, &musb_usb_reg_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
#endif
#ifdef CONFIG_U3_PHY_SMT_LOOP_BACK_SUPPORT
	file = debugfs_create_file("smt_err_count", S_IRUGO, root, musb,
					&smt_err_count);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
#endif
#ifdef CONFIG_FPGA_EARLY_PORTING
	file = debugfs_create_file("scan_phase", S_IRUGO | S_IWUSR,
				   root, musb, &musb_scan_phase_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}

	file = debugfs_create_file("phy_reg", S_IRUGO | S_IWUSR, root, musb, &musb_phy_reg_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
#endif

	musb->debugfs_root = root;

	return 0;

err1:
	debugfs_remove_recursive(root);

err0:
	return ret;
}

void /* __init_or_exit */ musb_exit_debugfs(struct musb *musb)
{
	debugfs_remove_recursive(musb->debugfs_root);
}
