/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name			: tp_product_id_def.h
* Author			: Bob Huang
* Version			: V1.0.0
* Date				: 12/31/2017
* Description		: touch panel product id define
*******************************************************************************/
#ifndef TP_PRODUCT_ID_DEF_H
#define TP_PRODUCT_ID_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

/*//this file must be used in arm code
//define chip main type
//we plan use 2 bytes store it,so we can distinguish 65536 chip main types.*/
#define TP_GT900		0x0000
#define TP_GT9P			0x0001
#define TP_GT9L			0x0002
#define TP_GT9PT		0x0003
#define TP_BOSTON		0x0004
#define TP_NANJING		0x0005
#define TP_NORMANDY		0x0006
#define TP_ALTO			0x0007
#define TP_PHOENIX		0x0008
#define TP_OSLO         0x0009
#define TP_NORMANDY_L	0x000A

/*//we plan use two bytes store it,so we can distinguish 65536 chip sub types.*/
/*//TP_GT900 sub type*/
#define TP_GT915	0x0000
#define TP_GT9157	0x0001

/*//TP_GT9P sub type*/
#define TP_GT1151	0x0000
#define TP_GT9286	0x0001

/*//TP_GT9L sub type*/
#define TP_GT5668	0x0000
#define TP_GT5663	0x0001
#define TP_GT5688	0x0002

/*//TP_GT9PT sub type*/
#define TP_GT1158	0x0000
#define TP_GT7288	0x0001

/*//TP_NORMANDY sub type*/
#define TP_GT9886	0x0001	/*//normal iic*/
#define TP_GT9886S	0x0002	/*//GT9886S spi*/

/*//TP_PHOENIX sub type*/
#define TP_GT7387P  0x0001
#define TP_GT7385P  0x0002
#define TP_GT7389   0x0003
#define TP_GT7387   0x0004
#define TP_GT7385   0x0005

/*//TP_OSLO sub type*/
#define TP_GT6861    0x0001
#define TP_GT6861P   0x0002

#ifdef __cplusplus
}
#endif
#endif
