#include <mach/sec_osal.h> 
#include <mach/mt_sec_hal.h> 
#include "sec_osal_light.h"
#include "sec_boot_lib.h"
#include "sec_boot.h"
#include "sec_error.h"
#include "sec_log.h"
#include "sec_secroimg.h"


#if defined(CONFIG_MTK_GPT_SCHEME_SUPPORT)
#include <linux/types.h>
#include <linux/genhd.h> 
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/crc32.h>
#include <asm/uaccess.h>
#include <linux/mmc/sd_misc.h>
#include <mach/partition.h>

#endif


/**************************************************************************
 *  MACRO
 **************************************************************************/
#define MOD                         "ASF.DEV"
#if defined(CONFIG_MTK_GPT_SCHEME_SUPPORT)
#define PARTINFO_TITLE                      "Name"
#define USER_REGION_PATH                    "/dev/block/mmcblk0"
#define BOOT_REGION0_PATH                   "/dev/block/mmcblk0boot0"
#define USER_REGION_PART_PATH_PREFIX        "/dev/block/platform/mtk-msdc.0/by-name/"

#endif

/**************************************************************************
 *  GLOBAL VARIABLES
 *************************************************************************/
MtdPart                             mtd_part_map[MAX_MTD_PARTITIONS]; 
uint32                              secro_img_off = 0;
uint32                              secro_img_mtd_num = 0;

/**************************************************************************
 *  LOCAL VARIABLES
 *************************************************************************/
#if !defined(CONFIG_MTK_GPT_SCHEME_SUPPORT)
static bool dump_search_info        = FALSE;
#endif
static bool dump_secro_info         = FALSE;

/**************************************************************************
 *  EXTERNAL VARIABLES
 *************************************************************************/
extern AND_ROMINFO_T                rom_info;
extern uchar                        sha1sum[];
extern AND_SECROIMG_T               secroimg;
extern AND_SECROIMG_V5a_T           secroimg_v5;
extern uint32                       secro_v3_off;
extern bool                         bSecroExist;
extern bool                         bSecroV5Exist;
extern bool                         bSecroIntergiy;
extern bool                         bSecroV5Intergiy;
extern uint32                       secro_img_off;
extern uint32                       secro_img_mtd_num;
extern SECURE_INFO                  sec_info;



/**************************************************************************
 *  READ INFO
 **************************************************************************/
