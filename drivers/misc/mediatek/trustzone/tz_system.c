
#include <linux/module.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include "trustzone/tz_cross/trustzone.h"
#include "trustzone/tz_cross/ta_system.h"
#include "trustzone/kree/system.h"
#include "kree_int.h"
#include "sys_ipc.h"


static TZ_RESULT KREE_ServPuts(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);
static TZ_RESULT KREE_ServUSleep(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);
static TZ_RESULT tz_ree_service(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE]);
static TZ_RESULT KREE_ServThread_Create (u32 op, u8 uparam[REE_SERVICE_BUFFER_SIZE]);

static const KREE_REE_Service_Func ree_service_funcs[] = 
{
    0,
    KREE_ServPuts,
    KREE_ServUSleep,
    KREE_ServMutexCreate,
    KREE_ServMutexDestroy,
    KREE_ServMutexLock,
    KREE_ServMutexUnlock,
    KREE_ServMutexTrylock,
    KREE_ServMutexIslock,
    KREE_ServSemaphoreCreate,
    KREE_ServSemaphoreDestroy,
    KREE_ServSemaphoreDown,
    KREE_ServSemaphoreDownTimeout,
    KREE_ServSemaphoreDowntrylock,
    KREE_ServSemaphoreUp,
#if 0    
    KREE_ServWaitqCreate,
    KREE_ServWaitqDestroy,
    KREE_ServWaitqWaitevent,
    KREE_ServWaitqWaiteventTimeout,
    KREE_ServWaitqWakeup, 
#endif    
    KREE_ServRequestIrq,
    KREE_ServEnableIrq,
    KREE_ServEnableClock,
    KREE_ServDisableClock,
    KREE_ServThread_Create,
};
#define ree_service_funcs_num (sizeof(ree_service_funcs)/sizeof(ree_service_funcs[0]))

static u32 tz_service_call(u32 handle, u32 op, u32 arg1, u32 arg2)
{
    /* Reserve buffer for REE service call parameters */
    u32 param[REE_SERVICE_BUFFER_SIZE/sizeof(u32)];
    register u32 r0 asm("r0") = handle;
    register u32 r1 asm("r1") = op;
    register u32 r2 asm("r2") = arg1;
    register u32 r3 asm("r3") = arg2;
    register u32 r4 asm("r4") = (u32)param;

    asm volatile(
        ".arch_extension sec\n"
        __asmeq("%0", "r0")
        __asmeq("%1", "r1")
        __asmeq("%2", "r0")
        __asmeq("%3", "r1")
        __asmeq("%4", "r2")
        __asmeq("%5", "r3")
        __asmeq("%6", "r4")
        "smc    #0\n"
        : "=r" (r0), "=r" (r1)
        : "r" (r0), "r" (r1), "r" (r2), "r" (r3), "r" (r4)
        : "memory");

    while (r1 != 0)
    {
        /* Need REE service */
        /* r0 is the command, paramter in param buffer */
        r1 = tz_ree_service(r0, (u8*)param);

        /* Work complete. Going Back to TZ again */
        r0 = 0xffffffff;
        asm volatile(
            ".arch_extension sec\n"
            __asmeq("%0", "r0")
            __asmeq("%1", "r1")
            __asmeq("%2", "r0")
            __asmeq("%3", "r1")
            __asmeq("%4", "r4")
            "smc    #0\n"
            : "=r" (r0), "=r" (r1)
            : "r" (r0), "r" (r1), "r" (r4)
            : "memory");
    }

    return r0;
}

TZ_RESULT KREE_TeeServiceCallNoCheck(KREE_SESSION_HANDLE handle, 
                                     uint32_t command,
                                     uint32_t paramTypes, 
                                     MTEEC_PARAM param[4])
{
    return (TZ_RESULT)tz_service_call((u32)handle, command,
                                      paramTypes, (u32)param);
}

