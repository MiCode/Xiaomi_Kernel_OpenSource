// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Authors:
 *	Stanley Chu <stanley.chu@mediatek.com>
 *	Peter Wang <peter.wang@mediatek.com>
 */

#include <linux/arm-smccc.h>
#include <linux/bitfield.h>
#include <linux/cpumask.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>
#include <linux/reset.h>
#include <linux/tracepoint.h>

#if IS_ENABLED(CONFIG_RPMB)
#include <asm/unaligned.h>
#include <linux/async.h>
#include <linux/rpmb.h>
#endif

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
#include <mt-plat/mtk_blocktag.h>
#endif

#include "ufshcd.h"
#include "ufshcd-crypto.h"
#include "ufshcd-pltfrm.h"
#include "ufs_quirks.h"
#include "unipro.h"
#include "ufs-mediatek.h"
#include "ufs-mediatek-sip.h"

#if IS_ENABLED(CONFIG_SCSI_UFS_MEDIATEK_DBG)
#include "ufs-mediatek-dbg.h"
#endif

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
static int ufs_abort_aee_count;
#endif

extern void mt_irq_dump_status(unsigned int irq);
static void ufs_mtk_auto_hibern8_disable(struct ufs_hba *hba);

#define CREATE_TRACE_POINTS
#include "ufs-mediatek-trace.h"
#undef CREATE_TRACE_POINTS

static struct ufs_dev_fix ufs_mtk_dev_fixups[] = {
	UFS_FIX(UFS_ANY_VENDOR, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM |
		UFS_DEVICE_QUIRK_DELAY_AFTER_LPM),
	UFS_FIX(UFS_VENDOR_SKHYNIX, "H9HQ21AFAMZDAR",
		UFS_DEVICE_QUIRK_SUPPORT_EXTENDED_FEATURES),
	END_FIX
};

static const struct of_device_id ufs_mtk_of_match[] = {
	{ .compatible = "mediatek,mt8183-ufshci" },
	{},
};

static bool ufs_mtk_is_boost_crypt_enabled(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return !!(host->caps & UFS_MTK_CAP_BOOST_CRYPT_ENGINE);
}

static bool ufs_mtk_is_va09_supported(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return !!(host->caps & UFS_MTK_CAP_VA09_PWR_CTRL);
}

static bool ufs_mtk_is_broken_vcc(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return !!(host->caps & UFS_MTK_CAP_BROKEN_VCC);
}

static bool ufs_mtk_is_pmc_via_fastauto(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return (host->caps & UFS_MTK_CAP_PMC_VIA_FASTAUTO);
}

static bool ufs_mtk_is_clk_scale_ready(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_mtk_clk *mclk = &host->mclk;

	return mclk->ufs_sel_clki &&
		mclk->ufs_sel_max_clki &&
		mclk->ufs_sel_min_clki;
}

static bool ufs_mtk_is_force_vsx_lpm(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return !!(host->caps & UFS_MTK_CAP_FORCE_VSx_LPM);
}

static void ufs_mtk_cfg_unipro_cg(struct ufs_hba *hba, bool enable)
{
	u32 tmp;

	if (enable) {
		ufshcd_dme_get(hba,
			       UIC_ARG_MIB(VS_SAVEPOWERCONTROL), &tmp);
		tmp = tmp |
		      (1 << RX_SYMBOL_CLK_GATE_EN) |
		      (1 << SYS_CLK_GATE_EN) |
		      (1 << TX_CLK_GATE_EN);
		ufshcd_dme_set(hba,
			       UIC_ARG_MIB(VS_SAVEPOWERCONTROL), tmp);

		ufshcd_dme_get(hba,
			       UIC_ARG_MIB(VS_DEBUGCLOCKENABLE), &tmp);
		tmp = tmp & ~(1 << TX_SYMBOL_CLK_REQ_FORCE);
		ufshcd_dme_set(hba,
			       UIC_ARG_MIB(VS_DEBUGCLOCKENABLE), tmp);
	} else {
		ufshcd_dme_get(hba,
			       UIC_ARG_MIB(VS_SAVEPOWERCONTROL), &tmp);
		tmp = tmp & ~((1 << RX_SYMBOL_CLK_GATE_EN) |
			      (1 << SYS_CLK_GATE_EN) |
			      (1 << TX_CLK_GATE_EN));
		ufshcd_dme_set(hba,
			       UIC_ARG_MIB(VS_SAVEPOWERCONTROL), tmp);

		ufshcd_dme_get(hba,
			       UIC_ARG_MIB(VS_DEBUGCLOCKENABLE), &tmp);
		tmp = tmp | (1 << TX_SYMBOL_CLK_REQ_FORCE);
		ufshcd_dme_set(hba,
			       UIC_ARG_MIB(VS_DEBUGCLOCKENABLE), tmp);
	}
}

static void ufs_mtk_crypto_enable(struct ufs_hba *hba)
{
	struct arm_smccc_res res;

	ufs_mtk_crypto_ctrl(res, 1);
	if (res.a0) {
		dev_info(hba->dev, "%s: crypto enable failed, err: %lu\n",
			 __func__, res.a0);
		hba->caps &= ~UFSHCD_CAP_CRYPTO;
	}
}

static void ufs_mtk_host_reset(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	reset_control_assert(host->hci_reset);
	reset_control_assert(host->crypto_reset);
	reset_control_assert(host->unipro_reset);

	usleep_range(100, 110);

	reset_control_deassert(host->unipro_reset);
	reset_control_deassert(host->crypto_reset);
	reset_control_deassert(host->hci_reset);
}

static void ufs_mtk_init_reset_control(struct ufs_hba *hba,
				       struct reset_control **rc,
				       char *str)
{
	*rc = devm_reset_control_get(hba->dev, str);
	if (IS_ERR(*rc)) {
		dev_info(hba->dev, "Failed to get reset control %s: %ld\n",
			 str, PTR_ERR(*rc));
		*rc = NULL;
	}
}

static void ufs_mtk_init_reset(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	ufs_mtk_init_reset_control(hba, &host->hci_reset,
				   "hci_rst");
	ufs_mtk_init_reset_control(hba, &host->unipro_reset,
				   "unipro_rst");
	ufs_mtk_init_reset_control(hba, &host->crypto_reset,
				   "crypto_rst");
}

static int ufs_mtk_hce_enable_notify(struct ufs_hba *hba,
				     enum ufs_notify_change_status status)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	unsigned long flags;

	if (status == PRE_CHANGE) {
		if (host->unipro_lpm) {
			hba->vps->hba_enable_delay_us = 0;
		} else {
			hba->vps->hba_enable_delay_us = 600;
			ufs_mtk_host_reset(hba);
		}

		if (hba->caps & UFSHCD_CAP_CRYPTO)
			ufs_mtk_crypto_enable(hba);

		if (host->caps & UFS_MTK_CAP_DISABLE_AH8) {
			spin_lock_irqsave(hba->host->host_lock, flags);
			ufshcd_writel(hba, 0,
				      REG_AUTO_HIBERNATE_IDLE_TIMER);
			spin_unlock_irqrestore(hba->host->host_lock,
					       flags);

			hba->capabilities &= ~MASK_AUTO_HIBERN8_SUPPORT;
			hba->ahit = 0;
		}

		ufshcd_writel(hba,
			(ufshcd_readl(hba, REG_UFS_XOUFS_CTRL) | 0x80),
			REG_UFS_XOUFS_CTRL);
	}

	return 0;
}

static int ufs_mtk_bind_mphy(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;
	int err = 0;

	host->mphy = devm_of_phy_get_by_index(dev, np, 0);

	if (host->mphy == ERR_PTR(-EPROBE_DEFER)) {
		/*
		 * UFS driver might be probed before the phy driver does.
		 * In that case we would like to return EPROBE_DEFER code.
		 */
		err = -EPROBE_DEFER;
		dev_info(dev,
			 "%s: required phy hasn't probed yet. err = %d\n",
			__func__, err);
	} else if (IS_ERR(host->mphy)) {
		err = PTR_ERR(host->mphy);
		if (err != -ENODEV) {
			dev_info(dev, "%s: PHY get failed %d\n", __func__,
				 err);
		}
	}

	if (err)
		host->mphy = NULL;
	/*
	 * Allow unbound mphy because not every platform needs specific
	 * mphy control.
	 */
	if (err == -ENODEV)
		err = 0;

	return err;
}

static int ufs_mtk_setup_ref_clk(struct ufs_hba *hba, bool on)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct arm_smccc_res res;
	ktime_t timeout, time_checked;
	u32 value;

	if (host->ref_clk_enabled == on)
		return 0;

	ufs_mtk_ref_clk_notify(on, PRE_CHANGE, res);

	if (on) {
		ufshcd_writel(hba, REFCLK_REQUEST, REG_UFS_REFCLK_CTRL);
	} else {
		ufshcd_delay_us(host->ref_clk_gating_wait_us, 10);
		ufshcd_writel(hba, REFCLK_RELEASE, REG_UFS_REFCLK_CTRL);
	}

	/* Wait for ack */
	timeout = ktime_add_us(ktime_get(), REFCLK_REQ_TIMEOUT_US);
	do {
		time_checked = ktime_get();
		value = ufshcd_readl(hba, REG_UFS_REFCLK_CTRL);

		/* Wait until ack bit equals to req bit */
		if (((value & REFCLK_ACK) >> 1) == (value & REFCLK_REQUEST))
			goto out;

		usleep_range(100, 200);
	} while (ktime_before(time_checked, timeout));

	dev_err(hba->dev, "missing ack of refclk req, reg: 0x%x\n", value);

	ufs_mtk_ref_clk_notify(host->ref_clk_enabled, POST_CHANGE, res);

	return -ETIMEDOUT;

out:
	host->ref_clk_enabled = on;
	if (on)
		ufshcd_delay_us(host->ref_clk_ungating_wait_us, 10);

	ufs_mtk_ref_clk_notify(on, POST_CHANGE, res);

	return 0;
}

static void ufs_mtk_setup_ref_clk_wait_us(struct ufs_hba *hba,
					  u16 gating_us, u16 ungating_us)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (hba->dev_info.clk_gating_wait_us) {
		host->ref_clk_gating_wait_us =
			hba->dev_info.clk_gating_wait_us;
	} else {
		host->ref_clk_gating_wait_us = gating_us;
	}

	host->ref_clk_ungating_wait_us = ungating_us;
}

static void ufs_mtk_dbg_sel(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (((host->ip_ver >> 16) & 0xFF) >= 0x36) {
		ufshcd_writel(hba, 0x820820, REG_UFS_DEBUG_SEL);
		ufshcd_writel(hba, 0x0, REG_UFS_DEBUG_SEL_B0);
		ufshcd_writel(hba, 0x55555555, REG_UFS_DEBUG_SEL_B1);
		ufshcd_writel(hba, 0xaaaaaaaa, REG_UFS_DEBUG_SEL_B2);
		ufshcd_writel(hba, 0xffffffff, REG_UFS_DEBUG_SEL_B3);
	} else {
		ufshcd_writel(hba, 0x20, REG_UFS_DEBUG_SEL);
	}
}

static void ufs_mtk_wait_idle_state(struct ufs_hba *hba,
			    unsigned long retry_ms)
{
	u64 timeout, time_checked;
	u32 val, sm;
	bool wait_idle;

	timeout = sched_clock() + retry_ms * 1000000UL;


	/* wait a specific time after check base */
	udelay(10);
	wait_idle = false;

	do {
		time_checked = sched_clock();
		ufs_mtk_dbg_sel(hba);
		val = ufshcd_readl(hba, REG_UFS_PROBE);

		sm = val & 0x1f;

		/*
		 * if state is in H8 enter and H8 enter confirm
		 * wait until return to idle state.
		 */
		if ((sm >= VS_HIB_ENTER) && (sm <= VS_HIB_EXIT)) {
			wait_idle = true;
			udelay(50);
			continue;
		} else if (!wait_idle)
			break;

		if (wait_idle && (sm == VS_HCE_BASE))
			break;
	} while (time_checked < timeout);

	if (wait_idle && sm != VS_HCE_BASE)
		dev_info(hba->dev, "wait idle tmo: 0x%x\n", val);
}

