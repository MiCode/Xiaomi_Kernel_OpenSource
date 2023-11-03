/*****************************************************************************************
 * Copyright (c)  2021- 2022  TsingTeng MicroSystem Corp.Ltd.
 * TMS_EDIT
 * File       : tms_driver_init.c
 * Description: Source file for tms driver init
 * Version    : 1.0
 * Date       : 2022/4/11
 * Team       : NFC Middleware
 * Author     : Guoliang Wu
 * --------------------------------- Revision History: ---------------------------------
 *   <version>    <date>          < author >                            <desc>
 *******************************************************************************************/
#include "tms_common.h"
#if defined(CONFIG_TMS_GUIDE_DEVICE) || defined(CONFIG_TMS_GUIDE_DEVICE_MODULE)
#include "guidev/guide_driver.h"
#else
#if defined(CONFIG_TMS_NFC_DEVICE) || defined(CONFIG_TMS_NFC_DEVICE_MODULE)
#include "nfc/nfc_driver.h"
#endif
#if defined(CONFIG_TMS_ESE_DEVICE) || defined(CONFIG_TMS_ESE_DEVICE_MODULE)
#include "ese/ese_driver.h"
#endif
#endif

/*********** PART0: Global Variables Area ***********/
#ifdef TMS_MOUDLE
#undef TMS_MOUDLE
#define TMS_MOUDLE               "Init"
#endif
/*********** PART1: Declare Area ***********/

/*********** PART2: Function Area ***********/

/*********** PART3: TMS Init Start Area ***********/
static int __init tms_driver_init(void)
{
    int ret = 0;
    TMS_INFO("is called\n");

    ret = tms_common_init();
    if (ret) {
        goto err;
    }
#if defined(CONFIG_TMS_GUIDE_DEVICE) || defined(CONFIG_TMS_GUIDE_DEVICE_MODULE)
    ret = tms_guide_init();
    if (ret) {
        goto err;
    }
#else
#if defined(CONFIG_TMS_NFC_DEVICE) || defined(CONFIG_TMS_NFC_DEVICE_MODULE)
    ret = nfc_driver_init();
    if (ret) {
        goto err;
    }
#endif
#if defined(CONFIG_TMS_ESE_DEVICE) || defined(CONFIG_TMS_ESE_DEVICE_MODULE)
    ret = ese_driver_init();
    if (ret) {
        goto err;
    }
#endif
#endif
err:
    return ret;
}

static void __exit tms_driver_exit(void)
{
    TMS_INFO("is called\n");

#if defined(CONFIG_TMS_GUIDE_DEVICE) || defined(CONFIG_TMS_GUIDE_DEVICE_MODULE)
    tms_guide_exit();
#else
#if defined(CONFIG_TMS_NFC_DEVICE) || defined(CONFIG_TMS_NFC_DEVICE_MODULE)
    nfc_driver_exit();
#endif
#if defined(CONFIG_TMS_ESE_DEVICE) || defined(CONFIG_TMS_ESE_DEVICE_MODULE)
    ese_driver_exit();
#endif
#endif
    tms_common_exit();
}

module_init(tms_driver_init);
module_exit(tms_driver_exit);

MODULE_DESCRIPTION("TMS Devices");
MODULE_LICENSE("GPL");