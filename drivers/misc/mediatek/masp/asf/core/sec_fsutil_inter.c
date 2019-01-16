/******************************************************************************
 *  INCLUDE LIBRARY
 ******************************************************************************/
#include "sec_boot_lib.h"

/**************************************************************************
 *  MODULE NAME
 **************************************************************************/
#define MOD                         "ASF.FS"

/**************************************************************************
 *  EXTERNAL VARIABLES
 **************************************************************************/
extern bool                         bSecroExist;
extern bool                         bSecroIntergiy;

/**************************************************************************
 *  READ SECRO
 **************************************************************************/
uint32 sec_fs_read_secroimg (char* path, char* buf)
{
    uint32 ret  = SEC_OK;    
    const uint32 size = sizeof(AND_SECROIMG_T);
    uint32 temp = 0;    
    ASF_FILE fd;

    /* ------------------------ */    
    /* check parameter          */
    /* ------------------------ */
    SMSG(TRUE,"[%s] open '%s'\n",MOD,path);
    if(0 == size)
    {
        ret = ERR_FS_SECRO_READ_SIZE_CANNOT_BE_ZERO;
        goto _end;
    }

    /* ------------------------ */    
    /* open secro               */
    /* ------------------------ */    
    fd = ASF_OPEN(path);
    
    if (ASF_IS_ERR(fd)) 
    {
        ret = ERR_FS_SECRO_OPEN_FAIL;
        goto _open_fail;
    }

    /* ------------------------ */
    /* read secro               */
    /* ------------------------ */    
    /* configure file system type */
    osal_set_kernel_fs();
    
    /* adjust read off */
    ASF_SEEK_SET(fd,0);     
    
    /* read secro content */   
    if(0 >= (temp = ASF_READ(fd,buf,size)))
    {
        ret = ERR_FS_SECRO_READ_FAIL;
        goto _end;
    }

    if(size != temp)
    {
        SMSG(TRUE,"[%s] size '0x%x', read '0x%x'\n",MOD,size,temp);
        ret = ERR_FS_SECRO_READ_WRONG_SIZE;
        goto _end;
    }

    /* ------------------------ */       
    /* check integrity          */
    /* ------------------------ */
    if(SEC_OK != (ret = sec_secro_check()))
    {        
        goto _end;
    }

    /* ------------------------ */       
    /* SECROIMG is valid        */
    /* ------------------------ */
    bSecroExist = TRUE;    

_end:    
    ASF_CLOSE(fd);
    osal_restore_fs();
    
_open_fail:
    return ret;
}


