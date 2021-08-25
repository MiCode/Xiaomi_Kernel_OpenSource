// DS28E16.h - Include file for DS28E16/DS28E50?
//
#ifndef __DS28E16_COMMON_H__
#define __DS28E16_COMMON_H__

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

#endif
