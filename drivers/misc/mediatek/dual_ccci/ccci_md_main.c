/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccci_md.c
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   MT65XX MD initialization and handshake driver
 *
 *
 ****************************************************************************/
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/rtc.h>
#include <linux/aee.h>
#include <asm/atomic.h>
#include <ccci.h>
#include <ccif.h>


//------------------- md cotrol variable define---------------------//
typedef struct _md_ctl_block
{
    struct mutex        ccci_md_boot_mutex; // for ccci_mb_mutex
    struct mutex        ccci_reset_mutex;
    MD_CALL_BACK_HEAD_T    md_notifier;
    volatile int        md_boot_stage;
    atomic_t            md_reset_on_going;
    unsigned char        is_first_boot;
    unsigned char        md_ex_type;
    unsigned char        need_reload_image; // For md_ex_flag
    unsigned char        img_inf_ready; // for valid_image_info
    unsigned char        stop_retry_boot_md;
    unsigned char        image_type; // production, debug
    unsigned char        reboot_reason;
    unsigned char        ipo_h_restore;
    char                load_img_inf[IMG_INF_LEN]; // For image_buf
    smem_alloc_t        *smem_table;
    ccci_mem_layout_t    *md_layout;
    // -- TRM wake lock
    struct wake_lock    trm_wake_lock;
    char                wakelock_name[16];
    // -- Timer
    struct timer_list    md_ex_monitor;
    struct timer_list    md_boot_up_check_timer;
    // -- EE flag
    volatile unsigned int ee_info_got;
    volatile unsigned int ee_info_flag;
    spinlock_t            ctl_lock;
    unsigned int        m_md_id;
    struct ccci_reset_sta reset_sta[NR_CCCI_RESET_USER];
}md_ctl_block_t;

static md_ctl_block_t *md_ctlb[MAX_MD_NUM];

static void ex_monitor_func(unsigned long);
static void md_boot_up_timeout_func(unsigned long);

//-------------------external variable and function define--------------//
extern unsigned long long lg_ch_tx_debug_enable[];
extern unsigned long long lg_ch_rx_debug_enable[];
extern unsigned int tty_debug_enable[];
extern unsigned int fs_tx_debug_enable[]; 
extern unsigned int fs_rx_debug_enable[]; 

extern int get_md_wakeup_src(int md_id, char *buf, unsigned int len);

#ifndef ENABLE_SW_MEM_REMAP
int get_md2_ap_phy_addr_fixed(void)
{
    return 0;
}
#endif
#ifndef ENABLE_DUMP_MD_REGISTER
void ccci_dump_md_register(int md_id)
{
}
#endif
//-------------------other variable and function define----------------//
volatile atomic_t data_connect_sta[MAX_MD_NUM];



/****************************************************************************/
/* API about getting ccci share memory address&size                                                     */
/*                                                                                                                           */
/****************************************************************************/
int ccci_mdlog_base_req(int md_id, void *addr_vir, void *addr_phy, unsigned int *len)
{
    if(md_ctlb[md_id] == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    } else {
        if( addr_phy != NULL ) {
            *(unsigned int *)addr_phy = md_ctlb[md_id]->smem_table->ccci_mdlog_smem_base_phy;
        }
        if( addr_vir != NULL ) {
            *(unsigned int *)addr_vir = md_ctlb[md_id]->smem_table->ccci_mdlog_smem_base_virt;
        }
        if( len != NULL ) {
            *len = md_ctlb[md_id]->smem_table->ccci_mdlog_smem_size;
        }
        return 0;
    }
}

/*
 * ccci_pcm_base_reg: get PCM buffer information
 * @addr: kernel space buffer to store the address of PCM buffer
 * @len: kernel space buffer to store the length of PCM buffer
 * return 0 for success; negative value for failure
 */
int ccci_pcm_base_req(int md_id, void *addr_vir, void *addr_phy, unsigned int *len)
{
    if(md_ctlb[md_id] == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    } else {
        if( addr_phy != NULL ) {
            *(unsigned int *)addr_phy = md_ctlb[md_id]->smem_table->ccci_pcm_smem_base_phy;
        }
        if( addr_vir != NULL ) {
            *(unsigned int *)addr_vir = md_ctlb[md_id]->smem_table->ccci_pcm_smem_base_virt;
        }
        if( len != NULL ) {
            *len = md_ctlb[md_id]->smem_table->ccci_pcm_smem_size;
        }
        return 0;
    }
}

/*
 * ccci_uart_base_req: request TTY share buffer
 * @port: UART port to request
 * @addr: physical address of TTY share buffer
 * @len: length of TTY share buffer
 * return 0 for success; negative value for failure
 */
int ccci_uart_base_req(int md_id, int port, void *addr_vir, void *addr_phy, unsigned int *len)
{
    if (port >= CCCI_UART_PORT_NUM) {
        return -CCCI_ERR_INVALID_PARAM;
    }

    if(md_ctlb[md_id] == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    } else {
        if(addr_vir != NULL) {
            *(unsigned int *)addr_vir = md_ctlb[md_id]->smem_table->ccci_uart_smem_base_virt[port];
        }
        if(addr_phy != NULL) {
            *(unsigned int *)addr_phy = md_ctlb[md_id]->smem_table->ccci_uart_smem_base_phy[port];
        }
        if(len != NULL) {
            *len = md_ctlb[md_id]->smem_table->ccci_uart_smem_size[port];
        }
        return 0;
    }
}

/*
 * ccci_fs_base_req: request CCCI_FS share buffer
 * @addr: physical address of CCCI_FS share buffer
 * @len: length of CCCI_FS share buffer
 * return 0 for success; negative value for failure
 */
int ccci_fs_base_req(int md_id, void *addr_vir, void *addr_phy, unsigned int *len)
{
    if(md_ctlb[md_id] == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    } else {
        if(addr_vir != NULL) {
            *(unsigned int *)addr_vir = md_ctlb[md_id]->smem_table->ccci_fs_smem_base_virt;
        }
        if(addr_phy != NULL) {
            *(unsigned int *)addr_phy = md_ctlb[md_id]->smem_table->ccci_fs_smem_base_phy;
        }
        if(len != NULL) {
            *len = md_ctlb[md_id]->smem_table->ccci_fs_smem_size;
        }
        return 0;
    }
}

/*
 * ccci_rpc_base_req: requeset CCCI_FS share buffer
 * @addr: physical address of CCCI_FS share buffer
 * @len: length of CCCI_FS share buffer
 * return 0 for success; negative value for failure
 */
int ccci_rpc_base_req(int md_id, int *addr_vir, int *addr_phy, int *len)
{
    if(md_ctlb[md_id] == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    } else {
        if(addr_vir != NULL) {
            *(unsigned int *)addr_vir = md_ctlb[md_id]->smem_table->ccci_rpc_smem_base_virt;
        }
        if(addr_phy != NULL) {
            *(unsigned int *)addr_phy = md_ctlb[md_id]->smem_table->ccci_rpc_smem_base_phy;
        }
        if(len != NULL) {
            *len = md_ctlb[md_id]->smem_table->ccci_rpc_smem_size;
        }
        return 0;
    }
}


/*
 * ccci_pmic_base_req: get PMIC share buffer information
 * @addr: kernel space buffer to store the address of PMIC share buffer
 * @len: kernel space buffer to store the length of PMIC share buffer
 * return 0 for success; negative value for failure
 */
int ccci_pmic_base_req(int md_id, void *addr_vir, void *addr_phy, int *len)
{
    if(md_ctlb[md_id] == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    } else {
        if(addr_vir != NULL) {
            *(unsigned int *)addr_vir = md_ctlb[md_id]->smem_table->ccci_pmic_smem_base_virt;
        }
        if(addr_phy != NULL) {
            *(unsigned int *)addr_phy = md_ctlb[md_id]->smem_table->ccci_pmic_smem_base_phy;
        }
        if(len != NULL) {
            *len = md_ctlb[md_id]->smem_table->ccci_pmic_smem_size;
        }
        return 0;
    }
}

int ccci_ipc_base_req(int md_id, void *addr_vir, void *addr_phy, int *len)
{
    if(md_ctlb[md_id] == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    } else {
        if(addr_vir != NULL) {
            *(unsigned int *)addr_vir = md_ctlb[md_id]->smem_table->ccci_ipc_smem_base_virt;
        }
        if(addr_phy != NULL) {
            *(unsigned int *)addr_phy = md_ctlb[md_id]->smem_table->ccci_ipc_smem_base_phy;
        }
        if(len != NULL) {
            *len = md_ctlb[md_id]->smem_table->ccci_ipc_smem_size;
        }
        return 0;
    }
}

/*
 * ccmni_v2_ul_base_req: get CCMNI Up Link control buffer information
 * @addr: kernel space buffer to store the address of CCMNI control buffer
 * return 0 for success; negative value for failure
 */
int ccmni_v2_ul_base_req(int md_id, void *addr_vir, void *addr_phy)
{
    if(md_ctlb[md_id] == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    } else {
        if(addr_vir != NULL) {
            *(unsigned int *)addr_vir = md_ctlb[md_id]->smem_table->ccci_ccmni_smem_ul_base_virt;
        }
        if(addr_phy != NULL) {
            *(unsigned int *)addr_phy = md_ctlb[md_id]->smem_table->ccci_ccmni_smem_ul_base_phy;
        }
        return 0;
    }
}

int ccmni_v2_dl_base_req(int md_id, void *addr_vir, void *addr_phy)
{
    if(md_ctlb[md_id] == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    } else {
        if(addr_vir != NULL) {
            *(unsigned int *)addr_vir = md_ctlb[md_id]->smem_table->ccci_ccmni_smem_dl_base_virt;
        }
        if(addr_phy != NULL) {
            *(unsigned int *)addr_phy = md_ctlb[md_id]->smem_table->ccci_ccmni_smem_dl_base_phy;
        }
        return 0;
    }
}

int ccci_ccmni_v2_ctl_mem_base_req(int md_id, int port, int *addr_virt, int *addr_phy, int *len)
{
    if (port >= CCMNI_V2_PORT_NUM) {
        return -CCCI_ERR_INVALID_PARAM;
    }

    if(md_ctlb[md_id] == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    } else {
        *addr_virt = md_ctlb[md_id]->smem_table->ccci_ccmni_ctl_smem_base_virt[port];
        *addr_phy = md_ctlb[md_id]->smem_table->ccci_ccmni_ctl_smem_base_phy[port];
        *len = md_ctlb[md_id]->smem_table->ccci_ccmni_ctl_smem_size[port];
        return 0;
    }
}


/****************************************************************************/
/* API about getting modem information                                                                      */
/*                                                                                                                           */
/****************************************************************************/
int get_curr_md_state(int md_id)
{
    return md_ctlb[md_id]->md_boot_stage;
}

int get_md_exception_type(int md_id)
{
    return md_ctlb[md_id]->md_ex_type;
}

int get_common_cfg_setting(int md_id, int cfg[], int *num)
{
    #ifdef CONFIG_EVDO_DT_SUPPORT
    unsigned char*    str_slot1_mode = MTK_TELEPHONY_BOOTUP_MODE_SLOT1;
    unsigned char*    str_slot2_mode = MTK_TELEPHONY_BOOTUP_MODE_SLOT2;

    cfg[0] = str_slot1_mode[0] - '0';
    cfg[1] = str_slot2_mode[0] - '0';

    *num = 2;

    return 0;

    #else
    return -1;
    #endif
}


/****************************************************************************/
/* API about transfer information between AP and MD                                                    */
/*                                                                                                                           */
/****************************************************************************/
static int send_ccci_system_ch_msg(int md_id, unsigned int msg_id, unsigned int data)
{
    ccci_msg_t    msg;
    msg.data0 =    0xFFFFFFFF;
    msg.id = msg_id;
    msg.channel = CCCI_SYSTEM_TX;
    msg.reserved = data;
    return ccci_message_send(md_id, &msg, 1);
}


void lock_md_dormant(int md_id)
{
    ccci_msg_t    msg;
    msg.data0 = 0xFFFFFFFF;
    msg.id = MD_DORMANT_NOTIFY;
    msg.channel = CCCI_SYSTEM_TX;
    msg.reserved = 0x1;
    ccci_message_send(md_id, &msg, 1);
    CCCI_MSG_INF(md_id, "ctl", "notify modem lock dormant mode\n");
}

void unlock_md_dormant(int md_id)
{
    ccci_msg_t    msg;
    msg.data0 = 0xFFFFFFFF;
    msg.id = MD_DORMANT_NOTIFY;
    msg.channel = CCCI_SYSTEM_TX;
    msg.reserved = 0x0;
    ccci_message_send(md_id, &msg, 1);

    CCCI_MSG_INF(md_id, "ctl", "notify modem unlock dormant mode\n");
}

/*
ccci_dormancy
When "data_connect_sta" is not zero, which means there are data connection exist
So the suspend function should call this API to notify modem disconnect data 
connection before enter suspend state to save power. axs
 */
int ccci_dormancy(int md_id, char *buf, unsigned int len)
{
    if(atomic_read(&data_connect_sta[md_id]) > 0)
    {
        CCCI_MSG_INF(md_id, "ctl", "notify enter dormancy, data_sta<%d>\n", atomic_read(&data_connect_sta[md_id]));
        //ccci_write_mailbox(CCCI_SYSTEM_TX, 0xE);
        atomic_set(&data_connect_sta[md_id],0x0);
    }

    return 0;
}


/*
When data channel coming, the "data_connect_sta" should be marked
This function called in IRQ_handler function. 
*/
void check_data_connected(int md_id, int channel)
{
    if(channel >= CCCI_CCMNI1_RX && channel <= CCCI_CCMNI3_TX_ACK)
        atomic_inc((atomic_t*)&data_connect_sta[md_id]);
}


