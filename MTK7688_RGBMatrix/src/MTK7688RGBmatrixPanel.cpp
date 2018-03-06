#include "MTK7688RGBmatrixPanel.h"

#include <iostream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//G1		R1 |
//GND	B1 |
//G2		R2 |
//E 		B2 |
//B		A  |
//D		C  |
//LAT	CLK|
//GND	OE |


#define NOP5()  asm ("nop;nop;nop;nop;nop;");

//#define GAMMA_CORRECTION

#define HIGH 1
#define LOW  0
#define loops 10
#define CALLOVERHEAD 80   // Actual value measured = 80
#define LOOPTIME     20  // Actual value measured = 188
#define PLANES       1    // 4 bitplanes in RGB elements

static MTK7688RGBmatrixPanel *activePanel = NULL;
 /* create a hardware timer */
//hw_timer_t* displayUpdateTimer2 = NULL;

using namespace std;

//void IRAM_ATTR onDisplayUpdate2()
//{
//   if(activePanel==NULL)
//      return;
//
//   activePanel->updateDisplay();
//}


void MTK7688RGBmatrixPanel::SetGPIODir(mraa::Gpio* gpio,  mraa::Dir dir)
{
	mraa::Result state;
	state = gpio->dir(dir);
	if (state != mraa::SUCCESS)
		printError(state);
}

void MTK7688RGBmatrixPanel::SetGPIO(mraa::Gpio* gpio,  bool b)
{
	mraa::Result state;
	state = gpio->write(b ? HIGH : LOW);
	if (state != mraa::SUCCESS)
		printError(state);
}

void MTK7688RGBmatrixPanel::SetColorPin(mraa::Gpio* gpio,  uint16_t val)
{
#if defined(GAMMA_CORRECTION)
	SetGPIO(gpio, gamma8[val] > layer ? HIGH : LOW);
#else
	SetGPIO(gpio, val > layer ? HIGH : LOW);
#endif
}

MTK7688RGBmatrixPanel::MTK7688RGBmatrixPanel(const struct RGBPin Pins, uint16_t width, uint16_t height)
  : Adafruit_GFX(width, height)
  , loopNr(0), loopNrOn(0), row(0), plane(0), WIDTH(width), HEIGHT(height)
  , gpio_cha(0), gpio_chb(0), gpio_chc(0), gpio_chd(0), gpio_che(0)
  , gpio_r1(0), gpio_g1(0), gpio_b1(0), gpio_r2(0), gpio_g2(0), gpio_b2(0)
  , gpio_oe(0), gpio_clk(0), gpio_lat(0)
  , EPortAvailable(false), swapflag(false), backindex(0)
{
	memcpy(&m_Pins, &Pins, sizeof(RGBPin));
	layerStep = 256 / (1 << colorDepthPerChannel);
	layer = layerStep - 1;
    init();
}

MTK7688RGBmatrixPanel::~MTK7688RGBmatrixPanel()
{
	if(gpio_cha)
		delete gpio_cha;
	if(gpio_chb)
		delete gpio_chb;
	if(gpio_chc)
		delete gpio_chc;
	if(gpio_chd)
		delete gpio_chd;
	if(gpio_che)
		delete gpio_che;
	if(gpio_r1)
		delete gpio_r1;
	if(gpio_g1)
		delete gpio_g1;
	if(gpio_b1)
		delete gpio_b1;
	if(gpio_r2)
		delete gpio_r2;
	if(gpio_g2)
		delete gpio_g2;
	if(gpio_b2)
		delete gpio_b2;
	if(gpio_oe)
		delete gpio_oe;
	if(gpio_clk)
		delete gpio_clk;
	if(gpio_lat)
		delete gpio_lat;
}

void MTK7688RGBmatrixPanel::init(void)
{
    initGPIO();

    swapflag  = false;
    plane = PLANES - 1;
    row = 0;//ROWS - 1;

    // Allocate and initialize matrix buffer:
    int buffsize  = WIDTH * HEIGHT * sizeof(color) * 2;
    pixelBuf[0] = (color *)malloc(buffsize);
    memset(pixelBuf[0], 0, buffsize);
//    pixelBuf[1] = pixelBuf[0];
//    pixelBuf[1] = pix;
    // If not double-buffered, both buffers then point to the same address:
    pixelBuf[1] = &pixelBuf[0][buffsize];

    // Array index of back buffer
    backindex = 0;                      // Back buffer
    buffptr = pixelBuf[1 - backindex];  // Front buffer
    activePanel = this;                 // For interrupt hander
}

