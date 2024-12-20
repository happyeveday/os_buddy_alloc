#include <xtos.h>
#include <stddef.h>  

#define CSR_BADV 0x7
#define CSR_PWCL 0x1c
#define CSR_DMW0 0x180
#define CSR_DMW3 0x183
#define CSR_DMW0_PLV0 (1UL << 0)
#define MEMORY_SIZE 0x8000000
#define NR_PAGE (MEMORY_SIZE >> 12)
#define KERNEL_START_PAGE (0x200000UL >> 12)
#define KERNEL_END_PAGE (0x300000UL >> 12)
#define ENTRY_SIZE 8
#define PWCL_PTBASE 12
#define PWCL_PTWIDTH 9
#define PWCL_PDBASE 21
#define PWCL_PDWIDTH 9
#define PWCL_EWIDTH 0
#define ENTRYS 512

#define MAX_ORDER 12          // 最大块的阶数
#define N_PAGE 32768          // 128MB的物理内存，共有32768个物理页
#define PAGE_SIZE 4096        // 每页大小为4KB
#define MEM_SIZE (N_PAGE * PAGE_SIZE)  // 总内存大小

// mem_map数组的定义
char mem_map[N_PAGE];  // 每个物理页的状态，0为空闲，1为已占用

// 伙伴系统的结构定义
typedef struct free_block {
    unsigned long order;         // 块的阶数，表示块的大小是2^order
    struct free_block *next;     // 下一个空闲块
} free_block_t;

// 用一个数组记录每个阶数的空闲链表
free_block_t *free_list[MAX_ORDER + 1];  // free_list[0]表示1页块的空闲链表，free_list[1]表示2页块的空闲链表，以此类推

extern struct process *current;
extern struct shmem shmem_table[NR_SHMEM];

// 初始化伙伴系统
void buddy_system_init() {
    for (int i = 0; i <= MAX_ORDER; i++) {
        free_list[i] = NULL;  // 初始化所有阶数的空闲链表为空
    }

    // 初始时，整个内存作为一个大的空闲块
    free_block_t *block = (free_block_t *)get_page();
    block->order = MAX_ORDER; // 整个内存块大小为2^12页
    block->next = NULL;

    // 将这个大块添加到最大阶数的空闲链表
    free_list[MAX_ORDER] = block;
}

// 分配内存
void *buddy_alloc(unsigned long order) {
    if (order > MAX_ORDER) {
        panic("Request order exceeds MAX_ORDER.");
    }

    // 从空闲链表中查找合适大小的块
    for (int i = order; i <= MAX_ORDER; i++) {
        if (free_list[i] != NULL) {
            // 找到一个足够大的空闲块
            free_block_t *block = free_list[i];
            free_list[i] = block->next; // 从链表中移除

            // 如果该块大于请求大小，则拆分
            while (i > order) {
                // 拆分成两个更小的块
                free_block_t *buddy = (free_block_t *)((char *)block + (1 << (i - 1)) * PAGE_SIZE);
                buddy->order = i - 1;
                buddy->next = free_list[i - 1];
                free_list[i - 1] = buddy;

                i--;
            }

            // 返回分配的块的地址
            return (void *)block;
        }
    }

    panic("Out of memory in buddy allocator.");
    return NULL;
}

// 释放内存
void buddy_free(void *ptr, unsigned long order) {
    free_block_t *block = (free_block_t *)ptr;
    block->order = order;

    // 获取块的起始地址
    unsigned long block_start = (unsigned long)block & ~(PAGE_SIZE * (1 << order) - 1);

    // 查找块的伙伴
    unsigned long buddy_start = block_start ^ (PAGE_SIZE * (1 << order));
    free_block_t *buddy = (free_block_t *)buddy_start;

    // 合并伙伴
    if ((mem_map[buddy_start >> 12] == 0) && (buddy->order == order)) {
        // 伙伴为空闲且阶数相同，合并
        free_block_t **list = &free_list[order];
        while (*list != NULL && *list != buddy) {
            list = &(*list)->next;
        }

        // 移除伙伴
        if (*list == buddy) {
            *list = buddy->next;
        }

        // 合并后插入到当前阶数的空闲链表
        if (block_start < buddy_start) {
            block->order = order + 1;
            block->next = free_list[order + 1];
            free_list[order + 1] = block;
        } else {
            buddy->order = order + 1;
            buddy->next = free_list[order + 1];
            free_list[order + 1] = buddy;
        }
    } else {
        // 伙伴不可合并，直接将块加入空闲链表
        block->next = free_list[order];
        free_list[order] = block;
    }
}

// 获取一页内存（使用伙伴系统）
unsigned long get_page() {
    void *page = buddy_alloc(0);  // 请求分配一个页（阶数为0的块）
    return (unsigned long)page;
}

// 释放一页内存（使用伙伴系统）
void free_page(unsigned long page) {
    buddy_free((void *)page, 0);  // 释放一个页（阶数为0的块）
}

