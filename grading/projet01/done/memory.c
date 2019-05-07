/**
 * @memory.c
 * @brief memory management functions (dump, init from file, etc.)
 *
 * @date 2018-19
 */

#if defined _WIN32  || defined _WIN64
#define __USE_MINGW_ANSI_STDIO 1
#endif

#include "addr.h"
#include "memory.h"
#include "page_walk.h"
#include "addr_mng.h"
#include "stdbool.h"
#include "util.h" // for SIZE_T_FMT
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h> // for memset()
#include <inttypes.h> // for SCNx macros
#include <assert.h>

// ======================================================================
/**
 * @brief Tool function to print an address.
 *
 * @param show_addr the format how to display addresses; see addr_fmt_t type in memory.h
 * @param reference the reference address; i.e. the top of the main memory
 * @param addr the address to be displayed
 * @param sep a separator to print after the address (and its colon, printed anyway)
 *
 */
static void address_print(addr_fmt_t show_addr, const void* reference,
                          const void* addr, const char* sep)
{
    switch (show_addr) {
    case POINTER:
        (void)printf("%p", addr);
        break;
    case OFFSET:
        (void)printf("%zX", (const char*)addr - (const char*)reference);
        break;
    case OFFSET_U:
        (void)printf(SIZE_T_FMT, (const char*)addr - (const char*)reference);
        break;
    default:
        // do nothing
        return;
    }
    (void)printf(":%s", sep);
}

// ======================================================================
/**
 * @brief Tool function to print the content of a memory area
 *
 * @param reference the reference address; i.e. the top of the main memory
 * @param from first address to print
 * @param to first address NOT to print; if less that `from`, nothing is printed;
 * @param show_addr the format how to display addresses; see addr_fmt_t type in memory.h
 * @param line_size how many memory byted to print per stdout line
 * @param sep a separator to print after the address and between bytes
 *
 */
static void mem_dump_with_options(const void* reference, const void* from, const void* to,
                                  addr_fmt_t show_addr, size_t line_size, const char* sep)
{
    assert(line_size != 0);
    size_t nb_to_print = line_size;
    for (const uint8_t* addr = from; addr < (const uint8_t*) to; ++addr) {
        if (nb_to_print == line_size) {
            address_print(show_addr, reference, addr, sep);
        }
        (void)printf("%02"PRIX8"%s", *addr, sep);
        if (--nb_to_print == 0) {
            nb_to_print = line_size;
            putchar('\n');
        }
    }
    if (nb_to_print != line_size) putchar('\n');
}

// ======================================================================
// See memory.h for description
int vmem_page_dump_with_options(const void *mem_space, const virt_addr_t* from,
                                addr_fmt_t show_addr, size_t line_size, const char* sep)
{
#ifdef DEBUG
    debug_print("mem_space=%p\n", mem_space);
    (void)fprintf(stderr, __FILE__ ":%d:%s(): virt. addr.=", __LINE__, __func__);
    print_virtual_address(stderr, from);
    (void)fputc('\n', stderr);
#endif
    phy_addr_t paddr;
    zero_init_var(paddr);

    M_EXIT_IF_ERR(page_walk(mem_space, from, &paddr),
                  "calling page_walk() from vmem_page_dump_with_options()");
#ifdef DEBUG
    (void)fprintf(stderr, __FILE__ ":%d:%s(): phys. addr.=", __LINE__, __func__);
    print_physical_address(stderr, &paddr);
    (void)fputc('\n', stderr);
#endif

    const uint32_t paddr_offset = ((uint32_t) paddr.phy_page_num << PAGE_OFFSET);
    const char * const page_start = (const char *)mem_space + paddr_offset;
    const char * const start = page_start + paddr.page_offset;
    const char * const end_line = start + (line_size - paddr.page_offset % line_size);
    const char * const end   = page_start + PAGE_SIZE;
    debug_print("start=%p (offset=%zX)\n", (const void*) start, start - (const char *)mem_space);
    debug_print("end  =%p (offset=%zX)\n", (const void*) end, end   - (const char *)mem_space) ;
    mem_dump_with_options(mem_space, page_start, start, show_addr, line_size, sep);
    const size_t indent = paddr.page_offset % line_size;
    if (indent == 0) putchar('\n');
    address_print(show_addr, mem_space, start, sep);
    for (size_t i = 1; i <= indent; ++i) printf("  %s", sep);
    mem_dump_with_options(mem_space, start, end_line, NONE, line_size, sep);
    mem_dump_with_options(mem_space, end_line, end, show_addr, line_size, sep);
    return ERR_NONE;
}
/**
 * @brief : reads the content of filename and stores it in the "memory" parameter. Stores the number of bytes of the memory in mem_capacity_in_bytes
 * 
 * @param : filename : name of the file where we can read the memory. Must be non null
 * @param : memory : pointer to the output memory : must be non null (and *memory must be non null)
 * @param : nb of bytes of the memory : must be non null
 * 
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
	// revient au debut du fichier (pour le lire par la suite)
	rewind(file);
	
	// correcteur: missing check of mem_capacity_in_bytes being a multiple of a page size
	//allocate memory
	*memory = calloc(*mem_capacity_in_bytes, sizeof(byte_t));
	// test succes of allocation 
	// correcteur: missing file closing in case of error
	M_REQUIRE_NON_NULL(*memory);
	
	size_t nb_read = fread(*memory, sizeof(byte_t), *mem_capacity_in_bytes, file);
	// check the number of bytes written
	// correcteur: missing finalisation of memory in case of error
	M_REQUIRE(nb_read == *mem_capacity_in_bytes, ERR_IO, "Error reading file %c", ' ');
	fclose(file);
	return ERR_NONE;
	}
/**
 * @brief : helper function that stores the content of filename (that must be a page) in memory (with max size memorySize) at starting address addr.
 * 
 * @param : memory : memory where the content of filename must be written. Must be non null (*memory must also be non null)
 * @param : memorySize : size of memory
 * @param : addr : starting address (physical) of the memory from which the content of filename must be written
 * @param : filename : name of the file where the data can be found. Must be non null
 * 
 */
