/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccmni.c
 *
 * Project:
 * --------
 *   YuSu
 *
 * Description:
 * ------------
 *   MT6516 Cross Chip Modem Network Interface
 *
 * Author:
 * -------
 *   TL Lau (mtk02008)
 *
 ****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/bitops.h>
#include <linux/wakelock.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <asm/dma-mapping.h>
#include <asm/bitops.h>
#include <linux/timer.h>
#include <mach/mt_typedefs.h>
#include <ccmni_pfp.h>
#include <ccci_tty.h>
#include <ccci.h>

#define  CCMNI_TX_QUEUE         8
#define  CCMNI_UART_OFFSET      2

extern void *ccmni_ctl_block[];

typedef struct
{
    int                    channel;
    int                    m_md_id;
    int                    uart_rx;
    int                 uart_rx_ack;
    int                    uart_tx;
    int                    uart_tx_ack;
    int                    ready;
    int                    net_if_off;
    unsigned long        flags;
    struct timer_list    timer;
    unsigned long        send_len;
    struct net_device    *dev;
    struct wake_lock    wake_lock;
    spinlock_t            spinlock;

    shared_mem_tty_t    *shared_mem;
    int                    shared_mem_phys_addr;

    unsigned char        write_buffer [CCMNI_MTU + 4];
    unsigned char        read_buffer  [CCCI1_CCMNI_BUF_SIZE];
    unsigned char        decode_buffer[CCCI1_CCMNI_BUF_SIZE];

    unsigned char        mac_addr     [ETH_ALEN];

    struct tasklet_struct    tasklet;
    void                *owner;
    
} ccmni_instance_t;  

typedef struct _ccmni_v1_ctl_block
{
    int                    m_md_id;
    int                    ccci_is_ready;
    ccmni_instance_t    *ccmni_instance[CCMNI_V1_PORT_NUM];
    struct wake_lock    ccmni_wake_lock;
    char                wakelock_name[16];
    MD_CALL_BACK_QUEUE    ccmni_notifier;
}ccmni_v1_ctl_block_t;

static void ccmni_read        (unsigned long arg);
//static DECLARE_TASKLET        (ccmni_read_tasklet, ccmni_read, 0);

static void reset_ccmni_instance_buffer(ccmni_instance_t *ccmni_instance)
{
    unsigned long flags;
    spin_lock_irqsave(&ccmni_instance->spinlock, flags);
    ccci_reset_buffers(ccmni_instance->shared_mem, CCCI1_CCMNI_BUF_SIZE);
    spin_unlock_irqrestore(&ccmni_instance->spinlock, flags);
}

int ccmni_v1_ipo_h_restore(int md_id)
{
    int i;
    ccmni_v1_ctl_block_t    *ctlb;

    ctlb = ccmni_ctl_block[md_id];
    for(i=0; i<CCMNI_V1_PORT_NUM; i++)
        ccci_reset_buffers(ctlb->ccmni_instance[i]->shared_mem, CCCI1_CCMNI_BUF_SIZE);

    return 0;
}

static void restore_ccmni_instance(ccmni_instance_t *ccmni_instance)
{
    unsigned long flags;
    spin_lock_irqsave(&ccmni_instance->spinlock, flags);
    if(ccmni_instance->net_if_off) {
        ccmni_instance->net_if_off = 0;
        netif_carrier_on(ccmni_instance->dev);
    }
    spin_unlock_irqrestore(&ccmni_instance->spinlock, flags);
}

static void stop_ccmni_instance(ccmni_instance_t *ccmni_instance)
{
    unsigned long flags;
    spin_lock_irqsave(&ccmni_instance->spinlock, flags);
    if(ccmni_instance->net_if_off == 0) {
        ccmni_instance->net_if_off = 1;
        del_timer(&ccmni_instance->timer);
        netif_carrier_off(ccmni_instance->dev);
    }
    spin_unlock_irqrestore(&ccmni_instance->spinlock, flags);
}


