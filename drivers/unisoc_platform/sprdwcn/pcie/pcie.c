/*
 * Copyright (C) 2016-2018 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>
#include <linux/msi.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/pcie-rc-sprd.h>
#include <linux/platform_device.h>
#include <misc/wcn_bus.h>

#include "edma_engine.h"
#include "ioctl.h"
#include "mchn.h"
#include "pcie.h"
#include "pcie_dbg.h"
#include "pcie_pm.h"
#include "wcn_boot.h"
#include "wcn_log.h"
#include "wcn_op.h"
#include "wcn_procfs.h"
#include "wcn_txrx.h"
#include "sprd_wcn.h"

#define WAIT_AT_DONE_MAX_CNT 5

#define WCN_PCIE_PHY_DEBUG_R0		0x728
#define LTSSM_STATE_MASK		0x3f
#define LTSSM_STATE_L0			0x11
#define LTSSM_STATE_L0S			0x12
#define LTSSM_STATE_L1_IDLE		0x14
#define LTSSM_STATE_L2_IDLE		0x15

#define N	16

static int (*scan_card_notify)(void);
static struct wcn_pcie_info *g_pcie_dev;

void wcn_dump_ep_mems(struct wcn_pcie_info *priv)
{
	u32 reg;
	u32 mem[N];
	int i;

	pci_read_config_dword(priv->dev, 00, &reg);
	WCN_INFO("EP [00] =0x%x\n", reg);
	pci_read_config_dword(priv->dev, 04, &reg);
	WCN_INFO("EP [04] =0x%x\n", reg);
	pci_read_config_dword(priv->dev, 0x10, &reg);
	WCN_INFO("EP [10] =0x%x\n", reg);
	pci_read_config_dword(priv->dev, 0x18, &reg);
	WCN_INFO("EP [18] =0x%x\n", reg);
	pci_read_config_dword(priv->dev, 0x20, &reg);
	WCN_INFO("EP [20] =0x%x\n", reg);
	pci_read_config_dword(priv->dev, 0x24, &reg);
	WCN_INFO("EP [24] =0x%x\n", reg);
	sprd_pcie_mem_read(0x40500000, mem, N * 4);
	for (i = 0; i < N; i++)
		WCN_INFO("mem[%d]= 0x%x\n", i, mem[i]);

	sprd_pcie_mem_read(0x40130000, mem, N * 4);
	for (i = 0; i < N; i++)
		WCN_INFO("4013[%d]= 0x%x\n", i, mem[i]);

	sprd_pcie_mem_read(0x40160000, mem, N * 4);
	for (i = 0; i < N; i++)
		WCN_INFO("EDMA_GLB[%d]= 0x%x\n", i, mem[i]);
}

void wcn_dump_ep_breg(struct wcn_pcie_info *priv, u32 breg_offset_addr)
{
	struct inbound_reg *breg;

	if (!pcie_bar_vmem(priv, 4)) {
		WCN_INFO("get bar4 base err\n");
		return;
	}
	breg = (struct inbound_reg *) (pcie_bar_vmem(priv, 4) +
		breg_offset_addr);
	WCN_INFO("type = 0x%x\n", breg->type);
	WCN_INFO("en = 0x%x\n", breg->en);
	WCN_INFO("lower_base_addr = 0x%x\n", breg->lower_base_addr);
	WCN_INFO("upper_base_addr = 0x%x\n", breg->upper_base_addr);
	WCN_INFO("limit = 0x%x\n", breg->limit);
	WCN_INFO("lower_target_addr = 0x%x\n", breg->lower_target_addr);
	WCN_INFO("upper_target_addr = 0x%x\n", breg->upper_target_addr);
}

void wcn_dump_ep_regs(struct wcn_pcie_info *priv)
{
	u32 reg;

	wcn_dump_ep_mems(priv);
	pci_read_config_dword(priv->dev, WCN_PCIE_PHY_DEBUG_R0, &reg);
	WCN_INFO("EP config reg [0x728]: 0x%x\n", reg);
	pci_read_config_dword(priv->dev, PCI_VENDOR_ID, &reg);
	WCN_INFO("EP config reg [0]: 0x%x\n", reg);
	WCN_INFO("------------[ EP ibreg0 ]------------\n");
	wcn_dump_ep_breg(priv, IBREG0_OFFSET_ADDR);
	WCN_INFO("------------[ EP ibreg1 ]------------\n");
	wcn_dump_ep_breg(priv, IBREG1_OFFSET_ADDR);
	WCN_INFO("EP memory reg [0x0]: 0x%x, [0x4]: 0x%x\n",
		sprd_pcie_read_reg32(priv, WCN_PCIE_DEV_AND_VND_ID),
		sprd_pcie_read_reg32(priv, WCN_PCIE_CMD));
}

struct wcn_pcie_info *get_wcn_device_info(void)
{
	return g_pcie_dev;
}

int wcn_get_edma_status(void)
{
	struct wcn_pcie_info *priv = get_wcn_device_info();

	return atomic_read(&priv->edma_ready);
}

int wcn_get_tx_complete_status(void)
{
	struct wcn_pcie_info *priv = get_wcn_device_info();

	return atomic_read(&priv->tx_complete);
}

void wcn_set_tx_complete_status(int flag)
{
	struct wcn_pcie_info *priv = get_wcn_device_info();

	atomic_set(&priv->tx_complete, flag);
}

static void wcn_bus_change_state(struct wcn_pcie_info *bus,
				 enum wcn_bus_state state)
{
	bus->pci_status = state;
}

static irqreturn_t sprd_pcie_msi_irq(int irq, void *arg)
{
	struct wcn_pcie_info *priv = arg;

	/*
	 * priv->irq : the first msi irq
	 * irq: the current irq
	 */
	irq -= priv->irq;
	msi_irq_handle(irq);

	return IRQ_HANDLED;
}

