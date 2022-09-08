/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_LP_SYSFS__
#define __MTK_LP_SYSFS__

#define MTK_LP_SYSFS_HAS_ENTRY		(1)

#define MTK_LP_SYSFS_ENTRY_NAME		"mtk_lpm"
#define MTK_LP_SYSFS_BUF_READSZ		8192
#define MTK_LP_SYSFS_BUF_WRITESZ	512

typedef ssize_t (*f_mtk_idle_sysfs_show)(char *ToUserBuf
			, size_t sz, void *priv);
typedef ssize_t (*f_mtk_idle_sysfs_write)(char *FromUserBuf
			, size_t sz, void *priv);

struct mtk_lp_sysfs_handle {
	void *_current;
};

struct mtk_lp_sysfs_op {
	f_mtk_idle_sysfs_show	fs_read;
	f_mtk_idle_sysfs_write	fs_write;
	void *priv;
};

struct mtk_lp_sysfs_attr {
	char	*name;
	umode_t	mode;
	struct mtk_lp_sysfs_op sysfs_op;
};
struct mtk_lp_sysfs_group {
	struct mtk_lp_sysfs_attr **attrs;
	unsigned int attr_num;
};


#define __MTK_LP_SYSFS_ATTR(_name, _mode, _read, _write, _priv) {\
	.name = __stringify(_name),\
	.mode = _mode,\
	.sysfs_op.fs_read = _read,\
	.sysfs_op.fs_write = _write,\
	.sysfs_op.priv = _priv,\
}

#define DEFINE_MTK_LP_SYSFS_ATTR(_name, _mode, _read, _write, _priv)\
	struct mtk_lp_sysfs_attr mtk_lp_sysfs_attr_##_name =\
		__MTK_LP_SYSFS_ATTR(_name, _mode, _read, _write, _priv)

#define MTK_LP_SYSFS_ATTR_PTR(_name)	(&mtk_lp_sysfs_attr_##_name)



/* Macro for auto generate which used by internal */
#define AUTO_MACRO_MTK_LP_SYSFS_ATTR_PTR(_mod, _name)\
	(&mtk_##_mod##_attrs_def.mtk_lp_sysfs_attr_##_name)

#define AUTO_MACRO_MTK_LP_SYSFS_GROUP(_mod, _name, _mode, _read, _write, _priv)\
	AUTO_MACRO_MTK_LP_SYSFS_ATTR_PTR(_mod, _name),

#define AUTO_MACRO_MTK_LP_SYSFS_STRUCT_DECLARE(_mod, _name, _mode\
		, _read, _write, _priv)\
	struct mtk_lp_sysfs_attr mtk_lp_sysfs_attr_##_name

#define AUTO_MACRO_MTK_LP_SYSFS_STRUCT_DEFINE(_mod, _name, _mode\
		, _read, _write, _priv)\
	.mtk_lp_sysfs_attr_##_name =\
		__MTK_LP_SYSFS_ATTR(_name, _mode, _read, _write, _priv),

#define MTK_LP_SYSFS_GROUP_PTR(_name)	(&mtk_lp_##_name##_group)

/* Macro for declare debug group sysfs */
#define DECLARE_MTK_LP_SYSFS_GROUP(_name, _mode, _foreach) \
	struct _##_name##_attr_declare_section_ \
		_foreach(AUTO_MACRO_MTK_LP_SYSFS_STRUCT_DECLARE, _name); \
	struct _##_name##_attr_declare_section_ mtk_##_name##_attrs_def =\
		_foreach(AUTO_MACRO_MTK_LP_SYSFS_STRUCT_DEFINE, _name); \
	struct mtk_lp_sysfs_attr *mtk_lp_##_name##_group_attr[] =\
		_foreach(AUTO_MACRO_MTK_LP_SYSFS_GROUP, _name);\
	struct mtk_lp_sysfs_group mtk_lp_##_name##_group = {\
		.attrs = mtk_lp_##_name##_group_attr,\
		.attr_num =\
			sizeof(struct _##_name##_attr_declare_section_)\
				/ sizeof(struct mtk_lp_sysfs_attr)\
	}

#define IS_MTK_LP_SYS_HANDLE_VALID(x)\
	({ struct mtk_lp_sysfs_handle *Po = x;\
	if ((Po == NULL) || (Po->_current == NULL))\
		Po = NULL;\
	(Po != NULL); })

int mtk_lp_sysfs_entry_func_create(const char *name, int mode
			, struct mtk_lp_sysfs_handle *parent
			, struct mtk_lp_sysfs_handle *handle);

int mtk_lp_sysfs_entry_func_node_add(const char *name
		, int mode, const struct mtk_lp_sysfs_op *op
		, struct mtk_lp_sysfs_handle *parent
		, struct mtk_lp_sysfs_handle *node);

int mtk_lp_sysfs_entry_func_node_remove(
		struct mtk_lp_sysfs_handle *node);

int mtk_lp_sysfs_entry_func_group_create(const char *name
		, int mode, struct mtk_lp_sysfs_group *_group
		, struct mtk_lp_sysfs_handle *parent
		, struct mtk_lp_sysfs_handle *handle);

int mtk_lp_sysfs_get_path(struct mtk_lp_sysfs_handle *handle
		, char *name, int namelen);

#endif
