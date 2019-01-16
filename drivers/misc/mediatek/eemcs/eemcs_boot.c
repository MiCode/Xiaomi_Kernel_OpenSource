#include <linux/sched.h>
#include <linux/jiffies.h> 
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/poll.h>
#include <linux/export.h>
#include <linux/aee.h>

#include <mach/sec_osal.h>
#include <mach/mt_sec_export.h>
#include <mach/mtk_eemcs_helper.h>
#include "eemcs_kal.h"
#include "eemcs_debug.h"
#include "eemcs_boot.h"
#include "eemcs_boot_trace.h"
#include "eemcs_md.h"

#include "lte_main.h"

static KAL_UINT32 g_eemcs_file_pkt_len = BOOT_TX_MAX_PKT_LEN;
#define EEMCS_MBXRD_TO 10*HZ  //10 sec 
EEMCS_BOOT_SET eemcs_boot_inst;
eemcs_cdev_node_t *boot_node = NULL;
static KAL_INT32 boot_reset_state = 0;


#ifdef _EEMCS_BOOT_UT

#define EEMCS_BOOT_UT_BL_CNT (1)            // Choose the boot-loader count you want to simulate
#if (EEMCS_BOOT_UT_BL_CNT >= 2)
#define EEMCS_BOOT_UT_BL_LOOP_START (12)    // index of CMDID_BIN_LOAD_START for multiple boot-loader
#define EEMCS_BOOT_UT_BL_LOOP_END   (22)    // index of CMDID_BIN_LOAD_END for multiple boot-loader
#endif

// These commands are for a specific modem.img.
// If modem.img changes, these command content may be different.
// But the commands flow are almost similar.
static EEMCS_BOOT_UT_CMD ut_md_xcmd[] = {
    { BOOT_UT_MBX,          .data.mbx =          { SDIOMAIL_BOOTING_REQ, 0 } },
    { BOOT_UT_XCMD,         .data.xcmd =         { MAGIC_MD_CMD, CMDID_BIN_LOAD_START, XBOOT_STAGE_BROM, {0} } },
    { BOOT_UT_XCMD_GETBIN,  .data.xcmd_getbin =  { MAGIC_MD_CMD, CMDID_GET_BIN,        0x14,             0x4,        {0} } },
    { BOOT_UT_XCMD_GETBIN,  .data.xcmd_getbin =  { MAGIC_MD_CMD, CMDID_GET_BIN,        0xB000,           0xC,        {0} } },
    { BOOT_UT_XCMD_GETBIN,  .data.xcmd_getbin =  { MAGIC_MD_CMD, CMDID_GET_BIN,        0xB00C,           0x4,        {0} } },
    { BOOT_UT_XCMD_GETBIN,  .data.xcmd_getbin =  { MAGIC_MD_CMD, CMDID_GET_BIN,        0x10000,          0x10,       {0} } },
    { BOOT_UT_XCMD_GETBIN,  .data.xcmd_getbin =  { MAGIC_MD_CMD, CMDID_GET_BIN,        0x10010,          0xD724,     {0} } },
    { BOOT_UT_XCMD,         .data.xcmd =         { MAGIC_MD_CMD, CMDID_BIN_LOAD_END,   0,                {0} } },
    { BOOT_UT_MBX,          .data.mbx =          { SDIOMAIL_DL_REQ,      0 } },
    { BOOT_UT_MSD_OUTPUT,   .data.msd_print =    { MAGIC_MD_CMD, CMDID_MD_MSD_OUTPUT,  0x14,             "MT7208 BROM XBOOT OK" } },
    { BOOT_UT_MSD_OUTPUT,   .data.msd_print =    { MAGIC_MD_CMD, CMDID_MD_MSD_OUTPUT,  0x1,              "" } },
    { BOOT_UT_MSD_FLUSH,    .data.msd_print =    { MAGIC_MD_CMD, CMDID_MSG_FLUSH,      0x0,              "" } },
#if (EEMCS_BOOT_UT_BL_CNT >= 2)
    { BOOT_UT_MBX,          .data.mbx =          { SDIOMAIL_BOOTING_REQ, 0 } },
    { BOOT_UT_XCMD,         .data.xcmd =         { MAGIC_MD_CMD, CMDID_BIN_LOAD_START, XBOOT_STAGE_BROM, {0} } },
    { BOOT_UT_XCMD_GETBIN,  .data.xcmd_getbin =  { MAGIC_MD_CMD, CMDID_GET_BIN,        0x14,             0x4,        {0} } },
    { BOOT_UT_XCMD_GETBIN,  .data.xcmd_getbin =  { MAGIC_MD_CMD, CMDID_GET_BIN,        0xB000,           0xC,        {0} } },
    { BOOT_UT_XCMD_GETBIN,  .data.xcmd_getbin =  { MAGIC_MD_CMD, CMDID_GET_BIN,        0xB00C,           0x4,        {0} } },
    { BOOT_UT_XCMD_GETBIN,  .data.xcmd_getbin =  { MAGIC_MD_CMD, CMDID_GET_BIN,        0x10000,          0x10,       {0} } },
    { BOOT_UT_XCMD_GETBIN,  .data.xcmd_getbin =  { MAGIC_MD_CMD, CMDID_GET_BIN,        0x10010,          0xD724,     {0} } },
    { BOOT_UT_MSD_OUTPUT,   .data.msd_print =    { MAGIC_MD_CMD, CMDID_MD_MSD_OUTPUT,  0x12,             "MT7208 BL OK" } },
    { BOOT_UT_MSD_OUTPUT,   .data.msd_print =    { MAGIC_MD_CMD, CMDID_MD_MSD_OUTPUT,  0x1,              "" } },
    { BOOT_UT_MSD_FLUSH,    .data.msd_print =    { MAGIC_MD_CMD, CMDID_MSG_FLUSH,      0x0,              "" } },
    { BOOT_UT_XCMD,         .data.xcmd =         { MAGIC_MD_CMD, CMDID_BIN_LOAD_END,   0,                {0} } },
#endif
    { BOOT_UT_MBX,          .data.mbx =          { SDIOMAIL_BOOTING_REQ, 0 } },
    { BOOT_UT_XCMD,         .data.xcmd =         { MAGIC_MD_CMD, CMDID_BIN_LOAD_START, XBOOT_STAGE_BROM, {0} } },
    { BOOT_UT_XCMD_GETBIN,  .data.xcmd_getbin =  { MAGIC_MD_CMD, CMDID_GET_BIN,        0x50000,          0x4,        {0} } },
    { BOOT_UT_XCMD_GETBIN,  .data.xcmd_getbin =  { MAGIC_MD_CMD, CMDID_GET_BIN,        0x50004,          0x103E74,   {0} } },
    { BOOT_UT_XCMD,         .data.xcmd =         { MAGIC_MD_CMD, CMDID_MD_BOOT_END,    0,                {0} } },
};

#endif // _EEMCS_BOOT_UT

//
// Boot State String
//
unsigned char* g_md_sta_str[] = {
    "MD_INVALID",
    "MD_INIT",
    "MD_BROM_SDIO_INIT",
    "MD_BROM_SDIO_MBX_HS",    
    "MD_BROM_DL_START",
    "MD_BROM_DL_GET",
    "MD_BROM_DL_END",
    "MD_BROM_SEND_STATUS",
    "MD_BROM_SDDL_HS",
    "MD_BL_SDIO_INIT",
    "MD_BL_SDIO_MBX_HS",        
    "MD_BL_DL_START",
    "MD_BL_DL_GET",
    "MD_BL_DL_END",
    "MD_BOOT_END",
    "MD_ROM_SDIO_INIT",
    "MD_ROM_SDIO_MBX_HS",            
    "MD_ROM_BOOTING",
    "MD_ROM_BOOT_READY",
    "MD_ROM_EXCEPTION",
    NULL,
};

#define BOOT_CDEV_NAME "eemcs_ctrl"

extern void lte_sdio_on(void);
extern void lte_sdio_off(void);


/*modem image version definitions*/
static char * product_str[] = {	[INVALID_VARSION]=INVALID_STR, 
				[DEBUG_VERSION]=DEBUG_STR, 
				[RELEASE_VERSION]=RELEASE_STR};

static char * type_str[] = {	[modem_invalid]=VER_INVALID_STR,
				[modem_2g]=VER_2G_STR,
				[modem_3g]=VER_3G_STR,
				[modem_wg]=VER_WG_STR,
				[modem_tg]=VER_TG_STR,
				[modem_lwg]=VER_LWG_STR,
				[modem_ltg]=VER_LTG_STR,
				[modem_sglte]=VER_SGLTE_STR,
				};
typedef struct {
    char post_fix_name[EXT_MD_POST_FIX_LEN];
    modem_type_t   modem_type;
} MD_IMG_LIST;

static MD_IMG_LIST md_img_list[MD_IMG_MAX_CNT] = 
{ 
	{"2g_n",  modem_2g},
	{"3g_n",  modem_3g},
	{"wg_n",  modem_wg},
	{"tg_n",  modem_tg},
        {"lwg_n",  modem_lwg},
        {"ltg_n",  modem_ltg},
	{"sglte_n",  modem_sglte},
};

#if 0
static void memory_dump(void * start_addr, int size){
    unsigned int *curr_p = (unsigned int *)start_addr;
    int i = 0;
    for(i=0; i*4 <size; i++){
		DBGLOG(BOOT, DBG,"%03X: %08X %08X %08X %08X", 
				i*16, *curr_p, *(curr_p+1), *(curr_p+2), *(curr_p+3) );
		curr_p+=4;
	}
}
#endif

static unsigned int boot_time_out_val = 0;
static volatile unsigned int time_out_abort = 0;
static void bootup_to_monitor(unsigned long val);
static DEFINE_TIMER(bootup_to_monitor_timer, bootup_to_monitor, 0, 0);

static int query_feature_setting(char buff[])
{
	int ret = -1;
	
	if(buff == NULL)
		return ret;
	
	if(strcmp(buff, "MTK_LTE_DC_SUPPORT") == 0) {
		#ifdef MTK_LTE_DC_SUPPORT
		((int*)buff)[0] = 1;
		#else
		((int*)buff)[0] = 0;
		#endif
		DBGLOG(BOOT, INF, "query_feature_setting: MTK_LTE_DC_SUPPORT=%d", ((int*)buff)[0]);
		ret = 0;
	} else if (strcmp(buff, "GEMINI") == 0) {
		#ifdef GEMINI
		((int*)buff)[0] = 1;
		#else
		((int*)buff)[0] = 0;
		#endif
		DBGLOG(BOOT, INF, "query_feature_setting: GEMINI=%d", ((int*)buff)[0]);
		ret = 0;
	} else {
		DBGLOG(BOOT, ERR, "query_feature_setting fail: %s not exist", buff);
	}
	
	return ret;
}


int is_modem_debug_ver(void){
    return eemcs_boot_inst.md_info.img_info[MD_INDEX].img_chk_header.product_ver;
}

