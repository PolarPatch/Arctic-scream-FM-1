/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* The flash chips an FM-1 can carry, by the JEDEC id (command 0x9F). Both are 1 MiB parts that take the same SPI NOR
 * commands (read, page program, 4 KiB erase, status, write enable), so only the id differs.
 *   0x856014  Puya P25Q80, the first FM-1s
 *   0xB36014  UC25HQ80, FM-1s sold since autumn 2026
 * Any other id: the flash is left alone (saves stay in RAM, the update loader writes nothing).
 * After SLOOP 2.8.5 (isod89) and Felucca's fix #320 (Hügelton Instruments), which found the second chip. */
#pragma once

#define FL_ID_P25Q80   0x856014u
#define FL_ID_UC25HQ80 0xB36014u

/* a macro, never a call: the loader and the RAM code may not reach into XIP */
#define fl_id_known(id) ((id) == FL_ID_P25Q80 || (id) == FL_ID_UC25HQ80)
