/**
 * Night Circuit grandstand decks. Visual only: these boxes are not colliders.
 * The court ends at |x| = 12 and |z| = 8. Every deck stays several meters
 * outside that line so the bowl reads as a pit around the floor.
 * Face is the side that looks at the court.
 */
export type BowlFace = "north" | "south" | "east" | "west";

export type BowlDeck = {
  pos: [number, number, number];
  size: [number, number, number];
  face: BowlFace;
};

/**
 * Far bowl, about twice the old reach. The first tier is the one the tight
 * camera still catches along the top of the frame; the rest climbs back
 * into the dark.
 */
export const FAR_DECKS: BowlDeck[] = [
  { pos: [0, 3.0, -14.85], size: [76, 3.7, 3.3], face: "north" },
  { pos: [0, 3.7, -20.35], size: [84, 3.8, 3.2], face: "north" },
  { pos: [0, 5.5, -24.7], size: [92, 3.8, 3.4], face: "north" },
  { pos: [0, 7.2, -29.3], size: [100, 3.8, 3.6], face: "north" },
  { pos: [0, 8.6, -34.1], size: [108, 3.6, 3.8], face: "north" },
  { pos: [0, 9.8, -39.0], size: [116, 3.4, 4.0], face: "north" },
];

/** Flanks set back from the side rails, stepping out and up. */
export const SIDE_DECKS: BowlDeck[] = [
  { pos: [-21.2, 4.65, -1.05], size: [5.6, 6.4, 24], face: "west" },
  { pos: [21.2, 4.65, -1.05], size: [5.6, 6.4, 24], face: "east" },
  { pos: [-27.65, 4.5, 0.95], size: [6.2, 7.0, 28], face: "west" },
  { pos: [27.65, 4.5, 0.95], size: [6.2, 7.0, 28], face: "east" },
  { pos: [-34.65, 5.6, 2.95], size: [6.6, 7.2, 32], face: "west" },
  { pos: [34.65, 5.6, 2.95], size: [6.6, 7.2, 32], face: "east" },
];

/** Near end of the bowl, behind the camera until a rally opens the view. */
export const END_DECKS: BowlDeck[] = [
  { pos: [0, 3.55, 23.4], size: [84, 6.6, 5.4], face: "south" },
  { pos: [0, 7.5, 30.2], size: [100, 7.2, 5.8], face: "south" },
  { pos: [0, 11.4, 37.2], size: [116, 7.6, 6.2], face: "south" },
];

export const BOWL_DECKS: BowlDeck[] = [...FAR_DECKS, ...SIDE_DECKS, ...END_DECKS];
