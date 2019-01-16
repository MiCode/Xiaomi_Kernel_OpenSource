/*
 * drivers/gpu/tegra/tegra_ion.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
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

#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include "ion_priv.h"
#include <linux/ion_drv.h>
#include <asm/cacheflush.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/mmprofile.h>
#include <linux/vmalloc.h>
#include "ion_profile.h"
#include <linux/debugfs.h>
#include "ion_drv_priv.h"
#include <linux/mtk_ion.h>

#define ION_FUNC_ENTER  //MMProfileLogMetaString(MMP_ION_DEBUG, MMProfileFlagStart, __func__);
#define ION_FUNC_LEAVE  //MMProfileLogMetaString(MMP_ION_DEBUG, MMProfileFlagEnd, __func__);
#ifdef CONFIG_MTK_ION_DEBUG
#define ION_DEBUG_INFO KERN_DEBUG
#define ION_DEBUG_TRACE KERN_DEBUG
#define ION_DEBUG_ERROR KERN_ERR
#define ION_DEBUG_WARN KERN_WARNING
extern unsigned int ion_debugger;
#endif
//#pragma GCC optimize ("O0")
#define DEFAULT_PAGE_SIZE 0x1000
#define PAGE_ORDER 12
extern int record_ion_info(int from_kernel,ion_sys_record_t *param);
extern char *get_userString_from_hashTable(char *string_name,unsigned int len);
extern struct ion_heap *ion_mm_heap_create(struct ion_platform_heap *unused);


struct ion_device *g_ion_device;
//struct ion_heap *g_ion_heaps[ION_HEAP_IDX_MAX];

EXPORT_SYMBOL(g_ion_device);

// Import from multimedia heap
extern struct ion_heap_ops sys_contig_heap_ops;
extern struct ion_heap_ops mm_heap_ops;
long ion_mm_ioctl(struct ion_client *client, unsigned int cmd, unsigned long arg, int from_kernel);

void smp_inner_dcache_flush_all(void);

#ifndef dmac_map_area
#define dmac_map_area __dma_map_area
#endif
#ifndef dmac_unmap_area
#define dmac_unmap_area __dma_unmap_area
#endif
#ifndef dmac_flush_range
#define dmac_flush_range __dma_flush_range
#endif

#define __ION_CACHE_SYNC_USER_VA_EN__

static int __ion_cache_sync_kernel(unsigned long start, size_t size, ION_CACHE_SYNC_TYPE sync_type)
{
    unsigned long end = start + size;
    start = (start / L1_CACHE_BYTES * L1_CACHE_BYTES);
    size = (end - start + L1_CACHE_BYTES - 1) / L1_CACHE_BYTES * L1_CACHE_BYTES;
    // L1 cache sync
    if((sync_type==ION_CACHE_CLEAN_BY_RANGE) || (sync_type==ION_CACHE_CLEAN_BY_RANGE_USE_VA))
    {
        MMProfileLogEx(ION_MMP_Events[PROFILE_DMA_CLEAN_RANGE], MMProfileFlagStart, size, 0);
        //printk("[ion_sys_cache_sync]: ION cache clean by range. start=0x%08X size=0x%08X\n", start, size);
        dmac_map_area((void*)start, size, DMA_TO_DEVICE);
    }
    else if ((sync_type == ION_CACHE_INVALID_BY_RANGE)||(sync_type == ION_CACHE_INVALID_BY_RANGE_USE_VA))
    {
        MMProfileLogEx(ION_MMP_Events[PROFILE_DMA_INVALID_RANGE], MMProfileFlagStart, size, 0);
        //printk("[ion_sys_cache_sync]: ION cache invalid by range. start=0x%08X size=0x%08X\n", start, size);
        dmac_unmap_area((void*)start, size, DMA_FROM_DEVICE);
    }
    else if ((sync_type == ION_CACHE_FLUSH_BY_RANGE)||(sync_type == ION_CACHE_FLUSH_BY_RANGE_USE_VA))
    {
        MMProfileLogEx(ION_MMP_Events[PROFILE_DMA_FLUSH_RANGE], MMProfileFlagStart, size, 0);
        //printk("[ion_sys_cache_sync]: ION cache flush by range. start=0x%08X size=0x%08X\n", start, size);
        dmac_flush_range((void*)start, (void*)(start+size-1));
    }

    return 0;
}

static struct vm_struct *cache_map_vm_struct = NULL;
static int ion_cache_sync_init(void)
{
    cache_map_vm_struct = get_vm_area(PAGE_SIZE, VM_ALLOC);
    if (!cache_map_vm_struct)
        return -ENOMEM;

    return 0;
}

static void* ion_cache_map_page_va(struct page* page)
{
    int ret;
    struct page** ppPage = &page;

    ret = map_vm_area(cache_map_vm_struct, PAGE_KERNEL, &ppPage);
    if(ret)
    {
        IONMSG("error to map page\n");
        return NULL;
    }
    return cache_map_vm_struct->addr;
}

static void ion_cache_unmap_page_va(unsigned long va)
{
    unmap_kernel_range((unsigned long)cache_map_vm_struct->addr,  PAGE_SIZE);
}

//lock to protect cache_map_vm_struct
static DEFINE_MUTEX(gIon_cache_sync_user_lock);

static long ion_sys_cache_sync(struct ion_client *client, ion_sys_cache_sync_param_t* pParam, int from_kernel)
{
    ION_FUNC_ENTER;
    if (pParam->sync_type < ION_CACHE_CLEAN_ALL)
    {
        // By range operation
        unsigned long start = -1;
        size_t size = 0;
        unsigned long end, page_num, page_start;
        struct ion_handle *kernel_handle;   

        kernel_handle = ion_drv_get_handle(client, pParam->handle, pParam->kernel_handle, from_kernel);
        if(IS_ERR(kernel_handle))
        {
            pr_err("ion cache sync fail! \n");
            return -EINVAL;
        }

#ifdef __ION_CACHE_SYNC_USER_VA_EN__
        if(pParam->sync_type < ION_CACHE_CLEAN_BY_RANGE_USE_VA)
#endif
        {
        	struct ion_buffer *buffer;
        	struct scatterlist *sg;
        	int i, j;
        	struct sg_table *table = NULL;
        	int npages = 0;

        	mutex_lock(&client->lock);
        	/*if (!ion_handle_validate(client, kernel_handle)) {
        		pr_err("%s: invalid handle.\n", __func__);
        		mutex_unlock(&client->lock);
        		return -EINVAL;
        	}
*/
        	buffer = kernel_handle->buffer;

        	table = buffer->sg_table;
        	npages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;

            mutex_lock(&gIon_cache_sync_user_lock);

			if (!cache_map_vm_struct) {
				IONMSG(" error: cache_map_vm_struct is NULL, retry\n");
				ion_cache_sync_init();
			}

			if (!cache_map_vm_struct) {
				IONMSG("error: cache_map_vm_struct is NULL, no vmalloc area\n");
			    mutex_unlock(&gIon_cache_sync_user_lock);
        		mutex_unlock(&client->lock);
				return -ENOMEM;
			}

			for_each_sg(table->sgl, sg, table->nents, i)
			{
				int npages_this_entry = PAGE_ALIGN(sg->length) / PAGE_SIZE;
				struct page *page = sg_page(sg);
				BUG_ON(i >= npages);
				for (j = 0; j < npages_this_entry; j++) {
					start = (unsigned long) ion_cache_map_page_va(page++);

					if (IS_ERR_OR_NULL((void*) start)) {
						IONMSG("cannot do cache sync: ret=%lu\n", start);
						mutex_unlock(&gIon_cache_sync_user_lock);
						mutex_unlock(&client->lock);
						return -EFAULT;
					}

					__ion_cache_sync_kernel(start, PAGE_SIZE, pParam->sync_type);

					ion_cache_unmap_page_va(start);
				}
			}

		    mutex_unlock(&gIon_cache_sync_user_lock);
        	mutex_unlock(&client->lock);
        }
        else
        {
            start = (unsigned long)pParam->va;
            size = pParam->size;

            __ion_cache_sync_kernel(start, size, pParam->sync_type);

#ifdef __ION_CACHE_SYNC_USER_VA_EN__
			if (pParam->sync_type < ION_CACHE_CLEAN_BY_RANGE_USE_VA)
#endif
			{
				ion_unmap_kernel(client, kernel_handle);
			}
        }

