#include <linux/device.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtc.h>
#include <linux/kernel.h>
#include <mach/mt_gpio.h>
#include <cust_clk_buf.h>
#include <mach/mt_sec_export.h>

#if defined (CONFIG_MTK_AEE_FEATURE)
#include <linux/aee.h>
#else
#define DB_OPT_DEFAULT    (0) // Dummy macro define to avoid build error
#define DB_OPT_FTRACE   (0) // Dummy macro define to avoid build error
#endif
#include <mach/ccci_config.h>

#ifdef FEATURE_RF_CLK_BUF
#include <mach/mt_clkbuf_ctl.h>
#endif
#include "ccci_core.h"
#include "ccci_bm.h"
#include "ccci_platform.h"
#include "port_kernel.h"

#if defined (FEATURE_GET_MD_ADC_NUM)
extern int IMM_get_adc_channel_num(char *channel_name, int len);
#endif
#if defined (FEATURE_GET_MD_ADC_VAL)
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);
#endif

#if defined(FEATURE_GET_MD_PMIC_ADC_VAL) 
extern int PMIC_IMM_GetOneChannelValue(int dwChannel, int deCount, int trimd);
#endif

#if defined (FEATURE_GET_MD_PMIC_ADC_NUM)
extern int PMIC_IMM_get_adc_channel_num(char *adc_name, int len);
#endif

#if defined (FEATURE_GET_DRAM_TYPE_CLK)
extern int get_dram_info(int *clk, int *type);
#endif
#if defined (FEATURE_GET_MD_EINT_ATTR)
extern int get_eint_attribute(char *name, unsigned int name_len, unsigned int type, char * result, unsigned int *len);
#endif
static void ccci_ee_info_dump(struct ccci_modem *md);
static void ccci_aed(struct ccci_modem *md, unsigned int dump_flag, char *aed_str, int db_opt);
extern void ccci_set_dsp_region_protection(struct ccci_modem *md, int loaded);
#define MAX_QUEUE_LENGTH 16
#define EX_TIMER_SWINT 10
#define EX_TIMER_MD_EX 5
#define EX_TIMER_MD_EX_REC_OK 10
#define EX_TIMER_MD_HANG 5

/*
 * all supported modems should follow these handshake messages as a protocol.
 * but we still can support un-usual modem by providing cutomed kernel_port_ops.
 */
static void control_msg_handler(struct ccci_port *port, struct ccci_request *req)
{
    struct ccci_modem *md = port->modem;
    struct ccci_header *ccci_h = (struct ccci_header *)req->skb->data;
    unsigned long flags;
    char need_update_state = 0;
    
    CCCI_INF_MSG(md->index, KERN, "control message 0x%X,0x%X\n", ccci_h->data[1], ccci_h->reserved);
    if(ccci_h->data[1] == MD_INIT_START_BOOT 
            && ccci_h->reserved == MD_INIT_CHK_ID 
            && md->boot_stage == MD_BOOT_STAGE_0) {
        del_timer(&md->bootup_timer);
        md->boot_stage = MD_BOOT_STAGE_1;
        ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_BOOT_UP, 0);
    } else if(ccci_h->data[1] == MD_NORMAL_BOOT 
            && md->boot_stage == MD_BOOT_STAGE_1) {
        del_timer(&md->bootup_timer);
        md->boot_stage = MD_BOOT_STAGE_2;
        md->ops->broadcast_state(md, READY);
        ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_BOOT_READY, 0);
    } else if(ccci_h->data[1] == MD_EX) {
        if (unlikely(ccci_h->reserved != MD_EX_CHK_ID)) {
            CCCI_ERR_MSG(md->index, KERN, "receive invalid MD_EX\n");
        } else {    
            spin_lock_irqsave(&md->ctrl_lock, flags);
            //md->boot_stage = MD_BOOT_STAGE_EXCEPTION;
            md->ee_info_flag |= ((1<<MD_EE_FLOW_START)|(1<<MD_EE_MSG_GET)|(1<<MD_STATE_UPDATE)|\
                                        (1<<MD_EE_TIME_OUT_SET));
            md->config.setting |= MD_SETTING_RELOAD;
            spin_unlock_irqrestore(&md->ctrl_lock, flags);
            del_timer(&md->bootup_timer);
            if(!MD_IN_DEBUG(md))
                mod_timer(&md->ex_monitor, jiffies+EX_TIMER_MD_EX*HZ);
            md->ops->broadcast_state(md, EXCEPTION);
            ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_EXCEPTION, 0);
            ccci_send_msg_to_md(md, CCCI_CONTROL_TX, MD_EX, MD_EX_CHK_ID, 1);

			CCCI_INF_MSG(md->index, KERN, "Disable WDT at exception enter.");
			md->ops->ee_callback(md, EE_FLAG_DISABLE_WDT);
        }
    } else if(ccci_h->data[1] == MD_EX_REC_OK) {
        if (unlikely(ccci_h->reserved!=MD_EX_REC_OK_CHK_ID || req->skb->len<(sizeof(struct ccci_header)+sizeof(EX_LOG_T)))) {
            CCCI_ERR_MSG(md->index, KERN, "receive invalid MD_EX_REC_OK, resv=%x, len=%d\n", ccci_h->reserved, req->skb->len);
        } else {
            spin_lock_irqsave(&md->ctrl_lock, flags);
            md->ee_info_flag |= ((1<<MD_EE_FLOW_START)|(1<<MD_EE_OK_MSG_GET));
            if((md->ee_info_flag & (1<<MD_STATE_UPDATE)) == 0) {
                md->ee_info_flag |= (1<<MD_STATE_UPDATE);
                md->ee_info_flag &= ~(1<<MD_EE_TIME_OUT_SET);
                //md->boot_stage = MD_BOOT_STAGE_EXCEPTION;
                md->config.setting |= MD_SETTING_RELOAD;
                need_update_state = 1;
            }
            spin_unlock_irqrestore(&md->ctrl_lock, flags);
            if(need_update_state) {
                CCCI_ERR_MSG(md->index, KERN, "get MD_EX_REC_OK without exception MD_EX\n");
                del_timer(&md->bootup_timer);
                md->ops->broadcast_state(md, EXCEPTION);
            }
            // copy exception info
            memcpy(&md->ex_info, skb_pull(req->skb, sizeof(struct ccci_header)), sizeof(EX_LOG_T));
            mod_timer(&md->ex_monitor, jiffies);
        }
    }else if (ccci_h->data[1] == MD_EX_PASS) {
        // dump share memory again to let MD check exception flow
        mod_timer(&md->ex_monitor2, jiffies);
    }else if (ccci_h->data[1] == MD_INIT_START_BOOT 
                && ccci_h->reserved == MD_INIT_CHK_ID 
                && !(md->config.setting&MD_SETTING_FIRST_BOOT)) {
        md->boot_stage = MD_BOOT_STAGE_0;
        CCCI_ERR_MSG(md->index, KERN, "MD second bootup detected!\n");
        ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_RESET, 0);
    } else if (ccci_h->data[1] == MD_EX_RESUME) {
		memset(&md->debug_info, 0, sizeof(md->debug_info));
		md->debug_info.type = MD_EX_TYPE_EMI_CHECK;
		md->debug_info.name = "EMI_CHK";
		md->debug_info.data = *(ccci_msg_t *)ccci_h;
        md->ex_type = MD_EX_TYPE_EMI_CHECK;
		ccci_ee_info_dump(md);
    } else if (ccci_h->data[1] == CCCI_DRV_VER_ERROR) {
        CCCI_ERR_MSG(md->index, KERN, "AP CCCI driver version mis-match to MD!!\n");
        md->config.setting |= MD_SETTING_STOP_RETRY_BOOT;
        ccci_aed(md, 0, "AP/MD driver version mis-match\n", DB_OPT_DEFAULT);
    } else {
        CCCI_ERR_MSG(md->index, KERN, "receive unknow data from CCCI_CONTROL_RX = %d\n", ccci_h->data[1]);
    }
    req->policy = RECYCLE;
    ccci_free_req(req);
}

// for backward compatibility
ccci_sys_cb_func_info_t    ccci_sys_cb_table_100[MAX_MD_NUM][MAX_KERN_API];
ccci_sys_cb_func_info_t    ccci_sys_cb_table_1000[MAX_MD_NUM][MAX_KERN_API];
int register_ccci_sys_call_back(int md_id, unsigned int id, ccci_sys_cb_func_t func)
{
    int ret = 0;
    ccci_sys_cb_func_info_t *info_ptr;
    
    if( md_id >= MAX_MD_NUM ) {
        CCCI_ERR_MSG(md_id, KERN, "register_sys_call_back fail: invalid md id\n");
        return -EINVAL;
    }

    if((id >= 0x100)&&((id-0x100) < MAX_KERN_API)) {
        info_ptr = &(ccci_sys_cb_table_100[md_id][id-0x100]);
    } else if((id >= 0x1000)&&((id-0x1000) < MAX_KERN_API)) {
        info_ptr = &(ccci_sys_cb_table_1000[md_id][id-0x1000]);
    } else {
        CCCI_ERR_MSG(md_id, KERN, "register_sys_call_back fail: invalid func id(0x%x)\n", id);
        return -EINVAL;
    }
    
    if(info_ptr->func == NULL) {
        info_ptr->id = id;
        info_ptr->func = func;
    }
    else {
        CCCI_ERR_MSG(md_id, KERN, "register_sys_call_back fail: func(0x%x) registered!\n", id);
    }

    return ret;
}

void exec_ccci_sys_call_back(int md_id, int cb_id, int data)
{
    ccci_sys_cb_func_t func;
    int    id;
    ccci_sys_cb_func_info_t    *curr_table;
    
    if(md_id >= MAX_MD_NUM) {
        CCCI_ERR_MSG(md_id, KERN, "exec_sys_cb fail: invalid md id\n");
        return;
    }

    id = cb_id & 0xFF;
    if(id >= MAX_KERN_API) {
        CCCI_ERR_MSG(md_id, KERN, "exec_sys_cb fail: invalid func id(0x%x)\n", cb_id);
        return;
    }

    if ((cb_id & (0x1000|0x100))==0x1000) {
        curr_table = ccci_sys_cb_table_1000[md_id];
    } else if ((cb_id & (0x1000|0x100))==0x100) {
        curr_table = ccci_sys_cb_table_100[md_id];
    } else {
        CCCI_ERR_MSG(md_id, KERN, "exec_sys_cb fail: invalid func id(0x%x)\n", cb_id);
        return;
    }
    
    func = curr_table[id].func;
    if(func != NULL) {
        func(md_id, data);
    } else {
        CCCI_ERR_MSG(md_id, KERN, "exec_sys_cb fail: func id(0x%x) not register!\n", cb_id);
    }
}

