//Publica los datos de temp. y hum. en la web 80 de la ip local,batria o carga si o no, blink no conectado, datos cuando no internet, voltaje interno esp
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include "DHT.h"
#include <SimpleTimer.h>

#define AP_RESTART "esprestart"
#define AP_CLEAREEPROM "cleareeprom"
#define LED_NODEMCU 2 // pin del led nodemcu
#define PIN_DHT 0 // a que pin esta conectado el DHT?
#define PIN_BOTON 1 // donde esta conectado el boton
#define LED_AMARILLO 5 // D1

int pin = 3; // aquí el pin gpio 3 CAMBIADO POR rx

#include <DNSServer.h>
const char* APssid = "esp8266-pablo";
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);
DNSServer dnsServer;
ESP8266WebServer server(80);

// caracteres para guardar en eeprom
#define MEM_MAX_EEPROM 512
const char* sep_eeprom = "|";
const char* fin_eeprom = "&";
//
// server es el servidor para mostrar los datos leidos de los sensores
// APserver es el servidor para configurar la SSID para conectarse a internet
const char* restartcommand = "/" AP_RESTART;
const char* cleareepromcommand = "/" AP_CLEAREEPROM;
#define ESPWIFI2EEPROM_VERSION "5"

// estado de la conexion
// conectado = 0 sin conexion
// conectado = 1 conectado y con internet
// conectado = 2 conectado pero sin internet
// estructura de datos para las redes Wifi
struct RedesWifi
{
  String q_ssid;
  String q_pass;
  String api_key;
  String q_tiempo;
  String ip;
  long nivel;
  int quality;
  int conectado;
};
RedesWifi red_actual; // informacion de la red seleccionada
//
// estructura de datos para las mediciones
struct Lectura
{
  String hora;
  float temp;
  float hum;
  int power;
  String carga;
  float voltaje;
};
Lectura ultima_lec;
//
long tiempo_ms;      // tiempo entre lecturas en milisegundos
int tiempo1 = 5;     // tiempo en minutos
int tiempo2 = 15; 
int tiempo3 = 30;
int tiempo4 = 60;
//
// html por donde se mostraran los datos
String webPage1 = "<!DOCTYPE HTML>\n"
                  "<html><head><meta content=\"text/html;charset=utf-8\"><title>Sensor Temp. Wi-Fi</title>\n"
                  "<meta name=viewport content=width=device-width, initial-scale=1.0/>\n"
                  "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">"
                  "<meta http-equiv='refresh' content='20'>"
                  "<style type=\"text/css\">\n"
                  ".ip {font-size:40px;color:blue}\n"
                  ".dt {font-size:25px;color:blue}\n"
                  "</style>\n"
                  "</head>\n"
                  "<body style=background-color:#1acec5;>\n"
                  "<body>\n"
                  "<center>\n"
                  "<h1>Estado Actual</h1>\n";
String webPage2 = "</center>\n"
                  "</body></html>";
String webString = "";   // String to display
//
// html del servidor para configurar el ssid de conexion a internet
const char APwebPage1[] PROGMEM = 
                      "<!DOCTYPE HTML>\n"
                      "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"><title>Temp. Edison</title>\n"
                      "<style type=\"text/css\">\n"
                      "body{text-align:center;font-family: sans-serif;background-color: #013ADF;color: #fff;} a:link, a:visited, a:hover, a:active {color:#fff;}\n"
                      "table{font-size:12px;height:300px;}\n"
                      "input{width:99%; font-size:12px;}\n"
                      ".celda{text-align:left;width:99%;}\n"
                      "</style>\n"
                      "</head>\n"
                      "<body>\n"
                      "<h1>Monitor Temp. Setup</h1>\n<br>"
                      "<form action='/APsubmit' method='POST'>\n"
                      "<table style=\"width:99%;\">\n"
                      "<tr><td class=\"celda\">Red:</td></tr>\n"
                      "<tr><td class=\"celda\"><input type=\"text\" name=\"newssid\" id=\"formnewssid\" value=\"\"></td></tr>\n"
                      "<tr><td class=\"celda\">Pass:</td></tr>\n"
                      "<tr><td class=\"celda\"><input type=\"text\" name=\"newpass\" value=\"\" size=\"32\" maxlength=\"64\"></td></tr>\n"
                      "<tr><td class=\"celda\">Key:</td></tr>\n"
                      "<tr><td class=\"celda\"><input type=\"text\" name=\"apikey\" value=\"\"></td></tr>\n"
                      "<tr><td class=\"celda\">Tiempo:</td></tr>\n"
                      "<tr><td class=\"celda\"><input type=\"text\" name=\"tiempo\" value=\"\" size=\"1\"\">1:5 2:15 3:30 4:60 Minutos</td></tr>"
                      "<tr><td colspan=\"2\"><input type=\"submit\" value=\"Enviar\"></td></tr>"
                      "<tr><th style=\"text-align:left;width:99%;height:800px;display:block;overflow-y:scroll;white-space:nowrap;\">";
