#ifndef FREQ_WDG_H
#define FREQ_WDG_H
struct mi_freq_qos
{
	unsigned long magic_num;
	unsigned int wdg;
};

#define MAX_FREQ_QOS_REQ	100
#define GET_FREQ_ERR 		0

#define MAX_CLUSTER			3
#define MAX_UID_VALUE		(1 << 20)
#define USED_MI_FREQ_QOS	0xabcddcba
#endif
