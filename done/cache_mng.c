

#include "error.h"
#include "cache_mng.h"
#include "lru.h"
#include <inttypes.h>
#include <stdbool.h>
#include "addr_mng.h"
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
	#define extract_line_index(phy_addr, WORDS_PER_LINE, NB_LINES) \
	(((phy_addr)/(sizeof(word_t)*(WORDS_PER_LINE))) % (NB_LINES))
	
/**
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 */
#define cache_hit_generic(type, NB_LINES, WORDS_PER_LINE, REMAINING_BITS, WAYS) \
	uint16_t line_index = extract_line_index(phy_addr, WORDS_PER_LINE, NB_LINES);\
	uint32_t tag = phy_addr >> REMAINING_BITS;\
	foreach_way(way, WAYS) {\
		if (!cache_valid(type, WAYS, line_index, way) ){/* found a place*/ \
			LRU_age_increase(type, WAYS, line_index, way);/*update*/ \
			return ERR_NONE;\
			}\
		else if (cache_tag(type, WAYS, line_index, way) == tag){/*hit*/ \
			*p_line = cache_line(type, WAYS, line_index, way); \
			*hit_way = way; \
			*hit_index = line_index; \
			LRU_age_update(type, WAYS, line_index, way);/*update ages*/ \
			return ERR_NONE;\
		}\
	}\
	/*if we arrive here no entry has been found*/\
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
	M_REQUIRE_NON_NULL(mem_space);
	M_REQUIRE_NON_NULL(cache);
	M_REQUIRE_NON_NULL(paddr);
	M_REQUIRE_NON_NULL(p_line);
	M_REQUIRE_NON_NULL(hit_way);
	M_REQUIRE_NON_NULL(hit_index);
	M_REQUIRE(L1_ICACHE <= cache_type && cache_type <= L2_CACHE, ERR_BAD_PARAMETER, "%d is not a valid cache_type \n", cache_type);
	*hit_way = HIT_WAY_MISS;
	*hit_index = HIT_INDEX_MISS;
	uint32_t phy_addr = phy_to_int(paddr);
	
	switch(cache_type){
		case L1_ICACHE:{ cache_hit_generic(l1_icache_entry_t, L1_ICACHE_LINES, L1_ICACHE_WORDS_PER_LINE, L1_ICACHE_TAG_REMAINING_BITS, L1_ICACHE_WAYS);
		break;}
		case L1_DCACHE:{ cache_hit_generic(l1_dcache_entry_t, L1_DCACHE_LINES, L1_DCACHE_WORDS_PER_LINE, L1_DCACHE_TAG_REMAINING_BITS, L1_DCACHE_WAYS);
		break;}
		case L2_CACHE:{  cache_hit_generic(l2_cache_entry_t, L2_CACHE_LINES, L2_CACHE_WORDS_PER_LINE, L2_CACHE_TAG_REMAINING_BITS, L2_CACHE_WAYS);
		break;}
		default: return ERR_BAD_PARAMETER;
	}
}

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
	M_REQUIRE(L1_ICACHE <= cache_type && cache_type <= L2_CACHE, ERR_BAD_PARAMETER, "cache type is not a valid value : %d", cache_type);
	
	switch (cache_type) {
         case L1_ICACHE : {
		 M_REQUIRE(cache_way < L1_ICACHE_WAYS, ERR_BAD_PARAMETER, "cache way has to be inferior to L1_ICACHE_WAYS", ' ');
		 *cache_entry(l1_icache_entry_t,L1_ICACHE_WAYS, cache_line_index, cache_way ) = *(l1_icache_entry_t*)cache_line_in;
		 //memset(cache_line(l1_icache_entry_t, L1_ICACHE_WAYS, cache_line_index, cache_way), cache_line_in, L1_ICACHE_WORDS_PER_LINE*sizeof(word_t));
         return ERR_NONE;}
         case L1_DCACHE :
          M_REQUIRE(cache_way < L1_DCACHE_WAYS, ERR_BAD_PARAMETER, "cache way has to be inferior to L1_DCACHE_WAYS", ' ');
         *cache_entry(l1_dcache_entry_t,L1_DCACHE_WAYS, cache_line_index, cache_way ) = *(l1_dcache_entry_t*)cache_line_in;
	     //memset(cache_line(l1_dcache_entry_t, L1_DCACHE_WAYS, cache_line_index, cache_way), cache_line_in, L1_DCACHE_WORDS_PER_LINE*sizeof(word_t));
         
         return ERR_NONE;
         case L2_CACHE  :
          M_REQUIRE(cache_way < L2_CACHE_WAYS, ERR_BAD_PARAMETER, "cache way has to be inferior to L2_CACHE_WAYS", ' ');
         *cache_entry(l2_cache_entry_t,L2_CACHE_WAYS, cache_line_index, cache_way ) = *(l2_cache_entry_t*)cache_line_in;
         //memset(cache_line(l2_cache_entry_t, L2_CACHE_WAYS, cache_line_index, cache_way), cache_line_in, L2_CACHE_WORDS_PER_LINE*sizeof(word_t));
         
         return ERR_NONE;
         default        : return ERR_BAD_PARAMETER; break;
    }
					
}

