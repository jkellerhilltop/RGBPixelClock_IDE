// This #include statement was automatically added by the Particle IDE.
#include "RGBmatrixPanel/RGBmatrixPanel.h"

// This #include statement was automatically added by the Particle IDE.
#include "SparkIntervalTimer/SparkIntervalTimer.h"

// This #include statement was automatically added by the Particle IDE.
#include "Adafruit_mfGFX/Adafruit_mfGFX.h"

// This #include statement was automatically added by the Particle IDE.
#include "RGBPixelClock/RGBPixelClock.h"

/*  RGB Pong Clock
**  Adapted by Paul Kourany, @peekay123 from Andrew Holmes @pongclock
**  Inspired by, and shamelessly derived from 
**      Nick's LED Projects
**  https://123led.wordpress.com/about/
**  
**  Videos of the clock in action:
**  https://vine.co/v/hwML6OJrBPw
**  https://vine.co/v/hgKWh1KzEU0
**  https://vine.co/v/hgKz5V0jrFn
**
**  Please refer to  https://github.com/pkourany/RGBPixelClock_IDE for details
*/

/* !!!!  In order for this app to compile correctly, the following Web IDE libraries MUST be attched:   !!!
**  Adafruit_mfGFX
**  SparkIntervalTimer
**  RGBmatrixPanel
*/

#include "RGBPixelClock/RGBPixelClock.h"   // Core graphics library
#include "RGBPixelClock/fix_fft.h"
#include "RGBPixelClock/blinky.h"
#include "RGBPixelClock/font3x5.h"
#include "RGBPixelClock/font5x5.h"


SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);


// allow us to use itoa() in this scope
extern char* itoa(int a, char* buffer, unsigned char radix);


#define		RGBPCversion	"V1.1.2"

//#define DEBUGME

#ifdef DEBUGME
	#define DEBUGp(message)		Serial.print(message)
	#define DEBUGpln(message)	Serial.println(message)
#else
	#define DEBUGp(message)
	#define DEBUGpln(message)
#endif

/********** DEMO Mode definitions **********/
//#define DEMOMODE
#define WIRELESS_JUMPER	DAC
boolean demoActive;
/*******************************************/

// Modify for version of RGBShieldMatrix that you have
// HINT: Maker Faire 2016 Kit and later have shield version 4 (3 prior to that)
//
// NOTE: Version 4 of the RGBMatrix Shield only works with Photon and Electron (not Core)
#define RGBSHIELDVERSION	4


/** Define RGB matrix panel GPIO pins **/
#if (RGBSHIELDVERSION == 4)		// Newest shield with SD socket onboard
	#warning "new shield"
	#define CLK D6
	#define OE  D7
	#define LAT TX
	#define A   A0
	#define B   A1
	#define C   A2
	#define D	RX
#else
	#warning "old shield"
	#define CLK D6
	#define OE  D7
	#define LAT A4
	#define A   A0
	#define B   A1
	#define C   A2
	#define D	A3
#endif
/****************************************/


#define MAX_CLOCK_MODE		9                 // Number of clock modes

/********** RGB565 Color definitions **********/
#define Black           0x0000
#define Navy            0x000F
#define DarkGreen       0x03E0
#define DarkCyan        0x03EF
#define Maroon          0x7800
#define Purple          0x780F
#define Olive           0x7BE0
#define LightGrey       0xC618
#define DarkGrey        0x7BEF
#define Blue            0x001F
#define Green           0x07E0
#define Cyan            0x07FF
#define Red             0xF800
#define Magenta         0xF81F
#define Yellow          0xFFE0
#define White           0xFFFF
#define Orange          0xFD20
#define GreenYellow     0xAFE5
#define Pink			0xF81F
/**********************************************/






/***** Weather webhook definitions *****/
#define HOOK_RESP	"hook-response/"	// specify your hook event name here
#define HOOK_PUB	""		// and here
#define DEFAULT_CITY	"\"mycity\":\",\""	// Change to desired default city,state
#define API_KEY		"\"apikey\":\"\""// Add your API key here
#define UNITS		"\"units\":\"imperial\""		// Change to "imperial" for farenheit units
/***************************************/

/***** Create RGBmatrix Panel instance *****
 Last parameter = 'true' enables double-buffering, for flicker-free,
 buttery smooth animation.  Note that NOTHING WILL SHOW ON THE DISPLAY
 until the first call to swapBuffers().  This is normal. */
//RGBmatrixPanel matrix(A, B, C, CLK, LAT, OE, true, 32);	// 16x32
RGBmatrixPanel matrix(A, B, C, D,CLK, LAT, OE, true, 32);	// 32x32
//RGBmatrixPanel matrix(A, B, C, D,CLK, LAT, OE, true, 64); // 64x32
/*******************************************/


/************  Get weather definitions **********/
boolean weatherGood=false;
int badWeatherCall;
char w_temp[8][7] = {"15","18","20","-10","0","10","25","30"};			//Presets for demo (if demo mode enabled)
char w_id[8][4] = {"201","502","800","600","802","211","801","800"};

// This could be an EEPROM preset
char city[40] = DEFAULT_CITY;											// Set default city for weather webhook

boolean wasWeatherShownLast = true;
unsigned long lastWeatherTime = 0;
/***********************************************/


/**** Command parsing and clock mode definitions ****/
int stringPos;
int mode_changed = 0;			// Flag indicating mode changed.
bool mode_quick = false;		// Quick weather display
int clock_mode = 1;				// Default clock mode (1 = pong)
uint16_t showClock = 60000;		// Default time to show a clock face in seconds
unsigned long modeSwitch;
unsigned long updateCTime;		// 24hr timer for resyncing cloud time
/***************************************************/


/********** PACMAN definitions **********/
//#define usePACMAN		// Uncomment to enable PACMAN animations

