// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 TsingTeng MicroSystem Co., Ltd.
 */

#include "nfc_common.h"

/*********** PART0: Global Variables Area ***********/
static const uint8_t core_reset[] = {0x20, 0x00, 0x01, 0x00};
static const uint8_t bl_reset[] = {0x5A, 0xC4, 0x00, 0x9E};
static const uint8_t get_bl_ver[] = {0x5A, 0x00, 0x05, 0x00, 0x4A, 0x02, 0x00, 0x00, 0x17};
static const uint8_t get_hw_status[] = {0x5A, 0x40, 0x05, 0x00, 0x4A, 0xFE, 0x00, 0x00, 0xAB};

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

    nfc = NULL;
}

struct nfc_info *nfc_get_data(struct inode *inode)
{
    struct nfc_info *nfc;
    struct dev_register *char_dev;
    char_dev = container_of(inode->i_cdev, struct dev_register, chrdev);
    nfc = container_of(char_dev, struct nfc_info, dev);
    return nfc;
}

void nfc_hard_reset(struct tms_info *tms)
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
static bool tms_nfc_write(struct i2c_client *client, const uint8_t *cmd, size_t len)
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

void nfc_read_flush(struct nfc_info *nfc)
{
    nfc->release_read = true;
    wake_up(&nfc->read_wq);
    TMS_INFO("Release of blocked read\n");
}

bool nfc_write(struct i2c_client *client, const uint8_t *cmd,
               size_t len, const char *msg)
{
    int retry = 3;
    int count;
    ssize_t ret;

    if (IS_ERR_OR_NULL(client) || IS_ERR_OR_NULL(cmd) || !len || IS_ERR_OR_NULL(msg)) {
        TMS_ERR("Invalid parameters\n");
        return false;
    }

    for (count = 0; count < retry; count++) {
        ret = i2c_master_send(client, cmd, len);
        if (ret == len) {
            TMS_DUMP(LOG_LEVEL_ERROR, "Tx ->", cmd, len);
            return true;
        }

        if (ret < 0) {
            TMS_ERR("I2C write error: %zd\n", ret);
        } else {
            TMS_ERR("Incomplete write %s: %zd/%zu\n", msg, ret, len);
        }

        usleep_range(I2C_POLL_WAIT_TIME, I2C_POLL_WAIT_TIME + 100);
    }

    TMS_ERR("Write %s failed after %d retries\n", msg, retry);
    return false;
}

bool nfc_read(struct i2c_client *client, unsigned int irq_gpio,
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
        TMS_DUMP(LOG_LEVEL_ERROR, "Rx <-", data, len);
        return true;
    }

    if (ret < 0) {
        TMS_ERR("I2C read error: %zd\n", ret);
    } else {
        TMS_ERR("Incomplete read %s: %zd/%zu\n", msg, ret, len);
    }
    return false;
}

static bool bl_ver_check(struct i2c_client *client, struct tms_info *tms)
{
    const uint8_t normal_bl_ver_rsp_prefix[] = {0xA5, 0x00};
    uint8_t retry = 3;
    size_t len = 0;
    size_t payload_len = 0;
    uint8_t recv_buf[MAX_T1_BUFFER_SIZE] = {0};

    TMS_DEBUG("Enter\n");

    if (IS_ERR_OR_NULL(client)) {
        TMS_ERR("client is invalid, ret = %ld\n", IS_ERR(client) ? PTR_ERR(client) : -EINVAL);
        return false;
    }

    if (IS_ERR_OR_NULL(tms)) {
        TMS_ERR("tms is invalid, ret = %ld\n", IS_ERR(tms) ? PTR_ERR(tms) : -EINVAL);
        return false;
    }

    if (!nfc_write(client, get_bl_ver, sizeof(get_bl_ver), "get_bl_ver")) {
        return false;
    }

    do {
        if (!nfc_read(client, tms->hw_res.irq_gpio, recv_buf, T1_HDR_LEN, "bl_ver_h")) {
            break;
        }

        len = T1_HDR_LEN;
        payload_len = recv_buf[T1_PAYLOAD_LEN_BYTE];

        if (payload_len > (MAX_T1_BUFFER_SIZE - T1_HDR_LEN - T1_LRC_LEN)) {
            TMS_ERR("Payload length %zu exceeds buffer size\n", payload_len);
            break;
        }

        if (!nfc_read(client, tms->hw_res.irq_gpio, recv_buf + T1_HDR_LEN,
                      payload_len + T1_LRC_LEN, "bl_ver_t")) {
            break;
        }

        len += payload_len + T1_LRC_LEN;

        if (!memcmp(recv_buf, normal_bl_ver_rsp_prefix, sizeof(normal_bl_ver_rsp_prefix))) {
            if (0xB0 == (recv_buf[len - 4 - T1_LRC_LEN] & 0xF0)) {
                return true;
            } else {
                break;
            }
        }

    } while (retry--);

    TMS_ERR("BL failed to match TMS\n");
    return false;
}

