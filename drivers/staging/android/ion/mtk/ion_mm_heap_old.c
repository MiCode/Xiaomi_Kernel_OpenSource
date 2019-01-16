
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/xlog.h>
#include <mach/m4u.h>
#include <linux/ion.h>
#include "ion_priv.h"
#include <linux/ion_drv.h>
#include <linux/scatterlist.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/mmprofile.h>

#define ION_FUNC_ENTER  //MMProfileLogMetaString(MMP_ION_DEBUG, MMProfileFlagStart, __func__);
#define ION_FUNC_LEAVE  //MMProfileLogMetaString(MMP_ION_DEBUG, MMProfileFlagEnd, __func__);

extern struct ion_heap *g_ion_heaps[ION_HEAP_IDX_MAX];

typedef struct  
{
    int eModuleID;
    unsigned int security;
    unsigned int coherent;
    void* pVA;
    unsigned int MVA;
} ion_mm_buffer_info;

static DEFINE_MUTEX(ion_mm_buffer_info_mutex);

static int ion_mm_heap_allocate(struct ion_heap *heap,
                                struct ion_buffer *buffer,
                                unsigned long size, unsigned long align,
                                unsigned long flags)
{
    ion_mm_buffer_info* pBufferInfo = NULL;
    int ret;
    unsigned int addr;
    struct sg_table *table;
    struct scatterlist *sg;
    void* pVA;
    ION_FUNC_ENTER;
    pVA = vmalloc_user(size);
    buffer->priv_virt = NULL;
    if (IS_ERR_OR_NULL(pVA))
    {
        printk("[ion_mm_heap_allocate]: Error. Allocate buffer failed.\n");
        ION_FUNC_LEAVE;
        return -ENOMEM;
    }
    pBufferInfo = (ion_mm_buffer_info*) kzalloc(sizeof(ion_mm_buffer_info), GFP_KERNEL);
    if (IS_ERR_OR_NULL(pBufferInfo))
    {
        vfree(pVA);
        printk("[ion_mm_heap_allocate]: Error. Allocate ion_buffer failed.\n");
        ION_FUNC_LEAVE;
        return -ENOMEM;
    }
    table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
    if (!table)
    {
        vfree(pVA);
        kfree(pBufferInfo);
        ION_FUNC_LEAVE;
        return -ENOMEM;
    }
    ret = sg_alloc_table(table, PAGE_ALIGN(size) / PAGE_SIZE, GFP_KERNEL);
    if (ret)
    {
        vfree(pVA);
        kfree(pBufferInfo);
        kfree(table);
        ION_FUNC_LEAVE;
        return -ENOMEM;
    }
    sg = table->sgl;
    for (addr=(unsigned int)pVA; addr < (unsigned int) pVA + size; addr += PAGE_SIZE)
    {
        struct page *page = vmalloc_to_page((void*)addr);
        sg_set_page(sg, page, PAGE_SIZE, 0);
        sg = sg_next(sg);
    }
    buffer->sg_table = table;

    pBufferInfo->pVA = pVA;
    pBufferInfo->eModuleID = -1;
    buffer->priv_virt = pBufferInfo;
    ION_FUNC_LEAVE;
    return 0;
}

static void ion_mm_heap_free(struct ion_buffer *buffer)
{
    ion_mm_buffer_info* pBufferInfo = (ion_mm_buffer_info*) buffer->priv_virt;
    ION_FUNC_ENTER;
    mutex_lock(&ion_mm_buffer_info_mutex);
    if (pBufferInfo)
    {
        if ((pBufferInfo->eModuleID != -1) && (pBufferInfo->MVA))
            m4u_dealloc_mva(pBufferInfo->eModuleID, (unsigned int)pBufferInfo->pVA, buffer->size, pBufferInfo->MVA);
        if (pBufferInfo->pVA)
            vfree(pBufferInfo->pVA);
        kfree(pBufferInfo);
        if (buffer->sg_table)
            sg_free_table(buffer->sg_table);
        kfree(buffer->sg_table);
    }
    mutex_unlock(&ion_mm_buffer_info_mutex);
    ION_FUNC_LEAVE;
}

static void *ion_mm_heap_map_kernel(struct ion_heap *heap,
                                    struct ion_buffer *buffer)
{
    ion_mm_buffer_info* pBufferInfo = (ion_mm_buffer_info*) buffer->priv_virt;
    void* pVA = NULL;
    ION_FUNC_ENTER;
    if (pBufferInfo)
        pVA = pBufferInfo->pVA;
    ION_FUNC_LEAVE;
    return pVA;
}

static void ion_mm_heap_unmap_kernel(struct ion_heap *heap,
                                     struct ion_buffer *buffer)
{
    ION_FUNC_ENTER;
    ION_FUNC_LEAVE;
}

static struct sg_table* ion_mm_heap_map_dma(struct ion_heap *heap, struct ion_buffer *buffer)
{
    ION_FUNC_ENTER;
    ION_FUNC_LEAVE;
    return buffer->sg_table;
}

static void ion_mm_heap_unmap_dma(struct ion_heap *heap, struct ion_buffer *buffer)
{
    ION_FUNC_ENTER;
    ION_FUNC_LEAVE;
}


static int ion_mm_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
                                struct vm_area_struct *vma)
{
    int ret;
    ion_mm_buffer_info* pBufferInfo = (ion_mm_buffer_info*) buffer->priv_virt;
    ION_FUNC_ENTER;
    if ((!pBufferInfo) || (!pBufferInfo->pVA))
    {
        printk("[ion_mm_heap_map_user]: Error. Invalid buffer.\n");
        return -EFAULT;
    }
    ret = remap_vmalloc_range(vma, pBufferInfo->pVA, vma->vm_pgoff);
    ION_FUNC_LEAVE;
    return ret;
}