#if 0
void __ccci_dormancy(int md_id)
{
    char buf=0;
    ccci_dormancy(md_id, &buf, 1);
}
#endif


// ccci_sys_rx_dispatch_cb: CCCI_SYSTEM_RX message dispatch call back function for MODEM
// @buff: pointer to a CCCI buffer
// @private_data: pointer to private data of CCCI_SYSTEM_RX
void ccci_sys_rx_dispatch_cb(void *private)
{
    logic_channel_info_t    *ch_info = (logic_channel_info_t*)private;
    ccci_msg_t                msg;
    int                        md_id = ch_info->m_md_id;

    while(get_logic_ch_data(ch_info, &msg)) {
        exec_ccci_sys_call_back(md_id, msg.id, msg.reserved);
    }
}


int md_wdt_monitor(int md_id, int data)
{
    CCCI_MSG_INF(md_id, "ctl", "wdt msg\n");
    start_md_wdt_recov_timer(md_id);
    return 0;
}


int md_get_battery_info(int md_id, int data)
{
    ccci_system_message(md_id, CCCI_MD_MSG_SEND_BATTERY_INFO, 0);    
    CCCI_MSG_INF(md_id, "ctl", "request to send battery voltage to md\n");        

    return 0;
}

int md_set_sim_type(int md_id, int data)
{
    set_sim_type(md_id, data);
    CCCI_MSG_INF(md_id, "ctl", "receive sim type from md\n");        

    return 0;
}


void wakeup_md(int md_id)
{
    ccci_msg_t    msg;
    msg.data0 = 0xFFFFFFFF;
    msg.id = MD_WAKEN_UP;
    msg.channel = CCCI_SYSTEM_TX;
    msg.reserved = 0x0;
    ccci_message_send(md_id, &msg, 0x1);

    CCCI_MSG_INF(md_id, "ctl", "wake up md\n");
}


/****************************************************************************/
/* register store&show function of ccci debug log filter                                                    */
/*                                                                                                                            */
/****************************************************************************/
size_t ccci_msg_filter_store(char buf[], size_t len)
{
    unsigned int msg_mask=0;
    unsigned int key;
    int ret;
    int md_id;

    ret = sscanf(buf, "-l=%d 0x%x 0x%x", &md_id, &key, &msg_mask);
    if(ret != 3){
        CCCI_MSG("Parse msg filter fail: %d\n", ret);
    } else if( md_id >= MAX_MD_NUM){
        CCCI_MSG("Invalid MD sys ID:%d\n", md_id+1);
    } else if( key != 0x20111111){
        CCCI_MSG("Wrong key\n");
    } else{
        ccci_msg_mask[md_id] = msg_mask;
    }
    return len;
}

size_t ccci_msg_filter_show(char buf[], size_t len)
{
    int md_num;
    int i;
    int curr = 0;

    md_num = get_md_sys_max_num();

    for(i = 0; i < md_num; i++)
    {
        if(get_modem_is_enabled(i)){
            curr += snprintf(&buf[curr], len-curr, "[MD%d] msg mask: %x\n", i+1, ccci_msg_mask[i]);
        }
    }

    return curr;
}

size_t ccci_ch_filter_store(char buf[], size_t len)
{
    unsigned long long lg_ch_tx_mask= 0;
    unsigned long long lg_ch_rx_mask= 0;
    unsigned int tty_mask= 0;
    unsigned int fs_tx_mask= 0;
    unsigned int fs_rx_mask = 0;
    unsigned int key = 0;
    int    md_id;
    int ret = 0;

    //ret = sscanf(buf, "-c=0x%x 0x%llX 0x%llX 0x%x 0x%x 0x%x", &key, &lg_ch_tx_mask, 
    //    &lg_ch_rx_mask, &fs_tx_mask, &fs_rx_mask, &tty_mask);
    ret = sscanf(buf, "-c=%d 0x%x 0x%llx 0x%llx 0x%x 0x%x 0x%x ", &md_id, &key, &lg_ch_tx_mask, \
                    &lg_ch_rx_mask, &fs_tx_mask, &fs_rx_mask, &tty_mask);
    if(ret != 7){
        CCCI_MSG("Parse channel filter fail: %d\n", ret);
    } else if ( md_id >= MAX_MD_NUM){
        CCCI_MSG("Invalid MD sys ID:%d\n", md_id+1);
    } else if ( key != 0x20111111){
        CCCI_MSG("Wrong key\n");
    } else {
        CCCI_MSG("%x %x %x %x %llx %llx\n", key, fs_tx_mask, fs_rx_mask, tty_mask, \
            lg_ch_tx_mask, lg_ch_rx_mask);
        lg_ch_tx_debug_enable[md_id] = lg_ch_tx_mask;
        lg_ch_rx_debug_enable[md_id] = lg_ch_rx_mask;
        fs_tx_debug_enable[md_id] = fs_tx_mask;
        fs_rx_debug_enable[md_id] = fs_rx_mask;
        tty_debug_enable[md_id] = tty_mask;
    }
    return len;
}

size_t ccci_ch_filter_show(char buf[], size_t len)
{
    int md_num;
    int i;
    int curr = 0;

    md_num = get_md_sys_max_num();

    for(i=0; i<md_num; i++) {
        if(get_modem_is_enabled(i)) {
            curr += snprintf(&buf[curr], len-curr, "[MD%d] tx_mask: %llX\nrx_mask: %llX\nfs_tx_mask: %X\nfs_rx_mask: %X\ntty_msk: %X\n",
                            i+1, lg_ch_tx_debug_enable[i], lg_ch_rx_debug_enable[i],
                            fs_tx_debug_enable[i], fs_rx_debug_enable[i], tty_debug_enable[i]);
        }
    }

    return curr;
}


/****************************************************************************/
/* API about dump memory information                                                                        */
/*                                                                                                                            */
/****************************************************************************/
static void ccci_mem_dump(void *start_addr, int len)
{
    unsigned int *curr_p = (unsigned int *)start_addr;
    unsigned char *curr_ch_p;
    int _16_fix_num = len/16;
    int tail_num = len%16;
    char buf[16];
    int i,j;

    if(NULL == curr_p) {
        CCCI_MSG("NULL point!\n");
        return;
    }
    if(0 == len){
        CCCI_MSG("Not need to dump\n");
        return;
    }

    CCCI_DBG_COM_MSG("Base: %08x\n", (unsigned int)start_addr);
    // Fix section
    for(i=0; i<_16_fix_num; i++){
        CCCI_DBG_COM_MSG("%03X: %08X %08X %08X %08X\n", 
                i*16, *curr_p, *(curr_p+1), *(curr_p+2), *(curr_p+3) );
        curr_p+=4;
    }

    // Tail section
    if(tail_num > 0){
        curr_ch_p = (unsigned char*)curr_p;
        for(j=0; j<tail_num; j++){
            buf[j] = *curr_ch_p;
            curr_ch_p++;
        }
        for(; j<16; j++)
            buf[j] = 0;
        curr_p = (unsigned int*)buf;
        CCCI_DBG_COM_MSG("%03X: %08X %08X %08X %08X\n", 
                i*16, *curr_p, *(curr_p+1), *(curr_p+2), *(curr_p+3) );
    }
}