//=========================================================================

#define cache_entry_init_generic(type, cache_entry, phy_addr, CACHE_TAG_REMAINING_BITS, mem_space, WORDS_PER_LINE) {     \
			type* entry = cache_entry;                                                                                             \
			entry->v = 1;                                                                                                          \
			entry->age = 0;                                                                                                        \
			entry->tag = phy_addr >> CACHE_TAG_REMAINING_BITS;     																	\
			uint32_t addr = ((phy_addr/sizeof(word_t))/WORDS_PER_LINE)*WORDS_PER_LINE  ;            \
			memcpy (entry->line, (word_t*)(mem_space) + addr,WORDS_PER_LINE*sizeof(word_t));                                 \
			}
 
// ========================================================================
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
		case L1_ICACHE : cache_entry_init_generic(l1_icache_entry_t, cache_entry, phy_addr, L1_ICACHE_TAG_REMAINING_BITS, mem_space, L1_ICACHE_WORDS_PER_LINE) ; break;
		case L1_DCACHE : cache_entry_init_generic(l1_dcache_entry_t, cache_entry, phy_addr, L1_DCACHE_TAG_REMAINING_BITS, mem_space, L1_DCACHE_WORDS_PER_LINE) ; break;
		case L2_CACHE  : cache_entry_init_generic(l2_cache_entry_t,  cache_entry, phy_addr, L2_CACHE_TAG_REMAINING_BITS , mem_space , L2_CACHE_WORDS_PER_LINE) ; break;
		default: return ERR_BAD_PARAMETER; break; //should not arrive here
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
	memcpy (entry->line, l2_entry->line,L2_CACHE_WORDS_PER_LINE*sizeof(word_t));       

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
void modify_ages(cache_t cache_type,void *cache, uint8_t way, uint16_t line_index){
	//if (way == NOTHING_FOUND) return;//not a cold start and not a hit
	switch (cache_type){
		case L1_ICACHE: if (way == NOTHING_FOUND) {LRU_age_update(l1_icache_entry_t, L1_ICACHE_WAYS, line_index, way) }else {LRU_age_increase(l1_icache_entry_t, L1_ICACHE_WAYS, line_index, way);}break;
		case L1_DCACHE: if (way == NOTHING_FOUND) {LRU_age_update(l1_dcache_entry_t, L1_DCACHE_WAYS, line_index, way) }else {LRU_age_increase(l1_dcache_entry_t, L1_DCACHE_WAYS, line_index, way);}break;
		case L2_CACHE : if (way == NOTHING_FOUND) {LRU_age_update(l2_cache_entry_t, L2_CACHE_WAYS, line_index, way  ) }else {LRU_age_increase(l2_cache_entry_t, L2_CACHE_WAYS, line_index, way  );}break;
		 
		default         : fprintf(stderr, "wrong instance of mem access at modify ages"); break;
		}
	}


