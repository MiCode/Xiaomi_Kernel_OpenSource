/*****************************************************************************
 *
 * Filename:
 * ---------
 *   emd_cfifo.c
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   MT65XX emd Virtual cfifo Driver
 *
 ****************************************************************************/

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/dma-mapping.h>
#include <asm/dma-mapping.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <emd_ctl.h>

#define EMD_CFIFO_NAME    "emd_cfifo_drv"
#define EMD_CFIFO_DEV_NUM    (2)
#define EMD_CFIFO_MODEM    (0)
#define EMD_CFIFO_MUXD     (1)

#define EMD_SHARE_MEM_SIZE (8192)
typedef struct
{
    struct
    {
        unsigned int read;
        unsigned int write;
        unsigned int length;
    }    rx_control, tx_control;
    unsigned char    buffer[0]; 
} shared_mem_t;

typedef struct _cfifo_instance_t
{
    int            m_md_id;
    int            count;
    int            ready;
    int            need_reset;
    int            idx;
    struct wake_lock    wake_lock;
    char                wakelock_name[16];
    wait_queue_head_t   write_waitq;
    wait_queue_head_t   read_waitq;
    wait_queue_head_t   poll_waitq_r;
    wait_queue_head_t   poll_waitq_w;
    spinlock_t          poll_lock;
    struct 
    {
        struct
        {
            unsigned int  *read;
            unsigned int  *write;
            unsigned int  *length;
            unsigned char *buffer;
        } rx_control, tx_control;
    }  shared_mem;
    struct mutex            emd_cfifo_mutex;
    rwlock_t                emd_cfifo_rwlock; 
    struct _cfifo_instance_t*       other_side;
    struct timer_list       timer;
    int                 reset_handle;
} cfifo_instance_t;

typedef struct _cfifo_ctl_block{
    int                 m_md_id;
    cfifo_instance_t    emd_cfifo_modem; //map rx_control
    cfifo_instance_t    emd_cfifo_muxd;  //map tx_control
    shared_mem_t        *uart1_shared_mem;
    int                 cfifo_buf_size;
    char                drv_name[32];
    char                node_name[16];
    struct cdev         emd_cfifo_dev;
    int                 major;
    int                 minor;
}cfifo_ctl_block_t;

static cfifo_ctl_block_t    *emd_cfifo_ctlb[EMD_MAX_NUM];

static int emd_cfifo_readable(cfifo_instance_t    *cfifo_instance)
{
    int     read, write, size;
    read  = (int)(*(cfifo_instance->shared_mem.rx_control.read));
    write = (int)(*(cfifo_instance->shared_mem.rx_control.write)); 
    size  = write - read;

    if(size < 0)
        size += (int)(*(cfifo_instance->shared_mem.rx_control.length));

    return size;
}

static int emd_cfifo_writeable(cfifo_instance_t    *cfifo_instance)
{
    int     read, write, size,length;
    read   = (int)(*(cfifo_instance->shared_mem.tx_control.read));
    write  = (int)(*(cfifo_instance->shared_mem.tx_control.write));
    length = (int)(*(cfifo_instance->shared_mem.tx_control.length));

    if (read == write)
    {
        size = length - 1;
    }
    else if (read < write)
    {
        size =  length - write;
        size += read;
    }
    else
    {
        size = read - write - 1;
    }

    return size;
}

static int emd_cfifo_reset(cfifo_instance_t *cfifo_instance)
{
	int ret = 0;
	if (cfifo_instance)
	{
    	*(cfifo_instance->shared_mem.rx_control.read) = 0;
    	*(cfifo_instance->shared_mem.rx_control.write) = 0;
		memset(cfifo_instance->shared_mem.rx_control.buffer, 0, *(cfifo_instance->shared_mem.rx_control.length));
	}
	else
	{
		EMD_MSG_INF("cfifo","cfifo null when try to reset\n");
		ret = -EFAULT;
	}
	return ret;
}


