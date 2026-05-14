#include <strata/mm/pmm.h>

#include <assert.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <strata/arch/intrinsics/misc.h>
#include <strata/arch/mmu_constants.h>

#include <strata/plat/memmap.h>
#include <strata/plat/mm.h>

#include <strata/compiler.h>
#include <strata/log.h>
#include <strata/macros.h>
#include <strata/mm/allocation_owner.h>
#include <strata/mm/allocation_owner_refs.h>
#include <strata/mm/pmm_refs.h>
#include <strata/mm/types.h>
#include <strata/panic.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <strata/types.h>

#include "internal.h"

#define MODULE_NAME "pmm"

/*
    [ Physical Memory Management Hierarchy ]

    alloc_table_ptr_array (4096 entries)
    +-----------------------+
    | [0] 0 - 64MiB         |      alloc_table (512 entries)
    | [1] 64 - 128MiB       |      +---------------------------+
    | [2] Pointer to Table  |----->| [0] Bitmap (Direct Value) |--> (32 Pages)
    |          ...          |      | [1] Bitmap (Direct Value) |
    | [n] ALLOCENT_BMP_FREE |      | [2] Pointer to ExtEntry   |----o
    +-----------------------+      |            ...            |    |
                                   | [k] ALLOCENT_EXT_UNUSABLE |    |
                                   +---------------------------+    |
                                                                    |
           .--------------------------------------------------------o
           |
           |    extended_entry (32 flags)
           |    +-----------------------------+
           '--->| [page 0] EE_USED            |
                | [page 1] EE_UNUSABLE (hole) |
                | [page 2] EE_FREE            |
                |             ...             |
                +-----------------------------+
*/

/*
    [ Allocation Entry Format ]

    free entry (32-frame granularity)
    0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000

    bitmap mode (partially allocated entry)
    0bbb bbbb bbbb bbbb bbbb bbbb bbbb bbbb bbbb bbbb bbbb bbbb bbbb bbbb bbbb bbbb
     ^^^ |8p| |4p gran| |- 2-page gran.  -| |--------- 1-page granularity --------|
     |||
     |16 page granularity
     32 page granularity

    fully allocated entry (32-frame granularity)
    0111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111

    extended entry
    1ppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp ppp0 0000
     |---------------- pointer to extended entry (32B-aligned) -------------|

    fully unusable entry (32-frame granularity)
    1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111
*/

#define ALLOCENT_EXT_FLAG     0x8000000000000000ULL
#define ALLOCENT_BMP_32P_MASK 0x4000000000000000ULL
#define ALLOCENT_BMP_16P_MASK 0x3000000000000000ULL
#define ALLOCENT_BMP_8P_MASK  0x0F00000000000000ULL
#define ALLOCENT_BMP_4P_MASK  0x00FF000000000000ULL
#define ALLOCENT_BMP_2P_MASK  0x0000FFFF00000000ULL
#define ALLOCENT_BMP_1P_MASK  0x00000000FFFFFFFFULL

#define ALLOCENT_BMP_32P_POS 62
#define ALLOCENT_BMP_16P_POS 60
#define ALLOCENT_BMP_8P_POS  56
#define ALLOCENT_BMP_4P_POS  48
#define ALLOCENT_BMP_2P_POS  32
#define ALLOCENT_BMP_1P_POS  0

#define ALLOCENT_EXT_UNUSABLE   0xFFFFFFFFFFFFFFFFULL
#define ALLOCENT_BMP_FULL_ALLOC 0x7FFFFFFFFFFFFFFFULL
#define ALLOCENT_BMP_FREE       0x0000000000000000ULL

#ifdef TESTING
#    define ALLOCENT_GET_EXT_PTR(e)    ((struct extended_entry *)((e) & ~ALLOCENT_EXT_FLAG))
#    define ALLOCENT_SET_EXT_PTR(e, p) ((e) = (uintptr_t)(p) | ALLOCENT_EXT_FLAG)

#else
#    define ALLOCENT_GET_EXT_PTR(e)    ((struct extended_entry *)(e))
#    define ALLOCENT_SET_EXT_PTR(e, p) ((e) = (uintptr_t)(p))

#endif

/*
    [ Allocation Table Pointer Array Entry Format ] (64-bit)

    free entry
    0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000

    huge allocation entry
    1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1110

    unusable entry
    1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111

    pointer entry
    1ppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp bbbb bbbb bbbb
     |--------- pointer to allocation table (4kiB-aligned) --------| | hint bitmap|
*/

#define ATPA_UNUSABLE   UINTPTR_MAX
#define ATPA_HUGE_ALLOC (UINTPTR_MAX - 1)
#define ATPA_FREE       0

#define ATPA_ADDR_MASK 0xFFFFFFFFFFFFF000ULL

#define ATPA_ORDER_BMP_MASK      0x0000000000000FFFULL
#define ATPA_ORDER_BMP_IDX(o)    ((o) <= 2 ? 0 : (o) - 2)
#define ATPA_ORDER_BMP_GET(b, o) (!!((b) & (1ULL << ATPA_ORDER_BMP_IDX(o))))
#define ATPA_ORDER_BMP_SET(b, o) ((b) |= (1ULL << ATPA_ORDER_BMP_IDX(o)))
#define ATPA_ORDER_BMP_CLR(b, o)                                                                   \
    ((b) &= ~((ATPA_ORDER_BMP_MASK) & ~((1ULL << ATPA_ORDER_BMP_IDX(o)) - 1)))

#define ATPA_INDEX_LIMIT_4G  (ALIGN_DIV(1LL << 32, PAGE_SIZE * ALLOC_TABLE_COVERAGE_PAGES))
#define ATPA_INDEX_LIMIT_16M (ALIGN_DIV(1LL << 24, PAGE_SIZE * ALLOC_TABLE_COVERAGE_PAGES))
#define ATPA_INDEX_LIMIT_1M  (ALIGN_DIV(1LL << 20, PAGE_SIZE * ALLOC_TABLE_COVERAGE_PAGES))

#define ALLOC_TABLE_ENTRY_INDEX_LIMIT_16M                                                          \
    (ALIGN_DIV(1LL << 24, PAGE_SIZE * ALLOCENT_COVERAGE_PAGES))
#define ALLOC_TABLE_ENTRY_INDEX_LIMIT_1M (ALIGN_DIV(1LL << 20, PAGE_SIZE * ALLOCENT_COVERAGE_PAGES))

#define CTZ64(x)    __builtin_ctzll(x)
#define CLZ64(x)    __builtin_clzll(x)
#define POPCNT64(x) __builtin_popcountll(x)

#define EE_FREE     0
#define EE_USED     1
#define EE_UNUSABLE 2

/*
    Single allocation table covers 512 * 32 = 16384 frames (64 MiB)
    Total allocation table pointer array cover 4096 * 512 * 32 = 67108864 frames (256 GiB)
*/
#define PAGES_PER_ALLOCTABLE_ENTRY      ((St_PageCount)32)
#define ALLOC_TABLE_ENTRY_COUNT         512
#define ALLOC_TABLE_PTR_ARRAY_SIZE      4096
#define EARLY_ALLOC_TABLE_POOL_COUNT    128
#define EARLY_EXTENDED_ENTRY_POOL_COUNT 128
#define ALLOC_TABLE_POOL_LOW_WATERMARK  8
#define EXTENTRY_POOL_LOW_WATERMARK     16
#define METADATA_BLOCK_ENTRY_COUNT      (PAGE_SIZE / sizeof(struct pmm_metadata))

#define ALLOCENT_COVERAGE_PAGES       PAGES_PER_ALLOCTABLE_ENTRY
#define ALLOCENT_COVERAGE_BYTES       (ALLOCENT_COVERAGE_PAGES * PAGE_SIZE)
#define ALLOC_TABLE_COVERAGE_PAGES    (ALLOC_TABLE_ENTRY_COUNT * PAGES_PER_ALLOCTABLE_ENTRY)
#define ALLOC_TABLE_COVERAGE_BYTES    (ALLOC_TABLE_COVERAGE_PAGES * PAGE_SIZE)
#define ATPA_COVERAGE_PAGES           (ALLOC_TABLE_PTR_ARRAY_SIZE * ALLOC_TABLE_COVERAGE_PAGES)
#define ATPA_COVERAGE_BYTES           (ATPA_COVERAGE_PAGES * PAGE_SIZE)
#define METADATA_BLOCK_COVERAGE_PAGES (METADATA_BLOCK_ENTRY_COUNT)
#define METADATA_BLOCK_COVERAGE_BYTES (METADATA_BLOCK_COVERAGE_PAGES * PAGE_SIZE)
#define PMM_MAX_ORDER                 26

/*
    Extended entry is used when one or more pages in entry is not free nor used
    (e.g. unusable memory). Instead of using bitmap, we use array of flags here.
*/
struct extended_entry {
    uint8_t state_flags[32];
} __packed __aligned(32);

