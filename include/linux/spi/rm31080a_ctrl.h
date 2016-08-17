#ifndef _RM31080A_CTRL_H_
#define _RM31080A_CTRL_H_

#define RM31080_REG_11 0x11
#define RM31080_REG_1F 0x1F

#define FILTER_THRESHOLD_MODE                      0x08
#define FILTER_NONTHRESHOLD_MODE                   0x00

#define REPEAT_1                                   0x00

struct rm31080a_ctrl_para {

	unsigned short u16DataLength;
	unsigned short bICVersion;
	unsigned short bChannelNumberX;
	unsigned short bChannelNumberY;
	unsigned short bADCNumber;
	unsigned short u16ResolutionX;
	unsigned short u16ResolutionY;

	unsigned char bActiveRepeatTimes[2];
	unsigned char bIdleRepeatTimes[2];
	unsigned char bSenseNumber;
	unsigned char bfTHMode;
	unsigned char bTime2Idle;
	unsigned char bfPowerMode;
	unsigned char bDebugMessage;
	unsigned char bTimerTriggerScale;
	unsigned char bDummyRunCycle;
	unsigned char bfIdleModeCheck;
};

extern struct rm31080a_ctrl_para g_stCtrl;

int rm_tch_ctrl_clear_int(void);
int rm_tch_ctrl_scan_start(void);
void rm_tch_ctrl_wait_for_scan_finish(void);

void rm_tch_ctrl_init(void);
unsigned char rm_tch_ctrl_get_idle_mode (unsigned char *p);
void rm_tch_ctrl_get_parameter(void *arg);
void rm_set_repeat_times(u8 u8Times);
#endif				/*_RM31080A_CTRL_H_*/
