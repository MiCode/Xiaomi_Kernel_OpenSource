// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2019 Spreadtrum Communications Inc.

#include <linux/delay.h>
#include <linux/extcon-provider.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/usb/typec.h>
#include <linux/usb/sprd_tcpm.h>
#include <linux/usb/sprd_pd.h>
#include <linux/usb/typec_dp.h>

/* PMIC global registers definition */
#define SC27XX_MODULE_EN		0x1808
#define SC27XX_ARM_CLK_EN0		0x180c
#define SC27XX_RTC_CLK_EN0		0x1810
#define SC27XX_XTL_WAIT_CTRL0		0x1b78
#define UMP9620_MODULE_EN		0x2008
#define UMP9620_ARM_CLK_EN0		0x200c
#define UMP9620_RTC_CLK_EN0		0x2010
#define UMP9620_XTL_WAIT_CTRL0		0x2378

#define SC27XX_TYPEC_PD_EN		BIT(13)
#define SC27XX_CLK_PD_SEL		BIT(10)
#define SC27XX_CLK_PD_EN		BIT(9)
#define SC27XX_XTL_EN			BIT(8)

/* Typec controller registers definition */
#define SC27XX_TYPEC_EN			0x0
#define SC27XX_TYPEC_MODE		0x4
#define SC27XX_TYPEC_PD_CFG		0x8
#define SC27XX_TYPEC_STATUS		0x1c
#define SC27XX_TYPEC_SW_CFG		0x54
#define SC27XX_TYPEC_WC_REG		0x58
#define SC27XX_TYPEC_DBG1		0x60
#define SC27XX_TYPEC_IBIAS		0x70

#define SC27XX_TYPEC_RP_LEVEL(x)	(((x) << 2) & GENMASK(3, 2))
#define SC27XX_TYPEC_TRY_SRC_EN		BIT(7)
#define SC27XX_TYPEC_TRY_SINK_EN	BIT(8)

/* PD controller registers definition */
#define SC27XX_PD_TX_BUF		0x0
#define SC27XX_PD_RX_BUF		0x4
#define SC27XX_PD_HEAD_CFG		0x8
#define SC27XX_PD_CTRL			0xc
#define SC27XX_PD_CFG0			0x10
#define SC27XX_PD_CFG1			0x14
#define SC27XX_PD_MESG_ID_CFG		0x18
#define SC27XX_PD_STS0			0x1c
#define SC27XX_PD_STS1			0x20
#define SC27XX_INT_STS			0x24
#define SC27XX_INT_FLG			0x28
#define SC27XX_INT_CLR			0x2c
#define SC27XX_INT_EN			0x30
#define SC27XX_PD_PHY_CFG0		0x50
#define SC27XX_PD_PHY_CFG1		0x54
#define SC27XX_PD_PHY_CFG2		0x58
#define SC27XX_PD_DEBUG			0x5c

/* Bits definitions for SC27XX_TYPEC_PD_CFG register */
#define SC27XX_TYPEC_PD_SUPPORT		BIT(0)
#define SC27XX_TYPEC_PD_CONSTRACT	BIT(6)
#define SC27XX_TYPEC_PD_NO_CHEK_DETACH	BIT(9)
#define SC27XX_TYPEC_SW_FORCE_CC(x)	(((x) << 10) & GENMASK(11, 10))
#define SC27XX_TYPEC_VCCON_LDO_RDY	BIT(12)
#define SC27XX_TYPEC_VCCON_LDO_EN	BIT(13)

/* Bits definitions for SC27XX_TYPEC_STATUS register */
#define SC27XX_TYPEC_CURRENT_STATUS	GENMASK(4, 0)
#define SC27XX_TYPEC_FINAL_SWITCH	BIT(5)
#define SC27XX_TYPEC_VBUS_CL(x)		(((x) & GENMASK(7, 6)) >> 6)

/* Bits definitions for SC27XX_TYPEC_WC_REG register */
#define SC27XX_TYPEC_PD_PR_SWAP		BIT(2)
#define SC27XX_TYPEC_PD_VCONN_SWAP	BIT(1)

/* Bits definitions for SC27XX_TYPEC_DBG1 register */
#define SC27XX_TYPEC_VBUS_OK		BIT(8)
#define SC27XX_TYPEC_CONN_CC		BIT(9)

/* Bits definitions for SC27XX_TYPEC_IBIAS register */
#define SC27XX_TYPEC_RX_REF_DECREASE	BIT(7)

/* Bits definitions for SC27XX_PD_HEAD_CFG register */
#define SC27XX_PD_EXTHEAD		BIT(15)
#define SC27XX_PD_NUM_DO(x)		(((x) << 12) & GENMASK(14, 12))
#define SC27XX_PD_MESS_ID(x)		(((x) << 9) & GENMASK(11, 9))
#define SC27XX_PD_POWER_ROLE		BIT(8)
#define SC27XX_PD_SPEC_REV(x)		(((x) << 6) & GENMASK(7, 6))
#define SC27XX_PD_DATA_ROLE		BIT(5)
#define SC27XX_PD_MESSAGE_TYPE(x)	((x) & GENMASK(4, 0))
#define SC27XX_PD_SPEC_MASK		GENMASK(7, 6)

/* Bits definitions for SC27XX_PD_CTRL register */
#define SC27XX_PD_TX_START		BIT(0)
#define SC27XX_PD_HARD_RESET		BIT(2)
#define SC27XX_PD_TX_FLASH		BIT(3)
#define SC27XX_PD_RX_FLASH		BIT(4)
#define SC27XX_PD_FAST_START		BIT(6)
#define SC27XX_PD_CABLE_RESET		BIT(7)
#define SC27XX_PD_RX_ID_CLR		BIT(8)
#define SC27XX_PD_RP_SINKTXNG_CLR	BIT(9)

/* Bits definitions for SC27XX_PD_CFG0 register */
#define SC27XX_PD_RP_CONTROL(x)		((x) & GENMASK(1, 0))
#define SC27XX_PD_SINK_RP		GENMASK(3, 2)
#define SC27XX_PD_EN_SOP1_TX		BIT(6)
#define SC27XX_PD_EN_SOP2_TX		BIT(7)
#define SC27XX_PD_EN_SOP		BIT(8)
#define SC27XX_PD_SRC_SINK_MODE		BIT(9)
#define SC27XX_PD_CTL_EN		BIT(10)
#define SC27XX_PD_BIST_MODE_EN		BIT(11)

/* Bits definitions for SC27XX_PD_CFG1 register */
#define SC27XX_PD_AUTO_RETRY		BIT(0)
#define SC27XX_PD_RETRY(x)		(((x) << 1) & GENMASK(2, 1))
#define SC27XX_PD_HEADER_REG_EN		BIT(3)
#define SC27XX_PD_EN_SOP1_DEBUG_RX	BIT(4)
#define SC27XX_PD_EN_SOP2_DEBUG_RX	BIT(5)
#define SC27XX_PD_EN_SOP1_RX		BIT(6)
#define SC27XX_PD_EN_SOP2_RX		BIT(7)
#define SC27XX_PD_EN_SOP_RX		BIT(8)
#define SC27XX_PD_RX_AUTO_GOOD_CRC	BIT(9)
#define SC27XX_PD_FRS_DETECT_EN		BIT(10)
#define SC27XX_PD_PHY_13M		BIT(11)
#define SC27XX_PD_TX_AUTO_GOOD_CRC	BIT(12)
#define SC27XX_PD_GOOD_CRC_VER_SEL	BIT(13)

/* Bits definitions for SC27XX_PD_MESG_ID_CFG register */
#define SC27XX_PD_MESS_ID_TX(x)		((x) & GENMASK(2, 0))
#define SC27XX_PD_MESS_ID_RX(x)		(((x) << 4) & GENMASK(6, 4))
#define SC27XX_PD_RETRY_MASK		GENMASK(2, 1)
#define SC27XX_PD_MESS_ID_MASK		GENMASK(2, 0)

/* Bits definitions for SC27XX_TYPEC_SW_CFG register */
#define SC27XX_TYPEC_SW_SWITCH(x)	(((x) << 10) & GENMASK(11, 10))

/* Bits definitions for SC27XX_INT_FLG register */
#define SC27XX_PD_HARD_RST_FLAG		BIT(0)
#define SC27XX_PD_CABLE_RST_FLAG	BIT(1)
#define SC27XX_PD_SOFT_RST_FLAG		BIT(2)
#define SC27XX_PD_PS_RDY_FLAG		BIT(3)
#define SC27XX_PD_PKG_RV_FLAG		BIT(4)
#define SC27XX_PD_TX_OK_FLAG		BIT(5)
#define SC27XX_PD_TX_ERROR_FLAG		BIT(6)
#define SC27XX_PD_TX_COLLSION_FLAG	BIT(7)
#define SC27XX_PD_PKG_RV_ERROR_FLAG	BIT(8)
#define SC27XX_PD_FRS_RV_FLAG		BIT(9)
#define SC27XX_PD_RX_FIFO_OVERFLOW_FLAG	BIT(10)

/* Bits definitions for SC27XX_PD_STS1 register */
#define SC27XX_PD_RX_DATA_NUM_MASK	GENMASK(8, 0)
#define SC27XX_PD_RX_EMPTY		BIT(13)

/* Bits definitions for SC27XX_INT_CLR register */
#define SC27XX_PD_HARD_RST_RV_CLR	BIT(0)
#define SC27XX_PD_CABLE_RST_RV_CLR	BIT(1)
#define SC27XX_PD_SOFT_RST_RV_CLR	BIT(2)
#define SC27XX_PD_PS_RDY_CLR		BIT(3)
#define SC27XX_PD_PKG_RV_CLR		BIT(4)
#define SC27XX_PD_TX_OK_CLR		BIT(5)
#define SC27XX_PD_TX_ERROR_CLR		BIT(6)
#define SC27XX_PD_TX_COLLSION_CLR	BIT(7)
#define SC27XX_PD_PKG_RV_ERROR_CLR	BIT(8)
#define SC27XX_PD_FRS_RV_CLR		BIT(9)
#define SC27XX_PD_RX_FIFO_OVERFLOW_CLR	BIT(10)

/* Bits definitions for SC27XX_INT_EN register */
#define SC27XX_PD_HARD_RST_RV_EN	BIT(0)
#define SC27XX_PD_CABLE_RST_RV_EN	BIT(1)
#define SC27XX_PD_SOFT_RST_RV_EN	BIT(2)
#define SC27XX_PD_PS_RDY_EN		BIT(3)
#define SC27XX_PD_PKG_RV_EN		BIT(4)
#define SC27XX_PD_TX_OK_EN		BIT(5)
#define SC27XX_PD_TX_ERROR_EN		BIT(6)
#define SC27XX_PD_TX_COLLSION_EN	BIT(7)
#define SC27XX_PD_PKG_RV_ERROR_EN	BIT(8)
#define SC27XX_PD_FRS_RV_EN		BIT(9)
#define SC27XX_PD_RX_FIFO_OVERFLOW_EN	BIT(10)

/* SC27XX_PD_PHY_CFG1 */
#define SC27XX_PD_CC1_SW		BIT(3)
#define SC27XX_PD_CC2_SW		BIT(2)

/* SC27XX_PD_PHY_CFG2 */
#define SC27XX_PD_CFG2_PD_CLK_BIT		BIT(6)
#define SC27XX_PD_CFG2_RX_REF_CAL_BIT		BIT(7)
#define SC27XX_PD_CFG2_RX_REF_CAL_SHIFT		7

/* SC27XX_PD_DEBUG */
#define SC27XX_PD_PHY_RX_STATE_SHIFT	6
#define SC27XX_PD_PHY_RX_CRC		3
#define SC27XX_PD_RX_DATA		1

#define SC27XX_TX_RX_BUF_MASK		GENMASK(15, 0)
#define SC27XX_PD_INT_CLR		GENMASK(13, 0)
#define SC27XX_STATE_MASK		GENMASK(4, 0)
#define SC27XX_EVENT_MASK		GENMASK(15, 0)
#define SC27XX_TYPEC_INT_CLR_MASK	GENMASK(9, 0)
#define SC27XX_PD_HEAD_CONFIG_MASK	GENMASK(15, 0)
#define SC27XX_PD_CFG0_MASK		GENMASK(8, 0)
#define SC27XX_PD_PHY_CFG0_MASK		0x5102
#define	SC27XX_PD_PHY_CFG1_MASK		0x463c
#define SC27XX_PD_PHY_CFG2_MASK		0x40

#define SC27XX_PD_CFG0_RCCAL_MASK	GENMASK(8, 6)
#define SC27XX_PD_CFG0_VTL_MASK		GENMASK(3, 2)
#define SC27XX_PD_CFG0_VTH_MASK		GENMASK(1, 0)
#define SC27XX_PD_CFG0_RCCAL_SHIFT	6
#define SC27XX_PD_CFG0_VTL_SHIFT	2
#define SC27XX_PD_CFG1_VREF_SEL_MASK		GENMASK(7, 6)
#define SC27XX_PD_CFG1_REF_CAL_MASK		GENMASK(14, 12)
#define SC27XX_PD_CFG1_VREF_SEL_SHIFT		6
#define SC27XX_PD_CFG1_REF_CAL_SHIFT		12

#define SC27XX_PD_DATA_MASK		GENMASK(15, 0)
#define SC27XX_INT_CLR_MASK		0x3fff
#define SC27XX_INT_EN_MASK		0x85ff
#define SC27xx_DETECT_TYPEC_DELAY	700
#define SC27XX_RX_STATE_MONITOR_DELAY	5000

/* chunked extended message */
#define SC27XX_CHUNKED_EXT_MSG_MASK	GENMASK(7, 0)
#define SC27XX_CHUNKED_EXT_MSG_SHIFT	8

/* Timeout (us) for pd data ready according pd datasheet */
#define SC27XX_PD_RDY_TIMEOUT		2000
#define SC27XX_PD_POLL_RAW_STATUS	50

#define SC27XX_TYPEC_VBUS_OK_TIMEOUT	80000
#define SC27XX_TYPEC_VBUS_OK_STATUS	100

/* Timeout (us) for vbus ready according need */
#define SC27XX_PD_VBUS_TIMEOUT		200000
#define SC27XX_PD_POLL_VBUS_STATUS	50

/* pmic compatible */
#define SC2730_RC_EFUSE_SHIFT		9
#define SC2730_REF_EFUSE_SHIFT		12
#define SC2730_DELTA_EFUSE_SHIFT	7

#define UMP9620_RC_EFUSE_SHIFT		5
#define UMP9620_REF_EFUSE_SHIFT		6
#define UMP9620_DELTA_EFUSE_SHIFT	9

/* soc compatible */
#define UMS9620_MASK_AON_APB_R2G_ANALOG_BB_TOP_SINDRV_ENA	0x0020
#define UMS9620_REG_AON_APB_MIPI_CSI_POWER_CTRL			0x0350

#define SC27XX_PD_SHIFT(n)		(n)

#define PMIC_SC2730			1
#define PMIC_UMP9620			2

#define SC27XX_PD_RX_ERROR_CNT_THRESHOLD	100
#define SC27XX_PD_RX_ERROR_TIME_THRESHOLD	120
#define SC27XX_PD_HARD_RESET_TIME_THRESHOLD	4
#define SC27XX_PD_RX_ERROR_INT_STATUS		0x100

enum sc27xx_state {
	SC27XX_DETACHED_SNK,
	SC27XX_ATTACHWAIT_SNK,
	SC27XX_ATTACHED_SNK,
	SC27XX_DETACHED_SRC,
	SC27XX_ATTACHWAIT_SRC,
	SC27XX_ATTACHED_SRC,
	SC27XX_POWERED_CABLE,
	SC27XX_AUDIO_CABLE,
	SC27XX_DEBUG_CABLE,
};

enum sc27xx_dp_hpd_status {
	SC27XX_DP_HOT_UNPLUG = 0,
	SC27XX_DP_HOT_PLUG,
	SC27XX_DP_HPD_IRQ,
	SC27XX_DP_TYPE_DISCONNECT,
};