int read_info (int part_index, uint32 part_off, uint32 search_region, char* info_name, uint32 info_name_len, uint32 info_sz, char* info_buf)
{
    int ret  = ERR_MTD_INFO_NOT_FOUND;    
    char part_path[100];        
    uint32 off = 0;    
    uchar *buf;
    
    MtdRCtx *ctx = (MtdRCtx*) osal_kmalloc(sizeof(MtdRCtx));  

    if (ctx == NULL) 
    {
        ret = ERR_ROM_INFO_ALLOCATE_BUF_FAIL;
        goto _end;
    }

    ctx->buf = osal_kmalloc(search_region);
    memset(ctx->buf,0,search_region);

    if (ctx->buf == NULL) 
    {
        ret = ERR_ROM_INFO_ALLOCATE_BUF_FAIL;
        goto _end;
    }


    /* ------------------------ */    
    /* open file                */
    /* ------------------------ */    
    /* in order to keep key finding process securely,
       open file in kernel module */    
#if defined(CONFIG_MTK_GPT_SCHEME_SUPPORT)

    if ((MAX_MTD_PARTITIONS + 1) == part_index)
    {
        // if  part_index == MAX_MTD_PARTITIONS + 1 means read preloader
        mcpy(part_path,BOOT_REGION0_PATH,strlen(BOOT_REGION0_PATH));
        part_path[strlen(BOOT_REGION0_PATH)] = '\0';
    }
    else if (part_index < MAX_MTD_PARTITIONS)
    {
        mcpy(part_path,USER_REGION_PART_PATH_PREFIX,strlen(USER_REGION_PART_PATH_PREFIX));
        mcpy(part_path + strlen(USER_REGION_PART_PATH_PREFIX),mtd_part_map[part_index].name, strlen(mtd_part_map[part_index].name ));  
        part_path[strlen(USER_REGION_PART_PATH_PREFIX) + strlen(mtd_part_map[part_index].name)] = '\0';
    }
    else
    {   
        ret = ERR_INFO_OVER_MAX_PART_COUNT;
        goto _end;
    }
    
#else   
    if(TRUE == sec_usif_enabled())
    {
        sec_usif_part_path(part_index,part_path,sizeof(part_path));
        if(FALSE == dump_search_info)
        {
            SMSG(TRUE,"[%s] open '%s'\n",MOD,part_path);                 
            dump_search_info = TRUE;
        }
    }    
    else
    {
        sprintf(part_path, "/dev/mtd/mtd%d", part_index);
        if(FALSE == dump_search_info)
        {
            SMSG(TRUE,"[%s] open '%s'\n",MOD,part_path);                     
            dump_search_info = TRUE;
        }
    }

#endif

    //SMSG(TRUE,"[%s] open '%s'\n",MOD,part_path);

    ctx->fd = ASF_OPEN(part_path);
            
    if (ASF_IS_ERR(ctx->fd)) 
    {
        SMSG(true,"[%s] open fail\n",MOD);     
        ret = ERR_INFO_PART_NOT_FOUND;
        goto _open_fail;
    }
    
    /* ------------------------ */    
    /* read partition           */
    /* ------------------------ */    
    /* configure file system type */
    osal_set_kernel_fs();

    /* adjust read off */
    ASF_SEEK_SET(ctx->fd,part_off);     
    
    /* read partition */
    if(0 >= (ret = ASF_READ(ctx->fd,ctx->buf,search_region)))
    {
        SMSG(TRUE,"[%s] read fail (%d)\n",MOD,ret);
        ret = ERR_ROM_INFO_MTD_READ_FAIL;
        goto _end;
    }
    else
    {

        /* ------------------------ */    
        /* search info              */
        /* ------------------------ */    
        for(off = 0; off<(search_region-info_sz); off++)
        {
            buf = ctx->buf + off;

            if(0 == strncmp(buf,info_name,info_name_len))
            {                    
                osal_mtd_lock();
                    
                /* ------------------------ */    
                /* fill info                */
                /* ------------------------ */    
                mcpy(info_buf, buf, info_sz);                 

                /* --------------------------------------------- */    
                /* keep secro v3 offset, for v5 search usage.    */
                /* --------------------------------------------- */    

                if (info_buf == (char*)&secroimg)
                {
                    secro_v3_off = off;
                    SMSG(TRUE,"[%s] keep sro v3 off: 0x%x\n",MOD,off);
                }

                ret = SEC_OK;
                osal_mtd_unlock();                    
                break;
                }
        }
    }
    
_end:
    ASF_CLOSE(ctx->fd);
    osal_restore_fs();
    
_open_fail :    
    osal_kfree(ctx->buf);
    osal_kfree(ctx);            
    return ret;
}

/**************************************************************************
 *  DUMP PARTITION
 **************************************************************************/
void sec_dev_dump_part(void)
{
    uint32 i = 0;
    
    for(i = 0; i < MAX_MTD_PARTITIONS; i++)
    {
#if defined(CONFIG_MTK_GPT_SCHEME_SUPPORT)    
        SMSG(TRUE,"[%s] #%2d, part %15s, offset 0x%llx, sz 0x%llx\n",MOD,i,
                                                mtd_part_map[i].name,
                                                mtd_part_map[i].off,
                                                mtd_part_map[i].sz);

#else
        SMSG(TRUE,"[%s] #%2d, part %10s, offset 0x%8x, sz 0x%8x\n",MOD,i,
                                                        mtd_part_map[i].name,
                                                        mtd_part_map[i].off,
                                                        mtd_part_map[i].sz);
#endif                                                        
    }
}

/**************************************************************************
 *  FIND DEVICE PARTITION
 **************************************************************************/