int scan_all_md_img(MD_IMG_LIST img_list[])
{
    struct file *filp = NULL;
    char full_path[64]={0};
    int i = 0, img_list_index = 0;
    int md_id = eemcs_boot_inst.md_id + 1;

    DEBUG_LOG_FUNCTION_ENTRY;

    if (!eemcs_boot_inst.md_img_list_scaned) {
        eemcs_boot_inst.md_img_list_scaned = 1;
    } else {
        DEBUG_LOG_FUNCTION_LEAVE;
        return 0;
    }

    //=========== Find MD image ====================== 
    for (i = 0; i< MD_IMG_MAX_CNT; i++){
        memset(&eemcs_boot_inst.md_img_file_list[img_list_index], 0, sizeof(img_mapping_t));
        // Find CIP first
        snprintf(full_path, 64, "%smodem_%d_%s.img", 
            CONFIG_MODEM_FIRMWARE_CIP_FOLDER, md_id, img_list[i].post_fix_name);
        filp = filp_open(full_path, O_RDONLY, 0777);
        if (IS_ERR(filp)) {
            // Find std path
            snprintf(full_path, 64, "%smodem_%d_%s.img", 
                CONFIG_MODEM_FIRMWARE_FOLDER, md_id, img_list[i].post_fix_name);
            filp = filp_open(full_path, O_RDONLY, 0777);
            if (IS_ERR(filp)) {
                continue;
            }
        }

        // If run to here, that means file found at CIP or STD
        filp_close(filp, NULL);
        // Copy post fix
        sprintf(eemcs_boot_inst.md_img_file_list[img_list_index].post_fix, "%d_%s", md_id, img_list[i].post_fix_name);
        // Copy full path
        sprintf(eemcs_boot_inst.md_img_file_list[img_list_index].full_path, "%s", full_path);
        // Update md image exist list
        eemcs_boot_inst.md_img_exist_list[img_list_index++] = img_list[i].modem_type;
        DBGLOG(BOOT, INF, "Find MD ROM %s", full_path);
    }
    memset(&eemcs_boot_inst.md_img_file_list[img_list_index], 0, sizeof(img_mapping_t));

    //=========== Find DSP image ====================== 
    img_list_index = 0;
    for (i = 0; i< MD_IMG_MAX_CNT; i++){
        memset(&eemcs_boot_inst.dsp_img_file_list[img_list_index], 0, sizeof(img_mapping_t));
        // Find CIP first
        snprintf(full_path, 64, "%sdsp_%d_%s.bin", 
            CONFIG_MODEM_FIRMWARE_CIP_FOLDER, md_id, img_list[i].post_fix_name);
        filp = filp_open(full_path, O_RDONLY, 0777);
        if (IS_ERR(filp)) {
            // Find std path
            snprintf(full_path, 64, "%sdsp_%d_%s.bin", 
                CONFIG_MODEM_FIRMWARE_FOLDER, md_id, img_list[i].post_fix_name);
            filp = filp_open(full_path, O_RDONLY, 0777);
            if (IS_ERR(filp)) {
                continue;
            }
        }

        // If run to here, that means file found at CIP or STD
        filp_close(filp, NULL);
        // Copy post fix
        sprintf(eemcs_boot_inst.dsp_img_file_list[img_list_index].post_fix, "%d_%s", md_id, img_list[i].post_fix_name);
        // Copy full path
        sprintf(eemcs_boot_inst.dsp_img_file_list[img_list_index].full_path, "%s", full_path);
        img_list_index++;
        DBGLOG(BOOT, INF, "Find DSP ROM %s", full_path);
    }
    memset(&eemcs_boot_inst.dsp_img_file_list[img_list_index], 0, sizeof(img_mapping_t));

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_INT32 set_md_info(char* str)
{
    int i;
    MD_INFO *md_info = &eemcs_boot_inst.md_info;
    KAL_INT32 ret = 0;
    //sprintf(md_info->img_info[MD_INDEX].file_name, "%smodem_%s.img", CONFIG_MODEM_FIRMWARE_FOLDER, str);
    //sprintf(md_info->img_info[DSP_INDEX].file_name, "%sdsp_%s.bin", CONFIG_MODEM_FIRMWARE_FOLDER, str);
    // Find and set MD
    for(i=0; i<MD_IMG_MAX_CNT; i++) {
       	if(eemcs_boot_inst.md_img_file_list[i].post_fix[0] != '\0') {
            if(strcmp(str, eemcs_boot_inst.md_img_file_list[i].post_fix) == 0) {
                // Find !
                sprintf(md_info->img_info[MD_INDEX].file_name, "%s", eemcs_boot_inst.md_img_file_list[i].full_path);
                break;
            }
        } else {
            ret = -1;
            DBGLOG(BOOT, ERR, "modem_%s.img not exist", str);
            break;
        }
    }

    // Find and set DSP
    for(i=0; i<MD_IMG_MAX_CNT; i++) {
        if(eemcs_boot_inst.dsp_img_file_list[i].post_fix[0] != '\0') {
            if(strcmp(str, eemcs_boot_inst.dsp_img_file_list[i].post_fix) == 0) {
                // Find !
                sprintf(md_info->img_info[DSP_INDEX].file_name, "%s", eemcs_boot_inst.dsp_img_file_list[i].full_path);
                break;
            }
        } else {
            ret = -1;
            DBGLOG(BOOT, ERR, "dsp_%s.bin not exist", str);
            break;
        }
    }
    
    get_ap_platform_ver(md_info->ap_platform);
    md_info->ap_info.image_type=type_str[get_ext_modem_support(eemcs_boot_inst.md_id)];
    md_info->ap_info.platform = md_info->ap_platform;
    return ret;
}

KAL_INT32 eemcs_get_md_info_str(char* info_str){
    MD_CHECK_HEADER *img_info= &eemcs_boot_inst.md_info.img_info[MD_INDEX].img_chk_header;
    AP_CHECK_INFO   *ap_info = &eemcs_boot_inst.md_info.ap_info;
	
    /* Construct image information string */
    return sprintf(info_str, "MD:%s*%s*%s*%s\nAP:%s*%s\n(MD)%s\n",
    			type_str[img_info->image_type],img_info->platform, 
    			img_info->build_ver,img_info->build_time,
    			ap_info->image_type,ap_info->platform,
    			product_str[img_info->product_ver]);
}

KAL_UINT32 eemcs_check_md_img_exist_list(void){
    struct file *filp = NULL;
    char full_path[64]={0};
    int i = 0, img_list_index = 0;

    DEBUG_LOG_FUNCTION_ENTRY;
    memset(eemcs_boot_inst.md_img_exist_list, 0, sizeof(eemcs_boot_inst.md_img_exist_list));
    for (i = 0; i< MD_IMG_MAX_CNT; i++){
        snprintf(full_path, 64, "%smodem_%d_%s.img", 
            CONFIG_MODEM_FIRMWARE_FOLDER,
            eemcs_boot_inst.md_id + 1,
            md_img_list[i].post_fix_name);

        filp = filp_open(full_path, O_RDONLY, 0777);
        if (IS_ERR(filp)) {
            DBGLOG(BOOT, ERR, "MD ROM %s not exist!", full_path);
            continue;
        }
        filp_close(filp, NULL);
        eemcs_boot_inst.md_img_exist_list[img_list_index++] = md_img_list[i].modem_type;
    }
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_UINT32* eemcs_get_md_img_exist_list(void){
    DEBUG_LOG_FUNCTION_ENTRY;
    eemcs_check_md_img_exist_list();
    DEBUG_LOG_FUNCTION_LEAVE;
    return eemcs_boot_inst.md_img_exist_list;
}

KAL_UINT32 eemcs_get_md_img_exist_list_size(void){
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return sizeof(eemcs_boot_inst.md_img_exist_list);
}

KAL_UINT32 eemcs_get_md_id(void){
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return eemcs_boot_inst.md_id;
}

KAL_INT32 eemcs_boot_get_ext_md_post_fix(char *post_fix)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    memcpy(post_fix, eemcs_boot_inst.ext_modem_post_fix, EXT_MD_POST_FIX_LEN);
    DEBUG_LOG_FUNCTION_LEAVE;

    return KAL_SUCCESS;
}

void eemcs_set_reload_image(KAL_INT32 need_reload){
    DBGLOG(BOOT, TRA, "eemcs_set_reload_image: md%d=%d", eemcs_boot_inst.md_id+1, need_reload);
    eemcs_boot_inst.md_need_reload = need_reload;
    memset(&eemcs_boot_inst.md_info.img_info[MD_INDEX].img_chk_header, 0, sizeof(MD_CHECK_HEADER));
}
/****************************************************************************/
#define ENABLE_MD_TYPE_CHECK
static int check_md_header( int md_id, 
							unsigned int parse_addr, 
							IMG_INFO *image)
{
	int ret;
	bool md_type_check = false;
	bool md_plat_check = false;
	bool md_sys_match  = false;
	bool md_size_check = false;
	MD_CHECK_HEADER *head = &image->img_chk_header;
    AP_CHECK_INFO *ap_info = &eemcs_boot_inst.md_info.ap_info;
    
#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
	memcpy(head, (void*)(parse_addr - sizeof(MD_CHECK_HEADER)), sizeof(MD_CHECK_HEADER));
#else
    {
        //get MD Check Header from none cipher image
        struct file *filp = NULL;
        mm_segment_t curr_fs = get_fs();
        set_fs(KERNEL_DS);
        DBGLOG(BOOT, INF, "Get header form MD%d image name=%s", eemcs_boot_inst.md_id+1, image->file_name);
        filp = filp_open(image->file_name, O_RDONLY, 0777);
        if (IS_ERR(filp)) {
            DBGLOG(BOOT, ERR, "open MD ROM %s fail", image->file_name);
            return -CCCI_ERR_LOAD_IMG_NOMEM;
        }
        
        filp->f_op->llseek(filp, 0, SEEK_END);
        filp->f_pos = filp->f_pos - sizeof(MD_CHECK_HEADER);
        filp->f_op->read(filp, (char*)head, sizeof(MD_CHECK_HEADER), &filp->f_pos);
        filp_close(filp, NULL);
        set_fs(curr_fs);  
    }
#endif

	DBGLOG(BOOT, INF, "**********************MD image check***************************");
	ret = strncmp(head->check_header, MD_HEADER_MAGIC_NO, 12);
	if(ret) {
		DBGLOG(BOOT, INF, "md check header not exist!");
		ret = 0;
	}
	else {
		if(head->header_verno != MD_HEADER_VER_NO) {
			DBGLOG(BOOT, INF, "[Error]md check header version mis-match to AP:[%d]!", 
				head->header_verno);
		} else {
			#ifdef ENABLE_MD_TYPE_CHECK
			if((head->image_type != modem_invalid) && (head->image_type == get_ext_modem_support(md_id))) {
				md_type_check = true;
			}
			#else
			md_type_check = true;
			#endif

			#ifdef ENABLE_CHIP_VER_CHECK
			if(!strncmp(head->platform, ap_info->ap_platorm, AP_PLATFORM_LEN)) {
				md_plat_check = true;
			}
			#else
			md_plat_check = true;
			#endif

			if(head->bind_sys_id == (md_id+1)) {
				md_sys_match = true;
			}

			#ifdef ENABLE_MEM_SIZE_CHECK
			if(head->header_verno >= 2) {
                unsigned int md_size = 0;
				md_size = get_ext_md_mem_size(md_id);
				if (head->mem_size == md_size) {
					md_size_check = true;
				} else if(head->mem_size < md_size) {
					md_size_check = true;
					DBGLOG(BOOT, WAR, "[Warning]md size in md header isn't sync to DFO setting: (%08x, %08x)",
						head->mem_size, md_size);
				}
			} else {
				md_size_check = true;
			}
			#else
			md_size_check = true;
			#endif



			if(md_type_check && md_plat_check && md_sys_match && md_size_check) {
				DBGLOG(BOOT, INF, "Modem header check OK!");
			}
			else {
				DBGLOG(BOOT, INF, "[Error]Modem header check fail!");
				if(!md_type_check){
					DBGLOG(BOOT, INF, "[Reason]MD type(2G/3G/LTE) mis-match to AP!");
                    aee_kernel_warning("[EEMCS]", "[ERROR] MD type(2G/3G/LTE) mis-match to AP!");
				}
				if(!md_plat_check)
					DBGLOG(BOOT, INF, "[Reason]MD platform mis-match to AP!");

				if(!md_sys_match)
					DBGLOG(BOOT, INF, "[Reason]MD image is not for MD SYS%d!", md_id+1);

				if(!md_size_check)
					DBGLOG(BOOT, INF, "[Reason]MD mem size mis-match to AP setting!");

				ret = -CCCI_ERR_LOAD_IMG_MD_CHECK;
			}

			DBGLOG(BOOT, INF, "(MD)[type]=%s, (AP)[type]=%s",type_str[image->img_chk_header.image_type], ap_info->image_type);
			DBGLOG(BOOT, INF, "(MD)[plat]=%s, (AP)[plat]=%s",image->img_chk_header.platform, ap_info->platform);
			if(head->header_verno >= 2)
				DBGLOG(BOOT, INF, "(AP)[size]=%x", image->mem_size);
			DBGLOG(BOOT, INF, "(MD)[build_ver]=%s, [build_time]=%s",(char*)image->img_chk_header.build_ver, (char*)image->img_chk_header.build_time);
			DBGLOG(BOOT, INF, "(MD)[product_ver]=%s", product_str[image->img_chk_header.product_ver]);
		}
	}
	DBGLOG(BOOT, INF, "**********************MD image check***************************");

	return ret;
}


/*********************************************************************************/
/*  API about security check                                                     */
/*                                                                               */
/*********************************************************************************/
#if defined(ENABLE_MD_IMG_SECURITY_FEATURE) || defined(ENABLE_MD_SECURITY_RO_FEATURE)
static int sec_lib_version_check(void)
{
	int ret = 0;

	int sec_lib_ver = masp_ccci_version_info();
	if(sec_lib_ver != CURR_SEC_CCCI_SYNC_VER){
		DBGLOG(BOOT, ERR, "[Error]sec lib for ccci mismatch: sec_ver:%d, ccci_ver:%d", sec_lib_ver, CURR_SEC_CCCI_SYNC_VER);
		ret = -1;
	}

	return ret;
}
#endif
#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
//--------------------------------------------------------------------------------------------------//
// New signature check version. 2012-2-2. 
// Change to use masp_ccci_signfmt_verify_file(char *file_path, unsigned int *data_offset, unsigned int *data_sec_len)
//  masp_ccci_signfmt_verify_file parameter description
//    @ file_path: such as etc/firmware/modem.img
//    @ data_offset: the offset address that bypass signature header
//    @ data_sec_len: length of signature header + tail
//    @ return value: 0-success;
//---------------------------------------------------------------------------------------------------//
static int signature_check_v2(int md_id, char* file_path, unsigned int *sec_tail_length)
{
	unsigned int bypass_sec_header_offset = 0;
	unsigned int sec_total_len = 0;

	if( masp_ccci_signfmt_verify_file(file_path, &bypass_sec_header_offset, &sec_total_len) == 0 ){
		//signature lib check success
		//-- check return value
		if(bypass_sec_header_offset > sec_total_len){
			DBGLOG(BOOT, ERR, "sign check fail(0x%x, 0x%x!)", bypass_sec_header_offset, sec_total_len);
			return -CCCI_ERR_LOAD_IMG_SIGN_FAIL;
		} else {
			DBGLOG(BOOT, TRA, "sign check success(0x%x, 0x%x)", bypass_sec_header_offset, sec_total_len);
			*sec_tail_length = sec_total_len - bypass_sec_header_offset;
			return (int)bypass_sec_header_offset; // Note here, offset is more than 2G is not hoped 
		}
	} else {
		DBGLOG(BOOT, ERR, "sign check fail!");
		return -CCCI_ERR_LOAD_IMG_SIGN_FAIL;
	}
}


static struct file *open_img_file(char *name, int *sec_fp_id)
{
	#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
	int fp_id = OSAL_FILE_NULL;
	fp_id = osal_filp_open_read_only(name);  
	DBGLOG(BOOT, TRA, "sec_open: fd=%d", fp_id); 

	if(sec_fp_id != NULL)
		*sec_fp_id = fp_id;

    DBGLOG(BOOT, TRA, "sec_open: file_ptr=0x%x", (unsigned int)osal_get_filp_struct(fp_id)); 

	return (struct file *)osal_get_filp_struct(fp_id);
	#else
	//CCCI_DBG_COM_MSG("std_open!\n");
	return filp_open(name, O_RDONLY, 0644);// 0777
	#endif
}

static void close_img_file(struct file *filp_id, int sec_fp_id)
{
	#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
	DBGLOG(BOOT, TRA, "sec_close: fd=%d", sec_fp_id);
	osal_filp_close(sec_fp_id);
	#else
	filp_close(filp_id,current->files);
	#endif
}

/*********************************************************************************/
/* load modem&dsp image function                                                                                     */
/*                                                                                                                                   */
/*********************************************************************************/
static int load_cipher_firmware_v2( int md_id, 
									int fp_id, 
									IMG_INFO *img,
									unsigned int cipher_img_offset, 
									unsigned int cipher_img_len)
{
	int ret;
    unsigned int data_offset;
	void *addr = ioremap_nocache(img->address,cipher_img_len);
	img->remap_address = (unsigned int)addr;
	

	if (addr==NULL) {
		DBGLOG(BOOT, ERR,  "ioremap image fail!");
		ret = -CCCI_ERR_LOAD_IMG_NO_ADDR;
		goto out;
	}

	if(SEC_OK != (ret = masp_ccci_decrypt_cipherfmt(fp_id, cipher_img_offset, (char*)addr, cipher_img_len, &data_offset)) ) {
		DBGLOG(BOOT, ERR,  "cipher image decrypt fail:ret=%d!", ret);
		ret = -CCCI_ERR_LOAD_IMG_CIPHER_FAIL;
		goto unmap_out;
	}

	img->size = cipher_img_len;
	img->offset += data_offset;	
	addr+=cipher_img_len;

	ret=check_md_header(md_id, ((unsigned int)addr), img);

unmap_out:
//		iounmap(img->remap_address);
out:
	return ret;
}

static int load_std_firmware(int md_id, 
							 struct file *filp, 
							 IMG_INFO *img)
{
	void			*start;
	int				ret = 0;
	int				check_ret = 0;
	int				read_size = 0;
	mm_segment_t	curr_fs;
	unsigned long	load_addr;
	unsigned int	end_addr;
	const int		size_per_read = 1024 * 1024;
//		const int		size = 1024;

	curr_fs = get_fs();
	set_fs(KERNEL_DS);

	load_addr = img->address;
	filp->f_pos = img->offset;

	while (1) {
		// Map 1M memory
		start = ioremap_nocache((load_addr + read_size), size_per_read);
		//DBGLOG(BOOT, ERR,  "map %08x --> %08x\n", (unsigned int)(load_addr+read_size), (unsigned int)start);
		if (start <= 0) {
			DBGLOG(BOOT, ERR, "image ioremap fail: %d", (unsigned int)start);
			set_fs(curr_fs);
			return -CCCI_ERR_LOAD_IMG_NOMEM;
		}

		ret = filp->f_op->read(filp, start, size_per_read, &filp->f_pos);
		//if ((ret < 0) || (ret > size_per_read)) {
		if ((ret < 0) || (ret > size_per_read) || ((ret == 0) && (read_size == 0))) { //make sure image size isn't 0
			DBGLOG(BOOT, ERR, "image read fail: size=%d", ret);
			ret = -CCCI_ERR_LOAD_IMG_FILE_READ;
			goto error;
		} else if(ret == size_per_read) {
			read_size += ret;
			iounmap(start);
		} else {
			read_size += ret;
			img->size = read_size - img->tail_length; /* Note here, signatured image has file tail info. */
			DBGLOG(BOOT, INF, "%s, image_size=0x%x, read_size=0x%x, tail=0x%x", 
							img->file_name, img->size, read_size, img->tail_length);
			iounmap(start);
			break;
		}
	}


	start = ioremap_nocache(round_down(load_addr + img->size - 0x4000, 0x4000), 
				round_up(img->size, 0x4000) - round_down(img->size - 0x4000, 0x4000)); // Make sure in one scope
	end_addr = ((unsigned int)start + img->size - round_down(img->size - 0x4000, 0x4000));
	if((check_ret = check_md_header(md_id, end_addr, img)) < 0) {
		ret = check_ret;
		goto error;
	}
	iounmap(start);


	set_fs(curr_fs);
	DBGLOG(BOOT, INF, "Load %s (size=0x%x) to 0x%lx", img->file_name, read_size, load_addr);

    img->remap_address = (unsigned int)ioremap_nocache(img->address, img->size);
	return read_size;

error:
	iounmap(start);
	set_fs(curr_fs);
	return ret;
}

/***********************************************************************************************/
KAL_INT32 eemcs_boot_load_image(unsigned int md_id)
{
    struct file *filp = NULL;
    int				fp_id;
    int				ret = KAL_SUCCESS;
    int             offset = 0;
    unsigned int	sec_tail_length = 0;

    #ifdef ENABLE_MD_IMG_SECURITY_FEATURE
    unsigned int	img_len=0;
    #endif
    IMG_INFO *img = &eemcs_boot_inst.md_info.img_info[MD_INDEX];

    if (eemcs_boot_inst.md_need_reload == 0){
        DBGLOG(BOOT, INF, "md%d image no need reload", md_id+1);
        return KAL_SUCCESS;
    }
        
    DEBUG_LOG_FUNCTION_ENTRY;
    img->address  = get_ext_md_mem_start_addr(eemcs_boot_inst.md_id);
    img->mem_size = get_ext_md_mem_size(eemcs_boot_inst.md_id);    
    //clear_md_region_protection(eemcs_boot_inst.md_id);
    get_ext_md_post_fix(eemcs_boot_inst.md_id, eemcs_boot_inst.ext_modem_post_fix, NULL);
    if (set_md_info(eemcs_boot_inst.ext_modem_post_fix) < 0) {
        ret = -CCCI_ERR_LOAD_IMG_FILE_OPEN;
        goto out;
    }
   
	filp = open_img_file(img->file_name, &fp_id);
	if (IS_ERR(filp)) {
		DBGLOG(BOOT, ERR, "open %s fail: %ld", img->file_name, PTR_ERR(filp));
		ret = -CCCI_ERR_LOAD_IMG_FILE_OPEN;
		filp = NULL;
		goto out;
	} else {
		DBGLOG(BOOT, INF, "open %s OK", img->file_name);
	}

	//Begin to check header
	//only modem.img need check signature and cipher header
	//sign_check = false;
	sec_tail_length = 0;

	//step1:check if need to signature
	//offset=signature_check(filp);
	#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
	offset = signature_check_v2(md_id, img->file_name, &sec_tail_length);
	DBGLOG(BOOT, INF, "signature_check offset:%d, tail:%d", offset, sec_tail_length);
	if (offset<0) {
		DBGLOG(BOOT, ERR, "signature_check fail: %d", offset);
		ret=offset;
		goto out;
	}
	#endif

	img->offset=offset;
	img->tail_length = sec_tail_length;
    iounmap((void*)img->remap_address);
    
	//step2:check if need to cipher
	#ifdef ENABLE_MD_IMG_SECURITY_FEATURE       
	if (masp_ccci_is_cipherfmt(fp_id, offset, &img_len)) {// Cipher image
		DBGLOG(BOOT, INF, "cipher image");
		//ret=load_cipher_firmware(filp,img,&cipher_header);
		ret=load_cipher_firmware_v2(md_id, fp_id, img, offset, img_len);
		if(ret<0) {
			DBGLOG(BOOT, ERR, "load_cipher_firmware fail: %d", ret);
			goto out;
		}
		DBGLOG(BOOT, INF, "load_cipher_firmware done: %d", ret);
	} 
	else
	#endif
	{
		DBGLOG(BOOT, INF, "Not cipher image");
		ret=load_std_firmware(md_id, filp, img);
		if(ret<0) {
			DBGLOG(BOOT, ERR, "load_firmware fail: %d", ret);
			goto out;
		}
	}      

out:    
    if(filp != NULL){ 
		close_img_file(filp, fp_id);
	}
    DEBUG_LOG_FUNCTION_LEAVE;
    if (ret == KAL_SUCCESS){
        eemcs_boot_inst.md_need_reload = 0; 
    }
	return ret;
    
}

#endif

KAL_INT32 eemcs_boot_swint_callback(KAL_UINT32 status)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(BOOT, DBG, "[SWINT CALLBACK] status = 0x%08X", status);
    KAL_ASSERT(status != 0);
    eemcs_boot_inst.mailbox.d2h_wakeup_cond = 1;
    wake_up(&eemcs_boot_inst.mailbox.d2h_wait_q);
    DEBUG_LOG_FUNCTION_LEAVE;

    return KAL_SUCCESS;
}

