#include "MTK7688RGBmatrixPanel.h"

#include <iostream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>


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


//#include <vector>
//
//static volatile uint8_t *timer1Mhz = NULL;
//
//static void sleep_nanos_rpi_1(long nanos);
////static void sleep_nanos_rpi_2(long nanos);
//static void (*busy_sleep_impl)(long) = sleep_nanos_rpi_1;
//class Timers {
//public:
//  static bool Init();
//  static void sleep_nanos(long t);
//};
//void Timers::sleep_nanos(long nanos) {
//  // For smaller durations, we go straight to busy wait.
//
//  // For larger duration, we use nanosleep() to give the operating system
//  // a chance to do something else.
//  // However, these timings have a lot of jitter, so we do a two way
//  // approach: we use nanosleep(), but for some shorter time period so
//  // that we can tolerate some jitter (also, we need at least an offset of
//  // EMPIRICAL_NANOSLEEP_OVERHEAD_US as the nanosleep implementations on RPi
//  // actually have such offset).
//  //
//  // We use the global 1Mhz hardware timer to measure the actual time period
//  // that has passed, and then inch forward for the remaining time with
//  // busy wait.
//  static long kJitterAllowanceNanos = 25 * 1000;
//  if (nanos > kJitterAllowanceNanos + 5000) {
//    const uint32_t before = *timer1Mhz;
//    struct timespec sleep_time
//      = { 0, nanos - kJitterAllowanceNanos };
//    nanosleep(&sleep_time, NULL);
//    const uint32_t after = *timer1Mhz;
//    const long nanoseconds_passed = 1000 * (uint32_t)(after - before);
//    if (nanoseconds_passed > nanos) {
//      return;  // darn, missed it.
//    } else {
//      nanos -= nanoseconds_passed; // remaining time with busy-loop
//    }
//  }
//
//  busy_sleep_impl(nanos);
//}
//
//static void sleep_nanos_rpi_1(long nanos) {
//  if (nanos < 70) return;
//  // The following loop is determined empirically on a 700Mhz RPi
//  for (uint32_t i = (nanos - 70) >> 2; i != 0; --i) {
//    asm("nop");
//  }
//}
//
//bool Timers::Init() {
////  const bool isRPi2 = IsRaspberryPi2();
////  uint32_t *timereg = mmap_bcm_register(COUNTER_1Mhz_REGISTER_OFFSET);
//
//  uint8_t *timereg = 0;//mmap_bcm_register(0);
////  timereg += COUNTER_1Mhz_REGISTER_OFFSET;
//  if (timereg == NULL) {
//    return false;
//  }
//  timer1Mhz = timereg + 1;
//
////  busy_sleep_impl = isRPi2 ? sleep_nanos_rpi_2 : sleep_nanos_rpi_1;
////  if (isRPi2) DisableRealtimeThrottling();
//  busy_sleep_impl = sleep_nanos_rpi_1;
//  return true;
//}
//
//class TimerBasedPinPulser {
//public:
//  TimerBasedPinPulser(MTK7688GPIO &Gpio) : m_Gpio(Gpio)
//  {
//	for (int b = 0; b < 11; ++b)
//		nano_specs_.push_back((130 << b));
//
//  }
//
//  void WaitPulseFinished() {}
//  virtual void SendPulse(int time_spec_number) {
//    m_Gpio.SetPin(3, LOW);
//	Timers::sleep_nanos(nano_specs_[time_spec_number]);
//    m_Gpio.SetPin(3, HIGH);
//  }
//
//private:
//  MTK7688GPIO& m_Gpio;
//  std::vector<int> nano_specs_;
//};

void* MTK7688RGBmatrixPanel::DisplayCallBack( void *ptr )
{

	MTK7688RGBmatrixPanel *matrix = (MTK7688RGBmatrixPanel*)ptr;
	while(!matrix->m_Exit)
	{
		struct timeval start, end;
		gettimeofday(&start, NULL);
		matrix->updateDisplay();
		gettimeofday(&end, NULL);
		int64_t usec = ((uint64_t)end.tv_sec * 1000000 + end.tv_usec)
		  - ((int64_t)start.tv_sec * 1000000 + start.tv_usec);
		printf("\b\b\b\b\b\b\b\b%6.1fHz,", 1e6 / usec);

		printf("diff %ld\n",end.tv_usec-start.tv_usec);
	}

	return 0;
}

