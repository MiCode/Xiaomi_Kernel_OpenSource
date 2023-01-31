// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2021, Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/acpi.h>
#include <linux/time.h>
#include <linux/of.h>
#include <linux/bitfield.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/gpio/consumer.h>
#include <linux/reset-controller.h>
#include <linux/interconnect.h>
#include <linux/phy/phy-qcom-ufs.h>
#include <linux/clk/qcom.h>
#include <linux/devfreq.h>
#include <linux/cpu.h>
#include <linux/blk-mq.h>
#include <linux/thermal.h>
#include <linux/cpufreq.h>
#include <linux/debugfs.h>
#include <trace/hooks/ufshcd.h>
#include <linux/ipc_logging.h>
#include <linux/nvmem-consumer.h>
#include <linux/irq.h>

#include "ufshcd.h"
#include "ufshcd-pltfrm.h"
#include "unipro.h"
#include "ufs-qcom.h"
#include "ufshci.h"
#include "ufs_quirks.h"
#include "ufshcd-crypto-qti.h"
#include <trace/hooks/ufshcd.h>

#define UFS_QCOM_DEFAULT_DBG_PRINT_EN	\
	(UFS_QCOM_DBG_PRINT_REGS_EN | UFS_QCOM_DBG_PRINT_TEST_BUS_EN)

#define UFS_DDR "ufs-ddr"
#define CPU_UFS "cpu-ufs"
#define MAX_PROP_SIZE		   32
#define VDDP_REF_CLK_MIN_UV        1200000
#define VDDP_REF_CLK_MAX_UV        1200000

#define UFS_QCOM_LOAD_MON_DLY_MS 30
#define UFS_QCOM_DEFAULT_DBG_PRINT_EN	\
	(UFS_QCOM_DBG_PRINT_REGS_EN | UFS_QCOM_DBG_PRINT_TEST_BUS_EN)

#define	ANDROID_BOOT_DEV_MAX	30

#define	UFS_QCOM_IRQ_PRIME_MASK	0x80
#define	UFS_QCOM_IRQ_SLVR_MASK	0x0f

/* Max number of log pages */
#define UFS_QCOM_MAX_LOG_SZ	10
#define ufs_qcom_log_str(host, fmt, ...)	\
	do {	\
		if (host->ufs_ipc_log_ctx && host->dbg_en)	\
			ipc_log_string(host->ufs_ipc_log_ctx,	\
					",%d,"fmt, current->cpu, ##__VA_ARGS__);\
	} while (0)

#define UFS_BOOT_DEVICE  0x1
static u32 is_bootdevice_ufs = UFS_BOOT_DEVICE;

static char android_boot_dev[ANDROID_BOOT_DEV_MAX];

static DEFINE_PER_CPU(struct freq_qos_request, qos_min_req);

/* clk freq mode */
enum {
	LOW_SVS,
	NOM,
	TURBO,
	TURBO_L1,
	INVAL_MODE,
};

enum {
	TSTBUS_UAWM,
	TSTBUS_UARM,
	TSTBUS_TXUC,
	TSTBUS_RXUC,
	TSTBUS_DFC,
	TSTBUS_TRLUT,
	TSTBUS_TMRLUT,
	TSTBUS_OCSC,
	TSTBUS_UTP_HCI,
	TSTBUS_COMBINED,
	TSTBUS_WRAPPER,
	TSTBUS_UNIPRO,
	TSTBUS_MAX,
};

struct ufs_qcom_dev_params {
	u32 pwm_rx_gear;	/* pwm rx gear to work in */
	u32 pwm_tx_gear;	/* pwm tx gear to work in */
	u32 hs_rx_gear;		/* hs rx gear to work in */
	u32 hs_tx_gear;		/* hs tx gear to work in */
	u32 rx_lanes;		/* number of rx lanes */
	u32 tx_lanes;		/* number of tx lanes */
	u32 rx_pwr_pwm;		/* rx pwm working pwr */
	u32 tx_pwr_pwm;		/* tx pwm working pwr */
	u32 rx_pwr_hs;		/* rx hs working pwr */
	u32 tx_pwr_hs;		/* tx hs working pwr */
	u32 hs_rate;		/* rate A/B to work in HS */
	u32 desired_working_mode;
};

static struct ufs_qcom_host *ufs_qcom_hosts[MAX_UFS_QCOM_HOSTS];

static void ufs_qcom_get_default_testbus_cfg(struct ufs_qcom_host *host);
static int ufs_qcom_set_dme_vs_core_clk_ctrl_clear_div(struct ufs_hba *hba,
						       u32 clk_1us_cycles,
						       u32 clk_40ns_cycles);
static void ufs_qcom_parse_limits(struct ufs_qcom_host *host);
static void ufs_qcom_parse_lpm(struct ufs_qcom_host *host);
static int ufs_qcom_set_dme_vs_core_clk_ctrl_max_freq_mode(struct ufs_hba *hba);
static int ufs_qcom_init_sysfs(struct ufs_hba *hba);
static int ufs_qcom_update_qos_constraints(struct qos_cpu_group *qcg,
					   enum constraint type);
static int ufs_qcom_unvote_qos_all(struct ufs_hba *hba);
static void ufs_qcom_parse_g4_workaround_flag(struct ufs_qcom_host *host);
static int ufs_qcom_mod_min_cpufreq(unsigned int cpu, s32 new_val);
static void ufs_qcom_hook_clock_scaling(void *used, struct ufs_hba *hba, bool *force_out,
		bool *force_saling, bool *scale_up);

static int ufs_qcom_get_pwr_dev_param(struct ufs_qcom_dev_params *qcom_param,
				      struct ufs_pa_layer_attr *dev_max,
				      struct ufs_pa_layer_attr *agreed_pwr)
{
	int min_qcom_gear;
	int min_dev_gear;
	bool is_dev_sup_hs = false;
	bool is_qcom_max_hs = false;

	if (dev_max->pwr_rx == FAST_MODE)
		is_dev_sup_hs = true;

	if (qcom_param->desired_working_mode == FAST) {
		is_qcom_max_hs = true;
		min_qcom_gear = min_t(u32, qcom_param->hs_rx_gear,
				      qcom_param->hs_tx_gear);
	} else {
		min_qcom_gear = min_t(u32, qcom_param->pwm_rx_gear,
				      qcom_param->pwm_tx_gear);
	}

	/*
	 * device doesn't support HS but qcom_param->desired_working_mode is
	 * HS, thus device and qcom_param don't agree
	 */
	if (!is_dev_sup_hs && is_qcom_max_hs) {
		pr_err("%s: failed to agree on power mode (device doesn't support HS but requested power is HS)\n",
			__func__);
		return -ENOTSUPP;
	} else if (is_dev_sup_hs && is_qcom_max_hs) {
		/*
		 * since device supports HS, it supports FAST_MODE.
		 * since qcom_param->desired_working_mode is also HS
		 * then final decision (FAST/FASTAUTO) is done according
		 * to qcom_params as it is the restricting factor
		 */
		agreed_pwr->pwr_rx = agreed_pwr->pwr_tx =
						qcom_param->rx_pwr_hs;
	} else {
		/*
		 * here qcom_param->desired_working_mode is PWM.
		 * it doesn't matter whether device supports HS or PWM,
		 * in both cases qcom_param->desired_working_mode will
		 * determine the mode
		 */
		agreed_pwr->pwr_rx = agreed_pwr->pwr_tx =
			qcom_param->rx_pwr_pwm;
	}

	/*
	 * we would like tx to work in the minimum number of lanes
	 * between device capability and vendor preferences.
	 * the same decision will be made for rx
	 */
	agreed_pwr->lane_tx = min_t(u32, dev_max->lane_tx,
						qcom_param->tx_lanes);
	agreed_pwr->lane_rx = min_t(u32, dev_max->lane_rx,
						qcom_param->rx_lanes);

	/* device maximum gear is the minimum between device rx and tx gears */
	min_dev_gear = min_t(u32, dev_max->gear_rx, dev_max->gear_tx);

	/*
	 * if both device capabilities and vendor pre-defined preferences are
	 * both HS or both PWM then set the minimum gear to be the chosen
	 * working gear.
	 * if one is PWM and one is HS then the one that is PWM get to decide
	 * what is the gear, as it is the one that also decided previously what
	 * pwr the device will be configured to.
	 */
	if ((is_dev_sup_hs && is_qcom_max_hs) ||
	    (!is_dev_sup_hs && !is_qcom_max_hs))
		agreed_pwr->gear_rx = agreed_pwr->gear_tx =
			min_t(u32, min_dev_gear, min_qcom_gear);
	else if (!is_dev_sup_hs)
		agreed_pwr->gear_rx = agreed_pwr->gear_tx = min_dev_gear;
	else
		agreed_pwr->gear_rx = agreed_pwr->gear_tx = min_qcom_gear;

	agreed_pwr->hs_rate = qcom_param->hs_rate;
	return 0;
}

static struct ufs_qcom_host *rcdev_to_ufs_host(struct reset_controller_dev *rcd)
{
	return container_of(rcd, struct ufs_qcom_host, rcdev);
}

static void ufs_qcom_dump_regs_wrapper(struct ufs_hba *hba, int offset, int len,
				       const char *prefix, void *priv)
{
	ufshcd_dump_regs(hba, offset, len * 4, prefix);
}

static int ufs_qcom_get_connected_tx_lanes(struct ufs_hba *hba, u32 *tx_lanes)
{
	int err = 0;

	err = ufshcd_dme_get(hba,
			UIC_ARG_MIB(PA_CONNECTEDTXDATALANES), tx_lanes);
	if (err)
		dev_err(hba->dev, "%s: couldn't read PA_CONNECTEDTXDATALANES %d\n",
				__func__, err);

	return err;
}

static int ufs_qcom_get_connected_rx_lanes(struct ufs_hba *hba, u32 *rx_lanes)
{
	int err = 0;

	err = ufshcd_dme_get(hba,
			UIC_ARG_MIB(PA_CONNECTEDRXDATALANES), rx_lanes);
	if (err)
		dev_err(hba->dev, "%s: couldn't read PA_CONNECTEDRXDATALANES %d\n",
				__func__, err);

	return err;
}

static int ufs_qcom_host_clk_get(struct device *dev,
		const char *name, struct clk **clk_out, bool optional)
{
	struct clk *clk;
	int err = 0;

	clk = devm_clk_get(dev, name);
	if (!IS_ERR(clk)) {
		*clk_out = clk;
		return 0;
	}

	err = PTR_ERR(clk);

	if (optional && err == -ENOENT) {
		*clk_out = NULL;
		return 0;
	}

	if (err != -EPROBE_DEFER)
		dev_err(dev, "failed to get %s err %d\n", name, err);

	return err;
}

static int ufs_qcom_host_clk_enable(struct device *dev,
		const char *name, struct clk *clk)
{
	int err = 0;

	err = clk_prepare_enable(clk);
	if (err)
		dev_err(dev, "%s: %s enable failed %d\n", __func__, name, err);

	return err;
}

static void ufs_qcom_disable_lane_clks(struct ufs_qcom_host *host)
{
	if (!host->is_lane_clks_enabled)
		return;

	if (host->tx_l1_sync_clk)
		clk_disable_unprepare(host->tx_l1_sync_clk);
	clk_disable_unprepare(host->tx_l0_sync_clk);
	if (host->rx_l1_sync_clk)
		clk_disable_unprepare(host->rx_l1_sync_clk);
	clk_disable_unprepare(host->rx_l0_sync_clk);

	host->is_lane_clks_enabled = false;
}

static int ufs_qcom_enable_lane_clks(struct ufs_qcom_host *host)
{
	int err = 0;
	struct device *dev = host->hba->dev;

	if (host->is_lane_clks_enabled)
		return 0;

	err = ufs_qcom_host_clk_enable(dev, "rx_lane0_sync_clk",
		host->rx_l0_sync_clk);
	if (err)
		goto out;

	err = ufs_qcom_host_clk_enable(dev, "tx_lane0_sync_clk",
		host->tx_l0_sync_clk);
	if (err)
		goto disable_rx_l0;

	if (host->hba->lanes_per_direction > 1) {
		err = ufs_qcom_host_clk_enable(dev, "rx_lane1_sync_clk",
			host->rx_l1_sync_clk);
		if (err)
			goto disable_tx_l0;

		/* The tx lane1 clk could be muxed, hence keep this optional */
		if (host->tx_l1_sync_clk) {
			err = ufs_qcom_host_clk_enable(dev, "tx_lane1_sync_clk",
					host->tx_l1_sync_clk);
			if (err)
				goto disable_rx_l1;
		}
	}

	host->is_lane_clks_enabled = true;
	goto out;

disable_rx_l1:
	clk_disable_unprepare(host->rx_l1_sync_clk);
disable_tx_l0:
	clk_disable_unprepare(host->tx_l0_sync_clk);
disable_rx_l0:
	clk_disable_unprepare(host->rx_l0_sync_clk);
out:
	return err;
}

static int ufs_qcom_init_lane_clks(struct ufs_qcom_host *host)
{
	int err = 0;
	struct device *dev = host->hba->dev;

	if (has_acpi_companion(dev))
		return 0;

	err = ufs_qcom_host_clk_get(dev, "rx_lane0_sync_clk",
					&host->rx_l0_sync_clk, false);
	if (err) {
		dev_err(dev, "%s: failed to get rx_lane0_sync_clk, err %d\n",
				__func__, err);
		goto out;
	}

	err = ufs_qcom_host_clk_get(dev, "tx_lane0_sync_clk",
					&host->tx_l0_sync_clk, false);
	if (err) {
		dev_err(dev, "%s: failed to get tx_lane0_sync_clk, err %d\n",
				__func__, err);
		goto out;
	}

	/* In case of single lane per direction, don't read lane1 clocks */
	if (host->hba->lanes_per_direction > 1) {
		err = ufs_qcom_host_clk_get(dev, "rx_lane1_sync_clk",
			&host->rx_l1_sync_clk, false);
		if (err) {
			dev_err(dev, "%s: failed to get rx_lane1_sync_clk, err %d\n",
					__func__, err);
			goto out;
		}

		err = ufs_qcom_host_clk_get(dev, "tx_lane1_sync_clk",
			&host->tx_l1_sync_clk, true);
	}
out:
	return err;
}

static int ufs_qcom_link_startup_post_change(struct ufs_hba *hba)
{
	u32 tx_lanes;
	int err = 0;
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct phy *phy = host->generic_phy;

	err = ufs_qcom_get_connected_tx_lanes(hba, &tx_lanes);
	if (err)
		goto out;

	ufs_qcom_phy_set_tx_lane_enable(phy, tx_lanes);
	/*
	 * Some UFS devices send incorrect LineCfg data as part of power mode
	 * change sequence which may cause host PHY to go into bad state.
	 * Disabling Rx LineCfg of host PHY should help avoid this.
	 */
	if (ufshcd_get_local_unipro_ver(hba) == UFS_UNIPRO_VER_1_41)
		ufs_qcom_phy_ctrl_rx_linecfg(phy, false);

	/*
	 * UFS controller has *clk_req output to GCC, for each of the clocks
	 * entering it. When *clk_req for a specific clock is de-asserted,
	 * a corresponding clock from GCC is stopped. UFS controller de-asserts
	 * *clk_req outputs when it is in Auto Hibernate state only if the
	 * Clock request feature is enabled.
	 * Enable the Clock request feature:
	 * - Enable HW clock control for UFS clocks in GCC (handled by the
	 *   clock driver as part of clk_prepare_enable).
	 * - Set the AH8_CFG.*CLK_REQ register bits to 1.
	 */
	if (ufshcd_is_auto_hibern8_supported(hba))
		ufshcd_writel(hba, ufshcd_readl(hba, UFS_AH8_CFG) |
				   UFS_HW_CLK_CTRL_EN,
				   UFS_AH8_CFG);
	/*
	 * Make sure clock request feature gets enabled for HW clk gating
	 * before further operations.
	 */
	mb();

out:
	return err;
}

static int ufs_qcom_check_hibern8(struct ufs_hba *hba)
{
	int err;
	u32 tx_fsm_val = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(HBRN8_POLL_TOUT_MS);

	do {
		err = ufshcd_dme_get(hba,
				UIC_ARG_MIB_SEL(MPHY_TX_FSM_STATE,
					UIC_ARG_MPHY_TX_GEN_SEL_INDEX(0)),
				&tx_fsm_val);
		if (err || tx_fsm_val == TX_FSM_HIBERN8)
			break;

		/* sleep for max. 200us */
		usleep_range(100, 200);
	} while (time_before(jiffies, timeout));

	/*
	 * we might have scheduled out for long during polling so
	 * check the state again.
	 */
	if (time_after(jiffies, timeout))
		err = ufshcd_dme_get(hba,
				UIC_ARG_MIB_SEL(MPHY_TX_FSM_STATE,
					UIC_ARG_MPHY_TX_GEN_SEL_INDEX(0)),
				&tx_fsm_val);

	if (err) {
		dev_err(hba->dev, "%s: unable to get TX_FSM_STATE, err %d\n",
				__func__, err);
	} else if (tx_fsm_val != TX_FSM_HIBERN8) {
		err = tx_fsm_val;
		dev_err(hba->dev, "%s: invalid TX_FSM_STATE = %d\n",
				__func__, err);
	}

	return err;
}

static void ufs_qcom_select_unipro_mode(struct ufs_qcom_host *host)
{
	ufshcd_rmwl(host->hba, QUNIPRO_SEL,
		   ufs_qcom_cap_qunipro(host) ? QUNIPRO_SEL : 0,
		   REG_UFS_CFG1);
	/* make sure above configuration is applied before we return */
	mb();
}

/*
 * ufs_qcom_host_reset - reset host controller and PHY
 */
static int ufs_qcom_host_reset(struct ufs_hba *hba)
{
	int ret = 0;
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	bool reenable_intr = false;

	host->reset_in_progress = true;

	if (!host->core_reset) {
		dev_warn(hba->dev, "%s: reset control not set\n", __func__);
		goto out;
	}

	reenable_intr = hba->is_irq_enabled;
	disable_irq(hba->irq);
	hba->is_irq_enabled = false;

	ret = reset_control_assert(host->core_reset);
	if (ret) {
		dev_err(hba->dev, "%s: core_reset assert failed, err = %d\n",
				 __func__, ret);
		goto out;
	}

	/*
	 * The hardware requirement for delay between assert/deassert
	 * is at least 3-4 sleep clock (32.7KHz) cycles, which comes to
	 * ~125us (4/32768). To be on the safe side add 200us delay.
	 */
	usleep_range(200, 210);

	ret = reset_control_deassert(host->core_reset);
	if (ret)
		dev_err(hba->dev, "%s: core_reset deassert failed, err = %d\n",
				 __func__, ret);

	usleep_range(1000, 1100);
	/*
	 * The ice registers are also reset to default values after a ufs
	 * host controller reset. Reset the ice internal software flags here
	 * so that the ice hardware will be re-initialized properly in the
	 * later part of the UFS host controller reset.
	 */
	ufs_qcom_ice_disable(host);

	if (reenable_intr) {
		enable_irq(hba->irq);
		hba->is_irq_enabled = true;
	}

out:
	host->reset_in_progress = false;
	return ret;
}

static int ufs_qcom_phy_power_on(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct phy *phy = host->generic_phy;
	int ret = 0;

	mutex_lock(&host->phy_mutex);
	if (!host->is_phy_pwr_on) {
		ret = phy_power_on(phy);
		if (ret) {
			mutex_unlock(&host->phy_mutex);
			return ret;
		}
		host->is_phy_pwr_on = true;
	}
	mutex_unlock(&host->phy_mutex);

	return ret;
}

static int ufs_qcom_phy_power_off(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct phy *phy = host->generic_phy;
	int ret = 0;

	mutex_lock(&host->phy_mutex);
	if (host->is_phy_pwr_on) {
		ret = phy_power_off(phy);
		if (ret) {
			mutex_unlock(&host->phy_mutex);
			return ret;
		}
		host->is_phy_pwr_on = false;
	}
	mutex_unlock(&host->phy_mutex);

	return ret;
}

static int ufs_qcom_power_up_sequence(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct phy *phy = host->generic_phy;
	int ret = 0;
	enum phy_mode mode = (host->limit_rate == PA_HS_MODE_B) ?
					PHY_MODE_UFS_HS_B : PHY_MODE_UFS_HS_A;
	int submode = host->limit_phy_submode;

	if (host->hw_ver.major < 0x4)
		submode = UFS_QCOM_PHY_SUBMODE_NON_G4;
	phy_set_mode_ext(phy, mode, submode);

	ret = ufs_qcom_phy_power_on(hba);
	if (ret) {
		dev_err(hba->dev, "%s: phy power on failed, ret = %d\n",
				 __func__, ret);
		goto out;
	}

	ret = phy_calibrate(phy);
	if (ret) {
		dev_err(hba->dev, "%s: Failed to calibrate PHY %d\n",
				  __func__, ret);
		goto out;
	}

	ufs_qcom_select_unipro_mode(host);

out:
	return ret;
}

/*
 * The UTP controller has a number of internal clock gating cells (CGCs).
 * Internal hardware sub-modules within the UTP controller control the CGCs.
 * Hardware CGCs disable the clock to inactivate UTP sub-modules not involved
 * in a specific operation, UTP controller CGCs are by default disabled and
 * this function enables them (after every UFS link startup) to save some power
 * leakage.
 *
 * UFS host controller v3.0.0 onwards has internal clock gating mechanism
 * in Qunipro, enable them to save additional power.
 */
static int ufs_qcom_enable_hw_clk_gating(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct ufs_clk_info *clki;
	int err = 0;
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;

	clki = list_first_entry(&hba->clk_list_head, struct ufs_clk_info, list);

	/* Enable UTP internal clock gating */
	ufshcd_writel(hba,
		ufshcd_readl(hba, REG_UFS_CFG2) | REG_UFS_CFG2_CGC_EN_ALL,
		REG_UFS_CFG2);

	/* Ensure that HW clock gating is enabled before next operations */
	mb();

	/* Enable Qunipro internal clock gating if supported */
	if (!ufs_qcom_cap_qunipro_clk_gating(host))
		goto out;

	/* Enable all the mask bits */
	err = ufshcd_dme_rmw(hba, DL_VS_CLK_CFG_MASK,
				DL_VS_CLK_CFG_MASK, DL_VS_CLK_CFG);
	if (err)
		goto out;

	/*
	 * As per HPG, ATTR_HW_CGC_EN bit should be enabled when operating at
	 * more than 300mhz for target which support multi level clk scaling
	 * and hence should be disable in UFS init sequence for those target.
	 * For rest all other target set the default to 1 like earlier target.
	 * CGC bit should be enabled for target supporting multilevel and
	 * irrespective of type of UFS device.
	 */
	if (of_property_read_bool(np, "disable-cgc")) {
		if (host->ml_scale_sup &&
			(clk_get_rate(clki->clk) > UFS_NOM_THRES_FREQ))
			err = ufshcd_dme_rmw(hba, PA_VS_CLK_CFG_REG_MASK,
			PA_VS_CLK_CFG_REG_MASK, PA_VS_CLK_CFG_REG);
		else
			err = ufshcd_dme_rmw(hba, PA_VS_CLK_CFG_REG_MASK,
					 PA_VS_CLK_CFG_REG_MASK1, PA_VS_CLK_CFG_REG);
	} else {
		err = ufshcd_dme_rmw(hba, PA_VS_CLK_CFG_REG_MASK,
				PA_VS_CLK_CFG_REG_MASK, PA_VS_CLK_CFG_REG);
	}

	if (err)
		goto out;

	if (!((host->hw_ver.major == 4) && (host->hw_ver.minor == 0) &&
	     (host->hw_ver.step == 0))) {
		err = ufshcd_dme_rmw(hba, DME_VS_CORE_CLK_CTRL_DME_HW_CGC_EN,
					DME_VS_CORE_CLK_CTRL_DME_HW_CGC_EN,
					DME_VS_CORE_CLK_CTRL);
	} else {
		dev_err(hba->dev, "%s: skipping DME_HW_CGC_EN set\n",
			__func__);
	}
out:
	return err;
}

static void ufs_qcom_force_mem_config(struct ufs_hba *hba)
{
	struct ufs_clk_info *clki;

	/*
	 * Configure the behavior of ufs clocks core and peripheral
	 * memory state when they are turned off.
	 * This configuration is required to allow retaining
	 * ICE crypto configuration (including keys) when
	 * core_clk_ice is turned off, and powering down
	 * non-ICE RAMs of host controller.
	 *
	 * This is applicable only to gcc clocks.
	 */
	list_for_each_entry(clki, &hba->clk_list_head, list) {

		/* skip it for non-gcc (rpmh) clocks */
		if (!strcmp(clki->name, "ref_clk"))
			continue;

		if (!strcmp(clki->name, "core_clk_ice") ||
			!strcmp(clki->name, "core_clk_ice_hw_ctl"))
			qcom_clk_set_flags(clki->clk, CLKFLAG_RETAIN_MEM);
		else
			qcom_clk_set_flags(clki->clk, CLKFLAG_NORETAIN_MEM);
		qcom_clk_set_flags(clki->clk, CLKFLAG_NORETAIN_PERIPH);
		qcom_clk_set_flags(clki->clk, CLKFLAG_PERIPH_OFF_CLEAR);
	}
}

static int ufs_qcom_hce_enable_notify(struct ufs_hba *hba,
				      enum ufs_notify_change_status status)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	int err = 0;

	switch (status) {
	case PRE_CHANGE:
		ufs_qcom_force_mem_config(hba);
		ufs_qcom_power_up_sequence(hba);
		/*
		 * The PHY PLL output is the source of tx/rx lane symbol
		 * clocks, hence, enable the lane clocks only after PHY
		 * is initialized.
		 */
		err = ufs_qcom_enable_lane_clks(host);
		/*
		 * ICE enable needs to be called before ufshcd_crypto_enable
		 * during resume as it is needed before reprogramming all
		 * keys. So moving it to PRE_CHANGE.
		 */
		ufs_qcom_ice_enable(host);
		break;
	case POST_CHANGE:
		/* check if UFS PHY moved from DISABLED to HIBERN8 */
		err = ufs_qcom_check_hibern8(hba);
		ufs_qcom_enable_hw_clk_gating(hba);
		break;
	default:
		dev_err(hba->dev, "%s: invalid status %d\n", __func__, status);
		err = -EINVAL;
		break;
	}

	ufs_qcom_log_str(host, "-,%d,%d\n", status, err);
	return err;
}

