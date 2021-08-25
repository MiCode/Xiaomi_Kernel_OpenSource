#include <linux/jiffies.h>
#include <linux/pagemap.h>
#include <linux/pagevec.h>


inline unsigned long elapsed_jiffies(unsigned long start)
{
	unsigned long end = jiffies;

	if (end >= start)
		return (unsigned long)(end - start);

	return (unsigned long)(end + (MAX_JIFFY_OFFSET - start) + 1);
}

inline unsigned long filemap_range_nr_page(struct address_space *mapping,
		pgoff_t index, pgoff_t end)
{
	struct page *page;
	int nr_page = 0;

	while (index <= end) {
		if (find_get_pages_range(mapping, &index, index, 1, &page)) {
			nr_page++;
			put_page(page);
		}
		index++;
	}

	return nr_page;
}

inline unsigned long get_dirty_page_count(struct address_space *mapping)
{
	int nr_pages;
	unsigned long nr_dirty_pages = 0;
	pgoff_t index = 0;
	struct pagevec pvec;

	if (NULL == mapping)
		return 0;

	if (0 == mapping->nrpages)
		return 0;

	pagevec_init(&pvec, 0);
	while ((nr_pages = pagevec_lookup_tag(&pvec, mapping, &index,
			PAGECACHE_TAG_DIRTY))) {
		nr_dirty_pages += nr_pages;
		pagevec_release(&pvec);
		cond_resched();
	}

	return nr_dirty_pages;
}

inline bool native_need_limit(void)
{
	int index;
	int item_nums;
	char *filter_items[] = {"init", "dex2oat", "dexopt"};

	item_nums = sizeof(filter_items) / sizeof(filter_items[0]);
	for (index = 0; index < item_nums; index++) {
		if (0 == strncmp(current->comm, filter_items[index], strlen(filter_items[index])))
			return false;
	}
	return true;
}
