#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/ipv6.h>
#include <linux/slab.h>
#include <net/checksum.h>
#include <net/ip6_checksum.h>

#include <linux/mii.h>
#include "eemcs_debug.h"
#include "eemcs_kal.h"
#include "eccmni.h"
#include "eemcs_ccci.h"
#include "eemcs_state.h"

#define ECCMNI_DEBUG_ENABLE (0)
enum {
	/* netdev interface */	
	/*
	 * Out of NWG spec (R1_v1.2.2), 3.3.3 ASN Bearer Plane MTU Size
	 * The MTU is 1400 or less
 	 */
//20121227 #ifdef _ECCMNI_LB_UT_
#if 0
	ECCMNI_DEF_MTU = 1600,
	ECCMNI_MAX_MTU = 1600,
#else
	ECCMNI_DEF_MTU = 1500,
	ECCMNI_MAX_MTU = 1500,
#endif
	ECCMNI_TX_TIMEOUT = 1*HZ, /* 1sec tx timeout */
	ECCMNI_TX_QLEN = 1000,
};

enum ECCMNI_ID{
    ECCMNI0_ID = 0, /* eccmni0 */
    ECCMNI1_ID,     /* eccmni1 */
    ECCMNI2_ID,     /* eccmni2 */
    ECCMNI_MAX_ID = ECCMNI_MAX_DEV
};

static eccmni_inst_t eccmni_inst[ECCMNI_MAX_DEV];
static KAL_UINT8 eccmni_port_mapping[ECCMNI_MAX_DEV]={
    CCCI_PORT_NET1, /* eccmni0 */
    CCCI_PORT_NET2, /* eccmni1 */
    CCCI_PORT_NET3  /* eccmni2 */
};

#ifdef _ECCMNI_SKB_DBG_
static void eccmni_dbg_skb(struct sk_buff *skb){
    DBGLOG(NETD,DBG,"[SKB_DUMP] head(0x%x) end(0x%x) data(0x%x) tail(0x%x) len(0x%x) data_len(0x%x)",\
        (unsigned int)skb->head, (unsigned int)skb->end, \
        (unsigned int)skb->data, (unsigned int)skb->tail,\
        (unsigned int)skb->len,  (unsigned int)skb->data_len);
}
static void eccmni_dbg_skb_addr(struct sk_buff *skb, int idx){

    eccmni_dbg_skb(skb);
    DBGLOG(NETD,DBG, "[SKBSKB](%d) addr(0x%x) size(%d)", idx, (unsigned int)skb->data, skb->len);
    DBGLOG(NETD,DBG, "[SKBSKB](%d) L2 addr(0x%x) L3 addr(0x%x) L4 addr(0x%x)", idx, (unsigned int)skb_mac_header(skb), \
        (unsigned int)skb_network_header(skb), (unsigned int)skb_transport_header(skb));

}
static void eccmni_dbg_eth_header(struct ethhdr *ethh){
    DBGLOG(NETD,DBG, "[NETD_DUMP] L2 header addr(0x%x) size(%d)", (unsigned int)ethh, sizeof(struct ethhdr));
    DBGLOG(NETD,DBG, "[NETD_DUMP] L2 ethhdr: dest_mac %02x:%02x:%02x:%02x:%02x:%02x",\
        ethh->h_dest[0], ethh->h_dest[1], ethh->h_dest[2], ethh->h_dest[3], ethh->h_dest[4], ethh->h_dest[5]);
    DBGLOG(NETD,DBG, "[NETD_DUMP] L2 ethhdr: src_mac %02x:%02x:%02x:%02x:%02x:%02x",\
        ethh->h_source[0], ethh->h_source[1], ethh->h_source[2], ethh->h_source[3], ethh->h_source[4], ethh->h_source[5]);
    DBGLOG(NETD,DBG, "[NETD_DUMP] L2 ethhdr: (0x%04x)", ethh->h_proto);
}
static void eccmni_dbg_ip_header(struct iphdr *iph){
    DBGLOG(NETD,DBG, "[NETD_DUMP] L3 header addr(0x%x) size(%d)", (unsigned int)iph, iph->ihl*4);
    DBGLOG(NETD,DBG, "[NETD_DUMP] L3 IP package data: ihl(0x%02x) version(0x%02x) tos(0x%02x) tot_len(0x%04x)",\
        iph->ihl, iph->version, iph->tos, iph->tot_len);
    DBGLOG(NETD,DBG, "[NETD_DUMP] L3 IP package data: id(0x%04x) frag_off(0x%04x) ttl(0x%02x) protocol(0x%02x) check(0x%04x)",\
        iph->id, iph->frag_off, iph->ttl, iph->protocol, iph->check);
    DBGLOG(NETD,DBG, "[NETD_DUMP] L3 IP package data: saddr(0x%x) daddr(0x%x)", iph->saddr, iph->daddr);
}
static void eccmni_dbg_skb_header(struct sk_buff *skb){
    struct ethhdr *ethh;
    struct iphdr  *iph;

    ethh = (struct ethhdr *)skb->data;
    if(NULL == ethh){return;}
    eccmni_dbg_eth_header(ethh);

    iph = ip_hdr(skb);
    if(NULL == iph){return;}
    eccmni_dbg_ip_header(iph);

    switch (iph->protocol) {
        case IPPROTO_UDP:
            {
                struct udphdr *udph;
                udph = (struct udphdr *)skb_transport_header(skb);
                DBGLOG(NETD,DBG, "[NETD_DUMP] L4 header addr(0x%x) size(%d)", (unsigned int)udph, sizeof(struct udphdr));
                DBGLOG(NETD,DBG, "[NETD_DUMP] L4 UDP source: (%d), dest: (%d), len (%d), check (0x%04x)",\
                        ntohs(udph->source), ntohs(udph->dest), ntohs(udph->len), udph->check);
                break;
            }
        case IPPROTO_TCP:
            break;
        default:
            break;
    }
}
#endif //_ECCMNI_SKB_DBG_


