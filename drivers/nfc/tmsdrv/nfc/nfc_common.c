/*****************************************************************************************
 * Copyright (c)  2021- 2022  TsingTeng MicroSystem Corp.Ltd.
 * TMS_EDIT
 * File       : nfc_common.c
 * Description: Source file for tms nfc common
 * Version    : 1.0
 * Date       : 2022/4/11
 * Team       : NFC Middleware
 * Author     : Guoliang Wu
 * --------------------------------- Revision History: ---------------------------------
 *   <version>    <date>          < author >                            <desc>
 *******************************************************************************************/
#include "nfc_common.h"
#include "../guidev/guide_driver.h"
/*********** PART0: Global Variables Area ***********/

/*********** PART1: Function Area ***********/
struct nfc_info *nfc_data_alloc(struct device *dev, struct nfc_info *nfc)
{
    nfc = devm_kzalloc(dev, sizeof(struct nfc_info), GFP_KERNEL);
    return nfc;
}

void nfc_data_free(struct device *dev, struct nfc_info *nfc)
{
    if (nfc) {
        devm_kfree(dev, nfc);
    }

}

struct nfc_info *nfc_get_data(struct inode *inode)
{
    struct nfc_info *nfc;
    struct dev_register *char_dev;
    char_dev = container_of(inode->i_cdev, struct dev_register, chrdev);
    nfc = container_of(char_dev, struct nfc_info, dev);
    return nfc;
}

void nfc_hard_reset(struct nfc_info *nfc, uint32_t mdelay)
{
    if (!nfc->tms->set_ven) {
        TMS_ERR("nfc->tms->set_ven is NULL");
        return;
    }

    usleep_range(GPIO_VEN_SET_WAIT_TIME_US, (GPIO_VEN_SET_WAIT_TIME_US) + 100);
    nfc->tms->set_ven(nfc->hw_res, OFF);
    usleep_range(GPIO_VEN_SET_WAIT_TIME_US, (GPIO_VEN_SET_WAIT_TIME_US) + 100);
    nfc->tms->set_ven(nfc->hw_res, ON);

    if (mdelay) {
        usleep_range(mdelay, (mdelay) + 100);
    }

    TMS_DEBUG("Finished");
}
void tms_nfc_hard_reset(struct tms_info *tms)
{
    TMS_DEBUG("Hard reset\n");

    if (IS_ERR_OR_NULL(tms)) {
        TMS_ERR("tms is invalid, ret = %ld\n", IS_ERR(tms) ? PTR_ERR(tms) : -EINVAL);
        return;
    }

    if (IS_ERR_OR_NULL(tms->set_gpio)) {
        TMS_ERR("set_gpio func invalid, ret = %ld\n",
                IS_ERR(tms->set_gpio) ? PTR_ERR(tms->set_gpio) : -EINVAL);
        return;
    }

    tms->set_gpio(tms->hw_res.ven_gpio, OFF, WAIT_TIME_20000US, WAIT_TIME_20000US);
    tms->set_gpio(tms->hw_res.ven_gpio, ON, WAIT_TIME_NONE, WAIT_TIME_20000US);
}

bool tms_nfc_write(struct i2c_client *client, const uint8_t *cmd, size_t len)
{
    int count;
    ssize_t ret;

    for (count = 0; count < RETRY_TIMES; count++) {
        ret = i2c_master_send(client, cmd, len);
        if (ret == len) {
            tms_buffer_dump("Tx ->", cmd, len);
            return true;
        }

        TMS_ERR("Error writting: %zd\n", ret);
    }

    return false;
}
bool tms_nfc_read(struct i2c_client *client, unsigned int irq_gpio,
              uint8_t *data, size_t len, const char *msg)
{
    ssize_t ret;
    /**
     * @brief Retry 18 times because the some chip needs to wait for 50ms after reset.
     * After the reset is done, it will wait for 20ms. To ensure reliability,
     * the polling interval is 2ms each time, which can provide a total of 36ms.
     * The sum of 20ms and 36ms is 56ms, which is sufficient to cover the required 50ms.
    */
    int retry = 18;

    if (IS_ERR_OR_NULL(client) || IS_ERR_OR_NULL(data) || !len || IS_ERR_OR_NULL(msg)) {
        TMS_ERR("Invalid parameters\n");
        return false;
    }

    do {
        if (gpio_get_value(irq_gpio)) {
            break;
        }

        TMS_DEBUG("Wait for data...(%s)\n", msg);
        usleep_range(I2C_POLL_WAIT_TIME, I2C_POLL_WAIT_TIME + 100);
    } while (retry--);

    if (retry < 0) {
        TMS_ERR("%s read timeout after %d retries\n", msg, 10);
        return false;
    }

    ret = i2c_master_recv(client, data, len);
    if (ret == len) {
        tms_buffer_dump("Rx <-", data, len);
        return true;
    }

    if (ret < 0) {
        TMS_ERR("I2C read error: %zd\n", ret);
    } else {
        TMS_ERR("Incomplete read %s: %zd/%zu\n", msg, ret, len);
    }
    return false;
}

