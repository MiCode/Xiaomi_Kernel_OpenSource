/*

**************************************************************************
**                        STMicroelectronics		                **
**************************************************************************
**                        marco.cali@st.com				**
**************************************************************************
*                                                                        *
*                     FTS Utility Functions				 *
*                                                                        *
**************************************************************************
**************************************************************************

*/
#define GPIO_NOT_DEFINED		-1

#define TIMEOUT_RESOLUTION		10
#define GENERAL_TIMEOUT			(50 * TIMEOUT_RESOLUTION)
#define RELEASE_INFO_TIMEOUT		(15 * TIMEOUT_RESOLUTION)


#define FEAT_ENABLE			1
#define FEAT_DISABLE			0

#define SYSTEM_RESET_RETRY		3

#define B2_RETRY				2
#define LOCKDOWN_CODE_RETRY     2
#define LOCKDOWN_CODE_READ_CHUNK    4


int readB2(u16 address, u8 *outBuf, int len);
int readB2U16(u16 address, u8 *outBuf, int byteToRead);
int releaseInformation(void);
char *printHex(char *label, u8 *buff, int count);
int pollForEvent(int *event_to_search, int event_bytes, u8 *readData, int time_to_wait);
int fts_disableInterrupt(void);
int fts_enableInterrupt(void);
int u8ToU16(u8 *src, u16 *dst);
int u8ToU16_le(u8 *src, u16 *dst);
int u8ToU16n(u8 *src, int src_length, u16 *dst);
int u16ToU8(u16 src, u8 *dst);
int u16ToU8_le(u16 src, u8 *dst);
int u16ToU8_be(u16 src, u8 *dst);
int u16ToU8n(u16 *src, int src_length, u8 *dst);
int u8ToU32(u8 *src, u32 *dst);
int u32ToU8(u32 src, u8 *dst);
int attempt_function(int(*code)(void), unsigned long wait_before_retry, int retry_count);
void setResetGpio(int gpio);
int fts_system_reset(void);
int isSystemResettedUp(void);
int isSystemResettedDown(void);
void setSystemResettedUp(int val);
void setSystemResettedDown(int val);
int senseOn(void);
int senseOff(void);
int keyOn(void);
int keyOff(void);
int featureEnableDisable(int on_off, u8 feature);
int checkEcho(u8 *cmd, int size);
void print_frame_short(char *label, short **matrix, int row, int column);
short **array1dTo2d_short(short *data, int size, int columns);
u8 **array1dTo2d_u8(u8 *data, int size, int columns);
void print_frame_u8(char *label, u8 **matrix, int row, int column);
void print_frame_u32(char *label, u32 **matrix, int row, int column);
void print_frame_int(char *label, int **matrix, int row, int column);
int cleanUp(int enableTouch);
int flushFIFO(void);
int fts_get_lockdown_info(u8 *data);
int writeNoiseParameters(u8 *noise);
int readNoiseParameters(u8 *noise);
