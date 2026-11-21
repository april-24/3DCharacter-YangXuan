# YangXuan 3D Character

YangXuan is an interactive 3D character prototype created for a fictional sequel to *Ne Zha 2*. Inspired by the Chinese mythological warrior Yang Jian, the project combines a fully articulated character, traditional temple environment, weapons, combat animations, textures, lighting, and camera controls.

The application is written in C++ for Windows using the Win32 API, OpenGL, and GLU.

## Features

- Articulated head, arms, forearms, wrists, fingers, legs, knees, and feet.
- Character walking, turning, collision boundaries, and an optional earthquake effect.
- Mouse and keyboard camera rotation, panning, zooming, and projection switching.
- Spear animations including divine fire, throwing, heavenly rain, and forward thrust.
- Animated bell with summoning, hovering, attack, shockwave, return, and dismissal states.
- BMP texture mapping for the character, weapons, temple, ground, and sky.
- Multiple lighting modes, movable lighting, and morning, afternoon, and evening scenes.
- Interactive temple doors and texture toggling.

## Run the Prebuilt Application

This build runs on Windows.

1. Download or clone the repository.
2. Open `x64/Debug`.
3. Keep the `Textures` folder beside `YangXuan.exe`.
4. Double-click `YangXuan.exe`.

## Build from Source

### Requirements

- Windows 10 or later
- Visual Studio 2022
- Desktop development with C++ workload
- Windows 10 or 11 SDK

### Steps

1. Open `YangXuan.sln` in Visual Studio.
2. Select the `x64` platform and `Debug` configuration.
3. Build the solution.
4. Confirm that `x64/Debug/Textures` is beside the generated executable.
5. Run the project from Visual Studio or open `x64/Debug/YangXuan.exe`.

OpenGL32 and GLU32 are linked through the Windows SDK by the project source.

## Controls

### Camera

| Input | Action |
| --- | --- |
| Left-click and drag | Rotate the camera |
| Arrow keys | Pan the camera |
| Shift + Arrow keys | Rotate the camera by keyboard |
| Mouse wheel | Zoom in or out |
| `P` | Switch between perspective and orthographic projection |

### Character Movement

| Input | Action |
| --- | --- |
| `W` / `S` | Walk forward / backward |
| `A` / `D` | Strafe left / right |
| `Q` / `E` | Rotate left / right |

### Joint Controls

| Body part | Input |
| --- | --- |
| Head | `Z`, `X`, `Shift+Z`, `Shift+X` |
| Left arm and forearm | `T`, `G`, `Shift+T`, `Shift+G`, `[`, `]` |
| Right arm and forearm | `Y`, `H`, `Shift+Y`, `Shift+H`, `Shift+[`, `Shift+]` |
| Wrists | `-`, `=`, `,`, `.`, with `Shift` for yaw controls |
| Fingers | `F` to blend between an open hand and a fist |
| Left leg and knee | `U`, `J`, `Shift+U`, `Shift+J` |
| Right leg and knee | `I`, `K`, `Shift+I`, `Shift+K` |
| Feet | `N`, `M`, `Shift+N`, `Shift+M` |

### Weapons and Effects

| Input | Action |
| --- | --- |
| `F1` | Show or hide the spear |
| `F2` | Toggle divine fire |
| `F3` | Throw the spear and trigger heavenly rain |
| `F4` | Perform a forward spear thrust |
| `F5` | Toggle automatic arm movement |
| `F6` | Summon the bell |
| `F7` | Launch the bell attack |
| `F8` | Dismiss the bell |

### Environment

| Input | Action |
| --- | --- |
| `Enter` | Open or close the temple doors |
| `1` | Toggle the walking earthquake effect |
| `2` | Cycle the time of day |
| `3` | Toggle textures |
| `4` | Cycle lighting modes |
| `5` / `6` | Rotate the light clockwise / anticlockwise |
| `7` / `8` | Move the light upward / downward |
| `R` | Reset the scene |
| `Esc` | Exit the application |

## Project Structure

```text
YangXuan-3D-Character/
|-- YangXuan.sln
|-- YangXuan/
|   |-- main.cpp
|   |-- YangXuan.vcxproj
|   `-- YangXuan.vcxproj.filters
`-- x64/Debug/
    |-- YangXuan.exe
    `-- Textures/
```

