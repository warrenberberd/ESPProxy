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

#ifndef ESPPROXY_H
#define ESPROXY_H

#include <Arduino.h>
#include <EEPROM.h>

#include <WiFiClientSecureBearSSL.h>

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#include <ESP8266HTTPClient.h>

using namespace BearSSL;
//#include "WiFiClientSecureAxTLS.h"
//using namespace axTLS;

#include <time.h> // For NTP

// For reading file from SPIFFS
//#include <LittleFS.h>
//#include <FS.h>

//#include <ESP32-targz.h>

// Constants
#define HTTP_PROXY_PORT 3128
#define ADMIN_PORT 80

#define LED_PIN  LED_BUILTIN

// For parsing HTTP Query
#include "HttpRequest.h"

/*
static const char ca_cert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDxTCCAq2gAwIBAgIQAqxcJmoLQJuPC3nyrkYldzANBgkqhkiG9w0BAQUFADBs
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
................................................................................................................................................................
vEsXCS+0yx5DaMkHJ8HSXPfqIbloEpw8nL+e/IBcm2PN7EeqJSdnoDfzAIJ9VNep
+OkuE6N36B9K
-----END CERTIFICATE-----
)EOF";

static const char client_cert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDWjCCAkKgAwIBAgIVAKAEpMzTvgSj6Cfki29+Bwd2GcxJMA0GCSqGSIb3DQEB
CwUAME0xSzBJBgNVBAsMQkFtYXpvbiBXZWIgU2VydmljZXMgTz1BbWF6b24uY29t
..........................................................................................................................................................................
uWjzrmLkeAn4+hsX3w/m48Pw67s5iWEztffx9pXT386SfnBrzrJnHXa5frr6bA==
-----END CERTIFICATE-----
)EOF";

static const char client_key[] PROGMEM = R"KEY(
-----BEGIN RSA PRIVATE KEY-----
MIIEpAIBAAKCAQEAuqHZchnoHXQYOvkN0xViIi9ARmp3y9FLkA7thSbu5pQOedzq
fN5zFHUAMC0ye5pXzZ0rtu5+PrpMsxfXcClx6Dcqiwc1Ug1vI5bWzeI1AviRSM+k
........................................................................................................................................................
/7EpcQKBgQCnEaef7D1/f+lAh00Vh4X3wW8icOPYZHycyKpCQ2E0gZ6alayezpD/
ffpUFwfAgpu52YmS4sh0SRSKXfvdaw+NQBYG7nnnGGp3mkJ43e847Se4f7oRL5ZT
uhdis+by3+AZsbVRujicleG9PaKizWujYvlsOQImqhrX5BcZESadFQ==
-----END RSA PRIVATE KEY-----
)KEY";
*/

class ESPProxy{
    private:
        // Buffer size used in CONNECT mode
        uint BUFFER_SIZE=128;  // should be lesser than : <= 124
        uint TIMEOUT=3000;
        uint TUNNEL_TIMEOUT=60;

        const uint LISTENING_PORT=HTTP_PROXY_PORT;
        const uint ADMIN_LISTENING_PORT=ADMIN_PORT;

        WiFiServer* _webServer;
        WiFiServer* _adminServer;

        uint lastTimestamp=0;

        String blackListHashCache[];

        bool searchInDirectoryFile(const char* dirS, String url,String host);
        bool searchInFile(File& f,String url,String host);

        bool processAdminQuery(WiFiClient& client);
        bool processAdminList(String reqURL,WiFiClient& adminClient);
        bool processAdminDir(String reqURL,WiFiClient& adminClient);
        bool processAdminBuffer(String reqURL,WiFiClient& adminClient);
        bool processAdminTimeout(String reqURL,WiFiClient& adminClient);
        bool processAdminTunnelTimeout(String reqURL,WiFiClient& adminClient);
        
        String archiveAccessLog(String reqURL,WiFiClient& adminClient);

        String addToFile(String filePath, String url);
        String removeFromFile(String filePath, String url);
        String checkFromFile(String filePath, String url);
        String dumpFile(String filePath);
        String dirFromDirectory(String dirPath);
        String downloadInList(String filePath,String url);
        bool downloadUrlFile(String url,String filePath);

        bool logRequest(String action,HttpRequest& reqOBJ);
        String getAccessLog();

    public:
        ESPProxy();    // Constructor
        ~ESPProxy(){};   // Destructor
        bool begin();   // Begin function

        // LED Management
        void enableLED();
        void disableLED();

        bool cycle();       // Function called each cycle
        bool sendRequestHeader(WiFiClient& _client,const int http_code,String contentType,String cacheControl);  // For sending HTTP Response to client

        bool isInBlacklist(String url,String host);
        bool isInWhitelist(String url,String host);

        bool proxy(HttpRequest reqOBJ,WiFiClient& _client); // To proxy the request
        bool proxyConnect(WiFiClient& _client,HttpRequest& reqOBJ);

        time_t setClock();    // Set NTP Time
        HttpRequest fetchURL(BearSSL::WiFiClientSecure& client, HttpRequest& reqOBJ);
        //void fetchCertAuthority(const char *host, const uint16_t port, const char *path);

        String getHeadersFromClient(HTTPClient& client);

        bool setBufferSize(uint s);
        bool setTimeout(uint s);
        bool setTunnelTimeout(uint s);
};

#endif