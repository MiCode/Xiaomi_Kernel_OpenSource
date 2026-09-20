// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 TsingTeng MicroSystem Co., Ltd.
 */

#include "guide_driver.h"

/*********** PART0: Global Variables Area ***********/
struct guide_dev *guidev = NULL;
static chip_t chip_type = TMS_NFC;

/*********** PART1: Function declarations Area ***********/
#define NCI_VENDOR_ID_BYTE 6
static const uint8_t core_reset[] = {0x20, 0x00, 0x01, 0x00};
//extern int xxx_probe(struct i2c_client *client, const struct i2c_device_id *id);
//extern int xxx_remove(struct i2c_client *client);
//extern int xxx_suspend(struct device *device);
//extern int xxx_resume(struct device *device);
//extern int xxx_ese_dev_init(void);
//extern void xxx_ese_dev_exit(void);

/*********** PART2: Guidev Driver Start Area ***********/
bool get_board_id_from_dtb(char boardidinfo[]) {
    struct device_node *np;
    const char *boardid_str;
    const void *boardid_raw;
    int len;
    np = of_find_node_by_path("/project-info");
    if (!np) {
        TMS_ERR("/project-info node not found\n");
        return false;
    }
    //board id
    if (!of_property_read_string(np, "board_id", &boardid_str)) {
        memcpy(boardidinfo, boardid_str, sizeof(boardid_str)+1);
        TMS_ERR("of_property_read_string board_id = %s\n", boardidinfo);
    } else {
        boardid_raw = of_get_property(np, "board_id", &len);
        if (!boardid_raw || len <= 0) {
            TMS_ERR(" board_id property not found or empty\n");
            return false;  
        }
        //memcpy(hwlevel, hwlevel_raw, min(len, sizeof(hwlevel) - 1));
        memcpy(boardidinfo, boardid_raw, min_t(size_t, len, strlen(boardidinfo) - 1));
    }
    of_node_put(np);  // 释放节点引用
    TMS_ERR(" board_id = %s\n", boardidinfo);
    return true;
}
bool support_download_gpio(void)
{
    bool ret;
    char boardid[32]={0};
    bool dl_support = true;
    TMS_INFO("enter check if support download_gpio");
    ret = get_board_id_from_dtb(boardid);
    if (ret) {
        if(!strcmp(boardid, "111111111"))
        {
            dl_support = true;
        }else {
            TMS_INFO("chip is not support download_gpio");
            dl_support = false;
        }
    }
    return dl_support;
}
bool other_nfc_match(struct i2c_client *client, struct tms_info *tms)
{
    const uint8_t normal_core_reset_ntf_prefix[] = {0x60, 0x00, 0x0A, 0x02};
    uint8_t normal_core_reset_ntf_len_min = 8;
    uint8_t retry = 5;
    size_t len = 0;
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
        return false;
    }

    do {
        if (!nfc_read(client, tms->hw_res.irq_gpio, recv_buf, NCI_HDR_LEN, "core_reset_h")) {
            break;
        }

        len = NCI_HDR_LEN;
        payload_len = recv_buf[NCI_PAYLOAD_LEN_BYTE];

        if (!nfc_read(client, tms->hw_res.irq_gpio, recv_buf + NCI_HDR_LEN,
            payload_len, "core_reset_t")) {
            break;
        }

        len += payload_len;

        if ((len >= normal_core_reset_ntf_len_min) &&
            (!memcmp(recv_buf, normal_core_reset_ntf_prefix, sizeof(normal_core_reset_ntf_prefix))) &&
            (recv_buf[NCI_VENDOR_ID_BYTE] == 0x04)) {
            return true;
        }

    } while (retry--);

    TMS_ERR("Not Match\n");
    return false;
}

static void guidev_resource_release(struct guide_dev *gdata)
{
    gpio_free(gdata->hw_res.irq_gpio);
    gpio_free(gdata->hw_res.ven_gpio);

    if (gdata->tms->feature.dl_support) {
        gpio_free(gdata->hw_res.download_gpio);
    }
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,3,0)
static int guidev_probe_driver(struct i2c_client *client,
                               const struct i2c_device_id *id, chip_t chip_type)
