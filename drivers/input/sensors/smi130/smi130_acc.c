/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * (C) Modification Copyright 2018 Robert Bosch Kft  All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * Special: Description of the Software:
 *
 * This software module (hereinafter called "Software") and any
 * information on application-sheets (hereinafter called "Information") is
 * provided free of charge for the sole purpose to support your application
 * work. 
 *
 * As such, the Software is merely an experimental software, not tested for
 * safety in the field and only intended for inspiration for further development 
 * and testing. Any usage in a safety-relevant field of use (like automotive,
 * seafaring, spacefaring, industrial plants etc.) was not intended, so there are
 * no precautions for such usage incorporated in the Software.
 * 
 * The Software is specifically designed for the exclusive use for Bosch
 * Sensortec products by personnel who have special experience and training. Do
 * not use this Software if you do not have the proper experience or training.
 * 
 * This Software package is provided as is and without any expressed or
 * implied warranties, including without limitation, the implied warranties of
 * merchantability and fitness for a particular purpose.
 * 
 * Bosch Sensortec and their representatives and agents deny any liability for
 * the functional impairment of this Software in terms of fitness, performance
 * and safety. Bosch Sensortec and their representatives and agents shall not be
 * liable for any direct or indirect damages or injury, except as otherwise
 * stipulated in mandatory applicable law.
 * The Information provided is believed to be accurate and reliable. Bosch
 * Sensortec assumes no responsibility for the consequences of use of such
 * Information nor for any infringement of patents or other rights of third
 * parties which may result from its use.
 * 
 *------------------------------------------------------------------------------
 * The following Product Disclaimer does not apply to the BSX4-HAL-4.1NoFusion Software 
 * which is licensed under the Apache License, Version 2.0 as stated above.  
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Product Disclaimer
 *
 * Common:
 *
 * Assessment of Products Returned from Field
 *
 * Returned products are considered good if they fulfill the specifications / 
 * test data for 0-mileage and field listed in this document.
 *
 * Engineering Samples
 * 
 * Engineering samples are marked with (e) or (E). Samples may vary from the
 * valid technical specifications of the series product contained in this
 * data sheet. Therefore, they are not intended or fit for resale to
 * third parties or for use in end products. Their sole purpose is internal
 * client testing. The testing of an engineering sample may in no way replace
 * the testing of a series product. Bosch assumes no liability for the use
 * of engineering samples. The purchaser shall indemnify Bosch from all claims
 * arising from the use of engineering samples.
 *
 * Intended use
 *
 * Provided that SMI130 is used within the conditions (environment, application,
 * installation, loads) as described in this TCD and the corresponding
 * agreed upon documents, Bosch ensures that the product complies with
 * the agreed properties. Agreements beyond this require
 * the written approval by Bosch. The product is considered fit for the intended
 * use when the product successfully has passed the tests
 * in accordance with the TCD and agreed upon documents.
 *
 * It is the responsibility of the customer to ensure the proper application
 * of the product in the overall system/vehicle.
 *
 * Bosch does not assume any responsibility for changes to the environment
 * of the product that deviate from the TCD and the agreed upon documents 
 * as well as all applications not released by Bosch
  *
 * The resale and/or use of products are at the purchaser’s own risk and 
 * responsibility. The examination and testing of the SMI130 
 * is the sole responsibility of the purchaser.
 *
 * The purchaser shall indemnify Bosch from all third party claims 
 * arising from any product use not covered by the parameters of 
 * this product data sheet or not approved by Bosch and reimburse Bosch 
 * for all costs and damages in connection with such claims.
 *
 * The purchaser must monitor the market for the purchased products,
 * particularly with regard to product safety, and inform Bosch without delay
 * of all security relevant incidents.
 *
 * Application Examples and Hints
 *
 * With respect to any application examples, advice, normal values
 * and/or any information regarding the application of the device,
 * Bosch hereby disclaims any and all warranties and liabilities of any kind,
 * including without limitation warranties of
 * non-infringement of intellectual property rights or copyrights
 * of any third party.
 * The information given in this document shall in no event be regarded 
 * as a guarantee of conditions or characteristics. They are provided
 * for illustrative purposes only and no evaluation regarding infringement
 * of intellectual property rights or copyrights or regarding functionality,
 * performance or error has been made.
 *
 * @filename smi130_acc.c
 * @date    2015/11/17 10:32
 * @Modification Date 2018/08/28 18:20
 * @id       "836294d"
 * @version  2.1.2
 *
 * @brief
 * This file contains all function implementations for the SMI_ACC2X2 in linux
*/

#ifdef CONFIG_SIG_MOTION
#undef CONFIG_HAS_EARLYSUSPEND
#endif
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <linux/math64.h>
#include <linux/cpu.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/string.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#endif

#include "boschclass.h"
#include "bs_log.h"
#define DRIVER_VERSION "0.0.53.0"
#define ACC_NAME  "ACC"
#define SMI_ACC2X2_ENABLE_INT2 1
#define CONFIG_SMI_ACC_ENABLE_NEWDATA_INT 1

#define SENSOR_NAME                 "smi130_acc"
#define SMI130_ACC_USE_BASIC_I2C_FUNC        1
#define SMI130_HRTIMER 1
#define MSC_TIME                6
#define ABSMIN                      -512
#define ABSMAX                      512
#define SLOPE_THRESHOLD_VALUE       32
#define SLOPE_DURATION_VALUE        1
#define INTERRUPT_LATCH_MODE        13
#define INTERRUPT_ENABLE            1
#define INTERRUPT_DISABLE           0
#define MAP_SLOPE_INTERRUPT         2
#define SLOPE_X_INDEX               5
#define SLOPE_Y_INDEX               6
#define SLOPE_Z_INDEX               7
#define SMI_ACC2X2_MAX_DELAY            200
#define SMI_ACC2X2_RANGE_SET            3  /* +/- 2G */
#define SMI_ACC2X2_BW_SET               12 /* 125HZ  */

#define LOW_G_INTERRUPT             REL_Z
#define HIGH_G_INTERRUPT            REL_HWHEEL
#define SLOP_INTERRUPT              REL_DIAL
#define DOUBLE_TAP_INTERRUPT        REL_WHEEL
#define SINGLE_TAP_INTERRUPT        REL_MISC
#define ORIENT_INTERRUPT            ABS_PRESSURE
#define FLAT_INTERRUPT              ABS_DISTANCE
#define SLOW_NO_MOTION_INTERRUPT    REL_Y

#define HIGH_G_INTERRUPT_X_HAPPENED                 1
#define HIGH_G_INTERRUPT_Y_HAPPENED                 2
#define HIGH_G_INTERRUPT_Z_HAPPENED                 3
#define HIGH_G_INTERRUPT_X_NEGATIVE_HAPPENED        4
#define HIGH_G_INTERRUPT_Y_NEGATIVE_HAPPENED        5
#define HIGH_G_INTERRUPT_Z_NEGATIVE_HAPPENED        6
#define SLOPE_INTERRUPT_X_HAPPENED                  7
#define SLOPE_INTERRUPT_Y_HAPPENED                  8
#define SLOPE_INTERRUPT_Z_HAPPENED                  9
#define SLOPE_INTERRUPT_X_NEGATIVE_HAPPENED         10
#define SLOPE_INTERRUPT_Y_NEGATIVE_HAPPENED         11
#define SLOPE_INTERRUPT_Z_NEGATIVE_HAPPENED         12
#define DOUBLE_TAP_INTERRUPT_HAPPENED               13
#define SINGLE_TAP_INTERRUPT_HAPPENED               14
#define UPWARD_PORTRAIT_UP_INTERRUPT_HAPPENED       15
#define UPWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED     16
#define UPWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED    17
#define UPWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED   18
#define DOWNWARD_PORTRAIT_UP_INTERRUPT_HAPPENED     19
#define DOWNWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED   20
#define DOWNWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED  21
#define DOWNWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED 22
#define FLAT_INTERRUPT_TURE_HAPPENED                23
#define FLAT_INTERRUPT_FALSE_HAPPENED               24
#define LOW_G_INTERRUPT_HAPPENED                    25
#define SLOW_NO_MOTION_INTERRUPT_HAPPENED           26

#define PAD_LOWG                    0
#define PAD_HIGHG                   1
#define PAD_SLOP                    2
#define PAD_DOUBLE_TAP              3
#define PAD_SINGLE_TAP              4
#define PAD_ORIENT                  5
#define PAD_FLAT                    6
#define PAD_SLOW_NO_MOTION          7

#define SMI_ACC2X2_EEP_OFFSET                       0x16
#define SMI_ACC2X2_IMAGE_BASE                       0x38
#define SMI_ACC2X2_IMAGE_LEN                        22

#define SMI_ACC2X2_CHIP_ID_REG                      0x00
#define SMI_ACC2X2_VERSION_REG                      0x01
#define SMI_ACC2X2_X_AXIS_LSB_REG                   0x02
#define SMI_ACC2X2_X_AXIS_MSB_REG                   0x03
#define SMI_ACC2X2_Y_AXIS_LSB_REG                   0x04
#define SMI_ACC2X2_Y_AXIS_MSB_REG                   0x05
#define SMI_ACC2X2_Z_AXIS_LSB_REG                   0x06
#define SMI_ACC2X2_Z_AXIS_MSB_REG                   0x07
#define SMI_ACC2X2_TEMPERATURE_REG                  0x08
#define SMI_ACC2X2_STATUS1_REG                      0x09
#define SMI_ACC2X2_STATUS2_REG                      0x0A
#define SMI_ACC2X2_STATUS_TAP_SLOPE_REG             0x0B
#define SMI_ACC2X2_STATUS_ORIENT_HIGH_REG           0x0C
#define SMI_ACC2X2_STATUS_FIFO_REG                  0x0E
#define SMI_ACC2X2_RANGE_SEL_REG                    0x0F
#define SMI_ACC2X2_BW_SEL_REG                       0x10
#define SMI_ACC2X2_MODE_CTRL_REG                    0x11
#define SMI_ACC2X2_LOW_NOISE_CTRL_REG               0x12
#define SMI_ACC2X2_DATA_CTRL_REG                    0x13
#define SMI_ACC2X2_RESET_REG                        0x14
#define SMI_ACC2X2_INT_ENABLE1_REG                  0x16
#define SMI_ACC2X2_INT_ENABLE2_REG                  0x17
#define SMI_ACC2X2_INT_SLO_NO_MOT_REG               0x18
#define SMI_ACC2X2_INT1_PAD_SEL_REG                 0x19
#define SMI_ACC2X2_INT_DATA_SEL_REG                 0x1A
#define SMI_ACC2X2_INT2_PAD_SEL_REG                 0x1B
#define SMI_ACC2X2_INT_SRC_REG                      0x1E
#define SMI_ACC2X2_INT_SET_REG                      0x20
#define SMI_ACC2X2_INT_CTRL_REG                     0x21
#define SMI_ACC2X2_LOW_DURN_REG                     0x22
#define SMI_ACC2X2_LOW_THRES_REG                    0x23
#define SMI_ACC2X2_LOW_HIGH_HYST_REG                0x24
#define SMI_ACC2X2_HIGH_DURN_REG                    0x25
#define SMI_ACC2X2_HIGH_THRES_REG                   0x26
#define SMI_ACC2X2_SLOPE_DURN_REG                   0x27
#define SMI_ACC2X2_SLOPE_THRES_REG                  0x28
#define SMI_ACC2X2_SLO_NO_MOT_THRES_REG             0x29
#define SMI_ACC2X2_TAP_PARAM_REG                    0x2A
#define SMI_ACC2X2_TAP_THRES_REG                    0x2B
#define SMI_ACC2X2_ORIENT_PARAM_REG                 0x2C
#define SMI_ACC2X2_THETA_BLOCK_REG                  0x2D
#define SMI_ACC2X2_THETA_FLAT_REG                   0x2E
#define SMI_ACC2X2_FLAT_HOLD_TIME_REG               0x2F
#define SMI_ACC2X2_FIFO_WML_TRIG                    0x30
#define SMI_ACC2X2_SELF_TEST_REG                    0x32
#define SMI_ACC2X2_EEPROM_CTRL_REG                  0x33
#define SMI_ACC2X2_SERIAL_CTRL_REG                  0x34
#define SMI_ACC2X2_EXTMODE_CTRL_REG                 0x35
#define SMI_ACC2X2_OFFSET_CTRL_REG                  0x36
#define SMI_ACC2X2_OFFSET_PARAMS_REG                0x37
#define SMI_ACC2X2_OFFSET_X_AXIS_REG                0x38
#define SMI_ACC2X2_OFFSET_Y_AXIS_REG                0x39
#define SMI_ACC2X2_OFFSET_Z_AXIS_REG                0x3A
#define SMI_ACC2X2_GP0_REG                          0x3B
#define SMI_ACC2X2_GP1_REG                          0x3C
#define SMI_ACC2X2_FIFO_MODE_REG                    0x3E
#define SMI_ACC2X2_FIFO_DATA_OUTPUT_REG             0x3F

#define SMI_ACC2X2_CHIP_ID__POS             0
#define SMI_ACC2X2_CHIP_ID__MSK             0xFF
#define SMI_ACC2X2_CHIP_ID__LEN             8
#define SMI_ACC2X2_CHIP_ID__REG             SMI_ACC2X2_CHIP_ID_REG

#define SMI_ACC2X2_VERSION__POS          0
#define SMI_ACC2X2_VERSION__LEN          8
#define SMI_ACC2X2_VERSION__MSK          0xFF
#define SMI_ACC2X2_VERSION__REG          SMI_ACC2X2_VERSION_REG

#define SMI130_ACC_SLO_NO_MOT_DUR__POS   2
#define SMI130_ACC_SLO_NO_MOT_DUR__LEN   6
#define SMI130_ACC_SLO_NO_MOT_DUR__MSK   0xFC
#define SMI130_ACC_SLO_NO_MOT_DUR__REG   SMI_ACC2X2_SLOPE_DURN_REG

#define SMI_ACC2X2_NEW_DATA_X__POS          0
#define SMI_ACC2X2_NEW_DATA_X__LEN          1
#define SMI_ACC2X2_NEW_DATA_X__MSK          0x01
#define SMI_ACC2X2_NEW_DATA_X__REG          SMI_ACC2X2_X_AXIS_LSB_REG

#define SMI_ACC2X2_ACC_X14_LSB__POS           2
#define SMI_ACC2X2_ACC_X14_LSB__LEN           6
#define SMI_ACC2X2_ACC_X14_LSB__MSK           0xFC
#define SMI_ACC2X2_ACC_X14_LSB__REG           SMI_ACC2X2_X_AXIS_LSB_REG

#define SMI_ACC2X2_ACC_X12_LSB__POS           4
#define SMI_ACC2X2_ACC_X12_LSB__LEN           4
#define SMI_ACC2X2_ACC_X12_LSB__MSK           0xF0
#define SMI_ACC2X2_ACC_X12_LSB__REG           SMI_ACC2X2_X_AXIS_LSB_REG

#define SMI_ACC2X2_ACC_X10_LSB__POS           6
#define SMI_ACC2X2_ACC_X10_LSB__LEN           2
#define SMI_ACC2X2_ACC_X10_LSB__MSK           0xC0
#define SMI_ACC2X2_ACC_X10_LSB__REG           SMI_ACC2X2_X_AXIS_LSB_REG

#define SMI_ACC2X2_ACC_X8_LSB__POS           0
#define SMI_ACC2X2_ACC_X8_LSB__LEN           0
#define SMI_ACC2X2_ACC_X8_LSB__MSK           0x00
#define SMI_ACC2X2_ACC_X8_LSB__REG           SMI_ACC2X2_X_AXIS_LSB_REG

#define SMI_ACC2X2_ACC_X_MSB__POS           0
#define SMI_ACC2X2_ACC_X_MSB__LEN           8
#define SMI_ACC2X2_ACC_X_MSB__MSK           0xFF
#define SMI_ACC2X2_ACC_X_MSB__REG           SMI_ACC2X2_X_AXIS_MSB_REG

#define SMI_ACC2X2_NEW_DATA_Y__POS          0
#define SMI_ACC2X2_NEW_DATA_Y__LEN          1
#define SMI_ACC2X2_NEW_DATA_Y__MSK          0x01
#define SMI_ACC2X2_NEW_DATA_Y__REG          SMI_ACC2X2_Y_AXIS_LSB_REG

#define SMI_ACC2X2_ACC_Y14_LSB__POS           2
#define SMI_ACC2X2_ACC_Y14_LSB__LEN           6
#define SMI_ACC2X2_ACC_Y14_LSB__MSK           0xFC
#define SMI_ACC2X2_ACC_Y14_LSB__REG           SMI_ACC2X2_Y_AXIS_LSB_REG

#define SMI_ACC2X2_ACC_Y12_LSB__POS           4
#define SMI_ACC2X2_ACC_Y12_LSB__LEN           4
#define SMI_ACC2X2_ACC_Y12_LSB__MSK           0xF0
#define SMI_ACC2X2_ACC_Y12_LSB__REG           SMI_ACC2X2_Y_AXIS_LSB_REG

#define SMI_ACC2X2_ACC_Y10_LSB__POS           6
#define SMI_ACC2X2_ACC_Y10_LSB__LEN           2
#define SMI_ACC2X2_ACC_Y10_LSB__MSK           0xC0
#define SMI_ACC2X2_ACC_Y10_LSB__REG           SMI_ACC2X2_Y_AXIS_LSB_REG

#define SMI_ACC2X2_ACC_Y8_LSB__POS           0
#define SMI_ACC2X2_ACC_Y8_LSB__LEN           0
#define SMI_ACC2X2_ACC_Y8_LSB__MSK           0x00
#define SMI_ACC2X2_ACC_Y8_LSB__REG           SMI_ACC2X2_Y_AXIS_LSB_REG

#define SMI_ACC2X2_ACC_Y_MSB__POS           0
#define SMI_ACC2X2_ACC_Y_MSB__LEN           8
#define SMI_ACC2X2_ACC_Y_MSB__MSK           0xFF
#define SMI_ACC2X2_ACC_Y_MSB__REG           SMI_ACC2X2_Y_AXIS_MSB_REG

#define SMI_ACC2X2_NEW_DATA_Z__POS          0
#define SMI_ACC2X2_NEW_DATA_Z__LEN          1
#define SMI_ACC2X2_NEW_DATA_Z__MSK          0x01
#define SMI_ACC2X2_NEW_DATA_Z__REG          SMI_ACC2X2_Z_AXIS_LSB_REG

#define SMI_ACC2X2_ACC_Z14_LSB__POS           2
#define SMI_ACC2X2_ACC_Z14_LSB__LEN           6
#define SMI_ACC2X2_ACC_Z14_LSB__MSK           0xFC
#define SMI_ACC2X2_ACC_Z14_LSB__REG           SMI_ACC2X2_Z_AXIS_LSB_REG

#define SMI_ACC2X2_ACC_Z12_LSB__POS           4
#define SMI_ACC2X2_ACC_Z12_LSB__LEN           4
#define SMI_ACC2X2_ACC_Z12_LSB__MSK           0xF0
#define SMI_ACC2X2_ACC_Z12_LSB__REG           SMI_ACC2X2_Z_AXIS_LSB_REG

#define SMI_ACC2X2_ACC_Z10_LSB__POS           6
#define SMI_ACC2X2_ACC_Z10_LSB__LEN           2
#define SMI_ACC2X2_ACC_Z10_LSB__MSK           0xC0
#define SMI_ACC2X2_ACC_Z10_LSB__REG           SMI_ACC2X2_Z_AXIS_LSB_REG

#define SMI_ACC2X2_ACC_Z8_LSB__POS           0
#define SMI_ACC2X2_ACC_Z8_LSB__LEN           0
#define SMI_ACC2X2_ACC_Z8_LSB__MSK           0x00
#define SMI_ACC2X2_ACC_Z8_LSB__REG           SMI_ACC2X2_Z_AXIS_LSB_REG

#define SMI_ACC2X2_ACC_Z_MSB__POS           0
#define SMI_ACC2X2_ACC_Z_MSB__LEN           8
#define SMI_ACC2X2_ACC_Z_MSB__MSK           0xFF
#define SMI_ACC2X2_ACC_Z_MSB__REG           SMI_ACC2X2_Z_AXIS_MSB_REG

#define SMI_ACC2X2_TEMPERATURE__POS         0
#define SMI_ACC2X2_TEMPERATURE__LEN         8
#define SMI_ACC2X2_TEMPERATURE__MSK         0xFF
#define SMI_ACC2X2_TEMPERATURE__REG         SMI_ACC2X2_TEMP_RD_REG

#define SMI_ACC2X2_LOWG_INT_S__POS          0
#define SMI_ACC2X2_LOWG_INT_S__LEN          1
#define SMI_ACC2X2_LOWG_INT_S__MSK          0x01
#define SMI_ACC2X2_LOWG_INT_S__REG          SMI_ACC2X2_STATUS1_REG

#define SMI_ACC2X2_HIGHG_INT_S__POS          1
#define SMI_ACC2X2_HIGHG_INT_S__LEN          1
#define SMI_ACC2X2_HIGHG_INT_S__MSK          0x02
#define SMI_ACC2X2_HIGHG_INT_S__REG          SMI_ACC2X2_STATUS1_REG

#define SMI_ACC2X2_SLOPE_INT_S__POS          2
#define SMI_ACC2X2_SLOPE_INT_S__LEN          1
#define SMI_ACC2X2_SLOPE_INT_S__MSK          0x04
#define SMI_ACC2X2_SLOPE_INT_S__REG          SMI_ACC2X2_STATUS1_REG


#define SMI_ACC2X2_SLO_NO_MOT_INT_S__POS          3
#define SMI_ACC2X2_SLO_NO_MOT_INT_S__LEN          1
#define SMI_ACC2X2_SLO_NO_MOT_INT_S__MSK          0x08
#define SMI_ACC2X2_SLO_NO_MOT_INT_S__REG          SMI_ACC2X2_STATUS1_REG

#define SMI_ACC2X2_DOUBLE_TAP_INT_S__POS     4
#define SMI_ACC2X2_DOUBLE_TAP_INT_S__LEN     1
#define SMI_ACC2X2_DOUBLE_TAP_INT_S__MSK     0x10
#define SMI_ACC2X2_DOUBLE_TAP_INT_S__REG     SMI_ACC2X2_STATUS1_REG

#define SMI_ACC2X2_SINGLE_TAP_INT_S__POS     5
#define SMI_ACC2X2_SINGLE_TAP_INT_S__LEN     1
#define SMI_ACC2X2_SINGLE_TAP_INT_S__MSK     0x20
#define SMI_ACC2X2_SINGLE_TAP_INT_S__REG     SMI_ACC2X2_STATUS1_REG

#define SMI_ACC2X2_ORIENT_INT_S__POS         6
#define SMI_ACC2X2_ORIENT_INT_S__LEN         1
#define SMI_ACC2X2_ORIENT_INT_S__MSK         0x40
#define SMI_ACC2X2_ORIENT_INT_S__REG         SMI_ACC2X2_STATUS1_REG

#define SMI_ACC2X2_FLAT_INT_S__POS           7
#define SMI_ACC2X2_FLAT_INT_S__LEN           1
#define SMI_ACC2X2_FLAT_INT_S__MSK           0x80
#define SMI_ACC2X2_FLAT_INT_S__REG           SMI_ACC2X2_STATUS1_REG

#define SMI_ACC2X2_FIFO_FULL_INT_S__POS           5
#define SMI_ACC2X2_FIFO_FULL_INT_S__LEN           1
#define SMI_ACC2X2_FIFO_FULL_INT_S__MSK           0x20
#define SMI_ACC2X2_FIFO_FULL_INT_S__REG           SMI_ACC2X2_STATUS2_REG

#define SMI_ACC2X2_FIFO_WM_INT_S__POS           6
#define SMI_ACC2X2_FIFO_WM_INT_S__LEN           1
#define SMI_ACC2X2_FIFO_WM_INT_S__MSK           0x40
#define SMI_ACC2X2_FIFO_WM_INT_S__REG           SMI_ACC2X2_STATUS2_REG

#define SMI_ACC2X2_DATA_INT_S__POS           7
#define SMI_ACC2X2_DATA_INT_S__LEN           1
#define SMI_ACC2X2_DATA_INT_S__MSK           0x80
#define SMI_ACC2X2_DATA_INT_S__REG           SMI_ACC2X2_STATUS2_REG

#define SMI_ACC2X2_SLOPE_FIRST_X__POS        0
#define SMI_ACC2X2_SLOPE_FIRST_X__LEN        1
#define SMI_ACC2X2_SLOPE_FIRST_X__MSK        0x01
#define SMI_ACC2X2_SLOPE_FIRST_X__REG        SMI_ACC2X2_STATUS_TAP_SLOPE_REG

#define SMI_ACC2X2_SLOPE_FIRST_Y__POS        1
#define SMI_ACC2X2_SLOPE_FIRST_Y__LEN        1
#define SMI_ACC2X2_SLOPE_FIRST_Y__MSK        0x02
#define SMI_ACC2X2_SLOPE_FIRST_Y__REG        SMI_ACC2X2_STATUS_TAP_SLOPE_REG

#define SMI_ACC2X2_SLOPE_FIRST_Z__POS        2
#define SMI_ACC2X2_SLOPE_FIRST_Z__LEN        1
#define SMI_ACC2X2_SLOPE_FIRST_Z__MSK        0x04
#define SMI_ACC2X2_SLOPE_FIRST_Z__REG        SMI_ACC2X2_STATUS_TAP_SLOPE_REG

#define SMI_ACC2X2_SLOPE_SIGN_S__POS         3
#define SMI_ACC2X2_SLOPE_SIGN_S__LEN         1
#define SMI_ACC2X2_SLOPE_SIGN_S__MSK         0x08
#define SMI_ACC2X2_SLOPE_SIGN_S__REG         SMI_ACC2X2_STATUS_TAP_SLOPE_REG

#define SMI_ACC2X2_TAP_FIRST_X__POS        4
#define SMI_ACC2X2_TAP_FIRST_X__LEN        1
#define SMI_ACC2X2_TAP_FIRST_X__MSK        0x10
#define SMI_ACC2X2_TAP_FIRST_X__REG        SMI_ACC2X2_STATUS_TAP_SLOPE_REG

#define SMI_ACC2X2_TAP_FIRST_Y__POS        5
#define SMI_ACC2X2_TAP_FIRST_Y__LEN        1
#define SMI_ACC2X2_TAP_FIRST_Y__MSK        0x20
#define SMI_ACC2X2_TAP_FIRST_Y__REG        SMI_ACC2X2_STATUS_TAP_SLOPE_REG

#define SMI_ACC2X2_TAP_FIRST_Z__POS        6
#define SMI_ACC2X2_TAP_FIRST_Z__LEN        1
#define SMI_ACC2X2_TAP_FIRST_Z__MSK        0x40
#define SMI_ACC2X2_TAP_FIRST_Z__REG        SMI_ACC2X2_STATUS_TAP_SLOPE_REG

#define SMI_ACC2X2_TAP_SIGN_S__POS         7
#define SMI_ACC2X2_TAP_SIGN_S__LEN         1
#define SMI_ACC2X2_TAP_SIGN_S__MSK         0x80
#define SMI_ACC2X2_TAP_SIGN_S__REG         SMI_ACC2X2_STATUS_TAP_SLOPE_REG

#define SMI_ACC2X2_HIGHG_FIRST_X__POS        0
#define SMI_ACC2X2_HIGHG_FIRST_X__LEN        1
#define SMI_ACC2X2_HIGHG_FIRST_X__MSK        0x01
#define SMI_ACC2X2_HIGHG_FIRST_X__REG        SMI_ACC2X2_STATUS_ORIENT_HIGH_REG

#define SMI_ACC2X2_HIGHG_FIRST_Y__POS        1
#define SMI_ACC2X2_HIGHG_FIRST_Y__LEN        1
#define SMI_ACC2X2_HIGHG_FIRST_Y__MSK        0x02
#define SMI_ACC2X2_HIGHG_FIRST_Y__REG        SMI_ACC2X2_STATUS_ORIENT_HIGH_REG

#define SMI_ACC2X2_HIGHG_FIRST_Z__POS        2
#define SMI_ACC2X2_HIGHG_FIRST_Z__LEN        1
#define SMI_ACC2X2_HIGHG_FIRST_Z__MSK        0x04
#define SMI_ACC2X2_HIGHG_FIRST_Z__REG        SMI_ACC2X2_STATUS_ORIENT_HIGH_REG

#define SMI_ACC2X2_HIGHG_SIGN_S__POS         3
#define SMI_ACC2X2_HIGHG_SIGN_S__LEN         1
#define SMI_ACC2X2_HIGHG_SIGN_S__MSK         0x08
#define SMI_ACC2X2_HIGHG_SIGN_S__REG         SMI_ACC2X2_STATUS_ORIENT_HIGH_REG

#define SMI_ACC2X2_ORIENT_S__POS             4
#define SMI_ACC2X2_ORIENT_S__LEN             3
#define SMI_ACC2X2_ORIENT_S__MSK             0x70
#define SMI_ACC2X2_ORIENT_S__REG             SMI_ACC2X2_STATUS_ORIENT_HIGH_REG

#define SMI_ACC2X2_FLAT_S__POS               7
#define SMI_ACC2X2_FLAT_S__LEN               1
#define SMI_ACC2X2_FLAT_S__MSK               0x80
#define SMI_ACC2X2_FLAT_S__REG               SMI_ACC2X2_STATUS_ORIENT_HIGH_REG

#define SMI_ACC2X2_FIFO_FRAME_COUNTER_S__POS             0
#define SMI_ACC2X2_FIFO_FRAME_COUNTER_S__LEN             7
#define SMI_ACC2X2_FIFO_FRAME_COUNTER_S__MSK             0x7F
#define SMI_ACC2X2_FIFO_FRAME_COUNTER_S__REG             SMI_ACC2X2_STATUS_FIFO_REG

#define SMI_ACC2X2_FIFO_OVERRUN_S__POS             7
#define SMI_ACC2X2_FIFO_OVERRUN_S__LEN             1
#define SMI_ACC2X2_FIFO_OVERRUN_S__MSK             0x80
#define SMI_ACC2X2_FIFO_OVERRUN_S__REG             SMI_ACC2X2_STATUS_FIFO_REG

#define SMI_ACC2X2_RANGE_SEL__POS             0
#define SMI_ACC2X2_RANGE_SEL__LEN             4
#define SMI_ACC2X2_RANGE_SEL__MSK             0x0F
#define SMI_ACC2X2_RANGE_SEL__REG             SMI_ACC2X2_RANGE_SEL_REG

#define SMI_ACC2X2_BANDWIDTH__POS             0
#define SMI_ACC2X2_BANDWIDTH__LEN             5
#define SMI_ACC2X2_BANDWIDTH__MSK             0x1F
#define SMI_ACC2X2_BANDWIDTH__REG             SMI_ACC2X2_BW_SEL_REG

#define SMI_ACC2X2_SLEEP_DUR__POS             1
#define SMI_ACC2X2_SLEEP_DUR__LEN             4
#define SMI_ACC2X2_SLEEP_DUR__MSK             0x1E
#define SMI_ACC2X2_SLEEP_DUR__REG             SMI_ACC2X2_MODE_CTRL_REG

#define SMI_ACC2X2_MODE_CTRL__POS             5
#define SMI_ACC2X2_MODE_CTRL__LEN             3
#define SMI_ACC2X2_MODE_CTRL__MSK             0xE0
#define SMI_ACC2X2_MODE_CTRL__REG             SMI_ACC2X2_MODE_CTRL_REG

#define SMI_ACC2X2_DEEP_SUSPEND__POS          5
#define SMI_ACC2X2_DEEP_SUSPEND__LEN          1
#define SMI_ACC2X2_DEEP_SUSPEND__MSK          0x20
#define SMI_ACC2X2_DEEP_SUSPEND__REG          SMI_ACC2X2_MODE_CTRL_REG

#define SMI_ACC2X2_EN_LOW_POWER__POS          6
#define SMI_ACC2X2_EN_LOW_POWER__LEN          1
#define SMI_ACC2X2_EN_LOW_POWER__MSK          0x40
#define SMI_ACC2X2_EN_LOW_POWER__REG          SMI_ACC2X2_MODE_CTRL_REG

#define SMI_ACC2X2_EN_SUSPEND__POS            7
#define SMI_ACC2X2_EN_SUSPEND__LEN            1
#define SMI_ACC2X2_EN_SUSPEND__MSK            0x80
#define SMI_ACC2X2_EN_SUSPEND__REG            SMI_ACC2X2_MODE_CTRL_REG

#define SMI_ACC2X2_SLEEP_TIMER__POS          5
#define SMI_ACC2X2_SLEEP_TIMER__LEN          1
#define SMI_ACC2X2_SLEEP_TIMER__MSK          0x20
#define SMI_ACC2X2_SLEEP_TIMER__REG          SMI_ACC2X2_LOW_NOISE_CTRL_REG

#define SMI_ACC2X2_LOW_POWER_MODE__POS          6
#define SMI_ACC2X2_LOW_POWER_MODE__LEN          1
#define SMI_ACC2X2_LOW_POWER_MODE__MSK          0x40
#define SMI_ACC2X2_LOW_POWER_MODE__REG          SMI_ACC2X2_LOW_NOISE_CTRL_REG

