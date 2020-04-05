/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2017 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#include <linux/interrupt.h>
#include <linux/pm_runtime.h>

#include "atl_common.h"
#include "atl_hw.h"
#include "atl_ring.h"
#include "atl2_fw.h"

static void atl_unplugged(struct atl_hw *hw)
{
	if (!hw->regs)
		return;
	hw->regs = 0;
	dev_err(&hw->pdev->dev, "Device removed\n");
}

void atl_check_unplug(struct atl_hw *hw, uint32_t addr)
{
	uint32_t val;

	if (addr == ATL_GLOBAL_MIF_ID) {
		atl_unplugged(hw);
		return;
	}

	val = atl_read(hw, ATL_GLOBAL_MIF_ID);
	if (val == 0xffffffff)
		atl_unplugged(hw);
}

int atl_read_mcp_mem(struct atl_hw *hw, uint32_t mcp_addr, void *host_addr,
		      unsigned int size)
{
	uint32_t *addr = (uint32_t *)host_addr;

	size = (size + 3) & ~3u;
	atl_write(hw, ATL_GLOBAL_MBOX_ADDR, mcp_addr);
	while (size) {
		uint32_t next;

		atl_write(hw, ATL_GLOBAL_MBOX_CTRL, 0x8000);

		busy_wait(100, udelay(10), next,
			  atl_read(hw, ATL_GLOBAL_MBOX_ADDR), next == mcp_addr);
		if (next == mcp_addr) {
			atl_dev_err("mcp mem read timed out (%d remaining)\n",
				    size);
			return -EIO;
		}
		*addr = atl_read(hw, ATL_GLOBAL_MBOX_DATA);
		mcp_addr += 4;
		addr++;
		size -= 4;
	}
	return 0;
}


static inline void atl_glb_soft_reset(struct atl_hw *hw)
{
	atl_write_bit(hw, ATL_GLOBAL_STD_CTRL, 14, 0);
	atl_write_bit(hw, ATL_GLOBAL_STD_CTRL, 15, 1);
}

static inline void atl_glb_soft_reset_full(struct atl_hw *hw)
{
	atl_write_bit(hw, ATL_TX_CTRL1, 29, 0);
	atl_write_bit(hw, ATL_RX_CTRL1, 29, 0);
	atl_write_bit(hw, ATL_INTR_CTRL, 29, 0);
	atl_write_bit(hw, ATL_MPI_CTRL1, 29, 0);
	atl_glb_soft_reset(hw);
}

static void atl2_hw_new_rx_filter_vlan_promisc(struct atl_hw *hw, bool promisc);
static void atl2_hw_new_rx_filter_promisc(struct atl_hw *hw, bool promisc);
static void atl2_hw_init_new_rx_filters(struct atl_hw *hw);

static void atl_set_promisc(struct atl_hw *hw, bool enabled)
{
	atl_write_bit(hw, ATL_RX_FLT_CTRL1, 3, enabled);
	if (hw->new_rpf)
		atl2_hw_new_rx_filter_promisc(hw, enabled);
}

void atl_set_vlan_promisc(struct atl_hw *hw, int promisc)
{
	atl_write_bit(hw, ATL_RX_VLAN_FLT_CTRL1, 1, !!promisc);
	if (hw->new_rpf)
		atl2_hw_new_rx_filter_vlan_promisc(hw, !!promisc);
}

static inline void atl_enable_dma_net_lpb_mode(struct atl_nic *nic)
{
	struct atl_hw *hw = &nic->hw;

	atl_set_vlan_promisc(hw, 1);
	atl_set_promisc(hw, 1);
	atl_write_bit(hw, ATL_TX_PBUF_CTRL1, 4, 0);
	atl_write_bit(hw, ATL_TX_CTRL1, 4, 1);
	atl_write_bit(hw, ATL_RX_CTRL1, 4, 1);
}
/* entered with fw lock held */
static int atl_hw_reset_nonrbl(struct atl_hw *hw)
{
	uint32_t tries;
	uint32_t reg = atl_read(hw, ATL_GLOBAL_DAISY_CHAIN_STS1);
	int ret;

	bool daisychain_running = (reg & 0x30) != 0x30;

	if (daisychain_running)
		atl_dev_dbg("AQDBG: daisychain running (0x18: %#x)\n",
			    atl_read(hw, ATL_GLOBAL_FW_IMAGE_ID));

	atl_write(hw, 0x404, 0x40e1);
	mdelay(50);

	atl_write(hw, 0x534, 0xa0);
	atl_write(hw, 0x100, 0x9f);
	atl_write(hw, 0x100, 0x809f);
	mdelay(50);

	atl_glb_soft_reset(hw);

	atl_write(hw, 0x404, 0x80e0);
	atl_write(hw, 0x32a8, 0);
	atl_write(hw, 0x520, 1);
	mdelay(50);
	atl_write(hw, 0x404, 0x180e0);

	tries = busy_wait(10000, mdelay(1), reg, atl_read(hw, 0x704),
		!(reg & 0x10));
	if (!(reg & 0x10)) {
		atl_dev_err("FLB kickstart timed out: %#x\n", reg);
		ret = -EIO;
		goto unlock;
	}
	atl_dev_dbg("FLB kickstart took %d ms\n", tries);

	atl_write(hw, 0x404, 0x40e1);
	mdelay(50);
	atl_write(hw, 0x3a0, 1);

	atl_glb_soft_reset_full(hw);

	if (hw->mcp.ops)
		hw->mcp.ops->restore_cfg(hw);

	/* unstall FW*/
	atl_write(hw, 0x404, 0x40e0);

	ret = atl_fw_init(hw);

unlock:
	atl_unlock_fw(hw);

	if (ret)
		set_bit(ATL_ST_RESET_NEEDED, &hw->state);
	else
		set_bit(ATL_ST_GLOBAL_CONF_NEEDED, &hw->state);

	return ret;
}


#define ATL2_BOOT_STARTED         BIT(0x18)
#define ATL2_CRASH_INIT           BIT(0x1B)
#define ATL2_BOOT_CODE_FAILED     BIT(0x1C)
#define ATL2_FW_INIT_FAILED       BIT(0x1D)
#define ATL2_FW_INIT_COMP_SUCCESS BIT(0x1F)
#define ATL2_FW_BOOT_FAILED_MASK (ATL2_CRASH_INIT | \
				  ATL2_BOOT_CODE_FAILED | \
				  ATL2_FW_INIT_FAILED)
#define ATL2_FW_BOOT_COMPLETE_MASK (ATL2_FW_BOOT_FAILED_MASK | \
				    ATL2_FW_INIT_COMP_SUCCESS)

#define ATL2_FW_BOOT_REQ_REBOOT        BIT(0x0)
#define ATL2_FW_BOOT_REQ_HOST_BOOT     BIT(0x8)
#define ATL2_FW_BOOT_REQ_MAC_FAST_BOOT BIT(0xA)
#define ATL2_FW_BOOT_REQ_PHY_FAST_BOOT BIT(0xB)

