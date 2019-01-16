/* 
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
 * Definitions for CM36652 als/ps sensor chip.
 */
#ifndef __CM36652_H__
#define __CM36652_H__

#include <linux/ioctl.h>

/*cm36652 als/ps sensor register related macro*/
#define CM36652_REG_CS_CONF 		0X00
#define CM36652_REG_ALS_THDH 		0X01
#define CM36652_REG_ALS_THDL 		0X02
#define CM36652_REG_PS_CONF1_2		0X03
#define CM36652_REG_PS_THD			0X05
#define CM36652_REG_PS_CANC			0X06
#define CM36652_REG_PS_DATA			0X07
#define CM36652_REG_ALS_DATA		0X09
#define CM36652_REG_INT_FLAG		0X0C
#define CM36652_REG_ID_MODE			0X0D

/*CM36652 related driver tag macro*/
#define CM36652_SUCCESS				 		 0
#define CM36652_ERR_I2C						-1
#define CM36652_ERR_STATUS					-3
#define CM36652_ERR_SETUP_FAILURE			-4
#define CM36652_ERR_GETGSENSORDATA			-5
#define CM36652_ERR_IDENTIFICATION			-6

/*----------------------------------------------------------------------------*/
typedef enum{
    CM36652_NOTIFY_PROXIMITY_CHANGE = 1,
}CM36652_NOTIFY_TYPE;
/*----------------------------------------------------------------------------*/
typedef enum{
    CM36652_CUST_ACTION_SET_CUST = 1,
    CM36652_CUST_ACTION_CLR_CALI,
    CM36652_CUST_ACTION_SET_CALI,
    CM36652_CUST_ACTION_SET_PS_THRESHODL,
    CM36652_CUST_ACTION_SET_EINT_INFO,
    CM36652_CUST_ACTION_GET_ALS_RAW_DATA,
    CM36652_CUST_ACTION_GET_PS_RAW_DATA,
}CM36652_CUST_ACTION;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
}CM36652_CUST;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
    uint16_t    part;
    int32_t    data[0];
}CM36652_SET_CUST;
/*----------------------------------------------------------------------------*/
typedef CM36652_CUST CM36652_CLR_CALI;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
    int32_t     cali;
}CM36652_SET_CALI;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
    int32_t     threshold[2];
}CM36652_SET_PS_THRESHOLD;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
    uint32_t    gpio_pin;
    uint32_t    gpio_mode;
    uint32_t    eint_num;
    uint32_t    eint_is_deb_en;
    uint32_t    eint_type;
}CM36652_SET_EINT_INFO;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
    uint16_t    als;
}CM36652_GET_ALS_RAW_DATA;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
    uint16_t    ps;
} CM36652_GET_PS_RAW_DATA;
/*----------------------------------------------------------------------------*/
typedef union
{
    uint32_t                    data[10];
    CM36652_CUST                cust;
    CM36652_SET_CUST            setCust;
    CM36652_CLR_CALI            clearCali;
    CM36652_SET_CALI            setCali;
    CM36652_SET_PS_THRESHOLD    setPSThreshold;
    CM36652_SET_EINT_INFO       setEintInfo;
    CM36652_GET_ALS_RAW_DATA    getALSRawData;
    CM36652_GET_PS_RAW_DATA     getPSRawData;
}CM36652_CUST_DATA;
/*----------------------------------------------------------------------------*/

#endif