int insert_level2(l2_cache_entry_t* cache, l2_cache_entry_t* entry, uint32_t phy_addr){
	uint16_t line_index = extract_line_index(phy_addr, L2_CACHE_WORDS_PER_LINE, L2_CACHE_LINES);
	int err = ERR_NONE;
	//find place
	int cache_way = find_empty_slot(L2_CACHE, cache, line_index);
	if (cache_way != NOTHING_FOUND){ // there is some place in l2 cache
		//if ((err = cache_insert(line_index,cache_way,entry,cache, L2_CACHE))!= ERR_NONE) return err;//error propagation
		//LRU_age_increase(l2_cache_entry_t, L2_CACHE_WAYS, line_index, cache_way);		//update ages
		}
	else { // there is no empty slot in l2 cache
		l2_cache_entry_t* evicted = evict(L2_CACHE, cache, line_index); //eviction
		entry->age = evicted->age;
		//if ((err = cache_insert(line_index,cache_way,entry,cache, L2_CACHE))!= ERR_NONE) return err;//error propagation
		//LRU_age_update(l2_cache_entry_t, L2_CACHE_WAYS, line_index, cache_way); //update ages
		}
	if ((err = cache_insert(line_index,cache_way,((l2_cache_entry_t*)entry),cache, L2_CACHE))!= ERR_NONE) return err;//error propagation
	modify_ages(L2_CACHE, cache, cache_way, line_index);
	return ERR_NONE;
	}

#define cast_l1_entry(access, entry) ( ((access) == INSTRUCTION)? ((l1_icache_entry_t*)(entry)): ((l1_icache_entry_t*)(entry)))


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
		memcpy(l2_entry.line, cast_l1_entry(access,evicted_entry)->line, L2_CACHE_WORDS_PER_LINE*sizeof(word_t)); // copy content of l1 entry to l2 entry
		// move entry to level 2
		if ((err = insert_level2((l2_cache_entry_t*)l2_cache, &l2_entry, phy_addr))!= ERR_NONE) return err; //error propagation
		cast_l1_entry(access,entry)->age = cast_l1_entry(access, evicted_entry)->age; //prepare for modifying ages policy 
		}
		
	//insert in l1 cache (here we are sure that there is at least an empt way)
	int mesCouilles = phy_addr >> L1_DCACHE_TAG_REMAINING_BITS;
	fprintf(stdout, "\n*** CACHE WAY : %d; CACHE LINE : %d ; TAG : %d", cache_way, line_index, cast_l1_entry(access,entry)->tag);
    if ((err = cache_insert(line_index,cache_way, cast_l1_entry(access,entry), l1_cache,cache_type))!= ERR_NONE) return err; // error propagation
   			
	modify_ages(cache_type,l1_cache, cache_way,line_index);//update ages
	
	return ERR_NONE;
	} 