static bool atl2_mcp_boot_complete(struct atl_hw *hw)
{
	u32 rbl_status;

	rbl_status = atl_read(hw, ATL2_MIF_BOOT_REG_ADR);
	if (rbl_status & ATL2_FW_BOOT_COMPLETE_MASK)
		return true;

	/* Host boot requested */
	if (atl_read(hw, ATL2_MCP_HOST_REQ_INT) & 0x1)
		return true;

	return false;
}

static int atl2_hw_reset(struct atl_hw *hw)
{
	bool rbl_complete = false;
	u32 rbl_status = 0;
	u32 rbl_request;
	int err = 0;

	atl_lock_fw(hw);

	busy_wait(50, mdelay(1), rbl_status,
		  atl_read(hw, ATL2_MIF_BOOT_REG_ADR),
		  ((rbl_status & ATL2_BOOT_STARTED) == 0) ||
		  (rbl_status == 0xffffffff));
	if (!(rbl_status & ATL2_BOOT_STARTED))
		atl_dev_dbg("Boot code probably hanged, reboot anyway");


	atl_write(hw, ATL2_MCP_HOST_REQ_INT_CLR, 0x01);
	rbl_request = ATL2_FW_BOOT_REQ_REBOOT;
#ifdef AQ_CFG_FAST_START
	rbl_request |= ATL2_FW_BOOT_REQ_MAC_FAST_BOOT;
#endif

/*
	atl_set_bits(hw, 0x404, 1);
*/
	atl_write(hw, ATL2_MIF_BOOT_REG_ADR, rbl_request);
/*
	if (hw->mcp.ops)
		hw->mcp.ops->restore_cfg(hw);
	atl_clear_bits(hw, 0x404, 1);
*/
	/* Wait for RBL boot */
	busy_wait(200, mdelay(1), rbl_status,
		  atl_read(hw, ATL2_MIF_BOOT_REG_ADR),
		  ((rbl_status & ATL2_BOOT_STARTED) == 0) ||
		  (rbl_status == 0xffffffff));
	if (!(rbl_status & ATL2_BOOT_STARTED)) {
		err = -ETIME;
		atl_dev_err("Boot code hanged");
		goto unlock;
	}

	busy_wait(100, mdelay(1), rbl_complete,
		  atl2_mcp_boot_complete(hw),
		  !rbl_complete);
	if (!rbl_complete) {
		err = -ETIME;
		atl_dev_err("FW Restart timed out");
		goto unlock;
	}

	rbl_status = atl_read(hw, ATL2_MIF_BOOT_REG_ADR);

	if (rbl_status & ATL2_FW_BOOT_FAILED_MASK) {
		err = -EIO;
		atl_dev_err("FW Restart failed, status  = %#x", rbl_status);
		goto unlock;
	}

	if (atl_read(hw, ATL2_MCP_HOST_REQ_INT) & 0x1) {
		err = -EIO;
		atl_dev_err("No FW detected. Dynamic FW load not implemented");
		goto unlock;
	}

	err = atl2_fw_init(hw);

unlock:
	atl_unlock_fw(hw);

	if (err)
		set_bit(ATL_ST_RESET_NEEDED, &hw->state);
	else
		set_bit(ATL_ST_GLOBAL_CONF_NEEDED, &hw->state);
	return err;
}

static int atl1_hw_reset(struct atl_hw *hw)
{
	uint32_t reg;
	uint32_t flb_stat;
	int tries = 0;
	/* bool host_load_done = false; */
	int ret;

	atl_lock_fw(hw);

	reg = atl_read(hw, ATL_MCP_SCRATCH(RBL_STS));
	flb_stat = atl_read(hw, ATL_GLOBAL_DAISY_CHAIN_STS1);

	while (!reg && flb_stat == 0x6000000 && tries++ < 1000) {
		mdelay(1);
		reg = atl_read(hw, ATL_MCP_SCRATCH(RBL_STS));
		flb_stat = atl_read(hw, ATL_GLOBAL_DAISY_CHAIN_STS1);
	}

	atl_dev_dbg("0x388: %#x 0x704: %#x\n", reg, flb_stat);
	if (tries >= 1000) {
		atl_dev_err("Timeout waiting to choose RBL or FLB path\n");
		ret = -EIO;
		goto unlock;
	}

	if (!reg)
		/* atl_hw_reset_nonrbl() releases the fw lock */
		return atl_hw_reset_nonrbl(hw);

	atl_write(hw, 0x404, 0x40e1);
	atl_write(hw, 0x3a0, 1);
	atl_write(hw, 0x32a8, 0);

	atl_write(hw, ATL_MCP_SCRATCH(RBL_STS), 0xdead);

	atl_glb_soft_reset_full(hw);

	if (hw->mcp.ops)
		hw->mcp.ops->restore_cfg(hw);

	atl_write(hw, ATL_GLOBAL_CTRL2, 0x40e0);

	for (tries = 0; tries < 10000; mdelay(1)) {
		tries++;
		reg = atl_read(hw, ATL_MCP_SCRATCH(RBL_STS)) & 0xffff;

		if (reg && reg != 0xdead)
			break;
	}

	if (reg == 0xf1a7) {
		atl_dev_err("MAC FW Host load not supported yet\n");
		ret = -EIO;
		goto unlock;
	}
	if (!reg || reg == 0xdead) {
		atl_dev_err("RBL restart timeout: %#x\n", reg);
		ret = -EIO;
		goto unlock;
	}
	atl_dev_dbg("RBL restart took %d ms result %#x\n", tries, reg);

	/* if (host_load_done) { */
	/* 	// Wait for MAC FW to decide whether it wants to reload the PHY FW */
	/* 	busy_wait(10, mdelay(1), reg, atl_read(hw, 0x340), !(reg & (1 << 9 | 1 << 1 | 1 << 0))); */

	/* 	if (reg & 1 << 9) { */
	/* 		ret = atl_load_phy_fw(hw); */
	/* 		if (ret) { */
	/* 			atl_dev_err("PHY FW host load failed\n"); */
	/* 			return ret; */
	/* 		} */
	/* 	} */
	/* } */

	ret = atl_fw_init(hw);

unlock:
	atl_unlock_fw(hw);

	if (ret)
		set_bit(ATL_ST_RESET_NEEDED, &hw->state);
	else
		set_bit(ATL_ST_GLOBAL_CONF_NEEDED, &hw->state);

	return ret;
}

/* Must be called either during early init when netdev isn't yet
 * registered, or with RTNL lock held
 */
int atl_hw_reset(struct atl_hw *hw)
{
	int ret;

	if (hw->chip_id == ATL_ANTIGUA)
		ret = atl2_hw_reset(hw);
	else
		ret = atl1_hw_reset(hw);
	if (ret)
		return ret;

	ret = atl_fw_configure(hw);

	return ret;
}

static int atl_get_mac_addr(struct atl_hw *hw, uint8_t *buf)
{
	int ret;

	ret = hw->mcp.ops->get_mac_addr(hw, buf);

	return ret;
}

static unsigned int atl_newrpf = 1;
atl_module_param(newrpf, uint, 0644);