static void system_msg_handler(struct ccci_port *port, struct ccci_request *req)
{
    struct ccci_modem *md = port->modem;
    struct ccci_header *ccci_h = (struct ccci_header *)req->skb->data;
    
    CCCI_DBG_MSG(md->index, KERN, "system message (%x %x %x %x)\n", ccci_h->data[0], ccci_h->data[1], ccci_h->channel, ccci_h->reserved);
    switch(ccci_h->data[1]) {
    case MD_GET_BATTERY_INFO:
        ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_SEND_BATTERY_INFO, 0);
        break;
    case MD_WDT_MONITOR:
        // abandoned
        break;
    case MD_SIM_TYPE:
        md->sim_type = ccci_h->reserved;
        break;
    case MD_TX_POWER:
    case MD_RF_TEMPERATURE:
    case MD_RF_TEMPERATURE_3G:
#if defined(CONFIG_MTK_SWITCH_TX_POWER)
    case MD_SW_MD1_TX_POWER_REQ:
    case MD_SW_MD2_TX_POWER_REQ:        
#endif                
        exec_ccci_sys_call_back(md->index, ccci_h->data[1], ccci_h->reserved);
        break;
    };
    req->policy = RECYCLE;
    ccci_free_req(req);
}

static int get_md_gpio_val(unsigned int num)
{
#if defined (FEATURE_GET_MD_GPIO_VAL)
    return mt_get_gpio_in(num);
#else
    return -1;
#endif
}

static int get_md_adc_val(unsigned int num)
{
#if defined (FEATURE_GET_MD_ADC_VAL)
    int data[4] = {0,0,0,0};
    int val = 0;
    int ret = 0;
    CCCI_INF_MSG(0, RPC, "FEATURE_GET_MD_ADC_VAL \n");
    ret = IMM_GetOneChannelValue(num, data, &val);
    if (ret == 0)
        return val;
    else
        return ret;
#elif defined(FEATURE_GET_MD_PMIC_ADC_VAL)
	CCCI_INF_MSG(0, RPC, "FEATURE_GET_MD_PMIC_ADC_VAL \n");
	return PMIC_IMM_GetOneChannelValue(num, 1, 0);
#else
    return -1;
#endif
}

static int get_td_eint_info(char *eint_name, unsigned int len)
{
#if defined (FEATURE_GET_TD_EINT_NUM)
    return get_td_eint_num(eint_name, len);
#else
    return -1;
#endif
}    

static int get_md_adc_info(char *adc_name, unsigned int len)
{
#if defined (FEATURE_GET_MD_ADC_NUM)
	CCCI_INF_MSG(0, RPC, "FEATURE_GET_MD_ADC_NUM \n");
	return IMM_get_adc_channel_num(adc_name, len);
#elif defined(FEATURE_GET_MD_PMIC_ADC_NUM)
	CCCI_INF_MSG(0, RPC, "FEATURE_GET_MD_PMIC_ADC_NUM \n");
	return PMIC_IMM_get_adc_channel_num(adc_name, len);
#else
    return -1;
#endif
}
    
static int get_md_gpio_info(char *gpio_name, unsigned int len)
{
#if defined (FEATURE_GET_MD_GPIO_NUM)
    return mt_get_md_gpio(gpio_name, len);
#else
    return -1;
#endif
}

static int get_dram_type_clk(int *clk, int *type)
{
#if defined (FEATURE_GET_DRAM_TYPE_CLK)
    return get_dram_info(clk, type);
#else
    return -1;
#endif
}

static int get_eint_attr(char *name, unsigned int name_len, unsigned int type, char *result, unsigned int *len)
{
#if defined (FEATURE_GET_MD_EINT_ATTR)
    return get_eint_attribute(name, name_len, type, result, len);
#else
    return -1;
#endif
}

static void ccci_rpc_work_helper(struct ccci_modem *md, struct rpc_pkt *pkt, 
    struct rpc_buffer *p_rpc_buf, unsigned int tmp_data[])
{
    // tmp_data[] is used to make sure memory address is valid after this function return, be careful with the size!
    int pkt_num = p_rpc_buf->para_num;

