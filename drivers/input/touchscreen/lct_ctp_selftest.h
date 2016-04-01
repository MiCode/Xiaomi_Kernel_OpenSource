#ifndef _LCT_CTP_SELFTEST_H
#define _LCT_CTP_SELFTEST_H

typedef  int (*CTP_SELFTEST_FUNC)(void);

void lct_ctp_selftest_int(CTP_SELFTEST_FUNC PTestfunc);

int lct_get_ctp_selttest_status(void);

#endif
