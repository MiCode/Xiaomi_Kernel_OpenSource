/* ************************************************************************
 *       Filename:  drv2624_parser_interfaces.h
 *    Description:  
 *        Version:  1.0
 *        Created:  04/22/2020 05:10:24 PM
 *       Revision:  none
 *       Compiler:  gcc
 *         Author:  YOUR NAME (), 
 *        Company:  
 * ************************************************************************/

#define DRV_INTERFACE_VER 1.0.2020.4.22
#define DRV_PARSER_ERR_NOTOPEN -1 //effect ID is not open
#define DRV_PARSER_ERR_EMPTY -2 //reach the end of effect wave data
#define DRV_PARSER_ERR_READ_OVER -3 //effect ID is empty
#define INVALID 1
#define SUCCESS 0
typedef struct {
	unsigned char hybrid_loop;
	unsigned char auto_brake;
	unsigned char auto_brake_standby;
	unsigned char fb_brake_factor;
	unsigned char rated_Voltage;
	unsigned char overDrive_Voltage;
	unsigned char F0;
} mod_prof;

typedef struct {
	unsigned char shape;
	unsigned char loop_mod;
	unsigned char braking;
} wav_prof;

typedef struct {
	unsigned char gain;
	unsigned char step;
} wav_frame;

typedef struct {
	unsigned char duration;
	unsigned char length;
	unsigned char offset;
} wav_msg;
typedef struct {
	int (*init)(void *file_buf, mod_prof *mod_cfg); //provide file whole buf, get mode configrations
	int (*eff_open)(int ID);
	int (*eff_get_duration)(int ID, unsigned int *dur);
	int (*eff_get_length)(int ID, unsigned int *dur);
	int (*eff_get_cfg)(int ID, wav_prof * wav_cfg);
	int (*eff_read)(int ID, wav_frame * frame); //one frame/read, return error at wav end
	int (*eff_close)(int ID);
} parser_interfaces;
//RTP parser entry
//void Add_RAM_parser(parser_interfaces *  fun_inf);
//RAM parser entry
void Add_RTP_parser(parser_interfaces *  fun_inf);
mod_prof *get_def_mod_cfg(void);
wav_prof *get_def_wav_cfg(void);
//Constant parser entry, virtual parser
//int Add_CONSTANT_parser(parser_interface *  fun_inf);



