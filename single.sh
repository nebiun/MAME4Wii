#!/bin/bash

size=/c/devkitPro/devkitPPC/bin/powerpc-eabi-size.exe
prj=powerpc-eabi-mame4wii
obj=obj/wii/${prj}

build="MAME4Wii_Single"
make_par="-D__SINGLE__ -D__WII_BUILD__=0"

echo ${build}
rm -fr ${obj}/libosd.a ${obj}/osd
rm -f ${obj}/version.o ${obj}/mame/4wii.o ${build}.elf ${build}.dol
make GAME_LIST="${make_par}"
if [[ $? -eq 0 ]]
then
	mv ${prj}.elf ${build}.elf
	mv ${prj}.dol ${build}.dol
	ls -l ${build}.dol
	$size  ${build}.elf
fi

