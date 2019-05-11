

#include "error.h"
#include "cache_mng.h"
#include "lru.h"
#include <stdbool.h>
//=========================================================================
//=========================================================================

#define phy_to_int(phy) (uint32_t)(((phy)->phy_page_num << PAGE_OFFSET) | (phy)->page_offset)
/**
 * @brief Cleans a cache with type type
 *
 * @param type     : type of cache (l1_icache_entry_t, l1_dcache_entry_t, l2_cache_entry_t)
 * @param cache    : the (generic) pointer to the cache
 * @param NB_LINES : the number of lines of the cache
 * @param WAYS     : the number of ways of the cache
 * 
 * it first test the overflow of sizeof(type)*NB_LINE*WAYS
 */
#define flush_generic(type, cache, NB_LINES, WAYS)                                                                   \
	 M_REQUIRE(((NB_LINES) < SIZE_MAX/(WAYS)), ERR_SIZE, "Could not memset : overflow, %c", " ");                    \
	 M_REQUIRE((((NB_LINES)*(WAYS)) < SIZE_MAX/sizeof(type)), ERR_SIZE, "Could not memset : overflow, %c", " ");     \
	 memset(cache , 0, sizeof(type)*(NB_LINES)*(WAYS));                                                              
//=========================================================================
/**
 * @brief Clean a cache (invalidate, reset...).
 *
 * This function erases all cache data.
 * @param cache pointer to the cache
 * @param cache_type an enum to distinguish between different caches
 * @return error code
 */
int cache_flush(void *cache, cache_t cache_type){
	M_REQUIRE_NON_NULL(cache);
	// test if the tlb_type is a valid instance of tlb_t
	M_REQUIRE(L1_ICACHE <= cache_type && cache_type <= L2_CACHE, ERR_BAD_PARAMETER, "%d is not a valid cache_type \n", cache_type);

	// for each cache type call the generic macro defined before
	switch (cache_type) {
        case L1_ICACHE : flush_generic(l1_icache_entry_t, cache, L1_ICACHE_LINES, L1_ICACHE_WAYS); break;
        case L1_DCACHE : flush_generic(l1_dcache_entry_t, cache, L1_DCACHE_LINES, L1_DCACHE_WAYS); break;
        case L2_CACHE  : flush_generic(l2_cache_entry_t , cache, L2_CACHE_LINES , L2_CACHE_WAYS) ; break;
        default        : return ERR_BAD_PARAMETER; break;
    }
	
	return ERR_NONE;
	}

//=========================================================================
/**
 * @brief Check if a instruction/data is present in one of the caches.
 *
 * On hit, return success (1) and update the cache-line-size chunk of data passed as the pointer to the function.
 * On miss, return miss (0).
 *
 * @param mem_space starting address of the memory space
 * @param cache pointer to the beginning of the cache
 * @param paddr pointer to physical address
 * @param p_line pointer to a cache-line-size chunk of data to return
 * @param hit_way (modified) cache way where hit was detected, HIT_WAY_MISS on miss
 * @param hit_index (modified) cache line index where hit was detected, HIT_INDEX_MISS on miss
 * @param cache_type to distinguish between different caches
 * @return  error code
 */

