/**
 * @file test-addr.c
 * @brief test code for virtual and physical address types and functions
 *
 * @author Jean-Cédric Chappelier and Atri Bhattacharyya
 * @date Jan 2019
 */

// -------------------------------------------------------------
// for thread-safe randomization
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

// ------------------------------------------------------------
#include <stdio.h> // for puts(). to be removed when no longer needed.

#include <check.h>
#include <inttypes.h>
#include <assert.h>

#include "tests.h"
#include "util.h"
#include "addr.h"
#include "addr_mng.h"

// ------------------------------------------------------------
// Preliminary stuff

#define random_value(TYPE)	(TYPE)(rand() % 4096)
#define REPEAT(N) for(unsigned int i_ = 1; i_ <= N; ++i_)

uint64_t generate_Nbit_random(int n)
{
    uint64_t val = 0;
    for(int i = 0; i < n; i++) {
        val <<= 1;
        val |= (unsigned)(rand() % 2);
    }
    return val;
}

// ------------------------------------------------------------
// First basic tests, provided to students

START_TEST(addr_basic_test_1)
{
    puts("==================== Start custom tests ==========================");
    
     puts("---------- Init_virt_addr64 -------------");
    virt_addr_t vaddr2;
    zero_init_var(vaddr2);
    REPEAT(100) {
        uint64_t pgd_entry   = (uint64_t) generate_Nbit_random(PGD_ENTRY);
        uint64_t pud_entry   = (uint64_t) generate_Nbit_random(PUD_ENTRY);
        uint64_t pmd_entry   = (uint64_t) generate_Nbit_random(PMD_ENTRY);
        uint64_t pte_entry   = (uint64_t) generate_Nbit_random(PTE_ENTRY);
        uint64_t page_offset = (uint64_t) generate_Nbit_random(PAGE_OFFSET);
        //pud_entry = 511;
        uint64_t vaddr64 = ( pgd_entry << PGD_ENTRY_START) | (pud_entry << PUD_ENTRY_START) | (pmd_entry << PMD_ENTRY_START) | (pte_entry << PTE_ENTRY_START) | page_offset;

		
        (void)init_virt_addr64(&vaddr2, vaddr64);

        ck_assert_int_eq(vaddr2.pgd_entry, pgd_entry);
        ck_assert_int_eq(vaddr2.pud_entry, pud_entry);
        ck_assert_int_eq(vaddr2.pmd_entry, pmd_entry);
        ck_assert_int_eq(vaddr2.pte_entry, pte_entry);
        ck_assert_int_eq(vaddr2.page_offset, page_offset);
        
        puts("-------- virt_addr_t_to_uint64_t --------");
        print_virtual_address(stdout,&vaddr2);
		uint64_t returnValue64 = virt_addr_t_to_uint64_t(&vaddr2);
		//ck_assert_int_eq(returnValue64, vaddr64);
    }
    
	
    
    puts("===================== End custiom tests ==========================");
// ------------------------------------------------------------
    virt_addr_t vaddr;
    zero_init_var(vaddr);

    srand(time(NULL) ^ getpid() ^ pthread_self());
#pragma GCC diagnostic pop

    REPEAT(100) {
        uint16_t pgd_entry   = (uint16_t) generate_Nbit_random(PGD_ENTRY);
        uint16_t pud_entry   = (uint16_t) generate_Nbit_random(PUD_ENTRY);
        uint16_t pmd_entry   = (uint16_t) generate_Nbit_random(PMD_ENTRY);
        uint16_t pte_entry   = (uint16_t) generate_Nbit_random(PTE_ENTRY);
        uint16_t page_offset = (uint16_t) generate_Nbit_random(PAGE_OFFSET);

        (void)init_virt_addr(&vaddr, pgd_entry, pud_entry, pmd_entry, pte_entry, page_offset);

        ck_assert_int_eq(vaddr.pgd_entry, pgd_entry);
        ck_assert_int_eq(vaddr.pud_entry, pud_entry);
        ck_assert_int_eq(vaddr.pmd_entry, pmd_entry);
        ck_assert_int_eq(vaddr.pte_entry, pte_entry);
        ck_assert_int_eq(vaddr.page_offset, page_offset);
    }
}
END_TEST

// ======================================================================
Suite* addr_test_suite()
{
    Suite* s = suite_create("Virtual and Physical Address Tests");

    Add_Case(s, tc1, "basic tests");
    tcase_add_test(tc1, addr_basic_test_1);

    return s;
}

TEST_SUITE(addr_test_suite)
