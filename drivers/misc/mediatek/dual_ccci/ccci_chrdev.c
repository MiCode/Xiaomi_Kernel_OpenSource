#include <linux/module.h>
#include <linux/wakelock.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <ccci_chrdev.h>
#include <ccci.h>

extern unsigned long long lg_ch_tx_debug_enable[];
extern unsigned long long lg_ch_rx_debug_enable[];
//extern unsigned int md_ex_type;
//unsigned int push_data_fail = 0;
unsigned int get_sim_switch_type(void);

static chr_ctl_block_t *chr_ctlb[MAX_MD_NUM];
static struct wake_lock    chrdev_wakelock[MAX_MD_NUM];
static struct wake_lock    chrdev_wakelock_mdlogger[MAX_MD_NUM];
char chrdev_wakelock_name[MAX_MD_NUM][32];
char chrdev_wakelock_mdlog_name[MAX_MD_NUM][32];
unsigned int        md_img_exist[MD_IMG_MAX_CNT] = {0};
unsigned int        md_type_saving = 0;

unsigned int curr_sim_mode[MAX_MD_NUM];

struct ccci_dev_client  *md_logger_client = NULL;
static spinlock_t    md_logger_lock;
static unsigned int    catch_more = 0;


#ifdef CONFIG_MTK_MD_SBP_CUSTOM_VALUE
static unsigned int md_sbp_code = 0;
static unsigned int md_sbp_code_default = 0;
#endif //CONFIG_MTK_MD_SBP_CUSTOM_VALUE

unsigned int __weak get_sim_switch_type(void)
{
    CCCI_MSG("%s is not implement!!! line:%d\n", __FUNCTION__, __LINE__);;
    return 0;
}

int ccci_misc_ipo_h_restore(int md_id)
{
    int reset_mode = (0x5AA5<<16);
    
    if(curr_sim_mode[md_id] != -1) {
        exec_ccci_kern_func(ID_SSW_SWITCH_MODE, (char*)(&reset_mode), sizeof(unsigned int));
        CCCI_MSG_INF(md_id, "chr", "restore sim mode to %08x\n", curr_sim_mode[md_id]);
        exec_ccci_kern_func(ID_SSW_SWITCH_MODE, (char*)(&curr_sim_mode[md_id]), sizeof(unsigned int));
    }
    return 0;
}

//==============================================================
// CCCI Standard charactor device function
//==============================================================
static void ccci_client_init(struct ccci_dev_client *client,int ch,pid_t pid)
{
    WARN_ON(client==NULL);
    spin_lock_init(&client->lock);
    atomic_set(&client->user,1);
    client->pid=pid;
    client->ch_num=ch;
    INIT_LIST_HEAD(&client->dev_list);
    init_waitqueue_head(&client->wait_q);
    client->fasync=NULL;
    client->wakeup_waitq=0;
}


static void release_client(struct ccci_dev_client *client)
{
    unsigned long flags;
    chr_ctl_block_t *ctlb = (chr_ctl_block_t *)client->ctlb;
    WARN_ON(spin_is_locked(&client->lock)||list_empty(&client->dev_list));
    mutex_lock(&ctlb->chr_dev_mutex);
    if(client->ch_num == CCCI_MD_LOG_RX) {
        spin_lock_irqsave(&md_logger_lock, flags);
        md_logger_client = NULL;
        spin_unlock_irqrestore(&md_logger_lock, flags);
    }
    list_del(&client->dev_list);
    un_register_to_logic_ch(client->md_id, client->ch_num);
    kfree(client);
    mutex_unlock(&ctlb->chr_dev_mutex);
}

static inline void ccci_put_client(struct ccci_dev_client *client)
{
    WARN_ON(client==NULL);
    if (atomic_dec_and_test(&client->user))
        release_client(client);
}


static void ccci_chrdev_callback(void *private)
{
    logic_channel_info_t *ch_info = (logic_channel_info_t*)private;
    struct ccci_dev_client *client=(struct ccci_dev_client *)(ch_info->m_owner);

    client->wakeup_waitq = 1;
    wake_up_interruptible(&client->wait_q);
    if(client->ch_num != CCCI_MD_LOG_RX)
        wake_lock_timeout(&chrdev_wakelock[client->md_id], HZ/2);
    else
        wake_lock_timeout(&chrdev_wakelock_mdlogger[client->md_id], HZ); // MD logger using 1s wake lock

    kill_fasync(&client->fasync, SIGIO, POLL_IN);
    return ;
}

static struct ccci_dev_client *find_get_client(int md_id, int ch, pid_t pid)
{
    struct ccci_dev_client    *client=NULL;
    int                        ret;
    chr_ctl_block_t            *ctlb = chr_ctlb[md_id];

    //schedule_timeout(10*HZ);
    mutex_lock(&ctlb->chr_dev_mutex);

    list_for_each_entry(client,&ctlb->chr_dev_list,dev_list)
    {
        if (client->ch_num == ch)
        {
            atomic_inc(&client->user);
            break ;
        }
    }
    if (&client->dev_list==&ctlb->chr_dev_list)
    {
        CCCI_CHR_MSG(md_id, "Create a Client for CH%d\n",ch);
        client = kmalloc(sizeof(*client),GFP_KERNEL);
        if (client==NULL)
        {
            CCCI_MSG_INF(md_id, "chr", "kmalloc for create client fail \n");
            client = ERR_PTR(-ENOMEM);
            goto out;
        }

        ccci_client_init(client,ch,pid);
        client->md_id = md_id;
        client->ctlb = ctlb;
        list_add(&client->dev_list,&ctlb->chr_dev_list);

        ret = register_to_logic_ch(md_id, ch, ccci_chrdev_callback, client);
        if (ret)
        {
            //CCCI_MSG_INF(md_id, "chr", "register ch fail: %d \n",ret);
            kfree(client);
            client = ERR_PTR(ret);
            goto out;
        }
    }

out:
    mutex_unlock(&ctlb->chr_dev_mutex);
    return client;
}


