/*
 * Kernel User-Space Stress Test
 * 목표: CPU 연산, 메모리 접근, libc 로직 검증
 */

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#if defined(__x86_64__) || defined(__i386__)
#    include <immintrin.h>
#endif

/* [검증 1] .data 및 .bss 섹션 로딩 테스트 */
/* Loader가 이 값들을 제대로 초기화하지 않으면 테스트가 실패합니다. */
int g_initialized_var = 0xDEADBEEF;  // .data section
int g_uninitialized_var;             // .bss section (should be 0)
const char *g_argv0 = NULL;

#if defined(__x86_64__) || defined(__i386__)
static void cpuid_leaf(
    uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx
)
{
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf), "c"(subleaf));
}

static uint64_t xgetbv0(void)
{
    uint32_t eax, edx;

    __asm__ volatile(".byte 0x0f, 0x01, 0xd0" : "=a"(eax), "=d"(edx) : "c"(0));
    return ((uint64_t)edx << 32) | eax;
}

static int cpu_has_avx(void)
{
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    uint64_t xcr0;

    cpuid_leaf(1, 0, &eax, &ebx, &ecx, &edx);
    if (!(ecx & (1u << 27)) || !(ecx & (1u << 28))) {
        return 0;
    }

    xcr0 = xgetbv0();
    return (xcr0 & 0x6) == 0x6;
}

#    if defined(__GNUC__) || defined(__clang__)
__attribute__((target("avx"))) static void avx_state_step(
    double s0[4], double s1[4], double s2[4], int steps
)
{
    __m256d v0 = _mm256_loadu_pd(s0);
    __m256d v1 = _mm256_loadu_pd(s1);
    __m256d v2 = _mm256_loadu_pd(s2);
    const __m256d inc = _mm256_set1_pd(1e-12);
    const __m256d scale = _mm256_set1_pd(0.99999999991);
    int i;

    for (i = 0; i < steps; i++) {
        v0 = _mm256_add_pd(v0, v1);
        v1 = _mm256_add_pd(v1, v2);
        v1 = _mm256_mul_pd(v1, scale);
        v2 = _mm256_add_pd(v2, inc);
        v0 = _mm256_sub_pd(v0, v2);
    }

    _mm256_storeu_pd(s0, v0);
    _mm256_storeu_pd(s1, v1);
    _mm256_storeu_pd(s2, v2);
}
#    endif
#endif

/* [검증 2] 부동 소수점(FPU) 컨텍스트 테스트 */
/* 커널이 인터럽트 처리 시 FPU 레지스터를 저장/복구하지 않으면 값이 깨집니다. */
void test_fpu_operations(void)
{
    printf("[Test] FPU Operations... ");

    volatile double a = 123.456;
    volatile double b = 789.012;
    volatile double result = 0.0;

    // 반복 연산으로 레지스터 체류 시간 늘림 (컨텍스트 스위칭 유도)
    for (int i = 0; i < 1000; i++) {
        result += (a * b) / (a + 1.0);
        a = a + 0.001;
    }

    // 예상 범위 내인지 확인 (정확한 값은 부동소수점 오차가 있을 수 있음)
    if (result > 0.0) {
        printf("PASS (Result: %f)\n", result);
    } else {
        printf("FAIL (Result: %f)\n", result);
    }
}

/* [검증 3] 스택 깊이 및 정렬(Alignment) 테스트 */
/* 재귀 호출을 통해 스택이 충분히 확보되었는지, Overflow 처리가 되는지 간접 확인 */
uint64_t fibonacci(int n)
{
    if (n <= 1) return n;

    // 큰 배열을 선언하여 스택 프레임을 강제로 늘림
    volatile char padding[64];
    padding[0] = n;  // 최적화 방지

    return fibonacci(n - 1) + fibonacci(n - 2);
}

void test_stack_recursion(void)
{
    printf("[Test] Stack Recursion (Fibonacci)... ");
    int n = 20;
    uint64_t result = fibonacci(n);

    if (result == 6765) {
        printf("PASS (Fib(%d) = %lu)\n", n, result);
    } else {
        printf("FAIL (Fib(%d) = %lu, Expected: 6765)\n", n, result);
    }
}

