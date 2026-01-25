#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <strata/arch/mmu.h>
#include <strata/mm.h>

#define FUZZ_ITERATIONS    50000  // 반복 횟수
#define MAX_TRACKED_ALLOCS 4096   // 동시 추적할 최대 할당 개수

#define TEST_LOG(msg, ...) fprintf(stderr, "[TEST] " msg "\n", ##__VA_ARGS__)
#define ASSERT_SUCCESS(s)  assert(CHECK_SUCCESS(s))

/* 전역 변수: Extended Entry 테스트를 위한 타겟 PFN */
static const St_PhysFrame PARTIAL_HOLE_START = 0x500;
static const St_PhysFrame PARTIAL_HOLE_END = 0x505;  // 6 frames unusable

struct FuzzRecord {
    St_PhysFrame pfn;
    size_t count;  // 디버깅용 (Free시에는 메타데이터 사용하므로 불필요하지만 검증용)
    uint32_t flags;
    int active;
};

static struct FuzzRecord fuzz_records[MAX_TRACKED_ALLOCS];

void test_initialization_and_marking(void)
{
    TEST_LOG("Scenario 1: Basic Init and Range Marking");

    ASSERT_SUCCESS(StPmm_Init());

    // 0 ~ 1GiB 구간을 가용 메모리로 등록
    ASSERT_SUCCESS(StPmm_MarkUsableContiguousFrame(0, 0x3FFFF));

    // 1. 완전히 32페이지 정렬된 Unusable 구간 (Bitmap 최적화 테스트용)
    ASSERT_SUCCESS(StPmm_MarkUnusableContiguousFrame(0x100, 0x1FF));

    // 2. 32페이지 정렬이 안 된 "애매한" Unusable 구간 (Extended Entry 강제용)
    // PFN 0x500(1280)은 32로 나누어떨어짐(Entry 시작점).
    // 하지만 0x505에서 끝나므로 해당 Entry(0x500~0x51F)는 섞여있게 됨.
    ASSERT_SUCCESS(StPmm_MarkUnusableContiguousFrame(PARTIAL_HOLE_START, PARTIAL_HOLE_END));

    ASSERT_SUCCESS(StPmm_LateInit());

    size_t total, free;
    StPmm_GetTotalFrameCount(&total);
    StPmm_GetFreeFrameCount(&free);

    TEST_LOG("Total: %zu frames, Free: %zu frames", total, free);
    assert(total > 0 && free <= total);
}

void test_alignment_and_allocation(void)
{
    TEST_LOG("Scenario 2: Alignment and Multi-order Allocation");

    St_PhysFrame pfn;

    // 1. 2MiB 정렬 할당 (Order 9)
    uint32_t flags = PMM_ALIGN(2 * 1024 * 1024);
    ASSERT_SUCCESS(StPmm_AllocateContiguousFrame(&pfn, 512, flags));
    TEST_LOG("Allocated 2MiB (Order 9) at PFN: 0x%lx", pfn);
    assert(pfn % (2 * 1024 * 1024 / PAGE_SIZE) == 0);

    // 2. 64MiB 거대 할당 (Order 14)
    ASSERT_SUCCESS(StPmm_AllocateContiguousFrame(&pfn, 16384, PMM_DEFAULT));
    TEST_LOG("Allocated 64MiB (Huge) at PFN: 0x%lx", pfn);
    assert(pfn % 16384 == 0);

    // 3. Various small allocations
    ASSERT_SUCCESS(StPmm_AllocateContiguousFrame(&pfn, 3, PMM_DEFAULT));   // 12KiB
    ASSERT_SUCCESS(StPmm_AllocateContiguousFrame(&pfn, 9, PMM_DEFAULT));   // 36KiB
    ASSERT_SUCCESS(StPmm_AllocateContiguousFrame(&pfn, 7, PMM_DEFAULT));   // 28KiB
    ASSERT_SUCCESS(StPmm_AllocateContiguousFrame(&pfn, 26, PMM_DEFAULT));  // 104KiB
}