static int ccci_dev_open(struct inode *inode, struct file *file)
{
    int minor = iminor(inode);
    int major = imajor(inode);
    int index, minor_start;
    int md_id;
    int ret = 0;
    struct ccci_dev_client *client = NULL;

    ret = get_md_id_by_dev_major(major);
    if(ret < 0) {
        CCCI_MSG("[Error]invalid md sys id: %d\n", ret);
        goto out;
    }
    md_id = ret;
    
    ret = get_dev_id_by_md_id(md_id, "std chr", NULL, &minor_start);
    if(ret < 0){
        CCCI_MSG_INF(md_id, "chr", "get minor start fail(%d)\n", ret);
        goto out;
    }
    index = minor - minor_start;

    client = find_get_client(md_id, index, current->pid);
    CCCI_CHR_MSG(md_id, "Open by %s ch:%d\n", current->comm,index);
    if (IS_ERR(client))
    {
        CCCI_MSG_INF(md_id, "chr", "find client fail\n");
        ret = PTR_ERR(client);
        goto out;
    }
    file->private_data=client;
    if(index == CCCI_MD_LOG_RX) {
        md_logger_client = client;
        catch_more = 0;
    }

    nonseekable_open(inode,file);
out:
    return ret;

}


static int ccci_dev_release(struct inode *inode, struct file *file)
{
    struct ccci_dev_client *client=(struct ccci_dev_client *)file->private_data;
    ccci_put_client(client);
    return 0;
}


static int ccci_dev_fasync(int fd, struct file *file, int on)
{
    struct ccci_dev_client *client=(struct ccci_dev_client *)file->private_data;
    return fasync_helper(fd,file,on,&client->fasync);
}


static unsigned int ccci_dev_poll(struct file *file,poll_table *wait)
{
    struct ccci_dev_client    *client=(struct ccci_dev_client *)file->private_data;
    int                        ret=0;
    logic_channel_info_t    *ch_info;
    unsigned long            flags;

    ch_info = get_logic_ch_info(client->md_id, client->ch_num);

    poll_wait(file,&client->wait_q,wait);
    spin_lock_irqsave(&client->lock, flags);
    if ( ch_info && get_logic_ch_data_len(ch_info))
    {
         ret |= POLLIN|POLLRDNORM;
    }
    if( (client->ch_num == CCCI_MD_LOG_RX) && (catch_more) ) {
        CCCI_MSG_INF(0, "chr", "add poll error\n");
        catch_more = 0;
        ret |= POLLERR;
    }

    spin_unlock_irqrestore(&client->lock, flags);
    return ret;
}


static ssize_t ccci_dev_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    struct ccci_dev_client *client=(struct ccci_dev_client *)file->private_data;
    int md_id = client->md_id;
    int i=count/sizeof(ccci_msg_t);
    int j=0;
    int ret=0;
    int ch=client->ch_num;
    ccci_msg_t *buff = NULL;
    ccci_msg_t msg;
    WARN_ON(count%sizeof(ccci_msg_t));

    if (!buf || (i < 1))
    {
        CCCI_MSG_INF(md_id, "chr", "count:%d, i:%d, buf:0x%x \n", count, i, ((unsigned int) buf));
        ret=-EINVAL;
        goto out;
    }

    buff = (ccci_msg_t *)kmalloc(i*sizeof(ccci_msg_t),GFP_KERNEL);
    if (buff==NULL)
    {
        CCCI_MSG_INF(md_id, "chr", "kmalloc for ccci_msg_t fail \n");
        ret=-ENOMEM;
        goto out;
    }
    if (copy_from_user(buff,buf,i*sizeof(ccci_msg_t)))
    {
        CCCI_MSG_INF(md_id, "chr", "ccci_dev_write: copy from user fail \n");
        ret=-EFAULT;
        goto out_free;
    }

    if(lg_ch_tx_debug_enable[md_id] & (1ULL<<buff->channel)) {
        CCCI_MSG_INF(md_id, "chr", "ccci_dev_write: PID: %d, client: %p, lg_ch: %d\n", 
            client->pid, client, buff->channel);
    }

    for (j=0;j<i;j++)
    {
        //ret=ccci_write(ch,buff+j);
        msg.magic = buff[j].data0;
        msg.id = buff[j].data1;
        msg.channel = ch;
        msg.reserved = buff[j].reserved;
        CCCI_CHR_MSG(md_id, "msg: %08X %08X %02d %08X \n",msg.magic, msg.id, msg.channel, msg.reserved);

        ret = ccci_message_send(md_id, &msg, 1);
        if (ret<0)
        {
            CCCI_MSG_INF(md_id, "chr", "ccci_write fail: %d \n",ret);
            break;
        }
    }
    if (j)
    {
        ret=sizeof(*buff)*j;
    }

out_free:
    kfree(buff);
out:
    return ret;

}

static ssize_t ccci_dev_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    struct ccci_dev_client *client=(struct ccci_dev_client *)file->private_data;
    int ret=0,i=0;
    ccci_msg_t buff,*u_buff=(ccci_msg_t *)buf;
    int value, n;
    logic_channel_info_t    *ch_info;
    int md_id;

    n = count/sizeof(ccci_msg_t);
    WARN_ON(client==NULL);
    WARN_ON(count%sizeof(ccci_msg_t));

    md_id = client->md_id;
    ch_info = get_logic_ch_info(client->md_id, client->ch_num);

retry:

    for (;i<n;i++)
    {
        spin_lock_bh(&client->lock);
        ret = get_logic_ch_data(ch_info, &buff);
        spin_unlock_bh(&client->lock);

        if (ret==sizeof(ccci_msg_t)) {
            if (copy_to_user(u_buff+i,&buff,sizeof(ccci_msg_t))) {
                CCCI_MSG_INF(md_id, "chr", "read: [%d:%s]copy_to_user fail: %08X, %08X, %02d, %08X\n", 
                    client->pid, current->comm, buff.data0, buff.data1, buff.channel, buff.reserved);
                ret= -EFAULT;
                break;
            }

            if(lg_ch_rx_debug_enable[md_id] & (1ULL<<buff.channel)) {
                CCCI_MSG_INF(md_id, "chr", "read: [%d:%s] %08X, %08X, %02d, %08X\n", 
                    client->pid, current->comm, 
                    buff.data0, buff.data1, buff.channel, buff.reserved);
            }
        } else {
            if (file->f_flags & O_NONBLOCK) {    
                ret=-EAGAIN;
                goto out;
            }

            value = wait_event_interruptible(client->wait_q, client->wakeup_waitq);
            if(value == -ERESTARTSYS) {
                CCCI_CHR_MSG(md_id, "Interrupted syscall.signal_pend=0x%llx\n",
                    *(long long *)current->pending.signal.sig);
                ret = -EINTR;
                goto  out;
            }
            else if(value == 0) {
                client->wakeup_waitq = 0;
                if(lg_ch_rx_debug_enable[md_id] & (1ULL<<buff.channel)) {
                    CCCI_MSG_INF(md_id, "chr", "exit from schedule(%d):[%d:%s] \n", 
                        client->ch_num, current->pid, current->comm);
                }
                goto retry;
            }
        }
    }

