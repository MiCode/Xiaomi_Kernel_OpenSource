#include "fusb30x_global.h"

struct fusb30x_chip* g_chip = NULL;  // Our driver's relevant data

struct fusb30x_chip* fusb30x_GetChip(void)
{
    return g_chip;      // return a pointer to our structs
}

void fusb30x_SetChip(struct fusb30x_chip* newChip)
{
    g_chip = newChip;   // assign the pointer to our struct
}