void MTK7688RGBmatrixPanel::Wait(void)
{
	pthread_join( m_thread1, NULL);
}

void MTK7688RGBmatrixPanel::SetColorPin(int gpio,  uint16_t val)
{
#if defined(GAMMA_CORRECTION)
	m_Gpio.SetPin(gpio, gamma8[val] > layer ? HIGH : LOW);
#else
	m_Gpio.SetPin(gpio, val > layer ? HIGH : LOW);
#endif
}

MTK7688RGBmatrixPanel::MTK7688RGBmatrixPanel(const struct RGBPin Pins, uint16_t width, uint16_t height)
  : Adafruit_GFX(width, height)
  , loopNr(0), loopNrOn(0), row(0), plane(0), WIDTH(width), HEIGHT(height)
  , EPortAvailable(false), swapflag(false), backindex(0), m_Exit(false)
{
	memcpy(&m_Pins, &Pins, sizeof(RGBPin));
	layerStep = 256 / (1 << colorDepthPerChannel);
	layer = layerStep - 1;
    init();
}

MTK7688RGBmatrixPanel::~MTK7688RGBmatrixPanel()
{

}

void MTK7688RGBmatrixPanel::init(void)
{
    initGPIO();

    swapflag  = false;
    plane = PLANES - 1;
    row = 0;//ROWS - 1;

    // Allocate and initialize matrix buffer:
    int buffsize  = WIDTH * HEIGHT * sizeof(color);
    pixelBuf[0] = (color *)malloc(buffsize);
    pixelBuf[1] = (color *)malloc(buffsize);
    memset(pixelBuf[0], 0, buffsize);
    memset(pixelBuf[1], 0, buffsize);
    // If not double-buffered, both buffers then point to the same address:

    // Array index of back buffer
    backindex = 0;                      // Back buffer
    buffptr = pixelBuf[1 - backindex];  // Front buffer
    activePanel = this;                 // For interrupt hander

	/* Create independent threads each of which will execute function */

	int iret1 = pthread_create( &m_thread1, NULL, DisplayCallBack, (void*) this);
	if(iret1)
	{
		fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
	}
}

