/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccmni_pfp.c
 *
 * Project:
 * --------
 *   YuSu
 *
 * Description:
 * ------------
 *   MT6516 Cross Chip Modem Network Interface - Packet Framing Protocol
 *
 * Author:
 * -------
 *   Stanley Chou (mtk01411)
 *
 ****************************************************************************/

#define pr_fmt(fmt) "[" KBUILD_MODNAME "][pfp]" fmt

#include <linux/module.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include "ccmni_pfp.h"

/* Compile option for pfp kernel debug */
#undef __PFP_KERNEL_DEBUG__ 


#if 1

#define  PFP_LOG(instance_inx, flag, args...)

#else

    #define  PFP_LOG(instance_inx, flag, args...)         \
    do                                                    \
    {                                                     \
        printk(flag "CCMNI%d: ", instance_inx);           \
        printk(args);                                     \
    } while (0)

#endif

ccmni_record_t ccmni_dev          [MAX_PDP_CONT_NUM];
unsigned char  frame_cooked_data  [MAX_PFP_LEN_FIELD_VALUE];
unsigned char  frame_raw_data     [MAX_PFP_LEN_FIELD_VALUE + 4];
unsigned char  unframe_raw_data   [FREE_RAW_DATA_BUF_SIZE];
unsigned char  unframe_cooked_data[FREE_COOKED_DATA_BUF_SIZE];

#ifndef __SUPPORT_DYNAMIC_MULTIPLE_FRAME__
complete_ippkt_t complete_ippkt_pool[SUPPORT_PKT_NUM];
#endif

#ifndef __SUPPORT_DYNAMIC_MULTIPLE_FRAME__
complete_ippkt_t* get_one_available_complete_ippkt_entry()
{
    int i = 0;
    for (i=0; i < SUPPORT_PKT_NUM; i++)
    {
        if (complete_ippkt_pool[i].entry_used == 0)
        {
            complete_ippkt_pool[i].entry_used = 1;
            return &complete_ippkt_pool[i];
        }
    }
    BUG_ON(1); 
    return NULL;
}

void release_one_used_complete_ippkt_entry(complete_ippkt_t *entry)
{
    entry->pkt_size = 0;
    entry->pkt_data = NULL;
    entry->entry_used = 0;
    entry->next = NULL;
}
#endif

void pfp_reset(int ccmni_inx)
{
    ccmni_dev[ccmni_inx].unframe_state = PARSE_PFP_FRAME_START_FLAG_STATE;
    ccmni_dev[ccmni_inx].pkt_size = 0;
    ccmni_dev[ccmni_inx].last_pkt_node = NULL;
}





frame_info_t pfp_frame(unsigned char* raw_data, unsigned char* cooked_data, int cooked_size, int frame_flag, int ccmni_inx)
{
    frame_info_t entry;

    /* One IP packet will only be packed as one frame */
    raw_data[0] = PFP_FRAME_START_FLAG;
    raw_data[1] = PFP_FRAME_MAGIC_NUM;
    /* Length Field Byte#1: Low Byte */
    raw_data[2] = (cooked_size & 0x000000FF); 
    /* Length Field Byte#2: High Byte */
    raw_data[3] = ((cooked_size >> 8) & 0x000000FF);
 
    // printk(KERN_INFO "CCMNI%d: TX_IPID=0x%02x%02x,pkt_size=%d\n",ccmni_inx,cooked_data[4],cooked_data[5],cooked_size);
    /* Copy data from the input buffer to output buffer */
    memcpy(raw_data + 4, cooked_data, cooked_size);

    entry.num_frames = 1;
    //entry.frame_list = (frame_format_t *) vmalloc(sizeof(frame_format_t) * entry.num_frames);
    entry.frame_list[0].frame_size = cooked_size + 4;
    entry.frame_list[0].frame_data = raw_data;
    entry.pending_data_flag = FRAME_END;
    entry.consumed_length   = cooked_size;
    
    /* NOTE: CCCI Client must update its passed pointer cooked_data by adding
     *       the size (i.e., cooked_size) after returning from this API.
     *        
     * NOTE: CCCI Client must update its passed poiniter raw_data by adding the
     *       frame_list[0].frame_size after returning from this API
     */
    
    return entry;
}