#define SMI_ACC2X2_EN_LOW_NOISE__POS          7
#define SMI_ACC2X2_EN_LOW_NOISE__LEN          1
#define SMI_ACC2X2_EN_LOW_NOISE__MSK          0x80
#define SMI_ACC2X2_EN_LOW_NOISE__REG          SMI_ACC2X2_LOW_NOISE_CTRL_REG

#define SMI_ACC2X2_DIS_SHADOW_PROC__POS       6
#define SMI_ACC2X2_DIS_SHADOW_PROC__LEN       1
#define SMI_ACC2X2_DIS_SHADOW_PROC__MSK       0x40
#define SMI_ACC2X2_DIS_SHADOW_PROC__REG       SMI_ACC2X2_DATA_CTRL_REG

#define SMI_ACC2X2_EN_DATA_HIGH_BW__POS         7
#define SMI_ACC2X2_EN_DATA_HIGH_BW__LEN         1
#define SMI_ACC2X2_EN_DATA_HIGH_BW__MSK         0x80
#define SMI_ACC2X2_EN_DATA_HIGH_BW__REG         SMI_ACC2X2_DATA_CTRL_REG

#define SMI_ACC2X2_EN_SOFT_RESET__POS         0
#define SMI_ACC2X2_EN_SOFT_RESET__LEN         8
#define SMI_ACC2X2_EN_SOFT_RESET__MSK         0xFF
#define SMI_ACC2X2_EN_SOFT_RESET__REG         SMI_ACC2X2_RESET_REG

#define SMI_ACC2X2_EN_SOFT_RESET_VALUE        0xB6

#define SMI_ACC2X2_EN_SLOPE_X_INT__POS         0
#define SMI_ACC2X2_EN_SLOPE_X_INT__LEN         1
#define SMI_ACC2X2_EN_SLOPE_X_INT__MSK         0x01
#define SMI_ACC2X2_EN_SLOPE_X_INT__REG         SMI_ACC2X2_INT_ENABLE1_REG

#define SMI_ACC2X2_EN_SLOPE_Y_INT__POS         1
#define SMI_ACC2X2_EN_SLOPE_Y_INT__LEN         1
#define SMI_ACC2X2_EN_SLOPE_Y_INT__MSK         0x02
#define SMI_ACC2X2_EN_SLOPE_Y_INT__REG         SMI_ACC2X2_INT_ENABLE1_REG

#define SMI_ACC2X2_EN_SLOPE_Z_INT__POS         2
#define SMI_ACC2X2_EN_SLOPE_Z_INT__LEN         1
#define SMI_ACC2X2_EN_SLOPE_Z_INT__MSK         0x04
#define SMI_ACC2X2_EN_SLOPE_Z_INT__REG         SMI_ACC2X2_INT_ENABLE1_REG

#define SMI_ACC2X2_EN_DOUBLE_TAP_INT__POS      4
#define SMI_ACC2X2_EN_DOUBLE_TAP_INT__LEN      1
#define SMI_ACC2X2_EN_DOUBLE_TAP_INT__MSK      0x10
#define SMI_ACC2X2_EN_DOUBLE_TAP_INT__REG      SMI_ACC2X2_INT_ENABLE1_REG

#define SMI_ACC2X2_EN_SINGLE_TAP_INT__POS      5
#define SMI_ACC2X2_EN_SINGLE_TAP_INT__LEN      1
#define SMI_ACC2X2_EN_SINGLE_TAP_INT__MSK      0x20
#define SMI_ACC2X2_EN_SINGLE_TAP_INT__REG      SMI_ACC2X2_INT_ENABLE1_REG

#define SMI_ACC2X2_EN_ORIENT_INT__POS          6
#define SMI_ACC2X2_EN_ORIENT_INT__LEN          1
#define SMI_ACC2X2_EN_ORIENT_INT__MSK          0x40
#define SMI_ACC2X2_EN_ORIENT_INT__REG          SMI_ACC2X2_INT_ENABLE1_REG

#define SMI_ACC2X2_EN_FLAT_INT__POS            7
#define SMI_ACC2X2_EN_FLAT_INT__LEN            1
#define SMI_ACC2X2_EN_FLAT_INT__MSK            0x80
#define SMI_ACC2X2_EN_FLAT_INT__REG            SMI_ACC2X2_INT_ENABLE1_REG

#define SMI_ACC2X2_EN_HIGHG_X_INT__POS         0
#define SMI_ACC2X2_EN_HIGHG_X_INT__LEN         1
#define SMI_ACC2X2_EN_HIGHG_X_INT__MSK         0x01
#define SMI_ACC2X2_EN_HIGHG_X_INT__REG         SMI_ACC2X2_INT_ENABLE2_REG

#define SMI_ACC2X2_EN_HIGHG_Y_INT__POS         1
#define SMI_ACC2X2_EN_HIGHG_Y_INT__LEN         1
#define SMI_ACC2X2_EN_HIGHG_Y_INT__MSK         0x02
#define SMI_ACC2X2_EN_HIGHG_Y_INT__REG         SMI_ACC2X2_INT_ENABLE2_REG

#define SMI_ACC2X2_EN_HIGHG_Z_INT__POS         2
#define SMI_ACC2X2_EN_HIGHG_Z_INT__LEN         1
#define SMI_ACC2X2_EN_HIGHG_Z_INT__MSK         0x04
#define SMI_ACC2X2_EN_HIGHG_Z_INT__REG         SMI_ACC2X2_INT_ENABLE2_REG

#define SMI_ACC2X2_EN_LOWG_INT__POS            3
#define SMI_ACC2X2_EN_LOWG_INT__LEN            1
#define SMI_ACC2X2_EN_LOWG_INT__MSK            0x08
#define SMI_ACC2X2_EN_LOWG_INT__REG            SMI_ACC2X2_INT_ENABLE2_REG

#define SMI_ACC2X2_EN_NEW_DATA_INT__POS        4
#define SMI_ACC2X2_EN_NEW_DATA_INT__LEN        1
#define SMI_ACC2X2_EN_NEW_DATA_INT__MSK        0x10
#define SMI_ACC2X2_EN_NEW_DATA_INT__REG        SMI_ACC2X2_INT_ENABLE2_REG

#define SMI_ACC2X2_INT_FFULL_EN_INT__POS        5
#define SMI_ACC2X2_INT_FFULL_EN_INT__LEN        1
#define SMI_ACC2X2_INT_FFULL_EN_INT__MSK        0x20
#define SMI_ACC2X2_INT_FFULL_EN_INT__REG        SMI_ACC2X2_INT_ENABLE2_REG

#define SMI_ACC2X2_INT_FWM_EN_INT__POS        6
#define SMI_ACC2X2_INT_FWM_EN_INT__LEN        1
#define SMI_ACC2X2_INT_FWM_EN_INT__MSK        0x40
#define SMI_ACC2X2_INT_FWM_EN_INT__REG        SMI_ACC2X2_INT_ENABLE2_REG

#define SMI_ACC2X2_INT_SLO_NO_MOT_EN_X_INT__POS        0
#define SMI_ACC2X2_INT_SLO_NO_MOT_EN_X_INT__LEN        1
#define SMI_ACC2X2_INT_SLO_NO_MOT_EN_X_INT__MSK        0x01
#define SMI_ACC2X2_INT_SLO_NO_MOT_EN_X_INT__REG        SMI_ACC2X2_INT_SLO_NO_MOT_REG

#define SMI_ACC2X2_INT_SLO_NO_MOT_EN_Y_INT__POS        1
#define SMI_ACC2X2_INT_SLO_NO_MOT_EN_Y_INT__LEN        1
#define SMI_ACC2X2_INT_SLO_NO_MOT_EN_Y_INT__MSK        0x02
#define SMI_ACC2X2_INT_SLO_NO_MOT_EN_Y_INT__REG        SMI_ACC2X2_INT_SLO_NO_MOT_REG

#define SMI_ACC2X2_INT_SLO_NO_MOT_EN_Z_INT__POS        2
#define SMI_ACC2X2_INT_SLO_NO_MOT_EN_Z_INT__LEN        1
#define SMI_ACC2X2_INT_SLO_NO_MOT_EN_Z_INT__MSK        0x04
#define SMI_ACC2X2_INT_SLO_NO_MOT_EN_Z_INT__REG        SMI_ACC2X2_INT_SLO_NO_MOT_REG

#define SMI_ACC2X2_INT_SLO_NO_MOT_EN_SEL_INT__POS        3
#define SMI_ACC2X2_INT_SLO_NO_MOT_EN_SEL_INT__LEN        1
#define SMI_ACC2X2_INT_SLO_NO_MOT_EN_SEL_INT__MSK        0x08
#define SMI_ACC2X2_INT_SLO_NO_MOT_EN_SEL_INT__REG        SMI_ACC2X2_INT_SLO_NO_MOT_REG

#define SMI_ACC2X2_EN_INT1_PAD_LOWG__POS        0
#define SMI_ACC2X2_EN_INT1_PAD_LOWG__LEN        1
#define SMI_ACC2X2_EN_INT1_PAD_LOWG__MSK        0x01
#define SMI_ACC2X2_EN_INT1_PAD_LOWG__REG        SMI_ACC2X2_INT1_PAD_SEL_REG

#define SMI_ACC2X2_EN_INT1_PAD_HIGHG__POS       1
#define SMI_ACC2X2_EN_INT1_PAD_HIGHG__LEN       1
#define SMI_ACC2X2_EN_INT1_PAD_HIGHG__MSK       0x02
#define SMI_ACC2X2_EN_INT1_PAD_HIGHG__REG       SMI_ACC2X2_INT1_PAD_SEL_REG

#define SMI_ACC2X2_EN_INT1_PAD_SLOPE__POS       2
#define SMI_ACC2X2_EN_INT1_PAD_SLOPE__LEN       1
#define SMI_ACC2X2_EN_INT1_PAD_SLOPE__MSK       0x04
#define SMI_ACC2X2_EN_INT1_PAD_SLOPE__REG       SMI_ACC2X2_INT1_PAD_SEL_REG

#define SMI_ACC2X2_EN_INT1_PAD_SLO_NO_MOT__POS        3
#define SMI_ACC2X2_EN_INT1_PAD_SLO_NO_MOT__LEN        1
#define SMI_ACC2X2_EN_INT1_PAD_SLO_NO_MOT__MSK        0x08
#define SMI_ACC2X2_EN_INT1_PAD_SLO_NO_MOT__REG        SMI_ACC2X2_INT1_PAD_SEL_REG

#define SMI_ACC2X2_EN_INT1_PAD_DB_TAP__POS      4
#define SMI_ACC2X2_EN_INT1_PAD_DB_TAP__LEN      1
#define SMI_ACC2X2_EN_INT1_PAD_DB_TAP__MSK      0x10
#define SMI_ACC2X2_EN_INT1_PAD_DB_TAP__REG      SMI_ACC2X2_INT1_PAD_SEL_REG

#define SMI_ACC2X2_EN_INT1_PAD_SNG_TAP__POS     5
#define SMI_ACC2X2_EN_INT1_PAD_SNG_TAP__LEN     1
#define SMI_ACC2X2_EN_INT1_PAD_SNG_TAP__MSK     0x20
#define SMI_ACC2X2_EN_INT1_PAD_SNG_TAP__REG     SMI_ACC2X2_INT1_PAD_SEL_REG

#define SMI_ACC2X2_EN_INT1_PAD_ORIENT__POS      6
#define SMI_ACC2X2_EN_INT1_PAD_ORIENT__LEN      1
#define SMI_ACC2X2_EN_INT1_PAD_ORIENT__MSK      0x40
#define SMI_ACC2X2_EN_INT1_PAD_ORIENT__REG      SMI_ACC2X2_INT1_PAD_SEL_REG

#define SMI_ACC2X2_EN_INT1_PAD_FLAT__POS        7
#define SMI_ACC2X2_EN_INT1_PAD_FLAT__LEN        1
#define SMI_ACC2X2_EN_INT1_PAD_FLAT__MSK        0x80
#define SMI_ACC2X2_EN_INT1_PAD_FLAT__REG        SMI_ACC2X2_INT1_PAD_SEL_REG

#define SMI_ACC2X2_EN_INT2_PAD_LOWG__POS        0
#define SMI_ACC2X2_EN_INT2_PAD_LOWG__LEN        1
#define SMI_ACC2X2_EN_INT2_PAD_LOWG__MSK        0x01
#define SMI_ACC2X2_EN_INT2_PAD_LOWG__REG        SMI_ACC2X2_INT2_PAD_SEL_REG

#define SMI_ACC2X2_EN_INT2_PAD_HIGHG__POS       1
#define SMI_ACC2X2_EN_INT2_PAD_HIGHG__LEN       1
#define SMI_ACC2X2_EN_INT2_PAD_HIGHG__MSK       0x02
#define SMI_ACC2X2_EN_INT2_PAD_HIGHG__REG       SMI_ACC2X2_INT2_PAD_SEL_REG

#define SMI_ACC2X2_EN_INT2_PAD_SLOPE__POS       2
#define SMI_ACC2X2_EN_INT2_PAD_SLOPE__LEN       1
#define SMI_ACC2X2_EN_INT2_PAD_SLOPE__MSK       0x04
#define SMI_ACC2X2_EN_INT2_PAD_SLOPE__REG       SMI_ACC2X2_INT2_PAD_SEL_REG

#define SMI_ACC2X2_EN_INT2_PAD_SLO_NO_MOT__POS        3
#define SMI_ACC2X2_EN_INT2_PAD_SLO_NO_MOT__LEN        1
#define SMI_ACC2X2_EN_INT2_PAD_SLO_NO_MOT__MSK        0x08
#define SMI_ACC2X2_EN_INT2_PAD_SLO_NO_MOT__REG        SMI_ACC2X2_INT2_PAD_SEL_REG

#define SMI_ACC2X2_EN_INT2_PAD_DB_TAP__POS      4
#define SMI_ACC2X2_EN_INT2_PAD_DB_TAP__LEN      1
#define SMI_ACC2X2_EN_INT2_PAD_DB_TAP__MSK      0x10
#define SMI_ACC2X2_EN_INT2_PAD_DB_TAP__REG      SMI_ACC2X2_INT2_PAD_SEL_REG

#define SMI_ACC2X2_EN_INT2_PAD_SNG_TAP__POS     5
#define SMI_ACC2X2_EN_INT2_PAD_SNG_TAP__LEN     1
#define SMI_ACC2X2_EN_INT2_PAD_SNG_TAP__MSK     0x20
#define SMI_ACC2X2_EN_INT2_PAD_SNG_TAP__REG     SMI_ACC2X2_INT2_PAD_SEL_REG

#define SMI_ACC2X2_EN_INT2_PAD_ORIENT__POS      6
#define SMI_ACC2X2_EN_INT2_PAD_ORIENT__LEN      1
#define SMI_ACC2X2_EN_INT2_PAD_ORIENT__MSK      0x40
#define SMI_ACC2X2_EN_INT2_PAD_ORIENT__REG      SMI_ACC2X2_INT2_PAD_SEL_REG

#define SMI_ACC2X2_EN_INT2_PAD_FLAT__POS        7
#define SMI_ACC2X2_EN_INT2_PAD_FLAT__LEN        1
#define SMI_ACC2X2_EN_INT2_PAD_FLAT__MSK        0x80
#define SMI_ACC2X2_EN_INT2_PAD_FLAT__REG        SMI_ACC2X2_INT2_PAD_SEL_REG

#define SMI_ACC2X2_EN_INT1_PAD_NEWDATA__POS     0
#define SMI_ACC2X2_EN_INT1_PAD_NEWDATA__LEN     1
#define SMI_ACC2X2_EN_INT1_PAD_NEWDATA__MSK     0x01
#define SMI_ACC2X2_EN_INT1_PAD_NEWDATA__REG     SMI_ACC2X2_INT_DATA_SEL_REG

#define SMI_ACC2X2_EN_INT1_PAD_FWM__POS     1
#define SMI_ACC2X2_EN_INT1_PAD_FWM__LEN     1
#define SMI_ACC2X2_EN_INT1_PAD_FWM__MSK     0x02
#define SMI_ACC2X2_EN_INT1_PAD_FWM__REG     SMI_ACC2X2_INT_DATA_SEL_REG

#define SMI_ACC2X2_EN_INT1_PAD_FFULL__POS     2
#define SMI_ACC2X2_EN_INT1_PAD_FFULL__LEN     1
#define SMI_ACC2X2_EN_INT1_PAD_FFULL__MSK     0x04
#define SMI_ACC2X2_EN_INT1_PAD_FFULL__REG     SMI_ACC2X2_INT_DATA_SEL_REG

#define SMI_ACC2X2_EN_INT2_PAD_FFULL__POS     5
#define SMI_ACC2X2_EN_INT2_PAD_FFULL__LEN     1
#define SMI_ACC2X2_EN_INT2_PAD_FFULL__MSK     0x20
#define SMI_ACC2X2_EN_INT2_PAD_FFULL__REG     SMI_ACC2X2_INT_DATA_SEL_REG

#define SMI_ACC2X2_EN_INT2_PAD_FWM__POS     6
#define SMI_ACC2X2_EN_INT2_PAD_FWM__LEN     1
#define SMI_ACC2X2_EN_INT2_PAD_FWM__MSK     0x40
#define SMI_ACC2X2_EN_INT2_PAD_FWM__REG     SMI_ACC2X2_INT_DATA_SEL_REG

#define SMI_ACC2X2_EN_INT2_PAD_NEWDATA__POS     7
#define SMI_ACC2X2_EN_INT2_PAD_NEWDATA__LEN     1
#define SMI_ACC2X2_EN_INT2_PAD_NEWDATA__MSK     0x80
#define SMI_ACC2X2_EN_INT2_PAD_NEWDATA__REG     SMI_ACC2X2_INT_DATA_SEL_REG

#define SMI_ACC2X2_UNFILT_INT_SRC_LOWG__POS        0
#define SMI_ACC2X2_UNFILT_INT_SRC_LOWG__LEN        1
#define SMI_ACC2X2_UNFILT_INT_SRC_LOWG__MSK        0x01
#define SMI_ACC2X2_UNFILT_INT_SRC_LOWG__REG        SMI_ACC2X2_INT_SRC_REG

#define SMI_ACC2X2_UNFILT_INT_SRC_HIGHG__POS       1
#define SMI_ACC2X2_UNFILT_INT_SRC_HIGHG__LEN       1
#define SMI_ACC2X2_UNFILT_INT_SRC_HIGHG__MSK       0x02
#define SMI_ACC2X2_UNFILT_INT_SRC_HIGHG__REG       SMI_ACC2X2_INT_SRC_REG

#define SMI_ACC2X2_UNFILT_INT_SRC_SLOPE__POS       2
#define SMI_ACC2X2_UNFILT_INT_SRC_SLOPE__LEN       1
#define SMI_ACC2X2_UNFILT_INT_SRC_SLOPE__MSK       0x04
#define SMI_ACC2X2_UNFILT_INT_SRC_SLOPE__REG       SMI_ACC2X2_INT_SRC_REG

#define SMI_ACC2X2_UNFILT_INT_SRC_SLO_NO_MOT__POS        3
#define SMI_ACC2X2_UNFILT_INT_SRC_SLO_NO_MOT__LEN        1
#define SMI_ACC2X2_UNFILT_INT_SRC_SLO_NO_MOT__MSK        0x08
#define SMI_ACC2X2_UNFILT_INT_SRC_SLO_NO_MOT__REG        SMI_ACC2X2_INT_SRC_REG

#define SMI_ACC2X2_UNFILT_INT_SRC_TAP__POS         4
#define SMI_ACC2X2_UNFILT_INT_SRC_TAP__LEN         1
#define SMI_ACC2X2_UNFILT_INT_SRC_TAP__MSK         0x10
#define SMI_ACC2X2_UNFILT_INT_SRC_TAP__REG         SMI_ACC2X2_INT_SRC_REG

#define SMI_ACC2X2_UNFILT_INT_SRC_DATA__POS        5
#define SMI_ACC2X2_UNFILT_INT_SRC_DATA__LEN        1
#define SMI_ACC2X2_UNFILT_INT_SRC_DATA__MSK        0x20
#define SMI_ACC2X2_UNFILT_INT_SRC_DATA__REG        SMI_ACC2X2_INT_SRC_REG

#define SMI_ACC2X2_INT1_PAD_ACTIVE_LEVEL__POS       0
#define SMI_ACC2X2_INT1_PAD_ACTIVE_LEVEL__LEN       1
#define SMI_ACC2X2_INT1_PAD_ACTIVE_LEVEL__MSK       0x01
#define SMI_ACC2X2_INT1_PAD_ACTIVE_LEVEL__REG       SMI_ACC2X2_INT_SET_REG

#define SMI_ACC2X2_INT2_PAD_ACTIVE_LEVEL__POS       2
#define SMI_ACC2X2_INT2_PAD_ACTIVE_LEVEL__LEN       1
#define SMI_ACC2X2_INT2_PAD_ACTIVE_LEVEL__MSK       0x04
#define SMI_ACC2X2_INT2_PAD_ACTIVE_LEVEL__REG       SMI_ACC2X2_INT_SET_REG

#define SMI_ACC2X2_INT1_PAD_OUTPUT_TYPE__POS        1
#define SMI_ACC2X2_INT1_PAD_OUTPUT_TYPE__LEN        1
#define SMI_ACC2X2_INT1_PAD_OUTPUT_TYPE__MSK        0x02
#define SMI_ACC2X2_INT1_PAD_OUTPUT_TYPE__REG        SMI_ACC2X2_INT_SET_REG

#define SMI_ACC2X2_INT2_PAD_OUTPUT_TYPE__POS        3
#define SMI_ACC2X2_INT2_PAD_OUTPUT_TYPE__LEN        1
#define SMI_ACC2X2_INT2_PAD_OUTPUT_TYPE__MSK        0x08
#define SMI_ACC2X2_INT2_PAD_OUTPUT_TYPE__REG        SMI_ACC2X2_INT_SET_REG

#define SMI_ACC2X2_INT_MODE_SEL__POS                0
#define SMI_ACC2X2_INT_MODE_SEL__LEN                4
#define SMI_ACC2X2_INT_MODE_SEL__MSK                0x0F
#define SMI_ACC2X2_INT_MODE_SEL__REG                SMI_ACC2X2_INT_CTRL_REG

#define SMI_ACC2X2_RESET_INT__POS           7
#define SMI_ACC2X2_RESET_INT__LEN           1
#define SMI_ACC2X2_RESET_INT__MSK           0x80
#define SMI_ACC2X2_RESET_INT__REG           SMI_ACC2X2_INT_CTRL_REG

#define SMI_ACC2X2_LOWG_DUR__POS                    0
#define SMI_ACC2X2_LOWG_DUR__LEN                    8
#define SMI_ACC2X2_LOWG_DUR__MSK                    0xFF
#define SMI_ACC2X2_LOWG_DUR__REG                    SMI_ACC2X2_LOW_DURN_REG

#define SMI_ACC2X2_LOWG_THRES__POS                  0
#define SMI_ACC2X2_LOWG_THRES__LEN                  8
#define SMI_ACC2X2_LOWG_THRES__MSK                  0xFF
#define SMI_ACC2X2_LOWG_THRES__REG                  SMI_ACC2X2_LOW_THRES_REG

#define SMI_ACC2X2_LOWG_HYST__POS                   0
#define SMI_ACC2X2_LOWG_HYST__LEN                   2
#define SMI_ACC2X2_LOWG_HYST__MSK                   0x03
#define SMI_ACC2X2_LOWG_HYST__REG                   SMI_ACC2X2_LOW_HIGH_HYST_REG

#define SMI_ACC2X2_LOWG_INT_MODE__POS               2
#define SMI_ACC2X2_LOWG_INT_MODE__LEN               1
#define SMI_ACC2X2_LOWG_INT_MODE__MSK               0x04
#define SMI_ACC2X2_LOWG_INT_MODE__REG               SMI_ACC2X2_LOW_HIGH_HYST_REG

#define SMI_ACC2X2_HIGHG_DUR__POS                    0
#define SMI_ACC2X2_HIGHG_DUR__LEN                    8
#define SMI_ACC2X2_HIGHG_DUR__MSK                    0xFF
#define SMI_ACC2X2_HIGHG_DUR__REG                    SMI_ACC2X2_HIGH_DURN_REG

#define SMI_ACC2X2_HIGHG_THRES__POS                  0
#define SMI_ACC2X2_HIGHG_THRES__LEN                  8
#define SMI_ACC2X2_HIGHG_THRES__MSK                  0xFF
#define SMI_ACC2X2_HIGHG_THRES__REG                  SMI_ACC2X2_HIGH_THRES_REG

#define SMI_ACC2X2_HIGHG_HYST__POS                  6
#define SMI_ACC2X2_HIGHG_HYST__LEN                  2
#define SMI_ACC2X2_HIGHG_HYST__MSK                  0xC0
#define SMI_ACC2X2_HIGHG_HYST__REG                  SMI_ACC2X2_LOW_HIGH_HYST_REG

#define SMI_ACC2X2_SLOPE_DUR__POS                    0
#define SMI_ACC2X2_SLOPE_DUR__LEN                    2
#define SMI_ACC2X2_SLOPE_DUR__MSK                    0x03
#define SMI_ACC2X2_SLOPE_DUR__REG                    SMI_ACC2X2_SLOPE_DURN_REG

#define SMI_ACC2X2_SLO_NO_MOT_DUR__POS                    2
#define SMI_ACC2X2_SLO_NO_MOT_DUR__LEN                    6
#define SMI_ACC2X2_SLO_NO_MOT_DUR__MSK                    0xFC
#define SMI_ACC2X2_SLO_NO_MOT_DUR__REG                    SMI_ACC2X2_SLOPE_DURN_REG

#define SMI_ACC2X2_SLOPE_THRES__POS                  0
#define SMI_ACC2X2_SLOPE_THRES__LEN                  8
#define SMI_ACC2X2_SLOPE_THRES__MSK                  0xFF
#define SMI_ACC2X2_SLOPE_THRES__REG                  SMI_ACC2X2_SLOPE_THRES_REG

#define SMI_ACC2X2_SLO_NO_MOT_THRES__POS                  0
#define SMI_ACC2X2_SLO_NO_MOT_THRES__LEN                  8
#define SMI_ACC2X2_SLO_NO_MOT_THRES__MSK                  0xFF
#define SMI_ACC2X2_SLO_NO_MOT_THRES__REG           SMI_ACC2X2_SLO_NO_MOT_THRES_REG

#define SMI_ACC2X2_TAP_DUR__POS                    0
#define SMI_ACC2X2_TAP_DUR__LEN                    3
#define SMI_ACC2X2_TAP_DUR__MSK                    0x07
#define SMI_ACC2X2_TAP_DUR__REG                    SMI_ACC2X2_TAP_PARAM_REG

#define SMI_ACC2X2_TAP_SHOCK_DURN__POS             6
#define SMI_ACC2X2_TAP_SHOCK_DURN__LEN             1
#define SMI_ACC2X2_TAP_SHOCK_DURN__MSK             0x40
#define SMI_ACC2X2_TAP_SHOCK_DURN__REG             SMI_ACC2X2_TAP_PARAM_REG

#define SMI_ACC2X2_ADV_TAP_INT__POS                5
#define SMI_ACC2X2_ADV_TAP_INT__LEN                1
#define SMI_ACC2X2_ADV_TAP_INT__MSK                0x20
#define SMI_ACC2X2_ADV_TAP_INT__REG                SMI_ACC2X2_TAP_PARAM_REG

#define SMI_ACC2X2_TAP_QUIET_DURN__POS             7
#define SMI_ACC2X2_TAP_QUIET_DURN__LEN             1
#define SMI_ACC2X2_TAP_QUIET_DURN__MSK             0x80
#define SMI_ACC2X2_TAP_QUIET_DURN__REG             SMI_ACC2X2_TAP_PARAM_REG

#define SMI_ACC2X2_TAP_THRES__POS                  0
#define SMI_ACC2X2_TAP_THRES__LEN                  5
#define SMI_ACC2X2_TAP_THRES__MSK                  0x1F
#define SMI_ACC2X2_TAP_THRES__REG                  SMI_ACC2X2_TAP_THRES_REG

#define SMI_ACC2X2_TAP_SAMPLES__POS                6
#define SMI_ACC2X2_TAP_SAMPLES__LEN                2
#define SMI_ACC2X2_TAP_SAMPLES__MSK                0xC0
#define SMI_ACC2X2_TAP_SAMPLES__REG                SMI_ACC2X2_TAP_THRES_REG

#define SMI_ACC2X2_ORIENT_MODE__POS                  0
#define SMI_ACC2X2_ORIENT_MODE__LEN                  2
#define SMI_ACC2X2_ORIENT_MODE__MSK                  0x03
#define SMI_ACC2X2_ORIENT_MODE__REG                  SMI_ACC2X2_ORIENT_PARAM_REG

#define SMI_ACC2X2_ORIENT_BLOCK__POS                 2
#define SMI_ACC2X2_ORIENT_BLOCK__LEN                 2
#define SMI_ACC2X2_ORIENT_BLOCK__MSK                 0x0C
#define SMI_ACC2X2_ORIENT_BLOCK__REG                 SMI_ACC2X2_ORIENT_PARAM_REG

#define SMI_ACC2X2_ORIENT_HYST__POS                  4
#define SMI_ACC2X2_ORIENT_HYST__LEN                  3
#define SMI_ACC2X2_ORIENT_HYST__MSK                  0x70
#define SMI_ACC2X2_ORIENT_HYST__REG                  SMI_ACC2X2_ORIENT_PARAM_REG

#define SMI_ACC2X2_ORIENT_AXIS__POS                  7
#define SMI_ACC2X2_ORIENT_AXIS__LEN                  1
#define SMI_ACC2X2_ORIENT_AXIS__MSK                  0x80
#define SMI_ACC2X2_ORIENT_AXIS__REG                  SMI_ACC2X2_THETA_BLOCK_REG

#define SMI_ACC2X2_ORIENT_UD_EN__POS                  6
#define SMI_ACC2X2_ORIENT_UD_EN__LEN                  1
#define SMI_ACC2X2_ORIENT_UD_EN__MSK                  0x40
#define SMI_ACC2X2_ORIENT_UD_EN__REG                  SMI_ACC2X2_THETA_BLOCK_REG

#define SMI_ACC2X2_THETA_BLOCK__POS                  0
#define SMI_ACC2X2_THETA_BLOCK__LEN                  6
#define SMI_ACC2X2_THETA_BLOCK__MSK                  0x3F
#define SMI_ACC2X2_THETA_BLOCK__REG                  SMI_ACC2X2_THETA_BLOCK_REG

#define SMI_ACC2X2_THETA_FLAT__POS                  0
#define SMI_ACC2X2_THETA_FLAT__LEN                  6
#define SMI_ACC2X2_THETA_FLAT__MSK                  0x3F
#define SMI_ACC2X2_THETA_FLAT__REG                  SMI_ACC2X2_THETA_FLAT_REG

#define SMI_ACC2X2_FLAT_HOLD_TIME__POS              4
#define SMI_ACC2X2_FLAT_HOLD_TIME__LEN              2
#define SMI_ACC2X2_FLAT_HOLD_TIME__MSK              0x30
#define SMI_ACC2X2_FLAT_HOLD_TIME__REG              SMI_ACC2X2_FLAT_HOLD_TIME_REG

#define SMI_ACC2X2_FLAT_HYS__POS                   0
#define SMI_ACC2X2_FLAT_HYS__LEN                   3
#define SMI_ACC2X2_FLAT_HYS__MSK                   0x07
#define SMI_ACC2X2_FLAT_HYS__REG                   SMI_ACC2X2_FLAT_HOLD_TIME_REG

#define SMI_ACC2X2_FIFO_WML_TRIG_RETAIN__POS                   0
#define SMI_ACC2X2_FIFO_WML_TRIG_RETAIN__LEN                   6
#define SMI_ACC2X2_FIFO_WML_TRIG_RETAIN__MSK                   0x3F
#define SMI_ACC2X2_FIFO_WML_TRIG_RETAIN__REG                   SMI_ACC2X2_FIFO_WML_TRIG

#define SMI_ACC2X2_EN_SELF_TEST__POS                0
#define SMI_ACC2X2_EN_SELF_TEST__LEN                2
#define SMI_ACC2X2_EN_SELF_TEST__MSK                0x03
#define SMI_ACC2X2_EN_SELF_TEST__REG                SMI_ACC2X2_SELF_TEST_REG

#define SMI_ACC2X2_NEG_SELF_TEST__POS               2
#define SMI_ACC2X2_NEG_SELF_TEST__LEN               1
#define SMI_ACC2X2_NEG_SELF_TEST__MSK               0x04
#define SMI_ACC2X2_NEG_SELF_TEST__REG               SMI_ACC2X2_SELF_TEST_REG

#define SMI_ACC2X2_SELF_TEST_AMP__POS               4
#define SMI_ACC2X2_SELF_TEST_AMP__LEN               1
#define SMI_ACC2X2_SELF_TEST_AMP__MSK               0x10
#define SMI_ACC2X2_SELF_TEST_AMP__REG               SMI_ACC2X2_SELF_TEST_REG


#define SMI_ACC2X2_UNLOCK_EE_PROG_MODE__POS     0
#define SMI_ACC2X2_UNLOCK_EE_PROG_MODE__LEN     1
#define SMI_ACC2X2_UNLOCK_EE_PROG_MODE__MSK     0x01
#define SMI_ACC2X2_UNLOCK_EE_PROG_MODE__REG     SMI_ACC2X2_EEPROM_CTRL_REG

