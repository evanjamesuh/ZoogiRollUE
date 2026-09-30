/**
 * A marble that is still in the match stays drawn.
 * Whose turn it is does not matter. Only a knockout hides the body.
 */
export function marbleIsShown(
  entity: { isKnockedOut?: boolean; isRespawning?: boolean } | null | undefined,
  firstPerson = false,
): boolean {
  if (!entity || firstPerson) return false;
  return !entity.isKnockedOut && !entity.isRespawning;
}
