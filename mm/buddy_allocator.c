#include "buddy_allocator.h"

#define MAX_ORDER 15  // 最大订单
#define PAGE_SIZE 4096
#define NR_PAGE (128 * 1024 * 1024 / PAGE_SIZE) // 128MB内存划分为4KB页
page_t mem_map[NR_PAGE];  // 定义 mem_map 数组

typedef struct free_area {
    struct page *free_list;
    unsigned long nr_free;
} free_area_t;

free_area_t free_area[MAX_ORDER + 1];

void buddy_test() {
    printk("Starting buddy system test...\n");

    // 测试分配一个4KB的页
    page_t *page1 = buddy_alloc(0);
    if (page1 != NULL) {
        printk("Test 1: Successfully allocated 4KB page\n");
    } else {
        printk("Test 1: Failed to allocate 4KB page\n");
    }

    // 测试释放该页
    buddy_free(page1, 0);
    printk("Test 2: Successfully freed 4KB page at\n");

    // 测试分配8KB块（order为1）
    page_t *page2 = buddy_alloc(1);
    if (page2 != NULL) {
        printk("Test 3: Successfully allocated 8KB page at\n");
    } else {
        printk("Test 3: Failed to allocate 8KB page\n");
    }

    // 释放8KB块
    buddy_free(page2, 1);
    printk("Test 4: Successfully freed 8KB page at\n");

    // 测试连续分配多个页面并释放，验证合并是否正常
    page_t *pages[4];
    for (int i = 0; i < 4; i++) {
        pages[i] = buddy_alloc(0);
        printk("Test 5: Allocated 4KB page\n");
    }
    for (int i = 0; i < 4; i++) {
        buddy_free(pages[i], 0);
        printk("Test 6: Freed 4KB page\n");
    }

    printk("Buddy system test completed.\n");
}

// 伙伴系统初始化
void buddy_init() {
    memset(mem_map, 0, sizeof(mem_map));
    for (int order = 0; order <= MAX_ORDER; order++) {
        free_area[order].free_list = NULL;
        free_area[order].nr_free = 0;
    }

    int start_page = KERNEL_END_PAGE;
    for (int i = start_page; i < NR_PAGE; i++) {
        mem_map[i].next = free_area[MAX_ORDER].free_list;
        if (free_area[MAX_ORDER].free_list) {
            free_area[MAX_ORDER].free_list->prev = &mem_map[i];
        }
        free_area[MAX_ORDER].free_list = &mem_map[i];
        free_area[MAX_ORDER].nr_free++;
    }
}

// 分配内存块
page_t *buddy_alloc(int order) {
    for (int current_order = order; current_order <= MAX_ORDER; current_order++) {
        if (free_area[current_order].nr_free > 0) {
            page_t *page = free_area[current_order].free_list;
            free_area[current_order].free_list = page->next;
            if (page->next) page->next->prev = NULL;
            free_area[current_order].nr_free--;

            while (current_order > order) {
                current_order--;
                page_t *buddy = page + (1 << current_order);
                buddy->next = free_area[current_order].free_list;
                free_area[current_order].free_list = buddy;
                free_area[current_order].nr_free++;
            }

            page->flags |= 1; // 标记为已分配
            return page;
        }
    }
    return NULL;
}

// 释放内存块
void buddy_free(page_t *page, int order) {
    page->flags &= ~1;

    while (order < MAX_ORDER) {
        page_t *buddy = mem_map + ((page - mem_map) ^ (1 << order));

        if (buddy->flags & 1 || buddy->flags != order) {
            break;
        }

        if (buddy->prev) buddy->prev->next = buddy->next;
        if (buddy->next) buddy->next->prev = buddy->prev;
        if (free_area[order].free_list == buddy) {
            free_area[order].free_list = buddy->next;
        }
        free_area[order].nr_free--;

        if (buddy < page) page = buddy;

        order++;
    }

    page->next = free_area[order].free_list;
    if (free_area[order].free_list) {
        free_area[order].free_list->prev = page;
    }
    free_area[order].free_list = page;
    free_area[order].nr_free++;
}
