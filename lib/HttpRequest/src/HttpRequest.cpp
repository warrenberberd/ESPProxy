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

#include "HttpRequest.h"

#ifdef DEBUG
bool DEBUG_HTTP=true;
#else
bool DEBUG_HTTP=false;
#endif

/* bool HttpRequest::parseFromString(String req){
    this->clear();

    this->fullRequest=req;

    if(!this->parseRawProperties()) return false;
    if(!this->parseProperties()) return false;

    return true;
} */

// Clear old value
void HttpRequest::clear(){
    this->method=F("");
    this->URL=F("");
    this->Accept=F("");
    this->Accept_Encoding=F("");
    this->Accept_Language=F("");
    this->Cache_Control=F("");
    this->Connection=F("");
    this->fullRequest=F("");
    this->User_Agent=F("");
    this->Referer=F("");
    this->Proxy_Connection=F("");
    this->payload=F("");
    this->Port=0;
    this->Proto=F("http");
    this->HTTP_CODE=0;
    this->HTTP_STATUS=F("");

    this->Date=F("");
    this->Content_Type=F("");
    this->Set_Cookie=F("");
    this->Server=F("");
    this->isInHeadersSection=true;
    this->headers=F("");
}

bool HttpRequest::isError(){
    if(this->URL.isEmpty()) return true;

    return false;
}

/* bool HttpRequest::parseRawProperties(){
    // this->fullRequest

    uint length = this->fullRequest.length();



    return true;
}

bool HttpRequest::parseProperties(){
    
    return true;
}
*/

// Parse argument from First Line header
bool HttpRequest::parseFirstLine(String line){
    // GET / HTTP/1.1
    // HEAD http://toto/ HTTP/1.1
    char separator=' ';

    int oldIdx=-1;
    uint newIdx=0;

    // Method
    newIdx=line.indexOf(separator,oldIdx+1);
    this->method=line.substring(oldIdx+1,newIdx);
    oldIdx=newIdx;

    // Path
    newIdx=line.indexOf(separator,oldIdx+1);
    this->URL=line.substring(oldIdx+1,newIdx);
    oldIdx=newIdx;

    // Capture the http or https
    int tempIdx=this->URL.indexOf(F(":"));
    if(tempIdx<7 && tempIdx>1) this->Proto=this->URL.substring(0,tempIdx);

    // In path we only keep the /**** side (for CONNECT host:port only sequence)
    int searchFrom=0;
    if(this->URL.startsWith(F("http"))) searchFrom=9; // if https:// we search slash AFTER the second slash separator of proto
    int subIdx=this->URL.indexOf(F("/"),searchFrom);
    this->URL=this->URL.substring(subIdx);
    if(this->URL.isEmpty()) this->URL=F("/");

    // Http version
    //newIdx=line.indexOf(separator,oldIdx+1);
    this->HTTP_VERSION=line.substring(oldIdx+1);
    //oldIdx=newIdx;

    return true;
}

// Parse argument from First Line header
bool HttpRequest::parseResponseFirstLine(String line){
    // HTTP/1.0 200 OK
    char separator=' ';

    int oldIdx=-1;
    uint newIdx=0;

    // Http version
    newIdx=line.indexOf(separator,oldIdx+1);
    this->HTTP_VERSION=line.substring(oldIdx+1,newIdx);
    oldIdx=newIdx;

    // Return Code
    newIdx=line.indexOf(separator,oldIdx+1);
    this->HTTP_CODE=line.substring(oldIdx+1,newIdx).toInt();
    oldIdx=newIdx;

    // Status
    //newIdx=line.indexOf(separator,oldIdx+1);
    this->HTTP_STATUS=line.substring(oldIdx+1);
    //oldIdx=newIdx;


    return true;
}