int cache_hit (const void * mem_space, void * cache, phy_addr_t * paddr, const uint32_t ** p_line, uint8_t *hit_way, uint16_t *hit_index, cache_t cache_type){
	M_REQUIRE_NON_NULL(mem_space);
	M_REQUIRE_NON_NULL(cache);
	M_REQUIRE_NON_NULL(p_line);
	M_REQUIRE_NON_NULL(hit_way);
	M_REQUIRE_NON_NULL(hit_index);
	
	uint32_t phy_addr = phy_to_int(paddr);
	uint16_t line_index = (phy_addr>>L1_ICACHE_WORDS_PER_LINE)%L1_ICACHE_LINE;
	uint32_t tag = phy_addr >> L1_ICACHE_TAG_REMAINING_BITS;
	// initialise return values
	*hit_way = HIT_WAY_MISS;
	*hit_index = HIT_INDEX_MISS;


	foreach_way(way, L1_ICACHE_WAYS) {
		if (!cache_valid(l1_icache_entry_t, L1_ICACHE_WAYS, line_index, way) ){//found a place
			LRU_age_increase(l1_icache_entry_t, L1_ICACHE_WAYS, line_index, way);//update ages
			return ERR_NONE;
			}
		else if (cache_tag(l1_icache_entry_t, L1_ICACHE_WAYS, line_index, way) == tag){//hit
			*p_line = cache_line(l1_icache_entry_t, L1_ICACHE_WAYS, line_index, way);
			*hit_way = way;
			*hit_index = line_index;
			LRU_age_update(l1_icache_entry_t, L1_ICACHE_WAYS, line_index, way);//update ages
			return ERR_NONE;
		}
	}
	//if we arrive here no entry has been found
	return ERR_NONE;
}
#define init_cache_entry(TYPE, LINE, AGE,VALID)
//=========================================================================
/**
 * @brief Insert an entry to a cache.
 *
 * @param cache_line_index the number of the line to overwrite
 * @param cache_way the number of the way where to insert
 * @param cache_line_in pointer to the cache line to insert
 * @param cache pointer to the cache
 * @param cache_type to distinguish between different caches
 * @return  error code
 */

int cache_insert(uint16_t cache_line_index,uint8_t cache_way,const void * cache_line_in,
                 void * cache, cache_t cache_type){
	M_REQUIRE_NON_NULL(cache);
	M_REQUIRE_NON_NULL(cache_line_in);
	word_t line = *((word_t*)cache_line_in);
	
	switch (cache_type) {
         case L1_ICACHE : 
        // cache_line(L1_ICACHE, L1_ICACHE_WAYS, cache_line_index, cache_way) = line;
         return ERR_NONE;
         break;
         case L1_DCACHE : return ERR_NONE;
         case L2_CACHE  : return ERR_NONE;
         default        : return ERR_BAD_PARAMETER; break;
    }
					
				 }

//=========================================================================
/**
 * @brief Initialize a cache entry (write to the cache entry for the first time)
 *
 * @param mem_space starting address of the memory space
 * @param paddr pointer to physical address, to extract the tag
 * @param cache_entry pointer to the entry to be initialized
 * @param cache_type to distinguish between different caches
 * @return  error code
 */
int cache_entry_init(const void * mem_space, const phy_addr_t * paddr,void * cache_entry,cache_t cache_type){
	M_REQUIRE_NON_NULL(mem_space);
	M_REQUIRE_NON_NULL(paddr);
	M_REQUIRE_NON_NULL(cache_entry);
	M_REQUIRE((L1_ICACHE<=cache_type)&&(cache_type <= L2_CACHE),ERR_BAD_PARAMETER,"Wrong instance of cache_type %c",' ');
	
	uint32_t phy_addr = phy_to_int(paddr);
	switch (cache_type) {
		case L1_ICACHE : {
			l1_icache_entry_t* entry = cache_entry;
			entry->v = 1;
			entry->age = 0;
			entry->tag = phy_addr >> L1_ICACHE_TAG_REMAINING_BITS;
			memcpy (entry->line, mem_space + phy_addr/((sizeof(byte_t)*L1_ICACHE_LINES)),L1_ICACHE_WORDS_PER_LINE);
			}
		break;
		case L1_DCACHE :
		break;
		case L2_CACHE  :
		break;
		default: return ERR_BAD_PARAMETER; break;
	}
	return ERR_NONE;
	}

//=========================================================================
//======================== helper functions cache read ====================

#define cast_l2_to_l1_generic(type, l1_entry, l2_entry, phy_addr, TAG_REMAINING_BITS)        \
	type* entry = (type*) l1_entry;                                                          \
	entry->v = 1;                                                                            \
	entry->age = 0;                                                                          \
	entry->tag = phy_addr >> TAG_REMAINING_BITS;                                             \
	memcpy (entry->line, l2_entry->line,L2_CACHE_WORDS_PER_LINE);       

