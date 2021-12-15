/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef TOUCHPANEL_UPGRADE_H__
#define TOUCHPANEL_UPGRADE_H__

/*#define CTP_DETECT_SUPPLIER_THROUGH_GPIO  1*/

#ifdef TPD_ALWAYS_UPGRADE_FW
#undef TPD_ALWAYS_UPGRADE_FW
#endif

/*#define GPIO_CTP_COMPAT_PIN1	GPIO28*/
/*#define GPIO_CTP_COMPAT_PIN2	GPIO29*/

#define TPD_SUPPLIER_0 0
#define TPD_SUPPLIER_1 1
#define TPD_SUPPLIER_2 2
#define TPD_SUPPLIER_3 3

#define TPD_FW_VER_0 0x00
#define TPD_FW_VER_1 0x11 /* ht tp   0   1 (choice 0) */
#define TPD_FW_VER_2 0x00
#define TPD_FW_VER_3 0x10 /* zxv tp  0   0 */

/*static unsigned char TPD_FW0[] = {
 *#include "FT5436IFT5x36i_Ref_20141230_app.i"
 *};
 *
 *static unsigned char TPD_FW1[] = {
 * // #include "ft5x0x_fw_6127_ht.i"
 *#include "FT5436IFT5x36i_Ref_20141230_app.i"
 *};
 *
 *static unsigned char TPD_FW2[] = {
 *#include "FT5436IFT5x36i_Ref_20141230_app.i"
 *};
 *
 *static unsigned char TPD_FW3[] = {
 * // #include "ft5x0x_fw_6127_zxv.i"
 *#include "FT5436IFT5x36i_Ref_20141230_app.i"
 *};
 */

static unsigned char TPD_FW[] = {
#include "HQ_AW875_FT3427_DJ_A.08.02_V1.0_V02_D01_20151204_app.i"
};
#ifdef CTP_DETECT_SUPPLIER_THROUGH_GPIO
static unsigned char TPD_FW0[] = {
#include "HQ_AW875_FT3427_DJ_A.08.02_V1.0_V02_D01_20151204_app.i"
};

static unsigned char TPD_FW1[] = {
/* #include "ft5x0x_fw_6127_ht.i" */
#include "HQ_AW875_FT3427_DJ_A.08.02_V1.0_V02_D01_20151204_app.i"
};

static unsigned char TPD_FW2[] = {
#include "HQ_AW875_FT3427_DJ_A.08.02_V1.0_V02_D01_20151204_app.i"
};

static unsigned char TPD_FW3[] = {
#include "HQ_AW875_FT3427_DJ_A.08.02_V1.0_V02_D01_20151204_app.i"
};
#endif /* CTP_DETECT_SUPPLIER_THROUGH_GPIO */
#endif /* TOUCHPANEL_UPGRADE_H__ */
