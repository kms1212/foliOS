# Strata Kernel Coding Style & Naming Convention

This document outlines the coding standards, naming conventions, and best practices for the Strata kernel and FoliOS SDK. It is derived from `strata/naming.md` and analysis of the existing codebase.

## 1. Naming Conventions

### 1.1 Functions
Functions follow **PascalCase** with specific prefixes to denote their scope and layer.

**Syntax:** `Prefix[Region][Scope]_[Action]()`

-   **Prefix**: `St` (Strata Global)
-   **Region** (Optional): Subsystem (e.g., `Vmm`, `Pmm`, `Sched`, `Irq`, `Fs`).
-   **Scope** (Optional):
    -   `A`: Architecture-specific (CPU/ISA).
    -   `P`: Platform-specific (Board/Firmware).
    -   None: Generic/High-level.
-   **Action**: Verb indicating the operation.

**Examples:**
-   `StVmm_MapPage()` (Generic VMM)
-   `StA_SwitchContext()` (Architecture: Context switch)
-   `StP_PowerOff()` (Platform: Power control)
-   `StIrqP_InitController()` (Platform implementation of IRQ subsystem)

**Static Functions**: Private helper functions (declared `static`) use **snake_case** without the `St` prefix.
-   `static int find_free_entry(void)`

### 1.2 Variables & Parameters
-   **Format**: `snake_case` (applies to local, global, and static variables)
-   **Example**: `size_t page_size;`, `static int initialized;`

### 1.3 Data Types (Structs, Unions, Enums, Typedefs)
-   **Format**: **PascalCase**, often with `St` prefix if global.
-   **Structs**: `struct St_SomeStruct`
-   **Unions**: `union St_PageEntry`
-   **Enums**: `enum St_Status`
-   **Typedefs**: `St_PhysFrame`

### 1.4 Macros & Constants
-   **Format**: **UPPER_SNAKE_CASE**
-   **Example**: `PAGE_SIZE`, `STATUS_SUCCESS`, `MODULE_NAME`

---

## 2. File Organization

### 2.1 Header Guards
Headers must use unique include guards based on the file path. **Do NOT use `#pragma once`**.

**Format**: `__STRATA_[SUBDIR]_[FILENAME]_H__`
**Example**: `strata/include/strata/mm.h` -> `__STRATA_MM_H__`

### 2.2 Include Order
Includes should be grouped and separated by blank lines in the following order:
1.  **Related Header**: (If `foo.c`, include `foo.h` first)
2.  **Standard Library**: `<stdint.h>`, `<stddef.h>`, etc.
3.  **System/Arch Headers**: `<strata/arch/...>`
4.  **Project Headers**: `<strata/...>`

### 2.3 File Layout
-   **Module Name**: Define `MODULE_NAME` at the top of `.c` files.
-   **Macros/Constants**: Define after includes.
-   **Types**: Struct definitions (often `__packed`).
-   **Helper Functions**: `static inline` functions before main logic.
-   **Main Implementation**: Core functions.

---

## 3. Formatting & Style

### 3.1 Indentation
-   **Indent**: 4 spaces (No tabs).
-   **Alignment**: Align multi-line expressions or variable declarations if it improves readability.

### 3.2 Braces
-   **Style**: K&R (Kernighan & Ritchie)
    -   **Functions**: Opening brace on a **new line** after the function signature.
    -   **Control Structures** (if, while, etc.): Opening brace on the **same line**.
    -   Closing brace on a **new line**.
    -   `else` / `else if` on the same line as the closing brace.

```c
// Function: Brace on new line
void function(void)
{
    // Control: Brace on same line
    if (condition) {
        // code
    } else {
        // code
    }
}
```

### 3.3 Spacing
-   **Keywords**: Space after `if`, `for`, `while`, `switch`.
    -   `if (x)` (Correct)
    -   `if(x)` (Incorrect)
-   **Pointers**: The `*` binds to the **variable name**, not the type.
    -   `St_VirtPage *vpn` (Correct)
    -   `St_VirtPage* vpn` (Incorrect)
-   **Operators**: Spaces around binary operators (`=`, `+`, `==`, etc.).

### 3.4 Control Structures
-   **Single Line Statements**: Braces are optional for single-line statements, but heavily dependent on context. `return` on the same line is occasionally used for short checks.
    -   `if (!inverted) return -1;` (Seen in codebase)
-   **Switch**: standard indentation.

### 3.5 Compiler Attributes & Macros
Use the provided macros in `<strata/compiler.h>` instead of raw `__attribute__`.