static bool nfc_read_header(struct i2c_client *client, unsigned int irq_gpio,
                            uint8_t *header, size_t header_len)
{
    ssize_t ret;
    int retry = 10;

    do {
        if (gpio_get_value(irq_gpio)) {
            break;
        }

        TMS_DEBUG("Wait for data...\n");
        usleep_range(WAIT_TIME_1000US, WAIT_TIME_1000US + 100);
    } while (retry--);

    ret = i2c_master_recv(client, header, header_len);
    if (ret == header_len) {
        tms_buffer_dump("Rx <-", header, header_len);
        return true;
    }

    TMS_ERR("Error reading header: %zd\n", ret);
    return false;
}

static bool nfc_read_payload(struct i2c_client *client, unsigned int irq_gpio,
                             uint8_t *payload, size_t payload_len)
{
    ssize_t ret;
    int retry = 10;
    size_t read_len = (payload_len == 1) ? payload_len + 1 : payload_len;

    do {
        if (gpio_get_value(irq_gpio)) {
            break;
        }

        TMS_DEBUG("Wait for data...\n");
        usleep_range(WAIT_TIME_1000US, WAIT_TIME_1000US + 100);
    } while (retry--);

    ret = i2c_master_recv(client, payload, read_len);
    if (ret == read_len) {
        tms_buffer_dump("Rx <-", payload, payload_len);
        return true;
    }

    TMS_ERR("Error reading payload: %zd\n", ret);
    return false;
}

void nfc_jump_fw(struct i2c_client *client, unsigned int irq_gpio)
{
    const uint8_t core_reset[] = {0x20, 0x00, 0x01, 0x00};
    const uint8_t chk_rsp_hdr[] = {0x40, 0x00, 0x01};
    uint8_t rsp_hdr[NCI_HDR_LEN] = {0};
    uint8_t rsp_payload[MAX_NCI_PAYLOAD_LEN] = {0};
    /* It is possible to receive up to two times and redundant once */
    int retry = 2;

    if (!tms_nfc_write(client, core_reset, sizeof(core_reset))) {
        TMS_ERR("send core_reset error\n");
        return;
    }

    do {
        if (!nfc_read_header(client, irq_gpio, rsp_hdr, NCI_HDR_LEN)) {
            return;
        }

        if (!nfc_read_payload(client, irq_gpio, rsp_payload, rsp_hdr[HEAD_PAYLOAD_BYTE])) {
            TMS_ERR("Read core_reset rsp payload error\n");
            return;
        }

        if ((!memcmp(rsp_hdr, chk_rsp_hdr, NCI_HDR_LEN)) && rsp_payload[0] == 0xFE) {
            usleep_range(WAIT_TIME_10000US, WAIT_TIME_10000US + 100);
            continue;
        } else if ((!memcmp(rsp_hdr, chk_rsp_hdr, NCI_HDR_LEN)) && rsp_payload[0] == 0x00) {
            /* Core reset NTF needs to be received in FW */
            retry = 1;
            continue;
        } else if ((!memcmp(rsp_hdr, chk_rsp_hdr, NCI_HDR_LEN)) && rsp_payload[0] == 0xFF) {
            TMS_ERR("Failed, need upgrade the firmware\n");
            break;
        }
    } while (retry--);
}