/* [검증 3-2] 유저 스택 자동 확장 테스트 */
/* 페이지 단위 alloca로 guard page를 순서대로 밟아 demand stack growth를 검증합니다. */
volatile uintptr_t g_stack_probe_anchor = 0;
volatile size_t g_stack_probe_alloc_size = 4096;

__attribute__((noinline)) unsigned long long stack_growth_probe_pages(size_t pages)
{
    unsigned long long checksum = 0;

    for (size_t i = 0; i < pages; i++) {
        size_t alloc_size = g_stack_probe_alloc_size;
        volatile unsigned char *page = (volatile unsigned char *)__builtin_alloca(alloc_size);
        unsigned char head = (unsigned char)(0x31u + (unsigned int)(i * 17u));
        unsigned char tail = (unsigned char)(0xA7u ^ (unsigned int)i);

        page[0] = head;
        page[alloc_size - 1] = tail;
        checksum += (unsigned long long)page[0];
        checksum += (unsigned long long)page[alloc_size - 1] << 8;
        checksum += (unsigned long long)((uintptr_t)page & 0xFFFu);
        g_stack_probe_anchor = (uintptr_t)page;
    }

    return checksum;
}

void test_stack_growth(void)
{
    enum {
        STACK_GROWTH_PROBE_PAGES = 32,
    };

    printf("[Test] Stack Growth Guard Pages... ");

    unsigned long long checksum = stack_growth_probe_pages(STACK_GROWTH_PROBE_PAGES);
    if (checksum == 0 || g_stack_probe_anchor == 0) {
        printf("FAIL (checksum=%llu anchor=%p)\n", checksum, (void *)g_stack_probe_anchor);
        return;
    }

    printf("PASS (%d pages, checksum=0x%llX)\n", STACK_GROWTH_PROBE_PAGES, checksum);
}

/* [검증 4] Libc 문자열 조작 및 메모리 함수 (Pure Logic) */
/* sprintf는 시스템 콜 없이 버퍼에 포맷팅만 수행하므로 매우 좋은 테스트 대상입니다. */
void test_string_manipulation(void)
{
    printf("[Test] String & Memory Ops... ");

    char buffer[100];
    const char *expect = "Integer: 1234, Hex: 0xFE, Float: 3.14";

    // memset, sprintf 테스트
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "Integer: %d, Hex: 0x%X, Float: %.2f", 1234, 254, 3.14159);

    // strcmp, memcmp 테스트
    if (strcmp(buffer, expect) == 0) {
        printf("PASS\n");
    } else {
        printf("FAIL\nExpected: '%s'\nActual:   '%s'\n", expect, buffer);
    }
}

/* [검증 5] 복잡한 알고리즘 (Sorting) */
/* qsort는 함수 포인터(Callback)와 메모리 접근을 복합적으로 테스트합니다. */
int compare_ints(const void *a, const void *b)
{
    int arg1 = *(const int *)a;
    int arg2 = *(const int *)b;
    return (arg1 > arg2) - (arg1 < arg2);
}

void test_sorting(void)
{
    printf("[Test] qsort & Function Pointers... ");

    int values[] = {88, 56, 100, 2, 25};
    int sorted[] = {2, 25, 56, 88, 100};

    qsort(values, 5, sizeof(int), compare_ints);

    if (memcmp(values, sorted, sizeof(values)) == 0) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
    }
}

