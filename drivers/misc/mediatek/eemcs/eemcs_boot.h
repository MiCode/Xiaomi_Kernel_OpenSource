#ifndef __EEMCS_BOOT_H__
#define __EEMCS_BOOT_H__

#include <asm/ioctl.h>
#include <asm/atomic.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/wakelock.h>

#include "eemcs_ccci.h"
#include "eemcs_kal.h"
#include "eemcs_char.h"
#include "eemcs_expt.h"

//===================================================================
//  Preprocessors
//===================================================================

#define BOOT_DEBUG_BUF_LEN          (512)                   // 512 bytes
#define BOOT_TX_MAX_PKT_LEN         (2095- 255)             // Max Tx packet size of xBoot


#define CONFIG_MODEM_FIRMWARE_FOLDER      "/etc/firmware/"
#define CONFIG_MODEM_FIRMWARE_CIP_FOLDER  "/custom/etc/firmware/"

#define EXT_MD_POST_FIX_LEN (20)
#define MD_INFO_STR_LEN (256)

#define CURR_SEC_CCCI_SYNC_VER			(1)	// Note: must sync with sec lib, if eemcs and sec has dependency change

//===================================================================
//  XBOOT Commands
//===================================================================

#define MAGIC_MD_CMD                0x444D434D      //"MCMD"
#define MAGIC_MD_CMD_ACK            0x4D4B4341      //"ACKM"

#define SDIOMAIL_BOOTING_REQ        0x53444254      //"SDBT"  //From Modem Request SDIO Boot
#define SDIOMAIL_BOOTING_ACK        0x534254FF      //"SBT5"  //ACK to modem
#define SDIOMAIL_DL_REQ             0x5344444C      //"SDDL"  //From Modem Request SDIO DL
#define SDIOMAIL_DL_ACK             0x53444CFF      //"SDL5"  //ACK to modem
#define SDIOMAIL_REF                0x52454655      //"REFU"  //Refuse to modem

typedef enum XBOOT_CMD_ID_e {
    CMDID_BIN_LOAD_START = 0,
    CMDID_ACK_BIN_LOAD_START,
    CMDID_GET_BIN,
    CMDID_ACK_GET_BIN,
    CMDID_BIN_LOAD_END,
    CMDID_ACK_BIN_LOAD_END,
    CMDID_MD_BOOT_END,
    CMDID_ACK_MD_BOOT_END,
    CMDID_MD_MSD_OUTPUT,
    CMDID_ACK_MD_MSD_OUTPUT,
    CMDID_MSG_FLUSH,
    CMDID_ACK_MSG_FLUSH,
    CMDID_MD_BUF_SIZE_CHANGE,
    CMDID_ACK_MD_BUF_SIZE_CHANGE,
    CMDID_MAX,
} XBOOT_CMD_ID;

// This enumeration is used in ACK command.
// Only XBOOT_OK is used currently
typedef enum XBOOT_STATUS_e {
    XBOOT_OK,
    XBOOT_STAGE_BROM,
    XBOOT_STAGE_BL,
    XBOOT_STAGE_BL_EXT,
    XBOOT_STAGE_BROM_MAUI,    
    XBOOT_ERROR = 0x1000,
    XB_STATUS_END = 0x0fffffff,
} XBOOT_STATUS;

//===================================================================
//  XBOOT Local States
//===================================================================

typedef enum EEMCS_BOOT_STATE_e {
    MD_INVALID,
    START_OF_MD_STATE,
    MD_INIT = START_OF_MD_STATE,
    MD_BROM_SDIO_INIT,
    MD_BROM_SDIO_MBX_HS,
    MD_BROM_DL_START,
    MD_BROM_DL_GET,
    MD_BROM_DL_END,
    MD_BROM_SEND_STATUS,
    MD_BROM_SDDL_HS,
    MD_BL_SDIO_INIT,
    MD_BL_SDIO_MBX_HS,
    MD_BL_DL_START,
    MD_BL_DL_GET,
    MD_BL_DL_END,
    MD_BOOT_END,
    MD_ROM_SDIO_INIT,
    MD_ROM_SDIO_MBX_HS,
    MD_ROM_BOOTING,
    MD_ROM_BOOT_READY,
    MD_ROM_EXCEPTION,
    END_OF_MD_STATE,
} EEMCS_BOOT_STATE;

enum {
	MD_BOOT_XBOOT_FAIL = 0,
	MD_BOOT_HS1_FAIL = 1,
	MD_BOOT_HS2_FAIL = 2
};

//===================================================================
//  XBOOT Structures
//===================================================================

typedef struct XBOOT_CMD_st {
    KAL_UINT32    magic;
    KAL_UINT32    msg_id;
    KAL_UINT32    status;
    KAL_UINT32    reserved[5];
} XBOOT_CMD_STATUS, XBOOT_CMD;