static void ccmni_notifier_call(MD_CALL_BACK_QUEUE *notifier, unsigned long val)
{
    int                        i;
    ccmni_v1_ctl_block_t    *ctl_b = container_of(notifier, ccmni_v1_ctl_block_t, 
                                            ccmni_notifier);
    ccmni_instance_t        *instance;

    switch(val)
    {
        case CCCI_MD_EXCEPTION :
            ctl_b->ccci_is_ready=0;
            for(i=0;i<CCMNI_V1_PORT_NUM;i++)
            {
                instance = ctl_b->ccmni_instance[i];
                if (instance)  
                    stop_ccmni_instance(instance);
            }
            break;
        case CCCI_MD_STOP:
            for(i=0;i<CCMNI_V1_PORT_NUM;i++)
            {
                instance = ctl_b->ccmni_instance[i];
                if (instance) { 
                    stop_ccmni_instance(instance);
                }
            }
            break;
        case CCCI_MD_RESET     :
            ctl_b->ccci_is_ready=0;
                for(i=0;i<CCMNI_V1_PORT_NUM;i++)
                {
                    instance = ctl_b->ccmni_instance[i];
                    if (instance) { 
                        reset_ccmni_instance_buffer(instance);
                    }
                }
            break;

        case CCCI_MD_BOOTUP:
            if (ctl_b->ccci_is_ready==0)
            {
                ctl_b->ccci_is_ready=1;
                for(i=0;i<CCMNI_V1_PORT_NUM;i++)
                {
                    instance = ctl_b->ccmni_instance[i];
                    if (instance) 
                        restore_ccmni_instance(instance);
                }
            }
            break;

        default:
            break;
    }

    return ;
}


static void timer_func(unsigned long data)
{
    ccmni_instance_t        *ccmni=(ccmni_instance_t *)data;
    int                        contin=0;
    int                        ret=0;
    ccci_msg_t                msg;
    ccmni_v1_ctl_block_t    *ctl_b = (ccmni_v1_ctl_block_t*)ccmni->owner;
    int                        md_id = ctl_b->m_md_id;
    spin_lock_bh(&ccmni->spinlock);

    if (ctl_b->ccci_is_ready == 0)  
        goto out;

    if (test_bit(CCMNI_RECV_ACK_PENDING,&ccmni->flags))
    {
        msg.magic = 0;
        msg.id = CCMNI_CHANNEL_OFFSET + ccmni->channel;
        msg.channel = ccmni->uart_rx_ack;
        msg.reserved = 0;
        ret = ccci_message_send(md_id, &msg, 1);

        if (ret==-CCCI_ERR_CCIF_NO_PHYSICAL_CHANNEL)
            contin=1;
        else 
            clear_bit(CCMNI_RECV_ACK_PENDING, &ccmni->flags);
    }

    if (test_bit(CCMNI_SEND_PENDING,&ccmni->flags))
    {
        msg.addr = 0;
        msg.len = ccmni->send_len;
        msg.channel = ccmni->uart_tx;
        msg.reserved = 0;
        ret = ccci_message_send(md_id, &msg, 1);

        if (ret==-CCCI_ERR_CCIF_NO_PHYSICAL_CHANNEL)
            contin=1;
        else {
            clear_bit(CCMNI_SEND_PENDING,&ccmni->flags);
            ccmni->send_len=0;
        }        
    }
out:
    spin_unlock_bh(&ccmni->spinlock);
    if (contin)
        mod_timer(&ccmni->timer,jiffies+2);    

    return;

}

static void ccmni_make_etherframe(void *_eth_hdr, u8 *mac_addr, int packet_type)
{
    struct ethhdr *eth_hdr = _eth_hdr;

    memcpy(eth_hdr->h_dest,   mac_addr, sizeof(eth_hdr->h_dest));
    memset(eth_hdr->h_source, 0, sizeof(eth_hdr->h_source));
    if(packet_type == IPV6_VERSION){
        eth_hdr->h_proto = __constant_cpu_to_be16(ETH_P_IPV6);
    }else{
        eth_hdr->h_proto = __constant_cpu_to_be16(ETH_P_IP);
    }
}

