/* Copyright (c) 2010-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/iommu.h>
#include <linux/platform_device.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/idr.h>
#include <linux/sizes.h>
#include <asm/page.h>
#include <linux/qcom_iommu.h>
#include <linux/msm_iommu_domains.h>
#include "msm_iommu_priv.h"
#include <soc/qcom/socinfo.h>

struct msm_iova_data {
	struct rb_node node;
	struct mem_pool *pools;
	int npools;
	struct iommu_domain *domain;
	int domain_num;
};

struct msm_iommu_data_entry {
	struct list_head list;
	void *data;
};

static struct rb_root domain_root;
DEFINE_MUTEX(domain_mutex);
static DEFINE_IDA(domain_nums);

void msm_iommu_set_client_name(struct iommu_domain *domain, char const *name)
{
	struct msm_iommu_priv *priv = domain->priv;

	priv->client_name = name;
}

int msm_use_iommu(void)
{
	return iommu_present(msm_iommu_non_sec_bus_type);
}

bool msm_iommu_page_size_is_supported(unsigned long page_size)
{
	return page_size == SZ_4K
		|| page_size == SZ_64K
		|| page_size == SZ_1M
		|| page_size == SZ_16M;
}

int msm_iommu_map_extra(struct iommu_domain *domain,
				unsigned long start_iova,
				phys_addr_t phy_addr,
				unsigned long size,
				unsigned long page_size,
				int prot)
{
	int ret = 0;
	int i = 0;
	unsigned long temp_iova = start_iova;
	/* the extra "padding" should never be written to. map it
	 * read-only. */
	prot &= ~IOMMU_WRITE;

	if (msm_iommu_page_size_is_supported(page_size)) {
		struct scatterlist *sglist;
		unsigned int nrpages = PFN_ALIGN(size) >> PAGE_SHIFT;
		struct page *dummy_page = phys_to_page(phy_addr);
		size_t map_ret;

		sglist = vmalloc(sizeof(*sglist) * nrpages);
		if (!sglist) {
			ret = -ENOMEM;
			goto out;
		}

		sg_init_table(sglist, nrpages);

		for (i = 0; i < nrpages; i++)
			sg_set_page(&sglist[i], dummy_page, PAGE_SIZE, 0);

		map_ret = iommu_map_sg(domain, temp_iova, sglist, nrpages,
					prot);
		if (map_ret != size) {
			pr_err("%s: could not map extra %lx in domain %p\n",
				__func__, start_iova, domain);
			ret = -EINVAL;
		} else {
			ret = 0;
		}

		vfree(sglist);
	} else {
		unsigned long order = get_order(page_size);
		unsigned long aligned_size = ALIGN(size, page_size);
		unsigned long nrpages = aligned_size >> (PAGE_SHIFT + order);

		for (i = 0; i < nrpages; i++) {
			ret = iommu_map(domain, temp_iova, phy_addr, page_size,
						prot);
			if (ret) {
				pr_err("%s: could not map %lx in domain %p, error: %d\n",
					__func__, start_iova, domain, ret);
				ret = -EAGAIN;
				goto out;
			}
			temp_iova += page_size;
		}
	}
	return ret;
out:
	for (; i > 0; --i) {
		temp_iova -= page_size;
		iommu_unmap(domain, start_iova, page_size);
	}
	return ret;
}

void msm_iommu_unmap_extra(struct iommu_domain *domain,
				unsigned long start_iova,
				unsigned long size,
				unsigned long page_size)
{
	int i;
	unsigned long order = get_order(page_size);
	unsigned long aligned_size = ALIGN(size, page_size);
	unsigned long nrpages =  aligned_size >> (PAGE_SHIFT + order);
	unsigned long temp_iova = start_iova;

	for (i = 0; i < nrpages; ++i) {
		iommu_unmap(domain, temp_iova, page_size);
		temp_iova += page_size;
	}
}

static int msm_iommu_map_iova_phys(struct iommu_domain *domain,
				unsigned long iova,
				phys_addr_t phys,
				unsigned long size,
				int cached)
{
	int ret;
	int prot = IOMMU_WRITE | IOMMU_READ;

	prot |= cached ? IOMMU_CACHE : 0;

	ret = iommu_map(domain, iova, phys, size, prot);
	if (ret) {
		pr_err("%s: could not map extra %lx in domain %p\n",
			__func__, iova, domain);
	}

	return ret;

}