#ifdef usePACMAN
	#define BAT1_X 2		// Pong left bat x pos (this is where the ball collision occurs, the bat is drawn 1 behind these coords)
	#define BAT2_X 28        

	#define X_MAX 31		// 32   Matrix X max LED coordinate (for 2 displays placed next to each other)
	#define Y_MAX 31		// 15

	int powerPillEaten = 0;
#endif
/****************************************/


/************  FFT definitions **********/
//#define useFFT			// Uncomment to enable FFT clock

#if defined (useFFT) && !defined (DEMOMODE)
	// Define MIC input pin
	#define MIC DAC			// DAC is only input pin available

	int8_t im[128];
	int8_t fftdata[128];
	int8_t spectrum[32];

	byte
	peak[32],		// Peak level of each column; used for falling dots
	dotCount = 0,	// Frame counter for delaying dot-falling speed
	colCount = 0;	// Frame counter for storing past column data

	int8_t
	col[32][10],	// Column levels for the prior 10 frames
	minLvlAvg[32],	// For dynamic adjustment of low & high ends of graph,
	maxLvlAvg[32];	// pseudo rolling averages for the prior few frames.
#endif
/***************************************/


/************  PLASMA definitions **********/
static const int8_t PROGMEM sinetab[256] = {
     0,   2,   5,   8,  11,  15,  18,  21,
    24,  27,  30,  33,  36,  39,  42,  45,
    48,  51,  54,  56,  59,  62,  65,  67,
    70,  72,  75,  77,  80,  82,  85,  87,
    89,  91,  93,  96,  98, 100, 101, 103,
   105, 107, 108, 110, 111, 113, 114, 116,
   117, 118, 119, 120, 121, 122, 123, 123,
   124, 125, 125, 126, 126, 126, 126, 126,
   127, 126, 126, 126, 126, 126, 125, 125,
   124, 123, 123, 122, 121, 120, 119, 118,
   117, 116, 114, 113, 111, 110, 108, 107,
   105, 103, 101, 100,  98,  96,  93,  91,
    89,  87,  85,  82,  80,  77,  75,  72,
    70,  67,  65,  62,  59,  56,  54,  51,
    48,  45,  42,  39,  36,  33,  30,  27,
    24,  21,  18,  15,  11,   8,   5,   2,
     0,  -3,  -6,  -9, -12, -16, -19, -22,
   -25, -28, -31, -34, -37, -40, -43, -46,
   -49, -52, -55, -57, -60, -63, -66, -68,
   -71, -73, -76, -78, -81, -83, -86, -88,
   -90, -92, -94, -97, -99,-101,-102,-104,
  -106,-108,-109,-111,-112,-114,-115,-117,
  -118,-119,-120,-121,-122,-123,-124,-124,
  -125,-126,-126,-127,-127,-127,-127,-127,
  -128,-127,-127,-127,-127,-127,-126,-126,
  -125,-124,-124,-123,-122,-121,-120,-119,
  -118,-117,-115,-114,-112,-111,-109,-108,
  -106,-104,-102,-101, -99, -97, -94, -92,
   -90, -88, -86, -83, -81, -78, -76, -73,
   -71, -68, -66, -63, -60, -57, -55, -52,
   -49, -46, -43, -40, -37, -34, -31, -28,
   -25, -22, -19, -16, -12,  -9,  -6,  -3
};

const float radius1  = 65.2, radius2  = 92.0, radius3  = 163.2, radius4  = 176.8,
            centerx1 = 64.4, centerx2 = 46.4, centerx3 =  93.6, centerx4 =  16.4, 
            centery1 = 34.8, centery2 = 26.0, centery3 =  56.0, centery4 = -11.6;
float       angle1   =  0.0, angle2   =  0.0, angle3   =   0.0, angle4   =   0.0;
long        hueShift =  0;
/*******************************************/


/************ Conway Life definitions **********/
const uint8_t kMatrixWidth = 32;
const uint8_t kMatrixHeight = 32;

const uint16_t colorPalette[] = {Navy, DarkGreen, DarkCyan, Maroon, Purple, Olive, LightGrey, DarkGrey, Blue, Green, Cyan, Red, Magenta, Yellow, White, Orange, GreenYellow, Pink};

class Cell {
public:
  byte alive =  1;
  byte prev =  1;
};

Cell world[kMatrixWidth][kMatrixHeight];
/**********************************************/



/************ PROTOTYPES **************/
bool IsDST(int dayOfMonth, int month, int dayOfWeek);
int setMode(String command);
void quickWeather();
void getWeather();
void processWeather(const char *name, const char *data);
void showWeather();
void drawWeatherIcon(uint8_t x, uint8_t y, int id);
void scrollBigMessage(char *m);
void scrollMessage(char* top, char* bottom ,uint8_t top_font_size,uint8_t bottom_font_size, uint16_t top_color, uint16_t bottom_color);
void pacClear();
void pacMan();
void drawPac(int x, int y, int z);
void drawGhost( int x, int y, int color);
void drawScaredGhost( int x, int y);
void cls();
void pong();
byte pong_get_ball_endpoint(float tempballpos_x, float  tempballpos_y, float  tempballvel_x, float tempballvel_y);
void normal_clock();
void vectorNumber(int n, int x, int y, int color, float scale_x, float scale_y);
void word_clock();
void jumble();
void display_date();
void flashing_cursor(byte xpos, byte ypos, byte cursor_width, byte cursor_height, byte repeats);
void drawString(int x, int y, char* c,uint8_t font_size, uint16_t color);
void drawChar(int x, int y, char c, uint8_t font_size, uint16_t color);
int calc_font_dixsplacement(uint8_t font_size);
void spectrumDisplay();
void plasma();
void marquee();
void randomFillWorld();
int neighbours(int x, int y);
void conwayLife();
void rainbow();
/*************************************/