/*
    An allocation table entry is a 64-bit value that can be interpreted in two ways:
    1. A multi-level buddy bitmap (if MSB is 0).
    2. A pointer to an extended entry (if MSB is 1).

    In x86_64 higher-half kernels, pointers to kernel memory (e.g., 0xFFFF8000...)
    always have the Most Significant Bit (MSB) set due to sign extension of
    canonical addresses. This allows us to distinguish between a bitmap and a
    pointer without an explicit tag bit, provided the bitmap logic ensures
    the MSB remains 0.

    Special cases:
    - 0x0000000000000000: Entirely FREE (Bitmap mode)
    - 0xFFFFFFFFFFFFFFFF: Entirely UNUSABLE (Pointer mode, non-dereferenceable)
*/
union alloc_table_entry {
    uint64_t bitmap;
    uintptr_t ptr;
};

/* sizeof(union alloc_table_entry) * ALLOC_TABLE_ENTRY_COUNT = PAGE_SIZE */
struct alloc_table {
    union alloc_table_entry entries[ALLOC_TABLE_ENTRY_COUNT];
} __packed __aligned(PAGE_SIZE);

static uintptr_t alloc_table_ptr_array[ALLOC_TABLE_PTR_ARRAY_SIZE] __aligned(PAGE_SIZE);

struct metadata_block {
    struct pmm_metadata entries[METADATA_BLOCK_ENTRY_COUNT];
} __packed __aligned(PAGE_SIZE);

/*
    Use these arrays for first some allocations.
    This is because we don't have enough memory to allocate structs in early
    stage.
    It might be sufficient for most cases.
*/
static struct alloc_table early_alloc_table_pool[EARLY_ALLOC_TABLE_POOL_COUNT] __aligned(PAGE_SIZE);
static int early_alloctbl_pool_used_count = 0;

static struct extended_entry early_extended_entry_pool[EARLY_EXTENDED_ENTRY_POOL_COUNT]
    __aligned(PAGE_SIZE);
static int early_extentry_pool_used_count = 0;

static struct alloc_table *dynamic_alloctbl_freelist = NULL;
static size_t dynamic_alloctbl_free_count = 0;
static struct extended_entry *dynamic_extentry_freelist = NULL;
static size_t dynamic_extentry_free_count = 0;
static int is_topping_up_alloctbl_pool = 0;
static int is_topping_up_extentry_pool = 0;

/* statistics */
static size_t total_frames = 0;
static size_t free_frames = 0;

/* status flags */
static int allocation_available = 0;
static int remarking_unavailable = 0;
static int metadata_available = 0;

#define PHYS_TO_DIRECTMAP_PTR(pa)                                                                  \
    ((void *)((uintptr_t)(pa) + PAGE_TO_ADDR(MEMMAP_DIRECTMAP_VPN_BASE)))

static void push_dynamic_alloc_table(struct alloc_table *table)
{
    uintptr_t next = (uintptr_t)dynamic_alloctbl_freelist;
    memcpy(table, &next, sizeof(next));
    dynamic_alloctbl_freelist = table;
    dynamic_alloctbl_free_count++;
}

static struct alloc_table *pop_dynamic_alloc_table(void)
{
    struct alloc_table *table = dynamic_alloctbl_freelist;
    uintptr_t next;
    if (!table) return NULL;

    memcpy(&next, table, sizeof(next));
    dynamic_alloctbl_freelist = (struct alloc_table *)next;
    dynamic_alloctbl_free_count--;

    return table;
}

static void push_dynamic_extentry(struct extended_entry *entry)
{
    uintptr_t next = (uintptr_t)dynamic_extentry_freelist;
    memcpy(entry->state_flags, &next, sizeof(next));
    dynamic_extentry_freelist = entry;
    dynamic_extentry_free_count++;
}

static struct extended_entry *pop_dynamic_extentry(void)
{
    struct extended_entry *entry = dynamic_extentry_freelist;
    uintptr_t next;
    if (!entry) return NULL;

    memcpy(&next, entry->state_flags, sizeof(next));
    dynamic_extentry_freelist = (struct extended_entry *)next;
    dynamic_extentry_free_count--;

    return entry;
}

static int is_dynamic_alloc_table(const struct alloc_table *table)
{
    uintptr_t addr;
    uintptr_t early_start;
    uintptr_t early_end;

    if (!table) return 0;

    addr = (uintptr_t)table;
    early_start = (uintptr_t)&early_alloc_table_pool[0];
    early_end = (uintptr_t)&early_alloc_table_pool[EARLY_ALLOC_TABLE_POOL_COUNT];

    if (addr < early_start || addr >= early_end) return 1;
    return 0;
}

static int alloc_table_is_all_free(const struct alloc_table *table)
{
    if (!table) return 0;
    for (size_t i = 0; i < ARRAY_SIZE(table->entries); i++) {
        if (table->entries[i].bitmap != ALLOCENT_BMP_FREE) return 0;
    }
    return 1;
}

static size_t get_alloctbl_pool_remaining(void)
{
    size_t early_remaining = 0;
    if (early_alloctbl_pool_used_count < EARLY_ALLOC_TABLE_POOL_COUNT) {
        early_remaining = EARLY_ALLOC_TABLE_POOL_COUNT - (size_t)early_alloctbl_pool_used_count;
    }
    return early_remaining + dynamic_alloctbl_free_count;
}

static size_t get_extentry_pool_remaining(void)
{
    size_t early_remaining = 0;
    if (early_extentry_pool_used_count < EARLY_EXTENDED_ENTRY_POOL_COUNT) {
        early_remaining = EARLY_EXTENDED_ENTRY_POOL_COUNT - (size_t)early_extentry_pool_used_count;
    }
    return early_remaining + dynamic_extentry_free_count;
}

static StStatus topup_alloc_table_pool(void)
{
    StStatus status;
    St_PhysFrame pfn = (St_PhysFrame)-1;
    struct alloc_table *table;

    if (!allocation_available) return STATUS_CONFLICTING_STATE;
    if (is_topping_up_alloctbl_pool) return STATUS_SUCCESS;

    is_topping_up_alloctbl_pool = 1;

    status = StPmm_AllocateContiguousFrame(&pfn, (St_PageCount)1, NULL, AF_DEFAULT);
    if (CHECK_SUCCESS(status)) {
        table = PHYS_TO_DIRECTMAP_PTR(FRAME_TO_ADDR(pfn));
        push_dynamic_alloc_table(table);
    }

    is_topping_up_alloctbl_pool = 0;
    return status;
}

static StStatus topup_extentry_pool(void)
{
    StStatus status;
    St_PhysFrame pfn = (St_PhysFrame)-1;
    struct extended_entry *base;
    size_t count;

    if (!allocation_available) return STATUS_CONFLICTING_STATE;
    if (is_topping_up_extentry_pool) return STATUS_SUCCESS;

    is_topping_up_extentry_pool = 1;

    status = StPmm_AllocateContiguousFrame(&pfn, (St_PageCount)1, NULL, AF_DEFAULT);
    if (CHECK_SUCCESS(status)) {
        base = PHYS_TO_DIRECTMAP_PTR(FRAME_TO_ADDR(pfn));
        count = PAGE_SIZE / sizeof(struct extended_entry);
        for (size_t i = 0; i < count; i++) {
            push_dynamic_extentry(&base[i]);
        }
    }

    is_topping_up_extentry_pool = 0;
    return status;
}

static void maybe_topup_management_pools(void)
{
    StStatus status;

    if (!allocation_available) return;

    if (!is_topping_up_alloctbl_pool &&
        get_alloctbl_pool_remaining() <= ALLOC_TABLE_POOL_LOW_WATERMARK) {
        status = topup_alloc_table_pool();
        if (!CHECK_SUCCESS(status)) {
            LOG_WARN(
                LM_CAT_UNCLASSIFIED,
                "failed to top up alloc-table pool (status=%08X)\n",
                status
            );
        }
    }

    if (!is_topping_up_extentry_pool &&
        get_extentry_pool_remaining() <= EXTENTRY_POOL_LOW_WATERMARK) {
        status = topup_extentry_pool();
        if (!CHECK_SUCCESS(status)) {
            LOG_WARN(LM_CAT_UNCLASSIFIED, "failed to top up extentry pool (status=%08X)\n", status);
        }
    }
}

static int get_order(size_t count)
{
    size_t temp;

    if (count == 0) return -1;
    if (count <= 1) return 0;

    temp = count - 1;
    return 64 - CLZ64(temp);
}