enum sc27xx_pd_clk_mode {
	SC27XX_PD_CLK_D6M_A6M = 0,
	SC27XX_PD_CLK_D13M_A6P5M,
	SC27XX_PD_CLK_D13M_A13M,
};

enum {
	SOURCE_ATTACHED = 1,
	SINK_ATTACHED,
};

struct sc27xx_pd_variant_data {
	u32 efuse_rc_shift;
	u32 efuse_ref_shift;
	u32 efuse_delta_shift;
	u32 id;
	u32 module_en;
	u32 arm_clk_en0;
	u32 rtc_clk_en0;
	u32 xtl_wait_ctrl0;
	u32 aon_apb_r2g_analog_bb_top_sindrv_ena;
	u32 reg_aon_apb_mipi_csi_power_ctrl;
};

static const struct sc27xx_pd_variant_data sc2730_data = {
	.efuse_rc_shift = SC2730_RC_EFUSE_SHIFT,
	.efuse_ref_shift = SC2730_REF_EFUSE_SHIFT,
	.efuse_delta_shift = SC2730_DELTA_EFUSE_SHIFT,
	.id = PMIC_SC2730,
	.module_en = SC27XX_MODULE_EN,
	.arm_clk_en0 = SC27XX_ARM_CLK_EN0,
	.rtc_clk_en0 = SC27XX_RTC_CLK_EN0,
	.xtl_wait_ctrl0 = SC27XX_XTL_WAIT_CTRL0,
};

static const struct sc27xx_pd_variant_data ump9620_data = {
	.efuse_rc_shift = UMP9620_RC_EFUSE_SHIFT,
	.efuse_ref_shift = UMP9620_REF_EFUSE_SHIFT,
	.efuse_delta_shift = UMP9620_DELTA_EFUSE_SHIFT,
	.id = PMIC_UMP9620,
	.module_en = UMP9620_MODULE_EN,
	.arm_clk_en0 = UMP9620_ARM_CLK_EN0,
	.rtc_clk_en0 = UMP9620_RTC_CLK_EN0,
	.xtl_wait_ctrl0 = UMP9620_XTL_WAIT_CTRL0,
	.aon_apb_r2g_analog_bb_top_sindrv_ena =
		UMS9620_MASK_AON_APB_R2G_ANALOG_BB_TOP_SINDRV_ENA,
	.reg_aon_apb_mipi_csi_power_ctrl =
		UMS9620_REG_AON_APB_MIPI_CSI_POWER_CTRL,
};

struct sc27xx_pd {
	struct device *dev;
	struct extcon_dev *edev;
	struct extcon_dev *extcon;
	struct extcon_dev *vbus_extcon;
	struct notifier_block extcon_nb;
	struct notifier_block extcon_audio_nb;
	struct notifier_block vbus_extcon_nb;
	struct sprd_tcpm_port *sprd_tcpm_port;
	struct delayed_work typec_detect_work;
	struct delayed_work  read_msg_work;
	struct delayed_work  rx_state_monitor_work;
	struct workqueue_struct *pd_wq;
	struct regmap *regmap;
	struct regmap *aon_apb;
	struct tcpc_dev tcpc;
	struct mutex lock;
	struct regulator *vbus;
	struct regulator *vconn;
	struct tcpc_config config;
	struct work_struct pd_work;
	struct work_struct rp_work;
	struct work_struct pd_audio_work;
	unsigned long audio_event;
	const struct sc27xx_pd_variant_data *var_data;
	enum sprd_typec_cc_polarity cc_polarity;
	enum sprd_typec_cc_status cc1;
	enum sprd_typec_cc_status cc2;
	enum typec_role role;
	enum typec_data_role data;
	enum sc27xx_state state;
	enum sc27xx_state pre_state;
	bool attached;
	bool constructed;
	bool vconn_on;
	bool vbus_on;
	bool rx_on;
	bool charge_on;
	bool vbus_present;
	bool role_swap;
	u32 base;
	u32 typec_base;
	u32 rc_cal;
	u32 delta_cal;
	u32 ref_cal;
	u32 comp_code;
	int need_retry;
	bool typec_online;
	bool pd_attached;
	bool shutdown_flag;
	bool ignore_msg;
	bool set_rx;
	bool is_sink;
	bool is_source;
	bool sink_connect;
	bool source_connect;
	bool use_pdhub_c2c;
	bool is_first_negotiate;
	int rx_error_cnt;
	int hard_reset_cnt;
	bool ignore_hard_reset;
	bool can_communication;
	bool in_hard_reset;
	struct timespec64 rx_err_start_time;
	struct timespec64 hard_reset_start_time;
	bool vbus_only;
	bool suspend;
	u64 resume_time;

	bool igr_goodcrc_msg;
	bool need_rx_flush;
	bool update_spec_rev;
};

extern void sc27xx_set_typec_mode(int typec_mode_renew);
/*
 * Logging
 */
static void (*sprd_pd_tcpm_log)(struct sprd_tcpm_port *port, const char *dev_tag,
				const char *fmt, va_list args);

static const char *sprd_pd_log_tag = "[sprd_pd_log]";

static void sprd_pd_log(struct sc27xx_pd *pd, const char *fmt, ...)
{
	va_list args;

	if (!pd->pd_attached)
		return;

	va_start(args, fmt);
	sprd_pd_tcpm_log(pd->sprd_tcpm_port, sprd_pd_log_tag, fmt, args);
	va_end(args);
}

static inline struct sc27xx_pd *tcpc_to_sc27xx_pd(struct tcpc_dev *tcpc)
{
	return container_of(tcpc, struct sc27xx_pd, tcpc);
}

static int sc27xx_get_vbus_ok_status(void *data)
{
	int ret = 0;
	u32 reg_val = 0;
	struct sc27xx_pd *pd = (struct sc27xx_pd *)data;

	ret = regmap_read(pd->regmap, pd->typec_base + SC27XX_TYPEC_DBG1, &reg_val);
	if (ret < 0)
		return 0;

	pr_info("SC27XX_TYPEC_DBG1 0x%x\n", reg_val);
	if (reg_val & SC27XX_TYPEC_VBUS_OK)
		return 1;
	else
		return 0;
}

static int sc27xx_pd_set_aon_clock(struct sc27xx_pd *pd, bool on)
{
	int ret;
	u32 mask, reg;

	if (!pd->aon_apb) {
		dev_warn(pd->dev, "aon apb NULL\n");
		return 0;
	}

	mask = pd->var_data->aon_apb_r2g_analog_bb_top_sindrv_ena;
	reg = pd->var_data->reg_aon_apb_mipi_csi_power_ctrl;

	if (on) {
		ret = regmap_update_bits(pd->aon_apb, reg, mask, mask);
		if (ret)
			return ret;
	} else {
		ret = regmap_update_bits(pd->aon_apb, reg, mask, ~mask);
		if (ret)
			return ret;
	}

	return 0;
}

static int sc27xx_pd_clk_cfg(struct sc27xx_pd *pd)
{
	int ret;

	ret = regmap_update_bits(pd->regmap, pd->var_data->module_en,
				 SC27XX_TYPEC_PD_EN, SC27XX_TYPEC_PD_EN);
	if (ret)
		return ret;

	ret = regmap_update_bits(pd->regmap, pd->var_data->arm_clk_en0,
				 SC27XX_CLK_PD_EN, SC27XX_CLK_PD_EN);
	if (ret)
		return ret;

	return regmap_update_bits(pd->regmap, pd->var_data->xtl_wait_ctrl0,
				  SC27XX_XTL_EN, SC27XX_XTL_EN);
}

static int sc27xx_pd_disable_clk(struct sc27xx_pd *pd)
{
	int ret;

	ret = regmap_update_bits(pd->regmap, pd->var_data->module_en,
				 SC27XX_TYPEC_PD_EN, 0);
	if (ret)
		return ret;

	return regmap_update_bits(pd->regmap, pd->var_data->arm_clk_en0,
				 SC27XX_CLK_PD_EN, 0);
}

static int sc27xx_pd_start_drp_toggling(struct tcpc_dev *tcpc,
					enum typec_port_type port_type,
					enum sprd_typec_cc_status cc)
{
	return 0;
}

static int sc27xx_pd_set_typec_roles(struct tcpc_dev *tcpc,
				     enum typec_port_type role,
				     enum typec_data_role data)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret = 0;

	if (!pd->use_pdhub_c2c) {
		sprd_pd_log(pd, "not use_pdhub_c2c data %d, role %d", data, role);
		return 0;
	}

	mutex_lock(&pd->lock);
	if (role == TYPEC_PORT_SNK) {
		pd->role_swap = true;
		sprd_pd_log(pd, "try sink en");
	} else if (role == TYPEC_PORT_SRC) {
		pd->role_swap = true;
		sprd_pd_log(pd, "try source en");
	} else {
		pd->role_swap = false;
		sprd_pd_log(pd, "clear try source and sink en");
	}

	mutex_unlock(&pd->lock);
	return ret;
}

static int sc27xx_pd_force_switch_rp_rd(struct tcpc_dev *tcpc, enum typec_role pwr_role)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret = 0;

	if (!pd->use_pdhub_c2c) {
		sprd_pd_log(pd, "%s, not use_pdhub_c2c, role: %s",
			    __func__, pwr_role == TYPEC_SINK ? "source" : "sink");
		return 0;
	}
	if(pwr_role == TYPEC_SOURCE)
		sc27xx_set_typec_mode(SOURCE_ATTACHED);

	if(pwr_role == TYPEC_SINK)
		sc27xx_set_typec_mode(SINK_ATTACHED);
	
	sprd_pd_log(pd, "%s, power role: %s",
		    __func__, pwr_role == TYPEC_SINK ? "source" : "sink");

	ret = regmap_update_bits(pd->regmap, pd->typec_base + SC27XX_TYPEC_WC_REG,
				 SC27XX_TYPEC_PD_PR_SWAP, SC27XX_TYPEC_PD_PR_SWAP);

	return ret;
}

static int sc27xx_pd_set_cc(struct tcpc_dev *tcpc, enum sprd_typec_cc_status cc)
{
	return 0;
}

static int sc27xx_pd_get_cc(struct tcpc_dev *tcpc,
			    enum sprd_typec_cc_status *cc1,
			    enum sprd_typec_cc_status *cc2)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);

	mutex_lock(&pd->lock);
	*cc1 = pd->cc1;
	*cc2 = pd->cc2;
	mutex_unlock(&pd->lock);
	return 0;
}

static void sc27xx_pd_wait_for_i2c_set_vbus(struct sc27xx_pd *pd)
{
	u64 cur_time;
	int ret, retry_cnt = 0;

	cur_time = ktime_to_ms(ktime_get_boottime());
	if (((cur_time - pd->resume_time) > 0 &&
	    (cur_time - pd->resume_time) <= 80) || pd->suspend) {
		sprd_pd_log(pd, "waitting for i2c resume to ready");
		dev_info(pd->dev, "waitting for i2c resume to ready\n");
		usleep_range(30000, 31000);
		do {
			ret = regulator_is_enabled(pd->vbus);
			if (ret == -ESHUTDOWN) {
				sprd_pd_log(pd, "waitting for i2c resume again");
				dev_info(pd->dev, "waitting for i2c resume again\n");
				usleep_range(20000, 21000);
			}
		} while ((ret == -ESHUTDOWN) && retry_cnt++ < 4);
	}
}

static int sc27xx_pd_set_vbus(struct tcpc_dev *tcpc, bool on, bool charge)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret = 0;

	mutex_lock(&pd->lock);
	if (pd->vbus_on == on) {
		sprd_pd_log(pd, "vbus is already %s\n", on ? "On" : "Off");
		dev_info(pd->dev, "vbus is already %s\n", on ? "On" : "Off");
	} else {
		if (!pd->vbus) {
			pd->vbus = devm_regulator_get_optional(pd->dev, "vbus");
			if (IS_ERR(pd->vbus)) {
				dev_err(pd->dev, "failed to get vbus supply\n");
				pd->vbus = NULL;
				goto set_vbus_done;
			}
		}

		sc27xx_pd_wait_for_i2c_set_vbus(pd);

		if (on) {
			if (!regulator_is_enabled(pd->vbus)) {
				sprd_pd_log(pd, "set vbus on");
				ret = regulator_enable(pd->vbus);
				if (ret)  {
					dev_err(pd->dev, "cannot enable vbus regulator, ret=%d\n", ret);
					goto set_vbus_done;
				}
			}
		} else {
			if (regulator_is_enabled(pd->vbus)) {
				sprd_pd_log(pd, "set vbus off");
				ret = regulator_disable(pd->vbus);
				if (ret)  {
					dev_err(pd->dev, "cannot disable vbus regulator, ret=%d\n", ret);
					goto set_vbus_done;
				}
			}
		}

		pd->vbus_on = on;
		sprd_pd_log(pd, "vbus := %s", on ? "On" : "Off");
		dev_info(pd->dev, "vbus := %s", on ? "On" : "Off");
	}

	if (pd->charge_on == charge)
		dev_info(pd->dev,  "charge is already %s\n",
			    charge ? "On" : "Off");
	else
		pd->charge_on = charge;

set_vbus_done:
	mutex_unlock(&pd->lock);

	return ret;
}

static int sc27xx_pd_get_vbus(struct tcpc_dev *tcpc)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret;

	mutex_lock(&pd->lock);
	ret = pd->vbus_present ? 1 : 0;
	mutex_unlock(&pd->lock);
	return ret;
}

static int sc27xx_pd_set_current_limit(struct tcpc_dev *dev, u32 max_ma, u32 mv)
{
	return 0;
}

static int sc27xx_pd_get_current_limit(struct tcpc_dev *dev)
{
	return 100;
}

static int sc27xx_pd_set_polarity(struct tcpc_dev *tcpc,
				  enum sprd_typec_cc_polarity polarity)
{
	return 0;
}

static int sc27xx_pd_set_roles(struct tcpc_dev *tcpc, bool attached,
			       enum typec_role role, enum typec_data_role data)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret;
	u32 mask, head;

	mutex_lock(&pd->lock);
	pd->role = role;
	pd->data = data;
	pd->attached = attached;
	if (pd->role == TYPEC_SINK)
		mask = (u32)~SC27XX_PD_SRC_SINK_MODE;
	else
		mask = SC27XX_PD_SRC_SINK_MODE;

	ret = regmap_update_bits(pd->regmap,
				 pd->base + SC27XX_PD_CFG0,
				 SC27XX_PD_SRC_SINK_MODE, mask);
	if (ret < 0) {
		sprd_pd_log(pd, "update sink mode failed, ret = %d", ret);
		goto out;
	}

	ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_HEAD_CFG, &head);
	if (ret < 0) {
		sprd_pd_log(pd, "read header failed, ret = %d", ret);
	} else if (ret == 0) {
		sprd_pd_log(pd, "read head, head = 0x%x", head);
		if (data == TYPEC_HOST)
			head |= SPRD_PD_HEADER_DATA_ROLE;
		else
			head &= ~SPRD_PD_HEADER_DATA_ROLE;

		if (role == TYPEC_SOURCE)
			head |= SPRD_PD_HEADER_PWR_ROLE;
		else
			head &= ~SPRD_PD_HEADER_PWR_ROLE;

		sprd_pd_log(pd, "update head, head = 0x%x", head);
		ret = regmap_write(pd->regmap, pd->base + SC27XX_PD_HEAD_CFG, head);
		if (ret < 0)
			sprd_pd_log(pd, "write head cfg fail, ret = %d", ret);
	}
out:
	mutex_unlock(&pd->lock);
	return ret;
}