/* [검증 7] Heap 할당/해제 테스트 */
void test_malloc_free(void)
{
    printf("[Test] malloc/free... ");

    enum {
        BLOCK_COUNT = 256,
        HOLD_ROUNDS = 8192,
    };
    void *blocks[BLOCK_COUNT];
    size_t sizes[BLOCK_COUNT];
    size_t total_bytes = 0;
    unsigned long long checksum = 0;
    int i;

    memset(blocks, 0, sizeof(blocks));

    for (i = 0; i < BLOCK_COUNT; i++) {
        sizes[i] = 8192 + (size_t)(i * 521 % 32768);
        total_bytes += sizes[i];
        blocks[i] = malloc(sizes[i]);
        if (!blocks[i]) {
            printf("FAIL (malloc failed at block %d, size=%lu)\n", i, (unsigned long)sizes[i]);
            goto has_error;
        }
        memset(blocks[i], (unsigned char)(0xA5 ^ i), sizes[i]);
    }

    for (i = 0; i < BLOCK_COUNT; i++) {
        unsigned char *ptr = (unsigned char *)blocks[i];
        unsigned char expected = (unsigned char)(0xA5 ^ i);

        if (ptr[0] != expected || ptr[sizes[i] - 1] != expected) {
            printf("FAIL (memory corrupted at block %d)\n", i);
            goto has_error;
        }
    }

    for (int round = 0; round < HOLD_ROUNDS; round++) {
        for (i = 0; i < BLOCK_COUNT; i++) {
            unsigned char *ptr = (unsigned char *)blocks[i];
            size_t len = sizes[i];
            unsigned char x = (unsigned char)(round + i);

            for (size_t off = 0; off < len; off += 4096) {
                checksum += ptr[off];
                ptr[off] ^= x;
            }
            checksum += ptr[len - 1];
            ptr[len - 1] ^= (unsigned char)(x + 17);
        }
    }

    if (checksum == 0) {
        printf("FAIL (checksum stayed zero)\n");
        goto has_error;
    }

    printf("PASS (held %lu KB)\n", (unsigned long)(total_bytes / 1024));

    for (i = 0; i < BLOCK_COUNT; i++) {
        if (blocks[i]) {
            free(blocks[i]);
            blocks[i] = NULL;
        }
    }
    return;

has_error:
    for (i = 0; i < BLOCK_COUNT; i++) {
        if (blocks[i]) {
            free(blocks[i]);
            blocks[i] = NULL;
        }
    }
}

/* [검증 8] Status -> errno 매핑 확인 */
void test_status_errno_mapping(void)
{
    printf("[Test] Status -> errno Mapping... ");

    int fd;
    char ch = 0;

    errno = 0;
    fd = open("/__folisdk_abi_test_no_such_file__", O_RDONLY);
    if (fd != -1 || errno != ENOENT) {
        if (fd >= 0) close(fd);
        printf("FAIL (open errno=%d, expected=%d)\n", errno, ENOENT);
        return;
    }

    errno = 0;
    if (read(-1, &ch, 1) != -1 || errno != EBADF) {
        printf("FAIL (read errno=%d, expected=%d)\n", errno, EBADF);
        return;
    }

    errno = 0;
    if (lseek(-1, 0, SEEK_SET) != (off_t)-1 || errno != EBADF) {
        printf("FAIL (lseek errno=%d, expected=%d)\n", errno, EBADF);
        return;
    }

    printf("PASS\n");
}

/* [검증 9] FD/Handle 생명주기 (dup/dup2/close) */
void test_fd_lifecycle(void)
{
    printf("[Test] FD Lifecycle (dup/dup2/close)... ");

    int fd, dupfd = -1, dup2fd = -1;
    int closed_fd = -1;
    char ch = 0;
    off_t moved, shared;

    if (!g_argv0 || !g_argv0[0]) {
        printf("SKIP (argv[0] unavailable)\n");
        return;
    }

    fd = open(g_argv0, O_RDONLY);
    if (fd < 0) {
        printf("SKIP (open argv[0] failed: errno=%d)\n", errno);
        return;
    }

    dupfd = dup(fd);
    if (dupfd < 0) {
        printf("FAIL (dup errno=%d)\n", errno);
        close(fd);
        return;
    }

    moved = lseek(fd, 8, SEEK_SET);
    shared = lseek(dupfd, 0, SEEK_CUR);
    if (moved < 0 || shared < 0 || shared != moved) {
        printf(
            "FAIL (shared offset mismatch: moved=%lld shared=%lld)\n",
            (long long)moved,
            (long long)shared
        );
        close(dupfd);
        close(fd);
        return;
    }

    dup2fd = dup2(fd, dupfd + 10);
    if (dup2fd != dupfd + 10) {
        printf("FAIL (dup2 errno=%d)\n", errno);
        close(dupfd);
        close(fd);
        if (dup2fd >= 0) close(dup2fd);
        return;
    }

    closed_fd = fd;
    close(fd);
    fd = -1;

    errno = 0;
    if (read(dupfd, &ch, 1) < 0 && errno == EBADF) {
        printf("FAIL (dup fd invalid after closing original)\n");
        close(dup2fd);
        close(dupfd);
        return;
    }

    close(dup2fd);
    close(dupfd);

    errno = 0;
    if (read(closed_fd, &ch, 1) != -1 || errno != EBADF) {
        printf("FAIL (closed fd not rejected, errno=%d)\n", errno);
        return;
    }

    printf("PASS\n");
}

