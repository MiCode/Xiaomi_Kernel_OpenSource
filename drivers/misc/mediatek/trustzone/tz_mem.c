
/* Release secure chunk memory for normal world usage.
 *
 * For better utilization, part of secure chunk memory (pre-defined size) can be used by
 * normal world through memory TA.
 * After release, pre-defined secure memory can be read/write by normal world through memroy TA, 
 * and can not be used by secure world. After append, it can be used for secure world again.
 * For easy usage at user level, a block device can be registered, and it can access released
 * secure chunk memory by memory TA.
 *  
 * How to use secure chunk memory at user level:
 * 1) Create a block device node, ex: /dev/tzmem
 * 2) Release secure chunk memory by UREE_ReleaseSecurechunkmem.
 *    After releasing, the pre-defined chunk memory will be used by normal world only.
 * 3) Open /dev/tzmem for read/write
 * 4) If finishing to use, close it.
 * 5) Append secure chunk memory back to secure world usage by UREE_AppendSecurechunkmem.
 *    After appending, the pre-defined chunk memory will not be used by normal world.
 *
 * Or simply, using APIs
 * 1) UREE_ReleaseTzmem for release secure chunk memory to normal world usage.
 * 2) UREE_AppendTzmem for append secure chunk memory back to secure world.
 *
 */

//-----------------------------------------------------------------------------
// Include files
//-----------------------------------------------------------------------------

#include <trustzone/kree/mem.h>
#include "trustzone/kree/system.h"
#include <trustzone/tz_cross/ta_mem.h>
#include <linux/xlog.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include "trustzone/kree/tz_mem.h"

/* Use this define to enable module for TZMEM
*/
#define MTEE_TZMEM_ENABLE

/* enable debug logs
*/
#define MTEE_TZMEM_DBG

#define KREE_RELEASECM_MAX_SIZE 4096 // bytes


typedef struct {
    uint32_t control; // 0 = not released, 1 = released
    uint32_t size; // real released pool size in bytes
    uint32_t pool_size;
    struct gendisk *disk;
    KREE_SESSION_HANDLE session;
    KREE_RELEASECM_HANDLE handle;
} tzmem_diskinfo_t;

#ifdef MTEE_TZMEM_ENABLE
static uint32_t tzmem_poolIndex = 0; // currently, always 0. for future extension...
static tzmem_diskinfo_t _tzmem_diskInfo[IO_NODE_NUMBER_TZMEM];

static DEFINE_MUTEX(tzmem_probe_mutex);
static DEFINE_SPINLOCK(tzmem_blk_lock);

static TZ_RESULT _tzmem_get_poolsize (uint32_t *size)
{
    KREE_SESSION_HANDLE session;
    int ret = TZ_RESULT_SUCCESS;

    ret = KREE_CreateSession(TZ_TA_MEM_UUID, &session);
    if (ret != TZ_RESULT_SUCCESS)
    {
        xlog_printk(ANDROID_LOG_ERROR, MTEE_TZMEM_TAG, "[%s] _tzmem_get_poolsize: KREE_CreateSession Error = 0x%x\n", MODULE_NAME, ret);
        return ret;
    } 

    // get ta preset tzmem size
    ret = KREE_GetSecurechunkReleaseSize (session, size);
    if (ret != TZ_RESULT_SUCCESS)
    {
        xlog_printk(ANDROID_LOG_ERROR, MTEE_TZMEM_TAG, "[%s] _tzmem_get_poolsize: KREE_GetSecurechunkReleaseSize Error = 0x%x\n", MODULE_NAME, ret);
        KREE_CloseSession(session);
        return ret;
    }

    ret = KREE_CloseSession(session);
    if (ret != TZ_RESULT_SUCCESS)
    {
        xlog_printk(ANDROID_LOG_ERROR, MTEE_TZMEM_TAG, "[%s] _tzmem_get_poolsize: KREE_CloseSession Error = 0x%x\n", MODULE_NAME, ret);
        return ret;
    }

    return ret;
}

