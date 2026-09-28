const STORAGE_KEY = "zoogi_device_id";

export function getDeviceId(): string {
  let deviceId = localStorage.getItem(STORAGE_KEY);
  if (!deviceId) {
    deviceId = `device_${Date.now()}_${Math.random().toString(36).substring(2, 15)}`;
    localStorage.setItem(STORAGE_KEY, deviceId);
  }
  return deviceId;
}