static ssize_t emd_cfifo_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    cfifo_instance_t *cfifo_instance=(cfifo_instance_t *)file->private_data;

    int                part, size, ret = 0;
    int                value;
    unsigned        read, write, length;
    char            *rx_buffer;
    int                data_be_read;
    char            *u_buf = buf;
    read  = (int)(*(cfifo_instance->shared_mem.rx_control.read));
    write = (int)(*(cfifo_instance->shared_mem.rx_control.write)); 
    length = (int)(*(cfifo_instance->shared_mem.rx_control.length));
    rx_buffer = cfifo_instance->shared_mem.rx_control.buffer;
    // EMD_MSG_INF("cfifo","emd_cfifo%d_read: read=%d,wirte=%d,length=%d,count=%d\n",cfifo_instance->idx,read,write,length,count);

    do {
        size = emd_cfifo_readable(cfifo_instance);

        if (size == 0) {
            if (file->f_flags & O_NONBLOCK) {    
                ret=-EAGAIN;
                goto out;
            }
            else {
                value = wait_event_interruptible(cfifo_instance->read_waitq, emd_cfifo_readable(cfifo_instance));
                if(value == -ERESTARTSYS) {
                    EMD_MSG_INF("cfifo","Interrupted syscall.signal_pend=0x%llx\n",
                        *(long long *)current->pending.signal.sig);
                    ret = -EINTR;
                    goto  out;
                }
            }
        }
        else
        {
            break;
        }

    }while(size==0);

    data_be_read = (int)count;
    if(data_be_read > size)
        data_be_read = size;
    //copy_to_user may be scheduled, 
    //So add 1s wake lock to make sure emd user can be running.
    wake_lock_timeout(&cfifo_instance->wake_lock, HZ);
    if( (read + data_be_read) >= length ) {
        // Need read twice

        // Copy first part
        part = length - read;
        if (copy_to_user(u_buf,&rx_buffer[read],part)) {
          EMD_MSG_INF("cfifo","read: copy_to_user fail:u_buf=%08x,rx_buffer=0x%08x,read=%d,size=%d ret=%d line=%d\n",\
                  (unsigned int)u_buf,(unsigned int)rx_buffer,read,part,ret,__LINE__);

            ret= -EFAULT;
            goto out;
        }
        // Copy second part
        if (copy_to_user(&u_buf[part],rx_buffer,data_be_read - part)) {
          EMD_MSG_INF("cfifo","read: copy_to_user fail:u_buf=%08x,rx_buffer=0x%08x,read=%d,size=%d ret=%d line=%d\n",\
                  (unsigned int)u_buf,(unsigned int)rx_buffer,read,data_be_read - part,ret,__LINE__);

            ret= -EFAULT;
            goto out;
        }
    }
    else {
        // Just need read once
        if (copy_to_user(u_buf,&rx_buffer[read],data_be_read)) {
          EMD_MSG_INF("cfifo","read: copy_to_user fail:u_buf=%08x,rx_buffer=0x%08x,read=%d,size=%d ret=%d line=%d\n",\
                  (unsigned int)u_buf,(unsigned int)rx_buffer,read,data_be_read,ret,__LINE__);
            ret= -EFAULT;
            goto out;
        }
    }

    // Update read pointer
    read += data_be_read;
    if(read >= length)
        read -= length;
    *(cfifo_instance->shared_mem.rx_control.read) = read;

    ret = data_be_read;
    if(emd_cfifo_writeable(cfifo_instance->other_side)) {
        wake_up_interruptible(&cfifo_instance->other_side->write_waitq); 
        wake_up_interruptible_poll(&cfifo_instance->other_side->poll_waitq_w,POLLOUT);
    }
    
    EMD_MSG_INF("cfifo","emd_cfifo%d_read: ret=%d\n",cfifo_instance->idx,ret);
out:

    return ret;
}