#if 0
        // Cache line align
        end = start + size;
        start = (start / L1_CACHE_BYTES * L1_CACHE_BYTES);
        size = (end - start + L1_CACHE_BYTES - 1) / L1_CACHE_BYTES * L1_CACHE_BYTES;
        page_num = ((start&(~PAGE_MASK))+size+(~PAGE_MASK))>>PAGE_ORDER;
        page_start = start & PAGE_MASK;


        // L2 cache sync
        //printk("[ion_sys_cache_sync]: page_start=0x%08X, page_num=%d\n", page_start, page_num);
        for (i=0; i<page_num; i++, page_start+=DEFAULT_PAGE_SIZE)
        {
            phys_addr_t phys_addr;

            if (page_start>=VMALLOC_START && page_start<=VMALLOC_END)
            {
                ppage = vmalloc_to_page((void*)page_start);
                if (!ppage)
                {
                	printk("[ion_sys_cache_sync]: Cannot get vmalloc page. addr=0x%08X\n", page_start);
                    ion_unmap_kernel(client, pParam->handle);
                    return -EFAULT;
                }
                phys_addr = page_to_phys(ppage);
            }
            else
                phys_addr = virt_to_phys((void*)page_start);
            if (pParam->sync_type == ION_CACHE_CLEAN_BY_RANGE)
                outer_clean_range(phys_addr, phys_addr+DEFAULT_PAGE_SIZE);
            else if (pParam->sync_type == ION_CACHE_INVALID_BY_RANGE)
                outer_inv_range(phys_addr, phys_addr+DEFAULT_PAGE_SIZE);
            else if (pParam->sync_type == ION_CACHE_FLUSH_BY_RANGE)
                outer_flush_range(phys_addr, phys_addr+DEFAULT_PAGE_SIZE);
        }
