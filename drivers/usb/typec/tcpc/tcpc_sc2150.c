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
#include <linux/sched/clock.h>

#include "inc/pd_dbg_info.h"
#include "inc/tcpci.h"
#include "inc/sc2150.h"
#ifdef SUPPORT_SOUTHCHIP_CPU_BOOST
#include "../../cpu_boost.h"
#endif

#ifdef CONFIG_RT_REGMAP
#include "inc/rt-regmap.h"
#endif /* CONFIG_RT_REGMAP */

#define SC2150_DRV_VER      "0.0.1"
#define SC2150_IRQ_WAKE_TIME        (500) // ms

#ifndef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
#define SOUTHCHIP_PD_VID    0x311C
#define SC2150_PID			0x2150
#endif

struct sc2150 {
    struct i2c_client *client;
    struct device *dev;
    #ifdef CONFIG_RT_REGMAP
    struct rt_regmap_device *m_dev;
#endif /* CONFIG_RT_REGMAP */
    struct tcpc_desc *tcpc_desc;
    struct tcpc_device *tcpc;

    int irq_gpio;
    int irq;
    int chip_id;
    uint16_t chip_pid;
    uint16_t chip_vid;
};


#ifdef CONFIG_RT_REGMAP
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
RT_REG_DECL(TCPC_V10_REG_RX_BYTE_CNT, 4, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_DATA, 28, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TRANSMIT, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TX_BYTE_CNT, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_TX_HDR, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_TX_DATA, 28, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(SC2150_REG_ANA_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(SC2150_REG_ANA_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(SC2150_REG_ANA_INT, 1, RT_VOLATILE, {});
RT_REG_DECL(SC2150_REG_ANA_MASK, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(SC2150_REG_ANA_CTRL2, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(SC2150_REG_ANA_CTRL3, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(SC2150_REG_RST_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(SC2150_REG_DRP_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(SC2150_REG_DRP_DUTY_CTRL, 2, RT_NORMAL_WR_ONCE, {});

static const rt_register_map_t sc2150_regmap[] = {
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
	RT_REG(TCPC_V10_REG_RX_DATA),
	RT_REG(TCPC_V10_REG_TRANSMIT),
	RT_REG(TCPC_V10_REG_TX_BYTE_CNT),
	RT_REG(TCPC_V10_REG_TX_HDR),
	RT_REG(TCPC_V10_REG_TX_DATA),
    RT_REG(SC2150_REG_ANA_CTRL1),
    RT_REG(SC2150_REG_ANA_STATUS),
    RT_REG(SC2150_REG_ANA_INT),
    RT_REG(SC2150_REG_ANA_MASK),
    RT_REG(SC2150_REG_ANA_CTRL2),
    RT_REG(SC2150_REG_ANA_CTRL3),
    RT_REG(SC2150_REG_RST_CTRL),
    RT_REG(SC2150_REG_DRP_CTRL),
    RT_REG(SC2150_REG_DRP_DUTY_CTRL),
};
#define SC2150_REGMAP_SIZE ARRAY_SIZE(sc2150_regmap)

#endif /* CONFIG_RT_REGMAP */


static int sc2150_read_device(void *client, u32 reg, int len, void *dst)
{
    struct i2c_client *i2c = client;
    int ret = 0, count = 5;
    u64 t1 = 0, t2 = 0;

    while (1) {
        t1 = local_clock();
        ret = i2c_smbus_read_i2c_block_data(i2c, reg, len, dst);
        t2 = local_clock();
        SC2150_INFO("%s del = %lluus, reg = 0x%02X, len = %d, val = 0x%08X\n",
                __func__, (t2 - t1) / NSEC_PER_USEC, reg, len, *(u16 *)dst);
        if (ret < 0 && count > 1)
            count--;
        else
            break;
        udelay(100);
    }
    return ret;
}


static int sc2150_write_device(void *client, u32 reg, int len, const void *src)
{
    struct i2c_client *i2c = client;
    int ret = 0, count = 5;
    u64 t1 = 0, t2 = 0;

    while (1) {
        t1 = local_clock();
        ret = i2c_smbus_write_i2c_block_data(i2c, reg, len, src);
        t2 = local_clock();
        SC2150_INFO("%s del = %lluus, reg = %02X, len = %d, val = 0x%08X\n",
                __func__, (t2 - t1) / NSEC_PER_USEC, reg, len, *(u8 *)src);
        if (ret < 0 && count > 1)
            count--;
        else
            break;
        udelay(100);
    }
    return ret;
}

static int sc2150_reg_read(struct i2c_client *i2c, u8 reg)
{
    struct sc2150 *sc = i2c_get_clientdata(i2c);
    u8 val = 0;
    int ret = 0;

#ifdef CONFIG_RT_REGMAP
    ret = rt_regmap_block_read(sc->m_dev, reg, 1, &val);
#else
    ret = sc2150_read_device(sc->client, reg, 1, &val);
#endif /* CONFIG_RT_REGMAP */
    if (ret < 0) {
        dev_err(sc->dev, "sc2150 reg read fail\n");
        return ret;
    }
    return val;
}

static int sc2150_reg_write(struct i2c_client *i2c, u8 reg, const u8 data)
{
    struct sc2150 *sc = i2c_get_clientdata(i2c);
    int ret = 0;

#ifdef CONFIG_RT_REGMAP
    ret = rt_regmap_block_write(sc->m_dev, reg, 1, &data);
#else
    ret = sc2150_write_device(sc->client, reg, 1, &data);
#endif /* CONFIG_RT_REGMAP */
    if (ret < 0)
        dev_err(sc->dev, "sc2150 reg write fail\n");
    return ret;
}

static int sc2150_block_read(struct i2c_client *i2c,
            u8 reg, int len, void *dst)
{
    struct sc2150 *sc = i2c_get_clientdata(i2c);
    int ret = 0;
#ifdef CONFIG_RT_REGMAP
    ret = rt_regmap_block_read(sc->m_dev, reg, len, dst);
#else
    ret = sc2150_read_device(sc->client, reg, len, dst);
#endif /* #ifdef CONFIG_RT_REGMAP */
    if (ret < 0)
        dev_err(sc->dev, "sc2150 block read fail\n");
    return ret;
}

static int sc2150_block_write(struct i2c_client *i2c,
            u8 reg, int len, const void *src)
{
    struct sc2150 *sc = i2c_get_clientdata(i2c);
    int ret = 0;
#ifdef CONFIG_RT_REGMAP
    ret = rt_regmap_block_write(sc->m_dev, reg, len, src);
#else
    ret = sc2150_write_device(sc->client, reg, len, src);
#endif /* #ifdef CONFIG_RT_REGMAP */
    if (ret < 0)
        dev_err(sc->dev, "sc2150 block write fail\n");
    return ret;
}


static int32_t sc2150_write_word(struct i2c_client *client,
                    uint8_t reg_addr, uint16_t data)
{
    int ret;

    /* don't need swap */
    ret = sc2150_block_write(client, reg_addr, 2, (uint8_t *)&data);
    return ret;
}

static int32_t sc2150_read_word(struct i2c_client *client,
                    uint8_t reg_addr, uint16_t *data)
{
    int ret;

    /* don't need swap */
    ret = sc2150_block_read(client, reg_addr, 2, (uint8_t *)data);
    return ret;
}

static inline int sc2150_i2c_write8(
    struct tcpc_device *tcpc, u8 reg, const u8 data)
{
    struct sc2150 *sc = tcpc_get_dev_data(tcpc);

    return sc2150_reg_write(sc->client, reg, data);
}

static inline int sc2150_i2c_write16(
        struct tcpc_device *tcpc, u8 reg, const u16 data)
{
    struct sc2150 *sc = tcpc_get_dev_data(tcpc);

    return sc2150_write_word(sc->client, reg, data);
}

static inline int sc2150_i2c_read8(struct tcpc_device *tcpc, u8 reg)
{
    struct sc2150 *sc = tcpc_get_dev_data(tcpc);

    return sc2150_reg_read(sc->client, reg);
}

static inline int sc2150_i2c_read16(
    struct tcpc_device *tcpc, u8 reg)
{
    struct sc2150 *sc = tcpc_get_dev_data(tcpc);
    u16 data;
    int ret;

    ret = sc2150_read_word(sc->client, reg, &data);
    if (ret < 0)
        return ret;
    return data;
}

#ifdef CONFIG_RT_REGMAP
static struct rt_regmap_fops sc2150_regmap_fops = {
    .read_device = sc2150_read_device,
    .write_device = sc2150_write_device,
};
#endif /* CONFIG_RT_REGMAP */

static int sc2150_regmap_init(struct sc2150 *sc)
{
#ifdef CONFIG_RT_REGMAP
    struct rt_regmap_properties *props;
    char name[32];
    int len;

    props = devm_kzalloc(sc->dev, sizeof(*props), GFP_KERNEL);
    if (!props)
        return -ENOMEM;

    props->register_num = SC2150_REGMAP_SIZE;
    props->rm = sc2150_regmap;

    props->rt_regmap_mode = RT_MULTI_BYTE |
                RT_IO_PASS_THROUGH | RT_DBG_SPECIAL;
    snprintf(name, sizeof(name), "sc2150-%02x", sc->client->addr);

    len = strlen(name);
    props->name = kzalloc(len+1, GFP_KERNEL);
    props->aliases = kzalloc(len+1, GFP_KERNEL);

    if ((!props->name) || (!props->aliases))
        return -ENOMEM;

    strlcpy((char *)props->name, name, len+1);
    strlcpy((char *)props->aliases, name, len+1);
    props->io_log_en = 0;

    sc->m_dev = rt_regmap_device_register(props,
            &sc2150_regmap_fops, sc->dev, sc->client, sc);
    if (!sc->m_dev) {
        dev_err(sc->dev, "sc2150 rt_regmap register fail\n");
        return -EINVAL;
    }
#endif
    return 0;
}

static int sc2150_regmap_deinit(struct sc2150 *sc)
{
#ifdef CONFIG_RT_REGMAP
    rt_regmap_device_unregister(sc->m_dev);
#endif
    return 0;
}

__maybe_unused
static int sc2150_test_mode(struct sc2150 *sc)
{
    return 0;
}

static inline int sc2150_software_reset(struct tcpc_device *tcpc)
{
    int ret = sc2150_i2c_write8(tcpc, SC2150_REG_RST_CTRL, 1);
#ifdef CONFIG_RT_REGMAP
    struct sc2150 *sc = tcpc_get_dev_data(tcpc);
#endif /* CONFIG_RT_REGMAP */

    if (ret < 0)
        return ret;
#ifdef CONFIG_RT_REGMAP
    rt_regmap_cache_reload(sc->m_dev);
#endif /* CONFIG_RT_REGMAP */
    usleep_range(1000, 2000);
    return 0;
}

static inline int sc2150_command(struct tcpc_device *tcpc, uint8_t cmd)
{
    return sc2150_i2c_write8(tcpc, TCPC_V10_REG_COMMAND, cmd);
}

static int sc2150_init_alert_mask(struct tcpc_device *tcpc)
{
    uint16_t mask;

    mask = TCPC_V10_REG_ALERT_CC_STATUS | TCPC_V10_REG_ALERT_POWER_STATUS;

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	/* Need to handle RX overflow */
	mask |= TCPC_V10_REG_ALERT_TX_SUCCESS | TCPC_V10_REG_ALERT_TX_DISCARDED
			| TCPC_V10_REG_ALERT_TX_FAILED
			| TCPC_V10_REG_ALERT_RX_HARD_RST
			| TCPC_V10_REG_ALERT_RX_STATUS
			| TCPC_V10_REG_RX_OVERFLOW;
#endif

    mask |= TCPC_REG_ALERT_FAULT;

    return sc2150_i2c_write16(tcpc, TCPC_V10_REG_ALERT_MASK, mask);
}

static int sc2150_init_iicrst(struct tcpc_device *tcpc)
{
    const uint8_t mask = SC2150_IICRST_EN;

    return sc2150_i2c_write8(tcpc,
            SC2150_REG_ANA_CTRL3, mask);
}

static int sc2150_init_power_status_mask(struct tcpc_device *tcpc)
{
    const uint8_t mask = TCPC_V10_REG_POWER_STATUS_VBUS_PRES;

    return sc2150_i2c_write8(tcpc,
            TCPC_V10_REG_POWER_STATUS_MASK, mask);
}

static int sc2150_init_fault_mask(struct tcpc_device *tcpc)
{
    const uint8_t mask =
        TCPC_V10_REG_FAULT_STATUS_VCONN_OV |
        TCPC_V10_REG_FAULT_STATUS_VCONN_OC;

    return sc2150_i2c_write8(tcpc,
            TCPC_V10_REG_FAULT_STATUS_MASK, mask);
}

static inline int sc2150_init_prv_mask(struct tcpc_device *tcpc)
{
    return sc2150_i2c_write8(tcpc, SC2150_REG_ANA_MASK, SC2150_REG_MASK_VBUS_80);
}

static irqreturn_t sc2150_intr_handler(int irq, void *data)
{
	struct sc2150 *sc = data;
#ifdef SUPPORT_SOUTHCHIP_CPU_BOOST
	sc_cpufreq_update(CPU_CHG_FREQ_STAT_UP);
#endif
	pm_wakeup_event(sc->dev, SC2150_IRQ_WAKE_TIME);

    tcpci_lock_typec(sc->tcpc);
    tcpci_alert(sc->tcpc);
    tcpci_unlock_typec(sc->tcpc);

    return IRQ_HANDLED;
}

static int sc2150_init_alert(struct tcpc_device *tcpc)
{
    struct sc2150 *sc = tcpc_get_dev_data(tcpc);
    int ret = 0;
    char *name = NULL;

    /* Clear Alert Mask & Status */
    sc2150_write_word(sc->client, TCPC_V10_REG_ALERT_MASK, 0);
    sc2150_write_word(sc->client, TCPC_V10_REG_ALERT, 0xffff);
    sc2150_reg_write(sc->client, SC2150_REG_ANA_MASK, 0);
    sc2150_reg_write(sc->client, SC2150_REG_ANA_INT, 0xff);
    sc2150_reg_write(sc->client, TCPC_V10_REG_FAULT_STATUS, 0xff);

    name = devm_kasprintf(sc->dev, GFP_KERNEL, "%s-IRQ",
                sc->tcpc_desc->name);
    if (!name)
        return -ENOMEM;

    dev_info(sc->dev, "%s name = %s, gpio = %d\n",
                __func__, sc->tcpc_desc->name, sc->irq_gpio);

    ret = devm_gpio_request(sc->dev, sc->irq_gpio, name);

    if (ret < 0) {
        dev_notice(sc->dev, "%s request GPIO fail(%d)\n",
                    __func__, ret);
        return ret;
    }

    ret = gpio_direction_input(sc->irq_gpio);
    if (ret < 0) {
        dev_notice(sc->dev, "%s set GPIO fail(%d)\n", __func__, ret);
        return ret;
    }

    ret = gpio_to_irq(sc->irq_gpio);
    if (ret < 0) {
        dev_notice(sc->dev, "%s gpio to irq fail(%d)",
                    __func__, ret);
        return ret;
    }
    sc->irq = ret;

    dev_info(sc->dev, "%s IRQ number = %d\n", __func__, sc->irq);

    ret = devm_request_threaded_irq(sc->dev, sc->irq, NULL,
                    sc2150_intr_handler,
                    IRQF_TRIGGER_LOW | IRQF_ONESHOT,
                    name, sc);
    if (ret < 0) {
        dev_notice(sc->dev, "%s request irq fail(%d)\n",
                    __func__, ret);
        return ret;
    }
    device_init_wakeup(sc->dev, true);

    return 0;
}

int sc2150_alert_status_clear(struct tcpc_device *tcpc, uint32_t mask)
{
    int ret;
    uint16_t mask_t1;
    uint8_t mask_t2;

    mask_t1 = mask;
    if (mask_t1) {
        ret = sc2150_i2c_write16(tcpc, TCPC_V10_REG_ALERT, mask_t1);
        if (ret < 0)
            return ret;
    }
    mask_t2 = mask >> 16;
    if (mask_t2) {
        ret = sc2150_i2c_write8(tcpc, SC2150_REG_ANA_INT, mask_t2);
        if (ret < 0)
            return ret;
    }
    return 0;
}

static int sc2150_tcpc_init(struct tcpc_device *tcpc, bool sw_reset)
{
    int ret;

    if (sw_reset) {
        ret = sc2150_software_reset(tcpc);
        if (ret < 0)
            return ret;
    }
    sc2150_init_iicrst(tcpc);
    /* UFP Both RD setting */
    /* DRP = 0, RpVal = 0 (Default), Rd, Rd */
    sc2150_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL,
        TCPC_V10_REG_ROLE_CTRL_RES_SET(0, 0, CC_RD, CC_RD));

    sc2150_i2c_write8(tcpc, TCPC_V10_REG_COMMAND,
        TCPM_CMD_ENABLE_VBUS_DETECT);

    /*
    * DRP Toggle Cycle : 51.2 + 6.4*val ms
    * DRP Duty Ctrl : dcSRC / 1024
    */
    sc2150_i2c_write8(tcpc, SC2150_REG_DRP_CTRL, 4);
    sc2150_i2c_write16(tcpc,
        SC2150_REG_DRP_DUTY_CTRL, TCPC_NORMAL_RP_DUTY);

    sc2150_alert_status_clear(tcpc, 0xffffffff);

    sc2150_init_power_status_mask(tcpc);
    sc2150_init_alert_mask(tcpc);
    sc2150_init_fault_mask(tcpc);
    sc2150_init_prv_mask(tcpc);

    /* shutdown off */
    sc2150_i2c_write8(tcpc, SC2150_REG_ANA_CTRL2, SC2150_REG_SHUTDOWN_OFF);
    mdelay(1);

    return 0;
}

#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
static int sc2150_get_chip_id(struct tcpc_device *tcpc,uint32_t *id) 
{
    struct sc2150 *sc = tcpc_get_dev_data(tcpc);
    *id = sc->chip_id;
    return 0;
}

static int sc2150_get_chip_pid(struct tcpc_device *tcpc,uint32_t *pid)
{
    struct sc2150 *sc = tcpc_get_dev_data(tcpc);
    *pid = sc->chip_pid;
    return 0;
}

static int sc2150_get_chip_vid(struct tcpc_device *tcpc,uint32_t *vid) 
{
    struct sc2150 *sc = tcpc_get_dev_data(tcpc);
    *vid = sc->chip_vid;
    return 0;
}
#endif /* CONFIG_SUPPORT_SOUTHCHIP_PDPHY */

static inline int sc2150_fault_status_vconn_ov(struct tcpc_device *tcpc)
{
    int ret;

    ret = sc2150_i2c_read8(tcpc, SC2150_REG_ANA_CTRL1);
    if (ret < 0)
        return ret;

    ret &= ~SC2150_REG_VCONN_DISCHARGE_EN;
    return sc2150_i2c_write8(tcpc, SC2150_REG_ANA_CTRL1, ret);
}

static int sc2150_set_vconn(struct tcpc_device *tcpc, int enable);
static int sc2150_fault_status_clear(struct tcpc_device *tcpc, uint8_t status)
{
    int ret;

    if (status & TCPC_V10_REG_FAULT_STATUS_VCONN_OV)
        ret = sc2150_fault_status_vconn_ov(tcpc);
    if (status & TCPC_V10_REG_FAULT_STATUS_VCONN_OC)
        ret = sc2150_set_vconn(tcpc, false);

    sc2150_i2c_write8(tcpc, TCPC_V10_REG_FAULT_STATUS, status);
    return 0;
}

static int sc2150_get_alert_mask(struct tcpc_device *tcpc, uint32_t *mask)
{
    int ret;
    uint8_t v2;

    ret = sc2150_i2c_read16(tcpc, TCPC_V10_REG_ALERT_MASK);
    if (ret < 0)
        return ret;

    *mask = (uint16_t) ret;

    ret = sc2150_i2c_read8(tcpc, SC2150_REG_ANA_MASK);
    if (ret < 0)
        return ret;

    v2 = (uint8_t) ret;
    *mask |= v2 << 16;

    return 0;
}

int sc2150_get_alert_status(struct tcpc_device *tcpc, uint32_t *alert)
{
    int ret;
    uint8_t v2;

    ret = sc2150_i2c_read16(tcpc, TCPC_V10_REG_ALERT);
    if (ret < 0)
        return ret;

    *alert = (uint16_t) ret;

    ret = sc2150_i2c_read8(tcpc, SC2150_REG_ANA_INT);
    if (ret < 0)
        return ret;

    v2 = (uint8_t) ret;
    *alert |= v2 << 16;

    return 0;
}

static int sc2150_get_power_status(
        struct tcpc_device *tcpc, uint16_t *pwr_status)
{
    int ret;

    ret = sc2150_i2c_read8(tcpc, TCPC_V10_REG_POWER_STATUS);
    if (ret < 0)
        return ret;

    *pwr_status = 0;

    if (ret & TCPC_V10_REG_POWER_STATUS_VBUS_PRES)
        *pwr_status |= TCPC_REG_POWER_STATUS_VBUS_PRES;

    ret = sc2150_i2c_read8(tcpc, SC2150_REG_ANA_STATUS);
    if (ret < 0)
        return ret;

    if (ret & SC2150_REG_VBUS_80)
        *pwr_status |= TCPC_REG_POWER_STATUS_EXT_VSAFE0V;

    return 0;
}

int sc2150_get_fault_status(struct tcpc_device *tcpc, uint8_t *status)
{
    int ret;

    ret = sc2150_i2c_read8(tcpc, TCPC_V10_REG_FAULT_STATUS);
    if (ret < 0)
        return ret;
    *status = (uint8_t) ret;
    return 0;
}

static int sc2150_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2)
{
    int status, role_ctrl, cc_role;
    bool act_as_sink, act_as_drp;

    status = sc2150_i2c_read8(tcpc, TCPC_V10_REG_CC_STATUS);
    if (status < 0)
        return status;

    role_ctrl = sc2150_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL);
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

    /**
    * cc both connect
    */
    if (act_as_drp && act_as_sink) {
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

    return 0;
}

static int sc2150_enable_vsafe0v_detect(
    struct tcpc_device *tcpc, bool enable)
{
    int ret = sc2150_i2c_read8(tcpc, SC2150_REG_ANA_MASK);

    if (ret < 0)
        return ret;

    if (enable)
        ret |= SC2150_REG_MASK_VBUS_80;
    else
        ret &= ~SC2150_REG_MASK_VBUS_80;

    return sc2150_i2c_write8(tcpc, SC2150_REG_ANA_MASK, (uint8_t) ret);
}

static int sc2150_set_cc(struct tcpc_device *tcpc, int pull)
{
    int ret = 0;
    uint8_t data = 0, old_data = 0;
    int rp_lvl = TYPEC_CC_PULL_GET_RP_LVL(pull), pull1, pull2;

    pull = TYPEC_CC_PULL_GET_RES(pull);

    old_data = sc2150_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL);

    if (pull == TYPEC_CC_DRP) {
        data = TCPC_V10_REG_ROLE_CTRL_RES_SET(
                1, rp_lvl, TYPEC_CC_RP, TYPEC_CC_RP);

        if (old_data != data) {
            ret = sc2150_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, data);
        }

        if (ret == 0) {
            sc2150_enable_vsafe0v_detect(tcpc, false);
            ret = sc2150_command(tcpc, TCPM_CMD_LOOK_CONNECTION);
        }
    } else {
        pull1 = pull2 = pull;

        if (pull == TYPEC_CC_RP && tcpc->typec_is_attached_src) {
            if (tcpc->typec_polarity)
                pull1 = TYPEC_CC_OPEN;
            else
                pull2 = TYPEC_CC_OPEN;
        }
        data = TCPC_V10_REG_ROLE_CTRL_RES_SET(0, rp_lvl, pull1, pull2);
        if (old_data != data) {
            ret = sc2150_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, data);
        }
    }

    return 0;
}

static int sc2150_set_polarity(struct tcpc_device *tcpc, int polarity)
{
    int data;

    data = sc2150_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
    if (data < 0)
        return data;

    data &= ~TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT;
    data |= polarity ? TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT : 0;

    return sc2150_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}

static int sc2150_set_low_rp_duty(struct tcpc_device *tcpc, bool low_rp)
{
    uint16_t duty = low_rp ? TCPC_LOW_RP_DUTY : TCPC_NORMAL_RP_DUTY;

    return sc2150_i2c_write16(tcpc, SC2150_REG_DRP_DUTY_CTRL, duty);
}

static int sc2150_set_vconn(struct tcpc_device *tcpc, int enable)
{
    int rv;
    int data;

    data = sc2150_i2c_read8(tcpc, TCPC_V10_REG_POWER_CTRL);
    if (data < 0)
        return data;

    data &= ~TCPC_V10_REG_POWER_CTRL_VCONN;
    data |= enable ? TCPC_V10_REG_POWER_CTRL_VCONN : 0;

    rv = sc2150_i2c_write8(tcpc, TCPC_V10_REG_POWER_CTRL, data);
    if (rv < 0)
        return rv;

    return rv;
}

#ifdef CONFIG_TCPC_LOW_POWER_MODE
static int sc2150_is_low_power_mode(struct tcpc_device *tcpc)
{
    int rv = sc2150_i2c_read8(tcpc, SC2150_REG_ANA_CTRL1);
    if (rv < 0)
        return rv;

    return (rv & SC2150_REG_LPM_EN) != 0;
}

static int sc2150_set_low_power_mode(
        struct tcpc_device *tcpc, bool en, int pull)
{
    uint8_t data = 0;

    sc2150_enable_vsafe0v_detect(tcpc, !en);
    data = sc2150_i2c_read8(tcpc, SC2150_REG_ANA_CTRL1);
    if(data < 0) {
        return data;
    }
    if(en) {
        data |= SC2150_REG_LPM_EN;
    } else {
        data &= ~SC2150_REG_LPM_EN;
    }

    return sc2150_i2c_write8(tcpc, SC2150_REG_ANA_CTRL1, data);
}
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

#ifdef CONFIG_TCPC_WATCHDOG_EN
int sc2150_set_watchdog(struct tcpc_device *tcpc, bool en)
{
    uint8_t data = 0;

    data = sc2150_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
    if (data < 0)
        return data;
    if (en) {
        data |= TCPC_V10_REG_TCPC_CTRL_EN_WDT;
    } else {
        data &= (~TCPC_V10_REG_TCPC_CTRL_EN_WDT);
    }
    SC2150_INFO("%s set watchdog %d\n", __func__, en);
    return sc2150_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}
#endif	/* CONFIG_TCPC_WATCHDOG_EN */

static int sc2150_tcpc_deinit(struct tcpc_device *tcpc)
{
#ifdef CONFIG_RT_REGMAP
    struct sc2150 *sc = tcpc_get_dev_data(tcpc);
#endif /* CONFIG_RT_REGMAP */

#ifdef CONFIG_TCPC_SHUTDOWN_CC_DETACH
    sc2150_set_cc(tcpc, TYPEC_CC_DRP);
    sc2150_set_cc(tcpc, TYPEC_CC_OPEN);
    mdelay(150);
    sc2150_i2c_write8(tcpc, SC2150_REG_RST_CTRL, 1);
#else
    sc2150_i2c_write8(tcpc, SC2150_REG_RST_CTRL, 1);
#endif	/* CONFIG_TCPC_SHUTDOWN_CC_DETACH */
#ifdef CONFIG_RT_REGMAP
    rt_regmap_cache_reload(sc->m_dev);
#endif /* CONFIG_RT_REGMAP */

    return 0;
}

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
static int sc2150_set_msg_header(
    struct tcpc_device *tcpc, uint8_t power_role, uint8_t data_role)
{
    uint8_t msg_hdr = TCPC_V10_REG_MSG_HDR_INFO_SET(data_role, power_role);

    return sc2150_i2c_write8(
        tcpc, TCPC_V10_REG_MSG_HDR_INFO, msg_hdr);
}

static int sc2150_set_rx_enable(struct tcpc_device *tcpc, uint8_t enable)
{
    return sc2150_i2c_write8(tcpc, TCPC_V10_REG_RX_DETECT, enable);
}

static int sc2150_set_bist_test_mode(struct tcpc_device *tcpc, bool en);
static int sc2150_get_message(struct tcpc_device *tcpc, uint32_t *payload,
            uint16_t *msg_head, enum tcpm_transmit_type *frame_type)
{
	struct sc2150 *sc = tcpc_get_dev_data(tcpc);
	int rv = 0;
	uint8_t cnt = 0, buf[4];

	rv = sc2150_block_read(sc->client, TCPC_V10_REG_RX_BYTE_CNT, 4, buf);
	if (rv < 0)
		return rv;

	cnt = buf[0];
	*frame_type = buf[1];
	*msg_head = le16_to_cpu(*(uint16_t *)&buf[2]);

#ifndef CONFIG_USB_PD_ONLY_PRINT_SYSTEM_BUSY
	if (PD_DATA_BIST == PD_HEADER_TYPE(*msg_head) && cnt > 3){
		sc2150_set_bist_test_mode(tcpc, true);
		SC2150_INFO("%s \n", __func__);
	}
		
#endif /* CONFIG_USB_PD_ONLY_PRINT_SYSTEM_BUSY */

	/* TCPC 1.0 ==> no need to subtract the size of msg_head */
	if (cnt > 3) {
		cnt -= 3; /* MSG_HDR */
		rv = sc2150_block_read(sc->client, TCPC_V10_REG_RX_DATA, cnt,
				       payload);
	}

	return rv;
}

#pragma pack(push, 1)
struct tcpc_transmit_packet {
    uint8_t cnt;
    uint16_t msg_header;
    uint8_t data[sizeof(uint32_t)*7];
};
#pragma pack(pop)

static int sc2150_transmit(struct tcpc_device *tcpc,
    enum tcpm_transmit_type type, uint16_t header, const uint32_t *data)
{
    struct sc2150 *sc = tcpc_get_dev_data(tcpc);
    int rv;
    int data_cnt;
    struct tcpc_transmit_packet packet;

    if (type < TCPC_TX_HARD_RESET) {
        data_cnt = sizeof(uint32_t) * PD_HEADER_CNT(header);

        packet.cnt = data_cnt + sizeof(uint16_t);
        packet.msg_header = header;

        packet.cnt += 4;
        
        if (data_cnt > 0)
            memcpy(packet.data, (uint8_t *) data, data_cnt);		
        /* Length need add CRC(4)*/
		rv = sc2150_block_write(sc->client,
				TCPC_V10_REG_TX_BYTE_CNT,
				packet.cnt - 3, (uint8_t *) &packet);
		if (rv < 0)
			return rv;
		SC2150_INFO("%s data_cnt:%d header:%04X data:%08X\n", __func__, packet.cnt, header, *packet.data);
	}
	return sc2150_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
			TCPC_V10_REG_TRANSMIT_SET(
			tcpc->pd_retry_count, type));
}

static int sc2150_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
	int data = 0;
	
	data = sc2150_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
	if (data < 0)
		return data;

    data &= ~TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE;
    data |= en ? TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE : 0;

    return sc2150_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}
#endif /* CONFIG_USB_POWER_DELIVERY */

static struct tcpc_ops sc2150_tcpc_ops = {
    .init = sc2150_tcpc_init,
    .alert_status_clear = sc2150_alert_status_clear,
    .fault_status_clear = sc2150_fault_status_clear,
    .get_alert_mask = sc2150_get_alert_mask,
    .get_alert_status = sc2150_get_alert_status,
    .get_power_status = sc2150_get_power_status,
    .get_fault_status = sc2150_get_fault_status,
#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
    .get_chip_id = sc2150_get_chip_id,
    .get_chip_vid = sc2150_get_chip_vid,
    .get_chip_pid = sc2150_get_chip_pid,
#endif /* CONFIG_SUPPORT_SOUTHCHIP_PDPHY */
    .get_cc = sc2150_get_cc,
    .set_cc = sc2150_set_cc,
    .set_polarity = sc2150_set_polarity,
    .set_low_rp_duty = sc2150_set_low_rp_duty,
    .set_vconn = sc2150_set_vconn,
    .deinit = sc2150_tcpc_deinit,

#ifdef CONFIG_TCPC_LOW_POWER_MODE
    .is_low_power_mode = sc2150_is_low_power_mode,
    .set_low_power_mode = sc2150_set_low_power_mode,
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

#ifdef CONFIG_TCPC_WATCHDOG_EN
    .set_watchdog = sc2150_set_watchdog,
#endif	/* CONFIG_TCPC_WATCHDOG_EN */

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	.set_msg_header = sc2150_set_msg_header,
	.set_rx_enable = sc2150_set_rx_enable,
	.get_message = sc2150_get_message,
	.transmit = sc2150_transmit,
	.set_bist_test_mode = sc2150_set_bist_test_mode,
#endif	/* CONFIG_USB_POWER_DELIVERY */
};

static int sc2150_parse_dt(struct sc2150 *sc, struct device *dev)
{
    struct device_node *np = dev->of_node;
    struct tcpc_desc *desc;
    u32 val, len;
    const char *name = "default";
    int ret = 0;

    desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
    if (!desc)
        return -ENOMEM;
    
    sc->irq_gpio = of_get_named_gpio(np, "sc2150,intr-gpio", 0);

    if (ret < 0)
        pr_err("%s no intr_gpio info\n", __func__);

    dev_info(dev, "tcpc irq = %d\n", sc->irq_gpio);

    if (of_property_read_u32(np, "sc-tcpc,role_def", &val) >= 0) {
        if (val >= TYPEC_ROLE_NR)
            desc->role_def = TYPEC_ROLE_DRP;
        else
            desc->role_def = val;
    } else {
        dev_info(dev, "use default Role DRP\n");
        desc->role_def = TYPEC_ROLE_DRP;
    }

    if (of_property_read_u32(np, "sc-tcpc,rp_level", &val) >= 0) {
        switch (val) {
        case TYPEC_RP_DFT:
        case TYPEC_RP_1_5:
        case TYPEC_RP_3_0:
            desc->rp_lvl = val;
            break;
        default:
            break;
        }
    }

#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
    if (of_property_read_u32(np, "sc-tcpc,vconn_supply", &val) >= 0) {
        if (val >= TCPC_VCONN_SUPPLY_NR)
            desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
        else
            desc->vconn_supply = val;
    } else {
        dev_info(dev, "use default VconnSupply\n");
        desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
    }
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

    if (of_property_read_string(np, "sc-tcpc,name",
                (char const **)&name) < 0) {
        dev_info(dev, "use default name\n");
    }

    len = strlen(name);
    desc->name = kzalloc(len+1, GFP_KERNEL);
    if (!desc->name)
        return -ENOMEM;

    strlcpy((char *)desc->name, name, len+1);

    sc->tcpc_desc = desc;

    return ret < 0 ? ret : 0;
}

static int sc2150_tcpcdev_init(struct sc2150 *sc, struct device *dev)
{
    struct device_node *np = dev->of_node;

    dev_info(dev, "%s\n", __func__);

    sc->tcpc = tcpc_device_register(dev,
            sc->tcpc_desc, &sc2150_tcpc_ops, sc);
    if (IS_ERR_OR_NULL(sc->tcpc))
        return -EINVAL;

#ifdef CONFIG_USB_PD_DISABLE_PE
    sc->tcpc->disable_pe =
            of_property_read_bool(np, "sc-tcpc,disable_pe");
#endif	/* CONFIG_USB_PD_DISABLE_PE */

    sc->tcpc->tcpc_flags = TCPC_FLAGS_LPM_WAKEUP_WATCHDOG |
            TCPC_FLAGS_VCONN_SAFE5V_ONLY;

#ifdef CONFIG_USB_PD_REV30
    sc->tcpc->tcpc_flags |= TCPC_FLAGS_PD_REV30;
    dev_info(dev, "PD_REV30\n");
#endif	/* CONFIG_USB_PD_REV30 */
    sc->tcpc->tcpc_flags |= TCPC_FLAGS_ALERT_V10;

    return 0;
}

static inline int sc2150_check_revision(struct i2c_client *client)
{
    u16 vid, pid, did;
    int ret;
    u8 data = 1;
    struct sc2150 *sc = i2c_get_clientdata(client);

    ret = sc2150_read_device(client, TCPC_V10_REG_VID, 2, &vid);
    if (ret < 0) {
        dev_err(&client->dev, "read chip ID fail(%d)\n", ret);
        return -EIO;
    }

    if (vid != SOUTHCHIP_PD_VID) { 
        pr_info("%s failed, VID=0x%04x\n", __func__, vid);
        return -ENODEV;
    }
    sc->chip_vid = vid;

    ret = sc2150_read_device(client, TCPC_V10_REG_PID, 2, &pid);
    if (ret < 0) {
        dev_err(&client->dev, "read product ID fail(%d)\n", ret);
        return -EIO;
    }

    if (pid != SC2150_PID) {
        pr_info("%s failed, PID=0x%04x\n", __func__, pid);
        return -ENODEV;
    }
    sc->chip_pid = pid;

    // close watchdog
    ret = sc2150_write_device(client, TCPC_V10_REG_TCPC_CTRL, 1, &data);
    ret |= sc2150_write_device(client, SC2150_REG_RST_CTRL, 1, &data);
    if (ret < 0)
        return ret;

    usleep_range(1000, 2000);

    ret = sc2150_read_device(client, TCPC_V10_REG_DID, 2, &did);
    if (ret < 0) {
        dev_err(&client->dev, "read device ID fail(%d)\n", ret);
        return -EIO;
    }

    return did;
}

static int sc2150_i2c_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
	struct sc2150 *sc;
	int ret = 0, chip_id;
	bool use_dt = client->dev.of_node;

	pr_info("%s (%s)\n", __func__, SC2150_DRV_VER);
	if (i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_I2C_BLOCK | I2C_FUNC_SMBUS_BYTE_DATA))
		pr_info("I2C functionality : OK...\n");
	else
		pr_info("I2C functionality check : failuare...\n");

	sc = devm_kzalloc(&client->dev, sizeof(*sc), GFP_KERNEL);
	if (!sc)
		return -ENOMEM;

	sc->dev = &client->dev;
	sc->client = client;
	i2c_set_clientdata(client, sc);
	chip_id = sc2150_check_revision(client);
	if (chip_id < 0)
		return chip_id;
	
	sc->chip_id = chip_id;

	pr_info("chip info [0x%0x,0x%0x,0x%0x]\n", sc->chip_vid, 
                            sc->chip_pid, sc->chip_id);
    if (use_dt) {
        ret = sc2150_parse_dt(sc, &client->dev);
        if (ret < 0)
            return ret;
    } else {
        dev_err(&client->dev, "no dts node\n");
        return -ENODEV;
    }

    ret = sc2150_regmap_init(sc);
    if (ret < 0) {
        dev_err(sc->dev, "sc2150 regmap init fail\n");
        goto err_regmap_init;
    }

    ret = sc2150_tcpcdev_init(sc, &client->dev);
    if (ret < 0) {
        dev_err(&client->dev, "sc2150 tcpc dev init fail\n");
        goto err_tcpc_reg;
    }

    ret = sc2150_init_alert(sc->tcpc);
    if (ret < 0) {
        pr_err("sc2150 init alert fail\n");
        goto err_irq_init;
    }
    pr_info("%s probe OK!\n", __func__);
    return 0;

