#ifndef _LCT_IF_CTP_UPGRADE_H
#define _LCT_IF_CTP_UPGRADE_H

typedef  int (*CTP_UPGRADE_FUNC)(void);
typedef void (*CTP_READ_VERSION)(char *version);




void lct_if_ctp_upgrade_int(CTP_UPGRADE_FUNC PUpdatefunc, CTP_READ_VERSION PVerfunc, CTP_READ_VERSION POldVerfunc);

int lct_set_if_ctp_upgrade_status(char *status);


#endif