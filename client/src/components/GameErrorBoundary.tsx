import { Component, type ReactNode } from "react";

interface Props {
  children: ReactNode;
}

interface State {
  failed: boolean;
}

/** Last resort so a drawing error shows a message instead of a blank screen. */
export class GameErrorBoundary extends Component<Props, State> {
  state: State = { failed: false };

  static getDerivedStateFromError(): State {
    return { failed: true };
  }

  componentDidCatch(error: Error) {
    console.error("Zoogi Roll failed to draw.", error);
  }

  render() {
    if (!this.state.failed) return this.props.children;

    return (
      <div className="fixed inset-0 flex items-center justify-center bg-[#12081f] text-white p-6">
        <div className="max-w-sm text-center">
          <h1 className="text-2xl font-bold mb-3">Zoogi Roll hit a snag</h1>
          <p className="text-white/80 mb-6">
            The game screen failed to draw. Your phone is fine. Try again.
          </p>
          <button
            type="button"
            onClick={() => window.location.reload()}
            className="px-6 py-3 rounded-full bg-white text-black font-semibold"
          >
            Try again
          </button>
        </div>
      </div>
    );
  }
}