static void hw_check(struct i2c_client *client, struct tms_info *tms)
{
    const uint8_t normal_hw_status_rsp_prefix[] = {0xA5, 0x40};
    uint8_t retry = 3;
    size_t len = 0;
    size_t payload_len = 0;
    uint8_t recv_buf[MAX_T1_BUFFER_SIZE] = {0};

    TMS_DEBUG("Enter\n");

    if (IS_ERR_OR_NULL(client)) {
        TMS_ERR("client is invalid, ret = %ld\n", IS_ERR(client) ? PTR_ERR(client) : -EINVAL);
        return;
    }

    if (IS_ERR_OR_NULL(tms)) {
        TMS_ERR("tms is invalid, ret = %ld\n", IS_ERR(tms) ? PTR_ERR(tms) : -EINVAL);
        return;
    }

    if (!nfc_write(client, get_hw_status, sizeof(get_hw_status), "get_hw_status")) {
        return;
    }

    do {
        if (!nfc_read(client, tms->hw_res.irq_gpio, recv_buf, T1_HDR_LEN, "hw_status_h")) {
            break;
        }

        len = T1_HDR_LEN;
        payload_len = recv_buf[T1_PAYLOAD_LEN_BYTE];

        if (payload_len > (MAX_T1_BUFFER_SIZE - T1_HDR_LEN - T1_LRC_LEN)) {
            TMS_ERR("Payload length %zu exceeds buffer size\n", payload_len);
            break;
        }

        if (!nfc_read(client, tms->hw_res.irq_gpio, recv_buf + T1_HDR_LEN,
                      payload_len + T1_LRC_LEN, "hw_status_t")) {
            break;
        }

        len += payload_len + T1_LRC_LEN;

        if (!memcmp(recv_buf, normal_hw_status_rsp_prefix, sizeof(normal_hw_status_rsp_prefix))) {
            return;
        }

    } while (retry--);

    TMS_ERR("Failed to get hw status\n");
    return;
}

