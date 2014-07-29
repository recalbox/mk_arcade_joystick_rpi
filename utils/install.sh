#!/bin/sh

if [ "$1" = "updatesystem" ]
then
	sudo apt-get update
	sudo apt-get upgrade -y
	sudo rpi-update
	echo "Please reboot if the message above asks for it"
else
	echo "Installing required dependencies"
	sudo apt-get install -y --force-yes dkms cpp-4.7 gcc-4.7 git joystick
	echo "Downloading current kernel headers"
	wget http://www.niksula.hut.fi/~mhiienka/Rpi/linux-headers-rpi/linux-headers-`uname -r`_`uname -r`-2_armhf.deb
	echo "Installing current kernel headers"
	sudo dpkg -i linux-headers-`uname -r`_`uname -r`-2_armhf.deb
	rm linux-headers-`uname -r`_`uname -r`-2_armhf.deb
	echo "Downloading mk_arcade_joystick_rpi 0.1.1"
	wget https://github.com/digitalLumberjack/mk_arcade_joystick_rpi/releases/download/0.1.1/mk-arcade-joystick-rpi-0.1.1.deb
	echo "Installing mk_arcade_joystick_rpi 0.1.1"
	sudo dpkg -i mk-arcade-joystick-rpi-0.1.1.deb
	if [ "$?" -eq "0" ] 
	then
		echo "Installation OK"
		echo "Load the module with 'sudo modprobe mk_arcade_joystick_rpi map=1' for 1 joystick"
		echo "or with 'sudo modprobe mk_arcade_joystick_rpi map=1,2' for 2 joysticks"
		echo "See https://github.com/digitalLumberjack/mk_arcade_joystick_rpi#loading-the-driver for more details"
	else
		echo "Something went wrong..."
	fi
fi