static int ccmni_receive(ccmni_instance_t *ccmni, int length)
{
    int                        counter, ret;
    packet_info_t            packet_info;
    complete_ippkt_t        *packet;
    complete_ippkt_t        *processed_packet;
    struct sk_buff            *skb;
    complete_ippkt_t        last_packet = {0};
    int                        offset_put_pkt = 0;
    int                        offset_parse_frame = 0; 
    int                        packet_type;
    ccmni_v1_ctl_block_t    *ctl_b = (ccmni_v1_ctl_block_t*)ccmni->owner;
    int                        md_id = ctl_b->m_md_id;

    CCCI_CCMNI_MSG(md_id, "CCMNI%d_receive() invoke pfp_unframe()\n", ccmni->channel);
    do
    {
        packet_info = pfp_unframe(ccmni->decode_buffer+offset_put_pkt, \
                    CCCI1_CCMNI_BUF_SIZE-offset_put_pkt, ccmni->read_buffer+offset_parse_frame, \
                    length, ccmni->channel);
        packet = packet_info.pkt_list;
        
        CCCI_CCMNI_MSG(md_id, "CCMNI%d num_complete_pkt=%d after pfp_unframe \n", \
            ccmni->channel, packet_info.num_complete_packets);
        
        for(counter = 0; counter < packet_info.num_complete_packets; counter++)
        {
            skb = dev_alloc_skb(packet->pkt_size);
            if (skb)
            {
                packet_type = packet->pkt_data[0] & 0xF0;
                memcpy(skb_put(skb, packet->pkt_size), packet->pkt_data, packet->pkt_size);
                ccmni_make_etherframe(skb->data - ETH_HLEN, ccmni->dev->dev_addr, packet_type);
                skb_set_mac_header(skb, -ETH_HLEN);
                skb->dev = ccmni->dev;
                if(packet_type == IPV6_VERSION){            
                    skb->protocol  = htons(ETH_P_IPV6);
                }
                else {
                    skb->protocol  = htons(ETH_P_IP);
                }
                skb->ip_summed = CHECKSUM_NONE;

                ret = netif_rx(skb);

                CCCI_CCMNI_MSG(md_id, "CCMNI%d invoke netif_rx()=%d\n", ccmni->channel, ret);
                ccmni->dev->stats.rx_packets++;
                ccmni->dev->stats.rx_bytes  += packet->pkt_size;
                CCCI_CCMNI_MSG(md_id, "CCMNI%d rx_pkts=%ld, stats_rx_bytes=%ld\n", ccmni->channel, \
                    ccmni->dev->stats.rx_packets, ccmni->dev->stats.rx_bytes);
            }
            else
            {
                CCCI_DBG_MSG(md_id, "net", "CCMNI%d Socket buffer allocate fail\n", ccmni->channel);
            }

            processed_packet = packet;
            last_packet = *processed_packet;
            packet = packet->next;

            /* Only clear the entry_used flag as 0 */
            release_one_used_complete_ippkt_entry(processed_packet);
        };

        /* It must to check if it is necessary to invoke the pfp_unframe() again due to no available complete_ippkt entry */
        if (packet_info.try_decode_again == 1)
        {
            offset_put_pkt += (last_packet.pkt_data - ccmni->decode_buffer + last_packet.pkt_size);
            offset_parse_frame += packet_info.consumed_length;
        }
    } while (packet_info.try_decode_again == 1); 

    offset_parse_frame += packet_info.consumed_length;
    return offset_parse_frame;
}

