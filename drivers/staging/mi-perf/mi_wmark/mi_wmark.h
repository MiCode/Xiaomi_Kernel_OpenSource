#ifndef _MI_PERF_MODULE_H_
#define _MI_PERF_MODULE_H_

#define MI_PERF_ATTR_RW(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
		__ATTR(_name, 0660,	\
		_name##_show, _name##_store)

#endif
