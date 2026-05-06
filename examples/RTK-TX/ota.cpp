#include <Arduino.h>

#include <esp_wifi.h>
#include <WiFi.h>

#include <ArduinoOTA.h>

//-------------------------------------------------------------

void _loop_ota(void) 
{
    ArduinoOTA.handle();  // Continuously check for update requests.
}
//-------------------------------------------------------------
void _setup_ota(void) 
{

    Serial.printf("SSID %s\n", MY_SSID);

	WiFi.begin();
	WiFi.disconnect(true);

    for (int i = 0; i < 8; i++)
    {
        WiFi.begin(MY_SSID, MY_SSID_PASSWORD);  // Connect wifi and return connection status.
        if(WiFi.status() == WL_CONNECTED) break;
        Serial.printf ("wifi %s '%s' %d of 8 tries\n", MY_SSID, MY_SSID_PASSWORD, i);
        delay(1000);
    }
    
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());  // Output IP Address.  è¾“å‡ºIPåœ°å�€

	uint8_t baseMac[6];
	uint32_t bigMacLo;
	
	esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);

	if (ret == ESP_OK) {
	Serial.printf("MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
				 baseMac[0], baseMac[1], baseMac[2],
				 baseMac[3], baseMac[4], baseMac[5]);
	}
	
	bigMacLo=baseMac[5]       | baseMac[4] <<  8 | 
			 baseMac[3] << 16 | baseMac[2] << 24 ;
	
	char hName[40] = REMOTE_HOSTNAME;

	// overide hostname based on MAC
	if (bigMacLo == 0x84A7024C ) strcpy (hName, "YELLOW");
	if (bigMacLo == 0xA0D4CB8C ) strcpy (hName, "BLACK");

    ArduinoOTA.setHostname(hName);
    ///ArduinoOTA.setPassword("666666");

	Serial.printf("OTA Hostname: %s\n", hName);
    
    ArduinoOTA.begin();
}