static int sc27xx_pd_set_vconn(struct tcpc_dev *tcpc, bool enable)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret = 0;

	if (!pd->vconn) {
		dev_warn(pd->dev, "vconn NULL!!!\n");
		return -EINVAL;
	}

	mutex_lock(&pd->lock);

	if (pd->vconn_on == enable) {
		dev_info(pd->dev, "vconn already %s\n", enable ? "On" : "Off");
		goto unlock;
	}

	if (enable)
		ret = regulator_enable(pd->vconn);
	else
		ret = regulator_disable(pd->vconn);

	if (!ret)
		pd->vconn_on = enable;

unlock:
	mutex_unlock(&pd->lock);

	return ret;
}

#if IS_ENABLED(CONFIG_SPRD_TYPEC_DP_ALTMODE)
static int sc27xx_pd_dp_altmode_notify(struct tcpc_dev *tcpc, u32 vdo)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	union extcon_property_value hpd_status;

	if (vdo & DP_STATUS_HPD_STATE) {
		if (vdo & DP_STATUS_IRQ_HPD)
			hpd_status.intval = SC27XX_DP_HPD_IRQ;
		else
			hpd_status.intval = SC27XX_DP_HOT_PLUG;
	} else {
		hpd_status.intval = SC27XX_DP_HOT_UNPLUG;
	}

	dev_info(pd->dev, "%s: vdo:0x%x, hpd_status = %d\n", __func__, vdo, hpd_status.intval);
	extcon_set_state(pd->edev, EXTCON_DISP_DP, true);
	extcon_set_property(pd->edev, EXTCON_DISP_DP, EXTCON_PROP_DISP_HPD, hpd_status);
	extcon_sync(pd->edev, EXTCON_DISP_DP);
	return 0;
}

static void sc27xx_pd_extcon_notify_dp(struct sc27xx_pd *pd)
{
	union extcon_property_value hpd_status;

	hpd_status.intval = SC27XX_DP_TYPE_DISCONNECT;
	extcon_set_state(pd->edev, EXTCON_DISP_DP, true);
	extcon_set_property(pd->edev, EXTCON_DISP_DP, EXTCON_PROP_DISP_HPD, hpd_status);
	extcon_sync(pd->edev, EXTCON_DISP_DP);
	dev_info(pd->dev, "%s:hpd_status = %d\n", __func__, hpd_status.intval);
}
#else
static int sc27xx_pd_dp_altmode_notify(struct tcpc_dev *tcpc, u32 vdo)
{
	return 0;
}

static void sc27xx_pd_extcon_notify_dp(struct sc27xx_pd *pd)
{
	dev_info(pd->dev, "%s: not enable dp\n", __func__);
}
#endif

static int sc27xx_pd_tx_flush(struct sc27xx_pd *pd)
{
	return regmap_update_bits(pd->regmap,
				  pd->base + SC27XX_PD_CTRL, SC27XX_PD_TX_FLASH,
				  SC27XX_PD_TX_FLASH);
}

static int sc27xx_pd_rx_flush(struct sc27xx_pd *pd)
{
	int ret;

	pd->need_rx_flush = false;
	sprd_pd_log(pd, "%s:line%d: start rx flush", __func__, __LINE__);
	ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CTRL,
				 SC27XX_PD_RX_FLASH, SC27XX_PD_RX_FLASH);
	sprd_pd_log(pd, "%s:line%d: rx flush done", __func__, __LINE__);
	return ret;
}

static int sc27xx_pd_tx_rx_flush(struct sc27xx_pd *pd)
{
	int ret;
	u32 mask = SC27XX_PD_TX_FLASH | SC27XX_PD_RX_FLASH;
	u32 val = SC27XX_PD_TX_FLASH | SC27XX_PD_RX_FLASH;

	pd->need_rx_flush = false;
	sprd_pd_log(pd, "%s:line%d: start rx flush", __func__, __LINE__);
	ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CTRL,
				 mask, val);
	sprd_pd_log(pd, "%s:line%d: rx flush done", __func__, __LINE__);
	return ret;
}

static int sc27xx_pd_clear_rx_id(struct sc27xx_pd *pd)
{
	return regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CTRL,
				  SC27XX_PD_RX_ID_CLR, SC27XX_PD_RX_ID_CLR);
}

static int sc27xx_pd_set_tx_id(struct sc27xx_pd *pd, unsigned int tx_id)
{
	return regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_MESG_ID_CFG,
				  SC27XX_PD_MESS_ID_MASK, tx_id);
}

static int sc27xx_pd_reset(struct sc27xx_pd *pd, bool clear_tx_id)
{
	int ret;

	sprd_pd_log(pd, "%s:line%d: clear_tx_id = %d", __func__, __LINE__, clear_tx_id);

	ret = sc27xx_pd_rx_flush(pd);
	if (ret < 0)
		return ret;

	ret = sc27xx_pd_tx_flush(pd);
	if (ret < 0)
		return ret;

	ret = sc27xx_pd_clear_rx_id(pd);
	if (ret < 0)
		return ret;

	if (clear_tx_id) {
		sprd_pd_log(pd, "%s:line%d: clear tx id", __func__, __LINE__);
		ret = sc27xx_pd_set_tx_id(pd, 0x0);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int sc27xx_pd_send_hardreset(struct sc27xx_pd *pd)
{
	int ret, state;
	u64 curr_time;
	static bool first_hardreset_flag = true;

	curr_time = ktime_to_ms(ktime_get_boottime());

	sprd_pd_log(pd, "%s:line%d: send hard reset", __func__, __LINE__);
	if (pd->need_retry) {
		pd->need_retry = false;
		cancel_delayed_work(&pd->read_msg_work);
		sprd_pd_log(pd, "cancel retry read msg done");
	}

	sprd_pd_log(pd, "%s:line%d:  start send hard reset", __func__, __LINE__);
	ret = regmap_update_bits(pd->regmap,
				 pd->base + SC27XX_PD_CTRL,
				 SC27XX_PD_HARD_RESET, SC27XX_PD_HARD_RESET);
	if (ret < 0)
		return ret;
	sprd_pd_log(pd, "%s:line%d:  end send hard reset", __func__, __LINE__);

	ret = sc27xx_pd_reset(pd, true);
	if (ret < 0) {
		dev_err(pd->dev, "cannot PD reset, ret=%d\n", ret);
		return ret;
	}

	sprd_pd_log(pd, "sprd: %s, first_hardreset_flag: %d, can_communication: %d",
		    __func__, first_hardreset_flag, pd->can_communication);
	if (first_hardreset_flag) {
		first_hardreset_flag = false;
		if (curr_time < 10000 || pd->can_communication)
			pd->in_hard_reset = true;
	} else if (pd->can_communication) {
		pd->in_hard_reset = true;
	}

	state = extcon_get_state(pd->edev, EXTCON_CHG_USB_PD);
	if (state == true)
		extcon_set_state_sync(pd->edev, EXTCON_CHG_USB_PD, false);
	else if (state == false)
		extcon_set_state_sync(pd->edev, EXTCON_CHG_USB_PD, true);

	sprd_pd_log(pd, "PD send hardreset, ktime = %lld ms", curr_time);
	dev_warn(pd->dev, "IRQ: PD send hardreset, ktime = %lld ms\n", curr_time);

	return 0;
}

static int sc27xx_pd_enable_tx_auto_retry(struct tcpc_dev *tcpc, bool enable_auto_retry)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret = 0;
	u32 val = SC27XX_PD_AUTO_RETRY;

	mutex_lock(&pd->lock);
	sprd_pd_log(pd, "sprd: %s, enable_auto_retry: %d", __func__, enable_auto_retry);
	if (!enable_auto_retry)
		val = 0;

	ret = regmap_update_bits(pd->regmap,
				 pd->base + SC27XX_PD_CFG1,
				 SC27XX_PD_AUTO_RETRY, val);
	if (ret < 0)
		sprd_pd_log(pd, "%s, failed to write PD_Regs[0x.2x], val = 0x%x, ret = %d",
			    __func__, SC27XX_PD_CFG1, val, ret);

	mutex_unlock(&pd->lock);

	return ret;
}

static int sc27xx_pd_check_tx_goodcrc(struct tcpc_dev *tcpc, bool check_tx_goodcrc)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret = 0;
	u32 val = SC27XX_PD_TX_AUTO_GOOD_CRC;

	mutex_lock(&pd->lock);
	sprd_pd_log(pd, "sprd: %s, check_tx_goodcrc: %d", __func__, check_tx_goodcrc);
	if (!check_tx_goodcrc)
		val = 0;

	ret = regmap_update_bits(pd->regmap,
				 pd->base + SC27XX_PD_CFG1,
				 SC27XX_PD_TX_AUTO_GOOD_CRC, val);
	if (ret < 0) {
		sprd_pd_log(pd, "%s, failed to write PD_Regs[0x.2x], val = 0x%x, ret = %d",
			    __func__, SC27XX_PD_CFG1, val, ret);
		goto done;
	}

	pd->igr_goodcrc_msg = !check_tx_goodcrc;

done:
	mutex_unlock(&pd->lock);

	return ret;
}

static int sc27xx_pd_reset_rx_id(struct tcpc_dev *tcpc)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret = 0;

	mutex_lock(&pd->lock);
	ret = sc27xx_pd_clear_rx_id(pd);
	sprd_pd_log(pd, "reset rx id");
	dev_info(pd->dev, "reset rx id\n");
	mutex_unlock(&pd->lock);

	return ret;
}

static int sc27xx_set_pd_tx_id(struct tcpc_dev *tcpc, unsigned int tx_id)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret = 0;

	mutex_lock(&pd->lock);
	ret = sc27xx_pd_set_tx_id(pd, tx_id);
	sprd_pd_log(pd, "force set tx_id: 0x%x", tx_id);
	dev_info(pd->dev, "force set tx_id: 0x%x\n", tx_id);
	mutex_unlock(&pd->lock);

	return ret;
}

static int sc27xx_pd_set_rx(struct tcpc_dev *tcpc, bool on)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	u32 mask = SC27XX_PD_CTL_EN, mask1 = SC27XX_PD_PKG_RV_EN;
	u32 mask2 = SC27XX_PD_RX_AUTO_GOOD_CRC;
	int ret = 0;
	u32 reg_val = 0;

	if (pd->shutdown_flag)
		return 0;

	mutex_lock(&pd->lock);
	if (pd->rx_on == on) {
		sprd_pd_log(pd, "rx is already %s\n", on ? "On" : "Off");
		dev_info(pd->dev, "rx is already %s\n", on ? "On" : "Off");
		goto done;
	}
	ret = sc27xx_pd_reset(pd, false);
	if (ret < 0)
		goto done;

	ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_STS1, &reg_val);
	if (ret < 0)
		goto done;

	sprd_pd_log(pd, "set rx: on = %d, sts1 = 0x%x", on, reg_val);

	if (on) {
		pd->set_rx = true;
		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_INT_EN,
					 mask1, mask1);
		if (ret < 0)
			goto done;

		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG1,
					 mask2, mask2);
		if (ret < 0)
			goto done;

		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG0,
					 mask, mask);
		if (ret < 0)
			goto done;
	} else {
		pd->set_rx = false;
		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG0,
					 mask, ~mask);
		if (ret < 0)
			goto done;

		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_INT_EN,
					 mask1, ~mask1);
		if (ret < 0)
			goto done;

		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG1,
					 mask2, ~mask2);
		if (ret < 0)
			goto done;
	}

	pd->rx_on = on;

	sprd_pd_log(pd, "set rx: pd := %s", on ? "on" : "off");
	dev_info(pd->dev, "pd := %s", on ? "on" : "off");
done:
	mutex_unlock(&pd->lock);
	return ret;
}

static void sc27xx_pd_set_rx_enable(struct sc27xx_pd *pd, bool on)
{
	u32 mask = SC27XX_PD_CTL_EN, mask1 = SC27XX_PD_PKG_RV_EN;
	u32 mask2 = SC27XX_PD_RX_AUTO_GOOD_CRC;
	int ret;
	u32 reg_val = 0;

	ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_STS1, &reg_val);
	if (ret < 0) {
		dev_err(pd->dev, "%s %d, ret = %d", __func__, __LINE__, ret);
		return;
	}

	sprd_pd_log(pd, "set rx enable: on = %d, sts1 = 0x%x", on, reg_val);

	if (on) {
		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_INT_EN, mask1, mask1);
		if (ret < 0) {
			dev_err(pd->dev, "%s %d, ret = %d", __func__, __LINE__, ret);
			return;
		}

		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG1, mask2, mask2);
		if (ret < 0) {
			dev_err(pd->dev, "%s %d, ret = %d", __func__, __LINE__, ret);
			return;
		}

		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG0, mask, mask);
		if (ret < 0) {
			dev_err(pd->dev, "%s %d, ret = %d", __func__, __LINE__, ret);
			return;
		}
	} else {
		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG0, mask, ~mask);
		if (ret < 0) {
			dev_err(pd->dev, "%s %d, ret = %d", __func__, __LINE__, ret);
			return;
		}

		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_INT_EN, mask1, ~mask1);
		if (ret < 0) {
			dev_err(pd->dev, "%s %d, ret = %d", __func__, __LINE__, ret);
			return;
		}

		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG1, mask2, ~mask2);
		if (ret < 0) {
			dev_err(pd->dev, "%s %d, ret = %d", __func__, __LINE__, ret);
			return;
		}
	}

	sprd_pd_log(pd, "set rx enable: pd := %s", on ? "on" : "off");
	dev_info(pd->dev, "set rx enable:= %s", on ? "on" : "off");
}

static int sc27xx_pd_tx_msg(struct sc27xx_pd *pd, const struct sprd_pd_message *msg)
{
	u16 header;
	u32 data_obj_num, data[SPRD_PD_MAX_PAYLOAD * 2] = {0};
	int i, ret;
	int head = 0;

	if (!pd->need_rx_flush) {
		ret = sc27xx_pd_tx_flush(pd);
		if (ret < 0)
			return ret;
	} else {
		ret = sc27xx_pd_tx_rx_flush(pd);
		if (ret < 0)
			return ret;
	}

	data_obj_num = msg ? sprd_pd_header_cnt(msg->header) : 0;
	if (data_obj_num > SPRD_PD_MAX_PAYLOAD) {
		dev_err(pd->dev, "pd tmsg too long, num=%d\n", data_obj_num);
		return -EINVAL;
	}

	header = msg ? msg->header : 0;
	sprd_pd_log(pd, "tx msg: header = 0x%x", header);
	ret = regmap_write(pd->regmap, pd->base + SC27XX_PD_HEAD_CFG, header);
	if (ret < 0) {
		sprd_pd_log(pd, "write head cfg fail, ret = %d", ret);
		return ret;
	}
//#if 0
//optimize time
	ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_HEAD_CFG, &head);
	if (ret < 0) {
		sprd_pd_log(pd, "read header failed, ret = %d", ret);
		return ret;
	}
	sprd_pd_log(pd, "tx msg: header cfg = 0x%x", head);
//#endif
	if (msg) {
		for (i = 0; i < data_obj_num; i++) {
			data[2 * i] = (msg->payload[i]) & SC27XX_PD_DATA_MASK;
			data[2 * i + 1] = ((msg->payload[i]) >> 16) & SC27XX_PD_DATA_MASK;
		}
	}

	for (i = 0; i < data_obj_num * 2; i++) {
		sprd_pd_log(pd, "tx msg: data[%d] = 0x%x", i, data[i]);
		ret = regmap_write(pd->regmap, pd->base + SC27XX_PD_TX_BUF,
				   data[i]);
		if (ret < 0) {
			sprd_pd_log(pd, "write tx buf fail, ret = %d", ret);
			return ret;
		}
	}

	return regmap_update_bits(pd->regmap,
				  pd->base + SC27XX_PD_CTRL, SC27XX_PD_TX_START,
				  SC27XX_PD_TX_START);
}