static irqreturn_t sprd_pcie_legacy_irq(int irq, void *arg)
{
	legacy_irq_handle(irq);

	return IRQ_HANDLED;
}

int pcie_bar_write(struct wcn_pcie_info *priv, int bar, int offset,
		   void *buf, int len)
{
	char *mem = priv->bar[bar].vmem;

	if (!buf) {
		WCN_INFO("%s: buff is NULL, return\n", __func__);
		return -1;
	}
	mem += offset;

	WCN_DBG("%s(%d, 0x%x, 0x%x)\n", __func__, bar, offset, *((int *)buf));
	if (len == 1)
		writeb(*((unsigned char *)buf), mem);
	else if (len == 2)
		writew(*((unsigned short *)buf), mem);
	else if (len == 4)
		writel(*((unsigned int *)buf), mem);
	else
		memcpy_toio(mem, buf, len);

	return 0;
}
EXPORT_SYMBOL(pcie_bar_write);

int pcie_bar_read(struct wcn_pcie_info *priv, int bar, int offset,
		  void *buf, int len)
{
	char *mem = priv->bar[bar].vmem;

	mem += offset;

	if (len == 1)
		*((unsigned char *)buf) = readb(mem);
	else if (len == 2)
		*((unsigned short *)buf) = readw(mem);
	else if (len == 4)
		*((unsigned int *)buf) = readl(mem);
	else
		memcpy_fromio(buf, mem, len);

	return 0;
}
EXPORT_SYMBOL(pcie_bar_read);

char *pcie_bar_vmem(struct wcn_pcie_info *priv, int bar)
{
	if (!priv) {
		WCN_ERR("sprd pcie_dev NULL\n");
		return NULL;
	}

	return priv->bar[bar].vmem;
}

int dmalloc(struct wcn_pcie_info *priv, struct dma_buf *dm, int size)
{
	struct device *dev = &(priv->dev->dev);

	if (!dev) {
		WCN_ERR("%s(NULL)\n", __func__);
		return ERROR;
	}

	if (dma_set_mask(dev, DMA_BIT_MASK(64))) {
		WCN_INFO("dma_set_mask err\n");
		if (dma_set_coherent_mask(dev, DMA_BIT_MASK(64))) {
			WCN_ERR("dma_set_coherent_mask err\n");
			return ERROR;
		}
	}

	dm->vir =
	    (unsigned long)dma_alloc_coherent(dev, size,
					      (dma_addr_t *)(&(dm->phy)),
					      GFP_DMA);
	if (dm->vir == 0) {
		WCN_ERR("dma_alloc_coherent err\n");
		return ERROR;
	}
	dm->size = size;
	memset((unsigned char *)(dm->vir), 0x56, size);
	WCN_DBG("dma_alloc_coherent(0x%x) vir=0x%lx, phy=0x%lx\n",
		 size, dm->vir, dm->phy);

	return 0;
}
EXPORT_SYMBOL(dmalloc);

int dmfree(struct wcn_pcie_info *priv, struct dma_buf *dm)
{
	struct device *dev = &(priv->dev->dev);

	if (!dev) {
		WCN_ERR("%s(NULL)\n", __func__);
		return ERROR;
	}
	WCN_DBG("dma_free_coherent(0x%x,0x%lx,0x%lx)\n",
		 dm->size, dm->vir, dm->phy);
	dma_free_coherent(dev, dm->size, (void *)(dm->vir), dm->phy);
	memset(dm, 0x00, sizeof(struct dma_buf));

	return 0;
}

unsigned char *ibreg_base(struct wcn_pcie_info *priv, char region)
{
	unsigned char *p = pcie_bar_vmem(priv, 4);

	if (!p)
		return NULL;
	if (region > 8)
		return NULL;
	WCN_DBG("%s(%d):0x%x\n", __func__, region, (0x10100 | (region << 9)));
	/*
	 * 0x10000: iATU relative offset to BAR4.
	 * BAR4 included map iatu reg information.
	 * i= region
	 * Base = 0x10000
	 * outbound = Base + i * 0x200
	 * inbound = Base + i * 0x200 + 0x100
	 */
	p = p + (0x10100 | (region << 9));
	WCN_DBG("base =0x%p\n", p);

	return p;
}

