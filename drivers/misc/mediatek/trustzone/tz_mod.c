#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/xlog.h>
#include <linux/pagemap.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/platform_device.h>

#include "trustzone/kree/tz_mod.h"
#include "trustzone/kree/mem.h"
#include "trustzone/kree/system.h"
#include "trustzone/kree/tz_pm.h"
#include "trustzone/kree/tz_irq.h"
#include "kree_int.h"
#include "tz_counter.h"
#include "trustzone/tz_cross/ta_pm.h"
#include "trustzone/tz_cross/ta_mem.h"
#include "tz_ndbg.h"

#define MTEE_MOD_TAG "MTEE_MOD"

#define TZ_PAGESIZE 0x1000 // fix me!!!! need global define

#define PAGE_SHIFT 12 // fix me!!!! need global define

#define TZ_DEVNAME "mtk_tz"


/**************************************************************************
 *  TZ MODULE PARAMETER
 **************************************************************************/ 
static uint memsz = 0;
module_param(memsz, uint, S_IRUSR|S_IRGRP|S_IROTH); /* r--r--r-- */
MODULE_PARM_DESC(memsz, "the memory size occupied by tee");

/**Used for mapping user space address to physical memory
*/
typedef struct
{
    uint32_t start;
    uint32_t size;
    uint32_t pageArray;
    uint32_t nrPages;
} MTIOMMU_PIN_RANGE_T;

/*****************************************************************************
* FUNCTION DEFINITION
*****************************************************************************/
static struct cdev tz_client_cdev;
static dev_t tz_client_dev;
static int tz_client_open(struct inode *inode, struct file *filp);
static int tz_client_release(struct inode *inode, struct file *filp);
static long tz_client_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

static int tz_client_init_client_info(struct file *file);
static void tz_client_free_client_info(struct file *file);

#define TZ_CLIENT_INIT_HANDLE_SPACE  32
#define TZ_CLIENT_INIT_SHAREDMEM_SPACE  32


struct tz_sharedmem_info {
    KREE_SHAREDMEM_HANDLE mem_handle;
    KREE_SESSION_HANDLE session_handle;
    uint32_t *resouce;
};

struct tz_client_info
{
    int handle_num;
    struct mutex mux;
    KREE_SESSION_HANDLE *handles;
    struct tz_sharedmem_info *shm_info;
    int shm_info_num;
};


static const struct file_operations tz_client_fops = {
    .open = tz_client_open,
    .release = tz_client_release,
    .unlocked_ioctl = tz_client_ioctl,
    .owner = THIS_MODULE,
    };

static int tz_client_open(struct inode *inode, struct file *filp)
{
    return tz_client_init_client_info(filp);
}

static int tz_client_release(struct inode *inode, struct file *filp)
{
    tz_client_free_client_info(filp);

    return 0;
}

/* map user space pages */
/* control -> 0 = write, 1 = read only memory */
static long _map_user_pages (MTIOMMU_PIN_RANGE_T* pinRange, uint32_t uaddr, uint32_t size, uint32_t control)
{
    int nr_pages;
    unsigned int first,last;
    struct page **pages;
    int res, j;
    uint32_t write;

    if ((uaddr == 0) || (size == 0))
    {
        return -EFAULT;
    }

    pinRange->start = uaddr;
    pinRange->size = size;

    first = (uaddr & PAGE_MASK) >> PAGE_SHIFT;
    last  = ((uaddr + size + PAGE_SIZE - 1) & PAGE_MASK) >> PAGE_SHIFT;
    nr_pages = last-first;
    if((pages = kzalloc(nr_pages * sizeof(struct page), GFP_KERNEL)) == NULL)
    {
        return -ENOMEM;
    }

    pinRange->pageArray = (uint32_t)pages;
    write = (control == 0) ? 1 : 0;

    /* Try to fault in all of the necessary pages */
    down_read(&current->mm->mmap_sem);
    res = get_user_pages(
            current,
            current->mm,
            uaddr,
            nr_pages,
            write, 
            0, /* don't force */
            pages,
            NULL);
    up_read(&current->mm->mmap_sem);
    if (res < 0)
    {
        printk ("_map_user_pages error = 0x%x\n", res);
        goto out_free;
    }

    pinRange->nrPages = res;
    /* Errors and no page mapped should return here */
    if (res < nr_pages)
        goto out_unmap;

    //printk("_map_user_pages success: (start: 0x%x, size: 0x%x, #pages: 0x%x)\n", pinRange->start, pinRange->size, pinRange->nrPages);

    return 0;

out_unmap:
    printk("_map_user_pages fail\n");
    if (res > 0)
    {
        for (j=0; j < res; j++)
        {
            put_page(pages[j]);
        }
    }
    res = -EFAULT;
out_free:
    kfree(pages);
    return res;
}