int msm_iommu_map_contig_buffer(phys_addr_t phys,
				unsigned int domain_no,
				unsigned int partition_no,
				unsigned long size,
				unsigned long align,
				unsigned long cached,
				dma_addr_t *iova_val)
{
	unsigned long iova;
	int ret;
	struct iommu_domain *domain;

	if (size & (align - 1))
		return -EINVAL;

	if (!msm_use_iommu()) {
		*iova_val = phys;
		return 0;
	}

	ret = msm_allocate_iova_address(domain_no, partition_no, size, align,
						&iova);

	if (ret)
		return -ENOMEM;

	domain = msm_get_iommu_domain(domain_no);
	if (!domain) {
		pr_err("%s: Could not find domain %u. Unable to map\n",
			__func__, domain_no);
		msm_free_iova_address(iova, domain_no, partition_no, size);
		return -EINVAL;
	}
	ret = msm_iommu_map_iova_phys(domain, iova, phys, size, cached);

	if (ret)
		msm_free_iova_address(iova, domain_no, partition_no, size);
	else
		*iova_val = iova;

	return ret;
}
EXPORT_SYMBOL(msm_iommu_map_contig_buffer);

void msm_iommu_unmap_contig_buffer(dma_addr_t iova,
					unsigned int domain_no,
					unsigned int partition_no,
					unsigned long size)
{
	struct iommu_domain *domain;

	if (!msm_use_iommu())
		return;

	domain = msm_get_iommu_domain(domain_no);
	if (domain) {
		iommu_unmap_range(domain, iova, size);
	} else {
		pr_err("%s: Could not find domain %u. Unable to unmap\n",
			__func__, domain_no);
	}
	msm_free_iova_address(iova, domain_no, partition_no, size);
}
EXPORT_SYMBOL(msm_iommu_unmap_contig_buffer);

static struct msm_iova_data *find_domain(int domain_num)
{
	struct rb_root *root = &domain_root;
	struct rb_node *p;

	mutex_lock(&domain_mutex);
	p = root->rb_node;
	while (p) {
		struct msm_iova_data *node;

		node = rb_entry(p, struct msm_iova_data, node);
		if (domain_num < node->domain_num)
			p = p->rb_left;
		else if (domain_num > node->domain_num)
			p = p->rb_right;
		else {
			mutex_unlock(&domain_mutex);
			return node;
		}
	}
	mutex_unlock(&domain_mutex);
	return NULL;
}

static int add_domain(struct msm_iova_data *node)
{
	struct rb_root *root = &domain_root;
	struct rb_node **p = &root->rb_node;
	struct rb_node *parent = NULL;

	mutex_lock(&domain_mutex);
	while (*p) {
		struct msm_iova_data *tmp;

		parent = *p;

		tmp = rb_entry(parent, struct msm_iova_data, node);

		if (node->domain_num < tmp->domain_num)
			p = &(*p)->rb_left;
		else if (node->domain_num > tmp->domain_num)
			p = &(*p)->rb_right;
		else
			BUG();
	}
	rb_link_node(&node->node, parent, p);
	rb_insert_color(&node->node, root);
	mutex_unlock(&domain_mutex);
	return 0;
}

static int remove_domain(struct iommu_domain *domain)
{
	struct rb_root *root = &domain_root;
	struct rb_node *n;
	struct msm_iova_data *node;
	int ret = -EINVAL;

	mutex_lock(&domain_mutex);

	for (n = rb_first(root); n; n = rb_next(n)) {
		node = rb_entry(n, struct msm_iova_data, node);
		if (node->domain == domain) {
			rb_erase(&node->node, &domain_root);
			ret = 0;
			break;
		}
	}
	mutex_unlock(&domain_mutex);
	return ret;
}

struct iommu_domain *msm_get_iommu_domain(int domain_num)
{
	struct msm_iova_data *data;

	data = find_domain(domain_num);

	if (data)
		return data->domain;
	else
		return NULL;
}
EXPORT_SYMBOL(msm_get_iommu_domain);

