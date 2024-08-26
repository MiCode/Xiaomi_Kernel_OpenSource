#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/sched/prio.h>
#include <uapi/linux/sched/types.h>
#include <linux/platform_device.h>

#include "ufcs/inc/ufcsi.h"

#define subpmic_ufcs_slave_addr        0x01

#define SUBPMIC_ACK_RECEIVE_TIMEOUT_FLAG_MASK        0x01
#define SUBPMIC_ACK_RECEIVE_TIMEOUT_FLAG_SHIFT       0

#define SUBPMIC_MSG_TRANS_FAIL_FLAG_MASK             0x02
#define SUBPMIC_MSG_TRANS_FAIL_FLAG_SHIFT            1

#define SUBPMIC_RX_BUFFER_BUSY_FLAG_MASK             0x04
#define SUBPMIC_RX_BUFFER_BUSY_FLAG_SHIFT            2

#define SUBPMIC_RX_OVERFLOW_FLAG_MASK                0x08
#define SUBPMIC_RX_OVERFLOW_FLAG_SHIFT               3

#define SUBPMIC_DATA_READY_FLAG_MASK                 0x10
#define SUBPMIC_DATA_READY_FLAG_SHIFT                4

#define SUBPMIC_SENT_PACKET_COMPLETE_FLAG_MASK       0x20
#define SUBPMIC_SENT_PACKET_COMPLETE_FLAG_SHIFT      5

#define SUBPMIC_UFCS_HANDSHAKE_SUCC_FLAG_MASK        0x40
#define SUBPMIC_UFCS_HANDSHAKE_SUCC_FLAG_SHIFT       6

#define SUBPMIC_UFCS_HANDSHAKE_FAIL_FLAG_MASK        0x80
#define SUBPMIC_UFCS_HANDSHAKE_FAIL_FLAG_SHIFT       7

#define SUBPMIC_HARD_RESET_FLAG_MASK                 0x01
#define SUBPMIC_HARD_RESET_FLAG_SHIFT                0

#define SUBPMIC_CRC_ERROR_FLAG_MASK                  0x02
#define SUBPMIC_CRC_ERROR_FLAG_SHIFT                 1

#define SUBPMIC_STOP_ERROR_FLAG_MASK                 0x04
#define SUBPMIC_STOP_ERROR_FLAG_SHIFT                2

#define SUBPMIC_START_FAIL_FLAG_MASK                 0x08
#define SUBPMIC_START_FAIL_FLAG_SHIFT                3

#define SUBPMIC_LENGTH_ERROR_FLAG_MASK               0x10
#define SUBPMIC_LENGTH_ERROR_FLAG_SHIFT              4

#define SUBPMIC_DATA_BYTE_TMOUT_FLAG_MASK            0x20
#define SUBPMIC_DATA_BYTE_TMOUT_FLAG_SHIFT           5

#define SUBPMIC_TRAINING_BYTES_ERROR_FLAG_MASK       0x40
#define SUBPMIC_TRAINING_BYTES_ERROR_FLAG_SHIFT      6

#define SUBPMIC_BAUD_RATE_ERROR_FLAG_MASK            0x80
#define SUBPMIC_BAUD_RATE_ERROR_FLAG_SHIFT           7

#define SUBPMIC_RX_BUSY_MASK                         0x01
#define SUBPMIC_RX_BUSY_SHIFT                        0

#define SUBPMIC_TX_BUSY_MASK                         0x02
#define SUBPMIC_TX_BUSY_SHIFT                        1

#define SUBPMIC_DATA_BIT_ERR_FLAG_MASK               0x20
#define SUBPMIC_DATA_BIT_ERR_FLAG_SHIFT              5

#define SUBPMIC_BAUDRATE_CHG_FLAG_MASK               0x40
#define SUBPMIC_BAUDRATE_CHG_FLAG_SHIFT              6

#define SUBPMIC_BUS_CONFLICT_FLAG_MASK               0x80
#define SUBPMIC_BUS_CONFLICT_FLAG_SHIFT              7

