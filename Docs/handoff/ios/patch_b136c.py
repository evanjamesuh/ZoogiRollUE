# b136c (Evan 08:47: "Update the news every build"): NEWS entries for B127 - B136 (and from now on one per build)
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
ed1('Private/ZoogiFront.cpp', '''			{ TEXT("B126"), TEXT("REDIRECT HITS HARD"),''', '''			{ TEXT("B136"), TEXT("THE COSMOS IS WATCHING"), TEXT("On the Cosmic arena a colossal dark titan rises out of the void and watches the match: it follows the leader, its red eyes blaze on knockouts and criticals, it leans in on big REDIRECT combos, scoffs at misses and roars at the final round. The Skill Lab's strength and radius now work on every skill - Golden Apple, Apple Bombs, Wolf Pack, Bolt Balls, Ice Wall height and every delayed blast.") },
			{ TEXT("B135"), TEXT("ONE AWAKEN METER"), TEXT("The SUPER bar is now your AWAKEN meter. It fills from perfect taps, blocks, scoring, knockouts and every card you cast. Full = SHOWTIME lights up before your shot (anime cutscene + your Awaken power). Getting knocked off empties it. Crystals are REDIRECT fuel only.") },
			{ TEXT("B134"), TEXT("GOLDEN APPLE, METEOR FINALE"), TEXT("Golden Apple: the Apple of Eden forms on you, a golden apple is lobbed to the orb you tapped and a bigger blast goes off where it lands. SHOWTIME Meteor Strike: a second clip of one giant meteor, then the camera looks up to watch it hit the arena.") },
			{ TEXT("B133"), TEXT("TRAINING BOTS, SHOWTIME ANIME"), TEXT("Training pause menu: CPU OPPONENTS - tap to add up to 4 practice bots. SHOWTIME with a Meteor Strike Zoogi plays a 10 second full-screen anime first.") },
			{ TEXT("B132"), TEXT("STORY EP3 + TEMPEST"), TEXT("Story mode episode 3 'Storm Season' with its films and the new rival TEMPEST. Top banners stay clear of the round header.") },
			{ TEXT("B131"), TEXT("CAMERA + CLEAN TEXT"), TEXT("The roll camera no longer flies into the sky following flung orbs. Skill plates, meter labels, combo counter, hints and banners no longer overlap.") },
			{ TEXT("B130"), TEXT("PRISM GRENADE PICKUP"), TEXT("From round 2 the orb generator drops a PRISM GRENADE - the first Zoogi to touch it takes it. ITEM - throw it twice the arc range for a shockwave where it lands. New grenade art. Cosmic markers are a deeper purple.") },
			{ TEXT("B129"), TEXT("WOLF TARGETS, COSMIC MARKERS"), TEXT("Pick a different target for each wolf before they run. Recalled wolves no longer push you off. Cosmic markers rise purple without smoke from round 2 and grow every round.") },
			{ TEXT("B128"), TEXT("BOLTS FIRST, NO SHOWTIME GLOW"), TEXT("Use up all your bolts or wolves before the next COMBO cast (CPUs too). Bolt Balls hit orbs right next to you. No pulsing glow on the orbs during SHOWTIME.") },
			{ TEXT("B127"), TEXT("NEWS SCROLLS"), TEXT("This news page scrolls and lists every build.") },
			{ TEXT("B126"), TEXT("REDIRECT HITS HARD"),''')
print('b136c ok')
