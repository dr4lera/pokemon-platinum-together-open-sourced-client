#include "macros/btlcmd.inc"

_start:
    Call BATTLE_SUBSCRIPT_COMMUNITY_FORM_CHANGE
    PrintMessage BattleStrings_Text_CommunityStanceDefense, TAG_NONE
    Wait
    WaitButtonABTime 30
    End
