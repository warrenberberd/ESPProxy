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
#include <WiFiManager.h>
#include "ESPProxy.h"


#ifdef DEBUG
bool DEBUG_PROXY=true;
#else
bool DEBUG_PROXY=false;
#endif

// Wifi Manager
WiFiManager wifiManager;

WiFiServer webServer(HTTP_PROXY_PORT);
WiFiServer adminServer(ADMIN_PORT);


BearSSL::WiFiClientSecure sslClient;    // For TLS/SSL Connections
HTTPClient httpClient;                  // For Simple HTTP Connection
WiFiClient connectClient;               // For CONNECT mode

const char * httpResponseHeaders[] PROGMEM = {
    "Status","Server","Location",
    "User-Agent","Set-Cookie","Cookie","Connection",
    "Cache-Control","Host","Accept","Accept-Language","Accept-Encoding",
    "Upgrade-Insecure-Request",
    "Age","Allow","WWW-Authenticate","Pragma",
    "Date","Last-Modified","Expire",
    "Content-Length","Content-Location","Content-Language","Content-Encoding","Content-Type","Content-Disposition",
    "Proxy-Authenticate","X-Forwarded-For","X-Forwarded-Host"
};

// Config SSL
//BearSSL::WiFiClientSecure sslClient;
/*
BearSSL::X509List cert(ca_cert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(client_key);
*/

// Constructor
ESPProxy::ESPProxy(){}

bool ESPProxy::begin(){
    this->lastTimestamp=millis();

    //first parameter is name of access point, second is the password
    //wifiManager.autoConnect(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD);
    wifiManager.autoConnect(); // For auto-generated SSID

    if(DEBUG_PROXY) Serial.println("   Wifi Proxy is Connected.");

    this->_webServer=&webServer;
    this->_adminServer=&adminServer;

    IPAddress localIpAddr=WiFi.localIP();
    String localIP = String() + localIpAddr[0] + "." + localIpAddr[1] + "." + localIpAddr[2] + "." + localIpAddr[3];

    // Go to LISTENNING
    if(DEBUG_PROXY) Serial.print("Opening Listening port " + this->ADMIN_LISTENING_PORT);
    if(DEBUG_PROXY) Serial.println(" and " + this->LISTENING_PORT);
    this->_webServer->begin();
    this->_adminServer->begin();

    // For SSL we need NTP
    this->setClock();
    
    if(DEBUG_PROXY) Serial.print("AdminServer listenning on port : ");
    if(DEBUG_PROXY) Serial.print(localIP);
    if(DEBUG_PROXY) Serial.print(":");
    if(DEBUG_PROXY) Serial.println(ADMIN_PORT);

    if(DEBUG_PROXY) Serial.print("WebServer listenning on port : ");
    if(DEBUG_PROXY) Serial.print(localIP);
    if(DEBUG_PROXY) Serial.print(":");
    if(DEBUG_PROXY) Serial.println(HTTP_PROXY_PORT);

    // Initialize SPIFFS
    SPIFFS.begin();

    // Create logs dir if not exists
    if(!SPIFFS.exists("/logs/")) 
        SPIFFS.mkdir("/logs");

    return true;
}

// ON the LED
void ESPProxy::enableLED(){
    if(LED_PIN<0) return;

    digitalWrite(LED_PIN,LOW);
}

// Off the LED
void ESPProxy::disableLED(){
    if(LED_PIN<0) return;

    digitalWrite(LED_PIN,HIGH);
}

bool ESPProxy::cycle(){
    // Limitation for DEBUG_PROXYging
    //if(millis() - this->lastTimestamp<5000 && DEBUG_PROXY) delay(1000);

    //if(DEBUG_PROXY) Serial.println("ESPProxy::cycle()");
    this->lastTimestamp=millis();

    WiFiClient adminClient = this->_adminServer->available();   // Listen for incoming clients
    if(adminClient.connected()) this->processAdminQuery(adminClient);

    WiFiClient webClient = this->_webServer->available();   // Listen for incoming clients
    if(!webClient.connected()) return false;

    IPAddress clientIpAddr=webClient.localIP();
    String clientIP = String() + clientIpAddr[0] + "." + clientIpAddr[1] + "." + clientIpAddr[2] + "." + clientIpAddr[3];
    IPAddress localIpAddr=webClient.remoteIP();
    String localIP = String() + localIpAddr[0] + "." + localIpAddr[1] + "." + localIpAddr[2] + "." + localIpAddr[3];

    if(DEBUG_PROXY) Serial.println(F("============================================================================================"));
    if(DEBUG_PROXY) Serial.print(F("There is a WebClient connected : "));
    if(DEBUG_PROXY) Serial.println(clientIP);
    if(DEBUG_PROXY) Serial.print(F("Local Adress : "));
    if(DEBUG_PROXY) Serial.println(localIP);

    // Parsing Query
    HttpRequest reqOBJ;
    //reqOBJ.parseFromString(req);

    // To know what page is called
    reqOBJ.readWebRequest(webClient,this->TIMEOUT);

    // For debugging HttpRequest Parsing
    if(DEBUG_PROXY) reqOBJ.printDebug();

    if(DEBUG_PROXY) Serial.println(F("----------------------------------------------------------------------------------------"));
    if(DEBUG_PROXY) Serial.println(F("-                              A N A L Y Z E                                           -"));
    if(DEBUG_PROXY) Serial.println(F("----------------------------------------------------------------------------------------"));

    // If query is empty 
    if(reqOBJ.isError()){
        if(DEBUG_PROXY) Serial.println(F("Web Request is error"));
        this->sendRequestHeader(webClient,500,"","");
        webClient.println(); // End of Header
        webClient.stop();
        return false;
    }

    // What is the URL query
    String reqURL=reqOBJ.getFullUrl();
    String host=reqOBJ.Host;

    // if in Blacklist (prior to blacklist), we proxy
    if(this->isInWhitelist(reqURL,host)){
        if(DEBUG_PROXY) Serial.println("URL is in WhiteList : " + reqURL);

        this->logRequest(F("ACCEPT"),reqOBJ);
        return this->proxy(reqOBJ,webClient);
    }

    if(this->isInBlacklist(reqURL,host)){
        if(DEBUG_PROXY) Serial.println("URL is in blacklist : " + reqURL);
        this->logRequest(F("DENY"),reqOBJ);

        this->sendRequestHeader(webClient,403,"","");
        webClient.println(); // End of Header
        webClient.stop();
        return false;
    }

    if(DEBUG_PROXY) Serial.println("");

    // Logging
    this->logRequest("ACCEPT",reqOBJ);

    // If Not Denied, we proxy
    this->proxy(reqOBJ,webClient);

    //webClient.stop(); // Done in proxy() function

    return true;
}