/*
 * Returns zero for success and non-zero in case of a failure
 */
static int __ufs_qcom_cfg_timers(struct ufs_hba *hba, u32 gear,
			       u32 hs, u32 rate, bool update_link_startup_timer,
			       bool is_pre_scale_up)
{
	int ret = 0;
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct ufs_clk_info *clki;
	u32 core_clk_period_in_ns;
	u32 tx_clk_cycles_per_us = 0;
	unsigned long core_clk_rate = 0;
	u32 core_clk_cycles_per_us = 0;

	static u32 pwm_fr_table[][2] = {
		{UFS_PWM_G1, 0x1},
		{UFS_PWM_G2, 0x1},
		{UFS_PWM_G3, 0x1},
		{UFS_PWM_G4, 0x1},
	};

	static u32 hs_fr_table_rA[][2] = {
		{UFS_HS_G1, 0x1F},
		{UFS_HS_G2, 0x3e},
		{UFS_HS_G3, 0x7D},
	};

	static u32 hs_fr_table_rB[][2] = {
		{UFS_HS_G1, 0x24},
		{UFS_HS_G2, 0x49},
		{UFS_HS_G3, 0x92},
	};

	/*
	 * The Qunipro controller does not use following registers:
	 * SYS1CLK_1US_REG, TX_SYMBOL_CLK_1US_REG, CLK_NS_REG &
	 * UFS_REG_PA_LINK_STARTUP_TIMER
	 * But UTP controller uses SYS1CLK_1US_REG register for Interrupt
	 * Aggregation logic / Auto hibern8 logic.
	 * It is mandatory to write SYS1CLK_1US_REG register on UFS host
	 * controller V4.0.0 onwards.
	*/
	if (ufs_qcom_cap_qunipro(host) &&
	    (!(ufshcd_is_intr_aggr_allowed(hba) ||
	       ufshcd_is_auto_hibern8_supported(hba) ||
	       host->hw_ver.major >= 4)))
		goto out;

	if (gear == 0) {
		dev_err(hba->dev, "%s: invalid gear = %d\n", __func__, gear);
		goto out_error;
	}

	list_for_each_entry(clki, &hba->clk_list_head, list) {
		if (!strcmp(clki->name, "core_clk")) {
			if (is_pre_scale_up)
				core_clk_rate = clki->max_freq;
			else
				core_clk_rate = clk_get_rate(clki->clk);
		}
	}

	/* If frequency is smaller than 1MHz, set to 1MHz */
	if (core_clk_rate < DEFAULT_CLK_RATE_HZ)
		core_clk_rate = DEFAULT_CLK_RATE_HZ;

	core_clk_cycles_per_us = core_clk_rate / USEC_PER_SEC;

	/*
	 * As per HPG, some target needs SYS1CLK_1US_REG as Fs/2 where
	 * Fs is system clock frequency in MHz if operating beyond 300Mhz.
	 * Add 1 to it if its not multiple of 2.
	 */
	if (core_clk_rate > UFS_NOM_THRES_FREQ && host->turbo_additional_conf_req)
		core_clk_cycles_per_us = core_clk_cycles_per_us/2 +
				(core_clk_cycles_per_us % 2);

	if (ufshcd_readl(hba, REG_UFS_SYS1CLK_1US) != core_clk_cycles_per_us) {
		ufshcd_writel(hba, core_clk_cycles_per_us, REG_UFS_SYS1CLK_1US);
		/*
		 * make sure above write gets applied before we return from
		 * this function.
		 */
		mb();
	}

	if (ufs_qcom_cap_qunipro(host))
		goto out;

	core_clk_period_in_ns = NSEC_PER_SEC / core_clk_rate;
	core_clk_period_in_ns <<= OFFSET_CLK_NS_REG;
	core_clk_period_in_ns &= MASK_CLK_NS_REG;

	switch (hs) {
	case FASTAUTO_MODE:
	case FAST_MODE:
		if (rate == PA_HS_MODE_A) {
			if (gear > ARRAY_SIZE(hs_fr_table_rA)) {
				dev_err(hba->dev,
					"%s: index %d exceeds table size %zu\n",
					__func__, gear,
					ARRAY_SIZE(hs_fr_table_rA));
				goto out_error;
			}
			tx_clk_cycles_per_us = hs_fr_table_rA[gear-1][1];
		} else if (rate == PA_HS_MODE_B) {
			if (gear > ARRAY_SIZE(hs_fr_table_rB)) {
				dev_err(hba->dev,
					"%s: index %d exceeds table size %zu\n",
					__func__, gear,
					ARRAY_SIZE(hs_fr_table_rB));
				goto out_error;
			}
			tx_clk_cycles_per_us = hs_fr_table_rB[gear-1][1];
		} else {
			dev_err(hba->dev, "%s: invalid rate = %d\n",
				__func__, rate);
			goto out_error;
		}
		break;
	case SLOWAUTO_MODE:
	case SLOW_MODE:
		if (gear > ARRAY_SIZE(pwm_fr_table)) {
			dev_err(hba->dev,
					"%s: index %d exceeds table size %zu\n",
					__func__, gear,
					ARRAY_SIZE(pwm_fr_table));
			goto out_error;
		}
		tx_clk_cycles_per_us = pwm_fr_table[gear-1][1];
		break;
	case UNCHANGED:
	default:
		dev_err(hba->dev, "%s: invalid mode = %d\n", __func__, hs);
		goto out_error;
	}

	if (ufshcd_readl(hba, REG_UFS_TX_SYMBOL_CLK_NS_US) !=
	    (core_clk_period_in_ns | tx_clk_cycles_per_us)) {
		/* this register 2 fields shall be written at once */
		ufshcd_writel(hba, core_clk_period_in_ns | tx_clk_cycles_per_us,
			      REG_UFS_TX_SYMBOL_CLK_NS_US);
		/*
		 * make sure above write gets applied before we return from
		 * this function.
		 */
		mb();
	}

	if (update_link_startup_timer) {
		ufshcd_writel(hba, ((core_clk_rate / MSEC_PER_SEC) * 100),
			      REG_UFS_PA_LINK_STARTUP_TIMER);
		/*
		 * make sure that this configuration is applied before
		 * we return
		 */
		mb();
	}
	goto out;

out_error:
	ret = -EINVAL;
out:
	return ret;
}

static int ufs_qcom_cfg_timers(struct ufs_hba *hba, u32 gear,
			       u32 hs, u32 rate, bool update_link_startup_timer)
{
	return  __ufs_qcom_cfg_timers(hba, gear, hs, rate,
				      update_link_startup_timer, false);
}

static int ufs_qcom_set_dme_vs_core_clk_ctrl_max_freq_mode(struct ufs_hba *hba)
{
	struct ufs_clk_info *clki;
	struct list_head *head = &hba->clk_list_head;
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	u32 max_freq = 0;
	int err = 0;

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk) &&
		    (!strcmp(clki->name, "core_clk_unipro"))) {
			max_freq = clki->max_freq;
			/* clk mode change from TURBO to NOM */
			if ((host->clk_curr_mode == TURBO) &&
				(host->clk_next_mode == NOM))
				max_freq =  UFS_NOM_THRES_FREQ;
			break;
		}
	}

	switch (max_freq) {
	case 300000000:
		err = ufs_qcom_set_dme_vs_core_clk_ctrl_clear_div(hba, 300, 12);
		break;
	case 150000000:
		err = ufs_qcom_set_dme_vs_core_clk_ctrl_clear_div(hba, 150, 6);
		break;
	case 600000000:
		err = ufs_qcom_set_dme_vs_core_clk_ctrl_clear_div(hba, 300, 12);
		break;
	case 550000000:
		err = ufs_qcom_set_dme_vs_core_clk_ctrl_clear_div(hba, 275, 11);
		break;
	case 806400000:
		err = ufs_qcom_set_dme_vs_core_clk_ctrl_clear_div(hba, 202, 9);
		break;
	case 850000000:
		err = ufs_qcom_set_dme_vs_core_clk_ctrl_clear_div(hba, 213, 9);
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

/**
 * ufs_qcom_bypass_cfgready_signal - Tunes PA_VS_CONFIG_REG1 and
 * PA_VS_CONFIG_REG2 vendor specific attributes of local unipro
 * to bypass CFGREADY signal on Config interface between UFS
 * controller and PHY.
 *
 * The issue is related to config signals sampling from PHY
 * to controller. The PHY signals which are driven by 150MHz
 * clock and sampled by 300MHz instead of 150MHZ.
 *
 * The issue will be seen when only one of tx_cfg_rdyn_0
 * and tx_cfg_rdyn_1 is 0 around sampling clock edge and
 * if timing is not met as timing margin for some devices is
 * very less in one of the corner.
 *
 * To workaround this issue, controller should bypass the Cfgready
 * signal(TX_CFGREADY and RX_CFGREDY) because controller still wait
 * for another signal tx_savestatusn which will serve same purpose.
 *
 * The corresponding HW CR: 'QCTDD06985523' UFS HSG4 test fails
 * in SDF MAX GLS is linked to this issue.
 */
static int ufs_qcom_bypass_cfgready_signal(struct ufs_hba *hba)
{
	int err = 0;
	u32 pa_vs_config_reg1;
	u32 pa_vs_config_reg2;
	u32 mask;

	err = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_VS_CONFIG_REG1),
			&pa_vs_config_reg1);
	if (err)
		goto out;

	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_VS_CONFIG_REG1),
			(pa_vs_config_reg1 | BIT_TX_EOB_COND));
	if (err)
		goto out;

	err = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_VS_CONFIG_REG2),
			&pa_vs_config_reg2);
	if (err)
		goto out;

	mask = (BIT_RX_EOB_COND | BIT_LINKCFG_WAIT_LL1_RX_CFG_RDY |
					H8_ENTER_COND_MASK);
	pa_vs_config_reg2 = (pa_vs_config_reg2 & ~mask) |
				(0x2 << H8_ENTER_COND_OFFSET);

	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_VS_CONFIG_REG2),
			(pa_vs_config_reg2));
out:
	return err;
}

static void ufs_qcom_dump_attribs(struct ufs_hba *hba)
{
	int ret;
	int attrs[] = {0x15a0, 0x1552, 0x1553, 0x1554,
		       0x1555, 0x1556, 0x1557, 0x155a,
		       0x155b, 0x155c, 0x155d, 0x155e,
		       0x155f, 0x1560, 0x1561, 0x1568,
		       0x1569, 0x156a, 0x1571, 0x1580,
		       0x1581, 0x1583, 0x1584, 0x1585,
		       0x1586, 0x1587, 0x1590, 0x1591,
		       0x15a1, 0x15a2, 0x15a3, 0x15a4,
		       0x15a5, 0x15a6, 0x15a7, 0x15a8,
		       0x15a9, 0x15aa, 0x15ab, 0x15c0,
		       0x15c1, 0x15c2, 0x15d0, 0x15d1,
		       0x15d2, 0x15d3, 0x15d4, 0x15d5,
	};
	int cnt = ARRAY_SIZE(attrs);
	int i = 0, val;

	for (; i < cnt; i++) {
		ret = ufshcd_dme_get(hba, UIC_ARG_MIB(attrs[i]), &val);
		if (ret) {
			dev_err(hba->dev, "Failed reading: 0x%04x, ret:%d\n",
				attrs[i], ret);
			continue;
		}
		dev_err(hba->dev, "0x%04x: %d\n", attrs[i], val);
	}
}

static void ufs_qcom_validate_link_params(struct ufs_hba *hba)
{
	int val = 0;
	bool err = false;

	WARN_ON(ufs_qcom_get_connected_tx_lanes(hba, &val));
	if (val != hba->lanes_per_direction) {
		dev_err(hba->dev, "%s: Tx lane mismatch [config,reported] [%d,%d]\n",
			__func__, hba->lanes_per_direction, val);
		WARN_ON(1);
		err = true;
	}

	val = 0;
	WARN_ON(ufs_qcom_get_connected_rx_lanes(hba, &val));
	if (val != hba->lanes_per_direction) {
		dev_err(hba->dev, "%s: Rx lane mismatch [config,reported] [%d,%d]\n",
			__func__, hba->lanes_per_direction, val);
		WARN_ON(1);
		err = true;
	}

	if (err)
		ufs_qcom_dump_attribs(hba);
}

static void ufs_qcom_apply_turbo_setting(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	int err;

	if (host->turbo_unipro_attr_applied)
		return;

	if (host->turbo_additional_conf_req)
		ufshcd_rmwl(host->hba, TEST_BUS_CTRL_2_HCI_SEL_TURBO_MASK,
		TEST_BUS_CTRL_2_HCI_SEL_TURBO,
		UFS_TEST_BUS_CTRL_2);

	err = ufshcd_dme_rmw(hba, PA_VS_CLK_CFG_REG_MASK_TURBO,
			ATTR_HW_CGC_EN_TURBO, PA_VS_CLK_CFG_REG);
	if (err)
		dev_err(hba->dev, "%s apply of turbo setting failed\n", __func__);

	host->turbo_unipro_attr_applied = true;
}

static void ufs_qcom_remove_turbo_setting(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	int err;

	if (!host->turbo_unipro_attr_applied)
		return;

	if (host->turbo_additional_conf_req)
		ufshcd_rmwl(host->hba, TEST_BUS_CTRL_2_HCI_SEL_TURBO_MASK,
		TEST_BUS_CTRL_2_HCI_SEL_NONTURBO,
		UFS_TEST_BUS_CTRL_2);

	err = ufshcd_dme_rmw(hba, PA_VS_CLK_CFG_REG_MASK_TURBO,
			ATTR_HW_CGC_EN_NON_TURBO, PA_VS_CLK_CFG_REG);
	if (err)
		dev_err(hba->dev, "%s remove of turbo setting failed\n", __func__);

	host->turbo_unipro_attr_applied = false;
}

static int ufs_qcom_link_startup_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status status)
{
	int err = 0;
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct phy *phy = host->generic_phy;
	struct device *dev = hba->dev;
	u32 temp;

	switch (status) {
	case PRE_CHANGE:
		if (strlen(android_boot_dev) && strcmp(android_boot_dev, dev_name(dev))) {
			return -ENODEV;
		}

		if (host->ml_scale_sup)
			ufs_qcom_apply_turbo_setting(hba);

		if (ufs_qcom_cfg_timers(hba, UFS_PWM_G1, SLOWAUTO_MODE,
					0, true)) {
			dev_err(hba->dev, "%s: ufs_qcom_cfg_timers() failed\n",
				__func__);
			err = -EINVAL;
			goto out;
		}

		ufs_qcom_phy_ctrl_rx_linecfg(phy, true);

		if (ufs_qcom_cap_qunipro(host)) {
			err = ufs_qcom_set_dme_vs_core_clk_ctrl_max_freq_mode(
				hba);
			if (err)
				goto out;
		}

		err = ufs_qcom_enable_hw_clk_gating(hba);
		if (err)
			goto out;

		/*
		 * Controller checks ICE configuration error without
		 * checking if the command is SCSI command
		 */
		temp = readl_relaxed(host->dev_ref_clk_ctrl_mmio);
		temp |= BIT(31);
		writel_relaxed(temp, host->dev_ref_clk_ctrl_mmio);
		/* ensure that UTP_SCASI_CHECK_DIS is enabled before link startup */
		wmb();

		/*
		 * Some UFS devices (and may be host) have issues if LCC is
		 * enabled. So we are setting PA_Local_TX_LCC_Enable to 0
		 * before link startup which will make sure that both host
		 * and device TX LCC are disabled once link startup is
		 * completed.
		 */
		if (ufshcd_get_local_unipro_ver(hba) != UFS_UNIPRO_VER_1_41)
			err = ufshcd_disable_host_tx_lcc(hba);
		if (err)
			goto out;

		if (host->bypass_g4_cfgready)
			err = ufs_qcom_bypass_cfgready_signal(hba);
		break;
	case POST_CHANGE:
		ufs_qcom_link_startup_post_change(hba);
		ufs_qcom_validate_link_params(hba);
		break;
	default:
		break;
	}

out:
	ufs_qcom_log_str(host, "*,%d,%d\n", status, err);
	return err;
}

