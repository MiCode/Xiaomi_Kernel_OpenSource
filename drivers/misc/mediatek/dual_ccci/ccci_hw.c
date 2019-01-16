#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <ccci_common.h>


#define  CCIF_DEBUG    //ccif issue debug

extern unsigned long long lg_ch_tx_debug_enable[];
extern unsigned long long lg_ch_rx_debug_enable[];



static int __ccif_v1_en_intr(ccif_t* ccif)
{
    unsigned long flag;
    CCCI_FUNC_ENTRY(ccif->m_md_id);

    spin_lock_irqsave(&ccif->m_lock,flag);
    if(ccif->m_irq_dis_cnt) {
        enable_irq(ccif->m_irq_id);
        ccif->m_irq_dis_cnt--;
    }
    spin_unlock_irqrestore(&ccif->m_lock,flag);

    return 0;
}


static void __ccif_v1_dis_intr(ccif_t* ccif)
{
    unsigned long flag;
    CCCI_FUNC_ENTRY(ccif->m_md_id);

    spin_lock_irqsave(&ccif->m_lock,flag);
    if(ccif->m_irq_dis_cnt == 0) {
        disable_irq(ccif->m_irq_id);
        ccif->m_irq_dis_cnt++;
    }
    spin_unlock_irqrestore(&ccif->m_lock,flag);
}


static int __ccif_v1_dump_reg(ccif_t* ccif, unsigned int buf[], int len)
{
    int i,j;
    volatile unsigned int *curr_ccif_smem_addr = (volatile unsigned int *)CCIF_TXCHDATA(ccif->m_reg_base);

    CCCI_DBG_MSG(ccif->m_md_id, "cci", "[CCCI REG_INFO]\n");
    CCCI_DBG_MSG(ccif->m_md_id, "cci", "CON(%lx)=%08X, BUSY(%lx)=%08x, START(%lx)=%08x, MRCHNUM(%lx)=%08x\n", 
                    CCIF_CON(ccif->m_reg_base), ccci_read32(CCIF_CON(ccif->m_reg_base)),
                    CCIF_BUSY(ccif->m_reg_base), ccci_read32(CCIF_BUSY(ccif->m_reg_base)),
                    CCIF_START(ccif->m_reg_base), ccci_read32(CCIF_START(ccif->m_reg_base)),
                    MD_CCIF_RCHNUM(ccif->m_md_reg_base), ccci_read32(MD_CCIF_RCHNUM(ccif->m_md_reg_base)));
    CCCI_DBG_MSG(ccif->m_md_id, "cci", "MCON(%lx)=%08X, MBUSY(%lx)=%08x, MSTART(%lx)=%08x, RCHNUM(%lx)=%08x\n", 
                    MD_CCIF_CON(ccif->m_md_reg_base), ccci_read32(MD_CCIF_CON(ccif->m_md_reg_base)),
                    MD_CCIF_BUSY(ccif->m_md_reg_base), ccci_read32(MD_CCIF_BUSY(ccif->m_md_reg_base)),
                    MD_CCIF_START(ccif->m_md_reg_base), ccci_read32(MD_CCIF_START(ccif->m_md_reg_base)),
                    CCIF_RCHNUM(ccif->m_reg_base), ccci_read32(CCIF_RCHNUM(ccif->m_reg_base)));

    for(i=0; i<16; i++){
        CCCI_DBG_MSG(ccif->m_md_id, "cci", "%08X: %08X %08X %08X %08X\n", (unsigned int)curr_ccif_smem_addr, \
            curr_ccif_smem_addr[0], curr_ccif_smem_addr[1],
            curr_ccif_smem_addr[2], curr_ccif_smem_addr[3]);
        curr_ccif_smem_addr+=4;
    }

    if(buf == NULL || len < (4*16+8)){
        // Only dump by log
        return 0;
    }else{
        j=0;
        buf[j++] = ccci_read32(CCIF_CON(ccif->m_reg_base));
        buf[j++] = ccci_read32(CCIF_BUSY(ccif->m_reg_base));
        buf[j++] = ccci_read32(CCIF_START(ccif->m_reg_base));
        buf[j++] = ccci_read32(MD_CCIF_RCHNUM(ccif->m_reg_base));
        
        buf[j++] = ccci_read32(MD_CCIF_CON(ccif->m_reg_base));
        buf[j++] = ccci_read32(MD_CCIF_BUSY(ccif->m_reg_base));
        buf[j++] = ccci_read32(MD_CCIF_START(ccif->m_reg_base));
        buf[j++] = ccci_read32(CCIF_RCHNUM(ccif->m_reg_base));
        curr_ccif_smem_addr = (volatile unsigned int *)CCIF_TXCHDATA(ccif->m_reg_base);
        for(i=0; i<4*16; i++)
            buf[j++] = curr_ccif_smem_addr[i];
    }

    return j;
}