unsigned char *obreg_base(struct wcn_pcie_info *priv, char region)
{
	unsigned char *p = pcie_bar_vmem(priv, 4);

	if (!p)
		return NULL;
	if (region > 8)
		return NULL;
	WCN_DBG("%s(%d):0x%x\n", __func__, region, (0x10000 | (region << 9)));
	p = p + (0x10000 | (region << 9));

	return p;
}

static int sprd_ep_addr_map(struct wcn_pcie_info *priv)
{
	struct inbound_reg *ibreg0;
	struct outbound_reg *obreg0;
	struct outbound_reg *obreg1;
	u32 retries, val;

	if (!pcie_bar_vmem(priv, 4)) {
		WCN_INFO("get bar4 base err\n");
		return -1;
	}

	ibreg0 = (struct inbound_reg *) (pcie_bar_vmem(priv, 4) +
							IBREG0_OFFSET_ADDR);
	obreg0 = (struct outbound_reg *) (pcie_bar_vmem(priv, 4) +
							OBREG0_OFFSET_ADDR);
	obreg1 = (struct outbound_reg *) (pcie_bar_vmem(priv, 4) +
							OBREG1_OFFSET_ADDR);
	/*
	if (g_match_config && g_match_config->unisoc_wcn_m3e)
		writel(EP_IBAR0_BASE, &ibreg0->lower_target_addr);
	else
		writel(EP_IBAR0_BASE, &ibreg0->lower_target_addr);
	*/
	writel(EP_IBAR0_BASE, &ibreg0->lower_target_addr);

	writel(0x00000000, &ibreg0->upper_target_addr);
	writel(0x00000000, &ibreg0->type);
	writel(0x00FFFFFF, &ibreg0->limit);
	writel(REGION_EN | BAR_MATCH_MODE, &ibreg0->en);

	/*
	 * Make sure ATU enable takes effect before any subsequent config
	 * and I/O accesses.
	 */
	for (retries = 0; retries < LINK_WAIT_MAX_IATU_RETRIES; retries++) {
		val = readl((void *)(&ibreg0->en));
		if (val & PCIE_ATU_ENABLE)
			break;
		WCN_INFO("%s:ibreg0 retries=%d\n", __func__, retries);
		mdelay(LINK_WAIT_IATU);
	}

	writel(0x00000000, &obreg0->type);
	writel(0x00000000, &obreg0->lower_base_addr);
	writel(0x00000080, &obreg0->upper_base_addr);
	writel(0xffffffff, &obreg0->limit);
	writel(0x00000000, &obreg0->lower_target_addr);
	writel(0x00000000, &obreg0->upper_target_addr);
	writel(REGION_EN & ADDR_MATCH_MODE, &obreg0->en);

	/*
	 * Make sure ATU enable takes effect before any subsequent config
	 * and I/O accesses.
	 */
	for (retries = 0; retries < LINK_WAIT_MAX_IATU_RETRIES; retries++) {
		val = readl((void *)(&obreg0->en));
		if (val & PCIE_ATU_ENABLE)
			break;
		WCN_INFO("%s:obreg0 retries=%d\n", __func__, retries);
		mdelay(LINK_WAIT_IATU);
	}

	writel(0x00000000, &obreg1->type);
	writel(0x00000000, &obreg1->lower_base_addr);
	writel(0x00000081, &obreg1->upper_base_addr);
	writel(0xffffffff, &obreg1->limit);
	writel(0x00000000, &obreg1->lower_target_addr);
	writel(0x00000001, &obreg1->upper_target_addr);
	writel(REGION_EN & ADDR_MATCH_MODE, &obreg1->en);

	/*
	 * Make sure ATU enable takes effect before any subsequent config
	 * and I/O accesses.
	 */
	for (retries = 0; retries < LINK_WAIT_MAX_IATU_RETRIES; retries++) {
		val = readl((void *)(&obreg1->en));
		if (val & PCIE_ATU_ENABLE)
			break;
		WCN_INFO("%s:obreg1 retries=%d\n", __func__, retries);
		mdelay(LINK_WAIT_IATU);
	}

	return 0;
}

int pcie_config_read(struct wcn_pcie_info *priv, int offset, char *buf, int len)
{
	int i, ret;

	for (i = 0; i < len; i++) {
		ret = pci_read_config_byte(priv->dev, i, &(buf[i]));
		if (ret) {
			WCN_ERR("pci_read_config_dword %d err\n", ret);
			return ERROR;
		}
	}
	return 0;
}

int pcie_config_write(struct wcn_pcie_info *priv, int offset,
		      char *buf, int len)
{
	int i, ret;

	for (i = 0; i < len; i++) {
		ret = pci_write_config_byte(priv->dev, i, buf[i]);
		if (ret) {
			WCN_ERR("%s %d err\n", __func__, ret);
			return ERROR;
		}

	}
	return 0;
}