//reg define
enum {
    UFCS_CTRL1      = 0x00,
    UFCS_CTRL2      = 0x0101,
    UFCS_INT_FLAG1  = 0x0103,
    TX_LENGTH       = 0x0109,
    TX_BUFFER0      = 0x010A,
    RX_LENGTH       = 0x012E,
    RX_BUFFER0      = 0x012F,

};

//reg field
enum subpmic_ufcs_fields{
    //reg00
    UFCS_EN, UFCS_HANDSHAKE, SND_CMD, CABLE_HARDRESET, 
    SOURCE_HARDRESET,
    //reg01
    ACK_CABLE,

    F_MAX_FIELDS,
};

struct subpmic_ufcs_device {
    struct i2c_client *client;
    struct device *dev;
    struct regmap *rmap;
    struct regmap_field *rmap_fields[F_MAX_FIELDS];
    int irq;
    
    struct ufcs_desc *ufcs_cfg_desc;
    struct ufcs_dev ufcs;
};

#define UFCS_REG_FIELD(reg, n, m) \
            REG_FIELD(reg + (0x01<<8), n , m)

static const struct reg_field ufcs_reg_fields[] = {
    [UFCS_EN]               = UFCS_REG_FIELD(UFCS_CTRL1, 7, 7),
    [UFCS_HANDSHAKE]        = UFCS_REG_FIELD(UFCS_CTRL1, 5, 5),
    [SND_CMD]               = UFCS_REG_FIELD(UFCS_CTRL1, 2, 2),
    [CABLE_HARDRESET]       = UFCS_REG_FIELD(UFCS_CTRL1, 1, 1),
    [SOURCE_HARDRESET]      = UFCS_REG_FIELD(UFCS_CTRL1, 0, 0),
    [ACK_CABLE]             = UFCS_REG_FIELD(UFCS_CTRL2, 1, 1),

};

//------------------default config----------------
struct ufcs_desc subpmic_default_desc = {
    .name = "ufcs_port2",
    .default_baudrate = BAUDRATE_115200,
    .default_request_volt = 5000,
    .default_request_curr = 2000,
};

static int subpmic_ufcs_field_write(struct subpmic_ufcs_device *pdata,
			       enum subpmic_ufcs_fields field_id, u8 val)
{
    int ret = 0;
    ret = regmap_field_write(pdata->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(pdata->dev, "i2c field write failed\n");
    }
    return ret;
}
/******************** UFCS API ***************************/
static struct subpmic_ufcs_device *ufcs_dev_get_chip(struct ufcs_dev *ufcs)
{
    if(IS_ERR(ufcs))
        return ERR_PTR(-EINVAL);
    return ufcs->drv_data;
}

static int subpmic_ufcs_get_int_flag(struct ufcs_dev *ufcs, uint32_t *flag);
static int subpmic_ufcs_init(struct ufcs_dev *ufcs) 
{
    struct subpmic_ufcs_device *pdata = ufcs_dev_get_chip(ufcs);
    uint32_t flag = 0;
    if(IS_ERR(pdata))
        return -EINVAL;
    dev_err(pdata->dev, "subpmic_ufcs_init\n");
    subpmic_ufcs_get_int_flag(ufcs, &flag);
    return subpmic_ufcs_field_write(pdata, UFCS_EN, true);
}

static int subpmic_ufcs_handshake(struct ufcs_dev *ufcs)
{
    int ret = 0;
    struct subpmic_ufcs_device *pdata = ufcs_dev_get_chip(ufcs);
    if(IS_ERR(pdata))
        return -EINVAL;
    dev_err(pdata->dev, "subpmic_ufcs_handshake\n");
    ret = subpmic_ufcs_field_write(pdata, UFCS_HANDSHAKE, true);
    //ret |= subpmic_ufcs_field_write(pdata, UFCS_HANDSHAKE, false);

    return ret;
}

static int subpmic_ufcs_exit(struct ufcs_dev *ufcs)
{
    int ret = 0;
    struct subpmic_ufcs_device *pdata = ufcs_dev_get_chip(ufcs);
    if(IS_ERR(pdata))
        return -EINVAL;

    ret = subpmic_ufcs_field_write(pdata, UFCS_HANDSHAKE, false);
    ret |= subpmic_ufcs_field_write(pdata, UFCS_EN, false);

    return ret;     
}

