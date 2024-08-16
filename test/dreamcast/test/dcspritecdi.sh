$KOS_BASE/utils/scramble/scramble SDL2dcAnimationTest.bin 1st_read.bin
cp 1st_read.bin cd/
DIRS=$(find . -type d | grep 'cd/' | sed 's/^/-d /' | tr '\n' ' ')
~/code/dreamcast/mkdcdisc/builddir/mkdcdisc -n DCSDL2 -N $DIRS -e SDL2dcAnimationTest.elf -o sdl2dcAnimation.cdi -I -N
