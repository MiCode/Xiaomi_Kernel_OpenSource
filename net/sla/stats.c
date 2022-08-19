/*
    Copyright (c) 2018, The Linux Foundation. All rights reserved.

    This program is free software; you can redistribute it and/or modify it under the terms
    of the GNU General Public License version 2 and only version 2 as published by the Free
    Software Foundation.

    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
    without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU General Public License for more details.
*/

#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/tcp.h>
#include <linux/ip.h>
#include <linux/version.h>
#include <linux/skbuff.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/moduleparam.h>
#include <linux/spinlock.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/hash.h>
#include <linux/rculist.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/list.h>

#include "sla_stats.h"

/* about fs config */
#define USE_PROCFS

/* about stats */
#define STATS_BUF_SIZE (sizeof(struct sla_interface_stats)) //16*(1+ENTRY_TOTAL) = (20*1024)

#define MMAP_FILE_SUFFIX "_mmap"
#define TOTAL_STATS_FILE_SUFFIX "_stats"

static struct sla_config config_data;
static u64 pkt_alloc_num = 0;
static u64 pkt_free_num = 0;


LIST_HEAD(sla_iface_list) ;


/* about port hash list */
#define PORT_HASH_BITS 8
#define PORT_HASH_SIZE (1<<PORT_HASH_BITS)
#define MAX_SIZE_DEFAULT    (4*1024)
#define LIMIT_TIME_DEFAULT  (5)

#define STATS_LOCK_IRQ spin_lock_irqsave
#define STATS_UNLOCK_IRQ spin_unlock_irqrestore

struct uplink_pkt
{
    struct hlist_node   hlist;
    u16 local_port;
};

struct hlist_head   pkt_list[PORT_HASH_SIZE];


/* about fs */

static int procfs_u64_set(void *data, u64 val)
{
    *(u64 *)data = val;
    return 0;
}

static int procfs_u64_get(void *data, u64 *val)
{
    *val = *(u64 *)data;
    return 0;
}

static int fops_u64_open(struct inode *inode, struct file *filp)
{
    inode->i_private = PDE_DATA(inode);
    return simple_attr_open(inode, filp, procfs_u64_get, procfs_u64_set, "%llu\n");
}

static const struct file_operations fops_u64 =
{
    .owner   = THIS_MODULE,
    .open    = fops_u64_open,
    .release = simple_attr_release,
    .read    = simple_attr_read,
    .write   = simple_attr_write,
    .llseek  = generic_file_llseek,
};


/* dir */
static struct proc_dir_entry *sla_dir;
/* config file */
static struct proc_dir_entry *config_file; //Used for SLA config: enable, prots, reset_traffic and so on

static struct sla_interface * sla_find_interface(const char *interface_name);

/* about lock */
static spinlock_t stats_spin_lock;

static inline void stats_lock_init(void)
{
    spin_lock_init(&stats_spin_lock);
}

static inline void stats_lock(void)
{
    spin_lock(&stats_spin_lock);

}

static inline void stats_unlock(void)
{
    spin_unlock(&stats_spin_lock);
    
}

static spinlock_t pkt_spin_lock;

static inline void pkt_lock_init(void)
{
    spin_lock_init(&pkt_spin_lock);
}

static inline void pkt_lock(void)
{
    spin_lock(&pkt_spin_lock);

}

static inline void pkt_unlock(void)
{
    spin_unlock(&pkt_spin_lock);
}

static spinlock_t list_spin_lock;

static inline void list_lock_init(void)
{
    spin_lock_init(&list_spin_lock);
}

static inline struct hlist_head *pkt_hlist_head(u16 port)
{
    return &pkt_list[hash_32(port, PORT_HASH_BITS)];
}

static struct uplink_pkt *pkt_find(u16 port)
{
    struct uplink_pkt *pkt;

    hlist_for_each_entry_rcu(pkt, pkt_hlist_head(port), hlist)
    {
        if (pkt->local_port == port)
            return pkt;
    }

    return NULL;
}