static int ufs_qcom_config_vreg(struct device *dev,
		struct ufs_vreg *vreg, bool on)
{
	int ret = 0;
	struct regulator *reg;
	int min_uV, uA_load;

	if (!vreg) {
		WARN_ON(1);
		ret = -EINVAL;
		goto out;
	}

	reg = vreg->reg;
	if (regulator_count_voltages(reg) > 0) {
		uA_load = on ? vreg->max_uA : 0;
		ret = regulator_set_load(vreg->reg, uA_load);
		if (ret)
			goto out;
		if (vreg->min_uV && vreg->max_uV) {
			min_uV = on ? vreg->min_uV : 0;
			ret = regulator_set_voltage(reg, min_uV, vreg->max_uV);
			if (ret) {
				dev_err(dev, "%s: %s failed, err=%d\n",
					__func__, vreg->name, ret);
				goto out;
			}
		}
	}
out:
	return ret;
}

static int ufs_qcom_enable_vreg(struct device *dev, struct ufs_vreg *vreg)
{
	int ret = 0;

	if (vreg->enabled)
		return ret;

	ret = ufs_qcom_config_vreg(dev, vreg, true);
	if (ret)
		goto out;

	ret = regulator_enable(vreg->reg);
	if (ret)
		goto out;

	vreg->enabled = true;
out:
	return ret;
}

static int ufs_qcom_disable_vreg(struct device *dev, struct ufs_vreg *vreg)
{
	int ret = 0;

	if (!vreg->enabled)
		return ret;

	ret = regulator_disable(vreg->reg);
	if (ret)
		goto out;

	ret = ufs_qcom_config_vreg(dev, vreg, false);
	if (ret)
		goto out;

	vreg->enabled = false;
out:
	return ret;
}


static int ufs_qcom_mod_min_cpufreq(unsigned int cpu, s32 new_val)
{
	int ret = 0;
	struct freq_qos_request *qos_req;

	get_online_cpus();
	if (cpu_online(cpu)) {
		qos_req = &per_cpu(qos_min_req, cpu);
		ret = freq_qos_update_request(qos_req, new_val);
	}
	put_online_cpus();
	return ret;
}

static int ufs_qcom_init_cpu_minfreq_req(struct ufs_qcom_host *host,
					unsigned int cpu)
{
	int ret;
	struct cpufreq_policy *policy;
	struct freq_qos_request *req;

	policy = cpufreq_cpu_get(cpu);
	if (!policy) {
		dev_err(host->hba->dev, "Failed to get cpu(%u)freq policy\n",
			cpu);
		return -EINVAL;
	}

	req = &per_cpu(qos_min_req, cpu);
	ret = freq_qos_add_request(&policy->constraints, req, FREQ_QOS_MIN,
				FREQ_QOS_MIN_DEFAULT_VALUE);
	if (ret < 0)
		dev_err(host->hba->dev, "Failed to add freq qos req\n");
	cpufreq_cpu_put(policy);

	return ret;
}

static void ufs_qcom_set_affinity_hint(struct ufs_hba *hba, bool prime)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	unsigned int set = 0;
	unsigned int clear = 0;
	int ret;
	cpumask_t *affinity_mask;

	if (prime) {
		set = IRQ_NO_BALANCING;
		affinity_mask = &host->perf_mask;
	} else {
		clear = IRQ_NO_BALANCING;
		affinity_mask = &host->def_mask;
	}

	irq_modify_status(hba->irq, clear, set);
	ret = irq_set_affinity_hint(hba->irq, affinity_mask);
	if (ret < 0)
		dev_err(host->hba->dev, "prime=%d, err=%d\n", prime, ret);
}

static void ufs_qcom_toggle_pri_affinity(struct ufs_hba *hba, bool on)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	if (on && atomic_read(&host->therm_mitigation))
		return;

	atomic_set(&host->hi_pri_en, on);
	ufs_qcom_set_affinity_hint(hba, on);
}

static void ufs_qcom_cpufreq_dwork(struct work_struct *work)
{
	struct ufs_qcom_host *host = container_of(to_delayed_work(work),
							struct ufs_qcom_host,
							fwork);
	unsigned long cur_thres = atomic_read(&host->num_reqs_threshold);
	unsigned int freq_val = -1;
	int err = -1;

	atomic_set(&host->num_reqs_threshold, 0);

	if (cur_thres > NUM_REQS_HIGH_THRESH && !host->cur_freq_vote) {
		freq_val = host->max_cpu_scale_freq;
		if (host->irq_affinity_support)
			ufs_qcom_toggle_pri_affinity(host->hba, true);
	} else if (cur_thres < NUM_REQS_LOW_THRESH && host->cur_freq_vote) {
		freq_val = host->min_cpu_scale_freq;
		if (host->irq_affinity_support)
			ufs_qcom_toggle_pri_affinity(host->hba, false);
	}

	if (freq_val == -1)
		goto out;

	err = ufs_qcom_mod_min_cpufreq(host->config_cpu, freq_val);
	if (err < 0)
		dev_err(host->hba->dev, "fail set cpufreq-fmin to %d: %u\n",
				err, freq_val);
	else if (freq_val == host->max_cpu_scale_freq)
		host->cur_freq_vote = true;
	else if (freq_val == host->min_cpu_scale_freq)
		host->cur_freq_vote = false;
	dev_dbg(host->hba->dev, "cur_freq_vote=%d,freq_val=%u,cth=%u\n",
		host->cur_freq_vote, freq_val, cur_thres);
out:
	/*
	 * See ufs_qcom_suspend(), ufs_qcom_resume(), ufs_qcom_clk_scale_notify()
	 *
	 * Work rearms itself as per below table:
	 * cur_freq_vote | scaled_up | host_active |	Result
	 *  0			0	0		Exit work
	 *  0			0	1		Exit work
	 *  0			1	0		Exit work
	 *  0			1	1		Requeue work
	 *  1			0	0		Requeue work
	 *  1			0	1		Requeue work
	 *  1			1	0		Requeue work
	 *  1			1	1		Requeue work
	 */
	mutex_lock(&host->cpufreq_lock);
	if ((!host->cur_freq_vote && !(!!atomic_read(&host->scale_up)) &&
	    !host->active) ||
	    (!host->cur_freq_vote && !(!!atomic_read(&host->scale_up)) &&
	     host->active) ||
	     (!host->cur_freq_vote && !!atomic_read(&host->scale_up) &&
	      !host->active)) {
		mutex_unlock(&host->cpufreq_lock);
		return;
	}

	queue_delayed_work(host->ufs_qos->workq, &host->fwork,
			   msecs_to_jiffies(UFS_QCOM_LOAD_MON_DLY_MS));
	mutex_unlock(&host->cpufreq_lock);
}

static int add_group_qos(struct qos_cpu_group *qcg, enum constraint type)
{
	int cpu, err;
	struct dev_pm_qos_request *qos_req = qcg->qos_req;

	for_each_cpu(cpu, &qcg->mask) {
		dev_dbg(qcg->host->hba->dev, "%s: cpu: %d | mask: 0x%08x | assoc-qos-req: 0x%08x\n",
			__func__, cpu, qcg->mask, qos_req);
		memset(qos_req, 0,
		       sizeof(struct dev_pm_qos_request));
		err = dev_pm_qos_add_request(get_cpu_device(cpu),
					     qos_req,
					     DEV_PM_QOS_RESUME_LATENCY,
					     type);
		if (err < 0)
			return err;
		qos_req++;
	}
	return 0;
}

static int remove_group_qos(struct qos_cpu_group *qcg)
{
	int err, cpu;
	struct dev_pm_qos_request *qos_req = qcg->qos_req;

	for_each_cpu(cpu, &qcg->mask) {
		if (!dev_pm_qos_request_active(qos_req)) {
			qos_req++;
			continue;
		}
		err = dev_pm_qos_remove_request(qos_req);
		if (err < 0)
			return err;
		qos_req++;
	}
	return 0;
}

static void ufs_qcom_device_reset_ctrl(struct ufs_hba *hba, bool asserted)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	/* reset gpio is optional */
	if (!host->device_reset)
		return;

	gpiod_set_value_cansleep(host->device_reset, asserted);
}

static int ufs_qcom_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	int err = 0;

	/*
	 * If UniPro link is not active or OFF, PHY ref_clk, main PHY analog
	 * power rail and low noise analog power rail for PLL can be
	 * switched off.
	 */
	if (!ufs_qcom_is_link_active(hba)) {
		ufs_qcom_disable_lane_clks(host);
		if (host->vddp_ref_clk && ufs_qcom_is_link_off(hba))
			err = ufs_qcom_disable_vreg(hba->dev,
					host->vddp_ref_clk);
		if (host->vccq_parent && !hba->auto_bkops_enabled)
			ufs_qcom_disable_vreg(hba->dev, host->vccq_parent);
		if (!err)
			err = ufs_qcom_unvote_qos_all(hba);
	}

	if (!err && ufs_qcom_is_link_off(hba) && host->device_reset)
		ufs_qcom_device_reset_ctrl(hba, true);

	ufs_qcom_log_str(host, "&,%d,%d,%d,%d,%d,%d\n",
			pm_op, hba->rpm_lvl, hba->spm_lvl, hba->uic_link_state,
			hba->curr_dev_pwr_mode, err);
	ufs_qcom_ice_disable(host);

	/* Refer ufs_qcom_cpufreq_dwork() */
	mutex_lock(&host->cpufreq_lock);
	host->active = false;
	mutex_unlock(&host->cpufreq_lock);
	return err;
}

static int ufs_qcom_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	int err;

	if (host->vddp_ref_clk && (hba->rpm_lvl > UFS_PM_LVL_3 ||
				   hba->spm_lvl > UFS_PM_LVL_3))
		ufs_qcom_enable_vreg(hba->dev,
				      host->vddp_ref_clk);

	if (host->vccq_parent)
		ufs_qcom_enable_vreg(hba->dev, host->vccq_parent);

	err = ufs_qcom_enable_lane_clks(host);
	if (err)
		return err;

	ufs_qcom_log_str(host, "$,%d,%d,%d,%d,%d,%d\n",
			pm_op, hba->rpm_lvl, hba->spm_lvl, hba->uic_link_state,
			hba->curr_dev_pwr_mode, err);

	if (!host->cpufreq_dis) {
		/* Refer ufs_qcom_cpufreq_dwork() */
		mutex_lock(&host->cpufreq_lock);
		host->active = true;
		/* Reschedule the work if it exited when the clocks were scaled up */
		if (!!atomic_read(&host->scale_up))
			queue_delayed_work(host->fworkq, &host->fwork, 0);
		mutex_unlock(&host->cpufreq_lock);
	}

	return 0;
}

static int ufs_qcom_get_bus_vote(struct ufs_qcom_host *host,
		const char *speed_mode)
{
	struct device *dev = host->hba->dev;
	struct device_node *np = dev->of_node;
	int err;
	const char *key = "qcom,bus-vector-names";

	if (!speed_mode) {
		err = -EINVAL;
		goto out;
	}

	if (host->bus_vote.is_max_bw_needed && !!strcmp(speed_mode, "MIN"))
		err = of_property_match_string(np, key, "MAX");
	else
		err = of_property_match_string(np, key, speed_mode);

out:
	if (err < 0)
		dev_err(dev, "%s: Invalid %s mode %d\n",
				__func__, speed_mode, err);
	return err;
}

static void ufs_qcom_get_speed_mode(struct ufs_pa_layer_attr *p, char *result)
{
	int gear = max_t(u32, p->gear_rx, p->gear_tx);
	int lanes = max_t(u32, p->lane_rx, p->lane_tx);
	int pwr;

	/* default to PWM Gear 1, Lane 1 if power mode is not initialized */
	if (!gear)
		gear = 1;

	if (!lanes)
		lanes = 1;

	if (!p->pwr_rx && !p->pwr_tx) {
		pwr = SLOWAUTO_MODE;
		snprintf(result, BUS_VECTOR_NAME_LEN, "MIN");
	} else if (p->pwr_rx == FAST_MODE || p->pwr_rx == FASTAUTO_MODE ||
		 p->pwr_tx == FAST_MODE || p->pwr_tx == FASTAUTO_MODE) {
		pwr = FAST_MODE;
		snprintf(result, BUS_VECTOR_NAME_LEN, "%s_R%s_G%d_L%d", "HS",
			 p->hs_rate == PA_HS_MODE_B ? "B" : "A", gear, lanes);
	} else {
		pwr = SLOW_MODE;
		snprintf(result, BUS_VECTOR_NAME_LEN, "%s_G%d_L%d",
			 "PWM", gear, lanes);
	}
}

static int ufs_qcom_get_ib_ab(struct ufs_qcom_host *host, int index,
			      struct qcom_bus_vectors *ufs_ddr_vec,
			      struct qcom_bus_vectors *cpu_ufs_vec)
{
	struct qcom_bus_path *usecase;

	if (!host->qbsd)
		return -EINVAL;

	if (index > host->qbsd->num_usecase)
		return -EINVAL;

	usecase = host->qbsd->usecase;

	/*
	 *
	 * usecase:0  usecase:0
	 * ufs->ddr   cpu->ufs
	 * |vec[0&1] | vec[2&3]|
	 * +----+----+----+----+
	 * | ab | ib | ab | ib |
	 * |----+----+----+----+
	 * .
	 * .
	 * .
	 * usecase:n  usecase:n
	 * ufs->ddr   cpu->ufs
	 * |vec[0&1] | vec[2&3]|
	 * +----+----+----+----+
	 * | ab | ib | ab | ib |
	 * |----+----+----+----+
	 */

	/* index refers to offset in usecase */
	ufs_ddr_vec->ab = usecase[index].vec[0].ab;
	ufs_ddr_vec->ib = usecase[index].vec[0].ib;

	cpu_ufs_vec->ab = usecase[index].vec[1].ab;
	cpu_ufs_vec->ib = usecase[index].vec[1].ib;

	return 0;
}

static int __ufs_qcom_set_bus_vote(struct ufs_qcom_host *host, int vote)
{
	int err = 0;
	struct qcom_bus_scale_data *d = host->qbsd;
	struct qcom_bus_vectors path0, path1;
	struct device *dev = host->hba->dev;

	err = ufs_qcom_get_ib_ab(host, vote, &path0, &path1);
	if (err) {
		dev_err(dev, "Error: failed (%d) to get ib/ab\n",
			err);
		return err;
	}

	dev_dbg(dev, "Setting vote: %d: ufs-ddr: ab: %llu ib: %llu\n", vote,
		path0.ab, path0.ib);
	err = icc_set_bw(d->ufs_ddr, path0.ab, path0.ib);
	if (err) {
		dev_err(dev, "Error: failed setting (%s) bus vote\n", err,
			UFS_DDR);
		return err;
	}

	dev_dbg(dev, "Setting: cpu-ufs: ab: %llu ib: %llu\n", path1.ab,
		path1.ib);
	err = icc_set_bw(d->cpu_ufs, path1.ab, path1.ib);
	if (err) {
		dev_err(dev, "Error: failed setting (%s) bus vote\n", err,
			CPU_UFS);
		return err;
	}

	host->bus_vote.curr_vote = vote;

	return err;
}

static int ufs_qcom_update_bus_bw_vote(struct ufs_qcom_host *host)
{
	int vote;
	int err = 0;
	char mode[BUS_VECTOR_NAME_LEN];

	ufs_qcom_get_speed_mode(&host->dev_req_params, mode);

	vote = ufs_qcom_get_bus_vote(host, mode);
	if (vote >= 0)
		err = __ufs_qcom_set_bus_vote(host, vote);
	else
		err = vote;

	if (err)
		dev_err(host->hba->dev, "%s: failed %d\n", __func__, err);
	else
		host->bus_vote.saved_vote = vote;
	return err;
}

static int ufs_qcom_set_bus_vote(struct ufs_hba *hba, bool on)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	int vote, err;

	/*
	 * In case ufs_qcom_init() is not yet done, simply ignore.
	 * This ufs_qcom_set_bus_vote() shall be called from
	 * ufs_qcom_init() after init is done.
	 */
	if (!host)
		return 0;

	if (on) {
		vote = host->bus_vote.saved_vote;
		if (vote == host->bus_vote.min_bw_vote)
			ufs_qcom_update_bus_bw_vote(host);
	} else {
		vote = host->bus_vote.min_bw_vote;
	}

	err = __ufs_qcom_set_bus_vote(host, vote);
	if (err)
		dev_err(hba->dev, "%s: set bus vote failed %d\n",
				 __func__, err);

	return err;
}

static ssize_t
show_ufs_to_mem_max_bus_bw(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			host->bus_vote.is_max_bw_needed);
}

static ssize_t
store_ufs_to_mem_max_bus_bw(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	uint32_t value;

	if (!kstrtou32(buf, 0, &value)) {
		host->bus_vote.is_max_bw_needed = !!value;
		ufs_qcom_update_bus_bw_vote(host);
	}

	return count;
}

static struct qcom_bus_scale_data *ufs_qcom_get_bus_scale_data(struct device
							       *dev)

{
	struct platform_device *pdev = to_platform_device(dev);
	struct device_node *of_node = dev->of_node;
	struct qcom_bus_scale_data *qsd;
	struct qcom_bus_path *usecase = NULL;
	int ret = 0, i = 0, j, num_paths, len;
	const uint32_t *vec_arr = NULL;
	bool mem_err = false;

	if (!pdev) {
		dev_err(dev, "Null platform device!\n");
		return NULL;
	}

	qsd = devm_kzalloc(dev, sizeof(struct qcom_bus_scale_data), GFP_KERNEL);
	if (!qsd)
		return NULL;

	ret = of_property_read_string(of_node, "qcom,ufs-bus-bw,name",
				      &qsd->name);
	if (ret) {
		dev_err(dev, "Error: (%d) Bus name missing!\n", ret);
		return NULL;
	}

	ret = of_property_read_u32(of_node, "qcom,ufs-bus-bw,num-cases",
		&qsd->num_usecase);
	if (ret) {
		pr_err("Error: num-usecases not found\n");
		goto err;
	}

	usecase = devm_kzalloc(dev, (sizeof(struct qcom_bus_path) *
				   qsd->num_usecase), GFP_KERNEL);
	if (!usecase)
		return NULL;

	ret = of_property_read_u32(of_node, "qcom,ufs-bus-bw,num-paths",
				   &num_paths);
	if (ret) {
		pr_err("Error: num_paths not found\n");
		return NULL;
	}

	vec_arr = of_get_property(of_node, "qcom,ufs-bus-bw,vectors-KBps",
				  &len);
	if (vec_arr == NULL) {
		pr_err("Error: Vector array not found\n");
		return NULL;
	}

	for (i = 0; i < qsd->num_usecase; i++) {
		usecase[i].num_paths = num_paths;
		usecase[i].vec = devm_kzalloc(dev, num_paths *
					      sizeof(struct qcom_bus_vectors),
					      GFP_KERNEL);
		if (!usecase[i].vec) {
			mem_err = true;
			dev_err(dev, "Error: Failed to alloc mem for vectors\n");
			goto err;
		}

		for (j = 0; j < num_paths; j++) {
			uint32_t tab;
			int idx = ((i * num_paths) + j) * 2;

			tab = vec_arr[idx];
			usecase[i].vec[j].ab = ((tab & 0xff000000) >> 24) |
				((tab & 0x00ff0000) >> 8) |
				((tab & 0x0000ff00) << 8) | (tab << 24);

			tab = vec_arr[idx + 1];
			usecase[i].vec[j].ib = ((tab & 0xff000000) >> 24) |
				((tab & 0x00ff0000) >> 8) |
				((tab & 0x0000ff00) << 8) | (tab << 24);

			dev_dbg(dev, "ab: %llu ib:%llu [i]: %d [j]: %d\n",
				usecase[i].vec[j].ab, usecase[i].vec[j].ib, i,
				j);
		}
	}

	qsd->usecase = usecase;
	return qsd;
err:
	return NULL;
}

static int ufs_qcom_bus_register(struct ufs_qcom_host *host)
{
	int err = 0;
	struct device *dev = host->hba->dev;
	struct qcom_bus_scale_data *qsd;

	qsd = ufs_qcom_get_bus_scale_data(dev);
	if (!qsd) {
		dev_err(dev, "Failed: getting bus_scale data\n");
		return 0;
	}
	host->qbsd = qsd;

	qsd->ufs_ddr = of_icc_get(dev, UFS_DDR);
	if (IS_ERR(qsd->ufs_ddr)) {
		dev_err(dev, "Error: (%d) failed getting %s path\n",
			PTR_ERR(qsd->ufs_ddr), UFS_DDR);
		return PTR_ERR(qsd->ufs_ddr);
	}

	qsd->cpu_ufs = of_icc_get(dev, CPU_UFS);
	if (IS_ERR(qsd->cpu_ufs)) {
		dev_err(dev, "Error: (%d) failed getting %s path\n",
			PTR_ERR(qsd->cpu_ufs), CPU_UFS);
		return PTR_ERR(qsd->cpu_ufs);
	}

	/* cache the vote index for minimum and maximum bandwidth */
	host->bus_vote.min_bw_vote = ufs_qcom_get_bus_vote(host, "MIN");
	host->bus_vote.max_bw_vote = ufs_qcom_get_bus_vote(host, "MAX");

	host->bus_vote.max_bus_bw.show = show_ufs_to_mem_max_bus_bw;
	host->bus_vote.max_bus_bw.store = store_ufs_to_mem_max_bus_bw;
	sysfs_attr_init(&host->bus_vote.max_bus_bw.attr);
	host->bus_vote.max_bus_bw.attr.name = "max_bus_bw";
	host->bus_vote.max_bus_bw.attr.mode = S_IRUGO | S_IWUSR;
	err = device_create_file(dev, &host->bus_vote.max_bus_bw);
	if (err)
		dev_err(dev, "Error: (%d) Failed to create sysfs entries\n",
			err);
	return 0;
}

