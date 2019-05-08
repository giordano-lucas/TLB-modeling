

#include "error.h"
#include "cache_mng.h"
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
	 memset(cache , 0, sizeof(type)*(NB_LINES)*(WAYS));                                                              \
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
	uint8_t line_index = (phy_addr>>L1_ICACHE_WORDS_PER_LINE)%L1_ICACHE_LINE;
	uint32_t tag = phy_addr >> L1_ICACHE_TAG_REMAINING_BITS;
	foreach_way(way, L1_ICACHE_WAYS) {
		if (!cache_valid(l1_icache_entry_t, L1_ICACHE_WAYS, line_index, way) ){
			// ??????
			}
		if (cache_tag(l1_icache_entry_t, L1_ICACHE_WAYS, line_index, way) == tag){
			*p_line = cache_line(l1_icache_entry_t, L1_ICACHE_WAYS, line_index, way);
		}
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

int cache_insert(uint16_t cache_line_index,
                 uint8_t cache_way,
                 const void * cache_line_in,
                 void * cache,
                 cache_t cache_type){
	M_REQUIRE_NON_NULL(cache);
	M_REQUIRE_NON_NULL(cache_line_in);
	word_t line = *((word_t*)cache_line_in);
	
	switch (cache_type) {
         case L1_ICACHE : 
         cache_line(L1_ICACHE, L1_ICACHE_WAYS, cache_line_index, cache_way) = line;
         
         break;
         case L1_DCACHE :
         case L2_CACHE  : 
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
int cache_read(const void * mem_space,
               phy_addr_t * paddr,
               mem_access_t access,
               void * l1_cache,
               void * l2_cache,
               uint32_t * word,
               cache_replace_t replace);

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
int cache_read_byte(const void * mem_space,
                    phy_addr_t * p_paddr,
                    mem_access_t access,
                    void * l1_cache,
                    void * l2_cache,
                    uint8_t * p_byte,
                    cache_replace_t replace);

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
int cache_write(void * mem_space,
                phy_addr_t * paddr,
                void * l1_cache,
                void * l2_cache,
                const uint32_t * word,
                cache_replace_t replace);

//=========================================================================
/**
 * @brief Write to cache a byte of data. Endianess: LITTLE.
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
int cache_write_byte(void * mem_space,
                     phy_addr_t * paddr,
                     mem_access_t access,
                     void * l1_cache,
                     void * l2_cache,
                     uint8_t p_byte,
                     cache_replace_t replace);
