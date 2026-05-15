#include "config.h"

#include <strata/mm/pool.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <strata/arch/mmu_constants.h>

#include <strata/compiler.h>
#include <strata/macros.h>
#include <strata/mm.h>
#include <strata/mm/types.h>
#include <strata/mm/vmm.h>
#include <strata/status.h>
#include <strata/thread.h>

struct subpool_header {
    struct subpool_header *prev, *next, *prev_free, *next_free;
    uint16_t object_size;
    uint16_t object_count;
    uint16_t header_size;
    uint16_t subpool_page_count;
    uint16_t used_count;
    uint8_t alignment_bits;
    RESERVE_1BYTE;
    RESERVE_4BYTES;
    uint64_t bitmap[];
};

struct subpool_entry {
    uint16_t object_size;
    uint8_t alignment_bits;
    RESERVE_1BYTE;
    struct subpool_header *first, *last, *next_free;
};

enum {
    SUBPOOL_SIZE_CLASS_COUNT =
        ((__builtin_ctz(PAGE_SIZE) - STRATA_MM_POOL_MIN_ALIGN_BITS - STRATA_MM_POOL_MANTISSA_BITS +
          2)
         << (STRATA_MM_POOL_MANTISSA_BITS - 1)),
    SUBPOOL_ALIGNMENT_CLASS_COUNT = (__builtin_ctz(PAGE_SIZE) - STRATA_MM_POOL_MIN_ALIGN_BITS + 1),
};

static struct subpool_entry subpool_table[SUBPOOL_SIZE_CLASS_COUNT * SUBPOOL_ALIGNMENT_CLASS_COUNT];

static size_t get_subpool_span_size(void)
{
    return (size_t)STRATA_MM_POOL_SUBPOOL_PAGE_COUNT * PAGE_SIZE;
}

static size_t get_subpool_bitmap_word_count(uint16_t object_count)
{
    return ALIGN_DIV(object_count, 64);
}

static uint8_t normalize_alignment_bits(int alignment_bits __in)
{
    if (alignment_bits < STRATA_MM_POOL_MIN_ALIGN_BITS) {
        return STRATA_MM_POOL_MIN_ALIGN_BITS;
    }

    return (uint8_t)alignment_bits;
}

static uint16_t normalize_object_size(uint16_t object_size __in, uint8_t alignment_bits __in)
{
    uint32_t quantum_bits;
    uint32_t quantum;
    uint32_t minimum_alignment_bits = MAX(STRATA_MM_POOL_MIN_ALIGN_BITS, alignment_bits);

    if (object_size <= (1 << minimum_alignment_bits)) {
        return (uint16_t)(1 << minimum_alignment_bits);
    }

    uint32_t msb = 31 - __builtin_clz(object_size);

    quantum_bits =
        (msb + 1 > STRATA_MM_POOL_MANTISSA_BITS) ? (msb + 1 - STRATA_MM_POOL_MANTISSA_BITS) : 0;
    if (quantum_bits < minimum_alignment_bits) {
        quantum_bits = minimum_alignment_bits;
    }

    quantum = 1U << quantum_bits;

    return (uint16_t)ALIGN(object_size, quantum);
}

static struct subpool_entry *get_object_entry(
    uint16_t normalized_object_size __in, uint8_t alignment_bits __in
)
{
    struct subpool_entry *empty_entry = NULL;

    for (size_t i = 0; i < ARRAY_SIZE(subpool_table); i++) {
        if (subpool_table[i].object_size == normalized_object_size &&
            subpool_table[i].alignment_bits == alignment_bits) {
            return &subpool_table[i];
        }
        if (!subpool_table[i].object_size && !empty_entry) {
            empty_entry = &subpool_table[i];
        }
    }

    if (empty_entry) {
        empty_entry->object_size = normalized_object_size;
        empty_entry->alignment_bits = alignment_bits;
    }

    return empty_entry;
}

static void remove_from_free_list(struct subpool_entry *entry, struct subpool_header *header)
{
    if (!entry || !header) return;

    if (header->prev_free) {
        header->prev_free->next_free = header->next_free;
    } else if (entry->next_free == header) {
        entry->next_free = header->next_free;
    }

    if (header->next_free) {
        header->next_free->prev_free = header->prev_free;
    }

    header->prev_free = NULL;
    header->next_free = NULL;
}