static void ccmni_read(unsigned long arg)
{
    int                        part, size;
    int                        ret;
    int                        read, write, consumed;
    unsigned char            *string;
    ccmni_instance_t        *ccmni = (ccmni_instance_t *) arg;
    ccci_msg_t                msg;
    ccmni_v1_ctl_block_t    *ctl_b = (ccmni_v1_ctl_block_t*)ccmni->owner;
    int                        md_id= ctl_b->m_md_id;
    char                    *rx_buffer;

    spin_lock_bh(&ccmni->spinlock);
    if (ctl_b->ccci_is_ready==0)  
    {
        CCCI_DBG_MSG(md_id, "net", "CCMNI%d_read fail when modem not ready\n", ccmni->channel);
        goto out;
    }

    string = ccmni->read_buffer;
    read   = ccmni->shared_mem->rx_control.read;
    write  = ccmni->shared_mem->rx_control.write; 
    size   = write - read;
    part   = 0;
    rx_buffer = ccmni->shared_mem->buffer;

    if (size < 0)
    {
        size += ccmni->shared_mem->rx_control.length;
    }

    if (read > write)
    {
        part = ccmni->shared_mem->rx_control.length - read;       
        memcpy(string, &rx_buffer[read], part);

        size   -= part;
        string += part;
        read    = 0;
    }

    memcpy(string, &rx_buffer[read], size);
    CCCI_CCMNI_MSG(md_id, "CCMNI%d_receive[Before]: size=%d, read=%d\n", \
                            ccmni->channel, (size+part), read);
    consumed = ccmni_receive(ccmni, size + part);
    CCCI_CCMNI_MSG(md_id, "CCMNI%d_receive[After]: consume=%d\n", ccmni->channel, consumed);

    //  Calculate the new position of the read pointer.
    //  Take into consideration the number of bytes actually consumed;
    //  i.e. number of bytes taken up by complete IP packets.   
    read += size;
    if (read >= ccmni->shared_mem->rx_control.length)
    {
        read -= ccmni->shared_mem->rx_control.length;
    }

    if (consumed < (size + part))
    {
        read -= ((size + part) - consumed);
        if (read < 0)
        {
            read += ccmni->shared_mem->rx_control.length;
        }
    }

    ccmni->shared_mem->rx_control.read = read;
    //  Send an acknowledgement back to modem side.
    CCCI_CCMNI_MSG(md_id, "CCMNI%d_read to write mailbox(ch%d, tty%d)\n", ccmni->channel,
            ccmni->uart_rx_ack, CCMNI_CHANNEL_OFFSET + ccmni->channel);
    //ret = ccci_write_mailbox(ccmni->uart_rx_ack, CCMNI_CHANNEL_OFFSET + ccmni->channel); 
    msg.magic = 0xFFFFFFFF;
    msg.id = CCMNI_CHANNEL_OFFSET + ccmni->channel;
    msg.channel = ccmni->uart_rx_ack;
    msg.reserved = 0;
    ret = ccci_message_send(md_id, &msg, 1);
    if (ret==-CCCI_ERR_CCIF_NO_PHYSICAL_CHANNEL)    
    {
        set_bit(CCMNI_RECV_ACK_PENDING,&ccmni->flags);
        mod_timer(&ccmni->timer,jiffies);
    }
    else if (ret==sizeof(ccci_msg_t))
        clear_bit(CCMNI_RECV_ACK_PENDING,&ccmni->flags);
out:
    spin_unlock_bh(&ccmni->spinlock);

    CCCI_CCMNI_MSG(md_id, "CCMNI%d_read invoke wake_lock_timeout(1s)\n", ccmni->channel);
    wake_lock_timeout(&ctl_b->ccmni_wake_lock, HZ);

    return;
}


//  will be called when modem sends us something.
//  we will then copy it to the tty's buffer.
//  this is essentially the "read" fops.
static void ccmni_callback(void *private)
{
    logic_channel_info_t *ch_info = (logic_channel_info_t*)private;
    ccmni_instance_t  *ccmni = (ccmni_instance_t *)(ch_info->m_owner);
    ccci_msg_t msg;

    while(get_logic_ch_data(ch_info, &msg)){
        switch(msg.channel)
        {
            case CCCI_CCMNI1_TX_ACK:
            case CCCI_CCMNI2_TX_ACK:
            case CCCI_CCMNI3_TX_ACK:
                // this should be in an interrupt,
                // so no locking required...
                ccmni->ready = 1;
                netif_wake_queue(ccmni->dev);
                break;

            case CCCI_CCMNI1_RX:
            case CCCI_CCMNI2_RX:
            case CCCI_CCMNI3_RX:
                //ccmni_read_tasklet2.data = (unsigned long) private_data;
                //tasklet_schedule(&ccmni_read_tasklet);
                tasklet_schedule(&ccmni->tasklet);
                break;

            default:
                break;
        }
    }
}


