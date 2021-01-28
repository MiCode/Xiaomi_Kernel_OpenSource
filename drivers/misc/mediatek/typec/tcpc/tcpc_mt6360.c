// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/pm_runtime.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/cpu.h>
#include <linux/version.h>
#include <linux/pm_wakeup.h>
#include <linux/of_irq.h>
#include <linux/sched/clock.h>
#include <uapi/linux/sched/types.h>

#include "inc/pd_dbg_info.h"
#include "inc/tcpci.h"
#include "inc/mt6360.h"
#include "inc/tcpci_typec.h"

#ifdef CONFIG_RT_REGMAP
#include <mt-plat/rt-regmap.h>
#endif /* CONFIG_RT_REGMAP */

#include <linux/sched/rt.h>

#if defined(CONFIG_WATER_DETECTION) || defined(CONFIG_CABLE_TYPE_DETECTION)
#if CONFIG_MTK_GAUGE_VERSION == 30
#include <mt-plat/charger_class.h>
#endif /* CONFIG_MTK_GAUGE_VERSION == 30 */
#endif /* CONFIG_WATER_DETECTION || CONFIG_CABLE_TYPE_DETECTION */

/* #define DEBUG_GPIO	66 */

#define MT6360_DRV_VERSION	"2.0.2_MTK"

#define MT6360_IRQ_WAKE_TIME	(500) /* ms */

#define MEDIATEK_6360_VID	0x29cf
#define MEDIATEK_6360_PID	0x6360
#define MEDIATEK_6360_DID_V2	0x3492
#define MEDIATEK_6360_DID_V3	0x3493


struct mt6360_chip {
	struct i2c_client *client;
	struct device *dev;
#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_device *m_dev;
#endif /* CONFIG_RT_REGMAP */
	struct semaphore io_lock;
	struct semaphore suspend_lock;
	struct tcpc_desc *tcpc_desc;
	struct tcpc_device *tcpc;
	struct kthread_worker irq_worker;
	struct kthread_work irq_work;
	struct task_struct *irq_worker_task;
	struct wakeup_source *irq_wake_lock;
	struct wakeup_source *i2c_wake_lock;

	atomic_t poll_count;
	struct delayed_work poll_work;

	int irq_gpio;
	int irq;
	int chip_id;

#ifdef CONFIG_MTK_TYPEC_WATER_DETECT_BY_PCB
	int pcb_gpio;
	int pcb_gpio_polarity;
#endif /* CONFIG_MTK_TYPEC_WATER_DETECT_BY_PCB */

#ifdef CONFIG_WATER_DETECTION
	atomic_t wd_protect_rty;
	struct wakeup_source wd_wakeup_src;
#endif /* CONFIG_WATER_DETECTION */

#ifdef CONFIG_WD_SBU_POLLING
#ifdef CONFIG_WD_POLLING_ONLY
	struct delayed_work usbid_poll_work;
#endif /* CONFIG_WD_POLLING_ONLY */
	struct work_struct wd_work;
	struct mutex usbid_irq_lock;
	bool usbid_irqen;
#endif /* CONFIG_WD_SBU_POLLING */

#ifdef CONFIG_CABLE_TYPE_DETECTION
	bool handle_init_ctd;
	enum tcpc_cable_type init_cable_type;
#endif /* CONFIG_CABLE_TYPE_DETECTION */

#if defined(CONFIG_WATER_DETECTION) || defined(CONFIG_CABLE_TYPE_DETECTION)
#if CONFIG_MTK_GAUGE_VERSION == 30
	struct charger_device *chgdev;
#endif /* CONFIG_MTK_GAUGE_VERSION == 30 */
#endif /* CONFIG_WATER_DETECTION || CONFIG_CABLE_TYPE_DETECTION */
};

static const u8 mt6360_vend_alert_clearall[MT6360_VEND_INT_MAX] = {
	0x3F, 0xDF, 0xFF, 0xFF, 0xFF,
};

static const u8 mt6360_vend_alert_maskall[MT6360_VEND_INT_MAX] = {
	0x00, 0x00, 0x00, 0x00, 0x00,
};

#ifdef CONFIG_RT_REGMAP
RT_REG_DECL(TCPC_V10_REG_VID, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_PID, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_DID, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TYPEC_REV, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_PD_REV, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_PDIF_REV, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_ALERT, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_ALERT_MASK, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_POWER_STATUS_MASK, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_FAULT_STATUS_MASK, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_EXT_STATUS_MASK, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TCPC_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_ROLE_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_FAULT_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_POWER_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_CC_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_POWER_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_FAULT_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_EXT_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_COMMAND, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_MSG_HDR_INFO, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_DETECT, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_BYTE_CNT, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_BUF_FRAME_TYPE, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_HDR, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_DATA, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TRANSMIT, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TX_BYTE_CNT, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TX_HDR, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TX_DATA, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_PHY_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_PHY_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_PHY_CTRL3, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_PHY_CTRL4, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_PHY_CTRL5, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_PHY_CTRL6, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_PHY_CTRL7, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_CLK_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_CLK_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_PHY_CTRL8, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_CC1_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_VCONN_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_MODE_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_MODE_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_MODE_CTRL3, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_MT_MASK1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_MT_MASK2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_MT_MASK3, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_MT_MASK4, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_MT_MASK5, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_MT_INT1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_MT_INT2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_MT_INT3, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_MT_INT4, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_MT_INT5, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_MT_ST1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_MT_ST2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_MT_ST3, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_MT_ST4, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_MT_ST5, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_SWRESET, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_DEBOUNCE_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_DRP_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_DRP_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_DRP_CTRL3, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_PD3_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_VBUS_DISC_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_CTD_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_WATCHDOG_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_I2CRST_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_WD_DET_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_WD_DET_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_WD_DET_CTRL3, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_WD_DET_CTRL4, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_WD_DET_CTRL5, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_WD_DET_CTRL6, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_WD_DET_CTRL7, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_WD_DET_CTRL8, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_PHY_CTRL9, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_PHY_CTRL10, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_PHY_CTRL11, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_PHY_CTRL12, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_PHY_CTRL13, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_PHY_CTRL14, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_RX_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_RX_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_VBUS_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_HILO_CTRL5, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_VCONN_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_VCONN_CTRL3, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_DEBOUNCE_CTRL4, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_REG_CTD_CTRL2, 1, RT_VOLATILE, {});

static const rt_register_map_t mt6360_chip_regmap[] = {
	RT_REG(TCPC_V10_REG_VID),
	RT_REG(TCPC_V10_REG_PID),
	RT_REG(TCPC_V10_REG_DID),
	RT_REG(TCPC_V10_REG_TYPEC_REV),
	RT_REG(TCPC_V10_REG_PD_REV),
	RT_REG(TCPC_V10_REG_PDIF_REV),
	RT_REG(TCPC_V10_REG_ALERT),
	RT_REG(TCPC_V10_REG_ALERT_MASK),
	RT_REG(TCPC_V10_REG_POWER_STATUS_MASK),
	RT_REG(TCPC_V10_REG_FAULT_STATUS_MASK),
	RT_REG(TCPC_V10_REG_EXT_STATUS_MASK),
	RT_REG(TCPC_V10_REG_TCPC_CTRL),
	RT_REG(TCPC_V10_REG_ROLE_CTRL),
	RT_REG(TCPC_V10_REG_FAULT_CTRL),
	RT_REG(TCPC_V10_REG_POWER_CTRL),
	RT_REG(TCPC_V10_REG_CC_STATUS),
	RT_REG(TCPC_V10_REG_POWER_STATUS),
	RT_REG(TCPC_V10_REG_FAULT_STATUS),
	RT_REG(TCPC_V10_REG_EXT_STATUS),
	RT_REG(TCPC_V10_REG_COMMAND),
	RT_REG(TCPC_V10_REG_MSG_HDR_INFO),
	RT_REG(TCPC_V10_REG_RX_DETECT),
	RT_REG(TCPC_V10_REG_RX_BYTE_CNT),
	RT_REG(TCPC_V10_REG_RX_BUF_FRAME_TYPE),
	RT_REG(TCPC_V10_REG_RX_HDR),
	RT_REG(TCPC_V10_REG_RX_DATA),
	RT_REG(TCPC_V10_REG_TRANSMIT),
	RT_REG(TCPC_V10_REG_TX_BYTE_CNT),
	RT_REG(TCPC_V10_REG_TX_HDR),
	RT_REG(TCPC_V10_REG_TX_DATA),
	RT_REG(MT6360_REG_PHY_CTRL1),
	RT_REG(MT6360_REG_PHY_CTRL2),
	RT_REG(MT6360_REG_PHY_CTRL3),
	RT_REG(MT6360_REG_PHY_CTRL4),
	RT_REG(MT6360_REG_PHY_CTRL5),
	RT_REG(MT6360_REG_PHY_CTRL6),
	RT_REG(MT6360_REG_PHY_CTRL7),
	RT_REG(MT6360_REG_CLK_CTRL1),
	RT_REG(MT6360_REG_CLK_CTRL2),
	RT_REG(MT6360_REG_PHY_CTRL8),
	RT_REG(MT6360_REG_CC1_CTRL1),
	RT_REG(MT6360_REG_VCONN_CTRL1),
	RT_REG(MT6360_REG_MODE_CTRL1),
	RT_REG(MT6360_REG_MODE_CTRL2),
	RT_REG(MT6360_REG_MODE_CTRL3),
	RT_REG(MT6360_REG_MT_MASK1),
	RT_REG(MT6360_REG_MT_MASK2),
	RT_REG(MT6360_REG_MT_MASK3),
	RT_REG(MT6360_REG_MT_MASK4),
	RT_REG(MT6360_REG_MT_MASK5),
	RT_REG(MT6360_REG_MT_INT1),
	RT_REG(MT6360_REG_MT_INT2),
	RT_REG(MT6360_REG_MT_INT3),
	RT_REG(MT6360_REG_MT_INT4),
	RT_REG(MT6360_REG_MT_INT5),
	RT_REG(MT6360_REG_MT_ST1),
	RT_REG(MT6360_REG_MT_ST2),
	RT_REG(MT6360_REG_MT_ST3),
	RT_REG(MT6360_REG_MT_ST4),
	RT_REG(MT6360_REG_MT_ST5),
	RT_REG(MT6360_REG_SWRESET),
	RT_REG(MT6360_REG_DEBOUNCE_CTRL1),
	RT_REG(MT6360_REG_DRP_CTRL1),
	RT_REG(MT6360_REG_DRP_CTRL2),
	RT_REG(MT6360_REG_DRP_CTRL3),
	RT_REG(MT6360_REG_PD3_CTRL),
	RT_REG(MT6360_REG_VBUS_DISC_CTRL),
	RT_REG(MT6360_REG_CTD_CTRL1),
	RT_REG(MT6360_REG_WATCHDOG_CTRL),
	RT_REG(MT6360_REG_I2CRST_CTRL),
	RT_REG(MT6360_REG_WD_DET_CTRL1),
	RT_REG(MT6360_REG_WD_DET_CTRL2),
	RT_REG(MT6360_REG_WD_DET_CTRL3),
	RT_REG(MT6360_REG_WD_DET_CTRL4),
	RT_REG(MT6360_REG_WD_DET_CTRL5),
	RT_REG(MT6360_REG_WD_DET_CTRL6),
	RT_REG(MT6360_REG_WD_DET_CTRL7),
	RT_REG(MT6360_REG_WD_DET_CTRL8),
	RT_REG(MT6360_REG_PHY_CTRL9),
	RT_REG(MT6360_REG_PHY_CTRL10),
	RT_REG(MT6360_REG_PHY_CTRL11),
	RT_REG(MT6360_REG_PHY_CTRL12),
	RT_REG(MT6360_REG_PHY_CTRL13),
	RT_REG(MT6360_REG_PHY_CTRL14),
	RT_REG(MT6360_REG_RX_CTRL1),
	RT_REG(MT6360_REG_RX_CTRL2),
	RT_REG(MT6360_REG_VBUS_CTRL2),
	RT_REG(MT6360_REG_HILO_CTRL5),
	RT_REG(MT6360_REG_VCONN_CTRL2),
	RT_REG(MT6360_REG_VCONN_CTRL3),
	RT_REG(MT6360_REG_DEBOUNCE_CTRL4),
	RT_REG(MT6360_REG_CTD_CTRL2),
};
#define MT6360_CHIP_REGMAP_SIZE ARRAY_SIZE(mt6360_chip_regmap)
#endif /* CONFIG_RT_REGMAP */

