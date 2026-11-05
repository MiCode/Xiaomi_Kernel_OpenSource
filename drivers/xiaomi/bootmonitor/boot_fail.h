/*

   blackbox--bootfail partition

|-----------------------|   ----------------------------------->offset:154 M from blackbox
|   error_type_header   |   ---- header size : 4K
|-----------------------|
|      error_header     |   ---- header size : 4K
|          .            |
|          .            |   ---- MAX boot error type : 99
|          .            |
|-----------------------|   ----------------------------------->offset:1 M
|                       |
|    uefi_log_header    |   ---- header size : 4K
|                       |
|-----------------------|   ---->offset:1k  1M = 1025k
|      uefi_header      |
|          .            |   ---- header size : 4K
|          .            |   ---- log size : 64k
|          .            |   ---- MAX counts : 15
|          .            |
|-----------------------|   ------------------------------------>offset:2M
|                       |
|  boot_monitor_header  |
|          +            |   ---- 1M
|         kmsg          |
|                       |
|-----------------------|   ------------------------------------>offset:3M
|                       |
|         pmsg          |   ---- 2M
|                       |
|-----------------------|   ------------------------------------>offset:5M
*/
#ifndef __BOOT_FAIL_H__
#define __BOOT_FAIL_H__

#if defined __GNUC__
#define PACK(x)                        __attribute__((packed)) x
#elif defined __GNUG__
#define PACK(x)                        __attribute__((packed)) x
#elif defined __arm
#define PACK(x)                        __attribute__((packed)) x
#elif defined _WIN32
#define PACK(x)                        x
#else
#define PACK(x)                        __attribute__((packed)) x
#endif

typedef unsigned int                   uint32;
typedef unsigned char                  uint8;
typedef unsigned short                 uint16;
typedef unsigned long                  uint64;

#define BLACKBOX_VERSION               1001
#define BOOTFAIL_VERSION               0x1000
#define ERRORTYPE_VERSION              0x1000
#define UEFILOG_VERSION                0x1000
#define UEFI_VERSION                   0x1000

#define LOG_TABLE_HEADER_MAGIC         0x56775AF41BCDE0F0
#define LOG_ERROR_TYPE_HEADER_MAGIC    0x627759541BCDE0F0
#define LOG_ERROR_HEADER_MAGIC         0xE99BB480DD25E1F0

#define HEADER_SIZE                    0x400
#define HEADER_SIZE_4K                 0x1000
#define BLACK_BOX_OFFSET               0x0
#define LOG_CONTENT_OFFSET             0x400         // from 1k, size is 152M
#define MINIDUMP_OFFSET                0x3000000     // 48M
#define CONTROL_CONTENT_OFFSET         0x9800000     // from 152M, size is 16M
#define BOOT_CONTENT_OFFSET            0x9A00000     // from 154M, size is 10M
#define BOOT_FAIL_SIZE                 0xA00000      // for bootfail partition
#define UEFI_LOG_OFFSET                0x100000      // for bootfail partition
#define UEFI_LOG_SIZE                  0x11000       // 68k
#define BOOT_FAIL_TYPE                 0x100000      //1 M
#define ERASE_BUFF_SIZE_TWO            2*1024*1024   //2M

#define ERROR_RECORD_LEN               128
#define ERROR_STAGE                    16
#define BOITFAIL_MAX                   30
#define INIT_INDEX                     60
#define CRASH_MAX                      90

#define CURRENT_BOOT_STAGE_INIT        "init "
#define CURRENT_BOOT_INIT              "boot error for software"

/***************** header *******************/
struct PACK(log_table_header) {
	/* LOG_TABLE_MAGIC */
	uint64 magic;
	uint32 version;
	uint32 header_size;
	uint32 boot_index;
	uint32 log_content_offset;
	uint32 minidump_offset;
	uint32 minidump_is_valid;
	uint32 control_content_offset;
	uint32 boot_content_offset;
};

struct PACK(log_error_type_header) {
	/* LOG_TABLE_MAGIC */
	uint64 magic;
	uint32 version;
	uint32 header_size;
	uint32 boot_index;
	uint64 uefi_log_offset;
	uint32 error_num;
	uint32 error_header_size;
	uint64 current_error;
	uint8 error_record[ERROR_RECORD_LEN];
	uint8 stage[ERROR_STAGE];
	bool status;
};

struct PACK(log_error_header) {
	/* LOG_TABLE_MAGIC */
	uint64 magic;
	uint32 version;
	uint32 header_size;
	uint32 boot_index;
	uint64 error_type;
	uint32 current_error;
	bool error_current;
	uint8 error_record[ERROR_RECORD_LEN];
};

static uint32 error_init_type_table[CRASH_MAX]=
{
	0,			/* CRASH_NONE */
	901005302,	/* LASTFS */
	901005303,	/* ZYGOTE */
	901005304,	/* ZYGOTE_PRELOADER */
	901005305,	/* START_ANDROID */
	901005306,	/* PMS_START */
	901005307,	/* PMS_READY */
	901005308,	/* AMS_READY */
	901005309,	/* BOOT_COMPLETED */
};

struct PACK (log_boot_monitor_header) {
	/* LOG_TABLE_MAGIC */
	uint64 magic;
	uint32 version;
	uint32 boot_index;
	uint32 version_change;
	uint8 os_version[128];
	uint8 event[128];
	bool status;
};

#endif