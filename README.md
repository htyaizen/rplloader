Loader for RPL libraries on Linux.

Large part taken/inspired from Chromium/Android NDK's CrazyLinker.

This is intended to be a very simple loader of 32-bit ELF libraries that supports the compressed sections used in the RPL format, and ignore program headers in favour of section headers (RPL doesn't have any). One would cross-compile it for PowerPC and run in QEMU user mode to debug RPL binaries.

It's written in C++, uses some C++11 features, and uses CMake to compile.

For testing, this can also be used to load (but not apply relocations or execute, of course) other 32-bit ELF libraries, such as i386 ones.

Currently implemented:

- Read executable header
- Read section headers

Todo:

- Reserve address space
- Map/decompress sections into memory
- apply relocations and imports
- provide dlsym
