#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include "LittleFS.h"
#include "esp_wifi.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"

static mbedtls_net_context tlsListen;
static mbedtls_ssl_config tlsConf;
static mbedtls_x509_crt tlsCert;
static mbedtls_pk_context tlsKey;
static mbedtls_entropy_context tlsEnt;
static mbedtls_ctr_drbg_context tlsRng;

DNSServer dnsServer;
WebServer server(80);
typedef File File32;

#define firmwareVer "0.6"

//-------------------DEFAULT SETTINGS------------------//

// create access point
boolean startAP = true;
String AP_SSID = "PS5_RELAPSE_HOST";
String AP_PASS = "12345678";
IPAddress Server_IP(10, 1, 1, 1);
IPAddress Subnet_Mask(255, 255, 255, 0);
String WIFI_HOSTNAME = "ps5.local";

static uint8_t fileSendBuf[8192];
//-----------------------------------------------------//

bool instr(String str, String search) {
  int result = str.indexOf(search);
  if (result == -1) {
    return false;
  }
  return true;
}

String getMimeType(String filename) {
  if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".mjs")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  else if (filename.endsWith(".bin")) return "application/octet-stream";
  else if (filename.endsWith(".elf")) return "application/octet-stream";
  else if (filename.endsWith(".manifest")) return "text/cache-manifest";
  else if (filename.endsWith(".appcache")) return "text/cache-manifest";
  return "text/plain";
}

bool loadFromFlash(String path) {
  // Serial.println(path);
  if (path.endsWith("config.ini") || path.endsWith("pk.pem") || path.endsWith("cert.der")) return false;
  if (instr(path, "/update/") && instr(path, "/ps5/")) {
    server.send(
      200,
      "application/xml",
      "<?xml version=\"1.0\" ?><update_data_list><region id=\"us\"><force_update><system auto_update_version=\"00.00\" sdk_version=\"01.00.00.09-00.00.00.0.0\" upd_version=\"01.00.00.00\"/></force_update><system_pup auto_update_version=\"00.00\" label=\"20.02.02.20.00.07-00.00.00.0.0\" sdk_version=\"02.20.00.07-00.00.00.0.0\" upd_version=\"02.20.00.00\"><update_data update_type=\"full\"></update_data></system_pup></region></update_data_list>");
    return true;
  }

  if (path.endsWith("connecttest.txt")) {
    server.send(200, "text/plain", "Microsoft Connect Test");
    return true;
  }
  if (path.endsWith("/")) path += "index.html";

  String mimeType = getMimeType(path);
  File flashFile;
  flashFile = LittleFS.open(path + ".gz", "r");

  if (!flashFile) return false;

  if (path.endsWith(".elf")) {
    server.sendHeader("Cache-Control", "public, max-age=31536000, immutable");
  }
  else if (path.endsWith(".html") || path.endsWith("/")) {
    server.sendHeader("Cache-Control", "no-cache");
  }
  else {
    server.sendHeader("Cache-Control", "public, max-age=86400");
  }

  if (mimeType == "application/octet-stream") server.sendHeader("Content-Encoding", "gzip");
  if (server.streamFile(flashFile, mimeType) != flashFile.size()) {
    Serial.println("Sent less data than expected!");
  }

  flashFile.close();
  return true;
}

void handleNotFound() {
  if (loadFromFlash(server.uri())) {
    return;
  }
  if (server.uri().endsWith("/") || server.uri().endsWith("index.html") || server.uri().endsWith("index.htm")) {

    server.send(
      200,
      "text/html",
      "<!DOCTYPE html><html><head><style>body{background-color: #2F3335;color: #ffffff;font-size: 18px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}</style></head><center><br><br><br><br><br><br>index.html not found<br>connect the esp to a pc and transfer the files to the storage</center></html>");
    return;
  }

  Serial.println("Not Found: " + server.uri());
  server.send(404, "text/plain", "Not Found");
}

void loadAP() {
  WiFi.softAPConfig(Server_IP, Server_IP, Subnet_Mask);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  WiFi.setSleep(false);
  dnsServer.setTTL(30);
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  dnsServer.start(53, "*", Server_IP);
}

