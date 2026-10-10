/*
 * Copyright (C) 2020 Richtek Inc.
 *
 * Richtek RT1711H Type-C Port Control Driver
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
#include <uapi/linux/sched/types.h>
#include <linux/sched/clock.h>
#include <linux/sched/rt.h>

#include "inc/pd_dbg_info.h"
#include "inc/tcpci.h"
#include "inc/tcpc_ba41a.h"

#if IS_ENABLED(CONFIG_RT_REGMAP)
#include "inc/rt-regmap.h"
#endif /* CONFIG_RT_REGMAP */

/* #define DEBUG_GPIO	66 */
u8 reg9f=0;//uu+
//extern uint8_t flag_ba41;//uu+
//EXPORT_SYMBOL(flag_ba41);//uu639
uint8_t flag_ba41 = 0;

u8 g_type=0;//uu+
EXPORT_SYMBOL(g_type);//uu+

u8 flag_tx=0;//uu+
extern uint8_t flag_tx;//uu+
u8 flag_rx=0;//uu+
extern uint8_t flag_rx;//uu+

uint8_t g_buf_ba41_tx[33];   //uu668-
EXPORT_SYMBOL(g_buf_ba41_tx);  //uu668-

//uint8_t g_buf_ba41_rx[33];//uu665   //uu668-
//EXPORT_SYMBOL(g_buf_ba41_rx);//uu665  //uu668-

//uint8_t flag_rx_one;     //uu668
//EXPORT_SYMBOL(flag_rx_one);//uu665   //uu668-

//uint8_t flag_tx_one;  //uu668-
//EXPORT_SYMBOL(flag_tx_one);//uu665  //uu668-
extern int tcpc_schedule_dumpmsg_work(struct tcpc_device *tcpc);

#define RT1711H_DRV_VERSION	"2.0.6_G"

#define RT1711H_IRQ_WAKE_TIME	(500) /* ms */

#define HUSB311_DID	0x0000
#define PROBE_CNT_MAX	50
u8 reg9b=0;//uu+
EXPORT_SYMBOL(reg9b);//uu+
struct ba41_chip *g_tcpc_ba41a;
EXPORT_SYMBOL(g_tcpc_ba41a);

struct ba41_chip {
	struct i2c_client *client;
	struct device *dev;
#if IS_ENABLED(CONFIG_RT_REGMAP)
	struct rt_regmap_device *m_dev;
#endif /* CONFIG_RT_REGMAP */
	struct semaphore io_lock;
	struct semaphore suspend_lock;
	struct tcpc_desc *tcpc_desc;
	struct tcpc_device *tcpc;
	struct pinctrl *ba41a_pinctrl;
	struct kthread_worker irq_worker;
	struct kthread_work irq_work;
	struct task_struct *irq_worker_task;
	struct wakeup_source *irq_wake_lock;
	//struct delayed_work ba41a_dump_all_work;//uu731
	int irq_gpio;
	int irq;
	int chip_id;
	uint16_t chip_pid;
	uint16_t chip_vid;
};

#if IS_ENABLED(CONFIG_RT_REGMAP)
RT_REG_DECL(TCPC_V10_REG_VID, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_PID, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_DID, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_TYPEC_REV, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_PD_REV, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_PDIF_REV, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_ALERT, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_ALERT_MASK, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_POWER_STATUS_MASK, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_FAULT_STATUS_MASK, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_TCPC_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_ROLE_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_FAULT_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_POWER_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_CC_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_POWER_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_FAULT_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_COMMAND, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_MSG_HDR_INFO, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_RX_DETECT, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_RX_BYTE_CNT, 32, RT_VOLATILE, {});//uu616//648 32->1
RT_REG_DECL(TCPC_V10_REG_RX_BUF_FRAME_TYPE, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_HDR, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_DATA, 28, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TRANSMIT, 1, RT_VOLATILE, {});
//RT_REG_DECL(TCPC_V10_REG_TX_BYTE_CNT, 1, RT_NORMAL_WR_ONCE, {});
//RT_REG_DECL(TCPC_V10_REG_TX_HDR, 2, RT_NORMAL_WR_ONCE, {});
//RT_REG_DECL(TCPC_V10_REG_TX_DATA, 28, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_TX_BYTE_CNT, 32, RT_VOLATILE, {});//uu648 32->1
RT_REG_DECL(TCPC_V10_REG_TX_HDR, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TX_DATA, 28, RT_VOLATILE, {});
RT_REG_DECL(RT1711H_REG_CONFIG_GPIO0, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_PHY_CTRL1, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_CLK_CTRL2, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_CLK_CTRL3, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_PRL_FSM_RESET, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711H_REG_BMC_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711H_REG_BMCIO_RXDZSEL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_VCONN_CLIMITEN, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_RT_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711H_REG_RT_INT, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711H_REG_RT_MASK, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_IDLE_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_INTRST_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_WATCHDOG_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_I2CRST_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_SWRESET, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711H_REG_TTCPC_FILTER, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_DRP_TOGGLE_CYCLE, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_DRP_DUTY_CTRL, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_BMCIO_RXDZEN, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(RT1711H_REG_UNLOCK_PW_2, 2, RT_VOLATILE, {});
RT_REG_DECL(RT1711H_REG_EFUSE5, 1, RT_VOLATILE, {});

static const rt_register_map_t ba41_chip_regmap[] = {
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
	RT_REG(TCPC_V10_REG_TCPC_CTRL),
	RT_REG(TCPC_V10_REG_ROLE_CTRL),
	RT_REG(TCPC_V10_REG_FAULT_CTRL),
	RT_REG(TCPC_V10_REG_POWER_CTRL),
	RT_REG(TCPC_V10_REG_CC_STATUS),
	RT_REG(TCPC_V10_REG_POWER_STATUS),
	RT_REG(TCPC_V10_REG_FAULT_STATUS),
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
	RT_REG(RT1711H_REG_CONFIG_GPIO0),
	RT_REG(RT1711H_REG_PHY_CTRL1),
	RT_REG(RT1711H_REG_CLK_CTRL2),
	RT_REG(RT1711H_REG_CLK_CTRL3),
	RT_REG(RT1711H_REG_PRL_FSM_RESET),
	RT_REG(RT1711H_REG_BMC_CTRL),
	RT_REG(RT1711H_REG_BMCIO_RXDZSEL),
	RT_REG(RT1711H_REG_VCONN_CLIMITEN),
	RT_REG(RT1711H_REG_RT_STATUS),
	RT_REG(RT1711H_REG_RT_INT),
	RT_REG(RT1711H_REG_RT_MASK),
	RT_REG(RT1711H_REG_IDLE_CTRL),
	RT_REG(RT1711H_REG_INTRST_CTRL),
	RT_REG(RT1711H_REG_WATCHDOG_CTRL),
	RT_REG(RT1711H_REG_I2CRST_CTRL),
	RT_REG(RT1711H_REG_SWRESET),
	RT_REG(RT1711H_REG_TTCPC_FILTER),
	RT_REG(RT1711H_REG_DRP_TOGGLE_CYCLE),
	RT_REG(RT1711H_REG_DRP_DUTY_CTRL),
	RT_REG(RT1711H_REG_BMCIO_RXDZEN),
	RT_REG(RT1711H_REG_UNLOCK_PW_2),
	RT_REG(RT1711H_REG_EFUSE5),
};
#define RT1711_CHIP_REGMAP_SIZE ARRAY_SIZE(ba41_chip_regmap)
#if 0
	if(ba41_chip_regmap[i] == RT_REG_DECL(TCPC_V10_REG_TX_HDR, 2, RT_VOLATILE, {}))
		ba41_chip_regmap[i] = RT_REG_DECL(SY_TCPC_V10_REG_TX_BYTE_CNT, 2, RT_VOLATILE, {})
	RT_REG_DECL(SY_TCPC_V10_REG_TX_DATA, 28, RT_VOLATILE, {});
#endif

#endif /* CONFIG_RT_REGMAP */



static int rt1711_read_device(void *client, u32 reg, int len, void *dst)
{
	struct i2c_client *i2c = client;
	int ret = 0, count = 5;
	u64 t1 = 0, t2 = 0;

	while (1) {
		t1 = local_clock();
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, len, dst);
		t2 = local_clock();
		RT1711_INFO("%s del = %lluus, reg = %02X, len = %d\n",
			    __func__, (t2 - t1) / NSEC_PER_USEC, reg, len);
		if (ret < 0 && count > 1)
			count--;
		else
			break;
		udelay(100);
	}
	return ret;
}

static int rt1711_write_device(void *client, u32 reg, int len, const void *src)
{
	struct i2c_client *i2c = client;
	int ret = 0, count = 5;
	u64 t1 = 0, t2 = 0;

	while (1) {
		t1 = local_clock();
		ret = i2c_smbus_write_i2c_block_data(i2c, reg, len, src);
		t2 = local_clock();
		RT1711_INFO("%s del = %lluus, reg = %02X, len = %d\n",
			    __func__, (t2 - t1) / NSEC_PER_USEC, reg, len);
		if (ret < 0 && count > 1)
			count--;
		else
			break;
		udelay(100);
	}
	return ret;
}

static int rt1711_reg_read(struct i2c_client *i2c, u8 reg)
{
	struct ba41_chip *chip = i2c_get_clientdata(i2c);
	u8 val = 0;
	int ret = 0;

#if IS_ENABLED(CONFIG_RT_REGMAP)
	ret = rt_regmap_block_read(chip->m_dev, reg, 1, &val);
#else
	ret = rt1711_read_device(chip->client, reg, 1, &val);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0) {
		dev_err(chip->dev, "ba41 reg read fail\n");
		return ret;
	}
	return val;
}

static int rt1711_reg_write(struct i2c_client *i2c, u8 reg, const u8 data)
{
	struct ba41_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;

#if IS_ENABLED(CONFIG_RT_REGMAP)
	ret = rt_regmap_block_write(chip->m_dev, reg, 1, &data);
#else
	ret = rt1711_write_device(chip->client, reg, 1, &data);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0)
		dev_err(chip->dev, "ba41 reg write fail\n");
	return ret;
}

static int rt1711_block_read(struct i2c_client *i2c,
			u8 reg, int len, void *dst)
{
	struct ba41_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;
#if IS_ENABLED(CONFIG_RT_REGMAP)
	ret = rt_regmap_block_read(chip->m_dev, reg, len, dst);
#else
	ret = rt1711_read_device(chip->client, reg, len, dst);
#endif /* #if IS_ENABLED(CONFIG_RT_REGMAP) */
	if (ret < 0)
		dev_err(chip->dev, "ba41 block read fail\n");
	return ret;
}

