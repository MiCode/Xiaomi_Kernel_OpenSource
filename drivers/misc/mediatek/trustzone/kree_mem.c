
#include <trustzone/kree/mem.h>
#include <trustzone/kree/system.h>
#include <trustzone/tz_cross/ta_mem.h>
#include <linux/mm.h>


#define DBG_KREE_MEM

// notiec: handle type is the same
static inline TZ_RESULT _allocFunc (uint32_t cmd, KREE_SESSION_HANDLE session, 
    uint32_t *mem_handle, uint32_t alignment, uint32_t size, char *dbg)
{
    MTEEC_PARAM p[4];
    TZ_RESULT ret;

    if ((session == 0) || (mem_handle == NULL) || (size == 0))
    {
        return TZ_RESULT_ERROR_BAD_PARAMETERS;
    }
    
    p[0].value.a = alignment; 
    p[1].value.a = size;     
    ret = KREE_TeeServiceCall(session, cmd, 
            TZ_ParamTypes3( TZPT_VALUE_INPUT, TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT), p);
    if (ret != TZ_RESULT_SUCCESS)
    {
#ifdef DBG_KREE_MEM    
        printk("[kree] %s Error: %d\n", dbg, ret);
#endif
        return ret;
    }    
    
    *mem_handle = (KREE_SECUREMEM_HANDLE) p[2].value.a;

    return TZ_RESULT_SUCCESS;
}

static inline TZ_RESULT _handleOpFunc (uint32_t cmd, KREE_SESSION_HANDLE session, uint32_t mem_handle, char *dbg)
{
    MTEEC_PARAM p[4];
    TZ_RESULT ret;

    if ((session == 0) || (mem_handle == 0))
    {
        return TZ_RESULT_ERROR_BAD_PARAMETERS;
    }
    
    p[0].value.a = (uint32_t) mem_handle;     
    ret = KREE_TeeServiceCall(session, cmd, 
            TZ_ParamTypes1( TZPT_VALUE_INPUT), p);
    if (ret < 0)
    {
#ifdef DBG_KREE_MEM      
        printk("[kree] %s Error: %d\n", dbg, ret);
#endif
        return ret;
    }    
    
    return TZ_RESULT_SUCCESS;
}

static inline TZ_RESULT _handleOpFunc_1 (uint32_t cmd, KREE_SESSION_HANDLE session, uint32_t mem_handle, uint32_t *count, char *dbg)
{
    MTEEC_PARAM p[4];
    TZ_RESULT ret;

    if ((session == 0) || (mem_handle == 0) || (count == NULL))
    {
        return TZ_RESULT_ERROR_BAD_PARAMETERS;
    }

    p[0].value.a = (uint32_t) mem_handle;         
    ret = KREE_TeeServiceCall(session, cmd, 
            TZ_ParamTypes2( TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT), p);
    if (ret < 0)
    {
#ifdef DBG_KREE_MEM      
        printk("[kree] %s Error: %d\n", dbg, ret);
#endif
        *count = 0;
        return ret;
    }    

    *count = p[1].value.a;
    
    return TZ_RESULT_SUCCESS;
}


TZ_RESULT kree_register_sharedmem (KREE_SESSION_HANDLE session, KREE_SHAREDMEM_HANDLE *mem_handle, 
    uint32_t start, uint32_t size, uint32_t map_p)
{
    MTEEC_PARAM p[4];
    TZ_RESULT ret;

    p[0].value.a = start;
    p[1].value.a = size;
    p[2].value.a = map_p;    
    ret = KREE_TeeServiceCall(session, TZCMD_MEM_SHAREDMEM_REG, 
            TZ_ParamTypes4(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT, TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT), p);
    if (ret != TZ_RESULT_SUCCESS)
    {
        *mem_handle = 0;
        return ret;
    }

    *mem_handle = p[3].value.a; 

    return TZ_RESULT_SUCCESS;
}

TZ_RESULT kree_unregister_sharedmem (KREE_SESSION_HANDLE session, KREE_SHAREDMEM_HANDLE mem_handle)
{
    MTEEC_PARAM p[4];
    TZ_RESULT ret;

    p[0].value.a = (uint32_t) mem_handle; 
    ret = KREE_TeeServiceCall(session, TZCMD_MEM_SHAREDMEM_UNREG, 
            TZ_ParamTypes1(TZPT_VALUE_INPUT), p);
    if (ret != TZ_RESULT_SUCCESS)
    {
        return ret;
    }

    return TZ_RESULT_SUCCESS;
}