String APwebPage2 =  "</th></tr></table>\n"
                     "<br><br><form action=\"/\" target=\"_top\"><input type=\"submit\" value=\"Home / busca Redes\"></form>\n"
                     "<br><br><form action=\"" + String(restartcommand) + "\" target=\"_top\"><input type=\"submit\" value=\"Reinicia Monitor\"></form>\n"
                     "<br><br><form action=\"" + String(cleareepromcommand) + "\" target=\"_top\"><input type=\"submit\" value=\"! Borra CONFIG. !\"></form>\n"
                     "<br><br><b>- version: " ESPWIFI2EEPROM_VERSION " -</b>\n"
                     "</body></html>";
String APwebstring = "";   // String to display

// reemplazar con su API key,
String apiKey = "xxxxxxxxxxxxxxxx";
const char* servidor_thingspeak = "api.thingspeak.com";
const char* prueba_conexion = "www.google.com.ar";
long retardo = 0;

String thingtweetAPIKey = "ESCDDHRH6KN0EIVR";

//int blink = 0;
int estado_led = 0;

DHT dht(PIN_DHT, DHT11, 15);
SimpleTimer timer;
SimpleTimer timer_blink;

WiFiClient client;

int estado_boton = 0;

void setup() {
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);    
  
  dht.begin();
    
  red_actual.q_ssid = "";
  red_actual.q_pass = "";
  red_actual.api_key = "";
  red_actual.q_tiempo = "";
  red_actual.nivel = 0;
  red_actual.conectado = 0;

  ultima_lec.hora = "";
  ultima_lec.temp = 0.0;
  ultima_lec.hum = 0;
  ultima_lec.power = 0;
  ultima_lec.carga = "";
  ultima_lec.voltaje = 0.0;  
  
  EEPROM.begin(512);

  pinMode(LED_NODEMCU, OUTPUT); // Led del nodemcu como salida
  pinMode(LED_AMARILLO, OUTPUT); // led extra en la protoboard
  pinMode(PIN_BOTON, INPUT); // boton para borrar la eeprom

  Serial.begin(115200);
  //Serial.setDebugOutput(true);

  // para el esp8266 envia al string AAAAAAA por el tx y si el pulsador RST esta presionado. los recibe por rx y hace un borrado de la memoria
  char i;
  uint16_t A;
  while (1) {
    A = Serial.available();
    if (A == 0) {
      break;
    }
    i = Serial.read();
  }
  Serial.print("AAAAAAA");
  delay(100);
  A = Serial.available();
  if (A > 0) {
    i = Serial.read();
    if (i == 65) {
      parpadea(10, 150);
      // borra la memoria EEPROM
      EEPROM_borrar();
      delay(1000);
      red_actual.conectado = 0; // sin conexion
    }
  }
  conecta();

  timer.setInterval(tiempo_ms, lecturaSensor);
  timer_blink.setInterval(2000, cambioEstado);
  
  Serial.print(F("Mediciones cada:"));
  Serial.print(tiempo_ms/60000);
  Serial.println(F(" minutos"));
  // primera lectura y envio
  lecturaSensor();
}