static int ufs_mtk_wait_link_state(struct ufs_hba *hba, u32 state,
				   unsigned long max_wait_ms)
{
	ktime_t timeout, time_checked;
	u32 val;

	timeout = ktime_add_ms(ktime_get(), max_wait_ms);
	do {
		time_checked = ktime_get();
		ufs_mtk_dbg_sel(hba);
		val = ufshcd_readl(hba, REG_UFS_PROBE);
		val = val >> 28;

		if (val == state)
			return 0;

		/* Sleep for max. 200us */
		usleep_range(100, 200);
	} while (ktime_before(time_checked, timeout));

	if (val == state)
		return 0;

	return -ETIMEDOUT;
}

static void ufs_mtk_pm_qos(struct ufs_hba *hba, bool qos_en)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (host && host->pm_qos_init) {
		if (qos_en)
			cpu_latency_qos_update_request(
				&host->pm_qos_req, 0);
		else
			cpu_latency_qos_update_request(
				&host->pm_qos_req,
				PM_QOS_DEFAULT_VALUE);
	}
}

static int ufs_mtk_mphy_power_on(struct ufs_hba *hba, bool on)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct phy *mphy = host->mphy;
	struct arm_smccc_res res;
	int ret = 0;

	if (!mphy || !(on ^ host->mphy_powered_on))
		return 0;

	if (on) {
		if (ufs_mtk_is_va09_supported(hba)) {
			ret = regulator_enable(host->reg_va09);
			if (ret < 0)
				goto out;
			/* wait 200 us to stablize VA09 */
			usleep_range(200, 210);
			ufs_mtk_va09_pwr_ctrl(res, 1);
		}
		phy_power_on(mphy);
	} else {
		phy_power_off(mphy);
		if (ufs_mtk_is_va09_supported(hba)) {
			ufs_mtk_va09_pwr_ctrl(res, 0);
			ret = regulator_disable(host->reg_va09);
			if (ret < 0)
				goto out;
		}
	}
out:
	if (ret) {
		dev_info(hba->dev,
			 "failed to %s va09: %d\n",
			 on ? "enable" : "disable",
			 ret);
	} else {
		host->mphy_powered_on = on;
	}

	return ret;
}

static int ufs_mtk_get_host_clk(struct device *dev, const char *name,
				struct clk **clk_out)
{
	struct clk *clk;
	int err = 0;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk))
		err = PTR_ERR(clk);
	else
		*clk_out = clk;

	return err;
}

static void ufs_mtk_boost_crypt(struct ufs_hba *hba, bool boost)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_mtk_crypt_cfg *cfg;
	struct regulator *reg;
	int volt, ret;

	if (!ufs_mtk_is_boost_crypt_enabled(hba))
		return;

	cfg = host->crypt;
	volt = cfg->vcore_volt;
	reg = cfg->reg_vcore;

	ret = clk_prepare_enable(cfg->clk_crypt_mux);
	if (ret) {
		dev_info(hba->dev, "clk_prepare_enable(): %d\n",
			 ret);
		return;
	}

	if (boost) {
		ret = regulator_set_voltage(reg, volt, INT_MAX);
		if (ret) {
			dev_info(hba->dev,
				 "failed to set vcore to %d\n", volt);
			goto out;
		}

		ret = clk_set_parent(cfg->clk_crypt_mux,
				     cfg->clk_crypt_perf);
		if (ret) {
			dev_info(hba->dev,
				 "failed to set clk_crypt_perf\n");
			regulator_set_voltage(reg, 0, INT_MAX);
			goto out;
		}
	} else {
		ret = clk_set_parent(cfg->clk_crypt_mux,
				     cfg->clk_crypt_lp);
		if (ret) {
			dev_info(hba->dev,
				 "failed to set clk_crypt_lp\n");
			goto out;
		}

		ret = regulator_set_voltage(reg, 0, INT_MAX);
		if (ret) {
			dev_info(hba->dev,
				 "failed to set vcore to MIN\n");
		}
	}
out:
	clk_disable_unprepare(cfg->clk_crypt_mux);
}

static int ufs_mtk_init_host_clk(struct ufs_hba *hba, const char *name,
				 struct clk **clk)
{
	int ret;

	ret = ufs_mtk_get_host_clk(hba->dev, name, clk);
	if (ret) {
		dev_info(hba->dev, "%s: failed to get %s: %d", __func__,
			 name, ret);
	}

	return ret;
}

static void ufs_mtk_init_boost_crypt(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_mtk_crypt_cfg *cfg;
	struct device *dev = hba->dev;
	struct regulator *reg;
	u32 volt;

	host->crypt = devm_kzalloc(dev, sizeof(*(host->crypt)),
				   GFP_KERNEL);
	if (!host->crypt)
		goto disable_caps;

	reg = devm_regulator_get_optional(dev, "dvfsrc-vcore");
	if (IS_ERR(reg)) {
		dev_info(dev, "failed to get dvfsrc-vcore: %ld",
			 PTR_ERR(reg));
		goto disable_caps;
	}

	if (of_property_read_u32(dev->of_node, "boost-crypt-vcore-min",
				 &volt)) {
		dev_info(dev, "failed to get boost-crypt-vcore-min");
		goto disable_caps;
	}

	cfg = host->crypt;
	if (ufs_mtk_init_host_clk(hba, "crypt_mux",
				  &cfg->clk_crypt_mux))
		goto disable_caps;

	if (ufs_mtk_init_host_clk(hba, "crypt_lp",
				  &cfg->clk_crypt_lp))
		goto disable_caps;

	if (ufs_mtk_init_host_clk(hba, "crypt_perf",
				  &cfg->clk_crypt_perf))
		goto disable_caps;

	cfg->reg_vcore = reg;
	cfg->vcore_volt = volt;
	host->caps |= UFS_MTK_CAP_BOOST_CRYPT_ENGINE;

disable_caps:
	return;
}

static void ufs_mtk_init_va09_pwr_ctrl(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	host->reg_va09 = regulator_get(hba->dev, "va09");
	if (IS_ERR(host->reg_va09))
		dev_info(hba->dev, "failed to get va09");
	else
		host->caps |= UFS_MTK_CAP_VA09_PWR_CTRL;
}

static void ufs_mtk_init_host_caps(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct device_node *np = hba->dev->of_node;
	struct tag_bootmode *tag = NULL;

	if (of_property_read_bool(np, "mediatek,ufs-boost-crypt"))
		ufs_mtk_init_boost_crypt(hba);

	if (of_property_read_bool(np, "mediatek,ufs-support-va09"))
		ufs_mtk_init_va09_pwr_ctrl(hba);

	if (of_property_read_bool(np, "mediatek,ufs-disable-ah8"))
		host->caps |= UFS_MTK_CAP_DISABLE_AH8;

	if (of_property_read_bool(np, "mediatek,ufs-qos"))
		host->qos_enabled = host->qos_allowed = true;

	if (of_property_read_bool(np, "mediatek,ufs-broken-vcc"))
		host->caps |= UFS_MTK_CAP_BROKEN_VCC;

	if (of_property_read_bool(np, "mediatek,ufs-pmc-via-fastauto"))
		host->caps |= UFS_MTK_CAP_PMC_VIA_FASTAUTO;

	dev_info(hba->dev, "caps=0x%x", host->caps);

	/* Get boot type from bootmode */
	tag = (struct tag_bootmode *)ufs_mtk_get_boot_property(np,
								"atag,boot", NULL);
	if (!tag)
		dev_info(hba->dev, "failed to get atag,boot\n");
	else if (tag->boottype == BOOTDEV_UFS)
		host->boot_device = true;

}

/**
 * ufs_mtk_setup_clocks - enables/disable clocks
 * @hba: host controller instance
 * @on: If true, enable clocks else disable them.
 * @status: PRE_CHANGE or POST_CHANGE notify
 *
 * Returns 0 on success, non-zero on failure.
 */
static int ufs_mtk_setup_clocks(struct ufs_hba *hba, bool on,
				enum ufs_notify_change_status status)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	bool clk_pwr_off = false;
	int ret = 0;

	/*
	 * In case ufs_mtk_init() is not yet done, simply ignore.
	 * This ufs_mtk_setup_clocks() shall be called from
	 * ufs_mtk_init() after init is done.
	 */
	if (!host)
		return 0;

	if (!on && status == PRE_CHANGE) {
		if (ufshcd_is_link_off(hba)) {
			clk_pwr_off = true;
		} else if (ufshcd_is_link_hibern8(hba) ||
			 (!ufshcd_can_hibern8_during_gating(hba) &&
			 ufshcd_is_auto_hibern8_enabled(hba))) {
			/*
			 * Gate ref-clk and poweroff mphy if link state is in
			 * OFF or Hibern8 by either Auto-Hibern8 or
			 * ufshcd_link_state_transition().
			 */
			ret = ufs_mtk_wait_link_state(hba,
						      VS_LINK_HIBERN8,
						      15);
			if (!ret)
				clk_pwr_off = true;
		}

		if (clk_pwr_off) {
			if (!ufshcd_is_clkscaling_supported(hba))
				ufs_mtk_pm_qos(hba, on);
			ufs_mtk_boost_crypt(hba, on);
			ufs_mtk_setup_ref_clk(hba, on);
			phy_power_off(host->mphy);
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
			if (host->qos_enabled)
				ufs_mtk_biolog_clk_gating(on);
#endif
		}
	} else if (on && status == POST_CHANGE) {
		phy_power_on(host->mphy);
		ufs_mtk_setup_ref_clk(hba, on);
		ufs_mtk_boost_crypt(hba, on);
		if (!ufshcd_is_clkscaling_supported(hba))
			ufs_mtk_pm_qos(hba, on);
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
		if (host->qos_enabled)
			ufs_mtk_biolog_clk_gating(on);
#endif
	}

	return ret;
}

static inline bool ufs_mtk_is_data_cmd(struct scsi_cmnd *cmd)
{
	char cmd_op = cmd->cmnd[0];

	if (cmd_op == WRITE_10 || cmd_op == READ_10 ||
	    cmd_op == WRITE_16 || cmd_op == READ_16 ||
	    cmd_op == WRITE_6 || cmd_op == READ_6)
		return true;

	return false;
}

#define UFS_VEND_SAMSUNG  (1 << 0)

struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool init;
	unsigned int vend;
};

#if defined(CONFIG_UFSFEATURE)
static void ufs_mtk_trace_vh_prepare_command_vend_ss(void *data,
				struct ufs_hba *hba, struct request *rq,
				struct ufshcd_lrb *lrbp, int *err)
{
	ufsf_change_lun(ufs_mtk_get_ufsf(hba), lrbp);
	*err = ufsf_prep_fn(ufs_mtk_get_ufsf(hba), lrbp);
}

static void ufs_mtk_trace_vh_compl_command_vend_ss(struct ufs_hba *hba,
				struct ufshcd_lrb *lrbp,
				unsigned long out_reqs,
				unsigned long out_tasks)
{
	struct scsi_cmnd *cmd = lrbp->cmd;
	struct utp_upiu_header *header = &lrbp->ucd_rsp_ptr->header;
	int result = 0;
	int scsi_status;
	int ocs;
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);

	if (!cmd)
		return;

#if defined(CONFIG_UFSSHPB) && defined(CONFIG_HPB_DEBUG)
	trace_printk("%llu + %u cmd 0x%X comp tag[%d] out %lX\n",
		     (unsigned long long) blk_rq_pos(cmd->request),
		     (unsigned int) blk_rq_sectors(cmd->request),
		     cmd->cmnd[0], lrbp->task_tag, hba->outstanding_reqs);