#define MT6360_I2C_RETRY_CNT	5
static int mt6360_read_device(void *client, u32 reg, int len, void *dst)
{
	struct i2c_client *i2c = (struct i2c_client *)client;
	struct mt6360_chip *chip = i2c_get_clientdata(i2c);
	int ret, count = MT6360_I2C_RETRY_CNT;

	__pm_stay_awake(chip->i2c_wake_lock);
	down(&chip->suspend_lock);
	while (count) {
		if (len > 1) {
			ret = i2c_smbus_read_i2c_block_data(i2c, reg, len, dst);
			if (ret < 0)
				count--;
			else
				goto out;
		} else {
			ret = i2c_smbus_read_byte_data(i2c, reg);
			if (ret < 0)
				count--;
			else {
				*(u8 *)dst = (u8)ret;
				goto out;
			}
		}
		udelay(100);
	}
out:
	up(&chip->suspend_lock);
	__pm_relax(chip->i2c_wake_lock);
	return ret;
}

static int mt6360_write_device(void *client, u32 reg, int len, const void *src)
{
	const u8 *data;
	struct i2c_client *i2c = (struct i2c_client *)client;
	struct mt6360_chip *chip = i2c_get_clientdata(i2c);
	int ret, count = MT6360_I2C_RETRY_CNT;

	__pm_stay_awake(chip->i2c_wake_lock);
	down(&chip->suspend_lock);
	while (count) {
		if (len > 1) {
			ret = i2c_smbus_write_i2c_block_data(i2c, reg, len,
							     src);
			if (ret < 0)
				count--;
			else
				goto out;
		} else {
			data = src;
			ret = i2c_smbus_write_byte_data(i2c, reg, *data);
			if (ret < 0)
				count--;
			else
				goto out;
		}
		udelay(100);
	}
out:
	up(&chip->suspend_lock);
	__pm_relax(chip->i2c_wake_lock);
	return ret;
}

static int mt6360_reg_read(struct i2c_client *i2c, u8 reg, u8 *data)
{
	int ret;
	struct mt6360_chip *chip = i2c_get_clientdata(i2c);

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_read(chip->m_dev, reg, 1, data);
#else
	ret = mt6360_read_device(chip->client, reg, 1, data);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0) {
		dev_err(chip->dev, "%s fail(%d)\n", __func__, ret);
		return ret;
	}
	return 0;
}

static int mt6360_reg_write(struct i2c_client *i2c, u8 reg, const u8 data)
{
	int ret;
	struct mt6360_chip *chip = i2c_get_clientdata(i2c);

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_write(chip->m_dev, reg, 1, &data);
#else
	ret = mt6360_write_device(chip->client, reg, 1, &data);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0)
		dev_err(chip->dev, "%s fail(%d)\n", __func__, ret);
	return 0;
}

static int mt6360_block_read(struct i2c_client *i2c, u8 reg, int len, void *dst)
{
	int ret;
	struct mt6360_chip *chip = i2c_get_clientdata(i2c);

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_read(chip->m_dev, reg, len, dst);
#else
	ret = mt6360_read_device(chip->client, reg, len, dst);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0)
		dev_err(chip->dev, "%s fail(%d)\n", __func__, ret);
	return 0;
}

static int mt6360_block_write(struct i2c_client *i2c, u8 reg, int len,
			      const void *src)
{
	int ret;
	struct mt6360_chip *chip = i2c_get_clientdata(i2c);

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_write(chip->m_dev, reg, len, src);
#else
	ret = mt6360_write_device(chip->client, reg, len, src);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0)
		dev_err(chip->dev, "%s fail(%d)\n", __func__, ret);
	return 0;
}

static int mt6360_write_word(struct i2c_client *client, u8 reg_addr, u16 data)
{
	data = cpu_to_le16(data);
	return mt6360_block_write(client, reg_addr, 2, (u8 *)&data);
}

static int mt6360_read_word(struct i2c_client *client, u8 reg_addr, u16 *data)
{
	int ret;

	ret = mt6360_block_read(client, reg_addr, 2, (u8 *)data);
	if (ret < 0)
		return ret;
	*data = le16_to_cpu(*data);
	return 0;
}

static inline int mt6360_i2c_write8(struct tcpc_device *tcpc, u8 reg,
				    const u8 data)
{
	int ret;
	struct mt6360_chip *chip = tcpc_get_dev_data(tcpc);

	down(&chip->io_lock);
	ret = mt6360_reg_write(chip->client, reg, data);
	up(&chip->io_lock);
	return ret;
}

static inline int mt6360_i2c_write16(struct tcpc_device *tcpc, u8 reg,
				     const u16 data)
{
	int ret;
	struct mt6360_chip *chip = tcpc_get_dev_data(tcpc);

	down(&chip->io_lock);
	ret = mt6360_write_word(chip->client, reg, data);
	up(&chip->io_lock);
	return ret;
}

static inline int mt6360_i2c_read8(struct tcpc_device *tcpc, u8 reg, u8 *data)
{
	int ret;
	struct mt6360_chip *chip = tcpc_get_dev_data(tcpc);

	down(&chip->io_lock);
	ret = mt6360_reg_read(chip->client, reg, data);
	up(&chip->io_lock);
	return ret;
}

static inline int mt6360_i2c_read16(struct tcpc_device *tcpc, u8 reg, u16 *data)
{
	int ret;
	struct mt6360_chip *chip = tcpc_get_dev_data(tcpc);

	down(&chip->io_lock);
	ret = mt6360_read_word(chip->client, reg, data);
	up(&chip->io_lock);
	return ret;
}

static inline int mt6360_i2c_block_read(struct tcpc_device *tcpc, u8 reg,
					int len, void *dst)
{
	int ret;
	struct mt6360_chip *chip = tcpc_get_dev_data(tcpc);

	down(&chip->io_lock);
	ret = mt6360_block_read(chip->client, reg, len, dst);
	up(&chip->io_lock);
	return ret;
}

static inline int mt6360_i2c_block_write(struct tcpc_device *tcpc, u8 reg,
					 int len, const void *src)
{
	int ret;
	struct mt6360_chip *chip = tcpc_get_dev_data(tcpc);

	down(&chip->io_lock);
	ret = mt6360_block_write(chip->client, reg, len, src);
	up(&chip->io_lock);
	return ret;
}

static inline int mt6360_i2c_update_bits(struct tcpc_device *tcpc, u8 reg,
					 u8 val, u8 mask)
{
	int ret;
	u8 data;
	struct mt6360_chip *chip = tcpc_get_dev_data(tcpc);

	down(&chip->io_lock);
	ret = mt6360_reg_read(chip->client, reg, &data);
	if (ret < 0) {
		up(&chip->io_lock);
		return ret;
	}

	data &= ~mask;
	data |= (val & mask);

	ret = mt6360_reg_write(chip->client, reg, data);
	up(&chip->io_lock);
	return ret;
}

static inline int mt6360_i2c_set_bit(struct tcpc_device *tcpc, u8 reg, u8 mask)
{
	return mt6360_i2c_update_bits(tcpc, reg, mask, mask);
}

static inline int mt6360_i2c_clr_bit(struct tcpc_device *tcpc, u8 reg, u8 mask)
{
	return mt6360_i2c_update_bits(tcpc, reg, 0x00, mask);
}

#ifdef CONFIG_RT_REGMAP
static struct rt_regmap_fops mt6360_regmap_fops = {
	.read_device = mt6360_read_device,
	.write_device = mt6360_write_device,
};

