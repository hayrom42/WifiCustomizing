//
//  WifiCustomizing.cpp
//  
//
//  Created by Roman Hayer on 08.05.18.
//

#include "WifiCustomizing.h"
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define IDX_SSID 0
#define IDX_PWD 1

// some convenience macros

#define LOG(a,b) \
if (this->logger != NULL) {\
this->logger->log(a,b);\
}

#define LOG1(a,b,c) \
if (this->logger != NULL) {\
this->logger->log(a,b,c);\
}

#define LOG2(a,b,c,d) \
if (this->logger != NULL) {\
this->logger->log(a,b,c,d);\
}

const byte dataSavedFlag = 0b10101010;



// constructor/destructor
WifiCustomizing::WifiCustomizing(Logging *logger)
{
    
    this->logger = logger;
    //this->ssidAP = ssidAP;
    this->customizingServer = new ESP8266WebServer(80);
    
    // initialize customizing list
    this->addParameter("ssid","Wifi ssid","the ssid of your wifi",32,32);
    this->addParameter("pwd","Wifi Password","the password of your wifi",32,64);
    
}

WifiCustomizing::~WifiCustomizing()
{
    delete this->customizingServer;
}

void WifiCustomizing::addParameter(char *name, char *label,char *placeholder,int size, int maxlength)
{
    CustomizingEntry *entry = new CustomizingEntry();
    entry->name = name;
    entry->label = label;
    entry->placeholder = placeholder;
    entry->size = size;
    entry->maxlength = maxlength;
    entry->value = NULL;
    this->customizingList.add(entry);
}

void WifiCustomizing::finishParameters()
{
    LOG(Logging::INFO,"Finishing parameters");
    int requiredMemorySize = 0;
    // allocate space in EEPROM
    CustomizingEntry *entry;
    for (int i=0; i<this->customizingList.size(); i++) {
        entry = this->customizingList.get(i);
        requiredMemorySize += entry->maxlength;
        
        LOG2(Logging::INFO,"Name: %s, Label:%s",entry->name,entry->label);
    }
    
    EEPROM.begin(requiredMemorySize+sizeof(dataSavedFlag)); // or simply EEPROM.begin(512)
}

/***
    load string from EEPROM until either maxlength or '0'
 **/
 
int WifiCustomizing::loadDynamic(int startPos,char *value,int maxLength)
{
    LOG1(Logging::INFO,"loadDynamic max:%d",maxLength);
    int i=0;
    do
    {
        byte b = EEPROM.read(i+startPos);
        if (isalnum(b) || ispunct(b) || b==0) {
            value[i] = (char)b;
        } else {
            value[i]='?';
        }
        
        i++;
    } while (value[i-1] != '\0' && i<maxLength);
    if (i==maxLength) {
        value[maxLength] = '\0';
    }
    Serial.println();
    LOG2(Logging::INFO,"loadDynamic(%d,%s)",startPos,value);
    return i+startPos;
}

bool WifiCustomizing::loadCustomizing()
{
    char valueBuffer[64];
    int loadPosition = 0;
    byte savedFlag;
    EEPROM.get(loadPosition, savedFlag);
    loadPosition += sizeof(savedFlag);
    
    if (savedFlag == dataSavedFlag) { // we found meaningful values}
        CustomizingEntry *entry;
        for (int i=0; i<this->customizingList.size(); i++) {
            entry = this->customizingList.get(i);

            loadPosition = loadDynamic(loadPosition,valueBuffer,entry->maxlength);
            
            if (entry->value != NULL) {
                free(entry->value);
            }
            
            // values are dynamically managed on heap
            entry->value = (char *)malloc(strlen(valueBuffer)); // only til '\0'
            strcpy(entry->value,valueBuffer);
        }
        return true;
    } else {
        return false;
    }
}

int WifiCustomizing::saveDynamic(int startPos,const char *value)
{
    LOG2(Logging::INFO,"SaveDynamic(%d,%s)",startPos,value);
    int i=0;
    do
    {
        EEPROM.write(i+startPos,(byte)value[i]);
        i++;
    } while (value[i-1] != '\0');
    
    return i+startPos;
}



void WifiCustomizing::saveCustomizing()
{
    int savePosition = 0;
    EEPROM.put(savePosition,dataSavedFlag); // first mark the memory as used
    savePosition +=sizeof(dataSavedFlag);
    
    CustomizingEntry *entry;
    for (int i=0; i<this->customizingList.size(); i++) {
        entry = this->customizingList.get(i);
        char *value = entry->value;
        savePosition = saveDynamic(savePosition, value);
    }
    EEPROM.commit();
}


char *WifiCustomizing::getValue(const char *name)
{
    CustomizingEntry *entry;
    for (int i=0; i<this->customizingList.size(); i++) {
        entry = this->customizingList.get(i);
        if (strcmp(entry->name,name)==0) {
            return entry->value;
        }
    }
    return NULL;
}

char *WifiCustomizing::getValue(int index)
{
    CustomizingEntry *entry;
    entry = this->customizingList.get(index);
    return entry->value;
}


bool WifiCustomizing::connect()
{
    //check, if wifi credentials are already in EEPROM
    if (this->loadCustomizing()) {
        LOG(Logging::INFO,"Credentials found ...");
        if (connectToWifi()) {
            return true;
        } else {
            return false;
        }
    } else {
        LOG(Logging::INFO,"No credential in EEPROM, please connect to AP and customize");
        return false;
    }
}