static void _unmap_user_pages (MTIOMMU_PIN_RANGE_T* pinRange)
{
    int res;
    int j;
    struct page **pages;

    pages = (struct page **) pinRange->pageArray;

    res = pinRange->nrPages;

    if (res > 0)
    {
        for (j=0; j < res; j++)
        {
            put_page(pages[j]);
        }
        res = 0;
    }

    kfree(pages);
}

static struct tz_sharedmem_info *tz_get_sharedmem (struct tz_client_info *info, KREE_SHAREDMEM_HANDLE handle)
{
    struct tz_sharedmem_info *shm_info;
    int i;

    // search handle
    shm_info = NULL;
    for (i = 0; i < info->shm_info_num; i ++)
    {
        if (info->shm_info[i].mem_handle == handle)
        {
            shm_info = &info->shm_info[i];
            break;
        }
    }

    return shm_info;
}



/**************************************************************************
*  DEV tz_client_info handling
**************************************************************************/
static int
tz_client_register_session(struct file *file, KREE_SESSION_HANDLE handle)
{
    struct tz_client_info *info;
    int i, num, nspace, ret = -1;
    void *ptr;

    info = (struct tz_client_info*)file->private_data;

    // lock
    mutex_lock(&info->mux);

    // find empty space.
    num = info->handle_num;
    for (i=0; i<num; i++)
    {
        if ((int)info->handles[i] == 0)
        {
            ret = i;
            break;
        }
    }

    if (ret == -1)
    {
        // Try grow the space
        nspace = num * 2;
        ptr = krealloc(info->handles, nspace * sizeof(KREE_SESSION_HANDLE),
                       GFP_KERNEL);
        if (ptr == 0)
        {
            mutex_unlock(&info->mux);
            return -ENOMEM;
        }

        ret = num;
        info->handle_num = nspace;
        info->handles = (KREE_SESSION_HANDLE*)ptr;
        memset(&info->handles[num], 0,
               (nspace - num) * sizeof(KREE_SESSION_HANDLE));
    }

    if (ret >= 0)
        info->handles[ret] = handle;

    // unlock
    mutex_unlock(&info->mux);
    return ret+1;
}

static KREE_SESSION_HANDLE tz_client_get_session(struct file *file, int handle)
{
    struct tz_client_info *info;
    KREE_SESSION_HANDLE rhandle;

    info = (struct tz_client_info*)file->private_data;

    // lock
    mutex_lock(&info->mux);

    if (handle <= 0 || handle > info->handle_num)
        rhandle = (KREE_SESSION_HANDLE)0;
    else
        rhandle = info->handles[handle-1];

    // unlock
    mutex_unlock(&info->mux);
    return rhandle;
}

static int tz_client_unregister_session(struct file *file, int handle)
{
    struct tz_client_info *info;
    int ret = 0;

    info = (struct tz_client_info*)file->private_data;

    // lock
    mutex_lock(&info->mux);

    if (handle <= 0 || handle > info->handle_num || info->handles[handle-1] == 0)
        ret = -EINVAL;
    else
        info->handles[handle-1] = (KREE_SESSION_HANDLE)0;

    // unlock
    mutex_unlock(&info->mux);
    return ret;
}

