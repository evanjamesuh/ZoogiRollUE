# Wiring power sounds and effects

These pieces are standalone. Nothing in the match calls them yet. `isMuted` in `useAudio` still starts `true`, so a power stays silent until the player unmutes. Do not add audio files. Do not add point lights that mount when a power starts; the new effects use emissive and additive materials so a mid-match shader compile does not hitch.

Import the visuals from `client/src/components/game/PowerEffects.tsx`. Play sounds through the store (`useAudio.getState().playExplosion()` and the matching methods below) or call the functions in `client/src/lib/powerSounds.ts` directly. Both paths check the mute flag.

| Sound on `useAudio` | Visual | What it is |
| --- | --- | --- |
| `playPowerUnlock()` | `PowerUnlockFlash` | Star coin unlocks a one-shot power |
| `playWolfDash()` | `WolfCloneLook` | Wolfgang's three clones |
| `playExplosion()` | `ExplosionBlast` | Hotstreak's radius-8 blast |
| `playStunZap()` | `StunBurst` | Bolt's radius-8 shock |
| `playStunEnd()` | `StunnedIndicator` | Marble is stunned, then shakes it off |

`ExplosionBlast`, `StunBurst`, and `PowerUnlockFlash` take `position`, `startTime` (ms from `Date.now()`), and `radius`. They animate in `useFrame` and unmount themselves after about 0.9s, 0.9s, and 0.55s. Pass `radius={8}` for the Hotstreak and Bolt blasts so the ring matches the push. Pass about `radius={5.2}` for the unlock flash so it still reads from the wide follow camera. `StunnedIndicator` takes `position`, `remaining`, and `duration`. `WolfCloneLook` takes `position` and `velocity`.

## Star coin unlock

Whoever last hit a star coin gets that power. Two store paths both unlock today. Call the chime and record a flash in both, and only once per coin (the physics tick marks the orb `isOutOfRing` before `removeOrb` can run).

1. `useZoogiGame.tsx`, inside the main physics tick, in the orb loop where `orb.isStarOrb && orb.starOrbType` grants `wolfgangAbilityUnlocked`, `hotstreakAbilityUnlocked`, or `boltAbilityUnlocked` (the block that starts just after `triggerKnockoffBoundaryFlash` for a knockout). This is the path that runs when a star coin leaves the ring.
2. `useZoogiGame.tsx` `removeOrb`, in both branches that already log the unlock: `orb.lastHitBy === "player"` and the local-multiplayer `orb.lastHitBy === "enemy"` branch.

At each site:

```ts
useAudio.getState().playPowerUnlock();
```

Keep a short list on the store, for example `powerUnlocks: { id: string; position: [number, number, number]; startTime: number; color: string }[]`. Push one entry with `position` copied from `orb.position`, `startTime: Date.now()`, and a color from the coin: gold `#ffd700` for `wolfgang`, red `#ff4444` for `hotstreak`, blue `#4488ff` for `bolt`.

In `Game.tsx`, next to `<ExplosionEffect />`, render:

```tsx
{powerUnlocks.map((flash) => (
  <PowerUnlockFlash
    key={flash.id}
    position={flash.position}
    startTime={flash.startTime}
    radius={5.2}
    color={flash.color}
  />
))}
```

Drop entries older than about 700ms when you prune the list. `PowerUnlockFlash` also returns null on its own after 0.55s.

## Wolfgang clones

In `activateWolfgangAbility`, immediately after `get().spawnWolfClones(controlledEntity.position, velocity)`:

```ts
useAudio.getState().playWolfDash();
```

`spawnWolfClones` already places three clones in a fan. Do not play the whoosh inside `updateWolfClones`.

The gray sphere is `WolfClone` in `client/src/components/game/Effects.tsx` (the mesh `WolfClones` renders, including its `pointLight`). Replace that mesh with:

```tsx
<WolfCloneLook position={clone.position} velocity={clone.velocity} />
```

Leave `WolfClones` mounted from `Game.tsx` where it already is. The clone keeps the store position; the new look adds the silver-blue body, ears, fang badge, and trail.

## Hotstreak explosion

In `activateHotstreakAbility`, next to `get().triggerExplosion(explosionPos)` (the call that uses `EXPLOSION_RADIUS = 8`):

```ts
useAudio.getState().playExplosion();
```

`triggerExplosion` sets `showExplosion` to `{ position, timestamp, color }`. In `Game.tsx`, where `<ExplosionEffect />` is rendered, when `showExplosion` is set and `showExplosion.color` is not `"yellow"`:

```tsx
<ExplosionBlast
  position={showExplosion.position}
  startTime={showExplosion.timestamp}
  radius={8}
/>
```

Retire the orange sphere and the `pointLight` inside `ExplosionEffect` for this case. The blast's disc and ring both reach radius 8, which is the same radius as the push in `activateHotstreakAbility`.

Grenades in `tickTimers` also call `triggerExplosion`, with `GRENADE_RADIUS = 4`. If those should share the look, pass `radius={4}` and do not reuse the radius-8 star blast. A grenade does not need `playExplosion()` unless you want the same boom.

## Bolt stun

In `activateBoltAbility`, next to `get().triggerExplosion(explosionPos, "yellow")`:

```ts
useAudio.getState().playStunZap();
```

In `Game.tsx`, when `showExplosion.color === "yellow"`, render `StunBurst` instead of `ExplosionEffect`'s lightning (that lightning calls `Math.random()` while rendering, so it jumps on every parent render):

```tsx
<StunBurst
  position={showExplosion.position}
  startTime={showExplosion.timestamp}
  radius={8}
/>
```

Radius 8 matches `EXPLOSION_RADIUS` in `activateBoltAbility`. Bolt still sets `isStunned: true` and `stunTimer: 2` on each marble inside that radius.

## Stunned marble

`StunnedIndicator` replaces the small yellow octahedrons. Mount it only while `isStunned` is true.

- `Zoogi.tsx` `PlayerZoogi`: replace `{playerEntity.isStunned && <StunEffect position={pos} />}`. `StunEffect` also mounts a `pointLight`; drop that light with it.
- `Zoogi.tsx` `EnemyZoogi`: replace the `{enemy.isStunned && (` group of three octahedrons above the marble. Leave the marble's existing `pointLight` alone, or stop turning it yellow, so a stun does not compile a new light.
- `Zoogi.tsx` `LocalMultiplayerZoogi`: there is no stun marker today. The rendered entity is `playerIndex === 0 ? playerEntity : enemies[playerIndex - 1]`. Add the indicator beside the `pointLight` at the bottom of that component when `entity?.isStunned`.

```tsx
{entity.isStunned && (
  <StunnedIndicator
    position={entity.position}
    remaining={entity.stunTimer}
    duration={2}
  />
)}
```

`duration={2}` matches Bolt's `stunTimer: 2`. `applyStun` writes `stunTimer: 4`; pass `duration={4}` there if that stun should show a full ring at the start. The ring shrinks from `remaining / duration` and keeps moving between store updates, so the physics tick does not have to re-render every frame for the ring to move.

Play the shake-off once, on the transition into "not stunned", inside `tickTimers`:

- Player block: today `if (stunTimer <= 0)` clears `isStunned`. Call `playStunEnd()` only when `player.isStunned` was already true.
- Enemy block: inside `if (e.isStunned)`, when `newStunTimer <= 0`, call `playStunEnd()` before writing `isStunned: false`.

Do not call it on later ticks when the timer is already 0.

## Preview

`?powerPreview=1` renders `PowerPreview` from the `usePowerPreview` hook in `App.tsx` and skips the match. The four buttons play the same store methods and the same components. That page is not part of a match.
