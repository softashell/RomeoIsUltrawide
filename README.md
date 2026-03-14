# Romeo Is a Dead Man – Ultrawide Semi Fix

Ultrawide fix for *Romeo Is a Dead Man* ver 1.4.2 only!

## How it works?
- On every engine tick apply, constraint adjustments to all UUserWidgets and Cameras in the game

## How to compile (UI "fixes" only)
- Dump the SDK using Dumper-7 and paste it into the project directory
- Throw a MinHook libMinHook.x64.lib file into "RIADMW_SomeUIFixes\lib"
- Open .slnx file using Visual Studio (downgrade the vcxproj or use preview version of VS)
- Build!
- Rename the DLL to anything you want with the .asi extension
- Place the .asi file next to the game executable (also make, sure you have a loader like winmm.dll present in the Binaries folder)
- Done (if it doesn't compile with static_assert errors, make sure you're compiling x64 version, if the error is a single one - just comment the problematic line, lol)

## Installation
1. Download the [latest release](https://github.com/weespin/Romeo-Is-a-Dead-Man-Ultrawide-Fix/releases/tag/0.0.1 "latest release")
2. Extract the files
3. Copy the contents to the game installation folder
4. Launch the game

## Why?
The main (parent) repo worked but cutscenes were stretched, UI was very broken and unplayable to me.

So I had the idea (I have experience with UE4/5 and general reverse Engineering) to create this thing.

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

Thanks to Lyall and eu-tagami for releasing the initial version and bringing the idea to fix something — and to me 😝