void ccci_ee_info_dump(int md_id, DEBUG_INFO_T *debug_info)
{
    char ex_info[EE_BUF_LEN]="";
    char i_bit_ex_info[EE_BUF_LEN]="\n[Others] May I-Bit dis too long\n";
    
    struct rtc_time        tm;
    struct timeval        tv = {0};
    struct timeval        tv_android = {0};
    struct rtc_time        tm_android;

    do_gettimeofday(&tv);
    tv_android = tv;
    rtc_time_to_tm(tv.tv_sec, &tm);
    tv_android.tv_sec -= sys_tz.tz_minuteswest*60;
    rtc_time_to_tm(tv_android.tv_sec, &tm_android);
    CCCI_MSG_INF(md_id, "cci", "Sync:%d%02d%02d %02d:%02d:%02d.%u(%02d:%02d:%02d.%03d(TZone))\n", 
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
           tm.tm_hour, tm.tm_min, tm.tm_sec,
           (unsigned int) tv.tv_usec,
           tm_android.tm_hour, tm_android.tm_min, tm_android.tm_sec,
           (unsigned int) tv_android.tv_usec);

    CCCI_MSG_INF(md_id, "cci", "exception type(%d):%s\n",debug_info->type,debug_info->name?:"Unknown");

    switch(debug_info->type)
    {
        case MD_EX_TYPE_ASSERT_DUMP:
        case MD_EX_TYPE_ASSERT:
            CCCI_MSG_INF(md_id, "cci", "filename = %s\n", debug_info->assert.file_name);
            CCCI_MSG_INF(md_id, "cci", "line = %d\n", debug_info->assert.line_num);
            CCCI_MSG_INF(md_id, "cci", "para0 = %d, para1 = %d, para2 = %d\n", 
                    debug_info->assert.parameters[0],
                    debug_info->assert.parameters[1],
                    debug_info->assert.parameters[2]);
            snprintf(ex_info,EE_BUF_LEN,"\n[%s] file:%s line:%d\np1:0x%08x\np2:0x%08x\np3:0x%08x\n",
                    debug_info->name, 
                    debug_info->assert.file_name,
                    debug_info->assert.line_num, 
                    debug_info->assert.parameters[0],
                    debug_info->assert.parameters[1],
                    debug_info->assert.parameters[2]);
            break;
        case MD_EX_TYPE_FATALERR_BUF:
        case MD_EX_TYPE_FATALERR_TASK:
            CCCI_MSG_INF(md_id, "cci", "fatal error code 1 = %d\n", debug_info->fatal_error.err_code1);
            CCCI_MSG_INF(md_id, "cci", "fatal error code 2 = %d\n", debug_info->fatal_error.err_code2);
            snprintf(ex_info,EE_BUF_LEN,"\n[%s] err_code1:%d err_code2:%d\n", debug_info->name, 
                    debug_info->fatal_error.err_code1, debug_info->fatal_error.err_code2);
            break;
        case MD_EX_TYPE_EMI_CHECK:
            CCCI_MSG_INF(md_id, "cci", "md_emi_check: %08X, %08X, %02d, %08X\n", 
                    debug_info->data.data0, debug_info->data.data1,
                    debug_info->data.channel, debug_info->data.reserved);
            snprintf(ex_info,EE_BUF_LEN,"\n[emi_chk] %08X, %08X, %02d, %08X\n", 
                    debug_info->data.data0, debug_info->data.data1,
                    debug_info->data.channel, debug_info->data.reserved);
            break;
        case DSP_EX_TYPE_ASSERT:
            CCCI_MSG_INF(md_id, "cci", "filename = %s\n", debug_info->dsp_assert.file_name);
            CCCI_MSG_INF(md_id, "cci", "line = %d\n", debug_info->dsp_assert.line_num);
            CCCI_MSG_INF(md_id, "cci", "exec unit = %s\n", debug_info->dsp_assert.execution_unit);
            CCCI_MSG_INF(md_id, "cci", "para0 = %d, para1 = %d, para2 = %d\n", 
                    debug_info->dsp_assert.parameters[0],
                    debug_info->dsp_assert.parameters[1],
                    debug_info->dsp_assert.parameters[2]);
            snprintf(ex_info,EE_BUF_LEN,"\n[%s] file:%s line:%d\nexec:%s\np1:%d\np2:%d\np3:%d\n",
                    debug_info->name, debug_info->assert.file_name, debug_info->assert.line_num,
                    debug_info->dsp_assert.execution_unit, 
                    debug_info->dsp_assert.parameters[0],
                    debug_info->dsp_assert.parameters[1],
                    debug_info->dsp_assert.parameters[2]);
            break;
        case DSP_EX_TYPE_EXCEPTION:
            CCCI_MSG_INF(md_id, "cci", "exec unit = %s, code1:0x%08x\n", debug_info->dsp_exception.execution_unit,
                    debug_info->dsp_exception.code1);
            snprintf(ex_info,EE_BUF_LEN,"\n[%s] exec:%s code1:0x%08x\n",
                    debug_info->name, debug_info->dsp_exception.execution_unit,
                    debug_info->dsp_exception.code1);
            break;
        case DSP_EX_FATAL_ERROR:
            CCCI_MSG_INF(md_id, "cci", "exec unit = %s\n", debug_info->dsp_fatal_err.execution_unit);
            CCCI_MSG_INF(md_id, "cci", "err_code0 = 0x%08x, err_code1 = 0x%08x\n", 
                    debug_info->dsp_fatal_err.err_code[0],
                    debug_info->dsp_fatal_err.err_code[1]);

            snprintf(ex_info,EE_BUF_LEN,"\n[%s] exec:%s err_code1:0x%08x err_code2:0x%08x\n",
                    debug_info->name, debug_info->dsp_fatal_err.execution_unit, 
                    debug_info->dsp_fatal_err.err_code[0],
                    debug_info->dsp_fatal_err.err_code[1]);
            break;
        default: // Only display exception name
            snprintf(ex_info,EE_BUF_LEN,"\n[%s]\n", debug_info->name);
            break;
    }

    // Add additional info
    switch(debug_info->more_info)
    {
        case MD_EE_CASE_ONLY_EX:
            strcat(ex_info, "\nTime out case\n");
            break;
            
        case MD_EE_CASE_ONLY_EX_OK:
            strcat(ex_info, "\nOnly EX_OK case\n");
            break;
        case MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG:
            strcat(i_bit_ex_info, ex_info);
            strcpy(ex_info, i_bit_ex_info);
            #if defined (CONFIG_MTK_AEE_FEATURE) && defined (ENABLE_AEE_MD_EE)
            aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_FTRACE, "CCCI", i_bit_ex_info);
            #endif
            break;
        case MD_EE_CASE_TX_TRG:
        case MD_EE_CASE_ISR_TRG:
            //strcat("\n[Others] May I-Bit dis too long\n", ex_info);
            break;

        case MD_EE_CASE_NO_RESPONSE:
            strcat(ex_info, "\n[Others] MD long time no response\n");
            break;
            
        default:
            break;
    }

    // Dump Exception share memory
    CCCI_DBG_MSG(md_id, "cci", "\n\n");
    CCCI_DBG_MSG(md_id, "cci", "Dump MD%d Exception share memory\n", md_id+1);
    ccci_mem_dump((int*)md_ctlb[md_id]->smem_table->ccci_exp_smem_base_virt, md_ctlb[md_id]->smem_table->ccci_exp_smem_size);

    // Dump MD MCU  register
    CCCI_MSG_INF(md_id, "cci", "Dump MD MCU register(dummy)(%d)+\n", get_curr_md_state(md_id));
    ccci_dump_md_register(md_id);
    CCCI_MSG_INF(md_id, "cci", "Dump MD MCU register(dummy)(%d)\n", get_curr_md_state(md_id));
    
    // Dump MD image memory
    CCCI_DBG_MSG(md_id, "cci", "\n\n");
    CCCI_DBG_MSG(md_id, "cci", "Dump MD%d Image memory\n", md_id+1);
    ccci_mem_dump((int*)md_ctlb[md_id]->md_layout->md_region_vir, MD_IMG_DUMP_SIZE);
    // Dump MD image memory
    CCCI_DBG_MSG(md_id, "cci", "\n\n");
    CCCI_DBG_MSG(md_id, "cci", "Dump MD%d layout struct\n", md_id+1);
    ccci_mem_dump((int*)md_ctlb[md_id]->md_layout, sizeof(ccci_mem_layout_t));

    // Dump Logical layer info
    CCCI_DBG_MSG(md_id, "cci", "\n\n");
    CCCI_DBG_MSG(md_id, "cci", "Dump MD%d logic layer info\n", md_id+1);
    ccci_dump_logic_layer_info(md_id, NULL, 0);

    ccci_aed(md_id, CCCI_AED_DUMP_EX_MEM|CCCI_AED_DUMP_MD_IMG_MEM, ex_info);
}
extern void ccmni_v2_dump(int md_id);
static void ccci_dump_runtime_data(int md_id, modem_runtime_t *runtime, smem_alloc_t *smem)
{
    int        i;
    char    ctmp[12];
    int        *p;

    p = (int*)ctmp;
    *p = runtime->Prefix;
    p++;
    *p = runtime->Platform_L;
    p++;
    *p = runtime->Platform_H;

    CCCI_MSG_INF(md_id, "ctl", "**********************************************\n");
    CCCI_DBG_MSG(md_id, "ctl", "Prefix                      %c%c%c%c\n", ctmp[0], ctmp[1], ctmp[2], ctmp[3]);
    CCCI_MSG_INF(md_id, "ctl", "Platform_L                  %c%c%c%c\n", ctmp[4], ctmp[5], ctmp[6], ctmp[7]);
    CCCI_MSG_INF(md_id, "ctl", "Platform_H                  %c%c%c%c\n", ctmp[8], ctmp[9], ctmp[10], ctmp[11]);
    CCCI_MSG_INF(md_id, "ctl", "DriverVersion               0x%x\n", runtime->DriverVersion);
    CCCI_DBG_MSG(md_id, "ctl", "BootChannel                 %d\n", runtime->BootChannel);
    CCCI_MSG_INF(md_id, "ctl", "BootingStartID(Mode)        0x%x\n", runtime->BootingStartID);
    CCCI_DBG_MSG(md_id, "ctl", "BootAttributes              %d\n", runtime->BootAttributes);
    CCCI_DBG_MSG(md_id, "ctl", "BootReadyID                 %d\n", runtime->BootReadyID);
    
    CCCI_DBG_MSG(md_id, "ctl", "ExceShareMemBase            0x%x\n", runtime->ExceShareMemBase);
    CCCI_DBG_MSG(md_id, "ctl", "ExceShareMemSize            0x%x\n", runtime->ExceShareMemSize);
    
    CCCI_DBG_MSG(md_id, "ctl", "MDExExpInfoBase             0x%x\n", runtime->MDExExpInfoBase);
    CCCI_DBG_MSG(md_id, "ctl", "MDExExpInfoSize             0x%x\n", runtime->MDExExpInfoSize);
    
    CCCI_DBG_MSG(md_id, "ctl", "MiscInfoBase                0x%x\n", runtime->MiscInfoBase);
    CCCI_DBG_MSG(md_id, "ctl", "MiscInfoSize                0x%x\n", runtime->MiscInfoSize);
    
    CCCI_DBG_MSG(md_id, "ctl", "PcmShareMemBase             0x%x\n", runtime->PcmShareMemBase);
    CCCI_DBG_MSG(md_id, "ctl", "PcmShareMemSize             0x%x\n", runtime->PcmShareMemSize);
    
    CCCI_DBG_MSG(md_id, "ctl", "MdlogShareMemBase           0x%x\n", runtime->MdlogShareMemBase);
    CCCI_DBG_MSG(md_id, "ctl", "MdlogShareMemSize           0x%x\n", runtime->MdlogShareMemSize);

    CCCI_DBG_MSG(md_id, "ctl", "RpcShareMemBase             0x%x\n", runtime->RpcShareMemBase);
    CCCI_DBG_MSG(md_id, "ctl", "RpcShareMemSize             0x%x\n", runtime->RpcShareMemSize);
    
    CCCI_DBG_MSG(md_id, "ctl", "FileShareMemBase            0x%x\n", runtime->FileShareMemBase);
    CCCI_DBG_MSG(md_id, "ctl", "FileShareMemSize            0x%x\n", runtime->FileShareMemSize);

    CCCI_DBG_MSG(md_id, "ctl", "UartPortNum                 %d\n", runtime->UartPortNum);
    for (i = 0; i < CCCI_UART_PORT_NUM; i++) {
        CCCI_DBG_MSG(md_id, "ctl", "UartShareMemBase[%d]          0x%x\n", i, runtime->UartShareMemBase[i]);
        CCCI_DBG_MSG(md_id, "ctl", "UartShareMemSize[%d]          0x%x\n", i, runtime->UartShareMemSize[i]);
    }
    
    //CCCI_DBG_MSG(md_id, "ctl", "PmicShareMemBase            0x%x\n", runtime->PmicShareMemBase);
    //CCCI_DBG_MSG(md_id, "ctl", "PmicShareMemSize            0x%x\n", runtime->PmicShareMemSize);
    
    //CCCI_DBG_MSG(md_id, "ctl",  "SysShareMemBase             0x%x\n", runtime->SysShareMemBase);
    //CCCI_DBG_MSG(md_id, "ctl", "SysShareMemSize             0x%x\n", runtime->SysShareMemSize);
    
    CCCI_DBG_MSG(md_id, "ctl", "IPCShareMemBase             0x%x\n",runtime->IPCShareMemBase);
    CCCI_DBG_MSG(md_id, "ctl", "IPCShareMemSize             0x%x\n",runtime->IPCShareMemSize);

    CCCI_DBG_MSG(md_id, "ctl", "NetPortNum                  %d\n", runtime->NetPortNum);
    CCCI_DBG_MSG(md_id, "ctl", "MDULNetShareMemBase         0x%x\n", runtime->MDULNetShareMemBase);
    CCCI_DBG_MSG(md_id, "ctl", "MDULNetShareMemSize         0x%x\n", runtime->MDULNetShareMemSize);
    CCCI_DBG_MSG(md_id, "ctl", "MDDLNetShareMemBase         0x%x\n", runtime->MDDLNetShareMemBase);
    CCCI_DBG_MSG(md_id, "ctl", "MDDLNetShareMemSize         0x%x\n", runtime->MDDLNetShareMemSize);

#if 0
    for (i = 0; i < NET_PORT_NUM; i++) {
        CCCI_DBG_MSG(md_id, "ctl", "NetDLCtrlShareMemBase[%d]    0x%x\n", i, runtime->NetDLCtrlShareMemBase[i]);
        CCCI_DBG_MSG(md_id, "ctl", "NetDLCtrlShareMemSize[%d]    0x%x\n", i, runtime->NetDLCtrlShareMemSize[i]);
        
        CCCI_DBG_MSG(md_id, "ctl", "NetULCtrlShareMemBase[%d]    0x%x\n", i, runtime->NetULCtrlShareMemBase[i]);
        CCCI_DBG_MSG(md_id, "ctl", "NetULCtrlShareMemSize[%d]    0x%x\n", i, runtime->NetULCtrlShareMemSize[i]);
    }
#endif
    CCCI_DBG_MSG(md_id, "ctl", "CheckSum                    %d\n", runtime->CheckSum);

    p = (int*)ctmp;
    *p = runtime->Postfix;
    CCCI_DBG_MSG(md_id, "ctl", "Postfix                     %c%c%c%c\n", ctmp[0], ctmp[1], ctmp[2], ctmp[3]);

    CCCI_DBG_MSG(md_id, "ctl", "----------------------------------------------\n");

    CCCI_MSG_INF(md_id, "ctl", "ccci_smem_virt              0x%x\n", (unsigned int)smem->ccci_smem_vir);
    CCCI_MSG_INF(md_id, "ctl", "ccci_smem_phy,              AP=0x%x,MD=0x%x\n", (unsigned int)smem->ccci_smem_phy,(unsigned int)smem->ccci_smem_phy - get_md2_ap_phy_addr_fixed());
    CCCI_MSG_INF(md_id, "ctl", "ccci_smem_size              0x%x\n", smem->ccci_smem_size);

    CCCI_DBG_MSG(md_id, "ctl", "md_runtime_data_smem_virt   0x%x\n", smem->ccci_md_runtime_data_smem_base_virt);
    CCCI_DBG_MSG(md_id, "ctl", "md_runtime_data_smem_phy    AP=0x%x,MD=0x%x\n", smem->ccci_md_runtime_data_smem_base_phy,smem->ccci_md_runtime_data_smem_base_phy - get_md2_ap_phy_addr_fixed());
    CCCI_DBG_MSG(md_id, "ctl", "md_runtime_data_smem_size   0x%x\n", smem->ccci_md_runtime_data_smem_size);
    CCCI_MSG_INF(md_id, "ctl", "**********************************************\n");
    ccmni_v2_dump(md_id);
}



/****************************************************************************/
/* API about modem exception handle                                                                         */
/*                                                                                                                           */
/****************************************************************************/
static void ccci_md_exception(int md_id, DEBUG_INFO_T *debug_info)
{
    EX_LOG_T    *ex_info;
    int            ee_type;

    if(debug_info == NULL) {
        return;
    }

    ex_info = (EX_LOG_T*)md_ctlb[md_id]->smem_table->ccci_exp_smem_base_virt;
    memset(debug_info,0,sizeof(DEBUG_INFO_T));
    ee_type = ex_info->header.ex_type;
    debug_info->type = ee_type;
    md_ctlb[md_id]->md_ex_type = ee_type;

    switch (ee_type) 
    {
        case MD_EX_TYPE_INVALID:
            debug_info->name="INVALID";
            break;

        case MD_EX_TYPE_UNDEF:
            debug_info->name="UNDEF";
            break;

        case MD_EX_TYPE_SWI:
            debug_info->name="SWI";
            break;

        case MD_EX_TYPE_PREF_ABT:
            debug_info->name="PREFETCH ABORT";
            break;

        case MD_EX_TYPE_DATA_ABT:
            debug_info->name="DATA ABORT";
            break;

        case MD_EX_TYPE_ASSERT:
            debug_info->name="ASSERT";
            snprintf(debug_info->assert.file_name,sizeof(debug_info->assert.file_name),
                    ex_info->content.assert.filename);    
            debug_info->assert.line_num = ex_info->content.assert.linenumber;
            debug_info->assert.parameters[0] = ex_info->content.assert.parameters[0];
            debug_info->assert.parameters[1] = ex_info->content.assert.parameters[1];
            debug_info->assert.parameters[2] = ex_info->content.assert.parameters[2];
            break;

        case MD_EX_TYPE_FATALERR_TASK:
            debug_info->name="Fatal error (task)";
            debug_info->fatal_error.err_code1=ex_info->content.fatalerr.error_code.code1;
            debug_info->fatal_error.err_code2=ex_info->content.fatalerr.error_code.code2;
            break;

        case MD_EX_TYPE_FATALERR_BUF:
            debug_info->name="Fatal error (buff)";
            debug_info->fatal_error.err_code1=ex_info->content.fatalerr.error_code.code1;
            debug_info->fatal_error.err_code2=ex_info->content.fatalerr.error_code.code2;
            break;

        case MD_EX_TYPE_LOCKUP:
            debug_info->name="Lockup";
            break;

        case MD_EX_TYPE_ASSERT_DUMP:
            debug_info->name="ASSERT DUMP";
            snprintf(debug_info->assert.file_name,sizeof(debug_info->assert.file_name),
                    ex_info->content.assert.filename);
            debug_info->assert.line_num=ex_info->content.assert.linenumber;
            break;

        case DSP_EX_TYPE_ASSERT:
            debug_info->name="MD DMD ASSERT";
            snprintf(debug_info->dsp_assert.file_name,sizeof(debug_info->dsp_assert.file_name),
                    ex_info->content.assert.filename);
            debug_info->dsp_assert.line_num = ex_info->content.assert.linenumber;
            snprintf(debug_info->dsp_assert.execution_unit,sizeof(debug_info->dsp_assert.execution_unit),
                    ex_info->envinfo.execution_unit);    
            debug_info->dsp_assert.parameters[0] = ex_info->content.assert.parameters[0];
            debug_info->dsp_assert.parameters[1] = ex_info->content.assert.parameters[1];
            debug_info->dsp_assert.parameters[2] = ex_info->content.assert.parameters[2];
            break;

        case DSP_EX_TYPE_EXCEPTION:
            debug_info->name="MD DMD Exception";
            snprintf(debug_info->dsp_exception.execution_unit,sizeof(debug_info->dsp_exception.execution_unit),
                    ex_info->envinfo.execution_unit);
            debug_info->dsp_exception.code1 = ex_info->content.fatalerr.error_code.code1;
            break;

        case DSP_EX_FATAL_ERROR:
            debug_info->name="MD DMD FATAL ERROR";
            snprintf(debug_info->dsp_fatal_err.execution_unit,sizeof(debug_info->dsp_fatal_err.execution_unit),
                    ex_info->envinfo.execution_unit);    
            debug_info->dsp_fatal_err.err_code[0] = ex_info->content.fatalerr.error_code.code1;
            debug_info->dsp_fatal_err.err_code[1] = ex_info->content.fatalerr.error_code.code2;
            break;

        default:
            debug_info->name= "UNKNOWN Exception";
            break;
    }

    debug_info->ext_mem=(int*)ex_info;
    debug_info->ext_size=md_ctlb[md_id]->smem_table->ccci_exp_smem_size;
    debug_info->md_image=(int*)md_ctlb[md_id]->md_layout->md_region_vir;
    debug_info->md_size=MD_IMG_DUMP_SIZE;
}

