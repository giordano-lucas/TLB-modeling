#include "error.h"
#include "cache_mng.h"
#include "lru.h"
#include <inttypes.h>
#include <stdbool.h>
#include "addr_mng.h"
//=========================================================================
//=========================== HELPER FUNCTIONS ============================
/**
 * @brief convert a paddr_t into a uint32t
 */
#define phy_to_int(phy) ((uint32_t)(((phy)->phy_page_num << PAGE_OFFSET) | (phy)->page_offset))
/**
 * @brief extract the line_index from a phy_addr
 */
#define extract_line_index(phy_addr, CACHE_TYPE) (((phy_addr)/(sizeof(word_t)*(CACHE_TYPE ## _WORDS_PER_LINE))) % (CACHE_TYPE ## _LINES))
/**
 * @brief extract the word_index from a phy_addr
 */
#define extract_word_index(phy_addr, CACHE_TYPE) (((phy_addr)/(sizeof(word_t))) % (CACHE_TYPE ## _WORDS_PER_LINE))
/**
 * @brief cast an l1 void* entry to the type given by access
 */
#define cast_l1_entry(access, entry) ( ((access) == INSTRUCTION)? ((l1_icache_entry_t*)(entry)): ((l1_dcache_entry_t*)(entry)))
/**
 * @brief return the instance of cache_type_t given by access 
 */
#define to_l1_cache(access) (((access) == INSTRUCTION)?L1_ICACHE:L1_DCACHE)
/**
 * @brief compute address aligned on line_index (for memory accesses)
 */
#define compute_addr_line_aligned(phy_addr,CACHE_TYPE) (((phy_addr/sizeof(word_t))/(CACHE_TYPE ## _WORDS_PER_LINE))*(CACHE_TYPE ## _WORDS_PER_LINE))
/**
 * @brief function that accesses memory, simply copies a whole line of words from dest to src 
 */
void access_memory(const word_t* src, word_t* dest, uint8_t WORDS_PER_LINE){
	memcpy (dest, src, WORDS_PER_LINE*sizeof(word_t));
	}
/*******************************************************************/
/**
 * Macros defined as the ones in cache.h but that can be used with any cache name
 **/
 
// --------------------------------------------------
#define cache_cast_any(TYPE, CACHE) ((TYPE *)CACHE)

// --------------------------------------------------
#define cache_entry_any(TYPE, WAYS, LINE_INDEX, WAY, CACHE) \
        (cache_cast_any(TYPE, CACHE) + (LINE_INDEX) * (WAYS) + (WAY))

// --------------------------------------------------
#define cache_valid_any(TYPE, WAYS, LINE_INDEX, WAY, CACHE) \
        cache_entry_any(TYPE, WAYS, LINE_INDEX, WAY,CACHE)->v

// --------------------------------------------------
#define cache_age_any(TYPE, WAYS, LINE_INDEX, WAY, CACHE) \
        cache_entry_any(TYPE, WAYS, LINE_INDEX, WAY, CACHE)->age

// --------------------------------------------------
#define cache_tag_any(TYPE, WAYS, LINE_INDEX, WAY, CACHE) \
        cache_entry_any(TYPE, WAYS, LINE_INDEX, WAY, CACHE)->tag

// --------------------------------------------------
#define cache_line_any(TYPE, WAYS, LINE_INDEX, WAY, CACHE) \
        cache_entry_any(TYPE, WAYS, LINE_INDEX, WAY, CACHE)->line
// --------------------------------------------------

/**
 * @brief debug function and macro to print an entry 
 */