packet_info_t pfp_unframe(unsigned char* cooked_data, int cooked_data_buf_size,
                          unsigned char* raw_data, int raw_size, int ccmni_inx)
{
    /* If this function is invoked for multiple PDP contexts, each one should
     * have its own unframe_state belonged to its packet processing
     */
    
    //static int unframe_state[MAX_PDP_CONT_NUM] = {PARSE_PFP_FRAME_START_FLAG_STATE};
    //static int pkt_size[MAX_PDP_CONT_NUM] = {0};
    
    int local_raw_size                  = raw_size;
    int local_cooked_data_buf_free_size = cooked_data_buf_size;
    unsigned char *local_raw_data       = raw_data;
    unsigned char *local_cooked_data    = cooked_data;
    int consumed_length                 = 0;
    int keep_parsing                    = 1;
    int retrieved_num_ip_pkts           = 0;
    packet_info_t entry                 = {0};

    /* Lifecycle of CCMNIDev[ccmni_inx].last_node is same as that of PacketInfo entry */
    ccmni_dev[ccmni_inx].last_pkt_node = NULL;
    
    do
    {
#ifdef __PFP_KERNEL_DEBUG__
        pr_notice("CCMNI%d: pfp_unframe_state=%d\n",ccmni_inx,ccmni_dev[ccmni_inx].unframe_state);
#endif
        switch(ccmni_dev[ccmni_inx].unframe_state)
        {
            case PARSE_PFP_FRAME_START_FLAG_STATE:
            {
                // if(((unsigned char)local_raw_data[0]) == PFP_FRAME_START_FLAG)
#ifdef __PFP_KERNEL_DEBUG__                
                pr_notice("CCMNI%d: check start flag-local_raw_data[0]=0x%02x\n",ccmni_inx,local_raw_data[0]);
#endif
                if (local_raw_data[0] == PFP_FRAME_START_FLAG)
                {
                    /* 
                    * If the START_FLAG is not found, it will not change the
                    * unframe_state - still remain as PFP_FRAME_START_FLAG
                    */
                    local_raw_data++;
                    local_raw_size--;
                    consumed_length++;                    
                    break;   
                }
                else
                {
                    /* Fall through to check if it is a magic num */
                    ccmni_dev[ccmni_inx].unframe_state = PARSE_PFP_FRAME_MAGIC_NUM_STATE;
                }
                
            }
            
            case PARSE_PFP_FRAME_MAGIC_NUM_STATE:
            {
                // if(((unsigned char)local_raw_data[0]) != PFP_FRAME_MAGIC_NUM)
#ifdef __PFP_KERNEL_DEBUG__                
                pr_notice("CCMNI%d: check magic num-local_raw_data[0]=0x%02x\n",ccmni_inx,local_raw_data[0]);
#endif
                
                if (local_raw_data[0] != PFP_FRAME_MAGIC_NUM)
                {
                    /* Something is wrong! MAGIC_NUM must follow the START_FLAG */
                    ccmni_dev[ccmni_inx].unframe_state = PARSE_PFP_FRAME_START_FLAG_STATE;
                }
                else
                {
                    ccmni_dev[ccmni_inx].unframe_state = PARSE_PFP_FRAME_LENGTH_FIELD_STATE;
                }
                
                local_raw_data++;
                local_raw_size--;
                consumed_length++;
                
                break;
            }
            
            case PARSE_PFP_FRAME_LENGTH_FIELD_STATE:
            {
                /* Check if two bytes Length Field can be obtained from the raw_data[] */
#ifdef __PFP_KERNEL_DEBUG__                
                pr_notice("CCMNI%d: local_raw_size=%d\n",ccmni_inx,local_raw_size);
#endif
                if (local_raw_size >= 2)
                {
                    ccmni_dev[ccmni_inx].pkt_size = ((local_raw_data[1] << 8) | local_raw_data[0]);

#ifdef __PFP_KERNEL_DEBUG__
                    pr_notice("CCMNI%d: pkt_size=%d,len[0]=0x%02x,len[1]=0x%02x\n",ccmni_inx,ccmni_dev[ccmni_inx].pkt_size,local_raw_data[0],local_raw_data[1]);
#endif                    
                    /* Parse the Length Field */
                    local_raw_data  += 2;
                    local_raw_size  -= 2;
                    consumed_length += 2;

                    /* Check if the value is exceeded the maximum size of one IP Packet: 1510 bytes */
                    if (ccmni_dev[ccmni_inx].pkt_size <= MAX_PFP_LEN_FIELD_VALUE)
                    {
                        ccmni_dev[ccmni_inx].unframe_state = PARSE_PFP_FRAME_GET_DATA_STATE;
                    }
                    else
                    {
                        /* Change state to PARSE_START_FLAG to find the next frame */
#ifdef __PFP_KERNEL_DEBUG__
                        pr_notice("CCMNI%d: Reset decode state then continue to parse\n",ccmni_inx);
#endif
                        ccmni_dev[ccmni_inx].unframe_state = PARSE_PFP_FRAME_START_FLAG_STATE;
                        ccmni_dev[ccmni_inx].pkt_size      = 0;
                    }
                }
                else
                {
                    /*
                     * Not enough information to parse the Length Field's value:
                     * Keep the state as the PARSE_PFP_FRAME_LENGTH_FIELD_STATE
                     */
#ifdef __PFP_KERNEL_DEBUG__                    
                    pr_notice("CCMNI%d: not enough len bytes\n",ccmni_inx);
#endif
                    keep_parsing = 0;
                }
                
                break;
            }
            
            case PARSE_PFP_FRAME_GET_DATA_STATE:
            {
                if (local_raw_size < ccmni_dev[ccmni_inx].pkt_size)
                {
                    /*
                     * If only partial data, it will not do anything for it:
                     * Wait for the data can be retrived as one complete IP Packet
                     */
#ifdef __PFP_KERNEL_DEBUG__                    
                    pr_notice("CCMNI%d: not enough pkt bytes\n",ccmni_inx);
#endif
                    keep_parsing = 0;
                }
                else
                {
                    /*
                     * It must check the avaialbe space pointed by cooked_data is
                     * larger than the size of one packet
                     */
                    
                    if (local_cooked_data_buf_free_size >= ccmni_dev[ccmni_inx].pkt_size)
                    {                       
                        complete_ippkt_t *current_node  = entry.pkt_list;
                        complete_ippkt_t *previous_node = ccmni_dev[ccmni_inx].last_pkt_node;

                        /*
                         * It can retrieve a complete IP Packet: Record the start position
                         * of memory of this IP Packet
                         */
                        
                        if (current_node == NULL)
                        {
#ifdef __PFP_KERNEL_DEBUG__
                            pr_notice("CCMNI%d: malloc for first pkt node\n",ccmni_inx);
#endif
#ifdef __SUPPORT_DYNAMIC_MULTIPLE_FRAME__                             
                            current_node   = (complete_ippkt_t *) vmalloc(sizeof(complete_ippkt_t));
#else
                            /* Find one available entry */
                            current_node   = (complete_ippkt_t *) get_one_available_complete_ippkt_entry();
#endif                      
                            if (current_node != NULL)
                            {
                                entry.pkt_list = current_node;
                            }
                            else
                            {
                                /* Error Handle */
#ifdef __PFP_KERNEL_DEBUG__
                                pr_notice("CCMNI%d: Can't find one available complete_ippkt entry case1\n",ccmni_inx);
#endif
                                entry.try_decode_again = 1;
                                goto error_handle_return;
                            }
                        }
                        else
                        { 
#if 0
                            do
                            {   
                                previous_node = current_node;
                                current_node  = current_node->next;   
                            }
                            while(current_node != NULL);
#endif

#ifdef __SUPPORT_DYNAMIC_MULTIPLE_FRAME__
                            /* Find the 1st position to insert this new node into the linked list */
                            current_node = (complete_ippkt_t *) vmalloc(sizeof(complete_ippkt_t));
#else
                            /* Find one available entry */
                            current_node   = (complete_ippkt_t *) get_one_available_complete_ippkt_entry();
#endif
                            if (current_node != NULL)
                            {
                                previous_node->next = current_node;
                            }
                            else
                            {
                                /* Error Handle */
#ifdef __PFP_KERNEL_DEBUG__
                                pr_notice("CCMNI%d: Can't find one available complete_ippkt entry case2\n",ccmni_inx);
#endif                        
                                entry.try_decode_again = 1;
                                goto error_handle_return;
                            }
                        }
#ifdef __PFP_KERNEL_DEBUG__                        
                        pr_notice("CCMNI%d: prepare pkt node\n",ccmni_inx);
#endif
                        current_node->pkt_size = ccmni_dev[ccmni_inx].pkt_size;
                        
                        /* Copy data from the input buffer to output buffer */
                        memcpy(local_cooked_data, local_raw_data, ccmni_dev[ccmni_inx].pkt_size);


                        // printk(KERN_INFO "CCMNI%d: RX_IPID:0x%02x%02x,pkt_size=%d\n",ccmni_inx,local_cooked_data[4],local_cooked_data[5],current_node->pkt_size);

                        current_node->pkt_data = local_cooked_data;
                        local_cooked_data     += ccmni_dev[ccmni_inx].pkt_size;
                        current_node->next     = NULL;
                        
                        /*
                         * Remember the last_node for each CCMNI: It can directly access the tail
                         * one without iterating for inserting a new IP Pkt into this list
                         */
                        
                        ccmni_dev[ccmni_inx].last_pkt_node = current_node;
                        
                        local_raw_data += ccmni_dev[ccmni_inx].pkt_size;
                        local_raw_size -= ccmni_dev[ccmni_inx].pkt_size;
                        local_cooked_data_buf_free_size -= ccmni_dev[ccmni_inx].pkt_size;
                        
                        /*
                         * consumed_length will add the num of data bytes copied from input buffer
                         * to output buffer
                         */
                        
                        consumed_length += ccmni_dev[ccmni_inx].pkt_size;
                        retrieved_num_ip_pkts++;
                        
                        /* Change the state to parse the next one */
                        ccmni_dev[ccmni_inx].unframe_state = PARSE_PFP_FRAME_START_FLAG_STATE;
                        ccmni_dev[ccmni_inx].pkt_size      = 0;
                    }
                    else
                    {
                        /*
                         * Not available free space pointed by cooked_data to put one complete
                         * IP Packet : Keep the unframe_state as GET_DATA_STATE
                         */
#ifdef __PFP_KERNEL_DEBUG__                        
                        pr_notice("CCMNI%d: not enough free space provided by cooked_data\n",ccmni_inx);
#endif
                        keep_parsing = 0;
                    }
                }
                break;
            }
            
            default:
            {
                break;
            }
        }    
    }
    while (local_raw_size > 0 && keep_parsing == 1);
error_handle_return:
    entry.consumed_length      = consumed_length;
    entry.num_complete_packets = retrieved_num_ip_pkts;
    entry.parse_data_state     = ccmni_dev[ccmni_inx].unframe_state;
   
    return entry;
}


void traverse_pkt_list(complete_ippkt_t *node)
{
    complete_ippkt_t *t_pkt_node    = node;
    complete_ippkt_t *prev_pkt_node = NULL;
    
    while (t_pkt_node != NULL)
    {
#ifdef __PFP_KERNEL_DEBUG__
        pr_notice("Packet Node: data=0x%08x, size=%d\n", t_pkt_node->pkt_data, t_pkt_node->pkt_size);
#endif
        prev_pkt_node = t_pkt_node;
        t_pkt_node    = t_pkt_node->next;
        
        vfree(prev_pkt_node);
    }
}

EXPORT_SYMBOL(pfp_frame);
EXPORT_SYMBOL(pfp_unframe);
EXPORT_SYMBOL(release_one_used_complete_ippkt_entry);

