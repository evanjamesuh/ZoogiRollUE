/**
 * Heights that sit a round body on the y=0 floor.
 * The centre is one draw-radius up, so the bottom touches the surface
 * and the top stays clear of it.
 */
export const ZOOGI_DRAW_RADIUS = 0.86;
export const ZOOGI_REST_Y = ZOOGI_DRAW_RADIUS;
/** Visual width of a Zoogi. Arena scales are sized in these diameters. */
export const ZOOGI_DIAMETER = ZOOGI_DRAW_RADIUS * 2;

/** Comic blue orb. Larger than the collision circle so it reads from the high camera. */
export const ORB_DRAW_RADIUS = 0.62;
export const ORB_REST_Y = ORB_DRAW_RADIUS;

/** Panel 06 Orb_sky material: glossy blue with a soft sky glow. */
export const ORB_BODY_COLOR = "#0341CA";
export const ORB_GLOW_COLOR = "#0F4A99";