static void ufs_qcom_dev_ref_clk_ctrl(struct ufs_qcom_host *host, bool enable)
{
	if (host->dev_ref_clk_ctrl_mmio &&
	    (enable ^ host->is_dev_ref_clk_enabled)) {
		u32 temp = readl_relaxed(host->dev_ref_clk_ctrl_mmio);

		if (enable)
			temp |= host->dev_ref_clk_en_mask;
		else
			temp &= ~host->dev_ref_clk_en_mask;

		/*
		 * If we are here to disable this clock it might be immediately
		 * after entering into hibern8 in which case we need to make
		 * sure that device ref_clk is active for specific time after
		 * hibern8 enter.
		 */
		if (!enable) {
			unsigned long gating_wait;

			gating_wait = host->hba->dev_info.clk_gating_wait_us;
			if (!gating_wait) {
				udelay(1);
			} else {
				/*
				 * bRefClkGatingWaitTime defines the minimum
				 * time for which the reference clock is
				 * required by device during transition from
				 * HS-MODE to LS-MODE or HIBERN8 state. Give it
				 * more delay to be on the safe side.
				 */
				gating_wait += 10;
				usleep_range(gating_wait, gating_wait + 10);
			}
		}

		writel_relaxed(temp, host->dev_ref_clk_ctrl_mmio);

		/*
		 * Make sure the write to ref_clk reaches the destination and
		 * not stored in a Write Buffer (WB).
		 */
		readl(host->dev_ref_clk_ctrl_mmio);

		/*
		 * If we call hibern8 exit after this, we need to make sure that
		 * device ref_clk is stable for a given time before the hibern8
		 * exit command.
		 */
		if (enable)
			usleep_range(50, 60);

		host->is_dev_ref_clk_enabled = enable;
	}
}

static int ufs_qcom_pwr_change_notify(struct ufs_hba *hba,
				enum ufs_notify_change_status status,
				struct ufs_pa_layer_attr *dev_max_params,
				struct ufs_pa_layer_attr *dev_req_params)
{
	u32 val;
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct phy *phy = host->generic_phy;
	struct ufs_qcom_dev_params ufs_qcom_cap;
	int ret = 0;

	if (!dev_req_params) {
		pr_err("%s: incoming dev_req_params is NULL\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	switch (status) {
	case PRE_CHANGE:
		ufs_qcom_cap.hs_rx_gear = host->limit_rx_hs_gear;
		ufs_qcom_cap.hs_tx_gear = host->limit_tx_hs_gear;
		ufs_qcom_cap.pwm_tx_gear = host->limit_tx_pwm_gear;
		ufs_qcom_cap.pwm_rx_gear = host->limit_rx_pwm_gear;

		ufs_qcom_cap.tx_lanes = UFS_QCOM_LIMIT_NUM_LANES_TX;
		ufs_qcom_cap.rx_lanes = UFS_QCOM_LIMIT_NUM_LANES_RX;

		ufs_qcom_cap.rx_pwr_pwm = UFS_QCOM_LIMIT_RX_PWR_PWM;
		ufs_qcom_cap.tx_pwr_pwm = UFS_QCOM_LIMIT_TX_PWR_PWM;
		ufs_qcom_cap.rx_pwr_hs = UFS_QCOM_LIMIT_RX_PWR_HS;
		ufs_qcom_cap.tx_pwr_hs = UFS_QCOM_LIMIT_TX_PWR_HS;

		ufs_qcom_cap.hs_rate = host->limit_rate;

		ufs_qcom_cap.desired_working_mode =
					UFS_QCOM_LIMIT_DESIRED_MODE;

		if (host->hw_ver.major == 0x1) {
			/*
			 * HS-G3 operations may not reliably work on legacy QCOM
			 * UFS host controller hardware even though capability
			 * exchange during link startup phase may end up
			 * negotiating maximum supported gear as G3.
			 * Hence downgrade the maximum supported gear to HS-G2.
			 */
			if (ufs_qcom_cap.hs_tx_gear > UFS_HS_G2)
				ufs_qcom_cap.hs_tx_gear = UFS_HS_G2;
			if (ufs_qcom_cap.hs_rx_gear > UFS_HS_G2)
				ufs_qcom_cap.hs_rx_gear = UFS_HS_G2;
		} else if (host->hw_ver.major < 0x4) {
			if (ufs_qcom_cap.hs_tx_gear > UFS_HS_G3)
				ufs_qcom_cap.hs_tx_gear = UFS_HS_G3;
			if (ufs_qcom_cap.hs_rx_gear > UFS_HS_G3)
				ufs_qcom_cap.hs_rx_gear = UFS_HS_G3;
		}

		ret = ufs_qcom_get_pwr_dev_param(&ufs_qcom_cap,
						 dev_max_params,
						 dev_req_params);
		if (ret) {
			pr_err("%s: failed to determine capabilities\n",
					__func__);
			goto out;
		}

		/* enable the device ref clock before changing to HS mode */
		if (!ufshcd_is_hs_mode(&hba->pwr_info) &&
			ufshcd_is_hs_mode(dev_req_params))
			ufs_qcom_dev_ref_clk_ctrl(host, true);

		if (host->hw_ver.major >= 0x4) {
			if (dev_req_params->gear_tx == UFS_HS_G4) {
				/* INITIAL ADAPT */
				ufshcd_dme_set(hba,
					       UIC_ARG_MIB(PA_TXHSADAPTTYPE),
					       PA_INITIAL_ADAPT);
			} else {
				/* NO ADAPT */
				ufshcd_dme_set(hba,
					       UIC_ARG_MIB(PA_TXHSADAPTTYPE),
					       PA_NO_ADAPT);
			}
		}
		break;
	case POST_CHANGE:
		if (ufs_qcom_cfg_timers(hba, dev_req_params->gear_rx,
					dev_req_params->pwr_rx,
					dev_req_params->hs_rate, false)) {
			dev_err(hba->dev, "%s: ufs_qcom_cfg_timers() failed\n",
				__func__);
			/*
			 * we return error code at the end of the routine,
			 * but continue to configure UFS_PHY_TX_LANE_ENABLE
			 * and bus voting as usual
			 */
			ret = -EINVAL;
		}

		val = ~(MAX_U32 << dev_req_params->lane_tx);
		ufs_qcom_phy_set_tx_lane_enable(phy, val);

		/* cache the power mode parameters to use internally */
		memcpy(&host->dev_req_params,
				dev_req_params, sizeof(*dev_req_params));
		ufs_qcom_update_bus_bw_vote(host);

		/* disable the device ref clock if entered PWM mode */
		if (ufshcd_is_hs_mode(&hba->pwr_info) &&
			!ufshcd_is_hs_mode(dev_req_params))
			ufs_qcom_dev_ref_clk_ctrl(host, false);
		break;
	default:
		ret = -EINVAL;
		break;
	}
out:
	if (dev_req_params)
		ufs_qcom_log_str(host, "@,%d,%d,%d,%d,%d\n", status,
			dev_req_params->gear_rx, dev_req_params->pwr_rx,
			dev_req_params->hs_rate, ret);
	else
		ufs_qcom_log_str(host, "@,%d,%d,%d,%d,%d\n", status,
			0, 0, 0, ret);
	return ret;
}

static int ufs_qcom_quirk_host_pa_saveconfigtime(struct ufs_hba *hba)
{
	int err;
	u32 pa_vs_config_reg1;

	err = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_VS_CONFIG_REG1),
			     &pa_vs_config_reg1);
	if (err)
		goto out;

	/* Allow extension of MSB bits of PA_SaveConfigTime attribute */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_VS_CONFIG_REG1),
			    (pa_vs_config_reg1 | (1 << 12)));

out:
	return err;
}

static void ufs_qcom_override_pa_h8time(struct ufs_hba *hba)
{
	int ret;
	u32 pa_h8time = 0;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_HIBERN8TIME),
				&pa_h8time);
	if (ret) {
		dev_err(hba->dev, "Failed getting PA_HIBERN8TIME: %d\n", ret);
		return;
	}


	/* 1 implies 100 us */
	ret = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_HIBERN8TIME),
				pa_h8time + 1);
	if (ret)
		dev_err(hba->dev, "Failed updating PA_HIBERN8TIME: %d\n", ret);

}

static void ufs_qcom_override_pa_tx_hsg1_sync_len(struct ufs_hba *hba)
{
#define PA_TX_HSG1_SYNC_LENGTH 0x1552
	int err;
	int sync_len_val = 0x4A;

	err = ufshcd_dme_peer_set(hba, UIC_ARG_MIB(PA_TX_HSG1_SYNC_LENGTH),
				sync_len_val);
	if (err)
		dev_err(hba->dev, "Failed (%d) set PA_TX_HSG1_SYNC_LENGTH(%d)\n",
				err, sync_len_val);
}

static inline bool
ufshcd_is_valid_pm_lvl(enum ufs_pm_level lvl)
{
	return lvl >= 0 && lvl < UFS_PM_LVL_MAX;
}

static void ufshcd_parse_pm_levels(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	enum ufs_pm_level rpm_lvl = UFS_PM_LVL_MAX, spm_lvl = UFS_PM_LVL_MAX;

	if (!np)
		return;

	if (host->is_dt_pm_level_read)
		return;

	if (!of_property_read_u32(np, "rpm-level", &rpm_lvl) &&
		ufshcd_is_valid_pm_lvl(rpm_lvl))
		hba->rpm_lvl = rpm_lvl;
	if (!of_property_read_u32(np, "spm-level", &spm_lvl) &&
		ufshcd_is_valid_pm_lvl(spm_lvl))
		hba->spm_lvl = spm_lvl;
	host->is_dt_pm_level_read = true;
}

/*
 * ufs_qcom_parse_multilevel_support - read from DTS where multi level
 * clock scaling support is enabled
 */
static void ufs_qcom_parse_turbo_clk_freq(struct ufs_qcom_host *host)
{
	struct device_node *np = host->hba->dev->of_node;

	if (!np)
		return;

	host->ml_scale_sup = of_property_read_bool(np, "multi-level-clk-scaling-support");

	if (!host->ml_scale_sup)
		return;

	host->axi_turbo_clk_freq =  UFS_QCOM_DEFAULT_TURBO_FREQ;
	host->axi_turbo_l1_clk_freq = UFS_QCOM_DEFAULT_TURBO_L1_FREQ;
	host->ice_turbo_clk_freq =  UFS_QCOM_DEFAULT_TURBO_FREQ;
	host->ice_turbo_l1_clk_freq = UFS_QCOM_DEFAULT_TURBO_L1_FREQ;
	host->unipro_turbo_clk_freq =  UFS_QCOM_DEFAULT_TURBO_FREQ;
	host->unipro_turbo_l1_clk_freq = UFS_QCOM_DEFAULT_TURBO_L1_FREQ;

	of_property_read_u32(np, "axi-turbo-clk-freq", &host->axi_turbo_clk_freq);
	of_property_read_u32(np, "axi-turbo-l1-clk-freq", &host->axi_turbo_l1_clk_freq);
	of_property_read_u32(np, "ice-turbo-clk-freq", &host->ice_turbo_clk_freq);
	of_property_read_u32(np, "ice-turbo-l1-clk-freq", &host->ice_turbo_l1_clk_freq);
	of_property_read_u32(np, "unipro-turbo-clk-freq", &host->unipro_turbo_clk_freq);
	of_property_read_u32(np, "unipro-turbo-l1-clk-freq", &host->unipro_turbo_l1_clk_freq);

	/* only some target need specific turbo setting */
	if (host->axi_turbo_l1_clk_freq > 403000000)
		host->turbo_additional_conf_req = true;

	dev_info(host->hba->dev, "%s multi_level_clk_scaling_support = %d\n", __func__,
			host->ml_scale_sup);
}

/*
 * TUBRO freq mode for ice/unipro/axi clk is needed only for few UFS3.x devices
 * for which AFC latencies is high(>500ns). For other devices, overwrite max
 * clk freq from turbo clkfreq read from DT back to NOM freq(300mhz) and
 * set back clk rate to this freq.
 */
static int ufs_qcom_update_max_clk_freq(struct ufs_hba *hba)
{
	struct ufs_clk_info *clki;
	struct list_head *head = &hba->clk_list_head;
	u32 max_freq = UFS_NOM_THRES_FREQ;
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	int ret = 0;

	clki = list_first_entry(&hba->clk_list_head, struct ufs_clk_info, list);
	if (clki->max_freq > UFS_NOM_THRES_FREQ) {
	/* if ml_scale_sup is true and UFS 3.x continue with turbo freq */
		if (host->ml_scale_sup && host->limit_phy_submode) {
			host->curr_axi_freq = host->axi_turbo_clk_freq;
			host->curr_unipro_freq = host->unipro_turbo_clk_freq;
			host->curr_ice_freq = host->ice_turbo_clk_freq;
			host->clk_curr_mode = TURBO;
			return 0;
		}
	} else {
	/* Initial freq read from DT is 300mhz, continue with that */
		host->curr_axi_freq = UFS_NOM_THRES_FREQ;
		host->curr_unipro_freq = UFS_NOM_THRES_FREQ;
		host->curr_ice_freq = UFS_NOM_THRES_FREQ;
		host->clk_curr_mode = NOM;
		return 0;
	}

	if (list_empty(head))
		goto out;

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk)) {
			if (!strcmp(clki->name, "core_clk") ||
				!strcmp(clki->name, "core_clk_unipro") ||
				!strcmp(clki->name, "core_clk_ice")) {
				clki->max_freq = max_freq;
				ret = clk_set_rate(clki->clk, clki->max_freq);
				if (ret)
					dev_err(hba->dev,
					"%s:clk_set_rate failed\n", __func__);
				/* multi level clk scale not needed */
				host->ml_scale_sup = false;
			}
		}
	}
out:
	return ret;
}

static int ufs_qcom_apply_dev_quirks(struct ufs_hba *hba)
{
	unsigned long flags;
	int err = 0;

	spin_lock_irqsave(hba->host->host_lock, flags);
	/* Set the rpm auto suspend delay to 3s */
	hba->host->hostt->rpm_autosuspend_delay = UFS_QCOM_AUTO_SUSPEND_DELAY;
	/* Set the default auto-hiberate idle timer value to 5ms */
	hba->ahit = FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, 5) |
		    FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, 3);
	/* Set the clock gating delay to performance mode */
	hba->clk_gating.delay_ms = UFS_QCOM_CLK_GATING_DELAY_MS_PERF;
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (hba->dev_quirks & UFS_DEVICE_QUIRK_HOST_PA_SAVECONFIGTIME)
		err = ufs_qcom_quirk_host_pa_saveconfigtime(hba);

	if (hba->dev_info.wmanufacturerid == UFS_VENDOR_WDC)
		hba->dev_quirks |= UFS_DEVICE_QUIRK_HOST_PA_TACTIVATE;

	if (hba->dev_quirks & UFS_DEVICE_QUIRK_PA_HIBER8TIME)
		ufs_qcom_override_pa_h8time(hba);

	if (hba->dev_quirks & UFS_DEVICE_QUIRK_PA_TX_HSG1_SYNC_LENGTH)
		ufs_qcom_override_pa_tx_hsg1_sync_len(hba);

	ufshcd_parse_pm_levels(hba);

	if (hba->dev_info.wmanufacturerid == UFS_VENDOR_MICRON)
		hba->dev_quirks |= UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM;

	return err;
}

static u32 ufs_qcom_get_ufs_hci_version(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	if (host->hw_ver.major == 0x1)
		return ufshci_version(1, 1);
	else
		return ufshci_version(2, 0);
}

/**
 * ufs_qcom_advertise_quirks - advertise the known QCOM UFS controller quirks
 * @hba: host controller instance
 *
 * QCOM UFS host controller might have some non standard behaviours (quirks)
 * than what is specified by UFSHCI specification. Advertise all such
 * quirks to standard UFS host controller driver so standard takes them into
 * account.
 */
static void ufs_qcom_advertise_quirks(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	if (host->hw_ver.major == 0x01) {
		hba->quirks |= UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS
			    | UFSHCD_QUIRK_BROKEN_PA_RXHSUNTERMCAP
			    | UFSHCD_QUIRK_DME_PEER_ACCESS_AUTO_MODE;

		if (host->hw_ver.minor == 0x0001 && host->hw_ver.step == 0x0001)
			hba->quirks |= UFSHCD_QUIRK_BROKEN_INTR_AGGR;

		hba->quirks |= UFSHCD_QUIRK_BROKEN_LCC;
	}

	if (host->hw_ver.major == 0x2) {
		hba->quirks |= UFSHCD_QUIRK_BROKEN_UFS_HCI_VERSION;

		if (!ufs_qcom_cap_qunipro(host))
			/* Legacy UniPro mode still need following quirks */
			hba->quirks |= (UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS
				| UFSHCD_QUIRK_DME_PEER_ACCESS_AUTO_MODE
				| UFSHCD_QUIRK_BROKEN_PA_RXHSUNTERMCAP);
	}

	if (host->disable_lpm)
		hba->quirks |= UFSHCD_QUIRK_BROKEN_AUTO_HIBERN8;

#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO_QTI)
	hba->quirks |= UFSHCD_QUIRK_CUSTOM_KEYSLOT_MANAGER;
#endif
}

static void ufs_qcom_set_caps(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	if (!host->disable_lpm) {
		hba->caps |= UFSHCD_CAP_CLK_GATING |
		UFSHCD_CAP_HIBERN8_WITH_CLK_GATING |
		UFSHCD_CAP_CLK_SCALING | UFSHCD_CAP_AUTO_BKOPS_SUSPEND |
		UFSHCD_CAP_RPM_AUTOSUSPEND;
		hba->caps |= UFSHCD_CAP_WB_EN;
		hba->caps |= UFSHCD_CAP_AGGR_POWER_COLLAPSE;
	}

	hba->caps |= UFSHCD_CAP_CRYPTO;

	if (host->hw_ver.major >= 0x2)
		host->caps = UFS_QCOM_CAP_QUNIPRO |
			     UFS_QCOM_CAP_RETAIN_SEC_CFG_AFTER_PWR_COLLAPSE;

	if (host->hw_ver.major >= 0x3) {
		host->caps |= UFS_QCOM_CAP_QUNIPRO_CLK_GATING;
		/*
		 * The UFS PHY attached to v3.0.0 controller supports entering
		 * deeper low power state of SVS2. This lets the controller
		 * run at much lower clock frequencies for saving power.
		 * Assuming this and any future revisions of the controller
		 * support this capability. Need to revist this assumption if
		 * any future platform with this core doesn't support the
		 * capability, as there will be no benefit running at lower
		 * frequencies then.
		 */
		host->caps |= UFS_QCOM_CAP_SVS2;
	}
}

static int ufs_qcom_unvote_qos_all(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct ufs_qcom_qos_req *ufs_qos_req = host->ufs_qos;
	struct qos_cpu_group *qcg;
	int err = 0, i;

	if (!host->ufs_qos)
		return 0;

	qcg = ufs_qos_req->qcg;
	for (i = 0; i < ufs_qos_req->num_groups; i++, qcg++) {
		flush_work(&qcg->vwork);
		if (!qcg->voted)
			continue;
		err = ufs_qcom_update_qos_constraints(qcg, QOS_MAX);
		if (err)
			dev_err(hba->dev, "Failed (%d) removing qos grp(%d)\n",
				err, i);
	}
	return err;
}

/**
 * ufs_qcom_setup_clocks - enables/disable clocks
 * @hba: host controller instance
 * @on: If true, enable clocks else disable them.
 * @status: PRE_CHANGE or POST_CHANGE notify
 *
 * Returns 0 on success, non-zero on failure.
 */
static int ufs_qcom_setup_clocks(struct ufs_hba *hba, bool on,
				 enum ufs_notify_change_status status)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	int err = 0;
	struct phy *phy;

	/*
	 * In case ufs_qcom_init() is not yet done, simply ignore.
	 * This ufs_qcom_setup_clocks() shall be called from
	 * ufs_qcom_init() after init is done.
	 */
	if (!host)
		return 0;

	phy =  host->generic_phy;
	switch (status) {
	case PRE_CHANGE:
		if (on) {
			err = ufs_qcom_set_bus_vote(hba, true);
			if (ufs_qcom_is_link_hibern8(hba))
				ufs_qcom_phy_set_src_clk_h8_exit(phy);
		} else {
			if (!ufs_qcom_is_link_active(hba)) {
				/* disable device ref_clk */
				ufs_qcom_dev_ref_clk_ctrl(host, false);

				/* power off PHY during aggressive clk gating */
				err = ufs_qcom_phy_power_off(hba);
				if (err) {
					dev_err(hba->dev, "%s: phy power off failed, ret = %d\n",
							 __func__, err);
					return err;
				}
			}
		}
		break;
	case POST_CHANGE:
		if (on) {
			err = ufs_qcom_phy_power_on(hba);
			if (err) {
				dev_err(hba->dev, "%s: phy power on failed, ret = %d\n",
						 __func__, err);
				return err;
			}

			/* enable the device ref clock for HS mode*/
			if (ufshcd_is_hs_mode(&hba->pwr_info))
				ufs_qcom_dev_ref_clk_ctrl(host, true);
		} else {
			if (ufs_qcom_is_link_hibern8(hba))
				ufs_qcom_phy_set_src_clk_h8_enter(phy);
			err = ufs_qcom_set_bus_vote(hba, false);
			if (err)
				return err;
			err = ufs_qcom_unvote_qos_all(hba);
		}
		if (!err)
			atomic_set(&host->clks_on, on);
		break;
	}
	ufs_qcom_log_str(host, "#,%d,%d,%d\n", status, on, err);

	return err;
}

