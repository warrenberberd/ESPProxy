/*
    This code is attended to create an HTTP Proxy with a small ESP8266 or ESP32
    
    Copyright (C) 2020 Alban Ponche

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <Arduino.h>

// To have Wifi Access
//#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
//#include <DNSServer.h>
//#include <ESP8266WebServer.h>
//#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#include <ESPProxy.h>

#ifdef DEBUG
bool DEBUG_MAIN=true;
#else
bool DEBUG_MAIN=false;
#endif

#define PIN_UNCONNECTED -1

// My Proxy
ESPProxy myProxy;

// Main setup
void setup() {
  Serial.begin(115200);

  if(DEBUG_MAIN) while (!Serial) ; // wait for Arduino Serial Monitor to open
  if(DEBUG_MAIN) Serial.println();

  uint32_t free = system_get_free_heap_size();
  if(DEBUG_MAIN) Serial.printf("Free Memory at Begin: %i\n",free);

  Serial.setDebugOutput(false);
  Serial.setDebugOutput(true);
  if(!DEBUG_MAIN) Serial.setDebugOutput(false);
  //Serial.setDebugOutput(false);

  if(DEBUG_MAIN) Serial.println("");
  if(DEBUG_MAIN) delay(100);

  if(DEBUG_MAIN) Serial.println("main::Setup...");

  myProxy.begin();
}

// Main loop
void loop() {
  myProxy.cycle();

  delay(1);   // To prevent crash
  optimistic_yield(5);
}