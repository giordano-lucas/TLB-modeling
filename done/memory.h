#pragma once

/**
 * @file memory.h
 * @brief Functions for dealing wih the content of the memory (page directories and data).
 *
 * @author Mirjana Stojilovic & Jean-Cédric Chappelier
 * @date 2018-19
 */

#include "addr.h"   // for virt_addr_t
#include <stdlib.h> // for size_t and free()
#include <stdio.h>
#include "error.h"
#include <ctype.h>
#include <inttypes.h>

/**
 * @brief enum type to describe how to print address;
 * currently, it's either:
 *          NONE: don't print any address
 *          POINTER: print the actual pointer value
 *          OFFSET:   print the address as an offset value (in hexa)    from the start of the memory space
 *          OFFSET_U: print the address as an offset value (in decimal) from the start of the memory space
 */
enum addr_fmt {
    NONE,
    POINTER,
    OFFSET,
    OFFSET_U,
};
typedef enum addr_fmt addr_fmt_t;


/**
 * @brief Create and initialize the whole memory space from a provided
 * (binary) file containing one single dump of the whole memory space.
 *
 * @param filename the name of the memory dump file to read from
 * @param memory (modified) pointer to the begining of the memory
 * @param mem_capacity_in_bytes (modified) total size of the created memory
 * @return error code, *p_memory shall be NULL in case of error
 *
 */

int mem_init_from_dumpfile(const char* filename, void** memory, size_t* mem_capacity_in_bytes){
	M_REQUIRE_NON_NULL(filename);
	M_REQUIRE_NON_NULL(memory);
	M_REQUIRE_NON_NULL(mem_capacity_in_bytes);
	
	FILE* file = fopen(filename, "rb");
	M_REQUIRE_NON_NULL(file);
	
	// va tout au bout du fichier
	fseek(file, 0L, SEEK_END);
	// indique la position, et donc la taille (en octets)
	*mem_capacity_in_bytes = (size_t) ftell(file);
	// revient au début du fichier (pour le lire par la suite)
	rewind(file);
	
	//allocate memory
	*memory = calloc(*mem_capacity_in_bytes, sizeof(byte_t));
	M_REQUIRE_NON_NULL(*memory);
	
	size_t nb_read = fread(*memory, sizeof(byte_t), *mem_capacity_in_bytes, file);
	M_REQUIRE(nb_read == *mem_capacity_in_bytes, ERR_IO, "Error reading file %c", ' ');
	
	return ERR_NONE;
	}
int page_file_read(const void** memory,size_t memorySize, const uint64_t addr, const char* filename){
		M_REQUIRE_NON_NULL(memory);
		M_REQUIRE_NON_NULL(*memory);
		M_REQUIRE_NON_NULL(filename);
		// test that we can add an entire page at the given address 
		M_REQUIRE((addr + PAGE_SIZE) <= memorySize, ERR_MEM, "Cannot add a page of 4kB at the address : %"PRIx64" for memory size : %zu", addr,memorySize);
		FILE* file = fopen(filename, "rb");
		M_REQUIRE_NON_NULL(file);
		
		void* memoryFromAddr = &((*memory)[addr]);
		size_t nb_read = fread(memoryFromAddr, sizeof(byte_t), PAGE_SIZE,file);
		M_REQUIRE(nb_read == PAGE_SIZE, ERR_IO, "Error reading file : %c", ' ');
		return ERR_NONE;
	}

/**
 * @brief Create and initialize the whole memory space from a provided
 * (metadata text) file containing an description of the memory.
 * Its format is:
 *  line1:           TOTAL MEMORY SIZE (size_t)
 *  line2:           PGD PAGE FILENAME
 *  line4:           NUMBER N OF TRANSLATION PAGES (PUD+PMD+PTE)
 *  lines5 to (5+N): LIST OF TRANSLATION PAGES, expressed with two info per line:
 *                       INDEX OFFSET (uint32_t in hexa) and FILENAME
 *  remaining lines: LIST OF DATA PAGES, expressed with two info per line:
 *                       VIRTUAL ADDRESS (uint64_t in hexa) and FILENAME
 *
 * @param filename the name of the memory content description file to read from
 * @param memory (modified) pointer to the begining of the memory
 * @param mem_capacity_in_bytes (modified) total size of the created memory
 * @return error code, *p_memory shall be NULL in case of error
 *
 */
#define maxFileSize 100

int mem_init_from_description(const char* master_filename, void** memory, size_t* mem_capacity_in_bytes){
	FILE* f = fopen(master_filename, "r");
	size_t totalSize = -1;  //FIRST LINE : TOTAL MEM SIZE
	fscanf(f, "%zu", &totalSize);
	
	char pgd_location[maxFileSize];  //SECONDE LINE : PGD
	fgets(pgd_location, maxFileSize, f);
	strtok(pgd_location, "\n"); //removes newline char at the end
	
	size_t nb_tables; //THIRD LINE : NUMBER OF PDM+PUD+PTE
	fscanf(f, "%zu", &nb_tables);
	
	for(size_t i =0; i < nb_tables ; i++){
		char physicalAddress[10];
		fscanf("%s", physicalAddress);
		uint32_t location = strtoul(physicalAddress, (void)char**, 16);
		
		char pageLocation[maxFileSize];  
		fgets(pageLocation, maxFileSize, f);
		strtok(pageLocation, "\n"); //removes newline char at the end
		
		//USE PAGE FILE READ FOR EVERY OF THOSE TABLES
		page_file_read(memory, totalSize,location, pageLocation );
	}
	
	while(!feof(f)){
		char physicalAddress[18];
		fscanf("%s", physicalAddress);
		uint64_t location = strtoull(physicalAddress, (void)char**, 16);
		
		char pageLocation[maxFileSize];  
		fgets(pageLocation, maxFileSize, f);
		strtok(pageLocation, "\n"); //removes newline char at the end
		//CHANGE VIRTUAL TO PHYSICAL AND CALL PAGE READ
		
		virt_addr_t virt;
		init_virt_addr64(&virt, location);
		phy_addr_t phy;
		page_walk(*memory, &location, &phy);
		uint64_t physical = (phy.page_offset | (phy.phy_page_num << PAGE_OFFSET));
		page_file_read(memory, totalSize, physical, pageLocation);
	}
}


/**
 * @brief Prints the content of one page from its virtual address.
 * It prints the content reading it as 32 bits integers.
 * @param   mem_space the origin of the memory space simulating the whole memory
 * @param   from the virtual address of the page to print
 * @param   show_addr an option to indicate how to print the address of each printed bloc; see above
 * @param   line_size an option indicating how many 32-bits integers shall be displayed per line
 * @param   sep an option indicating what character string shall be used to separated 32-bits integer values that are printed
 * @return  error code
 */

int vmem_page_dump_with_options(const void mem_space, const virt_addr_t from,
                                addr_fmt_t show_addr, size_t line_size, const char* sep);





#define vmem_page_dump(mem, from) vmem_page_dump_with_options(mem, from, OFFSET, 16, " ")