static int mt6360_regmap_init(struct mt6360_chip *chip)
{
	struct rt_regmap_properties *props;
	char name[32];
	int len;

	props = devm_kzalloc(chip->dev, sizeof(*props), GFP_KERNEL);
	if (!props)
		return -ENOMEM;

	props->register_num = MT6360_CHIP_REGMAP_SIZE;
	props->rm = mt6360_chip_regmap;
	props->rt_regmap_mode = RT_MULTI_BYTE | RT_CACHE_DISABLE |
				RT_IO_PASS_THROUGH | RT_DBG_GENERAL;

	snprintf(name, sizeof(name), "mt6360-%02x", chip->client->addr);
	len = strlen(name);
	props->name = kzalloc(len + 1, GFP_KERNEL);
	props->aliases = kzalloc(len + 1, GFP_KERNEL);
	if ((!props->name) || (!props->aliases))
		return -ENOMEM;
	strlcpy((char *)props->name, name, len + 1);
	strlcpy((char *)props->aliases, name, len + 1);
	props->io_log_en = 0;
	chip->m_dev = rt_regmap_device_register(props, &mt6360_regmap_fops,
						chip->dev, chip->client, chip);
	if (!chip->m_dev) {
		dev_err(chip->dev, "%s register fail\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static void mt6360_regmap_deinit(struct mt6360_chip *chip)
{
	rt_regmap_device_unregister(chip->m_dev);
}
#endif /* CONFIG_RT_REGMAP */

static inline int mt6360_software_reset(struct tcpc_device *tcpc)
{
	int ret;

	ret = mt6360_i2c_write8(tcpc, MT6360_REG_SWRESET, 1);
	if (ret < 0)
		return ret;

	usleep_range(1000, 2000);
	return 0;
}

static inline int mt6360_command(struct tcpc_device *tcpc, uint8_t cmd)
{
	return mt6360_i2c_write8(tcpc, TCPC_V10_REG_COMMAND, cmd);
}

static int mt6360_init_power_status_mask(struct tcpc_device *tcpc)
{
	const u8 mask = TCPC_V10_REG_POWER_STATUS_VBUS_PRES;

	return mt6360_i2c_write8(tcpc, TCPC_V10_REG_POWER_STATUS_MASK, mask);
}

static int mt6360_init_fault_mask(struct tcpc_device *tcpc)
{
	const u8 mask = TCPC_V10_REG_FAULT_STATUS_VCONN_OC;

	return mt6360_i2c_write8(tcpc, TCPC_V10_REG_FAULT_STATUS_MASK, mask);
}

static int mt6360_init_ext_mask(struct tcpc_device *tcpc)
{
	const u8 mask = 0x00;

	return mt6360_i2c_write8(tcpc, TCPC_V10_REG_EXT_STATUS_MASK, mask);
}

static int mt6360_init_vend_mask(struct tcpc_device *tcpc)
{
	u8 mask[MT6360_VEND_INT_MAX] = {0};

	mask[MT6360_VEND_INT1] |= MT6360_M_VCONN_SHT_GND;
	mask[MT6360_VEND_INT2] |= MT6360_M_VCONN_OV_CC1 |
				  MT6360_M_VCONN_OV_CC2 |
				  MT6360_M_VCONN_OCR |
				  MT6360_M_VCONN_INVALID;
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	mask[MT6360_VEND_INT1] |= MT6360_M_VBUS_SAFE0V;
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

#ifdef CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG
	if (tcpc->tcpc_flags & TCPC_FLAGS_LPM_WAKEUP_WATCHDOG)
		mask[MT6360_VEND_INT1] |= MT6360_M_WAKEUP;
#endif	/* CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG */

#ifdef CONFIG_WATER_DETECTION
	if (tcpc->tcpc_flags & TCPC_FLAGS_WATER_DETECTION)
		mask[MT6360_VEND_INT2] |= MT6360_M_WD_EVT;
#endif /* CONFIG_WATER_DETECTION */

#ifdef CONFIG_CABLE_TYPE_DETECTION
	if (tcpc->tcpc_flags & TCPC_FLAGS_CABLE_TYPE_DETECTION)
		mask[MT6360_VEND_INT3] |= MT6360_M_CTD;
#endif /* CONFIG_CABLE_TYPE_DETECTION */

	return mt6360_i2c_block_write(tcpc, MT6360_REG_MT_MASK1,
				      MT6360_VEND_INT_MAX, mask);
}

static int __mt6360_init_alert_mask(struct tcpc_device *tcpc)
{
	u16 mask = TCPC_V10_REG_ALERT_CC_STATUS |
		   TCPC_V10_REG_ALERT_POWER_STATUS |
		   TCPC_V10_REG_ALERT_VENDOR_DEFINED;

#ifdef CONFIG_USB_POWER_DELIVERY
	mask |= TCPC_V10_REG_ALERT_TX_SUCCESS |
		TCPC_V10_REG_ALERT_TX_DISCARDED |
		TCPC_V10_REG_ALERT_TX_FAILED |
		TCPC_V10_REG_ALERT_RX_HARD_RST |
		TCPC_V10_REG_ALERT_RX_STATUS |
		TCPC_V10_REG_RX_OVERFLOW;
#endif /* CONFIG_USB_POWER_DELIVERY */

	mask |= TCPC_REG_ALERT_FAULT;
	return mt6360_i2c_write16(tcpc, TCPC_V10_REG_ALERT_MASK, mask);
}

static int mt6360_init_alert_mask(struct tcpc_device *tcpc)
{
	struct mt6360_chip *chip = tcpc_get_dev_data(tcpc);

	dev_info(chip->dev, "%s\n", __func__);

	__mt6360_init_alert_mask(tcpc);
	mt6360_init_power_status_mask(tcpc);
	mt6360_init_fault_mask(tcpc);
	mt6360_init_ext_mask(tcpc);
	mt6360_init_vend_mask(tcpc);

#ifdef CONFIG_CABLE_TYPE_DETECTION
	if (chip->handle_init_ctd) {
		chip->handle_init_ctd = false;
		tcpc_typec_handle_ctd(tcpc, chip->init_cable_type);
	}
#endif /* CONFIG_CABLE_TYPE_DETECTION */

	return 0;
}

static inline void mt6360_poll_ctrl(struct mt6360_chip *chip)
{
	cancel_delayed_work_sync(&chip->poll_work);

	if (atomic_read(&chip->poll_count) == 0) {
		atomic_inc(&chip->poll_count);
		cpu_idle_poll_ctrl(true);
	}

	schedule_delayed_work(&chip->poll_work, msecs_to_jiffies(40));
}

static void mt6360_irq_work_handler(struct kthread_work *work)
{
	int regval, gpio_val;
	struct mt6360_chip *chip = container_of(work, struct mt6360_chip,
						irq_work);

	mt6360_poll_ctrl(chip);
	tcpci_lock_typec(chip->tcpc);

#ifdef DEBUG_GPIO
	gpio_set_value(DEBUG_GPIO, 1);
#endif /* DEBUG_GPIO */

	do {
		regval = tcpci_alert(chip->tcpc);
		if (regval)
			break;
		gpio_val = gpio_get_value(chip->irq_gpio);
	} while (gpio_val == 0);

	tcpci_unlock_typec(chip->tcpc);

#ifdef DEBUG_GPIO
	gpio_set_value(DEBUG_GPIO, 1);
#endif /* DEBUG_GPIO */
}

static void mt6360_poll_work(struct work_struct *work)
{
	struct mt6360_chip *chip = container_of(work, struct mt6360_chip,
						poll_work.work);

	if (atomic_dec_and_test(&chip->poll_count))
		cpu_idle_poll_ctrl(false);
}

static irqreturn_t mt6360_intr_handler(int irq, void *data)
{
	struct mt6360_chip *chip = data;

	__pm_wakeup_event(chip->irq_wake_lock, MT6360_IRQ_WAKE_TIME);

#ifdef DEBUG_GPIO
	gpio_set_value(DEBUG_GPIO, 0);
#endif

	kthread_queue_work(&chip->irq_worker, &chip->irq_work);
	return IRQ_HANDLED;
}

static int mt6360_mask_clear_alert(struct tcpc_device *tcpc)
{
	/* Mask all alerts & clear them */
	mt6360_i2c_block_write(tcpc, MT6360_REG_MT_MASK1, MT6360_VEND_INT_MAX,
			       mt6360_vend_alert_maskall);
	mt6360_i2c_block_write(tcpc, MT6360_REG_MT_INT1, MT6360_VEND_INT_MAX,
			       mt6360_vend_alert_clearall);
	mt6360_i2c_write16(tcpc, TCPC_V10_REG_ALERT_MASK, 0);
	mt6360_i2c_write16(tcpc, TCPC_V10_REG_ALERT, 0xffff);
	return 0;
}

static inline int mt6360_enable_oneshot_rpconnect(struct tcpc_device *tcpc,
						  bool en)
{
	return (en ? mt6360_i2c_set_bit : mt6360_i2c_clr_bit)
		(tcpc, MT6360_REG_CTD_CTRL2, MT6360_RPDET_ONESHOT);
}

static inline int mt6360_enable_auto_rpconnect(struct tcpc_device *tcpc,
					       bool en)
{
	return (en ? mt6360_i2c_clr_bit : mt6360_i2c_set_bit)
		(tcpc, MT6360_REG_CTD_CTRL2, MT6360_DIS_RPDET);
}

#ifdef CONFIG_WD_SBU_POLLING
static int mt6360_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2);
static void mt6360_enable_usbid_irq(struct mt6360_chip *chip, bool en);

static int mt6360_enable_usbid_polling(struct mt6360_chip *chip, bool en)
{
	int ret;

	if (!(chip->tcpc->tcpc_flags & TCPC_FLAGS_WATER_DETECTION))
		return 0;

	if (en) {
		ret = charger_dev_set_usbid_src_ton(chip->chgdev, 100000);
		if (ret < 0) {
			dev_err(chip->dev, "%s usbid src on 100ms fail\n",
					__func__);
			return ret;
		}

		ret = charger_dev_set_usbid_rup(chip->chgdev, 75000);
		if (ret < 0) {
			dev_err(chip->dev, "%s usbid rup75k fail\n", __func__);
			return ret;
		}
	}

	ret = charger_dev_enable_usbid(chip->chgdev, en);
	if (ret < 0)
		return ret;
	mt6360_enable_usbid_irq(chip, en);
	return 0;
}

static void mt6360_wd_work(struct work_struct *work)
{
	int ret, cc1, cc2;
	struct mt6360_chip *chip = container_of(work, struct mt6360_chip,
						wd_work);

	tcpci_lock_typec(chip->tcpc);

	ret = mt6360_get_cc(chip->tcpc, &cc1, &cc2);
	if (ret < 0)
		goto out;

	/* Only handle usbid event during toggling */
	if (cc1 != TYPEC_CC_DRP_TOGGLING || cc2 != TYPEC_CC_DRP_TOGGLING)
		goto out;

	ret = tcpci_is_water_detected(chip->tcpc);
	if (ret <= 0) {
		mt6360_enable_usbid_polling(chip, true);
		goto out;
	}
	tcpc_typec_handle_wd(chip->tcpc, true);
out:
	tcpci_unlock_typec(chip->tcpc);
}

static irqreturn_t mt6360_pmu_usbid_evt_handler(int irq, void *data)
{
	u32 work_flag;
	struct mt6360_chip *chip = data;

	MT6360_INFO("%s\n", __func__);

	work_flag = work_busy(&chip->wd_work);
	if (work_flag & WORK_BUSY_PENDING || work_flag & WORK_BUSY_RUNNING)
		return IRQ_HANDLED;

	cancel_work_sync(&chip->wd_work);
	schedule_work(&chip->wd_work);

	return IRQ_HANDLED;
}
#endif /* CONFIG_WD_SBU_POLLING */

struct mt6360_pmu_irq_desc {
	const char *name;
	irq_handler_t irq_handler;
	int irq;
};

#define MT6360_PMU_IRQDESC(name) {#name, mt6360_pmu_##name##_handler, -1}

static struct mt6360_pmu_irq_desc mt6360_pmu_tcpc_irq_desc[] = {
#ifdef CONFIG_WD_SBU_POLLING
	MT6360_PMU_IRQDESC(usbid_evt),
#endif /* CONFIG_WD_SBU_POLLING */
};

#ifdef CONFIG_WD_SBU_POLLING
static void mt6360_enable_usbid_irq(struct mt6360_chip *chip, bool en)
{
	struct mt6360_pmu_irq_desc *irq_desc;
	int i;
	const char *name = "usbid_evt";

	mutex_lock(&chip->usbid_irq_lock);
	for (i = 0; i < ARRAY_SIZE(mt6360_pmu_tcpc_irq_desc); i++) {
		irq_desc = &mt6360_pmu_tcpc_irq_desc[i];
		if (!strcmp(irq_desc->name, name)) {
			if (en && !chip->usbid_irqen) {
				enable_irq(irq_desc->irq);
				chip->usbid_irqen = true;
			} else if (!en && chip->usbid_irqen) {
				disable_irq(irq_desc->irq);
				chip->usbid_irqen = false;
			}
			break;
		}
	}
	mutex_unlock(&chip->usbid_irq_lock);
}
#endif /* CONFIG_WD_SBU_POLLING */

static struct resource *mt6360_tcpc_get_irq_byname(struct device *dev,
						   unsigned int type,
						   const char *name)
{
	int i;
	struct mt6360_tcpc_platform_data *pdata = dev_get_platdata(dev);

	for (i = 0; i < pdata->irq_res_cnt; i++) {
		struct resource *r = pdata->irq_res + i;

		if (unlikely(!r->name))
			continue;

		if (type == resource_type(r) && !strcmp(r->name, name))
			return r;
	}
	return NULL;
}

static int mt6360_pmu_tcpc_irq_register(struct tcpc_device *tcpc)
{
	struct mt6360_chip *chip = tcpc_get_dev_data(tcpc);
	struct mt6360_pmu_irq_desc *irq_desc;
	struct resource *r;
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(mt6360_pmu_tcpc_irq_desc); i++) {
		irq_desc = mt6360_pmu_tcpc_irq_desc + i;
		if (unlikely(!irq_desc->name))
			continue;
		if (!(tcpc->tcpc_flags & TCPC_FLAGS_WATER_DETECTION) &&
		    strcmp(irq_desc->name, "usbid_evt") == 0)
			continue;
		r = mt6360_tcpc_get_irq_byname(chip->dev, IORESOURCE_IRQ,
					       irq_desc->name);
		if (!r)
			continue;
		irq_desc->irq = r->start;
		ret = devm_request_threaded_irq(chip->dev, irq_desc->irq, NULL,
						irq_desc->irq_handler,
						IRQF_TRIGGER_FALLING,
						irq_desc->name,
						chip);
		if (ret < 0)
			dev_err(chip->dev, "%s request %s irq fail\n", __func__,
				irq_desc->name);
	}
	return ret;
}

static int mt6360_init_alert(struct tcpc_device *tcpc)
{
	struct mt6360_chip *chip = tcpc_get_dev_data(tcpc);
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
	int ret, len;
	char *name;

	mt6360_mask_clear_alert(tcpc);

	ret = mt6360_pmu_tcpc_irq_register(tcpc);
	if (ret < 0)
		return ret;

	len = strlen(chip->tcpc_desc->name);
	name = devm_kzalloc(chip->dev, len + 5, GFP_KERNEL);
	if (!name)
		return -ENOMEM;
	snprintf(name, PAGE_SIZE, "%s-IRQ", chip->tcpc_desc->name);
	dev_info(chip->dev, "%s name = %s, gpio = %d\n", __func__,
		 chip->tcpc_desc->name, chip->irq_gpio);
	ret = devm_gpio_request(chip->dev, chip->irq_gpio, name);
#ifdef DEBUG_GPIO
	gpio_request(DEBUG_GPIO, "debug_latency_pin");
	gpio_direction_output(DEBUG_GPIO, 1);
#endif /* DEBUG_GPIO */
	if (ret < 0) {
		dev_err(chip->dev, "%s fail to request GPIO%d(%d)\n", __func__,
			chip->irq_gpio, ret);
		goto init_alert_err;
	}

	ret = gpio_direction_input(chip->irq_gpio);
	if (ret < 0) {
		dev_err(chip->dev, "%s fail to set GPIO%d as input pin(%d)\n",
			__func__, chip->irq_gpio, ret);
		goto init_alert_err;
	}

	chip->irq = gpio_to_irq(chip->irq_gpio);
	if (chip->irq <= 0) {
		dev_err(chip->dev, "%s gpio to irq fail, chip->irq(%d)\n",
			__func__, chip->irq);
		goto init_alert_err;
	}
	dev_info(chip->dev, "%s IRQ number = %d\n", __func__, chip->irq);

	kthread_init_worker(&chip->irq_worker);

	chip->irq_worker_task = kthread_run(kthread_worker_fn,
					    &chip->irq_worker,
					    chip->tcpc_desc->name);
	if (IS_ERR(chip->irq_worker_task)) {
		dev_err(chip->dev, "%s could not create tcpc task\n", __func__);
		goto init_alert_err;
	}

	sched_setscheduler(chip->irq_worker_task, SCHED_FIFO, &param);
	kthread_init_work(&chip->irq_work, mt6360_irq_work_handler);

	ret = request_irq(chip->irq, mt6360_intr_handler, IRQF_TRIGGER_FALLING |
			  IRQF_NO_THREAD | IRQF_NO_SUSPEND, name, chip);
	if (ret < 0) {
		dev_err(chip->dev, "%s fail to request irq%d, gpio%d (%d)\n",
			__func__, chip->irq, chip->irq_gpio, ret);
		goto init_alert_err;
	}
	enable_irq_wake(chip->irq);
	return 0;

init_alert_err:
	return -EINVAL;
}

static inline int mt6360_vend_alert_status_clear(struct tcpc_device *tcpc,
						 const u8 *mask)
{
	mt6360_i2c_block_write(tcpc, MT6360_REG_MT_INT1,
			       MT6360_VEND_INT_MAX, mask);
	return mt6360_i2c_write16(tcpc, TCPC_V10_REG_ALERT,
				  TCPC_V10_REG_ALERT_VENDOR_DEFINED);
}

static int mt6360_alert_status_clear(struct tcpc_device *tcpc, u32 mask)
{
	u16 std_mask = mask & 0xffff;

	if (std_mask)
		return mt6360_i2c_write16(tcpc, TCPC_V10_REG_ALERT, std_mask);
	return 0;
}

static int mt6360_set_clock_gating(struct tcpc_device *tcpc_dev, bool en)
{
	int ret = 0;

#ifdef CONFIG_TCPC_CLOCK_GATING
	u8 clk1 = MT6360_CLK_DIV_600K_EN | MT6360_CLK_DIV_300K_EN;
	u8 clk2 = MT6360_CLK_DIV_2P4M_EN;

	if (!en) {
		clk1 |= MT6360_CLK_BCLK2_EN | MT6360_CLK_BCLK_EN;
		clk2 |= MT6360_CLK_CK_24M_EN | MT6360_CLK_PCLK_EN;
	}

	if (en) {
		ret = mt6360_alert_status_clear(tcpc_dev,
						TCPC_REG_ALERT_RX_STATUS |
						TCPC_REG_ALERT_RX_HARD_RST |
						TCPC_REG_ALERT_RX_BUF_OVF);
	}

	if (ret == 0)
		ret = mt6360_i2c_write8(tcpc_dev, MT6360_REG_CLK_CTRL1, clk1);
	if (ret == 0)
		ret = mt6360_i2c_write8(tcpc_dev, MT6360_REG_CLK_CTRL2, clk2);
#endif	/* CONFIG_TCPC_CLOCK_GATING */

	return ret;
}

static inline int mt6360_init_drp_duty(struct tcpc_device *tcpc)
{
	/*
	 * DRP Toggle Cycle : 51.2 + 6.4*val ms
	 * DRP Duyt Ctrl : dcSRC: /1024
	 */
	mt6360_i2c_write8(tcpc, MT6360_REG_DRP_CTRL1, 4);
	mt6360_i2c_write16(tcpc, MT6360_REG_DRP_CTRL2, TCPC_NORMAL_RP_DUTY);
	return 0;
}

static inline int mt6360_init_phy_ctrl(struct tcpc_device *tcpc)
{
	/* Disable TX Discard and auto-retry method */
	mt6360_i2c_write8(tcpc, MT6360_REG_PHY_CTRL1,
			  MT6360_REG_PHY_CTRL1_SET(0, 7, 0, 0));
	/* PHY CDR threshold */
	mt6360_i2c_write8(tcpc, MT6360_REG_PHY_CTRL2, 0x3A);
	/* Transition window count */
	mt6360_i2c_write8(tcpc, MT6360_REG_PHY_CTRL3, 0x82);
	/* BMC Decoder idle time, 164ns per steps */
	mt6360_i2c_write8(tcpc, MT6360_REG_PHY_CTRL7, 0x36);
	mt6360_i2c_write8(tcpc, MT6360_REG_PHY_CTRL11, 0x60);
	/* Retry period setting, 416ns per step */
	mt6360_i2c_write8(tcpc, MT6360_REG_PHY_CTRL12, 0x3C);
	mt6360_i2c_write8(tcpc, MT6360_REG_RX_CTRL1, 0xE8);
	return 0;
}

static int mt6360_force_discharge_control(struct tcpc_device *tcpc, bool en)
{
	u8 dischg_bit = TCPC_V10_REG_FORCE_DISC_EN;

	return (en ? mt6360_i2c_set_bit : mt6360_i2c_clr_bit)
		(tcpc, TCPC_V10_REG_POWER_CTRL, dischg_bit);
}

static inline int mt6360_vconn_oc_handler(struct tcpc_device *tcpc)
{
	int ret;
	u8 reg;

	ret = mt6360_i2c_read8(tcpc, MT6360_REG_VCONN_CTRL1, &reg);
	if (ret < 0)
		return ret;
	/* If current limit is enabled, there's no need to turn off vconn */
	if (reg & MT6360_VCONN_CLIMIT_EN)
		return 0;
	return tcpci_set_vconn(tcpc, false);
}

static int mt6360_fault_status_clear(struct tcpc_device *tcpc, u8 status)
{
	int ret;

	/*
	 * Not sure how to react after discharge fail
	 * follow previous H/W behavior, turn off force discharge
	 */
	if (status & TCPC_V10_REG_FAULT_STATUS_FORCE_DISC_FAIL)
		mt6360_force_discharge_control(tcpc, false);
	if (status & TCPC_V10_REG_FAULT_STATUS_VCONN_OC)
		mt6360_vconn_oc_handler(tcpc);

	ret = mt6360_i2c_write8(tcpc, TCPC_V10_REG_FAULT_STATUS, status);


	return ret;
}

static int mt6360_get_alert_mask(struct tcpc_device *tcpc, u32 *mask)
{
	int ret;
	u16 data;

	ret = mt6360_i2c_read16(tcpc, TCPC_V10_REG_ALERT_MASK, &data);
	if (ret < 0)
		return ret;
	*mask = data;

	return 0;
}

static int mt6360_get_alert_status(struct tcpc_device *tcpc, u32 *alert)
{
	int ret;
	u16 data;

	ret = mt6360_i2c_read16(tcpc, TCPC_V10_REG_ALERT, &data);
	if (ret < 0)
		return ret;
	*alert = data;

	return 0;
}

static int mt6360_get_power_status(struct tcpc_device *tcpc, u16 *status)
{
	int ret;
	u8 data;

	ret = mt6360_i2c_read8(tcpc, TCPC_V10_REG_POWER_STATUS, &data);
	if (ret < 0)
		return ret;

	*status = 0;
	if (data & TCPC_V10_REG_POWER_STATUS_VBUS_PRES)
		*status |= TCPC_REG_POWER_STATUS_VBUS_PRES;

	/*
	 * Vsafe0v only triggers when vbus falls under 0.8V,
	 * also update parameter if vbus present triggers
	 */
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = tcpci_is_vsafe0v(tcpc);
	if (ret >= 0)
		tcpc->vbus_safe0v = ret ? true : false;
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

	return 0;
}

static inline int mt6360_get_fault_status(struct tcpc_device *tcpc, u8 *status)
{
	return mt6360_i2c_read8(tcpc, TCPC_V10_REG_FAULT_STATUS, status);
}

static int mt6360_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2)
{
	int ret;
	bool act_as_sink, act_as_drp;
	u8 status, role_ctrl, cc_role;

	ret = mt6360_i2c_read8(tcpc, TCPC_V10_REG_CC_STATUS, &status);
	if (ret < 0)
		return ret;

	ret = mt6360_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL, &role_ctrl);
	if (ret < 0)
		return ret;

	if (status & TCPC_V10_REG_CC_STATUS_DRP_TOGGLING) {
		*cc1 = TYPEC_CC_DRP_TOGGLING;
		*cc2 = TYPEC_CC_DRP_TOGGLING;
		return 0;
	}

	*cc1 = TCPC_V10_REG_CC_STATUS_CC1(status);
	*cc2 = TCPC_V10_REG_CC_STATUS_CC2(status);

	act_as_drp = TCPC_V10_REG_ROLE_CTRL_DRP & role_ctrl;

	if (act_as_drp)
		act_as_sink = TCPC_V10_REG_CC_STATUS_DRP_RESULT(status);
	else {
		cc_role = TCPC_V10_REG_CC_STATUS_CC1(role_ctrl);
		if (cc_role == TYPEC_CC_RP)
			act_as_sink = false;
		else
			act_as_sink = true;
	}

	/*
	 * If status is not open, then OR in termination to convert to
	 * enum tcpc_cc_voltage_status.
	 */

	if (*cc1 != TYPEC_CC_VOLT_OPEN)
		*cc1 |= (act_as_sink << 2);

	if (*cc2 != TYPEC_CC_VOLT_OPEN)
		*cc2 |= (act_as_sink << 2);

	return 0;
}

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
static int mt6360_enable_vsafe0v_detect(struct tcpc_device *tcpc, bool en)
{
	return (en ? mt6360_i2c_set_bit : mt6360_i2c_clr_bit)
		(tcpc, MT6360_REG_MT_MASK1, MT6360_M_VBUS_SAFE0V);
}
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

