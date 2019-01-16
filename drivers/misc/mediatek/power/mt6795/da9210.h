/*****************************************************************************
*
* Filename:
* ---------
*   da9210.h
*
* Project:
* --------
*   Android
*
* Description:
* ------------
*   da9210 header file
*
* Author:
* -------
*
****************************************************************************/

#ifndef _da9210_SW_H_
#define _da9210_SW_H_

extern void da9210_dump_register(void);
extern kal_uint32 da9210_read_interface (kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT);
extern kal_uint32 da9210_config_interface (kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT);

#endif // _da9210_SW_H_