void sec_dev_find_parts(void)
{
    ASF_FILE fd;
    const uint32 buf_len = 2048;
    char *buf = osal_kmalloc(buf_len);
    char *pmtdbufp;

    uint32 mtd_part_cnt = 0;
    ssize_t pm_sz;
    int cnt;
#if !defined(CONFIG_MTK_GPT_SCHEME_SUPPORT)     
    uint32 off = 0;
    uint32 rn = 0; 
#endif    

    osal_set_kernel_fs();

   /* -------------------------- */
    /* open proc device           */
    /* -------------------------- */
#if defined(CONFIG_MTK_GPT_SCHEME_SUPPORT) 

    /* -------------------------- */
    /* open proc/partinfo         */
    /* -------------------------- */
    SMSG(TRUE,"[%s] open /proc/partinfo\n",MOD);
    fd = ASF_OPEN("/proc/partinfo");


#else

    if(TRUE == sec_usif_enabled())
    {
        /* -------------------------- */
        /* open proc/dumchar_info     */
        /* -------------------------- */
        SMSG(TRUE,"[%s] open /proc/dumchar_info\n",MOD);
        fd = ASF_OPEN("/proc/dumchar_info");
    }
    else
    {
        /* -------------------------- */
        /* open proc/mtd              */
        /* -------------------------- */
        SMSG(TRUE,"[%s] open /proc/mtd\n",MOD);
        fd = ASF_OPEN("/proc/mtd");   
    }
    
#endif 


    if (ASF_IS_ERR(fd))
    {
        SMSG(TRUE,"[%s] open /proc/* failed\n",MOD);    
        goto _end;
    }

    
    buf[buf_len - 1] = '\0';
    pm_sz = ASF_READ(fd, buf, buf_len - 1);
    pmtdbufp = buf;

    /* -------------------------- */
    /* parsing proc device        */
    /* -------------------------- */
    while (pm_sz > 0) 
    {
        
#if defined(CONFIG_MTK_GPT_SCHEME_SUPPORT)    
        int m_num;
        unsigned long long m_sz, m_off;
        char m_name[16];
        m_name[0] = '\0';
        m_num = -1;

        m_num ++;

        /* -------------------------- */
        /* parsing proc/parinfo  */
        /* -------------------------- */        
        cnt = sscanf(pmtdbufp, "%15s %llx %llx",m_name, &m_off, &m_sz);
        //SMSG(TRUE,"[%s] find parts %s, off 0x%llx, size 0x%llx\n",MOD,m_name,m_off,m_sz);
        

        if (mtd_part_cnt < MAX_MTD_PARTITIONS) 
        {   
            if (0 != mcmp(m_name,PARTINFO_TITLE,strlen(PARTINFO_TITLE)))
            {
                mcpy(mtd_part_map[mtd_part_cnt].name, m_name, strlen(m_name));
                /* fill partition size */
                mtd_part_map[mtd_part_cnt].sz = m_sz;
                
                /* calculate partition off */
                mtd_part_map[mtd_part_cnt].off = m_off;
                /* part count */
              
                if(0 == mcmp(m_name,GPT_SECRO,strlen(GPT_SECRO)))
                {
                    secro_img_mtd_num = mtd_part_cnt;
                }

                mtd_part_cnt++;
            }
        }
        else 
        {
            SMSG(TRUE,"too many mtd partitions\n");
        }

#else
        int m_num, m_sz, mtd_e_sz;
        char m_name[16];
        m_name[0] = '\0';
        m_num = -1;

        m_num ++;

        if(TRUE == sec_usif_enabled())
        {
            /* -------------------------- */
            /* parsing proc/dumchar_info  */
            /* -------------------------- */        
            cnt = sscanf(pmtdbufp, "%15s %x %x %x",m_name, &m_sz, &mtd_e_sz, &rn);
            //SMSG(TRUE,"[%s] find parts %s, size 0x%x, cnt 0x%x, rn 0x%x\n",MOD,m_name,m_sz,cnt,rn);

            if ((cnt == 4) && (rn == 2))
            {
                
                if (mtd_part_cnt < MAX_MTD_PARTITIONS) 
                {   
                   
                    /* ===================== */
                    /* uboot                 */
                    /* ===================== */                
                    if(0 == mcmp(m_name,USIF_UBOOT,strlen(USIF_UBOOT)))
                    {   
                        mcpy(mtd_part_map[mtd_part_cnt].name, PL_UBOOT, strlen(PL_UBOOT));
                    }
                    /* ===================== */                
                    /* logo                  */
                    /* ===================== */                
                    else if(0 == mcmp(m_name,USIF_LOGO,strlen(USIF_LOGO)))
                    {
                        mcpy(mtd_part_map[mtd_part_cnt].name, PL_LOGO, strlen(PL_LOGO));
                    }
                    /* ===================== */                
                    /* boot image            */
                    /* ===================== */                
                    else if(0 == mcmp(m_name,USIF_BOOTIMG,strlen(USIF_BOOTIMG)))
                    {
                        mcpy(mtd_part_map[mtd_part_cnt].name, PL_BOOTIMG, strlen(PL_BOOTIMG));
                    }
                    /* ===================== */                
                    /* user data             */
                    /* ===================== */                
                    else if(0 == mcmp(m_name,USIF_USER,strlen(USIF_USER)))
                    {
                        mcpy(mtd_part_map[mtd_part_cnt].name, PL_USER, strlen(PL_USER));
                    }   
                    /* ===================== */                
                    /* android system image  */
                    /* ===================== */                
                    else if(0 == mcmp(m_name,USIF_ANDSYSIMG,strlen(USIF_ANDSYSIMG)))
                    {
                        mcpy(mtd_part_map[mtd_part_cnt].name, PL_ANDSYSIMG, strlen(PL_ANDSYSIMG));
                    }   
                    /* ===================== */                
                    /* recovery              */
                    /* ===================== */                
                    else if(0 == mcmp(m_name,USIF_RECOVERY,strlen(USIF_RECOVERY)))
                    {
                        mcpy(mtd_part_map[mtd_part_cnt].name, PL_RECOVERY, strlen(PL_RECOVERY));
                    }
                    /* ===================== */                
                    /* secroimg              */
                    /* ===================== */                
                    else if(0 == mcmp(m_name,USIF_SECRO,strlen(USIF_SECRO)))
                    {
                        mcpy(mtd_part_map[mtd_part_cnt].name, PL_SECRO, strlen(PL_SECRO));
                        secro_img_mtd_num = mtd_part_cnt;
                    }
                    /* ===================== */                
                    /* other                 */
                    /* ===================== */                
                    else
                    {
                        mcpy(mtd_part_map[mtd_part_cnt].name, m_name, sizeof(m_name)-1);
                    }

                    /* fill partition size */
                    mtd_part_map[mtd_part_cnt].sz = m_sz;

                    /* calculate partition off */
                    mtd_part_map[mtd_part_cnt].off = off;

                    /* update off and part count */
                    off += m_sz;                
                    mtd_part_cnt++;
                } 
                else 
                {
                    SMSG(TRUE,"too many mtd partitions\n");
                }
            }            

        }
        else
        {
            /* -------------------------- */
            /* parsing proc/mtd           */
            /* -------------------------- */                
            cnt = sscanf(pmtdbufp, "mtd%d: %x %x %15s",&m_num, &m_sz, &mtd_e_sz, m_name);        

            if ((cnt == 4) && (m_name[0] == '"')) 
            {
                char *x = strchr(m_name + 1, '"');

                if (x) 
                {
                    *x = 0;
                }

                if (mtd_part_cnt < MAX_MTD_PARTITIONS) 
                {                
                    /* ===================== */
                    /* uboot                 */
                    /* ===================== */                
                    if(0 == mcmp(m_name+1,MTD_UBOOT,strlen(MTD_UBOOT)))
                    {   
                        mcpy(mtd_part_map[mtd_part_cnt].name, PL_UBOOT, strlen(PL_UBOOT));
                    }
                    /* ===================== */                
                    /* logo                  */
                    /* ===================== */                
                    else if(0 == mcmp(m_name+1,MTD_LOGO,strlen(MTD_LOGO)))
                    {
                        mcpy(mtd_part_map[mtd_part_cnt].name, PL_LOGO, strlen(PL_LOGO));
                    }
                    /* ===================== */                
                    /* boot image            */
                    /* ===================== */                
                    else if(0 == mcmp(m_name+1,MTD_BOOTIMG,strlen(MTD_BOOTIMG)))
                    {
                        mcpy(mtd_part_map[mtd_part_cnt].name, PL_BOOTIMG, strlen(PL_BOOTIMG));
                    }
                    /* ===================== */                
                    /* user data             */
                    /* ===================== */                
                    else if(0 == mcmp(m_name+1,MTD_USER,strlen(MTD_USER)))
                    {
                        mcpy(mtd_part_map[mtd_part_cnt].name, PL_USER, strlen(PL_USER));
                    }   
                    /* ===================== */                
                    /* android system image  */
                    /* ===================== */                
                    else if(0 == mcmp(m_name+1,MTD_ANDSYSIMG,strlen(MTD_ANDSYSIMG)))
                    {
                        mcpy(mtd_part_map[mtd_part_cnt].name, PL_ANDSYSIMG, strlen(PL_ANDSYSIMG));
                    }   
                    /* ===================== */                
                    /* recovery              */
                    /* ===================== */                
                    else if(0 == mcmp(m_name+1,MTD_RECOVERY,strlen(MTD_RECOVERY)))
                    {
                        mcpy(mtd_part_map[mtd_part_cnt].name, PL_RECOVERY, strlen(PL_RECOVERY));
                    }
                    /* ===================== */                
                    /* secroimg              */
                    /* ===================== */                
                    else if(0 == mcmp(m_name+1,MTD_SECRO,strlen(MTD_SECRO)))
                    {
                        mcpy(mtd_part_map[mtd_part_cnt].name, PL_SECRO, strlen(PL_SECRO));
                        secro_img_mtd_num = mtd_part_cnt;
                        
                    }
                    /* ===================== */                
                    /* other                 */
                    /* ===================== */                
                    else
                    {
                        mcpy(mtd_part_map[mtd_part_cnt].name, m_name+1, sizeof(m_name)-1);                
                    }

                    /* fill partition size */
                    mtd_part_map[mtd_part_cnt].sz = m_sz;

                    /* calculate partition off */
                    mtd_part_map[mtd_part_cnt].off = off;

                    /* calculate partition erase size */
                    mtd_part_map[mtd_part_cnt].e_size = mtd_e_sz;

                    /* update off and part count */
                    off += m_sz;                
                    mtd_part_cnt++;
                } 
                else 
                {
                    SMSG(TRUE,"too many mtd partitions\n");
                }
            }            
        }

#endif
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

    ASF_CLOSE(fd);
    

_end:

    osal_kfree(buf);
    osal_restore_fs();

    /* ------------------------ */    
    /* dump partition           */
    /* ------------------------ */
    //sec_dev_dump_part();
}

/**************************************************************************
 *  READ ROM INFO
 **************************************************************************/
int sec_dev_read_rom_info(void) 
{    
    int ret  = SEC_OK;  
    uint32 search_offset = ROM_INFO_SEARCH_START;
    uint32 search_len = ROM_INFO_SEARCH_LEN;

#if !defined(CONFIG_MTK_GPT_SCHEME_SUPPORT)
    uint32 mtd_num = MTD_PL_NUM;
    uint32 mtd_off = ROM_INFO_SEARCH_START;  
#endif

    SMSG(TRUE,"search starts from '0x%x'\n",ROM_INFO_SEARCH_START);

    /* ------------------------ */    
    /* check size               */
    /* ------------------------ */    
    COMPILE_ASSERT(AND_ROM_INFO_SIZE == sizeof(AND_ROMINFO_T));   


    /* ------------------------ */    
    /* read rom info            */
    /* ------------------------ */    
#if defined(CONFIG_MTK_GPT_SCHEME_SUPPORT)

    // in GPT case, preloader is not in /proc/partinfo
    for(search_offset=ROM_INFO_SEARCH_START; search_offset<(search_len+ROM_INFO_SEARCH_START); search_offset+=ROM_INFO_SEARCH_REGION)
    {

        ret = read_info ( (MAX_MTD_PARTITIONS + 1), search_offset, ROM_INFO_SEARCH_REGION, RI_NAME, RI_NAME_LEN, sizeof(AND_ROMINFO_T), (uchar*)&rom_info) ;
        
        if(SEC_OK == ret)
        {
            /* ------------------------ */    
            /* double check rom info    */
            /* ------------------------ */    
            if(0 == mcmp(rom_info.m_id,RI_NAME,RI_NAME_LEN))
            {
                SMSG(TRUE,"[%s] ROM INFO ver '0x%x', ID '%s', 0x%x, 0x%x\n",MOD,rom_info.m_rom_info_ver,rom_info.m_id,
                    rom_info.m_SEC_CTRL.m_sec_usb_dl, rom_info.m_SEC_CTRL.m_sec_boot);                      
                goto _end;                        
            }
        }   
    }

#else
    if(FALSE == sec_usif_enabled())
    {
    	/* Search from Block 1 and len is limited in preloader*/    	
    	mtd_off = mtd_part_map[mtd_num].e_size;      
    	search_len = (mtd_part_map[mtd_num].sz-mtd_part_map[mtd_num].e_size);
    }
    /* read 1MB data to search rom info */
    for(search_offset=ROM_INFO_SEARCH_START; search_offset<(search_len+ROM_INFO_SEARCH_START); search_offset+=ROM_INFO_SEARCH_REGION)
    {
        /* search partition */
        if(mtd_off < mtd_part_map[mtd_num].sz)
        {
            if(FALSE == dump_search_info)
            {
                SMSG(TRUE,"dev %2d, 0x%08x, 0x%08x\n", mtd_num, mtd_off, search_offset);     
            }

            ret = read_info ( mtd_num, mtd_off, ROM_INFO_SEARCH_REGION, RI_NAME, RI_NAME_LEN, sizeof(AND_ROMINFO_T), (uchar*)&rom_info) ;

            if(SEC_OK == ret)
            {
                /* ------------------------ */    
                /* double check rom info    */
                /* ------------------------ */    
                if(0 == mcmp(rom_info.m_id,RI_NAME,RI_NAME_LEN))
                {
                    SMSG(TRUE,"[%s] ROM INFO ver '0x%x', ID '%s', 0x%x, 0x%x\n",MOD,rom_info.m_rom_info_ver,rom_info.m_id,
                        rom_info.m_SEC_CTRL.m_sec_usb_dl, rom_info.m_SEC_CTRL.m_sec_boot);                      
                    goto _end;                        
                }
            }   
        }
    
        /* next should move to next partition ? */
        if(search_offset >= mtd_part_map[mtd_num+1].off)
        {
            mtd_num ++;
            mtd_off = 0;
            search_offset -= ROM_INFO_SEARCH_REGION;
        }
        else
        {
            mtd_off += ROM_INFO_SEARCH_REGION;            
        } 
    }

#endif        

    SMSG(TRUE,"[%s] ROM INFO not found\n",MOD); 
    
    ret = ERR_ROM_INFO_MTD_NOT_FOUND;

_end:

    return ret;
}

/**************************************************************************
 *  READ ANDROID ANTI-CLONE REGION
 **************************************************************************/
int sec_dev_read_secroimg(void) 
{    
    int ret  = SEC_OK;  
    
    uint32 search_offset = SECRO_SEARCH_START;
    uint32 search_len = SECRO_SEARCH_LEN;

    uint32 mtd_num;
    uint32 mtd_off = SECRO_SEARCH_START;
    
    const uint32 img_len = rom_info.m_sec_ro_length;
    const uint32 cipher_len = sizeof(AND_AC_ANDRO_T) + sizeof(AND_AC_MD_T)+ sizeof(AND_AC_MD2_T);

    /* ------------------------ */    
    /* check status             */    
    /* ------------------------ */    
    if(0 == secro_img_mtd_num)
    {
        ret = ERR_SECROIMG_PART_NOT_FOUND;
        goto _end;
    }

    mtd_num = secro_img_mtd_num;

    /* ------------------------ */    
    /* check parameter          */    
    /* ------------------------ */    
    if(0 == img_len)
    {
        ret = ERR_SECROIMG_INVALID_IMG_LEN;
        goto _end;
    }    

    /* ------------------------------------------------------- */    
    /* size should minus padding part, to fit secro v5 design  */
    /* ------------------------------------------------------- */    
    if(img_len != sizeof(secroimg) - sizeof(AND_SECROIMG_PADDING_T))
    {
        ret = ERR_SECROIMG_LEN_INCONSISTENT_WITH_PL;
        goto _end;
    }


    #ifdef CONFIG_ARM64
    SMSG(TRUE,"[%s] SECRO v3 image len '0x%lx'\n",MOD, sizeof(secroimg));
    #else
    SMSG(TRUE,"[%s] SECRO v3 image len '0x%x'\n",MOD, sizeof(secroimg));
    #endif

    /* ------------------------ */    
    /* find ac region           */
    /* ------------------------ */    

    /* read 1MB nand flash data to search rom info */
    for(search_offset = SECRO_SEARCH_START; search_offset < (search_len + SECRO_SEARCH_START); search_offset += SECRO_SEARCH_REGION)
    {
        /* search partition */
        if(mtd_off < mtd_part_map[mtd_num].sz)
        {
            if(FALSE == dump_secro_info)
            {
                SMSG(TRUE,"dev %2d, 0x%08x, 0x%08x\n", mtd_num, mtd_off, search_offset);     
                dump_secro_info = TRUE;
            }

            /* ------------------------ */    
            /* search secro image       */
            /* ------------------------ */              
            ret = read_info (mtd_num, mtd_off, SECRO_SEARCH_REGION, ROM_SEC_AC_REGION_ID, ROM_SEC_AC_REGION_ID_LEN, 
                             sizeof(secroimg), (uchar*)&secroimg);
            
            if(SEC_OK == ret)
            {
                SMSG(TRUE,"[%s] SECRO v3 img is found (lock)\n",MOD);
                
                /* ------------------------ */       
                /* decrypt secro image      */
                /* ------------------------ */           
                osal_secro_lock();
                
                //dump_buf((uchar*)&secroimg.m_andro,0x4);
                if(TRUE == sec_secro_ac())
                {
                    masp_hal_sp_hacc_dec((uchar*)&secroimg.m_andro, cipher_len, TRUE,HACC_USER1,TRUE);
                }
                //dump_buf((uchar*)&secroimg.m_andro,0x4);    

                /* ------------------------ */       
                /* check integrity          */
                /* ------------------------ */
                if(SEC_OK != (ret = sec_secro_check()))
                {
                    osal_secro_unlock();
                    goto _end;
                }

                /* ------------------------ */       
                /* encrypt secro image      */
                /* ------------------------ */
                if(TRUE == sec_secro_ac())
                {                        
                    masp_hal_sp_hacc_enc((uchar*)&secroimg.m_andro, cipher_len, TRUE,FALSE,TRUE);
                }
                //dump_buf((uchar*)&secroimg.m_andro,0x4);   

                /* ------------------------ */       
                /* SECROIMG is valid        */
                /* ------------------------ */                
                bSecroExist = TRUE;
                        
                osal_secro_unlock();
                
                goto _end;

            }
            
        }
    
        /* next should move to next partition ? */
        if(search_offset >= mtd_part_map[mtd_num+1].off)
        {
            mtd_num ++;
            mtd_off = 0;
            search_offset -= SECRO_SEARCH_REGION;
        }
        else
        {
            mtd_off += SECRO_SEARCH_REGION;            
        } 
    }

    ret = ERR_SECROIMG_MTD_NOT_FOUND;

_end:

    return ret;
}


/**************************************************************************
 *  READ SECRO V5 format, md_index starting at 0, up to 9.
 **************************************************************************/
int sec_dev_read_secroimg_v5(uint32 md_index) 
{    
    int ret  = SEC_OK;  

    //uint32 search_start = SECRO_SEARCH_START;
    uint32 search_offset = 0;
    //uint32 search_len = SECRO_SEARCH_LEN;

    uint32 mtd_num;
    uint32 mtd_off = SECRO_SEARCH_START;
    
    const uint32 img_len = AND_SECROIMG_V5a_SIZE;
    const uint32 cipher_len = sizeof(AND_AC_MD_INFO_V5a_T) + sizeof(AND_AC_MD_V5a_T);

    /* ------------------------ */    
    /* check status             */    
    /* ------------------------ */    
    if(0 == secro_img_mtd_num)
    {
        ret = ERR_SECROIMG_PART_NOT_FOUND;
        goto _end;
    }

    mtd_num = secro_img_mtd_num;

    /* ------------------------ */    
    /* check parameter          */    
    /* ------------------------ */    
    if(0 == img_len)
    {
        ret = ERR_SECROIMG_INVALID_IMG_LEN;
        goto _end;
    }    

    if(img_len != sizeof(secroimg_v5))
    {
        ret = ERR_SECROIMG_LEN_INCONSISTENT_WITH_PL;
        goto _end;
    }

    #ifdef CONFIG_ARM64
    SMSG(TRUE,"[%s] SECRO v5 image len '0x%lx'\n",MOD, sizeof(secroimg_v5));
    #else
    SMSG(TRUE,"[%s] SECRO v5 image len '0x%x'\n",MOD, sizeof(secroimg_v5));
    #endif
    SMSG(TRUE,"[%s] md index '0x%x'\n",MOD, md_index);

    /* ------------------------ */    
    /* find ac region           */
    /* ------------------------ */    

    /* ------------------------------------------------------- */    
    /* secro v3 offset should be known during sec_boot_init    */
    /* ------------------------------------------------------- */    
    mtd_off = secro_v3_off;

    if(FALSE == dump_secro_info)
    {
        SMSG(TRUE,"dev %2d, 0x%08x, 0x%08x\n", mtd_num, mtd_off, search_offset);     
        dump_secro_info = TRUE;
    }


    /* ------------------------------------------------------- */    
    /* if secro v3 offset is not updated during sec_boot_init  */
    /* ------------------------------------------------------- */   
    if (MAX_SECRO_V3_OFFSET == mtd_off)
    {
        ret = ERR_SECROIMG_V3_OFFSET_NOT_INIT;
        goto _end;
    }

    mtd_off += (md_index + 1)*AND_SECROIMG_V5a_SIZE;

    /* ------------------------ */    
    /* search secro image       */
    /* ------------------------ */              
    ret = read_info (mtd_num, mtd_off, SECRO_SEARCH_REGION, ROM_SEC_AC_REGION_ID, ROM_SEC_AC_REGION_ID_LEN, img_len, (uchar*)&secroimg_v5);
    
    if(SEC_OK == ret)
    {
        SMSG(TRUE,"[%s] SECRO v5 img is found (lock)\n",MOD);
        
        /* ------------------------ */       
        /* decrypt secro image      */
        /* ------------------------ */           

        osal_secro_v5_lock();


        dump_buf((uchar*)&secroimg_v5.m_md_info_v5a,0x4);
        if(TRUE == sec_secro_ac())
        {
            masp_hal_sp_hacc_dec((uchar*)&secroimg_v5.m_md_info_v5a, cipher_len, TRUE,HACC_USER1,TRUE);
        }
        dump_buf((uchar*)&secroimg_v5.m_md_info_v5a,0x4);    


        /* ------------------------ */       
        /* check integrity          */
        /* ------------------------ */
        if(SEC_OK != (ret = sec_secro_v5_check()))
        {
            osal_secro_v5_unlock();
            goto _end;
        }

        /* ------------------------ */       
        /* encrypt secro image      */
        /* ------------------------ */
        if(TRUE == sec_secro_ac())
        {                        
            masp_hal_sp_hacc_enc((uchar*)&secroimg_v5.m_md_info_v5a, cipher_len, TRUE,FALSE,TRUE);
        }
        dump_buf((uchar*)&secroimg_v5.m_md_info_v5a,0x4);   

        /* ------------------------ */       
        /* SECROIMG is valid        */
        /* ------------------------ */                
        bSecroV5Exist = TRUE;
                
        osal_secro_v5_unlock();
        
        goto _end;

    }
    


    ret = ERR_SECROIMG_MTD_NOT_FOUND;

_end:

    return ret;
}


unsigned int sec_dev_read_image(char* part_name, char* buf, u64 off, unsigned int sz, unsigned int image_type)
{
    // not implemented yet.
    return SEC_OK;    
}