err_irq_init:
    tcpc_device_unregister(sc->dev, sc->tcpc);
err_tcpc_reg:
    sc2150_regmap_deinit(sc);
err_regmap_init:
    return ret;
}

static int sc2150_i2c_remove(struct i2c_client *client)
{
    struct sc2150 *sc = i2c_get_clientdata(client);

    if (sc) {
        tcpc_device_unregister(sc->dev, sc->tcpc);
        sc2150_regmap_deinit(sc);
    }

    return 0;
}

#ifdef CONFIG_PM
static int sc2150_i2c_suspend(struct device *dev)
{
    struct sc2150 *sc = dev_get_drvdata(dev);

    dev_info(dev, "%s\n", __func__);
    if (device_may_wakeup(dev))
        enable_irq_wake(sc->irq);
    disable_irq(sc->irq);

    return 0;
}

static int sc2150_i2c_resume(struct device *dev)
{
    struct sc2150 *sc = dev_get_drvdata(dev);

    dev_info(dev, "%s\n", __func__);
    enable_irq(sc->irq);
    if (device_may_wakeup(dev))
        disable_irq_wake(sc->irq);

    return 0;
}

static const struct dev_pm_ops sc2150_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(
            sc2150_i2c_suspend,
            sc2150_i2c_resume)
};
#endif /* CONFIG_PM */