    CCCI_DBG_MSG(md->index, RPC, "ccci_rpc_work_helper++ %d\n", p_rpc_buf->para_num);
    tmp_data[0] = 0;
    switch(p_rpc_buf->op_id) {
    case IPC_RPC_CPSVC_SECURE_ALGO_OP:
    {
        unsigned char Direction = 0;
        unsigned long  ContentAddr = 0;
        unsigned int   ContentLen = 0;
        sed_t CustomSeed = SED_INITIALIZER;
        unsigned char *ResText __always_unused= NULL;
        unsigned char *RawText __always_unused= NULL;
        unsigned int i __always_unused= 0;

        if(pkt_num < 4 || pkt_num >= RPC_MAX_ARG_NUM) {
            CCCI_ERR_MSG(md->index, RPC, "invalid pkt_num %d for RPC_SECURE_ALGO_OP!\n", pkt_num);
            tmp_data[0] = FS_PARAM_ERROR;
            pkt_num = 0;
            pkt[pkt_num].len = sizeof(unsigned int);
            pkt[pkt_num++].buf = (void*) &tmp_data[0];
            break;
        }

        Direction = *(unsigned char*)pkt[0].buf;
        ContentAddr = (unsigned long)pkt[1].buf;
        CCCI_DBG_MSG(md->index, RPC, "RPC_SECURE_ALGO_OP: Content_Addr = 0x%p, RPC_Base = 0x%p, RPC_Len = 0x%zu\n", 
            (void*)ContentAddr, p_rpc_buf, sizeof(unsigned int) + RPC_MAX_BUF_SIZE);
        if(ContentAddr < (unsigned long)p_rpc_buf || 
            ContentAddr > ((unsigned long)p_rpc_buf + sizeof(unsigned int) + RPC_MAX_BUF_SIZE)) {
            CCCI_ERR_MSG(md->index, RPC, "invalid ContentAdddr[0x%p] for RPC_SECURE_ALGO_OP!\n", (void*)ContentAddr);
            tmp_data[0] = FS_PARAM_ERROR;
            pkt[pkt_num].len = sizeof(unsigned int);
            pkt[pkt_num++].buf = (void*) &tmp_data[0];
            break;
        }
        ContentLen = *(unsigned int*)pkt[2].buf;
        //    CustomSeed = *(sed_t*)pkt[3].buf;
        WARN_ON(sizeof(CustomSeed.sed)<pkt[3].len);
        memcpy(CustomSeed.sed,pkt[3].buf,pkt[3].len);

#ifdef ENCRYPT_DEBUG
        unsigned char log_buf[128];
        int curr;

        if(Direction == TRUE)
            CCCI_INF_MSG(md->index, RPC, "HACC_S: EnCrypt_src:\n");
        else
            CCCI_INF_MSG(md->index, RPC, "HACC_S: DeCrypt_src:\n");
        for(i = 0; i < ContentLen; i++) {
            if(i % 16 == 0) {
                if(i!=0) {
                    CCCI_INF_MSG(md->index, RPC, "%s\n", log_buf);
                }
                curr = 0;
                curr += snprintf(log_buf, sizeof(log_buf)-curr, "HACC_S: ");
            }
            //CCCI_INF_MSG(md->index, RPC, "0x%02X ", *(unsigned char*)(ContentAddr+i));
            curr += snprintf(&log_buf[curr], sizeof(log_buf)-curr, "0x%02X ", *(unsigned char*)(ContentAddr+i));                    
            //sleep(1);
        }
        CCCI_INF_MSG(md->index, RPC, "%s\n", log_buf);
            
        RawText = kmalloc(ContentLen, GFP_KERNEL);
        if(RawText == NULL)
            CCCI_ERR_MSG(md->index, RPC, "Fail alloc Mem for RPC_SECURE_ALGO_OP!\n");
        else
            memcpy(RawText, (unsigned char*)ContentAddr, ContentLen);
#endif

        ResText = kmalloc(ContentLen, GFP_KERNEL);
        if(ResText == NULL) {
            CCCI_ERR_MSG(md->index, RPC, "Fail alloc Mem for RPC_SECURE_ALGO_OP!\n");
            tmp_data[0] = FS_PARAM_ERROR;
            pkt[pkt_num].len = sizeof(unsigned int);
            pkt[pkt_num++].buf = (void*) &tmp_data[0];
            break;
        }

#if (defined(ENABLE_MD_IMG_SECURITY_FEATURE) && defined(MTK_SEC_MODEM_NVRAM_ANTI_CLONE))
        if(!masp_secure_algo_init()) {
            CCCI_ERR_MSG(md->index, RPC, "masp_secure_algo_init fail!\n");
            ASSERT(0);
        }
        
        CCCI_DBG_MSG(md->index, RPC, "RPC_SECURE_ALGO_OP: Dir=0x%08X, Addr=0x%08lX, Len=0x%08X, Seed=0x%016llX\n", 
                Direction, ContentAddr, ContentLen, *(long long *)CustomSeed.sed);
        masp_secure_algo(Direction, ContentAddr, ContentLen, CustomSeed.sed, ResText);

        if(!masp_secure_algo_deinit())
            CCCI_ERR_MSG(md->index, RPC, "masp_secure_algo_deinit fail!\n");
#endif

        pkt_num = 0;
        pkt[pkt_num].len = sizeof(unsigned int);
        pkt[pkt_num++].buf = (void*) &tmp_data[0];
        pkt[pkt_num].len = ContentLen;    
        
#if (defined(ENABLE_MD_IMG_SECURITY_FEATURE) && defined(MTK_SEC_MODEM_NVRAM_ANTI_CLONE))
        memcpy(pkt[pkt_num++].buf, ResText, ContentLen);
        CCCI_INF_MSG(md->index, RPC, "RPC_Secure memory copy OK: %d!", ContentLen);
#else
        memcpy(pkt[pkt_num++].buf, (void *)ContentAddr, ContentLen);
        CCCI_INF_MSG(md->index, RPC, "RPC_NORMAL memory copy OK: %d!", ContentLen);
#endif
        
#ifdef ENCRYPT_DEBUG
        if(Direction == TRUE)
            CCCI_INF_MSG(md->index, RPC, "HACC_D: EnCrypt_dst:\n");
        else
            CCCI_INF_MSG(md->index, RPC, "HACC_D: DeCrypt_dst:\n");
        for(i = 0; i < ContentLen; i++) {
            if(i % 16 == 0){
                if(i!=0){
                    CCCI_DBG_MSG(md->index, RPC, "%s\n", log_buf);
                }
                curr = 0;
                curr += snprintf(&log_buf[curr], sizeof(log_buf)-curr, "HACC_D: ");
            }
            //CCCI_INF_MSG(md->index, RPC, "%02X ", *(ResText+i));
            curr += snprintf(&log_buf[curr], sizeof(log_buf)-curr, "0x%02X ", *(ResText+i));
            //sleep(1);
        }
        
        CCCI_INF_MSG(md->index, RPC, "%s\n", log_buf);

        if(RawText)
            kfree(RawText);
#endif

        kfree(ResText);
        break;
    }

#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
    case IPC_RPC_GET_SECRO_OP:
    {
        unsigned char *addr = NULL;
        unsigned int img_len = 0;
        unsigned int img_len_bak = 0;
        unsigned int blk_sz = 0;
        unsigned int tmp = 1;
        unsigned int cnt = 0;
        unsigned int req_len = 0;    
    
        if(pkt_num != 1) {
            CCCI_ERR_MSG(md->index, RPC, "RPC_GET_SECRO_OP: invalid parameter: pkt_num=%d \n", pkt_num);
            tmp_data[0] = FS_PARAM_ERROR;
            pkt_num = 0;
            pkt[pkt_num].len = sizeof(unsigned int);
            pkt[pkt_num++].buf = (void*) &tmp_data[0];
            pkt[pkt_num].len = sizeof(unsigned int);
            tmp_data[1] = img_len;
            pkt[pkt_num++].buf = (void*) &tmp_data[1];
            break;
        }
            
        req_len = *(unsigned int*)(pkt[0].buf);
        if(masp_secro_en()) {
            img_len = masp_secro_md_len(md->post_fix);

            if((img_len > RPC_MAX_BUF_SIZE) || (req_len > RPC_MAX_BUF_SIZE)) {
                pkt_num = 0;
                tmp_data[0] = FS_MEM_OVERFLOW;
                pkt[pkt_num].len = sizeof(unsigned int);
                pkt[pkt_num++].buf = (void*) &tmp_data[0];
                //set it as image length for modem ccci check when error happens
                pkt[pkt_num].len = img_len;
                ///pkt[pkt_num].len = sizeof(unsigned int);
                tmp_data[1] = img_len;
                pkt[pkt_num++].buf = (void*) &tmp_data[1];
                CCCI_ERR_MSG(md->index, RPC, "RPC_GET_SECRO_OP: md request length is larger than rpc memory: (%d, %d) \n", 
                    req_len, img_len);
                break;
            }
            
            if(img_len > req_len) {
                pkt_num = 0;
                tmp_data[0] = FS_NO_MATCH;
                pkt[pkt_num].len = sizeof(unsigned int);
                pkt[pkt_num++].buf = (void*) &tmp_data[0];
                //set it as image length for modem ccci check when error happens
                pkt[pkt_num].len = img_len;
                ///pkt[pkt_num].len = sizeof(unsigned int);
                tmp_data[1] = img_len;
                pkt[pkt_num++].buf = (void*) &tmp_data[1];
                CCCI_ERR_MSG(md->index, RPC, "RPC_GET_SECRO_OP: AP mis-match MD request length: (%d, %d) \n", 
                    req_len, img_len);
                break;
            }

            /* TODO : please check it */
            /* save original modem secro length */
            CCCI_DBG_MSG(md->index, RPC, "<rpc>RPC_GET_SECRO_OP: save MD SECRO length: (%d) \n",img_len);
            img_len_bak = img_len;
   
            blk_sz = masp_secro_blk_sz();
            for(cnt = 0; cnt < blk_sz; cnt++) {
                tmp = tmp*2;
                if(tmp >= blk_sz)
                    break;
            }
            ++cnt;
            img_len = ((img_len + (blk_sz-1)) >> cnt) << cnt;

            addr = (unsigned char*)&(p_rpc_buf->para_num) + 4*sizeof(unsigned int);
            tmp_data[0] = masp_secro_md_get_data(md->post_fix, addr, 0, img_len);

            /* TODO : please check it */
            /* restore original modem secro length */
            img_len = img_len_bak;

            CCCI_DBG_MSG(md->index, RPC, "<rpc>RPC_GET_SECRO_OP: restore MD SECRO length: (%d) \n",img_len);             

            if(tmp_data[0] != 0) {
                CCCI_ERR_MSG(md->index, RPC, "RPC_GET_SECRO_OP: get data fail:%d \n", tmp_data[0]);
                pkt_num = 0;
                pkt[pkt_num].len = sizeof(unsigned int);
                pkt[pkt_num++].buf = (void*) &tmp_data[0];
                pkt[pkt_num].len = sizeof(unsigned int);
                tmp_data[1] = img_len;
                pkt[pkt_num++].buf = (void*) &tmp_data[1];
            } else {
                CCCI_DBG_MSG(md->index, RPC, "RPC_GET_SECRO_OP: get data OK: %d,%d \n", img_len, tmp_data[0]);
                pkt_num = 0;
                pkt[pkt_num].len = sizeof(unsigned int);
                //pkt[pkt_num++].buf = (void*) &img_len;
                tmp_data[1] = img_len;
                pkt[pkt_num++].buf = (void*)&tmp_data[1];
                pkt[pkt_num].len = img_len;
                pkt[pkt_num++].buf = (void*) addr;
                //tmp_data[2] = (unsigned int)addr;
                //pkt[pkt_num++].buf = (void*) &tmp_data[2];
            }
        }else {
            CCCI_INF_MSG(md->index, RPC,  "RPC_GET_SECRO_OP: secro disable \n");
            tmp_data[0] = FS_NO_FEATURE;
            pkt_num = 0;
            pkt[pkt_num].len = sizeof(unsigned int);
            pkt[pkt_num++].buf = (void*) &tmp_data[0];
            pkt[pkt_num].len = sizeof(unsigned int);
            tmp_data[1] = img_len;
            pkt[pkt_num++].buf = (void*) &tmp_data[1];    
        }

        break;
    }
#endif

    //call EINT API to get TDD EINT configuration for modem EINT initial
    case IPC_RPC_GET_TDD_EINT_NUM_OP:
    case IPC_RPC_GET_GPIO_NUM_OP:
    case IPC_RPC_GET_ADC_NUM_OP:
    {
        int get_num = 0;
        unsigned char * name = NULL;
        unsigned int length = 0;    

        if(pkt_num<2 || pkt_num>RPC_MAX_ARG_NUM)    {
            CCCI_ERR_MSG(md->index, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n", 
                                p_rpc_buf->op_id, pkt_num);
            tmp_data[0] = FS_PARAM_ERROR;
            goto err1;
        }

        if((length = pkt[0].len) < 1) {
            CCCI_ERR_MSG(md->index, RPC, "invalid parameter for [0x%X]: pkt_num=%d, name_len=%d!\n", 
                p_rpc_buf->op_id, pkt_num, length);
            tmp_data[0] = FS_PARAM_ERROR;
            goto err1;
        }

        name = kmalloc(length, GFP_KERNEL);
        if(name == NULL) {
            CCCI_ERR_MSG(md->index, RPC, "Fail alloc Mem for [0x%X]!\n", p_rpc_buf->op_id);
            tmp_data[0] = FS_ERROR_RESERVED;
            goto err1;
        } else {
            memcpy(name, (unsigned char*)(pkt[0].buf), length);

            if(p_rpc_buf->op_id == IPC_RPC_GET_TDD_EINT_NUM_OP) {
                if((get_num = get_td_eint_info(name, length)) < 0) {
                    get_num = FS_FUNC_FAIL;
                }
            } else if(p_rpc_buf->op_id == IPC_RPC_GET_GPIO_NUM_OP) {
                if((get_num = get_md_gpio_info(name, length)) < 0)    {
                    get_num = FS_FUNC_FAIL;
                }
            } else if(p_rpc_buf->op_id == IPC_RPC_GET_ADC_NUM_OP) {
                if((get_num = get_md_adc_info(name, length)) < 0)    {
                    get_num = FS_FUNC_FAIL;
                }
            }
    
            CCCI_INF_MSG(md->index, RPC, "[0x%08X]: name:%s, len=%d, get_num:%d\n",p_rpc_buf->op_id,
                name, length, get_num);    
            pkt_num = 0;

            /* NOTE: tmp_data[1] not [0] */
            tmp_data[1] = (unsigned int)get_num;    // get_num may be invalid after exit this function
            pkt[pkt_num].len = sizeof(unsigned int);
            pkt[pkt_num++].buf = (void*)(&tmp_data[1]);
            pkt[pkt_num].len = sizeof(unsigned int);
            pkt[pkt_num++].buf = (void*)(&tmp_data[1]);
            kfree(name);
        }
        break;

err1:
        pkt_num = 0;
        pkt[pkt_num].len = sizeof(unsigned int);
        pkt[pkt_num++].buf = (void*) &tmp_data[0];
        pkt[pkt_num].len = sizeof(unsigned int);
        pkt[pkt_num++].buf = (void*) &tmp_data[0];
        break;
    }

    case IPC_RPC_GET_EMI_CLK_TYPE_OP:
    {
        int dram_type = 0;
        int dram_clk = 0;
    
        if(pkt_num != 0) {
            CCCI_ERR_MSG(md->index, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n", 
                                p_rpc_buf->op_id, pkt_num);
            tmp_data[0] = FS_PARAM_ERROR;
            goto err2;
        }

        if(get_dram_type_clk(&dram_clk, &dram_type)) {
            tmp_data[0] = FS_FUNC_FAIL;
            goto err2;
        } else {
            tmp_data[0] = 0;
            CCCI_INF_MSG(md->index, RPC, "[0x%08X]: dram_clk: %d, dram_type:%d \n",
                p_rpc_buf->op_id, dram_clk, dram_type);    
        }
    
        tmp_data[1] = (unsigned int)dram_type;
        tmp_data[2] = (unsigned int)dram_clk;
        
        pkt_num = 0;
        pkt[pkt_num].len = sizeof(unsigned int);
        pkt[pkt_num++].buf = (void*)(&tmp_data[0]);    
        pkt[pkt_num].len = sizeof(unsigned int);
        pkt[pkt_num++].buf = (void*)(&tmp_data[1]);    
        pkt[pkt_num].len = sizeof(unsigned int);
        pkt[pkt_num++].buf = (void*)(&tmp_data[2]);    
        break;
        
err2:
        pkt_num = 0;
        pkt[pkt_num].len = sizeof(unsigned int);
        pkt[pkt_num++].buf = (void*) &tmp_data[0];
        pkt[pkt_num].len = sizeof(unsigned int);
        pkt[pkt_num++].buf = (void*) &tmp_data[0];
        pkt[pkt_num].len = sizeof(unsigned int);
        pkt[pkt_num++].buf = (void*) &tmp_data[0];
        break;
    }
        
    case IPC_RPC_GET_EINT_ATTR_OP:    
    {
        char * eint_name = NULL;
        unsigned int name_len = 0;
        unsigned int type = 0;
        char * res = NULL;
        unsigned int res_len = 0;
        int ret = 0;
        
        if(pkt_num<3 || pkt_num>RPC_MAX_ARG_NUM)    {
            CCCI_ERR_MSG(md->index, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
                p_rpc_buf->op_id, pkt_num);
            tmp_data[0] = FS_PARAM_ERROR;
            goto err3;
        }
        
        if((name_len = pkt[0].len) < 1) {
            CCCI_ERR_MSG(md->index, RPC, "invalid parameter for [0x%X]: pkt_num=%d, name_len=%d!\n",
                p_rpc_buf->op_id, pkt_num, name_len);
            tmp_data[0] = FS_PARAM_ERROR;
            goto err3;
        }
        
        eint_name = kmalloc(name_len, GFP_KERNEL);
        if(eint_name == NULL) {
            CCCI_ERR_MSG(md->index, RPC, "Fail alloc Mem for [0x%X]!\n", p_rpc_buf->op_id);
            tmp_data[0] = FS_ERROR_RESERVED;
            goto err3;
        } else {
            memcpy(eint_name, (unsigned char*)(pkt[0].buf), name_len);
        }
        
        type = *(unsigned int*)(pkt[2].buf);
        res = (unsigned char*)&(p_rpc_buf->para_num) + 4*sizeof(unsigned int);
        ret = get_eint_attr(eint_name, name_len, type, res, &res_len);
        if (ret == 0) {
            tmp_data[0] = ret;
            pkt_num = 0;
            pkt[pkt_num].len = sizeof(unsigned int);
            pkt[pkt_num++].buf = (void*) &tmp_data[0];
            pkt[pkt_num].len = res_len;
            pkt[pkt_num++].buf = (void*) res;
            CCCI_INF_MSG(md->index, RPC, "[0x%08X] OK: name:%s, len:%d, type:%d, res:%d, res_len:%d\n",
                p_rpc_buf->op_id, eint_name, name_len, type, *res, res_len);
            kfree(eint_name);
        } else {
            tmp_data[0] = ret;
            CCCI_ERR_MSG(md->index, RPC, "[0x%08X] fail: name:%s, len:%d, type:%d, ret:%d\n", p_rpc_buf->op_id,
                eint_name, name_len, type, ret);
            kfree(eint_name);
            goto err3;
        }
        break;
        
err3:
        pkt_num = 0;
        pkt[pkt_num].len = sizeof(unsigned int);
        pkt[pkt_num++].buf = (void*) &tmp_data[0];
        pkt[pkt_num].len = sizeof(unsigned int);
        pkt[pkt_num++].buf = (void*) &tmp_data[0];
        break;
    }
#ifdef FEATURE_RF_CLK_BUF
    case IPC_RPC_GET_RF_CLK_BUF_OP:
    {
        u16 count = 0;
        struct ccci_rpc_clkbuf_result *clkbuf;
        CLK_BUF_SWCTRL_STATUS_T swctrl_status[CLKBUF_MAX_COUNT];
        
        if(pkt_num != 1)    {
            CCCI_ERR_MSG(md->index, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
                p_rpc_buf->op_id, pkt_num);
            tmp_data[0] = FS_PARAM_ERROR;
            pkt_num = 0;
            pkt[pkt_num].len = sizeof(unsigned int);
            pkt[pkt_num++].buf = (void*) &tmp_data[0];
            pkt[pkt_num].len = sizeof(unsigned int);
            pkt[pkt_num++].buf = (void*) &tmp_data[0];
            break;
        }
        count = *(u16*)(pkt[0].buf);
        pkt_num = 0;
        tmp_data[0] = 0;
        pkt[pkt_num].len = sizeof(unsigned int);
        pkt[pkt_num++].buf = (void*) &tmp_data[0];
        pkt[pkt_num].len = sizeof(struct ccci_rpc_clkbuf_result);
        pkt[pkt_num++].buf = (void*) &tmp_data[1];
        clkbuf = (struct ccci_rpc_clkbuf_result*) &tmp_data[1];
        if(count != CLKBUF_MAX_COUNT) {
            CCCI_ERR_MSG(md->index, RPC, "IPC_RPC_GET_RF_CLK_BUF, wrong count %d/%d\n", count, CLKBUF_MAX_COUNT);
            clkbuf->CLKBuf_Count = 0xFF;
            memset(&clkbuf->CLKBuf_Status, 0, sizeof(clkbuf->CLKBuf_Status));
        } else {
            clkbuf->CLKBuf_Count = CLKBUF_MAX_COUNT;
            clkbuf->CLKBuf_Status[0] = CLK_BUF1_STATUS;
            clkbuf->CLKBuf_Status[1] = CLK_BUF2_STATUS;
            clkbuf->CLKBuf_Status[2] = CLK_BUF3_STATUS;
            clkbuf->CLKBuf_Status[3] = CLK_BUF4_STATUS;
            clk_buf_get_swctrl_status(swctrl_status);
            clkbuf->CLKBuf_SWCtrl_Status[0] = swctrl_status[0];
            clkbuf->CLKBuf_SWCtrl_Status[1] = swctrl_status[1];
            clkbuf->CLKBuf_SWCtrl_Status[2] = swctrl_status[2];
            clkbuf->CLKBuf_SWCtrl_Status[3] = swctrl_status[3];
        }
        CCCI_INF_MSG(md->index, KERN, "IPC_RPC_GET_RF_CLK_BUF count=%x\n", clkbuf->CLKBuf_Count);
        break;
    }
#endif  
    case IPC_RPC_GET_GPIO_VAL_OP:
    case IPC_RPC_GET_ADC_VAL_OP:
    {
        unsigned int num = 0;
        int val = 0;
        
        if(pkt_num != 1)    {
            CCCI_ERR_MSG(md->index, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
                p_rpc_buf->op_id, pkt_num);
            tmp_data[0] = FS_PARAM_ERROR;
            goto err4;
        }

        num = *(unsigned int*)(pkt[0].buf);
        if(p_rpc_buf->op_id == IPC_RPC_GET_GPIO_VAL_OP) {
            val = get_md_gpio_val(num);
        } else if (p_rpc_buf->op_id == IPC_RPC_GET_ADC_VAL_OP) {
            val = get_md_adc_val(num);
        }
        tmp_data[0] = val;
        CCCI_INF_MSG(md->index, RPC, "[0x%X]: num=%d, val=%d!\n", p_rpc_buf->op_id, num, val);

err4:
        pkt_num = 0;
        pkt[pkt_num].len = sizeof(unsigned int);
        pkt[pkt_num++].buf = (void*) &tmp_data[0];
        pkt[pkt_num].len = sizeof(unsigned int);
        pkt[pkt_num++].buf = (void*) &tmp_data[0];
        break;
    }
    
    case IPC_RPC_GET_GPIO_ADC_OP:
    {
        int num;
        unsigned int val, i;
        struct ccci_rpc_gpio_adc_intput *input;
        struct ccci_rpc_gpio_adc_output *output;
        
        if(pkt_num != 1)    {
            CCCI_ERR_MSG(md->index, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
                p_rpc_buf->op_id, pkt_num);
            tmp_data[0] = FS_PARAM_ERROR;
            pkt_num = 0;
            pkt[pkt_num].len = sizeof(unsigned int);
            pkt[pkt_num++].buf = (void*) &tmp_data[0];
            pkt[pkt_num].len = sizeof(unsigned int);
            pkt[pkt_num++].buf = (void*) &tmp_data[0];
            break;
        }
        input = (struct ccci_rpc_gpio_adc_intput *)(pkt[0].buf);
        pkt_num = 0;
        tmp_data[0] = 0;
        pkt[pkt_num].len = sizeof(unsigned int);
        pkt[pkt_num++].buf = (void*) &tmp_data[0];
        pkt[pkt_num].len = sizeof(struct ccci_rpc_gpio_adc_output);
        pkt[pkt_num++].buf = (void*) &tmp_data[1];
        output = (struct ccci_rpc_gpio_adc_output*) &tmp_data[1];
        memset(output, 0xF, sizeof(struct ccci_rpc_gpio_adc_output)); // 0xF for failure
        CCCI_INF_MSG(md->index, KERN, "IPC_RPC_GET_GPIO_ADC_OP request=%x\n", input->reqMask);
        if((input->reqMask&(RPC_REQ_GPIO_PIN|RPC_REQ_GPIO_VALUE)) == (RPC_REQ_GPIO_PIN|RPC_REQ_GPIO_VALUE)) {
            for(i=0; i<GPIO_MAX_COUNT; i++) {
                if(input->gpioValidPinMask & (1<<i)) {
                    if((num = get_md_gpio_info(input->gpioPinName[i], strlen(input->gpioPinName[i]))) >= 0)    {
                        output->gpioPinNum[i] = num;
                        val = get_md_gpio_val(num);
                        output->gpioPinValue[i] = val;
                    }
                }
            }
        } else {        
            if(input->reqMask & RPC_REQ_GPIO_PIN) {
                for(i=0; i<GPIO_MAX_COUNT; i++) {
                    if(input->gpioValidPinMask & (1<<i)) {
                        if((num = get_md_gpio_info(input->gpioPinName[i], strlen(input->gpioPinName[i]))) >= 0)    {
                            output->gpioPinNum[i] = num;
                        }
                    }
                }
            }
            if(input->reqMask & RPC_REQ_GPIO_VALUE) {
                for(i=0; i<GPIO_MAX_COUNT; i++) {
                    if(input->gpioValidPinMask & (1<<i)) {
                        val = get_md_gpio_val(input->gpioPinNum[i]);
                        output->gpioPinValue[i] = val;
                    }
                }
            }
        }
        if((input->reqMask&(RPC_REQ_ADC_PIN|RPC_REQ_ADC_VALUE)) == (RPC_REQ_ADC_PIN|RPC_REQ_ADC_VALUE)) {
            if((num = get_md_adc_info(input->adcChName, strlen(input->adcChName))) >= 0)    {
                output->adcChNum = num;
                output->adcChMeasSum = 0;
                for(i=0; i<input->adcChMeasCount; i++) {
                    val = get_md_adc_val(num);
                    output->adcChMeasSum += val;
                }
            }
        } else {
            if(input->reqMask & RPC_REQ_ADC_PIN) {
                if((num = get_md_adc_info(input->adcChName, strlen(input->adcChName))) >= 0)    {
                    output->adcChNum = num;
                }
            }
            if(input->reqMask & RPC_REQ_ADC_VALUE) {
                output->adcChMeasSum = 0;
                for(i=0; i<input->adcChMeasCount; i++) {
                    val = get_md_adc_val(input->adcChNum);
                    output->adcChMeasSum += val;
                }
            }
        }
        break;
    }

	case IPC_RPC_DSP_EMI_MPU_SETTING:
	{
		struct ccci_rpc_dsp_emi_mpu_input *input, *output;
		
		if(pkt_num != 1)	{
			CCCI_ERR_MSG(md->index, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
				p_rpc_buf->op_id, pkt_num);
			tmp_data[0] = FS_PARAM_ERROR;
			pkt_num = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void*) &tmp_data[0];
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void*) &tmp_data[0];
			break;
		}
		input = (struct ccci_rpc_dsp_emi_mpu_input *)(pkt[0].buf);
		pkt_num = 0;
		tmp_data[0] = 0;
		pkt[pkt_num].len = sizeof(unsigned int);
		pkt[pkt_num++].buf = (void*) &tmp_data[0];
		pkt[pkt_num].len = sizeof(struct ccci_rpc_dsp_emi_mpu_input);
		pkt[pkt_num++].buf = (void*) &tmp_data[1];
		output = (struct ccci_rpc_dsp_emi_mpu_input *) &tmp_data[1];
		output->request = 0;
		CCCI_INF_MSG(md->index, KERN, "IPC_RPC_DSP_EMI_MPU_SETTING request=%x\n", input->request);
		if(md->mem_layout.dsp_region_phy != 0)
			ccci_set_dsp_region_protection(md, 1);
		break;
	}

    case IPC_RPC_IT_OP:
    {
        int i;
        CCCI_INF_MSG(md->index, RPC, "[RPCIT] enter IT operation in ccci_rpc_work\n");
        //exam input parameters in pkt
        for(i=0; i<pkt_num ; i++){
            CCCI_INF_MSG(md->index, RPC, "len=%d val=%X\n", pkt[i].len, *((unsigned int *)pkt[i].buf));
        }
        tmp_data[0] = 1;
        tmp_data[1] = 0xA5A5;
        pkt_num = 0;
        CCCI_INF_MSG(md->index, RPC,"[RPCIT] prepare output parameters\n");
        pkt[pkt_num].len = sizeof(unsigned int);
        pkt[pkt_num++].buf = (void*) &tmp_data[0];
        CCCI_INF_MSG(md->index, RPC,"[RPCIT] LV[%d]  len= 0x%08X, value= 0x%08X\n", 0, pkt[0].len, *((unsigned int *)pkt[0].buf));
        pkt[pkt_num].len = sizeof(unsigned int);
        pkt[pkt_num++].buf = (void*) &tmp_data[1];
        CCCI_INF_MSG(md->index, RPC,"[RPCIT] LV[%d]  len= 0x%08X, value= 0x%08X\n", 1, pkt[1].len, *((unsigned int *)pkt[1].buf));
        break;                
    }

    default:
        CCCI_INF_MSG(md->index, RPC, "[Error]Unknown Operation ID (0x%08X)\n", p_rpc_buf->op_id);            
        tmp_data[0] = FS_NO_OP;
        pkt_num = 0;
        pkt[pkt_num].len = sizeof(int);
        pkt[pkt_num++].buf = (void*) &tmp_data[0];
        break;
    }
    
    p_rpc_buf->para_num = pkt_num;
    CCCI_DBG_MSG(md->index, RPC, "ccci_rpc_work_helper-- %d\n", p_rpc_buf->para_num);
}


