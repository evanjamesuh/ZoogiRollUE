import { createRoot } from "react-dom/client";
import App from "./App";
import { installMobileTouchGuards } from "./lib/mobileTouch";
import "./index.css";

installMobileTouchGuards();
createRoot(document.getElementById("root")!).render(<App />);
