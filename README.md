# Zoogi Roll

A browser marble battler. One Node server serves the page and the API. A match is played in the browser. Accounts, leaderboards, chat, and saved creations use local Postgres.

## What you need

- Node.js 20 or newer
- Docker Desktop, for Postgres

These commands are for Windows PowerShell. On macOS or Linux, use `cp .env.example .env` instead of `Copy-Item`.

## First-time setup

1. Copy the game art from the Replit project into `client/public`. This snapshot does not include those files. You want the `models`, `sounds`, `videos`, and `textures` folders, plus `main-bg-video.mp4` at the root of `client/public`. The menu loads without them. A grass-map match needs `models/floating_island_stage.glb` or the arena cannot draw that stage. Do not commit those binaries unless you have decided to store them in git.

2. In PowerShell, from the project folder:

```powershell
Copy-Item .env.example .env -Force
docker compose up -d
npm install
npm run db:push
npm run dev
```

3. Open http://127.0.0.1:5000

`npm run dev` loads `.env` before the server reads any settings. That works the same on Windows and Mac. Postgres from Compose listens only on `127.0.0.1:5432`. The default login in `.env.example` matches `docker-compose.yml` (`zoogi` / `zoogi`, database `zoogiroll`). Change both files together if this machine is shared.

## Day to day

```sh
docker compose up -d
npm run dev
```

Stop the database with `docker compose stop`. The page still starts if Postgres is down or `DATABASE_URL` is missing. Sign-in, scores, and other saved data then return a clear error instead of saving.

## Play from another device on your network

In `.env`:

```
HOST=0.0.0.0
```

Restart `npm run dev`, then open `http://<this-computer's-lan-address>:5000` from the other device.

## Check the built app

```sh
npm run build
npm start
```

`npm start` serves the built files. The login cookie is marked secure in that mode, so signing in over plain `http://127.0.0.1` will not stick. Use `npm run dev` for normal play on your machine.

## Optional Meshy key

Create Zoogi and Create Arena call Meshy. Put a key in `MESHY_API_KEY` inside `.env` when you want that. Leave it blank to play without it. The shop does not take payment.

## Useful commands

| Command | What it does |
| --- | --- |
| `npm run dev` | Game and API with live reload |
| `npm test` | Headless check of launch, collision, and scoring |
| `npm run check` | Typecheck |
| `npm run build` | Production bundle in `dist/` |
| `npm start` | Serve the production bundle |
| `npm run db:push` | Create or update tables in local Postgres |

More about how the game is put together is in [docs/design-notes.md](docs/design-notes.md).