static void rpc_msg_handler(struct ccci_port *port, struct ccci_request *req)
{
    struct ccci_modem *md = port->modem;
    struct rpc_buffer *rpc_buf = (struct rpc_buffer *)req->skb->data;
    int i, data_len=0, AlignLength, ret;
    struct rpc_pkt pkt[RPC_MAX_ARG_NUM];
    char *ptr, *ptr_base;
    unsigned int tmp_data[64]; // size of tmp_data should be >= any RPC output result

    // sanity check
    if(rpc_buf->header.reserved<0 || rpc_buf->header.reserved>RPC_REQ_BUFFER_NUM ||
        rpc_buf->para_num<0 || rpc_buf->para_num>RPC_MAX_ARG_NUM) {
        CCCI_ERR_MSG(md->index, RPC, "invalid RPC index %d/%d\n", rpc_buf->header.reserved, rpc_buf->para_num);
        goto err_out;
    }
    // parse buffer
    ptr_base = ptr = rpc_buf->buffer;
    for(i=0; i<rpc_buf->para_num; i++) {
        pkt[i].len = *((unsigned int*)ptr);
        ptr += sizeof(pkt[i].len);
        pkt[i].buf = ptr;
        ptr += ((pkt[i].len+3)>>2)<<2; // 4byte align
    }
    if((ptr-ptr_base) > RPC_MAX_BUF_SIZE) {
        CCCI_ERR_MSG(md->index, RPC, "RPC overflow in parse 0x%p\n", (void*)(ptr-ptr_base));
        goto err_out;
    }
    // handle RPC request
    ccci_rpc_work_helper(md, pkt, rpc_buf, tmp_data);
    // write back to modem
    // update message
    rpc_buf->op_id |= RPC_API_RESP_ID;
    data_len += (sizeof(rpc_buf->op_id) + sizeof(rpc_buf->para_num));
    ptr = rpc_buf->buffer;
    for(i=0; i<rpc_buf->para_num; i++) {
        if((data_len + sizeof(pkt[i].len) + pkt[i].len) > RPC_MAX_BUF_SIZE) {
            CCCI_ERR_MSG(md->index, RPC, "RPC overflow in write %zu\n", data_len + sizeof(pkt[i].len)+ pkt[i].len);
            goto err_out;
        }

        *((unsigned int*)ptr) = pkt[i].len;
        ptr += sizeof(pkt[i].len);
        data_len += sizeof(pkt[i].len);

        AlignLength = ((pkt[i].len+3)>>2)<<2; // 4byte aligned
        data_len += AlignLength;

        if(ptr != pkt[i].buf)
            memcpy(ptr, pkt[i].buf, pkt[i].len);
        else
            CCCI_DBG_MSG(md->index, RPC, "same addr, no copy, op_id=0x%x\n", rpc_buf->op_id);

        ptr += AlignLength;
    }
    // resize skb
    data_len += sizeof(struct ccci_header);
    if(data_len > req->skb->len)
        skb_put(req->skb, data_len - req->skb->len);
    else if(data_len < req->skb->len)
        skb_trim(req->skb, data_len);
    // update CCCI header
    rpc_buf->header.channel = CCCI_RPC_TX;
    rpc_buf->header.data[1] = data_len;
    CCCI_DBG_MSG(md->index, RPC, "Write %d/%d, %08X, %08X, %08X, %08X, op_id=0x%x\n", req->skb->len, data_len,
            rpc_buf->header.data[0], rpc_buf->header.data[1], rpc_buf->header.channel, rpc_buf->header.reserved, rpc_buf->op_id);
    // switch to Tx request
    req->policy = RECYCLE;
    req->blocking = 1;
    ret = ccci_port_send_request(port, req);
    if(ret)
        goto err_out;
    return;
    
err_out:
    req->policy = RECYCLE;
    ccci_free_req(req);
    return;
}
int process_rpc_kernel_msg(struct ccci_port *port, struct ccci_request *req)
{
	struct ccci_modem *md = port->modem;
  struct rpc_buffer *rpc_buf = (struct rpc_buffer *)req->skb->data;
	switch(rpc_buf->op_id){
#ifdef CONFIG_MTK_TC1_FEATURE
		// LGE specific OP ID
		case RPC_CCCI_LGE_FAC_READ_SIM_LOCK_TYPE:
		case RPC_CCCI_LGE_FAC_READ_FUSG_FLAG:
		case RPC_CCCI_LGE_FAC_CHECK_UNLOCK_CODE_VALIDNESS:
		case RPC_CCCI_LGE_FAC_CHECK_NETWORK_CODE_VALIDNESS:
		case RPC_CCCI_LGE_FAC_WRITE_SIM_LOCK_TYPE:
		case RPC_CCCI_LGE_FAC_READ_IMEI:
		case RPC_CCCI_LGE_FAC_WRITE_IMEI:
		case RPC_CCCI_LGE_FAC_READ_NETWORK_CODE_LIST_NUM:
		case RPC_CCCI_LGE_FAC_READ_NETWORK_CODE:
		case RPC_CCCI_LGE_FAC_WRITE_NETWORK_CODE_LIST_NUM:
		case RPC_CCCI_LGE_FAC_WRITE_UNLOCK_CODE_VERIFY_FAIL_COUNT:
		case RPC_CCCI_LGE_FAC_READ_UNLOCK_CODE_VERIFY_FAIL_COUNT:
		case RPC_CCCI_LGE_FAC_WRITE_UNLOCK_FAIL_COUNT:
		case RPC_CCCI_LGE_FAC_READ_UNLOCK_FAIL_COUNT:
		case RPC_CCCI_LGE_FAC_WRITE_UNLOCK_CODE:
		case RPC_CCCI_LGE_FAC_VERIFY_UNLOCK_CODE:
		case RPC_CCCI_LGE_FAC_WRITE_NETWORK_CODE:
		case RPC_CCCI_LGE_FAC_INIT_SIM_LOCK_DATA:
			CCCI_INF_MSG(port->modem->index, KERN, "userspace rpc msg 0x%x\n", rpc_buf->op_id);
			return 0;
#endif
		default:
			CCCI_INF_MSG(port->modem->index, KERN, "kernelspace rpc msg 0x%x\n", rpc_buf->op_id);
			list_del(&req->entry); 
			rpc_msg_handler(port,req);
			CCCI_INF_MSG(port->modem->index, KERN, "kernelspace rpc msg done\n");
			return 1;
	}	
}
static void status_msg_handler(struct ccci_port *port, struct ccci_request *req)
{
    struct ccci_modem *md = port->modem;
	struct ccci_header *ccci_h = (struct ccci_header *)req->skb->data;
	
    del_timer(&port->modem->md_status_timeout);
	CCCI_INF_MSG(port->modem->index, KERN, "modem status info seq=0x%X\n", *(((u32 *)ccci_h)+2));
	ccci_cmpt_mem_dump(md->index, req->skb->data, req->skb->len);
    req->policy = RECYCLE;
    ccci_free_req(req);
    if(port->modem->md_state == READY)
		mod_timer(&port->modem->md_status_poller, jiffies+15*HZ);
}

