# The-Smiler-Game-New-Nintendo-3DS-Edition
A New 3DS/2DS port of the 2013 Mobile Game
# Information

The Smiler Game: New Nintendo 3DS Edition is a Port of the 2013 Mobile Game released for iOS and Android. The game was commissioned by Alton Towers Resort and Developed by Matmi. On June 5th 2015, just 3 days after The Smiler Accident, the game was taken down from all app stores due to some of it's content. (This content included an AR Model of The Smiler car crashing through the sign). This port aims to be a faithful recreation of the game for the New 3DS/2DS line of Systems, with the majority of the features being implemented into the game.

# Features

    The gameplay itself, including the upgrades for both the track and the  legs
    Stats and achievements
    “The Ride” section of the menu, showing off the ride and the stats around it
    Settings for lean sensitivity, ride speed and lean direction, plus options to turn tutorials off, replay them, and clear save data
    The ability to unlock AR-exclusive rewards through points
    Stereoscopic 3D support
    Dual Screen Support

# Not included

    The AR camera, as most of the AR elements are gone from the park (apart from the Smiler sign)
    Spin to Win and the Book Now button on the main menu, these features relied on servers that have been offline for over 10 years. 
    Social media icons and portals, as the New 3DS can't link to both Facebook and Twitter (sorry to the five people who still use Facebook)
    Graphics options, The games textures and some meshes have already been lowered in quality to run)
    6 achievements tied to features that aren’t in the port (AR, website, social media, Spin to Win)
    *Old 3DS/2DS support, as I would have to remove a lot of background scenery and effects to get the game running at a subpar frame rate

    *I have not tested the game on Old 3DS/2DS, so I may be completely wrong, if you want to see if it runs, be my guest.

# Installation

Download either (or both) the .cia or .3dsx files, and put them on your SD card, as they are already built to run.

.3dsx goes into the '3ds' folder

.cia goes into the 'cias' folder (or anywhere on your SD card, just be sure you know where it is)

If you chose the .cia option, install it with FBI in the folder you put it in.

(The game size is around 32.28mb for .cia while the .3dsx is 31.43mb)

# Performance

As mentioned earlier, Texture Quality and Some Mesh Quality has been reduced in order to make the game run smoothly, the game runs at 51 fps with minor dips, but there might be some stuttering on first boot.

(Note: using the Stereoscopic 3D will lower FPS)
(Note 2: Emulation works good across some tests, but most were done on actual hardware)

# How it was made

The Original Smiler Game ran in Unity 3.5, all of the assets from the latest build (v1.011) were extracted using AssetRipper. The game would not run well if Unity 5.1 or 5.2, so none of the original code or engine was used. The entire game has been rebuilt from scratch as a native 3DS app, written in C with devkitPro, libctru and citro3d. The data extracted was converted into formats that the 3DS can handle, this means that models and the track became binary files, textures have been converted to the 3DS's native texture format, audio has been recoded for the 3DS's sound hardware and the games logic (the ride physics, upgrades, UI and menus have been rewritten from scratch to match how the original plays, with the UI even supporting the second screen.

# AI Usage

Yes, some AI was used during the making of this port, it was used to rewrite most of the code for the game, however, the game has been play tested multiple times on actual hardware, to make sure any bugs have been stomped out and erased, as well as to make sure that the game plays just like the original mobile version, by using side by side comparisons to the android edition.

NO AI ASSETS, MESHES OR TEXTURES WERE GENERATED FOR THE PROJECT

# Conclusion

The game is currently on V1.4 (see if you can guess that reference) this is the only version I intend to put out at the moment. If 3 ghosts ever visit me at night and tell me to make another update, then I'll consider it, but for now, this is where I get off.

# Gameplay Images: https://postimg.cc/gallery/SyFgrbM

# Credits

WinterMute - devkitPro
Alton Towers Resort and Matmi - The Smiler Game (and Alton just being a good theme park in general)
3DSGuy - MakeRom
Epicpkmn11 - bannertool
Anthropic - Claude