static int rt1711_block_write(struct i2c_client *i2c,
			u8 reg, int len, const void *src)
{
	struct ba41_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;
#if IS_ENABLED(CONFIG_RT_REGMAP)
	ret = rt_regmap_block_write(chip->m_dev, reg, len, src);
#else
	ret = rt1711_write_device(chip->client, reg, len, src);
#endif /* #if IS_ENABLED(CONFIG_RT_REGMAP) */
	if (ret < 0)
		dev_err(chip->dev, "ba41 block write fail\n");
	return ret;
}

static int32_t rt1711_write_word(struct i2c_client *client,
					uint8_t reg_addr, uint16_t data)
{
	int ret;

	/* don't need swap */
	ret = rt1711_block_write(client, reg_addr, 2, (uint8_t *)&data);
	return ret;
}

static int32_t rt1711_read_word(struct i2c_client *client,
					uint8_t reg_addr, uint16_t *data)
{
	int ret;

	/* don't need swap */
	ret = rt1711_block_read(client, reg_addr, 2, (uint8_t *)data);
	return ret;
}

static inline int rt1711_i2c_write8(
	struct tcpc_device *tcpc, u8 reg, const u8 data)
{
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);

	return rt1711_reg_write(chip->client, reg, data);
}

static inline int rt1711_i2c_write16(
		struct tcpc_device *tcpc, u8 reg, const u16 data)
{
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);

	return rt1711_write_word(chip->client, reg, data);
}

static inline int rt1711_i2c_read8(struct tcpc_device *tcpc, u8 reg)
{
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);

	return rt1711_reg_read(chip->client, reg);
}

static inline int rt1711_i2c_read16(
	struct tcpc_device *tcpc, u8 reg)
{
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);
	u16 data;
	int ret;

	ret = rt1711_read_word(chip->client, reg, &data);
	if (ret < 0)
		return ret;
	return data;
}

int ba41_bg_ctrl(struct tcpc_device *tcpc, bool enable)
{
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);
	uint8_t data = rt1711_i2c_read8(tcpc, RT1711H_REG_BMC_CTRL);
	if(enable) {
		data |= BIT(2);
	} else {
		data &=~(BIT(2));
	}
	dev_err(chip->dev, "ba41 : FORCE_BG : %d \n",enable);
	return rt1711_i2c_write8(tcpc, RT1711H_REG_BMC_CTRL, data);
}

#if IS_ENABLED(CONFIG_RT_REGMAP)
static struct rt_regmap_fops rt1711_regmap_fops = {
	.read_device = rt1711_read_device,
	.write_device = rt1711_write_device,
};
#endif /* CONFIG_RT_REGMAP */

static int rt1711_regmap_init(struct ba41_chip *chip)
{
#if IS_ENABLED(CONFIG_RT_REGMAP)
	struct rt_regmap_properties *props;
	char name[32];
	int len;

	props = devm_kzalloc(chip->dev, sizeof(*props), GFP_KERNEL);
	if (!props)
		return -ENOMEM;

	props->register_num = RT1711_CHIP_REGMAP_SIZE;
	props->rm = ba41_chip_regmap;

	props->rt_regmap_mode = RT_MULTI_BYTE |
				RT_IO_PASS_THROUGH | RT_DBG_SPECIAL;
	snprintf(name, sizeof(name), "ba41-%02x", chip->client->addr);

	len = strlen(name);
	props->name = kzalloc(len+1, GFP_KERNEL);
	props->aliases = kzalloc(len+1, GFP_KERNEL);

	if ((!props->name) || (!props->aliases))
		return -ENOMEM;

	strlcpy((char *)props->name, name, len+1);
	strlcpy((char *)props->aliases, name, len+1);
	props->io_log_en = 0;

	chip->m_dev = rt_regmap_device_register(props,
			&rt1711_regmap_fops, chip->dev, chip->client, chip);
	if (!chip->m_dev) {
		dev_err(chip->dev, "ba41 chip rt_regmap register fail\n");
		return -EINVAL;
	}
#endif
	return 0;
}

static int rt1711_regmap_deinit(struct ba41_chip *chip)
{
#if IS_ENABLED(CONFIG_RT_REGMAP)
	rt_regmap_device_unregister(chip->m_dev);
#endif
	return 0;
}

static inline int rt1711_software_reset(struct tcpc_device *tcpc)
{
	int ret = rt1711_i2c_write8(tcpc, RT1711H_REG_SWRESET, 1);
#if IS_ENABLED(CONFIG_RT_REGMAP)
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);
#endif /* CONFIG_RT_REGMAP */

	if (ret < 0)
		return ret;
#if IS_ENABLED(CONFIG_RT_REGMAP)
	rt_regmap_cache_reload(chip->m_dev);
#endif /* CONFIG_RT_REGMAP */
	usleep_range(1000, 2000);
	return 0;
}

static inline int rt1711_command(struct tcpc_device *tcpc, uint8_t cmd)
{
	return rt1711_i2c_write8(tcpc, TCPC_V10_REG_COMMAND, cmd);
}

static int rt1711_init_vbus_cal(struct tcpc_device *tcpc)
{
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);
	const u8 val_en_test_mode[] = {0x86, 0x62};
	const u8 val_dis_test_mode[] = {0x00, 0x00};
	int ret = 0;
	u8 data = 0;
	s8 cal = 0;

	ret = rt1711_block_write(chip->client, RT1711H_REG_UNLOCK_PW_2,
			ARRAY_SIZE(val_en_test_mode), val_en_test_mode);
	if (ret < 0)
		dev_notice(chip->dev, "%s en test mode fail(%d)\n",
				__func__, ret);

	ret = rt1711_reg_read(chip->client, RT1711H_REG_EFUSE5);
	if (ret < 0)
		goto out;

	data = ret;
	data = (data & RT1711H_REG_M_VBUS_CAL) >> RT1711H_REG_S_VBUS_CAL;
	cal = (data & BIT(2)) ? (data | GENMASK(7, 3)) : data;
	cal -= 2;
	if (cal < RT1711H_REG_MIN_VBUS_CAL)
		cal = RT1711H_REG_MIN_VBUS_CAL;
	data = (cal << RT1711H_REG_S_VBUS_CAL) | (ret & GENMASK(4, 0));

	ret = rt1711_reg_write(chip->client, RT1711H_REG_EFUSE5, data);
out:
	ret = rt1711_block_write(chip->client, RT1711H_REG_UNLOCK_PW_2,
			ARRAY_SIZE(val_dis_test_mode), val_dis_test_mode);
	if (ret < 0)
		dev_notice(chip->dev, "%s dis test mode fail(%d)\n",
				__func__, ret);

	return ret;
}

static int rt1711_init_alert_mask(struct tcpc_device *tcpc)
{
	uint16_t mask;
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);

	mask = TCPC_V10_REG_ALERT_CC_STATUS | TCPC_V10_REG_ALERT_POWER_STATUS;

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	/* Need to handle RX overflow */
	mask |= TCPC_V10_REG_ALERT_TX_SUCCESS | TCPC_V10_REG_ALERT_TX_DISCARDED
			| TCPC_V10_REG_ALERT_TX_FAILED
			| TCPC_V10_REG_ALERT_RX_HARD_RST
			| TCPC_V10_REG_ALERT_RX_STATUS
			| TCPC_V10_REG_RX_OVERFLOW;
#endif /* CONFIG_USB_POWER_DELIVERY */

	mask |= TCPC_REG_ALERT_FAULT;

	return rt1711_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, mask);
}

static int rt1711_init_power_status_mask(struct tcpc_device *tcpc)
{
	const uint8_t mask = TCPC_V10_REG_POWER_STATUS_VBUS_PRES;

	return rt1711_i2c_write8(tcpc,
			TCPC_V10_REG_POWER_STATUS_MASK, mask);
}
static int rt1711_init_fault_mask_ba41(struct tcpc_device *tcpc)//uu615+ need review
{
const uint8_t mask =
		TCPC_V10_REG_FAULT_STATUS_VCONN_OV |
		TCPC_V10_REG_FAULT_STATUS_I2C_ERROR|//uu317+ i2c err can not be masked will cause request message can not be transmit out :cause vbus of pd reset 
		TCPC_V10_REG_FAULT_STATUS_VCONN_OC;
	return rt1711_i2c_write8(tcpc,
			TCPC_V10_REG_FAULT_STATUS_MASK, mask);
}

static int rt1711_init_fault_mask(struct tcpc_device *tcpc)
{

const uint8_t mask =
		TCPC_V10_REG_FAULT_STATUS_VCONN_OV |
		TCPC_V10_REG_FAULT_STATUS_VCONN_OC;
	return rt1711_i2c_write8(tcpc,
			TCPC_V10_REG_FAULT_STATUS_MASK, mask);
}

static int rt1711_init_rt_mask(struct tcpc_device *tcpc)
{
	uint8_t rt_mask = 0;

	/* CONFIG_TCPC_VSAFE0V_DETECT_IC */
	rt_mask |= RT1711H_REG_M_VBUS_80;

#ifdef CONFIG_TYPEC_CAP_RA_DETACH
	if (tcpc->tcpc_flags & TCPC_FLAGS_CHECK_RA_DETACH)
		rt_mask |= RT1711H_REG_M_RA_DETACH;
#endif /* CONFIG_TYPEC_CAP_RA_DETACH */

#ifdef CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG
	if (tcpc->tcpc_flags & TCPC_FLAGS_LPM_WAKEUP_WATCHDOG)
		rt_mask |= RT1711H_REG_M_WAKEUP;
#endif	/* CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG */

	return rt1711_i2c_write8(tcpc, RT1711H_REG_RT_MASK, rt_mask);
}

static void rt1711_irq_work_handler(struct kthread_work *work)
{
	struct ba41_chip *chip =
			container_of(work, struct ba41_chip, irq_work);
	int regval = 0;
	int gpio_val;

	/* make sure I2C bus had resumed */
	down(&chip->suspend_lock);
	tcpci_lock_typec(chip->tcpc);

	do {
		regval = tcpci_alert(chip->tcpc);
		if (regval)
			break;
		gpio_val = gpio_get_value(chip->irq_gpio);
	} while (gpio_val == 0);

	tcpci_unlock_typec(chip->tcpc);
	up(&chip->suspend_lock);

}

static irqreturn_t rt1711_intr_handler(int irq, void *data)
{
	struct ba41_chip *chip = data;

	//pm_wakeup_event(chip->dev, RT1711H_IRQ_WAKE_TIME);

	//tcpci_lock_typec(chip->tcpc);
	//tcpci_alert(chip->tcpc);
	//tcpci_unlock_typec(chip->tcpc);

	__pm_wakeup_event(chip->irq_wake_lock, RT1711H_IRQ_WAKE_TIME);

	kthread_queue_work(&chip->irq_worker, &chip->irq_work);

	return IRQ_HANDLED;
}