// Send an HTTP Header
bool ESPProxy::sendRequestHeader(WiFiClient& _client,const int http_code,String contentType,String cacheControl){
    String statusMSG="OK";
    if(http_code>=300) statusMSG="Redirect";
    if(http_code>=400) statusMSG="User Error";
    if(http_code>=500) statusMSG="Server Internal Error";

    String httpFirstLine="HTTP/1.1 ";
    httpFirstLine+=http_code;
    httpFirstLine+=" ";
    httpFirstLine+=statusMSG;

      // header
    _client.println(httpFirstLine);
    if(!contentType.isEmpty()) _client.println("Content-type: " + contentType);
    if(!cacheControl.isEmpty()) _client.println("Cache-Control: " + cacheControl);
    _client.println("Connection: close");
    //_client.println();

    return true;
}

// If URL is in BlackList
bool ESPProxy::isInBlacklist(String url,String host){
    return this->searchInDirectoryFile("/blacklist/",url,host);
}

// If URL is in Whitelist
bool ESPProxy::isInWhitelist(String url,String host){
    return this->searchInDirectoryFile("/whitelist/",url,host);
}

// Search for an URL in directory files
bool ESPProxy::searchInDirectoryFile(const char* dirS, String url,String host){
    if(DEBUG_PROXY) Serial.print(F("Searching in directory : "));
    if(DEBUG_PROXY) Serial.println(dirS);

    Dir dir = SPIFFS.openDir(dirS);
    bool founded=false;

    while (dir.next()) {
        if(DEBUG_PROXY) Serial.print(F("     Searching in file : "));
        if(DEBUG_PROXY) Serial.print(dir.fileName() + "  ");
        if(DEBUG_PROXY) Serial.printf("('%s' or '%s')\n",url.c_str(),host.c_str());

        //Serial.print(dir.fileName());
        File f = dir.openFile("r");
        founded=false;
        //Serial.println(f.size());

        founded=this->searchInFile(f,url,host);
        
        f.close();

        if(founded) break;
    }

    return founded;
}

bool ESPProxy::searchInFile(File& f,String url,String host){
    f.seek(0);

    url.trim();
    host.trim();

    bool isDetected=false;

    //Serial.println(String("DEBUG :  url='") + url + "'");
    //Serial.println(String("DEBUG : host='") + host + "'");

    String line;
    while(f.available()){
        line=f.readStringUntil('\n');
        line.trim();

        if(line.isEmpty()) continue;

        if(line.equals(url) or line.equals(host)) isDetected=true;
        if(url.startsWith(line) or host.endsWith(line)) isDetected=true;

        //Serial.println(String("DEBUG : line='") + line + "'");

        // if Exact match or startsWith
        if(isDetected){
            if(DEBUG_PROXY) Serial.print(F("      URL '"));
            if(DEBUG_PROXY) Serial.println(url + "' is Denied by : " + line);
            return true;
            break;
        }
    }

    return false;

    //f.close();
}

