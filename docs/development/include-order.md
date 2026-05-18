# Include Order {#development_include_order}

Keep include groups separated by one blank line. The order is:

1. `config.h`, if the file needs generated configuration.
2. The implementation target header, if there is one.
3. Standard C library headers.
4. External library headers. If platform or architecture variants exist,
   include the selected platform/architecture header before the generic header.
5. Kernel or bootloader headers. Within this group, platform/architecture
   headers come before generic subsystem headers.
6. Common shared headers such as `stload/*`.
7. Internal, private, or generated local headers.

Example:

```c
#include "config.h"

#include <strata/thread.h>

#include <assert.h>
#include <stdint.h>

#include <strata/plat/thread.h>

#include <strata/compiler.h>
#include <strata/status.h>

#include <stload/bootinfo.h>

#include "internal.h"
```

Headers should be self-contained and include the narrowest header that owns the
declaration. Do not create broad umbrella headers merely to avoid thinking
about ownership of declarations.

Use existing guard style:

- `__STRATA_..._H__`;
- `__VELLUM_..._H__`;
- `__STLOAD_..._H__`.

Do not use `#pragma once`.