static ssize_t emd_cfifo_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    cfifo_instance_t    *cfifo_instance=(cfifo_instance_t *)file->private_data;
    int                ret = 0, part;
    int                data_be_write, size, value;
    unsigned        read, write, length;
    char            *tx_buffer;

    if(count == 0)
    {   
        EMD_MSG_INF("cfifo","emd_cfifo_write: count=0\n");   
        return 0;
    }
    if(!is_traffic_on(0))
    {
    	return -ENODEV;
    }
    mutex_lock(&cfifo_instance->emd_cfifo_mutex);

    data_be_write = (int)count;
#if 0
    EMD_MSG_INF("cfifo","emd_cfifo%d_write: write_request=%d write=%d read=%d\n",
                cfifo_instance->idx, data_be_write, *(cfifo_instance->shared_mem.tx_control.write), 
                *(cfifo_instance->shared_mem.tx_control.read));
#endif

    size = 0;

    /* Check free space */
    read   = (int)(*(cfifo_instance->shared_mem.tx_control.read));
    write  = (int)(*(cfifo_instance->shared_mem.tx_control.write));
    length = (int)(*(cfifo_instance->shared_mem.tx_control.length));
    tx_buffer = cfifo_instance->shared_mem.tx_control.buffer;

    do {
        size = emd_cfifo_writeable(cfifo_instance);

        if (size == 0) {
            if (file->f_flags & O_NONBLOCK) {    
                ret=-EAGAIN;
                goto out;
            }
            else {
                // Block write
                value = wait_event_interruptible(cfifo_instance->write_waitq, emd_cfifo_writeable(cfifo_instance));
                if(value == -ERESTARTSYS) {
                    EMD_MSG_INF("cfifo","(W)Interrupted syscall.signal_pend=0x%llx\n",
                        *(long long *)current->pending.signal.sig);
                    ret = -EINTR;
                    goto  out;
                }
            }
        }
        else
            break;

    }while(size==0);

    // Calculate write size
    if(data_be_write >= size)
        data_be_write = size;

    if( (write+data_be_write) >= length) {
        // Write twice
        // 1st write
        part = length - write;
        ret = copy_from_user(&tx_buffer[write],buf,part);
        if (ret)
        {
            EMD_MSG_INF("cfifo","write: copy from user fail:tx_buffer=0x%08x,write=%d, buf=%08x, part=%d ret=%d line=%d\n",\
                    (unsigned int)tx_buffer,write,(unsigned int)buf,part,ret,__LINE__);
            ret=-EFAULT;
            goto out;
        }
        // 2nd write
        ret = copy_from_user(tx_buffer,&buf[part],data_be_write - part);
        if (ret)
        {
            EMD_MSG_INF("cfifo","write: copy from user fail:tx_buffer=0x%08x,buf=%08x,part=%d,data_be_write-part=%d ret=%d line=%d\n",\
                    (unsigned int)tx_buffer,(unsigned int)buf,part,data_be_write - part,ret,__LINE__);
            ret=-EFAULT;
            goto out;
        }
    }
    else {
        // Write once is OK
        ret = copy_from_user(&tx_buffer[write],buf,data_be_write);
        if (ret)
        {
            EMD_MSG_INF("cfifo","write: copy from user fail:tx_buffer=0x%08x,write=%d,buf=%08x,data_be_write=%d ret=%d line=%d\n",\
                    (unsigned int)tx_buffer,write,(unsigned int)buf,data_be_write,ret,__LINE__);
            ret=-EFAULT;
            goto out;
        }
    }

    // Update read pointer
    write += data_be_write;
    if(write >= length)
        write -= length;
    *(cfifo_instance->shared_mem.tx_control.write) = write;

    ret = data_be_write;

