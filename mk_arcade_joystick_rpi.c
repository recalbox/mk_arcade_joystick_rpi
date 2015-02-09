/*
 *  Arcade Joystick Driver for RaspberryPi
 *
 *  Copyright (c) 2014 Matthieu Proucelle
 *
 *  Based on the gamecon driver by Vojtech Pavlik, and Markus Hiienkari
 */


/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include <linux/ioport.h>
#include <asm/io.h>


MODULE_AUTHOR("Matthieu Proucelle");
MODULE_DESCRIPTION("GPIO and MCP23017 Arcade Joystick Driver");
MODULE_LICENSE("GPL");

#define MK_MAX_DEVICES		9

#ifdef RPI2
#define PERI_BASE        0x3F000000
#else
#define PERI_BASE        0x20000000
#endif

#define GPIO_BASE                (PERI_BASE + 0x200000) /* GPIO controller */

#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define GPIO_READ(g)  *(gpio + 13) &= (1<<(g))

#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)
#define GPIO_CLR *(gpio+10)

#define BSC1_BASE		(PERI_BASE + 0x804000)


/*
 * MCP23017 Defines
 */
#define MPC23017_GPIOA_MODE		0x00
#define MPC23017_GPIOB_MODE		0x01
#define MPC23017_GPIOA_PULLUPS_MODE	0x0c
#define MPC23017_GPIOB_PULLUPS_MODE	0x0d
#define MPC23017_GPIOA_READ             0x12
#define MPC23017_GPIOB_READ             0x13

/*
 * Defines for I2C peripheral (aka BSC, or Broadcom Serial Controller)
 */

#define BSC1_C		*(bsc1 + 0x00)
#define BSC1_S		*(bsc1 + 0x01)
#define BSC1_DLEN	*(bsc1 + 0x02)
#define BSC1_A		*(bsc1 + 0x03)
#define BSC1_FIFO	*(bsc1 + 0x04)

#define BSC_C_I2CEN	(1 << 15)
#define BSC_C_INTR	(1 << 10)
#define BSC_C_INTT	(1 << 9)
#define BSC_C_INTD	(1 << 8)
#define BSC_C_ST	(1 << 7)
#define BSC_C_CLEAR	(1 << 4)
#define BSC_C_READ	1

#define START_READ	BSC_C_I2CEN|BSC_C_ST|BSC_C_CLEAR|BSC_C_READ
#define START_WRITE	BSC_C_I2CEN|BSC_C_ST

#define BSC_S_CLKT	(1 << 9)
#define BSC_S_ERR	(1 << 8)
#define BSC_S_RXF	(1 << 7)
#define BSC_S_TXE	(1 << 6)
#define BSC_S_RXD	(1 << 5)
#define BSC_S_TXD	(1 << 4)
#define BSC_S_RXR	(1 << 3)
#define BSC_S_TXW	(1 << 2)
#define BSC_S_DONE	(1 << 1)
#define BSC_S_TA	1

#define CLEAR_STATUS	BSC_S_CLKT|BSC_S_ERR|BSC_S_DONE

static volatile unsigned *gpio;
static volatile unsigned *bsc1;

struct mk_config {
    int args[MK_MAX_DEVICES];
    unsigned int nargs;
};

static struct mk_config mk_cfg __initdata;

module_param_array_named(map, mk_cfg.args, int, &(mk_cfg.nargs), 0);
MODULE_PARM_DESC(map, "Enable or disable GPIO and MCP23017 Arcade Joystick");

enum mk_type {
    MK_NONE = 0,
    MK_ARCADE_GPIO,
    MK_ARCADE_GPIO_BPLUS,
    MK_ARCADE_MCP23017,
    MK_MAX
};

#define MK_REFRESH_TIME	HZ/100

struct mk_pad {
    struct input_dev *dev;
    enum mk_type type;
    char phys[32];
    int mcp23017addr;
};

struct mk_nin_gpio {
    unsigned pad_id;
    unsigned cmd_setinputs;
    unsigned cmd_setoutputs;
    unsigned valid_bits;
    unsigned request;
    unsigned request_len;
    unsigned response_len;
    unsigned response_bufsize;
};

struct mk {
    struct mk_pad pads[MK_MAX_DEVICES];
    struct timer_list timer;
    int pad_count[MK_MAX];
    int used;
    struct mutex mutex;
};

struct mk_subdev {
    unsigned int idx;
};

static struct mk *mk_base;

static const int mk_max_arcade_buttons = 12;

