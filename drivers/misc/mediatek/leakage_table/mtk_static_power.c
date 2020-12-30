// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/proc_fs.h>
#include <linux/debugfs.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/nvmem-consumer.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>

#define SP_TAG "[Power/spower] "
#define SPOWERTABLE 0
#define SPOWER_LOG_NONE 0
#define SPOWER_LOG_WITH_PRINTK 1
#define SPOWER_LOG_PRINT SPOWER_LOG_NONE
#define SPOWER_ERR(fmt, args...)
#define SPOWER_INFO(fmt, args...)
#define VSIZE 14
#define TSIZE 22
#define MAX_TABLE_SIZE 3
#define trow(tab, ti)           ((tab)->trow[ti])
#define mA(tab, vi, ti)         ((tab)->trow[ti].mA[vi])
#define mV(tab, vi)             ((tab)->vrow[0].mV[vi])
#define deg(tab, ti)            ((tab)->trow[ti].deg)
#define vsize(tab)              ((tab)->vsize)
#define tsize(tab)              ((tab)->tsize)
#define tab_validate(tab)       (!!(tab) && (tab)->data != NULL)

struct spower_raw_t {
	int vsize;
	int tsize;
	int table_size;
	int *table[3];
	unsigned int devinfo_domain;
	unsigned int spower_id;
	unsigned int leakage_id;
	unsigned int instance;
	bool print_leakage;
};

struct voltage_row_s {
	int mV[VSIZE];
};

struct temperature_row_s {
	int deg;
	int mA[VSIZE];
};

struct sptab_s {
	int vsize;
	int tsize;
	int *data;
	struct voltage_row_s *vrow;
	struct temperature_row_s *trow;
	unsigned int devinfo_domain;
	unsigned int spower_id;
	unsigned int leakage_id;
	unsigned int instance;
	bool print_leakage;
};

struct sptab_list {
	struct sptab_s tab_raw[MAX_TABLE_SIZE];
};

struct spower_leakage_info {
	const char *name;
	unsigned int devinfo_idx;
	unsigned int devinfo_offset;
	unsigned int value;
	unsigned int v_of_fuse;
	int t_of_fuse;
	unsigned int instance;
};

struct spower_leakage_info *spower_lkg_info;
struct spower_raw_t *spower_raw;
static struct sptab_s *sptab;
static char static_power_buf[128];
static char static_power_buf_precise[128];

static const int devinfo_table[] = {
	3539,   492,    1038,   106,    231,    17,     46,     2179,
	4,      481,    1014,   103,    225,    17,     45,     2129,
	3,      516,    1087,   111,    242,    19,     49,     2282,
	4,      504,    1063,   108,    236,    18,     47,     2230,
	4,      448,    946,    96,     210,    15,     41,     1986,
	2,      438,    924,    93,     205,    14,     40,     1941,
	2,      470,    991,    101,    220,    16,     43,     2080,
	3,      459,    968,    98,     215,    16,     42,     2033,
	3,      594,    1250,   129,    279,    23,     57,     2621,
	6,      580,    1221,   126,    273,    22,     56,     2561,
	6,      622,    1309,   136,    293,    24,     60,     2745,
	7,      608,    1279,   132,    286,    23,     59,     2683,
	6,      541,    1139,   117,    254,    20,     51,     2390,
	5,      528,    1113,   114,    248,    19,     50,     2335,
	4,      566,    1193,   123,    266,    21,     54,     2503,
	5,      553,    1166,   120,    260,    21,     53,     2446,
	5,      338,    715,    70,     157,    9,      29,     1505,
	3153,   330,    699,    69,     153,    9,      28,     1470,
	3081,   354,    750,    74,     165,    10,     31,     1576,
	3302,   346,    732,    72,     161,    10,     30,     1540,
	3227,   307,    652,    63,     142,    8,      26,     1371,
	2875,   300,    637,    62,     139,    7,      25,     1340,
	2809,   322,    683,    67,     149,    8,      27,     1436,
	3011,   315,    667,    65,     146,    8,      26,     1404,
	2942,   408,    862,    86,     191,    13,     37,     1811,
	1,      398,    842,    84,     186,    12,     36,     1769,
	1,      428,    903,    91,     200,    14,     39,     1896,
	2,      418,    882,    89,     195,    13,     38,     1853,
	2,      371,    785,    78,     173,    11,     33,     1651,
	3458,   363,    767,    76,     169,    10,     32,     1613,
	3379,   389,    823,    82,     182,    12,     35,     1729,
	1,      380,    804,    80,     177,    11,     34,     1689,
};