void setup() {

	unsigned long resetTime;

	demoActive = false;
    randomSeed(analogRead(0));  

#if defined (DEMOMODE)
	pinMode(WIRELESS_JUMPER, INPUT_PULLUP);

	if (!digitalRead(WIRELESS_JUMPER)) {
		demoActive = true;	//Wireless is disabled so go into demo mode
	}
#endif	

	
#if defined (DEBUGME)
	Serial.begin(115200);
#endif

	matrix.begin();
	matrix.setTextWrap(false); // Allow text to run off right edge
	matrix.setTextSize(1);
	matrix.setTextColor(matrix.Color333(210, 210, 210));

	if (!demoActive) {
		Particle.connect();
		while (!Particle.connected()) Particle.process();				// !!!!! Add timeout code here if can't connect ?
		
#if defined (DEBUGME)
		Particle.variable("city", city, STRING);						// !!! FOR DEBUGGING ONLY !!!
		Particle.variable("cmode", &clock_mode, INT);
#endif
		Particle.variable("RGBPCversion", RGBPCversion, STRING);
		
		Particle.function("setMode", setMode);							// Receive mode commands
		Particle.subscribe(HOOK_RESP, processWeather, MY_DEVICES);		// Lets listen for the hook response
		
		Particle.publish("RGBPongClock", RGBPCversion, 60, PRIVATE);	// Publish app version
		Particle.process();												// Force processing of Particle.publish()

		do {															// wait for correct epoc time, but not longer than 10 seconds
			delay(10);
		} while (Time.year() <= 1970 && millis() < 10000); 
		
		if (millis() < 10000) {
			DEBUGpln("Unable to sync time");
		}
		else {
			DEBUGpln("RTC has set been synced");
		}
	}


#if defined (useFFT) && !defined (DEMOMODE)
	memset(peak, 0, sizeof(peak));
	memset(col , 0, sizeof(col));

	for(uint8_t i=0; i<32; i++) {
		minLvlAvg[i] = 0;
		maxLvlAvg[i] = 255;
	}
#endif

	randomSeed(analogRead(A7));
	
	//*** RESTORE CITY FROM EEPROM - IF NOT PREVIOUSLY FLASHED, STORE DEFAULT CITY

//	pacMan();
//	quickWeather();

	clock_mode = random(0,MAX_CLOCK_MODE-1);
	modeSwitch = millis();
	badWeatherCall = 0;			// counts number of unsuccessful webhook calls, reset after 3 failed calls
	updateCTime = millis();		// Reset 24hr cloud time refresh counter
}


void loop(){

	// Add wifi/cloud connection retry code here

	// Re-sync cloud time every 24 hrs
	if ((millis() - updateCTime) > (24UL * 60UL * 60UL * 1000UL)) {
		Particle.syncTime();
		updateCTime = millis();
	}
	
	// DST adjstment
	bool daylightSavings = IsDST(Time.day(), Time.month(), Time.weekday());
	Time.zone(daylightSavings? -4 : -5);

	if (millis() - modeSwitch > 300000UL) {	//Switch modes every 5 mins
		clock_mode++;
		mode_changed = 1;
		modeSwitch = millis();
		if (clock_mode > MAX_CLOCK_MODE-1) {
			clock_mode = 0;
		}
		DEBUGp("Switch mode to ");
		DEBUGpln(clock_mode);
	}
	
	DEBUGp("in loop ");
	DEBUGpln(millis());
	//reset clock type clock_mode
	switch (clock_mode){
	case 0: 
		normal_clock();
		break; 
	case 1: 
		normal_clock();
		break;
	case 2: 
		normal_clock();
		break;
	case 3: 
		normal_clock(); 
		break; 
	case 4: 
		normal_clock();
		break;
	case 5: 
		normal_clock();
		break;
	case 6: 
		normal_clock();
		break;
	case 7: 
		normal_clock();
		break;
	case 8:
		normal_clock();
		break;
	default:
		normal_clock();
		break;
	}

	//if the mode hasn't changed, show the date
	pacClear();
	if (mode_changed == 0) { 
		display_date(); 
		pacClear();
	}
	else {
		//the mode has changed, so don't bother showing the date, just go to the new mode.
		mode_changed = 0; //reset mdoe flag.
	}
}

bool IsDST(int dayOfMonth, int month, int dayOfWeek)
{
	if (month < 3 || month > 11) {
		return false;
	}
	if (month > 3 && month < 11) {
		return true;
	}
	int previousSunday = dayOfMonth - (dayOfWeek - 1);
	if (month == 3) {
		return previousSunday >= 8;
	}
	return previousSunday <= 0;
}


int setMode(String command)
{
	mode_changed = 0;

	int j = command.indexOf('=',0);
	if (j>0) {	// "=" is used when setting city only
		if(command.substring(0,j) == "city") {
			unsigned char tmp[20] = "";
			int p = command.length();
			command.getBytes(tmp, (p-j), j+1);
			strcpy(city, "\"mycity\": \"");
			strcat(city, (const char *)tmp);
			strcat(city, "\" ");
			weatherGood = false;
			return 1;
		}
	}
	else if(command == "normal") {
		mode_changed = 1;
		clock_mode = 0;
	}
	else if(command == "pong") {
		mode_changed = 1;
		clock_mode = 0;
	}
	else if(command == "word") {
		mode_changed = 1;
		clock_mode = 0;
	}
	else if(command == "jumble") {
		mode_changed = 1;
		clock_mode = 0;
	}
	else if(command == "spectrum") {
		mode_changed = 1;
		clock_mode = 0;
	}
	else if(command == "quick") {
		mode_quick = true;
		return 1;
	}
	else if(command == "plasma") {
		mode_changed = 1;
		clock_mode = 0;
	}
	else if(command == "marquee") {
		mode_changed = 1;
		clock_mode = 0;
	}
	else if(command == "life") {
		mode_changed = 1;
		clock_mode = 0;
	}
	else if(command == "rainbow") {
		mode_changed = 1;
		clock_mode = 8;
	}
	if (mode_changed == 1) {
		modeSwitch = millis();
		return 1;
	}	  
	else return -1;
	
}


