/* linker.ld */

/* 定义程序入口点 */
ENTRY(_start)

/* 内存布局 */
MEMORY
{
    TEXT (rx) : ORIGIN = 0x9000000000200000, LENGTH = 256M
    DATA (rw) : ORIGIN = TEXT + LENGTH(TEXT), LENGTH = 256M
}

/* 段布局和符号定义 */
SECTIONS
{
    /* 将所有的 `.text` 段内容放在内存的 TEXT 区域 */
    .text :
    {
        *(.text*)       /* 所有目标文件中的 .text 段 */
        *(.rodata*)     /* 只读数据 */
    } > TEXT

    /* 将所有的 `.data` 段内容放在内存的 DATA 区域 */
    .data :
    {
        *(.data*)       /* 所有目标文件中的 .data 段 */
    } > DATA

    /* 将所有的 `.bss` 段内容放在内存的 DATA 区域 */
    .bss :
    {
        *(.bss*)         /* 所有目标文件中的 .bss 段 */
    } > DATA

    /* 指定未使用的段内容 */
    /DISCARD/ :
    {
        *(.note*)
        *(.comment*)
    }
}

