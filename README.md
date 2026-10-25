# 3DS Retro Arcade

**Three arcade clones games from the 80s in one Nintendo 3DS homebrew collection.**

RetroArcade brings three classic arcade games from the 80s to the Nintendo 3DS. Choose a game on the touchscreen, chase a new high score and return to the arcade menu whenever you want.

![RetroArcade program icon](icon.png)

## Games

### Delta Race

Delta Race puts you in control of a spacecraft inside a combat arena. Rotate, thrust and use your momentum to survive waves of enemy machines. Features include synthesized sound effects, a rhythmic background loop and an animated enemy scoring guide.

### Sky Shield

Sky Shield challenges you to defend six cities using three missile batteries. Incoming missiles leave trails across the upper screen; aim and fire using the touchscreen or physical controls.

Cities have three damage stages: a protected skyline, an unshielded skyline and a ruin. At the end of each wave, an animated tally awards points for unused interceptors and surviving cities.

### Rescue Wing

Rescue Wing is a side-scrolling helicopter rescue game. Pick up stranded people and bring them back to the base while avoiding enemy fire. The Huey-inspired helicopter features animated rotors, turns through a front-facing position and banks while steering.

Carry up to four passengers at a time. Land gently near people to board them, then return to the base on the left to unload.

## Installation

You need a Nintendo 3DS with a working Homebrew Launcher setup.

Copy the built files to your SD card:

```text
/3ds/RetroArcade/RetroArcade.3dsx
/3ds/RetroArcade/RetroArcade.smdh
```

Launch **RetroArcade** from the Homebrew Launcher. Graphics and sounds are embedded in the application; no separate asset folder is required on the SD card.

## Controls

### General

| Input | Action |
| --- | --- |
| Touch a game icon | Select a game |
| D-pad Left / Right | Select a game in the menu |
| A | Start the selected game or restart from its high-score screen |
| SELECT | Return to the game selection menu |
| START | Open the exit confirmation |
| A / B in the exit dialog | Confirm / cancel exit |

Delta Race displays its introduction before the start option becomes available. Release the start button or touchscreen contact before firing.

### Delta Race

| Input | Action |
| --- | --- |
| Circle Pad / D-pad Left and Right | Rotate |
| B / L / D-pad Up | Thrust |
| A / R | Fire |
| X / D-pad Down | Brake |
| Touchscreen buttons | Thrust / fire |

### Sky Shield

| Input | Action |
| --- | --- |
| Circle Pad / D-pad | Move the targeting crosshair |
| Y / A / B | Fire from the left / center / right battery |
| R | Fire from the center battery |
| Touch the aiming area | Aim and fire |
| Touch an ammunition button | Fire from that battery |

Incoming threats appear only on the upper screen.

### Rescue Wing

| Input | Action |
| --- | --- |
| Circle Pad / D-pad | Fly |
| A / R | Fire |
| Land gently near people | Pick up passengers |
| Land at the left base | Unload passengers |

## High scores

Each game keeps its own top ten. Qualifying scores use three-character initials: uppercase letters, numbers and spaces. Scores are saved after confirming the entry and loaded when the application starts.

The application creates its save directory automatically:

```text
/3ds/RetroArcade/scores-v2.txt
```

The save system keeps a backup and can recover from a complete temporary file or backup if the main file is missing or invalid. Keep your score files when updating the application.

If saving fails, the screen displays the operation and an error number. Press **Y** in the menu or high-score screen to retry.

## Building

Install **devkitPro**, including **devkitARM**, **libctru** and the 3DS build tools. With the development environment available, run this command from the project directory:

```sh
make -j4
```

The build creates `RetroArcade.3dsx`, `RetroArcade.smdh` and `RetroArcade.elf`.

The Makefile defaults to `/opt/devkitpro`. Set `DEVKITPRO` and `DEVKITARM` if your installation uses a different location.

Python 3 and Pillow are needed only when regenerating image assets with the scripts in `tools/`.

## Tests

Run the native test suite on your development computer:

```sh
./tests/run.sh
```

Tests cover gameplay, application flow, input handling, high scores, save recovery and audio synthesis. A native C compiler with UndefinedBehaviorSanitizer support is required.

Generated test programs, screenshots and temporary save files go into `build-host/`, which is excluded from Git. Native tests complement testing on an actual Nintendo 3DS.

## Project layout

| Directory | Contents |
| --- | --- |
| `source/` | Game logic, rendering, input, audio and high scores |
| `assets/` | Artwork, reference assets and previews |
| `data/` | Binary assets embedded during the build |
| `tests/` | Native tests and 3DS hardware stubs |
| `tools/` | Asset preparation scripts |
| `build-host/` | Generated local test output |

## Credits and license

Created by **dotMizi**. Code is available under the [MIT License](LICENSE).