int sprd_pcie_bar_map(struct wcn_pcie_info *priv, int bar,
		      unsigned int addr, char region)
{
	u32 retries, val;
	struct inbound_reg *ibreg = (struct inbound_reg *) ibreg_base(priv,
								      region);

	if (!ibreg) {
		WCN_ERR("ibreg(%d) NULL\n", region);
		return -1;
	}

	writel(addr, &ibreg->lower_target_addr);
	writel(0x00000000, &ibreg->upper_target_addr);
	writel(0x00000000, &ibreg->type);
	writel(0x00FFFFFF, &ibreg->limit);
	writel(REGION_EN | BAR_MATCH_MODE | (bar << 8), &ibreg->en);

	WCN_DBG("%s(bar=%d, addr=0x%x, region=%d)\n",
		__func__, bar, addr, region);

	/*
	 * Make sure ATU enable takes effect before any subsequent config
	 * and I/O accesses.
	 */
	for (retries = 0; retries < LINK_WAIT_MAX_IATU_RETRIES; retries++) {
		val = ibreg->en;
		if (val & PCIE_ATU_ENABLE)
			return 0;
		WCN_INFO("%s:retries=%d\n", __func__, retries);
		mdelay(LINK_WAIT_IATU);
	}
	WCN_ERR("inbound iATU is not being enabled\n");

	return -EBUSY;
}
EXPORT_SYMBOL(sprd_pcie_bar_map);

/*
 * set 1:   reg + 0x1000-->write 1 to set ref bit
 * clear 0: reg +0x2000-->write 1 to clear ref bit
 * for example:
 * bit2 = 1, bit3 = 1. mask = BIT(2)|BIT(3) , value = BIT(2)|BIT(3)
 * bit2 = 0, bit3 = 0. mask = BIT(2)|BIT(3) , value = ~(BIT(2)|BIT(3))
 */
int sprd_pcie_update_bits(unsigned int reg, unsigned int mask, unsigned int val)
{
	int ret;
	unsigned int base_addr;
	unsigned int offset;
	unsigned int base_upper_addr;
	int bar;
	char region;
	unsigned int set, clr;
	struct wcn_match_data *g_match_config = get_wcn_match_config();
	unsigned int ep_inbound_align;

	if (g_match_config && g_match_config->unisoc_wcn_m3e)
		ep_inbound_align = EP_INBOUND_ALIGN_M3E;
	else
		ep_inbound_align = EP_INBOUND_ALIGN;

	base_addr = reg / ep_inbound_align * ep_inbound_align;
	offset = reg % ep_inbound_align;
	base_upper_addr = ((reg + 4)/ep_inbound_align * ep_inbound_align);
	bar = 0;
	region = 0;

	if (base_addr != base_upper_addr)
		WARN_ON(1);
	WCN_INFO("%s: bar=%d, base=0x%x, offset=0x%x, upper=0x%x\n",
		 __func__, bar, base_addr, offset, base_upper_addr);

	ret = sprd_pcie_bar_map(g_pcie_dev, bar, base_addr, region);
	if (ret < 0)
		return ret;

	set = val & mask;
	clr = ~set & mask;

	if (set) {
		ret = pcie_bar_write(g_pcie_dev, bar, offset + 0x1000, &val, 4);
		if (ret < 0)
			return ret;

	}

	if (clr) {
		ret = pcie_bar_write(g_pcie_dev, bar, offset + 0x2000, &val, 4);
		if (ret < 0)
			return ret;
	}

	return ret;
}

int sprd_pcie_mem_write(unsigned int addr, void *buf, unsigned int len)
{
	int ret = 0;
	unsigned int base_addr;
	unsigned int offset;
	unsigned int base_upper_addr;
	int bar;
	char region;
	struct wcn_match_data *g_match_config = get_wcn_match_config();
	unsigned int ep_inbound_align;

	if (g_match_config && g_match_config->unisoc_wcn_m3e)
		ep_inbound_align = EP_INBOUND_ALIGN_M3E;
	else
		ep_inbound_align = EP_INBOUND_ALIGN;

	base_addr = addr / ep_inbound_align * ep_inbound_align;
	offset = addr % ep_inbound_align;
	base_upper_addr = ((addr + len)/ep_inbound_align * ep_inbound_align);
	bar = 2;
	region = 1;

	if (base_addr != base_upper_addr)
		WARN_ON(1);
	WCN_DBG("%s: bar=%d, base=0x%x, offset=0x%x, upper=0x%x, len=0x%x\n",
		 __func__, bar, base_addr, offset, base_upper_addr, len);

	ret = sprd_pcie_bar_map(g_pcie_dev, bar, base_addr, region);
	if (ret < 0)
		return ret;

	ret = pcie_bar_write(g_pcie_dev, bar, offset, buf, len);
	if (ret < 0)
		return ret;

	return ret;
}

