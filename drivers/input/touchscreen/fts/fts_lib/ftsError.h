/*

**************************************************************************
**                        STMicroelectronics		                **
**************************************************************************
**                        marco.cali@st.com				**
**************************************************************************
*                                                                        *
*                  FTS error/info kernel log reporting			 *
*                                                                        *
**************************************************************************
**************************************************************************

*/


#define OK								((int)0x00000000)
#define ERROR_ALLOC						((int)0x80000001)
#define ERROR_I2C_R						((int)0x80000002)
#define ERROR_I2C_W						((int)0x80000003)
#define ERROR_I2C_WR					((int)0x80000004)
#define ERROR_I2C_O						((int)0x80000005)
#define ERROR_OP_NOT_ALLOW				((int)0x80000006)
#define ERROR_TIMEOUT					((int)0x80000007)
#define ERROR_FILE_NOT_FOUND			((int)0x80000008)
#define ERROR_FILE_PARSE				((int)0x80000009)
#define ERROR_FILE_READ					((int)0x8000000A)
#define ERROR_LABEL_NOT_FOUND			((int)0x8000000B)
#define ERROR_FW_NO_UPDATE				((int)0x8000000C)
#define ERROR_FLASH_UNKNOWN				((int)0x8000000D)
#define ERROR_DISABLE_INTER				((int)0x80000200)
#define ERROR_ENABLE_INTER				((int)0x80000300)
#define ERROR_READ_B2					((int)0x80000400)
#define ERROR_GET_OFFSET				((int)0x80000500)
#define ERROR_GET_FRAME_DATA			((int)0x80000600)
#define ERROR_DIFF_COMP_TYPE			((int)0x80000700)
#define ERROR_WRONG_COMP_SIGN			((int)0x80000800)
#define ERROR_SENSE_ON_FAIL				((int)0x80000900)
#define ERROR_SENSE_OFF_FAIL			((int)0x80000A00)
#define ERROR_SYSTEM_RESET_FAIL			((int)0x80000B00)
#define ERROR_FLASH_NOT_READY			((int)0x80000C00)
#define ERROR_FW_VER_READ				((int)0x80000D00)
#define ERROR_GESTURE_ENABLE_FAIL		((int)0x80000E00)
#define ERROR_GESTURE_START_ADD			((int)0x80000F00)
#define ERROR_GESTURE_FINISH_ADD		((int)0x80001000)
#define ERROR_GESTURE_DATA_ADD			((int)0x80001100)
#define ERROR_GESTURE_REMOVE			((int)0x80001200)
#define ERROR_FEATURE_ENABLE_DISABLE	((int)0x80001300)
#define ERROR_CH_LEN					((int)0x80010000)
#define ERROR_REQU_COMP_DATA			((int)0x80020000)
#define ERROR_COMP_DATA_HEADER			((int)0x80030000)
#define ERROR_COMP_DATA_GLOBAL			((int)0x80040000)
#define ERROR_COMP_DATA_NODE			((int)0x80050000)
#define ERROR_TEST_CHECK_FAIL			((int)0x80060000)
#define ERROR_MEMH_READ					((int)0x80070000)
#define ERROR_FLASH_BURN_FAILED			((int)0x80080000)
#define ERROR_MS_TUNING					((int)0x80090000)
#define ERROR_SS_TUNING					((int)0x800A0000)
#define ERROR_LP_TIMER_TUNING			((int)0x800B0000)
#define ERROR_SAVE_CX_TUNING			((int)0x800C0000)
#define ERROR_HANDLER_STOP_PROC			((int)0x800D0000)
#define ERROR_CHECK_ECHO_FAIL			((int)0x800E0000)
#define ERROR_NOISE_PARAMETERS          ((int)0x80001400)
#define ERROR_PROD_TEST_DATA			((int)0x81000000)
#define ERROR_PROD_TEST_DATA_MS_RAW		((int)0x81000001)
#define ERROR_PROD_TEST_DATA_KEY_RAW	((int)0x81000002)
#define ERROR_PROD_TEST_DATA_MS_CX	    ((int)0x81000003)
#define ERROR_PROD_TEST_DATA_MS_KEY_CX	    ((int)0x81000004)
#define ERROR_PROD_TEST_DATA_SS_RAW		((int)0x81000005)
#define ERROR_PROD_TEST_DATA_SS_IX_CX	((int)0x81000006)
#define ERROR_FLASH_PROCEDURE			((int)0x82000000)
#define ERROR_PROD_TEST_ITO				((int)0x83000000)
#define ERROR_PROD_TEST_INITIALIZATION	((int)0x84000000)
#define ERROR_GET_INIT_STATUS			((int)0x85000000)
#define ERROR_LOCKDOWN_CODE             ((int)0x80001500)
#define log_debug(fmt, args...) pr_debug("" fmt, ## args)
#define log_error(fmt, args...) printk(KERN_ERR "" fmt, ## args)

int isI2cError(int error);
int dumpErrorInfo(void);
int errorHandler(u8 *event, int size);