// Proxy the request
bool ESPProxy::proxy(HttpRequest reqOBJ,WiFiClient &_client){
    if(DEBUG_PROXY) Serial.println(F("----------------------------------------------------------------------------------------"));
    if(DEBUG_PROXY) Serial.println(F("-                              R E S P O N S E                                         -"));
    if(DEBUG_PROXY) Serial.println(F("----------------------------------------------------------------------------------------"));

    //HTTPClient httpClient;                // For Simple HTTP Connection

    size_t httpResponseHeaderssize = sizeof(httpResponseHeaders)/sizeof(char*);

    int http_code=0;

    if(DEBUG_PROXY) Serial.print("Method : " + reqOBJ.method);
    if(DEBUG_PROXY) Serial.println(" : " + reqOBJ.getUrlPath());

    // HTTP Tunneling mode
    if(reqOBJ.method.equals(F("CONNECT"))) return this->proxyConnect(_client,reqOBJ);

    // To informe that we want to retrieve HTTP Response Headers
    httpClient.collectHeaders(httpResponseHeaders,httpResponseHeaderssize);

    // Start the connection
    httpClient.begin(reqOBJ.getUrlPath());

    String headers="";
    String payload="";

    // For HTTPS it's a little tricky
    if(reqOBJ.Proto.equals(F("https"))){
        //BearSSL::WiFiClientSecure sslClient;    // For TLS/SSL Connections
        // Configuration SSL
        
        //sslClient.setTrustAnchors(&cert);
        //sslClient.setClientRSACert(&client_crt, &key);
        //sslClient.setFingerprint("79 A9 78 95 56 8F AF D2 85 E2 1E 8F 23 E1 1E A9 40 D4 8B 09");  // Google fingerprint SHA-1
        sslClient.setInsecure();            // Do not check Cert Chain  -  Only exist in BearSSL
        //sslClient.allowSelfSignedCerts();   // Accept Self-Signed Certificat

        //httpClient.begin(sslClient,reqOBJ.getUrlPath());   // We switch to SSL Client

        delay(2);

        sslClient.setTimeout(this->TIMEOUT);
        if(DEBUG_PROXY) Serial.println(String(F("Connection HTTPS to : ")) + reqOBJ.Host + ":" + reqOBJ.Port);
        if (!sslClient.connect(reqOBJ.Host, reqOBJ.Port)) {
            Serial.println(F("ABORD : connection failed"));
            return false;
        }

        HttpRequest respOBJ = this->fetchURL(sslClient,reqOBJ);
        http_code=respOBJ.HTTP_CODE;

        headers=respOBJ.getHeaders();
        payload=respOBJ.getPayload();

        //httpClient.begin(sslClient,reqOBJ.getUrlPath());   // We switch to SSL Client
        //httpClient.begin(*_client,reqOBJ.getUrlPath());

        sslClient.stop();
    }else{
        if(reqOBJ.method.equals(F("GET"))){
            http_code=httpClient.GET();
        }else if(reqOBJ.method.equals(F("POST"))){
            http_code=httpClient.POST(reqOBJ.getPayload());
        }else if(reqOBJ.method.equals(F("PUT"))){
            http_code=httpClient.PUT(reqOBJ.getPayload());
        }else if(reqOBJ.method.equals(F("CONNECT"))){
            http_code=httpClient.GET();
        }else if(reqOBJ.method.equals(F("HEAD"))){
            http_code=httpClient.GET();
        }

        // Retrieve Headers from response
        headers=this->getHeadersFromClient(httpClient);

        // Retrieve Payload
        payload = httpClient.getString();

        // End Connection
        httpClient.end();
    }

    if(DEBUG_PROXY) Serial.print(F("HTTP_REPONSE_CODE : "));
    if(DEBUG_PROXY) Serial.println(http_code);

    // For better reading of errors
    if(http_code<0){
        switch(http_code){
            case HTTPC_ERROR_CONNECTION_REFUSED:
                if(DEBUG_PROXY) Serial.println(F("ERROR : HTTPC_ERROR_CONNECTION_REFUSED"));
                break;
            case HTTPC_ERROR_SEND_HEADER_FAILED:
                if(DEBUG_PROXY) Serial.println(F("ERROR : HTTPC_ERROR_SEND_HEADER_FAILED"));
                break;
            case HTTPC_ERROR_SEND_PAYLOAD_FAILED:
                if(DEBUG_PROXY) Serial.println(F("ERROR : HTTPC_ERROR_SEND_PAYLOAD_FAILED"));
                break;
            case HTTPC_ERROR_NOT_CONNECTED:
                if(DEBUG_PROXY) Serial.println(F("ERROR : HTTPC_ERROR_NOT_CONNECTED"));
                break;
            case HTTPC_ERROR_CONNECTION_LOST:
                if(DEBUG_PROXY) Serial.println(F("ERROR : HTTPC_ERROR_CONNECTION_LOST"));
                break;
            case HTTPC_ERROR_NO_STREAM:
                if(DEBUG_PROXY) Serial.println(F("ERROR : HTTPC_ERROR_NO_STREAM"));
                break;
            case HTTPC_ERROR_NO_HTTP_SERVER:
                if(DEBUG_PROXY) Serial.println(F("ERROR : HTTPC_ERROR_NO_HTTP_SERVER"));
                break;
            case HTTPC_ERROR_TOO_LESS_RAM:
                if(DEBUG_PROXY) Serial.println(F("ERROR : HTTPC_ERROR_TOO_LESS_RAM"));
                break;
            case HTTPC_ERROR_ENCODING:
                if(DEBUG_PROXY) Serial.println(F("ERROR : HTTPC_ERROR_ENCODING"));
                break;
            case HTTPC_ERROR_STREAM_WRITE:
                if(DEBUG_PROXY) Serial.println(F("ERROR : HTTPC_ERROR_STREAM_WRITE"));
                break;
            case HTTPC_ERROR_READ_TIMEOUT:
                if(DEBUG_PROXY) Serial.println(F("ERROR : HTTPC_ERROR_READ_TIMEOUT"));
                break;
        }
    }

    // If Error
    if(http_code<200){
        if(DEBUG_PROXY) Serial.println(F("Unable to forward the query"));
        if(http_code<=0) http_code=500;
        this->sendRequestHeader(_client,http_code,"","");
        _client.println(); // End of Header
        _client.stop();

        if(DEBUG_PROXY) Serial.println("");
        return false;
    }

    // For Response Code
    this->sendRequestHeader(_client,http_code,"","");

    // Send the header
    _client.println(headers);   // Transmit the headers
    
    // If request HEAD, stop here
    if(reqOBJ.method.equals("HEAD")){
        _client.stop();
        if(DEBUG_PROXY) Serial.println("");
        return true;
    }

    // Send the payload
    _client.println(payload);  // We Transmit the response body
    _client.stop();            // Closing connection

    if(DEBUG_PROXY) Serial.println(F("The Query was proxyfied"));

    if(DEBUG_PROXY) Serial.println(F("----------------------------------------------------------------"));
    //if(DEBUG_PROXY) Serial.println("Headers:");
    //if(DEBUG_PROXY) Serial.println(headers);
    if(DEBUG_PROXY) Serial.println("");
    if(DEBUG_PROXY) Serial.println(F("Payload:"));
    if(DEBUG_PROXY) Serial.println(payload);

    if(DEBUG_PROXY) Serial.println("");

    return true;
}