static void pkt_add(u16 port)
{
    struct uplink_pkt *pkt;

    if(pkt_find(port))
        return;

    pkt = kzalloc(sizeof(*pkt), GFP_ATOMIC);
    if (!pkt)
        return;

    pkt->local_port = port;
    spin_lock(&list_spin_lock);
    ++ pkt_alloc_num;
    hlist_add_head_rcu(&pkt->hlist, pkt_hlist_head(port));
    spin_unlock(&list_spin_lock);
}

static void pkt_release(struct uplink_pkt *pkt)
{
    spin_lock(&list_spin_lock);
    ++ pkt_free_num;
    hlist_del_rcu(&pkt->hlist);
    spin_unlock(&list_spin_lock);
    kfree(pkt);
}


static void pkt_hlist_init(void)
{
    unsigned int h;

    for (h = 0; h < PORT_HASH_SIZE; h++)
        INIT_HLIST_HEAD(&pkt_list[h]);
}

static void pkt_hlist_deinit(void)
{
    struct uplink_pkt *pkt;
    struct uplink_pkt *del;
    unsigned int h;

    for (h = 0; h < PORT_HASH_SIZE; h++)
    {
        del = NULL;
        hlist_for_each_entry_rcu(pkt, &pkt_list[h], hlist)
        {
            if(del)
                pkt_release(del);

            del = pkt;
        }

        if(del)
            pkt_release(del);
    }
}

static void pkt_hlist_print(void)
{
    struct uplink_pkt *pkt;
    unsigned int h;

    for (h = 0; h < PORT_HASH_SIZE; h++)
    {
        hlist_for_each_entry_rcu(pkt, &pkt_list[h], hlist)
            printk("%s() pkt_list[%d] local_port=%d\n", __func__, h, pkt->local_port);
    }
}


static bool sla_pkt_valid(struct sk_buff *skb, u8 link_type)
{
    int i;
    u16 local_port;
    u16 ext_port;
    u16 len;
    struct tcphdr *tcph;
    struct uplink_pkt *pkt;

    if(IPPROTO_TCP != ip_hdr(skb)->protocol)
        return false;

    if(LT_UPLINK == link_type)
    {
        local_port = ntohs(tcp_hdr(skb)->source);
        ext_port = ntohs(tcp_hdr(skb)->dest);
    }
    else
    {
        local_port = ntohs(tcp_hdr(skb)->dest);
        ext_port = ntohs(tcp_hdr(skb)->source);
    }

    for(i=0; (i<PORTS_MAX) && (0!=config_data.ports[i]); i++)
    {
        if(ext_port == config_data.ports[i])
            break;
    }

    if((i == PORTS_MAX) || ext_port != config_data.ports[i])
    {
        return false;
    }

    len = ntohs(ip_hdr(skb)->tot_len) - (ip_hdr(skb)->ihl*4 + tcp_hdrlen(skb));
    if(0 == len)
    {
        tcph = tcp_hdr(skb);
        if(tcph->fin || tcph->rst)
        {
            pkt_lock();
            pkt = pkt_find(local_port);
            if(pkt)
            {
                pkt_release(pkt);
            }
            pkt_unlock();
        }
        return false;
    }

    return true;
}

static bool sla_pkt_consolidate(struct sla_interface_stats *stats)
{
    u32 prev;
    struct sla_stats_entry* entry = &stats->entries[stats->cur_idx];

    if(0 == stats->cur_idx)
        prev = stats->total;
    else
        prev = stats->cur_idx - 1;

    if(stats->entries[prev].discard)
        return false;

    if(stats->entries[prev].port != entry->port)
        return false;

    if((stats->entries[prev].timestamp + config_data.limit_time) < entry->timestamp)
        return false;

    if((stats->entries[prev].size + entry->size) > config_data.max_size)
        return false;

    stats->entries[prev].size += entry->size;

    return true;
}



