
#include <linux/module.h>
#include <linux/types.h>

#include <mach/mt_clkmgr.h>

#include <trustzone/tz_cross/trustzone.h>
#include <trustzone/kree/system.h>
#include "kree_int.h"

TZ_RESULT KREE_ServEnableClock(u32 op, u8 uparam[REE_SERVICE_BUFFER_SIZE])
{
    struct ree_service_clock *param = (struct ree_service_clock*)uparam;
    int rret;
    TZ_RESULT ret = TZ_RESULT_SUCCESS;

    rret = enable_clock(param->clk_id, param->clk_name);
    if (rret < 0)
        ret = TZ_RESULT_ERROR_GENERIC;

    return ret;
}

TZ_RESULT KREE_ServDisableClock(u32 op, u8 uparam[REE_SERVICE_BUFFER_SIZE])
{
    struct ree_service_clock *param = (struct ree_service_clock*)uparam;
    int rret;
    TZ_RESULT ret = TZ_RESULT_SUCCESS;

    rret = disable_clock(param->clk_id, param->clk_name);
    if (rret < 0)
        ret = TZ_RESULT_ERROR_GENERIC;

    return ret;
}
