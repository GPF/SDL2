$KOS_BASE/utils/scramble/scramble SDL2dcvidTest.bin 1st_read.bin
cp 1st_read.bin cd/
#DIRS=$(find . -type d | grep 'cd/' | sed 's/^/-d /' | tr '\n' ' ')
~/code/dreamcast/mkdcdisc/builddir/mkdcdisc -n DCSDL2 -N -d cd -d cd/data -e SDL2dcvidTest.elf -o sdl2dcvid.cdi -I 