static const uint64_t alloc_masks[63] = {
    0x0000000000000001ULL,   0x0000000000000002ULL, 0x0000000000000004ULL, 0x0000000000000008ULL,
    0x0000000000000010ULL,   0x0000000000000020ULL, 0x0000000000000040ULL, 0x0000000000000080ULL,
    0x0000000000000100ULL,   0x0000000000000200ULL, 0x0000000000000400ULL, 0x0000000000000800ULL,
    0x0000000000001000ULL,   0x0000000000002000ULL, 0x0000000000004000ULL, 0x0000000000008000ULL,
    0x0000000000010000ULL,   0x0000000000020000ULL, 0x0000000000040000ULL, 0x0000000000080000ULL,
    0x0000000000100000ULL,   0x0000000000200000ULL, 0x0000000000400000ULL, 0x0000000000800000ULL,
    0x0000000001000000ULL,   0x0000000002000000ULL, 0x0000000004000000ULL, 0x0000000008000000ULL,
    0x0000000010000000ULL,   0x0000000020000000ULL, 0x0000000040000000ULL, 0x0000000080000000ULL,

    0x0000000100000003ULL,   0x000000020000000CULL, 0x0000000400000030ULL, 0x00000008000000C0ULL,
    0x0000001000000300ULL,   0x0000002000000C00ULL, 0x0000004000003000ULL, 0x000000800000C000ULL,
    0x0000010000030000ULL,   0x00000200000C0000ULL, 0x0000040000300000ULL, 0x0000080000C00000ULL,
    0x0000100003000000ULL,   0x000020000C000000ULL, 0x0000400030000000ULL, 0x00008000C0000000ULL,

    0x000100030000000FULL,   0x0002000C000000F0ULL, 0x0004003000000F00ULL, 0x000800C00000F000ULL,
    0x00100300000F0000ULL,   0x00200C0000F00000ULL, 0x004030000F000000ULL, 0x0080C000F0000000ULL,

    0x0103000F000000FFULL,   0x020C00F00000FF00ULL, 0x04300F0000FF0000ULL, 0x08C0F000FF000000ULL,

    0x130F00FF0000FFFFULL,   0x2CF0FF00FFFF0000ULL,

    ALLOCENT_BMP_FULL_ALLOC,
};

static inline void propagate_up(uint64_t *entry, unsigned start_page_idx, int start_order, int set)
{
    int offsets[] = {
        ALLOCENT_BMP_1P_POS,
        ALLOCENT_BMP_2P_POS,
        ALLOCENT_BMP_4P_POS,
        ALLOCENT_BMP_8P_POS,
        ALLOCENT_BMP_16P_POS,
        ALLOCENT_BMP_32P_POS
    };

    unsigned current_idx = start_page_idx >> start_order;

    for (int o = start_order; o < 5; o++) {
        unsigned next_idx = current_idx >> 1;    // Parent index
        unsigned sibling_idx = current_idx ^ 1;  // Sibling index

        unsigned parent_bit_pos = offsets[o + 1] + next_idx;
        unsigned sibling_bit_pos = offsets[o] + sibling_idx;

        if (set) {
            *entry |= (1ULL << parent_bit_pos);
        } else {
            if (!(*entry & (1ULL << sibling_bit_pos))) {
                *entry &= ~(1ULL << parent_bit_pos);
            } else {
                break;
            }
        }
        current_idx = next_idx;
    }
}

static inline int find_free_frame_idx_bitmap_entry(uint64_t entry, int align_order)
{
    uint64_t mask, inverted;
    int pos;

    switch (align_order) {
    case 0:
        mask = ALLOCENT_BMP_1P_MASK;
        pos = ALLOCENT_BMP_1P_POS;
        break;
    case 1:
        mask = ALLOCENT_BMP_2P_MASK;
        pos = ALLOCENT_BMP_2P_POS;
        break;
    case 2:
        mask = ALLOCENT_BMP_4P_MASK;
        pos = ALLOCENT_BMP_4P_POS;
        break;
    case 3:
        mask = ALLOCENT_BMP_8P_MASK;
        pos = ALLOCENT_BMP_8P_POS;
        break;
    case 4:
        mask = ALLOCENT_BMP_16P_MASK;
        pos = ALLOCENT_BMP_16P_POS;
        break;
    case 5:
        mask = ALLOCENT_BMP_32P_MASK;
        pos = ALLOCENT_BMP_32P_POS;
        break;
    default:
        return -1;
    }

    inverted = ~entry & mask;
    if (!inverted) return -1;

    return (CTZ64(inverted) - pos) * (1 << align_order);
}

static inline void allocate_from_bitmap_entry(uint64_t *entry, unsigned index, int order)
{
    // uint64_t old_entry_value = *entry;

    switch (order) {
    case 0:
        *entry |= alloc_masks[index];
        break;
    case 1:
        *entry |= alloc_masks[32 + (index / 2)];
        break;
    case 2:
        *entry |= alloc_masks[48 + (index / 4)];
        break;
    case 3:
        *entry |= alloc_masks[56 + (index / 8)];
        break;
    case 4:
        *entry |= alloc_masks[60 + (index / 16)];
        break;
    case 5:
        *entry |= alloc_masks[62];
        break;
    default:
        return;
    }

    propagate_up(entry, index, order, 1);

    // LOG_DEBUG(LM_CAT_UNCLASSIFIED, "A:%016lx:%016lx\n", old_entry_value, *entry);
}

static inline void free_to_bitmap_entry(uint64_t *entry, unsigned index, int order)
{
    // uint64_t old_entry_value = *entry;

    switch (order) {
    case 0:
        *entry &= ~alloc_masks[index];
        break;
    case 1:
        *entry &= ~alloc_masks[32 + (index / 2)];
        break;
    case 2:
        *entry &= ~alloc_masks[48 + (index / 4)];
        break;
    case 3:
        *entry &= ~alloc_masks[56 + (index / 8)];
        break;
    case 4:
        *entry &= ~alloc_masks[60 + (index / 16)];
        break;
    case 5:
        *entry &= ~alloc_masks[62];
        break;
    default:
        return;
    }
    propagate_up(entry, index, order, 0);

    // LOG_DEBUG(LM_CAT_UNCLASSIFIED, "F:%016lx:%016lx\n", old_entry_value, *entry);
}

static StStatus get_or_create_table(size_t table_idx, struct alloc_table **table)
{
    StStatus status;
    int create_as_free = 1;
    struct alloc_table *new_table = NULL;

    if (table_idx >= ALLOC_TABLE_PTR_ARRAY_SIZE) return STATUS_INVALID_VALUE;

    if (alloc_table_ptr_array[table_idx] != ATPA_FREE &&
        alloc_table_ptr_array[table_idx] != ATPA_UNUSABLE) {
        *table = (struct alloc_table *)(alloc_table_ptr_array[table_idx] & ATPA_ADDR_MASK);

        return STATUS_SUCCESS;
    }

    if (alloc_table_ptr_array[table_idx] == ATPA_UNUSABLE) {
        create_as_free = 0;
    }

    maybe_topup_management_pools();

    if (get_alloctbl_pool_remaining() == 0 && !is_topping_up_alloctbl_pool &&
        allocation_available) {
        status = topup_alloc_table_pool();
        if (!CHECK_SUCCESS(status)) return status;
    }

    // Need to allocate a new table.
    if (early_alloctbl_pool_used_count < EARLY_ALLOC_TABLE_POOL_COUNT) {
        new_table = &early_alloc_table_pool[early_alloctbl_pool_used_count++];
    } else if (dynamic_alloctbl_free_count > 0) {
        new_table = pop_dynamic_alloc_table();
    } else {
        LOG_ERROR(LM_CAT_UNCLASSIFIED, "failed to create allocation table");
        return STATUS_INSUFFICIENT_MEMORY;
    }

    // Initialize table to all allocated or all unusable.
    if (create_as_free) {
        for (size_t i = 0; i < ARRAY_SIZE(new_table->entries); i++) {
            new_table->entries[i].bitmap = ALLOCENT_BMP_FREE;
        }
    } else {
        for (size_t i = 0; i < ARRAY_SIZE(new_table->entries); i++) {
            new_table->entries[i].bitmap = ALLOCENT_EXT_UNUSABLE;
        }
    }

    // Set table pointer.
    alloc_table_ptr_array[table_idx] = (uintptr_t)new_table | ATPA_ORDER_BMP_MASK;

    if (table) *table = new_table;

    return STATUS_SUCCESS;
}

static StStatus get_or_create_extentry(
    struct alloc_table *table, unsigned entry_idx, struct extended_entry **entry
)
{
    StStatus status;
    int create_as_unusable = 0;
    struct extended_entry *new_entry = NULL;

    if (entry_idx >= ALLOC_TABLE_ENTRY_COUNT) return STATUS_INVALID_VALUE;

    if (table->entries[entry_idx].bitmap & ALLOCENT_EXT_FLAG &&
        table->entries[entry_idx].bitmap != ALLOCENT_EXT_UNUSABLE) {
        *entry = ALLOCENT_GET_EXT_PTR(table->entries[entry_idx].ptr);
        return STATUS_SUCCESS;
    }

    if (table->entries[entry_idx].bitmap == ALLOCENT_EXT_UNUSABLE) {
        create_as_unusable = 1;
    }

    maybe_topup_management_pools();

    if (get_extentry_pool_remaining() == 0 && !is_topping_up_extentry_pool &&
        allocation_available) {
        status = topup_extentry_pool();
        if (!CHECK_SUCCESS(status)) return status;
    }

    // need to allocate a new extended entry.
    if (early_extentry_pool_used_count < EARLY_EXTENDED_ENTRY_POOL_COUNT) {
        new_entry = &early_extended_entry_pool[early_extentry_pool_used_count++];
    } else if (dynamic_extentry_free_count > 0) {
        new_entry = pop_dynamic_extentry();
    } else {
        LOG_ERROR(LM_CAT_UNCLASSIFIED, "failed to create extended entry");
        return STATUS_INSUFFICIENT_MEMORY;
    }
    if (new_entry == NULL) {
        LOG_ERROR(LM_CAT_UNCLASSIFIED, "failed to create extended entry");
        return STATUS_INSUFFICIENT_MEMORY;
    }

    if (create_as_unusable) {
        memset(new_entry->state_flags, EE_UNUSABLE, sizeof(new_entry->state_flags));
    } else {
        memset(new_entry->state_flags, EE_FREE, sizeof(new_entry->state_flags));
    }

    ALLOCENT_SET_EXT_PTR(table->entries[entry_idx].ptr, new_entry);

#if UINTPTR_MAX != 0xFFFFFFFFFFFFFFFFULL
    table->entries[entry_idx].bitmap |= ALLOCENT_EXT_FLAG;

#endif

    if (entry) *entry = new_entry;

    return STATUS_SUCCESS;
}

