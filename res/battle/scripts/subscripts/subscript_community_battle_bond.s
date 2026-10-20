#include "macros/btlcmd.inc"

_start:
    PrintMessage BattleStrings_Text_CommunityBondStrength_Ally, TAG_NICKNAME, BTLSCR_MSG_TEMP
    Wait
    WaitButtonABTime 30
    Call BATTLE_SUBSCRIPT_COMMUNITY_FORM_CHANGE
    PrintMessage BattleStrings_Text_CommunityBondForm_Ally, TAG_NICKNAME, BTLSCR_MSG_TEMP
    Wait
    WaitButtonABTime 30
    End