static int
tz_client_register_sharedmem(struct file *file, KREE_SESSION_HANDLE handle, KREE_SHAREDMEM_HANDLE mem_handle, uint32_t *resource)
{
    struct tz_client_info *info;
    int i, num, nspace, ret = -1;
    void *ptr;

    info = (struct tz_client_info*)file->private_data;

    // lock
    mutex_lock(&info->mux);

    // find empty space.
    num = info->shm_info_num;
    for (i=0; i<num; i++)
    {
        if ((int)info->shm_info[i].mem_handle == 0)
        {
            ret = i;
            break;
        }
    }

    if (ret == -1)
    {
        // Try grow the space
        nspace = num * 2;
        ptr = krealloc(info->shm_info, nspace * sizeof(struct tz_sharedmem_info),
                       GFP_KERNEL);
        if (ptr == 0)
        {
            mutex_unlock(&info->mux);
            return -ENOMEM;
        }

        ret = num;
        info->shm_info_num = nspace;
        info->shm_info = (struct tz_sharedmem_info *) ptr;
        memset(&info->shm_info[num], 0,
               (nspace - num) * sizeof(struct tz_sharedmem_info));
    }

    if (ret >= 0)
    {
        info->shm_info[ret].mem_handle = mem_handle;
        info->shm_info[ret].session_handle = handle;
        info->shm_info[ret].resouce = resource;
    }

    // unlock
    mutex_unlock(&info->mux);

    return ret;
}

static int tz_client_unregister_sharedmem(struct file *file, KREE_SHAREDMEM_HANDLE handle)
{
    struct tz_client_info *info;
    struct tz_sharedmem_info *shm_info;
    MTIOMMU_PIN_RANGE_T *pin;
    int ret = 0;

    info = (struct tz_client_info*)file->private_data;

    // lock
    mutex_lock(&info->mux);

    shm_info = tz_get_sharedmem(info, handle);

    if ((shm_info == NULL) || (shm_info->mem_handle == 0))
        ret = -EINVAL;
    else
    {
        pin = (MTIOMMU_PIN_RANGE_T *) shm_info->resouce;
        _unmap_user_pages(pin);
        kfree(pin);
        memset (shm_info, 0, sizeof (struct tz_sharedmem_info));
    }

    // unlock
    mutex_unlock(&info->mux);

    return ret;
}


static int tz_client_init_client_info(struct file *file)
{
    struct tz_client_info *info;

    info = (struct tz_client_info*)
           kmalloc(sizeof(struct tz_client_info), GFP_KERNEL);
    if (!info)
        return -ENOMEM;

    info->handles = (KREE_SESSION_HANDLE*)kzalloc(
            TZ_CLIENT_INIT_HANDLE_SPACE * sizeof(KREE_SESSION_HANDLE),
            GFP_KERNEL);
    if (!info->handles)
    {
        kfree(info);
        return -ENOMEM;
    }
    info->handle_num = TZ_CLIENT_INIT_HANDLE_SPACE;

    // init shared memory
    info->shm_info = (struct tz_sharedmem_info *)kzalloc(
            TZ_CLIENT_INIT_SHAREDMEM_SPACE * sizeof (struct tz_sharedmem_info),
            GFP_KERNEL);
    if (!info->shm_info)
    {
        kfree(info->handles);
        kfree(info);
        return -ENOMEM;
    }
    info->shm_info_num = TZ_CLIENT_INIT_SHAREDMEM_SPACE;

    mutex_init(&info->mux);
    file->private_data = info;
    return 0;
}

static void tz_client_free_client_info(struct file *file)
{
    struct tz_client_info *info;
    struct tz_sharedmem_info *shm_info;
    MTIOMMU_PIN_RANGE_T *pin;
    int i, num;

    info = (struct tz_client_info*)file->private_data;

    // lock
    mutex_lock(&info->mux);

    num = info->handle_num;
    for (i=0; i<num; i++)
    {
        if (info->handles[i] == 0)
            continue;

        KREE_CloseSession(info->handles[i]);
        info->handles[i] = (KREE_SESSION_HANDLE)0;
    }

    // unregister shared memory
    num = info->shm_info_num;
    for (i=0; i<num; i++)
    {
        if (info->shm_info[i].mem_handle == 0)
            continue;

        shm_info = tz_get_sharedmem (info, info->shm_info[i].mem_handle);
        if (shm_info == NULL)
        {
            continue;
        }

        pin = (MTIOMMU_PIN_RANGE_T *) shm_info->resouce;

        _unmap_user_pages (pin);
        kfree (pin);
    }

    // unlock
    file->private_data = 0;
    mutex_unlock(&info->mux);
    kfree(info->handles);
    kfree(info->shm_info);
    kfree(info);
}