out:
    //finish_wait(&client->wait_q,&wait);
    if (i) {
        ret=i*sizeof(ccci_msg_t);
    }

//    spin_unlock_bh(&client->lock);
    return ret;
}

static long ccci_dev_ioctl( struct file *file, unsigned int cmd, unsigned long arg)
{
    int            addr, len, state, ret = 0;
    struct ccci_dev_client *client=(struct ccci_dev_client *)file->private_data;
    int         md_id = client->md_id;
    int            ch = client->ch_num;

    switch (cmd) 
    {
        case CCCI_IOC_GET_MD_PROTOCOL_TYPE:
            {
                char md_protol[] = "AP_TST";
                unsigned int data_size = sizeof(md_protol) / sizeof(char);

                CCCI_MSG_INF(md_id, "chr", "Call CCCI_IOC_GET_MD_PROTOCOL_TYPE!\n");

                if (copy_to_user((void __user *)arg, md_protol, data_size)) 
                {
                    CCCI_MSG_INF(md_id, "chr", "copy_to_user MD_PROTOCOL failed !!\n");
                    return -EFAULT;
                }

                break;
            }

        case CCCI_IOC_GET_MD_STATE:
            state = get_curr_md_state(md_id);
            if(state >= 0) { 
                //CCCI_DBG_MSG(md_id, "chr", "MD%d state %d\n", md_id+1, state);
                state+='0'; // Make number to charactor
                ret = put_user((unsigned int)state, (unsigned int __user *)arg);
            } else {
                CCCI_MSG_INF(md_id, "chr", "Get MD%d state fail: %d\n", md_id+1, state);
                ret = state;
            }
            break;

        case CCCI_IOC_PCM_BASE_ADDR:
            if( (ch == CCCI_PCM_RX) || (ch == CCCI_PCM_TX) ) {
                ccci_pcm_base_req(md_id, NULL, &addr, &len);
                //audio used this address in ap side, so return ap view
                ret = put_user((unsigned int)addr, (unsigned int __user *)arg);
            } else {
                CCCI_MSG_INF(md_id, "chr", "get PCM base fail: invalid user(%d) \n", ch);
                ret = -1;
            }
            break;

        case CCCI_IOC_PCM_LEN:
            if( (ch == CCCI_PCM_RX) || (ch == CCCI_PCM_TX) ) {
                ccci_pcm_base_req(md_id, NULL, &addr, &len);
                ret = put_user((unsigned int)len, (unsigned int __user *)arg);
            } else {
                CCCI_MSG_INF(md_id, "chr", "get PCM len fail: invalid user(%d)\n", ch);
                ret = -1;
            }
            break;

        case CCCI_IOC_ALLOC_MD_LOG_MEM:
            if( (ch == CCCI_MD_LOG_RX) || (ch == CCCI_MD_LOG_TX) ) {
                ccci_mdlog_base_req(md_id, NULL, &addr, &len);
                //mdlogger send this address to md, so return md view
                addr -=    get_md2_ap_phy_addr_fixed();
                ret = addr;
            } else {
                CCCI_MSG_INF(md_id, "chr", "get MD log base fail: invalid user(%d)\n", ch);
                ret = -1;
            }
            break;

        case CCCI_IOC_MD_RESET:
            CCCI_MSG_INF(md_id, "chr", "MD reset ioctl(%d) called by %s\n", ch, current->comm);
            ret = send_md_reset_notify(md_id);
            break;

        case CCCI_IOC_FORCE_MD_ASSERT:
            CCCI_MSG_INF(md_id, "chr", "Force MD assert ioctl(%d) called by %s\n", ch, current->comm);
            ret = ccci_trigger_md_assert(md_id);
            break;

        default:
            CCCI_MSG_INF(md_id, "chr", "illegal IOCTL %X called by %s\n", cmd, current->comm);
            ret = -ENOTTY;
            break;
    }

    //  CCCI_DEBUG("ret=%d cmd=0x%x addr=%0x len=%d\n",ret,cmd,addr,len);
    return ret;
}

static int ccci_dev_mmap(struct file *file, struct vm_area_struct *vma)
{
    int                pfn, len = 0; 
    unsigned long    addr;
    struct ccci_dev_client *client=(struct ccci_dev_client *)file->private_data;
    int                md_id = client->md_id;

    /* only PCM buffer for PCM channels can be mapped */
    if (client->ch_num == CCCI_PCM_RX || client->ch_num == CCCI_PCM_TX) {
        ccci_pcm_base_req(md_id, NULL, &addr, &len);
    } else if (client->ch_num==CCCI_MD_LOG_RX||client->ch_num==CCCI_MD_LOG_TX) {
        ccci_mdlog_base_req(md_id, NULL, &addr, &len);
    }

    CCCI_CHR_MSG(md_id, "remap addr:0x%lx len:%d  map-len:%lu\n",addr,len,vma->vm_end-vma->vm_start);
    if ((vma->vm_end-vma->vm_start)> len) {
        CCCI_DBG_MSG(md_id, "chr", "Get invalid mm size request from ch%d!\n", client->ch_num);
        return -1;  // mmap return -1 when fail
    }

    len=(vma->vm_end-vma->vm_start)<len?vma->vm_end-vma->vm_start:len;
    pfn = addr;
    pfn >>= PAGE_SHIFT;
    /* ensure that memory does not get swapped to disk */
    vma->vm_flags |= VM_IO;
    /* ensure non-cacheable */
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    if (remap_pfn_range(vma, vma->vm_start, pfn,len, vma->vm_page_prot)) {
        return -EAGAIN;
    }

    return 0;
}