#endif

        ion_drv_put_kernel_handle(kernel_handle);
        
        if (pParam->sync_type == ION_CACHE_CLEAN_BY_RANGE)
            MMProfileLogEx(ION_MMP_Events[PROFILE_DMA_CLEAN_RANGE], MMProfileFlagEnd, size, 0);
        else if (pParam->sync_type == ION_CACHE_INVALID_BY_RANGE)
            MMProfileLogEx(ION_MMP_Events[PROFILE_DMA_INVALID_RANGE], MMProfileFlagEnd, size, 0);
        else if (pParam->sync_type == ION_CACHE_FLUSH_BY_RANGE)
            MMProfileLogEx(ION_MMP_Events[PROFILE_DMA_FLUSH_RANGE], MMProfileFlagEnd, size, 0);
        
    }
    else
    {
        // All cache operation
        if (pParam->sync_type == ION_CACHE_CLEAN_ALL)
        {
            MMProfileLogEx(ION_MMP_Events[PROFILE_DMA_CLEAN_ALL], MMProfileFlagStart, 1, 1);
            //printk("[ion_sys_cache_sync]: ION cache clean all.\n");
            smp_inner_dcache_flush_all();
            //outer_clean_all();
            MMProfileLogEx(ION_MMP_Events[PROFILE_DMA_CLEAN_ALL], MMProfileFlagEnd, 1, 1);
        }
        else if (pParam->sync_type == ION_CACHE_INVALID_ALL)
        {
            MMProfileLogEx(ION_MMP_Events[PROFILE_DMA_INVALID_ALL], MMProfileFlagStart, 1, 1);
            //printk("[ion_sys_cache_sync]: ION cache invalid all.\n");
            smp_inner_dcache_flush_all();
            //outer_inv_all();
            //outer_flush_all();
            MMProfileLogEx(ION_MMP_Events[PROFILE_DMA_INVALID_ALL], MMProfileFlagEnd, 1, 1);
        }
        else if (pParam->sync_type == ION_CACHE_FLUSH_ALL)
        {
            MMProfileLogEx(ION_MMP_Events[PROFILE_DMA_FLUSH_ALL], MMProfileFlagStart, 1, 1);
            //printk("[ion_sys_cache_sync]: ION cache flush all.\n");
            smp_inner_dcache_flush_all();
            //outer_flush_all();
            MMProfileLogEx(ION_MMP_Events[PROFILE_DMA_FLUSH_ALL], MMProfileFlagEnd, 1, 1);
        }
    }
    ION_FUNC_LEAVE;
    return 0;
}


