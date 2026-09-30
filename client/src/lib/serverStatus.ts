import { useSyncExternalStore } from "react";
import {
  initialServerStatus,
  messageForStatus,
  serverEnvFrom,
  type ServerStatus,
} from "./serverUrl";

const env = serverEnvFrom(
  import.meta.env.VITE_NATIVE_APP === "true",
  import.meta.env.VITE_API_BASE_URL,
);

let status: ServerStatus = initialServerStatus(env);
const listeners = new Set<() => void>();

export function getServerEnv() {
  return env;
}

export function getServerStatus(): ServerStatus {
  return status;
}

export function getOnlineMessage(): string | null {
  return messageForStatus(status);
}

export function setServerStatus(next: ServerStatus) {
  if (next === status) return;
  status = next;
  listeners.forEach((listener) => listener());
}

export function subscribeOnlineStatus(listener: () => void) {
  listeners.add(listener);
  return () => listeners.delete(listener);
}

export function useOnlineMessage(): string | null {
  return useSyncExternalStore(subscribeOnlineStatus, getOnlineMessage, () => null);
}
