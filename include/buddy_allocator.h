#ifndef BUDDY_ALLOCATOR_H
#define BUDDY_ALLOCATOR_H

#include "xtos.h"

extern page_t mem_map[NR_PAGE];

void buddy_init();
page_t *buddy_alloc(int order);
void buddy_free(page_t *page, int order);
void buddy_test();

#endif // BUDDY_ALLOCATOR_H
