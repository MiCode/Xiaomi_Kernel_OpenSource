/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __LINUX_WUSB3801_H
#define __LINUX_WUSB3801_H

/*
 *Bit operations if we don't want to include #include <linux/bitops.h>
 */

#undef  __CONST_FFS
#define __CONST_FFS(_x) \
		((_x) & 0x0F ? ((_x) & 0x03 ? ((_x) & 0x01 ? 0 : 1) :\
										((_x) & 0x04 ? 2 : 3)) :\
						((_x) & 0x30 ? ((_x) & 0x10 ? 4 : 5) :\
										((_x) & 0x40 ? 6 : 7)))

#undef  FFS
#define FFS(_x) \
		((_x) ? __CONST_FFS(_x) : 0)

#undef  BITS
#define BITS(_end, _start) \
		((BIT(_end) - BIT(_start)) + BIT(_end))

#undef  __BITS_GET
#define __BITS_GET(_byte, _mask, _shift) \
		(((_byte) & (_mask)) >> (_shift))

#undef  BITS_GET
#define BITS_GET(_byte, _bit) \
		__BITS_GET(_byte, _bit, FFS(_bit))

/* Register Map */

#define WUSB3801_REG_VERSION_ID         0x01
#define WUSB3801_REG_CONTROL0           0x02
#define WUSB3801_REG_INTERRUPT          0x03
#define WUSB3801_REG_STATUS             0x04
#define WUSB3801_REG_CONTROL1           0x05
#define WUSB3801_REG_TEST0              0x06
#define WUSB3801_REG_TEST_01            0x07
#define WUSB3801_REG_TEST_02            0x08
#define WUSB3801_REG_TEST_03            0x09
#define WUSB3801_REG_TEST_04            0x0A
#define WUSB3801_REG_TEST_05            0x0B
#define WUSB3801_REG_TEST_06            0x0C
#define WUSB3801_REG_TEST_07            0x0D
#define WUSB3801_REG_TEST_08            0x0E
#define WUSB3801_REG_TEST_09            0x0F
#define WUSB3801_REG_TEST_0A            0x10
#define WUSB3801_REG_TEST_0B            0x11
#define WUSB3801_REG_TEST_0C            0x12
#define WUSB3801_REG_TEST_0D            0x13
#define WUSB3801_REG_TEST_0E            0x14
#define WUSB3801_REG_TEST_0F            0x15
#define WUSB3801_REG_TEST_10            0x16
#define WUSB3801_REG_TEST_11            0x17
#define WUSB3801_REG_TEST_12            0x18


#define WUSB3801_SLAVE_ADDR0            0xc0
#define WUSB3801_SLAVE_ADDR1            0xd0


/*Available modes*/
#define WUSB3801_DRP_ACC                (BIT_REG_CTRL0_RLE_DRP)
#define WUSB3801_DRP                    (BIT_REG_CTRL0_RLE_DRP | BIT_REG_CTRL0_DIS_ACC)
#define WUSB3801_SNK_ACC                (BIT_REG_CTRL0_RLE_SNK)
#define WUSB3801_SNK                    (BIT_REG_CTRL0_RLE_SNK | BIT_REG_CTRL0_DIS_ACC)
#define WUSB3801_SRC_ACC                (BIT_REG_CTRL0_RLE_SRC)
#define WUSB3801_SRC                    (BIT_REG_CTRL0_RLE_SRC | BIT_REG_CTRL0_DIS_ACC)
#define WUSB3801_DRP_PREFER_SRC_ACC     (WUSB3801_DRP_ACC | BIT_REG_CTRL0_TRY_SRC)
#define WUSB3801_DRP_PREFER_SRC         (WUSB3801_DRP     | BIT_REG_CTRL0_TRY_SRC)
#define WUSB3801_DRP_PREFER_SNK_ACC     (WUSB3801_DRP_ACC | BIT_REG_CTRL0_TRY_SNK)
#define WUSB3801_DRP_PREFER_SNK         (WUSB3801_DRP     | BIT_REG_CTRL0_TRY_SNK)


/*TODO: redefine your prefer role here*/
#define WUSB3801_INIT_MODE              (WUSB3801_DRP_PREFER_SNK_ACC)
/*Registers relevant values*/
#define WUSB3801_VENDOR_ID              0x06