static struct file_operations ccci_chrdev_fops=
{
    .owner = THIS_MODULE,
    .open = ccci_dev_open,
    .read = ccci_dev_read,
    .write = ccci_dev_write,
    .release = ccci_dev_release,
    .unlocked_ioctl = ccci_dev_ioctl,
    .fasync = ccci_dev_fasync,
    .poll = ccci_dev_poll,
    .mmap = ccci_dev_mmap,

};

int ccci_chrdev_init(int md_id)
{
    int  ret=0;
    int  major,minor;
    char name[16];
    chr_ctl_block_t *ctlb;

    ctlb = (chr_ctl_block_t *)kmalloc(sizeof(chr_ctl_block_t), GFP_KERNEL);
    if(ctlb == NULL) {
        ret = -CCCI_ERR_GET_MEM_FAIL;
        goto out;
    }

    // Init control struct
    memset(ctlb, 0, sizeof(chr_ctl_block_t));
    mutex_init(&ctlb->chr_dev_mutex);
    ctlb->chr_dev_list.next = &ctlb->chr_dev_list;
    ctlb->chr_dev_list.prev = &ctlb->chr_dev_list;
    ctlb->md_id = md_id;

    ret = get_dev_id_by_md_id(md_id, "std chr", &major, &minor);
    if (ret < 0) {
        CCCI_MSG_INF(md_id, "chr", "get std chr dev id fail: %d\n", ret);
        ret = -1;
        goto out;
    }
    
    snprintf(name, 16, "%s%d", CCCI_DEV_NAME, md_id);
    if (register_chrdev_region(MKDEV(major, minor), STD_CHR_DEV_NUM, name) != 0) {
        CCCI_MSG_INF(md_id, "chr", "regsiter CCCI_CHRDEV fail\n");
        ret = -1;
        goto out;
    }

    cdev_init(&ctlb->ccci_chrdev, &ccci_chrdev_fops);
    ctlb->ccci_chrdev.owner = THIS_MODULE;
    ret = cdev_add(&ctlb->ccci_chrdev, MKDEV(major,minor), STD_CHR_DEV_NUM);
    if (ret) {
        CCCI_MSG_INF(md_id, "chr", "cdev_add fail\n");
        goto out_err0;
    }
    
    sprintf(chrdev_wakelock_name[md_id], "ccci%d_chr", (md_id+1));
    wake_lock_init(&chrdev_wakelock[md_id], WAKE_LOCK_SUSPEND, chrdev_wakelock_name[md_id]);
    
    sprintf(chrdev_wakelock_mdlog_name[md_id], "ccci%d_chr_mdlog", (md_id+1));
    wake_lock_init(&chrdev_wakelock_mdlogger[md_id], WAKE_LOCK_SUSPEND, chrdev_wakelock_mdlog_name[md_id]);  
  
    spin_lock_init(&md_logger_lock);
  
    ctlb->major = major;
    ctlb->minor = minor;
    chr_ctlb[md_id] = ctlb;
    
    return ret;

out_err0:
    unregister_chrdev_region(MKDEV(major,minor), STD_CHR_DEV_NUM);
    
out:
    if (ctlb)
        kfree(ctlb);
    
    return ret;
}


void ccci_chrdev_exit(int md_id)
{
    if (chr_ctlb[md_id]) {
        unregister_chrdev_region(MKDEV(chr_ctlb[md_id]->major, chr_ctlb[md_id]->minor),CCCI_MAX_CH_NUM);
        cdev_del(&chr_ctlb[md_id]->ccci_chrdev);
        kfree(chr_ctlb[md_id]);
        chr_ctlb[md_id] = NULL;
    }
    wake_lock_destroy(&chrdev_wakelock[md_id]);
    wake_lock_destroy(&chrdev_wakelock_mdlogger[md_id]);
}


//=======================================================
// CCCI Virtual charactor device function
//=======================================================

static vir_ctl_block_t *vir_chr_ctlb[MAX_MD_NUM];



static void bind_system_msg_transfer(int md_id, ccci_vir_client_t *client)
{
    unsigned long    flags;
    vir_ctl_block_t    *ctlb = vir_chr_ctlb[md_id];
    spin_lock_irqsave(&ctlb->bind_lock, flags);
    ctlb->system_msg_client = client;
    spin_unlock_irqrestore(&ctlb->bind_lock, flags);
}


static void remove_system_msg_transfer(int md_id)
{
    unsigned long    flags;
    vir_ctl_block_t    *ctlb = vir_chr_ctlb[md_id];
    spin_lock_irqsave(&ctlb->bind_lock, flags);
    ctlb->system_msg_client = NULL;
    spin_unlock_irqrestore(&ctlb->bind_lock, flags);
}


void ccci_system_message(int md_id, unsigned int message, unsigned int resv)
{
    unsigned long        flags;
    vir_ctl_block_t        *ctlb = vir_chr_ctlb[md_id];
    ccci_msg_t            msg;
    struct kfifo        *sys_msg_fifo;
    ccci_vir_client_t    *client = ctlb->system_msg_client;

    msg.data0 = 0xFFFFFFFF;
    msg.data1 = message;
    msg.channel = CCCI_MONITOR_CH;
    msg.reserved = resv;

    spin_lock_irqsave(&ctlb->bind_lock, flags);
    if(client != NULL) {
        sys_msg_fifo = &client->private_fifo;
        if (kfifo_is_full(sys_msg_fifo)) {
            spin_unlock_irqrestore(&ctlb->bind_lock, flags);
            CCCI_MSG_INF(md_id, "chr", "send system msg fail: fifo full\n");
            return;
        } else {
            // Push data
            kfifo_in(&client->private_fifo, &msg, sizeof(ccci_msg_t));
            client->wakeup_waitq = 1;
            wake_up_interruptible(&client->wait_q);
        }
    } else {
        spin_unlock_irqrestore(&ctlb->bind_lock, flags);
        CCCI_MSG_INF(md_id, "chr", "send sys msg fail: no bind client\n");
        return;
    }
    spin_unlock_irqrestore(&ctlb->bind_lock, flags);
    
}


static void ccci_vir_client_init(ccci_vir_client_t *client, int idx, pid_t pid)
{
    WARN_ON(client==NULL);
    spin_lock_init(&client->lock);
    atomic_set(&client->user,1);
    client->pid=pid;
    client->index=idx;
    INIT_LIST_HEAD(&client->dev_list);
    init_waitqueue_head(&client->wait_q);
    client->fasync=NULL;
    client->wakeup_waitq=0;
}


