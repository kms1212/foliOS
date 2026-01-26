#include <strata/mm.h>

#include <inttypes.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <strata/arch/intrinsics/misc.h>
#include <strata/arch/mmu.h>

#include <strata/log.h>
#include <strata/macros.h>
#include <strata/panic.h>
#include <strata/status.h>
#include <strata/types.h>

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
#define ATPA_ORDER_BMP_GET(b, o) (!!(b & (1ULL << (o))))
#define ATPA_ORDER_BMP_SET(b, o) (b |= (1ULL << (o)))
#define ATPA_ORDER_BMP_CLR(b, o) (b &= ~(1ULL << (o)))

/*
    [ Metadata Directory Entry Format ] (64-bit)

    metadata entry
    pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp ppp0 0001
    |------------------ pointer to metadata (32B-aligned) ------------------|

    metadata table entry
    pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pppp pp00 0000 0000
    |------------ pointer to metadata table (1kiB-aligned) -----------|
*/
#define METADATA_DIR_ENTRY_LARGE_FLAG 0x0000000000000001ULL

#define ATPA_INDEX_LIMIT_4G  (ALIGN_DIV(1LL << 32, PAGE_SIZE * ALLOC_TABLE_COVERAGE_PAGES))
#define ATPA_INDEX_LIMIT_16M (ALIGN_DIV(1LL << 24, PAGE_SIZE * ALLOC_TABLE_COVERAGE_PAGES))
#define ATPA_INDEX_LIMIT_1M  (ALIGN_DIV(1LL << 20, PAGE_SIZE * ALLOC_TABLE_COVERAGE_PAGES))

#define ALLOC_TABLE_ENTRY_INDEX_LIMIT_16M                                                          \
    (ALIGN_DIV(1LL << 24, PAGE_SIZE * ALLOCENT_COVERAGE_PAGES))
#define ALLOC_TABLE_ENTRY_INDEX_LIMIT_1M (ALIGN_DIV(1LL << 20, PAGE_SIZE * ALLOCENT_COVERAGE_PAGES))

#define ctz64(x)    __builtin_ctzll(x)
#define clz64(x)    __builtin_clzll(x)
#define popcnt64(x) __builtin_popcountll(x)

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
#define EARLY_METADATA_TABLE_POOL_COUNT 128
#define EARLY_METADATA_DIR_POOL_COUNT   128
#define EARLY_METADATA_POOL_COUNT       128

#define ALLOCENT_COVERAGE_PAGES    PAGES_PER_ALLOCTABLE_ENTRY
#define ALLOCENT_COVERAGE_BYTES    (ALLOCENT_COVERAGE_PAGES * PAGE_SIZE)
#define ALLOC_TABLE_COVERAGE_PAGES (ALLOC_TABLE_ENTRY_COUNT * PAGES_PER_ALLOCTABLE_ENTRY)
#define ALLOC_TABLE_COVERAGE_BYTES (ALLOC_TABLE_COVERAGE_PAGES * PAGE_SIZE)
#define ATPA_COVERAGE_PAGES        (ALLOC_TABLE_PTR_ARRAY_SIZE * ALLOC_TABLE_COVERAGE_PAGES)
#define ATPA_COVERAGE_BYTES        (ATPA_COVERAGE_PAGES * PAGE_SIZE)

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

struct internal_public_metadata_view {
    St_PhysFrame pfn;
    void *owner;
    uint32_t flags;
    uint32_t order;
};

_Static_assert(
    sizeof(struct StPmm_AllocationMetadata) == sizeof(struct internal_public_metadata_view),
    "public metadata struct size mismatch"
);

struct metadata {
    struct internal_public_metadata_view public;

    struct metadata *owner_next, *owner_prev;

    atomic_uint_fast32_t refcount;
    atomic_uint_fast32_t lock;

    uint8_t padding
        [64 - sizeof(struct internal_public_metadata_view) - sizeof(struct metadata *) * 2 -
         sizeof(atomic_uint_fast32_t) * 2];
} __aligned(64);

_Static_assert(
    sizeof(struct metadata) == 64, "metadata struct size mismatch (sizeof(struct metadata) != 64)"
);

struct metadata_table {
    struct metadata entries[PAGES_PER_ALLOCTABLE_ENTRY];
} __packed __aligned(1024);

struct metadata_directory {
    uintptr_t entries[ALLOC_TABLE_ENTRY_COUNT];
} __packed __aligned(PAGE_SIZE);

static uintptr_t metadata_dir_ptr_array[ALLOC_TABLE_PTR_ARRAY_SIZE] __aligned(PAGE_SIZE);

/*
    Use this arrays for first some allocations.
    This is because we don't have enough memory to allocate structs in early
    stage.
    It might be sufficient for most cases.
*/
static struct alloc_table early_alloc_table_pool[EARLY_ALLOC_TABLE_POOL_COUNT] __aligned(PAGE_SIZE);
static int early_alloctbl_pool_used_count = 0;

static struct extended_entry early_extended_entry_pool[EARLY_EXTENDED_ENTRY_POOL_COUNT]
    __aligned(PAGE_SIZE);
static int early_extentry_pool_used_count = 0;

