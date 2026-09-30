/**
 * The 3D comic in reading order.
 *
 * Add a later chapter by putting its panel files under client/public/comics/
 * (for example comics/ch03/panel20.glb) and appending one entry here. The
 * viewer walks this list from start to end. It does not preload a chapter.
 *
 * Cameras keep the Spiraloid toolkit's global index: panel 1 is Camera.0000
 * and panel 9 is Camera.0008.
 */
export interface ComicChapterManifest {
  id: string;
  title: string;
  panels: readonly string[];
}

export const COMIC_MANIFEST: readonly ComicChapterManifest[] = [
  {
    id: "intro",
    title: "Intro",
    panels: [
      "/comics/intro/panel01.glb",
      "/comics/intro/panel02.glb",
      "/comics/intro/panel03.glb",
      "/comics/intro/panel04.glb",
      "/comics/intro/panel05.glb",
      "/comics/intro/panel06.glb",
      "/comics/intro/panel07.glb",
      "/comics/intro/panel08.glb",
    ],
  },
  {
    id: "ch01",
    title: "The Meadow Match",
    panels: [
      "/comics/ch01/panel09.glb",
      "/comics/ch01/panel10.glb",
      "/comics/ch01/panel11.glb",
      "/comics/ch01/panel12.glb",
      "/comics/ch01/panel13.glb",
      "/comics/ch01/panel14.glb",
    ],
  },
  {
    id: "ch02",
    title: "Volcanic Pit",
    panels: [
      "/comics/ch02/panel15.glb",
      "/comics/ch02/panel16.glb",
      "/comics/ch02/panel17.glb",
      "/comics/ch02/panel18.glb",
      "/comics/ch02/panel19.glb",
    ],
  },
];
