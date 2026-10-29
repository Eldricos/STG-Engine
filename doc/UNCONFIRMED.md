# Unconfirmed items

Behaviour that was worked out from the original's code but couldn't be
confirmed, or where the code left a choice open. Each entry says what the
engine does now and what would settle it. Deliberate differences are in
[KNOWN_DIFFERENCES.md](KNOWN_DIFFERENCES.md).

## Needs material from the original

- **Map marker styles.** SET_ACTOR_MAP_MARKER (0x40) picks a marker for the
  automap from `map\dot-*.bmp`. Some styles have never been seen on screen
  in the original, so whether they're drawn right isn't known. *Settles
  it:* screenshots of the original's map showing those markers.
- **Other game versions.** Only the 2-CD English release has been seen.
  *Settles it:* `stg --dump-files --path=...` output from another release.

## Space combat

- **The opening radar.** In the playthrough video, Halee's fight (SC16)
  opens with only the target on the radar, though the mission file has all
  four Birds of Prey within range. Here all four show. Nothing found in the
  code explains it: no ship cloaks at the start, and the fleet's
  `Cloaked=TRUE` in UNIVERSE.INI was only traced to the star map. *Settles
  it:* more footage of fight openings, or further tracing of the radar
  (`0x4AB102`) and of how a fleet's fight is set up.
- **Later waves fading in (SC9).** The original makes later-wave ships fully
  cloaked and uncloaks them, with a sound, when their wave comes in
  (`0x4A2FA9`). But cloaking and uncloaking only act on ships with flag
  0x2000, and nothing in the code was seen setting it. So either the ships
  fade in, or (if the flag is never set) they would never appear. Here they
  appear at once. *Settles it:* footage of SC9.
- **Steering by holding the mouse on the viewscreen.** The original
  measures the mouse from the middle of the viewscreen (`0x4AEFD4`), but
  which way that turns depends on how its joystick values are centred,
  which wasn't traced. Here left of the middle turns left and above it
  dives, as the arrow keys do. *Settles it:* trying it in the original.
- **What "dim" means on the schematics.** See
  [KNOWN_DIFFERENCES.md](KNOWN_DIFFERENCES.md). *Settles it:* comparing a
  damaged ship's panel with the original's.
- **How see-through the shields are.** The shield materials carry a
  parameter (4096 for the hit dome, 11000 for the spheres) whose meaning
  wasn't traced into the DRE library's drawing. Here both are mixed half and
  half with what's behind, unlit. *Settles it:* a screenshot of the
  original's shield facing display or of a shield hit up close.
- **Where a shield hit shows.** As traced, the flash sits twice the ship's
  length out from its middle, towards the shot, and a little dome for any
  ship but the Enterprise. That seems far out; it's built as read.
  *Settles it:* footage of a hit on a Bird of Prey up close.
- **The streaming stars' speed.** The original grows them once a frame, and
  its frame rate isn't known. Twenty frames a second is assumed.

## Away missions

- **View overlay mode 2.** SHOW_VIEW_OVERLAY's mode 2 (used only for
  Epsion's SHELL.OVR) draws row by row in the original (`0x409DDC`), and
  that routine wasn't fully decoded. Here it's drawn as mode 1 (centred),
  which should look the same for a full-width picture. *Settles it:* seeing
  the shell in Epsion during the playthrough.
- **The body in AMARGOS1.** Once, a body stayed in place after Riker asked
  for it to be beamed up. It couldn't be reproduced. *Settles it:* the
  playthrough.

- **Which way Mouse Look turns.** The original's heading and pitch signs
  weren't traced through to the screen. Here the view turns towards the
  mouse: left of the middle turns left, above it looks up. *Settles it:*
  holding the right button in the original.

## Bridge

- **Fleets marked `Cloaked`.** Here this keeps the fleet off the star map,
  and from starting a fight, until the story brings it in. Whether the
  original also carries it into the fight (see the opening radar above)
  isn't known.