int page_file_read(void** memory,size_t memorySize, const uint64_t addr, const char* filename){
		M_REQUIRE_NON_NULL(memory); //check if the args arent null
		M_REQUIRE_NON_NULL(*memory) ; //memory should already be allocated
		M_REQUIRE_NON_NULL(filename);
		// test that we can add an entire page at the given address 
		M_REQUIRE((addr + PAGE_SIZE) <= memorySize, ERR_MEM, "Cannot add a page of 4kB at the address : %"PRIx64" for memory size : %zu", addr,memorySize);
		FILE* file = fopen(filename, "rb"); //open file to read from
		// test the opening of file
		M_REQUIRE_NON_NULL(file);

		byte_t* memoryFromAddr = (byte_t*)*memory; //pointer arithmetic to get right address
		memoryFromAddr += addr;

		// correcteur: missing file closing in case of error

		M_REQUIRE_NON_NULL(memoryFromAddr); //checks that the cast to a byte* didnt ruin the pointer
		//read the content of filename
		size_t nb_read = fread(memoryFromAddr, sizeof(byte_t), PAGE_SIZE,file); //check number of bytes read
		M_REQUIRE(nb_read == PAGE_SIZE, ERR_IO, "Error reading file : %c", ' ');
		fclose(file);
		return ERR_NONE;
	}
	
	#define MAX_SIZE_BUFFER 50
	/**
	 * This method is the same method as defined in commands.c but we werent sure whether it was safe to include it or not since it is not in the commands.h file
	 * @param input : must be non null, stream where the caracters will be read
	 * @param buffer[] : must be non null, buffer where the caracters will be written
	 * @return the number of char that have been read until a whitespace was found
	 * 			OR ERR_IO if more than MAX_SIZE_BUFFER consecutive char not separated by a whitespace have been read
	 * 
	 */
	int readUntilNextSpace(FILE* input, char buffer[], size_t* nbOfBytes){
	M_REQUIRE_NON_NULL(input);
	int c = 0; 
	bool hasReadOneNonWhiteSpace = false; // flag to say that we need to read at lest one non white space char to exit the function
	*nbOfBytes = 0;
	while (!feof(input) && !ferror(input) && !(isspace(c) && hasReadOneNonWhiteSpace)){
		
		// we iterate until we find a whitespace (and at least one non whitespace has been read) or until the end of the file
		
		c = fgetc(input);
		
		if (!isspace(c) || c == '\n'){ // if the char is not a white space then it's added into the buffer. 
			//Moreover if we add a \n then it can only be the last charater since the loop condition at the next iteration will be false (because of isspace(c) && hasReadOneNonWhiteSpace)
			// the last \n is usefull to check if a command is terminated by a '\n'
		    hasReadOneNonWhiteSpace = true; // if we enter here it means that we have read a non whitespace char (or a \n but as mentionned before this is the last char that could be added)
			if (*nbOfBytes >= MAX_SIZE_BUFFER){ // if more than MAX_SIZE_BUFFER are read then it means that it is an invalid command (see def of MAX_SIZE_BUFFER for more details)
				
				return ERR_IO;
				}
				
			buffer[*nbOfBytes] = c;
			
			(*nbOfBytes)++;
			}
		}
		
	return ERR_NONE;
	}