static int rt1711_init_alert(struct tcpc_device *tcpc)
{
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
	int ret;
	char *name = NULL;
	int len;
	int data = 0;

	/* BMCIO_LPEN write to 0 */
	data = rt1711_i2c_read8(chip->tcpc, RT1711H_REG_BMC_CTRL);
	data &= ~RT1711H_REG_BMCIO_LPEN;
	rt1711_i2c_write8(chip->tcpc, RT1711H_REG_BMC_CTRL, data);

	rt1711_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, 0);
	rt1711_write_word(chip->client, TCPC_V10_REG_ALERT, 0xffff);
	rt1711_reg_write(chip->client, RT1711H_REG_RT_INT, 0xff);//uu687

	len = strlen(chip->tcpc_desc->name);
	name = devm_kzalloc(chip->dev, len+5, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	snprintf(name, PAGE_SIZE, "%s-IRQ", chip->tcpc_desc->name);

	pr_info("%s sy6976 name = %s, gpio = %d\n", __func__,
				chip->tcpc_desc->name, chip->irq_gpio);

	ret = devm_gpio_request(chip->dev, chip->irq_gpio, name);
	if (ret < 0) {
		pr_err("Error: failed to request GPIO%d (ret = %d)\n",
		chip->irq_gpio, ret);
		goto init_alert_err;
	}

	ret = gpio_direction_input(chip->irq_gpio);
	if (ret < 0) {
		pr_err("Error: failed to set GPIO%d as input pin(ret = %d)\n",
		chip->irq_gpio, ret);
		goto init_alert_err;
	}

	chip->irq = gpio_to_irq(chip->irq_gpio);
	if (chip->irq <= 0) {
		pr_err("%s gpio to irq fail, chip->irq(%d)\n",
						__func__, chip->irq);
		goto init_alert_err;
	}

	pr_info("%s :ba41_chip IRQ number = %d\n", __func__, chip->irq);

	kthread_init_worker(&chip->irq_worker);
	chip->irq_worker_task = kthread_run(kthread_worker_fn,
			&chip->irq_worker, "%s", chip->tcpc_desc->name);
	if (IS_ERR(chip->irq_worker_task)) {
		pr_err("Error: Could not create tcpc task\n");
		goto init_alert_err;
	}

	sched_setscheduler(chip->irq_worker_task, SCHED_FIFO, &param);
	kthread_init_work(&chip->irq_work, rt1711_irq_work_handler);

	pr_info("IRQF_NO_THREAD Test\n");
	ret = request_irq(chip->irq, rt1711_intr_handler,
		//SY6976 IRQF_TRIGGER_LOW | IRQF_ONESHOT, name, chip);
		IRQF_TRIGGER_FALLING | IRQF_NO_THREAD, name, chip);
	if (ret < 0) {
		pr_err("Error: failed to request irq%d (gpio = %d, ret = %d)\n",
			chip->irq, chip->irq_gpio, ret);
		goto init_alert_err;
	}

	enable_irq_wake(chip->irq);
	return 0;
init_alert_err:
	return -EINVAL;
}


int rt1711_alert_status_clear(struct tcpc_device *tcpc, uint32_t mask)
{
	int ret;
	uint16_t mask_t1;

#if CONFIG_TCPC_VSAFE0V_DETECT_IC
	uint8_t mask_t2;
#endif

	/* Write 1 clear */
	mask_t1 = (uint16_t) mask;
	if (mask_t1) {
		ret = rt1711_i2c_write16(tcpc, TCPC_V10_REG_ALERT, mask_t1);
		if (ret < 0)
			return ret;
		rt1711_i2c_write8(tcpc, RT1711H_REG_RT_INT, 0xff);//uu687
	}

#if CONFIG_TCPC_VSAFE0V_DETECT_IC
	mask_t2 = mask >> 16;
	if (mask_t2) {
		ret = rt1711_i2c_write8(tcpc, RT1711H_REG_RT_INT, mask_t2);
		if (ret < 0)
			return ret;
	}
#endif

	return 0;
}

static int rt1711h_set_clock_gating(struct tcpc_device *tcpc, bool en)
{
	int ret = 0;

#ifdef CONFIG_TCPC_CLOCK_GATING
	int i = 0;
	uint8_t clk2 = RT1711H_REG_CLK_DIV_600K_EN
		| RT1711H_REG_CLK_DIV_300K_EN | RT1711H_REG_CLK_CK_300K_EN;
	uint8_t clk3 = RT1711H_REG_CLK_DIV_2P4M_EN;

	if (!en) {
		clk2 |=
			RT1711H_REG_CLK_BCLK2_EN | RT1711H_REG_CLK_BCLK_EN;
		clk3 |=
			RT1711H_REG_CLK_CK_24M_EN | RT1711H_REG_CLK_PCLK_EN;
	}

	if (en) {
		for (i = 0; i < 2; i++)
			ret = rt1711_alert_status_clear(tcpc,
				TCPC_REG_ALERT_RX_ALL_MASK);
	}

	if (ret == 0)
		ret = rt1711_i2c_write8(tcpc, RT1711H_REG_CLK_CTRL2, clk2);
	if (ret == 0)
		ret = rt1711_i2c_write8(tcpc, RT1711H_REG_CLK_CTRL3, clk3);
#endif	/* CONFIG_TCPC_CLOCK_GATING */

	return ret;
}

static inline int rt1711h_init_cc_params(
			struct tcpc_device *tcpc, uint8_t cc_res)
{
	int rv = 0;

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#if 0 /*CONFIG_USB_PD_SNK_DFT_NO_GOOD_CRC*/
	uint8_t en, sel;
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);

	if (cc_res == TYPEC_CC_VOLT_SNK_DFT) {	/* 0.55 */
		en = 0;
		sel = 0x81;
	} else if (chip->chip_id >= RT1715_DID_D) {	/* 0.35 & 0.75 */
		en = 1;
		sel = 0x81;
	} else {	/* 0.4 & 0.7 */
		en = 1;
		sel = 0x80;
	}

	rv = rt1711_i2c_write8(tcpc, RT1711H_REG_BMCIO_RXDZEN, en);
	if (rv == 0)
		rv = rt1711_i2c_write8(tcpc, RT1711H_REG_BMCIO_RXDZSEL, sel);
#endif	/* CONFIG_USB_PD_SNK_DFT_NO_GOOD_CRC */
#endif	/* CONFIG_USB_POWER_DELIVERY */

	return rv;
}

static int rt1711_tcpc_init(struct tcpc_device *tcpc, bool sw_reset)
{
	int ret;
	bool retry_discard_old = false;
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);

	RT1711_INFO("\n");

	if (sw_reset) {
		ret = rt1711_software_reset(tcpc);
		if (ret < 0)
			return ret;
	}

#ifdef CONFIG_TCPC_I2CRST_EN
	if (chip->chip_id == HUSB311_DID) {
		rt1711_i2c_write8(tcpc, RT1711H_REG_I2CRST_CTRL, 0x08);
	} else {
		rt1711_i2c_write8(tcpc,
			RT1711H_REG_I2CRST_CTRL,
			RT1711H_REG_I2CRST_SET(true, 0x0f));
	}
#endif	/* CONFIG_TCPC_I2CRST_EN */

	/* UFP Both RD setting */
	/* DRP = 0, RpVal = 0 (Default), Rd, Rd */
	rt1711_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL,
		TCPC_V10_REG_ROLE_CTRL_RES_SET(0, 0, CC_RD, CC_RD));

	if (chip->chip_id == RT1711H_DID_A) {
		rt1711_i2c_write8(tcpc, TCPC_V10_REG_FAULT_CTRL,
			TCPC_V10_REG_FAULT_CTRL_DIS_VCONN_OV);
	} else if (chip->chip_id == SY6976_DID_A) {
		rt1711_i2c_write8(tcpc, TCPC_V10_REG_COMMAND,
			TCPM_CMD_ENABLE_VBUS_DETECT);
	}

	/*
	 * CC Detect Debounce : 26.7*val us
	 * Transition window count : spec 12~20us, based on 2.4MHz
	 * DRP Toggle Cycle : 51.2 + 6.4*val ms
	 * DRP Duyt Ctrl : dcSRC: /1024
	 */

	rt1711_i2c_write8(tcpc, RT1711H_REG_TTCPC_FILTER, 10);
	rt1711_i2c_write8(tcpc, RT1711H_REG_DRP_TOGGLE_CYCLE, 4);
	rt1711_i2c_write16(tcpc,
		RT1711H_REG_DRP_DUTY_CTRL, TCPC_NORMAL_RP_DUTY);

	/* Vconn OC */
	rt1711_i2c_write8(tcpc, RT1711H_REG_VCONN_CLIMITEN, 1);

	/* RX/TX Clock Gating (Auto Mode)*/
	if (!sw_reset)
		rt1711h_set_clock_gating(tcpc, true);

	if (!(tcpc->tcpc_flags & TCPC_FLAGS_RETRY_CRC_DISCARD))
		retry_discard_old = true;

	rt1711_i2c_write8(tcpc, RT1711H_REG_CONFIG_GPIO0, 0x80);

	/* For BIST, Change Transition Toggle Counter (Noise) from 3 to 7 */
	rt1711_i2c_write8(tcpc, RT1711H_REG_PHY_CTRL1,
		RT1711H_REG_PHY_CTRL1_SET(retry_discard_old, 7, 0, 1));

	rt1711_alert_status_clear(tcpc, 0xffffffff);

	rt1711_init_vbus_cal(tcpc);
	rt1711_init_power_status_mask(tcpc);
	rt1711_init_alert_mask(tcpc);
	if(flag_ba41==1){
	rt1711_init_fault_mask_ba41(tcpc);//uu615+ need review
	}else{
	rt1711_init_fault_mask(tcpc);
	}
	rt1711_init_rt_mask(tcpc);

	/* CK_300K from 320K, SHIPPING off, AUTOIDLE enable, TIMEOUT = 6.4ms */
	/* BIT(6):Dual_Ra_Detect_EN write to 1 */
	rt1711_i2c_write8(tcpc, RT1711H_REG_IDLE_CTRL,
		RT1711H_REG_IDLE_SET(0, 1, 1, 0) | BIT(6));
	mdelay(1);

	return 0;
}

static inline int rt1711_fault_status_vconn_ov(struct tcpc_device *tcpc)
{
	int ret;

	ret = rt1711_i2c_read8(tcpc, RT1711H_REG_BMC_CTRL);
	if (ret < 0)
		return ret;

	ret &= ~RT1711H_REG_DISCHARGE_EN;
	return rt1711_i2c_write8(tcpc, RT1711H_REG_BMC_CTRL, ret);
}

