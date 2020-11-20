/* ************************************************************************
 *       Filename:  RTP_parser.h
 *    Description:  
 *        Version:  1.0
 *        Created:  04/22/2020 05:11:03 PM
 *       Revision:  none
 *       Compiler:  gcc
 *         Author:  YOUR NAME (), 
 *        Company:  
 * ************************************************************************/
#include "drv2624_parser_interfaces.h"
typedef struct {
	unsigned char fb_brake_factor:3;
	unsigned char auto_brake_standby:1;
	unsigned char auto_brake:1;
	unsigned char hybrid_loop:1;
	unsigned char reserve:2;
	unsigned char rated_Voltage:8;
	unsigned char overDrive_Voltage:8;
	unsigned char F0:8;
	unsigned short wav_number:16;
} RTP_head;

typedef struct {
	unsigned short offset;
	unsigned short length;
	unsigned short duration;
	unsigned char brake:1;
	unsigned char loop_mod:1;
	unsigned char shape:1;
	unsigned char :0;
	unsigned char reserve:8;
} wav_info;

typedef struct {
	unsigned short eff_id_open;
	wav_frame *pPair;
} open_eff;

typedef struct{
//	void *rtp_haed_begain;
	RTP_head *rtp_head_inf;
	wav_info *eff_inf_lst;
	open_eff running_eff;
} RTP_info;