void nfc_disable_irq(struct nfc_info *nfc)
{
    unsigned long flag;
    spin_lock_irqsave(&nfc->irq_enable_slock, flag);

    if (nfc->irq_enable) {
        disable_irq_nosync(nfc->client->irq);
        nfc->irq_enable = false;
    }

    spin_unlock_irqrestore(&nfc->irq_enable_slock, flag);
}

void nfc_enable_irq(struct nfc_info *nfc)
{
    unsigned long flag;
    spin_lock_irqsave(&nfc->irq_enable_slock, flag);

    if (!nfc->irq_enable) {
        enable_irq(nfc->client->irq);
        nfc->irq_enable = true;
    }

    spin_unlock_irqrestore(&nfc->irq_enable_slock, flag);
}

static irqreturn_t nfc_irq_handler(int irq, void *dev_id)
{
    struct nfc_info *nfc;
    nfc = dev_id;

    if (device_may_wakeup(nfc->i2c_dev)) {
        pm_wakeup_event(nfc->i2c_dev, WAKEUP_SRC_TIMEOUT);
    }

    nfc_disable_irq(nfc);
    wake_up(&nfc->read_wq);
    return IRQ_HANDLED;
}

int nfc_irq_register(struct nfc_info *nfc)
{
    int ret;
    TMS_INFO("Start+\n");
    nfc->client->irq = gpio_to_irq(nfc->hw_res.irq_gpio);

    if (nfc->client->irq < 0) {
        TMS_ERR("Get soft irq number failed");
        return -ERROR;
    }

    ret = devm_request_irq(nfc->i2c_dev, nfc->client->irq, nfc_irq_handler,
                           IRQF_TRIGGER_HIGH, nfc->dev.name, nfc);

    if (ret) {
        TMS_ERR("Register irq failed, ret = %d\n", ret);
        return ret;
    }

    TMS_DEBUG("Register NFC IRQ[%d]\n", nfc->client->irq);
    TMS_INFO("Normal end-\n");
    return SUCCESS;
}

void nfc_power_control(struct nfc_info *nfc, bool state)
{
    if (!nfc->tms->set_ven) {
        TMS_ERR("nfc->tms->set_ven is NULL");
        return;
    }

    if (state == ON) {
        nfc_enable_irq(nfc);
        nfc->tms->set_ven(nfc->hw_res, ON);
        nfc->tms->ven_enable = true;
        TMS_DEBUG("Power On\n");
    } else if (state == OFF) {
        nfc_disable_irq(nfc);
        nfc->tms->set_ven(nfc->hw_res, OFF);
        nfc->tms->ven_enable = false;
        TMS_DEBUG("Power Off\n");
    }
}

void nfc_fw_download_control(struct nfc_info *nfc, bool state)
{
    if (!nfc->tms->feature.dl_support) {
        TMS_ERR("enter nfc_fw_download_control\n");
        return;
    }
    if (!nfc->tms->set_download) {
        TMS_ERR("nfc->tms->set_download is NULL");
        return;

    }

    if (state == ON) {
        nfc->tms->set_download(nfc->hw_res, ON);
        TMS_DEBUG("Download state\n");
    } else if (state == OFF) {
        nfc->tms->set_download(nfc->hw_res, OFF);
        TMS_DEBUG("Normal state\n");
    }
}

static int ese_power_control(struct nfc_info *nfc, bool state)
{
    if (!nfc->tms->set_ven) {
        TMS_ERR("nfc->tms->set_ven is NULL");
        return -ERROR;
    }

    if (state == ON) {
        nfc->tms->ven_enable = gpio_get_value(nfc->hw_res.ven_gpio);

        if (!nfc->tms->ven_enable) {
            nfc->tms->set_ven(nfc->hw_res, ON);
            TMS_INFO("eSE hal service setting power on\n");
        } else {
            TMS_WARN("Ven already high\n");
        }
    } else if (state == OFF) {
        if (!nfc->tms->ven_enable) {
            TMS_INFO("NFC is disabled, setting power off\n");
            nfc->tms->set_ven(nfc->hw_res, OFF);
        } else {
            TMS_WARN("NFC is enabled, Keep power on\n");
        }
    }

    return SUCCESS;
}