static void ex_monitor_func(unsigned long data)
{
    //int                md_ex_get, md_ex_ok_get; 
    //int                trusted = 0;
    //volatile int    reentrant_times;
    int                ee_on_going = 0;
    int                ee_case;
    int                need_update_state = 0;
    unsigned long    flags;
    unsigned int    ee_info_flag;
    md_ctl_block_t    *md_ctlb = (md_ctl_block_t *)data;
    DEBUG_INFO_T    debug_info;

    spin_lock_irqsave(&md_ctlb->ctl_lock, flags);
    if((1<<MD_EE_DUMP_ON_GOING)&md_ctlb->ee_info_flag) {
        ee_on_going = 1;
    } else {
        ee_info_flag = md_ctlb->ee_info_flag;
        md_ctlb->ee_info_flag |= (1<<MD_EE_DUMP_ON_GOING);
    }
    spin_unlock_irqrestore(&md_ctlb->ctl_lock, flags);                

    if(ee_on_going)
        return;

    if ((ee_info_flag&((1<<MD_EE_MSG_GET)|(1<<MD_EE_OK_MSG_GET))) == \
                        ((1<<MD_EE_MSG_GET)|(1<<MD_EE_OK_MSG_GET))) {
        ee_case = MD_EE_CASE_NORMAL;
        CCCI_DBG_MSG(md_ctlb->m_md_id, "ctl", "Receive MD_EX_REC_OK\n");
        if (ee_info_flag & (1 << MD_EE_AP_MASK_I_BIT_TOO_LONG)) {
            ee_case = MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG;
        }

    } else if((ee_info_flag&((1<<MD_EE_MSG_GET)|(1<<MD_EE_OK_MSG_GET))) == (1<<MD_EE_MSG_GET)) {
        ee_case = MD_EE_CASE_ONLY_EX;
        CCCI_DBG_MSG(md_ctlb->m_md_id, "ctl", \
                                "Only recv MD_EX, timeout trigger dump. Dump data may be not correct.\n");
        if (ee_info_flag & (1 << MD_EE_AP_MASK_I_BIT_TOO_LONG)) {
            ee_case = MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG;
        }

    } else if((ee_info_flag&((1<<MD_EE_MSG_GET)|(1<<MD_EE_OK_MSG_GET))) == (1<<MD_EE_OK_MSG_GET)) {
        ee_case = MD_EE_CASE_ONLY_EX_OK;
        CCCI_DBG_MSG(md_ctlb->m_md_id, "ctl", \
                                "Only recv MD_EX_OK, No physical channel occur.\n");
        if (ee_info_flag & (1 << MD_EE_AP_MASK_I_BIT_TOO_LONG)) {
            ee_case = MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG;
        }
    } else if (ee_info_flag & (1 << MD_EE_AP_MASK_I_BIT_TOO_LONG)) {
        ee_case = MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG;
        if((ee_info_flag&(1<<MD_STATE_UPDATE))==0){
            need_update_state = 1;
        }        
    } else if(ee_info_flag&(1<<MD_EE_FOUND_BY_ISR)) {
        ee_case = MD_EE_CASE_ISR_TRG;
        if((ee_info_flag&(1<<MD_STATE_UPDATE))==0){
            need_update_state = 1;
        }
    } else if(ee_info_flag&(1<<MD_EE_FOUND_BY_TX)) {
        ee_case = MD_EE_CASE_TX_TRG;
        if((ee_info_flag&(1<<MD_STATE_UPDATE))==0){
            need_update_state = 1;
        }
    } else if(ee_info_flag&(1<<MD_EE_PENDING_TOO_LONG)) {
        ee_case = MD_EE_CASE_NO_RESPONSE;
        if((ee_info_flag&(1<<MD_STATE_UPDATE))==0){
            need_update_state = 1;
            CCCI_DBG_MSG(md_ctlb->m_md_id, "ctl", "MD no response > 1500ms.\n");
        }
    } else { 
        CCCI_DBG_MSG(md_ctlb->m_md_id, "ctl", "Invalid MD_EX\n");
        goto _dump_done;
    }

    if(need_update_state) {
        spin_lock_irqsave(&md_ctlb->ctl_lock, flags);
        //set_curr_md_state(md_ctlb->m_md_id, MD_BOOT_STAGE_EXCEPTION);
        md_ctlb->md_boot_stage = MD_BOOT_STAGE_EXCEPTION;
        md_ctlb->need_reload_image = 1;
        spin_unlock_irqrestore(&md_ctlb->ctl_lock, flags);
        md_call_chain(&md_ctlb->md_notifier,CCCI_MD_EXCEPTION);
    }

    ccci_system_message(md_ctlb->m_md_id, CCCI_MD_MSG_NOTIFY, ee_case);
    ccci_md_exception(md_ctlb->m_md_id, &debug_info);
    debug_info.more_info = ee_case;
    ccci_ee_info_dump(md_ctlb->m_md_id, &debug_info);

_dump_done:
    spin_lock_irqsave(&md_ctlb->ctl_lock, flags);
    md_ctlb->ee_info_flag = 0;
    spin_unlock_irqrestore(&md_ctlb->ctl_lock, flags);

}


void ccci_aed(int md_id, unsigned int dump_flag, char *aed_str)
{
    #define AED_STR_LEN        (512)
    int *ex_log_addr = NULL;
    int ex_log_len = 0;
    int *md_img_addr = NULL;
    int md_img_len = 0;
    int info_str_len = 0;
    char buff[AED_STR_LEN];
    char *img_inf;

    img_inf = get_md_info_str(md_id);
    if(img_inf == NULL)
        img_inf = "";
    info_str_len = strlen(aed_str);
    info_str_len += strlen(img_inf);

    if(info_str_len > AED_STR_LEN){
        buff[AED_STR_LEN-1] = '\0'; // Cut string length to AED_STR_LEN
    }

    snprintf(buff, AED_STR_LEN, "md%d:%s%s", md_id+1, aed_str, img_inf);

    if(dump_flag & CCCI_AED_DUMP_EX_MEM){
        ex_log_addr = (int*)md_ctlb[md_id]->smem_table->ccci_exp_smem_base_virt;
        ex_log_len = md_ctlb[md_id]->smem_table->ccci_exp_smem_size;
    }
    if(dump_flag & CCCI_AED_DUMP_MD_IMG_MEM){
        md_img_addr = (int*)md_ctlb[md_id]->md_layout->md_region_vir;
        md_img_len = MD_IMG_DUMP_SIZE;
    }
    if(dump_flag & CCCI_AED_DUMP_CCIF_REG) {        
        ex_log_addr = (int *)md_ctlb[md_id]->smem_table->ccci_exp_smem_base_virt;
        ex_log_len = 68*4;
        ccci_dump_hw_reg_val(md_id, (unsigned int*)ex_log_addr, ex_log_len);
    }

    #if defined (CONFIG_MTK_AEE_FEATURE) && defined (ENABLE_AEE_MD_EE)
    aed_md_exception(ex_log_addr, ex_log_len, md_img_addr, md_img_len, buff);
    #endif
}

void md_emi_check(int md_id, ccci_msg_t *buff, DEBUG_INFO_T *debug_info)
{
    if ((buff==NULL) || (debug_info==NULL))
        return;

    memset(debug_info,0,sizeof(DEBUG_INFO_T));
    debug_info->type=MD_EX_TYPE_EMI_CHECK;
    debug_info->name="EMI_CHK";
    debug_info->data=*buff;

    if(md_id < get_md_sys_max_num()) {
        md_ctlb[md_id]->md_ex_type = MD_EX_TYPE_EMI_CHECK;
    }
}

void md_ee_info_check(int md_id, unsigned int *p_ee_info)
{
    md_ctl_block_t            *ctl_b = md_ctlb[md_id];
    modem_exception_exp_t    *exp_ee_info;

    *p_ee_info = 0;    
    exp_ee_info = (modem_exception_exp_t*)ctl_b->smem_table->ccci_md_ex_exp_info_smem_base_virt;

    if(exp_ee_info->exception_occur) {
        *p_ee_info = exp_ee_info->exception_occur << MD_EE_INFO_OFFSET;
    }
}

void ccci_check_md_no_physical_channel(int md_id, unsigned int args)
{
    md_ctl_block_t            *ctl_b = md_ctlb[md_id];
    modem_exception_exp_t    *exp_ee_info;
    int                        trigger_ee = 0;
    int                        trigger_ee_timer = 0;
    unsigned long            flags;
    unsigned int            ee_info;

    md_ee_info_check(md_id, &ee_info);

    if (ee_info){
        if(ctl_b->ee_info_got == 0) {
            ctl_b->ee_info_got = 1;
            exp_ee_info = (modem_exception_exp_t*)ctl_b->smem_table->ccci_md_ex_exp_info_smem_base_virt;
            CCCI_MSG_INF(md_id, "ctl", "receive MD_EX @S(%08X)(%d:%d)\n", exp_ee_info->exception_occur, \
                            exp_ee_info->send_time, exp_ee_info->wait_time);
            ccci_dump_hw_reg_val(md_id, NULL, 0);
            trigger_ee = 1;
        }
    }

    // Trigger EE if needed
    if(trigger_ee) {
        spin_lock_irqsave(&ctl_b->ctl_lock, flags);
        ctl_b->ee_info_flag |= ((1<<MD_EE_FLOW_START)|(1<<MD_EE_FOUND_BY_TX) | ee_info);
        if((ctl_b->ee_info_flag & (1<<MD_STATE_UPDATE)) == 0) {
            trigger_ee_timer = 1;
        }
        ctl_b->md_boot_stage = MD_BOOT_STAGE_EXCEPTION;
        spin_unlock_irqrestore(&ctl_b->ctl_lock, flags);
        if(trigger_ee_timer) {
            mod_timer(&ctl_b->md_ex_monitor,jiffies+EE_TIMER_BASE+2);
        }
    } else {
        if(ctl_b->ee_info_got == 0) {
            switch(args) 
            {
                case 1: // pending 50ms, dump EE buffer only
                    CCCI_MSG_INF(md_id, "cci", "TX 50ms\n");
                    ccci_dump_hw_reg_val(md_id, NULL, 0);
                    CCCI_MSG_INF(md_id, "cci", "Dump MD%d Exception share memory\n", md_id+1);
                		ccci_mem_dump((int*)md_ctlb[md_id]->smem_table->ccci_exp_smem_base_virt, md_ctlb[md_id]->smem_table->ccci_exp_smem_size);
                    break;

                case 2: //pending 1500ms, trigger EE
                    CCCI_MSG_INF(md_id, "cci", "TX 1500ms\n");
                    if ((ctl_b->ee_info_flag & (1<<MD_EE_FLOW_START)) == 0) {
                        ccci_dump_hw_reg_val(md_id, NULL, 0);
                    }
                    
                    spin_lock_irqsave(&ctl_b->ctl_lock, flags);
                    if((ctl_b->ee_info_flag & ((1<<MD_EE_PENDING_TOO_LONG)|(1<<MD_STATE_UPDATE))) == 0) {
                        trigger_ee_timer = 1;
                    }
                    ctl_b->ee_info_flag |= ((1<<MD_EE_FLOW_START)|(1<<MD_EE_PENDING_TOO_LONG));
                    spin_unlock_irqrestore(&ctl_b->ctl_lock, flags);
                        
                    if(trigger_ee_timer) {
                        mod_timer(&ctl_b->md_ex_monitor,jiffies+EE_TIMER_BASE+1);
                    }
                    break;
                        
                default:
                    break;
            }
        }
    }
}


