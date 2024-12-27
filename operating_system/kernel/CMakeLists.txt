cmake_minimum_required(VERSION 3.10)

project(Kernel C)

# 设置交叉编译工具链
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR loongarch64)

# 设置交叉编译器路径
set(CMAKE_C_COMPILER ../../cross-tool/bin/loongarch64-unknown-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER ../../cross-tool/bin/loongarch64-unknown-linux-gnu-g++)

# 设置编译选项
set(CMAKE_C_FLAGS "-Wall -Werror -O -fno-omit-frame-pointer -ggdb -MD -march=loongarch64 -mabi=lp64 -ffreestanding -fno-common -nostdlib -Iinclude -fno-stack-protector -fno-pie -no-pie")
set(CMAKE_EXE_LINKER_FLAGS "-z max-page-size=4096 -Ttext 0x9000000000200000")


# 源文件
file(GLOB SOURCES
    init/head.c
    init/main.c
    drv/console.c
    drv/font.c
    excp/exception_handler.c
    excp/exception.c
    mm/memory.c
    mm/buddy_allocator.c
    proc/process.c
    proc/swtch.c
    proc/ipc.c
    drv/disk.c
    fs/xtfs.c
)

# 添加可执行目标
add_executable(kernel ${SOURCES})

# 设置链接器选项
set_target_properties(kernel PROPERTIES LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

# 包含目录
target_include_directories(kernel PRIVATE ${CMAKE_SOURCE_DIR}/include)