// For proxy HTTP Tunnel (via CONNECT method)
bool ESPProxy::proxyConnect(WiFiClient& _client,HttpRequest& reqOBJ){
    //WiFiClient connectClient;               // For CONNECT mode

    if(DEBUG_PROXY) Serial.println(F("CONNECT to Tunnel is require"));

    if(DEBUG_PROXY) Serial.println(String(F("Connection HTTPS to : ")) + reqOBJ.Host + ":" + reqOBJ.Port);
    //if (!sslClient.connect(reqOBJ.Host, reqOBJ.Port)) {
    if (!connectClient.connect(reqOBJ.Host, reqOBJ.Port)) {
        Serial.println(F("ABORD : connection failed"));
        return false;
    }

    // We say that CONNECT is ok, the raw dialog can start
    _client.println(F("HTTP/1.0 200 Connection established"));
    _client.println(F("Proxy-agent: ESP8266Proxy/1.0"));
    _client.println(F("Connection: keep-alive"));
    _client.println(F(""));

    // No delay
    connectClient.setNoDelay(true);
    _client.setNoDelay(true);

    if(DEBUG_PROXY) Serial.println(F("Tunnel is established."));

    delay(1); // Little pause

    uint8_t buff[this->BUFFER_SIZE];

    // Infinite dialog loop
    while(true){
        yield();

        if(!reqOBJ.waitForDataIn(_client,this->TIMEOUT)) break;

        if(DEBUG_PROXY) Serial.print(F("Tunneling query...  ")); 
        if(DEBUG_PROXY) Serial.printf("   Available : %i ",_client.available());
        // While emitter has a query to say
        while(_client.available()){
            memset(&buff, 0, this->BUFFER_SIZE);  // Empty the buffer
            yield();
            //delay(2);

            int rlen=_client.read(buff, sizeof(buff) - 1);

            //if(DEBUG_PROXY) Serial.write(buff,rlen);

            long start=millis();
            while(!connectClient.availableForWrite() && millis()-start<this->TUNNEL_TIMEOUT) yield();

            //buffSize=sizeof buff;
            connectClient.write(buff,rlen);

            if(!reqOBJ.waitForDataIn(_client,this->TUNNEL_TIMEOUT)) break;
        }
        if(DEBUG_PROXY) Serial.print(F("End of Tunneling query."));
        if(DEBUG_PROXY) Serial.println("");

        yield();
        if(!reqOBJ.waitForDataIn(connectClient,this->TIMEOUT)) break;

        if(DEBUG_PROXY) Serial.print(F("Tunneling response...")); 
        if(DEBUG_PROXY) Serial.printf("  Available : %i ",connectClient.available());
        // While server have a response to transmit
        while(connectClient.available()){
            memset(&buff, 0, this->BUFFER_SIZE);  // Empty the buffer
            yield();
            //delay(2);

            int rlen=connectClient.read(buff, sizeof(buff) - 1);

            //if(DEBUG_PROXY) Serial.write(buff,rlen);

            long start=millis();
            while(!_client.availableForWrite() && millis()-start<this->TUNNEL_TIMEOUT) yield();

            //buffSize=sizeof buff;
            _client.write(buff,rlen);

            //_client.write(connectClient.read());
            if(!reqOBJ.waitForDataIn(connectClient,this->TUNNEL_TIMEOUT)) break;
        } 
        

        if(DEBUG_PROXY) Serial.print(F("End of Tunneling response.")); 
        if(DEBUG_PROXY) Serial.println("");
    }

    _client.stop();
    connectClient.stop();

    if(DEBUG_PROXY) Serial.println(F("No more data to transmit in tunnel"));
    if(DEBUG_PROXY) Serial.println("");

    return true;
}

// For HTTPS Query
HttpRequest ESPProxy::fetchURL(BearSSL::WiFiClientSecure& client, HttpRequest& reqOBJ) {
    if(DEBUG_PROXY) Serial.println(F("ESPProxy::fetchURL()"));
    HttpRequest respOBJ;

  if(DEBUG_PROXY) Serial.printf("Trying: %s:%i...", reqOBJ.Host.c_str(),reqOBJ.Port);
  client.connect(reqOBJ.Host,reqOBJ.Port);
  if (!client.connected()) {
    if(DEBUG_PROXY) Serial.println(F("*** Can't connect. ***"));
    return respOBJ;
  }
  if(DEBUG_PROXY) Serial.println(F("Connected!"));

    String request=String("GET ") + reqOBJ.getUrlPath() + " HTTP/1.0\r\n";
    request+="Host: " + reqOBJ.Host + "\r\n";

    if(!reqOBJ.User_Agent.isEmpty()){
        request+="User-Agent: " + reqOBJ.User_Agent + "\r\n";
    }

    request+="\r\n";

    // For debug, printing request
    if(DEBUG_PROXY) Serial.println(request);

    // Sending request
    client.write(request.c_str());
    
    respOBJ.readWebRequest(client,this->TIMEOUT);

    client.stop();
    //if(DEBUG_PROXY) Serial.printf("\n-------\n\n");
    return respOBJ;
}

// Set time via NTP, as required for x.509 validation
time_t ESPProxy::setClock(){
  configTime(3 * 3600, 0, "0.fr.pool.ntp.org", "1.fr.pool.ntp.org", "2.fr.pool.ntp.org");

  if(DEBUG_PROXY) Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    if(DEBUG_PROXY) Serial.print(".");
    now = time(nullptr);
  }
  if(DEBUG_PROXY) Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  if(DEBUG_PROXY) Serial.print("Current time: ");
  if(DEBUG_PROXY) Serial.print(asctime(&timeinfo));
  return now;
}