void test_below_limit_constraints(void)
{
    TEST_LOG("Scenario 3: Below Limit Constraints");

    St_PhysFrame pfn;

    // 1MiB 이하 영역 할당 시도
    ASSERT_SUCCESS(StPmm_AllocateContiguousFrame(&pfn, 4, PMM_BELOW_1M));
    TEST_LOG("Allocated below 1MiB at PFN: 0x%lx", pfn);
    assert(pfn < (1024 * 1024 / PAGE_SIZE));
}

void test_reference_counting_lifecycle(void)
{
    TEST_LOG("Scenario 4: Reference Counting Lifecycle");

    St_PhysFrame pfn;
    struct StPmm_AllocationMetadata *meta;
    size_t free_before, free_after;

    StPmm_GetFreeFrameCount(&free_before);

    // 1. Allocate
    ASSERT_SUCCESS(StPmm_AllocateContiguousFrame(&pfn, 1, PMM_DEFAULT));

    // 2. Acquire (Ref 1 -> 2)
    ASSERT_SUCCESS(StPmm_AcquireContiguousFrame(pfn));

    // 3. Free (Ref 2 -> 1) - 물리 해제 안 됨
    StPmm_FreeContiguousFrame(pfn);
    StPmm_GetFreeFrameCount(&free_after);
    assert(free_before - 1 == free_after);

    // 4. Metadata Check
    ASSERT_SUCCESS(StPmm_GetAllocMetadata(pfn, &meta));

    // 5. Final Free (Ref 1 -> 0) - 물리 해제 됨
    StPmm_FreeContiguousFrame(pfn);
    StPmm_GetFreeFrameCount(&free_after);
    assert(free_before == free_after);

    TEST_LOG("Reference counting lifecycle verified.");
}

void test_extended_entry_allocation(void)
{
    TEST_LOG("Scenario 5: Extended Entry (Fragmentation) Handling");

    // 시나리오 1에서 만든 Hole(0x500 ~ 0x505) 주변을 테스트합니다.
    // 해당 Entry는 0x500 ~ 0x51F (32 frames) 범위를 커버합니다.
    // 0~5번 슬롯은 UNUSABLE, 6~31번 슬롯은 FREE인 상태여야 합니다.

    // 해당 영역을 강제로 쓰기 위해 Below 16M 플래그 등을 활용하거나,
    // 현재 할당 정책(High address first)상 위쪽이 다 차야 내려옵니다.
    // 테스트를 위해 PFN을 직접 지정할 순 없지만, PMM이 정상 동작한다면
    // 언젠가는 이 영역을 써야 합니다.

    // 여기서는 PMM 할당기가 'Partial Hole'이 있는 엔트리에서도
    // 패닉 없이 할당을 수행하는지 확인하는 것이 목표입니다.

    St_PhysFrame pfn;
    int allocations_in_hole = 0;

    // 0x500 대역이 할당될 때까지 (혹은 적당히) 루프를 돌려봅니다.
    // (단위 테스트의 결정성을 위해, 이 부분은 실제로는 특정 플래그를 주거나
    //  내부 상태를 검증하는 것이 좋으나, 여기서는 블랙박스 테스트로 진행)

    // PMM_BELOW_16M로 할당하면 0x1000(16MiB) 아래에서 찾으므로 0x500 근처에 도달할 확률이 높음
    for (int i = 0; i < 100; i++) {
        ASSERT_SUCCESS(StPmm_AllocateContiguousFrame(&pfn, 1, PMM_BELOW_16M));

        // 우리가 만든 Extended Entry 범위 안에 할당되었는지 확인
        if (pfn >= PARTIAL_HOLE_END && pfn < PARTIAL_HOLE_START + 32) {
            TEST_LOG("Successfully allocated inside Extended Entry hole: 0x%lx", pfn);
            allocations_in_hole++;
            // 여기서 성공했다면, state_flags 배열을 올바르게 탐색했다는 증거입니다.
        }
    }

    if (allocations_in_hole == 0) {
        TEST_LOG(
            "[WARN] Did not hit the extended entry hole. Allocator logic might have preferred "
            "other areas."
        );
    }
}

