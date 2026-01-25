#include <vellum/mm.h>

int liballoc_lock(void)
{
    return 0;
}

int liballoc_unlock(void)
{
    return 0;
}

void *liballoc_alloc(int page_count)
{
    vpn_t vpn;

    if (!CHECK_SUCCESS(mm_allocate_pages(page_count, &vpn))) {
        return NULL;
    }

    return (void *)(vpn * PAGE_SIZE);
}

int liballoc_free(void *vaddr, int page_count)
{
    mm_free_pages((uintptr_t)vaddr / PAGE_SIZE, page_count);

    return page_count;
}
