#ifndef POWERINSIGHT_HASHTABLE_H
#define POWERINSIGHT_HASHTABLE_H

#include <linux/types.h>
#include <linux/hashtable.h>

#define DECLARE_POWERINSIGHT_HASHTABLE(htable, bits)		DECLARE_HASHTABLE(htable, bits)
#define powerinsight_hash_init(htable)					hash_init(htable)
#define powerinsight_hash_add(htable, node, key)			hash_add(htable, node, key)
#define powerinsight_hash_for_each(htable, bkt, obj, member)		hash_for_each(htable, bkt, obj, member)
#define powerinsight_hash_for_each_safe(htable, bkt, tmp, obj, member)	hash_for_each_safe(htable, bkt, tmp, obj, member)
#define powerinsight_hash_for_each_possible(htable, obj, member, key)	hash_for_each_possible(htable, obj, member, key)

#define POWERINSIGHT_DYNAMIC_HASH_BITS(htable)		((htable)->bits)
#define POWERINSIGHT_DYNAMIC_HASH_SIZE(htable)		BIT(POWERINSIGHT_DYNAMIC_HASH_BITS(htable))
#define powerinsight_dynamic_hash_empty(htable)	__hash_empty((htable)->head, POWERINSIGHT_DYNAMIC_HASH_SIZE(htable))
#define powerinsight_dynamic_hash_add(htable, node, key) \
	hlist_add_head(node, &(htable)->head[hash_min(key, POWERINSIGHT_DYNAMIC_HASH_BITS(htable))])
#define powerinsight_dynamic_hash_for_each(htable, bkt, obj, member) \
	for ((bkt) = 0, (obj) = NULL; (obj) == NULL && (bkt) < POWERINSIGHT_DYNAMIC_HASH_SIZE(htable); (bkt)++) \
		hlist_for_each_entry((obj), &(htable)->head[bkt], member)
#define powerinsight_dynamic_hash_for_each_safe(htable, bkt, tmp, obj, member) \
	for ((bkt) = 0, (obj) = NULL; (obj) == NULL && (bkt) < POWERINSIGHT_DYNAMIC_HASH_SIZE(htable); (bkt)++) \
		hlist_for_each_entry_safe((obj), (tmp), &(htable)->head[bkt], member)
#define powerinsight_dynamic_hash_for_each_possible(htable, obj, member, key) \
	hlist_for_each_entry((obj), &(htable)->head[hash_min(key, POWERINSIGHT_DYNAMIC_HASH_BITS(htable))], member)

struct powerinsight_dynamic_hashtable {
	uint32_t bits;
	struct hlist_head *head;
};

struct powerinsight_dynamic_hashtable *powerinsight_alloc_dynamic_hashtable(uint32_t bits);
void powerinsight_free_dynamic_hashtable(struct powerinsight_dynamic_hashtable *htable);

#endif // POWERINSIGHT_HASHTABLE_H