#endif
	ocs = le32_to_cpu(header->dword_2) & MASK_OCS;

	if (ocs == OCS_SUCCESS) {
		result = be32_to_cpu(header->dword_0) >> 24;
		if (result == UPIU_TRANSACTION_RESPONSE) {
			scsi_status = be32_to_cpu(header->dword_1) &
				MASK_SCSI_STATUS;
			if (scsi_status == SAM_STAT_GOOD) {
				ufsf_hpb_noti_rb(ufsf, lrbp);
				if (ufsf_upiu_check_for_ccd(lrbp)) {
					ufsf_copy_sense_data(lrbp);
					/*
					 * this applies to "GOOD" case
					 * in scsi_decide_disposition()
					 * and will pass normally.
					 * if result is 0x00, sense will not
					 * be able to copy in sg_scsi_ioctl()
					 * Finally, ioctl tool in userspace will
					 * receive the error as 1.
					 */
					result |= GOOD_CCD;
				}
			}
		}
	}
#if defined(CONFIG_UFSHID)
	/* Check if it is the last request to be completed */
	if (!out_tasks && !out_reqs)
		schedule_work(&ufsf->on_idle_work);
#endif
}

static void ufs_mtk_trace_vh_send_tm_command_vend_ss(void *data, struct ufs_hba *hba,
			int tag, const char *str)
{
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);

	if (strcmp(str, "tm_complete") == 0)
		ufsf_reset_lu(ufsf);

}

static void ufs_mtk_trace_vh_update_sdev_vend_ss(void *data, struct scsi_device *sdev)
{
	struct ufs_hba *hba = shost_priv(sdev->host);
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);

	if (hba->dev_info.wmanufacturerid == UFS_VENDOR_SAMSUNG)
		ufsf_slave_configure(ufsf, sdev);
}

static void ufs_mtk_trace_vh_send_command_vend_ss(void *data, struct ufs_hba *hba,
				struct ufshcd_lrb *lrbp)
{
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);

	ufsf_hid_acc_io_stat(ufsf, lrbp);
}
#endif

static void ufs_mtk_trace_vh_send_command(void *data, struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct scsi_cmnd *cmd = lrbp->cmd;

	if (!cmd)
		return;

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
	if (ufs_mtk_is_data_cmd(cmd)) {
		ufs_mtk_biolog_send_command(lrbp->task_tag, cmd);
		ufs_mtk_biolog_check(1);
	}
#endif
}

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER) || defined(CONFIG_UFSFEATURE)
static void ufs_mtk_get_outstanding_reqs(struct ufs_hba *hba,
				unsigned long **outstanding_reqs, int *nr_tag)
{
	struct ufs_hba_private *hba_priv =
			(struct ufs_hba_private *)hba->android_vendor_data1;

	if (hba_priv->is_mcq_enabled) {
		*outstanding_reqs = hba_priv->outstanding_mcq_reqs;
		*nr_tag = UFSHCD_MAX_TAG;
	} else {
		*outstanding_reqs = &hba->outstanding_reqs;
		*nr_tag = hba->nutrs;
	}
}
#endif

static void ufs_mtk_trace_vh_compl_command(void *data, struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct scsi_cmnd *cmd = lrbp->cmd;
	unsigned long *outstanding_reqs;
#if defined(CONFIG_UFSFEATURE)
	unsigned long outstanding_tasks;
	struct ufsf_feature *ufsf;
#endif

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER) || defined(CONFIG_UFSFEATURE)
	unsigned long ongoing_cnt = 0;
	int tmp_tag, nr_tag;
#endif

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
	int tag = lrbp->task_tag;
#endif

	if (!cmd)
		return;

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER) || defined(CONFIG_UFSFEATURE)
	ufs_mtk_get_outstanding_reqs(hba, &outstanding_reqs, &nr_tag);
	for_each_set_bit(tmp_tag, outstanding_reqs, nr_tag) {
		ongoing_cnt = 1;
		break;
	}
#endif

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
	if (ufs_mtk_is_data_cmd(cmd)) {
		ufs_mtk_biolog_transfer_req_compl(tag, ongoing_cnt);
		ufs_mtk_biolog_check(ongoing_cnt);
	}
#endif

#if defined(CONFIG_UFSFEATURE)
	ufsf = ufs_mtk_get_ufsf(hba);

	outstanding_tasks = hba->outstanding_tasks;

	if (ufsf->hba)
		ufs_mtk_trace_vh_compl_command_vend_ss(hba, lrbp,
			ongoing_cnt, outstanding_tasks);
#endif
}

static void ufs_mtk_trace_vh_update_sdev(void *data, struct scsi_device *sdev)
{
	struct ufs_hba *hba = shost_priv(sdev->host);
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	sdev->broken_fua = 1;

	dev_dbg(hba->dev, "lu %d slave configured", sdev->lun);

	if (hba->luns_avail == 1) {
		/* The last LUs */
		dev_info(hba->dev, "%s: LUNs ready");
		complete(&host->luns_added);
	}
}

void ufs_mtk_trace_vh_ufs_prepare_command(void *data, struct ufs_hba *hba, struct request *rq,
		 struct ufshcd_lrb *lrbp, int *err)
{
	struct scsi_cmnd *cmd = lrbp->cmd;
	char *cmnd = cmd->cmnd;
	if (cmnd[0] == WRITE_10 | cmnd[0] == WRITE_16)
		cmnd[1] &= ~0x08;
}

static struct tracepoints_table interests[] = {
	{
		.name = "android_vh_ufs_prepare_command",
		.func = ufs_mtk_trace_vh_ufs_prepare_command
	},
	{
		.name = "android_vh_ufs_send_command",
		.func = ufs_mtk_trace_vh_send_command
	},
	{
		.name = "android_vh_ufs_compl_command",
		.func = ufs_mtk_trace_vh_compl_command
	},
	{
		.name = "android_vh_ufs_update_sdev",
		.func = ufs_mtk_trace_vh_update_sdev
	},
#if defined(CONFIG_UFSFEATURE)
	{
		.name = "android_vh_ufs_prepare_command",
		.func = ufs_mtk_trace_vh_prepare_command_vend_ss,
		.vend = UFS_VEND_SAMSUNG
	},
	{
		.name = "android_vh_ufs_send_tm_command",
		.func = ufs_mtk_trace_vh_send_tm_command_vend_ss,
		.vend = UFS_VEND_SAMSUNG
	},
	{
		.name = "android_vh_ufs_update_sdev",
		.func = ufs_mtk_trace_vh_update_sdev_vend_ss,
		.vend = UFS_VEND_SAMSUNG
	},
	{
		.name = "android_vh_ufs_send_command",
		.func = ufs_mtk_trace_vh_send_command_vend_ss,
		.vend = UFS_VEND_SAMSUNG
	},
#endif
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(interests) / sizeof(struct tracepoints_table); \
	i++)

static void ufs_mtk_lookup_tracepoints(struct tracepoint *tp,
				       void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(interests[i].name, tp->name) == 0)
			interests[i].tp = tp;
	}
}

static void ufs_mtk_uninstall_tracepoints(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (interests[i].init) {
			tracepoint_probe_unregister(interests[i].tp,
						    interests[i].func,
						    NULL);
		}
	}
}

static int ufs_mtk_install_tracepoints(struct ufs_hba *hba)
{
	int i;
	unsigned int vend;

	/* Install the tracepoints */
	for_each_kernel_tracepoint(ufs_mtk_lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (interests[i].tp == NULL) {
			dev_info(hba->dev, "Error: tracepoint %s not found\n",
				interests[i].name);
			continue;
		}

		vend = interests[i].vend;
		if (vend & UFS_VEND_SAMSUNG) {
			if (hba->dev_info.wmanufacturerid != UFS_VENDOR_SAMSUNG)
				continue;
		}

		tracepoint_probe_register(interests[i].tp,
					  interests[i].func,
					  NULL);
		interests[i].init = true;
	}

	return 0;
}

static void ufs_mtk_get_controller_version(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	int ret, ver = 0;

	if (host->hw_ver.major)
		return;

	/* Set default (minimum) version anyway */
	host->hw_ver.major = 2;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_LOCALVERINFO), &ver);
	if (!ret) {
		if (ver >= UFS_UNIPRO_VER_1_8) {
			host->hw_ver.major = 3;
			/*
			 * Fix HCI version for some platforms with
			 * incorrect version
			 */
			if (hba->ufs_version < ufshci_version(3, 0))
				hba->ufs_version = ufshci_version(3, 0);
		}
	}
}

static u32 ufs_mtk_get_ufs_hci_version(struct ufs_hba *hba)
{
	return hba->ufs_version;
}


#if IS_ENABLED(CONFIG_RPMB)

#define SEC_PROTOCOL_UFS  0xEC
#define SEC_SPECIFIC_UFS_RPMB 0x0001

#define SEC_PROTOCOL_CMD_SIZE 12
#define SEC_PROTOCOL_RETRIES 3
#define SEC_PROTOCOL_RETRIES_ON_RESET 10
#define SEC_PROTOCOL_TIMEOUT msecs_to_jiffies(30000)

int ufs_mtk_rpmb_security_out(struct scsi_device *sdev,
			 struct rpmb_frame *frames, u32 cnt, u8 region)
{
	struct scsi_sense_hdr sshdr = {0};
	u32 trans_len = cnt * sizeof(struct rpmb_frame);
	int reset_retries = SEC_PROTOCOL_RETRIES_ON_RESET;
	int ret;
	u8 cmd[SEC_PROTOCOL_CMD_SIZE];

	memset(cmd, 0, SEC_PROTOCOL_CMD_SIZE);
	cmd[0] = SECURITY_PROTOCOL_OUT;
	cmd[1] = SEC_PROTOCOL_UFS;
	put_unaligned_be16((region << 8) | SEC_SPECIFIC_UFS_RPMB, cmd + 2);
	cmd[4] = 0;                              /* inc_512 bit 7 set to 0 */
	put_unaligned_be32(trans_len, cmd + 6);  /* transfer length */

retry:
	ret = scsi_execute_req(sdev, cmd, DMA_TO_DEVICE,
				     frames, trans_len, &sshdr,
				     SEC_PROTOCOL_TIMEOUT, SEC_PROTOCOL_RETRIES,
				     NULL);

	if (ret && scsi_sense_valid(&sshdr) &&
	    sshdr.sense_key == UNIT_ATTENTION)
		/*
		 * Device reset might occur several times,
		 * give it one more chance
		 */
		if (--reset_retries > 0)
			goto retry;

	if (ret)
		dev_err(&sdev->sdev_gendev, "%s: failed with err %0x\n",
			__func__, ret);

	if (scsi_sense_valid(&sshdr) && sshdr.sense_key)
		scsi_print_sense_hdr(sdev, "rpmb: security out", &sshdr);

	return ret;
}

int ufs_mtk_rpmb_security_in(struct scsi_device *sdev,
			struct rpmb_frame *frames, u32 cnt, u8 region)
{
	struct scsi_sense_hdr sshdr = {0};
	u32 alloc_len = cnt * sizeof(struct rpmb_frame);
	int reset_retries = SEC_PROTOCOL_RETRIES_ON_RESET;
	int ret;
	u8 cmd[SEC_PROTOCOL_CMD_SIZE];

	memset(cmd, 0, SEC_PROTOCOL_CMD_SIZE);
	cmd[0] = SECURITY_PROTOCOL_IN;
	cmd[1] = SEC_PROTOCOL_UFS;
	put_unaligned_be16((region << 8) | SEC_SPECIFIC_UFS_RPMB, cmd + 2);
	cmd[4] = 0;                             /* inc_512 bit 7 set to 0 */
	put_unaligned_be32(alloc_len, cmd + 6); /* allocation length */

retry:
	ret = scsi_execute_req(sdev, cmd, DMA_FROM_DEVICE,
				     frames, alloc_len, &sshdr,
				     SEC_PROTOCOL_TIMEOUT, SEC_PROTOCOL_RETRIES,
				     NULL);

	if (ret && scsi_sense_valid(&sshdr) &&
	    sshdr.sense_key == UNIT_ATTENTION)
		/*
		 * Device reset might occur several times,
		 * give it one more chance
		 */
		if (--reset_retries > 0)
			goto retry;

	if (ret)
		dev_err(&sdev->sdev_gendev, "%s: failed with err %0x\n",
			__func__, ret);

