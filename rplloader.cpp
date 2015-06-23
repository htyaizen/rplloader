#include <string>
#include <vector>
#include <iostream>
#include <cstring>
#include <elf.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

#define PAGE_SIZE (4096)
#define PAGE_MASK (~4095)
#define PAGE_START(x) ((x) & PAGE_MASK)
#define PAGE_OFFSET(x) ((x) & ~PAGE_MASK)
#define PAGE_END(x) (PAGE_START((x) + (PAGE_SIZE - 1)))

class RPLLoader;
class RPLLibrary;

class RPLLibrary {
public:
	RPLLoader*	loader;
	std::string	name;
	int fd;
	Elf32_Ehdr*	ehdr;
	Elf32_Word	base;
	Elf32_Shdr*	shdr;
	void*		load_start;
	bool readElfHeader();
	bool reserveAddressSpace();
};

class RPLLoader {
public:
	std::vector<RPLLibrary*> libs;
	RPLLibrary* open(std::string const& path);
	void close(RPLLibrary* handle);
	void reportErr(const char* err);
};

RPLLibrary* RPLLoader::open(std::string const& path) {
	int fd = ::open(path.c_str(), O_RDONLY);
	if (fd == -1) {
		reportErr(::strerror(errno));
		return nullptr;
	}
	RPLLibrary* lib = new RPLLibrary;
	lib->name = path;
	lib->loader = this;
	lib->fd = fd;
	lib->load_start = nullptr;
	if (!lib->readElfHeader()) goto fail;
	if (!lib->reserveAddressSpace()) goto fail;
	
	return lib;
fail:
	delete lib;
	return nullptr;
}

void RPLLoader::reportErr(const char* err) {
	std::cerr << err << std::endl;
}

void RPLLoader::close(RPLLibrary* handle) {
	// todo: close
}

bool RPLLibrary::readElfHeader() {
	void* ehdr_map = ::mmap(nullptr, PAGE_END(sizeof(Elf32_Phdr)),
		PROT_READ, MAP_PRIVATE, fd, 0);
	if (ehdr_map == MAP_FAILED) return false;
	ehdr = (Elf32_Ehdr*) ehdr_map;
	// map the section headers
	// from crazy linker
	Elf32_Addr page_min = PAGE_START(ehdr->e_shoff);
	Elf32_Addr page_max = PAGE_END(ehdr->e_shoff + 
		(ehdr->e_shnum * sizeof(Elf32_Shdr)));
	Elf32_Addr page_offset = PAGE_OFFSET(ehdr->e_shoff);
	void* shdr_map = ::mmap(nullptr, page_max - page_min,
		PROT_READ, MAP_PRIVATE, fd, page_min);
	if (shdr_map == MAP_FAILED) return false;
	shdr = (Elf32_Shdr*) (((uintptr_t) shdr_map) + page_offset);
	return true;
}

bool RPLLibrary::reserveAddressSpace() {
}



int main(int argc, char** argv) {
	RPLLoader loader;
	RPLLibrary* handle = loader.open(argv[1]);
	loader.close(handle);
	return 0;
}