int rt1711_fault_status_clear(struct tcpc_device *tcpc, uint8_t status)
{
	int ret;

	if (status & TCPC_V10_REG_FAULT_STATUS_VCONN_OV)
		ret = rt1711_fault_status_vconn_ov(tcpc);

	rt1711_i2c_write8(tcpc, TCPC_V10_REG_FAULT_STATUS, status);
	return 0;
}

int rt1711_get_alert_mask(struct tcpc_device *tcpc, uint32_t *mask)
{
	int ret;
#if CONFIG_TCPC_VSAFE0V_DETECT_IC
	uint8_t v2;
#endif

	ret = rt1711_i2c_read16(tcpc, TCPC_V10_REG_ALERT_MASK);
	if (ret < 0)
		return ret;

	*mask = (uint16_t) ret;

#if CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = rt1711_i2c_read8(tcpc, RT1711H_REG_RT_MASK);
	if (ret < 0)
		return ret;

	v2 = (uint8_t) ret;
	*mask |= v2 << 16;
#endif

	return 0;
}

int rt1711_get_alert_status(struct tcpc_device *tcpc, uint32_t *alert)
{
	int ret;
#if CONFIG_TCPC_VSAFE0V_DETECT_IC
	uint8_t v2;
#endif

	ret = rt1711_i2c_read16(tcpc, TCPC_V10_REG_ALERT);
	if (ret < 0)
		return ret;

	*alert = (uint16_t) ret;

#if CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = rt1711_i2c_read8(tcpc, RT1711H_REG_RT_INT);
	if (ret < 0)
		return ret;

	v2 = (uint8_t) ret;
	*alert |= v2 << 16;
#endif

	return 0;
}

//uu736
static bool pd_cc_is_rd(struct tcpc_device *tcpc)
{
	int val = 0;
	val = rt1711_i2c_read8(tcpc, 0x1A);
	if (((val & 0x03) == 0x02) || (((val >> 2) & 0x03) == 0x02)) {
		return true;
	}

	return false;
}

static int rt1711_get_fake_power_status(struct tcpc_device *tcpc, uint16_t *pwr_status)	//gw20240709
{
	int ret;
	int val = 0;

	ret = rt1711_i2c_read8(tcpc, TCPC_V10_REG_POWER_STATUS);
	if (ret < 0)
		return ret;
	*pwr_status = 0;
	val = rt1711_i2c_read8(tcpc, 0x9B);	//uu730
	if (ret & TCPC_V10_REG_POWER_STATUS_VBUS_PRES) {
		*pwr_status |= TCPC_REG_POWER_STATUS_VBUS_PRES;
		if (pd_cc_is_rd(tcpc)) {
			val &= ~BIT(3);
			rt1711_i2c_write8(tcpc, 0x9B, val);	//uu730//uu736
		}
	} else {
		//VBUS_PRESENT=0
		if((tcpc->typec_state == 6)&& !((tcpc->typec_during_role_swap)||(tcpc->pd_wait_pr_swap_complete)||(tcpc->pd_wait_hard_reset_complete)))
		{
			pr_err("FAKE VBUS_5V");
			*pwr_status |= TCPC_REG_POWER_STATUS_VBUS_PRES;
		}
		else
		{
			val |= BIT(3);
			rt1711_i2c_write8(tcpc, 0x9B, val);//uu730
			pr_err("TRUE NO_VBUS");
		}
	}
#if CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = rt1711_i2c_read8(tcpc, RT1711H_REG_RT_STATUS);
	if (ret < 0)
		return ret;
	if (ret & RT1711H_REG_VBUS_80)
	{
		*pwr_status |= TCPC_REG_POWER_STATUS_EXT_VSAFE0V;
		pr_err("3_7_TRUE VSAFE_0V");
	}
#endif
	return 0;
}

static int rt1711_get_power_status(
		struct tcpc_device *tcpc, uint16_t *pwr_status)
{
	int ret;
	int val = 0;

	ret = rt1711_i2c_read8(tcpc, TCPC_V10_REG_POWER_STATUS);
	if (ret < 0)
		return ret;

	*pwr_status = 0;
	val = rt1711_i2c_read8(tcpc, 0x9B);//uu730

	if (ret & TCPC_V10_REG_POWER_STATUS_VBUS_PRES) {
		*pwr_status |= TCPC_REG_POWER_STATUS_VBUS_PRES;
		if (pd_cc_is_rd(tcpc)) {
			val &= ~BIT(3);
			rt1711_i2c_write8(tcpc, 0x9B, val);//uu730//uu736
		}
	} else {
		val |= BIT(3);
		rt1711_i2c_write8(tcpc, 0x9B, val);//uu730
	}

#if CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = rt1711_i2c_read8(tcpc, RT1711H_REG_RT_STATUS);
	if (ret < 0)
		return ret;

	if (ret & RT1711H_REG_VBUS_80)
		*pwr_status |= TCPC_REG_POWER_STATUS_EXT_VSAFE0V;
#endif
	return 0;
}

int rt1711_get_fault_status(struct tcpc_device *tcpc, uint8_t *status)
{
	int ret;

	ret = rt1711_i2c_read8(tcpc, TCPC_V10_REG_FAULT_STATUS);
	if (ret < 0)
		return ret;
	*status = (uint8_t) ret;
	return 0;
}

static int rt1711_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2)
{
	int status, role_ctrl, cc_role;
	bool act_as_sink, act_as_drp;
	int pre_cc1, pre_cc2 = 15;

	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);

	status = rt1711_i2c_read8(tcpc, TCPC_V10_REG_CC_STATUS);
	if (status < 0)
		return status;

	role_ctrl = rt1711_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL);
	if (role_ctrl < 0)
		return role_ctrl;

	if (status & TCPC_V10_REG_CC_STATUS_DRP_TOGGLING) {
		*cc1 = TYPEC_CC_DRP_TOGGLING;
		*cc2 = TYPEC_CC_DRP_TOGGLING;
		return 0;
	}

	*cc1 = TCPC_V10_REG_CC_STATUS_CC1(status);
	*cc2 = TCPC_V10_REG_CC_STATUS_CC2(status);

	act_as_drp = TCPC_V10_REG_ROLE_CTRL_DRP & role_ctrl;

	if (act_as_drp) {
		act_as_sink = TCPC_V10_REG_CC_STATUS_DRP_RESULT(status);
	} else {
		if (tcpc->typec_polarity)
			cc_role = TCPC_V10_REG_CC_STATUS_CC2(role_ctrl);
		else
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

	if (chip->chip_id == SY6976_DID_A && act_as_drp &&
			act_as_sink) {
		if ((*cc1 + *cc2) > (2 * TYPEC_CC_VOLT_RA)) {
			if (*cc1 == TYPEC_CC_VOLT_RA)
				*cc1 = TYPEC_CC_VOLT_OPEN;
			if (*cc2 == TYPEC_CC_VOLT_RA)
				*cc2 = TYPEC_CC_VOLT_OPEN;
		}
	}

	if (*cc1 != TYPEC_CC_VOLT_OPEN)
		*cc1 |= (act_as_sink << 2);

	if (*cc2 != TYPEC_CC_VOLT_OPEN)
		*cc2 |= (act_as_sink << 2);

	pr_info("%s pre cc:%d %d curr cc:%d %d\n", __func__,
		tcpc->typec_remote_cc[0], tcpc->typec_remote_cc[1], *cc1, *cc2);

	pre_cc1 = tcpc->typec_remote_cc[0];
	pre_cc2 = tcpc->typec_remote_cc[1];

	if (pre_cc1 + pre_cc2 >= 5 && pre_cc1 + pre_cc2 <= 7) {
		if (*cc1+*cc2 >= 12 && *cc1+*cc2 < 15 ) {
			*cc1 = *cc1 & pre_cc1 ? *cc1 : 0;
			*cc2 = *cc2 & pre_cc2 ? *cc2 : 0;
		}
	}

	rt1711h_init_cc_params(tcpc,
		(uint8_t)tcpc->typec_polarity ? *cc2 : *cc1);

	return 0;
}

#if CONFIG_TCPC_VSAFE0V_DETECT_IC
static int rt1711_enable_vsafe0v_detect(
	struct tcpc_device *tcpc, bool enable)
{
	int ret = 0;
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);

	if (chip->chip_id == SY6976_DID_A) {
		enable = true;
	}
	ret = rt1711_i2c_read8(tcpc, RT1711H_REG_RT_MASK);

	if (ret < 0)
		return ret;

	if (enable)
		ret |= RT1711H_REG_M_VBUS_80;
	else
		ret &= ~RT1711H_REG_M_VBUS_80;

	return rt1711_i2c_write8(tcpc, RT1711H_REG_RT_MASK, (uint8_t) ret);
}
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

static int rt1711_set_cc(struct tcpc_device *tcpc, int pull)
{
	int ret = 0;
	uint8_t data = 0;
	uint8_t data0 = 0;
	uint8_t old_data = 0;
	int rp_lvl = TYPEC_CC_PULL_GET_RP_LVL(pull), pull1, pull2;
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);

	RT1711_INFO("pull = 0x%02X\n", pull);
	pull = TYPEC_CC_PULL_GET_RES(pull);

	if (chip->chip_id == SY6976_DID_A)
		old_data = rt1711_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL);

	if (pull == TYPEC_CC_DRP) {
		data0= TCPC_V10_REG_ROLE_CTRL_RES_SET(
			0, rp_lvl, TYPEC_CC_RD, TYPEC_CC_RD);
		ret = rt1711_i2c_write8(
			tcpc, TCPC_V10_REG_ROLE_CTRL, data0);
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(
			1, rp_lvl, TYPEC_CC_RD, TYPEC_CC_RD);
		ret = rt1711_i2c_write8(
			tcpc, TCPC_V10_REG_ROLE_CTRL, data); 

		if (ret == 0) {
#if CONFIG_TCPC_VSAFE0V_DETECT_IC
			rt1711_enable_vsafe0v_detect(tcpc, false);
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */
			ret = rt1711_command(tcpc, TCPM_CMD_LOOK_CONNECTION);
		}
	} else {
#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
		if (pull == TYPEC_CC_RD && tcpc->pd_wait_pr_swap_complete)
			rt1711h_init_cc_params(tcpc, TYPEC_CC_VOLT_SNK_DFT);
#endif	/* CONFIG_USB_POWER_DELIVERY */

		pull1 = pull2 = pull;
if(flag_ba41==1){//uu604+ for match pd
//uu312-		if (chip->chip_id != SY6976_DID_A) {
		if (chip->chip_id == SY6976_DID_A) {
			if ((pull == TYPEC_CC_RP_DFT || pull == TYPEC_CC_RP_1_5 ||
				pull == TYPEC_CC_RP_3_0) &&
				tcpc->typec_is_attached_src) {
				if (tcpc->typec_polarity)
					pull1 = TYPEC_CC_OPEN;
				else
					pull2 = TYPEC_CC_OPEN;
			}
		}
}else{
		if (chip->chip_id != SY6976_DID_A) {
			if ((pull == TYPEC_CC_RP_DFT || pull == TYPEC_CC_RP_1_5 ||
				pull == TYPEC_CC_RP_3_0) &&
				tcpc->typec_is_attached_src) {
				if (tcpc->typec_polarity)
					pull1 = TYPEC_CC_OPEN;
				else
					pull2 = TYPEC_CC_OPEN;
			}
}	
		}
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(0, rp_lvl, pull1, pull2);
		if (chip->chip_id != SY6976_DID_A|| old_data != data) {
			ret = rt1711_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, data);
		}
	}

	return 0;
}

