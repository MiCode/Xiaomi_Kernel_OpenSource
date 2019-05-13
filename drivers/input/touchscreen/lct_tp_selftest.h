#ifndef _LCT_CTP_SELFTEST_H
#define _LCT_CTP_SELFTEST_H

#define TP_FELF_TEST_RETROY_COUNT        3
#define TP_SELF_TEST_CHECK_STATE_COUNT   30

#define TP_SELF_TEST_PROC_FILE           "tp_selftest"

#define TP_SELF_TEST_TESTING             "Testing"
#define TP_SELF_TEST_RESULT_UNKNOW       "0\n"
#define TP_SELF_TEST_RESULT_FAIL         "1\n"
#define TP_SELF_TEST_RESULT_PASS         "2\n"

#define TP_SELF_TEST_LONGCHEER_MMI_CMD   "mmi"
#define TP_SELF_TEST_XIAOMI_I2C_CMD      "i2c"
#define TP_SELF_TEST_XIAOMI_OPEN_CMD     "open"
#define TP_SELF_TEST_XIAOMI_SHORT_CMD    "short"

enum lct_tp_selftest_cmd {
	TP_SELFTEST_CMD_LONGCHEER_MMI = 0x00,
	TP_SELFTEST_CMD_XIAOMI_I2C = 0x01,
	TP_SELFTEST_CMD_XIAOMI_OPEN = 0x02,
	TP_SELFTEST_CMD_XIAOMI_SHORT = 0x03,
};


typedef int (*tp_selftest_callback_t)(unsigned char cmd);


extern void lct_tp_selftest_init(tp_selftest_callback_t callback);

#endif