	if (scsi_sense_valid(&sshdr) && sshdr.sense_key)
		scsi_print_sense_hdr(sdev, "rpmb: security in", &sshdr);

	return ret;
}

static int ufs_mtk_rpmb_cmd_seq(struct device *dev,
			       struct rpmb_cmd *cmds, u32 ncmds, u8 region)
{
	unsigned long flags;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host;
	struct scsi_device *sdev;
	struct rpmb_cmd *cmd;
	int i;
	int ret;

	host = ufshcd_get_variant(hba);

	spin_lock_irqsave(hba->host->host_lock, flags);
	sdev = hba->sdev_rpmb;
	if (sdev) {
		ret = scsi_device_get(sdev);
		if (!ret && !scsi_device_online(sdev)) {
			ret = -ENODEV;
			scsi_device_put(sdev);
		}
	} else {
		ret = -ENODEV;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	if (ret)
		return ret;

	/*
	 * Send all command one by one.
	 * Use rpmb lock to prevent other rpmb read/write threads cut in line.
	 * Use mutex not spin lock because in/out function might sleep.
	 */
	down(&host->rpmb_sem);
	/* Ensure device is resumed before RPMB operation */
	scsi_autopm_get_device(sdev);

	for (ret = 0, i = 0; i < ncmds && !ret; i++) {
		cmd = &cmds[i];
		if (cmd->flags & RPMB_F_WRITE)
			ret = ufs_mtk_rpmb_security_out(sdev, cmd->frames,
						       cmd->nframes, region);
		else
			ret = ufs_mtk_rpmb_security_in(sdev, cmd->frames,
						      cmd->nframes, region);
	}

	/* Allow device to be runtime suspended */
	scsi_autopm_put_device(sdev);
	up(&host->rpmb_sem);

	scsi_device_put(sdev);
	return ret;
}

static struct rpmb_ops ufs_mtk_rpmb_dev_ops = {
	.cmd_seq = ufs_mtk_rpmb_cmd_seq,
	.type = RPMB_TYPE_UFS,
};
static struct rpmb_dev *rawdev_ufs_rpmb;

/**
 * ufs_mtk_rpmb_ddd - add mtk rpmb cdev
 * @data: host controller instance (hba)
 *
 * Read max ufs device read/write rpmb size support and
 * set to reliable_wr_cnt for rpmb cdev read/write reference.
 *
 * Register raw cdve device in rawdev_ufs_rpmb
 */
static void ufs_mtk_rpmb_add(void *data, async_cookie_t cookie)
{
	int err;
	u8 *desc_buf;
	struct rpmb_dev *rdev;
	u8 rw_size;
	struct ufs_mtk_host *host;
	struct ufs_hba *hba = (struct ufs_hba *)data;

	host = ufshcd_get_variant(hba);

	err = wait_for_completion_timeout(&host->luns_added, 10 * HZ);
	if (err == 0) {
		dev_warn(hba->dev, "%s: LUNs not ready before timeout. RPMB init failed");
		goto out;
	}

	desc_buf = kmalloc(QUERY_DESC_MAX_SIZE, GFP_KERNEL);
	if (!desc_buf)
		goto out;

	err = ufshcd_read_desc_param(hba, QUERY_DESC_IDN_GEOMETRY, 0, 0,
				     desc_buf, QUERY_DESC_MAX_SIZE);
	if (err) {
		dev_warn(hba->dev, "%s: cannot get rpmb rw limit %d\n",
			 dev_name(hba->dev), err);
		/* fallback to singel frame write */
		rw_size = 1;
	} else {
		rw_size = desc_buf[GEOMETRY_DESC_PARAM_RPMB_RW_SIZE];
	}

	kfree(desc_buf);
	dev_info(hba->dev, "rpmb rw_size: %d\n", rw_size);

	ufs_mtk_rpmb_dev_ops.reliable_wr_cnt = rw_size;

	if (unlikely(scsi_device_get(hba->sdev_rpmb)))
		goto out;

	rdev = rpmb_dev_register(hba->dev, &ufs_mtk_rpmb_dev_ops);
	if (IS_ERR(rdev)) {
		dev_warn(hba->dev, "%s: cannot register to rpmb %ld\n",
			 dev_name(hba->dev), PTR_ERR(rdev));
		goto out_put_dev;
	}

	/*
	 * Preserve rpmb_dev to globals for connection of legacy
	 * rpmb ioctl solution.
	 */
	rawdev_ufs_rpmb = rdev;

	/*
	 * Initialize rpmb semaphore.
	 */
	sema_init(&host->rpmb_sem, 1);

out_put_dev:
	scsi_device_put(hba->sdev_rpmb);

out:
	return;
}

struct rpmb_dev *ufs_mtk_rpmb_get_raw_dev(void)
{
	return rawdev_ufs_rpmb;
}
EXPORT_SYMBOL_GPL(ufs_mtk_rpmb_get_raw_dev);
#endif

/**
 * ufs_mtk_query_ioctl - perform user read queries
 * @hba: per-adapter instance
 * @lun: used for lun specific queries
 * @buffer: user space buffer for reading and submitting query data and params
 * @return: 0 for success negative error code otherwise
 *
 * Expected/Submitted buffer structure is struct ufs_ioctl_query_data.
 * It will read the opcode, idn and buf_length parameters, and, put the
 * response in the buffer field while updating the used size in buf_length.
 */
static int
ufs_mtk_query_ioctl(struct ufs_hba *hba, u8 lun, void __user *buffer)
{
	struct ufs_ioctl_query_data *ioctl_data;
	int err = 0;
	int length = 0;
	void *data_ptr;
	bool flag;
	u32 att;
	u8 index = 0;
	u8 *desc = NULL;

	ioctl_data = kzalloc(sizeof(*ioctl_data), GFP_KERNEL);
	if (!ioctl_data) {
		err = -ENOMEM;
		goto out;
	}

	/* extract params from user buffer */
	err = copy_from_user(ioctl_data, buffer,
			     sizeof(struct ufs_ioctl_query_data));
	if (err) {
		dev_err(hba->dev,
			"%s: Failed copying buffer from user, err %d\n",
			__func__, err);
		goto out_release_mem;
	}

#if defined(CONFIG_UFSFEATURE)
	if (ufsf_check_query(ioctl_data->opcode)) {
		err = ufsf_query_ioctl(ufs_mtk_get_ufsf(hba), lun, buffer,
				       ioctl_data, UFSFEATURE_SELECTOR);
		goto out_release_mem;
	}
#endif

	/* verify legal parameters & send query */
	switch (ioctl_data->opcode) {
	case UPIU_QUERY_OPCODE_READ_DESC:
		switch (ioctl_data->idn) {
		case QUERY_DESC_IDN_DEVICE:
		case QUERY_DESC_IDN_CONFIGURATION:
		case QUERY_DESC_IDN_INTERCONNECT:
		case QUERY_DESC_IDN_GEOMETRY:
		case QUERY_DESC_IDN_POWER:
			index = 0;
			break;
		case QUERY_DESC_IDN_UNIT:
			if (!ufs_is_valid_unit_desc_lun(&hba->dev_info, lun, 0)) {
				dev_err(hba->dev,
					"%s: No unit descriptor for lun 0x%x\n",
					__func__, lun);
				err = -EINVAL;
				goto out_release_mem;
			}
			index = lun;
			break;
		default:
			goto out_einval;
		}
		length = min_t(int, QUERY_DESC_MAX_SIZE,
			       ioctl_data->buf_size);
		desc = kzalloc(length, GFP_KERNEL);
		if (!desc) {
			dev_err(hba->dev, "%s: Failed allocating %d bytes\n",
				__func__, length);
			err = -ENOMEM;
			goto out_release_mem;
		}
		err = ufshcd_query_descriptor_retry(hba, ioctl_data->opcode,
						    ioctl_data->idn, index, 0,
						    desc, &length);
		break;
	case UPIU_QUERY_OPCODE_READ_ATTR:
		switch (ioctl_data->idn) {
		case QUERY_ATTR_IDN_BOOT_LU_EN:
		case QUERY_ATTR_IDN_POWER_MODE:
		case QUERY_ATTR_IDN_ACTIVE_ICC_LVL:
		case QUERY_ATTR_IDN_OOO_DATA_EN:
		case QUERY_ATTR_IDN_BKOPS_STATUS:
		case QUERY_ATTR_IDN_PURGE_STATUS:
		case QUERY_ATTR_IDN_MAX_DATA_IN:
		case QUERY_ATTR_IDN_MAX_DATA_OUT:
		case QUERY_ATTR_IDN_REF_CLK_FREQ:
		case QUERY_ATTR_IDN_CONF_DESC_LOCK:
		case QUERY_ATTR_IDN_MAX_NUM_OF_RTT:
		case QUERY_ATTR_IDN_EE_CONTROL:
		case QUERY_ATTR_IDN_EE_STATUS:
		case QUERY_ATTR_IDN_SECONDS_PASSED:
			index = 0;
			break;
		case QUERY_ATTR_IDN_DYN_CAP_NEEDED:
		case QUERY_ATTR_IDN_CORR_PRG_BLK_NUM:
			index = lun;
			break;
		default:
			goto out_einval;
		}
		err = ufshcd_query_attr(hba, ioctl_data->opcode,
					ioctl_data->idn, index, 0, &att);
		break;

	case UPIU_QUERY_OPCODE_WRITE_ATTR:
		err = copy_from_user(&att,
				     buffer +
				     sizeof(struct ufs_ioctl_query_data),
				     sizeof(u32));
		if (err) {
			dev_err(hba->dev,
				"%s: Failed copying buffer from user, err %d\n",
				__func__, err);
			goto out_release_mem;
		}

		switch (ioctl_data->idn) {
		case QUERY_ATTR_IDN_BOOT_LU_EN:
			index = 0;
			if (!att) {
				dev_err(hba->dev,
					"%s: Illegal ufs query ioctl data, opcode 0x%x, idn 0x%x, att 0x%x\n",
					__func__, ioctl_data->opcode,
					(unsigned int)ioctl_data->idn, att);
				err = -EINVAL;
				goto out_release_mem;
			}
			break;
		default:
			goto out_einval;
		}
		err = ufshcd_query_attr(hba, ioctl_data->opcode,
					ioctl_data->idn, index, 0, &att);
		break;

	case UPIU_QUERY_OPCODE_READ_FLAG:
		switch (ioctl_data->idn) {
		case QUERY_FLAG_IDN_FDEVICEINIT:
		case QUERY_FLAG_IDN_PERMANENT_WPE:
		case QUERY_FLAG_IDN_PWR_ON_WPE:
		case QUERY_FLAG_IDN_BKOPS_EN:
		case QUERY_FLAG_IDN_PURGE_ENABLE:
		case QUERY_FLAG_IDN_FPHYRESOURCEREMOVAL:
		case QUERY_FLAG_IDN_BUSY_RTC:
			break;
		default:
			goto out_einval;
		}
		err = ufshcd_query_flag(hba, ioctl_data->opcode,
					ioctl_data->idn, 0, &flag);
		break;
	default:
		goto out_einval;
	}

	if (err) {
		dev_err(hba->dev, "%s: Query for idn %d failed\n", __func__,
			ioctl_data->idn);
		goto out_release_mem;
	}

	/*
	 * copy response data
	 * As we might end up reading less data than what is specified in
	 * "ioctl_data->buf_size". So we are updating "ioctl_data->
	 * buf_size" to what exactly we have read.
	 */
	switch (ioctl_data->opcode) {
	case UPIU_QUERY_OPCODE_READ_DESC:
		ioctl_data->buf_size = min_t(int, ioctl_data->buf_size, length);
		data_ptr = desc;
		break;
	case UPIU_QUERY_OPCODE_READ_ATTR:
		ioctl_data->buf_size = sizeof(u32);
		data_ptr = &att;
		break;
	case UPIU_QUERY_OPCODE_READ_FLAG:
		ioctl_data->buf_size = 1;
		data_ptr = &flag;
		break;
	case UPIU_QUERY_OPCODE_WRITE_ATTR:
		goto out_release_mem;
	default:
		goto out_einval;
	}

	/* copy to user */
	err = copy_to_user(buffer, ioctl_data,
			   sizeof(struct ufs_ioctl_query_data));
	if (err)
		dev_err(hba->dev, "%s: Failed copying back to user.\n",
			__func__);
	err = copy_to_user(buffer + sizeof(struct ufs_ioctl_query_data),
			   data_ptr, ioctl_data->buf_size);
	if (err)
		dev_err(hba->dev, "%s: err %d copying back to user.\n",
			__func__, err);
	goto out_release_mem;

out_einval:
	dev_err(hba->dev,
		"%s: illegal ufs query ioctl data, opcode 0x%x, idn 0x%x\n",
		__func__, ioctl_data->opcode, (unsigned int)ioctl_data->idn);
	err = -EINVAL;
out_release_mem:
	kfree(ioctl_data);
	kfree(desc);
out:
	return err;
}

/**
 * ufs_mtk_ioctl - ufs ioctl callback registered in scsi_host
 * @dev: scsi device required for per LUN queries
 * @cmd: command opcode
 * @buffer: user space buffer for transferring data
 *
 * Supported commands:
 * UFS_IOCTL_QUERY
 */
static int
ufs_mtk_ioctl(struct scsi_device *dev, unsigned int cmd, void __user *buffer)
{
	struct ufs_hba *hba = shost_priv(dev->host);
	int err = 0;

	BUG_ON(!hba);
	if (!buffer) {
		dev_err(hba->dev, "%s: User buffer is NULL!\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case UFS_IOCTL_QUERY:
		pm_runtime_get_sync(hba->dev);
		err = ufs_mtk_query_ioctl(hba,
					  ufshcd_scsi_to_upiu_lun(dev->lun),
					  buffer);
		pm_runtime_put_sync(hba->dev);
		break;
	default:
		err = -ENOIOCTLCMD;
		dev_dbg(hba->dev, "%s: Unsupported ioctl cmd %d\n", __func__,
			cmd);
		break;
	}

	return err;
}

/**
 * ufs_mtk_init_clocks - Init mtk driver private clocks
 *
 * @param hba: per adapter instance
 * Returns zero on success else failed.
 */
static int ufs_mtk_init_clocks(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_clk_info *clki, *clki_tmp;
	struct list_head *head = &hba->clk_list_head;
	/*
	 * Find private clocks and store in struct ufs_mtk_clk.
	 * Remove "ufs_sel_min_src" and "ufs_sel_min_src" from list to avoid
	 * being switched on/off in clock gating.
	 */
	list_for_each_entry_safe(clki, clki_tmp, head, list) {
		if (!strcmp(clki->name, "ufs_sel")) {
			/* clk scaling */
			dev_dbg(hba->dev, "ufs_sel found");
			host->mclk.ufs_sel_clki = clki;
		} else if (!strcmp(clki->name, "ufs_sel_max_src")) {
			/* clk scaling */
			host->mclk.ufs_sel_max_clki = clki;
			clk_disable_unprepare(clki->clk);
			list_del(&clki->list);
			dev_dbg(hba->dev, "ufs_sel_max_src found");
		} else if (!strcmp(clki->name, "ufs_sel_min_src")) {
			/* clk scaling */
			host->mclk.ufs_sel_min_clki = clki;
			clk_disable_unprepare(clki->clk);
			list_del(&clki->list);
			dev_dbg(hba->dev, "ufs_sel_min_clki found");
		}
	}

	list_for_each_entry(clki, head, list) {
		dev_info(hba->dev, "clk \"%s\" present", clki->name);
	}

	if (!ufs_mtk_is_clk_scale_ready(hba)) {
		hba->caps &= ~UFSHCD_CAP_CLK_SCALING;
		dev_info(hba->dev, "%s: Clk scaling not ready. Feature disabled.", __func__);
		return -1;
	}
	return 0;
}

static int _ufshcd_get_vreg(struct device *dev, struct ufs_vreg *vreg)
{
	int ret = 0;

	if (!vreg)
		goto out;

	vreg->reg = devm_regulator_get(dev, vreg->name);
	if (IS_ERR(vreg->reg)) {
		ret = PTR_ERR(vreg->reg);
		dev_info(dev, "%s: %s get failed, err=%d\n",
				__func__, vreg->name, ret);
	}
out:
	return ret;
}

#define MAX_VCC_NAME 30
static int _ufshcd_populate_vreg(struct device *dev, const char *name,
			  struct ufs_vreg **out_vreg)
{
	char prop_name[MAX_VCC_NAME];
	struct ufs_vreg *vreg = NULL;
	struct device_node *np = dev->of_node;

	if (!np) {
		dev_info(dev, "%s: non DT initialization\n", __func__);
		goto out;
	}

	snprintf(prop_name, MAX_VCC_NAME, "%s-supply", name);
	if (!of_parse_phandle(np, prop_name, 0)) {
		dev_info(dev, "%s: Unable to find %s regulator, assuming enabled\n",
				__func__, prop_name);
		goto out;
	}

	vreg = devm_kzalloc(dev, sizeof(*vreg), GFP_KERNEL);
	if (!vreg)
		return -ENOMEM;

	vreg->name = devm_kstrdup(dev, name, GFP_KERNEL);
	if (!vreg->name)
		return -ENOMEM;

	snprintf(prop_name, MAX_VCC_NAME, "%s-max-microamp", name);
	if (of_property_read_u32(np, prop_name, &vreg->max_uA)) {
		dev_info(dev, "%s: unable to find %s\n", __func__, prop_name);
		vreg->max_uA = 0;
	}
out:
	*out_vreg = vreg;
	return 0;
}

static int ufs_mtk_vreg_fix_vcc(struct ufs_hba *hba)
{
	struct ufs_vreg_info *info = &hba->vreg_info;
	struct device_node *np = hba->dev->of_node;
	struct device *dev = hba->dev;
	char vcc_name[MAX_VCC_NAME];
	struct arm_smccc_res res;
	int err, ver;

	if (info->vcc)
		return 0;

	if (of_property_read_bool(np, "mediatek,ufs-vcc-by-num")) {
		ufs_mtk_get_vcc_num(res);
		if (res.a1 > UFS_VCC_NONE && res.a1 < UFS_VCC_MAX)
			snprintf(vcc_name, MAX_VCC_NAME, "vcc-opt%u", (unsigned int) res.a1);
		else
			return -ENODEV;
	} else if (of_property_read_bool(np, "mediatek,ufs-vcc-by-ver")) {
		ver = (hba->dev_info.wspecversion & 0xF00) >> 8;
		snprintf(vcc_name, MAX_VCC_NAME, "vcc-ufs%u", ver);
	} else {
		return 0;
	}

	err = _ufshcd_populate_vreg(dev, vcc_name, &info->vcc);
	if (err)
		return err;

	err = _ufshcd_get_vreg(dev, info->vcc);
	if (err)
		return err;

	err = regulator_enable(info->vcc->reg);
	if (!err) {
		info->vcc->enabled = true;
		dev_info(dev, "%s: %s enabled\n", __func__, vcc_name);
	}

	return err;
}

static void ufs_mtk_vreg_fix_vccqx(struct ufs_hba *hba)
{
	struct ufs_vreg_info *info = &hba->vreg_info;
	struct ufs_vreg **vreg_on, **vreg_off;

	if (hba->dev_info.wspecversion >= 0x0300) {
		vreg_on = &info->vccq;
		vreg_off = &info->vccq2;
	} else {
		vreg_on = &info->vccq2;
		vreg_off = &info->vccq;
	}

	if (*vreg_on)
		(*vreg_on)->always_on = true;

	if (*vreg_off) {
		regulator_disable((*vreg_off)->reg);
		devm_kfree(hba->dev, (*vreg_off)->name);
		devm_kfree(hba->dev, *vreg_off);
		*vreg_off  = NULL;
	}
}

static void ufs_mtk_setup_clk_gating(struct ufs_hba *hba)
{
	unsigned long flags;
	u32 ah_ms = 10;
	u32 ah_scale, ah_timer;
	u32 scale_us[] = {1, 10, 100, 1000, 10000, 100000};

	if (ufshcd_is_clkgating_allowed(hba)) {
		if (ufshcd_is_auto_hibern8_supported(hba) && hba->ahit) {
			ah_scale = FIELD_GET(UFSHCI_AHIBERN8_SCALE_MASK,
					  hba->ahit);
			ah_timer = FIELD_GET(UFSHCI_AHIBERN8_TIMER_MASK,
					  hba->ahit);
			if (ah_scale <= 5)
				ah_ms = ah_timer * scale_us[ah_scale] / 1000;
		}

		spin_lock_irqsave(hba->host->host_lock, flags);
		hba->clk_gating.delay_ms = max(ah_ms, 10U);
		spin_unlock_irqrestore(hba->host->host_lock, flags);
	}
}

static void ufs_mtk_fix_ahit(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (ufshcd_is_auto_hibern8_supported(hba)) {
		switch (hba->dev_info.wmanufacturerid) {
		case UFS_VENDOR_SAMSUNG:
			/* configure auto-hibern8 timer to 3.5 ms */
			host->desired_ahit =
				FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, 35) |
				FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, 2);
			break;

		case UFS_VENDOR_MICRON:
			/* configure auto-hibern8 timer to 2 ms */
			host->desired_ahit =
				FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, 2) |
				FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, 3);
			break;

