#pragma once

/**
 * @file tests.c
 * @brief Utilities for tests
 *
 * @date 2017
 */

#include <stdlib.h> // EXIT_FAILURE
#include <stdio.h>
#include <check.h>
#include "error.h"
/*
 * Creates a 16 bits mask of size "size" (nb of 1's)
 */
uint16_t mask16(size_t size);

/*
 * Creates a 32 bits mask of size "size" (nb of 1's)
 */
uint32_t mask32(size_t size);
/*
 * Creates a 64 bits mask of size "size" (nb of 1's)
 */
uint64_t mask64(size_t size);
/*
 * Tests if "toTest" is on Size	bits (= not greater that 2^size -1)
 */
int isOfSizeAsked32(size_t size, uint32_t toTest);

//=========================================================================