KAL_INT32 eemcs_boot_rx_callback(struct sk_buff *skb, KAL_UINT32 private_data)
{
    CCCI_BUFF_T *buf = NULL;
    DEBUG_LOG_FUNCTION_ENTRY;

    KAL_ASSERT(boot_node != NULL);

    if (skb) {
        buf = (CCCI_BUFF_T *)skb->data;
        DBGLOG(BOOT, DBG, "CCCI_H: 0x%08X, 0x%08X, %02d, 0x%08X",
            buf->magic, buf->id, buf->channel, buf->reserved);

        /* check is a exception packet or not*/
        if(buf->magic == MD_EX_MAGIC){
            eemcs_expt_handshake(skb);
            return KAL_SUCCESS;
        } else if (buf->magic == MD_TSID_MAGIC) {
            eemcs_bootup_trace(skb);
            return KAL_SUCCESS;
        }
    }

    if (CDEV_OPEN == atomic_read(&boot_node->cdev_state)) {
        skb_queue_tail(&boot_node->rx_skb_list, skb);    // spin_lock_ireqsave inside, refering skbuff.c
        atomic_inc(&boot_node->rx_pkt_cnt);              // increase rx_pkt_cnt
        wake_up(&boot_node->rx_waitq);                   // wake up rx_waitq
    } else {
        DBGLOG(BOOT, WAR, "PKT DROP when PORT%d(%s) closed", boot_node->eemcs_port_id, \
			boot_node->cdev_name);
        dev_kfree_skb(skb);
    }

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_UINT32 eemcs_boot_get_state(void)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;

    return eemcs_boot_inst.boot_state;
}