int nfc_ioctl_set_ese_state(struct nfc_info *nfc, unsigned long arg)
{
    int ret;
    TMS_DEBUG("arg = %lu\n", arg);

    switch (arg) {
    case REQUEST_ESE_POWER_ON:
        ret = ese_power_control(nfc, ON);
        break;

    case REQUEST_ESE_POWER_OFF:
        ret = ese_power_control(nfc, OFF);
        break;

    default:
        TMS_ERR("Bad control arg %lu\n", arg);
        ret = -ENOIOCTLCMD;
        break;
    }

    return ret;
}

int nfc_ioctl_get_ese_state(struct nfc_info *nfc, unsigned long arg)
{
    int ret;
    TMS_DEBUG("arg = %lu\n", arg);

    switch (arg) {
    case REQUEST_ESE_POWER_STATE:
        ret = gpio_get_value(nfc->hw_res.ven_gpio);
        break;

    default:
        TMS_ERR("Bad control arg %lu\n", arg);
        ret = -ENOIOCTLCMD;
        break;
    }

    return ret;
}

void nfc_gpio_release(struct nfc_info *nfc)
{
    gpio_free(nfc->hw_res.irq_gpio);
    gpio_free(nfc->hw_res.ven_gpio);
    if (nfc->tms->feature.dl_support) {
        TMS_ERR("enter nfc_gpio_release\n");
        gpio_free(nfc->hw_res.download_gpio);
    }
    TMS_DEBUG("Finished\n");
}

ssize_t nfc_write(struct nfc_info *nfc, const struct capsule *data)
{
    int count;
    ssize_t ret;
    for (count = 0; count < RETRY_TIMES; count++) {
        ret = i2c_master_send(nfc->client, data->nci_cmd, data->len);
        if (ret != data->len) {
            TMS_ERR("I2C writer error = %zd\n", ret);
            continue;
        }
        break;
    }
    tms_buffer_dump("Tx <-", data->nci_cmd, data->len);
    return ret;
}

/* data->nci_recv can not less than MAX_NCI_BUFFER_SIZE */
ssize_t nfc_read(struct nfc_info *nfc, struct capsule *data, bool block_mode)
{
    int count;
    bool need2bytes = true;
    ssize_t ret = -ERROR;
    size_t read_len = 0;

    for (count = 0; count < RETRY_TIMES; count++) {
        ret = -ERROR;
        msleep(5);

        if (true == block_mode && (!gpio_get_value(nfc->hw_res.irq_gpio))) {
            TMS_DEBUG("Wait for data...\n");
            continue;
        } else if (false == block_mode && (!gpio_get_value(nfc->hw_res.irq_gpio))) {
            TMS_WARN("Read called but no IRQ!\n");
            return -EAGAIN;
        }

        ret = i2c_master_recv(nfc->client, data->nci_recv, NCI_HDR_LEN);
        if (NCI_HDR_LEN == ret) {
                data->len = ret;
        } else {
            TMS_ERR("Read head error[%zd]\n", ret);
            continue;
        }

        read_len = data->nci_recv[HEAD_PAYLOAD_BYTE];
        if (1 == read_len && need2bytes) {
            read_len++;
        }
        if (read_len > MAX_NCI_PAYLOAD_LEN) {
            read_len = MAX_NCI_PAYLOAD_LEN;
            TMS_WARN("Receive nci payload is more than max\n");
        }

        ret = i2c_master_recv(nfc->client, &data->nci_recv[data->len], read_len);
        if (ret == read_len) {
            if (2 == read_len && need2bytes) {
                read_len--;
            }
            data->len = data->len + read_len;
        } else {
            TMS_ERR("Read payload error[%zd]\n", ret);
            continue;
        }
        ret = data->len;
        break;
    }
    tms_buffer_dump("Rx <-", data->nci_recv, data->len);

    return ret;
}