static unsigned int sla_ipv4_in(void *priv,
                                struct sk_buff *skb,
                                const struct nf_hook_state *state)
{
    struct sla_interface_stats *p;
    struct sla_stats_entry *entry;
    struct timespec tm;
    struct uplink_pkt *pkt;
    bool consolidated = false;
    struct sla_interface* iface = NULL;
    unsigned long flags;

    if(config_data.enable && config_data.rate_on)
    {
        if(!sla_pkt_valid(skb, LT_DOWNLINK))
            return NF_ACCEPT;


        //printk("#### SLA sla_ipv4_in ");
        //printk("#### %s devname=[%s] mac len=%d skb->len=%d ifaceid = ((%d)) \n", __func__, state->in->name, skb->mac_len, skb->len, state->in->ifindex);

        STATS_LOCK_IRQ(&stats_spin_lock,flags);
        iface =  sla_find_interface(state->in->name);

        if(iface == NULL)
        {
            STATS_UNLOCK_IRQ(&stats_spin_lock,flags);
            return NF_ACCEPT;
          
        }
        p = iface->mmap_struct;
        
        iface->total_trfc += skb->len;
        
        tm = current_kernel_time();
        
        entry = &p->entries[p->cur_idx];
        entry->link_type = LT_DOWNLINK;
        entry->timestamp = tm.tv_sec*1000 + tm.tv_nsec/1000000;
        entry->size = skb->len + 14; //14 is mac len
        entry->port = ntohs(tcp_hdr(skb)->dest);

        pkt_lock();
        pkt = pkt_find(entry->port);
        if(pkt)
        {
            pkt_release(pkt);

            entry->discard = 1;
        }
        else
        {
            entry->discard = 0;
            consolidated = sla_pkt_consolidate(p);
        }
        pkt_unlock();

        if(!consolidated)
        {
            p->cur_idx++;
            if(p->cur_idx >= p->total)
                p->cur_idx = 0;
        }
        else
            memset(entry, 0, sizeof(*entry));

        STATS_UNLOCK_IRQ(&stats_spin_lock,flags);
    }

    return NF_ACCEPT;
}

static unsigned int sla_ipv4_out(void *priv,
                                 struct sk_buff *skb,
                                 const struct nf_hook_state *state)
{
    struct sla_interface_stats *p;
    struct sla_stats_entry *entry;
    struct timespec tm;
    struct sla_interface* iface = NULL;
    unsigned long flags;

    if(config_data.enable && config_data.rate_on)
    {
        if(!sla_pkt_valid(skb, LT_UPLINK))
            return NF_ACCEPT;

        //printk("#### SLA sla_ipv4_out ");
        //printk("#### %s devname=[%s] mac-len=%d skb->len=%d ifaceid = ((%d)) \n", __func__, state->out->name, skb->mac_len, skb->len, state->out->ifindex);
        STATS_LOCK_IRQ(&stats_spin_lock,flags);
        iface =  sla_find_interface(state->out->name);

        if(iface == NULL)
        {
            STATS_UNLOCK_IRQ(&stats_spin_lock,flags);
            return NF_ACCEPT;
          
        }
        p = iface->mmap_struct;
        
        tm = current_kernel_time();
        pkt_add(ntohs(tcp_hdr(skb)->source));

        
        entry = &p->entries[p->cur_idx];
        entry->link_type = LT_UPLINK;

        entry->timestamp = tm.tv_sec*1000 + tm.tv_nsec/1000000;
        entry->size = skb->len + 14; //14 is mac len
        entry->port = ntohs(tcp_hdr(skb)->source);
        entry->discard = 1;

        p->cur_idx++;
        if(p->cur_idx >= p->total)
            p->cur_idx = 0;

        STATS_UNLOCK_IRQ(&stats_spin_lock,flags);
    }

    return NF_ACCEPT;
}

static struct nf_hook_ops sla_ipv4_ops[] __read_mostly =
{
    {
        .hook       = sla_ipv4_in,
        .pf         = NFPROTO_IPV4,
        .hooknum    = NF_INET_PRE_ROUTING,
        .priority   = NF_IP_PRI_CONNTRACK+2,
    },
    {
        .hook       = sla_ipv4_out,
        .pf         = NFPROTO_IPV4,
        .hooknum    = NF_INET_POST_ROUTING,
        .priority   = NF_IP_PRI_LAST,
    },
};