void md_status_poller_func(unsigned long data)
{
    struct ccci_modem *md = (struct ccci_modem *)data;
    int ret;
    
    mod_timer(&md->md_status_timeout, jiffies+5*HZ);
    ret = ccci_send_msg_to_md(md, CCCI_STATUS_TX, 0, 0, 0);
	CCCI_INF_MSG(md->index, KERN, "poll modem status %d seq=0x%X\n", ret, md->seq_nums[OUT][CCCI_STATUS_TX]);

    if(ret) {
        CCCI_ERR_MSG(md->index, KERN, "fail to send modem status polling msg ret=%d\n", ret);
        del_timer(&md->md_status_timeout);
		if(ret==-EBUSY && md->md_state==READY) {
			if(md->md_status_poller_flag & MD_STATUS_POLL_BUSY) {
				md->ops->dump_info(md, DUMP_FLAG_CLDMA, NULL, 0);
				ccci_md_exception_notify(md, MD_NO_RESPONSE);
			} else {
				md->md_status_poller_flag |= MD_STATUS_POLL_BUSY;
            mod_timer(&md->md_status_poller, jiffies+10*HZ);
    }
}
	} else {
		md->md_status_poller_flag &= ~MD_STATUS_POLL_BUSY;
	}
}

void md_status_timeout_func(unsigned long data)
{
    struct ccci_modem *md = (struct ccci_modem *)data;
    
	if(md->md_status_poller_flag & MD_STATUS_ASSERTED) {
		CCCI_ERR_MSG(md->index, KERN, "modem status polling timeout, assert fail\n");
		md->ops->dump_info(md, DUMP_FLAG_CLDMA, NULL, 0);
		ccci_md_exception_notify(md, MD_NO_RESPONSE);
	} else {
		CCCI_ERR_MSG(md->index, KERN, "modem status polling timeout, force assert\n");
		md->md_status_poller_flag |= MD_STATUS_ASSERTED;
		mod_timer(&md->md_status_timeout, jiffies+5*HZ);
    md->ops->force_assert(md, CCIF_INTR_SEQ);
}
}

