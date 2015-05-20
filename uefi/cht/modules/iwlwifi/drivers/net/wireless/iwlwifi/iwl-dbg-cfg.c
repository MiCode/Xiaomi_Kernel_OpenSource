/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2013 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2013 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#include <linux/types.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/firmware.h>
#include "iwl-dbg-cfg.h"

/* grab default values */
#undef CPTCFG_IWLWIFI_SUPPORT_DEBUG_OVERRIDES
#include "iwl-constants.h"
#if IS_ENABLED(CPTCFG_IWLXVT)
#include "xvt/constants.h"
#endif
#if IS_ENABLED(CPTCFG_IWLMVM)
#include "mvm/constants.h"
#endif

struct iwl_dbg_cfg current_dbg_config = {
#define DBG_CFG_REINCLUDE
#define IWL_DBG_CFG(type, name) \
	.name = IWL_ ## name,
#define IWL_DBG_CFG_NODEF(type, name) /* no default */
#define IWL_DBG_CFG_BIN(name) /* nothing, default empty */
#define IWL_DBG_CFG_BINA(name, max) /* nothing, default empty */
#define IWL_DBG_CFG_RANGE(type, name, min, max)	\
	.name = IWL_ ## name,
#include "iwl-dbg-cfg.h"
#undef IWL_DBG_CFG
#undef IWL_DBG_CFG_NODEF
#undef IWL_DBG_CFG_BIN
#undef IWL_DBG_CFG_BINA
#undef IWL_DBG_CFG_RANGE
};

static const char dbg_cfg_magic[] = "[IWL DEBUG CONFIG DATA]";

#define DBG_CFG_LOADER(_type)							\
static void dbg_cfg_load_ ## _type(const char *name, const char *val,		\
				   _type *out, _type min, _type max)		\
{										\
	_type r;								\
										\
	if (kstrto ## _type(val, 0, &r)) {					\
		printk(KERN_INFO "iwlwifi debug config: Invalid data for %s: %s\n",\
		       name, val);						\
		return;								\
	}									\
										\
	if (min && max && (r < min || r > max)) {				\
		printk(KERN_INFO "iwlwifi debug config: value %u for %s out of range [%u,%u]\n",\
		       r, name, min, max);					\
		return;								\
	}									\
										\
	*out = r;								\
	printk(KERN_INFO "iwlwifi debug config: %s=%d\n", name, *out);		\
}

DBG_CFG_LOADER(u8)
DBG_CFG_LOADER(u16)
DBG_CFG_LOADER(u32)

static void __maybe_unused /* no users yet */
dbg_cfg_load_bool(const char *name, const char *val, u32 *out, int min, int max)
{
	u8 v;

	if (kstrtou8(val, 0, &v)) {
		printk(KERN_INFO "iwlwifi debug config: Invalid data for %s: %s\n",
		       name, val);
	} else {
		*out = v;
		printk(KERN_INFO "iwlwifi debug config: %s=%d\n", name, *out);
	}
}

static int __maybe_unused /* because nobody uses it yet */
dbg_cfg_load_bin(const char *name, const char *val, struct iwl_dbg_cfg_bin *out)
{
	int len = strlen(val);
	u8 *data;

	if (len % 2)
		goto error;
	len /= 2;

	data = kzalloc(len, GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	if (hex2bin(data, val, len)) {
		kfree(data);
		goto error;
	}
	out->data = data;
	out->len = len;
	printk(KERN_INFO "iwlwifi debug config: %d bytes for %s\n", len, name);
	return 0;
error:
	printk(KERN_INFO "iwlwifi debug config: Invalid data for %s\n", name);
	return -EINVAL;
}

#ifdef CPTCFG_IWLWIFI_DEBUGFS
void iwl_dbg_cfg_init_dbgfs(struct dentry *root)
{
	struct dentry *dbg_cfg_root = debugfs_create_dir("dbg_cfg", root);

	/* not worried about errors as this is a debug tool */

	if (!dbg_cfg_root)
		return;

#define IWL_DBG_CFG(type, name) \
	debugfs_create_ ## type(#name, S_IRUSR | S_IWUSR, dbg_cfg_root, \
				&current_dbg_config.name);
#define IWL_DBG_CFG_NODEF(type, name) IWL_DBG_CFG(type, name)
#define IWL_DBG_CFG_BIN(name) /* blobs can't be preconfigured in debugfs */
#define IWL_DBG_CFG_BINA(name, max) /* ditto for blob arrays */
#define IWL_DBG_CFG_RANGE(type, name, min, max) /* not supported yet */
#include "iwl-dbg-cfg.h"
#undef IWL_DBG_CFG
#undef IWL_DBG_CFG_NODEF
#undef IWL_DBG_CFG_BIN
#undef IWL_DBG_CFG_BINA
#undef IWL_DBG_CFG_RANGE
}
#endif /* CPTCFG_IWLWIFI_DEBUGFS */

