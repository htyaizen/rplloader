Linux上的RPL库加载程序。

大部分来自Chromium / Android NDK的CrazyLinker。

这是一个非常简单的32位ELF库的加载程序，它支持RPL格式使用的压缩部分，忽略程序头，有利于section头部（RPL没有）。一个用于PowerPC交叉编译，并以QEMU用户模式运行，以调试RPL二进制文件。

它用C ++编写，使用一些C ++ 11功能，并使用CMake编译。

用于或者用于加载（但不应用重定位或执行）其他32位ELF库，如i386。

目前实现：

读可执行文件头
读section头部
储备地址空间
使用标准文件IO将部分读入内存

预计：

 - 解压section
 - 尽可能将section映射到mmap内存
 - 适用重定位和引用
 - 对RPL导入/导出表执行某些操作（目前的计划是忽略它）
 - 提供dlsym
