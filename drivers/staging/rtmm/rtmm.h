#ifndef __RTMM_H__
#define __RTMM_H__

enum {
	RTMM_POOL_THREADINFO = 0,
	RTMM_POOL_PGD,
	RTMM_POOL_KMALLOC_ORDER2,
	RTMM_POOL_DRM,
	RTMM_POOL_NR
};

#define KMALLOC_POOL_ORDER2 2
#define DRM_POOL_ORDER 6

int __init rtmm_pool_init(struct kobject *rtmm_kobj);
void *rtmm_alloc(int pool_type);
void rtmm_free(void *addr, int pool_type);

int __init rtmm_reclaim_init(struct kobject *rtmm_kobj);

int __init rtmm_compact_init(struct kobject *rtmm_kobj);

__weak int compact_memory_handler(int write)
{
	return 0;
}

#ifdef CONFIG_RTMM
bool rtmm_pool(const char *name);

#define DEFINE_SYSFS_OPS(_NAME, _TYPE)				\
struct _NAME##_attribute {					\
	struct attribute attr;					\
	ssize_t (*show)(_TYPE *, char *buf);			\
	ssize_t (*store)(_TYPE *, const char *buf, size_t len);	\
};								\
								\
static ssize_t _NAME##_attr_show(struct kobject *kobj,		\
			struct attribute *attr, char *buf)	\
{								\
	struct _NAME##_attribute *attribute;			\
	_TYPE *p;						\
								\
	attribute = container_of(attr,				\
			struct _NAME##_attribute, attr);	\
	if (!attribute->show)					\
		return -EIO;					\
								\
	p = container_of(kobj, _TYPE, kobj);			\
	return attribute->show(p, buf);				\
}								\
								\
static ssize_t _NAME##_attr_store(struct kobject *kobj,		\
			struct attribute *attr,			\
			const char *buf, size_t len)		\
{								\
	struct _NAME##_attribute *attribute;			\
	_TYPE *p;						\
								\
	attribute = container_of(attr,				\
			struct _NAME##_attribute, attr);	\
	if (!attribute->store)					\
		return -EIO;					\
								\
	p = container_of(kobj, _TYPE, kobj);			\
	return attribute->store(p, buf, len);			\
}								\
								\
static const struct sysfs_ops _NAME##_sysfs_ops = {		\
	.show = _NAME##_attr_show,				\
	.store = _NAME##_attr_store,				\
}

#else
static inline bool rtmm_pool(const char *name)
{
        return false;
}
#endif

#endif
