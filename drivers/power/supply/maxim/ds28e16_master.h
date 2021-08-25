// DS28E16.h - Include file for DS28E16/DS28E50?
//
#ifndef __DS28E16_MASTER_H__
#define __DS28E16_MASTER_H__

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


static int ds28el16_Read_RomID_retry(unsigned char *RomID);
static int ds28el16_get_page_status_retry(unsigned char *data);
static int DS28E16_cmd_computeS_Secret_retry(int anon, int bdconst,
					int pg, unsigned char *session_seed);
static int DS28E16_cmd_computeReadPageAuthentication_retry(int anon, int pg,
					unsigned char *challenge, unsigned char *hmac);
static int ds28el16_get_page_data_retry(int page, unsigned char *data);
#endif