#define SMI_ACC2X2_START_EE_PROG_TRIG__POS      1
#define SMI_ACC2X2_START_EE_PROG_TRIG__LEN      1
#define SMI_ACC2X2_START_EE_PROG_TRIG__MSK      0x02
#define SMI_ACC2X2_START_EE_PROG_TRIG__REG      SMI_ACC2X2_EEPROM_CTRL_REG

#define SMI_ACC2X2_EE_PROG_READY__POS          2
#define SMI_ACC2X2_EE_PROG_READY__LEN          1
#define SMI_ACC2X2_EE_PROG_READY__MSK          0x04
#define SMI_ACC2X2_EE_PROG_READY__REG          SMI_ACC2X2_EEPROM_CTRL_REG

#define SMI_ACC2X2_UPDATE_IMAGE__POS                3
#define SMI_ACC2X2_UPDATE_IMAGE__LEN                1
#define SMI_ACC2X2_UPDATE_IMAGE__MSK                0x08
#define SMI_ACC2X2_UPDATE_IMAGE__REG                SMI_ACC2X2_EEPROM_CTRL_REG

#define SMI_ACC2X2_EE_REMAIN__POS                4
#define SMI_ACC2X2_EE_REMAIN__LEN                4
#define SMI_ACC2X2_EE_REMAIN__MSK                0xF0
#define SMI_ACC2X2_EE_REMAIN__REG                SMI_ACC2X2_EEPROM_CTRL_REG

#define SMI_ACC2X2_EN_SPI_MODE_3__POS              0
#define SMI_ACC2X2_EN_SPI_MODE_3__LEN              1
#define SMI_ACC2X2_EN_SPI_MODE_3__MSK              0x01
#define SMI_ACC2X2_EN_SPI_MODE_3__REG              SMI_ACC2X2_SERIAL_CTRL_REG

#define SMI_ACC2X2_I2C_WATCHDOG_PERIOD__POS        1
#define SMI_ACC2X2_I2C_WATCHDOG_PERIOD__LEN        1
#define SMI_ACC2X2_I2C_WATCHDOG_PERIOD__MSK        0x02
#define SMI_ACC2X2_I2C_WATCHDOG_PERIOD__REG        SMI_ACC2X2_SERIAL_CTRL_REG

#define SMI_ACC2X2_EN_I2C_WATCHDOG__POS            2
#define SMI_ACC2X2_EN_I2C_WATCHDOG__LEN            1
#define SMI_ACC2X2_EN_I2C_WATCHDOG__MSK            0x04
#define SMI_ACC2X2_EN_I2C_WATCHDOG__REG            SMI_ACC2X2_SERIAL_CTRL_REG

#define SMI_ACC2X2_EXT_MODE__POS              7
#define SMI_ACC2X2_EXT_MODE__LEN              1
#define SMI_ACC2X2_EXT_MODE__MSK              0x80
#define SMI_ACC2X2_EXT_MODE__REG              SMI_ACC2X2_EXTMODE_CTRL_REG

#define SMI_ACC2X2_ALLOW_UPPER__POS        6
#define SMI_ACC2X2_ALLOW_UPPER__LEN        1
#define SMI_ACC2X2_ALLOW_UPPER__MSK        0x40
#define SMI_ACC2X2_ALLOW_UPPER__REG        SMI_ACC2X2_EXTMODE_CTRL_REG

#define SMI_ACC2X2_MAP_2_LOWER__POS            5
#define SMI_ACC2X2_MAP_2_LOWER__LEN            1
#define SMI_ACC2X2_MAP_2_LOWER__MSK            0x20
#define SMI_ACC2X2_MAP_2_LOWER__REG            SMI_ACC2X2_EXTMODE_CTRL_REG

#define SMI_ACC2X2_MAGIC_NUMBER__POS            0
#define SMI_ACC2X2_MAGIC_NUMBER__LEN            5
#define SMI_ACC2X2_MAGIC_NUMBER__MSK            0x1F
#define SMI_ACC2X2_MAGIC_NUMBER__REG            SMI_ACC2X2_EXTMODE_CTRL_REG

#define SMI_ACC2X2_UNLOCK_EE_WRITE_TRIM__POS        4
#define SMI_ACC2X2_UNLOCK_EE_WRITE_TRIM__LEN        4
#define SMI_ACC2X2_UNLOCK_EE_WRITE_TRIM__MSK        0xF0
#define SMI_ACC2X2_UNLOCK_EE_WRITE_TRIM__REG        SMI_ACC2X2_CTRL_UNLOCK_REG

#define SMI_ACC2X2_EN_SLOW_COMP_X__POS              0
#define SMI_ACC2X2_EN_SLOW_COMP_X__LEN              1
#define SMI_ACC2X2_EN_SLOW_COMP_X__MSK              0x01
#define SMI_ACC2X2_EN_SLOW_COMP_X__REG              SMI_ACC2X2_OFFSET_CTRL_REG

#define SMI_ACC2X2_EN_SLOW_COMP_Y__POS              1
#define SMI_ACC2X2_EN_SLOW_COMP_Y__LEN              1
#define SMI_ACC2X2_EN_SLOW_COMP_Y__MSK              0x02
#define SMI_ACC2X2_EN_SLOW_COMP_Y__REG              SMI_ACC2X2_OFFSET_CTRL_REG

#define SMI_ACC2X2_EN_SLOW_COMP_Z__POS              2
#define SMI_ACC2X2_EN_SLOW_COMP_Z__LEN              1
#define SMI_ACC2X2_EN_SLOW_COMP_Z__MSK              0x04
#define SMI_ACC2X2_EN_SLOW_COMP_Z__REG              SMI_ACC2X2_OFFSET_CTRL_REG

#define SMI_ACC2X2_FAST_CAL_RDY_S__POS             4
#define SMI_ACC2X2_FAST_CAL_RDY_S__LEN             1
#define SMI_ACC2X2_FAST_CAL_RDY_S__MSK             0x10
#define SMI_ACC2X2_FAST_CAL_RDY_S__REG             SMI_ACC2X2_OFFSET_CTRL_REG

#define SMI_ACC2X2_CAL_TRIGGER__POS                5
#define SMI_ACC2X2_CAL_TRIGGER__LEN                2
#define SMI_ACC2X2_CAL_TRIGGER__MSK                0x60
#define SMI_ACC2X2_CAL_TRIGGER__REG                SMI_ACC2X2_OFFSET_CTRL_REG

#define SMI_ACC2X2_RESET_OFFSET_REGS__POS           7
#define SMI_ACC2X2_RESET_OFFSET_REGS__LEN           1
#define SMI_ACC2X2_RESET_OFFSET_REGS__MSK           0x80
#define SMI_ACC2X2_RESET_OFFSET_REGS__REG           SMI_ACC2X2_OFFSET_CTRL_REG

#define SMI_ACC2X2_COMP_CUTOFF__POS                 0
#define SMI_ACC2X2_COMP_CUTOFF__LEN                 1
#define SMI_ACC2X2_COMP_CUTOFF__MSK                 0x01
#define SMI_ACC2X2_COMP_CUTOFF__REG                 SMI_ACC2X2_OFFSET_PARAMS_REG

#define SMI_ACC2X2_COMP_TARGET_OFFSET_X__POS        1
#define SMI_ACC2X2_COMP_TARGET_OFFSET_X__LEN        2
#define SMI_ACC2X2_COMP_TARGET_OFFSET_X__MSK        0x06
#define SMI_ACC2X2_COMP_TARGET_OFFSET_X__REG        SMI_ACC2X2_OFFSET_PARAMS_REG

#define SMI_ACC2X2_COMP_TARGET_OFFSET_Y__POS        3
#define SMI_ACC2X2_COMP_TARGET_OFFSET_Y__LEN        2
#define SMI_ACC2X2_COMP_TARGET_OFFSET_Y__MSK        0x18
#define SMI_ACC2X2_COMP_TARGET_OFFSET_Y__REG        SMI_ACC2X2_OFFSET_PARAMS_REG

#define SMI_ACC2X2_COMP_TARGET_OFFSET_Z__POS        5
#define SMI_ACC2X2_COMP_TARGET_OFFSET_Z__LEN        2
#define SMI_ACC2X2_COMP_TARGET_OFFSET_Z__MSK        0x60
#define SMI_ACC2X2_COMP_TARGET_OFFSET_Z__REG        SMI_ACC2X2_OFFSET_PARAMS_REG

#define SMI_ACC2X2_FIFO_DATA_SELECT__POS                 0
#define SMI_ACC2X2_FIFO_DATA_SELECT__LEN                 2
#define SMI_ACC2X2_FIFO_DATA_SELECT__MSK                 0x03
#define SMI_ACC2X2_FIFO_DATA_SELECT__REG                 SMI_ACC2X2_FIFO_MODE_REG

#define SMI_ACC2X2_FIFO_TRIGGER_SOURCE__POS                 2
#define SMI_ACC2X2_FIFO_TRIGGER_SOURCE__LEN                 2
#define SMI_ACC2X2_FIFO_TRIGGER_SOURCE__MSK                 0x0C
#define SMI_ACC2X2_FIFO_TRIGGER_SOURCE__REG                 SMI_ACC2X2_FIFO_MODE_REG

#define SMI_ACC2X2_FIFO_TRIGGER_ACTION__POS                 4
#define SMI_ACC2X2_FIFO_TRIGGER_ACTION__LEN                 2
#define SMI_ACC2X2_FIFO_TRIGGER_ACTION__MSK                 0x30
#define SMI_ACC2X2_FIFO_TRIGGER_ACTION__REG                 SMI_ACC2X2_FIFO_MODE_REG

#define SMI_ACC2X2_FIFO_MODE__POS                 6
#define SMI_ACC2X2_FIFO_MODE__LEN                 2
#define SMI_ACC2X2_FIFO_MODE__MSK                 0xC0
#define SMI_ACC2X2_FIFO_MODE__REG                 SMI_ACC2X2_FIFO_MODE_REG


#define SMI_ACC2X2_STATUS1                             0
#define SMI_ACC2X2_STATUS2                             1
#define SMI_ACC2X2_STATUS3                             2
#define SMI_ACC2X2_STATUS4                             3
#define SMI_ACC2X2_STATUS5                             4


#define SMI_ACC2X2_RANGE_2G                 3
#define SMI_ACC2X2_RANGE_4G                 5
#define SMI_ACC2X2_RANGE_8G                 8
#define SMI_ACC2X2_RANGE_16G                12


#define SMI_ACC2X2_BW_7_81HZ        0x08
#define SMI_ACC2X2_BW_15_63HZ       0x09
#define SMI_ACC2X2_BW_31_25HZ       0x0A
#define SMI_ACC2X2_BW_62_50HZ       0x0B
#define SMI_ACC2X2_BW_125HZ         0x0C
#define SMI_ACC2X2_BW_250HZ         0x0D
#define SMI_ACC2X2_BW_500HZ         0x0E
#define SMI_ACC2X2_BW_1000HZ        0x0F

#define SMI_ACC2X2_SLEEP_DUR_0_5MS        0x05
#define SMI_ACC2X2_SLEEP_DUR_1MS          0x06
#define SMI_ACC2X2_SLEEP_DUR_2MS          0x07
#define SMI_ACC2X2_SLEEP_DUR_4MS          0x08
#define SMI_ACC2X2_SLEEP_DUR_6MS          0x09
#define SMI_ACC2X2_SLEEP_DUR_10MS         0x0A
#define SMI_ACC2X2_SLEEP_DUR_25MS         0x0B
#define SMI_ACC2X2_SLEEP_DUR_50MS         0x0C
#define SMI_ACC2X2_SLEEP_DUR_100MS        0x0D
#define SMI_ACC2X2_SLEEP_DUR_500MS        0x0E
#define SMI_ACC2X2_SLEEP_DUR_1S           0x0F

#define SMI_ACC2X2_LATCH_DUR_NON_LATCH    0x00
#define SMI_ACC2X2_LATCH_DUR_250MS        0x01
#define SMI_ACC2X2_LATCH_DUR_500MS        0x02
#define SMI_ACC2X2_LATCH_DUR_1S           0x03
#define SMI_ACC2X2_LATCH_DUR_2S           0x04
#define SMI_ACC2X2_LATCH_DUR_4S           0x05
#define SMI_ACC2X2_LATCH_DUR_8S           0x06
#define SMI_ACC2X2_LATCH_DUR_LATCH        0x07
#define SMI_ACC2X2_LATCH_DUR_NON_LATCH1   0x08
#define SMI_ACC2X2_LATCH_DUR_250US        0x09
#define SMI_ACC2X2_LATCH_DUR_500US        0x0A
#define SMI_ACC2X2_LATCH_DUR_1MS          0x0B
#define SMI_ACC2X2_LATCH_DUR_12_5MS       0x0C
#define SMI_ACC2X2_LATCH_DUR_25MS         0x0D
#define SMI_ACC2X2_LATCH_DUR_50MS         0x0E
#define SMI_ACC2X2_LATCH_DUR_LATCH1       0x0F

#define SMI_ACC2X2_MODE_NORMAL             0
#define SMI_ACC2X2_MODE_LOWPOWER1          1
#define SMI_ACC2X2_MODE_SUSPEND            2
#define SMI_ACC2X2_MODE_DEEP_SUSPEND       3
#define SMI_ACC2X2_MODE_LOWPOWER2          4
#define SMI_ACC2X2_MODE_STANDBY            5

#define SMI_ACC2X2_X_AXIS           0
#define SMI_ACC2X2_Y_AXIS           1
#define SMI_ACC2X2_Z_AXIS           2

#define SMI_ACC2X2_Low_G_Interrupt       0
#define SMI_ACC2X2_High_G_X_Interrupt    1
#define SMI_ACC2X2_High_G_Y_Interrupt    2
#define SMI_ACC2X2_High_G_Z_Interrupt    3
#define SMI_ACC2X2_DATA_EN               4
#define SMI_ACC2X2_Slope_X_Interrupt     5
#define SMI_ACC2X2_Slope_Y_Interrupt     6
#define SMI_ACC2X2_Slope_Z_Interrupt     7
#define SMI_ACC2X2_Single_Tap_Interrupt  8
#define SMI_ACC2X2_Double_Tap_Interrupt  9
#define SMI_ACC2X2_Orient_Interrupt      10
#define SMI_ACC2X2_Flat_Interrupt        11
#define SMI_ACC2X2_FFULL_INTERRUPT       12
#define SMI_ACC2X2_FWM_INTERRUPT         13

#define SMI_ACC2X2_INT1_LOWG         0
#define SMI_ACC2X2_INT2_LOWG         1
#define SMI_ACC2X2_INT1_HIGHG        0
#define SMI_ACC2X2_INT2_HIGHG        1
#define SMI_ACC2X2_INT1_SLOPE        0
#define SMI_ACC2X2_INT2_SLOPE        1
#define SMI_ACC2X2_INT1_SLO_NO_MOT   0
#define SMI_ACC2X2_INT2_SLO_NO_MOT   1
#define SMI_ACC2X2_INT1_DTAP         0
#define SMI_ACC2X2_INT2_DTAP         1
#define SMI_ACC2X2_INT1_STAP         0
#define SMI_ACC2X2_INT2_STAP         1
#define SMI_ACC2X2_INT1_ORIENT       0
#define SMI_ACC2X2_INT2_ORIENT       1
#define SMI_ACC2X2_INT1_FLAT         0
#define SMI_ACC2X2_INT2_FLAT         1
#define SMI_ACC2X2_INT1_NDATA        0
#define SMI_ACC2X2_INT2_NDATA        1
#define SMI_ACC2X2_INT1_FWM          0
#define SMI_ACC2X2_INT2_FWM          1
#define SMI_ACC2X2_INT1_FFULL        0
#define SMI_ACC2X2_INT2_FFULL        1

#define SMI_ACC2X2_SRC_LOWG         0
#define SMI_ACC2X2_SRC_HIGHG        1
#define SMI_ACC2X2_SRC_SLOPE        2
#define SMI_ACC2X2_SRC_SLO_NO_MOT   3
#define SMI_ACC2X2_SRC_TAP          4
#define SMI_ACC2X2_SRC_DATA         5

#define SMI_ACC2X2_INT1_OUTPUT      0
#define SMI_ACC2X2_INT2_OUTPUT      1
#define SMI_ACC2X2_INT1_LEVEL       0
#define SMI_ACC2X2_INT2_LEVEL       1

#define SMI_ACC2X2_LOW_DURATION            0
#define SMI_ACC2X2_HIGH_DURATION           1
#define SMI_ACC2X2_SLOPE_DURATION          2
#define SMI_ACC2X2_SLO_NO_MOT_DURATION     3

#define SMI_ACC2X2_LOW_THRESHOLD            0
#define SMI_ACC2X2_HIGH_THRESHOLD           1
#define SMI_ACC2X2_SLOPE_THRESHOLD          2
#define SMI_ACC2X2_SLO_NO_MOT_THRESHOLD     3


#define SMI_ACC2X2_LOWG_HYST                0
#define SMI_ACC2X2_HIGHG_HYST               1

#define SMI_ACC2X2_ORIENT_THETA             0
#define SMI_ACC2X2_FLAT_THETA               1

#define SMI_ACC2X2_I2C_SELECT               0
#define SMI_ACC2X2_I2C_EN                   1

#define SMI_ACC2X2_SLOW_COMP_X              0
#define SMI_ACC2X2_SLOW_COMP_Y              1
#define SMI_ACC2X2_SLOW_COMP_Z              2

#define SMI_ACC2X2_CUT_OFF                  0
#define SMI_ACC2X2_OFFSET_TRIGGER_X         1
#define SMI_ACC2X2_OFFSET_TRIGGER_Y         2
#define SMI_ACC2X2_OFFSET_TRIGGER_Z         3

#define SMI_ACC2X2_GP0                      0
#define SMI_ACC2X2_GP1                      1

#define SMI_ACC2X2_SLO_NO_MOT_EN_X          0
#define SMI_ACC2X2_SLO_NO_MOT_EN_Y          1
#define SMI_ACC2X2_SLO_NO_MOT_EN_Z          2
#define SMI_ACC2X2_SLO_NO_MOT_EN_SEL        3

#define SMI_ACC2X2_WAKE_UP_DUR_20MS         0
#define SMI_ACC2X2_WAKE_UP_DUR_80MS         1
#define SMI_ACC2X2_WAKE_UP_DUR_320MS                2
#define SMI_ACC2X2_WAKE_UP_DUR_2560MS               3

#define SMI_ACC2X2_SELF_TEST0_ON            1
#define SMI_ACC2X2_SELF_TEST1_ON            2

#define SMI_ACC2X2_EE_W_OFF                 0
#define SMI_ACC2X2_EE_W_ON                  1

#define SMI_ACC2X2_LOW_TH_IN_G(gthres, range)           ((256 * gthres) / range)


#define SMI_ACC2X2_HIGH_TH_IN_G(gthres, range)          ((256 * gthres) / range)


#define SMI_ACC2X2_LOW_HY_IN_G(ghyst, range)            ((32 * ghyst) / range)


#define SMI_ACC2X2_HIGH_HY_IN_G(ghyst, range)           ((32 * ghyst) / range)


#define SMI_ACC2X2_SLOPE_TH_IN_G(gthres, range)    ((128 * gthres) / range)


#define SMI_ACC2X2_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)


#define SMI_ACC2X2_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

#define CHECK_CHIP_ID_TIME_MAX 5
#define SMI_ACC255_CHIP_ID 0XFA
#define SMI_ACC250E_CHIP_ID 0XF9
#define SMI_ACC222E_CHIP_ID 0XF8
#define SMI_ACC280_CHIP_ID 0XFB
#define SMI_ACC355_CHIP_ID 0XEA

#define SMI_ACC255_TYPE 0
#define SMI_ACC250E_TYPE 1
#define SMI_ACC222E_TYPE 2
#define SMI_ACC280_TYPE 3

#define MAX_FIFO_F_LEVEL 32
#define MAX_FIFO_F_BYTES 6
#define SMI_ACC_MAX_RETRY_I2C_XFER (100)

#ifdef CONFIG_DOUBLE_TAP
#define DEFAULT_TAP_JUDGE_PERIOD 1000    /* default judge in 1 second */
#endif

/*! Bosch sensor unknown place*/
#define BOSCH_SENSOR_PLACE_UNKNOWN (-1)
/*! Bosch sensor remapping table size P0~P7*/
#define MAX_AXIS_REMAP_TAB_SZ 8

/* How was SMI_ACC enabled(set to operation mode) */
#define SMI_ACC_ENABLED_ALL 0
#define SMI_ACC_ENABLED_SGM 1
#define SMI_ACC_ENABLED_DTAP 2
#define SMI_ACC_ENABLED_INPUT 3
#define SMI_ACC_ENABLED_BSX 4


/*!
 * @brief:BMI058 feature
 *  macro definition
*/

#define SMI_ACC2X2_FIFO_DAT_SEL_X                     1
#define SMI_ACC2X2_FIFO_DAT_SEL_Y                     2
#define SMI_ACC2X2_FIFO_DAT_SEL_Z                     3

#ifdef CONFIG_SENSORS_BMI058
#define C_BMI058_One_U8X                                 1
#define C_BMI058_Two_U8X                                 2
#define BMI058_OFFSET_TRIGGER_X                SMI_ACC2X2_OFFSET_TRIGGER_Y
#define BMI058_OFFSET_TRIGGER_Y                SMI_ACC2X2_OFFSET_TRIGGER_X

/*! BMI058 X AXIS OFFSET REG definition*/
#define BMI058_OFFSET_X_AXIS_REG              SMI_ACC2X2_OFFSET_Y_AXIS_REG
/*! BMI058 Y AXIS OFFSET REG definition*/
#define BMI058_OFFSET_Y_AXIS_REG              SMI_ACC2X2_OFFSET_X_AXIS_REG

#define BMI058_FIFO_DAT_SEL_X                       SMI_ACC2X2_FIFO_DAT_SEL_Y
#define BMI058_FIFO_DAT_SEL_Y                       SMI_ACC2X2_FIFO_DAT_SEL_X

/*! SMI130_ACC common slow no motion X interrupt type definition*/
#define SMI_ACC2X2_SLOW_NO_MOT_X_INT          12
/*! SMI130_ACC common slow no motion Y interrupt type definition*/
#define SMI_ACC2X2_SLOW_NO_MOT_Y_INT          13
/*! SMI130_ACC common High G X interrupt type definition*/
#define SMI_ACC2X2_HIGHG_X_INT          1
/*! SMI130_ACC common High G Y interrupt type definition*/
#define SMI_ACC2X2_HIGHG_Y_INT          2
/*! SMI130_ACC common slope X interrupt type definition*/
#define SMI_ACC2X2_SLOPE_X_INT          5
/*! SMI130_ACC common slope Y interrupt type definition*/
#define SMI_ACC2X2_SLOPE_Y_INT          6

/*! this structure holds some interrupt types difference
**between SMI130_ACC and BMI058.
*/
struct interrupt_map_t {
	int x;
	int y;
};
/*!*Need to use SMI130_ACC Common interrupt type definition to
* instead of Some of BMI058 reversed Interrupt type
* because of HW Register.
* The reversed Interrupt types contain:
* slow_no_mot_x_int && slow_not_mot_y_int
* highg_x_int && highg_y_int
* slope_x_int && slope_y_int
**/
static const struct interrupt_map_t int_map[] = {
	{SMI_ACC2X2_SLOW_NO_MOT_X_INT, SMI_ACC2X2_SLOW_NO_MOT_Y_INT},
	{SMI_ACC2X2_HIGHG_X_INT, SMI_ACC2X2_HIGHG_Y_INT},
	{SMI_ACC2X2_SLOPE_X_INT, SMI_ACC2X2_SLOPE_Y_INT}
};

/*! high g or slope interrupt type definition for BMI058*/
/*! High G interrupt of x, y, z axis happened */
#define HIGH_G_INTERRUPT_X            HIGH_G_INTERRUPT_Y_HAPPENED
#define HIGH_G_INTERRUPT_Y            HIGH_G_INTERRUPT_X_HAPPENED
#define HIGH_G_INTERRUPT_Z            HIGH_G_INTERRUPT_Z_HAPPENED
/*! High G interrupt of x, y, z negative axis happened */
#define HIGH_G_INTERRUPT_X_N          HIGH_G_INTERRUPT_Y_NEGATIVE_HAPPENED
#define HIGH_G_INTERRUPT_Y_N          HIGH_G_INTERRUPT_X_NEGATIVE_HAPPENED
#define HIGH_G_INTERRUPT_Z_N          HIGH_G_INTERRUPT_Z_NEGATIVE_HAPPENED
/*! Slope interrupt of x, y, z axis happened */
#define SLOPE_INTERRUPT_X             SLOPE_INTERRUPT_Y_HAPPENED
#define SLOPE_INTERRUPT_Y             SLOPE_INTERRUPT_X_HAPPENED
#define SLOPE_INTERRUPT_Z             SLOPE_INTERRUPT_Z_HAPPENED
/*! Slope interrupt of x, y, z negative axis happened */
#define SLOPE_INTERRUPT_X_N           SLOPE_INTERRUPT_Y_NEGATIVE_HAPPENED
#define SLOPE_INTERRUPT_Y_N           SLOPE_INTERRUPT_X_NEGATIVE_HAPPENED
#define SLOPE_INTERRUPT_Z_N           SLOPE_INTERRUPT_Z_NEGATIVE_HAPPENED


#else

/*! high g or slope interrupt type definition*/
/*! High G interrupt of x, y, z axis happened */
#define HIGH_G_INTERRUPT_X            HIGH_G_INTERRUPT_X_HAPPENED
#define HIGH_G_INTERRUPT_Y            HIGH_G_INTERRUPT_Y_HAPPENED
#define HIGH_G_INTERRUPT_Z            HIGH_G_INTERRUPT_Z_HAPPENED
/*! High G interrupt of x, y, z negative axis happened */
#define HIGH_G_INTERRUPT_X_N          HIGH_G_INTERRUPT_X_NEGATIVE_HAPPENED
#define HIGH_G_INTERRUPT_Y_N          HIGH_G_INTERRUPT_Y_NEGATIVE_HAPPENED
#define HIGH_G_INTERRUPT_Z_N          HIGH_G_INTERRUPT_Z_NEGATIVE_HAPPENED
/*! Slope interrupt of x, y, z axis happened */
#define SLOPE_INTERRUPT_X             SLOPE_INTERRUPT_X_HAPPENED
#define SLOPE_INTERRUPT_Y             SLOPE_INTERRUPT_Y_HAPPENED
#define SLOPE_INTERRUPT_Z             SLOPE_INTERRUPT_Z_HAPPENED
/*! Slope interrupt of x, y, z negative axis happened */
#define SLOPE_INTERRUPT_X_N           SLOPE_INTERRUPT_X_NEGATIVE_HAPPENED
#define SLOPE_INTERRUPT_Y_N           SLOPE_INTERRUPT_Y_NEGATIVE_HAPPENED
#define SLOPE_INTERRUPT_Z_N           SLOPE_INTERRUPT_Z_NEGATIVE_HAPPENED


#endif/*End of CONFIG_SENSORS_BMI058*/

/*! A workaroud mask definition with complete resolution exists
* aim at writing operation FIFO_CONFIG_1, 0x3E register */
#define FIFO_WORKAROUNDS_MSK         SMI_ACC2X2_FIFO_TRIGGER_SOURCE__MSK

struct smi130_acc_type_map_t {

	/*! smi130_acc sensor chip id */
	uint16_t chip_id;

	/*! smi130_acc sensor type */
	uint16_t sensor_type;

	/*! smi130_acc sensor name */
	const char *sensor_name;
};

static const struct smi130_acc_type_map_t sensor_type_map[] = {

	{SMI_ACC255_CHIP_ID, SMI_ACC255_TYPE, "SMI_ACC255/254"},
	{SMI_ACC355_CHIP_ID, SMI_ACC255_TYPE, "SMI_ACC355"},
	{SMI_ACC250E_CHIP_ID, SMI_ACC250E_TYPE, "SMI_ACC250E"},
	{SMI_ACC222E_CHIP_ID, SMI_ACC222E_TYPE, "SMI_ACC222E"},
	{SMI_ACC280_CHIP_ID, SMI_ACC280_TYPE, "SMI_ACC280"},

};

/*!
* Bst sensor common definition,
* please give parameters in BSP file.
*/
struct bosch_sensor_specific {
	char *name;
	/* 0 to 7 */
	int place;
	int irq;
	int (*irq_gpio_cfg)(void);
};


/*!
 * we use a typedef to hide the detail,
 * because this type might be changed
 */
struct bosch_sensor_axis_remap {
	/* src means which source will be mapped to target x, y, z axis */
	/* if an target OS axis is remapped from (-)x,
	 * src is 0, sign_* is (-)1 */
	/* if an target OS axis is remapped from (-)y,
	 * src is 1, sign_* is (-)1 */
	/* if an target OS axis is remapped from (-)z,
	 * src is 2, sign_* is (-)1 */
	int src_x:3;
	int src_y:3;
	int src_z:3;

	int sign_x:2;
	int sign_y:2;
	int sign_z:2;
};

struct bosch_sensor_data {
	union {
		int16_t v[3];
		struct {
			int16_t x;
			int16_t y;
			int16_t z;
		};
	};
};

#ifdef CONFIG_ENABLE_SMI_ACC_GYRO_BUFFERING
#define SMI_ACC_MAXSAMPLE        4000
#define G_MAX                    23920640
struct smi_acc_sample {
	int xyz[3];
	unsigned int tsec;
	unsigned long long tnsec;
};
#endif

struct smi130_accacc {
	s16 x;
	s16 y;
	s16 z;
};

struct smi130_acc_data {
	struct i2c_client *smi130_acc_client;
	atomic_t delay;
	atomic_t enable;
	atomic_t selftest_result;
	unsigned int chip_id;
	unsigned int fifo_count;
	unsigned char fifo_datasel;
	unsigned char mode;
	signed char sensor_type;
	uint64_t timestamp;
	uint64_t fifo_time;
	uint64_t base_time;
	uint64_t acc_count;
	uint64_t time_odr;
	uint8_t debug_level;
	struct work_struct report_data_work;
	int is_timer_running;
	struct hrtimer timer;
	ktime_t work_delay_kt;
	struct input_dev *input;

	struct bosch_dev *bosch_acc;

	struct smi130_accacc value;
	struct mutex value_mutex;
	struct mutex enable_mutex;
	struct mutex mode_mutex;
	struct delayed_work work;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	int16_t IRQ;
	struct bosch_sensor_specific *bosch_pd;

	int smi_acc_mode_enabled;
	struct input_dev *dev_interrupt;

#ifdef CONFIG_SIG_MOTION
	struct class *g_sensor_class;
	struct device *g_sensor_dev;

	/*struct smi_acc250_platform_data *pdata;*/
	atomic_t en_sig_motion;
#endif

#ifdef CONFIG_DOUBLE_TAP
	struct class *g_sensor_class_doubletap;
	struct device *g_sensor_dev_doubletap;
	atomic_t en_double_tap;
	unsigned char tap_times;
	struct mutex		tap_mutex;
	struct timer_list	tap_timer;
	int tap_time_period;
#endif
#ifdef CONFIG_ENABLE_SMI_ACC_GYRO_BUFFERING
	bool read_acc_boot_sample;
	int acc_bufsample_cnt;
	bool acc_buffer_smi130_samples;
	struct kmem_cache *smi_acc_cachepool;
	struct smi_acc_sample *smi130_acc_samplist[SMI_ACC_MAXSAMPLE];
	int max_buffer_time;
	struct input_dev *accbuf_dev;
	int report_evt_cnt;
#endif
#ifdef SMI130_HRTIMER
	struct hrtimer smi130_hrtimer;
#endif
};

#ifdef SMI130_HRTIMER
static void smi130_set_cpu_idle_state(bool value)
{
	cpu_idle_poll_ctrl(value);
}
static enum hrtimer_restart smi130_timer_function(struct hrtimer *timer)
{
	smi130_set_cpu_idle_state(true);