#if (ECCMNI_DEBUG_ENABLE || defined(_ECCMNI_SEQ_SUPPORT_))
static void memory_dump(void * start_addr, int size, char* str){
    unsigned int *curr_p = (unsigned int *)start_addr;
    int i = 0;
    for(i=0; i*16 <size; i++){
		printk(KERN_ERR "[ NETD ][ DUMP ] %s 0x%03X: %08X %08X %08X %08X", 
				str, i*16, *curr_p, *(curr_p+1), *(curr_p+2), *(curr_p+3) );
		curr_p+=4;
	}
}
#endif

static KAL_INT32 eccmni_cccich_to_devid(CCCI_CHANNEL_T cccich)
{
    KAL_INT32  eccmni_id;
    switch(cccich)
    {
        case CH_NET1_RX:
        case CH_NET1_TX:
            eccmni_id = ECCMNI0_ID;
            break;
        case CH_NET2_RX:
        case CH_NET2_TX:
            eccmni_id = ECCMNI1_ID;
            break;
        case CH_NET3_RX:
        case CH_NET3_TX:
            eccmni_id = ECCMNI2_ID;
            break;
        default:
            KAL_ASSERT(0);
            break;
    }
    return eccmni_id;
}

static KAL_INT32 eccmni_open(struct net_device *net_dev)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    /* start to trasmit packets !!*/
    netif_start_queue(net_dev) ;
    netif_carrier_on(net_dev);	
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

static KAL_INT32 eccmni_stop(struct net_device *net_dev)
{	
    DEBUG_LOG_FUNCTION_ENTRY;
    /* stop to trasmit packets*/
    netif_stop_queue(net_dev) ;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}
static KAL_INT32 eccmni_change_mtu(struct net_device *net_dev, KAL_INT32 new_mtu)
{
    KAL_INT32 result;
    DEBUG_LOG_FUNCTION_ENTRY;

    if (new_mtu > ECCMNI_MAX_MTU) {
        DBGLOG(NETD, ERR, "mtu siz(%d) > max value(%d)", new_mtu, ECCMNI_MAX_MTU);
        result = -EINVAL;
    }else{
        net_dev->mtu = new_mtu;
        result = KAL_SUCCESS;
    }

    DEBUG_LOG_FUNCTION_LEAVE;	
    return result;
}

static void eccmni_tx_timeout(struct net_device *net_dev)
{
    DEBUG_LOG_FUNCTION_ENTRY;
	
	net_dev->stats.tx_errors++;
	
	/* timeout, we should kick the tx to transmit again */
	/* insert NULL SKB to just kick the tx to transmit again */
    ccci_write_desc_to_q(CH_NET1_TX, NULL);
		
    DEBUG_LOG_FUNCTION_LEAVE;	
}

