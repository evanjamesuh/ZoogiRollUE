/**
 * Night Circuit grandstand decks. Every box sits outside the playable court
 * (|x| > 12.4 or |z| > 8.4) so the bowl can grow without touching gameplay.
 * Face is the side that looks at the court.
 */
export type BowlFace = "north" | "south" | "east" | "west";

export type BowlDeck = {
  pos: [number, number, number];
  size: [number, number, number];
  face: BowlFace;
};

/** Far bowl, stepping up and back. The near tiers fill the gameplay frame. */
export const FAR_DECKS: BowlDeck[] = [
  { pos: [0, 0.95, -9.3], size: [38, 1.7, 1.5], face: "north" },
  { pos: [0, 1.85, -10.85], size: [42, 1.85, 1.55], face: "north" },
  { pos: [0, 2.55, -12.45], size: [46, 1.9, 1.6], face: "north" },
  { pos: [0, 3.15, -14.15], size: [50, 1.9, 1.7], face: "north" },
  { pos: [0, 3.55, -16], size: [54, 1.8, 1.85], face: "north" },
  { pos: [0, 3.85, -17.9], size: [56, 1.7, 1.9], face: "north" },
];

/** Flanks beside the court, stepping out and up. */
export const SIDE_DECKS: BowlDeck[] = [
  { pos: [-14.6, 1.7, -1.2], size: [2.6, 3.2, 13.5], face: "west" },
  { pos: [14.6, 1.7, -1.2], size: [2.6, 3.2, 13.5], face: "east" },
  { pos: [-17.8, 3.55, -1.6], size: [3.0, 3.5, 15], face: "west" },
  { pos: [17.8, 3.55, -1.6], size: [3.0, 3.5, 15], face: "east" },
  { pos: [-21.2, 5.55, -2.0], size: [3.2, 3.6, 16.5], face: "west" },
  { pos: [21.2, 5.55, -2.0], size: [3.2, 3.6, 16.5], face: "east" },
];

/** Near end of the bowl, behind the camera until a rally opens the view. */
export const END_DECKS: BowlDeck[] = [
  { pos: [0, 1.8, 11.4], size: [40, 3.3, 2.5], face: "south" },
  { pos: [0, 3.9, 14.6], size: [48, 3.6, 2.7], face: "south" },
  { pos: [0, 6.1, 17.9], size: [54, 3.8, 2.9], face: "south" },
];

export const BOWL_DECKS: BowlDeck[] = [...FAR_DECKS, ...SIDE_DECKS, ...END_DECKS];