static void push_to_free_list(struct subpool_entry *entry, struct subpool_header *header)
{
    if (!entry || !header) return;
    if (header->prev_free || header->next_free || entry->next_free == header) return;

    header->prev_free = NULL;
    header->next_free = entry->next_free;
    if (entry->next_free) {
        entry->next_free->prev_free = header;
    }
    entry->next_free = header;
}

static int subpool_is_completely_full(const struct subpool_header *header)
{
    return header && header->used_count >= header->object_count;
}

static int subpool_is_completely_empty(const struct subpool_header *header)
{
    return header && header->used_count == 0;
}

static int validate_subpool_header_metadata(const struct subpool_header *header)
{
    size_t span_size;
    size_t bitmap_bytes;
    size_t object_bytes;
    size_t object_area_bytes;

    if (!header) return 0;
    if (header->subpool_page_count != STRATA_MM_POOL_SUBPOOL_PAGE_COUNT) return 0;
    if (!header->object_size || !header->object_count) return 0;
    if (header->used_count > header->object_count) return 0;
    if (header->alignment_bits < STRATA_MM_POOL_MIN_ALIGN_BITS) return 0;
    if (header->alignment_bits > __builtin_ctz(PAGE_SIZE)) return 0;

    span_size = get_subpool_span_size();
    bitmap_bytes = get_subpool_bitmap_word_count(header->object_count) * sizeof(uint64_t);
    object_bytes = (size_t)header->object_size * (size_t)header->object_count;
    object_area_bytes = span_size - header->header_size;

    if (header->header_size < sizeof(*header) + bitmap_bytes) return 0;
    if (header->header_size >= span_size) return 0;
    if (header->header_size % (1U << header->alignment_bits)) return 0;
    if (header->object_size % (1U << header->alignment_bits)) return 0;
    if (object_bytes > object_area_bytes) return 0;
    if (object_area_bytes - object_bytes >= header->object_size) return 0;

    return 1;
}

static int get_subpool_object_index(
    const struct subpool_header *header, const void *ptr, uint16_t *object_index_out
)
{
    uintptr_t object_start;
    uintptr_t object_end;
    uintptr_t ptr_addr;
    uintptr_t offset;
    uint16_t object_index;

    if (!header || !ptr) return 0;
    if (!validate_subpool_header_metadata(header)) return 0;

    object_start = (uintptr_t)header + header->header_size;
    object_end = object_start + ((uintptr_t)header->object_size * header->object_count);
    ptr_addr = (uintptr_t)ptr;

    if (ptr_addr < object_start || ptr_addr >= object_end) return 0;

    offset = ptr_addr - object_start;
    if (offset % header->object_size != 0) return 0;

    object_index = (uint16_t)(offset / header->object_size);
    if (object_index >= header->object_count) return 0;

    if (object_index_out) *object_index_out = object_index;
    return 1;
}

static struct subpool_header *find_subpool_header_from_ptr(
    const void *ptr, uint16_t *object_index_out
)
{
    StStatus status;
    StMm_MapFlags map_flags;
    St_VirtPage vpn;
    St_VirtPage begin_vpn;
    St_VirtPage end_vpn;
    struct subpool_header *header;

    if (!ptr) return NULL;

    vpn = VPTR_TO_PAGE(ptr);

    status = StMm_GetGlobalPageFlags(vpn, &map_flags);
    if (!CHECK_SUCCESS(status)) return NULL;
    if (!(map_flags & MF_POOL_SUBPOOL)) return NULL;

    status = StVmm_GetGlobalReservedRange(VMM_DOMAIN_KERNEL_SLOW, vpn, &begin_vpn, &end_vpn);
    if (!CHECK_SUCCESS(status)) return NULL;
    if (end_vpn <= begin_vpn) return NULL;
    if ((end_vpn - begin_vpn) != STRATA_MM_POOL_SUBPOOL_PAGE_COUNT) return NULL;

    header = PAGE_TO_VPTR(begin_vpn);

    if (!get_subpool_object_index(header, ptr, object_index_out)) return NULL;
    return header;
}