static int mt6360_set_cc(struct tcpc_device *tcpc, int pull)
{
	int ret;
	u8 data;
	int rp_lvl = TYPEC_CC_PULL_GET_RP_LVL(pull);
#ifdef CONFIG_WD_SBU_POLLING
	struct mt6360_chip *chip = tcpc_get_dev_data(tcpc);
#endif /* CONFIG_WD_SBU_POLLING */

	MT6360_INFO("%s %d\n", __func__, pull);
	pull = TYPEC_CC_PULL_GET_RES(pull);
	if (pull == TYPEC_CC_DRP) {
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(1, rp_lvl, TYPEC_CC_RD,
						      TYPEC_CC_RD);
		ret = mt6360_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, data);
		if (ret < 0)
			return ret;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
		mt6360_enable_vsafe0v_detect(tcpc, false);
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

		mt6360_enable_auto_rpconnect(tcpc, true);
		mt6360_enable_oneshot_rpconnect(tcpc, true);

#ifdef CONFIG_TCPC_LOW_POWER_MODE
		tcpci_set_low_power_mode(tcpc, true, pull);
#endif /* CONFIG_TCPC_LOW_POWER_MODE */
		ret = mt6360_command(tcpc, TCPM_CMD_LOOK_CONNECTION);
#ifdef CONFIG_WD_SBU_POLLING
#ifdef CONFIG_WD_POLLING_ONLY
		schedule_delayed_work(&chip->usbid_poll_work,
					msecs_to_jiffies(500));
#else
		mt6360_enable_usbid_polling(chip, true);
#endif /* CONFIG_WD_POLLING_ONLY */
#endif /* CONFIG_WD_SBU_POLLING */
	} else {
#ifdef CONFIG_WD_POLLING_ONLY
		cancel_delayed_work(&chip->usbid_poll_work);
		mt6360_enable_usbid_polling(chip, false);
#endif /* CONFIG_WD_POLLING_ONLY */
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(0, rp_lvl, pull, pull);
		ret = mt6360_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, data);
		mt6360_enable_auto_rpconnect(tcpc, false);
		mt6360_enable_oneshot_rpconnect(tcpc, true);
	}

	return ret;
}

static int mt6360_set_polarity(struct tcpc_device *tcpc, int polarity)
{
	return (polarity ? mt6360_i2c_set_bit : mt6360_i2c_clr_bit)
		(tcpc, TCPC_V10_REG_TCPC_CTRL,
		 TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT);
}

static int mt6360_set_low_rp_duty(struct tcpc_device *tcpc, bool low_rp)
{
	uint16_t duty = low_rp ? TCPC_LOW_RP_DUTY : TCPC_NORMAL_RP_DUTY;

	return mt6360_i2c_write16(tcpc, MT6360_REG_DRP_CTRL2, duty);
}

static int mt6360_is_vconn_fault(struct tcpc_device *tcpc, bool *fault)
{
	int ret;
	u8 status;

	ret = mt6360_i2c_read8(tcpc, MT6360_REG_MT_ST1, &status);
	if (ret < 0)
		return ret;
	if (status & MT6360_ST_VCONN_SHT_GND) {
		*fault = true;
		return 0;
	}

	ret = mt6360_i2c_read8(tcpc, MT6360_REG_MT_ST2, &status);
	if (ret < 0)
		return ret;
	*fault = (status & MT6360_ST_VCONN_FAULT) ? true : false;
	return 0;
}