void loop() {
  server.handleClient();
  timer.run();
    
  estado_boton = digitalRead(PIN_BOTON);
  if( estado_boton == 1 ) {
    // boton presionado y borro la eeprom
    handle_clearAPeeprom();
  }
  //Serial.print(F("Conectado?:"));
  //Serial.println(red_actual.conectado);
  switch(red_actual.conectado) {
    case 0:
      //Serial.println(F("Estado: Sin conexion."));
      conecta();
      break;
    case 1:
      // con conexion
      //Serial.println(F("Estado: conectado a internet"));
      timer_blink.run();
      break;
    case 2:
      Serial.println(F("Conectado pero sin internet!"));
      if (testWiFi() == 0) {
        if (client.connect(servidor_thingspeak, 80)) {
          red_actual.conectado = 1;
          parpadea(5,50);
        }
      }
      break;
  }
}
/*
 * borrar los primero 128 byte de la EEPROM
 */
void EEPROM_borrar() {
  Serial.println(F("! Borrando eeprom !"));
  for (int i = 0; i < 128; ++i) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}
/*
 * 
 */
int espNKWiFiconnect() {
  EEPROM_leer_todo( &red_actual );
  WiFi.mode(WIFI_STA);
  String qssid = red_actual.q_ssid;
  tiempo_ms = tiempo4 * 60000; // valor por defecto 4
  if( red_actual.q_tiempo=="1" ){ tiempo_ms = tiempo1 * 60000; }
  if( red_actual.q_tiempo=="2" ){ tiempo_ms = tiempo2 * 60000; }
  if( red_actual.q_tiempo=="3" ){ tiempo_ms = tiempo3 * 60000; }
  if (qssid != "") {
    String qpass = red_actual.q_pass;
    WiFi.begin( qssid.c_str(), qpass.c_str());
    if( testWiFi() == 0 )
    {
      // conectado
      return 0;
    }
  }
  Serial.println("No se pudo conectar");
  setupWiFiAP();
  return 1;
}
/**
 * 
 */
int testWiFi() {
  int c = 0;
  digitalWrite(LED_NODEMCU, HIGH);  // Turn the LED off by making the voltaje HIGH
  Serial.println(F("Esperando conexion por Wifi..."));
  // c at 60 with delay 500 is for 30 seconds ;)
  while ( c < 60 ) {
    Serial.print(".");
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Conexion exitosa:");
      Serial.println(printConnectionType(WiFi.status()));
      return 0;
    }
    delay(500);
    c++;
  }
  Serial.print("Estado de la conexion:");
  Serial.println(printConnectionType(WiFi.status()));
  return 1;
}
/*
 * configuracion del AP
 */
void setupWiFiAP() {
  long segundos=0;
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(APssid);

  server.on("/", handle_AProot);
  server.on("/generate_204", handle_AProot);
  server.on("/APsubmit", []() {
    // Submit del formulario de configuracion de la red Wifi para conectarse a internet
    String thenewssid = server.arg("newssid");
    String thenewpass = server.arg("newpass");
    String thenewkey  = server.arg("apikey");
    String thenewtime  = server.arg("tiempo");
    if (thenewssid != "") {
      EEPROM_borrar();
      Serial.println(F("! Guardar todo en eeprom !"));
      if( EEPROM_guardar_todo( thenewssid, thenewpass, thenewkey, thenewtime )) {
        String SServerSend = FPSTR(APwebPage1);
        SServerSend += APwebstring + APwebPage2;
        server.send(200, "text/html", SServerSend);
        delay(100);
      }
    }    
    ESP.restart();
  });
  server.on(restartcommand, handle_APrestart);
  server.on(cleareepromcommand, handle_clearAPeeprom);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println(F("Configure nuevo ssid para conectar a internet"));
  Serial.print(F("SoftAP IP address: "));
  Serial.println(WiFi.softAPIP());
  digitalWrite(LED_NODEMCU, LOW);
  while (WiFi.status() != WL_CONNECTED) {
    dnsServer.processNextRequest(); 
    server.handleClient();
    delay(1);
    segundos++;
    if (segundos>120000){break;}
  }
  digitalWrite(LED_NODEMCU, HIGH); 
}
/*
 * 
 */
