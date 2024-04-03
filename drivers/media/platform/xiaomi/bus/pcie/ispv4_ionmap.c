/*
 * Copyright (c) 2020, Xiaomi, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "ispv4 ionmap: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/iommu.h>
#include <linux/dma-buf.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <uapi/media/ispv4_defs.h>
#include <linux/mfd/ispv4_defs.h>
#include <linux/debugfs.h>
#include <linux/list.h>

MODULE_IMPORT_NS(DMA_BUF);

extern struct dentry *ispv4_debugfs;
static int ispv4_ionmap_debug_demain_unmap(struct ispv4_ionmap_dev *dev,
					   enum ispv4_ionmap_region region);

#define V4_IATU_OUTBOUND_CPUADDR0 0x60000000
#define V4_IATU_OUTBOUND_PCIADDR0 0xEF000000

#define V4_IATU_OUTBOUND_CPUADDR1 0x50000000
#define V4_IATU_OUTBOUND_PCIADDR1 0xDF000000
struct ispv4_addrmap {
	dma_addr_t start;
	dma_addr_t end;
};

static const struct ispv4_addrmap ionaddr_list[] = {
	// npu module0 16Mb
	[ISPV4_IONMAP_FOR_NPU0] = {
		.start = 0xEF000000,
		.end = 0xEFFFFFFF,
	},
	// tunning data 8Mb
	[ISPV4_IONMAP_FOR_TUNNING] = {
		.start = 0xF0000000,
		.end = 0xF07FFFFF,
	},
	// isp param 8Mb
	[ISPV4_IONMAP_FOR_PARAM] = {
		.start = 0xF0800000,
		.end = 0xF0FFFFFF,
	},
	// catch info(META)1 16Mb
	[ISPV4_IONMAP_FOR_CATCHINFO1] = {
		.start = 0xF1000000,
		.end = 0xF1FFFFFF,
	},
	// catch info(META)2 16Mb
	[ISPV4_IONMAP_FOR_CATCHINFO2] = {
		.start = 0xF2000000,
		.end = 0xF2FFFFFF,
	},
	// catch image1 32Mb
	[ISPV4_IONMAP_FOR_CATCHIMG1] = {
		.start = 0xF3000000,
		.end = 0xF4FFFFFF,
	},
	// catch image2 32Mb
	[ISPV4_IONMAP_FOR_CATCHIMG2] = {
		.start = 0xF5000000,
		.end = 0xF6FFFFFF,
	},
	// 3A info 64Mb
	[ISPV4_IONMAP_FOR_3A] = {
		.start = 0xF7000000,
		.end = 0xFAFFFFFF,
	},
	// DDR traning 1Mb
	[ISPV4_IONMAP_FOR_DDRTRANING] = {
		.start = 0xFB000000,
		.end = 0xFB0FFFFF,
	},
	// COREDUMP 32Mb
	[ISPV4_IONMAP_FOR_COREDUMP] = {
		.start = 0xFB100000,
		.end = 0xFD0FFFFF,
	},
	// FDFA 1Mb
	[ISPV4_IONMAP_FOR_FAFD] = {
		.start = 0xFD100000,
		.end = 0xFD1FFFFF,
	},
	// npu module1 16Mb
	[ISPV4_IONMAP_FOR_NPU1] = {
		.start = 0xFD200000,
		.end = 0xFE1FFFFF
	},
};

#define REGION_WITH_NAME(region) [region] = #region

static char *ionmap_region_name[ISPV4_IONMAP_NUM] = {
	REGION_WITH_NAME(ISPV4_IONMAP_FOR_NPU0),
	REGION_WITH_NAME(ISPV4_IONMAP_FOR_TUNNING),
	REGION_WITH_NAME(ISPV4_IONMAP_FOR_PARAM),
	REGION_WITH_NAME(ISPV4_IONMAP_FOR_CATCHINFO1),
	REGION_WITH_NAME(ISPV4_IONMAP_FOR_CATCHINFO2),
	REGION_WITH_NAME(ISPV4_IONMAP_FOR_CATCHIMG1),
	REGION_WITH_NAME(ISPV4_IONMAP_FOR_CATCHIMG2),
	REGION_WITH_NAME(ISPV4_IONMAP_FOR_3A),
	REGION_WITH_NAME(ISPV4_IONMAP_FOR_DDRTRANING),
	REGION_WITH_NAME(ISPV4_IONMAP_FOR_COREDUMP),
	REGION_WITH_NAME(ISPV4_IONMAP_FOR_FAFD),
	REGION_WITH_NAME(ISPV4_IONMAP_FOR_NPU1),
};

struct ispv4_ionmap_entry {
	int ion_fd;
	struct list_head mapentry;
	size_t maped_sz;
	struct dma_buf *buf;
	struct sg_table *sgl;
	struct dma_buf_attachment *attach;
	atomic_t maped;
	atomic_t could_close;
};

struct ispv4_ionmap_domaindebug {
	unsigned int nent;
	void *maped_mem;
	enum ispv4_ionmap_region region;
	struct debugfs_blob_wrapper blob;
	struct dentry *bfile;
	struct dentry *wfile;
	struct list_head node;
	size_t iomaped_size;
	struct page **pages;
	size_t page_count;
	struct sg_table sgt;
};

struct ispv4_ionmap_dev {
	struct iommu_domain *pcie_domain;
	struct platform_device *pdev;
	struct device *dev;
	struct ispv4_ionmap_entry entrys[ISPV4_IONMAP_NUM];
	struct list_head no_region_entry;
	struct device comp_dev;
	struct dentry *debugfs;
	struct mutex list_lock;
	struct list_head debug_node;
};

static int ispv4_ionmapfd(struct ispv4_ionmap_dev *dev, int fd,
			  enum ispv4_ionmap_region region, u32 *iova)
{
	int ret;
	int iommu_flag = IOMMU_READ | IOMMU_WRITE | IOMMU_NOEXEC | IOMMU_MMIO;
	struct ispv4_ionmap_entry *entry;
	dma_addr_t addr;
	u32 size;

	if (region >= ISPV4_IONMAP_NUM)
		return -ENOPARAM;

	entry = &dev->entrys[region];
	addr = ionaddr_list[region].start;
	size = ionaddr_list[region].end - addr + 1;

	ret = atomic_cmpxchg(&entry->maped, 0, 1);
	if (ret != 0) {
		ret = -EBUSY;
		goto busy;
	}

	entry->buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(entry->buf)) {
		ret = PTR_ERR(entry->buf);
		dev_err(dev->dev, "dma buf get failed %d\n", ret);
		goto buf_get_failed;
	}

	entry->attach = dma_buf_attach(entry->buf, &dev->pdev->dev);
	if (IS_ERR_OR_NULL(entry->attach)) {
		ret = PTR_ERR(entry->attach);
		dev_err(dev->dev, "dma buf attach failed %d\n", ret);
		goto attach_failed;
	}

	/* Has been maped to a alloced iova */
	entry->sgl = dma_buf_map_attachment(entry->attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(entry->sgl)) {
		ret = PTR_ERR(entry->sgl);
		dev_err(dev->dev, "dma buf attachment map failed %d\n", ret);
		goto dma_map_failed;
	}

	/* Remap this sg to a specified iova. */
	entry->maped_sz = iommu_map_sgtable(dev->pcie_domain, addr, entry->sgl,
					    iommu_flag);
	if (entry->maped_sz == 0 || entry->maped_sz > size) {
		ret = -EIO;
		dev_err(dev->dev, "iommu map sgl failed %d\n", ret);
		goto io_map_failed;
	} else {
		dev_dbg(dev->dev ,"iommu map region %d to 0x%x\n", region, addr);
	}

	if (iova != NULL)
		*iova = addr - V4_IATU_OUTBOUND_PCIADDR0 + V4_IATU_OUTBOUND_CPUADDR0;

	atomic_set(&entry->could_close, 1);
	dev_info(dev->dev, "map finish region=%d size=%d\n", region, entry->maped_sz);
	return 0;