// Parse one line of Query
bool HttpRequest::parseOneLine(String line){
    char separator=':';

    line.trim();

    if(DEBUG_HTTP) Serial.println(line);

    // If its a response Request
    if(line.startsWith(F("HTTP/")))    return this->parseResponseFirstLine(line);

    // The first line doesnt contains separator
    if(line.startsWith(F("GET ")))     return this->parseFirstLine(line);
    if(line.startsWith(F("POST ")))    return this->parseFirstLine(line);
    if(line.startsWith(F("HEAD ")))    return this->parseFirstLine(line);
    if(line.startsWith(F("PUT ")))     return this->parseFirstLine(line);
    if(line.startsWith(F("CONNECT "))) return this->parseFirstLine(line);  // For Proxy

    // Concat Headers
    if(this->isInHeadersSection) this->headers+=line + "\n";

    uint firstPartIdx=line.indexOf(separator,0);
    String param=line.substring(0,firstPartIdx);

    // If not and Headers line
    if(param.isEmpty()) return false;

    String value=line.substring(firstPartIdx+2);    // Start at +2 beause separator is followed by space

    if(param.equals(F("Host"))){
        //this->Host=value;
        int sep=value.indexOf(":");
        this->Host=value.substring(0,sep);
        if(sep>1) this->Port=(uint16_t)value.substring(sep+1).toInt();

        if(this->Port==0 && this->Proto.equals("http")) this->Port=80;
        if(this->Port==0 && this->Proto.equals("https")) this->Port=443;

        if(this->Port<1) this->Port=80; // Default port is HTTP

        if(this->Port==80 && !this->Proto.isEmpty())    this->Proto="http";
        if(this->Port==443 && !this->Proto.isEmpty())    this->Proto="https";

        //Serial.println("HOST:" + this->Host);
        //Serial.println("PORT:" + this->Port);

    }else if(param.equals(F("User-Agent"))){
        this->User_Agent=value;
    }else if(param.equals(F("Accept"))){
        this->Accept=value;
    }else if(param.equals(F("Accept-Language"))){
        this->Accept_Language=value;
    }else if(param.equals(F("Accept-Encoding"))){
        this->Accept_Encoding=value;
    }else if(param.equals(F("Connection"))){
        this->Connection=value;
    }else if(param.equals(F("Cache-Control"))){
        this->Cache_Control=value;
    }else if(param.equals(F("Referer"))){
        this->Referer=value;
    }else if(param.equals(F("Date"))){
        this->Date=value;
    }else if(param.equals(F("Content-Type"))){
        this->Content_Type=value;
    }else if(param.equals(F("Set-Cookie"))){
        this->Set_Cookie=value;
    }else if(param.equals(F("Server"))){
        this->Server=value;
    }else if(param.equals(F("Upgrade-Insecure-Requests"))){
        return false;
    }else if(param.equals(F("Save-Data"))){
        return false;
    }else if(param.equals(F("Pragma"))){
        return false;
    }else if(param.equals(F("TE"))){
        return false;
    }else if(param.equals(F("P3P"))){
        return false;
    }else if(param.equals(F("X-XSS-Protection"))){
        return false;
    }else if(param.equals(F("X-Frame-Options"))){
        return false;
    }else if(param.equals(F("Proxy-Connection"))){
        this->Proxy_Connection=value;
    }else{
        if(DEBUG_HTTP) Serial.print("Unknown HTTP parameter : " + param);
        if(DEBUG_HTTP) Serial.println("     Value : " + value);
        return false;
    }

    //Serial.println(param + " parsed.");

    return true;
}

// Decode input WebQuery
String HttpRequest::readWebRequest(WiFiClient& _client,uint timeout){
    this->clear();  // We clear old values

    String query;
    if(!_client.connected()){
        if(DEBUG_HTTP) Serial.println(F(" nobody is Connected"));
        return F("");    // There is nobody connected
    }

    // Waiting to data arrive
    if(!this->waitForDataIn(_client,timeout)) return F("");

    //delay(10);

    //Serial.println("There is a client connected");
    if(_client.available()<1){
        if(DEBUG_HTTP) Serial.println(F("No data to read"));
        return F("");
    }
    //Serial.println("We have data to read");

    String line=F("");
    char separator='\n';

    // While there is something to read
    while(_client.available()>0){
        String line=_client.readStringUntil(separator);

        if(!this->isInHeadersSection) this->payload+=line;
        line.trim();

        // At First empty line => we Exit Headers Section
        if(line.isEmpty()) this->isInHeadersSection=false;

        //if(DEBUG_HTTP) Serial.println("DEBUG : " + line);
        this->parseOneLine(line);   // Parsing of line

        query+=line;
    }

    query.trim(); //triming the string
    this->fullRequest=query;

    // Response with ack
    //if(DEBUG_HTTP) Serial.println("Query : " + query);

    return query;
}

