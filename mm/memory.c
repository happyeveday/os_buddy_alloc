#include <stddef.h>
#include "buddy_allocator.h"  // 引入伙伴系统的头文件
#include "xtos.h"

// 替换get_page函数，使用伙伴系统分配页
unsigned long get_page() {
    page_t *page = buddy_alloc(0); // 分配一个4KB页
    if (page == NULL) {
        panic("panic: out of memory!\n");
        return 0;
    }
    return ((unsigned long)(page - mem_map)) * PAGE_SIZE;
}

// 替换free_page函数，使用伙伴系统释放页
void free_page(unsigned long addr) {
    int page_index = addr / PAGE_SIZE;
    buddy_free(&mem_map[page_index], 0);
}

// 初始化内存管理，增加伙伴系统初始化
void mem_init() {
    buddy_init(); // 初始化伙伴系统

    buddy_test(); // 测试伙伴系统

    // 设置核心保留区
    for (int i = 0; i < NR_PAGE; i++) {
        if (i >= KERNEL_START_PAGE && i < KERNEL_END_PAGE)
            mem_map[i].flags = 1; // 标记内核内存区域
        else
            mem_map[i].flags = 0; // 标记为空闲
    }

    write_csr_64(CSR_DMW0_PLV0 | DMW_MASK, CSR_DMW0);
    write_csr_64(0, CSR_DMW3);
    write_csr_64((PWCL_EWIDTH << 30) | (PWCL_PDWIDTH << 15) | (PWCL_PDBASE << 10) | (PWCL_PTWIDTH << 5) | (PWCL_PTBASE << 0), CSR_PWCL);
    invalidate();

    shmem_init(); // 共享内存初始化
}
