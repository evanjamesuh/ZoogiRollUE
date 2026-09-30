# Zoogi Roll design notes

These are notes about how the game is built. Setup steps live in the README.

## What it is

Zoogi Roll is a 3D arena battler. You drag and release to launch a Zoogi, knock orbs and other marbles around, and score. The match itself runs in the browser. The Node server serves the page, stores accounts and community data, and relays voice-chat setup.

The client is React, TypeScript, and Tailwind, with Three.js through React Three Fiber. Match state lives in a Zustand store. The server is Express. Saved data uses Postgres and Drizzle.

## Playing

Five base Zoogis are Wolfgang, Hotstreak, Lars, Pinpoint, and Bolt. Each has its own ability. Trajectory colors follow the character: Wolfgang gray, Hotstreak orange, Lars blue, Pinpoint purple, Bolt yellow. Power is shown as low, medium, or high from how far you drag.

From the menu you can play against the computer or play local pass-and-play on one device. Story opens the comic. Settings covers sound and the arena button size.

Arenas are circular maps with obstacles such as mushrooms, snowmen, aliens, and trees. Ice patches speed marbles up. Flower patches slow them. The grass, ice, space, and other themes draw extra models from `client/public` when those files are present. Orbs bob and spin. Scoring zones and the knockout ring are part of the match.

The camera can be birds-eye, first person, over the shoulder, or a developer view. Drag-to-launch is turned off in the views that already use drag to move the camera.

## Accounts and saved data

Registration and login use bcrypt password hashes and an Express session cookie. Profiles track wins, losses, knockoffs, high score, games played, coins, gems, level, and XP. Friends, global chat, and direct messages are stored in Postgres.

The leaderboard, clans, tournaments, daily bonus, challenges, gallery, arena showcase, and replays have API routes and screens. A match result is still kept mainly in the browser; do not assume every score is written to Postgres unless that screen says it saved.

Map decorations can be saved per device and loaded again when that map is picked.

## Create Zoogi and Create Arena

Both can send an image to Meshy and store the result in Postgres, scoped by device id. That needs `MESHY_API_KEY`. A circular crop tool prepares the image before it is sent. Generated models are downloaded through `/api/meshy/download/:taskId`.

## Shop

The shop shows premium characters and says Coming Soon. There is a `purchases` table for a future shop. There is no checkout.

## Voice

Voice chat uses a WebSocket on this server for signaling and WebRTC between browsers. It is optional and is not required to play a match.

## Art

Models, sounds, videos, and textures are files under `client/public`. They are not required for the menu. They are required for the 3D stages and music.