static int subpmic_ufcs_set_baudrate(struct ufcs_dev *ufcs, uint8_t baudrate)
{
    return 0;
}

static int subpmic_ufcs_get_baudrate(struct ufcs_dev *ufcs, uint8_t *baudrate)
{
    return 0;
}

static int subpmic_ufcs_get_int_flag(struct ufcs_dev *ufcs, uint32_t *flag)
{
    int ret = 0;
    uint8_t val[3];
    struct subpmic_ufcs_device *pdata = ufcs_dev_get_chip(ufcs);
    if(IS_ERR(pdata))
        return -EINVAL;   

    ret = regmap_bulk_read(pdata->rmap, UFCS_INT_FLAG1, val, 3);
    if (ret < 0)
        return ret;
    dev_info(pdata->dev, "irqflag1:0x%x, irqflag2:0x%x, irqflag3:0x%x\n",
                         val[0], val[1], val[2]);
    *flag = 0;

    if (val[0] & SUBPMIC_ACK_RECEIVE_TIMEOUT_FLAG_MASK)
        *flag |= 1 << ACK_RECEIVE_TIMEOUT_FLAG;

    if (val[1] & SUBPMIC_HARD_RESET_FLAG_MASK)
        *flag |= 1 << HARD_RESET_FLAG;

    if (val[0] & SUBPMIC_DATA_READY_FLAG_MASK)
        *flag |= 1 << DATA_READY_FLAG;

    if (val[0] & SUBPMIC_SENT_PACKET_COMPLETE_FLAG_MASK)
        *flag |= 1 << SENT_PACKET_COMPLETE_FLAG;

    if (val[1] & SUBPMIC_CRC_ERROR_FLAG_MASK)
        *flag |= 1 << CRC_ERROR_FLAG;

    if (val[1] & SUBPMIC_BAUD_RATE_ERROR_FLAG_MASK)
        *flag |= 1 << BAUD_RATE_ERROR_FLAG;

    if (val[0] & SUBPMIC_UFCS_HANDSHAKE_SUCC_FLAG_MASK)
        *flag |= 1 << UFCS_HANDSHAKE_SUCC_FLAG;

    if (val[0] & SUBPMIC_UFCS_HANDSHAKE_FAIL_FLAG_MASK)
        *flag |= 1 << UFCS_HANDSHAKE_FAIL_FLAG;

    if (val[1] & SUBPMIC_TRAINING_BYTES_ERROR_FLAG_MASK) {
        *flag |= 1 << TRAINING_BYTES_ERROR_FLAG;
    }

    if (val[0] & SUBPMIC_MSG_TRANS_FAIL_FLAG_MASK) {
        *flag |= 1 << MSG_TRANS_FAIL_FLAG;
        *flag &= ~(1 << SENT_PACKET_COMPLETE_FLAG);
    }

    if (val[2] & SUBPMIC_RX_BUFFER_BUSY_FLAG_MASK)
        *flag |= 1 << RX_BUFFER_BUSY_FLAG;

    if (val[2] & SUBPMIC_DATA_BIT_ERR_FLAG_MASK)
        *flag |= 1 << DATA_BIT_ERR_FLAG;

    if (val[2] & SUBPMIC_BAUDRATE_CHG_FLAG_MASK)
        *flag |= 1 << BAUDRATE_CHG_FLAG;

    if (val[1] & SUBPMIC_LENGTH_ERROR_FLAG_MASK)
        *flag |= 1 << LENGTH_ERROR_FLAG;

    if (val[0] & SUBPMIC_RX_OVERFLOW_FLAG_MASK)
        *flag |= 1 << RX_OVERFLOW_FLAG;

    if (val[2] & SUBPMIC_BUS_CONFLICT_FLAG_MASK) {
        *flag |= 1 << BUS_CONFLICT_FLAG;
    }    
    return 0;
}