static ssize_t stats_read(struct file *file, char __user *user_buf,
                          size_t count, loff_t *ppos)
{
    struct sla_interface_stats *stats = file->private_data;

    return simple_read_from_buffer(user_buf, count, ppos, stats, STATS_BUF_SIZE);
}

static int stats_map(struct file *filp, struct vm_area_struct *vma)
{
    unsigned long page;
    unsigned long start = (unsigned long)vma->vm_start;
    unsigned long size = (unsigned long)(vma->vm_end - vma->vm_start);

    if(!filp->private_data)
        return -1;

    page = virt_to_phys(filp->private_data); //get phys addr

    //Maps a vma virtual memory space in user space to a continuous physical page beginning with page
    //The third parameter is the page frame number, obtained by shifting the PAGE_SHIFT to the right of the physical address
    if(remap_pfn_range(vma,start,page>>PAGE_SHIFT,size,PAGE_SHARED))
        return -1;

    return 0;
}

int stats_open(struct inode *inode, struct file *file)
{
    inode->i_private = PDE_DATA(inode);

    return simple_open(inode, file);
}


static const struct file_operations stats_fops =
{
    .owner = THIS_MODULE,
    .open = stats_open,
    .read = stats_read,
    .llseek = default_llseek,
    .mmap = stats_map,
};

static void sla_stats_data_init(struct sla_interface_stats *p)
{
    memset(p, 0, STATS_BUF_SIZE);

    p->magic_num = STATS_MAGIC;
    p->total = ENTRY_TOTAL;
    p->cur_idx = 0;
    p->reserve = 0;
}

/**
 *  
 * @param interface_name 
 * 
 * @return struct sla_interface* 
 *  
 * This function itterates over sla_iface_list to find the 
 * interface structure that matches interface_name. 
 *  
 * The caller needs to acquire stats_lock() before calling the 
 * function and while using the returned sla_interface ptr. 
 *  
 * The caller also needs to release the lock by calling 
 * stats_unlock() after using ( reading or writing to) 
 * sla_interface ptr. 
 */
static struct sla_interface * sla_find_interface(const char *interface_name)
{
    struct sla_interface * iface = NULL;
    
    list_for_each_entry (iface , &sla_iface_list, interface_list_hook) 
    { 
         
         if(strcmp(interface_name,iface->interface_name ) == 0)
         {
             return iface;
         }
    }

    return NULL;
}

