// SPDX-License-Identifier: GPL-3.0-or-later
#include "macros/btlcmd.inc"

// Reserved modern subscript slots fail explicitly until their owning rules
// and presentation are implemented. Repeated NARC members preserve native IDs.
    UpdateVar OPCODE_FLAG_ON, BTLVAR_MOVE_STATUS_FLAGS, MOVE_STATUS_FAILED
    End