static KAL_INT32 eccmni_set_mac_address(struct net_device *net_dev, void *p)
{
    DEBUG_LOG_FUNCTION_ENTRY;
	DEBUG_LOG_FUNCTION_LEAVE;	
	return -EOPNOTSUPP;
}

static void eccmni_mk_eth_header(void *_eth_hdr, KAL_UINT8 *mac_addr, KAL_UINT32 packet_type)
{
    struct ethhdr *eth_hdr = _eth_hdr;
    //const u8 s_rxhdr[] = {0x00, 0xbb, 0xaa, 0xcc, 0xdd, 0x44, 0x00, 0xa0, 0xb0, 0xc0, 0xd0, 0x60, 0x08, 0x00}; //test driver

    //memcpy(eth_hdr, s_rxhdr, ETH_HLEN);
    memcpy(eth_hdr->h_dest,   mac_addr, ETH_ALEN);      //sizeof(eth_hdr->h_dest));
    memset(eth_hdr->h_source, 0,        ETH_ALEN);      //sizeof(eth_hdr->h_source));   //reset source as 0
    //memcpy(eth_hdr->h_source, mac_addr, sizeof(eth_hdr->h_source)); Ian
    //eth_hdr->h_proto = __constant_cpu_to_be16(ETH_P_IP);

    if(packet_type == IPV6_VERSION){
		eth_hdr->h_proto = __constant_cpu_to_be16(ETH_P_IPV6);
	}else{
		eth_hdr->h_proto = __constant_cpu_to_be16(ETH_P_IP);
	}
}

static KAL_INT32 eccmni_rx_callback(struct sk_buff *skb, KAL_UINT32 private_data)
{
    CCCI_BUFF_T *p_cccih = NULL;
    struct net_device *net_dev ;
    KAL_UINT32 ccmni_index ;
    KAL_UINT32 packet_type;
    KAL_UINT32 skb_len = 0;
    KAL_INT32  ret = 0;
    
    DEBUG_LOG_FUNCTION_ENTRY;

    if (skb){
        /* |||||CCCI_BUFF_T  --- IP HEADER ---- PAYLOAD */
        /* ========> */
        /* CCCI_BUFF_T  --- |||||IP HEADER ---- PAYLOAD */
        //4 <1> remove CCCI header
        p_cccih = (CCCI_BUFF_T *)skb->data;
        DBGLOG(NETD,DBG,"ECCMNI[Rx] CCCIH(0x%x)(0x%x)(0x%x)(0x%x)",\
            p_cccih->data[0],p_cccih->data[1],p_cccih->channel, p_cccih->reserved );
        ccmni_index = eccmni_cccich_to_devid(p_cccih->channel);
        net_dev = eccmni_inst[ccmni_index].dev;


#ifdef _ECCMNI_SEQ_SUPPORT_
        {
            if(p_cccih->reserved != eccmni_inst[ccmni_index].seqno.DL){
                DBGLOG(NETD, ERR, "ECCMNI%d SEQNO error expect(%#X) in(%#X)", 
                       ccmni_index, eccmni_inst[ccmni_index].seqno.DL, p_cccih->reserved);
                memory_dump((void*)(skb->data), 48, "RX DUMP:");
            }
            eccmni_inst[ccmni_index].seqno.DL = p_cccih->reserved + 1;
        }
#endif  

#if 0 // for emcsva debug  
        {
            static KAL_UINT32 seq_no = 0;
            if((p_cccih+3)->data[1] != seq_no+1 ){
                //DBGLOG(NETD, ERR, "Buffer: %#X %#X %#X %#X",(p_cccih+3)->data[0],(p_cccih+3)->data[1],(p_cccih+3)->channel,(p_cccih+3)->reserved);
                DBGLOG(NETD, ERR, "---Index erro now(%#X) in(%#X)----", seq_no,(p_cccih+3)->data[1]);
            }
            seq_no = (p_cccih+3)->data[1];
        }
#endif        

#if ECCMNI_DEBUG_ENABLE
    {
        memory_dump((void*)(skb->data +16), 32, "RX DUMP:");
    }
#endif

#if 0
        {
            kal_uint32 *ptr;
            ptr = (kal_uint32 *)skb->data;
            ptr += 4;
            DBGLOG(NETD,ERR,"ECCMNI Rx packet dummp:\n(%#08X)(%#08X)(%#08X)(%#08X)(%#08X)(%#08X)",\
                (unsigned int)(*ptr),(unsigned int)(*(ptr+1)),(unsigned int)(*(ptr+2)),
                (unsigned int)(*(ptr+3)),(unsigned int)(*(ptr+4)),(unsigned int)(*(ptr+5)));            
        }
#endif
        skb_pull(skb, sizeof(CCCI_BUFF_T));

        //4 <2> append Ethernet header
#if 0   // 20121227 _1
        skb_push (skb, ETH_HLEN);
        eccmni_mk_eth_header(skb->data, net_dev->dev_addr);
        skb_set_mac_header(skb,0);
        skb->dev = net_dev;
        skb->protocol = htons(ETH_P_IP);
        skb->ip_summed = CHECKSUM_UNNECESSARY;
#else
        packet_type = skb->data[0] & 0xF0;
        eccmni_mk_eth_header(skb->data-ETH_HLEN, net_dev->dev_addr, packet_type);
        skb_set_mac_header(skb, -ETH_HLEN);
        skb->dev       = net_dev;
        if(packet_type == IPV6_VERSION){            
            skb->protocol  = htons(ETH_P_IPV6);
        } else {
            skb->protocol  = htons(ETH_P_IP);
        }
        //skb->ip_summed = CHECKSUM_UNNECESSARY;
	skb->ip_summed = CHECKSUM_NONE;
//        skb_reset_network_header(skb); 
//        skb_reset_transport_header(skb); 
#endif

        //eccmni_dbg_skb_header(skb);
        skb_len = skb->len;
#if 0
        if (unlikely(netif_rx(skb) != NET_RX_SUCCESS)){
            DBGLOG(NETD, ERR,"netif_rx fail: port%d, pkt_len=%d", \
			eccmni_inst[ccmni_index].eemcs_port_id, skb->len);
        }
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
        if(!in_interrupt()){ /* only in non-interrupt context */
            ret = netif_rx_ni(skb); 
        }
        else {
            ret = netif_rx(skb);
        }
#else
        ret = netif_rx(skb);
#endif
        if (ret != NET_RX_SUCCESS){
            DBGLOG(NETD, ERR,"netif_rx fail: port%d, pkt_len=%d", \
			eccmni_inst[ccmni_index].eemcs_port_id, skb->len);
        } 

        net_dev->stats.rx_packets++;
        net_dev->stats.rx_bytes += skb_len;

    }

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS ;
}

