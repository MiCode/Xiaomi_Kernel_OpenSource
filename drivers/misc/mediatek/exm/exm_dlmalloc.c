/* EXM */
#include <linux/module.h> 
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/exm_driver.h> 
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>

#include "exm_dlmalloc.h"

// Ugly inclusion of C file so that bionic specific #defines configure dlmalloc.
#include "malloc.c"

#define DEV_DRV_NAME "mt-extmem"

//#define EXTMEM_DEBUG
#ifdef EXTMEM_DEBUG
#define extmem_printk(fmt, args...) printk(fmt, ## args)
#else
#define extmem_printk(...) 
#endif

typedef void* mspace;
static mspace extmem_mspace = NULL;
static void * extmem_mspace_base = NULL;
static size_t extmem_mspace_size = 0;

#ifdef CONFIG_OF
#include <linux/of.h>
#include <asm/setup.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>

static phys_addr_t extmem_phys_base = 0;

static int extmem_scan_memory(unsigned long node, const char *uname, int depth, void *data)
{
	mem_desc_t *mem_desc;
	char *type = NULL;
	
	/* We are scanning "memory" nodes only */
	type = of_get_flat_dt_prop(node, "device_type", NULL);		
	if (type == NULL) {
		/*
		 * The longtrail doesn't have a device_type on the
		 * /memory node, so look for the node called /memory@0.
		 */
		if (depth != 1 || strcmp(uname, "memory@0") != 0)
			return 0;
	} else if (strcmp(type, "memory") != 0) {
		return 0;
	}
	
	/* lca reserved memory */
	mem_desc = (mem_desc_t *)of_get_flat_dt_prop(node,
			"lca_reserved_mem", NULL);
	if (mem_desc && mem_desc->size) {
		extmem_phys_base = mem_desc->start;
		extmem_mspace_size = mem_desc->size;
		extmem_printk("[EXT_MEM] extmem_scan_memory extmem_phys_base: 0x%p, extmem_mspace_size: 0x%zx\n", 
						(void *)extmem_phys_base, extmem_mspace_size);
	}
	
	return node;
}

static int __init extmem_get_lca_reserved_mem(void)
{
	int node;
	extmem_printk("[EXT_MEM] extmem_early_memory_info\n");
	
	/* avoid double init */
	if (extmem_phys_base) {
		printk(KERN_WARNING "[EXT_MEM] extmem_phys_base exist: 0x%p\n", (void *)extmem_phys_base);
		return 0;
	}
	
	node = of_scan_flat_dt(extmem_scan_memory, NULL);
	return 0;
}

early_initcall(extmem_get_lca_reserved_mem);

static void extmem_init(void) {
    if (extmem_mspace == NULL) {
		size_t extmem_vmalloc_limit = (VMALLOC_TOTAL/3)& ~(0x02000000-1);
		
        if (extmem_mspace_size > 0x02000000) {
            extmem_mspace_size -= 0x02000000;
		}
		else {
			extmem_get_lca_reserved_mem();
			if (extmem_mspace_size > 0x02000000) {
            	extmem_mspace_size -= 0x02000000;
			}
		}
	
		if (extmem_mspace_size > extmem_vmalloc_limit) {
			printk(KERN_WARNING "[EXT_MEM] extmem_mspace_size: 0x%zx over limit: 0x%zx\n", 
				     extmem_mspace_size, extmem_vmalloc_limit);
			extmem_mspace_size = extmem_vmalloc_limit;
		}
		
		if (extmem_mspace_size <= 0) {
			printk(KERN_ERR "[EXT_MEM] no extmem, need check config\n");
            BUG();
		}
		
	#ifdef CONFIG_ARM64
        extmem_mspace_base = (void*) ioremap_wc(extmem_phys_base, extmem_mspace_size);
    #else
        extmem_mspace_base = (void*) ioremap_cached(extmem_phys_base, extmem_mspace_size);
    #endif
        extmem_mspace = create_mspace_with_base(extmem_mspace_base, extmem_mspace_size, 1);
		extmem_printk("[EXT_MEM] extmem_phys_base:0x%p,extmem_mspace_size: 0x%zx,extmem_mspace:0x%p\n", 
                (void *)extmem_phys_base, extmem_mspace_size, extmem_mspace);
		extmem_printk("[EXT_MEM] %s extmem current used: 0x%zx, peak used: 0x%zx\n",
			__FUNCTION__, mspace_mem_used(extmem_mspace), mspace_mem_used_peak(extmem_mspace));
    }
}
#else
extern phys_addr_t get_max_phys_addr(void);
extern phys_addr_t get_max_DRAM_size(void);

