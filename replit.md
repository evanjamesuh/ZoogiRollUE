# Zoogi Roll Arena

## Overview

Zoogi Roll Arena is a fast-paced, physics-driven 3D arena battler built with React Three Fiber. Players launch customizable creatures, called Zoogis, into dynamic arenas using drag-and-release mechanics. The game features real-time physics, competitive gameplay, and persistent leaderboards. The project aims to deliver an engaging WebGL-based gaming experience with unique character abilities and strategic environmental interactions.

## User Preferences

- I want iterative development.
- Ask before making major changes.

## System Architecture

The project is built with React 18, TypeScript, and Tailwind CSS for the frontend, utilizing @react-three/fiber and @react-three/drei for 3D graphics. State management is handled by Zustand, and animations are powered by Framer Motion. The backend uses Express.js with PostgreSQL and Drizzle ORM for persistent data storage, primarily for leaderboards.

### UI/UX Decisions
- Main menu features a video background with right-side wheel menu for navigation. Bottom buttons provide quick access to "Create Zoogi" and "Create Arena" features.
- The 3D Town carousel has been removed in favor of a cleaner video-based main menu.
- A music toggle button is included in the GameUI.
- In-game UI displays health, score, abilities, opponent scores, and turn indicators for multiplayer.
- Leaderboard and Zoogipedia are accessible via modals.
- Trajectory lines are character-specific colors (Wolfgang=gray, Hotstreak=orange, Lars=blue, Pinpoint=purple, Bolt=yellow).
- Power indicators show LOW (green), MEDIUM (yellow), HIGH (red) based on drag distance.

