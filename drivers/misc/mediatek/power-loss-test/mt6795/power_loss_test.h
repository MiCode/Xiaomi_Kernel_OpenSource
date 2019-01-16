#ifndef POWER_LOSS_TEST_H
#define POWER_LOSS_TEST_H

#include <asm-generic/ioctl.h>
#include <linux/rwsem.h>
#include <linux/proc_fs.h>

typedef struct
{
    char a[32];
}CAHR_32_ARRAY;

//Add for proc debug
#define WDT_REBOOT_ON 1
#define WDT_REBOOT_OFF 0

typedef struct 
{
    struct rw_semaphore rwsem;
    int wdt_reboot_support;
} wdt_reboot_info;
//end of proc debug

#define PRINT_REBOOT_TIMES            _IOW('V', 1, int)
#define PRINT_DATA_COMPARE_ERR        _IOW('V', 2, CAHR_32_ARRAY)
#define PRINT_FILE_OPERATION_ERR      _IOW('V', 3, int)
#define PRINT_GENERAL_INFO            _IOW('V', 4, int)
#define PRINT_NVRAM_ERR               _IOW('V', 5, int)
#define PRINT_FAT_ERR                 _IOW('V', 6, int)
#define PRINT_RAW_RW_INFO             _IOW('V', 7, int)

#endif /* end of POWER_LOSS_TEST_H */