int msm_find_domain_no(const struct iommu_domain *domain)
{
	struct rb_root *root = &domain_root;
	struct rb_node *n;
	struct msm_iova_data *node;
	int domain_num = -EINVAL;

	mutex_lock(&domain_mutex);

	for (n = rb_first(root); n; n = rb_next(n)) {
		node = rb_entry(n, struct msm_iova_data, node);
		if (node->domain == domain) {
			domain_num = node->domain_num;
			break;
		}
	}
	mutex_unlock(&domain_mutex);
	return domain_num;
}
EXPORT_SYMBOL(msm_find_domain_no);

struct iommu_domain *msm_iommu_domain_find(const char *name)
{
	struct iommu_group *group = iommu_group_find(name);

	if (!group)
		return NULL;
	return iommu_group_get_iommudata(group);
}
EXPORT_SYMBOL(msm_iommu_domain_find);

int msm_iommu_domain_no_find(const char *name)
{
	struct iommu_domain *domain = msm_iommu_domain_find(name);

	if (!domain)
		return -EINVAL;
	return msm_find_domain_no(domain);
}
EXPORT_SYMBOL(msm_iommu_domain_no_find);

static struct msm_iova_data *msm_domain_to_iova_data(struct iommu_domain
						     const *domain)
{
	struct rb_root *root = &domain_root;
	struct rb_node *n;
	struct msm_iova_data *node;
	struct msm_iova_data *iova_data = ERR_PTR(-EINVAL);

	mutex_lock(&domain_mutex);

	for (n = rb_first(root); n; n = rb_next(n)) {
		node = rb_entry(n, struct msm_iova_data, node);
		if (node->domain == domain) {
			iova_data = node;
			break;
		}
	}
	mutex_unlock(&domain_mutex);
	return iova_data;
}

int msm_allocate_iova_address(unsigned int iommu_domain,
					unsigned int partition_no,
					unsigned long size,
					unsigned long align,
					unsigned long *iova)
{
	struct msm_iova_data *data;
	struct mem_pool *pool;
	unsigned long va;
	unsigned long pageno;
	unsigned long nbits = PAGE_ALIGN(size) >> PAGE_SHIFT;
	int ret;


	data = find_domain(iommu_domain);

	if (!data)
		return -EINVAL;

	if (partition_no >= data->npools)
		return -EINVAL;

	pool = &data->pools[partition_no];

	if (!pool->bitmap)
		return -EINVAL;

	ret = -ENOMEM;
	mutex_lock(&pool->pool_mutex);
	align = (1 << get_order(align)) - 1;

	pageno = bitmap_find_next_zero_area(pool->bitmap, pool->nr_pages,
						0, nbits, align);
	if (pageno < pool->nr_pages) {
		pool->free -= size;
		bitmap_set(pool->bitmap, pageno, nbits);
		va = pool->paddr + pageno * PAGE_SIZE;
		*iova = va;
		ret = 0;
	}

	mutex_unlock(&pool->pool_mutex);
	return ret;
}

void msm_free_iova_address(unsigned long iova,
				unsigned int iommu_domain,
				unsigned int partition_no,
				unsigned long size)
{
	struct msm_iova_data *data;
	struct mem_pool *pool;

	data = find_domain(iommu_domain);

	if (!data) {
		WARN(1, "Invalid domain %d\n", iommu_domain);
		return;
	}

	if (partition_no >= data->npools) {
		WARN(1, "Invalid partition %d for domain %d\n",
			partition_no, iommu_domain);
		return;
	}

	pool = &data->pools[partition_no];

	if (!pool)
		return;

	pool->free += size;

	mutex_lock(&pool->pool_mutex);
	bitmap_clear(pool->bitmap, (iova - pool->paddr) >> PAGE_SHIFT,
				PAGE_ALIGN(size) >> PAGE_SHIFT);
	mutex_unlock(&pool->pool_mutex);
}

