#ifndef _LCT_CTP_SELFTEST_H
#define _LCT_CTP_SELFTEST_H

//define touchpad self-test callback type
typedef  int (*CTP_SELFTEST_FUNC)(void);

//set touchpad self-test callback funcation
extern void lct_ctp_selftest_int(CTP_SELFTEST_FUNC PTestfunc);

//get touchpad self-test status
// 0 - ready
// 1 - testing
extern int lct_get_ctp_selttest_status(void);

#endif

