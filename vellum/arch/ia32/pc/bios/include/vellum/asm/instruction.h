#ifndef __VELLUM_ASM_TEST_INSTRUCTION_H__
#define __VELLUM_ASM_TEST_INSTRUCTION_H__

#include <vellum/status.h>

extern int _pc_invlpg_undefined;
extern int _pc_rdtsc_undefined;

status_t _pc_instruction_test(void (*test_func)(void), size_t instr_size, int *is_undefined);

#endif  // __VELLUM_ASM_TEST_INSTRUCTION_H__
