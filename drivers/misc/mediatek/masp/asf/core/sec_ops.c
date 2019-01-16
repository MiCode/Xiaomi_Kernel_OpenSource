#include <mach/sec_osal.h>
#include <mach/mt_sec_hal.h>
#include "sec_osal_light.h"
#include "sec_typedef.h"
#include "alg_sha1.h"
#include "sec_log.h"

static int sec_get_rid(uint32 *rid)
{
    uint32 obuf[5];
    uint32 ibuf[4];

    masp_hal_get_uuid(ibuf);

    sha1((uchar*)ibuf, 16, (uchar*)obuf);

    memcpy(rid, obuf, 16);

    #ifdef SEC_DEBUG
    {
        int i = 0;

        for (i = 0; i < 4; i++) 
        {
            SMSG(TRUE,"IBUF[%d] = 0x%.8x\n", i, ibuf[i]);
            SMSG(TRUE,"OBUF[%d] = 0x%.8x\n", i, obuf[i]);
        }
    }
    #endif

    return 0;
}


/**************************************************************************
 *  SEC RANDOM ID FUNCTION
 **************************************************************************/ 
int sec_get_random_id(unsigned int *rid)
{
    int ret;

    osal_rid_lock();
    ret = sec_get_rid(rid);
    osal_rid_unlock();
    return ret;
}
