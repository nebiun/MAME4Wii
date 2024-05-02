#!/bin/bash
# SETA e TAITO_SEIBU devono stare insieme perche` condividono driver

size=/c/devkitPro/devkitPPC/bin/powerpc-eabi-size.exe
prj=powerpc-eabi-mame4wii
obj=obj/wii/${prj}
hbc_src="src/hbc"
hbc_home="HBC"
hbc_apps="${hbc_home}/apps/mame4wii"
hbc_mame="${hbc_home}/mame/"
hbc_mamelibs="${hbc_mame}/libs"
hbc_mamesnap="${hbc_mame}/snap"
hbc_mamecrash="${hbc_mame}/crash"
hbc_mamelogs="${hbc_mame}/logs"
hbc_mameroms="${hbc_mame}/roms"
m4w_version="1.35.2"
elf_home="ELF"
cwd=$(pwd)

rm -fr ${obj}/libosd.a ${obj}/osd
if [[ "$1" == "--force" ]]
then
	rm -fr ${obj}
	shift
fi

mkdir -p ${hbc_home} ${elf_home}

if [[ "${1}" == "" ]]
then
	source ./liste.env
	lista_totale="${lista_taito} ${lista_sega} ${lista_konami} ${lista_neogeo}"

	mkdir -p ${hbc_mamelibs}
	
	rm -f ${hbc_mamelibs}/*
	rm -f ${elf_home}/*

	echo "Make LIBS"
	for ll in ${lista_totale}
	do
		build="MAME4Wii${ll}"
		lista="${ll}"

		make_par=""
		for l in ${lista}; do
			if [[ ${l:0:1} == "#" ]]; then
				echo "skip ${l:1}"
			else
				make_par="${make_par} -D${l}"
			fi
		done
		rm -f ${obj}/version.o ${obj}/mame/4wii.o ${build}.elf ${build}.dol

		make GAME_LIST="${make_par}" WII_VERSION="${m4w_version}" WII_BUILD="${build}"
		if [[ $? -eq 0 ]]
		then
			mkdir -p ${elf_home}
			mv ${prj}.elf ${build}.elf
			mv ${prj}.dol ${hbc_mamelibs}/${build}.dol
			$size  ${build}.elf
			mv -f ${build}.elf ${elf_home}/.
		else
			echo "Failed make ${build}"
			exit 1
		fi
	done
fi

# MENU
echo "Make MENU"
mkdir -p ${hbc_apps}
rm -f ${hbc_apps}/* 
cd src/menu
make clean
make
if [[ $? -eq 0 ]]
then
	cp -f menu.dol ${cwd}/${hbc_apps}/boot.dol
fi
cd ${cwd}

# LOADER
echo "Make LOADER"
rm -f ${hbc_mamelibs}/loader.dol
cd src/loader
make clean
make
if [[ $? -eq 0 ]]
then
	cp -f loader.dol ${cwd}/${hbc_mamelibs}/loader.dol
fi
cd ${cwd}


if [[ "${1}" == "" ]] || [[ "${1}" == "hbc" ]]
then
	# Make homebrew app
	echo "Make homebrew app"
	now=$(date +%Y%m%d)

	echo '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'  > ${hbc_apps}/meta.xml
	echo '<app version="'${m4w_version}'">'							>> ${hbc_apps}/meta.xml
	echo '	<name>MAME4Wii</name>'									>> ${hbc_apps}/meta.xml
	echo '	<coder>Nebiun</coder>'									>> ${hbc_apps}/meta.xml
	echo '	<version>'${m4w_version}'(MAME 0.135s)</version>'		>> ${hbc_apps}/meta.xml
	echo '	<release_date>'${now}'</release_date>'					>> ${hbc_apps}/meta.xml
	echo '	<short_description>Arcade Emulation</short_description>'>> ${hbc_apps}/meta.xml
	echo '	<long_description>MAME for Wii based on WII Mame 1.0</long_description>'>> ${hbc_apps}/meta.xml
	echo '	<no_ios_reload/>'										>> ${hbc_apps}/meta.xml
	echo '</app>'													>> ${hbc_apps}/meta.xml

	# IMAGE & INI
	cp -f ${hbc_src}/mame4wii.png ${hbc_apps}/.
	cp -f ${hbc_src}/mame.ini ${hbc_mame}/.

	# DIRECTORES
	mkdir -p ${hbc_mamesnap}
	mkdir -p ${hbc_mamelogs} 
	mkdir -p ${hbc_mamecrash}
	mkdir -p ${hbc_mameroms}

	# SNAPSHOT
	rm -f ${hbc_mamesnap}/*
	cp -f ${hbc_src}/nosnap.png ${hbc_mamesnap}/.

	# BUILD APP ARCHIVE
	cd ${hbc_home}
	rm -f mame4wii-${m4w_version}.zip
	zip -r mame4wii-${m4w_version}.zip apps mame
	cd ${cwd}
	
	cp -f mame4wii_snap/* ${hbc_mamesnap}/.
fi