static int sc27xx_pd_transmit(struct tcpc_dev *tcpc,
			      enum sprd_tcpm_transmit_type type,
			      const struct sprd_pd_message *msg)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret;

	cancel_delayed_work_sync(&pd->rx_state_monitor_work);
	mutex_lock(&pd->lock);
	switch (type) {
	case SPRD_TCPC_TX_SOP:
		ret = sc27xx_pd_tx_msg(pd, msg);
		sprd_pd_log(pd, "tx msg end");
		if (ret < 0)
			dev_err(pd->dev, "cannot send PD message, ret=%d\n",
				ret);
		break;
	case SPRD_TCPC_TX_HARD_RESET:
		ret = sc27xx_pd_send_hardreset(pd);
		if (ret < 0)
			dev_err(pd->dev, "cann't send hardreset ret=%d\n", ret);
		break;
	default:
		dev_err(pd->dev, "type %d not supported", type);
		ret = -EINVAL;
	}
	mutex_unlock(&pd->lock);

	return ret;
}

static int sc27xx_pd_read_ext_message(struct sc27xx_pd *pd, struct sprd_pd_message *msg)
{
	int ret, i, ext_msg_data_size;
	u32 data[SPRD_PD_MAX_PAYLOAD * 2] = {0};
	u32 ext_msg_header = 0;

	ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_RX_BUF, &ext_msg_header);
	if (ret < 0) {
		dev_err(pd->dev, "%s, failed to read ext_msg_header, ret = %d\n", __func__, ret);
		return ret;
	}

	msg->ext_msg.header = ext_msg_header & SC27XX_TX_RX_BUF_MASK;
	if (!(msg->ext_msg.header & SPRD_PD_EXT_HDR_CHUNKED)) {
		dev_info(pd->dev, "%s, unchunked ext_msg unsupported\n", __func__);
		goto done;
	}

	ext_msg_data_size = sprd_pd_ext_header_data_size(msg->ext_msg.header);
	if (ext_msg_data_size > SPRD_PD_EXT_MAX_CHUNK_DATA) {
		dev_err(pd->dev, "%s, chunked ext_msg too long, data_size=%d\n",
			__func__, ext_msg_data_size);
		goto done;
	}

	/*
	 * According to the datasheet, sc27xx_pd_rx_buf is 16bit,
	 * but PD protocol source code msg->ext_msg.data is 8bit.
	 */
	for (i = 0; i < (ext_msg_data_size + 1) / 2; i++) {
		ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_RX_BUF,
				(u32 *)&data[i]);
		if (ret < 0) {
			dev_err(pd->dev, "%s, failed to read chunked ext_msg data, ret = %d\n",
				__func__, ret);
			return ret;
		}

		msg->ext_msg.data[2 * i] = data[i] & SC27XX_CHUNKED_EXT_MSG_MASK;
		if (2 * i + 1 < ext_msg_data_size)
			msg->ext_msg.data[2 * i + 1] = (data[i] >> SC27XX_CHUNKED_EXT_MSG_SHIFT) &
						       SC27XX_CHUNKED_EXT_MSG_MASK;
	}

done:
	return sc27xx_pd_rx_flush(pd);
}

static bool sc27xx_pd_is_need_rx_flush(struct sc27xx_pd *pd, struct sprd_pd_message *msg)
{
	u32 data_obj_num;
	u32 p0;

	data_obj_num = sprd_pd_header_cnt(msg->header);
	if (!data_obj_num && sprd_pd_header_type(msg->header) == SPRD_PD_CTRL_ACCEPT)
		return false;

	if (!data_obj_num && sprd_pd_header_type(msg->header) == SPRD_PD_CTRL_PS_RDY) {
		if (pd->state == SC27XX_ATTACHED_SNK && pd->is_first_negotiate) {
			pd->is_first_negotiate = false;
			sprd_pd_log(pd, "first negotiate, ps rdy msg, not rx flush");
			return false;
		}

		if (pd->role_swap) {
			sprd_pd_log(pd, "power role swap, ps rdy msg, not rx flush");
			return false;
		}

		pd->need_rx_flush = true;
		sprd_pd_log(pd, "ps rdy msg, not rx flush");
		return false;
	}

	if (data_obj_num && !sprd_pd_header_ext_le(msg->header) &&
	    sprd_pd_header_type_le(msg->header) == SPRD_PD_DATA_VENDOR_DEF) {
		p0 = le32_to_cpu(msg->payload[0]);
		if (SPRD_PD_VDO_SVDM(p0) &&
		    SPRD_PD_VDO_CMDT(p0) == SPRD_CMDT_RSP_ACK &&
		    SPRD_PD_VDO_CMD(p0) == SPRD_CMD_DP_CONFIGURE) {
			sprd_pd_log(pd, "alternate mode negotiate, DP configure msg, not rx flush");
			return false;
		}
	}

	if (pd->need_retry) {
		sprd_pd_log(pd, "need retry, not rx flush");
		return false;
	}

	return true;
}

static void sc27xx_pd_goodcrc_ver_sel(struct sc27xx_pd *pd, bool auto_sel)
{
	int ret;
	u32 mask = SC27XX_PD_GOOD_CRC_VER_SEL, val;

	sprd_pd_log(pd, "update head cfg spec rev");

	if (auto_sel)
		val = 0;
	else
		val = SC27XX_PD_GOOD_CRC_VER_SEL;

	ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG1,
				 mask, val);
	if (ret < 0)
		sprd_pd_log(pd, "write SC27XX_PD_CFG1 goodcrc rev sel fail, ret = %d", ret);
}

static void sc27xx_pd_set_goodcrc_ver_auto(struct sc27xx_pd *pd, struct sprd_pd_message *msg)
{
	u32 data_obj_num;

	if (!pd->update_spec_rev)
		return;

	pd->update_spec_rev = false;
	data_obj_num = sprd_pd_header_cnt(msg->header);

	if (data_obj_num && sprd_pd_header_type(msg->header) == SPRD_PD_DATA_SOURCE_CAP) {
		sprd_pd_log(pd, "receive src cap, update head cfg spec rev");
		sc27xx_pd_goodcrc_ver_sel(pd, true);
	}
}

static int sc27xx_pd_read_msg_pdo(struct sc27xx_pd *pd, struct sprd_pd_message *msg)
{
	u32 data_obj_num, data[SPRD_PD_MAX_PAYLOAD * 2] = {0};
	int i, ret = 0;

	if (sprd_pd_header_ext(msg->header))
		return 0;

	data_obj_num = sprd_pd_header_cnt(msg->header);
	if (data_obj_num > SPRD_PD_MAX_PAYLOAD) {
		sprd_pd_log(pd, "pd msg too long, num=%d", data_obj_num);
		dev_err(pd->dev, "%s, pd msg too long, num=%d\n", __func__, data_obj_num);
		return -EINVAL;
	}

	for (i = 0; i < data_obj_num * 2; i++) {
		ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_RX_BUF, (u32 *)&data[i]);
		if (ret < 0) {
			sprd_pd_log(pd, "%s, failed to read msg data, ret = %d", ret);
			dev_err(pd->dev, "%s, failed to read msg data, ret = %d\n",
				__func__, ret);
			return ret;
		}
	}

	/*
	 * According to the datasheet, sc27xx_pd_rx_buf is 16bit,
	 * but PD protocol source code msg->payload is 32bit,
	 * so need two 16bit assignment one 32bit.
	 */
	for (i = 0; i < data_obj_num; i++)
		msg->payload[i] = data[2 * i + 1] << 16 | data[2 * i];

	return 0;
}

static int sc27xx_pd_retry_cnt_cfg(struct sc27xx_pd *pd, struct sprd_pd_message *msg)
{
	u32 spec = 0;
	int ret = 0;

	spec = sprd_pd_header_rev(msg->header);
	if (spec == 1)
		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG1,
					 SC27XX_PD_RETRY_MASK,
					 SC27XX_PD_RETRY(2));
	else if (spec == 2)
		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG1,
					 SC27XX_PD_RETRY_MASK,
					 SC27XX_PD_RETRY(1));

	if (ret < 0)
		sprd_pd_log(pd, "%s, failed to set retry th, ret=%d", __func__, ret);

	return ret;
}

static bool sc27xx_pd_is_matched_retry_type(struct sc27xx_pd *pd, struct sprd_pd_message *msg)
{
	u32 data_obj_num, type;
	bool is_ext_msg = false;

	data_obj_num = sprd_pd_header_cnt(msg->header);
	is_ext_msg = sprd_pd_header_ext(msg->header);
	type = sprd_pd_header_type(msg->header);
	if (is_ext_msg && (type == SPRD_PD_EXT_STATUS))
		return false;
	else if (!is_ext_msg && data_obj_num &&
		 (type == SPRD_PD_DATA_VENDOR_DEF || type == SPRD_PD_DATA_SOURCE_CAP ||
		  type == SPRD_PD_DATA_REQUEST || type == SPRD_PD_DATA_ALERT))
		return false;

	return true;
}

static int sc27xx_pd_check_message_packages(struct sc27xx_pd *pd, struct sprd_pd_message *msg)
{
	int ret = 0;
	u32 rx_fifo_data_num, data_obj_num,  reg_val = 0;

	ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_STS1, &reg_val);
	if (ret < 0) {
		sprd_pd_log(pd, "read sts1 failed, ret = %d", ret);
		return ret;
	}

	rx_fifo_data_num = reg_val & SC27XX_PD_RX_DATA_NUM_MASK;
	data_obj_num = sprd_pd_header_cnt(msg->header);
	sprd_pd_log(pd, "%s, reg_val = 0x%x, rx fifo data num = %d",
		    __func__, reg_val, rx_fifo_data_num);
	if (pd->need_retry) {
		pd->need_retry = false;
		cancel_delayed_work(&pd->read_msg_work);
		sprd_pd_log(pd, "cancel retry read msg done");
	}

	if ((data_obj_num * 4 + 1) < rx_fifo_data_num &&
	     sc27xx_pd_is_matched_retry_type(pd, msg)) {
		sprd_pd_log(pd, "retry read msg");
		pd->need_retry = true;
		queue_delayed_work(pd->pd_wq, &pd->read_msg_work, msecs_to_jiffies(5));
	} else if (pd->sprd_tcpm_port->data_role_swap &&
		   (data_obj_num * 4 + 1) < rx_fifo_data_num) {
		sprd_pd_log(pd, "data role swap, retry read msg");
		pd->need_retry = true;
		queue_delayed_work(pd->pd_wq, &pd->read_msg_work, msecs_to_jiffies(2));
	}

	return ret;
}

static int sc27xx_pd_read_message(struct sc27xx_pd *pd, struct sprd_pd_message *msg)
{
	int ret;
	u32 header = 0;

	ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_RX_BUF, &header);
	if (ret < 0) {
		sprd_pd_log(pd, "read header failed, ret = %d", ret);
		return ret;
	}

	msg->header = header & SC27XX_TX_RX_BUF_MASK;
	sprd_pd_log(pd, "header = 0x%x, msg header = 0x%x", header, msg->header);

	ret = sc27xx_pd_check_message_packages(pd, msg);
	if (ret) {
		sprd_pd_log(pd, "failed to handle message, ret = %d", ret);
		return ret;
	}

	ret = sc27xx_pd_retry_cnt_cfg(pd, msg);
	if (ret) {
		sprd_pd_log(pd, "%s, failed to configure retry", __func__);
		return ret;
	}

	if (sprd_pd_header_ext(msg->header)) {
		ret = sc27xx_pd_read_ext_message(pd, msg);
		if (ret < 0) {
			dev_err(pd->dev, "%s, not read pd ext_msg, ret=%d\n", __func__, ret);
			return ret;
		}

		sprd_tcpm_pd_receive(pd->sprd_tcpm_port, msg);
		return 0;
	}

	ret = sc27xx_pd_read_msg_pdo(pd, msg);
	if (ret) {
		sprd_pd_log(pd, "%s, failed to read message pdo", __func__);
		return ret;
	}

	sc27xx_pd_set_goodcrc_ver_auto(pd, msg);

	if (!sprd_pd_header_cnt(msg->header) &&
	    sprd_pd_header_type(msg->header) == SPRD_PD_CTRL_GOOD_CRC) {
		if (pd->igr_goodcrc_msg)
			return 0;

		if (!pd->constructed) {
			ret = regmap_update_bits(pd->regmap, pd->typec_base +
						 SC27XX_TYPEC_PD_CFG,
						 SC27XX_TYPEC_PD_CONSTRACT,
						 SC27XX_TYPEC_PD_CONSTRACT);
			if (ret < 0)
				return ret;
			pd->constructed = true;
		}
		sprd_tcpm_pd_transmit_complete(pd->sprd_tcpm_port, SPRD_TCPC_TX_SUCCESS);
	} else {
		if (pd->igr_goodcrc_msg && !sprd_pd_header_cnt(msg->header) &&
		   sprd_pd_header_type(msg->header) == SPRD_PD_CTRL_SOFT_RESET)
			goto done;

		sprd_tcpm_pd_receive(pd->sprd_tcpm_port, msg);
	}

done:
	if (!sc27xx_pd_is_need_rx_flush(pd, msg))
		return 0;

	return sc27xx_pd_rx_flush(pd);
}

static int sc27xx_pd_rc_ref_cal(struct sc27xx_pd *pd)
{
	u32 vol = 0;
	u32 pd_ref = 0;
	u32 val = 0;
	u32 cfg2_bit7, cfg0_vth = 0, cfg0_vtl = 0, typec_ibis = 0;
	u32 cfg0, cfg2, cfg0_mask, cfg2_mask = 0;
	int ret;

	vol = (pd->rc_cal >> SC27XX_PD_SHIFT(pd->var_data->efuse_rc_shift)) & 0x7f;
	pd_ref = (pd->ref_cal >> SC27XX_PD_SHIFT(pd->var_data->efuse_ref_shift)) & 0x7;

	/*
	 * According to the datasheet, depending on the calibration
	 * voltage, different register values should be configured.
	 */

	if (vol >= 0 && vol <= 26)
		val = 0x0;
	if (vol >= 27 && vol <= 37)
		val = 0x1;
	if (vol >= 38 && vol <= 48)
		val = 0x2;
	if (vol >= 49 && vol <= 59)
		val = 0x3;
	if (vol >= 60 && vol <= 69)
		val = 0x4;
	if (vol >= 70 && vol <= 80)
		val = 0x5;
	if (vol >= 81 && vol <= 90)
		val = 0x6;
	if (vol >= 91 && vol <= 127)
		val = 0x7;

	switch (pd_ref) {
	case 0:
		cfg2_bit7 = 0;
		cfg0_vth = 0;
		cfg0_vtl = 2;
		typec_ibis = 0;
		break;
	case 1:
		cfg2_bit7 = 0;
		cfg0_vth = 1;
		cfg0_vtl = 1;
		typec_ibis = 1;
		break;
	case 2:
		cfg2_bit7 = 0;
		cfg0_vth = 1;
		cfg0_vtl = 1;
		typec_ibis = 0;
		break;
	case 3:
		cfg2_bit7 = 1;
		cfg0_vth = 1;
		cfg0_vtl = 1;
		typec_ibis = 0;
		break;
	case 4:
		cfg2_bit7 = 0;
		cfg0_vth = 2;
		cfg0_vtl = 0;
		typec_ibis = 0;
		break;
	}

	if (typec_ibis == 1)
		ret = regmap_update_bits(pd->regmap,
					 pd->typec_base + SC27XX_TYPEC_IBIAS,
					 SC27XX_TYPEC_RX_REF_DECREASE,
					 SC27XX_TYPEC_RX_REF_DECREASE);
	else
		ret = regmap_update_bits(pd->regmap,
					 pd->typec_base + SC27XX_TYPEC_IBIAS,
					 SC27XX_TYPEC_RX_REF_DECREASE, 0);

	cfg0 = (cfg0_vtl << SC27XX_PD_CFG0_VTL_SHIFT) & SC27XX_PD_CFG0_VTL_MASK;
	cfg0 |= (val << SC27XX_PD_CFG0_RCCAL_SHIFT) & SC27XX_PD_CFG0_RCCAL_MASK;
	cfg0 |= cfg0_vth & SC27XX_PD_CFG0_VTH_MASK;
	cfg0_mask = SC27XX_PD_CFG0_RCCAL_MASK |
		    SC27XX_PD_CFG0_VTL_MASK | SC27XX_PD_CFG0_VTH_MASK;

	ret = regmap_update_bits(pd->regmap,
				 pd->base + SC27XX_PD_PHY_CFG0,
				 cfg0_mask, cfg0);
	if (ret < 0)
		return ret;

	cfg2 = (cfg2_bit7 << SC27XX_PD_CFG2_RX_REF_CAL_SHIFT) & 0x80;
	cfg2_mask = SC27XX_PD_CFG2_RX_REF_CAL_BIT;

	return regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_PHY_CFG2,
				  cfg2_mask, cfg2);
}