		default:
			/* configure auto-hibern8 timer to 500 us */
			host->desired_ahit =
				FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, 5) |
				FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, 2);
			break;
		}

		hba->ahit = host->desired_ahit;
	}

	ufs_mtk_setup_clk_gating(hba);
}

/**
 * ufs_mtk_init - find other essential mmio bases
 * @hba: host controller instance
 *
 * Binds PHY with controller and powers up PHY enabling clocks
 * and regulators.
 *
 * Returns -EPROBE_DEFER if binding fails, returns negative error
 * on phy power up failure and returns zero on success.
 */
static int ufs_mtk_init(struct ufs_hba *hba)
{
	const struct of_device_id *id;
	struct device *dev = hba->dev;
	struct ufs_mtk_host *host;
	int err = 0;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host) {
		err = -ENOMEM;
		dev_info(dev, "%s: no memory for mtk ufs host\n", __func__);
		goto out;
	}

	host->hba = hba;
	ufshcd_set_variant(hba, host);

	id = of_match_device(ufs_mtk_of_match, dev);
	if (!id) {
		err = -EINVAL;
		goto out;
	}

	/* Initialize host capability */
	ufs_mtk_init_host_caps(hba);

	/* MCQ init */
	err = ufs_mtk_mcq_alloc_priv(hba);
	if (err)
		goto out;

	ufs_mtk_mcq_host_dts(hba);
	ufs_mtk_mcq_request_irq(hba);
	err = ufs_mtk_mcq_memory_alloc(hba);
	if (err)
		goto out;

	err = ufs_mtk_bind_mphy(hba);
	if (err)
		goto out_variant_clear;

	ufs_mtk_init_reset(hba);

	/* Enable runtime autosuspend */
	hba->caps |= UFSHCD_CAP_RPM_AUTOSUSPEND;

	/* Enable clock-gating */
	hba->caps |= UFSHCD_CAP_CLK_GATING;

	/* Enable inline encryption */
	hba->caps |= UFSHCD_CAP_CRYPTO;

	/* Enable WriteBooster */
	hba->caps |= UFSHCD_CAP_WB_EN;

	/* enable clk scaling*/
	hba->caps |= UFSHCD_CAP_CLK_SCALING;
	host->clk_scale_up = true; /* default is max freq */

	hba->quirks |= UFSHCI_QUIRK_SKIP_MANUAL_WB_FLUSH_CTRL;
	hba->vps->wb_flush_threshold = UFS_WB_BUF_REMAIN_PERCENT(80);

	if (host->caps & UFS_MTK_CAP_DISABLE_AH8)
		hba->caps |= UFSHCD_CAP_HIBERN8_WITH_CLK_GATING;

	ufs_mtk_init_clocks(hba);

