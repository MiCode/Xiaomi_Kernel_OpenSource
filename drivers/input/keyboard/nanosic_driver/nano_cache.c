/** ***************************************************************************
 * @file nano_cache.c
 *
 * @brief implent nanosic cache input and delay to send input system
 *
 * <em>Copyright (C) 2010, Nanosic, Inc.  All rights reserved.</em>
 * Author : Bin.yuan bin.yuan@nanosic.com 
 * */

/*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/hid.h>

#include "nano_macro.h"

#define MAX_CACHE_BLOCKS  (30)

typedef struct{
    EM_PacketType type;
    int     datalen;
    uint8_t data[10];
}cahae_buffer_t;

typedef struct cache_s {
    cahae_buffer_t cache_buf[MAX_CACHE_BLOCKS];
    uint8_t cache_read_pos;
    uint8_t cache_write_pos;
    spinlock_t cache_spinlock;
    struct timer_list cache_timer;
    uint8_t cache_cnt;
}input_cache_t;

static input_cache_t gInputCache;

/** **************************************************************************
 * @func  Nanosic_cache_expire
 * 
 * @brief Handler for timer timeout process
 * 
 ** */
void Nanosic_cache_expire(struct timer_list *t)
{
    input_cache_t* input_cache = &gInputCache;
    cahae_buffer_t* cache_node;
    __u8 read_pos;
    
    dbgprint(DEBUG_LEVEL, "cache time expire\n");
    
    if(IS_ERR_OR_NULL(input_cache)){
        dbgprint(DEBUG_LEVEL, "invalid cache struct\n");
        return;
    }

    spin_lock(&input_cache->cache_spinlock);
    
    while(input_cache->cache_cnt)
    {
        read_pos = input_cache->cache_read_pos % MAX_CACHE_BLOCKS;
        cache_node = &input_cache->cache_buf[read_pos];
        rawdata_show("cache put",cache_node->data,cache_node->datalen);
        Nanosic_input_write(cache_node->type,cache_node->data,cache_node->datalen);
        input_cache->cache_read_pos ++;
        input_cache->cache_cnt--;
    }
    
    spin_unlock(&input_cache->cache_spinlock);
    return;
}

/** **************************************************************************
 * @func  Nanosic_cache_put
 * 
 * @brief Handler for put data from cache buffer
 * 
 ** */
int 
Nanosic_cache_put(void)
{
    input_cache_t* input_cache = &gInputCache;
    cahae_buffer_t* cache_node;
    __u8 read_pos;
        
    if(IS_ERR_OR_NULL(input_cache)){
        dbgprint(DEBUG_LEVEL, "invalid cache struct\n");
        return 0;
    }

    if(timer_pending(&input_cache->cache_timer))
        del_timer_sync(&input_cache->cache_timer);

    spin_lock(&input_cache->cache_spinlock);
    
    while(input_cache->cache_cnt)
    {
        read_pos = input_cache->cache_read_pos % MAX_CACHE_BLOCKS;
        cache_node = &input_cache->cache_buf[read_pos];
        rawdata_show("cache put",cache_node->data,cache_node->datalen);
        Nanosic_input_write(cache_node->type,cache_node->data,cache_node->datalen);
        input_cache->cache_read_pos ++;
        input_cache->cache_cnt--;
    }
    
    spin_unlock(&input_cache->cache_spinlock);
    return 0;
}

/** **************************************************************************
 * @func  Nanosic_cache_insert
 * 
 * @brief Handler for insert data to fifo buffer
 * 
 ** */
int 
Nanosic_cache_insert(EM_PacketType type ,void *data, size_t datalen)
{
    input_cache_t* input_cache = &gInputCache;
    __u8 write_pos;
    //int length;
    int ret = 0;

    if(!data)
        return -1;
    
    rawdata_show("cache insert",data,datalen);

    spin_lock(&input_cache->cache_spinlock);

    if (input_cache->cache_cnt < MAX_CACHE_BLOCKS)
    {
        write_pos = input_cache->cache_write_pos % MAX_CACHE_BLOCKS;
        dbgprint(DEBUG_LEVEL, "cache write_pos %d\n",write_pos);
        memcpy(input_cache->cache_buf[write_pos].data , data , datalen);
        input_cache->cache_buf[write_pos].datalen = datalen;
        input_cache->cache_buf[write_pos].type = type;
        input_cache->cache_write_pos ++;
        input_cache->cache_cnt ++;
        mod_timer(&input_cache->cache_timer,jiffies + 2*HZ);
        dbgprint(DEBUG_LEVEL, "cache time update\n");
    } else {
        ret = -1;
        dbgprint(DEBUG_LEVEL, "cache is full\n");
    }
    spin_unlock(&input_cache->cache_spinlock);

    return ret;
}

/** **************************************************************************
 * @func  Nanosic_cache_init
 * 
 * @brief Handler for init cache
 * 
 ** */
int 
Nanosic_cache_init(void)
{
    input_cache_t* input_cache = &gInputCache;

    input_cache->cache_read_pos = 0;
    input_cache->cache_write_pos = 0;
    input_cache->cache_cnt = 0;
    spin_lock_init(&input_cache->cache_spinlock);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
    setup_timer(&input_cache->cache_timer, Nanosic_cache_expire, 0);
#else
    timer_setup(&input_cache->cache_timer, Nanosic_cache_expire, 0);
#endif
	dbgprint(DEBUG_LEVEL,"cache init ok\n");

    return 0;
}

/** ************************************************************************//**
 *  @func Nanosic_cache_release
 * 
 *  @brief
 *
 ** */
int 
Nanosic_cache_release(void)
{
    input_cache_t* input_cache = &gInputCache;

    if(timer_pending(&input_cache->cache_timer)){
        del_timer(&input_cache->cache_timer);
    }
	dbgprint(DEBUG_LEVEL,"cache release ok\n");
	return 0;
}