void cast_cache_line_l2_to_l1(mem_access_t access, void* l1_entry, l2_cache_entry_t* l2_entry, uint32_t phy_addr){
	switch(access){
		case INSTRUCTION: {cast_l2_to_l1_generic(l1_icache_entry_t, l1_entry, l2_entry, phy_addr, L1_ICACHE_TAG_REMAINING_BITS);}break;
		case DATA       : {cast_l2_to_l1_generic(l1_dcache_entry_t, l1_entry, l2_entry, phy_addr, L1_DCACHE_TAG_REMAINING_BITS);}break;
		}
	}
#define NOTHING_FOUND -1
#define find_empty_slot_generic(TYPE, cache, LINE_INDEX, WAYS)    {                                     \
	foreach_way(way, WAYS){                                                                             \
		if (!cache_valid(TYPE, WAYS, LINE_INDEX, way)) {return way;}                                    \
		}                                                                                               \
		return NOTHING_FOUND;                                                                           \
	}
int find_empty_slot(cache_t cache_type, void* cache, uint16_t line_index){
	switch (cache_type){
		case L1_ICACHE: find_empty_slot_generic(l1_icache_entry_t, cache, line_index, L1_ICACHE_WAYS);break;
		case L1_DCACHE: find_empty_slot_generic(l1_dcache_entry_t, cache, line_index, L1_DCACHE_WAYS);break;
		case L2_CACHE : find_empty_slot_generic(l2_cache_entry_t , cache, line_index, L2_CACHE_WAYS) ;break;
		default: return NOTHING_FOUND;break;
		}
	}                                                                                            
#define evict_generic(TYPE, cache, LINE_INDEX, WAYS)    {                                                \
	unsigned max_age = 0 ;                                                                               \
	TYPE* entry_to_evict = NULL;                                                                         \
	foreach_way(way, WAYS){                                                                              \
		TYPE* entry = cache_entry(TYPE, WAYS, LINE_INDEX, way);                                          \
		if (entry->v && entry->age >= max_age) {                                                         \
		    max_age = entry->age;                                                                        \
		    entry_to_evict = entry;                                                                      \
			}                                                                                            \
		}                                                                                                \
		if (entry_to_evict == NULL) return NULL;                                                         \
		entry_to_evict->v = 0;                                                                           \
		return entry_to_evict;                                                                           \
		}
void* evict(cache_t cache_type, void* cache, uint16_t line_index) {
	switch (cache_type){
		case L1_ICACHE: evict_generic(l1_icache_entry_t, cache, line_index, L1_ICACHE_WAYS);break;
		case L1_DCACHE: evict_generic(l1_dcache_entry_t, cache, line_index, L1_DCACHE_WAYS);break;
		case L2_CACHE : evict_generic(l2_cache_entry_t , cache, line_index, L2_CACHE_WAYS) ;break;
		default: return NULL;break;
		}
	}
#define extract_line_index(phy_addr, WORDS_PER_LINE, NB_LINES) (((phy_addr)>>(WORDS_PER_LINE)) % (NB_LINES))

int insert_level2(l2_cache_entry_t* cache, l2_cache_entry_t* entry, uint32_t phy_addr){
	uint16_t line_index = extract_line_index(phy_addr, L2_CACHE_WORDS_PER_LINE, L2_CACHE_LINES);
	int err = ERR_NONE;
	//find place
	int cache_way = find_empty_slot(L2_CACHE, cache, line_index);
	if (cache_way != NOTHING_FOUND){ // there is some place in l2 cache
		//if ((err = cache_insert(line_index,cache_way,entry->line,cache, L2_CACHE))!= ERR_NONE) return err;//error propagation
		//LRU_age_increase(l2_cache_entry_t, L2_CACHE_WAYS, line_index, cache_way);		//update ages
		}
	else { // there is no empty slot in l2 cache
		evict(L2_CACHE, cache, line_index); //eviction
		//if ((err = cache_insert(line_index,cache_way,entry->line,cache, L2_CACHE))!= ERR_NONE) return err;//error propagation
		//LRU_age_update(l2_cache_entry_t, L2_CACHE_WAYS, line_index, cache_way); //update ages
		}
	if ((err = cache_insert(line_index,cache_way,entry->line,cache, L2_CACHE))!= ERR_NONE) return err;//error propagation
	if (cache_way != NOTHING_FOUND) LRU_age_increase(l2_cache_entry_t, L2_CACHE_WAYS, line_index, cache_way);//update ages
	return ERR_NONE;
	}