void MTK7688RGBmatrixPanel::initGPIO()
{
    // Enable all comm & address pins as outputs, set default states:

	if( m_Gpio.InitMmap() < 0 )
	{
		cout << "Initial Memory Map failed!!!" << endl;
		return;
	}
	//
	// create r1,b1,g1,r2,b2 and g2 data lines
	//
	m_Gpio.SetPinDir(m_Pins.R1, MTK7688GPIO::DIR_OUT);
	m_Gpio.SetPin(m_Pins.R1, LOW);
	m_Gpio.SetPinDir(m_Pins.G1, MTK7688GPIO::DIR_OUT);
	m_Gpio.SetPin(m_Pins.G1, LOW);
	m_Gpio.SetPinDir(m_Pins.B1, MTK7688GPIO::DIR_OUT);
	m_Gpio.SetPin(m_Pins.B1, LOW);
	m_Gpio.SetPinDir(m_Pins.R2, MTK7688GPIO::DIR_OUT);
	m_Gpio.SetPin(m_Pins.R2, LOW);
	m_Gpio.SetPinDir(m_Pins.G2, MTK7688GPIO::DIR_OUT);
	m_Gpio.SetPin(m_Pins.G2, LOW);
	m_Gpio.SetPinDir(m_Pins.B2, MTK7688GPIO::DIR_OUT);
	m_Gpio.SetPin(m_Pins.B2, LOW);

	//
	// decide A,B,C,D and E ports
	//
	// 1
	m_Gpio.SetPinDir(m_Pins.CHA, MTK7688GPIO::DIR_OUT);
	m_Gpio.SetPin(m_Pins.CHA, LOW);
	// 2
	m_Gpio.SetPinDir(m_Pins.CHB, MTK7688GPIO::DIR_OUT);
	m_Gpio.SetPin(m_Pins.CHB, LOW);
	// 4
	m_Gpio.SetPinDir(m_Pins.CHC, MTK7688GPIO::DIR_OUT);
	m_Gpio.SetPin(m_Pins.CHC, LOW);

    if(HEIGHT > 16)
    {
    		// 8
    		m_Gpio.SetPinDir(m_Pins.CHD, MTK7688GPIO::DIR_OUT);
    		m_Gpio.SetPin(m_Pins.CHD, LOW);
    }
    if(HEIGHT > 32)
    {
    		// 16
    		m_Gpio.SetPinDir(m_Pins.CHE, MTK7688GPIO::DIR_OUT);
    		m_Gpio.SetPin(m_Pins.CHE, LOW);
        EPortAvailable = true;
    }

	//
	// create lat,clk and oe control functions
	//
    m_Gpio.SetPinDir(m_Pins.LAT, MTK7688GPIO::DIR_OUT);
    	m_Gpio.SetPin(m_Pins.LAT, LOW);
    	m_Gpio.SetPinDir(m_Pins.CLK, MTK7688GPIO::DIR_OUT);
	m_Gpio.SetPin(m_Pins.CLK, LOW);
	m_Gpio.SetPinDir(m_Pins.OE, MTK7688GPIO::DIR_OUT);
	m_Gpio.SetPin(m_Pins.OE, HIGH);
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

	color* pixel = &pixelBuf[backindex][x * WIDTH  + y]; //backindex

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
//    NOP5();
//    return;
    
    if (loopNr == 0) 
        update();     //Display OFF-time (25s). 
    if (loopNr == loopNrOn)
        on();       //Turn Display ON
    if (++loopNr >= loops - 9)//9
    {
    		usleep(1);
    		loopNr = 0;
    }
}


void MTK7688RGBmatrixPanel::update()
{
    buffptr = pixelBuf[0]; // default 0
    
//    TimerBasedPinPulser OEPin(m_Gpio);
//    m_Gpio.SetPin(m_Pins.OE, HIGH);

    m_Gpio.SetPin(m_Pins.CHA, row & 1 << 0);
    m_Gpio.SetPin(m_Pins.CHB, row & 1 << 1);
    m_Gpio.SetPin(m_Pins.CHC, row & 1 << 2);
    if(HEIGHT > 16)
    		m_Gpio.SetPin(m_Pins.CHD, row & 1 << 3);
    if(HEIGHT > 32)
    		m_Gpio.SetPin(m_Pins.CHE, row & 1 << 4);

    // RESET timer duration
//    timerWrite(displayUpdateTimer2, duration/100);
//    timerAlarmWrite(displayUpdateTimer2, 80, true);

    uint16_t hi_index, lo_index;
    for(uint16_t column = 0; column < WIDTH; column++)
    {
        hi_index = row * WIDTH + column;
        lo_index = ((row + HEIGHT/2) * WIDTH) + column;

//        cout  << hex << (short)buffptr[hi_index].r << ", " << hex <<  (short)buffptr[hi_index].g << ", " << hex << (short)buffptr[hi_index].b << endl;
        SetColorPin(m_Pins.R1, buffptr[hi_index].r);
        SetColorPin(m_Pins.G1, buffptr[hi_index].g);
        SetColorPin(m_Pins.B1, buffptr[hi_index].b);
        SetColorPin(m_Pins.R2, buffptr[lo_index].r);
        SetColorPin(m_Pins.G2, buffptr[lo_index].g);
        SetColorPin(m_Pins.B2, buffptr[lo_index].b);

        m_Gpio.SetPin(m_Pins.CLK, HIGH);
        m_Gpio.SetPin(m_Pins.CLK, LOW);
    }

//    OEPin.WaitPulseFinished();
    m_Gpio.SetPin(m_Pins.LAT, HIGH);
	m_Gpio.SetPin(m_Pins.LAT, LOW);
	m_Gpio.SetPin(m_Pins.OE, HIGH);
//	OEPin.SendPulse(layer);

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
	m_Gpio.SetPin(m_Pins.OE, LOW);
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