int ion_sys_copy_client_name(const char *src, char *dst)
{
    int i;
    for(i=0; i<ION_MM_DBG_NAME_LEN-1; i++)
    {
        dst[i] = src[i];
    }
    dst[ION_MM_DBG_NAME_LEN-1] = '\0';

    return 0;
}

static long ion_sys_ioctl(struct ion_client *client, unsigned int cmd, unsigned long arg, int from_kernel)
{
    ion_sys_data_t Param;
    long ret = 0;
    unsigned long ret_copy = 0;
    ION_FUNC_ENTER;
    if (from_kernel)
        Param = *(ion_sys_data_t*) arg;
    else
        ret_copy = copy_from_user(&Param, (void __user *)arg, sizeof(ion_sys_data_t));

    switch (Param.sys_cmd)
    {
    case ION_SYS_CACHE_SYNC:
        ret = ion_sys_cache_sync(client, &Param.cache_sync_param, from_kernel);
        break;
    case ION_SYS_GET_PHYS:
        {
            struct ion_handle *kernel_handle;  

            kernel_handle = ion_drv_get_handle(client, Param.get_phys_param.handle, Param.get_phys_param.kernel_handle, from_kernel);
            if(IS_ERR(kernel_handle))
            {
            	pr_err("ion_get_phys fail!\n");
                ret = -EINVAL;
                break;
            }

            if (ion_phys(client, kernel_handle, (ion_phys_addr_t*)&(Param.get_phys_param.phy_addr), &(Param.get_phys_param.len)) < 0)
            {
                Param.get_phys_param.phy_addr = 0;
                Param.get_phys_param.len = 0;
                pr_err("[ion_sys_ioctl]: Error. Cannot get physical address.\n");
                ret = -EFAULT;
            }
            ion_drv_put_kernel_handle(kernel_handle);
        }
        break;
    case ION_SYS_GET_CLIENT:
        Param.get_client_param.client = (unsigned long) client;
        break;
    case ION_SYS_SET_CLIENT_NAME:
        ion_sys_copy_client_name(Param.client_name_param.name, client->dbg_name);
        break;
	case ION_SYS_RECORD:
	{
#ifdef CONFIG_MTK_ION_DEBUG
		unsigned int i;
		char *tmp_string = NULL;

		if(Param.record_param.action == ION_FUNCTION_CHECK_ENABLE)
		{
			ret = (long)ion_debugger;
			break;
		}
		if(ion_debugger)
		{
 
			///copy mapping info from userspace to kernel space 
			if(!from_kernel && (Param.record_param.backtrace_num > 0))
			{
				for(i = 0 ; i < Param.record_param.backtrace_num;i++)
				{
					if(Param.record_param.mapping_record[i].size > 0 )
					{
						unsigned int string_len = 0;
						string_len = strlen_user((void __user*)Param.record_param.mapping_record[i].name);
						if(string_len > 0)
						{
							int ret;
							tmp_string = (char *)kmalloc(string_len,GFP_KERNEL);
							ret = copy_from_user(tmp_string, (void __user *)Param.record_param.mapping_record[i].name, string_len);
							if(tmp_string!=NULL)
							{
								Param.record_param.mapping_record[i].name= get_userString_from_hashTable(tmp_string,string_len);
								kfree(tmp_string);
							}
							else
							{
								printk(ION_DEBUG_ERROR "[ION_FUNC%d][ion_sys_ioctl]tmp_string is NULL\n",Param.record_param.action);
							}
						}
						else
						{
							printk(ION_DEBUG_ERROR "[ION_FUNC%d][ion_sys_ioctl]mapping info error can't get right string len \n",Param.record_param.action);
						}
					}
				}
			}
			Param.record_param.client = client;
			if(Param.record_param.handle != NULL)
			{
				struct ion_handle *kernel_handle;  
				kernel_handle = ion_drv_get_handle(client,Param.record_param.handle, NULL, from_kernel); 
				if(IS_ERR(kernel_handle))
				{
					printk("ion_sys_record fail!\n");
					ret = -EINVAL;
					break;
				}
				Param.record_param.buffer = ion_handle_buffer(kernel_handle);
				if(!from_kernel)
				{
					Param.record_param.handle = kernel_handle;
				}
                ion_drv_put_kernel_handle(kernel_handle);

#if 0
				if(Param.record_param.buffer != NULL)
				{
					printk(ION_DEBUG_TRACE "[ION_FUNC%d][ion_sys_record]BUFFER :[%x] size :[%d] handle: [0x%x]\n",Param.record_param.action,(unsigned int)Param.record_param.buffer,Param.record_param.buffer->size,(unsigned int)Param.record_param.handle);
				}
				else
				{
					printk(ION_DEBUG_TRACE "[ION_FUNC%d]buffer is NULL handle: [0x%x]\n",Param.record_param.action,(unsigned int)Param.record_param.handle);
				} 
#endif
			}
			record_ion_info(from_kernel,&Param.record_param);
			//printk(ION_DEBUG_TRACE "[ION_FUNC%d][ion_sys_ioctl]DONE\n",Param.record_param.action);
		}	
#endif
		break;
	}
    case ION_SYS_SET_HANDLE_BACKTRACE:
    {
#if  ION_RUNTIME_DEBUGGER
	     unsigned int i;
        struct ion_handle *kernel_handle;  
        kernel_handle = ion_drv_get_handle(client, 
                        Param.record_param.handle, NULL, from_kernel);
        if(IS_ERR(kernel_handle))
        {
            IONMSG("ion_set_handle_bt fail!\n");
            ret = -EINVAL;
            break;
        }

        kernel_handle->dbg.pid = (unsigned int) current->pid;
        kernel_handle->dbg.tgid = (unsigned int)current->tgid;
        kernel_handle->dbg.backtrace_num = Param.record_param.backtrace_num;

        for(i=0; i<Param.record_param.backtrace_num; i++)
        {
            kernel_handle->dbg.backtrace[i] = Param.record_param.backtrace[i];
        }
        ion_drv_put_kernel_handle(kernel_handle);

#endif

	}
    break;
    
    default:
        printk("[ion_dbg][ion_sys_ioctl]: Error. Invalid command.\n");
        ret = -EFAULT;
        break;
    }
    if (from_kernel)
        *(ion_sys_data_t*)arg = Param;
    else
        ret_copy = copy_to_user((void __user *)arg, &Param, sizeof(ion_sys_data_t));
    ION_FUNC_LEAVE;
    return ret;
}