	return HRTIMER_NORESTART;
}
static void smi130_hrtimer_reset(struct smi130_acc_data *data)
{
	hrtimer_cancel(&data->smi130_hrtimer);
	/*forward HRTIMER just before 1ms of irq arrival*/
	hrtimer_forward(&data->smi130_hrtimer, ktime_get(),
			ns_to_ktime(data->time_odr - 1000000));
	hrtimer_restart(&data->smi130_hrtimer);
}
static void smi130_hrtimer_init(struct smi130_acc_data *data)
{
	hrtimer_init(&data->smi130_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->smi130_hrtimer.function = smi130_timer_function;
}
static void smi130_hrtimer_cleanup(struct smi130_acc_data *data)
{
	hrtimer_cancel(&data->smi130_hrtimer);
}
#else
static void smi130_set_cpu_idle_state(bool value)
{
}
static void smi130_hrtimer_reset(struct smi130_acc_data *data)
{

}
static void smi130_hrtimer_init(struct smi130_acc_data *data)
{

}
static void smi130_hrtimer_remove(struct smi130_acc_data *data)
{

}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void smi130_acc_early_suspend(struct early_suspend *h);
static void smi130_acc_late_resume(struct early_suspend *h);
#endif

static int smi130_acc_set_mode(struct i2c_client *client,
			u8 mode, u8 enabled_mode);
static int smi130_acc_get_mode(struct i2c_client *client, u8 *mode);
static int smi130_acc_get_fifo_mode(struct i2c_client *client, u8 *fifo_mode);
static int smi130_acc_set_fifo_mode(struct i2c_client *client, u8 fifo_mode);
static int smi130_acc_normal_to_suspend(struct smi130_acc_data *smi130_acc,
				unsigned char data1, unsigned char data2);

static void smi130_acc_delay(u32 msec)
{
	if (msec <= 20)
		usleep_range(msec * 1000, msec * 1000);
	else
		msleep(msec);
}
/*Remapping for SMI_ACC2X2*/
static const struct bosch_sensor_axis_remap
bosch_axis_remap_tab_dft[MAX_AXIS_REMAP_TAB_SZ] = {
	/* src_x src_y src_z  sign_x  sign_y  sign_z */
	{  0,    1,    2,     1,      1,      1 }, /* P0 */
	{  1,    0,    2,     1,     -1,      1 }, /* P1 */
	{  0,    1,    2,    -1,     -1,      1 }, /* P2 */
	{  1,    0,    2,    -1,      1,      1 }, /* P3 */

	{  0,    1,    2,    -1,      1,     -1 }, /* P4 */
	{  1,    0,    2,    -1,     -1,     -1 }, /* P5 */
	{  0,    1,    2,     1,     -1,     -1 }, /* P6 */
	{  1,    0,    2,     1,      1,     -1 }, /* P7 */
};


static void bosch_remap_sensor_data(struct bosch_sensor_data *data,
		const struct bosch_sensor_axis_remap *remap)
{
	struct bosch_sensor_data tmp;

	tmp.x = data->v[remap->src_x] * remap->sign_x;
	tmp.y = data->v[remap->src_y] * remap->sign_y;
	tmp.z = data->v[remap->src_z] * remap->sign_z;

	memcpy(data, &tmp, sizeof(*data));
}


static void bosch_remap_sensor_data_dft_tab(struct bosch_sensor_data *data,
		int place)
{
	/* sensor with place 0 needs not to be remapped */
	if ((place <= 0) || (place >= MAX_AXIS_REMAP_TAB_SZ))
		return;

	bosch_remap_sensor_data(data, &bosch_axis_remap_tab_dft[place]);
}

static void smi130_acc_remap_sensor_data(struct smi130_accacc *val,
		struct smi130_acc_data *client_data)
{
	struct bosch_sensor_data bsd;
	int place;

	if ((NULL == client_data->bosch_pd) || (BOSCH_SENSOR_PLACE_UNKNOWN
			 == client_data->bosch_pd->place))
		place = BOSCH_SENSOR_PLACE_UNKNOWN;
	else
		place = client_data->bosch_pd->place;

#ifdef CONFIG_SENSORS_BMI058
/*x,y need to be invesed becase of HW Register for BMI058*/
	bsd.y = val->x;
	bsd.x = val->y;
	bsd.z = val->z;
#else
	bsd.x = val->x;
	bsd.y = val->y;
	bsd.z = val->z;
#endif

	bosch_remap_sensor_data_dft_tab(&bsd, place);

	val->x = bsd.x;
	val->y = bsd.y;
	val->z = bsd.z;

}


static int smi130_acc_smbus_read_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
#if !defined SMI130_ACC_USE_BASIC_I2C_FUNC
	s32 dummy;
	int len = 1;
	if (NULL == client)
		return -ENODEV;

	while (0 != len--) {
#ifdef SMI130_ACC_SMBUS
		dummy = i2c_smbus_read_byte_data(client, reg_addr);
		if (dummy < 0) {
			PERR("i2c bus read error");
			return -EIO;
		}
		*data = (u8)(dummy & 0xff);
#else
		dummy = i2c_master_send(client, (char *)&reg_addr, 1);
		if (dummy < 0)
			return -EIO;

		dummy = i2c_master_recv(client, (char *)data, 1);
		if (dummy < 0)
			return -EIO;
#endif
		reg_addr++;
		data++;
	}
	return 0;
#else
	int retry;
	int len = 1;
	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg_addr,
		},

		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = len,
		 .buf = data,
		 },
	};

	for (retry = 0; retry < SMI_ACC_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			smi130_acc_delay(1);
	}

	if (SMI_ACC_MAX_RETRY_I2C_XFER <= retry) {
		PERR("I2C xfer error");
		return -EIO;
	}

	return 0;
#endif
}

static int smi130_acc_smbus_write_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
#if !defined SMI130_ACC_USE_BASIC_I2C_FUNC
	s32 dummy;
	int len = 1;
#ifndef SMI130_ACC_SMBUS
	u8 buffer[2];
#endif
	if (NULL == client)
		return -ENODEV;

	while (0 != len--) {
#ifdef SMI130_ACC_SMBUS
		dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
#else
		buffer[0] = reg_addr;
		buffer[1] = *data;
		dummy = i2c_master_send(client, (char *)buffer, 2);
#endif
		reg_addr++;
		data++;
		if (dummy < 0) {
			PERR("error writing i2c bus");
			return -EIO;
		}

	}
	return 0;
#else
	u8 buffer[2];
	int retry;
	int len = 1;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = buffer,
		},
	};
	while (0 != len--) {
		buffer[0] = reg_addr;
		buffer[1] = *data;
		for (retry = 0; retry < SMI_ACC_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg,
						ARRAY_SIZE(msg)) > 0) {
				break;
			} else {
				smi130_acc_delay(1);
			}
		}
		if (SMI_ACC_MAX_RETRY_I2C_XFER <= retry) {
			PERR("I2C xfer error");
			return -EIO;
		}
		reg_addr++;
		data++;
	}

	return 0;
#endif
}

static int smi130_acc_smbus_read_byte_block(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	int retry;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg_addr,
		},

		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};

	for (retry = 0; retry < SMI_ACC_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			smi130_acc_delay(1);
	}

	if (SMI_ACC_MAX_RETRY_I2C_XFER <= retry) {
		PERR("I2C xfer error");
		return -EIO;
	}
	return 0;
}

static int smi_acc_i2c_burst_read(struct i2c_client *client, u8 reg_addr,
		u8 *data, u16 len)
{
	int retry;

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg_addr,
		},

		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = len,
		 .buf = data,
		 },
	};

	for (retry = 0; retry < SMI_ACC_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			smi130_acc_delay(1);
	}

	if (SMI_ACC_MAX_RETRY_I2C_XFER <= retry) {
		PINFO("I2C xfer error");
		return -EIO;
	}

	return 0;
}

static int smi130_acc_check_chip_id(struct i2c_client *client,
					struct smi130_acc_data *data)
{
	int i = 0;
	int err = 0;
	unsigned char chip_id = 0;
	unsigned char read_count = 0;
	unsigned char smi130_acc_sensor_type_count = 0;

	smi130_acc_sensor_type_count =
		sizeof(sensor_type_map) / sizeof(struct smi130_acc_type_map_t);

	while (read_count++ < CHECK_CHIP_ID_TIME_MAX) {
		if (smi130_acc_smbus_read_byte(client, SMI_ACC2X2_CHIP_ID_REG,
							&chip_id) < 0) {
			PERR("Bosch Sensortec Device not found\n\n"
			"i2c bus read error, read chip_id:%d\n", chip_id);
			continue;
		} else {
		for (i = 0; i < smi130_acc_sensor_type_count; i++) {
			if (sensor_type_map[i].chip_id == chip_id) {
				data->sensor_type =
					sensor_type_map[i].sensor_type;
				data->chip_id = chip_id;
				PINFO("Bosch Sensortec Device detected,\n\n"
					" HW IC name: %s\n",
						sensor_type_map[i].sensor_name);
					return err;
			}
		}
		if (i < smi130_acc_sensor_type_count)
			return err;
		else {
			if (read_count == CHECK_CHIP_ID_TIME_MAX) {
				PERR("Failed! Bosch Sensortec Device\n\n"
					" not found, mismatch chip_id:%d\n",
								chip_id);
					err = -ENODEV;
					return err;
			}
		}
		smi130_acc_delay(1);
		}
	}
	return err;
}

#ifdef CONFIG_SMI_ACC_ENABLE_NEWDATA_INT
static int smi130_acc_set_newdata(struct i2c_client *client,
			unsigned char channel, unsigned char int_newdata)
{

	unsigned char data = 0;
	int comres = 0;

	switch (channel) {
	case SMI_ACC2X2_INT1_NDATA:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_EN_INT1_PAD_NEWDATA__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data,
				SMI_ACC2X2_EN_INT1_PAD_NEWDATA, int_newdata);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_EN_INT1_PAD_NEWDATA__REG, &data);
		break;
	case SMI_ACC2X2_INT2_NDATA:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_EN_INT2_PAD_NEWDATA__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data,
				SMI_ACC2X2_EN_INT2_PAD_NEWDATA, int_newdata);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_EN_INT2_PAD_NEWDATA__REG, &data);
		break;
	default:
		comres = -1;
		break;
	}

	return comres;

}
#endif /* CONFIG_SMI_ACC_ENABLE_NEWDATA_INT */

#ifdef SMI_ACC2X2_ENABLE_INT1
static int smi130_acc_set_int1_pad_sel(struct i2c_client *client, unsigned char
		int1sel)
{
	int comres = 0;
	unsigned char data = 0;
	unsigned char state;
	state = 0x01;


	switch (int1sel) {
	case 0:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_EN_INT1_PAD_LOWG__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_EN_INT1_PAD_LOWG,
				state);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_EN_INT1_PAD_LOWG__REG, &data);
		break;
	case 1:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_EN_INT1_PAD_HIGHG__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_EN_INT1_PAD_HIGHG,
				state);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_EN_INT1_PAD_HIGHG__REG, &data);
		break;
	case 2:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_EN_INT1_PAD_SLOPE__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_EN_INT1_PAD_SLOPE,
				state);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_EN_INT1_PAD_SLOPE__REG, &data);
		break;
	case 3:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_EN_INT1_PAD_DB_TAP__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_EN_INT1_PAD_DB_TAP,
				state);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_EN_INT1_PAD_DB_TAP__REG, &data);
		break;
	case 4:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_EN_INT1_PAD_SNG_TAP__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_EN_INT1_PAD_SNG_TAP,
				state);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_EN_INT1_PAD_SNG_TAP__REG, &data);
		break;
	case 5:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_EN_INT1_PAD_ORIENT__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_EN_INT1_PAD_ORIENT,
				state);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_EN_INT1_PAD_ORIENT__REG, &data);
		break;
	case 6:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_EN_INT1_PAD_FLAT__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_EN_INT1_PAD_FLAT,
				state);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_EN_INT1_PAD_FLAT__REG, &data);
		break;
	case 7:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_EN_INT1_PAD_SLO_NO_MOT__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_EN_INT1_PAD_SLO_NO_MOT,
				state);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_EN_INT1_PAD_SLO_NO_MOT__REG, &data);
		break;

	default:
		break;
	}

	return comres;
}
#endif /* SMI_ACC2X2_ENABLE_INT1 */

#ifdef SMI_ACC2X2_ENABLE_INT2
static int smi130_acc_set_int2_pad_sel(struct i2c_client *client, unsigned char
		int2sel)
{
	int comres = 0;
	unsigned char data = 0;
	unsigned char state;
	state = 0x01;


	switch (int2sel) {
	case 0:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_EN_INT2_PAD_LOWG__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_EN_INT2_PAD_LOWG,
				state);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_EN_INT2_PAD_LOWG__REG, &data);
		break;
	case 1:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_EN_INT2_PAD_HIGHG__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_EN_INT2_PAD_HIGHG,
				state);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_EN_INT2_PAD_HIGHG__REG, &data);
		break;
	case 2:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_EN_INT2_PAD_SLOPE__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_EN_INT2_PAD_SLOPE,
				state);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_EN_INT2_PAD_SLOPE__REG, &data);
		break;
	case 3:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_EN_INT2_PAD_DB_TAP__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_EN_INT2_PAD_DB_TAP,
				state);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_EN_INT2_PAD_DB_TAP__REG, &data);
		break;
	case 4:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_EN_INT2_PAD_SNG_TAP__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_EN_INT2_PAD_SNG_TAP,
				state);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_EN_INT2_PAD_SNG_TAP__REG, &data);
		break;
	case 5:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_EN_INT2_PAD_ORIENT__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_EN_INT2_PAD_ORIENT,
				state);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_EN_INT2_PAD_ORIENT__REG, &data);
		break;
	case 6:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_EN_INT2_PAD_FLAT__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_EN_INT2_PAD_FLAT,
				state);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_EN_INT2_PAD_FLAT__REG, &data);
		break;
	case 7:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_EN_INT2_PAD_SLO_NO_MOT__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_EN_INT2_PAD_SLO_NO_MOT,
				state);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_EN_INT2_PAD_SLO_NO_MOT__REG, &data);
		break;
	default:
		break;
	}

	return comres;
}
#endif /* SMI_ACC2X2_ENABLE_INT2 */

static int smi130_acc_set_Int_Enable(struct i2c_client *client, unsigned char
		InterruptType , unsigned char value)
{
	int comres = 0;
	unsigned char data1 = 0;
	unsigned char data2 = 0;

	if ((11 < InterruptType) && (InterruptType < 16)) {
		switch (InterruptType) {
		case 12:
			/* slow/no motion X Interrupt  */
			comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_INT_SLO_NO_MOT_EN_X_INT__REG, &data1);
			data1 = SMI_ACC2X2_SET_BITSLICE(data1,
				SMI_ACC2X2_INT_SLO_NO_MOT_EN_X_INT, value);
			comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_INT_SLO_NO_MOT_EN_X_INT__REG, &data1);
			break;
		case 13:
			/* slow/no motion Y Interrupt  */
			comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_INT_SLO_NO_MOT_EN_Y_INT__REG, &data1);
			data1 = SMI_ACC2X2_SET_BITSLICE(data1,
				SMI_ACC2X2_INT_SLO_NO_MOT_EN_Y_INT, value);
			comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_INT_SLO_NO_MOT_EN_Y_INT__REG, &data1);
			break;
		case 14:
			/* slow/no motion Z Interrupt  */
			comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_INT_SLO_NO_MOT_EN_Z_INT__REG, &data1);
			data1 = SMI_ACC2X2_SET_BITSLICE(data1,
				SMI_ACC2X2_INT_SLO_NO_MOT_EN_Z_INT, value);
			comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_INT_SLO_NO_MOT_EN_Z_INT__REG, &data1);
			break;
		case 15:
			/* slow / no motion Interrupt select */
			comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_INT_SLO_NO_MOT_EN_SEL_INT__REG, &data1);
			data1 = SMI_ACC2X2_SET_BITSLICE(data1,
				SMI_ACC2X2_INT_SLO_NO_MOT_EN_SEL_INT, value);
			comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_INT_SLO_NO_MOT_EN_SEL_INT__REG, &data1);
		}

	return comres;
	}


	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_INT_ENABLE1_REG, &data1);
	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_INT_ENABLE2_REG, &data2);

	value = value & 1;
	switch (InterruptType) {
	case 0:
		/* Low G Interrupt  */
		data2 = SMI_ACC2X2_SET_BITSLICE(data2, SMI_ACC2X2_EN_LOWG_INT, value);
		break;

	case 1:
		/* High G X Interrupt */
		data2 = SMI_ACC2X2_SET_BITSLICE(data2, SMI_ACC2X2_EN_HIGHG_X_INT,
				value);
		break;

	case 2:
		/* High G Y Interrupt */
		data2 = SMI_ACC2X2_SET_BITSLICE(data2, SMI_ACC2X2_EN_HIGHG_Y_INT,
				value);
		break;

	case 3:
		/* High G Z Interrupt */
		data2 = SMI_ACC2X2_SET_BITSLICE(data2, SMI_ACC2X2_EN_HIGHG_Z_INT,
				value);
		break;

	case 4:
		/* New Data Interrupt  */
		data2 = SMI_ACC2X2_SET_BITSLICE(data2, SMI_ACC2X2_EN_NEW_DATA_INT,
				value);
		break;

	case 5:
		/* Slope X Interrupt */
		data1 = SMI_ACC2X2_SET_BITSLICE(data1, SMI_ACC2X2_EN_SLOPE_X_INT,
				value);
		break;

	case 6:
		/* Slope Y Interrupt */
		data1 = SMI_ACC2X2_SET_BITSLICE(data1, SMI_ACC2X2_EN_SLOPE_Y_INT,
				value);
		break;

	case 7:
		/* Slope Z Interrupt */
		data1 = SMI_ACC2X2_SET_BITSLICE(data1, SMI_ACC2X2_EN_SLOPE_Z_INT,
				value);
		break;

	case 8:
		/* Single Tap Interrupt */
		data1 = SMI_ACC2X2_SET_BITSLICE(data1, SMI_ACC2X2_EN_SINGLE_TAP_INT,
				value);
		break;

	case 9:
		/* Double Tap Interrupt */
		data1 = SMI_ACC2X2_SET_BITSLICE(data1, SMI_ACC2X2_EN_DOUBLE_TAP_INT,
				value);
		break;

	case 10:
		/* Orient Interrupt  */
		data1 = SMI_ACC2X2_SET_BITSLICE(data1, SMI_ACC2X2_EN_ORIENT_INT, value);
		break;

	case 11:
		/* Flat Interrupt */
		data1 = SMI_ACC2X2_SET_BITSLICE(data1, SMI_ACC2X2_EN_FLAT_INT, value);
		break;

	default:
		break;
	}
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_INT_ENABLE1_REG,
			&data1);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_INT_ENABLE2_REG,
			&data2);

	return comres;
}


#if defined(SMI_ACC2X2_ENABLE_INT1) || defined(SMI_ACC2X2_ENABLE_INT2)
static int smi130_acc_get_interruptstatus1(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_STATUS1_REG, &data);
	*intstatus = data;

	return comres;
}

#ifdef CONFIG_SMI_ACC_ENABLE_NEWDATA_INT
/*
static int smi130_acc_get_interruptstatus2(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_STATUS2_REG, &data);
	*intstatus = data;

	return comres;
}
*/
#endif

static int smi130_acc_get_HIGH_first(struct i2c_client *client, unsigned char
						param, unsigned char *intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	switch (param) {
	case 0:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_STATUS_ORIENT_HIGH_REG, &data);
		data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_HIGHG_FIRST_X);
		*intstatus = data;
		break;
	case 1:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_STATUS_ORIENT_HIGH_REG, &data);
		data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_HIGHG_FIRST_Y);
		*intstatus = data;
		break;
	case 2:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_STATUS_ORIENT_HIGH_REG, &data);
		data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_HIGHG_FIRST_Z);
		*intstatus = data;
		break;
	default:
		break;
	}

	return comres;
}

static int smi130_acc_get_HIGH_sign(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_STATUS_ORIENT_HIGH_REG,
			&data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_HIGHG_SIGN_S);
	*intstatus = data;

	return comres;
}

#ifndef CONFIG_SIG_MOTION
static int smi130_acc_get_slope_first(struct i2c_client *client, unsigned char
	param, unsigned char *intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	switch (param) {
	case 0:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_STATUS_TAP_SLOPE_REG, &data);
		data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_SLOPE_FIRST_X);
		*intstatus = data;
		break;
	case 1:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_STATUS_TAP_SLOPE_REG, &data);
		data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_SLOPE_FIRST_Y);
		*intstatus = data;
		break;
	case 2:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_STATUS_TAP_SLOPE_REG, &data);
		data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_SLOPE_FIRST_Z);
		*intstatus = data;
		break;
	default:
		break;
	}

	return comres;
}

static int smi130_acc_get_slope_sign(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_STATUS_TAP_SLOPE_REG,
			&data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_SLOPE_SIGN_S);
	*intstatus = data;

	return comres;
}
#endif

static int smi130_acc_get_orient_mbl_status(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_STATUS_ORIENT_HIGH_REG,
			&data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_ORIENT_S);
	*intstatus = data;

	return comres;
}

static int smi130_acc_get_orient_mbl_flat_status(struct i2c_client *client, unsigned
		char *intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_STATUS_ORIENT_HIGH_REG,
			&data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_FLAT_S);
	*intstatus = data;

	return comres;
}
#endif /* defined(SMI_ACC2X2_ENABLE_INT1)||defined(SMI_ACC2X2_ENABLE_INT2) */

static int smi130_acc_set_Int_Mode(struct i2c_client *client, unsigned char Mode)
{
	int comres = 0;
	unsigned char data = 0;


	comres = smi130_acc_smbus_read_byte(client,
			SMI_ACC2X2_INT_MODE_SEL__REG, &data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_INT_MODE_SEL, Mode);
	comres = smi130_acc_smbus_write_byte(client,
			SMI_ACC2X2_INT_MODE_SEL__REG, &data);


	return comres;
}

static int smi130_acc_get_Int_Mode(struct i2c_client *client, unsigned char *Mode)
{
	int comres = 0;
	unsigned char data = 0;


	comres = smi130_acc_smbus_read_byte(client,
			SMI_ACC2X2_INT_MODE_SEL__REG, &data);
	data  = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_INT_MODE_SEL);
	*Mode = data;


	return comres;
}
static int smi130_acc_set_slope_duration(struct i2c_client *client, unsigned char
		duration)
{
	int comres = 0;
	unsigned char data = 0;


	comres = smi130_acc_smbus_read_byte(client,
			SMI_ACC2X2_SLOPE_DUR__REG, &data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_SLOPE_DUR, duration);
	comres = smi130_acc_smbus_write_byte(client,
			SMI_ACC2X2_SLOPE_DUR__REG, &data);

	return comres;
}

static int smi130_acc_get_slope_duration(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;


	comres = smi130_acc_smbus_read_byte(client,
			SMI_ACC2X2_SLOPE_DURN_REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_SLOPE_DUR);
	*status = data;


	return comres;
}

static int smi130_acc_set_slope_no_mot_duration(struct i2c_client *client,
			unsigned char duration)
{
	int comres = 0;
	unsigned char data = 0;


	comres = smi130_acc_smbus_read_byte(client,
			SMI130_ACC_SLO_NO_MOT_DUR__REG, &data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI130_ACC_SLO_NO_MOT_DUR, duration);
	comres = smi130_acc_smbus_write_byte(client,
			SMI130_ACC_SLO_NO_MOT_DUR__REG, &data);


	return comres;
}

static int smi130_acc_get_slope_no_mot_duration(struct i2c_client *client,
			unsigned char *status)
{
	int comres = 0;
	unsigned char data = 0;


	comres = smi130_acc_smbus_read_byte(client,
			SMI130_ACC_SLO_NO_MOT_DUR__REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI130_ACC_SLO_NO_MOT_DUR);
	*status = data;


	return comres;
}

static int smi130_acc_set_slope_threshold(struct i2c_client *client,
		unsigned char threshold)
{
	int comres = 0;
	unsigned char data = 0;

	data = threshold;
	comres = smi130_acc_smbus_write_byte(client,
			SMI_ACC2X2_SLOPE_THRES__REG, &data);

	return comres;
}

static int smi130_acc_get_slope_threshold(struct i2c_client *client,
		unsigned char *status)
{
	int comres = 0;
	unsigned char data = 0;


	comres = smi130_acc_smbus_read_byte(client,
			SMI_ACC2X2_SLOPE_THRES_REG, &data);
	*status = data;

	return comres;
}

static int smi130_acc_set_slope_no_mot_threshold(struct i2c_client *client,
		unsigned char threshold)
{
	int comres = 0;
	unsigned char data = 0;

	data = threshold;
	comres = smi130_acc_smbus_write_byte(client,
			SMI_ACC2X2_SLO_NO_MOT_THRES_REG, &data);

	return comres;
}

static int smi130_acc_get_slope_no_mot_threshold(struct i2c_client *client,
		unsigned char *status)
{
	int comres = 0;
	unsigned char data = 0;


	comres = smi130_acc_smbus_read_byte(client,
			SMI_ACC2X2_SLO_NO_MOT_THRES_REG, &data);
	*status = data;

	return comres;
}


static int smi130_acc_set_low_g_duration(struct i2c_client *client, unsigned char
		duration)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_LOWG_DUR__REG, &data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_LOWG_DUR, duration);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_LOWG_DUR__REG, &data);

	return comres;
}

static int smi130_acc_get_low_g_duration(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_LOW_DURN_REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_LOWG_DUR);
	*status = data;

	return comres;
}

static int smi130_acc_set_low_g_threshold(struct i2c_client *client, unsigned char
		threshold)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_LOWG_THRES__REG, &data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_LOWG_THRES, threshold);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_LOWG_THRES__REG, &data);

	return comres;
}

static int smi130_acc_get_low_g_threshold(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_LOW_THRES_REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_LOWG_THRES);
	*status = data;

	return comres;
}

static int smi130_acc_set_high_g_duration(struct i2c_client *client, unsigned char
		duration)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_HIGHG_DUR__REG, &data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_HIGHG_DUR, duration);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_HIGHG_DUR__REG, &data);

	return comres;
}

static int smi130_acc_get_high_g_duration(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_HIGH_DURN_REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_HIGHG_DUR);
	*status = data;

	return comres;
}

static int smi130_acc_set_high_g_threshold(struct i2c_client *client, unsigned char
		threshold)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_HIGHG_THRES__REG, &data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_HIGHG_THRES, threshold);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_HIGHG_THRES__REG,
			&data);

	return comres;
}

static int smi130_acc_get_high_g_threshold(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_HIGH_THRES_REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_HIGHG_THRES);
	*status = data;

	return comres;
}


static int smi130_acc_set_tap_duration(struct i2c_client *client, unsigned char
		duration)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_TAP_DUR__REG, &data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_TAP_DUR, duration);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_TAP_DUR__REG, &data);

	return comres;
}

static int smi130_acc_get_tap_duration(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_TAP_PARAM_REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_TAP_DUR);
	*status = data;

	return comres;
}

static int smi130_acc_set_tap_shock(struct i2c_client *client, unsigned char setval)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_TAP_SHOCK_DURN__REG,
			&data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_TAP_SHOCK_DURN, setval);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_TAP_SHOCK_DURN__REG,
			&data);

	return comres;
}

static int smi130_acc_get_tap_shock(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_TAP_PARAM_REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_TAP_SHOCK_DURN);
	*status = data;

	return comres;
}

static int smi130_acc_set_tap_quiet(struct i2c_client *client, unsigned char
		duration)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_TAP_QUIET_DURN__REG,
			&data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_TAP_QUIET_DURN, duration);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_TAP_QUIET_DURN__REG,
			&data);

	return comres;
}

static int smi130_acc_get_tap_quiet(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_TAP_PARAM_REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_TAP_QUIET_DURN);
	*status = data;

	return comres;
}

static int smi130_acc_set_tap_threshold(struct i2c_client *client, unsigned char
		threshold)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_TAP_THRES__REG, &data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_TAP_THRES, threshold);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_TAP_THRES__REG, &data);

	return comres;
}

static int smi130_acc_get_tap_threshold(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_TAP_THRES_REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_TAP_THRES);
	*status = data;

	return comres;
}

static int smi130_acc_set_tap_samp(struct i2c_client *client, unsigned char samp)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_TAP_SAMPLES__REG, &data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_TAP_SAMPLES, samp);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_TAP_SAMPLES__REG,
			&data);

	return comres;
}

static int smi130_acc_get_tap_samp(struct i2c_client *client, unsigned char *status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_TAP_THRES_REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_TAP_SAMPLES);
	*status = data;

	return comres;
}

static int smi130_acc_set_orient_mbl_mode(struct i2c_client *client, unsigned char mode)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_ORIENT_MODE__REG, &data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_ORIENT_MODE, mode);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_ORIENT_MODE__REG,
			&data);

	return comres;
}

static int smi130_acc_get_orient_mbl_mode(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_ORIENT_PARAM_REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_ORIENT_MODE);
	*status = data;

	return comres;
}

static int smi130_acc_set_orient_mbl_blocking(struct i2c_client *client, unsigned char
		samp)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_ORIENT_BLOCK__REG,
			&data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_ORIENT_BLOCK, samp);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_ORIENT_BLOCK__REG,
			&data);

	return comres;
}

static int smi130_acc_get_orient_mbl_blocking(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_ORIENT_PARAM_REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_ORIENT_BLOCK);
	*status = data;

	return comres;
}

static int smi130_acc_set_orient_mbl_hyst(struct i2c_client *client, unsigned char
		orient_mblhyst)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_ORIENT_HYST__REG, &data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_ORIENT_HYST, orient_mblhyst);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_ORIENT_HYST__REG,
			&data);

	return comres;
}

static int smi130_acc_get_orient_mbl_hyst(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_ORIENT_PARAM_REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_ORIENT_HYST);
	*status = data;

	return comres;
}
static int smi130_acc_set_theta_blocking(struct i2c_client *client, unsigned char
		thetablk)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_THETA_BLOCK__REG, &data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_THETA_BLOCK, thetablk);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_THETA_BLOCK__REG,
			&data);

	return comres;
}

static int smi130_acc_get_theta_blocking(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_THETA_BLOCK_REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_THETA_BLOCK);
	*status = data;

	return comres;
}

static int smi130_acc_set_theta_flat(struct i2c_client *client, unsigned char
		thetaflat)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_THETA_FLAT__REG, &data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_THETA_FLAT, thetaflat);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_THETA_FLAT__REG, &data);

	return comres;
}

static int smi130_acc_get_theta_flat(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_THETA_FLAT_REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_THETA_FLAT);
	*status = data;

	return comres;
}

static int smi130_acc_set_flat_hold_time(struct i2c_client *client, unsigned char
		holdtime)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_FLAT_HOLD_TIME__REG,
			&data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_FLAT_HOLD_TIME, holdtime);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_FLAT_HOLD_TIME__REG,
			&data);

	return comres;
}

static int smi130_acc_get_flat_hold_time(struct i2c_client *client, unsigned char
		*holdtime)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_FLAT_HOLD_TIME_REG,
			&data);
	data  = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_FLAT_HOLD_TIME);
	*holdtime = data;

	return comres;
}

/*!
 * brief: smi130_acc switch from normal to suspend mode
 * @param[i] smi130_acc
 * @param[i] data1, write to PMU_LPW
 * @param[i] data2, write to PMU_LOW_NOSIE
 *
 * @return zero success, none-zero failed
 */
static int smi130_acc_normal_to_suspend(struct smi130_acc_data *smi130_acc,
				unsigned char data1, unsigned char data2)
{
	unsigned char current_fifo_mode;
	unsigned char current_op_mode;
	if (smi130_acc == NULL)
		return -ENODEV;
	/* get current op mode from mode register */
	if (smi130_acc_get_mode(smi130_acc->smi130_acc_client, &current_op_mode) < 0)
		return -EIO;
	/* only aimed at operatiom mode chang from normal/lpw1 mode
	 * to suspend state.
	*/
	if (current_op_mode == SMI_ACC2X2_MODE_NORMAL ||
			current_op_mode == SMI_ACC2X2_MODE_LOWPOWER1) {
		/* get current fifo mode from fifo config register */
		if (smi130_acc_get_fifo_mode(smi130_acc->smi130_acc_client,
							&current_fifo_mode) < 0)
			return -EIO;
		else {
			smi130_acc_smbus_write_byte(smi130_acc->smi130_acc_client,
					SMI_ACC2X2_LOW_NOISE_CTRL_REG, &data2);
			smi130_acc_smbus_write_byte(smi130_acc->smi130_acc_client,
					SMI_ACC2X2_MODE_CTRL_REG, &data1);
			/*! Aim at fifo workarounds with FIFO_CONFIG_1 */
			current_fifo_mode |= FIFO_WORKAROUNDS_MSK;
			smi130_acc_smbus_write_byte(smi130_acc->smi130_acc_client,
				SMI_ACC2X2_FIFO_MODE__REG, &current_fifo_mode);
			smi130_acc_delay(3);
			return 0;
		}
	} else {
		smi130_acc_smbus_write_byte(smi130_acc->smi130_acc_client,
					SMI_ACC2X2_LOW_NOISE_CTRL_REG, &data2);
		smi130_acc_smbus_write_byte(smi130_acc->smi130_acc_client,
					SMI_ACC2X2_MODE_CTRL_REG, &data1);
		smi130_acc_delay(3);
		return 0;
	}

}

