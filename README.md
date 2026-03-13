# Romeo Is a Dead Man – Ultrawide Semi Fix

Ultrawide fix for *Romeo Is a Dead Man*.

## How it works?
- On every engine tick apply constraint adjustments to all UUserWidgets for UI fixes
- General ultrawide support through SUWSF - Somewhat Universal Widescreen Fix

## How to compile (UI "fixes" only)
- Dump the SDK using Dumper-7 and paste it into the project directory
- Open .slnx file using Visual Studio (downgrade vcxproj or use preview version of VS)
- Build!
- Rename the DLL to anything you want with .asi extension
- Place the .asi file next to the game executable (also make sure you have a loader like winmm.dll present in the Binaries folder)
- Done (if it doesn't compile with static_assert errors make sure you're compiling x64 version, if the error is single just comment the problematic line, lol)

## Installation
1. Download the [latest release](https://github.com/weespin/Romeo-Is-a-Dead-Man-Ultrawide-Fix/releases/tag/0.0.1 "latest release")
2. Extract the files
3. Copy the contents to the game installation folder
4. Launch the game

## Why?
The main, the parent repo worked but cutscenes were stretched, UI was very broken and unplayable to me.

So I had the idea (had experience with UE4/5 and general Reverse Engineering) to create this thing.

Just to play.

Bugs are expected 🤷‍♂️

### Screenshots
| Before | After |
|:--:|:--:|
| ![Ingame-Before](https://weesp.in/i/678b64acdcde8865.jpg) | ![Ingame-After](https://weesp.in/i/4af4abf4d6842d3e.jpg) |
| ![Menu-Before](https://weesp.in/i/1a916e36c3c7cbcb.png) | ![Menu-After](https://weesp.in/i/26b0d72e68cd47c4.png) |
| ![Cutscenes-Before](https://weesp.in/i/e45987d8e9753b94.png) | ![Cutscenes-After](https://weesp.in/i/088c9306f874706b.jpg) |
| ![DamageNumbers-Before](https://weesp.in/i/ff78a032058b43dc.jpg) | ![DamageNumbers-After](https://weesp.in/i/847d1a0bcf97a879.jpg) |

## Credits
This fix is based on **Lyall's UltrawidePatches** project.
https://codeberg.org/Lyall/UltrawidePatches
Base patch:
Crime Boss: Rockay City

Lyall, eu-tagami for releasing the initial version and me😝
