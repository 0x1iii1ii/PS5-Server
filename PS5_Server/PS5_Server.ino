#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266WebServerSecure.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include "psfree.h"
#include "offsets.h"
#include "module.h"


                     // use LITTLEFS not SPIFFS [ true / false ]
#define USELFS true // LITTLEFS will be used instead of SPIFFS for the storage filesystem.
                     // you can find the littlefs sketch data upload tool here https://github.com/earlephilhower/arduino-esp8266littlefs-plugin/releases

                     // enable internal etahen.h [ true / false ]
#define INTHEN false  // etahen is placed in the app partition to free up space on the storage for other payloads. 
                     // with this enabled you do not upload etahen to the board, set this to false if you wish to upload etahen.

                      // enable autohen [ true / false ]
#define AUTOHEN false // this will load etaHen instead of the normal index/payload selection page, use this if you only want hen and no other payloads.
                      // pressing R2 on the controller will stop the auto load and allow you to select between the psfree and fontface exploits.
                      // if you wish to update the etaHen payload name it "etahen.bin" and upload it to the board storage to override the internal copy. 

#include "elfldr.h"
#include "etahen.h"
#include "exploit.h"
#if USELFS
#include <LittleFS.h>
#define FILESYS LittleFS 
#else
#define FILESYS SPIFFS 
#endif


ADC_MODE(ADC_VCC);
MD5Builder md5;
DNSServer dnsServer;
ESP8266WebServer webServer;
ESP8266WebServerSecure sWebServer(443);
File upFile;
String firmwareVer = "1.00";


//-------------------DEFAULT SETTINGS------------------//

//create access point
boolean startAP = true;
String AP_SSID = "PS5_WEB_AP";
String AP_PASS = "password";
IPAddress Server_IP(10,1,1,1);
IPAddress Subnet_Mask(255,255,255,0);

//connect to wifi
boolean connectWifi = false;
String WIFI_SSID = "Home_WIFI";
String WIFI_PASS = "password";
String WIFI_HOSTNAME = "ps5.local";

//server port
int WEB_PORT = 80;

//-----------------------------------------------------//

static const char serverCert[] = "-----BEGIN CERTIFICATE-----\r\nMIIC1DCCAj2gAwIBAgIUFQgjEtkNYfmrrpNQKHVNl3+dl08wDQYJKoZIhvcNAQEL\r\nBQAwfDELMAkGA1UEBhMCVVMxEzARBgNVBAgMCkNhbGlmb3JuaWExEDAOBgNVBAcM\r\nB0ZyZW1vbnQxDDAKBgNVBAoMA2VzcDEMMAoGA1UECwwDZXNwMQwwCgYDVQQDDANl\r\nc3AxHDAaBgkqhkiG9w0BCQEWDWVzcEBlc3AubG9jYWwwHhcNMjEwMjIxMDAwMDQ4\r\nWhcNNDMwNzI4MDAwMDQ4WjB8MQswCQYDVQQGEwJVUzETMBEGA1UECAwKQ2FsaWZv\r\ncm5pYTEQMA4GA1UEBwwHRnJlbW9udDEMMAoGA1UECgwDZXNwMQwwCgYDVQQLDANl\r\nc3AxDDAKBgNVBAMMA2VzcDEcMBoGCSqGSIb3DQEJARYNZXNwQGVzcC5sb2NhbDCB\r\nnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEAsrfFqlV5H0ajdAkkZ51HTOseOjYj\r\nNiaUD4MA5mIRonnph6EKIWb9Yl85vVa6yfVkGn3TFebQ96MMdTfZgLuP4ryCwe6Y\r\n+tZs2g6TjGbR0O6yuA8wQ2Ln7E0T05C8oOl88SGNV4tVL6hz64oMzuVebVDo0J9I\r\nybvL0O/LhMvC4x8CAwEAAaNTMFEwHQYDVR0OBBYEFCMQIU+pZQDVySXejfbIYbLQ\r\ncLXiMB8GA1UdIwQYMBaAFCMQIU+pZQDVySXejfbIYbLQcLXiMA8GA1UdEwEB/wQF\r\nMAMBAf8wDQYJKoZIhvcNAQELBQADgYEAFHPz3YhhXQYiERTGzt8r0LhNWdggr7t0\r\nWEVuAoEukjzv+3DVB2O+56NtDa++566gTXBGGar0pWfCwfWCEu5K6MBkBdm6Ub/A\r\nXDy+sRQTqH/jTFFh5lgxeq246kHWHGRad8664V5PoIh+OSa0G3CEB+BXy7WF82Qq\r\nqx0X6E/mDUU=\r\n-----END CERTIFICATE-----";
static const char serverKey[] = "-----BEGIN PRIVATE KEY-----\r\nMIICdgIBADANBgkqhkiG9w0BAQEFAASCAmAwggJcAgEAAoGBALK3xapVeR9Go3QJ\r\nJGedR0zrHjo2IzYmlA+DAOZiEaJ56YehCiFm/WJfOb1Wusn1ZBp90xXm0PejDHU3\r\n2YC7j+K8gsHumPrWbNoOk4xm0dDusrgPMENi5+xNE9OQvKDpfPEhjVeLVS+oc+uK\r\nDM7lXm1Q6NCfSMm7y9Dvy4TLwuMfAgMBAAECgYEApKFbSeyQtfnpSlO9oGEmtDmG\r\nT9NdHl3tWFiydId0fTpWoKT9YwWvdnYIB12klbQicbDkyTEl4Gjnafd3ufmNsaH8\r\nZ9twopIdvvWDvGPIqGNjvTYcuczpXmQWiUnG5OTiVWI1XuZa3uZEGSFK9Ra6bE4g\r\nG2xklGZGdaqqcd6AVhECQQDnBXVXwBxExxSFppL8KUtWgyXAvJAEvkzvTOQfcCel\r\naIM5EEUofB7WZeMtDEKgBtoBl+i5PP+GnDF0zsjDFx2nAkEAxgqVQii6zURSVE2T\r\niJDihySXJ2bmLJUjRIi1nCs64I9Oz4fECVvGwZ1XU8Uzhh3ylyBSG2HjhzA5sTSC\r\n1a/tyQJAOgE12EWFE4PE1FXhm+ymXN9q8DyoEHjTilYNBRO88JwQLpi2NJcNixlj\r\n8+CbLeDqhfHlXfVB10OKa2CsKce5CwJAbhaN+DQJ+3dCSOjC3YSk2Dkn6VhTFW9m\r\nJn/UbNa/KPug9M5k1Er3RsO/OqsBxEk7hHUMD3qv74OIXpBxNnZQuQJASlwk5HZT\r\n7rULkr72fK/YYxkS0czBDIpTKqwklxU+xLSGWkSHvSvl7sK4TmQ1w8KVpjKlTCW9\r\nxKbbW0zVmGN6wQ==\r\n-----END PRIVATE KEY-----";

String defaultIndex = "<!DOCTYPE html><html><head><style>body { background-color: #1451AE;color: #ffffff;font-size: 14px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}</style></head><center><br><br><br><br><br><br>ESP</center></html>";


String split(String str, String from, String to)
{
  String tmpstr = str;
  tmpstr.toLowerCase();
  from.toLowerCase();
  to.toLowerCase();
  int pos1 = tmpstr.indexOf(from);
  int pos2 = tmpstr.indexOf(to, pos1 + from.length());   
  String retval = str.substring(pos1 + from.length() , pos2);
  return retval;
}


