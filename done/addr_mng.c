#pragma once

/**
 * @file addr_mng.h
 * @brief address management functions (init, print, etc.)
 *
 * @author Mirjana Stojilovic
 * @date 2018-19
 */

#include "addr.h"
#include "addr_mng.h"
#include <inttypes.h>
#include "error.h"
#include <stdio.h> // FILE


int isOfSizeAsked32(size_t size, uint32_t toTest){
	uint32_t mask = 0;
	if(size == 32){
		mask = ~mask;
	}else{
		mask = (1<<size)-1;
	}
	return (toTest&mask) == toTest;
}

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


//=========================================================================
/**
 * @brief Initialize virt_addr_t structure. Reserved bits are zeroed.
 * @param vaddr (modified) the virtual address structure to be initialized
 * @param pgd_entry the value of the PGD offset of the virtual address
 * @param pud_entry the value of the PUD offset of the virtual address
 * @param pmd_entry the value of the PMD offset of the virtual address
 * @param pte_entry the value of the PT  offset of the virtual address
 * @param page_offset the value of the physical memory page offset of the virtual address
 * @return error code
 */
int init_virt_addr(virt_addr_t * vaddr,
                   uint16_t pgd_entry,
                   uint16_t pud_entry, uint16_t pmd_entry,
                   uint16_t pte_entry, uint16_t page_offset){
					   M_REQUIRE(isOfSizeAsked32(PGD_ENTRY, pgd_entry), ERR_BAD_PARAMETER, "Pgd entry size = %" PRIu16 " superior to 9", pgd_entry);
					   M_REQUIRE(isOfSizeAsked32(PUD_ENTRY, pud_entry), ERR_BAD_PARAMETER, "Pud entry size = %" PRIu16 " superior to 9", pud_entry);
					   M_REQUIRE(isOfSizeAsked32(PMD_ENTRY, pmd_entry), ERR_BAD_PARAMETER, "Pmd entry size = %" PRIu16 " superior to 9", pmd_entry);
					   M_REQUIRE(isOfSizeAsked32(PTE_ENTRY, pte_entry), ERR_BAD_PARAMETER, "Pte entry size = %" PRIu16 " superior to 9", pte_entry);
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
 * @brief Initialize virt_addr_t structure from uint64_t. Reserved bits are zeros.
 * @param vaddr (modified) the virtual address structure to be initialized
 * @param vaddr64 the virtual address provided as a 64-bit pattern
 * @return error code
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
 * @brief Initialize phy_addr_t structure.
 * @param paddr (modified) the physical address structure to be initialized
 * @param page_begin the address of the top of the physical page
 * @param page_offset the index (offset) inside the physical page
 * @return error code
 */
int init_phy_addr(phy_addr_t* paddr, uint32_t page_begin, uint32_t page_offset);

//=========================================================================
/**
 * @brief Convert virt_addr_t structure to uint64_t. It's the reciprocal of init_virt_addr64().
 * @param vaddr the virtual address structure to be translated to a 64-bit pattern
 * @return the 64-bit pattern corresponding to the physical address
 */
uint64_t virt_addr_t_to_uint64_t(const virt_addr_t * vaddr){
		M_REQUIRE_NON_NULL(vaddr);
		return (virt_addr_t_to_virtual_page_number(vaddr) << PAGE_OFFSET) | vaddr->page_offset;
	}

//=========================================================================
/**
 * @brief Extract virtual page number from virt_addr_t structure.
 * @param vaddr the virtual address structure
 * @return the virtual page number corresponding to the virtual address
 */
uint64_t virt_addr_t_to_virtual_page_number(const virt_addr_t * vaddr){
		M_REQUIRE_NON_NULL(vaddr);
		uint64_t y = ( (uint64_t)(vaddr->pgd_entry) << PGD_ENTRY_START) | ((uint64_t)(vaddr->pud_entry) << PUD_ENTRY_START) | ( (uint64_t)(vaddr->pmd_entry) << PMD_ENTRY_START) | ((uint64_t)(vaddr->pte_entry) << PTE_ENTRY_START);
		return y >> PAGE_OFFSET_START;
	}

//=========================================================================
/**
 * @brief print a virtual address to stream
 * @param where the stream where to print to
 * @param vaddr the virtual address to be printed
 * @return number of printed characters
 */
int print_virtual_address(FILE* where, const virt_addr_t* vaddr){
	return fprintf(where, "PGD=0x%" PRIX16 "; PUD=0x%" PRIX16 "; PMD=0x%" PRIX16 "; PTE=0x%" PRIX16 "; offset=0x%" PRIX16, vaddr->pgd_entry, vaddr->pud_entry, vaddr->pmd_entry, vaddr->pte_entry, vaddr->page_offset);
}
//=========================================================================
/**
 * @brief print a physical address to stream
 * @param where the stream where to print to
 * @param paddr the physical address to be printed
 * @return number of printed characters
 */
int print_physical_address(FILE* where, const phy_addr_t* paddr);
