
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
	M_REQUIRE_NON_NULL(tlb);
	M_REQUIRE((TLB_LINES > SIZE_MAX/sizeof(tlb_entry_t)) == 0, ERR_IO, "Couldnt memset : overflow, %c", " "); //memset all the size that is needed for tlb_lines* the size of an entry to 0
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
				if(vaddr == NULL || paddr == NULL || tlb == NULL || replacement_policy == NULL){ //cant require non null since the return value contains information about hit
					return 0;
				}
				//cant propagate an error with this function since it is supposed to return a uint64 anyways
				uint64_t tag = virt_addr_t_to_virtual_page_number(vaddr); //first we extract the tag from the virt addr
				node_t* n = (replacement_policy->ll)->back; //we get the last node in the list
				while(n != NULL){ //iterate on the full list, each time going to the previous one in order to end at the first
					list_content_t value = n->value; //get the value corresponding to this node
					if (!(value < TLB_LINES)) return 0; // ERR_BAD_PARAMETER : Index to set has to be inferior to TLBLINES //the value in the node corresponds to an index : must be inferior to tlblines
					
					if(tlb[value].tag == tag && tlb[value].v == 1){ //we got a hit
						int err;
						if((err = init_phy_addr(paddr, tlb[value].phy_page_num << PAGE_OFFSET, vaddr->page_offset)) != ERR_NONE) return err; //if we hit, initialize a paddr to the value found
						replacement_policy->move_back((replacement_policy->ll), n); //move back the node, method has no return so we cant propagate err
						return 1; //hit
					}
					n = n->previous; //if no hit, iterate
				}
				return 0; //if no hit after iterating through all nodes, miss
				
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
					M_REQUIRE_NON_NULL(tlb_entry); //checks if every parameter is valid
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
						//cant propagate an error with this function since it is supposed to return a uint64 anyways
						tlb_entry->tag = virt_addr_t_to_virtual_page_number(vaddr); //sets the tag to the virt addr
						tlb_entry->phy_page_num = paddr->phy_page_num;              //sets the page num to the paddr's page num
						tlb_entry->v = 1;                                           //set validity bit to one since when we init an entry we want to insert it
						return ERR_NONE;
					}

//=========================================================================
/**
 * @brief Ask TLB for the translation.
 *
 * @param mem_space pointer to the memory space, must be non null
 * @param vaddr pointer to virtual address, must be non null
 * @param paddr (modified) pointer to physical address (returned from TLB), must be non null
 * @param tlb pointer to the beginning of the TLB, must be non null
 * @param hit_or_miss (modified) hit (1) or miss (0), must be non null
 * @return error code
 */
int tlb_search( const void * mem_space, const virt_addr_t * vaddr,  phy_addr_t * paddr, tlb_entry_t * tlb,replacement_policy_t * replacement_policy, int* hit_or_miss){
		M_REQUIRE_NON_NULL(mem_space); //checks that all pointers are non null
		M_REQUIRE_NON_NULL(vaddr);
		M_REQUIRE_NON_NULL(paddr);
		M_REQUIRE_NON_NULL(tlb);
		M_REQUIRE_NON_NULL(replacement_policy);
		M_REQUIRE_NON_NULL(hit_or_miss);
		M_REQUIRE_NON_NULL((replacement_policy->ll)->front);
		
		*hit_or_miss = tlb_hit(vaddr, paddr, tlb, replacement_policy); //checks if we have a hit or a miss
		if(*hit_or_miss == 0){ //if we have a hit we dont do anything, if hit == 0 (just to be clearer than !hit), then we miss and update the tlb
			int err;
			if((err = page_walk(mem_space, vaddr, paddr)) != ERR_NONE) return err; //modifies paddr to be the good value corresponding to vadddr
			list_content_t head = ((replacement_policy->ll)->front)->value; //creates the new head for the list
			M_REQUIRE(0 <= head && head < TLB_LINES, ERR_BAD_PARAMETER, "Head should be in TLB , actual value : %zu" , head);
			tlb_entry_t tlb_entr; //initalizes the new entry corresponding to the paddr we just computed
			if ((err = tlb_entry_init(vaddr,paddr, &tlb_entr))!= ERR_NONE) return err ;
			
			tlb_insert(head, &tlb_entr,tlb);//places the entry we initialized into the head we created
			replacement_policy->move_back(replacement_policy->ll, (replacement_policy->ll)->front); //moves back the head we created into the linked list, void method so no error propagation
		}
		return ERR_NONE;
		}
