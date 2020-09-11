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

int main(int argc, char **argv)
{
	void *addr;
	int ret;
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

	int done = 0;
	char command, command_buffer;
	int offset = 5;
	unsigned long addr_off = 0;
	while (!done) {
		fprintf(stdout, "Enter command ([a]ddress, [n]ext, [b]ack, [r]edo, or [q]uit): ");
		// Last character before newline is command
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
				if ((1 << offset) >= length)
					done = 1;
				else
					read_loop(offset, addr);
				break;
			case 'b':
				offset--;
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

	return ret;
}