#if SPOWERTABLE
static inline void spower_tab_construct(struct sptab_s *tab,
	struct spower_raw_t *raw, unsigned int id)
{
	int i;
	struct sptab_s *ptab = (struct sptab_s *)tab;

	for (i = 0; i < raw->table_size; i++) {
		ptab->vsize = raw->vsize;
		ptab->tsize = raw->tsize;
		ptab->data = raw->table[i];
		ptab->vrow = (struct voltage_row_s *)ptab->data;
		ptab->trow = (struct temperature_row_s *)(ptab->data + ptab->vsize);
		ptab->devinfo_domain = raw->devinfo_domain;
		ptab->spower_id = id;
		ptab->leakage_id = raw->leakage_id;
		ptab->instance = raw->instance;
		ptab->print_leakage = raw->print_leakage;
		ptab++;
	}
}
#endif

int interpolate(int x1, int x2, int x3, int y1, int y2)
{
	if (x1 == x2)
		return (y1 + y2) / 2;

#if defined(__LP64__) || defined(_LP64)
	return (long long)(x3 - x1) * (long long)(y2 - y1) /
		       (long long)(x2 - x1) +
	       y1;
#else
	return div64_s64((long long)(x3 - x1) * (long long)(y2 - y1),
			 (long long)(x2 - x1)) +
	       y1;
#endif
}

int interpolate_2d(struct sptab_s *tab, int v1, int v2, int t1, int t2,
		   int voltage, int degree)
{
	int c1, c2, p1, p2, p;

	if ((v1 == v2) && (t1 == t2)) {
		p = mA(tab, v1, t1);
	} else if (v1 == v2) {
		c1 = mA(tab, v1, t1);
		c2 = mA(tab, v1, t2);
		p = interpolate(deg(tab, t1), deg(tab, t2), degree, c1, c2);
	} else if (t1 == t2) {
		c1 = mA(tab, v1, t1);
		c2 = mA(tab, v2, t1);
		p = interpolate(mV(tab, v1), mV(tab, v2), voltage, c1, c2);
	} else {
		c1 = mA(tab, v1, t1);
		c2 = mA(tab, v1, t2);
		p1 = interpolate(deg(tab, t1), deg(tab, t2), degree, c1, c2);

		c1 = mA(tab, v2, t1);
		c2 = mA(tab, v2, t2);
		p2 = interpolate(deg(tab, t1), deg(tab, t2), degree, c1, c2);

		p = interpolate(mV(tab, v1), mV(tab, v2), voltage, p1, p2);
	}

	return p;
}

void interpolate_table(struct sptab_s *spt, int c1, int c2, int c3,
		       struct sptab_s *tab1, struct sptab_s *tab2)
{
	int v, t;

	/* avoid divid error, if we have bad raw data table */
	if (unlikely(c1 == c2)) {
		*spt = *tab1;
		SPOWER_INFO("sptab equal to tab1:%d/%d\n", c1, c3);
	} else {
		SPOWER_INFO("make sptab %d, %d, %d\n", c1, c2, c3);
		for (t = 0; t < tsize(spt); t++) {
			for (v = 0; v < vsize(spt); v++) {
				int *p = &mA(spt, v, t);

				p[0] = interpolate(c1, c2, c3, mA(tab1, v, t),
						   mA(tab2, v, t));

				if (v == 0 || v == vsize(spt) - 1)
					SPOWER_INFO("ma1, ma2=%d, %d, %d\n",
						    mA(tab1, v, t),
						    mA(tab2, v, t), p[0]);
			}
			SPOWER_INFO("\n");
		}
		SPOWER_INFO("make sptab done!\n");
	}
}