#if IS_ENABLED(CONFIG_SCSI_UFS_MEDIATEK_DBG)
	if (hba->caps & UFSHCD_CAP_CLK_SCALING)
		ufs_mtk_init_clk_scaling_sysfs(hba);
#endif

	/*
	 * ufshcd_vops_init() is invoked after
	 * ufshcd_setup_clock(true) in ufshcd_hba_init() thus
	 * phy clock setup is skipped.
	 *
	 * Enable phy power and clocks specifically here.
	 */
	ufs_mtk_mphy_power_on(hba, true);
	ufs_mtk_setup_clocks(hba, true, POST_CHANGE);

	host->ip_ver = ufshcd_readl(hba, REG_UFS_MTK_IP_VER);

	cpu_latency_qos_add_request(&host->pm_qos_req,
	     	   PM_QOS_DEFAULT_VALUE);
	host->pm_qos_init = true;

	init_completion(&host->luns_added);

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
	ufs_mtk_biolog_init(host->qos_allowed, host->boot_device);
#endif

#if IS_ENABLED(CONFIG_SCSI_UFS_MEDIATEK_DBG)
	ufs_mtk_dbg_register(hba);
#endif

#if IS_ENABLED(CONFIG_RPMB)
	async_schedule(ufs_mtk_rpmb_add, hba);
#endif

	/* Provide SCSI host ioctl API */
	hba->host->hostt->ioctl = (int (*)(struct scsi_device *, unsigned int,
				   void __user *))ufs_mtk_ioctl;
#ifdef CONFIG_COMPAT
	hba->host->hostt->compat_ioctl = (int (*)(struct scsi_device *,
					  unsigned int,
					  void __user *))ufs_mtk_ioctl;
#endif
	goto out;

out_variant_clear:
	ufshcd_set_variant(hba, NULL);
out:
	return err;
}

static bool ufs_mtk_pmc_via_fastauto(struct ufs_hba *hba,
	struct ufs_pa_layer_attr *dev_req_params)
{
	if (!ufs_mtk_is_pmc_via_fastauto(hba))
		return false;

	if (dev_req_params->hs_rate == hba->pwr_info.hs_rate)
		return false;

	if ((dev_req_params->pwr_tx != FAST_MODE) &&
		(dev_req_params->gear_tx < UFS_HS_G4))
		return false;

	if ((dev_req_params->pwr_rx != FAST_MODE) &&
		(dev_req_params->gear_rx < UFS_HS_G4))
		return false;

	return true;
}

static int ufs_mtk_pre_pwr_change(struct ufs_hba *hba,
				  struct ufs_pa_layer_attr *dev_max_params,
				  struct ufs_pa_layer_attr *dev_req_params)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_dev_params host_cap;
	int ret;

	ufshcd_init_pwr_dev_param(&host_cap);
	host_cap.hs_rx_gear = UFS_HS_G5;
	host_cap.hs_tx_gear = UFS_HS_G5;

	ret = ufshcd_get_pwr_dev_param(&host_cap,
				       dev_max_params,
				       dev_req_params);
	if (ret) {
		pr_info("%s: failed to determine capabilities\n",
			__func__);
	}

	if (ufs_mtk_pmc_via_fastauto(hba, dev_req_params)) {
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXTERMINATION), true);
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXGEAR), UFS_HS_G1);

		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_RXTERMINATION), true);
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_RXGEAR), UFS_HS_G1);

		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_ACTIVETXDATALANES),
			dev_req_params->lane_tx);
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_ACTIVERXDATALANES),
			dev_req_params->lane_rx);
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_HSSERIES),
			dev_req_params->hs_rate);

		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXHSADAPTTYPE),
			PA_NO_ADAPT);

		ret = ufshcd_uic_change_pwr_mode(hba,
			FASTAUTO_MODE << 4 | FASTAUTO_MODE);

		if (ret) {
			dev_err(hba->dev, "%s: HSG1B FASTAUTO failed ret=%d\n",
				__func__, ret);
		}
	}

	/* if already configured to the requested pwr_mode, skip adapt */
	if (dev_req_params->gear_rx == hba->pwr_info.gear_rx &&
	    dev_req_params->gear_tx == hba->pwr_info.gear_tx &&
	    dev_req_params->lane_rx == hba->pwr_info.lane_rx &&
	    dev_req_params->lane_tx == hba->pwr_info.lane_tx &&
	    dev_req_params->pwr_rx == hba->pwr_info.pwr_rx &&
	    dev_req_params->pwr_tx == hba->pwr_info.pwr_tx &&
	    dev_req_params->hs_rate == hba->pwr_info.hs_rate) {
		return ret;
	}

	if (host->hw_ver.major >= 3) {
		ret = ufshcd_dme_configure_adapt(hba,
					   dev_req_params->gear_tx,
					   PA_INITIAL_ADAPT);
	} else {
		ret = ufshcd_dme_configure_adapt(hba,
			   dev_req_params->gear_tx,
			   PA_NO_ADAPT);
	}

	return ret;
}

static int ufs_mtk_pwr_change_notify(struct ufs_hba *hba,
				     enum ufs_notify_change_status stage,
				     struct ufs_pa_layer_attr *dev_max_params,
				     struct ufs_pa_layer_attr *dev_req_params)
{
	int ret = 0;

	switch (stage) {
	case PRE_CHANGE:
		ret = ufs_mtk_pre_pwr_change(hba, dev_max_params,
					     dev_req_params);
		break;
	case POST_CHANGE:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ufs_mtk_unipro_set_lpm(struct ufs_hba *hba, bool lpm)
{
	int ret;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	ret = ufshcd_dme_set(hba,
			     UIC_ARG_MIB_SEL(VS_UNIPROPOWERDOWNCONTROL, 0),
			     lpm ? 1 : 0);
	if (!ret || !lpm) {
		/*
		 * Forcibly set as non-LPM mode if UIC commands is failed
		 * to use default hba_enable_delay_us value for re-enabling
		 * the host.
		 */
		host->unipro_lpm = lpm;
	}

#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG)
	if (ret) {
		int ret2, val = 0;

		/* maybe irq pending */
		mt_irq_dump_status(hba->irq);

		ret2 = ufshcd_dme_get(hba,
			UIC_ARG_MIB(VS_UNIPROPOWERDOWNCONTROL), &val);
		if (!ret2) {
			dev_info(hba->dev, "%s: read 0xD0A8 val=%d\n",
				__func__, val);
		}

		aee_kernel_warning_api(__FILE__,
			__LINE__, DB_OPT_FS_IO_LOG | DB_OPT_FTRACE,
			"ufs", "set 0xd0a8 fail, ret=%d, ret2=%d, 0xd0a8=%d",
			ret, ret2, val);
	}
#endif

	return ret;
}

static int ufs_mtk_pre_link(struct ufs_hba *hba)
{
	int ret;
	u32 tmp;

	ufs_mtk_get_controller_version(hba);

	ret = ufs_mtk_unipro_set_lpm(hba, false);
	if (ret)
		return ret;

	/*
	 * Setting PA_Local_TX_LCC_Enable to 0 before link startup
	 * to make sure that both host and device TX LCC are disabled
	 * once link startup is completed.
	 */
	ret = ufshcd_disable_host_tx_lcc(hba);
	if (ret)
		return ret;

	/* disable deep stall */
	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(VS_SAVEPOWERCONTROL), &tmp);
	if (ret)
		return ret;

	tmp &= ~(1 << 6);

	ret = ufshcd_dme_set(hba, UIC_ARG_MIB(VS_SAVEPOWERCONTROL), tmp);

	return ret;
}

static int ufs_mtk_post_link(struct ufs_hba *hba)
{
	/* enable unipro clock gating feature */
	ufs_mtk_cfg_unipro_cg(hba, true);

	return 0;
}

static int ufs_mtk_link_startup_notify(struct ufs_hba *hba,
				       enum ufs_notify_change_status stage)
{
	int ret = 0;

	switch (stage) {
	case PRE_CHANGE:
		ret = ufs_mtk_pre_link(hba);
		break;
	case POST_CHANGE:
		ret = ufs_mtk_post_link(hba);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ufs_mtk_device_reset(struct ufs_hba *hba)
{
	struct arm_smccc_res res;

#if IS_ENABLED(CONFIG_UFSFEATURE)
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);

	if (ufsf->hba)
		ufsf_reset_host(ufs_mtk_get_ufsf(hba));
#endif

	/* disable hba before device reset */
	ufshcd_hba_stop(hba);

	ufs_mtk_device_reset_ctrl(0, res);

	/*
	 * The reset signal is active low. UFS devices shall detect
	 * more than or equal to 1us of positive or negative RST_n
	 * pulse width.
	 *
	 * To be on safe side, keep the reset low for at least 10us.
	 */
	usleep_range(10, 15);

	ufs_mtk_device_reset_ctrl(1, res);

	/* Some devices may need time to respond to rst_n */
	usleep_range(10000, 15000);

	dev_info(hba->dev, "device reset done\n");

	return 0;
}

static int ufs_mtk_link_set_hpm(struct ufs_hba *hba)
{
	int err;
	u32 val;

	err = ufshcd_hba_enable(hba);
	if (err)
		return err;

	err = ufs_mtk_unipro_set_lpm(hba, false);
	if (err) {
		ufs_mtk_dbg_sel(hba);
		val = ufshcd_readl(hba, REG_UFS_PROBE);
		ufshcd_update_evt_hist(hba, UFS_EVT_RESUME_ERR, (u32)val);
		val = ufshcd_readl(hba, REG_INTERRUPT_STATUS);
		ufshcd_update_evt_hist(hba, UFS_EVT_RESUME_ERR, (u32)val);
		return err;
	}

	err = ufshcd_uic_hibern8_exit(hba);
	if (!err)
		ufshcd_set_link_active(hba);
	else
		return err;

	err = ufshcd_make_hba_operational(hba);
	if (err)
		return err;

	return 0;
}

static int ufs_mtk_link_set_lpm(struct ufs_hba *hba)
{
	int err;

	/* not wait unipro resetCnf */
	ufshcd_writel(hba,
		(ufshcd_readl(hba, REG_UFS_XOUFS_CTRL) & ~0x100),
		REG_UFS_XOUFS_CTRL);

	err = ufs_mtk_unipro_set_lpm(hba, true);
	if (err) {
		/* Resume UniPro state for following error recovery */
		ufs_mtk_unipro_set_lpm(hba, false);
		return err;
	}

	return 0;
}

static void ufs_mtk_vccqx_set_lpm(struct ufs_hba *hba, bool lpm)
{
	struct ufs_vreg *vccqx = NULL;

	if (!hba->vreg_info.vccq && !hba->vreg_info.vccq2)
		return;

	if (hba->vreg_info.vccq)
		vccqx = hba->vreg_info.vccq;
	else
		vccqx = hba->vreg_info.vccq2;

	regulator_set_mode(vccqx->reg,
		lpm ? REGULATOR_MODE_IDLE : REGULATOR_MODE_NORMAL);
}

static void ufs_mtk_vsx_set_lpm(struct ufs_hba *hba, bool lpm)
{
	struct arm_smccc_res res;

	ufs_mtk_device_pwr_ctrl(!lpm,
	(unsigned long)hba->dev_info.wspecversion, res);
}

static void ufs_mtk_dev_vreg_set_lpm(struct ufs_hba *hba, bool lpm)
{
	if (!hba->vreg_info.vccq && !hba->vreg_info.vccq2)
		return;

	/* prevent entering LPM when device is still active */
	if (lpm && ufshcd_is_ufs_dev_active(hba))
		return;

	/*  Skip if VCC is assumed always-on */
	if (!hba->vreg_info.vcc)
		return;

	/*
	 * If VCC kept always-on, we do not use smc call to avoid
	 * non-essential time consumption.
	 *
	 * We don't need to control VS buck (the upper layer of VCCQ/VCCQ2)
	 * to enter LPM, because UFS device may be active when VCC
	 * is always-on. We also introduce UFS_MTK_CAP_FORCE_VSx_LPM to
	 * allow overriding such protection to save power.
	 */
	if (lpm && hba->vreg_info.vcc->enabled &&
		!ufs_mtk_is_force_vsx_lpm(hba))
		return;

	if (lpm) {
		ufs_mtk_vccqx_set_lpm(hba, lpm);
		ufs_mtk_vsx_set_lpm(hba, lpm);
	} else {
		ufs_mtk_vsx_set_lpm(hba, lpm);
		ufs_mtk_vccqx_set_lpm(hba, lpm);
	}
}

static int ufs_mtk_suspend_check(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	struct device_link *link;
	int err = 0;
	static bool bypass;

	/* Once wl_device can suspend, no need check anymore */
	if (bypass)
		goto out;

	/* Only check runtime pm */
	if (pm_op != UFS_RUNTIME_PM)
		goto out;

	list_for_each_entry(link,
		&hba->sdev_ufs_device->sdev_gendev.links.consumers,
		s_node) {

		/* If consumer is active, stop supplier enter suspend. */
		if (link->consumer->power.runtime_status == RPM_ACTIVE) {
			err = -EBUSY;
			goto out;
		}
	}

	bypass = true;

out:

	return err;
}

static int ufs_mtk_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op,
	enum ufs_notify_change_status status)
{
	int err;
	struct arm_smccc_res res;