static ssize_t chip_name_show(struct device *dev, struct device_attribute *attr,
                              char *buf)
{
    struct tms_info *tms;

    tms = tms_common_data_binding();
    if (tms == NULL || tms->nfc_name == NULL) {
        return (ssize_t)(snprintf(buf, MAX_CHIP_NAME_SIZE - 1, "%s\n", "unknow"));
    }

    return (ssize_t)(snprintf(buf, MAX_CHIP_NAME_SIZE - 1, "%s\n", tms->nfc_name));
}

static struct device_attribute nfc_attr[] = {
    __ATTR(chip_name, 0444, chip_name_show, NULL),
};

int nfc_create_sysfs_interfaces(struct device *dev)
{
    int count;

    for (count = 0; count < ARRAY_SIZE(nfc_attr); count++) {
        if (device_create_file(dev, nfc_attr + count)) {
            TMS_ERR("NFC create file failed\n");
            goto create_file_error;
        }
    }
    TMS_INFO("Create sysfs interfaces success\n");
    return SUCCESS;
create_file_error:
    for (; count >= 0; count--) {
        device_remove_file(dev, nfc_attr + count);
    }

    return -ERROR;
}

void nfc_remove_sysfs_interfaces(struct device *dev)
{
    int count;

    for (count = 0; count < ARRAY_SIZE(nfc_attr); count++) {
        device_remove_file(dev, nfc_attr + count);
    }

    TMS_INFO("Remove sysfs interfaces success\n");
}

static int nfc_gpio_configure_init(struct nfc_info *nfc)
{
    int ret;
    TMS_INFO("Start+\n");

    if (gpio_is_valid(nfc->hw_res.irq_gpio)) {
        ret = gpio_direction_input(nfc->hw_res.irq_gpio);

        if (ret < 0) {
            TMS_ERR("Unable to set irq gpio as input\n");
            return ret;
        }

        TMS_INFO("Set irq gpio as input\n");
    }

    if (gpio_is_valid(nfc->hw_res.ven_gpio)) {
        ret = gpio_direction_output(nfc->hw_res.ven_gpio, nfc->hw_res.ven_flag);

        if (ret < 0) {
            TMS_ERR("Unable to set ven gpio as output\n");
            return ret;
        }
        TMS_INFO("Set ven gpio as output\n");
    }

    if(!(support_download_gpio()))
    {
        nfc->tms->feature.dl_support = false;
    }else{
        nfc->tms->feature.dl_support = true;
    }

    if(nfc->tms->feature.dl_support == true) {
        if (gpio_is_valid(nfc->hw_res.download_gpio)) {
            ret = gpio_direction_output(nfc->hw_res.download_gpio,
                                    nfc->hw_res.download_flag);

            if (ret < 0) {
                TMS_ERR("Unable to set download_gpio as output\n");
                return ret;
            }

            TMS_INFO("Set download gpio as output\n");
        }
    }
    TMS_INFO("Normal end-\n");
    return SUCCESS;
}

static int nfc_platform_clk_init(struct nfc_info *nfc)
{
    TMS_INFO("Start+\n");
    nfc->clk = devm_clk_get(nfc->i2c_dev, "clk_aux");

    if (IS_ERR(nfc->clk)) {
        TMS_ERR("Platform clock not specified in dts\n");
        return -ERROR;
    }

    nfc->clk_parent = devm_clk_get(nfc->i2c_dev, "source");

    if (IS_ERR(nfc->clk_parent)) {
        TMS_ERR("Clock parent not specified in dts\n");
        return -ERROR;
    }

    clk_set_parent(nfc->clk, nfc->clk_parent);
    clk_set_rate(nfc->clk, 26000000);
    nfc->clk_enable = devm_clk_get(nfc->i2c_dev, "enable");

    if (IS_ERR(nfc->clk_enable)) {
        TMS_ERR("Clock enable not specified in dts\n");
        return -ERROR;
    }

    clk_prepare_enable(nfc->clk);
    clk_prepare_enable(nfc->clk_enable);
    TMS_INFO("Normal end-\n");
    return SUCCESS;
}

