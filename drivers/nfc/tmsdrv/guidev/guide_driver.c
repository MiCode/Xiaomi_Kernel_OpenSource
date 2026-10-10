/*****************************************************************************************
 * Copyright (c)  2021- 2022  TsingTeng MicroSystem Corp.Ltd.
 * TMS_EDIT
 * File       : guide_driver.c
 * Description: Source file for tms devices guide
 * Version    : 1.0
 * Date       : 2022/4/11
 * Team       : NFC Middleware
 * Author     : Guoliang Wu
 * --------------------------------- Revision History: ---------------------------------
 *   <version>    <date>          < author >                            <desc>
 *******************************************************************************************/
#include "guide_driver.h"

/*********** PART0: Global Variables Area ***********/
struct guide_dev *guidev = NULL;
static chip_t chip_type = TMS_THN31;
const char *THN31S_chip_name= "THN31SWC2";
const char *THN31F_chip_name = "THN31FGB1A";
const char *SN220X_chip_name = "SN220X";
const uint8_t THN31F_major_version = 0xD2;
const uint8_t THN31S_major_version= 0xD1;
const uint8_t SN220X_major_version = 0x01;
/*********** PART1: Function declarations Area ***********/
#if defined(CONFIG_TMS_ESE_DEVICE) || defined(CONFIG_TMS_ESE_DEVICE_MODULE)
extern int ese_driver_init(void);
extern void ese_driver_exit(void);
#endif
//extern int xxx_probe(struct i2c_client *client, const struct i2c_device_id *id);
//extern int xxx_remove(struct i2c_client *client);
//extern int xxx_suspend(struct device *device);
//extern int xxx_resume(struct device *device);
//extern int xxx_ese_dev_init(void);
//extern void xxx_ese_dev_exit(void);
//tms add nxp compat begin
extern int nfc_i2c_dev_probe(struct i2c_client *client, const struct i2c_device_id *id);
extern void nfc_i2c_dev_remove(struct i2c_client *client);
extern int nfc_i2c_dev_suspend(struct device *device);
extern int nfc_i2c_dev_resume(struct device *device);
extern int p61_dev_init(void);
extern void p61_dev_exit(void);
//tms add nxp compat end
/*********** PART2: Guidev Driver Start Area ***********/
/*****************************************************************************
* boardid check functions
*****************************************************************************/
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

    if (!tms_nfc_write(client, get_bl_ver, sizeof(get_bl_ver))) {
        return false;
    }

    do {
        if (!tms_nfc_read(client, tms->hw_res.irq_gpio, recv_buf, T1_HDR_LEN, "bl_ver_h")) {
            break;
        }

        len = T1_HDR_LEN;
        payload_len = recv_buf[T1_PAYLOAD_LEN_BYTE];

        if (payload_len > (MAX_T1_BUFFER_SIZE - T1_HDR_LEN - T1_LRC_LEN)) {
            TMS_ERR("Payload length %zu exceeds buffer size\n", payload_len);
            break;
        }

        if (!tms_nfc_read(client, tms->hw_res.irq_gpio, recv_buf + T1_HDR_LEN,
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

    if (!tms_nfc_write(client, get_hw_status, sizeof(get_hw_status))) {
        return;
    }

    do {
        if (!tms_nfc_read(client, tms->hw_res.irq_gpio, recv_buf, T1_HDR_LEN, "hw_status_h")) {
            break;
        }

        len = T1_HDR_LEN;
        payload_len = recv_buf[T1_PAYLOAD_LEN_BYTE];

        if (payload_len > (MAX_T1_BUFFER_SIZE - T1_HDR_LEN - T1_LRC_LEN)) {
            TMS_ERR("Payload length %zu exceeds buffer size\n", payload_len);
            break;
        }

        if (!tms_nfc_read(client, tms->hw_res.irq_gpio, recv_buf + T1_HDR_LEN,
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

    tms_nfc_hard_reset(tms);

    if (!tms_nfc_write(client, bl_reset, sizeof(bl_reset))) {
        goto exit;
    }

    do {
        if (!tms_nfc_read(client, tms->hw_res.irq_gpio, recv_buf, T1_HDR_LEN, "bl_reset_h")) {
            break;
        }

        len = T1_HDR_LEN;
        payload_len = recv_buf[T1_PAYLOAD_LEN_BYTE];

        if (payload_len > (MAX_T1_BUFFER_SIZE - T1_HDR_LEN - T1_LRC_LEN)) {
            TMS_ERR("Payload length %zu exceeds buffer size\n", payload_len);
            break;
        }

        if (!tms_nfc_read(client, tms->hw_res.irq_gpio, recv_buf + T1_HDR_LEN,
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

    tms_nfc_hard_reset(tms);

    if (!tms_nfc_write(client, core_reset, sizeof(core_reset))) {
        goto exit;
    }

    do {
        if (!tms_nfc_read(client, tms->hw_res.irq_gpio, recv_buf, NCI_HDR_LEN, "core_reset_h")) {
            break;
        }

        len = NCI_HDR_LEN;
        payload_len = recv_buf[NCI_PAYLOAD_LEN_BYTE];
        is_two_byte = (payload_len == 1) ? 1 : 0;

        if (payload_len > (MAX_NCI_BUFFER_SIZE - NCI_HDR_LEN)) {
            TMS_ERR("Payload length %zu exceeds buffer size\n", payload_len);
            break;
        }

        if (!tms_nfc_read(client, tms->hw_res.irq_gpio, recv_buf + NCI_HDR_LEN,
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
//return failed: < 0  success:0
int get_board_id_from_dtb(char *board_mem) {
    struct device_node *proj_info_node;
    const char *read_board_id;
    char *ptr=NULL;
    int res_len=-1;
    char board_id_word[]="androidboot.board_id=";
    proj_info_node  = of_find_node_by_name(NULL,"chosen");
    if (NULL == proj_info_node) {
        TMS_ERR("%s:device chosen node not exist.",__func__);
        return res_len;
    }
    res_len = of_property_read_string(proj_info_node,"hwinfo",&read_board_id);
    if (res_len != 0) {
        TMS_ERR("%s:device bootargs read fail:res_len=%d.",__func__,res_len);
        return res_len;
    }
    TMS_INFO("%s:get chosen node and hwinfo success, hwinfo=%s\n",__func__,read_board_id);
    ptr = strstr(read_board_id,board_id_word);
    if (ptr != NULL) {
        strncpy(board_mem, ptr+strlen(board_id_word), 9); //copy the result to pointer
        board_mem[9] = '\0';
        TMS_INFO("%s, get result %s .\n", __func__,board_mem);
        return 0;
    }
    TMS_ERR("%s:strstr get pointer board_id failed.",__func__);
    return -1;
}

bool support_download_gpio(void)
{
    int ret;
    char boardid[16]={0};
    bool dl_support = true;
    TMS_INFO("enter check if support download_gpio");
    ret = get_board_id_from_dtb(boardid);
    if (ret == 0) {
        TMS_INFO("boardid get success: %s.",boardid);
        if(!strcmp(boardid, "1111"))
        {
            TMS_INFO("chip is not support download_gpio");
            dl_support = false;
        }else {
            dl_support = true;
        }
    }
    return dl_support = false;
}

static void guidev_resource_release(struct guide_dev *gdata)
{
    gpio_free(gdata->hw_res.irq_gpio);
    gpio_free(gdata->hw_res.ven_gpio);
    if (gdata->tms->feature.dl_support) {
        gpio_free(gdata->hw_res.download_gpio);
    }
    TMS_DEBUG("Finished");
}

static int guidev_probe_driver(struct i2c_client *client,
                               const struct i2c_device_id *id, chip_t chip_type)
{
    int ret = SUCCESS;
    guidev_resource_release(guidev);

    switch (chip_type) {
    case TMS_THN31:
        TMS_INFO("TMS THN31 driver start probe\n");
        ret = nfc_device_probe(client, id);
#if defined(CONFIG_TMS_ESE_DEVICE) || defined(CONFIG_TMS_ESE_DEVICE_MODULE)
        ese_driver_init();
#endif
        break;

    case NXP_SN110X:
        TMS_INFO("NXP SN110X driver start probe\n");
        nfc_i2c_dev_probe(client, id);
        TMS_INFO("NXP SN110X ese driver start probe\n");
        p61_dev_init();
        break;

    case SAMPLE_DEV_2:
        //TMS_INFO("XXX XXX driver start probe\n");
        //ret = sample_dev_2_probe(client, id);
        //sample_dev_2_ese_driver_init();
        break;

    default:
        TMS_ERR("Unknow chip %s[%d]\n", guidev->tms->nfc_name, chip_type);
        break;
    }

    return ret;
}

static int guidev_parse_dts(struct guide_dev *gdata)
{
    int rc;
    int ret = 0;
    struct device_node *np;
    TMS_INFO("Start+\n");
    np = gdata->dev->of_node;
    gdata->hw_res.irq_gpio = of_get_named_gpio(np, "tms,irq-gpio", 0);

    if (gpio_is_valid(gdata->hw_res.irq_gpio)) {
        rc = gpio_request(gdata->hw_res.irq_gpio, "guidev_int");

        if (rc) {
            TMS_WARN("unable to request gpio[%d] as irq\n", gdata->hw_res.irq_gpio);
        }
    } else {
        TMS_ERR("irq gpio not specified in dts\n");
        return -EINVAL;
    }

    gdata->hw_res.ven_gpio = of_get_named_gpio_flags(np, "tms,ven-gpio", 0,
                             &gdata->hw_res.ven_flag);

    if (gpio_is_valid(gdata->hw_res.ven_gpio)) {
        rc = gpio_request(gdata->hw_res.ven_gpio, "guidev_ven");

        if (rc) {
            TMS_WARN("unable to request gpio[%d] as ven\n", gdata->hw_res.ven_gpio);
        }
    } else {
        TMS_ERR("ven gpio not specified in dts\n");
        ret =  -EINVAL;
        goto err_ven;
    }

    gdata->hw_res.download_gpio = of_get_named_gpio_flags(np, "tms,download-gpio",
                                  0,
                                  &gdata->hw_res.download_flag);

    if(!(support_download_gpio()))
    {
        gdata->tms->feature.dl_support = false;
    }else{
        gdata->tms->feature.dl_support = true;
    }

    if(gdata->tms->feature.dl_support == true) {
        if (gpio_is_valid(gdata->hw_res.download_gpio)) {
            rc = gpio_request(gdata->hw_res.download_gpio, "guidev_fw_download");

            if (rc) {
                TMS_WARN("unable to request gpio[%d] as download\n",
                     gdata->hw_res.download_gpio);
            }

        } else {
            TMS_ERR("fw-download gpio not specified in dts\n");
            ret = -EINVAL;
            goto err_fw_download;
        }
    }

    TMS_DEBUG("irq_gpio = %d, ven_gpio = %d, dwnld_gpio = %d, error:%d\n",
              gdata->hw_res.irq_gpio, gdata->hw_res.ven_gpio, gdata->hw_res.download_gpio,
              ret);
    TMS_INFO("Normal end-\n");
    return SUCCESS;
err_fw_download:
    gpio_free(gdata->hw_res.ven_gpio);
err_ven:
    gpio_free(gdata->hw_res.irq_gpio);
    TMS_ERR("Error end, ret = %d\n", ret);
    return ret;
}

static int guidev_gpio_configure(struct hw_resource hw_res)
{
    int ret;
    TMS_INFO("Start+\n");

    if (gpio_is_valid(hw_res.irq_gpio)) {
        ret = gpio_direction_input(hw_res.irq_gpio);

        if (ret < 0) {
            TMS_ERR("not able to set irq gpio as input\n");
            return -ERROR;
        }

        TMS_INFO("set irq gpio as input\n");
    }

    if (gpio_is_valid(hw_res.ven_gpio)) {
        ret = gpio_direction_output(hw_res.ven_gpio, hw_res.ven_flag);

        if (ret < 0) {
            TMS_ERR("not able to set ven gpio as output\n");
            return -ERROR;
        }

        TMS_INFO("set ven gpio as output\n");
    }

    if (gpio_is_valid(hw_res.download_gpio)) {
        ret = gpio_direction_output(hw_res.download_gpio, hw_res.download_flag);

        if (ret < 0) {
            TMS_ERR("not able to set dwnld gpio as output\n");
            return -ERROR;
        }

        TMS_INFO("set dwnld gpio as output\n");
    }

    TMS_INFO("Normal end-\n");
    return SUCCESS;
}

static int guidev_dev_probe(struct i2c_client *client,
                            const struct i2c_device_id *id)
{
    int ret;
    TMS_INFO("Start+\n");
    /* step1 : alloc data */
    guidev = kzalloc(sizeof(struct guide_dev), GFP_KERNEL);

    if (guidev == NULL) {
        TMS_ERR("Guide info alloc memory error\n");
        return -ENOMEM;
    }

    /* step2 : binding tms common data */
    guidev->tms = tms_common_data_binding();

    if (guidev->tms == NULL) {
        TMS_ERR("Get tms common info  error\n");
        ret = -ENOMEM;
        goto err_free_guidev;
    }

    /* step3 : init and binding parameters for easy operate */
    guidev->client        = client;
    guidev->dev           = &client->dev;

    /* step4 : dts parse */
    ret = guidev_parse_dts(guidev);

    if (ret) {
        TMS_ERR("failed to parse dts\n");
        goto err_free_guidev;
    }

    /* step5 : I2C function check */
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        TMS_ERR("need I2C_FUNC_I2C\n");
        guidev_resource_release(guidev);
        ret = -ENODEV;
        goto err_free_guidev;
    }

    /* step6 : Set gpio */
    ret = guidev_gpio_configure(guidev->hw_res);

    if (ret) {
        TMS_ERR("failed to configure\n");
        guidev_resource_release(guidev);
        goto err_free_guidev;
    }

    guidev->tms->hw_res.ven_gpio = guidev->hw_res.ven_gpio;
    guidev->tms->hw_res.download_gpio = guidev->hw_res.download_gpio;
    guidev->tms->hw_res.irq_gpio = guidev->hw_res.irq_gpio;

    nfc_hw_check(guidev->client, guidev->tms);
    /* step7 : match nfc chip info */
    guidev->tms->nfc_name = "thn31";
    chip_type = TMS_THN31;
    TMS_INFO("nfc chip type is %s", guidev->tms->nfc_name);

    /* step8 : Probe driver */
    ret = guidev_probe_driver(client, id, chip_type);

    if (ret) {
        TMS_ERR("failed to configure\n");
        goto err_free_guidev;
    }

    /* step9 : Create system property node */
    ret = nfc_create_sysfs_interfaces(&client->dev);

    if (ret < 0) {
        TMS_ERR("Create sysfs interface failed\n");
        goto err_free_guidev;
    }

    ret = sysfs_create_link(NULL, &client->dev.kobj, "nfc");

    if (ret < 0) {
        TMS_ERR("create sysfs link failed\n");
        goto err_remove_sysfs_interface;
    }

    TMS_INFO("probe finished\n");
    return SUCCESS;
err_remove_sysfs_interface:
    nfc_remove_sysfs_interfaces(&client->dev);
err_free_guidev:
    kfree(guidev);
    guidev = NULL;
    TMS_ERR("probe failed, check hardware\n");
    return ret;
}

static int guidev_dev_remove(struct i2c_client *client)
{
    switch (chip_type) {
    case TMS_THN31:
        nfc_device_remove(client);
        TMS_INFO("TMS THN31 nfc start removed\n");
        break;

    case NXP_SN110X:
        TMS_INFO("NXP SN110X nfc start removed\n");
        nfc_i2c_dev_remove(client);
        break;

    case SAMPLE_DEV_2:
        //TMS_INFO("XXX XXX nfc start removed\n");
        //sample_dev_2_remove(client);
        break;

    default:
        TMS_ERR("No chip to remove %d\n", chip_type);
        break;
    }

    nfc_remove_sysfs_interfaces(&client->dev);
    kfree(guidev);
    guidev = NULL;
    return 0;
}

void guidev_dev_shutdown(struct i2c_client *client)
{
    if (!guidev || !guidev->tms || !guidev->tms->set_ven) {
        TMS_ERR("set_ven is NULL");
        return;
    }

    guidev->tms->set_ven(guidev->hw_res, OFF);
    msleep(20); // hard reset guard time
}

static int guide_suspend(struct device *device)
{
    switch (chip_type) {
    case TMS_THN31:
        TMS_INFO("TMS THN31 nfc start suspend\n");
        nfc_device_suspend(device);
        break;

    case NXP_SN110X:
        TMS_INFO("NXP SN110X nfc start suspend\n");
        nfc_i2c_dev_suspend(device);
        break;

    case SAMPLE_DEV_2:
        //TMS_INFO("XXX XXX nfc start suspend\n");
        //sample_dev_2_suspend(device);
        break;

    default:
        TMS_ERR("No chip to suspend %d\n", chip_type);
        break;
    }

    return SUCCESS;
}

static int guide_resume(struct device *device)
{
    switch (chip_type) {
    case TMS_THN31:
        TMS_INFO("TMS THN31 nfc start resume\n");
        nfc_device_resume(device);
        break;

    case NXP_SN110X:
        TMS_INFO("NXP SN110X nfc start resume\n");
        nfc_i2c_dev_resume(device);
        break;

    case SAMPLE_DEV_2:
        //TMS_INFO("XXX XXX nfc start resume\n");
        //sample_dev_2_suspend(device);
        break;

    default:
        TMS_ERR("No chip to resume %d\n", chip_type);
        break;
    }

    return SUCCESS;
}

static const struct i2c_device_id guide_dev_id[] = {
    {GUIDEDEV_NAME, 0 },
    { }
};

static struct of_device_id guide_match_table[] = {
    { .compatible = GUIDEDEV_NAME, },
    { }
};

static const struct dev_pm_ops guide_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(guide_suspend, guide_resume)
};

static struct i2c_driver tms_guide_driver = {
    .id_table = guide_dev_id,
    .probe  = guidev_dev_probe,
    .remove = guidev_dev_remove,
    .shutdown = guidev_dev_shutdown,
    .driver = {
        .name           = GUIDEDEV_NAME,
        .of_match_table = guide_match_table,
        .pm             = &guide_pm_ops,
        .probe_type     = PROBE_PREFER_ASYNCHRONOUS,
    },
};

MODULE_DEVICE_TABLE(of, guide_match_table);

int tms_guide_init(void)
{
    int ret = 0;
    TMS_INFO("Loading guide driver\n");
    ret = i2c_add_driver(&tms_guide_driver);

    if (ret != 0) {
        TMS_ERR("Add driver error ret = %d\n", ret);
    }

    return ret;
}

void tms_guide_exit(void)
{
    switch (chip_type) {
    case TMS_THN31:
#if defined(CONFIG_TMS_ESE_DEVICE) || defined(CONFIG_TMS_ESE_DEVICE_MODULE)
        ese_driver_exit();
#endif
        break;

    case NXP_SN110X:
        p61_dev_exit();
        break;

    case SAMPLE_DEV_2:
        //sample_dev_2_driver_exit(device);
        break;

    default:
        TMS_ERR("No chip to resume %d\n", chip_type);
        break;
    }

    TMS_INFO("Unloading guide driver\n");
    i2c_del_driver(&tms_guide_driver);
}

MODULE_DESCRIPTION("TMS Guide Driver");
MODULE_LICENSE("GPL v2");
