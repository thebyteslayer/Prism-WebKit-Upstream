/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_compact_heap_reservation.h"

#include "pas_heap_lock.h"
#include "pas_page_malloc.h"

#if PAS_PLATFORM(PLAYSTATION)
#include <memory-extra.h>
#endif

size_t pas_compact_heap_reservation_size =
    (size_t)1 << PAS_COMPACT_PTR_BITS << PAS_INTERNAL_MIN_ALIGN_SHIFT;
size_t pas_compact_heap_reservation_guard_size = 16;
uintptr_t pas_compact_heap_reservation_base = 0;
size_t pas_compact_heap_reservation_available_size = 0;
size_t pas_compact_heap_reservation_bump = 0;

#if PAS_PLATFORM(PLAYSTATION)
uintptr_t pas_compact_heap_reservation_committed = 0;
#endif

pas_aligned_allocation_result pas_compact_heap_reservation_try_allocate(size_t size, size_t alignment)
{
    pas_aligned_allocation_result result;
    uintptr_t reservation_end;
    uintptr_t padding_start;
    uintptr_t allocation_start;
    uintptr_t allocation_end;

    // PAS_INTERNAL_MIN_ALIGN is the alignment used to compact pointers, so we
    // have to align to at least that alignment.
    PAS_ASSERT(alignment >= PAS_INTERNAL_MIN_ALIGN);
    
    pas_heap_lock_assert_held();
    
    if (!pas_compact_heap_reservation_base) {
        pas_aligned_allocation_result page_result;

#if PAS_PLATFORM(PLAYSTATION)
        pas_zero_memory(&page_result, sizeof(pas_aligned_allocation_result));

        page_result.result = memory_extra_vss_reserve(pas_compact_heap_reservation_size, pas_page_malloc_alignment());
        PAS_ASSERT(page_result.result);
#else
        page_result = pas_page_malloc_try_allocate_without_deallocating_padding(
            pas_compact_heap_reservation_size, pas_alignment_create_trivial(), false);
        PAS_ASSERT(!page_result.left_padding_size);
        PAS_ASSERT(!page_result.right_padding_size);
        PAS_ASSERT(page_result.result);
        PAS_ASSERT(page_result.result_size == pas_compact_heap_reservation_size);
#endif

        pas_compact_heap_reservation_base =
            (uintptr_t)page_result.result - pas_compact_heap_reservation_guard_size;
        pas_compact_heap_reservation_available_size =
            pas_compact_heap_reservation_size - pas_compact_heap_reservation_guard_size;
        pas_compact_heap_reservation_bump = pas_compact_heap_reservation_guard_size;
    }

    reservation_end = pas_compact_heap_reservation_base + pas_compact_heap_reservation_available_size;
    padding_start = pas_compact_heap_reservation_base + pas_compact_heap_reservation_bump;
    allocation_start = pas_round_up_to_power_of_2(padding_start, alignment);

    if (allocation_start > reservation_end
        || allocation_start < padding_start
        || reservation_end - allocation_start < size)
        return pas_aligned_allocation_result_create_empty();

    allocation_end = allocation_start + size;

    pas_compact_heap_reservation_bump = allocation_end - pas_compact_heap_reservation_base;

#if PAS_PLATFORM(PLAYSTATION)
    if (pas_compact_heap_reservation_committed < allocation_end && size > 0) {
        uintptr_t need_commit_start;
        uintptr_t need_commit_end;
        bool success;
        uintptr_t page_size;

        if (!pas_compact_heap_reservation_committed)
            pas_compact_heap_reservation_committed = pas_compact_heap_reservation_base + pas_compact_heap_reservation_guard_size;

        page_size = pas_page_malloc_alignment();

        need_commit_start = pas_round_down_to_power_of_2(allocation_start, page_size);
        need_commit_end = pas_round_up_to_power_of_2(allocation_end, page_size);

        if (need_commit_start < pas_compact_heap_reservation_committed)
            need_commit_start = pas_compact_heap_reservation_committed;

        PAS_ASSERT(need_commit_start < need_commit_end);

        success = memory_extra_vss_commit((void*)need_commit_start, need_commit_end - need_commit_start, true, -1);
        PAS_ASSERT(success);

        pas_compact_heap_reservation_committed = need_commit_end;
    }
#endif

    result.left_padding = (void*)padding_start;
    result.left_padding_size = allocation_start - padding_start;
    result.result = (void*)allocation_start;
    result.result_size = allocation_end - allocation_start;
    result.right_padding = (void*)allocation_end;
    result.right_padding_size = 0;
    result.zero_mode = pas_zero_mode_is_all_zero;

    return result;
}

#endif /* LIBPAS_ENABLED */
