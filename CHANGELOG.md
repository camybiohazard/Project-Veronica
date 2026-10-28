# Changelog

## Unreleased

### Removed



### Changes

- **Ammo counter** (GAMEPLAY, on by default): the equipped weapon's magazine and its size, such
  as `12/15`, in the bottom right corner in the game's font. Red when empty.
- "You appear as" is saved as `appear_char` in `recvx.ini` (it was `coop_char`): choose your model
  again once after updating.
- The gamepad's left stick click is L3 again by default (it was the trade request).

### Fixes

- **Sub machine gun reload crash**: reloading the sub machine gun, with the Reload button or by
  firing on an empty magazine, no longer crashes the game. It has no reload animation, so the ammo
  goes in at once and you keep aiming.