#else
static int guidev_probe_driver(struct i2c_client *client, chip_t chip_type)
#endif
{
    int ret = SUCCESS;
    guidev_resource_release(guidev);

    switch (chip_type) {
    case TMS_NFC:
#if LINUX_VERSION_CODE < KERNEL_VERSION(6,3,0)
        ret = nfc_device_probe(client, id);
#else
        ret = nfc_device_probe(client);
#endif
#if IS_ENABLED(CONFIG_TMS_ESE_DEVICE)
        ese_driver_init();
#endif
        break;

    case OTHER_NFC:
        //TMS_INFO("XXX XXX driver start probe\n");
        //ret = xxx_nfc_probe(client, id);
        //xxx_ese_driver_init();
        break;

    default:
        TMS_ERR("%s chip [%d]\n", guidev->tms->vendor, chip_type);
        break;
    }

    return ret;
}

static int guidev_parse_dts(struct guide_dev *gdata)
{
    int rc;
    int ret = 0;
    struct device_node *np;

    np = gdata->dev->of_node;
    gdata->hw_res.irq_gpio = of_get_named_gpio(np, "tms,irq-gpio", 0);

    if (gpio_is_valid(gdata->hw_res.irq_gpio)) {
        rc = gpio_request(gdata->hw_res.irq_gpio, "guidev_int");

        if (rc) {
            TMS_WARN("unable to request gpio[%d] as irq\n", gdata->hw_res.irq_gpio);
        }
    } else {
        TMS_ERR("irq gpio not specified\n");
        return -EINVAL;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,3,0)
    gdata->hw_res.ven_gpio = of_get_named_gpio_flags(np, "tms,ven-gpio", 0,
                                                     &gdata->hw_res.ven_flag);
#else
    gdata->hw_res.ven_gpio = of_get_named_gpio(np, "tms,ven-gpio", 0);
    gdata->hw_res.ven_flag = 1;
#endif

    if (gpio_is_valid(gdata->hw_res.ven_gpio)) {
        rc = gpio_request(gdata->hw_res.ven_gpio, "guidev_ven");

        if (rc) {
            TMS_WARN("unable to request gpio[%d] as ven\n", gdata->hw_res.ven_gpio);
        }
    } else {
        TMS_ERR("ven gpio not specified\n");
        ret =  -EINVAL;
        goto err_ven;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,3,0)
    gdata->hw_res.download_gpio = of_get_named_gpio_flags(np, "tms,download-gpio",
                                                          0,
                                                          &gdata->hw_res.download_flag);
#else
    gdata->hw_res.download_gpio = of_get_named_gpio(np, "tms,download-gpio", 0);
    gdata->hw_res.download_flag = 0;
#endif

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

    if (of_get_property(np, "tms,vddio-supply", NULL)) {
        ret = of_property_read_u32_array(np, "tms,vddio-voltage",
              gdata->ldo.voltage_range, ARRAY_SIZE(gdata->ldo.voltage_range));
        if (ret) {
            TMS_ERR("Unable to read NFC voltage value\n");
            goto err_ven;
        }

        ret = of_property_read_u32(np, "tms,vddio-current", &gdata->ldo.load_current);
        if (ret) {
            TMS_ERR("Unable to read NFC current value\n");
            goto err_ven;
        }

        ret = nfc_regulator_get(gdata->dev, &gdata->ldo);
        if (ret) {
            TMS_ERR("Unable to get NFC regulator\n");
            goto err_ven;
        }
    } else {
        TMS_WARN("Vddio supply not specified");
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

    if (gpio_is_valid(hw_res.irq_gpio)) {
        ret = gpio_direction_input(hw_res.irq_gpio);

        if (ret < 0) {
            TMS_ERR("not able to set irq gpio as input\n");
            return -ERROR;
        }
    }

    if (gpio_is_valid(hw_res.ven_gpio)) {
        ret = gpio_direction_output(hw_res.ven_gpio, hw_res.ven_flag);

        if (ret < 0) {
            TMS_ERR("not able to set ven gpio as output\n");
            return -ERROR;
        }
    }

    if (gpio_is_valid(hw_res.download_gpio)) {
        ret = gpio_direction_output(hw_res.download_gpio, hw_res.download_flag);

        if (ret < 0) {
            TMS_ERR("not able to set dwnld gpio as output\n");
            return -ERROR;
        }
    }

    return SUCCESS;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,3,0)
static int guidev_dev_probe(struct i2c_client *client,
                            const struct i2c_device_id *id)
#else
static int guidev_dev_probe(struct i2c_client *client)
#endif
{
    int ret;
    TMS_INFO("Enter\n");
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

    /* step5 : LDO vote */
    ret = nfc_ldo_vote(&guidev->ldo);
    if (ret) {
        TMS_ERR("LDO voting failed, ret = %d\n", ret);
        guidev_resource_release(guidev);
        goto err_put_regulator;
    }

    /* step6 : I2C function check */
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        TMS_ERR("need I2C_FUNC_I2C\n");
        guidev_resource_release(guidev);
        ret = -ENODEV;
        goto err_free_ldo;
    }

    /* step7 : Set gpio */
    ret = guidev_gpio_configure(guidev->hw_res);

    if (ret) {
        TMS_ERR("failed to configure\n");
        guidev_resource_release(guidev);
        goto err_free_ldo;
    }

    guidev->tms->hw_res.ven_gpio = guidev->hw_res.ven_gpio;
    guidev->tms->hw_res.download_gpio = guidev->hw_res.download_gpio;
    guidev->tms->hw_res.irq_gpio = guidev->hw_res.irq_gpio;

    /* step7 : match nfc chip info */

    if (nfc_hw_check(guidev->client, guidev->tms)) {
        chip_type = TMS_NFC;
    } else if (other_nfc_match(guidev->client, guidev->tms)) {
        guidev->tms->vendor = "other";
        chip_type = OTHER_NFC;
    } else {
        guidev->tms->vendor = "unknown";
        TMS_ERR("No chip to match, use default %d\n", chip_type);
    }

    /* step8 : Match and probe driver */
#if LINUX_VERSION_CODE < KERNEL_VERSION(6,3,0)
    ret = guidev_probe_driver(client, id, chip_type);
#else
    ret = guidev_probe_driver(client, chip_type);
#endif

    if (ret) {
        TMS_ERR("failed to guide nfc driver\n");
        goto err_free_ldo;
    }

    nfc_ldo_unvote(&guidev->ldo);
    nfc_regulator_put(&guidev->ldo);

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

    TMS_INFO("successfully\n");
    return SUCCESS;
err_remove_sysfs_interface:
    nfc_remove_sysfs_interfaces(&client->dev);
err_free_ldo:
    nfc_ldo_unvote(&guidev->ldo);
err_put_regulator:
    nfc_regulator_put(&guidev->ldo);
err_free_guidev:
    kfree(guidev);
    guidev = NULL;
    TMS_ERR("Failed, ret = %d\n", ret);
    return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,1,0)