void ccif_isr_check(int md_id)
{
    md_ctl_block_t            *ctl_b = md_ctlb[md_id];
    modem_exception_exp_t    *exp_ee_info;
    int                        trigger_ee = 0;
    int                        trigger_ee_timer = 0;
    unsigned long            flags;
    unsigned int            ee_info = 0;
    
    // Check ext exception info
    md_ee_info_check(md_id, &ee_info);

    if (ee_info){
        if(ctl_b->ee_info_got == 0) {
            ctl_b->ee_info_got = 1;
            exp_ee_info = (modem_exception_exp_t*)ctl_b->smem_table->ccci_md_ex_exp_info_smem_base_virt;
            CCCI_MSG_INF(md_id, "ctl", "receive MD_EX @ISR(%08X)(%d:%d)\n", exp_ee_info->exception_occur, \
                            exp_ee_info->send_time, exp_ee_info->wait_time);
            ccci_dump_hw_reg_val(md_id, NULL, 0);
            trigger_ee = 1;
        }
    }
    // Trigger EE if needed
    if(trigger_ee) {
        spin_lock_irqsave(&ctl_b->ctl_lock, flags);
        ctl_b->ee_info_flag |= ((1<<MD_EE_FLOW_START)|(1<<MD_EE_FOUND_BY_ISR) | ee_info);
        if((ctl_b->ee_info_flag & (1<<MD_STATE_UPDATE)) == 0) {
            trigger_ee_timer = 1;
        }
        ctl_b->md_boot_stage = MD_BOOT_STAGE_EXCEPTION;
        spin_unlock_irqrestore(&ctl_b->ctl_lock, flags);
        if(trigger_ee_timer) {
            mod_timer(&ctl_b->md_ex_monitor,jiffies+EE_TIMER_BASE+1);
        }
    }
}



/****************************************************************************/
/* register callback function during modem boot up stage                                               */
/*                                                                                                                           */
/****************************************************************************/
void md_call_chain(MD_CALL_BACK_HEAD_T *head,unsigned long data)
{
    MD_CALL_BACK_QUEUE *queue;
    unsigned long flag;

    spin_lock_irqsave(&head->lock,flag);
    head->is_busy = 1;
    spin_unlock_irqrestore(&head->lock,flag);
    
    queue=head->next;
    while (queue)
    {
        queue->call(queue,data);
        queue=queue->next;
    }

    spin_lock_irqsave(&head->lock,flag);
    head->is_busy = 0;
    spin_unlock_irqrestore(&head->lock,flag);
}

static void notify_chain(unsigned long data)
{
    md_ctl_block_t    *ctl_b = (md_ctl_block_t *)data;
    md_call_chain(&ctl_b->md_notifier,CCCI_MD_STOP);
}


int md_register_call_chain(int md_id, MD_CALL_BACK_QUEUE *queue)
{
    MD_CALL_BACK_HEAD_T *head;
    unsigned long flag;
    int    retry= 10;

    head = &(md_ctlb[md_id]->md_notifier);

    do {
        spin_lock_irqsave(&head->lock,flag);
        if(head->is_busy) {
            spin_unlock_irqrestore(&head->lock,flag);
            msleep(10);
        } else {
            queue->next=head->next;
            head->next=queue;
            spin_unlock_irqrestore(&head->lock,flag);
            return 0;
        }
    }while(retry-->0);

    CCCI_MSG_INF(md_id, "ctl", "md_register_call_chain fail\n");
    return -1;
}


int md_unregister_call_chain(int md_id,MD_CALL_BACK_QUEUE *queue)
{
    unsigned long flag;
    int ret=-1;
    MD_CALL_BACK_HEAD_T *head;
    MD_CALL_BACK_QUEUE **_queue=NULL;
    int retry = 10;

    head = &(md_ctlb[md_id]->md_notifier);

    do {
        spin_lock_irqsave(&head->lock,flag);
        if(head->is_busy) {
            spin_unlock_irqrestore(&head->lock,flag);
            msleep(10);
        } else {
            // Find and remove--
            _queue=&head->next;
            while(*_queue)
            {
                if (*_queue==queue)
                {
                    head->next=(*_queue)->next;
                    *_queue=NULL;
                    ret=0;
                    break;
                }
                _queue=&(*_queue)->next;
            }
            //----------------
            spin_unlock_irqrestore(&head->lock,flag);
            return 0;
        }
    }while(retry-->0);
    CCCI_MSG_INF(md_id, "ctl", "md_unregister_call_chain fail\n");
    return ret;
}



/****************************************************************************/
/* modem boot up function                                                                                         */
/*                                                                                                                           */
/****************************************************************************/
static void md_boot_up_timeout_func(unsigned long data)
{
    md_ctl_block_t    *ctl_b = (md_ctl_block_t *)data;
    int                md_id = ctl_b->m_md_id;
    char ex_info[EE_BUF_LEN]="";
    
    CCCI_MSG_INF(md_id, "ctl", "Time out at md_boot_stage_%d! \n", ctl_b->md_boot_stage);

    if(ctl_b->stop_retry_boot_md)
        return;

    //ccci_system_message(ctl_b->m_md_id, CCCI_MD_MSG_BOOT_TIMEOUT, 0);
    if(ctl_b->md_boot_stage == MD_BOOT_STAGE_0) {
        // Handshake 1 fail, only dump ccif share memory
        snprintf(ex_info, EE_BUF_LEN, "\n[Others] MD_BOOT_UP_FAIL(HS%d)\n", (ctl_b->md_boot_stage+1));
        CCCI_DBG_MSG(md_id, "cci", "Dump MD%d Image memory\n", md_id+1);
        ccci_mem_dump((int*)md_ctlb[md_id]->md_layout->md_region_vir, MD_IMG_DUMP_SIZE);
    #ifdef ENABLE_CCCI_DRV_BUILDIN    
        CCCI_DBG_MSG(md_id, "cci", "Dump TASK_UNINTERRUPTIBLE\n");
        show_state_filter(TASK_UNINTERRUPTIBLE);
    #endif
        ccci_aed(md_id, CCCI_AED_DUMP_CCIF_REG, ex_info);
    } else if(ctl_b->md_boot_stage == MD_BOOT_STAGE_1) {
        #if defined (CONFIG_MTK_AEE_FEATURE) && defined (ENABLE_AEE_MD_EE)
        aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_FTRACE, "CCCI", "modem boot up timeout");
        #endif
        // Handshake 2 fail
        snprintf(ex_info, EE_BUF_LEN, "\n[Others] MD_BOOT_UP_FAIL(HS%d)\n", (ctl_b->md_boot_stage+1));
        CCCI_DBG_MSG(md_id, "cci", "Dump MD%d Image memory\n", md_id+1);
        ccci_mem_dump((int*)md_ctlb[md_id]->md_layout->md_region_vir, MD_IMG_DUMP_SIZE);
    #ifdef ENABLE_CCCI_DRV_BUILDIN    
        CCCI_DBG_MSG(md_id, "cci", "Dump TASK_UNINTERRUPTIBLE\n");
        show_state_filter(TASK_UNINTERRUPTIBLE);
    #endif
        ccci_dump_hw_reg_val(md_id, NULL, 0);
        
        CCCI_DBG_MSG(md_id, "cci", "Dump MD%d Exception share memory\n", md_id+1);
        ccci_mem_dump((int*)md_ctlb[md_id]->smem_table->ccci_exp_smem_base_virt, md_ctlb[md_id]->smem_table->ccci_exp_smem_size);
        
        ccci_aed(md_id, CCCI_AED_DUMP_EX_MEM, ex_info);
    }
}

// set_md_runtime: setup MODEM runtime data
static int set_md_runtime(int md_id, 
                          modem_runtime_t *runtime,
                          modem_runtime_info_tag_t *tag)
{
    int                i;
    struct file        *filp = NULL;
    LOGGING_MODE    mdlog_flag = MODE_IDLE;
    int                ret = 0;
    md_ctl_block_t    *ctl_b;
    int                dl_ctl_mem_size, ul_ctl_mem_size;

    ctl_b = md_ctlb[md_id];
    memset(runtime, 0, sizeof(modem_runtime_t));

    runtime->Prefix = 0x46494343; // "CCIF"
    runtime->Postfix = 0x46494343; // "CCIF"
    runtime->BootChannel = CCCI_CONTROL_RX;
    
    if(ctl_b->smem_table->ccci_sys_smem_size)
    {
        runtime->SysShareMemBase = ctl_b->smem_table->ccci_sys_smem_base_phy - get_md2_ap_phy_addr_fixed();
    runtime->SysShareMemSize = ctl_b->smem_table->ccci_sys_smem_size;
    }
    else
    {
        runtime->SysShareMemBase = 0;
        runtime->SysShareMemSize = 0;
    }

    if(ctl_b->smem_table->ccci_exp_smem_size)
    {
        runtime->ExceShareMemBase = ctl_b->smem_table->ccci_exp_smem_base_phy- get_md2_ap_phy_addr_fixed();
    runtime->ExceShareMemSize = ctl_b->smem_table->ccci_exp_smem_size;
    }
    else
    {
        runtime->ExceShareMemBase = 0;
        runtime->ExceShareMemSize = 0;
    }
    if(ctl_b->smem_table->ccci_mdlog_smem_size)
    {
        runtime->MdlogShareMemBase = ctl_b->smem_table->ccci_mdlog_smem_base_phy- get_md2_ap_phy_addr_fixed();
    runtime->MdlogShareMemSize = ctl_b->smem_table->ccci_mdlog_smem_size;
    }
    else
    {
        runtime->MdlogShareMemBase = 0;
        runtime->MdlogShareMemSize = 0;
    }
    if(ctl_b->smem_table->ccci_pcm_smem_size)
    {
        runtime->PcmShareMemBase = ctl_b->smem_table->ccci_pcm_smem_base_phy- get_md2_ap_phy_addr_fixed();
    runtime->PcmShareMemSize = ctl_b->smem_table->ccci_pcm_smem_size;
    }
    else
    {
        runtime->PcmShareMemBase = 0;
        runtime->PcmShareMemSize = 0;
    }

    if(ctl_b->smem_table->ccci_pmic_smem_size)
    {
        runtime->PmicShareMemBase = ctl_b->smem_table->ccci_pmic_smem_base_phy- get_md2_ap_phy_addr_fixed();
    runtime->PmicShareMemSize = ctl_b->smem_table->ccci_pmic_smem_size;

    }
    else
    {
        runtime->PmicShareMemBase = 0;
        runtime->PmicShareMemSize = 0;
    }

    if(ctl_b->smem_table->ccci_fs_smem_size)
    {
        runtime->FileShareMemBase = ctl_b->smem_table->ccci_fs_smem_base_phy- get_md2_ap_phy_addr_fixed();
    runtime->FileShareMemSize = ctl_b->smem_table->ccci_fs_smem_size;
    }
    else
    {
        runtime->FileShareMemBase = 0;
        runtime->FileShareMemSize = 0;
    }

    if(ctl_b->smem_table->ccci_rpc_smem_size)
    {
        runtime->RpcShareMemBase = ctl_b->smem_table->ccci_rpc_smem_base_phy- get_md2_ap_phy_addr_fixed();
    runtime->RpcShareMemSize = ctl_b->smem_table->ccci_rpc_smem_size;
    }
    else
    {
        runtime->RpcShareMemBase = 0;
        runtime->RpcShareMemSize = 0;
    }

    if(sizeof(CCCI_IPC_BUFFER))
    {
        runtime->IPCShareMemBase = ctl_b->smem_table->ccci_ipc_smem_base_phy+offset_of(CCCI_IPC_MEM,buffer)- get_md2_ap_phy_addr_fixed(); // Note this
    runtime->IPCShareMemSize = sizeof(CCCI_IPC_BUFFER); // Note this
    }
    else
    {
        runtime->IPCShareMemBase = 0;
        runtime->IPCShareMemSize = 0;
    }

    if(sizeof(ipc_ilm_t) * MAX_NUM_IPC_TASKS_MD)
    {
    runtime->IPCMDIlmShareMemBase = (int)ctl_b->smem_table->ccci_ipc_smem_base_phy \
                                        + offset_of(CCCI_IPC_MEM, ilm_md)- get_md2_ap_phy_addr_fixed();
    runtime->IPCMDIlmShareMemSize = sizeof(ipc_ilm_t) * MAX_NUM_IPC_TASKS_MD;

    }
    else
    {
        runtime->IPCMDIlmShareMemBase = 0;
        runtime->IPCMDIlmShareMemSize = 0;
    }

    if(sizeof(ipc_ilm_t) * MAX_NUM_IPC_TASKS_MD)
    {
        runtime->IPCMDIlmShareMemBase = (int)ctl_b->smem_table->ccci_ipc_smem_base_phy \
                                            + offset_of(CCCI_IPC_MEM, ilm_md)- get_md2_ap_phy_addr_fixed();
        runtime->IPCMDIlmShareMemSize = sizeof(ipc_ilm_t) * MAX_NUM_IPC_TASKS_MD;
    }
    else
    {
        runtime->IPCMDIlmShareMemBase = 0;
        runtime->IPCMDIlmShareMemSize = 0;
    }
    for (i = 0; i < CCCI_UART_PORT_NUM; i++) {
        if((ctl_b->smem_table->ccci_uart_smem_base_phy[i] != 0) && 
           (ctl_b->smem_table->ccci_uart_smem_size[i] != 0)) 
        {
            if(ctl_b->smem_table->ccci_uart_smem_size[i])
            {
                runtime->UartShareMemBase[i] = ctl_b->smem_table->ccci_uart_smem_base_phy[i]- get_md2_ap_phy_addr_fixed();
            runtime->UartShareMemSize[i] = ctl_b->smem_table->ccci_uart_smem_size[i];
        }
            else
            {
                runtime->UartShareMemBase[i] = 0;
                runtime->UartShareMemSize[i] = 0;
            }
        }
    }
    runtime->UartPortNum = i;

    for(i = 0; i < NET_PORT_NUM; i++)
    {
        if(ctl_b->smem_table->ccci_ccmni_ctl_smem_base_phy[i] == 0 || 
           ctl_b->smem_table->ccci_ccmni_ctl_smem_size[i] == 0) 
        {
            break;
        }
        if(ccci_get_sub_module_cfg(md_id, "net_dl_ctl", (char*)&dl_ctl_mem_size, sizeof(int)) != sizeof(int)) {
            CCCI_MSG_INF(md_id, "ctl", "Get net_dl_ctl fail\n");
            dl_ctl_mem_size = 0;
        }
        if(ccci_get_sub_module_cfg(md_id, "net_ul_ctl", (char*)&ul_ctl_mem_size, sizeof(int)) != sizeof(int)) {
            CCCI_MSG_INF(md_id, "ctl", "Get net_ul_ctl fail\n");
            dl_ctl_mem_size = 0;
        }
        if(dl_ctl_mem_size)
        {
            runtime->NetDLCtrlShareMemBase[i] = ctl_b->smem_table->ccci_ccmni_ctl_smem_base_phy[i]- get_md2_ap_phy_addr_fixed();
        runtime->NetDLCtrlShareMemSize[i] = dl_ctl_mem_size;
        }
        else
        {
            runtime->NetDLCtrlShareMemBase[i] = 0;
            runtime->NetDLCtrlShareMemSize[i] = 0;
        }

        if(ul_ctl_mem_size)
        {
            runtime->NetULCtrlShareMemBase[i] = ctl_b->smem_table->ccci_ccmni_ctl_smem_base_phy[i] + dl_ctl_mem_size- get_md2_ap_phy_addr_fixed();
        runtime->NetULCtrlShareMemSize[i] = ul_ctl_mem_size;
    }
        else
        {
            runtime->NetULCtrlShareMemBase[i] = 0;
            runtime->NetULCtrlShareMemSize[i] = 0;
        }

    }
    runtime->NetPortNum = i;

    if(ctl_b->smem_table->ccci_ccmni_smem_dl_size)
    {
        runtime->MDDLNetShareMemBase = ctl_b->smem_table->ccci_ccmni_smem_dl_base_phy- get_md2_ap_phy_addr_fixed();
    runtime->MDDLNetShareMemSize = ctl_b->smem_table->ccci_ccmni_smem_dl_size;
    }
    else
    {
        runtime->MDDLNetShareMemBase = 0;
        runtime->MDDLNetShareMemSize = 0;
    }
    if(ctl_b->smem_table->ccci_ccmni_smem_ul_size)
    {
        runtime->MDULNetShareMemBase = ctl_b->smem_table->ccci_ccmni_smem_ul_base_phy- get_md2_ap_phy_addr_fixed();
    runtime->MDULNetShareMemSize = ctl_b->smem_table->ccci_ccmni_smem_ul_size;
    }
    else
    {
        runtime->MDULNetShareMemBase = 0;
        runtime->MDULNetShareMemSize = 0;
    }
    if(ctl_b->smem_table->ccci_md_ex_exp_info_smem_size)
    {
        runtime->MDExExpInfoBase = ctl_b->smem_table->ccci_md_ex_exp_info_smem_base_phy- get_md2_ap_phy_addr_fixed();
    runtime->MDExExpInfoSize = ctl_b->smem_table->ccci_md_ex_exp_info_smem_size;
    }
    else
    {
        runtime->MDExExpInfoBase = 0;
        runtime->MDExExpInfoSize = 0;
    }
    if(ctl_b->smem_table->ccci_md_ex_exp_info_smem_size)
    {
        runtime->MiscInfoBase = ctl_b->smem_table->ccci_misc_info_base_phy- get_md2_ap_phy_addr_fixed();
    runtime->MiscInfoSize = ctl_b->smem_table->ccci_misc_info_size;
    }
    else
    {
        runtime->MiscInfoBase = 0;
        runtime->MiscInfoSize = 0;
    }

    //add a new attribute of mdlogger auto start info to notify md
    filp = filp_open(MDLOGGER_FILE_PATH, O_RDONLY, 0777);
    if (IS_ERR(filp)) {
        CCCI_MSG_INF(md_id, "ctl", "open /data/mdl/mdl_config fail:%ld\n", PTR_ERR(filp));
        filp=NULL;
    }
    else {
        ret = kernel_read(filp, 0, (char*)&mdlog_flag,sizeof(int));    
        if (ret != sizeof(int)) {
            CCCI_MSG_INF(md_id, "ctl", "read /data/mdl/mdl_config fail: %d!\n", ret);
            mdlog_flag = MODE_IDLE;
        }
    }

    if(filp != NULL) {
        //CCCI_MSG_INF("ctl", "close /data/mdl/mdl_config!\n");
        //filp_close(filp, current->files);
        filp_close(filp, NULL);
    }

    if (is_meta_mode() || is_advanced_meta_mode()) 
        runtime->BootingStartID = ((char)mdlog_flag <<8 | META_BOOT_ID);
    else 
        runtime->BootingStartID = ((char)mdlog_flag <<8 | NORMAL_BOOT_ID);

    //CCCI_MSG_INF(md_id, "ctl", "send /data/mdl/mdl_config =%d to modem!\n", mdlog_flag);

    platform_set_runtime_data(md_id, runtime); // Updata platform info and driver version
    config_misc_info(md_id, (unsigned int*)ctl_b->smem_table->ccci_misc_info_base_virt, ctl_b->smem_table->ccci_misc_info_size);

    // Configure runtime tag
    tag->prefix = runtime->Prefix;
    tag->platform_L = runtime->Platform_L;
    tag->platform_H = runtime->Platform_H;
    tag->driver_version = runtime->DriverVersion;
    tag->runtime_data_base = ctl_b->smem_table->ccci_md_runtime_data_smem_base_phy- get_md2_ap_phy_addr_fixed();
    tag->runtime_data_size = sizeof(modem_runtime_t);
    CCCI_MSG_INF(md_id, "ctl", "set runtime data: size=%d!\n", sizeof(modem_runtime_t));
    tag->postfix = runtime->Postfix;

    return 0;
}