/**************************************************************************
*  DEV DRIVER IOCTL
**************************************************************************/
static long tz_client_open_session(struct file *file, unsigned long arg)
{
    struct kree_session_cmd_param param;
    unsigned long cret;
    char uuid[40];
    long len;
    TZ_RESULT ret;
    KREE_SESSION_HANDLE handle;

    cret = copy_from_user(&param, (void*)arg, sizeof(param));
    if (cret)
        return -EFAULT;

    // Check if can we access UUID string. 10 for min uuid len.
    if (!access_ok(VERIFY_READ, param.data, 10))
        return -EFAULT;

    len = strncpy_from_user(uuid, param.data, sizeof(uuid));
    if (len <= 0)
        return -EFAULT;

    uuid[sizeof(uuid)-1]=0;
    ret = KREE_CreateSession(uuid, &handle);
    param.ret = ret;

    // Register session to fd
    if (ret == TZ_RESULT_SUCCESS)
    {
        param.handle = tz_client_register_session(file, handle);
        if (param.handle < 0)
            goto error_register;
    }

    cret = copy_to_user((void*)arg, &param, sizeof(param));
    if (cret)
        goto error_copy;

    return 0;

error_copy:
    tz_client_unregister_session(file, param.handle);
error_register:
    KREE_CloseSession(handle);
    return -EFAULT;
}

static long tz_client_close_session(struct file *file, unsigned long arg)
{
    struct kree_session_cmd_param param;
    unsigned long cret;
    TZ_RESULT ret;
    KREE_SESSION_HANDLE handle;

    cret = copy_from_user(&param, (void*)arg, sizeof(param));
    if (cret)
        return -EFAULT;

    handle = tz_client_get_session(file, param.handle);
    if (handle == 0)
        return -EINVAL;

    tz_client_unregister_session(file, param.handle);
    ret = KREE_CloseSession(handle);
    param.ret = ret;

    cret = copy_to_user((void*)arg, &param, sizeof(param));
    if (cret)
        return -EFAULT;

    return 0;
}