//*****************Weather Stuff*********************

void quickWeather(){ /*
	getWeather();
	if(weatherGood) {
		showWeather();
		//*** If city has changed, then since weatherGood then store city in EEPROM ***
	}
	else {
		cls();
		matrix.drawPixel(0,0,matrix.Color333(1,0,0));
		matrix.swapBuffers(true);
		Particle.process();
		delay(1000);
	} */
}

void getWeather(){
}


void processWeather(const char *name, const char *data){ 
}

void showWeather(){
	byte dow = Time.weekday()-1;
	char daynames[7][4]={"Sun", "Mon","Tue", "Wed", "Thu", "Fri", "Sat"};
	DEBUGpln("in showWeather");

}

void drawWeatherIcon(uint8_t x, uint8_t y, int id) 
{

}
//*****************End Weather Stuff*********************

void scrollBigMessage(char *m){
	matrix.setTextSize(1);
	int l = (strlen(m)*-6) - 32;
	for(int i = 32; i > l; i--){
		cls();
		matrix.setCursor(i,1);
		matrix.setTextColor(matrix.Color444(1,1,1));
		matrix.print(m);
		matrix.swapBuffers(false);
		delay(50);
		Particle.process();
	}

}

void scrollMessage(char* top, char* bottom ,uint8_t top_font_size,uint8_t bottom_font_size, uint16_t top_color, uint16_t bottom_color){

	int l = ((strlen(top)>strlen(bottom)?strlen(top):strlen(bottom))*-5) - 32;
	
	for(int i=32; i > l; i--){
		
		if (mode_changed == 1 || mode_quick) {
			return;
		}

		cls();
		
		drawString(i,1,top,top_font_size, top_color);
		drawString(i,9,bottom, bottom_font_size, bottom_color);
		matrix.swapBuffers(false);
		delay(50);
		Particle.process();
	}
}


//Runs pacman or other animation, refreshes weather data
void pacClear()
{
	DEBUGpln("in pacClear");
	//refresh weather if we havent had it for 30 mins
	//or the last time we had it, it was bad, 
	//or weve never had it before.
	if((millis()>lastWeatherTime+1800000) || lastWeatherTime==0 || !weatherGood) {
	//	getWeather();
	}

	if(!wasWeatherShownLast && weatherGood) {
	//	showWeather();
		wasWeatherShownLast = true;
	}
	else {  
		wasWeatherShownLast = false;
	//	pacMan();
	}
}  


void pacMan() {}

#if defined (usePACMAN)
void drawPac(int x, int y, int z){

}

void drawGhost( int x, int y, int color){

}  

void drawScaredGhost( int x, int y){

}  
#endif 

void cls(){
	matrix.fillScreen(0);
}

void pong(){
    normal_clock();}
