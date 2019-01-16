
#include <linux/kernel.h> 
#include <linux/module.h> 
#include <linux/init.h> 
#include <linux/moduleparam.h>
#include <linux/slab.h> 
#include <linux/unistd.h> 
#include <linux/sched.h> 
#include <linux/fs.h> 
#include <asm/uaccess.h> 
#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <mach/memory.h>
#include <asm/io.h>
#include <linux/proc_fs.h>

#include "secmem.h"

/* only available for trustonic */
#include "mobicore_driver_api.h"
#include "tlsecmem_api.h"

#define DEFAULT_HANDLES_NUM (64)
#define MAX_OPEN_SESSIONS   (0xffffffff - 1)

/* Debug message event */
#define DBG_EVT_NONE        (0)       /* No event */
#define DBG_EVT_CMD         (1 << 0)  /* SEC CMD related event */
#define DBG_EVT_FUNC        (1 << 1)  /* SEC function event */
#define DBG_EVT_INFO        (1 << 2)  /* SEC information event */
#define DBG_EVT_WRN         (1 << 30) /* Warning event */
#define DBG_EVT_ERR         (1 << 31) /* Error event */
#define DBG_EVT_ALL         (0xffffffff)

#define DBG_EVT_MASK        (DBG_EVT_ALL)