int sprd_pcie_mem_read(unsigned int addr, void *buf, unsigned int len)
{
	int ret = 0;
	unsigned int base_addr;
	unsigned int offset;
	unsigned int base_upper_addr;
	int bar;
	char region;
	struct wcn_match_data *g_match_config = get_wcn_match_config();
	unsigned int ep_inbound_align;

	if (g_match_config && g_match_config->unisoc_wcn_m3e)
		ep_inbound_align = EP_INBOUND_ALIGN_M3E;
	else
		ep_inbound_align = EP_INBOUND_ALIGN;

	base_addr = addr / ep_inbound_align * ep_inbound_align;
	offset = addr % ep_inbound_align;
	base_upper_addr = ((addr + len)/ep_inbound_align * ep_inbound_align);
	bar = 2;
	region = 1;

	if (base_addr != base_upper_addr)
		WARN_ON(1);
	WCN_DBG("%s: bar=%d, base=0x%x, offset=0x%x, upper=0x%x, len=0x%x\n",
		 __func__, bar, base_addr, offset, base_upper_addr, len);

	ret = sprd_pcie_bar_map(g_pcie_dev, bar, base_addr, region);
	if (ret < 0)
		return ret;

	ret = pcie_bar_read(g_pcie_dev, bar, offset, buf, len);
	if (ret < 0)
		return ret;

	return ret;
}

/* only for 4000 0000 ~ 4040 0000 */
u32 sprd_pcie_read_reg32(struct wcn_pcie_info *priv, int offset)
{
	char *addr = priv->bar[0].vmem;

	addr += offset;
	return readl(addr);
}

void sprd_pcie_write_reg32(struct wcn_pcie_info *priv, u32 reg_offset,
			    u32 value)
{
	char *address = priv->bar[0].vmem;

	address += reg_offset;
	writel(value, address);
}

int wcn_pcie_get_bus_status(void)
{
	if (!g_pcie_dev)
		return WCN_BUS_DOWN;

	return g_pcie_dev->pci_status;
}

enum wcn_bus_pm_state sprd_pcie_get_aspm_policy(void)
{
	return 0;
}

int sprd_pcie_set_aspm_policy(enum sub_sys subsys, enum wcn_bus_pm_state state)
{
	return 0;
}

void sprd_pcie_set_carddump_status(unsigned int flag)
{
	struct wcn_pcie_info *priv = get_wcn_device_info();

	WCN_INFO("carddump flag set[%d]\n", flag);
	priv->card_dump_flag = flag;
}

extern char wcn_assert_str[128];
unsigned int sprd_pcie_get_carddump_status(void)
{
	struct wcn_pcie_info *priv = get_wcn_device_info();

	if (priv->card_dump_flag != 0)
		WCN_ERR("fw assert:%s\n", wcn_assert_str);

	return priv->card_dump_flag;
}

static struct platform_device *to_pdev_from_ep_node(struct device_node *ep_node)
{
	struct device_node *pdev_node;

	WCN_INFO("%s\n", __func__);
	pdev_node = of_parse_phandle(ep_node, "sprd,rc-ctrl", 0);
	if (!pdev_node) {
		WCN_ERR("%s: pcie ep lacks property sprd,rc-ctrl\n", __func__);
		return NULL;
	}

	return of_find_device_by_node(pdev_node);
}

/* called by chip_power_on */
int sprd_pcie_scan_card(void *wcn_dev)
{
	struct wcn_pcie_info *priv = get_wcn_device_info();
	struct platform_device *pdev;
	struct device *dev;
	struct marlin_device *marlin_dev = wcn_dev;

	init_completion(&priv->scan_done);
	WCN_INFO("device node name: %s\n", marlin_dev->np->name);
	pdev = to_pdev_from_ep_node(marlin_dev->np);
	if (!pdev) {
		WCN_ERR("can't get pcie rc node\n");
		return 0;
	}
	dev = &pdev->dev;
	WCN_INFO("%s: rc node name: %s\n", __func__, dev->of_node->name);

	if (priv->dev)
		WCN_ERR("%s: card not NULL\n", __func__);

	sprd_pcie_configure_device(pdev);

	if (wait_for_completion_timeout(&priv->scan_done,
	    msecs_to_jiffies(5000)) == 0) {
		WCN_ERR("wait scan card time out\n");
		return -ENODEV;
	}
	WCN_INFO("scan end\n");

	return 0;
}

void sprd_pcie_register_scan_notify(void *func)
{
	scan_card_notify = func;
}

static int disable_pcie_irq(void)
{
	struct wcn_pcie_info *priv = get_wcn_device_info();
	int i;

	if (priv->msix_en == 1) {
		for (i = 0; i < priv->irq_num; i++) {
			disable_irq(priv->msix[i].vector);
			WCN_INFO("%s disable_irq(%d) ok\n", __func__,
				 priv->msix[i].vector);
		}
	}

	if (priv->msi_en == 1) {
		for (i = 0; i < priv->irq_num; i++) {
			if (!free_irq(priv->irq + i, (void *)priv))
				return -1;
		}

		pci_disable_msi(priv->dev);
	}

	return 0;
}

/*
 * called by chip_power_off
 * 1. tx: disable send
 * 2. tx: wait tx complete
 * 3.  delete expire time
 * 4. disable mis irq
 * 5. edma tasklet kill
 * 6. free some resource(desc, mem, buf,etc)(for free DMA buf depend on dev)
 * 7. remove pcie ep
 * 8. power down
 */
