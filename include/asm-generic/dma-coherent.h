#ifndef DMA_COHERENT_H
#define DMA_COHERENT_H

#ifdef CONFIG_HAVE_GENERIC_DMA_COHERENT
/*
 * These three functions are only for dma allocator.
 * Don't use them in device drivers.
 */
int dma_alloc_from_coherent_attr(struct device *dev, ssize_t size,
				       dma_addr_t *dma_handle, void **ret,
				       struct dma_attrs *attrs);
int dma_release_from_coherent_attr(struct device *dev, size_t size, void *vaddr,
				       struct dma_attrs *attrs);
#define dma_alloc_from_coherent(d, s, h, r) \
	 dma_alloc_from_coherent_attr(d, s, h, r, NULL)
#define dma_release_from_coherent(d, s, v) \
	 dma_release_from_coherent_attr(d, s, v, NULL)

int dma_mmap_from_coherent(struct device *dev, struct vm_area_struct *vma,
			    void *cpu_addr, size_t size, int *ret);
/*
 * Standard interface
 */
#define ARCH_HAS_DMA_DECLARE_COHERENT_MEMORY
extern int
dma_declare_coherent_memory(struct device *dev, dma_addr_t bus_addr,
			    dma_addr_t device_addr, size_t size, int flags);

extern void
dma_release_declared_memory(struct device *dev);

extern void *
dma_mark_declared_memory_occupied(struct device *dev,
				  dma_addr_t device_addr, size_t size);
#else
#define dma_alloc_from_coherent_attr(dev, size, handle, ret, attr) (0)
#define dma_release_from_coherent_attr(dev, size, vaddr, attr) (0)
#define dma_alloc_from_coherent(dev, size, handle, ret) (0)
#define dma_release_from_coherent(dev, order, vaddr) (0)
#define dma_mmap_from_coherent(dev, vma, vaddr, order, ret) (0)
#endif

#endif