#define cast_l1_entry(access, entry) ( ((access) == INSTRUCTION)? ((l1_icache_entry_t*)(entry)): ((l1_icache_entry_t*)(entry)))

void modify_ages_level1(mem_access_t access,void *cache, uint8_t way, uint16_t line_index){
	if (way == NOTHING_FOUND) return;//not a cold start and not a hit
	switch (access){
		case INSTRUCTION: LRU_age_increase(l1_icache_entry_t, L1_ICACHE_WAYS, line_index, way);break;
		case DATA       : LRU_age_increase(l1_dcache_entry_t, L1_DCACHE_WAYS, line_index, way);break;
		default         : fprintf(stderr, "wrong instance of mem access at modify ages"); break;
		}
	}

int insert_level1(mem_access_t access,void * l1_cache, void * l2_cache, void* entry, uint32_t phy_addr){
	int err = ERR_NONE;
	cache_t cache_type = (access == INSTRUCTION) ? L1_ICACHE: L1_DCACHE;
	uint16_t line_index = extract_line_index(phy_addr, L1_ICACHE_WORDS_PER_LINE, L1_ICACHE_LINES);
	//find place
	int cache_way = find_empty_slot(cache_type, l1_cache, line_index);
	if (cache_way == NOTHING_FOUND){// make some space and transfer evicted entry to level 2
		// there is no empty slot in l1 cache
		void* evicted_entry = evict(cache_type, l1_cache, line_index); //eviction
		l2_cache_entry_t l2_entry;
		l2_entry.v = 1; l2_entry.age = 0; l2_entry.tag = phy_addr >> L2_CACHE_TAG_REMAINING_BITS;
		memcpy(l2_entry.line, cast_l1_entry(access,evicted_entry)->line, L2_CACHE_WORDS_PER_LINE); // copy content of l1 entry to l2 entry
		// move entry to level 2
		if ((err = insert_level2((l2_cache_entry_t*)l2_cache, &l2_entry, phy_addr))!= ERR_NONE) return err; //error propagation
		}
	//insert in l1 cache (here we are sure that there is at least an empt way)
    if ((err = cache_insert(line_index,cache_way,cast_l1_entry(access, entry)->line, l1_cache,cache_type))!= ERR_NONE) return err; // error propagation
	modify_ages_level1(access,l1_cache, cache_way,line_index);//update ages
	return ERR_NONE;
	} 

//#define create_l1_entry_of_name(access, name) (access == INSTRUCTION)?#l1_icache_entry_t name:#l1_dcache_entry_t name
//#define type(access)  if (access == INSTRUCTION) l1_icache_entry_t* else l1_dcache_entry_t* 
int move_entry_to_level1(mem_access_t access,void * l1_cache, void * l2_cache, l2_cache_entry_t* l2_entry, uint32_t phy_addr){
	cache_t cache_type = 0;
	uint16_t line_index = 0;
	int err = ERR_NONE;
	void* l1_entry = NULL; 
	switch (access){
		case INSTRUCTION: {
			cache_type = L1_ICACHE;
			line_index = extract_line_index(phy_addr, L1_ICACHE_WORDS_PER_LINE, L1_ICACHE_LINES);
			l1_icache_entry_t entry; l1_entry = &entry;
			}break;
		case DATA       : {
			cache_type = L1_DCACHE;
			line_index = extract_line_index(phy_addr, L1_DCACHE_WORDS_PER_LINE, L1_DCACHE_LINES);
			l1_dcache_entry_t entry; l1_entry = &entry;
			}break;
		default: return ERR_BAD_PARAMETER;
		}
	cast_cache_line_l2_to_l1(access,l1_entry,l2_entry,phy_addr);
	l2_entry->v = 0; //invalidate l2_entry 
	if ((err = insert_level1(access,l1_cache, l2_cache, l1_entry, phy_addr))!= ERR_NONE) return err;//error propagation
	return ERR_NONE;
	}