void MTK7688RGBmatrixPanel::initGPIO()
{
    // Enable all comm & address pins as outputs, set default states:

	//
	// create r1,b1,g1,r2,b2 and g2 data lines
	//
	gpio_r1 = new mraa::Gpio(m_Pins.R1);
	SetGPIODir(gpio_r1,  mraa::DIR_OUT);
	SetGPIO(gpio_r1,  LOW);

	gpio_g1 = new mraa::Gpio(m_Pins.G1);
	SetGPIODir(gpio_g1,  mraa::DIR_OUT);
	SetGPIO(gpio_g1,  LOW);

	gpio_b1 = new mraa::Gpio(m_Pins.B1);
	SetGPIODir(gpio_b1,  mraa::DIR_OUT);
	SetGPIO(gpio_b1,  LOW);

	gpio_r2 = new mraa::Gpio(m_Pins.R2);
	SetGPIODir(gpio_r2,  mraa::DIR_OUT);
	SetGPIO(gpio_r2,  LOW);

	gpio_g2 = new mraa::Gpio(m_Pins.G2);
	SetGPIODir(gpio_g2,  mraa::DIR_OUT);
	SetGPIO(gpio_g2,  LOW);

	gpio_b2 = new mraa::Gpio(m_Pins.B2);
	SetGPIODir(gpio_b2,  mraa::DIR_OUT);
	SetGPIO(gpio_b2,  LOW);

	//
	// decide A,B,C,D and E ports
	//
	gpio_cha = new mraa::Gpio(m_Pins.CHA);			// 1
	SetGPIODir(gpio_cha,  mraa::DIR_OUT);
	SetGPIO(gpio_cha,  LOW);

	gpio_chb = new mraa::Gpio(m_Pins.CHB);			// 2
	SetGPIODir(gpio_chb,  mraa::DIR_OUT);
	SetGPIO(gpio_chb,  LOW);

	gpio_chc = new mraa::Gpio(m_Pins.CHC);			// 4
	SetGPIODir(gpio_chc,  mraa::DIR_OUT);
	SetGPIO(gpio_chc,  LOW);

    if(HEIGHT > 16)
    {
		gpio_chd = new mraa::Gpio(m_Pins.CHD);		// 8
		SetGPIODir(gpio_chd,  mraa::DIR_OUT);
		SetGPIO(gpio_chd,  LOW);
    }
    if(HEIGHT > 32)
    {
		gpio_che = new mraa::Gpio(m_Pins.CHE);		// 16
		SetGPIODir(gpio_che,  mraa::DIR_OUT);
		SetGPIO(gpio_che,  LOW);
        EPortAvailable = true;
    }

	//
	// create lat,clk and oe control functions
	//
	gpio_lat = new mraa::Gpio(m_Pins.LAT);
	SetGPIODir(gpio_lat,  mraa::DIR_OUT);
	SetGPIO(gpio_lat,  LOW);

	gpio_clk = new mraa::Gpio(m_Pins.CLK);
	SetGPIODir(gpio_clk,  mraa::DIR_OUT);
	SetGPIO(gpio_clk,  LOW);

	gpio_oe = new mraa::Gpio(m_Pins.OE);
	SetGPIODir(gpio_oe,  mraa::DIR_OUT);
	SetGPIO(gpio_oe,  HIGH);
}

void MTK7688RGBmatrixPanel::begin(void)
{


//    // Set up Timer for interrupt ??????
//    /* 1 tick take 1/(80MHZ/80) = 1us so we set divider 80 and count up */
//    displayUpdateTimer2 = timerBegin(0, 80, true);
//    /* Attach onTimer function to our timer */
//    timerAttachInterrupt(displayUpdateTimer2, &onDisplayUpdate2, true);
//    // Repeat the alarm (third parameter)
//    timerAlarmWrite(displayUpdateTimer2, 10, true);
//    // Start an alarm
//    timerAlarmEnable(displayUpdateTimer2);
}

void MTK7688RGBmatrixPanel::StopTimer(bool b)
{
//    timerDetachInterrupt(displayUpdateTimer2);
//    timerStop(displayUpdateTimer2);
//    if(b)
//        timerAlarmDisable(displayUpdateTimer2);
//    else
//        timerAlarmEnable(displayUpdateTimer2);
//    timerEnd(displayUpdateTimer2);
//    displayUpdateTimer2 = NULL;
//    Serial.println("Timer stopped");
}


void MTK7688RGBmatrixPanel::drawPixel(int16_t x, int16_t y, uint16_t c)
{
	if (x < 0 || x >= HEIGHT)
	    return;
	if (y < 0 || y >= WIDTH)
	    return;

	color* pixel = &pixelBuf[0][x * WIDTH  + y]; //backindex

	pixel->r = ((c & rmask)) << 4;
	pixel->g = ((c & gmask));
	pixel->b = ((c & bmask) >> 4);
}

void MTK7688RGBmatrixPanel::drawPixel(int16_t x, int16_t y, uint8 r, uint8 g, uint8 b)
{
	  drawPixel(x, y, AdafruitColor(r, g, b));
}

void MTK7688RGBmatrixPanel::fillScreen(uint16_t c)
{
    if((c == 0x0000) || (c == 0xffff))
    {
        // For black or white, all bits in frame buffer will be identically
        // set or unset (regardless of weird bit packing), so it's OK to just
        // quickly memset the whole thing:

        memset((void*)pixelBuf[backindex], c, WIDTH * HEIGHT * sizeof(color));
    } else {
        // Otherwise, need to handle it the long way:
        Adafruit_GFX::fillScreen(c);
    }
}