int sptab_lookup(struct sptab_s *tab, int voltage, int degree)
{
	int x1, x2, y1, y2, i;
	int mamper;

	/* lookup voltage */
	for (i = 0; i < vsize(tab); i++) {
		if (voltage <= mV(tab, i))
			break;
	}

	if (unlikely(voltage == mV(tab, i))) {
		x1 = x2 = i;
	} else if (unlikely(i == vsize(tab))) {
		x1 = vsize(tab) - 2;
		x2 = vsize(tab) - 1;
	} else if (i == 0) {
		x1 = 0;
		x2 = 1;
	} else {
		x1 = i - 1;
		x2 = i;
	}

	/* lookup degree */
	for (i = 0; i < tsize(tab); i++) {
		if (degree <= deg(tab, i))
			break;
	}

	if (unlikely(degree == deg(tab, i))) {
		y1 = y2 = i;
	} else if (unlikely(i == tsize(tab))) {
		y1 = tsize(tab) - 2;
		y2 = tsize(tab) - 1;
	} else if (i == 0) {
		y1 = 0;
		y2 = 1;
	} else {
		y1 = i - 1;
		y2 = i;
	}

	mamper = interpolate_2d(tab, x1, x2, y1, y2, voltage, degree);

	/*
	 * SPOWER_INFO("x1=%d, x2=%d, y1=%d, y2=%d\n", x1, x2, y1, y2);
	 * SPOWER_INFO("sptab_lookup-volt=%d, deg=%d, lkg=%d\n",
	 *                                 voltage, degree, mamper);
	 */
	return mamper;
}

int mtk_spower_make_table(struct sptab_s *spt, int voltage, int degree,
			  unsigned int id, struct sptab_list *all_tab[], int n_domain)
{
	int i, j;
	struct sptab_s *tab[MAX_TABLE_SIZE], *tab1, *tab2, *tspt;
	int wat; /* leakage that reads from efuse */
	int devinfo_domain;
	int c[MAX_TABLE_SIZE] = {0};
	int temp;

	/** FIXME, test only; please read efuse to assign. **/
	/* wat = 80; */
	/* voltage = 1150; */
	/* degree = 30; */

	SPOWER_INFO("spower_raw->table_size : %d\n", spower_raw->table_size);
	/* find out target domain's 3 raw table */
	for (i = 0; i < spower_raw->table_size; i++)
		tab[i] = &(all_tab[id]->tab_raw[i]);

	/* get leakage that reads from efuse */
	wat = spower_lkg_info[tab[0]->leakage_id].value / 1000;

	/** lookup tables which the chip type locates to **/
	for (i = 0; i < spower_raw->table_size; i++) {
		devinfo_domain = tab[i]->devinfo_domain;
		SPOWER_INFO("devinfo_domain : 0x%x\n", devinfo_domain);
		//for (j = 0; j < MTK_SPOWER_MAX; j++) {
		for (j = 0; j < n_domain; j++) {
			/* get table of reference bank, and look up target in
			 * that table
			 */
			if (devinfo_domain & BIT(j)) {
				temp = (sptab_lookup(&(all_tab[j]->tab_raw[i]),
						     voltage, degree));
				/* SPOWER_INFO("cal table %d lkg %d\n", j,
				 * temp);
				 */
				c[i] += (temp *
					 all_tab[j]->tab_raw[i].instance) >>
					10;
				/* SPOWER_INFO("total lkg %d\n", c[i]); */
			}
		}
		SPOWER_INFO("done-->get c=%d\n", c[i]);
		if (wat >= c[i])
			break;
	}

	/** FIXME,
	 * There are only 2 tables are used to interpolate to form SPTAB.
	 * Thus, sptab takes use of the container which raw data is not used
	 *anymore.
	 **/

	if (i == spower_raw->table_size) {
		i = spower_raw->table_size - 1;
/** above all **/
#if defined(EXTER_POLATION)
		tab1 = tab[spower_raw->table_size - 2];
		tab2 = tab[spower_raw->table_size - 1];

		/** occupy the free container**/
		tspt = tab[spower_raw->table_size - 3];
#else  /* #if defined(EXTER_POLATION) */
		tspt = tab1 = tab2 = tab[spower_raw->table_size - 1];
#endif /* #if defined(EXTER_POLATION) */

		SPOWER_INFO("sptab max tab:%d/%d\n", wat, c[i]);
	} else if (i == 0) {
#if defined(EXTER_POLATION)
		/** below all **/
		tab1 = tab[0];
		tab2 = tab[1];

		/** occupy the free container**/
		tspt = tab[2];
#else  /* #if defined(EXTER_POLATION) */
		tspt = tab1 = tab2 = tab[0];
#endif /* #if defined(EXTER_POLATION) */

		SPOWER_INFO("sptab min tab:%d/%d\n", wat, c[i]);
	} else if (wat == c[i]) {
		/** just match **/
		tab1 = tab2 = tab[i];
		/** pointer duplicate  **/
		tspt = tab1;
		SPOWER_INFO("sptab equal to tab:%d/%d\n", wat, c[i]);
	} else {
		/** anyone **/
		tab1 = tab[i - 1];
		tab2 = tab[i];

		/** occupy the free container**/
		tspt = tab[(i + 1) % spower_raw->table_size];
		SPOWER_INFO("sptab interpolate tab:%d/%d, i:%d\n", wat, c[i],
			    i);
	}

	if (wat == 0) {
		/* force mc50 */
		tab1 = tab2 = tab[1];
		tspt = tab1;
		SPOWER_INFO("@@~ force mc50\n");
	}

	/** sptab needs to interpolate 2 tables. **/
	if (tab1 != tab2)
		interpolate_table(tspt, c[i - 1], c[i], wat, tab1, tab2);

	/** update to global data **/
	*spt = *tspt;

	return 0;
}