static int guidev_dev_remove(struct i2c_client *client)
#else
static void guidev_dev_remove(struct i2c_client *client)
#endif
{
    switch (chip_type) {
    case TMS_NFC:
        nfc_device_remove(client);
        break;

    case OTHER_NFC:
        //TMS_INFO("XXX XXX nfc start removed\n");
        //xxx_nfc_remove(client);
        break;

    default:
        TMS_ERR("No chip to remove %d\n", chip_type);
        break;
    }

    nfc_remove_sysfs_interfaces(&client->dev);
    kfree(guidev);
    guidev = NULL;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6,1,0)
    return SUCCESS;
#endif
}

void guidev_dev_shutdown(struct i2c_client *client)
{
    if (!guidev || !guidev->tms || !guidev->tms->set_gpio) {
        TMS_ERR("set_gpio is NULL");
        return;
    }

    /* WAIT_TIME_20000US is hard reset guard time */
    guidev->tms->set_gpio(guidev->hw_res.ven_gpio, OFF, WAIT_TIME_NONE, WAIT_TIME_20000US);
}

static int guide_suspend(struct device *device)
{
    switch (chip_type) {
    case TMS_NFC:
        nfc_device_suspend(device);
        break;

    case OTHER_NFC:
        //TMS_INFO("XXX XXX nfc start suspend\n");
        //xxx_nfc_suspend(device);
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
    case TMS_NFC:
        nfc_device_resume(device);
        break;

    case OTHER_NFC:
        //TMS_INFO("XXX XXX nfc start resume\n");
        //xxx_nfc_resume(device);
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
    case TMS_NFC:
#if IS_ENABLED(CONFIG_TMS_ESE_DEVICE)
        ese_driver_exit();
#endif
        break;

    case OTHER_NFC:
        //xxx_nfc_driver_exit(device);
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
