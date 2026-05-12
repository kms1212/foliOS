#include <strata/plat/pic.h>

#include <stdint.h>

#include <strata/arch/intrinsics/io.h>
#include <strata/arch/io.h>

#include <strata/compiler.h>

#define MPIC_CMD  0x0020
#define MPIC_DATA 0x0021
#define SPIC_CMD  0x00A0
#define SPIC_DATA 0x00A1

static uint8_t master_int_base = 0x70;
static uint8_t slave_int_base = 0x08;

void StPicP_Remap(uint8_t master __in, uint8_t slave __in)
{
    master_int_base = master;
    slave_int_base = slave;

    StIoA_Out8(MPIC_CMD, 0x11);
    StIoA_Wait();
    StIoA_Out8(MPIC_DATA, master);
    StIoA_Wait();
    StIoA_Out8(MPIC_DATA, 0x04);
    StIoA_Wait();
    StIoA_Out8(MPIC_DATA, 0x01);
    StIoA_Wait();
    StIoA_Out8(MPIC_DATA, 0xFF);

    StIoA_Out8(SPIC_CMD, 0x11);
    StIoA_Wait();
    StIoA_Out8(SPIC_DATA, slave);
    StIoA_Wait();
    StIoA_Out8(SPIC_DATA, 0x02);
    StIoA_Wait();
    StIoA_Out8(SPIC_DATA, 0x01);
    StIoA_Wait();
    StIoA_Out8(SPIC_DATA, 0xFF);
}

void StPicP_Mask(int num __in)
{
    if ((master_int_base <= num) && (num < master_int_base + 8)) {
        StIoA_Out8(MPIC_DATA, StIoA_In8(MPIC_DATA) | (1 << (num - master_int_base)));
    } else if ((slave_int_base <= num) && (num < slave_int_base + 8)) {
        StIoA_Out8(SPIC_DATA, StIoA_In8(SPIC_DATA) | (1 << (num - slave_int_base)));
    }
}

void StPicP_Unmask(int num __in)
{
    if ((master_int_base <= num) && (num < master_int_base + 8)) {
        StIoA_Out8(MPIC_DATA, StIoA_In8(MPIC_DATA) & ~(1 << (num - master_int_base)));
    } else if ((slave_int_base <= num) && (num < slave_int_base + 8)) {
        StIoA_Out8(SPIC_DATA, StIoA_In8(SPIC_DATA) & ~(1 << (num - slave_int_base)));
    }
}

void StPicP_SendEoi(int num __in)
{
    if ((master_int_base <= num) && (num < master_int_base + 8)) {
        StIoA_Out8(MPIC_CMD, 0x20);
    } else if ((slave_int_base <= num) && (num < slave_int_base + 8)) {
        StIoA_Out8(SPIC_CMD, 0x20);
        StIoA_Out8(MPIC_CMD, 0x20);
    }
}

void StPicP_Disable(void)
{
    StIoA_Out8(MPIC_DATA, 0xFF);
    StIoA_Out8(SPIC_DATA, 0xFF);
}
