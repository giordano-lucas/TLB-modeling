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
#define extract_line_index(phy_addr, WORDS_PER_LINE, NB_LINES) (((phy_addr)/(sizeof(word_t)*(WORDS_PER_LINE))) % (NB_LINES))
/**
 * @brief extract the word_index from a phy_addr
 */
#define extract_word_index(phy_addr, WORDS_PER_LINE) (((phy_addr)/(sizeof(word_t))) % (WORDS_PER_LINE))
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
#define compute_addr_line_aligned(phy_addr,WORDS_PER_LINE) (((phy_addr/sizeof(word_t))/WORDS_PER_LINE)*WORDS_PER_LINE)
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
 * @param NB_LINES : the number of lines of the cache
 * @param WAYS     : the number of ways of the cache
 * 
 * it first test the overflow of sizeof(type)*NB_LINE*WAYS
 */
#define flush_generic(type, cache, NB_LINES, WAYS)                                                                   \
	 M_REQUIRE(((NB_LINES) <= SIZE_MAX/(WAYS)), ERR_SIZE, "Could not memset : overflow, %c", " ");                    \
	 M_REQUIRE((((NB_LINES)*(WAYS)) < SIZE_MAX/sizeof(type)), ERR_SIZE, "Could not memset : overflow, %c", " ");  /*overflow checks*/   \
	 memset(cache , 0, sizeof(type)*(NB_LINES)*(WAYS));   /* Memsets the full cache to 0*/                                                      
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
 * @brief generic helper function for cache_hit
 * 
 * @param type               : type of cache (l1_icache_entry_t, l1_dcache_entry_t, l2_cache_entry_t)
 * @param NB_LINES           : the number of lines of the cache
 * @param WORDS_PER_LINE     : the number of words per line of the type cache
 * @param WAYS               : the number of ways of the cache
 * @param TAG_REMAINING_BITS : remaining bits for the tag of the p_addr
 */
#define cache_hit_generic(type, NB_LINES, WORDS_PER_LINE, TAG_REMAINING_BITS, WAYS)    \
	uint16_t line_index = extract_line_index(phy_addr, WORDS_PER_LINE, NB_LINES);      \
	uint32_t tag = phy_addr >> TAG_REMAINING_BITS;                                     \
	foreach_way(Way, WAYS) {  /*iterate on each way : if a cold start or a hit is*/    \
		                                              /*found  stop the execution */   \
		if (!cache_valid(type, WAYS, line_index, Way) ){/* found a place*/             \
			/*LRU_age_increase(type, WAYS, line_index, way);update*/                   \
			return ERR_NONE;                                                           \
			}                                                                          \
		else if (cache_tag(type, WAYS, line_index, Way) == tag){/*hit*/                \
			*p_line = cache_line(type, WAYS, line_index, Way); /*if hit, set way and index*/\
			*hit_way = Way;                                                            \
			*hit_index = line_index;                                                   \
			LRU_age_update(type, WAYS, Way,line_index);/*update ages*/                 \
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
		case L1_ICACHE:{ cache_hit_generic(l1_icache_entry_t, L1_ICACHE_LINES, L1_ICACHE_WORDS_PER_LINE, L1_ICACHE_TAG_REMAINING_BITS, L1_ICACHE_WAYS);break;}
		case L1_DCACHE:{ cache_hit_generic(l1_dcache_entry_t, L1_DCACHE_LINES, L1_DCACHE_WORDS_PER_LINE, L1_DCACHE_TAG_REMAINING_BITS, L1_DCACHE_WAYS);break;}
		case L2_CACHE :{ cache_hit_generic(l2_cache_entry_t, L2_CACHE_LINES, L2_CACHE_WORDS_PER_LINE, L2_CACHE_TAG_REMAINING_BITS, L2_CACHE_WAYS);break;}
		default: return ERR_BAD_PARAMETER;
	}
}

//=========================================================================
#define cache_insert_generic(TYPE, WAYS, NB_LINES)                                                                              \
    /*verify that the way and lines are valid */                                                                                \
	M_REQUIRE(cache_way < WAYS && cache_line_index < NB_LINES, ERR_BAD_PARAMETER, "cache way and index have to match %c", ' '); \
	*cache_entry(TYPE,WAYS, cache_line_index, cache_way ) = *(TYPE*)cache_line_in; /*sets the entry to be the entry given in argument*/ \
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
         case L1_ICACHE : cache_insert_generic(l1_icache_entry_t, L1_ICACHE_WAYS, L1_ICACHE_LINES); break;
         case L1_DCACHE : cache_insert_generic(l1_dcache_entry_t, L1_DCACHE_WAYS, L1_DCACHE_LINES); break;
         case L2_CACHE  : cache_insert_generic(l2_cache_entry_t,  L2_CACHE_WAYS , L2_CACHE_LINES) ; break;
         default        : return ERR_BAD_PARAMETER; break;
    }	
}

