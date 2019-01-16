#ifndef __EEMCS_KAL_H__
#define __EEMCS_KAL_H__

#include <asm/types.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include "eemcs_cfg.h"

/* portable character for multichar character set */
#define KAL_CHAR            s8
/* portable wide character for unicode character set */
#define KAL_WCHAR           s16
/* portable 8-bit unsigned integer */
#define KAL_UINT8           u8
/* portable 8-bit signed integer */
#define KAL_INT8            s8
/* portable 16-bit unsigned integer */
#define KAL_UINT16          u16
/* portable 16-bit signed integer */
#define KAL_INT16           s16
/* portable 32-bit unsigned integer */
#define KAL_UINT32          u32
/* portable 32-bit signed integer */
#define KAL_INT32           s32
/* portable 64-bit unsigned integer */
#define KAL_UINT64          u64
/* portable 64-bit signed integer */
#define KAL_INT64			s64

#define KAL_SUCCESS         (0)
#define KAL_FAIL            (-1)

#define kal_uint8 	KAL_UINT8
#define kal_uint16 	KAL_UINT16
#define kal_uint32	KAL_UINT32
#define kal_bool	KAL_CHAR


#define KAL_MUTEX                       struct mutex 
#define KAL_AQUIREMUTEX(_lock_p)        mutex_init(_lock_p)
#define KAL_DESTROYMUTEX(_lock_p)       mutex_destroy(_lock_p)
#define KAL_MUTEXLOCK(_lock_p)          mutex_lock(_lock_p)
#define KAL_MUTEXUNLOCK(_lock_p)        mutex_unlock(_lock_p)

#define KAL_ENQUEUE_PKT(_queue, _pkt)   skb_queue_tail(_queue, _pkt)
#define KAL_DEQUEUE_PKT(_queue)         skb_dequeue(_queue)
#define KAL_QUEUE_PURGE(_queue)         skb_queue_purge(_queue)

#define KAL_ALLOCATE_PHYSICAL_MEM(_pBuf, _Size)         \
({                                                      \
    int result;                                         \
                                                        \
    _pBuf = kmalloc(_Size, GFP_KERNEL);                  \
    result = _pBuf == NULL ? -ENOMEM : 0;               \
                                                        \
    result;                                             \
})

#define KAL_ALLOCATE_PHYSICAL_DMA_MEM(_pBuf, _Size)     \
({                                                      \
    int result;                                         \
                                                        \
    _pBuf = kmalloc(_Size, GFP_KERNEL | GFP_DMA);        \
    result = _pBuf == NULL ? -ENOMEM : 0;               \
                                                        \
    result;                                             \
})

#define KAL_ALLOCATE_PHYSICAL_DMA_MEM_NEW(_pBuf, _Size)     \
({                                                      \
    int result;                                         \
                                                        \
    _pBuf = kmalloc(_Size, GFP_ATOMIC);        \
    result = _pBuf == NULL ? -ENOMEM : 0;               \
                                                        \
    result;                                             \
})


#define KAL_FREE_PHYSICAL_MEM(_pBuf)            kfree(_pBuf)
#define KAL_COPY_MEM(_pDst, _pSrc, _Size)       memcpy((void *)(_pDst), (void *)(_pSrc), (_Size))
#define KAL_ZERO_MEM(_pBuf, _Size)              memset((void *)(_pBuf), 0, (_Size))
#define KAL_COMP_MEM(_pDst, _pSrc, _Size)       memcmp((void *)(_pDst), (void *)(_pSrc), (_Size))

#define KAL_ALIGN_TO_DWORD(_value)              (((_value) + 0x3) & ~0x3)

#define KAL_SLEEP_USEC(_Usec)                   usleep_range(_Usec, _Usec+10)
#define KAL_SLEEP_MSEC(_Msec)                   msleep(_Msec)
#define KAL_SLEEP_SEC(_sec)                     msleep(_sec * 1000)

#define KAL_FUNC_NAME	__FUNCTION__

/* Message verbosity: lower values indicate higher urgency */
#define DBG_OFF                             0
#define DBG_ERROR                           1
#define DBG_WARN                            2
#define DBG_CRIT                            3
#define DBG_TRACE                           4
#define DBG_INFO                            5
#define DBG_LOUD                            6

#define KAL_DEBUG_LEVEL  DBG_ERROR

#define KERNEL_DEBUG

#define LOG_FUNC(a...) printk(KERN_ERR a)

extern unsigned int lte_kal_debug_level ;


#ifdef KERNEL_DEBUG
#define KAL_DBGPRINT(_mod, _level, _fmt)     \
({                                              \
    if (lte_kal_debug_level >= _level){         \
        LOG_FUNC _fmt;        \
    }                                           \
})
#define KAL_RAWPRINT(_fmt)                      \
({                                              \
    LOG_FUNC _fmt;        \
})

#define KAL_ASSERT(exp)                         \
({                                              \
    if (!(exp)){                                \
        panic("[EEMCS/KAL] [ASSERT]%s : %d : %s\r\n", __FILE__, __LINE__,#exp);    \
    }                                           \
})
#define KAL_DUMP_DWARD_DATA(dword_size, addr)   \
({                                              \
    KAL_INT32 i ;                                \
    for (i=0; i<dword_size; i++){                \
        LOG_FUNC("[EEMCS/KAL] [DUMP] 0x%x\t", *((unsigned int)(addr)+i));   \
    }                                           \
})
#else
#define KAL_DBGPRINT(_mod, _level, _fmt)
#define KAL_RAWPRINT(_fmt)                      \
({                                              \
    LOG_FUNC _fmt;        \
})						
#define KAL_ASSERT(exp)                         \
({                                              \
    if (!(exp)){                                \
        LOG_FUNC("[EEMCS/KAL] [ASSERT]%s : %d : %s\r\n", __FILE__, __LINE__,#exp);   \
    }                                           \
})
#define KAL_DUMP_DWARD_DATA(size, addr)
#endif


#endif
