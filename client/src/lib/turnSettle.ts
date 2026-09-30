import { SETTLE_DELAY_STEPS, SETTLE_TIMEOUT_STEPS } from "./simFeel";

/**
 * The turn passes only after a real shot, once every body has been still
 * for the settle delay. If something keeps moving, the timeout still passes
 * the turn so a jittering contact cannot stall the match.
 */
export function turnHandoffReady(input: {
  shotHappened: boolean;
  courtStill: boolean;
  settleHoldSteps: number;
  unsettledSteps: number;
}): { pass: boolean; settleHoldSteps: number; unsettledSteps: number } {
  if (!input.shotHappened) {
    return { pass: false, settleHoldSteps: 0, unsettledSteps: 0 };
  }
  const unsettledSteps = input.unsettledSteps + 1;
  if (unsettledSteps >= SETTLE_TIMEOUT_STEPS) {
    return { pass: true, settleHoldSteps: 0, unsettledSteps: 0 };
  }
  if (input.courtStill) {
    const settleHoldSteps = input.settleHoldSteps + 1;
    if (settleHoldSteps >= SETTLE_DELAY_STEPS) {
      return { pass: true, settleHoldSteps: 0, unsettledSteps: 0 };
    }
    return { pass: false, settleHoldSteps, unsettledSteps };
  }
  return { pass: false, settleHoldSteps: 0, unsettledSteps };
}
