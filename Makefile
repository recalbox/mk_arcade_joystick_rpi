obj-m := mk_arcade_joystick_rpi.o
KVERSION := `uname -r`

ifdef RPI2
CFLAGS_mk_arcade_joystick_rpi.o := -DRPI2
endif

all:
	$(MAKE) -C /lib/modules/$(KVERSION)/build M=$(PWD) modules

clean:
	$(MAKE) -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
