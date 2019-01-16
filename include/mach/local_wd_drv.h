#ifndef __L_WD_DRV_H
#define __L_WD_DRV_H
#include <mach/mt_typedefs.h>
#include "wd_api.h"


/* direct api */
int mpcore_wk_wdt_config(int reserved, int reserved2, int timeout_val);
int mpcore_wdt_restart(WD_RES_TYPE type);
void mtk_wd_resume(void);
void mtk_wd_suspend(void);
int local_wdt_enable(enum wk_wdt_en en);
/* used for extend request */
int mtk_local_wdt_misc_config(int bit, int set_value, int *reserved);
void mpcore_wk_wdt_stop(void);

#endif