void normal_clock()
{
	DEBUGpln("in normal_clock");
	matrix.setTextWrap(false); // Allow text to run off right edge
	matrix.setTextSize(2);
	matrix.setTextColor(matrix.Color333(2, (int)random(2,255), 2)); //check fail
	byte hours = Time.hour();
	byte mins = Time.minute();

	int  msHourPosition = 0;
	int  lsHourPosition = 0;
	int  msMinPosition = 0;
	int  lsMinPosition = 0;      
	int  msLastHourPosition = 0;
	int  lsLastHourPosition = 0;
	int  msLastMinPosition = 0;
	int  lsLastMinPosition = 0;      
    randomSeed(analogRead(A7));
	//Start with all characters off screen
	int c1 = -17;
	int c2 = -17;
	int c3 = -17;
	int c4 = -17;

	float scale_x =2.5;
	float scale_y =3.0;

	char lastHourBuffer[3]="  ";
	char lastMinBuffer[3] ="  ";

	int showTime = Time.now();
	
	while((Time.now() - showTime) < showClock) {
		cls();

		if (mode_changed == 1)
		return;
		if(mode_quick){
			mode_quick = false;
			//display_date();
			//quickWeather();
			normal_clock();
			return;
		}

		//udate mins and hours with the new time
		mins = Time.minute();
		hours = Time.hour();

		char buffer[3];

		itoa(hours,buffer,10);
		//fix - as otherwise if num has leading zero, e.g. "03" hours, itoa coverts this to chars with space "3 ". 
		if (hours < 10) {
			buffer[1] = buffer[0];
			buffer[0] = '0';
		}

		if(lastHourBuffer[0]!=buffer[0] && c1==0) c1= -17;
		if( c1 < 0 )c1++;
		msHourPosition = c1;
		msLastHourPosition = c1 + 17;

		if(lastHourBuffer[1]!=buffer[1] && c2==0) c2= -17;
		if( c2 < 0 )c2++;
		lsHourPosition = c2;
		lsLastHourPosition = c2 + 17;

		//update the display
		//shadows first 
		int coltimes9=(int)random(2,255);
		int coltimes10=(int)random(2,255);
		vectorNumber((lastHourBuffer[0]-'0'), 2, 2+msLastHourPosition, matrix.Color444(coltimes9,(int)random(2,255),1),scale_x,scale_y);
		vectorNumber((lastHourBuffer[1]-'0'), 9, 2+lsLastHourPosition, matrix.Color444(0,(int)random(2,255),1),scale_x,scale_y);
		vectorNumber((buffer[0]-'0'), 2, 2+msHourPosition, matrix.Color444(0,(int)random(2,255),(int)random(2,255)),scale_x,scale_y);
		vectorNumber((buffer[1]-'0'), 9, 2+lsHourPosition, matrix.Color444(0,0,1),scale_x,scale_y); 

		vectorNumber((lastHourBuffer[0]-'0'), 1, 1+msLastHourPosition, matrix.Color444(1,1,1),scale_x,scale_y);
		vectorNumber((lastHourBuffer[1]-'0'), 8, 1+lsLastHourPosition, matrix.Color444(1,(int)random(2,255),1),scale_x,scale_y);
		vectorNumber((buffer[0]-'0'), 1, 1+msHourPosition, matrix.Color444(1,99,99),scale_x,scale_y);
		vectorNumber((buffer[1]-'0'), 8, 1+lsHourPosition, matrix.Color444((int)random(2,255),74,48),scale_x,scale_y);    

		if(c1==0) lastHourBuffer[0]=buffer[0];
		if(c2==0) lastHourBuffer[1]=buffer[1];

		matrix.fillRect(16,5,2,2,matrix.Color444(0,0,Time.second()%2));
		matrix.fillRect(16,11,2,2,matrix.Color444(0,0,Time.second()%2));

		matrix.fillRect(15,4,2,2,matrix.Color444(Time.second()%2,Time.second()%2,Time.second()%2));
		matrix.fillRect(15,10,2,2,matrix.Color444(Time.second()%2,(int)random(2,255),Time.second()%2)); //change me FIXME

		itoa (mins, buffer, 10);
		if (mins < 10) {
			buffer[1] = buffer[0];
			buffer[0] = '0';
		}

		if(lastMinBuffer[0]!=buffer[0] && c3==0) c3= -17;
		if( c3 < 0 )c3++;
		msMinPosition = c3;
		msLastMinPosition= c3 + 17;

		if(lastMinBuffer[1]!=buffer[1] && c4==0) c4= -17;
		if( c4 < 0 )c4++;
		lsMinPosition = c4;
		lsLastMinPosition = c4 + 17;
        int coltimes11=(int)random(2,255);
        int coltimes12=(int)random(2,255);
		vectorNumber((buffer[0]-'0'), 19, 2+msMinPosition, matrix.Color444((int)random(2,255),coltimes10,(int)random(2,255)),scale_x,scale_y);
		vectorNumber((buffer[1]-'0'), 26, 2+lsMinPosition, matrix.Color444((int)random(2,255),(int)random(2,255),(int)random(2,255)),scale_x,scale_y);
		vectorNumber((lastMinBuffer[0]-'0'), 19, 2+msLastMinPosition, matrix.Color444(coltimes12,(int)random(2,255),(int)random(2,255)),scale_x,scale_y);
		vectorNumber((lastMinBuffer[1]-'0'), 26, 2+lsLastMinPosition, matrix.Color444((int)random(2,255),coltimes11,(int)random(2,255)),scale_x,scale_y);
        randomSeed(4903286709458670984676856270498567);
		vectorNumber((buffer[0]-'0'), 18, 1+msMinPosition, matrix.Color444((int)random(2,255),(int)random(2,255),(int)random(2,255)),scale_x,scale_y);
		vectorNumber((buffer[1]-'0'), 25, 1+lsMinPosition, matrix.Color444(1,(int)random(2,255),(int)random(2,255)),scale_x,scale_y);
		vectorNumber((lastMinBuffer[0]-'0'), 18, 1+msLastMinPosition, matrix.Color444((int)random(2,255),(int)random(2,255),(int)random(2,255)),scale_x,scale_y);
		vectorNumber((lastMinBuffer[1]-'0'), 25, 1+lsLastMinPosition, matrix.Color444((int)random(2,255),35,40),scale_x,scale_y);
    //    matrix.fillRect(15,15,1,1,matrix.Color444(30,Time.second()%2,Time.second()%2)); //Justin Justin Edit Justin Keller Test
		if(c3==0) lastMinBuffer[0]=buffer[0];
		if(c4==0) lastMinBuffer[1]=buffer[1];

		matrix.swapBuffers(false); 
		Particle.process();	//Give the background process some lovin'
	}
}

//Draw number n, with x,y as top left corner, in chosen color, scaled in x and y.
//when scale_x, scale_y = 1 then character is 3x5
void vectorNumber(int n, int x, int y, int color, float scale_x, float scale_y){

	switch (n){
	case 0:
		matrix.drawLine(x ,y , x , y+(4*scale_y) , color);
		matrix.drawLine(x , y+(4*scale_y) , x+(2*scale_x) , y+(4*scale_y), color);
		matrix.drawLine(x+(2*scale_x) , y , x+(2*scale_x) , y+(4*scale_y) , color);
		matrix.drawLine(x ,y , x+(2*scale_x) , y , color);
		break; 
	case 1: 
		matrix.drawLine( x+(1*scale_x), y, x+(1*scale_x),y+(4*scale_y), color);  
		matrix.drawLine(x , y+4*scale_y , x+2*scale_x , y+4*scale_y,color);
		matrix.drawLine(x,y+scale_y, x+scale_x, y,color);
		break;
	case 2:
		matrix.drawLine(x ,y , x+2*scale_x , y , color);
		matrix.drawLine(x+2*scale_x , y , x+2*scale_x , y+2*scale_y , color);
		matrix.drawLine(x+2*scale_x , y+2*scale_y , x , y+2*scale_y, color);
		matrix.drawLine(x , y+2*scale_y, x , y+4*scale_y,color);
		matrix.drawLine(x , y+4*scale_y , x+2*scale_x , y+4*scale_y,color);
		break; 
	case 3:
		matrix.drawLine(x ,y , x+2*scale_x , y , color);
		matrix.drawLine(x+2*scale_x , y , x+2*scale_x , y+4*scale_y , color);
		matrix.drawLine(x+2*scale_x , y+2*scale_y , x+scale_x , y+2*scale_y, color);
		matrix.drawLine(x , y+4*scale_y , x+2*scale_x , y+4*scale_y,color);
		break;
	case 4:
		matrix.drawLine(x+2*scale_x , y , x+2*scale_x , y+4*scale_y , color);
		matrix.drawLine(x+2*scale_x , y+2*scale_y , x , y+2*scale_y, color);
		matrix.drawLine(x ,y , x , y+2*scale_y , color);
		break;
	case 5:
		matrix.drawLine(x ,y , x+2*scale_x , y , color);
		matrix.drawLine(x , y , x , y+2*scale_y , color);
		matrix.drawLine(x+2*scale_x , y+2*scale_y , x , y+2*scale_y, color);
		matrix.drawLine(x+2*scale_x , y+2*scale_y, x+2*scale_x , y+4*scale_y,color);
		matrix.drawLine( x , y+4*scale_y , x+2*scale_x , y+4*scale_y,color);
		break; 
	case 6:
		matrix.drawLine(x ,y , x , y+(4*scale_y) , color);
		matrix.drawLine(x ,y , x+2*scale_x , y , color);
		matrix.drawLine(x+2*scale_x , y+2*scale_y , x , y+2*scale_y, color);
		matrix.drawLine(x+2*scale_x , y+2*scale_y, x+2*scale_x , y+4*scale_y,color);
		matrix.drawLine(x+2*scale_x , y+4*scale_y , x, y+(4*scale_y) , color);
		break;
	case 7:
		matrix.drawLine(x ,y , x+2*scale_x , y , color);
		matrix.drawLine( x+2*scale_x, y, x+scale_x,y+(4*scale_y), color);
		break;
	case 8:
		matrix.drawLine(x ,y , x , y+(4*scale_y) , color);
		matrix.drawLine(x , y+(4*scale_y) , x+(2*scale_x) , y+(4*scale_y), color);
		matrix.drawLine(x+(2*scale_x) , y , x+(2*scale_x) , y+(4*scale_y) , color);
		matrix.drawLine(x ,y , x+(2*scale_x) , y , color);
		matrix.drawLine(x+2*scale_x , y+2*scale_y , x , y+2*scale_y, color);
		break;
	case 9:
		matrix.drawLine(x ,y , x , y+(2*scale_y) , color);
		matrix.drawLine(x , y+(4*scale_y) , x+(2*scale_x) , y+(4*scale_y), color);
		matrix.drawLine(x+(2*scale_x) , y , x+(2*scale_x) , y+(4*scale_y) , color);
		matrix.drawLine(x ,y , x+(2*scale_x) , y , color);
		matrix.drawLine(x+2*scale_x , y+2*scale_y , x , y+2*scale_y, color);
		break;    
	}
}