// 初始化内存管理
void mem_init() {
    int i;

    // 初始化mem_map数组
    for (i = 0; i < N_PAGE; i++) {
        if (i >= KERNEL_START_PAGE && i < KERNEL_END_PAGE) {
            mem_map[i] = 1;  // 标记内核区为已占用
        } else {
            mem_map[i] = 0;  // 标记用户区为空闲
        }
    }

    // 初始化伙伴系统
    buddy_system_init();

    // 设置控制寄存器
    write_csr_64(CSR_DMW0_PLV0 | DMW_MASK, CSR_DMW0);
    write_csr_64(0, CSR_DMW3);
    write_csr_64((PWCL_EWIDTH << 30) | (PWCL_PDWIDTH << 15) | (PWCL_PDBASE << 10) | (PWCL_PTWIDTH << 5) | (PWCL_PTBASE << 0), CSR_PWCL);
    invalidate();

    // 初始化共享内存
    shmem_init();
}

// 获取页表项
unsigned long *get_pte(struct process *p, unsigned long u_vaddr) {
    unsigned long pd, pt;
    unsigned long *pde, *pte;

    pd = p->page_directory;
    pde = (unsigned long *)(pd + ((u_vaddr >> 21) & 0x1ff) * ENTRY_SIZE);
    if (*pde)
        pt = *pde | DMW_MASK;
    else {
        pt = get_page();
        *pde = pt & ~DMW_MASK;
    }
    pte = (unsigned long *)(pt + ((u_vaddr >> 12) & 0x1ff) * ENTRY_SIZE);
    return pte;
}

// 设置页表项
void put_page(struct process *p, unsigned long u_vaddr, unsigned long k_vaddr, unsigned long attr) {
    unsigned long *pte;

    pte = get_pte(p, u_vaddr);
    if (*pte)
        panic("panic: try to remap!");
    *pte = (k_vaddr & ~DMW_MASK) | attr;
    invalidate();
}

// 释放进程的页表
void free_page_table(struct process *p) {
    unsigned long pd, pt;
    unsigned long *pde, *pte;
    unsigned long page;

    pd = p->page_directory;
    pde = (unsigned long *)pd;
    for (int i = 0; i < ENTRYS; i++, pde++) {
        if (*pde == 0)
            continue;
        pt = *pde | DMW_MASK;
        pte = (unsigned long *)pt;
        for (int j = 0; j < ENTRYS; j++, pte++) {
            if (*pte == 0)
                continue;
            page = (~0xfffUL & *pte) | DMW_MASK;
            free_page(page);
            *pte = 0;
        }
        free_page(*pde | DMW_MASK);
        *pde = 0;
    }
}

// 复制页表
void copy_page_table(struct process *from, struct process *to) {
    unsigned long from_pd, to_pd, from_pt, to_pt;
    unsigned long *from_pde, *to_pde, *from_pte, *to_pte;
    unsigned long page;
    int i, j;

    from_pd = from->page_directory;
    from_pde = (unsigned long *)from_pd;
    to_pd = to->page_directory;
    to_pde = (unsigned long *)to_pd;
    for (i = 0; i < ENTRYS; i++, from_pde++, to_pde++) {
        if (*from_pde == 0)
            continue;
        from_pt = *from_pde | DMW_MASK;
        from_pte = (unsigned long *)from_pt;
        to_pt = get_page();
        to_pte = (unsigned long *)to_pt;
        *to_pde = to_pt | DMW_MASK;
        for (j = 0; j < ENTRYS; j++, from_pte++, to_pte++) {
            if (*from_pte == 0)
                continue;
            page = (~0xfffUL & *from_pte) | DMW_MASK;
            *to_pte = page;
        }
    }
}

void share_page(unsigned long page)
{
	unsigned long i;

	i = (page & ~DMW_MASK) >> 12;
	if (!mem_map[i])
		panic("panic: try to share free page!\n");
	mem_map[i]++;
}
int is_share_page(unsigned long page)
{
	unsigned long i;

	i = (page & ~DMW_MASK) >> 12;
	if (mem_map[i] > 1)
		return 1;
	else
		return 0;
}

void do_wp_page()
{
	unsigned long *pte;
	unsigned long u_vaddr;
	unsigned long old_page, new_page;

	u_vaddr = read_csr_64(CSR_BADV);
	pte = get_pte(current, u_vaddr);
	old_page = (~0xfff & *pte) | DMW_MASK;
	if (is_share_page(old_page))
	{
		new_page = get_page();
		*pte = (new_page & ~DMW_MASK) | PTE_PLV | PTE_D | PTE_V;
		copy_mem((char *)new_page, (char *)old_page, PAGE_SIZE);
		free_page(old_page);
	}
	else
		*pte |= PTE_D;
	invalidate();
}
void do_no_page()
{
	unsigned long page;
	unsigned long u_vaddr;

	u_vaddr = read_csr_64(CSR_BADV);
	u_vaddr &= ~0xfffUL;
	page = get_page();
	if (u_vaddr < current->exe_end)
		get_exe_page(u_vaddr, page);
	put_page(current, u_vaddr, page, PTE_PLV | PTE_D | PTE_V);
}