static int rt1711_set_polarity(struct tcpc_device *tcpc, int polarity)
{
	int data;

	data = rt1711h_init_cc_params(tcpc,
		tcpc->typec_remote_cc[polarity]);
	if (data)
		return data;

	data = rt1711_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT;
	data |= polarity ? TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT : 0;

	return rt1711_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}

static int rt1711_set_low_rp_duty(struct tcpc_device *tcpc, bool low_rp)
{
	uint16_t duty = low_rp ? TCPC_LOW_RP_DUTY : TCPC_NORMAL_RP_DUTY;

	return rt1711_i2c_write16(tcpc, RT1711H_REG_DRP_DUTY_CTRL, duty);
}

static int rt1711_set_vconn(struct tcpc_device *tcpc, int enable)
{
	int rv;
	int data;

	data = rt1711_i2c_read8(tcpc, TCPC_V10_REG_POWER_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_POWER_CTRL_VCONN;
	data |= enable ? TCPC_V10_REG_POWER_CTRL_VCONN : 0;

	rv = rt1711_i2c_write8(tcpc, TCPC_V10_REG_POWER_CTRL, data);
	if (rv < 0)
		return rv;

	/* BIT(6):Dual_Ra_Detect_EN write to 1 */
	return rt1711_i2c_write8(tcpc, RT1711H_REG_IDLE_CTRL,
		RT1711H_REG_IDLE_SET(0, 1, enable ? 0 : 1, 0) | BIT(6));
}

#ifdef CONFIG_TCPC_LOW_POWER_MODE
static int rt1711_is_low_power_mode(struct tcpc_device *tcpc)
{
	/*struct ba41_chip *chip = tcpc_get_dev_data(tcpc);
	int rv;
	rv = rt1711_i2c_read8(tcpc, RT1711H_REG_BMC_CTRL);

	if (rv < 0)
		return rv;

	if (chip->chip_id == HUSB311_DID) {
		pr_info("%s - read HUSB311_REG_BMC_CTRL=0x%x\n", __func__, rv);
		return (rv & RT1711H_REG_BMCIO_OSC_EN) != 0;
	}
	return (rv & RT1711H_REG_BMCIO_LPEN) != 0;*/
	return -1;
}

static int rt1711_set_low_power_mode(
		struct tcpc_device *tcpc, bool en, int pull)
{
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);
	int ret = 0;
	uint8_t data;

	return -1;//SY6976
if(flag_ba41==1){ret = rt1711_i2c_write8(tcpc, RT1711H_REG_IDLE_CTRL,
		RT1711H_REG_IDLE_SET(0, 1, 1, 0));
}else{
	ret = rt1711_i2c_write8(tcpc, RT1711H_REG_IDLE_CTRL,
		RT1711H_REG_IDLE_SET(0, 1, en ? 0 : 1, 0));
}
	if (ret < 0)
		return ret;
#if CONFIG_TCPC_VSAFE0V_DETECT_IC
	rt1711_enable_vsafe0v_detect(tcpc, !en);
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */
	if (en) {
if(flag_ba41==1){data=0;
//uu305		data = RT1711H_REG_BMCIO_LPEN;
}else{
		data = RT1711H_REG_BMCIO_LPEN;
}
		if (pull & TYPEC_CC_RP)
			data |= RT1711H_REG_BMCIO_LPRPRD;

#ifdef CONFIG_TYPEC_CAP_NORP_SRC
		data |= RT1711H_REG_BMCIO_BG_EN | RT1711H_REG_VBUS_DET_EN;
#endif
	} else {
		data = RT1711H_REG_BMCIO_BG_EN |
			RT1711H_REG_VBUS_DET_EN | RT1711H_REG_BMCIO_OSC_EN;
	}

	if (chip->chip_id == HUSB311_DID) {
if(flag_ba41==1){;
		data &= ~(RT1711H_REG_BMCIO_OSC_EN|RT1711H_REG_BMCIO_BG_EN|RT1711H_REG_VBUS_DET_EN|RT1711H_REG_BMCIO_LPEN);
}else{
		data &= ~RT1711H_REG_BMCIO_OSC_EN;
}
		pr_info("%s - write HUSB311_REG_BMC_CTRL=0x%x\n",
			__func__, data);
		return rt1711_i2c_write8(tcpc, RT1711H_REG_BMC_CTRL, data);
	}

	return rt1711_i2c_write8(tcpc, RT1711H_REG_BMC_CTRL, data);
}
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

#ifndef CONFIG_TCPC_WATCHDOG_EN
int rt1711h_set_watchdog(struct tcpc_device *tcpc, bool en)
{
	uint8_t data = RT1711H_REG_WATCHDOG_CTRL_SET(en, 7);

	return rt1711_i2c_write8(tcpc,
		RT1711H_REG_WATCHDOG_CTRL, data);
}
#endif	/* CONFIG_TCPC_WATCHDOG_EN */

#if CONFIG_TCPC_INTRST_EN
int rt1711h_set_intrst(struct tcpc_device *tcpc, bool en)
{
	return rt1711_i2c_write8(tcpc,
		RT1711H_REG_INTRST_CTRL, RT1711H_REG_INTRST_SET(en, 3));
}
#endif	/* CONFIG_TCPC_INTRST_EN */

static int rt1711_tcpc_deinit(struct tcpc_device *tcpc)
{
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);

#ifdef CONFIG_TCPC_SHUTDOWN_CC_DETACH
	rt1711_set_cc(tcpc, TYPEC_CC_DRP);
	rt1711_set_cc(tcpc, TYPEC_CC_OPEN);

	if (chip->chip_id == HUSB311_DID) {
		rt1711_i2c_write8(tcpc, RT1711H_REG_I2CRST_CTRL, 0x08);
	} else {
		rt1711_i2c_write8(tcpc,
			RT1711H_REG_I2CRST_CTRL,
			RT1711H_REG_I2CRST_SET(true, 4));
	}

	rt1711_i2c_write8(tcpc,
		RT1711H_REG_INTRST_CTRL,
		RT1711H_REG_INTRST_SET(true, 0));
	if (chip->chip_id == SY6976_DID_A)
		rt1711_i2c_write8(tcpc, RT1711H_REG_SWRESET, 1);
#else
	rt1711_i2c_write8(tcpc, RT1711H_REG_SWRESET, 1);
#endif	/* CONFIG_TCPC_SHUTDOWN_CC_DETACH */
#if IS_ENABLED(CONFIG_RT_REGMAP)
	rt_regmap_cache_reload(chip->m_dev);
#endif /* CONFIG_RT_REGMAP */

	return 0;
}
#if 0
//uu731+
static void ba41a_dump_all_workfunc(struct work_struct *work)
{   int i;
	struct tcpc_device *tcpc = container_of(
		work, struct tcpc_device,ba41a_dump_all_work.work);
    
if(flag_tx||flag_rx){
	pr_err("g_buf_ba41_tx:");
	for(i=0;i<32;i++)
	pr_err("%x",tcpc->g_buf_ba41_tx[i]);
	pr_err("g_buf_ba41_rx:");
	for(i=0;i<32;i++)
	pr_err("%x",tcpc->g_buf_ba41_rx[i]);
	flag_tx=0;
	flag_rx=0;
	pr_err("i2cdump_0x3e:");
	for(i=0;i<0xA4;i++)
{
switch(i){
    case 0x0c:continue;
    case 0x0d:continue;
    case 0x0e:continue;
    case 0x0f:continue;

    case 0x16:continue;
    case 0x17:continue;
    case 0x18:continue;

    case 0x20:continue;
    case 0x21:continue;
    case 0x22:continue;

    case 0x24:continue;
    case 0x25:continue;
    case 0x26:continue;
    case 0x27:continue;
    case 0x28:continue;
    case 0x29:continue;
    case 0x2a:continue;
    case 0x2b:continue;
    case 0x2c:continue;
    case 0x2d:continue;

    case 0x72:continue;
    case 0x73:continue;
    case 0x74:continue;
    case 0x75:continue;
    case 0x76:continue;
    case 0x77:continue;
    case 0x78:continue;
    case 0x79:continue;
    case 0x7a:continue;
    case 0x7b:continue;
    case 0x7c:continue;
    case 0x7d:continue;
    case 0x7e:continue;
    case 0x7f:continue;

    case 0x81:continue;
    case 0x82:continue;
    case 0x83:continue;
    case 0x84:continue;
    case 0x85:continue;
    case 0x86:continue;

    case 0x89:continue;
    case 0x8a:continue;
    case 0x8b:continue;
    case 0x8c:continue;
    case 0x8e:continue;
    case 0x8f:continue;

    case 0x91:continue;
    case 0x92:continue;
    case 0x94:continue;
    case 0x96:continue;
    case 0x9a:continue;
    case 0x9f:continue;
	
	default:
	pr_err("reg%x=0x%x",i,rt1711_i2c_read8(tcpc, i));
	}
}
	}
schedule_delayed_work(&tcpc->ba41a_dump_all_work, msecs_to_jiffies(1));//uu731+
}
//uu731+.
#endif

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
static int rt1711_set_msg_header(
	struct tcpc_device *tcpc, uint8_t power_role, uint8_t data_role)
{
	uint8_t msg_hdr = TCPC_V10_REG_MSG_HDR_INFO_SET(
		data_role, power_role);

	return rt1711_i2c_write8(
		tcpc, TCPC_V10_REG_MSG_HDR_INFO, msg_hdr);
}

static int rt1711_protocol_reset(struct tcpc_device *tcpc)
{
	rt1711_i2c_write8(tcpc, RT1711H_REG_PRL_FSM_RESET, 0);
	mdelay(1);
	rt1711_i2c_write8(tcpc, RT1711H_REG_PRL_FSM_RESET, 1);
	return 0;
}

