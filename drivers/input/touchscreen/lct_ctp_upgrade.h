#ifndef _LCT_CTP_UPGRADE_H
#define _LCT_CTP_UPGRADE_H

typedef  int (*CTP_UPGRADE_FUNC)(void);
typedef void (*CTP_READ_VERSION)(char* version);




void lct_ctp_upgrade_int(CTP_UPGRADE_FUNC PUpdatefunc, CTP_READ_VERSION PVerfunc);

int lct_set_ctp_upgrade_status(char * status);


#endif