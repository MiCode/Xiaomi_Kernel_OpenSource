/* SPDX-License-Identifier: ISC */
/* Copyright (c) 2019, The Linux Foundation. All rights reserved. */

#ifndef __WIL_CONFIG_H__
#define __WIL_CONFIG_H__

#define WIL_CONFIG_VAR_OFFSET(_struct, _var) (offsetof(_struct, _var))
#define WIL_CONFIG_VAR_SIZE(_struct, _var) (sizeof_field(_struct, _var))

enum wil_ini_param_type {
	wil_ini_param_type_unsigned,
	wil_ini_param_type_signed,
	wil_ini_param_type_string,
	wil_ini_param_type_macaddr,
};

#define WIL_CONFIG_INI_PARAM(_name, _type, _ref, _offset, _size, _min, _max)\
	{						\
		(_name),				\
		(_type),				\
		(_ref),					\
		(_offset),				\
		(_size),				\
		(_min),					\
		(_max),					\
		(NULL),					\
	}

#define WIL_CONFIG_INI_STRING_PARAM(_name, _ref, _offset, _size)\
	{						\
		(_name),				\
		(wil_ini_param_type_string),		\
		(_ref),					\
		(_offset),				\
		(_size),				\
		(0),					\
		(0),					\
		(NULL),					\
	}

#define WIL_CONFIG_INI_PARAM_WITH_HANDLER(_name, _handler)\
	{						\
		(_name),				\
		(wil_ini_param_type_string),		\
		(NULL),					\
		(0),					\
		(0),					\
		(0),					\
		(0),					\
		(_handler),				\
	}

/* forward declaration */
struct wil6210_priv;

struct wil_config_entry {
	char *name; /* variable name as expected in wigig.ini file */
	enum wil_ini_param_type type; /* variable type */
	void *var_ref; /* reference to global variable */
	u16 var_offset; /* offset to field inside wil6210_priv structure */
	u32 var_size;  /* size (in bytes) of the field */
	unsigned long min_val; /* minimum value, for range checking */
	unsigned long max_val; /* maximum value, for range checking */
	/* handler function for complex parameters */
	int (*handler)(struct wil6210_priv *wil, const char *buf,
		       size_t count);
};

/* read and parse ini file */
int wil_parse_config_ini(struct wil6210_priv *wil);

#endif /* __WIL_CONFIG_H__ */