### Technical Implementations
- **User Authentication**: Complete user account system with registration, login, and session management. Uses bcryptjs for password hashing and express-session for session storage. Users can create accounts to track stats, add friends, and access community features.
- **User Profiles**: Extended user profiles with stats tracking (wins, losses, knockoffs, high score, games played), virtual currency (coins, gems), leveling system (XP and level), login streak tracking, and avatar/display name customization.
- **Friend System**: Users can search for other players, send friend requests, accept/reject requests, and manage their friends list. Friends can message each other directly.
- **Chat System**: Global chat channel for all logged-in users, plus direct messaging between friends. Messages persist in PostgreSQL.
- **Custom Zoogi Creator**: Image-to-3D generation using Meshy.ai API. Users can upload images, generate 3D models, customize stats, and play with their custom creations. Backend service handles API calls securely with MESHY_API_KEY. Custom Zoogis persist to PostgreSQL database using deviceId for user scoping. Models are served via proxy endpoint (/api/meshy/download/:taskId) to avoid CORS issues. Features circular crop tool for consistent marble-like proportions.
- **Custom Arena Creator**: Image-to-3D arena generation using Meshy.ai API. Users upload arena/environment images to generate 3D battle arenas. Uses "hard" surface mode for terrain generation. Arenas persist to PostgreSQL with deviceId scoping. AI-generated arenas appear in MapSelection alongside default arenas. Game state tracks meshyArenaId and meshyArenaModelUrl for rendering in gameplay. Features circular crop tool for consistent results.
- **CircleCropTool**: Reusable UI component (client/src/components/ui/CircleCropTool.tsx) for circular image cropping. Features draggable/resizable circular overlay, image pan/zoom controls, touch support, and exports 512x512 circular PNG. Integrated into both CreateZoogi and CreateArena workflows before Meshy.ai submission.
- **My Collections**: Personal gallery for viewing all saved custom Zoogis and Arenas. Tabbed interface with 3D model previews, thumbnail display, and delete functionality. Accessible from the right-side menu via "My Collections" button. Fetches from /api/custom-zoogis and /api/custom-arenas endpoints using deviceId for user scoping. Includes default free arenas (like Zoogi Town) with a "FREE" badge that cannot be deleted.
- **Free 3D Arenas**: Built-in free arenas available to all users. Currently includes "Zoogi Town" (the main menu 3D model). Displayed in MapSelection under "Free 3D Arenas" section and in My Collections with a FREE badge. Uses sentinel meshyArenaId (-1) to trigger model loading.
- **Zoogi Creatures**: Five unique base characters (Wolfgang, Hotstreak, Lars, Pinpoint, Bolt) each with distinct abilities.
- **Gameplay Mechanics**: Drag-and-release launching, orb collection for points, enemy knock-out conditions, and arena-specific interactions.
- **Arenas**: Circular arenas with varying obstacles (mushrooms, snowmen, aliens, trees), speed-altering patches (ice, flowers), and environmental effects. The "Cosmic Platform" (space theme) features a Tron-style lightcycle arena model (cosmos_arena.glb) with glowing cyan destructible ring wall blocks. The "Frozen Ring" (ice theme) uses the Location 3 Winter GLB model (winter_location.glb) as a rich 3D environment with ice castles, bridges, and props surrounding the marble play area. The winter model was edited in Blender to trim outer ice floes and adjust material saturation/value. The ground plane, edge ring, danger zone, and rotating inner walls are hidden for the ice theme since the GLB model provides its own floor. Ice theme uses stylized lighting (warm sun, blue ambient, cool rim light, hemisphere) scoped only to ice maps. A gradient sky shader (bright blue top to light horizon) replaces the flat sky. Subtle atmospheric fog adds depth. Characters baked into the GLB (snowmen, penguins, etc.) have idle bobbing/swaying animations. Game orbs bob and spin. Interactive winter animal NPCs (fox, penguin, polar bear, walrus, fish) are placed around the arena with idle animations and collision-based wobble reactions. Snowfall particle effect adds atmosphere.
- **Destructible Ring Wall**: 192 physics-driven blocks arranged in a ring around each arena. Blocks scatter on Zoogi impact using Rapier physics. Theme-specific colors: cyan/glowing for space, ice blue for frozen, orange for lava, etc.
- **Multiplayer**: Includes a real-time Ringer Royale (FFA) mode and a local pass-and-play turn-based mode with player elimination and score tracking. Local multiplayer starts with player profile panel hidden by default with "Tap to expand" hint; panel state is saved and restored when switching game modes.
- **AI**: Features smart target selection, edge awareness for knock-offs, self-preservation, and power-scaled launches.
- **Physics**: Custom distance-based collision detection with no external physics library, using React refs for frame-by-frame updates. Swept collision detection for orbs and characters.
- **Audio**: Dynamic background music with intensity-based volume and tempo changes, smooth crossfading between levels.
- **Tutorial System**: On-demand interactive tutorials accessible via a help button.
- **Voice Chat**: WebRTC-based peer-to-peer voice communication for team modes, with WebSocket signaling server, mute/unmute controls, and session-based room isolation.
- **Arena Editor**: Developer mode for editing arena elements with drag controls, snap-to-grid, and export functionality. Accessed via camera menu when developerMoveMode is enabled.
- **Map Editor Persistence**: Save and load map decorations (placed 3D models and background settings) per map. Configurations persist to PostgreSQL via /api/map-decorations endpoints using deviceId scoping. Saved decorations auto-load when a map is selected in any game mode. Features Save button for persistence and Export button for downloading JSON configuration files.
- **Background Controls Panel**: Comprehensive controls for customizing arena backgrounds and 3D stage models. Includes Position (distance, height, width), Rotation, Mirror settings, Custom Image upload, and 3D Model controls (Position X/Y/Z and Scale). All arena stage models (FloatingIsland, ArabianNights, Cosmos, Meshy custom) respond to position and scale sliders. Cosmos arena physics collider rebuilds when model transforms change to maintain gameplay alignment.
- **Camera System**: Multiple camera views including Birds Eye, First Person, Over the Shoulder, and Developer Camera. Orbit controls via drag gestures. Pinch-to-zoom in Birds Eye view. Drag-to-launch is disabled in Birds Eye, First Person, and Over the Shoulder views to prevent control conflicts with camera gestures.
- **Ability Camera Effects**: Centralized camera effects controller (useCameraEffects.tsx) that allows abilities to push camera effects rather than controlling the camera directly. Features priority system (10=passive, 20=active, 25=explosions, 40=knockoffs), automatic blending, and smooth easing. Effects include zoom, tilt, shake, and timeScale. Each ability has defined camera hooks that trigger on activation.
- **Save Camera Position**: Export menu sub-option that saves the developer camera position as rotation-aware offsets (forward/height/right relative to player facing). Saved positions persist and apply when Over the Shoulder view is enabled, maintaining relative position as user orbits. Uses window globals (__ZOOGI_CAMERA__, __ZOOGI_CAMERA_TARGET__, __ZOOGI_ORBIT_ANGLE__) for cross-component access.