KAL_INT32 eccmni_hard_start_xmit(struct sk_buff *skb, struct net_device *net_dev)
{
    KAL_INT32 ret = NETDEV_TX_OK;
    KAL_UINT32 tx_len;
    CCCI_BUFF_T * ccci_header ;
    eccmni_inst_t *p_priv_eccmni;
    CCCI_CHANNEL_T ccci_tx_channel; 
    static KAL_UINT32 tx_busy_retry_cnt = 0;
#ifdef _ECCMNI_SEQ_SUPPORT_
    KAL_UINT32 ccmni_index = 0;
#endif

    DEBUG_LOG_FUNCTION_ENTRY;
    p_priv_eccmni = netdev_priv(net_dev);
    KAL_ASSERT(p_priv_eccmni != NULL);
    ccci_tx_channel = p_priv_eccmni->ccci_ch.tx;
    
    //eccmni_dbg_skb_addr(skb, 99);
    //eccmni_dbg_skb_header(skb);


    /* Note ALPS.ICS won't have MAC address, unlike ALPS.GB */
    //20121227 //4 <1> remove Etherheader
    //20121227 skb_pull(skb, ETH_HLEN);

    //4 <2> check size < MTU
    if (skb->len > ECCMNI_MAX_MTU)
    {
        DBGLOG(NETD, ERR, "ccmni%d pkt_len(%d)>MTU(%d)", p_priv_eccmni->eemcs_port_id, skb->len, ECCMNI_MAX_MTU);
        ret = NETDEV_TX_OK;
        goto NET_XMIT_ERR;
    }
    
    //4 <3> Insert CCCI header
    if (skb_headroom(skb) < sizeof(CCCI_BUFF_T)){
        DBGLOG(NETD,ERR,"ch%d headerroom: %d is not enough, pkt len is %d, hardheader is %d", ccci_tx_channel, skb_headroom(skb), skb->len, net_dev->hard_header_len) ;	
        ret = NETDEV_TX_OK;
        goto NET_XMIT_ERR;
    }
	
	ccmni_index = eccmni_cccich_to_devid(ccci_tx_channel);
	if (ccci_write_space_alloc(ccci_tx_channel)==0){
        if(tx_busy_retry_cnt %20000 == 10000){
		DBGLOG(NETD,WAR,"CCMNI%d TX busy: retry_times=%d", ccmni_index, tx_busy_retry_cnt);
        }
        tx_busy_retry_cnt++;
        ret = NETDEV_TX_BUSY;
        goto NET_XMIT_BUSY;
	}	
	/* Fill the CCCI header */
	{
		tx_busy_retry_cnt = 0;
		ccci_header = (CCCI_BUFF_T *)skb_push(skb, sizeof(CCCI_BUFF_T)) ;
		/* Fill the channel */
		ccci_header->channel = ccci_tx_channel; 
		/* reserved */
		ccci_header->data[0] = 0 ;
		/* Fill the packet length */
		ccci_header->data[1] = skb->len ;
#ifdef _ECCMNI_SEQ_SUPPORT_
		/* Fill the magic number */
		ccci_header->reserved = p_priv_eccmni->seqno.UL++;
#else
		ccci_header->reserved = 0 ;
#endif
	}
    
    tx_len = skb->len;
    ret = ccci_write_desc_to_q(ccci_tx_channel, skb);

	if (KAL_SUCCESS != ret) {
		//DBGLOG(NETD, ERR, "PKT DROP of ch%d!",ccci_tx_channel);
#ifdef _ECCMNI_SEQ_SUPPORT_
		/* Rollback the magic number */
		p_priv_eccmni->seqno.UL--;
#endif
		dev_kfree_skb(skb); 
	} else {
		/* queue skb to xmit wait queue and send it */
		net_dev->stats.tx_packets++;
		net_dev->stats.tx_bytes += (tx_len - sizeof(CCCI_BUFF_T));
	}

    DEBUG_LOG_FUNCTION_LEAVE;	
    return ret;

NET_XMIT_ERR:
    dev_kfree_skb(skb);
    net_dev->stats.tx_dropped++;
NET_XMIT_BUSY:
    DEBUG_LOG_FUNCTION_LEAVE;	
    return ret;
}