static int
ufs_qcom_reset_assert(struct reset_controller_dev *rcdev, unsigned long id)
{
	struct ufs_qcom_host *host = rcdev_to_ufs_host(rcdev);

	/* Currently this code only knows about a single reset. */
	WARN_ON(id);
	ufs_qcom_assert_reset(host->hba);
	/* provide 1ms delay to let the reset pulse propagate. */
	usleep_range(1000, 1100);
	return 0;
}

static int
ufs_qcom_reset_deassert(struct reset_controller_dev *rcdev, unsigned long id)
{
	struct ufs_qcom_host *host = rcdev_to_ufs_host(rcdev);

	/* Currently this code only knows about a single reset. */
	WARN_ON(id);
	ufs_qcom_deassert_reset(host->hba);

	/*
	 * after reset deassertion, phy will need all ref clocks,
	 * voltage, current to settle down before starting serdes.
	 */
	usleep_range(1000, 1100);
	return 0;
}

static const struct reset_control_ops ufs_qcom_reset_ops = {
	.assert = ufs_qcom_reset_assert,
	.deassert = ufs_qcom_reset_deassert,
};

#ifndef MODULE
static int __init get_android_boot_dev(char *str)
{
	strlcpy(android_boot_dev, str, ANDROID_BOOT_DEV_MAX);
	return 1;
}
__setup("androidboot.bootdevice=", get_android_boot_dev);
#endif

static int ufs_qcom_parse_reg_info(struct ufs_qcom_host *host, char *name,
				   struct ufs_vreg **out_vreg)
{
	int ret = 0;
	char prop_name[MAX_PROP_SIZE];
	struct ufs_vreg *vreg = NULL;
	struct device *dev = host->hba->dev;
	struct device_node *np = dev->of_node;

	if (!np) {
		dev_err(dev, "%s: non DT initialization\n", __func__);
		goto out;
	}

	snprintf(prop_name, MAX_PROP_SIZE, "%s-supply", name);
	if (!of_parse_phandle(np, prop_name, 0)) {
		dev_info(dev, "%s: Unable to find %s regulator, assuming enabled\n",
			 __func__, prop_name);
		ret = -ENODEV;
		goto out;
	}

	vreg = devm_kzalloc(dev, sizeof(*vreg), GFP_KERNEL);
	if (!vreg)
		return -ENOMEM;

	vreg->name = name;

	snprintf(prop_name, MAX_PROP_SIZE, "%s-max-microamp", name);
	ret = of_property_read_u32(np, prop_name, &vreg->max_uA);
	if (ret) {
		dev_err(dev, "%s: unable to find %s err %d\n",
			__func__, prop_name, ret);
		goto out;
	}

	vreg->reg = devm_regulator_get(dev, vreg->name);
	if (IS_ERR(vreg->reg)) {
		ret = PTR_ERR(vreg->reg);
		dev_err(dev, "%s: %s get failed, err=%d\n",
			__func__, vreg->name, ret);
	}

	snprintf(prop_name, MAX_PROP_SIZE, "%s-min-uV", name);
	ret = of_property_read_u32(np, prop_name, &vreg->min_uV);
	if (ret) {
		dev_dbg(dev, "%s: unable to find %s err %d, using default\n",
			__func__, prop_name, ret);
		if (!strcmp(name, "qcom,vddp-ref-clk"))
			vreg->min_uV = VDDP_REF_CLK_MIN_UV;
		else if (!strcmp(name, "qcom,vccq-parent"))
			vreg->min_uV = 0;
		ret = 0;
	}

	snprintf(prop_name, MAX_PROP_SIZE, "%s-max-uV", name);
	ret = of_property_read_u32(np, prop_name, &vreg->max_uV);
	if (ret) {
		dev_dbg(dev, "%s: unable to find %s err %d, using default\n",
			__func__, prop_name, ret);
		if (!strcmp(name, "qcom,vddp-ref-clk"))
			vreg->max_uV = VDDP_REF_CLK_MAX_UV;
		else if (!strcmp(name, "qcom,vccq-parent"))
			vreg->max_uV = 0;
		ret = 0;
	}

out:
	if (!ret)
		*out_vreg = vreg;
	return ret;
}

static void ufs_qcom_save_host_ptr(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	int id;

	if (!hba->dev->of_node)
		return;

	/* Extract platform data */
	id = of_alias_get_id(hba->dev->of_node, "ufshc");
	if (id <= 0)
		dev_err(hba->dev, "Failed to get host index %d\n", id);
	else if (id <= MAX_UFS_QCOM_HOSTS)
		ufs_qcom_hosts[id - 1] = host;
	else
		dev_err(hba->dev, "invalid host index %d\n", id);
}

/**
 * ufs_qcom_query_ioctl - perform user read queries
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
ufs_qcom_query_ioctl(struct ufs_hba *hba, u8 lun, void __user *buffer)
{
	struct ufs_ioctl_query_data *ioctl_data;
	int err = 0;
	int length = 0;
	void *data_ptr;
	bool flag;
	u32 att;
	u8 index;
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
 * ufs_qcom_ioctl - ufs ioctl callback registered in scsi_host
 * @dev: scsi device required for per LUN queries
 * @cmd: command opcode
 * @buffer: user space buffer for transferring data
 *
 * Supported commands:
 * UFS_IOCTL_QUERY
 */
static int
ufs_qcom_ioctl(struct scsi_device *dev, unsigned int cmd, void __user *buffer)
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
		err = ufs_qcom_query_ioctl(hba,
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

static int tag_to_cpu(struct ufs_hba *hba, unsigned int tag)
{
	struct ufshcd_lrb *lrbp = &hba->lrb[tag];

	if (lrbp && lrbp->cmd && lrbp->cmd->request)
		return blk_mq_rq_cpu(lrbp->cmd->request);
	return -EINVAL;
}

static struct qos_cpu_group *cpu_to_group(struct ufs_qcom_qos_req *r,
					  unsigned int cpu)
{
	int i;
	struct qos_cpu_group *g = r->qcg;

	if (cpu > num_possible_cpus())
		return NULL;
	for (i = 0; i < r->num_groups; i++, g++) {
		if (cpumask_test_cpu(cpu, &g->mask))
			return &r->qcg[i];
	}
	return NULL;
}

static int ufs_qcom_update_qos_constraints(struct qos_cpu_group *qcg,
					   enum constraint type)
{
	unsigned int vote;
	int cpu, err;
	struct dev_pm_qos_request *qos_req = qcg->qos_req;

	if (type == QOS_MAX)
		vote = S32_MAX;
	else
		vote = qcg->votes[type];

	dev_dbg(qcg->host->hba->dev, "%s: qcg: 0x%08x | const: %d\n",
		__func__, qcg, type);
	if (qcg->curr_vote == vote)
		return 0;
	for_each_cpu(cpu, &qcg->mask) {
		err = dev_pm_qos_update_request(qos_req, vote);
		dev_dbg(qcg->host->hba->dev, "%s: vote: %d | cpu: %d | qos_req: 0x%08x\n",
			__func__, vote, cpu, qos_req);
		if (err < 0)
			return err;
		++qos_req;
	}
	if (type == QOS_MAX)
		qcg->voted = false;
	else
		qcg->voted = true;
	qcg->curr_vote = vote;
	return 0;
}

static void ufs_qcom_qos(struct ufs_hba *hba, int tag, bool is_scsi_cmd)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct qos_cpu_group *qcg;
	int cpu;

	if (!host->ufs_qos)
		return;
	cpu = tag_to_cpu(hba, tag);
	if (cpu < 0)
		return;
	qcg = cpu_to_group(host->ufs_qos, cpu);
	if (!qcg)
		return;

	if (qcg->perf_core && !host->cpufreq_dis &&
					!!atomic_read(&host->scale_up))
		atomic_inc(&host->num_reqs_threshold);

	if (qcg->voted) {
		dev_dbg(qcg->host->hba->dev, "%s: qcg: 0x%08x | Mask: 0x%08x - Already voted - return\n",
			__func__, qcg, qcg->mask);
		return;
	}
	queue_work(host->ufs_qos->workq, &qcg->vwork);
	dev_dbg(hba->dev, "Queued QoS work- cpu: %d\n", cpu);
}

static void ufs_qcom_vote_work(struct work_struct *work)
{
	int err;
	struct qos_cpu_group *qcg = container_of(work, struct qos_cpu_group,
						 vwork);

	err = ufs_qcom_update_qos_constraints(qcg, QOS_PERF);
	if (err)
		dev_err(qcg->host->hba->dev, "%s: update qos - failed: %d\n",
			__func__, err);
}

static int ufs_qcom_setup_qos(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct device *dev = hba->dev;
	struct ufs_qcom_qos_req *qr = host->ufs_qos;
	struct qos_cpu_group *qcg = qr->qcg;
	struct cpufreq_policy *policy;
	unsigned int cpu;
	int i, err;

	for (i = 0; i < qr->num_groups; i++, qcg++) {
		qcg->qos_req = kcalloc(cpumask_weight(&qcg->mask),
					sizeof(struct dev_pm_qos_request),
					GFP_KERNEL);
		if (!qcg->qos_req) {
			err = -ENOMEM;
			if (!i)
				return err;
			goto free_mem;
		}
		dev_dbg(dev, "%s: qcg: 0x%08x | mask: 0x%08x | mask-wt: %d | qos_req: 0x%08x\n",
			__func__, qcg, qcg->mask, cpumask_weight(&qcg->mask),
			qcg->qos_req);
		err = add_group_qos(qcg, S32_MAX);
		if (err < 0) {
			dev_err(dev, "Fail (%d) add qos-req: grp-%d\n",
				err, i);
			if (!i) {
				kfree(qcg->qos_req);
				return err;
			}
			goto free_mem;
		}
		INIT_WORK(&qcg->vwork, ufs_qcom_vote_work);

		if (host->cpufreq_dis || qcg->perf_core)
			continue;
		/* Bump up the cpufreq of the non-perf group */
		cpu = cpumask_first((const struct cpumask *)&qcg->mask);
		policy = cpufreq_cpu_get(cpu);
		if (!policy) {
			dev_err(dev, "Failed cpufreq policy,cpu=%d,mask=0x%08x\n",
				__func__, cpu, qcg->mask);
			host->cpufreq_dis = true;
			host->config_cpu = -1;
			continue;
		}
		host->config_cpu = cpu;
		host->min_cpu_scale_freq = policy->cpuinfo.min_freq;
		host->max_cpu_scale_freq = policy->cpuinfo.max_freq;
		cpufreq_cpu_put(policy);
	}

	if (!host->cpufreq_dis) {
		err = ufs_qcom_init_cpu_minfreq_req(host, host->config_cpu);
		if (err) {
			dev_err(dev, "Failed to register for freq_qos: %d\n",
				err);
			host->cpufreq_dis = true;
		} else {
			/* Prone to deadlocks, hence create its own workqueue */
			host->fworkq =
				create_singlethread_workqueue("ufs_cpufreq_wq");
			if (host->fworkq) {
				INIT_DELAYED_WORK(&host->fwork,
						  ufs_qcom_cpufreq_dwork);
				mutex_init(&host->cpufreq_lock);
			}
		}
	}
	qr->workq = create_singlethread_workqueue("qc_ufs_qos_swq");
	if (qr->workq)
		return 0;
	err = -1;
free_mem:
	while (i--) {
		kfree(qcg->qos_req);
		qcg--;
	}
	return err;
}

static void ufs_qcom_qos_init(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;
	struct device_node *group_node;
	struct ufs_qcom_qos_req *qr;
	struct qos_cpu_group *qcg;
	int i, err, mask = 0;
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	host->cpufreq_dis = true;
	qr = kzalloc(sizeof(*qr), GFP_KERNEL);
	if (!qr)
		return;

	host->ufs_qos = qr;
	qr->num_groups = of_get_available_child_count(np);
	dev_dbg(hba->dev, "num-groups: %d\n", qr->num_groups);
	if (!qr->num_groups) {
		dev_err(dev, "QoS groups undefined\n");
		kfree(qr);
		host->ufs_qos = NULL;
		return;
	}
	qcg = kzalloc(sizeof(*qcg) * qr->num_groups, GFP_KERNEL);
	if (!qcg) {
		kfree(qr);
		host->ufs_qos = NULL;
		return;
	}
	qr->qcg = qcg;
	for_each_available_child_of_node(np, group_node) {
		of_property_read_u32(group_node, "mask", &mask);
		qcg->mask.bits[0] = mask;
		if (!cpumask_subset(&qcg->mask, cpu_possible_mask)) {
			dev_err(dev, "Invalid group mask\n");
			goto out_err;
		}

		if (of_property_read_bool(group_node, "perf")) {
			qcg->perf_core = true;
			host->cpufreq_dis = false;
		} else {
			qcg->perf_core = false;
		}

		err = of_property_count_u32_elems(group_node, "vote");
		if (err <= 0) {
			dev_err(dev, "1 vote is needed, bailing out: %d\n",
				err);
			goto out_err;
		}
		qcg->votes = kmalloc(sizeof(*qcg->votes) * err, GFP_KERNEL);
		if (!qcg->votes)
			goto out_err;
		for (i = 0; i < err; i++) {
			if (of_property_read_u32_index(group_node, "vote", i,
						       &qcg->votes[i]))
				goto out_vote_err;
		}
		dev_dbg(dev, "%s: qcg: 0x%08x\n", __func__, qcg);
		qcg->host = host;
		++qcg;
	}
	if (ufs_qcom_setup_qos(hba))
		goto out_vote_err;
	return;
out_vote_err:
	for (i = 0, qcg = qr->qcg; i < qr->num_groups; i++, qcg++)
		kfree(qcg->votes);
out_err:
	kfree(qr->qcg);
	kfree(qr);
	host->ufs_qos = NULL;
}

static void ufs_qcom_parse_irq_affinity(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	int mask = 0;

	if (np) {
		of_property_read_u32(np, "qcom,prime-mask", &mask);
		host->perf_mask.bits[0] = mask;
		if (!cpumask_subset(&host->perf_mask, cpu_possible_mask)) {
			dev_err(dev, "Invalid group prime mask\n");
			host->perf_mask.bits[0] = UFS_QCOM_IRQ_PRIME_MASK;
		}
		mask = 0;
		of_property_read_u32(np, "qcom,silver-mask", &mask);
		host->def_mask.bits[0] = mask;
		if (!cpumask_subset(&host->def_mask, cpu_possible_mask)) {
			dev_err(dev, "Invalid group silver mask\n");
			host->def_mask.bits[0] = UFS_QCOM_IRQ_SLVR_MASK;
		}
	}
	/* If device includes perf mask, enable dynamic irq affinity feature */
	if (host->perf_mask.bits[0])
		host->irq_affinity_support = true;
}

static void ufs_qcom_parse_pm_level(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;

	if (np) {
		if (of_property_read_u32(np, "rpm-level",
					 &hba->rpm_lvl))
			hba->rpm_lvl = -1;
		if (of_property_read_u32(np, "spm-level",
					 &hba->spm_lvl))
			hba->spm_lvl = -1;
	}
}

/* Returns the max mitigation level supported */
static int ufs_qcom_get_max_therm_state(struct thermal_cooling_device *tcd,
				  unsigned long *data)
{
	*data = UFS_QCOM_LVL_MAX_THERM;

	return 0;
}

/* Returns the current mitigation level */
static int ufs_qcom_get_cur_therm_state(struct thermal_cooling_device *tcd,
				  unsigned long *data)
{
	struct ufs_hba *hba = dev_get_drvdata(tcd->devdata);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	*data = host->uqt.curr_state;

	return 0;
}

/* Convert microseconds to Auto-Hibernate Idle Timer register value */
static u32 ufs_qcom_us_to_ahit(unsigned int timer)
{
	unsigned int scale;

	for (scale = 0; timer > UFSHCI_AHIBERN8_TIMER_MASK; ++scale)
		timer /= UFSHCI_AHIBERN8_SCALE_FACTOR;

	return FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, timer) |
	       FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, scale);
}

/* Sets the mitigation level to requested level */
static int ufs_qcom_set_cur_therm_state(struct thermal_cooling_device *tcd,
				  unsigned long data)
{
	struct ufs_hba *hba = dev_get_drvdata(tcd->devdata);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct scsi_device *sdev;

	if (data == host->uqt.curr_state)
		return 0;

	switch (data) {
	case UFS_QCOM_LVL_NO_THERM:
		dev_warn(tcd->devdata, "UFS host thermal mitigation stops\n");

		if (host->irq_affinity_support)
			atomic_set(&host->therm_mitigation, 0);
		/* Set the default auto-hiberate idle timer to 5 ms */
		ufshcd_auto_hibern8_update(hba, ufs_qcom_us_to_ahit(5000));

		/* Set the default auto suspend delay to 3000 ms */
		shost_for_each_device(sdev, hba->host)
			pm_runtime_set_autosuspend_delay(&sdev->sdev_gendev,
						UFS_QCOM_AUTO_SUSPEND_DELAY);
		break;
	case UFS_QCOM_LVL_AGGR_THERM:
	case UFS_QCOM_LVL_MAX_THERM:
		dev_warn(tcd->devdata, "Going into UFS host thermal mitigation state, performance may be impacted before UFS host thermal mitigation stops\n");

		if (host->irq_affinity_support) {
			/* Stop setting hi-pri to requests and set irq affinity to default value */
			atomic_set(&host->therm_mitigation, 1);
			ufs_qcom_toggle_pri_affinity(hba, false);
		}
		/* Set the default auto-hiberate idle timer to 1 ms */
		ufshcd_auto_hibern8_update(hba, ufs_qcom_us_to_ahit(1000));

		/* Set the default auto suspend delay to 100 ms */
		shost_for_each_device(sdev, hba->host)
			pm_runtime_set_autosuspend_delay(&sdev->sdev_gendev,
							 100);
		break;
	default:
		dev_err(tcd->devdata, "Invalid UFS thermal state (%d)\n", data);
		return -EINVAL;
	}

	host->uqt.curr_state = data;

	return 0;
}

struct thermal_cooling_device_ops ufs_thermal_ops = {
	.get_max_state = ufs_qcom_get_max_therm_state,
	.get_cur_state = ufs_qcom_get_cur_therm_state,
	.set_cur_state = ufs_qcom_set_cur_therm_state,
};

/**
 * ufs_qcom_init - bind phy with controller
 * @hba: host controller instance
 *
 * Binds PHY with controller and powers up PHY enabling clocks
 * and regulators.
 *
 * Returns -EPROBE_DEFER if binding fails, returns negative error
 * on phy power up failure and returns zero on success.
 */
static int ufs_qcom_init(struct ufs_hba *hba)
{
	int err;
	struct device *dev = hba->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct ufs_qcom_host *host;
	struct resource *res;
	struct ufs_qcom_thermal *ut;
	struct ufs_clk_info *clki;
	struct device_node *np = dev->of_node;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host) {
		err = -ENOMEM;
		dev_err(dev, "%s: no memory for qcom ufs host\n", __func__);
		goto out;
	}

	/* Make a two way bind between the qcom host and the hba */
	host->hba = hba;
	ufshcd_set_variant(hba, host);
	ut = &host->uqt;
#if defined(CONFIG_UFS_DBG)
	host->dbg_en = true;