static void extmem_init(void) {
    if (extmem_mspace == NULL) {
        if (extmem_mspace_size == 0) {
        	size_t extmem_vmalloc_limit = (VMALLOC_TOTAL/3)& ~(0x02000000-1);
        	
        	if (get_max_DRAM_size() < (CONFIG_MAX_DRAM_SIZE_SUPPORT + 0x02000000)) {
        		printk(KERN_ERR "[EXT_MEM] no extmem,get_max_DRAM_size:0x%p,CONFIG_MAX_DRAM_SIZE_SUPPORT:0x%x,get_max_phys_addr:0x%p\n", 
                                 (void *)get_max_DRAM_size(), CONFIG_MAX_DRAM_SIZE_SUPPORT, (void *)get_max_phys_addr());
            	BUG();
            }
            
            extmem_mspace_size = get_max_DRAM_size() - CONFIG_MAX_DRAM_SIZE_SUPPORT - 0x02000000;
            if (extmem_mspace_size > extmem_vmalloc_limit) {
				printk(KERN_WARNING "[EXT_MEM] extmem_mspace_size: 0x%zx over limit: 0x%zx\n", 
				     	extmem_mspace_size, extmem_vmalloc_limit);
				extmem_mspace_size = extmem_vmalloc_limit;
			}
		}
		
        //extmem_mspace_base = (void*) ioremap(get_max_phys_addr(), extmem_mspace_size);
        extmem_mspace_base = (void*) ioremap_cached(get_max_phys_addr(), extmem_mspace_size);
        extmem_mspace = create_mspace_with_base(extmem_mspace_base, extmem_mspace_size, 1);
		extmem_printk("[EXT_MEM] get_max_DRAM_size:0x%x,CONFIG_MAX_DRAM_SIZE_SUPPORT:0x%x,get_max_phys_addr:0x%p, extmem_mspace:0x%p\n", 
                                 get_max_DRAM_size(), CONFIG_MAX_DRAM_SIZE_SUPPORT, (void *)get_max_phys_addr(), extmem_mspace);
    }
}
#endif

void* extmem_malloc(size_t bytes) {
	void *mem = NULL;
    extmem_init();
	mem = mspace_malloc(extmem_mspace, bytes);
	extmem_printk("[EXT_MEM] %s mem:0x%p, size: 0x%zx\n", __FUNCTION__, mem, bytes);
	extmem_printk("[EXT_MEM] %s extmem current used: 0x%zx, peak used: 0x%zx\n",
		__FUNCTION__, mspace_mem_used(extmem_mspace), mspace_mem_used_peak(extmem_mspace));
    return mem;
}
EXPORT_SYMBOL(extmem_malloc);

void* extmem_malloc_page_align(size_t bytes) {
	void *mem = NULL;
    extmem_init();
	mem = mspace_memalign(extmem_mspace, 1<<PAGE_SHIFT, bytes);
    extmem_printk("[EXT_MEM] %s mem:0x%p, size: 0x%zx\n", __FUNCTION__, mem, bytes);
	extmem_printk("[EXT_MEM] %s extmem current used: 0x%zx, peak used: 0x%zx\n",
		__FUNCTION__, mspace_mem_used(extmem_mspace), mspace_mem_used_peak(extmem_mspace));
    return mem;
}
EXPORT_SYMBOL(extmem_malloc_page_align);