static int sla_interface_start(const char *interface_name)
{ 

    u32 offset;
    unsigned long flags;
    char mmap_file_name[IF_NAME_LEN + 5];
    char total_stats_file_name[IF_NAME_LEN + 6];
    struct sla_interface *new_interface;
    
    if(NULL == interface_name)
    {
      printk(KERN_CRIT "### %s Cannot create iface struct when iface name is NULL \n", __func__);
      return -1;

    }
    STATS_LOCK_IRQ(&stats_spin_lock,flags);
    if(sla_find_interface(interface_name) != NULL)
    {
        STATS_UNLOCK_IRQ(&stats_spin_lock,flags);
        printk(KERN_CRIT "### %s Interface %s already being tracked in SLA ", __func__, interface_name);
        return -1;

    }
    STATS_UNLOCK_IRQ(&stats_spin_lock,flags);

    new_interface = (struct sla_interface *)kmalloc(sizeof(struct sla_interface), GFP_ATOMIC);

    if(NULL == new_interface)
    {
        printk(KERN_CRIT "### %s kmalloc Failed for %s", __func__, interface_name);
        return -1;

    }

    if (strlcpy(new_interface->interface_name, interface_name , sizeof(new_interface->interface_name)) > IF_NAME_LEN)
    {
        // the interface_name passed to the function was longer than the MAX interface length.
        printk(KERN_CRIT "### %s invalid interface name %s", __func__, interface_name);
        kfree(new_interface);
        return -1;
    }
    printk(KERN_CRIT "### %s copy the iface name into struct %s \n", __func__, interface_name);

    strlcpy(mmap_file_name,new_interface->interface_name, sizeof(mmap_file_name));
    strlcat(mmap_file_name,MMAP_FILE_SUFFIX,sizeof(mmap_file_name));
    strlcpy(total_stats_file_name,new_interface->interface_name, sizeof(total_stats_file_name));
    strlcat(total_stats_file_name,TOTAL_STATS_FILE_SUFFIX,sizeof(total_stats_file_name));

    new_interface->mmap_struct = (struct sla_interface_stats *)kmalloc(STATS_BUF_SIZE, GFP_ATOMIC);

    if(NULL == new_interface->mmap_struct)
    {
        printk(KERN_CRIT "### %s kmalloc wlan_stats Fail\n", __func__);
        kfree(new_interface);
        return -1;
    }

    sla_stats_data_init(new_interface->mmap_struct);

    for(offset=0; offset<STATS_BUF_SIZE; offset+=PAGE_SIZE)
        SetPageReserved(virt_to_page(((u8 *)new_interface->mmap_struct)+offset)); //Set the segment memory to reserved

    

    new_interface->mmap_file = proc_create_data(mmap_file_name, 0644, sla_dir, &stats_fops, new_interface->mmap_struct);

    if (!new_interface->mmap_file)
    {
        printk(KERN_ERR "#### %s Fail to create file: mmap_file for interface= %s \n", __func__, interface_name);
        kfree(new_interface->mmap_struct);
        kfree(new_interface);
        return -1;
    }


    new_interface->total_trfc = 0;
    new_interface->total_stats_file = proc_create_data(total_stats_file_name, 0644, sla_dir, &fops_u64, &new_interface->total_trfc);

    if (!new_interface->total_stats_file)
    {
        printk(KERN_ERR "#### %s Fail to create total_stats_fils for interface = %s \n", __func__, interface_name);
        proc_remove(new_interface->mmap_file);
        kfree(new_interface->mmap_struct);
        kfree(new_interface);
        return 1;
    }

    STATS_LOCK_IRQ(&stats_spin_lock,flags);
    list_add(&new_interface->interface_list_hook, &sla_iface_list);
    STATS_UNLOCK_IRQ(&stats_spin_lock,flags);

    printk(KERN_CRIT "### %s was able to add  %s  %p \n", __func__, interface_name, new_interface);

    return 0;


}

   

static void sla_stats_disable(void)
{
    u32 offset = 0;
    struct sla_interface * iface = NULL;
    struct sla_interface * next = NULL;
    unsigned long flags;

    if(!config_data.enable)
    {
        printk("### %s disable already\n", __func__);
        return;
    }

    config_data.enable = 0;
    config_data.rate_on = 0;

    STATS_LOCK_IRQ(&stats_spin_lock,flags);
    
    list_for_each_entry_safe (iface,next,&sla_iface_list, interface_list_hook)
    {
       printk(KERN_CRIT "### %s trying to disable %s\n", __func__, iface->interface_name);
       iface->total_trfc = 0;
       if(iface->mmap_struct)
       {
          memset(iface->mmap_struct, 0, STATS_BUF_SIZE);

          for(offset=0; offset<STATS_BUF_SIZE; offset+=PAGE_SIZE)
               ClearPageReserved(virt_to_page(((u8 *)iface->mmap_struct)+offset));

          kfree(iface->mmap_struct);
          iface->mmap_struct = NULL;
       }

       if(iface->mmap_file)
       {
         proc_remove(iface->mmap_file);
       }

       if(iface->total_stats_file)
       {
         proc_remove(iface->total_stats_file);
       }

       printk(KERN_CRIT "### %s removing form list %s\n", __func__, iface->interface_name);

       list_del(&iface->interface_list_hook);
       printk(KERN_ERR "### %s removed form list %s %p \n", __func__, iface->interface_name, iface );

       kfree(iface);
    } 
    STATS_UNLOCK_IRQ(&stats_spin_lock,flags);

    printk(KERN_CRIT "### %s out of disable funk \n", __func__);

    pkt_hlist_deinit();
}