String printEncryptionType(int thisType) {
  String enc_type = "";
  // read the encryption type and print out the name:
  switch (thisType) {
    case ENC_TYPE_WEP:
      return enc_type = "WEP";
    case ENC_TYPE_TKIP:
      return enc_type = "WPA";
    case ENC_TYPE_CCMP:
      return enc_type = "WPA2";
    case ENC_TYPE_NONE:
      return enc_type = "None";
    case ENC_TYPE_AUTO:
      return enc_type = "Auto";
    default:
      return enc_type = "?";
  }
}
/*
 * 
 */
String printConnectionType(int thisType) {
  String con_type = "";
  // read connection type and print out the name:
  switch (thisType) {
    case 255:
      return con_type = "WL_NO_SHIELD";
    case 0:
      return con_type = "WL_IDLE_STATUS";
    case 1:
      return con_type = "WL_NO_SSID_AVAIL";
    case 2:
      return con_type = "WL_SCAN_COMPLETED";
    case 3:
      return con_type = "WL_CONNECTED";
    case 4:
      return con_type = "WL_CONNECT_FAILED";
    case 5:
      return con_type = "WL_CONNECTION_LOST";
    case 6:
      return con_type = "WL_DISCONNECTED";
    default:
      return con_type = "?";
  }
}
/*
 * Obtiene lista de redes wifi y arma el html para mostrar
 */
String getAPlist() {
  //WiFi.disconnect();
  //delay(100);
  String APstring = "";
  int n = WiFi.scanNetworks();
  Serial.println(F("Scan done"));
  if (n == 0) {
    Serial.println(F("No networks found :("));
    return F("No networks found :(");
  }
  // sort by RSSI
  int indices[n];
  for (int i = 0; i < n; i++) {
    indices[i] = i;
  }
  for (int i = 0; i < n; i++) {
    for (int j = i + 1; j < n; j++) {
      if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
        std::swap(indices[i], indices[j]);
      }
    }
  }
  Serial.println("");
  // HTML Print SSID and RSSI for each network found
  APstring = F("<ul>");
  for (int i = 0; i < n; ++i)
  {
    APstring += F("<li>");
    APstring += i + 1;
    APstring += F(":&nbsp;&nbsp;<b>");
    APstring += F("<a href=\"#\" target=\"_top\" onClick=\"document.getElementById(\'formnewssid\').value=\'");
    APstring += WiFi.SSID(indices[i]);
    APstring += F("\'\">");
    APstring += WiFi.SSID(indices[i]);
    APstring += F("</a>");
    APstring += F("</b>&nbsp;&nbsp;&nbsp;(");
    //APwebstring += WiFi.RSSI(indices[i]);
    //APwebstring += F("&nbsp;dBm)&nbsp;&nbsp;&nbsp;");
    APstring += conviertoPor(WiFi.RSSI(indices[i]));
    APstring += F("%)&nbsp;&nbsp;&nbsp;");
    APstring += printEncryptionType(WiFi.encryptionType(indices[i]));
    APstring += F("</li>");
  }
  APstring += F("</ul>");
  return APstring;
}
/*
 * 
 */
String estado_conexion(int conectado)
{
  switch (conectado) {
    case 0:
      return "Sin conexion";
    case 1:
      return "Si";
    case 2:
      return "Conectado pero sin internet";
  }
}
/*
 * Funcion que guarda ssid, pass, apikey y tiempo de lectura en la EEPROM
 */