static int static_power_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "%s", static_power_buf);
	return 0;
}

static int static_power_open(struct inode *inode, struct file *file)
{
	return single_open(file, static_power_show, NULL);
}

static const struct file_operations static_power_operations = {
	.open = static_power_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int static_power_precise_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "%s", static_power_buf_precise);
	return 0;
}

static int static_power_precise_open(struct inode *inode, struct file *file)
{
	return single_open(file, static_power_precise_show, NULL);
}

static const struct file_operations static_power_precise_operations = {
	.open = static_power_precise_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#define PROC_FOPS_RO(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct proc_ops name ## _proc_fops = {	\
		.proc_open		   = name ## _proc_open,		\
		.proc_read		   = seq_read,				\
		.proc_lseek		 = seq_lseek,				\
		.proc_release		= single_release,		\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

static int spower_lkg_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s", static_power_buf);
	return 0;
}
PROC_FOPS_RO(spower_lkg);

int spower_procfs_init(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct proc_ops  *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(spower_lkg),
	};

	dir = proc_mkdir("leakage", NULL);

	if (!dir) {
		pr_notice("fail to create /proc/leakage @ %s()\n",
								__func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create
		    (entries[i].name, 0664, dir, entries[i].fops))
			pr_notice("%s(), create /proc/leakage/%s failed\n",
				__func__, entries[i].name);
	}
	return 0;
}

static const struct of_device_id mtk_static_power_of_match[] = {
	{ .compatible = "mediatek,mtk-static-power", },
	{},
};