static long _ion_ioctl(struct ion_client *client, unsigned int cmd, unsigned long arg, int from_kernel)
{
    long ret = 0;
    ION_FUNC_ENTER;
    switch (cmd)
    {
    case ION_CMD_SYSTEM:
        ret = ion_sys_ioctl(client, cmd, arg, from_kernel);
        break;
    case ION_CMD_MULTIMEDIA:
        ret = ion_mm_ioctl(client, cmd, arg, from_kernel);
        break;
    }
    ION_FUNC_LEAVE;
    return ret;
}

long ion_kernel_ioctl(struct ion_client *client, unsigned int cmd, unsigned long arg)
{
    return _ion_ioctl(client, cmd, arg, 1);
}
EXPORT_SYMBOL(ion_kernel_ioctl);

static long ion_custom_ioctl(struct ion_client *client, unsigned int cmd, unsigned long arg)
{
    return _ion_ioctl(client, cmd, arg, 0);
}

static int debug_profile_get(void *data, u64 *val)
{
    *val = -1;
    return 0;
}

static int debug_profile_set(void *data, u64 val)
{
    ion_profile_init();
    return 0;
}


DEFINE_SIMPLE_ATTRIBUTE(debug_profile_fops, debug_profile_get,
                        debug_profile_set, "%llu\n");



static int num_heaps;
static struct ion_heap **heaps;