#endif
	host->crash_on_err =
		of_property_read_bool(np, "qcom,enable_crash_on_err");

	/* Setup the reset control of HCI */
	host->core_reset = devm_reset_control_get(hba->dev, "rst");
	if (IS_ERR(host->core_reset)) {
		err = PTR_ERR(host->core_reset);
		dev_warn(dev, "Failed to get reset control %d\n", err);
		host->core_reset = NULL;
		err = 0;
	}

	/* Fire up the reset controller. Failure here is non-fatal. */
	host->rcdev.of_node = dev->of_node;
	host->rcdev.ops = &ufs_qcom_reset_ops;
	host->rcdev.owner = dev->driver->owner;
	host->rcdev.nr_resets = 1;
	err = devm_reset_controller_register(dev, &host->rcdev);
	if (err) {
		dev_warn(dev, "Failed to register reset controller\n");
		err = 0;
	}

	/*
	 * voting/devoting device ref_clk source is time consuming hence
	 * skip devoting it during aggressive clock gating. This clock
	 * will still be gated off during runtime suspend.
	 */
	host->generic_phy = devm_phy_get(dev, "ufsphy");

	if (host->generic_phy == ERR_PTR(-EPROBE_DEFER)) {
		/*
		 * UFS driver might be probed before the phy driver does.
		 * In that case we would like to return EPROBE_DEFER code.
		 */
		err = -EPROBE_DEFER;
		dev_warn(dev, "%s: required phy device. hasn't probed yet. err = %d\n",
			__func__, err);
		goto out_variant_clear;
	} else if (IS_ERR(host->generic_phy)) {
		if (has_acpi_companion(dev)) {
			host->generic_phy = NULL;
		} else {
			err = PTR_ERR(host->generic_phy);
			dev_err(dev, "%s: PHY get failed %d\n", __func__, err);
			goto out_variant_clear;
		}
	}

	host->device_reset = devm_gpiod_get_optional(dev, "reset",
						     GPIOD_OUT_HIGH);
	if (IS_ERR(host->device_reset)) {
		err = PTR_ERR(host->device_reset);
		if (err != -EPROBE_DEFER)
			dev_err(dev, "failed to acquire reset gpio: %d\n", err);
		goto out_variant_clear;
	}

	err = ufs_qcom_bus_register(host);
	if (err)
		goto out_variant_clear;

	ufs_qcom_get_controller_revision(hba, &host->hw_ver.major,
		&host->hw_ver.minor, &host->hw_ver.step);

	/*
	 * for newer controllers, device reference clock control bit has
	 * moved inside UFS controller register address space itself.
	 */
	if (host->hw_ver.major >= 0x02) {
		host->dev_ref_clk_ctrl_mmio = hba->mmio_base + REG_UFS_CFG1;
		host->dev_ref_clk_en_mask = BIT(26);
	} else {
		/* "dev_ref_clk_ctrl_mem" is optional resource */
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   "dev_ref_clk_ctrl_mem");
		if (res) {
			host->dev_ref_clk_ctrl_mmio =
					devm_ioremap_resource(dev, res);
			if (IS_ERR(host->dev_ref_clk_ctrl_mmio)) {
				dev_warn(dev,
					"%s: could not map dev_ref_clk_ctrl_mmio, err %ld\n",
					__func__,
					PTR_ERR(host->dev_ref_clk_ctrl_mmio));
				host->dev_ref_clk_ctrl_mmio = NULL;
			}
			host->dev_ref_clk_en_mask = BIT(5);
		}
	}

	/* update phy revision information before calling phy_init() */
	/*
	 * FIXME:
	 * ufs_qcom_phy_save_controller_version(host->generic_phy,
	 *	host->hw_ver.major, host->hw_ver.minor, host->hw_ver.step);
	 */
	err = ufs_qcom_parse_reg_info(host, "qcom,vddp-ref-clk",
				      &host->vddp_ref_clk);

	err = phy_init(host->generic_phy);
	if (err) {
		dev_err(hba->dev, "%s: phy init failed, err %d\n",
				__func__, err);
		goto out_variant_clear;
	}
	mutex_init(&host->phy_mutex);

	if (host->vddp_ref_clk) {
		err = ufs_qcom_enable_vreg(dev, host->vddp_ref_clk);
		if (err) {
			dev_err(dev, "%s: failed enabling ref clk supply: %d\n",
				__func__, err);
			goto out_phy_exit;
		}
	}

	err = ufs_qcom_parse_reg_info(host, "qcom,vccq-parent",
				      &host->vccq_parent);
	if (host->vccq_parent) {
		err = ufs_qcom_enable_vreg(dev, host->vccq_parent);
		if (err) {
			dev_err(dev, "%s: failed enable vccq-parent err=%d\n",
				__func__, err);
			goto out_disable_vddp;
		}
	}

	list_for_each_entry(clki, &hba->clk_list_head, list) {
		if (!strcmp(clki->name, "core_clk_unipro"))
			clki->keep_link_active = true;
	}

	err = ufs_qcom_init_lane_clks(host);
	if (err)
		goto out_disable_vccq_parent;

	ufs_qcom_parse_pm_level(hba);
	ufs_qcom_parse_limits(host);
	ufs_qcom_parse_g4_workaround_flag(host);
	ufs_qcom_parse_turbo_clk_freq(host);
	ufs_qcom_update_max_clk_freq(hba);
	ufs_qcom_parse_lpm(host);
	if (host->disable_lpm)
		pm_runtime_forbid(host->hba->dev);

	ufs_qcom_set_caps(hba);
	ufs_qcom_advertise_quirks(hba);

	err = ufs_qcom_ice_init(host);
	if (err)
		goto out_variant_clear;

	/*
	 * Instantiate crypto capabilities for wrapped keys.
	 * It is controlled by CONFIG_SCSI_UFS_CRYPTO_QTI.
	 * If this is not defined, this API would return zero and
	 * non-wrapped crypto capabilities will be instantiated.
	 */
	ufshcd_qti_hba_init_crypto_capabilities(hba);

	ufs_qcom_set_bus_vote(hba, true);
	/* enable the device ref clock for HS mode*/
	if (ufshcd_is_hs_mode(&hba->pwr_info))
		ufs_qcom_dev_ref_clk_ctrl(host, true);

	if (hba->dev->id < MAX_UFS_QCOM_HOSTS)
		ufs_qcom_hosts[hba->dev->id] = host;

	host->dbg_print_en |= UFS_QCOM_DEFAULT_DBG_PRINT_EN;
	ufs_qcom_get_default_testbus_cfg(host);
	err = ufs_qcom_testbus_config(host);
	if (err) {
		dev_warn(dev, "%s: failed to configure the testbus %d\n",
				__func__, err);
		err = 0;
	}

	ufs_qcom_init_sysfs(hba);

	/* Provide SCSI host ioctl API */
	hba->host->hostt->ioctl = (int (*)(struct scsi_device *, unsigned int,
				   void __user *))ufs_qcom_ioctl;
#ifdef CONFIG_COMPAT
	hba->host->hostt->compat_ioctl = (int (*)(struct scsi_device *,
					  unsigned int,
					  void __user *))ufs_qcom_ioctl;
#endif

	ut->tcd = devm_thermal_of_cooling_device_register(dev,
							  dev->of_node,
							  "ufs",
							  dev,
							  &ufs_thermal_ops);
	if (IS_ERR(ut->tcd))
		dev_warn(dev, "Thermal mitigation registration failed: %d\n",
			 PTR_ERR(ut->tcd));
	else
		host->uqt.curr_state = UFS_QCOM_LVL_NO_THERM;

	ufs_qcom_save_host_ptr(hba);

	ufs_qcom_qos_init(hba);
	ufs_qcom_parse_irq_affinity(hba);
	host->ufs_ipc_log_ctx = ipc_log_context_create(UFS_QCOM_MAX_LOG_SZ,
							"ufs-qcom", 0);
	if (!host->ufs_ipc_log_ctx)
		dev_warn(dev, "IPC Log init - failed\n");

	goto out;

out_disable_vccq_parent:
	if (host->vccq_parent)
		ufs_qcom_disable_vreg(dev, host->vccq_parent);
out_disable_vddp:
	if (host->vddp_ref_clk)
		ufs_qcom_disable_vreg(dev, host->vddp_ref_clk);
out_phy_exit:
	phy_exit(host->generic_phy);
out_variant_clear:
	ufshcd_set_variant(hba, NULL);
out:
	return err;
}

static void ufs_qcom_exit(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	ufs_qcom_disable_lane_clks(host);
	ufs_qcom_phy_power_off(hba);
	phy_exit(host->generic_phy);
}

static int ufs_qcom_set_dme_vs_core_clk_ctrl_clear_div(struct ufs_hba *hba,
						       u32 clk_1us_cycles,
						       u32 clk_40ns_cycles)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	int err;
	u32 core_clk_ctrl_reg, clk_cycles;
	u32 mask = DME_VS_CORE_CLK_CTRL_MAX_CORE_CLK_1US_CYCLES_MASK;
	u32 offset = 0;

	/* Bits mask and offset changed on UFS host controller V4.0.0 onwards */
	if (host->hw_ver.major >= 4) {
		mask = DME_VS_CORE_CLK_CTRL_MAX_CORE_CLK_1US_CYCLES_MASK_V4;
		offset = DME_VS_CORE_CLK_CTRL_MAX_CORE_CLK_1US_CYCLES_OFFSET_V4;
	}

	if (clk_1us_cycles > mask)
		return -EINVAL;

	err = ufshcd_dme_get(hba,
			     UIC_ARG_MIB(DME_VS_CORE_CLK_CTRL),
			     &core_clk_ctrl_reg);
	if (err)
		goto out;

	core_clk_ctrl_reg &= ~mask;
	core_clk_ctrl_reg |= clk_1us_cycles;
	core_clk_ctrl_reg <<= offset;

	/* Clear CORE_CLK_DIV_EN */
	core_clk_ctrl_reg &= ~DME_VS_CORE_CLK_CTRL_CORE_CLK_DIV_EN_BIT;

	err = ufshcd_dme_set(hba,
			     UIC_ARG_MIB(DME_VS_CORE_CLK_CTRL),
			     core_clk_ctrl_reg);

	/* UFS host controller V4.0.0 onwards needs to program
	 * PA_VS_CORE_CLK_40NS_CYCLES attribute per programmed frequency of
	 * unipro core clk of UFS host controller.
	 */
	if (!err && (host->hw_ver.major >= 4)) {
		if (clk_40ns_cycles > PA_VS_CORE_CLK_40NS_CYCLES_MASK)
			return -EINVAL;

		err = ufshcd_dme_get(hba,
				     UIC_ARG_MIB(PA_VS_CORE_CLK_40NS_CYCLES),
				     &clk_cycles);
		if (err)
			goto out;

		clk_cycles &= ~PA_VS_CORE_CLK_40NS_CYCLES_MASK;
		clk_cycles |= clk_40ns_cycles;

		err = ufshcd_dme_set(hba,
				     UIC_ARG_MIB(PA_VS_CORE_CLK_40NS_CYCLES),
				     clk_cycles);
	}
out:
	return err;
}

static int ufs_qcom_turbo_specific_clk_scale_up_pre_change(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct ufs_pa_layer_attr *attr = &host->dev_req_params;
	int err = 0;
	struct ufs_clk_info *clki;

	/*
	 * If multilevel clock scaling is enabled but max freq
	 * is kept at 300mhz from DT, in that case turbo related setting
	 * are not required.
	 */
	clki = list_first_entry(&hba->clk_list_head, struct ufs_clk_info, list);
	if (clki->max_freq <= UFS_NOM_THRES_FREQ)
		goto out;

	/*
	 * This is case when clk mode changes from TURBO to NOM. Here the
	 * turbo related setting would be removed in clock scale up
	 * post change.
	 */
	if (host->clk_next_mode == NOM)
		return 0;

	/* Apply turbo related setting before changing to turbo mode */
	ufs_qcom_apply_turbo_setting(hba);

out:
	if (attr)
		__ufs_qcom_cfg_timers(hba, attr->gear_rx, attr->pwr_rx,
				      attr->hs_rate, false, true);

	err = ufs_qcom_set_dme_vs_core_clk_ctrl_max_freq_mode(hba);
	return err;
}

static int ufs_qcom_clk_scale_up_pre_change(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct ufs_pa_layer_attr *attr = &host->dev_req_params;
	int err = 0;

	if (!ufs_qcom_cap_qunipro(host))
		goto out;

	if (host->ml_scale_sup)
		return ufs_qcom_turbo_specific_clk_scale_up_pre_change(hba);

	if (attr)
		__ufs_qcom_cfg_timers(hba, attr->gear_rx, attr->pwr_rx,
				      attr->hs_rate, false, true);

	err = ufs_qcom_set_dme_vs_core_clk_ctrl_max_freq_mode(hba);
out:
	return err;
}

static int ufs_turbo_specific_qcom_clk_scale_up_post_change(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct ufs_pa_layer_attr *attr = &host->dev_req_params;
	int err = 0;
	struct ufs_clk_info *clki;

	clki = list_first_entry(&hba->clk_list_head, struct ufs_clk_info, list);
	if (!ufs_qcom_cap_qunipro(host))
		return 0;

	/* removed turbo setting when scaling down from TURBO to NOM */
	if (clki->max_freq > UFS_NOM_THRES_FREQ)
		ufs_qcom_remove_turbo_setting(hba);

	if (attr)
		ufs_qcom_cfg_timers(hba, attr->gear_rx, attr->pwr_rx,
				    attr->hs_rate, false);

	err = ufs_qcom_set_dme_vs_core_clk_ctrl_clear_div(hba, 300, 12);
	return err;
}

static int ufs_qcom_clk_scale_up_post_change(struct ufs_hba *hba)
{
	unsigned long flags;
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	/*
	 * Clk mode change is from TURBO to NOM, so we would have to remove
	 * turbo specific setting and also set counter/register according to
	 * new UFS clk freq. So is apparently considered similar to clk scale
	 * down scenarios. Hence call turbo specific clk scale down API
	 * to change this setting.
	 */
	if (host->ml_scale_sup && (host->clk_next_mode == NOM)) {
		ufs_turbo_specific_qcom_clk_scale_up_post_change(hba);
		return 0;
	}
	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->clk_gating.delay_ms = UFS_QCOM_CLK_GATING_DELAY_MS_PERF;
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return 0;
}

static int ufs_qcom_clk_scale_down_pre_change(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	int err;
	u32 core_clk_ctrl_reg;

	if (!ufs_qcom_cap_qunipro(host))
		return 0;

	err = ufshcd_dme_get(hba,
			    UIC_ARG_MIB(DME_VS_CORE_CLK_CTRL),
			    &core_clk_ctrl_reg);

	/* make sure CORE_CLK_DIV_EN is cleared */
	if (!err &&
	    (core_clk_ctrl_reg & DME_VS_CORE_CLK_CTRL_CORE_CLK_DIV_EN_BIT)) {
		core_clk_ctrl_reg &= ~DME_VS_CORE_CLK_CTRL_CORE_CLK_DIV_EN_BIT;
		err = ufshcd_dme_set(hba,
				    UIC_ARG_MIB(DME_VS_CORE_CLK_CTRL),
				    core_clk_ctrl_reg);
	}

	return err;
}

static int ufs_qcom_clk_scale_down_post_change(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct ufs_pa_layer_attr *attr = &host->dev_req_params;
	int err = 0;
	struct ufs_clk_info *clki;
	struct list_head *head = &hba->clk_list_head;
	u32 curr_freq = 0;
	unsigned long flags;

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->clk_gating.delay_ms = UFS_QCOM_CLK_GATING_DELAY_MS_PWR_SAVE;
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (!ufs_qcom_cap_qunipro(host))
		return 0;

	/* removed turbo setting when scaling down from TURBO to LOWSVS */
	if (host->ml_scale_sup)
		ufs_qcom_remove_turbo_setting(hba);

	if (attr)
		ufs_qcom_cfg_timers(hba, attr->gear_rx, attr->pwr_rx,
				    attr->hs_rate, false);

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk) &&
		    (!strcmp(clki->name, "core_clk_unipro"))) {
			curr_freq = clk_get_rate(clki->clk);
			break;
		}
	}

	switch (curr_freq) {
	case 37500000:
		err = ufs_qcom_set_dme_vs_core_clk_ctrl_clear_div(hba, 38, 2);
		break;
	case 75000000:
		err = ufs_qcom_set_dme_vs_core_clk_ctrl_clear_div(hba, 75, 3);
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

/*
 * Override freq from TURBO to NOM(300mhz) when changing freq mode from
 * TURBO to NOM when busy_percentage falls below the predefined threshold.
 * This also updates the curr clk freq variable defined in host structure for
 * bookkeeping purpose which can be read using sysfs.
 */
static int ufs_qcom_override_clk_freq(struct ufs_hba *hba)
{
	int ret = 0;
	struct ufs_clk_info *clki;
	struct list_head *head = &hba->clk_list_head;
	unsigned long max_freq = UFS_NOM_THRES_FREQ;
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	if (list_empty(head))
		goto out;

	clki = list_first_entry(&hba->clk_list_head, struct ufs_clk_info, list);

	/*
	 * Freq override from TURBO to NOM(300mhz) is required only when changing clk mode
	 * from TURBO to NOM. We should avoid overriding to NOM when clk scaling is disable
	 * so that we operate at turbo freq in case clk scaling is disabled.
	 */
	if (host->is_clk_scale_enabled &&
			!(host->clk_next_mode == NOM)) {

	/*
	 * No Need to override, continue with TURBO or LOWSVS freq,
	 * update curr clk freq before return
	 */
		if (host->clk_next_mode == TURBO) {
			host->curr_axi_freq = host->axi_turbo_clk_freq;
			host->curr_unipro_freq = host->unipro_turbo_clk_freq;
			host->curr_ice_freq = host->ice_turbo_clk_freq;
		} else if (host->clk_next_mode == LOW_SVS) {
			host->curr_axi_freq = clki->min_freq;
			host->curr_unipro_freq = clki->min_freq;
			host->curr_ice_freq = clki->min_freq;
		}
		return 0;
	}

	clki = list_first_entry(&hba->clk_list_head, struct ufs_clk_info, list);
	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk)) {
			if (!strcmp(clki->name, "core_clk") ||
				!strcmp(clki->name, "core_clk_unipro") ||
				!strcmp(clki->name, "core_clk_ice")) {

				if (!strcmp(clki->name, "core_clk")) {
					/*
					 * clock scale up request when clk scaling
					 * is being disable(sysfs)
					 */
					if (!host->is_clk_scale_enabled)
						max_freq = clki->max_freq;

					ret = clk_set_rate(clki->clk, max_freq);
					if (ret)
						goto out;
					host->curr_axi_freq = max_freq;
				}

				if (!strcmp(clki->name, "core_clk_ice")) {
					/*
					 * clock scale up request when clk scaling
					 *  is being disable(sysfs)
					 */
					if (!host->is_clk_scale_enabled)
						max_freq = clki->max_freq;

					ret = clk_set_rate(clki->clk, max_freq);
					if (ret)
						goto out;
					host->curr_ice_freq = max_freq;
				}

				if (!strcmp(clki->name, "core_clk_unipro")) {
					/*
					 * clock scale up request when clk scaling
					 * is being disable(sysfs)
					 */
					if (!host->is_clk_scale_enabled)
						max_freq = clki->max_freq;

					ret = clk_set_rate(clki->clk, max_freq);
					if (ret)
						goto out;
					host->curr_unipro_freq = max_freq;
				}

			}
		}

	}
	return 0;
 out:
	if (ret) {
		dev_err(hba->dev, "%s: %s clk set rate(%dHz) failed, %d\n",
				__func__, clki->name,
				max_freq, ret);
	}
	return ret;
}
/**
 * ufs_qcom_get_clk_mode - Get the clk next mode
 * and update is_turbo_enabled flag based on curr freq, max_freq,
 * scale_up flag received.
 */
static void ufs_qcom_get_clk_mode(struct ufs_hba *hba, bool scale_up)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct ufs_clk_info *clki;

	/*
	 * Cached the clk scaling is_enabled locally so that it doesn't
	 * get updated externally and use this throughout the clock scaling
	 * function.
	 */
	host->is_clk_scale_enabled = hba->clk_scaling.is_enabled;
	clki = list_first_entry(&hba->clk_list_head, struct ufs_clk_info, list);

	if (!scale_up) {
		host->clk_next_mode = LOW_SVS;
		/* There can be a chance, after scale_up being
		 * set to false in scaling vendor hook,
		 * is_clk_scale_enabled can be set to 0 via
		 * sysfs which will cause inconsistent state
		 * of scale_up as false and is_clk_scale_enabled
		 * as off. Hence override is_clk_scale_enabled to
		 * true to keep both the state consistent.
		 */
		host->is_clk_scale_enabled = true;
		host->is_turbo_enabled = false;
	} else {
		if ((clki->max_freq > UFS_NOM_THRES_FREQ) &&
			 (!host->is_clk_scale_enabled)) {
			/*
			 * This is when clk scaling is called during
			 * clock scaling disabled using sysfs.
			 */
			host->clk_next_mode = TURBO;
			host->is_turbo_enabled = true;
		} else if ((clki->max_freq > UFS_NOM_THRES_FREQ) &&
				(clk_get_rate(clki->clk) > UFS_NOM_THRES_FREQ)) {
			/*
			 * This is when clk scaling is called from devfreq
			 * and mode change is from TURBO TO NOM when bp
			 * falls below thres.
			 */
			host->clk_next_mode = NOM;
			host->is_turbo_enabled = false;
		} else if (clki->max_freq > UFS_NOM_THRES_FREQ) {
			/* this is scale up from LOWSVS to TURBO */
			host->clk_next_mode = TURBO;
			host->is_turbo_enabled = true;
		} else {
			/*
			 * this is multilevel enabled from DT but
			 * maxfreq is set to 300mhz.
			 */
			host->clk_next_mode = NOM;
			host->is_turbo_enabled = false;
		}
	}
}

