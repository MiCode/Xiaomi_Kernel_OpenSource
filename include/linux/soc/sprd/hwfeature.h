// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef __SPRD_HWFEATURE__
#define __SPRD_HWFEATURE__

#define HWFEATURE_KPROPERTY_DEFAULT_VALUE     "-1"
#define HWFEATURE_STR_SIZE_LIMIT              64
#define HWFEATURE_STR_SIZE_LIMIT_KEY          1024

#ifdef CONFIG_SPRD_HWFEATURE
void sprd_kproperty_get(const char *key, char *value, const char *default_value);
int sprd_kproperty_eq(const char *key, const char *value);
#else
static inline int sprd_kproperty_eq(const char *key, const char *value)
{
	return 0;
}

static inline void sprd_kproperty_get(const char *key, char *value, const char *default_value)
{
	if (default_value == NULL)
		default_value = HWFEATURE_KPROPERTY_DEFAULT_VALUE;

	strlcpy(value, default_value, HWFEATURE_STR_SIZE_LIMIT);
}
#endif

#define SPRD_KPROPERTY_EXPECTED_VALUE(name) \
	static inline int sprd_kproperty_##name(const char *expected_value) \
	{ \
		return sprd_kproperty_eq("auto/"#name, expected_value); \
	}

SPRD_KPROPERTY_EXPECTED_VALUE(efuse)
SPRD_KPROPERTY_EXPECTED_VALUE(chipid)
SPRD_KPROPERTY_EXPECTED_VALUE(adc)
SPRD_KPROPERTY_EXPECTED_VALUE(gpio)

#endif /*__SPRD_HWFEATURE__*/