out:
    mutex_unlock(&cfifo_instance->emd_cfifo_mutex);
    if(emd_cfifo_readable(cfifo_instance->other_side)) {
        wake_up_interruptible(&cfifo_instance->other_side->read_waitq);
        wake_up_interruptible_poll(&cfifo_instance->other_side->poll_waitq_r,POLLIN);
        wake_lock_timeout(&cfifo_instance->other_side->wake_lock, HZ / 2);
        if(cfifo_instance->idx == 1)
        {
            //muxd write, so wake up md to read
            request_wakeup_md_timeout(1, 1);
        }
    }
    EMD_MSG_INF("cfifo","emd_cfifo%d_write: ret=%d\n",cfifo_instance->idx,ret);
    return ret;
}

#define CCCI_IOC_MAGIC 'C'
#define CCCI_IOC_RESET_CFIFO         _IO(CCCI_IOC_MAGIC, 203)	/*Reset cfifo, used by factory to avoid md boot trace impact*/

static long emd_cfifo_ioctl( struct file *file, unsigned int cmd, unsigned long arg)
{
	cfifo_instance_t *cfifo_instance=(cfifo_instance_t *)file->private_data;
    int ret = 0;

	switch (cmd) {
	case CCCI_IOC_RESET_CFIFO:
        EMD_MSG_INF("chr", "reset buffer ioctl called by %s\n", current->comm);
        emd_cfifo_reset(cfifo_instance);
        break;
		
	default:
		EMD_MSG_INF("chr", "unsupported ioctl called by %s\n", current->comm);
		break;
	}

    return ret;
}


static unsigned int emd_cfifo_poll(struct file *file,poll_table *wait)
{
    cfifo_instance_t    *cfifo_instance=(cfifo_instance_t *)file->private_data;
    int                ret=0;
    unsigned long    flags;

    poll_wait(file, &cfifo_instance->poll_waitq_r, wait);
    poll_wait(file, &cfifo_instance->poll_waitq_w, wait);
    spin_lock_irqsave(&cfifo_instance->poll_lock, flags);
    if ( emd_cfifo_readable(cfifo_instance))
    {
         ret |= POLLIN|POLLRDNORM;
    }
    if ( emd_cfifo_writeable(cfifo_instance))
    {
         ret |= POLLOUT|POLLWRNORM;
    }
    spin_unlock_irqrestore(&cfifo_instance->poll_lock, flags);
    return ret;
}


static int emd_cfifo_open(struct inode *inode, struct file *file)
{
    int        minor = iminor(inode);
    int        major = imajor(inode);
    int        index, minor_start;
    int        md_id;
    int        ret = 0;
    char    *name;

    cfifo_ctl_block_t    *ctlb;
    cfifo_instance_t    *cfifo_instance = NULL;

    md_id = emd_get_md_id_by_dev_major(major);
    if(md_id<0)
    {
        EMD_MSG_INF("cfifo","get_md_id_by_dev_major(%d)=%d\n", major,md_id);
        return -ENODEV;
    }
    ret = emd_get_dev_id_by_md_id(md_id, "cfifo", NULL, &minor_start);
    if(ret < 0){
        EMD_MSG_INF("cfifo","get minor start fail(%d)\n", ret);
        return -ENODEV;
    }
    index = minor - minor_start;

    ctlb = emd_cfifo_ctlb[md_id];
    switch(index)
    {
        case EMD_CFIFO_MODEM:
            name = "emd_modem";
            EMD_MSG_INF("cfifo","emd_cfifo_open: emd_modem\n");
            cfifo_instance = &ctlb->emd_cfifo_modem;
            break;
        case EMD_CFIFO_MUXD:
            name = "emd_muxd";
            EMD_MSG_INF("cfifo","emd_cfifo_open: emd_muxd\n");
            cfifo_instance = &ctlb->emd_cfifo_muxd;
            cfifo_instance->reset_handle = emd_reset_register(name);
            if(cfifo_instance->reset_handle<0)
            {
                EMD_MSG_INF("cfifo","open emd_reset_register: failed\n");
            }
            break;

    default:
            EMD_MSG_INF("cfifo","error index(%d)=minor(%d)-minor_start(%d)\n", index,minor,minor_start);
        return -ENODEV;
    }

    mutex_lock(&cfifo_instance->emd_cfifo_mutex);

    cfifo_instance->count++;
    cfifo_instance->m_md_id = md_id;
    if(cfifo_instance->count > 1){
        EMD_MSG_INF("cfifo","[cfifo_open]Multi-Open! %s open %s%d, %s, count:%d\n", 
            current->comm, ctlb->node_name, index, name, cfifo_instance->count);
   
        mutex_unlock(&cfifo_instance->emd_cfifo_mutex);
        return -EMFILE;
    }
    else {
        EMD_MSG_INF("cfifo","[cfifo_open]%s open %s%d, %s, nb_flag:%x\n", current->comm, 
            ctlb->node_name, index, name, file->f_flags & O_NONBLOCK);
        write_lock_bh(&cfifo_instance->emd_cfifo_rwlock);

        cfifo_instance->ready = 1;
        
        write_unlock_bh(&cfifo_instance->emd_cfifo_rwlock);

        file->private_data=cfifo_instance;
        nonseekable_open(inode,file);
        mutex_unlock(&cfifo_instance->emd_cfifo_mutex);

    }
    
    return ret;
}