static int __ccif_v1_read_phy_ch_data(ccif_t* ccif, int ch, unsigned int buf[])
{
    ccif_msg_t *rx_msg = (ccif_msg_t*)(CCIF_RXCHDATA(ccif->m_reg_base));
    buf[0] = rx_msg[ch].data[0];
    buf[1] = rx_msg[ch].data[1];
    buf[2] = rx_msg[ch].channel;
    buf[3] = rx_msg[ch].reserved;
    return sizeof(ccif_msg_t);
}


static int  __ccif_v1_write_phy_ch_data(ccif_t* ccif, unsigned int buf[], int retry_en)
{
    int             ret = 0;
    unsigned int    busy;
    unsigned long    flag;
    unsigned int    retry_count = 200;
    unsigned int    ch;
    ccif_msg_t        *tx_msg;
    int md_id = ccif->m_md_id;

    CCCI_FUNC_ENTRY(md_id);
    if(retry_en == 0)
        retry_count = 1;

    do{
        spin_lock_irqsave(&ccif->m_lock, flag);
        busy=ccci_read32(CCIF_BUSY(ccif->m_reg_base));

        ch = ccif->m_tx_idx;
        if (busy&(1<<ch)) {                
            ret = -CCCI_ERR_CCIF_NO_PHYSICAL_CHANNEL;
            if (busy != 0xff) {
                CCCI_DBG_MSG(md_id, "cci", "Wrong Busy value: 0x%X\n", busy);
            }
            spin_unlock_irqrestore(&ccif->m_lock,flag);
            
            udelay(1);
            retry_count--;
        } else {
            ccci_write32(CCIF_BUSY(ccif->m_reg_base), 1<<ch);
            ccif->m_tx_idx++;
            ccif->m_tx_idx &= (CCIF_STD_V1_MAX_CH_NUM-1);
            //spin_unlock_irqrestore(&ccif->m_lock,flag);
            
            //mb(); // Note here, make sure data has write to memory be really send
            tx_msg = (ccif_msg_t*)(CCIF_TXCHDATA(ccif->m_reg_base));
            ccci_write32(&(tx_msg[ch].data[0]), buf[0]);
            ccci_write32(&(tx_msg[ch].data[1]), buf[1]);
            ccci_write32(&(tx_msg[ch].channel), buf[2]);
            ccci_write32(&(tx_msg[ch].reserved), buf[3]);
            //mb();

            ccci_write32(CCIF_TCHNUM(ccif->m_reg_base), ch);
            
            spin_unlock_irqrestore(&ccif->m_lock,flag);

            ret = sizeof(ccif_msg_t);
            break;
        }
    }while(retry_count>0);

    if(lg_ch_tx_debug_enable[md_id] & (1<< buf[2])) 
        CCCI_MSG_INF(md_id, "cci", "[TX]: %08X, %08X, %02d, %08X (%02d)\n",
             buf[0], buf[1], buf[2], buf[3], ch);

    return ret;
}


static int  __ccif_v1_get_rx_ch(ccif_t* ccif)
{
    while (CCIF_CON_ARB != ccci_read32(CCIF_CON(ccif->m_reg_base))){
        ccci_write32(CCIF_CON(ccif->m_reg_base), CCIF_CON_ARB);
    }
    return ccci_read32(CCIF_RCHNUM(ccif->m_reg_base));
}


static int  __ccif_v1_get_busy_state(ccif_t* ccif)
{
    return ccci_read32(CCIF_BUSY(ccif->m_reg_base));
}


static void __ccif_v1_set_busy_state(ccif_t* ccif, unsigned int val)
{
    //*CCIF_BUSY(ccif->m_reg_base) = val;
    ccci_write32(CCIF_BUSY(ccif->m_reg_base), val);
}