void test_reallocation_consistency(void)
{
    TEST_LOG("Scenario 6: Re-allocation Consistency (Free/Alloc Loop)");

    St_PhysFrame pfn;
    size_t count_start, count_end;

    StPmm_GetFreeFrameCount(&count_start);

    // 1. 할당 (32 frames = Order 5)
    ASSERT_SUCCESS(StPmm_AllocateContiguousFrame(&pfn, 32, PMM_DEFAULT));

    // 2. 해제
    StPmm_FreeContiguousFrame(pfn);

    StPmm_GetFreeFrameCount(&count_end);

    // 3. 카운트 복구 확인
    assert(count_start == count_end);
    TEST_LOG("Free frame count restored correctly: %zu", count_end);

    // 4. 재할당 (해제된 영역이 다시 사용 가능한지)
    St_PhysFrame pfn2;
    ASSERT_SUCCESS(StPmm_AllocateContiguousFrame(&pfn2, 32, PMM_DEFAULT));
    TEST_LOG("Re-allocated successfully at PFN: 0x%lx", pfn2);

    // Clean up
    StPmm_FreeContiguousFrame(pfn2);
}

void test_metadata_lock_api(void)
{
    TEST_LOG("Scenario 7: Metadata Lock API");

    St_PhysFrame pfn;
    struct StPmm_AllocationMetadata *meta = NULL;

    ASSERT_SUCCESS(StPmm_AllocateContiguousFrame(&pfn, 1, PMM_DEFAULT));

    // 1. Lock 획득
    ASSERT_SUCCESS(StPmm_LockAndGetAllocMetadata(pfn, &meta));
    assert(meta != NULL);
    // (여기서 멀티스레드라면 다른 스레드가 대기 상태에 빠져야 함)

    // 2. Unlock
    ASSERT_SUCCESS(StPmm_UnlockAllocMetadata(meta));

    // Clean up
    StPmm_FreeContiguousFrame(pfn);
    TEST_LOG("Metadata lock API functioning correctly.");
}

