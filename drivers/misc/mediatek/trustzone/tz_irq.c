
#include <linux/module.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/slab.h>

#include "trustzone/tz_cross/trustzone.h"
#include "trustzone/tz_cross/ta_system.h"
#include "trustzone/tz_cross/ta_irq.h"
#include "trustzone/kree/system.h"
#include "kree_int.h"

/*************************************************************
 *           REE Service
 *************************************************************/
static irqreturn_t KREE_IrqHandler(int irq, void *dev)
{
    uint32_t paramTypes;
    MTEEC_PARAM param[4];
    TZ_RESULT ret;

    param[0].value.a = (uint32_t)irq;
    paramTypes = TZ_ParamTypes1(TZPT_VALUE_INPUT);
    ret = KREE_TeeServiceCall((KREE_SESSION_HANDLE)MTEE_SESSION_HANDLE_SYSTEM,
                              TZCMD_SYS_IRQ,
                              paramTypes, param);
    return (ret == TZ_RESULT_SUCCESS) ? IRQ_HANDLED : IRQ_NONE;
}

TZ_RESULT KREE_ServRequestIrq(u32 op, u8 uparam[REE_SERVICE_BUFFER_SIZE])
{
    struct ree_service_irq *param = (struct ree_service_irq*)uparam;
    int rret;
    unsigned long flags;
    TZ_RESULT ret = TZ_RESULT_SUCCESS;
    unsigned int *token;

    if (param->enable)
    {
        flags = 0;
        if (param->flags & MTIRQF_SHARED)
            flags |= IRQF_SHARED;
        if (param->flags & MTIRQF_TRIGGER_LOW)
            flags |= IRQF_TRIGGER_LOW;
        else if (param->flags & MTIRQF_TRIGGER_HIGH)
            flags |= IRQF_TRIGGER_HIGH;
        else if (param->flags & MTIRQF_TRIGGER_RISING)
            flags |= IRQF_TRIGGER_RISING;
        else if (param->flags & MTIRQF_TRIGGER_FALLING)
            flags |= IRQF_TRIGGER_FALLING;

        // Generate a token if not already exists. Token is used as device 
        // for Share IRQ
        token = 0;
        if (!param->token)
        {
            token = (unsigned int*)kmalloc(sizeof(unsigned int), GFP_KERNEL);
            if (!token)
                return TZ_RESULT_ERROR_OUT_OF_MEMORY;
            *token = param->irq;
            param->token = (unsigned int)token;
        }
        rret = request_irq(param->irq, KREE_IrqHandler, flags, "TEE IRQ", (void*)param->token);
        if (rret)
        {
            if (token)
                kfree(token);
            if (rret == -ENOMEM)
                ret = TZ_RESULT_ERROR_OUT_OF_MEMORY;
            else
                ret = TZ_RESULT_ERROR_BAD_PARAMETERS;
        }
    }
    else
    {
        free_irq(param->irq, (void*)param->token);
        if (param->token && *(unsigned int*)param->token == param->irq)
            kfree((void*)param->token);
    }
    return ret;
}

TZ_RESULT KREE_ServEnableIrq(u32 op, u8 uparam[REE_SERVICE_BUFFER_SIZE])
{
    struct ree_service_irq *param = (struct ree_service_irq*)uparam;
    if (param->enable)
        enable_irq(param->irq);
    else
        disable_irq(param->irq);
    return TZ_RESULT_SUCCESS;
}


/*************************************************************
 *           TEE Service
 *************************************************************/
static KREE_SESSION_HANDLE irq_session;

void kree_irq_init(void)
{
    TZ_RESULT ret;
    ret = KREE_CreateSession(TZ_TA_IRQ_UUID, &irq_session);
    if (ret != TZ_RESULT_SUCCESS)
    {
        printk("%s: CreateSession error %d\n", __FUNCTION__, ret);
        return;
    }
}