void extmem_free(void* mem) {
    extmem_printk("[EXT_MEM] %s addr:0x%p\n", __FUNCTION__, mem);
    if (extmem_mspace != NULL) {
        mspace_free(extmem_mspace, mem);
		extmem_printk("[EXT_MEM] %s extmem current used: 0x%zx, peak used: 0x%zx\n",
			__FUNCTION__, mspace_mem_used(extmem_mspace), mspace_mem_used_peak(extmem_mspace));
    }
}
EXPORT_SYMBOL(extmem_free);

static unsigned long get_phys_from_mspace(unsigned long va)
{
#ifdef CONFIG_OF
	extmem_printk("[EXT_MEM] %s va: 0x%lx extmem_phys_base:0x%p extmem_mspace_base:0x%p\n",
	               __FUNCTION__, va, (void *)extmem_phys_base, extmem_mspace_base);
	return ( va - (unsigned long)extmem_mspace_base + extmem_phys_base);
#else
    return ( va - (unsigned long)extmem_mspace_base + get_max_phys_addr());
#endif
}

unsigned long get_virt_from_mspace(unsigned long pa)
{
#ifdef CONFIG_OF	
	extmem_printk("[EXT_MEM] %s pa:0x%lx extmem_phys_base:0x%p extmem_mspace_base:0x%p\n",
	               __FUNCTION__, pa, (void *)extmem_phys_base, extmem_mspace_base);
	return ( pa - extmem_phys_base + (unsigned long)extmem_mspace_base);
#else
    return ( pa - get_max_phys_addr() + (unsigned long)extmem_mspace_base);
#endif
}
EXPORT_SYMBOL(get_virt_from_mspace);

static void extmem_vma_close(struct vm_area_struct *vma)
{
	//if (extmem_in_mspace(vma))
	extmem_free((void *)get_virt_from_mspace((vma->vm_pgoff << PAGE_SHIFT)));
}

static const struct vm_operations_struct exm_vm_ops = {
	.close = extmem_vma_close,
};

bool extmem_in_mspace(struct vm_area_struct *vma)
{
    return (vma->vm_ops == &exm_vm_ops);
}
EXPORT_SYMBOL(extmem_in_mspace);

size_t extmem_get_mem_size(unsigned long pgoff)
{
    void * va = (void *)get_virt_from_mspace(pgoff << PAGE_SHIFT);
    mchunkptr p  = mem2chunk(va);
    size_t psize = chunksize(p) - TWO_SIZE_T_SIZES;

    extmem_printk("[EXT_MEM] %s size: %zu\n", __FUNCTION__, psize);
    return psize;
}
EXPORT_SYMBOL(extmem_get_mem_size);


static int mtk_mspace_mmap_physical(struct exm_info *info, struct vm_area_struct *vma)
{
    unsigned long size = vma->vm_end - vma->vm_start;
    void * va = NULL;
    unsigned long pa;
    int ret = -EINVAL;

    if ((vma->vm_flags & VM_SHARED) == 0) {
        return -EINVAL;
    }

    vma->vm_ops = &exm_vm_ops;
    va = extmem_malloc_page_align(size);

    if (!va) {
        printk(KERN_ERR "[EXT_MEM] %s failed...\n", __FUNCTION__);
        return -ENOMEM;
    }

    memset(va, 0, size);	
    vma->vm_flags |= (VM_DONTCOPY | VM_DONTEXPAND);

#if !defined(CONFIG_ARM64)
	vma->vm_page_prot = __pgprot_modify(vma->vm_page_prot, L_PTE_MT_MASK, L_PTE_MT_WRITEBACK);
#endif
    pa = get_phys_from_mspace((unsigned long) va);
    ret = remap_pfn_range(vma,
    		       vma->vm_start,
    		       (pa >> PAGE_SHIFT),
    		       size,
				#if !defined(CONFIG_ARM64)
    		       vma->vm_page_prot);
				#else
				   PAGE_SHARED);
				#endif

	extmem_printk("[EXT_MEM] pa:0x%lx, va:0x%p, vma->vm_pgoff:0x%lx, vm_start:0x%lx, vm_end:0x%lx, vm_page_prot:0x%zx\n", 
	               pa, va, vma->vm_pgoff, vma->vm_start, vma->vm_end, (size_t)pgprot_val(vma->vm_page_prot));
	if (ret) {
		printk(KERN_ERR "[EXT_MEM] %s fail ret:%d\n", __FUNCTION__, ret);
	}

    return ret;
}