static struct pmm_metadata *get_metadata(St_PhysFrame pfn)
{
    return (struct pmm_metadata *)(PAGE_TO_ADDR(MEMMAP_MFMAREA_VPN_BASE) +
                                   (pfn * sizeof(struct pmm_metadata)));
}

static St_PhysFrame get_pfn_from_metadata(struct pmm_metadata *metadata)
{
    return (St_PhysFrame)((uintptr_t)metadata - PAGE_TO_ADDR(MEMMAP_MFMAREA_VPN_BASE)) /
        sizeof(struct pmm_metadata);
}

static StStatus create_metadata(St_PhysFrame pfn, StAllocationOwner_StrongRef owner, int order)
{
    if (!metadata_available) return STATUS_SUCCESS;

    struct pmm_metadata *metadata = get_metadata(pfn);
    St_PageCount allocated_count = (St_PageCount)1ULL << order;

    metadata->lock = 0;
    metadata->refcount = 1;
    metadata->public.order = order;
    metadata->public.flags = 0;
    metadata->public.owner = owner;
    if (owner) {
        StAllocationOwner_Acquire(owner);
        owner->page_usage_count += allocated_count;
        if (owner->page_usage_peak_count < owner->page_usage_count) {
            owner->page_usage_peak_count = owner->page_usage_count;
        }
    }

    return STATUS_SUCCESS;
}

static void do_free_contiguous_frame(St_PhysFrame pfn, int order)
{
    size_t table_idx = pfn / ALLOC_TABLE_COVERAGE_PAGES;
    size_t entry_idx = (pfn % ALLOC_TABLE_COVERAGE_PAGES) / ALLOCENT_COVERAGE_PAGES;
    size_t slot_idx = pfn % ALLOCENT_COVERAGE_PAGES;
    struct alloc_table *table;
    size_t slots_needed;

    // 1. Huge Allocation
    if (order >= 14) {
        slots_needed = 1ULL << (order - 14);

        if (table_idx + slots_needed > ALLOC_TABLE_PTR_ARRAY_SIZE) {
            St_Panic(STATUS_INVALID_VALUE, "invalid pfn");
        }

        for (size_t i = 0; i < slots_needed; i++) {
            if (alloc_table_ptr_array[table_idx + i] != ATPA_HUGE_ALLOC) {
                St_Panic(STATUS_INVALID_VALUE, "invalid pfn");
            }
            alloc_table_ptr_array[table_idx + i] = ATPA_FREE;
        }

        free_frames += (1ULL << order);
        return;
    }

    if (alloc_table_ptr_array[table_idx] == ATPA_FREE ||
        alloc_table_ptr_array[table_idx] == ATPA_UNUSABLE) {
        St_Panic(STATUS_CONFLICTING_STATE, "double free");
    }

    table = (struct alloc_table *)(alloc_table_ptr_array[table_idx] & ATPA_ADDR_MASK);

    // 2. Normal Allocation
    if (order >= 5) {
        size_t entries_needed = 1ULL << (order - 5);

        if (entry_idx + entries_needed > ALLOC_TABLE_ENTRY_COUNT) {
            St_Panic(STATUS_INVALID_VALUE, "invalid pfn");
        }

        for (size_t i = 0; i < entries_needed; i++) {
            if (table->entries[entry_idx + i].bitmap != ALLOCENT_BMP_FULL_ALLOC) {
                St_Panic(STATUS_INVALID_VALUE, "invalid pfn");
            }
            table->entries[entry_idx + i].bitmap = ALLOCENT_BMP_FREE;
        }

        free_frames += (1ULL << order);

        ATPA_ORDER_BMP_SET(alloc_table_ptr_array[table_idx], order);

        if (alloc_table_is_all_free(table)) {
            alloc_table_ptr_array[table_idx] = ATPA_FREE;
            if (is_dynamic_alloc_table(table)) {
                push_dynamic_alloc_table(table);
            }
        }

        return;
    }

    // 3. Small Allocation
    slots_needed = 1ULL << order;

    if (table->entries[entry_idx].bitmap & ALLOCENT_EXT_FLAG) {
        struct extended_entry *extentry = ALLOCENT_GET_EXT_PTR(table->entries[entry_idx].ptr);

        if (slot_idx + slots_needed > PAGES_PER_ALLOCTABLE_ENTRY) {
            St_Panic(STATUS_INVALID_VALUE, "invalid pfn");
        }

        for (size_t i = 0; i < slots_needed; i++) {
            if (extentry->state_flags[slot_idx + i] != EE_USED) {
                St_Panic(STATUS_INVALID_VALUE, "invalid pfn");
            }
            extentry->state_flags[slot_idx + i] = EE_FREE;
        }
    } else {
        free_to_bitmap_entry(&table->entries[entry_idx].bitmap, slot_idx, order);
    }

    ATPA_ORDER_BMP_SET(alloc_table_ptr_array[table_idx], order);

    free_frames += (1ULL << order);

    if (alloc_table_is_all_free(table)) {
        alloc_table_ptr_array[table_idx] = ATPA_FREE;
        if (is_dynamic_alloc_table(table)) {
            push_dynamic_alloc_table(table);
        }
    }
}

StStatus StPmm_Init(void)
{
    for (int i = 0; i < ALLOC_TABLE_PTR_ARRAY_SIZE; i++) {
        alloc_table_ptr_array[i] = ATPA_UNUSABLE;
    }

    total_frames = 0;
    free_frames = 0;
    early_alloctbl_pool_used_count = 0;
    early_extentry_pool_used_count = 0;
    dynamic_alloctbl_freelist = NULL;
    dynamic_alloctbl_free_count = 0;
    dynamic_extentry_freelist = NULL;
    dynamic_extentry_free_count = 0;
    is_topping_up_alloctbl_pool = 0;
    is_topping_up_extentry_pool = 0;

    return STATUS_SUCCESS;
}

StStatus StPmm_MarkUsableContiguousFrame(St_PhysFrame base __in, St_PhysFrame limit __in)
{
    // Mark frames as Usable (Free).
    // This adds them to the pool.
    StStatus status;
    struct alloc_table *table = NULL;
    struct extended_entry *extentry = NULL;
    unsigned int table_idx = (unsigned int)-1, entry_idx;

    if (remarking_unavailable) return STATUS_CONFLICTING_STATE;

    // mark tables as free (64 MiB granularity)
    while (base <= limit && base < ATPA_COVERAGE_PAGES) {
        // If we can mark entire table to free, do so.
        if (!(base % ALLOC_TABLE_COVERAGE_PAGES) &&
            limit >= base + ALLOC_TABLE_COVERAGE_PAGES - 1) {

            // it's ok to mark entire allocation table to usable
            alloc_table_ptr_array[base / ALLOC_TABLE_COVERAGE_PAGES] = ATPA_FREE;
            base += (St_PhysFrame)ALLOC_TABLE_COVERAGE_PAGES;

            continue;
        }

        if (table_idx != base / ALLOC_TABLE_COVERAGE_PAGES) {
            table_idx = base / ALLOC_TABLE_COVERAGE_PAGES;

            // Get or create table.
            status = get_or_create_table(table_idx, &table);
            if (!CHECK_SUCCESS(status)) return status;
            if (!table) return STATUS_UNEXPECTED_RESULT;
        }

        // Mark frame entries as free. (128 KiB granularity)
        while (base <= limit && base / ALLOC_TABLE_COVERAGE_PAGES == table_idx) {
            if (!table) return STATUS_UNEXPECTED_RESULT;

            entry_idx = (base % ALLOC_TABLE_COVERAGE_PAGES) / ALLOCENT_COVERAGE_PAGES;

            // If we can mark entire allocation entry to free, do so.
            if (!(base % ALLOCENT_COVERAGE_PAGES) && limit >= base + ALLOCENT_COVERAGE_PAGES - 1) {
                // it's ok to mark entire allocation entry to usable
                table->entries[entry_idx].bitmap = ALLOCENT_BMP_FREE;
                base += ALLOCENT_COVERAGE_PAGES;

                continue;
            }

            // Now it's the hardest case.
            // We need to create a extended entry to mark the frames as free.
            status = get_or_create_extentry(table, entry_idx, &extentry);
            if (!CHECK_SUCCESS(status)) return status;
            if (!extentry) return STATUS_UNEXPECTED_RESULT;

            // mark frames as free. (4 KiB granularity)
            while (base <= limit &&
                   (base % ALLOC_TABLE_COVERAGE_PAGES) / ALLOCENT_COVERAGE_PAGES == entry_idx) {
                extentry->state_flags[base % ALLOCENT_COVERAGE_PAGES] = EE_FREE;
                base++;
            }
        }
    }

    return STATUS_SUCCESS;
}

