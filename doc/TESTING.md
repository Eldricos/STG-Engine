# Testing: the playthrough

The unit tests (`ctest`) cover the formats, the script interpreter, movement
and parts of the missions. What they can't cover is the whole game played
through on real data, which is what this list is for.

## Before you start

- Run with the game's data and both discs:
  `stg --path=/games/stg --cd1=D:/ --cd2=E:/`.
- For a log to send with a report, add `-d2 --debugflags=scripts` and
  redirect the output to a file. It records every script event.
- Save often (F5 in a mission, or Save in the menu), and keep the save from
  just before anything that goes wrong.
- To get back to a point quickly: `--story=SECTION` starts the story at any
  section (`--story=list` names them), and `--combat=N` (1-25) goes
  straight into a space combat.

## What to report

For each problem: where it happened (mission, place or fight), what you did
just before, what you expected (from the original, if you know it), and a
screenshot. Add the save and the log if you have them.

## Throughout

- Transitions: the briefing waits for PROCEED, then teal speckles cross-fade
  into the mission. At the end of a mission the screen speckles away to
  black. Epsion uses yellow streaks instead.
- Text and speech line up, and subtitles follow the setting.
- The automap: cyan walls on the blue panel, north up, with the player in
  the middle.
- Lifts, doors and switches respond, and the player isn't carried off by a
  lift unless standing on the car.

## Controls

- On the Controls screen, the defaults match the original's (away: arrows,
  Num / and Num *, Enter, Ctrl, Shift, PgUp, PgDn, Home, Insert and the
  mouse buttons; starship: arrows and the keypad, Ctrl, Shift, Enter, 1-7).
- Click a box, press a key: the key moves there from any other action, the
  old key in that box stops working, and Accept keeps it after a restart.
  Cancel throws the changes away.
- Mouse Look takes only the left or right mouse button.
- In a mission, Shift runs, and holding the right button turns and tilts the
  view towards the mouse. With Lookspring on, letting go levels the view.
- Keys you've changed work in space combat too.

## Away missions

| Mission | Check in particular |
|---------|---------------------|
| AMARGOS1 | The bodies Riker beams up disappear (this failed once and couldn't be reproduced). The lifts and the door that stuck before. |
| AMARGOS2 | The helmet overlay (helm.ovr) appears over the view when worn. |
| ANTILIOS | Water: wading, swimming under the surface and climbing out, with the low gravity under water. The tinted screen effects. |
| ARVADA1 | Actors told to guard something stay near it rather than following you. |
| BERSUS | The same guards (actors 38-47) stay near what they guard. Water, as in Antilios. |
| ENTERP | The finale on the Enterprise. |
| EPSION | The SHELL overlay over the view looks right (it's drawn by a mode not fully decoded). The yellow streak transitions. |
| GALORN1, GALORN2 | The BIGCROP overlay appears over the view. |
| QURASH | The helmet overlay. |
| VERID1 | The finale with Soran. |

## Space combat

Every fight:

- It opens with the target dead ahead on the radar.
- The right panel shows the Enterprise's schematic. Clicking the CONN line
  calls up the helm menu, and the CONN line shows the current order.
- The buttons at the foot of each side panel switch between the system
  display and the shield facing display (a 3D view of the ship), with the
  computer saying which.
- The slider beside the viewscreen zooms it; the one beside the radar sets
  the radar's range.
- Holding the mouse on the viewscreen steers. Tell us if up and down feel
  the wrong way round compared with the original.
- A ship blowing up shows as a flashing marker on the radar.

Particular fights:

- **Halee (SC16):** in the original, the radar opened showing only the
  target. Here all four Birds of Prey show. If your copy of the original
  does the same, note when the others appear.
- **SC9:** this is the only fight with later waves. Note whether the
  original's arriving ships fade in, and with what sound. Here they appear
  at once.

## Bridge

- Warping, scanning, the Nexus simulation and transporting all behave as in
  the original. Story deadlines run on stardates.
- Fleets marked cloaked stay off the star map, and don't start a fight,
  until the story brings them in.