static void release_vir_client(ccci_vir_client_t *client)
{
    vir_ctl_block_t *ctlb = (vir_ctl_block_t *)client->ctlb;
    WARN_ON(spin_is_locked(&client->lock)||list_empty(&client->dev_list));
    mutex_lock(&ctlb->chr_dev_mutex);

    if(client->index == 0) {
        remove_system_msg_transfer(ctlb->md_id);
    }

    list_del(&client->dev_list);
    if(client->fifo_ready) {
        kfifo_free(&client->private_fifo);
        client->fifo_ready = 0;
    }

    kfree(client);
    mutex_unlock(&ctlb->chr_dev_mutex);
}

static inline void ccci_put_vir_client(ccci_vir_client_t *client)
{
    WARN_ON(client == NULL);
    if (atomic_dec_and_test(&client->user))
        release_vir_client(client);
}


static ccci_vir_client_t *find_get_vir_client(int md_id, int idx, pid_t pid)
{
    ccci_vir_client_t    *client = NULL;
    vir_ctl_block_t        *ctlb = vir_chr_ctlb[md_id];

    mutex_lock(&ctlb->chr_dev_mutex);

    list_for_each_entry(client,&ctlb->chr_dev_list,dev_list)
    {
        if (client->index == idx) {
            atomic_inc(&client->user);
            break ;
        }
    }
    if (&client->dev_list == &ctlb->chr_dev_list) {
        CCCI_CHR_MSG(md_id, "Create a Vir Client %d\n", idx);
        client = kmalloc(sizeof(*client), GFP_KERNEL);
        if (client == NULL) {
            CCCI_MSG_INF(md_id, "chr", "kmalloc for create client fail \n");
            client = ERR_PTR(-ENOMEM);
            goto out;
        }
        memset(client, 0, sizeof(ccci_vir_client_t));

        if (idx == 0) {
            // Vir char 0(transfer msg between md_init and ccci driver) need fifo
            if (0!=kfifo_alloc(&client->private_fifo, sizeof(ccci_msg_t)*CCCI_VIR_CHR_KFIFO_SIZE, GFP_KERNEL)){
                CCCI_MSG_INF(md_id, "chr", "allocate kfifo fail for vir client0\n");
                client->fifo_ready = 0;
                kfree(client);
                client = NULL;
                goto out;
            } else {
                client->fifo_ready = 1;
            }
        }

        ccci_vir_client_init(client, idx, pid);
        client->md_id = md_id;
        client->ctlb = ctlb;
        list_add(&client->dev_list,&ctlb->chr_dev_list);
    }

out:
    mutex_unlock(&ctlb->chr_dev_mutex);
    return client;
}


static int ccci_vir_chr_open(struct inode *inode, struct file *file)
{
    int minor = iminor(inode);
    int major = imajor(inode);
    int index = -1, minor_start;
    int md_id;
    int ret = 0;
    ccci_vir_client_t *client = NULL;

    ret = get_md_id_by_dev_major(major);
    if(ret < 0) {
        CCCI_MSG("%s: get md id fail: %d\n", __FUNCTION__, ret);
        goto out;
    }
    
    md_id = ret;
    ret = get_dev_id_by_md_id(md_id, "vir chr", NULL, &minor_start);
    if(ret < 0) {
        CCCI_MSG_INF(md_id, "chr", "%s: get dev minor id fail: %d\n", __FUNCTION__, ret);
        goto out;
    }
    index = minor - minor_start;

    client = find_get_vir_client(md_id, index, current->pid);
    CCCI_CHR_MSG(md_id, "Vchar(ch%d) open by %s\n", index, current->comm);
    if (IS_ERR(client)) {
        CCCI_MSG_INF(md_id, "chr", "%s: find client fail \n", __FUNCTION__);
        ret = PTR_ERR(client);
        goto out;
    }

    if(atomic_read(&client->user)>1) {
        CCCI_MSG_INF(md_id, "chr", "%s: [Error]multi-open, not support it\n", __FUNCTION__);
        return -EPERM;
    }

    //CCCI_DBG_MSG(md_id, "chr", "idx:%d fifo%d\n", client->index, client->fifo_ready);

    file->private_data = client;
    nonseekable_open(inode, file);
out:
    if(index == 0) { // Always using vir char 0 as MD monitor port, and bind it to system message port
        bind_system_msg_transfer(md_id, client);
    }
    return ret;

}


static int ccci_vir_chr_release(struct inode *inode, struct file *file)
{
    ccci_vir_client_t *client = (ccci_vir_client_t *)file->private_data;
    ccci_put_vir_client(client);
    return 0;
}


static int ccci_vir_chr_fasync(int fd, struct file *file, int on)
{
    return -EACCES; // Dummy function, not support fasync for user space
}


static unsigned int ccci_vir_chr_poll(struct file *file,poll_table *wait)
{
    return -EACCES; // Dummy function, not support poll for user space
}


static ssize_t ccci_vir_chr_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    return -EACCES; // Dummy function, not support write for user space
}


static ssize_t ccci_vir_chr_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    ccci_vir_client_t *client = (ccci_vir_client_t *)file->private_data;
    ccci_msg_t buff,*u_buff = (ccci_msg_t *)buf;
    int ret = 0;
    int value;
    int md_id;

    WARN_ON(client == NULL);
    md_id = client->md_id;

retry:

    // Check fifo if has data
    if (kfifo_is_empty(&client->private_fifo)) {
        ret = 0;
    } else {
        // Pop data
        ret = kfifo_out(&client->private_fifo, &buff, sizeof(ccci_msg_t));
    }

    if (ret == sizeof(buff)) {
        if (copy_to_user(u_buff, &buff,sizeof(buff))) {
            CCCI_MSG_INF(md_id, "chr", "%s: copy_to_user fail: %08X, %08X, %02d, %08X\n", 
                    __FUNCTION__, buff.data0, buff.data1, buff.channel, buff.reserved);
            ret= -EFAULT;
        }
    } else {
        if (file->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            goto out;
        }

        value = wait_event_interruptible(client->wait_q, client->wakeup_waitq);
        if(value == -ERESTARTSYS) {
            ret = -EINTR;
            goto  out;
        } else if(value == 0) {
            client->wakeup_waitq = 0;
            goto retry;
        }
    }