// Collect headers from an Client Response
String ESPProxy::getHeadersFromClient(HTTPClient& client){
    // First send headers
    String headers="";
    String oneHeader;
    for(int i = 0; i< client.headers(); i++){
        String value=client.header(i);
        if(value.isEmpty()) continue;

        oneHeader="";
        oneHeader+=httpResponseHeaders[i];
        oneHeader+=": " + value;
        //_client.println(oneHeader);

        if(DEBUG_PROXY) Serial.print(F("Getting Response Header : "));
        if(DEBUG_PROXY) Serial.println(oneHeader);

        headers+=oneHeader + "\n";
    }

    return headers;
}

// To Log HTTP Request in the log file
bool ESPProxy::logRequest(String action, HttpRequest& reqOBJ){
    String logFile=F("/logs/access.log");

    // Getting time from NTP
    time_t now=this->setClock();

    // Formatting timestamp
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    String strTimestamp=String(asctime(&timeinfo));strTimestamp.trim();

    // Opening log file
    File f=SPIFFS.open(logFile,"a+");

    // Adding log
    String urlPath=reqOBJ.getUrlPath();urlPath.trim();
    String method=reqOBJ.method;method.trim();
    String host=reqOBJ.Host;host.trim();

    f.printf("%s\t%-10s %-7s %-30s %s\n",strTimestamp.c_str(),action.c_str(),method.c_str(),host.c_str(),urlPath.c_str());

    // Closing log file
    f.close();

    return true;
}

// Set the Buffer size, used in CONNECT method
bool ESPProxy::setBufferSize(uint s){
    if(s<1) return false;
    if(s>1024) return false;

    this->BUFFER_SIZE=s;
    return true;
}

// Set Query Timeout
bool ESPProxy::setTimeout(uint s){
    if(s<1) return false;
    if(s>60000) return false;

    this->TIMEOUT=s;
    return true;
}

// Set Tunnel Timeout
bool ESPProxy::setTunnelTimeout(uint s){
    if(s<1) return false;
    if(s>60000) return false;

    this->TUNNEL_TIMEOUT=s;
    return true;
}

// Retrieve log file content
String ESPProxy::getAccessLog(){
    String logFile="/logs/access.log";

    String out="";

    File f=SPIFFS.open(logFile,"r");

    while(f.available()){
        String line=f.readStringUntil('\n');
        if(DEBUG_PROXY) Serial.println("access.log : " + line);

        out+=line + "\n";
    }

    f.close();

    return out;
}

// ----------------------------- A D M I N    I N T E R F A C E --------------------------------
// Processing of Admin query (on ADMIN_PORT)
bool ESPProxy::processAdminQuery(WiFiClient& adminClient){
    // Parsing Query
    HttpRequest reqOBJ;
    //reqOBJ.parseFromString(req);

    // To know what page is called
    reqOBJ.readWebRequest(adminClient,this->TIMEOUT);

    // For debugging HttpRequest Parsing
    if(DEBUG_PROXY) reqOBJ.printDebug();

    // If query is empty 
    if(reqOBJ.isError()){
        if(DEBUG_PROXY) Serial.println("Web Request is error");
        this->sendRequestHeader(adminClient,500,"application/json","no-cache");
        adminClient.println(""); // End of Header
        adminClient.println("{");
        adminClient.println("\"error\": true,");
        adminClient.println("\"message\": \"Web Request is error\"");
        adminClient.println("}");
        adminClient.stop();
        return false;
    }

    if(DEBUG_PROXY) Serial.println("----------------------------------------------------------------------------------------");
    if(DEBUG_PROXY) Serial.println("-                                 A D M I N                                            -");
    if(DEBUG_PROXY) Serial.println("----------------------------------------------------------------------------------------");

    IPAddress localIpAddr=adminClient.localIP();
    String localIP = String() + localIpAddr[0] + "." + localIpAddr[1] + "." + localIpAddr[2] + "." + localIpAddr[3];
    IPAddress remoteIpAddr=adminClient.remoteIP();
    String remoteIP = String() + remoteIpAddr[0] + "." + remoteIpAddr[1] + "." + remoteIpAddr[2] + "." + remoteIpAddr[3];

    if(DEBUG_PROXY) Serial.print(F("Client Adress : "));
    if(DEBUG_PROXY) Serial.println(remoteIP);
    if(DEBUG_PROXY) Serial.print(F("Local Adress : "));
    if(DEBUG_PROXY) Serial.println(localIP);

    // What is the URL query
    String reqURL=reqOBJ.getUrlPath();


    if(reqURL.startsWith(F("/BLACKLIST"))) return this->processAdminList(reqURL,adminClient);
    if(reqURL.startsWith(F("/WHITELIST"))) return this->processAdminList(reqURL,adminClient);
    if(reqURL.startsWith(F("/DIR/")))      return this->processAdminDir(reqURL,adminClient);
    if(reqURL.startsWith(F("/BUFFER/")))   return this->processAdminBuffer(reqURL,adminClient);
    if(reqURL.startsWith(F("/TIMEOUT/")))  return this->processAdminTimeout(reqURL,adminClient);
    if(reqURL.startsWith(F("/TUNNEL_TIMEOUT/")))  return this->processAdminTunnelTimeout(reqURL,adminClient);
    
    if(reqURL.startsWith(F("/ARCHIVELOG"))){
        String archLog=this->archiveAccessLog(reqURL,adminClient);
        if(archLog.isEmpty()) return false;

        if(DEBUG_PROXY) Serial.println(" Log is archived to : " + archLog);
        return true;
    }

    // For ACCESSLOG
    if(reqURL.equals(F("/ACCESSLOG"))){
        this->sendRequestHeader(adminClient,200,"text/plain","no-cache");
        adminClient.println(); // End of Header
        adminClient.println(this->getAccessLog());
        adminClient.println();
        adminClient.stop();
        return true;
    }

    // If nothing to do, we respond an Error
    this->sendRequestHeader(adminClient,500,F("application/json"),F("no-cache"));
    adminClient.println(); // End of Header
    adminClient.println(F("{\"error\": true}"));
    if(DEBUG_PROXY) Serial.print(F("ADMIN : Nothing to do with query : "));
    if(DEBUG_PROXY) Serial.println(reqURL);
    adminClient.stop();

    return false;
}