static long tz_client_tee_service(struct file *file, unsigned long arg)
{
    struct kree_tee_service_cmd_param cparam;
    unsigned long cret;
    uint32_t tmpTypes;
    MTEEC_PARAM param[4], oparam[4];
    int i;
    TZ_RESULT ret;
    KREE_SESSION_HANDLE handle;

    cret = copy_from_user(&cparam, (void*)arg, sizeof(cparam));
    if (cret)
        return -EFAULT;

    if (cparam.paramTypes != TZPT_NONE || cparam.param)
    {
        // Check if can we access param
        if (!access_ok(VERIFY_READ, cparam.param, sizeof(oparam)))
            return -EFAULT;

        cret = copy_from_user(oparam, (void*)cparam.param, sizeof(oparam));
        if (cret)
            return -EFAULT;
    }

    // Check handle
    handle = tz_client_get_session(file, cparam.handle);
    if (handle <= 0)
        return -EINVAL;

    // Parameter processing.
    memset(param, 0, sizeof(param));
    tmpTypes = cparam.paramTypes;
    for (i=0; tmpTypes; i++)
    {
        TZ_PARAM_TYPES type = tmpTypes & 0xff;
        tmpTypes >>= 8;
        switch (type)
        {
            case TZPT_VALUE_INPUT:
            case TZPT_VALUE_INOUT:
                param[i] = oparam[i];
                break;

            case TZPT_VALUE_OUTPUT:
            case TZPT_NONE:
                // Nothing to do
                break;

            case TZPT_MEM_INPUT:
            case TZPT_MEM_OUTPUT:
            case TZPT_MEM_INOUT:
                // Mem Access check
                if (type != TZPT_MEM_OUTPUT)
                {
                    if (!access_ok(VERIFY_READ, oparam[i].mem.buffer,
                                   oparam[i].mem.size))
                    {
                        cret = -EFAULT;
                        goto error;
                    }
                }
                if (type != TZPT_MEM_INPUT)
                {
                    if (!access_ok(VERIFY_WRITE, oparam[i].mem.buffer,
                                   oparam[i].mem.size))
                    {
                        cret = -EFAULT;
                        goto error;
                    }
                }

                // Allocate kernel space memory. Fail if > 4kb
                if (oparam[i].mem.size > TEE_PARAM_MEM_LIMIT)
                {
                    cret = -ENOMEM;
                    goto error;
                }

                param[i].mem.size = oparam[i].mem.size;
                param[i].mem.buffer = kmalloc(param[i].mem.size, GFP_KERNEL);
                if (!param[i].mem.buffer)
                {
                    cret = -ENOMEM;
                    goto error;
                }

                if (type != TZPT_MEM_OUTPUT)
                {
                    cret = copy_from_user(param[i].mem.buffer,
                                          (void*)oparam[i].mem.buffer,
                                          param[i].mem.size);
                    if (cret)
                    {
                        cret = -EFAULT;
                        goto error;
                    }
                }
                break;

            case TZPT_MEMREF_INPUT:
            case TZPT_MEMREF_OUTPUT:
            case TZPT_MEMREF_INOUT:
                // Check if share memory is valid.
                // Not done yet.
                param[i] = oparam[i];
                break;

            default:
                // Bad format, return.
                ret = TZ_RESULT_ERROR_BAD_FORMAT;
                goto error;
        }
    }

    // Execute.
    ret = KREE_TeeServiceCallNoCheck(handle, cparam.command, cparam.paramTypes, param);

    // Copy memory back.
    cparam.ret = ret;
    tmpTypes = cparam.paramTypes;
    for (i=0; tmpTypes; i++)
    {
        TZ_PARAM_TYPES type = tmpTypes & 0xff;
        tmpTypes >>= 8;
        switch (type)
        {
            case TZPT_VALUE_OUTPUT:
            case TZPT_VALUE_INOUT:
                oparam[i] = param[i];
                break;

            default:
                // This should never happen
            case TZPT_MEMREF_INPUT:
            case TZPT_MEMREF_OUTPUT:
            case TZPT_MEMREF_INOUT:
            case TZPT_VALUE_INPUT:
            case TZPT_NONE:
                // Nothing to do
                break;

            case TZPT_MEM_INPUT:
            case TZPT_MEM_OUTPUT:
            case TZPT_MEM_INOUT:
                if (type != TZPT_MEM_INPUT)
                {
                    cret = copy_to_user((void*)oparam[i].mem.buffer,
                                        param[i].mem.buffer,
                                        param[i].mem.size);
                    if (cret)
                    {
                        cret = -EFAULT;
                        goto error;
                    }
                }

                if (param[i].mem.buffer)
                {
                    kfree(param[i].mem.buffer);
                    param[i].mem.buffer = 0;
                }
                break;
        }
    }

    // Copy data back.
    if (cparam.paramTypes != TZPT_NONE)
    {
        cret = copy_to_user((void*)cparam.param, oparam, sizeof(oparam));
        if (cret)
            return -EFAULT;
    }

    cret = copy_to_user((void*)arg, &cparam, sizeof(cparam));
    if (cret)
        return -EFAULT;
    return 0;

error:
    tmpTypes = cparam.paramTypes;
    for (i=0; tmpTypes; i++)
    {
        TZ_PARAM_TYPES type = tmpTypes & 0xff;
        tmpTypes >>= 8;
        switch (type)
        {
            case TZPT_MEM_INPUT:
            case TZPT_MEM_OUTPUT:
            case TZPT_MEM_INOUT:
                if (param[i].mem.buffer)
                    kfree(param[i].mem.buffer);
                break;

            default:
                // Don't care.
                break;
        }
    }

    return cret;
}

