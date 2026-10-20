#include <assert.h>
#include <stdio.h>
#include "battle/capture_rules_core.h"

typedef struct Inventory {
    int count;
    int debits;
} Inventory;

static int RemoveOne(void *context)
{
    Inventory *inventory = context;
    if (inventory->count == 0) {
        return 0;
    }
    inventory->count--;
    inventory->debits++;
    return 1;
}

int main(void)
{
    CaptureRuleSlot slots[4] = {
        { 0, 50, 1, 0 }, { 1, 40, 25, 0 },
        { 0, 30, 4, 0 }, { 1, 20, 133, 0 }
    };
    int target;
    assert(CaptureRules_SelectTarget(0, 0, slots, 2, &target) == CAPTURE_RULE_OK && target == 1);
    assert(CaptureRules_SelectTarget(1, 0, slots, 2, &target) == CAPTURE_RULE_TRAINER && target == -1);
    assert(CaptureRules_SelectTarget(2, 0, slots, 4, &target) == CAPTURE_RULE_MULTIPLE_TARGETS && target == -1);
    assert(CaptureRules_SelectTarget(0x8e, 0, slots, 4, &target) == CAPTURE_RULE_MULTIPLE_TARGETS);
    slots[1].hp = 0;
    assert(CaptureRules_SelectTarget(0x8e, 0, slots, 4, &target) == CAPTURE_RULE_OK && target == 3);
    assert(CaptureRules_SelectTarget(0x8f, 0, slots, 4, &target) == CAPTURE_RULE_TRAINER);
    slots[3].absent = 1;
    assert(CaptureRules_SelectTarget(0x8e, 0, slots, 4, &target) == CAPTURE_RULE_NO_TARGET && target == -1);
    slots[1].hp = 40;
    slots[1].species = 0;
    assert(CaptureRules_SelectTarget(0, 0, slots, 4, &target) == CAPTURE_RULE_NO_TARGET);
    slots[1].species = 25;
    slots[1].absent = 1;
    assert(CaptureRules_SelectTarget(0, 0, slots, 4, &target) == CAPTURE_RULE_NO_TARGET);
    slots[1].absent = 0;
    assert(CaptureRules_SelectTarget(0, 1, slots, 4, &target) == CAPTURE_RULE_NO_TARGET);
    assert(CaptureRules_SelectTarget(0, 0, slots, 0, &target) == CAPTURE_RULE_NO_TARGET);
    assert(CaptureRules_SelectTarget(0, 0, slots, 5, &target) == CAPTURE_RULE_NO_TARGET);
    Inventory inventory = { 2, 0 };
    int lastTurn = 0;
    unsigned usedMask = 0;
    assert(CaptureRules_CommitDebit(0, 0, &lastTurn, &usedMask, RemoveOne, &inventory) == CAPTURE_RULE_OK);
    assert(inventory.count == 1 && inventory.debits == 1);
    assert(CaptureRules_CommitDebit(0, 0, &lastTurn, &usedMask, RemoveOne, &inventory) == CAPTURE_RULE_ALREADY_USED);
    assert(inventory.count == 1 && inventory.debits == 1);
    assert(CaptureRules_CommitDebit(0, 2, &lastTurn, &usedMask, RemoveOne, &inventory) == CAPTURE_RULE_OK);
    assert(inventory.count == 0 && inventory.debits == 2);
    assert(CaptureRules_CommitDebit(1, 0, &lastTurn, &usedMask, RemoveOne, &inventory) == CAPTURE_RULE_NO_BALL);
    assert(inventory.count == 0 && inventory.debits == 2);
    inventory.count = 1;
    assert(CaptureRules_CommitDebit(1, 0, &lastTurn, &usedMask, RemoveOne, &inventory) == CAPTURE_RULE_OK);
    assert(inventory.count == 0 && inventory.debits == 3);
    assert(CaptureRules_CommitDebit(2, -1, &lastTurn, &usedMask, RemoveOne, &inventory) == CAPTURE_RULE_NO_TARGET);
    assert(inventory.debits == 3);
    puts("capture rules core: target eligibility and transactional debit checks passed");
    return 0;
}