static int rt1711_set_rx_enable(struct tcpc_device *tcpc, uint8_t enable)
{
	int ret = 0;

	if (enable)
		ret = rt1711h_set_clock_gating(tcpc, false);

	if (ret == 0)
		ret = rt1711_i2c_write8(tcpc, TCPC_V10_REG_RX_DETECT, enable);

	if ((ret == 0) && (!enable)) {
		rt1711_protocol_reset(tcpc);
		ret = rt1711h_set_clock_gating(tcpc, true);
	}

	return ret;
}

static int rt1711_get_chip_id(struct tcpc_device *tcpc, uint32_t *chip_id)
{
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);

	*chip_id = chip->chip_id;
	pr_err("%s //uu731 dump all\n", __func__);
	//INIT_DELAYED_WORK(&tcpc->ba41a_dump_all_work, ba41a_dump_all_workfunc);//uu731+//uu734-
	//schedule_delayed_work(&tcpc->ba41a_dump_all_work, msecs_to_jiffies(10000));//uu731+//uu734-
	return 0;
}

static int rt1711_get_chip_pid(struct tcpc_device *tcpc,uint32_t *chip_pid)
{
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);
	*chip_pid = chip->chip_pid;
	return 0;
}

static int rt1711_get_chip_vid(struct tcpc_device *tcpc,uint32_t *chip_vid)
{
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);
	*chip_vid = chip->chip_vid;
	return 0;
}

static int rt1711_get_message(struct tcpc_device *tcpc, uint32_t *payload,
			uint16_t *msg_head, enum tcpm_transmit_type *frame_type)
{
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);
	int rv = 0;
	uint8_t cnt = 0;
	uint8_t type = 0;
//uu616	uint8_t buf_cnt[4];//uu613
	uint8_t buf_ba41[33];//uu602 +_ba41 for match pd in this func
	uint8_t buf[4];
    uint8_t i=0;
	//const uint16_t alert_rx = TCPC_V10_REG_ALERT_RX_STATUS|TCPC_V10_REG_RX_OVERFLOW;
if(flag_ba41==1){
	rv = rt1711_block_read(chip->client, TCPC_V10_REG_RX_BYTE_CNT, 32 , buf_ba41);//uu598+_BA41//uu607 32-35//TCPC_V10_REG_RX_BYTE_CNT_BA41
       // pr_err("r_header:%x,%x",buf_ba41[2],buf_ba41[3]);//uu831 add rx print
	cnt = buf_ba41[0];//uu613
	type = buf_ba41[1];

	*msg_head = ((uint16_t )(buf_ba41[3]<<8))|(buf_ba41[2]);
	//TCPC 1.0 ==> no need to subtract the size of msg_head
	if (rv >= 0 && cnt > 3) {
		while(cnt>3) {
			*(payload+i)=(uint32_t)((buf_ba41[i*4+7]<<24)|(buf_ba41[i*4+6]<<16)|(buf_ba41[i*4+5]<<8)|buf_ba41[i*4+4]);//uu616 back
			cnt=cnt-4;
			i++;
			// pr_err("r_d%d],%x,%x,%x,%x",i,buf_ba41[i*4+4],buf_ba41[i*4+5],buf_ba41[i*4+6],buf_ba41[i*4+7]);//uu831 add rx data print
		}
	}
/* SY6976 TBD
    for(i=0;i<32;i++)//uu635
	{
		tcpc->g_buf_ba41_rx[i]=buf_ba41[i];
	}

	tcpc->flag_rx_one=1;
*/
	flag_rx=1;//uu731+
	*frame_type = (enum tcpm_transmit_type) type;//uu296
	// Read complete, clear RX status alert bit
 //uu302   for(j=0;j<33;j++){pr_err("2buf[%x]=%x\n",j,buf[j]);}//uu274+
	//tcpci_alert_status_clear(tcpc, alert_rx);
	rt1711_read_device(chip->client, RT1711H_REG_IDLE_CTRL, 1, &reg9b);//uu306 here read and update the value, usb work print.
	return rv;
}else{
//uu605
	rv = rt1711_block_read(chip->client, TCPC_V10_REG_RX_BYTE_CNT, 4, buf);
	if (rv < 0)
		return rv;

	cnt = buf[0];
	*frame_type = buf[1];
	*msg_head = le16_to_cpu(*(uint16_t *)&buf[2]);

	/* TCPC 1.0 ==> no need to subtract the size of msg_head */
	if (cnt > 3) {
		cnt -= 3; /* MSG_HDR */
		rv = rt1711_block_read(chip->client, TCPC_V10_REG_RX_DATA, cnt,
				       payload);
	}

	return rv;
}	
}

static int rt1711_set_bist_carrier_mode(
	struct tcpc_device *tcpc, uint8_t pattern)
{
	/* Don't support this function */
	return 0;
}

#ifndef CONFIG_USB_PD_RETRY_CRC_DISCARD
static int rt1711_retransmit(struct tcpc_device *tcpc)
{
	return rt1711_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
			TCPC_V10_REG_TRANSMIT_SET(
			tcpc->pd_retry_count, TCPC_TX_SOP));
}
#endif

#pragma pack(push, 1)
struct tcpc_transmit_packet {
	uint8_t cnt;
	uint16_t msg_header;
	uint8_t data[sizeof(uint32_t)*7];
};
#pragma pack(pop)

static int rt1711_transmit(struct tcpc_device *tcpc,
	enum tcpm_transmit_type type, uint16_t header, const uint32_t *data)
{
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);
	int rv;
	int data_cnt;
	struct tcpc_transmit_packet packet;
	int i;
	int val;
	if (type < TCPC_TX_HARD_RESET) {
		g_type=type;//uu634
		if(pd_cc_is_rd(tcpc)==0)//add start 20240626
		{
			val = rt1711_i2c_read8(tcpc, 0x9B);
			val &= ~BIT(3);//force 24M on 
			rt1711_i2c_write8(tcpc, 0x9B, val);
		}//add end 20240626
		rt1711_i2c_write8(tcpc, 0x23, 0xdd);//uu630 pps+
		data_cnt = sizeof(uint32_t) * PD_HEADER_CNT(header);

		packet.cnt = data_cnt + sizeof(uint16_t);
		tcpc->g_buf_ba41_tx[0]=packet.cnt;//uu638
		packet.msg_header = header;

		tcpc->g_buf_ba41_tx[1]=(uint8_t )header;
		tcpc->g_buf_ba41_tx[2]=(uint8_t )(header>>8);
             //   pr_err("t_h:%x,%x",g_buf_ba41_tx[0],g_buf_ba41_tx[1]);//uu831 add tx print  uu666
		if (data_cnt > 0)
			memcpy(packet.data, (uint8_t *) data, data_cnt);
//		for(i=0;i<data_cnt;i++)
//		g_buf_ba41_tx[i+3]= data[i];

		for(i=0;i<PD_HEADER_CNT(header);i++)
		{
		tcpc->g_buf_ba41_tx[i*4+3]= data[i]&(0xff);
		tcpc->g_buf_ba41_tx[i*4+4]= (data[i]>>8)&(0xff);
		tcpc->g_buf_ba41_tx[i*4+5]= (data[i]>>16)&(0xff);
		tcpc->g_buf_ba41_tx[i*4+6]= (data[i]>>24)&(0xff);
		}
		//tcpc->flag_tx_one=1;
		flag_rx=1;//uu731+
		//uu605
		if(flag_ba41==1)//uu603 +TCPC_V10_REG_TX_BYTE_CNT_BA41
		{
			rv = rt1711_block_write(chip->client,
					TCPC_V10_REG_TX_BYTE_CNT,
					packet.cnt+1, (uint8_t *) &packet);

			/*for(i=0;i<PD_HEADER_CNT(header);i++)//uu831 add tx print
			{
				pr_err("t_d%d]:%x,%x,%x,%x",i,g_buf_ba41_tx[i*4+3],g_buf_ba41_tx[i*4+4],g_buf_ba41_tx[i*4+5],g_buf_ba41_tx[i*4+6]);
			}
			*/
/*//uu725-
	pr_err("g_buf_ba41_tx:");//uu724+
	for(i=0;i<32;i++)//uu724+
		pr_err("%x",tcpc->g_buf_ba41_tx[i]);//uu724+
	pr_err("i2cdump_0x3e:");//uu724+
	for(i=0;i<0xA4;i++)//uu724+
	pr_err("%x",rt1711_i2c_read8(tcpc, i));//uu724+
*/
		} else {
			if (chip->chip_id == SY6976_DID_A)
			packet.cnt += 4;

			rv = rt1711_block_write(chip->client,
					TCPC_V10_REG_TX_BYTE_CNT,
					packet.cnt+1, (uint8_t *) &packet);
		}
		if (rv < 0)
			return rv;
	}

	rv = rt1711_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
			TCPC_V10_REG_TRANSMIT_SET(
			tcpc->pd_retry_count, type));
	if (pd_cc_is_rd(tcpc) == 0) { 
		val = rt1711_i2c_read8(tcpc, 0x9B);
		val |= BIT(3);
		rt1711_i2c_write8(tcpc, 0x9B, val);
	}

//uu727	pr_err("g_buf_ba41_tx:");//uu725+
//uu727	for(i=0;i<32;i++)//uu725+
//uu727		pr_err("%x",tcpc->g_buf_ba41_tx[i]);//uu725+
//uu726	pr_err("i2cdump_0x3e:");//uu725+
//uu726	for(i=0;i<0xA4;i++)//uu725+
//uu726	pr_err("reg%x=%x",i,rt1711_i2c_read8(tcpc, i));//uu725+	

//uu728+
if(0){//uu729 1->0 close print
	pr_err("g_buf_ba41_tx:");
	for(i=0;i<32;i++)
	pr_err("%x",tcpc->g_buf_ba41_tx[i]);
	pr_err("i2cdump_0x3e:");
	for(i=0;i<0xA4;i++)
{
switch(i){
    case 0x0c:continue;
    case 0x0d:continue;
    case 0x0e:continue;
    case 0x0f:continue;

    case 0x16:continue;
    case 0x17:continue;
    case 0x18:continue;

    case 0x20:continue;
    case 0x21:continue;
    case 0x22:continue;

    case 0x24:continue;
    case 0x25:continue;
    case 0x26:continue;
    case 0x27:continue;
    case 0x28:continue;
    case 0x29:continue;
    case 0x2a:continue;
    case 0x2b:continue;
    case 0x2c:continue;
    case 0x2d:continue;

    case 0x72:continue;
    case 0x73:continue;
    case 0x74:continue;
    case 0x75:continue;
    case 0x76:continue;
    case 0x77:continue;
    case 0x78:continue;
    case 0x79:continue;
    case 0x7a:continue;
    case 0x7b:continue;
    case 0x7c:continue;
    case 0x7d:continue;
    case 0x7e:continue;
    case 0x7f:continue;

    case 0x81:continue;
    case 0x82:continue;
    case 0x83:continue;
    case 0x84:continue;
    case 0x85:continue;
    case 0x86:continue;

    case 0x89:continue;
    case 0x8a:continue;
    case 0x8b:continue;
    case 0x8c:continue;
    case 0x8e:continue;
    case 0x8f:continue;

    case 0x91:continue;
    case 0x92:continue;
    case 0x94:continue;
    case 0x96:continue;
    case 0x9a:continue;
    case 0x9f:continue;
	
	default:
	pr_err("reg%x=0x%x",i,rt1711_i2c_read8(tcpc, i));
	}
}
	}//uu728+.

	return rv;
}