typedef struct XBOOT_CMD_GETBIN_st {
    KAL_UINT32      magic;
    KAL_UINT32      msg_id;
    KAL_UINT32      offset;
    KAL_UINT32      len;
    KAL_UINT32      reserved[4];
} XBOOT_CMD_GETBIN;

#ifdef _EEMCS_BOOT_UT

typedef struct XBOOT_CMD_PRINT_st {
    KAL_UINT32      magic;
    XBOOT_CMD_ID    msg_id;  
    KAL_UINT32      str_len;
    KAL_UINT8       str[128];
} XBOOT_CMD_PRINT;

#else // _EEMCS_BOOT_UT

typedef struct XBOOT_CMD_PRINT_st {
    KAL_UINT32      magic;
    XBOOT_CMD_ID    msg_id;  
    KAL_UINT32      str_len;
    KAL_UINT32      str[0];
} XBOOT_CMD_PRINT;

#endif // _EEMCS_BOOT_UT

//For MD_BUF_SIZE_CHANGE
typedef struct Xboot_CMD_BufSize_st {
    KAL_UINT32      magic;
    KAL_UINT32      msg_id;
    KAL_UINT32      buf_size; //The buf size
    KAL_UINT32      reserve[5];
} XBOOT_CMD_BUFSIZE;

//===================================================================
//  Boot Structures
//===================================================================

typedef struct MAILBOX_st {
    unsigned int data0;
    unsigned int data1;
} MAILBOX;

typedef struct EEMCS_MAILBOX_st {
    MAILBOX d2h_mbx;
    MAILBOX h2d_mbx;
    wait_queue_head_t d2h_wait_q;
    wait_queue_head_t h2d_wait_q;               // reserved
    volatile unsigned int d2h_wakeup_cond;
    volatile unsigned int h2d_wakeup_cond;     // reserved
} EEMCS_MAILBOX;
/*===========================================================*/
/* -------------ccci load md&dsp image define----------------*/
typedef struct {
	char *product_ver;	/* debug/release/invalid */
	char *image_type;	/*2G/3G/invalid*/
    char *platform;	    /* MT6573_S00(MT6573E1) or MT6573_S01(MT6573E2) */
	char *build_time;	/* build time string */
	char *build_ver;	/* project version, ex:11A_MD.W11.28 */
} AP_CHECK_INFO;


typedef enum{
	INVALID_VARSION = 0,
	DEBUG_VERSION,
	RELEASE_VERSION
}PRODUCT_VER_TYPE;



#define VER_2G_STR  	"2G"
#define VER_3G_STR  	"3G"
#define VER_WG_STR  	"WG"
#define VER_TG_STR  	"TG"
#define VER_LTG_STR  	"LTG"
#define VER_LWG_STR  	"LWG"
#define VER_SGLTE_STR  	"SGLTE"
#define VER_INVALID_STR "VER_INVALID"

#define DEBUG_STR   "Debug"
#define RELEASE_STR  "Release"
#define INVALID_STR "VER_INVALID"

#define MD_HEADER_MAGIC_NO "CHECK_HEADER"
#define MD_HEADER_VER_NO    (2)

typedef struct{
	KAL_UINT8 check_header[12];	    /* magic number is "CHECK_HEADER"*/
	KAL_UINT32 header_verno;	        /* header structure version number */
	KAL_UINT32 product_ver;	        /* 0x0:invalid; 0x1:debug version; 0x2:release version */
	KAL_UINT32 image_type;	            /* 0x0:invalid; 0x1:2G modem; 0x2: 3G modem */
	KAL_UINT8 platform[16];	        /* MT6573_S01 or MT6573_S02 */
	KAL_UINT8 build_time[64];	        /* build time string */
	KAL_UINT8 build_ver[64];	        /* project version, ex:11A_MD.W11.28 */
	KAL_UINT8 bind_sys_id;	            /* bind to md sys id, MD SYS1: 1, MD SYS2: 2 */
	KAL_UINT8 reserved[3];             /* for reserved */
	KAL_UINT32 reserved_info[3];       /* for reserved */
	KAL_UINT32 size;	                /* the size of this structure */
}MD_CHECK_HEADER;

typedef struct{
    KAL_UINT8  flavor[32];	           /* flavor, ex: MT6290M_LTE */
    KAL_UINT32 reserved[24];            /* for reserved */
    KAL_UINT8  mode[36];	           /* build mode, ex: MULTI_MODE_EXT_SP */
    KAL_UINT8  moly_week[16];	       /* project version, ex. MOLY.W13.28 */
    KAL_UINT32 reserved_2[12];       /* for reserved */
    MD_CHECK_HEADER md_chk_header;
}MD_TAIL;