io_map_failed:
	dma_buf_unmap_attachment(entry->attach, entry->sgl, DMA_BIDIRECTIONAL);
dma_map_failed:
	dma_buf_detach(entry->buf, entry->attach);
attach_failed:
	dma_buf_put(entry->buf);
buf_get_failed:
	atomic_set(&entry->maped, 0);
busy:
	return ret;
}

static int ispv4_ionunmap(struct ispv4_ionmap_dev *dev,
			  enum ispv4_ionmap_region region)
{
	int ret;
	struct ispv4_ionmap_entry *entry;
	dma_addr_t addr;

	if (region >= ISPV4_IONMAP_NUM)
		return -ENOPARAM;
	entry = &dev->entrys[region];
	addr = ionaddr_list[region].start;

	ret = atomic_cmpxchg(&entry->could_close, 1, 0);
	if (ret != 1)
		return -EINVAL;

	iommu_unmap(dev->pcie_domain, addr, entry->maped_sz);
	dma_buf_unmap_attachment(entry->attach, entry->sgl, DMA_BIDIRECTIONAL);
	dma_buf_detach(entry->buf, entry->attach);
	dma_buf_put(entry->buf);

	atomic_set(&entry->maped, 0);
	return 0;
}

static int ispv4_ionmapfd_no_region(struct ispv4_ionmap_dev *dev, int fd, u32 *iova)
{
	int ret;
	struct ispv4_ionmap_entry *entry;
	dma_addr_t addr;

	list_for_each_entry (entry, &dev->no_region_entry, mapentry) {
		if (entry->ion_fd == fd) {
			dev_err(dev->dev, "this buf has been maped,fd %d\n", entry->ion_fd);
			return -EBUSY;
		}
	}

	entry = kmalloc(sizeof(struct ispv4_ionmap_entry), GFP_KERNEL);
	if (!entry) {
		ret = -ENOMEM;
		goto malloc_fail;
	}

	entry->ion_fd = fd;
	entry->buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(entry->buf)) {
		ret = PTR_ERR(entry->buf);
		dev_err(dev->dev, "dma buf get failed %d\n", ret);
		goto buf_get_failed;
	}

	entry->attach = dma_buf_attach(entry->buf, dev->pdev->dev.parent);
	if (IS_ERR_OR_NULL(entry->attach)) {
		ret = PTR_ERR(entry->attach);
		dev_err(dev->dev, "dma buf attach failed %d\n", ret);
		goto attach_failed;
	}

	/* Has been maped to a alloced iova */
	entry->sgl = dma_buf_map_attachment(entry->attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(entry->sgl)) {
		ret = PTR_ERR(entry->sgl);
		dev_err(dev->dev, "dma buf attachment map failed %d\n", ret);
		goto dma_map_failed;
	}
	addr = sg_dma_address(entry->sgl->sgl);

	if (iova != NULL)
		*iova = addr - V4_IATU_OUTBOUND_PCIADDR1 + V4_IATU_OUTBOUND_CPUADDR1;

	atomic_set(&entry->maped, 1);
	atomic_set(&entry->could_close, 1);
	mutex_lock(&dev->list_lock);
	list_add_tail(&entry->mapentry, &dev->no_region_entry);
	mutex_unlock(&dev->list_lock);
	dev_info(dev->dev, "map finish , no region\n");
	return 0;