int msm_register_domain(struct msm_iova_layout *layout)
{
	int i;
	struct msm_iova_data *data;
	struct mem_pool *pools;
	struct bus_type *bus;
	int no_redirect;

	if (!layout)
		return -EINVAL;

	data = kmalloc(sizeof(*data), GFP_KERNEL);

	if (!data)
		return -ENOMEM;

	pools = kcalloc(layout->npartitions, sizeof(struct mem_pool),
			GFP_KERNEL);

	if (!pools)
		goto free_data;

	for (i = 0; i < layout->npartitions; i++) {
		if (layout->partitions[i].size == 0)
			continue;

		pools[i].paddr = layout->partitions[i].start;
		pools[i].size = layout->partitions[i].size;
		mutex_init(&pools[i].pool_mutex);
		pools[i].nr_pages = PAGE_ALIGN(layout->partitions[i].size)
					>> PAGE_SHIFT;
		pools[i].bitmap = kzalloc(
				BITS_TO_LONGS(pools[i].nr_pages) * sizeof(long),
					GFP_KERNEL);
		if (!pools[i].bitmap)
			continue;
	}

	bus = layout->is_secure == MSM_IOMMU_DOMAIN_SECURE ?
					&msm_iommu_sec_bus_type :
					msm_iommu_non_sec_bus_type;

	data->pools = pools;
	data->npools = layout->npartitions;
	data->domain_num = ida_simple_get(&domain_nums, 0, 0, GFP_KERNEL);
	if (data->domain_num < 0)
		goto free_pools;

	data->domain = iommu_domain_alloc(bus);
	if (!data->domain)
		goto free_domain_num;

	no_redirect = !(layout->domain_flags & MSM_IOMMU_DOMAIN_PT_CACHEABLE);
	iommu_domain_set_attr(data->domain,
			DOMAIN_ATTR_COHERENT_HTW_DISABLE, &no_redirect);

	msm_iommu_set_client_name(data->domain, layout->client_name);

	add_domain(data);

	return data->domain_num;

free_domain_num:
	ida_simple_remove(&domain_nums, data->domain_num);

free_pools:
	for (i = 0; i < layout->npartitions; i++)
		kfree(pools[i].bitmap);
	kfree(pools);
free_data:
	kfree(data);

	return -EINVAL;
}
EXPORT_SYMBOL(msm_register_domain);

int msm_unregister_domain(struct iommu_domain *domain)
{
	unsigned int i;
	struct msm_iova_data *data = msm_domain_to_iova_data(domain);

	if (IS_ERR_OR_NULL(data)) {
		pr_err("%s: Could not find iova_data\n", __func__);
		return -EINVAL;
	}

	if (remove_domain(data->domain)) {
		pr_err("%s: Domain not found. Failed to remove domain\n",
			__func__);
	}

	iommu_domain_free(domain);

	ida_simple_remove(&domain_nums, data->domain_num);

	for (i = 0; i < data->npools; ++i)
		kfree(data->pools[i].bitmap);

	kfree(data->pools);
	kfree(data);
	return 0;
}
EXPORT_SYMBOL(msm_unregister_domain);

static int find_and_add_contexts(struct iommu_group *group,
				 const struct device_node *node,
				 unsigned int num_contexts)
{
	unsigned int i;
	struct device *ctx;
	const char *name;
	struct device_node *ctx_node;
	int ret_val = 0;

	for (i = 0; i < num_contexts; ++i) {
		ctx_node = of_parse_phandle((struct device_node *) node,
					    "qcom,iommu-contexts", i);
		if (!ctx_node) {
			pr_err("Unable to parse phandle #%u\n", i);
			ret_val = -EINVAL;
			goto out;
		}
		if (of_property_read_string(ctx_node, "label", &name)) {
			pr_err("Could not find label property\n");
			ret_val = -EINVAL;
			goto out;
		}
		ctx = msm_iommu_get_ctx(name);
		if (IS_ERR(ctx)) {
			ret_val = PTR_ERR(ctx);
			goto out;
		}

		ret_val = iommu_group_add_device(group, ctx);
		if (ret_val)
			goto out;
	}
out:
	return ret_val;
}

