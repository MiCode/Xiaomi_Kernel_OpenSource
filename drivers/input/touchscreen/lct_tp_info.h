/****************************************************************************************
 *
 * @File Name   : lct_tp_info.h
 * @Author      : wanghan
 * @E-mail      : <wanghan@longcheer.com>
 * @Create Time : 2018-08-17 17:34:43
 * @Description : Display touchpad information.
 *
 ****************************************************************************************/

#ifndef __LCT_TP_INFO_H__
#define __LCT_TP_INFO_H__

#define TP_CALLBACK_CMD_INFO      "CMD_INFO"
#define TP_CALLBACK_CMD_LOCKDOWN  "CMD_LOCKDOWN"

//Initialization /proc/tp_info & /proc/tp_lockdown_info node
extern int init_lct_tp_info(char *tp_info_buf, char *tp_lockdown_info_buf);
//Update /proc/tp_info & /proc/tp_lockdown_info node
extern void update_lct_tp_info(char *tp_info_buf, char *tp_lockdown_info_buf);
//Set tp_info node callback funcation
extern void set_lct_tp_info_callback(int (*pfun)(const char *));

#endif //__LCT_TP_INFO_H__
