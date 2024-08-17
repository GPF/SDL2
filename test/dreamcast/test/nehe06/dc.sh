$KOS_BASE/utils/scramble/scramble SDL2dcNehe06.bin 1st_read.bin
cp 1st_read.bin cd/
DIRS=$(find . -type d | grep 'cd/' | sed 's/^/-d /' | tr '\n' ' ')
~/code/dreamcast/mkdcdisc/builddir/mkdcdisc -n DCSDL2 -N $DIRS -e SDL2dcNehe06.elf -o sdl2dcNehe06.cdi -I -N