int atl_hwinit(struct atl_hw *hw, enum atl_chip chip_id)
{
	int ret;

	hw->chip_id = chip_id;
	if (chip_id == ATL_ANTIGUA && atl_newrpf)
		hw->new_rpf = 1;

	hw->thermal = atl_def_thermal;

	ret = atl_hw_reset(hw);

	atl_dev_info("rev 0x%x chip 0x%x FW img 0x%x\n",
		 atl_read(hw, ATL_GLOBAL_CHIP_REV) & 0xffff,
		 atl_read(hw, ATL_GLOBAL_CHIP_ID) & 0xffff,
		 hw->mcp.fw_rev);

	if (ret)
		return ret;

	ret = atl_get_mac_addr(hw, hw->mac_addr);
	if (ret)
		atl_dev_err("couldn't read MAC address\n");

	return ret;
}

static void atl_rx_xoff_set(struct atl_hw *hw, bool fc)
{
	atl_write_bit(hw, ATL_RX_PBUF_REG2(0), 31, fc);
}

void atl_refresh_link(struct atl_nic *nic)
{
	struct atl_hw *hw = &nic->hw;
	struct atl_link_type *link, *prev_link = hw->link_state.link;

	if (test_bit(ATL_ST_RESETTING, &hw->state) ||
	    !test_bit(ATL_ST_ENABLED, &hw->state) ||
	    !test_and_clear_bit(ATL_ST_UPDATE_LINK, &hw->state))
		return;

	link = hw->mcp.ops->check_link(hw);

	if (link) {
		if (link != prev_link) {
			atl_nic_info("Link up: %s\n", link->name);
			netif_carrier_on(nic->ndev);
			pm_runtime_get_sync(&nic->hw.pdev->dev);
#if IS_ENABLED(CONFIG_MACSEC) && defined(NETIF_F_HW_MACSEC)
			atl_init_macsec(hw);
#endif
		}
	} else {
		if (link != prev_link) {
			atl_nic_info("Link down\n");
			netif_carrier_off(nic->ndev);
			pm_runtime_put_sync(&nic->hw.pdev->dev);
		}
	}
	atl_rx_xoff_set(hw, !!(hw->link_state.fc.cur & atl_fc_rx));

	atl_intr_enable_non_ring(nic);
}

static irqreturn_t atl_link_irq(int irq, void *priv)
{
	struct atl_nic *nic = (struct atl_nic *)priv;

	set_bit(ATL_ST_UPDATE_LINK, &nic->hw.state);
	atl_schedule_work(nic);
	if (nic->hw.chip_id == ATL_ANTIGUA)
		atl_write(&nic->hw, ATL2_MCP_HOST_REQ_INT_CLR, 0xffff);

	return IRQ_HANDLED;
}

static irqreturn_t atl_legacy_irq(int irq, void *priv)
{
	struct atl_nic *nic = priv;
	struct atl_hw *hw = &nic->hw;
	uint32_t mask = hw->non_ring_intr_mask;
	uint32_t stat;
	int cpu;
	int i;

	for (i = 0; i != nic->nvecs; i++)
		mask |= BIT(atl_qvec_intr(&nic->qvecs[i]));

	stat = atl_read(hw, ATL_INTR_STS);

	/* Mask asserted intr sources */
	atl_intr_disable(hw, stat);

	if (!(stat & mask))
		/* Interrupt from another device on a shared int
		 * line. As no status bits were set, nothing was
		 * masked above, so no need to unmask anything. */
		return IRQ_NONE;

	for (i = 0; i != nic->nvecs; i++) {
		if (likely(stat & BIT(atl_qvec_intr(&nic->qvecs[i])))) {
			if (nic->nvecs == 1 || !atl_wq_non_msi) {
				atl_ring_irq(irq, &nic->qvecs[i].napi);
				continue;
			}

			cpu = cpumask_any(&nic->qvecs[i].affinity_hint);
			WARN_ON_ONCE(cpu >= nr_cpu_ids);
			if (cpu >= nr_cpu_ids)
				cpu = 0;
			schedule_work_on(cpu, nic->qvecs[i].work);
		}
	}

	if (unlikely(stat & hw->non_ring_intr_mask))
		atl_link_irq(irq, nic);
	return IRQ_HANDLED;
}

int atl_alloc_link_intr(struct atl_nic *nic)
{
	struct pci_dev *pdev = nic->hw.pdev;
	int ret;

	if (nic->flags & ATL_FL_MULTIPLE_VECTORS) {
		ret = request_irq(pci_irq_vector(pdev, 0), atl_link_irq, 0,
		nic->ndev->name, nic);
		if (ret)
			atl_nic_err("request MSI link vector failed: %d\n",
				-ret);
		return ret;
	}

	ret = request_irq(pci_irq_vector(pdev, 0), atl_legacy_irq, IRQF_SHARED,
		nic->ndev->name, nic);
	if (ret)
		atl_nic_err("request legacy irq failed: %d\n", -ret);

	return ret;
}

void atl_free_link_intr(struct atl_nic *nic)
{
	struct atl_hw *hw = &nic->hw;

	atl_intr_disable(hw, BIT(0));
	free_irq(pci_irq_vector(hw->pdev, 0), nic);
}

void atl_set_uc_flt(struct atl_hw *hw, int idx, uint8_t mac_addr[ETH_ALEN])
{
	atl_write(hw, ATL_RX_UC_FLT_REG1(idx),
		be32_to_cpu(*(uint32_t *)&mac_addr[2]));
	atl_write(hw, ATL_RX_UC_FLT_REG2(idx),
		(uint32_t)be16_to_cpu(*(uint16_t *)mac_addr) |
		1 << 16 | 1 << 31);
	atl_write_bits(hw, ATL_RX_UC_FLT_REG2(idx), 22, 6, ATL2_RPF_TAG_BASE_UC);
}

static void atl_disable_uc_flt(struct atl_hw *hw, int idx)
{
	atl_write(hw, ATL_RX_UC_FLT_REG2(idx), 0);
}

void atl_set_rss_key(struct atl_hw *hw)
{
	int i;
	uint32_t val;

	for (i = 0; i < ATL_RSS_KEY_SIZE / 4; i++) {
		val = swab32(((uint32_t *)hw->rss_key)[i]);
		atl_write(hw, ATL_RX_RSS_KEY_WR_DATA, val);
		atl_write(hw, ATL_RX_RSS_KEY_ADDR, i | BIT(5));
		busy_wait(100, udelay(1), val,
			atl_read(hw, ATL_RX_RSS_KEY_ADDR),
			val & BIT(5));
		if (val & BIT(5)) {
			atl_dev_err("Timeout writing RSS key[%d]: %#x\n",
				i, val);
			return;
		}
	}
}

int atl_set_rss_tbl(struct atl_hw *hw)
{
	int i, shift = 0, addr = 0;
	uint32_t val = 0, stat;
	uint32_t tc = 0;

	if (hw->new_rpf)
		for (i = ATL_RSS_TBL_SIZE; i--;) {
			atl_write_bits(hw, ATL2_RPF_RSS_REDIR(tc, i),
				       5 * ((tc) % 4), 5, hw->rss_tbl[i]);
		}

	for (i = 0; i < ATL_RSS_TBL_SIZE; i++) {
		val |= (uint32_t)(hw->rss_tbl[i]) << shift;
		shift += 3;

		if (shift < 16)
			continue;

		atl_write(hw, ATL_RX_RSS_TBL_WR_DATA, val & 0xffff);
		atl_write(hw, ATL_RX_RSS_TBL_ADDR, addr | BIT(4));

		busy_wait(100, udelay(1), stat,
			atl_read(hw, ATL_RX_RSS_TBL_ADDR), stat & BIT(4));
		if (stat & BIT(4)) {
			atl_dev_err("Timeout writing RSS redir table[%d] (addr %d): %#x\n",
				    i, addr, stat);
			return -ETIME;
		}

		shift -= 16;
		val >>= 16;
		addr++;
	}
	return 0;
}

