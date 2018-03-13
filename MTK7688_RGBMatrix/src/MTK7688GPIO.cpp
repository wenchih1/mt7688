/*
 * MTK7688GPIO.cpp
 *
 *  Created on: 2018/3/4
 *      Author: primax
 */

#include "MTK7688GPIO.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#define MMAP_PATH    "/dev/mem"

#define RALINK_GPIO_DIR_IN        	0
#define RALINK_GPIO_DIR_OUT        	1

#define RALINK_REG_PIOINT        	0x690
#define RALINK_REG_PIOEDGE        	0x6A0
#define RALINK_REG_PIORENA        	0x650
#define RALINK_REG_PIOFENA        	0x660
#define RALINK_REG_PIODATA        	0x620
#define RALINK_REG_PIODIR        	0x600
#define RALINK_REG_PIOSET        	0x630
#define RALINK_REG_PIORESET        	0x640

#define RALINK_REG_PIO6332INT        0x694
#define RALINK_REG_PIO6332EDGE       0x6A4
#define RALINK_REG_PIO6332RENA       0x654
#define RALINK_REG_PIO6332FENA       0x664
#define RALINK_REG_PIO6332DATA       0x624
#define RALINK_REG_PIO6332DIR        0x604
#define RALINK_REG_PIO6332SET        0x634
#define RALINK_REG_PIO6332RESET      0x644

#define RALINK_REG_PIO9564INT        0x698
#define RALINK_REG_PIO9564EDGE       0x6A8
#define RALINK_REG_PIO9564RENA       0x658
#define RALINK_REG_PIO9564FENA       0x668
#define RALINK_REG_PIO9564DATA       0x628
#define RALINK_REG_PIO9564DIR        0x608
#define RALINK_REG_PIO9564SET        0x638
#define RALINK_REG_PIO9564RESET      0x648


enum {
	MUX_I2C = 0, MUX_UART0, MUX_UART1, MUX_UART2, MUX_PWM0, MUX_PWM1, MUX_REF_CLK, MUX_SPI_S,
	MUX_SPI_CS1, MUX_I2S, MUX_EPHY, MUX_WLED, MUX_MAX,
};

static struct pinmux {
	uint8_t index;
	int8_t pins[4];
	bool gpio_mode;
	unsigned int shift;
	unsigned int mask;
} mt7688_mux[] = {
	{
		.index = MUX_I2C,
		.pins = {4, 5, -1, -1},
		.gpio_mode = false,
		.shift = 20,
		.mask = 0x3,
	}, {
		.index = MUX_UART0,
		.pins = {12, 13, -1, -1},
		.gpio_mode = false,
		.shift = 8,
		.mask = 0x3,
	}, {
		.index = MUX_UART1,
		.pins = {45, 45, -1, -1},
		.gpio_mode = false,
		.shift = 24,
		.mask = 0x3,
	}, {
		.index = MUX_UART2,
		.pins = {20, 21, -1, -1},
		.gpio_mode = false,
		.shift = 26,
		.mask = 0x3,
	}, {
		.index = MUX_PWM0,
		.pins = {18, -1, -1, -1},
		.gpio_mode = false,
		.shift = 28,
		.mask = 0x3,
	}, {
		.index = MUX_PWM1,
		.pins = {18, 19, -1, -1},
		.gpio_mode = false,
		.shift = 30,
		.mask = 0x3,
	}, {
		.index = MUX_REF_CLK,
		.pins = {37, -1, -1, -1},
		.gpio_mode = false,
		.shift = 18,
		.mask = 0x1,
	}, {
		.index = MUX_SPI_S,
		.pins = {14, 15, 16, 17},
		.gpio_mode = false,
		.shift = 2,
		.mask = 0x3,
	}, {
		.index = MUX_SPI_CS1,
		.pins = {6, -1, -1, -1},
		.gpio_mode = false,
		.shift = 4,
		.mask = 0x3,
	}, {
		.index = MUX_I2S,
		.pins = {0, 1, 2, 3},
		.gpio_mode = false,
		.shift = 6,
		.mask = 0x3,
	}, {
		.index = MUX_EPHY,
		.pins = {43, -1, -1, -1},
		.gpio_mode = false,
		.shift = 34,
		.mask = 0x3,
	}, {
		.index = MUX_WLED,
		.pins = {44, -1, -1, -1},
		.gpio_mode = false,
		.shift = 32,
		.mask = 0x3,
	}
};

MTK7688GPIO::MTK7688GPIO()
: gpio_mmap_reg(NULL), gpio_mmap_fd(-1)
{
	// TODO Auto-generated constructor stub
}

MTK7688GPIO::~MTK7688GPIO()
{
	// TODO Auto-generated destructor stub
	if(gpio_mmap_reg != NULL)
	{
		munmap(gpio_mmap_reg, 1024);
		gpio_mmap_reg = NULL;
	}

	if(gpio_mmap_fd >= 0)
	{
		close(gpio_mmap_fd);
		gpio_mmap_fd = -1;
	}
}