static int find_large_allocation_range_from_ptr(
    const void *ptr, St_VirtPage *begin_vpn_out, St_PageCount *page_count_out
)
{
    StStatus status;
    StMm_MapFlags map_flags;
    St_VirtPage vpn;
    St_VirtPage begin_vpn;
    St_VirtPage end_vpn;

    if (!ptr) return 0;

    vpn = VPTR_TO_PAGE(ptr);

    status = StMm_GetGlobalPageFlags(vpn, &map_flags);
    if (!CHECK_SUCCESS(status)) return 0;
    if (!(map_flags & MF_POOL_LARGE_ALLOC)) return 0;

    status = StVmm_GetGlobalReservedRange(VMM_DOMAIN_KERNEL_SLOW, vpn, &begin_vpn, &end_vpn);
    if (!CHECK_SUCCESS(status)) return 0;

    if ((uintptr_t)ptr != PAGE_TO_ADDR(begin_vpn)) return 0;
    if (end_vpn <= begin_vpn) return 0;

    if (begin_vpn_out) *begin_vpn_out = begin_vpn;
    if (page_count_out) *page_count_out = (St_PageCount)(end_vpn - begin_vpn);

    return 1;
}

static StStatus allocate_subpool(
    uint16_t normalized_object_size __in,
    uint8_t alignment_bits __in,
    struct subpool_header **header_out __out
)
{
    assert(header_out);

    StStatus status;
    St_VirtPage start_vpn;
    struct subpool_header *header;
    uint16_t object_count;
    uint16_t header_size;
    struct subpool_entry *entry;
    size_t bitmap_word_count;
    size_t minimum_header_size;
    size_t span_size;
    size_t alignment;

    entry = get_object_entry(normalized_object_size, alignment_bits);
    if (!entry) return STATUS_TOO_LARGE;

    status = StMm_AllocateGlobalSparse(
        VMM_DOMAIN_KERNEL_SLOW,
        &start_vpn,
        STRATA_MM_POOL_SUBPOOL_PAGE_COUNT,
        NULL,
        AF_DEFAULT | AF_ALIGN(STRATA_MM_POOL_SUBPOOL_PAGE_COUNT * PAGE_SIZE),
        MF_KERNEL_DEFAULT | MF_ZERO_FILL | MF_POOL_SUBPOOL
    );
    if (!CHECK_SUCCESS(status)) return status;

    header = PAGE_TO_VPTR(start_vpn);
    span_size = get_subpool_span_size();
    alignment = 1ULL << alignment_bits;

    object_count = (uint16_t)(span_size / normalized_object_size);

    while (object_count) {
        uint16_t new_object_count;

        bitmap_word_count = get_subpool_bitmap_word_count(object_count);
        minimum_header_size = sizeof(*header) + (bitmap_word_count * sizeof(uint64_t));
        header_size = (uint16_t)ALIGN(minimum_header_size, alignment);
        new_object_count = (uint16_t)((span_size - header_size) / normalized_object_size);

        if (new_object_count == object_count) break;
        object_count = new_object_count;
    }

    if (object_count == 0) {
        StMm_FreeGlobal(VMM_DOMAIN_KERNEL_SLOW, start_vpn, STRATA_MM_POOL_SUBPOOL_PAGE_COUNT);
        return STATUS_INSUFFICIENT_SPACE;
    }

    header->object_size = normalized_object_size;
    header->object_count = object_count;
    header->header_size = header_size;
    header->subpool_page_count = STRATA_MM_POOL_SUBPOOL_PAGE_COUNT;
    header->used_count = 0;
    header->alignment_bits = alignment_bits;

    if ((object_count % 64) != 0) {
        uint32_t valid_bits = object_count % 64;
        header->bitmap[bitmap_word_count - 1] = ~((1ULL << valid_bits) - 1);
    }

    StThread_LockPreemption();

    header->prev = entry->last;
    header->next = NULL;
    header->prev_free = NULL;
    header->next_free = NULL;

    if (entry->last) {
        entry->last->next = header;
    } else {
        entry->first = header;
    }
    entry->last = header;

    push_to_free_list(entry, header);

    StThread_UnlockPreemption();

    *header_out = header;

    return STATUS_SUCCESS;
}

static St_VirtPage unlink_subpool_locked(struct subpool_header *header __in)
{
    struct subpool_entry *entry;
    St_VirtPage header_vpn = 0;

    if (!validate_subpool_header_metadata(header)) return 0;

    entry = get_object_entry(header->object_size, header->alignment_bits);
    if (!entry) return 0;

    header_vpn = VPTR_TO_PAGE(header);

    remove_from_free_list(entry, header);

    if (entry->first == header) {
        entry->first = header->next;
    } else if (header->prev) {
        header->prev->next = header->next;
    }

    if (entry->last == header) {
        entry->last = header->prev;
    } else {
        header->next->prev = header->prev;
    }

    if (!entry->first) {
        entry->object_size = 0;
        entry->alignment_bits = 0;
        entry->next_free = NULL;
    }

    header->prev = NULL;
    header->next = NULL;
    header->prev_free = NULL;
    header->next_free = NULL;
    header->object_size = 0;
    header->object_count = 0;
    header->header_size = 0;
    header->subpool_page_count = 0;
    header->used_count = 0;
    header->alignment_bits = 0;

    return header_vpn;
}