static bool initTLS() {
  mbedtls_net_init(&tlsListen);
  mbedtls_ssl_config_init(&tlsConf);
  mbedtls_x509_crt_init(&tlsCert);
  mbedtls_pk_init(&tlsKey);
  mbedtls_entropy_init(&tlsEnt);
  mbedtls_ctr_drbg_init(&tlsRng);

  if (mbedtls_ctr_drbg_seed(&tlsRng, mbedtls_entropy_func, &tlsEnt,
    (uint8_t*) "ps5srv", 6)
    != 0) return false;
  {
    File f = LittleFS.open("/cert.der", "r");
    size_t len = f.size();
    uint8_t* buf = (uint8_t*) malloc(len);
    f.read(buf, len);
    f.close();
    int r = mbedtls_x509_crt_parse_der(&tlsCert, buf, len);
    free(buf);
    if (r != 0) return false;
  }
  {
    File f = LittleFS.open("/pk.pem", "r");
    size_t len = f.size();
    uint8_t* buf = (uint8_t*) malloc(len + 1);
    f.read(buf, len);
    f.close();
    buf[len] = 0;
  #if MBEDTLS_VERSION_MAJOR >= 3
    int r = mbedtls_pk_parse_key(&tlsKey, buf, len + 1, NULL, 0, mbedtls_ctr_drbg_random, &tlsRng);
  #else
    int r = mbedtls_pk_parse_key(&tlsKey, buf, len + 1, NULL, 0);
  #endif
    free(buf);
    if (r != 0) return false;
  }
  if (mbedtls_ssl_config_defaults(&tlsConf, MBEDTLS_SSL_IS_SERVER,
    MBEDTLS_SSL_TRANSPORT_STREAM,
    MBEDTLS_SSL_PRESET_DEFAULT)
    != 0) return false;
  mbedtls_ssl_conf_rng(&tlsConf, mbedtls_ctr_drbg_random, &tlsRng);
  mbedtls_ssl_conf_own_cert(&tlsConf, &tlsCert, &tlsKey);
  if (mbedtls_net_bind(&tlsListen, NULL, "443", MBEDTLS_NET_PROTO_TCP) != 0) return false;
  mbedtls_net_set_nonblock(&tlsListen);
  return true;
}

static void tickTLS() {
  mbedtls_net_context client;
  mbedtls_ssl_context ssl;
  mbedtls_net_init(&client);
  mbedtls_ssl_init(&ssl);

  if (mbedtls_net_accept(&tlsListen, &client, NULL, 0, NULL) != 0)
    goto cleanup;

  if (mbedtls_ssl_setup(&ssl, &tlsConf) != 0) goto cleanup;
  mbedtls_ssl_set_bio(&ssl, &client, mbedtls_net_send, mbedtls_net_recv, NULL);

  {  // handshake
    int ret;
    unsigned long t = millis();
    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
      if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) goto cleanup;
      if (millis() - t > 5000) goto cleanup;
      yield();
    }
  }
  {  // read until end of headers, parse path
    char buf[512] = {};
    int total = 0, ret;
    unsigned long t = millis();
    while (total < (int) sizeof(buf) - 1 && millis() - t < 2000) {
      ret = mbedtls_ssl_read(&ssl, (uint8_t*) buf + total, 1);
      if (ret == MBEDTLS_ERR_SSL_WANT_READ) {
        yield();
        continue;
      }
      if (ret <= 0) break;
      if (++total >= 4 && memcmp(buf + total - 4, "\r\n\r\n", 4) == 0) break;
    }
    String req(buf);
    int s = req.indexOf(' '), e = req.indexOf(' ', s + 1);
    String path = (s >= 0 && e > s) ? req.substring(s + 1, e) : "/";

    String resp;
    if (instr(path, "/document/") && instr(path, "/ps5/")) {
      resp = "HTTP/1.1 301 Moved Permanently\r\nLocation: http://" + WIFI_HOSTNAME + "/index.html\r\nConnection: close\r\n\r\n";
    }
    else if (instr(path, "/update/") && instr(path, "/ps5/")) {
      static const char* xml =
        "<?xml version=\"1.0\" ?><update_data_list><region id=\"us\">"
        "<force_update><system auto_update_version=\"00.00\" sdk_version=\"01.00.00.09-00.00.00.0.0\""
        " upd_version=\"01.00.00.00\"/></force_update>"
        "<system_pup auto_update_version=\"00.00\" label=\"20.02.02.20.00.07-00.00.00.0.0\""
        " sdk_version=\"02.20.00.07-00.00.00.0.0\" upd_version=\"02.20.00.00\">"
        "<update_data update_type=\"full\"></update_data></system_pup></region></update_data_list>";
      resp = "HTTP/1.1 200 OK\r\nContent-Type: application/xml\r\nContent-Length: " + String(strlen(xml)) + "\r\nConnection: close\r\n\r\n" + String(xml);
    }
    else {
      resp = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nNot Found";
    }
    mbedtls_ssl_write(&ssl, (uint8_t*) resp.c_str(), resp.length());
    mbedtls_ssl_close_notify(&ssl);
  }
cleanup:
  mbedtls_ssl_free(&ssl);
  mbedtls_net_free(&client);
}

void setup() {
  Serial.begin(115200);
  Serial.println("start");

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    return;
  }
  loadAP();

  if (!LittleFS.exists("/pk.pem") || !LittleFS.exists("/cert.der")) {
    Serial.println("Missing pk.pem / cert.der — upload via LittleFS. See command below.");
  }
  else {
    if (!initTLS()) Serial.println("TLS init failed");
  }
  server.onNotFound(handleNotFound);
  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  tickTLS();
}