//=========================================================================
/**
 * initilize an entry with the given parameters
 */
#define cache_init_entry_with_param(entry, phy_addr, input_line, WORDS_PER_LINE, CACHE_TAG_REMAINING_BITS)            \
	(entry)->v = 1;     /* valid when we insert*/                                                                     \
	(entry)->age = 0;   /* entry age is 0 when we insert*/                                                            \
	(entry)->tag = phy_addr >> CACHE_TAG_REMAINING_BITS;  /*sets the tag depending on the type of the cache*/         \
	/*copy content of input entry to entry*/                                                                          \
	memcpy((entry)->line, input_line, WORDS_PER_LINE*sizeof(word_t));
/**
 * @brief                          : generic helper function for cache_entry_init
 * @param type                     : type of cache
 * @param cache_entry              : entry to init
 * @param phy_addr                 : physical address, to extract the tag
 * @param CACHE_TAG_REMAINING_BITS : remaining bits for the tag
 * @param mem_space                : starting address of the memory space
 * @param WORDS_PER_LINE           : the number of words per line of the cache
 */
#define cache_entry_init_generic(type, cache_entry, phy_addr, CACHE_TAG_REMAINING_BITS, mem_space, WORDS_PER_LINE) {     				 \
	type* entry = cache_entry;                                                                                             				 \
	uint32_t addr =  compute_addr_line_aligned(phy_addr,WORDS_PER_LINE) ;    /*get the right word addressed line*/			     \
	cache_init_entry_with_param(entry, phy_addr,  ((word_t*)(mem_space) + addr), WORDS_PER_LINE, CACHE_TAG_REMAINING_BITS);              \
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
		case L1_ICACHE : cache_entry_init_generic(l1_icache_entry_t, cache_entry, phy_addr, L1_ICACHE_TAG_REMAINING_BITS, mem_space, L1_ICACHE_WORDS_PER_LINE) ; break;
		case L1_DCACHE : cache_entry_init_generic(l1_dcache_entry_t, cache_entry, phy_addr, L1_DCACHE_TAG_REMAINING_BITS, mem_space, L1_DCACHE_WORDS_PER_LINE) ; break;
		case L2_CACHE  : cache_entry_init_generic(l2_cache_entry_t,  cache_entry, phy_addr, L2_CACHE_TAG_REMAINING_BITS , mem_space , L2_CACHE_WORDS_PER_LINE) ; break;
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
 * @param WAYS               : the number of ways of the cache
 * @param line_index : index of the cache line
 */
#define find_empty_slot_generic(TYPE, cache, LINE_INDEX, WAYS)    {                                                 \
	foreach_way(way, WAYS){    /*simply iterate on each way and stop as soon as a invalid cache entry is found*/    \
		if (!cache_valid(TYPE, WAYS, LINE_INDEX, way)) {return way;}                                                \
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
		case L1_ICACHE: find_empty_slot_generic(l1_icache_entry_t, cache, line_index, L1_ICACHE_WAYS);break;
		case L1_DCACHE: find_empty_slot_generic(l1_dcache_entry_t, cache, line_index, L1_DCACHE_WAYS);break;
		case L2_CACHE : find_empty_slot_generic(l2_cache_entry_t , cache, line_index, L2_CACHE_WAYS) ;break;
		default: return NOTHING_FOUND;break;
		}
	} 
/**
 * @brief generic helper function for evict
 * @param TYPE : type of cache
 * @param LINE_INDEX : index of the line of the cache where we want to evict
 * @param WAYS : number of ways of the cache
 * @param returns the evicted entry or NULL in case of an error
 */                                                                                           