dma_map_failed:
	dma_buf_detach(entry->buf, entry->attach);
attach_failed:
	dma_buf_put(entry->buf);
buf_get_failed:
	kfree(entry);
malloc_fail:
	return ret;
}


static int ispv4_ionunmap_no_region(struct ispv4_ionmap_dev *dev,
			  int fd)
{
	int ret;
	struct ispv4_ionmap_entry *entry;
	bool find = false;

	mutex_lock(&dev->list_lock);

	list_for_each_entry (entry, &dev->no_region_entry, mapentry) {
		if (entry->ion_fd == fd) {
			dev_info(dev->dev, "find fd %d\n", entry->ion_fd);
			find = true;
			break;
		}
	}
	if (!find) {
		dev_err(dev->dev, "can not find fd %d\n", fd);
		ret =  -EINVAL;
		goto unlock;
	}
	ret = atomic_cmpxchg(&entry->could_close, 1, 0);
	if (ret != 1) {
		ret = -EINVAL;
		goto unlock;
	}

	dma_buf_unmap_attachment(entry->attach, entry->sgl, DMA_BIDIRECTIONAL);
	dma_buf_detach(entry->buf, entry->attach);
	dma_buf_put(entry->buf);
	atomic_set(&entry->maped, 0);
	list_del(&entry->mapentry);
	iommu_flush_iotlb_all(dev->pcie_domain);
	mutex_unlock(&dev->list_lock);

	kfree(entry);
	dev_info(dev->dev, "unmap no region\n");
	return 0;

unlock:
	mutex_unlock(&dev->list_lock);
	return ret;
}

