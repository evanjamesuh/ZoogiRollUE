/**
 * Heights that sit a round body on the y=0 floor.
 * Draw radius and physics radius are the same number: a Zoogi is 0.5
 * and an orb is 0.4, so the centre rests one radius up and the bottom
 * touches the floor. The Zoogi-to-orb diameter ratio is 1.25, about 1.3.
 */
export const ZOOGI_DRAW_RADIUS = 0.5;
export const ZOOGI_REST_Y = ZOOGI_DRAW_RADIUS;
/** Collision and draw width of a Zoogi. Arena spans are counted in these. */
export const ZOOGI_DIAMETER = ZOOGI_DRAW_RADIUS * 2;

/** Comic blue orb. Smaller than a Zoogi, and the same size as its collider. */
export const ORB_DRAW_RADIUS = 0.4;
export const ORB_REST_Y = ORB_DRAW_RADIUS;

/** Panel 06 Orb_sky material: glossy blue with a soft sky glow. */
export const ORB_BODY_COLOR = "#0341CA";
export const ORB_GLOW_COLOR = "#0F4A99";