static int port_kernel_thread(void *arg)
{
    struct ccci_port *port = arg;
	//struct sched_param param = { .sched_priority = 1 };
    struct ccci_request *req;
    struct ccci_header *ccci_h;
    unsigned long flags;
    int ret;

    CCCI_DBG_MSG(port->modem->index, KERN, "port %s's thread runnning\n", port->name);
	//sched_setscheduler(current, SCHED_FIFO, &param);

    while(1) {
        if(list_empty(&port->rx_req_list)) {
            ret = wait_event_interruptible(port->rx_wq, !list_empty(&port->rx_req_list));    
            if(ret == -ERESTARTSYS) {
                continue; // FIXME
            }
        }
        if (kthread_should_stop()){
                break ;
        }
        CCCI_DBG_MSG(port->modem->index, KERN, "read on %s\n", port->name);
        // 1. dequeue
        spin_lock_irqsave(&port->rx_req_lock, flags);
        req = list_first_entry(&port->rx_req_list, struct ccci_request, entry);
        list_del(&req->entry);
        if(--(port->rx_length) == 0)
            ccci_port_ask_more_request(port);
        spin_unlock_irqrestore(&port->rx_req_lock, flags);
        // 2. process the request
        ccci_h = (struct ccci_header *)req->skb->data;
        switch(ccci_h->channel){ // for a certain thread, only one kind of message is handled
        case CCCI_CONTROL_RX:
            control_msg_handler(port, req);
            break;
        case CCCI_SYSTEM_RX:
            system_msg_handler(port, req);
            break;
        case CCCI_STATUS_RX:
            status_msg_handler(port, req);
            break;
        };
        // ATTENTION, message handler will free request, do NOT reference request any more
    }
    return 0;
}

static int port_kernel_init(struct ccci_port *port)
{
    CCCI_DBG_MSG(port->modem->index, KERN, "kernel port %s is initializing\n", port->name);
    port->private_data = kthread_run(port_kernel_thread, port, "%s", port->name);
    port->rx_length_th = MAX_QUEUE_LENGTH;
    return 0;
}

static int port_kernel_recv_req(struct ccci_port *port, struct ccci_request *req)
{
    unsigned long flags;
    
    spin_lock_irqsave(&port->rx_req_lock, flags);
    CCCI_DBG_MSG(port->modem->index, IPC, "recv on %s, len=%d\n", port->name, port->rx_length);
    if(port->rx_length < port->rx_length_th) {
        port->flags &= ~PORT_F_RX_FULLED;
        port->rx_length++;
        list_del(&req->entry); // dequeue from queue's list
        list_add_tail(&req->entry, &port->rx_req_list);
        spin_unlock_irqrestore(&port->rx_req_lock, flags);
        wake_lock_timeout(&port->rx_wakelock, HZ);
        wake_up_all(&port->rx_wq);
        return 0;
    } else {
        port->flags |= PORT_F_RX_FULLED;
        spin_unlock_irqrestore(&port->rx_req_lock, flags);
        if(port->flags & PORT_F_ALLOW_DROP/* || !(port->flags&PORT_F_RX_EXCLUSIVE)*/) {
            CCCI_INF_MSG(port->modem->index, IPC, "port %s Rx full, drop packet\n", port->name);
            goto drop;
        } else {
            return -CCCI_ERR_PORT_RX_FULL;
        }
    }

drop:
    // drop this packet
    CCCI_INF_MSG(port->modem->index, IPC, "drop on %s, len=%d\n", port->name, port->rx_length);
    list_del(&req->entry); 
    req->policy = RECYCLE;
    ccci_free_req(req);
    return -CCCI_ERR_DROP_PACKET;
}

static void port_kernel_md_state_notice(struct ccci_port *port, MD_STATE state)
{
    if(port->rx_ch != CCCI_CONTROL_RX)
        return;
    
    // only for thoes states which are updated by modem driver
    switch(state) {
    case RESET:
        del_timer(&port->modem->md_status_poller);
        del_timer(&port->modem->md_status_timeout);

		del_timer(&port->modem->ex_monitor);
		del_timer(&port->modem->ex_monitor2);

		port->modem->md_status_poller_flag = 0;
        break;
    default:
        break;
    };
}

struct ccci_port_ops kernel_port_ops = {
    .init = &port_kernel_init,
    .recv_request = &port_kernel_recv_req,
    .md_state_notice = &port_kernel_md_state_notice,
};

void ccci_md_exception_notify(struct ccci_modem *md, MD_EX_STAGE stage)
{
    unsigned long flags;

    spin_lock_irqsave(&md->ctrl_lock, flags);
    CCCI_INF_MSG(md->index, KERN, "MD exception logical %d->%d\n", md->ex_stage, stage);
    md->ex_stage = stage;
    switch(md->ex_stage) {
    case EX_INIT:
        del_timer(&md->ex_monitor2);
        del_timer(&md->bootup_timer);
        del_timer(&md->md_status_poller);
        del_timer(&md->md_status_timeout);
        md->ee_info_flag |= ((1<<MD_EE_FLOW_START)|(1<<MD_EE_SWINT_GET));
        if(!MD_IN_DEBUG(md))
            mod_timer(&md->ex_monitor,jiffies+EX_TIMER_SWINT*HZ);
        md->ops->broadcast_state(md, EXCEPTION);
        break;
    case EX_DHL_DL_RDY:
        break;
    case EX_INIT_DONE:
        ccci_reset_seq_num(md);
        break;
    case MD_NO_RESPONSE:
        // don't broadcast exception state, only dump
		del_timer(&md->md_status_timeout);
		CCCI_ERR_MSG(md->index, KERN, "MD long time no response, flag=%x\n", md->md_status_poller_flag);
        md->ee_info_flag |= ((1<<MD_EE_FLOW_START)|(1<<MD_EE_PENDING_TOO_LONG)|(1<<MD_STATE_UPDATE));
        mod_timer(&md->ex_monitor,jiffies);
        break;
    case MD_WDT:
		del_timer(&md->md_status_poller);
		del_timer(&md->md_status_timeout);

        md->ee_info_flag |= ((1<<MD_EE_FLOW_START)|(1<<MD_EE_WDT_GET)|(1<<MD_STATE_UPDATE));
        mod_timer(&md->ex_monitor,jiffies);
        break;
    default:
        break;
    };
    spin_unlock_irqrestore(&md->ctrl_lock, flags);
}
EXPORT_SYMBOL(ccci_md_exception_notify);

static void ccci_aed(struct ccci_modem *md, unsigned int dump_flag, char *aed_str, int db_opt)
{
    void *ex_log_addr = NULL;
    int ex_log_len = 0;
    void *md_img_addr = NULL;
    int md_img_len = 0;
    int info_str_len = 0;
    char buff[AED_STR_LEN];
    char *img_inf;

    img_inf = ccci_get_md_info_str(md->index);
    if(img_inf == NULL)
        img_inf = "";
    info_str_len = strlen(aed_str);
    info_str_len += strlen(img_inf);

    if(info_str_len > AED_STR_LEN){
        buff[AED_STR_LEN-1] = '\0'; // Cut string length to AED_STR_LEN
    }

	snprintf(buff, AED_STR_LEN, "md%d:%s%s", md->index+1, aed_str, img_inf); // MD ID must sync with aee_dump_ccci_debug_info()

    if(dump_flag & CCCI_AED_DUMP_CCIF_REG) { // check this first, as we overwrite share memory here
		ex_log_addr = md->smem_layout.ccci_exp_smem_mdss_debug_vir;
		ex_log_len = md->smem_layout.ccci_exp_smem_mdss_debug_size;
		md->ops->dump_info(md, DUMP_FLAG_CCIF, md->smem_layout.ccci_exp_smem_base_vir+CCCI_SMEM_OFFSET_CCIF_SRAM, CCCC_SMEM_CCIF_SRAM_SIZE);
    }
    if(dump_flag & CCCI_AED_DUMP_EX_MEM) {
		ex_log_addr = md->smem_layout.ccci_exp_smem_mdss_debug_vir;
		ex_log_len = md->smem_layout.ccci_exp_smem_mdss_debug_size;
    }
    if(dump_flag & CCCI_AED_DUMP_EX_PKT) {
        ex_log_addr = (void *)&md->ex_info;
        ex_log_len = sizeof(EX_LOG_T);
    }
    if(dump_flag & CCCI_AED_DUMP_MD_IMG_MEM) {
        md_img_addr = (void *)md->mem_layout.md_region_vir;
        md_img_len = MD_IMG_DUMP_SIZE;
    }

#if defined (CONFIG_MTK_AEE_FEATURE)
    aed_md_exception_api(ex_log_addr, ex_log_len, md_img_addr, md_img_len, buff, db_opt);
#endif
}