static int smi130_acc_set_mode(struct i2c_client *client, unsigned char mode,
						unsigned char enabled_mode)
{
	int comres = 0;
	unsigned char data1 = 0;
	unsigned char data2 = 0;
	int ret = 0;
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	mutex_lock(&smi130_acc->mode_mutex);
	if (SMI_ACC2X2_MODE_SUSPEND == mode) {
		if (enabled_mode != SMI_ACC_ENABLED_ALL) {
			if ((smi130_acc->smi_acc_mode_enabled &
						(1<<enabled_mode)) == 0) {
				/* sensor is already closed in this mode */
				mutex_unlock(&smi130_acc->mode_mutex);
				return 0;
			} else {
				smi130_acc->smi_acc_mode_enabled &= ~(1<<enabled_mode);
			}
		} else {
			/* shut down, close all and force do it*/
			smi130_acc->smi_acc_mode_enabled = 0;
		}
	} else if (SMI_ACC2X2_MODE_NORMAL == mode) {
		if ((smi130_acc->smi_acc_mode_enabled & (1<<enabled_mode)) != 0) {
			/* sensor is already enabled in this mode */
			mutex_unlock(&smi130_acc->mode_mutex);
			return 0;
		} else {
			smi130_acc->smi_acc_mode_enabled |= (1<<enabled_mode);
		}
	} else {
		/* other mode, close all and force do it*/
		smi130_acc->smi_acc_mode_enabled = 0;
	}
	mutex_unlock(&smi130_acc->mode_mutex);

	if (mode < 6) {
		comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_MODE_CTRL_REG,
				&data1);
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_LOW_NOISE_CTRL_REG,
				&data2);
		switch (mode) {
		case SMI_ACC2X2_MODE_NORMAL:
				data1  = SMI_ACC2X2_SET_BITSLICE(data1,
						SMI_ACC2X2_MODE_CTRL, 0);
				data2  = SMI_ACC2X2_SET_BITSLICE(data2,
						SMI_ACC2X2_LOW_POWER_MODE, 0);
				smi130_acc_smbus_write_byte(client,
						SMI_ACC2X2_MODE_CTRL_REG, &data1);
				smi130_acc_delay(3);
				smi130_acc_smbus_write_byte(client,
					SMI_ACC2X2_LOW_NOISE_CTRL_REG, &data2);
				break;
		case SMI_ACC2X2_MODE_LOWPOWER1:
				data1  = SMI_ACC2X2_SET_BITSLICE(data1,
						SMI_ACC2X2_MODE_CTRL, 2);
				data2  = SMI_ACC2X2_SET_BITSLICE(data2,
						SMI_ACC2X2_LOW_POWER_MODE, 0);
				smi130_acc_smbus_write_byte(client,
						SMI_ACC2X2_MODE_CTRL_REG, &data1);
				smi130_acc_delay(3);
				smi130_acc_smbus_write_byte(client,
					SMI_ACC2X2_LOW_NOISE_CTRL_REG, &data2);
				break;
		case SMI_ACC2X2_MODE_SUSPEND:
			if (smi130_acc->smi_acc_mode_enabled != 0) {
				PERR("smi_acc still working");
				return 0;
			}
			data1  = SMI_ACC2X2_SET_BITSLICE(data1,
					SMI_ACC2X2_MODE_CTRL, 4);
			data2  = SMI_ACC2X2_SET_BITSLICE(data2,
					SMI_ACC2X2_LOW_POWER_MODE, 0);
			/*aimed at anomaly resolution when switch to suspend*/
			ret = smi130_acc_normal_to_suspend(smi130_acc, data1, data2);
			if (ret < 0)
				PERR("Error switching to suspend");
			break;
		case SMI_ACC2X2_MODE_DEEP_SUSPEND:
			if (smi130_acc->smi_acc_mode_enabled != 0) {
				PERR("smi_acc still working");
				return 0;
			}
			data1  = SMI_ACC2X2_SET_BITSLICE(data1,
				SMI_ACC2X2_MODE_CTRL, 1);
			data2  = SMI_ACC2X2_SET_BITSLICE(data2,
				SMI_ACC2X2_LOW_POWER_MODE, 1);
			smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_MODE_CTRL_REG, &data1);
			smi130_acc_delay(3);
			smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_LOW_NOISE_CTRL_REG, &data2);
			break;
		case SMI_ACC2X2_MODE_LOWPOWER2:
				data1  = SMI_ACC2X2_SET_BITSLICE(data1,
						SMI_ACC2X2_MODE_CTRL, 2);
				data2  = SMI_ACC2X2_SET_BITSLICE(data2,
						SMI_ACC2X2_LOW_POWER_MODE, 1);
				smi130_acc_smbus_write_byte(client,
						SMI_ACC2X2_MODE_CTRL_REG, &data1);
				smi130_acc_delay(3);
				smi130_acc_smbus_write_byte(client,
					SMI_ACC2X2_LOW_NOISE_CTRL_REG, &data2);
				break;
		case SMI_ACC2X2_MODE_STANDBY:
				data1  = SMI_ACC2X2_SET_BITSLICE(data1,
						SMI_ACC2X2_MODE_CTRL, 4);
				data2  = SMI_ACC2X2_SET_BITSLICE(data2,
						SMI_ACC2X2_LOW_POWER_MODE, 1);
				smi130_acc_smbus_write_byte(client,
					SMI_ACC2X2_LOW_NOISE_CTRL_REG, &data2);
				smi130_acc_delay(3);
				smi130_acc_smbus_write_byte(client,
						SMI_ACC2X2_MODE_CTRL_REG, &data1);
				break;
		}
	} else {
		comres = -1;
	}

	return comres;
}


static int smi130_acc_get_mode(struct i2c_client *client, unsigned char *mode)
{
	int comres = 0;
	unsigned char data1 = 0;
	unsigned char data2 = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_MODE_CTRL_REG, &data1);
	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_LOW_NOISE_CTRL_REG,
			&data2);

	data1  = (data1 & 0xE0) >> 5;
	data2  = (data2 & 0x40) >> 6;


	if ((data1 == 0x00) && (data2 == 0x00)) {
		*mode  = SMI_ACC2X2_MODE_NORMAL;
	} else {
		if ((data1 == 0x02) && (data2 == 0x00)) {
			*mode  = SMI_ACC2X2_MODE_LOWPOWER1;
		} else {
			if ((data1 == 0x04 || data1 == 0x06) &&
						(data2 == 0x00)) {
				*mode  = SMI_ACC2X2_MODE_SUSPEND;
			} else {
				if (((data1 & 0x01) == 0x01)) {
					*mode  = SMI_ACC2X2_MODE_DEEP_SUSPEND;
				} else {
					if ((data1 == 0x02) &&
							(data2 == 0x01)) {
						*mode  = SMI_ACC2X2_MODE_LOWPOWER2;
					} else {
					if ((data1 == 0x04) && (data2 ==
									0x01)) {
							*mode  =
							SMI_ACC2X2_MODE_STANDBY;
					} else {
							*mode =
						SMI_ACC2X2_MODE_DEEP_SUSPEND;
						}
					}
				}
			}
		}
	}

	return comres;
}

static int smi130_acc_set_range(struct i2c_client *client, unsigned char Range)
{
	int comres = 0;
	unsigned char data1 = 0;

	if ((Range == 3) || (Range == 5) || (Range == 8) || (Range == 12)) {
		comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_RANGE_SEL_REG,
				&data1);
		switch (Range) {
		case SMI_ACC2X2_RANGE_2G:
			data1  = SMI_ACC2X2_SET_BITSLICE(data1,
					SMI_ACC2X2_RANGE_SEL, 3);
			break;
		case SMI_ACC2X2_RANGE_4G:
			data1  = SMI_ACC2X2_SET_BITSLICE(data1,
					SMI_ACC2X2_RANGE_SEL, 5);
			break;
		case SMI_ACC2X2_RANGE_8G:
			data1  = SMI_ACC2X2_SET_BITSLICE(data1,
					SMI_ACC2X2_RANGE_SEL, 8);
			break;
		case SMI_ACC2X2_RANGE_16G:
			data1  = SMI_ACC2X2_SET_BITSLICE(data1,
					SMI_ACC2X2_RANGE_SEL, 12);
			break;
		default:
			break;
		}
		comres += smi130_acc_smbus_write_byte(client, SMI_ACC2X2_RANGE_SEL_REG,
				&data1);
	} else {
		comres = -1;
	}

	return comres;
}

static int smi130_acc_get_range(struct i2c_client *client, unsigned char *Range)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_RANGE_SEL__REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_RANGE_SEL);
	*Range = data;

	return comres;
}


static int smi130_acc_set_bandwidth(struct i2c_client *client, unsigned char BW)
{
	int comres = 0;
	unsigned char data = 0;
	int Bandwidth = 0;
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (BW > 7 && BW < 16) {
		switch (BW) {
		case SMI_ACC2X2_BW_7_81HZ:
			Bandwidth = SMI_ACC2X2_BW_7_81HZ;
			smi130_acc->time_odr = 64000000;

			/*  7.81 Hz      64000 uS   */
			break;
		case SMI_ACC2X2_BW_15_63HZ:
			Bandwidth = SMI_ACC2X2_BW_15_63HZ;
			smi130_acc->time_odr = 32000000;
			/*  15.63 Hz     32000 uS   */
			break;
		case SMI_ACC2X2_BW_31_25HZ:
			Bandwidth = SMI_ACC2X2_BW_31_25HZ;
			smi130_acc->time_odr = 16000000;
			/*  31.25 Hz     16000 uS   */
			break;
		case SMI_ACC2X2_BW_62_50HZ:
			Bandwidth = SMI_ACC2X2_BW_62_50HZ;
			smi130_acc->time_odr = 8000000;
			/*  62.50 Hz     8000 uS   */
			break;
		case SMI_ACC2X2_BW_125HZ:
			Bandwidth = SMI_ACC2X2_BW_125HZ;
			smi130_acc->time_odr = 4000000;
			/*  125 Hz       4000 uS   */
			break;
		case SMI_ACC2X2_BW_250HZ:
			Bandwidth = SMI_ACC2X2_BW_250HZ;
			smi130_acc->time_odr = 2000000;
			/*  250 Hz       2000 uS   */
			break;
		case SMI_ACC2X2_BW_500HZ:
			Bandwidth = SMI_ACC2X2_BW_500HZ;
			smi130_acc->time_odr = 1000000;
			/*  500 Hz       1000 uS   */
			break;
		case SMI_ACC2X2_BW_1000HZ:
			Bandwidth = SMI_ACC2X2_BW_1000HZ;
			smi130_acc->time_odr = 500000;
			/*  1000 Hz      500 uS   */
			break;
		default:
			break;
		}
		comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_BANDWIDTH__REG,
				&data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_BANDWIDTH, Bandwidth);
		comres += smi130_acc_smbus_write_byte(client, SMI_ACC2X2_BANDWIDTH__REG,
				&data);
	} else {
		comres = -1;
	}

	return comres;
}

static int smi130_acc_get_bandwidth(struct i2c_client *client, unsigned char *BW)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_BANDWIDTH__REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_BANDWIDTH);
	*BW = data;

	return comres;
}

int smi130_acc_get_sleep_duration(struct i2c_client *client, unsigned char
		*sleep_dur)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client,
			SMI_ACC2X2_SLEEP_DUR__REG, &data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_SLEEP_DUR);
	*sleep_dur = data;

	return comres;
}

int smi130_acc_set_sleep_duration(struct i2c_client *client, unsigned char
		sleep_dur)
{
	int comres = 0;
	unsigned char data = 0;
	int sleep_duration = 0;

	if (sleep_dur > 4 && sleep_dur < 16) {
		switch (sleep_dur) {
		case SMI_ACC2X2_SLEEP_DUR_0_5MS:
			sleep_duration = SMI_ACC2X2_SLEEP_DUR_0_5MS;

			/*  0.5 MS   */
			break;
		case SMI_ACC2X2_SLEEP_DUR_1MS:
			sleep_duration = SMI_ACC2X2_SLEEP_DUR_1MS;

			/*  1 MS  */
			break;
		case SMI_ACC2X2_SLEEP_DUR_2MS:
			sleep_duration = SMI_ACC2X2_SLEEP_DUR_2MS;

			/*  2 MS  */
			break;
		case SMI_ACC2X2_SLEEP_DUR_4MS:
			sleep_duration = SMI_ACC2X2_SLEEP_DUR_4MS;

			/*  4 MS   */
			break;
		case SMI_ACC2X2_SLEEP_DUR_6MS:
			sleep_duration = SMI_ACC2X2_SLEEP_DUR_6MS;

			/*  6 MS  */
			break;
		case SMI_ACC2X2_SLEEP_DUR_10MS:
			sleep_duration = SMI_ACC2X2_SLEEP_DUR_10MS;

			/*  10 MS  */
			break;
		case SMI_ACC2X2_SLEEP_DUR_25MS:
			sleep_duration = SMI_ACC2X2_SLEEP_DUR_25MS;

			/*  25 MS  */
			break;
		case SMI_ACC2X2_SLEEP_DUR_50MS:
			sleep_duration = SMI_ACC2X2_SLEEP_DUR_50MS;

			/*  50 MS   */
			break;
		case SMI_ACC2X2_SLEEP_DUR_100MS:
			sleep_duration = SMI_ACC2X2_SLEEP_DUR_100MS;

			/*  100 MS  */
			break;
		case SMI_ACC2X2_SLEEP_DUR_500MS:
			sleep_duration = SMI_ACC2X2_SLEEP_DUR_500MS;

			/*  500 MS   */
			break;
		case SMI_ACC2X2_SLEEP_DUR_1S:
			sleep_duration = SMI_ACC2X2_SLEEP_DUR_1S;

			/*  1 SECS   */
			break;
		default:
			break;
		}
		comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_SLEEP_DUR__REG,
				&data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_SLEEP_DUR,
				sleep_duration);
		comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_SLEEP_DUR__REG,
				&data);
	} else {
		comres = -1;
	}


	return comres;
}

static int smi130_acc_get_fifo_mode(struct i2c_client *client, unsigned char
		*fifo_mode)
{
	int comres;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_FIFO_MODE__REG, &data);
	*fifo_mode = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_FIFO_MODE);

	return comres;
}

static int smi130_acc_set_fifo_mode(struct i2c_client *client, unsigned char
		fifo_mode)
{
	unsigned char data = 0;
	int comres = 0;

	if (fifo_mode < 4) {
		comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_FIFO_MODE__REG,
				&data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_FIFO_MODE, fifo_mode);
		/*! Aim at fifo workarounds with FIFO_CONFIG_1 */
		data |= FIFO_WORKAROUNDS_MSK;
		comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_FIFO_MODE__REG,
				&data);
	} else {
		comres = -1;
	}

	return comres;
}

static int smi130_acc_get_fifo_trig(struct i2c_client *client, unsigned char
		*fifo_trig)
{
	int comres;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client,
			SMI_ACC2X2_FIFO_TRIGGER_ACTION__REG, &data);
	*fifo_trig = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_FIFO_TRIGGER_ACTION);

	return comres;
}

static int smi130_acc_set_fifo_trig(struct i2c_client *client, unsigned char
		fifo_trig)
{
	unsigned char data = 0;
	int comres = 0;

	if (fifo_trig < 4) {
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_FIFO_TRIGGER_ACTION__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_FIFO_TRIGGER_ACTION,
				fifo_trig);
		/*! Aim at fifo workarounds with FIFO_CONFIG_1 */
		data |= FIFO_WORKAROUNDS_MSK;
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_FIFO_TRIGGER_ACTION__REG, &data);
	} else {
		comres = -1;
	}

	return comres;
}

static int smi130_acc_get_fifo_trig_src(struct i2c_client *client, unsigned char
		*trig_src)
{
	int comres;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client,
			SMI_ACC2X2_FIFO_TRIGGER_SOURCE__REG, &data);
	*trig_src = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_FIFO_TRIGGER_SOURCE);

	return comres;
}

static int smi130_acc_set_fifo_trig_src(struct i2c_client *client, unsigned char
		trig_src)
{
	unsigned char data = 0;
	int comres = 0;

	if (trig_src < 4) {
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_FIFO_TRIGGER_SOURCE__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_FIFO_TRIGGER_SOURCE,
				trig_src);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_FIFO_TRIGGER_SOURCE__REG, &data);
	} else {
		comres = -1;
	}

	return comres;
}

static int smi130_acc_get_fifo_framecount(struct i2c_client *client, unsigned char
			 *framecount)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client,
			SMI_ACC2X2_FIFO_FRAME_COUNTER_S__REG, &data);
	*framecount = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_FIFO_FRAME_COUNTER_S);

	return comres;
}

static int smi130_acc_get_fifo_data_sel(struct i2c_client *client, unsigned char
		*data_sel)
{
	int comres;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client,
			SMI_ACC2X2_FIFO_DATA_SELECT__REG, &data);
	*data_sel = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_FIFO_DATA_SELECT);

	return comres;
}

static int smi130_acc_set_fifo_data_sel(struct i2c_client *client, unsigned char
		data_sel)
{
	unsigned char data = 0;
	int comres = 0;

	if (data_sel < 4) {
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_FIFO_DATA_SELECT__REG,
				&data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_FIFO_DATA_SELECT,
				data_sel);
		/*! Aim at fifo workarounds with FIFO_CONFIG_1 */
		data |= FIFO_WORKAROUNDS_MSK;
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_FIFO_DATA_SELECT__REG,
				&data);
	} else {
		comres = -1;
	}

	return comres;
}


static int smi130_acc_get_offset_target(struct i2c_client *client, unsigned char
		channel, unsigned char *offset)
{
	unsigned char data = 0;
	int comres = 0;

	switch (channel) {
	case SMI_ACC2X2_CUT_OFF:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_COMP_CUTOFF__REG, &data);
		*offset = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_COMP_CUTOFF);
		break;
	case SMI_ACC2X2_OFFSET_TRIGGER_X:
		comres = smi130_acc_smbus_read_byte(client,
			SMI_ACC2X2_COMP_TARGET_OFFSET_X__REG, &data);
		*offset = SMI_ACC2X2_GET_BITSLICE(data,
				SMI_ACC2X2_COMP_TARGET_OFFSET_X);
		break;
	case SMI_ACC2X2_OFFSET_TRIGGER_Y:
		comres = smi130_acc_smbus_read_byte(client,
			SMI_ACC2X2_COMP_TARGET_OFFSET_Y__REG, &data);
		*offset = SMI_ACC2X2_GET_BITSLICE(data,
				SMI_ACC2X2_COMP_TARGET_OFFSET_Y);
		break;
	case SMI_ACC2X2_OFFSET_TRIGGER_Z:
		comres = smi130_acc_smbus_read_byte(client,
			SMI_ACC2X2_COMP_TARGET_OFFSET_Z__REG, &data);
		*offset = SMI_ACC2X2_GET_BITSLICE(data,
				SMI_ACC2X2_COMP_TARGET_OFFSET_Z);
		break;
	default:
		comres = -1;
		break;
	}

	return comres;
}

static int smi130_acc_set_offset_target(struct i2c_client *client, unsigned char
		channel, unsigned char offset)
{
	unsigned char data = 0;
	int comres = 0;

	switch (channel) {
	case SMI_ACC2X2_CUT_OFF:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_COMP_CUTOFF__REG, &data);
		data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_COMP_CUTOFF,
				offset);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_COMP_CUTOFF__REG, &data);
		break;
	case SMI_ACC2X2_OFFSET_TRIGGER_X:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_COMP_TARGET_OFFSET_X__REG,
				&data);
		data = SMI_ACC2X2_SET_BITSLICE(data,
				SMI_ACC2X2_COMP_TARGET_OFFSET_X,
				offset);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_COMP_TARGET_OFFSET_X__REG,
				&data);
		break;
	case SMI_ACC2X2_OFFSET_TRIGGER_Y:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_COMP_TARGET_OFFSET_Y__REG,
				&data);
		data = SMI_ACC2X2_SET_BITSLICE(data,
				SMI_ACC2X2_COMP_TARGET_OFFSET_Y,
				offset);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_COMP_TARGET_OFFSET_Y__REG,
				&data);
		break;
	case SMI_ACC2X2_OFFSET_TRIGGER_Z:
		comres = smi130_acc_smbus_read_byte(client,
				SMI_ACC2X2_COMP_TARGET_OFFSET_Z__REG,
				&data);
		data = SMI_ACC2X2_SET_BITSLICE(data,
				SMI_ACC2X2_COMP_TARGET_OFFSET_Z,
				offset);
		comres = smi130_acc_smbus_write_byte(client,
				SMI_ACC2X2_COMP_TARGET_OFFSET_Z__REG,
				&data);
		break;
	default:
		comres = -1;
		break;
	}

	return comres;
}

static int smi130_acc_get_cal_ready(struct i2c_client *client,
					unsigned char *calrdy)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_FAST_CAL_RDY_S__REG,
			&data);
	data = SMI_ACC2X2_GET_BITSLICE(data, SMI_ACC2X2_FAST_CAL_RDY_S);
	*calrdy = data;

	return comres;
}

static int smi130_acc_set_cal_trigger(struct i2c_client *client, unsigned char
		caltrigger)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_CAL_TRIGGER__REG, &data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_CAL_TRIGGER, caltrigger);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_CAL_TRIGGER__REG,
			&data);

	return comres;
}

static int smi130_acc_write_reg(struct i2c_client *client, unsigned char addr,
		unsigned char *data)
{
	int comres = 0;
	comres = smi130_acc_smbus_write_byte(client, addr, data);

	return comres;
}


static int smi130_acc_set_offset_x(struct i2c_client *client, unsigned char
		offsetfilt)
{
	int comres = 0;
	unsigned char data = 0;

	data =  offsetfilt;

#ifdef CONFIG_SENSORS_BMI058
	comres = smi130_acc_smbus_write_byte(client, BMI058_OFFSET_X_AXIS_REG,
							&data);
#else
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_OFFSET_X_AXIS_REG,
						&data);
#endif

	return comres;
}


static int smi130_acc_get_offset_x(struct i2c_client *client, unsigned char
						*offsetfilt)
{
	int comres = 0;
	unsigned char data = 0;

#ifdef CONFIG_SENSORS_BMI058
	comres = smi130_acc_smbus_read_byte(client, BMI058_OFFSET_X_AXIS_REG,
							&data);
#else
	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_OFFSET_X_AXIS_REG,
							&data);
#endif
	*offsetfilt = data;

	return comres;
}

static int smi130_acc_set_offset_y(struct i2c_client *client, unsigned char
						offsetfilt)
{
	int comres = 0;
	unsigned char data = 0;

	data =  offsetfilt;

#ifdef CONFIG_SENSORS_BMI058
	comres = smi130_acc_smbus_write_byte(client, BMI058_OFFSET_Y_AXIS_REG,
							&data);
#else
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_OFFSET_Y_AXIS_REG,
							&data);
#endif
	return comres;
}

static int smi130_acc_get_offset_y(struct i2c_client *client, unsigned char
						*offsetfilt)
{
	int comres = 0;
	unsigned char data = 0;

#ifdef CONFIG_SENSORS_BMI058
	comres = smi130_acc_smbus_read_byte(client, BMI058_OFFSET_Y_AXIS_REG,
							&data);
#else
	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_OFFSET_Y_AXIS_REG,
							&data);
#endif
	*offsetfilt = data;

	return comres;
}

static int smi130_acc_set_offset_z(struct i2c_client *client, unsigned char
						offsetfilt)
{
	int comres = 0;
	unsigned char data = 0;

	data =  offsetfilt;
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_OFFSET_Z_AXIS_REG,
						&data);

	return comres;
}

static int smi130_acc_get_offset_z(struct i2c_client *client, unsigned char
						*offsetfilt)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_OFFSET_Z_AXIS_REG,
						&data);
	*offsetfilt = data;

	return comres;
}


static int smi130_acc_set_selftest_st(struct i2c_client *client, unsigned char
		selftest)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_EN_SELF_TEST__REG,
			&data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_EN_SELF_TEST, selftest);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_EN_SELF_TEST__REG,
			&data);

	return comres;
}

static int smi130_acc_set_selftest_stn(struct i2c_client *client, unsigned char stn)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_NEG_SELF_TEST__REG,
			&data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_NEG_SELF_TEST, stn);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_NEG_SELF_TEST__REG,
			&data);

	return comres;
}

static int smi130_acc_set_selftest_amp(struct i2c_client *client, unsigned char amp)
{
	int comres = 0;
	unsigned char data = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_SELF_TEST_AMP__REG,
			&data);
	data = SMI_ACC2X2_SET_BITSLICE(data, SMI_ACC2X2_SELF_TEST_AMP, amp);
	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_SELF_TEST_AMP__REG,
			&data);

	return comres;
}

static int smi130_acc_read_accel_x(struct i2c_client *client,
				signed char sensor_type, short *a_x)
{
	int comres = 0;
	unsigned char data[2] = {0};

	switch (sensor_type) {
	case 0:
		comres = smi130_acc_smbus_read_byte_block(client,
					SMI_ACC2X2_ACC_X12_LSB__REG, data, 2);
		*a_x = SMI_ACC2X2_GET_BITSLICE(data[0], SMI_ACC2X2_ACC_X12_LSB)|
			(SMI_ACC2X2_GET_BITSLICE(data[1],
				SMI_ACC2X2_ACC_X_MSB)<<(SMI_ACC2X2_ACC_X12_LSB__LEN));
		*a_x = *a_x << (sizeof(short)*8-(SMI_ACC2X2_ACC_X12_LSB__LEN
					+ SMI_ACC2X2_ACC_X_MSB__LEN));
		*a_x = *a_x >> (sizeof(short)*8-(SMI_ACC2X2_ACC_X12_LSB__LEN
					+ SMI_ACC2X2_ACC_X_MSB__LEN));
		break;
	case 1:
		comres = smi130_acc_smbus_read_byte_block(client,
					SMI_ACC2X2_ACC_X10_LSB__REG, data, 2);
		*a_x = SMI_ACC2X2_GET_BITSLICE(data[0], SMI_ACC2X2_ACC_X10_LSB)|
			(SMI_ACC2X2_GET_BITSLICE(data[1],
				SMI_ACC2X2_ACC_X_MSB)<<(SMI_ACC2X2_ACC_X10_LSB__LEN));
		*a_x = *a_x << (sizeof(short)*8-(SMI_ACC2X2_ACC_X10_LSB__LEN
					+ SMI_ACC2X2_ACC_X_MSB__LEN));
		*a_x = *a_x >> (sizeof(short)*8-(SMI_ACC2X2_ACC_X10_LSB__LEN
					+ SMI_ACC2X2_ACC_X_MSB__LEN));
		break;
	case 2:
		comres = smi130_acc_smbus_read_byte_block(client,
					SMI_ACC2X2_ACC_X8_LSB__REG, data, 2);
		*a_x = SMI_ACC2X2_GET_BITSLICE(data[0], SMI_ACC2X2_ACC_X8_LSB)|
			(SMI_ACC2X2_GET_BITSLICE(data[1],
				SMI_ACC2X2_ACC_X_MSB)<<(SMI_ACC2X2_ACC_X8_LSB__LEN));
		*a_x = *a_x << (sizeof(short)*8-(SMI_ACC2X2_ACC_X8_LSB__LEN
					+ SMI_ACC2X2_ACC_X_MSB__LEN));
		*a_x = *a_x >> (sizeof(short)*8-(SMI_ACC2X2_ACC_X8_LSB__LEN
					+ SMI_ACC2X2_ACC_X_MSB__LEN));
		break;
	case 3:
		comres = smi130_acc_smbus_read_byte_block(client,
					SMI_ACC2X2_ACC_X14_LSB__REG, data, 2);
		*a_x = SMI_ACC2X2_GET_BITSLICE(data[0], SMI_ACC2X2_ACC_X14_LSB)|
			(SMI_ACC2X2_GET_BITSLICE(data[1],
				SMI_ACC2X2_ACC_X_MSB)<<(SMI_ACC2X2_ACC_X14_LSB__LEN));
		*a_x = *a_x << (sizeof(short)*8-(SMI_ACC2X2_ACC_X14_LSB__LEN
					+ SMI_ACC2X2_ACC_X_MSB__LEN));
		*a_x = *a_x >> (sizeof(short)*8-(SMI_ACC2X2_ACC_X14_LSB__LEN
					+ SMI_ACC2X2_ACC_X_MSB__LEN));
		break;
	default:
		break;
	}

	return comres;
}

static int smi130_acc_soft_reset(struct i2c_client *client)
{
	int comres = 0;
	unsigned char data = SMI_ACC2X2_EN_SOFT_RESET_VALUE;

	comres = smi130_acc_smbus_write_byte(client, SMI_ACC2X2_EN_SOFT_RESET__REG,
					&data);

	return comres;
}

static int smi130_acc_read_accel_y(struct i2c_client *client,
				signed char sensor_type, short *a_y)
{
	int comres = 0;
	unsigned char data[2] = {0};

	switch (sensor_type) {
	case 0:
		comres = smi130_acc_smbus_read_byte_block(client,
				SMI_ACC2X2_ACC_Y12_LSB__REG, data, 2);
		*a_y = SMI_ACC2X2_GET_BITSLICE(data[0], SMI_ACC2X2_ACC_Y12_LSB)|
			(SMI_ACC2X2_GET_BITSLICE(data[1],
				SMI_ACC2X2_ACC_Y_MSB)<<(SMI_ACC2X2_ACC_Y12_LSB__LEN));
		*a_y = *a_y << (sizeof(short)*8-(SMI_ACC2X2_ACC_Y12_LSB__LEN
						+ SMI_ACC2X2_ACC_Y_MSB__LEN));
		*a_y = *a_y >> (sizeof(short)*8-(SMI_ACC2X2_ACC_Y12_LSB__LEN
						+ SMI_ACC2X2_ACC_Y_MSB__LEN));
		break;
	case 1:
		comres = smi130_acc_smbus_read_byte_block(client,
				SMI_ACC2X2_ACC_Y10_LSB__REG, data, 2);
		*a_y = SMI_ACC2X2_GET_BITSLICE(data[0], SMI_ACC2X2_ACC_Y10_LSB)|
			(SMI_ACC2X2_GET_BITSLICE(data[1],
				SMI_ACC2X2_ACC_Y_MSB)<<(SMI_ACC2X2_ACC_Y10_LSB__LEN));
		*a_y = *a_y << (sizeof(short)*8-(SMI_ACC2X2_ACC_Y10_LSB__LEN
						+ SMI_ACC2X2_ACC_Y_MSB__LEN));
		*a_y = *a_y >> (sizeof(short)*8-(SMI_ACC2X2_ACC_Y10_LSB__LEN
						+ SMI_ACC2X2_ACC_Y_MSB__LEN));
		break;
	case 2:
		comres = smi130_acc_smbus_read_byte_block(client,
				SMI_ACC2X2_ACC_Y8_LSB__REG, data, 2);
		*a_y = SMI_ACC2X2_GET_BITSLICE(data[0], SMI_ACC2X2_ACC_Y8_LSB)|
				(SMI_ACC2X2_GET_BITSLICE(data[1],
				SMI_ACC2X2_ACC_Y_MSB)<<(SMI_ACC2X2_ACC_Y8_LSB__LEN));
		*a_y = *a_y << (sizeof(short)*8-(SMI_ACC2X2_ACC_Y8_LSB__LEN
						+ SMI_ACC2X2_ACC_Y_MSB__LEN));
		*a_y = *a_y >> (sizeof(short)*8-(SMI_ACC2X2_ACC_Y8_LSB__LEN
						+ SMI_ACC2X2_ACC_Y_MSB__LEN));
		break;
	case 3:
		comres = smi130_acc_smbus_read_byte_block(client,
				SMI_ACC2X2_ACC_Y14_LSB__REG, data, 2);
		*a_y = SMI_ACC2X2_GET_BITSLICE(data[0], SMI_ACC2X2_ACC_Y14_LSB)|
			(SMI_ACC2X2_GET_BITSLICE(data[1],
				SMI_ACC2X2_ACC_Y_MSB)<<(SMI_ACC2X2_ACC_Y14_LSB__LEN));
		*a_y = *a_y << (sizeof(short)*8-(SMI_ACC2X2_ACC_Y14_LSB__LEN
						+ SMI_ACC2X2_ACC_Y_MSB__LEN));
		*a_y = *a_y >> (sizeof(short)*8-(SMI_ACC2X2_ACC_Y14_LSB__LEN
						+ SMI_ACC2X2_ACC_Y_MSB__LEN));
		break;
	default:
		break;
	}

	return comres;
}

static int smi130_acc_read_accel_z(struct i2c_client *client,
				signed char sensor_type, short *a_z)
{
	int comres = 0;
	unsigned char data[2] = {0};

	switch (sensor_type) {
	case 0:
		comres = smi130_acc_smbus_read_byte_block(client,
				SMI_ACC2X2_ACC_Z12_LSB__REG, data, 2);
		*a_z = SMI_ACC2X2_GET_BITSLICE(data[0], SMI_ACC2X2_ACC_Z12_LSB)|
			(SMI_ACC2X2_GET_BITSLICE(data[1],
				SMI_ACC2X2_ACC_Z_MSB)<<(SMI_ACC2X2_ACC_Z12_LSB__LEN));
		*a_z = *a_z << (sizeof(short)*8-(SMI_ACC2X2_ACC_Z12_LSB__LEN
						+ SMI_ACC2X2_ACC_Z_MSB__LEN));
		*a_z = *a_z >> (sizeof(short)*8-(SMI_ACC2X2_ACC_Z12_LSB__LEN
						+ SMI_ACC2X2_ACC_Z_MSB__LEN));
		break;
	case 1:
		comres = smi130_acc_smbus_read_byte_block(client,
				SMI_ACC2X2_ACC_Z10_LSB__REG, data, 2);
		*a_z = SMI_ACC2X2_GET_BITSLICE(data[0], SMI_ACC2X2_ACC_Z10_LSB)|
			(SMI_ACC2X2_GET_BITSLICE(data[1],
				SMI_ACC2X2_ACC_Z_MSB)<<(SMI_ACC2X2_ACC_Z10_LSB__LEN));
		*a_z = *a_z << (sizeof(short)*8-(SMI_ACC2X2_ACC_Z10_LSB__LEN
						+ SMI_ACC2X2_ACC_Z_MSB__LEN));
		*a_z = *a_z >> (sizeof(short)*8-(SMI_ACC2X2_ACC_Z10_LSB__LEN
						+ SMI_ACC2X2_ACC_Z_MSB__LEN));
		break;
	case 2:
		comres = smi130_acc_smbus_read_byte_block(client,
				SMI_ACC2X2_ACC_Z8_LSB__REG, data, 2);
		*a_z = SMI_ACC2X2_GET_BITSLICE(data[0], SMI_ACC2X2_ACC_Z8_LSB)|
			(SMI_ACC2X2_GET_BITSLICE(data[1],
				SMI_ACC2X2_ACC_Z_MSB)<<(SMI_ACC2X2_ACC_Z8_LSB__LEN));
		*a_z = *a_z << (sizeof(short)*8-(SMI_ACC2X2_ACC_Z8_LSB__LEN
						+ SMI_ACC2X2_ACC_Z_MSB__LEN));
		*a_z = *a_z >> (sizeof(short)*8-(SMI_ACC2X2_ACC_Z8_LSB__LEN
						+ SMI_ACC2X2_ACC_Z_MSB__LEN));
		break;
	case 3:
		comres = smi130_acc_smbus_read_byte_block(client,
				SMI_ACC2X2_ACC_Z14_LSB__REG, data, 2);
		*a_z = SMI_ACC2X2_GET_BITSLICE(data[0], SMI_ACC2X2_ACC_Z14_LSB)|
				(SMI_ACC2X2_GET_BITSLICE(data[1],
				SMI_ACC2X2_ACC_Z_MSB)<<(SMI_ACC2X2_ACC_Z14_LSB__LEN));
		*a_z = *a_z << (sizeof(short)*8-(SMI_ACC2X2_ACC_Z14_LSB__LEN
						+ SMI_ACC2X2_ACC_Z_MSB__LEN));
		*a_z = *a_z >> (sizeof(short)*8-(SMI_ACC2X2_ACC_Z14_LSB__LEN
						+ SMI_ACC2X2_ACC_Z_MSB__LEN));
		break;
	default:
		break;
	}

	return comres;
}