static bool bl_check(struct i2c_client *client, struct tms_info *tms, bool is_force)
{
    const uint8_t normal_bl_reset_rsp_prefix[] = {0xA5, 0xE4};
    bool ret = false;
    uint8_t retry = 3;
    size_t len = 0;
    size_t payload_len = 0;
    uint8_t recv_buf[MAX_T1_BUFFER_SIZE] = {0};

    TMS_DEBUG("Enter\n");

    if (IS_ERR_OR_NULL(client)) {
        TMS_ERR("client is invalid, err = %ld\n", IS_ERR(client) ? PTR_ERR(client) : -EINVAL);
        return false;
    }

    if (IS_ERR_OR_NULL(tms)) {
        TMS_ERR("tms is invalid, err = %ld\n", IS_ERR(tms) ? PTR_ERR(tms) : -EINVAL);
        return false;
    }

    if (IS_ERR_OR_NULL(tms->set_gpio)) {
        TMS_ERR("set_gpio func invalid, err = %ld\n", IS_ERR(tms->set_gpio)
                ? PTR_ERR(tms->set_gpio) : -EINVAL);
        return false;
    }

    if (is_force && tms->feature.dl_support) {
        tms->set_gpio(tms->hw_res.download_gpio, ON,
                      WAIT_TIME_NONE, WAIT_TIME_10000US);
    }

    nfc_hard_reset(tms);

    if (!nfc_write(client, bl_reset, sizeof(bl_reset), "bl_reset")) {
        goto exit;
    }

    do {
        if (!nfc_read(client, tms->hw_res.irq_gpio, recv_buf, T1_HDR_LEN, "bl_reset_h")) {
            break;
        }

        len = T1_HDR_LEN;
        payload_len = recv_buf[T1_PAYLOAD_LEN_BYTE];

        if (payload_len > (MAX_T1_BUFFER_SIZE - T1_HDR_LEN - T1_LRC_LEN)) {
            TMS_ERR("Payload length %zu exceeds buffer size\n", payload_len);
            break;
        }

        if (!nfc_read(client, tms->hw_res.irq_gpio, recv_buf + T1_HDR_LEN,
                      payload_len + T1_LRC_LEN, "bl_reset_t")) {
            break;
        }

        len += payload_len + T1_LRC_LEN;

        if (!memcmp(recv_buf, normal_bl_reset_rsp_prefix, sizeof(normal_bl_reset_rsp_prefix))) {
            ret = bl_ver_check(client, tms);
            hw_check(client, tms);
            goto exit;
        }

    } while (retry--);

    TMS_ERR("Failed to match TMS\n");
exit:
    if (is_force && tms->feature.dl_support) {
        tms->set_gpio(tms->hw_res.download_gpio, OFF,
                      WAIT_TIME_NONE, WAIT_TIME_10000US);
    }
    return ret;
}

bool nfc_hw_check(struct i2c_client *client, struct tms_info *tms)
{
    const uint8_t normal_core_reset_rsp_prefix[] = {0x40, 0x00, 0x01};
    const uint8_t normal_core_reset_ntf_prefix[] = {0x60, 0x00, 0x0A, 0x02};
    uint8_t retry = 5;
    size_t len = 0;
    size_t is_two_byte = 0;
    size_t payload_len = 0;
    uint8_t recv_buf[MAX_NCI_BUFFER_SIZE] = {0};

    TMS_INFO("Enter\n");

    if (IS_ERR_OR_NULL(client)) {
        TMS_ERR("client is invalid, ret = %ld\n", IS_ERR(client) ? PTR_ERR(client) : -EINVAL);
        return false;
    }

    if (IS_ERR_OR_NULL(tms)) {
        TMS_ERR("tms is invalid, ret = %ld\n", IS_ERR(tms) ? PTR_ERR(tms) : -EINVAL);
        return false;
    }

    nfc_hard_reset(tms);

    if (!nfc_write(client, core_reset, sizeof(core_reset), "core_reset")) {
        goto exit;
    }

    do {
        if (!nfc_read(client, tms->hw_res.irq_gpio, recv_buf, NCI_HDR_LEN, "core_reset_h")) {
            break;
        }

        len = NCI_HDR_LEN;
        payload_len = recv_buf[NCI_PAYLOAD_LEN_BYTE];
        is_two_byte = (payload_len == 1) ? 1 : 0;

        if (payload_len > (MAX_NCI_BUFFER_SIZE - NCI_HDR_LEN)) {
            TMS_ERR("Payload length %zu exceeds buffer size\n", payload_len);
            break;
        }

        if (!nfc_read(client, tms->hw_res.irq_gpio, recv_buf + NCI_HDR_LEN,
            payload_len + is_two_byte, "core_reset_t")) {
            break;
        }

        len += payload_len;

        if (!memcmp(recv_buf, normal_core_reset_ntf_prefix, sizeof(normal_core_reset_ntf_prefix))) {
            if (0xC0 == (recv_buf[len - 3] & 0xF0)
                || 0xD0 == (recv_buf[len - 3] & 0xF0)) {
                return true;
            } else {
                TMS_INFO("Not TMS\n");
                return false;
            }
        }

        if (!memcmp(recv_buf, normal_core_reset_rsp_prefix, sizeof(normal_core_reset_rsp_prefix))) {
            if (0xFE == recv_buf[len - 1]) {
                usleep_range(WAIT_TIME_10000US, WAIT_TIME_10000US + 100);
                return true;
            } else if (0x00 == recv_buf[len - 1]) {
                continue;
            } else if (0xFF == recv_buf[len - 1]) {
                if (bl_check(client, tms, false)) {
                    return true;
                } else {
                    goto exit;
                }
            }
        }

    } while (retry--);

exit:
    TMS_ERR("Failed to check TMS, force check...\n");
    return bl_check(client, tms, true);
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

    TMS_INFO("Register NFC IRQ[%d]\n", nfc->client->irq);
    return SUCCESS;
}

