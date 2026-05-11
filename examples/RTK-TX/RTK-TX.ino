/*****************************************
 * ESP32 GPS VKEL 9600 Bds
 * This version is for T22_v01 20190612 board
 * As the power management chipset changed, it
 * require the axp20x library that can be found
 * https://github.com/lewisxhe/AXP202X_Library
 * You must import it as gzip in sketch submenu
 * in Arduino IDE
 * This way, it is required to power up the GPS
 * module, before trying to read it.
 *
 * Also get TinyGPS++ library from:
 * https://github.com/mikalhart/TinyGPSPlus
 ******************************************/

#include <TinyGPS++.h>
#include <axp20x.h>

#include <SPI.h>
#include <Wire.h>
#include "SSD1306.h"
#include "SX127x_Tx.h"
#include "ota.h"

#define LINE Serial.printf("%s:%d <<< \n", __FUNCTION__, __LINE__);

/*
 #define SCK     5    // GPIO5  -- SX1278's SCK
 #define MISO    19   // GPIO19 -- SX1278's MISO
 #define MOSI    27   // GPIO27 -- SX1278's MOSI
 #define SS      18   // GPIO18 -- SX1278's CS
 #define RST     14   // GPIO14 -- SX1278's RESET
 #define DI0     26   // GPIO26 -- SX1278's IRQ(Interrupt Request)
 #define BAND    433E6
 */

SSD1306 display(0x3c, 21, 22);

double slowLat = 0.0;
double slowLng = 0.0;

double fast_lat, fast_lng;
double diff_lat, diff_lng;

uint8_t idx = 0;


TinyGPSPlus gps;

HardwareSerial GPS(1);
AXP20X_Class axp;

#define HAVE_DOG 0
#if HAVE_DOG
    #include <esp_task_wdt.h>
	#define KICKDOG esp_task_wdt_reset()
    // Define configuration
    esp_task_wdt_config_t wdt_config =
    {
        .timeout_ms		= 5000,                 // Timeout in milliseconds
        .idle_core_mask = 0,                    // Do not watch idle tasks
        .trigger_panic	= false                 //true = restart ) on timeout
    };

#else
	#define KICKDOG
#endif

bool bWDTallowed = HAVE_DOG;


//------------------------------------------------------------------
#include <math.h>

#define PI 3.14159265358979323846
#define EARTH_RADIUS_M 6371000.0
// Convert degrees to radians
double degToRad(double deg)
{
    return deg * PI / 180.0;
}


// Calculate distance between two points in KM
double getGPSDistanceM(double lat1, double lon1, double lat2, double lon2)
{
    double dLat = degToRad(lat2 - lat1);
    double dLon = degToRad(lon2 - lon1);

    lat1 = degToRad(lat1);
    lat2 = degToRad(lat2);

    double a = sin(dLat / 2) * sin(dLat / 2) +
               cos(lat1) * cos(lat2) *
               sin(dLon / 2) * sin(dLon / 2);

    double c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return EARTH_RADIUS_M * c;
}


//------------------------------------------------------------------
u_int8_t char_height = 0;
static void setFont(uint8_t size)
{
    switch (size)
    {
    case 10:
        display.setFont(ArialMT_Plain_10);
        char_height = 10;
        break;

    case 16:
        display.setFont(ArialMT_Plain_16);
        char_height = 16;
        break;

    case 24:
        display.setFont(ArialMT_Plain_24);
        char_height = 24;
        break;
    }
}


int  xprintf(uint8_t lineNo, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    char buffer[100];
    vsprintf(buffer, format, args);

    display.drawString(0, lineNo * char_height, buffer);

    va_end(args);
    return 0;
}


//---------------------------------------------------------
void loop()
{
	_loop_ota();
};                                 // keep arduino happy
//---------------------------------------------------------
void led_display1(void)
{
    xprintf(0, "LA=%+9.7f", fast_lat);
    xprintf(1, "LO=%+9.7f", fast_lng);


    double dist = getGPSDistanceM(fast_lat, fast_lng, slowLat, slowLng);

    xprintf(2, "D=%lf", dist);


    //xprintf(2, "SA=%9.7f", slowLat - fast_lat);
    //xprintf(3, "SO=%9.7f", slowLng - fast_lng);

    //xprintf(3, "SO=%9.7f", slowLng);


    //xprintf(3, "%d", idx);

    //xprintf(2, "DIR=%3d %3s", (int)gps.course.deg(), gps.cardinal(gps.course.deg()));
    //xprintf(3, "M/S=%6.1f", gps.speed.mps());
    //xprintf(3, "%02d/%02d/%02d", gps.date.day(), gps.date.month(), gps.date.year());


    //display.display();

}


//---------------------------------------------------------

#define NUM_SAMPLES (1 << 8)
double hist_lat[NUM_SAMPLES];
double hist_lng[NUM_SAMPLES];

char rtlDiffMsg[60];

int32_t double2bin(double input)
{
	uint32_t ret;
	ret = input * 10000000.0;  // x.8 digits.
	return ret;
}