StStatus StPmm_MarkUnusableContiguousFrame(St_PhysFrame base __in, St_PhysFrame limit __in)
{
    // Mark frames as Unusable.
    // This removes them from the pool.
    StStatus status;
    struct alloc_table *table;
    struct extended_entry *extentry;
    unsigned int table_idx, entry_idx;

    if (remarking_unavailable) return STATUS_CONFLICTING_STATE;

    // mark tables as unusable (64 MiB granularity)
    while (base <= limit && base < ATPA_COVERAGE_PAGES) {
        table_idx = base / ALLOC_TABLE_COVERAGE_PAGES;

        // If we can mark entire table to unusable, do so.
        if (!(base % ALLOC_TABLE_COVERAGE_PAGES) &&
            limit >= base + ALLOC_TABLE_COVERAGE_PAGES - 1) {
            // it's ok to mark entire allocation table to unusable
            alloc_table_ptr_array[base / ALLOC_TABLE_COVERAGE_PAGES] = ATPA_UNUSABLE;
            base += (St_PhysFrame)ALLOC_TABLE_COVERAGE_PAGES;

            continue;
        }

        // Get or create table.
        status = get_or_create_table(table_idx, &table);
        if (!CHECK_SUCCESS(status)) return status;
        if (!table) return STATUS_UNEXPECTED_RESULT;

        // Mark frame entries as unusable. (128 KiB granularity)
        while (base <= limit && base / ALLOC_TABLE_COVERAGE_PAGES == table_idx) {
            entry_idx = (base % ALLOC_TABLE_COVERAGE_PAGES) / ALLOCENT_COVERAGE_PAGES;

            // If we can mark entire allocation entry to unusable, do so.
            if (!(base % ALLOCENT_COVERAGE_PAGES) && limit >= base + ALLOCENT_COVERAGE_PAGES - 1) {
                // it's ok to mark entire allocation entry to unusable
                table->entries[entry_idx].bitmap = ALLOCENT_EXT_UNUSABLE;
                base += ALLOCENT_COVERAGE_PAGES;

                continue;
            }

            // Now it's the hardest case.
            // We need to create a extended entry to mark the frames as unusable.
            status = get_or_create_extentry(table, entry_idx, &extentry);
            if (!CHECK_SUCCESS(status)) return status;
            if (!extentry) return STATUS_UNEXPECTED_RESULT;

            // mark frames as unusable. (4 KiB granularity)
            while (base <= limit &&
                   (base % ALLOC_TABLE_COVERAGE_PAGES) / ALLOCENT_COVERAGE_PAGES == entry_idx) {
                extentry->state_flags[base % ALLOCENT_COVERAGE_PAGES] = EE_UNUSABLE;
                base++;
            }
        }
    }

    return STATUS_SUCCESS;
}

// Calling this function fixes the (un)usable area in the memory map.
// Further usable/unusable area remarking will be treated as an error.
StStatus StPmm_LateInit(void)
{
    StStatus status;
    St_PageCount required_metadata_blocks = 0;
    St_PhysFrame metadata_area_begin = 0;
    St_PhysFrame metadata_area_end = 0;
    struct alloc_table *table;
    struct extended_entry *extentry;

    // Calculate the number of metadata blocks required.
    for (size_t i = 0; i < ARRAY_SIZE(alloc_table_ptr_array); i++) {
        if (alloc_table_ptr_array[i] == ATPA_UNUSABLE) continue;

        if (alloc_table_ptr_array[i] == ATPA_FREE) {
            required_metadata_blocks += ALLOC_TABLE_COVERAGE_PAGES / METADATA_BLOCK_COVERAGE_PAGES;

            continue;
        }

        table = (struct alloc_table *)(alloc_table_ptr_array[i] & ATPA_ADDR_MASK);
        for (size_t j = 0; j < ARRAY_SIZE(table->entries); j += 2) {
            if (table->entries[j].bitmap == ALLOCENT_EXT_UNUSABLE &&
                table->entries[j + 1].bitmap == ALLOCENT_EXT_UNUSABLE) {
                continue;
            }

            required_metadata_blocks += 1;
        }
    }

    if (required_metadata_blocks > ALLOC_TABLE_COVERAGE_PAGES) {
        return STATUS_TOO_LARGE;
    }

    LOG_DEBUG(
        LM_CAT_UNCLASSIFIED,
        "Required metadata blocks: %" PRIu64 "\n",
        required_metadata_blocks
    );

    // find free frames for metadata blocks
    for (ssize_t i = ARRAY_SIZE(alloc_table_ptr_array) - 1; i >= 0; i--) {
        St_PageCount free_cont_frames = 0;

        if (alloc_table_ptr_array[i] == ATPA_UNUSABLE) continue;

        if (alloc_table_ptr_array[i] == ATPA_FREE) {
            metadata_area_begin = (St_PhysFrame)(i * ALLOC_TABLE_COVERAGE_PAGES);
            metadata_area_end =
                (St_PhysFrame)((i * ALLOC_TABLE_COVERAGE_PAGES) + required_metadata_blocks);
            break;
        }

        table = (struct alloc_table *)(alloc_table_ptr_array[i] & ATPA_ADDR_MASK);
        for (ssize_t j = ARRAY_SIZE(table->entries) - 1; j >= 0; j--) {
            if (table->entries[j].bitmap != ALLOCENT_BMP_FREE) {
                free_cont_frames = 0;
                continue;
            }

            if (free_cont_frames == 0) {
                metadata_area_end = (St_PhysFrame)(
                    (i * ALLOC_TABLE_COVERAGE_PAGES) + (j * ALLOCENT_COVERAGE_PAGES) - 1
                );
            }

            free_cont_frames += ALLOCENT_COVERAGE_PAGES;

            if (free_cont_frames >= required_metadata_blocks) break;
        }

        if (free_cont_frames >= required_metadata_blocks) {
            metadata_area_begin = metadata_area_end + 1 - required_metadata_blocks;
            break;
        }
    }

    LOG_DEBUG(
        LM_CAT_UNCLASSIFIED,
        "Metadata area: %" PRIX64 " - %" PRIX64 "\n",
        metadata_area_begin,
        metadata_area_end
    );

    // Allocate and map metadata blocks.
    status = StPmm_MarkUnusableContiguousFrame(metadata_area_begin, metadata_area_end);
    if (!CHECK_SUCCESS(status)) return status;

    // calculate total/free frames
    for (size_t i = 0; i < ARRAY_SIZE(alloc_table_ptr_array); i++) {

        if (alloc_table_ptr_array[i] == ATPA_UNUSABLE) continue;

        if (alloc_table_ptr_array[i] == ATPA_FREE) {
            total_frames += ALLOC_TABLE_COVERAGE_PAGES;
            free_frames += ALLOC_TABLE_COVERAGE_PAGES;

            continue;
        }

        table = (struct alloc_table *)(alloc_table_ptr_array[i] & ATPA_ADDR_MASK);
        for (size_t j = 0; j < ARRAY_SIZE(table->entries); j++) {
            if (table->entries[j].bitmap == ALLOCENT_EXT_UNUSABLE) continue;

            if (!(table->entries[j].bitmap & ALLOCENT_EXT_FLAG)) {
                total_frames += ALLOCENT_COVERAGE_PAGES;
                free_frames += ALLOCENT_COVERAGE_PAGES;

                continue;
            }

            extentry = ALLOCENT_GET_EXT_PTR(table->entries[j].ptr);

            for (size_t k = 0; k < ARRAY_SIZE(extentry->state_flags); k++) {
                if (extentry->state_flags[k] == EE_UNUSABLE) continue;

                total_frames++;
                free_frames++;
            }
        }
    }

    remarking_unavailable = 1;
    allocation_available = 1;

    for (size_t i = 0; i < ARRAY_SIZE(alloc_table_ptr_array); i++) {
        if (alloc_table_ptr_array[i] == ATPA_UNUSABLE) continue;

        if (alloc_table_ptr_array[i] == ATPA_FREE) {
            status = StMmP_MapGlobalContiguousMemory(
                metadata_area_begin,
                MEMMAP_MFMAREA_VPN_BASE +
                    (i * ALLOC_TABLE_COVERAGE_PAGES / METADATA_BLOCK_COVERAGE_PAGES),
                ALLOC_TABLE_COVERAGE_PAGES / METADATA_BLOCK_COVERAGE_PAGES,
                MF_KERNEL_DEFAULT
            );
            if (!CHECK_SUCCESS(status)) return status;
            metadata_area_begin += ALLOC_TABLE_COVERAGE_PAGES / METADATA_BLOCK_COVERAGE_PAGES;

            continue;
        }

        table = (struct alloc_table *)(alloc_table_ptr_array[i] & ATPA_ADDR_MASK);
        size_t j = 0;
        while (j < ARRAY_SIZE(table->entries)) {
            if (table->entries[j].bitmap == ALLOCENT_EXT_UNUSABLE &&
                table->entries[j + 1].bitmap == ALLOCENT_EXT_UNUSABLE) {
                j += 2;
                continue;
            }

            size_t batch_count = 0;
            for (size_t k = j; k < ARRAY_SIZE(table->entries); k += 2) {
                if (table->entries[k].bitmap == ALLOCENT_EXT_UNUSABLE &&
                    table->entries[k + 1].bitmap == ALLOCENT_EXT_UNUSABLE) {
                    break;
                }

                batch_count++;
            }

            status = StMmP_MapGlobalContiguousMemory(
                metadata_area_begin,
                MEMMAP_MFMAREA_VPN_BASE +
                    (i * ALLOC_TABLE_COVERAGE_PAGES / METADATA_BLOCK_COVERAGE_PAGES) + (j / 2),
                batch_count,
                MF_KERNEL_DEFAULT
            );
            if (!CHECK_SUCCESS(status)) return status;

            metadata_area_begin += batch_count;
            j += batch_count * 2;
        }
    }

    metadata_available = 1;

    /* Seed management pools once, then keep topping up on demand. */
    (void)topup_alloc_table_pool();
    (void)topup_extentry_pool();

    return STATUS_SUCCESS;
}