// Map of the gpios :                     up, down, left, right, start, select, a,  b,  tr, y,  x,  tl
static const int mk_arcade_gpio_maps[] = {4,  17,    27,  22,    10,    9,      25, 24, 23, 18, 15, 14 };
// 2nd joystick on the b+ GPIOS                 up, down, left, right, start, select, a,  b,  tr, y,  x,  tl
static const int mk_arcade_gpio_maps_bplus[] = {11, 5,    6,    13,    19,    26,     21, 20, 16, 12, 7,  8 };
// Map of the mcp23017 on GPIOA            up, down, left, right, start, select
static const int mk_arcade_gpioa_maps[] = {0,  1,    2,    3,     4,     5      };

// Map of the mcp23017 on GPIOB            a, b, tr, y, x, tl
static const int mk_arcade_gpiob_maps[] = {0, 1, 2,  3, 4, 5 };

static const short mk_arcade_gpio_btn[] = {
    BTN_START, BTN_SELECT, BTN_A, BTN_B, BTN_TR, BTN_Y, BTN_X, BTN_TL
};

static const char *mk_names[] = {
    NULL, "GPIO Controller 1", "GPIO Controller 2", "MCP23017 Controller"
};

/* GPIO UTILS */
static void setGpioPullUps(int pullUps) {
    *(gpio + 37) = 0x02;
    udelay(10);
    *(gpio + 38) = pullUps;
    udelay(10);
    *(gpio + 37) = 0x00;
    *(gpio + 38) = 0x00;
}

static void setGpioAsInput(int gpioNum) {
    INP_GPIO(gpioNum);
}

/* I2C UTILS */
static void i2c_init(void) {
    INP_GPIO(2);
    SET_GPIO_ALT(2, 0);
    INP_GPIO(3);
    SET_GPIO_ALT(3, 0);
}

static void wait_i2c_done(void) {
    while ((!((BSC1_S) & BSC_S_DONE))) {
        udelay(100);
    }
}

// Function to write data to an I2C device via the FIFO.  This doesn't refill the FIFO, so writes are limited to 16 bytes
// including the register address. len specifies the number of bytes in the buffer.

static void i2c_write(char dev_addr, char reg_addr, char *buf, unsigned short len) {

    int idx;

    BSC1_A = dev_addr;
    BSC1_DLEN = len + 1; // one byte for the register address, plus the buffer length

    BSC1_FIFO = reg_addr; // start register address
    for (idx = 0; idx < len; idx++)
        BSC1_FIFO = buf[idx];

    BSC1_S = CLEAR_STATUS; // Reset status bits (see #define)
    BSC1_C = START_WRITE; // Start Write (see #define)

    wait_i2c_done();

}

// Function to read a number of bytes into a  buffer from the FIFO of the I2C controller

static void i2c_read(char dev_addr, char reg_addr, char *buf, unsigned short len) {

    i2c_write(dev_addr, reg_addr, NULL, 0);

    unsigned short bufidx;
    bufidx = 0;

    memset(buf, 0, len); // clear the buffer

    BSC1_DLEN = len;
    BSC1_S = CLEAR_STATUS; // Reset status bits (see #define)
    BSC1_C = START_READ; // Start Read after clearing FIFO (see #define)

    do {
        // Wait for some data to appear in the FIFO
        while ((BSC1_S & BSC_S_TA) && !(BSC1_S & BSC_S_RXD));

        // Consume the FIFO
        while ((BSC1_S & BSC_S_RXD) && (bufidx < len)) {
            buf[bufidx++] = BSC1_FIFO;
        }
    } while ((!(BSC1_S & BSC_S_DONE)));
}

/*  ------------------------------------------------------------------------------- */

static void mk_mcp23017_read_packet(struct mk_pad * pad, unsigned char *data) {
    int i;
    char resultA, resultB;
    i2c_read(pad->mcp23017addr, MPC23017_GPIOA_READ, &resultA, 1);
    i2c_read(pad->mcp23017addr, MPC23017_GPIOB_READ, &resultB, 1);

    // read direction
    for (i = 0; i < 4; i++) {
        data[i] = !((resultA >> mk_arcade_gpioa_maps[i]) & 0x1);
    }
    // read buttons on gpioa
    for (i = 4; i < 6; i++) {
        data[i] = !((resultA >> mk_arcade_gpioa_maps[i]) & 0x1);
    }
    // read buttons on gpiob
    for (i = 6; i < 12; i++) {
        data[i] = !((resultB >> (mk_arcade_gpiob_maps[i-6])) & 0x1);
    }
}

