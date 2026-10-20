#include "macros/btlcmd.inc"

_start:
    Call BATTLE_SUBSCRIPT_COMMUNITY_FORM_CHANGE
    PrintMessage BattleStrings_Text_CommunitySchoolSolo_Ally, TAG_NICKNAME, BTLSCR_MSG_TEMP
    Wait
    WaitButtonABTime 30
    End