static int smi130_acc_read_temperature(struct i2c_client *client,
					signed char *temperature)
{
	unsigned char data = 0;
	int comres = 0;

	comres = smi130_acc_smbus_read_byte(client, SMI_ACC2X2_TEMPERATURE_REG, &data);
	*temperature = (signed char)data;

	return comres;
}

#ifdef CONFIG_ENABLE_SMI_ACC_GYRO_BUFFERING
static inline int smi130_check_acc_early_buff_enable_flag(
		struct smi130_acc_data *client_data)
{
	if (client_data->acc_buffer_smi130_samples == true)
		return 1;
	else
		return 0;
}
#else
static inline int smi130_check_acc_early_buff_enable_flag(
		struct smi130_acc_data *client_data)
{
	return 0;
}
#endif

static ssize_t smi130_acc_enable_int_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int type, value;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);
#ifdef CONFIG_SENSORS_BMI058
	int i;
#endif

	sscanf(buf, "%3d %3d", &type, &value);

#ifdef CONFIG_SENSORS_BMI058
	for (i = 0; i < sizeof(int_map) / sizeof(struct interrupt_map_t); i++) {
		if (int_map[i].x == type) {
			type = int_map[i].y;
			break;
		}
		if (int_map[i].y == type) {
			type = int_map[i].x;
			break;
		}
	}
#endif

	if (smi130_acc_set_Int_Enable(smi130_acc->smi130_acc_client, type, value) < 0)
		return -EINVAL;

	return count;
}


static ssize_t smi130_acc_int_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_Int_Mode(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_acc_int_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_Int_Mode(smi130_acc->smi130_acc_client, (unsigned char)data) < 0)
		return -EINVAL;

	return count;
}
static ssize_t smi130_acc_slope_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_slope_duration(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_slope_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_slope_duration(smi130_acc->smi130_acc_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t smi130_acc_slope_no_mot_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_slope_no_mot_duration(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_slope_no_mot_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_slope_no_mot_duration(smi130_acc->smi130_acc_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}


static ssize_t smi130_acc_slope_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_slope_threshold(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_slope_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (smi130_acc_set_slope_threshold(smi130_acc->smi130_acc_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t smi130_acc_slope_no_mot_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_slope_no_mot_threshold(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_slope_no_mot_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (smi130_acc_set_slope_no_mot_threshold(smi130_acc->smi130_acc_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t smi130_acc_high_g_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_high_g_duration(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_high_g_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_high_g_duration(smi130_acc->smi130_acc_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t smi130_acc_high_g_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_high_g_threshold(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_high_g_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (smi130_acc_set_high_g_threshold(smi130_acc->smi130_acc_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t smi130_acc_low_g_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_low_g_duration(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_low_g_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_low_g_duration(smi130_acc->smi130_acc_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t smi130_acc_low_g_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_low_g_threshold(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_low_g_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (smi130_acc_set_low_g_threshold(smi130_acc->smi130_acc_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}
static ssize_t smi130_acc_tap_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_tap_threshold(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_tap_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (smi130_acc_set_tap_threshold(smi130_acc->smi130_acc_client, (unsigned char)data)
			< 0)
		return -EINVAL;

	return count;
}
static ssize_t smi130_acc_tap_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_tap_duration(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_tap_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_tap_duration(smi130_acc->smi130_acc_client, (unsigned char)data)
			< 0)
		return -EINVAL;

	return count;
}
static ssize_t smi130_acc_tap_quiet_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_tap_quiet(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_tap_quiet_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_tap_quiet(smi130_acc->smi130_acc_client, (unsigned char)data) <
			0)
		return -EINVAL;

	return count;
}

static ssize_t smi130_acc_tap_shock_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_tap_shock(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_tap_shock_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_tap_shock(smi130_acc->smi130_acc_client, (unsigned char)data) <
			0)
		return -EINVAL;

	return count;
}

static ssize_t smi130_acc_tap_samp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_tap_samp(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_tap_samp_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_tap_samp(smi130_acc->smi130_acc_client, (unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t smi130_acc_orient_mbl_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_orient_mbl_mode(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_orient_mbl_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_orient_mbl_mode(smi130_acc->smi130_acc_client, (unsigned char)data) <
			0)
		return -EINVAL;

	return count;
}

static ssize_t smi130_acc_orient_mbl_blocking_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_orient_mbl_blocking(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_orient_mbl_blocking_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_orient_mbl_blocking(smi130_acc->smi130_acc_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}
static ssize_t smi130_acc_orient_mbl_hyst_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_orient_mbl_hyst(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_orient_mbl_hyst_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_orient_mbl_hyst(smi130_acc->smi130_acc_client, (unsigned char)data) <
			0)
		return -EINVAL;

	return count;
}

static ssize_t smi130_acc_orient_mbl_theta_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_theta_blocking(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_orient_mbl_theta_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_theta_blocking(smi130_acc->smi130_acc_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t smi130_acc_flat_theta_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_theta_flat(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_flat_theta_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_theta_flat(smi130_acc->smi130_acc_client, (unsigned char)data) <
			0)
		return -EINVAL;

	return count;
}
static ssize_t smi130_acc_flat_hold_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_flat_hold_time(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}
static ssize_t smi130_acc_selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{


	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	return snprintf(buf, 16, "%d\n", atomic_read(&smi130_acc->selftest_result));

}

static ssize_t smi130_acc_softreset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_soft_reset(smi130_acc->smi130_acc_client) < 0)
		return -EINVAL;

	return count;
}
static ssize_t smi130_acc_selftest_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{

	unsigned long data;
	unsigned char clear_value = 0;
	int error;
	short value1 = 0;
	short value2 = 0;
	short diff = 0;
	unsigned long result = 0;
	unsigned char test_result_branch = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	smi130_acc_soft_reset(smi130_acc->smi130_acc_client);
	smi130_acc_delay(5);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (data != 1)
		return -EINVAL;

	smi130_acc_write_reg(smi130_acc->smi130_acc_client, 0x32, &clear_value);

	if ((smi130_acc->sensor_type == SMI_ACC280_TYPE) ||
		(smi130_acc->sensor_type == SMI_ACC255_TYPE)) {
#ifdef CONFIG_SENSORS_BMI058
		/*set self test amp */
		if (smi130_acc_set_selftest_amp(smi130_acc->smi130_acc_client, 1) < 0)
			return -EINVAL;
		/* set to 8 G range */
		if (smi130_acc_set_range(smi130_acc->smi130_acc_client,
							SMI_ACC2X2_RANGE_8G) < 0)
			return -EINVAL;
#else
		/* set to 4 G range */
		if (smi130_acc_set_range(smi130_acc->smi130_acc_client,
							SMI_ACC2X2_RANGE_4G) < 0)
			return -EINVAL;
#endif
	}

	if ((smi130_acc->sensor_type == SMI_ACC250E_TYPE) ||
			(smi130_acc->sensor_type == SMI_ACC222E_TYPE)) {
		/* set to 8 G range */
		if (smi130_acc_set_range(smi130_acc->smi130_acc_client, 8) < 0)
			return -EINVAL;
		if (smi130_acc_set_selftest_amp(smi130_acc->smi130_acc_client, 1) < 0)
			return -EINVAL;
	}

	/* 1 for x-axis(but BMI058 is 1 for y-axis )*/
	smi130_acc_set_selftest_st(smi130_acc->smi130_acc_client, 1);
	smi130_acc_set_selftest_stn(smi130_acc->smi130_acc_client, 0);
	smi130_acc_delay(10);
	smi130_acc_read_accel_x(smi130_acc->smi130_acc_client,
					smi130_acc->sensor_type, &value1);
	smi130_acc_set_selftest_stn(smi130_acc->smi130_acc_client, 1);
	smi130_acc_delay(10);
	smi130_acc_read_accel_x(smi130_acc->smi130_acc_client,
					smi130_acc->sensor_type, &value2);
	diff = value1-value2;

#ifdef CONFIG_SENSORS_BMI058
	PINFO("diff y is %d,value1 is %d, value2 is %d\n", diff,
				value1, value2);
	test_result_branch = 2;
#else
	PINFO("diff x is %d,value1 is %d, value2 is %d\n", diff,
				value1, value2);
	test_result_branch = 1;
#endif

	if (smi130_acc->sensor_type == SMI_ACC280_TYPE) {
#ifdef CONFIG_SENSORS_BMI058
		if (abs(diff) < 819)
			result |= test_result_branch;
#else
		if (abs(diff) < 1638)
			result |= test_result_branch;
#endif
	}
	if (smi130_acc->sensor_type == SMI_ACC255_TYPE) {
		if (abs(diff) < 409)
			result |= 1;
	}
	if (smi130_acc->sensor_type == SMI_ACC250E_TYPE) {
		if (abs(diff) < 51)
			result |= 1;
	}
	if (smi130_acc->sensor_type == SMI_ACC222E_TYPE) {
		if (abs(diff) < 12)
			result |= 1;
	}

	/* 2 for y-axis but BMI058 is 1*/
	smi130_acc_set_selftest_st(smi130_acc->smi130_acc_client, 2);
	smi130_acc_set_selftest_stn(smi130_acc->smi130_acc_client, 0);
	smi130_acc_delay(10);
	smi130_acc_read_accel_y(smi130_acc->smi130_acc_client,
					smi130_acc->sensor_type, &value1);
	smi130_acc_set_selftest_stn(smi130_acc->smi130_acc_client, 1);
	smi130_acc_delay(10);
	smi130_acc_read_accel_y(smi130_acc->smi130_acc_client,
					smi130_acc->sensor_type, &value2);
	diff = value1-value2;

#ifdef CONFIG_SENSORS_BMI058
	PINFO("diff x is %d,value1 is %d, value2 is %d\n", diff,
				value1, value2);
	test_result_branch = 1;
#else
	PINFO("diff y is %d,value1 is %d, value2 is %d\n", diff,
				value1, value2);
	test_result_branch = 2;
#endif

	if (smi130_acc->sensor_type == SMI_ACC280_TYPE) {
#ifdef CONFIG_SENSORS_BMI058
		if (abs(diff) < 819)
			result |= test_result_branch;
#else
		if (abs(diff) < 1638)
			result |= test_result_branch;
#endif
	}
	if (smi130_acc->sensor_type == SMI_ACC255_TYPE) {
		if (abs(diff) < 409)
			result |= test_result_branch;
	}
	if (smi130_acc->sensor_type == SMI_ACC250E_TYPE) {
		if (abs(diff) < 51)
			result |= test_result_branch;
	}
	if (smi130_acc->sensor_type == SMI_ACC222E_TYPE) {
		if (abs(diff) < 12)
			result |= test_result_branch;
	}


	smi130_acc_set_selftest_st(smi130_acc->smi130_acc_client, 3); /* 3 for z-axis*/
	smi130_acc_set_selftest_stn(smi130_acc->smi130_acc_client, 0);
	smi130_acc_delay(10);
	smi130_acc_read_accel_z(smi130_acc->smi130_acc_client,
					smi130_acc->sensor_type, &value1);
	smi130_acc_set_selftest_stn(smi130_acc->smi130_acc_client, 1);
	smi130_acc_delay(10);
	smi130_acc_read_accel_z(smi130_acc->smi130_acc_client,
					smi130_acc->sensor_type, &value2);
	diff = value1-value2;

	PINFO("diff z is %d,value1 is %d, value2 is %d\n", diff,
			value1, value2);

	if (smi130_acc->sensor_type == SMI_ACC280_TYPE) {
#ifdef CONFIG_SENSORS_BMI058
			if (abs(diff) < 409)
				result |= 4;
#else
			if (abs(diff) < 819)
				result |= 4;
#endif
	}
	if (smi130_acc->sensor_type == SMI_ACC255_TYPE) {
		if (abs(diff) < 204)
			result |= 4;
	}
	if (smi130_acc->sensor_type == SMI_ACC250E_TYPE) {
		if (abs(diff) < 25)
			result |= 4;
	}
	if (smi130_acc->sensor_type == SMI_ACC222E_TYPE) {
		if (abs(diff) < 6)
			result |= 4;
	}

	/* self test for smi_acc254 */
	if ((smi130_acc->sensor_type == SMI_ACC255_TYPE) && (result > 0)) {
		result = 0;
		smi130_acc_soft_reset(smi130_acc->smi130_acc_client);
		smi130_acc_delay(5);
		smi130_acc_write_reg(smi130_acc->smi130_acc_client, 0x32, &clear_value);
		/* set to 8 G range */
		if (smi130_acc_set_range(smi130_acc->smi130_acc_client, 8) < 0)
			return -EINVAL;
		if (smi130_acc_set_selftest_amp(smi130_acc->smi130_acc_client, 1) < 0)
			return -EINVAL;

		smi130_acc_set_selftest_st(smi130_acc->smi130_acc_client, 1); /* 1
								for x-axis*/
		smi130_acc_set_selftest_stn(smi130_acc->smi130_acc_client, 0); /*
							positive direction*/
		smi130_acc_delay(10);
		smi130_acc_read_accel_x(smi130_acc->smi130_acc_client,
						smi130_acc->sensor_type, &value1);
		smi130_acc_set_selftest_stn(smi130_acc->smi130_acc_client, 1); /*
							negative direction*/
		smi130_acc_delay(10);
		smi130_acc_read_accel_x(smi130_acc->smi130_acc_client,
						smi130_acc->sensor_type, &value2);
		diff = value1-value2;

		PINFO("diff x is %d,value1 is %d, value2 is %d\n",
						diff, value1, value2);
		if (abs(diff) < 204)
			result |= 1;

		smi130_acc_set_selftest_st(smi130_acc->smi130_acc_client, 2); /* 2
								for y-axis*/
		smi130_acc_set_selftest_stn(smi130_acc->smi130_acc_client, 0); /*
							positive direction*/
		smi130_acc_delay(10);
		smi130_acc_read_accel_y(smi130_acc->smi130_acc_client,
						smi130_acc->sensor_type, &value1);
		smi130_acc_set_selftest_stn(smi130_acc->smi130_acc_client, 1); /*
							negative direction*/
		smi130_acc_delay(10);
		smi130_acc_read_accel_y(smi130_acc->smi130_acc_client,
						smi130_acc->sensor_type, &value2);
		diff = value1-value2;
		PINFO("diff y is %d,value1 is %d, value2 is %d\n",
						diff, value1, value2);

		if (abs(diff) < 204)
			result |= 2;

		smi130_acc_set_selftest_st(smi130_acc->smi130_acc_client, 3); /* 3
								for z-axis*/
		smi130_acc_set_selftest_stn(smi130_acc->smi130_acc_client, 0); /*
							positive direction*/
		smi130_acc_delay(10);
		smi130_acc_read_accel_z(smi130_acc->smi130_acc_client,
						smi130_acc->sensor_type, &value1);
		smi130_acc_set_selftest_stn(smi130_acc->smi130_acc_client, 1); /*
							negative direction*/
		smi130_acc_delay(10);
		smi130_acc_read_accel_z(smi130_acc->smi130_acc_client,
						smi130_acc->sensor_type, &value2);
		diff = value1-value2;

		PINFO("diff z is %d,value1 is %d, value2 is %d\n",
						diff, value1, value2);
		if (abs(diff) < 102)
			result |= 4;
	}

	atomic_set(&smi130_acc->selftest_result, (unsigned int)result);

	smi130_acc_soft_reset(smi130_acc->smi130_acc_client);
	smi130_acc_delay(5);
	PINFO("self test finished\n");

	return count;
}



static ssize_t smi130_acc_flat_hold_time_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_flat_hold_time(smi130_acc->smi130_acc_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

const int smi130_acc_sensor_bitwidth[] = {
	12,  10,  8, 14
};

static int smi130_acc_read_accel_xyz(struct i2c_client *client,
		signed char sensor_type, struct smi130_accacc *acc)
{
	int comres = 0;
	unsigned char data[6] = {0};
	struct smi130_acc_data *client_data = i2c_get_clientdata(client);
#ifndef SMI_ACC2X2_SENSOR_IDENTIFICATION_ENABLE
	int bitwidth;
#endif
	comres = smi130_acc_smbus_read_byte_block(client,
				SMI_ACC2X2_ACC_X12_LSB__REG, data, 6);
	if (sensor_type >= 4)
		return -EINVAL;

	acc->x = (data[1]<<8)|data[0];
	acc->y = (data[3]<<8)|data[2];
	acc->z = (data[5]<<8)|data[4];

#ifndef SMI_ACC2X2_SENSOR_IDENTIFICATION_ENABLE
	bitwidth = smi130_acc_sensor_bitwidth[sensor_type];

	acc->x = (acc->x >> (16 - bitwidth));
	acc->y = (acc->y >> (16 - bitwidth));
	acc->z = (acc->z >> (16 - bitwidth));
#endif

	smi130_acc_remap_sensor_data(acc, client_data);
	return comres;
}

#ifndef CONFIG_SMI_ACC_ENABLE_NEWDATA_INT
static void smi130_acc_work_func(struct work_struct *work)
{
	struct smi130_acc_data *smi130_acc = container_of((struct delayed_work *)work,
			struct smi130_acc_data, work);
	static struct smi130_accacc acc;
	unsigned long delay = msecs_to_jiffies(atomic_read(&smi130_acc->delay));

	smi130_acc_read_accel_xyz(smi130_acc->smi130_acc_client, smi130_acc->sensor_type, &acc);
	input_report_abs(smi130_acc->input, ABS_X, acc.x);
	input_report_abs(smi130_acc->input, ABS_Y, acc.y);
	input_report_abs(smi130_acc->input, ABS_Z, acc.z);
	input_sync(smi130_acc->input);
	mutex_lock(&smi130_acc->value_mutex);
	smi130_acc->value = acc;
	mutex_unlock(&smi130_acc->value_mutex);
	schedule_delayed_work(&smi130_acc->work, delay);
}
#endif
static struct workqueue_struct *reportdata_wq;

uint64_t smi130_acc_get_alarm_timestamp(void)
{
	uint64_t ts_ap;
	struct timespec tmp_time;
	get_monotonic_boottime(&tmp_time);
	ts_ap = (uint64_t)tmp_time.tv_sec * 1000000000 + tmp_time.tv_nsec;
	return ts_ap;
}

#define ABS(x) ((x) > 0 ? (x) : -(x))

static void smi130_acc_timer_work_fun(struct work_struct *work)
{
	struct  smi130_acc_data *smi130_acc =
		container_of(work,
				struct smi130_acc_data, report_data_work);
	int i;
	unsigned char count = 0;
	unsigned char mode = 0;
	signed char fifo_data_out[MAX_FIFO_F_LEVEL * MAX_FIFO_F_BYTES] = {0};
	unsigned char f_len = 0;
	uint64_t del = 0;
	uint64_t time_internal = 0;
	int64_t drift_time = 0;
	static uint64_t time_odr;
	struct smi130_accacc acc_lsb;
	struct timespec ts;
	static uint32_t data_cnt;
	static uint32_t pre_data_cnt;
	static int64_t sample_drift_offset;

	if (smi130_acc->fifo_datasel) {
		/*Select one axis data output for every fifo frame*/
		f_len = 2;
	} else	{
		/*Select X Y Z axis data output for every fifo frame*/
		f_len = 6;
	}
	if (smi130_acc_get_fifo_framecount(smi130_acc->smi130_acc_client, &count) < 0) {
		PERR("smi130_acc_get_fifo_framecount err\n");
		return;
	}
	if (count == 0) {
		PERR("smi130_acc_get_fifo_framecount zero\n");
		return;
	}
	if (count > MAX_FIFO_F_LEVEL) {
		if (smi130_acc_get_mode(smi130_acc->smi130_acc_client, &mode) < 0) {
			PERR("smi130_acc_get_mode err\n");
			return;
		}
		if (SMI_ACC2X2_MODE_NORMAL == mode) {
			PERR("smi130_acc fifo_count: %d abnormal, op_mode: %d\n",
					count, mode);
			count = MAX_FIFO_F_LEVEL;
		} else {
			/*chip already suspend or shutdown*/
			count = 0;
			return;
		}
	}
	if (smi_acc_i2c_burst_read(smi130_acc->smi130_acc_client,
			SMI_ACC2X2_FIFO_DATA_OUTPUT_REG, fifo_data_out,
						count * f_len) < 0) {
		PERR("smi130_acc read fifo err\n");
		return;
	}
	smi130_acc->fifo_time = smi130_acc_get_alarm_timestamp();
	if (smi130_acc->acc_count == 0)
		smi130_acc->base_time = smi130_acc->timestamp =
		smi130_acc->fifo_time - (count-1) * smi130_acc->time_odr;

	smi130_acc->acc_count += count;
	del = smi130_acc->fifo_time - smi130_acc->base_time;
	time_internal = div64_u64(del, smi130_acc->acc_count);

	data_cnt++;
	if (data_cnt == 1)
		time_odr = smi130_acc->time_odr;

	if (time_internal > time_odr) {
		if (time_internal - time_odr > div64_u64 (time_odr, 200))
			time_internal = time_odr + div64_u64(time_odr, 200);
	} else {
		if (time_odr - time_internal > div64_u64(time_odr, 200))
			time_internal = time_odr - div64_u64(time_odr, 200);
	}
/* please give attation for the fifo output data format*/
	if (f_len == 6) {
		/* Select X Y Z axis data output for every frame */
		for (i = 0; i < count; i++) {
			if (smi130_acc->debug_level & 0x01)
				printk(KERN_INFO "smi_acc time =%llu fifo_time =%llu  smi_acc->count=%llu time_internal =%lld time_odr = %lld ",
				smi130_acc->timestamp, smi130_acc->fifo_time,
				smi130_acc->acc_count, time_internal, time_odr);

			ts = ns_to_timespec(smi130_acc->timestamp);
			acc_lsb.x =
			((unsigned char)fifo_data_out[i * f_len + 1] << 8 |
				(unsigned char)fifo_data_out[i * f_len + 0]);
			acc_lsb.y =
			((unsigned char)fifo_data_out[i * f_len + 3] << 8 |
				(unsigned char)fifo_data_out[i * f_len + 2]);
			acc_lsb.z =
			((unsigned char)fifo_data_out[i * f_len + 5] << 8 |
				(unsigned char)fifo_data_out[i * f_len + 4]);
#ifndef SMI_ACC2X2_SENSOR_IDENTIFICATION_ENABLE
			acc_lsb.x >>=
			(16 - smi130_acc_sensor_bitwidth[smi130_acc->sensor_type]);
			acc_lsb.y >>=
			(16 - smi130_acc_sensor_bitwidth[smi130_acc->sensor_type]);
			acc_lsb.z >>=
			(16 - smi130_acc_sensor_bitwidth[smi130_acc->sensor_type]);
#endif
			smi130_acc_remap_sensor_data(&acc_lsb, smi130_acc);
			input_event(smi130_acc->input, EV_MSC, MSC_TIME,
			ts.tv_sec);
			input_event(smi130_acc->input, EV_MSC, MSC_TIME,
			ts.tv_nsec);
			input_event(smi130_acc->input, EV_MSC,
				MSC_GESTURE, acc_lsb.x);
			input_event(smi130_acc->input, EV_MSC,
				MSC_RAW, acc_lsb.y);
			input_event(smi130_acc->input, EV_MSC,
				MSC_SCAN, acc_lsb.z);
			input_sync(smi130_acc->input);
			smi130_acc->timestamp +=
				time_internal - sample_drift_offset;
		}
	}
	drift_time = smi130_acc->timestamp - smi130_acc->fifo_time;
	if (data_cnt % 20 == 0) {
		if (ABS(drift_time) > div64_u64(time_odr, 5)) {
			sample_drift_offset =
			div64_s64(drift_time, smi130_acc->acc_count - pre_data_cnt);
			pre_data_cnt = smi130_acc->acc_count;
			time_odr = time_internal;
		}
	}

}

static enum hrtimer_restart reportdata_timer_fun(
	struct hrtimer *hrtimer)
{
	struct smi130_acc_data *client_data =
		container_of(hrtimer, struct smi130_acc_data, timer);
	int32_t delay = 0;
	delay = 8;
	queue_work(reportdata_wq, &(client_data->report_data_work));
	/*set delay 8ms*/
	client_data->work_delay_kt = ns_to_ktime(delay*1000000);
	hrtimer_forward(hrtimer, ktime_get(), client_data->work_delay_kt);

	return HRTIMER_RESTART;
}

static ssize_t smi130_acc_enable_timer_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	return snprintf(buf, 16, "%d\n", smi130_acc->is_timer_running);
}

static ssize_t smi130_acc_enable_timer_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);
	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (data) {
		if (0 == smi130_acc->is_timer_running) {
			hrtimer_start(&smi130_acc->timer,
			ns_to_ktime(1000000),
			HRTIMER_MODE_REL);
			smi130_acc->base_time = 0;
			smi130_acc->timestamp = 0;
			smi130_acc->is_timer_running = 1;
	}
	} else {
		if (1 == smi130_acc->is_timer_running) {
			hrtimer_cancel(&smi130_acc->timer);
			smi130_acc->is_timer_running = 0;
			smi130_acc->base_time = 0;
			smi130_acc->timestamp = 0;
			smi130_acc->fifo_time = 0;
			smi130_acc->acc_count = 0;
	}
	}
	return count;
}

static ssize_t smi130_acc_debug_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);
	err = snprintf(buf, 8, "%d\n", smi130_acc->debug_level);
	return err;
}
static ssize_t smi130_acc_debug_level_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int32_t ret = 0;
	unsigned long data;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	ret = kstrtoul(buf, 16, &data);
	if (ret)
		return ret;
	smi130_acc->debug_level = (uint8_t)data;
	return count;
}

static ssize_t smi130_acc_register_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int address, value;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	sscanf(buf, "%3d %3d", &address, &value);
	if (smi130_acc_write_reg(smi130_acc->smi130_acc_client, (unsigned char)address,
				(unsigned char *)&value) < 0)
		return -EINVAL;
	return count;
}
static ssize_t smi130_acc_register_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	size_t count = 0;
	u8 reg[0x40] = {0};
	int i;

	for (i = 0; i < 0x40; i++) {
		smi130_acc_smbus_read_byte(smi130_acc->smi130_acc_client, i, reg+i);

		count += snprintf(&buf[count], 32, "0x%x: %d\n", i, reg[i]);
	}
	return count;


}

static ssize_t smi130_acc_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_range(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_acc_range_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = smi130_check_acc_early_buff_enable_flag(smi130_acc);
	if (error)
		return count;

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (smi130_acc_set_range(smi130_acc->smi130_acc_client, (unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t smi130_acc_bandwidth_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_bandwidth(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_bandwidth_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = smi130_check_acc_early_buff_enable_flag(smi130_acc);
	if (error)
		return count;

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc->sensor_type == SMI_ACC280_TYPE)
		if ((unsigned char) data > 14)
			return -EINVAL;

	if (smi130_acc_set_bandwidth(smi130_acc->smi130_acc_client,
				(unsigned char) data) < 0)
		return -EINVAL;
	smi130_acc->base_time = 0;
	smi130_acc->acc_count = 0;

	return count;
}

static ssize_t smi130_acc_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_mode(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 32, "%d %d\n", data, smi130_acc->smi_acc_mode_enabled);
}

static ssize_t smi130_acc_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = smi130_check_acc_early_buff_enable_flag(smi130_acc);
	if (error)
		return count;

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (smi130_acc_set_mode(smi130_acc->smi130_acc_client,
		(unsigned char) data, SMI_ACC_ENABLED_BSX) < 0)
			return -EINVAL;

	return count;
}

static ssize_t smi130_acc_value_cache_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi130_acc_data *smi130_acc = input_get_drvdata(input);
	struct smi130_accacc acc_value;

	mutex_lock(&smi130_acc->value_mutex);
	acc_value = smi130_acc->value;
	mutex_unlock(&smi130_acc->value_mutex);

	return snprintf(buf, 96, "%d %d %d\n", acc_value.x, acc_value.y,
			acc_value.z);
}

static ssize_t smi130_acc_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi130_acc_data *smi130_acc = input_get_drvdata(input);
	struct smi130_accacc acc_value;

	smi130_acc_read_accel_xyz(smi130_acc->smi130_acc_client, smi130_acc->sensor_type,
								&acc_value);

	return snprintf(buf, 96, "%d %d %d\n", acc_value.x, acc_value.y,
			acc_value.z);
}

static ssize_t smi130_acc_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	return snprintf(buf, 16, "%d\n", atomic_read(&smi130_acc->delay));

}

static ssize_t smi130_acc_chip_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	return snprintf(buf, 16, "%u\n", smi130_acc->chip_id);

}


static ssize_t smi130_acc_place_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);
	int place = BOSCH_SENSOR_PLACE_UNKNOWN;

	if (NULL != smi130_acc->bosch_pd)
		place = smi130_acc->bosch_pd->place;

	return snprintf(buf, 16, "%d\n", place);
}


static ssize_t smi130_acc_delay_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (data > SMI_ACC2X2_MAX_DELAY)
		data = SMI_ACC2X2_MAX_DELAY;
	atomic_set(&smi130_acc->delay, (unsigned int) data);

	return count;
}


static ssize_t smi130_acc_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	return snprintf(buf, 16, "%d\n", atomic_read(&smi130_acc->enable));

}

static void smi130_acc_set_enable(struct device *dev, int enable)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);
	int pre_enable = atomic_read(&smi130_acc->enable);

	mutex_lock(&smi130_acc->enable_mutex);
	if (enable) {
		if (pre_enable == 0) {
			smi130_acc_set_mode(smi130_acc->smi130_acc_client,
					SMI_ACC2X2_MODE_NORMAL, SMI_ACC_ENABLED_INPUT);

		#ifndef CONFIG_SMI_ACC_ENABLE_NEWDATA_INT
			schedule_delayed_work(&smi130_acc->work,
				msecs_to_jiffies(atomic_read(&smi130_acc->delay)));
#endif
			atomic_set(&smi130_acc->enable, 1);
		}

	} else {
		if (pre_enable == 1) {
			smi130_acc_set_mode(smi130_acc->smi130_acc_client,
					SMI_ACC2X2_MODE_SUSPEND, SMI_ACC_ENABLED_INPUT);

		#ifndef CONFIG_SMI_ACC_ENABLE_NEWDATA_INT
			cancel_delayed_work_sync(&smi130_acc->work);
#endif
			atomic_set(&smi130_acc->enable, 0);
		}
	}
	mutex_unlock(&smi130_acc->enable_mutex);

}

static ssize_t smi130_acc_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if ((data == 0) || (data == 1))
		smi130_acc_set_enable(dev, data);

	return count;
}
static ssize_t smi130_acc_fast_calibration_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{


	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

#ifdef CONFIG_SENSORS_BMI058
	if (smi130_acc_get_offset_target(smi130_acc->smi130_acc_client,
				BMI058_OFFSET_TRIGGER_X, &data) < 0)
		return -EINVAL;
#else
	if (smi130_acc_get_offset_target(smi130_acc->smi130_acc_client,
				SMI_ACC2X2_OFFSET_TRIGGER_X, &data) < 0)
		return -EINVAL;
#endif

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_fast_calibration_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	signed char tmp;
	unsigned char timeout = 0;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

#ifdef CONFIG_SENSORS_BMI058
	if (smi130_acc_set_offset_target(smi130_acc->smi130_acc_client,
			BMI058_OFFSET_TRIGGER_X, (unsigned char)data) < 0)
		return -EINVAL;
#else
	if (smi130_acc_set_offset_target(smi130_acc->smi130_acc_client,
			SMI_ACC2X2_OFFSET_TRIGGER_X, (unsigned char)data) < 0)
		return -EINVAL;
#endif

	if (smi130_acc_set_cal_trigger(smi130_acc->smi130_acc_client, 1) < 0)
		return -EINVAL;

	do {
		smi130_acc_delay(2);
		smi130_acc_get_cal_ready(smi130_acc->smi130_acc_client, &tmp);

		/*PINFO("wait 2ms cal ready flag is %d\n", tmp); */
		timeout++;
		if (timeout == 50) {
			PINFO("get fast calibration ready error\n");
			return -EINVAL;
		};

	} while (tmp == 0);

	PINFO("x axis fast calibration finished\n");
	return count;
}

