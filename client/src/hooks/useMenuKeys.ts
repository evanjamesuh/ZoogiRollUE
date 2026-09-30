import { useEffect, useRef } from "react";

interface MenuKeyHandlers {
  onConfirm?: () => void;
  onBack?: () => void;
}

function isTyping(target: EventTarget | null): boolean {
  if (!(target instanceof HTMLElement)) return false;
  const tag = target.tagName;
  if (tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT") return true;
  return target.isContentEditable;
}

/** Enter confirms and Escape goes back, except while typing or when a button is focused. */
export function useMenuKeys(handlers: MenuKeyHandlers) {
  const handlersRef = useRef(handlers);
  handlersRef.current = handlers;

  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (event.repeat || event.altKey || event.metaKey || event.ctrlKey) return;
      if (event.key === "Escape") {
        if (!handlersRef.current.onBack) return;
        event.preventDefault();
        handlersRef.current.onBack();
        return;
      }
      if (event.key !== "Enter") return;
      if (!handlersRef.current.onConfirm) return;
      if (isTyping(event.target)) return;
      if (event.target instanceof HTMLElement && event.target.closest("button, a")) return;
      event.preventDefault();
      handlersRef.current.onConfirm();
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);
}