static int ispv4_ionunmap_entry_unsafe(struct ispv4_ionmap_dev *dev,
			  struct ispv4_ionmap_entry *entry)
{
	int ret;

	ret = atomic_cmpxchg(&entry->could_close, 1, 0);
	if (ret != 1) {
		ret = -EINVAL;
	}

	dma_buf_unmap_attachment(entry->attach, entry->sgl, DMA_BIDIRECTIONAL);
	dma_buf_detach(entry->buf, entry->attach);
	dma_buf_put(entry->buf);
	atomic_set(&entry->maped, 0);
	iommu_flush_iotlb_all(dev->pcie_domain);

	return 0;
}

int ispv4_remove_any_mappd(struct ispv4_ionmap_dev *dev)
{
	int ret = 0;
	int i;
	struct ispv4_ionmap_entry *entry;
	struct ispv4_ionmap_entry *tmp;

	mutex_lock(&dev->list_lock);
	list_for_each_entry_safe(entry, tmp, &dev->no_region_entry, mapentry) {
		if (entry->ion_fd) {
			dev_warn(dev->dev, "fd %d not unmap in last time, unmap it\n",
					   entry->ion_fd);
			ret = ispv4_ionunmap_entry_unsafe(dev, entry);
			if (ret) {
				dev_err(dev->dev, "unmap fd %d fail ret %d\n",
						   entry->ion_fd, ret);
				break;
			}
			list_del(&entry->mapentry);
			kfree(entry);
		}
	}
	mutex_unlock(&dev->list_lock);

	for (i = 0; i < ISPV4_IONMAP_NUM; i++) {
		entry = &dev->entrys[i];
		ispv4_ionmap_debug_demain_unmap(dev, i);
		if (atomic_read(&entry->maped) == 1) {
			ispv4_ionunmap(dev, i);
		}
	}

	return ret;
}

static ssize_t ispv4_ionmap_wr_write(struct file *filp, const char __user *data,
				     size_t len, loff_t *ppos)
{
	struct ispv4_ionmap_domaindebug *ddev;
	static u32 kbuf[1024];
	u32 remain_len = len;
	loff_t p = *ppos;
	u32 start_offset = p;
	int ret = 0;
	ddev = filp->private_data;

	if ((len + p) > ddev->iomaped_size)
		return -EINVAL;

	while (remain_len) {
		u32 deal_len;

		deal_len = min_t(u32, sizeof(kbuf), remain_len);
		if (copy_from_user(kbuf, data, deal_len)) {
			ret = -EFAULT;
			goto out;
		}
		memcpy((u8 *)ddev->maped_mem + start_offset, kbuf, deal_len);
		start_offset += deal_len;
		remain_len -= deal_len;
	}

out:
	if (ret == 0) {
		*ppos += len;
		return len;
	} else
		return ret;
}

static int ispv4_ionmap_wr_open(struct inode *inode, struct file *filp)
{
	struct ispv4_ionmap_dev *idev = inode->i_private;
	filp->private_data = idev;
	return 0;
}