void sprd_pcie_remove_card(void *wcn_dev)
{
	struct wcn_pcie_info *priv = get_wcn_device_info();
	struct platform_device *pdev;
	struct device *dev;
	struct marlin_device *marlin_dev = wcn_dev;
	int wait_cnt = 0;

	/* prevent at+loopcheck send */
	atomic_add(BUS_REMOVE_CARD_VAL, &priv->xmit_cnt);
	/* prevent tx send */
	atomic_set(&priv->edma_ready, 0x0);
	/* if tx have send, waiting complete */

	while (!atomic_read(&priv->tx_complete) &&
	       (wait_cnt < WAIT_AT_DONE_MAX_CNT)) {
		usleep_range_state(100, 200, TASK_UNINTERRUPTIBLE);
		wait_cnt++;
		WCN_INFO("%s:wait cnt =%d\n", __func__, wait_cnt);
	}

	edma_del_tx_timer();

	/* rx: disable txrx irq */
	if (disable_pcie_irq() < 0) {
		WCN_ERR(" irq have free\n");
		return;
	}

	/* rx: kill tasklet */
	edma_tasklet_deinit();

	wcn_bus_change_state(priv, WCN_BUS_DOWN);

	if (edma_hw_pause() < 0)
		WCN_ERR("edma_hw_pause fail\n");
	init_completion(&priv->remove_done);
	/* for proc_fs_exit, loopcheck/at/assert */
	mdbg_fs_channel_destroy();
	/* for log_dev_exit */
	mdbg_pt_ring_unreg();
	ioctlcmd_deinit(priv);
	edma_deinit();
	mutex_destroy(&priv->pm_lock);
	pdev = to_pdev_from_ep_node(marlin_dev->np);
	if (!pdev) {
		WCN_ERR("can't get pcie rc node\n");
		return;
	}
	dev = &pdev->dev;
	WCN_INFO("%s: rc node name: %s\n",
			__func__, dev->of_node->name);

	if (!priv->dev || priv->dev)
		WCN_ERR("%s: card exist!\n", __func__);

	sprd_pcie_unconfigure_device(pdev);

	if (wait_for_completion_timeout(&priv->remove_done,
					msecs_to_jiffies(5000)) == 0)
		WCN_ERR("remove card time out\n");
	else
		WCN_INFO("remove card end\n");
}

extern void marlin_scan_finish(void);

