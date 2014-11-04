#ifndef __TAG_GLOBAL_H_INCLUDED__
#define __TAG_GLOBAL_H_INCLUDED__

/* Data structure containing the tagging information which is used in
 * continuous mode to specify which frames should be captured.
 * num_captures		The number of RAW frames to be processed to
 *                      YUV. Setting this to -1 will make continuous
 *                      capture run until it is stopped.
 * skip			Skip N frames in between captures. This can be
 *                      used to select a slower capture frame rate than
 *                      the sensor output frame rate.
 * offset		Start the RAW-to-YUV processing at RAW buffer
 *                      with this offset. This allows the user to
 *                      process RAW frames that were captured in the
 *                      past or future.
 * exp_id		Exposure id of the RAW frame to tag.
 */
struct sh_css_tag_descr {
	int num_captures;
	unsigned int num_captures_sign;
	unsigned int skip;
	int offset;
	unsigned int offset_sign;
	unsigned int exp_id;
};

#endif /* __TAG_GLOBAL_H_INCLUDED__ */
