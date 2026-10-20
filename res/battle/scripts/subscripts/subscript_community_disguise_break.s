#include "macros/btlcmd.inc"

_start:
    PrintMessage BattleStrings_Text_CommunityDisguiseHit, TAG_NONE
    Wait
    WaitButtonABTime 30
    Call BATTLE_SUBSCRIPT_COMMUNITY_FORM_CHANGE
    UpdateVar OPCODE_FLAG_ON, BTLVAR_BATTLE_CTX_STATUS, SYSCTL_SKIP_SPRITE_BLINK
    Call BATTLE_SUBSCRIPT_UPDATE_HP
    PrintMessage BattleStrings_Text_CommunityDisguiseBroken_Ally, TAG_NICKNAME, BTLSCR_MSG_TEMP
    Wait
    WaitButtonABTime 30
    End