void nfc_power_control(struct nfc_info *nfc, bool state)
{
    if (!nfc->tms->set_gpio) {
        TMS_ERR("nfc->tms->set_gpio is NULL");
        return;
    }

    if (state == ON) {
        nfc_enable_irq(nfc);
        nfc->tms->set_gpio(nfc->hw_res.ven_gpio, ON, WAIT_TIME_NONE, WAIT_TIME_NONE);
        nfc->tms->ven_enable = true;
    } else if (state == OFF) {
        nfc_disable_irq(nfc);
        nfc->tms->set_gpio(nfc->hw_res.ven_gpio, OFF, WAIT_TIME_NONE, WAIT_TIME_NONE);
        nfc->tms->ven_enable = false;
    }
}

void nfc_fw_download_control(struct nfc_info *nfc, bool state)
{
    if (!nfc->tms->feature.dl_support) {
        TMS_ERR("2026/02/28 enter nfc_fw_download_control\n");
        return;
    }

    if (!nfc->tms->set_gpio) {
        TMS_ERR("nfc->tms->set_gpio is NULL");
        return;
    }

    if (state == ON) {
        nfc->tms->set_gpio(nfc->hw_res.download_gpio, ON, WAIT_TIME_NONE, WAIT_TIME_10000US);
    } else if (state == OFF) {
        nfc->tms->set_gpio(nfc->hw_res.download_gpio, OFF, WAIT_TIME_NONE, WAIT_TIME_10000US);
    }
}