static int ion_mm_heap_phys(struct ion_heap *heap,
                            struct ion_buffer *buffer,
                            ion_phys_addr_t *addr, size_t *len)
{
    ion_mm_buffer_info* pBufferInfo = (ion_mm_buffer_info*) buffer->priv_virt;
    ION_FUNC_ENTER;
    if (!pBufferInfo)
    {
        printk("[ion_mm_heap_phys]: Error. Invalid buffer.\n");
        ION_FUNC_LEAVE;
        return -EFAULT; // Invalid buffer
    }
    if (pBufferInfo->eModuleID == -1)
    {
        printk("[ion_mm_heap_phys]: Error. Buffer not configured.\n");
        ION_FUNC_LEAVE;
        return -EFAULT; // Buffer not configured.
    }
    // Allocate MVA
    mutex_lock(&ion_mm_buffer_info_mutex);
    if (pBufferInfo->MVA == 0)
    {
        int ret = m4u_alloc_mva(pBufferInfo->eModuleID, (unsigned int)pBufferInfo->pVA, buffer->size, pBufferInfo->security, pBufferInfo->coherent, &pBufferInfo->MVA);
        if (ret < 0)
        {
            mutex_unlock(&ion_mm_buffer_info_mutex);
            pBufferInfo->MVA = 0;
            printk("[ion_mm_heap_phys]: Error. Allocate MVA failed.\n");
            ION_FUNC_LEAVE;
            return -EFAULT;
        }
    }
    mutex_unlock(&ion_mm_buffer_info_mutex);
    *addr = (ion_phys_addr_t) pBufferInfo->MVA;  // MVA address
    *len = buffer->size;
    ION_FUNC_LEAVE;
    return 0;
}

long ion_mm_ioctl(struct ion_client *client, unsigned int cmd, unsigned long arg, int from_kernel)
{
    ion_mm_data_t Param;
    long ret = 0;
    char dbgstr[256];
    unsigned long ret_copy;
    ION_FUNC_ENTER;
    if (from_kernel)
        Param = *(ion_mm_data_t*) arg;
    else
        ret_copy = copy_from_user(&Param, (void __user *)arg, sizeof(ion_mm_data_t));
    switch (Param.mm_cmd)
    {
    case ION_MM_CONFIG_BUFFER:
        {
            struct ion_buffer* buffer;
            if (Param.config_buffer_param.handle)
            {
                buffer = ion_handle_buffer(Param.config_buffer_param.handle);
                if (buffer->heap == g_ion_heaps[ION_HEAP_IDX_MULTIMEDIA])
                {
                    ion_mm_buffer_info* pBufferInfo = buffer->priv_virt;
                    mutex_lock(&ion_mm_buffer_info_mutex);
                    if (pBufferInfo->MVA == 0)
                    {
                    pBufferInfo->eModuleID = Param.config_buffer_param.eModuleID;
                    pBufferInfo->security = Param.config_buffer_param.security;
                    pBufferInfo->coherent = Param.config_buffer_param.coherent;
                    }
                    else
                    {
                        //printk("[ion_mm_heap]: Warning. Cannot config buffer after GET_PHYS is called.\n");
                        ret = -ION_ERROR_CONFIG_LOCKED;
                    }
                    mutex_unlock(&ion_mm_buffer_info_mutex);
                }
                else
                {
                    printk("[ion_mm_heap]: Error. Cannot configure buffer that is not from multimedia heap.\n");
                    ret = -EFAULT;
                }
            }
            else
            {
                printk("[ion_mm_heap]: Error. Configure buffer with invalid handle.\n");
                ret = -EFAULT;
            }
            sprintf(dbgstr, "ION_MM_CONFIG_BUFFER:handle=0x%08X, eModuleID=%d, security=%d, coherent=%d", (unsigned int)Param.config_buffer_param.handle, Param.config_buffer_param.eModuleID, Param.config_buffer_param.security, Param.config_buffer_param.coherent);
        }
        break;
    default:
        printk("[ion_mm_heap]: Error. Invalid command.\n");
        ret = -EFAULT;
    }
    if (from_kernel)
        *(ion_mm_data_t*)arg = Param;
    else
        ret_copy = copy_to_user((void __user *)arg, &Param, sizeof(ion_mm_data_t));
    ION_FUNC_LEAVE;
    return ret;
}

struct ion_heap_ops mm_heap_ops = {
    .allocate = ion_mm_heap_allocate,
    .free = ion_mm_heap_free,
    .map_kernel = ion_mm_heap_map_kernel,
    .unmap_kernel = ion_mm_heap_unmap_kernel,
    .map_dma = ion_mm_heap_map_dma,
    .unmap_dma = ion_mm_heap_unmap_dma,
    .map_user = ion_mm_heap_map_user,
    .phys = ion_mm_heap_phys,
};


struct ion_heap *ion_mm_heap_create(struct ion_platform_heap *unused)
{
        struct ion_heap *heap;
        heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);
        if (!heap)
                return ERR_PTR(-ENOMEM);
        heap->ops = &mm_heap_ops;
        heap->type = ION_HEAP_TYPE_MULTIMEDIA;
        return heap;
}

void ion_mm_heap_destroy(struct ion_heap *heap)
{
        kfree(heap);
}



