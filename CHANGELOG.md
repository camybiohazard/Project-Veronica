# Changelog

## Unreleased

### Removed



### Changes

- **Ammo counter** (GAMEPLAY, on by default): the equipped weapon's magazine and its size, such
  as `12/15`, in the bottom right corner in the game's font. Red when empty.
- **Weapon shortcuts** (GAMEPLAY, off by default): 1 / 2 or the d-pad's left / right switch to the
  previous / next weapon in the bag, without the inventory. In play the d-pad's left / right no
  longer turn (the left stick does), up / down still walk; menus and aiming keep the whole d-pad.
  L2 / R2 moved from 1 / 2 to 9 / 0.
- **Move while aiming** (THIRD PERSON, Modern gamepad, on by default): the left stick walks slowly
  any way while aiming and firing, the legs walking (or stepping back) under the aim.
- **Fixed camera controls** start as Classic (tank), as the original; Modern (camera-relative) is
  still in CONTROLS. A `recvx.ini` that has a choice keeps it.
- **Fog in third person**: the third-person camera uses 75% of Fog intensity (100% there is 75%):
  close behind Claire the full fog looked too thick in many rooms. The fixed cameras keep the value.
- ENHANCED SETTINGS -> CONTROLS shows the gamepad's bindings first (the device row switches to the
  keyboard and mouse).
- "You appear as" is saved as `appear_char` in `recvx.ini` (it was `coop_char`): choose your model
  again once after updating.
- The gamepad's left stick click is L3 again by default (it was the trade request).

### Fixes

- **Sub machine gun reload crash**: reloading the sub machine gun, with the Reload button or by
  firing on an empty magazine, no longer crashes the game. It has no reload animation, so the ammo
  goes in at once and you keep aiming.

