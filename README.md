# Romeo Is a Dead Man – Ultrawide Semi Fix

Ultrawide fix for *Romeo Is a Dead Man*.

## How it works?
- On every engine tick apply constraint adjustments to all UUserWidgets

## How to compile (UI "fixes" only)
- Dump the sdk using Dumper-7 and paste it into the project directory
- Open .slnx file using Visual Studio (downgrade vcxproj or use preview version of VS)
- Build!
- Rename the DLL to anything you want with .asi extension
- Place the .asi file next to the game executable (also make sure you have a loader like winmm.dll present in the Binaries folder)
- Done (if it doesn't compile with static_assert errors make sure you're compiling x64 version, if the error is single just comment the problematic line, lol)

## Installation
1. Download the latest release
2. Extract the files
3. Copy the contents to the game installation folder
4. Launch the game

## Why?
The main, the parent repo worked but cutscenes were stretched, UI was very broken and unplayable to me.
So I had the idea (had experience with UE4/5 and general Reverse Engineering) to create this thing. Just to play.
Bugs are expected ¯\_(ツ)_/¯

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

and me😝

## What was changed
- Folder structure adapted for *Romeo Is a Dead Man*
- No changes were made to the original patch logic

## Installation
1. Download the latest release or ZIP
2. Extract the files
3. Copy the contents to the game installation folder
4. Launch the game

## Notes
- Unofficial community fix
- May cause issues in cutscenes
- Tested on 32:9 resolutions
- The fix is not fully complete.
- Some HUD/UI elements may appear misaligned or stretched in ultrawide resolutions.
- This project is not maintained, I'm only accepting pull requests
- Thank you Lyall for your work :D 

## License
MIT License