unsigned int atl_fwd_rx_buf_reserve =
#ifdef CONFIG_ATLFWD_FWD_RXBUF
	CONFIG_ATLFWD_FWD_RXBUF;
#else
	0;
#endif

unsigned int atl_fwd_tx_buf_reserve =
#ifdef CONFIG_ATLFWD_FWD_TXBUF
	CONFIG_ATLFWD_FWD_TXBUF;
#else
	0;
#endif

module_param_named(fwd_tx_buf_reserve, atl_fwd_tx_buf_reserve, uint, 0444);
module_param_named(fwd_rx_buf_reserve, atl_fwd_rx_buf_reserve, uint, 0444);

/* Must be called either during early init when netdev isn't yet
 * registered, or with RTNL lock held */
void atl_start_hw_global(struct atl_nic *nic)
{
	struct atl_hw *hw = &nic->hw;
	int rpb_size = 320, tpb_size = 160;

	if (!test_and_clear_bit(ATL_ST_GLOBAL_CONF_NEEDED, &hw->state))
		return;

	if (nic->priv_flags & ATL_PF_BIT(LPB_NET_DMA))
		atl_enable_dma_net_lpb_mode(nic);

	/* Enable TPO2 */
	atl_write(hw, 0x7040, 0x10000);
	/* Enable RPF2, filter logic 3 */
	atl_write(hw, 0x5040, BIT(16) | (3 << 17));

	if (hw->chip_id == ATL_ANTIGUA) {
		rpb_size = 192;
		tpb_size = 128;
	}
	/* Alloc TPB */
	/* TC1: space for offload engine iface */
	atl_write(hw, ATL_TX_PBUF_REG1(1), atl_fwd_tx_buf_reserve);
	atl_write(hw, ATL_TX_PBUF_REG2(1),
		(atl_fwd_tx_buf_reserve * 32 * 66 / 100) << 16 |
		(atl_fwd_tx_buf_reserve * 32 * 50 / 100));
	/* TC0: 160k minus TC1 size */
	atl_write(hw, ATL_TX_PBUF_REG1(0), tpb_size - atl_fwd_tx_buf_reserve);
	atl_write(hw, ATL_TX_PBUF_REG2(0),
		((tpb_size - atl_fwd_tx_buf_reserve) * 32 * 66 / 100) << 16 |
		((tpb_size - atl_fwd_tx_buf_reserve) * 32 * 50 / 100));
	/* 4-TC | Enable TPB */
	atl_set_bits(hw, ATL_TX_PBUF_CTRL1, BIT(8) | BIT(0));
	/* TX Buffer clk gate  off */
	if (hw->chip_id == ATL_ANTIGUA)
		atl_clear_bits(hw, ATL_TX_PBUF_CTRL1, BIT(5));

	/* Alloc RPB */
	/* TC1: space for offload engine iface */
	atl_write(hw, ATL_RX_PBUF_REG1(1), atl_fwd_rx_buf_reserve);
	atl_write(hw, ATL_RX_PBUF_REG2(1), BIT(31) |
		(atl_fwd_rx_buf_reserve * 32 * 66 / 100) << 16 |
		(atl_fwd_rx_buf_reserve * 32 * 50 / 100));
	/* TC1: 320k minus TC1 size */
	atl_write(hw, ATL_RX_PBUF_REG1(0), rpb_size - atl_fwd_rx_buf_reserve);
	atl_write(hw, ATL_RX_PBUF_REG2(0), BIT(31) |
		((rpb_size - atl_fwd_rx_buf_reserve) * 32 * 66 / 100) << 16 |
		((rpb_size - atl_fwd_rx_buf_reserve) * 32 * 50 / 100));
	/* 4-TC | Enable RPB */
	atl_set_bits(hw, ATL_RX_PBUF_CTRL1, BIT(8) | BIT(4) | BIT(0));

	/* TPO */
	/* Enable L3 | L4 chksum */
	atl_set_bits(hw, ATL_TX_PO_CTRL1, 3);
	/* TSO TCP flags bitmask first / middle */
	atl_write(hw, ATL_TX_LSO_TCP_CTRL1, 0x0ff60ff6);
	/* TSO TCP flags bitmask last */
	atl_write(hw, ATL_TX_LSO_TCP_CTRL2, 0xf7f);

	/* RPO */
	/* Enable  L3 | L4 chksum */
	atl_set_bits(hw, ATL_RX_PO_CTRL1, 3);
	atl_write_bits(hw, ATL_RX_LRO_CTRL2, 12, 2, 0);
	atl_write_bits(hw, ATL_RX_LRO_CTRL2, 5, 2, 0);
	/* 10uS base, 20uS inactive timeout, 60 uS max coalescing
	 * interval
	 */
	atl_write(hw, ATL_RX_LRO_TMRS, 0xc35 << 20 | 2 << 10 | 6);
	atl_write(hw, ATL_INTR_RSC_DELAY, (atl_min_intr_delay / 2) - 1);

	/* RPF */
	/* Default RPF2 parser options */
	atl_write(hw, ATL_RX_FLT_CTRL2, 0x0);
	atl_set_uc_flt(hw, 0, hw->mac_addr);
	/* BC action host */
	atl_write_bits(hw, ATL_RX_FLT_CTRL1, 12, 3, 1);
	/* Enable BC */
	atl_write_bit(hw, ATL_RX_FLT_CTRL1, 0, 1);
	/* BC thresh */
	atl_write_bits(hw, ATL_RX_FLT_CTRL1, 16, 16, 0x1000);

	/* Enable untagged packets */
	atl_write(hw, ATL_RX_VLAN_FLT_CTRL1, 1 << 2 | 1 << 3);

	if (hw->chip_id == ATL_ANTIGUA) {
		/* RSS hash type */
		atl_set_bits(hw, ATL2_RX_RSS_HASH_TYPE_ADR, BIT(9) - 1);

		/* ATL2 Apply legacy ring to TC mapping */
		atl_write(hw, ATL2_RX_Q_TO_TC_MAP(0), 0x00000000);
		atl_write(hw, ATL2_RX_Q_TO_TC_MAP(1), 0x11111111);
		atl_write(hw, ATL2_RX_Q_TO_TC_MAP(2), 0x22222222);
		atl_write(hw, ATL2_RX_Q_TO_TC_MAP(3), 0x33333333);
		atl_write(hw, ATL2_TX_Q_TO_TC_MAP(0), 0x00000000);
		atl_write(hw, ATL2_TX_Q_TO_TC_MAP(1), 0x00000000);
		atl_write(hw, ATL2_TX_Q_TO_TC_MAP(2), 0x01010101);
		atl_write(hw, ATL2_TX_Q_TO_TC_MAP(3), 0x01010101);
		atl_write(hw, ATL2_TX_Q_TO_TC_MAP(4), 0x02020202);
		atl_write(hw, ATL2_TX_Q_TO_TC_MAP(5), 0x02020202);
		atl_write(hw, ATL2_TX_Q_TO_TC_MAP(6), 0x03030303);
		atl_write(hw, ATL2_TX_Q_TO_TC_MAP(7), 0x03030303);

	}
	/* Turn new filters on*/
	if (hw->new_rpf) {
		atl_set_bits(hw, ATL_RX_FLT_CTRL2, BIT(0xB));
		atl2_hw_init_new_rx_filters(hw);
	}

	/* Reprogram ethtool Rx filters */
	atl_refresh_rxfs(nic);

	atl_set_rss_key(hw);
	/* Enable RSS | 8 queues per TC */
	atl_write(hw, ATL_RX_RSS_CTRL, BIT(31) | 3);

	/* Global interrupt block init */
	if (nic->flags & ATL_FL_MULTIPLE_VECTORS) {
		/* MSI or MSI-X mode interrupt mode */
		uint32_t ctrl = hw->pdev->msix_enabled ? 2 : 1;

		/* Enable multi-vector mode and mask autoclear
		 * register */
		ctrl |= BIT(2) | BIT(5);

		atl_write(hw, ATL_INTR_CTRL, ctrl);

		/* Enable auto-masking of link interrupt on intr generation */
		atl_set_bits(hw, ATL_INTR_AUTO_MASK, BIT(0));
		/* Enable status auto-clear on link intr generation */
		atl_set_bits(hw, ATL_INTR_AUTO_CLEAR, BIT(0));
	} else {
		/* Enable legacy INTx mode and status clear-on-read */
		atl_write(hw, ATL_INTR_CTRL, BIT(7));
		/* Clear the registers, which might have been set on previous
		 * driver load and might interfere with legacy IRQ handling
		 */
		atl_write(hw, ATL_INTR_AUTO_MASK, 0);
		atl_write(hw, ATL_INTR_AUTO_CLEAR, 0);
	}

	/* Map MCP 4 interrupt to cause 0 */
	atl_write(hw, ATL_INTR_GEN_INTR_MAP4, BIT(7) | (0 << 0));

	if (hw->chip_id == ATL_ANTIGUA) {
		atl_set_bits(hw, ATL_GLOBAL_CTRL2, BIT(3) << 0xA);
		atl_write(hw, ATL2_MCP_HOST_REQ_INT_MASK(3),
			  ATL2_FW_HOST_INTERRUPT_LINK_UP |
			  ATL2_FW_HOST_INTERRUPT_LINK_DOWN);
	}

	atl_write(hw, ATL_TX_INTR_CTRL, BIT(4));
	atl_write(hw, ATL_RX_INTR_CTRL, BIT(3));

	/* Reset Rx/Tx on unexpected PERST# */
	atl_write_bit(hw, 0x1000, 29, 0);
	atl_write(hw, 0x448, 3);
}