// Unable to find how to NOT duplicate this code....
// Its bad, but it's work :/
// Decode input WiFiClientSecure
String HttpRequest::readWebRequest(BearSSL::WiFiClientSecure& _client,uint timeout){
    this->clear();  // We clear old values

    String query;
    if(!_client.connected()){
        if(DEBUG_HTTP) Serial.println(F(" nobody is Connected"));
        return F("");    // There is nobody connected
    }

    if(!this->waitForDataIn(_client,timeout)) return F("");

    //delay(10);

    //Serial.println("There is a client connected");
    if(_client.available()<1){
        if(DEBUG_HTTP) Serial.println(F("No data to read"));
        return F("");
    }
    //Serial.println("We have data to read");

    String line=F("");
    char separator='\n';

    // While there is something to read
    while(_client.available()>0){
        String line=_client.readStringUntil(separator);

        if(!this->isInHeadersSection) this->payload+=line;
        line.trim();

        // At First empty line => we Exit Headers Section
        if(line.isEmpty()) this->isInHeadersSection=false;

        if(DEBUG_HTTP) Serial.println("DEBUG : " + line);
        this->parseOneLine(line);   // Parsing of line

        query+=line;
    }

    query.trim(); //triming the string
    this->fullRequest=query;

    // Response with ack
    //if(DEBUG_HTTP) Serial.println("Query : " + query

    return query;
}

// Get the requested URL
String HttpRequest::getUrlPath(){
    return this->URL;
}

// Print the Query For Debugging
void HttpRequest::printDebug(bool rawMode){
    Serial.println(F("----------------------------------------------------------------------------------------"));
    Serial.println(F("-                                 Q U E R Y                                            -"));
    Serial.println(F("----------------------------------------------------------------------------------------"));
    if(rawMode){
        Serial.println(this->fullRequest);
        Serial.println(F(""));
        return;
    }
    Serial.println("Query : " + this->method + " " + this->URL + " " + this->HTTP_VERSION);
    if(!this->Host.isEmpty())              Serial.println("Host: "             + this->Host);
    if(!this->Proto.isEmpty())             Serial.println("Proto: "            + this->Proto);
    if(this->Port>0)                       Serial.printf ("Port: %i\n"         , this->Port);
    if(!this->User_Agent.isEmpty())        Serial.println("User-Agent: "       + this->User_Agent);
    if(!this->Accept.isEmpty())            Serial.println("Accept: "           + this->Accept);
    if(!this->Accept_Encoding.isEmpty())   Serial.println("Accept_Encoding: "  + this->Accept_Encoding);
    if(!this->Accept_Language.isEmpty())   Serial.println("Accept_Language: "  + this->Accept_Language);
    if(!this->Cache_Control.isEmpty())     Serial.println("Cache_Control: "    + this->Cache_Control);
    Serial.println(F(""));
}

// Retrieve last payload
String HttpRequest::getPayload(){
    return this->payload;
}

// Return Headers
String HttpRequest::getHeaders(){
    return this->headers;
}


// Waiting for data to come from WiFiClientSecure
bool HttpRequest::waitForDataIn(BearSSL::WiFiClientSecure& sslClient,uint timeout){
    if(sslClient.available()) return true;

    long start=millis();

    //sslClient.read();

    while(millis()-start<timeout){
        if(sslClient.available()) return true;

        delay(2);
        yield();
    }

    //long stop=millis();
    return false;
}

// Waiting for data to come from WiFiClient
bool HttpRequest::waitForDataIn(WiFiClient& client,uint timeout){
    if(client.available()) return true;
    
    long start=millis();

    //client.read();

    while(millis()-start<timeout){
        if(client.available()) return true;

        delay(2);
        yield();
    }

    //long stop=millis();
    return false;
}

// Recomposition de l'URL complete
String HttpRequest::getFullUrl(bool withHttp){
    String out=F("");

    if(withHttp) out+=this->Proto + "://";

    out+=this->Host;

    if(this->Proto.equals(F("http")) && this->Port!=80) out+=String(":") + this->Port;
    if(this->Proto.equals(F("https")) && this->Port!=443) out+=String(":") + this->Port;

    out+=this->getUrlPath();


    return out;
}