/*
 * MTK7688GPIO.h
 *
 *  Created on: 2018/3/4
 *      Author: primax
 */

#ifndef MTK7688GPIO_H_
#define MTK7688GPIO_H_

#include <stdint.h>

class MTK7688GPIO {
public:
	enum {DIR_IN = 0, DIR_OUT = 1};

public:
	MTK7688GPIO();
	virtual ~MTK7688GPIO();

public:
	int InitMmap(void);
	void SetPinDir(int pin, int is_output);
	void SetPin(int pin, int value);
	int GetPin(int pin);

private:
	uint8_t* gpio_mmap_reg;
	int gpio_mmap_fd;
};

#endif /* MTK7688GPIO_H_ */