int move_entry_to_level1(mem_access_t access,void * l1_cache, void * l2_cache, l2_cache_entry_t* l2_entry, uint32_t phy_addr){
	
	int err = ERR_NONE;
	void* l1_entry = NULL; 
	switch (access){
		case INSTRUCTION: { l1_icache_entry_t entry; l1_entry = &entry;}break;
		case DATA       : { l1_dcache_entry_t entry; l1_entry = &entry;}break;
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
	M_REQUIRE((paddr->page_offset % sizeof(word_t)) == 0, ERR_BAD_PARAMETER, "paddr should be word aligned for cache_read  %c",' ');
	fprintf(stdout, "*** READING : ");
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
		printf("***HIT WAY L1");
		//found it
		cache = l1_cache;
		if (access == INSTRUCTION) *word = cache_line_any(l1_icache_entry_t, L1_ICACHE_WAYS, hit_index, hit_way, l1_cache)[(phy_addr/sizeof(word_t))%L1_ICACHE_WORDS_PER_LINE];
		else                       *word = cache_line_any(l1_dcache_entry_t, L1_DCACHE_WAYS, hit_index, hit_way, l1_cache)[(phy_addr/sizeof(word_t))%L1_DCACHE_WORDS_PER_LINE];
		}
	else{
		//not found in l1 => search in l2
		if((err = cache_hit(mem_space, l2_cache, paddr,&p_line,&hit_way,&hit_index, L2_CACHE)) != ERR_NONE) return err;
		if (hit_way != HIT_WAY_MISS) {
			printf("***IF");
			cache = l2_cache;
			if ((err = move_entry_to_level1(access,l1_cache, l2_cache, cache_entry(l2_cache_entry_t, L2_CACHE_WAYS, hit_index, hit_way), phy_addr))!= ERR_NONE){
				return err;}//error propagation
			*word = cache_line_any(l2_cache_entry_t, L2_CACHE_WAYS, hit_index, hit_way, l2_cache)[(phy_addr/sizeof(word_t))%L2_CACHE_WORDS_PER_LINE];
			}
		else {
			printf("***ELSE");
			void* entry = NULL;
			if (access == INSTRUCTION){
				l1_icache_entry_t cache_entry;
				entry = &cache_entry;
				if ((err =cache_entry_init(mem_space,paddr,entry,L1_ICACHE))!= ERR_NONE) return err; //error propagation
				if ((err = insert_level1(access,l1_cache, l2_cache, entry,phy_addr))!= ERR_NONE) return err; //error propagation
				*word = cache_entry.line[(phy_addr/sizeof(word_t))%L1_ICACHE_WORDS_PER_LINE];
				}
			else {
				l1_dcache_entry_t cache_entry;
				entry = &cache_entry;
				if ((err =cache_entry_init(mem_space,paddr,entry,L1_DCACHE))!= ERR_NONE) return err; //error propagation
				if ((err = insert_level1(access,l1_cache, l2_cache, entry,phy_addr))!= ERR_NONE) return err; //error propagation
				*word = cache_entry.line[(phy_addr/sizeof(word_t))%L1_DCACHE_WORDS_PER_LINE];
			}
			//insert level 1
			
			
			}
		}
		printf("***END READ");
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
 #define MASK_BYTE 0b1111
int cache_read_byte(const void * mem_space, phy_addr_t * p_paddr, mem_access_t access,
					void * l1_cache,void * l2_cache,uint8_t * p_byte, cache_replace_t replace){
	M_REQUIRE_NON_NULL(mem_space);
	M_REQUIRE_NON_NULL(p_paddr);
	M_REQUIRE_NON_NULL(l1_cache);	
	M_REQUIRE_NON_NULL(l2_cache);	
	M_REQUIRE_NON_NULL(p_byte);	
	M_REQUIRE(replace == LRU, ERR_BAD_PARAMETER, "replace is not a valid instance of cache_replace_t %c", ' ');
	M_REQUIRE(access == INSTRUCTION || access == DATA, ERR_BAD_PARAMETER, "access is not a valid instance of mem_access_t %c", ' ');
	int err = ERR_NONE; //used for error propagation
	
	word_t word = 0;
	int byte_index = p_paddr->page_offset % sizeof(word_t);
	phy_addr_t paddr = *p_paddr;
	paddr.page_offset -= byte_index;
	if ((err = cache_read(mem_space,&paddr, access,l1_cache, l2_cache, &word, replace)) != ERR_NONE) return err;//error propagation
	*p_byte = (word>>byte_index)&MASK_BYTE;//final affectation
	
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
    switch (cache_type) {
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
