folios_build_config_set(
    CONFIG_KSTACK_PAGE_COUNT
    "4"
    "^[1-9][0-9]*$"
    "Default kernel thread stack page count"
)
folios_build_config_set(
    CONFIG_USTACK_PAGE_COUNT
    "4"
    "^[1-9][0-9]*$"
    "Default user thread stack page count"
)
folios_build_config_set(
    CONFIG_MM_POOL_MANTISSA_BITS
    "4"
    "^[1-9][0-9]*$"
    "Subpool mantissa bit count"
)
folios_build_config_set(
    CONFIG_MM_POOL_MIN_ALIGN_BITS
    "4"
    "^[1-9][0-9]*$"
    "Subpool minimum alignment bits"
)
folios_build_config_set(
    CONFIG_MM_POOL_SUBPOOL_PAGE_COUNT
    "16"
    "^[1-9][0-9]*$"
    "Subpool page count"
)