//=========================================================================
/**
 * @brief Ask cache for a word of data.
 *  Exclusive policy (https://en.wikipedia.org/wiki/Cache_inclusion_policy)
 *      Consider the case when L2 is exclusive of L1. Suppose there is a
 *      processor read request for block X. If the block is found in L1 cache,
 *      then the data is read from L1 cache and returned to the processor. If
 *      the block is not found in the L1 cache, but present in the L2 cache,
 *      then the cache block is moved from the L2 cache to the L1 cache. If
 *      this causes a block to be evicted from L1, the evicted block is then
 *      placed into L2. This is the only way L2 gets populated. Here, L2
 *      behaves like a victim cache. If the block is not found in both L1 and
 *      L2, then it is fetched from main memory and placed just in L1 and not
 *      in L2.
 *
 * @param mem_space pointer to the memory space
 * @param paddr pointer to a physical address
 * @param access to distinguish between fetching instructions and reading/writing data
 * @param l1_cache pointer to the beginning of L1 CACHE
 * @param l2_cache pointer to the beginning of L2 CACHE
 * @param word pointer to the word of data that is returned by cache
 * @param replace replacement policy
 * @return  error code
 */
int cache_read(const void * mem_space,phy_addr_t * paddr, mem_access_t access,
               void * l1_cache, void * l2_cache, uint32_t * word, cache_replace_t replace){
	M_REQUIRE_NON_NULL(mem_space);
	M_REQUIRE_NON_NULL(paddr);
	M_REQUIRE_NON_NULL(l1_cache);
	M_REQUIRE_NON_NULL(l2_cache);
	M_REQUIRE_NON_NULL(word);
	M_REQUIRE(replace == LRU, ERR_BAD_PARAMETER, "replace is not a valid instance of cache_replace_t %c", ' ');
	M_REQUIRE(access == INSTRUCTION || access == DATA, ERR_BAD_PARAMETER, "access is not a valid instance of mem_access_t %c", ' ');
	void* cache = NULL; // to use macro defined in cache_h
	int err = ERR_NONE; // used to propagate errors
	uint32_t phy_addr = phy_to_int(paddr);
	const uint32_t * p_line;
	uint8_t hit_way;
	uint16_t hit_index;
	err = (access == INSTRUCTION)? cache_hit(mem_space, l1_cache, paddr,&p_line,&hit_way,&hit_index, L1_ICACHE):
									   cache_hit(mem_space, l1_cache, paddr,&p_line,&hit_way,&hit_index, L1_DCACHE);
	if (err != ERR_NONE){return err;} //error handling
	if  (hit_way != HIT_WAY_MISS){
		//found it
		cache = l1_cache;
		if (access == INSTRUCTION) *word = cache_line(l1_icache_entry_t, L1_ICACHE_WAYS, hit_index, hit_way)[paddr->page_offset%L1_ICACHE_WORDS_PER_LINE];
		else                       *word = cache_line(l1_dcache_entry_t, L1_DCACHE_WAYS, hit_index, hit_way)[paddr->page_offset%L1_DCACHE_WORDS_PER_LINE];
		}
	else{
		//not found in l1 => search in l2
		cache_hit(mem_space, l2_cache, paddr,&p_line,&hit_way,&hit_index, L2_CACHE);
		if (hit_way != HIT_WAY_MISS) {
			cache = l2_cache;
			if ((err = move_entry_to_level1(access,l1_cache, l2_cache, cache_entry(l2_cache_entry_t, L2_CACHE_WAYS, hit_index, hit_way), phy_addr))!= ERR_NONE){
				return err;}//error propagation
			*word = cache_line(l2_cache_entry_t, L2_CACHE_WAYS, hit_index, hit_way)[paddr->page_offset%L2_CACHE_WORDS_PER_LINE];
			}
		else {
			//not foud in l2
			void* entry = NULL;
			if (access == INSTRUCTION){
				l1_icache_entry_t cache_entry;
				entry = &cache_entry;
				if ((err =cache_entry_init(mem_space,paddr,entry,L1_ICACHE))!= ERR_NONE) return err; //error propagation
				}
			else {
				l1_dcache_entry_t cache_entry;
				entry = &cache_entry;
				if ((err =cache_entry_init(mem_space,paddr,entry,L1_DCACHE))!= ERR_NONE) return err; //error propagation
				//insert level 1
				if ((err = insert_level1(access,l1_cache, l2_cache, entry,phy_addr))!= ERR_NONE) return err; //error propagation
				}
			}
		}
	return ERR_NONE;

}