static int ufs_qcom_clk_scale_notify(struct ufs_hba *hba,
		bool scale_up, enum ufs_notify_change_status status)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct ufs_pa_layer_attr *dev_req_params = &host->dev_req_params;
	int err = 0;

	if (status == PRE_CHANGE) {
		if (host->ml_scale_sup)
			ufs_qcom_get_clk_mode(hba, scale_up);
		err = ufshcd_uic_hibern8_enter(hba);
		if (err)
			return err;
		if (scale_up) {
			err = ufs_qcom_clk_scale_up_pre_change(hba);
			if (!host->cpufreq_dis && !atomic_read(&host->scale_up) &&
					!(atomic_read(&host->therm_mitigation))) {
				atomic_set(&host->num_reqs_threshold, 0);
				/* ensure that work is queued */
				mutex_lock(&host->cpufreq_lock);
				queue_delayed_work(host->fworkq, &host->fwork,
						   msecs_to_jiffies(
						   UFS_QCOM_LOAD_MON_DLY_MS));
				atomic_set(&host->scale_up, scale_up);
				mutex_unlock(&host->cpufreq_lock);
			}
		} else {
			err = ufs_qcom_clk_scale_down_pre_change(hba);
		}
		if (err)
			ufshcd_uic_hibern8_exit(hba);
	} else {
		if (host->ml_scale_sup)
			ufs_qcom_override_clk_freq(hba);
		if (scale_up)
			err = ufs_qcom_clk_scale_up_post_change(hba);
		else
			err = ufs_qcom_clk_scale_down_post_change(hba);


		if (err || !dev_req_params) {
			ufshcd_uic_hibern8_exit(hba);
			goto out;
		}

		ufs_qcom_cfg_timers(hba,
				    dev_req_params->gear_rx,
				    dev_req_params->pwr_rx,
				    dev_req_params->hs_rate,
				    false);
		/*
		 * set is_max_bw_needed to 1 so that IB value of max_bw
		 * row from DT would be picked. IB value is voted to make
		 * sure voting is guranteed to keep bus vote to Turbo mode
		 */
		if (host->ml_scale_sup && host->is_turbo_enabled)
			host->bus_vote.is_max_bw_needed = true;
		else
			host->bus_vote.is_max_bw_needed = false;

		ufs_qcom_update_bus_bw_vote(host);
		ufshcd_uic_hibern8_exit(hba);

		/*
		 * Update clk curr mode with clk next mode in end of
		 * clk scaling post sequence and mark clk next mode
		 * as INVAL.
		 */
		if (host->ml_scale_sup) {
			host->clk_curr_mode = host->clk_next_mode;
			host->clk_next_mode = INVAL_MODE;
		}
	}

	if (!err)
		atomic_set(&host->scale_up, scale_up);
out:
	if (scale_up)
		ufs_qcom_log_str(host, "^,%d,%d\n", status, err);
	else
		ufs_qcom_log_str(host, "v,%d,%d\n", status, err);
	return err;
}

void ufs_qcom_print_hw_debug_reg_all(struct ufs_hba *hba,
		void *priv, void (*print_fn)(struct ufs_hba *hba,
		int offset, int num_regs, const char *str, void *priv))
{
	u32 reg;
	struct ufs_qcom_host *host;

	if (unlikely(!hba)) {
		pr_err("%s: hba is NULL\n", __func__);
		return;
	}
	if (unlikely(!print_fn)) {
		dev_err(hba->dev, "%s: print_fn is NULL\n", __func__);
		return;
	}

	host = ufshcd_get_variant(hba);
	if (!(host->dbg_print_en & UFS_QCOM_DBG_PRINT_REGS_EN))
		return;

	reg = ufs_qcom_get_debug_reg_offset(host, UFS_UFS_DBG_RD_REG_OCSC);
	print_fn(hba, reg, 44, "UFS_UFS_DBG_RD_REG_OCSC ", priv);

	reg = ufshcd_readl(hba, REG_UFS_CFG1);
	reg |= UTP_DBG_RAMS_EN;
	ufshcd_writel(hba, reg, REG_UFS_CFG1);

	reg = ufs_qcom_get_debug_reg_offset(host, UFS_UFS_DBG_RD_EDTL_RAM);
	print_fn(hba, reg, 32, "UFS_UFS_DBG_RD_EDTL_RAM ", priv);

	reg = ufs_qcom_get_debug_reg_offset(host, UFS_UFS_DBG_RD_DESC_RAM);
	print_fn(hba, reg, 128, "UFS_UFS_DBG_RD_DESC_RAM ", priv);

	reg = ufs_qcom_get_debug_reg_offset(host, UFS_UFS_DBG_RD_PRDT_RAM);
	print_fn(hba, reg, 64, "UFS_UFS_DBG_RD_PRDT_RAM ", priv);

	/* clear bit 17 - UTP_DBG_RAMS_EN */
	ufshcd_rmwl(hba, UTP_DBG_RAMS_EN, 0, REG_UFS_CFG1);

	reg = ufs_qcom_get_debug_reg_offset(host, UFS_DBG_RD_REG_UAWM);
	print_fn(hba, reg, 4, "UFS_DBG_RD_REG_UAWM ", priv);

	reg = ufs_qcom_get_debug_reg_offset(host, UFS_DBG_RD_REG_UARM);
	print_fn(hba, reg, 4, "UFS_DBG_RD_REG_UARM ", priv);

	reg = ufs_qcom_get_debug_reg_offset(host, UFS_DBG_RD_REG_TXUC);
	print_fn(hba, reg, 48, "UFS_DBG_RD_REG_TXUC ", priv);

	reg = ufs_qcom_get_debug_reg_offset(host, UFS_DBG_RD_REG_RXUC);
	print_fn(hba, reg, 27, "UFS_DBG_RD_REG_RXUC ", priv);

	reg = ufs_qcom_get_debug_reg_offset(host, UFS_DBG_RD_REG_DFC);
	print_fn(hba, reg, 19, "UFS_DBG_RD_REG_DFC ", priv);

	reg = ufs_qcom_get_debug_reg_offset(host, UFS_DBG_RD_REG_TRLUT);
	print_fn(hba, reg, 34, "UFS_DBG_RD_REG_TRLUT ", priv);

	reg = ufs_qcom_get_debug_reg_offset(host, UFS_DBG_RD_REG_TMRLUT);
	print_fn(hba, reg, 9, "UFS_DBG_RD_REG_TMRLUT ", priv);
}

static void ufs_qcom_enable_test_bus(struct ufs_qcom_host *host)
{
	if (host->dbg_print_en & UFS_QCOM_DBG_PRINT_TEST_BUS_EN) {
		ufshcd_rmwl(host->hba, UFS_REG_TEST_BUS_EN,
				UFS_REG_TEST_BUS_EN, REG_UFS_CFG1);
		ufshcd_rmwl(host->hba, TEST_BUS_EN, TEST_BUS_EN, REG_UFS_CFG1);
	} else {
		ufshcd_rmwl(host->hba, UFS_REG_TEST_BUS_EN, 0, REG_UFS_CFG1);
		ufshcd_rmwl(host->hba, TEST_BUS_EN, 0, REG_UFS_CFG1);
	}
}

static void ufs_qcom_get_default_testbus_cfg(struct ufs_qcom_host *host)
{
	/* provide a legal default configuration */
	host->testbus.select_major = TSTBUS_UNIPRO;
	host->testbus.select_minor = 37;
}

static bool ufs_qcom_testbus_cfg_is_ok(struct ufs_qcom_host *host)
{
	if (host->testbus.select_major >= TSTBUS_MAX) {
		dev_err(host->hba->dev,
			"%s: UFS_CFG1[TEST_BUS_SEL} may not equal 0x%05X\n",
			__func__, host->testbus.select_major);
		return false;
	}

	return true;
}

/*
 * The caller of this function must make sure that the controller
 * is out of runtime suspend and appropriate clocks are enabled
 * before accessing.
 */
int ufs_qcom_testbus_config(struct ufs_qcom_host *host)
{
	int reg;
	int offset;
	u32 mask = TEST_BUS_SUB_SEL_MASK;
	unsigned long flags;
	struct ufs_hba *hba;

	if (!host)
		return -EINVAL;
	hba = host->hba;
	spin_lock_irqsave(hba->host->host_lock, flags);
	if (!ufs_qcom_testbus_cfg_is_ok(host))
		return -EPERM;

	switch (host->testbus.select_major) {
	case TSTBUS_UAWM:
		reg = UFS_TEST_BUS_CTRL_0;
		offset = 24;
		break;
	case TSTBUS_UARM:
		reg = UFS_TEST_BUS_CTRL_0;
		offset = 16;
		break;
	case TSTBUS_TXUC:
		reg = UFS_TEST_BUS_CTRL_0;
		offset = 8;
		break;
	case TSTBUS_RXUC:
		reg = UFS_TEST_BUS_CTRL_0;
		offset = 0;
		break;
	case TSTBUS_DFC:
		reg = UFS_TEST_BUS_CTRL_1;
		offset = 24;
		break;
	case TSTBUS_TRLUT:
		reg = UFS_TEST_BUS_CTRL_1;
		offset = 16;
		break;
	case TSTBUS_TMRLUT:
		reg = UFS_TEST_BUS_CTRL_1;
		offset = 8;
		break;
	case TSTBUS_OCSC:
		reg = UFS_TEST_BUS_CTRL_1;
		offset = 0;
		break;
	case TSTBUS_WRAPPER:
		reg = UFS_TEST_BUS_CTRL_2;
		offset = 16;
		break;
	case TSTBUS_COMBINED:
		reg = UFS_TEST_BUS_CTRL_2;
		offset = 8;
		break;
	case TSTBUS_UTP_HCI:
		reg = UFS_TEST_BUS_CTRL_2;
		offset = 0;
		break;
	case TSTBUS_UNIPRO:
		reg = UFS_UNIPRO_CFG;
		offset = 20;
		mask = 0xFFF;
		break;
	/*
	 * No need for a default case, since
	 * ufs_qcom_testbus_cfg_is_ok() checks that the configuration
	 * is legal
	 */
	}
	mask <<= offset;

	spin_unlock_irqrestore(hba->host->host_lock, flags);
	ufshcd_rmwl(host->hba, TEST_BUS_SEL,
		    (u32)host->testbus.select_major << 19,
		    REG_UFS_CFG1);
	ufshcd_rmwl(host->hba, mask,
		    (u32)host->testbus.select_minor << offset,
		    reg);
	ufs_qcom_enable_test_bus(host);
	/*
	 * Make sure the test bus configuration is
	 * committed before returning.
	 */
	mb();

	return 0;
}

static void ufs_qcom_testbus_read(struct ufs_hba *hba)
{
	ufshcd_dump_regs(hba, UFS_TEST_BUS, 4, "UFS_TEST_BUS ");
}

static void ufs_qcom_print_unipro_testbus(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	u32 *testbus = NULL;
	int i, nminor = 256, testbus_len = nminor * sizeof(u32);

	testbus = kmalloc(testbus_len, GFP_KERNEL);
	if (!testbus)
		return;

	host->testbus.select_major = TSTBUS_UNIPRO;
	for (i = 0; i < nminor; i++) {
		host->testbus.select_minor = i;
		ufs_qcom_testbus_config(host);
		testbus[i] = ufshcd_readl(hba, UFS_TEST_BUS);
	}
	print_hex_dump(KERN_ERR, "UNIPRO_TEST_BUS ", DUMP_PREFIX_OFFSET,
			16, 4, testbus, testbus_len, false);
	kfree(testbus);
}

static void ufs_qcom_print_utp_hci_testbus(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	u32 *testbus = NULL;
	int i, nminor = 32, testbus_len = nminor * sizeof(u32);

	testbus = kmalloc(testbus_len, GFP_KERNEL);
	if (!testbus)
		return;

	host->testbus.select_major = TSTBUS_UTP_HCI;
	for (i = 0; i < nminor; i++) {
		host->testbus.select_minor = i;
		ufs_qcom_testbus_config(host);
		testbus[i] = ufshcd_readl(hba, UFS_TEST_BUS);
	}
	print_hex_dump(KERN_ERR, "UTP_HCI_TEST_BUS ", DUMP_PREFIX_OFFSET,
			16, 4, testbus, testbus_len, false);
	kfree(testbus);
}

static void ufs_qcom_dump_dbg_regs(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct phy *phy = host->generic_phy;

	host->err_occurred = true;

	ufshcd_dump_regs(hba, REG_UFS_SYS1CLK_1US, 16 * 4,
			 "HCI Vendor Specific Registers ");

	/* sleep a bit intermittently as we are dumping too much data */
	ufs_qcom_print_hw_debug_reg_all(hba, NULL, ufs_qcom_dump_regs_wrapper);

	if (in_task()) {
		usleep_range(1000, 1100);
		ufs_qcom_testbus_read(hba);
		usleep_range(1000, 1100);
		ufs_qcom_print_unipro_testbus(hba);
		usleep_range(1000, 1100);
		ufs_qcom_print_utp_hci_testbus(hba);
		usleep_range(1000, 1100);
		ufs_qcom_phy_dbg_register_dump(phy);
	}

	BUG_ON(host->crash_on_err);
}

/*
 * Read sdam register for ufs device identification using
 * nvmem interface and accordingly set phy submode.
 * sdam Value = 0 : UFS 3.x, phy_submode = 1.
 * sdam Value = 1 : UFS 2.x, phy_submode = 0.
 */
void ufs_qcom_read_nvmem_cell(struct ufs_qcom_host *host)
{
	size_t len;
	u8 *data;
	bool ufs_dev;

	host->nvmem_cell = nvmem_cell_get(host->hba->dev, "ufs_dev");
	if (IS_ERR(host->nvmem_cell)) {
		dev_info(host->hba->dev, "(%s) Failed to get nvmem cell\n", __func__);
		return;
	}

	data = (u8 *)nvmem_cell_read(host->nvmem_cell, &len);
	if (IS_ERR(data)) {
		dev_info(host->hba->dev, "(%s) Failed to read from nvmem\n", __func__);
		goto cell_put;
	}

	ufs_dev = *data;
	/* Revert as below
	 * Value = 0 : UFS 3.x
	 * Value = 1 : UFS 2.x
	 */
	if (host->ufs_dev_revert)
		host->limit_phy_submode = ufs_dev;
	else
		host->limit_phy_submode = !ufs_dev;

	if (host->limit_phy_submode)
		dev_info(host->hba->dev, "(%s) UFS device is 3.x, phy_submode = %d\n",
						__func__, host->limit_phy_submode);
	else
		dev_info(host->hba->dev, "(%s) UFS device is 2.x, phy_submode = %d\n",
						__func__, host->limit_phy_submode);

	kfree(data);

cell_put:
	nvmem_cell_put(host->nvmem_cell);
}

/*
 * ufs_qcom_parse_limits - read limits from DTS
 */
static void ufs_qcom_parse_limits(struct ufs_qcom_host *host)
{
	struct device_node *np = host->hba->dev->of_node;

	if (!np)
		return;

	host->limit_tx_hs_gear = UFS_QCOM_LIMIT_HSGEAR_TX;
	host->limit_rx_hs_gear = UFS_QCOM_LIMIT_HSGEAR_RX;
	host->limit_tx_pwm_gear = UFS_QCOM_LIMIT_PWMGEAR_TX;
	host->limit_rx_pwm_gear = UFS_QCOM_LIMIT_PWMGEAR_RX;
	host->limit_rate = UFS_QCOM_LIMIT_HS_RATE;
	host->limit_phy_submode = UFS_QCOM_LIMIT_PHY_SUBMODE;
	host->ufs_dev_types = 0;

	of_property_read_u32(np, "limit-tx-hs-gear", &host->limit_tx_hs_gear);
	of_property_read_u32(np, "limit-rx-hs-gear", &host->limit_rx_hs_gear);
	of_property_read_u32(np, "limit-tx-pwm-gear", &host->limit_tx_pwm_gear);
	of_property_read_u32(np, "limit-rx-pwm-gear", &host->limit_rx_pwm_gear);
	of_property_read_u32(np, "limit-rate", &host->limit_rate);
	of_property_read_u32(np, "limit-phy-submode", &host->limit_phy_submode);
	of_property_read_u32(np, "ufs-dev-types", &host->ufs_dev_types);
	host->ufs_dev_revert = of_property_read_bool(np, "qcom,ufs-dev-revert");

	if (host->ufs_dev_types >= 2)
		ufs_qcom_read_nvmem_cell(host);
}

/*
 * ufs_qcom_parse_g4_workaround_flag - read bypass-g4-cfgready entry from DT
 */
static void ufs_qcom_parse_g4_workaround_flag(struct ufs_qcom_host *host)
{
	struct device_node *np = host->hba->dev->of_node;
	const char *str  = "bypass-g4-cfgready";

	if (!np)
		return;

	host->bypass_g4_cfgready = of_property_read_bool(np, str);
}

/**
 * ufs_qcom_hook_clock_scaling -  Influence the UFS clock scaling policy
 * @force_out: flag to decide whether to skip scale up/down check in ufshcd_devfreq_target
 * @force_scaling: Decide whether to do force scaling in ufshcd_devfreq_target
 * @scale_up: scale up or down request coming from devfreq
 *
 * This API Updates force_out, force_scaling and scale_up parameter before returning.
 * It also updates clk_next_mode and is_turbo_enabled. Based on this, required setting
 * would be applied as part of clk scaling pre and post
 * sequence. At end of clk scaling post sequence clk_curr_mode would be updated with
 * clk_next_mode value.
 */
static void ufs_qcom_hook_clock_scaling(void *unused, struct ufs_hba *hba, bool *force_out,
	bool *force_scaling, bool *scale_up)
{
	/*
	 * TURBO_DOWN_THRESHOLD is the busy% threshold to scale down from TURBO to NOM.
	 *
	 * TODO: Need to adjust threshold taking into account of power and perf tradeoff
	 */
	#define TURBO_DOWN_THRESHOLD         50
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	struct devfreq_dev_status *stat = &hba->devfreq->last_status;
	struct ufs_clk_info *clki;
	int busy_percentage;
	unsigned long core_clk_rate = 0;

	if (!host->ml_scale_sup)
		return;

	/*
	 * Return if clock scaling is disabled,scaled
	 * up called during disabling clkscale(sysfs)
	 * will take care of scaling to max clk freq.
	 */
	if (!hba->clk_scaling.is_enabled) {
		*force_out = true;
		return;
	}

	busy_percentage = (stat->busy_time * 100)/(stat->total_time);
	clki = list_first_entry(&hba->clk_list_head, struct ufs_clk_info, list);
	core_clk_rate = host->curr_axi_freq;

	/*
	 * In case of clk scale down, we align with devfreq i.e we
	 * scale down from either TURBO(TURBO_L1) or NOM to LOW_SVS if
	 * scale down is received from devfreq
	 */
	if (!(*scale_up)) {
		*force_out = false;
		*force_scaling = false;
		host->turbo_down_thres_cnt = 0;
		return;
	}
	/* This is scale_up from LOW_SVS to TURBO */
	if (core_clk_rate == clki->min_freq) {
		*force_out = false;
		*force_scaling = false;
		host->turbo_down_thres_cnt = 0;
		return;
	} else if (core_clk_rate > UFS_NOM_THRES_FREQ) {
		/* We are currently operating in TURBO freq */
		if (busy_percentage < TURBO_DOWN_THRESHOLD) {
			/*
			 * To avoid ping-pong b/w different clk mode ,
			 * have a threshold count to scale down from TURBO
			 * to NOM mode.
			 */
			if (++host->turbo_down_thres_cnt == 2) {
				/* We need to scale down from TURBO or TURBO_L1 to NOM */
				*force_out = false;
				*force_scaling = true;
				*scale_up = true;
				host->turbo_down_thres_cnt = 0;
				return;
			}
		} else {
			/* Continue with TURBO clk freq */
			host->turbo_down_thres_cnt = 0;
			*force_out = true;
			*force_scaling = false;
			return;
		}
	} else {
		*force_out = false;
		*force_scaling = false;
		 host->turbo_down_thres_cnt = 0;
	}
}
/*
 * ufs_qcom_parse_lpm - read from DTS whether LPM modes should be disabled.
 */
static void ufs_qcom_parse_lpm(struct ufs_qcom_host *host)
{
	struct device_node *node = host->hba->dev->of_node;

	host->disable_lpm = of_property_read_bool(node, "qcom,disable-lpm");
	if (host->disable_lpm)
		dev_info(host->hba->dev, "(%s) All LPM is disabled\n",
			 __func__);
}

/**
 * ufs_qcom_device_reset() - toggle the (optional) device reset line
 * @hba: per-adapter instance
 *
 * Toggles the (optional) reset line to reset the attached device.
 */
static int ufs_qcom_device_reset(struct ufs_hba *hba)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	int ret;

	/* Reset UFS Host Controller and PHY */
	ret = ufs_qcom_host_reset(hba);
	if (ret)
		dev_warn(hba->dev, "%s: host reset returned %d\n",
					__func__, ret);

	/* reset gpio is optional */
	if (!host->device_reset)
		return -EOPNOTSUPP;

	/*
	 * The UFS device shall detect reset pulses of 1us, sleep for 10us to
	 * be on the safe side.
	 */
	ufs_qcom_device_reset_ctrl(hba, true);
	usleep_range(10, 15);

	ufs_qcom_device_reset_ctrl(hba, false);
	usleep_range(10, 15);

	return 0;
}

static void ufs_qcom_config_scaling_param(struct ufs_hba *hba,
					  struct devfreq_dev_profile *p,
					  void *data)
{
	static struct devfreq_simple_ondemand_data *d;

	if (!data)
		return;

	d = (struct devfreq_simple_ondemand_data *)data;
	p->polling_ms = 60;
	p->timer = DEVFREQ_TIMER_DELAYED;
	d->upthreshold = 70;
	d->downdifferential = 65;
}