int ccci_send_run_time_data(int md_id)
{
    int ret=0;
    md_ctl_block_t                *ctl_b;
    modem_runtime_info_tag_t    runtime_tag;
    ccci_msg_t                    msg;
    modem_runtime_t                *runtime;

    ctl_b = md_ctlb[md_id];
    if(ctl_b == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    }

    /* Set runtime data and echo start-boot command */
    //CCCI_MSG_INF(md_id, "ctl", "set modem runtime\n");
    runtime = (modem_runtime_t*)(ctl_b->smem_table->ccci_md_runtime_data_smem_base_virt);
    ret = set_md_runtime(md_id, runtime, &runtime_tag);
    if (ret < 0) {
        CCCI_MSG_INF(md_id, "ctl", "set MODEM runtime data fail\n");
        return ret;
    }

    //if(ctl_b->is_first_boot) {
        //CCCI_MSG_INF(md_id, "ctl", "dump md%d runtime data\n", md_id+1);
        ccci_dump_runtime_data(md_id, runtime, ctl_b->smem_table);
    //}

    ret = ccci_write_runtime_data(md_id, (unsigned char*)&runtime_tag, sizeof(modem_runtime_info_tag_t) );
    if (ret < 0) {
        CCCI_MSG_INF(md_id, "ctl", "fail to write MODEM runtime data(%d)\n", ret);
        return ret;
    }

    msg.magic = 0xFFFFFFFF;
    msg.id = MD_INIT_START_BOOT;
    msg.channel = CCCI_CONTROL_TX;
    msg.reserved = MD_INIT_CHK_ID;
    md_boot_up_additional_operation(md_id);
    mb();
    
    ret = ccci_message_send(md_id, &msg, 1);
    if (ret != sizeof(msg)) {
        CCCI_MSG_INF(md_id, "ctl", "fail to write CCCI_CONTROL_TX(%d)\n", ret);
        return ret;
    }
    
    if ((get_debug_mode_flag()&(DBG_FLAG_JTAG|DBG_FLAG_DEBUG))==0)
        mod_timer(&ctl_b->md_boot_up_check_timer, jiffies+15*HZ);
    
    CCCI_MSG_INF(md_id, "ctl", "wait for NORMAL_BOOT_ID @ %d\n", get_curr_md_state(md_id));

    //prepare md boot up env, such as set mpu protection
    md_env_setup_before_ready(md_id);

    return 0;
}

int ccci_set_reload_modem(int md_id)
{
    md_ctl_block_t *ctl_b;
    ctl_b = md_ctlb[md_id];
    ctl_b->need_reload_image = 1;
    CCCI_MSG_INF(md_id, "ctl", "md image will be reloaded!\n");
    return 0;
}

//ccci_start_modem: do start modem operation
int ccci_start_modem(int md_id)
{
    md_ctl_block_t *ctl_b;
    int ret = 0;
    char            err_str[256];

    ctl_b = md_ctlb[md_id];
    if(ctl_b == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    }

    if(ctl_b->ipo_h_restore) {
        ctl_b->ipo_h_restore = 0;
        ccci_misc_ipo_h_restore(md_id);
        ccci_load_firmware(md_id, LOAD_ALL_IMG, err_str, 256);
    } else if(ctl_b->need_reload_image) {
        CCCI_MSG_INF(md_id, "ctl", "re-load firmware!\n");
        if((ret = ccci_load_firmware(md_id, RELOAD_ONLY, err_str, 256)) <0) {
            CCCI_MSG_INF(md_id, "ctl", "load firmware fail, so modem boot fail:%d!\n", ret);
            ccci_aed(md_id, 0, err_str);
            return -CCCI_ERR_LOAD_IMG_NOMEM;
        } else {
            //when load firmware successfully, no need to load it again when reset modem
            CCCI_MSG_INF(md_id, "ctl", "load firmware successful!\n");
        }
        ctl_b->need_reload_image = 0;
    }

    ctl_b->ee_info_got = 0;

    md_env_setup_before_boot(md_id);

    //update_active_md_sys_state(md_id, 1);
    ccci_enable_md_intr(md_id);
    
    ret = let_md_go(md_id);
    if(ret == 0) {
        mod_timer(&ctl_b->md_boot_up_check_timer, jiffies+5*HZ);
    } else    {
        CCCI_MSG_INF(md_id, "ctl", "ungate_md fail: %d\n", ret);
    }

    atomic_set(&ctl_b->md_reset_on_going, 0);

    CCCI_MSG_INF(md_id, "ctl", "wait for MD_INIT_START_BOOT\n");
    return 0;
}


int ccci_pre_stop(int md_id)
{
    md_ctl_block_t        *ctl_b;
    int                    ret = 0;

    ctl_b = md_ctlb[md_id];
    if(ctl_b == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    }
    /* prevent another reset modem action from wdt timeout IRQ during modem reset */
    if(atomic_inc_and_test(&ctl_b->md_reset_on_going) > 1){
        CCCI_MSG_INF(md_id, "ctl", "One reset flow is on-going \n");
        return -CCCI_ERR_MD_IN_RESET;
    }

    additional_operation_before_stop_md(md_id);    

    CCCI_MSG_INF(md_id, "ctl", "Now disable CCIF irq\n");

    ccci_disable_md_intr(md_id);

    CCCI_MSG_INF(md_id, "ctl", "CCIF irq disabled\n");
    //set_curr_md_state(md_id, MD_BOOT_STAGE_0);
    ctl_b->md_boot_stage = MD_BOOT_STAGE_0;

    return ret;
}

extern void ccci_md_logger_notify(void);
extern void ccci_fs_resetfifo(int md_id);
int ccci_stop_modem(int md_id, unsigned int timeout)
{
    md_ctl_block_t        *ctl_b;
    int                    ret = 0, i;

    ctl_b = md_ctlb[md_id];

    if(in_irq()) {
        CCCI_MSG_INF(md_id, "ctl", "@I\n");
        // If at ISR, using tasklet do call chain work
        tasklet_schedule(&ctl_b->md_notifier.tasklet);
    } else {
        CCCI_MSG_INF(md_id, "ctl", "@N\n");
        md_call_chain(&ctl_b->md_notifier,CCCI_MD_STOP);
    }
    ccci_md_logger_notify();
    CCCI_MSG_INF(md_id, "ctl", "md power off before\n");

    CCCI_MSG_INF(md_id, "ctl", "stop modem, delete boot up check timer\n");
    del_timer(&ctl_b->md_boot_up_check_timer);
    ccmni_v2_dump(md_id);
    let_md_stop(md_id, timeout);
    for (i = 0; i < NR_CCCI_RESET_USER; i++) {
        ctl_b->reset_sta[i].is_reset = 0;
    }
    md_call_chain(&ctl_b->md_notifier,CCCI_MD_RESET);
    CCCI_MSG_INF(md_id, "ctl", "md power off end\n");
    ccmni_v2_dump(md_id);
    // Reset share memory if needed
    memset( (void*)ctl_b->smem_table->ccci_md_ex_exp_info_smem_base_virt, 0, sizeof(modem_exception_exp_t));
    ccci_fs_resetfifo(md_id);
    ret = logic_layer_reset(md_id);

    return ret;
}


//send_md_reset_notify: send modem reset message to user
int send_md_reset_notify(int md_id)
{
    int ret;
    md_ctl_block_t        *ctl_b;

    ctl_b = md_ctlb[md_id];
    if(ctl_b == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    }

    CCCI_MSG_INF(md_id, "ctl", "send reset modem request message\n");

    ret = ccci_pre_stop(md_id);
    //if( (ret < 0)&&(ret != -CCCI_ERR_MD_IN_RESET) )
    if (ret < 0)
        return ret;
    wake_lock_timeout(&ctl_b->trm_wake_lock, 10*HZ);
    ccci_system_message(md_id, CCCI_MD_MSG_RESET, 0);

    return 0;
}


