#include "sec_error.h"
#include "sec_boot.h"
#include "sec_mtd.h"
#include "sec_typedef.h"
#include "sec_osal_light.h"

/**************************************************************************
 *  MODULE NAME
 **************************************************************************/
#define MOD                         "MTD_UTIL"

/**************************************************************************
 *  PART NAME QUERY
 **************************************************************************/
char* mtd2pl (char* part_name)
{
    /* sync mtd partition name with PL's and DA's */
    /* ----------------- */
    /* seccfg            */
    /* ----------------- */    
    if(0 == mcmp(part_name,MTD_SECCFG,strlen(MTD_SECCFG)))
    {   
        return (char*) PL_SECCFG;
    }
    /* ----------------- */
    /* uboot             */
    /* ----------------- */    
    else if(0 == mcmp(part_name,MTD_UBOOT,strlen(MTD_UBOOT)))
    {   
        return (char*) PL_UBOOT;
    }
    /* ----------------- */    
    /* logo              */
    /* ----------------- */    
    else if(0 == mcmp(part_name,MTD_LOGO,strlen(MTD_LOGO)))
    {
        return (char*) PL_LOGO;
    }
    /* ----------------- */
    /* boot image        */
    /* ----------------- */    
    else if(0 == mcmp(part_name,MTD_BOOTIMG,strlen(MTD_BOOTIMG)))
    {
        return (char*) PL_BOOTIMG;
    }
    /* ----------------- */    
    /* user data         */
    /* ----------------- */    
    else if(0 == mcmp(part_name,MTD_USER,strlen(MTD_USER)))
    {
        return (char*) PL_USER;               
    }   
    /* ----------------- */    
    /* system image      */
    /* ----------------- */    
    else if(0 == mcmp(part_name,MTD_ANDSYSIMG,strlen(MTD_ANDSYSIMG)))
    {
        return (char*) PL_ANDSYSIMG;
    }   
    /* ----------------- */    
    /* recovery          */
    /* ----------------- */    
    else if(0 == mcmp(part_name,MTD_RECOVERY,strlen(MTD_RECOVERY)))
    {
        return (char*) PL_RECOVERY;
    }       
    /* ----------------- */    
    /* sec ro            */
    /* ----------------- */    
    else if(0 == mcmp(part_name,MTD_SECRO,strlen(MTD_SECRO)))
    {
        return (char*) PL_SECRO;
    }
    /* ----------------- */    
    /* not found         */
    /* ----------------- */    
    else
    {
        return part_name;
    }    
}

char* pl2mtd (char* part_name)
{
    /* sync mtd partition name with PL's and DA's */
    /* ----------------- */
    /* seccfg            */
    /* ----------------- */    
    if(0 == mcmp(part_name,PL_SECCFG,strlen(PL_SECCFG)))
    {   
        return (char*) MTD_SECCFG;
    }
    /* ----------------- */    
    /* uboot             */
    /* ----------------- */    
    else if(0 == mcmp(part_name,PL_UBOOT,strlen(PL_UBOOT)))
    {   
        return (char*) MTD_UBOOT;
    }
    /* ----------------- */    
    /* logo              */
    /* ----------------- */    
    else if(0 == mcmp(part_name,PL_LOGO,strlen(PL_LOGO)))
    {
        return (char*) MTD_LOGO;
    }
    /* ----------------- */    
    /* boot image        */
    /* ----------------- */    
    else if(0 == mcmp(part_name,PL_BOOTIMG,strlen(PL_BOOTIMG)))
    {
        return (char*) MTD_BOOTIMG;
    }
    /* ----------------- */    
    /* user data         */
    /* ----------------- */    
    else if(0 == mcmp(part_name,PL_USER,strlen(PL_USER)))
    {
        return (char*) MTD_USER;               
    }   
    /* ----------------- */    
    /* system image      */
    /* ----------------- */    
    else if(0 == mcmp(part_name,PL_ANDSYSIMG,strlen(PL_ANDSYSIMG)))
    {
        return (char*) MTD_ANDSYSIMG;
    }   
    /* ----------------- */    
    /* recovery          */
    /* ----------------- */    
    else if(0 == mcmp(part_name,PL_RECOVERY,strlen(PL_RECOVERY)))
    {
        return (char*) MTD_RECOVERY;
    }       
    /* ----------------- */    
    /* sec ro            */
    /* ----------------- */    
    else if(0 == mcmp(part_name,PL_SECRO,strlen(PL_SECRO)))
    {
        return (char*) MTD_SECRO;
    }
    /* ----------------- */    
    /* not found         */
    /* ----------------- */    
    else
    {
        return part_name;
    }    
}


