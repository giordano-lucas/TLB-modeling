#pragma once

/**
 * @file addr.h
 * @brief Type definitions for a virtual and physical addresses.
 *
 * @author Mirjana Stojilovic
 * @date 2018-19
 */

#include <stdint.h>

#define PAGE_OFFSET     12
#define PAGE_SIZE       4096 // = 2^12 B = 4 kiB pages


#define PTE_ENTRY       9
#define PMD_ENTRY       9
#define PUD_ENTRY       9
#define PGD_ENTRY       9
/* the number of entries in a page directory = 2^9
* each entry size is equal to the size of a physical address = 32b
*/
#define PD_ENTRIES      512

#define VIRT_PAGE_NUM   36 // = PTE_ENTRY + PUD_ENTRY + PMD_ENTRY + PGD_ENTRY
#define VIRT_ADDR_RES   16
#define VIRT_ADDR       64 // = VIRT_ADDR_RES + 4*9 + PAGE_OFFSET

#define PHY_PAGE_NUM    20
#define PHY_ADDR        32 // = PHY_PAGE_NUM + PAGE_OFFSET

/* TODO WEEK 04:
 * Définir ici les types
 *      word_t,
 *      byte_t,
 *      pte_t,
 *      virt_addr_t
 *  et phy_addr_t
 * (et supprimer ces huit lignes de commentaire).
 */