// correcteur: make it 1024
#define maxFileSize 200
/**
 * @brief : reads the content of the master filename and does a full memory dump accordingly
 * First it reads the total memory capacity and allocates memory param dynamically
 * Then it puts the pgd table in memory at address *memory[0]
 * Then it reads the number of tables to put in memory
 * Then it puts the number of tables given in memory (using the physical address given)
 * Then, until the end of file, it reads the virtual address of where to put the raw memory files and puts them in physical memory
 * @param : master_filename : name of the file where we can read the memory. Must be non null
 * @param : memory : pointer to the output memory : must be non null (and *memory must be non null)
 * @param : nb of bytes of the memory : must be non null
 * 
 * 
 */
int mem_init_from_description(const char* master_filename, void** memory, size_t* mem_capacity_in_bytes){
	
	// correcteur: missing validity check of master_filename
	// correcteur: missing validity check of memory
	// correcteur: missing validity check of mem_capacity_in_bytes

	// correcteur: missing NULL check
	FILE* f = fopen(master_filename, "r");
	
	fscanf(f, "%zu", mem_capacity_in_bytes);
	M_REQUIRE(fgetc(f) == '\n', ERR_IO, "Didn't get a new line : error %c", " ");
	//alloc memory first
	// correcteur: missing NULL check
	// correcteur: missing file closing in case of error
	*memory = calloc(*mem_capacity_in_bytes, sizeof(byte_t));

	char pgd_location[maxFileSize];  //Reads the filename of PGD
	// correcteur: missing error handling, i.e. EOF
	// correcteur: missing file closing in case of error
	// correcteur: missing finalisation of memory in case of error
	fgets(pgd_location, maxFileSize, f);
	strtok(pgd_location, "\n"); //removes newline char at the end
	// correcteur: missing test and propagation of error code of page_file_read
	page_file_read(memory, *mem_capacity_in_bytes, 0, pgd_location); //puts the pgd at address *memory[0] in memory
	size_t nb_tables; //THIRD LINE : NUMBER OF PDM+PUD+PTE
	// correcteur: missing error handling of reading number of translation pages
	fscanf(f, "%zu", &nb_tables); //reads the number of tables
	
	for(size_t i =0; i < nb_tables ; i++){ //for each of these table

		// correcteur: missing file closing in case of error
		// correcteur: missing finalisation of memory in case of error

		uint32_t location;
		// correcteur: missing error handling of reading number of location
		fscanf(f, "%" PRIX32, &location); //read the physical address of the table to put
		M_REQUIRE(fgetc(f) == ' ', ERR_BAD_PARAMETER, "There should be a single space between the physical address and the path to the file %c", ' ' ); //removes the space between the address and the path to the file
		char pageLocation[maxFileSize];  
		// correcteur: missing error handling of reading pageLocation
		fgets(pageLocation, maxFileSize, f); //gets the path to the file
		strtok(pageLocation, "\n"); //removes newline char at the end
		
		//USE PAGE FILE READ FOR EVERY OF THOSE TABLES
		
		// correcteur: missing test and propagation of error code of page_file_read
		page_file_read(memory, *mem_capacity_in_bytes,location, pageLocation ); //places the file in path read at the physical address read
	}
	
	while(!feof(f) && !ferror(f)){
		
		// correcteur: missing file closing in case of error
		// correcteur: missing finalisation of memory in case of error

		uint64_t virtaddr;
		char string[maxFileSize];
		size_t s;
		readUntilNextSpace(f, string,&s ); //reads the virtual address char by char to make sure to read up to the end of the file
		if(s <= 0){return 0;}
		else{
			string[s] = '\0'; //add a 0 at the end to make sure strtoull gets the right result
			virtaddr = strtoull(string, (char**)NULL, 16); //convert the virtual address in char* to a uint64
			string[s] = ' '; //resets the char at the end of the buffer
		}
		
		readUntilNextSpace(f, string, &s); //reads the full file path 
		if(s <= 0){return 0;} //if the size read is 0, we are at the end of the file : return
		string[s-1] = '\0'; //else : cut the file path  (remove the \n)
		//CHANGE VIRTUAL TO PHYSICAL AND CALL PAGE READ
		virt_addr_t virt;
		// correcteur: missing test and propagation of error code of init_virt_addr64
		init_virt_addr64(&virt, virtaddr); //inits the virtual address with the uint64 value we got by reading the file
		phy_addr_t phy;
		// correcteur: missing test and propagation of error code of page_walk
		page_walk(*memory, &virt, &phy); //translates the virtual address to a physical address
		uint64_t physical = (phy.phy_page_num << PAGE_OFFSET )| phy.page_offset; //gets the physical address as a uint64		
		// correcteur: missing test and propagation of error code of page_file_read
		page_file_read(memory, *mem_capacity_in_bytes, physical, &string[0]); //writes the raw page file at the translated physical address
		
	}
	fclose(f);
	return ERR_NONE;
}