static ssize_t smi130_acc_fast_calibration_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{


	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

#ifdef CONFIG_SENSORS_BMI058
	if (smi130_acc_get_offset_target(smi130_acc->smi130_acc_client,
					BMI058_OFFSET_TRIGGER_Y, &data) < 0)
		return -EINVAL;
#else
	if (smi130_acc_get_offset_target(smi130_acc->smi130_acc_client,
					SMI_ACC2X2_OFFSET_TRIGGER_Y, &data) < 0)
		return -EINVAL;
#endif

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_fast_calibration_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	signed char tmp;
	unsigned char timeout = 0;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

#ifdef CONFIG_SENSORS_BMI058
	if (smi130_acc_set_offset_target(smi130_acc->smi130_acc_client,
			BMI058_OFFSET_TRIGGER_Y, (unsigned char)data) < 0)
		return -EINVAL;
#else
	if (smi130_acc_set_offset_target(smi130_acc->smi130_acc_client,
			SMI_ACC2X2_OFFSET_TRIGGER_Y, (unsigned char)data) < 0)
		return -EINVAL;
#endif

	if (smi130_acc_set_cal_trigger(smi130_acc->smi130_acc_client, 2) < 0)
		return -EINVAL;

	do {
		smi130_acc_delay(2);
		smi130_acc_get_cal_ready(smi130_acc->smi130_acc_client, &tmp);

		/*PINFO("wait 2ms cal ready flag is %d\n", tmp);*/
		timeout++;
		if (timeout == 50) {
			PINFO("get fast calibration ready error\n");
			return -EINVAL;
		};

	} while (tmp == 0);

	PINFO("y axis fast calibration finished\n");
	return count;
}

static ssize_t smi130_acc_fast_calibration_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{


	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_offset_target(smi130_acc->smi130_acc_client, 3, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_fast_calibration_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	signed char tmp;
	unsigned char timeout = 0;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_offset_target(smi130_acc->smi130_acc_client, 3, (unsigned
					char)data) < 0)
		return -EINVAL;

	if (smi130_acc_set_cal_trigger(smi130_acc->smi130_acc_client, 3) < 0)
		return -EINVAL;

	do {
		smi130_acc_delay(2);
		smi130_acc_get_cal_ready(smi130_acc->smi130_acc_client, &tmp);

		/*PINFO("wait 2ms cal ready flag is %d\n", tmp);*/
		timeout++;
		if (timeout == 50) {
			PINFO("get fast calibration ready error\n");
			return -EINVAL;
		};

	} while (tmp == 0);

	PINFO("z axis fast calibration finished\n");
	return count;
}


static ssize_t smi130_acc_SleepDur_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_sleep_duration(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_SleepDur_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (smi130_acc_set_sleep_duration(smi130_acc->smi130_acc_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t smi130_acc_fifo_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_fifo_mode(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_fifo_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (smi130_acc_set_fifo_mode(smi130_acc->smi130_acc_client,
				(unsigned char) data) < 0)
		return -EINVAL;
	return count;
}



static ssize_t smi130_acc_fifo_trig_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_fifo_trig(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_fifo_trig_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (smi130_acc_set_fifo_trig(smi130_acc->smi130_acc_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}



static ssize_t smi130_acc_fifo_trig_src_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_fifo_trig_src(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_fifo_trig_src_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (smi130_acc_set_fifo_trig_src(smi130_acc->smi130_acc_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}


/*!
 * @brief show fifo_data_sel axis definition(Android definition, not sensor HW reg).
 * 0--> x, y, z axis fifo data for every frame
 * 1--> only x axis fifo data for every frame
 * 2--> only y axis fifo data for every frame
 * 3--> only z axis fifo data for every frame
 */
static ssize_t smi130_acc_fifo_data_sel_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);
	signed char place = BOSCH_SENSOR_PLACE_UNKNOWN;
	if (smi130_acc_get_fifo_data_sel(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

#ifdef CONFIG_SENSORS_BMI058
/*Update BMI058 fifo_data_sel to the SMI130_ACC common definition*/
	if (BMI058_FIFO_DAT_SEL_X == data)
		data = SMI_ACC2X2_FIFO_DAT_SEL_X;
	else if (BMI058_FIFO_DAT_SEL_Y == data)
		data = SMI_ACC2X2_FIFO_DAT_SEL_Y;
#endif

	/*remaping fifo_dat_sel if define virtual place in BSP files*/
	if ((NULL != smi130_acc->bosch_pd) &&
		(BOSCH_SENSOR_PLACE_UNKNOWN != smi130_acc->bosch_pd->place)) {
		place = smi130_acc->bosch_pd->place;
		/* sensor with place 0 needs not to be remapped */
		if ((place > 0) && (place < MAX_AXIS_REMAP_TAB_SZ)) {
			/* SMI_ACC2X2_FIFO_DAT_SEL_X: 1, Y:2, Z:3;
			* but bosch_axis_remap_tab_dft[i].src_x:0, y:1, z:2
			* so we need to +1*/
			if (SMI_ACC2X2_FIFO_DAT_SEL_X == data)
				data = bosch_axis_remap_tab_dft[place].src_x + 1;
			else if (SMI_ACC2X2_FIFO_DAT_SEL_Y == data)
				data = bosch_axis_remap_tab_dft[place].src_y + 1;
		}
	}

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_fifo_framecount_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	unsigned char mode;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_fifo_framecount(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	if (data > MAX_FIFO_F_LEVEL) {

		if (smi130_acc_get_mode(smi130_acc->smi130_acc_client, &mode) < 0)
			return -EINVAL;

		if (SMI_ACC2X2_MODE_NORMAL == mode) {
			PERR("smi130_acc fifo_count: %d abnormal, op_mode: %d",
					data, mode);
			data = MAX_FIFO_F_LEVEL;
		} else {
			/*chip already suspend or shutdown*/
			data = 0;
		}
	}

	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_acc_fifo_framecount_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	smi130_acc->fifo_count = (unsigned int) data;

	return count;
}

static ssize_t smi130_acc_temperature_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_read_temperature(smi130_acc->smi130_acc_client, &data) < 0)
		return -EINVAL;

	return snprintf(buf, 16, "%d\n", data);

}

/*!
 * @brief store fifo_data_sel axis definition(Android definition, not sensor HW reg).
 * 0--> x, y, z axis fifo data for every frame
 * 1--> only x axis fifo data for every frame
 * 2--> only y axis fifo data for every frame
 * 3--> only z axis fifo data for every frame
 */
static ssize_t smi130_acc_fifo_data_sel_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);
	signed char place;

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	/*save fifo_data_sel(android definition)*/
	smi130_acc->fifo_datasel = (unsigned char) data;

	/*remaping fifo_dat_sel if define virtual place*/
	if ((NULL != smi130_acc->bosch_pd) &&
		(BOSCH_SENSOR_PLACE_UNKNOWN != smi130_acc->bosch_pd->place)) {
		place = smi130_acc->bosch_pd->place;
		/* sensor with place 0 needs not to be remapped */
		if ((place > 0) && (place < MAX_AXIS_REMAP_TAB_SZ)) {
			/*Need X Y axis revesal sensor place: P1, P3, P5, P7 */
			/* SMI_ACC2X2_FIFO_DAT_SEL_X: 1, Y:2, Z:3;
			  * but bosch_axis_remap_tab_dft[i].src_x:0, y:1, z:2
			  * so we need to +1*/
			if (SMI_ACC2X2_FIFO_DAT_SEL_X == data)
				data =  bosch_axis_remap_tab_dft[place].src_x + 1;
			else if (SMI_ACC2X2_FIFO_DAT_SEL_Y == data)
				data =  bosch_axis_remap_tab_dft[place].src_y + 1;
		}
	}
#ifdef CONFIG_SENSORS_BMI058
	/*Update BMI058 fifo_data_sel to the SMI130_ACC common definition*/
		if (SMI_ACC2X2_FIFO_DAT_SEL_X == data)
			data = BMI058_FIFO_DAT_SEL_X;
		else if (SMI_ACC2X2_FIFO_DAT_SEL_Y == data)
			data = BMI058_FIFO_DAT_SEL_Y;

#endif
	if (smi130_acc_set_fifo_data_sel(smi130_acc->smi130_acc_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t smi130_acc_fifo_data_out_frame_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char f_len = 0;
	unsigned char count = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);
	if (smi130_acc->fifo_datasel) {
		/*Select one axis data output for every fifo frame*/
		f_len = 2;
	} else	{
		/*Select X Y Z axis data output for every fifo frame*/
		f_len = 6;
	}
	if (smi130_acc_get_fifo_framecount(smi130_acc->smi130_acc_client, &count) < 0) {
		PERR("smi130_acc_get_fifo_framecount err\n");
		return -EINVAL;
	}
	if (count == 0)
		return 0;
	if (smi_acc_i2c_burst_read(smi130_acc->smi130_acc_client,
			SMI_ACC2X2_FIFO_DATA_OUTPUT_REG, buf,
						count * f_len) < 0)
		return -EINVAL;

	return count * f_len;
}

static ssize_t smi130_acc_offset_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_offset_x(smi130_acc->smi130_acc_client, &data) < 0)
		return snprintf(buf, 48, "Read error\n");

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_offset_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_offset_x(smi130_acc->smi130_acc_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t smi130_acc_offset_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_offset_y(smi130_acc->smi130_acc_client, &data) < 0)
		return snprintf(buf, 48, "Read error\n");

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_offset_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_offset_y(smi130_acc->smi130_acc_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t smi130_acc_offset_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	if (smi130_acc_get_offset_z(smi130_acc->smi130_acc_client, &data) < 0)
		return snprintf(buf, 48, "Read error\n");

	return snprintf(buf, 16, "%d\n", data);

}

static ssize_t smi130_acc_offset_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (smi130_acc_set_offset_z(smi130_acc->smi130_acc_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t smi130_acc_driver_version_show(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);
	int ret;

	if (smi130_acc == NULL) {
		printk(KERN_ERR "Invalid client_data pointer");
		return -ENODEV;
	}

	ret = snprintf(buf, 128, "Driver version: %s\n",
			DRIVER_VERSION);
	return ret;
}

#ifdef CONFIG_SIG_MOTION
static int smi130_acc_set_en_slope_int(struct smi130_acc_data *smi130_acc,
		int en)
{
	int err;
	struct i2c_client *client = smi130_acc->smi130_acc_client;

	if (en) {
		/* Set the related parameters which needs to be fine tuned by
		* interfaces: slope_threshold and slope_duration
		*/
		/*dur: 192 samples ~= 3s*/
		err = smi130_acc_set_slope_duration(client, 0x0);
		err += smi130_acc_set_slope_threshold(client, 0x16);

		/*Enable the interrupts*/
		err += smi130_acc_set_Int_Enable(client, 5, 1);/*Slope X*/
		err += smi130_acc_set_Int_Enable(client, 6, 1);/*Slope Y*/
		err += smi130_acc_set_Int_Enable(client, 7, 1);/*Slope Z*/
	#ifdef SMI_ACC2X2_ENABLE_INT1
		/* TODO: SLOPE can now only be routed to INT1 pin*/
		err += smi130_acc_set_int1_pad_sel(client, PAD_SLOP);
	#else
		/* err += smi130_acc_set_int2_pad_sel(client, PAD_SLOP); */
	#endif
	} else {
		err = smi130_acc_set_Int_Enable(client, 5, 0);/*Slope X*/
		err += smi130_acc_set_Int_Enable(client, 6, 0);/*Slope Y*/
		err += smi130_acc_set_Int_Enable(client, 7, 0);/*Slope Z*/
	}
	return err;
}

static ssize_t smi130_acc_en_sig_motion_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	return snprintf(buf, 16, "%d\n", atomic_read(&smi130_acc->en_sig_motion));
}

static int smi130_acc_set_en_sig_motion(struct smi130_acc_data *smi130_acc,
		int en)
{
	int err = 0;

	en = (en >= 1) ? 1 : 0;  /* set sig motion sensor status */

	if (atomic_read(&smi130_acc->en_sig_motion) != en) {
		if (en) {
			err = smi130_acc_set_mode(smi130_acc->smi130_acc_client,
					SMI_ACC2X2_MODE_NORMAL, SMI_ACC_ENABLED_SGM);
			err = smi130_acc_set_en_slope_int(smi130_acc, en);
			enable_irq_wake(smi130_acc->IRQ);
		} else {
			disable_irq_wake(smi130_acc->IRQ);
			err = smi130_acc_set_en_slope_int(smi130_acc, en);
			err = smi130_acc_set_mode(smi130_acc->smi130_acc_client,
					SMI_ACC2X2_MODE_SUSPEND, SMI_ACC_ENABLED_SGM);
		}
		atomic_set(&smi130_acc->en_sig_motion, en);
	}
	return err;
}

static ssize_t smi130_acc_en_sig_motion_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if ((data == 0) || (data == 1))
		smi130_acc_set_en_sig_motion(smi130_acc, data);

	return count;
}
#endif

#ifdef CONFIG_DOUBLE_TAP
static int smi130_acc_set_en_single_tap_int(struct smi130_acc_data *smi130_acc, int en)
{
	int err;
	struct i2c_client *client = smi130_acc->smi130_acc_client;

	if (en) {
		/* set tap interruption parameter here if needed.
		smi130_acc_set_tap_duration(client, 0xc0);
		smi130_acc_set_tap_threshold(client, 0x16);
		*/

		/*Enable the single tap interrupts*/
		err = smi130_acc_set_Int_Enable(client, 8, 1);
	#ifdef SMI_ACC2X2_ENABLE_INT1
		err += smi130_acc_set_int1_pad_sel(client, PAD_SINGLE_TAP);
	#else
		err += smi130_acc_set_int2_pad_sel(client, PAD_SINGLE_TAP);
	#endif
	} else {
		err = smi130_acc_set_Int_Enable(client, 8, 0);
	}
	return err;
}

static ssize_t smi130_acc_tap_time_period_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	return snprintf(buf, 16, "%d\n", smi130_acc->tap_time_period);
}

static ssize_t smi130_acc_tap_time_period_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	smi130_acc->tap_time_period = data;

	return count;
}

static ssize_t smi130_acc_en_double_tap_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	return snprintf(buf, 16, "%d\n", atomic_read(&smi130_acc->en_double_tap));
}

static int smi130_acc_set_en_double_tap(struct smi130_acc_data *smi130_acc,
		int en)
{
	int err = 0;

	en = (en >= 1) ? 1 : 0;

	if (atomic_read(&smi130_acc->en_double_tap) != en) {
		if (en) {
			err = smi130_acc_set_mode(smi130_acc->smi130_acc_client,
					SMI_ACC2X2_MODE_NORMAL, SMI_ACC_ENABLED_DTAP);
			err = smi130_acc_set_en_single_tap_int(smi130_acc, en);
		} else {
			err = smi130_acc_set_en_single_tap_int(smi130_acc, en);
			err = smi130_acc_set_mode(smi130_acc->smi130_acc_client,
					SMI_ACC2X2_MODE_SUSPEND, SMI_ACC_ENABLED_DTAP);
		}
		atomic_set(&smi130_acc->en_double_tap, en);
	}
	return err;
}

static ssize_t smi130_acc_en_double_tap_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if ((data == 0) || (data == 1))
		smi130_acc_set_en_double_tap(smi130_acc, data);

	return count;
}

static void smi130_acc_tap_timeout_handle(unsigned long data)
{
	struct smi130_acc_data *smi130_acc = (struct smi130_acc_data *)data;

	PINFO("tap interrupt handle, timeout\n");
	mutex_lock(&smi130_acc->tap_mutex);
	smi130_acc->tap_times = 0;
	mutex_unlock(&smi130_acc->tap_mutex);

	/* if a single tap need to report, open the define */
#ifdef REPORT_SINGLE_TAP_WHEN_DOUBLE_TAP_SENSOR_ENABLED
	input_report_rel(smi130_acc->dev_interrupt,
		SINGLE_TAP_INTERRUPT,
		SINGLE_TAP_INTERRUPT_HAPPENED);
	input_sync(smi130_acc->dev_interrupt);
#endif

}
#endif

#ifdef CONFIG_ENABLE_SMI_ACC_GYRO_BUFFERING
static int smi_acc_read_bootsampl(struct smi130_acc_data *client_data,
		unsigned long enable_read)
{
	int i = 0;

	if (enable_read) {
		client_data->acc_buffer_smi130_samples = false;
		for (i = 0; i < client_data->acc_bufsample_cnt; i++) {
			if (client_data->debug_level & 0x08)
				PINFO("acc=%d,x=%d,y=%d,z=%d,sec=%d,ns=%lld\n",
				i, client_data->smi130_acc_samplist[i]->xyz[0],
				client_data->smi130_acc_samplist[i]->xyz[1],
				client_data->smi130_acc_samplist[i]->xyz[2],
				client_data->smi130_acc_samplist[i]->tsec,
				client_data->smi130_acc_samplist[i]->tnsec);
			input_report_abs(client_data->accbuf_dev, ABS_X,
				client_data->smi130_acc_samplist[i]->xyz[0]);
			input_report_abs(client_data->accbuf_dev, ABS_Y,
				client_data->smi130_acc_samplist[i]->xyz[1]);
			input_report_abs(client_data->accbuf_dev, ABS_Z,
				client_data->smi130_acc_samplist[i]->xyz[2]);
			input_report_abs(client_data->accbuf_dev, ABS_RX,
				client_data->smi130_acc_samplist[i]->tsec);
			input_report_abs(client_data->accbuf_dev, ABS_RY,
				client_data->smi130_acc_samplist[i]->tnsec);
			input_sync(client_data->accbuf_dev);
		}
	} else {
		/* clean up */
		if (client_data->acc_bufsample_cnt != 0) {
			for (i = 0; i < SMI_ACC_MAXSAMPLE; i++)
				kmem_cache_free(client_data->smi_acc_cachepool,
					client_data->smi130_acc_samplist[i]);
			kmem_cache_destroy(client_data->smi_acc_cachepool);
			client_data->acc_bufsample_cnt = 0;
		}

	}
	/*SYN_CONFIG indicates end of data*/
	input_event(client_data->accbuf_dev, EV_SYN, SYN_CONFIG, 0xFFFFFFFF);
	input_sync(client_data->accbuf_dev);
	if (client_data->debug_level & 0x08)
		PINFO("End of acc samples bufsample_cnt=%d\n",
				client_data->acc_bufsample_cnt);
	return 0;
}
static ssize_t read_acc_boot_sample_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);

	return snprintf(buf, 16, "%u\n",
			smi130_acc->read_acc_boot_sample);
}

static ssize_t read_acc_boot_sample_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	struct i2c_client *client = to_i2c_client(dev);
	struct smi130_acc_data *smi130_acc = i2c_get_clientdata(client);
	unsigned long enable = 0;

	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;
	if (enable > 1) {
		PERR("Invalid value of input, input=%ld\n", enable);
		return -EINVAL;
	}
	err = smi_acc_read_bootsampl(smi130_acc, enable);
	if (err)
		return err;

	smi130_acc->read_acc_boot_sample = enable;
	return count;
}
#endif

static DEVICE_ATTR(range, S_IRUGO | S_IWUSR,
		smi130_acc_range_show, smi130_acc_range_store);
static DEVICE_ATTR(bandwidth, S_IRUGO | S_IWUSR,
		smi130_acc_bandwidth_show, smi130_acc_bandwidth_store);
#ifdef CONFIG_ENABLE_SMI_ACC_GYRO_BUFFERING
static DEVICE_ATTR(read_acc_boot_sample, S_IRUGO|S_IWUSR,
		read_acc_boot_sample_show, read_acc_boot_sample_store);
#endif
static DEVICE_ATTR(op_mode, S_IRUGO | S_IWUSR,
		smi130_acc_mode_show, smi130_acc_mode_store);
static DEVICE_ATTR(value, S_IRUSR,
		smi130_acc_value_show, NULL);
static DEVICE_ATTR(value_cache, S_IRUSR,
		smi130_acc_value_cache_show, NULL);
static DEVICE_ATTR(delay, S_IRUGO | S_IWUSR,
		smi130_acc_delay_show, smi130_acc_delay_store);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR,
		smi130_acc_enable_show, smi130_acc_enable_store);
static DEVICE_ATTR(SleepDur, S_IRUGO | S_IWUSR,
		smi130_acc_SleepDur_show, smi130_acc_SleepDur_store);
static DEVICE_ATTR(fast_calibration_x, S_IRUGO | S_IWUSR,
		smi130_acc_fast_calibration_x_show,
		smi130_acc_fast_calibration_x_store);
static DEVICE_ATTR(fast_calibration_y, S_IRUGO | S_IWUSR,
		smi130_acc_fast_calibration_y_show,
		smi130_acc_fast_calibration_y_store);
static DEVICE_ATTR(fast_calibration_z, S_IRUGO | S_IWUSR,
		smi130_acc_fast_calibration_z_show,
		smi130_acc_fast_calibration_z_store);
static DEVICE_ATTR(fifo_mode, S_IRUGO | S_IWUSR,
		smi130_acc_fifo_mode_show, smi130_acc_fifo_mode_store);
static DEVICE_ATTR(fifo_framecount, S_IRUGO | S_IWUSR,
		smi130_acc_fifo_framecount_show, smi130_acc_fifo_framecount_store);
static DEVICE_ATTR(fifo_trig, S_IRUGO | S_IWUSR,
		smi130_acc_fifo_trig_show, smi130_acc_fifo_trig_store);
static DEVICE_ATTR(fifo_trig_src, S_IRUGO | S_IWUSR,
		smi130_acc_fifo_trig_src_show, smi130_acc_fifo_trig_src_store);
static DEVICE_ATTR(fifo_data_sel, S_IRUGO | S_IWUSR,
		smi130_acc_fifo_data_sel_show, smi130_acc_fifo_data_sel_store);
static DEVICE_ATTR(fifo_data_frame, S_IRUGO,
		smi130_acc_fifo_data_out_frame_show, NULL);
static DEVICE_ATTR(reg, S_IRUGO | S_IWUSR,
		smi130_acc_register_show, smi130_acc_register_store);
static DEVICE_ATTR(chip_id, S_IRUSR,
		smi130_acc_chip_id_show, NULL);
static DEVICE_ATTR(offset_x, S_IRUGO | S_IWUSR,
		smi130_acc_offset_x_show,
		smi130_acc_offset_x_store);
static DEVICE_ATTR(offset_y, S_IRUGO | S_IWUSR,
		smi130_acc_offset_y_show,
		smi130_acc_offset_y_store);
static DEVICE_ATTR(offset_z, S_IRUGO | S_IWUSR,
		smi130_acc_offset_z_show,
		smi130_acc_offset_z_store);
static DEVICE_ATTR(enable_int, S_IWUSR,
		NULL, smi130_acc_enable_int_store);
static DEVICE_ATTR(int_mode, S_IRUGO | S_IWUSR,
		smi130_acc_int_mode_show, smi130_acc_int_mode_store);
static DEVICE_ATTR(slope_duration, S_IRUGO | S_IWUSR,
		smi130_acc_slope_duration_show, smi130_acc_slope_duration_store);
static DEVICE_ATTR(slope_threshold, S_IRUGO | S_IWUSR,
		smi130_acc_slope_threshold_show, smi130_acc_slope_threshold_store);
static DEVICE_ATTR(slope_no_mot_duration, S_IRUGO | S_IWUSR,
		smi130_acc_slope_no_mot_duration_show,
			smi130_acc_slope_no_mot_duration_store);
static DEVICE_ATTR(slope_no_mot_threshold, S_IRUGO | S_IWUSR,
		smi130_acc_slope_no_mot_threshold_show,
			smi130_acc_slope_no_mot_threshold_store);
static DEVICE_ATTR(high_g_duration, S_IRUGO | S_IWUSR,
		smi130_acc_high_g_duration_show, smi130_acc_high_g_duration_store);
static DEVICE_ATTR(high_g_threshold, S_IRUGO | S_IWUSR,
		smi130_acc_high_g_threshold_show, smi130_acc_high_g_threshold_store);
static DEVICE_ATTR(low_g_duration, S_IRUGO | S_IWUSR,
		smi130_acc_low_g_duration_show, smi130_acc_low_g_duration_store);
static DEVICE_ATTR(low_g_threshold, S_IRUGO | S_IWUSR,
		smi130_acc_low_g_threshold_show, smi130_acc_low_g_threshold_store);
static DEVICE_ATTR(tap_duration, S_IRUGO | S_IWUSR,
		smi130_acc_tap_duration_show, smi130_acc_tap_duration_store);
static DEVICE_ATTR(tap_threshold, S_IRUGO | S_IWUSR,
		smi130_acc_tap_threshold_show, smi130_acc_tap_threshold_store);
static DEVICE_ATTR(tap_quiet, S_IRUGO | S_IWUSR,
		smi130_acc_tap_quiet_show, smi130_acc_tap_quiet_store);
static DEVICE_ATTR(tap_shock, S_IRUGO | S_IWUSR,
		smi130_acc_tap_shock_show, smi130_acc_tap_shock_store);
static DEVICE_ATTR(tap_samp, S_IRUGO | S_IWUSR,
		smi130_acc_tap_samp_show, smi130_acc_tap_samp_store);
static DEVICE_ATTR(orient_mbl_mode, S_IRUGO | S_IWUSR,
		smi130_acc_orient_mbl_mode_show, smi130_acc_orient_mbl_mode_store);
static DEVICE_ATTR(orient_mbl_blocking, S_IRUGO | S_IWUSR,
		smi130_acc_orient_mbl_blocking_show, smi130_acc_orient_mbl_blocking_store);
static DEVICE_ATTR(orient_mbl_hyst, S_IRUGO | S_IWUSR,
		smi130_acc_orient_mbl_hyst_show, smi130_acc_orient_mbl_hyst_store);
static DEVICE_ATTR(orient_mbl_theta, S_IRUGO | S_IWUSR,
		smi130_acc_orient_mbl_theta_show, smi130_acc_orient_mbl_theta_store);
static DEVICE_ATTR(flat_theta, S_IRUGO | S_IWUSR,
		smi130_acc_flat_theta_show, smi130_acc_flat_theta_store);
static DEVICE_ATTR(flat_hold_time, S_IRUGO | S_IWUSR,
		smi130_acc_flat_hold_time_show, smi130_acc_flat_hold_time_store);
static DEVICE_ATTR(selftest, S_IRUGO | S_IWUSR,
		smi130_acc_selftest_show, smi130_acc_selftest_store);
static DEVICE_ATTR(softreset, S_IWUSR,
		NULL, smi130_acc_softreset_store);
static DEVICE_ATTR(enable_timer, S_IRUGO | S_IWUSR,
		smi130_acc_enable_timer_show, smi130_acc_enable_timer_store);
static DEVICE_ATTR(debug_level, S_IRUGO | S_IWUSR,
		smi130_acc_debug_level_show, smi130_acc_debug_level_store);
static DEVICE_ATTR(temperature, S_IRUSR,
		smi130_acc_temperature_show, NULL);
static DEVICE_ATTR(place, S_IRUSR,
		smi130_acc_place_show, NULL);
static DEVICE_ATTR(driver_version, S_IRUSR,
		smi130_acc_driver_version_show, NULL);

#ifdef CONFIG_SIG_MOTION
static DEVICE_ATTR(en_sig_motion, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		smi130_acc_en_sig_motion_show, smi130_acc_en_sig_motion_store);
#endif
#ifdef CONFIG_DOUBLE_TAP
static DEVICE_ATTR(tap_time_period, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		smi130_acc_tap_time_period_show, smi130_acc_tap_time_period_store);
static DEVICE_ATTR(en_double_tap, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		smi130_acc_en_double_tap_show, smi130_acc_en_double_tap_store);
#endif

static struct attribute *smi130_acc_attributes[] = {
	&dev_attr_range.attr,
	&dev_attr_bandwidth.attr,
#ifdef CONFIG_ENABLE_SMI_ACC_GYRO_BUFFERING
	&dev_attr_read_acc_boot_sample.attr,
#endif
	&dev_attr_op_mode.attr,
	&dev_attr_value.attr,
	&dev_attr_value_cache.attr,
	&dev_attr_delay.attr,
	&dev_attr_enable.attr,
	&dev_attr_SleepDur.attr,
	&dev_attr_reg.attr,
	&dev_attr_fast_calibration_x.attr,
	&dev_attr_fast_calibration_y.attr,
	&dev_attr_fast_calibration_z.attr,
	&dev_attr_fifo_mode.attr,
	&dev_attr_fifo_framecount.attr,
	&dev_attr_fifo_trig.attr,
	&dev_attr_fifo_trig_src.attr,
	&dev_attr_fifo_data_sel.attr,
	&dev_attr_fifo_data_frame.attr,
	&dev_attr_chip_id.attr,
	&dev_attr_offset_x.attr,
	&dev_attr_offset_y.attr,
	&dev_attr_offset_z.attr,
	&dev_attr_enable_int.attr,
	&dev_attr_enable_timer.attr,
	&dev_attr_debug_level.attr,
	&dev_attr_int_mode.attr,
	&dev_attr_slope_duration.attr,
	&dev_attr_slope_threshold.attr,
	&dev_attr_slope_no_mot_duration.attr,
	&dev_attr_slope_no_mot_threshold.attr,
	&dev_attr_high_g_duration.attr,
	&dev_attr_high_g_threshold.attr,
	&dev_attr_low_g_duration.attr,
	&dev_attr_low_g_threshold.attr,
	&dev_attr_tap_threshold.attr,
	&dev_attr_tap_duration.attr,
	&dev_attr_tap_quiet.attr,
	&dev_attr_tap_shock.attr,
	&dev_attr_tap_samp.attr,
	&dev_attr_orient_mbl_mode.attr,
	&dev_attr_orient_mbl_blocking.attr,
	&dev_attr_orient_mbl_hyst.attr,
	&dev_attr_orient_mbl_theta.attr,
	&dev_attr_flat_theta.attr,
	&dev_attr_flat_hold_time.attr,
	&dev_attr_selftest.attr,
	&dev_attr_softreset.attr,
	&dev_attr_temperature.attr,
	&dev_attr_place.attr,
	&dev_attr_driver_version.attr,
#ifdef CONFIG_SIG_MOTION
	&dev_attr_en_sig_motion.attr,
#endif
#ifdef CONFIG_DOUBLE_TAP
	&dev_attr_en_double_tap.attr,
#endif

	NULL
};

static struct attribute_group smi130_acc_attribute_group = {
	.attrs = smi130_acc_attributes
};

#ifdef CONFIG_SIG_MOTION
static struct attribute *smi130_acc_sig_motion_attributes[] = {
	&dev_attr_slope_duration.attr,
	&dev_attr_slope_threshold.attr,
	&dev_attr_en_sig_motion.attr,
	NULL
};
static struct attribute_group smi130_acc_sig_motion_attribute_group = {
	.attrs = smi130_acc_sig_motion_attributes
};
#endif

#ifdef CONFIG_DOUBLE_TAP
static struct attribute *smi130_acc_double_tap_attributes[] = {
	&dev_attr_tap_threshold.attr,
	&dev_attr_tap_duration.attr,
	&dev_attr_tap_quiet.attr,
	&dev_attr_tap_shock.attr,
	&dev_attr_tap_samp.attr,
	&dev_attr_tap_time_period.attr,
	&dev_attr_en_double_tap.attr,
	NULL
};
static struct attribute_group smi130_acc_double_tap_attribute_group = {
	.attrs = smi130_acc_double_tap_attributes
};
#endif


#if defined(SMI_ACC2X2_ENABLE_INT1) || defined(SMI_ACC2X2_ENABLE_INT2)
unsigned char *orient_mbl[] = {"upward looking portrait upright",
	"upward looking portrait upside-down",
		"upward looking landscape left",
		"upward looking landscape right",
		"downward looking portrait upright",
		"downward looking portrait upside-down",
		"downward looking landscape left",
		"downward looking landscape right"};


static void smi130_acc_high_g_interrupt_handle(struct smi130_acc_data *smi130_acc)
{
	unsigned char first_value = 0;
	unsigned char sign_value = 0;
	int i;

	for (i = 0; i < 3; i++) {
		smi130_acc_get_HIGH_first(smi130_acc->smi130_acc_client, i, &first_value);
		if (first_value == 1) {
			smi130_acc_get_HIGH_sign(smi130_acc->smi130_acc_client,
								&sign_value);
			if (sign_value == 1) {
				if (i == 0)
					input_report_rel(smi130_acc->dev_interrupt,
							HIGH_G_INTERRUPT,
							HIGH_G_INTERRUPT_X_N);
				if (i == 1)
					input_report_rel(smi130_acc->dev_interrupt,
							HIGH_G_INTERRUPT,
							HIGH_G_INTERRUPT_Y_N);
				if (i == 2)
					input_report_rel(smi130_acc->dev_interrupt,
							HIGH_G_INTERRUPT,
							HIGH_G_INTERRUPT_Z_N);
			} else {
				if (i == 0)
					input_report_rel(smi130_acc->dev_interrupt,
							HIGH_G_INTERRUPT,
							HIGH_G_INTERRUPT_X);
				if (i == 1)
					input_report_rel(smi130_acc->dev_interrupt,
							HIGH_G_INTERRUPT,
							HIGH_G_INTERRUPT_Y);
				if (i == 2)
					input_report_rel(smi130_acc->dev_interrupt,
							HIGH_G_INTERRUPT,
							HIGH_G_INTERRUPT_Z);
			}
		}

		PINFO("High G interrupt happened,exis is %d,\n\n"
					"first is %d,sign is %d\n", i,
						first_value, sign_value);
	}


}

