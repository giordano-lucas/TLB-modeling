
#include "tlb_hrchy_mng.h"
#include "addr.h"
#include "addr_mng.h"
#include "page_walk.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "error.h"
#include <stdbool.h>


//=========================================================================
#define flush_generic(type, tlb, NB_LINES)                                                                       \
	 M_REQUIRE((L1_ITLB_LINES > SIZE_MAX/sizeof(type)) == 0, ERR_IO, "Could not memset : overflow, %c", " "); \
	 memset(tlb , 0, sizeof(type)*NB_LINES);                                                             \
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
        case L1_ITLB: flush_generic(l1_itlb_entry_t, tlb, L1_ITLB_LINES); break;
        case L1_DTLB: flush_generic(l1_dtlb_entry_t, tlb, L1_DTLB_LINES); break;
        case L2_TLB : flush_generic(l2_tlb_entry_t , tlb, L2_TLB_LINES ); break;
        default: break;
    }
	
	return ERR_NONE;
	}

//=========================================================================
#define hit_generic(type, tlb, vaddr, paddr, LINES_BITS, LINES)                      \
	uint64_t addr = virt_addr_t_to_virtual_page_number(vaddr);                       \
	uint32_t tag = (addr) >> (LINES_BITS);                                           \
	uint8_t  line = (addr) % (LINES);                                                \
	type entry = ((type*) tlb)[line];                                                \
	if (entry.tag == tag && entry.v == 1) {                                          \
		init_phy_addr(paddr, entry.phy_page_num << PAGE_OFFSET, vaddr->page_offset); \
		return 1;                                                                    \
		}                                                                            \
	else return 0;                                                                   \
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
	
	switch (tlb_type) {
        case L1_ITLB: { hit_generic(l1_itlb_entry_t, tlb, vaddr, paddr, L1_ITLB_LINES_BITS, L1_ITLB_LINES);} break;
        case L1_DTLB: {	hit_generic(l1_dtlb_entry_t, tlb, vaddr, paddr, L1_DTLB_LINES_BITS, L1_DTLB_LINES);} break;
        case L2_TLB : { hit_generic(l2_tlb_entry_t , tlb, vaddr, paddr, L2_TLB_LINES_BITS , L2_TLB_LINES );} break;
        default: break;
    }
    // should not arrive here
	return 0;
	}

//=========================================================================

#define insert_generic(type, tlb, tlb_entry, line_index, LINES)                                               \
	M_REQUIRE(line_index < LINES, ERR_BAD_PARAMETER, "%"PRIx32" should be smaller than " #LINES, line_index); \
	((type*)tlb)[line_index] = *((type*)tlb_entry);                                                           \

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
        case L1_ITLB: insert_generic(l1_itlb_entry_t, tlb, tlb_entry, line_index, L1_ITLB_LINES); break;
        case L1_DTLB: insert_generic(l1_dtlb_entry_t, tlb, tlb_entry, line_index, L1_DTLB_LINES); break;
        case L2_TLB : insert_generic(l2_tlb_entry_t , tlb, tlb_entry, line_index, L2_TLB_LINES) ; break;
        default: break;
    }
	return ERR_NONE;
	}

//=========================================================================
		
#define init_generic(type, tlb_entry, LINES_BITS, vaddr, paddr)                 \
		type* entry = (type*)(tlb_entry);                                       \
		entry->tag = virt_addr_t_to_virtual_page_number(vaddr) >> (LINES_BITS); \
		entry->phy_page_num = (paddr)->phy_page_num;                            \
		entry->v = 1;                                                           \
		
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
		case L1_ITLB : { init_generic(l1_itlb_entry_t, tlb_entry, L1_ITLB_LINES_BITS, vaddr, paddr);} break;
		case L1_DTLB : { init_generic(l1_dtlb_entry_t, tlb_entry, L1_DTLB_LINES_BITS, vaddr, paddr);} break;
		case L2_TLB  : { init_generic(l2_tlb_entry_t , tlb_entry, L2_TLB_LINES_BITS , vaddr, paddr);} break;
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

		*hit_or_miss = (access == INSTRUCTION)? tlb_hit(vaddr, paddr, l1_itlb, L1_ITLB):tlb_hit(vaddr, paddr, l1_dtlb, L1_DTLB);
		if(*hit_or_miss != 0) return ERR_NONE; //if found in lvl 1, return
		
		*hit_or_miss = tlb_hit(vaddr, paddr, l2_tlb, L2_TLB);//else search for it in lvl2
		uint8_t previouslyValid = 0;
		uint32_t previousTag = 0;
		bool needEviction = true;
		if(!*hit_or_miss){ //do page_walk if not found
			M_REQUIRE(page_walk(mem_space, vaddr, paddr) == ERR_NONE, ERR_MEM, "Couldnt find the paddr corresponding to this vaddr", "");
			uint8_t line = virt_addr_t_to_virtual_page_number(vaddr) % L2_TLB_LINES;
			l2_tlb_entry_t entry;
			tlb_entry_init(vaddr, paddr, &entry, L2_TLB);
			//check if there was a previously valid entry at this part
			previouslyValid = l2_tlb[line].v;
			previousTag = (l2_tlb[line].tag);
			needEviction = false;
			tlb_insert(line, &entry, l2_tlb, L2_TLB);
		}
		//always need to insert in the lvl 1 tlb
		#define invalidate(tlb,line) if((previouslyValid && tlb[line].v && (tlb[line].tag >> 2== previousTag)) || needEviction) tlb[line].v = 0;
		if(access == INSTRUCTION){
			//create entry + index
			l1_itlb_entry_t entry;
			tlb_entry_init(vaddr,paddr,&entry, L1_ITLB);
			uint8_t line = virt_addr_t_to_virtual_page_number(vaddr) % L1_ITLB_LINES;
			tlb_insert(line, &entry, l1_itlb, L1_ITLB);
			//invalidate entry in the other tlb
			invalidate(l1_dtlb, line);
		}
		else{
			l1_dtlb_entry_t entry;
			tlb_entry_init(vaddr,paddr,&entry, L1_DTLB);
			uint8_t line = virt_addr_t_to_virtual_page_number(vaddr) % L1_DTLB_LINES;
			tlb_insert(line, &entry, l1_dtlb, L1_DTLB);
			//invalidate entry in the other tlb
			invalidate(l1_itlb, line);
		}
		return ERR_NONE;
		}