static void ccmni_write(ccmni_instance_t *ccmni, frame_info_t *frame_info)
{
    int                        size, over, total;
    int                        ret;
    unsigned                read, write, length, len;
    unsigned                tmp_write;
    unsigned char            *ptr;
    ccci_msg_t                msg;
    char                    *tx_buffer;
    ccmni_v1_ctl_block_t    *ctl_b = (ccmni_v1_ctl_block_t*)ccmni->owner;
    int                        md_id = ctl_b->m_md_id;

    size = 0;
    ptr  = (unsigned char *) frame_info->frame_list[0].frame_data;
    len  =                   frame_info->frame_list[0].frame_size;


    read   = ccmni->shared_mem->tx_control.read;
    write  = ccmni->shared_mem->tx_control.write;
    length = ccmni->shared_mem->tx_control.length;
    over   = length - write;
    tx_buffer = ccmni->shared_mem->buffer + length;

    if (read == write)
    {
        size = length;
    }
    else if (read < write)
    {
        size  = length -  write;
        size += read;
    }
    else
    {
        size = read - write;
    }

    if (len > size)
    {
        len   = size;
        total = size;
    }

    total = len;

    if (over < len)
    {
        memcpy(&tx_buffer[write], (void *) ptr, over);
        len   -= over;
        ptr   += over;
        write  = 0;
    }

    memcpy(&tx_buffer[write], (void *) ptr, len);
    mb();
    tmp_write = write + len;
    if (tmp_write >= length)
    {
        tmp_write -= length;
    }
    ccmni->shared_mem->tx_control.write = tmp_write;

    // ccmni->ready = 0;
    len = total;
    msg.addr = 0;
    msg.len = len;
    msg.channel = ccmni->uart_tx;
    msg.reserved = 0;

    ret = ccci_message_send(md_id, &msg, 1);
    if (ret==-CCCI_ERR_CCIF_NO_PHYSICAL_CHANNEL)
    {
        set_bit(CCMNI_SEND_PENDING,&ccmni->flags);
        ccmni->send_len +=len;
        mod_timer(&ccmni->timer,jiffies);
    }
    else if (ret==sizeof(ccci_msg_t))
        clear_bit(CCMNI_SEND_PENDING,&ccmni->flags);

    return;
}


//  The function start_xmit is called when there is one packet to transmit.
static int ccmni_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    int                        ret = NETDEV_TX_OK;
    int                        size;
    unsigned int            read, write, length;
    frame_info_t            frame_info;
    ccmni_instance_t        *ccmni = netdev_priv(dev);
    ccmni_v1_ctl_block_t    *ctl_b = (ccmni_v1_ctl_block_t*)ccmni->owner;
    int                        md_id = ctl_b->m_md_id;
   
    spin_lock_bh(&ccmni->spinlock);

    if (ctl_b->ccci_is_ready==0) 
    {
        CCCI_DBG_MSG(md_id, "net", "CCMNI%d transfer data fail when modem not ready \n", ccmni->channel);
        ret = NETDEV_TX_BUSY;
        goto _ccmni_start_xmit_busy;
    }

    read   = ccmni->shared_mem->tx_control.read;
    write  = ccmni->shared_mem->tx_control.write;
    length = ccmni->shared_mem->tx_control.length;
    size   = read - write;

    CCCI_CCMNI_MSG(md_id, "CCMNI%d_start_xmit: skb_len=%d, size=%d, ccmni_ready=%d \n", \
                        ccmni->channel, skb->len, size, ccmni->ready);

    if (size <= 0)
    {
        size += length;
    }

    if (skb->len > CCMNI_MTU)
    {
        //  Sanity check; this should not happen!
        //  Digest and return OK.
        CCCI_DBG_MSG(md_id, "net", "CCMNI%d packet size exceed 1500 bytes: size=%d \n", \
            ccmni->channel, skb->len);
        dev->stats.tx_dropped++;
        goto _ccmni_start_xmit_exit;
    }

    if(size >= 1)
        size-=1;
    else
        CCCI_DBG_MSG(md_id, "net", "CCMNI%d size is Zero(1) \n", ccmni->channel);

    if (size < (skb->len + 4))
    {
        //  The TX buffer is full, or its not ready yet,
        //  we should stop the net queue for the moment.
        CCCI_DBG_MSG(md_id, "net", "CCMNI%d TX busy and stop queue: size=%d, skb->len=%d \n", \
            ccmni->channel, size, skb->len);
        CCCI_DBG_MSG(md_id, "net", "       TX read = %d  write = %d\n", \
            ccmni->shared_mem->tx_control.read, ccmni->shared_mem->tx_control.write); 
        CCCI_DBG_MSG(md_id, "net", "       RX read = %d  write = %d\n", \
            ccmni->shared_mem->rx_control.read, ccmni->shared_mem->rx_control.write);

        netif_stop_queue(ccmni->dev);

        //  Set CCMNI ready to ZERO, and wait for the ACK from modem side.
        ccmni->ready = 0;
        ret          = NETDEV_TX_BUSY;

        goto _ccmni_start_xmit_busy;
    }

    frame_info = pfp_frame(ccmni->write_buffer, skb->data, skb->len, FRAME_START, ccmni->channel);
    ccmni_write (ccmni, &frame_info);

    dev->stats.tx_packets++;
    dev->stats.tx_bytes  += skb->len;