static int  __ccif_v1_ack(ccif_t* ccif, int ch)
{
    //*CCIF_ACK(ccif->m_reg_base) = (1 << ch);
    ccci_write32(CCIF_ACK(ccif->m_reg_base), (1<<ch));

    return 0;
}


static int  __ccif_v1_clear_sram(ccif_t* ccif)
{
    int i;
    volatile unsigned int *ccif_tx_addr = (volatile unsigned int *) CCIF_TXCHDATA(ccif->m_reg_base);

    for(i=0; i<4*16; i++){
        ccci_write32(&ccif_tx_addr[i], 0);
    }
    return 0;
}


static int  __ccif_v1_write_runtime_data(ccif_t* ccif, unsigned int buf[], int len)
{
    int i;
    volatile unsigned int *curr_ccif_smem_addr = (unsigned int*)(ccif->m_reg_base+CCIF_STD_V1_RUN_TIME_DATA_OFFSET);
    if ( len > CCIF_STD_V1_RUM_TIME_MEM_MAX_LEN ) {
        return -CCCI_ERR_CCIF_INVALID_RUNTIME_LEN;
    }
    for(i=0; i<len; i++){
        ccci_write32(&curr_ccif_smem_addr[i], buf[i]);
    }
    //__ccif_v1_dump_reg(ccif, NULL, 0);
    return 0;
}


static int  __ccif_v1_reset(ccif_t* ccif)
{
    ccci_write32(CCIF_CON(ccif->m_reg_base), 1);
    ccif->m_rx_idx = 0;
    ccif->m_tx_idx = 0;
    // ACK MD all channel
    ccci_write32(CCIF_ACK(ccif->m_reg_base), 0xFF);
    __ccif_v1_clear_sram(ccif);

    return 0;
}


// Note: This is a common function
static irqreturn_t __ccif_irq_handler(int irq, void *data)
{
    int ret;
    ccif_t *ccif = (ccif_t*)data;

    ret = ccif->ccif_intr_handler(ccif);
    if(ret){
        CCCI_MSG_INF(ccif->m_md_id, "cci", "ccif_irq_handler fail: %d!\n", ret);
    }
    
    return IRQ_HANDLED;
}


static int __ccif_v1_reg_intr(ccif_t* ccif)
{
    int ret;
    unsigned long flags;

    spin_lock_irqsave(&ccif->m_lock,flags);
    ccif->m_irq_dis_cnt=0;
    spin_unlock_irqrestore(&ccif->m_lock,flags);
    ret = request_irq(ccif->m_irq_id, __ccif_irq_handler, IRQF_TRIGGER_LOW, "CCIF", ccif);

    return ret;
}


static int __ccif_v1_init(ccif_t* ccif)
{
    //*CCIF_CON(ccif->m_reg_base) = 1;
    ccci_write32(CCIF_CON(ccif->m_reg_base), CCIF_CON_ARB);
    ccif->m_rx_idx = 0;
    ccif->m_tx_idx = 0;
    // ACK MD all channel
    //*CCIF_ACK(ccif->m_reg_base) = 0xFF;
    ccci_write32(CCIF_ACK(ccif->m_reg_base), 0xFF);
    __ccif_v1_clear_sram(ccif);

    return 0;
}


static int __ccif_v1_de_init(ccif_t* ccif)
{
    // Disable ccif irq, no need for there is kernel waring of free already-free irq when free_irq
    //ccif->ccif_dis_intr(ccif);

    // Check if TOP half is running
    while(test_bit(CCIF_TOP_HALF_RUNNING,&ccif->m_status))
        yield();

    WARN_ON(spin_is_locked(&ccif->m_lock));

    // Un-register irq
    free_irq(ccif->m_irq_id,ccif);

    // Free memory
    kfree(ccif);

    return 0;
}


static int __ccif_v1_register_call_back(ccif_t* ccif, int (*push_func)(ccif_msg_t*, void*), void (*notify_func)(void*))
{
    if(!test_and_set_bit(CCIF_CALL_BACK_FUNC_LOCKED, &ccif->m_status)) {
        ccif->push_msg = push_func;
        ccif->notify_push_done = notify_func;
        return 0;
    } else {
        CCCI_DBG_MSG(ccif->m_md_id, "cci", "[Error]ccif call back func has registered!\n");
        return CCCI_ERR_CCIF_CALL_BACK_HAS_REGISTERED;
    }
}