static int mt6360_set_vconn(struct tcpc_device *tcpc, int en)
{
	int ret;
	bool fault = false;

	MT6360_INFO("%s %d\n", __func__, en);
	/*
	 * Set Vconn OVP RVP
	 * Otherwise vconn present fail will be triggered
	 */
	if (en) {
		mt6360_i2c_set_bit(tcpc, MT6360_REG_VCONN_CTRL2,
				   MT6360_VCONN_OVP_CC_EN);
		mt6360_i2c_set_bit(tcpc, MT6360_REG_VCONN_CTRL3,
				   MT6360_VCONN_RVP_EN);
		usleep_range(20, 50);
		ret = mt6360_is_vconn_fault(tcpc, &fault);
		if (ret >= 0 && fault) {
			MT6360_INFO("%s Vconn fault\n", __func__);
			return -EINVAL;
		}
	}
	ret = (en ? mt6360_i2c_set_bit : mt6360_i2c_clr_bit)
		(tcpc, TCPC_V10_REG_POWER_CTRL, TCPC_V10_REG_POWER_CTRL_VCONN);
	if (!en) {
		mt6360_i2c_clr_bit(tcpc, MT6360_REG_VCONN_CTRL2,
				   MT6360_VCONN_OVP_CC_EN);
		mt6360_i2c_clr_bit(tcpc, MT6360_REG_VCONN_CTRL3,
				   MT6360_VCONN_RVP_EN);
	}
	return ret;
}

#ifdef CONFIG_TCPC_LOW_POWER_MODE
static int mt6360_is_low_power_mode(struct tcpc_device *tcpc)
{
	int ret;
	u8 data;

	ret = mt6360_i2c_read8(tcpc, MT6360_REG_MODE_CTRL3, &data);
	if (ret < 0)
		return ret;

	return (data & MT6360_LPWR_EN) != 0;
}

static int mt6360_set_low_power_mode(struct tcpc_device *tcpc, bool en,
				     int pull)
{
	u8 data = 0;

	if (en) {
		data = MT6360_LPWR_EN | MT6360_LPWR_LDO_EN;

#ifdef CONFIG_TYPEC_CAP_NORP_SRC
		data |= MT6360_VBUS_DET_EN | MT6360_PD_BG_EN |
			MT6360_PD_IREF_EN;
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */
	} else {
		data = MT6360_VBUS_DET_EN | MT6360_PD_BG_EN |
			MT6360_PD_IREF_EN | MT6360_BMCIO_OSC_EN;
		mt6360_enable_vsafe0v_detect(tcpc, true);
	}
	return mt6360_i2c_write8(tcpc, MT6360_REG_MODE_CTRL3, data);
}
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

static int mt6360_set_watchdog(struct tcpc_device *tcpc_dev, bool en)
{
	return (en ? mt6360_i2c_set_bit : mt6360_i2c_clr_bit)
		(tcpc_dev, TCPC_V10_REG_TCPC_CTRL,
		 TCPC_V10_REG_TCPC_CTRL_EN_WDT);
}

static int mt6360_tcpc_deinit(struct tcpc_device *tcpc_dev)
{
#ifdef CONFIG_TCPC_SHUTDOWN_CC_DETACH
	mt6360_set_cc(tcpc_dev, TYPEC_CC_DRP);
	mt6360_set_cc(tcpc_dev, TYPEC_CC_OPEN);

	mt6360_i2c_write8(tcpc_dev, MT6360_REG_I2CRST_CTRL,
			  MT6360_REG_I2CRST_SET(true, 4));
#else
	mt6360_i2c_write8(tcpc_dev, MT6360_REG_SWRESET, 1);
#endif	/* CONFIG_TCPC_SHUTDOWN_CC_DETACH */

	return 0;
}

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
static int mt6360_is_vsafe0v(struct tcpc_device *tcpc)
{
	int ret;
	u8 regval;

	ret = mt6360_i2c_read8(tcpc, MT6360_REG_MT_ST1, &regval);
	if (ret < 0)
		return ret;
	return (regval & MT6360_ST_VBUS_SAFE0V) ? 1 : 0;
}

static int mt6360_vsafe0v_irq_handler(struct tcpc_device *tcpc)
{
	int ret;

	MT6360_INFO("%s\n", __func__);
	ret = tcpci_is_vsafe0v(tcpc);
	if (ret < 0)
		return ret;
	tcpc->vbus_safe0v = ret ? true : false;
	return 0;
}
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

#ifdef CONFIG_WATER_DETECTION
static int mt6360_wd_irq_handler(struct tcpc_device *tcpc)
{
	int ret;
	u8 status;
	struct mt6360_chip *chip = tcpc_get_dev_data(tcpc);

	MT6360_INFO("%s\n", __func__);

	ret = mt6360_i2c_read8(tcpc, MT6360_REG_WD_DET_CTRL1, &status);
	if (ret < 0)
		return ret;

	if (status & MT6360_WD_ALL_RUST_STS) {
		MT6360_INFO("%s not to handle detecting water\n", __func__);
		return 0;
	}
	ret = tcpci_is_water_detected(tcpc);
	if (ret < 0)
		return ret;
	if (ret)
		goto retry;
	if (atomic_dec_and_test(&chip->wd_protect_rty)) {
		tcpc_typec_handle_wd(tcpc, false);
		atomic_set(&chip->wd_protect_rty,
			   CONFIG_WD_PROTECT_RETRY_COUNT);
		return 0;
	}
	MT6360_INFO("%s rty %d\n",
		    __func__, atomic_read(&chip->wd_protect_rty));
retry:
	/* retry */
	tcpci_set_water_protection(tcpc, false);
	tcpci_set_water_protection(tcpc, true);
	return 0;
}

#ifdef CONFIG_WD_POLLING_ONLY
static void mt6360_usbid_poll_work(struct work_struct *work)
{
	int ret, cc1, cc2;
	struct mt6360_chip *chip = container_of(work, struct mt6360_chip,
						usbid_poll_work.work);

	tcpci_lock_typec(chip->tcpc);

	ret = mt6360_get_cc(chip->tcpc, &cc1, &cc2);
	if (ret < 0)
		goto out;

	if (cc1 != TYPEC_CC_DRP_TOGGLING || cc2 != TYPEC_CC_DRP_TOGGLING)
		goto out;

	mt6360_enable_usbid_polling(chip, true);
out:
	tcpci_unlock_typec(chip->tcpc);
}
#endif /* CONFIG_WD_POLLING_ONLY */
#endif /* CONFIG_WATER_DETECTION */

#ifdef CONFIG_CABLE_TYPE_DETECTION
static inline int mt6360_get_cable_type(struct tcpc_device *tcpc,
					enum tcpc_cable_type *type)
{
	int ret;
	u8 status;

	ret = mt6360_i2c_read8(tcpc, MT6360_REG_MT_ST3, &status);
	if (ret < 0)
		return ret;
	*type = (status & MT6360_ST_CABLE_TYPE) ?
		TCPC_CABLE_TYPE_A2C : TCPC_CABLE_TYPE_C2C;
	return 0;
}

static inline int mt6360_ctd_evt_process(struct tcpc_device *tcpc)
{
	int ret;
	enum tcpc_cable_type cable_type;

	ret = mt6360_get_cable_type(tcpc, &cable_type);
	if (ret < 0)
		return ret;

	tcpc_typec_handle_ctd(tcpc, cable_type);
	return ret;
}

static int mt6360_ctd_irq_handler(struct tcpc_device *tcpc)
{
	return mt6360_ctd_evt_process(tcpc);
}
#endif /* CONFIG_CABLE_TYPE_DETECTION */

static inline int mt6360_vconn_fault_evt_process(struct tcpc_device *tcpc)
{
	int ret;
	bool fault = false;

	ret = mt6360_is_vconn_fault(tcpc, &fault);
	if (ret >= 0 && fault) {
		MT6360_INFO("%s\n", __func__);
		mt6360_i2c_clr_bit(tcpc, MT6360_REG_VCONN_CTRL2,
				   MT6360_VCONN_OVP_CC_EN);
		mt6360_i2c_clr_bit(tcpc, MT6360_REG_VCONN_CTRL3,
				   MT6360_VCONN_RVP_EN);
	}
	return 0;
}

static int mt6360_vconn_shtgnd_irq_handler(struct tcpc_device *tcpc)
{
	MT6360_INFO("%s\n", __func__);
	mt6360_vconn_fault_evt_process(tcpc);
	return 0;
}

static int mt6360_vconn_ov_cc1_irq_handler(struct tcpc_device *tcpc)
{
	MT6360_INFO("%s\n", __func__);
	mt6360_vconn_fault_evt_process(tcpc);
	return 0;
}

static int mt6360_vconn_ov_cc2_irq_handler(struct tcpc_device *tcpc)
{
	MT6360_INFO("%s\n", __func__);
	mt6360_vconn_fault_evt_process(tcpc);
	return 0;
}

static int mt6360_vconn_ocr_irq_handler(struct tcpc_device *tcpc)
{
	MT6360_INFO("%s\n", __func__);
	mt6360_vconn_fault_evt_process(tcpc);
	return 0;
}

static int mt6360_vconn_invalid_irq_handler(struct tcpc_device *tcpc)
{
	MT6360_INFO("%s\n", __func__);
	mt6360_vconn_fault_evt_process(tcpc);
	return 0;
}

struct irq_mapping_tbl {
	u8 num;
	const char *name;
	int (*hdlr)(struct tcpc_device *tcpc);
};