static int create_and_add_domain(struct iommu_group *group,
				 struct device_node const *node,
				 char const *name)
{
	unsigned int ret_val = 0;
	unsigned int i, j;
	struct msm_iova_layout l;
	struct msm_iova_partition *part = 0;
	struct iommu_domain *domain = 0;
	unsigned int *addr_array = 0;
	unsigned int array_size;
	int domain_no;
	int secure_domain;
	int l2_redirect;

	if (of_get_property(node, "qcom,virtual-addr-pool", &array_size)) {
		l.npartitions = array_size / sizeof(unsigned int) / 2;
		part = kmalloc(
			sizeof(struct msm_iova_partition) * l.npartitions,
			       GFP_KERNEL);
		if (!part) {
			pr_err("%s: could not allocate space for partition",
				__func__);
			ret_val = -ENOMEM;
			goto out;
		}
		addr_array = kmalloc(array_size, GFP_KERNEL);
		if (!addr_array) {
			ret_val = -ENOMEM;
			goto free_mem;
		}

		ret_val = of_property_read_u32_array(node,
					"qcom,virtual-addr-pool",
					addr_array,
					array_size/sizeof(unsigned int));
		if (ret_val) {
			ret_val = -EINVAL;
			goto free_mem;
		}

		for (i = 0, j = 0; j < l.npartitions * 2; i++, j += 2) {
			part[i].start = addr_array[j];
			part[i].size = addr_array[j+1];
		}
	} else {
		l.npartitions = 1;
		part = kmalloc(
			sizeof(struct msm_iova_partition) * l.npartitions,
			       GFP_KERNEL);
		if (!part) {
			pr_err("%s: could not allocate space for partition",
				__func__);
			ret_val = -ENOMEM;
			goto out;
		}
		part[0].start = 0x0;
		part[0].size = 0xFFFFFFFF;
	}

	l.client_name = name;
	l.partitions = part;

	secure_domain = of_property_read_bool(node, "qcom,secure-domain");
	l.is_secure = (secure_domain) ? MSM_IOMMU_DOMAIN_SECURE : 0;

	l2_redirect = of_property_read_bool(node, "qcom,l2-redirect");
	l.domain_flags = (l2_redirect) ? MSM_IOMMU_DOMAIN_PT_CACHEABLE : 0;

	domain_no = msm_register_domain(&l);
	if (domain_no >= 0)
		domain = msm_get_iommu_domain(domain_no);
	else
		ret_val = domain_no;

	iommu_group_set_iommudata(group, domain, NULL);

free_mem:
	kfree(addr_array);
	kfree(part);
out:
	return ret_val;
}

static int __msm_group_get_domain(struct device *dev, void *data)
{
	struct msm_iommu_data_entry *list_entry;
	struct list_head *dev_list = data;
	int ret_val = 0;

	list_entry = kmalloc(sizeof(*list_entry), GFP_KERNEL);
	if (list_entry) {
		list_entry->data = dev;
		list_add(&list_entry->list, dev_list);
	} else {
		ret_val = -ENOMEM;
	}

	return ret_val;
}

static void __msm_iommu_group_remove_device(struct iommu_group *grp)
{
	struct msm_iommu_data_entry *tmp;
	struct msm_iommu_data_entry *list_entry;
	struct list_head dev_list;

	INIT_LIST_HEAD(&dev_list);
	iommu_group_for_each_dev(grp, &dev_list, __msm_group_get_domain);

	list_for_each_entry_safe(list_entry, tmp, &dev_list, list) {
		iommu_group_remove_device(list_entry->data);
		list_del(&list_entry->list);
		kfree(list_entry);
	}
}