/* APIs
*/ 
TZ_RESULT KREE_RegisterSharedmem (KREE_SESSION_HANDLE session, 
    KREE_SHAREDMEM_HANDLE *shm_handle, KREE_SHAREDMEM_PARAM *param)
{
    TZ_RESULT ret;

    if ((session == 0) || (shm_handle == NULL) || (param->buffer == NULL) || (param->size == 0))
    {
        return TZ_RESULT_ERROR_BAD_PARAMETERS;
    }

    // only for kmalloc
    if (((uint32_t) param->buffer >= PAGE_OFFSET) &&
     ((uint32_t) param->buffer < (uint32_t) high_memory))
    {
        ret = kree_register_sharedmem (session, shm_handle, (uint32_t) param->buffer, (uint32_t) param->size, 0); // set 0 for no remap...
        if (ret != TZ_RESULT_SUCCESS)
        {
#ifdef DBG_KREE_MEM      
            printk("[kree] KREE_RegisterSharedmem Error: %d\n", ret);
#endif
            return ret;
        }          
    }
    else
    {
        printk("[kree] KREE_RegisterSharedmem Error: support kmalloc only!!!\n");        
        return TZ_RESULT_ERROR_NOT_IMPLEMENTED;
    }

    return TZ_RESULT_SUCCESS;
}

TZ_RESULT KREE_UnregisterSharedmem (KREE_SESSION_HANDLE session, KREE_SHAREDMEM_HANDLE shm_handle)
{
    TZ_RESULT ret;

    if ((session == 0) || (shm_handle == 0))
    {
        return TZ_RESULT_ERROR_BAD_PARAMETERS;
    }

    ret = kree_unregister_sharedmem (session, shm_handle);
    if (ret < 0)
    {
#ifdef DBG_KREE_MEM      
        printk("[kree] KREE_UnregisterSharedmem Error: %d\n", ret);
#endif
        return ret;
    }   
        
    return TZ_RESULT_SUCCESS;
}

TZ_RESULT KREE_AllocSecuremem (KREE_SESSION_HANDLE session, 
    KREE_SECUREMEM_HANDLE *mem_handle, uint32_t alignment, uint32_t size)
{
    TZ_RESULT ret;

    ret = _allocFunc (TZCMD_MEM_SECUREMEM_ALLOC, session, mem_handle, alignment, size, "KREE_AllocSecuremem");

    return ret;
}

TZ_RESULT KREE_ReferenceSecuremem (KREE_SESSION_HANDLE session, KREE_SECUREMEM_HANDLE mem_handle)
{
    TZ_RESULT ret;

    ret = _handleOpFunc (TZCMD_MEM_SECUREMEM_REF, session, mem_handle, "KREE_ReferenceSecuremem");

    return ret;
}

TZ_RESULT KREE_UnreferenceSecuremem (KREE_SESSION_HANDLE session, KREE_SECUREMEM_HANDLE mem_handle)
{
    TZ_RESULT ret;
    uint32_t count = 0;

    ret = _handleOpFunc_1 (TZCMD_MEM_SECUREMEM_UNREF, session, mem_handle, &count, "KREE_UnreferenceSecuremem");
#ifdef DBG_KREE_MEM  
    printk ("KREE_UnreferenceSecuremem: count = 0x%x\n", count);
#endif

    return ret;
}

TZ_RESULT KREE_AllocSecurechunkmem (KREE_SESSION_HANDLE session, 
    KREE_SECUREMEM_HANDLE *cm_handle, uint32_t alignment, uint32_t size)
{
    TZ_RESULT ret;

    ret = _allocFunc (TZCMD_MEM_SECURECM_ALLOC, session, cm_handle, alignment, size, "KREE_AllocSecurechunkmem");

    return ret;
}