void iwl_dbg_cfg_free(struct iwl_dbg_cfg *dbgcfg)
{
#define IWL_DBG_CFG(t, n) /* nothing */
#define IWL_DBG_CFG_NODEF(t, n) /* nothing */
#define IWL_DBG_CFG_BIN(n)				\
	do {						\
		kfree(dbgcfg->n.data);			\
		dbgcfg->n.data = NULL;			\
		dbgcfg->n.len = 0;			\
	} while (0);
#define IWL_DBG_CFG_BINA(n, max)			\
	do {						\
		int i;					\
							\
		for (i = 0; i < max; i++) {		\
			kfree(dbgcfg->n[i].data);	\
			dbgcfg->n[i].data = NULL;	\
			dbgcfg->n[i].len = 0;		\
		}					\
		dbgcfg->n_ ## n = 0;			\
	} while (0);
#define IWL_DBG_CFG_RANGE(t, n, min, max) /* nothing */
#include "iwl-dbg-cfg.h"
#undef IWL_DBG_CFG
#undef IWL_DBG_CFG_NODEF
#undef IWL_DBG_CFG_BIN
#undef IWL_DBG_CFG_BINA
#undef IWL_DBG_CFG_RANGE
}

void iwl_dbg_cfg_load_ini(struct device *dev, struct iwl_dbg_cfg *dbgcfg)
{
	const struct firmware *fw;
	char *data, *end, *pos;
	int err;

	/* TODO: maybe add a per-device file? */
	err = request_firmware(&fw, "iwl-dbg-cfg.ini", dev);
	if (err)
		return;

	/* must be ini file style with magic section header */
	if (fw->size < strlen(dbg_cfg_magic))
		goto release;
	if (memcmp(fw->data, dbg_cfg_magic, strlen(dbg_cfg_magic))) {
		printk(KERN_INFO "iwlwifi debug config: file is malformed\n");
		goto release;
	}

	/* +1 guarantees the last line gets NUL-terminated even without \n */
	data = kzalloc(fw->size - strlen(dbg_cfg_magic) + 1, GFP_KERNEL);
	if (!data)
		goto release;
	memcpy(data, fw->data + strlen(dbg_cfg_magic),
	       fw->size - strlen(dbg_cfg_magic));
	end = data + fw->size - strlen(dbg_cfg_magic);
	/* replace CR/LF with NULs to make parsing easier */
	for (pos = data; pos < end; pos++) {
		if (*pos == '\n' || *pos == '\r')
			*pos = '\0';
	}

	pos = data;
	while (pos < end) {
		const char *line = pos;
		/* skip to next line */
		while (pos < end && *pos)
			pos++;
		/* skip to start of next line, over empty ones if any */
		while (pos < end && !*pos)
			pos++;

		/* skip empty lines and comments */
		if (!*line || *line == '#')
			continue;

#define IWL_DBG_CFG(t, n)						\
		if (strncmp(#n, line, strlen(#n)) == 0 &&		\
		    line[strlen(#n)] == '=') {				\
			dbg_cfg_load_##t(#n, line + strlen(#n) + 1,	\
					 &dbgcfg->n, 0, 0);		\
			continue;					\
		}
#define IWL_DBG_CFG_NODEF(t, n) IWL_DBG_CFG(t, n)
#define IWL_DBG_CFG_BIN(n)						\
		if (strncmp(#n, line, strlen(#n)) == 0 &&		\
		    line[strlen(#n)] == '=') {				\
			dbg_cfg_load_bin(#n, line + strlen(#n) + 1,	\
					 &dbgcfg->n);			\
			continue;					\
		}
#define IWL_DBG_CFG_BINA(n, max)					\
		if (strncmp(#n, line, strlen(#n)) == 0 &&		\
		    line[strlen(#n)] == '=') {				\
			if (dbgcfg->n_##n >= max) {			\
				printk(KERN_INFO			\
				       "iwlwifi debug config: " #n " given too many times\n");\
				continue;				\
			}						\
			if (!dbg_cfg_load_bin(#n, line + strlen(#n) + 1,\
					      &dbgcfg->n[dbgcfg->n_##n]))\
				dbgcfg->n_##n++;			\
			continue;					\
		}
#define IWL_DBG_CFG_RANGE(t, n, min, max)				\
		if (strncmp(#n, line, strlen(#n)) == 0 &&		\
		    line[strlen(#n)] == '=') {				\
			dbg_cfg_load_##t(#n, line + strlen(#n) + 1,	\
					 &dbgcfg->n, min, max);		\
			continue;					\
		}
#include "iwl-dbg-cfg.h"
#undef IWL_DBG_CFG
#undef IWL_DBG_CFG_NODEF
#undef IWL_DBG_CFG_BIN
#undef IWL_DBG_CFG_BINA
#undef IWL_DBG_CFG_RANGE
		printk(KERN_INFO "iwlwifi debug config: failed to load line \"%s\"\n",
		       line);
	}

	kfree(data);
 release:
	release_firmware(fw);
}
