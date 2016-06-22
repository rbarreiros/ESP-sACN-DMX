
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <LXESP8266UARTDMX.h>
#include <LXDMXWiFi.h>
#include <LXWiFiArtNet.h>
#include <LXWiFiSACN.h>
#include <ESP8266WebServer.h>
#include <FS.h>

#include "LXDMXWiFiConfig.h"

#define STARTUP_MODE_PIN 16
#define DIRECTION_PIN     4
#define CONNECT_TIMEOUT  15 // Timeout to connect to AP in seconds

DMXWiFiConfig *espConfig;
LXDMXWiFi *iface;
WiFiUDP wUDP;
ESP8266WebServer server(80);

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path)
{
  Serial.print("handleFileRead: ");
  Serial.println(path);

  if(path.endsWith("/"))
    path += "index.html";

  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";

  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
  {
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void setup(void)
{
  Serial.begin(115200);
  //Serial.setDebugOutput(1);
  Serial.println("Starting...");

  // Setup Pins
  //pinMode(BUILTIN_LED, OUTPUT);
  pinMode(STARTUP_MODE_PIN, INPUT);
  pinMode(DIRECTION_PIN, OUTPUT);

  ESP8266DMX.startOutput();
  digitalWrite(DIRECTION_PIN, HIGH);

  EEPROM.begin(DMXWiFiConfigSIZE);
  espConfig = (DMXWiFiConfig*)EEPROM.getDataPtr();

  if(digitalRead(STARTUP_MODE_PIN) == 0)
  {
    initConfig(espConfig);
    Serial.println("Default Startup");
  } else if( strcmp(CONFIG_PACKET_IDENT, (const char*)espConfig) != 0)
  {
    initConfig(espConfig);
    EEPROM.write(8, 0);
    EEPROM.commit();
    Serial.println("Initialized EEPROM");
  } else
    Serial.println("EEPROM Read OK");

  if(espConfig->wifi_mode == AP_MODE || espConfig->wifi_mode == AP_STA_MODE)
  {
    WiFi.softAP(espConfig->ssid);
    WiFi.softAPConfig(espConfig->ap_address, espConfig->ap_gateway, espConfig->ap_subnet);
    Serial.print("Creating Access point ");
    Serial.print(espConfig->ssid);
    Serial.println(".");
  }

  if(espConfig->wifi_mode == STATION_MODE || espConfig->wifi_mode == AP_STA_MODE)
  {
    WiFi.begin(espConfig->ssid, espConfig->pwd);

    if(espConfig->protocol_mode & STATIC_MODE)
      WiFi.config(espConfig->sta_address, espConfig->sta_gateway, espConfig->sta_subnet);

    Serial.print("Connecting to accesspoint ");
    Serial.print(espConfig->ssid);
    Serial.print(" ");

    int startTime = millis();
    while(WiFi.status() != WL_CONNECTED)
    {
      delay(100);
      int currentTime = millis();

      if( ((currentTime - startTime) % 1000) == 0 )
        Serial.print(".");

      if(currentTime > (startTime + (CONNECT_TIMEOUT * 1000)))
      {
        Serial.println("\r\nConnection timed out");
        if(espConfig->wifi_mode != AP_STA_MODE)
        {
          Serial.println("Enabling AP");
          espConfig->wifi_mode = AP_STA_MODE;
          WiFi.softAP(espConfig->ssid);
          WiFi.softAPConfig(espConfig->ap_address, espConfig->ap_gateway, espConfig->ap_subnet);
        }
        break;
      }
    }
  }

  if(espConfig->wifi_mode == AP_STA_MODE)
    WiFi.mode(WIFI_AP_STA);
  else if(espConfig->wifi_mode == AP_MODE)
    WiFi.mode(WIFI_AP);
  else if(espConfig->wifi_mode == STATION_MODE)
    WiFi.mode(WIFI_STA);

  Serial.println("\r\nWiFi Started.");

  iface = new LXWiFiSACN();
  iface->setUniverse(espConfig->sacn_universe);

  Serial.println("Interface Created.");

  if((espConfig->protocol_mode & MULTICAST_MODE))
  { // Start listening for UDP on port
    if(espConfig->wifi_mode == AP_MODE)
      wUDP.beginMulticast(WiFi.softAPIP(), espConfig->multi_address, iface->dmxPort());
    else
      wUDP.beginMulticast(WiFi.localIP(), espConfig->multi_address, iface->dmxPort());
  } else {
    wUDP.begin(iface->dmxPort());
  }

  Serial.println("Setup OTA");

  ArduinoOTA.onStart([]() {
    Serial.println("Starting OTA Upgrade");
  });

  ArduinoOTA.onEnd([](){
    Serial.println("OTA Upgrade finished");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  Serial.println("OTA Setup Complete");
  Serial.println("Setting up Web Server");

  SPIFFS.begin();
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });

  server.on("/all", HTTP_GET, [](){
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += ", \"freesketchspace\":" + String(ESP.getFreeSketchSpace());
    json += ", \"analog\":" + String(analogRead(A0));
    json += ", \"gpio\":" + String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
    json += "}";
    server.send(200, "text/json", json);
    json = String();
  });
  server.begin();
  Serial.println("Web Server setup complete");

  Serial.println("Setup Complete.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop(void)
{
  // OTA
  ArduinoOTA.handle();

  // Web Server
  server.handleClient();

  // DMX
  uint8_t good_dmx = iface->readDMXPacket(&wUDP);

  if(good_dmx)
  {
     for(int i = 1; i <= iface->numberOfSlots(); i++)
        ESP8266DMX.setSlot(i , iface->getSlot(i));
     //blinkLED();
  }
  else // We should remove this config through UDP after the web server
  // interface is finished
  {
    if(strcmp(CONFIG_PACKET_IDENT, (const char *) iface->packetBuffer()) == 0)
    {  //match header to config packet
      Serial.print("ESP-DMX received, ");
      uint8_t reply = 0;
      if(iface->packetBuffer()[8] == '?')
      {  //packet opcode is query
        EEPROM.begin(DMXWiFiConfigSIZE);            //deletes data (ie esp_config) and reads from flash
        espConfig = (DMXWiFiConfig*)EEPROM.getDataPtr();
        reply = 1;
      }
      else if((iface->packetBuffer()[8] == '!') && (iface->packetSize() >= 171))
      { //packet opcode is set
        int k;
        uint8_t* p = (uint8_t*) espConfig;
        for(k=0; k<171; k++) {
          p[k] = iface->packetBuffer()[k]; //copy packet to config
        }

        espConfig->opcode = 0;               // reply packet opcode is data
        if(iface->packetSize() >= 203)
        {
          for(k=171; k < iface->packetSize(); k++) {
            p[k] = iface->packetBuffer()[k]; //copy node_name to config
          }
          strcpy(((LXWiFiArtNet*)iface)->longName(), (char*)&iface->packetBuffer()[171]);
        }
        EEPROM.write(8,0);  //opcode is zero for data (also sets dirty flag)
        EEPROM.commit();
        reply = 1;
        Serial.print("eprom written, ");
      } else {
        Serial.println("packet error.");
      }

      if(reply)
      {
        erasePassword(espConfig);                  //don't show pwd if queried
        wUDP.beginPacket(wUDP.remoteIP(), iface->dmxPort());
        wUDP.write((uint8_t*)espConfig, sizeof(*espConfig));
        wUDP.endPacket();
        Serial.println("reply complete.");
      }
      iface->packetBuffer()[0] = 0; //insure loop without recv doesn't re-trgger
    } // packet has ESP-DMX header
  }   // not good_dmx
}