void eccmni_state_callback_func(EEMCS_STATE state){
    int ccmni_idx = 0;
	
    DBGLOG(NETD, TRA, "eccmni_state_callback_func: state=%d ", state);	
	
    switch(state){
        case EEMCS_EXCEPTION:
            for(ccmni_idx = 0; ccmni_idx < ECCMNI_MAX_DEV; ccmni_idx++)
            {
                netif_carrier_off(eccmni_inst[ccmni_idx].dev);
            }
            break;
        case EEMCS_GATE: //MD reset
            for(ccmni_idx = 0; ccmni_idx < ECCMNI_MAX_DEV; ccmni_idx++)
            {
                netif_carrier_off(eccmni_inst[ccmni_idx].dev);
            }
            break;
        case EEMCS_BOOTING_DONE:
            for(ccmni_idx = 0; ccmni_idx < ECCMNI_MAX_DEV; ccmni_idx++)
            {
                netif_carrier_on(eccmni_inst[ccmni_idx].dev);
                #if (defined(_ECCMNI_SEQ_SUPPORT_))
                eccmni_inst[ccmni_idx].seqno.UL = 0;
                eccmni_inst[ccmni_idx].seqno.DL = 0;
                #endif
            }        
            break; 
        default:
            break;
    }
}

EEMCS_STATE_CALLBACK_T eccmni_state_callback ={
    .callback = eccmni_state_callback_func,
};

static struct net_device_ops eccmni_ops[ECCMNI_MAX_DEV] ;
//static void haha(struct net_device *dev){}
struct net_device* eccmni_dev_init(KAL_INT32 index, KAL_INT32 mtu_size) 
{
    struct net_device *net_dev; 
    eccmni_inst_t *p_priv_eccmni;

    DEBUG_LOG_FUNCTION_ENTRY;

    net_dev = alloc_etherdev(sizeof(eccmni_inst_t));
    if(!net_dev){
        DBGLOG(NETD, ERR, "alloc_etherdev fail");		
        return NULL ;
    }

    p_priv_eccmni = netdev_priv(net_dev);
    memset(p_priv_eccmni,0,sizeof(eccmni_inst_t));