static int __ccif_v1_register_isr_notify(ccif_t* ccif, void (*notify_func)(int))
{
    if(!test_and_set_bit(CCIF_ISR_INFO_CALL_BACK_LOCKED, &ccif->m_status)){
        ccif->isr_notify = notify_func;
        return 0;
    } else {
        CCCI_DBG_MSG(ccif->m_md_id, "cci", "[Error]ccif isr call back func has registered\n");
        return CCCI_ERR_CCIF_CALL_BACK_HAS_REGISTERED;
    }
}


static int __ccif_v1_intr_handler(ccif_t *ccif)
{
    ccif_msg_t        phy_ch_data;
    int                re_enter_cnt = 0;
    int                r_ch_val;
    int                i;
    int                rx_ch;
    int                md_id = ccif->m_md_id;
    bool             reg_err = FALSE;
    unsigned int    msg[4];

    CCCI_FUNC_ENTRY(md_id);
    set_bit(CCIF_TOP_HALF_RUNNING,&ccif->m_status);
                            
    //CCCI_DBG_MSG(md_id, "cci", "ISR\n");
                            
    if(ccif->isr_notify)
        ccif->isr_notify(md_id);

    rx_ch = ccif->m_rx_idx;

    while( (r_ch_val = ccif->ccif_get_rx_ch(ccif)) && (re_enter_cnt<CCIF_INTR_MAX_RE_ENTER_CNT) )
    {
        for(i=0; i<CCIF_STD_V1_MAX_CH_NUM; i++)
        {
            if (r_ch_val&(1<<rx_ch)) {
                // We suppose always read success
                ccif->ccif_read_phy_ch_data(ccif, rx_ch, (unsigned int*)&phy_ch_data);
                #ifdef CCIF_DEBUG
                if (phy_ch_data.channel >= CCCI_MAX_CH_NUM) {
                    if (!reg_err) {
                        reg_err = TRUE;
                        __ccif_v1_dump_reg(ccif, NULL, 0);
                        CCCI_MSG_INF(md_id, "cci", "[CCIF Register Error]RX: %08X, %08X, %02d, %08X (%02d)\n", phy_ch_data.data[0], \
                            phy_ch_data.data[1], phy_ch_data.channel, phy_ch_data.reserved, rx_ch);
                    }
                }
                #endif

                if((lg_ch_rx_debug_enable[md_id] & ENABLE_ALL_RX_LOG) || 
                    (lg_ch_rx_debug_enable[md_id] & (1<< phy_ch_data.channel))) {
                    CCCI_DBG_MSG(md_id, "cci", "[RX]: %08X, %08X, %02d, %08X (%02d)\n", phy_ch_data.data[0], \
                        phy_ch_data.data[1], phy_ch_data.channel, phy_ch_data.reserved, rx_ch);
                }

                // push ccif message to up layer
                if (unlikely(ccif->push_msg == NULL)) {
                    CCCI_DBG_MSG(md_id, "cci", "push_msg func not registered:0x%08x, 0x%08x, %02d, 0x%08x\n", \
                            phy_ch_data.data[0], phy_ch_data.data[1], phy_ch_data.channel, phy_ch_data.reserved);
                } else {
                    if ( ccif->push_msg(&phy_ch_data, ccif->m_logic_ctl_block) != sizeof(ccif_msg_t) ) {
                        //CCCI_DBG_MSG(md_id, "cci", "push data fail(ch%d)\n", phy_ch_data.channel);
                    }
                }

                // Ack modem side ccif
                ccci_write32(CCIF_ACK(ccif->m_reg_base), (1<<rx_ch));

                r_ch_val &=~(1<<rx_ch);
            } else  {
                if (r_ch_val != 0) {
                    // We suppose rx channel usage should be fifo mode
                    CCCI_DBG_MSG(md_id, "cci", "rx channel error(rx>%02x : %d<curr)\n", r_ch_val, rx_ch);
                    __ccif_v1_dump_reg(ccif, NULL, 0);
                } else {
                    break;
                }
            }
            ++rx_ch;
            rx_ch = rx_ch&(CCIF_STD_V1_MAX_CH_NUM-1);
        }
        re_enter_cnt++;
    }
    
    if( (re_enter_cnt>=CCIF_INTR_MAX_RE_ENTER_CNT) && (r_ch_val!=0) ) {
        CCCI_DBG_MSG(md_id, "cci", "too much message to process\n");
        __ccif_v1_dump_reg(ccif, NULL, 0);
    }

    // Store latest rx channel index
    ccif->m_rx_idx = rx_ch;

    // Notify uplayer begin to process data
    if (unlikely(ccif->notify_push_done == NULL)) {
        //CCCI_DBG_MSG(md_id, "cci", "notify_push_done not registered!\n");
    } else {
        ccif->notify_push_done(ccif->m_logic_ctl_block);
    }

    clear_bit(CCIF_TOP_HALF_RUNNING,&ccif->m_status);

    
    #ifdef CCIF_DEBUG
    if (reg_err) {        
        reg_err = FALSE;
        msg[0]    = 0xFFFFFFFF;
        msg[1]    = 0x5B5B5B5B;
        msg[2]    = CCCI_FORCE_ASSERT_CH;
        msg[3]  = 0xB5B5B5B5;
        __ccif_v1_write_phy_ch_data(ccif, msg, 0);
    }
    #endif

    return 0;
}