//print a clock using words rather than numbers
void word_clock() {
	DEBUGpln("in word_clock");
	cls();
}


//show time and date and use a random jumble of letters transition each time the time changes.
void jumble() {
}

void display_date()
{
	DEBUGpln("in display_date");
	uint16_t color = matrix.Color333(0,1,0);
	cls();
	matrix.swapBuffers(true);
	
	byte dow = Time.weekday()-1;		// Take one off the weekday, as array of days is 0-6 and weekday outputs  1-7.
	byte date = Time.day();
	byte mont = Time.month()-1;

	//array of day and month names to print on the display. Some are shortened as we only have 8 characters across to play with 
	char daynames[7][9]={"Sunday", "Monday","Tuesday", "Wed", "Thursday", "Friday", "Saturday"};
	char monthnames[12][9]={"January", "February", "March", "April", "May", "June", "July", "August", "Sept", "October", "November", "December"};

	//call the flashing cursor effect for one blink at x,y pos 0,0, height 5, width 7, repeats 1
	flashing_cursor(0,0,3,5,1);

	//print the day name
	int i = 0;
	while(daynames[dow][i])
	{
		flashing_cursor(i*4,0,3,5,0);
		drawChar(i*4,0,daynames[dow][i],51,color);
		matrix.swapBuffers(true);
		i++;

		if (mode_changed == 1)
		return;
	}

	//pause at the end of the line with a flashing cursor if there is space to print it.
	//if there is no space left, dont print the cursor, just wait.
	if (i*4 < 32){
		flashing_cursor(i*4,0,3,5,1);  
	} 
	else {
		Particle.process();	//Give the background process some lovin'
		delay(300);
	}

	//flash the cursor on the next line  
	flashing_cursor(0,8,3,5,0);

	//print the date on the next line: First convert the date number to chars
	char buffer[3];
	itoa(date,buffer,10);

	//then work out date 2 letter suffix - eg st, nd, rd, th etc
	char suffix[4][3]={
		"st", "nd", "rd", "th"                    };
	byte s = 3; 
	if(date == 1 || date == 21 || date == 31) {
		s = 0;
	} 
	else if (date == 2 || date == 22) {
		s = 1;
	} 
	else if (date == 3 || date == 23) {
		s = 2;
	} 

	drawChar(0,8,buffer[0],51,color);
	matrix.swapBuffers(true);

	byte suffixposx = 4;

	if (date > 9){
		suffixposx = 8;
		flashing_cursor(4,8,3,5,0); 
		drawChar(4,8,buffer[1],51,color);
		matrix.swapBuffers(true);
	}

	//print the 2 suffix characters
	flashing_cursor(suffixposx, 8,3,5,0);
	drawChar(suffixposx,8,suffix[s][0],51,color);
	matrix.swapBuffers(true);

	flashing_cursor(suffixposx+4,8,3,5,0);
	drawChar(suffixposx+4,8,suffix[s][1],51,color);
	matrix.swapBuffers(true);

	//blink cursor after 
	flashing_cursor(suffixposx + 8,8,3,5,1);  

	//replace day name with date on top line - effectively scroll the bottom line up by 8 pixels
	for(int q = 8; q>=0; q--){
		cls();
		int w =0 ;
		while(daynames[dow][w]) {
			drawChar(w*4,q-8,daynames[dow][w],51,color);
			w++;
		}

		matrix.swapBuffers(true);						// Display, copying display buffer
		drawChar(0,q,buffer[0],51,color);				// date first digit
		drawChar(4,q,buffer[1],51,color);				// date second digit - this may be blank and overwritten if the date is a single number
		drawChar(suffixposx,q,suffix[s][0],51,color);	// date suffix
		drawChar(suffixposx+4,q,suffix[s][1],51,color);	// date suffix
		matrix.swapBuffers(true);
		delay(50);
	}
	//flash the cursor for a second for effect
	flashing_cursor(suffixposx + 8,0,3,5,0);  

	//print the month name on the bottom row
	i = 0;
	while(monthnames[mont][i]) {  
		flashing_cursor(i*4,8,3,5,0);
		drawChar(i*4,8,monthnames[mont][i],51,color);
		matrix.swapBuffers(true);
		i++; 
	}

	//blink the cursor at end if enough space after the month name, otherwise juts wait a while
	if (i*4 < 32){
		flashing_cursor(i*4,8,3,5,2);  
	} 
	else {
		delay(1000);
	}

	for(int q = 8; q>=-8; q--){
		cls();
		int w =0 ;
		while(monthnames[mont][w]) {
			drawChar(w*4,q,monthnames[mont][w],51,color);
			w++;
		}

		matrix.swapBuffers(true);
		drawChar(0,q-8,buffer[0],51,color);				//date first digit
		drawChar(4,q-8,buffer[1],51,color);				//date second digit - this may be blank and overwritten if the date is a single number
		drawChar(suffixposx,q-8,suffix[s][0],51,color);	//date suffix
		drawChar(suffixposx+4,q-8,suffix[s][1],51,color);	//date suffix
		matrix.swapBuffers(true);
		delay(50);
	}
}


