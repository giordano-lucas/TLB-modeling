
#include "tlb_hrchy_mng.h"
#include "addr.h"
#include "addr_mng.h"
#include "addr_mng.c"
#include "page_walk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include <stdbool.h>


//=========================================================================
/**
 * @brief Clean a TLB (invalidate, reset...).
 *
 * This function erases all TLB data.
 * @param  tlb (generic) pointer to the TLB
 * @param tlb_type an enum to distinguish between different TLBs
 * @return  error code
 */

int tlb_flush(void *tlb, tlb_t tlb_type){
	M_REQUIRE_NON_NULL(tlb);
	M_REQUIRE(L1_ITLB <= tlb_type && tlb_type <= L2_TLB, ERR_BAD_PARAMETER, "%d is not a valid tlb_type \n", tlb_type);

	
	switch (tlb_type) {
        case L1_ITLB:
            M_REQUIRE((L1_ITLB_LINES > SIZE_MAX/sizeof(l1_itlb_entry_t)) == 0, ERR_IO, "Couldnt memset : overflow, %c", " ");
			memset(tlb , 0, sizeof(l1_itlb_entry_t)*L1_ITLB_LINES);
            break;
        case L1_DTLB:
            M_REQUIRE((L1_DTLB_LINES > SIZE_MAX/sizeof(l1_dtlb_entry_t)) == 0, ERR_IO, "Couldnt memset : overflow, %c", " ");
			memset(tlb, 0, sizeof(l1_dtlb_entry_t)*L1_DTLB_LINES);
            break;
        case L2_TLB:
            M_REQUIRE((L2_TLB_LINES > SIZE_MAX/sizeof(l2_tlb_entry_t)) == 0, ERR_IO, "Couldnt memset : overflow, %c", " ");
			memset(tlb, 0, sizeof(l2_tlb_entry_t)*L2_TLB_LINES);
            break;
        default:
			//error
			//????
            break;
    }
	
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
 * @param tlb_type to distinguish between different TLBs
 * @return hit (1) or miss (0)
 */

int tlb_hit( const virt_addr_t * vaddr, phy_addr_t * paddr, const void  * tlb, tlb_t tlb_type){
	if(vaddr == NULL || paddr == NULL || tlb == NULL)return 0;
	if (! (L1_ITLB <= tlb_type && tlb_type <= L2_TLB)) return 0;
	
	uint64_t addr = virt_addr_t_to_virtual_page_number(vaddr);
	uint32_t tag = 0;
	uint8_t line = 0;
	
	bool hit = false;
	uint32_t phy_page_num = 0;
	
	switch (tlb_type) {
        case L1_ITLB: {
			tag = addr >> L1_ITLB_LINES_BITS; // takes last L1_ITLB_LINES_BITS bits
			line = addr % L1_ITLB_LINES; // takes first L1_ITLB_LINES  bits
			l1_itlb_entry_t entry = *(((l1_itlb_entry_t*) tlb + line));
			if (entry.tag == tag && entry.v == 1) {
				hit = true;
				phy_page_num = entry.phy_page_num;
				}
			}
            break;
        case L1_DTLB: {
			tag = addr >> L1_DTLB_LINES_BITS;
			line = addr % L1_DTLB_LINES;
			l1_dtlb_entry_t entry = *(((l1_dtlb_entry_t*) tlb + line));
			if (entry.tag == tag && entry.v == 1) {
				hit = true;
				phy_page_num = entry.phy_page_num;
				}
			}
            break;
        case L2_TLB: {
			tag = addr >> L2_TLB_LINES_BITS;
			line = addr % L2_TLB_LINES;
			l2_tlb_entry_t entry = *(((l2_tlb_entry_t*) tlb + line));
			if (entry.tag == tag && entry.v == 1) {
				hit = true;
				phy_page_num = entry.phy_page_num;
				}
			}
            break;
        default:
			//error
			//????
            break;
    }
	
	if(hit){ //we got a hit
		init_phy_addr(paddr, phy_page_num << PAGE_OFFSET, vaddr->page_offset);
		return 1;
		}
	
	return 0;
	}

//=========================================================================

#define insert_generic(type, tlb, tlb_entry, line_index, LINES) \
	M_REQUIRE(line_index < LINES, ERR_BAD_PARAMETER, "%"PRIx32" should be smaller than " #LINES, line_index); \
	((type*)tlb)[line_index] = *((type*)tlb_entry); \

//=========================================================================
/**
 * @brief Insert an entry to a tlb. Eviction policy is simple since
 *        direct mapping is used.
 * @param line_index the number of the line to overwrite
 * @param tlb_entry pointer to the tlb entry to insert
 * @param tlb pointer to the TLB
 * @param tlb_type to distinguish between different TLBs
 * @return  error code
 */

int tlb_insert( uint32_t line_index, const void * tlb_entry, void * tlb,tlb_t tlb_type){
	M_REQUIRE_NON_NULL(tlb);
	M_REQUIRE(L1_ITLB <= tlb_type && tlb_type <= L2_TLB, ERR_BAD_PARAMETER, "%d is not a valid tlb_type \n", tlb_type);
	
	switch (tlb_type) {
        case L1_ITLB:
			/*M_REQUIRE(line_index < L1_ITLB_LINES, ERR_BAD_PARAMETER, "%"PRIx32" should be smaller than L1_ITLB_LINES", line_index);
			// tlb is now an array of l1_itlb_entry_t
			((l1_itlb_entry_t*)tlb)[line_index] = *((l1_itlb_entry_t*)tlb_entry);*/
			insert_generic(l1_itlb_entry_t, tlb, tlb_entry, line_index, L1_ITLB_LINES);
            break;
        case L1_DTLB:
            M_REQUIRE(line_index < L1_DTLB_LINES, ERR_BAD_PARAMETER, "%"PRIx32" should be smaller than L1_ITLB_LINES", line_index);
			// tlb is now an array of l1_dtlb_entry_t
			((l1_dtlb_entry_t*)tlb)[line_index] = *((l1_dtlb_entry_t*)tlb_entry);
			break;
        case L2_TLB:
			M_REQUIRE(line_index < L2_TLB_LINES, ERR_BAD_PARAMETER, "%"PRIx32" should be smaller than L1_ITLB_LINES", line_index);
			// tlb is now an array of l2_tlb_entry_t
			((l2_tlb_entry_t*)tlb)[line_index] = *((l2_tlb_entry_t*)tlb_entry);
			break;
        default:
			//error
			//????
            break;
    }
	
	
	return ERR_NONE;
	}

//=========================================================================
		
#define init_generic(type, tlb_entry, LINES_BITS, vaddr, paddr) \
		type* entry = (type*)(tlb_entry); \
		entry->tag = virt_addr_t_to_virtual_page_number(vaddr) >> (LINES_BITS); \
		entry->phy_page_num = (paddr)->phy_page_num; \
		entry->v = 1; \
		
//=========================================================================
/**
 * @brief Initialize a TLB entry
 * @param vaddr pointer to virtual address, to extract tlb tag
 * @param paddr pointer to physical address, to extract physical page number
 * @param tlb_entry pointer to the entry to be initialized
 * @param tlb_type to distinguish between different TLBs
 * @return  error code
 */

int tlb_entry_init( const virt_addr_t * vaddr, const phy_addr_t * paddr, void * tlb_entry,tlb_t tlb_type){
	M_REQUIRE_NON_NULL(vaddr);
	M_REQUIRE_NON_NULL(paddr);
	M_REQUIRE_NON_NULL(tlb_entry);
	M_REQUIRE(L1_ITLB <= tlb_type && tlb_type <= L2_TLB, ERR_BAD_PARAMETER, "%d is not a valid tlb_type \n", tlb_type);

	switch (tlb_type){
		case L1_ITLB : {
			/*l1_itlb_entry_t* l1i = (l1_itlb_entry_t*)tlb_entry;
			l1i->tag = virt_addr_t_to_virtual_page_number(vaddr) >> L1_ITLB_LINES_BITS;
			l1i->phy_page_num = paddr->phy_page_num;
			l1i->v = 1;
			*/
			init_generic(l1_itlb_entry_t, tlb_entry, L1_ITLB_LINES_BITS, vaddr, paddr);
			}
		break;
		case L1_DTLB: {
			l1_dtlb_entry_t* l1d = (l1_dtlb_entry_t*)tlb_entry;
			l1d->tag = virt_addr_t_to_virtual_page_number(vaddr) >> L1_DTLB_LINES_BITS;
			l1d->phy_page_num = paddr->phy_page_num;
			l1d->v = 1;
			}
		break;
		case L2_TLB: {
			l2_tlb_entry_t* l2 = (l2_tlb_entry_t*)tlb_entry;
			l2->tag = virt_addr_t_to_virtual_page_number(vaddr) >> L2_TLB_LINES_BITS;
			l2->phy_page_num = paddr->phy_page_num;
			l2->v = 1;
			}
		break;
		}
	
	return ERR_NONE;
	}

//=========================================================================
/**
 * @brief Ask TLB for the translation.
 *
 * @param mem_space pointer to the memory space
 * @param vaddr pointer to virtual address
 * @param paddr (modified) pointer to physical address (returned from TLB)
 * @param access to distinguish between fetching instructions and reading/writing data
 * @param l1_itlb pointer to the beginning of L1 ITLB
 * @param l1_dtlb pointer to the beginning of L1 DTLB
 * @param l2_tlb pointer to the beginning of L2 TLB
 * @param hit_or_miss (modified) hit (1) or miss (0)
 * @return error code
 */

int tlb_search( const void * mem_space,const virt_addr_t * vaddr, phy_addr_t * paddr, mem_access_t access, l1_itlb_entry_t * l1_itlb, 
				l1_dtlb_entry_t * l1_dtlb, l2_tlb_entry_t * l2_tlb, int* hit_or_miss){
					M_REQUIRE_NON_NULL(mem_space);
					M_REQUIRE_NON_NULL(vaddr);
					M_REQUIRE_NON_NULL(paddr);
					M_REQUIRE_NON_NULL(l1_itlb);
					M_REQUIRE_NON_NULL(l1_dtlb);
					M_REQUIRE_NON_NULL(l2_tlb);
					M_REQUIRE_NON_NULL(hit_or_miss);
					
					if(access == INSTRUCTION){*hit_or_miss = tlb_hit(vaddr, paddr, l1_itlb, L1_ITLB);}
					else{*hit_or_miss = tlb_hit(vaddr, paddr, l1_dtlb, L1_DTLB);}
					if(!*hit_or_miss){
							*hit_or_miss = tlb_hit(vaddr, paddr, l2_tlb, L2_TLB);
							page_walk(mem_space, vaddr, paddr);
							//int line = addr % L2_2TLB_LINES;
							l2_tlb_entry_t entry;
							tlb_entry_init(vaddr, paddr, &entry, L2_TLB);
							//l2_tlb[line] = entry;
							if(access == INSTRUCTION){//init itlb entry
								
							//invalidate dtlb entry
							}
							else{//init dtlb entry
								
								//invalidate itlb entry
							}
					}
					
					
					return ERR_NONE;
					}
					

int tlb_search_2( const void * mem_space,const virt_addr_t * vaddr, phy_addr_t * paddr, mem_access_t access, l1_itlb_entry_t * l1_itlb, 
				l1_dtlb_entry_t * l1_dtlb, l2_tlb_entry_t * l2_tlb, int* hit_or_miss){
					
			*hit_or_miss = (access == INSTRUCTION) ? tlb_hit( vaddr, paddr,  l1_itlb, L1_ITLB) :tlb_hit( vaddr, paddr,  l1_dtlb, L1_DTLB);
			
			if (*hit_or_miss != 0) return ERR_NONE; 
			
			*hit_or_miss = tlb_hit(vaddr, paddr, l2_tlb, L2_TLB);
			
			
			if (*hit_or_miss != 0){
				//si on la trouve au niveau 2 : il ne reste plus qu'à mettre le paramètre hit à 1, 
				//mais aussi à reporter les bonnes information dans le TLB niveau 1 correspondant 
				//(ITLB ou DTLB en fonction de l'accès demandé) ;
				uint64_t addr = virt_addr_t_to_virtual_page_number(vaddr); 
				uint32_t line_index = 0;
				uint32_t tag = 0;
				void* entry = NULL;
				tlb_t type = 0;
				void* tlb = NULL;
				switch (access) {
					case INSTRUCTION: {
						line_index = addr % L1_ITLB_LINES;
						tag = addr >> L1_ITLB_LINES_BITS;
						l1_itlb_entry_t e  = {tag, paddr->phy_page_num, 1};
						entry = &e;
					}
					break;
					case DATA:
						line_index = addr % L1_DTLB_LINES;
						tag = addr >> L1_DTLB_LINES_BITS;
					break;
					}
				
				tlb_insert(line_index, entry, tlb, type);
				}
	
	}