static long tz_client_reg_sharedmem (struct file *file, unsigned long arg)
{
    unsigned long cret;
    struct kree_sharedmemory_cmd_param cparam;
    KREE_SESSION_HANDLE session;
    uint32_t mem_handle;
    MTIOMMU_PIN_RANGE_T *pin;
    uint32_t *map_p;
    TZ_RESULT ret;
    struct page **page;
    int i;
    long errcode;

	cret = copy_from_user(&cparam, (void*)arg, sizeof(cparam));
    if (cret)
    {
        return -EFAULT;
    }

    // session handle
    session = tz_client_get_session(file, cparam.session);
    if (session <= 0)
    {
        return -EINVAL;
    }

    /* map pages
       */
    // 1. get user pages
    // note: 'pin' resource need to keep for unregister. It will be freed after unregisted.
    if((pin = kzalloc (sizeof (MTIOMMU_PIN_RANGE_T), GFP_KERNEL)) == NULL)
    {
        errcode = -ENOMEM;
        goto client_regshm_mapfail;
    }
    cret = _map_user_pages (pin, (uint32_t) cparam.buffer, cparam.size, cparam.control);
    if (cret != 0)
    {
        printk ("tz_client_reg_sharedmem fail: map user pages = 0x%x\n", (uint32_t) cret);
        errcode = -EFAULT;
        goto client_regshm_mapfail_1;
    }

    // 2. build PA table
    if((map_p = kzalloc(sizeof (uint32_t) * (pin->nrPages + 1), GFP_KERNEL)) == NULL)
    {
        errcode = -ENOMEM;
        goto client_regshm_mapfail_2;
    }
    map_p[0] = pin->nrPages;
    page = (struct page **) pin->pageArray;
    for (i = 0; i < pin->nrPages; i ++)
    {
		map_p[1 + i] = PFN_PHYS(page_to_pfn(page[i])); // get PA
		//printk ("tz_client_reg_sharedmem ---> 0x%x\n", map_p[1+i]);
    }

    /* register it ...
       */
    ret = kree_register_sharedmem (session, &mem_handle, (uint32_t) pin->start, pin->size, (uint32_t) map_p);
    if (ret != TZ_RESULT_SUCCESS)
    {
        printk ("tz_client_reg_sharedmem fail: register shm 0x%x\n", ret);
        kfree (map_p);
        errcode = -EFAULT;
        goto client_regshm_free;
    }
    kfree (map_p);

    /* register to fd
       */
    cret = tz_client_register_sharedmem (file, session, (KREE_SHAREDMEM_HANDLE) mem_handle, (uint32_t *)pin);
    if (cret < 0)
    {
        printk ("tz_client_reg_sharedmem fail: register fd 0x%x\n", (uint32_t) cret);
        errcode = -EFAULT;
        goto client_regshm_free_1;
    }

    cparam.mem_handle = mem_handle;
    cparam.ret = ret; // TEE service call return
    cret = copy_to_user((void*)arg, &cparam, sizeof(cparam));
    if (cret)
    {
        return -EFAULT;
    }

    return TZ_RESULT_SUCCESS;

client_regshm_free_1:
    kree_unregister_sharedmem (session, mem_handle);
client_regshm_free:
    _unmap_user_pages (pin);
    kfree (pin);
    cparam.ret = ret; // TEE service call return
    cret = copy_to_user((void*)arg, &cparam, sizeof(cparam));
    printk ("tz_client_reg_sharedmem fail: shm reg\n");
    return errcode;

client_regshm_mapfail_2:
    _unmap_user_pages (pin);
client_regshm_mapfail_1:
    kfree (pin);
client_regshm_mapfail:
    printk ("tz_client_reg_sharedmem fail: map memory\n");
    return errcode;
}

static long tz_client_unreg_sharedmem (struct file *file, unsigned long arg)
{
    unsigned long cret;
    struct kree_sharedmemory_cmd_param cparam;
    KREE_SESSION_HANDLE session;
    TZ_RESULT ret;

	cret = copy_from_user(&cparam, (void*)arg, sizeof(cparam));
    if (cret)
    {
        return -EFAULT;
    }

    // session handle
    session = tz_client_get_session(file, cparam.session);
    if (session <= 0)
    {
        return -EINVAL;
    }

    /* Unregister
       */
    ret = kree_unregister_sharedmem (session, (uint32_t) cparam.mem_handle);
    if (ret != TZ_RESULT_SUCCESS)
    {
        printk ("tz_client_unreg_sharedmem: 0x%x\n", ret);
        cparam.ret = ret;
        cret = copy_to_user((void*)arg, &cparam, sizeof(cparam));
        return -EFAULT;
    }

    /* unmap user pages and unregister to fd
       */
    ret = tz_client_unregister_sharedmem (file, cparam.mem_handle);
    if (ret != TZ_RESULT_SUCCESS)
    {
        printk ("tz_client_unreg_sharedmem: unregister shm = 0x%x\n", ret);
        return -EFAULT;
    }

    cparam.ret = ret;
    cret = copy_to_user((void*)arg, &cparam, sizeof(cparam));
    if (cret)
    {
        return -EFAULT;
    }

    return TZ_RESULT_SUCCESS;
}



