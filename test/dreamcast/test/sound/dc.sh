sh-elf-objcopy -R .stack -O binary SDL2dcSoundTest.elf SDL2dcSoundTest.bin
$KOS_BASE/utils/scramble/scramble SDL2dcSoundTest.bin 1st_read.bin
cp 1st_read.bin cd/
~/code/dreamcast/mkdcdisc/builddir/mkdcdisc -n DCVice -N -d cd -d cd/data -e SDL2dcSoundTest.elf -o SDL2dcSoundTest.cdi -I -N