#define MSG(evt, fmt, args...) \
do {    \
    if ((DBG_EVT_##evt) & DBG_EVT_MASK) { \
        printk("[%s] "fmt, SECMEM_NAME, ##args); \
    }   \
} while(0)

#define MSG_FUNC() MSG(FUNC, "%s\n", __FUNCTION__)

typedef struct {
    u32 id;
    u32 type;
} secmem_handle_t;

struct secmem_context {
    spinlock_t lock;
    secmem_handle_t *handles;
    u32 handle_num;
};

static DEFINE_MUTEX(secmem_lock);
#if 0
static struct cdev secmem_dev;
static struct class *secmem_class = NULL;
#endif
//#define SECMEM_UUID_SDRV_DEFINE { 0x08, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
static const struct mc_uuid_t secmem_uuid = {TL_SECMEM_UUID};
static struct mc_session_handle secmem_session = {0};
static u32 secmem_session_ref = 0;
static u32 secmem_devid = MC_DEVICE_ID_DEFAULT;
static tciMessage_t *secmem_tci = NULL;

static int secmem_execute(u32 cmd, struct secmem_param *param)
{
    enum mc_result mc_ret;

    mutex_lock(&secmem_lock);

    if (NULL == secmem_tci) {
        mutex_unlock(&secmem_lock);    
        MSG(ERR, "secmem_tci not exist\n");
        return -ENODEV;
    }

    secmem_tci->cmd_secmem.header.commandId = (tciCommandId_t)cmd;
    secmem_tci->cmd_secmem.len = 0;
    secmem_tci->sec_handle= param->sec_handle;
    secmem_tci->alignment = param->alignment;
    secmem_tci->size = param->size;
    secmem_tci->refcount = param->refcount;

    mc_ret = mc_notify(&secmem_session);

    if (MC_DRV_OK != mc_ret) {
        MSG(ERR, "mc_notify failed: %d", mc_ret);
        goto exit;
    }

    mc_ret = mc_wait_notification(&secmem_session, -1);

    if (MC_DRV_OK != mc_ret) {
        MSG(ERR, "mc_wait_notification failed: %d", mc_ret);
        goto exit;
    }

    /* correct handle should be get after return from secure world. */
    param->sec_handle = secmem_tci->sec_handle;
    param->refcount = secmem_tci->refcount;
    param->alignment = secmem_tci->alignment;
    param->size = secmem_tci->size;

    if (RSP_ID(cmd) != secmem_tci->rsp_secmem.header.responseId) {
        MSG(ERR, "trustlet did not send a response: %d",
            secmem_tci->rsp_secmem.header.responseId);
        mc_ret = MC_DRV_ERR_INVALID_RESPONSE;
        goto exit;
    }

    if (MC_DRV_OK != secmem_tci->rsp_secmem.header.returnCode) {
        MSG(ERR, "trustlet did not send a valid return code: %d",
            secmem_tci->rsp_secmem.header.returnCode);
        mc_ret = secmem_tci->rsp_secmem.header.returnCode;
    }

exit:

    mutex_unlock(&secmem_lock);

    if (MC_DRV_OK != mc_ret)
        return -ENOSPC;

    return 0;
}

static int secmem_handle_register(struct secmem_context *ctx, u32 type, u32 id)
{
    secmem_handle_t *handle;
    u32 i, num, nspace;

    spin_lock(&ctx->lock);

    num = ctx->handle_num;
    handle = ctx->handles;

    /* find empty space. */
    for (i = 0; i < num; i++, handle++) {
        if (handle->id == 0) {
            handle->id = id;
            handle->type = type;
            spin_unlock(&ctx->lock);
            return 0;
        }
    }

    /* try grow the space */
    nspace = num * 2;
    handle = (secmem_handle_t*)krealloc(ctx->handles, 
        nspace * sizeof(secmem_handle_t), GFP_KERNEL);
    if (handle == NULL) {
        spin_unlock(&ctx->lock);
        return -ENOMEM;
    }
    ctx->handle_num = nspace;
    ctx->handles = handle;

    handle += num;

    memset(handle, 0, (nspace - num) * sizeof(secmem_handle_t));

    handle->id = id;
    handle->type = type;    

    spin_unlock(&ctx->lock);
    
    return 0;
}

static void secmem_handle_unregister_check(struct secmem_context *ctx, u32 type, u32 id)
{
    secmem_handle_t *handle;
    u32 i, num;    

    spin_lock(&ctx->lock);

    num = ctx->handle_num;
    handle = ctx->handles;

    /* find empty space. */
    for (i = 0; i < num; i++, handle++) {
        if (handle->id == id) {
            if(handle->type != type){                
                MSG(ERR, "unref check failed, type mismatched (%d!=%d), handle=0x%x\n", 
                    _IOC_NR(handle->type), _IOC_NR(type), handle->id);
            }
            break;
        }
    }

    spin_unlock(&ctx->lock);
}

static int secmem_handle_unregister(struct secmem_context *ctx, u32 id)
{
    secmem_handle_t *handle;
    u32 i, num;

    spin_lock(&ctx->lock);

    num = ctx->handle_num;
    handle = ctx->handles;

    /* find empty space. */
    for (i = 0; i < num; i++, handle++) {
        if (handle->id == id) {
            memset(handle, 0, sizeof(secmem_handle_t));
            break;
        }
    }

    spin_unlock(&ctx->lock);

    return 0;
}

static int secmem_handle_cleanup(struct secmem_context *ctx)
{
    int ret = 0;
    u32 i, num, cmd=0;
    secmem_handle_t *handle;
    struct secmem_param param = {0};

    spin_lock(&ctx->lock);
    
    num = ctx->handle_num;
    handle = ctx->handles;
    
    for (i = 0; i < num; i++, handle++) {
        if (handle->id != 0) {
            param.sec_handle = handle->id;
            switch (handle->type) {
            case SECMEM_MEM_ALLOC:
                cmd = CMD_SEC_MEM_UNREF;
                break;
            case SECMEM_MEM_REF:
                cmd = CMD_SEC_MEM_UNREF;
                break;
            case SECMEM_MEM_ALLOC_TBL:
                cmd = CMD_SEC_MEM_UNREF_TBL;
                break;                
            default:
                MSG(ERR, "secmem_handle_cleanup: incorrect type=%d (ioctl:%d)\n", 
                    handle->type, _IOC_NR(handle->type));
                goto error;
                break;
            }
            spin_unlock(&ctx->lock);
            ret = secmem_execute(cmd, &param);
            MSG(INFO, "secmem_handle_cleanup: id=0x%x type=%d (ioctl:%d)\n", 
                handle->id, handle->type, _IOC_NR(handle->type));
            spin_lock(&ctx->lock);
        }
    }
    
error:    
    spin_unlock(&ctx->lock);

    return ret;   
}

static int secmem_session_open(void)
{
    enum mc_result mc_ret = MC_DRV_OK;

    mutex_lock(&secmem_lock);   

    do {
        /* sessions reach max numbers ? */
        if (secmem_session_ref > MAX_OPEN_SESSIONS) {
            MSG(WRN, "secmem_session > 0x%x\n", MAX_OPEN_SESSIONS);
            break;
        }

        if (secmem_session_ref > 0) {
            secmem_session_ref++;
            break;
        }
        
        /* open device */
        mc_ret = mc_open_device(secmem_devid);
        if (MC_DRV_OK != mc_ret) {
            MSG(ERR, "mc_open_device failed: %d\n", mc_ret);     
            break;
        }
        
        /* allocating WSM for DCI */        
        mc_ret = mc_malloc_wsm(secmem_devid, 0, sizeof(tciMessage_t), 
            (uint8_t **)&secmem_tci, 0);
        if (MC_DRV_OK != mc_ret) {
            mc_close_device(secmem_devid);
            MSG(ERR, "mc_malloc_wsm failed: %d\n", mc_ret);           
            break;
        }

        /* open session */
        secmem_session.device_id = secmem_devid;
        mc_ret = mc_open_session(&secmem_session, &secmem_uuid, 
            (uint8_t *)secmem_tci, sizeof(tciMessage_t));

        if (MC_DRV_OK != mc_ret) {
            mc_free_wsm(secmem_devid, (uint8_t *)secmem_tci);            
            mc_close_device(secmem_devid);
            secmem_tci = NULL;
            MSG(ERR, "mc_open_session failed: %d\n", mc_ret);
            break;
        }
        secmem_session_ref = 1;

    } while(0);

    MSG(INFO, "secmem_session_open: ret=%d, ref=%d\n", mc_ret, secmem_session_ref);

    mutex_unlock(&secmem_lock);

    if (MC_DRV_OK != mc_ret)
        return -ENXIO;

    return 0;
}
static int secmem_session_close(void)
{
    enum mc_result mc_ret = MC_DRV_OK;

    mutex_lock(&secmem_lock);

    do {
        /* session is already closed ? */
        if (secmem_session_ref == 0) {
            MSG(WRN, "secmem_session already closed\n");
            break;
        }

        if (secmem_session_ref > 1) {
            secmem_session_ref--;
            break;
        }

        /* close session */
        mc_ret = mc_close_session(&secmem_session);
        if (MC_DRV_OK != mc_ret) {
            MSG(ERR, "mc_close_session failed: %d\n", mc_ret);     
            break;
        }
        
        /* free WSM for DCI */        
        mc_ret = mc_free_wsm(secmem_devid, (uint8_t*)secmem_tci);
        if (MC_DRV_OK != mc_ret) {
            MSG(ERR, "mc_free_wsm failed: %d\n", mc_ret);           
            break;
        }
        secmem_tci = NULL;
        secmem_session_ref = 0;

        /* close device */
        mc_ret = mc_close_device(secmem_devid);
        if (MC_DRV_OK != mc_ret) {
            MSG(ERR, "mc_close_device failed: %d\n", mc_ret);
        }    

    } while(0);

    MSG(INFO, "secmem_session_close: ret=%d, ref=%d\n", mc_ret, secmem_session_ref);

    mutex_unlock(&secmem_lock);

    if (MC_DRV_OK != mc_ret)
        return -ENXIO;

    return 0;

}

static int secmem_open(struct inode *inode, struct file *file)
{
    struct secmem_context *ctx;

    /* allocate session context */
    ctx = kmalloc(sizeof(struct secmem_context), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;

    ctx->handle_num = DEFAULT_HANDLES_NUM;
    ctx->handles = kzalloc(sizeof(secmem_handle_t) * DEFAULT_HANDLES_NUM, GFP_KERNEL);    
    spin_lock_init(&ctx->lock);
    
    if (!ctx->handles) {
        kfree(ctx);
        return -ENOMEM;
    }

    /* open session */
    if (secmem_session_open() < 0) {
        kfree(ctx->handles);
        kfree(ctx);
        return -ENXIO;
    }

    file->private_data = (void*)ctx;

    return 0;
}

static int secmem_release(struct inode *inode, struct file *file)
{
    int ret = 0;
    struct secmem_context *ctx = (struct secmem_context *)file->private_data;

    if (ctx) {
        /* release session context */
        secmem_handle_cleanup(ctx);
        kfree(ctx->handles);
        kfree(ctx);
        file->private_data = NULL;

        /* close session */
        ret = secmem_session_close();
    }
    return ret;
}

static long secmem_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int err = 0;
    struct secmem_context *ctx = (struct secmem_context *)file->private_data;
    struct secmem_param param;
    u32 handle;

    if (_IOC_TYPE(cmd) != SECMEM_IOC_MAGIC)
        return -ENOTTY;
    if (_IOC_NR(cmd) > SECMEM_IOC_MAXNR)
        return -ENOTTY;

    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    if (_IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

    if (err) return -EFAULT;

    err = copy_from_user(&param, (void*)arg, sizeof(param));

    if (err) return -EFAULT;

    switch (cmd) {
    case SECMEM_MEM_ALLOC:
        if (!(file->f_mode & FMODE_WRITE)) {
            return -EROFS;
        }
        err = secmem_execute(CMD_SEC_MEM_ALLOC, &param);
        if (!err) {
            secmem_handle_register(ctx, SECMEM_MEM_ALLOC, param.sec_handle);
        }
        break;
    case SECMEM_MEM_REF:
        err = secmem_execute(CMD_SEC_MEM_REF, &param);
        if (!err) {
            secmem_handle_register(ctx, SECMEM_MEM_REF, param.sec_handle);
        }
        break;
    case SECMEM_MEM_UNREF:
        handle = param.sec_handle;
        secmem_handle_unregister_check(ctx, SECMEM_MEM_ALLOC, handle);
        err = secmem_execute(CMD_SEC_MEM_UNREF, &param);
        if (!err) {
            secmem_handle_unregister(ctx, handle);
        }
        break;
	case SECMEM_MEM_ALLOC_TBL:
        if (!(file->f_mode & FMODE_WRITE)) {
            return -EROFS;
        }
        err = secmem_execute(CMD_SEC_MEM_ALLOC_TBL, &param);
        if (!err) {
            secmem_handle_register(ctx, SECMEM_MEM_ALLOC_TBL, param.sec_handle);
        }
        break;
    case SECMEM_MEM_UNREF_TBL:
        handle = param.sec_handle;
        secmem_handle_unregister_check(ctx, SECMEM_MEM_ALLOC_TBL, handle);
        err = secmem_execute(CMD_SEC_MEM_UNREF_TBL, &param);
        if (!err) {
            secmem_handle_unregister(ctx, handle);
        }
        break;
    case SECMEM_MEM_USAGE_DUMP:
        if (!(file->f_mode & FMODE_WRITE)) {
            return -EROFS;
        }
        err = secmem_execute(CMD_SEC_MEM_USAGE_DUMP, &param);
        break;
    default:
        return -ENOTTY;
    }

    if (!err)
        err = copy_to_user((void*)arg, &param, sizeof(param));

    return err;
}

#define TEE_DEBUG_TESTING 0
#if TEE_DEBUG_TESTING
#include <mach/emi_mpu.h>
#include <mach/mt_secure_api.h>
static int secmem_write(struct file *file, const char __user *buffer, 
                size_t count, loff_t *data)
{
    char desc[32]; 
    int len = 0;
    char cmd[10];
    
    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if (sscanf(desc, "%s", cmd) == 1) { 
        if (!strcmp(cmd, "1")) {
            printk("[SECMEM] - test for command 1\n");
            tbase_trigger_aee_dump();
        } else if (!strcmp(cmd, "2")) {
            printk("[SECMEM] - test for command 2\n");
        }
    }

    return count;
}
#endif

static struct file_operations secmem_fops = {
    .owner   = THIS_MODULE,
    .open    = secmem_open,
    .release = secmem_release,
    .unlocked_ioctl = secmem_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = secmem_ioctl,
#endif
#if TEE_DEBUG_TESTING    
    .write   = secmem_write,
#else   
    .write   = NULL,
#endif
    .read    = NULL,
};

#if 0
static int __init secmem_init(void)
{
    int alloc_ret = -1;
    int cdev_ret = -1;
    int major;
    dev_t dev;
    struct device *device = NULL;

    alloc_ret = alloc_chrdev_region(&dev, 0, 1, SECMEM_NAME);

    if (alloc_ret)
        goto error;

    major = MAJOR(dev);

    cdev_init(&secmem_dev, &secmem_fops);
    secmem_dev.owner = THIS_MODULE;

    cdev_ret = cdev_add(&secmem_dev, MKDEV(major, 0), 1);
    if (cdev_ret)
        goto error;

    secmem_class = class_create(THIS_MODULE, SECMEM_NAME);

    if (IS_ERR(secmem_class))
        goto error;

    device = device_create(secmem_class, NULL, MKDEV(major, 0), NULL,
        SECMEM_NAME "%d", 0);

    if (IS_ERR(device))
        goto error;

    return 0;

error:

    if (secmem_class)
        class_destroy(secmem_class);
    
    if (cdev_ret == 0)
        cdev_del(&secmem_dev);

    if (alloc_ret == 0)
        unregister_chrdev_region(dev, 1);

    return -1;
}
#endif

static int __init secmem_init(void)
{
#if 0
    struct proc_dir_entry *secmem_proc;
    secmem_proc = create_proc_entry("secmem0", 
        (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH), NULL);
    
    if (IS_ERR(secmem_proc))
        goto error;

    secmem_proc->proc_fops = &secmem_fops;
#else
    proc_create("secmem0", (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH), NULL, &secmem_fops);
#endif

#if TEE_DEBUG_TESTING
    {
        unsigned int sec_mem_mpu_attr = SET_ACCESS_PERMISSON(FORBIDDEN, SEC_RW, SEC_RW, FORBIDDEN);
        unsigned int set_mpu_ret = 0;
        set_mpu_ret = emi_mpu_set_region_protection(0xF6000000, 0xFFFFFFFF, 0, sec_mem_mpu_attr);
        printk("[SECMEM] - test for set EMI MPU on region 0, ret:%d\n", set_mpu_ret);
    }
#endif

    return 0;

#if 0    
error:
    return -1;
#endif
}

late_initcall(secmem_init);
