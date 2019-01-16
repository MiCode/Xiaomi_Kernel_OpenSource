/*****************************************************************************
*
* Filename:
* ---------
*   sn65dsi83.h
*
* Project:
* --------
*   Android
*
* Description:
* ------------
*   sn65dsi83 header file
*
* Author:
* -------
*
****************************************************************************/

#ifndef _sn65dsi83_SW_H_
#define _sn65dsi83_SW_H_
#ifndef BUILD_LK

//---------------------------------------------------------
extern int sn65dsi83_read_byte(kal_uint8 cmd, kal_uint8 *returnData);
extern int sn65dsi83_write_byte(kal_uint8 cmd, kal_uint8 writeData);
#endif
#endif // _fan5405_SW_H_