static struct metadata early_metadata_pool[EARLY_METADATA_POOL_COUNT] __aligned(PAGE_SIZE);
static int early_metadata_pool_used_count = 0;

static struct metadata_table early_metadata_table_pool[EARLY_METADATA_TABLE_POOL_COUNT]
    __aligned(PAGE_SIZE);
static int early_metadata_table_pool_used_count = 0;

static struct metadata_directory early_metadata_dir_pool[EARLY_METADATA_DIR_POOL_COUNT]
    __aligned(PAGE_SIZE);
static int early_metadata_dir_pool_used_count = 0;

/* statistics */
static size_t total_frames = 0;
static size_t free_frames = 0;

/* status flags */
static int allocation_available = 0;
static int remarking_unavailable = 0;

static int get_order(size_t count)
{
    size_t temp;

    if (count == 0) return -1;
    if (count <= 1) return 0;

    temp = count - 1;
    return 64 - clz64(temp);
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

static inline void propagate_up(uint64_t *entry, int start_page_idx, int start_order, int set)
{
    int offsets[] = {
        ALLOCENT_BMP_1P_POS,
        ALLOCENT_BMP_2P_POS,
        ALLOCENT_BMP_4P_POS,
        ALLOCENT_BMP_8P_POS,
        ALLOCENT_BMP_16P_POS,
        ALLOCENT_BMP_32P_POS
    };

    int current_idx = start_page_idx >> start_order;

    for (int o = start_order; o < 5; o++) {
        int next_idx = current_idx >> 1;    // Parent index
        int sibling_idx = current_idx ^ 1;  // Sibling index

        int parent_bit_pos = offsets[o + 1] + next_idx;
        int sibling_bit_pos = offsets[o] + sibling_idx;

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

// TODO: search inside entry with explicit alignment
static inline int find_free_frame_idx_bitmap_entry(uint64_t entry, int order, int align_order)
{
    uint64_t mask, inverted;
    int pos;

    switch (order) {
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

    return (ctz64(inverted) - pos) * (1 << order);
}

static inline void allocate_from_bitmap_entry(uint64_t *entry, int index, int order)
{
    uint64_t prev = *entry;

    switch (order) {
    case 0:
        *entry |= alloc_masks[index];
        break;
    case 1:
        *entry |= alloc_masks[32 + index / 2];
        break;
    case 2:
        *entry |= alloc_masks[48 + index / 4];
        break;
    case 3:
        *entry |= alloc_masks[56 + index / 8];
        break;
    case 4:
        *entry |= alloc_masks[60 + index / 16];
        break;
    case 5:
        *entry |= alloc_masks[62];
        break;
    default:
        return;
    }

    propagate_up(entry, index, order, 1);

    LOG_TRACE("bitmap modified: 0x%016" PRIX64 " -> 0x%016" PRIX64 "\n", prev, *entry);
}

static inline void free_to_table(struct alloc_table *table, St_PhysFrame index, int order)
{
    St_PhysFrame entry_idx = index / ALLOCENT_COVERAGE_PAGES;
    St_PhysFrame page_idx = index % ALLOCENT_COVERAGE_PAGES;

    if ((int64_t)table->entries[entry_idx].bitmap < 0)
        return;  // Extended entry - manual handling required

    switch (order) {
    case 0:
        table->entries[entry_idx].bitmap &= ~alloc_masks[page_idx];
        break;
    case 1:
        table->entries[entry_idx].bitmap &= ~alloc_masks[32 + page_idx / 2];
        break;
    case 2:
        table->entries[entry_idx].bitmap &= ~alloc_masks[48 + page_idx / 4];
        break;
    case 3:
        table->entries[entry_idx].bitmap &= ~alloc_masks[56 + page_idx / 8];
        break;
    case 4:
        table->entries[entry_idx].bitmap &= ~alloc_masks[60 + page_idx / 16];
        break;
    case 5:
        table->entries[entry_idx].bitmap &= ~alloc_masks[62];
        break;
    }
    propagate_up(&table->entries[entry_idx].bitmap, (int)page_idx, order, 0);
}

static StStatus get_or_create_table(size_t table_idx, struct alloc_table **table)
{
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

    // Need to allocate a new table.
    if (early_alloctbl_pool_used_count < EARLY_ALLOC_TABLE_POOL_COUNT) {
        new_table = &early_alloc_table_pool[early_alloctbl_pool_used_count++];
    } else {
        // TODO: use newly allocated table pool created at late init phase.
        LOG_ERROR("failed to create allocation table");
        return STATUS_UNIMPLEMENTED;
    }

    // Initialize table to all allocated or all unusable.
    if (create_as_free) {
        for (int i = 0; i < ARRAY_SIZE(new_table->entries); i++) {
            new_table->entries[i].bitmap = ALLOCENT_BMP_FREE;
        }
    } else {
        for (int i = 0; i < ARRAY_SIZE(new_table->entries); i++) {
            new_table->entries[i].bitmap = ALLOCENT_EXT_UNUSABLE;
        }
    }

    // Set table pointer.
    alloc_table_ptr_array[table_idx] = (uintptr_t)new_table | ATPA_ORDER_BMP_MASK;

    LOG_TRACE("created table %p -> [%ld]\n", (void *)new_table, table_idx);

    if (table) *table = new_table;

    return STATUS_SUCCESS;
}

static StStatus get_or_create_extentry(
    struct alloc_table *table, int entry_idx, struct extended_entry **entry
)
{
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

    // need to allocate a new extended entry.
    if (early_extentry_pool_used_count < EARLY_EXTENDED_ENTRY_POOL_COUNT) {
        new_entry = &early_extended_entry_pool[early_extentry_pool_used_count++];
    } else {
        // TODO: use newly allocated table pool created at late init phase.
        LOG_ERROR("failed to create extended entry");
        return STATUS_UNIMPLEMENTED;
    }

    if (create_as_unusable) {
        for (int i = 0; i < ARRAY_SIZE(new_entry->state_flags); i++) {
            new_entry->state_flags[i] = EE_UNUSABLE;
        }
    } else {
        for (int i = 0; i < ARRAY_SIZE(new_entry->state_flags); i++) {
            new_entry->state_flags[i] = EE_FREE;
        }
    }

    ALLOCENT_SET_EXT_PTR(table->entries[entry_idx].ptr, new_entry);

#if UINTPTR_MAX != 0xFFFFFFFFFFFFFFFFULL
    table->entries[entry_idx].bitmap |= ALLOCENT_EXT_FLAG;

#endif

    LOG_TRACE("created extentry %p -> [%d]\n", (void *)new_entry, entry_idx);

    if (entry) *entry = new_entry;

    return STATUS_SUCCESS;
}

static StStatus get_or_create_metadata_dir(
    size_t mdpa_idx, int order, struct metadata_directory **dir
)
{
    struct metadata_directory *metadata_dir;

    if (order < 14) {
        order = 14;
    }

    if (metadata_dir_ptr_array[mdpa_idx]) {
        if (dir) *dir = (struct metadata_directory *)metadata_dir_ptr_array[mdpa_idx];

        return STATUS_SUCCESS;
    }

    if (early_metadata_dir_pool_used_count < EARLY_METADATA_DIR_POOL_COUNT) {
        metadata_dir = &early_metadata_dir_pool[early_metadata_dir_pool_used_count++];
    } else {
        // TODO: use newly allocated table pool created at late init phase.
        LOG_ERROR("failed to create metadata directory");
        return STATUS_UNIMPLEMENTED;
    }

    for (int i = 0; i < (1 << (order - 14)); i++) {
        metadata_dir_ptr_array[mdpa_idx + i] = (uintptr_t)metadata_dir;
    }

    if (dir) *dir = metadata_dir;

    return STATUS_SUCCESS;
}

static StStatus get_or_create_metadata_table(
    size_t mdpa_idx, size_t metadata_dir_entry_idx, int order, struct metadata_table **table
)
{
    StStatus status;
    struct metadata_directory *metadata_dir;
    struct metadata_table *metadata_table;

    if (order < 5) {
        order = 5;
    }

    status = get_or_create_metadata_dir(mdpa_idx, order, &metadata_dir);
    if (!CHECK_SUCCESS(status)) return status;

    if (metadata_dir->entries[metadata_dir_entry_idx] & METADATA_DIR_ENTRY_LARGE_FLAG) {
        return STATUS_INVALID_RESOURCE;
    }

    if (metadata_dir->entries[metadata_dir_entry_idx]) {
        if (table)
            *table = (struct metadata_table *)(metadata_dir->entries[metadata_dir_entry_idx] &
                                               ~METADATA_DIR_ENTRY_LARGE_FLAG);

        return STATUS_SUCCESS;
    }

    if (early_metadata_table_pool_used_count < EARLY_METADATA_TABLE_POOL_COUNT) {
        metadata_table = &early_metadata_table_pool[early_metadata_table_pool_used_count++];
    } else {
        // TODO: use newly allocated table pool created at late init phase.
        LOG_ERROR("failed to create metadata table");
        return STATUS_UNIMPLEMENTED;
    }

    for (int i = 0; i < (1 << (order - 5)); i++) {
        metadata_dir->entries[metadata_dir_entry_idx + i] = (uintptr_t)metadata_table;
    }

    if (table) *table = metadata_table;

    return STATUS_SUCCESS;
}

static StStatus create_metadata(St_PhysFrame pfn, void *owner, int order)
{
    StStatus status;

    struct metadata *metadata;
    struct metadata_directory *metadata_dir;
    struct metadata_table *metadata_table;

    size_t mdpa_idx = pfn / ALLOC_TABLE_COVERAGE_PAGES;
    size_t metadata_dir_entry_idx = (pfn / ALLOCENT_COVERAGE_PAGES) % ALLOC_TABLE_ENTRY_COUNT;
    size_t metadata_table_entry_idx = pfn % ALLOCENT_COVERAGE_PAGES;

    if (order >= 14) {
        if (early_metadata_pool_used_count < EARLY_METADATA_POOL_COUNT) {
            metadata = &early_metadata_pool[early_metadata_pool_used_count++];
        } else {
            // TODO: use newly allocated table pool created at late init phase.
            LOG_ERROR("failed to create metadata");
            return STATUS_UNIMPLEMENTED;
        }

        for (size_t i = 0; i < (1 << (order - 14)); i++) {
            metadata_dir_ptr_array[mdpa_idx + i] = (uintptr_t)metadata;
        }
    } else if (order >= 5) {
        status = get_or_create_metadata_dir(mdpa_idx, order, &metadata_dir);
        if (!CHECK_SUCCESS(status)) return status;

        if (early_metadata_pool_used_count < EARLY_METADATA_POOL_COUNT) {
            metadata = &early_metadata_pool[early_metadata_pool_used_count++];
        } else {
            // TODO: use newly allocated table pool created at late init phase.
            LOG_ERROR("failed to create metadata");
            return STATUS_UNIMPLEMENTED;
        }

        for (size_t i = 0; i < (1 << (order - 5)); i++) {
            metadata_dir->entries[metadata_dir_entry_idx + i] =
                (uintptr_t)metadata | METADATA_DIR_ENTRY_LARGE_FLAG;
        }
    } else {
        status =
            get_or_create_metadata_table(mdpa_idx, metadata_dir_entry_idx, order, &metadata_table);
        if (!CHECK_SUCCESS(status)) return status;

        for (size_t i = 1; i < (1 << order); i++) {
            metadata_table->entries[metadata_table_entry_idx + i].public.order = order;
        }

        metadata = &metadata_table->entries[metadata_table_entry_idx];
    }

    metadata->lock = 0;
    metadata->refcount = 1;
    metadata->public.order = order;
    metadata->public.pfn = pfn;
    metadata->public.flags = 0;
    metadata->public.owner = owner;

    LOG_TRACE("refcount: set to 1\n");

    return STATUS_SUCCESS;
}

static StStatus get_metadata(St_PhysFrame pfn, struct metadata **metadata)
{
    size_t table_idx = pfn / ALLOC_TABLE_COVERAGE_PAGES;
    size_t entry_idx = (pfn / ALLOCENT_COVERAGE_PAGES) % ALLOC_TABLE_ENTRY_COUNT;
    size_t slot_idx = pfn % ALLOCENT_COVERAGE_PAGES;
    struct metadata_directory *metadata_directory;
    struct metadata_table *metadata_table;

    if (alloc_table_ptr_array[table_idx] == ATPA_FREE ||
        alloc_table_ptr_array[table_idx] == ATPA_UNUSABLE) {
        return STATUS_NOT_ALLOCATED;
    }

    if (alloc_table_ptr_array[table_idx] == ATPA_HUGE_ALLOC) {
        LOG_TRACE("metadata found at (table: %zd) (huge alloc)\n", table_idx);

        *metadata = (struct metadata *)metadata_dir_ptr_array[table_idx];

        return STATUS_SUCCESS;
    }

    metadata_directory = (struct metadata_directory *)metadata_dir_ptr_array[table_idx];
    if (!metadata_directory) return STATUS_NOT_ALLOCATED;
    if (!metadata_directory->entries[entry_idx]) return STATUS_NOT_ALLOCATED;

    if (metadata_directory->entries[entry_idx] & METADATA_DIR_ENTRY_LARGE_FLAG) {
        LOG_TRACE(
            "metadata found at (table: %zd, entry: %zd) (large flag)\n",
            table_idx,
            entry_idx
        );

        *metadata = (struct metadata *)(metadata_directory->entries[entry_idx] &
                                        ~METADATA_DIR_ENTRY_LARGE_FLAG);

        return STATUS_SUCCESS;
    }

    metadata_table = (struct metadata_table *)(metadata_directory->entries[entry_idx] &
                                               ~METADATA_DIR_ENTRY_LARGE_FLAG);
    if (!metadata_table) return STATUS_NOT_ALLOCATED;
    if (!metadata_table->entries[slot_idx].refcount) return STATUS_NOT_ALLOCATED;  // HERE

    LOG_TRACE(
        "metadata found at (table: %zd, entry: %zd, slot: %zd)\n",
        table_idx,
        entry_idx,
        slot_idx
    );

    *metadata = &metadata_table->entries[slot_idx];

    return STATUS_SUCCESS;
}

static void do_free_contiguous_frame(St_PhysFrame pfn, int order)
{
    size_t table_idx = pfn / ALLOC_TABLE_COVERAGE_PAGES;
    size_t entry_idx = (pfn % ALLOC_TABLE_COVERAGE_PAGES) / ALLOCENT_COVERAGE_PAGES;
    size_t slot_idx = pfn % ALLOCENT_COVERAGE_PAGES;
    struct alloc_table *table;
    size_t slots_needed;
    struct metadata_directory *metadata_dir;
    struct metadata_table *metadata_table;

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

        // TODO: unallocate metadata
        for (int i = 0; i < slots_needed; i++) {
            metadata_dir_ptr_array[table_idx + i] = 0;
        }

        free_frames += (1ULL << order);
        return;
    }

    if (alloc_table_ptr_array[table_idx] == ATPA_FREE ||
        alloc_table_ptr_array[table_idx] == ATPA_UNUSABLE) {
        St_Panic(STATUS_CONFLICTING_STATE, "double free");
    }

    table = (struct alloc_table *)(alloc_table_ptr_array[table_idx] & ATPA_ADDR_MASK);

    metadata_dir = (struct metadata_directory *)metadata_dir_ptr_array[table_idx];

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

        // TODO: unallocate metadata directory & metadata
        for (int i = 0; i < entries_needed; i++) {
            metadata_dir->entries[entry_idx + i] = 0;
        }

        free_frames += (1ULL << order);
        return;
    }

    // 3. Small Allocation
    slots_needed = 1ULL << order;

    metadata_table = (struct metadata_table *)(metadata_dir->entries[entry_idx] &
                                               ~METADATA_DIR_ENTRY_LARGE_FLAG);

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
        free_to_table(table, slot_idx, order);
    }

    for (size_t i = 1; i < slots_needed; i++) {
        metadata_table->entries[slot_idx + i].public.order = 0;
    }

    free_frames += (1ULL << order);
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

    return STATUS_SUCCESS;
}

StStatus StPmm_MarkUsableContiguousFrame(St_PhysFrame base __in, St_PhysFrame limit __in)
{
    // Mark frames as Usable (Free).
    // This adds them to the pool.
    StStatus status;
    struct alloc_table *table;
    struct extended_entry *extentry;
    unsigned int table_idx, entry_idx;

    if (remarking_unavailable) return STATUS_CONFLICTING_STATE;

    // mark tables as free (64 MiB granularity)
    while (base <= limit) {
        table_idx = base / ALLOC_TABLE_COVERAGE_PAGES;

        // If we can mark entire table to free, do so.
        if (!(base % ALLOC_TABLE_COVERAGE_PAGES) &&
            limit >= base + ALLOC_TABLE_COVERAGE_PAGES - 1) {
            LOG_TRACE("writing to ATPA\n");

            // it's ok to mark entire allocation table to usable
            alloc_table_ptr_array[base / ALLOC_TABLE_COVERAGE_PAGES] = ATPA_FREE;
            base += ALLOC_TABLE_COVERAGE_PAGES;

            continue;
        }

        // Get or create table.
        status = get_or_create_table(table_idx, &table);
        if (!CHECK_SUCCESS(status)) return status;

        LOG_TRACE("writing to table: %p\n", (void *)table);

        // Mark frame entries as free. (128 KiB granularity)
        while (base <= limit && base / ALLOC_TABLE_COVERAGE_PAGES == table_idx) {

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

            LOG_TRACE("writing to extentry: %p\n", (void *)extentry);

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
    while (base <= limit) {
        table_idx = base / ALLOC_TABLE_COVERAGE_PAGES;

        // If we can mark entire table to unusable, do so.
        if (!(base % ALLOC_TABLE_COVERAGE_PAGES) &&
            limit >= base + ALLOC_TABLE_COVERAGE_PAGES - 1) {
            // it's ok to mark entire allocation table to unusable
            alloc_table_ptr_array[base / ALLOC_TABLE_COVERAGE_PAGES] = ATPA_UNUSABLE;
            base += ALLOC_TABLE_COVERAGE_PAGES;

            continue;
        }

        // Get or create table.
        status = get_or_create_table(table_idx, &table);
        if (!CHECK_SUCCESS(status)) return status;

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
    remarking_unavailable = 1;
    allocation_available = 1;

    for (size_t i = 0; i < ARRAY_SIZE(alloc_table_ptr_array); i++) {
        struct alloc_table *table;

        if (alloc_table_ptr_array[i] == ATPA_UNUSABLE) continue;

        if (alloc_table_ptr_array[i] == ATPA_HUGE_ALLOC) {
            total_frames += ALLOC_TABLE_COVERAGE_PAGES;

            continue;
        }

        if (alloc_table_ptr_array[i] == ATPA_FREE) {
            total_frames += ALLOC_TABLE_COVERAGE_PAGES;
            free_frames += ALLOC_TABLE_COVERAGE_PAGES;

            continue;
        }

        table = (struct alloc_table *)(alloc_table_ptr_array[i] & ATPA_ADDR_MASK);
        for (size_t j = 0; j < ARRAY_SIZE(table->entries); j++) {
            struct extended_entry *extentry;

            if (table->entries[j].bitmap == ALLOCENT_EXT_UNUSABLE) continue;

            if (table->entries[j].bitmap == ALLOCENT_BMP_FREE) {
                total_frames += ALLOCENT_COVERAGE_PAGES;
                free_frames += ALLOCENT_COVERAGE_PAGES;

                continue;
            }

            if (!(table->entries[j].bitmap & ALLOCENT_EXT_FLAG)) {
                int free_cnt = 32 - popcnt64(table->entries[j].bitmap & ALLOCENT_BMP_1P_MASK);
                total_frames += ALLOCENT_COVERAGE_PAGES;
                free_frames += free_cnt;

                continue;
            }

            if (table->entries[j].bitmap == ALLOCENT_EXT_UNUSABLE) continue;

            extentry = ALLOCENT_GET_EXT_PTR(table->entries[j].ptr);

            LOG_TRACE("extentry %p[%ld] = %p\n", (void *)table, j, (void *)extentry);

            for (size_t k = 0; k < ARRAY_SIZE(extentry->state_flags); k++) {
                if (extentry->state_flags[k] == EE_UNUSABLE) continue;

                total_frames++;

                if (extentry->state_flags[k] != EE_FREE) continue;

                free_frames++;
            }
        }
    }

    // TODO: allocate additional pools

    return STATUS_SUCCESS;
}

StStatus StPmm_GetTotalFrameCount(St_PageCount *frame_count __out)
{
    *frame_count = total_frames;
    return STATUS_SUCCESS;
}

StStatus StPmm_GetFreeFrameCount(St_PageCount *frame_count __out)
{
    *frame_count = free_frames;
    return STATUS_SUCCESS;
}

StStatus StPmm_AllocateContiguousFrame(
    St_PhysFrame *pfn __out, St_PageCount count __in, StPmm_AllocFlags alloc_flags __in
)
{
    StStatus status;
    St_PhysFrame allocated_pfn;
    int order;
    uint32_t below_value;
    int align_order;
    ssize_t atpa_search_start;
    size_t atpa_align_jump;
    struct alloc_table *table;
    ssize_t table_search_start;
    size_t table_align_jump;

    if (!allocation_available) return STATUS_CONFLICTING_STATE;

    order = get_order(count);
    if (order < 0) return STATUS_INVALID_VALUE;

    LOG_TRACE("allocation order: %d\n", order);

    below_value = alloc_flags & PMM_BELOW_MASK;
    align_order = ((alloc_flags & PMM_ALIGN_MASK) >> 4) - 12;

    if (align_order < order) {
        align_order = order;
    }

    LOG_TRACE("align order: %d\n", align_order);

    atpa_search_start = ARRAY_SIZE(alloc_table_ptr_array);

    switch (below_value) {
    case PMM_BELOW_NONE:
        break;
    case PMM_BELOW_1M:
        if (order > 8) return STATUS_INSUFFICIENT_MEMORY;
        if (atpa_search_start > ATPA_INDEX_LIMIT_1M) {
            atpa_search_start = ATPA_INDEX_LIMIT_1M;
        }
        break;
    case PMM_BELOW_16M:
        if (order > 12) return STATUS_INSUFFICIENT_MEMORY;
        if (atpa_search_start > ATPA_INDEX_LIMIT_16M) {
            atpa_search_start = ATPA_INDEX_LIMIT_16M;
        }
        break;
    case PMM_BELOW_4G:
        if (order > 20) return STATUS_INSUFFICIENT_MEMORY;
        if (atpa_search_start > ATPA_INDEX_LIMIT_4G) {
            atpa_search_start = ATPA_INDEX_LIMIT_4G;
        }
        break;
    default:
        return STATUS_INVALID_VALUE;
    }

    atpa_align_jump = align_order > 13 ? (1ULL << (align_order - 13)) : 1;
    atpa_search_start = ((atpa_search_start / atpa_align_jump) - 1) * atpa_align_jump;

    LOG_TRACE(
        "ATPA search start: %zu, negative jump amount: %zu\n",
        atpa_search_start,
        atpa_align_jump
    );

    // 1. order >= 14 is a huge allocation. Use whole ATPA entries.
    if (order >= 14) {
        size_t atpa_slots_needed = (1ULL << (order - 14));

        LOG_TRACE("ATPA slots needed: %zu\n", atpa_slots_needed);

        // find first fit
        for (ssize_t i = atpa_search_start; i >= 0; i -= atpa_align_jump) {
            int allocatable = 1;

            if (alloc_table_ptr_array[i] != ATPA_FREE) continue;

            for (size_t j = 1; j < atpa_slots_needed; j++) {
                if (alloc_table_ptr_array[i + j] != ATPA_FREE) {
                    allocatable = 0;
                    break;
                }
            }

            if (!allocatable) continue;
            LOG_TRACE("found (table: %zd)\n", i);

            allocated_pfn = i * ALLOC_TABLE_COVERAGE_PAGES;

            // mark all ATPA entries as allocated
            LOG_TRACE("filling ATPA entry %zd-%zd\n", i, i + atpa_slots_needed - 1);
            for (size_t j = 0; j < atpa_slots_needed; j++) {
                alloc_table_ptr_array[i + j] = ATPA_HUGE_ALLOC;
            }

            // allocate and fill metadata
            status = create_metadata(allocated_pfn, NULL, order);
            if (!CHECK_SUCCESS(status)) return status;

            *pfn = allocated_pfn;
            free_frames -= (1ULL << order);
            return STATUS_SUCCESS;
        }

        return STATUS_INSUFFICIENT_MEMORY;
    }

    table_search_start = ARRAY_SIZE(table->entries);
    table_align_jump = align_order > 5 ? (1ULL << (align_order - 5)) : 1;
    table_search_start = ((table_search_start / table_align_jump) - 1) * table_align_jump;

    switch (below_value) {
    case PMM_BELOW_1M:
        if (order > 8) return STATUS_INSUFFICIENT_MEMORY;
        if (table_search_start > ALLOC_TABLE_ENTRY_INDEX_LIMIT_1M) {
            table_search_start = ALLOC_TABLE_ENTRY_INDEX_LIMIT_1M;
        }
        break;
    case PMM_BELOW_16M:
        if (order > 12) return STATUS_INSUFFICIENT_MEMORY;
        if (table_search_start > ALLOC_TABLE_ENTRY_INDEX_LIMIT_16M) {
            table_search_start = ALLOC_TABLE_ENTRY_INDEX_LIMIT_16M;
        }
        break;
    }

    LOG_TRACE(
        "table search start: %zu, negative jump amount: %zu\n",
        table_search_start,
        table_align_jump
    );

    // 2. 5 <= order < 14 is a normal allocation. Use whole allocation table entries.
    if (order >= 5) {
        size_t table_entries_needed = (1ULL << (order - 5));

        for (ssize_t i = atpa_search_start; i >= 0; i -= atpa_align_jump) {
            if (alloc_table_ptr_array[i] == ATPA_FREE) {
                LOG_TRACE("found (table: %zd, entry: %zd)\n", i, table_search_start);

                status = get_or_create_table(i, &table);
                if (!CHECK_SUCCESS(status)) return status;

                for (size_t j = 0; j < table_entries_needed; j++) {
                    // mark the whole entry as allocated
                    table->entries[table_search_start + j].bitmap = ALLOCENT_BMP_FULL_ALLOC;
                }

                allocated_pfn =
                    i * ALLOC_TABLE_COVERAGE_PAGES + table_search_start * ALLOCENT_COVERAGE_PAGES;

                // allocate and fill metadata directory & metadata
                status = create_metadata(allocated_pfn, NULL, order);
                if (!CHECK_SUCCESS(status)) return status;

                LOG_TRACE(
                    "filling allocation table entry %zd-%zd\n",
                    table_search_start,
                    table_search_start + table_entries_needed - 1
                );
                *pfn = allocated_pfn;
                free_frames -= (1ULL << order);
                return STATUS_SUCCESS;
            }

            if (alloc_table_ptr_array[i] == ATPA_HUGE_ALLOC ||
                alloc_table_ptr_array[i] == ATPA_UNUSABLE)
                continue;

            // when order < 12, check only the bitmap and skip the table.
            // when 12 <= order < 14, manually check the table.
            if (order < 12 && !ATPA_ORDER_BMP_GET(alloc_table_ptr_array[i], order)) continue;

            table = (struct alloc_table *)(alloc_table_ptr_array[i] & ATPA_ADDR_MASK);

            for (ssize_t j = table_search_start; j >= 0; j -= table_align_jump) {
                int allocatable = 1;

                if (table->entries[j].bitmap != ALLOCENT_BMP_FREE) continue;

                for (size_t k = 0; k < table_entries_needed; k++) {
                    if (table->entries[j + k].bitmap != ALLOCENT_BMP_FREE) {
                        allocatable = 0;
                        break;
                    }
                }

                if (!allocatable) continue;
                allocated_pfn = i * ALLOC_TABLE_COVERAGE_PAGES + j * ALLOCENT_COVERAGE_PAGES;
                LOG_TRACE("found (table: %zd, entry: %zd)\n", i, j);

                // mark entire entry as allocated
                for (size_t k = 0; k < table_entries_needed; k++) {
                    table->entries[j + k].bitmap = ALLOCENT_BMP_FULL_ALLOC;
                }

                // allocate and fill metadata directory & metadata
                status = create_metadata(allocated_pfn, NULL, order);
                if (!CHECK_SUCCESS(status)) return status;

                LOG_TRACE(
                    "filling allocation table entry %zd-%zd\n",
                    j,
                    j + table_entries_needed - 1
                );
                *pfn = allocated_pfn;
                free_frames -= (1ULL << order);
                return STATUS_SUCCESS;
            }
        }
    }

    // 3. 0 <= order < 5 is a small allocation. Use extended entries or hierarchial bitmap.
    for (ssize_t i = atpa_search_start; i >= 0; i -= atpa_align_jump) {
        if (alloc_table_ptr_array[i] == ATPA_FREE) {
            int index;

            status = get_or_create_table(i, &table);
            if (!CHECK_SUCCESS(status)) return status;

            index = find_free_frame_idx_bitmap_entry(
                table->entries[table_search_start].bitmap,
                order,
                align_order
            );
            if (index < 0) {
                St_Panic(STATUS_UNEXPECTED_RESULT, "how did you do that?");
            }

            LOG_TRACE("found (table: %zd, entry: %zd, index: %d)\n", i, table_search_start, index);

            allocate_from_bitmap_entry(&table->entries[table_search_start].bitmap, index, order);

            allocated_pfn = i * ALLOC_TABLE_COVERAGE_PAGES +
                table_search_start * ALLOCENT_COVERAGE_PAGES + index;

            // allocate and fill metadata directory & metadata table & metadata
            status = create_metadata(allocated_pfn, NULL, order);
            if (!CHECK_SUCCESS(status)) return status;

            *pfn = allocated_pfn;
            free_frames -= (1ULL << order);
            return STATUS_SUCCESS;
        }

        if (alloc_table_ptr_array[i] == ATPA_HUGE_ALLOC ||
            alloc_table_ptr_array[i] == ATPA_UNUSABLE)
            continue;

        // when order < 12, check only the bitmap and skip the table.
        // when 12 <= order < 14, manually check the table.
        if (order < 12 && !ATPA_ORDER_BMP_GET(alloc_table_ptr_array[i], order)) continue;

        table = (struct alloc_table *)(alloc_table_ptr_array[i] & ATPA_ADDR_MASK);

        for (ssize_t j = table_search_start; j >= 0; j -= table_align_jump) {
            struct extended_entry *extentry;
            int index = -1;
            size_t extentry_slots_needed = 1ULL << order;

            if (table->entries[j].bitmap & ALLOCENT_EXT_FLAG) {
                ssize_t extentry_search_start;
                size_t extentry_align_jump;

                if (table->entries[j].bitmap == ALLOCENT_EXT_UNUSABLE) continue;

                extentry = ALLOCENT_GET_EXT_PTR(table->entries[j].ptr);
                extentry_search_start = ARRAY_SIZE(extentry->state_flags);
                extentry_align_jump = align_order > 12 ? (1ULL << (align_order - 12)) : 1;
                extentry_search_start =
                    ((extentry_search_start / extentry_align_jump) - 1) * extentry_align_jump;

                for (ssize_t k = extentry_search_start; k >= 0; k -= extentry_align_jump) {
                    int allocatable = 1;

                    if (extentry->state_flags[k] != EE_FREE) continue;

                    for (size_t l = 0; l < extentry_slots_needed; l++) {
                        if (extentry->state_flags[k + l] != EE_FREE) {
                            allocatable = 0;
                            break;
                        }
                    }
                    if (!allocatable) continue;

                    index = k;
                    break;
                }

                if (index < 0) continue;

                LOG_TRACE("found (table: %zd, entry: %zd, index: %d) (extended)\n", i, j, index);
                LOG_TRACE(
                    "filling extended entry slot %d-%zd\n",
                    index,
                    index + extentry_slots_needed - 1
                );

                for (size_t k = 0; k < extentry_slots_needed; k++) {
                    extentry->state_flags[index + k] = EE_USED;
                }
            } else {
                index =
                    find_free_frame_idx_bitmap_entry(table->entries[j].bitmap, order, align_order);
                if (index < 0) continue;

                LOG_TRACE("found (table: %zd, entry: %zd, index: %d) (bitmap)\n", i, j, index);
                LOG_TRACE("filling bitmap %d-%d\n", index, index + (1 << order) - 1);

                allocate_from_bitmap_entry(&table->entries[j].bitmap, index, order);
            }

            allocated_pfn = i * ALLOC_TABLE_COVERAGE_PAGES + j * ALLOCENT_COVERAGE_PAGES + index;

            // allocate and fill metadata directory & metadata table & metadata
            status = create_metadata(allocated_pfn, NULL, order);
            if (!CHECK_SUCCESS(status)) return status;

            *pfn = allocated_pfn;
            free_frames -= (1ULL << order);
            return STATUS_SUCCESS;
        }
    }

    return STATUS_INVALID_VALUE;
}

StStatus StPmm_AcquireContiguousFrame(St_PhysFrame pfn __in)
{
    StStatus status;
    struct metadata *metadata;
    uint32_t prev_refcount;

    status = get_metadata(pfn, &metadata);
    if (!CHECK_SUCCESS(status)) return status;

    prev_refcount = atomic_fetch_add_explicit(&metadata->refcount, 1, memory_order_relaxed);

    LOG_TRACE("refcount: %" PRId32 " -> %" PRId32 "\n", prev_refcount, prev_refcount + 1);

    return STATUS_SUCCESS;
}

void StPmm_FreeContiguousFrame(St_PhysFrame pfn __in)
{
    StStatus status;
    struct metadata *metadata;
    uint32_t prev_refcount;

    status = get_metadata(pfn, &metadata);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(STATUS_CONFLICTING_STATE, "double free");
    }

    prev_refcount = atomic_fetch_sub_explicit(&metadata->refcount, 1, memory_order_relaxed);

    LOG_TRACE("refcount: %" PRId32 " -> %" PRId32 "\n", prev_refcount, prev_refcount - 1);

    if (prev_refcount > 1) return;
    if (prev_refcount == 0) {
        St_Panic(STATUS_CONFLICTING_STATE, "double free");
    }

    do_free_contiguous_frame(pfn, metadata->public.order);
}

StStatus StPmm_GetAllocMetadata(St_PhysFrame pfn __in, struct StPmm_AllocationMetadata **meta __out)
{
    StStatus status;
    struct metadata *metadata;

    status = get_metadata(pfn, &metadata);
    if (!CHECK_SUCCESS(status)) return status;

    *meta = (struct StPmm_AllocationMetadata *)metadata;

    return STATUS_SUCCESS;
}

StStatus StPmm_LockAndGetAllocMetadata(
    St_PhysFrame pfn, struct StPmm_AllocationMetadata **meta __out
)
{
    StStatus status;
    struct metadata *metadata;
    uint32_t expected = 0;

    status = get_metadata(pfn, &metadata);
    if (!CHECK_SUCCESS(status)) return status;

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

    *meta = (struct StPmm_AllocationMetadata *)metadata;

    return STATUS_SUCCESS;
}

StStatus StPmm_UnlockAllocMetadata(struct StPmm_AllocationMetadata *meta __in)
{
    struct metadata *metadata = (struct metadata *)meta;

    uint32_t expected = 1;
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