static struct file_operations ionmap_debug_write_fops = {
	.open = ispv4_ionmap_wr_open,
	.write = ispv4_ionmap_wr_write,
};

void free_scatter_pages(struct page **pages, unsigned int count)
{
	int i;
	if (!pages)
		return;
	for (i = 0; i < count; i++)
		__free_page(pages[i]);
	kvfree(pages);
}

static struct page **alloc_scatter_pages(unsigned int count, gfp_t gfp)
{
	struct page **pages;
	unsigned int i = 0, array_size = count * sizeof(struct page *);

	pages = vzalloc(array_size);
	if (!pages)
		return NULL;

	gfp |= __GFP_NOWARN | __GFP_HIGHMEM;
	for (i = 0; i < count; ++i) {
		struct page *page = alloc_page(gfp);
		if (!page) {
			free_scatter_pages(pages, i);
			return NULL;
		}
		pages[i] = page;
	}
	return pages;
}

static int ispv4_ionmap_debug_demain_map(struct ispv4_ionmap_dev *dev,
					 enum ispv4_ionmap_region region)
{
	struct ispv4_ionmap_domaindebug *domain;
	int iommu_flag = IOMMU_READ | IOMMU_WRITE | IOMMU_NOEXEC | IOMMU_MMIO;
	bool busy = false;
	int ret = 0, size;
	char file_name[32];
	char *region_name;

	mutex_lock(&dev->list_lock);

	list_for_each_entry (domain, &dev->debug_node, node) {
		if (domain->region == region) {
			busy = true;
			break;
		}
	}

	if (busy) {
		ret = -EBUSY;
		goto busy;
	}

	domain = kzalloc(sizeof(*domain), GFP_KERNEL);
	if (domain == NULL) {
		ret = -ENOMEM;
		goto domain_alloc_err;
	}

	domain->region = region;
	size = ionaddr_list[region].end - ionaddr_list[region].start + 1;
	domain->page_count = ALIGN(size, SZ_4K) >> PAGE_SHIFT;
	domain->pages = alloc_scatter_pages(domain->page_count, GFP_KERNEL);
	if (domain->pages == NULL) {
		ret = -ENOMEM;
		goto sp_alloc_failed;
	}

	ret = sg_alloc_table_from_pages(&domain->sgt, domain->pages,
					domain->page_count, 0, size,
					GFP_KERNEL);
	if (ret != 0) {
		goto sgl_alloc_failed;
	}

	domain->iomaped_size =
		iommu_map_sgtable(dev->pcie_domain, ionaddr_list[region].start,
				  &domain->sgt, iommu_flag);
	if (domain->iomaped_size != size) {
		goto iommu_map_err;
	}

	domain->maped_mem =
		vmap(domain->pages, domain->page_count, VM_MAP, __pgprot(PROT_NORMAL_NC));
	if (domain->maped_mem == NULL) {
		ret = -EFAULT;
		goto vmap_err;
	}

	domain->blob.size = size;
	domain->blob.data = domain->maped_mem;

	region_name = strstr(ionmap_region_name[region], "FOR");
	snprintf(file_name, 32, "R_%s", region_name);
	domain->bfile = debugfs_create_blob(file_name, 0444, dev->debugfs,
					    &domain->blob);
	if (IS_ERR_OR_NULL(domain->bfile)) {
		dev_err(dev->dev, "%s debugfs failed\n", __FUNCTION__);
		ret = PTR_ERR(domain->bfile);
		goto debugfs_blob_err;
	}
	snprintf(file_name, 32, "W_%s", region_name);
	domain->wfile = debugfs_create_file(file_name, 0222, dev->debugfs,
					    domain, &ionmap_debug_write_fops);
	if (IS_ERR_OR_NULL(domain->wfile)) {
		dev_err(dev->dev, "%s debugfs failed\n", __FUNCTION__);
		ret = PTR_ERR(domain->wfile);
		goto debugfs_w_err;
	}