static int sc27xx_pd_select_clk_mode(struct sc27xx_pd *pd, u8 pd_clk_mode)
{
	int ret;
	u32 arm_clk_en0_mask, pd_cfg1_mask, pd_phy_cfg2_mask;

	switch (pd_clk_mode) {
	case SC27XX_PD_CLK_D6M_A6M:
		arm_clk_en0_mask = (u32)SC27XX_CLK_PD_SEL;
		pd_cfg1_mask = (u32)~SC27XX_PD_PHY_13M;
		pd_phy_cfg2_mask = (u32)~SC27XX_PD_CFG2_PD_CLK_BIT;
		break;
	case SC27XX_PD_CLK_D13M_A13M:
		arm_clk_en0_mask = (u32)~SC27XX_CLK_PD_SEL;
		pd_cfg1_mask = (u32)SC27XX_PD_PHY_13M;
		pd_phy_cfg2_mask = (u32)~SC27XX_PD_CFG2_PD_CLK_BIT;
		break;
	case SC27XX_PD_CLK_D13M_A6P5M:
	default:
		arm_clk_en0_mask = (u32)~SC27XX_CLK_PD_SEL;
		pd_cfg1_mask = (u32)SC27XX_PD_PHY_13M;
		pd_phy_cfg2_mask = (u32)SC27XX_PD_CFG2_PD_CLK_BIT;
		break;
	}

	ret = regmap_update_bits(pd->regmap, pd->var_data->arm_clk_en0,
				 SC27XX_CLK_PD_SEL, arm_clk_en0_mask);
	if (ret < 0) {
		dev_err(pd->dev, "failed to set arm clk en0, ret = %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG1,
				 SC27XX_PD_PHY_13M, pd_cfg1_mask);
	if (ret < 0) {
		dev_err(pd->dev, "failed to set pd cfg1 13m, ret = %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_PHY_CFG2,
				 SC27XX_PD_CFG2_PD_CLK_BIT, pd_phy_cfg2_mask);
	if (ret < 0) {
		dev_err(pd->dev, "failed to set pd cfg2 clk, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int sc27xx_pd_get_clk_mode(struct sc27xx_pd *pd)
{
	int ret;
	u32 val;

	ret = regmap_read(pd->regmap, pd->var_data->arm_clk_en0, &val);
	if (ret < 0) {
		dev_err(pd->dev, "failed to read arm clk en0, ret = %d\n", ret);
		return ret;
	}
	dev_info(pd->dev, "arm_clk_en0 = 0x%x\n", val);

	ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_CFG1, &val);
	if (ret < 0) {
		dev_err(pd->dev, "failed to read pd cfg1 13m, ret = %d\n", ret);
		return ret;
	}
	dev_info(pd->dev, "SC27XX_PD_CFG1 = 0x%x\n", val);

	ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_PHY_CFG2, &val);
	if (ret < 0) {
		dev_err(pd->dev, "failed to read pd cfg2 clk, ret = %d\n", ret);
		return ret;
	}
	dev_info(pd->dev, "SC27XX_PD_PHY_CFG2 = 0x%x\n", val);

	return 0;
}

static int sc27xx_pd_delta_cal(struct sc27xx_pd *pd)
{
	u32 delta_cal = pd->delta_cal;
	u32 vol, vref_sel = 0, ref_cal = 0, delta = 0;
	u32 cfg1, cfg1_mask;

	cfg1_mask = SC27XX_PD_CC1_SW | SC27XX_PD_CC2_SW |
		    SC27XX_PD_CFG1_VREF_SEL_MASK |
		    SC27XX_PD_CFG1_REF_CAL_MASK;

	delta = ((delta_cal >> SC27XX_PD_SHIFT(pd->var_data->efuse_delta_shift)) & 0x7f) +
		pd->comp_code;
	/*
	 * According to the datasheet, delta is efuse caliration
	 * vol = delta * 2 + 1000
	 */
	vol = delta * 2 + 1000;

	/*
	 * According to the datasheet, depending on the calibration
	 * voltage, different register values should be configured.
	 */

	if (vol >= 1185 && vol <= 1195) {
		vref_sel = 0x0;
		ref_cal = 0x0;
	} else if (vol >= 1175 && vol < 1185) {
		vref_sel = 0x0;
		ref_cal = 0x1;
	} else if (vol >= 1165 && vol < 1175) {
		vref_sel = 0x0;
		ref_cal = 0x2;
	} else if (vol >= 1155 && vol < 1165) {
		vref_sel = 0x0;
		ref_cal = 0x3;
	} else if (vol >= 1145 && vol < 1155) {
		vref_sel = 0x0;
		ref_cal = 0x4;
	} else if (vol >= 1135 && vol < 1145) {
		vref_sel = 0x0;
		ref_cal = 0x5;
	} else if (vol >= 1125 && vol < 1135) {
		vref_sel = 0x0;
		ref_cal = 0x6;
	} else if (vol >= 1115 && vol < 1125) {
		vref_sel = 0x1;
		ref_cal = 0x2;
	} else if (vol >= 1105 && vol < 1115) {
		vref_sel = 0x1;
		ref_cal = 0x3;
	} else if (vol >= 1095 && vol < 1105) {
		vref_sel = 0x1;
		ref_cal = 0x4;
	} else if (vol >= 1085 && vol < 1095) {
		vref_sel = 0x1;
		ref_cal = 0x5;
	} else if (vol >= 1075 && vol < 1085) {
		vref_sel = 0x1;
		ref_cal = 0x6;
	} else if (vol >= 1065 && vol < 1075) {
		vref_sel = 0x2;
		ref_cal = 0x2;
	} else if (vol >= 1055 && vol < 1065) {
		vref_sel = 0x2;
		ref_cal = 0x3;
	} else if (vol >= 1045 && vol < 1055) {
		vref_sel = 0x2;
		ref_cal = 0x4;
	} else if (vol >= 1035 && vol < 1045) {
		vref_sel = 0x2;
		ref_cal = 05;
	} else if (vol >= 1025 && vol < 1035) {
		vref_sel = 0x2;
		ref_cal = 0x6;
	} else if (vol >= 1015 && vol < 1025) {
		vref_sel = 0x3;
		ref_cal = 0x2;
	} else if (vol >= 1005 && vol < 1015) {
		vref_sel = 0x3;
		ref_cal = 0x3;
	} else if (vol >= 1000 && vol < 1005) {
		vref_sel = 0x3;
		ref_cal = 0x4;
	}

	cfg1 = SC27XX_PD_CC1_SW | SC27XX_PD_CC2_SW;
	cfg1 |= (vref_sel << SC27XX_PD_CFG1_VREF_SEL_SHIFT)
		& SC27XX_PD_CFG1_VREF_SEL_MASK;
	cfg1 |= (ref_cal << SC27XX_PD_CFG1_REF_CAL_SHIFT)
		& SC27XX_PD_CFG1_REF_CAL_MASK;

	return regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_PHY_CFG1,
				  cfg1_mask, cfg1);
}

static int sc27xx_pd_module_init(struct sc27xx_pd *pd)
{
	int ret;
	u32 mask2 = SC27XX_PD_RX_AUTO_GOOD_CRC;
	enum sc27xx_pd_clk_mode clk_mode;

	ret = sc27xx_pd_delta_cal(pd);
	if (ret < 0)
		return ret;

	ret = sc27xx_pd_rc_ref_cal(pd);
	if (ret < 0)
		return ret;

	if (!pd->use_pdhub_c2c)
		clk_mode = SC27XX_PD_CLK_D13M_A6P5M;
	else
		clk_mode = SC27XX_PD_CLK_D6M_A6M;
	ret = sc27xx_pd_select_clk_mode(pd, clk_mode);
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG1,
				 SC27XX_PD_RETRY(3), SC27XX_PD_RETRY(2));
	if (ret < 0)
		return ret;

	if (pd->use_pdhub_c2c) {
		ret = regmap_update_bits(pd->regmap, pd->typec_base + SC27XX_TYPEC_PD_CFG,
					 SC27XX_TYPEC_PD_SUPPORT, SC27XX_TYPEC_PD_SUPPORT);
		if (ret < 0)
			return ret;
	}

	ret = sc27xx_pd_set_tx_id(pd, 0x0);
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(pd->regmap,
				 pd->base + SC27XX_PD_CFG0,
				 SC27XX_PD_BIST_MODE_EN,
				 SC27XX_PD_BIST_MODE_EN);
	if (ret < 0)
		return ret;

	ret = regmap_write(pd->regmap, pd->base + SC27XX_INT_CLR,
			   SC27XX_INT_CLR_MASK);
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_PD_CFG1,
				 mask2, ~mask2);
	if (ret < 0)
		return ret;

	sc27xx_pd_get_clk_mode(pd);

	return regmap_write(pd->regmap, pd->base + SC27XX_INT_EN,
			    SC27XX_INT_EN_MASK);
}

static int sc27xx_pd_init(struct tcpc_dev *tcpc)
{
	struct sc27xx_pd *pd = tcpc_to_sc27xx_pd(tcpc);
	int ret;

	ret = sc27xx_pd_clk_cfg(pd);
	if (ret)
		return ret;

	return sc27xx_pd_module_init(pd);
}

static void sc27xx_pd_check_rx_state(struct sc27xx_pd *pd)
{
	int ret;
	u32 pd_debug, pd_phy_rx_state, pd_rx_state;

	ret = regmap_read(pd->regmap, pd->base + SC27XX_PD_DEBUG, &pd_debug);
	if (ret < 0) {
		dev_err(pd->dev, "read pd debug failed, ret = %d", ret);
		return;
	}

	pd_phy_rx_state = (pd_debug >> SC27XX_PD_PHY_RX_STATE_SHIFT) & 0x7;
	pd_rx_state = pd_debug & 0x7;

	if (pd->can_communication && pd_phy_rx_state  == SC27XX_PD_PHY_RX_CRC &&
	    pd_rx_state == SC27XX_PD_RX_DATA) {
		sprd_pd_log(pd, "pd controller reset, pd_debug = 0x%x", pd_debug);

		sc27xx_pd_set_rx_enable(pd, false);
		udelay(5);
		sc27xx_pd_set_rx_enable(pd, true);
	}
}

static irqreturn_t sc27xx_pd_irq(int irq, void *dev_id)
{
	struct sc27xx_pd *pd = dev_id;
	struct sprd_pd_message pd_msg = {0};
	u32 int_sts = 0, pd_sts1 = 0;
	int ret, state;
	struct timespec64 cur_time;
	u32 rx_error_mask = SC27XX_PD_PKG_RV_ERROR_EN;
	u64 curr_time;

	curr_time = ktime_to_ms(ktime_get_boottime());

	sprd_pd_log(pd, "pd irq: start handle irq, irq = %d", irq);
	cancel_delayed_work_sync(&pd->rx_state_monitor_work);
	mutex_lock(&pd->lock);
	ret = regmap_read(pd->regmap, pd->base + SC27XX_INT_FLG, &int_sts);
	if (ret < 0) {
		sprd_pd_log(pd, "pd irq: read int sts fail, ret = %d", ret);
		goto done;
	}

	int_sts &= SC27XX_INT_CLR_MASK;
	sprd_pd_log(pd, "pd irq: int sts = 0x%x", int_sts);
	ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_INT_CLR,
				 SC27XX_INT_CLR_MASK,
				 int_sts);
	if (ret < 0) {
		dev_err(pd->dev, "failed to clr int mask, int_sts = %d, ret = %d\n", int_sts, ret);
		sprd_pd_log(pd, "failed to clr int mask, int_sts = %d, ret = %d", int_sts, ret);
		goto done;
	}

	if (int_sts == SC27XX_PD_RX_ERROR_INT_STATUS) {
		if (!pd->rx_error_cnt)
			pd->rx_err_start_time = ktime_to_timespec64(ktime_get_boottime());
		pd->rx_error_cnt++;
	} else {
		pd->rx_error_cnt = 0;
	}

	if (pd->rx_error_cnt > SC27XX_PD_RX_ERROR_CNT_THRESHOLD) {
		sprd_pd_log(pd, "pd irq: pd->rx_error_cnt = %d", pd->rx_error_cnt);
		cur_time = ktime_to_timespec64(ktime_get_boottime());
		sprd_pd_log(pd, "pd irq: cur_time.tv_sec = %d", cur_time.tv_sec);
		sprd_pd_log(pd, "pd irq: start_time.tv_sec = %d", pd->rx_err_start_time.tv_sec);
		if ((cur_time.tv_sec - pd->rx_err_start_time.tv_sec) <=
		    SC27XX_PD_RX_ERROR_TIME_THRESHOLD) {
			sprd_pd_log(pd, "pd irq: disable rx error int and ignore hard reset");
			pd->ignore_hard_reset = true;
			regmap_update_bits(pd->regmap, pd->base + SC27XX_INT_EN,
					   rx_error_mask, ~rx_error_mask);
		}
	}

	if (pd->shutdown_flag) {
		ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_INT_CLR,
					 SC27XX_INT_CLR_MASK,
					 SC27XX_INT_CLR_MASK);
		dev_info(pd->dev, "SC27XX_INT_FLG(0x28)=0x%x, ret=%d ->: return!!!\n", int_sts, ret);
		goto done;
	}

	if (int_sts & SC27XX_PD_HARD_RST_FLAG) {
		dev_warn(pd->dev, "IRQ: PD received hardreset, ktime = %lld ms\n", curr_time);
		sprd_pd_log(pd, "pd irq: receive hard reset, ktime = %lld ms", curr_time);
		if (pd->ignore_hard_reset) {
			sprd_pd_log(pd, "pd irq: ignore hard reset, hard_reset_cnt = %d",
				    pd->hard_reset_cnt);
			if (pd->hard_reset_cnt) {
				cur_time = ktime_to_timespec64(ktime_get_boottime());
				sprd_pd_log(pd, "pd irq: hard reset cur_time.tv_sec = %d",
					    cur_time.tv_sec);
				if ((cur_time.tv_sec - pd->hard_reset_start_time.tv_sec) <=
				    SC27XX_PD_HARD_RESET_TIME_THRESHOLD) {
					pd->hard_reset_cnt = 0;
					sprd_pd_log(pd, "pd irq: handle hard reset");
					goto irq_hard_reset;
				} else {
					pd->hard_reset_cnt = 0;
				}
			}

			if (pd->can_communication) {
				pd->hard_reset_start_time =
					ktime_to_timespec64(ktime_get_boottime());
				sprd_pd_log(pd, "pd irq: hard_reset_start_time.tv_sec = %d",
					    pd->hard_reset_start_time.tv_sec);
				pd->hard_reset_cnt++;
			}
			goto done;
		}
irq_hard_reset:

		if (pd->need_retry) {
			pd->need_retry = false;
			cancel_delayed_work(&pd->read_msg_work);
			sprd_pd_log(pd, "cancel retry read msg done");
		}

		ret = sc27xx_pd_reset(pd, true);
		if (ret < 0) {
			dev_err(pd->dev, "cannot PD reset, ret=%d\n", ret);
			goto done;
		}

		if (pd->can_communication)
			pd->in_hard_reset = true;

		state = extcon_get_state(pd->edev, EXTCON_CHG_USB_PD);
		if (state == true)
			extcon_set_state_sync(pd->edev, EXTCON_CHG_USB_PD, false);
		else if (state == false)
			extcon_set_state_sync(pd->edev, EXTCON_CHG_USB_PD, true);

		sprd_tcpm_pd_hard_reset(pd->sprd_tcpm_port);
	}

	if (int_sts & SC27XX_PD_CABLE_RST_FLAG) {
		sprd_pd_log(pd, "pd irq: cable reset flag");
		dev_warn(pd->dev, "IRQ: PD cable rst flag\n");
	}

	if (int_sts & SC27XX_PD_SOFT_RST_FLAG) {
		sprd_pd_log(pd, "pd irq: soft reset flag");
		if (pd->igr_goodcrc_msg)
			sprd_tcpm_pd_soft_reset(pd->sprd_tcpm_port);
	}

	if (int_sts & SC27XX_PD_TX_OK_FLAG) {
		pd->can_communication = true;
		if (pd->in_hard_reset)
			pd->in_hard_reset = false;

		sprd_tcpm_pd_transmit_complete(pd->sprd_tcpm_port, SPRD_TCPC_TX_SUCCESS);
	}

	if (int_sts & SC27XX_PD_TX_ERROR_FLAG) {
		sprd_pd_log(pd, "pd irq: tx error failed");
		dev_err(pd->dev, "IRQ: tx error failed\n");
		sprd_tcpm_pd_transmit_complete(pd->sprd_tcpm_port, SPRD_TCPC_TX_FAILED);
	}

	if (int_sts & SC27XX_PD_TX_COLLSION_FLAG) {
		sprd_pd_log(pd, "pd irq: PD collision, ktime = %lld ms", curr_time);
		dev_err(pd->dev, "IRQ: PD collision, ktime = %lld ms\n", curr_time);

		sc27xx_pd_check_rx_state(pd);
		sprd_tcpm_pd_transmit_complete(pd->sprd_tcpm_port, SPRD_TCPC_TX_FAILED);
	}

	if ((int_sts & SC27XX_PD_PKG_RV_FLAG)) {
		pd->can_communication = true;
		if (pd->in_hard_reset)
			pd->in_hard_reset = false;
	       /*
		* According to the requirements of SC2730 ASIC spec, after receiving
		* the interrupt,the data should be read after 500us.
		*/
		if (pd->var_data->id == PMIC_SC2730) {
			sprd_pd_log(pd, "pd irq: delay 500 us");
			usleep_range(500, 510);
		}
		ret = regmap_read_poll_timeout(pd->regmap,
					       pd->base + SC27XX_PD_STS1,
					       pd_sts1,
					       (pd_sts1 & (~SC27XX_PD_RX_EMPTY)),
					       SC27XX_PD_POLL_RAW_STATUS,
					       SC27XX_PD_RDY_TIMEOUT);
		if (ret < 0) {
			sprd_pd_log(pd, "failed to read rx status, ret = %d", ret);
			dev_err(pd->dev, "failed to read rx status, ret = %d\n", ret);
			goto done;
		}

		ret = sc27xx_pd_read_message(pd, &pd_msg);
		if (ret < 0) {
			sprd_pd_log(pd, "not read PD msg, ret=%d", ret);
			dev_err(pd->dev, "not read PD msg, ret=%d\n", ret);
			goto done;
		}
	}

	if (int_sts & SC27XX_PD_PS_RDY_FLAG)
		sprd_pd_log(pd, "pd irq: ps rdy flag, ktime = %lld ms", curr_time);


	if (int_sts & SC27XX_PD_PKG_RV_ERROR_FLAG) {
		sprd_pd_log(pd, "pd irq: PD rx error flag, ktime = %lld ms", curr_time);
		dev_err(pd->dev, "IRQ: PD rx error flag, ktime = %lld ms\n", curr_time);
	}

	if (int_sts & SC27XX_PD_FRS_RV_FLAG)
		sprd_pd_log(pd, "pd irq: SC27XX_PD_FRS_RV_FLAG");

	if (int_sts & SC27XX_PD_RX_FIFO_OVERFLOW_FLAG) {
		sprd_pd_log(pd, "pd irq: PD rx fifo overflow flag");
		dev_err(pd->dev, "IRQ: PD rx fifo overflow flag\n");
	}

