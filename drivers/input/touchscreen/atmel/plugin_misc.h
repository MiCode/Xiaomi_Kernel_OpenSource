
#ifndef __LINUX_ATMEL_MXT_PLUG_MISC
#define __LINUX_ATMEL_MXT_PLUG_MISC

#include <linux/types.h>

extern struct plugin_misc mxt_plugin_msc;

ssize_t plugin_misc_show(struct plugin_misc *p, char *buf, size_t count);
int plugin_misc_store(struct plugin_misc *p, const char *buf, size_t count);

#define PID_MAGIC_WORD0 64
#define PID_MAGIC_WORD1 0

#define MISC_PDS_PID_LEN 8
#define MISC_PDS_PTC_LEN (PTC_KEY_GROUPS * sizeof(s16)) /*8*/
#define MISC_PDS_HEAD_LEN 2
#define MISC_PDS_DATA_LEN (MISC_PDS_PID_LEN + MISC_PDS_PTC_LEN)

#define PTC_TUNING_STEP_VAL 100
#define PTC_TUNING_MAX_COUNT 50
#define PTC_TARGE_REF_VAL 512

#endif


