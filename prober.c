// SPDX-License-Identifier: GPL-2.0
/* Copyright 2019 Linux Kernel Contributers
 * Copyright 2020 Joshua Bakita
 *
 * Make many uncached reads and writes to addresses with only a specific
 * bit set to enable a logic-analyzer-based reverse engineering of the DRAM
 * bank mapping function.
 *
 * Based on:
 * Example of using hugepage memory in a user application using the mmap
 * system call with MAP_HUGETLB flag.  Before running this program make
 * sure the administrator has allocated enough default sized huge pages
 * to cover the 2 MB allocation.
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdint.h>
#include <math.h>

#ifndef MAP_HUGETLB
#define MAP_HUGETLB 0x40000 /* arch specific */
#endif

#ifndef MAP_HUGE_SHIFT
#define MAP_HUGE_SHIFT 26
#endif

#ifndef MAP_HUGE_MASK
#define MAP_HUGE_MASK 0x3f
#endif

#define ITERATIONS 1000*1000*20lu

void read_at_offset(int offset, volatile char* memblock) {
	fprintf(stdout, "Reading 20M times from offset %d (bit #%d)\n", offset, offset+1);
	for (unsigned long i = 0; i < ITERATIONS; i++) {
		volatile char* addr = memblock + (1 << offset);
		// Access update every word in the cacheline (
		(*(addr+0))++;
		(*(addr+8))++;
		(*(addr+16))++;
		(*(addr+24))++;
		(*(addr+32))++;
		(*(addr+40))++;
		(*(addr+48))++;
		(*(addr+56))++;
//		__asm__ volatile("clflush (%0)" : : "r" (addr) : "memory");
	}
}

void read_loop_legacy(int magnitude, volatile char* memblock) {
	fprintf(stdout, "Looping 1M times over 2^%d bits\n", magnitude);
	for (unsigned long j = 0; j < ITERATIONS/20; j++)
		for (unsigned long i = 0; i < (1 << magnitude); i += 64)
			memblock[i]++;
}

void read_loop(int magnitude, volatile char* memblock) {
	fprintf(stdout, "Looping 1M times over address aligned at 2^%d bytes\n", magnitude);
	for (unsigned long j = 0; j < ITERATIONS/20; j++)
		memblock[1 << magnitude]++;
}

#define PAGEMAP_ENTRY_SIZE 8
uint64_t get_page_frame_number(void *virt_addr) {
	int err;
	uint64_t pagemap_entry;
	FILE * f = fopen("/proc/self/pagemap", "rb");
	if (!f) {
		perror("Unable to open /proc/self/pagemap!");
		return -1;
	}

	uint64_t file_offset = (uint64_t)virt_addr / getpagesize() * PAGEMAP_ENTRY_SIZE;
	err = fseek(f, file_offset, SEEK_SET);
	if (err) {
		perror("Unable to seek /proc/self/pagemap!");
		return -1;
	}
	err = fread(&pagemap_entry, PAGEMAP_ENTRY_SIZE, 1, f);
	if (err == -1) {
		perror("Unable to read from /proc/self/pagemap!");
		return -1;
	}
	// PFN is lower 55 bits
	return pagemap_entry & 0x007FFFFFFFFFFFFFul;
}

int main(int argc, char **argv)
{
	void *addr;
	size_t length = 1024 << 20; // 1GB
	int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB;
	int shift = 0;

	if (argc > 1)
		length = atol(argv[1]) << 20;
	if (argc > 2) {
		shift = atoi(argv[2]);
		if (shift)
			flags |= (shift & MAP_HUGE_MASK) << MAP_HUGE_SHIFT;
	}

	if (shift)
		printf("%u kB hugepages\n", 1 << shift);
	else
		printf("Default size hugepages\n");
	printf("Mapping %lu Mbytes\n", (unsigned long)length >> 20);

	addr = mmap(NULL, length, PROT_READ | PROT_WRITE, flags, -1, 0);
	if (addr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	// Make sure the page is actually allocated
	*((volatile char*)addr) = 1;
	*((volatile char*)addr) = 0;
	uint64_t pfn = get_page_frame_number(addr);
	fprintf(stdout, "Mapped 1GB at virtual address %p", addr);
	if (pfn == -1)
		fprintf(stderr, "Unable to get page frame number of huge page. Are you running as sudo?\n");
	else
		fprintf(stdout, " with physical page frame number %#lx (1GB hugepage #%lu)", pfn, pfn >> 18);
	fprintf(stdout, "\n");

	int done = 0;
	char command, command_buffer;
	int offset = 5;
	unsigned long addr_off = 0;
	while (!done) {
		fprintf(stdout, "Enter command ([a]ddress, [n]ext, [b]ack, [r]edo, or [q]uit): ");
		// Last character before newline is command
		// If no command is entered, the last command is repeated
		while ((command_buffer = fgetc(stdin)) != '\n')
			command = command_buffer;
		switch (command) {
			case 'a':
				fprintf(stdout, "Enter address as hex: ");
				if (fscanf(stdin, "%lx", &addr_off) < 1)
					fprintf(stdout, "Unable to read input. Enter as hex without a '0x' prefix.\n");
				else
					fprintf(stdout, "Accessing %lx...\n", addr_off);
				fgetc(stdin);
				read_loop(0, addr+addr_off);
				break;
			case 'n':
				offset++;
				// Exit if an oversized offset is requested
				if ((1 << offset) >= length)
					done = 1;
				else
					read_loop(offset, addr);
				break;
			case 'b':
				offset--;
				// Exit if an undersized offset is requested
				if (offset < 0)
					done = 1;
				else
					read_loop(offset, addr);
				break;
			case 'r':
				read_loop(offset, addr);
				break;
			case 'q':
				done = 1;
				break;
			default:
				fprintf(stderr, "Unrecognized command %c\n", command);
		}
	}

	/* munmap() length of MAP_HUGETLB memory must be hugepage aligned */
	if (munmap(addr, length)) {
		perror("munmap");
		exit(1);
	}

	return 0;
}
