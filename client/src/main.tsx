import { createRoot } from "react-dom/client";
import App from "./App";
import { GameErrorBoundary } from "./components/GameErrorBoundary";
import { installMobileTouchGuards } from "./lib/mobileTouch";
import { installNativeApi } from "./lib/installNativeApi";
import "./index.css";

installMobileTouchGuards();
installNativeApi();
createRoot(document.getElementById("root")!).render(
  <GameErrorBoundary>
    <App />
  </GameErrorBoundary>,
);