void StPmm_GetTotalFrameCount(St_PageCount *frame_count __out)
{
    assert(frame_count);

    *frame_count = total_frames;
}

void StPmm_GetFreeFrameCount(St_PageCount *frame_count __out)
{
    assert(frame_count);

    *frame_count = free_frames;
}

StStatus StPmm_AllocateContiguousFrame(
    St_PhysFrame *pfn __out,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in
)
{
    assert(pfn);

    StStatus status;
    St_PhysFrame allocated_pfn = 0;
    int order;
    uint32_t below_value;
    int align_order;
    ssize_t atpa_search_start;
    ssize_t atpa_align_jump;
    struct alloc_table *table;
    ssize_t table_search_start;
    ssize_t table_align_jump;

    if (!allocation_available) {
        LOG_ERROR(
            LM_CAT_UNCLASSIFIED,
            "StPmm_AllocateContiguousFrame: allocation unavailable (count=%zu flags=%08X)\n",
            count,
            alloc_flags
        );
        return STATUS_CONFLICTING_STATE;
    }

    order = get_order(count);
    if (order < 0) return STATUS_INVALID_VALUE;
    if (order > PMM_MAX_ORDER) return STATUS_INSUFFICIENT_MEMORY;
    if (owner && StAllocationOwner_IsClosed(owner)) return STATUS_CONFLICTING_STATE;

    below_value = alloc_flags & AF_PMM_BELOW_MASK;
    align_order = (int)(((alloc_flags & AF_ALIGN_MASK) >> 4) - 12);

    if (align_order < order) {
        align_order = order;
    }
    if (align_order > PMM_MAX_ORDER) return STATUS_INSUFFICIENT_MEMORY;

    atpa_search_start = ARRAY_SIZE(alloc_table_ptr_array);

    switch (below_value) {
    case AF_PMM_BELOW_NONE:
        break;
    case AF_PMM_BELOW_1M:
        if (order > 8) return STATUS_INSUFFICIENT_MEMORY;
        if (atpa_search_start > (ssize_t)ATPA_INDEX_LIMIT_1M) {
            atpa_search_start = ATPA_INDEX_LIMIT_1M;
        }
        break;
    case AF_PMM_BELOW_16M:
        if (order > 12) return STATUS_INSUFFICIENT_MEMORY;
        if (atpa_search_start > (ssize_t)ATPA_INDEX_LIMIT_16M) {
            atpa_search_start = ATPA_INDEX_LIMIT_16M;
        }
        break;
    case AF_PMM_BELOW_4G:
        if (order > 20) return STATUS_INSUFFICIENT_MEMORY;
        if (atpa_search_start > (ssize_t)ATPA_INDEX_LIMIT_4G) {
            atpa_search_start = ATPA_INDEX_LIMIT_4G;
        }
        break;
    default:
        return STATUS_INVALID_VALUE;
    }

    StThread_LockPreemption();

    atpa_align_jump = align_order > 13 ? (1LL << (align_order - 13)) : 1;
    atpa_search_start = ((atpa_search_start - 1) / atpa_align_jump) * atpa_align_jump;

    // 1. order >= 14 is a huge allocation. Use whole ATPA entries.
    if (order >= 14) {
        size_t atpa_slots_needed = (1ULL << (order - 14));
        size_t atpa_entry_count = ARRAY_SIZE(alloc_table_ptr_array);

        if (atpa_slots_needed > atpa_entry_count) {
            status = STATUS_INSUFFICIENT_MEMORY;
            goto has_error;
        }

        // find first fit
        for (ssize_t i = atpa_search_start; i >= 0; i -= atpa_align_jump) {
            int allocatable = 1;

            if ((size_t)i + atpa_slots_needed > atpa_entry_count) continue;
            if (alloc_table_ptr_array[i] != ATPA_FREE) continue;

            for (size_t j = 1; j < atpa_slots_needed; j++) {
                if (alloc_table_ptr_array[i + j] != ATPA_FREE) {
                    allocatable = 0;
                    break;
                }
            }

            if (!allocatable) continue;
            // LOG_TRACE(LM_CAT_UNCLASSIFIED, "found (%zd.-.-)\n", i);

            allocated_pfn = (St_PhysFrame)(i * ALLOC_TABLE_COVERAGE_PAGES);

            // mark all ATPA entries as allocated
            for (size_t j = 0; j < atpa_slots_needed; j++) {
                alloc_table_ptr_array[i + j] = ATPA_HUGE_ALLOC;
            }

            // allocate and fill metadata
            status = create_metadata(allocated_pfn, owner, order);
            if (!CHECK_SUCCESS(status)) goto has_error;

            *pfn = allocated_pfn;
            free_frames -= (1ULL << order);

            goto success;
        }

        status = STATUS_INSUFFICIENT_MEMORY;
        goto has_error;
    }

    table_search_start = ARRAY_SIZE(table->entries);
    table_align_jump = align_order > 5 ? (1LL << (align_order - 5)) : 1;
    table_search_start = ((table_search_start - 1) / table_align_jump) * table_align_jump;

    switch (below_value) {
    case AF_PMM_BELOW_1M:
        if (order > 8) {
            status = STATUS_INSUFFICIENT_MEMORY;
            goto has_error;
        }
        if (table_search_start > (ssize_t)ALLOC_TABLE_ENTRY_INDEX_LIMIT_1M) {
            table_search_start = ALLOC_TABLE_ENTRY_INDEX_LIMIT_1M;
        }
        break;
    case AF_PMM_BELOW_16M:
        if (order > 12) {
            status = STATUS_INSUFFICIENT_MEMORY;
            goto has_error;
        }
        if (table_search_start > (ssize_t)ALLOC_TABLE_ENTRY_INDEX_LIMIT_16M) {
            table_search_start = ALLOC_TABLE_ENTRY_INDEX_LIMIT_16M;
        }
        break;
    default:
        break;
    }

    // 2. 5 <= order < 14 is a normal allocation. Use whole allocation table entries.
    if (order >= 5) {
        size_t table_entries_needed = (1ULL << (order - 5));
        size_t table_entry_count = ARRAY_SIZE(table->entries);
        ssize_t normal_table_search_start = table_search_start;

        if (table_entries_needed > table_entry_count) {
            status = STATUS_INSUFFICIENT_MEMORY;
            goto has_error;
        }
        if (normal_table_search_start > (ssize_t)(table_entry_count - table_entries_needed)) {
            normal_table_search_start =
                ((ssize_t)(table_entry_count - table_entries_needed) / table_align_jump) *
                table_align_jump;
        }

        for (ssize_t i = atpa_search_start; i >= 0; i -= atpa_align_jump) {
            if (alloc_table_ptr_array[i] == ATPA_FREE) {
                size_t table_start;

                if (normal_table_search_start < 0) continue;
                table_start = (size_t)normal_table_search_start;
                if (table_start + table_entries_needed > table_entry_count) continue;

                // LOG_TRACE(LM_CAT_UNCLASSIFIED, "found (%zd.%zd.-)\n", i,
                // normal_table_search_start);

                status = get_or_create_table(i, &table);
                if (!CHECK_SUCCESS(status)) goto has_error;
                if (!table) {
                    status = STATUS_UNEXPECTED_RESULT;
                    goto has_error;
                }

                for (size_t j = 0; j < table_entries_needed; j++) {
                    // mark the whole entry as allocated
                    table->entries[table_start + j].bitmap = ALLOCENT_BMP_FULL_ALLOC;
                }

                allocated_pfn =
                    (i * ALLOC_TABLE_COVERAGE_PAGES) + (table_start * ALLOCENT_COVERAGE_PAGES);

                // allocate and fill metadata directory & metadata
                status = create_metadata(allocated_pfn, owner, order);
                if (!CHECK_SUCCESS(status)) goto has_error;

                *pfn = allocated_pfn;
                free_frames -= (1ULL << order);

                goto success;
            }

            if (alloc_table_ptr_array[i] == ATPA_HUGE_ALLOC ||
                alloc_table_ptr_array[i] == ATPA_UNUSABLE)
                continue;

            if (!ATPA_ORDER_BMP_GET(alloc_table_ptr_array[i], order)) continue;

            table = (struct alloc_table *)(alloc_table_ptr_array[i] & ATPA_ADDR_MASK);

            for (ssize_t j = normal_table_search_start; j >= 0; j -= table_align_jump) {
                int allocatable = 1;

                if ((size_t)j + table_entries_needed > table_entry_count) continue;
                if (table->entries[j].bitmap != ALLOCENT_BMP_FREE) continue;

                for (size_t k = 0; k < table_entries_needed; k++) {
                    if (table->entries[j + k].bitmap != ALLOCENT_BMP_FREE) {
                        allocatable = 0;
                        break;
                    }
                }

                if (!allocatable) continue;
                allocated_pfn =
                    (St_PhysFrame)((i * ALLOC_TABLE_COVERAGE_PAGES) + (j * ALLOCENT_COVERAGE_PAGES));
                // LOG_TRACE(LM_CAT_UNCLASSIFIED, "found (%zd.%zd.-)\n", i, j);

                // mark entire entry as allocated
                for (size_t k = 0; k < table_entries_needed; k++) {
                    table->entries[j + k].bitmap = ALLOCENT_BMP_FULL_ALLOC;
                }

                // allocate and fill metadata directory & metadata
                status = create_metadata(allocated_pfn, owner, order);
                if (!CHECK_SUCCESS(status)) goto has_error;

                *pfn = allocated_pfn;
                free_frames -= (1ULL << order);

                goto success;
            }

            ATPA_ORDER_BMP_CLR(alloc_table_ptr_array[i], order);
        }

        status = STATUS_INSUFFICIENT_MEMORY;
        goto has_error;
    }

    // 3. 0 <= order < 5 is a small allocation. Use extended entries or hierarchial bitmap.
    if (table_search_start < 0 || table_search_start >= (ssize_t)ARRAY_SIZE(table->entries)) {
        status = STATUS_INSUFFICIENT_MEMORY;
        goto has_error;
    }

    for (ssize_t i = atpa_search_start; i >= 0; i -= atpa_align_jump) {
        if (alloc_table_ptr_array[i] == ATPA_FREE) {
            int index;

            status = get_or_create_table(i, &table);
            if (!CHECK_SUCCESS(status)) goto has_error;
            if (!table) {
                status = STATUS_UNEXPECTED_RESULT;
                goto has_error;
            }

            index = find_free_frame_idx_bitmap_entry(
                table->entries[table_search_start].bitmap,
                align_order
            );
            if (index < 0) {
                St_Panic(STATUS_UNEXPECTED_RESULT, "how did you do that?");
            }

            // LOG_TRACE(LM_CAT_UNCLASSIFIED, "found (%zd.%zd.%d)\n", i, table_search_start, index);

            allocate_from_bitmap_entry(&table->entries[table_search_start].bitmap, index, order);

            allocated_pfn = (St_PhysFrame)(
                (i * ALLOC_TABLE_COVERAGE_PAGES) +
                (table_search_start * ALLOCENT_COVERAGE_PAGES) + index
            );

            // allocate and fill metadata directory & metadata table & metadata
            status = create_metadata(allocated_pfn, owner, order);
            if (!CHECK_SUCCESS(status)) goto has_error;

            *pfn = allocated_pfn;
            free_frames -= (1ULL << order);

            goto success;
        }

        if (alloc_table_ptr_array[i] == ATPA_HUGE_ALLOC ||
            alloc_table_ptr_array[i] == ATPA_UNUSABLE) {
            continue;
        }

        if (!ATPA_ORDER_BMP_GET(alloc_table_ptr_array[i], order)) continue;

        table = (struct alloc_table *)(alloc_table_ptr_array[i] & ATPA_ADDR_MASK);

        for (ssize_t j = table_search_start; j >= 0; j -= table_align_jump) {
            struct extended_entry *extentry;
            ssize_t index = -1;
            size_t extentry_slots_needed = 1ULL << order;

            if (table->entries[j].bitmap & ALLOCENT_EXT_FLAG) {
                ssize_t extentry_search_start;
                ssize_t extentry_align_jump;

                if (table->entries[j].bitmap == ALLOCENT_EXT_UNUSABLE) continue;

                extentry = ALLOCENT_GET_EXT_PTR(table->entries[j].ptr);
                extentry_search_start = ARRAY_SIZE(extentry->state_flags);
                extentry_align_jump = align_order > 12 ? (1LL << (align_order - 12)) : 1;
                extentry_search_start =
                    ((extentry_search_start - 1) / extentry_align_jump) * extentry_align_jump;

                for (ssize_t k = extentry_search_start; k >= 0; k -= extentry_align_jump) {
                    int allocatable = 1;
                    size_t k_idx;
                    size_t end_idx;

                    if (k < 0) continue;
                    k_idx = (size_t)k;
                    end_idx = k_idx + extentry_slots_needed;
                    if (k_idx >= ARRAY_SIZE(extentry->state_flags) ||
                        end_idx > ARRAY_SIZE(extentry->state_flags)) {
                        continue;
                    }

                    if (extentry->state_flags[k_idx] != EE_FREE) continue;

                    for (size_t idx = k_idx; idx < end_idx; idx++) {
                        if (extentry->state_flags[idx] != EE_FREE) {
                            allocatable = 0;
                            break;
                        }
                    }
                    if (!allocatable) continue;

                    index = k;
                    break;
                }

                if (index < 0) continue;

                // LOG_TRACE(LM_CAT_UNCLASSIFIED, "found (%zd.%zd.%zd) (extended)\n", i, j, index);

                {
                    size_t used_begin = (size_t)index;
                    size_t used_end = used_begin + extentry_slots_needed;

                    if (used_end > ARRAY_SIZE(extentry->state_flags)) {
                        status = STATUS_UNEXPECTED_RESULT;
                        goto has_error;
                    }

                    for (size_t idx = used_begin; idx < used_end; idx++) {
                        extentry->state_flags[idx] = EE_USED;
                    }
                }
            } else {
                index = find_free_frame_idx_bitmap_entry(table->entries[j].bitmap, align_order);
                if (index < 0) continue;

                // LOG_TRACE(LM_CAT_UNCLASSIFIED, "found (%zd.%zd.%zd) (bitmap)\n", i, j, index);

                allocate_from_bitmap_entry(&table->entries[j].bitmap, index, order);
            }

            allocated_pfn = (St_PhysFrame)(
                (i * ALLOC_TABLE_COVERAGE_PAGES) + (j * ALLOCENT_COVERAGE_PAGES) + index
            );

            // allocate and fill metadata directory & metadata table & metadata
            status = create_metadata(allocated_pfn, owner, order);
            if (!CHECK_SUCCESS(status)) goto has_error;

            *pfn = allocated_pfn;
            free_frames -= (1ULL << order);

            goto success;
        }

        ATPA_ORDER_BMP_CLR(alloc_table_ptr_array[i], order);
    }

success:
    LOG_TRACE(LM_CAT_UNCLASSIFIED, "allocated %zu frames at %013zX\n", count, allocated_pfn);

    StThread_UnlockPreemption();

    return STATUS_SUCCESS;

has_error:
    LOG_ERROR(
        LM_CAT_UNCLASSIFIED,
        "StPmm_AllocateContiguousFrame failed: count=%zu flags=%08X order=%d align_order=%d "
        "status=%08X free_frames=%zu\n",
        count,
        alloc_flags,
        order,
        align_order,
        status,
        free_frames
    );
    StThread_UnlockPreemption();

    return status;
}

