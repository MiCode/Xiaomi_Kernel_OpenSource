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
unsigned long int_sqrt(unsigned long x)
{
	unsigned long tmp;
	unsigned long place;
	unsigned long root;
	unsigned long remainder;

	if (x <= 1)
		return x;

	root = 0;
	remainder = x;
	place = 1UL << (BITS_PER_LONG - 2);
	
	while (place > remainder)  
		place >>= 2;

	while (place != 0) {
		tmp = root + place;

		if (remainder >= tmp) 
		{
			remainder -= tmp;
			root += (place << 1);
		}
		root >>= 1;
		place >>= 2;
	}

	return root;
}
EXPORT_SYMBOL(int_sqrt);