void taskGPS(void *not_used)
{
    static uint32_t last_tick;
    static bool bInited = false;

	uint32_t num_satellites;
	double percent;
	uint32_t tick = 0;

    while (1)
    {
        display.clear();
        fast_lat = gps.location.lat();
        fast_lng = gps.location.lng();

		num_satellites = gps.satellites.value();
		percent = 100. - min(100., gps.hdop.value()/10.);
		//Serial.printf("yyyyyyyyyyyy %f\n", percent);

        if (!fast_lat || !fast_lng)
        {
            display.clear();

            xprintf(0, "LA=%+9.7f", fast_lat);
            xprintf(1, "LN=%+9.7f", fast_lng);

            xprintf(2, "t=%d\n", tick++);
        }
        else
        {
        	// first valid sample pre-populates entire array
            if (!bInited && fast_lat)
            {
                for (int j = 0; j < NUM_SAMPLES; j++)
                {
                    hist_lat[j] = fast_lat;
                    hist_lng[j] = fast_lng;
                }

                bInited = true;
            }

            last_tick = micros();

            Serial.printf("Latitude  : %lf\n", fast_lat);
            Serial.printf("Longitude : %lf\n", fast_lng);

            hist_lat[idx] = fast_lat;
            hist_lng[idx] = fast_lng;

            idx++;

            if (idx == NUM_SAMPLES)
                idx = 0;

            //Serial.printf("idx = %d\n", idx);

            slowLat = 0.0;
            slowLng = 0.0;

            for (int i = 0; i < NUM_SAMPLES; i++)
            {
                slowLat += hist_lat[i];
                slowLng += hist_lng[i];
            }

            slowLat /= (float)NUM_SAMPLES;
            slowLng /= (float)NUM_SAMPLES;

			int len;
#if 1
			len = snprintf(rtlDiffMsg, sizeof(rtlDiffMsg), 
							   "%d 0x%f 0x%f %f %f", 
							   idx, slowLat, slowLng, 
							   slowLat-fast_lat, slowLng-fast_lng);
				   
			Serial.printf("%d %s\n", strlen(rtlDiffMsg), rtlDiffMsg);
#endif

		    len = snprintf(rtlDiffMsg, sizeof(rtlDiffMsg), 
		    				   "%d 0x%X 0x%X %+d %+d", 
		    				   idx, double2bin(slowLat), double2bin(slowLng), 
		    				   double2bin(slowLat-fast_lat), double2bin(slowLng-fast_lng));
		    				   
		    Serial.printf("%d %s\n", strlen(rtlDiffMsg), rtlDiffMsg);

            led_display1();
            /*
             *              Serial.print("Satellites: ");
             *              Serial.println(gps.satellites.value());
             *              Serial.print("Altitude  : ");
             *              Serial.print(gps.altitude.feet() / 3.2808);
             *              Serial.println("M");
             *
             *              Serial.print("Time      : ");
             *              Serial.print(gps.time.hour());
             *              Serial.print(":");
             *              Serial.print(gps.time.minute());
             *              Serial.print(":");
             *              Serial.println(gps.time.second());
             *
             *
             *              Serial.print("Speed     : ");
             *              Serial.println(gps.speed.kmph());
             *              Serial.println("**********************");
             */

            smartDelay(1000);
            
            uint32_t diff_time = micros() - last_tick;

            if (diff_time > 1200000 && gps.charsProcessed() < 10)
                Serial.println(F("No GPS data received: check wiring"));
        }

		xprintf(3, "SA=%02d Q=%.1f%%\n", num_satellites, min((double)999, percent));
		printf("%8d SA=%d QUAL=%.1f%%\n", tick, num_satellites, percent);
		
		

        display.display();
        smartDelay(1000);

		KICKDOG;

    }
}


//---------------------------------------------------------
void taskRadio(void *not_used)
{
    setup_radio();

    while (1)
        loop_radio();
}

//---------------------------------------------------------
static void smartDelay(unsigned long ms)
{
    unsigned long start = millis();
    do
    {
        if (GPS.available()) gps.encode(GPS.read());
        // don't dog in a delay routine duh.
		KICKDOG;
		yield();
    }
    while (millis() - start < ms);
}


//---------------------------------------------------------
void setup()
{
    //Serial.begin(921600);
    Serial.begin(115200);

    // oled stuff
    pinMode(16, OUTPUT);
    digitalWrite(16, LOW);      // set GPIO16 low to reset OLED
    delay(50);
    digitalWrite(16, HIGH);     // while OLED is running, must set GPIO16 in high?

	delay(3000);
	
    _setup_ota();

    // gps power mgt

    Wire.begin(21, 22);

    if (!axp.begin(Wire, AXP192_SLAVE_ADDRESS))
    {
	    axp.setPowerOutPut(AXP192_LDO2, AXP202_ON);
	    axp.setPowerOutPut(AXP192_LDO3, AXP202_ON);
	    axp.setPowerOutPut(AXP192_DCDC2, AXP202_ON);
	    axp.setPowerOutPut(AXP192_EXTEN, AXP202_ON);
	    axp.setPowerOutPut(AXP192_DCDC1, AXP202_ON);
        Serial.println("AXP192 Begin PASS");
	}
    else
    {
    	while(true)
    	{
	        Serial.println("AXP192 Begin FAIL");
	        delay(1000);
	    }
	}
    
    GPS.begin(9600, SERIAL_8N1, 34, 12);       //17-TX 18-RX


    display.init();
    display.flipScreenVertically();
    setFont(16);
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    xprintf(0, "START");
    display.display();


    TaskHandle_t hRunRadio, hRunGPS;

    xTaskCreate(taskRadio, "taskRadio", 4096 * 4, NULL, 8, &hRunRadio);
    xTaskCreate(taskGPS, "taskGPS", 4096 * 4, NULL, 8, &hRunGPS);

 //   esp_task_wdt_reconfigure(&wdt_config);
 //   esp_task_wdt_add(hRunRadio);        // Add task hRunGPS to WDT
 //   esp_task_wdt_add(hRunGPS);          // Add task hRunGPS to WDT

}