static struct ufs_dev_fix ufs_qcom_dev_fixups[] = {
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_PA_HIBER8TIME |
		UFS_DEVICE_QUIRK_PA_TX_HSG1_SYNC_LENGTH),
	UFS_FIX(UFS_VENDOR_MICRON, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM),
	UFS_FIX(UFS_VENDOR_SKHYNIX, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM),
	UFS_FIX(UFS_VENDOR_WDC, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_HOST_PA_TACTIVATE),
	END_FIX
};

static void ufs_qcom_fixup_dev_quirks(struct ufs_hba *hba)
{
	ufshcd_fixup_dev_quirks(hba, ufs_qcom_dev_fixups);
}

/*
 * struct ufs_hba_qcom_vops - UFS QCOM specific variant operations
 *
 * The variant operations configure the necessary controller and PHY
 * handshake during initialization.
 */
static const struct ufs_hba_variant_ops ufs_hba_qcom_vops = {
	.name                   = "qcom",
	.init                   = ufs_qcom_init,
	.exit                   = ufs_qcom_exit,
	.get_ufs_hci_version	= ufs_qcom_get_ufs_hci_version,
	.clk_scale_notify	= ufs_qcom_clk_scale_notify,
	.setup_clocks           = ufs_qcom_setup_clocks,
	.hce_enable_notify      = ufs_qcom_hce_enable_notify,
	.link_startup_notify    = ufs_qcom_link_startup_notify,
	.pwr_change_notify	= ufs_qcom_pwr_change_notify,
	.apply_dev_quirks	= ufs_qcom_apply_dev_quirks,
	.suspend		= ufs_qcom_suspend,
	.resume			= ufs_qcom_resume,
	.dbg_register_dump	= ufs_qcom_dump_dbg_regs,
	.device_reset		= ufs_qcom_device_reset,
	.config_scaling_param = ufs_qcom_config_scaling_param,
	.setup_xfer_req         = ufs_qcom_qos,
	.program_key		= ufs_qcom_ice_program_key,
	.fixup_dev_quirks       = ufs_qcom_fixup_dev_quirks,
};

/**
 * QCOM specific sysfs group and nodes
 */
static ssize_t err_state_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	return scnprintf(buf, PAGE_SIZE, "%d\n", !!host->err_occurred);
}

static DEVICE_ATTR_RO(err_state);

static ssize_t power_mode_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	static const char * const names[] = {
		"INVALID MODE",
		"FAST MODE",
		"SLOW MODE",
		"INVALID MODE",
		"FASTAUTO MODE",
		"SLOWAUTO MODE",
		"INVALID MODE",
	};

	/* Print current power info */
	return scnprintf(buf, PAGE_SIZE,
		"[Rx,Tx]: Gear[%d,%d], Lane[%d,%d], PWR[%s,%s], Rate-%c\n",
		hba->pwr_info.gear_rx, hba->pwr_info.gear_tx,
		hba->pwr_info.lane_rx, hba->pwr_info.lane_tx,
		names[hba->pwr_info.pwr_rx],
		names[hba->pwr_info.pwr_tx],
		hba->pwr_info.hs_rate == PA_HS_MODE_B ? 'B' : 'A');
}

static DEVICE_ATTR_RO(power_mode);

static ssize_t bus_speed_mode_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			 !!atomic_read(&host->scale_up));
}

static DEVICE_ATTR_RO(bus_speed_mode);

static ssize_t clk_status_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			 !!atomic_read(&host->clks_on));
}

static DEVICE_ATTR_RO(clk_status);

static unsigned int ufs_qcom_gec(struct ufs_hba *hba, u32 id,
				 char *err_name)
{
	unsigned long flags;
	int i, cnt_err = 0;
	struct ufs_event_hist *e;

	if (id >= UFS_EVT_CNT)
		return -EINVAL;

	e = &hba->ufs_stats.event[id];

	spin_lock_irqsave(hba->host->host_lock, flags);
	for (i = 0; i < UFS_EVENT_HIST_LENGTH; i++) {
		int p = (i + e->pos) % UFS_EVENT_HIST_LENGTH;

		if (e->tstamp[p] == 0)
			continue;
		dev_err(hba->dev, "%s[%d] = 0x%x at %lld us\n", err_name, p,
			e->val[p], ktime_to_us(e->tstamp[p]));
	}
	cnt_err = e->cnt;

	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return cnt_err;
}

static ssize_t err_count_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE,
			 "%s: %d\n%s: %d\n%s: %d\n%s: %d\n",
			 "pa_err_cnt_total",
			 ufs_qcom_gec(hba, UFS_EVT_PA_ERR,
				      "pa_err_cnt_total"),
			 "dl_err_cnt_total",
			 ufs_qcom_gec(hba, UFS_EVT_DL_ERR,
				      "dl_err_cnt_total"),
			 "dme_err_cnt",
			 ufs_qcom_gec(hba, UFS_EVT_DME_ERR,
				      "dme_err_cnt"),
			 "req_abort_cnt",
			  hba->req_abort_count);

}

static DEVICE_ATTR_RO(err_count);

static ssize_t dbg_state_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	bool v;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (kstrtobool(buf, &v))
		return -EINVAL;

	host->dbg_en = v;

	return count;
}

static ssize_t dbg_state_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	return scnprintf(buf, PAGE_SIZE, "%d\n", host->dbg_en);
}


static DEVICE_ATTR_RW(dbg_state);

static ssize_t crash_on_err_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	return scnprintf(buf, PAGE_SIZE, "%d\n", !!host->crash_on_err);
}

static ssize_t crash_on_err_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	bool v;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (kstrtobool(buf, &v))
		return -EINVAL;

	host->crash_on_err = v;

	return count;
}

static DEVICE_ATTR_RW(crash_on_err);

static ssize_t clk_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	static const char * const mode[] = {
				"LOW_SVS",
				"NORM",
				"TURBO",
				"TURBO_L1",
				"INVAL_MODE",
		};

	/* Print current clk mode info */
	return sysfs_emit(buf,
		"Clk_mode: %s ,turbo_enabled: %d, axi_freq:%ld,unipro_freq:%ld, ice_freq: %ld\n",
		mode[host->clk_curr_mode], host->is_turbo_enabled,
		host->curr_axi_freq,
		host->curr_unipro_freq,
		host->curr_ice_freq);
}

static DEVICE_ATTR_RO(clk_mode);

static ssize_t turbo_support_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	/* Print whether turbo support is enabled or not */
	return sysfs_emit(buf, "%d\n", host->ml_scale_sup);
}

static ssize_t turbo_support_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	bool value;
	int ret = 0;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (kstrtobool(buf, &value))
		return -EINVAL;

	down_read(&hba->clk_scaling_lock);
	if (host->clk_curr_mode == TURBO) {
		/*
		 * Since curr_mode is turbo, turbo support
		 * can't be enabled/disabled
		 */
		ret = -EINVAL;
		goto out;
	}

	if (host->ml_scale_sup == value)
		goto out;

	host->ml_scale_sup = value;
	if (atomic_read(&host->scale_up))
		host->clk_curr_mode = NOM;
	else
		host->clk_curr_mode = LOW_SVS;
out:
	up_read(&hba->clk_scaling_lock);
	return ret ? ret : count;
}
static DEVICE_ATTR_RW(turbo_support);

static ssize_t hibern8_count_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	u32 hw_h8_enter;
	u32 sw_h8_enter;
	u32 sw_hw_h8_enter;
	u32 hw_h8_exit;
	u32	sw_h8_exit;
	struct ufs_hba *hba = dev_get_drvdata(dev);

	pm_runtime_get_sync(hba->dev);
	ufshcd_hold(hba, false);
	hw_h8_enter = ufshcd_readl(hba, REG_UFS_HW_H8_ENTER_CNT);
	sw_h8_enter = ufshcd_readl(hba, REG_UFS_SW_H8_ENTER_CNT);
	sw_hw_h8_enter = ufshcd_readl(hba, REG_UFS_SW_AFTER_HW_H8_ENTER_CNT);
	hw_h8_exit = ufshcd_readl(hba, REG_UFS_HW_H8_EXIT_CNT);
	sw_h8_exit = ufshcd_readl(hba, REG_UFS_SW_H8_EXIT_CNT);
	ufshcd_release(hba);
	pm_runtime_put_sync(hba->dev);

	return sysfs_emit(buf,
			 "%s: %d\n%s: %d\n%s: %d\n%s: %d\n%s: %d\n",
			 "hw_h8_enter", hw_h8_enter,
			 "sw_h8_enter", sw_h8_enter,
			 "sw_after_hw_h8_enter", sw_hw_h8_enter,
			 "hw_h8_exit", hw_h8_exit,
			 "sw_h8_exit", sw_h8_exit);

}

static DEVICE_ATTR_RO(hibern8_count);

static ssize_t irq_affinity_support_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	bool value;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (kstrtobool(buf, &value))
		return -EINVAL;

	/* if current irq_affinity_support is same as requested one, don't proceed */
	if (value == host->irq_affinity_support)
		goto out;

	/* if perf-mask is not set, don't proceed */
	if (!host->perf_mask.bits[0])
		goto out;

	host->irq_affinity_support = !!value;
	/* Reset cpu affinity accordingly */
	if (host->irq_affinity_support)
		ufs_qcom_set_affinity_hint(hba, true);
	else
		ufs_qcom_set_affinity_hint(hba, false);

	return count;

out:
	return -EINVAL;
}

static ssize_t irq_affinity_support_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	return scnprintf(buf, PAGE_SIZE, "%d\n", !!host->irq_affinity_support);
}

static DEVICE_ATTR_RW(irq_affinity_support);

static struct attribute *ufs_qcom_sysfs_attrs[] = {
	&dev_attr_err_state.attr,
	&dev_attr_power_mode.attr,
	&dev_attr_bus_speed_mode.attr,
	&dev_attr_clk_status.attr,
	&dev_attr_err_count.attr,
	&dev_attr_dbg_state.attr,
	&dev_attr_crash_on_err.attr,
	&dev_attr_clk_mode.attr,
	&dev_attr_turbo_support.attr,
	&dev_attr_hibern8_count.attr,
	&dev_attr_irq_affinity_support.attr,
	NULL
};

static const struct attribute_group ufs_qcom_sysfs_group = {
	.name = "qcom",
	.attrs = ufs_qcom_sysfs_attrs,
};

static int ufs_qcom_init_sysfs(struct ufs_hba *hba)
{
	int ret;

	ret = sysfs_create_group(&hba->dev->kobj, &ufs_qcom_sysfs_group);
	if (ret)
		dev_err(hba->dev, "%s: Failed to create qcom sysfs group (err = %d)\n",
				 __func__, ret);

	return ret;
}

static void ufs_qcom_hook_send_command(void *param, struct ufs_hba *hba,
				       struct ufshcd_lrb *lrbp)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	if (lrbp && lrbp->cmd && lrbp->cmd->cmnd[0]) {
		struct request *rq = blk_mq_rq_from_pdu(lrbp->cmd);
		int sz = rq ? blk_rq_sectors(rq) : 0;
		ufs_qcom_log_str(host, "<,%x,%d,%x,%d\n",
				lrbp->cmd->cmnd[0],
				lrbp->task_tag,
				ufshcd_readl(hba,
					REG_UTP_TRANSFER_REQ_DOOR_BELL),
				sz);

		if ((host->irq_affinity_support) && atomic_read(&host->hi_pri_en) && rq)
			rq->cmd_flags |= REQ_HIPRI;
	}
}

static void ufs_qcom_hook_compl_command(void *param, struct ufs_hba *hba,
				       struct ufshcd_lrb *lrbp)
{

	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	if (lrbp && lrbp->cmd) {
		int sz = lrbp->cmd->request ?
				blk_rq_sectors(lrbp->cmd->request) : 0;
		ufs_qcom_log_str(host, ">,%x,%d,%x,%d\n",
				lrbp->cmd->cmnd[0],
				lrbp->task_tag,
				ufshcd_readl(hba,
					REG_UTP_TRANSFER_REQ_DOOR_BELL),
				sz);
	}
}

static void ufs_qcom_hook_send_uic_command(void *param, struct ufs_hba *hba,
					struct uic_command *ucmd,
					const char *str)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	unsigned int a, b, c, cmd;
	char ch;

	if (str[0] == 's') {
		a = ucmd->argument1;
		b = ucmd->argument2;
		c = ucmd->argument3;
		cmd = ucmd->command;
		ch = '(';
	} else {
		a = ufshcd_readl(hba, REG_UIC_COMMAND_ARG_1),
		b = ufshcd_readl(hba, REG_UIC_COMMAND_ARG_2),
		c = ufshcd_readl(hba, REG_UIC_COMMAND_ARG_3);
		cmd = ufshcd_readl(hba, REG_UIC_COMMAND);
		ch = ')';
	}
	ufs_qcom_log_str(host, "%c,%x,%x,%x,%x\n",
				ch, cmd, a, b, c);
}

static void ufs_qcom_hook_send_tm_command(void *param, struct ufs_hba *hba,
					int tag, const char *str)
{

}

static void ufs_qcom_hook_check_int_errors(void *param, struct ufs_hba *hba,
					bool queue_eh_work)
{
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);

	if (queue_eh_work)
		ufs_qcom_log_str(host, "_,%x,%x\n",
					hba->errors, hba->uic_error);
}

/*
 * Refer: common/include/trace/hooks/ufshcd.h for available hooks
 */
static void ufs_qcom_register_hooks(void)
{
	register_trace_android_vh_ufs_send_command(ufs_qcom_hook_send_command,
						NULL);
	register_trace_android_vh_ufs_compl_command(
				ufs_qcom_hook_compl_command, NULL);
	register_trace_android_vh_ufs_send_uic_command(
				ufs_qcom_hook_send_uic_command, NULL);
	register_trace_android_vh_ufs_send_tm_command(
				ufs_qcom_hook_send_tm_command, NULL);
	register_trace_android_vh_ufs_check_int_errors(
				ufs_qcom_hook_check_int_errors, NULL);
	register_trace_android_vh_ufs_clock_scaling(
				ufs_qcom_hook_clock_scaling, NULL);
}

#ifdef CONFIG_ARM_QCOM_CPUFREQ_HW
static int ufs_cpufreq_status(void)
{
	struct cpufreq_policy *policy;

	policy = cpufreq_cpu_get(0);
	if (!policy) {
		dev_warn(dev, "cpufreq not probed yet, defer once\n");
		return -EPROBE_DEFER;
	}

	cpufreq_cpu_put(policy);

	return 0;
}
#else
static int ufs_cpufreq_status(void)
{
	return 0;
}
#endif

static int ufs_qcom_read_boot_config(struct platform_device *pdev)
{
	u8 *buf;
	size_t len;
	u32 data;
	struct nvmem_cell *cell;

	cell = nvmem_cell_get(&pdev->dev, "boot_conf");
	if (IS_ERR(cell)) {
		dev_err(&pdev->dev, "nvmem get errs\n");
		return -EINVAL;
	}

	buf = (u8 *)nvmem_cell_read(cell, &len);
	if (IS_ERR(buf)) {
		dev_err(&pdev->dev, "nvmem read err\n");
		return -EINVAL;
	}

	/*
	 * Storage boot device fuse is present in QFPROM_RAW_OEM_CONFIG_ROW0_LSB
	 * this fuse is blown by bootloader and pupulated in boot_config
	 * register[1:5] - hence shift read data by 1 and mask it with 0x1f.
	 */
	data = (*buf) >> 1 & 0x1f;
	is_bootdevice_ufs = (data > 0) ? 0 : UFS_BOOT_DEVICE;
	dev_info(&pdev->dev, "boot_config val = %x, is_bootdevice_ufs = %x\n",
			*buf, is_bootdevice_ufs);

	kfree(buf);
	nvmem_cell_put(cell);

	return is_bootdevice_ufs;
}

/**
 * ufs_qcom_probe - probe routine of the driver
 * @pdev: pointer to Platform device handle
 *
 * Return zero for success and non-zero for failure
 */
static int ufs_qcom_probe(struct platform_device *pdev)
{
	int err = 0;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	if (!ufs_qcom_read_boot_config(pdev)) {
		dev_err(dev, "UFS is not boot dev.\n");
		return err;
	}
	/**
	 * CPUFreq driver is needed for performance reasons.
	 * Assumption - cpufreq gets probed the second time.
	 * If cpufreq is not probed by the time the driver requests for cpu
	 * policy later on, cpufreq would be disabled and performance would be
	 * impacted adversly.
	 */
	err = ufs_cpufreq_status();
	if (err)
		return err;

	/*
	 * On qcom platforms, bootdevice is the primary storage
	 * device. This device can either be eMMC or UFS.
	 * The type of device connected is detected at runtime.
	 * So, if an eMMC device is connected, and this function
	 * is invoked, it would turn-off the regulator if it detects
	 * that the storage device is not ufs.
	 * These regulators are turned ON by the bootloaders & turning
	 * them off without sending PON may damage the connected device.
	 * Hence, check for the connected device early-on & don't turn-off
	 * the regulators.
	 */
	if (of_property_read_bool(np, "non-removable") &&
	    strlen(android_boot_dev) &&
	    strcmp(android_boot_dev, dev_name(dev)))
		return -ENODEV;

	/* Perform generic probe */
	err = ufshcd_pltfrm_init(pdev, &ufs_hba_qcom_vops);
	if (err)
		dev_err(dev, "ufshcd_pltfrm_init() failed %d\n", err);

	ufs_qcom_register_hooks();
	return err;
}

/**
 * ufs_qcom_remove - set driver_data of the device to NULL
 * @pdev: pointer to platform device handle
 *
 * Always returns 0
 */
static int ufs_qcom_remove(struct platform_device *pdev)
{
	struct ufs_hba *hba;
	struct ufs_qcom_host *host;
	struct ufs_qcom_qos_req *r;
	struct qos_cpu_group *qcg;
	int i;

	if (!is_bootdevice_ufs) {
		dev_info(&pdev->dev, "UFS is not boot dev.\n");
		return 0;
	}

	hba =  platform_get_drvdata(pdev);
	host = ufshcd_get_variant(hba);
	r = host->ufs_qos;
	qcg = r->qcg;

	pm_runtime_get_sync(&(pdev)->dev);
	for (i = 0; i < r->num_groups; i++, qcg++)
		remove_group_qos(qcg);
	ufshcd_remove(hba);
	return 0;
}

static void ufs_qcom_shutdown(struct platform_device *pdev)
{
	struct ufs_hba *hba;
	struct scsi_device *sdev;
	struct ufs_qcom_host *host;

	if (!is_bootdevice_ufs) {
		dev_info(&pdev->dev, "UFS is not boot dev.\n");
		return;
	}

	hba =  platform_get_drvdata(pdev);
	host = ufshcd_get_variant(hba);

	ufs_qcom_log_str(host, "0xdead\n");
	pm_runtime_get_sync(hba->dev);

	shost_for_each_device(sdev, hba->host) {
		if (sdev == hba->sdev_ufs_device)
			scsi_device_quiesce(sdev);
		else
			scsi_remove_device(sdev);
	}
	ufshcd_shutdown(hba);

	/* UFS_RESET TLMM register cannot reset to POR value '1' after warm
	 * reset, so deassert ufs device reset line after UFS device shutdown
	 * to ensure the UFS_RESET TLMM register value is POR value
	 */
	ufs_qcom_device_reset_ctrl(hba, false);
}

static const struct of_device_id ufs_qcom_of_match[] = {
	{ .compatible = "qcom,ufshc"},
	{},
};
MODULE_DEVICE_TABLE(of, ufs_qcom_of_match);

#ifdef CONFIG_ACPI
static const struct acpi_device_id ufs_qcom_acpi_match[] = {
	{ "QCOM24A5" },
	{ },
};
MODULE_DEVICE_TABLE(acpi, ufs_qcom_acpi_match);
#endif

static int ufshcd_pltfrm_suspend_wrapper(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	if (hba == NULL || !is_bootdevice_ufs) {
		dev_err(dev, "%s UFS is not boot dev.\n", __func__);
		return 0;
	}
	return ufshcd_pltfrm_suspend(dev);
}

static int ufshcd_pltfrm_resume_wrapper(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	if (hba == NULL || !is_bootdevice_ufs) {
		dev_err(dev, "%s UFS is not boot dev.\n", __func__);
		return 0;
	}
	return ufshcd_pltfrm_resume(dev);
}

static const struct dev_pm_ops ufs_qcom_pm_ops = {
	.suspend	= ufshcd_pltfrm_suspend_wrapper,
	.resume		= ufshcd_pltfrm_resume_wrapper,
	.runtime_suspend = ufshcd_pltfrm_runtime_suspend,
	.runtime_resume  = ufshcd_pltfrm_runtime_resume,
	.runtime_idle    = ufshcd_pltfrm_runtime_idle,
};

static struct platform_driver ufs_qcom_pltform = {
	.probe	= ufs_qcom_probe,
	.remove	= ufs_qcom_remove,
	.shutdown = ufs_qcom_shutdown,
	.driver	= {
		.name	= "ufshcd-qcom",
		.pm	= &ufs_qcom_pm_ops,
		.of_match_table = of_match_ptr(ufs_qcom_of_match),
		.acpi_match_table = ACPI_PTR(ufs_qcom_acpi_match),
	},
};
module_platform_driver(ufs_qcom_pltform);

MODULE_LICENSE("GPL v2");