_ccmni_start_xmit_exit:
    dev_kfree_skb(skb);

_ccmni_start_xmit_busy:
    spin_unlock_bh(&ccmni->spinlock);

    return ret;
}


static int ccmni_open(struct net_device *dev)
{
    ccmni_instance_t        *ccmni = netdev_priv(dev);
    ccmni_v1_ctl_block_t    *ctl_b = (ccmni_v1_ctl_block_t*)ccmni->owner;
    int                        md_id= ctl_b->m_md_id;

    CCCI_MSG_INF(md_id, "net", "CCMNI%d open \n", ccmni->channel); 
    if (ctl_b->ccci_is_ready == 0) {    
        CCCI_MSG_INF(md_id, "net", "CCMNI%d open fail when modem not ready \n", ccmni->channel);
        return -EIO;
    }
    netif_start_queue(dev);
    return 0;
}

static int ccmni_close(struct net_device *dev)
{
    ccmni_instance_t        *ccmni = netdev_priv(dev);
    ccmni_v1_ctl_block_t    *ctl_b = (ccmni_v1_ctl_block_t*)ccmni->owner;

    CCCI_MSG_INF(ctl_b->m_md_id, "net", "CCMNI%d close \n", ccmni->channel); 
    netif_stop_queue(dev);
    return 0;
}

static int ccmni_net_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
    //  No implementation at this moment.
    //  This is a place holder.

    return 0;
}


static void ccmni_tx_timeout(struct net_device *dev)
{
    //  No implementation at this moment.
    //  This is a place holder.

    dev->stats.tx_errors++;
    netif_wake_queue(dev);   
}


static const struct net_device_ops ccmni_netdev_ops = 
{
    .ndo_open        = ccmni_open,
    .ndo_stop        = ccmni_close,
    .ndo_start_xmit    = ccmni_start_xmit,
    .ndo_do_ioctl    = ccmni_net_ioctl,
    .ndo_tx_timeout    = ccmni_tx_timeout,
};

extern int is_mac_addr_duplicate(char *mac);
static void ccmni_setup(struct net_device *dev)
{
    int    retry = 10;

    ether_setup(dev);

    dev->header_ops        = NULL;
    dev->netdev_ops        = &ccmni_netdev_ops;
    dev->flags             = IFF_NOARP & (~IFF_BROADCAST & ~IFF_MULTICAST);
    dev->mtu               = CCMNI_MTU;
    dev->tx_queue_len      = CCMNI_TX_QUEUE;
    dev->addr_len          = ETH_ALEN;
    dev->destructor        = free_netdev;

    while(retry-->0){
        random_ether_addr((u8 *) dev->dev_addr);
        if(is_mac_addr_duplicate((u8*)dev->dev_addr))
            continue;
        else
            break;
    }

    return;
}

