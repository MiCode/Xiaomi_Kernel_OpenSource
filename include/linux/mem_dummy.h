#ifndef _MEMORY_DUMMY_H_
#define _MEMORY_DUMMY_H_

#define MEM_DUMMY_VA2PA (0x1)

typedef struct VAtoPA {
	unsigned long va;
	unsigned long pa;
} VAtoPA;

#endif				/* _MEMORY_DUMMY_H_ */