#define MT6360_IRQ_MAPPING(_num, _name) \
	{ .num = _num, .name = #_name, .hdlr = mt6360_##_name##_irq_handler }

static struct irq_mapping_tbl mt6360_vend_irq_mapping_tbl[] = {
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	MT6360_IRQ_MAPPING(1, vsafe0v),
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

#ifdef CONFIG_WATER_DETECTION
	MT6360_IRQ_MAPPING(14, wd),
#endif /* CONFIG_WATER_DETECTION */

#ifdef CONFIG_CABLE_TYPE_DETECTION
	MT6360_IRQ_MAPPING(20, ctd),
#endif /* CONFIG_CABLE_TYPE_DETECTION */

	MT6360_IRQ_MAPPING(3, vconn_shtgnd),
	MT6360_IRQ_MAPPING(8, vconn_ov_cc1),
	MT6360_IRQ_MAPPING(9, vconn_ov_cc2),
	MT6360_IRQ_MAPPING(10, vconn_ocr),
	MT6360_IRQ_MAPPING(12, vconn_invalid),
};

static int mt6360_alert_vendor_defined_handler(struct tcpc_device *tcpc)
{
	int ret, i, irqnum, irqbit;
	u8 alert[MT6360_VEND_INT_MAX];
	u8 mask[MT6360_VEND_INT_MAX];

	ret = mt6360_i2c_block_read(tcpc, MT6360_REG_MT_INT1,
				    MT6360_VEND_INT_MAX, alert);
	if (ret < 0)
		return ret;

	ret = mt6360_i2c_block_read(tcpc, MT6360_REG_MT_MASK1,
				    MT6360_VEND_INT_MAX, mask);
	if (ret < 0)
		return ret;

	for (i = 0; i < MT6360_VEND_INT_MAX; i++) {
		if (!alert[i])
			continue;
		MT6360_INFO("Vend INT%d:0x%02X\n", i + 1, alert[i]);
		alert[i] &= mask[i];
	}

	mt6360_vend_alert_status_clear(tcpc, alert);

	for (i = 0; i < ARRAY_SIZE(mt6360_vend_irq_mapping_tbl); i++) {
		irqnum = mt6360_vend_irq_mapping_tbl[i].num / 8;
		if (irqnum >= MT6360_VEND_INT_MAX)
			continue;
		alert[irqnum] &= mask[irqnum];
		irqbit = mt6360_vend_irq_mapping_tbl[i].num % 8;
		if (alert[irqnum] & (1 << irqbit))
			mt6360_vend_irq_mapping_tbl[i].hdlr(tcpc);
	}
	return 0;
}

#ifdef CONFIG_WATER_DETECTION
static inline int mt6360_init_water_detection(struct tcpc_device *tcpc)
{
	/*
	 * 0xc1[7:6] -> Water Detection mode
	 * 0xc1[3:2] -> Detect Count for judge system in RUST or not
	 * 0xc1[1:0] -> Rust exiting counts during rust protection flow
	 * (when RUST_PROTECT_EN is "1"), set as 4
	 */
	mt6360_i2c_write8(tcpc, MT6360_REG_WD_DET_CTRL2, 0x02);

	/* DPDM Pull up capability, 220u */
	mt6360_i2c_write8(tcpc, MT6360_REG_WD_DET_CTRL3, 0xFF);

	/* CC Role Setting in RUST protection or one-shot process, CC = OPEN */
	mt6360_i2c_set_bit(tcpc, MT6360_REG_WD_DET_CTRL4,
			   MT6360_WD_DET_CC_RPSEL | MT6360_WD_DET_CC_ROLE_PRT);

	/* sleep time, water protection check frequency */
	mt6360_i2c_write8(tcpc, MT6360_REG_WD_DET_CTRL5,
			  MT6360_REG_WD_DET_CTRL5_SET(9));

	/* Enable Water Rust Detection Flow */
	mt6360_i2c_set_bit(tcpc, MT6360_REG_WD_DET_CTRL1, MT6360_WD_DET_EN);

	return 0;
}

static inline int mt6360_get_usbid_adc(struct tcpc_device *tcpc, int *usbid)
{
	int ret;
	struct mt6360_chip *chip = tcpc_get_dev_data(tcpc);

	ret = charger_dev_get_adc(chip->chgdev, ADC_CHANNEL_USBID,
				  usbid, usbid);
	if (ret < 0) {
		dev_err(chip->dev, "%s fail(%d)\n", __func__, ret);
		return ret;
	}
	/* To mV */
	*usbid /= 1000;
	MT6360_INFO("%s %dmV\n", __func__, *usbid);
	return 0;
}

static inline bool mt6360_is_audio_device(struct tcpc_device *tcpc, int usbid)
{
	int ret;
	struct mt6360_chip *chip = tcpc_get_dev_data(tcpc);

	if (usbid >= CONFIG_WD_SBU_PH_AUDDEV)
		return false;

	/* Pull high with 1K resistor */
	ret = charger_dev_set_usbid_rup(chip->chgdev, 1000);
	if (ret < 0) {
		dev_err(chip->dev, "%s usbid rup1k fail\n", __func__);
		goto not_auddev;
	}

	ret = mt6360_get_usbid_adc(tcpc, &usbid);
	if (ret < 0) {
		dev_err(chip->dev, "%s get usbid adc fail\n", __func__);
		goto not_auddev;
	}

	if (usbid >= CONFIG_WD_SBU_AUD_UBOUND)
		goto not_auddev;

	return true;
not_auddev:
	charger_dev_set_usbid_rup(chip->chgdev, 500000);
	return false;
}

static int mt6360_is_water_detected(struct tcpc_device *tcpc)
{
	int ret, usbid;
	u32 ub, lb;
	struct mt6360_chip *chip = tcpc_get_dev_data(tcpc);
#ifdef CONFIG_CABLE_TYPE_DETECTION
	u8 ctd_evt;
	enum tcpc_cable_type cable_type;
#endif /* CONFIG_CABLE_TYPE_DETECTION */

	__pm_stay_awake(&chip->wd_wakeup_src);

#ifdef CONFIG_WD_SBU_POLLING
	ret = mt6360_enable_usbid_polling(chip, false);
#else
	ret = charger_dev_enable_usbid(chip->chgdev, false);
#endif /* CONFIG_WD_SBU_POLLING */
	if (ret < 0) {
		dev_err(chip->dev, "%s pull low usbid fail\n", __func__);
		goto err;
	}

	ret = charger_dev_enable_usbid_floating(chip->chgdev, false);
	if (ret < 0)
		dev_info(chip->dev, "%s disable usbid float fail\n", __func__);
	ret = mt6360_get_usbid_adc(tcpc, &usbid);
	if (ret < 0) {
		dev_err(chip->dev, "%s get usbid adc fail\n", __func__);
		goto err;
	}
	ret = charger_dev_enable_usbid_floating(chip->chgdev, true);
	if (ret < 0)
		dev_info(chip->dev, "%s enable usbid float fail\n", __func__);
	MT6360_INFO("%s pl usbid %dmV\n", __func__, usbid);

	/* Water detected, check again */
	if (usbid > CONFIG_WD_SBU_PL_BOUND) {
		charger_dev_enable_usbid_floating(chip->chgdev, false);
		if (ret < 0)
			dev_info(chip->dev, "%s disable usbid float fail\n",
				 __func__);
		ret = mt6360_get_usbid_adc(tcpc, &usbid);
		if (ret < 0) {
			dev_info(chip->dev, "%s get usbid adc fail\n",
				 __func__);
			goto err;
		}
		charger_dev_enable_usbid_floating(chip->chgdev, true);
		if (ret < 0)
			dev_info(chip->dev, "%s enable usbid float fail\n",
				 __func__);
		MT6360_INFO("%s recheck pl usbid %dmV\n", __func__, usbid);
		if (usbid > CONFIG_WD_SBU_PL_BOUND) {
			ret = 1;
			goto out;
		}
	}

	/* Pull high usb idpin */
	ret = charger_dev_set_usbid_src_ton(chip->chgdev, 0);
	if (ret < 0) {
		dev_err(chip->dev, "%s usbid always src on fail\n", __func__);
		goto err;
	}

	ret = charger_dev_set_usbid_rup(chip->chgdev, 500000);
	if (ret < 0) {
		dev_err(chip->dev, "%s usbid rup500k fail\n", __func__);
		goto err;
	}

	ret = charger_dev_enable_usbid(chip->chgdev, true);
	if (ret < 0) {
		dev_err(chip->dev, "%s usbid pulled high fail\n", __func__);
		goto err;
	}

	ret = mt6360_get_usbid_adc(tcpc, &usbid);
	if (ret < 0) {
		dev_err(chip->dev, "%s get usbid adc fail\n", __func__);
		goto err;
	}
	ub = tcpc->usbid_calib * 110 / 100;
	lb = CONFIG_WD_SBU_PH_LBOUND;
	MT6360_INFO("%s lb %d, ub %d, ph usbid %dmV\n", __func__, lb, ub,
		    usbid);

	if (usbid >= lb && usbid <= ub) {
		ret = 0;
		goto out;
	}

	/* Water detected, check again */
	msleep(100); /* to avoid the same behavior of the other device */
	ret = mt6360_get_usbid_adc(tcpc, &usbid);
	if (ret >= 0) {
		MT6360_INFO("%s recheck usbid %dmV\n", __func__, usbid);
		if (usbid >= lb && usbid <= ub) {
			ret = 0;
			goto out;
		}
	} else
		dev_info(chip->dev, "%s get usbid adc fail\n", __func__);
#ifdef CONFIG_CABLE_TYPE_DETECTION
	cable_type = tcpc->typec_cable_type;
	if (cable_type == TCPC_CABLE_TYPE_NONE) {
		ret = mt6360_i2c_read8(chip->tcpc, MT6360_REG_MT_INT3,
				       &ctd_evt);
		if (ret >= 0 && (ctd_evt & MT6360_M_CTD))
			ret = mt6360_get_cable_type(tcpc, &cable_type);
	}
	if (cable_type == TCPC_CABLE_TYPE_C2C) {
		if (((usbid >= CONFIG_WD_SBU_PH_LBOUND1_C2C) &&
		    (usbid <= CONFIG_WD_SBU_PH_UBOUND1_C2C)) ||
		    (usbid > CONFIG_WD_SBU_PH_UBOUND2_C2C)) {
			MT6360_INFO("%s ignore for C2C\n", __func__);
			ret = 0;
			goto out;
		}
	}
#endif /* CONFIG_CABLE_TYPE_DETECTION */

	if (mt6360_is_audio_device(tcpc, usbid)) {
		ret = 0;
		MT6360_INFO("%s audio dev but not water\n", __func__);
		goto out;
	}
	ret = 1;
out:
	MT6360_INFO("%s %s water\n", __func__, ret ? "with" : "without");
err:
	charger_dev_enable_usbid_floating(chip->chgdev, true);
	charger_dev_enable_usbid(chip->chgdev, false);
	__pm_relax(&chip->wd_wakeup_src);
	return ret;
}

static int mt6360_set_water_protection(struct tcpc_device *tcpc, bool en)
{
	int ret;

	if (en)
		mt6360_enable_auto_rpconnect(tcpc, false);
	ret = (en ? mt6360_i2c_set_bit : mt6360_i2c_clr_bit)
		(tcpc, MT6360_REG_WD_DET_CTRL1, MT6360_WD_PROTECTION_EN);
	if (!en)
		mt6360_enable_auto_rpconnect(tcpc, true);
	return ret;
}

static int mt6360_set_usbid_polling(struct tcpc_device *tcpc, bool en)
{
	int ret;
	struct mt6360_chip *chip = tcpc_get_dev_data(tcpc);

	if (!en)
		cancel_delayed_work(&chip->usbid_poll_work);

	ret = mt6360_enable_usbid_polling(chip, en);
	if (ret < 0) {
		dev_notice(chip->dev, "%s fail\n", __func__);
		return ret;
	}

	return ret;
}

static int mt6360_water_calibration(struct tcpc_device *tcpc)
{
	/* TODO: Calibration flow */
	MT6360_INFO("%s not to calibrate, use 1.8V\n", __func__);
	tcpc->usbid_calib = CONFIG_WD_SBU_CALIB_INIT;
	return 0;
}
#endif /* CONFIG_WATER_DETECTION */

static int mt6360_tcpc_init(struct tcpc_device *tcpc, bool sw_reset)
{
	int ret;

	MT6360_INFO("\n");

	if (sw_reset) {
		ret = mt6360_software_reset(tcpc);
		if (ret < 0)
			return ret;
	}

#ifdef CONFIG_TCPC_I2CRST_EN
	mt6360_i2c_write8(tcpc, MT6360_REG_I2CRST_CTRL,
			  MT6360_REG_I2CRST_SET(true, 0x0f));
#endif	/* CONFIG_TCPC_I2CRST_EN */

	/* UFP Both RD setting */
	/* DRP = 0, RpVal = 0 (Default), Rd, Rd */
	mt6360_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL,
			  TCPC_V10_REG_ROLE_CTRL_RES_SET(0, 0, CC_RD, CC_RD));

	/*
	 * CC Detect Debounce : 26.7*val us
	 * Transition window count : spec 12~20us, based on 2.4MHz
	 */
	mt6360_i2c_write8(tcpc, MT6360_REG_DEBOUNCE_CTRL1, 10);
	mt6360_init_drp_duty(tcpc);

	/* RX/TX Clock Gating (Auto Mode) */
	if (!sw_reset)
		mt6360_set_clock_gating(tcpc, true);

	mt6360_init_phy_ctrl(tcpc);

	/* Vconn OC */
	mt6360_i2c_write8(tcpc, MT6360_REG_VCONN_CTRL1, 0x41);

	/* Set HILOCCFILTER 250us */
	mt6360_i2c_write8(tcpc, MT6360_REG_VBUS_CTRL2, 0x0A);

	/* Set cc open when PMIC sends Vsys UV signal */
	mt6360_i2c_set_bit(tcpc, MT6360_REG_RX_CTRL2, MT6360_OPEN400MS_EN);

	/* Enable LOOK4CONNECTION alert */
	mt6360_i2c_set_bit(tcpc, TCPC_V10_REG_TCPC_CTRL,
			   TCPC_V10_REG_TCPC_CTRL_EN_LOOK4CONNECTION_ALERT);

	/* Enable Rp connect oneshot */
	mt6360_enable_oneshot_rpconnect(tcpc, true);

#ifdef CONFIG_WATER_DETECTION
	mt6360_init_water_detection(tcpc);
#endif /* CONFIG_WATER_DETECTION */

	if (sw_reset)
		mt6360_init_alert_mask(tcpc);

	if (tcpc->tcpc_flags & TCPC_FLAGS_WATCHDOG_EN) {
		mt6360_i2c_write8(tcpc, MT6360_REG_WATCHDOG_CTRL,
				  MT6360_REG_WATCHDOG_CTRL_SET(7));
		mt6360_set_watchdog(tcpc, true);
	}

	/* SHIPPING off, AUTOIDLE off */
	mt6360_i2c_write8(tcpc, MT6360_REG_MODE_CTRL2,
			  MT6360_REG_MODE_CTRL2_SET(1, 0, 2));

	return 0;
}

#ifdef CONFIG_USB_POWER_DELIVERY
static int mt6360_set_msg_header(
	struct tcpc_device *tcpc, u8 power_role, u8 data_role)
{
	u8 msg_hdr = TCPC_V10_REG_MSG_HDR_INFO_SET(data_role, power_role);

	return mt6360_i2c_write8(tcpc, TCPC_V10_REG_MSG_HDR_INFO, msg_hdr);
}

static int mt6360_protocol_reset(struct tcpc_device *tcpc_dev)
{
	mt6360_i2c_clr_bit(tcpc_dev, MT6360_REG_PHY_CTRL8, MT6360_PRL_FSM_RSTB);
	mdelay(1);
	mt6360_i2c_set_bit(tcpc_dev, MT6360_REG_PHY_CTRL8, MT6360_PRL_FSM_RSTB);
	return 0;
}

static int mt6360_set_rx_enable(struct tcpc_device *tcpc, u8 en)
{
	int ret = 0;

	if (en)
		ret = mt6360_set_clock_gating(tcpc, false);

	if (ret == 0)
		ret = mt6360_i2c_write8(tcpc, TCPC_V10_REG_RX_DETECT, en);

	if ((ret == 0) && !en)
		ret = mt6360_set_clock_gating(tcpc, true);

	/* For testing */
	if (!en)
		mt6360_protocol_reset(tcpc);
	return ret;
}