int ion_drv_probe(struct platform_device *pdev)
{
	int ret,i;
	struct ion_platform_data *pdata = pdev->dev.platform_data;
    
	IONMSG("ion_drv_probe() heap_nr=%d\n", pdata->nr);
	num_heaps = pdata->nr;

	heaps = kzalloc(sizeof(struct ion_heap *) * pdata->nr, GFP_KERNEL);

	g_ion_device = ion_device_create(ion_custom_ioctl);
	if (IS_ERR_OR_NULL(g_ion_device))
	{
		printk("ion_device_create() error! device=%p\n", g_ion_device);
		return PTR_ERR(g_ion_device);
	}

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];

		if(heap_data->type == ION_HEAP_TYPE_CARVEOUT 
			&& heap_data->base == 0) {
			//reserve for carveout heap failed
			heap_data->size = 0;
			heaps[i] = NULL;
			continue;
		}

		heaps[i] = ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heaps[i])) {
			ret = PTR_ERR(heaps[i]);
                        IONMSG("%s: %d heap is err or null.\n", __func__, i);
			goto err;
		}
		heaps[i]->name = heap_data->name;
		heaps[i]->id = heap_data->id;        
		ion_device_add_heap(g_ion_device, heaps[i]);
	}
    
	platform_set_drvdata(pdev, g_ion_device);

	debugfs_create_file("ion_profile", 0644, g_ion_device->debug_root, NULL,
			&debug_profile_fops);
	debugfs_create_symlink("ion_mm_heap", g_ion_device->debug_root, "./heaps/ion_mm_heap");

	ion_history_init();

	return 0;

err:

	for (i = 0; i < num_heaps; i++) {
		if (heaps[i])
			ion_heap_destroy(heaps[i]);
	}
	kfree(heaps);
	return ret;
    
}

int ion_drv_remove(struct platform_device *pdev)
{
    unsigned int i;
	struct ion_device *idev = platform_get_drvdata(pdev);
	for (i = 0; i < num_heaps; i++)
		ion_heap_destroy(heaps[i]);
    ion_device_destroy(idev);
    kfree(heaps);
	return 0;
}

struct ion_heap * ion_drv_get_heap(int heap_id)
{
	int i;
	for(i = 0; i < num_heaps; i++) {
		if(heaps[i]->id == heap_id)
			return heaps[i];
	}
        IONMSG("%s: The heap_id %d has no corresponding heap.\n", __func__, heap_id);
	return NULL;
}

static struct ion_platform_heap ion_drv_platform_heaps[] = 
{
    {
        .type = ION_HEAP_TYPE_SYSTEM_CONTIG,
        .id = ION_HEAP_TYPE_SYSTEM_CONTIG,
        .name = "ion_system_contig_heap",
        .base = 0,
        .size = 0,
        .align = 0,
        .priv = NULL,
    },
    {
        .type = ION_HEAP_TYPE_MULTIMEDIA,
        .id = ION_HEAP_TYPE_MULTIMEDIA,
        .name = "ion_mm_heap",
        .base = 0,
        .size = 0,
        .align = 0,
        .priv = NULL,
    },
    {
        .type = ION_HEAP_TYPE_CARVEOUT,
        .id = ION_HEAP_TYPE_CARVEOUT,
        .name = "ion_carveout_heap",
        .base = 0,
        .size = 0, //32*1024*1024, //reserve in /kernel/arch/arm/mm/init.c ion_reserve();
        .align = 0x1000,    //this must not be 0. (or ion_reserve wiil fail)
        .priv = NULL,
    },
    
};


struct ion_platform_data ion_drv_platform_data = 
{
    .nr = 3,
    .heaps = ion_drv_platform_heaps,
};


static struct platform_driver ion_driver = {
	.probe = ion_drv_probe,
	.remove = ion_drv_remove,
	.driver = { .name = "ion-drv" }
};

static struct platform_device ion_device = {
	.name = "ion-drv",
	.id   = 0,
	.dev = {
	    .platform_data = &ion_drv_platform_data,
	},
	
};

static int __init ion_init(void)
{
    printk("ion_init()\n");
	if (platform_device_register(&ion_device))
	{
                IONMSG("%s platform device register failed.\n", __func__);
		return -ENODEV;
	}
	if (platform_driver_register(&ion_driver))
	{
		platform_device_unregister(&ion_device);
                IONMSG("%s platform driver register failed.\n", __func__);
		return -ENODEV;
	}
	return 0;
}

static void __exit ion_exit(void)
{
	platform_driver_unregister(&ion_driver);
	platform_device_unregister(&ion_device);
}

module_init(ion_init);
module_exit(ion_exit);