/* [검증 10] readv/writev 인자 마샬링 */
void test_readv_writev_abi(void)
{
    printf("[Test] readv/writev ABI... ");

    int fd;
    char a[16], b[16];
    struct iovec iov[3];
    ssize_t n;

    if (!g_argv0 || !g_argv0[0]) {
        printf("SKIP (argv[0] unavailable)\n");
        return;
    }

    fd = open(g_argv0, O_RDONLY);
    if (fd < 0) {
        printf("SKIP (open argv[0] failed: errno=%d)\n", errno);
        return;
    }

    memset(a, 0, sizeof(a));
    memset(b, 0, sizeof(b));

    iov[0].iov_base = a;
    iov[0].iov_len = sizeof(a);
    iov[1].iov_base = b;
    iov[1].iov_len = sizeof(b);
    iov[2].iov_base = NULL;
    iov[2].iov_len = 0;

    n = readv(fd, iov, 3);
    if (n <= 0) {
        printf("FAIL (readv n=%ld errno=%d)\n", (long)n, errno);
        close(fd);
        return;
    }

    errno = 0;
    if (writev(fd, iov, 2) != -1 || errno != EBADF) {
        printf("FAIL (writev errno=%d, expected=%d)\n", errno, EBADF);
        close(fd);
        return;
    }

    close(fd);
    printf("PASS\n");
}

/* [검증 11] 64-bit 오프셋 ABI */
void test_lseek_64bit_boundary(void)
{
    printf("[Test] lseek 64-bit Offset ABI... ");

    int fd;
    off_t end, big, pos;

    if (!g_argv0 || !g_argv0[0]) {
        printf("SKIP (argv[0] unavailable)\n");
        return;
    }

    fd = open(g_argv0, O_RDONLY);
    if (fd < 0) {
        printf("SKIP (open argv[0] failed: errno=%d)\n", errno);
        return;
    }

    end = lseek(fd, 0, SEEK_END);
    if (end < 0) {
        printf("FAIL (seek end errno=%d)\n", errno);
        close(fd);
        return;
    }

    big = ((off_t)1 << 33) + 7;
    pos = lseek(fd, big, SEEK_SET);
    if (pos != big) {
        printf(
            "FAIL (64-bit seek mismatch: got=%lld expected=%lld errno=%d)\n",
            (long long)pos,
            (long long)big,
            errno
        );
        close(fd);
        return;
    }

    errno = 0;
    if (lseek(fd, (off_t)-1, SEEK_SET) != (off_t)-1 || errno != EINVAL) {
        printf("FAIL (negative seek errno=%d, expected=%d)\n", errno, EINVAL);
        close(fd);
        return;
    }

    close(fd);
    printf("PASS\n");
}

/* [검증 12] fstat 구조체 ABI */
void test_fstat_abi(void)
{
    printf("[Test] fstat Struct ABI... ");

    int fd;
    struct stat st;
    off_t end;

    if (!g_argv0 || !g_argv0[0]) {
        printf("SKIP (argv[0] unavailable)\n");
        return;
    }

    fd = open(g_argv0, O_RDONLY);
    if (fd < 0) {
        printf("SKIP (open argv[0] failed: errno=%d)\n", errno);
        return;
    }

    memset(&st, 0, sizeof(st));
    if (fstat(fd, &st) != 0) {
        printf("FAIL (fstat errno=%d)\n", errno);
        close(fd);
        return;
    }

    end = lseek(fd, 0, SEEK_END);
    if (end < 0) {
        printf("FAIL (seek end errno=%d)\n", errno);
        close(fd);
        return;
    }

    if (st.st_mode == 0 || st.st_nlink == 0 || st.st_size != end) {
        printf(
            "FAIL (mode=%u nlink=%lu size=%lld end=%lld)\n",
            (unsigned)st.st_mode,
            (unsigned long)st.st_nlink,
            (long long)st.st_size,
            (long long)end
        );
        close(fd);
        return;
    }

    close(fd);
    printf("PASS\n");
}

