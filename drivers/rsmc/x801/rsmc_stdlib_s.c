
#include "rsmc_stdlib_s.h"
#include <linux/string.h>

/*
 * __SIZE_MAX__ value is depending on platform:
 * Firmware Dongle: RAMSIZE (Dongle Specific Limit).
 * LINUX NIC/Windows/MACOSX/Application: OS Native or
 * 0xFFFFFFFFu if not defined.
 */
#ifndef SIZE_MAX
#ifndef __SIZE_MAX__
#define __SIZE_MAX__ 0xFFFFFFFFu
#endif /* __SIZE_MAX__ */
#define SIZE_MAX __SIZE_MAX__
#endif /* SIZE_MAX */
#define RSIZE_MAX (SIZE_MAX >> 1u)

/*
 * memcpy_s - secure memcpy
 * dest : pointer to the object to copy to
 * destsz : size of the destination buffer
 * src : pointer to the object to copy from
 * n : number of bytes to copy
 * Return Value : zero on success and non-zero on error
 * Also on error, if dest is not a null pointer and destsz not greater
 * than RSIZE_MAX, writes destsz zero bytes into the dest object.
 */
int
memcpy_s(void *dest, size_t destsz, const void *src, size_t n)
{
	int err = EOK;
	char *d = dest;
	const char *s = src;

	if ((!d) || ((d + destsz) < d)) {
		err = EBADARG;
		goto exit;
	}

	if (destsz > RSIZE_MAX) {
		err = EBADLEN;
		goto exit;
	}

	if (destsz < n) {
		memset(dest, 0, destsz);
		err = EBADLEN;
		goto exit;
	}

	if ((!s) || ((s + n) < s)) {
		memset(dest, 0, destsz);
		err = EBADARG;
		goto exit;
	}

	/* overlap checking between dest and src */
	if (!(((d + destsz) <= s) || (d >= (s + n)))) {
		memset(dest, 0, destsz);
		err = EBADARG;
		goto exit;
	}

	//(void)memcpy(dest, src, n);
	memcpy(dest, src, n);
exit:
	return err;
}

/*
 * memset_s - secure memset
 * dest : pointer to the object to be set
 * destsz : size of the destination buffer
 * c : byte value
 * n : number of bytes to be set
 * Return Value : zero on success and non-zero on error
 * Also on error, if dest is not a null pointer and destsz not greater
 * than RSIZE_MAX, writes destsz bytes with value c into the dest object.
 */
int
memset_s(void *dest, size_t destsz, int c, size_t n)
{
	int err = EOK;
	if ((!dest) || (((char *)dest + destsz) < (char *)dest)) {
		err = EBADARG;
		goto exit;
	}

	if (destsz > RSIZE_MAX) {
		err = EBADLEN;
		goto exit;
	}

	if (destsz < n) {
		(void)memset(dest, c, destsz);
		err = EBADLEN;
		goto exit;
	}

	(void)memset(dest, c, n);
exit:
	return err;
}
