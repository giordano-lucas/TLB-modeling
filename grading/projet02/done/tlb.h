#pragma once

/**
 * @file tlb.h
 * @brief definitions associated to a fully-associative TLB
 *
 * @date 2018-19
 */

#include "addr.h"

#include <stdint.h>

#define TLB_LINES 128 // the number of entries

/* TODO WEEK 08:
 * Définir ici le type tlb_entry_t
 * (et supprimer ces quatre lignes de commentaire).
 */

typedef struct{
	uint64_t tag : VIRT_PAGE_NUM;
	uint32_t phy_page_num : PHY_PAGE_NUM;
	uint8_t v : 1;
	
} tlb_entry_t;