StStatus StPmm_AcquireContiguousFrame(St_PhysFrame pfn __in)
{
    struct pmm_metadata *metadata;
    // uint32_t prev_refcount;

    metadata = get_metadata(pfn);

    /* prev_refcount = */ atomic_fetch_add_explicit(&metadata->refcount, 1, memory_order_relaxed);

    // LOG_TRACE(
    //     LM_CAT_UNCLASSIFIED,
    //     "refcount: %" PRId32 " -> %" PRId32 "\n",
    //     prev_refcount,
    //     prev_refcount + 1
    // );

    return STATUS_SUCCESS;
}

void StPmm_FreeContiguousFrame(St_PhysFrame pfn __in)
{
    struct pmm_metadata *metadata;
    StAllocationOwner_StrongRef owner;
    St_PageCount allocated_count;
    uint32_t prev_refcount;

    metadata = get_metadata(pfn);
    if (!metadata) {
        St_Panic(STATUS_CONFLICTING_STATE, "double free");
    }

    prev_refcount = atomic_fetch_sub_explicit(&metadata->refcount, 1, memory_order_relaxed);

    LOG_TRACE(
        LM_CAT_UNCLASSIFIED,
        "freeing %" PRIu64 " pages at %013zX\n",
        (1UL << metadata->public.order),
        pfn
    );

    if (prev_refcount > 1) return;
    if (prev_refcount == 0) {
        St_Panic(STATUS_CONFLICTING_STATE, "double free");
    }

    StThread_LockPreemption();
    owner = metadata->public.owner;
    if (owner) {
        allocated_count = (St_PageCount)1ULL << metadata->public.order;
        if (owner->page_usage_count >= allocated_count) {
            owner->page_usage_count -= allocated_count;
        } else {
            owner->page_usage_count = 0;
        }
        metadata->public.owner = NULL;
        StAllocationOwner_Release(owner);
    }
    do_free_contiguous_frame(pfn, (int)metadata->public.order);
    StThread_UnlockPreemption();
}