// send_md_stop_notify
int send_md_stop_notify(int md_id)
{
    int ret;
    CCCI_MSG_INF(md_id, "ctl", "send stop modem request message\n");

    ret = ccci_pre_stop(md_id);
    if(ret == 0) {
        ccci_stop_modem(md_id, 1*1000); // <<<<<<<<< Fix here
    //} else if( (ret < 0)&&(ret != -CCCI_ERR_MD_IN_RESET) )
    } else if (ret < 0)
        return ret;
    ccci_system_message(md_id, CCCI_MD_MSG_STOP_MD_REQUEST, 0);
    return 0;
}


//send_md_start_notify
int send_md_start_notify(int md_id)
{
    md_ctl_block_t        *ctl_b;

    ctl_b = md_ctlb[md_id];
    if(ctl_b == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    }

    CCCI_MSG_INF(md_id, "ctl", "send start modem request message\n");
    ccci_system_message(md_id, CCCI_MD_MSG_START_MD_REQUEST, 0);
    return 0;
}

int send_enter_flight_mode_request(int md_id)
{
    md_ctl_block_t    *ctl_b;
    int             ret;

    ctl_b = md_ctlb[md_id];
    if(ctl_b == NULL) {    
        return -CCCI_ERR_FATAL_ERR;
    }
    if(ctl_b->md_boot_stage != MD_BOOT_STAGE_2){
        return -CCCI_ERR_MD_NOT_READY;
    }

    CCCI_MSG_INF(md_id, "ctl", "send enter flight mode message\n");
    ret = ccci_pre_stop(md_id);
    if(ret == 0) {
        ccci_stop_modem(md_id, 1*1000);
    //} else if( (ret < 0)&&(ret != -CCCI_ERR_MD_IN_RESET) )
    } else if(ret < 0)
        return ret;
    ccci_system_message(md_id, CCCI_MD_MSG_ENTER_FLIGHT_MODE, 0);
    return 0;
}

int send_leave_flight_mode_request(int md_id)
{
    md_ctl_block_t        *ctl_b;

    ctl_b = md_ctlb[md_id];
    if(ctl_b == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    }

    CCCI_MSG_INF(md_id, "ctl", "send leave flight mode message\n");
    ccci_system_message(md_id, CCCI_MD_MSG_LEAVE_FLIGHT_MODE, 0);
    return 0;
}

int send_power_on_md_request(int md_id)
{
    md_ctl_block_t        *ctl_b;

    ctl_b = md_ctlb[md_id];
    if(ctl_b == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    }

    CCCI_MSG_INF(md_id, "ctl", "send power up md message\n");
    ccci_system_message(md_id, CCCI_MD_MSG_POWER_ON_REQUEST, 0);
    return 0;
}

int send_power_down_md_request(int md_id)
{
    md_ctl_block_t        *ctl_b;

    ctl_b = md_ctlb[md_id];
    if(ctl_b == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    }

    CCCI_MSG_INF(md_id, "ctl", "send power down md message\n");
    ccci_system_message(md_id, CCCI_MD_MSG_POWER_DOWN_REQUEST, 0);
    return 0;
}

int send_update_cfg_request(int md_id, unsigned int val)
{
    md_ctl_block_t        *ctl_b;

    ctl_b = md_ctlb[md_id];
    if(ctl_b == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    }

    CCCI_MSG_INF(md_id, "ctl", "send update nvram request 0x%x\n", val);
    ccci_system_message(md_id, CCCI_MD_MSG_CFG_UPDATE, val);
    return 0;
}


// ccci_reset_register: register a user for ccci reset
// @name: user name
// return a handle if success; return negative value if failure
int ccci_reset_register(int md_id, char *name)
{
    int                    handle, i;
    md_ctl_block_t        *ctl_b;

    ctl_b = md_ctlb[md_id];
    if(ctl_b == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    }

    //CCCI_MSG_INF(md_id, "ctl", "Register a reset handle\n");

    if (name == NULL) {
        CCCI_MSG_INF(md_id, "ctl", "[Error]Invalid reset handle name\n");
        return -CCCI_ERR_INVALID_PARAM;
    }

    mutex_lock(&ctl_b->ccci_reset_mutex);
    for (handle = 0; handle < NR_CCCI_RESET_USER; handle++) {
        if (ctl_b->reset_sta[handle].is_allocate == 0) {
            ctl_b->reset_sta[handle].is_allocate = 1;
            break;
        }
    }

    if (handle < NR_CCCI_RESET_USER) {
        ctl_b->reset_sta[handle].is_reset = 0;
        mutex_unlock(&ctl_b->ccci_reset_mutex);

        for (i = 0; i < NR_CCCI_RESET_USER_NAME; i++) {
            if (name[i] == '\0') {
                break;
            } else {
                ctl_b->reset_sta[handle].name[i] = name[i];
            }
        }
        CCCI_MSG_INF(md_id, "ctl", "Register a reset handle by %s(%d)\n", current->comm, handle);
        return handle;
    } 
    else {
        mutex_unlock(&ctl_b->ccci_reset_mutex);
        ASSERT(0);
        return -CCCI_ERR_ASSERT_ERR;
    }
}



// ccci_user_ready_to_reset: ready to reset and request to reset md
// @handle: a user handle gotten from ccci_reset_register()
// return 0 if CCCI is reset; return negative value for failure
int ccci_user_ready_to_reset(int md_id, int handle)
{
    int i;
    int reset_ready = 1;
    md_ctl_block_t        *ctl_b;

    ctl_b = md_ctlb[md_id];
    if(ctl_b == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    }
    if(atomic_read(&ctl_b->md_reset_on_going) == 0){
        CCCI_MSG_INF(md_id, "ctl", "Ignore reset request\n");
        mutex_lock(&ctl_b->ccci_reset_mutex);
        ctl_b->reset_sta[handle].is_allocate = 0;
        ctl_b->reset_sta[handle].is_reset = 0;
        mutex_unlock(&ctl_b->ccci_reset_mutex);
        return 0;
    }
    if (handle >= NR_CCCI_RESET_USER) {
        CCCI_MSG_INF(md_id, "ctl", "reset_request: invalid handle:%d \n", handle);
        return -CCCI_ERR_INVALID_PARAM;
    }

    if (ctl_b->reset_sta[handle].is_allocate == 0) {
        CCCI_MSG_INF(md_id, "ctl", "reset_request: handle(%d) not alloc: alloc=%d \n", 
                handle, ctl_b->reset_sta[handle].is_allocate);
        return -CCCI_ERR_INVALID_PARAM;
    } 
    CCCI_MSG_INF(md_id, "ctl", "%s (%d) call reset request \n",current->comm, handle);

    mutex_lock(&ctl_b->ccci_reset_mutex);
    ctl_b->reset_sta[handle].is_allocate = 0;
    ctl_b->reset_sta[handle].is_reset = 1;
    CCCI_MSG_INF(md_id, "ctl", "Dump not ready list++++\n");
    for (i = 0; i < NR_CCCI_RESET_USER; i++) {
        if (ctl_b->reset_sta[i].is_allocate && (ctl_b->reset_sta[i].is_reset == 0)) {
            reset_ready = 0;
            CCCI_MSG_INF(md_id, "ctl", " ==> %s\n", ctl_b->reset_sta[i].name);
        }
    }
    CCCI_MSG_INF(md_id, "ctl", "Dump not ready list----\n");
    mutex_unlock(&ctl_b->ccci_reset_mutex);

    if (reset_ready == 0) 
        return -CCCI_ERR_RESET_NOT_READY;

    // All service ready, send reset request
    CCCI_MSG_INF(md_id, "ctl", "Reset MD by %s(%d) \n", current->comm, handle);
    ccci_system_message(md_id, CCCI_MD_MSG_READY_TO_RESET, 0);

    return 0;
}

int ccci_trigger_md_assert(int md_id)
{
    ccci_msg_t    msg;
    msg.magic    = 0xFFFFFFFF;
    msg.id        = 0x5A5A5A5A;
    msg.channel    = CCCI_FORCE_ASSERT_CH;
    msg.reserved= 0xA5A5A5A5;
    return ccci_message_send(md_id, &msg, 1);
}

int ccci_force_md_assert(int md_id, char buf[], unsigned int len)
{
    ccci_trigger_md_assert(md_id);
    return 0;
}

// ccci_md_ctrl_cb: CCCI_CONTROL_RX callback function for MODEM
// @buff: pointer to a CCCI buffer
// @private_data: pointer to private data of CCCI_CONTROL_RX
void ccci_md_ctrl_cb(void *private)
{
    logic_channel_info_t    *ch_info = (logic_channel_info_t*)private;
    ccci_msg_t                msg;
    int                        ret;
    md_ctl_block_t            *ctl_b = (md_ctl_block_t *)ch_info->m_owner;
    int                        md_id = ctl_b->m_md_id;
    DEBUG_INFO_T            debug_info;
    unsigned long            flags;
    int                        need_update_state = 0;

    while(get_logic_ch_data(ch_info, &msg)) {
        if (msg.id == MD_INIT_START_BOOT &&
            msg.reserved == MD_INIT_CHK_ID && 
            ctl_b->md_boot_stage == MD_BOOT_STAGE_0) 
        {
            del_timer(&ctl_b->md_boot_up_check_timer);
            CCCI_MSG_INF(md_id, "ctl", "receive MD_INIT_START_BOOT\n");
            //set_curr_md_state(md_id, MD_BOOT_STAGE_1);
            ctl_b->md_boot_stage = MD_BOOT_STAGE_1;

            //md_boot_up_additional_operation(md_id);

            ccci_system_message(md_id, CCCI_MD_MSG_BOOT_UP, 0);
        }
        else if (msg.id == NORMAL_BOOT_ID &&
                ctl_b->md_boot_stage == MD_BOOT_STAGE_1) 
        {
            del_timer(&ctl_b->md_boot_up_check_timer);
            CCCI_MSG_INF(md_id, "ctl", "receive NORMAL_BOOT_ID\n");
            //set_curr_md_state(md_id, MD_BOOT_STAGE_2);
            ctl_b->md_boot_stage = MD_BOOT_STAGE_2;
            md_boot_ready_additional_operation(md_id);

            md_call_chain(&ctl_b->md_notifier,CCCI_MD_BOOTUP);

            ccci_system_message(md_id, CCCI_MD_MSG_BOOT_READY, 0);

        }
        else if (msg.id == MD_EX) 
        {
            del_timer(&ctl_b->md_boot_up_check_timer);
            if (unlikely(msg.reserved != MD_EX_CHK_ID)) 
                CCCI_MSG_INF(md_id, "ctl", "receive invalid MD_EX\n");
            else 
            {
                spin_lock_irqsave(&ctl_b->ctl_lock, flags);
                ctl_b->ee_info_flag |= ((1<<MD_EE_FLOW_START)|(1<<MD_EE_MSG_GET)|(1<<MD_STATE_UPDATE)|\
                                        (1<<MD_EE_TIME_OUT_SET));
                //set_curr_md_state(md_id, MD_BOOT_STAGE_EXCEPTION);
                ctl_b->md_boot_stage = MD_BOOT_STAGE_EXCEPTION;
                ctl_b->need_reload_image = 1;
                spin_unlock_irqrestore(&ctl_b->ctl_lock, flags);
                md_call_chain(&ctl_b->md_notifier,CCCI_MD_EXCEPTION);
                
                //atomic_set(&ctl_b->md_ex, 1);
                mod_timer(&ctl_b->md_ex_monitor,jiffies+EE_TIMER_BASE);
                CCCI_MSG_INF(md_id, "ctl", "receive MD_EX\n");
                //ret = ccci_write(CCCI_CONTROL_TX, buff);
                msg.channel = CCCI_CONTROL_TX;
                ret = ccci_message_send(md_id, &msg, 1);
                if (ret != sizeof(msg) )
                { 
                    CCCI_MSG_INF(md_id, "ctl", "write CCCI_CONTROL_TX fail: %d\n", ret);
                }

                ccci_system_message(md_id, CCCI_MD_MSG_EXCEPTION, 0);
            }
        }
        else if (msg.id == MD_EX_REC_OK) 
        {
            if (unlikely(msg.reserved != MD_EX_REC_OK_CHK_ID)) 
                CCCI_MSG_INF(md_id, "ctl", "receive invalid MD_EX_REC_OK\n");
            else 
            {
                spin_lock_irqsave(&ctl_b->ctl_lock, flags);
                ctl_b->ee_info_flag |= ((1<<MD_EE_FLOW_START)|(1<<MD_EE_OK_MSG_GET));
                if((ctl_b->ee_info_flag & (1<<MD_STATE_UPDATE)) == 0) {
                    ctl_b->ee_info_flag |= (1<<MD_STATE_UPDATE);
                    ctl_b->ee_info_flag &= ~(1<<MD_EE_TIME_OUT_SET);
                    //set_curr_md_state(md_id, MD_BOOT_STAGE_EXCEPTION);
                    ctl_b->md_boot_stage = MD_BOOT_STAGE_EXCEPTION;
                    ctl_b->need_reload_image = 1;
                    need_update_state = 1;
                }
                spin_unlock_irqrestore(&ctl_b->ctl_lock, flags);
                
                if(need_update_state) {
                    md_call_chain(&ctl_b->md_notifier,CCCI_MD_EXCEPTION);
                    del_timer(&ctl_b->md_boot_up_check_timer);
                }
                //atomic_set(&ctl_b->md_ex_ok, 1);
                mod_timer(&ctl_b->md_ex_monitor,jiffies);
            }
        } 
        else if (msg.id == MD_INIT_START_BOOT &&
                 msg.reserved == MD_INIT_CHK_ID && !ctl_b->is_first_boot) 
        {
            /* reset state and notify the user process md_init */
            //set_curr_md_state(md_id, MD_BOOT_STAGE_0);
            ctl_b->md_boot_stage = MD_BOOT_STAGE_0;
            CCCI_MSG_INF(md_id, "ctl", "MD second bootup detected!\n");

            ccci_system_message(md_id, CCCI_MD_MSG_RESET, 0);
        }
        else if (msg.id == MD_EX_RESUME_CHK_ID) 
        {
            md_emi_check(md_id, &msg, &debug_info);
            ccci_ee_info_dump(md_id, &debug_info);
        }
        else if (msg.id == CCCI_DRV_VER_ERROR)
        {
            CCCI_MSG_INF(md_id, "ctl", "AP CCCI driver version mis-match to MD!!\n");
            ctl_b->stop_retry_boot_md = 1;
            ccci_aed(md_id, 0, "AP/MD driver version mis-match\n");
        }
        else 
        {
            CCCI_MSG_INF(md_id, "ctl", "receive unknow data from CCCI_CONTROL_RX = %d\n", msg.id);
        }
    }
}