static int nfc_parse_dts_init(struct nfc_info *nfc)
{
    int ret, rcv;
    struct device_node *np;
    TMS_INFO("Start+\n");
    np = nfc->i2c_dev->of_node;
    rcv = of_property_read_string(np, "tms,device-name", &nfc->dev.name);

    if (rcv < 0) {
        nfc->dev.name = "tms_nfc";
        TMS_WARN("Device name not specified in dts\n");
    }

    rcv = of_property_read_u32(np, "tms,device-count", &nfc->dev.count);

    if (rcv < 0) {
        nfc->dev.count = 1;
        TMS_WARN("Number of devices not specified in dts\n");
    }

    nfc->hw_res.irq_gpio = of_get_named_gpio(np, "tms,irq-gpio", 0);

    if (gpio_is_valid(nfc->hw_res.irq_gpio)) {
        rcv = gpio_request(nfc->hw_res.irq_gpio, "nfc_int");

        if (rcv) {
            TMS_WARN("Unable to request gpio[%d] as IRQ\n",
                     nfc->hw_res.irq_gpio);
        }
    } else {
        TMS_ERR("Irq gpio not specified in dts\n");
        return -EINVAL;
    }

    nfc->hw_res.ven_gpio = of_get_named_gpio_flags(np, "tms,ven-gpio", 0,
                           &nfc->hw_res.ven_flag);

    if (gpio_is_valid(nfc->hw_res.ven_gpio)) {
        rcv = gpio_request(nfc->hw_res.ven_gpio, "nfc_ven");

        if (rcv) {
            TMS_WARN("Unable to request gpio[%d] as VEN\n",
                     nfc->hw_res.ven_gpio);
        }
    } else {
        TMS_ERR("Ven gpio not specified in dts\n");
        ret =  -EINVAL;
        goto err_free_irq;
    }

    nfc->hw_res.download_gpio = of_get_named_gpio_flags(np, "tms,download-gpio", 0,
                                &nfc->hw_res.download_flag);

    if (gpio_is_valid(nfc->hw_res.download_gpio)) {
        rcv = gpio_request(nfc->hw_res.download_gpio, "nfc_fw_download");

        if (rcv) {
            TMS_WARN("Unable to request gpio[%d] as FWDownLoad\n",
                     nfc->hw_res.download_gpio);
        }
    } else {
        TMS_ERR("FW-Download gpio not specified in dts\n");
        //ret = -EINVAL;
        //goto err_free_ven;
    }

    TMS_DEBUG("NFC device name is %s, count = %d\n", nfc->dev.name,
              nfc->dev.count);
    TMS_DEBUG("irq_gpio = %d, ven_gpio = %d, download_gpio = %d\n",
              nfc->hw_res.irq_gpio, nfc->hw_res.ven_gpio, nfc->hw_res.download_gpio);
    TMS_INFO("Normal end-\n");
    return SUCCESS;
//err_free_ven:
//    gpio_free(nfc->hw_res.ven_gpio);
err_free_irq:
    gpio_free(nfc->hw_res.irq_gpio);
    TMS_ERR("Error end, ret = %d\n", ret);
    return ret;
}

int nfc_common_info_init(struct nfc_info *nfc)
{
    int ret;
    TMS_INFO("Start+\n");
    /* step1 : binding tms common data */
    nfc->tms = tms_common_data_binding();

    if (nfc->tms == NULL) {
        TMS_ERR("Get tms common info  error\n");
        return -ENOMEM;
    }

    /* step2 : dts parse */
    ret = nfc_parse_dts_init(nfc);

    if (ret) {
        TMS_ERR("Parse dts failed.\n");
        return ret;
    }

    /* step3 : set platform clock */
    ret = nfc_platform_clk_init(nfc);

    if (ret) {
        TMS_WARN("Do not set platform clock\n");
    }

    /* step4 : set gpio work mode */
    ret = nfc_gpio_configure_init(nfc);

    if (ret) {
        TMS_ERR("Init gpio control failed.\n");
        goto err_free_gpio;
    }

    /* step5 : binding common function */
    nfc->tms->hw_res.ven_gpio = nfc->hw_res.ven_gpio;
    nfc->tms->ven_enable      = false;
    TMS_INFO("Normal end-\n");
    return SUCCESS;
err_free_gpio:
    nfc_gpio_release(nfc);
    TMS_ERR("Error end, ret = %d\n", ret);
    return ret;
}