done:
	mutex_unlock(&pd->lock);
	queue_delayed_work(system_unbound_wq, &pd->rx_state_monitor_work,
			   msecs_to_jiffies(SC27XX_RX_STATE_MONITOR_DELAY));
	sprd_pd_log(pd, "pd irq: IRQ_HANDLED");

	return IRQ_HANDLED;
}

static int sc27xx_get_vbus_status(struct sc27xx_pd *pd)
{
	u32 status = 0;
	bool vbus_present;
	int ret;

	if (pd->typec_online) {
		if (pd->state == SC27XX_ATTACHED_SRC) {
			sprd_pd_log(pd, "connect SC27XX_ATTACHED_SRC not get vbus ok");
			return 0;
		}
		sprd_pd_log(pd, "online start to poll vbus status");
		ret = regmap_read_poll_timeout(pd->regmap,
					       pd->typec_base + SC27XX_TYPEC_DBG1,
					       status,
					       (status & SC27XX_TYPEC_VBUS_OK),
					       SC27XX_PD_POLL_VBUS_STATUS,
					       SC27XX_PD_VBUS_TIMEOUT);
		if (ret < 0) {
			dev_err(pd->dev, "failed to get vbus status, ret = %d\n", ret);
			return ret;
		}
		sprd_pd_log(pd, "online finish to poll vbus status");
	} else {
		if (pd->pre_state == SC27XX_ATTACHED_SRC) {
			pd->pre_state = SC27XX_DETACHED_SNK;
			sprd_pd_log(pd, "disconnect SC27XX_ATTACHED_SRC not get vbus ok");
			return 0;
		}
		pd->pre_state = SC27XX_DETACHED_SNK;
		/* purpose: wait for vbus to step down */
		ret = regmap_read_poll_timeout(pd->regmap,
					       pd->typec_base + SC27XX_TYPEC_DBG1,
					       status,
					       (!(status & SC27XX_TYPEC_VBUS_OK)),
					       SC27XX_TYPEC_VBUS_OK_STATUS,
					       SC27XX_TYPEC_VBUS_OK_TIMEOUT);
		if (ret < 0) {
			sprd_pd_log(pd, "%s, failed to read typec_reg[0x%x], ret=%d",
				    __func__, SC27XX_TYPEC_DBG1, ret);
			pd->vbus_present = false;
			sprd_tcpm_vbus_change(pd->sprd_tcpm_port);
			return ret;
		}
	}

	vbus_present = !!(status & SC27XX_TYPEC_VBUS_OK);
	sprd_pd_log(pd, "vbus status = 0x%x, vbus_present = %d", status, vbus_present);
	if (vbus_present != pd->vbus_present) {
		pd->vbus_present = vbus_present;
		sprd_tcpm_vbus_change(pd->sprd_tcpm_port);
	}

	return 0;
}

static void sc27xx_cc_polarity_status(struct sc27xx_pd *pd, u32 status)
{
	if (status & SC27XX_TYPEC_FINAL_SWITCH)
		pd->cc_polarity = SPRD_TYPEC_POLARITY_CC1;
	else
		pd->cc_polarity = SPRD_TYPEC_POLARITY_CC2;
}

static void sc27xx_cc_status(struct sc27xx_pd *pd, u32 status)
{
	u32 cc_rp, rp_sts = SC27XX_TYPEC_VBUS_CL(status);
	enum sprd_typec_cc_status cc1;
	enum sprd_typec_cc_status cc2;

	switch (rp_sts) {
	case 0:
		cc_rp = SPRD_TYPEC_CC_RP_DEF;
		break;
	case 1:
		cc_rp = SPRD_TYPEC_CC_RP_1_5;
		break;
	case 2:
		cc_rp = SPRD_TYPEC_CC_RP_3_0;
		break;
	default:
		cc_rp = SPRD_TYPEC_CC_OPEN;
		break;
	}

	switch (pd->state) {
	case SC27XX_ATTACHED_SNK:
		if (pd->cc_polarity == SPRD_TYPEC_POLARITY_CC1) {
			cc1 = cc_rp;
			cc2 = SPRD_TYPEC_CC_OPEN;
		} else {
			cc1 = SPRD_TYPEC_CC_OPEN;
			cc2 = cc_rp;
		}
		break;

	case SC27XX_ATTACHED_SRC:
		if (pd->cc_polarity == SPRD_TYPEC_POLARITY_CC1) {
			cc1 = SPRD_TYPEC_CC_RD;
			cc2 = SPRD_TYPEC_CC_OPEN;
		} else {
			cc1 = SPRD_TYPEC_CC_OPEN;
			cc2 = SPRD_TYPEC_CC_RD;
		}
		break;

	case SC27XX_POWERED_CABLE:
		if (pd->cc_polarity == SPRD_TYPEC_POLARITY_CC1) {
			cc1 = SPRD_TYPEC_CC_RD;
			cc2 = SPRD_TYPEC_CC_RA;
		} else {
			cc1 = SPRD_TYPEC_CC_RA;
			cc2 = SPRD_TYPEC_CC_RD;
		}
		break;
	default:
		cc1 = SPRD_TYPEC_CC_OPEN;
		cc2 = SPRD_TYPEC_CC_OPEN;
		break;
	}

	if (pd->cc1 != cc1 || pd->cc2 != cc2) {
		pd->cc1 = cc1;
		pd->cc2 = cc2;
		sprd_tcpm_cc_change(pd->sprd_tcpm_port);
	}
}

static void sc27xx_cc_status_use_pdhub_c2c(struct sc27xx_pd *pd, u32 status)
{
	u32 cc_rp, rp_sts = SC27XX_TYPEC_VBUS_CL(status);
	enum sprd_typec_cc_status cc1;
	enum sprd_typec_cc_status cc2;

	switch (rp_sts) {
	case 0:
		cc_rp = SPRD_TYPEC_CC_RP_DEF;
		break;
	case 1:
		cc_rp = SPRD_TYPEC_CC_RP_1_5;
		break;
	case 2:
		cc_rp = SPRD_TYPEC_CC_RP_3_0;
		break;
	default:
		cc_rp = SPRD_TYPEC_CC_OPEN;
		break;
	}

	switch (pd->state) {
	case SC27XX_ATTACHED_SNK:
	case SC27XX_DEBUG_CABLE:
		if (pd->cc_polarity == SPRD_TYPEC_POLARITY_CC1) {
			cc1 = cc_rp;
			cc2 = SPRD_TYPEC_CC_OPEN;
		} else {
			cc1 = SPRD_TYPEC_CC_OPEN;
			cc2 = cc_rp;
		}
		break;

	case SC27XX_ATTACHED_SRC:
		if (pd->cc_polarity == SPRD_TYPEC_POLARITY_CC1) {
			cc1 = SPRD_TYPEC_CC_RD;
			cc2 = SPRD_TYPEC_CC_OPEN;
		} else {
			cc1 = SPRD_TYPEC_CC_OPEN;
			cc2 = SPRD_TYPEC_CC_RD;
		}
		break;

	case SC27XX_POWERED_CABLE:
		if (pd->cc_polarity == SPRD_TYPEC_POLARITY_CC1) {
			cc1 = SPRD_TYPEC_CC_RD;
			cc2 = SPRD_TYPEC_CC_RA;
		} else {
			cc1 = SPRD_TYPEC_CC_RA;
			cc2 = SPRD_TYPEC_CC_RD;
		}
		break;
	default:
		cc1 = SPRD_TYPEC_CC_OPEN;
		cc2 = SPRD_TYPEC_CC_OPEN;
		break;
	}

	if (!pd->source_connect && pd->state == SC27XX_DEBUG_CABLE &&
	    pd->pre_state == SC27XX_ATTACHED_SRC) {
		cc1 = SPRD_TYPEC_CC_OPEN;
		cc2 = SPRD_TYPEC_CC_OPEN;
		sprd_pd_log(pd, "source connect false, cc open");
	}

	if (pd->cc1 != cc1 || pd->cc2 != cc2) {
		pd->cc1 = cc1;
		pd->cc2 = cc2;
		sprd_tcpm_cc_change(pd->sprd_tcpm_port);
	}
}

static int sc27xx_pd_typec_connect(struct sc27xx_pd *pd)
{
	enum typec_data_role data_role = TYPEC_DEVICE;
	enum typec_role power_role = TYPEC_SINK;
	enum typec_role vconn_role = TYPEC_SINK;

	sprd_pd_log(pd, "connect, use_pdhub_c2c = %d", pd->use_pdhub_c2c);

	if (!pd->use_pdhub_c2c)
		return 0;

	if (pd->vbus_only) {
		pd->vbus_only = false;
		sprd_pd_log(pd, "typec connect, unregister typec partner, first");
		typec_unregister_partner(pd->sprd_tcpm_port->partner);
		pd->sprd_tcpm_port->partner = NULL;
	}

	sprd_pd_log(pd, "is_sink = %d, is_source = %d", pd->is_sink, pd->is_source);

	if (!pd->is_sink && !pd->is_source)
		return 0;

	sprd_pd_log(pd, "pd->state = %d", pd->state);

	switch (pd->state) {
	case SC27XX_ATTACHED_SNK:
	case SC27XX_DEBUG_CABLE:
		power_role = TYPEC_SINK;
		data_role = TYPEC_DEVICE;
		vconn_role = TYPEC_SINK;
		break;
	case SC27XX_ATTACHED_SRC:
		power_role = TYPEC_SOURCE;
		data_role = TYPEC_HOST;
		vconn_role = TYPEC_SOURCE;
		break;
	default:
		break;
	}

	typec_set_pwr_opmode(pd->sprd_tcpm_port->typec_port, TYPEC_PWR_MODE_USB);
	typec_set_pwr_role(pd->sprd_tcpm_port->typec_port, power_role);
	typec_set_data_role(pd->sprd_tcpm_port->typec_port, data_role);
	typec_set_vconn_role(pd->sprd_tcpm_port->typec_port, vconn_role);

	return 0;
}

static int sc27xx_pd_typec_connect_vbus_only(struct sc27xx_pd *pd)
{
	enum typec_data_role data_role = TYPEC_DEVICE;
	enum typec_role power_role = TYPEC_SINK;
	enum typec_role vconn_role = TYPEC_SINK;
	struct typec_partner_desc desc = {0, TYPEC_ACCESSORY_NONE, NULL, 0x0300};

	sprd_pd_log(pd, "connect, use_pdhub_c2c = %d", pd->use_pdhub_c2c);

	if (!pd->use_pdhub_c2c)
		return 0;

	sprd_pd_log(pd, "is_sink = %d, is_source = %d", pd->is_sink, pd->is_source);

	if (!pd->is_sink && !pd->is_source)
		return 0;

	sprd_pd_log(pd, "vbus only, pd->state = %d", pd->state);

	pd->sprd_tcpm_port->partner = typec_register_partner(pd->sprd_tcpm_port->typec_port, &desc);
	if (!pd->sprd_tcpm_port->partner)
		return -ENODEV;

	sprd_pd_log(pd, "connect, register typec partner");

	typec_set_pwr_opmode(pd->sprd_tcpm_port->typec_port, TYPEC_PWR_MODE_USB);
	typec_set_pwr_role(pd->sprd_tcpm_port->typec_port, power_role);
	typec_set_data_role(pd->sprd_tcpm_port->typec_port, data_role);
	typec_set_vconn_role(pd->sprd_tcpm_port->typec_port, vconn_role);

	return 0;
}

