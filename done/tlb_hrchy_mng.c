
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

//================================================================================================================
// define constants for hit and miss
#define HIT 1
#define MISS 0 
//================================================================================================================
/**
 * @brief Clean a TLB with type type
 *
 * @param type     : type of tlb (l1_itlb_entry_t, l1_dtlb_entry_t, l2_tlb_entry_t)
 * @param tlb      : the (generic) pointer to the TLB
 * @param NB_LINES : the maximum number of lines of the tlb
 * 
 * it first test the overflow of sizeof(type)*NB_LINE
 */
#define flush_generic(type, tlb, NB_LINES)                                                                       \
	 M_REQUIRE((L1_ITLB_LINES > SIZE_MAX/sizeof(type)), ERR_SIZE, "Could not memset : overflow, %c", " ");    \
	 memset(tlb , 0, sizeof(type)*NB_LINES);                                                                     \
//================================================================================================================
/**
 * @brief Clean a TLB (invalidate, reset...). This function erases all TLB data.
 * 
 * Requirements : 
 * @param  tlb (generic) pointer to the TLB, must be non null
 * @param tlb_type an enum to distinguish between different TLBs
 * @return  error code
 * 
 * 
 */

int tlb_flush(void *tlb, tlb_t tlb_type){
	M_REQUIRE_NON_NULL(tlb);
	// test if the tlb_type is a valid instance of tlb_t
	M_REQUIRE(L1_ITLB <= tlb_type && tlb_type <= L2_TLB, ERR_BAD_PARAMETER, "%d is not a valid tlb_type \n", tlb_type);

	// for each tlb type call the generic macro defined before
	switch (tlb_type) {
        case L1_ITLB : flush_generic(l1_itlb_entry_t, tlb, L1_ITLB_LINES); break;
        case L1_DTLB : flush_generic(l1_dtlb_entry_t, tlb, L1_DTLB_LINES); break;
        case L2_TLB  : flush_generic(l2_tlb_entry_t , tlb, L2_TLB_LINES ); break;
        default      : return ERR_BAD_PARAMETER; break;
    }
	
	return ERR_NONE;
	}

//====================================================================================
/**
 * @brief generic macro that checks if a TLB entry exists in the TLB.
 *
 * On hit, return HIS (1) and update the physical page number passed as the pointer to the function.
 * On miss, return MISS (0).
 *
 * @param type       : type of tlb (l1_itlb_entry_t, l1_dtlb_entry_t, l2_tlb_entry_t)
 * @param tlb        : pointer to the beginning of the tlb
 * @param vaddr      : pointer to virtual address
 * @param paddr      : (modified) pointer to physical address
 * @param LINES_BITS : the number of bits needed to represent NB_LINES
 * @param NB_LINES   : the maximum number of lines of the tlb
 * @return HIS or MISS 
 * 
 * the method first computes the 64 bits virtual address in order to extract the tag and the 
 * line where the entry could be found.
 * 
 * if the entry is valid and the tag is correct => it's a hit and we update paddr else it's a miss
 */
#define hit_generic(type, tlb, vaddr, paddr, LINES_BITS, NB_LINES)                   \
	uint64_t addr = virt_addr_t_to_virtual_page_number(vaddr);                       \
	uint32_t tag = (addr) >> (LINES_BITS);                                           \
	uint8_t  line = (addr) % (NB_LINES);                                             \
	type entry = ((type*) tlb)[line];                                                \
	if (entry.tag == tag && entry.v == 1) {                                          \
		init_phy_addr(paddr, entry.phy_page_num << PAGE_OFFSET, vaddr->page_offset); \
		return HIT;                                                                  \
		}                                                                            \
	else return MISS;                                                                \
//====================================================================================
/**
 * @brief Check if a TLB entry exists in the TLB.
 *
 * On hit, return HIT(1) and update the physical page number passed as the pointer to the function.
 * On miss, return MISS (0).
 *
 * Requirements : 
 * @param vaddr    : must be non null
 * @param paddr    : must be non null
 * @param tlb      : must be non null
 * @param tlb_type : must be a valid instance of tlb_t
 * @return HIT (1) or MISS (0)
 */