	list_add(&domain->node, &dev->debug_node);
	mutex_unlock(&dev->list_lock);

	return 0;

debugfs_w_err:
	debugfs_remove(domain->bfile);
debugfs_blob_err:
	vunmap(domain->blob.data);
vmap_err:
	iommu_unmap(dev->pcie_domain, ionaddr_list[region].start,
		    domain->iomaped_size);
iommu_map_err:
	sg_free_table(&domain->sgt);
sgl_alloc_failed:
	free_scatter_pages(domain->pages, domain->page_count);
sp_alloc_failed:
	kfree(domain);
domain_alloc_err:
busy:
	mutex_unlock(&dev->list_lock);
	dev_err(dev->dev, "%s failed %d\n", __FUNCTION__, ret);
	return ret;
};

static int ispv4_ionmap_debug_demain_unmap(struct ispv4_ionmap_dev *dev,
					   enum ispv4_ionmap_region region)
{
	struct ispv4_ionmap_domaindebug *domain;
	bool exist = false;
	int ret = 0;

	mutex_lock(&dev->list_lock);
	list_for_each_entry (domain, &dev->debug_node, node) {
		if (domain->region == region) {
			exist = true;
			break;
		}
	}

	if (!exist) {
		ret = -EINVAL;
		goto no_exist;
	}

	debugfs_remove(domain->bfile);
	debugfs_remove(domain->wfile);
	list_del(&domain->node);
	vunmap(domain->blob.data);
	iommu_unmap(dev->pcie_domain, ionaddr_list[region].start,
		    domain->iomaped_size);
	sg_free_table(&domain->sgt);
	free_scatter_pages(domain->pages, domain->page_count);
	kfree(domain);
	mutex_unlock(&dev->list_lock);
	return 0;

no_exist:
	mutex_unlock(&dev->list_lock);
	return ret;
}

static int ispv4_comp_bind(struct device *comp, struct device *master,
			   void *master_data)
{
	struct ispv4_v4l2_dev *priv = master_data;
	struct ispv4_ionmap_dev *ionmap_dev = NULL;

	ionmap_dev = container_of(comp, struct ispv4_ionmap_dev, comp_dev);
	priv->v4l2_ionmap.mapfd = ispv4_ionmapfd;
	priv->v4l2_ionmap.unmap = ispv4_ionunmap;
	priv->v4l2_ionmap.mapfd_no_region = ispv4_ionmapfd_no_region;
	priv->v4l2_ionmap.unmap_no_region = ispv4_ionunmap_no_region;
	priv->v4l2_ionmap.remove_any_mappd = ispv4_remove_any_mappd;
	priv->v4l2_ionmap.dev = ionmap_dev;
	priv->v4l2_ionmap.avalid = true;

	dev_info(comp, "avalid!!\n");
	return 0;
}

static void ispv4_comp_unbind(struct device *comp, struct device *master,
			      void *master_data)
{
	struct ispv4_v4l2_dev *priv = master_data;
	priv->v4l2_ionmap.avalid = false;
}

__maybe_unused static const struct component_ops comp_ops = {
	.bind = ispv4_comp_bind,
	.unbind = ispv4_comp_unbind
};

static int ispv4_ionmap_dbgfs_map(void *data, u64 val)
{
	if (val >= ISPV4_IONMAP_NUM)
		return -EFAULT;
	return ispv4_ionmap_debug_demain_map(data, val);
}

static int ispv4_ionmap_dbgfs_unmap(void *data, u64 val)
{
	if (val >= ISPV4_IONMAP_NUM)
		return -EFAULT;
	return ispv4_ionmap_debug_demain_unmap(data, val);
}

DEFINE_DEBUGFS_ATTRIBUTE(map_fops, NULL, ispv4_ionmap_dbgfs_map, "%llu\n");
DEFINE_DEBUGFS_ATTRIBUTE(unmap_fops, NULL, ispv4_ionmap_dbgfs_unmap, "%llu\n");

