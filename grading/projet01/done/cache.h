#pragma once

/**
 * @file cache.h
 * @brief definitions associated to a a two-level hierarchy of cache memories
 *
 * @date 2018-19
 */

#include <stdint.h>

#define L1_ICACHE_LINE   16 // 16 bytes (4 words) per line
#define L1_ICACHE_WAYS   4
#define L1_ICACHE_LINES  64  // Do not modify this!
#define L1_ICACHE_WORDS_PER_LINE 4

#define L1_DCACHE_LINE   L1_ICACHE_LINE
#define L1_DCACHE_WAYS   L1_ICACHE_WAYS
#define L1_DCACHE_LINES  L1_ICACHE_LINES
#define L1_DCACHE_WORDS_PER_LINE L1_ICACHE_WORDS_PER_LINE

#define L2_CACHE_LINE   L1_CACHE_LINE
#define L2_CACHE_WAYS   8
#define L2_CACHE_LINES  512  // Do not modify this!
#define L2_CACHE_WORDS_PER_LINE L1_ICACHE_WORDS_PER_LINE

/**
 * L1 ICACHE, L1 DCACHE:
 *  - byte addressing
 *  - physically addressed
 *  - 4-way set-associative
 *  - 4 words/way, where word = 4 bytes (=> 128 bits/way)
 *  - 64 sets (= 64 blocks per way) (= 6 bits to index)
 *  - total capacity = 4kiB
 *  - write-through policy (no dirty bit)
 *  - write-allocate on write miss
 *
 * L2 CACHE:
 *  - byte addressing
 *  - physically addressed
 *  - 8-way set-associative
 *  - 4 words/way, where word = 4 bytes (=> 128 bits/way)
 *  - 512 sets (= 512 blocks per way) (= 9 bits to index)
 *  - total capacity = 64kiB
 *  - write-through policy (no dirty bit)
 *  - write-allocate on write miss
 *
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
 */

/* TODO WEEK 11:
 * Définir ici les types :
 *    - l1_icache_entry_t;
 *    - l1_dcache_entry_t;
 *    - l2_cache_entry_t;
 *    - et cache_t;
 * (et supprimer ces huit lignes de commentaire).
 */