StStatus StPmm_GetAllocMetadata(St_PhysFrame pfn __in, StPmm_AllocationMetadata_BorrowedRef *meta __out)
{
    assert(meta);

    struct pmm_metadata *metadata;

    metadata = get_metadata(pfn);

    *meta = (StPmm_AllocationMetadata_BorrowedRef)metadata;

    return STATUS_SUCCESS;
}

StStatus StPmm_LockAndGetAllocMetadata(
    St_PhysFrame pfn __in, StPmm_AllocationMetadata_LockedRef *meta __out
)
{
    assert(meta);

    struct pmm_metadata *metadata;
    unsigned int expected = 0;

    metadata = get_metadata(pfn);

    while (!atomic_compare_exchange_strong_explicit(
        &metadata->lock,
        &expected,
        1,
        memory_order_acquire,
        memory_order_relaxed
    )) {
        expected = 0;
        StA_Pause();
    }

    *meta = (StPmm_AllocationMetadata_LockedRef)metadata;

    return STATUS_SUCCESS;
}

StStatus StPmm_UnlockAllocMetadata(StPmm_AllocationMetadata_LockedRef meta __in)
{
    struct pmm_metadata *metadata = (struct pmm_metadata *)meta;

    unsigned int expected = 1;
    atomic_compare_exchange_strong_explicit(
        &metadata->lock,
        &expected,
        0,
        memory_order_release,
        memory_order_relaxed
    );

    return STATUS_SUCCESS;
}

#ifdef TESTING

/* 시각화 헬퍼 매크로 */
#    define ANSI_COLOR_RED    "\x1b[31m"
#    define ANSI_COLOR_GREEN  "\x1b[32m"
#    define ANSI_COLOR_YELLOW "\x1b[33m"
#    define ANSI_COLOR_BLUE   "\x1b[34m"
#    define ANSI_COLOR_RESET  "\x1b[0m"

// 특정 페이지(0~31)가 비트맵상에서 할당되어 있는지 확인하는 헬퍼
static int is_page_allocated_in_bitmap(uint64_t bitmap, int page_idx)
{
    // Order 5 (Whole 32 pages) check
    if (bitmap & ALLOCENT_BMP_32P_MASK) return 1;

    // Order 4 (16 pages) check
    int o4_idx = page_idx / 16;
    if (bitmap & (1ULL << (ALLOCENT_BMP_16P_POS + o4_idx))) return 1;

    // Order 3 (8 pages) check
    int o3_idx = page_idx / 8;
    if (bitmap & (1ULL << (ALLOCENT_BMP_8P_POS + o3_idx))) return 1;

    // Order 2 (4 pages) check
    int o2_idx = page_idx / 4;
    if (bitmap & (1ULL << (ALLOCENT_BMP_4P_POS + o2_idx))) return 1;

    // Order 1 (2 pages) check
    int o1_idx = page_idx / 2;
    if (bitmap & (1ULL << (ALLOCENT_BMP_2P_POS + o1_idx))) return 1;

    // Order 0 (1 page) check
    if (bitmap & (1ULL << (ALLOCENT_BMP_1P_POS + page_idx))) return 1;

    return 0;
}

/**
 * @brief 특정 범위의 물리 프레임 상태를 상세하게 출력합니다.
 * @param start_pfn 시작 프레임 번호
 * @param count 출력할 프레임 개수
 */
void StPmm_DebugDumpRegion(St_PhysFrame start_pfn, size_t count)
{
    St_PhysFrame end_pfn = start_pfn + count;
    St_PhysFrame current_pfn = start_pfn;

    printf("\n=== PMM Memory Map Dump [PFN 0x%lx - 0x%lx] ===\n", start_pfn, end_pfn - 1);
    printf(
        "Legend: " ANSI_COLOR_GREEN "." ANSI_COLOR_RESET " Free, " ANSI_COLOR_RED
        "*" ANSI_COLOR_RESET " Used, " ANSI_COLOR_YELLOW "X" ANSI_COLOR_RESET
        " Unusable, " ANSI_COLOR_BLUE "H" ANSI_COLOR_RESET " HugeAlloc\n\n"
    );

    while (current_pfn < end_pfn) {
        size_t table_idx = current_pfn / ALLOC_TABLE_COVERAGE_PAGES;
        size_t entry_idx = (current_pfn % ALLOC_TABLE_COVERAGE_PAGES) / ALLOCENT_COVERAGE_PAGES;
        size_t page_idx = current_pfn % ALLOCENT_COVERAGE_PAGES;

        // 주소 헤더 출력 (32 프레임 단위 줄바꿈)
        if (current_pfn % 32 == 0 || current_pfn == start_pfn) {
            if (current_pfn != start_pfn) printf("\n");
            printf("0x%08lx: ", current_pfn);
        }

        // 1. ATPA Level Check
        if (table_idx >= ALLOC_TABLE_PTR_ARRAY_SIZE) {
            printf("?");  // Out of bound
            current_pfn++;
            continue;
        }

        uintptr_t atpa_entry = alloc_table_ptr_array[table_idx];

        if (atpa_entry == ATPA_UNUSABLE) {
            printf(ANSI_COLOR_YELLOW "X" ANSI_COLOR_RESET);
            current_pfn++;
            continue;
        }
        if (atpa_entry == ATPA_HUGE_ALLOC) {
            printf(ANSI_COLOR_BLUE "H" ANSI_COLOR_RESET);
            current_pfn++;
            continue;
        }
        if (atpa_entry == ATPA_FREE) {
            printf(ANSI_COLOR_GREEN "." ANSI_COLOR_RESET);
            current_pfn++;
            continue;
        }

        // 2. Table Level Check
        struct alloc_table *table = (struct alloc_table *)(atpa_entry & ATPA_ADDR_MASK);
        union alloc_table_entry *entry = &table->entries[entry_idx];

        // 3. Entry Level Check (Bitmap or Extended)
        char *symbol = "?";

        if (entry->bitmap == ALLOCENT_EXT_UNUSABLE) {
            symbol = ANSI_COLOR_YELLOW "X" ANSI_COLOR_RESET;
        } else if (entry->bitmap == ALLOCENT_BMP_FREE) {
            symbol = ANSI_COLOR_GREEN "." ANSI_COLOR_RESET;
        } else if (entry->bitmap == ALLOCENT_BMP_FULL_ALLOC) {
            symbol = ANSI_COLOR_RED "*" ANSI_COLOR_RESET;
        } else if (entry->bitmap & ALLOCENT_EXT_FLAG) {
            // Extended Entry
            struct extended_entry *ext = ALLOCENT_GET_EXT_PTR(entry->ptr);
            uint8_t flag = ext->state_flags[page_idx];
            switch (flag) {
            case EE_FREE:
                symbol = ANSI_COLOR_GREEN "." ANSI_COLOR_RESET;
                break;
            case EE_USED:
                symbol = ANSI_COLOR_RED "*" ANSI_COLOR_RESET;
                break;
            case EE_UNUSABLE:
                symbol = ANSI_COLOR_YELLOW "X" ANSI_COLOR_RESET;
                break;
            default:
                symbol = "?";
                break;
            }
        } else {
            // Partial Bitmap
            if (is_page_allocated_in_bitmap(entry->bitmap, page_idx)) {
                symbol = ANSI_COLOR_RED "*" ANSI_COLOR_RESET;
            } else {
                symbol = ANSI_COLOR_GREEN "." ANSI_COLOR_RESET;
            }
        }

        printf("%s", symbol);  // 문자열 포맷 주의 (색상 코드 포함)
        current_pfn++;
    }
    printf("\n=================================================\n");
}

/**
 * @brief ATPA(최상위 테이블)의 전체적인 점유 상태를 요약해서 보여줍니다.
 * 64MiB 블록 단위로 출력됩니다.
 */
void StPmm_DebugDumpAtpa(void)
{
    printf("\n=== ATPA Overview (Block = 64 MiB) ===\n");
    int count = 0;
    for (int i = 0; i < ALLOC_TABLE_PTR_ARRAY_SIZE; i++) {
        uintptr_t val = alloc_table_ptr_array[i];

        // 생략 로직: 연속된 UNUSABLE은 압축해서 표현
        if (val == ATPA_UNUSABLE) {
            count++;
            continue;
        }

        if (count > 0) {
            printf("[Unusable x %d blocks] ", count);
            count = 0;
        }

        printf("[%04d: ", i);
        if (val == ATPA_FREE)
            printf(ANSI_COLOR_GREEN "FREE" ANSI_COLOR_RESET);
        else if (val == ATPA_HUGE_ALLOC)
            printf(ANSI_COLOR_BLUE "HUGE" ANSI_COLOR_RESET);
        else
            printf("TBL");  // Table exists (Partial/Full)
        printf("] ");

        if ((i + 1) % 8 == 0) printf("\n");
    }
    if (count > 0) printf("[Unusable x %d blocks]\n", count);
    printf("\n");
}

#endif