ccif_t* ccif_create_instance(ccif_hw_info_t *info, void* ctl_b, int md_id)
{
    ccif_t            *ccif;

    if(info == NULL){
        CCCI_MSG_INF(md_id, "cci", "[error]ccif hw info is null\n");
        return NULL;
    }

    ccif = kmalloc(sizeof(ccif_t), GFP_KERNEL);
    if(ccif == NULL){
        CCCI_MSG_INF(md_id, "cci", "[error]allocate memory for ccif structure fail\n");
        return NULL;
    }

    if (info->md_id != md_id) {
        CCCI_MSG_INF(md_id, "cci", "[error]ccif_instance_md_id is mis-match to hw_info_md_id: (%d, %d)\n",
            md_id, info->md_id);
        return NULL;
    }

    switch(info->type){
        case CCIF_STD_V1:
            ccif->m_ccif_type = info->type;
            ccif->m_irq_id = info->irq_id;
            ccif->m_reg_base = info->reg_base;
            ccif->m_md_reg_base = info->md_reg_base;
            ccif->m_irq_attr = info->irq_attr;
            ccif->m_status = 0;
            ccif->m_rx_idx = 0;
            ccif->m_md_id = md_id; //info->md_id;
            spin_lock_init(&ccif->m_lock);
            ccif->register_call_back_func = __ccif_v1_register_call_back;
            ccif->register_isr_notify_func = __ccif_v1_register_isr_notify;
            ccif->ccif_init = __ccif_v1_init;
            ccif->ccif_de_init = __ccif_v1_de_init;
            ccif->ccif_register_intr = __ccif_v1_reg_intr;
            ccif->ccif_en_intr = __ccif_v1_en_intr;
            ccif->ccif_dis_intr = __ccif_v1_dis_intr;
            ccif->ccif_dump_reg = __ccif_v1_dump_reg;
            ccif->ccif_read_phy_ch_data = __ccif_v1_read_phy_ch_data;
            ccif->ccif_write_phy_ch_data = __ccif_v1_write_phy_ch_data;
            ccif->ccif_get_rx_ch = __ccif_v1_get_rx_ch;
            ccif->ccif_get_busy_state = __ccif_v1_get_busy_state;
            ccif->ccif_set_busy_state = __ccif_v1_set_busy_state;
            ccif->ccif_ack_phy_ch = __ccif_v1_ack;
            ccif->ccif_clear_sram = __ccif_v1_clear_sram;
            ccif->ccif_write_runtime_data = __ccif_v1_write_runtime_data;
            ccif->ccif_intr_handler = __ccif_v1_intr_handler;
            ccif->ccif_reset = __ccif_v1_reset;
            ccif->m_logic_ctl_block = ctl_b;
            ccif->m_irq_dis_cnt = 0;
            return ccif;
            
        case CCIF_VIR:
        default:
            CCCI_MSG_INF(md_id, "cci", "%s: [error]invalid ccif type(%d)\n", __FUNCTION__, info->type);
            kfree(ccif);
            return NULL;
    }
}


