#ifndef HQSYS_PCBA
#define HQSYS_PCBA

typedef enum {
	PCBA_UNKNOW = 0,
	PCBA_END,
} PCBA_CONFIG;

int get_huaqin_pcba_config(void);
char *get_huaqin_pcba_str(void);

#endif