//SY6976 handle for I2C error
 int rt1711_transmit_ba41(struct tcpc_device *tcpc)
{
	struct ba41_chip *chip = tcpc_get_dev_data(tcpc);
	int rv;
	int i;

	rt1711_i2c_write8(tcpc, 0x23, 0xdd);//uu630 pps+
	rv = rt1711_block_write(chip->client,
			TCPC_V10_REG_TX_BYTE_CNT,
			tcpc->g_buf_ba41_tx[0]+1, (uint8_t *)(tcpc->g_buf_ba41_tx));
	if (rv < 0)
		return rv;

	rv = rt1711_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
			TCPC_V10_REG_TRANSMIT_SET(
			tcpc->pd_retry_count, g_type));
	pr_err("g_buf_ba41_tx:");
	for(i=0;i<7;i++)
		pr_err("%x",tcpc->g_buf_ba41_tx[i]);

	return rv;
}

static int rt1711_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
	int data;

	data = rt1711_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE;
	data |= en ? TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE : 0;

	return rt1711_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}
#endif /* CONFIG_USB_POWER_DELIVERY */

static struct tcpc_ops rt1711_tcpc_ops = {
	.init = rt1711_tcpc_init,
	.alert_status_clear = rt1711_alert_status_clear,
	.fault_status_clear = rt1711_fault_status_clear,
	.get_chip_id = rt1711_get_chip_id,
	.get_chip_vid = rt1711_get_chip_vid,
	.get_chip_pid = rt1711_get_chip_pid,
	.get_alert_mask = rt1711_get_alert_mask,
	.get_alert_status = rt1711_get_alert_status,
	.get_power_status = rt1711_get_power_status,
	.get_fake_power_status = rt1711_get_fake_power_status,
	.get_fake_power_status = rt1711_get_fake_power_status,
	.get_fault_status = rt1711_get_fault_status,
	.get_cc = rt1711_get_cc,
	.set_cc = rt1711_set_cc,
	.set_polarity = rt1711_set_polarity,
	.set_low_rp_duty = rt1711_set_low_rp_duty,
	.set_vconn = rt1711_set_vconn,
	.deinit = rt1711_tcpc_deinit,

#ifdef CONFIG_TCPC_LOW_POWER_MODE
	.is_low_power_mode = rt1711_is_low_power_mode,
	.set_low_power_mode = rt1711_set_low_power_mode,
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

#ifndef CONFIG_TCPC_WATCHDOG_EN
	.set_watchdog = rt1711h_set_watchdog,
#endif	/* CONFIG_TCPC_WATCHDOG_EN */

#if CONFIG_TCPC_INTRST_EN
	.set_intrst = rt1711h_set_intrst,
#endif	/* CONFIG_TCPC_INTRST_EN */

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	.set_msg_header = rt1711_set_msg_header,
	.set_rx_enable = rt1711_set_rx_enable,
	.protocol_reset = rt1711_protocol_reset,
	.get_message = rt1711_get_message,
	.transmit = rt1711_transmit,
	.force_retry=rt1711_transmit_ba41,//uu635
	.set_bg_enable = ba41_bg_ctrl,
	.set_bist_test_mode = rt1711_set_bist_test_mode,
	.set_bist_carrier_mode = rt1711_set_bist_carrier_mode,
#endif	/* CONFIG_USB_POWER_DELIVERY */

#ifndef CONFIG_USB_PD_RETRY_CRC_DISCARD
	.retransmit = rt1711_retransmit,
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */
};

static int ba41a_parse_dt(struct ba41_chip *chip, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret = 0;

	pr_info("%s\n", __func__);

	ret = of_get_named_gpio(np, "rt1711pd,intr_gpio", 0);
	if (ret < 0) {
		pr_err("%s no intr_gpio info\n", __func__);
		return ret;
	}
	chip->irq_gpio = ret;

	return ret < 0 ? ret : 0;
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

#if IS_ENABLED(CONFIG_PD_DBG_INFO)
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
		pr_info("pr_info : t2-t1 = %lu\n",
				(unsigned long)nsrem / 1000);
	}
#else
	for (i = 0; i < 10; i++) {
		t1 = local_clock();
		pr_info("%d\n", i);
		t2 = local_clock();
		t2 -= t1;
		nsrem = do_div(t2, 1000000000);
		pr_info("t2-t1 = %lu\n",
				(unsigned long)nsrem /  1000);
		PD_BUG_ON(nsrem > 100*1000);
	}
#endif /* CONFIG_PD_DBG_INFO */
}
#endif /* TCPC_ENABLE_ANYMSG */

static int rt1711_tcpcdev_init(struct ba41_chip *chip, struct device *dev)
{
	struct tcpc_desc *desc;
	struct device_node *np = dev->of_node;
	u32 val, len;
	const char *name = "default";

	dev_info(dev, "%s\n", __func__);

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	if (of_property_read_u32(np, "rt-tcpc,role_def", &val) >= 0) {
		if (val >= TYPEC_ROLE_NR)
			desc->role_def = TYPEC_ROLE_DRP;
		else
			desc->role_def = val;
	} else {
		dev_info(dev, "use default Role DRP\n");
		desc->role_def = TYPEC_ROLE_DRP;
	}

	if (of_property_read_u32(
		np, "rt-tcpc,notifier_supply_num", &val) >= 0) {
		if (val < 0)
			desc->notifier_supply_num = 0;
		else
			desc->notifier_supply_num = val;
	} else
		desc->notifier_supply_num = 0;

	if (of_property_read_u32(np, "rt-tcpc,rp_level", &val) >= 0) {
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
	if (of_property_read_u32(np, "rt-tcpc,vconn_supply", &val) >= 0) {
		if (val >= TCPC_VCONN_SUPPLY_NR)
			desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
		else
			desc->vconn_supply = val;
	} else {
		dev_info(dev, "use default VconnSupply\n");
		desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
	}
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

	if (of_property_read_string(np, "rt-tcpc,name",
				(char const **)&name) < 0) {
		dev_info(dev, "use default name\n");
	}

	len = strlen(name);
	desc->name = kzalloc(len+1, GFP_KERNEL);
	if (!desc->name)
		return -ENOMEM;

	strlcpy((char *)desc->name, name, len+1);

	chip->tcpc_desc = desc;

	chip->tcpc = tcpc_device_register(dev,
			desc, &rt1711_tcpc_ops, chip);
	if (IS_ERR_OR_NULL(chip->tcpc))
		return -EINVAL;

#ifndef CONFIG_USB_PD_DISABLE_PE
	chip->tcpc->disable_pe =
			of_property_read_bool(np, "rt-tcpc,disable_pe");
#endif	/* CONFIG_USB_PD_DISABLE_PE */

	chip->tcpc->tcpc_flags = TCPC_FLAGS_LPM_WAKEUP_WATCHDOG |
			TCPC_FLAGS_VCONN_SAFE5V_ONLY;

	if (chip->chip_id > RT1711H_DID_B)
		chip->tcpc->tcpc_flags |= TCPC_FLAGS_CHECK_RA_DETACH;

#ifndef CONFIG_USB_PD_RETRY_CRC_DISCARD
	if (chip->chip_id >= RT1715_DID_D)
		chip->tcpc->tcpc_flags |= TCPC_FLAGS_RETRY_CRC_DISCARD;
#endif  /* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD //20240624 added by gw 
	if (chip->chip_id == SY6976_DID_A)
		chip->tcpc->tcpc_flags |= TCPC_FLAGS_RETRY_CRC_DISCARD;
#endif  /* CONFIG_USB_PD_RETRY_CRC_DISCARD */   //20240624 added by gw 

#ifdef CONFIG_USB_PD_REV30
	chip->tcpc->tcpc_flags |= TCPC_FLAGS_PD_REV30;
	dev_info(dev, "PD_REV30\n");
#endif	/* CONFIG_USB_PD_REV30 */
	chip->tcpc->tcpc_flags |= TCPC_FLAGS_ALERT_V10;

	return 0;
}

//+Bug676386, yangpingao.wt, 2021.9.20, add hynetek husb311 support
#define RICHTEK_1711_VID	0x29cf
#define RICHTEK_1711_PID	0x1711
#define RICHTEK_1711_DID	0x2173

#define SC2150A_VID		0x311C
#define SC2150A_PID		0x2150

#define SY6976_VID		0X311C
#define SY6976_PID		0X6600
#define SY6976_DID		0X0001

#define HYNETEK_HUSB311_VID	0x2e99
#define HYNETEK_HUSB311_PID	0x0311
#define HYNETEK_HUSB311_DID	0x0000
static inline int rt1711h_check_revision(struct i2c_client *client)
{
	u16 vid, pid, did;
	int ret;
	u8 data = 1;

	ret = rt1711_read_device(client, TCPC_V10_REG_VID, 2, &vid);
	if (ret < 0) {
		dev_err(&client->dev, "read chip ID fail(%d)\n", ret);
		return -EIO;
	}

	if ((vid != RICHTEK_1711_VID) && (vid != HYNETEK_HUSB311_VID)
				&& (vid != SC2150A_VID) && (vid != SY6976_VID)) {
		pr_info("%s failed, VID=0x%04x\n", __func__, vid);
		return -ENODEV;
	}

	ret = rt1711_read_device(client, TCPC_V10_REG_PID, 2, &pid);
	if (ret < 0) {
		dev_err(&client->dev, "read product ID fail(%d)\n", ret);
		return -EIO;
	}

	if ((pid != RICHTEK_1711_PID) && (pid != HYNETEK_HUSB311_PID)
				&& (pid != SC2150A_PID) && (pid != SY6976_PID)) {
		pr_info("%s failed, PID=0x%04x\n", __func__, pid);
		return -ENODEV;
	}
if(flag_ba41==1){
	ret = rt1711_write_device(client, TCPC_V10_REG_TCPC_CTRL, 1, &data);
}else{
	ret = rt1711_write_device(client, RT1711H_REG_SWRESET, 1, &data);
	if (ret < 0)
		return ret;
}
#if 0
//SY6976 CONFIG_TCPC_LOW_POWER_MODE
	if (pid == HYNETEK_HUSB311_PID) {
		msleep(5);
		data = 0x00;
		rt1711_write_device(client, RT1711H_REG_BMC_CTRL, 1, &data);
		pr_info("%s set low pwr mode\n", __func__);
	}
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

	usleep_range(1000, 2000);

	ret = rt1711_read_device(client, TCPC_V10_REG_DID, 2, &did);
	if (ret < 0) {
		dev_err(&client->dev, "read device ID fail(%d)\n", ret);
		return -EIO;
	}

	if (did == SY6976_DID)
		did = SY6976_DID_A;

	//if (did == HYNETEK_HUSB311_DID) {
		pr_err("%s, HUSB311_DID=0x%04x\n", __func__, did);
	//	did = RICHTEK_1711_DID;
//	}

	return did;
}
//-Bug676386, yangpingao.wt, 2021.9.20, add hynetek husb311 support

static int ba41_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct ba41_chip *chip;
	int ret = 0, chip_id;
	bool use_dt = client->dev.of_node;
#if 0//CONFIG_TCPC_LOW_POWER_MODE
	u8 data;
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */
	u16 test_vid;
	static int probe_cnt = 0;
	u8 reg99 = 0;//uu687

	dev_err(&client->dev, "%s (%s) start, probe_cnt = %d\n",
			__func__, RT1711H_DRV_VERSION, ++probe_cnt);

	if (i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_I2C_BLOCK | I2C_FUNC_SMBUS_BYTE_DATA))
		pr_info("ba41a I2C functionality : OK...\n");
	else
		pr_info("ba41a I2C functionality check : failuare...\n");

	chip_id = rt1711h_check_revision(client);
	if (chip_id < 0)
		return chip_id;

	ret = rt1711_read_device(client, TCPC_V10_REG_VID, 2, &test_vid);
	if (ret < 0) {
		dev_err(&client->dev, "%s: no pdphy device found:%d\n", __func__, ret);
		//chip->ba41a_pinctrl = devm_pinctrl_get(chip->dev);
		//devm_pinctrl_put(chip->ba41a_pinctrl);
		ret = -EPROBE_DEFER;
		msleep(500);
		if (probe_cnt >= PROBE_CNT_MAX)
			return -ENODEV;
		else
			goto no_pdphy_device;
	}