static int mt_spower_init(struct platform_device *pdev)
{
	int devinfo = 0;
	unsigned int temp_lkg;
	struct nvmem_device *nvmem_dev;
	int i;
	unsigned int v_of_fuse;
	int t_of_fuse;
	unsigned int idx = 0;
	char *p_buf = static_power_buf;
	char *p_buf_precise = static_power_buf_precise;
	int n_domain = 0;
	struct device_node *node = pdev->dev.of_node;
	const char *domain;
	u32 value[6];
	int ret;
#if SPOWERTABLE
	int *spower_value_0, *spower_value_1, *spower_value_2;
	struct sptab_list **tab;
	const char *spower;
	char cell_name[25];
#endif

	nvmem_dev = nvmem_device_get(&pdev->dev, "mtk_efuse");
	ret = IS_ERR(nvmem_dev);
	if (ret) {
		ret = PTR_ERR(nvmem_dev);
		return ret;
	}

	n_domain = of_property_count_strings(node, "domain");
	spower_lkg_info = kmalloc_array(n_domain, sizeof(struct spower_leakage_info), GFP_KERNEL);
#if SPOWERTABLE
	tab = kmalloc_array(n_domain, sizeof(struct sptab_list *), GFP_KERNEL);
	sptab = kmalloc_array(n_domain, sizeof(struct sptab_s), GFP_KERNEL);
#endif
	spower_raw = kmalloc_array(n_domain, sizeof(struct spower_raw_t), GFP_KERNEL);

	for (i = 0; i < n_domain; i++) {
#if SPOWERTABLE
		tab[i] = kmalloc(sizeof(struct sptab_list), GFP_KERNEL);
#endif
		ret = of_property_read_string_index(node, "domain", i, &domain);
		ret = of_property_read_u32_array(node, domain, value, ARRAY_SIZE(value));
#if SPOWERTABLE
		ret = of_property_read_string_index(node, "spower", i, &spower);
		spower_value_0 = kmalloc(sizeof(int)*(VSIZE*TSIZE+VSIZE+TSIZE), GFP_KERNEL);
		memset(cell_name, '\0', sizeof(cell_name));
		ret = snprintf(cell_name, sizeof(cell_name), "%s_%d", spower, 0);
		ret = of_property_read_u32_array(node, cell_name, spower_value_0,
			VSIZE*TSIZE+VSIZE+TSIZE);

		spower_value_1 = kmalloc(sizeof(int)*(VSIZE*TSIZE+VSIZE+TSIZE), GFP_KERNEL);
		memset(cell_name, '\0', sizeof(cell_name));
		ret = snprintf(cell_name, sizeof(cell_name), "%s_%d", spower, 1);
		ret = of_property_read_u32_array(node, cell_name, spower_value_1,
			VSIZE*TSIZE+VSIZE+TSIZE);

		spower_value_2 = kmalloc(sizeof(int)*(VSIZE*TSIZE+VSIZE+TSIZE), GFP_KERNEL);
		memset(cell_name, '\0', sizeof(cell_name));
		ret = snprintf(cell_name, sizeof(cell_name), "%s_%d", spower, 2);
		ret = of_property_read_u32_array(node, cell_name, spower_value_2,
			VSIZE*TSIZE+VSIZE+TSIZE);
#endif
		spower_raw[i].vsize = VSIZE;
		spower_raw[i].tsize = TSIZE;
		spower_raw[i].table_size = MAX_TABLE_SIZE;
#if SPOWERTABLE
		spower_raw[i].table[0] = spower_value_0;
		spower_raw[i].table[1] = spower_value_1;
		spower_raw[i].table[2] = spower_value_2;
#endif
		spower_raw[i].devinfo_domain = BIT(i);
		spower_raw[i].leakage_id = i;
		spower_raw[i].instance = value[4];
		spower_raw[i].print_leakage = true;
		spower_lkg_info[i].name = domain;
		spower_lkg_info[i].devinfo_offset = value[1];
		spower_lkg_info[i].v_of_fuse = value[2];
		spower_lkg_info[i].t_of_fuse = value[3];
		spower_lkg_info[i].instance = value[4];

		nvmem_device_read(nvmem_dev, value[0], sizeof(__u32), &devinfo);
		temp_lkg = (devinfo & spower_lkg_info[i].devinfo_offset)
			>> find_first_bit((unsigned long *)&spower_lkg_info[i].devinfo_offset, 32);
		if (temp_lkg != 0) {
			temp_lkg = devinfo_table[temp_lkg];
			spower_lkg_info[i].value = temp_lkg * spower_lkg_info[i].v_of_fuse;
		} else
			spower_lkg_info[i].value = 0;
	}

#if SPOWERTABLE
	SPOWER_INFO("spower table construct\n");
	for (i = 0; i < n_domain; i++)
		spower_tab_construct(tab[i]->tab_raw, &spower_raw[i], i);
#endif

	for (idx = 0; idx < n_domain; idx++)  {
		v_of_fuse = spower_lkg_info[idx].v_of_fuse;
		t_of_fuse = spower_lkg_info[idx].t_of_fuse;
#if SPOWERTABLE
		mtk_spower_make_table(&sptab[i], v_of_fuse, t_of_fuse,
				      (unsigned int)i, tab, n_domain);
#endif
		p_buf += sprintf(p_buf, "%d/",
			(spower_lkg_info[idx].value / 1000 /
			spower_lkg_info[idx].instance));
		p_buf_precise += sprintf(p_buf_precise, "%d.%d/",
			DIV_ROUND_CLOSEST(spower_lkg_info[idx].value,
				spower_lkg_info[idx].instance * 100) / 10,
			DIV_ROUND_CLOSEST(spower_lkg_info[idx].value,
					spower_lkg_info[idx].instance *
					100) % 10
		);
	}

	p_buf += sprintf(p_buf, "\n");
	p_buf_precise += sprintf(p_buf_precise, "\n");

	/* print static_power_buf and generate debugfs node */
	SPOWER_ERR("%s", static_power_buf);

	debugfs_create_file("static_power", S_IFREG | 0400, NULL, NULL,
			    &static_power_operations);

	SPOWER_ERR("%s", static_power_buf_precise);

	debugfs_create_file("static_power_precise", S_IFREG | 0400, NULL, NULL,
			    &static_power_precise_operations);

	spower_procfs_init();
#if SPOWERTABLE
	for (i = 0; i < n_domain; i++)
		kfree(tab[i]);
#endif

	return 0;
}