int tlb_hit( const virt_addr_t * vaddr, phy_addr_t * paddr, const void  * tlb, tlb_t tlb_type){
	if(vaddr == NULL || paddr == NULL || tlb == NULL)return MISS;
	// check that tlb_type is a valid instance of tlb_t
	if (! (L1_ITLB <= tlb_type && tlb_type <= L2_TLB)) return MISS;
	
	// for each tlb type call the generic macro defined before
	switch (tlb_type) {
        case L1_ITLB : { hit_generic(l1_itlb_entry_t, tlb, vaddr, paddr, L1_ITLB_LINES_BITS, L1_ITLB_LINES);} break;
        case L1_DTLB : { hit_generic(l1_dtlb_entry_t, tlb, vaddr, paddr, L1_DTLB_LINES_BITS, L1_DTLB_LINES);} break;
        case L2_TLB  : { hit_generic(l2_tlb_entry_t , tlb, vaddr, paddr, L2_TLB_LINES_BITS , L2_TLB_LINES );} break;
        default      : return ERR_BAD_PARAMETER; break;
    }
    // should not arrive here 
	return MISS;
	}

//=========================================================================
/**
 * @brief Insert an entry to a tlb (in a generic fashion)
 * 
 * @param type       : type of tlb (l1_itlb_entry_t, l1_dtlb_entry_t, l2_tlb_entry_t)
 * @param tlb        : pointer to the beginning of the tlb
 * @param tlb_entry  : pointer to the tlb entry to insert
 * @param line_index : the number of the line to overwrite
 * @param NB_LINES   : the maximum number of lines of the tlb
 * 
 * it first validate line_index since line_index sould be smaller that NB_LINES
 * then it casts tlb and indexes it to update it with the new entry (that also need to be casted)
 */