out:
    return ret;
}

void ccci_md_logger_notify(void)
{
    unsigned long        flags;
    spin_lock_irqsave(&md_logger_lock, flags);
    if (md_logger_client) {
        wake_up_interruptible(&md_logger_client->wait_q);
        wake_lock_timeout(&chrdev_wakelock_mdlogger[md_logger_client->md_id], HZ); // MD logger using 1s wake lock
        catch_more = 1;
    }
    spin_unlock_irqrestore(&md_logger_lock, flags);
}
static long ccci_vir_chr_ioctl( struct file *file, unsigned int cmd, unsigned long arg)
{
    int                    addr, ret = 0;
    ccci_vir_client_t    *client=(ccci_vir_client_t *)file->private_data;
    int                 md_id = client->md_id;
    int                    idx = client->index;
    unsigned int        sim_mode;
    unsigned int            sim_switch_type;
    unsigned int         md_type;
    unsigned int        sim_type;
    unsigned int        enable_sim_type;
#ifdef CONFIG_MTK_ICUSB_SUPPORT
    unsigned int        sim_id;
#endif
    unsigned int        sim_slot_cfg[3];
    int                    ccci_cfg_setting[2];
    int                    setting_num;

    switch (cmd) 
    {
        case CCCI_IOC_GET_MD_PROTOCOL_TYPE:
            {
                char md_protol[] = "AP_TST";
                unsigned int data_size = sizeof(md_protol) / sizeof(char);

                CCCI_MSG_INF(md_id, "chr", "Call CCCI_IOC_GET_MD_PROTOCOL_TYPE!\n");

                if (copy_to_user((void __user *)arg, md_protol, data_size)) 
                {
                    CCCI_MSG_INF(md_id, "chr", "copy_to_user MD_PROTOCOL failed !!\n");
                    return -EFAULT;
                }

                break;
            }

        case CCCI_IOC_MD_RESET:
            CCCI_MSG_INF(md_id, "chr", "MD reset ioctl vir(%d) called by %s\n", idx, current->comm);
            ret = send_md_reset_notify(md_id);
            break;

        case CCCI_IOC_FORCE_MD_ASSERT:
            CCCI_MSG_INF(md_id, "chr", "Force MD assert ioctl(%d) called by %s\n", idx, current->comm);
            ret = ccci_trigger_md_assert(md_id);
            break;

        case CCCI_IOC_SEND_RUN_TIME_DATA:
            if(idx == 0) {
                ret = ccci_send_run_time_data(md_id);
            } else {
                CCCI_MSG_INF(md_id, "chr", "Set runtime by invalid user(%d) called by %s\n", idx, current->comm);
                ret = -1;
            }
            break;

        case CCCI_IOC_GET_MD_INFO:
            addr = is_modem_debug_ver(md_id);
            ret = put_user((unsigned int)addr, (unsigned int __user *)arg);
            break;

        case CCCI_IOC_GET_MD_EX_TYPE:
            ret = get_md_exception_type(md_id);
            CCCI_MSG_INF(md_id, "chr", "get modem exception type=%d\n", ret);
            break;

        case CCCI_IOC_SEND_STOP_MD_REQUEST:
            CCCI_MSG_INF(md_id, "chr", "stop MD request ioctl called by %s\n", current->comm);
            ret = send_md_stop_notify(md_id);
            break;

        case CCCI_IOC_SEND_START_MD_REQUEST:
            CCCI_MSG_INF(md_id, "chr", "start MD request ioctl called by %s\n", current->comm);
            ret = send_md_start_notify(md_id);
            break;

        case CCCI_IOC_DO_START_MD:
            CCCI_MSG_INF(md_id, "chr", "start MD ioctl called by %s\n", current->comm);
            ret = ccci_start_modem(md_id);
            break;

        case CCCI_IOC_DO_STOP_MD:
            CCCI_MSG_INF(md_id, "chr", "stop MD ioctl called by %s\n", current->comm);
            ret = ccci_stop_modem(md_id, 0);
            break;

        case CCCI_IOC_ENTER_DEEP_FLIGHT:
            CCCI_MSG_INF(md_id, "chr", "enter MD flight mode ioctl called by %s\n", current->comm);
            ret = send_enter_flight_mode_request(md_id);
            break;

        case CCCI_IOC_LEAVE_DEEP_FLIGHT:
            CCCI_MSG_INF(md_id, "chr", "leave MD flight mode ioctl called by %s\n", current->comm);
            ret = send_leave_flight_mode_request(md_id);
            break;

        case CCCI_IOC_POWER_ON_MD_REQUEST:
            CCCI_MSG_INF(md_id, "chr", "Power on MD request ioctl called by %s\n", current->comm);
            ret = send_power_on_md_request(md_id);
            break;

        case CCCI_IOC_POWER_OFF_MD_REQUEST:
            CCCI_MSG_INF(md_id, "chr", "Power off MD request ioctl called by %s\n", current->comm);
            ret = send_power_down_md_request(md_id);
            break;

        case CCCI_IOC_POWER_ON_MD:
            if(idx == 0) {
                ret = let_md_go(md_id);
            } else {
                CCCI_MSG_INF(md_id, "chr", "Power on MD by invalid user(%d) called by %s\n", idx, current->comm);
                ret = -1;
            }
            break;

        case CCCI_IOC_POWER_OFF_MD:
            if(idx == 0) {
                ret = let_md_stop(md_id, 1*1000);  // <<<< Fix this
            } else {
                CCCI_MSG_INF(md_id, "chr", "Power off MD by invalid user(%d) called by %s\n", idx, current->comm);
                ret = -1;
            }
            break;
        
        case CCCI_IOC_SIM_SWITCH:
            if(copy_from_user(&sim_mode, (void __user *)arg, sizeof(unsigned int))) {
                CCCI_MSG_INF(md_id, "chr", "IOC_SIM_SWITCH: copy_from_user fail!\n");
                ret = -EFAULT;
            } else {
                ret = exec_ccci_kern_func(ID_SSW_SWITCH_MODE, (char*)(&sim_mode), sizeof(unsigned int));//switch_sim_mode(sim_mode);
                CCCI_MSG_INF(md_id, "chr", "IOC_SIM_SWITCH(%x): %d\n", sim_mode, ret);     
            }
            break;

        case CCCI_IOC_UPDATE_SIM_SLOT_CFG:
            if(copy_from_user(&sim_slot_cfg, (void __user *)arg, sizeof(sim_slot_cfg))) {
                CCCI_MSG_INF(md_id, "chr", "CCCI_IOC_UPDATE_SIM_SLOT_CFG: copy_from_user fail!\n");
                ret = -EFAULT;
            } else {
                CCCI_MSG_INF(md_id, "chr", "CCCI_IOC_UPDATE_SIM_SLOT_CFG get s0:%d s1:%d en:%d\n", 
                                sim_slot_cfg[0], sim_slot_cfg[1], sim_slot_cfg[2]);
                sim_mode = (2<<16)|(sim_slot_cfg[0]&0x000000FF)|((sim_slot_cfg[1]<<8)&0x0000FF00);
                ret = exec_ccci_kern_func(ID_SSW_SWITCH_MODE, (char*)(&sim_mode), sizeof(unsigned int));
                if(ret == 0) {
                    curr_sim_mode[md_id] = sim_mode;
                    sim_mode = sim_mode&0x0000FFFF;
                    sim_mode |= (1<<24); // SIM slot save to ccci nvram idx 1
                    if ( sim_slot_cfg[2] ) { // Need save setting
                        sim_mode |= (1<<31); // Set save nvram flag
                    }
                    send_update_cfg_request(md_id, sim_mode);
                } else {
                    CCCI_MSG_INF(md_id, "chr", "CCCI_IOC_UPDATE_SIM_SLOT_CFG exec (%d)\n", ret);
                }
                ret = 0;
            }
            break;

        case CCCI_IOC_SIM_SWITCH_TYPE:
            sim_switch_type = get_sim_switch_type();
            ret = put_user(sim_switch_type, (unsigned int __user *)arg);
            break;
        
        case CCCI_IOC_SEND_BATTERY_INFO:
            send_battery_info(md_id);
            break;

#ifdef CONFIG_MTK_ICUSB_SUPPORT
        case CCCI_IOC_SEND_ICUSB_NOTIFY:
            if(copy_from_user(&sim_id, (void __user *)arg, sizeof(unsigned int))) {
                CCCI_MSG_INF(md_id, "chr", "CCCI_IOC_SEND_ICUSB_NOTIFY: copy_from_user fail!\n");
                ret = -EFAULT;
            } else {
                send_icusb_notify(md_id, sim_id);
            }
            break;
#endif            

        case CCCI_IOC_RELOAD_MD_TYPE:
            if(copy_from_user(&md_type, (void __user *)arg, sizeof(unsigned int))) {
                CCCI_MSG_INF(md_id, "chr", "IOC_RELOAD_MD_TYPE: copy_from_user fail!\n");
                ret = -EFAULT;
            } else {
                CCCI_MSG_INF(md_id, "chr", "IOC_RELOAD_MD_TYPE: storing md type(%d)!\n", md_type);
                set_modem_support(md_id, md_type);
                ccci_set_reload_modem(md_id);
            }
            break;

        case CCCI_IOC_GET_SIM_TYPE:            //for regional phone boot animation
            if (get_sim_type(md_id, &sim_type))
            {
                CCCI_MSG_INF(md_id, "chr", "sim type may not be correct\n");
            }
            ret = put_user((unsigned int)sim_type, (unsigned int __user *)arg);
            break;

        case CCCI_IOC_ENABLE_GET_SIM_TYPE:    //for regional phone boot animation
            if(copy_from_user(&enable_sim_type, (void __user *)arg, sizeof(unsigned int))) {
                CCCI_MSG_INF(md_id, "chr", "CCCI_IOC_ENABLE_GET_SIM_TYPE: copy_from_user fail!\n");
                ret = -EFAULT;
            } else {
                enable_get_sim_type(md_id, enable_sim_type);
            }
            break;

        case CCCI_IOC_SET_MD_IMG_EXIST:
            if(copy_from_user(&md_img_exist, (void __user *)arg, sizeof(md_img_exist))) {
                CCCI_MSG_INF(md_id, "chr", "CCCI_IOC_ENABLE_GET_SIM_TYPE: copy_from_user fail!\n");
                ret = -EFAULT;
            }
            break;

        case CCCI_IOC_GET_MD_IMG_EXIST:
            if (copy_to_user((void __user *)arg, &md_img_exist, sizeof(md_img_exist))) {
                CCCI_MSG_INF(md_id, "chr", "CCCI_IOC_GET_MD_IMG_EXIST: copy_to_user fail\n");
                ret= -EFAULT;
            }
            break;

        case CCCI_IOC_GET_MD_TYPE:
            md_type = get_modem_support(md_id);
            ret = put_user((unsigned int)md_type, (unsigned int __user *)arg);
            break;

        case CCCI_IOC_STORE_MD_TYPE:
            CCCI_DBG_MSG(md_id, "chr", "store md type ioctl called by %s!\n",  current->comm);
            if(copy_from_user(&md_type_saving, (void __user *)arg, sizeof(unsigned int))) {
                CCCI_MSG_INF(md_id, "chr", "store md type fail: copy_from_user fail!\n");
                ret = -EFAULT;
            } else {
                CCCI_MSG_INF(md_id, "chr", "storing md type(%d) in kernel space!\n", md_type_saving);
                if (0x1 <= md_type_saving && md_type_saving <= 0x4){
                    if (md_type_saving != get_modem_support(md_id))
                        CCCI_MSG_INF(md_id, "chr", "Maybe Wrong: md type storing not equal with current setting!(%d %d)\n", md_type, get_modem_support(md_id));
                    //Notify md_init daemon to store md type in nvram
                    ccci_system_message(md_id, CCCI_MD_MSG_STORE_NVRAM_MD_TYPE, 0);
                }
                else {
                    CCCI_MSG_INF(md_id, "chr", "store md type fail: invalid md type(0x%x)\n", md_type_saving);
                }
            }
            break;

        case CCCI_IOC_GET_MD_TYPE_SAVING:
            ret = put_user(md_type_saving, (unsigned int __user *)arg);
            break;

        case CCCI_IOC_GET_CFG_SETTING:
            setting_num = 2;
            ret = get_common_cfg_setting(md_id, ccci_cfg_setting, &setting_num);
            if (copy_to_user((void __user *)arg, ccci_cfg_setting, sizeof(ccci_cfg_setting))) {
                CCCI_MSG_INF(md_id, "chr", "CCCI_IOC_GET_CFG_SETTING: copy_to_user fail\n");
                ret= -EFAULT;
            }
            break;

#ifdef CONFIG_MTK_MD_SBP_CUSTOM_VALUE
        case CCCI_IOC_GET_MD_SBP_CFG:
            CCCI_MSG_INF(md_id, "chr", "SBP confg length:%d!\n", strlen(CONFIG_MTK_MD_SBP_CUSTOM_VALUE));
            if (strlen(CONFIG_MTK_MD_SBP_CUSTOM_VALUE) > 0) {
                if (!md_sbp_code_default) {
                    int tmpret = kstrtouint(CONFIG_MTK_MD_SBP_CUSTOM_VALUE, 0, &md_sbp_code_default);
                    if (!tmpret){
                        CCCI_MSG_INF(md_id, "chr", "GET_MD_SBP_CFG: get config sbp code:%d!\n", md_sbp_code_default);
                    } else {
                        CCCI_MSG_INF(md_id, "chr", "GET_MD_SBP_CFG: get config sbp code fail! ret:%d, Config val:%s\n"
                            , tmpret, CONFIG_MTK_MD_SBP_CUSTOM_VALUE);
                    }
                } else {
                    CCCI_MSG_INF(md_id, "chr", "GET_MD_SBP_CFG: config sbp code:%d!\n", md_sbp_code_default);
                }

                ret = put_user(md_sbp_code_default, (unsigned int __user *)arg);

            } else {
                ret = -ENOTTY;
            }
            break;

        case CCCI_IOC_SET_MD_SBP_CFG:
            if(copy_from_user(&md_sbp_code, (void __user *)arg, sizeof(unsigned int))) {
                CCCI_MSG_INF(md_id, "chr", "SET_MD_SBP_CFG: copy_from_user fail!\n");
                ret = -EFAULT;
            } else {
                CCCI_MSG_INF(md_id, "chr", "SET_MD_SBP_CFG: set md sbp code:0x%x!\n", md_sbp_code);
                ccci_set_md_sbp(md_id, md_sbp_code);
            }
            break;
#endif // CONFIG_MTK_MD_SBP_CUSTOM_VALUE

        default:
            CCCI_MSG_INF(md_id, "chr", "illegal IOCTL %X called by %s\n", cmd, current->comm);
            ret = -ENOTTY;
            break;
    }

    return ret;
}