bool instr(String str, String search)
{
int result = str.indexOf(search);
if (result == -1)
{
  return false;
}
return true;
}


String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+" B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+" KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+" MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+" GB";
  }
}


String urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
      }
      yield();
    }
    encodedString.replace("%2E",".");
    return encodedString;
}



void sendwebmsg(String htmMsg)
{
    String tmphtm = "<!DOCTYPE html><html><head><style>body { background-color: #1451AE;color: #ffffff;font-size: 14px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}</style></head><center><br><br><br><br><br><br>" + htmMsg + "</center></html>";
    webServer.setContentLength(tmphtm.length());
    webServer.send(200, "text/html", tmphtm);
}


String errorMsg(int errnum)
{
  if(errnum == UPDATE_ERROR_OK){
    return "No Error";
  } else if(errnum == UPDATE_ERROR_WRITE){
    return "Flash Write Failed";
  } else if(errnum == UPDATE_ERROR_ERASE){
    return "Flash Erase Failed";
  } else if(errnum == UPDATE_ERROR_READ){
    return "Flash Read Failed";
  } else if(errnum == UPDATE_ERROR_SPACE){
    return "Not Enough Space";
  } else if(errnum == UPDATE_ERROR_SIZE){
    return "Bad Size Given";
  } else if(errnum == UPDATE_ERROR_STREAM){
    return "Stream Read Timeout";
  } else if(errnum == UPDATE_ERROR_MD5){
    return "MD5 Check Failed";
  } else if(errnum == UPDATE_ERROR_FLASH_CONFIG){
     return "Flash config wrong real: " + String(ESP.getFlashChipRealSize()) + "<br>IDE: " + String( ESP.getFlashChipSize());
  } else if(errnum == UPDATE_ERROR_NEW_FLASH_CONFIG){
    return "new Flash config wrong real: " + String(ESP.getFlashChipRealSize());
  } else if(errnum == UPDATE_ERROR_MAGIC_BYTE){
    return "Magic byte is wrong, not 0xE9";
  } else if (errnum == UPDATE_ERROR_BOOTSTRAP){
    return "Invalid bootstrapping state, reset ESP8266 before updating";
  } else {
    return "UNKNOWN";
  }
}


String getContentType(String filename){
  if(filename.endsWith(".htm")) return "text/html";
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
  else if(filename.endsWith(".bin")) return "application/octet-stream";
  else if(filename.endsWith(".manifest")) return "text/cache-manifest";
  return "text/plain";
}


void handleBinload(String pload, int port)
{
 int scode;
  WiFiClient client;
  if (!client.connect(webServer.client().remoteIP(), port)) {
    delay(1000);
    scode = 400;
  }
  else
  {
     delay(1000);
     File dataFile = FILESYS.open(pload, "r");
     if (dataFile) {
       while (dataFile.available()) {
         client.write(dataFile.read());
       }
    dataFile.close(); 
  }
  client.stop();
  scode = 200;
  }
  String tmphtm = "0";
  webServer.setContentLength(tmphtm.length());
  webServer.send(scode, "text/html", tmphtm);
}