static void sc2150_shutdown(struct i2c_client *client)
{
    struct sc2150 *sc = i2c_get_clientdata(client);

    /* Please reset IC here */
    if (sc != NULL) {
        if (sc->irq)
            disable_irq(sc->irq);
        tcpm_shutdown(sc->tcpc);
    } else {
        i2c_smbus_write_byte_data(
            client, SC2150_REG_RST_CTRL, 0x01);
    }
}

static const struct i2c_device_id sc2150_id_table[] = {
    {"sc2150", 0},
    {},
};
MODULE_DEVICE_TABLE(i2c, sc2150_id_table);

static const struct of_device_id sc_match_table[] = {
    {.compatible = "southchip,sc2150",},
    {},
};

static struct i2c_driver sc2150_driver = {
    .driver = {
        .name = "sc2150-driver",
        .owner = THIS_MODULE,
        .of_match_table = sc_match_table,
        .pm = &sc2150_pm_ops,
    },
    .probe = sc2150_i2c_probe,
    .remove = sc2150_i2c_remove,
    .shutdown = sc2150_shutdown,
    .id_table = sc2150_id_table,
};

module_i2c_driver(sc2150_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Boqiang Liu <air-liu@southchip.com>");
MODULE_DESCRIPTION("SC2150 TCPC Driver");
MODULE_VERSION(SC2150_DRV_VER);







