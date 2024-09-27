# Game Boy Emulator

This is a project I built during the summer between my freshman and sophomore years in college. It is a Game Boy emulator, which basically means it immitates the behavior of a Game Boy and executes instructions from a ROM file as though it were the actual device. After finishing the core Game Boy functionality, I also went ahead and implemented a Game Boy Color emulator, which is effectively the same thing but with a few added complications.

Overall, I think this was a great project! I learned a bunch about how fetching opcodes work (i.e. machine code instructions) and how the Game Boy CPU interacts with the PPU (Pixel Processing Unit), which turned out to be really interesting and intricate. It is not fully done in that I have yet to implement the APU (Audio Processing Unit), though that might not happen in a bit as college has now started up again for me for the fall.

## Usage

Download this repo as well as SDL2, which is the graphics library I used. SDL2 can be downloaded using these instructions: [https://wiki.libsdl.org/SDL2/Installation](url). Then, navigate to the downloaded repo and do these instructions:

```
mkdir build
cd build
cmake ..
make
```

After that, you should be able to run it (fingers crossed!). To actually play a game, you'll want to run the command `./GameBoyEmu [ROM]` for a DMG (regular Game Boy) emulator, or `./GameBoyEmu -c [ROM]` for a GBC (Game Boy Color) emulator. Note that the ROM file will have to be a path relative to the *build directory*. That means that your run command might look something like this: `./GameBoyEmu -c ../roms/Pokemon_gold.gbc` (if you had a Pokemon Gold ROM in a directory called roms).

Nintendo notoriously cares a lot about copyright, so I haven't included any ROM files in this repo. If you really want to play, it's relatively easy to find them online. Also, if you want to enable file saves, you'll want to create a `saves` folder in whatever folder you're keeping your ROMs in as that's where I put the save files.

That should be all. If you're struggling to run the emulator, feel free to create an issue on this GitHub page. I'm already guessing that this won't work out of the box for Windows users, but I guess I'll see.