bool loadFromSdCard(String path) {
 path = webServer.urlDecode(path);
 //Serial.println(path);
 if (path.equals("/connecttest.txt"))
 {
  webServer.setContentLength(22);
  webServer.send(200, "text/plain", "Microsoft Connect Test");
  return true;
 }
 
 if (path.equals("/config.ini"))
 {
  return false;
 }
 
 if (path.equals("/"))
 {
   path = "/index.html";
 }
 
  String dataType = getContentType(path);

  if (path.endsWith("/index.html"))
  {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), index_gz, sizeof(index_gz));
    return true;
  }

  if (path.endsWith("/1.00.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o1_00_gz, sizeof(o1_00_gz));
    return true;
  }

  if (path.endsWith("/1.01.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o1_01_gz, sizeof(o1_01_gz));
    return true;
  }

  if (path.endsWith("/1.02.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o1_02_gz, sizeof(o1_02_gz));
    return true;
  }

  if (path.endsWith("/1.05.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o1_05_gz, sizeof(o1_05_gz));
    return true;
  }

  if (path.endsWith("/1.10.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o1_10_gz, sizeof(o1_10_gz));
    return true;
  }

  if (path.endsWith("/1.11.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o1_11_gz, sizeof(o1_11_gz));
    return true;
  }

  if (path.endsWith("/1.12.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o1_12_gz, sizeof(o1_12_gz));
    return true;
  }

  if (path.endsWith("/1.13.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o1_13_gz, sizeof(o1_13_gz));
    return true;
  }

  if (path.endsWith("/1.14.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o1_14_gz, sizeof(o1_14_gz));
    return true;
  }

  if (path.endsWith("/2.00.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o2_00_gz, sizeof(o2_00_gz));
    return true;
  }

  if (path.endsWith("/2.20.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o2_20_gz, sizeof(o2_20_gz));
    return true;
  }

  if (path.endsWith("/2.25.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o2_25_gz, sizeof(o2_25_gz));
    return true;
  }

  if (path.endsWith("/2.26.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o2_26_gz, sizeof(o2_26_gz));
    return true;
  }

  if (path.endsWith("/2.30.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o2_30_gz, sizeof(o2_30_gz));
    return true;
  }

  if (path.endsWith("/2.50.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o2_50_gz, sizeof(o2_50_gz));
    return true;
  }

  if (path.endsWith("/2.70.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o2_70_gz, sizeof(o2_70_gz));
    return true;
  }

  if (path.endsWith("/3.00.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o3_00_gz, sizeof(o3_00_gz));
    return true;
  }

  if (path.endsWith("/3.10.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o3_10_gz, sizeof(o3_10_gz));
    return true;
  }

  if (path.endsWith("/3.20.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o3_20_gz, sizeof(o3_20_gz));
    return true;
  }

  if (path.endsWith("/3.21.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o3_21_gz, sizeof(o3_21_gz));
    return true;
  }

  if (path.endsWith("/4.00.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o4_00_gz, sizeof(o4_00_gz));
    return true;
  }

  if (path.endsWith("/4.02.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o4_02_gz, sizeof(o4_02_gz));
    return true;
  }

  if (path.endsWith("/4.03.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o4_03_gz, sizeof(o4_03_gz));
    return true;
  }

  if (path.endsWith("/4.50.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o4_50_gz, sizeof(o4_50_gz));
    return true;
  }

  if (path.endsWith("/4.51.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o4_51_gz, sizeof(o4_51_gz));
    return true;
  }

  if (path.endsWith("/5.00.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o5_00_gz, sizeof(o5_00_gz));
    return true;
  }

  if (path.endsWith("/5.02.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o5_02_gz, sizeof(o5_02_gz));
    return true;
  }

  if (path.endsWith("/5.10.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o5_10_gz, sizeof(o5_10_gz));
    return true;
  }

  if (path.endsWith("/5.50.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), o5_50_gz, sizeof(o5_50_gz));
    return true;
  }

  if (path.endsWith("/psfree/alert.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), alert_gz, sizeof(alert_gz));
    return true;
  }

  if (path.endsWith("/psfree/config.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), config_gz, sizeof(config_gz));
    return true;
  }

  if (path.endsWith("/psfree/psfree.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), psfreeo_gz, sizeof(psfreeo_gz));
    return true;
  }
  if (path.endsWith("/psfree/module/int64.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), int64m_gz, sizeof(int64m_gz));
    return true;
  }
  if (path.endsWith("/psfree/module/offset.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), offset_gz, sizeof(offset_gz));
    return true;
  }
  if (path.endsWith("/psfree/module/utils.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), utils_gz, sizeof(utils_gz));
    return true;
  }

  if (path.endsWith("/psfree/module/chain.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), chain_gz, sizeof(chain_gz));
    return true;
  }

  if (path.endsWith("/psfree/module/mem.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), mem_gz, sizeof(mem_gz));
    return true;
  }

  if (path.endsWith("/psfree/module/memtools.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), memtools_gz, sizeof(memtools_gz));
    return true;
  }

  if (path.endsWith("/psfree/module/rw.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), rw_gz, sizeof(rw_gz));
    return true;
  }

  if (path.endsWith("/custom_host_stuff.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), custom_host_stuff_gz, sizeof(custom_host_stuff_gz));
    return true;
  }

  if (path.endsWith("/int64.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), int64_gz, sizeof(int64_gz));
    return true;
  }

  if (path.endsWith("/main.css")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), main_gz, sizeof(main_gz));
    return true;
  }

  if (path.endsWith("/main.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), mainj_gz, sizeof(mainj_gz));
    return true;
  }

  if (path.endsWith("/rop.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), rop_gz, sizeof(rop_gz));
    return true;
  }

  if (path.endsWith("/rop_slave.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), rop_slave_gz, sizeof(rop_slave_gz));
    return true;
  }

  if (path.endsWith("/syscalls.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), syscalls_gz, sizeof(syscalls_gz));
    return true;
  }

  if (path.endsWith("/umtx2.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), umtx2_gz, sizeof(umtx2_gz));
    return true;
  }

  if (path.endsWith("/elfldr.elf")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), elfldr_gz, sizeof(elfldr_gz));
    return true;
  }

  if (path.endsWith("/payload_map.js")) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.send(200, dataType.c_str(), payload_map_gz, sizeof(payload_map_gz));
    return true;
  }

  // if (path.endsWith("/payload_map.js")) {
  //   handlePayloads();
  //   return true;
  // }

  bool isGzip = false;

  File dataFile;
  dataFile = FILESYS.open(path + ".gz", "r");
  if (!dataFile) {
    dataFile = FILESYS.open(path, "r");
  }
  else
  {
    isGzip = true;
  }
  
  if (!dataFile) {
     if (path.endsWith("index.html") || path.endsWith("index.htm"))
     {
        webServer.send(200, "text/html", defaultIndex);
        return true;
     }
#if INTHEN       
     if (path.endsWith("/etahen.bin"))
     {
        webServer.sendHeader("Content-Encoding", "gzip");
        webServer.send(200, dataType.c_str(), etahen_gz, sizeof(etahen_gz));
        return true;
     }
#endif     
    return false;
  }
  if (webServer.hasArg("download")) {
    dataType = "application/octet-stream";
    String dlFile = path;
    if (dlFile.startsWith("/"))
    {
     dlFile = dlFile.substring(1);
    }
    webServer.sendHeader("Content-Disposition", "attachment; filename=\"" + dlFile + "\"");
    webServer.sendHeader("Content-Transfer-Encoding", "binary");
  }
  else
  {
    if (isGzip && path.endsWith(".bin"))
    {
      webServer.sendHeader("Content-Encoding", "gzip");
    }
  }
  if (webServer.streamFile(dataFile, dataType) != dataFile.size()) {
    //Serial.println("Sent less data than expected!");
  }
  dataFile.close();
  return true;
}


void handleNotFound() {
  if (loadFromSdCard(webServer.uri())) {
    return;
  }
  String message = "\n\n";
  message += "URI: ";
  message += webServer.uri();
  message += "\nMethod: ";
  message += (webServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += webServer.args();
  message += "\n";
  for (uint8_t i = 0; i < webServer.args(); i++) {
    message += " NAME:" + webServer.argName(i) + "\n VALUE:" + webServer.arg(i) + "\n";
  }
  webServer.send(404, "text/plain", "Not Found");
  //Serial.print(message);
}

void handleFileUpload() {
  if (webServer.uri() != "/upload.html") {
    webServer.send(500, "text/plain", "Internal Server Error");
    return;
  }
  HTTPUpload& upload = webServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    if (filename.equals("/config.ini"))
    {return;}
    upFile = FILESYS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (upFile) {
      upFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (upFile) {
      upFile.close();
    }
  }
}


void handleFwUpdate() {
  if (webServer.uri() != "/update.html") {
    sendwebmsg("Error");
    return;
  }
  HTTPUpload& upload = webServer.upload();
  if (upload.filename != "fwupdate.bin") {
    sendwebmsg("Invalid update file: " + upload.filename);
    return;
  }
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    upFile = FILESYS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (upFile) {
      upFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (upFile) {
      upFile.close();
    }
    updateFw();
  }
}


void updateFw()
{
  if (FILESYS.exists("/fwupdate.bin")) {
  File updateFile;
  //Serial.println("Update file found");
  updateFile = FILESYS.open("/fwupdate.bin", "r");
 if (updateFile) {
  size_t updateSize = updateFile.size();
   if (updateSize > 0) {   
    md5.begin();
    md5.addStream(updateFile,updateSize);
    md5.calculate();
    String md5Hash = md5.toString();
    //Serial.println("Update file hash: " + md5Hash);
    updateFile.close();
    updateFile = FILESYS.open("/fwupdate.bin", "r");
  if (updateFile) {
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace, U_FLASH)) {
    //Update.printError(Serial);
    digitalWrite(BUILTIN_LED, HIGH);
    updateFile.close();
    sendwebmsg("Update failed<br><br>" + errorMsg(Update.getError()));
    return;
    }
    int md5BufSize = md5Hash.length() + 1;
    char md5Buf[md5BufSize];
    md5Hash.toCharArray(md5Buf, md5BufSize) ;
    Update.setMD5(md5Buf);
    //Serial.println("Updating firmware...");
   long bsent = 0;
   int cprog = 0;
    while (updateFile.available()) {
    uint8_t ibuffer[1];
    updateFile.read((uint8_t *)ibuffer, 1);
    Update.write(ibuffer, sizeof(ibuffer));
      bsent++;
      int progr = ((double)bsent /  updateSize)*100;
      if (progr >= cprog) {
        cprog = progr + 10;
      //Serial.println(String(progr) + "%");
      }
    }
    updateFile.close(); 
  if (Update.end(true))
  {
  digitalWrite(BUILTIN_LED, HIGH);
  //Serial.println("Installed firmware hash: " + Update.md5String()); 
  //Serial.println("Update complete");
  FILESYS.remove("/fwupdate.bin");
  sendwebmsg("Uploaded file hash: " + md5Hash + "<br>Installed firmware hash: " + Update.md5String() + "<br><br>Update complete, Rebooting.");
  delay(1000);
  ESP.restart();
  }
  else
  {
    digitalWrite(BUILTIN_LED, HIGH);
    //Serial.println("Update failed");
    sendwebmsg("Update failed");
     //Update.printError(Serial);
    }
  }
  }
  else {
  //Serial.println("Error, file is invalid");
  updateFile.close(); 
  digitalWrite(BUILTIN_LED, HIGH);
  FILESYS.remove("/fwupdate.bin");
  sendwebmsg("Error, file is invalid");
  return;    
  }
  }
  }
  else
  {
    //Serial.println("No update file found");
    sendwebmsg("No update file found");
  }
}


void handleFormat()
{
  //Serial.print("Formatting Filesystem");
  FILESYS.end();
  FILESYS.format();
  FILESYS.begin();
  writeConfig();
  webServer.sendHeader("Location","/fileman.html");
  webServer.send(302, "text/html", "");
}


void handleDelete(){
  if(!webServer.hasArg("file")) 
  {
    webServer.sendHeader("Location","/fileman.html");
    webServer.send(302, "text/html", "");
    return;
  }
 String path = webServer.arg("file");
 if (FILESYS.exists("/" + path) && path != "/" && !path.equals("config.ini")) {
    FILESYS.remove("/" + path);
 }
   webServer.sendHeader("Location","/fileman.html");
   webServer.send(302, "text/html", "");
}


void handleFileMan() {
  Dir dir = FILESYS.openDir("/");
  String output = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>File Manager</title><style type=\"text/css\">a:link {color: #ffffff; text-decoration: none;} a:visited {color: #ffffff; text-decoration: none;} a:hover {color: #ffffff; text-decoration: underline;} a:active {color: #ffffff; text-decoration: underline;} table {font-family: arial, sans-serif; border-collapse: collapse; width: 100%;} td, th {border: 1px solid #dddddd; text-align: left; padding: 8px;} button {display: inline-block; padding: 1px; margin-right: 6px; vertical-align: top; float:left;} body {background-color: #1451AE;color: #ffffff; font-size: 14px; padding: 0.4em 0.4em 0.4em 0.6em; margin: 0 0 0 0.0;}</style><script>function statusDel(fname) {var answer = confirm(\"Are you sure you want to delete \" + fname + \" ?\");if (answer) {return true;} else { return false; }}</script></head><body><br><table id=filetable></table><script>var filelist = ["; 
  int fileCount = 0;
  while(dir.next()){
    File entry = dir.openFile("r");
    String fname = String(entry.name());
    if (fname.startsWith("/")){fname = fname.substring(1);}
    if (fname.length() > 0 && !fname.equals("config.ini"))
    {
      fileCount++;
      fname.replace("|","%7C");fname.replace("\"","%22");
      output += "\"" + fname + "|" + formatBytes(entry.size()) + "\",";
    }
    entry.close();
  }
  if (fileCount == 0)
  {
      output += "];</script><center>No files found<br>You can upload files using the <a href=\"/upload.html\" target=\"mframe\"><u>File Uploader</u></a> page.</center></p></body></html>";
  }
  else
  {
      output += "];var output = \"\";filelist.forEach(function(entry) {var splF = entry.split(\"|\"); output += \"<tr>\";output += \"<td><a href=\\\"\" +  splF[0] + \"\\\">\" + splF[0] + \"</a></td>\"; output += \"<td>\" + splF[1] + \"</td>\";output += \"<td><a href=\\\"/\" + splF[0] + \"\\\" download><button type=\\\"submit\\\">Download</button></a></td>\";output += \"<td><form action=\\\"/delete\\\" method=\\\"post\\\"><button type=\\\"submit\\\" name=\\\"file\\\" value=\\\"\" + splF[0] + \"\\\" onClick=\\\"return statusDel('\" + splF[0] + \"');\\\">Delete</button></form></td>\";output += \"</tr>\";}); document.getElementById(\"filetable\").innerHTML = output;</script></body></html>";
  }
  webServer.setContentLength(output.length());
  webServer.send(200, "text/html", output);
}



void handleConfig()
{
  if(webServer.hasArg("ap_ssid") && webServer.hasArg("ap_pass") && webServer.hasArg("web_ip") && webServer.hasArg("web_port") && webServer.hasArg("subnet") && webServer.hasArg("wifi_ssid") && webServer.hasArg("wifi_pass") && webServer.hasArg("wifi_host") && webServer.hasArg("usbwait")) 
  {
    AP_SSID = webServer.arg("ap_ssid");
    if (!webServer.arg("ap_pass").equals("********"))
    {
      AP_PASS = webServer.arg("ap_pass");
    }
    WIFI_SSID = webServer.arg("wifi_ssid");
    if (!webServer.arg("wifi_pass").equals("********"))
    {
      WIFI_PASS = webServer.arg("wifi_pass");
    }
    String tmpip = webServer.arg("web_ip");
    String tmpwport = webServer.arg("web_port");
    String tmpsubn = webServer.arg("subnet");
    String WIFI_HOSTNAME = webServer.arg("wifi_host");
    String tmpua = "false";
    String tmpcw = "false";
    if (webServer.hasArg("useap")){tmpua = "true";}
    if (webServer.hasArg("usewifi")){tmpcw = "true";}
    int USB_WAIT = webServer.arg("usbwait").toInt();
    File iniFile = FILESYS.open("/config.ini", "w");
    if (iniFile) {
    iniFile.print("\r\nAP_SSID=" + AP_SSID + "\r\nAP_PASS=" + AP_PASS + "\r\nWEBSERVER_IP=" + tmpip + "\r\nWEBSERVER_PORT=" + tmpwport + "\r\nSUBNET_MASK=" + tmpsubn + "\r\nWIFI_SSID=" + WIFI_SSID + "\r\nWIFI_PASS=" + WIFI_PASS + "\r\nWIFI_HOST=" + WIFI_HOSTNAME + "\r\nUSEAP=" + tmpua + "\r\nCONWIFI=" + tmpcw + "\r\nUSBWAIT=" + String(USB_WAIT) + "\r\n");
    iniFile.close();
    }
    String htmStr = "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"8; url=/info.html\"><style type=\"text/css\">#loader {  z-index: 1;   width: 50px;   height: 50px;   margin: 0 0 0 0;   border: 6px solid #f3f3f3;   border-radius: 50%;   border-top: 6px solid #3498db;   width: 50px;   height: 50px;   -webkit-animation: spin 2s linear infinite;   animation: spin 2s linear infinite; } @-webkit-keyframes spin {  0%  {  -webkit-transform: rotate(0deg);  }  100% {  -webkit-transform: rotate(360deg); }}@keyframes spin {  0% { transform: rotate(0deg); }  100% { transform: rotate(360deg); }} body { background-color: #1451AE; color: #ffffff; font-size: 20px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}   #msgfmt { font-size: 16px; font-weight: normal;}#status { font-size: 16px;  font-weight: normal;}</style></head><center><br><br><br><br><br><p id=\"status\"><div id='loader'></div><br>Config saved<br>Rebooting</p></center></html>";
    webServer.setContentLength(htmStr.length());
    webServer.send(200, "text/html", htmStr);
    delay(1000);
    ESP.restart();
  }
  else
  {
   webServer.sendHeader("Location","/config.html");
   webServer.send(302, "text/html", "");
  }
}



void handleReboot()
{
  //Serial.print("Rebooting ESP");
  String htmStr = "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"8; url=/info.html\"><style type=\"text/css\">#loader {  z-index: 1;   width: 50px;   height: 50px;   margin: 0 0 0 0;   border: 6px solid #f3f3f3;   border-radius: 50%;   border-top: 6px solid #3498db;   width: 50px;   height: 50px;   -webkit-animation: spin 2s linear infinite;   animation: spin 2s linear infinite; } @-webkit-keyframes spin {  0%  {  -webkit-transform: rotate(0deg);  }  100% {  -webkit-transform: rotate(360deg); }}@keyframes spin {  0% { transform: rotate(0deg); }  100% { transform: rotate(360deg); }} body { background-color: #1451AE; color: #ffffff; font-size: 20px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}   #msgfmt { font-size: 16px; font-weight: normal;}#status { font-size: 16px;  font-weight: normal;}</style></head><center><br><br><br><br><br><p id=\"status\"><div id='loader'></div><br>Rebooting</p></center></html>";
  webServer.setContentLength(htmStr.length());
  webServer.send(200, "text/html", htmStr);
  delay(1000);
  ESP.restart();
}


void handleConfigHtml()
{
  String tmpUa = "";
  String tmpCw = "";
  if (startAP){tmpUa = "checked";}
  if (connectWifi){tmpCw = "checked";}
  String htmStr = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Config Editor</title><style type=\"text/css\">body {    background-color: #1451AE; color: #ffffff; font-size: 14px;  font-weight: bold;    margin: 0 0 0 0.0;    padding: 0.4em 0.4em 0.4em 0.6em;}  input[type=\"submit\"]:hover {     background: #ffffff;    color: green; }input[type=\"submit\"]:active {     outline-color: green;    color: green;    background: #ffffff; }table {    font-family: arial, sans-serif;     border-collapse: collapse;}td {border: 1px solid #dddddd;     text-align: left;    padding: 8px;}  th {border: 1px solid #dddddd; background-color:gray;    text-align: center;    padding: 8px;}</style></head><body><form action=\"/config.html\" method=\"post\"><center><table><tr><th colspan=\"2\"><center>Access Point</center></th></tr><tr><td>AP SSID:</td><td><input name=\"ap_ssid\" value=\"" + AP_SSID + "\"></td></tr><tr><td>AP PASSWORD:</td><td><input name=\"ap_pass\" value=\"********\"></td></tr><tr><td>AP IP:</td><td><input name=\"web_ip\" value=\"" + Server_IP.toString() + "\"></td></tr><tr><td>SUBNET MASK:</td><td><input name=\"subnet\" value=\"" + Subnet_Mask.toString() + "\"></td></tr><tr><td>START AP:</td><td><input type=\"checkbox\" name=\"useap\" " + tmpUa +"></td></tr><tr><th colspan=\"2\"><center>Web Server</center></th></tr><tr><td>WEBSERVER PORT:</td><td><input name=\"web_port\" value=\"" + String(WEB_PORT) + "\"></td></tr><tr><th colspan=\"2\"><center>Wifi Connection</center></th></tr><tr><td>WIFI SSID:</td><td><input name=\"wifi_ssid\" value=\"" + WIFI_SSID + "\"></td></tr><tr><td>WIFI PASSWORD:</td><td><input name=\"wifi_pass\" value=\"********\"></td></tr><tr><td>WIFI HOSTNAME:</td><td><input name=\"wifi_host\" value=\"" + WIFI_HOSTNAME + "\"></td></tr><tr><td>CONNECT WIFI:</td><td><input type=\"checkbox\" name=\"usewifi\" " + tmpCw + "></tr><tr></table><br><input id=\"savecfg\" type=\"submit\" value=\"Save Config\"></center></form></body></html>";
  webServer.setContentLength(htmStr.length());
  webServer.send(200, "text/html", htmStr);
}


void handleUpdateHtml()
{
  String htmStr = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Firmware Update</title><style type=\"text/css\">#loader {  z-index: 1;  width: 50px;  height: 50px;  margin: 0 0 0 0;  border: 6px solid #f3f3f3;  border-radius: 50%;  border-top: 6px solid #3498db;  width: 50px;  height: 50px;  -webkit-animation: spin 2s linear infinite;  animation: spin 2s linear infinite;}@-webkit-keyframes spin {  0% { -webkit-transform: rotate(0deg); }  100% { -webkit-transform: rotate(360deg); }}@keyframes spin {  0% { transform: rotate(0deg); }  100% { transform: rotate(360deg); }}body {    background-color: #1451AE; color: #ffffff; font-size: 20px;  font-weight: bold;    margin: 0 0 0 0.0;    padding: 0.4em 0.4em 0.4em 0.6em;}  input[type=\"submit\"]:hover {     background: #ffffff;    color: green; }input[type=\"submit\"]:active {     outline-color: green;    color: green;    background: #ffffff; }input[type=\"button\"]:hover {     background: #ffffff;    color: #000000; }input[type=\"button\"]:active {     outline-color: #000000;    color: #000000;    background: #ffffff; }#selfile {  font-size: 16px;  font-weight: normal;}#status {  font-size: 16px;  font-weight: normal;}</style><script>function formatBytes(bytes) {  if(bytes == 0) return '0 Bytes';  var k = 1024,  dm = 2,  sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'],  i = Math.floor(Math.log(bytes) / Math.log(k));  return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];}function statusUpl() {  document.getElementById(\"upload\").style.display=\"none\";  document.getElementById(\"btnsel\").style.display=\"none\";  document.getElementById(\"status\").innerHTML = \"<div id='loader'></div><br>Uploading firmware file...\";  setTimeout(statusUpd, 5000);}function statusUpd() {  document.getElementById(\"status\").innerHTML = \"<div id='loader'></div><br>Updating firmware, Please wait.\";}function FileSelected(e){  file = document.getElementById('fwfile').files[document.getElementById('fwfile').files.length - 1];  if (file.name.toLowerCase() == \"fwupdate.bin\")  {  var b = file.slice(0, 1);  var r = new FileReader();  r.onloadend = function(e) {  if (e.target.readyState === FileReader.DONE) {  var mb = new Uint8Array(e.target.result);   if (parseInt(mb[0]) == 233)  {  document.getElementById(\"selfile\").innerHTML =  \"File: \" + file.name + \"<br>Size: \" + formatBytes(file.size) + \"<br>Magic byte: 0x\" + parseInt(mb[0]).toString(16).toUpperCase();   document.getElementById(\"upload\").style.display=\"block\"; } else  {  document.getElementById(\"selfile\").innerHTML =  \"<font color='#df3840'><b>Invalid firmware file</b></font><br><br>Magic byte is wrong<br>Expected: 0xE9<br>Found: 0x\" + parseInt(mb[0]).toString(16).toUpperCase();     document.getElementById(\"upload\").style.display=\"none\";  }    }    };    r.readAsArrayBuffer(b);  }  else  {    document.getElementById(\"selfile\").innerHTML =  \"<font color='#df3840'><b>Invalid firmware file</b></font><br><br>File should be fwupdate.bin\";    document.getElementById(\"upload\").style.display=\"none\";  }}</script></head><body><center><form action=\"/update.html\" enctype=\"multipart/form-data\" method=\"post\"><p>Firmware Updater<br><br></p><p><input id=\"btnsel\" type=\"button\" onclick=\"document.getElementById('fwfile').click()\" value=\"Select file\" style=\"display: block;\"><p id=\"selfile\"></p><input id=\"fwfile\" type=\"file\" name=\"fwupdate\" size=\"0\" accept=\".bin\" onChange=\"FileSelected();\" style=\"width:0; height:0;\"></p><div><p id=\"status\"></p><input id=\"upload\" type=\"submit\" value=\"Update Firmware\" onClick=\"statusUpl();\" style=\"display: none;\"></div></form><center></body></html>";
  webServer.setContentLength(htmStr.length());
  webServer.send(200, "text/html", htmStr);
}


void handleUploadHtml()
{
  String htmStr = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>File Upload</title><style type=\"text/css\">#loader {  z-index: 1;  width: 50px;  height: 50px;  margin: 0 0 0 0;  border: 6px solid #f3f3f3;  border-radius: 50%;  border-top: 6px solid #3498db;  width: 50px;  height: 50px;  -webkit-animation: spin 2s linear infinite;  animation: spin 2s linear infinite;}@-webkit-keyframes spin {  0% { -webkit-transform: rotate(0deg); }  100% { -webkit-transform: rotate(360deg); }}@keyframes spin {  0% { transform: rotate(0deg); }  100% { transform: rotate(360deg); }}body {  overflow-y:auto;  background-color: #1451AE; color: #ffffff; font-size: 20px;  font-weight: bold;    margin: 0 0 0 0.0;    padding: 0.4em 0.4em 0.4em 0.6em;}  input[type=\"submit\"]:hover {     background: #ffffff;    color: green; }input[type=\"submit\"]:active {     outline-color: green;    color: green;    background: #ffffff;  } input[type=\"button\"]:hover {     background: #ffffff;    color: #000000; }input[type=\"button\"]:active {     outline-color: #000000;    color: #000000;    background: #ffffff; }#selfile {  font-size: 16px;  font-weight: normal;}#status {  font-size: 16px;  font-weight: normal;}</style><script>function formatBytes(bytes) {  if(bytes == 0) return '0 Bytes';  var k = 1024,  dm = 2,  sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'],  i = Math.floor(Math.log(bytes) / Math.log(k));  return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];}function statusUpl() {  document.getElementById(\"upload\").style.display=\"none\";  document.getElementById(\"btnsel\").style.display=\"none\";  document.getElementById(\"status\").innerHTML = \"<div id='loader'></div><br>Uploading files\";}function FileSelected(e){  var strdisp = \"\";  var file = document.getElementById(\"upfiles\").files;  for (var i = 0; i < file.length; i++)  {   strdisp = strdisp + file[i].name + \" - \" + formatBytes(file[i].size) + \"<br>\";  }  document.getElementById(\"selfile\").innerHTML = strdisp;  document.getElementById(\"upload\").style.display=\"block\";}</script></head><body><center><form action=\"/upload.html\" enctype=\"multipart/form-data\" method=\"post\"><p>File Uploader<br><br></p><p><input id=\"btnsel\" type=\"button\" onclick=\"document.getElementById('upfiles').click()\" value=\"Select files\" style=\"display: block;\"><p id=\"selfile\"></p><input id=\"upfiles\" type=\"file\" name=\"fwupdate\" size=\"0\" onChange=\"FileSelected();\" style=\"width:0; height:0;\" multiple></p><div><p id=\"status\"></p><input id=\"upload\" type=\"submit\" value=\"Upload Files\" onClick=\"statusUpl();\" style=\"display: none;\"></div></form><center></body></html>";
  webServer.setContentLength(htmStr.length());
  webServer.send(200, "text/html", htmStr);
}


void handleFormatHtml()
{
  String htmStr = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Storage Format</title><style type=\"text/css\">#loader {  z-index: 1;  width: 50px;  height: 50px;  margin: 0 0 0 0;  border: 6px solid #f3f3f3;  border-radius: 50%;  border-top: 6px solid #3498db;  width: 50px;  height: 50px;  -webkit-animation: spin 2s linear infinite;  animation: spin 2s linear infinite;}@-webkit-keyframes spin {  0% { -webkit-transform: rotate(0deg); }  100% { -webkit-transform: rotate(360deg); }}@keyframes spin {  0% { transform: rotate(0deg); }  100% { transform: rotate(360deg); }}body {    background-color: #1451AE; color: #ffffff; font-size: 20px;  font-weight: bold;    margin: 0 0 0 0.0;    padding: 0.4em 0.4em 0.4em 0.6em;}  input[type=\"submit\"]:hover {     background: #ffffff;    color: green; }input[type=\"submit\"]:active {     outline-color: green;    color: green;    background: #ffffff; }#msgfmt { font-size: 16px;  font-weight: normal;}#status {  font-size: 16px;  font-weight: normal;}</style><script>function statusFmt() { var answer = confirm(\"Are you sure you want to format?\");  if (answer) {   document.getElementById(\"format\").style.display=\"none\";   document.getElementById(\"status\").innerHTML = \"<div id='loader'></div><br>Formatting Storage\";   return true;  }  else {   return false;  }}</script></head><body><center><form action=\"/format.html\" method=\"post\"><p>Storage Format<br><br></p><p id=\"msgfmt\">This will delete all the files on the server</p><div><p id=\"status\"></p><input id=\"format\" type=\"submit\" value=\"Format Storage\" onClick=\"return statusFmt();\" style=\"display: block;\"></div></form><center></body></html>";
  webServer.setContentLength(htmStr.length());
  webServer.send(200, "text/html", htmStr);
}


void handleAdminHtml()
{
  String htmStr = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Admin Panel</title><style>body {    background-color: #1451AE; color: #ffffff; font-size: 14px;  font-weight: bold;    margin: 0 0 0 0.0;    padding: 0.4em 0.4em 0.4em 0.6em;}.sidenav {    width: 140px;    position: fixed;    z-index: 1;    top: 20px;    left: 10px;    background: #6495ED;    overflow-x: hidden;    padding: 8px 0;}.sidenav a {    padding: 6px 8px 6px 16px;    text-decoration: none;    font-size: 14px;    color: #ffffff;    display: block;}.sidenav a:hover {    color: #1451AE;}.main {    margin-left: 150px;     padding: 10px 10px; position: absolute;   top: 0;   right: 0; bottom: 0;  left: 0;}</style></head><body><div class=\"sidenav\"><a href=\"/index.html\" target=\"mframe\">Main Page</a><a href=\"/info.html\" target=\"mframe\">ESP Information</a><a href=\"/fileman.html\" target=\"mframe\">File Manager</a><a href=\"/upload.html\" target=\"mframe\">File Uploader</a><a href=\"/update.html\" target=\"mframe\">Firmware Update</a><a href=\"/config.html\" target=\"mframe\">Config Editor</a><a href=\"/format.html\" target=\"mframe\">Storage Format</a><a href=\"/reboot.html\" target=\"mframe\">Reboot ESP</a></div><div class=\"main\"><iframe src=\"info.html\" name=\"mframe\" height=\"100%\" width=\"100%\" frameborder=\"0\"></iframe></div></table></body></html> ";
  webServer.setContentLength(htmStr.length());
  webServer.send(200, "text/html", htmStr);
}


void handleConsoleUpdate(String rgn)
{
  String Version = "05.050.000";
  String sVersion = "05.050.000";
  String lblVersion = "5.05";
  String imgSize = "0";
  String imgPath = "";
  String xmlStr = "<?xml version=\"1.0\" ?><update_data_list><region id=\"" + rgn + "\"><force_update><system level0_system_ex_version=\"0\" level0_system_version=\"" + Version + "\" level1_system_ex_version=\"0\" level1_system_version=\"" + Version + "\"/></force_update><system_pup ex_version=\"0\" label=\"" + lblVersion + "\" sdk_version=\"" + sVersion + "\" version=\"" + Version + "\"><update_data update_type=\"full\"><image size=\"" + imgSize + "\">" + imgPath + "</image></update_data></system_pup><recovery_pup type=\"default\"><system_pup ex_version=\"0\" label=\"" + lblVersion + "\" sdk_version=\"" + sVersion + "\" version=\"" + Version + "\"/><image size=\"" + imgSize + "\">" + imgPath + "</image></recovery_pup></region></update_data_list>";
  webServer.setContentLength(xmlStr.length());
  webServer.send(200, "text/xml", xmlStr);
}


void handleRebootHtml()
{
  String htmStr = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>ESP Reboot</title><style type=\"text/css\">#loader {  z-index: 1;   width: 50px;   height: 50px;   margin: 0 0 0 0;   border: 6px solid #f3f3f3;   border-radius: 50%;   border-top: 6px solid #3498db;   width: 50px;   height: 50px;   -webkit-animation: spin 2s linear infinite;   animation: spin 2s linear infinite; } @-webkit-keyframes spin {  0%  {  -webkit-transform: rotate(0deg);  }  100% {  -webkit-transform: rotate(360deg); }}@keyframes spin {  0% { transform: rotate(0deg); }  100% { transform: rotate(360deg); }} body { background-color: #1451AE; color: #ffffff; font-size: 20px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}   input[type=\"submit\"]:hover { background: #ffffff; color: green; }input[type=\"submit\"]:active { outline-color: green; color: green; background: #ffffff; } #msgfmt { font-size: 16px; font-weight: normal;}#status { font-size: 16px;  font-weight: normal;} </style><script>function statusRbt() { var answer = confirm(\"Are you sure you want to reboot?\");  if (answer) {document.getElementById(\"reboot\").style.display=\"none\";   document.getElementById(\"status\").innerHTML = \"<div id='loader'></div><br>Rebooting ESP Board\"; return true;  }else {   return false;  }}</script></head><body><center><form action=\"/reboot.html\" method=\"post\"><p>ESP Reboot<br><br></p><p id=\"msgrbt\">This will reboot the esp board</p><div><p id=\"status\"></p><input id=\"reboot\" type=\"submit\" value=\"Reboot ESP\" onClick=\"return statusRbt();\" style=\"display: block;\"></div></form><center></body></html>";
  webServer.setContentLength(htmStr.length());
  webServer.send(200, "text/html", htmStr);
}


void handlePayloads()
{
  String output = "const payload_map =\r\n[";
  output += "{\r\n";
  output += "displayTitle: 'etaHEN',\r\n"; //internal etahen bin
  output += "description: 'Runs With 3.xx and 4.xx. FPKG enabler For FW 3.xx / 4.03-4.51 Only.',\r\n";  
  output += "fileName: 'etahen.bin',\r\n";
  output += "author: 'LightningMods_, sleirsgoevy, ChendoChap, astrelsky, illusion',\r\n";
  output += "source: 'https://github.com/LightningMods/etaHEN',\r\n";
  output += "version: 'v1.6 beta'\r\n}\r\n";
  
  Dir dir = FILESYS.openDir("/");
  while(dir.next())
  {
    File file = dir.openFile("r");
    String fname = String(file.name());
    fname.replace("/", "");
    if (fname.endsWith(".gz"))
    {
      fname = fname.substring(0, fname.length() - 3);
    }
    if (fname.length() > 0 && !file.isDirectory() && fname.endsWith(".bin") || fname.endsWith(".elf"))
    {
      String fnamev = fname;
      fnamev.replace(".bin", "");
      fnamev.replace(".elf", "");
      output += ",{\r\n";
      output += "displayTitle: '" + fnamev + "',\r\n";
      output += "description: '" + fname + "',\r\n";  
      output += "fileName: '" + fname + "',\r\n";
      output += "author: '',\r\n";
      output += "source: '',\r\n";
      output += "version: ''\r\n}\r\n";
    }
    file.close();
  }
  output += "\r\n];";
  webServer.setContentLength(output.length());
  webServer.send(200, "application/javascript", output);
}


void handleCacheManifest() {
  String output = "CACHE MANIFEST\r\n";
  Dir dir = FILESYS.openDir("/");
  while(dir.next()){
    File entry = dir.openFile("r");
    String fname = String(entry.name()).substring(1);
    if (fname.length() > 0 && !fname.equals("config.ini"))
    {
      if (fname.endsWith(".gz")) {
        fname = fname.substring(0, fname.length() - 3);
      }
     output += urlencode(fname) + "\r\n";
    }
    entry.close();
  }
  if(!instr(output,"index.html\r\n"))
  {
    output += "index.html\r\n";
  }
  webServer.setContentLength(output.length());
  webServer.send(200, "text/cache-manifest", output);
}


void handleInfo()
{
  FSInfo fs_info;
  FILESYS.info(fs_info);
  float flashFreq = (float)ESP.getFlashChipSpeed() / 1000.0 / 1000.0;
  FlashMode_t ideMode = ESP.getFlashChipMode();
  float supplyVoltage = (float)ESP.getVcc()/ 1000.0 ;
  String output = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>System Information</title><style type=\"text/css\">body { background-color: #1451AE;color: #ffffff;font-size: 14px;font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}</style></head>";
  output += "<hr>###### Software ######<br><br>";
  output += "Firmware version " + firmwareVer + "<br>";
  output += "Core version: " + String(ESP.getCoreVersion()) + "<br>";
  output += "SDK version: " + String(ESP.getSdkVersion()) + "<br>";
  output += "Boot version: " + String(ESP.getBootVersion()) + "<br>";
  output += "Boot mode: " + String(ESP.getBootMode()) + "<br>";
  output += "Chip Id: " + String(ESP.getChipId()) + "<br><hr>";
  output += "###### CPU ######<br><br>";
  output += "CPU frequency: " + String(ESP.getCpuFreqMHz()) + "MHz<br><hr>";
  output += "###### Flash chip information ######<br><br>";
  output += "Flash chip Id: " +  String(ESP.getFlashChipId()) + "<br>";
  output += "Estimated Flash size: " + formatBytes(ESP.getFlashChipSize()) + "<br>";
  output += "Actual Flash size based on chip Id: " + formatBytes(ESP.getFlashChipRealSize()) + "<br>";
  output += "Flash frequency: " + String(flashFreq) + " MHz<br>";
  output += "Flash write mode: " + String((ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN")) + "<br><hr>";
#if USELFS
  output += "###### File system (LittleFS) ######<br><br>";
#else
  output += "###### File system (SPIFFS) ######<br><br>";
#endif
  output += "Total space: " + formatBytes(fs_info.totalBytes) + "<br>";
  output += "Used space: " + formatBytes(fs_info.usedBytes) + "<br>";
  output += "Block size: " + String(fs_info.blockSize) + "<br>";
  output += "Page size: " + String(fs_info.pageSize) + "<br>";
  output += "Maximum open files: " + String(fs_info.maxOpenFiles) + "<br>";
  output += "Maximum path length: " +  String(fs_info.maxPathLength) + "<br><hr>";
  output += "###### Sketch information ######<br><br>";
  output += "Sketch hash: " + ESP.getSketchMD5() + "<br>";
  output += "Sketch size: " +  formatBytes(ESP.getSketchSize()) + "<br>";
  output += "Free space available: " +  formatBytes(ESP.getFreeSketchSpace()) + "<br><hr>";
  output += "###### power ######<br><br>";
  output += "Supply voltage: " +  String(supplyVoltage) + " V<br><hr>";
  output += "</html>";
  webServer.setContentLength(output.length());
  webServer.send(200, "text/html", output);
}


void writeConfig()
{
  File iniFile = FILESYS.open("/config.ini", "w");
  if (iniFile) {
  String tmpua = "false";
  String tmpcw = "false";
  if (startAP){tmpua = "true";}
  if (connectWifi){tmpcw = "true";}
  iniFile.print("\r\nAP_SSID=" + AP_SSID + "\r\nAP_PASS=" + AP_PASS + "\r\nWEBSERVER_IP=" + Server_IP.toString() + "\r\nWEBSERVER_PORT=" + String(WEB_PORT) + "\r\nSUBNET_MASK=" + Subnet_Mask.toString() + "\r\nWIFI_SSID=" + WIFI_SSID + "\r\nWIFI_PASS=" + WIFI_PASS + "\r\nWIFI_HOST=" + WIFI_HOSTNAME + "\r\nUSEAP=" + tmpua + "\r\nCONWIFI=" + tmpcw + "\r\n");
  iniFile.close();
  }
}



void setup(void) 
{

  //Serial.begin(115200);
  //Serial.setDebugOutput(true);
  //Serial.println("Version: " + firmwareVer);
  if (FILESYS.begin()) {
  if (FILESYS.exists("/config.ini")) {
  File iniFile = FILESYS.open("/config.ini", "r");
  if (iniFile) {
  String iniData;
    while (iniFile.available()) {
      char chnk = iniFile.read();
      iniData += chnk;
    }
   iniFile.close();
   
   if(instr(iniData,"AP_SSID="))
   {
   AP_SSID = split(iniData,"AP_SSID=","\r\n");
   AP_SSID.trim();
   }
   
   if(instr(iniData,"AP_PASS="))
   {
   AP_PASS = split(iniData,"AP_PASS=","\r\n");
   AP_PASS.trim();
   }
   
   if(instr(iniData,"WEBSERVER_PORT="))
   {
   String strWprt = split(iniData,"WEBSERVER_PORT=","\r\n");
   strWprt.trim();
   WEB_PORT = strWprt.toInt();
   }
   
   if(instr(iniData,"WEBSERVER_IP="))
   {
    String strwIp = split(iniData,"WEBSERVER_IP=","\r\n");
    strwIp.trim();
    Server_IP.fromString(strwIp);
   }

   if(instr(iniData,"SUBNET_MASK="))
   {
    String strsIp = split(iniData,"SUBNET_MASK=","\r\n");
    strsIp.trim();
    Subnet_Mask.fromString(strsIp);
   }

   if(instr(iniData,"WIFI_SSID="))
   {
   WIFI_SSID = split(iniData,"WIFI_SSID=","\r\n");
   WIFI_SSID.trim();
   }
   
   if(instr(iniData,"WIFI_PASS="))
   {
   WIFI_PASS = split(iniData,"WIFI_PASS=","\r\n");
   WIFI_PASS.trim();
   }
   
   if(instr(iniData,"WIFI_HOST="))
   {
   WIFI_HOSTNAME = split(iniData,"WIFI_HOST=","\r\n");
   WIFI_HOSTNAME.trim();
   }
   
   if(instr(iniData,"USEAP="))
   {
    String strua = split(iniData,"USEAP=","\r\n");
    strua.trim();
    if (strua.equals("true"))
    {
      startAP = true;
    }
    else
    {
      startAP = false;
    }
   }

   if(instr(iniData,"CONWIFI="))
   {
    String strcw = split(iniData,"CONWIFI=","\r\n");
    strcw.trim();
    if (strcw.equals("true"))
    {
      connectWifi = true;
    }
    else
    {
      connectWifi = false;
    }
   }

    }
  }
  else
  {
   writeConfig(); 
  }
  }
  else
  {
    //Serial.println("No Filesystem");
  }


  if (startAP)
  {
    //Serial.println("SSID: " + AP_SSID);
    //Serial.println("Password: " + AP_PASS);
    //Serial.println("");
    //Serial.println("WEB Server IP: " + Server_IP.toString());
    //Serial.println("Subnet: " + Subnet_Mask.toString());
    //Serial.println("WEB Server Port: " + String(WEB_PORT));
    //Serial.println("");
    WiFi.softAPConfig(Server_IP, Server_IP, Subnet_Mask);
    WiFi.softAP(AP_SSID, AP_PASS);
    //Serial.println("WIFI AP started");
    dnsServer.setTTL(30);
    dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
    dnsServer.start(53, "*", Server_IP);
    //Serial.println("DNS server started");
    //Serial.println("DNS Server IP: " + Server_IP.toString());
  }


  if (connectWifi && WIFI_SSID.length() > 0 && WIFI_PASS.length() > 0)
  {
    WiFi.setAutoConnect(true); 
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    WiFi.hostname(WIFI_HOSTNAME);
    //Serial.println("WIFI connecting");
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      //Serial.println("Wifi failed to connect");
    } else {
      IPAddress LAN_IP = WiFi.localIP(); 
      if (LAN_IP)
      {
        //Serial.println("Wifi Connected");
        //Serial.println("WEB Server LAN IP: " + LAN_IP.toString());
        //Serial.println("WEB Server Port: " + String(WEB_PORT));
        //Serial.println("WEB Server Hostname: " + WIFI_HOSTNAME);
        String mdnsHost = WIFI_HOSTNAME;
        mdnsHost.replace(".local","");
        MDNS.begin(mdnsHost, LAN_IP);
        if (!startAP)
        {
          dnsServer.setTTL(30);
          dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
          dnsServer.start(53, "*", LAN_IP);
          //Serial.println("DNS server started");
          //Serial.println("DNS Server IP: " + LAN_IP.toString());
        }
      }
    }
  }


  webServer.onNotFound(handleNotFound);
  /*
  webServer.on("/update.html", HTTP_POST, []() {} ,handleFwUpdate);
  webServer.on("/update.html", HTTP_GET, handleUpdateHtml);
  webServer.on("/upload.html", HTTP_POST, []() {
  webServer.sendHeader("Location","/fileman.html");
  webServer.send(302, "text/html", "");
  }, handleFileUpload);
  webServer.on("/upload.html", HTTP_GET, handleUploadHtml);
  webServer.on("/format.html", HTTP_GET, handleFormatHtml);
  webServer.on("/format.html", HTTP_POST, handleFormat);
  webServer.on("/fileman.html", HTTP_GET, handleFileMan);
  webServer.on("/info.html", HTTP_GET, handleInfo);
  webServer.on("/delete", HTTP_POST, handleDelete);
  webServer.on("/config.html", HTTP_GET, handleConfigHtml);
  webServer.on("/config.html", HTTP_POST, handleConfig);
  webServer.on("/admin.html", HTTP_GET, handleAdminHtml);
  webServer.on("/reboot.html", HTTP_GET, handleRebootHtml);
  webServer.on("/reboot.html", HTTP_POST, handleReboot);
  */
  webServer.begin(WEB_PORT);


  sWebServer.getServer().setRSACert(new X509List(serverCert), new PrivateKey(serverKey));
  sWebServer.onNotFound([]() {
  sWebServer.sendHeader("Location", String("http://" + Server_IP.toString() + "/index.html" ), true);
  sWebServer.send(301, "text/plain", "");
  });
  sWebServer.begin();

  
  //Serial.println("HTTP server started");
}


void loop(void) {
  dnsServer.processNextRequest();
  webServer.handleClient();
  sWebServer.handleClient();
}