static int mt_mspace_probe(struct platform_device *dev) 
{ 
    //struct resource *regs;
    struct exm_info *info;

    extmem_printk("[EXT_MEM] probing mt_mspace\n");
    info = kzalloc(sizeof(struct exm_info), GFP_KERNEL);
    if (!info) 
        return -ENOMEM;

    extmem_init();
#ifdef CONFIG_OF
	info->mem[0].addr = extmem_phys_base;
#else
    info->mem[0].addr = get_max_phys_addr();
#endif
    info->mem[0].size = extmem_mspace_size;
    info->mmap = mtk_mspace_mmap_physical;

    if (!info->mem[0].addr) {
        dev_err(&dev->dev, "Invalid memory resource\n");
        return -ENODEV;
    }

    info->version = "0.0.2";
    info->name= DEV_DRV_NAME;

    if (exm_register_device(&dev->dev, info)) {
        iounmap(info->mem[0].internal_addr);
        printk(KERN_ERR "[EXT_MEM] exm_register failed\n");
        return -ENODEV;
    }
    platform_set_drvdata(dev, info);
    printk("[EXT_MEM] probing mt_mspace success\n");

    return 0;
} 

static int mt_mspace_remove(struct platform_device *dev) 
{ 
    struct exm_info *info = platform_get_drvdata(dev);

    exm_unregister_device(info);
    platform_set_drvdata(dev, NULL);
    iounmap(info->mem[0].internal_addr);
    kfree(info);
    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id extmem_of_ids[] = {
	{ .compatible = "mediatek,mt-extmem", },
	{}
};
#endif

static struct platform_driver mt_mspace_driver = {
    .probe = mt_mspace_probe,
    .remove = mt_mspace_remove,
    .driver = {
        .name = DEV_DRV_NAME,
	#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(extmem_of_ids),
	#endif
    },
};

static int __init mt_mspace_init(void)
{
	extmem_printk("[EXT_MEM] %s\n", __FUNCTION__);
	return platform_driver_register(&mt_mspace_driver);
}

static void __exit mt_mspace_exit(void)
{
	platform_driver_unregister(&mt_mspace_driver);
}

static size_t extmem_used = 0; /* extmem current used */
static size_t extmem_used_peak = 0; /* extmem peak used */

static int param_get_extmem_used(char *buffer, const struct kernel_param *kp)
{
	int result = 0;
	extmem_used = mspace_mem_used(extmem_mspace);
	extmem_used_peak = mspace_mem_used_peak(extmem_mspace);
	result = sprintf(buffer, "current_used: %10zu bytes\npeak_used: %10zu bytes\n", extmem_used, extmem_used_peak);
	return result;
}

static const struct kernel_param_ops param_ops_extmem_used = {
	.set = param_set_uint,
	.get = param_get_extmem_used,
};

module_param_cb(mem_used, &param_ops_extmem_used, &extmem_used, 0644);

module_init(mt_mspace_init);
module_exit(mt_mspace_exit);

MODULE_LICENSE("GPL");