    net_dev->header_ops     = NULL;
    net_dev->mtu            = mtu_size;
    net_dev->tx_queue_len   = ECCMNI_TX_QLEN;
    net_dev->watchdog_timeo = ECCMNI_TX_TIMEOUT;
    net_dev->flags          = IFF_NOARP &                       /* eccmni is a pure IP device */
                            (~IFF_BROADCAST & ~IFF_MULTICAST);  /* eccmni is P2P */
    net_dev->features       = NETIF_F_VLAN_CHALLENGED ;         /* not support VLAN */
    net_dev->addr_len       = ETH_ALEN;                         /* ethernet header size */
    net_dev->destructor     = free_netdev;
    net_dev->hard_header_len += sizeof(CCCI_BUFF_T);            /* reserve Tx CCCI header room */
    
	/*clear memory before using*/
	memset(&eccmni_ops[index],0,sizeof(struct net_device_ops));
	eccmni_ops[index].ndo_open            = eccmni_open;		
	eccmni_ops[index].ndo_stop            = eccmni_stop;
	eccmni_ops[index].ndo_change_mtu      = eccmni_change_mtu;
	eccmni_ops[index].ndo_tx_timeout      = eccmni_tx_timeout;
	eccmni_ops[index].ndo_set_mac_address = eccmni_set_mac_address;	
	eccmni_ops[index].ndo_start_xmit      = eccmni_hard_start_xmit ;
	net_dev->netdev_ops = &eccmni_ops[index]; 

//	sprintf(net_dev->name, "eccmni%d", index);
    sprintf(net_dev->name, "ccemni%d", index);

#if 0
    {
        char net_mac[6] ;
        if(index == 0){
          	net_mac[0] = 0x2A ; 
        	net_mac[1] = 0x2B ; 
        	net_mac[2] = 0x2C ; 
        	net_mac[3] = 0x2D ; 
        	net_mac[4] = 0x2E ; 
        	net_mac[5] = 0x2F ; 
            memcpy(net_dev->dev_addr, net_mac, ETH_ALEN);
        }else if(index == 1){
          	net_mac[0] = 0x1A ; 
        	net_mac[1] = 0x1B ; 
        	net_mac[2] = 0x1C ; 
        	net_mac[3] = 0x1D ; 
        	net_mac[4] = 0x1E ; 
        	net_mac[5] = 0x1F ; 
            memcpy(net_dev->dev_addr, net_mac, ETH_ALEN);

        }else if(index == 2){
          	net_mac[0] = 0x0A ; 
        	net_mac[1] = 0x0B ; 
        	net_mac[2] = 0x0C ; 
        	net_mac[3] = 0x0D ; 
        	net_mac[4] = 0x0E ; 
        	net_mac[5] = 0x0F ; 
            memcpy(net_dev->dev_addr, net_mac, ETH_ALEN);

        }
    }
#else
    random_ether_addr((KAL_UINT8 *) net_dev->dev_addr);
#endif
   
    DEBUG_LOG_FUNCTION_LEAVE;
	return net_dev ;
}

KAL_UINT32 eccmni_init_single(KAL_UINT8 ccmni_idx)
{
    KAL_INT32 ret;
    ccci_port_cfg *curr_port_info = NULL;
    eccmni_inst_t *p_priv_eccmni;

    DEBUG_LOG_FUNCTION_ENTRY;

    //4 <1> alloc_etherdev
    eccmni_inst[ccmni_idx].dev = eccmni_dev_init(ccmni_idx,ECCMNI_DEF_MTU);
    if(!eccmni_inst[ccmni_idx].dev){
        DBGLOG(NETD, ERR, "eccmni%d alloc netdev fail", ccmni_idx);
        goto NETIF_ALLOC_FAIL;
    }
    
    //4 <2>  register_netdev
    /* set eccmni instance */
    eccmni_inst[ccmni_idx].eemcs_port_id = eccmni_port_mapping[ccmni_idx];
    curr_port_info = ccci_get_port_info(eccmni_inst[ccmni_idx].eemcs_port_id);
    eccmni_inst[ccmni_idx].ccci_ch.rx = curr_port_info->ch.rx;
    eccmni_inst[ccmni_idx].ccci_ch.tx = curr_port_info->ch.tx;
    /* set priv inform in net_dev */
    p_priv_eccmni                = netdev_priv(eccmni_inst[ccmni_idx].dev);
    p_priv_eccmni->dev           = eccmni_inst[ccmni_idx].dev;
    p_priv_eccmni->eemcs_port_id = eccmni_inst[ccmni_idx].eemcs_port_id;
    p_priv_eccmni->ccci_ch.rx    = eccmni_inst[ccmni_idx].ccci_ch.rx;
    p_priv_eccmni->ccci_ch.tx    = eccmni_inst[ccmni_idx].ccci_ch.tx;
        
    ret = ccci_register(eccmni_inst[ccmni_idx].ccci_ch.rx, eccmni_rx_callback, 0);
    KAL_ASSERT(ret == KAL_SUCCESS);
    ret = register_netdev(eccmni_inst[ccmni_idx].dev);
    if (ret){
        DBGLOG(NETD, ERR, "eccmni%d register_netdev fail: %d", ccmni_idx, ret);
        goto NET_REG_FAIL ;
    }

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
	
NET_REG_FAIL:
    //unregister_netdev(eccmni_inst[ccmni_idx].dev);
    ccci_unregister(eccmni_inst[ccmni_idx].ccci_ch.rx);
    
    /*free the net device*/
    free_netdev(eccmni_inst[ccmni_idx].dev);
	
NETIF_ALLOC_FAIL:    
    DEBUG_LOG_FUNCTION_LEAVE;
	return KAL_FAIL;
}