static int ccmni_create_instance(int md_id, int channel)
{
    int                        ret, size;
    int                        uart_rx, uart_rx_ack;
    int                        uart_tx, uart_tx_ack;
    ccmni_instance_t        *ccmni;
    struct net_device        *dev = NULL;
    ccmni_v1_ctl_block_t    *ctl_b = (ccmni_v1_ctl_block_t *)ccmni_ctl_block[md_id];

    //  Network device creation and registration.
    dev = alloc_netdev(sizeof(ccmni_instance_t), "", ccmni_setup);
    if (dev == NULL)
    {
        CCCI_MSG_INF(md_id, "net", "CCMNI%d allocate netdev fail!\n", channel); 
        return -ENOMEM;
    }

    ccmni          = netdev_priv(dev);
    ccmni->dev     = dev;
    ccmni->channel = channel;
    ccmni->owner   = ccmni_ctl_block[md_id];

    if(md_id == MD_SYS1) {
        sprintf(dev->name, "ccmni%d", channel);
    } else {
        sprintf(dev->name, "cc%dmni%d", md_id+1, channel);
        //sprintf(dev->name, "ccmni%d", channel);
    }

    ret = register_netdev(dev);
    if (ret != 0)
    {
        CCCI_MSG_INF(md_id, "net", "CCMNI%d register netdev fail: %d\n", ccmni->channel, ret);        
        goto _ccmni_create_instance_exit;
    }

    //  CCCI channel registration.
    ASSERT(ccci_uart_base_req(md_id, CCMNI_UART_OFFSET + ccmni->channel, (int*)&ccmni->shared_mem,
                                &ccmni->shared_mem_phys_addr, &size) == CCCI_SUCCESS);

    if (ccmni->shared_mem == NULL)
    {
        CCCI_MSG_INF(md_id, "net", "CCMNI%d allocate memory fail\n", ccmni->channel);
        unregister_netdev(dev);        
        ret = -ENOMEM;
        goto _ccmni_create_instance_exit;
    }

    CCCI_CCMNI_MSG(md_id, "0x%08X:0x%08X:%d\n", (unsigned int)ccmni->shared_mem, \
                    (unsigned int)ccmni->shared_mem_phys_addr, size);

    ccmni->shared_mem->tx_control.length = CCCI1_CCMNI_BUF_SIZE;
    ccmni->shared_mem->tx_control.read   = 0;
    ccmni->shared_mem->tx_control.write  = 0;

    ccmni->shared_mem->rx_control.length = CCCI1_CCMNI_BUF_SIZE;
    ccmni->shared_mem->rx_control.read   = 0;
    ccmni->shared_mem->rx_control.write  = 0;

    switch(ccmni->channel)
    {
        case 0:
            uart_rx     = CCCI_CCMNI1_RX;
            uart_rx_ack = CCCI_CCMNI1_RX_ACK;
            uart_tx     = CCCI_CCMNI1_TX;
            uart_tx_ack = CCCI_CCMNI1_TX_ACK;
            break;            

        case 1:
            uart_rx     = CCCI_CCMNI2_RX;
            uart_rx_ack = CCCI_CCMNI2_RX_ACK;
            uart_tx     = CCCI_CCMNI2_TX;
            uart_tx_ack = CCCI_CCMNI2_TX_ACK;
            break;            

        case 2:
            uart_rx     = CCCI_CCMNI3_RX;
            uart_rx_ack = CCCI_CCMNI3_RX_ACK;
            uart_tx     = CCCI_CCMNI3_TX;
            uart_tx_ack = CCCI_CCMNI3_TX_ACK;
            break;            

        default:
            CCCI_MSG_INF(md_id, "net", "[Error]CCMNI%d Invalid ccmni number\n", ccmni->channel);
            unregister_netdev(dev);
            ret = -ENOSYS;
            goto _ccmni_create_instance_exit;
    }
    ccmni->m_md_id = md_id;

    ccmni->uart_rx      = uart_rx;
    ccmni->uart_rx_ack  = uart_rx_ack;
    ccmni->uart_tx      = uart_tx;
    ccmni->uart_tx_ack  = uart_tx_ack;

    // Register this ccmni instance to the ccci driver.
    // pass it the notification handler.
    ASSERT(register_to_logic_ch(md_id, uart_rx,     ccmni_callback, (void *) ccmni) == 0);
    ASSERT(register_to_logic_ch(md_id, uart_tx_ack, ccmni_callback, (void *) ccmni) == 0);

    // Initialize the spinlock.
    spin_lock_init(&ccmni->spinlock);
    setup_timer(&ccmni->timer,timer_func,(unsigned long)ccmni);

    // Initialize the tasklet.
    tasklet_init(&ccmni->tasklet, ccmni_read, (unsigned long)ccmni);

    ctl_b->ccmni_instance[channel] = ccmni;
    ccmni->ready = 1;
    ccmni->net_if_off = 0;
    return ret;

_ccmni_create_instance_exit:
    free_netdev(dev);
    kfree(ccmni);
    ctl_b->ccmni_instance[channel] = NULL;
    return ret;
}