static StStatus allocate_large(size_t size __in, uint8_t alignment_bits __in, void **ptr __out)
{
    assert(ptr);

    StStatus status;
    St_VirtPage vpn;
    St_PageCount page_count;
    StMm_AllocFlags alloc_flags = AF_DEFAULT;

    page_count = ALIGN_DIV(size, PAGE_SIZE);
    if (alignment_bits > __builtin_ctz(PAGE_SIZE)) {
        alloc_flags |= AF_ALIGN(1ULL << alignment_bits);
    }

    status = StMm_AllocateGlobalSparse(
        VMM_DOMAIN_KERNEL_SLOW,
        &vpn,
        page_count,
        NULL,
        alloc_flags,
        MF_KERNEL_DEFAULT | MF_ZERO_FILL | MF_POOL_LARGE_ALLOC
    );
    if (!CHECK_SUCCESS(status)) return status;

    *ptr = PAGE_TO_VPTR(vpn);

    return STATUS_SUCCESS;
}

static StStatus allocate_internal(size_t size __in, int alignment_bits __in, void **ptr __out)
{
    assert(ptr);

    StStatus status;
    uint8_t normalized_alignment_bits;
    uint16_t normalized_size;
    struct subpool_entry *entry;
    struct subpool_header *header;
    size_t bitmap_word_count;
    uint16_t object_index;

    *ptr = NULL;
    if (size == 0) size = 1;
    if (alignment_bits < 0 || alignment_bits >= (int)(sizeof(size_t) * 8)) {
        return STATUS_INVALID_VALUE;
    }

    normalized_alignment_bits = normalize_alignment_bits(alignment_bits);
    if (size > PAGE_SIZE || normalized_alignment_bits > __builtin_ctz(PAGE_SIZE)) {
        return allocate_large(size, normalized_alignment_bits, ptr);
    }

    normalized_size = normalize_object_size((uint16_t)size, normalized_alignment_bits);
    if (normalized_size > PAGE_SIZE) {
        return allocate_large(size, normalized_alignment_bits, ptr);
    }

    entry = get_object_entry(normalized_size, normalized_alignment_bits);
    if (!entry) return STATUS_TOO_LARGE;

    header = entry->next_free;
    if (!header) {
        status = allocate_subpool(normalized_size, normalized_alignment_bits, &header);
        if (!CHECK_SUCCESS(status)) return status;
    }

    bitmap_word_count = get_subpool_bitmap_word_count(header->object_count);
    
    StThread_LockPreemption();

    for (size_t i = 0; i < bitmap_word_count; i++) {
        int bit_idx;
        if (header->bitmap[i] == UINT64_MAX) continue;

        bit_idx = __builtin_ctzll(~header->bitmap[i]);
        header->bitmap[i] |= (1ULL << bit_idx);
        header->used_count++;

        if (subpool_is_completely_full(header)) {
            remove_from_free_list(entry, header);
        }

        object_index = (uint16_t)((i * 64) + (uint16_t)bit_idx);
        *ptr = (void *)((uintptr_t)header + header->header_size +
                        ((uintptr_t)object_index * normalized_size));

        StThread_UnlockPreemption();
        break;
    }

    if (!*ptr) {
        StThread_UnlockPreemption();
        return STATUS_INSUFFICIENT_SPACE;
    }

    return STATUS_SUCCESS;
}

StStatus StPool_Allocate(size_t size __in, void **ptr __out)
{
    assert(ptr);

    return allocate_internal(size, STRATA_MM_POOL_MIN_ALIGN_BITS, ptr);
}

StStatus StPool_AllocateAligned(size_t size __in, int alignment __in, void **ptr __out)
{
    assert(ptr);

    return allocate_internal(size, alignment, ptr);
}

StStatus StPool_AllocateClear(size_t size __in, void **ptr __out)
{
    assert(ptr);

    StStatus status;

    status = StPool_Allocate(size, ptr);
    if (!CHECK_SUCCESS(status)) return status;

    memset(*ptr, 0, size);

    return STATUS_SUCCESS;
}