KAL_INT32 eccmni_mod_init(void) 
{
#ifdef __ECCMNI_SUPPORT__
    KAL_INT32 ccmni_idx = 0;
    KAL_INT32 ret = KAL_SUCCESS;
    DEBUG_LOG_FUNCTION_ENTRY;

    for(ccmni_idx = 0; ccmni_idx < ECCMNI_MAX_DEV; ccmni_idx ++)
    {
        if ((ret = eccmni_init_single(ccmni_idx)) != KAL_SUCCESS){
            ret = KAL_FAIL;
            break;
        }
    }

    eemcs_state_callback_register(&eccmni_state_callback);
    DEBUG_LOG_FUNCTION_LEAVE;
    return ret;
#else
    return KAL_SUCCESS;
#endif
}

void  eccmni_deinit_mod_exit(void)
{
#ifdef __ECCMNI_SUPPORT__
    KAL_INT32 curr_unreg_idx = 0;
    DEBUG_LOG_FUNCTION_ENTRY;

    for(curr_unreg_idx = 0; curr_unreg_idx < ECCMNI_MAX_DEV; curr_unreg_idx++)
    {
        if(eccmni_inst[curr_unreg_idx].dev){
        	unregister_netdev(eccmni_inst[curr_unreg_idx].dev);
            //free_netdev(eccmni_inst[curr_unreg_idx].dev);
        }
        ccci_unregister(eccmni_inst[curr_unreg_idx].ccci_ch.rx);
    }
    DEBUG_LOG_FUNCTION_LEAVE;
#endif
    return;
}

/*******************************************************************************
*                            U    T
********************************************************************************/

