#include "macros/btlcmd.inc"

_start:
    CompareMonDataToValue OPCODE_FLAG_NOT, BTLSCR_MSG_TEMP, BATTLEMON_VOLATILE_STATUS, VOLATILE_CONDITION_SUBSTITUTE, _change
    PlayBattleAnimation BTLSCR_MSG_TEMP, BATTLE_ANIMATION_SUB_OUT
    Wait
    RestoreSprite BTLSCR_MSG_TEMP
    Wait
    PlayBattleAnimation BTLSCR_MSG_TEMP, BATTLE_ANIMATION_SUB_IN
    Wait
_change:
    PlaySound BTLSCR_MSG_TEMP, 1980
    SetMosaic BTLSCR_MSG_TEMP, 8, 1
    Wait
    ChangeForm BTLSCR_MSG_TEMP
    PlaySound BTLSCR_MSG_TEMP, 1984
    SetMosaic BTLSCR_MSG_TEMP, 0, 1
    Wait
    CompareMonDataToValue OPCODE_FLAG_NOT, BTLSCR_MSG_TEMP, BATTLEMON_VOLATILE_STATUS, VOLATILE_CONDITION_SUBSTITUTE, _done
    PlayBattleAnimation BTLSCR_MSG_TEMP, BATTLE_ANIMATION_SUB_OUT
    Wait
    RefreshSprite BTLSCR_MSG_TEMP
    Wait
    PlayBattleAnimation BTLSCR_MSG_TEMP, BATTLE_ANIMATION_SUB_IN
    Wait
_done:
    End