static int ese_power_control(struct nfc_info *nfc, bool state)
{
    if (!nfc->tms->set_gpio) {
        TMS_ERR("nfc->tms->set_gpio is NULL");
        return -ERROR;
    }

    if (state == ON) {
        nfc->tms->ven_enable = gpio_get_value(nfc->hw_res.ven_gpio);

        if (!nfc->tms->ven_enable) {
            nfc->tms->set_gpio(nfc->hw_res.ven_gpio, ON, WAIT_TIME_NONE, WAIT_TIME_NONE);
        }
    } else if (state == OFF) {
        if (!nfc->tms->ven_enable) {
            nfc->tms->set_gpio(nfc->hw_res.ven_gpio, OFF, WAIT_TIME_NONE, WAIT_TIME_NONE);
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
        gpio_free(nfc->hw_res.download_gpio);
        TMS_ERR("enter nfc_gpio_release\n");
    }
}

int nfc_enable_rf_clk(struct nfc_info *nfc)
{
    int ret;

    if (!nfc->tms->feature.rf_clk_enable_support) {
        return SUCCESS;
    }

    ret = IS_ERR(nfc->clk);
    if (ret) {
        TMS_ERR("Check platform clock error, ret = %d\n", ret);
        return ret;
    }

    ret = IS_ERR(nfc->clk_enable);
    if (ret) {
        TMS_ERR("Check clock enable error, ret = %d\n", ret);
        return ret;
    }

    ret = clk_prepare_enable(nfc->clk);
    if (ret) {
        TMS_ERR("Platform clock enable failed, ret = %d\n", ret);
        return ret;
    }

    ret = clk_prepare_enable(nfc->clk_enable);
    if (ret) {
        TMS_ERR("Clock enable failed, ret = %d\n", ret);
    }

    return ret;
}

void nfc_disable_rf_clk(struct nfc_info *nfc)
{
    int ret;

    if (!nfc->tms->feature.rf_clk_enable_support) {
        return;
    }

    ret = IS_ERR(nfc->clk);
    if (ret) {
        TMS_ERR("Check platform clock error, ret = %d\n", ret);
        return;
    }

    ret = IS_ERR(nfc->clk_enable);
    if (ret) {
        TMS_ERR("Check clock enable error, ret = %d\n", ret);
        return;
    }

    clk_disable_unprepare(nfc->clk);
    clk_disable_unprepare(nfc->clk_enable);
}

static ssize_t chip_name_show(struct device *dev, struct device_attribute *attr,
                              char *buf)
{
    struct tms_info *tms;

    tms = tms_common_data_binding();
    if (tms == NULL || tms->vendor == NULL) {
        return (ssize_t)(snprintf(buf, MAX_CHIP_NAME_SIZE - 1, "%s\n", "unknow"));
    }

    return (ssize_t)(snprintf(buf, MAX_CHIP_NAME_SIZE - 1, "%s\n", tms->vendor));
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

}

int nfc_regulator_get(struct device *i2c_dev, struct ldo *ldo)
{
    if (!ldo || !i2c_dev) {
        TMS_ERR("invalid parameters\n");
        return -EINVAL;
    }

    TMS_INFO("Get the regulator handle\n");
    ldo->reg = regulator_get(i2c_dev, "tms,vddio");
    if (IS_ERR_OR_NULL(ldo->reg)) {
        TMS_ERR("Unable to get regulator handle for vddio, ret = %ld\n",
                IS_ERR(ldo->reg) ? PTR_ERR(ldo->reg) : -EINVAL);
        return -EINVAL;
    }

    return SUCCESS;
}

void nfc_regulator_put(struct ldo *ldo)
{
    if (!ldo) {
        TMS_ERR("invalid parameters\n");
        return;
    }

    if (ldo->reg) {
        TMS_INFO("Put regulator\n");
        regulator_put(ldo->reg);
        ldo->reg = NULL;
    }
}

int nfc_ldo_vote(struct ldo *ldo)
{
    int ret;

    if (!ldo) {
        TMS_ERR("invalid parameters\n");
        return -EINVAL;
    }

    if (!ldo->reg) {
        return SUCCESS;
    }

    TMS_INFO("Do ldo vote\n");
    ret = regulator_set_voltage(ldo->reg,
                                (int)ldo->voltage_range[0],
                                (int)ldo->voltage_range[1]);
    if (ret < 0) {
        TMS_ERR("Set voltage failed, ret=%d\n", ret);
        return ret;
    }

    /* pass expected current from NFC in uA */
    ret = regulator_set_load(ldo->reg, (int)ldo->load_current);
    if (ret < 0) {
        TMS_ERR("Set load failed, ret=%d\n", ret);
        return ret;
    }

    ret = regulator_enable(ldo->reg);
    if (ret < 0) {
        TMS_ERR("Regulator_enable failed, ret=%d\n", ret);
        return ret;
    }

    ldo->enabled = true;
    return SUCCESS;
}

void nfc_ldo_unvote(struct ldo *ldo)
{
    int ret;

    if (!ldo) {
        TMS_ERR("invalid parameters");
        return;
    }

    if (!ldo->reg) {
        return;
    }

    if (!ldo->enabled) {
        TMS_ERR("regulator already disabled\n");
        return;
    }

    TMS_INFO("Do ldo unvote\n");
    ret = regulator_disable(ldo->reg);
    if (ret < 0) {
        TMS_ERR("Regulator_disable failed, ret=%d\n", ret);
    }

    ldo->enabled = false;
}

static int nfc_gpio_configure_init(struct nfc_info *nfc)
{
    int ret;

    if (gpio_is_valid(nfc->hw_res.irq_gpio)) {
        ret = gpio_direction_input(nfc->hw_res.irq_gpio);

        if (ret < 0) {
            TMS_ERR("Unable to set irq gpio as input\n");
            return ret;
        }
    }

    if (gpio_is_valid(nfc->hw_res.ven_gpio)) {
        ret = gpio_direction_output(nfc->hw_res.ven_gpio, nfc->hw_res.ven_flag);

        if (ret < 0) {
            TMS_ERR("Unable to set ven gpio as output\n");
            return ret;
        }
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
    int ret = SUCCESS;

    nfc->clk = devm_clk_get(nfc->i2c_dev, "clk_aux");

    ret = IS_ERR(nfc->clk);
    if (ret) {
        TMS_WARN("Platform clock not specified, ret = %d\n", ret);
        return ret;
    }

    nfc->clk_parent = devm_clk_get(nfc->i2c_dev, "source");

    ret = IS_ERR(nfc->clk_parent);
    if (ret) {
        TMS_ERR("Clock parent not specified, ret = %d\n", ret);
        return ret;
    }

    clk_set_parent(nfc->clk, nfc->clk_parent);
    clk_set_rate(nfc->clk, 26000000);
    nfc->clk_enable = devm_clk_get(nfc->i2c_dev, "enable");

    ret = IS_ERR(nfc->clk_enable);
    if (ret) {
        TMS_ERR("Clock enable not specified, ret = %d\n", ret);
        return ret;
    }

    if (!nfc->tms->feature.rf_clk_enable_support) {
        ret = clk_prepare_enable(nfc->clk);
        if (ret) {
            TMS_ERR("Platform clock enable failed, ret = %d\n", ret);
            return ret;
        }

        ret = clk_prepare_enable(nfc->clk_enable);
        if (ret) {
            TMS_ERR("Clock enable failed, ret = %d\n", ret);
        }
    }

    return ret;
}

static int nfc_parse_dts_init(struct nfc_info *nfc)
{
    int ret, rcv;
    struct device_node *np;

    np = nfc->i2c_dev->of_node;
    nfc->tms->feature.rf_clk_enable_support = of_property_read_bool(np, "rf_clk_enable_support");
    rcv = of_property_read_string(np, "tms,device-name", &nfc->dev.name);

    if (rcv < 0) {
        nfc->dev.name = "tms_nfc";
        TMS_WARN("device-name not specified, set default\n");
    }

    rcv = of_property_read_u32(np, "tms,device-count", &nfc->dev.count);

    if (rcv < 0) {
        nfc->dev.count = 1;
        TMS_WARN("device-count not specified, set default\n");
    }

    nfc->hw_res.irq_gpio = of_get_named_gpio(np, "tms,irq-gpio", 0);

    if (gpio_is_valid(nfc->hw_res.irq_gpio)) {
        rcv = gpio_request(nfc->hw_res.irq_gpio, "nfc_int");

        if (rcv) {
            TMS_WARN("Unable to request gpio[%d] as IRQ\n",
                     nfc->hw_res.irq_gpio);
        }
    } else {
        TMS_ERR("Irq gpio not specified\n");
        return -EINVAL;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,3,0)
    nfc->hw_res.ven_gpio = of_get_named_gpio_flags(np, "tms,ven-gpio", 0,
                                                   &nfc->hw_res.ven_flag);
#else
    nfc->hw_res.ven_gpio = of_get_named_gpio(np, "tms,ven-gpio", 0);
    nfc->hw_res.ven_flag = 1;
#endif

    if (gpio_is_valid(nfc->hw_res.ven_gpio)) {
        rcv = gpio_request(nfc->hw_res.ven_gpio, "nfc_ven");

        if (rcv) {
            TMS_WARN("Unable to request gpio[%d] as VEN\n",
                     nfc->hw_res.ven_gpio);
        }
    } else {
        TMS_ERR("Ven gpio not specified\n");
        ret =  -EINVAL;
        goto err_free_irq;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,3,0)
    nfc->hw_res.download_gpio = of_get_named_gpio_flags(np, "tms,download-gpio", 0,
                                                        &nfc->hw_res.download_flag);
#else
    nfc->hw_res.download_gpio = of_get_named_gpio(np, "tms,download-gpio", 0);
    nfc->hw_res.download_flag = 0;
#endif

    if (gpio_is_valid(nfc->hw_res.download_gpio)) {
        nfc->tms->feature.dl_support = true;
        rcv = gpio_request(nfc->hw_res.download_gpio, "nfc_fw_download");

        if (rcv) {
            TMS_WARN("Unable to request gpio[%d] as DownLoad\n",
                     nfc->hw_res.download_gpio);
        }
    } else {
        nfc->tms->feature.dl_support = false;
        TMS_WARN("Download gpio not specified or non-pin\n");
    }

    if (of_get_property(np, "tms,vddio-supply", NULL)) {
        ret = of_property_read_u32_array(np, "tms,vddio-voltage",
              nfc->ldo.voltage_range, ARRAY_SIZE(nfc->ldo.voltage_range));
        if (ret) {
            TMS_ERR("Unable to read NFC voltage value\n");
            goto err_free_irq;
        }

        ret = of_property_read_u32(np, "tms,vddio-current", &nfc->ldo.load_current);
        if (ret) {
            TMS_ERR("Unable to read NFC current value\n");
            goto err_free_irq;
        }

        ret = nfc_regulator_get(nfc->i2c_dev, &nfc->ldo);
        if (ret) {
            TMS_ERR("Unable to get NFC regulator\n");
            goto err_free_irq;
        }
    } else {
        TMS_WARN("Vddio supply not specified");
    }

    TMS_DEBUG("NFC device name is %s, count = %d\n", nfc->dev.name,
              nfc->dev.count);
    TMS_INFO("irq_gpio = %d, ven_gpio = %d, download_gpio = %d\n",
             nfc->hw_res.irq_gpio, nfc->hw_res.ven_gpio,
             nfc->tms->feature.dl_support ? nfc->hw_res.download_gpio : -1);
    return SUCCESS;
err_free_irq:
    gpio_free(nfc->hw_res.irq_gpio);
    TMS_ERR("Failed, ret = %d\n", ret);
    return ret;
}

int nfc_common_info_init(struct nfc_info *nfc)
{
    int ret;
    TMS_INFO("Enter\n");
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

    /* step3 : LDO vote */
    ret = nfc_ldo_vote(&nfc->ldo);
    if (ret) {
        TMS_ERR("LDO voting failed, ret = %d\n", ret);
        goto err_free_gpio;
    }

    /* step4 : Configure platform clock */
    ret = nfc_platform_clk_init(nfc);

    if (ret) {
        TMS_WARN("Do not configure platform clock\n");
    }

    /* step5 : set gpio work mode */
    ret = nfc_gpio_configure_init(nfc);

    if (ret) {
        TMS_ERR("Init gpio control failed.\n");
        goto err_ldo_unvote;
    }

    /* step6 : binding common function */
    nfc->tms->hw_res.ven_gpio = nfc->hw_res.ven_gpio;
    nfc->tms->hw_res.download_gpio = nfc->hw_res.download_gpio;
    nfc->tms->hw_res.irq_gpio = nfc->hw_res.irq_gpio;
    nfc->tms->ven_enable      = false;
    TMS_INFO("Successfully\n");
    return SUCCESS;
err_ldo_unvote:
    nfc_ldo_unvote(&nfc->ldo);
err_free_gpio:
    nfc_regulator_put(&nfc->ldo);
    nfc_gpio_release(nfc);
    TMS_ERR("Failed, ret = %d\n", ret);
    return ret;
}
