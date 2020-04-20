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

#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiClientSecureBearSSL.h>
using namespace BearSSL;
//#include "WiFiClientSecureAxTLS.h"
//using namespace axTLS;
//#include <hashtable.h>

#define WAIT_FOR_DATA_TIMEOUT   3000

class HttpRequest{
    private:
        bool isInHeadersSection=true;

        bool parseRawProperties();
        bool parseProperties();

        void clear();

    public:
        uint max_age=0;

        String method="";       // GET/POST
        String URL="";          // /path
        String HTTP_VERSION=""; // 1.1


        // Raw Properies
        String query="";        // GET / HTTP/1.1
        String Host="";         // 192.168.1.13
        uint16_t Port=0;         // Port
        String Proto="http";    // http or https
        String User_Agent;      // Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:74.0) Gecko/20100101 Firefox/74.0
        String Accept;             // text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8
        String Accept_Language;    // fr,fr-FR;q=0.8,en-US;q=0.5,en;q=0.3
        String Accept_Encoding;    // gzip, deflate
        String Connection = "";    // keep-alive
        String Cache_Control = ""; // max-age=0
        String Referer;
        String Proxy_Connection;
        int    HTTP_CODE=0;         // HTTP_CODE of Response query
        String HTTP_STATUS;         // The status of the code

        String Date;
        String Content_Type;
        String Set_Cookie;
        String Server;
        
        // Original Query
        String fullRequest;
        String payload;
        String headers;
        //String dataBody;

        bool parseFromString(String req);
        String readWebRequest(WiFiClient& _client,uint timeout=WAIT_FOR_DATA_TIMEOUT);                     // Read from WiFiClient
        String readWebRequest(BearSSL::WiFiClientSecure& _client,uint timeout=WAIT_FOR_DATA_TIMEOUT);   // Read from WiFiClientSecure
        bool parseOneLine(String line);                 // Parsing One line
        bool parseFirstLine(String line);
        bool parseResponseFirstLine(String line);

        bool isError();
        void printDebug(bool rawMode=false);

        String getPayload();
        String getHeaders();

        String getUrlPath();
        String getFullUrl(bool withHttp=true);

        bool waitForDataIn(BearSSL::WiFiClientSecure& sslClient,uint timeout);
        bool waitForDataIn(WiFiClient& sslClient,uint timeout);
};

#endif