#ifndef CONFIG_SIG_MOTION
static void smi130_acc_slope_interrupt_handle(struct smi130_acc_data *smi130_acc)
{
	unsigned char first_value = 0;
	unsigned char sign_value = 0;
	int i;
	for (i = 0; i < 3; i++) {
		smi130_acc_get_slope_first(smi130_acc->smi130_acc_client, i, &first_value);
		if (first_value == 1) {
			smi130_acc_get_slope_sign(smi130_acc->smi130_acc_client,
								&sign_value);
			if (sign_value == 1) {
				if (i == 0)
					input_report_rel(smi130_acc->dev_interrupt,
							SLOP_INTERRUPT,
							SLOPE_INTERRUPT_X_N);
				if (i == 1)
					input_report_rel(smi130_acc->dev_interrupt,
							SLOP_INTERRUPT,
							SLOPE_INTERRUPT_Y_N);
				if (i == 2)
					input_report_rel(smi130_acc->dev_interrupt,
							SLOP_INTERRUPT,
							SLOPE_INTERRUPT_Z_N);
			} else {
				if (i == 0)
					input_report_rel(smi130_acc->dev_interrupt,
							SLOP_INTERRUPT,
							SLOPE_INTERRUPT_X);
				if (i == 1)
					input_report_rel(smi130_acc->dev_interrupt,
							SLOP_INTERRUPT,
							SLOPE_INTERRUPT_Y);
				if (i == 2)
					input_report_rel(smi130_acc->dev_interrupt,
							SLOP_INTERRUPT,
							SLOPE_INTERRUPT_Z);

			}
		}

		PINFO("Slop interrupt happened,exis is %d,\n\n"
					"first is %d,sign is %d\n", i,
						first_value, sign_value);
	}
}
#endif
#ifdef CONFIG_ENABLE_SMI_ACC_GYRO_BUFFERING
static void store_acc_boot_sample(struct smi130_acc_data *client_data,
				int x, int y, int z, struct timespec ts)
{
	if (false == client_data->acc_buffer_smi130_samples)
		return;
	if (ts.tv_sec <  client_data->max_buffer_time) {
		if (client_data->acc_bufsample_cnt < SMI_ACC_MAXSAMPLE) {
			client_data->smi130_acc_samplist[client_data->
				acc_bufsample_cnt]->xyz[0] = x;
			client_data->smi130_acc_samplist[client_data->
				acc_bufsample_cnt]->xyz[1] = y;
			client_data->smi130_acc_samplist[client_data->
				acc_bufsample_cnt]->xyz[2] = z;
			client_data->smi130_acc_samplist[client_data->
				acc_bufsample_cnt]->tsec = ts.tv_sec;
			client_data->smi130_acc_samplist[client_data->
				acc_bufsample_cnt]->tnsec = ts.tv_nsec;
			client_data->acc_bufsample_cnt++;
		}
	} else {
		PINFO("End of ACC buffering %d\n",
				client_data->acc_bufsample_cnt);
		client_data->acc_buffer_smi130_samples = false;
	}
}
#else
static void store_acc_boot_sample(struct smi130_acc_data *client_data,
		int x, int y, int z, struct timespec ts)
{
}
#endif

#ifdef CONFIG_ENABLE_SMI_ACC_GYRO_BUFFERING
static int smi130_acc_early_buff_init(struct i2c_client *client,
			struct smi130_acc_data *client_data)
{
	int i = 0, err = 0;

	client_data->acc_bufsample_cnt = 0;
	client_data->report_evt_cnt = 5;
	client_data->max_buffer_time = 40;

	client_data->smi_acc_cachepool = kmem_cache_create("acc_sensor_sample",
			sizeof(struct smi_acc_sample),
			0,
			SLAB_HWCACHE_ALIGN, NULL);
	if (!client_data->smi_acc_cachepool) {
		PERR("smi_acc_cachepool cache create failed\n");
		err = -ENOMEM;
		return 0;
	}
	for (i = 0; i < SMI_ACC_MAXSAMPLE; i++) {
		client_data->smi130_acc_samplist[i] =
			kmem_cache_alloc(client_data->smi_acc_cachepool,
					GFP_KERNEL);
		if (!client_data->smi130_acc_samplist[i]) {
			err = -ENOMEM;
			goto clean_exit1;
		}
	}

	client_data->accbuf_dev = input_allocate_device();
	if (!client_data->accbuf_dev) {
		err = -ENOMEM;
		PERR("input device allocation failed\n");
		goto clean_exit1;
	}
	client_data->accbuf_dev->name = "smi130_accbuf";
	client_data->accbuf_dev->id.bustype = BUS_I2C;
	input_set_events_per_packet(client_data->accbuf_dev,
			client_data->report_evt_cnt * SMI_ACC_MAXSAMPLE);
	set_bit(EV_ABS, client_data->accbuf_dev->evbit);
	input_set_abs_params(client_data->accbuf_dev, ABS_X,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->accbuf_dev, ABS_Y,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->accbuf_dev, ABS_Z,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->accbuf_dev, ABS_RX,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->accbuf_dev, ABS_RY,
			-G_MAX, G_MAX, 0, 0);
	err = input_register_device(client_data->accbuf_dev);
	if (err) {
		PERR("unable to register input device %s\n",
				client_data->accbuf_dev->name);
		goto clean_exit2;
	}

	client_data->acc_buffer_smi130_samples = true;

	smi130_set_cpu_idle_state(true);

	smi130_acc_set_mode(client, SMI_ACC2X2_MODE_NORMAL, 1);
	smi130_acc_set_bandwidth(client, SMI_ACC2X2_BW_62_50HZ);
	smi130_acc_set_range(client, SMI_ACC2X2_RANGE_2G);

	return 1;

clean_exit2:
	input_free_device(client_data->accbuf_dev);
clean_exit1:
	for (i = 0; i < SMI_ACC_MAXSAMPLE; i++)
		kmem_cache_free(client_data->smi_acc_cachepool,
				client_data->smi130_acc_samplist[i]);
	kmem_cache_destroy(client_data->smi_acc_cachepool);

	return 0;
}

static void smi130_acc_input_cleanup(struct smi130_acc_data *client_data)
{
	int i = 0;

	input_unregister_device(client_data->accbuf_dev);
	input_free_device(client_data->accbuf_dev);
	for (i = 0; i < SMI_ACC_MAXSAMPLE; i++)
		kmem_cache_free(client_data->smi_acc_cachepool,
				client_data->smi130_acc_samplist[i]);
	kmem_cache_destroy(client_data->smi_acc_cachepool);
}
#else
static int smi130_acc_early_buff_init(struct i2c_client *client,
			struct smi130_acc_data *client_data)
{
	return 1;
}
static void smi130_acc_input_cleanup(struct smi130_acc_data *client_data)
{
}
#endif

static irqreturn_t smi130_acc_irq_work_func(int irq, void *handle)
{
	struct smi130_acc_data *smi130_acc = handle;
#ifdef CONFIG_DOUBLE_TAP
	struct i2c_client *client = smi130_acc->smi130_acc_client;
#endif

	unsigned char status = 0;
	unsigned char first_value = 0;
	unsigned char sign_value = 0;

#ifdef CONFIG_SMI_ACC_ENABLE_NEWDATA_INT
	static struct smi130_accacc acc;
	struct timespec ts;
	/*
	do not use this function judge new data interrupt
	smi130_acc_get_interruptstatus2(smi130_acc->smi130_acc_client, &status);
	use the
	x-axis value bit new_data_x
	y-axis value bit new_data_y
	z-axis value bit new_data_z
	judge if this is the new data
	*/
	/* PINFO("New data interrupt happened\n");*/
	smi130_acc_read_accel_xyz(smi130_acc->smi130_acc_client,
				smi130_acc->sensor_type, &acc);
	ts = ns_to_timespec(smi130_acc->timestamp);
	//if ((acc.x & SMI_ACC2X2_NEW_DATA_X__MSK) &&
	//	(acc.y & SMI_ACC2X2_NEW_DATA_Y__MSK) &&
	//	(acc.x & SMI_ACC2X2_NEW_DATA_Z__MSK))
	{
		input_event(smi130_acc->input, EV_MSC, MSC_TIME,
			ts.tv_sec);
		input_event(smi130_acc->input, EV_MSC, MSC_TIME,
			ts.tv_nsec);
		input_event(smi130_acc->input, EV_MSC,
			MSC_GESTURE, acc.x);
		input_event(smi130_acc->input, EV_MSC,
			MSC_RAW, acc.y);
		input_event(smi130_acc->input, EV_MSC,
			MSC_SCAN, acc.z);
		input_sync(smi130_acc->input);
		mutex_lock(&smi130_acc->value_mutex);
		smi130_acc->value = acc;
		mutex_unlock(&smi130_acc->value_mutex);
	}
	store_acc_boot_sample(smi130_acc, acc.x, acc.y, acc.z, ts);

	smi130_set_cpu_idle_state(false);
	return IRQ_HANDLED;
#endif
	smi130_acc_get_interruptstatus1(smi130_acc->smi130_acc_client, &status);
	PDEBUG("smi130_acc_irq_work_func, status = 0x%x\n", status);

#ifdef CONFIG_SIG_MOTION
	if (status & 0x04)	{
		if (atomic_read(&smi130_acc->en_sig_motion) == 1) {
			PINFO("Significant motion interrupt happened\n");
			/* close sig sensor,
			it will be open again if APP wants */
			smi130_acc_set_en_sig_motion(smi130_acc, 0);

			input_report_rel(smi130_acc->dev_interrupt,
				SLOP_INTERRUPT, 1);
			input_sync(smi130_acc->dev_interrupt);
		}
	}
#endif

#ifdef CONFIG_DOUBLE_TAP
	if (status & 0x20) {
		if (atomic_read(&smi130_acc->en_double_tap) == 1) {
			PINFO("single tap interrupt happened\n");
			smi130_acc_set_Int_Enable(client, 8, 0);
			if (smi130_acc->tap_times == 0)	{
				mod_timer(&smi130_acc->tap_timer, jiffies +
				msecs_to_jiffies(smi130_acc->tap_time_period));
				smi130_acc->tap_times = 1;
			} else {
				/* only double tap is judged */
				PINFO("double tap\n");
				mutex_lock(&smi130_acc->tap_mutex);
				smi130_acc->tap_times = 0;
				del_timer(&smi130_acc->tap_timer);
				mutex_unlock(&smi130_acc->tap_mutex);
				input_report_rel(smi130_acc->dev_interrupt,
					DOUBLE_TAP_INTERRUPT,
					DOUBLE_TAP_INTERRUPT_HAPPENED);
				input_sync(smi130_acc->dev_interrupt);
			}
			smi130_acc_set_Int_Enable(client, 8, 1);
		}
	}
#endif

	switch (status) {

	case 0x01:
		PINFO("Low G interrupt happened\n");
		input_report_rel(smi130_acc->dev_interrupt, LOW_G_INTERRUPT,
				LOW_G_INTERRUPT_HAPPENED);
		break;

	case 0x02:
		smi130_acc_high_g_interrupt_handle(smi130_acc);
		break;

#ifndef CONFIG_SIG_MOTION
	case 0x04:
		smi130_acc_slope_interrupt_handle(smi130_acc);
		break;
#endif

	case 0x08:
		PINFO("slow/ no motion interrupt happened\n");
		input_report_rel(smi130_acc->dev_interrupt,
			SLOW_NO_MOTION_INTERRUPT,
			SLOW_NO_MOTION_INTERRUPT_HAPPENED);
		break;

#ifndef CONFIG_DOUBLE_TAP
	case 0x10:
		PINFO("double tap interrupt happened\n");
		input_report_rel(smi130_acc->dev_interrupt,
			DOUBLE_TAP_INTERRUPT,
			DOUBLE_TAP_INTERRUPT_HAPPENED);
		break;
	case 0x20:
		PINFO("single tap interrupt happened\n");
		input_report_rel(smi130_acc->dev_interrupt,
			SINGLE_TAP_INTERRUPT,
			SINGLE_TAP_INTERRUPT_HAPPENED);
		break;
#endif

	case 0x40:
		smi130_acc_get_orient_mbl_status(smi130_acc->smi130_acc_client,
				    &first_value);
		PINFO("orient_mbl interrupt happened,%s\n",
				orient_mbl[first_value]);
		if (first_value == 0)
			input_report_abs(smi130_acc->dev_interrupt,
			ORIENT_INTERRUPT,
			UPWARD_PORTRAIT_UP_INTERRUPT_HAPPENED);
		else if (first_value == 1)
			input_report_abs(smi130_acc->dev_interrupt,
				ORIENT_INTERRUPT,
				UPWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED);
		else if (first_value == 2)
			input_report_abs(smi130_acc->dev_interrupt,
				ORIENT_INTERRUPT,
				UPWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED);
		else if (first_value == 3)
			input_report_abs(smi130_acc->dev_interrupt,
				ORIENT_INTERRUPT,
				UPWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED);
		else if (first_value == 4)
			input_report_abs(smi130_acc->dev_interrupt,
				ORIENT_INTERRUPT,
				DOWNWARD_PORTRAIT_UP_INTERRUPT_HAPPENED);
		else if (first_value == 5)
			input_report_abs(smi130_acc->dev_interrupt,
				ORIENT_INTERRUPT,
				DOWNWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED);
		else if (first_value == 6)
			input_report_abs(smi130_acc->dev_interrupt,
				ORIENT_INTERRUPT,
				DOWNWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED);
		else if (first_value == 7)
			input_report_abs(smi130_acc->dev_interrupt,
				ORIENT_INTERRUPT,
				DOWNWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED);
		break;
	case 0x80:
		smi130_acc_get_orient_mbl_flat_status(smi130_acc->smi130_acc_client,
				    &sign_value);
		PINFO("flat interrupt happened,flat status is %d\n",
				    sign_value);
		if (sign_value == 1) {
			input_report_abs(smi130_acc->dev_interrupt,
				FLAT_INTERRUPT,
				FLAT_INTERRUPT_TURE_HAPPENED);
		} else {
			input_report_abs(smi130_acc->dev_interrupt,
				FLAT_INTERRUPT,
				FLAT_INTERRUPT_FALSE_HAPPENED);
		}
		break;

	default:
		break;
	}
}

static irqreturn_t smi130_acc_irq_handler(int irq, void *handle)
{
	struct smi130_acc_data *data = handle;

	if (data == NULL)
		return IRQ_HANDLED;
	if (data->smi130_acc_client == NULL)
		return IRQ_HANDLED;
	data->timestamp = smi130_acc_get_alarm_timestamp();
	smi130_hrtimer_reset(data);

	return IRQ_WAKE_THREAD;
}
#endif /* defined(SMI_ACC2X2_ENABLE_INT1)||defined(SMI_ACC2X2_ENABLE_INT2) */


static int smi130_acc_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int err = 0;
	struct smi130_acc_data *data;
	struct input_dev *dev;
	struct bosch_dev  *dev_acc;
#if defined(SMI_ACC2X2_ENABLE_INT1) || defined(SMI_ACC2X2_ENABLE_INT2)
	struct bosch_sensor_specific *pdata;
#endif
	struct input_dev *dev_interrupt;

	PINFO("smi130_acc_probe start\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		PERR("i2c_check_functionality error\n");
		err = -EIO;
		goto exit;
	}
	data = kzalloc(sizeof(struct smi130_acc_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	/* read and check chip id */
	if (smi130_acc_check_chip_id(client, data) < 0) {
		err = -EINVAL;
		goto kfree_exit;
	}

	/* do soft reset */
	smi130_acc_delay(5);
	if (smi130_acc_soft_reset(client) < 0) {
		PERR("i2c bus write error, pls check HW connection\n");
		err = -EINVAL;
		goto kfree_exit;
	}
	smi130_acc_delay(20);

	i2c_set_clientdata(client, data);
	data->smi130_acc_client = client;
	mutex_init(&data->value_mutex);
	mutex_init(&data->mode_mutex);
	mutex_init(&data->enable_mutex);
	smi130_acc_set_bandwidth(client, SMI_ACC2X2_BW_SET);
	smi130_acc_set_range(client, SMI_ACC2X2_RANGE_SET);

#if defined(SMI_ACC2X2_ENABLE_INT1) || defined(SMI_ACC2X2_ENABLE_INT2)

	pdata = client->dev.platform_data;
	if (pdata) {
		if (pdata->irq_gpio_cfg && (pdata->irq_gpio_cfg() < 0)) {
			PERR("IRQ GPIO conf. error %d\n",
				client->irq);
		}
	}

#ifdef SMI_ACC2X2_ENABLE_INT1
	/* maps interrupt to INT1 pin */
	smi130_acc_set_int1_pad_sel(client, PAD_LOWG);
	smi130_acc_set_int1_pad_sel(client, PAD_HIGHG);
	smi130_acc_set_int1_pad_sel(client, PAD_SLOP);
	smi130_acc_set_int1_pad_sel(client, PAD_DOUBLE_TAP);
	smi130_acc_set_int1_pad_sel(client, PAD_SINGLE_TAP);
	smi130_acc_set_int1_pad_sel(client, PAD_ORIENT);
	smi130_acc_set_int1_pad_sel(client, PAD_FLAT);
	smi130_acc_set_int1_pad_sel(client, PAD_SLOW_NO_MOTION);
#ifdef CONFIG_SMI_ACC_ENABLE_NEWDATA_INT
	smi130_acc_set_newdata(client, SMI_ACC2X2_INT1_NDATA, 1);
	smi130_acc_set_newdata(client, SMI_ACC2X2_INT2_NDATA, 0);
#endif
#endif

#ifdef SMI_ACC2X2_ENABLE_INT2
	/* maps interrupt to INT2 pin */
	smi130_acc_set_int2_pad_sel(client, PAD_LOWG);
	smi130_acc_set_int2_pad_sel(client, PAD_HIGHG);
	smi130_acc_set_int2_pad_sel(client, PAD_SLOP);
	smi130_acc_set_int2_pad_sel(client, PAD_DOUBLE_TAP);
	smi130_acc_set_int2_pad_sel(client, PAD_SINGLE_TAP);
	smi130_acc_set_int2_pad_sel(client, PAD_ORIENT);
	smi130_acc_set_int2_pad_sel(client, PAD_FLAT);
	smi130_acc_set_int2_pad_sel(client, PAD_SLOW_NO_MOTION);
#ifdef CONFIG_SMI_ACC_ENABLE_NEWDATA_INT
	smi130_acc_set_newdata(client, SMI_ACC2X2_INT1_NDATA, 0);
	smi130_acc_set_newdata(client, SMI_ACC2X2_INT2_NDATA, 1);
#endif
#endif

	smi130_acc_set_Int_Mode(client, 1);/*latch interrupt 250ms*/

	/* do not open any interrupt here  */
	/*10,orient_mbl
	11,flat*/
	/* smi130_acc_set_Int_Enable(client, 10, 1);	*/
	/* smi130_acc_set_Int_Enable(client, 11, 1); */

#ifdef CONFIG_SMI_ACC_ENABLE_NEWDATA_INT
	/* enable new data interrupt */
	smi130_acc_set_Int_Enable(client, 4, 1);
#endif

#ifdef CONFIG_SIG_MOTION
	enable_irq_wake(data->IRQ);
#endif
	if (err)
		PERR("could not request irq\n");

#endif

#ifndef CONFIG_SMI_ACC_ENABLE_NEWDATA_INT
	INIT_DELAYED_WORK(&data->work, smi130_acc_work_func);
#endif
	atomic_set(&data->delay, SMI_ACC2X2_MAX_DELAY);
	atomic_set(&data->enable, 0);

	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;

	dev_interrupt = input_allocate_device();
	if (!dev_interrupt) {
		kfree(data);
		input_free_device(dev); /*free the successful dev and return*/
		return -ENOMEM;
	}

	/* only value events reported */
	dev->name = SENSOR_NAME;
	dev->id.bustype = BUS_I2C;
	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_abs_params(dev, ABS_X, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Y, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Z, ABSMIN, ABSMAX, 0, 0);
	input_set_capability(dev, EV_MSC, MSC_GESTURE);
	input_set_capability(dev, EV_MSC, MSC_RAW);
	input_set_capability(dev, EV_MSC, MSC_SCAN);
	input_set_capability(dev, EV_MSC, MSC_TIME);
	input_set_drvdata(dev, data);
	err = input_register_device(dev);
	if (err < 0)
		goto err_register_input_device;

	/* all interrupt generated events are moved to interrupt input devices*/
	dev_interrupt->name = "smi_acc_interrupt";
	dev_interrupt->id.bustype = BUS_I2C;
	input_set_capability(dev_interrupt, EV_REL,
		SLOW_NO_MOTION_INTERRUPT);
	input_set_capability(dev_interrupt, EV_REL,
		LOW_G_INTERRUPT);
	input_set_capability(dev_interrupt, EV_REL,
		HIGH_G_INTERRUPT);
	input_set_capability(dev_interrupt, EV_REL,
		SLOP_INTERRUPT);
	input_set_capability(dev_interrupt, EV_REL,
		DOUBLE_TAP_INTERRUPT);
	input_set_capability(dev_interrupt, EV_REL,
		SINGLE_TAP_INTERRUPT);
	input_set_capability(dev_interrupt, EV_ABS,
		ORIENT_INTERRUPT);
	input_set_capability(dev_interrupt, EV_ABS,
		FLAT_INTERRUPT);
	input_set_drvdata(dev_interrupt, data);

	err = input_register_device(dev_interrupt);
	if (err < 0)
		goto err_register_input_device_interrupt;

	data->dev_interrupt = dev_interrupt;
	data->input = dev;

#ifdef CONFIG_SIG_MOTION
	data->g_sensor_class = class_create(THIS_MODULE, "sig_sensor");
	if (IS_ERR(data->g_sensor_class)) {
		err = PTR_ERR(data->g_sensor_class);
		data->g_sensor_class = NULL;
		PERR("could not allocate g_sensor_class\n");
		goto err_create_class;
	}

	data->g_sensor_dev = device_create(data->g_sensor_class,
				NULL, 0, "%s", "g_sensor");
	if (unlikely(IS_ERR(data->g_sensor_dev))) {
		err = PTR_ERR(data->g_sensor_dev);
		data->g_sensor_dev = NULL;

		PERR("could not allocate g_sensor_dev\n");
		goto err_create_g_sensor_device;
	}

	dev_set_drvdata(data->g_sensor_dev, data);

	err = sysfs_create_group(&data->g_sensor_dev->kobj,
			&smi130_acc_sig_motion_attribute_group);
	if (err < 0)
		goto error_sysfs;
#endif

#ifdef CONFIG_DOUBLE_TAP
	data->g_sensor_class_doubletap =
		class_create(THIS_MODULE, "dtap_sensor");
	if (IS_ERR(data->g_sensor_class_doubletap)) {
		err = PTR_ERR(data->g_sensor_class_doubletap);
		data->g_sensor_class_doubletap = NULL;
		PERR("could not allocate g_sensor_class_doubletap\n");
		goto err_create_class;
	}

	data->g_sensor_dev_doubletap = device_create(
				data->g_sensor_class_doubletap,
				NULL, 0, "%s", "g_sensor");
	if (unlikely(IS_ERR(data->g_sensor_dev_doubletap))) {
		err = PTR_ERR(data->g_sensor_dev_doubletap);
		data->g_sensor_dev_doubletap = NULL;

		PERR("could not allocate g_sensor_dev_doubletap\n");
		goto err_create_g_sensor_device_double_tap;
	}

	dev_set_drvdata(data->g_sensor_dev_doubletap, data);

	err = sysfs_create_group(&data->g_sensor_dev_doubletap->kobj,
			&smi130_acc_double_tap_attribute_group);
	if (err < 0)
		goto error_sysfs;
#endif

	err = sysfs_create_group(&data->input->dev.kobj,
			&smi130_acc_attribute_group);
	if (err < 0)
		goto error_sysfs;

	dev_acc = bosch_allocate_device();
	if (!dev_acc) {
		err = -ENOMEM;
		goto error_sysfs;
	}
	dev_acc->name = ACC_NAME;

	bosch_set_drvdata(dev_acc, data);

	err = bosch_register_device(dev_acc);
	if (err < 0)
		goto bosch_free_acc_exit;

	data->bosch_acc = dev_acc;
	err = sysfs_create_group(&data->bosch_acc->dev.kobj,
			&smi130_acc_attribute_group);

	if (err < 0)
		goto bosch_free_exit;

	if (NULL != client->dev.platform_data) {
		data->bosch_pd = kzalloc(sizeof(*data->bosch_pd),
				GFP_KERNEL);

		if (NULL != data->bosch_pd) {
			memcpy(data->bosch_pd, client->dev.platform_data,
					sizeof(*data->bosch_pd));
			PINFO("%s sensor driver set place: p%d",
				data->bosch_pd->name, data->bosch_pd->place);
		}
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = smi130_acc_early_suspend;
	data->early_suspend.resume = smi130_acc_late_resume;
	register_early_suspend(&data->early_suspend);
#endif
	INIT_WORK(&data->report_data_work,
	smi130_acc_timer_work_fun);
	reportdata_wq = create_singlethread_workqueue("smi130_acc_wq");
	if (NULL == reportdata_wq)
		PERR("fail to create the reportdta_wq");
	hrtimer_init(&data->timer, CLOCK_MONOTONIC,
		HRTIMER_MODE_REL);
	data->timer.function = reportdata_timer_fun;
	data->work_delay_kt = ns_to_ktime(4000000);
	data->is_timer_running = 0;
	data->timestamp = 0;
	data->time_odr = 4000000;/*default bandwidth 125HZ*/
	data->smi_acc_mode_enabled = 0;
	data->fifo_datasel = 0;
	data->fifo_count = 0;
	data->acc_count = 0;

#ifdef CONFIG_SIG_MOTION
	atomic_set(&data->en_sig_motion, 0);
#endif
#ifdef CONFIG_DOUBLE_TAP
	atomic_set(&data->en_double_tap, 0);
	data->tap_times = 0;
	data->tap_time_period = DEFAULT_TAP_JUDGE_PERIOD;
	mutex_init(&data->tap_mutex);
	setup_timer(&data->tap_timer, smi130_acc_tap_timeout_handle,
			(unsigned long)data);
#endif
	if (smi130_acc_set_mode(client, SMI_ACC2X2_MODE_SUSPEND, SMI_ACC_ENABLED_ALL) < 0)
		return -EINVAL;
	data->IRQ = client->irq;
	PDEBUG("data->IRQ = %d", data->IRQ);
	err = request_threaded_irq(data->IRQ, smi130_acc_irq_handler,
			smi130_acc_irq_work_func, IRQF_TRIGGER_RISING,
			"smi130_acc", data);

	smi130_hrtimer_init(data);
	err = smi130_acc_early_buff_init(client, data);
	if (!err)
		goto exit;

	PINFO("SMI130_ACC driver probe successfully");

	return 0;

bosch_free_exit:
	bosch_unregister_device(dev_acc);

bosch_free_acc_exit:
	bosch_free_device(dev_acc);

error_sysfs:
	input_unregister_device(data->input);

#ifdef CONFIG_DOUBLE_TAP
err_create_g_sensor_device_double_tap:
	class_destroy(data->g_sensor_class_doubletap);
#endif

#ifdef CONFIG_SIG_MOTION
err_create_g_sensor_device:
	class_destroy(data->g_sensor_class);
#endif

#if defined(CONFIG_SIG_MOTION) || defined(CONFIG_DOUBLE_TAP)
err_create_class:
	input_unregister_device(data->dev_interrupt);
#endif

err_register_input_device_interrupt:
	input_free_device(dev_interrupt);
	input_unregister_device(data->input);

err_register_input_device:
	input_free_device(dev);

kfree_exit:
	if ((NULL != data) && (NULL != data->bosch_pd)) {
		kfree(data->bosch_pd);
		data->bosch_pd = NULL;
	}
	kfree(data);
exit:
	return err;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void smi130_acc_early_suspend(struct early_suspend *h)
{
	struct smi130_acc_data *data =
		container_of(h, struct smi130_acc_data, early_suspend);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		smi130_acc_set_mode(data->smi130_acc_client,
			SMI_ACC2X2_MODE_SUSPEND, SMI_ACC_ENABLED_INPUT);
#ifndef CONFIG_SMI_ACC_ENABLE_NEWDATA_INT
		cancel_delayed_work_sync(&data->work);
#endif
	}
	if (data->is_timer_running) {
		/*diable fifo_mode when close timer*/
		if (smi130_acc_set_fifo_mode(data->smi130_acc_client, 0) < 0)
			PERR("set fifo_mode falied");
		hrtimer_cancel(&data->timer);
		data->base_time = 0;
		data->timestamp = 0;
		data->fifo_time = 0;
		data->acc_count = 0;
	}
	mutex_unlock(&data->enable_mutex);
}

static void smi130_acc_late_resume(struct early_suspend *h)
{
	struct smi130_acc_data *data =
		container_of(h, struct smi130_acc_data, early_suspend);
	if (NULL == data)
		return;

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		smi130_acc_set_mode(data->smi130_acc_client,
			SMI_ACC2X2_MODE_NORMAL, SMI_ACC_ENABLED_INPUT);
#ifndef CONFIG_SMI_ACC_ENABLE_NEWDATA_INT
		schedule_delayed_work(&data->work,
				msecs_to_jiffies(atomic_read(&data->delay)));
#endif
	}
	if (data->is_timer_running) {
		hrtimer_start(&data->timer,
					ns_to_ktime(data->time_odr),
			HRTIMER_MODE_REL);
		/*enable fifo_mode when init*/
		if (smi130_acc_set_fifo_mode(data->smi130_acc_client, 2) < 0)
			PERR("set fifo_mode falied");
		data->base_time = 0;
		data->timestamp = 0;
		data->is_timer_running = 1;
		data->acc_count = 0;
	}
	mutex_unlock(&data->enable_mutex);
}
#endif

static int smi130_acc_remove(struct i2c_client *client)
{
	struct smi130_acc_data *data = i2c_get_clientdata(client);

	if (NULL == data)
		return 0;

	smi130_hrtimer_cleanup(data);
	smi130_acc_input_cleanup(data);
	smi130_acc_set_enable(&client->dev, 0);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	sysfs_remove_group(&data->input->dev.kobj, &smi130_acc_attribute_group);
	input_unregister_device(data->input);

	if (NULL != data->bosch_pd) {
		kfree(data->bosch_pd);
		data->bosch_pd = NULL;
	}

	kfree(data);
	return 0;
}

void smi130_acc_shutdown(struct i2c_client *client)
{
	struct smi130_acc_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->enable_mutex);
	smi130_acc_set_mode(data->smi130_acc_client,
		SMI_ACC2X2_MODE_DEEP_SUSPEND, SMI_ACC_ENABLED_ALL);
	mutex_unlock(&data->enable_mutex);
}

#ifdef CONFIG_PM
static int smi130_acc_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct smi130_acc_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		smi130_acc_set_mode(data->smi130_acc_client,
			SMI_ACC2X2_MODE_SUSPEND, SMI_ACC_ENABLED_INPUT);
#ifndef CONFIG_SMI_ACC_ENABLE_NEWDATA_INT
		cancel_delayed_work_sync(&data->work);
#endif
	}
	if (data->is_timer_running) {
		hrtimer_cancel(&data->timer);
		data->base_time = 0;
		data->timestamp = 0;
		data->fifo_time = 0;
		data->acc_count = 0;
	}
	mutex_unlock(&data->enable_mutex);

	return 0;
}

static int smi130_acc_resume(struct i2c_client *client)
{
	struct smi130_acc_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		smi130_acc_set_mode(data->smi130_acc_client,
			SMI_ACC2X2_MODE_NORMAL, SMI_ACC_ENABLED_INPUT);
#ifndef CONFIG_SMI_ACC_ENABLE_NEWDATA_INT
		schedule_delayed_work(&data->work,
				msecs_to_jiffies(atomic_read(&data->delay)));
#endif
	}
	if (data->is_timer_running) {
		hrtimer_start(&data->timer,
					ns_to_ktime(data->time_odr),
			HRTIMER_MODE_REL);
		data->base_time = 0;
		data->timestamp = 0;
		data->is_timer_running = 1;
	}
	mutex_unlock(&data->enable_mutex);

	return 0;
}

#else

#define smi130_acc_suspend      NULL
#define smi130_acc_resume       NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id smi130_acc_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, smi130_acc_id);
static const struct of_device_id smi130_acc_of_match[] = {
	{ .compatible = "smi130_acc", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, smi130_acc_of_match);

static struct i2c_driver smi130_acc_driver = {
	.driver = {
		.owner  = THIS_MODULE,
		.name   = SENSOR_NAME,
		.of_match_table = smi130_acc_of_match,
	},
	.suspend    = smi130_acc_suspend,
	.resume     = smi130_acc_resume,
	.id_table   = smi130_acc_id,
	.probe      = smi130_acc_probe,
	.remove     = smi130_acc_remove,
	.shutdown   = smi130_acc_shutdown,
};

static int __init SMI_ACC2X2_init(void)
{
	return i2c_add_driver(&smi130_acc_driver);
}

static void __exit SMI_ACC2X2_exit(void)
{
	i2c_del_driver(&smi130_acc_driver);
}

MODULE_AUTHOR("contact@bosch-sensortec.com");
MODULE_DESCRIPTION("SMI_ACC2X2 ACCELEROMETER SENSOR DRIVER");
MODULE_LICENSE("GPL v2");

module_init(SMI_ACC2X2_init);
module_exit(SMI_ACC2X2_exit);