static int sprd_pcie_probe(struct pci_dev *pdev,
			   const struct pci_device_id *pci_id)
{
	struct wcn_pcie_info *priv;
	unsigned int val32;

	int ret = -ENODEV, i, flag;

	WCN_INFO("%s Enter\n", __func__);

	priv = get_wcn_device_info();
	if (priv == NULL) {
		WCN_ERR("priv is NULL\n");
		goto err_out;
	}
	priv->dev = pdev;
	pci_set_drvdata(pdev, priv);

	/* enable device */
	if (pci_enable_device(pdev)) {
		WCN_ERR("cannot enable device:%s\n", pci_name(pdev));
		goto err_out;
	}

	/* enable bus master capability on device */
	pci_set_master(pdev);

	priv->irq = pdev->irq;
	WCN_INFO("dev->irq %d\n", pdev->irq);

	priv->legacy_en = 0;
	priv->msi_en = 1;
	priv->msix_en = 0;

	if (priv->legacy_en == 1)
		priv->irq = pdev->irq;

	if (priv->msi_en == 1) {
		priv->irq_num = pci_msi_vec_count(pdev);
		WCN_INFO("pci_msix_vec_count ret %d\n", priv->irq_num);

		ret = pci_alloc_irq_vectors(pdev, 1, priv->irq_num,
					    PCI_IRQ_MSI);
		if (ret > 0) {
			WCN_INFO("pci_enable_msi_range %d ok\n", ret);
			priv->msi_en = 1;
		} else {
			WCN_INFO("pci_enable_msi_range err=%d\n", ret);
			priv->msi_en = 0;
		}
		priv->irq = pdev->irq;
	}

	if (priv->msix_en == 1) {
		for (i = 0; i < 65; i++) {
			priv->msix[i].entry = i;
			priv->msix[i].vector = 0;
		}
		priv->irq_num = pci_enable_msix_range(pdev, priv->msix, 1, 64);
		if (priv->irq_num > 0) {
			WCN_INFO("pci_enable_msix_range %d ok\n",
				 priv->irq_num);
			priv->msix_en = 1;
		} else {
			WCN_INFO("pci_enable_msix_range %d err\n",
				 priv->irq_num);
			priv->msix_en = 0;
		}
		priv->irq = pdev->irq;
	}
	WCN_INFO("dev->irq %d\n", pdev->irq);
	WCN_INFO("legacy %d msi_en %d, msix_en %d\n", priv->legacy_en,
		 priv->msi_en, priv->msix_en);
	for (i = 0; i < 8; i++) {
		flag = pci_resource_flags(pdev, i);
		if (!(flag & IORESOURCE_MEM))
			continue;

		priv->bar[i].mmio_start = pci_resource_start(pdev, i);
		priv->bar[i].mmio_end = pci_resource_end(pdev, i);
		priv->bar[i].mmio_flags = pci_resource_flags(pdev, i);
		priv->bar[i].mmio_len = pci_resource_len(pdev, i);
		priv->bar[i].mem =
		    ioremap(priv->bar[i].mmio_start, priv->bar[i].mmio_len);
		priv->bar[i].vmem = priv->bar[i].mem;
		if (priv->bar[i].vmem == NULL) {
			WCN_ERR("%s:cannot remap mmio, aborting\n",
				pci_name(pdev));
			ret = -EIO;
			goto err_out;
		}
		WCN_INFO("BAR(%d) (0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx)\n", i,
			 (unsigned long)priv->bar[i].mmio_start,
			 (unsigned long)priv->bar[i].mmio_end,
			 priv->bar[i].mmio_flags,
			 (unsigned long)priv->bar[i].mmio_len,
			 (unsigned long)priv->bar[i].vmem);
	}
	priv->bar_num = 8;
	ret = pci_request_regions(pdev, DRVER_NAME);
	if (ret) {
		priv->in_use = 1;
		goto err_out;
	}

	if (priv->legacy_en == 1) {
		ret = request_irq(priv->irq,
				 (irq_handler_t) (&sprd_pcie_legacy_irq),
				 IRQF_NO_SUSPEND | IRQF_NO_THREAD | IRQF_PERCPU,
				 DRVER_NAME, (void *)priv);
		if (ret)
			WCN_ERR("%s request_irq(%d), error %d\n", __func__,
				priv->irq, ret);
		else
			WCN_INFO("%s request_irq(%d) ok\n", __func__,
				 priv->irq);
	}
	if (priv->msi_en == 1) {
		for (i = 0; i < priv->irq_num; i++) {
			ret =
			    request_irq(priv->irq + i,
					(irq_handler_t) (&sprd_pcie_msi_irq),
					IRQF_SHARED, DRVER_NAME, (void *)priv);
			if (ret) {
				WCN_ERR("%s request_irq(%d), error %d\n",
					__func__, priv->irq + i, ret);
				break;
			}
			WCN_INFO("%s request_irq(%d) ok\n", __func__,
				 priv->irq + i);
		}
		if (i == priv->irq_num)
			priv->irq_en = 1;
	}
	if (priv->msix_en == 1) {
		for (i = 0; i < priv->irq_num; i++) {
			ret =
			    request_irq(priv->msix[i].vector,
					(irq_handler_t) (&sprd_pcie_msi_irq),
					IRQF_SHARED, DRVER_NAME, (void *)priv);
			if (ret) {
				WCN_ERR("%s request_irq(%d), error %d\n",
					__func__, priv->msix[i].vector, ret);
				break;
			}

			WCN_INFO("%s request_irq(%d) ok\n", __func__,
				 priv->msix[i].vector);
		}
	}
	device_wakeup_enable(&(pdev->dev));
	ret = sprd_ep_addr_map(priv);
	if (ret < 0)
		return ret;

	wcn_bus_change_state(priv, WCN_BUS_UP);
	atomic_set(&priv->xmit_cnt, 0x0);
	atomic_set(&priv->is_suspending, 0);
	complete(&priv->scan_done);

	edma_init(priv);
	atomic_set(&priv->edma_ready, 0x1);
	mutex_init(&priv->pm_lock);

	dbg_attach_bus(priv);

	/* for proc_fs_init */
	mdbg_fs_channel_init();
	/* for log_dev_init */
	mdbg_pt_ring_reg();
	sprd_pcie_set_aspm_policy(AUTO, BUS_PM_ALL_ENABLE);
	pci_read_config_dword(pdev, 0x0728, &val32);
	WCN_INFO("EP link status 728=0x%x\n", val32);
	pci_read_config_dword(pdev, 0x072c, &val32);
	WCN_INFO("EP link status 72c=0x%x\n", val32);
	/* calling rescan callback to inform download */
	//if (scan_card_notify != NULL)
	//	scan_card_notify();
	marlin_scan_finish();
	WCN_INFO("%s ok\n", __func__);
	return 0;

err_out:
	return ret;
}

static void sprd_pcie_remove(struct pci_dev *pdev)
{
	int i;
	struct wcn_pcie_info *priv;

	WCN_INFO("%s\n", __func__);
	priv = (struct wcn_pcie_info *) pci_get_drvdata(pdev);

	if (priv->legacy_en == 1)
		free_irq(priv->irq, (void *)priv);

	if (priv->msix_en == 1) {
		WCN_INFO("disable MSI-X");
		for (i = 0; i < priv->irq_num; i++)
			free_irq(priv->msix[i].vector, (void *)priv);

		pci_disable_msix(pdev);
	}
	for (i = 0; i < priv->bar_num; i++) {
		if (priv->bar[i].mem)
			iounmap(priv->bar[i].mem);
	}
	complete(&priv->remove_done);
	pci_release_regions(pdev);
	//kfree(priv);
	pci_set_drvdata(pdev, NULL);
	pci_disable_device(pdev);

	WCN_INFO("%s end\n", __func__);
}

