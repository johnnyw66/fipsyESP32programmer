#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <WiFiScan.h>
#include <ETH.h>
#include <WiFiClient.h>
#include <WiFiSTA.h>
#include <WiFiServer.h>
#include <WiFiType.h>
#include <WiFiGeneric.h>

#include "heltec.h"
#include "images.h"

#include "Streaming.h"
#include <WiFi.h>
#include "fipsy.hpp"

#define _OLED_DISPLAY

const char* WIFI_SSID = "YOUR_WIFI_AP_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
const int tcpport = 34779 ;
Fipsy fipsy(SPI);
Fipsy::FuseTable fuseTable;
WiFiServer listener(tcpport);
bool fipsyExists = true ;
void
setup()
{
  Serial.begin(115200);
#ifdef _OLED_DISPLAY
  initOLED() ;
#endif

  if (!fipsy.begin(14, 12, 13, 17)) {
    fipsyExists = false;
    Serial << "Fipsy not found" << endl;
   // return;
  } else {
    Serial << "Fipsy FOUND " << endl;
    fipsyExists = true;
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
  }
  Serial << "IP ADDR " << WiFi.localIP() << endl;
  delay(1000) ;

  listener.begin();



}


void initOLED() {

  Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Enable*/, true /*Serial Enable*/);
  logo();
  Serial << "Display Initialised.. " << endl ;
  delay(2000);
  Heltec.display->clear();
}

void displayStatus() {
  display(String(WIFI_SSID), 0 ,0) ;
  display(WiFi.localIP().toString(),0,9) ;
  display("Port : " + String(tcpport),0,18) ;
  if (fipsyExists) {
    display("Fipsy *CONNECTED* " ,0,27) ;
  } else {
    display("Fipsy *NOT CONNECTED*" ,0,27) ;
  }
//  Heltec.display -> display();
//  delay(150) ;
//  Heltec.display->clear();

}

void logo(){
  Heltec.display -> clear();
  Heltec.display -> drawXbm(0,0,logo_width,logo_height,(const unsigned char *)logo_bits);
  Heltec.display -> display();
}

void display(String msg,int x, int y) {
  Heltec.display -> drawString(x, y, msg);


}

int count = 0 ;

void displayWaitingBar(int c) {
      int busyX = 66 ;
      int busyY = 40 ;
      Heltec.display -> clear();

      displayStatus() ;


      display("WAITING..",0,busyY) ;

      switch(c % 4) {
          case 0:
                display("/",busyX, busyY) ;
                break ;
          case 1:
                display("-",busyX, busyY) ;
                break ;
          case 2:
                display("\\",busyX,busyY) ;
                break ;
          case 3:
                display("|",busyX, busyY) ;
                break ;
      }
      Heltec.display -> display();
      delay(150) ;

}

void connectedStatus() {
    Heltec.display -> clear();
    display("*PROGRAMMING*",20, 30) ;
    Heltec.display -> display();

}

void loop()
{
  count++ ;
  WiFiClient client = listener.available();
  if (!client) {
#ifdef _OLED_DISPLAY
    displayWaitingBar(count)  ;
#endif
    return;
  } else {

#ifdef _OLED_DISPLAY
    connectedStatus() ;
#endif

  }

  if (!fipsyExists) {
      client << "PROGRAMMER SERVER CONNECTION COMPLETE BUT FIPSY NOT CONNECTED.. ABORTING " << endl ;
      return ;
  }

  Fipsy::JedecError parseError = Fipsy::parseJedec(client, fuseTable);
  if (parseError != Fipsy::JedecError::OK) {
    client << "JEDEC parse error: " << static_cast<int>(parseError) << endl;
    client.stop();
    return;
  }
  client << "JEDEC OK, fuse checksum: " << _HEX(fuseTable.computeChecksum()) << endl;

  if (!fipsy.enable()) {
    client << "Cannot enable configuration mode, status: " << _HEX(fipsy.readStatus().v) << endl;
    client.stop();
    return;
  }

  uint32_t featureRow0, featureRow1;
  uint16_t feabits;
  fipsy.readFeatures(featureRow0, featureRow1, feabits);
  client << "Feature Row: " << _HEX(featureRow0) << ' ' << _HEX(featureRow1) << endl
         << "FEABITS: " << _HEX(feabits) << endl;

  client << "Programming ..." << endl;

  if (fipsy.program(fuseTable)) {
    client << "Success" << endl;
  } else {
    client << "Failed" << endl;
  }

  fipsy.disable();
  client.flush();
  client.stop();
}
