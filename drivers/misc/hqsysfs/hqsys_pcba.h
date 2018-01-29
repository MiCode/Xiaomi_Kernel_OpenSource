#ifndef HQSYS_PCBA
#define HQSYS_PCBA


typedef enum {
	PCBA_UNKNOW = 0,
	PCBA_P0,
	PCBA_P1,
	PCBA_D2_MP1_CN,
	PCBA_D2_MP1_GLOBAL,
	PCBA_D2_MP1_IN,

	PCBA_C6A = 0x40,
	PCBA_C6A_CN,
	PCBA_C6A_IN,
	PCBA_C6A_GLOBAL,

	PCBA_END,


} PCBA_CONFIG;

struct pcba_info {
	PCBA_CONFIG pcba_config;
	char pcba_name[32];
};



PCBA_CONFIG get_huaqin_pcba_config(void);

#endif