static int mt6360_get_message(struct tcpc_device *tcpc, u32 *payload,
			      u16 *msg_head,
			      enum tcpm_transmit_type *frame_type)
{
	int ret;
	u8 type, cnt = 0;
	u8 buf[4] = {0};

	ret = mt6360_i2c_block_read(tcpc, TCPC_V10_REG_RX_BYTE_CNT, 4, buf);
	cnt = buf[0];
	type = buf[1];
	*msg_head = *(u16 *)&buf[2];

	/* TCPC 1.0 ==> no need to subtract the size of msg_head */
	if (ret >= 0 && cnt > 3) {
		cnt -= 3; /* MSG_HDR */
		ret = mt6360_i2c_block_read(tcpc, TCPC_V10_REG_RX_DATA, cnt,
					    (u8 *)payload);
	}
	*frame_type = (enum tcpm_transmit_type)type;

	/* Read complete, clear RX status alert bit */
	tcpci_alert_status_clear(tcpc, TCPC_V10_REG_ALERT_RX_STATUS |
				 TCPC_V10_REG_RX_OVERFLOW);

	return ret;
}

static int mt6360_set_bist_carrier_mode(struct tcpc_device *tcpc, u8 pattern)
{
	/* Don't support this function */
	return 0;
}

/* message header (2byte) + data object (7*4) */
#define MT6360_TRANSMIT_MAX_SIZE \
	(sizeof(u16) + sizeof(u32) * 7)

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
static int mt6360_retransmit(struct tcpc_device *tcpc)
{
	return mt6360_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
				 TCPC_V10_REG_TRANSMIT_SET(tcpc->pd_retry_count,
				 TCPC_TX_SOP));
}
#endif /* CONFIG_USB_PD_RETRY_CRC_DISCARD */

static int mt6360_transmit(struct tcpc_device *tcpc,
			   enum tcpm_transmit_type type, u16 header,
			   const u32 *data)
{
	int ret, data_cnt, packet_cnt;
	u8 temp[MT6360_TRANSMIT_MAX_SIZE];

	if (type < TCPC_TX_HARD_RESET) {
		data_cnt = sizeof(u32) * PD_HEADER_CNT(header);
		packet_cnt = data_cnt + sizeof(u16);

		temp[0] = packet_cnt;
		memcpy(temp + 1, (u8 *)&header, 2);
		if (data_cnt > 0)
			memcpy(temp + 3, (u8 *)data, data_cnt);

		ret = mt6360_i2c_block_write(tcpc, TCPC_V10_REG_TX_BYTE_CNT,
					     packet_cnt + 1, (u8 *)temp);
		if (ret < 0)
			return ret;
	}

	return mt6360_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
				TCPC_V10_REG_TRANSMIT_SET(tcpc->pd_retry_count,
				type));
}

static int mt6360_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
	return (en ? mt6360_i2c_set_bit : mt6360_i2c_clr_bit)
		(tcpc, TCPC_V10_REG_TCPC_CTRL,
		 TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE);
}
#endif /* CONFIG_USB_POWER_DELIVERY */

static struct tcpc_ops mt6360_tcpc_ops = {
	.init = mt6360_tcpc_init,
	.init_alert_mask = mt6360_init_alert_mask,
	.alert_status_clear = mt6360_alert_status_clear,
	.fault_status_clear = mt6360_fault_status_clear,
	.get_alert_mask = mt6360_get_alert_mask,
	.get_alert_status = mt6360_get_alert_status,
	.get_power_status = mt6360_get_power_status,
	.get_fault_status = mt6360_get_fault_status,
	.get_cc = mt6360_get_cc,
	.set_cc = mt6360_set_cc,
	.set_polarity = mt6360_set_polarity,
	.set_low_rp_duty = mt6360_set_low_rp_duty,
	.set_vconn = mt6360_set_vconn,
	.deinit = mt6360_tcpc_deinit,
	.alert_vendor_defined_handler = mt6360_alert_vendor_defined_handler,

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	.is_vsafe0v = mt6360_is_vsafe0v,
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

#ifdef CONFIG_TCPC_LOW_POWER_MODE
	.is_low_power_mode = mt6360_is_low_power_mode,
	.set_low_power_mode = mt6360_set_low_power_mode,
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

	.set_watchdog = mt6360_set_watchdog,

#ifdef CONFIG_USB_POWER_DELIVERY
	.set_msg_header = mt6360_set_msg_header,
	.set_rx_enable = mt6360_set_rx_enable,
	.protocol_reset = mt6360_protocol_reset,
	.get_message = mt6360_get_message,
	.transmit = mt6360_transmit,
	.set_bist_test_mode = mt6360_set_bist_test_mode,
	.set_bist_carrier_mode = mt6360_set_bist_carrier_mode,
#endif	/* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	.retransmit = mt6360_retransmit,
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#ifdef CONFIG_WATER_DETECTION
	.is_water_detected = mt6360_is_water_detected,
	.set_water_protection = mt6360_set_water_protection,
	.set_usbid_polling = mt6360_set_usbid_polling,
#endif /* CONFIG_WATER_DETECTION */

};

static int mt6360_init_ctd(struct mt6360_chip *chip)
{
	int ret = 0;

#ifdef CONFIG_CABLE_TYPE_DETECTION
	u8 ctd_evt;
#if CONFIG_MTK_GAUGE_VERSION == 30
	u8 status;
#endif

	chip->tcpc->typec_cable_type = TCPC_CABLE_TYPE_NONE;
	chip->handle_init_ctd = true;
	ret = mt6360_i2c_read8(chip->tcpc, MT6360_REG_MT_INT3, &ctd_evt);
	if (ret < 0)
		return ret;
	if (ctd_evt & MT6360_M_CTD) {
		mt6360_get_cable_type(chip->tcpc, &chip->init_cable_type);
#if CONFIG_MTK_GAUGE_VERSION == 30
		if (chip->init_cable_type == TCPC_CABLE_TYPE_C2C) {
			ret = charger_dev_get_ctd_dischg_status(chip->chgdev,
								&status);
			if (ret >= 0 && (status & 0x82))
				chip->init_cable_type = TCPC_CABLE_TYPE_A2C;
		}
#endif
	}
#endif /* CONFIG_CABLE_TYPE_DETECTION */

	return ret;
}

static int mt6360_parse_dt(struct mt6360_chip *chip, struct device *dev,
			   struct mt6360_tcpc_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	struct resource *res;
	int res_cnt, ret;

	if (!np)
		return -EINVAL;

	pr_info("%s\n", __func__);

	np = of_find_node_by_name(NULL, "type_c_port0");
	if (!np) {
		dev_err(dev, "%s find node fail\n", __func__);
		return -ENODEV;
	}

#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
	ret = of_get_named_gpio(np, "mt6360pd,intr_gpio", 0);
	if (ret < 0) {
		dev_err(dev, "%s no intr_gpio info(gpiolib)\n", __func__);
		return ret;
	}
	chip->irq_gpio = ret;
#else
	ret = of_property_read_u32(np, "mt6360pd,intr_gpio_num",
				   &chip->irq_gpio);
	if (ret < 0) {
		dev_err(dev, "%s no intr_gpio info\n", __func__);
		return ret;
	}
#endif /* !CONFIG_MTK_GPIO || CONFIG_MTK_GPIOLIB_STAND */

#ifdef CONFIG_MTK_TYPEC_WATER_DETECT_BY_PCB
#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
	ret = of_get_named_gpio(np, "mt6360pd,pcb_gpio", 0);
	if (ret < 0) {
		dev_info(dev, "%s no pcb_gpio info(gpiolib)\n", __func__);
		return ret;
	}
	chip->pcb_gpio = ret;

	ret = of_property_read_u32(np, "mt6360pd,pcb_gpio_polarity",
				    &chip->pcb_gpio_polarity);
	if (ret < 0) {
		dev_info(dev, "%s no pcb_gpio_polarity info\n", __func__);
		return ret;
	}
#else
	ret = of_property_read_u32(np, "mt6360pd,pcb_gpio_num",
				   &chip->pcb_gpio);
	if (ret < 0) {
		dev_info(dev, "%s no pcb_gpio info\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(np, "mt6360pd,pcb_gpio_polarity",
				    &chip->pcb_gpio_polarity);
	if (ret < 0) {
		dev_info(dev, "%s no pcb_gpio_polarity info\n", __func__);
		return ret;
	}
#endif /* !CONFIG_MTK_GPIO || CONFIG_MTK_GPIOLIB_STAND */
	ret = devm_gpio_request(dev, chip->pcb_gpio, "pcb_gpio");
	if (ret < 0) {
		dev_info(dev, "%s request pcb gpio fail\n", __func__);
		return ret;
	}
#endif /* CONFIG_MTK_TYPEC_WATER_DETECT_BY_PCB */

	res_cnt = of_irq_count(np);
	if (!res_cnt) {
		dev_info(dev, "%s no irqs specified\n", __func__);
		return 0;
	}
	res = devm_kzalloc(dev,  res_cnt * sizeof(*res), GFP_KERNEL);
	if (!res)
		return -ENOMEM;
	ret = of_irq_to_resource_table(np, res, res_cnt);
	pdata->irq_res = res;
	pdata->irq_res_cnt = ret;
	return 0;
}

/*
 * In some platform pr_info may spend too much time on printing debug message.
 * So we use this function to test the printk performance.
 * If your platform cannot not pass this check function, please config
 * PD_DBG_INFO, this will provide the threaded debug message for you.
 */
#if TCPC_ENABLE_ANYMSG
static void check_printk_performance(void)
{
	int i;
	u64 t1, t2;
	u32 nsrem;

#ifdef CONFIG_PD_DBG_INFO
	for (i = 0; i < 10; i++) {
		t1 = local_clock();
		pd_dbg_info("%d\n", i);
		t2 = local_clock();
		t2 -= t1;
		nsrem = do_div(t2, 1000000000);
		pd_dbg_info("pd_dbg_info : t2-t1 = %lu\n",
			    (unsigned long)nsrem / 1000);
	}
	for (i = 0; i < 10; i++) {
		t1 = local_clock();
		pr_info("%d\n", i);
		t2 = local_clock();
		t2 -= t1;
		nsrem = do_div(t2, 1000000000);
		pr_info("pr_info : t2-t1 = %lu\n", (unsigned long)nsrem / 1000);
	}
#else
	for (i = 0; i < 10; i++) {
		t1 = local_clock();
		pr_info("%d\n", i);
		t2 = local_clock();
		t2 -= t1;
		nsrem = do_div(t2, 1000000000);
		pr_info("t2-t1 = %lu\n", (unsigned long)nsrem /  1000);
		PD_BUG_ON(nsrem > 100*1000);
	}
#endif /* CONFIG_PD_DBG_INFO */
}
#endif /* TCPC_ENABLE_ANYMSG */

static int mt6360_tcpcdev_init(struct mt6360_chip *chip, struct device *dev)
{
	struct tcpc_desc *desc;
	struct device_node *np;
	u32 val, len;
	const char *name = "default";

	np = of_find_node_by_name(NULL, "type_c_port0");
	if (!np) {
		dev_err(dev, "%s find type_c_port0 fail\n", __func__);
		return -ENODEV;
	}

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	if (of_property_read_u32(np, "mt-tcpc,role_def", &val) >= 0) {
		if (val >= TYPEC_ROLE_NR)
			desc->role_def = TYPEC_ROLE_DRP;
		else
			desc->role_def = val;
	} else {
		dev_info(dev, "%s use default Role DRP\n", __func__);
		desc->role_def = TYPEC_ROLE_DRP;
	}

	if (of_property_read_u32(np, "mt-tcpc,notifier_supply_num",
				 &val) >= 0) {
		if (val < 0)
			desc->notifier_supply_num = 0;
		else
			desc->notifier_supply_num = val;
	} else
		desc->notifier_supply_num = 0;

	if (of_property_read_u32(np, "mt-tcpc,rp_level", &val) >= 0) {
		switch (val) {
		case 0: /* RP Default */
			desc->rp_lvl = TYPEC_CC_RP_DFT;
			break;
		case 1: /* RP 1.5V */
			desc->rp_lvl = TYPEC_CC_RP_1_5;
			break;
		case 2: /* RP 3.0V */
			desc->rp_lvl = TYPEC_CC_RP_3_0;
			break;
		default:
			break;
		}
	}

#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
	if (of_property_read_u32(np, "mt-tcpc,vconn_supply", &val) >= 0) {
		if (val >= TCPC_VCONN_SUPPLY_NR)
			desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
		else
			desc->vconn_supply = val;
	} else {
		dev_info(dev, "%s use default VconnSupply\n", __func__);
		desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
	}
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

	of_property_read_string(np, "mt-tcpc,name", (char const **)&name);
	len = strlen(name);
	desc->name = kzalloc(len + 1, GFP_KERNEL);
	if (!desc->name)
		return -ENOMEM;
	strlcpy((char *)desc->name, name, len + 1);

	chip->tcpc_desc = desc;
	chip->tcpc = tcpc_device_register(dev, desc, &mt6360_tcpc_ops, chip);
	if (IS_ERR(chip->tcpc))
		return -EINVAL;

	/* Init tcpc_flags */
	chip->tcpc->tcpc_flags = TCPC_FLAGS_LPM_WAKEUP_WATCHDOG |
				 TCPC_FLAGS_RETRY_CRC_DISCARD;
#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	chip->tcpc->tcpc_flags |= TCPC_FLAGS_RETRY_CRC_DISCARD;
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */
#ifdef CONFIG_USB_PD_REV30
	chip->tcpc->tcpc_flags |= TCPC_FLAGS_PD_REV30;
#endif	/* CONFIG_USB_PD_REV30 */

	if (chip->tcpc->tcpc_flags & TCPC_FLAGS_PD_REV30)
		dev_info(dev, "%s PD REV30\n", __func__);
	else
		dev_info(dev, "%s PD REV20\n", __func__);

	chip->tcpc->tcpc_flags |= TCPC_FLAGS_DISABLE_LEGACY;
	chip->tcpc->tcpc_flags |= TCPC_FLAGS_WATCHDOG_EN;
#ifdef CONFIG_MTK_TYPEC_WATER_DETECT_BY_PCB
	if (gpio_get_value(chip->pcb_gpio) == chip->pcb_gpio_polarity)
		chip->tcpc->tcpc_flags |= TCPC_FLAGS_WATER_DETECTION;
#else
	chip->tcpc->tcpc_flags |= TCPC_FLAGS_WATER_DETECTION;
#endif /* CONFIG_MTK_TYPEC_WATER_DETECT_BY_PCB */
	chip->tcpc->tcpc_flags |= TCPC_FLAGS_CABLE_TYPE_DETECTION;
	return 0;
}

static inline int mt6360_check_revision(struct i2c_client *client)
{
	int ret;
	u16 id;

	ret = i2c_smbus_read_i2c_block_data(client,
					    TCPC_V10_REG_VID, 2, (u8 *)&id);
	if (ret < 0) {
		dev_err(&client->dev, "%s read chip ID fail\n", __func__);
		return ret;
	}
	if (id != MEDIATEK_6360_VID) {
		dev_err(&client->dev, "%s check vid fail(0x%04X)\n", __func__,
			id);
		return -ENODEV;
	}

	ret = i2c_smbus_read_i2c_block_data(client,
					    TCPC_V10_REG_PID, 2, (u8 *)&id);
	if (ret < 0) {
		dev_err(&client->dev, "%s read product ID fail\n", __func__);
		return ret;
	}

	if (id != MEDIATEK_6360_PID) {
		dev_err(&client->dev, "%s check pid fail(0x%04X)\n", __func__,
			id);
		return -ENODEV;
	}

	ret = i2c_smbus_read_i2c_block_data(client,
					    TCPC_V10_REG_DID, 2, (u8 *)&id);
	if (ret < 0) {
		dev_err(&client->dev, "%s read device ID fail\n", __func__);
		return ret;
	}

	return id;
}

static int mt6360_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct mt6360_tcpc_platform_data *pdata =
		dev_get_platdata(&client->dev);
	bool use_dt = client->dev.of_node;
	struct mt6360_chip *chip;
	int ret, chip_id;

	pr_info("%s\n", __func__);
	ret = i2c_check_functionality(client->adapter,
				      I2C_FUNC_SMBUS_I2C_BLOCK |
				      I2C_FUNC_SMBUS_BYTE_DATA);
	pr_info("%s I2C functionality : %s\n", __func__, ret ? "ok" : "fail");

	chip_id = mt6360_check_revision(client);
	if (chip_id < 0)
		return chip_id;

#if TCPC_ENABLE_ANYMSG
	check_printk_performance();
#endif /* TCPC_ENABLE_ANYMSG */

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	if (use_dt) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		ret = mt6360_parse_dt(chip, &client->dev, pdata);
		if (ret < 0)
			return -EINVAL;
		client->dev.platform_data = pdata;
	} else {
		dev_err(&client->dev, "%s no dts node\n", __func__);
		return -ENODEV;
	}
	chip->chip_id = chip_id;
	chip->dev = &client->dev;
	chip->client = client;
	sema_init(&chip->io_lock, 1);
	sema_init(&chip->suspend_lock, 1);
	i2c_set_clientdata(client, chip);
	INIT_DELAYED_WORK(&chip->poll_work, mt6360_poll_work);
	chip->irq_wake_lock =
		wakeup_source_register("mt6360_irq_wakelock");
	chip->i2c_wake_lock =
		wakeup_source_register("mt6360_i2c_wakelock");