#define  NAME_LEN 100
#define  AP_PLATFORM_LEN 16
#define  MD_INDEX  0
#define  DSP_INDEX 1
typedef	struct image_info{
    int             type;            
    char			file_name[NAME_LEN];
    unsigned int    mem_size;   /* MD_Image memory size*/
    unsigned int    address;	   /* MD_Image start address */
    unsigned int    remap_address; /* MD_Image start address */
    ssize_t			size;          /* MD_Image size*/
    loff_t			offset;
    unsigned int	tail_length;
    MD_CHECK_HEADER img_chk_header;
} IMG_INFO;
typedef struct md_info
{
    IMG_INFO        img_info[2];    /*type=0,modem image; type=1, dsp image */
    char            md_img_info_str[MD_INFO_STR_LEN];
    char			ap_platform[AP_PLATFORM_LEN];
	AP_CHECK_INFO   ap_info;
	unsigned int	flags;
}MD_INFO;


#define IMG_PATH_LEN 48
#define POST_FIX_LEN 16
typedef struct md_img_mapping{
    char post_fix[POST_FIX_LEN];
    char full_path[IMG_PATH_LEN];
}img_mapping_t;
/*===========================================================*/
typedef struct EEMCS_BOOT_SET_st {
    volatile EEMCS_BOOT_STATE boot_state;
    EEMCS_MAILBOX mailbox;
    KAL_UINT8 *cmd_buff;                /* Used to send XBOOT command and data */
    KAL_UINT8 *debug_buff;              /* Used to store message from MD */
    KAL_UINT32 debug_offset;            /* Indicator of debug_buff */
    KAL_UINT32 cb_id;
    KAL_UINT32 ccci_hs_bypass;          /* CCCI handshake is bypassed */

    struct class *dev_class;            /* class_create/class_destroy/device_create/device_destroy */
    struct cdev *eemcs_boot_chrdev;     /* cdev_alloc/cdev_del/cdev_init/cdev_add */
    eemcs_cdev_node_t boot_node;
    KAL_INT32 expt_cb_id;               /* Exception callback function ID */
    wait_queue_head_t  rst_waitq;       /* wait for users (fs/mdlog/mux) close port to do reset */
    MD_INFO  md_info;
    KAL_UINT32 md_need_reload;          /* 0: not need reload, 1: need reload */
    char ext_modem_post_fix[EXT_MD_POST_FIX_LEN]; /*Postfix of modem image, ex: "x_xxx_n"*/
    struct wake_lock	eemcs_boot_wake_lock;
    KAL_UINT8  eemcs_boot_wakelock_name[32];
    KAL_UINT32 md_id;                               /* MD ID ex. MT6290 = MD_SYS5 = 4 */
    KAL_UINT32 md_img_exist_list[MD_IMG_MAX_CNT+1]; /* for world phone feature, +1 for 0 */
    KAL_UINT32 md_img_list_scaned;
    img_mapping_t md_img_file_list[MD_IMG_MAX_CNT+1];
    img_mapping_t dsp_img_file_list[MD_IMG_MAX_CNT+1];
    atomic_t md_reset_cnt;              /* 0: no md reset; > 0: md reset is on-going*/
    
#ifdef _EEMCS_BOOT_UT
    KAL_UINT32 ut_xcmd_idx;
    KAL_UINT32 ut_bl_cnt;
    KAL_UINT32 ut_h2d_intr;
    KAL_UINT32 ut_getbin_time;
    KAL_UINT32 ut_getbin_size;
#endif // _EEMCS_BOOT_UT
} EEMCS_BOOT_SET;

#ifdef _EEMCS_BOOT_UT

typedef enum EEMCS_BOOT_UT_CMD_TYPE_e {
    BOOT_UT_MBX,
    BOOT_UT_XCMD,
    BOOT_UT_XCMD_GETBIN,
    BOOT_UT_MSD_OUTPUT,
    BOOT_UT_MSD_FLUSH,
} EEMCS_BOOT_UT_CMD_TYPE;

typedef struct EEMCS_BOOT_UT_CMD_st {
    KAL_UINT32 type;
    union {
        MAILBOX mbx;
        XBOOT_CMD xcmd;
        XBOOT_CMD_GETBIN xcmd_getbin;
        XBOOT_CMD_PRINT msd_print;
        XBOOT_CMD_BUFSIZE xcmd_bufsize;
    } data;
    KAL_UINT8 message[128];
} EEMCS_BOOT_UT_CMD;

#endif // _EEMCS_BOOT_UT

//===================================================================
//  Wrapper Functions
//===================================================================

#ifdef _EEMCS_BOOT_UT