/*
* flashing_cursor
* print a flashing_cursor at xpos, ypos and flash it repeats times 
*/
void flashing_cursor(byte xpos, byte ypos, byte cursor_width, byte cursor_height, byte repeats)
{
	for (byte r = 0; r <= repeats; r++) {
		matrix.fillRect(xpos,ypos,cursor_width, cursor_height, matrix.Color333(0,3,0));
		matrix.swapBuffers(true);

		if (repeats > 0) {
			delay(400);
		} 
		else {
			delay(70);
		}

		matrix.fillRect(xpos,ypos,cursor_width, cursor_height, matrix.Color333(0,0,0));
		matrix.swapBuffers(true);

		//if cursor set to repeat, wait a while
		if (repeats > 0) {
			delay(400); 
		}
		Particle.process();	//Give the background process some lovin'
	}
}


void drawString(int x, int y, char* c,uint8_t font_size, uint16_t color)
{
	// x & y are positions, c-> pointer to string to disp, update_s: false(write to mem), true: write to disp
	//font_size : 51(ascii value for 3), 53(5) and 56(8)
	for(uint16_t i=0; i< strlen(c); i++) {
		drawChar(x, y, c[i],font_size, color);
		x+=calc_font_displacement(font_size); // Width of each glyph
	}
}

int calc_font_displacement(uint8_t font_size)
{
	switch(font_size)
	{
	case 51:
		return 4;  //5x3 hence occupies 4 columns ( 3 + 1(space btw two characters))
		break;

	case 53:
		return 6;
		break;
		//case 56:
		//return 6;
		//break;
	default:
		return 6;
		break;
	}
}

void drawChar(int x, int y, char c, uint8_t font_size, uint16_t color)  // Display the data depending on the font size mentioned in the font_size variable
{
	uint8_t dots;

	if ((c >= 'A' && c <= 'Z') ||
			(c >= 'a' && c <= 'z')) {
		c &= 0x1F;   // A-Z maps to 1-26
	} 
	else if (c >= '0' && c <= '9') {
		c = (c - '0') + 27;
	} 
	else if (c == ' ') {
		c = 0; // space
	}
	else if (c == '#'){
		c=37;
	}
	else if (c=='/'){
		c=37;
	}

	switch(font_size)
	{
	case 51:  // font size 3x5  ascii value of 3: 51
		if(c==':'){
			matrix.drawPixel(x+1,y+1,color);
			matrix.drawPixel(x+1,y+3,color);
		}
		else if(c=='-'){
			matrix.drawLine(x,y+2,3,0,color);
		}
		else if(c=='.'){
			matrix.drawPixel(x+1,y+2,color);
		}
		else if(c==39 || c==44){
			matrix.drawLine(x+1,y,2,0,color);
			matrix.drawPixel(x+2,y+1,color);
		}
		else{
			for (uint8_t row=0; row< 5; row++) {
				dots = font3x5[(uint8_t)c][row];
				for (uint8_t col=0; col < 3; col++) {
					int x1=x;
					int y1=y;
					if (dots & (4>>col))
					matrix.drawPixel(x1+col, y1+row, color);
				}    
			}
		}
		break;

	case 53:  // font size 5x5   ascii value of 5: 53

		if(c==':'){
			matrix.drawPixel(x+2,y+1,color);
			matrix.drawPixel(x+2,y+3,color);
		}
		else if(c=='-'){
			matrix.drawLine(x+1,y+2,3,0,color);
		}
		else if(c=='.'){
			matrix.drawPixel(x+2,y+2,color);
		}
		else if(c==39 || c==44){
			matrix.drawLine(x+2,y,2,0,color);
			matrix.drawPixel(x+4,y+1,color);
		}
		else{
			for (uint8_t row=0; row< 5; row++) {
				dots = font5x5[(uint8_t)c][row];
				for (uint8_t col=0; col < 5; col++) {
					int x1=x;
					int y1=y;
					if (dots & (64>>col))  // For some wierd reason I have the 5x5 font in such a way that.. last two bits are zero.. 
					matrix.drawPixel(x1+col, y1+row, color);        
				}
			}
		}          

		break;
	default:
		break;
	}
}