// Processing of valid admin query
bool ESPProxy::processAdminList(String reqURL,WiFiClient& adminClient){
    int oldIdx=0;

    String dirList="";
    String action="";
    String value="";

    int newIdx=reqURL.indexOf("/",oldIdx+1);
    dirList=reqURL.substring(oldIdx+1,newIdx);dirList.toLowerCase();
    oldIdx=newIdx;

    newIdx=reqURL.indexOf("/",oldIdx+1);
    action=reqURL.substring(oldIdx+1,newIdx);
    oldIdx=newIdx;

    //newIdx=reqURL.indexOf("/",oldIdx+1);
    //value=reqURL.substring(oldIdx+1,newIdx);
    value=reqURL.substring(oldIdx+1);
    //oldIdx=newIdx;

    if(action.isEmpty()) value="";

    if(DEBUG_PROXY) Serial.println("Directory: " + dirList);
    if(DEBUG_PROXY) Serial.println("Action: " + action);
    if(DEBUG_PROXY) Serial.println("value: " + value);

    // if no action, it's an error
    if(action.isEmpty()){
        this->sendRequestHeader(adminClient,500,"application/json","no-cache");
        adminClient.println(); // End of Header

        adminClient.println("{\"error\":true}");
        adminClient.stop();

        return false;
    }

    this->sendRequestHeader(adminClient,200,"application/json","no-cache");
    adminClient.println(""); // End of header

    adminClient.println("{");
    adminClient.println("\"directory\": \"" + dirList + "\",");
    adminClient.println("\"action\": \"" + action + "\",");
    adminClient.println("\"value\": \""+ value + "\",");

    String filePath=String("/") + dirList + "/custom";
    
    String result;
    if(action.equals(F("ADD")))    result=this->addToFile(filePath,value);
    if(action.equals(F("REMOVE"))) result=this->removeFromFile(filePath,value);
    if(action.equals(F("CHECK")))  result=this->checkFromFile(filePath,value);
    if(action.equals(F("DUMP")))   result=this->dumpFile(filePath);
    if(action.equals(F("DIR")))    result=this->dirFromDirectory(String("/") + dirList);
    if(action.equals(F("DOWNLOAD")))  result=this->downloadInList(filePath,value);

    if(result.isEmpty()){
        adminClient.println(F("\"error\": true\n}"));
        adminClient.stop();
        return false;
    }

    adminClient.println("\"result\": \""+ result + "\",");

    adminClient.println(F("\"error\": false\n}"));
    adminClient.stop();
    return true;
}

String ESPProxy::addToFile(String filePath,String url){
    if(!SPIFFS.exists(filePath)) return "";

    String out;
    File f = SPIFFS.open(filePath,"r");

    if(this->searchInFile(f,url,String(""))){
        f.close();
        out+=F(" Already blocked. Ignore");
        if(DEBUG_PROXY) Serial.println(out);
        return out;
    }
    f.close(); // We close ReadOnly pointer

    // Open in Append mode
    f = SPIFFS.open(filePath,"a");
    f.println(url);
    f.close();

    out+="URL Added.";
    if(DEBUG_PROXY) Serial.println(out);
    return out;
}

// Remove url from a file
String ESPProxy::removeFromFile(String filePath,String url){
    if(!SPIFFS.exists(filePath)) return "";

    String out="";
    File f = SPIFFS.open(filePath,"r+");

    if(!this->searchInFile(f,url,String(""))){
        f.close();
        out=String(" Url '") + url + "' not in file : " + filePath;
        if(DEBUG_PROXY) Serial.println(out);
        return out;
    }
    f.seek(0);    

    while(f.available()>0){
        size_t curPos=f.position();

        String line=f.readStringUntil('\n');
        line.trim();

        if(line.equals(url)){
            f.seek(curPos);

            // Create of buffer with the size of the line to erase
            char buff[line.length()];
            memcpy(&buff,'\0',line.length());   // Fill with null char
            f.write(buff,sizeof(buff)); // Write buffer at the emplacement of line

            f.close();

            out="URL Removed.";
            if(DEBUG_PROXY) Serial.println(out);
            return out;
        }
    }

    f.close();

    return out;
}

// Check if URL is in this file
String ESPProxy::checkFromFile(String filePath,String url){
    if(!SPIFFS.exists(filePath)) return "";

    String out;
    File f = SPIFFS.open(filePath,"r");

    if(this->searchInFile(f,url,String(""))){
        f.close();
        out="URL is in file";
        if(DEBUG_PROXY) Serial.println(out);
        return out;
    }
    f.close(); // We close ReadOnly pointer
    out="URL is not in file";
    if(DEBUG_PROXY) Serial.println(out);
    return out;
}

// Dump a file
String ESPProxy::dumpFile(String filePath){
    if(!SPIFFS.exists(filePath)) return "";

    String out="";
    File f = SPIFFS.open(filePath,"r");

    f.seek(0);    

    uint lineNum=0;

    if(DEBUG_PROXY) Serial.println("Dumping File : " + filePath);
    while(f.available()>0){
        String line=f.readStringUntil('\n');
        line.trim();

        if(DEBUG_PROXY) Serial.printf("DUMP Line %i : %s\n",lineNum++,line.c_str());
        out+=line + "\n";
    }

    f.close();
    return out;
}

