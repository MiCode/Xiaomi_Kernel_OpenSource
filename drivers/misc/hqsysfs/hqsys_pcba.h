#ifndef HQSYS_PCBA
#define HQSYS_PCBA


typedef enum
{
   PCBA_UNKNOW = 0,

   PCBA_V1_CN = 0x20,
   PCBA_V1_SA,
   PCBA_V1_GLOBAL,
   PCBA_V1_IN,

   PCBA_V2_CN,
   PCBA_V2_GLOBAL,
   PCBA_V2_IN,

   PCBA_E7 = 0x80,
   PCBA_E7_CN,
   PCBA_E7_IN,
   PCBA_E7_GLOBAL,

   PCBA_END,


}PCBA_CONFIG;

struct pcba_info
{
	PCBA_CONFIG pcba_config;
	char pcba_name[32];
};



PCBA_CONFIG get_huaqin_pcba_config(void);

#endif