#define print_entry_generic(TYPE, entry) \
		fprintf(stderr, "Type : "#TYPE", Valid : %d, Age : %d, Tag : %"PRIx32", Data [", entry->v, entry->age, entry->tag); \
		for(int i = 0; i < L1_DCACHE_WORDS_PER_LINE; i++)										\
			fprintf(stderr, "%" PRIx32 ",", entry->line[i]);									\
		fprintf(stderr, "]\n");																	\

void print_entry(cache_t type, void* entry){
	switch(type){
		case L1_DCACHE: print_entry_generic(l1_dcache_entry_t,((l1_dcache_entry_t*) entry)); break;
		case L1_ICACHE: print_entry_generic(l1_icache_entry_t,((l1_icache_entry_t*) entry)); break;
		case L2_CACHE : print_entry_generic(l2_cache_entry_t, ((l2_cache_entry_t* ) entry)); break;
	}
}
//=========================================================================
/**
 * @brief Cleans a cache with type type
 *
 * @param type     : type of cache (l1_icache_entry_t, l1_dcache_entry_t, l2_cache_entry_t)
 * @param cache    : the (generic) pointer to the cache
 * @param CACHE_TYPE : type of cache (in caps like L1_DCACHE, L1_ICACHE, L2_CACHE)
 * 
 * it first test the overflow of sizeof(type)*NB_LINE*WAYS
 */
#define flush_generic(type, cache, CACHE_TYPE)                                                                   \
	 M_REQUIRE(((CACHE_TYPE ## _LINES) <= SIZE_MAX/(CACHE_TYPE ## _WAYS)), ERR_SIZE, "Could not memset : overflow, %c", " ");                    \
	 M_REQUIRE((((CACHE_TYPE ## _LINES)*(CACHE_TYPE ## _WAYS)) < SIZE_MAX/sizeof(type)), ERR_SIZE, "Could not memset : overflow, %c", " ");  /*overflow checks*/   \
	 memset(cache , 0, sizeof(type)*(CACHE_TYPE ## _LINES)*(CACHE_TYPE ## _WAYS));   /* Memsets the full cache to 0*/                                                      
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
        case L1_ICACHE : flush_generic(l1_icache_entry_t, cache, L1_ICACHE); break;
        case L1_DCACHE : flush_generic(l1_dcache_entry_t, cache, L1_DCACHE); break;
        case L2_CACHE  : flush_generic(l2_cache_entry_t , cache, L2_CACHE) ; break;
        default        : return ERR_BAD_PARAMETER; break;
    }
	
	return ERR_NONE;
	}
	
//=========================================================================
/**
 * @brief generic helper function for cache_hit
 * 
 * @param type               : type of cache (l1_icache_entry_t, l1_dcache_entry_t, l2_cache_entry_t)
 * @param CACHE_TYPE         : type of cache (in caps like L1_DCACHE, L1_ICACHE, L2_CACHE)
 */
#define cache_hit_generic(type, CACHE_TYPE)    \
	uint16_t line_index = extract_line_index(phy_addr, CACHE_TYPE);      \
	uint32_t tag = phy_addr >> (CACHE_TYPE ## _TAG_REMAINING_BITS);                                     \
	foreach_way(Way, CACHE_TYPE ## _WAYS) {  /*iterate on each way : if a cold start or a hit is*/    \
		                                              /*found  stop the execution */   \
		if (!cache_valid(type, CACHE_TYPE ## _WAYS, line_index, Way) ){/* found a place*/             \
			/*LRU_age_increase(type, WAYS, line_index, way);update*/                   \
			return ERR_NONE;                                                           \
			}                                                                          \
		else if (cache_tag(type, CACHE_TYPE ## _WAYS, line_index, Way) == tag){/*hit*/                \
			*p_line = cache_line(type,CACHE_TYPE ## _WAYS, line_index, Way); /*if hit, set way and index*/\
			*hit_way = Way;                                                            \
			*hit_index = line_index;                                                   \
			LRU_age_update(type, CACHE_TYPE ## _WAYS, Way,line_index);/*update ages*/                 \
			return ERR_NONE;                                                           \
		}                                                                              \
	}                                                                                  \
	/*if we arrive here no entry has been found*/                                      \
	return ERR_NONE;


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
	M_REQUIRE_NON_NULL(mem_space); //check all requirements
	M_REQUIRE_NON_NULL(cache);
	M_REQUIRE_NON_NULL(paddr);
	M_REQUIRE_NON_NULL(p_line);
	M_REQUIRE_NON_NULL(hit_way);
	M_REQUIRE_NON_NULL(hit_index);
	M_REQUIRE(L1_ICACHE <= cache_type && cache_type <= L2_CACHE, ERR_BAD_PARAMETER, "%d is not a valid cache_type \n", cache_type);
	*hit_way = HIT_WAY_MISS;//init to miss first and set to hit otherwise
	*hit_index = HIT_INDEX_MISS;
	*p_line = NULL;
	uint32_t phy_addr = phy_to_int(paddr);
	
	switch(cache_type){ //use the generic macro depending on the type of the cache
		case L1_ICACHE:{ cache_hit_generic(l1_icache_entry_t, L1_ICACHE);break;}
		case L1_DCACHE:{ cache_hit_generic(l1_dcache_entry_t, L1_DCACHE);break;}
		case L2_CACHE :{ cache_hit_generic(l2_cache_entry_t, L2_CACHE);break;}
		default: return ERR_BAD_PARAMETER;
	}
}

//=========================================================================
/**
 * @brief generic helper function for cache_insert
 * 
 * @param type               : type of cache (l1_icache_entry_t, l1_dcache_entry_t, l2_cache_entry_t)
 * @param CACHE_TYPE         : type of cache (in caps like L1_DCACHE, L1_ICACHE, L2_CACHE)
 */
#define cache_insert_generic(TYPE, CACHE_TYPE)                                                                              \
    /*verify that the way and lines are valid */                                                                                \
	M_REQUIRE(cache_way < (CACHE_TYPE ## _WAYS) && cache_line_index < (CACHE_TYPE ## _LINES), ERR_BAD_PARAMETER, "cache way and index have to match %c", ' '); \
	*cache_entry(TYPE, (CACHE_TYPE ## _WAYS), cache_line_index, cache_way ) = *(TYPE*)cache_line_in; /*sets the entry to be the entry given in argument*/ \
	return ERR_NONE;
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
	M_REQUIRE_NON_NULL(cache);//check requirements
	M_REQUIRE_NON_NULL(cache_line_in);
	M_REQUIRE(L1_ICACHE <= cache_type && cache_type <= L2_CACHE, ERR_BAD_PARAMETER, "cache type is not a valid value : %d", cache_type);
	
	switch (cache_type) { //use the generic macro depending on the type of the cache
         case L1_ICACHE : cache_insert_generic(l1_icache_entry_t, L1_ICACHE); break;
         case L1_DCACHE : cache_insert_generic(l1_dcache_entry_t, L1_DCACHE); break;
         case L2_CACHE  : cache_insert_generic(l2_cache_entry_t,  L2_CACHE) ; break;
         default        : return ERR_BAD_PARAMETER; break;
    }	
}

//=========================================================================
/**
 * initialize an entry with the given parameters
 */
#define cache_init_entry_with_param(entry, phy_addr, input_line, CACHE_TYPE)            \
	(entry)->v = 1;     /* valid when we insert*/                                                                     \
	(entry)->age = 0;   /* entry age is 0 when we insert*/                                                            \
	(entry)->tag = phy_addr >> (CACHE_TYPE ## _TAG_REMAINING_BITS);  /*sets the tag depending on the type of the cache*/         \
	/*copy content of input entry to entry*/                                                                          \
	memcpy((entry)->line, input_line, (CACHE_TYPE ## _WORDS_PER_LINE)*sizeof(word_t));
/**
 * @brief                          : generic helper function for cache_entry_init
 * @param type                     : type of cache
 * @param cache_entry              : entry to init
 * @param phy_addr                 : physical address, to extract the tag
 * @param CACHE_TYPE         : type of cache (in caps like L1_DCACHE, L1_ICACHE, L2_CACHE)
 * @param mem_space                : starting address of the memory space
 */
#define cache_entry_init_generic(type, cache_entry, phy_addr, CACHE_TYPE, mem_space) {     				 \
	type* entry = cache_entry;                                                                                             				 \
	uint32_t addr =  compute_addr_line_aligned(phy_addr,CACHE_TYPE) ;    /*get the right word addressed line*/			     \
	cache_init_entry_with_param(entry, phy_addr,  ((word_t*)(mem_space) + addr), CACHE_TYPE);              \
}
 
// ========================================================================
/**
 * @brief Initialize a cache entry (write to the cache entry for the first time)
 *
 * @param mem_space starting address of the memory space, must be non null
 * @param paddr pointer to physical address, to extract the tag, must be non null
 * @param cache_entry pointer to the entry to be initialized, must be non null
 * @param cache_type to distinguish between different caches
 * @return  error code
 * 
 */
int cache_entry_init(const void * mem_space, const phy_addr_t * paddr,void * cache_entry,cache_t cache_type){
	M_REQUIRE_NON_NULL(mem_space); //basic checks
	M_REQUIRE_NON_NULL(paddr);
	M_REQUIRE_NON_NULL(cache_entry);
	M_REQUIRE((L1_ICACHE<=cache_type)&&(cache_type <= L2_CACHE),ERR_BAD_PARAMETER,"Wrong instance of cache_type %c",' '); //ctype has to be a part of the enum
	
	uint32_t phy_addr = phy_to_int(paddr);
	switch (cache_type) { //use the generic macro depending on the type of the cache
		case L1_ICACHE : cache_entry_init_generic(l1_icache_entry_t, cache_entry, phy_addr, L1_ICACHE, mem_space) ; break;
		case L1_DCACHE : cache_entry_init_generic(l1_dcache_entry_t, cache_entry, phy_addr, L1_DCACHE, mem_space) ; break;
		case L2_CACHE  : cache_entry_init_generic(l2_cache_entry_t,  cache_entry, phy_addr, L2_CACHE , mem_space) ; break;
		default: return ERR_BAD_PARAMETER; break; //should not arrive here
	}
	return ERR_NONE;
	}

//=========================================================================
//======================== helper functions cache read ====================
//=========================================================================
/*flag to say that no empty way has been found */
#define NOTHING_FOUND (-1)
/**
 * @brief the generic funcion of find_empty_slot
 * 
 * @param type               : type of cache (l1_icache_entry_t, l1_dcache_entry_t, l2_cache_entry_t)
 * @param cache : the cache where we need to find the empty slot
 * @param CACHE_TYPE         : type of cache (in caps like L1_DCACHE, L1_ICACHE, L2_CACHE)
 * @param line_index : index of the cache line
 */
#define find_empty_slot_generic(TYPE, cache, LINE_INDEX, CACHE_TYPE)    {                                           \
	foreach_way(way, (CACHE_TYPE ## _WAYS)){    /*simply iterate on each way and stop as soon as a invalid cache entry is found*/    \
		if (!cache_valid(TYPE, (CACHE_TYPE ## _WAYS), LINE_INDEX, way)) {return way;}                                                \
		}                                                                                                           \
		return NOTHING_FOUND;                                                                                       \
	}        
/**
 * @brief function that find an empty slot (way) in the cache at the given line or return NOTHING_FOUND
 * 
 * @param cache_type : type of cache
 * @param cache : the cache where we need to find the empty slot
 * @param line_index : index of the cache line
 */
int find_empty_slot(cache_t cache_type, void* cache, uint16_t line_index){
	switch (cache_type){ //use the generic macro depending on the type of the cache
		case L1_ICACHE: find_empty_slot_generic(l1_icache_entry_t, cache, line_index, L1_ICACHE);break;
		case L1_DCACHE: find_empty_slot_generic(l1_dcache_entry_t, cache, line_index, L1_DCACHE);break;
		case L2_CACHE : find_empty_slot_generic(l2_cache_entry_t , cache, line_index, L2_CACHE) ;break;
		default: return NOTHING_FOUND;break;
		}
	} 
/**
 * @brief generic helper function for evict
 * @param TYPE : type of cache
 * @param LINE_INDEX : index of the line of the cache where we want to evict
 * @param CACHE_TYPE         : type of cache (in caps like L1_DCACHE, L1_ICACHE, L2_CACHE)
 * @param returns the evicted entry or NULL in case of an error
 */                                                                                           
#define evict_generic(TYPE, LINE_INDEX, CACHE_TYPE)    {                                                       \
	unsigned int max_age = 0 ;                                                                           \
	TYPE* entry_to_evict = NULL;                                                                         \
	foreach_way(way, CACHE_TYPE ## _WAYS){ /*iterate on every way to find the entry with max age (LRU)*/                \
		TYPE* entry = cache_entry(TYPE, CACHE_TYPE ## _WAYS, LINE_INDEX, way);                                          \
		if (entry->v && entry->age >= max_age) { /*get the entry with the biggest age*/                  \
		    max_age = entry->age;                                                                        \
		    entry_to_evict = entry;                                                                      \
			}                                                                                            \
		}                                                                                                \
		if (entry_to_evict != NULL) entry_to_evict->v = 0; /*entry_to_evict should always be != NULL in practice*/   \
		return entry_to_evict;                                                                           \
	}
/**
 * @brief : function that evicts an entry of the given cache at line_index and returns it (with LRU policy)
 * @param cache_type : type of cache
 * @param cache      : cache from which we want to evict 
 * @param line_index : index of the line 
 * @param returns the evicted entry or NULL in case of an error
 */
void* evict(cache_t cache_type, void* cache, uint16_t line_index) {
	switch (cache_type){ //use the generic macro depending on the type of the cache
		case L1_ICACHE: evict_generic(l1_icache_entry_t, line_index, L1_ICACHE);break;
		case L1_DCACHE: evict_generic(l1_dcache_entry_t, line_index, L1_DCACHE);break;
		case L2_CACHE : evict_generic(l2_cache_entry_t , line_index, L2_CACHE) ;break;
		default: return NULL;break;
		}
	}
/**
 * @brief this function updates the ages of the given cache 
 * 
 * @param cache_type : type of cache 
 * @param cache      : pointer to the cache that needs to be updated
 * @param way        : way where a place has been found to put the line or NOTHING_FOUND if no place has been found
 * @param line_index : index of the line to be updated
 */
void modify_ages(cache_t cache_type,void *cache, uint8_t Way, uint16_t Line_index,bool isColdStart){
	switch (cache_type){
		case L1_ICACHE: if (!isColdStart) {LRU_age_update(l1_icache_entry_t, L1_ICACHE_WAYS, Way, Line_index) }else {LRU_age_increase(l1_icache_entry_t, L1_ICACHE_WAYS, Way, Line_index);}break;
		case L1_DCACHE: if (!isColdStart) {LRU_age_update(l1_dcache_entry_t, L1_DCACHE_WAYS, Way, Line_index) }else {LRU_age_increase(l1_dcache_entry_t, L1_DCACHE_WAYS, Way, Line_index);}break;
		case L2_CACHE : if (!isColdStart) {LRU_age_update(l2_cache_entry_t , L2_CACHE_WAYS , Way, Line_index) }else {LRU_age_increase(l2_cache_entry_t , L2_CACHE_WAYS , Way, Line_index);}break;
		default         : fprintf(stderr, "wrong instance of mem access at modify ages"); break;
		}
	}

/**
 * @brief          : function that inserts an entry in l2 cache
 * @param cache    : L2 cache where we want to insert entry
 * @param entry    : entry to insert
 * @param phy_addr : physical address
 * @return         : error code
 */
int insert_level2(l2_cache_entry_t* cache, l2_cache_entry_t* entry, uint32_t phy_addr){
	uint16_t line_index = extract_line_index(phy_addr, L2_CACHE);
	int err = ERR_NONE;                                                 //err propagation
	bool isColdStart = true;
	int cache_way = find_empty_slot(L2_CACHE, cache, line_index);       //find a place
	if (cache_way == NOTHING_FOUND){                                    // there is no empty slot in l2 cache => evict
		l2_cache_entry_t* evicted = evict(L2_CACHE, cache, line_index); //eviction
		if (evicted == NULL) return ERR_MEM;                            // error propagation
		entry->age = evicted->age;                                      // prepare for modify_ages
		isColdStart = false;                                            // if we evict an entry it is no longer a cold start
		cache_way = find_empty_slot(L2_CACHE, cache, line_index);       //update cache_way with the way where evicted entry was
		}
	 //insert in l2 cache (here we are sure that there is at least an empt way)
	if ((err = cache_insert(line_index,cache_way,entry,cache, L2_CACHE))!= ERR_NONE) return err;//error propagation
	modify_ages(L2_CACHE, cache, cache_way, line_index, isColdStart);//update ages
	return ERR_NONE;
	}

/**
 * constant used in the L1-L2 convertion entry in case of an eviction from L1
 */
#define SHIFT_BITS_CAST_TAG_L1_TO_L2 3
/**
 * = TAG_REMAINING_BITS - LINE_REMAINING_BITS and is always equals to 4 for all caches
 */
#define TAG_AND_LINE_INDEX_REMAINING_BITS 4
/**
 * @brief : fonction used to recompute old phyaddr for an entry that is evicted from l1 and pushed to l2
 * @param tag : tag of the L1 entry
 * @param CACHE_TYPE         : type of cache (in caps like L1_DCACHE, L1_ICACHE, L2_CACHE)
 * @param index : index of the line where the entry has been placed in  L1
 */
#define recomputeOldPhyAddr(tag, CACHE_TYPE, index) ((((uint32_t)(tag))<<(CACHE_TYPE ## _TAG_REMAINING_BITS)) | (((uint32_t)(index))<< TAG_AND_LINE_INDEX_REMAINING_BITS))
/**
 * @brief          : function that inserts a L1 entry into l1_cache (and deal with eviction if needed)
 * @param access   : to distinguish between fetching instructions and reading/writing data
 * @param l1_cache : pointer to the beginning of L1 CACHE
 * @param l2_cache : pointer to the beginning of L2 CACHE
 * @param entry    : pointer to the entry to be inserted in L1_cache
 * @param phy_addr : physical address
 * @return         : error code
 */
int insert_level1(mem_access_t access,void * l1_cache, void * l2_cache, void* entry, uint32_t phy_addr){
	
	int err = ERR_NONE;
	cache_t cache_type = (access == INSTRUCTION) ? L1_ICACHE: L1_DCACHE;
	bool isColdStart = true;
	uint16_t line_index = (access == INSTRUCTION) ? extract_line_index(phy_addr, L1_ICACHE): extract_line_index(phy_addr, L1_DCACHE);
	int cache_way = find_empty_slot(cache_type, l1_cache, line_index); //find a place
	if (cache_way == NOTHING_FOUND){// there is no empty slot in l1 cache => evict an entry and move it to L2
		void* evicted_entry = evict(cache_type, l1_cache, line_index); //eviction
		if (evicted_entry == NULL) return ERR_MEM; // error propagation 
		uint32_t oldPhyAddr = (access == INSTRUCTION)? recomputeOldPhyAddr(cast_l1_entry(access,evicted_entry)->tag, L1_ICACHE, line_index ) : recomputeOldPhyAddr(cast_l1_entry(access,evicted_entry)->tag, L1_DCACHE, line_index );
		l2_cache_entry_t l2_entry; // cast evicted_entry to an l2 entry
		cache_init_entry_with_param(&l2_entry, oldPhyAddr, cast_l1_entry(access,evicted_entry)->line, L2_CACHE);
		// move entry to level 2
		if ((err = insert_level2((l2_cache_entry_t*)l2_cache, &l2_entry, oldPhyAddr))!= ERR_NONE) return err; //error propagation
		cast_l1_entry(access,entry)->age = cast_l1_entry(access, evicted_entry)->age; //prepare for modifying ages policy 
		isColdStart = false; // if we evict an entry it is no longer a cold start
		cache_way = find_empty_slot(cache_type, l1_cache, line_index); //update cache_way with the way where evicted entry was
		}
    //insert in l1 cache (here we are sure that there is at least an empt way)
    if ((err = cache_insert(line_index,cache_way, cast_l1_entry(access,entry), l1_cache,cache_type))!= ERR_NONE) return err; // error propagation
	modify_ages(cache_type,l1_cache, cache_way,line_index, isColdStart);//update ages
	return ERR_NONE;
	} 
//====================================================================================================
/**
 * @brief this the generic function corresponding to move_entry_to_level1
 * 
 * @param access             : used to distinguish between fetching instructions and reading/writing data
 * @param type               : type of cache entry (l1_icache_entry_t, l1_dcache_entry_t, l2_cache_entry_t)
 * @param l1_cache           : pointer to the beginning of L1 CACHE
 * @param l2_cache           : pointer to the beginning of L2 CACHE
 * @param l2_entry           : the entry that needs to be moved from l2 to l1
 * @param phy_addr           : the physical address
 * @param CACHE_TYPE         : type of cache (in caps like L1_DCACHE, L1_ICACHE, L2_CACHE)
 * @return error code
 */
#define move_entry_to_level1_generic(access, type, l2_entry, l1_cache, l2_cache, phy_addr, CACHE_TYPE)     \
	type l1_entry;    /*create L1 entry for casting form l2_entry*/                                                \
	/* cast it */                                                                                                  \
	cache_init_entry_with_param(&l1_entry, phy_addr, l2_entry->line, CACHE_TYPE)                                    \
	/* insert new entry in l1_cache and if needed, do the error propagation*/                                      \
	int err = ERR_NONE; 																					  	   \
	(l2_entry)->v = 0;      /*invalidate l2_entry*/                                                                \
	if ((err = insert_level1(access,l1_cache, l2_cache, &l1_entry, phy_addr))!= ERR_NONE) return err; 

/**
 * @brief this function moves an entry from l2 cache to l1 cache
 * 
 * @param access   :used to distinguish between fetching instructions and reading/writing data
 * @param l1_cache :pointer to the beginning of L1 CACHE
 * @param l2_cache : pointer to the beginning of L2 CACHE
 * @param l2_entry : the entry that needs to be moved from l2 to l1
 * @param phy_addr :the physical address
 * @return error code
 */
int move_entry_to_level1(mem_access_t access,void * l1_cache, void * l2_cache, l2_cache_entry_t* l2_entry, const uint32_t phy_addr){
	switch(access){ //use the generic macro depending on the type of the cache
		case INSTRUCTION: {move_entry_to_level1_generic(access, l1_icache_entry_t, l2_entry,l1_cache, l2_cache, phy_addr, L1_ICACHE);}break;
		case DATA       : {move_entry_to_level1_generic(access, l1_dcache_entry_t, l2_entry,l1_cache, l2_cache, phy_addr, L1_DCACHE);}break;
		default: return ERR_BAD_PARAMETER; break;
		}

	return ERR_NONE;
	}
// ========================================================================
/**
 * @brief generic helper function for search_in_memory_and_affect
 * 
 * @param type               : type of cache entry (l1_icache_entry_t, l1_dcache_entry_t, l2_cache_entry_t)
 * @param cache_type         : corresponds to an instance of the enum cache_type_t (L1_DCACHE_...)
 * @param CACHE_TYPE         : type of cache (in caps like L1_DCACHE, L1_ICACHE, L2_CACHE)
 */
#define search_in_memory_and_affect_generic(type, CACHE_TYPE) {                                           \
	type entry;                                                                                                                                         \
	if ((err = cache_entry_init(mem_space,paddr,&entry,CACHE_TYPE))!= ERR_NONE) return err; /*error propagation, init the entry from memory*/           \
	if ((err = insert_level1(access,l1_cache, l2_cache, &entry,phy_addr))!= ERR_NONE) return err; /*error propagation, insert the entry in memory*/     \
	*word = entry.line[extract_word_index(phy_addr,CACHE_TYPE)]; /*sets the word using the entry*/                                                  \
	}
/**
 * @brief search the cache line in memory and insert it in level 1 cache and affect word
 * 
 * @param access to distinguish between fetching instructions and reading/writing data
 * @param word pointer to the word of data that is returned by cache
 * @param mem_space pointer to the memory space
 * @param paddr pointer to a physical address
 * @param phy_addr : phy_addr casted in uint32
 * @param l1_cache pointer to the beginning of L1 CACHE
 * @param l2_cache pointer to the beginning of L2 CACHE
 */
int search_in_memory_and_affect(mem_access_t access, uint32_t * word, phy_addr_t * paddr, uint32_t phy_addr, const void* mem_space, void * l1_cache, void * l2_cache){
	int err = ERR_NONE;
	switch (access){
		case INSTRUCTION: search_in_memory_and_affect_generic(l1_icache_entry_t, L1_ICACHE);break;
		case DATA       : search_in_memory_and_affect_generic(l1_dcache_entry_t, L1_DCACHE);break;
		default         : return ERR_BAD_PARAMETER; break;
		}
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
	M_REQUIRE((paddr->page_offset % sizeof(word_t)) == 0, ERR_BAD_PARAMETER, "paddr should be word aligned for cache_read  %c",' ');

	int err = ERR_NONE; // used to propagate errors
	uint32_t phy_addr = phy_to_int(paddr);
	const uint32_t * p_line = 0;
	uint8_t hit_way = 0;
	uint16_t hit_index = 0;
	
	if ((err = cache_hit(mem_space, l1_cache, paddr,&p_line,&hit_way,&hit_index, to_l1_cache(access))) != ERR_NONE) return err;//error handling, check if word is in l1
	if  (hit_way != HIT_WAY_MISS){//if found found in level 1 nothing to be done, just affect word
		*word = p_line[(access == INSTRUCTION) ? extract_word_index(phy_addr, L1_ICACHE) : extract_word_index(phy_addr, L1_DCACHE) ]; //set word since we found it in either l1_i or l1_d
		}
	else{//not found in l1 => search in l2
		if((err = cache_hit(mem_space, l2_cache, paddr,&p_line,&hit_way,&hit_index, L2_CACHE)) != ERR_NONE) return err; //check if it is in l2, error propagate
		if (hit_way != HIT_WAY_MISS) { // found in level 2 => move entry to level 1 and affect word
			*word = p_line[extract_word_index(phy_addr,L2_CACHE)];
			if ((err = move_entry_to_level1(access,l1_cache, l2_cache, cache_entry_any(l2_cache_entry_t, L2_CACHE_WAYS, hit_index, hit_way, l2_cache), phy_addr))!= ERR_NONE){return err;}//error propagation
			}
		else { // not found in L2 => search in memory
			search_in_memory_and_affect(access, word, paddr, phy_addr, mem_space, l1_cache, l2_cache);
			}
		}
	return ERR_NONE;

}
//=========================================================================
/**
 * /!\ used for read_bytes and write_bytes
 * 
 * @brief : function that compute word aligned p_addr and cast the resulting word into a byte_t pointer
 * @param ACCESS : memory access, DATA or INSTRUCTION
 * @param P_ADDR : P_ADDR : physical address that needs to be word_aligned
 */
#define cache_read_deal_with_bytes(ACCESS, P_ADDR) {                                                               \
	byte_index = P_ADDR->page_offset % sizeof(word_t);  /*index of the byte inside of the word*/                   \
	phy_addr = *P_ADDR;                                 /*value of paddr from pointer*/                            \
	phy_addr.page_offset -= byte_index;                 /*remove the byte index to use cache read*/                \
	if ((err = cache_read(mem_space,&phy_addr, ACCESS,l1_cache, l2_cache, &word, replace)) != ERR_NONE) return err;\
													    /*error propagation, read word from memory*/               \
	word_as_byte = (byte_t*)&word;                      /*cast to byte to get the byte we want only*/              \
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
	M_REQUIRE_NON_NULL(mem_space); //basic checks
	M_REQUIRE_NON_NULL(p_paddr);
	M_REQUIRE_NON_NULL(l1_cache);	
	M_REQUIRE_NON_NULL(l2_cache);	
	M_REQUIRE_NON_NULL(p_byte);	
	M_REQUIRE(replace == LRU, ERR_BAD_PARAMETER, "replace is not a valid instance of cache_replace_t %c", ' ');
	M_REQUIRE(access == INSTRUCTION || access == DATA, ERR_BAD_PARAMETER, "access is not a valid instance of mem_access_t %c", ' ');
	int err = ERR_NONE; //used for error propagation
	byte_t* word_as_byte = NULL;
	int byte_index = 0;
	word_t word = 0;                                         //word that we will read from cache
	phy_addr_t phy_addr;
	cache_read_deal_with_bytes (access, p_paddr);
	*p_byte = word_as_byte[byte_index];                      //get the byte we want
	
	return ERR_NONE;
}
//=========================================================================
/**
 * @brief Writes a whole line to memory at the address given by the phy_addr
 *
 * @param mem_space pointer to the memory space
 * @param paddr uint corresponding to a physical address
 * @param line pointer to the line to write to the memory
 * @return nothing (void)
 */
void write_memory(void * mem_space, uint32_t phy_addr, word_t* line) {
	uint32_t addr = compute_addr_line_aligned(phy_addr,L1_DCACHE);//gets the word addressed phy addr and sets memory to line, assume number of words per line is the same in all caches as said on moodle
	access_memory(line, (word_t*)(mem_space) + addr, L1_DCACHE_WORDS_PER_LINE);  //let's assume that WORDS_PER_LINE*sizeof(word_t) is not going to overflow
	}
	
/**
 * @brief Reads a whole line in memory at the address given by the phy_addr
 *
 * @param mem_space pointer to the memory space
 * @param paddr uint corresponding to a physical address
 * @param line pointer to the line to set after reading the memory
 * @return nothing (void)
 */
void read_memory(void * mem_space, uint32_t phy_addr, word_t* line) {
	uint32_t addr = compute_addr_line_aligned(phy_addr,L2_CACHE); //gets the word addressed phy addr and sets line using memory, assume number of words per line is the same in all caches as said on moodle  
	access_memory((word_t*)(mem_space) + addr, line, L2_CACHE_WORDS_PER_LINE);   //let us assume that WORDS_PER_LINE*sizeof(word_t) is not going to overflow
	}

//================================================================================================
//================================== helper functions for write ===================================
/**
 * @brief : update word in line and write back line to memory
 */
#define set_word_and_write_back(input_line, CACHE_TYPE) {                                         \
	input_line[extract_word_index(phy_addr,CACHE_TYPE)] = *word;/*update word*/                   \
	write_memory(mem_space, phy_addr,input_line);                       /*update memory */            \
	}
/**
 * @brief : read line from cache, update it, write back to memory, insert it back to cache and update ages
 * @param TYPE           : type of cache
 * @param CACHE_TYPE     : instance of cache_t 
 * @param CACHE          : instance of the cache
 */
#define read_modifyLine_insert_updateAges_writeBackInMemory(TYPE, CACHE_TYPE, CACHE) {                           \
	 TYPE entry;                                                         /*new entry*/                                                    \
	 entry = *cache_entry_any(TYPE, (CACHE_TYPE ## _WAYS), hit_index, hit_way, CACHE); /*get the entry corresponding (with deep copy)*/                 \
	 set_word_and_write_back(entry.line,CACHE_TYPE);                                                                                 \
	 if ((err = cache_insert(hit_index, hit_way, &entry, CACHE, CACHE_TYPE))!= ERR_NONE) return err; /*insert back in CACHE and error propagation if needed*/      \
	 modify_ages(CACHE_TYPE, CACHE, hit_way, hit_index, false);          /*modify ages accordingly (since its a hit its not cold start)*/ \
	}
//================================================================================================
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
	M_REQUIRE_NON_NULL(mem_space);//basic checks
	M_REQUIRE_NON_NULL(paddr);
	M_REQUIRE_NON_NULL(l1_cache);	
	M_REQUIRE_NON_NULL(l2_cache);	
	M_REQUIRE_NON_NULL(word);	
	M_REQUIRE((paddr->page_offset % sizeof(word_t)) == 0, ERR_BAD_PARAMETER, "paddr should be word aligned for cache_read  %c",' ');
	M_REQUIRE(replace == LRU, ERR_BAD_PARAMETER, "replace is not a valid instance of cache_replace_t %c", ' ');
	int err = ERR_NONE; // used to propagate errors
	uint32_t phy_addr = phy_to_int(paddr); //get the uint32 corresponding to the paddr given
	const uint32_t * p_line = 0;//values that we use for using cache hit
	uint8_t hit_way = 0;
	uint16_t hit_index = 0;
	
	if ((err = cache_hit(mem_space, l1_cache, paddr,&p_line,&hit_way,&hit_index, L1_DCACHE)) != ERR_NONE) return err;//error handling, check if we have a valid entry corresponding
	if  (hit_way != HIT_WAY_MISS){//if found found in level 1
		  read_modifyLine_insert_updateAges_writeBackInMemory(l1_dcache_entry_t, L1_DCACHE, l1_cache);
		}
	else{//not found in l1 => search in l2
		if((err = cache_hit(mem_space, l2_cache, paddr,&p_line,&hit_way,&hit_index, L2_CACHE)) != ERR_NONE) return err; //error handling, check if we have an entry in l2
		if (hit_way != HIT_WAY_MISS) { // found in level 2 => find line, update ages,  move entry to level 1 , update memory
			read_modifyLine_insert_updateAges_writeBackInMemory(l2_cache_entry_t, L2_CACHE, l2_cache);	
			if ((err = move_entry_to_level1(DATA,l1_cache, l2_cache, cache_entry_any(l2_cache_entry_t, L2_CACHE_WAYS, hit_index, hit_way, l2_cache), phy_addr))!= ERR_NONE){return err;}//error propagation, insert in l1
			}
		else { // not found in L2 => search in memory
			word_t line[L1_ICACHE_WORDS_PER_LINE];               //init line to read from memory
			read_memory(mem_space, phy_addr, line);              // read line from memory
			set_word_and_write_back(line, L1_DCACHE);
			l1_dcache_entry_t entry;
			if ((err = cache_entry_init(mem_space, paddr,&entry,L1_DCACHE))!= ERR_NONE) return err; //init entry from memory (gets everything from mem)
			if((err = insert_level1(DATA, l1_cache, l2_cache, &entry, phy_to_int(paddr))) != ERR_NONE) return err; //insert entry to l1
			}
		}
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
 * @param p_byte pointer to the byte to be written
 * @param replace replacement policy
 * @return error code
 */
int cache_write_byte(void * mem_space, phy_addr_t * paddr, void * l1_cache,
                     void * l2_cache, uint8_t p_byte,cache_replace_t replace){
	M_REQUIRE_NON_NULL(mem_space); //basic checks
	M_REQUIRE_NON_NULL(paddr);
	M_REQUIRE_NON_NULL(l1_cache);	
	M_REQUIRE_NON_NULL(l2_cache);	
	M_REQUIRE(replace == LRU, ERR_BAD_PARAMETER, "replace is not a valid instance of cache_replace_t %c", ' ');
	int err = ERR_NONE; //for error propagation
	word_t word = 0; //word that will be read from cache
	int byte_index = 0;
	byte_t* word_as_byte = NULL;
	phy_addr_t phy_addr;
	cache_read_deal_with_bytes(DATA, paddr);
	
	word_as_byte[byte_index] = p_byte;  //set the byte wanted inside the word
	if ((err = cache_write(mem_space,&phy_addr,l1_cache, l2_cache, &word, replace)) != ERR_NONE) return err;//error propagation, write word back in memory
	return ERR_NONE;					 
	}

//=========================================================================
#define PRINT_CACHE_LINE(OUTFILE, TYPE, WAYS, LINE_INDEX, WAY, WORDS_PER_LINE) \
    do { \
            fprintf(OUTFILE, "V: %1" PRIx8 ", AGE: %1" PRIx8 ", TAG: 0x%03" PRIx16 ", values: ( ", \
                        cache_valid(TYPE, WAYS, LINE_INDEX, WAY), \
                        cache_age(TYPE, WAYS, LINE_INDEX, WAY), \
                        cache_tag(TYPE, WAYS, LINE_INDEX, WAY)); \
            for(int i_ = 0; i_ < WORDS_PER_LINE; i_++) \
                fprintf(OUTFILE, "0x%08" PRIx32 " ", \
                        cache_line(TYPE, WAYS, LINE_INDEX, WAY)[i_]); \
            fputs(")\n", OUTFILE); \
    } while(0)

#define PRINT_INVALID_CACHE_LINE(OUTFILE, TYPE, WAYS, LINE_INDEX, WAY, WORDS_PER_LINE) \
    do { \
            fprintf(OUTFILE, "V: %1" PRIx8 ", AGE: -, TAG: -----, values: ( ---------- ---------- ---------- ---------- )\n", \
                        cache_valid(TYPE, WAYS, LINE_INDEX, WAY)); \
    } while(0)

#define DUMP_CACHE_TYPE(OUTFILE, TYPE, WAYS, LINES, WORDS_PER_LINE)  \
    do { \
        for(uint16_t index = 0; index < LINES; index++) { \
            foreach_way(way, WAYS) { \
                fprintf(output, "%02" PRIx8 "/%04" PRIx16 ": ", way, index); \
                if(cache_valid(TYPE, WAYS, index, way)) \
                    PRINT_CACHE_LINE(OUTFILE, const TYPE, WAYS, index, way, WORDS_PER_LINE); \
                else \
                    PRINT_INVALID_CACHE_LINE(OUTFILE, const TYPE, WAYS, index, way, WORDS_PER_LINE);\
            } \
        } \
    } while(0)

//=========================================================================
// see cache_mng.h
int cache_dump(FILE* output, const void* cache, cache_t cache_type)
{
    M_REQUIRE_NON_NULL(output);
    M_REQUIRE_NON_NULL(cache);

    fputs("WAY/LINE: V: AGE: TAG: WORDS\n", output);
    switch (cache_type) { //use the generic macro depending on the type of the cache
    case L1_ICACHE:
        DUMP_CACHE_TYPE(output, l1_icache_entry_t, L1_ICACHE_WAYS,
                        L1_ICACHE_LINES, L1_ICACHE_WORDS_PER_LINE);
        break;
    case L1_DCACHE:
        DUMP_CACHE_TYPE(output, l1_dcache_entry_t, L1_DCACHE_WAYS,
                        L1_DCACHE_LINES, L1_DCACHE_WORDS_PER_LINE);
        break;
    case L2_CACHE:
        DUMP_CACHE_TYPE(output, l2_cache_entry_t, L2_CACHE_WAYS,
                        L2_CACHE_LINES, L2_CACHE_WORDS_PER_LINE);
        break;
    default:
        debug_print("%d: unknown cache type", cache_type);
        return ERR_BAD_PARAMETER;
    }
    putc('\n', output);

    return ERR_NONE;
}