#ifdef CONFIG_WATER_DETECTION
	wakeup_source_init(&chip->wd_wakeup_src, "mt6360_wd_wakeup_src");
	atomic_set(&chip->wd_protect_rty, CONFIG_WD_PROTECT_RETRY_COUNT);
#ifdef CONFIG_WD_POLLING_ONLY
	INIT_DELAYED_WORK(&chip->usbid_poll_work, mt6360_usbid_poll_work);
#endif /* CONFIG_WD_POLLING_ONLY */
#endif /* CONFIG_WATER_DETECTION */
#ifdef CONFIG_WD_SBU_POLLING
	mutex_init(&chip->usbid_irq_lock);
	chip->usbid_irqen = true;
	INIT_WORK(&chip->wd_work, mt6360_wd_work);
#endif /* CONFIG_WD_SBU_POLLING */

	dev_info(chip->dev, "%s chipID = 0x%0X\n", __func__, chip->chip_id);

#ifdef CONFIG_RT_REGMAP
	ret = mt6360_regmap_init(chip);
	if (ret < 0) {
		dev_err(chip->dev, "%s regmap init fail(%d)\n", __func__, ret);
		return -EINVAL;
	}
#endif /* CONIFG_RT_REGMAP */

#if defined(CONFIG_WATER_DETECTION) || defined(CONFIG_CABLE_TYPE_DETECTION)
#if CONFIG_MTK_GAUGE_VERSION == 30
	chip->chgdev = get_charger_by_name("primary_chg");
	if (!chip->chgdev) {
		dev_err(chip->dev, "%s get charger device fail\n", __func__);
		ret = -EINVAL;
		goto err_tcpc_reg;
	}
#endif /* CONFIG_MTK_GAUGE_VERSION == 30 */
#endif /* CONFIG_WATER_DETECTION || CONFIG_CABLE_TYPE_DETECTION */

	ret = mt6360_tcpcdev_init(chip, &client->dev);
	if (ret < 0) {
		dev_err(chip->dev, "%s tcpc dev init fail\n", __func__);
		goto err_tcpc_reg;
	}

	/* Must init before sw reset */
	ret = mt6360_init_ctd(chip);
	if (ret < 0) {
		dev_err(chip->dev, "%s init ctd fail\n", __func__);
		goto err_tcpc_reg;
	}

	ret = mt6360_software_reset(chip->tcpc);
	if (ret < 0) {
		dev_err(chip->dev, "%s sw reset fail\n", __func__);
		goto err_sw_reset;
	}

	ret = mt6360_init_alert(chip->tcpc);
	if (ret < 0) {
		dev_err(chip->dev, "%s init alert fail\n", __func__);
		goto err_sw_reset;
	}

	tcpc_schedule_init_work(chip->tcpc);
#ifdef CONFIG_WATER_DETECTION
	mt6360_water_calibration(chip->tcpc);
#endif /* CONFIG_WATER_DETECTION */
	dev_info(chip->dev, "%s successfully!\n", __func__);
	return 0;

err_sw_reset:
	tcpc_device_unregister(chip->dev, chip->tcpc);
err_tcpc_reg:
#ifdef CONFIG_RT_REGMAP
	mt6360_regmap_deinit(chip);
#endif /* CONFIG_RT_REGMAP */
	return ret;
}

static int mt6360_i2c_remove(struct i2c_client *client)
{
	struct mt6360_chip *chip = i2c_get_clientdata(client);

	if (chip) {
		cancel_delayed_work_sync(&chip->poll_work);
#ifdef CONFIG_WD_SBU_POLLING
		cancel_work_sync(&chip->wd_work);
#ifdef CONFIG_WD_POLLING_ONLY
		cancel_delayed_work_sync(&chip->usbid_poll_work);
#endif /* CONFIG_WD_POLLING_ONLY */
#endif /* CONFIG_WD_SBU_POLLING */
		tcpc_device_unregister(chip->dev, chip->tcpc);
#ifdef CONFIG_RT_REGMAP
		mt6360_regmap_deinit(chip);
#endif /* CONFIG_RT_REGMAP */
	}

	return 0;
}

#ifdef CONFIG_PM
static int mt6360_i2c_suspend(struct device *dev)
{
	struct mt6360_chip *chip;
	struct i2c_client *client = to_i2c_client(dev);

	if (client) {
		chip = i2c_get_clientdata(client);
		if (chip)
			down(&chip->suspend_lock);
	}

	return 0;
}

static int mt6360_i2c_resume(struct device *dev)
{
	struct mt6360_chip *chip;
	struct i2c_client *client = to_i2c_client(dev);

	if (client) {
		chip = i2c_get_clientdata(client);
		if (chip)
			up(&chip->suspend_lock);
	}

	return 0;
}

static void mt6360_shutdown(struct i2c_client *client)
{
	struct mt6360_chip *chip = i2c_get_clientdata(client);

	/* Please reset IC here */
	if (chip) {
		if (chip->irq)
			disable_irq(chip->irq);
		tcpm_shutdown(chip->tcpc);
	} else
		i2c_smbus_write_byte_data(client, MT6360_REG_SWRESET, 0x01);
}

#ifdef CONFIG_PM_RUNTIME
static int mt6360_pm_suspend_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: suspending...\n");
	return 0;
}

static int mt6360_pm_resume_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: resuming...\n");
	return 0;
}
#endif /* CONFIG_PM_RUNTIME */

static const struct dev_pm_ops mt6360_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mt6360_i2c_suspend, mt6360_i2c_resume)
#ifdef CONFIG_PM_RUNTIME
	SET_RUNTIME_PM_OPS(mt6360_pm_suspend_runtime, mt6360_pm_resume_runtime,
			   NULL)
#endif /* CONFIG_PM_RUNTIME */
};
#define MT6360_PM_OPS	(&mt6360_pm_ops)
#else
#define MT6360_PM_OPS	(NULL)
#endif /* CONFIG_PM */

static const struct i2c_device_id mt6360_id_table[] = {
	{"mt6360", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, mt6360_id_table);

static const struct of_device_id mt_match_table[] = {
	{.compatible = "mediatek,usb_type_c",},
	{},
};

static struct i2c_driver mt6360_driver = {
	.driver = {
		.name = "usb_type_c",
		.owner = THIS_MODULE,
		.of_match_table = mt_match_table,
		.pm = MT6360_PM_OPS,
	},
	.probe = mt6360_i2c_probe,
	.remove = mt6360_i2c_remove,
	.shutdown = mt6360_shutdown,
	.id_table = mt6360_id_table,
};

static int __init mt6360_init(void)
{
	struct device_node *np;

	pr_info("%s (%s)\n", __func__, MT6360_DRV_VERSION);
	np = of_find_node_by_name(NULL, "usb_type_c");
	pr_info("%s usb_type_c node %s\n", __func__,
		np == NULL ? "not found" : "found");

	return i2c_add_driver(&mt6360_driver);
}
device_initcall_sync(mt6360_init);

static void __exit mt6360_exit(void)
{
	i2c_del_driver(&mt6360_driver);
}
module_exit(mt6360_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MT6360 TCPC Driver");
MODULE_VERSION(MT6360_DRV_VERSION);

/**** Release Note ****
 * 2.0.2_MTK
 *	(1) Add vendor defined irq handler
 *	(2) Remove init_cc_param
 *	(3) Add shield protection WD
 *	(4) Add update/set/clr bit
 *
 * 2.0.1_MTK
 *	First released PD3.0 Driver on MTK platform
 */