	if (status == PRE_CHANGE) {
		err = ufs_mtk_suspend_check(hba, pm_op);
		if (err)
			return err;

		if (!ufshcd_is_auto_hibern8_supported(hba))
			return 0;
		ufs_mtk_auto_hibern8_disable(hba);
		return 0;
	}

	if (ufshcd_is_link_hibern8(hba)) {
		err = ufs_mtk_link_set_lpm(hba);
		if (err)
			goto fail;
	}

	if (!ufshcd_is_link_active(hba)) {
		/*
		 * Make sure no error will be returned to prevent
		 * ufshcd_suspend() re-enabling regulators while vreg is still
		 * in low-power mode.
		 */
		err = ufs_mtk_mphy_power_on(hba, false);
		if (err)
			goto fail;
	}

	if (ufshcd_is_link_off(hba))
		ufs_mtk_device_reset_ctrl(0, res);

	/* Transfer the ufs version to tfa */
	ufs_mtk_host_pwr_ctrl(HOST_PWR_HCI, false, res);

	return 0;
fail:
	/*
	 * Set link as off state enforcedly to trigger
	 * ufshcd_host_reset_and_restore() in ufshcd_suspend()
	 * for completed host reset.
	 */
	ufshcd_set_link_off(hba);
	return -EAGAIN;
}

static int ufs_mtk_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	int err;
	struct arm_smccc_res res;

	if (hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL)
		ufs_mtk_dev_vreg_set_lpm(hba, false);

	/* Transfer the ufs version to tfa */
	ufs_mtk_host_pwr_ctrl(HOST_PWR_HCI, true, res);

	err = ufs_mtk_mphy_power_on(hba, true);
	if (err)
		goto fail;

	if (ufshcd_is_link_hibern8(hba)) {
		err = ufs_mtk_link_set_hpm(hba);
		if (err)
			goto fail;
	}

	return 0;
fail:
	return ufshcd_link_recovery(hba);
}

static void ufs_mtk_dbg_register_dump(struct ufs_hba *hba)
{

	mt_irq_dump_status(hba->irq);

	/* Dump ufshci register 0x140 ~ 0x14C */
	ufshcd_dump_regs(hba, REG_UFS_XOUFS_CTRL, 0x10, "XOUFS Ctrl (0x140): ");

	ufshcd_dump_regs(hba, REG_UFS_EXTREG, 0x4, "Ext Reg ");

	/* Dump ufshci register 0x100 ~ 0x160 */
	ufshcd_dump_regs(hba, REG_UFS_CCAP,
			 REG_UFS_MMIO_OPT_CTRL_0 - REG_UFS_CCAP + 4,
			 "UFSHCI (0x100): ");

	/* Dump ufshci register 0x190 ~ 0x194 */
	ufshcd_dump_regs(hba, REG_UFS_MMIO_SQ_IS,
			 REG_UFS_MMIO_SQ_IE - REG_UFS_MMIO_SQ_IS + 4,
			 "UFSHCI (0x190): ");

	/* Dump ufshci register 0x1A0 ~ 0x1A4 */
	ufshcd_dump_regs(hba, REG_UFS_MMIO_CQ_IS,
			 REG_UFS_MMIO_CQ_IE - REG_UFS_MMIO_CQ_IS + 4,
			 "UFSHCI (0x1A0): ");

	/* Dump ufshci register 0x320 ~ 0x498 */
	ufshcd_dump_regs(hba, REG_UFS_MCQ_BASE,
			 MCQ_ADDR(REG_UFS_CQ_TAIL, 7) - REG_UFS_MCQ_BASE + 4,
			 "UFSHCI (0x320): ");

	/* Dump ufshci register 0x2200 ~ 0x22AC */
	ufshcd_dump_regs(hba, REG_UFS_MPHYCTRL,
			 REG_UFS_AH8X_MON - REG_UFS_MPHYCTRL + 4,
			 "UFSHCI (0x2200): ");

	/* Direct debugging information to REG_MTK_PROBE */
	ufs_mtk_dbg_sel(hba);
	ufshcd_dump_regs(hba, REG_UFS_PROBE, 0x4, "Debug Probe ");
#if IS_ENABLED(CONFIG_SCSI_UFS_MEDIATEK_DBG)
	ufs_mtk_dbg_dump(100);
#endif

#if IS_ENABLED(CONFIG_MTK_UFS_DEBUG)
	aee_kernel_warning_api(__FILE__,
		__LINE__, DB_OPT_FS_IO_LOG | DB_OPT_FTRACE,
		"ufs", "error dump");
#endif
}

static int ufs_mtk_apply_dev_quirks(struct ufs_hba *hba)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;
	u16 mid = dev_info->wmanufacturerid;

	if (mid == UFS_VENDOR_SAMSUNG) {
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TACTIVATE), 6);
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_HIBERN8TIME), 10);
	}

	/*
	 * Decide waiting time before gating reference clock and
	 * after ungating reference clock according to vendors'
	 * requirements. Default 30, 30.
	 */
	if (mid == UFS_VENDOR_SAMSUNG)
		ufs_mtk_setup_ref_clk_wait_us(hba, 1, 32);
	else if (mid == UFS_VENDOR_SKHYNIX)
		ufs_mtk_setup_ref_clk_wait_us(hba, 30, 30);
	else if (mid == UFS_VENDOR_TOSHIBA)
		ufs_mtk_setup_ref_clk_wait_us(hba, 100, 32);
	else
		ufs_mtk_setup_ref_clk_wait_us(hba, 30, 30);

	return 0;
}

static void ufs_mtk_fixup_dev_quirks(struct ufs_hba *hba)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	ufshcd_fixup_dev_quirks(hba, ufs_mtk_dev_fixups);

	if (STR_PRFX_EQUAL("H9HQ15AFAMBDAR", dev_info->model))
		host->caps |= UFS_MTK_CAP_BROKEN_VCC | UFS_MTK_CAP_FORCE_VSx_LPM;

	if (ufs_mtk_is_broken_vcc(hba) && hba->vreg_info.vcc &&
	    (hba->dev_quirks & UFS_DEVICE_QUIRK_DELAY_AFTER_LPM)) {
		hba->vreg_info.vcc->always_on = true;
		/*
		 * VCC will be kept always-on thus we don't
		 * need any delay during regulator operations
		 */
		hba->dev_quirks &= ~(UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM |
			UFS_DEVICE_QUIRK_DELAY_AFTER_LPM);
	}

	ufs_mtk_vreg_fix_vcc(hba);
	ufs_mtk_vreg_fix_vccqx(hba);
	ufs_mtk_fix_ahit(hba);

	ufs_mtk_install_tracepoints(hba);

#if IS_ENABLED(CONFIG_UFSFEATURE)
	if (hba->dev_info.wmanufacturerid == UFS_VENDOR_SAMSUNG) {
		host->ufsf.hba = hba;
		ufsf_set_init_state(ufs_mtk_get_ufsf(hba));
	}
#endif
}

static void ufs_mtk_event_notify(struct ufs_hba *hba,
				 enum ufs_event_type evt, void *data)
{
	unsigned int val = *(u32 *)data;
	unsigned long reg;
	uint8_t bit;

	trace_ufs_mtk_event(evt, val);

	/* Print details of UIC Errors */
	if (evt <= UFS_EVT_DME_ERR) {
		dev_info(hba->dev,
			 "Host UIC Error Code (%s): %08x\n",
			 ufs_uic_err_str[evt], val);
		reg = val;
	}

	if (evt == UFS_EVT_PA_ERR) {
		for_each_set_bit(bit, &reg, ARRAY_SIZE(ufs_uic_pa_err_str))
			dev_info(hba->dev, "%s\n", ufs_uic_pa_err_str[bit]);

		ufs_mtk_dbg_register_dump(hba);
	}

	if (evt == UFS_EVT_DL_ERR) {
		for_each_set_bit(bit, &reg, ARRAY_SIZE(ufs_uic_dl_err_str))
			dev_info(hba->dev, "%s\n", ufs_uic_dl_err_str[bit]);
	}

	/*
	 * After error handling of ufshcd_host_reset_and_restore
	 * Bypass clear ua to send scsi command request sense, else
	 * deadlock hang because scsi is waiting error handling done.
	 */
	/* TODO: clearing wlun_dev_clr_ua is removed,
	 * make sure related fix is pulled.
	 * https://android-review.googlesource.com/c/kernel/common/+/1905492
	 */

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	if (evt == UFS_EVT_ABORT && !ufs_abort_aee_count) {
		ufs_abort_aee_count++;
#if IS_ENABLED(CONFIG_SCSI_UFS_MEDIATEK_DBG)
		ufs_mtk_dbg_cmd_hist_disable();
		aee_kernel_warning_api(__FILE__,
			__LINE__, DB_OPT_FS_IO_LOG,
			"ufshcd_abort", "timeout at tag %d", val);
#endif
	}
#endif
}

static void ufs_mtk_auto_hibern8_disable(struct ufs_hba *hba)
{
	unsigned long flags;
	int ret;

	/* disable auto-hibern8 */
	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_writel(hba, 0, REG_AUTO_HIBERNATE_IDLE_TIMER);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	/* wait host return to idle state when auto-hibern8 off */
	ufs_mtk_wait_idle_state(hba, 5);

	ret = ufs_mtk_wait_link_state(hba, VS_LINK_UP, 100);
	if (ret)
		dev_warn(hba->dev, "exit h8 state fail, ret=%d\n", ret);
}

void ufs_mtk_setup_task_mgmt(struct ufs_hba *hba, int tag, u8 tm_function)
{
#if IS_ENABLED(CONFIG_UFSFEATURE)
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);

	if (ufsf->hba && (tm_function == UFS_LOGICAL_RESET))
		ufsf_prepare_reset_lu(ufsf);
#endif
}


static void ufs_mtk_config_scaling_param(struct ufs_hba *hba,
					struct devfreq_dev_profile *profile,
					void *data)
{
	/* customize clk scaling parms */
	hba->clk_scaling.min_gear = UFS_HS_G4;

	hba->vps->devfreq_profile.polling_ms = 200;
	hba->vps->ondemand_data.upthreshold = 40;
	hba->vps->ondemand_data.downdifferential = 20;
}

/**
 * ufs_mtk_clk_scale - Internal clk scaling operation
 * MTK platform supports clk scaling by switching parent of ufs_sel(mux).
 * The ufs_sel downstream to ufs_ck and fed directly to UFS hardware.
 * Max and min clocks rate of ufs_sel defined in dts should match rate of
 * "ufs_sel_max_src" and "ufs_sel_min_src" respectively.
 * This prevent chaning rate of pll clock that is shared between modules.
 *
 * WARN: Need fix if pll clk rate is not static.
 *
 * @param hba: per adapter instance
 * @param scale_up: True for scaling up and false for scaling down
 */