static KAL_INT32 eemcs_boot_change_state(KAL_UINT32 new_state)
{
    DEBUG_LOG_FUNCTION_ENTRY;
	
	DBGLOG(BOOT, INF, "XBOOT_STA: %s(%d) -> %s(%d)", g_md_sta_str[eemcs_boot_inst.boot_state], \
		eemcs_boot_inst.boot_state, g_md_sta_str[new_state], new_state);
    eemcs_boot_inst.boot_state = new_state;
    eemcs_boot_trace_boot(new_state);

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

static KAL_INT32 eemcs_boot_mbx_read_noack(KAL_UINT32 *val, KAL_UINT32 size)
{
    //int value = -1;
    KAL_INT32 result = KAL_FAIL;
    DEBUG_LOG_FUNCTION_ENTRY;

    if (val == NULL || size < sizeof(KAL_UINT32) * 2) {
        DBGLOG(BOOT, ERR, "Invalid mailbox arguments");
        return -EMCS_ERR_INVALID_PARA;
    }

    wait_event(eemcs_boot_inst.mailbox.d2h_wait_q, eemcs_boot_inst.mailbox.d2h_wakeup_cond == 1 || time_out_abort);
    if (time_out_abort) {
        time_out_abort  = 0;
        DBGLOG(BOOT, ERR, "[RX]PORT0 get boot mailbox timeout");

        return -EMCS_ERR_TIMEOUT;
    } else {
        eemcs_boot_inst.mailbox.d2h_wakeup_cond = 0;
    }

    do {
        if (ccci_boot_mbx_read(val, size) != KAL_SUCCESS)
            break;
        result = KAL_SUCCESS;
    } while (0);
	
    if (*val == 0 && *(val + 1) == 0) {
        DBGLOG(BOOT, ERR, "D2H mailbox are not available");
        return -EMCS_ERR_CMDCRC;
    }

    DEBUG_LOG_FUNCTION_LEAVE;
    return result;
}

static KAL_INT32 eemcs_boot_mbx_write_noack(KAL_UINT32 *val, KAL_UINT32 size)
{
    KAL_INT32 result = KAL_FAIL;
    DEBUG_LOG_FUNCTION_ENTRY;

    if (!val || size < sizeof(KAL_UINT32)*2) {
        DBGLOG(BOOT, ERR, "Invalid mailbox arguments");
        return -EMCS_ERR_INVALID_PARA;
    }

    do {
        DBGLOG(BOOT, DBG, "boot mailbox: 0x%X, 0x%X", *val, *(val + 1));
        if (ccci_boot_mbx_write(val, size) != KAL_SUCCESS)
            break;
        result = KAL_SUCCESS;
    } while (0);

    DEBUG_LOG_FUNCTION_LEAVE;   
    return result;
}

static KAL_INT32 eemcs_boot_mbx_ack(void)
{
    KAL_INT32 result = KAL_SUCCESS;
    KAL_UINT32 mbx_data[2] = {0};
    DEBUG_LOG_FUNCTION_ENTRY;

    mbx_data[0] = SDIOMAIL_BOOTING_ACK;
    mbx_data[1] = 0;
    if (eemcs_boot_mbx_write_noack(mbx_data, sizeof(KAL_INT32)*2) != KAL_SUCCESS)
        result = KAL_FAIL;
    else
        eemcs_boot_trace_mbx(mbx_data, sizeof(KAL_UINT32)*2, TRA_TYPE_MBX_TX);

    DEBUG_LOG_FUNCTION_LEAVE;
    return result;
}

static KAL_INT32 eemcs_boot_mbx_refuse(void)
{
    KAL_INT32 result = KAL_SUCCESS;
    KAL_UINT32 mbx_data[2] = {0};
    DEBUG_LOG_FUNCTION_ENTRY;

    mbx_data[0] = SDIOMAIL_REF;
    mbx_data[1] = 0;
    if (eemcs_boot_mbx_write_noack(mbx_data, sizeof(KAL_INT32) * 2) != KAL_SUCCESS)
        result = KAL_FAIL;
    else
        eemcs_boot_trace_mbx(mbx_data, sizeof(KAL_UINT32) * 2, TRA_TYPE_MBX_TX);

    DEBUG_LOG_FUNCTION_LEAVE;
    return result;
}

struct sk_buff *prepare_xcmd(KAL_UINT32 magic, KAL_UINT32 msg_id, KAL_UINT32 status)
{
    struct sk_buff *new_skb = NULL;
    XBOOT_CMD *xcmd_header = NULL;

    new_skb = ccci_boot_mem_alloc(sizeof(XBOOT_CMD) + CCCI_BOOT_HEADER_ROOM, GFP_ATOMIC);
    if (new_skb == NULL) {
        DBGLOG(BOOT, ERR, "alloc skb fail");
        goto _fail;
    }

    skb_reserve(new_skb, CCCI_BOOT_HEADER_ROOM);
    xcmd_header = (XBOOT_CMD *)skb_put(new_skb, sizeof(XBOOT_CMD));
    memset(new_skb->data, 0, sizeof(XBOOT_CMD));
    xcmd_header->magic = magic;
    xcmd_header->msg_id = msg_id;
    xcmd_header->status = status;
    return new_skb;
_fail:
    return NULL;
}

KAL_INT32 eemcs_boot_cmd_ack(KAL_UINT32 ack_id)
{
    KAL_INT32 result = KAL_FAIL;
    struct sk_buff *new_skb = NULL;
    XBOOT_CMD *xcmd_header = NULL;
    DEBUG_LOG_FUNCTION_ENTRY;

    if (ccci_boot_write_space_check() == 0) {
        DBGLOG(BOOT, ERR, "Memory space is not available on TX_Q_0!");
        goto _fail;
    }

    new_skb = prepare_xcmd(MAGIC_MD_CMD_ACK, ack_id, XBOOT_OK);
    if (new_skb == NULL) {
        DBGLOG(BOOT, ERR, "Failed to alloc skb!");
        goto _fail;
    }

    xcmd_header = (XBOOT_CMD *)new_skb->data;
    eemcs_boot_trace_xcmd(xcmd_header, TRA_TYPE_XCMD_TX);

    if (ccci_boot_write_desc_to_q(new_skb) != KAL_SUCCESS) {
        DBGLOG(BOOT, ERR, "Failed to send xBoot ack!");
        goto _write_fail;
    }
    result = KAL_SUCCESS;
    goto _ok;

_write_fail:
    dev_kfree_skb(new_skb);
_fail:
_ok:
    DEBUG_LOG_FUNCTION_LEAVE;
    return result;
}

KAL_UINT8 eemcs_boot_calc_checksum(KAL_UINT8 *data, KAL_UINT32 len, KAL_UINT8 *sum)
{
    KAL_UINT8 cs = 0;
    KAL_UINT32 idx = 0;

    for (idx = 0; idx < len; idx++) {
        cs += data[idx];
    }
    if (sum != NULL)
        *sum = cs;
    return ~cs;
}

//#define _EEMCS_BOOT_FILE_PACKET_TRACE   // Trace each file packet
//#define _EEMCS_BOOT_DEBUG_IMG           // Debug sent image data

KAL_INT32 eemcs_boot_file_packet_ack(KAL_UINT32 file_offset, KAL_UINT32 size)
{
    KAL_UINT8 checksum = 0;
    KAL_UINT32 bin_block_count = 0;
#if defined(_EEMCS_BOOT_FILE_PACKET_TRACE) || defined(_EEMCS_TRACE_SUPPORT)    
    KAL_UINT8 sum = 0;
    KAL_UINT8 sum_tmp = 0;
#endif
    KAL_UINT32 tx_pkt_room = 0;
    KAL_INT32 result = KAL_FAIL;
    KAL_UINT32 bin_offset = file_offset;
    mm_segment_t curr_fs;
    struct file *filp = NULL;
#ifdef _EEMCS_BOOT_DEBUG_IMG
    struct file *s_filp = NULL;
    char s_name[256] = {0};
#endif // _EEMCS_BOOT_DEBUG_IMG
    struct sk_buff *new_skb = NULL;
    char full_path[64];
    DEBUG_LOG_FUNCTION_ENTRY;

    curr_fs = get_fs();
    set_fs(KERNEL_DS);

    // Open modem.img
    snprintf(full_path, 64, "%s", eemcs_boot_inst.md_info.img_info[MD_INDEX].file_name);
    DBGLOG(BOOT, DBG, "MD%d image name=%s", eemcs_boot_inst.md_id+1, full_path);
    filp = filp_open(full_path, O_RDONLY, 0777);
    if (IS_ERR(filp)) {
        DBGLOG(BOOT, ERR, "open MD ROM %s fail", full_path);
        goto _open_fail;
    }
#ifdef _EEMCS_BOOT_DEBUG_IMG
    sprintf(s_name, "/data/app/dbg_0x%08X_0x%08X.dat", file_offset, size);
    s_filp = filp_open(s_name, O_WRONLY | O_CREAT, 0777);
    if (IS_ERR(s_filp)) {
        DBGLOG(BOOT, ERR, "open debug image file %s fail: %d", s_name);
    } else {
        DBGLOG(BOOT, DBG, "Debug image file to %s", s_name);
    }
#endif // _EEMCS_BOOT_DEBUG_IMG
    DBGLOG(BOOT, DBG, "Prepare md image:(OFFSET,LEN)=(0x%X,0x%X)", file_offset, size);

    while (size > 0) {
        KAL_UINT32 skb_size = 0;
        KAL_UINT32 bin_size = 0;
        //KAL_UINT32 retry_cnt = 0;
        unsigned char *bin_head = NULL;

        new_skb = NULL;
        while ((tx_pkt_room = ccci_boot_write_space_check()) <= 0){
            //if ((retry_cnt++ % 1000) == 0)
            //    DBGLOG(BOOT, ERR, "ccci_boot_write_space_check: no space!");
			
            if (time_out_abort) {
                DBGLOG(BOOT, ERR, "send md image timeout");
                time_out_abort = 0;
                result = -EMCS_ERR_TIMEOUT;
                goto _skb_fail;
            }
        };
        //retry_cnt = 0;
		
        if (tx_pkt_room > 0) {
            #ifdef CCCI_SDIO_HEAD
            if (size > (g_eemcs_file_pkt_len - CCCI_BOOT_HEADER_ROOM)) {
                skb_size = g_eemcs_file_pkt_len;
                bin_size = g_eemcs_file_pkt_len - CCCI_BOOT_HEADER_ROOM;
            } else {
                skb_size = size + CCCI_BOOT_HEADER_ROOM;
                bin_size = size;
            }
            #else
            if (size > g_eemcs_file_pkt_len) {
                skb_size = g_eemcs_file_pkt_len;
                bin_size = g_eemcs_file_pkt_len;
            } else {
                skb_size = size;
                bin_size = size;
            }
            #endif
            new_skb = ccci_boot_mem_alloc(skb_size, GFP_ATOMIC);
        } else {
            DBGLOG(BOOT, WAR, "SWQ space is not available");
        }
        if (new_skb == NULL) {
            DBGLOG(BOOT, ERR, "boot_mem_alloc_skb fail");
            goto _skb_fail;
        }
        // Reserve a header room for feature usage
        #ifdef CCCI_SDIO_HEAD
        skb_reserve(new_skb, CCCI_BOOT_HEADER_ROOM);
        #endif
        bin_head = skb_put(new_skb, bin_size);
        
        // Read data from modem.img
        #ifdef ENABLE_MD_IMG_SECURITY_FEATURE
        {
            IMG_INFO *img = &eemcs_boot_inst.md_info.img_info[MD_INDEX];
            //DBGLOG(BOOT, DBG, "Read image from memory %#X", (unsigned int)(img->remap_address + bin_offset) );
            memcpy(bin_head, (void *)(img->remap_address  + bin_offset), bin_size);         
        }
        #else
        filp->f_pos = bin_offset;
        result = filp->f_op->read(filp, bin_head, bin_size, &filp->f_pos);
        if (result <= 0) {
            DBGLOG(BOOT, ERR, "read %s %dB(%d, off=%d) fail: %d", full_path, bin_size, \
				size, bin_offset, result);
            goto _read_write_fail;
        }
        #endif
    
#ifdef _EEMCS_BOOT_DEBUG_IMG
        s_filp->f_op->write(s_filp, bin_head, bin_size, &s_filp->f_pos);
        DBGLOG(BOOT, INF, "Write debug image file. Size = %d, Pos = %d", bin_size, s_filp->f_pos);
#endif // _EEMCS_BOOT_DEBUG_IMG

#if defined(_EEMCS_BOOT_FILE_PACKET_TRACE) || defined(_EEMCS_TRACE_SUPPORT)
        checksum = eemcs_boot_calc_checksum(bin_head, bin_size, &sum_tmp);
    #if defined(_EEMCS_BOOT_FILE_PACKET_TRACE)
        eemcs_boot_trace_xcmd_file(bin_offset, bin_size, checksum);
    #else // !_EEMCS_BOOT_FILE_PACKET_TRACE
        sum += sum_tmp;
    #endif // _EEMCS_BOOT_FILE_PACKET_TRACE
#endif // _EEMCS_BOOT_FILE_PACKET_TRACE || _EEMCS_TRACE_SUPPORT

        // Write out
        if (ccci_boot_write_desc_to_q(new_skb) != KAL_SUCCESS) {
            DBGLOG(BOOT, ERR, "send xBoot file fail");
            goto _read_write_fail;
        }
        size = size - bin_size;
        bin_offset = bin_offset + bin_size;
        KAL_ASSERT(size >= 0);
        if ((bin_block_count++ % 100) == 0){
//	            printk(KERN_ERR "."); // Print with prefix
//	            printk_emit(0,3,NULL,0,"."); // Print without prefix
            DBGLOG(BOOT, INF, "MD Image Loading..");  // Print line by line
        }
//	        msleep(50);
    };
#ifdef _EEMCS_BOOT_DEBUG_IMG
    filp_close(s_filp, NULL);
#endif
#if defined(_EEMCS_TRACE_SUPPORT)
    checksum = ~sum;
#endif // _EEMCS_TRACE_SUPPORT
    eemcs_boot_trace_xcmd_file(file_offset, bin_offset - file_offset, checksum);
    result = KAL_SUCCESS;
    goto _ok;

_read_write_fail:
    dev_kfree_skb(new_skb);
_skb_fail:
_ok:
    filp_close(filp, NULL);
    set_fs(curr_fs);
_open_fail:
    DEBUG_LOG_FUNCTION_LEAVE;
    return result;
}

static KAL_INT32 eemcs_boot_mbx_process(void)
{
    KAL_INT32 result = -1;
    KAL_UINT32 mbx_data[2] = {0};
    DEBUG_LOG_FUNCTION_ENTRY;

    DBGLOG(BOOT, INF, "wait for boot mailbox ....");
    result = eemcs_boot_mbx_read_noack(mbx_data, sizeof(KAL_UINT32)*2);
    if (result != KAL_SUCCESS) {
        //DBGLOG(BOOT, ERR, "read mailbox fail: %d",result);
        return result;
    }

	DBGLOG(BOOT, DBG, "XBOOT_MBX: 0x%08X, 0x%08X", mbx_data[0], mbx_data[1]);
    eemcs_boot_trace_mbx(mbx_data, sizeof(KAL_UINT32)*2, TRA_TYPE_MBX_RX);

    switch(mbx_data[0])
    {
        case SDIOMAIL_BOOTING_REQ:	
            DBGLOG(BOOT, INF, "XBOOT_MBX: BOOTING_REQ @%s", g_md_sta_str[eemcs_boot_inst.boot_state]);
            if (eemcs_boot_get_state() == MD_BROM_SDIO_MBX_HS)
                eemcs_boot_change_state(MD_BROM_DL_START);
            else if(eemcs_boot_get_state() == MD_BL_SDIO_MBX_HS)
                eemcs_boot_change_state(MD_BL_DL_START);
            else
                DBGLOG(BOOT, ERR, "XBOOT_MBX: BOOTING_REQ @Invalid Status(%d)", eemcs_boot_inst.boot_state);
            result = eemcs_boot_mbx_ack();
            break;

        case SDIOMAIL_DL_REQ:			
            DBGLOG(BOOT, INF, "XBOOT_MBX: DL_REQ @%s", g_md_sta_str[eemcs_boot_inst.boot_state]);
            if(eemcs_boot_get_state() == MD_BROM_SDDL_HS)
                eemcs_boot_change_state(MD_BROM_SEND_STATUS);
            else
                DBGLOG(BOOT, ERR, "XBOOT_MBX: BOOT_REQ @Invalid Status(%d)", eemcs_boot_inst.boot_state);
            result = eemcs_boot_mbx_refuse();
            break;

        default:
            DBGLOG(BOOT, INF, "XBOOT_MBX:(0x%08X, 0x%08X) @%s", mbx_data[0], mbx_data[1], \
				g_md_sta_str[eemcs_boot_inst.boot_state]);
            result = eemcs_boot_mbx_refuse();
    }
    DEBUG_LOG_FUNCTION_LEAVE;
    if (result != KAL_SUCCESS)
        return KAL_FAIL;
    else
        return KAL_SUCCESS;
}

static KAL_INT32 eemcs_boot_cmd_process(void)
{
    KAL_INT32 result = KAL_FAIL;
    KAL_UINT32 offset = 0;
    KAL_UINT32 len = 0;
    KAL_UINT32 do_tx_transfer = 0;
    KAL_UINT32 rx_pkt_cnt = 0;
    KAL_UINT32 ack_id = 0;
    struct sk_buff *rx_skb = NULL;
    XBOOT_CMD *p_xcmd = NULL;
    XBOOT_CMD_PRINT *print_cmd = NULL;
    int i = 0;
    DEBUG_LOG_FUNCTION_ENTRY;

    KAL_ASSERT(boot_node != NULL);
    DBGLOG(BOOT, INF, "Wait for boot cmd ...");
    wake_lock_timeout(&eemcs_boot_inst.eemcs_boot_wake_lock, HZ/2); 

    do {
        wait_event(boot_node->rx_waitq, (atomic_read(&boot_node->rx_pkt_cnt) > 0) || time_out_abort);
        if (time_out_abort) {
            result = -EMCS_ERR_TIMEOUT;
            time_out_abort  = 0;
            DBGLOG(BOOT, ERR, "[RX]PORT0 get boot cmd timeout");
            goto _exit;
        } else {
            break;
        }
    } while(1);
    DBGLOG(BOOT, DBG, "Wait boot cmd done");    

    rx_skb = skb_dequeue(&boot_node->rx_skb_list);
    // There should be rx_skb in the list
    KAL_ASSERT(NULL != rx_skb);
    atomic_dec(&boot_node->rx_pkt_cnt);
    rx_pkt_cnt = atomic_read(&boot_node->rx_pkt_cnt);
    KAL_ASSERT(rx_pkt_cnt >= 0);
    p_xcmd = (XBOOT_CMD *)rx_skb->data;
    DBGLOG(BOOT, DBG, "XBOOT_CMD: 0x%08X, 0x%08X, 0x%08X, 0x%08X",\
        p_xcmd->magic, p_xcmd->msg_id, p_xcmd->status, p_xcmd->reserved[0]);
    eemcs_boot_trace_xcmd(p_xcmd, TRA_TYPE_XCMD_RX);

    /* For reset in boot modem function*/
    if ((p_xcmd->magic == CCCI_MAGIC_NUM)){ 
        DBGLOG(BOOT, ERR, "Reset modem at md boot stage!");
        eemcs_cdev_msg(CCCI_PORT_CTRL, p_xcmd->msg_id, 0);
        result = -EMCS_ERR_RESET_MD;
        goto _free_exit;
    }

    if (p_xcmd->magic != (unsigned int)MAGIC_MD_CMD) {
        DBGLOG(BOOT, ERR, "invalid XBOOT CMD Magic: 0x%X!=0x%X", p_xcmd->magic, MAGIC_MD_CMD);
        result = -EMCS_ERR_CMDCRC;
        goto _free_exit;
    }

    switch(p_xcmd->msg_id)
    {
        case CMDID_MD_BUF_SIZE_CHANGE:
        {
            XBOOT_CMD_BUFSIZE *p_cmd = (XBOOT_CMD_BUFSIZE*)p_xcmd;
            g_eemcs_file_pkt_len = p_cmd->buf_size;
            DBGLOG(BOOT, INF, "XBOOT_CMD: MD_BUF_SIZE_CHANGE=%d", g_eemcs_file_pkt_len);
            ack_id = CMDID_ACK_MD_BUF_SIZE_CHANGE;
            break;
        }
        case CMDID_BIN_LOAD_START:
        {
            DBGLOG(BOOT, INF, "XBOOT_CMD: BIN_LOAD_START @%s", g_md_sta_str[eemcs_boot_inst.boot_state]);
            if (eemcs_boot_get_state() == MD_BROM_DL_START)
                eemcs_boot_change_state(MD_BROM_DL_GET);
            else if (eemcs_boot_get_state() == MD_BL_DL_START)
                eemcs_boot_change_state(MD_BL_DL_GET);
            else {
                DBGLOG(BOOT, ERR, "unexpected XBOOT_CMD: BIN_LOAD_START @%s", g_md_sta_str[eemcs_boot_inst.boot_state]);
                result = -EMCS_ERR_BT_STATUS;
                goto _free_exit;
            }
            ack_id = CMDID_ACK_BIN_LOAD_START;
            break;
         }   
        case CMDID_GET_BIN:
        {
            XBOOT_CMD_GETBIN *p_cmd = (XBOOT_CMD_GETBIN *)p_xcmd;
            offset = p_cmd->offset;
            len = p_cmd->len;
			
            DBGLOG(BOOT, INF, "XBOOT_CMD: GET_BIN(%x, %x) @%s", offset, len, \
				g_md_sta_str[eemcs_boot_inst.boot_state]);
            if (eemcs_boot_get_state() == MD_BROM_DL_GET)
                eemcs_boot_change_state(MD_BROM_DL_END);
            else if (eemcs_boot_get_state() == MD_BL_DL_GET)
                eemcs_boot_change_state(MD_BL_DL_END);
            else {
                if ((eemcs_boot_get_state() != MD_BL_DL_END) && (eemcs_boot_get_state() != MD_BROM_DL_END)) {        
            		DBGLOG(BOOT, ERR, "unexpected XBOOT_CMD: GET_BIN @%s", g_md_sta_str[eemcs_boot_inst.boot_state]);
                   	result = -EMCS_ERR_BT_STATUS;
                   	goto _free_exit;
                }
            }
            ack_id = CMDID_ACK_GET_BIN;
            // set flag for transfer.
            do_tx_transfer = 1;
            break;
        }
        case CMDID_BIN_LOAD_END:
        {
            DBGLOG(BOOT, INF, "XBOOT_CMD: BIN_LOAD_END @%s", g_md_sta_str[eemcs_boot_inst.boot_state]);
            if (eemcs_boot_get_state() == MD_BROM_DL_END)
                eemcs_boot_change_state(MD_BROM_SDDL_HS);
            else if (eemcs_boot_get_state() == MD_BL_DL_END)
                eemcs_boot_change_state(MD_BL_SDIO_INIT);
            else {
               	DBGLOG(BOOT, ERR, "unexpected XBOOT_CMD: BIN_LOAD_END @%s", g_md_sta_str[eemcs_boot_inst.boot_state]);
                result = -EMCS_ERR_BT_STATUS;
                goto _free_exit;
            }
            ack_id = CMDID_ACK_BIN_LOAD_END;
            break;
        }
        case CMDID_MD_BOOT_END:
        {		
            DBGLOG(BOOT, INF, "XBOOT_CMD: MD_BOOT_END @%s", g_md_sta_str[eemcs_boot_get_state()]);
            if (eemcs_boot_get_state() == MD_BL_DL_END || eemcs_boot_get_state() == MD_BL_DL_GET) {
                eemcs_boot_change_state(MD_ROM_SDIO_INIT);
                change_device_state(EEMCS_MOLY_HS_P1);
            } else {
                DBGLOG(BOOT, ERR, "unexpected XBOOT_CMD: MD_BOOT_END @%s", g_md_sta_str[eemcs_boot_inst.boot_state]);
                result = -EMCS_ERR_BT_STATUS;
                goto _free_exit;
            }
            ack_id = CMDID_ACK_MD_BOOT_END;
            break;
        }
        case CMDID_MD_MSD_OUTPUT:
        {		
            DBGLOG(BOOT, INF, "XBOOT_CMD: MD_MSD_OUTPUT @%s", g_md_sta_str[eemcs_boot_inst.boot_state]);
            print_cmd = (XBOOT_CMD_PRINT *)p_xcmd;
            if (unlikely((eemcs_boot_inst.debug_offset + print_cmd->str_len) >= BOOT_DEBUG_BUF_LEN)) {
                DBGLOG(BOOT, ERR, "xBoot print command overflow: len=%d", print_cmd->str_len);
                result = -EMCS_ERR_MSG_OVERFLOW;
                goto _free_exit;
            }
            memcpy(eemcs_boot_inst.debug_buff + eemcs_boot_inst.debug_offset, print_cmd->str, print_cmd->str_len);
            eemcs_boot_inst.debug_offset += print_cmd->str_len;
            ack_id = CMDID_ACK_MD_MSD_OUTPUT;
            break;
        }
        case CMDID_MSG_FLUSH:
        {
            DBGLOG(BOOT, INF, "XBOOT_CMD: MSG_FLUSH @%s", g_md_sta_str[eemcs_boot_inst.boot_state]);
            print_cmd = (XBOOT_CMD_PRINT *)p_xcmd;
            if (unlikely((eemcs_boot_inst.debug_offset + print_cmd->str_len) >= BOOT_DEBUG_BUF_LEN)) {
                DBGLOG(BOOT, ERR, "xBoot print command overflow: len=%d", print_cmd->str_len);
                result = -EMCS_ERR_MSG_OVERFLOW;
                goto _free_exit;
            }
            memcpy(eemcs_boot_inst.debug_buff + eemcs_boot_inst.debug_offset, print_cmd->str, print_cmd->str_len);
            eemcs_boot_inst.debug_offset += print_cmd->str_len;
            // output debug message whenever emcs debug level setting.
            printk("[EEMCS/BOOT] MD_Debug_Log(len=%d):\n", eemcs_boot_inst.debug_offset);
            for (i = 0; i < eemcs_boot_inst.debug_offset; i++)
                 printk("%c", *(eemcs_boot_inst.debug_buff + i));
            printk("\n");
            // Reset debug buffer
            eemcs_boot_inst.debug_offset = 0;
            memset(eemcs_boot_inst.debug_buff, 0, BOOT_DEBUG_BUF_LEN);

            if (eemcs_boot_get_state() == MD_BROM_SEND_STATUS)
                eemcs_boot_change_state(MD_BL_SDIO_INIT);
            else if (eemcs_boot_get_state() == MD_BL_DL_END)
                DBGLOG(BOOT, DBG, "[CMDID_MSG_FLUSH] NOTHING TO DO ...");
            ack_id = CMDID_ACK_MSG_FLUSH;
            break;
        }
        default:
        {
            DBGLOG(BOOT, ERR, "Invalid XBOOT_CMD id=0x%x", p_xcmd->msg_id);
            result = -EMCS_ERR_BT_STATUS;
            goto _free_exit;
        }
    }
    dev_kfree_skb(rx_skb);
    result = eemcs_boot_cmd_ack(ack_id);
    if (result != KAL_SUCCESS)
        goto _exit;
    if (do_tx_transfer) {
        result = eemcs_boot_file_packet_ack(offset, len);
    }

    DEBUG_LOG_FUNCTION_LEAVE;
    return result;
_free_exit:
    dev_kfree_skb(rx_skb);
_exit:
    DEBUG_LOG_FUNCTION_LEAVE;
    return result;
}

static KAL_INT32 eemcs_boot_mbx_init(void)
{
    KAL_INT32 result = KAL_SUCCESS;
    DEBUG_LOG_FUNCTION_ENTRY;

    init_waitqueue_head(&eemcs_boot_inst.mailbox.d2h_wait_q);
    init_waitqueue_head(&eemcs_boot_inst.mailbox.h2d_wait_q);
#if 0 /*20130528 move to MD_INIT*/
	/* registered at MD_INIT, unregistered at MD_ROM_BOOT_READY*/
    eemcs_boot_inst.cb_id = ccci_boot_mbx_register(eemcs_boot_swint_callback);
    if (eemcs_boot_inst.cb_id == KAL_FAIL) {
        DBGLOG(BOOT, ERR, "Failed to register SWINT callback !!");
        result = KAL_FAIL;
    }
#endif	
    DEBUG_LOG_FUNCTION_LEAVE;
    return result;
}

static int eemcs_boot_open(struct inode *inode,  struct file *file)
{
	int id = iminor(inode);
    int ret = 0;
    DEBUG_LOG_FUNCTION_ENTRY;

    DBGLOG(BOOT, DEF, "open PORT%d(%s)", id, eemcs_boot_inst.boot_node.cdev_name);

    //4 <1> check multiple open
    if(CDEV_OPEN == atomic_read(&eemcs_boot_inst.boot_node.cdev_state)){
        DBGLOG(BOOT, ERR, "PORT%d(%s) multi-open fail", id, eemcs_boot_inst.boot_node.cdev_name);
        return -EIO;
    }
    //4 <2>  clear the rx_skb_list
    skb_queue_purge(&eemcs_boot_inst.boot_node.rx_skb_list);
    atomic_set(&eemcs_boot_inst.boot_node.rx_pkt_cnt, 0);
    //4 <3>  register ccci channel 
    ret = ccci_boot_register(eemcs_boot_inst.boot_node.ccci_ch.rx, eemcs_boot_rx_callback, 0);
    if(ret != KAL_SUCCESS){
        DBGLOG(BOOT, ERR, "PORT%d(%s) open fail", id, eemcs_boot_inst.boot_node.cdev_name);
        return -EIO;
    }
    
    file->private_data = &eemcs_boot_inst.boot_node;
    nonseekable_open(inode, file);
    atomic_set(&eemcs_boot_inst.boot_node.cdev_state, CDEV_OPEN);
    DEBUG_LOG_FUNCTION_LEAVE;
	return ret;
}

static int eemcs_boot_release(struct inode *inode, struct file *file)
{    
	int id = iminor(inode);
    DEBUG_LOG_FUNCTION_ENTRY;    

    DBGLOG(BOOT, INF, "release boot device: %s(%d)", eemcs_boot_inst.boot_node.cdev_name, id);

    atomic_set(&eemcs_boot_inst.boot_node.cdev_state, CDEV_CLOSE);
    skb_queue_purge(&eemcs_boot_inst.boot_node.rx_skb_list);
    atomic_set(&eemcs_boot_inst.boot_node.rx_pkt_cnt, 0);

    DEBUG_LOG_FUNCTION_LEAVE;
    return 0;
}

static ssize_t eemcs_boot_read(struct file *fp, char *buf, size_t count, loff_t *ppos)
{
    unsigned int flag;
    eemcs_cdev_node_t *curr_node = (eemcs_cdev_node_t *)fp->private_data;
    KAL_UINT8 port_id = curr_node->eemcs_port_id; /* port_id */
    KAL_UINT32 p_type, rx_pkt_cnt, read_len;
    struct sk_buff *rx_skb;
    unsigned char *payload=NULL;
    CCCI_BUFF_T *ccci_header;
    int ret = 0;

    DEBUG_LOG_FUNCTION_ENTRY;
    
    flag=fp->f_flags;
    DBGLOG(BOOT, TRA, "[RX]PORT%d User expect read_len=%d", port_id, count);

    p_type = ccci_get_port_type(port_id);
    if (p_type != EX_T_USER) {
        DBGLOG(BOOT, ERR, "[RX]PORT%d refuse user read: %d", port_id, p_type);
        goto _exit;                    
    }

    rx_pkt_cnt = atomic_read(&curr_node->rx_pkt_cnt);
    KAL_ASSERT(rx_pkt_cnt >= 0);

    if (rx_pkt_cnt == 0) {
        if (flag & O_NONBLOCK) {	
            ret = -EAGAIN;
            DBGLOG(BOOT, TRA, "[RX]PORT%d no data to read",port_id);
            goto _exit;
        }
        ret = wait_event_interruptible(curr_node->rx_waitq, (atomic_read(&curr_node->rx_pkt_cnt) > 0)||(time_out_abort));
        if (ret) {
            ret = -EINTR;
            DBGLOG(BOOT, ERR, "[RX]PORT%d Interrupted by syscall.signal=%lld", port_id, \
				*(long long *)current->pending.signal.sig);	
            goto _exit;
        } else if (time_out_abort) {
            time_out_abort = 0;
            ret = -EMCS_ERR_TIMEOUT;
            DBGLOG(BOOT, ERR, "[RX]PORT%d Interrupted by timeout abort", port_id);	
            goto _exit;
        }
    }

    /*Cached memory from last read fail */
    DBGLOG(BOOT, DBG, "[RX]PORT%d read rx_skb_list(pkt_len=%d)", port_id, rx_pkt_cnt);
    rx_skb = skb_dequeue(&curr_node->rx_skb_list);
    /* There should be rx_skb in the list */
    KAL_ASSERT(NULL != rx_skb);
    atomic_dec(&curr_node->rx_pkt_cnt);
    rx_pkt_cnt = atomic_read(&curr_node->rx_pkt_cnt);
    KAL_ASSERT(rx_pkt_cnt >= 0);

    ccci_header = (CCCI_BUFF_T *)rx_skb->data;

    DBGLOG(BOOT, DBG, "[RX]PORT%d data: 0x%08x, 0x%08x, %02d, 0x%08x",\
            port_id, ccci_header->data[0],ccci_header->data[1], ccci_header->channel, ccci_header->reserved);

    /*If not match please debug EEMCS CCCI demux skb part*/
    //KAL_ASSERT(ccci_header->channel == curr_node->ccci_ch.rx);
    if (ccci_header->channel != curr_node->ccci_ch.rx){
        dev_kfree_skb(rx_skb);
        DBGLOG(BOOT, ERR, "[RX]PORT%d drop pkt when Ch%d error: 0x%08x, 0x%08x, %02d, 0x%08x", \
                port_id, curr_node->ccci_ch.rx, ccci_header->data[0],ccci_header->data[1], ccci_header->channel, ccci_header->reserved);
        atomic_inc(&curr_node->rx_pkt_drop_cnt);
        goto _exit;
    }
    if (!(ccci_get_port_cflag(port_id) & EXPORT_CCCI_H)) {
        read_len = ccci_header->data[1] - sizeof(CCCI_BUFF_T);
        /* remove CCCI_HEADER */
        skb_pull(rx_skb, sizeof(CCCI_BUFF_T));
    } else {
        if (ccci_header->data[0] == CCCI_MAGIC_NUM) {
            read_len = sizeof(CCCI_BUFF_T); 
        } else {
            read_len = ccci_header->data[1];
        }
    }

    payload = (unsigned char*)rx_skb->data;
    if (count < read_len) {
        DBGLOG(BOOT, ERR, "[RX]PORT%d User has not enough memory(%d, %d)", 
                port_id, count, read_len);
        atomic_inc(&curr_node->rx_pkt_drop_cnt);
        ret = -ENOMEM;
        goto _exit;
    }

    DBGLOG(BOOT, DBG, "[RX]PORT%d copy_to_user(len=%d, %p->%p)\n", port_id, read_len, payload, buf);

    ret = copy_to_user(buf, payload, read_len);
    if (ret) {
        DBGLOG(BOOT, ERR, "[RX]PORT%d copy_to_user(len=%d, %p->%p) fail: %d", \
			port_id, read_len, payload, buf, ret);
    }       

    dev_kfree_skb(rx_skb);

    if (ret == 0) {
        DEBUG_LOG_FUNCTION_LEAVE;
        return read_len;
    }
_exit:    

    DEBUG_LOG_FUNCTION_LEAVE;
	return ret;
}

static ssize_t eemcs_boot_write(struct file *fp, const char __user *buf, size_t in_sz, loff_t *ppos)
{
    ssize_t ret   = -EMCS_ERR_INVAL_PARA;
    eemcs_cdev_node_t *curr_node = (eemcs_cdev_node_t *)fp->private_data;
    KAL_UINT8 port_id = curr_node->eemcs_port_id; /* port_id */
    KAL_UINT32 p_type, control_flag;    
    struct sk_buff *new_skb;
    CCCI_BUFF_T *ccci_header;
    size_t count = in_sz;
    size_t skb_alloc_size;
    KAL_UINT32 alloc_time = 0, curr_time = 0;
    
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(BOOT, TRA, "[TX]write deivce iminor(%d) (%s),length(%d)",port_id,curr_node->cdev_name,count);

    p_type = ccci_get_port_type(port_id);
    if (p_type != EX_T_USER) {
        DBGLOG(BOOT, ERR, "[TX]PORT%d refuse port(%d) access user port", port_id, p_type);
        goto _exit;                    
    }
    if (port_id != CCCI_PORT_CTRL) {
        DBGLOG(BOOT, ERR, "[TX]PORT%d is not CCCI_PORT_CTRL", port_id);
        goto _exit;                    
    }
    control_flag = ccci_get_port_cflag(port_id);

    if((control_flag & EXPORT_CCCI_H) && (count < sizeof(CCCI_BUFF_T)))
    {
        DBGLOG(BOOT,ERR,"[TX]PORT%d invalid wirte len(%d)", port_id, count);
        goto _exit;            
    }

    if(control_flag & EXPORT_CCCI_H){
        if(count > (MAX_TX_BYTE+sizeof(CCCI_BUFF_T))){
            DBGLOG(BOOT,WAR,"[TX]PORT%d wirte len (%d) > MTU (%d)!", port_id, count, MAX_TX_BYTE);
            count = MAX_TX_BYTE+sizeof(CCCI_BUFF_T);
        }
        skb_alloc_size = count - sizeof(CCCI_BUFF_T);
    }else{
        if(count > MAX_TX_BYTE){
            DBGLOG(BOOT,WAR,"[TX]PORT%d wirte len (%d) > MTU (%d)!", port_id, count, MAX_TX_BYTE);
            count = MAX_TX_BYTE;
        }
        skb_alloc_size = count;
    }

    if (ccci_boot_write_space_check() == 0) {
        ret = -EMCS_ERROR_BUSY;
        DBGLOG(BOOT, ERR, "[TX]PORT%d ccci_boot_write_space_check return 0", port_id);
        goto _exit;
    }	

    new_skb = ccci_boot_mem_alloc((skb_alloc_size + CCCI_CDEV_HEADER_ROOM), GFP_ATOMIC);
    if (NULL == new_skb) {
        alloc_time = jiffies;		
        DBGLOG(BOOT, INF, "[TX]PORT%d alloc skb with wait flag", port_id);
        new_skb = ccci_boot_mem_alloc((skb_alloc_size + CCCI_CDEV_HEADER_ROOM), GFP_KERNEL);
        if (NULL == new_skb) {
            ret = -EMCS_ERROR_BUSY;
            DBGLOG(BOOT, ERR, "[TX]PORT%d alloc skb with wait flag fail", port_id);
            goto _exit; 
        }
		
        curr_time = jiffies;
        if ((curr_time - alloc_time) >= 1) {			
            DBGLOG(BOOT, ERR, "[TX]PORT%d alloc skb with wait flag delay: time=%dms", \
			port_id, 10*(curr_time - alloc_time));
        }
    }

    /* reserve SDIO_H header room */
    #ifdef CCCI_SDIO_HEAD
    skb_reserve(new_skb, CCCI_BOOT_HEADER_ROOM);
    #endif

    if (control_flag & EXPORT_CCCI_H) {
        ccci_header = (CCCI_BUFF_T *)new_skb->data;
    } else {
        ccci_header = (CCCI_BUFF_T *)skb_put(new_skb, sizeof(CCCI_BUFF_T)) ;
    }

    if (copy_from_user(skb_put(new_skb, count), buf, count)) {
        DBGLOG(BOOT, ERR, "[TX]PORT%d copy_from_user(len=%d, %p->%p) fail", \
			port_id, count, buf, new_skb->data);
        dev_kfree_skb(new_skb);
        goto _exit;
    }

    if(control_flag & EXPORT_CCCI_H) {
        /* user bring down the ccci header */
        if (count == sizeof(ccci_header)){
            ccci_header->data[0]= CCCI_MAGIC_NUM;
        } else {
            ccci_header->data[1]= count; 
        }

        if(ccci_header->channel != curr_node->ccci_ch.tx){
            DBGLOG(BOOT, WAR, "[TX]PORT%d ch mis-match (%d) vs (%d)!! will correct by char_dev ", \
				port_id, ccci_header->channel, curr_node->ccci_ch.tx);
        }
    } else {
        /* user bring down the payload only */
        ccci_header->data[1]    = count + sizeof(CCCI_BUFF_T);
        ccci_header->reserved   = 0;
    }
    ccci_header->channel = curr_node->ccci_ch.tx;

    DBGLOG(BOOT, TRA, "[TX]PORT%d CCCI_MSG(0x%x, 0x%x, %d, 0x%x)", 
                    port_id, 
                    ccci_header->data[0], ccci_header->data[1],
                    ccci_header->channel, ccci_header->reserved);

    ret = ccci_boot_write_desc_to_q(new_skb);

	if (KAL_SUCCESS != ret) {
		DBGLOG(BOOT, ERR, "[TX]PORT%d PKT DROP of ch%d!", port_id, curr_node->ccci_ch.tx);
		dev_kfree_skb(new_skb);
        ret = -EMCS_ERROR_BUSY;
	} else {
        atomic_inc(&curr_node->tx_pkt_cnt);
        wake_up(&curr_node->tx_waitq);
	}

_exit:
    DEBUG_LOG_FUNCTION_LEAVE;
    if(!ret){
        return count;
    }
    return ret;
}

unsigned int eemcs_boot_poll(struct file *fp,poll_table *wait)
{
    eemcs_cdev_node_t *curr_node = (eemcs_cdev_node_t *)fp->private_data;
    unsigned int mask = 0;
    
    DEBUG_LOG_FUNCTION_ENTRY;    
    DBGLOG(BOOT, DEF, "eemcs_boot_poll emcs poll enter");

	poll_wait(fp,&curr_node->tx_waitq, wait);  /* non-blocking, wake up to indicate the state change */
	poll_wait(fp,&curr_node->rx_waitq, wait);  /* non-blocking, wake up to indicate the state change */

    if (ccci_boot_write_space_check() != 0) {
        DBGLOG(BOOT, DEF, "eemcs_boot_poll TX avaliable");
        mask |= POLLOUT|POLLWRNORM;
    }

	if (0 != atomic_read(&curr_node->rx_pkt_cnt)) {
        DBGLOG(BOOT, DEF, "eemcs_boot_poll RX avaliable");
        mask |= POLLIN|POLLRDNORM;
    }
    DEBUG_LOG_FUNCTION_LEAVE;
    return mask;    
}

long eemcs_ctrl_ioctl(struct file *fp, unsigned int cmd, unsigned long arg);

static struct file_operations eemcs_boot_ops = {
	.owner          =   THIS_MODULE,
	.open           =   eemcs_boot_open,
	.read           =   eemcs_boot_read,
	.write          =   eemcs_boot_write,
	.release        =   eemcs_boot_release,
	.unlocked_ioctl =   eemcs_ctrl_ioctl,
	.poll           =   eemcs_boot_poll,  
};

#define EEMCS_BOOT_CDEV_MAX_NUM 1

extern struct class *eemcs_char_get_class(void);
/*
 * @brief Store packets of CCCI_PORT_CTRL port when
 *        MD exception occurs.
 * @param
 *     None
 * @return
 *     None
 */
void eemcs_boot_exception_log_pkts(void)
{
    KAL_UINT32 j = 0, pkt_cnt = 0;
    struct sk_buff *skb = NULL;

//    if (!is_valid_exception_rx_port(CCCI_PORT_CTRL)) {
        // get packet number in this port
        pkt_cnt = atomic_read(&boot_node->rx_pkt_cnt);
        if (pkt_cnt != 0) {		
            DBGLOG(BOOT, TRA, "%d packets in Rx list of port%d", pkt_cnt, boot_node->eemcs_port_id);
            for (j = 0; j < pkt_cnt; j++) {
                skb = skb_dequeue(&boot_node->rx_skb_list);
                if (skb != NULL) {
                    atomic_dec(&boot_node->rx_pkt_cnt);
                    // store skb to exception structure
                    eemcs_expt_log_port(skb, boot_node->eemcs_port_id);
                } else {
                    DBGLOG(BOOT, TRA, "dequeue NULL skb from port%d list", boot_node->eemcs_port_id);
                }
            }
        }
//    }
}

/*
 * @brief Exception callback function.
 * @param
 *     msg_id [in] Exception indicator.
 * @return
 *     None
 */
void eemcs_boot_exception_callback(KAL_UINT32 msg_id)
{
    DBGLOG(BOOT, INF, "Boot exception Callback 0x%X", msg_id);
    switch (msg_id) {
        /* 1st stage, MD exception occurs */
        case EEMCS_EX_INIT:
            eemcs_boot_exception_log_pkts();
            break;
        /* 2nd stage, DHL DL channel is ready */
        case EEMCS_EX_DHL_DL_RDY:
            break;
        /* 3rd stage, all MD exception initialization is done */
        case EEMCS_EX_INIT_DONE:
            break;
        default:
            DBGLOG(BOOT, ERR, "Unknown boot exception callback 0x%X", msg_id);
    }
}

void eemcs_boot_reset(void)
{
	/* registered at MD_INIT, unregistered at MD_ROM_BOOT_READY*/
	eemcs_boot_inst.cb_id = ccci_boot_mbx_register(eemcs_boot_swint_callback);
	KAL_ASSERT(eemcs_boot_inst.cb_id != KAL_FAIL);
	
	/* Rest Boot DL size*/
	g_eemcs_file_pkt_len = BOOT_TX_MAX_PKT_LEN;
	
	/* Clear boot rx queue*/
	skb_queue_purge(&eemcs_boot_inst.boot_node.rx_skb_list);
	atomic_set(&eemcs_boot_inst.boot_node.rx_pkt_cnt, 0);

	return;
}

KAL_INT32 eemcs_boot_mod_init(void)
{
//#ifdef __EEMCS_XBOOT_SUPPORT__
    KAL_INT32 result = KAL_SUCCESS;
    KAL_INT32 ret   = 0;
    dev_t boot_dev;
    ccci_port_cfg *curr_port_info = NULL;
    struct device *boot_devices;
    DEBUG_LOG_FUNCTION_ENTRY;

    memset(&eemcs_boot_inst, 0, sizeof(EEMCS_BOOT_SET));

    eemcs_boot_inst.md_id = MD_SYS5;
    eemcs_boot_inst.md_need_reload = 1; 
    
    KAL_ALLOCATE_PHYSICAL_MEM(eemcs_boot_inst.debug_buff, BOOT_DEBUG_BUF_LEN);
    eemcs_boot_inst.boot_state = MD_INVALID;
    eemcs_boot_inst.ccci_hs_bypass = 0;
 
    // register characer device region 
    ret = register_chrdev_region(MKDEV(EEMCS_DEV_MAJOR, START_OF_BOOT_PORT), EEMCS_BOOT_CDEV_MAX_NUM, EEMCS_DEV_NAME);
    if (ret) {
        DBGLOG(BOOT, ERR, "register_chrdev_region %s fail: %d", BOOT_CDEV_NAME, ret);
        ret = KAL_FAIL;
        goto _boot_register_chrdev_fail;
    }

    // allocate character device
    eemcs_boot_inst.eemcs_boot_chrdev = cdev_alloc();
    if (eemcs_boot_inst.eemcs_boot_chrdev == NULL) {
        DBGLOG(BOOT, ERR, "cdev_alloc %s fail", BOOT_CDEV_NAME);
        ret = KAL_FAIL;
        goto _boot_cdev_alloc_fail;
    }

    cdev_init(eemcs_boot_inst.eemcs_boot_chrdev, &eemcs_boot_ops);
    eemcs_boot_inst.eemcs_boot_chrdev->owner = THIS_MODULE;

    ret = cdev_add(eemcs_boot_inst.eemcs_boot_chrdev, MKDEV(EEMCS_DEV_MAJOR, START_OF_BOOT_PORT), EEMCS_BOOT_CDEV_MAX_NUM);
    if (ret) {
        DBGLOG(BOOT, ERR, "cdev_add %s fail: %d", BOOT_CDEV_NAME, ret);
        goto _boot_cdev_add_fail;
    }

    // Get device class from eemcs_char
    eemcs_boot_inst.dev_class = eemcs_char_get_class();

	boot_dev = MKDEV(EEMCS_DEV_MAJOR, START_OF_BOOT_PORT);
	boot_devices = device_create((struct class *)eemcs_boot_inst.dev_class, NULL, boot_dev, NULL, "%s", BOOT_CDEV_NAME);
	if (IS_ERR(boot_devices)) {
		ret = KAL_FAIL;
		DBGLOG(BOOT, ERR, "device_create %s fail: %ld", BOOT_CDEV_NAME, PTR_ERR(boot_devices));
    }

    memset(eemcs_boot_inst.boot_node.cdev_name, 0, sizeof(eemcs_boot_inst.boot_node.cdev_name)); 
    strncpy(eemcs_boot_inst.boot_node.cdev_name, BOOT_CDEV_NAME, sizeof(BOOT_CDEV_NAME));
 
    eemcs_boot_inst.boot_node.eemcs_port_id = CCCI_PORT_CTRL;
    curr_port_info = ccci_get_port_info(CCCI_PORT_CTRL);
    eemcs_boot_inst.boot_node.ccci_ch.rx = curr_port_info->ch.rx;
    eemcs_boot_inst.boot_node.ccci_ch.tx = curr_port_info->ch.tx;
    atomic_set(&eemcs_boot_inst.boot_node.cdev_state, CDEV_CLOSE);

    skb_queue_head_init(&eemcs_boot_inst.boot_node.rx_skb_list);
    atomic_set(&eemcs_boot_inst.boot_node.rx_pkt_cnt, 0);
    atomic_set(&eemcs_boot_inst.boot_node.rx_pkt_drop_cnt, 0);
    init_waitqueue_head(&eemcs_boot_inst.boot_node.rx_waitq);
    init_waitqueue_head(&eemcs_boot_inst.boot_node.tx_waitq);
    atomic_set(&eemcs_boot_inst.boot_node.tx_pkt_cnt, 0);
    boot_node = &eemcs_boot_inst.boot_node;
    
    init_waitqueue_head(&eemcs_boot_inst.rst_waitq);
	
    snprintf(eemcs_boot_inst.eemcs_boot_wakelock_name, sizeof(eemcs_boot_inst.eemcs_boot_wakelock_name), "eemcs_boot_wakelock");
    wake_lock_init(&eemcs_boot_inst.eemcs_boot_wake_lock, WAKE_LOCK_SUSPEND, eemcs_boot_inst.eemcs_boot_wakelock_name);

    DBGLOG(BOOT, DBG, "char dev(%s)(%d), rx_ch(%d), tx_ch(%d)",\
        eemcs_boot_inst.boot_node.cdev_name, eemcs_boot_inst.boot_node.eemcs_port_id,\
        eemcs_boot_inst.boot_node.ccci_ch.rx, eemcs_boot_inst.boot_node.ccci_ch.tx);

    result = eemcs_boot_mbx_init();

    /* register exception callback function */
    eemcs_boot_inst.expt_cb_id = ccci_boot_expt_register(eemcs_boot_exception_callback);
    if (eemcs_boot_inst.expt_cb_id == KAL_FAIL){
        DBGLOG(BOOT, ERR, "register exception callback fail");
    }
	
	atomic_set(&eemcs_boot_inst.md_reset_cnt, 0);
	
#ifdef _EEMCS_BOOT_UT
    bootut_init();
#endif

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
	
_boot_cdev_add_fail:
    cdev_del(eemcs_boot_inst.eemcs_boot_chrdev);
_boot_cdev_alloc_fail:
    unregister_chrdev_region(MKDEV(EEMCS_DEV_MAJOR, START_OF_BOOT_PORT), EEMCS_BOOT_CDEV_MAX_NUM);
_boot_register_chrdev_fail:
    DEBUG_LOG_FUNCTION_LEAVE;
    return ret;  

//#else // !__EEMCS_XBOOT_SUPPORT__
//    DEBUG_LOG_FUNCTION_ENTRY;
//    DEBUG_LOG_FUNCTION_LEAVE;
//    return KAL_SUCCESS;
//#endif // __EEMCS_XBOOT_SUPPORT__
}

void eemcs_boot_exit(void)
{
    DEBUG_LOG_FUNCTION_ENTRY;
//#ifdef __EEMCS_XBOOT_SUPPORT__
    if (eemcs_boot_inst.expt_cb_id != -1){
        ccci_boot_expt_unregister(eemcs_boot_inst.expt_cb_id);
    }
    device_destroy(eemcs_boot_inst.dev_class, MKDEV(EEMCS_DEV_MAJOR, START_OF_BOOT_PORT));
    
    if (eemcs_boot_inst.eemcs_boot_chrdev) {
        DBGLOG(BOOT, DBG, "eemcs_boot_chrdev unregister ");
        cdev_del(eemcs_boot_inst.eemcs_boot_chrdev);
    }
    unregister_chrdev_region(MKDEV(EEMCS_DEV_MAJOR,START_OF_BOOT_PORT), EEMCS_BOOT_CDEV_MAX_NUM);  

    KAL_FREE_PHYSICAL_MEM(eemcs_boot_inst.cmd_buff);
    KAL_FREE_PHYSICAL_MEM(eemcs_boot_inst.debug_buff);
    
    wake_lock_destroy(&eemcs_boot_inst.eemcs_boot_wake_lock);
    
//#else // !__EEMCS_XBOOT_SUPPORT__
//#endif // __EEMCS_XBOOT_SUPPORT__
    DEBUG_LOG_FUNCTION_LEAVE;
}

#define BOOT_DEBUG_STATE \
    do { \
        DBGLOG(BOOT, DBG, "xBoot State : %s", \
            g_md_sta_str[eemcs_boot_inst.boot_state]); \
    } while (0)

#ifdef _EEMCS_BOOT_UT
    KAL_INT32 bootut_start_ccci_handshake(void);
    KAL_INT32 bootut_lb(struct sk_buff *);
#endif // _EEMCS_BOOT_UT

static KAL_INT32 eemcs_boot_modem(void)
{
    KAL_INT32 result = -1;
    kal_bool state = false;
    DEBUG_LOG_FUNCTION_ENTRY;

#if !defined(__EEMCS_XBOOT_SUPPORT__)
    /* 20130418 ian bybass the emcs_boot_modem process and entering state MD_ROM_BOOT_READY */
    state = change_device_state(EEMCS_XBOOT);
    if (unlikely(state == false)) {
        DBGLOG(BOOT, ERR, "change state to XBOOT fail");
        goto _boot_err;
    }
    /* still go through each state */
    eemcs_boot_change_state(MD_INIT);
    lte_sdio_off();
    lte_sdio_on();    
    eemcs_boot_change_state(MD_BROM_SDIO_MBX_HS);
    eemcs_boot_change_state(MD_BROM_DL_START);
    eemcs_boot_change_state(MD_BROM_DL_GET);
    eemcs_boot_change_state(MD_BROM_DL_END);
    eemcs_boot_change_state(MD_BROM_SEND_STATUS);
    eemcs_boot_change_state(MD_BL_SDIO_INIT);
    eemcs_boot_change_state(MD_BL_SDIO_MBX_HS);
    eemcs_boot_change_state(MD_BL_DL_START);
    eemcs_boot_change_state(MD_BL_DL_GET);
    eemcs_boot_change_state(MD_BL_DL_END);
    eemcs_boot_change_state(MD_ROM_SDIO_INIT);
    eemcs_boot_change_state(MD_ROM_SDIO_MBX_HS);
    eemcs_boot_change_state(MD_ROM_BOOTING);
    eemcs_boot_change_state(MD_ROM_BOOT_READY);

    state = change_device_state(EEMCS_MOLY_HS_P1);
    if (unlikely(state == false)) {
        DBGLOG(BOOT, ERR, "change state to MOLY_HS_P1 fail");
        goto _boot_err;
    }
    
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
#endif

	atomic_set(&eemcs_boot_inst.md_reset_cnt, 0);

    state = change_device_state(EEMCS_XBOOT);
    if (unlikely(state == false)) {
        DBGLOG(BOOT, ERR, "change state to XBOOT fail");
        goto _boot_err;
    }

#if 0
    /* 20130418 remove the part which reset the embedded MD */
    *(volatile unsigned int*)(0xC0001830) = 0x06;
    printk("0xc0001830 =%d\n",*(volatile unsigned int*)(0xC0001830));
#endif

    eemcs_boot_change_state(MD_INIT);

    scan_all_md_img(md_img_list);

    #if defined(ENABLE_MD_IMG_SECURITY_FEATURE) || defined(ENABLE_MD_SECURITY_RO_FEATURE)
    static int masp_boot_inited = 0;
    int sec_ret = 0;
    if (!masp_boot_inited) {
    	DBGLOG(BOOT, TRA, "masp_boot_init\n");
        masp_boot_inited = 1;
        sec_ret = masp_boot_init();
        if (sec_ret != 0) {
            DBGLOG(BOOT, ERR, "masp_boot_init fail: %d\n",sec_ret);
            goto _boot_err;
        }

        sec_ret = sec_lib_version_check();
        if (sec_ret != 0) {
        	DBGLOG(BOOT, ERR, "sec lib version check error(%d)\n", sec_ret);
        	goto _boot_err;
        }
    }
    #endif

#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
    if((result = eemcs_boot_load_image(eemcs_boot_inst.md_id)) < 0){
        char error_srt[64] = {0};
        DBGLOG(BOOT, ERR, "Load Modem Image Fail: %d", result);
        sprintf(error_srt, "MD%d: Load Security Modem Image Fail!!! ERROR=%d !!! \n", eemcs_boot_inst.md_id+1, result);
        aee_kernel_warning("[EEMCS]", error_srt);
        goto _boot_err;
    }
#else
    get_ext_md_post_fix(eemcs_boot_inst.md_id, eemcs_boot_inst.ext_modem_post_fix, NULL);
    if( (result = set_md_info(eemcs_boot_inst.ext_modem_post_fix)) < 0)
        goto _boot_err;
    check_md_header(eemcs_boot_inst.md_id, 0, &eemcs_boot_inst.md_info.img_info[MD_INDEX]);
#endif

    // A big loop to do booting process
    while (1) {
        switch (eemcs_boot_get_state())
        {
            case MD_INIT:
            {
                eemcs_boot_change_state(MD_BROM_SDIO_MBX_HS);
                /* power off MD */
                lte_sdio_off();
                /* reset excecption info*/
                eemcs_expt_reset();
                /* reset boot dev node*/
                eemcs_boot_reset();
                /* reset char dev node*/
                eemcs_cdev_reset();
                /* reset ccci layer*/
                eemcs_ccci_reset();
                /* power on MD and SDIO card identify */
                lte_sdio_on();
                DBGLOG(BOOT, INF, "wait for sdio driver probe done");
                del_timer(&bootup_to_monitor_timer);

#ifdef MT_LTE_AUTO_CALIBRATION
                mtlte_sys_sdio_wait_probe_done();
#endif
                mod_timer(&bootup_to_monitor_timer,jiffies+boot_time_out_val*HZ);

#ifdef _EEMCS_BOOT_UT
                result = bootut_md_send_command();
                if (result != KAL_SUCCESS)
                    goto _boot_err;
#endif // _EEMCS_BOOT_UT
                DBGLOG(BOOT, INF, "MD%d start to run...", eemcs_boot_inst.md_id+1);
                break;
            }

            case MD_BROM_SDIO_MBX_HS:
            case MD_BL_SDIO_MBX_HS:
            case MD_BROM_SDDL_HS:
            {
                result = eemcs_boot_mbx_process();
                if (result != KAL_SUCCESS) {
                    DBGLOG(BOOT, ERR, "Mailbox process fail when eemcs_boot_state=%d", eemcs_boot_get_state());
                    goto _boot_err;
                }
#ifdef _EEMCS_BOOT_UT
                ccci_boot_write_desc_to_q(NULL);
#endif // _EEMCS_BOOT_UT
                break;
            }
            case MD_BROM_DL_START:
            case MD_BROM_DL_GET:
            case MD_BROM_DL_END:
            case MD_BL_DL_START:
            case MD_BL_DL_GET:
            case MD_BL_DL_END:
            case MD_BOOT_END:
            case MD_BROM_SEND_STATUS:
                result = eemcs_boot_cmd_process();
                if (result != KAL_SUCCESS) {
                    DBGLOG(BOOT, ERR, "xBoot commands process is failed !!");
                    goto _boot_err;
                }
                break;

            case MD_BL_SDIO_INIT:
                eemcs_boot_change_state(MD_BL_SDIO_MBX_HS);
                break;

            case MD_ROM_SDIO_INIT:
                eemcs_boot_change_state(MD_ROM_SDIO_MBX_HS);
                break;

            case MD_ROM_SDIO_MBX_HS:
                eemcs_boot_change_state(MD_ROM_BOOTING);
                break;

            case MD_ROM_BOOTING:
                eemcs_boot_change_state(MD_ROM_BOOT_READY);
                break;

            case MD_ROM_BOOT_READY:
				/* registered at MD_INIT, unregistered at MD_ROM_BOOT_READY*/
                if (eemcs_boot_inst.cb_id >= 0)
                    ccci_boot_mbx_unregister(eemcs_boot_inst.cb_id);
#ifdef _EEMCS_BOOT_UT
                bootut_start_ccci_handshake();
#else // !_EEMCS_BOOT_UT
                // If CCCI handshake is bypassed, we change EEMCS state to ready
                if (eemcs_boot_inst.ccci_hs_bypass) {
                    change_device_state(EEMCS_BOOTING_DONE);
                }
#endif // _EEMCS_BOOT_UT
                return KAL_SUCCESS;
                break;

            case MD_ROM_EXCEPTION:
                break;

            default:
                DBGLOG(BOOT, ERR, "Error State (%d) ???", eemcs_boot_get_state());
        }
//        DBGLOG(BOOT, DBG, "xBoot State : %s",
//            g_md_sta_str[eemcs_boot_get_state()]);
        BOOT_DEBUG_STATE;
        if (boot_reset_state){
            printk(KERN_ERR "@@@@@@@@@ Boot Reset Test: %s @@@@@@@@@@\n",
            g_md_sta_str[boot_reset_state]);
            while( boot_reset_state == eemcs_boot_get_state());
        }
        
    }

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;

_boot_err:
    DBGLOG(BOOT, ERR, "xBoot fail: %d", result);
    DEBUG_LOG_FUNCTION_LEAVE;
    return result;
}


static void eemcs_md_boot_up_timeout_func(unsigned int stage, unsigned int timeout)
{
	KAL_UINT32 mbx_data[2] = {0};
	char ex_info[EE_BUF_LEN]="";
	KAL_INT32 ret;

	switch (stage) {
		case MD_BOOT_XBOOT_FAIL:
			if (timeout > 0) {
				ret = ccci_boot_mbx_read(mbx_data, 2*sizeof(KAL_UINT32));
				if (ret != KAL_SUCCESS) {
					DBGLOG(BOOT, ERR, "[MD_BOOT_XBOOT_FAIL]read mbx fail: %d", ret);
				} else {
					DBGLOG(BOOT, INF, "[MD_BOOT_XBOOT_FAIL]XBOOT_MBX: 0x%08X, 0x%08X", mbx_data[0], mbx_data[1]);
				}
					
				snprintf(ex_info, EE_BUF_LEN,"\n[Others] MD_BOOT_UP_FAIL(XBOOT >%ds)(TSID=0x%X)\n", \
					timeout, mbx_data[1]);
				eemcs_aed(0, ex_info);
			} else if (timeout == 0) {
				DBGLOG(BOOT, INF, "[MD_BOOT_XBOOT_FAIL](other)");
				snprintf(ex_info, EE_BUF_LEN,"\n[Others] MD_BOOT_UP_FAIL(Other)\n");
				eemcs_aed(0, ex_info);
			}
			break;
			
		case MD_BOOT_HS1_FAIL:
			ret = ccci_boot_mbx_read(mbx_data, 2*sizeof(KAL_UINT32));
			if (ret != KAL_SUCCESS) {
				DBGLOG(BOOT, ERR, "[MD_BOOT_HS1_FAIL]read mbx fail: %d", ret);
			} else {
				DBGLOG(BOOT, INF, "[MD_BOOT_HS1_FAIL]XBOOT_MBX: 0x%08X, 0x%08X", mbx_data[0], mbx_data[1]);
			}
			snprintf(ex_info, EE_BUF_LEN,"\n[Others] MD_BOOT_UP_FAIL(HS1 >%ds)(TSID=0x%X)\n", \
				timeout, mbx_data[1]);
			eemcs_aed(0, ex_info);
			break;
			
		case MD_BOOT_HS2_FAIL:
			DBGLOG(BOOT, INF, "[MD_BOOT_HS2_FAIL]HS2 timeout>%ds", timeout);
			snprintf(ex_info, EE_BUF_LEN,"\n[Others] MD_BOOT_UP_FAIL(HS2 >%ds)\n", timeout);
			eemcs_aed(CCCI_AED_DUMP_EX_MEM, ex_info);
			break;

		default:
			break;
	}
	return;
}

static void bootup_to_monitor(unsigned long val)
{
	time_out_abort = 1;
	mb();
	wake_up(&eemcs_boot_inst.mailbox.d2h_wait_q);
	wake_up(&boot_node->rx_waitq);
	DBGLOG(BOOT, INF, "Boot time out trigger(%d)", boot_time_out_val);
}


static int set_bootup_timeout_value(unsigned int val)
{
	if(val)
		mod_timer(&bootup_to_monitor_timer,jiffies+val*HZ);
	else
		del_timer(&bootup_to_monitor_timer);
	boot_time_out_val = val;

	return 0;
}


/* for executable "cli_eemcs" */
#if TEST_DRV
#if EMCS_SDIO_DRVTST 
extern int call_function(char *buf);
#endif
//static char boot_wr_buf[200];
//static char boot_rd_buf[200] = "IOCTL_READ TEST STRING";

#define CLI_MAGIC 'CLI' /* this cause compile warning, please ask Wei-De to solve it */
#define IOCTL_READ _IOR(CLI_MAGIC, 0, int)
#define IOCTL_WRITE _IOW(CLI_MAGIC, 1, int)
#endif

void eemcs_md_logger_notify(void);
KAL_INT32 eemcs_power_off_md(KAL_INT32 md_id, KAL_UINT32 timeout)
{   
    DEBUG_LOG_FUNCTION_ENTRY;
    eemcs_md_logger_notify();
//    eemcs_ccci_remove();
    lte_sdio_off();
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_INT32 eemcs_power_on_md(KAL_INT32 md_id, KAL_UINT32 timeout)
{   
    int ret = CCCI_ERR_MODULE_INIT_OK;
    DEBUG_LOG_FUNCTION_ENTRY;
    //lte_sdio_on();
//    ret = eemcs_ccci_init();
//    if(ret != CCCI_ERR_MODULE_INIT_OK){
//        DBGLOG(INIT,ERR, "eemcs_ccci_power_on_md error !!!");
//  }
    DEBUG_LOG_FUNCTION_LEAVE;
    return ret;
}

KAL_INT32 eemcs_md_reset(void)
{
    DEBUG_LOG_FUNCTION_ENTRY;
	
	/* prevent another reset modem action from wdt timeout IRQ during modem reset */
	if(atomic_inc_and_test(&eemcs_boot_inst.md_reset_cnt) > 1) {
		DBGLOG(BOOT, ERR, "One reset flow is on-going");
		return KAL_SUCCESS;
	}

    change_device_state(EEMCS_GATE);
    wake_lock_timeout(&eemcs_boot_inst.eemcs_boot_wake_lock, 10*HZ); //wake lock 10s
    eemcs_cdev_msg(CCCI_PORT_CTRL, CCCI_MD_MSG_RESET, 0);
    
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

void eemcs_boot_user_exit_notify(void){
    wake_up(&eemcs_boot_inst.rst_waitq);
}

KAL_INT32 eemcs_wait_usr_n_rst(int port_id){
    long ret;

    while(false==eemcs_cdev_rst_port_closed()){
        ret = wait_event_interruptible(eemcs_boot_inst.rst_waitq, (true==eemcs_cdev_rst_port_closed()));
        if (-ERESTARTSYS == ret) {
                DBGLOG(BOOT, INF, "wait_user_ready_to_reset signaled by -ERESTARTSYS(%ld)", ret);
        }
    }

    DBGLOG(BOOT, INF, "send CCCI_MD_MSG_READY_TO_RESET to daemon by PORT%d", port_id);
            
    eemcs_cdev_msg(CCCI_PORT_CTRL, CCCI_MD_MSG_READY_TO_RESET, 0);

    return KAL_SUCCESS;
}

extern void apply_pre_md_run_setting(void);
extern void apply_post_md_run_setting(void);
long eemcs_ctrl_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    EEMCS_STATE state;
    KAL_UINT8 port_id = 0;

    eemcs_cdev_node_t *curr_node = (eemcs_cdev_node_t *)fp->private_data;
    port_id = curr_node->eemcs_port_id;

    DEBUG_LOG_FUNCTION_ENTRY;

    switch(cmd)
    {
        case CCCI_IOC_BOOT_MD:
        {
            DBGLOG(BOOT, INF, "CCCI_IOC_BOOT_MD by %s", current->comm);
            ret = eemcs_boot_modem();
            apply_pre_md_run_setting();
        }
        break;
        // NOTE : We rename CCCI_IOC_CHECK_MD_STATE to CCCI_IOC_CHECK_STATE in EEMCS
        case CCCI_IOC_CHECK_STATE:
        {
            state = check_device_state();
            DBGLOG(BOOT, INF, "CCCI_IOC_CHECK_STATE: %d", state);
            ret = put_user((unsigned int)state, (unsigned int __user *)arg);
        }
        break;
        // NOTE : We rename CCCI_IOC_SET_MD_STATE to CCCI_IOC_SET_STATE in EEMCS
        case CCCI_IOC_SET_STATE:
        {
            if (get_user(state, (int __user *)arg)) {
                return -EFAULT;
            }		
            DBGLOG(BOOT, INF, "CCCI_IOC_SET_STATE: %d", state);
            ret = change_device_state(state);
        }
        break;

        case CCCI_IOC_GET_EXCEPTION_LENGTH:
        {
            DBGLOG(BOOT, INF, "CCCI_IOC_GET_EXCEPTION_LENGTH");
            ret = put_user(MD_EX_LOG_SIZE, (unsigned int __user *)arg);
        }
        break;

        case CCCI_IOC_GET_MD_BOOT_INFO:
        {
            ret = put_user((unsigned int)eemcs_boot_inst.boot_state, (unsigned int __user *)arg);	
            DBGLOG(BOOT, INF, "CCCI_IOC_GET_MD_BOOT_INFO: %d", eemcs_boot_inst.boot_state);
        }
        break;

        case CCCI_IOC_START_BOOT:
        {
            DBGLOG(BOOT, INF, "CCCI_IOC_START_BOOT");
            change_device_state(EEMCS_MOLY_HS_P2);
            // TODO : This function is empty in EMCS
            //emcs_md_ctrl_start_boot();
        }
        break;

        case CCCI_IOC_BOOT_DONE:
        {
            DBGLOG(BOOT, INF, "CCCI_IOC_BOOT_DONE");
            change_device_state(EEMCS_BOOTING_DONE);
            DBGLOG(BOOT, INF, "CCCI_IOC_BOOT_DONE: Modem is ready ...");

            mtlte_sys_sdio_driver_init_after_phase2();
            // TODO : In EMCS, this function invokes a call chain.
            //        Functions in call chain are registered by other users who
            //        care about MD is booting to ready.
            //        What do we do ??? Do we need this ???
            //emcs_md_ctrl_boot_done();
        }
        break;

        case CCCI_IOC_REBOOT:
        {
            DBGLOG(BOOT, INF, "CCCI_IOC_REBOOT");
            //change_device_state(EEMCS_INIT);
            // TODO : This function is empty in EMCS
            //emcs_md_ctrl_reboot();
        }
        break;

        case CCCI_IOC_MD_EXCEPTION:
        {
            DBGLOG(BOOT, INF, "CCCI_IOC_MD_EXCEPTION");
            change_device_state(EEMCS_EXCEPTION);
            // TODO : Needed ???
            //emcs_md_ctrl_md_expection();
        }
        break;

        case CCCI_IOC_MD_EX_REC_OK:
        {
            DBGLOG(BOOT, INF, "CCCI_IOC_MD_EX_REC_OK");
            // TODO : Needed ???
            //emcs_md_ctrl_md_expection_rec();
        }
        break;

        case CCCI_IOC_GET_MD_INFO:
        {
            int mode = is_modem_debug_ver();
            ret = put_user((unsigned int)mode, (unsigned int __user *)arg);
            break;
        }

        case CCCI_IOC_GET_RUNTIME_DATA:
        {
            void __user *argp = NULL;
            void *runtime_data = NULL;
            KAL_UINT32 data_size = 0;

            DBGLOG(BOOT, INF, "CCCI_IOC_GET_RUNTIME_DATA");
            argp = (void __user *)arg;
            data_size = eemcs_md_gen_runtime_data(&runtime_data);
            DBGLOG(BOOT, INF, "Runtime data size: %d", data_size);
            if (unlikely(runtime_data == 0)) {
                return -ENOMEM;
            }

            if (copy_to_user(argp, runtime_data, data_size)) {
                DBGLOG(BOOT, ERR, "copy_to_user RUNTIME_DATA failed !!");
                eemcs_md_destroy_runtime_data(runtime_data);
                return -EFAULT;
            }
            eemcs_md_destroy_runtime_data(runtime_data);
        }
        break;

        case CCCI_IOC_SET_EXCEPTION_DATA:
        {
            DBGLOG(BOOT, INF,"CCCI_IOC_SET_EXCEPTION_DATA");
#if 0
            extern EX_LOG_T md_ex_log;
            void __user *argp = (void __user *)arg;
            if(copy_from_user(&md_ex_log,argp,MD_EX_LOG_SIZE))
            {
                DBGLOG(PORT,ERR,"copy_from_user failed.");
                return -EFAULT;
            }
            md_exception(&md_ex_log);
#endif      
        }
        break;

        case CCCI_IOC_MD_RESET:
        {
            DBGLOG(BOOT, INF, "CCCI_IOC_MD_RESET by PORT%d", port_id);
            eemcs_md_reset();
        }
        break;

        case CCCI_IOC_DO_MD_RST:
        {
            DBGLOG(BOOT, INF, "CCCI_IOC_DO_MD_RST");
            // TODO : This is a dummy function in EMCS, only do following code
            //     md_boot_stage = MD_BOOT_STAGE_0;
            // What do we do ???
            //ret = md_reset();
        }
        break;

        case CCCI_IOC_DO_STOP_MD:
        {
            DBGLOG(BOOT, INF, "CCCI_IOC_DO_STOP_MD by PORT%d", port_id);
            ret = eemcs_power_off_md(0, 0);
            return ret;
            #if 0
            KAL_INT32 result;
            result = eemcs_wait_usr_n_rst(port_id);
            if(KAL_SUCCESS != result){
                 return result;
            }
            #endif
        }
        break;
        
        case CCCI_IOC_WAIT_RDY_RST:
        {
            KAL_INT32 result;			
            DBGLOG(BOOT, INF, "CCCI_IOC_WAIT_RDY_RST by PORT%d", port_id);
            result = eemcs_wait_usr_n_rst(port_id);
            if(KAL_SUCCESS != result){
                 return result;
            }
        }
        break;
        
        case CCCI_IOC_SET_HEADER:
        {
            KAL_UINT32 ori_port_flag = 0;
            KAL_UINT32 new_port_flag = 0;

            ori_port_flag = ccci_get_port_cflag(port_id);
            ccci_set_port_type(port_id, (ori_port_flag|EXPORT_CCCI_H));
            new_port_flag = ccci_get_port_cflag(port_id);
            DBGLOG(BOOT, INF, "CCCI_IOC_SET_HEADER(%d, %d) by PORT%d", ori_port_flag, new_port_flag, port_id);
        }
        break;

        case CCCI_IOC_CLR_HEADER:
        {
            KAL_UINT32 ori_port_flag = 0;
            KAL_UINT32 new_port_flag = 0;

            ori_port_flag = ccci_get_port_cflag(port_id);
            ccci_set_port_type(port_id, (ori_port_flag&(~EXPORT_CCCI_H)));
            new_port_flag = ccci_get_port_cflag(port_id);
            DBGLOG(BOOT, INF, "CCCI_IOC_CLR_HEADER(%d, %d) by PORT%d", ori_port_flag, new_port_flag, port_id);
        }
        break;

        case CCCI_IOC_DO_START_MD:
        {
            DBGLOG(BOOT, INF, "CCCI_IOC_DO_START_MD by PORT%d", port_id);
            ret = eemcs_power_on_md(0, 0);
            if(ret != KAL_SUCCESS){
                DBGLOG(BOOT, ERR, "CCCI_IOC_DO_START_MD fail");
            }
            change_device_state(EEMCS_INIT);
        }
        break;

        case CCCI_IOC_RELOAD_MD_TYPE:
        {
            int md_type = 0;
            if(copy_from_user(&md_type, (void __user *)arg, sizeof(unsigned int))) {
                DBGLOG(BOOT, ERR, "IOC_RELOAD_MD_TYPE: copy_from_user fail!");
                ret = -EFAULT;
                break;
            } 
            
            if (md_type > modem_invalid && md_type < modem_max_type){
                //DBGLOG(BOOT, DBG, "IOC_RELOAD_MD_TYPE: storing md type(%d)", md_type);
                ret = set_ext_modem_support(eemcs_get_md_id(), md_type);
            }
            else{
                DBGLOG(BOOT, ERR, "IOC_RELOAD_MD_TYPE: invalid md type(%d)", md_type);
                ret = -EFAULT;
            }
            eemcs_set_reload_image(true);
        }
        break;

        case CCCI_IOC_FLOW_CTRL_SETTING:
        {
            unsigned int limit[2] = {0, 0};
            if(copy_from_user(&limit, (void __user *)arg, 2*sizeof(unsigned int))) {
                DBGLOG(BOOT, ERR, "IOC_FLOW_CTRL_SETTING: copy_from_user fail!");
                ret = -EFAULT;
                break;
            } else {
                DBGLOG(BOOT, INF, "IOC_FLOW_CTRL_SETTING: limit=%d, thresh=%d", limit[0], limit[1]);
                mtlte_df_Init_DL_flow_ctrl(RXQ_Q2, false, limit[0], limit[1]);
            }
        }
        break;

	case CCCI_IOC_GET_MD_TYPE:
        {
            int md_type = get_ext_modem_support(eemcs_get_md_id());
            DBGLOG(BOOT, INF, "CCCI_IOC_GET_MD_TYPE(%d)", md_type);
            ret = put_user((unsigned int)md_type, (unsigned int __user *)arg);
        }
        break;

        case CCCI_IOC_BOOT_UP_TIMEOUT:
        {
            unsigned int para[2] = {0, 0};
            if(copy_from_user(&para, (void __user *)arg, 2*sizeof(unsigned int))) {
                DBGLOG(BOOT, ERR, "IOC_BOOT_UP_TIMEOUT: copy_from_user fail!");
                ret = -EFAULT;
                break;
            } else {
                DBGLOG(BOOT, INF, "IOC_BOOT_UP_TIMEOUT: boot_sta=%d, timeout=%d", para[0], para[1]);
                eemcs_md_boot_up_timeout_func(para[0], para[1]);
            }
        }
        break;

	case CCCI_IOC_GET_CFG_SETTING:
        {
            char buffer[64];
            void __user *argp = NULL;
            argp = (void __user *)arg;

            if(copy_from_user(buffer, (void __user *)arg, sizeof(buffer))) {
                DBGLOG(BOOT, ERR, "CCCI_IOC_FEATRUE_SET_QUERY: copy_from_user fail!");
                ret = -EFAULT;
            } else {

                DBGLOG(BOOT, INF, "CCCI_IOC_FEATRUE_SET_QUERY: %s\n", buffer);
                // Query setting
                if(query_feature_setting(buffer) == 0) {
                    if (copy_to_user(argp, buffer, sizeof(buffer))) {
			DBGLOG(BOOT, ERR, "Query copy_to_user failed !!");
			ret = -EFAULT;
                    }
                }
            }
        }
        break;

	case CCCI_IOC_SET_BOOT_TO_VAL:
	{
		unsigned int to_val;
		if(copy_from_user(&to_val, (void __user *)arg, sizeof(unsigned int))) {
			DBGLOG(BOOT, ERR, "CCCI_IOC_SET_BOOT_TO_VAL: copy_from_user fail!");
			ret = -EFAULT;
		} else {
			DBGLOG(BOOT, INF, "CCCI_IOC_SET_BOOT_TO_VAL: %d", to_val);
			set_bootup_timeout_value(to_val);
		}
	}
	break;

        default:
            DBGLOG(BOOT, ERR, "invalid ioctl cmd(%d)", cmd);
            ret = -EFAULT;
        break;
    }

    DEBUG_LOG_FUNCTION_LEAVE;
    return ret;
}

//
// For reset test
//
KAL_INT32 eemcs_boot_reset_test(KAL_INT32 reset_state){
    boot_reset_state = reset_state;
    return boot_reset_state;
}


//
// UT
//
#ifdef _EEMCS_BOOT_UT

KAL_UINT32 eemcs_boot_ut_mbx[2] = {0};
XBOOT_CMD eemcs_boot_ut_cmd = {0};


KAL_UINT32 bootut_register_callback(CCCI_CHANNEL_T chn, EEMCS_CCCI_CALLBACK func_ptr , KAL_UINT32 private_data) {	
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(BOOT, DBG, "CCCI channel (%d) register callback", chn);
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_UINT32 bootut_unregister_callback(CCCI_CHANNEL_T chn) {
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(BOOT, DBG, "CCCI channel (%d) UNregister callback", chn);
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_INT32 bootut_init(void)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    eemcs_boot_inst.ut_bl_cnt = 0;
    eemcs_boot_inst.ut_xcmd_idx = 0;
    eemcs_boot_inst.ut_h2d_intr = 0;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_INT32 bootut_md_write_mbx(KAL_UINT32 data0, KAL_UINT32 data1)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    eemcs_boot_ut_mbx[0] = data0;
    eemcs_boot_ut_mbx[1] = data1;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_INT32 bootut_md_read_mbx(KAL_UINT32 *data0, KAL_UINT32 *data1)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    *data0 = eemcs_boot_ut_mbx[0];
    *data1 = eemcs_boot_ut_mbx[1];
    eemcs_boot_inst.ut_h2d_intr = 0;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_INT32 bootut_md_send_command(void)
{
    KAL_INT32 result = KAL_SUCCESS;
    EEMCS_BOOT_UT_CMD *xcmd = NULL;

    DEBUG_LOG_FUNCTION_ENTRY;
    xcmd = &ut_md_xcmd[eemcs_boot_inst.ut_xcmd_idx];
    switch (xcmd->type)
    {
        case BOOT_UT_MBX:
        {
            //ccci_boot_mbx_write(xcmd->data.mbx, sizeof(KAL_UINT32) * 2);
            bootut_md_write_mbx(xcmd->data.mbx.data0, xcmd->data.mbx.data1);
            eemcs_boot_swint_callback(1 << 16);
            break;
        }
        case BOOT_UT_XCMD:
        {
            struct sk_buff *new_skb = NULL;
            new_skb = prepare_xcmd(xcmd->data.xcmd.magic, xcmd->data.xcmd.msg_id, xcmd->data.xcmd.status);
            eemcs_boot_rx_callback(new_skb, 0);
            break;
        }
        case BOOT_UT_MSD_OUTPUT:
        case BOOT_UT_MSD_FLUSH:
        {
            struct sk_buff *new_skb = NULL;
            XBOOT_CMD_PRINT *header = NULL;

            new_skb = ccci_boot_mem_alloc(sizeof(XBOOT_CMD_PRINT));
            if (new_skb == NULL) {
                DBGLOG(BOOT, ERR, "alloc skb fail");
                result = KAL_FAIL;
                break;
            }
            header = (XBOOT_CMD_PRINT *)skb_put(new_skb, sizeof(XBOOT_CMD_PRINT));
            memset(new_skb->data, 0, sizeof(XBOOT_CMD_PRINT));
            header->magic = xcmd->data.msd_print.magic;
            header->msg_id = xcmd->data.msd_print.msg_id;
            header->str_len = xcmd->data.msd_print.str_len;
            memcpy(header->str, xcmd->data.msd_print.str, xcmd->data.msd_print.str_len);
            eemcs_boot_rx_callback(new_skb, 0);
            break;
        }
        case BOOT_UT_XCMD_GETBIN:
        {
            struct sk_buff *new_skb = NULL;
            XBOOT_CMD_GETBIN *header = NULL;
        
            new_skb = ccci_boot_mem_alloc(sizeof(XBOOT_CMD_GETBIN));
            if (new_skb == NULL) {
                DBGLOG(BOOT, ERR, "Failed to alloc skb !!");
                result = KAL_FAIL;
                break;
            }
            header = (XBOOT_CMD_GETBIN *)skb_put(new_skb, sizeof(XBOOT_CMD_GETBIN));
            memset(new_skb->data, 0, sizeof(XBOOT_CMD_GETBIN));
            header->magic = xcmd->data.xcmd_getbin.magic;
            header->msg_id = xcmd->data.xcmd_getbin.msg_id;
            header->offset = xcmd->data.xcmd_getbin.offset;
            header->len = xcmd->data.xcmd_getbin.len;
            eemcs_boot_rx_callback(new_skb, 0);
            break;
        }
        default:
            break;
    }
    DEBUG_LOG_FUNCTION_LEAVE;
    return result;
}

KAL_INT32 bootut_register_swint_callback(EEMCS_CCCI_SWINT_CALLBACK func_ptr)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_INT32 bootut_unregister_swint_callback(KAL_UINT32 id)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_INT32 bootut_download_handler(struct sk_buff *skb)
{
    KAL_INT32 result = KAL_SUCCESS;
    KAL_UINT32 md_mbx[2] = {0};
    EEMCS_BOOT_UT_CMD *xcmd = NULL;
    XBOOT_CMD *ack = NULL;
    KAL_UINT32 msg_id = -1;
    DEBUG_LOG_FUNCTION_ENTRY;

    xcmd = &ut_md_xcmd[eemcs_boot_inst.ut_xcmd_idx];
    // H2D mailbox
    if (skb == NULL) {
        bootut_md_read_mbx(&md_mbx[0], &md_mbx[1]);
        if ((xcmd->data.mbx.data0 == SDIOMAIL_BOOTING_REQ && md_mbx[0] == SDIOMAIL_BOOTING_ACK) ||
            (xcmd->data.mbx.data0 == SDIOMAIL_DL_REQ && md_mbx[0] == SDIOMAIL_REF)) {
            eemcs_boot_inst.ut_xcmd_idx++;
        }
    // Command
    } else {
        ack = (XBOOT_CMD *)skb->data;
        switch (xcmd->type)
        {
            case BOOT_UT_XCMD:
                msg_id = xcmd->data.xcmd.msg_id;
                goto _jump;
            case BOOT_UT_MSD_OUTPUT:
            case BOOT_UT_MSD_FLUSH:
                msg_id = xcmd->data.msd_print.msg_id;
_jump:
            {
                if (msg_id + 1 != ack->msg_id) {
                    DBGLOG(BOOT, ERR, "[BOOT_UT] ACK from AP is incorrect !! MD send (%d) but got (%d)", msg_id, ack->msg_id);
                    result = KAL_FAIL;
                    goto _ack_fail;
                }
                if (ack->msg_id == CMDID_ACK_BIN_LOAD_END) {
                    eemcs_boot_inst.ut_bl_cnt++;
#if (EEMCS_BOOT_UT_BL_CNT >= 2)
                    // Repeat BL download simulation
                    if (eemcs_boot_inst.ut_bl_cnt > 1 && eemcs_boot_inst.ut_bl_cnt < EEMCS_BOOT_UT_BL_CNT) {
                        eemcs_boot_inst.ut_xcmd_idx = EEMCS_BOOT_UT_BL_LOOP_START;
                        break;
                    }
#endif
                }
                // Go to next MD command
                if (ack->msg_id != CMDID_ACK_MD_BOOT_END)
                    eemcs_boot_inst.ut_xcmd_idx++;
                else {
                    DBGLOG(BOOT, TRA, "[BOOT_UT] +++ CMDID_ACK_MD_BOOT_END +++ Download is OK !!");
                    goto _boot_end;
                }
                break;
            }
            case BOOT_UT_XCMD_GETBIN:
            {
                // GETBIN ack from AP side
                if (!eemcs_boot_inst.ut_getbin_time) {
                    DBGLOG(BOOT, DBG, "[BOOT_UT] CMDID_GET_BIN start !!");
                    if (ack->msg_id == CMDID_ACK_GET_BIN) {
                        eemcs_boot_inst.ut_getbin_time = 1;
                        eemcs_boot_inst.ut_getbin_size = 0;
                        goto _wait_ack;
                    }
                    // SHOULD NOT BE HERE !!
                    KAL_ASSERT(0);
                // GETBIN data from AP side
                } else {
                    eemcs_boot_inst.ut_getbin_size += skb->len;
                    DBGLOG(BOOT, DBG, "[BOOT_UT] CMDID_GET_BIN. Total : %d, skb : %d",
                        eemcs_boot_inst.ut_getbin_size, skb->len);
                    if (eemcs_boot_inst.ut_getbin_size < xcmd->data.xcmd_getbin.len) {
                        DBGLOG(BOOT, DBG, "[BOOT_UT] CMDID_GET_BIN continue ...");
                        goto _wait_ack;
                    }
                    DBGLOG(BOOT, DBG, "[BOOT_UT] CMDID_GET_BIN end !!");
                    eemcs_boot_inst.ut_getbin_time = 0;
                    eemcs_boot_inst.ut_xcmd_idx++;
                }
                break;
            }
        }
    }
    bootut_md_send_command();
_boot_end:
_wait_ack:
_ack_fail:
    dev_kfree_skb(skb);
    DEBUG_LOG_FUNCTION_LEAVE;
    return result;

}

struct sk_buff *bootut_gen_handshake_data(KAL_UINT32 data1, KAL_UINT32 reserved)
{
    struct sk_buff *new_skb = NULL;
    CCCI_BUFF_T *header = NULL;

    new_skb = ccci_boot_mem_alloc(sizeof(CCCI_BUFF_T));
    if (new_skb == NULL)
        return NULL;
    header = (CCCI_BUFF_T *)skb_put(new_skb, sizeof(CCCI_BUFF_T));
    memset(new_skb->data, 0, sizeof(CCCI_BUFF_T));
    header->channel = CH_CTRL_RX;
    header->data[0] = CCCI_MAGIC_NUM;
    header->data[1] = data1;
    header->reserved = reserved;
    return new_skb;
}

KAL_INT32 bootut_ccci_handshake_handler(struct sk_buff *skb)
{
    KAL_INT32 result = KAL_SUCCESS;
    CCCI_BUFF_T *ccci = NULL;
    MODEM_RUNTIME *runtime = NULL;
    struct sk_buff *new_skb = NULL;
    DEBUG_LOG_FUNCTION_ENTRY;

    KAL_ASSERT(skb != NULL);
    // Check CCCI header
    ccci = (CCCI_BUFF_T *)skb->data;
    if (ccci->reserved != MD_INIT_CHK_ID) {
        result = KAL_FAIL;
        goto _check_fail;
    }
    // Check runtime data
    runtime = (MODEM_RUNTIME *)(skb->data + sizeof(CCCI_BUFF_T));
    if ((runtime->Prefix != 0x46494343 || runtime->Postfix != 0x46494343) ||
        (runtime->DriverVersion != 0x20110118)) {
        result = KAL_FAIL;
        goto _check_fail;
    }
    // Ack to AP side
    new_skb = bootut_gen_handshake_data(0, NORMAL_BOOT_ID);
    if (new_skb == NULL) {
        DBGLOG(BOOT, ERR, "[BOOT_UT] Failed to alloc skb !!");
        result = KAL_FAIL;
        goto _alloc_fail;
    }
    eemcs_boot_rx_callback(new_skb, 0);
_alloc_fail:
_check_fail:
    dev_kfree_skb(skb);
    DEBUG_LOG_FUNCTION_LEAVE;
    return result;
}

KAL_INT32 bootut_start_ccci_handshake(void)
{
    KAL_INT32 result = KAL_SUCCESS;
    struct sk_buff *new_skb = NULL;
    DEBUG_LOG_FUNCTION_ENTRY;

    new_skb = bootut_gen_handshake_data(MD_INIT_START_BOOT, MD_INIT_CHK_ID);
    if (new_skb == NULL) {
        DBGLOG(BOOT, ERR, "[BOOT_UT] Failed to alloc skb !!");
        result = KAL_FAIL;
        goto _alloc_fail;
    }
    eemcs_boot_rx_callback(new_skb, 0);
    DEBUG_LOG_FUNCTION_LEAVE;
_alloc_fail:
    return result;
}

inline KAL_INT32 bootut_UL_write_skb_to_swq(struct sk_buff *skb)
{
    if (eemcs_boot_get_state() < MD_ROM_BOOT_READY)
        return bootut_download_handler(skb);
    else {
        if (!eemcs_device_ready())
            return bootut_ccci_handshake_handler(skb);
        else
            return bootut_lb(skb);
    }
}

KAL_INT32 bootut_lb(struct sk_buff *skb)
{
    CCCI_BUFF_T *pccci_h = (CCCI_BUFF_T *)skb->data;
    KAL_UINT8 port_id;
    KAL_UINT32 tx_ch, rx_ch;
    struct sk_buff *new_skb;

    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(BOOT, DBG, "[BOOT_UT] CCCI channel bootut_lb CCCI_H(0x%x)(0x%x)(0x%x)(0x%x)",
        pccci_h->data[0], pccci_h->data[1], pccci_h->channel, pccci_h->reserved);

    new_skb = ccci_boot_mem_alloc(skb->len);
    if (new_skb == NULL) {
        DBGLOG(BOOT, ERR, "[BOOT_UT] _EEMCS_BOOT_UT dev_alloc_skb fail sz(%d).", skb->len);
        dev_kfree_skb(skb);
        DEBUG_LOG_FUNCTION_LEAVE;
        return KAL_FAIL;
    }        
    memcpy(skb_put(new_skb, skb->len), skb->data, skb->len);
    pccci_h = (CCCI_BUFF_T *)new_skb->data;
    port_id = ccci_ch_to_port(pccci_h->channel);
    tx_ch = pccci_h->channel;
    rx_ch =  eemcs_boot_inst.boot_node.ccci_ch.rx;
    pccci_h->channel = rx_ch;
    DBGLOG(BOOT, DBG, "[BOOT_UT] ========= PORT(%d) tx_ch(%d) LB to rx_ch(%d)",
        port_id, tx_ch, rx_ch);
    eemcs_boot_rx_callback(new_skb, 0);

    dev_kfree_skb(skb);
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

inline KAL_INT32 bootut_UL_write_room_check(void)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return 32;
}

void bootut_reset_txq_count(void)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
}

int bootut_xboot_mb_rd(KAL_UINT32 *pBuffer, KAL_UINT32 size)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    KAL_ASSERT(pBuffer != NULL);

    if (size == sizeof(KAL_UINT32)) {
        *pBuffer = eemcs_boot_ut_mbx[0];
    } else if (size == (sizeof(KAL_UINT32) * 2)) {
        *pBuffer = eemcs_boot_ut_mbx[0];
        *(pBuffer + 1) = eemcs_boot_ut_mbx[1];
    } else {
        DBGLOG(BOOT, ERR, "[BOOT_UT] Invalid mailbox read !! size = %d", size);
    }
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

int bootut_xboot_mb_wr(KAL_UINT32 *pBuffer, KAL_UINT32 size)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    KAL_ASSERT(pBuffer != NULL);

    if (size == sizeof(KAL_UINT32)) {
        eemcs_boot_ut_mbx[0] = *pBuffer;
    } else if (size == (sizeof(KAL_UINT32) * 2)) {
        eemcs_boot_ut_mbx[0] = *pBuffer;
        eemcs_boot_ut_mbx[1] = *(pBuffer + 1);
    } else {
        DBGLOG(BOOT, ERR, "[BOOT_UT] Invalid mailbox write !! size = %d", size);
    }

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}
KAL_UINT32 bootut_register_expt_callback(EEMCS_CCCI_EXCEPTION_IND_CALLBACK func_ptr)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_UINT32 bootut_unregister_expt_callback(KAL_UINT32 cb_id)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}
#endif // _EEMCS_BOOT_UT