int kree_set_fiq(int irq, unsigned long irq_flags)
{
    MTEEC_PARAM param[4];
    TZ_RESULT ret;
    unsigned long tz_irq_flags = 0;
    
    if (irq_flags & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)) {
        tz_irq_flags |= TZ_IRQF_EDGE_SENSITIVE;
        tz_irq_flags |= (irq_flags & IRQF_TRIGGER_FALLING) ? TZ_IRQF_LOW : TZ_IRQF_HIGH;
    } else if (irq_flags & (IRQF_TRIGGER_HIGH | IRQF_TRIGGER_LOW)) {
        tz_irq_flags |= TZ_IRQF_LEVEL_SENSITIVE;
        tz_irq_flags |= (irq_flags & IRQF_TRIGGER_LOW) ? TZ_IRQF_LOW : TZ_IRQF_HIGH;
    }

    param[0].value.a = irq;
    param[1].value.a = tz_irq_flags;
    ret = KREE_TeeServiceCall(irq_session, TZCMD_IRQ_SET_FIQ,
                              TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT),
                              param);
    if (ret != TZ_RESULT_SUCCESS)
    {
        printk("%s error: %s\n", __FUNCTION__, TZ_GetErrorString(ret));
    }
    return ret;
}

static void __kree_enable_fiq(int irq, int enable)
{
    MTEEC_PARAM param[4];
    TZ_RESULT ret;

    param[0].value.a = irq;
    param[1].value.a = enable;
    ret = KREE_TeeServiceCall(irq_session, TZCMD_IRQ_ENABLE_FIQ,
                              TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT),
                              param);
    if (ret != TZ_RESULT_SUCCESS)
    {
        printk("%s error: %s\n", __FUNCTION__, TZ_GetErrorString(ret));
    }
}

void kree_enable_fiq(int irq)
{
    __kree_enable_fiq(irq, 1);
}

void kree_disable_fiq(int irq)
{
    __kree_enable_fiq(irq, 0);
}

void kree_query_fiq(int irq, int *enable, int *pending)
{
    MTEEC_PARAM param[4];
    TZ_RESULT ret;

    param[0].value.a = irq;
    ret = KREE_TeeServiceCall(irq_session, TZCMD_IRQ_QUERY_FIQ,
                              TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT),
                              param);
    if (ret != TZ_RESULT_SUCCESS)
    {
        printk("%s error: %s\n", __FUNCTION__, TZ_GetErrorString(ret));
        param[1].value.a = 0;
    }

    *enable = param[1].value.a & 1;
    *pending = (param[1].value.a & 2) != 0;
}

unsigned int kree_fiq_get_intack(void)
{
    MTEEC_PARAM param[4];
    TZ_RESULT ret;

    ret = KREE_TeeServiceCall(irq_session, TZCMD_IRQ_GET_INTACK,
                              TZ_ParamTypes1(TZPT_VALUE_OUTPUT), param);
    if (ret == TZ_RESULT_SUCCESS)
        return param[0].value.a;

    return 0xffff;
}

void kree_fiq_eoi(unsigned int iar)
{
    MTEEC_PARAM param[4];

    param[0].value.a = iar;
    KREE_TeeServiceCall(irq_session, TZCMD_IRQ_EOI,
                        TZ_ParamTypes1(TZPT_VALUE_INPUT), param);
}

int kree_raise_softfiq(unsigned int mask, unsigned int irq)
{
    MTEEC_PARAM param[4];
    TZ_RESULT ret;

    param[0].value.a = mask;
    param[1].value.a = irq;
    ret = KREE_TeeServiceCall(irq_session, TZCMD_IRQ_TRIGGER_SGI,
                              TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT),
                              param);
    return (ret == TZ_RESULT_SUCCESS);
}

void kree_irq_mask_all(unsigned int *pmask, unsigned int size)
{
    MTEEC_PARAM param[4];
    TZ_RESULT ret;

    param[0].mem.buffer = pmask;
    param[0].mem.size = size;
    ret = KREE_TeeServiceCall(irq_session, TZCMD_IRQ_MASK_ALL,
                              TZ_ParamTypes1(TZPT_MEM_OUTPUT),
                              param);
    if (ret != TZ_RESULT_SUCCESS)
    {
        printk("%s error: %s\n", __FUNCTION__, TZ_GetErrorString(ret));
    }
}

void kree_irq_mask_restore(unsigned int *pmask, unsigned int size)
{
    MTEEC_PARAM param[4];
    TZ_RESULT ret;

    param[0].mem.buffer = pmask;
    param[0].mem.size = size;
    ret = KREE_TeeServiceCall(irq_session, TZCMD_IRQ_MASK_RESTORE,
                              TZ_ParamTypes1(TZPT_MEM_INPUT),
                              param);
    if (ret != TZ_RESULT_SUCCESS)
    {
        printk("%s error: %s\n", __FUNCTION__, TZ_GetErrorString(ret));
    }
}