static void ufs_mtk_clk_scale(struct ufs_hba *hba, bool scale_up)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_mtk_clk *mclk = &host->mclk;
	struct ufs_clk_info *clki = mclk->ufs_sel_clki;
	int ret = 0;
	int ver;
	/* u32 ahit; */
	static bool skip_switch;

	if (host->clk_scale_up == scale_up)
		goto out;

	/* set longer ah8 timer when scale up */
	/*
	if (scale_up)
		ahit = FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, 5) |
				FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, 3);
	else
		ahit = host->desired_ahit;
	ufshcd_auto_hibern8_update(hba, ahit);
	*/

	if (skip_switch)
		goto skip;

	/* Do switch */
	ret = clk_prepare_enable(clki->clk);
	if (ret) {
		dev_info(hba->dev, "clk_prepare_enable() fail, ret = %d\n", ret);
		goto skip;
	}

	/*
	 * Parent switching may have glich and uic error.
	 * Keep UFS4.0 device fast clock and UFS3.1 device slow clock.
	 */
	ver = (hba->dev_info.wspecversion & 0xF00) >> 8;
	if (ver >= 4)
		ret = clk_set_parent(clki->clk, mclk->ufs_sel_max_clki->clk);
	else
		ret = clk_set_parent(clki->clk, mclk->ufs_sel_min_clki->clk);

	if (ret)
		dev_info(hba->dev, "Failed to set ufs_sel_clki, ret = %d\n", ret);

	clk_disable_unprepare(clki->clk);

	skip_switch = true;

skip:
	host->clk_scale_up = scale_up;

	/* Must always set before clk_set_rate() */
	if (scale_up)
		clki->curr_freq = clki->max_freq;
	else
		clki->curr_freq = clki->min_freq;
out:
	trace_ufs_mtk_clk_scale(clki->name, scale_up, clk_get_rate(clki->clk));
}

static int ufs_mtk_clk_scale_notify(struct ufs_hba *hba, bool scale_up,
				enum ufs_notify_change_status status)
{
	if (!ufshcd_is_clkscaling_supported(hba) || !hba->clk_scaling.is_enabled)
		return 0;

	if (status == PRE_CHANGE) {
		ufs_mtk_pm_qos(hba, scale_up);

		/* do parent switching before clk_set_rate() */
		ufs_mtk_clk_scale(hba, scale_up);
	}

	return 0;
}

/*
 * struct ufs_hba_mtk_vops - UFS MTK specific variant operations
 *
 * The variant operations configure the necessary controller and PHY
 * handshake during initialization.
 */
static const struct ufs_hba_variant_ops ufs_hba_mtk_vops = {
	.name                = "mediatek.ufshci",
	.init                = ufs_mtk_init,
	.get_ufs_hci_version = ufs_mtk_get_ufs_hci_version,
	.setup_clocks        = ufs_mtk_setup_clocks,
	.hce_enable_notify   = ufs_mtk_hce_enable_notify,
	.link_startup_notify = ufs_mtk_link_startup_notify,
	.pwr_change_notify   = ufs_mtk_pwr_change_notify,
	.apply_dev_quirks    = ufs_mtk_apply_dev_quirks,
	.fixup_dev_quirks    = ufs_mtk_fixup_dev_quirks,
	.suspend             = ufs_mtk_suspend,
	.resume              = ufs_mtk_resume,
	.dbg_register_dump   = ufs_mtk_dbg_register_dump,
	.device_reset        = ufs_mtk_device_reset,
	.event_notify        = ufs_mtk_event_notify,
	.setup_task_mgmt     = ufs_mtk_setup_task_mgmt,
	.config_scaling_param = ufs_mtk_config_scaling_param,
	.clk_scale_notify    = ufs_mtk_clk_scale_notify,
};

/**
 * ufs_mtk_probe - probe routine of the driver
 * @pdev: pointer to Platform device handle
 *
 * Return zero for success and non-zero for failure
 */
static int ufs_mtk_probe(struct platform_device *pdev)
{
	int err = 0;
	struct device *dev = &pdev->dev, *phy_dev;
	struct device_node *reset_node, *phy_node = NULL;
	struct platform_device *reset_pdev, *phy_pdev = NULL;
	struct device_link *link, *phy_link;
	struct ufs_hba *hba;

	reset_node = of_find_compatible_node(NULL, NULL,
					     "ti,syscon-reset");
	if (!reset_node) {
		dev_notice(dev, "find ti,syscon-reset fail\n");
		goto skip_reset;
	}
	reset_pdev = of_find_device_by_node(reset_node);
	if (!reset_pdev) {
		dev_notice(dev, "find reset_pdev fail\n");
		goto skip_reset;
	}
	link = device_link_add(dev, &reset_pdev->dev,
		DL_FLAG_AUTOPROBE_CONSUMER);
	if (!link) {
		dev_notice(dev, "add reset device_link fail\n");
		goto skip_reset;
	}
	/* supplier is not probed */
	if (link->status == DL_STATE_DORMANT) {
		err = -EPROBE_DEFER;
		goto out;
	}

skip_reset:
	/* find phy node */
	phy_node = of_parse_phandle(dev->of_node, "phys", 0);

	if (phy_node) {
		phy_pdev = of_find_device_by_node(phy_node);
		if (!phy_pdev)
			goto skip_phy;
		phy_dev = &phy_pdev->dev;

		pm_runtime_set_active(phy_dev);
		phy_link = device_link_add(dev, phy_dev, DL_FLAG_PM_RUNTIME);
		if (!phy_link) {
			dev_notice(dev, "add phys device_link fail\n");
			goto skip_phy;
		}
		pm_runtime_enable(phy_dev);
		dev_info(dev, "phys node found\n");
	} else {
		dev_notice(dev, "phys node not found\n");
	}

skip_phy:
	/* Get IRQ */
	ufs_mtk_mcq_get_irq(pdev);
	ufs_mtk_mcq_install_tracepoints();

	/* perform generic probe */
	err = ufshcd_pltfrm_init(pdev, &ufs_hba_mtk_vops);
	if (err) {
		dev_info(dev, "probe failed %d\n", err);
		goto out;
	}

	/* set affinity to cpu3 */
	hba = platform_get_drvdata(pdev);
	if (hba && hba->irq)
		irq_set_affinity_hint(hba->irq, get_cpu_mask(3));

	ufs_mtk_mcq_set_irq_affinity(hba);

	/*
	 * Because the default power setting of VSx (the upper layer of
	 * VCCQ/VCCQ2) is HWLP, we need to prevent VCCQ/VCCQ2 from
	 * entering LPM.
	 */
	ufs_mtk_dev_vreg_set_lpm(hba, false);

out:
	of_node_put(phy_node);
	of_node_put(reset_node);
	return err;
}

#if defined(CONFIG_UFSFEATURE)
static void ufs_mtk_remove_ufsf(struct ufs_hba *hba)
{
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);

	if (ufsf->hba)
		ufsf_remove(ufsf);
}
#endif

/**
 * ufs_mtk_remove - set driver_data of the device to NULL
 * @pdev: pointer to platform device handle
 *
 * Always return 0
 */
static int ufs_mtk_remove(struct platform_device *pdev)
{
	struct ufs_hba *hba = platform_get_drvdata(pdev);

	pm_runtime_get_sync(&(pdev)->dev);

#if defined(CONFIG_UFSFEATURE)
	ufs_mtk_remove_ufsf(hba);
#endif


#if IS_ENABLED(CONFIG_SCSI_UFS_MEDIATEK_DBG)
	if (hba->caps & UFSHCD_CAP_CLK_SCALING)
		ufs_mtk_remove_clk_scaling_sysfs(hba);
#endif

	ufshcd_remove(hba);
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
	ufs_mtk_biolog_exit();
#endif
	ufs_mtk_uninstall_tracepoints();
	return 0;
}

int ufs_mtk_system_suspend(struct device *dev)
{
	int ret = 0;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host;
#if defined(CONFIG_UFSFEATURE)
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);
#endif

	host = ufshcd_get_variant(hba);
	if (down_trylock(&host->rpmb_sem))
		return -EBUSY;

#if defined(CONFIG_UFSFEATURE)
	if (ufsf->hba)
		ufsf_suspend(ufsf);
#endif

	/* Check if shutting down */
	if (!ufshcd_is_user_access_allowed(hba)) {
		ret = -EBUSY;
		goto out;
	}

	ret = ufshcd_system_suspend(dev);

	if (!ret)
		ufs_mtk_dev_vreg_set_lpm(hba, true);
out:

#if defined(CONFIG_UFSFEATURE)
	/* We assume link is off */
	if (ret && ufsf)
		ufsf_resume(ufsf, true);
#endif
	if (ret)
		up(&host->rpmb_sem);

	return ret;
}

int ufs_mtk_system_resume(struct device *dev)
{
	int ret = 0;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host;
#if defined(CONFIG_UFSFEATURE)
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);
	bool is_link_off = ufshcd_is_link_off(hba);
#endif

	ufs_mtk_dev_vreg_set_lpm(hba, false);

	ret = ufshcd_system_resume(dev);

#if defined(CONFIG_UFSFEATURE)
	if (!ret && ufsf->hba)
		ufsf_resume(ufsf, is_link_off);
#endif

	host = ufshcd_get_variant(hba);
	if (!ret)
		up(&host->rpmb_sem);

	return ret;
}

int ufs_mtk_runtime_suspend(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	int ret = 0;

#if defined(CONFIG_UFSFEATURE)
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);

	if (ufsf->hba)
		ufsf_suspend(ufsf);
#endif
	ret = ufshcd_runtime_suspend(dev);

	if (!ret)
		ufs_mtk_dev_vreg_set_lpm(hba, true);

#if defined(CONFIG_UFSFEATURE)
	/* We assume link is off */
	if (ret && ufsf->hba)
		ufsf_resume(ufsf, true);
#endif

	return ret;
}

int ufs_mtk_runtime_resume(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	int ret = 0;

#if defined(CONFIG_UFSFEATURE)
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);
	bool is_link_off = ufshcd_is_link_off(hba);
#endif
	ufs_mtk_dev_vreg_set_lpm(hba, false);

	ret = ufshcd_runtime_resume(dev);

#if defined(CONFIG_UFSFEATURE)
	if (!ret && ufsf->hba)
		ufsf_resume(ufsf, is_link_off);
#endif

	return ret;
}

void ufs_mtk_shutdown(struct platform_device *pdev)
{
#if defined(CONFIG_UFSFEATURE)
	struct device *dev = &pdev->dev;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);

	if (ufsf->hba)
		ufsf_suspend(ufsf);
#endif

	/*
	 * ufshcd_wl_shutdown may run concurrently and have racing problem.
	 * ufshcd_pltfrm_shutdown only turn off power and clock, which is not
	 * necessary in shutdwon flow. Beside, ufshcd_shutdown flow is
	 * incorrect, it will not turn off power and clock after
	 * ufshcd_wl_shutdown (dev is poweroff and link is off)
	 * So, it is not necessary and remove ufshcd_pltfrm_shutdown.
	 */
}

static const struct dev_pm_ops ufs_mtk_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ufs_mtk_system_suspend, ufs_mtk_system_resume)
	SET_RUNTIME_PM_OPS(ufs_mtk_runtime_suspend, ufs_mtk_runtime_resume, NULL)
	.prepare	 = ufshcd_suspend_prepare,
	.complete	 = ufshcd_resume_complete,
};

static struct platform_driver ufs_mtk_pltform = {
	.probe      = ufs_mtk_probe,
	.remove     = ufs_mtk_remove,
	.shutdown   = ufs_mtk_shutdown,
	.driver = {
		.name   = "ufshcd-mtk",
		.pm     = &ufs_mtk_pm_ops,
		.of_match_table = ufs_mtk_of_match,
	},
};

MODULE_AUTHOR("Stanley Chu <stanley.chu@mediatek.com>");
MODULE_AUTHOR("Peter Wang <peter.wang@mediatek.com>");
MODULE_DESCRIPTION("MediaTek UFS Host Driver");
MODULE_LICENSE("GPL v2");

module_platform_driver(ufs_mtk_pltform);