static long tz_client_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int err   = 0;

    /* ---------------------------------- */
    /* IOCTL                              */
    /* ---------------------------------- */
    if (_IOC_TYPE(cmd) != MTEE_IOC_MAGIC)
        return -ENOTTY;
    if (_IOC_NR(cmd) > DEV_IOC_MAXNR)
        return -ENOTTY;

    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    if (_IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

    if (err) return -EFAULT;

    switch (cmd)
    {
        case MTEE_CMD_OPEN_SESSION:
            return tz_client_open_session(file, arg);

        case MTEE_CMD_CLOSE_SESSION:
            return tz_client_close_session(file, arg);

        case MTEE_CMD_TEE_SERVICE:
            return tz_client_tee_service(file, arg);

		case MTEE_CMD_SHM_REG:
		    return tz_client_reg_sharedmem (file, arg);

		case MTEE_CMD_SHM_UNREG:
		    return tz_client_unreg_sharedmem (file, arg);

        default:
            return -ENOTTY;
    }

    return 0;
}

/* pm op funcstions */
static int tz_suspend(struct device *pdev)
{
    TZ_RESULT tzret;
    tzret = kree_pm_device_ops(MTEE_SUSPEND);
    return (tzret != TZ_RESULT_SUCCESS)?(-EBUSY):(0);
}

static int tz_suspend_late(struct device *pdev)
{
    TZ_RESULT tzret;
    tzret = kree_pm_device_ops(MTEE_SUSPEND_LATE);
    return (tzret != TZ_RESULT_SUCCESS)?(-EBUSY):(0);
}

static int tz_resume(struct device *pdev)
{
    TZ_RESULT tzret;
    tzret = kree_pm_device_ops(MTEE_RESUME);
    return (tzret != TZ_RESULT_SUCCESS)?(-EBUSY):(0);
}

static int tz_resume_early(struct device *pdev)
{
    TZ_RESULT tzret;
    tzret = kree_pm_device_ops(MTEE_RESUME_EARLY);
    return (tzret != TZ_RESULT_SUCCESS)?(-EBUSY):(0);
}

static const struct dev_pm_ops tz_pm_ops = {
        .suspend_late   = tz_suspend_late,
        .freeze_late    = tz_suspend_late,
        .resume_early   = tz_resume_early,
        .thaw_early     = tz_resume_early,
        SET_SYSTEM_SLEEP_PM_OPS(tz_suspend, tz_resume)
};

/* add tz virtual driver for suspend/resume support */
static struct platform_driver tz_driver = {
        .driver     = {
                      .name = TZ_DEVNAME,
                      .pm = &tz_pm_ops,
                      .owner = THIS_MODULE,
        },
};


/* add tz virtual device for suspend/resume support */
static struct platform_device tz_device = {
        .name    = TZ_DEVNAME,
        .id      = -1,
};

/******************************************************************************
 * register_tz_driver
 *
 * DESCRIPTION:
 *   register the device driver !
 *
 * PARAMETERS:
 *   None
 *
 * RETURNS:
 *   0 for success
 *
 * NOTES:
 *   None
 *
 ******************************************************************************/
