# mk_arcade_joystick_rpi #

The Raspberry Pi GPIO Joystick Driver

The mk_arcade_joystick_rpi is fully integrated in the **recalbox** distribution : see http://www.recalbox.com

This version bring the support of an 9th button for each player but lack of the MCP23017 support. See the [master branch](https://github.com/digitalLumberjack/mk_arcade_joystick_rpi/tree/master)


## Introduction ##

The RaspberryPi is an amazing tool I discovered a month ago. The RetroPie project made me want to build my own Arcade Cabinet with simple arcade buttons and joysticks.

So i started to wire my joysticks and buttons to my raspberry pi, and I wrote the first half of this driver in order to wire my joysticks and buttons directly to the RPi GPIOs.

UPDATE 0.1.4 : Compatibily with rpi2 

UPDATE 0.1.3 : Compatibily with 3.18.3 :

The driver installation now works with 3.18.3 kernel, distributed with the last firmware.

UPDATE 0.1.2 : Downgrade to 3.12.28+ :

As the module will not load with recent kernel and headers, we add the possibility of downgrading your firmware to a compatible version, until we find a fix.

UPDATE 0.1.1 : RPi B+ VERSION :

The new Raspberry Pi B+ Revision brought us 9 more GPIOs, so we are now able to connect 2 joysticks and 12 buttons directly to GPIOs. I updated the driver in order to support the 2 joysticks on GPIO configuration.

## The Software ##
The joystick driver is based on the gamecon_gpio_rpi driver by [marqs](https://github.com/marqs85)

It is written for 4 directions joysticks and 9 buttons per player.

It can read one joystick + 9 buttons wired on RPi GPIOs (two on RPi B+ revision)  

It uses internal pull-ups of RPi, so all switches must be directly connected to its corresponding GPIO and to the ground.


## Common Case : Joysticks connected to GPIOs ##


### Pinout ###
Let's consider a 7 buttons cab panel with this button order : 

     ↑   Ⓨ Ⓧ Ⓛ  
    ← →	 Ⓑ Ⓐ Ⓡ Ⓗ
     ↓  

With Ⓡ = Right trigger = TR and Ⓛ  = Left rtigger = TL  
Ⓗ is the Hotkey.

Here is the rev B GPIO pinout summary :

![GPIO Interface](https://github.com/DigitalLumberjack/mk_arcade_joystick_rpi/raw/hotkeybtn/wiki/images/mk_joystick_arcade_GPIOs-hk.png)

If you have a Rev B+ RPi or RPi2:


![GPIO Interface](https://github.com/DigitalLumberjack/mk_arcade_joystick_rpi/raw/hotkeybtn/wiki/images/mk_joystick_arcade_GPIOsb+-hk.png)

Of course the ground can be common for all switches.

### Installation ###

### Installation Script ###

Download the installation script : 
```shell
mkdir mkjoystick
cd mkjoystick
wget https://github.com/digitalLumberjack/mk_arcade_joystick_rpi/releases/download/v0.1.4/install.sh
```

Update your system :
```shell
sudo sh ./install.sh updatesystem
sudo reboot
```

Don't forget to reboot (or the next part won't work) and re-run the script without any arguments :
```shell
sudo sh ./install.sh
```

Now jump to [Loading the driver](#loading-the-driver)


### Manual Installation ###
Update system :
```shell
sudo apt-get update
sudo apt-get upgrade
sudo rpi-update
```


1 - Install all you need :
```shell
sudo apt-get install -y --force-yes dkms cpp-4.7 gcc-4.7 git joystick
```

2 - Install last kernel headers :
```shell
wget http://www.niksula.hut.fi/~mhiienka/Rpi/linux-headers-rpi/linux-headers-`uname -r`_`uname -r`-2_armhf.deb
sudo dpkg -i linux-headers-`uname -r`_`uname -r`-2_armhf.deb
sudo rm linux-headers-`uname -r`_`uname -r`-2_armhf.deb
```

3 - Compile the driver
TODO

### Loading the driver ###

The driver is loaded with the modprobe command and take one parameter nammed "map" representing connected joysticks. 
When you will have to load the driver you must pass a list of parameters that represent the list of connected Joysticks. The first parameter will be the joystick mapped to /dev/input/js0, the second to js1 etc..

If you have connected a joystick on RPi GPIOs (joystick 1 on the pinout image) you must pass "map=1" as a parameter. If you are on B+ revision and you connected 2 joysticks you must pass map="1,2" as a parameter.

If you have one joystick connected on your RPi B or B+ version you will have to run the following command :

```shell
sudo modprobe mk_arcade_joystick_rpi map=1
```

If you have two joysticks connected on your RPi B+ version you will have to run the following command :

```shell
sudo modprobe mk_arcade_joystick_rpi map=1,2
```

The GPIO joystick 1 events will be reported to the file "/dev/input/js0" and the GPIO joystick 2  events will be reported to "/dev/input/js1"

### Auto load at startup ###

Open /etc/modules :

```shell
sudo nano /etc/modules
```

And add the line you use to load the driver : 

```shell
mk_arcade_joystick_rpi map=1,2
```

### Testing ###

Use the following command to test joysticks inputs :
```shell
jstest /dev/input/js0
```

Credits
-------------
-  [gamecon_gpio_rpi](https://github.com/petrockblog/RetroPie-Setup/wiki/gamecon_gpio_rpi) by [marqs](https://github.com/marqs85)
-  [RetroPie-Setup](https://github.com/petrockblog/RetroPie-Setup) by [petRockBlog](http://blog.petrockblock.com/)
-  [Low Level Programming of the Raspberry Pi in C](http://www.pieter-jan.com/node/15) by [Pieter-Jan](http://www.pieter-jan.com/)
