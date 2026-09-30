import type { CapacitorConfig } from "@capacitor/cli";

/**
 * iPhone app identity.
 *
 * Change the bundle id here (appId), then run `npm run cap:sync`.
 * That writes the same id into the Xcode project. Do not edit it in two places.
 */
const config: CapacitorConfig = {
  appId: "com.evanjames.zoogiroll",
  appName: "Zoogi Roll",
  webDir: "dist/ios",
  backgroundColor: "#12081f",
  ios: {
    contentInset: "never",
    backgroundColor: "#12081f",
    // Stops the page from bouncing when a finger drags past the edge.
    scrollEnabled: false,
    preferredContentMode: "mobile",
  },
  plugins: {
    SplashScreen: {
      launchAutoHide: true,
      launchShowDuration: 1500,
      backgroundColor: "#12081f",
      showSpinner: false,
      splashFullScreen: true,
      splashImmersive: true,
    },
    StatusBar: {
      overlaysWebView: true,
      style: "DARK",
      backgroundColor: "#12081f",
    },
  },
};

export default config;