#define insert_generic(type, tlb, tlb_entry, line_index, NB_LINES)                                                  \
	M_REQUIRE(line_index < NB_LINES, ERR_BAD_PARAMETER, "%"PRIx32" should be smaller than " #NB_LINES, line_index); \
	((type*)tlb)[line_index] = *((type*)tlb_entry);                                                                 \

//=========================================================================
/**
 * @brief Insert an entry to a tlb
 * 
 * Requirements : 
 * @param line_index should be smaller than the maximum number of lines of tlb
 * @param tlb_entry must be non mull
 * @param tlb must be non null
 * @param tlb_type : must be a valid instance of tlb_t
 * @return  error code
 */

int tlb_insert( uint32_t line_index, const void * tlb_entry, void * tlb,tlb_t tlb_type){
	M_REQUIRE_NON_NULL(tlb);
	M_REQUIRE_NON_NULL(tlb_entry);
	// check that tlb_type is a valid instance of tlb_t
	M_REQUIRE(L1_ITLB <= tlb_type && tlb_type <= L2_TLB, ERR_BAD_PARAMETER, "%d is not a valid tlb_type \n", tlb_type);
	
	// for each tlb type call the generic macro defined above
	switch (tlb_type) {
        case L1_ITLB : insert_generic(l1_itlb_entry_t, tlb, tlb_entry, line_index, L1_ITLB_LINES); break;
        case L1_DTLB : insert_generic(l1_dtlb_entry_t, tlb, tlb_entry, line_index, L1_DTLB_LINES); break;
        case L2_TLB  : insert_generic(l2_tlb_entry_t , tlb, tlb_entry, line_index, L2_TLB_LINES) ; break;
        default      : return ERR_BAD_PARAMETER; break;
    }
	return ERR_NONE;
	}

//=========================================================================
/**
 * @brief Initialize a TLB entry (in a generic fashion)
 * 
 * @param type       : type of tlb (l1_itlb_entry_t, l1_dtlb_entry_t, l2_tlb_entry_t)
 * @param tlb_entry  : pointer to the entry to be initialized
 * @param LINES_BITS : the number of bits needed to represent the number of lines of the given tlb
 * @param vaddr      : pointer to virtual address to extract the tag
 * @param paddr      : pointer to physical address to extract the physical page number
 * 
 * it first compute the tag by converting the vaddr to a 64 bits virtual address
 * then it set phy_page_num = (paddr)->phy_page_num and set the valid bit to 1
 */		
#define init_generic(type, tlb_entry, LINES_BITS, vaddr, paddr)                 \
		type* entry = (type*)(tlb_entry);                                       \
		entry->tag = virt_addr_t_to_virtual_page_number(vaddr) >> (LINES_BITS); \
		entry->phy_page_num = (paddr)->phy_page_num;                            \
		entry->v = 1;                                                           \
		
//=========================================================================
/**
 * @brief Initialize a TLB entry
 * 
 * Requirements : 
 * @param vaddr     : must be non null
 * @param paddr     : must be non null
 * @param tlb_entry : must be non null
 * @param tlb_type  : must be a valid instance of tlb_t
 * @return  error code
 */

int tlb_entry_init( const virt_addr_t * vaddr, const phy_addr_t * paddr, void * tlb_entry,tlb_t tlb_type){
	M_REQUIRE_NON_NULL(vaddr);
	M_REQUIRE_NON_NULL(paddr);
	M_REQUIRE_NON_NULL(tlb_entry);
	// check that tlb_type is a valid instance of tlb_t
	M_REQUIRE(L1_ITLB <= tlb_type && tlb_type <= L2_TLB, ERR_BAD_PARAMETER, "%d is not a valid tlb_type \n", tlb_type);

	// for each tlb type call the generic macro defined above
	switch (tlb_type){
		case L1_ITLB : { init_generic(l1_itlb_entry_t, tlb_entry, L1_ITLB_LINES_BITS, vaddr, paddr);} break;
		case L1_DTLB : { init_generic(l1_dtlb_entry_t, tlb_entry, L1_DTLB_LINES_BITS, vaddr, paddr);} break;
		case L2_TLB  : { init_generic(l2_tlb_entry_t , tlb_entry, L2_TLB_LINES_BITS , vaddr, paddr);} break;
		default      : return ERR_BAD_PARAMETER; break;
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
		if(*hit_or_miss == HIT) return ERR_NONE; //if found in lvl 1, return
		
		*hit_or_miss = tlb_hit(vaddr, paddr, l2_tlb, L2_TLB);//else search for it in lvl2
		uint8_t previouslyValid = 0;//previouslyValid and tag exist to check whether to invalidate the lvl1 tlb entry or not
		uint32_t previousTag = 0;
		bool needEviction = true;
		if(!*hit_or_miss){ //do page_walk if not found
			M_REQUIRE(page_walk(mem_space, vaddr, paddr) == ERR_NONE, ERR_MEM, "Couldnt find the paddr corresponding to this vaddr", ""); //page walk to get the right paddr since we havent found
			uint8_t line = virt_addr_t_to_virtual_page_number(vaddr) % L2_TLB_LINES; //get the right line in the lvl2 to create the entry
			l2_tlb_entry_t entry;
			tlb_entry_init(vaddr, paddr, &entry, L2_TLB); //init the lvl2 entry
			//check if there was a previously valid entry at this part
			previouslyValid = l2_tlb[line].v; //init previouslyValid and tag
			previousTag = (l2_tlb[line].tag);
			needEviction = false; //boolean that tells us whether we also need to evict
			tlb_insert(line, &entry, l2_tlb, L2_TLB);//inserts the new entry in the lvl2 tlb
		}
		//always need to insert in the lvl 1 tlb
		//invalidate macro for genericity
		#define invalidate(tlb,line) if((previouslyValid && tlb[line].v && (tlb[line].tag >> 2== previousTag)) || needEviction) tlb[line].v = 0;
		if(access == INSTRUCTION){
			//create entry + index
			l1_itlb_entry_t entry; //inits the entry for the itlb
			tlb_entry_init(vaddr,paddr,&entry, L1_ITLB);
			uint8_t line = virt_addr_t_to_virtual_page_number(vaddr) % L1_ITLB_LINES;//get the right line for the itlb
			tlb_insert(line, &entry, l1_itlb, L1_ITLB);// inserts the entry in the itlb
			//invalidate entry in the other tlb (line is the same because both lvl1 tlbs have the same number of entries)
			invalidate(l1_dtlb, line);
		}
		else{
			l1_dtlb_entry_t entry; //init the entry for the dtlb
			tlb_entry_init(vaddr,paddr,&entry, L1_DTLB);
			uint8_t line = virt_addr_t_to_virtual_page_number(vaddr) % L1_DTLB_LINES; //get the right line for inserting the entry in the dtlb
			tlb_insert(line, &entry, l1_dtlb, L1_DTLB);//inserts the entry in the dtlb
			//invalidate entry in the other tlb (line is the same because both lvl1 tlbs have the same number of entries)
			invalidate(l1_itlb, line);
		}
		return ERR_NONE;
		}