static struct platform_driver mtk_static_power_driver = {
	.remove  = NULL,
	.shutdown   = NULL,
	.probe    = mt_spower_init,
	.suspend        = NULL,
	.resume  = NULL,
	.driver  = {
		.name   = "mtk-static-power",
		.of_match_table = mtk_static_power_of_match,
	},
};

static int __init mtk_static_power_init(void)
{
	int err = 0;

	err = platform_driver_register(&mtk_static_power_driver);
	if (err)
		return err;

	return 0;
}

static void __exit mtk_static_power_exit(void)
{
}

/* return 0, means sptab is not yet ready. */
/* vol unit should be mv */
int mt_spower_get_leakage(int dev, unsigned int vol, int deg)
{
	int ret;

	if (dev < 0 || !tab_validate(&sptab[dev]))
		return 0;

	if (vol > mV(&sptab[dev], VSIZE - 1))
		vol = mV(&sptab[dev], VSIZE - 1);
	else if (vol < mV(&sptab[dev], 0))
		vol = mV(&sptab[dev], 0);

	if (deg > deg(&sptab[dev], TSIZE - 1))
		deg = deg(&sptab[dev], TSIZE - 1);
	else if (deg < deg(&sptab[dev], 0))
		deg = deg(&sptab[dev], 0);

	ret = sptab_lookup(&sptab[dev], (int)vol, deg) >> 10;

	SPOWER_INFO("%s-dev=%d,volt=%d, deg=%d, lkg=%d\n", __func__,
		    dev, vol, deg, ret);
	return ret;
}
EXPORT_SYMBOL(mt_spower_get_leakage);

int mt_spower_get_leakage_uW(int dev, unsigned int vol, int deg)
{
	int ret;

	if (dev < 0 || !tab_validate(&sptab[dev]))
		return 0;

	if (vol > mV(&sptab[dev], VSIZE - 1))
		vol = mV(&sptab[dev], VSIZE - 1);
	else if (vol < mV(&sptab[dev], 0))
		vol = mV(&sptab[dev], 0);

	if (deg > deg(&sptab[dev], TSIZE - 1))
		deg = deg(&sptab[dev], TSIZE - 1);
	else if (deg < deg(&sptab[dev], 0))
		deg = deg(&sptab[dev], 0);

	ret = sptab_lookup(&sptab[dev], (int)vol, deg);

	SPOWER_INFO("%s-dev=%d,volt=%d, deg=%d, lkg=%d\n", __func__,
		    dev, vol, deg, ret);
	return ret;
}
EXPORT_SYMBOL(mt_spower_get_leakage_uW);

int mt_spower_get_efuse_lkg(int dev)
{
	return spower_lkg_info[dev].value / 1000;
}
EXPORT_SYMBOL(mt_spower_get_efuse_lkg);

module_init(mtk_static_power_init);
module_exit(mtk_static_power_exit);
MODULE_DESCRIPTION("MediaTek Leakage Driver");
MODULE_LICENSE("GPL");