#define atl_vlan_flt_val(vid) ((uint32_t)(vid) | 1 << 16 | 1 << 31)

static void atl_set_all_multi(struct atl_hw *hw, bool all_multi)
{
	atl_write_bit(hw, ATL_RX_MC_FLT_MSK, 14, all_multi);
	atl_write(hw, ATL_RX_MC_FLT(0), all_multi ? 0x80010FFF : 0x00010FFF);
}

void atl_set_rx_mode(struct net_device *ndev)
{
	struct atl_nic *nic = netdev_priv(ndev);
	struct atl_hw *hw = &nic->hw;
	bool is_multicast_enabled = !!(ndev->flags & IFF_MULTICAST);
	int all_multi_needed = !!(ndev->flags & IFF_ALLMULTI);
	int promisc_needed = !!(ndev->flags & IFF_PROMISC);
	int uc_count = netdev_uc_count(ndev);
	int mc_count = 0;
	int i = 1; /* UC filter 0 reserved for MAC address */
	struct netdev_hw_addr *hwaddr;

	if (!pm_runtime_active(&nic->hw.pdev->dev))
		return;

	if (is_multicast_enabled)
		mc_count = netdev_mc_count(ndev);

	if (uc_count > ATL_UC_FLT_NUM - 1)
		promisc_needed |= 1;
	else if (uc_count + mc_count > ATL_UC_FLT_NUM - 1)
		all_multi_needed |= 1;


	/* Enable promisc VLAN mode if IFF_PROMISC explicitly
	 * requested or too many VIDs registered
	 */
	atl_set_vlan_promisc(hw,
		ndev->flags & IFF_PROMISC || nic->rxf_vlan.promisc_count ||
		!nic->rxf_vlan.vlans_active);

	atl_set_promisc(hw, promisc_needed);
	if (promisc_needed)
		return;

	netdev_for_each_uc_addr(hwaddr, ndev)
		atl_set_uc_flt(hw, i++, hwaddr->addr);

	atl_set_all_multi(hw, is_multicast_enabled && all_multi_needed);

	if (is_multicast_enabled && !all_multi_needed)
		netdev_for_each_mc_addr(hwaddr, ndev)
			atl_set_uc_flt(hw, i++, hwaddr->addr);

	while (i < ATL_UC_FLT_NUM)
		atl_disable_uc_flt(hw, i++);
}

int atl_alloc_descs(struct atl_nic *nic, struct atl_hw_ring *ring)
{
	struct device *dev = &nic->hw.pdev->dev;

	ring->descs = dma_alloc_coherent(dev, ring->size * sizeof(*ring->descs),
					 &ring->daddr, GFP_KERNEL);

	if (!ring->descs)
		return -ENOMEM;

	return 0;
}

void atl_free_descs(struct atl_nic *nic, struct atl_hw_ring *ring)
{
	struct device *dev = &nic->hw.pdev->dev;

	if (!ring->descs)
		return;

	dma_free_coherent(dev, ring->size * sizeof(*ring->descs),
		ring->descs, ring->daddr);
	ring->descs = 0;
}

void atl_set_intr_bits(struct atl_hw *hw, int idx, int rxbit, int txbit)
{
	int shift = idx & 1 ? 0 : 8;
	uint32_t clear_mask = 0;
	uint32_t set_mask = 0;
	uint32_t val;

	if (rxbit >= 0) {
		clear_mask |= BIT(7) | (BIT(5) - 1);
		if (rxbit < ATL_NUM_MSI_VECS)
			set_mask |= BIT(7) | rxbit;
	}
	if (txbit >= 0) {
		clear_mask |= (BIT(7) | (BIT(5) - 1)) << 0x10;
		if (txbit < ATL_NUM_MSI_VECS)
			set_mask |= (BIT(7) | txbit) << 0x10;
	}

	val = atl_read(hw, ATL_INTR_RING_INTR_MAP(idx));
	val &= ~(clear_mask << shift);
	val |= set_mask << shift;
	atl_write(hw, ATL_INTR_RING_INTR_MAP(idx), val);
}

