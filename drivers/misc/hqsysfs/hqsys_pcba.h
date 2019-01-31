#ifndef HQSYS_PCBA
#define HQSYS_PCBA


typedef enum
{
	PCBA_UNKNOW=0,
	PCBA_F9_V1_CN,
	PCBA_F9_V2_CN,
	PCBA_F9_V3_CN,
	PCBA_F9_V4_CN,
	PCBA_END,

}PCBA_CONFIG;

extern PCBA_CONFIG huaqin_pcba_config;

struct pcba_info
{
	PCBA_CONFIG pcba_config;
	char pcba_name[32];
};

PCBA_CONFIG get_huaqin_pcba_config(void);

#endif
