
# Simple DirectMedia Layer (SDL) Version 2.0

https://www.libsdl.org/

Simple DirectMedia Layer is a cross-platform development library designed
to provide low level access to audio, keyboard, mouse, joystick, and graphics
hardware via OpenGL and Direct3D. It is used by video playback software,
emulators, and popular games including Valve's award winning catalog
and many Humble Bundle games.

More extensive documentation is available in the docs directory, starting
with README.md

Enjoy!

Sam Lantinga (slouken@libsdl.org)


Very, very  limited SDL2 port to the dreamcast :)
to build
cd build-scripts
./dreamcast.sh

to build the dreamcast test binary
cd test/dreamcast/test
make -f Makefile.dc

![Screenshot 2024-08-14 162634](https://github.com/user-attachments/assets/bb984048-3754-403b-9a9a-4adf6f32b0a9)

added joystick support and a sdl2 sprite sheet animation dreamcast test.

https://github.com/user-attachments/assets/b92223ca-1fb6-4827-a6c6-622c84ae65ac


ported the nehe06 SDL tutorial example to SDL2 opengl(GLdc), to test the opengl driver.

https://github.com/user-attachments/assets/54ae20e1-4dd2-4fd0-bb2b-8bf3fa0b2bff

![Screenshot 2024-08-16 220329](https://github.com/user-attachments/assets/31753f7b-ce28-4c31-be64-3e9a59ebc1d6)

