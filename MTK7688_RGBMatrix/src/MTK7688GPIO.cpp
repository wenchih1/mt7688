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