#define evict_generic(TYPE, LINE_INDEX, WAYS)    {                                                       \
	unsigned int max_age = 0 ;                                                                           \
	TYPE* entry_to_evict = NULL;                                                                         \
	foreach_way(way, WAYS){ /*iterate on every way to find the entry with max age (LRU)*/                \
		TYPE* entry = cache_entry(TYPE, WAYS, LINE_INDEX, way);                                          \
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
		case L1_ICACHE: evict_generic(l1_icache_entry_t, line_index, L1_ICACHE_WAYS);break;
		case L1_DCACHE: evict_generic(l1_dcache_entry_t, line_index, L1_DCACHE_WAYS);break;
		case L2_CACHE : evict_generic(l2_cache_entry_t , line_index, L2_CACHE_WAYS) ;break;
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
	uint16_t line_index = extract_line_index(phy_addr, L2_CACHE_WORDS_PER_LINE, L2_CACHE_LINES);
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
 * @param TAG_REMAINING_BITS : bits that are not used by the tag in L1 cache
 * @param index : index of the line where the entry has been placed in  L1
 */
#define recomputeOldPhyAddr(tag, TAG_REMAINING_BITS, index) ((((uint32_t)(tag))<<(TAG_REMAINING_BITS)) | (((uint32_t)(index))<< TAG_AND_LINE_INDEX_REMAINING_BITS))
/**
 * @brief          : function that inserts a L1 entry into l1_cache
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
	uint16_t line_index = (access == INSTRUCTION) ? extract_line_index(phy_addr, L1_ICACHE_WORDS_PER_LINE, L1_ICACHE_LINES): extract_line_index(phy_addr, L1_DCACHE_WORDS_PER_LINE, L1_DCACHE_LINES);
	int cache_way = find_empty_slot(cache_type, l1_cache, line_index); //find a place
	if (cache_way == NOTHING_FOUND){// there is no empty slot in l1 cache => evict an entry and move it to L2
		void* evicted_entry = evict(cache_type, l1_cache, line_index); //eviction
		if (evicted_entry == NULL) return ERR_MEM; // error propagation 
		uint32_t oldPhyAddr = (access == INSTRUCTION)? recomputeOldPhyAddr(cast_l1_entry(access,evicted_entry)->tag, L1_ICACHE_TAG_REMAINING_BITS, line_index ) : recomputeOldPhyAddr(cast_l1_entry(access,evicted_entry)->tag, L1_DCACHE_TAG_REMAINING_BITS, line_index );
		l2_cache_entry_t l2_entry; // cast evicted_entry to an l2 entry
		cache_init_entry_with_param(&l2_entry, oldPhyAddr, cast_l1_entry(access,evicted_entry)->line, L2_CACHE_WORDS_PER_LINE, L2_CACHE_TAG_REMAINING_BITS);
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
//============================================================================================
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
 * @param TAG_REMAINING_BITS : the number of bits of the remaining bits for the tag of phy_addr
 * @return error code
 */
#define move_entry_to_level1_generic(access, type, l2_entry, l1_cache, l2_cache, phy_addr, TAG_REMAINING_BITS)     \
	type l1_entry;    /*create L1 entry for casting form l2_entry*/                                                \
	/* cast it */                                                                                                  \
	cache_init_entry_with_param(&l1_entry, phy_addr, l2_entry->line, L2_CACHE_WORDS_PER_LINE, TAG_REMAINING_BITS)  \
	/* insert new entry in l1_cache and if needed, do the error propagation*/                                      \
	int err = ERR_NONE; 																						\
	(l2_entry)->v = 0;      /*invalidate l2_entry*/                                                                 \
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
		case INSTRUCTION: {move_entry_to_level1_generic(access, l1_icache_entry_t, l2_entry,l1_cache, l2_cache, phy_addr, L1_ICACHE_TAG_REMAINING_BITS);}break;
		case DATA       : {move_entry_to_level1_generic(access, l1_dcache_entry_t, l2_entry,l1_cache, l2_cache, phy_addr, L1_DCACHE_TAG_REMAINING_BITS);}break;
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
 * @param WORDS_PER_LINE     : the number of words per line of the type cache: 
 */
#define search_in_memory_and_affect_generic(type, cache_type, WORDS_PER_LINE) {                                           \
	type entry;                                                                                                                                         \
	if ((err = cache_entry_init(mem_space,paddr,&entry,cache_type))!= ERR_NONE) return err; /*error propagation, init the entry from memory*/           \
	if ((err = insert_level1(access,l1_cache, l2_cache, &entry,phy_addr))!= ERR_NONE) return err; /*error propagation, insert the entry in memory*/     \
	*word = entry.line[extract_word_index(phy_addr,WORDS_PER_LINE)]; /*sets the word using the entry*/                                                  \
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
		case INSTRUCTION: search_in_memory_and_affect_generic(l1_icache_entry_t, L1_ICACHE, L1_ICACHE_WORDS_PER_LINE);break;
		case DATA       : search_in_memory_and_affect_generic(l1_dcache_entry_t, L1_DCACHE, L1_DCACHE_WORDS_PER_LINE);break;
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
		*word = p_line[extract_word_index(phy_addr, ((access == INSTRUCTION)?L1_ICACHE_WORDS_PER_LINE:L1_DCACHE_WORDS_PER_LINE))]; //set word since we found it in either l1_i or l1_d
		}
	else{//not found in l1 => search in l2
		if((err = cache_hit(mem_space, l2_cache, paddr,&p_line,&hit_way,&hit_index, L2_CACHE)) != ERR_NONE) return err; //check if it is in l2, error propagate
		if (hit_way != HIT_WAY_MISS) { // found in level 2 => move entry to level 1 and affect word
			*word = p_line[extract_word_index(phy_addr,L2_CACHE_WORDS_PER_LINE)];
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
	
	word_t word = 0;                                         //word that we will read from cache
	int byte_index = p_paddr->page_offset % sizeof(word_t);  //index of the byte inside of the word
	phy_addr_t paddr = *p_paddr;                             //value of paddr from pointer
	paddr.page_offset -= byte_index;                         //remove the byte index to use cache read
	if ((err = cache_read(mem_space,&paddr, access,l1_cache, l2_cache, &word, replace)) != ERR_NONE) return err;//error propagation, read word from memory
	byte_t* word_as_byte = (byte_t*)&word;                   //cast to byte to get the byte we want only
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
	uint32_t addr = compute_addr_line_aligned(phy_addr,L1_DCACHE_WORDS_PER_LINE);//gets the word addressed phy addr and sets memory to line
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
	uint32_t addr = compute_addr_line_aligned(phy_addr,L2_CACHE_WORDS_PER_LINE); //gets the word addressed phy addr and sets line using memory   
	access_memory((word_t*)(mem_space) + addr, line, L2_CACHE_WORDS_PER_LINE);   //let us assume that WORDS_PER_LINE*sizeof(word_t) is not going to overflow
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
		  l1_dcache_entry_t entry = *cache_entry_any(l1_dcache_entry_t, L1_DCACHE_WAYS, hit_index, hit_way, l1_cache); //get the entry corresponding
		  word_t* line = entry.line;
		  line[extract_word_index(phy_addr,L1_DCACHE_WORDS_PER_LINE)] = *word; //update word in its line
		  cache_insert(hit_index, hit_way, &entry, l1_cache, L1_DCACHE); //insert back in l1
		  modify_ages(L1_DCACHE, l1_cache, hit_way, hit_index, false); //modify ages accordingly (since its a hit its not cold start)
		  write_memory(mem_space, phy_addr,line); // update memory 
		}
	else{//not found in l1 => search in l2
		
		if((err = cache_hit(mem_space, l2_cache, paddr,&p_line,&hit_way,&hit_index, L2_CACHE)) != ERR_NONE) return err; //error handling, check if we have an entry in l2
		if (hit_way != HIT_WAY_MISS) { // found in level 2 => find line, update ages,  move entry to level 1 , update memory
			l2_cache_entry_t entry = *cache_entry_any(l2_cache_entry_t, L2_CACHE_WAYS, hit_index, hit_way, l2_cache); //get the entry
			word_t* line = entry.line;//get its line
			line[extract_word_index(phy_addr,L2_CACHE_WORDS_PER_LINE)] = *word; // update word in line
			cache_insert(hit_index, hit_way, &entry, l2_cache, L2_CACHE);//insert in back in l2
			modify_ages(L2_CACHE, l2_cache, hit_way, hit_index, false);//update ages in l2
			if ((err = move_entry_to_level1(DATA,l1_cache, l2_cache, cache_entry_any(l2_cache_entry_t, L2_CACHE_WAYS, hit_index, hit_way, l2_cache), phy_addr))!= ERR_NONE){return err;}//error propagation, insert in l1
			write_memory(mem_space, phy_addr,line); // update memory 
			}
		else { // not found in L2 => search in memory
			word_t line[L1_ICACHE_WORDS_PER_LINE]; //init line to read from memory
			read_memory(mem_space, phy_addr, line); // read line from memory
			line[extract_word_index(phy_addr,L1_DCACHE_WORDS_PER_LINE)] = *word;//update word
			write_memory(mem_space, phy_addr,line); // update memory 
			l1_dcache_entry_t entry;
			cache_entry_init(mem_space, paddr,&entry,L1_DCACHE); //init entry from memory (gets everything from mem)
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
	int err = 0; //for error propagation
	word_t word = 0; //word that will be read from cache
	int byte_index = paddr->page_offset % sizeof(word_t); //index location of byte
	phy_addr_t p_paddr = *paddr; //value of paddr
	p_paddr.page_offset -= byte_index; //get word based address
	if ((err = cache_read(mem_space, &p_paddr, DATA, l1_cache, l2_cache, &word, replace)) != ERR_NONE) return err; //read the full word
	
	//set word
	byte_t* word_byte = (byte_t*)&word;
	word_byte[byte_index] = p_byte;  //set the byte wanted inside the word
	if ((err = cache_write(mem_space,&p_paddr,l1_cache, l2_cache, &word, replace)) != ERR_NONE) return err;//error propagation, write word back in memory
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
