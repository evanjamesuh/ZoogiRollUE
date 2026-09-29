export type MenuScreen = "home" | "play" | "settings" | "story";

let nextScreen: MenuScreen = "home";

/** The next time the main menu opens, show this screen once. Used by Back from setup. */
export function returnToMenuScreen(screen: MenuScreen) {
  nextScreen = screen;
}

export function consumeMenuScreen(): MenuScreen {
  const screen = nextScreen;
  nextScreen = "home";
  return screen;
}