// Dump a file
String ESPProxy::dirFromDirectory(String dirPath){
    String out="";

    //if(!SPIFFS.exists(dirPath)) return "";

    Dir dir = SPIFFS.openDir(dirPath);

    out+="Listing Directory : " + dirPath + "\n";

    if(DEBUG_PROXY) Serial.println(out);
    while (dir.next()) {
        if(DEBUG_PROXY) Serial.print("  File : ");
        if(DEBUG_PROXY) Serial.println(dir.fileName());

        out+=" -" + dir.fileName() + "\n";
    }

    return out;
}

// To archive current access.log
String ESPProxy::archiveAccessLog(String reqURL,WiFiClient& adminClient){
    this->sendRequestHeader(adminClient,200,F("application/json"),F("no-cache"));
    adminClient.println(""); // End of header

    adminClient.println("{");
    adminClient.println(F("\"action\": \"archiveAccessLog\","));

    // Getting time from NTP
    time_t now=this->setClock();

    char buf[60];
    strftime(buf, sizeof(buf), "%F", gmtime(&now));

    String ts=String(buf);ts.trim();

    String logFile=F("/logs/access.log");
    String archiveLogFile=logFile + "_" + ts;

    // if already a "day named" archive, we go to Y-m-d_H:M:S
    if(SPIFFS.exists(archiveLogFile)){
        char buf[60];
        strftime(buf, sizeof(buf), "%T", gmtime(&now));

        archiveLogFile+="_" + String(buf);
    }

    if(SPIFFS.rename(logFile,archiveLogFile)){
        adminClient.println("\"error\": false,");
        adminClient.println("\"fileName\": \"" + archiveLogFile + "\"");

        adminClient.println("}");

        adminClient.stop();
        return archiveLogFile;
    }
        
    adminClient.println(F("\"error\": true"));
    adminClient.println("}");
    adminClient.stop();

    return "";  // if error when renaming
}

// Ask from ADMIN_PORT to DOWNLOAD a list
String ESPProxy::downloadInList(String filePath,String url){
    if(DEBUG_PROXY) Serial.println("downloadInList(" + filePath + "," + url + ")");

    int idx=url.lastIndexOf("/");
    String fileName=url.substring(idx+1);
    String fileDest=filePath + "/" + fileName;
    String uncompressFileDest=fileDest;uncompressFileDest.replace(".tar.gz","");


    if(!this->downloadUrlFile(url,fileDest)){
        if(DEBUG_PROXY) Serial.println("Unable to download " + url);
        return "";
    }

    // Uncompress the file
    bool uncompressed=false;
    //uncompressed=tarGzExpander(SPIFFS, fileDest.c_str(), SPIFFS, uncompressFileDest.c_str());
    return "";

    // If Deflate Successful, we remove compressed file
    if(uncompressed) SPIFFS.remove(fileDest);

    return "";
}

// Download HTTP File on local SPIFFS
bool ESPProxy::downloadUrlFile(String url,String filePath){
    if(DEBUG_PROXY) Serial.println("Dowloading '" + url + "' to '" + filePath + "'");


    int idxEndHost=url.indexOf("/",9);
    String request=url.substring(idxEndHost+1);
    String hostWithPrefix=url.substring(0,idxEndHost);
    int idxStartHost=hostWithPrefix.lastIndexOf("/");
    String hostWithPort=hostWithPrefix.substring(idxStartHost+1);

    String host=hostWithPort;
    uint16_t port=0;

    int idxPort=hostWithPort.indexOf(":");
    if(idxPort>0){
        host=hostWithPort.substring(0,idxPort);
        port=hostWithPort.substring(idxPort+1).toInt();
    }else{
        if(port<1 && url.startsWith(F("https:"))) port=443;
        if(port<1 && url.startsWith(F("http:"))) port=80;
    }

    bool modeHTTPS=false;
    if(url.startsWith("https")) modeHTTPS=true;

    if(modeHTTPS){
        //BearSSL::WiFiClientSecure sslClient;    // For TLS/SSL Connections

        //sslClient.allowSelfSignedCerts();
        sslClient.setInsecure(); // Exists only in BearSSL
        // Fingerprint of : https://dsi.ut-capitole.fr/   expire at : 14/10/2021 
        //sslClient.setFingerprint("96 CC 22 5E 24 DD 7B 14 D7 5E B3 38 CE 57 4E 8E A3 97 1E 32");
        //sslClient.setBufferSizes(this->BUFFER_SIZE,this->BUFFER_SIZE);

        optimistic_yield(500);

        if(DEBUG_PROXY) Serial.print(F("Memory Fragmentation: "));
        if(DEBUG_PROXY) Serial.println(ESP.getHeapFragmentation());
        if(DEBUG_PROXY) Serial.print(F("Free Memory Heap: "));
        if(DEBUG_PROXY) Serial.println(ESP.getFreeHeap());
        if(DEBUG_PROXY) Serial.print(F("Memory Max Free BlockSize: "));
        if(DEBUG_PROXY) Serial.println(ESP.getMaxFreeBlockSize());

        char errBuff[1024];

        if(DEBUG_PROXY) Serial.println("Connecting to " + host + ":" + port);
        //yield();
        //Serial.println("clearWriteError()");
        //sslClient.clearWriteError();
        //Serial.println("connect()");
        //sslClient.stopAll(); // Close all existing connections

        sslClient.setTimeout(this->TIMEOUT*10);
        optimistic_yield(1000);
        sslClient.connect(host,port);
        //Serial.println("yield()");
        //yield();
        sslClient.setTimeout(this->TIMEOUT);
        if(!sslClient.connected()){
            if(DEBUG_PROXY) Serial.print(F("ABORD : connection failed to url "));
            sslClient.getLastSSLError(errBuff, sizeof(errBuff)-1); // Only exist in BearSSL
			Serial.printf("ssl_error: %s\n", errBuff);

            if(DEBUG_PROXY) Serial.print(F("Memory Fragmentation: "));
            if(DEBUG_PROXY) Serial.println(ESP.getHeapFragmentation());
            if(DEBUG_PROXY) Serial.print(F("Free Memory Heap: "));
            if(DEBUG_PROXY) Serial.println(ESP.getFreeHeap());
            if(DEBUG_PROXY) Serial.print(F("Memory Max Free BlockSize: "));
            if(DEBUG_PROXY) Serial.println(ESP.getMaxFreeBlockSize());

            if(DEBUG_PROXY) Serial.println(url);
            return false;
        }
        if(DEBUG_PROXY) Serial.println(F("   is connected."));

        // Wait until we can write
        long start=millis();
        while(sslClient.availableForWrite()<1 && millis()-start<10000) optimistic_yield(500);

        String request="GET " + request + " HTTP/1.0\r\n";
        request+="Host: " + host + "\r\n";
        request+=F("User-Agent: ESP8266/ProxyApp\r\n");
        request+="\r\n";    // End of Header

        // Sending the SSL Request
        sslClient.println(request);

        yield();
        HttpRequest respOBJ;
        if(!respOBJ.waitForDataIn(sslClient,this->TIMEOUT)){
            if(DEBUG_PROXY) Serial.print(F("The server never respond to "));
            if(DEBUG_PROXY) Serial.println(request);
            sslClient.stop();
            return false;
        }

        uint8_t buff[this->BUFFER_SIZE];

        File outFile=SPIFFS.open(filePath,"w");


        if(DEBUG_PROXY) Serial.println(F("Downloading..."));
        sslClient.setTimeout(this->TIMEOUT);
        while(sslClient.available()){
            memset(&buff, 0, this->BUFFER_SIZE);  // Empty the buffer
            yield();
            //delay(2);

            int rlen=sslClient.read(buff, sizeof(buff) - 1);

            //if(DEBUG_PROXY) Serial.write(buff,rlen);

            //long start=millis();
            //while(!_client.availableForWrite() && millis()-start<this->TUNNEL_TIMEOUT) yield();

            outFile.write(buff,rlen);

            // if no more data to read
            if(respOBJ.waitForDataIn(sslClient,this->TUNNEL_TIMEOUT)) break;
        }

        outFile.close();
        sslClient.stopAll();

        if(DEBUG_PROXY) Serial.println(F("    Success !"));

        return true;
    }else{

    }


    return false;
}

