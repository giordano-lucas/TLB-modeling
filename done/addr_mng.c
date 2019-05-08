
#include "addr.h"
#include "addr_mng.h"
#include <inttypes.h>
#include "error.h"
#include <stdio.h> // FILE

/*
 * Others constants 
 */ 
#define PAGE_OFFSET_START 0
#define PTE_ENTRY_START 12
#define PMD_ENTRY_START 21
#define PUD_ENTRY_START 30
#define PGD_ENTRY_START 39


/*
 * Creates a 16 bits mask of size "size" (nb of 1's)
 */
uint16_t mask16(size_t size){
	uint16_t mask = 0;
	if (size == 16)
		return ~mask;
	else {
		mask = 1;
		mask = (mask << size) - 1;
		return mask;
	}
}

/*
 * Creates a 32 bits mask of size "size" (nb of 1's)
 */
uint32_t mask32(size_t size){
	uint32_t mask = 0;
	if (size == 32)
		return ~mask;
	else {
		mask = 1;
		mask = (mask << size) - 1;
		return mask;
	}
}
/*
 * Creates a 64 bits mask of size "size" (nb of 1's)
 */
uint64_t mask64(size_t size){
	uint64_t mask = 0;
	if (size == 64)
		return ~mask;
	else {
		mask = 1;
		mask = (mask << size) - 1;
		return mask;
	}
}
/*
 * Tests if "toTest" is on Size	bits (= not greater that 2^size -1)
 */
int isOfSizeAsked32(size_t size, uint32_t toTest){
	uint32_t mask = mask32(size);
	return (toTest&mask) == toTest;
}


/**
 * Initialize virt_addr_t structure. Reserved bits are zeros.
 *
 * 
 * Requirements:
 * 
 * @param vaddr : must be non null
 * @param pgd_entry : must be of 9 bits
 * @param pud_entry : must be of 9 bits
 * @param pmd_entry : must be of 9 bits
 * @param pte_entry : must be of 9 bits
 * @param page_offset : must be of 12 bits
 * 
 * 
 */
int init_virt_addr(virt_addr_t * vaddr,
                   uint16_t pgd_entry,
                   uint16_t pud_entry, uint16_t pmd_entry,
                   uint16_t pte_entry, uint16_t page_offset){
					   M_REQUIRE(isOfSizeAsked32(PGD_ENTRY, pgd_entry), ERR_BAD_PARAMETER, "Pgd entry size = %" PRIu16 " superior to 9", pgd_entry);
					   M_REQUIRE(isOfSizeAsked32(PUD_ENTRY, pud_entry), ERR_BAD_PARAMETER, "Pud entry size = %" PRIu16 " superior to 9", pud_entry);
					   M_REQUIRE(isOfSizeAsked32(PMD_ENTRY, pmd_entry), ERR_BAD_PARAMETER, "Pmd entry size = %" PRIu16 " superior to 9", pmd_entry);
					   M_REQUIRE(isOfSizeAsked32(PTE_ENTRY, pte_entry), ERR_BAD_PARAMETER, "Pte entry size = %" PRIu16 " superior to 9", pte_entry);
					   M_REQUIRE(isOfSizeAsked32(PAGE_OFFSET, page_offset), ERR_BAD_PARAMETER, "Page offset size = %" PRIu16 " superior to 12", page_offset);
					   M_REQUIRE_NON_NULL(vaddr);
					   
					   vaddr->pgd_entry = pgd_entry;
					   vaddr->pud_entry = pud_entry;
					   vaddr->pte_entry = pte_entry;
					   vaddr->pmd_entry = pmd_entry;
					   vaddr->page_offset = page_offset;
					   vaddr->reserved = 0;
					  
					  return ERR_NONE;
				   }

//=========================================================================
/**
 * @brief Initialize virt_addr_t structure from uint64_t. Reserved bits are zeros,
 * according to this format : 63-RESERVED-48|47-PGD-39|38-PUD-30|29-PMD-21|20-PTE-12|11-offset-0|
 * 
 * 
 * Requirements:
 * 
 * @param vaddr : must be non null
 */