static int subpmic_ufcs_clear_int_flag(struct ufcs_dev *ufcs, uint32_t flag) 
{
    return 0;
}

static int subpmic_ufcs_send_cable_hardreset(struct ufcs_dev *ufcs) 
{
    return 0;
}

static int subpmic_ufcs_send_source_hardreset(struct ufcs_dev *ufcs) 
{
    return 0;
}

static int subpmic_ufcs_send_message(struct ufcs_dev *ufcs, struct ufcs_msg_e *msg)
{
    int ret;
    uint8_t tx_buff[msg->msg_len + 2];
    struct subpmic_ufcs_device *pdata = ufcs_dev_get_chip(ufcs);
    if(IS_ERR(pdata))
        return -EINVAL;

    ret = regmap_write(pdata->rmap, TX_LENGTH, msg->msg_len + 2);

    tx_buff[0] = msg->msg_hdr >> 8;
    tx_buff[1] = msg->msg_hdr;
    memcpy(tx_buff + 2, msg->payload, msg->msg_len);
    ret = regmap_bulk_write(pdata->rmap, TX_BUFFER0, tx_buff, msg->msg_len + 2);
    ret |= subpmic_ufcs_field_write(pdata, SND_CMD, true);

    return ret;
}

static int subpmic_ufcs_get_message(struct ufcs_dev *ufcs, struct ufcs_msg_e *msg)
{
    int ret = 0;
    int reg_val;
    uint8_t rx_buff[256];
    struct subpmic_ufcs_device *pdata = ufcs_dev_get_chip(ufcs);
    if(IS_ERR(pdata))
        return -EINVAL;

    ret = regmap_read(pdata->rmap, RX_LENGTH, &reg_val);
    ret |= regmap_bulk_read(pdata->rmap, RX_BUFFER0, rx_buff, reg_val);
    if (ret) {
        return -EINVAL;
    }

    msg->msg_len = reg_val - 2;
    msg->msg_hdr = (rx_buff[0] << 8) | rx_buff[1];
    memcpy(msg->payload, rx_buff + 2, msg->msg_len);

    return 0;      
}

static int subpmic_ufcs_get_reply_sender(struct ufcs_dev *ufcs, uint8_t *sender) 
{
    return 0;
}

static int subpmic_ufcs_deinit(struct ufcs_dev *ufcs) 
{
    return 0;
}

static irqreturn_t subpmic_ufcs_alert_handler(int irq, void *data)
{
    struct subpmic_ufcs_device *pdata = data;
    dev_info(pdata->dev, "%s\n", __func__);
    ufcsi_irq_handle(&pdata->ufcs);
    return IRQ_HANDLED;
}

struct ufcs_dev_ops subpmic_ufcs_ops = {
    .init = subpmic_ufcs_init,
    .handshake = subpmic_ufcs_handshake,
    .exit = subpmic_ufcs_exit,
    .set_baudrate = subpmic_ufcs_set_baudrate,
    .get_baudrate = subpmic_ufcs_get_baudrate,
    .get_int_flag = subpmic_ufcs_get_int_flag,
    .clear_int_flag = subpmic_ufcs_clear_int_flag,
    .send_cable_hardreset = subpmic_ufcs_send_cable_hardreset,
    .send_source_hardreset = subpmic_ufcs_send_source_hardreset,
    .get_reply_sender = subpmic_ufcs_get_reply_sender,
    .send_message = subpmic_ufcs_send_message,
    .get_message = subpmic_ufcs_get_message,
    .deinit = subpmic_ufcs_deinit,
};

static int subpmic_ufcs_request_irq(struct subpmic_ufcs_device *pdata)
{
    int ret = 0;
    const char *irq_name = "UFCS";

    ret = platform_get_irq_byname(to_platform_device(pdata->dev),
                                irq_name);
    if (ret < 0) {
        dev_err(pdata->dev, "failed to get irq %s\n", irq_name);
        return ret;   
    } 

    pdata->irq = ret;
    dev_info(pdata->dev, "%s irq = %d\n", irq_name, ret);
    ret = devm_request_threaded_irq(pdata->dev, ret, NULL,
                    subpmic_ufcs_alert_handler, IRQF_ONESHOT,
                    dev_name(pdata->dev), pdata);
    if (ret < 0) {
        dev_err(pdata->dev, "failed to request irq %s\n", irq_name);
        return ret;
    }
    return 0;
}