int MTK7688GPIO::InitMmap(void)
{
    if ((gpio_mmap_fd = open(MMAP_PATH, O_RDWR)) < 0) {
        fprintf(stderr, "unable to open mmap file");
        return -1;
    }

    gpio_mmap_reg = (uint8_t*) mmap(NULL, 1024, PROT_READ | PROT_WRITE,
        MAP_FILE | MAP_SHARED, gpio_mmap_fd, 0x10000000);

    close(gpio_mmap_fd);
    if (gpio_mmap_reg == MAP_FAILED) {
        perror("foo");
        fprintf(stderr, "failed to mmap");
        gpio_mmap_reg = NULL;
//        close(gpio_mmap_fd);
        return -1;
    }

    return 0;
}


void MTK7688GPIO::gpiomode_set(unsigned int mask, unsigned int shift, unsigned int val)
{
	// val: 0:fun1, 1:gpio, 2:fun3, 3:fun4, in gpio group
	volatile uint32_t *reg = (volatile uint32_t*) (gpio_mmap_reg + 0x60);
	unsigned int v;

	if (shift > 31) {
		shift -= 32;
		reg++;
	}

	v = *reg;
	v &= ~(mask << shift);
	v |= (val << shift);
	*(volatile uint32_t*) (reg) = v;
}

void MTK7688GPIO::SetPinMode(uint8_t pin, bool isgpio)
{
	uint8_t i = 0;
	for(; i < MUX_MAX; ++ i)
	{
		for(uint8_t j = 0; j < 4; ++ j)
		{
			if(mt7688_mux[i].pins[j] == pin && ! mt7688_mux[i].gpio_mode)
			{
				gpiomode_set(mt7688_mux[i].mask, mt7688_mux[i].shift, isgpio);
				mt7688_mux[i].gpio_mode = true;
				return;
			}
		}
	}
}

void MTK7688GPIO::SetPinDir(int pin, int is_output)
{
    uint32_t tmp;

    /* MT7621, MT7628 */
    if (pin <= 31) {
        tmp = *(volatile uint32_t *)(gpio_mmap_reg + RALINK_REG_PIODIR);
        if (is_output)
            tmp |=  (1u << pin);
        else
            tmp &= ~(1u << pin);
        *(volatile uint32_t *)(gpio_mmap_reg + RALINK_REG_PIODIR) = tmp;
    } else if (pin <= 63) {
        tmp = *(volatile uint32_t *)(gpio_mmap_reg + RALINK_REG_PIO6332DIR);
        if (is_output)
            tmp |=  (1u << (pin-32));
        else
            tmp &= ~(1u << (pin-32));
        *(volatile uint32_t *)(gpio_mmap_reg + RALINK_REG_PIO6332DIR) = tmp;
    } else if (pin <= 95) {
        tmp = *(volatile uint32_t *)(gpio_mmap_reg + RALINK_REG_PIO9564DIR);
        if (is_output)
            tmp |=  (1u << (pin-64));
        else
            tmp &= ~(1u << (pin-64));
        *(volatile uint32_t *)(gpio_mmap_reg + RALINK_REG_PIO9564DIR) = tmp;
    }
}

void MTK7688GPIO::SetPin(int pin, int value)
{
    uint32_t tmp;

    /* MT7621, MT7628 */
    if (pin <= 31) {
        tmp = (1u << pin);
        if (value)
            *(volatile uint32_t *)(gpio_mmap_reg + RALINK_REG_PIOSET) = tmp;
        else
            *(volatile uint32_t *)(gpio_mmap_reg + RALINK_REG_PIORESET) = tmp;
    } else if (pin <= 63) {
        tmp = (1u << (pin-32));
        if (value)
            *(volatile uint32_t *)(gpio_mmap_reg + RALINK_REG_PIO6332SET) = tmp;
        else
            *(volatile uint32_t *)(gpio_mmap_reg + RALINK_REG_PIO6332RESET) = tmp;
    } else if (pin <= 95) {
        tmp = (1u << (pin-64));
        if (value)
            *(volatile uint32_t *)(gpio_mmap_reg + RALINK_REG_PIO9564SET) = tmp;
        else
            *(volatile uint32_t *)(gpio_mmap_reg + RALINK_REG_PIO9564RESET) = tmp;
    }

}

int MTK7688GPIO::GetPin(int pin)
{
    uint32_t tmp = 0;

    /* MT7621, MT7628 */
    if (pin <= 31) {
        tmp = *(volatile uint32_t *)(gpio_mmap_reg + RALINK_REG_PIODATA);
        tmp = (tmp >> pin) & 1u;
    } else if (pin <= 63) {
        tmp = *(volatile uint32_t *)(gpio_mmap_reg + RALINK_REG_PIO6332DATA);
        tmp = (tmp >> (pin-32)) & 1u;
    } else if (pin <= 95) {
        tmp = *(volatile uint32_t *)(gpio_mmap_reg + RALINK_REG_PIO9564DATA);
        tmp = (tmp >> (pin-64)) & 1u;
        tmp = (tmp >> (pin-24)) & 1u;
    }

    return tmp;
}