KAL_INT32 bootut_init(void);
KAL_INT32 bootut_md_send_command(void);
KAL_UINT32 bootut_register_callback(CCCI_CHANNEL_T chn, EEMCS_CCCI_CALLBACK func_ptr , KAL_UINT32 private_data);
KAL_UINT32 bootut_unregister_callback(CCCI_CHANNEL_T chn);
KAL_INT32 bootut_register_swint_callback(EEMCS_CCCI_SWINT_CALLBACK);
KAL_INT32 bootut_unregister_swint_callback(KAL_UINT32);
inline KAL_INT32 bootut_UL_write_skb_to_swq(struct sk_buff *);
inline KAL_INT32 bootut_UL_write_room_check(void);
int bootut_xboot_mb_rd(KAL_UINT32 *pBuffer, KAL_UINT32 size);
int bootut_xboot_mb_wr(KAL_UINT32 *pBuffer, KAL_UINT32 size);
KAL_UINT32 bootut_register_expt_callback(EEMCS_CCCI_EXCEPTION_IND_CALLBACK func_ptr);
KAL_UINT32 bootut_unregister_expt_callback(KAL_UINT32 cb_id);

#define ccci_boot_register(ch_num,cb,para)           bootut_register_callback(ch_num,cb,para) 
#define ccci_boot_unregister(ch_num)			     bootut_unregister_callback(ch_num) 
#define ccci_boot_mbx_register(cb)                   bootut_register_swint_callback(cb)
#define ccci_boot_mbx_unregister(id)                 bootut_unregister_swint_callback(id)

#define ccci_boot_write_desc_to_q(desc_p)            bootut_UL_write_skb_to_swq(desc_p)
#define ccci_boot_write_space_check()                bootut_UL_write_room_check()

#define ccci_boot_mbx_read(buf,len)                  bootut_xboot_mb_rd(buf,len)
#define ccci_boot_mbx_write(buf,len)                 bootut_xboot_mb_wr(buf,len)
#define ccci_boot_expt_register(cb)                  bootut_register_expt_callback(cb)
#define ccci_boot_expt_unregister(cb)                bootut_unregister_expt_callback(cb)

#else // !_EEMCS_BOOT_UT

#define ccci_boot_register(ch_num,cb,para)           eemcs_ccci_register_callback(ch_num,cb,para)
#define ccci_boot_unregister(ch_num)			     eemcs_ccci_unregister_callback(ch_num) 
#define ccci_boot_mbx_register(cb)                   eemcs_ccci_register_swint_callback(cb)
#define ccci_boot_mbx_unregister(id)                 eemcs_ccci_unregister_swint_callback(id)

#define ccci_boot_write_desc_to_q(desc_p)            eemcs_ccci_boot_UL_write_skb_to_swq(desc_p)
#define ccci_boot_write_space_check()                eemcs_ccci_boot_UL_write_room_check()

#define ccci_boot_mbx_read(buf,len)                  sdio_xboot_mb_rd(buf,len)
#define ccci_boot_mbx_write(buf,len)                 sdio_xboot_mb_wr(buf,len)
#define ccci_boot_expt_register(cb)                  eemcs_register_expt_callback(cb)
#define ccci_boot_expt_unregister(cb)                eemcs_unregister_expt_callback(cb)

#endif // _EEMCS_BOOT_UT


#define CCCI_BOOT_HEADER_ROOM                   	 sizeof(SDIO_H)
//#define ccci_boot_mem_alloc(sz)                 dev_alloc_skb(sz)
#define ccci_boot_mem_alloc(sz, flag)                __dev_alloc_skb(sz, flag)

KAL_INT32 eemcs_boot_mod_init(void);
void eemcs_boot_exit(void);
KAL_UINT32 eemcs_boot_get_state(void);
KAL_INT32 eemcs_boot_get_ext_md_post_fix(char *post_fix);
KAL_INT32 eemcs_get_md_info_str(char* info_str);
KAL_UINT32* eemcs_get_md_img_exist_list(void);
KAL_UINT32 eemcs_get_md_img_exist_list_size(void);
KAL_UINT32 eemcs_get_md_id(void);
void eemcs_set_reload_image(KAL_INT32 need_reload);

/* export boot rx callback for send message to CTRL chanenl. ex. entering/leaving flight mode*/
KAL_INT32 eemcs_boot_rx_callback(struct sk_buff *skb, KAL_UINT32 private_data); 
KAL_INT32 eemcs_power_on_md(KAL_INT32 md_id, KAL_UINT32 timeout);
KAL_INT32 eemcs_power_off_md(KAL_INT32 md_id, KAL_UINT32 timeout);
KAL_INT32 eemcs_md_reset(void);
void eemcs_boot_user_exit_notify(void);
KAL_INT32 eemcs_wait_usr_n_rst(int port_id);
KAL_INT32 eemcs_boot_reset_test(KAL_INT32 reset_state);
#endif // __EEMCS_BOOT_H__