static int subpmic_ufcs_hw_init(struct subpmic_ufcs_device *pdata)
{
    //todo: reset ufcs
    return 0;
}

static int subpmic_ufcs_register(struct subpmic_ufcs_device *pdata)
{
    int i;
    int ret;
    struct device_node *np = pdata->dev->of_node;

    struct {
        char *name;
        int *conv_data;
    } props[] = {
        {"sc6601-ufcs,default-baudrate", 
            &(pdata->ufcs_cfg_desc->default_baudrate)},
        {"sc6601-ufcs,default-request-volt", 
            &(pdata->ufcs_cfg_desc->default_request_volt)},
        {"sc6601-ufcs,default-request-curr", 
            &(pdata->ufcs_cfg_desc->default_request_curr)},
    };

    ret = of_property_read_string(np, "sc6601-ufcs,name",
				(char const **)&(pdata->ufcs_cfg_desc->name));
    if (ret < 0) {
        dev_err(pdata->dev, "%s read ufcs port name fail\n", __func__);
    }   

    for (i = 0; i < ARRAY_SIZE(props); i++) {
        ret = of_property_read_u32(np, props[i].name,
                        props[i].conv_data);
        if (ret < 0) {
            dev_err(pdata->dev, "can not read %s \n", props[i].name);
            continue;
        }
    }

    return ufcs_device_register(pdata->dev, pdata->ufcs_cfg_desc, 
                                &subpmic_ufcs_ops, pdata, &pdata->ufcs);
}

static int subpmic_ufcs_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct subpmic_ufcs_device *pdata;
    int i = 0;
    int ret = 0;

    pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
    if (!pdata)
        return -ENOMEM;

    pdata->rmap = dev_get_regmap(dev->parent, NULL);
    if (!pdata->rmap) {
        dev_err(dev, "failed to get regmap\n");
        return -ENODEV;
    }
    pdata->dev = dev;
    pdata->ufcs_cfg_desc = &subpmic_default_desc;
    platform_set_drvdata(pdev, pdata);

    for (i = 0; i < ARRAY_SIZE(ufcs_reg_fields); i++) {
        pdata->rmap_fields[i] = devm_regmap_field_alloc(dev, 
                            pdata->rmap, ufcs_reg_fields[i]);
        if (IS_ERR(pdata->rmap_fields[i])) {
            dev_err(dev, "cannot allocate regmap field\n");
            return PTR_ERR(pdata->rmap_fields[i]);
        }
    }

    ret = subpmic_ufcs_request_irq(pdata);
    if (ret < 0) {
        dev_err(dev, "irq request failed\n");
        goto err;
    }    

    ret = subpmic_ufcs_hw_init(pdata);
    if (ret < 0) {
        dev_err(dev, "hw init failed\n");
        goto err;
    }

    subpmic_ufcs_register(pdata);
    dev_info(dev, "probe success\n");
    return 0;


err:
    dev_info(dev, "probe failed\n");
    return ret;    
}

static int subpmic_ufcs_remove(struct platform_device *pdev)
{
    
    return 0;
}

static const struct of_device_id subpmic_ufcs_of_match[] = {
    {.compatible = "southchip,subpmic_ufcs",},
    {},
};

static struct platform_driver subpmic_ufcs_driver = {
    .driver = {
        .name = "subpmic_ufcs",
        .of_match_table = of_match_ptr(subpmic_ufcs_of_match),
    },
    .probe = subpmic_ufcs_probe,
    .remove = subpmic_ufcs_remove,
};

module_platform_driver(subpmic_ufcs_driver);

MODULE_AUTHOR("Yongsheng Zhan <Yongsheng-Zhan@southchip.com>");
MODULE_DESCRIPTION("subpmic ufcs core driver");
MODULE_LICENSE("GPL v2");