boolean EEPROM_guardar_todo( String newssid, String newpass, String newkey, String newtime )
{
  //formo cadena para guardar en eeprom
  String cad_eeprom = newssid + sep_eeprom + newpass + sep_eeprom + newkey + sep_eeprom + newtime + sep_eeprom + fin_eeprom;
  for (int i = 0; i < cad_eeprom.length(); i++)  
  {
    EEPROM.write(i, cad_eeprom[i]);
  }
  if (EEPROM.commit()) {
    APwebstring = F("<h2><b><p>Datos Guardados - Reiniciando</b></h2>\n");
    Serial.println(F("Datos guardados"));
    return true;
  } else {
    APwebstring = F("<h2><b><p>No se pudo guardar . Reintente.</b></h2>\n");
    Serial.println(F("No se pudo guardar"));
  }
  return false;
}
/*
 * Funcion para leer la info guardada en la EEPROM
 */
void EEPROM_leer_todo( RedesWifi* red_actual )
{
  int posi=0;
  String caracter="";
  String cadena="";
  Serial.println("");
  for( int i=0; i<=MEM_MAX_EEPROM; i++)
  {
    caracter = char(EEPROM.read(i));
    //Serial.print(caracter);
    if( caracter == fin_eeprom ) break;
    if( caracter == sep_eeprom )
    {
      switch(posi)
      {
        case 0:
          red_actual->q_ssid = cadena;
          Serial.print("SSID:");
          break;
        case 1:
          red_actual->q_pass = cadena;
          Serial.print("PASS:");
          break;
        case 2:
          red_actual->api_key = cadena;
          Serial.print("API_KEY:");
          break;
        case 3:
          red_actual->q_tiempo = cadena;
          Serial.print("Tiempo:");
          break;
      }
      Serial.println(cadena);
      cadena="";
      posi++;
    } else if ( caracter != sep_eeprom )
    {
      cadena += caracter;
    }
  } 
}
/*
 * borrar EEPROM hasta deilimitador final
 */
void handle_clearAPeeprom() {
  EEPROM_borrar();
  delay(1000);
  ESP.restart();
}
/*
 * Reinicia esp8266
 */
void handle_APrestart() {
  Serial.println(F("!Reiniciando!"));
  delay(1000);
  ESP.restart();
}
/*
 * AP server con formulario para alta de red Wifi para conectarse a internet
 */
void handle_AProot() {
  APwebstring = getAPlist();
  String SServerSend = FPSTR(APwebPage1);
  SServerSend += APwebstring + APwebPage2;
  server.send(200, "text/html", SServerSend);
  delay(100);
}
/*
 * Se ejecuta esta parte cuando se accede a una URI que no esta definida / que no se encuentra
 */
void handleNotFound() {
  Serial.print("\t\t\t\t URI Not Found: ");
  Serial.println(server.uri());
  server.send( 200, "text/plain", "URI Not Found" );
}
/*
 * 
 */
void handle_root() {
  // para saber cuanto falta para la proxima medicion
  long dife;
  dife = (tiempo_ms - retardo) / 1000;
  int mi, se;
  se = dife % 60; // % resto de una division
  mi = dife / 60;
  String tt = "";
  tt += String(mi) + ":";
  if (se < 10) {
    tt += "0";
  }
  tt += String(se);
  if ( red_actual.conectado != 1 ) {
    // sin conexion a internet
    tt = "--";
    parpadea(12, 80);
  }
  // Envia los datos de temperatura al webserver local 80
  webString = "<p class='dt'>Temperatura: " + String(ultima_lec.temp) + " C°<br>" +
              "Humedad: " + String(ultima_lec.hum) + "%<br>" +
              "Lecturas cada: " + String(tiempo_ms / 60000) + " Min." + "<br>" +
              "IP  Local: " + WiFi.localIP().toString() + "<br>" +
              "Prox. Envío en: " + tt + " Min." + "<br>";
  server.send(200, "text/html", webPage1 + webString + webPage2);
  delay(100);
}
/*
 * 
 */
