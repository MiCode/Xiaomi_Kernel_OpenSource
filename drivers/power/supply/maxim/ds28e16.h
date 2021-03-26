// DS28E16.h - Include file for DS28E16/DS28E50?
//
#ifndef __DS28E16_H__
#define __DS28E16_H__

#define ERROR_NO_DEVICE					-1
#define ERROR_R_STATUS					-2
#define ERROR_R_ROMID					-3
#define ERROR_R_PAGEDATA				-4
#define ERROR_COMPUTE_MAC				-5
#define ERROR_S_SECRET					-6
#define ERROR_UNMATCH_MAC				-7
#define DS_TRUE							1
#define DS_FALSE						0

#define CMD_RELEASE_BYTE				0xAA
// 1-wire ROM commands
#define CMD_SEARCH_ROM					0xF0
#define CMD_READ_ROM					0x33
#define CMD_MATCH_ROM					0x55
#define CMD_SKIP_ROM					0xCC
#define CMD_RESUME_ROM					0xA5

// DS28E16 commands
#define CMD_START						0x66
#define CMD_WRITE_MEM					0x96
#define CMD_READ_MEM					0x44
#define CMD_READ_STATUS					0xAA
#define CMD_SET_PAGE_PROT				0xC3
#define CMD_COMP_READ_AUTH				0xA5
#define CMD_COMP_S_SECRET				0x3C
#define CMD_DECREMENT_CNT				0xC9
#define CMD_DISABLE_DEVICE				0x33

#define CMD_READ_RNG					0xD2

// Result bytes
#define RESULT_SUCCESS					0xAA
#define RESULT_FAIL_PROTECTION			0x55
#define RESULT_FAIL_PARAMETETER			0x77
#define RESULT_FAIL_DEVICEDISABLED		0x88
#define RESULT_FAIL_COUNTERINVALID		0x22
#define RESULT_FAIL_COUNTERNOSET		0x33
#define RESULT_FAIL_SEQUENCEWRONG		0x00
#define RESULT_FAIL_NONE				0xFF

// Delays
#define DELAY_DS28E16_EE_WRITE			100
#define DELAY_DS28E16_EE_READ			50
#define DELAY_DS28E16_RNG_READ			40
#define DELAY_DS28E16_ENC_READ			40

// DS28E16
#define PROT_RP							0x01
#define PROT_WP							0x02
#define PROT_WP_RP						0x03
#define PROT_DC							0x08

// page number
#define PAGE0							0x00
#define PAGE1							0x01
#define DC_PAGE							0x02
#define SECRET_PAGE						0x03
#define MAX_PAGENUM						0x04

#define ANONYMOUS						1

//retry times config
#define SET_SRAM_RETRY					4
#define GET_USER_MEMORY_RETRY			8
#define GET_PERSONALITY_RETRY			8
#define GET_ROM_ID_RETRY				8
#define GET_BLOCK_STATUS_RETRY			8
#define SET_BLOCK_STATUS_RETRY			8
#define GET_MAC_RETRY					8
#define GET_VERIFY_RETRY				8
#define GET_S_SECRET_RETRY				4

// xiaomi's battery identity
#define FAMILY_CODE						0x9f
#define CUSTOM_ID_MSB					0x04
#define CUSTOM_ID_LSB					0xF0

extern unsigned char last_result_byte;

// Command functions
// 1-Wire ROMID commands
unsigned char crc_low_first(unsigned char *ptr, unsigned char len);
short Read_RomID(unsigned char *RomID);

// Common Functions
unsigned short docrc16(unsigned short data);
int DS28E16_standard_cmd_flow(unsigned char *write_buf, int delay_ms, unsigned char *read_buf, int *read_len, int write_len);

// Command Functions (no high level verification)
int DS28E16_cmd_readStatus(unsigned char *data);				// 读入芯片的页面保护属性、一个MAN_ID字节和一个芯片版本字节，合计6个字节
int DS28E16_cmd_readMemory(int pg, unsigned char *data);		// pg=0,1,2页面，一次读入32个字节，前16个字节为有效数据，后续16个字节固定为0x00或0xFF
int DS28E16_cmd_writeMemory(int pg, unsigned char *data);		// pg=0,1,2页面，一次必须完整写入16个字节
int DS28E16_cmd_decrementCounter(void);							// 执行此函数，完成17位计数器减1操作
int DS28E16_cmd_setPageProtection(int pg, unsigned char prot);	// 设置芯片的页面属性
int DS28E16_cmd_device_disable(int op, unsigned char *password);// 芯片自毁，不能恢复
int DS28E16_cmd_computeReadPageAuthentication(int anon, int pg, unsigned char *challenge, unsigned char *hmac); // 器件基于会话密码、随机数、页面数据和ROMID，返回器件的算法结果
int DS28E16_cmd_computeS_Secret(int anon, int bdconst, int pg, unsigned char *partial);   						// 基于主机发送的密钥种子，器件在内部计算出会话密码

// host functions
int AuthenticateDS28E16(int anon, int bdconst, int S_Secret_PageNum, int PageNum, unsigned char *Challenge, unsigned char *Secret_Seeds, unsigned char *S_Secret);

// retry interface
static int ds28el16_Read_RomID_retry(unsigned char *RomID);
static int ds28el16_get_page_status_retry(unsigned char *data);
static int DS28E16_cmd_computeS_Secret_retry(int anon, int bdconst,
					int pg, unsigned char *session_seed);
static int DS28E16_cmd_computeReadPageAuthentication_retry(int anon, int pg,
					unsigned char *challenge, unsigned char *hmac);
static int ds28el16_get_page_data_retry(int page, unsigned char *data);
#endif
