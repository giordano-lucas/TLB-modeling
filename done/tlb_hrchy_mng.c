
#include "tlb_hrchy_mng.h"
#include "addr.h"
#include "addr_mng.h"
#include "tlb_mng.h"
#include "page_walk.h"
#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

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
	
	
	switch (tlb_type) {
        case L1_ITLB:
            M_REQUIRE((L1_ITLB_LINES > SIZE_MAX/sizeof(l1_itlb_entry_t)) == 0, ERR_IO, "Couldnt memset : overflow, %c", " ");
			memset((l1_itlb_entry_t)tlb_type , 0, sizeof(l1_itlb_entry_t)*L1_ITLB_LINES);
            break;
        case L1_DTLB:
            M_REQUIRE((L1_DTLB_LINES > SIZE_MAX/sizeof(l1_dtlb_entry_t)) == 0, ERR_IO, "Couldnt memset : overflow, %c", " ");
			memset((l1_dtlb_entry_t)tlb_type , 0, sizeof(l1_dtlb_entry_t)*L1_DTLB_LINES);
            break;
        case L2_TLB:
            M_REQUIRE((L2_TLB_LINES > SIZE_MAX/sizeof(l2_tlb_entry_t)) == 0, ERR_IO, "Couldnt memset : overflow, %c", " ");
			memset((l2_tlb_entry_t)tlb_type , 0, sizeof(l2_tlb_entry_t)*L2_TLB_LINES);
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

int tlb_hit( const virt_addr_t * vaddr,
             phy_addr_t * paddr,
             const void  * tlb,
             tlb_t tlb_type);

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

int tlb_insert( uint32_t line_index,
                const void * tlb_entry,
                void * tlb,
                tlb_t tlb_type);

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
	
	if (tlb_type == L1_ITLB){
		l1_itlb_entry_t* l1i = (l1_itlb_entry_t*)tlb_entry;
        l1i->tag = virt_addr_t_to_virtual_page_number(vaddr) >> L1_ITLB_LINES_BITS;
        l1i->phy_page_num = paddr->phy_page_num;
		l1i->v = 1;
		}
	else if (tlb_type == L1_DTLB) {
		l1_dtlb_entry_t* l1d = (l1_dtlb_entry_t*)tlb_entry;
		l1d->tag = virt_addr_t_to_virtual_page_number(vaddr) >> L1_DTLB_LINES_BITS;
        l1d->phy_page_num = paddr->phy_page_num;
		l1d->v = 1;
		}
	else {
		l2_tlb_entry_t* l2 = (l2_tlb_entry_t*)tlb_entry;
		l2->tag = virt_addr_t_to_virtual_page_number(vaddr) >> L2_TLB_LINES_BITS;
        l2->phy_page_num = paddr->phy_page_num;
		l2->v = 1;
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

int tlb_search( const void * mem_space,
                const virt_addr_t * vaddr,
                phy_addr_t * paddr,
                mem_access_t access,
                l1_itlb_entry_t * l1_itlb,
                l1_dtlb_entry_t * l1_dtlb,
                l2_tlb_entry_t * l2_tlb,
                int* hit_or_miss);