int init_virt_addr64(virt_addr_t * vaddr, uint64_t vaddr64){
	uint16_t pgd_entry = mask64(PGD_ENTRY) & (vaddr64 >> (PGD_ENTRY_START));
	uint16_t pud_entry = mask64(PUD_ENTRY) & (vaddr64 >> (PUD_ENTRY_START));
	uint16_t pmd_entry = mask64(PMD_ENTRY) & (vaddr64 >> (PMD_ENTRY_START));
	uint16_t pte_entry = mask64(PTE_ENTRY) & (vaddr64 >> (PTE_ENTRY_START));
	uint16_t page_offset = mask64(PAGE_OFFSET) & (vaddr64 >> (PAGE_OFFSET_START));
	return init_virt_addr(vaddr,pgd_entry,pud_entry,pmd_entry,pte_entry,page_offset);
}

//=========================================================================
/**
 * @brief Initialize phy_addr_t structure,
 * according to this format :
 *  page_begin(bits 31 to 12) | page_offset
 * 
 * Requirements:
 * 
 * @param vaddr : must be non null
 * @param page_offset : must be of 12 bits
 * 
 */
int init_phy_addr(phy_addr_t* paddr, uint32_t page_begin, uint32_t page_offset){
					   M_REQUIRE(page_begin%PAGE_OFFSET == 0, ERR_BAD_PARAMETER, "Page begin must be a multiple of Page_offset","");
					   M_REQUIRE(isOfSizeAsked32(PAGE_OFFSET, page_offset), ERR_BAD_PARAMETER, "Page offset = %" PRIu16 " not on 12 bits", page_offset);
					   M_REQUIRE_NON_NULL(paddr);
					   paddr->phy_page_num = (page_begin >> PAGE_OFFSET);
					   paddr->page_offset = page_offset & mask32(PAGE_OFFSET);
					   
					   return ERR_NONE;
}


//=========================================================================
/**
 * @brief Convert virt_addr_t structure to uint64_t. It's the reciprocal of init_virt_addr64().
 * 
 * Requirements:
 * 
 * @param vaddr : must be non null
 * 
 * 
 * ---------------------------------
 * 
 * As mentionned in the documentation this function just shifts the result of virt_addr_t_to_virtual_page_number() and adds the page_offset bits
 */
uint64_t virt_addr_t_to_uint64_t(const virt_addr_t * vaddr){
		M_REQUIRE_NON_NULL(vaddr);
		return (virt_addr_t_to_virtual_page_number(vaddr) << PAGE_OFFSET) | vaddr->page_offset;
	}

//=========================================================================
/**
 * @brief Extract virtual page number from virt_addr_t structure.
 * 
 * Requirements:
 * 
 * @param vaddr : must be non null
 * --------------------------------------
 * 
 * Each value of vaddr is casted to a 64 bits value before shifting to ensure that the shifting process will not result in a 0 value 
 * (because intitaly it's a 16 bit value that is shifted by up to 39 bits (>16 bits))
 * 
 */
uint64_t virt_addr_t_to_virtual_page_number(const virt_addr_t * vaddr){
		M_REQUIRE_NON_NULL(vaddr);
		uint64_t y = ( (uint64_t)(vaddr->pgd_entry) << PGD_ENTRY_START) | ((uint64_t)(vaddr->pud_entry) << PUD_ENTRY_START) | ( (uint64_t)(vaddr->pmd_entry) << PMD_ENTRY_START) | ((uint64_t)(vaddr->pte_entry) << PTE_ENTRY_START);
		return y >> PAGE_OFFSET;
	}

//=========================================================================
/**
 * @brief print a virtual address the stream "where"
 */
int print_virtual_address(FILE* where, const virt_addr_t* vaddr){
	return fprintf(where, "PGD=0x%" PRIX16 "; PUD=0x%" PRIX16 "; PMD=0x%" PRIX16 "; PTE=0x%" PRIX16 "; offset=0x%" PRIX16, vaddr->pgd_entry, vaddr->pud_entry, vaddr->pmd_entry, vaddr->pte_entry, vaddr->page_offset);
}
//=========================================================================
/**
 * @brief print a physical address the stream "where"
 */
int print_physical_address(FILE* where, const phy_addr_t* paddr){
	M_REQUIRE_NON_NULL(paddr);
	return fprintf(where, "page num=0x%" PRIX32 "; offset=0x%" PRIX16, paddr->phy_page_num, paddr->page_offset);
}