TZ_RESULT KREE_ReferenceSecurechunkmem (KREE_SESSION_HANDLE session, KREE_SECURECM_HANDLE cm_handle)
{
    TZ_RESULT ret;

    ret = _handleOpFunc (TZCMD_MEM_SECURECM_REF, session, cm_handle, "KREE_ReferenceSecurechunkmem");

    return ret;
}

TZ_RESULT KREE_UnreferenceSecurechunkmem (KREE_SESSION_HANDLE session, KREE_SECURECM_HANDLE cm_handle)
{
    TZ_RESULT ret;
    uint32_t count = 0;

    ret = _handleOpFunc_1 (TZCMD_MEM_SECURECM_UNREF, session, cm_handle, &count, "KREE_UnreferenceSecurechunkmem");
#ifdef DBG_KREE_MEM  
        printk ("KREE_UnreferenceSecurechunkmem: count = 0x%x\n", count);
#endif

    return ret;
}

TZ_RESULT KREE_ReadSecurechunkmem (KREE_SESSION_HANDLE session, uint32_t offset, uint32_t size, void *buffer)
{
    MTEEC_PARAM p[4];
    TZ_RESULT ret;

    if ((session == 0) || (size == 0)) 
    {
        return TZ_RESULT_ERROR_BAD_PARAMETERS;
    }

    p[0].value.a = offset; 
    p[1].value.a = size; 
    p[2].mem.buffer = buffer; 
    p[2].mem.size = size; // fix me!!!!
    ret = KREE_TeeServiceCall(session, TZCMD_MEM_SECURECM_READ, 
            TZ_ParamTypes3(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT, TZPT_MEM_OUTPUT), p);
    if (ret != TZ_RESULT_SUCCESS)
    {
#ifdef DBG_KREE_MEM    
        printk("[kree] KREE_ReadSecurechunkmem Error: %d\n", ret);
#endif
        return ret;
    }    

    return TZ_RESULT_SUCCESS;
}

TZ_RESULT KREE_WriteSecurechunkmem (KREE_SESSION_HANDLE session, uint32_t offset, uint32_t size, void *buffer)
{
    MTEEC_PARAM p[4];
    TZ_RESULT ret;

    if ((session == 0) || (size == 0)) 
    {
        return TZ_RESULT_ERROR_BAD_PARAMETERS;
    }
    
    p[0].value.a = offset; 
    p[1].value.a = size; 
    p[2].mem.buffer = buffer; 
    p[2].mem.size = size; // fix me!!!!
    ret = KREE_TeeServiceCall(session, TZCMD_MEM_SECURECM_WRITE, 
            TZ_ParamTypes3(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT, TZPT_MEM_INPUT), p);
    if (ret != TZ_RESULT_SUCCESS)
    {
#ifdef DBG_KREE_MEM    
        printk("[kree] KREE_WriteSecurechunkmem Error: %d\n", ret);
#endif
        return ret;
    }    

    return TZ_RESULT_SUCCESS;
}


TZ_RESULT KREE_GetSecurechunkReleaseSize (KREE_SESSION_HANDLE session, uint32_t *size)
{
    MTEEC_PARAM p[4];
    TZ_RESULT ret;

    ret = KREE_TeeServiceCall(session, TZCMD_MEM_SECURECM_RSIZE, TZ_ParamTypes1(TZPT_VALUE_OUTPUT), p);
    if (ret != TZ_RESULT_SUCCESS)
    {
#ifdef DBG_KREE_MEM    
        printk("[kree] KREE_GetSecurechunkReleaseSize Error: %d\n", ret);
#endif
        return ret;
    }    

    *size = p[0].value.a;

    return TZ_RESULT_SUCCESS;
}

TZ_RESULT KREE_GetTEETotalSize (KREE_SESSION_HANDLE session, uint32_t *size)
{
    MTEEC_PARAM p[4];
    TZ_RESULT ret;

    ret = KREE_TeeServiceCall(session, TZCMD_MEM_TOTAL_SIZE, TZ_ParamTypes1(TZPT_VALUE_OUTPUT), p);
    if (ret != TZ_RESULT_SUCCESS)
    {
#ifdef DBG_KREE_MEM    
        printk("[kree] KREE_GetTEETotalSize Error: %d\n", ret);
#endif
        return ret;
    }    

    *size = p[0].value.a;

    return TZ_RESULT_SUCCESS;
}