StStatus StPool_AllocateClearAligned(size_t size __in, int alignment __in, void **ptr __out)
{
    assert(ptr);

    StStatus status;

    status = StPool_AllocateAligned(size, alignment, ptr);
    if (!CHECK_SUCCESS(status)) return status;

    memset(*ptr, 0, size);

    return STATUS_SUCCESS;
}

StStatus StPool_Reallocate(void *ptr __in, size_t size __in, void **new_ptr __out)
{
    assert(new_ptr);

    StStatus status;
    void *allocated_ptr;
    struct subpool_header *subpool_header;
    St_VirtPage begin_vpn;
    St_PageCount page_count;
    uint16_t object_index;
    size_t copy_size;

    if (!ptr) {
        return StPool_Allocate(size, new_ptr);
    }

    if (size == 0) {
        StPool_Free(ptr);
        *new_ptr = NULL;
        return STATUS_SUCCESS;
    }

    if (find_large_allocation_range_from_ptr(ptr, &begin_vpn, &page_count)) {
        int current_alignment_bits;

        copy_size = (size_t)page_count * PAGE_SIZE;
        if (size <= copy_size) {
            *new_ptr = ptr;
            return STATUS_SUCCESS;
        }

        current_alignment_bits = __builtin_ctzll((unsigned long long)(uintptr_t)ptr);
        status = StPool_AllocateAligned(size, current_alignment_bits, &allocated_ptr);
        if (!CHECK_SUCCESS(status)) return status;

        memcpy(allocated_ptr, ptr, copy_size);
        StPool_Free(ptr);

        *new_ptr = allocated_ptr;
        return STATUS_SUCCESS;
    }

    subpool_header = find_subpool_header_from_ptr(ptr, &object_index);
    if (!subpool_header) return STATUS_INVALID_VALUE;

    if (size <= subpool_header->object_size) {
        *new_ptr = ptr;
        return STATUS_SUCCESS;
    }
    copy_size = subpool_header->object_size;

    status = StPool_AllocateAligned(size, subpool_header->alignment_bits, &allocated_ptr);
    if (!CHECK_SUCCESS(status)) return status;

    memcpy(allocated_ptr, ptr, copy_size);
    StPool_Free(ptr);

    *new_ptr = allocated_ptr;
    return STATUS_SUCCESS;
}

void StPool_Free(void *ptr __in)
{
    St_VirtPage begin_vpn;
    St_PageCount page_count;
    St_VirtPage header_vpn;
    struct subpool_header *subpool_header;
    struct subpool_entry *entry;
    uint16_t object_index;
    uint16_t word_index;
    uint16_t bit_index;
    uint64_t bit_mask;
    int was_full;

    if (!ptr) return;

    StThread_LockPreemption();

    if (find_large_allocation_range_from_ptr(ptr, &begin_vpn, &page_count)) {
        StThread_UnlockPreemption();
        StMm_FreeGlobal(VMM_DOMAIN_KERNEL_SLOW, begin_vpn, page_count);
        return;
    }

    subpool_header = find_subpool_header_from_ptr(ptr, &object_index);
    if (subpool_header) {
        entry = get_object_entry(subpool_header->object_size, subpool_header->alignment_bits);
        if (!entry) {
            StThread_UnlockPreemption();
            return;
        }

        word_index = object_index / 64;
        bit_index = object_index % 64;
        bit_mask = 1ULL << bit_index;

        if (!(subpool_header->bitmap[word_index] & bit_mask)) {
            StThread_UnlockPreemption();
            return;
        }

        was_full = subpool_is_completely_full(subpool_header);
        subpool_header->bitmap[word_index] &= ~bit_mask;
        if (subpool_header->used_count) {
            subpool_header->used_count--;
        }

        if (subpool_is_completely_empty(subpool_header)) {
            header_vpn = unlink_subpool_locked(subpool_header);
            StThread_UnlockPreemption();
            if (header_vpn) {
                StMm_FreeGlobal(
                    VMM_DOMAIN_KERNEL_SLOW,
                    header_vpn,
                    STRATA_MM_POOL_SUBPOOL_PAGE_COUNT
                );
            }
            return;
        }

        if (was_full) {
            push_to_free_list(entry, subpool_header);
        }

        StThread_UnlockPreemption();
        return;
    }

    StThread_UnlockPreemption();
}