static long tzmem_gen_ioctl (dev_t dev, unsigned int cmd, unsigned long arg)
{
    int ret = 0;

#ifdef MTEE_TZMEM_DBG
    printk ("====> tzmem_gen_ioctl\n");
#endif    

    switch (cmd) 
    {        
        default:
            ret = -EINVAL;
    }

    return ret;
}

static void do_tzmem_blk_request(struct request_queue *q)
{
    struct request *req;
    uint32_t i;

#ifdef MTEE_TZMEM_DBG
    printk ("====> do_tzmem_blk_request\n");
#endif

    req = blk_fetch_request(q);
    while (req) 
    {
        unsigned long start = blk_rq_pos(req) << 9;
        unsigned long len  = blk_rq_cur_bytes(req);
        int err = 0;
        struct gendisk *disk = req->rq_disk;
        tzmem_diskinfo_t *diskInfo = (tzmem_diskinfo_t *)disk->private_data;
        KREE_SESSION_HANDLE session;

        session = diskInfo->session;

#ifdef MTEE_TZMEM_DBG
        printk ("====> 0x%x 0x%x\n", (uint32_t) session, diskInfo->size);
#endif

        if ((start + len > diskInfo->size) || (start > diskInfo->size) || (len > diskInfo->size)) 
        {
            err = -EIO;
            goto done;
        }
        
        if (rq_data_dir(req) == READ)
        {
#ifdef MTEE_TZMEM_DBG        
            printk ("====> do_tzmem_blk_request: read = 0x%x, 0x%x\n", (uint32_t) start, (uint32_t) len);
#endif            
            for (i = 0; i < len / KREE_RELEASECM_MAX_SIZE; i ++)
            {
                KREE_ReadSecurechunkmem ((KREE_SESSION_HANDLE) session,  
                    start + i * KREE_RELEASECM_MAX_SIZE, 
                    KREE_RELEASECM_MAX_SIZE, 
                    req->buffer + i * KREE_RELEASECM_MAX_SIZE);
            }
            if (len % KREE_RELEASECM_MAX_SIZE)
            {
                KREE_ReadSecurechunkmem ((KREE_SESSION_HANDLE) session,  
                    start + i * KREE_RELEASECM_MAX_SIZE, 
                    len % KREE_RELEASECM_MAX_SIZE, 
                    req->buffer + i * KREE_RELEASECM_MAX_SIZE);
            }
        }
        else
        {
#ifdef MTEE_TZMEM_DBG        
            printk ("====> do_tzmem_blk_request: write = 0x%x, 0x%x\n", (uint32_t) start, (uint32_t) len);
#endif            
            for (i = 0; i < len / KREE_RELEASECM_MAX_SIZE; i ++)
            {
                KREE_WriteSecurechunkmem ((KREE_SESSION_HANDLE) session, 
                    start + i * KREE_RELEASECM_MAX_SIZE, 
                    KREE_RELEASECM_MAX_SIZE, 
                    req->buffer + i * KREE_RELEASECM_MAX_SIZE);
            }
            if (len % KREE_RELEASECM_MAX_SIZE)
            {
                KREE_WriteSecurechunkmem ((KREE_SESSION_HANDLE) session, 
                    start + i * KREE_RELEASECM_MAX_SIZE, 
                    len % KREE_RELEASECM_MAX_SIZE, 
                    req->buffer + i * KREE_RELEASECM_MAX_SIZE);
            }
            
        }
        
done:
        if (!__blk_end_request_cur(req, err))
        {
            req = blk_fetch_request(q);
        }
    }
}

static int tzmem_blk_ioctl (struct block_device *bdev, fmode_t mode, unsigned cmd, unsigned long arg)
{
    return tzmem_gen_ioctl(bdev->bd_dev, cmd, arg);
}

/* block device module info
*/
static const struct block_device_operations tzmem_blk_fops = {
    .owner =        THIS_MODULE,
    .ioctl =        tzmem_blk_ioctl,
};