void WifiCustomizing::setupWifiAP()
{
    IPAddress    apIP(42, 42, 42, 42);
    
    LOG1(Logging::INFO,"Launching AP: %s",this->ssidAP.c_str())
    //set-up the custom IP address
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));   // subnet FF FF FF 00
    
    // generate a unique name for the accesspoint
    String APName = WiFi.macAddress();
    APName.replace(":","_");
    
    /* You can remove the password parameter if you want the AP to be open. */
    WiFi.softAP(APName.c_str(),"configpwd");
    // TODO: captive portal???
    
    IPAddress myIP = WiFi.softAPIP();
    setupWebServer();
    
}

bool WifiCustomizing::connectToWifi()
{
    String ssid = String(((CustomizingEntry *)(this->customizingList.get(IDX_SSID)))->value);
    String pwd = String(((CustomizingEntry *)(this->customizingList.get(IDX_PWD)))->value);
    LOG2(Logging::INFO,"Wifi.begin(%s,%s)",ssid.c_str(),pwd.c_str());
    WiFi.begin(ssid.c_str(), pwd.c_str());
    
    // connect to the WIFI
    int tryCounter = 20;   //try 20 seconds to connect ==> should be increased in reality
    while ((WiFi.status() != WL_CONNECTED) && (tryCounter > 0) )
    {
        tryCounter--;
        Serial.print(".");
        int lastMillis=millis();
        while (millis() - lastMillis < 1000) {
            //yield();
            /*
            if (client.connected()) {
                client.loop();
            }
            customizingServer.handleClient();*/
            this->customizingServer->handleClient();
        }
    }
    
    if (tryCounter==0) {
        LOG(Logging::INFO,"Connect to wifi failed");
        LOG(Logging::INFO,"data should be configured anew via accesspoint");
        WiFi.disconnect();
        return false;
    } else {
        LOG(Logging::INFO,"Connected");
        
        return true;
    }
}

// webserver stuff
void WifiCustomizing::setupWebServer()
{
    LOG(Logging::INFO,"Setting up webserver");
    // setup Webserver
    this->customizingServer->on ( "/", std::bind(&WifiCustomizing::handleRoot, this) );
    this->customizingServer->on ( "/submit", std::bind(&WifiCustomizing::handleRoot, this) );
    //this->customizingServer->on ( "/submit", std::bind(&WifiCustomizing::handleRoot, this));
//    customizingServer.onNotFound ( handleNotFound );
    
    this->customizingServer->begin();
}

void WifiCustomizing::handleClient()
{
    this->customizingServer->handleClient();
}


void WifiCustomizing::handleRoot()
{
    if (this->customizingServer->args() > 0 ) {
        for ( uint8_t i = 0; i < customizingServer->args(); i++ ) {
            String argValue = customizingServer->arg(i);
            
            argValue.trim();
            int length = argValue.length();
            LOG2(Logging::INFO,"handleRoot: argValue=%s, Length: %d",argValue.c_str(),length);
            
            CustomizingEntry *entry = this->customizingList.get(i);
            
            if (entry->value != NULL) {
                free(entry->value);
            }
            entry->value = (char *)malloc(length+1);
            strncpy(entry->value,argValue.c_str(),length+1);
        }
        saveCustomizing();
        connectToWifi();
    } else { //nothing sent via submit ==> initial call of website
        loadCustomizing();
        
    }
    
    LOG(Logging::INFO,"handleRoot, sending HTML");
    sendHTML();
}

String WifiCustomizing::htmlFromCustomizingEntry(CustomizingEntry *entry)
{
    /* String to be created:
     <tr><td>SSID:    </td><td> <input type='text' value='%s' name='ssid' size='%d' placeholder=\"Wifi SSID\" maxlength='%d' ></td></tr>\
     */
    
    String retval = String("<tr><td>" + String(entry->label));
    retval += String("</td><td> <input type='text' value='" + String((entry->value==NULL)?"":entry->value)); // initialize values with empty string
    retval += String("' name='" + String(entry->name));
    retval += String("' size='" + String(entry->size));
    retval += String("' placeholder='" + String(entry->placeholder));
    retval += String("' maxlength='" + String(entry->maxlength));
    retval += String("' ></td></tr>" );
    
    return retval;
}

void WifiCustomizing::sendHTML()
{
    LOG(Logging::INFO,"Enter sendHTMLPage");
    String html="<html>\
                    <head>\
                    <title>Distance Sensor</title>\
                    <style>\
                    body { background-color: #87ceff; font-family: Arial, Helvetica, Sans-Serif; font-size: 36px; Color: #000000; }\
                    td {font-size: 36px}\
                    h1 { Color: #AA0000; }\
                    input { font-size: 36px; }\
                    </style>\
                    </head>\
                    <body>\
                    <form action='http://42.42.42.42/submit' method='post'>\
                    <h1>Distance Sensor Customizing</h1>\
                    <h2>Credentials of your WiFi</h2>\
    <table>";
    
    for (int i=0; i<this->customizingList.size(); i++) {
        CustomizingEntry *entry = this->customizingList.get(i);
        html += htmlFromCustomizingEntry(entry);
    }
    
    html += "</table>\
                <input type='submit' value='Speichern'>\
                </form>\
                </body>\
                </html>";
    
    customizingServer->send ( 200, "text/html", html.c_str() );
}


