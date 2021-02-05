#ifndef _WT_BOOT_REASON_H
#define _WT_BOOT_REASON_H
#include <linux/types.h>

#define   RESET_MAGIC_PANIC          0x43494E50     /* P N I C */
#define   RESET_MAGIC_WDT_BARK       0x4B524257     /* W B R K */
#define   RESET_MAGIC_THERMAL        0x4C4D4854     /* T H M L */
#define   RESET_MAGIC_VMREBOOT       0x42524D56     /* V M R B */
#define   RESET_MAGIC_CMD_REBOOT     0x42444D43     /* C M D B */
#define   RESET_MAGIC_INIT           0x54494E49     /* I N I T */
/* SSR */
#define   RESET_MAGIC_IPA_FWS        0x46415049     /* I P A F */
#define   RESET_MAGIC_VENUS          0x534E4556     /* V E N S */
#define   RESET_MAGIC_SPSS           0x53535053     /* S P S S */
#define   RESET_MAGIC_AXXX_ZAP       0x50415A41     /* A Z A P */
#define   RESET_MAGIC_SLPI           0x49504C53     /* S L P I */
#define   RESET_MAGIC_ADSP           0x50534441     /* A D S P */
#define   RESET_MAGIC_CDSP           0x50534443     /* C D S P */
#define   RESET_MAGIC_WCNSS          0x534E4357     /* W C N S */
#define   RESET_MAGIC_MODEM          0x4D444F4D     /* M O D M */
#define   RESET_MAGIC_SUBSYSTEM      0x53425553     /* S U B S */
/* USER SPACE ERROR */
#define   RESET_MAGIC_US_PANIC       0x41505355     /* U S P A */

#define WT_MAX_SSR_REASON_LEN 256U
extern char subsys_restart_reason[];
extern int wt_panic_oops;

void set_reset_magic(uint32_t magic_number);
int save_panic_key_log(const char *fmt, ...);
void save_panic_key_log_symbol(const char *fmt, unsigned long address);
int  wt_boot_reason_init(void);
void wt_boot_reason_exit(void);

#endif