static void ccci_ee_info_dump(struct ccci_modem *md)
{
    char ex_info[EE_BUF_LEN]=""; // attention, be careful with string length!
    char i_bit_ex_info[EE_BUF_LEN]="\n[Others] May I-Bit dis too long\n";
    int db_opt = (DB_OPT_DEFAULT|DB_OPT_FTRACE);
    int dump_flag = 0;
	DEBUG_INFO_T *debug_info = &md->debug_info;
	unsigned char c;

    struct rtc_time        tm;
    struct timeval        tv = {0};
    struct timeval        tv_android = {0};
    struct rtc_time        tm_android;

    do_gettimeofday(&tv);
    tv_android = tv;
    rtc_time_to_tm(tv.tv_sec, &tm);
    tv_android.tv_sec -= sys_tz.tz_minuteswest*60;
    rtc_time_to_tm(tv_android.tv_sec, &tm_android);
    CCCI_INF_MSG(md->index, KERN, "Sync:%d%02d%02d %02d:%02d:%02d.%u(%02d:%02d:%02d.%03d(TZone))\n", 
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
           tm.tm_hour, tm.tm_min, tm.tm_sec,
           (unsigned int) tv.tv_usec,
           tm_android.tm_hour, tm_android.tm_min, tm_android.tm_sec,
           (unsigned int) tv_android.tv_usec);

    CCCI_INF_MSG(md->index, KERN, "exception type(%d):%s\n",debug_info->type,debug_info->name?:"Unknown");

    switch(debug_info->type)
    {
        case MD_EX_TYPE_ASSERT_DUMP:
        case MD_EX_TYPE_ASSERT:
            CCCI_INF_MSG(md->index, KERN, "filename = %s\n", debug_info->assert.file_name);
            CCCI_INF_MSG(md->index, KERN, "line = %d\n", debug_info->assert.line_num);
            CCCI_INF_MSG(md->index, KERN, "para0 = %d, para1 = %d, para2 = %d\n", 
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
        case MD_EX_TYPE_UNDEF:
        case MD_EX_TYPE_SWI:
        case MD_EX_TYPE_PREF_ABT:
        case MD_EX_TYPE_DATA_ABT:
        case MD_EX_TYPE_FATALERR_BUF:
        case MD_EX_TYPE_FATALERR_TASK:
            CCCI_INF_MSG(md->index, KERN, "fatal error code 1 = %d\n", debug_info->fatal_error.err_code1);
            CCCI_INF_MSG(md->index, KERN, "fatal error code 2 = %d\n", debug_info->fatal_error.err_code2);
			CCCI_INF_MSG(md->index, KERN, "fatal error offender %s\n", debug_info->fatal_error.offender);
			if(debug_info->fatal_error.offender[0] != '\0') {
				snprintf(ex_info,EE_BUF_LEN,"\n[%s] err_code1:%d err_code2:%d\nMD Offender:%s\n", debug_info->name, 
						debug_info->fatal_error.err_code1, debug_info->fatal_error.err_code2,
						debug_info->fatal_error.offender);
			} else {
            snprintf(ex_info,EE_BUF_LEN,"\n[%s] err_code1:%d err_code2:%d\n", debug_info->name, 
                    debug_info->fatal_error.err_code1, debug_info->fatal_error.err_code2);
			}
            break;
        case MD_EX_TYPE_EMI_CHECK:
            CCCI_INF_MSG(md->index, KERN, "md_emi_check: %08X, %08X, %02d, %08X\n", 
                    debug_info->data.data0, debug_info->data.data1,
                    debug_info->data.channel, debug_info->data.reserved);
            snprintf(ex_info,EE_BUF_LEN,"\n[emi_chk] %08X, %08X, %02d, %08X\n", 
                    debug_info->data.data0, debug_info->data.data1,
                    debug_info->data.channel, debug_info->data.reserved);
            break;
        case DSP_EX_TYPE_ASSERT:
            CCCI_INF_MSG(md->index, KERN, "filename = %s\n", debug_info->dsp_assert.file_name);
            CCCI_INF_MSG(md->index, KERN, "line = %d\n", debug_info->dsp_assert.line_num);
            CCCI_INF_MSG(md->index, KERN, "exec unit = %s\n", debug_info->dsp_assert.execution_unit);
            CCCI_INF_MSG(md->index, KERN, "para0 = %d, para1 = %d, para2 = %d\n", 
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
            CCCI_INF_MSG(md->index, KERN, "exec unit = %s, code1:0x%08x\n", debug_info->dsp_exception.execution_unit,
                    debug_info->dsp_exception.code1);
            snprintf(ex_info,EE_BUF_LEN,"\n[%s] exec:%s code1:0x%08x\n",
                    debug_info->name, debug_info->dsp_exception.execution_unit,
                    debug_info->dsp_exception.code1);
            break;
        case DSP_EX_FATAL_ERROR:
            CCCI_INF_MSG(md->index, KERN, "exec unit = %s\n", debug_info->dsp_fatal_err.execution_unit);
            CCCI_INF_MSG(md->index, KERN, "err_code0 = 0x%08x, err_code1 = 0x%08x\n", 
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
    switch(debug_info->more_info) {
    case MD_EE_CASE_ONLY_SWINT:
        strcat(ex_info, "\nOnly SWINT case\n");
        break;
    case MD_EE_CASE_SWINT_MISSING:
        strcat(ex_info, "\nSWINT missing case\n");
        break;
    case MD_EE_CASE_ONLY_EX:
        strcat(ex_info, "\nOnly EX case\n");
        break;
    case MD_EE_CASE_ONLY_EX_OK:
        strcat(ex_info, "\nOnly EX_OK case\n");
        break;
    case MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG:
        strcat(i_bit_ex_info, ex_info);
        strcpy(ex_info, i_bit_ex_info);
        break;
    case MD_EE_CASE_TX_TRG:
    case MD_EE_CASE_ISR_TRG:
        strcat(ex_info, "\n[Others] May I-Bit dis too long\n");
        break;
    case MD_EE_CASE_NO_RESPONSE:
        // use strcpy, otherwise if this happens after a MD EE, the former EE info will be printed out
        strcpy(ex_info, "\n[Others] MD long time no response\n");
        db_opt |= DB_OPT_FTRACE;
        break;
    case MD_EE_CASE_WDT:
        strcpy(ex_info, "\n[Others] MD watchdog timeout interrupt\n");
        break;
    default:
        break;
    }

	// get ELM_status field from MD side
	c = md->ex_info.envinfo.ELM_status;
	CCCI_INF_MSG(md->index, KERN, "ELM_status: %x\n", c);
	switch (c)
	{
		case 0xFF:
			strcat(ex_info, "\nno ELM info\n");
			break;
		case 0xAE:
			strcat(ex_info, "\nELM rlat:FAIL\n");
			break;
		case 0xBE:
			strcat(ex_info, "\nELM wlat:FAIL\n");
			break;
		case 0xDE:
			strcat(ex_info, "\nELM r/wlat:PASS\n");
			break;
		default:
			break;
	}

    // Dump MD EE info
    CCCI_INF_MSG(md->index, KERN, "Dump MD EX log\n");
    if(debug_info->more_info == MD_EE_CASE_NORMAL && 
        md->boot_stage == MD_BOOT_STAGE_0) {
        ccci_mem_dump(md->index, &md->ex_info, sizeof(EX_LOG_T));
    } else {
        ccci_mem_dump(md->index, md->smem_layout.ccci_exp_smem_base_vir, md->smem_layout.ccci_exp_dump_size);
    }
    // Dump MD image memory
    CCCI_INF_MSG(md->index, KERN, "Dump MD image memory\n");
    ccci_mem_dump(md->index, (void *)md->mem_layout.md_region_vir, MD_IMG_DUMP_SIZE);
    // Dump MD memory layout
    CCCI_INF_MSG(md->index, KERN, "Dump MD layout struct\n");
    ccci_mem_dump(md->index, &md->mem_layout, sizeof(struct ccci_mem_layout));
    // Dump MD register
    md->ops->dump_info(md, DUMP_FLAG_REG, NULL, 0);

    if(debug_info->more_info == MD_EE_CASE_NORMAL && 
        md->boot_stage == MD_BOOT_STAGE_0) { // MD will not fill in share memory before we send runtime data
        dump_flag = CCCI_AED_DUMP_EX_PKT;
    } else { // otherwise always dump whole share memory, as MD will fill debug log into its 2nd 1K region after bootup
        dump_flag = CCCI_AED_DUMP_EX_MEM;
        if(debug_info->more_info == MD_EE_CASE_NO_RESPONSE)
            dump_flag |= CCCI_AED_DUMP_CCIF_REG;
    }
    md->boot_stage = MD_BOOT_STAGE_EXCEPTION; // update here to maintain handshake stage info druing exception handling
    ccci_aed(md, dump_flag, ex_info, db_opt);
	if(debug_info->more_info == MD_EE_CASE_ONLY_SWINT)	{
        md->ops->dump_info(md, DUMP_FLAG_CLDMA, NULL, 0);
    }
	if(*((int *)(md->mem_layout.smem_region_vir+CCCI_SMEM_OFFSET_SEQERR)) != 0) {
		CCCI_INF_MSG(md->index, KERN, "MD found wrong sequence number\n");
		md->ops->dump_info(md, DUMP_FLAG_CLDMA, NULL, 0);
	}
}

/*
 * copy raw data (EX_LOG_T) received from modem into CCCI's DEBUG_INFO_T
 */
static void ccci_md_exception(struct ccci_modem *md)
{
    EX_LOG_T    *ex_info;
	int			ee_type, ee_case;
	DEBUG_INFO_T *debug_info = &md->debug_info;

    if(debug_info == NULL) {
        return;
    }

	if(debug_info->more_info==MD_EE_CASE_NORMAL &&
		md->boot_stage == MD_BOOT_STAGE_0) {
		ex_info = &md->ex_info;
	} else {
        ex_info = (EX_LOG_T    *)md->smem_layout.ccci_exp_rec_base_vir;
	}
	ee_case = debug_info->more_info;
	
    memset(debug_info, 0, sizeof(DEBUG_INFO_T));
    ee_type = ex_info->header.ex_type;
    debug_info->type = ee_type;
	debug_info->more_info = ee_case;
    md->ex_type = ee_type;

	if(*((char *)ex_info+CCCI_EXREC_OFFSET_OFFENDER) != 0xCC) {
		memcpy(debug_info->fatal_error.offender, (char *)ex_info+CCCI_EXREC_OFFSET_OFFENDER, sizeof(debug_info->fatal_error.offender)-1);
		debug_info->fatal_error.offender[sizeof(debug_info->fatal_error.offender)-1] = '\0';
	} else {
		debug_info->fatal_error.offender[0] = '\0';
	}

    switch (ee_type) 
    {
        case MD_EX_TYPE_INVALID:
            debug_info->name="INVALID";
            break;

        case MD_EX_TYPE_UNDEF:
            debug_info->name="Fatal error (undefine)";
            debug_info->fatal_error.err_code1=ex_info->content.fatalerr.error_code.code1;
            debug_info->fatal_error.err_code2=ex_info->content.fatalerr.error_code.code2;
            break;

        case MD_EX_TYPE_SWI:
            debug_info->name="Fatal error (swi)";
            debug_info->fatal_error.err_code1=ex_info->content.fatalerr.error_code.code1;
            debug_info->fatal_error.err_code2=ex_info->content.fatalerr.error_code.code2;
            break;

        case MD_EX_TYPE_PREF_ABT:
            debug_info->name="Fatal error (prefetch abort)";
            debug_info->fatal_error.err_code1=ex_info->content.fatalerr.error_code.code1;
            debug_info->fatal_error.err_code2=ex_info->content.fatalerr.error_code.code2;
            break;

        case MD_EX_TYPE_DATA_ABT:
            debug_info->name="Fatal error (data abort)";
            debug_info->fatal_error.err_code1=ex_info->content.fatalerr.error_code.code1;
            debug_info->fatal_error.err_code2=ex_info->content.fatalerr.error_code.code2;
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

    debug_info->ext_mem = ex_info;
    debug_info->ext_size = sizeof(EX_LOG_T);
    debug_info->md_image = (void *)md->mem_layout.md_region_vir;
    debug_info->md_size = MD_IMG_DUMP_SIZE;
}

void md_ex_monitor_func(unsigned long data)
{
    int ee_on_going = 0;
    int ee_case;
    int need_update_state = 0;
    unsigned long flags;
    unsigned int ee_info_flag = 0;
    struct ccci_modem *md = (struct ccci_modem *)data;
#if defined (CONFIG_MTK_AEE_FEATURE)	
    CCCI_INF_MSG(md->index, KERN, "MD exception timer 1:disable tracing\n");
    tracing_off();
#endif
	CCCI_INF_MSG(md->index, KERN, "MD exception timer 1! ee=%x\n", md->ee_info_flag);
    spin_lock_irqsave(&md->ctrl_lock, flags);
    if((1<<MD_EE_DUMP_ON_GOING) & md->ee_info_flag) {
        ee_on_going = 1;
    } else {
        ee_info_flag = md->ee_info_flag;
        md->ee_info_flag |= (1<<MD_EE_DUMP_ON_GOING);
    }
    spin_unlock_irqrestore(&md->ctrl_lock, flags);                

    if(ee_on_going)
        return;

    if ((ee_info_flag&((1<<MD_EE_MSG_GET)|(1<<MD_EE_OK_MSG_GET)|(1<<MD_EE_SWINT_GET))) == \
                        ((1<<MD_EE_MSG_GET)|(1<<MD_EE_OK_MSG_GET)|(1<<MD_EE_SWINT_GET))) {
        ee_case = MD_EE_CASE_NORMAL;
        CCCI_DBG_MSG(md->index, KERN, "Recv SWINT & MD_EX & MD_EX_REC_OK\n");
        if (ee_info_flag & (1 << MD_EE_AP_MASK_I_BIT_TOO_LONG)) {
            ee_case = MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG;
        }
    } else if(!(ee_info_flag&(1<<MD_EE_SWINT_GET)) && (ee_info_flag&((1<<MD_EE_MSG_GET)|(1<<MD_EE_SWINT_GET)))) {
        ee_case = MD_EE_CASE_SWINT_MISSING;
        CCCI_INF_MSG(md->index, KERN, \
                                "SWINT missing, ee_info_flag=%x\n", ee_info_flag);
    } else if((ee_info_flag&((1<<MD_EE_MSG_GET)|(1<<MD_EE_SWINT_GET))) & (1<<MD_EE_MSG_GET)) {
        ee_case = MD_EE_CASE_ONLY_EX;
        CCCI_INF_MSG(md->index, KERN, \
                                "Only recv SWINT & MD_EX.\n");
        if (ee_info_flag & (1 << MD_EE_AP_MASK_I_BIT_TOO_LONG)) {
            ee_case = MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG;
        }
    } else if((ee_info_flag&((1<<MD_EE_OK_MSG_GET)|(1<<MD_EE_SWINT_GET))) & (1<<MD_EE_OK_MSG_GET)) {
        ee_case = MD_EE_CASE_ONLY_EX_OK;
        CCCI_INF_MSG(md->index, KERN, \
                                "Only recv SWINT & MD_EX_OK\n");
        if (ee_info_flag & (1 << MD_EE_AP_MASK_I_BIT_TOO_LONG)) {
            ee_case = MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG;
        }
    } else if(ee_info_flag&(1<<MD_EE_SWINT_GET)) {
        ee_case = MD_EE_CASE_ONLY_SWINT;
        CCCI_INF_MSG(md->index, KERN, \
                                "Only recv SWINT.\n");
        if((ee_info_flag&(1<<MD_STATE_UPDATE))==0){
            need_update_state = 1;
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
        }
    } else if(ee_info_flag&(1<<MD_EE_WDT_GET)) {
        ee_case = MD_EE_CASE_WDT;
        if((ee_info_flag&(1<<MD_STATE_UPDATE))==0){
            need_update_state = 1;
        }
    } else { 
        CCCI_ERR_MSG(md->index, KERN, "Invalid MD_EX, ee_info=%x\n", ee_info_flag);
        goto _dump_done;
    }

    if(need_update_state) {
        //md->boot_stage = MD_BOOT_STAGE_EXCEPTION;
        md->config.setting |= MD_SETTING_RELOAD;
        md->ops->broadcast_state(md, EXCEPTION);
    }

    if(md->boot_stage < MD_BOOT_STAGE_2) {
        md->ops->broadcast_state(md, BOOT_FAIL);
    }

    //ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_NOTIFY, ee_case); // unsafe in timer context
	md->debug_info.more_info = ee_case;
	// Dump MD EE info
	CCCI_INF_MSG(md->index, KERN, "Dump MD EX log\n");
	ccci_mem_dump(md->index, md->smem_layout.ccci_exp_smem_base_vir, md->smem_layout.ccci_exp_dump_size);
	// Dump MD register
	md->ops->dump_info(md, DUMP_FLAG_REG, NULL, 0);
	mod_timer(&md->ex_monitor2, jiffies+EX_TIMER_MD_EX_REC_OK*HZ);

_dump_done:
    return;
}
EXPORT_SYMBOL(md_ex_monitor_func);

void md_ex_monitor2_func(unsigned long data)
{
    struct ccci_modem *md = (struct ccci_modem *)data;
    unsigned long flags;
    
	int ee_on_going = 0;
	
	CCCI_INF_MSG(md->index, KERN, "MD exception timer 2! ee=%x\n", md->ee_info_flag);

	spin_lock_irqsave(&md->ctrl_lock, flags);
	if((1<<MD_EE_TIMER2_DUMP_ON_GOING) & md->ee_info_flag) {
		ee_on_going = 1;
	} else {
		md->ee_info_flag |= (1<<MD_EE_TIMER2_DUMP_ON_GOING);
	}
	spin_unlock_irqrestore(&md->ctrl_lock, flags);				

	if(ee_on_going)
		return;

	ccci_md_exception(md);
	ccci_ee_info_dump(md);
	
    spin_lock_irqsave(&md->ctrl_lock, flags);
    md->ee_info_flag = 0; // this should be the last action of a regular exception flow, clear flag for reset MD later
    spin_unlock_irqrestore(&md->ctrl_lock, flags);

	CCCI_INF_MSG(md->index, KERN, "Enable WDT at exception exit.");
	md->ops->ee_callback(md, EE_FLAG_ENABLE_WDT);
}
EXPORT_SYMBOL(md_ex_monitor2_func);

void md_bootup_timeout_func(unsigned long data)
{
    struct ccci_modem *md = (struct ccci_modem *)data;
    char ex_info[EE_BUF_LEN]="";
    
    CCCI_INF_MSG(md->index, KERN, "MD_BOOT_HS%d_FAIL!\n", (md->boot_stage+1));
    md->ops->broadcast_state(md, BOOT_FAIL);
    if(md->config.setting & MD_SETTING_STOP_RETRY_BOOT)
        return;

    //ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_BOOT_TIMEOUT, 0);
    snprintf(ex_info, EE_BUF_LEN, "\n[Others] MD_BOOT_UP_FAIL(HS%d)\n", (md->boot_stage+1));
    CCCI_INF_MSG(md->index, KERN, "Dump MD image memory\n");
    ccci_mem_dump(md->index, (void*)md->mem_layout.md_region_vir, MD_IMG_DUMP_SIZE);
    CCCI_INF_MSG(md->index, KERN, "Dump MD layout struct\n");
    ccci_mem_dump(md->index, &md->mem_layout, sizeof(struct ccci_mem_layout));
    md->ops->dump_info(md, DUMP_FLAG_CLDMA, NULL, 0);
    
    if(md->boot_stage == MD_BOOT_STAGE_0) {
        // Handshake 1 fail
        ccci_aed(md, CCCI_AED_DUMP_CCIF_REG|CCCI_AED_DUMP_MD_IMG_MEM, ex_info, DB_OPT_DEFAULT);
    } else if(md->boot_stage == MD_BOOT_STAGE_1) {
        // Handshake 2 fail
        CCCI_INF_MSG(md->index, KERN, "Dump MD EX log\n");
        ccci_mem_dump(md->index, md->smem_layout.ccci_exp_smem_base_vir, md->smem_layout.ccci_exp_dump_size);
        
        ccci_aed(md, CCCI_AED_DUMP_CCIF_REG|CCCI_AED_DUMP_EX_MEM, ex_info, DB_OPT_FTRACE);
    }
}
EXPORT_SYMBOL(md_bootup_timeout_func);

