import RAPIER from "@dimforge/rapier3d-compat";

let rapierInstance: typeof RAPIER | null = null;
let initPromise: Promise<typeof RAPIER> | null = null;

export async function initRapier(): Promise<typeof RAPIER> {
  if (rapierInstance) {
    return rapierInstance;
  }
  
  if (initPromise) {
    return initPromise;
  }
  
  initPromise = RAPIER.init().then(() => {
    rapierInstance = RAPIER;
    console.log("Rapier physics initialized");
    return RAPIER;
  });
  
  return initPromise;
}

export function getRapier(): typeof RAPIER {
  if (!rapierInstance) {
    throw new Error("Rapier not initialized. Call initRapier() first.");
  }
  return rapierInstance;
}

export function isRapierReady(): boolean {
  return rapierInstance !== null;
}
