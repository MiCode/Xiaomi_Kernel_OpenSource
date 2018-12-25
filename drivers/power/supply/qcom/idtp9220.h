/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.*
 */
#ifndef __IDTP9220_H__
#define __IDTP9220_H__

#include <linux/types.h>
#include <linux/device.h>
#include <linux/workqueue.h>

#define IDT_DRIVER_NAME      "idtp9220"
#define IDT_I2C_ADDR         0x61

#define HSCLK                60000

#define ADJUST_METE_MV       35
#define IDTP9220_DELAY       2000
#define CHARGING_FULL        100
#define CHARGING_NEED        95

/* status low regiter bits define */
#define STATUS_VOUT_ON       (1 << 7)
#define STATUS_VOUT_OFF      (1 << 6)
#define STATUS_TX_DATA_RECV  (1 << 4)
#define STATUS_OV_TEMP       (1 << 2)
#define STATUS_OV_VOL        (1 << 1)
#define STATUS_OV_CURR       (1 << 0)

#define OVER_EVENT_OCCUR     (STATUS_OV_TEMP | STATUS_OV_VOL | STATUS_OV_CURR)
#define TOGGLE_LDO_ON_OFF    (1 << 1)

/* interrupt register bits define */
#define INT_IDAUTH_SUCCESS (1 << 13)
#define INT_IDAUTH_FAIL   (1 << 12)
#define INT_SEND_SUCCESS   (1 << 11)
#define INT_SEND_TIMEOUT  (1 << 10)
#define INT_AUTH_SUCCESS   (1 << 9)
#define INT_AUTH_FAIL     (1 << 8)
#define INT_VOUT_OFF      (1 << 7)
#define INT_VOUT_ON       (1 << 6)
#define INT_MODE_CHANGE   (1 << 5)
#define INT_TX_DATA_RECV  (1 << 4)
#define INT_OV_TEMP       (1 << 2)
#define INT_OV_VOL        (1 << 1)
#define INT_OV_CURR       (1 << 0)

/* used registers define */
#define REG_CHIP_ID_L        0x0000
#define REG_CHIP_ID_H        0x0001
#define REG_CHIP_REV         0x0002
#define REG_CTM_ID           0x0003
#define REG_OTPFWVER_ADDR    0x0004
#define REG_EPRFWVER_ADDR    0x001c
#define REG_STATUS_L         0x0034
#define REG_STATUS_H         0x0035
#define REG_INTR_L           0x0036
#define REG_INTR_H           0x0037
#define REG_INTR_EN_L        0x0038
#define REG_INTR_EN_H        0x0039
#define REG_CHG_STATUS       0x003A
#define REG_ADC_VOUT_L       0x003C
#define REG_ADC_VOUT_H       0x003D
#define REG_VOUT_SET         0x003E
#define REG_VRECT_ADJ        0x003F
#define REG_ADC_VRECT        0x0040
#define REG_RX_LOUT_L        0x0044
#define REG_RX_LOUT_H        0x0045
#define REG_FREQ_ADDR        0x0048
#define REG_ILIM_SET         0x004A
#define REG_SIGNAL_STRENGTH  0x004B
#define REG_SSCMND           0x004e
#define REG_PROPPKT          0x0050
#define REG_PPPDATA          0x0051
#define REG_SSINTCLR         0x0056
#define REG_BCHEADER         0x0058
#define REG_BCDATA           0x0059
#define REG_FC_VOLTAGE_L     0x0078
#define REG_FC_VOLTAGE_H     0x0079


#define PROPRIETARY18        0x18
#define PROPRIETARY28        0x28
#define PROPRIETARY38        0x38
#define PROPRIETARY48        0x48
#define PROPRIETARY58        0x58


#define BC_NONE               0x00
#define BC_SET_FREQ           0x03
#define BC_GET_FREQ           0x04
#define BC_READ_FW_VER        0x05
#define BC_READ_Iin           0x06
#define BC_READ_Vin           0x07
#define BC_SET_Vin            0x0a

#define BC_ADAPTER_TYPE       0x0b
#define BC_RESET              0x0c
#define BC_READ_I2C           0x0d
#define BC_WRITE_I2C          0x0e
#define BC_VI2C_INIT          0x10


#define BC_READ_IOUT          0x12
#define BC_READ_VOUT          0x13
#define BC_START_CHARGE       0x30
#define BC_SET_AP_OVERLOAD    0x31
#define BC_ENABLE_FAST_CHARGE 0x32

/* Adapter_list = {0x00:'ADAPTER_UNKNOWN',  */
/*            0x01:'SDP 500mA',  */
/*            0x02:'CDP 1.1A',  */
/*            0x03:'DCP 1.5A',  */
/*            0x05:'QC2.0',  */
/*            0x06:'QC3.0',  */
/*            0x07:'PD',} */

#define ADAPTER_NONE 0x00
#define ADAPTER_SDP  0x01
#define ADAPTER_CDP  0x02
#define ADAPTER_DCP  0x03
#define ADAPTER_QC2  0x05
#define ADAPTER_QC3  0x06
#define ADAPTER_PD   0x07
#define ADAPTER_AUTH_FAILED   0x08



#define VOUTCHANGED          BIT(7)

#define TXDATARCVD           BIT(4)


#define VSWITCH              BIT(7)

#define CLRINT               BIT(5)

#define LDOTGL               BIT(1)

#define SENDPROPP            BIT(0)

#define SEND_DEVICE_AUTH     BIT(2)

enum VOUT_SET_VAL {
  VOUT_VAL_3500_MV = 0,
  VOUT_VAL_3600_MV,
  VOUT_VAL_3700_MV,
  VOUT_VAL_3800_MV,
  VOUT_VAL_3900_MV,
  VOUT_VAL_4000_MV,
  VOUT_VAL_4100_MV,
  VOUT_VAL_4200_MV,
  VOUT_VAL_4300_MV,
  VOUT_VAL_4400_MV,
  VOUT_VAL_4500_MV,
  VOUT_VAL_4600_MV,
  VOUT_VAL_4700_MV,
  VOUT_VAL_4800_MV,
  VOUT_VAL_4900_MV,
  VOUT_VAL_5000_MV,
  VOUT_VAL_5100_MV,
  VOUT_VAL_5200_MV,
};

enum IMIL_SET_VAL {
  CURR_VAL_100_MA = 0,
  CURR_VAL_200_MA,
  CURR_VAL_300_MA,
  CURR_VAL_400_MA,
  CURR_VAL_500_MA,
  CURR_VAL_600_MA,
  CURR_VAL_700_MA,
  CURR_VAL_800_MA,
  CURR_VAL_900_MA,
  CURR_VAL_1000_MA,
  CURR_VAL_1100_MA,
  CURR_VAL_1200_MA,
  CURR_VAL_1300_MA,
};

struct vol_curr_table {
  int index;
  char *val;
};
/*
struct idtp9220_platform_data {
  enum VOUT_SET_VAL vout_val_default;
  enum IMIL_SET_VAL curr_val_default;
  unsigned long gpio_en;
};
*/
typedef struct {
  u16 status;
  u16 startAddr;
  u16 codeLength;
  u16 dataChksum;
  u8  dataBuf[128];
}idtp9220_packet_t;


typedef struct {
  u8 header;
  u8 cmd;
  u8 data[4];
} ProPkt_Type;

extern uint32_t get_hw_version_major(void);

#endif
