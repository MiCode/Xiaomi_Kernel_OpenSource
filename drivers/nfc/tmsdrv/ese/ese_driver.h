/*****************************************************************************************
 * Copyright (c)  2021- 2022  TsingTeng MicroSystem Corp.Ltd.
 * TMS_EDIT
 * File       : ese_driver.h
 * Description: Source file for tms ese driver
 * Version    : 1.0
 * Date       : 2022/4/11
 * Team       : NFC Middleware
 * Author     : Guoliang Wu
 * --------------------------------- Revision History: ---------------------------------
 *   <version>    <date>          < author >                            <desc>
 *******************************************************************************************/
#ifndef _TMS_ESE_THN31_H_
#define _TMS_ESE_THN31_H_

#include "ese_common.h"

/*********** PART1: Define Area ***********/
//#define ESE_DEVICE               "tms,ese"
#define ESE_DEVICE               "nxp,p61"

#define ESE_MAX_BUFFER_SIZE      (4096)
#define ESE_CMD_RSP_TIMEOUT_MS   (2000)
#define ESE_MAGIC                (0xEA)
#define ESE_SET_STATE            _IOW(ESE_MAGIC, 0x04, long)

#define ESE_ENBLE_SPI_CLK     _IOW(ESE_MAGIC, 0x0D, long)
#define ESE_DISABLE_SPI_CLK     _IOW(ESE_MAGIC, 0x0E, long)

enum ese_ioctl_request_table {
    ESE_POWER_OFF     = 11,  /* ESE power off with ven low */
    ESE_POWER_ON      = 10,  /* ESE power on with ven high */
    ESE_COS_DWNLD_OFF = 12,  /* ESE firmware download gpio low */
    ESE_COS_DWNLD_ON  = 13,  /* ESE firmware download gpio high */
};

/*********** PART2: Struct Area ***********/

/*********** PART3: Function or variables for other files ***********/
int ese_driver_init(void);
void ese_driver_exit(void);
#endif /* _TMS_ESE_THN31_H_ */