### Feature Specifications
- **Zoogipedia**: In-game encyclopedia with detailed lore, stats, rarity tiers, and filter options for all 16 characters (5 base + 11 premium).
- **Shop**: In-app purchase system for 11 premium characters and bundles, integrated with Stripe, featuring 3D preview modals with stats and ability descriptions.
- **Enhanced Leaderboard**: Multi-tab leaderboard with All-Time, Weekly, and Seasonal views. Shows player rank, score, Zoogi used. Seasonal leaderboards display time remaining and user's current rank.
- **Character Abilities**: Each Zoogi has unique abilities (e.g., Wolfgang's dash, Hotstreak's instant explosion, Bolt's stun pass-through).
- **Obstacles**: Mushroom, snowman, and alien obstacles repel characters. Flower patches slow characters. Ice patches accelerate characters. Trees wobble on collision.
- **Game Modes**: Ringer Royale (FFA) and Local Multiplayer (turn-based).

### Community Features (Phases 2-5)
- **Zoogi Gallery**: Community showcase for custom Zoogis. Users can share their AI-generated Zoogis, browse others' creations, and vote on favorites. Features upvote/downvote system with duplicate vote prevention.
- **Arena Showcase**: Community hub for sharing custom arenas. Users can share arena configurations, browse featured arenas, and download them for local play.
- **Clans System**: Create or join clans with unique names and tags. Clans have leaders and members. Members can view clan rosters and chat (planned).
- **Tournaments**: Browse active and upcoming tournaments. View tournament details, prizes, and leaderboards. Join tournaments to compete against other players.
- **Daily Bonuses**: 7-day streak reward system. Escalating rewards (coins, gems) encourage daily logins. Streak tracking with visual progress indicators.
- **Replay System**: Save and share match replays. Browse community replays or view personal replay library. Share replays via native share API or clipboard links.
- **Discord Integration**: Utility library for sharing scores, replays, and tournament results to Discord via webhooks. Embed builders for consistent Discord formatting.

### System Design Choices
- **State Management**: Zustand is used for its simplicity and direct state mutation capabilities.
- **Physics Handling**: Direct manipulation via refs for performance-critical physics updates, avoiding external physics libraries.
- **Collision Detection**: Implemented custom distance-based and swept collision detection.

## External Dependencies

- **PostgreSQL**: Used for persistent storage, specifically for the leaderboard system.
- **Drizzle ORM**: Object-Relational Mapper for interacting with PostgreSQL.
- **Stripe**: Integrated for in-app purchases within the Shop, handling payment processing and webhooks.
- **@react-three/fiber**: React renderer for Three.js, enabling 3D graphics.
- **@react-three/drei**: A collection of useful helpers and abstractions for @react-three/fiber.
- **Zustand**: State management library.
- **Framer Motion**: Animation library.
- **Express.js**: Backend framework for API routes and server management.
- **Tailwind CSS**: Utility-first CSS framework for styling.
- **Meshy.ai**: Image-to-3D model generation API for custom Zoogi creation. Requires MESHY_API_KEY secret.