/*Switch to enable/disable feature of specified Registers*/
#define BIT_REG_CTRL0_DIS_ACC           (0x01 << 7)
#define BIT_REG_CTRL0_TRY_SRC           (0x02 << 5)
#define BIT_REG_CTRL0_TRY_SNK           (0x01 << 5)
#define BIT_REG_CTRL0_CUR_DEF           (0x00 << 3)
#define BIT_REG_CTRL0_CUR_1P5           (0x01 << 3)
#define BIT_REG_CTRL0_CUR_3P0           (0x02 << 3)
#define BIT_REG_CTRL0_RLE_SNK           (0x00 << 1)
#define BIT_REG_CTRL0_RLE_SRC           (0x01 << 1)
#define BIT_REG_CTRL0_RLE_DRP           (0x02 << 1)
#define BIT_REG_CTRL0_INT_MSK           (0x01 << 0)
#define BIT_REG_STATUS_VBUS             (0x01 << 7)
#define BIT_REG_STATUS_STANDBY          (0x00 << 5)
#define BIT_REG_STATUS_CUR_DEF          (0x01 << 5)
#define BIT_REG_STATUS_CUR_MID          (0x02 << 5)
#define BIT_REG_STATUS_CUR_HIGH         (0x03 << 5)
#define BIT_REG_STATUS_ATC_STB          (0x00 << 1)
#define BIT_REG_STATUS_ATC_SNK          (0x01 << 1)
#define BIT_REG_STATUS_ATC_SRC          (0x02 << 1)
#define BIT_REG_STATUS_ATC_ACC          (0x03 << 1)
#define BIT_REG_STATUS_ATC_DACC         (0x04 << 1)
#define BIT_REG_STATUS_PLR_STB          (0x00 << 0)
#define BIT_REG_STATUS_PLR_CC1          (0x01 << 0)
#define BIT_REG_STATUS_PLR_CC2          (0x02 << 0)
#define BIT_REG_STATUS_PLR_BOTH         (0x03 << 0)
#define BIT_REG_CTRL1_SW02_DIN          (0x01 << 4)
#define BIT_REG_CTRL1_SW02_EN           (0x01 << 3)
#define BIT_REG_CTRL1_SW01_DIN          (0x01 << 2)
#define BIT_REG_CTRL1_SW01_EN           (0x01 << 1)
#define BIT_REG_CTRL1_SM_RST            (0x01 << 0)



#define BIT_REG_TEST02_FORCE_ERR_RCY    (0x01)

#define WUSB3801_WAIT_VBUS               0x40
/*Fixed duty cycle period. 40ms:40ms*/
#define WUSB3801_TGL_40MS                0
#define WUSB3801_HOST_DEFAULT            0
#define WUSB3801_HOST_1500MA             1
#define WUSB3801_HOST_3000MA             2
#define WUSB3801_INT_ENABLE              0x00
#define WUSB3801_INT_DISABLE             0x01
#define WUSB3801_DISABLED                0x0A
#define WUSB3801_ERR_REC                 0x01
#define WUSB3801_VBUS_OK                 0x80

#define WUSB3801_SNK_0MA                (0x00 << 5)
#define WUSB3801_SNK_DEFAULT            (0x01 << 5)
#define WUSB3801_SNK_1500MA             (0x02 << 5)
#define WUSB3801_SNK_3000MA             (0x03 << 5)
#define WUSB3801_ATTACH                  0x1C

//#define WUSB3801_TYPE_PWR_ACC           (0x00 << 2) /*Ra/Rd treated as Open*/
#define WUSB3801_TYPE_INVALID           (0x00)
#define WUSB3801_TYPE_SNK               (0x01 << 2)
#define WUSB3801_TYPE_SRC               (0x02 << 2)
#define WUSB3801_TYPE_AUD_ACC           (0x03 << 2)
#define WUSB3801_TYPE_DBG_ACC           (0x04 << 2)

#define WUSB3801_INT_DETACH              (0x01 << 1)
#define WUSB3801_INT_ATTACH              (0x01 << 0)

#define WUSB3801_REV20                   0x02

/* Masks for Read-Modified-Write operations*/
#define WUSB3801_HOST_CUR_MASK           0x18  /*Host current for IIC*/
#define WUSB3801_INT_MASK                0x01
#define WUSB3801_BCLVL_MASK              0x60
#define WUSB3801_TYPE_MASK               0x1C
#define WUSB3801_MODE_MASK               0xE6  /*Roles relevant bits*/
#define WUSB3801_INT_STS_MASK            0x03
#define WUSB3801_FORCE_ERR_RCY_MASK      0x80  /*Force Error recovery*/
#define WUSB3801_ROLE_MASK               0x06
#define WUSB3801_VENDOR_ID_MASK          0x07
#define WUSB3801_VERSION_ID_MASK         0xF8
#define WUSB3801_VENDOR_SUB_ID_MASK         0xA0
#define WUSB3801_POLARITY_CC_MASK        0x03
#define WUSB3801_CC_STS_MASK            0x03

#define WUSB3801_CC2_CONNECTED 1
#define WUSB3801_CC1_CONNECTED 0

#define REVERSE_CHG_SOURCE				0X01
#define REVERSE_CHG_SINK				0X02
#define REVERSE_CHG_DRP					0X03

enum typec_mode {
NOTHING_ATTACHED,
SINK_ATTACHED,
POWERED_CABLE_W_SINK,
DEBUG_ACCESSORY,
AUDIO_ADAPTER,
POWERED_CABLE_W_O_SINK,
SOURCE_ATTACHED_DEFAULT_CURRENT,
SOURCE_ATTACHED_MEDIUM_CURRENT,
SOURCE_ATTACHED_HIGH_CURRENT,
NON_COMPLIANT,
};

#endif /* #ifndef __LINUX_WUSB3801_H */