/* block device probe function
*/
// Create disk on demand. So we won't create lots of disk for un-used devices.
static struct kobject *tzmem_blk_probe(dev_t dev, int *part, void *data)
{
    uint32_t len;
    struct gendisk *disk;
    struct kobject *kobj;
    struct request_queue *queue;
    tzmem_diskinfo_t *diskInfo; 
    int ret;
    KREE_SESSION_HANDLE session;

#ifdef MTEE_TZMEM_DBG
    printk ("====> tzmem_blk_probe\n");
#endif    

    mutex_lock(&tzmem_probe_mutex);
    
    diskInfo = (tzmem_diskinfo_t *) &_tzmem_diskInfo[tzmem_poolIndex];
    if (diskInfo->disk == NULL)
    {
        disk = alloc_disk(1);
        if (!disk)
        {
            goto out_info;
        }

        queue = blk_init_queue(do_tzmem_blk_request, &tzmem_blk_lock);
        if (!queue)
        {
            goto out_queue;
        }
            
        blk_queue_max_hw_sectors(queue, 1024); 
        blk_queue_bounce_limit(queue, BLK_BOUNCE_ANY);

        if (_tzmem_get_poolsize (&len))
        {
            goto out_init;
        }

        disk->major = IO_NODE_MAJOR_TZMEM;
        disk->first_minor = MINOR(dev);
        disk->fops = &tzmem_blk_fops;
        disk->private_data = &_tzmem_diskInfo;
        snprintf(disk->disk_name, sizeof(disk->disk_name), "tzmem%d", MINOR(dev));
        disk->queue = queue;
        set_capacity(disk, len / 512);
        add_disk(disk);

        ret = KREE_CreateSession(TZ_TA_MEM_UUID, &session);
        if (ret != TZ_RESULT_SUCCESS)
        {
            xlog_printk(ANDROID_LOG_ERROR, MTEE_TZMEM_TAG, "[%s] _tzmem_get_poolsize: KREE_CreateSession Error = 0x%x\n", MODULE_NAME, ret);
            goto out_init;
        } 

        diskInfo->session = session;
        diskInfo->pool_size = len;
        diskInfo->disk = disk;
        diskInfo->size = len;
    }

    *part = 0;
    kobj = diskInfo ? get_disk(diskInfo->disk) : ERR_PTR(-ENOMEM);
    
    mutex_unlock(&tzmem_probe_mutex);
    return kobj;

out_init:
    blk_cleanup_queue(queue);
out_queue:
    put_disk(disk);
out_info:
    mutex_unlock(&tzmem_probe_mutex);
    return ERR_PTR(-ENOMEM);
}

/* tzmem block device module init
*/
//static struct class*  pTzClass  = NULL;
//static struct device* pTzDevice = NULL;
static dev_t tz_client_dev;

static int __init tzmem_blkdev_init(void)
{
#ifdef MTEE_TZMEM_DBG
    printk ("====> tzmem_blkdev_init\n");
#endif        

    if (register_blkdev(IO_NODE_MAJOR_TZMEM, DEV_TZMEM))
    {
        xlog_printk(ANDROID_LOG_ERROR, MTEE_TZMEM_TAG, "[%s] tzmem_blkdev_init: register_blkdev error\n", MODULE_NAME);
        return -EFAULT;
    }

    tz_client_dev = MKDEV(IO_NODE_MAJOR_TZMEM, IO_NODE_MINOR_TZMEM);
    blk_register_region(tz_client_dev, IO_NODE_NUMBER_TZMEM,
        THIS_MODULE, tzmem_blk_probe, NULL, NULL); 

#if 0
    /* create /dev/tzmem automaticly */
    pTzClass = class_create(THIS_MODULE, DEV_TZMEM);
    if (IS_ERR(pTzClass)) {
        int ret = PTR_ERR(pTzClass);
        xlog_printk(ANDROID_LOG_ERROR, MTEE_TZMEM_TAG ,"[%s] could not create class for the device, ret:%d\n", MODULE_NAME, ret);
        return ret;
    }
    pTzDevice = device_create(pTzClass, NULL, tz_client_dev, NULL, DEV_TZMEM);        
#endif

    return 0;
}

module_init(tzmem_blkdev_init);
#endif