static int iommu_domain_parse_dt(const struct device_node *dt_node)
{
	struct device_node *node;
	int sz;
	unsigned int num_contexts;
	int ret_val = 0;
	struct iommu_group *group = 0;
	const char *name;
	struct msm_iommu_data_entry *grp_list_entry;
	struct msm_iommu_data_entry *tmp;
	struct list_head iommu_group_list;

	INIT_LIST_HEAD(&iommu_group_list);

	for_each_child_of_node(dt_node, node) {
		group = iommu_group_alloc();
		if (IS_ERR(group)) {
			ret_val = PTR_ERR(group);
			group = 0;
			goto free_group;
		}

		/* This is only needed to clean up memory if something fails */
		grp_list_entry = kmalloc(sizeof(*grp_list_entry),
					   GFP_KERNEL);
		if (grp_list_entry) {
			grp_list_entry->data = group;
			list_add(&grp_list_entry->list, &iommu_group_list);
		} else {
			ret_val = -ENOMEM;
			goto free_group;
		}

		if (of_property_read_string(node, "label", &name)) {
			ret_val = -EINVAL;
			goto free_group;
		}
		iommu_group_set_name(group, name);

		if (!of_get_property(node, "qcom,iommu-contexts", &sz)) {
			pr_err("Could not find qcom,iommu-contexts property\n");
			ret_val = -EINVAL;
			goto free_group;
		}
		num_contexts = sz / sizeof(unsigned int);

		ret_val = find_and_add_contexts(group, node, num_contexts);
		if (ret_val)
			goto free_group;

		ret_val = create_and_add_domain(group, node, name);
		if (ret_val) {
			ret_val = -EINVAL;
			goto free_group;
		}

		/* Remove reference to the group that is taken when the group
		 * is allocated. This will ensure that when all the devices in
		 * the group are removed the group will be released.
		 */
		iommu_group_put(group);
	}

	list_for_each_entry_safe(grp_list_entry, tmp, &iommu_group_list, list) {
		list_del(&grp_list_entry->list);
		kfree(grp_list_entry);
	}
	goto out;

free_group:
	list_for_each_entry_safe(grp_list_entry, tmp, &iommu_group_list, list) {
		struct iommu_domain *d;

		d = iommu_group_get_iommudata(grp_list_entry->data);
		if (d)
			msm_unregister_domain(d);

		__msm_iommu_group_remove_device(grp_list_entry->data);
		list_del(&grp_list_entry->list);
		kfree(grp_list_entry);
	}
	iommu_group_put(group);
out:
	return ret_val;
}

static int iommu_domain_probe(struct platform_device *pdev)
{
	struct iommu_domains_pdata *p  = pdev->dev.platform_data;
	int i, j;

	if (!msm_use_iommu())
		return -ENODEV;

	if (pdev->dev.of_node)
		return iommu_domain_parse_dt(pdev->dev.of_node);
	else if (!p)
		return -ENODEV;

	for (i = 0; i < p->ndomains; i++) {
		struct msm_iova_layout l;
		struct msm_iova_partition *part;
		struct msm_iommu_domain *domains;

		domains = p->domains;
		l.npartitions = domains[i].npools;
		part = kmalloc(
			sizeof(struct msm_iova_partition) * l.npartitions,
				GFP_KERNEL);

		if (!part) {
			pr_info("%s: could not allocate space for domain %d",
				__func__, i);
			continue;
		}

		for (j = 0; j < l.npartitions; j++) {
			part[j].start = p->domains[i].iova_pools[j].paddr;
			part[j].size = p->domains[i].iova_pools[j].size;
		}

		l.partitions = part;

		msm_register_domain(&l);

		kfree(part);
	}

	for (i = 0; i < p->nnames; i++) {
		struct device *ctx = msm_iommu_get_ctx(
						p->domain_names[i].name);
		struct iommu_domain *domain;

		if (!ctx)
			continue;

		domain = msm_get_iommu_domain(p->domain_names[i].domain);

		if (!domain)
			continue;

		if (iommu_attach_device(domain, ctx)) {
			WARN(1, "%s: could not attach domain %p to context %s. iommu programming will not occur.\n",
				__func__, domain, p->domain_names[i].name);
			continue;
		}
	}
	return 0;
}

static int iommu_domain_exit(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id msm_iommu_domain_match_table[] = {
	{ .name = "qcom,iommu-domains", },
	{}
};

static struct platform_driver iommu_domain_driver = {
	.driver         = {
		.name = "iommu_domains",
		.of_match_table = msm_iommu_domain_match_table,
		.owner = THIS_MODULE
	},
	.probe		= iommu_domain_probe,
	.remove		= iommu_domain_exit,
};

static int __init msm_subsystem_iommu_init(void)
{
	int ret;

	ret = platform_driver_register(&iommu_domain_driver);
	if (ret != 0)
		pr_err("Failed to register IOMMU domain driver\n");
	return ret;
}

static void __exit msm_subsystem_iommu_exit(void)
{
	platform_driver_unregister(&iommu_domain_driver);
}

device_initcall(msm_subsystem_iommu_init);
module_exit(msm_subsystem_iommu_exit);
