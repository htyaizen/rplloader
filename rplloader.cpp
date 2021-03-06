#include <string>
#include <vector>
#include <iostream>
#include <cstring>
#include <elf.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

#include <zlib.h>

#define PAGE_SIZE (4096)
#define PAGE_MASK (~4095)
#define PAGE_START(x) ((x) & PAGE_MASK)
#define PAGE_OFFSET(x) ((x) & ~PAGE_MASK)
#define PAGE_END(x) (PAGE_START((x) + (PAGE_SIZE - 1)))
// RPL has import/export sections here; ignore them when loading
// We do not resolve with export IDs, but with symbol table entries
#define VIRTUAL_SECTION_ADDR (0xc0000000)
#define SHF_RPL_COMPRESSED (0x08000000)

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
	bool mapSections();
	bool mapCompressedSection(Elf32_Shdr* s);
	uint32_t getSectionUncompressedSize(Elf32_Shdr* s);
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
	if (!lib->mapSections()) goto fail;
	
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

static bool SectionIsAlloc(Elf32_Shdr* s) {
	return (s->sh_flags & SHF_ALLOC) &&
		(s->sh_addr < VIRTUAL_SECTION_ADDR);
}

uint32_t RPLLibrary::getSectionUncompressedSize(Elf32_Shdr* s) {
	if (!(s->sh_flags & SHF_RPL_COMPRESSED)) return s->sh_size;
	uint32_t size = 0;
	lseek(fd, s->sh_offset, SEEK_SET);
	read(fd, &size, sizeof(size));
	fprintf(stderr, "size: %x\n", size);
	return size;
}
	

bool RPLLibrary::reserveAddressSpace() {
	Elf32_Addr min_addr = shdr[0].sh_addr;
	Elf32_Addr max_addr = min_addr + getSectionUncompressedSize(&shdr[0]);
	for (int i = 1; i < ehdr->e_shnum; i++) {
		Elf32_Shdr* s = &shdr[i];
		if (!SectionIsAlloc(s)) continue;
		if (s->sh_addr < min_addr) min_addr = s->sh_addr;
		if (s->sh_addr + getSectionUncompressedSize(s) > max_addr) {
			max_addr = s->sh_addr + getSectionUncompressedSize(s);
		}
	}
	size_t mysize = PAGE_END(max_addr) - PAGE_START(min_addr);
	load_start = mmap(load_start, mysize,
		PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (load_start == MAP_FAILED) return false;
	return true;
}

bool RPLLibrary::mapSections() {
	// mmap is for smart people
	for (int i = 0; i < ehdr->e_shnum; i++) {
		Elf32_Shdr* s = &shdr[i];
		if (!SectionIsAlloc(s)) continue;
		if (s->sh_type == SHT_NOBITS) continue;
		if (s->sh_flags & SHF_RPL_COMPRESSED) {
			if (!mapCompressedSection(s)) return false;
			continue;
		}
		void* start_addr = (void*)
			(((uintptr_t) load_start) + s->sh_addr);
		off_t seekoff = lseek(fd, s->sh_offset, SEEK_SET);
		if (seekoff == -1) {
			return false;
		}
		ssize_t readresult = read(fd, start_addr, s->sh_size);
		if (readresult == -1) {
			return false;
		}
	}
	return true;
}

bool RPLLibrary::mapCompressedSection(Elf32_Shdr* s) {
	void* start_addr = (void*)
		(((uintptr_t) load_start) + s->sh_addr);
	off_t seekoff = lseek(fd, s->sh_offset + sizeof(uint32_t), SEEK_SET);
	if (seekoff == -1) {
		return false;
	}
	uint32_t compressed_size = s->sh_size - sizeof(uint32_t);
	void* buf = malloc(compressed_size);
	ssize_t readresult = read(fd, buf, compressed_size);
	if (readresult == -1) {
		free(buf);
		return false;
	}
	uLongf actualDecompressedSize = getSectionUncompressedSize(s);
	int unzip = uncompress((Bytef*) start_addr,
		&actualDecompressedSize,
		(Bytef*) buf, compressed_size);
	free(buf);
	if (unzip != Z_OK) {
		return false;
	}
	return true;
}

int main(int argc, char** argv) {
	RPLLoader loader;
	RPLLibrary* handle = loader.open(argv[1]);
	if (handle == nullptr) {
		std::cerr << "failed to load" << std::endl;
		return 0;
	}
	loader.close(handle);
	return 0;
}
