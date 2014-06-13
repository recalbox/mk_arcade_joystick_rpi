mk_arcade_joystick_rpi - Work in progress
==============


Introduction
-------------
The RaspberryPi is an amazing tool I discovered a month ago. The RetroPie project made me want to build my own Arcade Cabinet with simple arcade buttons and joysticks.

The Raspberry Pi Board B Rev 2 has a maximum of 21 usable GPIOs, not enough to wire all the 24 switches (2 joystick and 16 buttons) that a standard panel requires.

A hardware solution
-------------
A little cheap chip named MCP23017 allows you to add 16 external GPIO, and take only two GPIO on the RPi. The chip allows you to use GPIO as output or input, input is what we are looking for. If you want to use more than one chip, the i2c protocol lets you choose different addresses for the connected peripheral, but all use the same SDA and SCL GPIOs.

The Software 
-------------
The joystick driver is based on the gamecon_gpio_rpi driver by [marqs](https://github.com/marqs85)

It can read one joystick + buttons wired on RPi GPIOs and up to 5 other joysticks + buttons from MCP23017 chips. One MCP23017 is required for each joystick.

It uses internal pull-ups of RPi and MCP23017, so all switches must be connected to its corresponding GPIO and to the ground.

Pinout
-------------
Here is the GPIO pinout summary :


![GPIO Interface](https://github.com/DigitalLumberjack/mk_arcade_joystick_rpi/raw/master/wiki/images/mk_joystick_arcade_GPIOs.png)


And here is the MCP23017 pinout summary :


![MCP23017 Interface](https://github.com/DigitalLumberjack/mk_arcade_joystick_rpi/raw/master/wiki/images/mk_joystick_arcade_mcp23017.png)

Of course the ground can be common for all switches.

Preparation 
-------------
Activate i2c on your RPi :
```shell
sudo nano /etc/modules
```
Add the following lines in order to load i2c modules automatically :
```shell
i2c-bcm2708 
i2c-dev
```

And if the file /etc/modprobe.d/raspi-blacklist.conf exists : 
```shell
sudo nano /etc/modprobe.d/raspi-blacklist.conf
```

Check if you have a line with :
```shell
i2c-bcm2708 
```
and add a # at the beginning of the line to remove the blacklisting

Reboot or load the two module :
```shell
modprobe i2c-bcm2708 i2c-dev
```

Compilation 
-------------

You need to have the last firmware installed.

Install all you need :
```shell
apt-get update
apt-get install -y --force-yes dkms cpp-4.7 gcc-4.7 git joystick i2c-tools
```

Install last kernel headers :
```shell
wget http://www.niksula.hut.fi/~mhiienka/Rpi/linux-headers-rpi/linux-headers-`uname -r`_`uname -r`-2_armhf.deb
dpkg -i linux-headers-`uname -r`_`uname -r`-2_armhf.deb
rm linux-headers-`uname -r`_`uname -r`-2_armhf.deb
```

Install driver from release (prefered):
```shell
wget https://github.com/digitalLumberjack/mk_arcade_joystick_rpi/releases/download/0.1.0/mk-arcade-joystick-rpi-0.1.0.deb
sudo dpkg -i mk-arcade-joystick-rpi-0.1.0.deb
```

Install driver from sources :
```shell
cd /usr/src
sudo git clone --depth=0 https://github.com/digitalLumberjack/mk_arcade_joystick_rpi.git mk_arcade_joystick_rpi-0.1.0
cd mk_arcade_joystick_rpi-0.1.0
sudo dkms build -m mk_arcade_joystick_rpi -v 0.1.0
sudo dkms install -m mk_arcade_joystick_rpi -v 0.1.0
cd ..
sudo rm -rf mk_arcade_joystick_rpi-0.1.0
```


Configuration 
-------------
When you want to load the driver you must pass a list of parameters that represent the list of connected Joysticks. The first parameter will be the joystick mapped to /dev/input/js0, the second to js1 etc..

If you have connected a joystick on RPi GPIOs you must pass "1" as a parameter.

If you have connected one or more joysticks with MCP23017, you must pass the address of I2C peripherals connected, which you can get with the command :

```shell
sudo i2cdetect -y 1
```

The configuration of each MCP23017 is done by setting pads A0 A1 and A2 to 0 or 1.

If you configured your MCP23017 with A0 A1 and A2 connected to the ground, the address returned by i2cdetect should be 0x20

So if you have a joystick connected to RPi GPIOs and a joystick on a MCP23017 with the address 0x20, in order to load the driver, you must run the command :

```shell
sudo modprobe mk_arcade_joystick_rpi map=1,0x20
```


The GPIO joystick events will be reported to the file "/dev/input/js0" and the mcp23017 joystick events will be reported to "/dev/input/js1"

Testing
-------------

Use the following command to test joysticks inputs :
```shell
jstest /dev/input/js0
```

Known Bugs
-------------
If you try to read or write on i2c with a tool like i2cget or i2cset when the driver is loaded, you are gonna have a bad time... 

If you try i2cdetect when the driver is running, it will show you strange peripheral addresses...

Credits
-------------
-  [gamecon_gpio_rpi](https://github.com/petrockblog/RetroPie-Setup/wiki/gamecon_gpio_rpi) by [marqs](https://github.com/marqs85)
-  [RetroPie-Setup](https://github.com/petrockblog/RetroPie-Setup) by [petRockBlog](http://blog.petrockblock.com/)
-  [Low Level Programming of the Raspberry Pi in C](http://www.pieter-jan.com/node/15) by [Pieter-Jan](http://www.pieter-jan.com/)