static int __init register_tz_driver(void)
{
    int ret = 0;
    if(platform_device_register(&tz_device))
    {
        ret = -ENODEV;
        xlog_printk(ANDROID_LOG_ERROR, MTEE_MOD_TAG ,"[%s] could not register device for the device, ret:%d\n", MODULE_NAME, ret);
        return ret;
    }

    if(platform_driver_register(&tz_driver))
    {
        ret = -ENODEV;
        xlog_printk(ANDROID_LOG_ERROR, MTEE_MOD_TAG ,"[%s] could not register device for the device, ret:%d\n", MODULE_NAME, ret);
        platform_device_unregister(&tz_device);
        return ret;
    }

    return ret;
}

/******************************************************************************
 * tz_client_init
 *
 * DESCRIPTION:
 *   Init the device driver !
 *
 * PARAMETERS:
 *   None
 *
 * RETURNS:
 *   0 for success
 *
 * NOTES:
 *   None
 *
 ******************************************************************************/
static struct class*  pTzClass  = NULL;
static struct device* pTzDevice = NULL;

static int __init tz_client_init(void)
{
    int ret = 0;
    TZ_RESULT tzret;
    KREE_SESSION_HANDLE session;
#ifdef ENABLE_INC_ONLY_COUNTER
    struct task_struct *thread;
#endif
    ret = register_tz_driver();
    if (ret)
    {
	xlog_printk(ANDROID_LOG_ERROR, MTEE_MOD_TAG ,"[%s] register device/driver failed, ret:%d\n", MODULE_NAME, ret);
        return ret;
    }

    tz_client_dev = MKDEV(MAJOR_DEV_NUM, 0);

	xlog_printk(ANDROID_LOG_INFO, MTEE_MOD_TAG ," init\n");

	ret = register_chrdev_region(tz_client_dev, 1, TZ_DEV_NAME );
	if (ret)
	{
	    xlog_printk(ANDROID_LOG_ERROR, MTEE_MOD_TAG ,"[%s] register device failed, ret:%d\n", MODULE_NAME, ret);
	    return ret;
    }

	/* initialize the device structure and register the device  */
    cdev_init(&tz_client_cdev, &tz_client_fops);
    tz_client_cdev.owner = THIS_MODULE;

    if ((ret = cdev_add(&tz_client_cdev, tz_client_dev, 1)) < 0)
    {
        xlog_printk(ANDROID_LOG_ERROR, MTEE_MOD_TAG ,"[%s] could not allocate chrdev for the device, ret:%d\n", MODULE_NAME, ret);
        return ret;
    }

    tzret = KREE_InitTZ();
    if (tzret != TZ_RESULT_SUCCESS)
    {
        printk("tz_client_init: TZ Faild %d\n", (int)tzret);
        BUG();
    }

    kree_irq_init();
    kree_pm_init();

#ifdef ENABLE_INC_ONLY_COUNTER
    thread = kthread_run(update_counter_thread, NULL, "update_tz_counter");
#endif

    printk("tz_client_init: successfully\n");

    //tz_test();

    #ifdef CC_ENABLE_NDBG
    tz_ndbg_init();
    #endif

    /* create /dev/trustzone automaticly */
    pTzClass = class_create(THIS_MODULE, TZ_DEV_NAME);
    if (IS_ERR(pTzClass)) {
        int ret = PTR_ERR(pTzClass);
        xlog_printk(ANDROID_LOG_ERROR, MTEE_MOD_TAG ,"[%s] could not create class for the device, ret:%d\n", MODULE_NAME, ret);
        return ret;
    }
    pTzDevice = device_create(pTzClass, NULL, tz_client_dev, NULL, TZ_DEV_NAME);

    tzret = KREE_CreateSession(TZ_TA_MEM_UUID, &session);
    if(tzret!=TZ_RESULT_SUCCESS)
    {
        printk("KREE_CreateSession (TA_MEM) Fail, ret=%d\n", (int)tzret);
        return 0;
    }
    tzret = KREE_GetTEETotalSize(session, &memsz);
    if(tzret!=TZ_RESULT_SUCCESS)
    {
        printk("KREE_GetTEETotalSize Fail, ret=%d\n", (int)tzret);
    }
    tzret = KREE_CloseSession(session);
    if(tzret!=TZ_RESULT_SUCCESS)
    {
        printk("KREE_CloseSession Fail, ret=%d\n", (int)tzret);
    }

    return 0;
}

arch_initcall(tz_client_init);