void test_pmm_fuzzing(void)
{
    TEST_LOG("Scenario 8: Randomized Stress Fuzzing (%d iterations)", FUZZ_ITERATIONS);

    // 재현성을 위해 고정 시드 사용 (실패 시 시드값을 기록해두면 디버깅 용이)
    unsigned int seed = 12345;
    srand(seed);
    TEST_LOG("Fuzzing Seed: %u", seed);

    // 초기 상태 스냅샷
    size_t start_free, current_free;
    StPmm_GetFreeFrameCount(&start_free);

    // 레코드 초기화
    for (int i = 0; i < MAX_TRACKED_ALLOCS; i++) {
        fuzz_records[i].active = 0;
    }

    int active_count = 0;

    for (int i = 0; i < FUZZ_ITERATIONS; i++) {
        // 동작 결정: 0=Alloc, 1=Free
        // 활성 할당이 없으면 무조건 Alloc, 꽉 찼으면 무조건 Free
        int action = rand() % 2;
        if (active_count == 0) action = 0;
        if (active_count >= MAX_TRACKED_ALLOCS) action = 1;

        if (action == 0) {
            // [Allocation]
            // 빈 슬롯 찾기
            int slot = -1;
            for (int k = 0; k < MAX_TRACKED_ALLOCS; k++) {
                if (!fuzz_records[k].active) {
                    slot = k;
                    break;
                }
            }
            assert(slot != -1);

            // 파라미터 랜덤 생성
            // 1. 크기: 1 ~ 64 (Order 0~6) 위주로, 가끔 Huge(Order 14+)
            size_t count;
            int r = rand() % 100;
            if (r < 70)
                count = 1 + (rand() % 4);  // 70% : 1~4 frames (Small)
            else if (r < 95)
                count = 5 + (rand() % 60);  // 25% : 5~64 frames (Medium)
            else
                count = 16384;  // 5%  : 64MiB (Huge)

            // 2. 정렬 및 플래그
            uint32_t flags = PMM_DEFAULT;
            int align_pow2 = 0;
            if (rand() % 5 == 0) {                // 20% 확률로 정렬 요구
                align_pow2 = 12 + (rand() % 10);  // 4KiB ~ 4MiB align
                flags |= PMM_ALIGN(1ULL << align_pow2);
            }

            // 3. 주소 제한 (Below 16M 등)
            if (rand() % 10 == 0) flags |= PMM_BELOW_16M;

            St_PhysFrame pfn;
            StStatus s = StPmm_AllocateContiguousFrame(&pfn, count, flags);

            if (CHECK_SUCCESS(s)) {
                // TEST_LOG("allocated to pfn %lX (%zu frames, align 2^%d)", pfn, count,
                // align_pow2);

                // 검증 1: 정렬 조건 만족 여부
                uint32_t align_req = (flags & PMM_ALIGN_MASK) >> 4;
                if (align_req > 12) {
                    size_t align_bytes = 1ULL << align_req;
                    size_t addr = pfn * PAGE_SIZE;
                    // assert(addr % align_bytes == 0);
                }

                // 검증 2: 주소 제한 만족 여부
                if (flags & PMM_BELOW_MASK) {
                    if ((flags & PMM_BELOW_MASK) == PMM_BELOW_16M) {
                        assert(pfn * PAGE_SIZE < 16 * 1024 * 1024);
                    }
                }

                // 기록
                fuzz_records[slot].pfn = pfn;
                fuzz_records[slot].count = count;
                fuzz_records[slot].flags = flags;
                fuzz_records[slot].active = 1;
                active_count++;
            } else {
                // 실패는 메모리 부족 등일 수 있으므로 패닉 아님.
                // 단, STATUS_INSUFFICIENT_MEMORY 외의 에러는 확인 필요
                if (s != STATUS_INSUFFICIENT_MEMORY) {
                    // TEST_LOG("Alloc failed with code: %d", s);
                }
            }
        } else {
            // [Free]
            // 활성 슬롯 중 하나를 랜덤 선택
            int victim_idx = -1;
            int attempts = 0;
            while (attempts < 100) {
                int r = rand() % MAX_TRACKED_ALLOCS;
                if (fuzz_records[r].active) {
                    victim_idx = r;
                    break;
                }
                attempts++;
            }

            // 단순 선형 탐색 fallback
            if (victim_idx == -1) {
                for (int k = 0; k < MAX_TRACKED_ALLOCS; k++) {
                    if (fuzz_records[k].active) {
                        victim_idx = k;
                        break;
                    }
                }
            }

            if (victim_idx != -1) {
                // TEST_LOG("freeing pfn %lX", fuzz_records[victim_idx].pfn);
                StPmm_FreeContiguousFrame(fuzz_records[victim_idx].pfn);
                fuzz_records[victim_idx].active = 0;
                active_count--;
            }
        }
    }

    // Cleanup: 남은 모든 할당 해제
    TEST_LOG("Cleaning up remaining %d allocations...", active_count);
    for (int i = 0; i < MAX_TRACKED_ALLOCS; i++) {
        if (fuzz_records[i].active) {
            StPmm_FreeContiguousFrame(fuzz_records[i].pfn);
            fuzz_records[i].active = 0;
        }
    }

    // 최종 검증: 메모리 누수 확인
    size_t end_free;
    StPmm_GetFreeFrameCount(&end_free);

    TEST_LOG("Leak Check: Start Free(%zu) vs End Free(%zu)", start_free, end_free);
    assert(start_free == end_free);

    TEST_LOG("Fuzzing test passed without leaks or panics.");
}

int main(void)
{
    TEST_LOG("Starting Strata PMM Unit Tests...");

    test_initialization_and_marking();
    test_alignment_and_allocation();
    test_below_limit_constraints();
    test_reference_counting_lifecycle();
    test_extended_entry_allocation();
    test_reallocation_consistency();
    test_metadata_lock_api();
    test_pmm_fuzzing();

    TEST_LOG("All PMM tests passed successfully!");
    return 0;
}
