
/**
 * @file page_walk.c
 * @brief
 *
 */

#include "addr.h"
#include "error.h"
#include "addr_mng.h"
#include <inttypes.h>
/*
 * Creates a 16 bits mask of size "size" (nb of 1's)
 */
uint16_t maskof16(size_t size){
	uint16_t mask = 0;
	if (size == 16)
		return ~mask;
	else {
		mask = 1;
		mask = (mask << size) - 1;
		return mask;
	}
}

#define START_PAGE_TABLE 0
static inline pte_t read_page_entry(const pte_t * start, pte_t page_start, uint16_t index);
/**
 * @brief Page walker: virtual address to physical address conversion.
 *
 * @param mem_space starting address of our simulated memory space
 * @param vaddr virtual address to be converted
 * @param paddr (SET) physical address
 * @return error code
 */
int page_walk(const void* mem_space, const virt_addr_t* vaddr, phy_addr_t* paddr){
	M_REQUIRE_NON_NULL(mem_space);
	M_REQUIRE_NON_NULL(vaddr);
	M_REQUIRE_NON_NULL(paddr);
	pte_t page_begin = START_PAGE_TABLE;
	
	//read pgd
	page_begin = read_page_entry(mem_space,page_begin, vaddr->pgd_entry);
	//read pud
	page_begin = read_page_entry(mem_space,page_begin, vaddr->pud_entry);
	//read pmd
	page_begin = read_page_entry(mem_space,page_begin, vaddr->pmd_entry);
	//read pte
	page_begin = read_page_entry(mem_space,page_begin, vaddr->pte_entry);
	//intialize phy addr
	init_phy_addr(paddr, page_begin, vaddr->page_offset);

	return ERR_NONE;
	}



/** 
 * 
 * @brief returns the indexth word contained at page_start address if the memory starts at address start

 * @param start : starting address of the memory ;
 * @param  page_start :  index of the page to read ;
 * @param index : index, in the page, of the word to return
 * 
 * Requirements :
 * @param start : must be non null ;
 * @param  page_start :  XXXXX
 * @param index : must be of 12 bits
 * 
 */
static inline pte_t read_page_entry(const pte_t * start, pte_t page_start, uint16_t index){
	M_REQUIRE_NON_NULL(start);
	M_REQUIRE( ((maskof16(PAGE_OFFSET) & index) == index), ERR_BAD_PARAMETER, "index should be on 12 bits %c"," ");
	
	//check overflow 
		 return start[page_start/sizeof(pte_t)+index]; 
}
