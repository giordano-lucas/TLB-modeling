#pragma once


#include "cache.h"
#include "cache_mng.h"
/**
 * @brief increase ages of all entries in LINE_INDEX or set age to 0 for the entry at WAY_INDEX
 * @param TYPE : type of cache entry
 * @param WAYS : number of ways of the cache
 * @param WAY_INDEX : index of the way that needs to be reset
 * @param LINE_INDEX : index of the line of the cache that needs to be updated
 */
#define LRU_age_increase(TYPE, WAYS, WAY_INDEX, LINE_INDEX)                       \
	foreach_way(way, WAYS){	                                                      \
	        TYPE* entry = cache_entry(TYPE, WAYS, LINE_INDEX, way);           \
	        /*fprintf(stderr, "TESTINGWAY " #TYPE );*/ \
			if (entry->v == 1){                                                   \
				if (way == (WAY_INDEX)) {entry->age = 0;}                         \
			    else if (entry->age < ((WAYS)-1)) {(entry->age)+=1;}               \
				}                                                                 \
			/*fprintf(stderr, "LRU_AGE_INCREASE : valid = %d, way = %d, line index = %"PRIu32", age =  %d, ways = %d \n", entry->v, way, LINE_INDEX, entry->age, WAYS);*/\
		}

/**
 * @brief update ages of all entries in LINE_INDEX if their ages is inferior to the age of the entry at WAY_INDEX and set age to 0 for the entry at WAY_INDEX
 * @param TYPE : type of cache entry
 * @param WAYS : number of ways of the cache
 * @param WAY_INDEX : index of the way that needs to be reset
 * @param LINE_INDEX : index of the line of the cache that needs to be updated
 */
#define LRU_age_update(TYPE, WAYS, WAY_INDEX, L_INDEX)                       \
	uint8_t compare_age = cache_age(TYPE, WAYS, L_INDEX, WAY_INDEX);         \
	foreach_way(way, WAYS){	   \
			/*fprintf(stderr, "line index1 : %d " #L_INDEX, L_INDEX); */							\
		    TYPE* entry = cache_entry(TYPE, WAYS, L_INDEX, way);             \
		    /*fprintf(stderr, " TYPE " #TYPE ); */                           \
		    if (entry->v == 1){                                                 \
				if (way == (WAY_INDEX)) {entry->age = 0;}                      \
				else if (entry->age < compare_age) {(entry->age)++;}            \
			}                                                                   \
			/*fprintf(stderr, "LRU_AGE_UPDATE : valid = %d, way = %d, line index = %"PRIx32", age =  %d, ways = %d \n", entry->v, way, L_INDEX, entry->age, WAYS);*/\
		}
