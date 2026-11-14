# Zoogi Roll SFX credits (Oct 4, 2026)

All 20 anime-style SFX below are **original**, synthesized from scratch by `Scripts/synth_sfx.py` (Python, numpy/scipy:
oscillators, filtered noise, envelopes, a synthetic noise-burst reverb). No third-party samples and no CC0 packs were used,
so there is nothing to attribute. They are project-owned and can be shipped and changed freely.
Source WAVs: `SourceArt/Audio/SFX/*.wav` (48 kHz, 16-bit mono). Imported as `/Game/Audio/S_<Name>` by `Scripts/import_sfx.py`.

Design brief: the movie team's `TEAM-NOTES-fight-and-victory.md` (3-6 stacked layers per hit, weight in a soft low end with one small
bright layer, sound on the impact-flash frame, duck music 3-6 dB rather than muting).

Style reference only: Evan's "naruto video examples" (official Naruto / Boruto anime clips). **No audio from those clips is
cut, extracted, sampled, or shipped.** The movie team's *measurements* of them (transient timing, attack/decay, spectral
centroid, low-frequency share, the beat of silence before big hits) guided the envelopes and the timing.

| Asset | Used for | How it was made |
|---|---|---|
| S_Dash | launch whoosh / dash | band-pass noise sweeping 450 Hz -> 3.8 kHz, 230 ms rise, faint rising tone |
| S_HitCrack | Zoogi-vs-Zoogi impact (volume scales with impact) | soft low thump 180 -> 70 Hz + mid "boing" + squish noise + mid crack + tiny 3.1 kHz tick, soft clip, small room |
| S_CritBoom | crit / KO / combo (+ ~4.4 dB music duck) | on the flash frame: distorted low boom + sub drop 110 -> 32 Hz + mid punch + small crack + crackle accent + long low rumble |
| S_Inhale | power-up wind-up before a launch | breath-in noise swell (low-pass opening 300 Hz -> 3 kHz) + rising tone, hard cut, 0.11 s silence |
| S_AwakenCharge | Infuse (Awaken) | low 55/110 Hz charge hum under a detuned saw chord sweeping 200 -> 760 Hz with accelerating tremolo; sparkle + two-note chime + low boom at release |
| S_GuardClang | Guard / guard block | soft low "whomp" + 120 Hz thud under six inharmonic metal partials (quiet top partials) + click |
| S_RockCrack | rock cracks | dry knock + gritty crackle burst + pebble ticks |
| S_RockShatter | rock shatters | low thump + broadband crunch + thinning debris rattle (~1.6 s) |
| S_StingEmber | Ember powered hit / infuse | low-passed "whoomp" + falling roar + ember crackle |
| S_StingStorm | Storm powered hit / infuse | jittery ~95 Hz square buzz + arcing crackle + snap |
| S_StingFrost | Frost powered hit / infuse | soft low thud + random glassy partials 2-6 kHz + tinkle crackle + light hiss |
| S_StingTitan | Titan powered hit / infuse | 40-90 Hz thump + low-passed crunch + ground rumble |
| S_StingVoid | Void powered hit / infuse | reverse swell into a falling sine 420 -> 55 Hz through a flanger + low drone |
| S_ClimaxRumble | the one climax per match (match-winning KO) | ~2.2 s deep rumble (low-passed noise + 38/57 Hz sines, throb that quickens, faint crackle creeping in) then 0.3 s silence; CritBoom lands right after with the impact flash |
| S_CrowdWave1 / S_CrowdWave2 | victory screen, wave 1 then the denser wave 2 (~3 s later) | synthetic crowd texture: hundreds of short buzzy "cheer" grains (sawtooth + breath noise through two vowel formants, rising glides), filtered-noise claps and a murmur bed; grains get denser over time while each gets quieter (1/sqrt(density)), so it builds by density, not loudness. No recorded or synthesized speech |
| S_RollLoop | per-Zoogi roll loop (looping) | seamless 2 s loop: brown-ish noise < 200 Hz + soft 200-700 Hz texture with an 11 Hz grain; runtime volume/pitch follow speed, heavier Zoogis deeper |
| S_UiClick / S_UiConfirm / S_UiCancel | menu and Awaken buttons | soft 1.4 kHz tick with a 260 Hz body / two rising sine notes E5-B5 / two falling notes C5-G4, low-passed |

Loudness: each file normalized to about -16 LUFS max momentary (EBU R128), true peak <= -1 dBTP (measured with ffmpeg ebur128).
Very short files are true-peak limited first and end up quieter (UiClick -24.7, UiConfirm/UiCancel about -17.5 LUFS). Runtime
volumes keep the crowd (0.45-0.5), roll loop (<= 0.3) and UI sounds (0.4-0.55) under the hits and the music.

## Nov 1, 2026 (b59) - realistic layer

Evan: "Time to add the most realistic sounds for everything." `ZoogiUtil::PlaySfx` now maps most names to realistic sounds first
(table in `Source/ZoogiRoll/Private/ZoogiTypes.cpp`, `-ZoogiSynthSfx` = the synth set above only):

- `/Game/Audio/Sounds/Real/SW_*` - **original** physically modelled impacts made by `Scripts/synth_real_sfx.py` (numpy modal
  synthesis: contact click + sphere / glass partials + body thump + small room): BallClack x6 (Zoogi on Zoogi), OrbClink x6 (glass orbs),
  WallKnock x5, Land x3, RollLoop_Real (rolling rumble loop). Project-owned. Source WAVs: `SourceArt/Audio/Real`.
- `/Game/Audio/Sounds/{Impacts,SFX,UI,Explosions,Foley_Misc}` - sound waves from **Epic's Lyra Starter Game** sample (C:\dev\Lyra56),
  free Epic content licensed for use in Unreal Engine projects: grenade punch blasts (crit / explosion / ring-out), melee impacts and
  whooshes, jump-pad whooshes (launch / ARC), dash, plaster + glass debris (rocks), pickups (score), victory sting, weapon-swap
  foley (arm), player-spawn (Aurex transform), foley steps (edge hang). Unused Lyra waves were moved to C:\dev\_to_delete\lyra_unused_b59.
- Suno (Evan's request) was blocked on Nov 1: the account's subscription renewal failed, so no Suno sounds yet.