void atl_set_loopback(struct atl_nic *nic, int idx, bool on)
{
	struct atl_hw *hw = &nic->hw;

	switch (idx) {
	case ATL_PF_LPB_SYS_DMA:
		atl_write_bit(hw, ATL_TX_CTRL1, 6, on);
		atl_write_bit(hw, ATL_RX_CTRL1, 6, on);
		break;
	case ATL_PF_LPB_SYS_PB:
		atl_write_bit(hw, ATL_TX_CTRL1, 7, on);
		atl_write_bit(hw, ATL_RX_CTRL1, 8, on);
		break;
	case ATL_PF_LPB_INT_PHY:
	case ATL_PF_LPB_EXT_PHY:
		hw->mcp.ops->set_phy_loopback(nic, idx);
		break;
	case ATL_PF_LPB_NET_DMA:
		/* To switch DMANetworkLoopback mode
		 * you need a reset datapath
		 */
		set_bit(ATL_ST_GLOBAL_CONF_NEEDED, &hw->state);
		atl_reconfigure(nic);
		break;
	}
}

int atl_hwsem_get(struct atl_hw *hw, int idx)
{
	uint32_t val;

	busy_wait(10000, udelay(1), val, atl_read(hw, ATL_MCP_SEM(idx)), !val);

	if (!val)
		return -ETIME;

	return 0;
}

void atl_hwsem_put(struct atl_hw *hw, int idx)
{
	atl_write(hw, ATL_MCP_SEM(idx), 1);
}

static int atl_msm_wait(struct atl_hw *hw)
{
	uint32_t val;

	busy_wait(10, udelay(1), val, atl_read(hw, ATL_MPI_MSM_ADDR),
		val & BIT(12));
	if (val & BIT(12))
		return -ETIME;

	return 0;
}

int __atl_msm_read(struct atl_hw *hw, uint32_t addr, uint32_t *val)
{
	int ret;

	ret = atl_msm_wait(hw);
	if (ret)
		return ret;

	atl_write(hw, ATL_MPI_MSM_ADDR, (addr >> 2) | BIT(9));
	ret = atl_msm_wait(hw);
	if (ret)
		return ret;

	*val = atl_read(hw, ATL_MPI_MSM_RD);
	return 0;
}

int atl_msm_read(struct atl_hw *hw, uint32_t addr, uint32_t *val)
{
	int ret;

	ret = atl_hwsem_get(hw, ATL_MCP_SEM_MSM);
	if (ret)
		return ret;

	ret = __atl_msm_read(hw, addr, val);
	atl_hwsem_put(hw, ATL_MCP_SEM_MSM);

	return ret;
}

int __atl_msm_write(struct atl_hw *hw, uint32_t addr, uint32_t val)
{
	int ret;

	ret = atl_msm_wait(hw);
	if (ret)
		return ret;

	atl_write(hw, ATL_MPI_MSM_WR, val);
	atl_write(hw, ATL_MPI_MSM_ADDR, (addr >> 2) | BIT(8));
	ret = atl_msm_wait(hw);
	if (ret)
		return ret;

	return 0;
}

int atl_msm_write(struct atl_hw *hw, uint32_t addr, uint32_t val)
{
	int ret;

	ret = atl_hwsem_get(hw, ATL_MCP_SEM_MSM);
	if (ret)
		return ret;

	ret = __atl_msm_write(hw, addr, val);
	atl_hwsem_put(hw, ATL_MCP_SEM_MSM);

	return ret;
}

static int atl_mdio_wait(struct atl_hw *hw)
{
	uint32_t val;

	busy_wait(20, udelay(1), val, atl_read(hw, ATL_GLOBAL_MDIO_CMD),
		val & BIT(31));
	if (val & BIT(31))
		return -ETIME;

	return 0;
}

int atl_mdio_hwsem_get(struct atl_hw *hw)
{
	int ret;

	ret = atl_hwsem_get(hw, ATL_MCP_SEM_MDIO);
	if (ret)
		return ret;

	/* Enable MDIO Clock (active low) in case MBU have disabled
	 * it. */
	atl_write_bit(hw, ATL_GLOBAL_MDIO_CTL, 14, 0);
	return 0;
}

void atl_mdio_hwsem_put(struct atl_hw *hw)
{
	/* It's ok to leave MDIO Clock running according to FW
	 * guys. In fact that's what FW does. */
	atl_hwsem_put(hw, ATL_MCP_SEM_MDIO);
}

static void atl_mdio_set_addr(struct atl_hw *hw, uint8_t prtad, uint8_t mmd,
	uint16_t addr)
{
	/* Set address */
	atl_write(hw, ATL_GLOBAL_MDIO_ADDR, addr & (BIT(16) - 1));
	/* Address operation | execute | prtad + mmd */
	atl_write(hw, ATL_GLOBAL_MDIO_CMD, BIT(15) | 3 << 12 |
		prtad << 5 | mmd);
}

int __atl_mdio_read(struct atl_hw *hw, uint8_t prtad, uint8_t mmd,
	uint16_t addr, uint16_t *val)
{
	int ret;

	ret = atl_mdio_wait(hw);
	if (ret)
		return ret;

	atl_mdio_set_addr(hw, prtad, mmd, addr);
	ret = atl_mdio_wait(hw);
	if (ret)
		return ret;

	/* Read operation | execute | prtad + mmd */
	atl_write(hw, ATL_GLOBAL_MDIO_CMD, BIT(15) | 1 << 12 |
		prtad << 5 | mmd);

	ret = atl_mdio_wait(hw);
	if (ret)
		return ret;

	*val = atl_read(hw, ATL_GLOBAL_MDIO_RDATA);
	return 0;
}

int atl_mdio_read(struct atl_hw *hw, uint8_t prtad, uint8_t mmd,
	uint16_t addr, uint16_t *val)
{
	int ret;

	ret = atl_mdio_hwsem_get(hw);
	if (ret)
		return ret;

	ret = __atl_mdio_read(hw, prtad, mmd, addr, val);
	atl_mdio_hwsem_put(hw);

	return ret;
}

int __atl_mdio_write(struct atl_hw *hw, uint8_t prtad, uint8_t mmd,
	uint16_t addr, uint16_t val)
{
	int ret;

	ret = atl_mdio_wait(hw);
	if (ret)
		return ret;

	atl_mdio_set_addr(hw, prtad, mmd, addr);
	ret = atl_mdio_wait(hw);
	if (ret)
		return ret;

	atl_write(hw, ATL_GLOBAL_MDIO_WDATA, val);
	/* Write operation | execute | prtad + mmd */
	atl_write(hw, ATL_GLOBAL_MDIO_CMD, BIT(15) | 2 << 12 |
		prtad << 5 | mmd);
	ret = atl_mdio_wait(hw);
	if (ret)
		return ret;

	return 0;
}

int atl_mdio_write(struct atl_hw *hw, uint8_t prtad, uint8_t mmd,
	uint16_t addr, uint16_t val)
{
	int ret;

	ret = atl_mdio_hwsem_get(hw);
	if (ret)
		return ret;

	ret = __atl_mdio_write(hw, prtad, mmd, addr, val);
	atl_mdio_hwsem_put(hw);

	return 0;
}