TZ_RESULT KREE_TeeServiceCall(KREE_SESSION_HANDLE handle, uint32_t command,
                              uint32_t paramTypes, MTEEC_PARAM oparam[4])
{
    int i, do_copy = 0;
    TZ_RESULT ret;
    uint32_t tmpTypes;
    MTEEC_PARAM param[4];

    // Parameter processing.
    memset(param, 0, sizeof(param));
    tmpTypes = paramTypes;
    for (i=0; tmpTypes; i++)
    {
        TZ_PARAM_TYPES type = tmpTypes & 0xff;
        tmpTypes >>= 8;
        switch (type)
        {
            case TZPT_VALUE_INPUT:
            case TZPT_VALUE_INOUT:
            case TZPT_VALUE_OUTPUT:
                param[i] = oparam[i];
                break;

            case TZPT_MEM_INPUT:
            case TZPT_MEM_OUTPUT:
            case TZPT_MEM_INOUT:
                // Check if point to kernel low memory
                param[i] = oparam[i];
                if ((unsigned int)param[i].mem.buffer < PAGE_OFFSET ||
                    (unsigned int)param[i].mem.buffer >= (unsigned int)high_memory)
                {
                    // No, we need to copy....
                    if (param[i].mem.size > TEE_PARAM_MEM_LIMIT)
                    {
                        param[i].mem.buffer = 0;
                        ret = TZ_RESULT_ERROR_OUT_OF_MEMORY;
                        goto error;
                    }

                    param[i].mem.buffer = kmalloc(param[i].mem.size, GFP_KERNEL);
                    if (!param[i].mem.buffer)
                    {
                        ret = TZ_RESULT_ERROR_OUT_OF_MEMORY;
                        goto error;
                    }

                    memcpy(param[i].mem.buffer, oparam[i].mem.buffer,
                           param[i].mem.size);
                }
                break;

            case TZPT_MEMREF_INPUT:
            case TZPT_MEMREF_OUTPUT:
            case TZPT_MEMREF_INOUT:
                // Check if share memory is valid.
                // Not done yet.
                param[i] = oparam[i];
                break;

            default:
                // Bad format, return.
                return TZ_RESULT_ERROR_BAD_FORMAT;
        }
    }

    // Real call.
    do_copy = 1;
    ret = KREE_TeeServiceCallNoCheck(handle, command, paramTypes, param);

error:
    tmpTypes = paramTypes;
    for (i=0; tmpTypes; i++)
    {
        TZ_PARAM_TYPES type = tmpTypes & 0xff;
        tmpTypes >>= 8;
        switch (type)
        {
            case TZPT_VALUE_INOUT:
            case TZPT_VALUE_OUTPUT:
                oparam[i] = param[i];
                break;

            case TZPT_MEM_INPUT:
            case TZPT_MEM_OUTPUT:
            case TZPT_MEM_INOUT:
                // Check if point to kernel memory
                if (param[i].mem.buffer && 
                    (param[i].mem.buffer != oparam[i].mem.buffer))
                {
                    if (type != TZPT_MEM_INPUT && do_copy)
                        memcpy(oparam[i].mem.buffer, param[i].mem.buffer,
                               param[i].mem.size);
                    kfree(param[i].mem.buffer);
                }
                break;

            case TZPT_MEMREF_INPUT:
            case TZPT_MEMREF_OUTPUT:
            case TZPT_MEMREF_INOUT:
            case TZPT_VALUE_INPUT:
            default:
                // Nothing to do
                break;
        }
    }

    return ret;
}
EXPORT_SYMBOL(KREE_TeeServiceCall);


static TZ_RESULT KREE_ServPuts(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE])
{
    param[REE_SERVICE_BUFFER_SIZE-1]=0;
    printk(param);
    return TZ_RESULT_SUCCESS;
}

static TZ_RESULT KREE_ServUSleep(u32 op, u8 uparam[REE_SERVICE_BUFFER_SIZE])
{
    struct ree_service_usleep *param = (struct ree_service_usleep*)uparam;
    usleep_range(param->ustime, param->ustime);
    return TZ_RESULT_SUCCESS;
}

