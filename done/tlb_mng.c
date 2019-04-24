
/**
 * @file tlb_mng.h
 * @brief TLB management functions for fully-associative TLB
 *
 * @author Mirjana Stojilovic
 * @date 2018-19
 */

#include "tlb.h"
#include "addr.h"
#include "addr_mng.h"
#include "tlb_mng.h"
#include "page_walk.h"
#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#ifndef SIZE_MAX
#define SIZE_MAX (~(size_t)0)
#endif

//=========================================================================
/**
 * @brief Clean a TLB (invalidate, reset...).
 *
 * This function erases all TLB data.
 * @param tlb pointer to the TLB
 * @return error code
 */
int tlb_flush(tlb_entry_t * tlb){
	M_REQUIRE((TLB_LINES > SIZE_MAX/sizeof(tlb_entry_t)) == 0, ERR_IO, "Couldnt memset : overflow, %c", " ");
	memset(tlb , 0, sizeof(tlb_entry_t)*TLB_LINES);
	return ERR_NONE;
}

//=========================================================================
/**
 * @brief Check if a TLB entry exists in the TLB.
 *
 * On hit, return success (1) and update the physical page number passed as the pointer to the function.
 * On miss, return miss (0).
 *
 * @param vaddr pointer to virtual address
 * @param paddr (modified) pointer to physical address
 * @param tlb pointer to the beginning of the tlb
 * @return hit (1) or miss (0)
 */
int tlb_hit(const virt_addr_t * vaddr, phy_addr_t * paddr,const tlb_entry_t * tlb,replacement_policy_t * replacement_policy){
				if(vaddr == NULL || paddr == NULL || tlb == NULL || replacement_policy == NULL){
					return 0;
				}
				uint64_t tag = virt_addr_t_to_virtual_page_number(vaddr);
				node_t* n = (replacement_policy->ll)->back;
				while(n != NULL){
					list_content_t value = n->value;
					M_REQUIRE(value < TLB_LINES, ERR_BAD_PARAMETER, "Index to set has to be inferior to TLBLINES, %c" ,"");
					
					if(tlb[value].tag == tag && tlb[value].v == 1){ //we got a hit
						init_phy_addr(paddr, tlb[value].phy_page_num << PAGE_OFFSET, vaddr->page_offset);
						replacement_policy->move_back((replacement_policy->ll), n);
						return 1;
					}
					n = n->previous;
				}
				return 0;
				
			}

//=========================================================================
/**
 * @brief Insert an entry to a tlb.
 * Eviction policy is least recently used (LRU).
 *
 * @param line_index the number of the line to overwrite
 * @param tlb_entry pointer to the tlb entry to insert
 * @param tlb pointer to the TLB
 * @return  error code
 */
int tlb_insert( uint32_t line_index,
                const tlb_entry_t * tlb_entry,
                tlb_entry_t * tlb){
					M_REQUIRE_NON_NULL(tlb_entry);
					M_REQUIRE_NON_NULL(tlb);
					M_REQUIRE(line_index < TLB_LINES, ERR_BAD_PARAMETER, "Index to set has to be inferior to TLBLINES, %c" ,"");
					
					//set the line at index given to the new entry
					tlb[line_index] = *tlb_entry;
					return ERR_NONE;
				}

//=========================================================================
/**
 * @brief Initialize a TLB entry
 * @param vaddr pointer to virtual address, to extract tlb tag
 * @param paddr pointer to physical address, to extract physical page number
 * @param tlb_entry pointer to the entry to be initialized
 * @return  error code
 */
int tlb_entry_init( const virt_addr_t * vaddr,
                    const phy_addr_t * paddr,
                    tlb_entry_t * tlb_entry){
						M_REQUIRE_NON_NULL(vaddr);
						M_REQUIRE_NON_NULL(paddr);
						M_REQUIRE_NON_NULL(tlb_entry);
						tlb_entry->tag = virt_addr_t_to_virtual_page_number(vaddr);
						tlb_entry->phy_page_num = paddr->phy_page_num;
						tlb_entry->v = 1;
						return ERR_NONE;
					}

//=========================================================================
/**
 * @brief Ask TLB for the translation.
 *
 * @param mem_space pointer to the memory space
 * @param vaddr pointer to virtual address
 * @param paddr (modified) pointer to physical address (returned from TLB)
 * @param tlb pointer to the beginning of the TLB
 * @param hit_or_miss (modified) hit (1) or miss (0)
 * @return error code
 */
int tlb_search( const void * mem_space,
                const virt_addr_t * vaddr,
                phy_addr_t * paddr,
                tlb_entry_t * tlb,
                replacement_policy_t * replacement_policy,
                int* hit_or_miss){
					M_REQUIRE_NON_NULL(mem_space);
					M_REQUIRE_NON_NULL(vaddr);
					M_REQUIRE_NON_NULL(paddr);
					M_REQUIRE_NON_NULL(tlb);
					M_REQUIRE_NON_NULL(replacement_policy);
					M_REQUIRE_NON_NULL((replacement_policy->ll)->front);
					*hit_or_miss = tlb_hit(vaddr, paddr, tlb, replacement_policy);
					if(*hit_or_miss == 0){ //replace stuff
						page_walk(mem_space, vaddr, paddr); //modifies paddr to be the good value
						list_content_t head = ((replacement_policy->ll)->front)->value;
						M_REQUIRE(0 <= head && head < TLB_LINES, ERR_BAD_PARAMETER, "Head should be in TLB , actual value : %zu" , head);
						tlb_entry_t tlb_entr;
						tlb_entry_init(vaddr,paddr, &tlb_entr);
						tlb[head]  = tlb_entr;
						replacement_policy->move_back(replacement_policy->ll, (replacement_policy->ll)->front);
					}
					return ERR_NONE;
				}