static int sprd_ep_suspend(struct device *dev)
{
	int ret;
	struct mchn_ops_t *ops;
	int chn;
	struct pci_dev *pdev = to_pci_dev(dev);
	struct wcn_pcie_info *priv = pci_get_drvdata(pdev);

	wcn_bus_change_state(priv, WCN_BUS_DOWN);
	atomic_set(&priv->is_suspending, 1);

	for (chn = 0; chn < 16; chn++) {
		ops = mchn_ops(chn);
		if ((ops != NULL) && (ops->power_notify != NULL)) {
			ret = ops->power_notify(chn, 0);
			if (ret != 0) {
				WCN_INFO("[%s] chn:%d suspend fail\n",
					 __func__, chn);
				atomic_set(&priv->is_suspending, 0);
				wcn_bus_change_state(priv, WCN_BUS_UP);
				return ret;
			}
		}
	}

	if (edma_hw_pause() < 0) {
		atomic_set(&priv->is_suspending, 0);
		return -1;
	}

	WCN_INFO("%s[+]\n", __func__);

	if (!pdev)
		return 0;

	pci_save_state(to_pci_dev(dev));
	priv->saved_state = pci_store_saved_state(to_pci_dev(dev));

	ret = pci_enable_wake(pdev, PCI_D3hot, 1);
	WCN_INFO("pci_enable_wake(PCI_D3hot) ret %d\n", ret);
	ret = pci_set_power_state(pdev, PCI_D3hot);
	WCN_INFO("pci_set_power_state(PCI_D3hot) ret %d\n", ret);
	WCN_INFO("%s[-]\n", __func__);

	return 0;
}

static int sprd_ep_resume(struct device *dev)
{
	int ret;
	struct mchn_ops_t *ops;
	int chn;
	struct pci_dev *pdev = to_pci_dev(dev);
	struct wcn_pcie_info *priv = pci_get_drvdata(pdev);

	WCN_INFO("%s[+]\n", __func__);
	if (!pdev)
		return 0;

	pci_load_and_free_saved_state(to_pci_dev(dev), &priv->saved_state);
	pci_restore_state(to_pci_dev(dev));
	pci_write_config_dword(to_pci_dev(dev), 0x60, 0);

	ret = pci_set_power_state(pdev, PCI_D0);
	WCN_INFO("pci_set_power_state(PCI_D0) ret %d\n", ret);
	ret = pci_enable_wake(pdev, PCI_D0, 0);
	WCN_INFO("pci_enable_wake(PCI_D0) ret %d\n", ret);

	ret = sprd_ep_addr_map(priv);
	if (ret)
		return ret;

	edma_hw_restore();

	wcn_bus_change_state(priv, WCN_BUS_UP);
	atomic_set(&priv->is_suspending, 0);
	for (chn = 0; chn < 16; chn++) {
		ops = mchn_ops(chn);
		if ((ops != NULL) && (ops->power_notify != NULL)) {
			ret = ops->power_notify(chn, 1);
			if (ret != 0) {
				WCN_INFO("[%s] chn:%d resume fail\n",
					 __func__, chn);
				wcn_bus_change_state(priv, WCN_BUS_DOWN);
				return ret;
			}
		}
	}
	WCN_INFO("%s[-]\n", __func__);
	return 0;
}

const struct dev_pm_ops sprd_ep_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sprd_ep_suspend, sprd_ep_resume)
};

static struct pci_device_id sprd_pcie_tbl[] = {
	{0x1db3, 0x2355, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0,}
};

MODULE_DEVICE_TABLE(pci, sprd_pcie_tbl);
static struct pci_driver sprd_pcie_driver = {
	.name = DRVER_NAME,
	.id_table = sprd_pcie_tbl,
	.probe = sprd_pcie_probe,
	.remove = sprd_pcie_remove,
	.driver = {
		.pm = &sprd_ep_pm_ops,
	},
};

//static int __init sprd_pcie_init(void)
int sprd_pcie_init(void)
{
	int ret = 0;
	struct wcn_pcie_info *priv;

	WCN_INFO("%s init\n", __func__);
	priv = kzalloc(sizeof(struct wcn_pcie_info), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	g_pcie_dev = priv;

	ret = pci_register_driver(&sprd_pcie_driver);
	WCN_INFO("pci_register_driver ret %d\n", ret);

	return ret;
}

//static void __exit sprd_pcie_exit(void)
void sprd_pcie_exit(void)
{
	struct wcn_pcie_info *priv = get_wcn_device_info();

	WCN_INFO("%s\n", __func__);
	pci_unregister_driver(&sprd_pcie_driver);
	kfree(priv);
}

//module_init(sprd_pcie_init);
//module_exit(sprd_pcie_exit);
//
//MODULE_LICENSE("GPL");
//MODULE_DESCRIPTION("marlin3 pcie/edma drv");