static void sla_stats_enable(void)
{
    if(config_data.enable)
    {
        printk("### %s enable already\n", __func__);
        return;
    }

    //sla_stats_disable();
    //sla_stats_remove_file(); 

    pkt_hlist_init();
    config_data.enable = 1;
    config_data.rate_on = 1;
}

static int config_file_show(struct seq_file *s, void *data)
{
    struct sla_config *config = (struct sla_config *)s->private;
    int i;

    seq_printf(s, CMD_ENABLE"%d\n", config->enable);
    seq_printf(s, CMD_RATE_ON"%d\n", config->rate_on);

    seq_puts(s, CMD_PORTS);
    if(0 != config->ports[0])
    {
        seq_printf(s, "%d", config->ports[0]);

        for(i=1; (i<PORTS_MAX) && (0!=config->ports[i]); i++)
            seq_printf(s, ",%d", config->ports[i]);
    }

    seq_printf(s, "\n");
    seq_printf(s, CMD_MAX_SIZE"%d\n", config->max_size);
    seq_printf(s, CMD_LIMIT_TIME"%d\n", config->limit_time);


    return 0;
}

static int config_file_open(struct inode *inode, struct file *filp)
{

    return single_open(filp, config_file_show, PDE_DATA(inode));

}

static ssize_t config_file_write(struct file *filp, const char __user *buffer,  size_t count, loff_t *ppos)
{
    char cmd[CONFIG_CMD_MAX];
    unsigned long num = 0;
    int len = 0;
    int i = 0;
    char *p = NULL;
    char *buf = NULL;

    if(count > CONFIG_CMD_MAX)
        return -EFAULT;

    if(copy_from_user(cmd, buffer, count))
        return -EFAULT;

    cmd[count-1] = 0;

    
    len = strlen(CMD_IFACE_ADD);
    if(0 == strncmp(cmd, CMD_IFACE_ADD, len))
    {
        printk("#### %s CMD_IFACE_ADD=[%s]\n", __func__, cmd+len);
        buf = cmd+len;
        if(strlen(buf) < IF_NAME_LEN)
            sla_interface_start((const char*)buf);
        else
            printk(KERN_ERR "#### %s ifname=[%s] is invaild\n", __func__, buf);

        return count;
    }

    len = strlen(CMD_ENABLE);
    if(0 == strncmp(cmd, CMD_ENABLE, len))
    {
        printk("#### %s enable=%c\n", __func__, cmd[len]);
        if('1' == cmd[len] && 0 == cmd[len+1])
        {
            sla_stats_enable();
        }
        else if('0' == cmd[len] && 0 == cmd[len+1])
        {
            sla_stats_disable();

        }

        return count;
    }

    len = strlen(CMD_RATE_ON);
    if(0 == strncmp(cmd, CMD_RATE_ON, len))
    {
        printk("#### %s rate_on=%c\n", __func__, cmd[len]);
        if('1' == cmd[len] && 0 == cmd[len+1])
            config_data.rate_on = 1;
        else if('0' == cmd[len] && 0 == cmd[len+1])
            config_data.rate_on = 0;

        return count;
    }

    len = strlen(CMD_PORTS);
    if(0 == strncmp(cmd, CMD_PORTS, len))
    {
        printk("#### %s ports=[%s]\n", __func__, cmd+len);
        buf = cmd+len;
        i = 0;
        while(NULL != (p = strsep(&buf, ",")))
        {
            num = simple_strtoul(p, NULL, 10);
            if((0 < num) && (num <= 0xFFFF))
            {
                config_data.ports[i] = num & 0xFFFF;
                printk("#### %s ports=[%d]\n", __func__, config_data.ports[i]);

                i++;
                if(i >= PORTS_MAX)
                    break;
            }
        }

        return count;
    }

    len = strlen(CMD_RESET_TRAFFIC);
    if(0 == strncmp(cmd, CMD_RESET_TRAFFIC, len))
    {
        if('1' == cmd[len] && 0 == cmd[len+1])
        {
            struct sla_interface * iface = NULL;
            unsigned long flags;
            STATS_LOCK_IRQ(&stats_spin_lock,flags);
            list_for_each_entry (iface , &sla_iface_list, interface_list_hook) 
            {
                iface->total_trfc = 0;

            }
            STATS_UNLOCK_IRQ(&stats_spin_lock,flags);
            printk("#### %s reset_traffic=%c\n", __func__, cmd[len]);

        }

        return count;
    }

    len = strlen(CMD_HASH_PRINT);
    if(0 == strncmp(cmd, CMD_HASH_PRINT, len))
    {
        printk("#### %s hash print %llu, %llu\n", __func__, pkt_alloc_num, pkt_free_num);
        pkt_hlist_print();

        return count;
    }

    len = strlen(CMD_HASH_CLEAR);
    if(0 == strncmp(cmd, CMD_HASH_CLEAR, len))
    {
        printk("#### %s hash clear\n", __func__);
        pkt_hlist_deinit();

        return count;
    }

    len = strlen(CMD_MAX_SIZE);
    if(0 == strncmp(cmd, CMD_MAX_SIZE, len))
    {
        num = simple_strtoul(&cmd[len], NULL, 10);
        if(num < MAX_SIZE_DEFAULT)
        {
            printk(KERN_ERR "#### %s max_size=%lu is invaild, need greater than %d\n", __func__, num, MAX_SIZE_DEFAULT);
        }
        else
        {
            config_data.max_size = num & 0xFFFF;
            printk("#### %s max_size=%d\n", __func__, config_data.max_size);
        }

        return count;
    }

    len = strlen(CMD_LIMIT_TIME);
    if(0 == strncmp(cmd, CMD_LIMIT_TIME, len))
    {
        num = simple_strtoul(&cmd[len], NULL, 10);
        if(num < LIMIT_TIME_DEFAULT)
        {
            printk(KERN_ERR "#### %s limit_time=%lu is invaild, need longer than %d\n", __func__, num, LIMIT_TIME_DEFAULT);
        }
        else
        {
            config_data.limit_time = num & 0xFFFF;
            printk("#### %s limit_time=%d\n", __func__, config_data.limit_time);
        }

        return count;
    }



    return count;
}