static int sc27xx_pd_typec_disconnect(struct sc27xx_pd *pd)
{
	sprd_pd_log(pd, "disconnect, use_pdhub_c2c = %d", pd->use_pdhub_c2c);

	if (!pd->use_pdhub_c2c)
		return 0;

	if (pd->vbus_only) {
		sprd_pd_log(pd, "disconnect, unregister typec partner");
		typec_unregister_partner(pd->sprd_tcpm_port->partner);
		pd->sprd_tcpm_port->partner = NULL;
	}

	typec_set_pwr_opmode(pd->sprd_tcpm_port->typec_port, TYPEC_PWR_MODE_USB);
	typec_set_pwr_role(pd->sprd_tcpm_port->typec_port, TYPEC_SINK);
	typec_set_data_role(pd->sprd_tcpm_port->typec_port, TYPEC_DEVICE);
	typec_set_vconn_role(pd->sprd_tcpm_port->typec_port, TYPEC_SINK);

	return 0;
}

static int sc27xx_pd_update_header(struct sc27xx_pd *pd)
{
	int ret;

	if (pd->state == SC27XX_ATTACHED_SNK) {
		sprd_pd_log(pd, "update head cfg");
		ret = regmap_update_bits(pd->regmap,
					 pd->base + SC27XX_PD_HEAD_CFG,
					 SC27XX_PD_POWER_ROLE, 0);
		if (ret < 0) {
			sprd_pd_log(pd, "write head cfg power role fail, ret = %d", ret);
			return ret;
		}

		ret = regmap_update_bits(pd->regmap,
					 pd->base + SC27XX_PD_HEAD_CFG,
					 SC27XX_PD_DATA_ROLE, 0);
		if (ret < 0) {
			sprd_pd_log(pd, "write head cfg data role fail, ret = %d", ret);
			return ret;
		}
	}

	return 0;
}

static int sc27xx_pd_update_goodcrc_sel_spec_rev(struct sc27xx_pd *pd)
{
	int ret;

	if (pd->state == SC27XX_ATTACHED_SNK) {
		pd->update_spec_rev = true;
		sprd_pd_log(pd, "attach sink, update head cfg spec rev");
		sc27xx_pd_goodcrc_ver_sel(pd, false);
		ret = regmap_update_bits(pd->regmap,
					 pd->base + SC27XX_PD_HEAD_CFG,
					 SC27XX_PD_SPEC_MASK, SC27XX_PD_SPEC_REV(1));
		if (ret < 0) {
			sprd_pd_log(pd, "write head cfg spec rev fail, ret = %d", ret);
			return ret;
		}
	} else if (pd->state == SC27XX_ATTACHED_SRC) {
		sprd_pd_log(pd, "attach src, update head cfg spec rev");
		sc27xx_pd_goodcrc_ver_sel(pd, true);
	}

	return 0;
}

static int sc27xx_pd_check_vbus_cc_status(struct sc27xx_pd *pd)
{
	u32 val = 0;
	int ret;
	u32 rx_error_mask = SC27XX_PD_PKG_RV_ERROR_EN;

	ret = regmap_read(pd->regmap, pd->typec_base + SC27XX_TYPEC_STATUS,
			  &val);
	if (ret)
		return ret;

	ret = sc27xx_pd_set_aon_clock(pd, true);
	if (ret)
		return ret;

	ret = sc27xx_pd_clk_cfg(pd);
	if (ret)
		return ret;

	pd->state = val & SC27XX_STATE_MASK;
	sprd_pd_log(pd, "typec status = %d", pd->state);

	if (!pd->source_connect && pd->state == SC27XX_DEBUG_CABLE &&
	    pd->pre_state == SC27XX_ATTACHED_SRC) {
		sprd_pd_log(pd, "source connect false");
		goto detach;
	}

	if (pd->use_pdhub_c2c && (pd->state == SC27XX_ATTACHED_SNK ||
				  pd->state == SC27XX_ATTACHED_SRC ||
				  pd->state == SC27XX_DEBUG_CABLE)) {
		pd->pre_state = pd->state;
		sprd_pd_log(pd, "use pdhub c2c typec plug in");
		if (pd->typec_online) {
			sprd_pd_log(pd, "typec online already");
			goto out;
		}
		pd->typec_online = true;
		pd->is_first_negotiate = true;
		sc27xx_pd_update_goodcrc_sel_spec_rev(pd);
		sc27xx_pd_typec_connect(pd);
	} else if (pd->use_pdhub_c2c && pd->is_sink) {
		sprd_pd_log(pd, "use pdhub c2c typec plug in, vbus only");
		pd->typec_online = true;
		pd->is_first_negotiate = true;
		pd->vbus_only = true;
		sc27xx_pd_typec_connect_vbus_only(pd);
		goto out;
	} else if (!pd->use_pdhub_c2c && (pd->state == SC27XX_ATTACHED_SNK ||
					  pd->state == SC27XX_ATTACHED_SRC)) {
		sprd_pd_log(pd, "typec plug in");
		pd->typec_online = true;
		pd->is_first_negotiate = true;
	} else {
detach:
		sprd_pd_log(pd, "typec plug out");
		pd->typec_online = false;
		pd->is_first_negotiate = false;
		sc27xx_pd_typec_disconnect(pd);
		sc27xx_pd_goodcrc_ver_sel(pd, true);
		pd->can_communication = false;
		pd->in_hard_reset = false;
		pd->need_rx_flush = false;
		pd->update_spec_rev = false;
		if (pd->ignore_hard_reset) {
			sprd_pd_log(pd, "rx error handle clear");
			pd->ignore_hard_reset = false;
			pd->rx_error_cnt = 0;
			pd->hard_reset_cnt = 0;
			ret = regmap_update_bits(pd->regmap, pd->base + SC27XX_INT_EN,
						 rx_error_mask, rx_error_mask);
			if (ret < 0)
				dev_err(pd->dev, "failed to set rx error irq en, ret = %d\n", ret);
		}
		if (pd->vbus_only) {
			sprd_pd_log(pd, "vbus only, typec plug out");
			pd->vbus_only = false;
			goto out;
		}
		cancel_delayed_work_sync(&pd->rx_state_monitor_work);
	}

	sc27xx_pd_update_header(pd);
	sc27xx_pd_reset(pd, true);
	sc27xx_cc_polarity_status(pd, val);
	if (!pd->use_pdhub_c2c)
		sc27xx_cc_status(pd, val);
	else
		sc27xx_cc_status_use_pdhub_c2c(pd, val);
	sc27xx_get_vbus_status(pd);

out:
	return 0;
}

#define SC27XX_EXTCON_SINK		3
#define SC27XX_EXTCON_SOURCE		4
#define SC27XX_EXTCON_RP_VALUE_CHANGE	13

static int sc27xx_pd_extcon_event(struct notifier_block *nb,
				  unsigned long event, void *param)
{
	struct sc27xx_pd *pd = container_of(nb, struct sc27xx_pd, extcon_nb);
	int sink_state, source_state, rp_value_change_state;

	dev_info(pd->dev, "typec in or out, pd attached = %d\n", pd->pd_attached);
	if (pd->use_pdhub_c2c) {
		if (!pd->pd_attached)
			return NOTIFY_OK;

		source_state = extcon_get_state(pd->extcon, SC27XX_EXTCON_SOURCE);
		if (source_state < 0)
			return NOTIFY_OK;
		sink_state = extcon_get_state(pd->extcon, SC27XX_EXTCON_SINK);
		if (sink_state < 0)
			return NOTIFY_OK;

		rp_value_change_state = extcon_get_state(pd->extcon, SC27XX_EXTCON_RP_VALUE_CHANGE);
		dev_info(pd->dev, "rp_value_change_state %d\n", rp_value_change_state);
		if ((pd->is_sink == sink_state) && (pd->is_source == source_state)) {
			if (pd->is_sink == true && rp_value_change_state > 0)
				schedule_work(&pd->rp_work);
			return NOTIFY_OK;
		}

		if (sink_state)
			pd->sink_connect = true;
		else if (!sink_state)
			pd->sink_connect = false;

		if (source_state)
			pd->source_connect = true;
		else if (!source_state)
			pd->source_connect = false;

		pd->is_sink = sink_state;
		pd->is_source = source_state;

		/* if power role swap, ignore some event */
		if (pd->role_swap) {
			sprd_pd_log(pd, "sc27xx pd power role swap true");
			dev_info(pd->dev, "sc27xx pd power role swap true\n");
			if (extcon_get_state(pd->extcon, SC27XX_EXTCON_SINK) == true ||
			    extcon_get_state(pd->extcon, SC27XX_EXTCON_SOURCE) == true) {
				dev_info(pd->dev, "sc27xx pd power role swap true attach\n");
			}
		} else {
			sc27xx_pd_extcon_notify_dp(pd);
			sprd_pd_log(pd, "sc27xx pd power role swap false");
			dev_info(pd->dev, "sc27xx pd power role swap false\n");
			schedule_work(&pd->pd_work);
		}
	} else {
		sc27xx_pd_extcon_notify_dp(pd);
		if (pd->pd_attached)
			schedule_work(&pd->pd_work);
	}
	return NOTIFY_OK;
}

// ADD: Audio_Typec_Analog
static int sc27xx_pd_audio_notifier(struct notifier_block *nb,
						unsigned long event, void *param)
{
	struct sc27xx_pd *pd = container_of(nb, struct sc27xx_pd, extcon_audio_nb);
	dev_info(pd->dev, "%s:event=%lu\n", __func__, event);

	pd->audio_event = event;
	schedule_work(&pd->pd_audio_work);
	return NOTIFY_OK;
}

static void sc27xx_pd_audio_work(struct work_struct *work)
{
	struct sc27xx_pd *pd = container_of(work, struct sc27xx_pd, pd_audio_work);
	struct typec_partner_desc desc = {0, TYPEC_ACCESSORY_AUDIO, NULL, 0x0300};
	u64 curr_time;

	curr_time = ktime_to_ms(ktime_get_boottime());

	sprd_pd_log(pd, "%s:pd audio, ktime = %lld ms", __func__, curr_time);
	dev_info(pd->dev, "%s:pd audio, ktime = %lld ms\n", __func__, curr_time);

	dev_info(pd->dev, "%s:audio_event=%lu\n", __func__, pd->audio_event);

	if (pd->audio_event) {
		pd->sprd_tcpm_port->partner =
			typec_register_partner(pd->sprd_tcpm_port->typec_port, &desc);
		if (!pd->sprd_tcpm_port->partner) {
			dev_err(pd->dev, "%s:failed to register audio typec partner\n", __func__);
			return;
		}
		dev_info(pd->dev, "%s:connect, register audio typec partner\n", __func__);
	} else {
		if (pd->sprd_tcpm_port->partner)
			typec_unregister_partner(pd->sprd_tcpm_port->partner);
		pd->sprd_tcpm_port->partner = NULL;
		dev_info(pd->dev, "%s:disconnect, unregister audio typec partner\n", __func__);
	}
	pd->audio_event = 0;
}
// END: Audio_Typec_Analog

static void sc27xx_pd_work(struct work_struct *work)
{
	struct sc27xx_pd *pd = container_of(work, struct sc27xx_pd, pd_work);
	int ret;
	u64 curr_time;

	curr_time = ktime_to_ms(ktime_get_boottime());

	sprd_pd_log(pd, "check vbus and cc, ktime = %lld ms", curr_time);
	dev_info(pd->dev, "check vbus and cc, ktime = %lld ms\n", curr_time);
	ret = sc27xx_pd_check_vbus_cc_status(pd);
	if (ret)
		dev_err(pd->dev, "failed to check vbus and cc status\n");
}

static void sc27xx_rp_work(struct work_struct *work)
{
	struct sc27xx_pd *pd = container_of(work, struct sc27xx_pd, rp_work);
	int ret;
	u64 curr_time;
	u32 val = 0;

	curr_time = ktime_to_ms(ktime_get_boottime());

	sprd_pd_log(pd, "rp, ktime = %lld ms", curr_time);
	dev_info(pd->dev, "rp, ktime = %lld ms\n", curr_time);

	ret = regmap_read(pd->regmap, pd->typec_base + SC27XX_TYPEC_STATUS,
			  &val);
	if (ret)
		return;

	sc27xx_cc_polarity_status(pd, val);
	if (!pd->use_pdhub_c2c)
		sc27xx_cc_status(pd, val);
	else
		sc27xx_cc_status_use_pdhub_c2c(pd, val);
}

static int sc27xx_pd_extcon_vbus_change_event(struct notifier_block *nb,
					      unsigned long event,
					      void *param)
{
	struct sc27xx_pd *pd = container_of(nb, struct sc27xx_pd, vbus_extcon_nb);
	bool vbus_online;

	vbus_online = !!event;
	dev_err(pd->dev, "%s, vbus_online: %d, can_communication: %d, Power Role: %s, in_hard_reset: %d\n",
		__func__, vbus_online, pd->can_communication,
		pd->role == TYPEC_SINK ? "Sink" : "Source", pd->in_hard_reset);

	if (pd->role == TYPEC_SINK && pd->in_hard_reset) {
		sprd_pd_log(pd, "%s, vbus_present: %d --> %d",
			    __func__, pd->vbus_present, vbus_online);
		if (vbus_online != pd->vbus_present) {
			pd->vbus_present = vbus_online;
			sprd_tcpm_vbus_change(pd->sprd_tcpm_port);
		}
	}

	return NOTIFY_OK;
}

#define DP_PIN_ASSIGN_GEN2_BR	(BIT(DP_PIN_ASSIGN_A) | \
					 BIT(DP_PIN_ASSIGN_B))

/* Pin assignments that use DP v1.3 signaling to carry DP protocol */
#define DP_PIN_ASSIGN_DP_BR		(BIT(DP_PIN_ASSIGN_C) | \
					 BIT(DP_PIN_ASSIGN_D) | \
					 BIT(DP_PIN_ASSIGN_E) | \
					 BIT(DP_PIN_ASSIGN_F))
#define DP_CAP_PIN_DFP_ASSIGN(_cap_)	((((_cap_) & GENMASK(7, 0)) << 16) | \
					 (((_cap_) & GENMASK(7, 0)) << 8))

static const struct typec_altmode_desc sc27xx_alt_modes = {
	.svid = USB_TYPEC_DP_SID,
	.mode = USB_TYPEC_DP_MODE,
	.vdo = DP_CAP_CAPABILITY(DP_CAP_DFP_D) | DP_CAP_DP_SIGNALING | \
		DP_CAP_RECEPTACLE | \
		DP_CAP_PIN_DFP_ASSIGN(DP_PIN_ASSIGN_DP_BR),
};

static const struct tcpc_config sc27xx_pd_config = {
	.type = TYPEC_PORT_DRP,
	.default_role = TYPEC_SOURCE,
	.alt_modes = &sc27xx_alt_modes,
	.nr_alt_modes = 1,
};

static void sc27xx_init_tcpc_dev(struct sc27xx_pd *pd)
{
	pd->tcpc.config = &pd->config;
	pd->tcpc.init = sc27xx_pd_init;
	pd->tcpc.get_vbus = sc27xx_pd_get_vbus;
	pd->tcpc.get_current_limit = sc27xx_pd_get_current_limit;
	pd->tcpc.set_cc = sc27xx_pd_set_cc;
	pd->tcpc.get_cc = sc27xx_pd_get_cc;
	pd->tcpc.force_swich_rp_rd = sc27xx_pd_force_switch_rp_rd;
	pd->tcpc.set_typec_role = sc27xx_pd_set_typec_roles;
	pd->tcpc.set_polarity = sc27xx_pd_set_polarity;
	pd->tcpc.set_vconn = sc27xx_pd_set_vconn;
	pd->tcpc.set_vbus = sc27xx_pd_set_vbus;
	pd->tcpc.set_current_limit = sc27xx_pd_set_current_limit;
	pd->tcpc.set_pd_rx = sc27xx_pd_set_rx;
	pd->tcpc.set_roles = sc27xx_pd_set_roles;
	pd->tcpc.start_toggling = sc27xx_pd_start_drp_toggling;
	pd->tcpc.pd_transmit = sc27xx_pd_transmit;
	pd->tcpc.dp_altmode_notify = sc27xx_pd_dp_altmode_notify;
	pd->tcpc.reset_pd_rx_id = sc27xx_pd_reset_rx_id;
	pd->tcpc.set_pd_tx_id = sc27xx_set_pd_tx_id;
	pd->tcpc.check_tx_goodcrc = sc27xx_pd_check_tx_goodcrc;
	pd->tcpc.enable_tx_auto_retry = sc27xx_pd_enable_tx_auto_retry;
}