void conecta() {
  if( espNKWiFiconnect() == 0 ) {
    red_actual.conectado = 1;
    Serial.println("Conectado");
    Serial.print("IP Local:");
    Serial.println(WiFi.localIP());
    server.on(restartcommand, handle_APrestart);
    server.on(cleareepromcommand, handle_clearAPeeprom);
    server.on("/info", []() {
      webString = "<p class='dt'>Direccion IP Local: " + WiFi.localIP().toString() + "<br>ApiKey: " + apiKey + "</p>";
      server.send(200, "text/html", webPage1 + webString + webPage2);
      delay(100);
    });
    // comando para hacer un reincio
    server.on("/restart", []() {
      ESP.restart();
    });
    server.on("/reset", []() {
      //Borra parametros guardados
      parpadea(8,150);
      // borro la memoria EEPROM
      EEPROM_borrar();
      delay(1000);
      red_actual.conectado = 0;
    });
    server.on("/", handle_root);
    server.begin();
    Serial.println("Servidor HTTP iniciado");
  } else {
    Serial.println("No se puede conectar usando los datos guardados en la EEPROM");
    red_actual.conectado = 0;
  }
}
/*
 * 
 */
void lecturaSensor() {
  lecturaSensor_DHT11();
  if (client.connect(servidor_thingspeak, 80)) {
    //if (client.available()) {
    Serial.println(F("Enviando a thingspeak"));
    envioThinkSpeak();
  } else {
    Serial.println(F("Conexion a thingspeak falló"));
  }
}
/*
 * 
 */
void cambioEstado() {
  estado_led = !estado_led;
  digitalWrite(LED_NODEMCU, estado_led);
}
/*
 * 
 */
void lecturaSensor_DHT11() {
  delay(100);
  ultima_lec.temp = dht.readTemperature();
  ultima_lec.hum = dht.readHumidity();
  ultima_lec.voltaje = ESP.getVcc();
  ultima_lec.power = !digitalRead(pin);
  Serial.print(F("Temperatura:"));
  Serial.println(ultima_lec.temp);
  Serial.print(F("Humedad:"));
  Serial.println(ultima_lec.hum);
  Serial.print(F("Voltaje:"));
  Serial.println(ultima_lec.voltaje);
  return;
}
/*
 * 
 */
void envioThinkSpeak() {
  String postStr = red_actual.api_key;
  postStr += "&field1=";
  postStr += String(ultima_lec.temp);
  postStr += "\r\n\r\n";
  postStr += "&field2=";
  postStr += String(ultima_lec.hum);
  postStr += "\r\n\r\n";
  postStr += "&field3=";
  postStr += String(ultima_lec.voltaje / 1024.00f);
  postStr += "\r\n\r\n";
  Serial.print(F("api_key en envioThinkSpeak:"));
  Serial.println(red_actual.api_key);
  client.print("POST /update HTTP/1.1\n");
  client.print("Host: api.thingspeak.com\n");
  client.print("Connection: close\n");
  client.print("X-THINGSPEAKAPIKEY: " + red_actual.api_key + "\n");
  client.print("Content-Type: application/x-www-form-urlencoded\n");
  client.print("Content-Length: ");
  client.print(postStr.length());
  client.print("\n\n");
  client.print(postStr);
  // Read all the lines of the reply from server and print them to Serial
  String line = client.readStringUntil('\r');
  Serial.println(line);
  client.stop();
  Serial.println("envioThinkSpeak postStr: ");
  Serial.println(postStr);
  Serial.println("Esperando...");
  // thingspeak needs minimum 15 sec delay between updates
  delay(20000);
}
/*
 * 
 */
int conviertoPor(float niv) {
  int quality=0;    
  // Conversion rssi (dB) a % Quality:
  if(niv <= -100) quality = 0;
  else if(niv >= -50) quality = 100;
  else quality = 2 * (niv + 100);
  return quality;
}
/*
 * 
 */
void parpadea(int veces, int tiempo) {
  int estado_led2 = 0;
  for( int i=0;i<veces;i++ ) {
    int estado_led2 = !estado_led2;
    digitalWrite(LED_NODEMCU, estado_led2);
    delay(tiempo);
  }
}