static const struct file_operations config_fops =
{
    .owner = THIS_MODULE,
    .open = config_file_open,
    .read = seq_read,
    .write = config_file_write,
    .llseek = seq_lseek,
    .release = single_release,
};


static int __init sla_static_collector_init(void)
{
    int err = 0;
    printk("#### %s in version V 3.0 \n", __func__);

    err = nf_register_net_hooks(&init_net, sla_ipv4_ops, ARRAY_SIZE(sla_ipv4_ops));
    if (err < 0)
    {
        printk(KERN_ERR "%s: can't register hooks.\n", __func__);
        return err;
    }

    list_lock_init();
    stats_lock_init();
    pkt_lock_init();


    sla_dir = proc_mkdir("sla", NULL);

    if (!sla_dir)
    {
        printk(KERN_ERR "#### %s Fail to create sla dir\n", __func__);
        return 1;
    }

    //creat config
    memset(&config_data, 0, sizeof(config_data));
    config_data.ports[0] = 80; //default
    config_data.ports[1] = 443;
    config_data.max_size = MAX_SIZE_DEFAULT;
    config_data.limit_time = LIMIT_TIME_DEFAULT;


    config_file = proc_create_data("config", 0644, sla_dir, &config_fops, &config_data);

    if (!config_file)
    {
        printk(KERN_ERR "#### %s Fail to create file: config_file\n", __func__);
        return 1;
    }


    return 0;
}

static void __exit sla_static_collector_deinit(void)
{
    printk("#### sla_static_collector_deinit in\n");
    nf_unregister_net_hooks(&init_net, sla_ipv4_ops, ARRAY_SIZE(sla_ipv4_ops));

    sla_stats_disable();
    proc_remove(sla_dir);

}

MODULE_LICENSE("GPL v2");
module_init(sla_static_collector_init);
module_exit(sla_static_collector_deinit);