static void ccmni_destroy_instance(int md_id, int channel)
{
    ccmni_v1_ctl_block_t *ctl_b = (ccmni_v1_ctl_block_t *)ccmni_ctl_block[md_id];
    ccmni_instance_t *ccmni = ctl_b->ccmni_instance[channel];

    if (ccmni != NULL)
    {
        ccmni->ready = 0;
        un_register_to_logic_ch(md_id, ccmni->uart_rx);
        un_register_to_logic_ch(md_id, ccmni->uart_tx_ack);

        if (ccmni->shared_mem != NULL)    {
            ccmni->shared_mem           = NULL;
            ccmni->shared_mem_phys_addr = 0;
        }

        if(ccmni->dev != NULL) {
            unregister_netdev(ccmni->dev);
            //free_netdev(ccmni->dev);
        }
        //tasklet_kill(&ccmni->tasklet);
        ctl_b->ccmni_instance[channel] = NULL;
    }
}


int ccmni_v1_init(int md_id)
{
    int                        count, ret, curr;
    ccmni_v1_ctl_block_t    *ctl_b;

    // Create control block structure
    ctl_b = (ccmni_v1_ctl_block_t *)kmalloc(sizeof(ccmni_v1_ctl_block_t), GFP_KERNEL);
    if(ctl_b == NULL)
        return -CCCI_ERR_GET_MEM_FAIL;

    memset(ctl_b, 0, sizeof(ccmni_v1_ctl_block_t));
    ccmni_ctl_block[md_id] = ctl_b;

    // Init ctl_b
    ctl_b->m_md_id = md_id;
    ctl_b->ccmni_notifier.call = ccmni_notifier_call;
    ctl_b->ccmni_notifier.next = NULL;

    for(count = 0; count < CCMNI_V1_PORT_NUM; count++)
    {
        ret = ccmni_create_instance(md_id, count);
        if (ret != 0) {
            CCCI_MSG_INF(md_id, "net", "CCMNI%d create instance fail: %d\n", count, ret);
            goto _CCMNI_INSTANCE_CREATE_FAIL;
        } else {
            //CCCI_MSG_INF(md_id, "net", "CCMNI%d create instance ok!\n", count);
        }
    }

    ret=md_register_call_chain(md_id ,&ctl_b->ccmni_notifier);
    if(ret) {
        CCCI_MSG_INF(md_id, "net", "md_register_call_chain fail: %d\n", ret);
        goto _CCMNI_INSTANCE_CREATE_FAIL;
    }

    snprintf(ctl_b->wakelock_name, sizeof(ctl_b->wakelock_name), "ccci%d_net_v1", (md_id+1));   
    wake_lock_init(&ctl_b->ccmni_wake_lock, WAKE_LOCK_SUSPEND, ctl_b->wakelock_name);
    
    return ret;

_CCMNI_INSTANCE_CREATE_FAIL:
    for(curr=0; curr<=count; curr++) {
        ccmni_destroy_instance(md_id, curr);
    }
    kfree(ctl_b);
    ccmni_ctl_block[md_id] = NULL;
    return ret;
}


void ccmni_v1_exit(int md_id)
{
    int                        count;
    ccmni_v1_ctl_block_t    *ctl_b = (ccmni_v1_ctl_block_t *)ccmni_ctl_block[md_id];

    if (ctl_b) {
        for(count = 0; count < CCMNI_V1_PORT_NUM; count++)
            ccmni_destroy_instance(md_id, count);

        md_unregister_call_chain(md_id, &ctl_b->ccmni_notifier);
        wake_lock_destroy(&ctl_b->ccmni_wake_lock);
        kfree(ctl_b);
        ccmni_ctl_block[md_id] = NULL;
    }

    return;
}