-   `__always_inline`: Force inlining.
-   `__always_unused`: Suppress unused variable warnings.
-   `__noreturn`: Function never returns (e.g., panic, infinite loop).
-   `__naked`: Function has no prologue/epilogue (used for assembly handlers).
-   `__packed`: Pack structure members (no padding).
-   `__aligned(n)`: Align variable or structure to `n` bytes.
-   `__section(s)`: Place variable/function in specific ELF section.
-   `__format_printf(fmt, chk)`: Validate printf-style format strings.
-   `__malloc_like(free_func)`: Mark function as an allocator.
-   `__constructor` / `__destructor`: Function runs at startup/exit.

---

## 4. Error Handling

### 4.1 Status Codes
-   Functions returning status must return `StStatus` (from `<strata/status.h>`).
-   **Success**: return `STATUS_SUCCESS` (0).
-   **Failure**: return appropriate `STATUS_*` code (e.g., `STATUS_INVALID_PARAMETER`, `STATUS_INSUFFICIENT_MEMORY`).
-   **Checking**: Always use macros, **never** compare directly with 0.
    -   `CHECK_SUCCESS(status)`
    -   `CHECK_FAILURE(status)`

```c
StStatus status = Stp_SomeFunction();
if (!CHECK_SUCCESS(status)) {
    return status;
}
```

### 4.2 Panic
-   Use `St_Panic(code, message)` for unrecoverable system errors.

### 4.3 Logging
-   Use `LOG_TRACE`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR` from `<strata/log.h>`.

---

## 5. Type Usage & Standard Library

### 5.1 Integer Types
-   **Standard**: Use `<stdint.h>` types for fixed-width integers.
    -   `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`
    -   `int8_t`, `int16_t`, `int32_t`, `int64_t`
-   **Bit-width**: Do NOT use `short`, `int`, `long`, `long long` for data with specific width requirements.
-   **Sizes**: Use `size_t` for object sizes and counts.

### 5.2 Pointers & Addresses
-   **Arithmetic**: Use `uintptr_t` for address arithmetic.
-   **Void Pointer**: Use `void *` for generic data pointers.
-   **Physical/Virtual**: Use `St_PhysFrame` and `St_VirtPage` (defined in `<strata/types.h>`) for page-granularity values.

### 5.3 Booleans
-   **Do Not Use** `<stdbool.h>` or `bool` types.
-   Use `int` (0 for false, non-zero/1 for true).

---



## 6. Resource Cleanup & Error Recovery

### 6.1 The `has_error` Label
When a function performs multiple failable operations, use the **goto error** pattern with **NULL checks** for robust cleanup.

-   Initialize all pointers and resource handles to `NULL` (or an invalid state) at the start.
-   On failure, `goto has_error;`.
-   At the label, check if resources are non-NULL before freeing/releasing them.
-   Cleanup is performed in **reverse order** of allocation (though with NULL checks, order is less critical for independent resources, reverse is still best practice).

### 6.2 Example Pattern

```c
StStatus StExample_Create(struct StObject **out)
{
    StStatus status;
    struct StObject *obj = NULL;
    void *buffer = NULL;

    // 1. Initialize pointers to NULL (done above)
    // 2. Allocate Object (calloc ensures internal members are NULL)
    obj = calloc(1, sizeof(*obj));
    if (!obj) {
        status = STATUS_INSUFFICIENT_MEMORY;
        goto has_error;
    }

    // 3. Allocate Buffer
    buffer = malloc(4096);
    if (!buffer) {
        status = STATUS_INSUFFICIENT_MEMORY;
        goto has_error;
    }
    
    // Transfer ownership to object
    obj->buffer = buffer;
    buffer = NULL; // Clear local var so we don't double-free on error (optional if logic handles it)
                   // Or keep it and rely on obj->buffer check in cleanup if implicit

    // 4. Initialize Sub-resource
    // If Init function allocates, ensure it handles its own failure or sets pointer only on success.
    status = StResource_Init(&obj->sub_res); 
    if (!CHECK_SUCCESS(status)) goto has_error;


    // Success!
    *out = obj;
    return STATUS_SUCCESS;

has_error:
    // Cleanup - check for NULL/Validity
    if (obj) {
        // Free sub-resources attached to obj
        if (obj->buffer) free(obj->buffer);
        // StResource_Deinit checks internally or we check specific field
        StResource_Deinit(&obj->sub_res); 
        
        free(obj);
    }
    
    // If we had local resources not yet attached to obj, free them here
    if (buffer) free(buffer);

    return status;
}
```

