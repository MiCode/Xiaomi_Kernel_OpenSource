/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GBE_H
#define GBE_H
enum GBE_KICKER {
	KIR_GBE1,
	KIR_GBE2,
	KIR_NUM,
};
void gbe_boost(enum GBE_KICKER kicker, int boost);
void gbe_trace_printk(int pid, char *module, char *string);
void gbe_trace_count(int tid, int val, const char *fmt, ...);

extern struct dentry *gbe_debugfs_dir;
#endif

