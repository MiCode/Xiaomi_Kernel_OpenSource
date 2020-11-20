#include "drv2624_parser_interfaces.h"
mod_prof def_mod_cfg = {
	// unsigned char hybrid_loop;
	0,
	// unsigned char auto_brake;
	1,
	// unsigned char auto_brake_standby;
	1,
	// unsigned char fb_brake_factor;
	3,
	// unsigned char rated_Voltage;
	81,
	// unsigned char overDrive_Voltage;
	136,
	// unsigned char F0;
	208,
};
wav_prof def_wav_cfg = {
	// unsigned char shape;
	1,
	// unsigned char loop_mod;
	1,
	// unsigned char braking;
	1,
};
mod_prof* get_def_mod_cfg(void) {
	return &def_mod_cfg;
}

wav_prof* get_def_wav_cfg(void) {
	return &def_wav_cfg;
}

/*
void Add_def_param_parser(parser_interfaces *  fun_inf) {
}*/
