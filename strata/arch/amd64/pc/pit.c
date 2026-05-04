#include <strata/plat/pit.h>

#include <strata/arch/intrinsics/io.h>
#include <strata/arch/io.h>

#include <strata/macros.h>

#define PIT_BASE_CLK_HZ 1193182

#define PIT_REG_CH0_DATA 0x0040
#define PIT_REG_CH1_DATA 0x0041
#define PIT_REG_CH2_DATA 0x0042
#define PIT_REG_MODE_CMD 0x0043

static uint16_t read_counter0(void)
{
    uint16_t current_value;

    StIoA_Out8(PIT_REG_MODE_CMD, 0x00);
    current_value = StIoA_In8(PIT_REG_CH0_DATA);
    StIoA_Wait();
    current_value |= StIoA_In8(PIT_REG_CH0_DATA) << 8;
    StIoA_Wait();

    return current_value;
}

StStatus StPitP_Init(void)
{
    return STATUS_SUCCESS;
}

StStatus StPitP_SetPeriodic(uint64_t freq_hz __in)
{
    uint16_t pit_value;

    if (freq_hz > PIT_BASE_CLK_HZ) {
        return STATUS_INVALID_VALUE;
    }

    pit_value = (uint16_t)(PIT_BASE_CLK_HZ / freq_hz);

    StIoA_Out8(PIT_REG_MODE_CMD, 0x34);
    StIoA_Wait();
    StIoA_Out8(PIT_REG_CH0_DATA, pit_value & 0xFF);
    StIoA_Wait();
    StIoA_Out8(PIT_REG_CH0_DATA, (pit_value >> 8) & 0xFF);
    StIoA_Wait();

    return STATUS_SUCCESS;
}

StStatus StPitP_SetOneshot(uint64_t us __in)
{
    uint64_t pit_value = us * PIT_BASE_CLK_HZ / 1000000;

    if (pit_value == 0 || pit_value > UINT16_MAX) {
        return STATUS_INVALID_VALUE;
    }

    StIoA_Out8(PIT_REG_MODE_CMD, 0x30);
    StIoA_Wait();
    StIoA_Out8(PIT_REG_CH0_DATA, pit_value & 0xFF);
    StIoA_Wait();
    StIoA_Out8(PIT_REG_CH0_DATA, (pit_value >> 8) & 0xFF);
    StIoA_Wait();

    return STATUS_SUCCESS;
}

void StPitP_SetOneshotAndBusyWait(uint64_t us __in)
{
    uint64_t target_count = us * PIT_BASE_CLK_HZ / 1000000, current_target_count;
    uint16_t current_value;

    while (target_count > 0) {
        current_target_count = MIN(UINT16_MAX, target_count);

        StIoA_Out8(PIT_REG_MODE_CMD, 0x30);
        StIoA_Wait();
        StIoA_Out8(PIT_REG_CH0_DATA, current_target_count & 0xFF);
        StIoA_Wait();
        StIoA_Out8(PIT_REG_CH0_DATA, (current_target_count >> 8) & 0xFF);
        StIoA_Wait();

        do {
            current_value = read_counter0();
        } while (current_value == 0 || current_value > current_target_count);

        do {
            current_value = read_counter0();
        } while (current_value > 0);

        target_count -= current_target_count;
    }
}

void StPitP_Stop(void)
{
    StIoA_Out8(PIT_REG_MODE_CMD, 0x30);
    StIoA_Wait();
}
