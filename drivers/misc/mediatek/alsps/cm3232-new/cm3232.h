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
 * Definitions for CM3232 als/ps sensor chip.
 */
#ifndef __CM3232_H__
#define __CM3232_H__

#include <linux/ioctl.h>

/*cm3232 als/ps sensor register related macro*/
#define CM3232_REG_ALS_CONF 		0X00
#define CM3232_REG_ALS_THDH 		0X01
#define CM3232_REG_ALS_THDL 		0X02
#define CM3232_REG_PS_CONF1_2		0X03
#define CM3232_REG_PS_CONF3_MS		0X04
#define CM3232_REG_PS_CANC			0X05
#define CM3232_REG_PS_THD			0X06
#define CM3232_REG_PS_DATA			0X08
#define CM3232_REG_ALS_DATA		0X50
#define CM3232_REG_INT_FLAG		0X0B
#define CM3232_REG_ID_MODE			0X0C

/*CM3232 related driver tag macro*/
#define CM3232_SUCCESS				 		 0
#define CM3232_ERR_I2C						-1
#define CM3232_ERR_STATUS					-3
#define CM3232_ERR_SETUP_FAILURE			-4
#define CM3232_ERR_GETGSENSORDATA			-5
#define CM3232_ERR_IDENTIFICATION			-6

#define ALS_K_VALUE 	20

/*----------------------------------------------------------------------------*/
typedef enum{
    CM3232_NOTIFY_PROXIMITY_CHANGE = 1,
}CM3232_NOTIFY_TYPE;
/*----------------------------------------------------------------------------*/
typedef enum{
    CM3232_CUST_ACTION_SET_CUST = 1,
    CM3232_CUST_ACTION_CLR_CALI,
    CM3232_CUST_ACTION_SET_CALI,
    CM3232_CUST_ACTION_SET_PS_THRESHODL,
    CM3232_CUST_ACTION_SET_EINT_INFO,
    CM3232_CUST_ACTION_GET_ALS_RAW_DATA,
    CM3232_CUST_ACTION_GET_PS_RAW_DATA,
}CM3232_CUST_ACTION;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
}CM3232_CUST;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
    uint16_t    part;
    int32_t    data[0];
}CM3232_SET_CUST;
/*----------------------------------------------------------------------------*/
typedef CM3232_CUST CM3232_CLR_CALI;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
    int32_t     cali;
}CM3232_SET_CALI;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
    int32_t     threshold[2];
}CM3232_SET_PS_THRESHOLD;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
    uint32_t    gpio_pin;
    uint32_t    gpio_mode;
    uint32_t    eint_num;
    uint32_t    eint_is_deb_en;
    uint32_t    eint_type;
}CM3232_SET_EINT_INFO;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
    uint16_t    als;
}CM3232_GET_ALS_RAW_DATA;
/*----------------------------------------------------------------------------*/
typedef struct
{
    uint16_t    action;
    uint16_t    ps;
} CM3232_GET_PS_RAW_DATA;
/*----------------------------------------------------------------------------*/
typedef union
{
    uint32_t                    data[10];
    CM3232_CUST                cust;
    CM3232_SET_CUST            setCust;
    CM3232_CLR_CALI            clearCali;
    CM3232_SET_CALI            setCali;
    CM3232_SET_PS_THRESHOLD    setPSThreshold;
    CM3232_SET_EINT_INFO       setEintInfo;
    CM3232_GET_ALS_RAW_DATA    getALSRawData;
    CM3232_GET_PS_RAW_DATA     getPSRawData;
}CM3232_CUST_DATA;
/*----------------------------------------------------------------------------*/

#endif