//=========================================================================
/**
 * @brief Ask cache for a byte of data. Endianess: LITTLE.
 *
 * @param mem_space pointer to the memory space
 * @param p_addr pointer to a physical address
 * @param access to distinguish between fetching instructions and reading/writing data
 * @param l1_icache pointer to the beginning of L1 ICACHE
 * @param l1_dcache pointer to the beginning of L1 DCACHE
 * @param l2_cache pointer to the beginning of L2 CACHE
 * @param byte pointer to the byte to be returned
 * @param replace replacement policy
 * @return  error code
 */
int cache_read_byte(const void * mem_space, phy_addr_t * p_paddr, mem_access_t access,
					void * l1_cache,void * l2_cache,uint8_t * p_byte, cache_replace_t replace){
	M_REQUIRE_NON_NULL(mem_space);
	M_REQUIRE_NON_NULL(p_paddr);
	M_REQUIRE_NON_NULL(l1_cache);	
	M_REQUIRE_NON_NULL(l2_cache);	
	M_REQUIRE_NON_NULL(p_byte);	
	M_REQUIRE(replace == LRU, ERR_BAD_PARAMETER, "replace is not a valid instance of cache_replace_t %c", ' ');
	M_REQUIRE(access == INSTRUCTION || access == DATA, ERR_BAD_PARAMETER, "access is not a valid instance of mem_access_t %c", ' ');
						
	return ERR_NONE;
}

//=========================================================================
/**
 * @brief Change a word of data in the cache.
 *  Exclusive policy (see cache_read)
 *
 * @param mem_space pointer to the memory space
 * @param paddr pointer to a physical address
 * @param l1_cache pointer to the beginning of L1 DCACHE
 * @param l2_cache pointer to the beginning of L2 CACHE
 * @param word const pointer to the word of data that is to be written to the cache
 * @param replace replacement policy
 * @return error code
 */
int cache_write(void * mem_space,phy_addr_t * paddr, void * l1_cache,
                void * l2_cache,const uint32_t * word, cache_replace_t replace){
	M_REQUIRE_NON_NULL(mem_space);
	M_REQUIRE_NON_NULL(paddr);
	M_REQUIRE_NON_NULL(l1_cache);	
	M_REQUIRE_NON_NULL(l2_cache);	
	M_REQUIRE_NON_NULL(word);	
	M_REQUIRE(replace == LRU, ERR_BAD_PARAMETER, "replace is not a valid instance of cache_replace_t %c", ' ');
	return ERR_NONE;
	}

//=========================================================================
/**
 * @brief Write to cache a byte of data. Endianess: LITTLE.
 *
 * @param mem_space pointer to the memory space
 * @param paddr pointer to a physical address
 * @param l1_cache pointer to the beginning of L1 ICACHE
 * @param l2_cache pointer to the beginning of L2 CACHE
 * @param p_byte pointer to the byte to be returned
 * @param replace replacement policy
 * @return error code
 */
int cache_write_byte(void * mem_space, phy_addr_t * paddr, void * l1_cache,
                     void * l2_cache, uint8_t p_byte,cache_replace_t replace){
	M_REQUIRE_NON_NULL(mem_space);
	M_REQUIRE_NON_NULL(paddr);
	M_REQUIRE_NON_NULL(l1_cache);	
	M_REQUIRE_NON_NULL(l2_cache);	
	M_REQUIRE(replace == LRU, ERR_BAD_PARAMETER, "replace is not a valid instance of cache_replace_t %c", ' ');
	return ERR_NONE;					 
	}
