
#include "Adafruit_GFX.h"
#include "mraa/common.hpp"
#include "mraa/gpio.hpp"

typedef unsigned char byte;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef short	int16_t;
typedef unsigned short uint16_t;

//G1		R1 |
//GND	B1 |
//G2		R2 |
//E 		B2 |
//B		A  |
//D		C  |
//LAT	CLK|
//GND	OE |

struct color
{
public:
	uint8 r;
	uint8 g;
	uint8 b;
};

struct RGBPin
{
	uint8 OE;
	uint8 CLK;
	uint8 LAT;

	uint8 CHA;
	uint8 CHB;
	uint8 CHC;
	uint8 CHD;
	uint8 CHE;

	uint8 R1;
	uint8 G1;
	uint8 B1;
	uint8 R2;
	uint8 G2;
	uint8 B2;
};

#define layers 256
#define colorDepthPerChannel 4  //(2^colorDepthPerChannel)^3  Colors. 4096@4 bit.  262144 @ 6 bit. 

//0b0000RRRRGGGGBBBB
#define rmask 0b0000000000001111
#define gmask 0b0000000011110000
#define bmask 0b0000111100000000

class Colors
{
public:
	static const int16_t BLACK = 0;
	static const int16_t RED = rmask;
	static const int16_t GREEN = gmask;
	static const int16_t BLUE = bmask;
	static const int16_t WHITE = rmask | gmask | bmask;
	static const int16_t CYAN = gmask | bmask;
	static const int16_t YELLOW = rmask | gmask;
	static const int16_t MAGENTA = rmask | bmask;
};

const uint8_t gamma8[256] = {
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
  2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
  5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
  10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
  17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
  25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
  37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
  51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
  69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
  90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255
};

class MTK7688RGBmatrixPanel : public Adafruit_GFX
{
public:
	/*variable pin setup*/
	MTK7688RGBmatrixPanel(const struct RGBPin Pins, uint16_t width, uint16_t height);
	~MTK7688RGBmatrixPanel();

public:
	void begin(void);
	void drawPixel(int16_t x, int16_t y, uint16_t c);
	void drawPixel(int16_t x, int16_t y, uint8 r, uint8 g, uint8 b);
	
	void fillScreen(uint16_t c);
	/* reset all Pixels to black */
	void black();
	/*  call every 1-3us! */
	void updateDisplay();
	/*  from 0 = off to 10 = max */
	void setBrightness(byte brightness);
	/*  returns Color for call Adafruit_GFS methods */
	static int16_t AdafruitColor(uint8 r, uint8 g, uint8 b);

	void on();
	void update();
	void StopTimer(bool b);
	void swapBuffers(bool);
	bool EPortSupport() const { return EPortAvailable; }
	void setBackBuffer(color* c) { pixelBuf[backindex] = c; }

private:
	void init(void);
	void initGPIO();
	void SetColorPin(mraa::Gpio* gpio,  uint16_t val);
	void SetGPIO(mraa::Gpio* gpio,  bool b);
	void SetGPIODir(mraa::Gpio* gpio,  mraa::Dir dir);
	color* backBuffer();

	color *pixelBuf[2];
	color *buffptr;

	volatile uint8_t backindex;
	volatile bool swapflag;
	volatile byte loopNr;
	volatile byte loopNrOn;

    mraa::Gpio* gpio_oe;
    mraa::Gpio* gpio_clk;
    mraa::Gpio* gpio_lat;

    mraa::Gpio* gpio_cha;
    mraa::Gpio* gpio_chb;
    mraa::Gpio* gpio_chc;
    mraa::Gpio* gpio_chd;
    mraa::Gpio* gpio_che;

    mraa::Gpio* gpio_r1;
    mraa::Gpio* gpio_g1;
    mraa::Gpio* gpio_b1;
    mraa::Gpio* gpio_r2;
    mraa::Gpio* gpio_g2;
    mraa::Gpio* gpio_b2;

    struct RGBPin m_Pins;

	uint16_t WIDTH;
	uint16_t HEIGHT;

	uint32_t gpio;

	uint8 layerStep;
	uint8 layer;
	uint8 row;
	uint8 plane;

	bool EPortAvailable;
};

