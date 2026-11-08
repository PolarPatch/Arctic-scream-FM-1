/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* The FM-1's flash chips (firmware/hal/fm1_flash_id.h): the Puya P25Q80 and the UC25HQ80 are known, nothing else is
 * (another id, no chip at all, a bus that reads all ones). */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "fm1_flash_id.h"

int main(void)
{
    static const uint32_t no[] = {0u, 0xFFFFFFu, 0xFFFFFFFFu, 0xEF4014u, 0x856015u, 0xB36013u, 0x85601400u};
    uint32_t i;
    assert(fl_id_known(0x856014u) && fl_id_known(0xB36014u));
    for (i = 0; i < sizeof no / sizeof no[0]; i++) assert(!fl_id_known(no[i]));
    puts("flash  the P25Q80 (0x856014) and the UC25HQ80 (0xB36014) known, other ids not");
    return 0;
}