static int ccci_vir_chr_mmap(struct file *file, struct vm_area_struct *vma)
{
    return -1; // Dummy function, not support mmap, // mmap return -1 when fail
}


static struct file_operations ccci_vir_chrdev_fops=
{
    .owner = THIS_MODULE,
    .open=ccci_vir_chr_open,
    .read=ccci_vir_chr_read,
    .write=ccci_vir_chr_write,
    .release=ccci_vir_chr_release,
    .unlocked_ioctl=ccci_vir_chr_ioctl,
    .fasync=ccci_vir_chr_fasync,
    .poll=ccci_vir_chr_poll,
    .mmap=ccci_vir_chr_mmap,

};


int ccci_vir_chrdev_init(int md_id)
{
    int  ret = 0;
    int  major, minor;
    char name[16];
    vir_ctl_block_t *ctlb;

    curr_sim_mode[md_id] = -1;

    ctlb = (vir_ctl_block_t *)kmalloc(sizeof(vir_ctl_block_t), GFP_KERNEL);
    if(ctlb == NULL) {
        ret = -CCCI_ERR_GET_MEM_FAIL;
        goto out;
    }

    // Init control struct
    memset(ctlb, 0, sizeof(vir_ctl_block_t));
    mutex_init(&ctlb->chr_dev_mutex);
    ctlb->chr_dev_list.next = &ctlb->chr_dev_list;
    ctlb->chr_dev_list.prev = &ctlb->chr_dev_list;
    ctlb->md_id = md_id;

    ret = get_dev_id_by_md_id(md_id, "vir chr", &major, &minor);
    if (ret < 0) {
        CCCI_MSG_INF(md_id, "chr", "Get ccci vir dev id fail(%d)!\n", ret);
        ret=-1;
        goto out;
    }
    
    snprintf(name, 16, "vir_chr%d", md_id);
    if (register_chrdev_region(MKDEV(major,minor),CCCI_MAX_VCHR_NUM, name) != 0) {
        CCCI_MSG_INF(md_id, "chr", "Regsiter CCCI_VCHRDEV failed!\n");
        ret=-1;
        goto out;
    }

    cdev_init(&ctlb->ccci_chrdev,&ccci_vir_chrdev_fops);
    ctlb->ccci_chrdev.owner = THIS_MODULE;
    ret = cdev_add(&ctlb->ccci_chrdev,MKDEV(major,minor),CCCI_MAX_VCHR_NUM);
    if (ret) {
        CCCI_MSG_INF(md_id, "chr", "cdev_add failed\n");
        goto out_err0;
    }

    ctlb->major = major;
    ctlb->minor = minor;
    spin_lock_init(&ctlb->bind_lock);
    vir_chr_ctlb[md_id] = ctlb;
    return ret;

out_err0:
    unregister_chrdev_region(MKDEV(major,minor),CCCI_MAX_VCHR_NUM);
    
out:
    if (ctlb)
        kfree(ctlb);
    
    return ret;
}


void ccci_vir_chrdev_exit(int md_id)
{
    if (vir_chr_ctlb[md_id]) {
        unregister_chrdev_region(MKDEV(vir_chr_ctlb[md_id]->major, vir_chr_ctlb[md_id]->minor),CCCI_MAX_CH_NUM);
        cdev_del(&vir_chr_ctlb[md_id]->ccci_chrdev);
        kfree(vir_chr_ctlb[md_id]);
        vir_chr_ctlb[md_id] = NULL;
    }
}


#ifdef CONFIG_MTK_MD_SBP_CUSTOM_VALUE

int __weak ccci_set_md_sbp(int md_id, unsigned int md_sbp)
{
    CCCI_ERR("calling weak function %s(%d,%d)\n", __FUNCTION__, md_id, md_sbp);
    return -1;
}

#endif // CONFIG_MTK_MD_SBP_CUSTOM_VALUE
