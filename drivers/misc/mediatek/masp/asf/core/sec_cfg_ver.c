/******************************************************************************
 *  INCLUDE LIBRARY
 ******************************************************************************/
#include "sec_boot_lib.h"
#include "sec_cfg_ver.h"

/******************************************************************************
 * CONSTANT DEFINITIONS                                                       
 ******************************************************************************/
#define MOD                         "SECCFG_VER"

/******************************************************************************
 *  LOCAL VARIABLE
 ******************************************************************************/
static SECCFG_VER seccfg_ver       = SECCFG_V1;

/******************************************************************************
 *  SECCFG VERSION
 ******************************************************************************/
SECCFG_VER get_seccfg_ver (void)
{
    switch(seccfg_ver)
    {
        case SECCFG_V1:
        case SECCFG_V1_2:
            return seccfg_ver;
        case SECCFG_V3:
            return seccfg_ver;
        default:
            SEC_ASSERT(0);
    }
    
    return 0;
}

void set_seccfg_ver (SECCFG_VER val)
{
    switch(val)
    {
        case SECCFG_V1: 
        case SECCFG_V1_2:
            seccfg_ver = val; 
            break;            
        case SECCFG_V3:
        case SECCFG_UNSET:
            seccfg_ver = val; 
            break;            
        default:
            SEC_ASSERT(0);           
    }
}

/******************************************************************************
 *  SECCFG STATUS
 ******************************************************************************/
SECCFG_STATUS get_seccfg_status (SECCFG_U *p_seccfg)
{
    switch(get_seccfg_ver())
    {
        case SECCFG_V1: 
        case SECCFG_V1_2:
            return p_seccfg->v1.status;        
        case SECCFG_V3:
            return p_seccfg->v3.seccfg_status;
        case SECCFG_UNSET:
        default:        
            memset(p_seccfg,0,sizeof(SECCFG_U));        
            return 0;
    }    
}

void set_seccfg_status (SECCFG_U *p_seccfg, SECCFG_STATUS val)
{
    switch(get_seccfg_ver())
    {
        case SECCFG_V1: 
        case SECCFG_V1_2:
            p_seccfg->v1.status = val;   
            break;
        case SECCFG_V3:
            p_seccfg->v3.seccfg_status = val;
            break;                        
        case SECCFG_UNSET:
        default:        
            memset(p_seccfg,0,sizeof(SECCFG_U));               
            break;            
    }
}

/******************************************************************************
 *  SECCFG SIU
 ******************************************************************************/
uint32 get_seccfg_siu (SECCFG_U *p_seccfg)
{
    switch(get_seccfg_ver())
    {
        case SECCFG_V1: 
        case SECCFG_V1_2:
            return p_seccfg->v1.siu_status;        
        case SECCFG_V3:
            return p_seccfg->v3.siu_status;        
        case SECCFG_UNSET:
        default:        
            memset(p_seccfg,0,sizeof(SECCFG_U));
            return 0;
    }    
}

void set_seccfg_siu (SECCFG_U *p_seccfg, uint32 val)
{
    switch(get_seccfg_ver())
    {
        case SECCFG_V1: 
        case SECCFG_V1_2:
            p_seccfg->v1.siu_status = val;                         
            break;
        case SECCFG_V3:
            p_seccfg->v3.siu_status = val;      
            break;                        
        case SECCFG_UNSET:
        default:        
            memset(p_seccfg,0,sizeof(SECCFG_U));                   
            break;            
    }
}

/******************************************************************************
 *  SECCFG IMAGE COUNT
 ******************************************************************************/
uint32 get_seccfg_img_cnt (void)
{
    switch(get_seccfg_ver())
    {
        case SECCFG_V1: 
        case SECCFG_V1_2:
            return SECURE_IMAGE_COUNT;
        case SECCFG_V3:
            return SECURE_IMAGE_COUNT_V3;
        case SECCFG_UNSET:
        default:        
            return 0;
    }    
}

/******************************************************************************
 *  SECCFG VERSION DETECT
 ******************************************************************************/