/* [검증 13] O_NONBLOCK 동작 */
void test_nonblocking_behavior(void)
{
    printf("[Test] Non-blocking FD Behavior... ");

    int flags, verify;
    char ch = 0;
    ssize_t n;

    flags = fcntl(STDIN_FILENO, F_GETFL);
    if (flags < 0) {
        printf("SKIP (F_GETFL errno=%d)\n", errno);
        return;
    }

    if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) < 0) {
        printf("SKIP (F_SETFL errno=%d)\n", errno);
        return;
    }

    verify = fcntl(STDIN_FILENO, F_GETFL);
    if (verify < 0 || !(verify & O_NONBLOCK)) {
        (void)fcntl(STDIN_FILENO, F_SETFL, flags);
        printf("FAIL (O_NONBLOCK not set)\n");
        return;
    }

    errno = 0;
    n = read(STDIN_FILENO, &ch, 1);
    if (n == -1 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
        (void)fcntl(STDIN_FILENO, F_SETFL, flags);
        printf("FAIL (read errno=%d)\n", errno);
        return;
    }

    (void)fcntl(STDIN_FILENO, F_SETFL, flags);
    printf("PASS\n");
}

/* [검증 6] BSS 및 Data 섹션 초기화 확인 */
void test_data_sections(void)
{
    printf("[Test] .data & .bss Sections... ");

    int fail = 0;
    if (g_initialized_var != 0xDEADBEEF) {
        printf("FAIL (.data corrupted: 0x%X) ", g_initialized_var);
        fail = 1;
    }

    if (g_uninitialized_var != 0) {
        printf("FAIL (.bss not zeroed: %d) ", g_uninitialized_var);
        fail = 1;
    }

    if (!fail) printf("PASS\n");
}

/* * [검증 목표]
 * 커널이 스택 최상단에 인자를 규격(System V ABI 등)에 맞게 밀어 넣었는가?
 * 잘못되면 argv[0]이 NULL이거나 쓰레기 값이 나옵니다.
 */
void test_arguments(int argc, char *argv[], char *envp[])
{
    printf("[Test] Arguments & Environment...\n");

    // Argc Check
    printf("  argc: %d\n", argc);

    // Argv Check
    for (int i = 0; i < argc; i++) {
        if (argv[i] == NULL) {
            printf("FAIL: argv[%d] is NULL\n", i);
            return;
        }
        printf("  argv[%d]: %s\n", i, argv[i]);
    }

    // Envp Check (환경변수가 하나라도 있는지 확인)
    if (envp && envp[0]) {
        printf("  envp[0]: %s (Environment seems OK)\n", envp[0]);
    } else {
        printf("WARN: No environment variables found.\n");
    }
}

/* * [검증 목표]
 * 커널이 FS/GS 세그먼트 레지스터를 설정했는지, TLS 영역이 매핑되었는지 확인
 */
__thread int tls_var = 12345;  // TLS 변수 선언

void test_tls(void)
{
    printf("[Test] Thread Local Storage (TLS)... ");

    // TLS 변수 값을 변경하고 읽어봄
    tls_var += 1;

    if (tls_var == 12346) {
        printf("PASS (TLS Read/Write OK)\n");
    } else {
        printf("FAIL (Value mismatch)\n");
    }
}

/* * [검증 목표]
 * LOCK prefix 명령어(x86) 또는 LL/SC(ARM) 동작 여부
 */
void test_atomics(void)
{
    printf("[Test] Atomic Operations... ");

    int val = 10;
    // GCC 내장 함수 사용 (Assembly: lock xadd 등 생성)
    int old = __sync_fetch_and_add(&val, 5);  // val을 15로 만들고, 10을 반환

    if (old == 10 && val == 15) {
        printf("PASS\n");
    } else {
        printf("FAIL (Old: %d, New: %d)\n", old, val);
    }
}

/* * [검증 목표]
 * _start -> __libc_csu_init -> constructors -> main 흐름 검증
 */
int g_constructor_check = 0;