void emd_reset_buffers(shared_mem_t *shared_mem, int size)
{
    shared_mem->tx_control.length = size;
    shared_mem->tx_control.read   = 0;
    shared_mem->tx_control.write  = 0;
    shared_mem->rx_control.length = size;
    shared_mem->rx_control.read   = 0;
    shared_mem->rx_control.write  = 0;
    memset(shared_mem->buffer, 0, size*2);
    return;
}

static int emd_cfifo_release(struct inode *inode, struct file *file)
{
    cfifo_instance_t    *cfifo_instance = (cfifo_instance_t *)file->private_data;
    int                md_id;
    cfifo_ctl_block_t    *ctlb = NULL;

    md_id = cfifo_instance->m_md_id;
    ctlb = emd_cfifo_ctlb[md_id];

    mutex_lock(&cfifo_instance->emd_cfifo_mutex);
    
    cfifo_instance->count--;

    EMD_MSG_INF("cfifo","[cfifo_close]Port%d count %d \n", cfifo_instance->idx, cfifo_instance->count);

    if (cfifo_instance->count == 0) {
        write_lock_bh(&cfifo_instance->emd_cfifo_rwlock);        
        cfifo_instance->ready = 0;
        write_unlock_bh(&cfifo_instance->emd_cfifo_rwlock);
        if(cfifo_instance->reset_handle!=-1)
        {
            emd_user_ready_to_reset(cfifo_instance->reset_handle);
        }
    }

    mutex_unlock(&cfifo_instance->emd_cfifo_mutex);
    
    return 0;
}



int emd_uart_ipo_h_restore(int md_id)
{
    cfifo_ctl_block_t    *ctlb = NULL;

    ctlb = emd_cfifo_ctlb[md_id];
    emd_reset_buffers(ctlb->uart1_shared_mem, ctlb->cfifo_buf_size);
    return 0;
}


static struct file_operations emd_cfifo_ops=
{
    .owner = THIS_MODULE,
    .open = emd_cfifo_open,
    .read = emd_cfifo_read,
    .write = emd_cfifo_write,
    .release = emd_cfifo_release,
    .unlocked_ioctl = emd_cfifo_ioctl,
    .poll = emd_cfifo_poll,

};

void emd_cfifo_instance_init(cfifo_instance_t *instance)
{
    rwlock_init(&instance->emd_cfifo_rwlock);
    spin_lock_init(&instance->poll_lock);
    wake_lock_init(&instance->wake_lock, WAKE_LOCK_SUSPEND, instance->wakelock_name);
    mutex_init(&instance->emd_cfifo_mutex);
    init_waitqueue_head(&instance->read_waitq);
    init_waitqueue_head(&instance->write_waitq);
    init_waitqueue_head(&instance->poll_waitq_r);
    init_waitqueue_head(&instance->poll_waitq_w);
}

