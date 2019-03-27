#pragma once

/**
 * @file tests.c
 * @brief Utilities for tests
 *
 * @author Valérian Rousset
 * @date 2017
 */

#include <stdlib.h> // EXIT_FAILURE
#include <check.h>
#include "error.h"
/*
 * Creates a 16 bits mask of size "size" (nb of 1's)
 */
uint16_t mask16(size_t size){
	uint16_t mask = 0;
	if (size == 16)
		return ~mask;
	else {
		mask = 1;
		mask = (mask << size) - 1;
		return mask;
	}
}

/*
 * Creates a 32 bits mask of size "size" (nb of 1's)
 */
uint32_t mask32(size_t size){
	uint32_t mask = 0;
	if (size == 32)
		return ~mask;
	else {
		mask = 1;
		mask = (mask << size) - 1;
		return mask;
	}
}
/*
 * Creates a 64 bits mask of size "size" (nb of 1's)
 */
uint64_t mask64(size_t size){
	uint64_t mask = 0;
	if (size == 64)
		return ~mask;
	else {
		mask = 1;
		mask = (mask << size) - 1;
		return mask;
	}
}
/*
 * Tests if "toTest" is on Size	bits (= not greater that 2^size -1)
 */
int isOfSizeAsked32(size_t size, uint32_t toTest){
	uint32_t mask = mask32(size);
	return (toTest&mask) == toTest;
}


//=========================================================================