// Dir from a directory
bool ESPProxy::processAdminDir(String reqURL,WiFiClient& adminClient){
    String req=reqURL;
    req.replace("/DIR","");

    this->sendRequestHeader(adminClient,200,"application/json","no-cache");
    adminClient.println(""); // End of header

    adminClient.println("{");
    adminClient.println("\"action\": \"dir\",");
    adminClient.println("\"directory\": \""+ req + "\",");
    
    String out=this->dirFromDirectory(req);

    if(out.isEmpty()){
        adminClient.println("\"error\": true");
        adminClient.println("\"message\": \"Result is empty\"");
        adminClient.println("}");
        adminClient.stop();
    }

    adminClient.println("\"error\": false,");
    adminClient.println("\"content\": \""+ out + "\"");
        

    adminClient.println("}");
    adminClient.stop();

    return true;
}

// ASK from ADMIN_PORT to change BUFFER_SIZE
bool ESPProxy::processAdminBuffer(String reqURL,WiFiClient& adminClient){
    String req=reqURL;
    req.replace("/BUFFER/","");

    this->sendRequestHeader(adminClient,200,"application/json","no-cache");
    adminClient.println(""); // End of header

    adminClient.println("{");
    adminClient.println("\"action\": \"setBufferSize\",");
    adminClient.println("\"query\": \""+ req + "\",");

    uint s=req.toInt();

    if(DEBUG_PROXY) Serial.printf("BufferSize set to : %i\n",s);

    bool status=this->setBufferSize(s);

    if(status){
        adminClient.println("\"error\": false");
    }else{
        adminClient.println("\"error\": true");
    }
    
    adminClient.println("}");
    adminClient.stop();

    return status;
}

// Ask from ADMIN_PORT to change Query Timeout
bool ESPProxy::processAdminTimeout(String reqURL,WiFiClient& adminClient){
    String req=reqURL;
    req.replace("/TIMEOUT/","");

    this->sendRequestHeader(adminClient,200,"application/json","no-cache");
    adminClient.println(""); // End of header

    adminClient.println("{");
    adminClient.println("\"action\": \"setTimeout\",");
    adminClient.println("\"query\": \""+ req + "\",");

    uint s=req.toInt();
    if(DEBUG_PROXY) Serial.printf("Timeout set to : %i\n",s);

    bool status=this->setTimeout(s);

    if(status){
        adminClient.println("\"error\": false");
    }else{
        adminClient.println("\"error\": true");
    }
    
    adminClient.println("}");
    adminClient.stop();

    return status;
}

// Ask from ADMIN_PORT to change TUNNEL_TIMEOUT
bool ESPProxy::processAdminTunnelTimeout(String reqURL,WiFiClient& adminClient){
    String req=reqURL;
    req.replace("/TUNNEL_TIMEOUT/","");

    this->sendRequestHeader(adminClient,200,"application/json","no-cache");
    adminClient.println(""); // End of header

    adminClient.println("{");
    adminClient.println("\"action\": \"setTunnelTimeout\",");
    adminClient.println("\"query\": \""+ req + "\",");

    uint s=req.toInt();

    if(DEBUG_PROXY) Serial.printf("Tunnel Timeout set to : %i\n",s);
    bool status=this->setTunnelTimeout(s);

    if(status){
        adminClient.println("\"error\": false");
    }else{
        adminClient.println("\"error\": true");
    }
    
    adminClient.println("}");
    adminClient.stop();

    return status;
}