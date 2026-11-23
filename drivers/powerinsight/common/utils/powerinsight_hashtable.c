#include "powerinsight_hashtable.h"

#include "linux/slab.h"

#include "powerinsight_utils.h"

struct powerinsight_dynamic_hashtable *powerinsight_alloc_dynamic_hashtable(uint32_t bits)
{
	size_t size, hash_size;
	struct powerinsight_dynamic_hashtable *htable = NULL;

	if (bits > 20) {
		powerinsight_err("Too big bits: %u", bits);
		return NULL;
	}
	htable = kzalloc(sizeof(struct powerinsight_dynamic_hashtable), GFP_ATOMIC);
	if (!htable)
		return htable;

	htable->bits = bits;
	hash_size = BIT(bits);
	size = sizeof(struct hlist_head) * hash_size;
	htable->head = kzalloc(size, GFP_ATOMIC);
	if (!htable->head) {
		kfree(htable);
		htable = NULL;
		return htable;
	}
	__hash_init(htable->head, hash_size);

	return htable;
}

void powerinsight_free_dynamic_hashtable(struct powerinsight_dynamic_hashtable *htable)
{
	if (!htable)
		return;

	if (htable->head) {
		powerinsight_err("free head");
		kfree(htable->head);
	}
	kfree(htable);
	powerinsight_err("free hashtable");
}
