#ifndef SEC_PAL_H
#define SEC_PAL_H

#include "sec_osal_light.h"
#include <mach/sec_osal.h>  

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

typedef unsigned int uint32;
typedef unsigned char uchar;

/******************************************************************************
 *  DEBUG
 ******************************************************************************/
/* Debug message event */
#define DBG_EVT_NONE        (0)       /* No event */
#define DBG_EVT_CMD         (1 << 0)  /* SEC CMD related event */
#define DBG_EVT_FUNC        (1 << 1)  /* SEC function event */
#define DBG_EVT_INFO        (1 << 2)  /* SEC information event */
#define DBG_EVT_WRN         (1 << 30) /* Warning event */
#define DBG_EVT_ERR         (1 << 31) /* Error event */
#define DBG_EVT_ALL         (0xffffffff)

#define DBG_EVT_MASK        (DBG_EVT_ALL)

#ifdef SEC_DEBUG
#define MSG(evt, fmt, args...) \
do {    \
    if ((DBG_EVT_##evt) & DBG_EVT_MASK) { \
        printk(fmt, ##args); \
    }   \
} while(0)
#else
#define MSG(evt, fmt, args...)  do{}while(0)
#endif

#define MSG_FUNC(mod) MSG(FUNC, "[%s] %s\n", mod, __FUNCTION__)

/******************************************************************************
 *  EXPORT FUNCTION
 ******************************************************************************/
extern void *mcpy(void *dest, const void *src, int  count);
extern int mcmp (const void *cs, const void *ct, int count);
extern void dump_buf(unsigned char* buf, unsigned int len);

#endif /* end of SEC_LIB_H */