int seccfg_ver_detect (void)
{
    int ret = SEC_OK;   

    /* ----------------------------------- */
    /* check seccfg magic                  */
    /* ----------------------------------- */    
    if(SEC_CFG_MAGIC_NUM != seccfg.v1.magic_number)
    {                
        ret = ERR_SEC_CFG_MAGIC_INVALID;
        goto _end;        
    }

    /* ----------------------------------- */
    /* detect seccfg version               */
    /* ----------------------------------- */    
    if(SEC_CFG_END_PATTERN == seccfg.v1.end_pattern)
    {
        SMSG(true,"[%s] seccfg version v%d detected\n",MOD,seccfg.v1.lib_ver);    
        if(SECCFG_V1 != seccfg.v1.lib_ver)
        {
            SMSG(true,"[%s] seccfg version not v1, correct it\n",MOD);
        }
        set_seccfg_ver(SECCFG_V1);
        set_shdr_ver(SEC_HDR_V1);        
    }
    else if(SEC_CFG_END_PATTERN == seccfg.v3.end_pattern)
    {
        SMSG(true,"[%s] seccfg version v%d detected\n",MOD,seccfg.v3.seccfg_ver);        
        if(SECCFG_V3 != seccfg.v3.seccfg_ver)
        {
            SMSG(true,"[%s] seccfg version not v3, correct it\n",MOD);
        }        
        set_seccfg_ver(SECCFG_V3);
        set_shdr_ver(SEC_HDR_V3);
    }
    else
    {
        ret = ERR_SEC_CFG_VERSION_INVALID;
        goto _end;
    }

_end:

    return ret;
}

/******************************************************************************
 *  SECCFG VERSION CORRECT
 ******************************************************************************/
int seccfg_ver_correct (void)
{
    int ret = SEC_OK;
    SEC_IMG_HEADER_U *sign_header = 0;

    /* ----------------------------------- */
    /* check seccfg magic                  */
    /* ----------------------------------- */    
    if(SEC_CFG_MAGIC_NUM != seccfg.v1.magic_number)
    {                
        ret = ERR_SEC_CFG_MAGIC_INVALID;
        goto _end;        
    }

    /* ----------------------------------- */
    /* check seccfg version                */
    /* ----------------------------------- */    
    switch(get_seccfg_ver())
    {
        case SECCFG_V1:
        case SECCFG_V1_2:

            /* correct seccfg version */
            sign_header = (SEC_IMG_HEADER_U *)&seccfg.v1.image_info[0].header;

            if(SEC_IMG_MAGIC != sign_header->v1.magic_number)
            {
                ret = ERR_SBOOT_CHECK_INVALID_IMG_MAGIC_NUM;
                goto _end;
            }
            
            if(0 == sign_header->v2.signature_length)
            {   
                set_shdr_ver(SEC_HDR_V1);
                set_seccfg_ver(SECCFG_V1);
            }
            else
            {
                set_shdr_ver(SEC_HDR_V2);
                set_seccfg_ver(SECCFG_V1_2);        
            }        
            
            break;            
            
        case SECCFG_V3:

            set_shdr_ver(SEC_HDR_V3);   
            set_seccfg_ver(SECCFG_V3);      

            break;
        default:

            ret = ERR_SEC_CFG_VERSION_INVALID;
            goto _end;           
    }    

_end:

    return ret;
}

/******************************************************************************
 *  SECCFG VERSION CHECK
 ******************************************************************************/
int seccfg_ver_verify (void)
{
    int ret = SEC_OK;

    /* ----------------------------------- */
    /* check seccfg magic                  */
    /* ----------------------------------- */    
    if(SEC_CFG_MAGIC_NUM != seccfg.v1.magic_number)
    {                
        ret = ERR_SEC_CFG_MAGIC_INVALID;
        goto _end;        
    }

    /* ----------------------------------- */
    /* check seccfg version                */
    /* ----------------------------------- */    
    switch(get_seccfg_ver())
    {
        case SECCFG_V1:
        case SECCFG_V1_2:
            SMSG(true,"[%s] seccfg id = %s\n",MOD,seccfg.v1.id);
            
            if(0 != mcmp(seccfg.v1.id,SEC_CFG_BEGIN,SEC_CFG_BEGIN_LEN))
            {
                ret = ERR_SEC_CFG_INVALID_ID;
                goto _end;
            }            

            SMSG(true,"[%s] seccfg end pattern = 0x%x\n",MOD,seccfg.v1.end_pattern);

            if(SEC_CFG_END_PATTERN != seccfg.v1.end_pattern)
            {
                ret = ERR_SEC_CFG_INVALID_END_PATTERN;
                goto _end;
            } 
            
            break;            
            
        case SECCFG_V3:
            SMSG(true,"[%s] seccfg id = %s\n",MOD,seccfg.v3.id);
            
            if(0 != mcmp(seccfg.v3.id,SEC_CFG_BEGIN,SEC_CFG_BEGIN_LEN))
            {
                ret = ERR_SEC_CFG_INVALID_ID;
                goto _end;
            }            

            SMSG(true,"[%s] seccfg end pattern = 0x%x\n",MOD,seccfg.v3.end_pattern);

            if(SEC_CFG_END_PATTERN != seccfg.v3.end_pattern)
            {
                ret = ERR_SEC_CFG_INVALID_END_PATTERN;
                goto _end;
            } 

            break;
        default:

            ret = ERR_SEC_CFG_VERSION_INVALID;
            goto _end;           
    }    

_end:

    return ret;
}