int emd_cfifo_init(int md_id)
{
    int                    ret = 0;
    int                    cfifo_buf_len;
    int                    major,minor;
    cfifo_ctl_block_t        *ctlb;
    char                 name[16];

    // Create control block structure
    ctlb = (cfifo_ctl_block_t *)kmalloc(sizeof(cfifo_ctl_block_t), GFP_KERNEL);
    if(ctlb == NULL)
    {           
        EMD_MSG_INF("cfifo","emd_cfifo_init kmalloc(%d) fail\n",sizeof(cfifo_ctl_block_t));
        return -1;
    }
    memset(ctlb, 0, sizeof(cfifo_ctl_block_t));
    emd_cfifo_ctlb[md_id] = ctlb;

    // Init ctlb
    ctlb->m_md_id = md_id;

    ret = emd_get_dev_id_by_md_id(md_id, "cfifo", &major, &minor);
    if(ret < 0)
    {
        EMD_MSG_INF("cfifo","emd_get_dev_id_by_md_id(cfifo) ret=%d fail\n",ret);
        goto _RELEASE_CTL_MEMORY;
    }
    snprintf(name, 16, "%s", EMD_CFIFO_NAME);
    if (register_chrdev_region(MKDEV(major, minor), EMD_CFIFO_NUM*2, name) != 0) {
        EMD_MSG_INF("cfifo","regsiter emd_cfifo fail\n");
        ret = -1;
        goto _RELEASE_CTL_MEMORY;
    }

    cdev_init(&ctlb->emd_cfifo_dev, &emd_cfifo_ops);
    ctlb->emd_cfifo_dev.owner = THIS_MODULE;
    ret = cdev_add(&ctlb->emd_cfifo_dev, MKDEV(major,minor), EMD_CFIFO_NUM*2);
    if (ret) {
        EMD_MSG_INF("cfifo","cdev_add fail\n");
        goto _DEL_cfifo_DRV;
    }

    snprintf(ctlb->drv_name, 32, EMD_CFIFO_NAME);
    snprintf(ctlb->node_name, 16, "emd_cfifo");
    ctlb->uart1_shared_mem = (shared_mem_t*)kmalloc(EMD_SHARE_MEM_SIZE, GFP_KERNEL);
    if(ctlb->uart1_shared_mem ==NULL)
    {
        EMD_MSG_INF("cfifo","kmalloc share memory fail\n");
        goto _DEL_cfifo_DRV;
    }
    cfifo_buf_len = (EMD_SHARE_MEM_SIZE - sizeof(shared_mem_t))/2;
    ctlb->cfifo_buf_size = cfifo_buf_len;
    ctlb->uart1_shared_mem->tx_control.length = cfifo_buf_len;
    ctlb->uart1_shared_mem->tx_control.read   = 0;
    ctlb->uart1_shared_mem->tx_control.write  = 0;
    ctlb->uart1_shared_mem->rx_control.length = cfifo_buf_len;
    ctlb->uart1_shared_mem->rx_control.read   = 0;
    ctlb->uart1_shared_mem->rx_control.write  = 0;

    ctlb->emd_cfifo_modem.count        = 0;
    ctlb->emd_cfifo_modem.shared_mem.rx_control.length  = &(ctlb->uart1_shared_mem->rx_control.length);
    ctlb->emd_cfifo_modem.shared_mem.rx_control.read    = &(ctlb->uart1_shared_mem->rx_control.read);
    ctlb->emd_cfifo_modem.shared_mem.rx_control.write   = &(ctlb->uart1_shared_mem->rx_control.write);
    ctlb->emd_cfifo_modem.shared_mem.rx_control.buffer  = ctlb->uart1_shared_mem->buffer;
    ctlb->emd_cfifo_modem.shared_mem.tx_control.length  = &(ctlb->uart1_shared_mem->tx_control.length);
    ctlb->emd_cfifo_modem.shared_mem.tx_control.read    = &(ctlb->uart1_shared_mem->tx_control.read);
    ctlb->emd_cfifo_modem.shared_mem.tx_control.write   = &(ctlb->uart1_shared_mem->tx_control.write);
    ctlb->emd_cfifo_modem.shared_mem.tx_control.buffer  = ctlb->uart1_shared_mem->buffer+cfifo_buf_len;
    ctlb->emd_cfifo_modem.ready        = 1;
    ctlb->emd_cfifo_modem.idx          = 0;
    ctlb->emd_cfifo_modem.reset_handle =-1;
    ctlb->emd_cfifo_modem.other_side = &ctlb->emd_cfifo_muxd;
    snprintf(ctlb->emd_cfifo_modem.wakelock_name, sizeof(ctlb->emd_cfifo_muxd.wakelock_name), "%s", "emd_modem");
    emd_cfifo_instance_init(&ctlb->emd_cfifo_modem);

    ctlb->emd_cfifo_muxd.count        = 0;
    ctlb->emd_cfifo_muxd.shared_mem.rx_control.length = &(ctlb->uart1_shared_mem->tx_control.length);
    ctlb->emd_cfifo_muxd.shared_mem.rx_control.read   = &(ctlb->uart1_shared_mem->tx_control.read);
    ctlb->emd_cfifo_muxd.shared_mem.rx_control.write  = &(ctlb->uart1_shared_mem->tx_control.write);
    ctlb->emd_cfifo_muxd.shared_mem.rx_control.buffer = ctlb->uart1_shared_mem->buffer+cfifo_buf_len;
    ctlb->emd_cfifo_muxd.shared_mem.tx_control.length = &(ctlb->uart1_shared_mem->rx_control.length);
    ctlb->emd_cfifo_muxd.shared_mem.tx_control.read   = &(ctlb->uart1_shared_mem->rx_control.read);
    ctlb->emd_cfifo_muxd.shared_mem.tx_control.write  = &(ctlb->uart1_shared_mem->rx_control.write);
    ctlb->emd_cfifo_muxd.shared_mem.tx_control.buffer = ctlb->uart1_shared_mem->buffer;
    ctlb->emd_cfifo_muxd.ready        = 1;
    ctlb->emd_cfifo_muxd.idx          = 1;
    ctlb->emd_cfifo_muxd.reset_handle = -1;
    ctlb->emd_cfifo_muxd.other_side = &ctlb->emd_cfifo_modem;
    snprintf(ctlb->emd_cfifo_muxd.wakelock_name, sizeof(ctlb->emd_cfifo_muxd.wakelock_name), "%s", "emd_muxd");
    emd_cfifo_instance_init(&ctlb->emd_cfifo_muxd);
    EMD_MSG_INF("cfifo","emd_cfifo_init success\n");

    return 0;

_DEL_cfifo_DRV:
    unregister_chrdev_region(MKDEV(major,minor), EMD_CFIFO_NUM*2);

_RELEASE_CTL_MEMORY:
    if(ctlb->uart1_shared_mem)
    {
        kfree(ctlb->uart1_shared_mem);
    }
    if(ctlb)
    {
        kfree(ctlb);
    }
    emd_cfifo_ctlb[md_id] = NULL;

    return ret;
}


void emd_cfifo_exit(int md_id)
{
    cfifo_ctl_block_t *ctlb = emd_cfifo_ctlb[md_id];
    if(ctlb != NULL) {
        unregister_chrdev_region(MKDEV(ctlb->major, ctlb->minor),EMD_CFIFO_NUM*2);
        cdev_del(&ctlb->emd_cfifo_dev);
        if(ctlb->uart1_shared_mem)
        {
            kfree(ctlb->uart1_shared_mem);
            ctlb->uart1_shared_mem=NULL;
        }

        wake_lock_destroy(&ctlb->emd_cfifo_modem.wake_lock);
        kfree(ctlb);
        emd_cfifo_ctlb[md_id] = NULL;
    }
    return;
}