/****************************************************************************/
/* modem boot up function                                                                                         */
/* return 0 for success; return negative values for failure                                                */
/****************************************************************************/
static int boot_md(int md_id)
{
    int                ret=0;
    md_ctl_block_t    *ctl_b;
    char             err_str[256];

    CCCI_MSG_INF(md_id, "ctl", "boot md%d\n", md_id+1);
    if(md_id >= get_md_sys_max_num()) {
        return -CCCI_ERR_INVALID_PARAM;
    }
    
    ctl_b = md_ctlb[md_id];
    if(ctl_b == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    }

    if (ctl_b->md_boot_stage != MD_BOOT_STAGE_0) {
        CCCI_MSG_INF(md_id, "ctl", "MD has boot up!\n");
        return 0;
    }

    CCCI_MSG_INF(md_id, "ctl", "booting up MODEM: start to load firmware...\n");

    // Step 1, load image
    if(ctl_b->is_first_boot) {
        if((ret = ccci_load_firmware(md_id, LOAD_ALL_IMG, err_str, 256)) <0) {
            CCCI_MSG_INF(md_id, "ctl", "load firmware fail, so modem boot fail!\n");
            ccci_aed(md_id, 0, err_str);
            return ret;
        } else {
            //when load firmware successfully, no need to load it again when reset modem
            ctl_b->is_first_boot = 0;
            CCCI_MSG_INF(md_id, "ctl", "load firmware successful!\n");
        }
    } else {
        CCCI_MSG_INF(md_id, "ctl", "modem&dsp firmware already exist, not load again!\n");
    }

    // Step 2.1, register md control call back function
    ret = register_to_logic_ch(md_id ,CCCI_CONTROL_RX, ccci_md_ctrl_cb, ctl_b);
    if (ret != 0 ) {
        CCCI_MSG_INF(md_id, "ctl", "register CCCI_CONTROL_RX fail\n");
        un_register_to_logic_ch(0, CCCI_CONTROL_TX);
        return ret;
    }

    // Step 2.2, register md control call back function
    ret = register_to_logic_ch(md_id ,CCCI_SYSTEM_RX, ccci_sys_rx_dispatch_cb, NULL);
    if (ret != 0 ) {
        CCCI_MSG_INF(md_id, "ctl", "register CCCI_SYSTEM_RX fail\n");
        un_register_to_logic_ch(0, CCCI_SYSTEM_RX);
        return ret;
    }

    // Step 3, register MD wdt call back.
    ccci_md_wdt_notify_register(md_id, send_md_reset_notify);

    // Step 4, power on modem
    ccci_power_on_md(md_id);

    // Step 5, register md intr
    ccci_hal_irq_register(md_id);

    // step 6, start modem */
    ccci_start_modem(md_id);

    return ret;
}


static ssize_t boot_md_show(char *buf)
{
    int        md_num = get_md_sys_max_num();
    int        i;
    int        curr=0;

    for(i=0; i<md_num; i++){
        if(!get_modem_is_enabled(i))
            continue;
        else
            curr += snprintf(&buf[curr], 128, "md%d:%d\n", (i+1), md_ctlb[i]->md_boot_stage);
    }

    return curr;
}

static ssize_t boot_md_store(const char *buf, size_t count)
{
    md_ctl_block_t    *ctl_b;
    int                md_id;

    if(buf[0] == '0') {
        md_id = 0;
    } else if(buf[0] == '1') {
        md_id = 1;
    } else {
        //md_id = 100;        
        CCCI_MSG("[Error] invalid md sys id: %d\n", buf[0]);
        return 0;
    }

    ctl_b = md_ctlb[md_id];
    if(ctl_b == NULL) {
        CCCI_MSG_INF(md_id, "ctl", "[Error]md ctlb is null\n");
        return 0;
    }

    mutex_lock(&ctl_b->ccci_md_boot_mutex);

    if (ctl_b->md_boot_stage == MD_BOOT_STAGE_0) {
        //CCCI_MSG_INF(md_id, "ctl", "Boot MD sys %d\n", md_id+1);
        boot_md(md_id);
    } else {
        CCCI_MSG_INF(md_id, "ctl", "MD already in boot stage %d\n", ctl_b->md_boot_stage);
    }

    mutex_unlock(&ctl_b->ccci_md_boot_mutex);

    return count;
}


/****************************************************************************/
/* API for IPO feature                                                                                                */
/*                                                                                                                           */
/****************************************************************************/
int ccci_ipo_h_restore(int md_id, char buf[], unsigned int len)
{
    md_ctl_block_t        *ctl_b;

    ctl_b = md_ctlb[md_id];
    if(ctl_b == NULL) {
        return -CCCI_ERR_FATAL_ERR;
    }

    // 1. Restore memory re-mapping/MD env/WDT irq
    if(ccci_ipo_h_platform_restore(md_id) <0 ) {
        printk("[ccci/ctl] IPO-H p1 fail\n");
        return -CCCI_IPO_H_RESTORE_FAIL;
    }
    //md_dsp_wdt_irq_dis(md_id);

    // 2. Restore share memory
    // Clear all share memory to zero
    CCCI_DBG_MSG(md_id, "ctl", "IPO-H: SMEM add:0x%08x, size:%d\n", \
                    ctl_b->smem_table->ccci_smem_vir, ctl_b->smem_table->ccci_smem_size);
    memset((void*)ctl_b->smem_table->ccci_smem_vir, 0, ctl_b->smem_table->ccci_smem_size);
    // 2-1 UART
    if(ccci_uart_ipo_h_restore(md_id)<0)
        return -CCCI_IPO_H_RESTORE_FAIL;
    // 2-2 IPC
    if(ccci_ipc_ipo_h_restore(md_id)<0)
        return -CCCI_IPO_H_RESTORE_FAIL;
    // 2-3 CCMNI
    if(ccmni_ipo_h_restore(md_id)<0)
        return -CCCI_IPO_H_RESTORE_FAIL;

    // 3. Set flag to reload image
    ctl_b->ipo_h_restore = 1;

    // 4. Re-init CCIF
    ccci_hal_reset(md_id);

    // 5. Re register CCIF interrupt
    //ccci_hal_irq_register(md_id);
    //ccci_disable_md_intr(md_id);

    // 6. Power on MD
    ccci_power_on_md(md_id);

    return 0;
    
}


/****************************************************************************/
/* modem control initial function                                                                                 */
/*                                                                                                                           */
/****************************************************************************/
int ccci_md_ctrl_init(int md_id)
{
    int                ret;
    md_ctl_block_t    *ctlb;

    // Allocate md ctrl struct memory
    ctlb = (md_ctl_block_t *)kmalloc(sizeof(md_ctl_block_t), GFP_KERNEL);
    if(ctlb == NULL)
        return -CCCI_ERR_GET_MEM_FAIL;

    // Init control struct
    memset(ctlb, 0, sizeof(md_ctl_block_t));
    mutex_init(&ctlb->ccci_md_boot_mutex);
    mutex_init(&ctlb->ccci_reset_mutex);
    spin_lock_init(&ctlb->md_notifier.lock);
    ctlb->md_notifier.next=NULL;
    tasklet_init(&ctlb->md_notifier.tasklet, notify_chain, (unsigned long)ctlb);

    ctlb->md_boot_stage = MD_BOOT_STAGE_0;
    ctlb->is_first_boot = 1;
    ctlb->md_ex_type = 0;
    ctlb->need_reload_image = 0;
    ctlb->img_inf_ready = 0;
    ctlb->stop_retry_boot_md  = 0;
    ctlb->image_type = 0;
    ctlb->reboot_reason = 0;
    ctlb->ipo_h_restore = 0;
    ctlb->smem_table = get_md_smem_layout(md_id);
    if(ctlb->smem_table == NULL) {
        ret = -CCCI_ERR_GET_SMEM_SETTING_FAIL;
        goto _GET_SMEM_SETTING_FAIL;
    }
    ctlb->md_layout = get_md_sys_layout(md_id);
    if(ctlb->md_layout == NULL) {
        ret = -CCCI_ERR_GET_MEM_LAYOUT_FAIL;
        goto _GET_SMEM_SETTING_FAIL;
    }
    snprintf(ctlb->wakelock_name, sizeof(ctlb->wakelock_name), "ccci%d_trm", (md_id+1));
    wake_lock_init(&ctlb->trm_wake_lock, WAKE_LOCK_SUSPEND, ctlb->wakelock_name);

    // Timer init
    init_timer(&ctlb->md_ex_monitor);
    ctlb->md_ex_monitor.function = ex_monitor_func;
    ctlb->md_ex_monitor.data = (unsigned long)ctlb;
    init_timer(&ctlb->md_boot_up_check_timer);
    ctlb->md_boot_up_check_timer.function = md_boot_up_timeout_func;
    ctlb->md_boot_up_check_timer.data = (unsigned long)ctlb;

    spin_lock_init(&ctlb->ctl_lock);
    ctlb->ee_info_got = 0;
    ctlb->ee_info_flag = 0;

    ctlb->m_md_id = md_id;
    md_ctlb[md_id] = ctlb;

    //register lock/unlock modem dormant mode when AP suspend&resume
    //register_resume_notify(md_id, RSM_ID_MD_LOCK_DORMANT, lock_md_dormant);
    //register_suspend_notify(md_id, SLP_ID_MD_UNLOCK_DORMANT, unlock_md_dormant);
    #ifdef ENABLE_MD_WAKE_UP
    register_resume_notify(md_id, RSM_ID_WAKE_UP_MD, wakeup_md);
    #endif

    //register fast dormancy function as ccci kernel func and suspend callback
    register_ccci_kern_func_by_md_id(md_id, ID_CCCI_DORMANCY, ccci_dormancy);
    register_suspend_notify(md_id, SLP_ID_MD_FAST_DROMANT, md_fast_dormancy);

    //md send msg before it trigger watdog timeout irq
    register_ccci_sys_call_back(md_id, MD_WDT_MONITOR, md_wdt_monitor);

    //send info to md by system channel
    register_sys_msg_notify_func(md_id, send_ccci_system_ch_msg);

    //print the name of get wake up source of CCIF_MD when AP wake up
    register_ccci_kern_func_by_md_id(md_id, ID_GET_MD_WAKEUP_SRC, get_md_wakeup_src);

    //register call back function in isr and no physical channel to check if md ex happens
    bind_to_low_layer_notify(md_id, ccif_isr_check, ccci_check_md_no_physical_channel);

    //md get AP battery voltage by send system rx msg
    register_ccci_sys_call_back(md_id, MD_GET_BATTERY_INFO, md_get_battery_info);

    //md set sim type by send system rx msg
    register_ccci_sys_call_back(md_id, MD_SIM_TYPE, md_set_sim_type);

    // register IPO-H call back
    register_ccci_kern_func_by_md_id(md_id, ID_IPO_H_RESTORE_CB, ccci_ipo_h_restore);

    register_ccci_kern_func_by_md_id(md_id, ID_FORCE_MD_ASSERT, ccci_force_md_assert);

    //Clear all share memory to zero
    memset((void*)ctlb->smem_table->ccci_smem_vir, 0, ctlb->smem_table->ccci_smem_size);

    return 0;

_GET_SMEM_SETTING_FAIL:
    kfree(ctlb);
    return ret;
}


int ccci_md_ctrl_common_init(void)
{
    register_filter_func("-l", ccci_msg_filter_store, ccci_msg_filter_show);
    register_filter_func("-c", ccci_ch_filter_store, ccci_ch_filter_show);
    // MUST register callbacks after memory is allocated
    //boot_register_md_func(boot_md_show, boot_md_store);

    register_ccci_attr_func("boot", boot_md_show, boot_md_store);
    return 0;
}

/*
 * ccci_md_ctrl_exit
 */
void ccci_md_ctrl_exit(int md_id)
{
    md_ctl_block_t    *ctlb = md_ctlb[md_id];
    
    if (ctlb == NULL)
        return;
    wake_lock_destroy(&ctlb->trm_wake_lock);
    del_timer(&ctlb->md_boot_up_check_timer);
    del_timer(&ctlb->md_boot_up_check_timer);
    //ccci_free_smem(md_id);
    tasklet_kill(&ctlb->md_notifier.tasklet);
    kfree(ctlb);
    md_ctlb[md_id] = NULL;
}


