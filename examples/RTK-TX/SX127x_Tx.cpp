/*
 * RadioLib SX127x Transmit with Interrupts Example
 *
 * This example transmits LoRa packets with one second delays
 * between them. Each packet contains up to 255 bytes
 * of data, in the form of:
 * - Arduino String
 * - null-terminated char array (C-string)
 * - arbitrary binary data (byte array)
 *
 * Other modules from SX127x/RFM9x family can also be used.
 *
 * For default module settings, see the wiki page
 * https://github.com/jgromes/RadioLib/wiki/Default-configuration#sx127xrfm9x---lora-modem
 *
 * For full API reference, see the GitHub Pages
 * https://jgromes.github.io/RadioLib/
 */

#include "SX127x_Tx.h"

// include the library
#include <RadioLib.h>

extern bool bWDTallowed;
#include <esp_task_wdt.h>



void dogDelay(unsigned long ms)
{
    unsigned long start = millis();

    do
    {
        if(bWDTallowed) esp_task_wdt_reset();    // don't dog in a delay routine duh.
        delay(10);
    }
    while (millis() - start < ms);
}


// SX1278 has the following connections:
// NSS pin:   10
// DIO0 pin:  2
// RESET pin: 9
// DIO1 pin:  3
//SX1278 radio = new Module(10, 2, 9, 3);

#ifdef ARDUINO_TTGO_LoRa32_V1
// Define pins for TTGO LoRa32 V1
#define NSS     18
#define RESET   14
#define DIO0    26
#define DIO1    -1
#define BAND    433E6
#else
#error "setup new params for radio board"
#endif

SX1278 radio = new Module(NSS, DIO0, RESET, DIO1);
// or detect the pinout automatically using RadioBoards
// https://github.com/radiolib-org/RadioBoards
/*
 #define RADIO_BOARD_AUTO
 #include <RadioBoards.h>
 * Radio radio = new RadioModule();
 */

// save transmission state between loops
int transmissionState = RADIOLIB_ERR_NONE;

// flag to indicate that a packet was sent
volatile bool bTransmitterAvail = true;

ICACHE_RAM_ATTR void setFlag(void)
{
    // we sent a packet, set the flag
    bTransmitterAvail = true;
}


#define FREQ 434.0000
void setup_radio()
{

    // initialize SX1278 with default settings
    Serial.print("[SX1278] Initializing ... ");


    int state = radio.beginFSK(FREQ,                    //freq
                               4.8,                     //br
                               5.0,                     //freqDev =
                               125.0,                   //rxBw =
                               1,                       //txpwr =
                               16,                      //preambleLength =
                               false);                  //enableOOK =

    //int state = radio.begin();

    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println("success!");
    }
    else
    {
        Serial.print("failed, code ");
        Serial.println(state);

        while (true)
            delay(10);
    }

    // set the function that will be called
    // when packet transmission is finished
    radio.setPacketSentAction(setFlag);

    radio.setOutputPower(2, false);
    //radio.setFrequency(FREQ);
    //radio.set

    // start transmitting the first packet
    Serial.print("[SX1278] Sending first packet ... ");

    // you can transmit C-string or Arduino string up to
    // 255 characters long
    transmissionState = radio.startTransmit("Hello World!");

    // you can also transmit byte array up to 255 bytes long
    /*
     * byte byteArr[] = {0x01, 0x23, 0x45, 0x67,
     *                  0x89, 0xAB, 0xCD, 0xEF};
     * transmissionState = radio.startTransmit(byteArr, 8);
     */
}

int txPassPktCtr = 0;
int txFailPktCtr = 0;

void SendString(char *msg)
{
    // check if the previous transmission finished
    while(!bTransmitterAvail) dogDelay(10);
    
    // reset flag
    bTransmitterAvail = false;

    if (transmissionState == RADIOLIB_ERR_NONE)
    {
        // packet was successfully sent
		txPassPktCtr++;
        Serial.printf("TX Pass packet count #%d\n", txPassPktCtr);
    }
    else
    {
        txFailPktCtr++;
        Serial.printf("TX Fail packet count #%d\n", txFailPktCtr);
    }

    // ensure last transmission is finished
    // this will ensure transmitter is disabled,
    // RF switch is powered down etc.
    radio.finishTransmit();

	Serial.printf("tx packet: %s\n", msg);
	// now swing back into tx packet mode and send it.
    transmissionState = radio.startTransmit(msg);

}

// counter to keep track of transmitted packets
void loop_radio()
{
	char test[63];
	sprintf(test, "hello %d", txPassPktCtr);
	SendString(test);
	
	dogDelay(10000);
}