static void mk_gpio_read_packet(struct mk_pad * pad, unsigned char *data) {
    int i;
    // Duplicate code to avoid the if at each loop
    if (pad->type == MK_ARCADE_GPIO) {
        for (i = 0; i < mk_max_arcade_buttons; i++) {
            int read = GPIO_READ(mk_arcade_gpio_maps[i]);
            if (read == 0) data[i] = 1;
            else data[i] = 0;
        }
    }else if (pad->type == MK_ARCADE_GPIO_BPLUS) {
         for (i = 0; i < mk_max_arcade_buttons; i++) {
            int read = GPIO_READ(mk_arcade_gpio_maps_bplus[i]);
            if (read == 0) data[i] = 1;
            else data[i] = 0;
        }
    }
}

static void mk_input_report(struct mk_pad * pad, unsigned char * data) {
    struct input_dev * dev = pad->dev;
    int j;
    input_report_abs(dev, ABS_Y, !data[0]-!data[1]);
    input_report_abs(dev, ABS_X, !data[2]-!data[3]);
    for (j = 4; j < mk_max_arcade_buttons; j++) {
        input_report_key(dev, mk_arcade_gpio_btn[j - 4], data[j]);
    }
    input_sync(dev);
}

static void mk_process_packet(struct mk *mk) {

    unsigned char data[mk_max_arcade_buttons];
    struct mk_pad *pad;
    int i;

    for (i = 0; i < MK_MAX_DEVICES; i++) {
        pad = &mk->pads[i];
        if (pad->type == MK_ARCADE_GPIO || pad->type == MK_ARCADE_GPIO_BPLUS) {
            mk_gpio_read_packet(pad, data);
            mk_input_report(pad, data);
        }
        if (pad->type == MK_ARCADE_MCP23017) {
            mk_mcp23017_read_packet(pad, data);
            mk_input_report(pad, data);
        }
    }

}

/*
 * mk_timer() initiates reads of console pads data.
 */

static void mk_timer(unsigned long private) {
    struct mk *mk = (void *) private;
    mk_process_packet(mk);
    mod_timer(&mk->timer, jiffies + MK_REFRESH_TIME);
}

static int mk_open(struct input_dev *dev) {
    struct mk *mk = input_get_drvdata(dev);
    int err;

    err = mutex_lock_interruptible(&mk->mutex);
    if (err)
        return err;

    if (!mk->used++)
        mod_timer(&mk->timer, jiffies + MK_REFRESH_TIME);

    mutex_unlock(&mk->mutex);
    return 0;
}

static void mk_close(struct input_dev *dev) {
    struct mk *mk = input_get_drvdata(dev);

    mutex_lock(&mk->mutex);
    if (!--mk->used) {
        del_timer_sync(&mk->timer);
    }
    mutex_unlock(&mk->mutex);
}