#if TCPC_ENABLE_ANYMSG
	check_printk_performance();
#endif /* TCPC_ENABLE_ANYMSG */

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	if (use_dt) {
		ret = ba41a_parse_dt(chip, &client->dev);
		if (ret < 0)
			return ret;
	} else {
		dev_err(&client->dev, "no dts node\n");
		return -ENODEV;
	}

	chip->dev = &client->dev;
	chip->client = client;
	//chip->irq = client->irq;
	sema_init(&chip->io_lock, 1);
	sema_init(&chip->suspend_lock, 1);
	i2c_set_clientdata(client, chip);
	chip->irq_wake_lock =
		wakeup_source_register(chip->dev, "rt1711h_irq_wake_lock");


	ret=rt1711_read_device(client, 0x9f, 1, &reg9f);
	if((reg9f & BIT(0)) == 1)
	{
		flag_ba41=1;
		//RT_REG(TCPC_V10_REG_TX_BYTE_CNT)->size = 32;//uu651
		//RT_REG(TCPC_V10_REG_RX_BYTE_CNT)->size = 32;//uu652
		pr_err("flag_ba41==1:ok >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
	}
	pr_err("reg9f=%x",reg9f);
//uu580+.

//uu674
 	rt1711_read_device(client, 0x9f, 1, &reg9f);
 	reg9f |= BIT(2);
 	rt1711_write_device(client, 0x9f, 1, &reg9f);

 	rt1711_read_device(client, 0x9b, 1, &reg9b);
 	reg9b |= BIT(6);//uu675
 	rt1711_write_device(client, 0x9b, 1, &reg9b);
 	 pr_err("reg9f=%x. reg9b=%x",reg9f, reg9b);
//uu674

  	i2c_smbus_write_byte_data(client, RT1711H_REG_RT_INT, 0xFF); //uu687
  	rt1711_read_device(client, RT1711H_REG_RT_MASK, 1, &reg99);
	reg99 |= BIT(2);
	rt1711_write_device(client, RT1711H_REG_RT_MASK, 1, &reg99);//uu687




	chip->chip_id = chip_id;
	pr_info("rt1711h_chipID = 0x%0x\n", chip_id);

	ret = rt1711_regmap_init(chip);
	if (ret < 0) {
		dev_err(chip->dev, "rt1711 regmap init fail\n");
		goto err_regmap_init;
	}

//uu+	
	if(flag_ba41==1){
		ret = rt1711_read_device(client, RT1711H_REG_IDLE_CTRL, 1, &reg9b);
		if(ret<0) {
			pr_err("error:BA76_PD_reg9b!!!!\n");
			goto err_irq_init;
		}
		reg9b|=0x08;
		rt1711_write_device(client, RT1711H_REG_IDLE_CTRL, 1, &reg9b);//auto idl
		rt1711_write_word(chip->client, TCPC_V10_REG_ALERT, 0xffff);//clr alert
		rt1711_reg_write(chip->client, RT1711H_REG_RT_INT, 0xff);//uu687

		pr_err("BA76_PD_reg9b=%x\n",reg9b);
	}
//uu+.  pd:7E: REG10->FF ,11->FF; PD_INT :10->FF 11->FF; for chip fix.

	ret = rt1711_tcpcdev_init(chip, &client->dev);
	if (ret < 0) {
		dev_err(&client->dev, "ba41 tcpc dev init fail\n");
		goto err_tcpc_reg;
	}

	ret = rt1711_init_alert(chip->tcpc);
	if (ret < 0) {
		pr_err("ba41 init alert fail\n");
		goto err_irq_init;
	}

#if 0 
//SY6976  if CONFIG_TCPC_LOW_POWER_MODE
	if (chip->chip_id == HUSB311_DID) {
		data = 0x00;
		rt1711_write_device(client, RT1711H_REG_BMC_CTRL, 1, &data);
		pr_info("%s set low pwr mode\n", __func__);
	}
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

	//tcpc_schedule_dumpmsg_work(chip->tcpc);//uu665//uu671-
	g_tcpc_ba41a = chip;
//	INIT_DELAYED_WORK(&tcpc->ba41a_dump_all_work, ba41a_dump_all_workfunc);//uu731+
	dev_err(&client->dev, "%s probe OK!\n", __func__);
//	schedule_delayed_work(&tcpc->ba41a_dump_all_work, msecs_to_jiffies(10000));//uu731+
	return 0;

err_irq_init:
	tcpc_device_unregister(chip->dev, chip->tcpc);
err_tcpc_reg:
	rt1711_regmap_deinit(chip);
err_regmap_init:
no_pdphy_device:
	return ret;
}

static int ba41_i2c_remove(struct i2c_client *client)
{
	struct ba41_chip *chip = i2c_get_clientdata(client);

	if (chip) {
		tcpc_device_unregister(chip->dev, chip->tcpc);
		rt1711_regmap_deinit(chip);
	}
	flag_ba41=0;//uu581
	return 0;
}

#if CONFIG_PM
static int rt1711_i2c_suspend(struct device *dev)
{
	struct ba41_chip *chip = dev_get_drvdata(dev);

	dev_info(dev, "%s\n", __func__);
	if (device_may_wakeup(dev))
		enable_irq_wake(chip->irq);
	disable_irq(chip->irq);

	return 0;
}

static int rt1711_i2c_resume(struct device *dev)
{
	struct ba41_chip *chip = dev_get_drvdata(dev);

	dev_info(dev, "%s\n", __func__);
	enable_irq(chip->irq);
	if (device_may_wakeup(dev))
		disable_irq_wake(chip->irq);

	return 0;
}

static void ba41_shutdown(struct i2c_client *client)
{
	struct ba41_chip *chip = i2c_get_clientdata(client);
	flag_ba41=0;//uu581
	i2c_smbus_write_byte_data(client, 0x1A, 0x0A);
	/* Please reset IC here */
	if (chip != NULL) {
		if (chip->irq)
			disable_irq(chip->irq);
		tcpm_shutdown(chip->tcpc);
	} else {
		i2c_smbus_write_byte_data(
			client, RT1711H_REG_SWRESET, 0x01);
		if (chip->chip_id == HUSB311_DID) {
			i2c_smbus_write_byte_data(client, RT1711H_REG_BMC_CTRL, 0x00);
		}
	}
}

#if IS_ENABLED(CONFIG_PM_RUNTIME)
static int rt1711_pm_suspend_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: suspending...\n");
	return 0;
}

static int rt1711_pm_resume_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: resuming...\n");
	return 0;
}
#endif /* #if CONFIG_PM_RUNTIME */

static const struct dev_pm_ops rt1711_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
			rt1711_i2c_suspend,
			rt1711_i2c_resume)
#if IS_ENABLED(CONFIG_PM_RUNTIME)
	SET_RUNTIME_PM_OPS(
		rt1711_pm_suspend_runtime,
		rt1711_pm_resume_runtime,
		NULL
	)
#endif /* #if CONFIG_PM_RUNTIME */
};
#define RT1711_PM_OPS	(&rt1711_pm_ops)
#else
#define RT1711_PM_OPS	(NULL)
#endif /* CONFIG_PM */

static const struct i2c_device_id ba41_id_table[] = {
//uu626	{"rt1711h", 0},	
	{"ba41a", 0},
	{"rt1715", 0},
	{"rt1716", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, ba41_id_table);

static const struct of_device_id rt_match_table[] = {
//uu626	{.compatible = "richtek,rt1711h",},
	{.compatible = "sy,ba41a",},//uu420+ for dts
	//{.compatible = "richtek,rt1715",},
	//{.compatible = "richtek,rt1716",},
	{},
};
MODULE_DEVICE_TABLE(of, rt_match_table);

static struct i2c_driver ba41_driver = {
	.driver = {
		.name = "tcpc_ba4x",
		.owner = THIS_MODULE,
		.of_match_table = rt_match_table,
		.pm = RT1711_PM_OPS,
	},
	.probe = ba41_i2c_probe,
	.remove = ba41_i2c_remove,
	.shutdown = ba41_shutdown,
	.id_table = ba41_id_table,
};

module_i2c_driver(ba41_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("david.dai@silergycorp.com>");
MODULE_DESCRIPTION("BA41 TCPC Driver");