static int sc27xx_pd_efuse_read(struct sc27xx_pd *pd,
				const char *cell_id, u32 *val)
{
	struct nvmem_cell *cell;
	void *buf;
	size_t len = 0;

	cell = nvmem_cell_get(pd->dev, cell_id);
	if (IS_ERR_OR_NULL(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(val, buf, min(len, sizeof(u16)));

	kfree(buf);
	return 0;
}

static int sc27xx_pd_cal(struct sc27xx_pd *pd)
{
	int ret;
	u32 typec_cc_mark = 0x5a5aa5a5;

	pd->comp_code = 0;
	ret = sc27xx_pd_efuse_read(pd, "pdrc_calib", &pd->rc_cal);
	if (ret)
		return ret;

	ret = sc27xx_pd_efuse_read(pd, "pddelta_calib", &pd->delta_cal);
	if (ret)
		return ret;

	ret = sc27xx_pd_efuse_read(pd, "pdref_calib", &pd->ref_cal);
	if (ret)
		return ret;

	if (pd->var_data->id == PMIC_UMP9620) {
		if (sc27xx_pd_efuse_read(pd, "typec_cc_mark", &typec_cc_mark)) {
			dev_err(pd->dev, "%s, failed to read typec_cc_mark efuse\n", __func__);
			return 0;
		}

		if (!typec_cc_mark)
			pd->comp_code = 44;

		dev_info(pd->dev, "%s, typec_cc_mark: 0x%x, comp_code: %d\n",
			 __func__, typec_cc_mark, pd->comp_code);
	}

	return 0;
}

static void sc27xx_pd_read_msg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sc27xx_pd *pd = container_of(dwork, struct sc27xx_pd,
					   read_msg_work);
	struct sprd_pd_message pd_msg;

	mutex_lock(&pd->lock);

	if (!pd->need_retry)
		goto out;

	pd->need_retry = false;

	sprd_pd_log(pd, "read msg again");
	sc27xx_pd_read_message(pd, &pd_msg);

out:
	mutex_unlock(&pd->lock);
}

static void sc27xx_pd_rx_state_monitor_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sc27xx_pd *pd = container_of(dwork, struct sc27xx_pd, rx_state_monitor_work);
	int work_cycle = SC27XX_RX_STATE_MONITOR_DELAY;

	if (!pd->typec_online)
		return;

	if (!pd->can_communication) {
		work_cycle = SC27XX_RX_STATE_MONITOR_DELAY * 4;
		goto out;
	}

	sc27xx_pd_check_rx_state(pd);
out:
	queue_delayed_work(system_unbound_wq, &pd->rx_state_monitor_work,
			   msecs_to_jiffies(work_cycle));
}

static void sc27xx_pd_detect_typec_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sc27xx_pd *pd = container_of(dwork, struct sc27xx_pd,
					   typec_detect_work);

	dev_info(pd->dev, "pd try to detect typec extcon\n");
	pd->pd_attached = true;
	if (pd->use_pdhub_c2c) {
		if (extcon_get_state(pd->extcon, SC27XX_EXTCON_SINK) == true) {
			pd->is_sink = true;
			sc27xx_pd_check_vbus_cc_status(pd);
		} else if (extcon_get_state(pd->extcon, SC27XX_EXTCON_SOURCE) == true) {
			pd->is_source = true;
			sc27xx_pd_check_vbus_cc_status(pd);
		}
	} else {
		if (extcon_get_state(pd->extcon, EXTCON_USB) ||
		    extcon_get_state(pd->extcon, EXTCON_USB_HOST))
			sc27xx_pd_check_vbus_cc_status(pd);
	}

#if IS_ENABLED(CONFIG_SPRD_TYPEC_DP_ALTMODE)
	extcon_set_property_capability(pd->edev, EXTCON_DISP_DP, EXTCON_PROP_DISP_HPD);
#endif
}

static int sc27xx_pd_extcon_init(struct sc27xx_pd *pd)
{
	int ret = 0;

	if (!of_property_read_bool(pd->dev->of_node, "extcon")) {
		dev_err(pd->dev, "%s, the extcon device is not defined\n", __func__);
		return 0;
	}

	pd->extcon = extcon_get_edev_by_phandle(pd->dev, 0);
	if (IS_ERR(pd->extcon)) {
		dev_err(pd->dev, "%s, failed to find typec extcon device\n", __func__);
		return PTR_ERR(pd->extcon);
	}

	pd->extcon_nb.notifier_call = sc27xx_pd_extcon_event;
	ret = devm_extcon_register_notifier_all(pd->dev, pd->extcon, &pd->extcon_nb);
	if (ret) {
		dev_err(pd->dev, "%s, failed to register typec extcon\n", __func__);
		return ret;
	}

	// ADD: Audio_Typec_Analog
	pd->extcon_audio_nb.notifier_call = sc27xx_pd_audio_notifier;
	ret = devm_extcon_register_notifier(pd->dev, pd->extcon,
					EXTCON_JACK_HEADPHONE,
					&pd->extcon_audio_nb);
	if (ret) {
		dev_err(pd->dev, "%s, failed to register typec audio extcon\n", __func__);
		return ret;
	}
	// END: Audio_Typec_Analog

	pd->vbus_extcon = extcon_get_edev_by_phandle(pd->dev, 1);
	if (IS_ERR(pd->vbus_extcon)) {
		dev_err(pd->dev, "%s, failed to find vbus extcon device\n", __func__);
		return 0;
	}

	pd->vbus_extcon_nb.notifier_call = sc27xx_pd_extcon_vbus_change_event;
	ret = devm_extcon_register_notifier_all(pd->dev, pd->vbus_extcon, &pd->vbus_extcon_nb);
	if (ret)
		dev_err(pd->dev, "%s, failed to register vbus extcon\n", __func__);

	return 0;
}

static const u32 sc27xx_pd_hardreset[] = {
#if IS_ENABLED(CONFIG_SPRD_TYPEC_DP_ALTMODE)
	EXTCON_DISP_DP,
#endif
	EXTCON_CHG_USB_PD,
	EXTCON_NONE,
};

static int sc27xx_pd_probe(struct platform_device *pdev)
{
	struct sc27xx_pd *pd;
	const struct sc27xx_pd_variant_data *pdata;
	int pd_irq, ret;

	pdata = of_device_get_match_data(&pdev->dev);
	if (!pdata) {
		dev_err(&pdev->dev, "No matching driver data found\n");
		return -EINVAL;
	}

	pd = devm_kzalloc(&pdev->dev, sizeof(*pd), GFP_KERNEL);
	if (!pd) {
		dev_err(&pdev->dev, "failed to alloc memory\n");
		return -ENOMEM;
	}

	pd->dev = &pdev->dev;
	sprd_pd_tcpm_log = sprd_tcpm_log_do_outside;
	pd->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!pd->regmap) {
		dev_err(&pdev->dev, "failed to get pd regmap\n");
		return -ENODEV;
	}

	pd->use_pdhub_c2c = device_property_read_bool(&pdev->dev, "use-pdhub-c2c");

	pd->edev = devm_extcon_dev_allocate(&pdev->dev, sc27xx_pd_hardreset);
	if (IS_ERR(pd->edev)) {
		dev_err(&pdev->dev, "failed to allocate extcon device\n");
		return PTR_ERR(pd->edev);
	}

	ret = devm_extcon_dev_register(&pdev->dev, pd->edev);
	if (ret < 0) {
		dev_err(&pdev->dev, "can't register extcon device: %d\n", ret);
		return ret;
	}

	ret = sc27xx_pd_extcon_init(pd);
	if (ret) {
		dev_err(&pdev->dev, "%s, failed to init extcon\n", __func__);
		return ret;
	}

	ret = of_property_read_u32_index(pdev->dev.of_node, "reg", 0,
					&pd->base);
	if (ret) {
		dev_err(&pdev->dev, "failed to get pd reg address\n");
		return ret;
	}

	ret = of_property_read_u32_index(pdev->dev.of_node, "reg", 1,
					&pd->typec_base);
	if (ret) {
		dev_err(&pdev->dev, "failed to get typec reg address\n");
		return ret;
	}

	pd_irq = platform_get_irq(pdev, 0);
	if (pd_irq < 0) {
		dev_err(&pdev->dev, "failed to get pd irq number\n");
		return pd_irq;
	}

	mutex_init(&pd->lock);
	pd->var_data = pdata;
	pd->vbus_present = false;
	pd->constructed = false;
	pd->config = sc27xx_pd_config;
	pd->tcpc.config = &pd->config;
	pd->tcpc.fwnode = device_get_named_child_node(&pdev->dev, "connector");

	ret = sc27xx_pd_cal(pd);
	if (ret) {
		dev_err(&pdev->dev, "failed to calibrate with efuse data\n");
		return ret;
	}
	sc27xx_init_tcpc_dev(pd);

	pd->vbus = devm_regulator_get(pd->dev, "vbus");
	if (IS_ERR(pd->vbus)) {
		dev_err(&pdev->dev, "pd failed to get vbus\n");
		return PTR_ERR(pd->vbus);
	}

	pd->vconn = devm_regulator_get_optional(pd->dev, "vconn");
	if (IS_ERR(pd->vconn)) {
		ret = PTR_ERR(pd->vconn);
		if (ret == -ENODEV) {
			dev_warn(pd->dev, "unable to get vddldo supply\n");
			pd->vconn = NULL;
		} else {
			dev_err(pd->dev, "failed to get vddldo supply\n");
			return ret;
		}
	}

	if (of_property_read_bool(pdev->dev.of_node, "sprd,syscon-aon-apb")) {
		pd->aon_apb = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
							      "sprd,syscon-aon-apb");
		if (IS_ERR(pd->aon_apb)) {
			dev_err(pd->dev, "failed to map aon registers (via syscon)\n");
			return PTR_ERR(pd->aon_apb);
		}
	} else {
		pd->aon_apb = NULL;
	}

	ret = devm_request_threaded_irq(pd->dev, pd_irq, NULL,
					sc27xx_pd_irq,
					IRQF_ONESHOT | IRQF_TRIGGER_LOW,
					"sc27xx_pd", pd);
	if (ret < 0) {
		dev_err(pd->dev, "failed to request irq\n");
		return ret;
	}

	pd->pd_wq = create_singlethread_workqueue("sprd_pd_driver");
	if (!pd->pd_wq) {
		dev_err(pd->dev, "failed to create singlethread workqueue\n");
		return -ENOMEM;
	}

	pd->sprd_tcpm_port = sprd_tcpm_register_port(pd->dev, &pd->tcpc);
	if (IS_ERR(pd->sprd_tcpm_port)) {
		dev_err(pd->dev, "failed to register tcpm port\n");
		destroy_workqueue(pd->pd_wq);
		return PTR_ERR(pd->sprd_tcpm_port);
	}

	pd->sprd_tcpm_port->driver_data = (void *)pd;
	pd->sprd_tcpm_port->get_vbus_ok = sc27xx_get_vbus_ok_status;
	pd->sprd_tcpm_port->can_power_data_role_swap = pd->use_pdhub_c2c;
	dev_info(&pdev->dev, "use_pdhub_c2c = %d\n", pd->use_pdhub_c2c);

	sprd_tcpm_set_support_accessory_mode(pd->sprd_tcpm_port);

	INIT_DELAYED_WORK(&pd->typec_detect_work, sc27xx_pd_detect_typec_work);
	INIT_DELAYED_WORK(&pd->read_msg_work, sc27xx_pd_read_msg_work);
	INIT_DELAYED_WORK(&pd->rx_state_monitor_work, sc27xx_pd_rx_state_monitor_work);
	INIT_WORK(&pd->pd_work, sc27xx_pd_work);
	INIT_WORK(&pd->rp_work, sc27xx_rp_work);
	INIT_WORK(&pd->pd_audio_work, sc27xx_pd_audio_work);

	platform_set_drvdata(pdev, pd);
	schedule_delayed_work(&pd->typec_detect_work,
			      msecs_to_jiffies(SC27xx_DETECT_TYPEC_DELAY));

	return 0;
}

static int sc27xx_pd_remove(struct platform_device *pdev)
{
	struct sc27xx_pd *pd = platform_get_drvdata(pdev);

	sprd_tcpm_unregister_port(pd->sprd_tcpm_port);
	return 0;
}

static void sc27xx_pd_shutdown(struct platform_device *pdev)
{
	int ret;

	struct sc27xx_pd *pd = platform_get_drvdata(pdev);

	if (!pd->sprd_tcpm_port) {
		dev_warn(pd->dev, "sprd_tcpm_port Null!!!\n");
		return;
	}

	sprd_tcpm_shutdown(pd->sprd_tcpm_port);

	ret = sc27xx_pd_set_rx(&pd->tcpc, false);
	if (ret) {
		dev_err(pd->dev, "failed to disable set_rx at shutdown, ret = %d\n", ret);
		return;
	}

	pd->shutdown_flag = true;

	cancel_delayed_work_sync(&pd->rx_state_monitor_work);
	cancel_delayed_work_sync(&pd->read_msg_work);
	cancel_work_sync(&pd->pd_work);
	cancel_work_sync(&pd->pd_audio_work);
}

#ifdef CONFIG_PM_SLEEP
static int sc27xx_pd_suspend(struct device *dev)
{
	struct sc27xx_pd *pd = dev_get_drvdata(dev);
	int ret;

	pd->suspend = true;

	if (pd->typec_online) {
		cancel_delayed_work_sync(&pd->rx_state_monitor_work);
		return 0;
	}

	dev_info(pd->dev, "typec offline, disable pd clock when suspend\n");
	ret = sc27xx_pd_disable_clk(pd);
	if (ret) {
		dev_err(pd->dev, "failed to disable pd clock when suspend, ret = %d\n", ret);
		return ret;
	}

	ret = sc27xx_pd_set_aon_clock(pd, false);
	if (ret) {
		dev_err(pd->dev, "failed to disable aon pd clock when suspend, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int sc27xx_pd_resume(struct device *dev)
{
	struct sc27xx_pd *pd = dev_get_drvdata(dev);
	int ret;

	if (pd->typec_online)
		queue_delayed_work(system_unbound_wq, &pd->rx_state_monitor_work,
				   msecs_to_jiffies(SC27XX_RX_STATE_MONITOR_DELAY));

	ret = sc27xx_pd_set_aon_clock(pd, true);
	if (ret) {
		dev_err(pd->dev, "failed to enable aon pd clock when suspend, ret = %d\n", ret);
		return ret;
	}

	ret = sc27xx_pd_clk_cfg(pd);
	if (ret) {
		dev_err(pd->dev, "failed to enable pd clock when resume, ret = %d\n", ret);
		return ret;
	}

	pd->suspend = false;
	pd->resume_time = ktime_to_ms(ktime_get_boottime());

	return 0;
}
#endif

static const struct dev_pm_ops sc27xx_pd_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sc27xx_pd_suspend, sc27xx_pd_resume)
};

static const struct of_device_id sc27xx_pd_of_match[] = {
	{.compatible = "sprd,sc2730-pd", .data = &sc2730_data},
	{.compatible = "sprd,ump9620-pd", .data = &ump9620_data},
	{}
};

static struct platform_driver sc27xx_pd_driver = {
	.probe = sc27xx_pd_probe,
	.remove = sc27xx_pd_remove,
	.shutdown = sc27xx_pd_shutdown,
	.driver = {
		.name = "sc27xx-typec-pd",
		.of_match_table = sc27xx_pd_of_match,
		.pm = &sc27xx_pd_pm_ops,
	},
};

module_platform_driver(sc27xx_pd_driver);

MODULE_AUTHOR("Freeman Liu <freeman.liu@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum SC27xx typec driver");
MODULE_LICENSE("GPL v2");