static int __init mk_setup_pad(struct mk *mk, int idx, int pad_type_arg) {
    struct mk_pad *pad = &mk->pads[idx];
    struct input_dev *input_dev;
    int i, pad_type;
    int err;
    char FF = 0xFF;
    pr_err("pad type : %d\n",pad_type_arg);

    if (pad_type_arg == MK_ARCADE_GPIO) {
        pad_type = MK_ARCADE_GPIO;
    }
    else if (pad_type_arg == MK_ARCADE_GPIO_BPLUS) {
        pad_type = MK_ARCADE_GPIO_BPLUS;
    } else {
        pad_type = MK_ARCADE_MCP23017;
    }
    if (pad_type < 1) {
        pr_err("Pad type %d unknown\n", pad_type);
        return -EINVAL;
    }
    pr_err("pad type : %d\n",pad_type);
    pad->dev = input_dev = input_allocate_device();
    if (!input_dev) {
        pr_err("Not enough memory for input device\n");
        return -ENOMEM;
    }

    pad->type = pad_type;
    pad->mcp23017addr = pad_type_arg;
    snprintf(pad->phys, sizeof (pad->phys),
            "input%d", idx);

    input_dev->name = mk_names[pad_type];
    input_dev->phys = pad->phys;
    input_dev->id.bustype = BUS_PARPORT;
    input_dev->id.vendor = 0x0001;
    input_dev->id.product = pad_type;
    input_dev->id.version = 0x0100;

    input_set_drvdata(input_dev, mk);

    input_dev->open = mk_open;
    input_dev->close = mk_close;

    input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);

    for (i = 0; i < 2; i++)
        input_set_abs_params(input_dev, ABS_X + i, -1, 1, 0, 0);
    for (i = 0; i < mk_max_arcade_buttons - 4; i++)
        __set_bit(mk_arcade_gpio_btn[i], input_dev->keybit);

    mk->pad_count[pad_type]++;

    switch (pad_type) {
        case MK_ARCADE_GPIO:
            for (i = 0; i < mk_max_arcade_buttons; i++) {
                setGpioAsInput(mk_arcade_gpio_maps[i]);
            }
            setGpioPullUps(0xBC6C610);
            printk("GPIO configured for pad%d\n", idx);
            break;
        case MK_ARCADE_GPIO_BPLUS:
            for (i = 0; i < mk_max_arcade_buttons; i++) {
                setGpioAsInput(mk_arcade_gpio_maps_bplus[i]);
            }
            setGpioPullUps(0xFFFFFF0);
            printk("GPIO configured for pad%d\n", idx);
            break;
        case MK_ARCADE_MCP23017:
            i2c_init();
            udelay(1000);
            // Put all GPIOA inputs on MCP23017 in INPUT mode
            i2c_write(pad->mcp23017addr, MPC23017_GPIOA_MODE, &FF, 1);
            udelay(1000);
            // Put all inputs on MCP23017 in pullup mode
            i2c_write(pad->mcp23017addr, MPC23017_GPIOA_PULLUPS_MODE, &FF, 1);
            udelay(1000);
            // Put all GPIOB inputs on MCP23017 in INPUT mode
            i2c_write(pad->mcp23017addr, MPC23017_GPIOB_MODE, &FF, 1);
            udelay(1000);
            // Put all inputs on MCP23017 in pullup mode
            i2c_write(pad->mcp23017addr, MPC23017_GPIOB_PULLUPS_MODE, &FF, 1);
            udelay(1000);
            // Put all inputs on MCP23017 in pullup mode a second time
            // Known bug : if you remove this line, you will not have pullups on GPIOB 
            i2c_write(pad->mcp23017addr, MPC23017_GPIOB_PULLUPS_MODE, &FF, 1);
            udelay(1000);
            break;
    }

    err = input_register_device(pad->dev);
    if (err)
        goto err_free_dev;

    return 0;

err_free_dev:
    input_free_device(pad->dev);
    pad->dev = NULL;
    return err;
}

static struct mk __init *mk_probe(int *pads, int n_pads) {
    struct mk *mk;
    int i;
    int count = 0;
    int err;

    mk = kzalloc(sizeof (struct mk), GFP_KERNEL);
    if (!mk) {
        pr_err("Not enough memory\n");
        err = -ENOMEM;
        goto err_out;
    }

    for (i = 0; i < n_pads && i < MK_MAX_DEVICES; i++) {
        if (!pads[i])
            continue;

        err = mk_setup_pad(mk, i, pads[i]);
        if (err)
            goto err_unreg_devs;

        count++;
    }

    if (count == 0) {
        pr_err("No valid devices specified\n");
        err = -EINVAL;
        goto err_free_mk;
    }

    mutex_init(&mk->mutex);
    setup_timer(&mk->timer, mk_timer, (long) mk);

    return mk;

err_unreg_devs:
    while (--i >= 0)
        if (mk->pads[i].dev)
            input_unregister_device(mk->pads[i].dev);
err_free_mk:
    kfree(mk);
err_out:
    return ERR_PTR(err);
}

static void mk_remove(struct mk *mk) {
    int i;

    for (i = 0; i < MK_MAX_DEVICES; i++)
        if (mk->pads[i].dev)
            input_unregister_device(mk->pads[i].dev);
    kfree(mk);
}

static int __init mk_init(void) {
    /* Set up gpio pointer for direct register access */
    if ((gpio = ioremap(GPIO_BASE, 0xB0)) == NULL) {
        pr_err("io remap failed\n");
        return -EBUSY;
    }
    /* Set up i2c pointer for direct register access */
    if ((bsc1 = ioremap(BSC1_BASE, 0xB0)) == NULL) {
        pr_err("io remap failed\n");
        return -EBUSY;
    }
    if (mk_cfg.nargs < 1) {
        pr_err("at least one device must be specified\n");
        return -EINVAL;
    } else {
        mk_base = mk_probe(mk_cfg.args, mk_cfg.nargs);
        if (IS_ERR(mk_base))
            return -ENODEV;
    }
    return 0;
}

static void __exit mk_exit(void) {
    if (mk_base)
        mk_remove(mk_base);

    iounmap(gpio);
    iounmap(bsc1);
}

module_init(mk_init);
module_exit(mk_exit);
