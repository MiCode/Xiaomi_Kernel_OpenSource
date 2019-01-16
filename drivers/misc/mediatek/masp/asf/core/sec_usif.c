#include "sec_osal_light.h"

#include "sec_boot_lib.h"
#include "sec_boot.h"
#include "sec_error.h"
#include "sec_typedef.h"
#include "sec_log.h"

/**************************************************************************
 *  MACRO
 **************************************************************************/
#define MOD                                "ASF.USIF"

/**************************************************************************
 *  EXTERNAL VARIABLES
 *************************************************************************/
extern SECURE_INFO                         sec_info;


/**************************************************************************
 *  FIND DEVICE PARTITION
 **************************************************************************/
int sec_usif_check(void)
{
    int ret = SEC_OK;
    ASF_FILE fd;
    const uint32 buf_len = 2048;
    char *buf = ASF_MALLOC(buf_len);
    char *pmtdbufp;

    uint32 rn = 0;    
    ssize_t pm_sz;
    int cnt;

    ASF_GET_DS   

    /* -------------------------- */
    /* open proc device           */
    /* -------------------------- */
    SMSG(TRUE,"[%s] open /proc/dumchar_info\n",MOD);
    fd = ASF_OPEN("/proc/dumchar_info");   

    if (ASF_FILE_ERROR(fd))
    {
        SMSG(TRUE,"[%s] open /proc/dumchar_info fail\n",MOD);
        goto _usif_dis;
    }
    
    buf[buf_len - 1] = '\0';
    pm_sz = ASF_READ(fd, buf, buf_len - 1);
    pmtdbufp = buf;

    /* -------------------------- */
    /* parsing proc device        */
    /* -------------------------- */
    while (pm_sz > 0) 
    {
        int m_num, m_sz, mtd_e_sz;
        char m_name[16];
        m_name[0] = '\0';
        m_num = -1;

        m_num ++;

        /* -------------------------- */
        /* parsing proc/dumchar_info  */
        /* -------------------------- */        
        cnt = sscanf(pmtdbufp, "%15s %x %x %x",m_name, &m_sz, &mtd_e_sz, &rn);

        if ((4 == cnt) && (2 == rn))
        {
            SMSG(TRUE,"[%s] RN = 2\n",MOD);        
            goto _usif_en;
        }
        else if ((4 == cnt) && (1 == rn))
        {
            SMSG(TRUE,"[%s] RN = 1\n",MOD);        
            goto _usif_dis;            
        }

        while (pm_sz > 0 && *pmtdbufp != '\n') 
        {
            pmtdbufp++;
            pm_sz--;
        }
        
        if (pm_sz > 0) 
        {
            pmtdbufp++;
            pm_sz--;
        }
    }
    
    SMSG(TRUE,"[%s] usif RN not found\n",MOD);
    ret = ERR_USIF_PROC_RN_NOT_FOUND;
    goto _exit;

_usif_en:
    SMSG(TRUE,"[%s] usif enabled\n",MOD);
    sec_info.bUsifEn = TRUE;
    goto _exit;    

_usif_dis:
    SMSG(TRUE,"[%s] usif disabled\n",MOD);
    sec_info.bUsifEn = FALSE;    

_exit:
    ASF_CLOSE(fd);
    ASF_FREE(buf);
    ASF_PUT_DS
        
    return ret;
}