/* TEE Kthread create by REE service, TEE service call body
*/
static int kree_thread_function (void *arg)
{
    MTEEC_PARAM param[4];
    uint32_t paramTypes;
    int ret;
    struct REE_THREAD_INFO *info = (struct REE_THREAD_INFO *) arg;
    
    paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT);
    param[0].value.a = (uint32_t) info->handle;

    // free parameter resource
    kfree (info);

    // create TEE kthread
    ret = KREE_TeeServiceCall((KREE_SESSION_HANDLE)MTEE_SESSION_HANDLE_SYSTEM,
                              TZCMD_SYS_THREAD_CREATE,
                              paramTypes, param);

    return ret;
}

/* TEE Kthread create by REE service
*/
static TZ_RESULT KREE_ServThread_Create (u32 op, u8 uparam[REE_SERVICE_BUFFER_SIZE])
{
    struct REE_THREAD_INFO *info;

    // get parameters
    // the resource will be freed after the thread stops
    info = (struct REE_THREAD_INFO *) kmalloc (sizeof (struct REE_THREAD_INFO), GFP_KERNEL);
    if (info == NULL)
    {
        return TZ_RESULT_ERROR_OUT_OF_MEMORY;
    }
    memcpy (info, uparam, sizeof (struct REE_THREAD_INFO));
    
    // create thread and run ...
    if (!kthread_run (kree_thread_function, info, info->name))
    {
        return TZ_RESULT_ERROR_GENERIC;
    }
    
    return TZ_RESULT_SUCCESS;
}


static TZ_RESULT tz_ree_service(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE])
{
    KREE_REE_Service_Func func;

    if (op >= ree_service_funcs_num)
        return TZ_RESULT_ERROR_BAD_PARAMETERS;
    
    func = ree_service_funcs[op];
    if (!func)
        return TZ_RESULT_ERROR_BAD_PARAMETERS;

    return (func)(op, param);
}

TZ_RESULT KREE_InitTZ(void)
{
    uint32_t paramTypes;
    MTEEC_PARAM param[4];
    TZ_RESULT ret;

    paramTypes = TZPT_NONE;
    ret = KREE_TeeServiceCall((KREE_SESSION_HANDLE)MTEE_SESSION_HANDLE_SYSTEM,
                              TZCMD_SYS_INIT,
                              paramTypes, param);

    return ret;
}


TZ_RESULT KREE_CreateSession(const char *ta_uuid, KREE_SESSION_HANDLE *pHandle)
{
    uint32_t paramTypes;
    MTEEC_PARAM param[4];
    TZ_RESULT ret;

    if (!ta_uuid || !pHandle)
        return TZ_RESULT_ERROR_BAD_PARAMETERS;

    param[0].mem.buffer = (char*)ta_uuid;
    param[0].mem.size = strlen(ta_uuid)+1;
    paramTypes = TZ_ParamTypes2(TZPT_MEM_INPUT, TZPT_VALUE_OUTPUT);

    ret = KREE_TeeServiceCall((KREE_SESSION_HANDLE)MTEE_SESSION_HANDLE_SYSTEM, 
                              TZCMD_SYS_SESSION_CREATE,
                              paramTypes, param);

    if (ret == TZ_RESULT_SUCCESS)
        *pHandle = (KREE_SESSION_HANDLE)param[1].value.a;

    return ret;
}
EXPORT_SYMBOL(KREE_CreateSession);

TZ_RESULT KREE_CloseSession(KREE_SESSION_HANDLE handle)
{
    uint32_t paramTypes;
    MTEEC_PARAM param[4];
    TZ_RESULT ret;

    param[0].value.a = (uint32_t)handle;
    paramTypes = TZ_ParamTypes1(TZPT_VALUE_INPUT);

    ret = KREE_TeeServiceCall((KREE_SESSION_HANDLE)MTEE_SESSION_HANDLE_SYSTEM,
                              TZCMD_SYS_SESSION_CLOSE,
                              paramTypes, param);

    return ret;
}
EXPORT_SYMBOL(KREE_CloseSession);

#include "trustzone/tz_cross/tz_error_strings.h"

const char *TZ_GetErrorString(TZ_RESULT res)
{
    return _TZ_GetErrorString(res);
}
EXPORT_SYMBOL(TZ_GetErrorString);