static int ionmap_info_show(struct seq_file *m, void *unused)
{
	int i = 0;
	seq_puts(m, "=====================================================\n");
	for (; i < ISPV4_IONMAP_NUM; i++) {
		seq_printf(m, "{ %d } %s [0x%08x - 0x%08x]\n", i,
			   ionmap_region_name[i], ionaddr_list[i].start,
			   ionaddr_list[i].end);
	}
	seq_puts(m, "=====================================================\n");
	return 0;
}

DEFINE_SHOW_ATTRIBUTE(ionmap_info);

static int ispv4_ionmap_probe(struct platform_device *pdev)
{
	int ret, i;
	struct ispv4_ionmap_entry *entry;
	struct ispv4_ionmap_dev *ispv4_ionmap = devm_kzalloc(
		&pdev->dev, sizeof(struct ispv4_ionmap_dev), GFP_KERNEL);

	if (ispv4_ionmap == NULL)
		return -ENOMEM;

	dma_set_mask(&pdev->dev, DMA_BIT_MASK(64));
	ispv4_ionmap->pdev = pdev;
	ispv4_ionmap->dev = &pdev->dev;
	ispv4_ionmap->pcie_domain = iommu_get_domain_for_dev(pdev->dev.parent);
	if (ispv4_ionmap->pcie_domain == NULL) {
		dev_err(&pdev->dev, "get pci dma domain failed.\n");
		return -ENODEV;
	}
	platform_set_drvdata(pdev, ispv4_ionmap);

	for (i = 0; i < ISPV4_IONMAP_NUM; i++) {
		entry = &ispv4_ionmap->entrys[i];
		atomic_set(&entry->maped, 0);
		atomic_set(&entry->could_close, 0);
	}

	device_initialize(&ispv4_ionmap->comp_dev);
	dev_set_name(&ispv4_ionmap->comp_dev, "ispv4-ionmap");
	pr_err("comp add %s! priv = %x, comp_name = %s\n", __FUNCTION__,
	       ispv4_ionmap, dev_name(&ispv4_ionmap->comp_dev));
	ret = component_add(&ispv4_ionmap->comp_dev, &comp_ops);
	if (ret != 0) {
		dev_err(&pdev->dev, "register ionmap component failed.\n");
		return ret;
	}
	mutex_init(&ispv4_ionmap->list_lock);
	INIT_LIST_HEAD(&ispv4_ionmap->no_region_entry);
	INIT_LIST_HEAD(&ispv4_ionmap->debug_node);
	ispv4_ionmap->debugfs =
		debugfs_create_dir("ispv4_ionmap", ispv4_debugfs);
	if (IS_ERR_OR_NULL(ispv4_ionmap)) {
		dev_err(&pdev->dev, "create debugfs failed!\n");
	} else {
		debugfs_create_file("map", 0222, ispv4_ionmap->debugfs,
				    ispv4_ionmap, &map_fops);
		debugfs_create_file("unmap", 0222, ispv4_ionmap->debugfs,
				    ispv4_ionmap, &unmap_fops);
		debugfs_create_file("info", 0444, ispv4_ionmap->debugfs,
				    ispv4_ionmap, &ionmap_info_fops);
	}

	dev_info(&pdev->dev, "probe finish\n");
	return 0;
}

static int ispv4_ionmap_remove(struct platform_device *pdev)
{
	struct ispv4_ionmap_dev *dev = platform_get_drvdata(pdev);
	component_del(&dev->comp_dev, &comp_ops);

	debugfs_remove(dev->debugfs);
	ispv4_remove_any_mappd(dev);

	return 0;
}

static struct platform_driver ispv4_ionmap_driver = {
	.probe = ispv4_ionmap_probe,
	.remove = ispv4_ionmap_remove,
	.driver = {
		.name = "ispv4-ionmap",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(ispv4_ionmap_driver);
MODULE_AUTHOR("ChenHonglin<chenhonglin@xiaomi.com>");
MODULE_DESCRIPTION("Xiaomi ISPV4.");
MODULE_LICENSE("GPL v2");
