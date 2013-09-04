/*
 * Copyright (C) 2013 Davidlohr Bueso <davidlohr.bueso@hp.com>
 *
 *  Based on the shift-and-subtract algorithm for computing integer
 *  square root from Guy L. Steele.
 */

#include <linux/kernel.h>
#include <linux/export.h>

/**
 * int_sqrt - rough approximation to sqrt
 * @x: integer of which to calculate the sqrt
 *
 * A very rough approximation to the sqrt() function.
 */
inline unsigned long int_sqrt(unsigned long x)
{
	register unsigned long tmp;
	register unsigned long place;
	register unsigned long root = 0;

	if (x <= 1)
		return x;

	place = 1UL << (BITS_PER_LONG - 2);

	do{
		place >>= 2;
	}while(place > x);

	do {
		tmp = root + place;
		root >>= 1;

		if (x >= tmp)
		{
			x -= tmp;
			root += place;
		}
		place >>= 2;
	}while (place != 0);

	return root;
}
EXPORT_SYMBOL(int_sqrt);