void MTK7688RGBmatrixPanel::black()
{
    memset((void*)pixelBuf[backindex], Colors::BLACK, WIDTH * HEIGHT * sizeof(color));
}

int16_t MTK7688RGBmatrixPanel::AdafruitColor(uint8 r, uint8 g, uint8 b)
{
  	int16_t  c = 0;
  	c = r >> 4;
  	c |= (g >> 4) << 4;
  	c |= (b >> 4) << 8;
  	return c;
}

void MTK7688RGBmatrixPanel::setBrightness(byte brightness)
{
    if (brightness < 0) 
        brightness = 0;
    if (brightness >= loops) 
        brightness = loops;
  
    if (brightness > 0)
        loopNrOn = loops - brightness;
    else //never ON
        loopNrOn = 255;
}

void MTK7688RGBmatrixPanel::updateDisplay()
{
//    update();
//    on();
//    usleep(0);
//    return;
    
    if (loopNr == 0) 
        update();     //Display OFF-time (25s). 
    if (loopNr == loopNrOn)
        on();       //Turn Display ON
    if (++loopNr >= loops - 9)//9
    {
    		usleep(1);
//      NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();
//      NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();
//      NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();
//      NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();
//      NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();
//      NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();
//      NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();
//      NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();
//      NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();
//      NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();
//      NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();
//      NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();
//      NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();
//      NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();NOP5();
      loopNr = 0;
    }
}


void MTK7688RGBmatrixPanel::update()
{
    buffptr = pixelBuf[0]; // default 0
    
    SetGPIO(gpio_oe, HIGH);

    SetGPIO(gpio_cha, row & 1 << 0);
    SetGPIO(gpio_chb, row & 1 << 1);
    SetGPIO(gpio_chc, row & 1 << 2);
    if(HEIGHT > 16)
        SetGPIO(gpio_chd, row & 1 << 3);
    if(HEIGHT > 32)
        SetGPIO(gpio_che, row & 1 << 4);

    // RESET timer duration
//    timerWrite(displayUpdateTimer2, duration/100);
//    timerAlarmWrite(displayUpdateTimer2, 80, true);

    uint16_t hi_index, lo_index;
    for(uint16_t column = 0; column < WIDTH; column++)
    {
        hi_index = row * WIDTH + column;
        lo_index = ((row + HEIGHT/2) * WIDTH) + column;

//        cout  << hex << (short)buffptr[hi_index].r << ", " << hex <<  (short)buffptr[hi_index].g << ", " << hex << (short)buffptr[hi_index].b << endl;
        SetColorPin(gpio_r1, buffptr[hi_index].r);
        SetColorPin(gpio_g1, buffptr[hi_index].g);
        SetColorPin(gpio_b1, buffptr[hi_index].b);
        SetColorPin(gpio_r2, buffptr[lo_index].r);
        SetColorPin(gpio_g2, buffptr[lo_index].g);
        SetColorPin(gpio_b2, buffptr[lo_index].b);

        SetGPIO(gpio_clk, HIGH);
        SetGPIO(gpio_clk, LOW);
    }

	SetGPIO(gpio_lat, HIGH);
	SetGPIO(gpio_lat, LOW);
//    SetGPIO(LAT, LOW);
//     SetGPIO(OE, LOW);

    row ++;
    if (row % 32==0)
    {
        row = 0;
        layer += layerStep;

        // Swap front/back buffers if requested
        if(swapflag)
        {
            backindex = 1 - backindex;
            swapflag  = false;
        }
        buffptr = pixelBuf[1-backindex];  // Reset into front buffer
    }
    if (layer + 1 >= layers) 
        layer = layerStep - 1;
}

void MTK7688RGBmatrixPanel::on()
{
    SetGPIO(gpio_oe, LOW);
}


// For smooth animation -- drawing always takes place in the "back" buffer;
// this method pushes it to the "front" for display.  Passing "true", the
// updated display contents are then copied to the new back buffer and can
// be incrementally modified.  If "false", the back buffer then contains
// the old front buffer contents -- your code can either clear this or
// draw over every pixel.  (No effect if double-buffering is not enabled.)
void MTK7688RGBmatrixPanel::swapBuffers(bool copy)
{
    uint32_t len = sizeof(color) * HEIGHT * WIDTH;
    color *ptrBack = pixelBuf[backindex];
    color *ptrFront = pixelBuf[1-backindex];

    if(memcmp( ptrBack, ptrFront, len ) != 0)
    {
        // To avoid 'tearing' display, actual swap takes place in the interrupt
        // handler, at the end of a complete screen refresh cycle.
        swapflag = true;                  // Set flag here, then...
        // wait for interrupt to clear it
        while(swapflag)
        		usleep(1000);

        if(copy)
        {
            memcpy(ptrFront, ptrBack, len);
        }
    }
}

// Return address of back buffer -- can then load/store data directly
color* MTK7688RGBmatrixPanel::backBuffer()
{
    return (color*)pixelBuf[backindex];
}

