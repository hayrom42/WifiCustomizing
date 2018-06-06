//
//  WifiCustomizing.h
//  
//
//  Created by Roman Hayer on 08.05.18.
//

#ifndef WifiCustomizing_h
#define WifiCustomizing_h
#include <Arduino.h>
#include <Logging.h>
#include <ESP8266WebServer.h>
#include "LinkedList.h"



/*
struct Customizing {
    byte initialFlag;   // flag to check whether values in EEPROM are initial
    char ssid[32];
    char pwd[64];
    char mqtt_server[64];
    char mqtt_user[16];
    char mqtt_pwd[16];
};
*/
class CustomizingEntry {
public:
    char *name;
    char *label;
    char *placeholder;
    int size; // size of text field in customizing UI
    int maxlength;
    char *value;
};

class WifiCustomizing {
private:
  
    LinkedList<CustomizingEntry*> customizingList = LinkedList<CustomizingEntry*>();
    
    Logging *logger;
    String ssidAP;
    ESP8266WebServer *customizingServer;
    
    
    bool connectToWifi();
    bool loadCustomizing();
    void saveCustomizing();
    void setupWebServer();
    void sendHTML();
    String htmlFromCustomizingEntry(CustomizingEntry* entry);
    int saveDynamic(int startPos,const char *value);
    int loadDynamic(int startPos,char *value, int maxLength);
    
    
public:
    WifiCustomizing(Logging *logger=NULL);
    ~WifiCustomizing();
    
    void addParameter(char *name, char*label,char* placeholder,int size, int maxlength);
    void finishParameters();
    char *getValue(const char *name);
    char *getValue(int index);

    
    void setupWifiAP();
    bool connect();
    void handleClient();
    void handleRoot();
    
};


#endif /* WifiCustomizing_h */