//Spectrum Analyser stuff
void spectrumDisplay(){
#if defined (useFFT) && !defined (DEMOMODE)
	uint8_t static i = 0;
	//static unsigned long tt = 0;
	int16_t val;

	uint8_t  c;
	uint16_t x,minLvl, maxLvl;
	int      level, y, off;

	DEBUGpln("in Spectrum");

	off = 0;

	cls();
	int showTime = Time.now();
	
	while((Time.now() - showTime) < showClock) {
		if (mode_changed == 1)
		return;	
		if(mode_quick){
			mode_quick = false;
			display_date();
			quickWeather();
			spectrumDisplay();
			return;
		}

		if (i < 128){
			val = map(analogRead(MIC),0,4095,0,1023);
			fftdata[i] = (val / 4) - 128;
			im[i] = 0;
			i++;   
		}
		else {
			//this could be done with the fix_fftr function without the im array.
			fix_fft(fftdata,im,7,0);
			//fix_fftr(fftdata,7,0);
			
			// I am only interessted in the absolute value of the transformation
			for (i=0; i< 64;i++){
				fftdata[i] = sqrt(fftdata[i] * fftdata[i] + im[i] * im[i]); 
				//ftdata[i] = sqrt(fftdata[i] * fftdata[i] + fftdata[i+64] * fftdata[i+64]); 
			}

			for (i=0; i< 32;i++){
				spectrum[i] = fftdata[i*2] + fftdata[i*2 + 1];   // average together 
			}

			for(int l=0; l<16;l++){
				int col = matrix.Color444(16-l,0,l);
				matrix.drawLine(0,l,31,l,col);
			}

			// Downsample spectrum output to 32 columns:
			for(x=0; x<32; x++) {
				col[x][colCount] = spectrum[x];
				
				minLvl = maxLvl = col[x][0];
				int colsum=col[x][0];
				for(i=1; i<10; i++) { // Get range of prior 10 frames
					if(i<10)colsum = colsum + col[x][i];
					if(col[x][i] < minLvl)      minLvl = col[x][i];
					else if(col[x][i] > maxLvl) maxLvl = col[x][i];
				}
				if((maxLvl - minLvl) < 16) maxLvl = minLvl + 8;
				minLvlAvg[x] = (minLvlAvg[x] * 7 + minLvl) >> 3; // Dampen min/max levels
				maxLvlAvg[x] = (maxLvlAvg[x] * 7 + maxLvl) >> 3; // (fake rolling average)

				level = col[x][colCount];
				// Clip output and convert to byte:
				if(level < 0L)      c = 0;
				else if(level > 18) c = 18; // Allow dot to go a couple pixels off top
				else                c = (uint8_t)level;

				if(c > peak[x]) peak[x] = c; // Keep dot on top

				if(peak[x] <= 0) { // Empty column?
					matrix.drawLine(x, 0, x, 15, off);
					continue;
				}
				else if(c < 15) { // Partial column?
					matrix.drawLine(x, 0, x, 15 - c, off);
				}

				// The 'peak' dot color varies, but doesn't necessarily match
				// the three screen regions...yellow has a little extra influence.
				y = 16 - peak[x];
				matrix.drawPixel(x,y,matrix.Color444(peak[x],0,16-peak[x]));
			}
			i=0;
		}

		int mins = Time.minute();
		int hours = Time.hour();

		char buffer[3];

		itoa(hours,buffer,10);
		//fix - as otherwise if num has leading zero, e.g. "03" hours, itoa coverts this to chars with space "3 ". 
		if (hours < 10) {
			buffer[1] = buffer[0];
			buffer[0] = '0';
		}
		vectorNumber(buffer[0]-'0',8,1,matrix.Color333(0,1,0),1,1);
		vectorNumber(buffer[1]-'0',12,1,matrix.Color333(0,1,0),1,1);

		itoa(mins,buffer,10);
		//fix - as otherwise if num has leading zero, e.g. "03" hours, itoa coverts this to chars with space "3 ". 
		if (mins < 10) {
			buffer[1] = buffer[0];
			buffer[0] = '0';
		}
		vectorNumber(buffer[0]-'0',18,1,matrix.Color333(0,1,0),1,1);
		vectorNumber(buffer[1]-'0',22,1,matrix.Color333(0,1,0),1,1);

		matrix.drawPixel(16,2,matrix.Color333(0,1,0));
		matrix.drawPixel(16,4,matrix.Color333(0,1,0));

		matrix.swapBuffers(true);
		//delay(10);


		// Every third frame, make the peak pixels drop by 1:
		if(++dotCount >= 3) {
			dotCount = 0;
			for(x=0; x<32; x++) {
				if(peak[x] > 0) peak[x]--;
			}
		}

		if(++colCount >= 10) colCount = 0;
		
		Particle.process();	//Give the background process some lovin'
	}
#else
	clock_mode = random(0,MAX_CLOCK_MODE-1);		// FFT is not active so switch to other random mode
	mode_changed = 1;
#endif
}


void plasma()
{

}

void marquee()
{
}




void randomFillWorld() {
  for (int i = 0; i < kMatrixWidth; i++) {
    for (int j = 0; j < kMatrixHeight; j++) {
      if (random(100) < 50UL) {
        world[i][j].alive = 1;
      }
      else {
        world[i][j].alive = 0;
      }
      world[i][j].prev = world[i][j].alive;
    }
  }
}

int neighbours(int x, int y) {
  return (world[(x + 1) % kMatrixWidth][y].prev) +
    (world[x][(y + 1) % kMatrixHeight].prev) +
    (world[(x + kMatrixWidth - 1) % kMatrixWidth][y].prev) +
    (world[x][(y + kMatrixHeight - 1) % kMatrixHeight].prev) +
    (world[(x + 1) % kMatrixWidth][(y + 1) % kMatrixHeight].prev) +
    (world[(x + kMatrixWidth - 1) % kMatrixWidth][(y + 1) % kMatrixHeight].prev) +
    (world[(x + kMatrixWidth - 1) % kMatrixWidth][(y + kMatrixHeight - 1) % kMatrixHeight].prev) +
    (world[(x + 1) % kMatrixWidth][(y + kMatrixHeight - 1) % kMatrixHeight].prev);
}


void conwayLife()
{
}

void rainbow() {
    normal_clock();
}