#define __READ_MSM_OR_GOTO(RET, HW, REGISTER, PVARIABLE, label) \
	RET = __atl_msm_read(HW, REGISTER, PVARIABLE); \
	if (RET)							\
		goto label;

void atl_adjust_eth_stats(struct atl_ether_stats *stats,
	struct atl_ether_stats *base, bool add)
{
	int i;
	uint64_t *_stats = (uint64_t *)stats;
	uint64_t *_base = (uint64_t *)base;

	for (i = 0; i < sizeof(*stats) / sizeof(uint64_t); i++)
		_stats[i] += add ? _base[i] : - _base[i];
}

int atl_update_eth_stats(struct atl_nic *nic)
{
	struct atl_hw *hw = &nic->hw;
	struct atl_ether_stats stats = {0};
	uint32_t reg = 0, reg2 = 0;
	int ret;

	if (!test_bit(ATL_ST_ENABLED, &nic->hw.state) ||
	    test_bit(ATL_ST_RESETTING, &nic->hw.state))
		return 0;

	atl_lock_fw(hw);

	ret = atl_hwsem_get(hw, ATL_MCP_SEM_MSM);
	if (ret)
		goto unlock_fw;

	__READ_MSM_OR_GOTO(ret, hw, ATL_MSM_CTR_TX_PAUSE, &reg, hwsem_put);
	stats.tx_pause = reg;

	__READ_MSM_OR_GOTO(ret, hw, ATL_MSM_CTR_RX_PAUSE, &reg, hwsem_put);
	stats.rx_pause = reg;

	__READ_MSM_OR_GOTO(ret, hw, ATL_MSM_CTR_RX_OCTETS_LO, &reg, hwsem_put);
	__READ_MSM_OR_GOTO(ret, hw, ATL_MSM_CTR_RX_OCTETS_HI, &reg2, hwsem_put);
	stats.rx_ether_octets = ((uint64_t)reg2 << 32) | reg;

	__READ_MSM_OR_GOTO(ret, hw, ATL_MSM_CTR_RX_PKTS_GOOD, &reg, hwsem_put);
	__READ_MSM_OR_GOTO(ret, hw, ATL_MSM_CTR_RX_ERRS, &reg2, hwsem_put);
	stats.rx_ether_pkts = reg + reg2;;

	__READ_MSM_OR_GOTO(ret, hw, ATL_MSM_CTR_RX_BROADCAST, &reg, hwsem_put);
	stats.rx_ether_broacasts = reg;

	__READ_MSM_OR_GOTO(ret, hw, ATL_MSM_CTR_RX_MULTICAST, &reg, hwsem_put);
	stats.rx_ether_multicasts = reg;

	__READ_MSM_OR_GOTO(ret, hw, ATL_MSM_CTR_RX_FCS_ERRS, &reg, hwsem_put);
	__READ_MSM_OR_GOTO(ret, hw, ATL_MSM_CTR_RX_ALIGN_ERRS, &reg2, hwsem_put);
	stats.rx_ether_crc_align_errs = reg + reg2;

	stats.rx_ether_drops = atl_read(hw, ATL_RX_DMA_STATS_CNT7);

	/* capture debug counters*/
	atl_write_bit(hw, ATL_RX_RPF_DBG_CNT_CTRL, 0x1f, 1);

	reg = atl_read(hw, ATL_RX_RPF_HOST_CNT_LO);
	reg2 = atl_read(hw, ATL_RX_RPF_HOST_CNT_HI);
	stats.rx_filter_host = ((uint64_t)reg2 << 32) | reg;

	reg = atl_read(hw, ATL_RX_RPF_LOST_CNT_LO);
	reg2 = atl_read(hw, ATL_RX_RPF_LOST_CNT_HI);
	stats.rx_filter_lost = ((uint64_t)reg2 << 32) | reg;

	spin_lock(&nic->stats_lock);

	atl_adjust_eth_stats(&stats, &nic->stats.eth_base, false);
	nic->stats.eth = stats;

	spin_unlock(&nic->stats_lock);

	ret = 0;

hwsem_put:
	atl_hwsem_put(hw, ATL_MCP_SEM_MSM);
unlock_fw:
	atl_unlock_fw(hw);
	return ret;
}
#undef __READ_MSM_OR_GOTO

int atl_get_lpi_timer(struct atl_nic *nic, uint32_t *lpi_delay)
{
	struct atl_hw *hw = &nic->hw;
	uint32_t lpi;
	int ret = 0;


	ret = atl_msm_read(hw, ATL_MSM_TX_LPI_DELAY, &lpi);
	if (ret)
		return ret;
	*lpi_delay = ATL_HW_CLOCK_TO_US(lpi);

	return ret;
}

static uint32_t atl_mcp_mbox_wait(struct atl_hw *hw, enum mcp_area area, int loops)
{
	uint32_t stat;

	busy_wait(loops, cpu_relax(), stat,
		(atl_read(hw, ATL_MCP_SCRATCH(FW2_MBOX_CMD)) & (0xf << 28)),
		stat == area);

	return stat;
}

int atl_write_mcp_mem(struct atl_hw *hw, uint32_t offt, void *host_addr,
	size_t size, enum mcp_area area)
{
	uint32_t *addr = (uint32_t *)host_addr;

	if (offt > 0xffff)
		return -EINVAL;

	while (size) {
		uint32_t stat;

		atl_write(hw, ATL_MCP_SCRATCH(FW2_MBOX_DATA), *addr++);
		atl_write(hw, ATL_MCP_SCRATCH(FW2_MBOX_CMD), area | offt);
		ndelay(750);
		stat = atl_mcp_mbox_wait(hw, area, 5);

		if (stat == area) {
			/* Send MCP mbox interrupt */
			atl_set_bits(hw, ATL_GLOBAL_CTRL2, BIT(1));
			ndelay(1200);
			stat = atl_mcp_mbox_wait(hw, area, 10000);
		}

		if (stat == area) {
			atl_dev_err("FW mbox timeout offt %x, remaining %zx\n",
				offt, size);
			return -ETIME;
		} else if (stat != BIT(0x1E)) {
			atl_dev_err("FW mbox error status 0x%x, offt 0x%x, remaining %zx\n",
				stat, offt, size);
			return -EIO;
		}

		offt += 4;
		size -= 4;
	}

	return 0;
}

void atl_thermal_check(struct atl_hw *hw, bool alarm)
{
	int temp, ret;
	struct atl_link_state *lstate = &hw->link_state;
	struct atl_link_type *link = lstate->link;
	int lowest;

	if (link) {
		/* ffs() / fls() number bits starting at 1 */
		lowest = ffs(lstate->lp_advertized) - 1;
		if (lowest < lstate->lp_lowest) {
			lstate->lp_lowest = lowest;
			if (lowest < lstate->throttled_to &&
				lstate->thermal_throttled && alarm)
				/* We're still thermal-throttled, and
				 * just found out we can lower the
				 * speed even more, so renegotiate.
				 */
				goto relink;
		}
	} else
		lstate->lp_lowest = fls(lstate->supported) - 1;

	if (alarm == lstate->thermal_throttled)
		return;

	lstate->thermal_throttled = alarm;

	ret = hw->mcp.ops->get_phy_temperature(hw, &temp);
	if (ret)
		temp = 0;
	else
		/* Temperature is in millidegrees C */
		temp = (temp + 50) / 100;

	if (alarm) {
		if (temp)
			atl_dev_warn("PHY temperature above threshold: %d.%d\n",
				temp / 10, temp % 10);
		else
			atl_dev_warn("PHY temperature above threshold\n");
	} else {
		if (temp)
			atl_dev_warn("PHY temperature back in range: %d.%d\n",
				temp / 10, temp % 10);
		else
			atl_dev_warn("PHY temperature back in range\n");
	}

relink:
	if (hw->thermal.flags & atl_thermal_throttle) {
		/* If throttling is enabled, renegotiate link */
		lstate->link = 0;
		lstate->throttled_to = lstate->lp_lowest;
		hw->mcp.ops->set_link(hw, true);
	}
}

