/*
**************************************************************************
**				STMicroelectronics
**************************************************************************
**				marco.cali@st.com
**************************************************************************
*
*		  FTS error/info kernel log reporting
*
**************************************************************************
**************************************************************************
*/


/*FIRST LEVEL ERROR CODE*/
#define OK			((int)0x00000000)		/*No ERROR*/
#define ERROR_ALLOC		((int)0x80000001)		/*allocation of memory failed*/
#define ERROR_I2C_R		((int)0x80000002)		/*i2c read failed*/
#define ERROR_I2C_W		((int)0x80000003)		/*i2c write failed*/
#define ERROR_I2C_WR		((int)0x80000004)		/*i2c write/read failed*/
#define ERROR_I2C_O		((int)0x80000005)		/*error during opening a i2c device*/
#define ERROR_OP_NOT_ALLOW	((int)0x80000006)	/*operation not allowed*/
#define ERROR_TIMEOUT		((int)0x80000007)		/*timeout expired! exceed the max number of retries or the max waiting time*/
#define ERROR_FILE_NOT_FOUND	((int)0x80000008)	/*the file that i want to open is not found*/
#define ERROR_FILE_PARSE	((int)0x80000009)	/*error during parsing the file*/
#define ERROR_FILE_READ		((int)0x8000000A)		/*error during reading the file*/
#define ERROR_LABEL_NOT_FOUND	((int)0x8000000B)	/*label not found*/
#define ERROR_FW_NO_UPDATE	((int)0x8000000C)	/*fw in the chip newer than the one in the memmh*/
#define ERROR_FLASH_UNKNOWN	((int)0x8000000D)	/*flash status busy or unknown*/

/*SECOND LEVEL ERROR CODE*/
#define ERROR_DISABLE_INTER	((int)0x80000200)	/*unable to disable the interrupt*/
#define ERROR_ENABLE_INTER	((int)0x80000300)	/*unable to activate the interrup*/
#define ERROR_READ_B2		((int)0x80000400)		/*B2 command failed*/
#define ERROR_GET_OFFSET	((int)0x80000500)	/*unable to read an offset from memory*/
#define ERROR_GET_FRAME_DATA	((int)0x80000600)	/*unable to retrieve the data of a required frame*/
#define ERROR_DIFF_COMP_TYPE	((int)0x80000700)	/*FW answers with an event that has a different address respect the request done*/
#define ERROR_WRONG_COMP_SIGN	((int)0x80000800)	/*the signature of the compensation data is not A5*/
#define ERROR_SENSE_ON_FAIL	((int)0x80000900)	/*the command Sense On failed*/
#define ERROR_SENSE_OFF_FAIL	((int)0x80000A00)	/*the command Sense Off failed*/
#define ERROR_SYSTEM_RESET_FAIL	((int)0x80000B00)	/*the command SYSTEM RESET failed*/
#define ERROR_FLASH_NOT_READY	((int)0x80000C00)	/*flash status not ready within a timeout*/
#define ERROR_FW_VER_READ	((int)0x80000D00)	/*unable to retrieve fw_vers or the config_id*/
#define ERROR_GESTURE_ENABLE_FAIL	((int)0x80000E00)	/*unable to enable/disable the gesture*/
#define ERROR_GESTURE_START_ADD	((int)0x80000F00)	/*unable to start to add custom gesture*/
#define ERROR_GESTURE_FINISH_ADD	((int)0x80001000)	/*unable to finish to add custom gesture*/
#define ERROR_GESTURE_DATA_ADD	((int)0x80001100)	/*unable to add custom gesture data*/
#define ERROR_GESTURE_REMOVE	((int)0x80001200)	/*unable to remove custom gesture data*/
#define ERROR_FEATURE_ENABLE_DISABLE	((int)0x80001300)	/*unable to enable/disable a feature mode in the IC*/
/*THIRD LEVEL ERROR CODE*/
#define ERROR_CH_LEN		((int)0x80010000)		/*unable to retrieve the force and/or sense length*/
#define ERROR_REQU_COMP_DATA	((int)0x80020000)	/*compensation data request failed*/
#define ERROR_COMP_DATA_HEADER	((int)0x80030000)	/*unable to retrieve the compensation data	header*/
#define ERROR_COMP_DATA_GLOBAL	((int)0x80040000)	/*unable to retrieve the global compensation data*/
#define ERROR_COMP_DATA_NODE	((int)0x80050000)	/*unable to retrieve the compensation data for each node*/
#define ERROR_TEST_CHECK_FAIL	((int)0x80060000)	/*check of production limits or of fw answers failed*/
#define ERROR_MEMH_READ	((int)0x80070000)		/*memh reading failed*/
#define ERROR_FLASH_BURN_FAILED	((int)0x80080000)	/*flash burn failed*/
#define ERROR_MS_TUNING	((int)0x80090000)		/*ms tuning failed*/
#define ERROR_SS_TUNING	((int)0x800A0000)		/*ss tuning failed*/
#define ERROR_LP_TIMER_TUNING	((int)0x800B0000)	/*lp timer calibration failed*/
#define ERROR_SAVE_CX_TUNING	((int)0x800C0000)	/*save cx data to flash failed*/
#define ERROR_HANDLER_STOP_PROC	((int)0x800D0000)	/*stop the poll of the FIFO if particular errors are found*/
#define ERROR_CHECK_ECHO_FAIL	((int)0x800E0000)	/*unable to retrieve echo event*/

/*FOURTH LEVEL ERROR CODE*/
#define ERROR_PROD_TEST_DATA	((int)0x81000000)	/*production data test failed*/
#define ERROR_FLASH_PROCEDURE	((int)0x82000000)	/*complete flash procedure failed*/
#define ERROR_PROD_TEST_ITO	((int)0x83000000)	/*production ito test failed*/
#define ERROR_PROD_TEST_INITIALIZATION	((int)0x84000000)	/*production initialization test failed*/
#define ERROR_GET_INIT_STATUS	((int)0x85000000)	/*mismatch of the MS or SS tuning_version*/

void logError(int force, const char *msg, ...);
int isI2cError(int error);
int errorHandler(u8 *event, int size);
