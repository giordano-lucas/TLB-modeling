#pragma once

/**
 * @file tlb_hrchy.h
 * @brief definitions associated to a two-level hierarchy of TLBs
 *
 * @date 2018-19
 */

#include "addr.h"

#include <stdint.h>

#define L1_ITLB_WAYS    1   // Direct mapped
// Do not modify the number of lines.
// It directly affects the tag field size.
#define L1_ITLB_LINES   16
#define L1_ITLB_LINES_BITS 4  // log_2(L1_ITLB_LINES)

#define L1_DTLB_WAYS    L1_ITLB_WAYS
// Do not modify the number of lines.
// It directly affects the tag field size.
#define L1_DTLB_LINES   L1_ITLB_LINES
#define L1_DTLB_LINES_BITS L1_ITLB_LINES_BITS // log_2(L1_DTLB_LINES)

#define L2_TLB_WAYS     1   // Direct mapped
#define L2_TLB_LINES    64  // 64, Do not modify this!
#define L2_TLB_LINES_BITS 6  // log_2(L2_TLB_LINES)

/**
 * L1 ITLB, L1 DTLB, and L2 TLB are all direct-mapped.
 */
/* TODO WEEK 09:
 * Définir ici les types :
 *    - l1_itlb_entry_t;
 *    - l1_dtlb_entry_t;
 *    - l2_tlb_entry_t;
 *    - et tlb_t;
 * (et supprimer ces huit lignes de commentaire).
 */
