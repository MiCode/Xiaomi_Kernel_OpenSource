/* ************************************************************************
 *       Filename:  RTP_parser.c
 *    Description:  
 *        Version:  1.0
 *        Created:  04/22/2020 05:09:20 PM
 *       Revision:  none
 *       Compiler:  gcc
 *         Author:  YOUR NAME (), 
 *        Company:  
 * ************************************************************************/

//#include "drv2624_parser_interfaces.h"
#include "RTP_parser.h"
#include <linux/err.h>
#include <linux/kernel.h>
RTP_info rtp_info;
RTP_head rtp_head;
static void *instruction_end;
int rtp_init(void *file_buf, mod_prof *mod_cfg); //provide file whole buf, get mode configrations
int rtp_eff_open(int ID);
int rtp_eff_get_duration(int ID, unsigned int *dur);
int rtp_eff_get_cfg(int ID, wav_prof *wav_cfg);
int rtp_eff_read(int ID, wav_frame * frame); //one frame/read, return error at wav end
int rtp_eff_close(int ID);
int rtp_eff_get_length(int ID, unsigned int *dur);

void Add_RTP_parser(parser_interfaces *  fun_inf)
{
	fun_inf->init	 			=rtp_init;
	fun_inf->eff_open 			=rtp_eff_open;
	fun_inf->eff_get_duration	=rtp_eff_get_duration;
	fun_inf->eff_get_cfg		=rtp_eff_get_cfg;
	fun_inf->eff_read			=rtp_eff_read;
	fun_inf->eff_get_length		=rtp_eff_get_length;

}
int rtp_init(void *file_buf, mod_prof *mod_cfg)
{
	pr_info("%s:enter!\n", __func__);
	rtp_info.rtp_head_inf = file_buf;

	mod_cfg->auto_brake = rtp_info.rtp_head_inf->auto_brake;
	mod_cfg->auto_brake_standby =  rtp_info.rtp_head_inf->auto_brake_standby;
	mod_cfg->fb_brake_factor = rtp_info.rtp_head_inf->fb_brake_factor;
	mod_cfg->hybrid_loop = rtp_info.rtp_head_inf->hybrid_loop;
	mod_cfg->overDrive_Voltage = rtp_info.rtp_head_inf->overDrive_Voltage;
	mod_cfg->rated_Voltage = rtp_info.rtp_head_inf->rated_Voltage;
	mod_cfg->F0 = rtp_info.rtp_head_inf->F0;
	rtp_head.wav_number = rtp_info.rtp_head_inf->wav_number;
	rtp_info.eff_inf_lst = file_buf + sizeof(RTP_head);
	pr_info("%s:head size = %ld\n", __func__, sizeof(RTP_head));
	pr_info("%s: wav number = %d\n", __func__, rtp_info.rtp_head_inf->wav_number);
	pr_info("%s: length = %d\n", __func__, rtp_info.eff_inf_lst[0].length);
	pr_info("%s: duration = %d\n", __func__, rtp_info.eff_inf_lst[0].duration);
	pr_info("%s: offset  = %d\n", __func__, rtp_info.eff_inf_lst[0].offset);
	pr_info("%s:auto_brake = %d\n", __func__,  mod_cfg->auto_brake);
	pr_info("%s:mod_cfg->auto_brake_standby = %d\n", __func__,  mod_cfg->auto_brake_standby);
	pr_info("%s:mod_cfg->fb_brake_factor = %d\n", __func__, mod_cfg->fb_brake_factor);
	pr_info("%s:mod_cfg->hybrid_loop = %d\n", __func__,  mod_cfg->hybrid_loop);
	pr_info("%s:mod_cfg->F0 = %d\n", __func__,  mod_cfg->F0);
	pr_info("%s:mod_cfg->rated_Voltage = %d\n", __func__,  mod_cfg->rated_Voltage);
	pr_info("%s:mod_cfg->overDrive_Voltage = %d\n", __func__,  mod_cfg->overDrive_Voltage);
	return 0;
}
int rtp_eff_open(int ID)
{
	pr_info("%s:enter!\n", __func__);
	if(rtp_info.eff_inf_lst[ID].length == 0) {
		return DRV_PARSER_ERR_READ_OVER;
	}
	rtp_info.running_eff.eff_id_open = ID;
	rtp_info.running_eff.pPair = (void *)(rtp_info.rtp_head_inf) + rtp_info.eff_inf_lst[ID].offset;
	instruction_end = (void *)(rtp_info.rtp_head_inf) + rtp_info.eff_inf_lst[ID].offset + rtp_info.eff_inf_lst[ID].length*2;
	return SUCCESS;
}

int rtp_eff_get_duration(int ID, unsigned int *dur)
{
	pr_info("%s:enter!\n", __func__);
	if (ID != rtp_info.running_eff.eff_id_open)
		return DRV_PARSER_ERR_NOTOPEN;
	*dur = rtp_info.eff_inf_lst[ID].duration;
	return SUCCESS;
}
int rtp_eff_get_length(int ID, unsigned int *dur)
{
	pr_info("%s:enter!\n", __func__);
	if (ID != rtp_info.running_eff.eff_id_open)
		return DRV_PARSER_ERR_NOTOPEN;
	*dur = rtp_info.eff_inf_lst[ID].length;
	return 0;
}
int rtp_eff_get_cfg(int ID, wav_prof *wav_cfg)
{
	pr_info("%s:enter!\n", __func__);
	if (ID != rtp_info.running_eff.eff_id_open)
		return DRV_PARSER_ERR_NOTOPEN;
	wav_cfg->shape = rtp_info.eff_inf_lst[ID].shape;
	wav_cfg->loop_mod = rtp_info.eff_inf_lst[ID].loop_mod;
	wav_cfg->braking = rtp_info.eff_inf_lst[ID].brake;
	pr_info("%s: shape:%d , loop_mod=%d, braking=%d\n", __func__, wav_cfg->shape, wav_cfg->loop_mod, wav_cfg->braking);
	return SUCCESS;
}

int rtp_eff_read(int ID, wav_frame *frame)
{

	pr_info("%s:enter!\n", __func__);
	if (ID != rtp_info.running_eff.eff_id_open)
		return DRV_PARSER_ERR_NOTOPEN;
	if((void *)rtp_info.running_eff.pPair >= instruction_end)
		return	DRV_PARSER_ERR_READ_OVER;
	*frame = *rtp_info.running_eff.pPair;//
	rtp_info.running_eff.pPair++;
	return SUCCESS;
}//one frame/read, return error at wav end