void __attribute__((constructor)) my_init(void)
{
    g_constructor_check = 1;
    // 주의: 여기서 printf 사용 시 초기화 순서에 따라 위험할 수 있음 (write syscall 직접 호출 권장)
}

void test_constructors(void)
{
    printf("[Test] Constructors (.init_array)... ");
    if (g_constructor_check == 1) {
        printf("PASS\n");
    } else {
        printf("FAIL (Constructor did not run)\n");
    }
}

/*
 * Register Thrashing Test
 * 목표: 메모리 사용 최소화, GPR 및 FPU/SIMD 레지스터 부하 최대화
 * 특징: 행렬(배열) 없음. 오직 지역 변수와 레지스터 연산만 수행.
 */
void test_register_thrashing(void)
{
    printf("[Test] CPU Register Thrashing (GPR + FPU/SIMD)...\n");
    printf("       Progress: ");

    // [설정] 반복 횟수 (너무 빠르면 숫자를 늘리세요)
    // 1억 번 반복. 에뮬레이터에서는 시간이 걸릴 수 있습니다.
    const long long ITERATIONS = 10000000;
    const uint64_t EXPECTED_GPR_CHECKSUM = 0x07BC885A39F5A864ULL;
    const char *EXPECTED_FPU_CHECKSUM = "61.264390348284692";
    const uint64_t EXPECTED_AVX_CHECKSUM = 0x011FDE84EB5CBC8AULL;

    /* * [GPR 부하용 변수]
     * 컴파일러가 r8~r15 등 범용 레지스터에 할당하도록 유도
     */
    uint64_t g1 = 0x123456789ABCDEF0;
    uint64_t g2 = 0x0FEDCBA987654321;
    uint64_t g3 = 0xA5A5A5A55A5A5A5A;
    uint64_t g4 = 0xFF00FF0000FF00FF;

    /* * [FPU/SIMD 부하용 변수]
     * 컴파일러가 xmm0~xmm15 등 벡터 레지스터에 할당하도록 유도
     * double 타입을 사용하여 정밀도 유지 필요
     */
    double f1 = 1.0000001;
    double f2 = 0.9999999;
    double f3 = 3.1415926;
    double f4 = 2.7182818;
#if defined(__x86_64__) || defined(__i386__)
#    if defined(__GNUC__) || defined(__clang__)
    int avx_enabled = cpu_has_avx();
    double avx0[4] = {1.0, 2.0, 3.0, 4.0};
    double avx1[4] = {0.9999991, 1.0000013, 0.9999987, 1.0000007};
    double avx2[4] = {1e-7, 2e-7, 3e-7, 4e-7};
    const int avx_chunk_steps = 16;
    const long long avx_stride = 64;
    uint64_t avx_checksum = 0;
#    endif
#endif

    /*
     * [메인 루프]
     * 메모리 접근 없이 레지스터끼리만 계속 값을 주고받습니다.
     * 의존성(Dependency Chain)을 복잡하게 만들어 파이프라인을 채웁니다.
     */
    for (long long i = 0; i < ITERATIONS; i++) {
        // 1. GPR 연산 (Bitwise + Arithmetic)
        // Xorshift 변형 알고리즘으로 레지스터 값 계속 변화
        g1 ^= (g2 << 13);
        g2 ^= (g3 >> 7);
        g3 += g4;
        g4 = (g4 << 3) | (g4 >> 61);  // Rotate Left
        g1 += i;                      // 루프 카운터 섞기

        // 2. FPU 연산 (SIMD 활용 유도)
        // 곱셈과 덧셈을 섞어 FMA(Fused Multiply-Add) 유도 가능
        f1 = f1 * f2 + 0.0000000000001;
        f2 = f2 + 0.0000000000001;
        f3 = f3 / 1.00000001 + f4 * 0.00000001;
        f4 = f4 - 0.0000000000001;

        // 3. Cross-Domain Interaction (가끔씩 섞기)
        // 정수 레지스터와 부동소수점 레지스터 간 이동 테스트 (cvtsi2sd 등 명령어)
        if ((i & 0xFFFFF) == 0) {
            f1 += (double)(g1 & 0xFF) * 0.0000001;
            g4 ^= (uint64_t)f2;
            printf(".");  // 생존 신고 (너무 자주는 안 함)
        }

#if defined(__x86_64__) || defined(__i386__)
#    if defined(__GNUC__) || defined(__clang__)
        if (avx_enabled && (i % avx_stride) == 0) {
            avx0[0] += (double)(g1 & 0x3F) * 1e-13;
            avx1[1] += (double)(g2 & 0x3F) * 1e-13;
            avx2[2] += (double)(g3 & 0x3F) * 1e-13;
            avx_state_step(avx0, avx1, avx2, avx_chunk_steps);
        }
#    endif
#endif
    }

#if defined(__x86_64__) || defined(__i386__)
#    if defined(__GNUC__) || defined(__clang__)
    if (avx_enabled) {
        for (int i = 0; i < 4; i++) {
            uint64_t bits0 = 0, bits1 = 0, bits2 = 0;

            memcpy(&bits0, &avx0[i], sizeof(bits0));
            memcpy(&bits1, &avx1[i], sizeof(bits1));
            memcpy(&bits2, &avx2[i], sizeof(bits2));
            avx_checksum ^=
                bits0 + (bits1 << 1) + (bits2 << 2) + (0x9E3779B185EBCA87ULL * (uint64_t)(i + 1));
        }
    }
#    endif
#endif

    printf(" Done.\n");

    /* * [결과 검증]
     * 문맥 전환 중 레지스터가 하나라도 깨졌다면 이 최종 결과값들이
     * 예상된 값(Deterministic)과 달라지게 됩니다.
     */
    uint64_t gpr_checksum = g1 ^ g2 ^ g3 ^ g4;
    char fpu_checksum_text[32];
    int gpr_pass = gpr_checksum == EXPECTED_GPR_CHECKSUM;
    int fpu_pass;
    int avx_pass = 1;

    snprintf(fpu_checksum_text, sizeof(fpu_checksum_text), "%.15f", f1 + f2 + f3 + f4);
    fpu_pass = strcmp(fpu_checksum_text, EXPECTED_FPU_CHECKSUM) == 0;

    printf(
        "       GPR Checksum: 0x%llX (expected 0x%llX) %s\n",
        (unsigned long long)gpr_checksum,
        (unsigned long long)EXPECTED_GPR_CHECKSUM,
        gpr_pass ? "PASS" : "FAIL"
    );
    printf(
        "       FPU Checksum: %s (expected %s) %s\n",
        fpu_checksum_text,
        EXPECTED_FPU_CHECKSUM,
        fpu_pass ? "PASS" : "FAIL"
    );
#if defined(__x86_64__) || defined(__i386__)
#    if defined(__GNUC__) || defined(__clang__)
    if (avx_enabled) {
        avx_pass = avx_checksum == EXPECTED_AVX_CHECKSUM;
        printf(
            "       AVX Checksum: 0x%llX (expected 0x%llX) %s\n",
            (unsigned long long)avx_checksum,
            (unsigned long long)EXPECTED_AVX_CHECKSUM,
            avx_pass ? "PASS" : "FAIL"
        );
    } else {
        printf("       AVX Checksum: SKIP (AVX/XSAVE unsupported)\n");
    }
#    else
    printf("       AVX Checksum: SKIP (compiler unsupported)\n");
#    endif
#else
    printf("       AVX Checksum: SKIP (non-x86 arch)\n");
#endif

    printf(
        "       Register Thrashing Result: %s\n",
        (gpr_pass && fpu_pass && avx_pass) ? "PASS" : "FAIL"
    );
}

int main(int argc, char *argv[], char *envp[])
{
    printf("=== User-Space Application Test ===\n");
    if (argc > 0 && argv && argv[0]) {
        g_argv0 = argv[0];
    }

    test_arguments(argc, argv, envp);
    test_constructors();
    test_tls();
    test_atomics();
    test_data_sections();
    test_status_errno_mapping();
    test_fd_lifecycle();
    test_readv_writev_abi();
    test_lseek_64bit_boundary();
    test_fstat_abi();
    test_nonblocking_behavior();
    test_stack_recursion();
    test_stack_growth();
    test_fpu_operations();
    test_string_manipulation();
    test_sorting();
    test_malloc_free();
    test_register_thrashing();

    printf("=== All Tests Completed ===\n");
    return 0;
}
