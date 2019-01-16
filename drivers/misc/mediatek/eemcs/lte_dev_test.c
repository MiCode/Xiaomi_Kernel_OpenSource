#include <linux/module.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/version.h>
#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/fs.h> 
#include <linux/file.h> 


#include "eemcs_kal.h"
#include "lte_dev_test.h"
#include "lte_dev_test_at.h"
#include "lte_dev_test_lib.h"


#include "lte_df_main.h"
//#include "lte_dl_main.h"
//#include "lte_dev_sdio_hal.h"
#include "lte_hif_sdio.h"
#include "sdio_reg_fw_side.h"


#define MTK_TEST_LIB
#include "lte_dev_test_lib.h"


#define MAX_DL_PKT_CNT  48
#define HW_MAX_DL_PKT_CNT  64


#define LINUX_3_8_AFTER 0


/*each recv ep has it's own fragment data compare information*/
recv_fragment_ctrl_t recv_frag_ctrl[HIF_MAX_DLQ_NUM];
/*the actually transfer received counter, used in f_compare_fragment_pattern()*/
 volatile unsigned int recv_total_pkt_cnt_agg;

/*total skb count*/
 volatile unsigned int recv_total_pkt_cnt ;
 volatile int recv_th_rslt ;
 volatile unsigned long long recv_total_bytes_cnt;

volatile unsigned int que_recv_pkt_cnt[HIF_MAX_DLQ_NUM] ;

//AUTOEXT volatile lb_data_pattern_e send_pattern = ATCASE_LB_DATA_AUTO;
//AUTOEXT volatile lb_data_pattern_e cmp_pattern = ATCASE_LB_DATA_AUTO;
volatile lb_data_pattern_e send_pattern = ATCASE_LB_DATA_AUTO;
volatile lb_data_pattern_e cmp_pattern = ATCASE_LB_DATA_AUTO;

volatile attest_option_t sdio_test_option;




#define TEST_H2DSM0R	(0x0070)
#define TEST_H2DSM1R	(0x0074)
#define TEST_D2HRM0R	(0x0078)	
#define TEST_D2HRM1R	(0x007C)


////////////////////////////////////////////////////////////////////////////

#define CLI_MAGIC 'L'
#define IOCTL_READ _IOR(CLI_MAGIC, 0, int)
#define IOCTL_WRITE _IOW(CLI_MAGIC, 1, int)


#define BROM_MAGIC 'BROM'
#define BROM_READ _IOR(BROM_MAGIC, 0, int)
#define BROM_WRITE _IOW(BROM_MAGIC, 1, int)
#define BROM_SYNC _IOW(BROM_MAGIC, 2, int)
#define BROM_SYNC_GDB _IOW(BROM_MAGIC, 3, int)


typedef struct
{
	unsigned int data_len;
	char *data_ptr;
} BROM_RW_T;


#define BUF_SIZE 200
#define MAX_ARG_SIZE 10
#define MAX_PARAM_NUM (MAX_ARG_SIZE-1)

////////////////////////////////////////////////////////////////////////////

/* return code */
#define RET_SUCCESS 0
#define RET_FAIL 1
#define RET_BUSY 2

#define WAIT_TIMEOUT   false
#define RETURN_NOW   true


#define MAX_WAIT_COUNT 1000

static at_msg_status    dev_test_at_msg_status;
static athif_cmd_t      *dev_test_athif_cmd_t;
static athif_status_t   *dev_test_athif_result_t;
static athif_status_t   *athif_result_save_t;



unsigned char *buff_kmemory_sent;
unsigned char *buff_kmemory_receive;
unsigned char *buff_kmemory_save_result;

unsigned char *buff_kmemory_ulpkt_data;
unsigned char *buff_kmemory_hwlimit;

unsigned short result_total_len;
unsigned short result_rest_len;

volatile bool testing_swint = false;
volatile unsigned int swint_status_for_test;


extern KAL_UINT32 test_rx_tail_change;
extern KAL_UINT32 test_rx_pkt_cnt_q0;
extern KAL_UINT32 test_rx_pkt_cnt_q1;
extern KAL_UINT32 test_rx_pkt_cnt_q2;
extern KAL_UINT32 test_rx_pkt_cnt_q3;

typedef enum
{
    CCCI_SUCCESS = 0,
    CCCI_FAIL = -EIO,
    CCCI_IN_USE = -EEXIST,
    CCCI_NOT_OWNER = -EPERM,
    CCCI_INVALID_PARAM = -EINVAL,
    CCCI_NO_PHY_CHANNEL = -ENXIO,
    CCCI_IN_INTERRUPT = -EACCES,
    CCCI_IN_IRQ = -EINTR,
    CCCI_MD_NOT_READY = -EBUSY,
    CCCI_RESET_NOT_READY = -ENODEV
}CCCI_RETURNVAL_T;


// static int t_dev_auto(int argc, char** argv);
// static int t_dev_init(int argc, char** argv);
static int t_dev_reset_ulq_dlq(int argc, char** argv);
static int t_dev_auto_regression(int argc, char** argv);

static int t_dev_test_print(int argc, char** argv);
static int t_dev_rw_reg(int argc, char** argv);

static int t_dev_mb_rw_manual(int argc, char** argv);
static int t_dev_at_cmd_manual(int argc, char** argv);
static int t_dev_cisram_rw(int argc, char** argv);
static int t_dev_mb_auto(int argc, char** argv);
static int t_dev_sw_int(int argc, char** argv);
static int t_dev_d2h_normal_op(int argc, char** argv);
static int t_dev_normal_int(int argc, char** argv);
static int t_dev_normal_op_dl(int argc, char** argv);
static int t_dev_txrx_basic(int argc , char **argv);
static int t_dev_dl_basic_trans(int argc, char** argv);
static int t_dev_dl_gpd_ext(int argc, char** argv);
static int t_dev_rx_basic(int argc , char **argv);
static int t_dev_tx_basic(int argc , char **argv);
static int t_dev_tx_multique(int argc , char **argv);
static int t_dev_simple_lb(int argc , char **argv);
static int t_dev_single_queue_lb(int argc , char **argv);
static int t_dev_one_pkt_lb(int argc , char **argv);
static int t_dev_random_lb(int argc , char **argv);


static int t_dev_rx_len_fifo_max(int argc , char **argv);
static int t_dev_single_allow_len(int argc, char** argv);
static int t_dev_bd_allow_len(int argc, char** argv);
static int t_dev_small_pkt_loopback(int argc, char** argv);
static int t_dev_tx_big_packet(int argc, char** argv);
static int t_dev_rx_big_packet(int argc, char** argv);
static int t_dev_misalign_loopback(int argc, char** argv);
static int t_dev_network_loopback(int argc, char** argv);
static int t_dev_rand_enqueue_loopback(int argc, char** argv);


static int t_dev_fw_own_err(int argc, char** argv);
static int t_dev_dl_underflow_err(int argc, char** argv);
static int t_dev_ul_overflow_err(int argc, char** argv);
static int t_dev_ulq_random_stop(int argc, char** argv);
static int t_dev_dlq_random_stop(int argc, char** argv);
static int t_dev_ul_allowlen_error(int argc, char** argv);
static int t_dev_dl_len_error(int argc, char** argv);
static int t_dev_dl_fifolen_overflow_err(int argc, char** argv);
static int t_dev_txrx_cs_err(int argc , char **argv);


static int t_dev_dl_write_timeout(int argc, char** argv);
static int t_dev_dl_read_timeout(int argc, char** argv);



static int t_dev_perf(int argc, char **argv);
static int t_dev_bypass_lb(int argc , char **argv);

static int t_dev_txrx_bypass(int argc, char **argv);

static int t_dev_tcm_lb(int argc , char **argv);
static int t_dev_tcm_misalign_lb(int argc , char **argv);

static int t_dev_atcmd_data_interleave_lb(int argc , char **argv);
static int t_dev_stress_random_lb(int argc , char **argv);

static int t_dev_auto_debug(int argc, char** argv);


//static int t_dev_brom_sync_test(int argc , char **argv);
static int t_dev_brom_lb_test(int argc , char **argv);
static int t_dev_brom_ioctl_test(int argc , char **argv);

static int t_dev_brom_sync_test_new(int argc , char **argv);
static int t_dev_brom_sync_timeout_test(int argc , char **argv);


static int t_dev_brom_sync_test_xboot(int argc , char **argv);
static int brom_sync_xboot_no_timeout(void);
static int brom_sync_gdb_no_timeout(void);


static int t_dev_brom_dl_timeout_test(int argc , char **argv);
static int t_dev_rx_pkt_cnt_change_test(int argc , char **argv);

static int t_dev_test_mode_pattern_test(int argc , char **argv);

static int t_dev_set_max_rx_pkt(int argc , char **argv);
static int t_dev_max_rx_pkt_test(int argc , char **argv);

static int t_dev_set_wd_reset(int argc , char **argv);
static int t_dev_device_self_sleep(int argc , char **argv);
static int t_dev_device_wake_event_test(int argc , char **argv);
static int t_dev_device_set_wake_eint(int argc , char **argv);

static int t_dev_autotest_by_file(int argc , char **argv);
static int t_dev_kal_msec_sleep(int argc , char **argv);
static int t_dev_enable_auto_sleep(int argc , char **argv);
static int t_dev_give_own_back(int argc , char **argv);
static int t_dev_set_abnormal_stall(int argc , char **argv);

static int t_dev_simple_lb_empty_enq(int argc , char **argv);
static int t_dev_exception_dump_test(int argc , char **argv);




typedef struct
{
	char name[256];
	int (*cb_func)(int argc, char** argv);
} CMD_TBL_T;

CMD_TBL_T _arPCmdTbl[] =
{
    {"dev.auto", &t_dev_auto_regression},
//	{"dev.init", &t_dev_init},
    {"dev.test_print", &t_dev_test_print},
    {"dev.rw_reg", &t_dev_rw_reg},   
    {"dev.mb_rw_manual", &t_dev_mb_rw_manual},
    {"dev.at_cmd_manual", &t_dev_at_cmd_manual},
    {"dev.cisram_rw", &t_dev_cisram_rw},
    {"dev.mb_auto", &t_dev_mb_auto},
    {"dev.sw_int", &t_dev_sw_int},
    {"dev.d2h_normal_op", &t_dev_d2h_normal_op},
    {"dev.dev_normal_int", &t_dev_normal_int},
    {"dev.normal_dl_op", &t_dev_normal_op_dl},
    {"dev.len_fifo_max", &t_dev_rx_len_fifo_max},
    {"dev.dl_basic_trans", &t_dev_dl_basic_trans},  
    {"dev.dl_gpd_ext", &t_dev_dl_gpd_ext},  

	{"dev.txrx_basic", &t_dev_txrx_basic},
	{"dev.perf", &t_dev_perf},	

	{"dev.small_lb", &t_dev_small_pkt_loopback},	
    {"dev.single_allowlen", &t_dev_single_allow_len},
	{"dev.bd_allowlen", &t_dev_bd_allow_len},	
	{"dev.rx_basic", &t_dev_rx_basic},
    {"dev.tx_basic", &t_dev_tx_basic},
    {"dev.tx_multique", &t_dev_tx_multique},
    {"dev.simple_lb", &t_dev_simple_lb},
    {"dev.single_que_lb", &t_dev_single_queue_lb},
    {"dev.simple_lb_empty", &t_dev_simple_lb_empty_enq},  
    {"dev.onepkt_lb", &t_dev_one_pkt_lb},
    {"dev.stress_rand_lb", &t_dev_random_lb},
    {"dev.misalign_lb", &t_dev_misalign_loopback},
    {"dev.network_lb", &t_dev_network_loopback},
    {"dev.rand_lb", &t_dev_rand_enqueue_loopback},
    {"dev.tx_bigpkt", &t_dev_tx_big_packet},
    {"dev.rx_bigpkt", &t_dev_rx_big_packet},
    {"dev.fw_own_err", &t_dev_fw_own_err},
    {"dev.ul_overflow_err", &t_dev_ul_overflow_err},
    {"dev.dl_underflow_err", &t_dev_dl_underflow_err},
    {"dev.ul_rand_stop", &t_dev_ulq_random_stop},
    {"dev.dl_rand_stop", &t_dev_dlq_random_stop},
    {"dev.ul_allowlen_err", &t_dev_ul_allowlen_error},
    {"dev.dl_len_err", &t_dev_dl_len_error},
    {"dev.dl_lenfifo_overflow", &t_dev_dl_fifolen_overflow_err},
//	{"dev.rx_done_empty", &t_dev_qmu_rx_done_empty},
//	{"dev.tx_done_empty", &t_dev_qmu_tx_done_empty},
    {"dev.bypass_lb", &t_dev_bypass_lb},
	{"dev.txrx_bypass", &t_dev_txrx_bypass},
//	{"dev.rx_ioc", &t_dev_qmu_rx_ioc},	
//	{"dev.tx_ioc", &t_dev_qmu_tx_ioc},	
//	{"dev.txrx_ioc_stress", &t_dev_qmu_txrx_ioc_stress},	
//	{"dev.txrx_cs_en", &t_dev_qmu_txrx_cs_en},	
	{"dev.txrx_cs_err", &t_dev_txrx_cs_err},	
//	{"dev.txrx_len_err", &t_dev_qmu_txrx_len_err},	
//	{"dev.perf_misalign", &t_dev_perf_misalign},	
//	{"dev.perf_1kbound", &t_dev_perf_1k_boundary},
    {"dev.tcm_lb", &t_dev_tcm_lb},
    {"dev.tcm_misalign_lb", &t_dev_tcm_misalign_lb},
    {"dev.atcmd_data_interleave", &t_dev_atcmd_data_interleave_lb},
    {"dev.stress_random_lb", &t_dev_stress_random_lb},

    {"dev.brom_sync_gdb", &t_dev_brom_sync_test_new},
    {"dev.brom_sync_xboot", &t_dev_brom_sync_test_xboot},
    {"dev.brom_lb", &t_dev_brom_lb_test},
    {"dev.brom_ioctl", &t_dev_brom_ioctl_test},
    {"dev.brom_sync_new", &t_dev_brom_sync_test_new},
    {"dev.brom_sync_timeout", &t_dev_brom_sync_timeout_test},
    {"dev.brom_dl_timeout", &t_dev_brom_dl_timeout_test},
        
    {"dev.dl_pktcnt_test", &t_dev_rx_pkt_cnt_change_test},
    {"dev.test_mode_test", &t_dev_test_mode_pattern_test},
    {"dev.set_rxpkt_len", &t_dev_set_max_rx_pkt},
    {"dev.max_rxpkt_len", &t_dev_max_rx_pkt_test},
//        
    {"dev.wd_reset", &t_dev_set_wd_reset},
    {"dev.self_sleep", &t_dev_device_self_sleep},
    {"dev.wake_evt_test", &t_dev_device_wake_event_test},
    {"dev.set_wake_eint", &t_dev_device_set_wake_eint},

    {"dev.autotest_file", &t_dev_autotest_by_file},
    {"dev.reset_que", &t_dev_reset_ulq_dlq},
    {"dev.delay_msec", &t_dev_kal_msec_sleep},
    {"dev.auto_sleep", &t_dev_enable_auto_sleep},
    {"dev.give_own_back", &t_dev_give_own_back},

    {"dev.except_test", &t_dev_exception_dump_test},
    //{"dev.set_ab_stall", &t_dev_set_abnormal_stall},
	{NULL, NULL},
};



unsigned int str_to_int(char *str)
{
	unsigned int ret_int = 0 , tmp = 0;
	unsigned int idx = 0 , len = 0;

	len = strlen(str);
	//printk(KERN_ERR "[%s] str len = %d \n", __FUNCTION__, len);
	for (idx = 0 ; idx < len ; idx ++) {
		tmp = str[idx] - 0x30;
		if (idx != 0) {
			ret_int = ret_int * 10;
		}
		ret_int += tmp;
	}
	//printk(KERN_ERR "[%s] value = %d \n", __FUNCTION__, ret_int);

	return ret_int;
}


unsigned int str_to_hex(char *str)
{
	unsigned int ret_int = 0 , tmp = 0;
	unsigned int idx = 0 , len = 0;

	len = strlen(str);
	//printk(KERN_ERR "[%s] str len = %d", __FUNCTION__, len);
	for (idx = 0 ; idx < len ; idx ++) {

        if( (str[idx] >= 0x30) && (str[idx] <= 0x39)){
            tmp = str[idx] - 0x30;
        }
        else if( (str[idx] >= 0x41) && (str[idx] <= 0x46)){
            tmp = str[idx] - 0x31;
        }
        else if( (str[idx] >= 0x61) && (str[idx] <= 0x66)){
            tmp = str[idx] - 0x51;
        }
        
        
		if (idx != 0) {
			ret_int = ret_int * 16;
		}
		ret_int += tmp;
	}
	//printk(KERN_ERR "[%s] value = %d", __FUNCTION__, ret_int);

	return ret_int;
}


void int_to_str(int target_int, char *des_str)
{
    sprintf(des_str,"%d", target_int);
}



int test_h2d_mailbox_wr(int index, void *pValue) 								
{																		
		if (index == 0)													
			return sdio_func1_wr(TEST_H2DSM0R, pValue, 4) ;				
		else{															
			return sdio_func1_wr(TEST_H2DSM1R, pValue, 4) ;			
		}																
}

int test_d2h_mailbox_rd(int index, void *pValue) 								
{																		
		if (index == 0)													
			return sdio_func1_rd(TEST_D2HRM0R, pValue, 4) ;				
		else{															
			return sdio_func1_rd(TEST_D2HRM1R, pValue, 4) ;			
		}																
}


int check_mb0_with_timeout_limit(unsigned int check_value, unsigned int timeout)
{
    unsigned int rd_D2HMB0;
    unsigned int rd_count;
    rd_D2HMB0 = 0;
    rd_count = 0;

    test_d2h_mailbox_rd(0, &rd_D2HMB0);
    
    while(rd_D2HMB0 != check_value){
        rd_count++;
        if(rd_count > timeout){
            return RET_FAIL;
        }
        KAL_SLEEP_USEC(1);
        test_d2h_mailbox_rd(0, &rd_D2HMB0);
        //KAL_DBGPRINT(KAL, DBG_TRACE,("mb0 = 0x%08x, check value = 0x%08x \n", rd_D2HMB0, check_value)) ;
    }

    return RET_SUCCESS;
}


int set_mb_stepctrl(unsigned int step)
{
    unsigned int trans_value;

    if(0 == step % 2){
        trans_value = 0xABCDEF00 + step;
    }
    else{
        trans_value = 0xFEDCBA00 + step;
    }
    
    return test_h2d_mailbox_wr(0, &trans_value);
}


int wait_mb_stepctrl(unsigned int step, unsigned int timeout_us)
{
    unsigned int check_val;

    if(0 == step % 2){
        check_val = 0xABCDEF00 + step;
    }
    else{
        check_val = 0xFEDCBA00 + step;
    }

    if (RET_SUCCESS != check_mb0_with_timeout_limit(check_val, timeout_us)){
        // KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] timeout when waiting step ctrl %d !\n", check_val));
        return RET_FAIL;
    }
    else{
        return RET_SUCCESS;
    }
}


static struct mtlte_ttydev lte_test_ttydev ;
static struct tty_driver *lte_testdev_tty_driver ;

#if LINUX_3_8_AFTER
struct tty_port *lte_testdev_tty_port_ptr;
#endif


static void mtlte_dev_test_read_tasklet(unsigned long data);

static DEFINE_MUTEX(open_mutex);

static DECLARE_TASKLET      (testdev_read_tasklet,  	mtlte_dev_test_read_tasklet, 0);

static void mtlte_dev_test_read_tasklet(unsigned long data)
{
	unsigned rx_qno = (MTLTE_DF_RX_QUEUE_TYPE)data ;
	int accept = 0; 
	struct sk_buff *skb = NULL; 
	struct mtlte_ttydev *ttydev = NULL; 

	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;	

	ttydev = &lte_test_ttydev ;

	if (ttydev->tty_ref){

		/* get the new buffer from new skb*/
		do {
			skb = mtlte_df_DL_read_skb_from_swq(rx_qno) ;
	 		if (skb==NULL){
				break ;
	 		}
			mtlte_df_DL_pkt_handle_complete(rx_qno) ;
			if (ttydev->port.count){
				/* keep insert string to tty core */
				accept = tty_insert_flip_string(ttydev->tty_ref, skb->data, skb->len);
#if !defined(FORMAL_RELEASE)				
				//KAL_ASSERT(accept == skb->len) ;
#endif 				
				/* complete insertion, free it */
				dev_kfree_skb(skb) ;			
				/* push the data to user spaces  */		
				tty_flip_buffer_push(ttydev->tty_ref);			
			}else{
				dev_kfree_skb(skb) ;
			}
		}while(1) ;
	}

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	return  ;
}


static void mtlte_dev_test_start_receive_result(void)
{
    unsigned int read_mb_val, H2D_sw_int;
    athif_init_cmd_t *init_cmd;
    int ret = RET_SUCCESS;
    
    
    if(dev_test_at_msg_status.D2H_NEW_MSD_receiving_st == true){
        KAL_DBGPRINT(KAL, DBG_ERROR,("the old result receiving had not end!!\n"));
        ret = RET_FAIL;
    }
    
    if(dev_test_at_msg_status.D2H_NEW_MSG_arrived == true){
        KAL_DBGPRINT(KAL, DBG_ERROR,("the old result had not process by host!!\n"));
        ret = RET_FAIL;
    }

    dev_test_at_msg_status.D2H_NEW_MSD_receiving_st = true;

    test_d2h_mailbox_rd(0, &read_mb_val);
    init_cmd = (athif_init_cmd_t *)(&read_mb_val);

    if('F' == init_cmd->signature[0] && 'D' == init_cmd->signature[1]){
        result_rest_len = init_cmd->length;
        result_total_len = result_rest_len;
    }
    else{
        KAL_DBGPRINT(KAL, DBG_ERROR,("the signature of init result receive is not correct!!\n"));
        result_rest_len = 0;
        result_total_len = 0;
        ret = RET_FAIL;
    }

    
    if (RET_SUCCESS == ret){
        H2D_sw_int = H2D_INT_D2HMB_init_ack;
        sdio_func1_wr(SDIO_IP_WSICR, &H2D_sw_int, 4);
    }
}


static void mtlte_dev_test_cont_receive_result(void)
{
    unsigned int read_mb0_val;
    unsigned int read_mb1_val;
    unsigned int H2D_sw_int;
    int buf_index;

    char *p_result_buf;
    p_result_buf = (char *)dev_test_athif_result_t;

    
    if(dev_test_at_msg_status.D2H_NEW_MSD_receiving_st == false){
        KAL_DBGPRINT(KAL, DBG_ERROR,("the new result is coming with out init!!\n"));
    }
    else
    {
        test_d2h_mailbox_rd(0, &read_mb0_val);
        test_d2h_mailbox_rd(1, &read_mb1_val);
        
        if(result_rest_len == 0)
        {
            KAL_DBGPRINT(KAL, DBG_ERROR, ("No need to continue receive result!! \n\r"));
            KAL_ASSERT(0);
        }   
        else if(result_rest_len > 8)
        {
            buf_index = result_total_len - result_rest_len;
            *(unsigned int *)(p_result_buf + buf_index) = read_mb0_val;
            buf_index += 4;
            *(unsigned int *)(p_result_buf + buf_index) = read_mb1_val;

            result_rest_len -= 8;
            H2D_sw_int = H2D_INT_D2HMB_data_ack;
            sdio_func1_wr(SDIO_IP_WSICR, &H2D_sw_int, 4);
                
        }
        else
        {
            /* length <= 8 means this is the last time of payload transfer*/
            buf_index = result_total_len - result_rest_len;
            *(unsigned int *)(p_result_buf + buf_index) = read_mb0_val;

            if(result_rest_len > 4)
            {
                buf_index += 4;
                *(unsigned int *)(p_result_buf + buf_index) = read_mb1_val;
            }
            result_rest_len = 0;
            H2D_sw_int = H2D_INT_D2HMB_data_ack;
            sdio_func1_wr(SDIO_IP_WSICR, &H2D_sw_int, 4);

            /* All payload is received, alarm process to handle */
            dev_test_at_msg_status.D2H_NEW_MSG_arrived = true;
            dev_test_at_msg_status.D2H_NEW_MSD_receiving_st = false;
            
            
        }
    }

}


static int mtlte_dev_test_read_callback_empty(unsigned int private_data)
{
    // Because we must use code if we want to use the DL queue. 
    // so this callback function is for some queue which may not use callback function. 
    return 0 ;
}

static int mtlte_dev_test_read_callback(unsigned int que_no)
{
    //int ret = RET_SUCCESS;
	unsigned int i=0;
    struct sk_buff *result_ptr = NULL;
     
    if(true == sdio_test_option.auto_receive_pkt){
	    KAL_DBGPRINT(KAL, DBG_INFO,("====> %s, RXQ type = %d\n",KAL_FUNC_NAME,(MTLTE_DF_RX_QUEUE_TYPE)que_no )) ;		

        result_ptr = mtlte_df_DL_read_skb_from_swq(que_no);
        
        /*receiving packets*/
	    while( result_ptr != NULL ){
            mtlte_df_DL_pkt_handle_complete(que_no);
            KAL_DBGPRINT(KAL, DBG_TRACE,("[INFO] : receive pkt from RxQ %d .\n", que_no));

            /*show received pakcet payload if needed*/
            if(true == sdio_test_option.show_dl_content){
                KAL_DBGPRINT(KAL, DBG_ERROR,("Content : "));
                for(i=0; i<result_ptr->len; i++){
                    KAL_DBGPRINT(KAL, DBG_ERROR,("%x ", *(result_ptr->data+i) ));
                }
                KAL_DBGPRINT(KAL, DBG_ERROR,(" \n"));
            }
            KAL_DBGPRINT(KAL, DBG_TRACE,("[%s]:Current received pkt = %d !!! \n", \
                            KAL_FUNC_NAME, recv_total_pkt_cnt)) ;
            
            /*compare received packet payload if needed*/
            if(true == sdio_test_option.exam_dl_content){
                if ( RET_SUCCESS != f_compare_recv_pkt(result_ptr, que_no) ){
                     KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] data compare error at que=%d !!! \n", \
                            KAL_FUNC_NAME, que_no)) ;
                    recv_th_rslt = RET_FAIL;

                    KAL_DBGPRINT(KAL, DBG_ERROR,("Content : "));
                    for(i=0; i<result_ptr->len; i++){
                        KAL_DBGPRINT(KAL, DBG_ERROR,("%x ", *(result_ptr->data+i) ));
                    }
                    KAL_DBGPRINT(KAL, DBG_ERROR,(" \n"));
                
                     
                }
            }

            /* increase the received packet count */
            if(RET_SUCCESS == recv_th_rslt){
                //if(cmp_pattern == ATCASE_LB_DATA_FRAGMENT){
                //    recv_total_pkt_cnt_agg++;
		        //    recv_total_bytes_cnt += result_ptr->len;
                //}else{
                    recv_total_pkt_cnt++;
		            recv_total_bytes_cnt += result_ptr->len;
                    que_recv_pkt_cnt[que_no]++;
                //}
            }
            
            dev_kfree_skb(result_ptr); 
            result_ptr = mtlte_df_DL_read_skb_from_swq(que_no);
	    }

   
        /* check the callback source  */
         /* task is not use now....
        testdev_read_tasklet.data = (MTLTE_DF_RX_QUEUE_TYPE)private_data ;
        tasklet_schedule(&testdev_read_tasklet);
		*/
	    KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
    }
	return 0 ;
}

static int mtlte_dev_test_swint_callback(unsigned int swint_status)
{
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;

    if(testing_swint == true){
        // save the interrupt status for examine & sent it back to device to inform  
        swint_status_for_test = swint_status;
        //test_h2d_mailbox_wr(0, &swint_status);
    }
    else{
        if( (swint_status & 0xF0000000)){
            if(swint_status & D2H_INT_H2DMB_init_ack){
                KAL_DBGPRINT(KAL, DBG_INFO,("[INFO] Recieved the D2H_INT_H2DMB_init_ack!\n"));	
                dev_test_at_msg_status.D2H_INT_H2DMB_init_ack_st = true;
            }
            if(swint_status & D2H_INT_H2DMB_data_ack){
                KAL_DBGPRINT(KAL, DBG_INFO,("[INFO] Recieved the D2H_INT_H2DMB_data_ack!\n"));
                dev_test_at_msg_status.D2H_INT_H2DMB_data_ack_st = true;
            }
            if(swint_status & D2H_INT_D2HMB_init_req){
                KAL_DBGPRINT(KAL, DBG_INFO,("[INFO] Recieved the D2H_INT_D2HMB_init_req!\n"));	
                mtlte_dev_test_start_receive_result();
            }
            if(swint_status & D2H_INT_D2HMB_data_sent){
                KAL_DBGPRINT(KAL, DBG_INFO,("[INFO] Recieved the D2H_INT_D2HMB_data_sent!\n"));
                mtlte_dev_test_cont_receive_result();
            }
            
        }
    }
		
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	return 0 ;
}


static int mtlte_dev_test_write(struct tty_struct *tty, const unsigned char *buf, int len)
{
	//struct sk_buff *skb = NULL; 
	int ret = 0;

	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;	
	if (!len ){
		return 0 ;	
	}

	KAL_RAWPRINT(("[TEST_DEV] write len %d, string %s\r\n", len, buf));
#if 0	
	if (mtlte_df_UL_swq_space(TXQ_Q0)==0){
		return -ENOMEM ;		
	}
	
	if ((skb = dev_alloc_skb(len))==NULL){
		KAL_DBGPRINT(KAL, DBG_WARN,("mtlte_dev_tty_write allocate skb failed\n"));
		return -ENOMEM ;	
	}

	/* fill the data content */
	memcpy(skb_put(skb, len), buf, len) ;
	
	/* always reply we have free space or add ccci_write_space_check */
	ret = mtlte_df_UL_write_skb_to_swq(TXQ_Q0, skb) ;

#else
	ret = CCCI_SUCCESS ;
#endif
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	if (ret == CCCI_SUCCESS)
		return len ;
	else
		return ret ;

	
}

static int mtlte_dev_test_write_room(struct tty_struct *tty)
{
	unsigned int space ;
	/* always reply we have free space or add ccci_write_space_check */
	if (lte_test_ttydev.tty_ref){				
		/* Check the TXQ0 space */
		space =	DEV_MAX_PKT_SIZE ;
		KAL_DBGPRINT(KAL, DBG_INFO,("tty %d write_room space is %d\r\n",tty->index, space)) ;
		return space ;
	}else{
		KAL_DBGPRINT(KAL, DBG_INFO,("tty %d write_room invalid\r\n",tty->index)) ;
		return -ENODEV ;
	}	

    return 0 ;//size;
}

 static int mtlte_dev_test_open(struct tty_struct *tty, struct file *filp)
{
	struct tty_port *port ;
	int ret = 0;
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;	
	KAL_RAWPRINT(("opening test dev tty port %d\n", tty->index)) ;
    
	if (tty->index != LTE_TEST_DEVICE_MINOR){
		KAL_DBGPRINT(KAL, DBG_ERROR,("the test dev tty minor port %d is not supported\n", tty->index)) ;	
		return -ENODEV ;
	}

	mutex_lock(&open_mutex);

	/* check the port has been opened or not */
	port = &(lte_test_ttydev.port) ;
	if (port->count){
		KAL_DBGPRINT(KAL, DBG_ERROR,("the test dev tty minor port %d had been opened\n", tty->index)) ;	
		ret = -EIO ;
		goto OPEN_END ;
	}
	
	set_bit(TTY_NO_WRITE_SPLIT, &tty->flags);
	tty->driver_data = &lte_test_ttydev ;	
	
	/* assign the tty instance to global var for read operation */
	lte_test_ttydev.tty_ref = tty ;
	//kref_get(&ue->kref);
OPEN_END:
	mutex_unlock(&open_mutex);

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	return ret ;
}

static void mtlte_dev_test_close(struct tty_struct *tty, struct file *filp)
{
	struct tty_port *port ;
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;	
	KAL_DBGPRINT(KAL, DBG_INFO,("ready to close the tty minor port %d\n", tty->index)) ;

	if(!tty){
		KAL_DBGPRINT(KAL, DBG_ERROR,("tty is NULL\n"));
		return ;
	}
	
	mutex_lock(&open_mutex);

	/* check the port has been closed or not */
	port = &(lte_test_ttydev.port) ;
	if(port->count == 0){
		KAL_DBGPRINT(KAL, DBG_WARN,("port wasn't opened\n"));
		goto CLOSE_END ;
	}

	/* port closing procedure */
	if(tty_port_close_start(port, tty, filp) == 0){
		KAL_DBGPRINT(KAL, DBG_WARN,("not the last to close...\n"));
		goto CLOSE_END ;
	}
	tty_port_hangup(port);
	tty->driver_data = NULL;	
	//kref_put(&ue->kref, mtlte_destroy);
	tty_port_close_end(port, tty);
	tty_port_tty_set(port, NULL);

CLOSE_END:	
	mutex_unlock(&open_mutex);
	
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	return ;
}

static const struct tty_port_operations mtlte_port_ops = {
};
// We will allocate the 3 tty device to our LTE tty driver here. The dev here should be the SDIO device
int mtlte_dev_test_probe(int index, struct device *dev)
{
	int ret = 0 ;
	struct device *tty_dev ;
	
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;

	if (index!=LTE_TEST_DEVICE_MINOR){
		KAL_DBGPRINT(KAL, DBG_ERROR,("no such tty (%d) device!!", index)) ;
		return -1 ;
	}

	//ccci_register(CCCI_CH_TTY_MODEM+index, mtlte_dev_test_read_callback, NULL) ;

    // Notice : We must register callback function if we want to use this queue
    mtlte_df_register_rx_callback(RXQ_Q0, mtlte_dev_test_read_callback, RXQ_Q0) ;
    mtlte_df_register_rx_callback(RXQ_Q1, mtlte_dev_test_read_callback, RXQ_Q1) ;
    mtlte_df_register_rx_callback(RXQ_Q2, mtlte_dev_test_read_callback, RXQ_Q2) ;
    mtlte_df_register_rx_callback(RXQ_Q3, mtlte_dev_test_read_callback, RXQ_Q3) ;
    
	//mtlte_df_register_rx_callback(RXQ_Q0, mtlte_dev_test_read_callback_empty, RXQ_Q0) ;
    //mtlte_df_register_rx_callback(RXQ_Q1, mtlte_dev_test_read_callback_empty, RXQ_Q1) ;
    //mtlte_df_register_rx_callback(RXQ_Q2, mtlte_dev_test_read_callback_empty, RXQ_Q2) ;
    //mtlte_df_register_rx_callback(RXQ_Q3, mtlte_dev_test_read_callback_empty, RXQ_Q3) ;

    // initial the value of dev_test_at_msg_status
    dev_test_at_msg_status.D2H_INT_H2DMB_data_ack_st= false;
    dev_test_at_msg_status.D2H_INT_H2DMB_init_ack_st= false;
    dev_test_at_msg_status.D2H_NEW_MSD_receiving_st = false;
    dev_test_at_msg_status.D2H_NEW_MSG_arrived = false;
    //register the sw interrupt callback function for auto test
    mtlte_df_register_swint_callback(mtlte_dev_test_swint_callback);

    // initial the at test option
    sdio_test_option.show_dl_content = false;
    sdio_test_option.exam_dl_content = false;
    sdio_test_option.auto_receive_pkt = false;

    //init the memory of at_cmd;
    KAL_ALLOCATE_PHYSICAL_MEM(buff_kmemory_sent, 2048);
    dev_test_athif_cmd_t = (athif_cmd_t *)buff_kmemory_sent;

    KAL_ALLOCATE_PHYSICAL_MEM(buff_kmemory_receive, 2048);
    dev_test_athif_result_t = (athif_status_t *)buff_kmemory_receive;

    KAL_ALLOCATE_PHYSICAL_MEM(buff_kmemory_save_result, 2048);
    athif_result_save_t = (athif_status_t *)buff_kmemory_save_result;

    KAL_ALLOCATE_PHYSICAL_MEM(buff_kmemory_ulpkt_data, 16384);
    KAL_ALLOCATE_PHYSICAL_MEM(buff_kmemory_hwlimit, 458752);

	/* assign the minor to the index and init the port */
	lte_test_ttydev.minor = index ;
	tty_port_init(&lte_test_ttydev.port);	
	lte_test_ttydev.port.ops = &mtlte_port_ops ;

#if LINUX_3_8_AFTER
    lte_testdev_tty_port_ptr = (&lte_test_ttydev.port);
    lte_testdev_tty_driver->ports = &lte_testdev_tty_port_ptr;
#endif



	KAL_DBGPRINT(KAL, DBG_INFO,("minor - %d , dev - 0x%08x, lte_testdev_tty_driver - 0x%08x\r\n", lte_test_ttydev.minor, (unsigned int)dev, (unsigned int)lte_testdev_tty_driver)) ;
	/* register the tty device with this index minor to tty core*/
	tty_dev = tty_register_device(lte_testdev_tty_driver, lte_test_ttydev.minor, dev);		
	if (IS_ERR(tty_dev)) {
		KAL_DBGPRINT(KAL, DBG_ERROR,("tty_register_device ERROR, %ld", IS_ERR(tty_dev))) ;
		ret = PTR_ERR(tty_dev);      
		goto TTY_REG_FAIL;
  	}

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	return 0;

	TTY_REG_FAIL:
	//ccci_unregister(CCCI_CH_TTY_MODEM+index) ;

	return ret;
}

void mtlte_dev_test_detach(int index)
{	
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;

	/* unregister the tty device with this index minor */
	tty_unregister_device(lte_testdev_tty_driver, index);

	//ccci_unregister(CCCI_CH_TTY_MODEM+index) ;
	
	KAL_FREE_PHYSICAL_MEM(buff_kmemory_sent);
    KAL_FREE_PHYSICAL_MEM(buff_kmemory_receive);
    KAL_FREE_PHYSICAL_MEM(buff_kmemory_save_result);

    KAL_FREE_PHYSICAL_MEM(buff_kmemory_ulpkt_data);
    KAL_FREE_PHYSICAL_MEM(buff_kmemory_hwlimit);
	
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
}



////////////////////////////////////////////////////////////////////////////

char w_buf[BUF_SIZE];
char r_buf[BUF_SIZE] = "this is a test";

////////////////////////////////////////////////////////////////////////////

int call_function(char *buf)
{
	int i;
	int argc;
	char *argv[MAX_ARG_SIZE];

	argc = 0;
	do
	{
		argv[argc] = strsep(&buf, " ");
		printk(KERN_DEBUG "[%d] %s\r\n", argc, argv[argc]);
		argc++;
	} while (buf);

	for (i = 0; i < sizeof(_arPCmdTbl)/sizeof(CMD_TBL_T); i++)
	{
		if ((!strcmp(_arPCmdTbl[i].name, argv[0])) && (_arPCmdTbl[i].cb_func != NULL))
			return _arPCmdTbl[i].cb_func(argc, argv);
	}

    printk("[ERR] This test item is not exist!! : %s\r\n", argv[0]);
	return -1;
}


#define BROM_ULQ    0
#define BROM_DLQ    0
#define BROM_DATA_SIZE 2048

char brom_ins_buf[BUF_SIZE];
char brom_io_buf[BROM_DATA_SIZE];
char brom_rd_buf[BROM_DATA_SIZE];

unsigned int brom_rd_buf_head;
unsigned int brom_rd_buf_tail;
struct sk_buff *unfinished_skb = NULL;


int brom_write_pkt(unsigned int ulq_no, unsigned int data_len, char *data_pt)
{
    int ret;
    struct sk_buff *skb = NULL; 
    int timeout;
    
    // check UL buffer space
    while(mtlte_df_UL_swq_space(ulq_no)==0){
        KAL_SLEEP_MSEC(1) ;
        timeout++;
        if(timeout > 1000){
            KAL_DBGPRINT(KAL, DBG_ERROR,("%s : send pkt timeout becaucse no que space!\n", KAL_FUNC_NAME));
	        return RET_FAIL ;	
        }
    }

    if ((skb = dev_alloc_skb(data_len))==NULL){
		KAL_DBGPRINT(KAL, DBG_ERROR,("%s : allocate skb failed\n", KAL_FUNC_NAME));
		return RET_FAIL ;	
	}

    /* fill the data content */
    memcpy(skb_put(skb, data_len), data_pt, data_len) ;
	ret = mtlte_df_UL_write_skb_to_swq(ulq_no, skb);

    if(ret != KAL_SUCCESS){
        KAL_DBGPRINT(KAL, DBG_ERROR,("%s : write skb to sw que failed\n", KAL_FUNC_NAME));
        return RET_FAIL;
	}else{
	    ret = data_len;
	}
    
	return ret;
    
}

int brom_read_pkt(unsigned int dlq_no, unsigned int data_len, char *data_pt)
{
    int ret;
    int timeout;
    int data_back_thistime = 0;
    int remain_skb_len = 0;
    int remain_data_len = 0;
    
    while(data_back_thistime < data_len){
        
        if (NULL == unfinished_skb){

            unfinished_skb = mtlte_df_DL_read_skb_from_swq(dlq_no);
            
            if(NULL == unfinished_skb){  // No any DL pkt anymore.
                return data_back_thistime;
            }

            brom_rd_buf_head = 0;
            brom_rd_buf_tail = unfinished_skb->len;
            mtlte_df_DL_pkt_handle_complete(dlq_no);
            
        }
        else
        {
            remain_skb_len = brom_rd_buf_tail - brom_rd_buf_head;
            remain_data_len = data_len - data_back_thistime;
            
            if( remain_skb_len > remain_data_len){
                memcpy(data_pt, (unfinished_skb->data+brom_rd_buf_head), remain_data_len);
                
                data_back_thistime += remain_data_len;
                brom_rd_buf_head += remain_data_len;
                
                return data_back_thistime;
                
            }else{
                memcpy(data_pt, (unfinished_skb->data+brom_rd_buf_head), remain_skb_len);
                data_back_thistime += remain_skb_len;

                brom_rd_buf_head = 0xFFFFFF;
                brom_rd_buf_tail = 0xFFFFFF;
                dev_kfree_skb(unfinished_skb); 
                unfinished_skb = NULL;
            }
        }      
    }
    
	return data_back_thistime;
    
}

// for old linux kernel, please use this ioctl API
//static int mtlte_dev_test_ioctl(struct tty_struct *tty, struct file *filp, unsigned int cmd, unsigned long arg)

// for new linux kernel, please use this ioctl API
static int mtlte_dev_test_ioctl(struct tty_struct *tty, unsigned int cmd, unsigned long arg)
{

    int len = BUF_SIZE;
    
    BROM_RW_T   *brom_rw_data;
    brom_rw_data = (BROM_RW_T *)brom_ins_buf;

	switch (cmd) {
		case IOCTL_READ:
			copy_to_user((char *) arg, r_buf, len);
			printk(KERN_ERR, "[In kernel module]IOCTL_READ: %s\r\n", r_buf);
			break;
            
		case IOCTL_WRITE:
			copy_from_user(w_buf, (char *) arg, len);
			printk(KERN_ERR, "[In kernel module]IOCTL_WRITE: %s\r\n", w_buf);

            //invoke function
			return call_function(w_buf);
			break;
            
        case BROM_READ:
            copy_from_user(brom_ins_buf, (char *) arg, sizeof(BROM_RW_T));
            printk(KERN_DEBUG "IOCTL_BROM_READ: len=%d, data_ptr=%d \r\n", brom_rw_data->data_len, brom_rw_data->data_ptr);
            
            len = brom_read_pkt(BROM_ULQ, brom_rw_data->data_len, brom_rd_buf);
            if(len > 0){
                copy_to_user(brom_rw_data->data_ptr, brom_rd_buf, len);
            }
            return len;
            break;
            
        case BROM_WRITE:
            copy_from_user(brom_ins_buf, (char *) arg, sizeof(BROM_RW_T));
            printk(KERN_DEBUG "IOCTL_BROM_WRITE: len=%d, data_ptr=%d \r\n", brom_rw_data->data_len, brom_rw_data->data_ptr);

            len = brom_rw_data->data_len;
            copy_from_user(brom_io_buf, brom_rw_data->data_ptr, brom_rw_data->data_len);
            
            return brom_write_pkt(BROM_ULQ, len, brom_io_buf);
            break;

        case BROM_SYNC:
            return brom_sync_xboot_no_timeout();
            break;

        case BROM_SYNC_GDB:
            return brom_sync_gdb_no_timeout();
            break;
		default:
			return -ENOTTY;
	}

	return len;
}


static const struct tty_operations mtlte_testdev_operations = {
	.open =			mtlte_dev_test_open,
	.close =		mtlte_dev_test_close,
	.write =		mtlte_dev_test_write,
	.write_room =	mtlte_dev_test_write_room, 
	.ioctl =        mtlte_dev_test_ioctl,
};


// We will init the TTY driver here
int mtlte_dev_test_drvinit(void)
{
	int ret = 0;
	
	KAL_DBGPRINT(KAL, DBG_ERROR,("====> %s\n",KAL_FUNC_NAME)) ;

	/* allocate the tty driver pointer */
	lte_testdev_tty_driver = alloc_tty_driver(1);
	if (lte_testdev_tty_driver == NULL)
    {
        return -ENOMEM;
    }

	/* fill the parameters of the tty driver pointer */
	lte_testdev_tty_driver->owner = THIS_MODULE;
	lte_testdev_tty_driver->driver_name = "mtlte_testdev_driver";
	/* TTY driver name and instance name */	
	lte_testdev_tty_driver->name = "tty_lte_testdev";
	lte_testdev_tty_driver->major = 0; //auto assign
	/* TTY instance will be started from 0 to LTE_TTY_MAX_DEVNUM */	
	lte_testdev_tty_driver->minor_start = 0;
	lte_testdev_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	lte_testdev_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	/* TTY_DRIVER_REAL_RAW means the data from FW will contain the break character */	
	lte_testdev_tty_driver->flags = TTY_DRIVER_RESET_TERMIOS | TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	lte_testdev_tty_driver->init_termios = tty_std_termios;
	lte_testdev_tty_driver->init_termios.c_cflag = B38400 | CS8 | CREAD | HUPCL | CLOCAL;
	lte_testdev_tty_driver->init_termios.c_lflag = 0 ;
	
	tty_set_operations(lte_testdev_tty_driver, &mtlte_testdev_operations);
	/* register the lte tty driver to tty core*/
	ret = tty_register_driver(lte_testdev_tty_driver);
	if (ret!=0){
		KAL_RAWPRINT(("[TTY_INIT] FAIL, tty_register_driver fail.\n")) ;
		put_tty_driver(lte_testdev_tty_driver);
		return ret;
	}		

	/* assign the mapping table of [tty device index] and the [CCCI channel] */
	lte_test_ttydev.tty_ref = NULL ;
	/* setup all rx tasklet data register the call back function of each tty device minor*/
	/* mapping to RXQ_Q0 */
	
	testdev_read_tasklet.data = 0;			
	
	KAL_DBGPRINT(KAL, DBG_ERROR,("<==== %s\n",KAL_FUNC_NAME)) ;
	
	return ret;
	
}

int mtlte_dev_test_drvdeinit(void)
{	
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;

	/* unregister the tty driver*/
	if (lte_testdev_tty_driver){
		KAL_DBGPRINT(KAL, DBG_INFO,("release the lte_testdev_tty_driver\n")) ;
		tty_unregister_driver(lte_testdev_tty_driver);
		put_tty_driver(lte_testdev_tty_driver);
	}

	lte_test_ttydev.tty_ref = NULL ;
	
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	
	return 0;
	
}

static int mtlte_dev_test_config_atcmd(char* signature, athif_cmd_code_e cmd_code, char* payload, unsigned int payload_len, athif_cmd_t *cmd_t)
{
    cmd_t->signature[0] = signature[0];
    cmd_t->signature[1] = signature[1];
    cmd_t->cmd = (short)cmd_code;
    cmd_t->len = (short)(payload_len+ATHIF_CMD_HD_LEN);
    memcpy(cmd_t->buf, payload, payload_len);

    return 0;
}

static int mtlte_dev_test_add_atcmd_payload(char* payload, unsigned int payload_len, athif_cmd_t *cmd_t)
{

    memcpy( ((char *)cmd_t)+cmd_t->len, payload, payload_len);
    cmd_t->len = cmd_t->len + (short)(payload_len);

    return 0;
}


static int mtlte_dev_test_sent_atcmd(athif_cmd_t *cmd_t)
{
    unsigned int init_sent_mb, H2D_sw_int;
    volatile unsigned int wait_count; 
    int i;
    unsigned char *p_payload;

    init_sent_mb = 0;
    H2D_sw_int = 0;
    wait_count = 0;

    if( dev_test_at_msg_status.D2H_INT_H2DMB_init_ack_st == true || dev_test_at_msg_status.D2H_INT_H2DMB_data_ack_st == true){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] another atcmd is sending!!! \n")) ;
    }
    init_sent_mb |= 'F';
    init_sent_mb |= ('H'<<8);
    init_sent_mb |= (cmd_t->len<<16);

    test_h2d_mailbox_wr(0, &init_sent_mb);

    H2D_sw_int = H2D_INT_H2DMB_init_req;
    sdio_func1_wr(SDIO_IP_WSICR, &H2D_sw_int, 4);

    while( dev_test_at_msg_status.D2H_INT_H2DMB_init_ack_st == false){
        wait_count++;
        KAL_SLEEP_USEC(1);
        if(wait_count > 500){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Device is no response for atcmd init!!! \n")) ;
            return RET_FAIL;
        }
    }
    dev_test_at_msg_status.D2H_INT_H2DMB_init_ack_st = false;
    p_payload = (unsigned char *)cmd_t;

    for(i=0; i<cmd_t->len; i+=8){

        test_h2d_mailbox_wr(0, p_payload + i);
        test_h2d_mailbox_wr(1, p_payload + (i+4));

        H2D_sw_int = H2D_INT_H2DMB_data_sent;
        sdio_func1_wr(SDIO_IP_WSICR, &H2D_sw_int, 4);
        wait_count = 0;

        while( dev_test_at_msg_status.D2H_INT_H2DMB_data_ack_st == false){
            wait_count++;
            KAL_SLEEP_USEC(1); 
            if(wait_count > 500){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Device is no response for atcmd data send!!! \n")) ;
                return RET_FAIL;
            }
        }
        dev_test_at_msg_status.D2H_INT_H2DMB_data_ack_st= false;
    }
    return RET_SUCCESS;
    
}


static int mtlte_dev_test_check_cmd_ack(athif_status_t *result_copy, bool busy_return)
{
    int wait_count;

    if(busy_return == WAIT_TIMEOUT){
        wait_count = 0;
        while(dev_test_at_msg_status.D2H_NEW_MSG_arrived == false){
            wait_count++;
            KAL_SLEEP_MSEC(1) ;
            if(wait_count > MAX_WAIT_COUNT){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Device time-out for cmd ack!!! \n"));
                return RET_FAIL;
            }
        }
    }
    else{
        if(dev_test_at_msg_status.D2H_NEW_MSG_arrived == false){
            return RET_BUSY;
        }
    }

    memcpy(result_copy, dev_test_athif_result_t, dev_test_athif_result_t->len);

    if(dev_test_athif_result_t->status != AT_CMD_ACK_READY_SUCCESS){ 
        KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Device ack this cmd is not success!!! \n"));
        
        dev_test_at_msg_status.D2H_NEW_MSG_arrived = false;
        return RET_FAIL;
    }
    else{
        
        KAL_DBGPRINT(KAL, DBG_INFO,("[INFO] Device ack is totally recieved \n"));
        dev_test_at_msg_status.D2H_NEW_MSG_arrived = false;
        return RET_SUCCESS;
    }
    
}


static int t_dev_reset_ulq_dlq(int argc, char** argv)
{
	int ret = RET_SUCCESS;
    athif_cmd_t cmd;
	unsigned int q_num = 0;

    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();        
   
    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    cmd.cmd = SDIO_AT_RESET_UL_QUEUE;
	cmd.buf[0] = 0xFF; // reset all UL queue
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    

    for (q_num = 0 ; q_num < HIF_MAX_DLQ_NUM ; q_num ++) {
        cmd.cmd = SDIO_AT_RESET_DL_QUEUE;
	    cmd.buf[0] = q_num; 
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    }

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_FREE_ONLY;
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
    cmd.buf[0] = ATHIF_RGPD_BASIC;
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 0;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    KAL_SLEEP_MSEC(1);
    mtlte_hif_sdio_enable_fw_own_back(1);
    return ret;
    
}




#define print_success(_ret, _test_case, _param_no, _idx)  do{  \
    if(_ret != RET_SUCCESS){  \
        KAL_DBGPRINT(KAL, DBG_ERROR,("Regression Test fail at %s \n", _test_case[0])); } \
    else{  \
        KAL_DBGPRINT(KAL, DBG_ERROR,("Regression Test of %s is success \n", _test_case[0])); } \
    if(_param_no > 1){  \
        KAL_DBGPRINT(KAL, DBG_ERROR,("param = ")); \
        for(_idx = 1; _idx < _param_no ; _idx++){  \
            KAL_DBGPRINT(KAL, DBG_ERROR,("%s ", _test_case[_idx])); } \
        KAL_DBGPRINT(KAL, DBG_ERROR,(" \n")); \
    }  \
    if(_ret != RET_SUCCESS){ return _ret; } \
}while(0)



mm_segment_t oldfs; 
struct file *openFile(char *path,int flag,int mode) 
{ 
    struct file *fp; 
     
    fp=filp_open(path, flag, 0); 
    if (fp) return fp; 
    else return NULL; 
} 
     
int readFile(struct file *fp,char *buf,int readlen) 
{ 
    if (fp->f_op && fp->f_op->read) 
        return fp->f_op->read(fp,buf,readlen, &fp->f_pos); 
    else 
        return -1; 
} 
     
int closeFile(struct file *fp) 
{ 
    filp_close(fp,NULL); 
    return 0; 
} 
     
void initKernelEnv(void) 
{ 
     oldfs = get_fs(); 
     set_fs(KERNEL_DS); 
} 


char test_flie_buf[16384]; 
static int t_dev_autotest_by_file(int argc, char** argv)
{ 
    int test_item = 0;
	char *test_cmd[200];
    char test_list_name[100];
    char *buf_ptr;
    int test_ret;
    struct timespec start_t , end_t, diff_t;

    struct file *fp; 
    int ret; 

    strcpy(test_list_name, "/data/");
    strcat(test_list_name, argv[1]);
    
    initKernelEnv(); 
    fp=openFile(test_list_name,O_RDONLY,0); 
    
    if (fp!=NULL) 
    { 
        memset(test_flie_buf,0,16384); 
        if ((ret=readFile(fp,test_flie_buf,16384))>0){
            printk("[INFO] read test file success \n"); 
        }
        else 
            printk("[ERR] input file is too big!!! %d\n",ret); 
        
        closeFile(fp); 
    }
    else
    {
        printk("[ERR] Input file NOT EXIST!!! %d\n",ret); 
        return -1;
    }
    
    set_fs(oldfs);

    
    //at_mtlte_hif_sdio_reset_abnormal();
    buf_ptr = test_flie_buf;
    do
	{ 
		test_cmd[test_item] = strsep(&buf_ptr, "\r");
        if('#' == *test_cmd[test_item]){
            printk("[%s] Test Item ignore (or comment): %s\r\n", __FUNCTION__, test_cmd[test_item]);
        }else{
    		printk("[%s] Now perform Test Item %d : %s\r\n", __FUNCTION__, test_item, test_cmd[test_item]);
            jiffies_to_timespec(jiffies , &start_t);

            test_ret = call_function(test_cmd[test_item]);
            if(test_ret){
                printk("[%s][ERR] Auto Regrassion Test fail at item %d !! \r\n", __FUNCTION__, test_item);
                printk("[%s][ERR] Failed item : %s  \r\n", __FUNCTION__, test_cmd[test_item]);
                return ret;
            }else{
                printk("[%s] Test Sucess at item %d ... \r\n", __FUNCTION__, test_item);
            }
            
            jiffies_to_timespec(jiffies , &end_t);
			diff_t = time_diff(start_t, end_t);
            printk("[%s] This test is use %d seconds... \r\n", __FUNCTION__, diff_t.tv_sec);
            test_item++;
        }
        
        while(1){
            if(buf_ptr){
                if('\r' == *buf_ptr || '\n' == *buf_ptr || ' ' == *buf_ptr)
                    buf_ptr++;
                else if (0 == *buf_ptr)
                    buf_ptr = NULL;
                else
                    break;
            }
            else 
                break;
        }
        
	} while (buf_ptr);

    printk("[%s] ALL Test Items is PASS, Congratulation!! \r\n", __FUNCTION__);
    return 0; 
    
} 

static int t_dev_auto_regression(int argc, char** argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
    char *test_param[MAX_ARG_SIZE];
    char test_param_buff[MAX_ARG_SIZE][40];
    int  test_param_no;
    int  ii, jj, kk =0;

    
    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}


        for (idx = 0; idx < 10; idx ++) {
            test_param[idx] = test_param_buff[idx];
    	}

        //test_param_no = 1;
        //strcpy(test_param_buff[0], "t_dev_cisram_rw");
        //ret = t_dev_cisram_rw(test_param_no, test_param_buff);
        //print_success(ret, test_param, test_param_no, idx);

        // NOTICE: this regression test cannot use to test error because we enable reset abnormal
        at_mtlte_hif_sdio_reset_abnormal_enable(300);


        test_param_no = 2;
        strcpy(test_param_buff[0], "t_dev_mb_auto");
        int_to_str(10, test_param_buff[1]);
        ret = t_dev_mb_auto(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);

        //test_param_no = 1;
        //strcpy(test_param_buff[0], "t_dev_test_mode_pattern_test");
        //ret = t_dev_test_mode_pattern_test(test_param_no, test_param);
        //print_success(ret, test_param, test_param_no, idx);

        test_param_no = 1;
        strcpy(test_param_buff[0], "t_dev_set_max_rx_pkt");
        ret = t_dev_set_max_rx_pkt(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);

        test_param_no = 1;
        strcpy(test_param_buff[0], "t_dev_normal_op_dl");
        ret = t_dev_normal_op_dl(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);

        test_param_no = 1;
        strcpy(test_param_buff[0], "t_dev_rx_len_fifo_max");
        ret = t_dev_rx_len_fifo_max(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);
        

        //test_param_no = 1;
        //strcpy(test_param_buff[0], "t_dev_d2h_normal_op");
        //ret = t_dev_d2h_normal_op(test_param_no, test_param);
        //print_success(ret, test_param, test_param_no, idx);

        //test_param_no = 1;
        //strcpy(test_param_buff[0], "t_dev_normal_int");
        //ret = t_dev_normal_int(test_param_no, test_param);
        //print_success(ret, test_param, test_param_no, idx);

        test_param_no = 5;
        strcpy(test_param_buff[0], "t_dev_txrx_basic");
        int_to_str(0, test_param_buff[1]);
        for(ii=0; ii<HIF_MAX_ULQ_NUM; ii++){       
            int_to_str(ii, test_param_buff[2]);
            int_to_str(40, test_param_buff[3]);
            int_to_str(1515, test_param_buff[4]);
            ret = t_dev_txrx_basic(test_param_no, test_param);
            print_success(ret, test_param, test_param_no, idx);
        }

        int_to_str(1, test_param_buff[1]);
        for(ii=0; ii<HIF_MAX_ULQ_NUM; ii++){       
            int_to_str(ii, test_param_buff[2]);
            int_to_str(40, test_param_buff[3]);
            int_to_str(0, test_param_buff[4]);
            ret = t_dev_txrx_basic(test_param_no, test_param);
            print_success(ret, test_param, test_param_no, idx);
        }

        int_to_str(2, test_param_buff[1]);   
        int_to_str(HIF_MAX_ULQ_NUM-1, test_param_buff[2]);
        int_to_str(300, test_param_buff[3]);
        int_to_str(1515, test_param_buff[4]);
        ret = t_dev_txrx_basic(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);

        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        
        int_to_str(3, test_param_buff[1]);
        for(ii=0; ii<HIF_MAX_DLQ_NUM; ii++){       
            int_to_str(ii, test_param_buff[2]);
            int_to_str(40, test_param_buff[3]);
            int_to_str(1515, test_param_buff[4]);
            ret = t_dev_txrx_basic(test_param_no, test_param);
            print_success(ret, test_param, test_param_no, idx);
        }

        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 3;
        strcpy(test_param_buff[0], "t_dev_tx_basic");
        int_to_str(10, test_param_buff[1]);
        int_to_str(2048, test_param_buff[2]);
        ret = t_dev_tx_basic(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);

        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 3;
        strcpy(test_param_buff[0], "t_dev_tx_multique");
        int_to_str(10, test_param_buff[1]);
        int_to_str(2048, test_param_buff[2]);
        ret = t_dev_tx_multique(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);

        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 2;
        strcpy(test_param_buff[0], "t_dev_rx_basic");
        for(ii=0; ii<=ATCASE_DL_2TBD_EXT_BOUNDARY; ii++){       
            int_to_str(ii, test_param_buff[1]);
            ret = t_dev_rx_basic(test_param_no, test_param);
            print_success(ret, test_param, test_param_no, idx);
        }

        KAL_SLEEP_MSEC(100) ;
        KAL_SLEEP_MSEC(100) ;


        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 5;
        strcpy(test_param_buff[0], "t_dev_simple_lb");
        int_to_str(10, test_param_buff[1]);
        int_to_str(2048, test_param_buff[2]);
        int_to_str(1, test_param_buff[3]);
        int_to_str(16, test_param_buff[4]);
        ret = t_dev_simple_lb(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);



        //2  NOTICE:  The problem of host read 2 256byte block by use byte read with 512 byte (not allow by device IP)
        //2 :              can be solved by "func->card->quirks |= MMC_QUIRK_BLKSZ_FOR_BYTE_MODE;"  at linux kernal bigger than 2.6.34 
        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 2;
        strcpy(test_param_buff[0], "t_dev_small_pkt_loopback");
        for(ii=0; ii<3; ii++){       
            int_to_str(ii, test_param_buff[1]);
            ret = t_dev_small_pkt_loopback(test_param_no, test_param);
            print_success(ret, test_param, test_param_no, idx);
        }


        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 1;
        strcpy(test_param_buff[0], "t_dev_single_allow_len");
        ret = t_dev_single_allow_len(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);

        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 2;
        strcpy(test_param_buff[0], "t_dev_bd_allow_len");
        for(ii=0; ii<=4; ii++){       
            int_to_str(ii, test_param_buff[1]);
            if (ii == 4){
                test_param_no = 3;
                int_to_str(100, test_param_buff[2]);
            }
            ret = t_dev_bd_allow_len(test_param_no, test_param);
            print_success(ret, test_param, test_param_no, idx);
        }

        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 4;
        strcpy(test_param_buff[0], "t_dev_misalign_loopback");
        int_to_str(10, test_param_buff[1]);
        int_to_str(2048, test_param_buff[2]);
        int_to_str(0xFF, test_param_buff[3]);
        ret = t_dev_misalign_loopback(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);

        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 3;
        strcpy(test_param_buff[0], "t_dev_network_loopback");
        int_to_str(10, test_param_buff[1]);
        int_to_str(1600, test_param_buff[2]);
        ret = t_dev_network_loopback(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);

        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 3;
        strcpy(test_param_buff[0], "t_dev_rand_enqueue_loopback");
        int_to_str(10, test_param_buff[1]);
        int_to_str(2048, test_param_buff[2]);
        ret = t_dev_rand_enqueue_loopback(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);

        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 1;
        strcpy(test_param_buff[0], "t_dev_dl_gpd_ext");
        ret = t_dev_dl_gpd_ext(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);

        // TODO: Now Tx bypass is have some problem when Tx_no_header is enable, re-test when designer fix the bug.
        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 5;
        strcpy(test_param_buff[0], "t_dev_txrx_bypass");
        int_to_str(0, test_param_buff[1]);
        int_to_str(10, test_param_buff[2]);
        int_to_str(1, test_param_buff[3]);
        int_to_str(1513, test_param_buff[4]);
        ret = t_dev_txrx_bypass(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);



        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 1;
        strcpy(test_param_buff[0], "t_dev_tx_big_packet");
        ret = t_dev_tx_big_packet(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);

        // TODO: solve the host driver buffer size problem (maybe add a check in hif layer?)
        //ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        //test_param_no = 1;
        //strcpy(test_param_buff[0], "t_dev_rx_big_packet");
        //ret = t_dev_rx_big_packet(test_param_no, test_param);
        //print_success(ret, test_param, test_param_no, idx);

        // TODO: Fix this test on MT6575
        //ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        //test_param_no = 1;
        //strcpy(test_param_buff[0], "t_dev_fw_own_err");
        //ret = t_dev_fw_own_err(test_param_no, test_param);
        //print_success(ret, test_param, test_param_no, idx);

        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 1;
        strcpy(test_param_buff[0], "t_dev_ul_overflow_err");
        ret = t_dev_ul_overflow_err(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);



    /*
        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 1;
        strcpy(test_param_buff[0], "t_dev_dl_underflow_err");
        ret = t_dev_dl_underflow_err(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);

        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 1;
        strcpy(test_param_buff[0], "t_dev_ulq_random_stop");
        ret = t_dev_ulq_random_stop(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);

        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 1;
        strcpy(test_param_buff[0], "t_dev_dlq_random_stop");
        ret = t_dev_dlq_random_stop(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);

        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 1;
        strcpy(test_param_buff[0], "t_dev_dl_write_timeout");
        ret = t_dev_dl_write_timeout(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);

        // TODO: perform it when test case is completed
        //ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        //test_param_no = 1;
        //strcpy(test_param_buff[0], "t_dev_dl_read_timeout");
        //ret = t_dev_dl_read_timeout(test_param_no, test_param);
        //print_success(ret, test_param, test_param_no, idx);

        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 2;
        strcpy(test_param_buff[0], "t_dev_ul_allowlen_error");
        for(ii=0; ii<8; ii++){       
            int_to_str(ii, test_param_buff[1]);
            ret = t_dev_ul_allowlen_error(test_param_no, test_param);
            print_success(ret, test_param, test_param_no, idx);
        }

        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 2;
        strcpy(test_param_buff[0], "t_dev_dl_len_error");
        for(ii=0; ii<4; ii++){       
            int_to_str(ii, test_param_buff[1]);
            ret = t_dev_dl_len_error(test_param_no, test_param);
            print_success(ret, test_param, test_param_no, idx);
        }

        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 1;
        strcpy(test_param_buff[0], "t_dev_dl_fifolen_overflow_err");
        ret = t_dev_dl_fifolen_overflow_err(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);
    */

        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
        test_param_no = 5;
        strcpy(test_param_buff[0], "t_dev_simple_lb");
        int_to_str(10, test_param_buff[1]);
        int_to_str(2048, test_param_buff[2]);
        int_to_str(40, test_param_buff[3]);
        int_to_str(50, test_param_buff[4]);
        ret = t_dev_simple_lb(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);
        
        ret = t_dev_reset_ulq_dlq(test_param_no, test_param);    
        test_param_no = 1;
        strcpy(test_param_buff[0], "t_dev_sw_int");
        ret = t_dev_sw_int(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);

        
        return ret;
}


static int t_dev_auto_debug(int argc, char** argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
    char *test_param[MAX_ARG_SIZE];
    char test_param_buff[MAX_ARG_SIZE][40];
    int  test_param_no;
    int  ii, jj, kk =0;

    
    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    for (idx = 0; idx < 10; idx ++) {
        test_param[idx] = test_param_buff[idx];
	}

    //test_param_no = 1;
    //strcpy(test_param_buff[0], "t_dev_cisram_rw");
    //ret = t_dev_cisram_rw(test_param_no, test_param_buff);
    //print_success(ret, test_param, test_param_no, idx);

    at_mtlte_hif_sdio_reset_abnormal_enable(300);

/*
    test_param_no = 2;
    strcpy(test_param_buff[0], "t_dev_mb_auto");
    int_to_str(10, test_param_buff[1]);
    ret = t_dev_mb_auto(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);

    test_param_no = 1;
    strcpy(test_param_buff[0], "t_dev_normal_op_dl");
    ret = t_dev_normal_op_dl(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);

    test_param_no = 1;
    strcpy(test_param_buff[0], "t_dev_rx_len_fifo_max");
    ret = t_dev_rx_len_fifo_max(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);
    

    //test_param_no = 1;
    //strcpy(test_param_buff[0], "t_dev_d2h_normal_op");
    //ret = t_dev_d2h_normal_op(test_param_no, test_param);
    //print_success(ret, test_param, test_param_no, idx);

    //test_param_no = 1;
    //strcpy(test_param_buff[0], "t_dev_normal_int");
    //ret = t_dev_normal_int(test_param_no, test_param);
    //print_success(ret, test_param, test_param_no, idx);

    test_param_no = 5;
    strcpy(test_param_buff[0], "t_dev_txrx_basic");
    int_to_str(0, test_param_buff[1]);
    for(ii=0; ii<HIF_MAX_ULQ_NUM; ii++){       
        int_to_str(ii, test_param_buff[2]);
        int_to_str(40, test_param_buff[3]);
        int_to_str(1515, test_param_buff[4]);
        ret = t_dev_txrx_basic(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);
    }

    int_to_str(1, test_param_buff[1]);
    for(ii=0; ii<HIF_MAX_ULQ_NUM; ii++){       
        int_to_str(ii, test_param_buff[2]);
        int_to_str(40, test_param_buff[3]);
        int_to_str(0, test_param_buff[4]);
        ret = t_dev_txrx_basic(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);
    }

    int_to_str(2, test_param_buff[1]);   
            int_to_str(HIF_MAX_ULQ_NUM-1, test_param_buff[2]);
            int_to_str(300, test_param_buff[3]);
            int_to_str(1515, test_param_buff[4]);
            ret = t_dev_txrx_basic(test_param_no, test_param);
            print_success(ret, test_param, test_param_no, idx);

    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    
    int_to_str(3, test_param_buff[1]);
    for(ii=0; ii<HIF_MAX_DLQ_NUM; ii++){       
        int_to_str(ii, test_param_buff[2]);
        int_to_str(40, test_param_buff[3]);
        int_to_str(1515, test_param_buff[4]);
        ret = t_dev_txrx_basic(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);
    }

    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);    
    test_param_no = 1;
    strcpy(test_param_buff[0], "t_dev_sw_int");
    ret = t_dev_sw_int(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);


    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 3;
    strcpy(test_param_buff[0], "t_dev_tx_basic");
    int_to_str(10, test_param_buff[1]);
    int_to_str(2048, test_param_buff[2]);
    ret = t_dev_tx_basic(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);
*/
    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 3;
    strcpy(test_param_buff[0], "t_dev_tx_multique");
    int_to_str(10, test_param_buff[1]);
    int_to_str(2048, test_param_buff[2]);
    ret = t_dev_tx_multique(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);

    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 2;
    strcpy(test_param_buff[0], "t_dev_rx_basic");
    for(ii=0; ii<=ATCASE_DL_2TBD_EXT_BOUNDARY; ii++){       
        int_to_str(ii, test_param_buff[1]);
        ret = t_dev_rx_basic(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);
    }

    KAL_SLEEP_MSEC(100) ;
    KAL_SLEEP_MSEC(100) ;


    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 5;
    strcpy(test_param_buff[0], "t_dev_simple_lb");
    int_to_str(10, test_param_buff[1]);
    int_to_str(2048, test_param_buff[2]);
    int_to_str(1, test_param_buff[3]);
    int_to_str(16, test_param_buff[4]);
    ret = t_dev_simple_lb(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);

    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 5;
    strcpy(test_param_buff[0], "t_dev_simple_lb");
    int_to_str(10, test_param_buff[1]);
    int_to_str(2048, test_param_buff[2]);
    int_to_str(40, test_param_buff[3]);
    int_to_str(50, test_param_buff[4]);
    ret = t_dev_simple_lb(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);



/*
    //2  NOTICE:  The problem of host read 2 256byte block by use byte read with 512 byte (not allow by device IP)
    //2 :              can be solved by "func->card->quirks |= MMC_QUIRK_BLKSZ_FOR_BYTE_MODE;"  at linux kernal bigger than 2.6.34 
    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 2;
    strcpy(test_param_buff[0], "t_dev_small_pkt_loopback");
    for(ii=0; ii<3; ii++){       
        int_to_str(ii, test_param_buff[1]);
        ret = t_dev_small_pkt_loopback(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);
    }


    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 1;
    strcpy(test_param_buff[0], "t_dev_single_allow_len");
    ret = t_dev_single_allow_len(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);

    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 2;
    strcpy(test_param_buff[0], "t_dev_bd_allow_len");
    for(ii=0; ii<=4; ii++){       
        int_to_str(ii, test_param_buff[1]);
        if (ii == 4){
            test_param_no = 3;
            int_to_str(100, test_param_buff[2]);
        }
        ret = t_dev_bd_allow_len(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);
    }

    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 4;
    strcpy(test_param_buff[0], "t_dev_misalign_loopback");
    int_to_str(10, test_param_buff[1]);
    int_to_str(2048, test_param_buff[2]);
    int_to_str(0xFF, test_param_buff[3]);
    ret = t_dev_misalign_loopback(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);

    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 3;
    strcpy(test_param_buff[0], "t_dev_network_loopback");
    int_to_str(10, test_param_buff[1]);
    int_to_str(1600, test_param_buff[2]);
    ret = t_dev_network_loopback(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);

    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 3;
    strcpy(test_param_buff[0], "t_dev_rand_enqueue_loopback");
    int_to_str(10, test_param_buff[1]);
    int_to_str(2048, test_param_buff[2]);
    ret = t_dev_rand_enqueue_loopback(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);

    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 1;
    strcpy(test_param_buff[0], "t_dev_dl_gpd_ext");
    ret = t_dev_dl_gpd_ext(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);

    // TODO: Now Tx bypass is have some problem when Tx_no_header is enable, re-test when designer fix the bug.
    //ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    //test_param_no = 5;
    //strcpy(test_param_buff[0], "t_dev_txrx_bypass");
    //int_to_str(0, test_param_buff[1]);
    //int_to_str(10, test_param_buff[2]);
    //int_to_str(1, test_param_buff[3]);
    //int_to_str(1513, test_param_buff[4]);
    //ret = t_dev_txrx_bypass(test_param_no, test_param);
    //print_success(ret, test_param, test_param_no, idx);



    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 1;
    strcpy(test_param_buff[0], "t_dev_tx_big_packet");
    ret = t_dev_tx_big_packet(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);

    // TODO: solve the host driver buffer size problem (maybe add a check in hif layer?)
    //ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    //test_param_no = 1;
    //strcpy(test_param_buff[0], "t_dev_rx_big_packet");
    //ret = t_dev_rx_big_packet(test_param_no, test_param);
    //print_success(ret, test_param, test_param_no, idx);

    // TODO: Fix this test on MT6575
    //ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    //test_param_no = 1;
    //strcpy(test_param_buff[0], "t_dev_fw_own_err");
    //ret = t_dev_fw_own_err(test_param_no, test_param);
    //print_success(ret, test_param, test_param_no, idx);

    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 1;
    strcpy(test_param_buff[0], "t_dev_ul_overflow_err");
    ret = t_dev_ul_overflow_err(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);
*/


/*
    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 1;
    strcpy(test_param_buff[0], "t_dev_dl_underflow_err");
    ret = t_dev_dl_underflow_err(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);

    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 1;
    strcpy(test_param_buff[0], "t_dev_ulq_random_stop");
    ret = t_dev_ulq_random_stop(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);

    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 1;
    strcpy(test_param_buff[0], "t_dev_dlq_random_stop");
    ret = t_dev_dlq_random_stop(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);

    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 1;
    strcpy(test_param_buff[0], "t_dev_dl_write_timeout");
    ret = t_dev_dl_write_timeout(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);

    // TODO: perform it when test case is completed
    //ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    //test_param_no = 1;
    //strcpy(test_param_buff[0], "t_dev_dl_read_timeout");
    //ret = t_dev_dl_read_timeout(test_param_no, test_param);
    //print_success(ret, test_param, test_param_no, idx);

    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 2;
    strcpy(test_param_buff[0], "t_dev_ul_allowlen_error");
    for(ii=0; ii<8; ii++){       
        int_to_str(ii, test_param_buff[1]);
        ret = t_dev_ul_allowlen_error(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);
    }

    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 2;
    strcpy(test_param_buff[0], "t_dev_dl_len_error");
    for(ii=0; ii<4; ii++){       
        int_to_str(ii, test_param_buff[1]);
        ret = t_dev_dl_len_error(test_param_no, test_param);
        print_success(ret, test_param, test_param_no, idx);
    }

    ret = t_dev_reset_ulq_dlq(test_param_no, test_param);
    test_param_no = 1;
    strcpy(test_param_buff[0], "t_dev_dl_fifolen_overflow_err");
    ret = t_dev_dl_fifolen_overflow_err(test_param_no, test_param);
    print_success(ret, test_param, test_param_no, idx);
*/
    
    return ret;
}


static int t_dev_test_print(int argc, char** argv)
{
    int ii;
    for (ii = 0; ii < sizeof(_arPCmdTbl)/sizeof(CMD_TBL_T); ii++)
    {
    	if ((!strcmp(_arPCmdTbl[ii].name, argv[1])) && (_arPCmdTbl[ii].cb_func != NULL))
    		return _arPCmdTbl[ii].cb_func(argc-1, &(argv[1]));
    }
}


static int t_dev_mb_rw_manual(int argc, char** argv)
{
    unsigned int mb_num, rw_val, rw_val2;

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    mb_num = str_to_int(argv[2]);

    if( 0 == strcmp("w", argv[1]) ){
        rw_val = str_to_int(argv[3]);
        test_h2d_mailbox_wr(mb_num, &rw_val);
        KAL_DBGPRINT(KAL, DBG_INFO,("the write value = %d \n", rw_val)) ;
    }
    else if( 0 == strcmp("r", argv[1]) ){
        test_d2h_mailbox_rd(mb_num, &rw_val);
        KAL_DBGPRINT(KAL, DBG_INFO,("the read value = %d \n", rw_val)) ;
    }
    else if( 0 == strcmp("2w", argv[1]) ){
        rw_val = str_to_int(argv[2]);
        rw_val2 = str_to_int(argv[3]);
        test_h2d_mailbox_wr(0, &rw_val); 
        test_h2d_mailbox_wr(1, &rw_val2);
        KAL_DBGPRINT(KAL, DBG_INFO,("mb0 write value = %d \n", rw_val)) ;
        KAL_DBGPRINT(KAL, DBG_INFO,("mb1 write value = %d \n", rw_val2)) ;
    }
    else{
        KAL_DBGPRINT(KAL, DBG_WARN,("[WARN] Not read or write!!! \n")) ;
        return RET_FAIL;
    }
    
    return RET_SUCCESS;

}



static int t_dev_cisram_rw(int argc, char** argv)
{
    athif_cmd_t test_cmd;  
    unsigned char *temp_mem;
    athif_mem_tst_cfg_t *rw_arg;
    unsigned char   orig_cis[256];
    //unsigned char   test_cis[256];
    unsigned char   read_cis[128];
    unsigned int    i, idx, cisram_no;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    
    KAL_ALLOCATE_PHYSICAL_MEM(temp_mem, 512);
    rw_arg = (athif_mem_tst_cfg_t *)temp_mem;

    for(cisram_no=0; cisram_no<2; cisram_no++){
           
        rw_arg->len = 256;
        if(0 == cisram_no){rw_arg->mem_addr = SDIO_CIS0_FW_ADDR;}
        else{rw_arg->mem_addr = SDIO_CIS1_FW_ADDR;}
        
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_READ_MEM, (char *)rw_arg, sizeof(athif_mem_tst_cfg_t), &test_cmd);
        mtlte_dev_test_sent_atcmd(&test_cmd);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
        memcpy(orig_cis, athif_result_save_t->buf, 256);

        for(i=0; i<256; i++){
            if(i%4 == 0 || i%4 == 1){ rw_arg->mem_val[i] = 0x5A;}
            else{rw_arg->mem_val[i] = 0x0;}
        }

        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_WRITE_MEM, (char *)rw_arg, sizeof(athif_mem_tst_cfg_t)+256, &test_cmd);
        mtlte_dev_test_sent_atcmd(&test_cmd);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_READ_MEM, (char *)rw_arg, sizeof(athif_mem_tst_cfg_t), &test_cmd);
        mtlte_dev_test_sent_atcmd(&test_cmd);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        if (0 != memcmp(rw_arg->mem_val, athif_result_save_t->buf, 256) ){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR]  set CISRAM fail !!! \n",KAL_FUNC_NAME)) ;
            return RET_FAIL;
        }

        if(0 == cisram_no){ sdio_func0_rd(SDIO_FN0_CCCR_CIS0, read_cis, 128); }
        else{ sdio_func0_rd(SDIO_FN0_CCCR_CIS1, read_cis, 128); }

        for(i=0; i<128; i++){
            if(0x5A != read_cis[i]){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR]  The CISRAM read back val wrong  !!! \n",KAL_FUNC_NAME)) ;
                return RET_FAIL;
            }
        }

        memcpy(rw_arg->mem_val, orig_cis, 256);
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_WRITE_MEM, (char *)rw_arg, sizeof(athif_mem_tst_cfg_t)+256, &test_cmd);
        mtlte_dev_test_sent_atcmd(&test_cmd);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        
        if(0 == cisram_no){ sdio_func0_rd(SDIO_FN0_CCCR_CIS0, read_cis, 128); }
        else{ sdio_func0_rd(SDIO_FN0_CCCR_CIS1, read_cis, 128); }

        for(i=0; i<256; i++){
            if(i%4 == 0 || i%4 == 1){
                idx = (i/2) + (i%4);
                if(read_cis[idx] != orig_cis[i]){
                    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR]  The CISRAM recover error !!! \n",KAL_FUNC_NAME));
                    return RET_FAIL;
                }
            }
        }
    }
    
    KAL_FREE_PHYSICAL_MEM(temp_mem);
    
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
    return RET_SUCCESS;
}



static int t_dev_rw_reg(int argc, char** argv)
{

	unsigned int idx = 0 , param_num = 0;
    unsigned int read_rev;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	//int ret = RET_SUCCESS;

    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
        if( idx<2 ){ arg[idx-1] = str_to_int(argv[idx]); }
        else {arg[idx-1] = str_to_hex(argv[idx]);}
        
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	};

    if( 0 == strcmp("r", argv[1]) ){
        if(arg[1] == 9999){
            sdio_func1_rd(SDIO_IP_WHISR, buff_kmemory_ulpkt_data, sizeof(sdio_whisr_enhance)) ;
        }
        else{
            sdio_func1_rd(arg[1], &read_rev, 4);
            KAL_DBGPRINT(KAL, DBG_ERROR,("[WARN] read %x, value = 0x%x!!! \n",arg[1],read_rev)) ;
        }
    }
    else if( 0 == strcmp("w", argv[1]) ){
        sdio_func1_wr(arg[1], &arg[2], 4);
        KAL_DBGPRINT(KAL, DBG_ERROR,("[WARN] write %x for value 0x%x!!! \n",arg[1],arg[2])) ;
    }
    else{
        KAL_DBGPRINT(KAL, DBG_ERROR,("[WARN] Not read or write!!! \n")) ;
        return RET_FAIL;
    }

    // enable the fw own back function after test is over
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
    
    return RET_SUCCESS;

} 



static int t_dev_at_cmd_manual(int argc, char** argv)
{

    athif_cmd_t test_cmd;   
    char      *rev_result;
    int       payload_len;
    int       i;

    payload_len = 0;
    // get driver before start sent cmd
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, SDIO_AT_CMD_LOOPBACK, argv[1], strlen(argv[1]), &test_cmd);

    mtlte_dev_test_sent_atcmd(&test_cmd);

    mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT);
    rev_result = athif_result_save_t->buf;

    KAL_DBGPRINT(KAL, DBG_WARN,("[Result] the payload of ACK = ")) ;
    for(i=0; i<athif_result_save_t->len-ATHIF_STATUS_HD_LEN; i++){
        KAL_DBGPRINT(KAL, DBG_WARN,("%c", rev_result[i])) ;
    }
    KAL_DBGPRINT(KAL, DBG_WARN,(" \n")) ;

    // enable the fw own back function after test is over
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
    
    return RET_SUCCESS;

} 

static int t_dev_mb_auto(int argc, char** argv)
{
    
    int i,rd_count;
    //unsigned int test_D2HMB2;
    unsigned short lb_times;
    unsigned int test_val_1, test_val_2;
    unsigned int old_val_MB0, old_val_MB1;
    unsigned int rd_D2HMB0, rd_D2HMB1;
    unsigned int wr_D2HMB0, wr_D2HMB1;

    rd_count = 0;

    lb_times =     str_to_int(argv[1]) +2;
    //test_D2HMB2 =  str_to_int(argv[2]);

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();


    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, SDIO_AT_MB_LOOPBACK, (char *)&lb_times, 3, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);


    wr_D2HMB0 = 0xABCDABCD;
    test_h2d_mailbox_wr(0, &wr_D2HMB0);
    
    if( RET_FAIL == check_mb0_with_timeout_limit(0xABCDABCD, 1000) ){
        return RET_FAIL;
    }

    test_d2h_mailbox_rd(0, &rd_D2HMB0);
    test_d2h_mailbox_rd(1, &rd_D2HMB1);
    old_val_MB0 = rd_D2HMB0;
    old_val_MB1 = rd_D2HMB1;

    for(i=0; i<lb_times; i++)
    {
        if(i==0){
            test_val_1 = 0;
            test_val_2 = 0;
        }
        else if(i==1){
            test_val_1 = 0xFFFFFFFF;
            test_val_2 = 0xFFFFFFFF;
        }
        else if(i==2){
            test_val_1 = 0x00030000;
            test_val_2 = 0x033F0000;
        }
        else{
            get_random_bytes(&test_val_1, sizeof(unsigned int)) ;
            get_random_bytes(&test_val_2, sizeof(unsigned int)) ;
        }
        wr_D2HMB0 = test_val_1 ;
        wr_D2HMB1 = test_val_2 ;

        test_h2d_mailbox_wr(1, &wr_D2HMB1);
        test_h2d_mailbox_wr(0, &wr_D2HMB0);
        KAL_DBGPRINT(KAL, DBG_TRACE,("the MB1 write value = 0x%08x \n", wr_D2HMB1)) ;
        KAL_DBGPRINT(KAL, DBG_TRACE,("the MB0 write value = 0x%08x \n", wr_D2HMB0)) ;
        
        rd_count = 0;
        while(old_val_MB0 == rd_D2HMB0)
        {
            rd_count++;
            if(rd_count > 1000){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] mailbox no renew!!! \n")) ;
                return RET_FAIL;
            }
            KAL_SLEEP_USEC(1);
            test_d2h_mailbox_rd(0, &rd_D2HMB0);
            test_d2h_mailbox_rd(1, &rd_D2HMB1);
        }

        if(rd_D2HMB0 != wr_D2HMB0 || rd_D2HMB1 != wr_D2HMB1){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] The loopbacked value is wrong at %d times!!! \n", i)) ;
            return RET_FAIL;
        }
        old_val_MB0 = rd_D2HMB0;
        old_val_MB1 = rd_D2HMB1;

    }

    wr_D2HMB0 = 0xEDEDEDED;
    test_h2d_mailbox_wr(0, &wr_D2HMB0);

    if( RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT)){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Device response it is error!!! \n")) ;
        return RET_FAIL;
    }
    else{
        KAL_DBGPRINT(KAL, DBG_WARN,("[PASS] The final test result is PASS!!! \n")) ;
    }   
    
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
    return RET_SUCCESS;

}

#define MAX_AVAIL_H2D_SWINT 31
#define MIN_AVAIL_H2D_SWINT 16
#define rand_test_times     100

static int t_dev_sw_int(int argc, char** argv)
{
    unsigned short  direction;
    unsigned int    H2D_sw_int, i, rd_count, D2H_mask;
    unsigned int    orig_D2H_mask;
    unsigned int    rd_D2HMB0, wr_D2HMB0;
    unsigned int    old_swint_status;
    unsigned short  rand_num;   
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    /*  **************** */
    /*    H2D SW int test    */
    /*  **************** */
    
    direction = 1;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, SDIO_AT_SW_INT, (char *)&direction, 2, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);

    /* wait device to turn normal ISR to SW interrupt test ISR */ 


    if( RET_FAIL == check_mb0_with_timeout_limit(0x0123BBAA, 1000) ){
        KAL_DBGPRINT(KAL, DBG_ERROR,("%s :[ERR] Device no response of H2D sw int init!!! \n", KAL_FUNC_NAME));
        return RET_FAIL;
    }

    /* test H2D SW interrupt in order */ 
    for(i=MIN_AVAIL_H2D_SWINT; i<=MAX_AVAIL_H2D_SWINT; i++){
        H2D_sw_int = 0x1 <<(i);
        sdio_func1_wr(SDIO_IP_WSICR, &H2D_sw_int, 4);

        if( RET_FAIL == check_mb0_with_timeout_limit(H2D_sw_int, 1000) ){
            KAL_DBGPRINT(KAL, DBG_ERROR,("%s :[ERR] Device response wrong in H2D sw int test!!! \n", KAL_FUNC_NAME));
            return RET_FAIL;
        }
    }

    /* test H2D SW interrupt randomly */ 
    for(i=0; i<rand_test_times; i++ ){
        get_random_bytes(&rand_num, sizeof(unsigned short)) ;

        H2D_sw_int = rand_num<<MIN_AVAIL_H2D_SWINT;
        sdio_func1_wr(SDIO_IP_WSICR, &H2D_sw_int, 4);

        if( RET_FAIL == check_mb0_with_timeout_limit(H2D_sw_int, 1000) ){
            KAL_DBGPRINT(KAL, DBG_ERROR,("%s :[ERR] Device response wrong in H2D sw int random test !!! \n", KAL_FUNC_NAME));
            return RET_FAIL;
        }
    }

    /* inform device and wait device to mask interrupt for test */ 
    wr_D2HMB0 = 0x0123AABB;
    test_h2d_mailbox_wr(0, &wr_D2HMB0);

    if( RET_FAIL == check_mb0_with_timeout_limit(0x0123DDCC, 1000) ){
        KAL_DBGPRINT(KAL, DBG_ERROR,("%s :[ERR] Device no response in set mask of H2D SW int !!! \n", KAL_FUNC_NAME));
        return RET_FAIL;
    }

    /* test H2D SW interrupt with mask */ 
    for(i=MIN_AVAIL_H2D_SWINT; i<=MAX_AVAIL_H2D_SWINT; i++){
        H2D_sw_int = 0x1 <<(i);
        sdio_func1_wr(SDIO_IP_WSICR, &H2D_sw_int, 4);

        KAL_SLEEP_USEC(5);
    }

    wr_D2HMB0 = 0x0123CCDD;
    test_h2d_mailbox_wr(0, &wr_D2HMB0);

    if( RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT)){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Device response H2D SW int test fail!!! \n")) ;
        return RET_FAIL;
    }

    /*  **************** */
    /*    D2H SW int test    */
    /*  **************** */
    direction = 2;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, SDIO_AT_SW_INT, (char *)&direction, 2, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);

    testing_swint = true;
    swint_status_for_test = 0;
    old_swint_status = 0;

    /* inform device to start test D2H SW int */ 
    wr_D2HMB0 = 0x0123EFEF;
    test_h2d_mailbox_wr(0, &wr_D2HMB0);

    rd_D2HMB0 = 0;
    rd_count = 0;

    test_d2h_mailbox_rd(0, &rd_D2HMB0);
    
    while(rd_D2HMB0 != 0x0123BBAA){
        rd_count++;
        if(rd_count > 500){
            KAL_DBGPRINT(KAL, DBG_ERROR,("%s :[ERR] Device no response in testing D2H SW int !!! \n", KAL_FUNC_NAME));
            return RET_FAIL;
        }
        
        if(old_swint_status != swint_status_for_test){
            old_swint_status = swint_status_for_test;
            test_h2d_mailbox_wr(0, &old_swint_status);
        }
        
        KAL_SLEEP_USEC(1);
        test_d2h_mailbox_rd(0, &rd_D2HMB0);
    }

    
    /* set SW D2H interrupt mask to off */
    swint_status_for_test = 0;
    sdio_func1_rd(SDIO_IP_WHIER, &D2H_mask, 4);
    orig_D2H_mask = D2H_mask;
    D2H_mask = D2H_mask & 0x000000FF;
    sdio_func1_wr(SDIO_IP_WHIER, &D2H_mask, 4);

    /* inform device mask setting is completed */ 
    wr_D2HMB0 = 0x0123AABB;
    test_h2d_mailbox_wr(0, &wr_D2HMB0);

    /* wait device set D2H SW interrupt finished*/
    if( RET_FAIL == check_mb0_with_timeout_limit(0x0123DDCC, 1000) ){
        KAL_DBGPRINT(KAL, DBG_ERROR,("%s :[ERR] Device no response in testing mask D2H SW int  !!! \n", KAL_FUNC_NAME));
        return RET_FAIL;
    }

    /* detect the test result */
    if(swint_status_for_test != 0){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Host recieve D2H SW int when masking!!! \n")) ;
        return RET_FAIL;
    }

    /* turn on SW D2H interrupt */
    swint_status_for_test = 0;
    sdio_func1_rd(SDIO_IP_WHIER, &D2H_mask, 4);
    D2H_mask = D2H_mask | 0xFFFFFF00;
    sdio_func1_wr(SDIO_IP_WHIER, &D2H_mask, 4);

    rd_count = 0;
    while(0 == swint_status_for_test){
        rd_count++;
        if(rd_count > 100){
            KAL_DBGPRINT(KAL, DBG_ERROR,("%s :[ERR] the interrupt not occur after mask is removed !!! \n", KAL_FUNC_NAME));
            return RET_FAIL;
        }
        KAL_SLEEP_USEC(1);
    }

    wr_D2HMB0 = 0x0123CCDD;
    test_h2d_mailbox_wr(0, &wr_D2HMB0);
    testing_swint = false;


    if( RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT)){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Device response H2D SW int test fail!!! \n")) ;
        return RET_FAIL;
    }

    sdio_func1_wr(SDIO_IP_WHIER, &orig_D2H_mask, 4);

    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
    return RET_SUCCESS;
}


static int t_dev_d2h_normal_op(int argc, char** argv)
{
    athif_stopq_tst_cfg_t stopq_parm;
    athif_test_param_t    attest_param;
    unsigned short        i;
    unsigned char         rand_num;
    unsigned int          int_mask, wr_D2HMB0, rd_count; 

    unsigned int step;
    athif_sdio_set_ulq_count_t ulq_count_set;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    stopq_parm.t_tick = 0;
    stopq_parm.is_tx = 0;
    stopq_parm.q_num= 0xFFFF;
    
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_STOPQ_TIME, (char *)&stopq_parm, sizeof(athif_stopq_tst_cfg_t), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);

    if( RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT)){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Device response H2D SW int test fail!!! \n",KAL_FUNC_NAME)) ;
        return RET_FAIL;
    }

    attest_param = at_mtlte_hif_get_test_param();
    attest_param.testing_ulq_count = 1;
    attest_param.testing_dlq_int = 0;
    attest_param.int_indicator = 0;
    for(i=0; i<8; i++){
        attest_param.received_ulq_count[i] = 0;
    }  
    at_mtlte_hif_set_test_param(attest_param, set_all);


        /* turn on Tx_Done  interrupt */
    sdio_func1_rd(SDIO_IP_WHIER, &int_mask, 4);
    int_mask = int_mask | 0x00000001;
    sdio_func1_wr(SDIO_IP_WHIER, &int_mask, 4);

    //step 1 : test one time count set & Tx_Done int
    for(i=0; i<100; i++){
        
        step = 1;
        ulq_count_set.q_num = i%8;
        get_random_bytes(&rand_num, sizeof(unsigned char)) ;
        ulq_count_set.gpd_num = 0x000000FF & rand_num;
        if(ulq_count_set.gpd_num == 0){
            ulq_count_set.gpd_num = 1;
        }
 
        
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, SDIO_AT_D2H_NORMAL_OP, (char *)&step, sizeof(unsigned int), dev_test_athif_cmd_t);
        mtlte_dev_test_add_atcmd_payload((char *)&ulq_count_set, sizeof(athif_sdio_set_ulq_count_t), dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);


        attest_param.int_indicator = 0;
        at_mtlte_hif_set_test_param(attest_param, set_int_indicator);

        wr_D2HMB0 = 0xABCDEF00;;
        test_h2d_mailbox_wr(0, &wr_D2HMB0);
        
        rd_count = 0;
        while(attest_param.int_indicator == 0){
            rd_count++;
            if(rd_count > 1000){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Device no interrupt !!! \n",KAL_FUNC_NAME)) ;
                return RET_FAIL;
            }
            KAL_SLEEP_USEC(100);
            attest_param = at_mtlte_hif_get_test_param();
        }

        if(attest_param.received_ulq_count[ulq_count_set.q_num] != ulq_count_set.gpd_num){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] TxQ count test fail, queue num = %d , expect val = %d, real val = %d. \n",
                                                       KAL_FUNC_NAME, ulq_count_set.q_num, ulq_count_set.gpd_num, attest_param.received_ulq_count[ulq_count_set.q_num])) ;
            return RET_FAIL;  
        }

        attest_param.received_ulq_count[ulq_count_set.q_num] = 0;
        at_mtlte_hif_set_test_param(attest_param, set_received_ulq_count);
        wr_D2HMB0 = 0xFEDCBA00;
        test_h2d_mailbox_wr(0, &wr_D2HMB0);

        if( RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT)){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Device response fail in testing D2H normal int !!! \n",KAL_FUNC_NAME)) ;
            return RET_FAIL;
        }


        
        
    }

        /* turn off Tx_Done  interrupt */
    sdio_func1_rd(SDIO_IP_WHIER, &int_mask, 4);
    int_mask = int_mask & 0xFFFFFFFE;
    sdio_func1_wr(SDIO_IP_WHIER, &int_mask, 4);

    //step 2 : test interrupt enable & accumulate count set
    for(i=0; i<100; i++){
        
        step = 2;
        ulq_count_set.q_num = i%8;
        get_random_bytes(&rand_num, sizeof(unsigned char)) ;
        ulq_count_set.gpd_num = 0x000000FF & rand_num;
        if(ulq_count_set.gpd_num == 0){
            ulq_count_set.gpd_num = 1;
        }
 
        
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, SDIO_AT_D2H_NORMAL_OP, (char *)&step, sizeof(unsigned int), dev_test_athif_cmd_t);
        mtlte_dev_test_add_atcmd_payload((char *)&ulq_count_set, sizeof(athif_sdio_set_ulq_count_t), dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);


        if( RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT)){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Device response fail in testing D2H normal int @ step 2!!! \n",KAL_FUNC_NAME)) ;
            return RET_FAIL;
        }

        attest_param = at_mtlte_hif_get_test_param();
        if(attest_param.received_ulq_count[ulq_count_set.q_num] != ulq_count_set.gpd_num){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] TxQ count test fail @ step 2, queue num = %d , expect val = %d, real val = %d. \n",
                                                       KAL_FUNC_NAME, ulq_count_set.q_num, ulq_count_set.gpd_num, attest_param.received_ulq_count[ulq_count_set.q_num])) ;
            return RET_FAIL;           
        }

        attest_param.received_ulq_count[ulq_count_set.q_num] = 0;
        at_mtlte_hif_set_test_param(attest_param, set_received_ulq_count);
        
    }

    // step 3 :test accumlate ability bigger than 255
    // this function has no be implement 
    /*
    for(i=0; i<100; i++){
        
        step = 3;
        ulq_count_set.q_num = i%8;
        get_random_bytes(&rand_num, sizeof(unsigned short)) ;
        ulq_count_set.gpd_num = (0x000006FF & rand_num) + 255;
        
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, SDIO_AT_D2H_NORMAL_OP, (char *)&step, sizeof(unsigned int), dev_test_athif_cmd_t);
        mtlte_dev_test_add_atcmd_payload((char *)&ulq_count_set, sizeof(athif_sdio_set_ulq_count_t), dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);


        if( RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT)){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Device response fail in testing D2H normal int @ step 3!!! \n",KAL_FUNC_NAME)) ;
            return RET_FAIL;
        }

        attest_param = at_mtlte_hif_get_test_param();
        over_count = 0;
        
        while(attest_param.received_ulq_count[ulq_count_set.q_num] != ulq_count_set.gpd_num){
            mtlte_df_UL_write_skb_to_swq(TXQ_Q0, NULL);
            KAL_SLEEP_USEC(1);
            over_count++;
            if(over_count > 8){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] TxQ count test fail @ step 3, queue num = %d , expect val = %d, real val = %d. \n",
                                                       KAL_FUNC_NAME, ulq_count_set.q_num, ulq_count_set.gpd_num, attest_param.received_ulq_count[ulq_count_set.q_num])) ;
                return RET_FAIL; 
            }
        }
        
        KAL_DBGPRINT(KAL, DBG_INFO,("[%s]:[INFO] success at %d times @ step 3, queue num = %d , real val = %d. \n",
                                                       KAL_FUNC_NAME, i, ulq_count_set.q_num, attest_param.received_ulq_count[ulq_count_set.q_num])) ;
        attest_param.received_ulq_count[ulq_count_set.q_num] = 0;
        at_mtlte_hif_set_test_param(attest_param, set_received_ulq_count);
        
    }
    */

        //step 4 : test simutanous read write tx count
    for(i=0; i<100; i++){
        
        step = 4;
        ulq_count_set.q_num = i%8;
        ulq_count_set.gpd_num = 1;
        
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, SDIO_AT_D2H_NORMAL_OP, (char *)&step, sizeof(unsigned int), dev_test_athif_cmd_t);
        mtlte_dev_test_add_atcmd_payload((char *)&ulq_count_set, sizeof(athif_sdio_set_ulq_count_t), dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);

        rd_count = 0;
        while (RET_BUSY == mtlte_dev_test_check_cmd_ack(athif_result_save_t, RETURN_NOW)){
            mtlte_df_UL_write_skb_to_swq(TXQ_Q0, NULL);
            KAL_SLEEP_USEC(1);
            rd_count++;
            if(rd_count > 1000){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Device no response !!! \n",KAL_FUNC_NAME)) ;
                return RET_FAIL;
            }
        }    


        attest_param = at_mtlte_hif_get_test_param();
        if(attest_param.received_ulq_count[ulq_count_set.q_num] != 255){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] TxQ count test fail @ step 4, queue num = %d , expect val = %d, real val = %d. \n",
                                                       KAL_FUNC_NAME, ulq_count_set.q_num, 255, attest_param.received_ulq_count[ulq_count_set.q_num])) ;
            return RET_FAIL;           
        }

        attest_param.received_ulq_count[ulq_count_set.q_num] = 0;
        at_mtlte_hif_set_test_param(attest_param, set_received_ulq_count);
        
    }

    attest_param.testing_ulq_count = 0;
    at_mtlte_hif_set_test_param(attest_param, set_testing_ulq_count);
   
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
    return RET_SUCCESS;
    
}
    

static int t_dev_txrx_basic(int argc , char **argv)
{
	unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int testmode = 0, pkt_num = 0, test_que_no = 0 , pkt_sz = 0, pkt_mode = 0;
	unsigned int rand_num = 0;
    unsigned int max_que_no = 0;
	athif_dl_tgpd_cfg_t dl_cfg;

    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    
    send_pattern = ATCASE_LB_DATA_INC;

	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

	memset(&dl_cfg , 0 ,sizeof(athif_dl_tgpd_cfg_t));
	testmode = arg[0];
	switch (testmode) {
		case ATCASE_UL_BASIC_SEND:
			test_que_no = arg[1];
			pkt_num = arg[2];
			pkt_sz = arg[3];
			for (idx = 0 ; idx < pkt_num ; idx ++) {
				ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
				if (ret != RET_SUCCESS) {
					KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] ATCASE_UL_BASIC_SEND fail !\n",__FUNCTION__));
					/*send packet fail and retry*/
					idx --;
				}
			}
			break;
		case ATCASE_UL_BASIC_SEND_RAND:
			test_que_no = arg[1];
			pkt_num = arg[2];
			pkt_mode = arg[3];
			for (idx = 0 ; idx < pkt_num ; idx ++) {
				get_random_bytes(&rand_num, sizeof(rand_num));
				if (pkt_mode == 0) {
					get_random_bytes(&rand_num, sizeof(rand_num));
					pkt_sz = 100 + (rand_num % 1940);					
				} else {
					pkt_sz = pkt_mode;
				}
				ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
				if (ret != RET_SUCCESS) {
					KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] ATCASE_UL_BASIC_SEND fail !\n",__FUNCTION__));
					/*send packet fail and retry*/
					idx --;
				}
			}
			break;
		case ATCASE_UL_BASIC_MANY_QUE:
			max_que_no = arg[1];
			pkt_num = arg[2];
			pkt_sz = arg[3];
            for(test_que_no=0; test_que_no<=max_que_no; test_que_no++){
			    for (idx = 0 ; idx < pkt_num ; idx ++) {
				    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
				    if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] ATCASE_UL_BASIC_SEND fail !\n",__FUNCTION__));
					    /*send packet fail and retry*/
					    idx --;
				    }
			    }
            }
			break;
		case ATCASE_DL_BASIC_RECV :

            test_que_no = arg[1];
			pkt_num = arg[2];
			pkt_sz = arg[3];
			dl_cfg.gpd_num = pkt_num;
			dl_cfg.q_num = test_que_no;
			dl_cfg.tgpd_format.tgpd_ext_len = 0;
			dl_cfg.tgpd_format.tgpd_buf_len = pkt_sz;
			dl_cfg.tgpd_format.tbd_num = 0;
            sdio_test_option.show_dl_content = true;
            sdio_test_option.exam_dl_content = false;
			ret = sdio_dl_npkt(&dl_cfg);			
			break;
		default :
			break;
	}

    sdio_test_option.show_dl_content = false;
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

	return ret;
}


static int t_dev_normal_int(int argc, char** argv)
{
    unsigned short  direction;
    unsigned short  i, j;
    unsigned int    step_now, timeout;
    struct sk_buff *result_ptr = NULL;
    kal_uint32  test_isr_mask, orig_isr_mask;
    athif_cmd_t cmd;

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    
    for(i=0;i<HIF_MAX_ULQ_NUM; i++){
            
        direction = i;
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, SDIO_AT_SELF_NORMAL_INT, (char *)&direction, 2, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);

        step_now = 1;
        if( RET_FAIL == wait_mb_stepctrl(step_now, 10000) ){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] wait step ctrl %d timeout!!! \n",KAL_FUNC_NAME, step_now)) ;
            return RET_FAIL;
        }

        sdio_send_pkt(i, 4, 0, 0);
        
        step_now++;
        set_mb_stepctrl(step_now);

        step_now++;
        if( RET_FAIL == wait_mb_stepctrl(step_now, 10000) ){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] wait step ctrl %d timeout!!! \n",KAL_FUNC_NAME, step_now)) ;
            return RET_FAIL;
        }

        sdio_send_pkt(i, 4, 0, 0);

        step_now++;
        set_mb_stepctrl(step_now);

        if( RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT)){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Device response test fail or timeout!!! \n",KAL_FUNC_NAME)) ;
            return RET_FAIL;
        }
    }


    for(i=0;i<HIF_MAX_DLQ_NUM; i++){

        cmd.cmd = SDIO_AT_RESET_DL_QUEUE;
	    cmd.buf[0] = i; 
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
      
        //mask normal downlink interrupt
        sdio_func1_rd(SDIO_IP_WHIER, &orig_isr_mask, 4) ;
        test_isr_mask = orig_isr_mask & (~( 0x2<<(i)));
        sdio_func1_wr(SDIO_IP_WHIER, &test_isr_mask, 4); 
        
        direction = i+10;
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, SDIO_AT_SELF_NORMAL_INT, (char *)&direction, 2, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);

        step_now = 0;

        for(j=0; j<4; j++){
            step_now++;
            if( RET_FAIL == wait_mb_stepctrl(step_now, 10000) ){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] wait step ctrl %d timeout!!! \n",KAL_FUNC_NAME, step_now)) ;
                return RET_FAIL;
            }
            
            sdio_func1_wr(SDIO_IP_WHIER, &orig_isr_mask, 4) ;

            timeout = 0;
            result_ptr = NULL;
        	while(result_ptr == NULL) {
                KAL_SLEEP_MSEC(1);
		        timeout ++; 
		        if (timeout > 1000) { //1sec
		            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] No packet transferred from device !!! \n",KAL_FUNC_NAME)) ;
			        return RET_FAIL;
		        }
                result_ptr = mtlte_df_DL_read_skb_from_swq(i);
        	}

            mtlte_df_DL_pkt_handle_complete(i);
            dev_kfree_skb(result_ptr);
            sdio_func1_wr(SDIO_IP_WHIER, &test_isr_mask, 4); 
            
            step_now++;
            set_mb_stepctrl(step_now);

        }

        if( RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT)){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Device response test fail or timeout!!! \n",KAL_FUNC_NAME)) ;
            return RET_FAIL;
        }
        sdio_func1_wr(SDIO_IP_WHIER, &orig_isr_mask, 4) ;
	}

    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
    return RET_SUCCESS;

}


static int t_dev_normal_op_dl(int argc, char** argv)
{
    unsigned short  i, j, k, rx_pkt_len;
    unsigned int    step_now, fifo_cnt;
    unsigned char   *temp_mem, *temp_mem2;
    athif_mem_tst_cfg_t *rw_arg;
    unsigned int    set_WHIER, backup_WHIER;
    athif_dl_tgpd_cfg_t dl_cfg;
    sdio_whisr_enhance  *test_whisr;
    unsigned short *read_pkt_no, *read_pkt_len;
    athif_test_param_t    attest_param;
    athif_cmd_t cmd;

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    KAL_ALLOCATE_PHYSICAL_MEM(temp_mem, 768);
    KAL_ALLOCATE_PHYSICAL_MEM(temp_mem2, 768);
    rw_arg = (athif_mem_tst_cfg_t *)temp_mem;
    test_whisr = (sdio_whisr_enhance *)(temp_mem+32);

    // turn off the RX Done interrupt
    sdio_func1_rd(SDIO_IP_WHIER, &backup_WHIER, 4);
    set_WHIER = backup_WHIER & 0xFFFFFFE1;
    sdio_func1_wr(SDIO_IP_WHIER, &set_WHIER, 4);

    // set SW flag to change the behavior in test ISR
    attest_param.testing_dlq_pkt_fifo = 1;
    at_mtlte_hif_set_test_param(attest_param, set_testing_dlq_pkt_fifo);

    for(i=0; i<HIF_MAX_DLQ_NUM; i++){
        step_now = 0;

        // tell device to sent a DL packet and test the WRPLR function
        dl_cfg.gpd_num = 1;
		dl_cfg.q_num = i;
		dl_cfg.tgpd_format.tgpd_ext_len = 0;
		dl_cfg.tgpd_format.tgpd_buf_len = 16;
		dl_cfg.tgpd_format.tbd_num = 0;
		mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_DL_SEND_N, (char *)&dl_cfg, sizeof(athif_dl_perf_cfg_t), dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);

        if( RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT)){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Device response test fail or timeout!!! \n",KAL_FUNC_NAME)) ;
            return RET_FAIL;
        }

        
        // read WRPLR twice without really read the packet and check the FIFO value NOT pop-out
        if( i==0 || i == 1) { sdio_func1_rd( SDIO_IP_WRPLR, temp_mem2, 4); }
        else{ sdio_func1_rd( SDIO_IP_WRPLR1, temp_mem2, 4); }

        if( i==0 || i == 2) { rx_pkt_len = *(unsigned short *)temp_mem2; }
        else { rx_pkt_len = *(unsigned short *)(((unsigned int)(temp_mem2)) + 2);}
        
        if(rx_pkt_len!= 16 ){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Rx length not match at first time!!! \n",KAL_FUNC_NAME)) ;
            return RET_FAIL;
        }

        if( i==0 || i == 1) { sdio_func1_rd( SDIO_IP_WRPLR, temp_mem2, 4); }
        else{ sdio_func1_rd( SDIO_IP_WRPLR1, temp_mem2, 4); }

        if( i==0 || i == 2) { rx_pkt_len = *(unsigned short *)temp_mem2; }
        else { rx_pkt_len = *(unsigned short *)(((unsigned int)(temp_mem2)) + 2);}
        if(rx_pkt_len!= 16 ){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Rx length not match at second time!!! \n",KAL_FUNC_NAME)) ;
            return RET_FAIL;
        }
        
        // check the value in HWRLFACR is right.
        rw_arg->len = 4;
        rw_arg->mem_addr = (kal_uint32)ORG_SDIO_HWRLFACR;
        
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_READ_MEM, (char *)rw_arg, sizeof(athif_mem_tst_cfg_t), dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
        fifo_cnt = (*(unsigned int *)athif_result_save_t->buf & (0xFF <<(i*8))) >> (i*8);

        if(fifo_cnt != MT_LTE_RX_Q0_PKT_CNT-1 ){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] FW side rx fifo count error!!! \n",KAL_FUNC_NAME)) ;
            return RET_FAIL;
        }

        // tell device to hang 8 GPDs but only set 3 packet length into FIFO
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, SDIO_AT_NORMAL_DLQ_OP, (char *)&i, 2, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t); 

        step_now++;
        if( RET_FAIL == wait_mb_stepctrl(step_now, 10000) ){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] wait step ctrl %d timeout!!! \n",KAL_FUNC_NAME, step_now)) ;
            return RET_FAIL;
        }

        if(sdio_func1_rd(SDIO_IP_WHISR, test_whisr,sizeof(sdio_whisr_enhance))){
		    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] read SDIO_IP_WHISR fail in test \r\n", KAL_FUNC_NAME)) ;
		    return RET_FAIL;
        }

        at_transform_whisr_rx_tail(test_whisr);

        /*
        KAL_DBGPRINT(KAL, DBG_INFO,("The WHISR value = : \r\n")) ;
        for(j=0; j<sizeof(sdio_whisr_enhance); j+=4){
            KAL_DBGPRINT(KAL, DBG_INFO,("%x ", *(unsigned int *)((unsigned int)(test_whisr)+j))) ;
        }
        KAL_DBGPRINT(KAL, DBG_INFO,("\r\n")) ;
*/
        switch(i){
            case 0:
                read_pkt_no = &test_whisr->rx0_num;
                read_pkt_len = &test_whisr->rx0_pkt_len[0];
                break;
            case 1:
                read_pkt_no = &test_whisr->rx1_num;
                read_pkt_len = &test_whisr->rx1_pkt_len[0];
                break;
            case 2:
                read_pkt_no = &test_whisr->rx2_num;
                read_pkt_len = &test_whisr->rx2_pkt_len[0];
                break;
            case 3:
                read_pkt_no = &test_whisr->rx3_num;
                read_pkt_len = &test_whisr->rx3_pkt_len[0];
                break;
            default : 
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] i is too big!! i= %d! \r\n", KAL_FUNC_NAME, i));
                return RET_FAIL;
        }
        //read_pkt_no = (unsigned short *)((unsigned int)(test_whisr)+12+i*2);
        //read_pkt_len = (unsigned short *)( (unsigned int)(test_whisr)+20+i*32);
        
        if( *read_pkt_no != 3){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] packet number error in que %d!, read num = %d \r\n", KAL_FUNC_NAME, i, *read_pkt_no));
		    return RET_FAIL;
        }

        for(j=0; j<3; j++){
            if( *read_pkt_len != 16){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] packet len error in que %d!, read num = %d \r\n", KAL_FUNC_NAME, i, *read_pkt_no));
		        return RET_FAIL;
            }
        }

        if(sdio_func1_rd((SDIO_IP_WRDR0 +4*i), temp_mem2,512)){
		    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] read data fail in test \r\n", KAL_FUNC_NAME)) ;
		    return RET_FAIL;
        }

        KAL_DBGPRINT(KAL, DBG_INFO,("[Result] The read back value = : \r\n")) ;
        for(j=0; j<32; j++){
            for(k=0; k<16; k++){
                KAL_DBGPRINT(KAL, DBG_INFO,("%02x", *(unsigned char *)((unsigned int)(temp_mem2)+(j*16+k)) )) ;
            }
            KAL_DBGPRINT(KAL, DBG_INFO,("\r\n")) ;
        }

        step_now++;
        set_mb_stepctrl(step_now);

        if( RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT)){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Device response test fail or timeout!!! \n",KAL_FUNC_NAME)) ;
            return RET_FAIL;
        }

        cmd.cmd = SDIO_AT_RESET_DL_QUEUE;
	    cmd.buf[0] = i; 
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            
    }
    sdio_func1_wr(SDIO_IP_WHIER, &backup_WHIER, 4);

    attest_param.testing_dlq_pkt_fifo = 0;
    at_mtlte_hif_set_test_param(attest_param, set_testing_dlq_pkt_fifo);
    
    KAL_FREE_PHYSICAL_MEM(temp_mem);
    KAL_FREE_PHYSICAL_MEM(temp_mem2);
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
    return RET_SUCCESS;

}


static int t_dev_dl_basic_trans(int argc, char** argv)
{
    athif_dl_tgpd_cfg_t dl_cfg;
    kal_uint32 que_no, gpd_num, buf_len, bd_num;
    kal_uint32 i;
    int ret = RET_SUCCESS;
    //struct sk_buff *result_ptr = NULL;

    kal_uint32 idx;
    len_range_t tgpd_len_range[] = {
		{sizeof(AT_PKT_HEADER)	, 30},
        {110	, 140	},
        {240	, 270	},
		{500	, 530	},
		{1010	, 1040	},
		{1520	, 1550	},
		{2030	, 2048	}
	};
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    cmp_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.show_dl_content = false;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = false;


    for(que_no=0; que_no<HIF_MAX_DLQ_NUM; que_no++){
        for(buf_len=16; buf_len<2048; buf_len++){
            for(gpd_num=1; gpd_num<=3; gpd_num++){

                dl_cfg.q_num = que_no;
                dl_cfg.gpd_num = gpd_num;
                dl_cfg.tgpd_format.tgpd_ext_len = 0;
			    dl_cfg.tgpd_format.tgpd_buf_len = buf_len;
			    dl_cfg.tgpd_format.tbd_num = 0;
                ret = sdio_dl_npkt(&dl_cfg);

                if( RET_FAIL == ret){
                    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] send DL packet request fail!!! \n",KAL_FUNC_NAME)) ;
                    return RET_FAIL;
                }
                    
            }
        }
    }
  

    for(que_no=0; que_no<HIF_MAX_DLQ_NUM; que_no++){
        for (idx = 0 ; idx < (sizeof(tgpd_len_range)/sizeof(len_range_t)) ; idx ++) {
            for(buf_len = tgpd_len_range[idx].start_len; buf_len <= tgpd_len_range[idx].end_len; buf_len++){
                for(gpd_num=1; gpd_num<=3; gpd_num++){
                    for(bd_num=1; bd_num<=3; bd_num++){

                        // NOTE: to avoid packet is bigger than limitattion
                        if(DEV_MAX_PKT_SIZE >= (buf_len * bd_num)){

                            dl_cfg.q_num = que_no;
                            dl_cfg.gpd_num = gpd_num;
                            dl_cfg.tgpd_format.tgpd_ext_len = 0;
        			        dl_cfg.tgpd_format.tgpd_buf_len = buf_len * bd_num; 
                            for(i=0; i<bd_num; i++){
                                dl_cfg.tgpd_format.tbd_format[i].tbd_buf_len = buf_len;
                                dl_cfg.tgpd_format.tbd_format[i].tbd_ext_len = 0;
                            }
                            ret = sdio_dl_npkt(&dl_cfg);

                            if( RET_FAIL == ret){
                                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] send DL packet request fail!!! \n",KAL_FUNC_NAME)) ;
                                return RET_FAIL;
                            }
                            
                        }
                        
                    }    
                }
            }
        }
    }

    sdio_test_option.exam_dl_content = false;
    sdio_test_option.auto_receive_pkt = true;
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
    
    return RET_SUCCESS;
}


static int t_dev_dl_gpd_ext(int argc, char** argv)
{
    athif_dl_tgpd_cfg_t dl_cfg;
    kal_uint32 que_no, gpd_num, buf_len, gpd_ext;
    kal_uint32 idx;
    int ret = RET_SUCCESS;

    len_range_t tgpd_len_range[] = {
		{sizeof(AT_PKT_HEADER)	, 30},
        {110	, 140	},
        {240	, 270	},
		{500	, 530	},
		{1010	, 1040	},
		{1520	, 1550	},
		{2030	, 2048	}
	};
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    cmp_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.show_dl_content = false;
    sdio_test_option.exam_dl_content = true;


	for (que_no = 0 ; que_no < HIF_MAX_DLQ_NUM ; que_no ++) {
		for (gpd_num = 3 ; gpd_num <= 3; gpd_num++) {
			for (idx = 0 ; idx < (sizeof(tgpd_len_range)/sizeof(len_range_t)) ; idx ++) {
                for(buf_len = tgpd_len_range[idx].start_len; buf_len <= tgpd_len_range[idx].end_len; buf_len++){
				    for (gpd_ext = 0; gpd_ext <= 10; gpd_ext++) {
					    dl_cfg.q_num = que_no;
			 		    dl_cfg.gpd_num = gpd_num;
					    dl_cfg.tgpd_format.tgpd_ext_len = gpd_ext;
					    dl_cfg.tgpd_format.tgpd_buf_len = buf_len;
					    dl_cfg.tgpd_format.tbd_num = 0;
					    ret = sdio_dl_npkt(&dl_cfg);
					    if (ret != RET_SUCCESS) {
					    	KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] send DL packet request fail!!! \n",KAL_FUNC_NAME)) ;
                            return RET_FAIL;
					    }


                        if(buf_len > 285){
                            dl_cfg.q_num = que_no;
			 		        dl_cfg.gpd_num = gpd_num;
					        dl_cfg.tgpd_format.tgpd_ext_len = gpd_ext;
					        dl_cfg.tgpd_format.tgpd_buf_len = buf_len - gpd_ext;
					        dl_cfg.tgpd_format.tbd_num = 0;
					        ret = sdio_dl_npkt(&dl_cfg);
					        if (ret != RET_SUCCESS) {
					    	    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] send DL packet request fail!!! \n",KAL_FUNC_NAME)) ;
                                return RET_FAIL;
					        }
				        }
				    }

                    for (gpd_ext = 244; gpd_ext <= 255; gpd_ext++) {
					    dl_cfg.q_num = que_no;
			 		    dl_cfg.gpd_num = gpd_num;
					    dl_cfg.tgpd_format.tgpd_ext_len = gpd_ext;
					    dl_cfg.tgpd_format.tgpd_buf_len = buf_len;
					    dl_cfg.tgpd_format.tbd_num = 0;
					    ret = sdio_dl_npkt(&dl_cfg);
					    if (ret != RET_SUCCESS) {
					    	KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] send DL packet request fail!!! \n",KAL_FUNC_NAME)) ;
                            return RET_FAIL;
					    }


                        if(buf_len > 285){
                            dl_cfg.q_num = que_no;
			 		        dl_cfg.gpd_num = gpd_num;
					        dl_cfg.tgpd_format.tgpd_ext_len = gpd_ext;
					        dl_cfg.tgpd_format.tgpd_buf_len = buf_len - gpd_ext;
					        dl_cfg.tgpd_format.tbd_num = 0;
					        ret = sdio_dl_npkt(&dl_cfg);
					        if (ret != RET_SUCCESS) {
					    	    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] send DL packet request fail!!! \n",KAL_FUNC_NAME)) ;
                                return RET_FAIL;
					        }
				        }
				    }
                    
                }
			}
		}
	}	

    sdio_test_option.exam_dl_content = false;
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    return RET_SUCCESS;
}


static int t_dev_rx_basic(int argc , char **argv)
{
	unsigned int idx = 0 , param_num = 0, idx2 = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int testmode = 0 , pkt_num = 0, ep_num = 0 , pkt_sz = 0;
	unsigned int gpd_len = 0, gpd_ext = 0, bd_num = 0, bd1_buf_len = 0, bd1_ext = 0, bd2_ext = 0;
	//unsigned int rand_num = 0;
	athif_dl_tgpd_cfg_t dl_cfg;
	len_range_t tgpd_len_range[] = {
		{sizeof(AT_PKT_HEADER)	, 30},
        {110	, 140	},
        {240	, 270	},
		{500	, 530	},
		{1010	, 1040	},
		{1520	, 1550	},
		{2030	, 2048	}
	};
	len_range_t tgpd_ext_range[] ={
		{0		,10		},
		{120	,140	},
		{240	,255	}
	};
	len_range_t tbd_len_range[] = {
		{sizeof(AT_PKT_HEADER)	,30},
        {110	,140	},
        {240	,270	},
		{500	,530	},
		{1010	,1040	},
		{1520	,1550	},
		{2030	,2048	},
	};
	len_range_t bd1_ext_ragne[] = {
		{0		,10		},
		{240	,255	},
	};
	len_range_t bd2_ext_ragne[] = {
		{0		,10		},
		{240	,255	},
	};

	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] param[%d] = %d\n",KAL_FUNC_NAME,idx-1, arg[idx-1]));
	}

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    sdio_test_option.exam_dl_content = true;
    //sdio_test_option.show_dl_content= true;
    cmp_pattern = ATCASE_LB_DATA_AUTO;

	memset(&dl_cfg , 0 ,sizeof(athif_dl_tgpd_cfg_t));
	testmode = arg[0];
	switch (testmode) {
		case ATCASE_DL_TGPD_NUM_BOUNDARY:
			/*TGPD=1~10, TGPD Ext=0,TGPD len=1520~1550, TBD=0,TBD Ext=0*/
			for (ep_num = 0 ; ep_num < HIF_MAX_DLQ_NUM ; ep_num ++) {
				for (pkt_num = 1 ; pkt_num <= 10 ; pkt_num ++) {
					for (pkt_sz = 1520 ; pkt_sz <= 1550 ; pkt_sz ++) {
						dl_cfg.q_num = ep_num;
			 			dl_cfg.gpd_num = pkt_num;
						dl_cfg.tgpd_format.tgpd_ext_len = 0;
						dl_cfg.tgpd_format.tgpd_buf_len = pkt_sz;
						dl_cfg.tgpd_format.tbd_num = 0;
						ret = sdio_dl_npkt(&dl_cfg);
						if (ret != RET_SUCCESS) {
							goto rx_basic_err;
						}
					}
				}
			}
			break;
		case ATCASE_DL_TGPD_LEN_BOUNDARY:
			/*TGPD=5,TGPD Ext=0,TGPD len=8~20/110~140/240~270/500~530/1010~1040/1520~1550/2030~2048, TBD=0*/
			pkt_num = 5;
			for (ep_num = 0 ; ep_num < HIF_MAX_DLQ_NUM ; ep_num ++) {
				for (idx = 0 ; idx < (sizeof(tgpd_len_range)/sizeof(len_range_t)) ; idx ++) {
					for (pkt_sz = tgpd_len_range[idx].start_len ; pkt_sz <= tgpd_len_range[idx].end_len ; pkt_sz ++) {
						dl_cfg.q_num = ep_num;
				 		dl_cfg.gpd_num = pkt_num;
						dl_cfg.tgpd_format.tgpd_ext_len = 0;
						dl_cfg.tgpd_format.tgpd_buf_len = pkt_sz;
						dl_cfg.tgpd_format.tbd_num = 0;
						ret = sdio_dl_npkt(&dl_cfg);
						if (ret != RET_SUCCESS) {
							goto rx_basic_err;
						}					
					}
				}
			}			
			break;
		case ATCASE_DL_TGPD_EXT_BOUNDARY:
			/*TGPD=5, TGPD Ext=0~10/120~140/240~255, TGPD len = (1520~1550)-TGPD Ext, TBD=0*/
			pkt_num = 5;
			for (ep_num = 0 ; ep_num < HIF_MAX_DLQ_NUM ; ep_num ++) {
				for (idx = 0 ; idx < (sizeof(tgpd_ext_range)/sizeof(len_range_t)) ; idx ++) {
					for (pkt_sz = 1520 ; pkt_sz <= 1550 ; pkt_sz ++) {
						for (gpd_ext = tgpd_ext_range[idx].start_len ; gpd_ext <= tgpd_ext_range[idx].end_len ; gpd_ext++) {
							dl_cfg.q_num = ep_num;
					 		dl_cfg.gpd_num = pkt_num;
							dl_cfg.tgpd_format.tgpd_ext_len = gpd_ext;
							dl_cfg.tgpd_format.tgpd_buf_len = pkt_sz - gpd_ext;
							dl_cfg.tgpd_format.tbd_num = 0;
							ret = sdio_dl_npkt(&dl_cfg);
							if (ret != RET_SUCCESS) {
								goto rx_basic_err;
							}					
						}
					}
				}
			}						
			break;
		case ATCASE_DL_TBD_NUM_BOUNDARY:
			/*TGPD=5, TGPD Ext=0, TGPD Len=0, TBD=1~10,TBD Ext=0, TBD Len=189~195*/
			pkt_num = 5;
			for (ep_num = 0 ; ep_num < HIF_MAX_DLQ_NUM ; ep_num ++) {
				for (bd_num = 1 ; bd_num <= ATHIF_MAX_TBD_NUM ; bd_num ++) {
					dl_cfg.q_num = ep_num;
					dl_cfg.gpd_num = pkt_num;
					dl_cfg.tgpd_format.tgpd_ext_len = 0;
					dl_cfg.tgpd_format.tbd_num = bd_num;
					for (bd1_buf_len = 189 ; bd1_buf_len <= 195 ; bd1_buf_len ++) {
						pkt_sz = 0;
						for (idx = 0 ; idx < bd_num ; idx ++) {
							dl_cfg.tgpd_format.tbd_format[idx].tbd_ext_len = 0;
							dl_cfg.tgpd_format.tbd_format[idx].tbd_buf_len = bd1_buf_len;							
							pkt_sz += dl_cfg.tgpd_format.tbd_format[idx].tbd_ext_len;
							pkt_sz += dl_cfg.tgpd_format.tbd_format[idx].tbd_buf_len;
						}
						dl_cfg.tgpd_format.tgpd_buf_len = pkt_sz;
						ret = sdio_dl_npkt(&dl_cfg);
						if (ret != RET_SUCCESS) {
							goto rx_basic_err;
						}										
					}
				}
			}			
			break;
		case ATCASE_DL_1TBD_LEN_BOUNDARY:
			/*TGPD=5, TGDP Ext=0, TGPD Len=0, TBD=1, TBD Ext=0, TBD Len=5~20/110~140/240~270/500~530/1010~1040/1520~1550/2030~2048*/
			pkt_num = 5;
			for (ep_num = 0 ; ep_num < HIF_MAX_DLQ_NUM ; ep_num ++) {
				for (idx = 0 ; idx < (sizeof(tbd_len_range)/sizeof(len_range_t)) ; idx ++) {
					for (bd1_buf_len = tbd_len_range[idx].start_len ; bd1_buf_len <= tbd_len_range[idx].end_len ; bd1_buf_len ++) {
						pkt_sz = 0;
						dl_cfg.q_num = ep_num;
						dl_cfg.gpd_num = pkt_num;
						dl_cfg.tgpd_format.tgpd_ext_len = 0;
						dl_cfg.tgpd_format.tbd_num = 1;
						dl_cfg.tgpd_format.tbd_format[0].tbd_ext_len = 0;
						dl_cfg.tgpd_format.tbd_format[0].tbd_buf_len = bd1_buf_len;

						pkt_sz += dl_cfg.tgpd_format.tbd_format[0].tbd_ext_len;
						pkt_sz += dl_cfg.tgpd_format.tbd_format[0].tbd_buf_len;		
						dl_cfg.tgpd_format.tgpd_buf_len = pkt_sz;
						
						ret = sdio_dl_npkt(&dl_cfg);
						if (ret != RET_SUCCESS) {
							goto rx_basic_err;
						}															
					}
				}
			}
			break;
		case ATCASE_DL_2TBD_LEN_BOUNDARY:
			/*TGPD=5, TGPD Ext=0, TGPD len=0, TBD=2, TBD1 Ext=0,TBD1 len= 120~140, TBD2 Ext=0,TBD2 len=(1530~1540)-(BD1 Len)*/
			pkt_num = 5;
			for (ep_num = 0 ; ep_num < HIF_MAX_DLQ_NUM ; ep_num ++) {
				for (pkt_sz = 1530 ; pkt_sz <= 1540 ; pkt_sz ++) {
					for (bd1_buf_len = 120 ; bd1_buf_len <= 140 ; bd1_buf_len ++) {
						dl_cfg.q_num = ep_num;
						dl_cfg.gpd_num = pkt_num;
						dl_cfg.tgpd_format.tgpd_ext_len = 0;
						dl_cfg.tgpd_format.tgpd_buf_len = pkt_sz;
						dl_cfg.tgpd_format.tbd_num = 2;
						dl_cfg.tgpd_format.tbd_format[0].tbd_ext_len = 0;
						dl_cfg.tgpd_format.tbd_format[0].tbd_buf_len = bd1_buf_len;
						dl_cfg.tgpd_format.tbd_format[1].tbd_ext_len = 0;
						dl_cfg.tgpd_format.tbd_format[1].tbd_buf_len = pkt_sz - bd1_buf_len;
						ret = sdio_dl_npkt(&dl_cfg);
						if (ret != RET_SUCCESS) {
							goto rx_basic_err;
						}															
					}
				}
			}
			break;
		case ATCASE_DL_2TBD_EXT_BOUNDARY:
			/*TGPD=5,TGPD Ext=0, TGPD len=0, TBD=2, TBD1 Ext=0~10/240~255, TBD1 len=512, TBD2 Ext=0~10/240~255, TBD2 len=1000*/
			pkt_num = 5;
			for (ep_num = 0 ; ep_num < HIF_MAX_DLQ_NUM ; ep_num ++) {
				for (idx = 0 ; idx < (sizeof(bd1_ext_ragne)/sizeof(len_range_t)) ; idx ++) {
					for (idx2 = 0 ; idx2 < (sizeof(bd2_ext_ragne)/sizeof(len_range_t)) ; idx2 ++) {
						for (bd1_ext = bd1_ext_ragne[idx].start_len ; bd1_ext <= bd1_ext_ragne[idx].end_len ; bd1_ext ++) {
							for (bd2_ext = bd2_ext_ragne[idx2].start_len ; bd2_ext <= bd2_ext_ragne[idx2].end_len ; bd2_ext ++) {
								pkt_sz = 0;
								dl_cfg.q_num = ep_num;
								dl_cfg.gpd_num = pkt_num;
								dl_cfg.tgpd_format.tgpd_ext_len = 0;
								dl_cfg.tgpd_format.tbd_num = 2;
								dl_cfg.tgpd_format.tbd_format[0].tbd_ext_len = bd1_ext;
								dl_cfg.tgpd_format.tbd_format[0].tbd_buf_len = 512;
								dl_cfg.tgpd_format.tbd_format[1].tbd_ext_len = bd2_ext;
								dl_cfg.tgpd_format.tbd_format[1].tbd_buf_len = 1000;

								pkt_sz += dl_cfg.tgpd_format.tbd_format[0].tbd_ext_len;
								pkt_sz += dl_cfg.tgpd_format.tbd_format[0].tbd_buf_len;
								pkt_sz += dl_cfg.tgpd_format.tbd_format[1].tbd_ext_len;
								pkt_sz += dl_cfg.tgpd_format.tbd_format[1].tbd_buf_len;
								dl_cfg.tgpd_format.tgpd_buf_len = pkt_sz;
								ret = sdio_dl_npkt(&dl_cfg);
								if (ret != RET_SUCCESS) {
									goto rx_basic_err;
								}																							
							}
						}
					}
				}
			}			
			break;
 		case ATCASE_DL_RANDOM:
			if (param_num > 1) { //if use the parameter then issue user specific configure
	 			ep_num = arg[1];
 				pkt_num = arg[2];
 				ret = sdio_dl_n_rand_pkt(pkt_num, ep_num);
			} else { //test fully test plan DL random GPD type test
				for (ep_num = 0 ; ep_num < HIF_MAX_DLQ_NUM ; ep_num ++) {
					for (pkt_num = 1 ; pkt_num <= 30 ; pkt_num ++) {
 						ret = sdio_dl_n_rand_pkt(pkt_num, ep_num);
 						if (ret != RET_SUCCESS) {
 							goto rx_basic_err;
 						}
					}
				}
			}
 			break;
 		case ATCASE_DL_RANDOM_STRESS:
			ep_num = arg[1];
			pkt_num = arg[2];
			ret = sdio_dl_n_rand_stress(pkt_num, ep_num);
 			break;
 		case ATCASE_DL_SPECIFIC:
			pkt_sz = 0;
			ep_num = arg[1];
			pkt_num = arg[2];
 			gpd_ext = arg[3];
			gpd_len = arg[4];
 			bd_num = arg[5];
 			bd1_ext = arg[6];
 			bd1_buf_len = arg[7];
 			dl_cfg.q_num = ep_num ;
			dl_cfg.gpd_num = pkt_num;
			dl_cfg.tgpd_format.tgpd_ext_len = gpd_ext;
			dl_cfg.tgpd_format.tgpd_buf_len = gpd_len;
			dl_cfg.tgpd_format.tbd_num = bd_num;
			if (bd_num) {				
				for (idx = 0 ; idx < bd_num ; idx ++) {
					dl_cfg.tgpd_format.tbd_format[idx].tbd_ext_len = bd1_ext;
					dl_cfg.tgpd_format.tbd_format[idx].tbd_buf_len = bd1_buf_len;
					pkt_sz += dl_cfg.tgpd_format.tbd_format[idx].tbd_ext_len;
					pkt_sz += dl_cfg.tgpd_format.tbd_format[idx].tbd_buf_len;
				}
				//dl_cfg.tgpd_format.tgpd_buf_len = pkt_sz;
			}
			ret = sdio_dl_npkt(&dl_cfg);
			if (ret != RET_SUCCESS) {
				goto rx_basic_err;
			}																							

 			break;
        case ATCASE_DL_SDIO_SP:
			pkt_sz = 0;
			ep_num = arg[1];
			pkt_num = arg[2];
 			gpd_ext = arg[3];
			gpd_len = arg[4];
 			bd_num = arg[5];
 			bd1_ext = arg[6];
 			bd1_buf_len = arg[7];
 			dl_cfg.q_num = ep_num ;
			dl_cfg.gpd_num = pkt_num;
			dl_cfg.tgpd_format.tgpd_ext_len = gpd_ext;
			dl_cfg.tgpd_format.tgpd_buf_len = gpd_len;
			dl_cfg.tgpd_format.tbd_num = bd_num;
			if (bd_num) {				
				for (idx = 0 ; idx < bd_num ; idx ++) {
					dl_cfg.tgpd_format.tbd_format[idx].tbd_ext_len = bd1_ext;
					dl_cfg.tgpd_format.tbd_format[idx].tbd_buf_len = bd1_buf_len;
					pkt_sz += dl_cfg.tgpd_format.tbd_format[idx].tbd_ext_len;
					pkt_sz += dl_cfg.tgpd_format.tbd_format[idx].tbd_buf_len;
				}
				//dl_cfg.tgpd_format.tgpd_buf_len = pkt_sz;
			}
			ret = sdio_dl_npkt_sp(&dl_cfg);
			if (ret != RET_SUCCESS) {
				goto rx_basic_err;
			}																							

 			break;
 		default :
			break;
	}
rx_basic_err:	

    sdio_test_option.show_dl_content= false;
    sdio_test_option.exam_dl_content = false;
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
	return ret;
}


static int t_dev_tx_basic(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
    athif_fwd_mode_e tx_deq_mode;

    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    tx_deq_mode = ATHIF_FWD_CMP_DATA;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    send_pattern = ATCASE_LB_DATA_AUTO;

	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

	for(test_que_no=0; test_que_no<HIF_MAX_ULQ_NUM; test_que_no++){
        for(pkt_sz=arg[0]; pkt_sz<=arg[1]; pkt_sz++){ 
	        for(pkt_num=40; pkt_num<=40; pkt_num++){
			    for (idx = 0 ; idx < pkt_num ; idx ++) {
                    KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                        __FUNCTION__, test_que_no, pkt_sz, idx));
				    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
				    if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					    /*send packet fail and retry*/
                        idx --;
                        return ret;
				    } 
			    }
                KAL_SLEEP_MSEC(1) ;
            }
	    }
        //test print
        printk(KERN_ERR "[%s] que[%d] test is passed \n",__FUNCTION__,test_que_no);
        
	}

	

    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

	return ret;
}


static int t_dev_tx_multique(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
	unsigned short rand_num = 0;
    athif_fwd_mode_e tx_deq_mode;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    tx_deq_mode = ATHIF_FWD_CMP_DATA;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    send_pattern = ATCASE_LB_DATA_AUTO;

	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

        for(pkt_sz=arg[0]; pkt_sz<=arg[1]; pkt_sz++){
	        for(pkt_num=4; pkt_num<=16; pkt_num++){
			    for (idx = 0 ; idx < pkt_num ; idx ++) {
                    get_random_bytes(&rand_num, sizeof(unsigned short));
                    test_que_no = rand_num % HIF_MAX_ULQ_NUM;
                     
                    KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                        __FUNCTION__, test_que_no, pkt_sz, idx));
                    
				    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
				    if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					    /*send packet fail and retry*/
                        idx --;
                        return ret;
				    } 
			    }
                KAL_SLEEP_MSEC(1) ;
            }
	    }

    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

	return ret;
}



static int t_dev_simple_lb(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
	unsigned int expect_num = 0, timeout=0;
    unsigned int old_pkt_cnt = 0;
    athif_fwd_mode_e tx_deq_mode;
    athif_cmd_t cmd;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}
    
    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    //cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
    //cmd.buf[0] = ATHIF_LB_TGPD_EMPTY_ENQ;
	//cmd.len = 1;

	//mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    //mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    //if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;


	for(test_que_no=0; test_que_no<HIF_MAX_ULQ_NUM; test_que_no++){
        for(pkt_sz=arg[0]; pkt_sz<=arg[1]; pkt_sz++){ 
	        for(pkt_num=arg[2]; pkt_num<=arg[3]; pkt_num++){
			    for (idx = 0 ; idx < pkt_num ; idx ++) {
                    KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                        __FUNCTION__, test_que_no, pkt_sz, idx));
				    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
                    expect_num++;
                    
				    if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					    /*send packet fail and retry*/
                        idx --;
                        return ret;
				    } 
			    }
                KAL_SLEEP_MSEC(1) ;
            }
            // test debug
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Passe of que=%d, pkt_size=%d\n", \
                        __FUNCTION__, test_que_no, pkt_sz));
	    }
	}

    while(expect_num != recv_total_pkt_cnt){
        
        KAL_SLEEP_MSEC(1) ;
        if(recv_total_pkt_cnt != old_pkt_cnt){ timeout ==0; }
        else{ timeout++; }
        
        if(timeout > 1000){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;
            return RET_FAIL;
        }
        old_pkt_cnt = recv_total_pkt_cnt;
    }

    tx_deq_mode = ATHIF_FWD_FREE_ONLY;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    sdio_test_option.exam_dl_content = false;
    sdio_test_option.auto_receive_pkt = false;
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    if(recv_th_rslt == RET_SUCCESS){
	    return ret;
    }else{
        return RET_FAIL;
    }
}



static int t_dev_simple_lb_empty_enq(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
	unsigned int expect_num = 0, timeout=0;
    unsigned int old_pkt_cnt = 0;
    athif_fwd_mode_e tx_deq_mode;
    athif_cmd_t cmd;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		KAL_DBGPRINT(KAL, DBG_WARN,("[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]));
	}
    
    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
    cmd.buf[0] = ATHIF_LB_TGPD_EMPTY_ENQ;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;


	for(test_que_no=0; test_que_no<HIF_MAX_ULQ_NUM; test_que_no++){
        for(pkt_sz=arg[0]; pkt_sz<=arg[1]; pkt_sz++){ 
	        for(pkt_num=arg[2]; pkt_num<=arg[3]; pkt_num++){
			    for (idx = 0 ; idx < pkt_num ; idx ++) {
                    KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                        __FUNCTION__, test_que_no, pkt_sz, idx));
				    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
                    expect_num++;
                    
				    if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					    /*send packet fail and retry*/
                        idx --;
                        return ret;
				    } 
			    }
                KAL_SLEEP_MSEC(1) ;
            }
            // test debug
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Passe of que=%d, pkt_size=%d\n", \
                        __FUNCTION__, test_que_no, pkt_sz));
	    }
	}

    while(expect_num != recv_total_pkt_cnt){
        
        KAL_SLEEP_MSEC(1) ;
        if(recv_total_pkt_cnt != old_pkt_cnt){ timeout ==0; }
        else{ timeout++; }
        
        if(timeout > 1000){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;
            return RET_FAIL;
        }
        old_pkt_cnt = recv_total_pkt_cnt;
    }

    tx_deq_mode = ATHIF_FWD_FREE_ONLY;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    sdio_test_option.exam_dl_content = false;
    sdio_test_option.auto_receive_pkt = false;
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    if(recv_th_rslt == RET_SUCCESS){
	    return ret;
    }else{
        return RET_FAIL;
    }
}



static int t_dev_random_lb(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
	unsigned int expect_num = 0, timeout=0;
    unsigned int old_pkt_cnt = 0;
    athif_fwd_mode_e tx_deq_mode;
    athif_cmd_t cmd;
    unsigned int round;
    struct timespec start_t , end_t, diff_t;
    unsigned long long transferdata=0; 
    unsigned long long diff_ms = 0;
    unsigned int performance = 0;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;


	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    for(round=0; round<arg[0]; round++){
        expect_num = 0;
        transferdata = 0;
        recv_total_pkt_cnt = 0;
        jiffies_to_timespec(jiffies , &start_t);
        
        while(expect_num < arg[1]){
            
            get_random_bytes(&test_que_no, sizeof(unsigned int)) ;
        	test_que_no = test_que_no % HIF_MAX_ULQ_NUM;
            get_random_bytes(&pkt_num, sizeof(unsigned int)) ;
            pkt_num = (pkt_num % 50) + 50;    
            //pkt_num = 60;

    		for (idx = 0 ; idx < pkt_num ; idx ++) {
                get_random_bytes(&pkt_sz, sizeof(unsigned int)) ;
                pkt_sz = (pkt_sz % 512) + 1536;
                //pkt_sz = 1500;
    		    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
                expect_num++;
                transferdata += pkt_sz;
                
    		    if (ret != RET_SUCCESS) {
    			    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
    			    /*send packet fail and retry*/
                    idx --;
                    return ret;
    		    } 
    		}
                    
            KAL_SLEEP_USEC(1) ;
        }

        while(expect_num != recv_total_pkt_cnt){
            
            KAL_SLEEP_USEC(1) ;
            if(recv_total_pkt_cnt != old_pkt_cnt){ timeout ==0; }
            else{ timeout++; }
            
            if(timeout > 3000){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Test Fail at round %d !!! \n",KAL_FUNC_NAME, round)) ;
                return RET_FAIL;
            }
            old_pkt_cnt = recv_total_pkt_cnt;
        
        }
        
        jiffies_to_timespec(jiffies , &end_t);
		diff_t = time_diff(start_t, end_t);
		diff_ms = (1000 * diff_t.tv_sec) + ((unsigned int)(diff_t.tv_nsec) / (1000*1000)) ;

        performance = ((unsigned int)(transferdata * 2) / (unsigned int)diff_ms);
        
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[INFO] Total transfer %d bytes, Throughtput of this round = %dKByte/s (%dKbit/s) \n",KAL_FUNC_NAME, \
                                                          (unsigned int)transferdata*2, performance, performance*8)) ;
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[INFO] Test Pass at round %d , continue...... \n",KAL_FUNC_NAME, round)) ;
    }






    tx_deq_mode = ATHIF_FWD_FREE_ONLY;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    sdio_test_option.exam_dl_content = false;
    sdio_test_option.auto_receive_pkt = false;
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    if(recv_th_rslt == RET_SUCCESS){
	    return ret;
    }else{
        return RET_FAIL;
    }
}



static int t_dev_single_queue_lb(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
	unsigned int expect_num = 0, timeout=0;
    unsigned int old_pkt_cnt = 0;
    athif_fwd_mode_e tx_deq_mode;
    athif_cmd_t cmd;
    unsigned int test_time;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    if(0 == arg[0]){
        cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
        cmd.buf[0] = ATHIF_LB_TGPD_DIRECT;
    	cmd.len = 1;
    
    	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    }else{
        cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
        cmd.buf[0] = ATHIF_LB_TGPD_EMPTY_ENQ;
    	cmd.len = 1;

    	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    }

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;


    //test_que_no = arg[1];
    for(test_time=0; test_time<=arg[1]; test_time++){
        
        recv_total_pkt_cnt = 0;
        expect_num = 0;
        
        for(test_que_no=0; test_que_no<HIF_MAX_ULQ_NUM; test_que_no++){    
	        for(pkt_num=0; pkt_num<=arg[2]; pkt_num++){
                for(pkt_sz=arg[3]; pkt_sz<=arg[4]; pkt_sz++){ 
                
                    KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                        __FUNCTION__, test_que_no, pkt_sz, idx));
				    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
                    expect_num++;
                    
				    if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					    /*send packet fail and retry*/
                        idx --;
                        return ret;                  
			        }
                    
                }
                KAL_SLEEP_USEC(1) ;
                // test debug
                KAL_DBGPRINT(KAL, DBG_WARN, ("[%s] Passe of que=%d, pkt_size=%d\n", \
                         __FUNCTION__, test_que_no, pkt_sz));
            }
        }

        while(expect_num != recv_total_pkt_cnt){
            
            KAL_SLEEP_MSEC(1) ;
            if(recv_total_pkt_cnt != old_pkt_cnt){ timeout ==0; }
            else{ timeout++; }
            
            if(timeout > 1000){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;
                return RET_FAIL;
            }
            old_pkt_cnt = recv_total_pkt_cnt;
        }

        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Test %d time PASS!!! \n",KAL_FUNC_NAME, test_time)) ;
    }

    tx_deq_mode = ATHIF_FWD_FREE_ONLY;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    sdio_test_option.exam_dl_content = false;
    sdio_test_option.auto_receive_pkt = false;
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    if(recv_th_rslt == RET_SUCCESS){
	    return ret;
    }else{
        return RET_FAIL;
    }
}


static int t_dev_one_pkt_lb(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
	unsigned int expect_num = 0, timeout=0;
    unsigned int old_pkt_cnt = 0;
    athif_fwd_mode_e tx_deq_mode;
    athif_cmd_t cmd;
    unsigned int test_time;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    if(0 == arg[0]){
        cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
        cmd.buf[0] = ATHIF_LB_TGPD_DIRECT;
    	cmd.len = 1;
    
    	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    }else{
        cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
        cmd.buf[0] = ATHIF_LB_TGPD_EMPTY_ENQ;
    	cmd.len = 1;

    	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    }

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;


    //test_que_no = arg[1];
    for(test_time=0; test_time<=arg[1]; test_time++){
        
        recv_total_pkt_cnt = 0;
        expect_num = 0;
        
        test_que_no=1;
	    pkt_num = 1;
        get_random_bytes(&pkt_sz, sizeof(unsigned int)) ;
        pkt_sz = (pkt_sz % 2048) + 1;
        
                
                    KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                        __FUNCTION__, test_que_no, pkt_sz, idx));
				    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
                    expect_num++;
                    
				    if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					    /*send packet fail and retry*/
                        idx --;
                        return ret;                  
			        }
                    
                KAL_SLEEP_USEC(1) ;
                // test debug
                KAL_DBGPRINT(KAL, DBG_WARN, ("[%s] Passe of que=%d, pkt_size=%d\n", \
                         __FUNCTION__, test_que_no, pkt_sz));


        while(expect_num != recv_total_pkt_cnt){
            
            KAL_SLEEP_USEC(1) ;
            if(recv_total_pkt_cnt != old_pkt_cnt){ timeout ==0; }
            else{ timeout++; }
            
            if(timeout > 1000){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Not receive pkt back (num=%d, len=%d) !!! \n",KAL_FUNC_NAME, test_time, pkt_sz)) ;
                return RET_FAIL;
            }
            old_pkt_cnt = recv_total_pkt_cnt;
        }

        if(0 == (test_time & 0xFF)){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Test %d time PASS!!! \n",KAL_FUNC_NAME, test_time)) ;
        }
    }

    tx_deq_mode = ATHIF_FWD_FREE_ONLY;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    sdio_test_option.exam_dl_content = false;
    sdio_test_option.auto_receive_pkt = false;
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    if(recv_th_rslt == RET_SUCCESS){
	    return ret;
    }else{
        return RET_FAIL;
    }
}


static int t_dev_rx_len_fifo_max(int argc , char **argv)
{
    athif_dl_tgpd_cfg_t dl_cfg;
    kal_uint32 que_no; 
    // kal_uint32 gpd_num, buf_len, bd_num;
    kal_uint32 i, max_fifo_len, v_whcr;
    int ret = RET_SUCCESS;
    //struct sk_buff *result_ptr = NULL;
    athif_test_param_t    attest_param;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    cmp_pattern = ATCASE_LB_DATA_AUTO;
    attest_param = at_mtlte_hif_get_test_param();
    attest_param.testing_fifo_max= 1;
    at_mtlte_hif_set_test_param(attest_param, set_testing_fifo_max);
    attest_param.test_result= KAL_SUCCESS;
    at_mtlte_hif_set_test_param(attest_param, set_test_result);

    if ((ret = sdio_func1_rd(SDIO_IP_WHCR, &v_whcr, 4)) != KAL_SUCCESS){
		 return RET_FAIL ; 
	}


    for(que_no=0; que_no<HIF_MAX_DLQ_NUM; que_no++){
        for(max_fifo_len=1; max_fifo_len<16; max_fifo_len++){

            v_whcr = v_whcr & 0xFFFFFF0F;
            v_whcr = v_whcr + (max_fifo_len<< 4);
            
            if ((ret = sdio_func1_wr(SDIO_IP_WHCR, &v_whcr, 4)) != KAL_SUCCESS){
		        return RET_FAIL ; 
	        }	
            
            for(i=0; i<HIF_MAX_DLQ_NUM; i++){
                attest_param.fifo_max[i] = max_fifo_len;
            }
            at_mtlte_hif_set_test_param(attest_param, set_fifo_max);
            
            dl_cfg.q_num = que_no;
            dl_cfg.gpd_num = 16;
            dl_cfg.tgpd_format.tgpd_ext_len = 0;
		    dl_cfg.tgpd_format.tgpd_buf_len = 30;
		    dl_cfg.tgpd_format.tbd_num = 0;
            ret = sdio_dl_npkt(&dl_cfg);

            if( RET_FAIL == ret){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] send DL packet request fail!!! \n",KAL_FUNC_NAME)) ;
                return RET_FAIL;
            }
           
            attest_param = at_mtlte_hif_get_test_param();
            if(attest_param.test_result == KAL_FAIL){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] rx packet num is bigget than MAX setting %d at rxq%d !!! \n",KAL_FUNC_NAME, max_fifo_len, que_no)) ;
                return RET_FAIL;
            }
        }
    }

    v_whcr = v_whcr & 0xFFFFFF0F;
    if ((ret = sdio_func1_wr(SDIO_IP_WHCR, &v_whcr, 4)) != KAL_SUCCESS){
         return RET_FAIL ; 
    }

    attest_param.testing_fifo_max= 0;
    at_mtlte_hif_set_test_param(attest_param, set_testing_fifo_max);
  
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    return ret;
}


/*RGPD allow length test list*/
typedef struct _allowlen_info {
	unsigned int valid;
	unsigned int rgpd_allow_len;
} allowlen_info_t;
allowlen_info_t allow_len_list[] = {
    {1 , 128},
    {1 , 256},
	{1 , 512},
	{1 , 1024},
	{1 , 1536},
	{1 , 2048}
};


static int t_dev_single_allow_len(int argc, char** argv)
{
	unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int testmode = 0;
	athif_cmd_t cmd;
	athif_status_t status;
	unsigned int rand_num = 0 , i = 0 , xfer_len = 0 , que_no = 0 , allow_len_idx = 0, loop = 0, tmp = 0;
	athif_ul_rgpd_format_t rgpd_format;
	unsigned int rbd1_allow_len = 0, rbd2_allow_len = 0, rbd3_allow_len = 0;

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    sdio_test_option.auto_receive_pkt = true;
    //sdio_test_option.show_dl_content = true;
    sdio_test_option.exam_dl_content = true;
    
    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

	/*start loopback mode*/
	cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
 

	KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_RGPD_ALLOW_LEN, start\n",__FUNCTION__,__LINE__ ));			
	for (allow_len_idx = 0 ; allow_len_idx < sizeof(allow_len_list)/sizeof(allowlen_info_t) ; allow_len_idx ++) {
		if (allow_len_list[allow_len_idx].valid == 0) {
			continue;
		}				
		memset(&rgpd_format, 0 , sizeof(athif_ul_rgpd_format_t));			
		rgpd_format.rgpd_allow_len = allow_len_list[allow_len_idx].rgpd_allow_len;
		rgpd_format.rbd_num = 0;
		KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_RGPD_ALLOW_LEN, GPD allow_len=%d\n",__FUNCTION__,__LINE__, rgpd_format.rgpd_allow_len));							
		for (que_no = 0 ; que_no < HIF_MAX_ULQ_NUM ; que_no ++) {
			while (1) {
				if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, sizeof(AT_PKT_HEADER), 20) != RET_SUCCESS)
					break;
  
				if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, (rgpd_format.rgpd_allow_len-30), (rgpd_format.rgpd_allow_len)) != RET_SUCCESS)
					break;

				break;
			}
			if (ret != RET_SUCCESS) {
				return ret;
			}
		}
		if (ret != RET_SUCCESS) {
			return ret;
		}
	}
	if (ret == RET_SUCCESS) {
		KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_RGPD_ALLOW_LEN, passed\n",__FUNCTION__,__LINE__ ));
	}

	/*allow len : RBD1=512/1024/1536/2048*/
	memset(&rgpd_format, 0 , sizeof(athif_ul_rgpd_format_t));			
	rgpd_format.rgpd_allow_len = 0;
	rgpd_format.rbd_num = 1;
	unsigned int bd_allowlen[] = {128, 256, 512, 1024, 1536, 2048};
	KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_1RBD_ALLOW_LEN, 1BD test start\n",__FUNCTION__,__LINE__ ));			
	for (allow_len_idx = 0 ; allow_len_idx < sizeof(bd_allowlen)/sizeof(unsigned int) ; allow_len_idx ++) {
		rgpd_format.rbd_allow_len[0] = bd_allowlen[allow_len_idx];
		KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_1RBD_ALLOW_LEN, 1BD test BD1=%d\n",__FUNCTION__,__LINE__ ,rgpd_format.rbd_allow_len[0]));			
		for (que_no = 0 ; que_no < HIF_MAX_ULQ_NUM ; que_no ++) {
			while (1) {
				if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, sizeof(AT_PKT_HEADER), 20) != RET_SUCCESS)
					break;
				if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, (bd_allowlen[allow_len_idx]-30), (bd_allowlen[allow_len_idx])) != RET_SUCCESS)
					break;


				break;
			}
			if (ret != RET_SUCCESS) {
				break;
			}
		}
	}
    
    if (ret == RET_SUCCESS) {
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_1RBD_ALLOW_LEN, 1BD test passed\n",__FUNCTION__,__LINE__ ));
    }
    
    sdio_test_option.auto_receive_pkt = false;
    sdio_test_option.exam_dl_content = false;

    return ret;

}

static int t_dev_bd_allow_len(int argc, char** argv)
{
	unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int testmode = 0;
	athif_cmd_t cmd;
	athif_status_t status;
	unsigned int rand_num = 0 , i = 0 , xfer_len = 0 , que_no = 0 , allow_len_idx = 0, loop = 0, tmp = 0;
	athif_ul_rgpd_format_t rgpd_format;
	unsigned int rbd1_allow_len = 0, rbd2_allow_len = 0, rbd3_allow_len = 0;

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    sdio_test_option.auto_receive_pkt = true;
    //sdio_test_option.show_dl_content = true;
    sdio_test_option.exam_dl_content = true;
    
    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

	/*start loopback mode*/
	cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
 

	testmode = arg[0];

	switch (testmode) {

		case ATCASE_2RBD_ALLOW_LEN_CASE1:
			/*allow len : RBD1=252+4(0~2) , RBD2=2048-RBD1*/
			memset(&rgpd_format, 0 , sizeof(athif_ul_rgpd_format_t));			
			rgpd_format.rgpd_allow_len = 0;
			rgpd_format.rbd_num = 2;
			
			KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_2RBD_ALLOW_LEN, 2BD test start\n",__FUNCTION__,__LINE__ ));			
			for (rbd1_allow_len = 252 ; rbd1_allow_len <= 260 ; rbd1_allow_len += 4) {
				rbd2_allow_len = 2048 - rbd1_allow_len;
				rgpd_format.rbd_allow_len[0] = rbd1_allow_len;
				rgpd_format.rbd_allow_len[1] = rbd2_allow_len;
				KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_2RBD_ALLOW_LEN, 1BD test BD1=%d, BD2=%d\n",__FUNCTION__,__LINE__ ,rbd1_allow_len, rbd2_allow_len));

				for (que_no = 0 ; que_no < HIF_MAX_ULQ_NUM ; que_no ++) {
					do {
						if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, sizeof(AT_PKT_HEADER), 20) != RET_SUCCESS)
							break;
                        if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 110, 140) != RET_SUCCESS)
							break;
                        if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 240, 270) != RET_SUCCESS)
							break;
						if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 500, 530) != RET_SUCCESS)
							break;
						if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 1010, 1040) != RET_SUCCESS)
							break;
						if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 1520, 1550) != RET_SUCCESS)
							break;
						if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 2030, 2048) != RET_SUCCESS)
							break;
					} while(0);
                    
					if (ret != RET_SUCCESS) {
						goto rd2_allowlen_case1_err;
						break;
					}
				}
			}
			KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_2RBD_ALLOW_LEN, 2BD test passed\n",__FUNCTION__,__LINE__ ));

rd2_allowlen_case1_err:
            break;

		case ATCASE_2RBD_ALLOW_LEN_CASE2:
			/*allow len : RBD1=500+4(0~5) , RBD2=2048-RBD1*/
			memset(&rgpd_format, 0 , sizeof(athif_ul_rgpd_format_t));			
			rgpd_format.rgpd_allow_len = 0;
			rgpd_format.rbd_num = 2;
			
			KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_2RBD_ALLOW_LEN, 2BD test start\n",__FUNCTION__,__LINE__ ));			
			for (rbd1_allow_len = 500 ; rbd1_allow_len <= 520 ; rbd1_allow_len += 4) {
				rbd2_allow_len = 2048 - rbd1_allow_len;
				rgpd_format.rbd_allow_len[0] = rbd1_allow_len;
				rgpd_format.rbd_allow_len[1] = rbd2_allow_len;
				KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_2RBD_ALLOW_LEN, 1BD test BD1=%d, BD2=%d\n",__FUNCTION__,__LINE__ ,rbd1_allow_len, rbd2_allow_len));

				for (que_no = 0 ; que_no < HIF_MAX_ULQ_NUM ; que_no ++) {
					do {
						if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, sizeof(AT_PKT_HEADER), 20) != RET_SUCCESS)
							break;
                        if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 110, 140) != RET_SUCCESS)
							break;
                        if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 240, 270) != RET_SUCCESS)
							break;
						if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 500, 530) != RET_SUCCESS)
							break;
						if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 1010, 1040) != RET_SUCCESS)
							break;
						if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 1520, 1550) != RET_SUCCESS)
							break;
						if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 2030, 2048) != RET_SUCCESS)
							break;

					} while(0);
                    
					if (ret != RET_SUCCESS) {
						goto rd2_allowlen_case2_err;
						break;
					}
				}
			}
			KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_2RBD_ALLOW_LEN, 2BD test passed\n",__FUNCTION__,__LINE__ ));
			
rd2_allowlen_case2_err:
			break;

		case ATCASE_3RBD_ALLOW_LEN_CASE1:
			/*allow len : RBD1=124+4(0~2) , RBD2=252+4(0~2), RBD3=2048-RBD1-RBD2*/
			memset(&rgpd_format, 0 , sizeof(athif_ul_rgpd_format_t));			
			rgpd_format.rgpd_allow_len = 0;
			rgpd_format.rbd_num = 3;
			
			KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_3RBD_ALLOW_LEN, 3BD test start\n",__FUNCTION__,__LINE__ ));			
			for (rbd1_allow_len = 124 ; rbd1_allow_len <= 132 ; rbd1_allow_len += 4) {
				for (rbd2_allow_len = 252 ; rbd2_allow_len <= 260 ; rbd2_allow_len += 4) {
					rbd3_allow_len = 2048 - rbd1_allow_len - rbd2_allow_len;
					rgpd_format.rbd_allow_len[0] = rbd1_allow_len;
					rgpd_format.rbd_allow_len[1] = rbd2_allow_len;
					rgpd_format.rbd_allow_len[2] = rbd3_allow_len;
					KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_3RBD_ALLOW_LEN, 1BD test BD1=%d, BD2=%d, BD3=%d\n",
												__FUNCTION__,__LINE__ ,rbd1_allow_len, rbd2_allow_len, rbd3_allow_len));

					for (que_no = 0 ; que_no < HIF_MAX_ULQ_NUM ; que_no ++) {
						do {
							if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, sizeof(AT_PKT_HEADER), 20) != RET_SUCCESS)
								break;
                            if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 110, 140) != RET_SUCCESS)
							    break;
                            if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 240, 270) != RET_SUCCESS)
							    break;
							if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 500, 530) != RET_SUCCESS)
								break;
							if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 1010, 1040) != RET_SUCCESS)
								break;
							if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 1520, 1550) != RET_SUCCESS)
								break;
                            if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 2030, 2048) != RET_SUCCESS)
                                break;

						} while(0);
                        
						if (ret != RET_SUCCESS) {
							goto rd3_allowlen_case1_err;
							break;
						}
					}
				}
			}
			KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_3RBD_ALLOW_LEN, 3BD test passed\n",__FUNCTION__,__LINE__ ));
            
rd3_allowlen_case1_err:
            break;

		case ATCASE_3RBD_ALLOW_LEN_CASE2:
			/*allow len : RBD1=500+4(0~5) , RBD2=1000+4(0~12), RBD3=2048-RBD1-RBD2*/
			memset(&rgpd_format, 0 , sizeof(athif_ul_rgpd_format_t));			
			rgpd_format.rgpd_allow_len = 0;
			rgpd_format.rbd_num = 3;
			
			KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_3RBD_ALLOW_LEN, 3BD test start\n",__FUNCTION__,__LINE__ ));			
			for (rbd1_allow_len = 500 ; rbd1_allow_len <= 520 ; rbd1_allow_len += 4) {
				for (rbd2_allow_len = 1000 ; rbd2_allow_len <= 1048 ; rbd2_allow_len += 4) {
					rbd3_allow_len = 2048 - rbd1_allow_len - rbd2_allow_len;
					rgpd_format.rbd_allow_len[0] = rbd1_allow_len;
					rgpd_format.rbd_allow_len[1] = rbd2_allow_len;
					rgpd_format.rbd_allow_len[2] = rbd3_allow_len;
					KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_3RBD_ALLOW_LEN, 1BD test BD1=%d, BD2=%d, BD3=%d\n",
												__FUNCTION__,__LINE__ ,rbd1_allow_len, rbd2_allow_len, rbd3_allow_len));

					for (que_no = 0 ; que_no < HIF_MAX_ULQ_NUM ; que_no ++) {
						do {
							if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, sizeof(AT_PKT_HEADER), 20) != RET_SUCCESS)
								break;
                            if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 110, 140) != RET_SUCCESS)
							    break;
                            if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 240, 270) != RET_SUCCESS)
							    break;
							if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 500, 530) != RET_SUCCESS)
								break;
							if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 1010, 1040) != RET_SUCCESS)
								break;
							if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 1520, 1550) != RET_SUCCESS)
								break;
                            if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 2030, 2048) != RET_SUCCESS)
                                break;

						} while(0);
                        
						if (ret != RET_SUCCESS) {
							goto rd3_allowlen_case2_err;
							break;
						}
					}
				}
			}
			KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_3RBD_ALLOW_LEN, 3BD test passed\n",__FUNCTION__,__LINE__ ));
			
rd3_allowlen_case2_err:
			break;
            
		case ATCASE_2RBD_ALLOW_LEN_STRESS:
			
			loop = arg[1];
			memset(&rgpd_format, 0 , sizeof(athif_ul_rgpd_format_t));			
			rgpd_format.rgpd_allow_len = 0;
			rgpd_format.rbd_num = 2;
			
			KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_2RBD_ALLOW_LEN_STRESS, 2BD test start\n",__FUNCTION__,__LINE__ ));			
			tmp = 0;
			while (loop) {
				tmp += 4;
				rbd1_allow_len = 4 + tmp % 2044; // 4~2044
				rbd2_allow_len = 2048 - rbd1_allow_len; // 2044~4
				rgpd_format.rbd_allow_len[0] = rbd1_allow_len;
				rgpd_format.rbd_allow_len[1] = rbd2_allow_len;
				KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_2RBD_ALLOW_LEN_STRESS,loop=%d, 1BD test BD1=%d, BD2=%d\n",__FUNCTION__,__LINE__ ,loop,rbd1_allow_len, rbd2_allow_len));
				for (que_no = 0 ; que_no < HIF_MAX_ULQ_NUM ; que_no ++) {
					do {
						if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, sizeof(AT_PKT_HEADER), 20) != RET_SUCCESS)
							break;
                        if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 110, 140) != RET_SUCCESS)
							break;
                        if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 240, 270) != RET_SUCCESS)
							break;
						if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 500, 530) != RET_SUCCESS)
							break;
						if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 1010, 1040) != RET_SUCCESS)
							break;
						if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 1520, 1550) != RET_SUCCESS)
							break;
						if (ret = f_ul_rgpd_allow_len_tst(que_no ,&rgpd_format, 2030, 2048) != RET_SUCCESS)
							break;
                        
					} while(0);
                    
					if (ret != RET_SUCCESS) {
						break;
					}
				}
				if (ret != RET_SUCCESS) {
					break;
				}
				loop --;
			}

			KAL_DBGPRINT(KAL, DBG_ERROR,("[%s : %d] ATCASE_2RBD_ALLOW_LEN_STRESS, 2BD test passed\n",__FUNCTION__,__LINE__ ));
			
			break;
		default:
			break;
	}

    sdio_test_option.auto_receive_pkt = false;
    //sdio_test_option.show_dl_content = false;
    sdio_test_option.exam_dl_content = false;
    
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

	return ret;
}


static int t_dev_small_pkt_loopback(int argc, char** argv)
{
	unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pattern = 0;
	athif_cmd_t cmd;
	athif_status_t status;
	unsigned int rand_num = 0 , i = 0 , xfer_len = 0 ;
    unsigned int test_time;

	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    sdio_test_option.auto_receive_pkt = true;
    //sdio_test_option.show_dl_content = true;
    sdio_test_option.exam_dl_content = true;

	pattern = arg[0];
    test_time = arg[1];

    for(i=0; i<test_time; i++){
    	if (arg[0] > ATCASE_LB_DATA_INC) {
    		get_random_bytes(&rand_num, sizeof(unsigned int)) ;
            pattern = rand_num % (ATCASE_LB_DATA_INC+1);            
    	}else{
    	    pattern = arg[0];
    	}
        
    	/*issue auto-test command*/
    	cmd.cmd = ATHIF_CMD_SET_FWD_MD;
    	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
    	cmd.len = 1;

    	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    	
    	ret = f_small_pkt_lb(pattern);

    	/*get tx/rx packet count recorded in device*/
    	cmd.cmd = ATHIF_CMD_GET_PKT_CNT;
    	cmd.len = ATHIF_CMD_HD_LEN;

        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){
            return RET_FAIL;
        }else{
            KAL_DBGPRINT(KAL, DBG_WARN,("UL pkt cnt = %d, DL pkt cnt = %d\n",*(unsigned int*)athif_result_save_t->buf, *(unsigned int*)(athif_result_save_t->buf+4)));
        }

        if(ret != RET_SUCCESS){
            break;
        }
    }

    sdio_test_option.auto_receive_pkt = false;
    //sdio_test_option.show_dl_content = false;
    sdio_test_option.exam_dl_content = false;
    
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
	return ret;
}


// TODO: this test case is not done yet.
static int t_dev_misalign_loopback(int argc, char** argv)
{
	unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pattern = 0;
	athif_cmd_t cmd;
	athif_status_t status;
	unsigned int rand_num = 0 , i = 0 , xfer_len = 0 ;
    unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
    unsigned int expect_num = 0, timeout=0;
    athif_ul_rgpd_format_t *ul_gpd_format;
    unsigned int old_pkt_cnt = 0;
    unsigned int unalign_offset;
    unsigned int *p_unalign_offset;

	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;


		/*issue auto-test command*/

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    //cmd.cmd = ATHIF_CMD_CFG_SPECIFIC_RGPD_RL;
	//ul_gpd_format = cmd.buf;
    //ul_gpd_format->rgpd_allow_len = 2048;
    //ul_gpd_format->rbd_num = 2;
    //ul_gpd_format->rbd_allow_len[0] = 512;
    //ul_gpd_format->rbd_allow_len[1] = 1536;
	//cmd.len = sizeof(athif_ul_rgpd_format_t);

	//mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    //mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    //if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    unalign_offset = arg[2];
    if(unalign_offset == 0 || unalign_offset > 0xFF){
        unalign_offset = 0xFF;
    }
    cmd.cmd = ATHIF_CMD_SET_BUF_OFFSET;
	p_unalign_offset = (unsigned int *)cmd.buf;
    *p_unalign_offset = unalign_offset;
	cmd.len = 4;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
	cmd.buf[0] = ATHIF_RGPD_BUF_MISALIGN;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
		

    
    //for(pkt_sz=sizeof(AT_PKT_HEADER); pkt_sz<=2044; pkt_sz++){
    for(pkt_sz=arg[0]; pkt_sz<=arg[1]; pkt_sz++){
        for(test_que_no=0; test_que_no<HIF_MAX_ULQ_NUM; test_que_no++){
	        for(pkt_num=10; pkt_num<=20; pkt_num++){
			    for (idx = 0 ; idx < pkt_num ; idx ++) {
                    KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                        __FUNCTION__, test_que_no, pkt_sz, idx));
				    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
                    expect_num++;
                    
				    if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					    /*send packet fail and retry*/
                        idx --;
                        return ret;
				    } 
			    }
                KAL_SLEEP_MSEC(1) ;

                if(recv_th_rslt != RET_SUCCESS){
                    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] data error while device sending que=%d, pkt_size=%d, pkt_num=%d !!! \n", \
                        KAL_FUNC_NAME, test_que_no, pkt_sz, pkt_num)) ;
                    return RET_FAIL;
                }
                
            }
	    }
	}

    while(expect_num != recv_total_pkt_cnt){
        
        KAL_SLEEP_MSEC(1) ;
        if(recv_total_pkt_cnt != old_pkt_cnt){ timeout ==0; }
        else{ timeout++; }
        
        if(timeout > 1000){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;
            return RET_FAIL;
        }
        old_pkt_cnt = recv_total_pkt_cnt;
    }   

	/*get tx/rx packet count recorded in device*/
	cmd.cmd = ATHIF_CMD_GET_PKT_CNT;
	cmd.len = 0;

    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){
        return RET_FAIL;
    }else{
        KAL_DBGPRINT(KAL, DBG_WARN,("UL pkt cnt = %d, DL pkt cnt = %d\n",*(unsigned int*)athif_result_save_t->buf, *(unsigned int*)(athif_result_save_t->buf+4)));
    }
		
    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_FREE_ONLY;
    cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_BUF_OFFSET;
	p_unalign_offset = (unsigned int *)cmd.buf;
    *p_unalign_offset = 0;
	cmd.len = 4;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    sdio_test_option.auto_receive_pkt = false;
    //sdio_test_option.show_dl_content = false;
    sdio_test_option.exam_dl_content = false;
    
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
    
	if(recv_th_rslt == RET_SUCCESS){
	    return ret;
    }else{
        return RET_FAIL;
    }
}


static int t_dev_network_loopback(int argc, char** argv)
{
	unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pattern = 0;
	athif_cmd_t cmd;
	athif_status_t status;
	unsigned int rand_num = 0 , i = 0 , xfer_len = 0 ;
    unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
    unsigned int expect_num = 0, timeout=0;
    athif_ul_rgpd_format_t *ul_gpd_format;
    unsigned int old_pkt_cnt = 0;

	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;


		/*issue auto-test command*/

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    //cmd.cmd = ATHIF_CMD_CFG_SPECIFIC_RGPD_RL;
	//ul_gpd_format = cmd.buf;
    //ul_gpd_format->rgpd_allow_len = 2048;
    //ul_gpd_format->rbd_num = 2;
    //ul_gpd_format->rbd_allow_len[0] = 512;
    //ul_gpd_format->rbd_allow_len[1] = 1536;
	//cmd.len = sizeof(athif_ul_rgpd_format_t);

	//mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    //mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    //if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
	cmd.buf[0] = ATHIF_RGPD_ECM;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
		

    
    //for(pkt_sz=sizeof(AT_PKT_HEADER); pkt_sz<=2044; pkt_sz++){
    for(pkt_sz=arg[0]; pkt_sz<=arg[1]; pkt_sz++){
        for(test_que_no=0; test_que_no<HIF_MAX_ULQ_NUM; test_que_no++){
	        for(pkt_num=10; pkt_num<=20; pkt_num++){
			    for (idx = 0 ; idx < pkt_num ; idx ++) {
                    KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                        __FUNCTION__, test_que_no, pkt_sz, idx));
				    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
                    expect_num++;
                    
				    if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					    /*send packet fail and retry*/
                        idx --;
                        return ret;
				    } 
			    }
                KAL_SLEEP_MSEC(1) ;

                if(recv_th_rslt != RET_SUCCESS){
                    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] data error while device sending que=%d, pkt_size=%d, pkt_num=%d !!! \n", \
                        KAL_FUNC_NAME, test_que_no, pkt_sz, pkt_num)) ;
                    return RET_FAIL;
                }
                
            }
	    }
	}

    while(expect_num != recv_total_pkt_cnt){
        
        KAL_SLEEP_MSEC(1) ;
        if(recv_total_pkt_cnt != old_pkt_cnt){ timeout ==0; }
        else{ timeout++; }
        
        if(timeout > 1000){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;
            return RET_FAIL;
        }
        old_pkt_cnt = recv_total_pkt_cnt;
    }   

	/*get tx/rx packet count recorded in device*/
	cmd.cmd = ATHIF_CMD_GET_PKT_CNT;
	cmd.len = 0;

    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){
        return RET_FAIL;
    }else{
        KAL_DBGPRINT(KAL, DBG_WARN,("UL pkt cnt = %d, DL pkt cnt = %d\n",*(unsigned int*)athif_result_save_t->buf, *(unsigned int*)(athif_result_save_t->buf+4)));
    }
		
    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_FREE_ONLY;
    cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    sdio_test_option.auto_receive_pkt = false;
    //sdio_test_option.show_dl_content = false;
    sdio_test_option.exam_dl_content = false;
    
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
    
	if(recv_th_rslt == RET_SUCCESS){
	    return ret;
    }else{
        return RET_FAIL;
    }
}


static int t_dev_rand_enqueue_loopback(int argc, char** argv)
{
	unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pattern = 0;
	athif_cmd_t cmd;
	athif_status_t status;
	unsigned int rand_num = 0 , i = 0 , xfer_len = 0 ;
    unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
    unsigned int expect_num = 0, timeout=0;
    athif_ul_rgpd_format_t *ul_gpd_format;
    unsigned int old_pkt_cnt = 0;

	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;


		/*issue auto-test command*/

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    //cmd.cmd = ATHIF_CMD_CFG_SPECIFIC_RGPD_RL;
	//ul_gpd_format = cmd.buf;
    //ul_gpd_format->rgpd_allow_len = 2048;
    //ul_gpd_format->rbd_num = 2;
    //ul_gpd_format->rbd_allow_len[0] = 512;
    //ul_gpd_format->rbd_allow_len[1] = 1536;
	//cmd.len = sizeof(athif_ul_rgpd_format_t);

	//mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    //mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    //if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    /*set loopback type*/
	cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
	cmd.buf[0] = ATHIF_LB_TGPD_RANDOM;
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

	/*set RGPD reload type*/
	cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
	cmd.buf[0] = ATHIF_RGPD_RANDOM;
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
		

    
    //for(pkt_sz=sizeof(AT_PKT_HEADER); pkt_sz<=2044; pkt_sz++){
    for(pkt_sz=arg[0]; pkt_sz<=arg[1]; pkt_sz++){
        for(test_que_no=0; test_que_no<HIF_MAX_ULQ_NUM; test_que_no++){
	        for(pkt_num=10; pkt_num<=20; pkt_num++){
			    for (idx = 0 ; idx < pkt_num ; idx ++) {
                    KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                        __FUNCTION__, test_que_no, pkt_sz, idx));
				    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
                    expect_num++;
                    
				    if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					    /*send packet fail and retry*/
                        idx --;
                        return ret;
				    } 
			    }
                KAL_SLEEP_MSEC(1) ;

                if(recv_th_rslt != RET_SUCCESS){
                    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] data error while device sending que=%d, pkt_size=%d, pkt_num=%d !!! \n", \
                        KAL_FUNC_NAME, test_que_no, pkt_sz, pkt_num)) ;
                    return RET_FAIL;
                }
                
            }
	    }
	}

    while(expect_num != recv_total_pkt_cnt){
        
        KAL_SLEEP_MSEC(1) ;
        if(recv_total_pkt_cnt != old_pkt_cnt){ timeout ==0; }
        else{ timeout++; }
        
        if(timeout > 1000){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;
            return RET_FAIL;
        }
        old_pkt_cnt = recv_total_pkt_cnt;
    }   

	/*get tx/rx packet count recorded in device*/
	cmd.cmd = ATHIF_CMD_GET_PKT_CNT;
	cmd.len = 0;

    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){
        return RET_FAIL;
    }else{
        KAL_DBGPRINT(KAL, DBG_WARN,("UL pkt cnt = %d, DL pkt cnt = %d\n",*(unsigned int*)athif_result_save_t->buf, *(unsigned int*)(athif_result_save_t->buf+4)));
    }
		
    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_FREE_ONLY;
    cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    sdio_test_option.auto_receive_pkt = false;
    //sdio_test_option.show_dl_content = false;
    sdio_test_option.exam_dl_content = false;
    
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
    
	if(recv_th_rslt == RET_SUCCESS){
	    return ret;
    }else{
        return RET_FAIL;
    }
}


static int t_dev_tx_big_packet(int argc, char** argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
    athif_fwd_mode_e tx_deq_mode;
    athif_cmd_t cmd;
    unsigned int expect_num = 0, timeout=0;
    athif_ul_rgpd_tst_cfg_t *p_rgpd_cfg;

    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    tx_deq_mode = ATHIF_FWD_LOOPBACK;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    send_pattern = ATCASE_LB_DATA_AUTO;

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    cmd.cmd = SDIO_AT_BIG_SIZE;
	cmd.buf[0] = 0;
	cmd.len = 1;

    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    


    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;


	
    for(pkt_sz=4060; pkt_sz<=4091; pkt_sz++){ 

        at_mtlte_hif_sdio_clear_tx_count();

        cmd.cmd = SDIO_AT_UL_BIG_SIZE;
        p_rgpd_cfg = (athif_ul_rgpd_tst_cfg_t *)cmd.buf;
	    p_rgpd_cfg->q_num = 0;
	    /*must add one more gpd for queue initial tail*/
	    p_rgpd_cfg->gpd_num = 6;
	    p_rgpd_cfg->rgpd_format.rgpd_allow_len = 8192;
        p_rgpd_cfg->rgpd_format.rbd_num = 0;
        p_rgpd_cfg->rgpd_format.rbd_allow_len[0] = 0;
	    cmd.len = sizeof(athif_ul_rgpd_tst_cfg_t);

        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
                    
        for(test_que_no=0; test_que_no<HIF_MAX_ULQ_NUM; test_que_no++){

                    recv_total_pkt_cnt = 0;
                    expect_num = 0;
            
                    KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=3 ...\n", \
                        __FUNCTION__, test_que_no, pkt_sz));
				    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
                     expect_num++;
                    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
                     expect_num++;
                    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
                     expect_num++;
                    
				    if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					    /*send packet fail and retry*/
                        return ret;
				    }
                        
                KAL_SLEEP_MSEC(1) ;

                while(expect_num != recv_total_pkt_cnt){
                    KAL_SLEEP_MSEC(1) ;
                    timeout++;
                    if(timeout > 1000){
                        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;
                        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] test que =%d , test pkt size =%d !!! \n",KAL_FUNC_NAME, test_que_no, pkt_sz)) ;
                        return RET_FAIL;
                    }
                }

                if(recv_th_rslt != RET_SUCCESS){
                    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] data error at que=%d, pkt_sz=%d !!! \n",KAL_FUNC_NAME, test_que_no, pkt_sz)) ;
                    return RET_FAIL;
                }
                
	    }
	}

    
    cmd.cmd = SDIO_AT_BIG_SIZE;
	cmd.buf[0] = 1;
	cmd.len = 1;

    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    tx_deq_mode = ATHIF_FWD_FREE_ONLY;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    sdio_test_option.exam_dl_content = false;
    sdio_test_option.auto_receive_pkt = false;

	/*resume reload rgpd flow*/
    at_mtlte_hif_sdio_clear_tx_count();

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, 1, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

	return ret;
}


unsigned int testing_rx_big_packet = 0;

static int t_dev_rx_big_packet(int argc, char** argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
    athif_fwd_mode_e tx_deq_mode;
    athif_cmd_t cmd;
    unsigned int expect_num = 0, timeout=0;
    athif_ul_rgpd_tst_cfg_t *p_rgpd_cfg;
    athif_dl_tgpd_cfg_t *p_tgpd_cfg;
    unsigned int test_pkt_num;

    testing_rx_big_packet = 1;

    // temp set rx report length
    unsigned int orig_WHCR = 0;
    unsigned int changed_WHCR = 0;
    unsigned int orig_WPLRCR = 0;
    unsigned int changed_WPLRCR = 0;

    unsigned int test_rx_tail_change_bak = 0;
    unsigned int test_rx_pkt_cnt_q0_bak = 0;
    unsigned int test_rx_pkt_cnt_q1_bak = 0;
    unsigned int test_rx_pkt_cnt_q2_bak = 0;
    unsigned int test_rx_pkt_cnt_q3_bak = 0;
    
    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    // set max dl pkt to 2
    sdio_func1_rd(SDIO_IP_WHCR, &orig_WHCR, 4);
    sdio_func1_rd(SDIO_IP_WPLRCR, &orig_WPLRCR, 4);
    
    changed_WHCR = orig_WHCR | RPT_OWN_RX_PACKET_LEN;
    sdio_func1_wr(SDIO_IP_WHCR, &changed_WHCR, 4);

    changed_WPLRCR = orig_WPLRCR;
    changed_WPLRCR &= (~(RX_RPT_PKT_LEN(0)));
    changed_WPLRCR |= 2<<(8*0);;
    changed_WPLRCR &= (~(RX_RPT_PKT_LEN(1)));
    changed_WPLRCR |= 2<<(8*1);;
    changed_WPLRCR &= (~(RX_RPT_PKT_LEN(2)));
    changed_WPLRCR |= 2<<(8*2);;
    changed_WPLRCR &= (~(RX_RPT_PKT_LEN(3)));
    changed_WPLRCR |= 2<<(8*3);;

    sdio_func1_wr(SDIO_IP_WPLRCR, &changed_WPLRCR, 4);

    test_rx_tail_change_bak = test_rx_tail_change;
    test_rx_pkt_cnt_q0_bak = test_rx_pkt_cnt_q0;
    test_rx_pkt_cnt_q1_bak = test_rx_pkt_cnt_q1;
    test_rx_pkt_cnt_q2_bak = test_rx_pkt_cnt_q2;
    test_rx_pkt_cnt_q3_bak = test_rx_pkt_cnt_q3;

    test_rx_tail_change = 1;
    test_rx_pkt_cnt_q0 = 2;
    test_rx_pkt_cnt_q1 = 2;
    test_rx_pkt_cnt_q2 = 2;
    test_rx_pkt_cnt_q3 = 2;

    tx_deq_mode = ATHIF_FWD_LOOPBACK;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    send_pattern = ATCASE_LB_DATA_AUTO;

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    cmd.cmd = SDIO_AT_BIG_SIZE;
	cmd.buf[0] = 0;
	cmd.len = 1;

    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}


    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;
    

    for(pkt_sz=30000; pkt_sz<=30005; pkt_sz++){ 

        at_mtlte_hif_sdio_clear_tx_count();

        recv_total_pkt_cnt = 0;
        expect_num = 0;

        cmd.cmd = SDIO_AT_DL_BIG_SIZE;
        p_tgpd_cfg = (athif_dl_tgpd_cfg_t *)cmd.buf;
	    p_tgpd_cfg->q_num = 0;
	    p_tgpd_cfg->gpd_num = 3;
	    p_tgpd_cfg->tgpd_format.tgpd_buf_len = pkt_sz;
        p_tgpd_cfg->tgpd_format.tgpd_ext_len = 0;
        p_tgpd_cfg->tgpd_format.tbd_num = 0;
	    cmd.len = sizeof(athif_dl_tgpd_cfg_t);

        for(test_que_no=0; test_que_no<HIF_MAX_DLQ_NUM; test_que_no++){
          
                     expect_num++;
                     expect_num++;
                     expect_num++;
        }

        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
                    
                        
        KAL_SLEEP_MSEC(1) ;

        while(expect_num != recv_total_pkt_cnt){
                    KAL_SLEEP_MSEC(1) ;
                    timeout++;
                    if(timeout > 1000){
                        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;
                        return RET_FAIL;
                    }
        }

                //KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:receive pkt =%d But expect pkt =%d ... \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;

        if(recv_th_rslt != RET_SUCCESS){
                    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] data error at que=%d, pkt_sz=%d !!! \n",KAL_FUNC_NAME, test_que_no, pkt_sz)) ;
                    return RET_FAIL;
        }
                
	}

    KAL_SLEEP_MSEC(500);

    changed_WPLRCR = orig_WPLRCR;
    changed_WPLRCR &= (~(RX_RPT_PKT_LEN(0)));
    changed_WPLRCR |= 1<<(8*0);;
    changed_WPLRCR &= (~(RX_RPT_PKT_LEN(1)));
    changed_WPLRCR |= 1<<(8*1);;
    changed_WPLRCR &= (~(RX_RPT_PKT_LEN(2)));
    changed_WPLRCR |= 1<<(8*2);;
    changed_WPLRCR &= (~(RX_RPT_PKT_LEN(3)));
    changed_WPLRCR |= 1<<(8*3);;

    sdio_func1_wr(SDIO_IP_WPLRCR, &changed_WPLRCR, 4);
    

    for(pkt_sz=65000; pkt_sz<=65005; pkt_sz++){ 

        at_mtlte_hif_sdio_clear_tx_count();

        recv_total_pkt_cnt = 0;
        expect_num = 0;

        cmd.cmd = SDIO_AT_DL_BIG_SIZE;
        p_tgpd_cfg = (athif_dl_tgpd_cfg_t *)cmd.buf;
	    p_tgpd_cfg->q_num = 0;
	    p_tgpd_cfg->gpd_num = 3;
	    p_tgpd_cfg->tgpd_format.tgpd_buf_len = pkt_sz;
        p_tgpd_cfg->tgpd_format.tgpd_ext_len = 0;
        p_tgpd_cfg->tgpd_format.tbd_num = 0;
	    cmd.len = sizeof(athif_dl_tgpd_cfg_t);

        for(test_que_no=0; test_que_no<HIF_MAX_DLQ_NUM; test_que_no++){
          
                     expect_num++;
                     expect_num++;
                     expect_num++;
        }

        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
                    
                        
        KAL_SLEEP_MSEC(1) ;

        while(expect_num != recv_total_pkt_cnt){
                    KAL_SLEEP_MSEC(1) ;
                    timeout++;
                    if(timeout > 1000){
                        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;
                        return RET_FAIL;
                    }
        }

                //KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:receive pkt =%d But expect pkt =%d ... \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;

        if(recv_th_rslt != RET_SUCCESS){
                    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] data error at que=%d, pkt_sz=%d !!! \n",KAL_FUNC_NAME, test_que_no, pkt_sz)) ;
                    return RET_FAIL;
        }
                
	}
    
    cmd.cmd = SDIO_AT_BIG_SIZE;
	cmd.buf[0] = 1;
	cmd.len = 1;

    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    tx_deq_mode = ATHIF_FWD_FREE_ONLY;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    sdio_test_option.exam_dl_content = false;
    sdio_test_option.auto_receive_pkt = false;

	/*resume reload rgpd flow*/
    at_mtlte_hif_sdio_clear_tx_count();

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}


    sdio_func1_wr(SDIO_IP_WHCR, &orig_WHCR, 4);
    sdio_func1_wr(SDIO_IP_WPLRCR, &orig_WPLRCR, 4);

    test_rx_pkt_cnt_q0 = test_rx_pkt_cnt_q0_bak;
    test_rx_pkt_cnt_q1 = test_rx_pkt_cnt_q1_bak;
    test_rx_pkt_cnt_q2 = test_rx_pkt_cnt_q2_bak;
    test_rx_pkt_cnt_q3 = test_rx_pkt_cnt_q3_bak;
    test_rx_tail_change = test_rx_tail_change_bak;

    testing_rx_big_packet = 0;

    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

	return ret;
}


static int t_dev_fw_own_err(int argc, char** argv)
{
    int ret = RET_SUCCESS;
    athif_test_param_t    attest_param;
    kal_uint32  abnormal_st;
    kal_uint32  timeout;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    sdio_func1_rd(SDIO_IP_WASR, &abnormal_st, 4) ;	
    
    if( 0 != abnormal_st){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] abnormal register is not right before test, WASR = %x !!! \n",KAL_FUNC_NAME, abnormal_st)) ;
        return RET_FAIL;
    }

    attest_param = at_mtlte_hif_get_test_param();
    attest_param.abnormal_status= 0;
    at_mtlte_hif_set_test_param(attest_param, set_abnormal_status);

    KAL_SLEEP_MSEC(1) ;
    mtlte_hif_sdio_enable_fw_own_back(1);
    KAL_SLEEP_MSEC(1) ;
    at_mtlte_hif_sdio_give_fw_own();

    KAL_SLEEP_MSEC(1) ;
    sdio_func1_rd(SDIO_IP_WASR, &abnormal_st, 4);
    KAL_SLEEP_MSEC(1) ;
    
    attest_param = at_mtlte_hif_get_test_param();
    timeout = 0;
    while((attest_param.abnormal_status & 0x00010000) == 0){
        
        timeout++;
        if(timeout > 100){
            KAL_DBGPRINT(KAL, DBG_ERROR,("%s : The FW_OWN_INVALD_ACCESS INT not triggered!\n", KAL_FUNC_NAME));
		    return RET_FAIL ;	
        }
        KAL_SLEEP_MSEC(1) ;
        attest_param = at_mtlte_hif_get_test_param();
    }
        
	return ret;
}


static int t_dev_ul_overflow_err(int argc, char** argv)
{
    int ret = RET_SUCCESS;
    athif_test_param_t    attest_param;
    kal_uint32  i, j;
    kal_uint32  *tx_packet_header;
    kal_uint32  abnormal_st;
    kal_uint32  timeout;
    kal_uint32  test_que_no;
    kal_uint32  test_port_no;
    kal_uint32  test_isr_mask, orig_isr_mask;
    athif_cmd_t cmd;
    athif_ul_rgpd_tst_cfg_t *gpd_format;
    athif_mem_tst_cfg_t *rw_arg;
    PAT_PKT_HEADER pAtHeader = NULL;
    unsigned char *buf;
    unsigned char cksm = 0;
    unsigned char rand_seed = 0;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

        //Set interrupt test mode flag
    cmd.cmd = SDIO_AT_DL_INT_TEST_SWITCH;
	cmd.buf[0] = 1; // 1 : test mode , 0 : normal mode
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    for(test_que_no=0; test_que_no<HIF_MAX_ULQ_NUM; test_que_no++){

        // now we have no way to clean tx fifo
        //if(test_que_no >= 2){ break;}
        
        test_port_no = 1;
        
        sdio_func1_rd(SDIO_IP_WASR, &abnormal_st, 4) ; 
        if( 0 != abnormal_st){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] abnormal register is not right before test, WASR = %x !!! \n",KAL_FUNC_NAME, abnormal_st)) ;
            return RET_FAIL;
            }

        attest_param = at_mtlte_hif_get_test_param();
        attest_param.abnormal_status= 0;
        at_mtlte_hif_set_test_param(attest_param, set_abnormal_status);

        //Stop reload uplink GPD task
        cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	    cmd.buf[0] = 1; // 1 : pause , 0 : resume
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
        
        cmd.cmd = ATHIF_CMD_PREPARE_RGPD;
        gpd_format = (athif_ul_rgpd_tst_cfg_t *)cmd.buf;
        gpd_format->q_num = test_que_no;
        gpd_format->gpd_num = 2;
        gpd_format->rgpd_format.rgpd_allow_len = 2000;
        gpd_format->rgpd_format.rbd_num = 0;
	    cmd.len = sizeof(athif_ul_rgpd_tst_cfg_t);
	    
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        cmd.cmd = ATHIF_CMD_WRITE_MEM;
        rw_arg = (athif_mem_tst_cfg_t *)cmd.buf;
        rw_arg->mem_addr = (kal_uint32)ORG_SDIO_HWTPCCR;
        *(unsigned int*)rw_arg->mem_val = test_que_no <<(12);
        *(unsigned int*)rw_arg->mem_val = *(unsigned int*)rw_arg->mem_val + 1;
        rw_arg->len = 4;
	    cmd.len = sizeof(athif_mem_tst_cfg_t) + 4;
        
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        attest_param = at_mtlte_hif_get_test_param();
        timeout = 0;

        // TODO:  gerenate pattern of 3 packet
        buf = buff_kmemory_ulpkt_data;
        memset(buf, 0 , 1536);
        
        for(i=0; i<3; i++){

            tx_packet_header = (kal_uint32 *)(buf + 512*i);
            if(test_que_no == 0){
                *tx_packet_header = 0;
            }else{
                *tx_packet_header = (test_que_no-1) << 29;
            }
            //*tx_packet_header = *tx_packet_header + 508;  
            *tx_packet_header = *tx_packet_header + 32; 
            
            pAtHeader = (PAT_PKT_HEADER)(buf + 512*i + 4);
			memset(pAtHeader, 0 , sizeof(AT_PKT_HEADER));

            rand_seed = i;
			pAtHeader->RndSeed = rand_seed;
			pAtHeader->SrcQID = 0;  
			pAtHeader->DstQID = 0;
			pAtHeader->SeqNo = i;
			pAtHeader->PktLen = 508;
			
			f_calc_cs_byte(pAtHeader, sizeof(AT_PKT_HEADER), &cksm);
			pAtHeader->Checksum = ~cksm;

			 // fill payload, don't fill memory lenght larger than URB buffer
			for (j = 0 ; j < (508 - sizeof(AT_PKT_HEADER)) ; j ++) {
				pAtHeader->Data[j] = rand_seed++;
			}
			break;
        }
        
        //if(test_que_no == 0){
        //    sdio_func1_wr(SDIO_IP_WTDR0, buf, 1536) ;
        //}else{
            sdio_func1_wr(SDIO_IP_WTDR1, buf, 1536) ;
        //}
        memset(buf, 0 , 1536);
        
        while((attest_param.abnormal_status & (0x1<<(test_port_no)) ) == 0){
        
            timeout++;
            if(timeout > 100){
                KAL_DBGPRINT(KAL, DBG_ERROR,("%s : The Tx%d overflow INT not triggered!\n", KAL_FUNC_NAME, test_que_no));
		        return RET_FAIL ;	
            }
            KAL_SLEEP_MSEC(1) ;
            attest_param = at_mtlte_hif_get_test_param();
        }

        at_mtlte_hif_sdio_clear_tx_count();

        //mask normal downlink interrupt again & recover the queue state
        cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	    cmd.buf[0] = 1; // 1 : pause , 0 : resume
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        
    }

    cmd.cmd = SDIO_AT_DL_INT_TEST_SWITCH;
	cmd.buf[0] = 0; // 1 : test mode , 0 : normal mode
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
        
    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    return ret;
}



static int t_dev_dl_underflow_err(int argc, char** argv)
{
    int ret = RET_SUCCESS;
    athif_test_param_t    attest_param;
    kal_uint32  abnormal_st;
    kal_uint32  timeout;
    kal_uint32  test_que_no;
    kal_uint32  test_isr_mask, orig_isr_mask;
    athif_cmd_t cmd;
    athif_dl_tgpd_cfg_t *gpd_format;
    athif_mem_tst_cfg_t *rw_arg;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    //Set interrupt test mode flag
    cmd.cmd = SDIO_AT_DL_INT_TEST_SWITCH;
	cmd.buf[0] = 1; // 1 : test mode , 0 : normal mode
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    for(test_que_no=0; test_que_no<HIF_MAX_DLQ_NUM; test_que_no++){

        // check the abnormal interrupt status before test
        sdio_func1_rd(SDIO_IP_WASR, &abnormal_st, 4) ;	
        if( 0 != abnormal_st){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] abnormal register is not right before Rx%d test, WASR = %x !!! \n",KAL_FUNC_NAME, test_que_no, abnormal_st)) ;
            return RET_FAIL;
        }

        attest_param = at_mtlte_hif_get_test_param();
        attest_param.abnormal_status= 0;
        attest_param.testing_dlq_int = 1;
        attest_param.testing_dlq_pkt_fifo = 1;
        at_mtlte_hif_set_test_param(attest_param, set_abnormal_status);
        at_mtlte_hif_set_test_param(attest_param, set_testing_dlq_int);
        at_mtlte_hif_set_test_param(attest_param, set_testing_dlq_pkt_fifo);

        //mask normal downlink interrupt
        sdio_func1_rd(SDIO_IP_WHIER, &orig_isr_mask, 4) ;
        test_isr_mask = orig_isr_mask & (~( 0x2<<(test_que_no)));
        sdio_func1_wr(SDIO_IP_WHIER, &test_isr_mask, 4) ;

        // let device sent few normal packets
        cmd.cmd = ATHIF_CMD_DL_SEND_N;
        gpd_format = (athif_dl_tgpd_cfg_t *)cmd.buf;
        gpd_format->q_num = test_que_no;
        gpd_format->gpd_num = 2;
        gpd_format->tgpd_format.tgpd_buf_len = 555;
        gpd_format->tgpd_format.tgpd_ext_len = 0;
        gpd_format->tgpd_format.tbd_num = 0;
	    cmd.len = sizeof(athif_dl_tgpd_cfg_t);
	    
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        // set a non-exist packet length so the situation of underflow can be create
        cmd.cmd = ATHIF_CMD_WRITE_MEM;
        rw_arg = (athif_mem_tst_cfg_t *)cmd.buf;
        rw_arg->mem_addr = (kal_uint32)ORG_SDIO_RQCR_n(test_que_no);
        *(unsigned int*)rw_arg->mem_val = 0x1<<(18);
        *(unsigned int*)rw_arg->mem_val = *(unsigned int*)rw_arg->mem_val + 567;
        rw_arg->len = 4;
	    cmd.len = sizeof(athif_mem_tst_cfg_t) + 4;
        
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        // unmask the normal interrupt to trigger host to read DL packet
        attest_param.testing_dlq_int = 0;
        attest_param.testing_dlq_pkt_fifo = 0;
        at_mtlte_hif_set_test_param(attest_param, set_testing_dlq_int);
        at_mtlte_hif_set_test_param(attest_param, set_testing_dlq_pkt_fifo);

        test_isr_mask |= ( 0x2<<(test_que_no));
        sdio_func1_wr(SDIO_IP_WHIER, &test_isr_mask, 4) ;

        attest_param = at_mtlte_hif_get_test_param();
        timeout = 0;
        while((attest_param.abnormal_status & (0x100<<(test_que_no)) ) == 0){
        
            timeout++;
            if(timeout > 100){
                KAL_DBGPRINT(KAL, DBG_ERROR,("%s : The Rx%d Underflow INT not triggered!\n", KAL_FUNC_NAME, test_que_no));
		        return RET_FAIL ;	
            }
            KAL_SLEEP_MSEC(1) ;
            attest_param = at_mtlte_hif_get_test_param();
        }

        //mask normal downlink interrupt again & recover the queue state
        sdio_func1_rd(SDIO_IP_WHIER, &orig_isr_mask, 4) ;
        test_isr_mask = orig_isr_mask & (~( 0x2<<(test_que_no)));
        sdio_func1_wr(SDIO_IP_WHIER, &test_isr_mask, 4) ;


        cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	    cmd.buf[0] = 1; // 1 : pause , 0 : resume
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        test_isr_mask |= ( 0x2<<(test_que_no));
        sdio_func1_wr(SDIO_IP_WHIER, &test_isr_mask, 4) ;

        cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	    cmd.buf[0] = 0; // 1 : pause , 0 : resume
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        
    }

    cmd.cmd = SDIO_AT_DL_INT_TEST_SWITCH;
	cmd.buf[0] = 0; // 1 : test mode , 0 : normal mode
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        
    sdio_func1_wr(SDIO_IP_WHIER, &orig_isr_mask, 4) ;


    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    return ret;
}


static int t_dev_dl_fifolen_overflow_err(int argc, char** argv)
{
    int ret = RET_SUCCESS;
    athif_test_param_t    attest_param;
    kal_uint32  abnormal_st;
    kal_uint32  timeout;
    kal_uint32  test_que_no;
    kal_uint32  test_isr_mask, orig_isr_mask;
    athif_cmd_t cmd;
    athif_dl_tgpd_cfg_t *gpd_format;
    athif_mem_tst_cfg_t *rw_arg;
    hifsdio_isr_status_t *device_int_st;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    for(test_que_no=0; test_que_no<HIF_MAX_DLQ_NUM; test_que_no++){


        attest_param = at_mtlte_hif_get_test_param();
        attest_param.testing_dlq_int = 1;
        attest_param.testing_dlq_pkt_fifo = 1;
        at_mtlte_hif_set_test_param(attest_param, set_testing_dlq_int);
        at_mtlte_hif_set_test_param(attest_param, set_testing_dlq_pkt_fifo);

        cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	    cmd.buf[0] = 1; // 1 : pause , 0 : resume
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        cmd.cmd = SDIO_AT_RESET_DL_QUEUE;
	    cmd.buf[0] = test_que_no; 
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        //Set interrupt test mode flag
        cmd.cmd = SDIO_AT_DL_INT_TEST_SWITCH;
	    cmd.buf[0] = 1; // 1 : test mode , 0 : normal mode
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        //mask normal downlink interrupt
        sdio_func1_rd(SDIO_IP_WHIER, &orig_isr_mask, 4) ;
        test_isr_mask = orig_isr_mask & (~( 0x2<<(test_que_no)));
        sdio_func1_wr(SDIO_IP_WHIER, &test_isr_mask, 4) ;

        // let device sent HW_MAX_DL_PKT_CNT normal packets
        cmd.cmd = ATHIF_CMD_DL_SEND_N;
        gpd_format = (athif_dl_tgpd_cfg_t *)cmd.buf;
        gpd_format->q_num = test_que_no;
        gpd_format->gpd_num = HW_MAX_DL_PKT_CNT;
        gpd_format->tgpd_format.tgpd_buf_len = 555;
        gpd_format->tgpd_format.tgpd_ext_len = 0;
        gpd_format->tgpd_format.tbd_num = 0;
	    cmd.len = sizeof(athif_dl_tgpd_cfg_t);
	    
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        // set a non-exist packet length so the situation of length fifo overflow can be create
        cmd.cmd = ATHIF_CMD_WRITE_MEM;
        rw_arg = (athif_mem_tst_cfg_t *)cmd.buf;
        rw_arg->mem_addr = (kal_uint32)ORG_SDIO_RQCR_n(test_que_no);
        *(unsigned int*)rw_arg->mem_val = 0x1<<(18);
        *(unsigned int*)rw_arg->mem_val = *(unsigned int*)rw_arg->mem_val + 567;
        rw_arg->len = 4;
	    cmd.len = sizeof(athif_mem_tst_cfg_t) + 4;
        
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}


        /*get qmu interrupt info and check whether it meet expect*/
        cmd.cmd = SDIO_AT_READ_INT_STATUS;
	    cmd.buf[0] = 0; 
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;} 

        device_int_st = (hifsdio_isr_status_t *)athif_result_save_t->buf;

        if ( (device_int_st->DL0_INTR_Status & 0x1<<(24+test_que_no)) == 0 ) {
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] DL LENFIFO Overflow ERR interrupt check fail, q_num=%d !\n"
                                                ,__FUNCTION__,__LINE__, test_que_no));
            ret = RET_FAIL;
            return ret;
        } else {
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] DL LENFIFO Overflow interrupt check success, q_num=%d !\n"
                                                ,__FUNCTION__,__LINE__, test_que_no));
        }  

        cmd.cmd = SDIO_AT_RESET_DL_QUEUE;
	    cmd.buf[0] = test_que_no; 
	    cmd.len = 1;

	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        cmd.cmd = SDIO_AT_DL_INT_TEST_SWITCH;
	    cmd.buf[0] = 0; // 1 : test mode , 0 : normal mode
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	    cmd.buf[0] = 0; // 1 : pause , 0 : resume
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        attest_param.testing_dlq_int = 0;
        attest_param.testing_dlq_pkt_fifo = 0;
        at_mtlte_hif_set_test_param(attest_param, set_testing_dlq_int);
        at_mtlte_hif_set_test_param(attest_param, set_testing_dlq_pkt_fifo);

        // unmask the normal interrupt 
        sdio_func1_wr(SDIO_IP_WHIER, &orig_isr_mask, 4) ;

        
    }
    sdio_func1_wr(SDIO_IP_WHIER, &orig_isr_mask, 4) ;


    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    return ret;
}


// TODO: finisih the auto test flow of this test
static int t_dev_dl_write_timeout(int argc, char** argv)
{
    int ret = RET_SUCCESS;
    athif_test_param_t    attest_param;
    kal_uint32  abnormal_st;
    kal_uint32  timeout;
    kal_uint32  test_que_no;
    kal_uint32  test_isr_mask, orig_isr_mask;
    athif_cmd_t cmd;
    athif_dl_tgpd_cfg_t *gpd_format;
    athif_mem_tst_cfg_t *rw_arg;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    for(test_que_no=0; test_que_no<HIF_MAX_DLQ_NUM; test_que_no++){

        // check the abnormal interrupt status before test
        sdio_func1_rd(SDIO_IP_WASR, &abnormal_st, 4) ;	
        if( 0 != abnormal_st){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] abnormal register is not right before Rx%d test, WASR = %x !!! \n",KAL_FUNC_NAME, test_que_no, abnormal_st)) ;
            return RET_FAIL;
            }

        attest_param = at_mtlte_hif_get_test_param();
        attest_param.abnormal_status= 0;
        attest_param.testing_dlq_int = 1;
        attest_param.testing_dlq_pkt_fifo = 1;
        at_mtlte_hif_set_test_param(attest_param, set_abnormal_status);
        at_mtlte_hif_set_test_param(attest_param, set_testing_dlq_int);
        at_mtlte_hif_set_test_param(attest_param, set_testing_dlq_pkt_fifo);

        //mask normal downlink interrupt
        sdio_func1_rd(SDIO_IP_WHIER, &orig_isr_mask, 4) ;
        test_isr_mask = orig_isr_mask & (~( 0x2<<(test_que_no)));
        sdio_func1_wr(SDIO_IP_WHIER, &test_isr_mask, 4) ;

        // let device sent few normal packets
        cmd.cmd = ATHIF_CMD_DL_SEND_N;
        gpd_format = (athif_dl_tgpd_cfg_t *)cmd.buf;
        gpd_format->q_num = test_que_no;
        gpd_format->gpd_num = 2;
        gpd_format->tgpd_format.tgpd_buf_len = 555;
        gpd_format->tgpd_format.tgpd_ext_len = 0;
        gpd_format->tgpd_format.tbd_num = 0;
	    cmd.len = sizeof(athif_dl_tgpd_cfg_t);
	    
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        // set a non-exist packet length so the situation of underflow can be create
        cmd.cmd = ATHIF_CMD_WRITE_MEM;
        rw_arg = (athif_mem_tst_cfg_t *)cmd.buf;
        rw_arg->mem_addr = (kal_uint32)ORG_SDIO_RQCR_n(test_que_no);
        *(unsigned int*)rw_arg->mem_val = 0x1<<(18);
        *(unsigned int*)rw_arg->mem_val = *(unsigned int*)rw_arg->mem_val + 567;
        rw_arg->len = 4;
	    cmd.len = sizeof(athif_mem_tst_cfg_t) + 4;
        
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        // unmask the normal interrupt to trigger host to read DL packet
        attest_param.testing_dlq_int = 0;
        attest_param.testing_dlq_pkt_fifo = 0;
        at_mtlte_hif_set_test_param(attest_param, set_testing_dlq_int);
        at_mtlte_hif_set_test_param(attest_param, set_testing_dlq_pkt_fifo);

        test_isr_mask |= ( 0x2<<(test_que_no));
        sdio_func1_wr(SDIO_IP_WHIER, &test_isr_mask, 4) ;

        attest_param = at_mtlte_hif_get_test_param();
        timeout = 0;
        while((attest_param.abnormal_status & (0x100<<(test_que_no)) ) == 0){
        
            timeout++;
            if(timeout > 100){
                KAL_DBGPRINT(KAL, DBG_ERROR,("%s : The Rx%d Underflow INT not triggered!\n", KAL_FUNC_NAME, test_que_no));
		        return RET_FAIL ;	
            }
            KAL_SLEEP_MSEC(1) ;
            attest_param = at_mtlte_hif_get_test_param();
        }

        //mask normal downlink interrupt again & recover the queue state
        sdio_func1_rd(SDIO_IP_WHIER, &orig_isr_mask, 4) ;
        test_isr_mask = orig_isr_mask & (~( 0x2<<(test_que_no)));
        sdio_func1_wr(SDIO_IP_WHIER, &test_isr_mask, 4) ;

        cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	    cmd.buf[0] = 1; // 1 : pause , 0 : resume
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        test_isr_mask |= ( 0x2<<(test_que_no));
        sdio_func1_wr(SDIO_IP_WHIER, &test_isr_mask, 4) ;

        cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	    cmd.buf[0] = 0; // 1 : pause , 0 : resume
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        
    }
    sdio_func1_wr(SDIO_IP_WHIER, &orig_isr_mask, 4) ;


    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    return ret;
}

// TODO: finisih the auto test flow of this test
static int t_dev_dl_read_timeout(int argc, char** argv)
{
    int ret = RET_SUCCESS;
    athif_test_param_t    attest_param;
    kal_uint32  abnormal_st;
    kal_uint32  timeout;
    kal_uint32  test_que_no;
    kal_uint32  test_isr_mask, orig_isr_mask;
    athif_cmd_t cmd;
    athif_dl_tgpd_cfg_t *gpd_format;
    athif_mem_tst_cfg_t *rw_arg;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    for(test_que_no=0; test_que_no<HIF_MAX_DLQ_NUM; test_que_no++){

        // check the abnormal interrupt status before test
        sdio_func1_rd(SDIO_IP_WASR, &abnormal_st, 4) ;	
        if( 0 != abnormal_st){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] abnormal register is not right before Rx%d test, WASR = %x !!! \n",KAL_FUNC_NAME, test_que_no, abnormal_st)) ;
            return RET_FAIL;
            }

        attest_param = at_mtlte_hif_get_test_param();
        attest_param.abnormal_status= 0;
        attest_param.testing_dlq_int = 1;
        attest_param.testing_dlq_pkt_fifo = 1;
        at_mtlte_hif_set_test_param(attest_param, set_abnormal_status);
        at_mtlte_hif_set_test_param(attest_param, set_testing_dlq_int);
        at_mtlte_hif_set_test_param(attest_param, set_testing_dlq_pkt_fifo);

        //mask normal downlink interrupt
        sdio_func1_rd(SDIO_IP_WHIER, &orig_isr_mask, 4) ;
        test_isr_mask = orig_isr_mask & (~( 0x2<<(test_que_no)));
        sdio_func1_wr(SDIO_IP_WHIER, &test_isr_mask, 4) ;

        // let device sent few normal packets
        cmd.cmd = ATHIF_CMD_DL_SEND_N;
        gpd_format = (athif_dl_tgpd_cfg_t *)cmd.buf;
        gpd_format->q_num = test_que_no;
        gpd_format->gpd_num = 2;
        gpd_format->tgpd_format.tgpd_buf_len = 555;
        gpd_format->tgpd_format.tgpd_ext_len = 0;
        gpd_format->tgpd_format.tbd_num = 0;
	    cmd.len = sizeof(athif_dl_tgpd_cfg_t);
	    
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        // set a non-exist packet length so the situation of underflow can be create
        cmd.cmd = ATHIF_CMD_WRITE_MEM;
        rw_arg = (athif_mem_tst_cfg_t *)cmd.buf;
        rw_arg->mem_addr = (kal_uint32)ORG_SDIO_RQCR_n(test_que_no);
        *(unsigned int*)rw_arg->mem_val = 0x1<<(18);
        *(unsigned int*)rw_arg->mem_val = *(unsigned int*)rw_arg->mem_val + 567;
        rw_arg->len = 4;
	    cmd.len = sizeof(athif_mem_tst_cfg_t) + 4;
        
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        // unmask the normal interrupt to trigger host to read DL packet
        attest_param.testing_dlq_int = 0;
        attest_param.testing_dlq_pkt_fifo = 0;
        at_mtlte_hif_set_test_param(attest_param, set_testing_dlq_int);
        at_mtlte_hif_set_test_param(attest_param, set_testing_dlq_pkt_fifo);

        test_isr_mask |= ( 0x2<<(test_que_no));
        sdio_func1_wr(SDIO_IP_WHIER, &test_isr_mask, 4) ;

        attest_param = at_mtlte_hif_get_test_param();
        timeout = 0;
        while((attest_param.abnormal_status & (0x100<<(test_que_no)) ) == 0){
        
            timeout++;
            if(timeout > 100){
                KAL_DBGPRINT(KAL, DBG_ERROR,("%s : The Rx%d Underflow INT not triggered!\n", KAL_FUNC_NAME, test_que_no));
		        return RET_FAIL ;	
            }
            KAL_SLEEP_MSEC(1) ;
            attest_param = at_mtlte_hif_get_test_param();
        }

        //mask normal downlink interrupt again & recover the queue state
        sdio_func1_rd(SDIO_IP_WHIER, &orig_isr_mask, 4) ;
        test_isr_mask = orig_isr_mask & (~( 0x2<<(test_que_no)));
        sdio_func1_wr(SDIO_IP_WHIER, &test_isr_mask, 4) ;

        cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	    cmd.buf[0] = 1; // 1 : pause , 0 : resume
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        test_isr_mask |= ( 0x2<<(test_que_no));
        sdio_func1_wr(SDIO_IP_WHIER, &test_isr_mask, 4) ;

        cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	    cmd.buf[0] = 0; // 1 : pause , 0 : resume
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        
    }
    sdio_func1_wr(SDIO_IP_WHIER, &orig_isr_mask, 4) ;


    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    return ret;
}



static int t_dev_perf(int argc, char **argv)
{
	unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	unsigned int ep_idx = 0;
	int ret = RET_SUCCESS;
	unsigned int testmode = 0, LoopbackMode = 0 , loop = 0 , pkt_num = 0 , q_num = 0, pkt_sz = 0,q_md = 0 , pkt_md = 0;
	athif_cmd_t cmd;
	athif_status_t status;
	athif_ul_rgpd_format_t *p_rl_rgpd_format;
    unsigned int unalign_offset;
    unsigned int *p_unalign_offset;

	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

	/*set default compare mode*/
	cmp_pattern = ATCASE_LB_DATA_AUTO;
	/*SDIO has no ZLP*/
	// send_no_zlp = 0;

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    
	/*pause rx reload*/
	cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
        
	/*set allocate RX GPD type*/
	cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
	cmd.buf[0] = ATHIF_RGPD_BASIC;
	cmd.len =  1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}	

    at_mtlte_hif_sdio_clear_tx_count();

    /*resume rx reload for new buffer type*/
	cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;} 

	testmode = arg[0];
	switch (testmode) {
		case ATCASE_PERF_TX :
			/*issue auto-test command*/
			cmd.cmd = ATHIF_CMD_SET_FWD_MD;
			cmd.buf[0] = ATHIF_FWD_FREE_ONLY;
			cmd.len = 1;

			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}	

            /*start tx/rx transfer*/
			q_md = arg[1];
			loop = arg[2];
			pkt_md = arg[3];
			pkt_sz = arg[4];
			
			ret = f_tx_rx_ep0_perf_lb(loop, 0, pkt_md, q_md, pkt_sz, ATCASE_PERF_TX);

			/*get tx/rx packet count recorded in device*/
			cmd.cmd = ATHIF_CMD_GET_PKT_CNT;
			cmd.len = 0;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            else {
				KAL_DBGPRINT(KAL, DBG_WARN,("UL pkt cnt = %d, DL pkt cnt = %d\n",*(unsigned int*)athif_result_save_t->buf, *(unsigned int*)(athif_result_save_t->buf+4)));
            }
			break;
		case ATCASE_PERF_TX_DEV_CMP:
			/*issue auto-test command*/
			cmd.cmd = ATHIF_CMD_SET_FWD_MD;
			cmd.buf[0] = ATHIF_FWD_CMP_DATA;
			cmd.len = 1;

			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            
			/*start tx/rx transfer*/
			q_md = arg[1];
			loop = arg[2];
			pkt_md = arg[3];
			pkt_sz = arg[4];
			
			ret = f_tx_rx_ep0_perf_lb(loop, 0, pkt_md, q_md, pkt_sz, ATCASE_PERF_TX);

			/*get tx/rx packet count recorded in device*/
			cmd.cmd = ATHIF_CMD_GET_PKT_CNT;
			cmd.len = 0;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            else {
				KAL_DBGPRINT(KAL, DBG_WARN,("UL pkt cnt = %d, DL pkt cnt = %d\n",*(unsigned int*)athif_result_save_t->buf, *(unsigned int*)(athif_result_save_t->buf+4)));
			}		

			break;
		case ATCASE_PERF_RX:
			q_md = arg[1];
			loop = arg[2];
			pkt_md = arg[3];
			pkt_sz = arg[4];
			ret = f_rx_perf_tst(loop, 0, pkt_md, q_md, pkt_sz, ATCASE_PERF_RX);
			break;
		case ATCASE_PERF_TXRX:
			/*issue auto-test command*/
			cmd.cmd = ATHIF_CMD_SET_FWD_MD;
			cmd.buf[0] = ATHIF_FWD_LOOPBACK;
			cmd.len = 1;

			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}	
			
			/*start tx/rx transfer*/
			q_md = arg[1];
			loop = arg[2];
			pkt_md = arg[3];
			pkt_sz = arg[4];
			
			ret = f_tx_rx_ep0_perf_lb(loop, 0, pkt_md, q_md, pkt_sz, ATCASE_PERF_TXRX);

			/*get tx/rx packet count recorded in device*/
			cmd.cmd = ATHIF_CMD_GET_PKT_CNT;
			cmd.len = 0;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            else {
				KAL_DBGPRINT(KAL, DBG_WARN,("UL pkt cnt = %d, DL pkt cnt = %d\n",*(unsigned int*)athif_result_save_t->buf, *(unsigned int*)(athif_result_save_t->buf+4)));
			}		

			break;

		case ATCASE_PERF_TXRX_RAND:
			/*issue auto-test command*/
			cmd.cmd = ATHIF_CMD_SET_FWD_MD;
			cmd.buf[0] = ATHIF_FWD_LOOPBACK;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            
			/*set loopback type*/
			cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
			cmd.buf[0] = ATHIF_LB_TGPD_RANDOM;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

			/*set RGPD reload type*/
			cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
			cmd.buf[0] = ATHIF_RGPD_RANDOM;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

			/*start tx/rx transfer*/
			q_md = arg[1];
			loop = arg[2];
			pkt_md = arg[3];
			pkt_sz = arg[4];
			
			if (ret = f_tx_rx_ep0_perf_lb(loop, 0, pkt_md, q_md, pkt_sz, ATCASE_PERF_TXRX) == RET_FAIL) {
				break;
			}

			/*restore loopback type*/
			cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
			cmd.buf[0] = ATHIF_LB_TGPD_DIRECT;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

			/*restore RGPD reload type*/
			cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
			cmd.buf[0] = ATHIF_RGPD_BASIC;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}


			/*get tx/rx packet count recorded in device*/
			cmd.cmd = ATHIF_CMD_GET_PKT_CNT;
			cmd.len = 0;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            else {
				KAL_DBGPRINT(KAL, DBG_WARN,("UL pkt cnt = %d, DL pkt cnt = %d\n",*(unsigned int*)athif_result_save_t->buf, *(unsigned int*)(athif_result_save_t->buf+4)));
			}		
			break;
            
		case ATCASE_PERF_TXRX_ECM:
			/*issue auto-test command*/
			cmd.cmd = ATHIF_CMD_SET_FWD_MD;
			cmd.buf[0] = ATHIF_FWD_LOOPBACK;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            
			/*set loopback type*/
			cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
			cmd.buf[0] = ATHIF_LB_TGPD_DIRECT;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

			/*pause rx reload*/
			cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
			cmd.buf[0] = 1; // 1 : pause , 0 : resume
			cmd.len =  1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            
			/*set allocate RX GPD type*/
			cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
			cmd.buf[0] = ATHIF_RGPD_ECM;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

            at_mtlte_hif_sdio_clear_tx_count();
            
			/*resume rx reload for new buffer type*/
			cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
			cmd.buf[0] = 0; // 1 : pause , 0 : resume
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}


			/*start tx/rx transfer*/
			q_md = arg[1];
			loop = arg[2];
			pkt_md = arg[3];
			pkt_sz = arg[4];

			/*ECM Type buffer only has 544 x 3 = 1632 bytes*/
			if (pkt_md == 0) { //random pkt size
				pkt_md = 1;
				pkt_sz = 1632;
			} else if (pkt_md == 1) {
				pkt_md = 1;
				if (pkt_sz > 1632) {
					pkt_sz = 1632;
				}
			} else {
				if (pkt_sz > 1632) {
					pkt_md = 2;
					pkt_sz = 1632;
				}
			}
			if (ret = f_tx_rx_ep0_perf_lb(loop, 0, pkt_md, q_md, pkt_sz, ATCASE_PERF_TXRX) == RET_FAIL) {
				break;
			}			

#if 0
			/*pause rx reload*/
			cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
			cmd.buf[0] = 1; // 1 : pause , 0 : resume
			cmd.len = ATHIF_CMD_HD_LEN + 1;
			if (ret = t_dev_set_cmd_wait_done(&cmd, &status) != RET_SUCCESS){
				return ret;
			}
			/*set allocate RX GPD type*/
			cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
			cmd.buf[0] = ATHIF_RGPD_BASIC;
			cmd.len = ATHIF_CMD_HD_LEN + 1;
			if (ret = t_dev_set_cmd_wait_done(&cmd , &status) == RET_FAIL) {
				return ret;
			}		
			/*resume rx reload for new buffer type*/
			cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
			cmd.buf[0] = 0; // 1 : pause , 0 : resume
			cmd.len = ATHIF_CMD_HD_LEN + 1;
			if (ret = t_dev_set_cmd_wait_done(&cmd, &status) != RET_SUCCESS){
				return ret;
			}
#endif
			break;

		case ATCASE_PERF_UL_RAND:
			/*issue auto-test command*/
			cmd.cmd = ATHIF_CMD_SET_FWD_MD;
			cmd.buf[0] = ATHIF_FWD_LOOPBACK;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            
			/*set loopback type*/
			cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
			cmd.buf[0] = ATHIF_LB_TGPD_DIRECT;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

			/*set RGPD reload type*/
			cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
			cmd.buf[0] = ATHIF_RGPD_RANDOM;
			cmd.len =  1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

			/*start tx/rx transfer*/
			q_md = arg[1];
			loop = arg[2];
			pkt_md = arg[3];
			pkt_sz = arg[4];
			
			if (ret = f_tx_rx_ep0_perf_lb(loop, 0, pkt_md, q_md, pkt_sz, ATCASE_PERF_TXRX) == RET_FAIL) {
				break;
			}
#if 0
			/*restore loopback type*/
			cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
			cmd.buf[0] = ATHIF_LB_TGPD_DIRECT;
			cmd.len = ATHIF_CMD_HD_LEN + 1;
			if (ret = t_dev_set_cmd_wait_done(&cmd , &status) == RET_FAIL) {
				break;
			}

			/*restore RGPD reload type*/
			cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
			cmd.buf[0] = ATHIF_RGPD_BASIC;
			cmd.len = ATHIF_CMD_HD_LEN + 1;
			if (ret = t_dev_set_cmd_wait_done(&cmd , &status) == RET_FAIL) {
				break;
			}
#endif			

			/*get tx/rx packet count recorded in device*/
			cmd.cmd = ATHIF_CMD_GET_PKT_CNT;
			cmd.len = 0;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            else {
				KAL_DBGPRINT(KAL, DBG_WARN,("UL pkt cnt = %d, DL pkt cnt = %d\n",*(unsigned int*)athif_result_save_t->buf, *(unsigned int*)(athif_result_save_t->buf+4)));
			}	
			break;
		case ATCASE_PERF_TXRX_ACM:
			/*set ACM RGPD type*/
			cmd.cmd = ATHIF_CMD_CFG_SPECIFIC_RGPD_RL;
			p_rl_rgpd_format = cmd.buf;
			p_rl_rgpd_format->rgpd_allow_len = 512;
			p_rl_rgpd_format->rbd_num = 0;
			cmd.len = sizeof(athif_ul_rgpd_format_t);
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            
			/*issue auto-test command*/
			cmd.cmd = ATHIF_CMD_SET_FWD_MD;
			cmd.buf[0] = ATHIF_FWD_LOOPBACK;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            
			/*set loopback type*/
			cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
			cmd.buf[0] = ATHIF_LB_TGPD_DIRECT;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            
			/*pause rx reload*/
			cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
			cmd.buf[0] = 1; // 1 : pause , 0 : resume
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            
			/*set allocate RX GPD type*/
			cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
			cmd.buf[0] = ATHIF_RGPD_SPECIFIC;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

            at_mtlte_hif_sdio_clear_tx_count();
            
			/*resume rx reload for new buffer type*/
			cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
			cmd.buf[0] = 0; // 1 : pause , 0 : resume
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

			/*change the compare mode to agrregate frag packets*/
			cmp_pattern = ATCASE_LB_DATA_FRAGMENT;

            /*SDIO has no ZLP*/
			//send_no_zlp = 1;

            /*set recv compare fragment check size information*/
			for (ep_idx = 0 ; ep_idx < HIF_MAX_DLQ_NUM ; ep_idx ++) {
				recv_frag_ctrl[ep_idx].max_frag_unit_sz = 512;
			}

			/*start tx/rx transfer*/
			q_md = arg[1];
			loop = arg[2];
			pkt_md = arg[3];
			pkt_sz = arg[4];
			
			if (ret = f_tx_rx_ep0_perf_lb(loop, 0, pkt_md, q_md, pkt_sz, ATCASE_PERF_TXRX) == RET_FAIL) {
				break;
			}

			/*set default compare mode*/
			cmp_pattern = ATCASE_LB_DATA_AUTO;

            /*SDIO has no ZLP*/
			//send_no_zlp = 0;

			break;
            
		case ATCASE_PERF_TXRX_EMPTY_ENQ:
			/*issue auto-test command*/
			cmd.cmd = ATHIF_CMD_SET_FWD_MD;
			cmd.buf[0] = ATHIF_FWD_LOOPBACK;
			cmd.len =  1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            
			/*pause rx reload*/
			cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
			cmd.buf[0] = 1; // 1 : pause , 0 : resume
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            
			/*set loopback type, must stop the queue before use the ATHIF_LB_TGPD_EMPTY_ENQ*/
			cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
			cmd.buf[0] = ATHIF_LB_TGPD_EMPTY_ENQ;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            
            
			/*set allocate RX GPD type*/
			cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
			cmd.buf[0] = ATHIF_RGPD_BASIC;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

            at_mtlte_hif_sdio_clear_tx_count();
            
			/*resume rx reload for new buffer type*/
			cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
			cmd.buf[0] = 0; // 1 : pause , 0 : resume
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

			/*start tx/rx transfer*/
			q_md = arg[1];
			loop = arg[2];
			pkt_md = arg[3];
			pkt_sz = arg[4];
			
			if (ret = f_tx_rx_ep0_perf_lb(loop, 0, pkt_md, q_md, pkt_sz, ATCASE_PERF_TXRX) == RET_FAIL) {
				break;
			}

			/*pause rx reload*/
			cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
			cmd.buf[0] = 1; // 1 : pause , 0 : resume
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            
			/*set loopback type, must stop the queue before use the ATHIF_LB_TGPD_EMPTY_ENQ*/
			cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
			cmd.buf[0] = ATHIF_LB_TGPD_DIRECT;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

            at_mtlte_hif_sdio_clear_tx_count();
            
			/*resume rx reload for new buffer type*/
			cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
			cmd.buf[0] = 0; // 1 : pause , 0 : resume
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
			
			break;

        case ATCASE_PERF_TX_HW_LIMIT :
			/*issue auto-test command*/
			cmd.cmd = ATHIF_CMD_SET_FWD_MD;
			cmd.buf[0] = ATHIF_FWD_FREE_ONLY;
			cmd.len = 1;

			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}	

            /*start tx/rx transfer*/
			q_md = arg[1];
			loop = arg[2];
			pkt_md = arg[3];
			pkt_sz = arg[4];
			
			ret = tx_perf_hw_limit(loop, 0, pkt_md, q_md, pkt_sz, ATCASE_PERF_TX);

			/*get tx/rx packet count recorded in device*/
			cmd.cmd = ATHIF_CMD_GET_PKT_CNT;
			cmd.len = 0;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            else {
				KAL_DBGPRINT(KAL, DBG_WARN,("UL pkt cnt = %d, DL pkt cnt = %d\n",*(unsigned int*)athif_result_save_t->buf, *(unsigned int*)(athif_result_save_t->buf+4)));
            }
			break;

        case ATCASE_PERF_TXRX_UNALIGN:
			/*issue auto-test command*/
			cmd.cmd = ATHIF_CMD_SET_FWD_MD;
			cmd.buf[0] = ATHIF_FWD_LOOPBACK;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            
			/*set loopback type*/
			cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
			cmd.buf[0] = ATHIF_LB_TGPD_DIRECT;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

			/*set RGPD reload type*/
			unalign_offset = arg[5];
    		if(unalign_offset == 0 || unalign_offset > 0xFF){
    		    unalign_offset = 0xFF;
            }
            cmd.cmd = ATHIF_CMD_SET_BUF_OFFSET;
		    p_unalign_offset = (unsigned int *)cmd.buf;
    		*p_unalign_offset = unalign_offset;
	        cmd.len = 4;
		    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    		mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
            cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
		    cmd.buf[0] = ATHIF_RGPD_BUF_MISALIGN;
	        cmd.len = 1;
	        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

			/*start tx/rx transfer*/
			q_md = arg[1];
			loop = arg[2];
			pkt_md = arg[3];
			pkt_sz = arg[4];
			
			if (ret = f_tx_rx_ep0_perf_lb(loop, 0, pkt_md, q_md, pkt_sz, ATCASE_PERF_TXRX) == RET_FAIL) {
				break;
			}

			/*restore loopback type*/
			cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
			cmd.buf[0] = ATHIF_LB_TGPD_DIRECT;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

			/*restore RGPD reload type*/
			cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
			cmd.buf[0] = ATHIF_RGPD_BASIC;
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

            cmd.cmd = ATHIF_CMD_SET_BUF_OFFSET;
		    p_unalign_offset = (unsigned int *)cmd.buf;
    		*p_unalign_offset = 0;
	        cmd.len = 4;
		    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    		mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}


			/*get tx/rx packet count recorded in device*/
			cmd.cmd = ATHIF_CMD_GET_PKT_CNT;
			cmd.len = 0;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            else {
				KAL_DBGPRINT(KAL, DBG_WARN,("UL pkt cnt = %d, DL pkt cnt = %d\n",*(unsigned int*)athif_result_save_t->buf, *(unsigned int*)(athif_result_save_t->buf+4)));
			}		
			break;
            
		default :
			break;
	} 

    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
	return ret;
}


static int t_dev_ulq_random_stop(int argc, char** argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
    athif_fwd_mode_e tx_deq_mode;
    athif_cmd_t cmd;

    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    
	for(test_que_no=0; test_que_no<HIF_MAX_ULQ_NUM; test_que_no++){

        tx_deq_mode = ATHIF_FWD_FREE_ONLY;
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
        
        for(pkt_sz=1520; pkt_sz<=1550; pkt_sz++){ 
	        for(pkt_num=20; pkt_num<=20; pkt_num++){
			    for (idx = 0 ; idx < pkt_num ; idx ++) {
                    KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                        __FUNCTION__, test_que_no, pkt_sz, idx));
				    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
				    if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					    /*send packet fail and retry*/
                        idx --;
                        return ret;
				    } 
			    }
                KAL_SLEEP_MSEC(1) ;
            }
	    }

        // TODO:  if host has reset data flow & hif layer function, set it at here.
        
        cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
		cmd.buf[0] = 1; // 1 : pause , 0 : resume
		cmd.len = 1;
		mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            
        tx_deq_mode = ATHIF_FWD_CMP_DATA;
        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
		cmd.buf[0] = 0; // 1 : pause , 0 : resume
		cmd.len = 1;
		mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
        
    
        send_pattern = ATCASE_LB_DATA_AUTO;

        for(pkt_sz=1550; pkt_sz<=1580; pkt_sz++){ 
	        for(pkt_num=20; pkt_num<=20; pkt_num++){
			    for (idx = 0 ; idx < pkt_num ; idx ++) {
                    KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                        __FUNCTION__, test_que_no, pkt_sz, idx));
				    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
				    if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					    /*send packet fail and retry*/
                        idx --;
                        return ret;
				    } 
			    }
                KAL_SLEEP_MSEC(1) ;
            }
	    }
	}


    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

	return ret;
    
}

static int t_dev_dlq_random_stop(int argc, char** argv)
{
    unsigned int idx = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int  pkt_num = 0, q_num = 0 , pkt_sz = 0;
	//unsigned int rand_num = 0;
	athif_dl_tgpd_cfg_t dl_cfg;
    athif_cmd_t cmd;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    sdio_test_option.exam_dl_content = false;
    //sdio_test_option.show_dl_content= true;
    cmp_pattern = ATCASE_LB_DATA_AUTO;

	memset(&dl_cfg , 0 ,sizeof(athif_dl_tgpd_cfg_t));
    
	for (q_num = 0 ; q_num < HIF_MAX_DLQ_NUM ; q_num ++) {
        
		for (pkt_num = 10 ; pkt_num <= 10 ; pkt_num ++) {
			for (pkt_sz = 1520 ; pkt_sz <= 1530 ; pkt_sz ++) {
				dl_cfg.q_num = q_num;
	 			dl_cfg.gpd_num = pkt_num;
				dl_cfg.tgpd_format.tgpd_ext_len = 0;
				dl_cfg.tgpd_format.tgpd_buf_len = pkt_sz;
				dl_cfg.tgpd_format.tbd_num = 0;
				ret = sdio_dl_npkt(&dl_cfg);
				if (ret != RET_SUCCESS) {
                    KAL_DBGPRINT(KAL, DBG_WARN,("[%s] receiving DL packet fail, the DLQ random stop test will not start !\n",__FUNCTION__));
                    return RET_FAIL;
				}
                
			}
		}

        cmd.cmd = SDIO_AT_RESET_DL_QUEUE;
	    cmd.buf[0] = q_num; 
	    cmd.len = 1;

	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        // TODO: if there is reset data flow & hif layer function, use it at here.
        sdio_test_option.exam_dl_content = true;

        for (pkt_num = 10 ; pkt_num <= 10 ; pkt_num ++) {
			for (pkt_sz = 1520 ; pkt_sz <= 1530 ; pkt_sz ++) {
				dl_cfg.q_num = q_num;
	 			dl_cfg.gpd_num = pkt_num;
				dl_cfg.tgpd_format.tgpd_ext_len = 0;
				dl_cfg.tgpd_format.tgpd_buf_len = pkt_sz;
				dl_cfg.tgpd_format.tbd_num = 0;
				ret = sdio_dl_npkt(&dl_cfg);
				if (ret != RET_SUCCESS) {
                    KAL_DBGPRINT(KAL, DBG_WARN,("[%s] receiving DL packet fail during DLQ random stop test !!\n",__FUNCTION__));
                    return RET_FAIL;
				}
                
			}
		}

        
	}
            
    sdio_test_option.show_dl_content= false;
    sdio_test_option.exam_dl_content = false;
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
	return ret;

            
}


static int t_dev_ul_allowlen_error(int argc, char** argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
    athif_fwd_mode_e tx_deq_mode;
    athif_cmd_t cmd;

    athif_ul_rgpd_tst_cfg_t *p_rgpd_cfg;
	unsigned int pktSize = 0;
	unsigned int q_num = 0 , pkt_cnt = 0;
	//int send_err_timeout = SEND_ERR_TIMEOUT, send_err_retry = SEND_ERR_RETRY;
	unsigned int total_allow_len = 0;
    unsigned int tst_idx = 0;
    unsigned int normal_pkt_cnt = 0, err_pkt=0;
    unsigned int normal_sz = 0, err_sz = 0;
    hifsdio_isr_status_t *device_int_st;

    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}	

    tst_idx = arg[0];
    //for (tst_idx = 0 ; tst_idx < 5 ; tst_idx ++) { 
        for (q_num = 0 ; q_num < HIF_MAX_ULQ_NUM ; q_num ++) {
   
            cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
			cmd.buf[0] = 1; // 1 : pause , 0 : resume
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
               
            // prepare ul gpd to test
            pkt_cnt = 60;
 	 	 	cmd.cmd = ATHIF_CMD_PREPARE_RGPD;
 	 	 	p_rgpd_cfg = (athif_ul_rgpd_tst_cfg_t *)cmd.buf;
 	 	 	p_rgpd_cfg->q_num = q_num;
 	 	 	/*must add one more gpd for queue initial tail*/
 	 	 	p_rgpd_cfg->gpd_num = pkt_cnt + 1;

            switch (tst_idx) {
				case 0:	// RGPD allow_len = 0 , send 1 pkt 60 bytes and err
					p_rgpd_cfg->rgpd_format.rgpd_allow_len = 0;
					p_rgpd_cfg->rgpd_format.rbd_num = 0;
					normal_sz = 60;
					err_sz = 60;	
					normal_pkt_cnt = 0;
					err_pkt = 1;
					break;
				case 1:	// RGPD allow_len= 2000, send 10 pkt 1000 bytes, 11th pkt 2040 bytes and err
					p_rgpd_cfg->rgpd_format.rgpd_allow_len = 2000;
					p_rgpd_cfg->rgpd_format.rbd_num = 0;					
					normal_sz = 1000;
					err_sz = 2040;	
					normal_pkt_cnt = 10;
					err_pkt = 1;
					break;
				case 2:	// 2RBD, RBD1=0 RBD2=1000, send 10pkt 600bytes , no err
					p_rgpd_cfg->rgpd_format.rgpd_allow_len = 0;
					p_rgpd_cfg->rgpd_format.rbd_num = 2;
					p_rgpd_cfg->rgpd_format.rbd_allow_len[0] = 0;
					p_rgpd_cfg->rgpd_format.rbd_allow_len[1] = 1000;
					normal_sz = 600;
					err_sz = 60;	
					normal_pkt_cnt = 10;
					err_pkt = 0;
					break;
				case 3:	// 3RBD, RBD1=1000, RBD2=0, RBD3=1000, transfer 10pkt 1500bytes, no err
					p_rgpd_cfg->rgpd_format.rgpd_allow_len = 0;
					p_rgpd_cfg->rgpd_format.rbd_num = 3;
					p_rgpd_cfg->rgpd_format.rbd_allow_len[0] = 1000;
					p_rgpd_cfg->rgpd_format.rbd_allow_len[1] = 0;
					p_rgpd_cfg->rgpd_format.rbd_allow_len[2] = 1000;
					normal_sz = 1500;
					err_sz = 60;	
					normal_pkt_cnt = 10;
					err_pkt = 0;
					break;
				case 4:	// 2RBD, RBD1=1000, RBD2=1000, send 10 pkt 1500 bytes, 11th pkt 2040 byte and err
					p_rgpd_cfg->rgpd_format.rgpd_allow_len = 0;
					p_rgpd_cfg->rgpd_format.rbd_num = 2;
					p_rgpd_cfg->rgpd_format.rbd_allow_len[0] = 1000;
					p_rgpd_cfg->rgpd_format.rbd_allow_len[1] = 1000;
					normal_sz = 1500;
					err_sz = 2040;	
					normal_pkt_cnt = 10;
					err_pkt = 1;
					break;
				default :
					break;
			}

            /*clear interrupt info*/
			cmd.len = sizeof(athif_ul_rgpd_tst_cfg_t);
 	 	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

            //Set interrupt test mode flag
            cmd.cmd = SDIO_AT_DL_INT_TEST_SWITCH;
	        cmd.buf[0] = 1; // 1 : test mode , 0 : normal mode
	        cmd.len = 1;
	        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

            recv_th_rslt = RET_SUCCESS;
	        recv_total_pkt_cnt = 0;
	        recv_total_pkt_cnt_agg = 0;
            sdio_test_option.auto_receive_pkt = true;
            

			if (normal_pkt_cnt) {
				recv_th_rslt = RET_SUCCESS;
				cmp_pattern = ATCASE_LB_DATA_AUTO;

				for (idx = 0 ; idx < normal_pkt_cnt ; idx ++) {
                    
					ret = sdio_send_pkt(q_num, normal_sz , q_num, 0);
					if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Test fail at sent pkt !\n",__FUNCTION__));
					    /*send packet fail and retry*/
                        idx --;
                        return ret;
					}

					if (recv_th_rslt != RET_SUCCESS) {
						KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] recv thread report fail\n", __FUNCTION__));
						ret = RET_FAIL;
						break;
					}			
				}

				if (ret == RET_SUCCESS) {
					/*wait loopback data*/
					ret = f_wait_recv_pkt_cnt(normal_pkt_cnt , 10000);
					if (ret != RET_SUCCESS) {
						KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] f_wait_recv_pkt_cnt timeout\n", __FUNCTION__));
					}
					if (recv_th_rslt != RET_SUCCESS) {
						KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] recv thread report fail\n", __FUNCTION__));
						ret = RET_FAIL;					
					}
					recv_th_rslt = RET_SUCCESS;
					recv_total_pkt_cnt = 0;
				}

				cmp_pattern = ATCASE_LB_DATA_AUTO;
                
			}

            cmd.cmd = SDIO_AT_READ_INT_STATUS;
	        cmd.buf[0] = 0; 
	        cmd.len = 1;
	        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;} 

            device_int_st = (hifsdio_isr_status_t *)athif_result_save_t->buf;
            if( (device_int_st->UL0_INTR_Status & 0xFFFFFF00) !=0){
                KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d][ERR] UL INT has some error before test !!\n", __FUNCTION__,__LINE__));
                ret = RET_FAIL;
                return RET_FAIL;
            }

			if (err_pkt) {
				ret = sdio_send_pkt(q_num, err_sz, q_num, 0);
				if (ret != RET_SUCCESS) {
					return RET_FAIL;
				}
				KAL_SLEEP_MSEC(10);
                
				//get qmu interrupt info and expect there is error interrupt
				cmd.cmd = SDIO_AT_READ_INT_STATUS;
	            cmd.buf[0] = 0; 
	            cmd.len = 1;
	            mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
                mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
                if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;} 

                device_int_st = (hifsdio_isr_status_t *)athif_result_save_t->buf;
                if( (device_int_st->UL0_INTR_Status & ORG_SDIO_TXQ_LEN_ERR(q_num) ) ==0){
                    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d][ERR] UL LEN ERR INT has not occur at que%d test_idx=%d!!\n", __FUNCTION__,__LINE__, q_num, tst_idx));
				    ret = RET_FAIL;
                    return RET_FAIL;
                }
			}

            cmd.cmd = SDIO_AT_DL_INT_TEST_SWITCH;
	        cmd.buf[0] = 0; // 1 : test mode , 0 : normal mode
	        cmd.len = 1;
	        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

            cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
			cmd.buf[0] = 0; // 1 : pause , 0 : resume
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        }
    //}

    //Set interrupt back to normal mode 
    cmd.cmd = SDIO_AT_DL_INT_TEST_SWITCH;
	cmd.buf[0] = 0; // 1 : test mode , 0 : normal mode
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_FREE_ONLY;
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    recv_th_rslt = RET_SUCCESS;
	recv_total_pkt_cnt = 0;
	recv_total_pkt_cnt_agg = 0;
    sdio_test_option.auto_receive_pkt = false;

    mtlte_hif_sdio_enable_fw_own_back(1);
    return ret;

}


static int t_dev_dl_len_error(int argc, char** argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
    athif_fwd_mode_e tx_deq_mode;
    athif_cmd_t cmd;

    athif_dl_tgpd_cfg_t *p_tgpd_cfg;
	unsigned int pktSize = 0;
	unsigned int q_num = 0 , pkt_cnt = 0;
	//int send_err_timeout = SEND_ERR_TIMEOUT, send_err_retry = SEND_ERR_RETRY;
	unsigned int total_allow_len = 0;
    unsigned int tst_idx = 0;
    unsigned int normal_pkt_cnt = 0, err_pkt=0;
    unsigned int normal_sz = 0, err_sz = 0;
    hifsdio_isr_status_t *device_int_st;

    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}
    
    //for (tst_idx = 0 ; tst_idx < 8 ; tst_idx ++) { 
    tst_idx = arg[0];
    
        for (q_num = 0 ; q_num < HIF_MAX_DLQ_NUM ; q_num ++) {
   
            cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
			cmd.buf[0] = 1; // 1 : pause , 0 : resume
			cmd.len = 1;
			mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

            cmd.cmd = SDIO_AT_RESET_DL_QUEUE;
	        cmd.buf[0] = q_num; 
	        cmd.len = 1;
	        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

            //Set interrupt test mode flag
            cmd.cmd = SDIO_AT_DL_INT_TEST_SWITCH;
	        cmd.buf[0] = 1; // 1 : test mode , 0 : normal mode
	        cmd.len = 1;
	        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

            cmd.cmd = SDIO_AT_READ_INT_STATUS;
	        cmd.buf[0] = 0; 
	        cmd.len = 1;
	        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;} 

            device_int_st = (hifsdio_isr_status_t *)athif_result_save_t->buf;
            if( (device_int_st->DL1_INTR_Status & 0x0000FF00) !=0){
                KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d][ERR] DL INT has some error before test !!\n", __FUNCTION__,__LINE__));
				ret = RET_FAIL;
            }
               
            /*prepare TGPD*/
            pkt_cnt = 20;
			cmd.cmd = ATHIF_CMD_DL_SEND_N;
			p_tgpd_cfg = cmd.buf;
			p_tgpd_cfg->q_num = q_num;
			p_tgpd_cfg->gpd_num = pkt_cnt;

            switch (tst_idx) {
				case 0:	  // tgpd ext=0 , tgpd len =0 =>error
					p_tgpd_cfg->tgpd_format.tgpd_ext_len = 0;
					p_tgpd_cfg->tgpd_format.tgpd_buf_len = 0; //should fill whole BD total length
					p_tgpd_cfg->tgpd_format.tbd_num = 0;
					normal_pkt_cnt = 0;
					err_pkt = 1;
					break; 
				case 1:	//tgpd ext=10, tgpd len = 0 =>no error
					p_tgpd_cfg->tgpd_format.tgpd_ext_len = 10;
					p_tgpd_cfg->tgpd_format.tgpd_buf_len = 0; //should fill whole BD total length
					p_tgpd_cfg->tgpd_format.tbd_num = 0;
					normal_pkt_cnt = 20;
					err_pkt = 0;
					break;
				case 2:	 //tgpd ext=0, tbd ext=0 , tbd len=0 => error
					p_tgpd_cfg->tgpd_format.tgpd_ext_len = 0;
					p_tgpd_cfg->tgpd_format.tgpd_buf_len = 0; //should fill whole BD total length
					p_tgpd_cfg->tgpd_format.tbd_num = 1;
					p_tgpd_cfg->tgpd_format.tbd_format[0].tbd_ext_len = 0;
					p_tgpd_cfg->tgpd_format.tbd_format[0].tbd_buf_len = 0;
					normal_pkt_cnt = 0;
					err_pkt = 1;
					break; 
				case 3:	//tgpd ext=10, tbd ext=10 , tbd len=0 ==> no error
					p_tgpd_cfg->tgpd_format.tgpd_ext_len = 10;
					p_tgpd_cfg->tgpd_format.tgpd_buf_len = 10; //should fill whole BD total length
					p_tgpd_cfg->tgpd_format.tbd_num = 1;
					p_tgpd_cfg->tgpd_format.tbd_format[0].tbd_ext_len = 10;
					p_tgpd_cfg->tgpd_format.tbd_format[0].tbd_buf_len = 0;
					normal_pkt_cnt = 20;
					err_pkt = 0;
					break;
				case 4:	//tgpd ext=0 , tbd ext=10, tbd len=0 ==> no error
					p_tgpd_cfg->tgpd_format.tgpd_ext_len = 0;
					p_tgpd_cfg->tgpd_format.tgpd_buf_len = 10; //should fill whole BD total length
					p_tgpd_cfg->tgpd_format.tbd_num = 1;
					p_tgpd_cfg->tgpd_format.tbd_format[0].tbd_ext_len = 10;
					p_tgpd_cfg->tgpd_format.tbd_format[0].tbd_buf_len = 0;
					normal_pkt_cnt = 20;
					err_pkt = 0;
					break;
				case 5:	//tgpd ext=10 , tbd ext=0, tbd len=0 ==> error
					p_tgpd_cfg->tgpd_format.tgpd_ext_len = 10;
					p_tgpd_cfg->tgpd_format.tgpd_buf_len = 0; //should fill whole BD total length
					p_tgpd_cfg->tgpd_format.tbd_num = 1;
					p_tgpd_cfg->tgpd_format.tbd_format[0].tbd_ext_len = 0;
					p_tgpd_cfg->tgpd_format.tbd_format[0].tbd_buf_len = 0;
					normal_pkt_cnt = 0;
					err_pkt = 1;
					break;
				case 6:	//tgpd ext=0 , tbd1 ext=10, tbd1 len=0, tbd2 ext=0, tbd2 len=0, tbd3 ext=10 , tbd3 len = 100 ==>error
					p_tgpd_cfg->tgpd_format.tgpd_ext_len = 0;
					p_tgpd_cfg->tgpd_format.tgpd_buf_len = 120;
					p_tgpd_cfg->tgpd_format.tbd_num = 3;
					p_tgpd_cfg->tgpd_format.tbd_format[0].tbd_ext_len = 10;
					p_tgpd_cfg->tgpd_format.tbd_format[0].tbd_buf_len = 0;
					p_tgpd_cfg->tgpd_format.tbd_format[1].tbd_ext_len = 0;
					p_tgpd_cfg->tgpd_format.tbd_format[1].tbd_buf_len = 0;
					p_tgpd_cfg->tgpd_format.tbd_format[2].tbd_ext_len = 10;
					p_tgpd_cfg->tgpd_format.tbd_format[2].tbd_buf_len = 100;
					normal_pkt_cnt = 0;
					err_pkt = 1;
					break;
				case 7:	//tgpd ext=0 , tbd1 ext=0, tbd1 len=100 ==> no error
					p_tgpd_cfg->tgpd_format.tgpd_ext_len = 0;
					p_tgpd_cfg->tgpd_format.tgpd_buf_len = 100;
					p_tgpd_cfg->tgpd_format.tbd_num = 1;
					p_tgpd_cfg->tgpd_format.tbd_format[0].tbd_ext_len = 0;
					p_tgpd_cfg->tgpd_format.tbd_format[0].tbd_buf_len = 100;
					normal_pkt_cnt = 20;
					err_pkt = 0;
					break;
				default :
					break;
			}


            // start receive the DL packet
            recv_th_rslt = RET_SUCCESS;
            cmp_pattern = ATCASE_LB_DATA_AUTO;
            recv_total_pkt_cnt = 0;
            sdio_test_option.auto_receive_pkt = true;
            
            cmd.len = sizeof(athif_dl_tgpd_cfg_t);
 	 	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
            

			if (normal_pkt_cnt) {
				if (ret == RET_SUCCESS) {
					/*wait loopback data*/
					ret = f_wait_recv_pkt_cnt(normal_pkt_cnt , 10000);
					if (ret != RET_SUCCESS) {
						KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] f_wait_recv_pkt_cnt timeout\n", __FUNCTION__));
                        ret = RET_FAIL;
                        return ret;
					}
					if (recv_th_rslt != RET_SUCCESS) {
						KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] recv thread report fail\n", __FUNCTION__));
						ret = RET_FAIL;	
                        return ret;
					}
					recv_th_rslt = RET_SUCCESS;
					recv_total_pkt_cnt = 0;
				}

				cmp_pattern = ATCASE_LB_DATA_AUTO;
                
			}

            KAL_SLEEP_MSEC(10);

            /*get qmu interrupt info and check whether it meet expect*/
            cmd.cmd = SDIO_AT_READ_INT_STATUS;
	        cmd.buf[0] = 0; 
	        cmd.len = 1;
	        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;} 

            device_int_st = (hifsdio_isr_status_t *)athif_result_save_t->buf;

            if (err_pkt) {
                if ( (device_int_st->DL1_INTR_Status & 0x1<<(8+q_num)) ==0 ) {
                    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] DL LEN ERR interrupt check fail, q_num=%d, tst_idx=%d !\n"
                                                        ,__FUNCTION__,__LINE__, q_num, tst_idx));
                    ret = RET_FAIL;
                    return ret;
                } else {
                    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] DL LEN ERR interrupt check success, q_num=%d, tst_idx=%d !\n"
                                                        ,__FUNCTION__,__LINE__, q_num, tst_idx));
                }      
            }else {         
                if ( (device_int_st->DL1_INTR_Status & 0x0000FF00) !=0) {
                    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] DL LEN ERR interrupt check fail, q_num=%d, tst_idx=%d !\n"
                                                            ,__FUNCTION__,__LINE__, q_num, tst_idx));
                    ret = RET_FAIL;
                    return ret;
                } else {
                    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] DL LEN ERR interrupt check success, q_num=%d, tst_idx=%d !\n"
                                                            ,__FUNCTION__,__LINE__, q_num, tst_idx));
                }      
            }

            cmd.cmd = SDIO_AT_RESET_DL_QUEUE;
	        cmd.buf[0] = q_num; 
	        cmd.len = 1;

	        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

            cmd.cmd = SDIO_AT_DL_INT_TEST_SWITCH;
	        cmd.buf[0] = 0; // 1 : test mode , 0 : normal mode
	        cmd.len = 1;
	        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

            cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	        cmd.buf[0] = 0; // 1 : pause , 0 : resume
	        cmd.len = 1;
	        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
            mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
            if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

            KAL_SLEEP_MSEC(10);

        }
    //}

    //Set interrupt back to normal mode 
    cmd.cmd = SDIO_AT_DL_INT_TEST_SWITCH;
	cmd.buf[0] = 0; // 1 : test mode , 0 : normal mode
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    mtlte_hif_sdio_enable_fw_own_back(1);
    return ret;
}



static int t_dev_bypass_lb(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
	unsigned int expect_num = 0, timeout=0;
    unsigned int old_pkt_cnt = 0;
    athif_fwd_mode_e tx_deq_mode;
    athif_cmd_t cmd;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    	/*set RGPD reload type*/
	cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
	cmd.buf[0] = ATHIF_RGPD_BYPASS;
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    //cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
    //cmd.buf[0] = ATHIF_LB_TGPD_EMPTY_ENQ;
	//cmd.len = 1;

	//mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    //mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    //if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;


	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

	for(test_que_no=0; test_que_no<HIF_MAX_ULQ_NUM; test_que_no++){
        for(pkt_sz=arg[0]; pkt_sz<=arg[1]; pkt_sz++){ 
	        for(pkt_num=arg[2]; pkt_num<=arg[3]; pkt_num++){
			    for (idx = 0 ; idx < pkt_num ; idx ++) {
                    KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                        __FUNCTION__, test_que_no, pkt_sz, idx));
				    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
                    expect_num++;
                    
				    if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					    /*send packet fail and retry*/
                        idx --;
                        return ret;
				    } 
			    }
                KAL_SLEEP_MSEC(1) ;
            }
            // test debug
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Passe of que=%d, pkt_size=%d\n", \
                        __FUNCTION__, test_que_no, pkt_sz));
	    }
	}

    while(expect_num != recv_total_pkt_cnt){
        
        KAL_SLEEP_MSEC(1) ;
        if(recv_total_pkt_cnt != old_pkt_cnt){ timeout ==0; }
        else{ timeout++; }
        
        if(timeout > 1000){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;
            return RET_FAIL;
        }
        old_pkt_cnt = recv_total_pkt_cnt;
    }

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_FREE_ONLY;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    	/*set RGPD reload type*/
	cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
	cmd.buf[0] = ATHIF_RGPD_BASIC;
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();
    

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    sdio_test_option.exam_dl_content = false;
    sdio_test_option.auto_receive_pkt = false;
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    if(recv_th_rslt == RET_SUCCESS){
	    return ret;
    }else{
        return RET_FAIL;
    }
}


static int t_dev_txrx_bypass(int argc, char **argv)
{
	unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int testmode = 0, LoopbackMode = 0 , loop = 0 , pkt_num = 0 , q_num = 0, pkt_sz = 0,q_md = 0 , pkt_md = 0;
	athif_cmd_t cmd;
	athif_status_t status;

	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    
	/*issue auto-test command*/
	cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

	/*pause reload rgpd flow*/
	cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

	/*set RGPD reload type*/
	cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
	cmd.buf[0] = ATHIF_RGPD_BYPASS;
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();
    
	/*resume reload rgpd flow to start new reload type*/
	cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
			
	/*start tx/rx transfer*/
	q_md = arg[0];
	loop = arg[1];
	pkt_md = arg[2];
	pkt_sz = arg[3];
			
	ret = f_tx_rx_ep0_perf_lb(loop, 0, pkt_md, q_md, pkt_sz, ATCASE_PERF_TXRX);

	/*get tx/rx packet count recorded in device*/
	cmd.cmd = ATHIF_CMD_GET_PKT_CNT;
	cmd.len = 0;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    else {
		KAL_DBGPRINT(KAL, DBG_WARN, ("UL pkt cnt = %d, DL pkt cnt = %d\n",*(unsigned int*)athif_result_save_t->buf, *(unsigned int*)(athif_result_save_t->buf+4)));
	}

	/*pause reload rgpd flow*/
	cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
	/*set RGPD reload type*/
	cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
	cmd.buf[0] = ATHIF_RGPD_BASIC;
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
	/*resume reload rgpd flow to start new reload type*/
	cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    mtlte_hif_sdio_enable_fw_own_back(1);
	return ret;
}

static int t_dev_txrx_cs_err(int argc , char **argv)
{
	unsigned int idx = 0 , param_num = 0, ep_num = 0, q_num = 0, mask = 0, test_mode = 0, loop = 0;
	unsigned int q_md = 0 , pkt_sz = 0, pkt_md = 0, org_cs_len = 0;;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	athif_cmd_t cmd;
	athif_status_t status;
	athif_gpd_ioc_cfg_t *p_ioc_cfg;
	athif_mem_tst_cfg_t *p_mem_rw_cfg;


	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    	/*RGPD/RBD CS ERR*/

	/*cs_12 , gpd cs err*/
	if (ret = f_ul_cs_err_tst(12, 0) == RET_FAIL) {
		return ret;
	} 
    
    t_dev_reset_ulq_dlq(argc, argv);
    KAL_SLEEP_MSEC(100);
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    
	/*cs_16 , gpd cs err*/
	if (ret = f_ul_cs_err_tst(16, 0) == RET_FAIL) {
		return ret;
	}
    t_dev_reset_ulq_dlq(argc, argv);
    KAL_SLEEP_MSEC(100);
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    
	/*cs_12 , bd cs err*/
	if (ret = f_ul_cs_err_tst(12, 1) == RET_FAIL) {
		return ret;
	}
    t_dev_reset_ulq_dlq(argc, argv);
    KAL_SLEEP_MSEC(100);
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    
	/*cs_16 , bd cs err*/
	if (ret = f_ul_cs_err_tst(16, 1) == RET_FAIL) {
		return ret;
	}
    t_dev_reset_ulq_dlq(argc, argv);
    KAL_SLEEP_MSEC(100);
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

	/*TGPD/BD CS ERR*/

    /*cs_12 , bd cs err*/
	if (ret = f_dl_cs_err_tst(12, 1) == RET_FAIL) {
		return ret;
	}
    t_dev_reset_ulq_dlq(argc, argv);
    KAL_SLEEP_MSEC(100);
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    
	/*cs_16 , bd cs err*/
	if (ret = f_dl_cs_err_tst(16, 1) == RET_FAIL) {
		return ret;
	}
    t_dev_reset_ulq_dlq(argc, argv);
    KAL_SLEEP_MSEC(100);
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    

	/*cs_12 , gpd cs err*/
	if (ret = f_dl_cs_err_tst(12, 0) == RET_FAIL) {
		return ret;
	}
    t_dev_reset_ulq_dlq(argc, argv);
    KAL_SLEEP_MSEC(100);
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    
	/*cs_16 , gpd cs err*/
	if (ret = f_dl_cs_err_tst(16, 0) == RET_FAIL) {
		return ret;
	}
    t_dev_reset_ulq_dlq(argc, argv);
    KAL_SLEEP_MSEC(100);
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    

    mtlte_hif_sdio_enable_fw_own_back(1);
	return ret;
}


static int t_dev_tcm_lb(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
	unsigned int expect_num = 0, timeout=0;
    unsigned int old_pkt_cnt = 0;
    athif_fwd_mode_e tx_deq_mode;
    athif_cmd_t cmd;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
    cmd.buf[0] = ATHIF_RGPD_TCM;
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;


	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

	for(test_que_no=0; test_que_no<HIF_MAX_ULQ_NUM; test_que_no++){
        for(pkt_sz=arg[0]; pkt_sz<=arg[1]; pkt_sz++){ 
	        for(pkt_num=arg[2]; pkt_num<=arg[3]; pkt_num++){
			    for (idx = 0 ; idx < pkt_num ; idx ++) {
                    KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                        __FUNCTION__, test_que_no, pkt_sz, idx));
				    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
                    expect_num++;
                    
				    if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					    /*send packet fail and retry*/
                        idx --;
                        return ret;
				    } 
			    }
                KAL_SLEEP_MSEC(1) ;
            }
            // test debug
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Passe of que=%d, pkt_size=%d\n", \
                        __FUNCTION__, test_que_no, pkt_sz));
	    }
	}

    while(expect_num != recv_total_pkt_cnt){
        
        KAL_SLEEP_MSEC(1) ;
        if(recv_total_pkt_cnt != old_pkt_cnt){ timeout ==0; }
        else{ timeout++; }
        
        if(timeout > 1000){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;
            return RET_FAIL;
        }
        old_pkt_cnt = recv_total_pkt_cnt;
    }

    tx_deq_mode = ATHIF_FWD_FREE_ONLY;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
    cmd.buf[0] = ATHIF_RGPD_BASIC;
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    sdio_test_option.exam_dl_content = false;
    sdio_test_option.auto_receive_pkt = false;
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    if(recv_th_rslt == RET_SUCCESS){
	    return ret;
    }else{
        return RET_FAIL;
    }
}


static int t_dev_tcm_misalign_lb(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
	unsigned int expect_num = 0, timeout=0;
    unsigned int old_pkt_cnt = 0;
    athif_fwd_mode_e tx_deq_mode;
    athif_cmd_t cmd;
    unsigned int unalign_offset;
    unsigned int *p_unalign_offset;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    unalign_offset = 0xFF;
    cmd.cmd = ATHIF_CMD_SET_BUF_OFFSET;
	p_unalign_offset = (unsigned int *)cmd.buf;
    *p_unalign_offset = unalign_offset;
	cmd.len = 4;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
    cmd.buf[0] = ATHIF_RGPD_TCM;
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;


	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

	for(test_que_no=0; test_que_no<HIF_MAX_ULQ_NUM; test_que_no++){
        for(pkt_sz=arg[0]; pkt_sz<=arg[1]; pkt_sz++){ 
	        for(pkt_num=arg[2]; pkt_num<=arg[3]; pkt_num++){
			    for (idx = 0 ; idx < pkt_num ; idx ++) {
                    KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                        __FUNCTION__, test_que_no, pkt_sz, idx));
				    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
                    expect_num++;
                    
				    if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					    /*send packet fail and retry*/
                        idx --;
                        return ret;
				    } 
			    }
                KAL_SLEEP_MSEC(1) ;
            }
            // test debug
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Passe of que=%d, pkt_size=%d\n", \
                        __FUNCTION__, test_que_no, pkt_sz));
	    }
	}

    while(expect_num != recv_total_pkt_cnt){
        
        KAL_SLEEP_MSEC(1) ;
        if(recv_total_pkt_cnt != old_pkt_cnt){ timeout ==0; }
        else{ timeout++; }
        
        if(timeout > 1000){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;
            return RET_FAIL;
        }
        old_pkt_cnt = recv_total_pkt_cnt;
    }

    tx_deq_mode = ATHIF_FWD_FREE_ONLY;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
    cmd.buf[0] = ATHIF_RGPD_BASIC;
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    sdio_test_option.exam_dl_content = false;
    sdio_test_option.auto_receive_pkt = false;
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    if(recv_th_rslt == RET_SUCCESS){
	    return ret;
    }else{
        return RET_FAIL;
    }
}


static int t_dev_atcmd_data_interleave_lb(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
	unsigned int expect_num = 0, timeout=0;
    unsigned int old_pkt_cnt = 0;
    athif_fwd_mode_e tx_deq_mode;
    athif_cmd_t cmd;
    unsigned int    *p_unalign_offset;  
    unsigned int    rand_num, rand_len, atcmd_threhold, atcmd_len;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    param_num = argc - 1;
	//first argument is the command string
	for (idx = 1 ; idx < argc ; idx ++) {
		//translate number string to value
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}
    

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    cmd.cmd = ATHIF_CMD_SET_BUF_OFFSET;
	p_unalign_offset = (unsigned int *)cmd.buf;
    *p_unalign_offset = 0xFF;
	cmd.len = 4;
    
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
	cmd.buf[0] = ATHIF_RGPD_BUF_MISALIGN;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;

    //3 //for atcmd loopback test
    cmd.cmd = SDIO_AT_CMD_LOOPBACK;
	atcmd_threhold = 0x8;

	for(test_que_no=0; test_que_no<HIF_MAX_ULQ_NUM; test_que_no++){
        for(pkt_sz=arg[0]; pkt_sz<=arg[1]; pkt_sz++){ 
	        for(pkt_num=arg[2]; pkt_num<=arg[3]; pkt_num++){
			    for (idx = 0 ; idx < pkt_num ; idx ++) {
                    KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                        __FUNCTION__, test_que_no, pkt_sz, idx));
				    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
                    expect_num++;
                    
				    if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					    //send packet fail and retry
                        idx --;
                        return ret;
				    } 

                    //3 //for atcmd loopback test
                    get_random_bytes(&rand_num, sizeof(unsigned int));
                    rand_num = rand_num & 0xFF;
                    if (rand_num <= atcmd_threhold){
                        get_random_bytes(&rand_len, sizeof(unsigned int));
                        rand_len = rand_len & 0x3F;
                        get_random_bytes(cmd.buf, rand_len);
                        cmd.len = rand_len;
                        
                        mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
                        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
                        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

                        if (0 != memcmp(cmd.buf, athif_result_save_t->buf, rand_len) ){
                            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR]  data of atcmd compare fail !!! \n",KAL_FUNC_NAME)) ;
                            return RET_FAIL;
                        }
                        else{
                            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[log]  Send a atcmd with len %d and compare success. \n",KAL_FUNC_NAME, rand_len)) ;
                        }
                    }
			    }
                KAL_SLEEP_MSEC(1) ;
            }
            // test debug
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Passe of que=%d, pkt_size=%d\n", \
                        __FUNCTION__, test_que_no, pkt_sz));
	    }
	}

    while(expect_num != recv_total_pkt_cnt){
        
        KAL_SLEEP_MSEC(1) ;
        if(recv_total_pkt_cnt != old_pkt_cnt){ timeout ==0; }
        else{ timeout++; }
        
        if(timeout > 1000){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;
            return RET_FAIL;
        }
        old_pkt_cnt = recv_total_pkt_cnt;
    }

    tx_deq_mode = ATHIF_FWD_FREE_ONLY;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
    cmd.buf[0] = ATHIF_RGPD_BASIC;
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    sdio_test_option.exam_dl_content = false;
    sdio_test_option.auto_receive_pkt = false;
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    if(recv_th_rslt == RET_SUCCESS){
	    return ret;
    }else{
        return RET_FAIL;
    }
}


static int t_dev_stress_random_lb(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
	unsigned int expect_num = 0, timeout=0;
    unsigned int old_pkt_cnt = 0;
    athif_fwd_mode_e tx_deq_mode;
    athif_cmd_t cmd;
    unsigned int    *p_unalign_offset;  
    unsigned int    rand_num, rand_len, atcmd_threhold, atcmd_len;
    unsigned int    target_big_loop, now_big_loop;
    unsigned int    target_small_loop, now_small_loop;
    unsigned int    target_que, now_que;
    unsigned int    target_pktnum, now_pktnum;
    unsigned int    target_size_max, target_size_min, now_size;

    struct timespec start_t , end_t, diff_t;
    unsigned long long transferdata=0,performance = 0;
    unsigned long long diff_ms = 0 ;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    param_num = argc - 1;
	//first argument is the command string
	for (idx = 1 ; idx < argc ; idx ++) {
		//translate number string to value
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}
    

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    cmd.cmd = ATHIF_CMD_SET_BUF_OFFSET;
	p_unalign_offset = (unsigned int *)cmd.buf;
    *p_unalign_offset = 0xFF;
	cmd.len = 4;
    
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
	cmd.buf[0] = ATHIF_RGPD_BUF_MISALIGN;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;

    //2 set target parameter
    target_big_loop = arg[0];
    if (target_big_loop == 0){
        target_big_loop = 0xFFFFFF;
    }
    target_small_loop = 10000;
    target_que = HIF_MAX_ULQ_NUM;
    target_pktnum = 40;
    target_size_max = 2048;
    target_size_min = 1;

    //3 //for atcmd loopback test
    //cmd.cmd = SDIO_AT_CMD_LOOPBACK;
	//atcmd_threhold = 0x8;

	for(now_big_loop=0; now_big_loop<target_big_loop; now_big_loop++){
        recv_total_pkt_cnt = 0;
        expect_num = 0;
        jiffies_to_timespec(jiffies , &start_t);
        transferdata = 0;
        
        for(now_small_loop=0; now_small_loop<target_small_loop; now_small_loop++){

            get_random_bytes(&test_que_no, sizeof(unsigned int));
            test_que_no = test_que_no % target_que;

            get_random_bytes(&now_pktnum, sizeof(unsigned int));
            now_pktnum = (now_pktnum % target_pktnum) + 1;
            
			for (idx = 0 ; idx < now_pktnum ; idx ++) {
                get_random_bytes(&now_size, sizeof(unsigned int));
                now_size = (now_size % (target_size_max - target_size_min) ) + target_size_min;
                transferdata += now_size;
                
                //KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] pkt size this time = %d !\n",__FUNCTION__, now_size));    
				ret = sdio_send_pkt(test_que_no, now_size, 0, 0);
                expect_num++;
                
				if (ret != RET_SUCCESS) {
				    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
				    //send packet fail and retry
                    idx --;
                    return ret;
				} 

                //for atcmd loopback test
                //get_random_bytes(&rand_num, sizeof(unsigned int));
                //rand_num = rand_num & 0xFF;
                //if (rand_num <= atcmd_threhold){
                //    get_random_bytes(&rand_len, sizeof(unsigned int));
                //    rand_len = rand_len & 0x3F;
                //    get_random_bytes(cmd.buf, rand_len);
                //   cmd.len = rand_len;
                //    
                //    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
                //    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
                //    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

                //    if (0 != memcmp(cmd.buf, athif_result_save_t->buf, rand_len) ){
                //        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR]  data of atcmd compare fail !!! \n",KAL_FUNC_NAME)) ;
                //        return RET_FAIL;
                //    }
                //    else{
                //        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[log]  Send a atcmd with len %d and compare success. \n",KAL_FUNC_NAME, rand_len)) ;
                //    }
                //}
                    
			}
                
            // test debug
            //KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s]transfered pkt at que=%d, pkt_num=%d\n",  __FUNCTION__, test_que_no, now_pktnum));
        }

        // exam the packet num which receive.
        while(expect_num != recv_total_pkt_cnt){
        
            KAL_SLEEP_MSEC(1) ;
            if(recv_total_pkt_cnt != old_pkt_cnt){ timeout ==0; }
            else{ timeout++; }
        
            if(timeout > 1000){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;
                return RET_FAIL;
            }
            old_pkt_cnt = recv_total_pkt_cnt;

            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] loop %d is finished\n",  __FUNCTION__, now_big_loop));
        }

        jiffies_to_timespec(jiffies , &end_t);
		diff_t = time_diff(start_t, end_t);
		diff_ms = (1000 * diff_t.tv_sec) ;
		diff_ms += (diff_t.tv_nsec / 1000000);
 		performance = ((unsigned int)transferdata / (unsigned int)diff_ms);

		KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] performance = %d KBPS\n", __FUNCTION__, performance ));
		KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] transfered data=%u\n", __FUNCTION__, transferdata));
		KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] diff_ms=%u\n", __FUNCTION__, diff_ms));
        
	}

    tx_deq_mode = ATHIF_FWD_FREE_ONLY;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_RGPD_RL_MD;
    cmd.buf[0] = ATHIF_RGPD_BASIC;
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    sdio_test_option.exam_dl_content = false;
    sdio_test_option.auto_receive_pkt = false;
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    if(recv_th_rslt == RET_SUCCESS){
	    return ret;
    }else{
        return RET_FAIL;
    }
}

/*
static int t_dev_brom_sync_test(int argc , char **argv)
{
    unsigned int mb_num, wr_val, rd_val;
    unsigned int timeout;

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    
    wr_val = 0x72088888;
    rd_val = 0x00000000;
    timeout = 0;
    
    test_h2d_mailbox_wr(0, &wr_val);

    test_d2h_mailbox_rd(0, &rd_val);
    while(rd_val != 0x88887208){
        timeout++;
        KAL_SLEEP_MSEC(1) ;
        if (timeout > 100){
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] brom sync timeout at step1 0x88887208!! \n", __FUNCTION__));
    	    return RET_FAIL;
        }
        test_d2h_mailbox_rd(0, &rd_val);
    }

    wr_val = 0x00000000;
    test_h2d_mailbox_wr(0, &wr_val);
    
    test_d2h_mailbox_rd(0, &rd_val);
    while(rd_val != 0x00000000){
        timeout++;
        KAL_SLEEP_MSEC(1) ;
        if (timeout > 100){
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] brom sync timeout step2 0x00000000!! \n", __FUNCTION__));
    	    return RET_FAIL;
        }
        test_d2h_mailbox_rd(0, &rd_val);
    }

    mtlte_hif_sdio_enable_fw_own_back(1);
    return RET_SUCCESS;
    
}
*/

static int t_dev_brom_sync_test_new(int argc , char **argv)
{
    unsigned int mb_num, wr_val, rd_val;
    unsigned int timeout;
    unsigned int timeout_for_device;

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    timeout_for_device = 10;
    wr_val = 0x47444200 | timeout_for_device;
    rd_val = 0x00000000;
    timeout = 0;
    
    test_h2d_mailbox_wr(0, &wr_val);

    test_d2h_mailbox_rd(0, &rd_val);
    while(rd_val != 0x59474442){
        timeout++;
        KAL_SLEEP_MSEC(1) ;
        if (timeout > 100){
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] brom sync timeout at step1 0x59474442!! \n", __FUNCTION__));
    	    return RET_FAIL;
        }
        test_d2h_mailbox_rd(0, &rd_val);
    }

    wr_val = 0x00000000;
    test_h2d_mailbox_wr(0, &wr_val);
    
    test_d2h_mailbox_rd(0, &rd_val);
    while(rd_val != 0x00000000){
        timeout++;
        KAL_SLEEP_MSEC(1) ;
        if (timeout > 100){
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] brom sync timeout step2 0x00000000!! \n", __FUNCTION__));
    	    return RET_FAIL;
        }
        test_d2h_mailbox_rd(0, &rd_val);
    }

    mtlte_hif_sdio_enable_fw_own_back(1);
    return RET_SUCCESS;
    
}

static int t_dev_brom_sync_timeout_test(int argc , char **argv)
{
    unsigned int mb_num, wr_val, rd_val;
    unsigned int timeout;
    unsigned int timeout_for_device;
    unsigned int idx = 0 , param_num = 0;
    unsigned int arg[MAX_ARG_SIZE-1] ;

    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    timeout_for_device = arg[0];
    wr_val = 0x47444200 | timeout_for_device;
    rd_val = 0x00000000;
    timeout = 0;
    
    test_h2d_mailbox_wr(0, &wr_val);

    test_d2h_mailbox_rd(0, &rd_val);
    while(rd_val != 0x59474442){
        timeout++;
        KAL_SLEEP_MSEC(1) ;
        if (timeout > 100){
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] brom sync timeout at step1 0x59474442!! \n", __FUNCTION__));
    	    return RET_FAIL;
        }
        test_d2h_mailbox_rd(0, &rd_val);
    }


    mtlte_hif_sdio_enable_fw_own_back(1);
    return RET_SUCCESS;
    
}



static int t_dev_brom_lb_test(int argc , char **argv)
{
	unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pattern = 0;
	athif_cmd_t cmd;
	athif_status_t status;
	unsigned int rand_num = 0 , i = 0 , xfer_len = 0 ;

	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    sdio_test_option.auto_receive_pkt = true;
    //sdio_test_option.show_dl_content = true;
    sdio_test_option.exam_dl_content = true;

	pattern = arg[0];


    if (pattern > ATCASE_LB_DATA_INC) {
		pattern = ATCASE_LB_DATA_AUTO;
        ret = f_brom_pkt_lb(pattern, 50, 2048);	
	} else {	
		ret = f_brom_pkt_lb(pattern, 1, 50);	
	}

    sdio_test_option.auto_receive_pkt = false;
    //sdio_test_option.show_dl_content = false;
    sdio_test_option.exam_dl_content = false;
    
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
	return ret;

}


static int t_dev_brom_ioctl_test(int argc , char **argv)
{
	unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pattern = 0;
	athif_cmd_t cmd;
	unsigned int rand_num = 0 , i = 0 , xfer_len = 0 ;
    lb_data_pattern_e org_send_pattern = 0, org_cmp_pattern = 0;
    unsigned char *buf, *check_buf;
    unsigned int data_length;
    PAT_PKT_HEADER pAtHeader = NULL;
	unsigned char rand_seed = 0, bak_seed = 0;
    unsigned char cksm = 0, timeout=0;
    unsigned int temp_read_len, total_read_len, read_len_thistime;
    struct sk_buff *check_skb;
    unsigned int test_time, tt;
    bool bak_auto_receive_pkt;

	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    ret = RET_SUCCESS;
    buf = brom_io_buf;

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

	pattern = arg[0];
    test_time = arg[1];

    if (pattern > ATCASE_LB_DATA_INC) {
		pattern = ATCASE_LB_DATA_AUTO;	
	}

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    //cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
    //cmd.buf[0] = ATHIF_LB_TGPD_EMPTY_ENQ;
	//cmd.len = 1;

	//mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    //mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    //if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    bak_auto_receive_pkt = sdio_test_option.auto_receive_pkt;
    sdio_test_option.auto_receive_pkt = false;
    
    /*backup pattern mode*/
    org_send_pattern = send_pattern;
    org_cmp_pattern = cmp_pattern;
    send_pattern = pattern; 
    cmp_pattern = pattern;

    for(tt=0; tt<test_time; tt++){
        get_random_bytes(&data_length, sizeof(int));
        data_length = (data_length % 2048) +1;
    
        switch (send_pattern) {
    		case ATCASE_LB_DATA_5A :
    			memset(buf, 0x5a , data_length);			
    			break;
    		case ATCASE_LB_DATA_A5:
    			memset(buf, 0xa5 , data_length);			
    			break;
    		case ATCASE_LB_DATA_INC:
    			get_random_bytes(&rand_seed , 1);
    			for (i = 0 ; i < data_length ; i ++) {
    				buf[i] = rand_seed++;
    			}
    			break;
    
    		case ATCASE_LB_DATA_AUTO :
            default:
    			// fill packet payload
    		 	pAtHeader = (PAT_PKT_HEADER)buf;
    			memset(pAtHeader, 0 , sizeof(AT_PKT_HEADER));
    
    			get_random_bytes(&rand_seed , 1);
    			bak_seed = rand_seed;
                KAL_DBGPRINT(KAL, DBG_TRACE,("rand_seed = %d..\n", rand_seed));
    			pAtHeader->RndSeed = rand_seed;
    			pAtHeader->SrcQID = 0;  
    			pAtHeader->DstQID = 0;
    			pAtHeader->SeqNo = 0;
    			if (data_length < sizeof(AT_PKT_HEADER)) {
    				data_length = sizeof(AT_PKT_HEADER); 
    			}
    			pAtHeader->PktLen = data_length;
    			
    			f_calc_cs_byte(pAtHeader, sizeof(AT_PKT_HEADER), &cksm);
    			pAtHeader->Checksum = ~cksm;
    
    			 // fill payload, don't fill memory lenght larger than URB buffer
    			for (i = 0 ; i < (data_length - sizeof(AT_PKT_HEADER)) ; i ++) {
    				pAtHeader->Data[i] = rand_seed++;
    			}
    			break;
    	}
    
        ret = brom_write_pkt(BROM_ULQ, data_length, brom_io_buf);
    	if (ret != data_length) {
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] brom write pkt fail!! data_len=%d  return=%d  \n", __FUNCTION__, data_length, ret));
    	    return RET_FAIL;
    	}
    
        // receive lb data
        total_read_len = 0;
        check_buf = buff_kmemory_ulpkt_data;
        
        while(total_read_len != data_length){
            get_random_bytes(&temp_read_len, sizeof(unsigned int));
            //temp_read_len = (temp_read_len % 512) +1;
            temp_read_len = 4;
            read_len_thistime = brom_read_pkt(BROM_DLQ, temp_read_len, brom_rd_buf);
    
            timeout =0;
            while (0 == read_len_thistime ){
                KAL_SLEEP_MSEC(1) ;
                timeout++;
                if(timeout > 100){
                    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] brom read pkt timeout!! data_len=%d  now len=%d  \n", __FUNCTION__, data_length, total_read_len));
    	            return RET_FAIL;
    	        }
                
                read_len_thistime = brom_read_pkt(BROM_DLQ, temp_read_len, brom_rd_buf);
            } 
            
            memcpy( (check_buf+total_read_len), brom_rd_buf, read_len_thistime) ;
            total_read_len = total_read_len + read_len_thistime;
        }
    
        // compare lb data
        if ((check_skb = dev_alloc_skb(data_length))==NULL){
    		KAL_DBGPRINT(KAL, DBG_ERROR,("%s : allocate skb failed\n", KAL_FUNC_NAME));
    		return RET_FAIL ;	
        }
        memcpy(skb_put(check_skb, data_length), check_buf, data_length);
        if ( RET_SUCCESS != f_compare_recv_pkt(check_skb, BROM_DLQ) ){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] data compare error at brom test!!! \n", KAL_FUNC_NAME)) ;
            return RET_FAIL ;
        }
    }

    /*restore pattern mode*/
	send_pattern = org_send_pattern;
	cmp_pattern = org_cmp_pattern;

    sdio_test_option.auto_receive_pkt = bak_auto_receive_pkt;
    
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
	return RET_SUCCESS;

}


#define SDIOMB_BOOT_REQ_MAGIC 0x53444254 // "SDBT"
#define SDIOMB_BOOT_ACK_MAGIC 0x53425400 // "SBTx"
#define SDIOMB_DOWNLOAD_REQ_MAGIC 0x5344444C // "SDDL"
#define SDIOMB_DOWNLOAD_ACK_MAGIC 0x53444C00 // "REFU"
#define SDIOMB_REQ_REFUSE_MAGIC 0x52454655 // "REFU"
#define SDIOMB_ACK_TIMEOUT_MASK 0x000000FF

static int t_dev_brom_sync_test_xboot(int argc , char **argv)
{
    unsigned int mb_num, wr_val, rd_val;
    unsigned int timeout;
    unsigned int timeout_for_device;

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    
    rd_val = 0x00000000;
    timeout = 0;
    
    test_d2h_mailbox_rd(0, &rd_val);
    while(rd_val != SDIOMB_BOOT_REQ_MAGIC){
        timeout++;
        KAL_SLEEP_MSEC(1) ;
        if (timeout > 100){
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] brom xboot sync timeout at step1 0x53444254!! \n", __FUNCTION__));
    	    return RET_FAIL;
        }
        test_d2h_mailbox_rd(0, &rd_val);
    }

    timeout_for_device = 200;
    wr_val = SDIOMB_BOOT_ACK_MAGIC | timeout_for_device;
    test_h2d_mailbox_wr(0, &wr_val);
    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] brom xboot sync success!! \n", __FUNCTION__));

    mtlte_hif_sdio_enable_fw_own_back(1);
    return RET_SUCCESS;
    
}


static int brom_sync_xboot_no_timeout(void)
{
    unsigned int mb_num, wr_val, rd_val;
    unsigned int timeout_for_device;

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    
    rd_val = 0x00000000;
    
    test_d2h_mailbox_rd(0, &rd_val);
    while(rd_val != SDIOMB_BOOT_REQ_MAGIC){
        
        test_d2h_mailbox_rd(0, &rd_val);
    }

    timeout_for_device = 200;
    wr_val = SDIOMB_BOOT_ACK_MAGIC | timeout_for_device;
    test_h2d_mailbox_wr(0, &wr_val);

    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] brom xboot sync success with upper call!! \n", __FUNCTION__));

    mtlte_hif_sdio_enable_fw_own_back(1);
    return RET_SUCCESS;
    
}


static int brom_sync_gdb_no_timeout(void)
{
    unsigned int mb_num, wr_val, rd_val;
    unsigned int timeout;
    unsigned int timeout_for_device;

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    timeout_for_device = 10;
    wr_val = 0x47444200 | timeout_for_device;
    rd_val = 0x00000000;
    timeout = 0;
    
    test_h2d_mailbox_wr(0, &wr_val);

    test_d2h_mailbox_rd(0, &rd_val);
    while(rd_val != 0x59474442){
        test_d2h_mailbox_rd(0, &rd_val);
    }

    wr_val = 0x00000000;
    test_h2d_mailbox_wr(0, &wr_val);
    
    test_d2h_mailbox_rd(0, &rd_val);
    while(rd_val != 0x00000000){
        test_d2h_mailbox_rd(0, &rd_val);
    }

    mtlte_hif_sdio_enable_fw_own_back(1);
    return RET_SUCCESS;
    
}

static int t_dev_brom_dl_timeout_test(int argc , char **argv)
{
	unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pattern = 0;
	athif_cmd_t cmd;
	unsigned int rand_num = 0 , i = 0 , xfer_len = 0 ;
    lb_data_pattern_e org_send_pattern = 0, org_cmp_pattern = 0;
    unsigned char *buf, *check_buf;
    unsigned int data_length;
    PAT_PKT_HEADER pAtHeader = NULL;
	unsigned char rand_seed = 0, bak_seed = 0;
    unsigned char cksm = 0, timeout=0;
    unsigned int temp_read_len, total_read_len, read_len_thistime;
    struct sk_buff *check_skb;
    unsigned int test_time, tt;

    unsigned int    set_WHIER, backup_WHIER;
    
	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    ret = RET_SUCCESS;
    buf = brom_io_buf;

    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();

    // turn off the RX Done interrupt
    sdio_func1_rd(SDIO_IP_WHIER, &backup_WHIER, 4);
    set_WHIER = backup_WHIER & 0xFFFFFFE1;
    sdio_func1_wr(SDIO_IP_WHIER, &set_WHIER, 4);

    pattern = ATCASE_LB_DATA_5A;	

    /*backup pattern mode*/
    org_send_pattern = send_pattern;
    org_cmp_pattern = cmp_pattern;
    send_pattern = pattern; 
    cmp_pattern = pattern;

        get_random_bytes(&data_length, sizeof(int));
        data_length = (data_length % 2048) +1;
    
    	memset(buf, 0x5a , data_length);			

    
        ret = brom_write_pkt(BROM_ULQ, data_length, brom_io_buf);
    	if (ret != data_length) {
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] brom write pkt fail!! data_len=%d  return=%d  \n", __FUNCTION__, data_length, ret));
    	    return RET_FAIL;
    	}

        KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Now wait and check whether device output error code of send timout?  \n", __FUNCTION__));
    

    /*restore pattern mode*/
	send_pattern = org_send_pattern;
	cmp_pattern = org_cmp_pattern;
    
    mtlte_hif_sdio_enable_fw_own_back(1);

    //because the write task will be excuted after we sleep, and timeout will happen after very long time, so temp mask these instruction.
    //at_mtlte_hif_sdio_give_fw_own();
    //sdio_func1_wr(SDIO_IP_WHIER, &backup_WHIER, 4);
	return RET_SUCCESS;

}


static int t_dev_rx_pkt_cnt_change_test(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
	unsigned int expect_num = 0, timeout=0;
    unsigned int old_pkt_cnt = 0;
    athif_fwd_mode_e tx_deq_mode;
    athif_cmd_t cmd;

    unsigned int changed_dl_que = 0;
    unsigned int changed_pkt_cnt = 0;
    unsigned int orig_WPLRCR = 0;
    unsigned int temp_WPLRCR = 0;
    unsigned int changed_WPLRCR = 0;

    unsigned int orig_WHCR = 0;
    unsigned int changed_WHCR = 0;
    
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    //cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
    //cmd.buf[0] = ATHIF_LB_TGPD_EMPTY_ENQ;
	//cmd.len = 1;

	//mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    //mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    //if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;


	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    sdio_func1_rd(SDIO_IP_WHCR, &orig_WHCR, 4);
    changed_WHCR = orig_WHCR | RPT_OWN_RX_PACKET_LEN;
    sdio_func1_wr(SDIO_IP_WHCR, &changed_WHCR, 4);
    
    sdio_func1_rd(SDIO_IP_WPLRCR, &orig_WPLRCR, 4);

    temp_WPLRCR = 0;
    temp_WPLRCR |= MAX_DL_PKT_CNT<<(8*0);
    temp_WPLRCR |= MAX_DL_PKT_CNT<<(8*1);
    temp_WPLRCR |= MAX_DL_PKT_CNT<<(8*2);
    temp_WPLRCR |= MAX_DL_PKT_CNT<<(8*3);
    
    test_rx_tail_change = 1;

    for(changed_dl_que=0; changed_dl_que<HIF_MAX_DLQ_NUM; changed_dl_que++){
        for(changed_pkt_cnt=1; changed_pkt_cnt<MAX_DL_PKT_CNT; changed_pkt_cnt++){

            recv_total_pkt_cnt = 0;
            expect_num = 0;
    
            changed_WPLRCR = orig_WPLRCR;
            changed_WPLRCR &= (~(RX_RPT_PKT_LEN(changed_dl_que)));
            changed_WPLRCR |= changed_pkt_cnt<<(8*changed_dl_que);

            sdio_func1_wr(SDIO_IP_WPLRCR, &changed_WPLRCR, 4);

            test_rx_pkt_cnt_q0 = MAX_DL_PKT_CNT;
            test_rx_pkt_cnt_q1 = MAX_DL_PKT_CNT;
            test_rx_pkt_cnt_q2 = MAX_DL_PKT_CNT;
            test_rx_pkt_cnt_q3 = MAX_DL_PKT_CNT;

            if(0 == changed_dl_que && changed_pkt_cnt != 0){
                test_rx_pkt_cnt_q0 = ((changed_pkt_cnt & 0x1 == 0x1) ? (changed_pkt_cnt+1) : changed_pkt_cnt);
            }
            if(1 == changed_dl_que && changed_pkt_cnt != 0){
                test_rx_pkt_cnt_q1 = ((changed_pkt_cnt & 0x1 == 0x1) ? (changed_pkt_cnt+1) : changed_pkt_cnt);
            }
            if(2 == changed_dl_que && changed_pkt_cnt != 0){
                test_rx_pkt_cnt_q2 = ((changed_pkt_cnt & 0x1 == 0x1) ? (changed_pkt_cnt+1) : changed_pkt_cnt);
            }
            if(3 == changed_dl_que && changed_pkt_cnt != 0){
                test_rx_pkt_cnt_q3 = ((changed_pkt_cnt & 0x1 == 0x1) ? (changed_pkt_cnt+1) : changed_pkt_cnt);
            }
            

            // TODO:  reset DLQ of device side (stop & start queue)
            
            for(pkt_sz=arg[0]; pkt_sz<=arg[1]; pkt_sz++){ 
	            for(pkt_num=arg[2]; pkt_num<=arg[3]; pkt_num++){
                	for(test_que_no=0; test_que_no<HIF_MAX_ULQ_NUM; test_que_no++){
			            for (idx = 0 ; idx < pkt_num ; idx ++) {
                            KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                                __FUNCTION__, test_que_no, pkt_sz, idx));
				            ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
                            expect_num++;
                    
				            if (ret != RET_SUCCESS) {
					            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					            /*send packet fail and retry*/
                                idx --;
                                return ret;
				            } 
			            }
                        
                    }
                    KAL_SLEEP_MSEC(1) ;
                    
                    // test debug
                    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Passed of pkt_size=%d, pkt_num=%d \n", \
                            __FUNCTION__, pkt_sz, pkt_num));
	            }
	        }


            while(expect_num != recv_total_pkt_cnt){
        
                KAL_SLEEP_MSEC(1) ;
                if(recv_total_pkt_cnt != old_pkt_cnt){ timeout ==0; }
                else{ timeout++; }
        
                if(timeout > 1000){
                    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;
                    return RET_FAIL;
                }
                old_pkt_cnt = recv_total_pkt_cnt;
                
            }

            if(recv_th_rslt != RET_SUCCESS){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] data is not right at pkt_cnt change to %d in dl_que %d test !!! \n",KAL_FUNC_NAME, changed_pkt_cnt, changed_dl_que)) ;
                return RET_FAIL;
            }

            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:pkt_cnt change to %d in dl_que %d test is success !!! \n",KAL_FUNC_NAME, changed_pkt_cnt, changed_dl_que)) ;
        }
    }

    tx_deq_mode = ATHIF_FWD_FREE_ONLY;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    sdio_test_option.exam_dl_content = false;
    sdio_test_option.auto_receive_pkt = false;
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    sdio_func1_wr(SDIO_IP_WPLRCR, &orig_WPLRCR, 4);
    test_rx_pkt_cnt_q0 = (orig_WPLRCR & RX_RPT_PKT_LEN(0)) << (8*0);
    test_rx_pkt_cnt_q1 = (orig_WPLRCR & RX_RPT_PKT_LEN(1)) << (8*1);
    test_rx_pkt_cnt_q2 = (orig_WPLRCR & RX_RPT_PKT_LEN(2)) << (8*2);
    test_rx_pkt_cnt_q3 = (orig_WPLRCR & RX_RPT_PKT_LEN(3)) << (8*3);
    test_rx_tail_change = 0;

    sdio_func1_wr(SDIO_IP_WHCR, &orig_WHCR, 4);

    if(recv_th_rslt == RET_SUCCESS){
	    return ret;
    }else{
        return RET_FAIL;
    }
}


#define DEFAULT_WTMDR    0x00000000
#define DEFAULT_WTMCR    0x00080000
#define DEFAULT_WTMDPCR0 0xF0F0F0F0
#define DEFAULT_WTMDPCR1 0xF0F0F0F0


static int t_dev_test_mode_pattern_test(int argc , char **argv)
{
    unsigned int WTMDR_val;
    unsigned int WTMCR_val;
    unsigned int WTMDPCR0_val;
    unsigned int WTMDPCR1_val;
    unsigned int *buffer_recv, *buffer_send;
    unsigned int comp_pattern0, comp_pattern1;
    unsigned int test_length;

    unsigned int i,j;
    
    
    sdio_func1_rd(SDIO_IP_WTMCR, &WTMCR_val, 4);
    if(1 == (WTMCR_val & TEST_MODE_FW_OWN)>>24){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]: The test mode is not controlable by host !!! \n",KAL_FUNC_NAME)) ;
        return RET_FAIL;
    }

    // Default value Test
    sdio_func1_rd(SDIO_IP_WTMCR, &WTMCR_val, 4);
    sdio_func1_rd(SDIO_IP_WTMDPCR0, &WTMDPCR0_val, 4);
    sdio_func1_rd(SDIO_IP_WTMDPCR1, &WTMDPCR1_val, 4);
/*
    if(DEFAULT_WTMCR != WTMCR_val){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]: Read WTMCR = %x but default should be %x !!! \n",KAL_FUNC_NAME, WTMCR_val, DEFAULT_WTMCR)) ;
        return RET_FAIL;
    }
    if(DEFAULT_WTMDPCR0 != WTMDPCR0_val){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]: Read WTMDPCR0 = %x but default should be %x !!! \n",KAL_FUNC_NAME, WTMDPCR0_val, DEFAULT_WTMDPCR0)) ;
        return RET_FAIL;
    }
    if(DEFAULT_WTMDPCR1 != WTMDPCR1_val){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]: Read WTMDPCR1 = %x but default should be %x !!! \n",KAL_FUNC_NAME, WTMDPCR1_val, DEFAULT_WTMDPCR1)) ;
        return RET_FAIL;
    }
*/
    test_length = 1024;
    buffer_recv = (unsigned int *)buff_kmemory_hwlimit + 2048;
    buffer_send = (unsigned int *)buff_kmemory_hwlimit + 4096;
    
    // Read pattern test

      //32-bit read pattern
    WTMCR_val &= (~TEST_MODE_SELECT);
    sdio_func1_wr(SDIO_IP_WTMCR, &WTMCR_val, 4);

    comp_pattern0 = DEFAULT_WTMDPCR0;
    sdio_func1_wr(SDIO_IP_WTMDPCR0, &comp_pattern0, 4);
    
    sdio_func1_rd(SDIO_IP_WTMDR, buffer_recv, test_length);
    for(i=0; i<(test_length/4); i++){
        if(comp_pattern0 != *(buffer_recv+i) ){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Read addr = %x, Read pattern = %x, expect be %x !!! \n",KAL_FUNC_NAME, i*4, *(buffer_recv+i), comp_pattern0));
            return RET_FAIL;
        }
    }

    for(j=0; j<32; j++){
        comp_pattern0 = 0x1<<j;
        sdio_func1_wr(SDIO_IP_WTMDPCR0, &comp_pattern0, 4);
        
        sdio_func1_rd(SDIO_IP_WTMDR, buffer_recv, test_length);
        
        for(i=0; i<(test_length/4); i++){
            if(comp_pattern0 != *(buffer_recv+i) ){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Read addr = %x, Read pattern = %x, expect be %x !!! \n",KAL_FUNC_NAME, i*4, *(buffer_recv+i), comp_pattern0));
                return RET_FAIL;
            }
        }
    }

    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]: Test mode of 32-bit Read Pass !!! \n",KAL_FUNC_NAME));

      //64-bit read pattern
    WTMCR_val &= (~TEST_MODE_SELECT);
    WTMCR_val |= 0x01;
    sdio_func1_wr(SDIO_IP_WTMCR, &WTMCR_val, 4);
    
    comp_pattern0 = DEFAULT_WTMDPCR0;
    comp_pattern1 = DEFAULT_WTMDPCR1;
    sdio_func1_wr(SDIO_IP_WTMDPCR0, &comp_pattern0, 4);
    sdio_func1_wr(SDIO_IP_WTMDPCR1, &comp_pattern1, 4);
    
    sdio_func1_rd(SDIO_IP_WTMDR, buffer_recv, test_length);
    for(i=0; i<(test_length/8); i++){
        if( comp_pattern0 != *(buffer_recv+(i<<1)) ){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Read addr = %x, Read pattern = %x, expect be %x !!! \n",KAL_FUNC_NAME, i*8, *(buffer_recv+(i<<1)), comp_pattern0));
            return RET_FAIL;
        }

        if( comp_pattern1 != *(buffer_recv+(i<<1)+1) ){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Read addr = %x, Read pattern = %x, expect be %x !!! \n",KAL_FUNC_NAME, i*8+1, *(buffer_recv+(i<<1)+1), comp_pattern1));
            return RET_FAIL;
        }
    }
    
    for(j=0; j<32; j++){
        comp_pattern0 = 0x1<<j;
        comp_pattern1 = 0x80000000>>j;
        sdio_func1_wr(SDIO_IP_WTMDPCR0, &comp_pattern0, 4);
        sdio_func1_wr(SDIO_IP_WTMDPCR1, &comp_pattern1, 4);
        
        sdio_func1_rd(SDIO_IP_WTMDR, buffer_recv, test_length);
        
        for(i=0; i<(test_length/8); i++){
            if( comp_pattern0 != *(buffer_recv+(i<<1)) ){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Read addr = %x, Read pattern = %x, expect be %x !!! \n",KAL_FUNC_NAME, i*8, *(buffer_recv+(i<<1)), comp_pattern0));
                return RET_FAIL;
            }

            if( comp_pattern1 != *(buffer_recv+(i<<1)+1) ){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Read addr = %x, Read pattern = %x, expect be %x !!! \n",KAL_FUNC_NAME, i*8+1, *(buffer_recv+(i<<1)+1), comp_pattern1));
                return RET_FAIL;
            }
        }
    }

    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]: Test mode of 64-bit Read Pass !!! \n",KAL_FUNC_NAME));

    // Write pattern test

      //32-bit write pattern
    WTMCR_val &= (~TEST_MODE_SELECT);
    sdio_func1_wr(SDIO_IP_WTMCR, &WTMCR_val, 4);

    comp_pattern1 = 0x00000000;
    sdio_func1_wr(SDIO_IP_WTMDPCR1, &comp_pattern1, 4);
    memset(buffer_send, 0, test_length);
    sdio_func1_wr(SDIO_IP_WTMDR, buffer_send, test_length);

    sdio_func1_rd(SDIO_IP_WTMCR, &WTMCR_val, 4);
    if(WTMCR_val & TEST_MODE_STATUS){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Device compare data fail with all 0 at 32-bit mode !!! \n",KAL_FUNC_NAME));
        return RET_FAIL;
    }
    
    for(j=0; j<32; j++){
        
        memset(buffer_send, 0, test_length);
        comp_pattern1 = 0x1<<j;
        memcpy(buffer_send+j, &comp_pattern1, 4);
        sdio_func1_wr(SDIO_IP_WTMDR, buffer_send, test_length);
        
        sdio_func1_rd(SDIO_IP_WTMCR, &WTMCR_val, 4);
        if(!(WTMCR_val & TEST_MODE_STATUS)){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Device compare data 0 wrong with bit %d at 32-bit mode !!! \n",KAL_FUNC_NAME, j));
            return RET_FAIL;
        }
    }

    comp_pattern1 = 0xFFFFFFFFF;
    sdio_func1_wr(SDIO_IP_WTMDPCR1, &comp_pattern1, 4);
    memset(buffer_send, 0xFF, test_length);
    sdio_func1_wr(SDIO_IP_WTMDR, buffer_send, test_length);

    sdio_func1_rd(SDIO_IP_WTMCR, &WTMCR_val, 4);
    if(WTMCR_val & TEST_MODE_STATUS){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Device compare data fail with all 1 at 32-bit mode !!! \n",KAL_FUNC_NAME));
        return RET_FAIL;
    }
    
    for(j=0; j<32; j++){
        
        memset(buffer_send, 0xFF, test_length);
        comp_pattern1 = ~(0x1<<j);
        memcpy(buffer_send+j, &comp_pattern1, 4);
        sdio_func1_wr(SDIO_IP_WTMDR, buffer_send, test_length);
        
        sdio_func1_rd(SDIO_IP_WTMCR, &WTMCR_val, 4);
        if(!(WTMCR_val & TEST_MODE_STATUS)){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Device compare data 1 wrong with bit %d at 32-bit mode !!! \n",KAL_FUNC_NAME, j));
            return RET_FAIL;
        }
    }

    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]: Test mode of 32-bit Write Pass !!! \n",KAL_FUNC_NAME));

          //64-bit write pattern
    WTMCR_val &= (~TEST_MODE_SELECT);
    WTMCR_val |= 0x01;
    sdio_func1_wr(SDIO_IP_WTMCR, &WTMCR_val, 4);

    comp_pattern0 = 0x00000000;
    comp_pattern1 = 0x00000000;
    sdio_func1_wr(SDIO_IP_WTMDPCR0, &comp_pattern0, 4);
    sdio_func1_wr(SDIO_IP_WTMDPCR1, &comp_pattern1, 4);
    memset(buffer_send, 0, test_length);
    sdio_func1_wr(SDIO_IP_WTMDR, buffer_send, test_length);

    sdio_func1_rd(SDIO_IP_WTMCR, &WTMCR_val, 4);
    if(WTMCR_val & TEST_MODE_STATUS){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Device compare data fail with all 0 at 64-bit mode !!! \n",KAL_FUNC_NAME));
        return RET_FAIL;
    }
    
    for(j=0; j<64; j++){
        
        memset(buffer_send, 0, test_length);
        if(j<32){
            comp_pattern0 = 0x1<<j;
            comp_pattern1 = 0x0;
        }else{
            comp_pattern0 = 0x0;
            comp_pattern1 = 0x1<<(j-32);
        }
        memcpy(buffer_send+(j<<1), &comp_pattern0, 4);
        memcpy(buffer_send+(j<<1)+1, &comp_pattern1, 4);
        sdio_func1_wr(SDIO_IP_WTMDR, buffer_send, test_length);
        
        sdio_func1_rd(SDIO_IP_WTMCR, &WTMCR_val, 4);
        if(!(WTMCR_val & TEST_MODE_STATUS)){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Device compare data 0 wrong with bit %d at 64-bit mode !!! \n",KAL_FUNC_NAME, j));
            return RET_FAIL;
        }
    }

    comp_pattern0 = 0xFFFFFFFFF;
    comp_pattern1 = 0xFFFFFFFFF;
    sdio_func1_wr(SDIO_IP_WTMDPCR0, &comp_pattern0, 4);
    sdio_func1_wr(SDIO_IP_WTMDPCR1, &comp_pattern1, 4);
    memset(buffer_send, 0xFF, test_length);
    sdio_func1_wr(SDIO_IP_WTMDR, buffer_send, test_length);

    sdio_func1_rd(SDIO_IP_WTMCR, &WTMCR_val, 4);
    if(WTMCR_val & TEST_MODE_STATUS){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Device compare data fail with all 1 at 64-bit mode !!! \n",KAL_FUNC_NAME));
        return RET_FAIL;
    }
    
    for(j=0; j<64; j++){
        
        memset(buffer_send, 0xFF, test_length);
        if(j<32){
            comp_pattern0 = ~(0x1<<j);
            comp_pattern1 = ~(0x0);
        }else{
            comp_pattern0 = ~(0x0);
            comp_pattern1 = ~(0x1<<(j-32));
        }
        memcpy(buffer_send+(j<<1), &comp_pattern0, 4);
        memcpy(buffer_send+(j<<1)+1, &comp_pattern1, 4);
        sdio_func1_wr(SDIO_IP_WTMDR, buffer_send, test_length);
        
        sdio_func1_rd(SDIO_IP_WTMCR, &WTMCR_val, 4);
        if(!(WTMCR_val & TEST_MODE_STATUS)){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] Device compare data 1 wrong with bit %d at 64-bit mode !!! \n",KAL_FUNC_NAME, j));
            return RET_FAIL;
        }
    }

    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]: Test mode of 64-bit Write Pass !!! \n",KAL_FUNC_NAME));
    
    return RET_SUCCESS;
}


/*  set Rx packet length report number
 *  arg[0]     : Switch - 0:use default(64), 1:use per RxQ setting
 *  arg[1~4] : RxQ report length setting if arg[0] = 1 
 */
static int t_dev_set_max_rx_pkt(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    
    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}
    
    // temp set rx report length
    unsigned int orig_WHCR = 0;
    unsigned int changed_WHCR = 0;
    unsigned int orig_WPLRCR = 0;
    unsigned int changed_WPLRCR = 0;

    if(0 == arg[0]){
        
        // close the function of per Rx queue adjust, back to default 64
        sdio_func1_rd(SDIO_IP_WHCR, &orig_WHCR, 4);
        changed_WHCR = orig_WHCR & (~RPT_OWN_RX_PACKET_LEN);
        sdio_func1_wr(SDIO_IP_WHCR, &changed_WHCR, 4);

        test_rx_tail_change = 0;
        
    }else{
        // open the function of per Rx queue adjust, each Rx queue is use setting param.
        sdio_func1_rd(SDIO_IP_WHCR, &orig_WHCR, 4);
        changed_WHCR = orig_WHCR | RPT_OWN_RX_PACKET_LEN;
        sdio_func1_wr(SDIO_IP_WHCR, &changed_WHCR, 4);

        sdio_func1_rd(SDIO_IP_WPLRCR, &orig_WPLRCR, 4);
        changed_WPLRCR = orig_WPLRCR;
        changed_WPLRCR &= (~(RX_RPT_PKT_LEN(0)));
        changed_WPLRCR |= arg[1]<<(8*0);
        changed_WPLRCR &= (~(RX_RPT_PKT_LEN(1)));
        changed_WPLRCR |= arg[2]<<(8*1);
        changed_WPLRCR &= (~(RX_RPT_PKT_LEN(2)));
        changed_WPLRCR |= arg[3]<<(8*2);
        changed_WPLRCR &= (~(RX_RPT_PKT_LEN(3)));
        changed_WPLRCR |= arg[4]<<(8*3);

        sdio_func1_wr(SDIO_IP_WPLRCR, &changed_WPLRCR, 4);

        test_rx_tail_change = 1;
        test_rx_pkt_cnt_q0 = arg[1];
        test_rx_pkt_cnt_q1 = arg[2];
        test_rx_pkt_cnt_q2 = arg[3];
        test_rx_pkt_cnt_q3 = arg[4];
        
    }
    
    mtlte_hif_sdio_enable_fw_own_back(1);

    return RET_SUCCESS;
}



static int t_dev_max_rx_pkt_test(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
	int ret = RET_SUCCESS;
	unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
	unsigned int expect_num = 0, timeout=0;
    unsigned int old_pkt_cnt = 0;
    athif_fwd_mode_e tx_deq_mode;
    athif_cmd_t cmd;

    unsigned int changed_dl_que = 0;
    unsigned int changed_pkt_cnt = 0;
    unsigned int orig_WPLRCR = 0;
    unsigned int changed_WPLRCR = 0;
    unsigned int basic_WPLRCR = 0;

    unsigned int orig_WHCR = 0;
    unsigned int changed_WHCR = 0;
    
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    //cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
    //cmd.buf[0] = ATHIF_LB_TGPD_EMPTY_ENQ;
	//cmd.len = 1;

	//mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    //mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    //if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;


	param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    sdio_func1_rd(SDIO_IP_WHCR, &orig_WHCR, 4);
    changed_WHCR = orig_WHCR | RPT_OWN_RX_PACKET_LEN;
    sdio_func1_wr(SDIO_IP_WHCR, &changed_WHCR, 4);
    
    sdio_func1_rd(SDIO_IP_WPLRCR, &orig_WPLRCR, 4);
    //changed_WPLRCR = orig_WPLRCR;
    //changed_WPLRCR &= (~(RX_RPT_PKT_LEN(changed_dl_que)));
    //changed_WPLRCR |= changed_pkt_cnt<<(8*changed_dl_que);
    basic_WPLRCR = 0;
    basic_WPLRCR |= MAX_DL_PKT_CNT<<(8*0);
    basic_WPLRCR |= MAX_DL_PKT_CNT<<(8*1);
    basic_WPLRCR |= MAX_DL_PKT_CNT<<(8*2);
    basic_WPLRCR |= MAX_DL_PKT_CNT<<(8*3);
    sdio_func1_wr(SDIO_IP_WPLRCR, &basic_WPLRCR, 4);
    
    test_rx_tail_change = 1;

    for(changed_dl_que=0; changed_dl_que<HIF_MAX_DLQ_NUM; changed_dl_que++){
        for(changed_pkt_cnt=0; changed_pkt_cnt<HW_MAX_DL_PKT_CNT; changed_pkt_cnt++){

            recv_total_pkt_cnt = 0;
            expect_num = 0;
    
            changed_WPLRCR = basic_WPLRCR;
            changed_WPLRCR &= (~(RX_RPT_PKT_LEN(changed_dl_que)));
            changed_WPLRCR |= changed_pkt_cnt<<(8*changed_dl_que);

            sdio_func1_wr(SDIO_IP_WPLRCR, &changed_WPLRCR, 4);

            test_rx_pkt_cnt_q0 = MAX_DL_PKT_CNT;
            test_rx_pkt_cnt_q1 = MAX_DL_PKT_CNT;
            test_rx_pkt_cnt_q2 = MAX_DL_PKT_CNT;
            test_rx_pkt_cnt_q3 = MAX_DL_PKT_CNT;

            if(0 == changed_dl_que ){
                if(changed_pkt_cnt != 0)
                    test_rx_pkt_cnt_q0 = ((changed_pkt_cnt & 0x1 == 0x1) ? (changed_pkt_cnt+1) : changed_pkt_cnt);
                else
                    test_rx_pkt_cnt_q0 = HW_MAX_DL_PKT_CNT;
            }
            if(1 == changed_dl_que ){
                if(changed_pkt_cnt != 0)
                    test_rx_pkt_cnt_q1 = ((changed_pkt_cnt & 0x1 == 0x1) ? (changed_pkt_cnt+1) : changed_pkt_cnt);
                else
                    test_rx_pkt_cnt_q1 = HW_MAX_DL_PKT_CNT;
            }
            if(2 == changed_dl_que ){
                if(changed_pkt_cnt != 0)
                    test_rx_pkt_cnt_q2 = ((changed_pkt_cnt & 0x1 == 0x1) ? (changed_pkt_cnt+1) : changed_pkt_cnt);
                else
                    test_rx_pkt_cnt_q2 = HW_MAX_DL_PKT_CNT;
            }
            if(3 == changed_dl_que ){
                if(changed_pkt_cnt != 0)
                    test_rx_pkt_cnt_q3 = ((changed_pkt_cnt & 0x1 == 0x1) ? (changed_pkt_cnt+1) : changed_pkt_cnt);
                else
                    test_rx_pkt_cnt_q3 = HW_MAX_DL_PKT_CNT;
            }
            

            // TODO:  reset DLQ of device side (stop & start queue)
            
            for(pkt_sz=arg[0]; pkt_sz<=arg[1]; pkt_sz++){ 
	            for(pkt_num=arg[2]; pkt_num<=arg[3]; pkt_num++){
                	for(test_que_no=0; test_que_no<HIF_MAX_ULQ_NUM; test_que_no++){
			            for (idx = 0 ; idx < pkt_num ; idx ++) {
                            KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                                __FUNCTION__, test_que_no, pkt_sz, idx));
				            ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
                            expect_num++;
                    
				            if (ret != RET_SUCCESS) {
					            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					            /*send packet fail and retry*/
                                idx --;
                                return ret;
				            } 
			            }
                        
                    }
                    KAL_SLEEP_MSEC(1) ;
                    
                    // test debug
                    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Passed of pkt_size=%d, pkt_num=%d \n", \
                            __FUNCTION__, pkt_sz, pkt_num));
	            }
	        }


            while(expect_num != recv_total_pkt_cnt){
        
                KAL_SLEEP_MSEC(1) ;
                if(recv_total_pkt_cnt != old_pkt_cnt){ timeout ==0; }
                else{ timeout++; }
        
                if(timeout > 1000){
                    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;
                    return RET_FAIL;
                }
                old_pkt_cnt = recv_total_pkt_cnt;
                
            }

            if(recv_th_rslt != RET_SUCCESS){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] data is not right at pkt_cnt change to %d in dl_que %d test !!! \n",KAL_FUNC_NAME, changed_pkt_cnt, changed_dl_que)) ;
                return RET_FAIL;
            }

            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:pkt_cnt change to %d in dl_que %d test is success !!! \n",KAL_FUNC_NAME, changed_pkt_cnt, changed_dl_que)) ;
        }
    }

    tx_deq_mode = ATHIF_FWD_FREE_ONLY;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_SET_FWD_MD, (char *)&tx_deq_mode, sizeof(athif_fwd_mode_e), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    sdio_test_option.exam_dl_content = false;
    sdio_test_option.auto_receive_pkt = false;
    mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();

    sdio_func1_wr(SDIO_IP_WPLRCR, &orig_WPLRCR, 4);
    test_rx_pkt_cnt_q0 = (orig_WPLRCR & RX_RPT_PKT_LEN(0)) >> (8*0);
    test_rx_pkt_cnt_q1 = (orig_WPLRCR & RX_RPT_PKT_LEN(1)) >> (8*1);
    test_rx_pkt_cnt_q2 = (orig_WPLRCR & RX_RPT_PKT_LEN(2)) >> (8*2);
    test_rx_pkt_cnt_q3 = (orig_WPLRCR & RX_RPT_PKT_LEN(3)) >> (8*3);
    test_rx_tail_change = 0;

    sdio_func1_wr(SDIO_IP_WHCR, &orig_WHCR, 4);

    if(recv_th_rslt == RET_SUCCESS){
	    return ret;
    }else{
        return RET_FAIL;
    }
}

/*
extern void lte_sdio_off(void); 
extern void lte_sdio_on(void);

static MTLTE_DF_TO_DEV_CALLBACK test_wd_reset_callback(void)
{
    lte_sdio_off();
    lte_sdio_on();
}
*/

static int t_dev_set_wd_reset(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
    athif_cmd_t cmd;
	athif_status_t status;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    
    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    cmd.cmd = SDIO_AT_WD_RESET;
	cmd.buf[0] = (unsigned char)arg[0];
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    
    mtlte_hif_sdio_enable_fw_own_back(1);

    //mtlte_df_register_WDT_callback(test_wd_reset_callback);

    return RET_SUCCESS;
}

static int t_dev_read_WCIR(int argc , char **argv)
{
    unsigned int WCIR_val;
    

    return RET_SUCCESS;
}

static int t_dev_device_self_sleep(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
    athif_cmd_t cmd;
	athif_status_t status;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    
    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    cmd.cmd = SDIO_AT_SELF_SLEEP;
	cmd.buf[0] = (unsigned char)arg[0];
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
    mtlte_hif_sdio_enable_fw_own_back(1);

    return RET_SUCCESS;
}


static int t_dev_device_wake_event_test(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
    athif_cmd_t cmd;
	athif_status_t status;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    
    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    if(0 == arg[0]){
        // test SM_F32K_SDCTL_BUSY_HOST event
        
        // init TOPSM & enable sleep after own back test
        cmd.cmd = SDIO_AT_WAKEUP_EVENT_TEST;
    	cmd.buf[0] = 0;
    	cmd.len = 1;

    	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        // test SM_F32K_SDCTL_BUSY_HOST event
        cmd.cmd = SDIO_AT_WAKEUP_EVENT_TEST;
    	cmd.buf[0] = 1;
    	cmd.len = 1; //enable SM_F32K_SDCTL_BUSY_HOST

    	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        KAL_SLEEP_MSEC(2000) ;
        
        mtlte_hif_sdio_enable_fw_own_back(1);
        at_mtlte_hif_sdio_give_fw_own();

        KAL_SLEEP_MSEC(2000) ;
        
        mtlte_hif_sdio_enable_fw_own_back(0);
        at_mtlte_hif_sdio_get_driver_own();

        // Disable all event
        cmd.cmd = SDIO_AT_WAKEUP_EVENT_TEST;
    	cmd.buf[0] = 0xFF;
    	cmd.len = 1; //enable SM_F32K_SDCTL_BUSY_HOST
    	
    	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
        
    }

    if(1 == arg[0]){
        // test SM_F32K_SDCTL_FW_INT_LV event
        
        // init TOPSM & enable sleep after own back test
        cmd.cmd = SDIO_AT_WAKEUP_EVENT_TEST;
    	cmd.buf[0] = 0;
    	cmd.len = 1;

    	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        // test SM_F32K_SDCTL_FW_INT_LV event
        cmd.cmd = SDIO_AT_WAKEUP_EVENT_TEST;
    	cmd.buf[0] = 2;
    	cmd.len = 1; //enable SM_F32K_SDCTL_FW_INT_LV

    	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        KAL_SLEEP_MSEC(2000) ;
        
        mtlte_hif_sdio_enable_fw_own_back(1);
        at_mtlte_hif_sdio_give_fw_own();

        KAL_SLEEP_MSEC(3000) ;
        
        mtlte_hif_sdio_enable_fw_own_back(0);
        at_mtlte_hif_sdio_get_driver_own();

        // Disable all event
        cmd.cmd = SDIO_AT_WAKEUP_EVENT_TEST;
    	cmd.buf[0] = 0xFF;
    	cmd.len = 1; //enable SM_F32K_SDCTL_BUSY_HOST
    	
    	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    }

    if( 2 == arg[0]){
        // test SM_NON_F32K_SDCTL_BUSY event
            
        // init TOPSM & enable sleep after own back test
        cmd.cmd = SDIO_AT_WAKEUP_EVENT_TEST;
    	cmd.buf[0] = 0;
    	cmd.len = 1;

    	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        // test SM_NON_F32K_SDCTL_BUSY event
        cmd.cmd = SDIO_AT_WAKEUP_EVENT_TEST;
    	cmd.buf[0] = 3;
    	cmd.len = 1; //enable SM_NON_F32K_SDCTL_BUSY

    	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        
        // device will test by itself and report success or fail
        KAL_SLEEP_MSEC(5000) ;
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        // Disable all event
        cmd.cmd = SDIO_AT_WAKEUP_EVENT_TEST;
    	cmd.buf[0] = 0xFF;
    	cmd.len = 1; //enable SM_F32K_SDCTL_BUSY_HOST
    	
    	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
        
    }

    if( 3 == arg[0]){
        // test if device should not be wake-up if all wake-up event is disable

        // Disable all event
        cmd.cmd = SDIO_AT_WAKEUP_EVENT_TEST;
    	cmd.buf[0] = 0xFF;
    	cmd.len = 1; //enable SM_F32K_SDCTL_BUSY_HOST
    	
    	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
        
        cmd.cmd = SDIO_AT_WAKEUP_EVENT_TEST;
    	cmd.buf[0] = 0;
    	cmd.len = 1;

    	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        KAL_SLEEP_MSEC(2000) ;
        
        mtlte_hif_sdio_enable_fw_own_back(1);
        at_mtlte_hif_sdio_give_fw_own();

        KAL_SLEEP_MSEC(3000) ;
        
        mtlte_hif_sdio_enable_fw_own_back(0);

        //should fail
        if(KAL_SUCCESS == at_mtlte_hif_sdio_get_driver_own()){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:Fail because device is woke-up while no wake-up event set!!! \n",KAL_FUNC_NAME)) ;
            return RET_FAIL;
        }

        KAL_SLEEP_MSEC(20000) ;
        at_mtlte_hif_sdio_get_driver_own();

        // Disable all event
        cmd.cmd = SDIO_AT_WAKEUP_EVENT_TEST;
    	cmd.buf[0] = 0xFF;
    	cmd.len = 1; //enable SM_F32K_SDCTL_BUSY_HOST
    	
    	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
        
    }

    if( 4 == arg[0]){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:only set wake-up event set!!! \n",KAL_FUNC_NAME)) ;
    }

    return RET_SUCCESS;
}


static int t_dev_device_set_wake_eint(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
    athif_cmd_t cmd;
	athif_status_t status;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    
    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    cmd.cmd = SDIO_AT_SET_WAKEUP_EINT;
    cmd.buf[0] = 8;
    cmd.len = 1;
    
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
}

static int t_dev_kal_msec_sleep(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;    
    
    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    KAL_SLEEP_MSEC(arg[0]);
}


static int t_dev_enable_auto_sleep(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
    athif_cmd_t cmd;
	athif_status_t status;
    athif_auto_sm_cfg_t *sm_cfg_ptr;
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    
    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}


    cmd.cmd = ATHIF_CMD_EN_AUTO_SLEEP_MD;
    sm_cfg_ptr = (athif_auto_sm_cfg_t *)cmd.buf;
    memset(sm_cfg_ptr, 0, sizeof(athif_auto_sm_cfg_t));
    sm_cfg_ptr->enable = (kal_bool)arg[0];
    sm_cfg_ptr->rtc_wk_en = (kal_bool)arg[2];
    sm_cfg_ptr->sleep_dur_ms= arg[1];
    sm_cfg_ptr->rtc_wk_dur_sec= arg[3];
    cmd.len = sizeof(athif_auto_sm_cfg_t);
    
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    //mtlte_hif_sdio_enable_fw_own_back(1);
    //at_mtlte_hif_sdio_give_fw_own();
}

static int t_dev_give_own_back(int argc , char **argv)
{
    mtlte_hif_sdio_enable_fw_own_back(1);
    at_mtlte_hif_sdio_give_fw_own();

    KAL_SLEEP_MSEC(1);
}


volatile unsigned int test_exception_msgid = 99;

void test_exception_callback(KAL_UINT32 msgid)
{

    if(msgid == EX_INIT)
    {
        //sdio_test_option.auto_receive_pkt = false;
        KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Exception phase %d !! Packet received right now = %d \n", \
                        __FUNCTION__, msgid, recv_total_pkt_cnt));

        recv_total_pkt_cnt = 0;
    }
    else if(msgid == EX_DHL_DL_RDY)
    {
        KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Exception phase %d !! \n", \
                        __FUNCTION__, msgid));
        
        if(0 != recv_total_pkt_cnt)
        {
            KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Error !! DL Packet transfer to upper layer at Exception phase %d \n", \
                        __FUNCTION__, EX_DHL_DL_RDY));
        }
        //sdio_test_option.auto_receive_pkt = true;
    }
    else if(msgid == EX_INIT_DONE)
    {
        KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Exception phase %d !! Packet received duing phase 1 = %d \n", \
                        __FUNCTION__, msgid, recv_total_pkt_cnt));

        recv_total_pkt_cnt = 0;
    }
    else
    {
        KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Exception phase ERROR !! now phase = %d \n", \
                        __FUNCTION__, msgid));
    }

    test_exception_msgid = msgid;
    
}

static int t_dev_exception_dump_test(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;
    athif_cmd_t cmd;
	athif_status_t status;
    unsigned int non_stopq, expt_que, loop_cnt;
    unsigned int *non_stopq_ptr, *expt_que_ptr, *loop_cnt_ptr;
    unsigned int ret;
    unsigned int pkt_num = 0, test_que_no = 0 , pkt_sz = 0;
    unsigned int except_que_num = 0;
    unsigned int expect_num = 0, timeout=0;
    unsigned int old_pkt_cnt = 0;
    unsigned int i;
    unsigned int int_temp_mask;
        
    
    mtlte_hif_sdio_enable_fw_own_back(0);
    at_mtlte_hif_sdio_get_driver_own();
    
    param_num = argc - 1;
	/*first argument is the command string*/
	for (idx = 1 ; idx < argc ; idx ++) {
		/*translate number string to value*/
		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    non_stopq = arg[0];
    expt_que = (arg[1] | (arg[2]<<16));  
    loop_cnt = arg[3];

    mtlte_expt_register_callback(test_exception_callback);

    mtlte_expt_q_num_init(non_stopq, expt_que);


    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_SET_FWD_MD;
	cmd.buf[0] = ATHIF_FWD_LOOPBACK;
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

    //cmd.cmd = ATHIF_CMD_SET_LB_TYPE;
    //cmd.buf[0] = ATHIF_LB_TGPD_EMPTY_ENQ;
	//cmd.len = 1;

	//mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    //mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    //if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;

    for(test_que_no=0; test_que_no<HIF_MAX_ULQ_NUM; test_que_no++){
        for(pkt_sz=100; pkt_sz<=103; pkt_sz++){ 
	        for(pkt_num=10; pkt_num<=10; pkt_num++){
			    for (idx = 0 ; idx < pkt_num ; idx ++) {
                    KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                        __FUNCTION__, test_que_no, pkt_sz, idx));
				    ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
                    expect_num++;
                    
				    if (ret != RET_SUCCESS) {
					    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
					    /*send packet fail and retry*/
                        idx --;
                        return ret;
				    } 
			    }
            }
            // test debug
            //KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Passe of que=%d, pkt_size=%d\n", \
            //            __FUNCTION__, test_que_no, pkt_sz));
	    }
    }

    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Each que sent %d packets, Total sent packet number before exception = %d\n", \
                        __FUNCTION__, expect_num/HIF_MAX_ULQ_NUM, expect_num));


    if(KAL_SUCCESS != sdio_func1_rd(SDIO_IP_WHIER, &int_temp_mask, 4)) return RET_FAIL;
    int_temp_mask = int_temp_mask & (~(0xf << 1));
    if(KAL_SUCCESS != sdio_func1_wr(SDIO_IP_WHIER, &int_temp_mask, 4)) return RET_FAIL;
    
    KAL_SLEEP_MSEC(1) ;

    cmd.cmd = SDIO_AT_EXCEPTION_TEST;
    non_stopq_ptr = (unsigned int *)cmd.buf;
    expt_que_ptr = (unsigned int *)(cmd.buf+4);
    loop_cnt_ptr = (unsigned int *)(cmd.buf+8);

    *non_stopq_ptr = non_stopq;
    *expt_que_ptr = expt_que;
    *loop_cnt_ptr = loop_cnt;
    
    cmd.len = 12;
    
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);

    while(test_exception_msgid != EX_INIT_DONE)
    {}

    send_pattern = ATCASE_LB_DATA_AUTO;
    sdio_test_option.exam_dl_content = true;
    sdio_test_option.auto_receive_pkt = true;
    recv_th_rslt = RET_SUCCESS;
    recv_total_pkt_cnt = 0;

    expect_num = 0;
    test_que_no=0;
    pkt_sz=300;


    while(expect_num < loop_cnt)
    {
        test_que_no++;
        pkt_sz +=3;
        
        if(test_que_no = HIF_MAX_ULQ_NUM){
            test_que_no = 0;
        }

        if(arg[2] & ((0x1)<<test_que_no) )
        {
        
            KAL_DBGPRINT(KAL, DBG_TRACE, ("[%s] sending in que=%d, pkt_size=%d, pkt_num=%d ...\n", \
                            __FUNCTION__, test_que_no, pkt_sz, idx));
    		ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
            expect_num++;

            if (ret != RET_SUCCESS) {
    		    KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] Tx basic test fail at sent pkt !\n",__FUNCTION__));
    		    /*send packet fail and retry*/
                return ret;
    		} 
        }
        else
        {        
            while(mtlte_df_UL_swq_space(test_que_no)!=0){
                ret = sdio_send_pkt(test_que_no, pkt_sz, 0, 0);
            }
        }
        
    }

     while(expect_num != recv_total_pkt_cnt){
    
        KAL_SLEEP_MSEC(1) ;
        if(recv_total_pkt_cnt != old_pkt_cnt){ timeout ==0; }
        else{ timeout++; }
        
        if(timeout > 1000){
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, expect_num)) ;
            return RET_FAIL;
        }
        old_pkt_cnt = recv_total_pkt_cnt;
    }
         
        

    return RET_SUCCESS;
   
}


/*
static int t_dev_set_abnormal_stall(int argc , char **argv)
{
    unsigned int idx = 0 , param_num = 0;
	unsigned int arg[MAX_ARG_SIZE-1] ;

    param_num = argc - 1;

	for (idx = 1 ; idx < argc ; idx ++) {

		arg[idx-1] = str_to_int(argv[idx]);
		printk(KERN_ERR "[%s] param[%d] = %d\n",__FUNCTION__,idx-1, arg[idx-1]);
	}

    if(1 == arg[0]){
        at_mtlte_hif_sdio_reset_abnormal_disable();
    }
    else{
        at_mtlte_hif_sdio_reset_abnormal_enable(arg[1]);
    }

    return RET_SUCCESS;
    
}
*/

#include "lte_dev_test_lib.c"