/* Atlanic2 new filters implementation */

/* set action resolver record */
static void atl2_rpf_act_rslvr_record_set(struct atl_hw *hw, u8 location,
				   u32 tag, u32 mask, u32 action)
{
	atl_write(hw, ATL2_RPF_ACT_RSLVR_REQ_TAG(location), tag);
	atl_write(hw, ATL2_RPF_ACT_RSLVR_TAG_MASK(location), mask);
	atl_write(hw, ATL2_RPF_ACT_RSLVR_ACTN(location), action);
}

int atl2_act_rslvr_table_set(struct atl_hw *hw, u8 location,
			     u32 tag, u32 mask, u32 action)
{
	static char action_str[][32] = {"Drop", "Host", "Management",
					"Host & Management"};
	static char valid_str[][32] = {"Not Valid", "Valid"};
	static char rss_str[][32] = {"Queue", "TC"};
	int err = 0;

	dev_dbg(&hw->pdev->dev, "ACTRSLVR[%d] TAG %#x MASK %#x ACTION %#x (%s, %s %d, %s)",
		location, tag, mask, action,
		action_str[(action >> 8) & 3], rss_str[!!(action & BIT(7))],
		(action >> 2) & 0x1f,
		valid_str[action & 1]);

	err = atl_hwsem_get(hw, ATL2_MCP_SEM_ACT_RSLVR);
	if (err)
		return err;

	atl2_rpf_act_rslvr_record_set(hw, location, tag, mask, action);

	atl_hwsem_put(hw, ATL2_MCP_SEM_ACT_RSLVR);

	return err;
}

/** Initialise new rx filters
 * L2 promisc OFF
 * VLAN promisc OFF
 *
 * VLAN
 * MAC
 * ALLMULTI
 * UT
 * VLAN promisc ON
 * L2 promisc ON
 */
static void atl2_hw_init_new_rx_filters(struct atl_hw *hw)
{
	atl_write(hw, ATL2_RPF_REC_TAB_EN, 0xFFFF);
	atl_write_bits(hw, ATL_RX_UC_FLT_REG2(0), 22, 6, ATL2_RPF_TAG_BASE_UC);
	atl_write_bits(hw, ATL2_RX_FLT_L2_BC_TAG, 0, 6, ATL2_RPF_TAG_BASE_UC);
	atl_set_bits(hw, ATL2_RPF_L3_FLT(0), BIT(0x17));

	atl2_act_rslvr_table_set(hw,
				 ATL2_RPF_L2_PROMISC_OFF_INDEX,
				 0,
				 ATL2_RPF_TAG_UC_MASK | ATL2_RPF_TAG_ALLMC_MASK,
				 ATL2_ACTION_DROP);

	atl2_act_rslvr_table_set(hw,
				 ATL2_RPF_VLAN_PROMISC_OFF_INDEX,
				 0,
				 ATL2_RPF_TAG_VLAN_MASK | ATL2_RPF_TAG_UNTAG_MASK,
				 ATL2_ACTION_DROP);


	atl2_act_rslvr_table_set(hw,
				 ATL2_RPF_VLAN_INDEX,
				 ATL2_RPF_TAG_BASE_VLAN,
				 ATL2_RPF_TAG_VLAN_MASK,
				 ATL2_ACTION_ASSIGN_TC(0));

	atl2_act_rslvr_table_set(hw,
				 ATL2_RPF_MAC_INDEX,
				 ATL2_RPF_TAG_BASE_UC,
				 ATL2_RPF_TAG_UC_MASK,
				 ATL2_ACTION_ASSIGN_TC(0));

	atl2_act_rslvr_table_set(hw,
				 ATL2_RPF_ALLMC_INDEX,
				 ATL2_RPF_TAG_BASE_ALLMC,
				 ATL2_RPF_TAG_ALLMC_MASK,
				 ATL2_ACTION_ASSIGN_TC(0));

	atl2_act_rslvr_table_set(hw,
				 ATL2_RPF_UNTAG_INDEX,
				 ATL2_RPF_TAG_UNTAG_MASK,
				 ATL2_RPF_TAG_UNTAG_MASK,
				 ATL2_ACTION_ASSIGN_TC(0));

	atl2_act_rslvr_table_set(hw,
				 ATL2_RPF_VLAN_PROMISC_ON_INDEX,
				 0,
				 ATL2_RPF_TAG_VLAN_MASK,
				 ATL2_ACTION_DISABLE);

	atl2_act_rslvr_table_set(hw,
				 ATL2_RPF_L2_PROMISC_ON_INDEX,
				 0,
				 ATL2_RPF_TAG_UC_MASK,
				 ATL2_ACTION_DISABLE);
}


static void atl2_hw_new_rx_filter_vlan_promisc(struct atl_hw *hw, bool promisc)
{
	u16 on_action = promisc ? ATL2_ACTION_ASSIGN_TC(0) : ATL2_ACTION_DISABLE;
	u16 off_action = !promisc ? ATL2_ACTION_DROP : ATL2_ACTION_DISABLE;

	atl2_act_rslvr_table_set(hw,
				 ATL2_RPF_VLAN_PROMISC_ON_INDEX,
				 0,
				 ATL2_RPF_TAG_VLAN_MASK,
				 on_action);

	atl2_act_rslvr_table_set(hw,
				 ATL2_RPF_VLAN_PROMISC_OFF_INDEX,
				 0,
				 ATL2_RPF_TAG_VLAN_MASK | ATL2_RPF_TAG_UNTAG_MASK,
				 off_action);
}

static void atl2_hw_new_rx_filter_promisc(struct atl_hw *hw, bool promisc)
{
	u16 on_action = promisc ? ATL2_ACTION_ASSIGN_TC(0) : ATL2_ACTION_DISABLE;
	u16 off_action = promisc ? ATL2_ACTION_DISABLE : ATL2_ACTION_DROP;

	atl2_act_rslvr_table_set(hw,
				 ATL2_RPF_L2_PROMISC_OFF_INDEX,
				 0,
				 ATL2_RPF_TAG_UC_MASK | ATL2_RPF_TAG_ALLMC_MASK,
				 off_action);

	atl2_act_rslvr_table_set(hw,
				 ATL2_RPF_L2_PROMISC_ON_INDEX,
				 0,
				 ATL2_RPF_TAG_UC_MASK,
				 on_action);
}