#if defined(_ECCMNI_LB_UT_) || defined(_EEMCS_CCCI_LB_UT)
KAL_INT32 eccmni_swap(struct sk_buff *skb)    
{
    struct iphdr  *iph, tmp_iph;
    struct udphdr *udph, tmp_udph;

    /****** please NOOOOOTES ******/
    /*skb->data must be ip header*/
#if 0
    iph = ip_hdr(skb);  
    if(iph == NULL){
        DBGLOG(NETD,DBG,"[NETD_UT] GET iph fail");
        return KAL_FAIL;
    }
    udph = (struct udphdr *)(skb_network_header(skb) + ip_hdrlen(skb));
    if (udph == NULL) {
        DBGLOG(NETD,DBG,"[NETD_UT] GET udph fail");
        return KAL_FAIL;
    }
#else
    iph  = (struct iphdr *)((u8 *)skb->data + sizeof(CCCI_BUFF_T));
    //eccmni_dbg_ip_header(iph);
    if(iph->version != 4){
        DBGLOG(NETD,ERR,"[NETD_UT] SWAP can only handle ipv4 (%d)", iph->version);
        return KAL_FAIL;
    }
    KAL_ASSERT(20 == (iph->ihl << 2));
    udph = (struct udphdr *)((u8 *)skb->data + (iph->ihl << 2));
#endif
    DBGLOG(NETD,DBG,"[NETD_UT] from iphr 0x%x 0x%x udphdr 0x%x 0x%x ", iph->saddr, iph->daddr, ntohs(udph->source), ntohs(udph->dest));
    {
        KAL_UINT16  ori_chk, ori_uchk; 
        KAL_UINT32  ori_schk;
        ori_uchk    = udph->check; 
        ori_schk    = skb->csum; 
        ori_chk     = iph->check;

        /* swap ip */
        tmp_iph.daddr   = iph->saddr;
        tmp_iph.saddr   = iph->daddr;
        iph->saddr      = tmp_iph.saddr;
        iph->daddr      = tmp_iph.daddr;
        /* swap port */
        tmp_udph.source = udph->dest;
        tmp_udph.dest   = udph->source;
       
        /* OKOK */
        iph->check      = 0;
        iph->check      = ip_fast_csum((u8 *)iph, iph->ihl);
    }
    DBGLOG(NETD,DBG,"[NETD_UT] from iphr 0x%x 0x%x udphdr 0x%x 0x%x ", iph->saddr, iph->daddr, ntohs(udph->source), ntohs(udph->dest));

#if 0
    /* ian 20130124 dump emcsva header */
    {
        static int _mod_idx = 0;
        unsigned int *ptr;
        ptr = ((u8 *)skb->data + (iph->ihl << 2) + 8);
        if(0 == (_mod_idx % 10)){
            DBGLOG(NETD,TRA,"[EMCSVA_DUMP] (0x%x) (0x%x) (0x%x) (0x%x)",\
                (unsigned int)*(ptr+4), (unsigned int)*(ptr+5), (unsigned int)*(ptr+6), (unsigned int)*(ptr+7));
        }
        _mod_idx ++;
    }
#endif

    return KAL_SUCCESS;
}
#endif

#if defined(_ECCMNI_LB_UT_)
KAL_UINT32 eccmniut_register_callback(CCCI_CHANNEL_T chn, EEMCS_CCCI_CALLBACK func_ptr , KAL_UINT32 private_data) {	
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(NETD,DBG, "[UT]CCCI channel (%d) register callback", chn);
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_UINT32 eccmniut_unregister_callback(CCCI_CHANNEL_T chn) {
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(NETD,DBG, "CCCI channel (%d) UNregister callback", chn);
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

inline KAL_UINT32 eccmniut_UL_write_room_alloc(CCCI_CHANNEL_T chn)
{
	DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
	return 1;
}

inline KAL_INT32 eccmniut_UL_write_skb_to_swq(CCCI_CHANNEL_T chn, struct sk_buff *skb)
{
	CCCI_BUFF_T *pccci_h = (CCCI_BUFF_T *)skb->data;
    
	DEBUG_LOG_FUNCTION_ENTRY;
    
	DBGLOG(NETD,DBG, "CCCI channel (%d) ccci_write CCCI_H(0x%x)(0x%x)(0x%x)(0x%x)",\
        chn, pccci_h->data[0], pccci_h->data[1], pccci_h->channel, pccci_h->reserved);

#ifdef _ECCMNI_LB_UT_
    {
        KAL_UINT8 ccmni_index;
        struct sk_buff *new_skb;
//	        int i = 0;
//	        for (i=0;i<3;i++)
        {
            ccmni_index = eccmni_cccich_to_devid(pccci_h->channel);
            pccci_h->channel = eccmni_inst[ccmni_index].ccci_ch.rx;
            new_skb = dev_alloc_skb(skb->len);
            if(new_skb == NULL){
                DBGLOG(NETD,ERR,"[NETD_UT] _ECCMNI_LB_UT_ dev_alloc_skb fail sz(%d).", skb->len);
                dev_kfree_skb(skb);
                DEBUG_LOG_FUNCTION_LEAVE;
        	    return KAL_SUCCESS;
            }
            memcpy(skb_put(new_skb, skb->len), skb->data, skb->len);
            eccmni_swap(new_skb);
            DBGLOG(NETD,DBG,"[NETD_UT] EEMCS_CCMNI_PORT(%d) LB packet (%d).", eccmni_inst[ccmni_index].eemcs_port_id, new_skb->len);
            //eccmni_dbg_skb_header(new_skb);
            eccmni_rx_callback(new_skb, ECCMNI_LB);
        }
    }
#endif

    dev_kfree_skb(skb);
    DEBUG_LOG_FUNCTION_LEAVE;
	return KAL_SUCCESS;
}
#endif
