#include "tag.h"

#ifndef __INLINE_QUEUE__
#include "queue_private.h"
#endif /* __INLINE_QUEUE__ */

/**
 * @brief	Creates the tag description from the given parameters.
 * @param[in]	num_captures
 * @param[in]	skip
 * @param[in]	offset
 * @param[out]	tag_descr
 */
void
sh_css_create_tag_descr(int num_captures,
			unsigned int skip,
			int offset,
			unsigned int exp_id,
			struct sh_css_tag_descr *tag_descr)
{
	tag_descr->num_captures = num_captures;
	tag_descr->skip		= skip;
	tag_descr->offset	= offset;
	tag_descr->exp_id	= exp_id;
	tag_descr->num_captures_sign = (num_captures < 0) ? 1 : 0;
	tag_descr->offset_sign = (offset < 0) ? 1 : 0;
}

/**
 * @brief	Encodes the members of tag description into a 32-bit value.
 * @param[in]	tag		Pointer to the tag description
 * @return	(unsigned int)	Encoded 32-bit tag-info
 */
unsigned int
sh_css_encode_tag_descr(struct sh_css_tag_descr *tag)
{
	int num_captures = (tag->num_captures < 0) ? (-tag->num_captures) : (tag->num_captures);
	unsigned int skip = tag->skip;
	int offset = (tag->offset < 0) ? (-tag->offset) : (tag->offset);
	unsigned int exp_id = tag->exp_id;
	unsigned int num_captures_sign = tag->num_captures_sign;
	unsigned int offset_sign = tag->offset_sign;

	unsigned int encoded_tag = (num_captures_sign & 0x00000001)
				|  (offset_sign  & 0x00000001) << 1
				|  (num_captures & 0x000000FF) << 4
				|  (exp_id       & 0x0000000F) << 12
				|  (skip         & 0x000000FF) << 16
				|  (offset       & 0x000000FF) << 24;

	return encoded_tag;
}

