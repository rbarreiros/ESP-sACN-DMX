#include <IPAddress.h>
#include "LXDMXWiFiConfig.h"
#include <ESP8266WiFi.h>

void initConfig(DMXWiFiConfig* cfptr) {
  //zero the complete config struct
  memset(cfptr, 0, DMXWiFiConfigSIZE);

  strncpy((char*)cfptr, CONFIG_PACKET_IDENT, 8); //add ident
  //strncpy(cfptr->ssid, "ESP-DMX-WiFi", 63);
  snprintf(cfptr->ssid, 63, "ESP-DMX-WiFi-%06x", ESP.getChipId());
  strncpy(cfptr->pwd, "*****", 63);
  cfptr->wifi_mode = AP_STA_MODE;         // AP_MODE, STATION_MODE or AP_STA_MODE
  //cfptr->protocol_mode = STATIC_MODE; // STATIC_MODE | MULTICAST_MODE
    // ARTNET_MODE or SACN_MODE ( plus optional: | STATIC_MODE, | MULTICAST_MODE, | INPUT_TO_NETWORK_MODE )
                                        // eg. cfptr->protocol_mode = SACN_MODE | MULTICAST_MODE ;
  cfptr->ap_chan = 2;
  cfptr->ap_address    = IPAddress(192,168,4,1);       // ip address of access point
  cfptr->ap_gateway    = IPAddress(192,168,4,1);
  cfptr->ap_subnet     = IPAddress(255,255,255,0);       // match what is passed to dchp connection from computer
  cfptr->sta_address   = IPAddress(192,168,1,69);       // station's static address for STATIC_MODE
  cfptr->sta_gateway   = IPAddress(192,168,1,1);
  cfptr->sta_subnet    = IPAddress(255,255,255,0);
  cfptr->multi_address = IPAddress(239,255,0,1);         // sACN multicast address should match universe
  cfptr->sacn_universe   = 1;
  cfptr->artnet_universe = 0;
  cfptr->artnet_subnet   = 0;
  strcpy((char*)cfptr->node_name, "esp_wifi_node");
  cfptr->input_address = IPAddress(10,255,255,255);
}

void erasePassword(DMXWiFiConfig* cfptr) {
  strncpy(cfptr->pwd, "********", 63);
}
