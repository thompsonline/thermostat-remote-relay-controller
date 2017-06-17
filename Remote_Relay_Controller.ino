#include <LiquidCrystal_I2C.h>
#include <Adafruit_CC3000.h>
#include <SPI.h>
#include "utility/debug.h"
#include "utility/socket.h"

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11

Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIVIDER); // you can change this clock speed

#define WLAN_SSID       "<SSID>"   // cannot be longer than 32 characters!
#define WLAN_PASS       "<PASS>"
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

#define LISTEN_PORT           80      // What TCP port to listen on for connections.  
                                      // The HTTP protocol uses port 80 by default.

#define MAX_ACTION            10      // Maximum length of the HTTP action that can be parsed.

#define MAX_PATH              64      // Maximum length of the HTTP request path that can be parsed.
                                      // There isn't much memory available so keep this short!

#define BUFFER_SIZE           MAX_ACTION + MAX_PATH + 20  // Size of buffer for incoming request data.
                                                          // Since only the first line is parsed this
                                                          // needs to be as large as the maximum action
                                                          // and path plus a little for whitespace and
                                                          // HTTP version.

#define TIMEOUT_MS            500    // Amount of time in milliseconds to wait for
                                     // an incoming request to finish.  Don't set this
                                     // too high or your server could be slow to respond.

#define CONNECTION_KEY "KEIWK23223MNN029KNX929D"
#define GREENRELAY 14
#define ORANGERELAY 15
#define YELLOWRELAY 16
#define AUXRELAY 17

Adafruit_CC3000_Server httpServer(LISTEN_PORT);
uint8_t buffer[BUFFER_SIZE+1];
int bufindex = 0;
char action[MAX_ACTION+1];
char path[MAX_PATH+1];
char params[MAX_PATH+1];
char* tokenizer;
char paramkey[MAX_PATH+1];
char value[MAX_PATH+1];

LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address

void setup(void)
{
  Serial.begin(115200);

  lcd.begin(16,2);
  for(int i = 0; i< 3; i++)  {
    lcd.backlight();
    delay(250);
    lcd.noBacklight();
    delay(250);
  }
  lcd.backlight(); // finish with backlight on 

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(F("Initializing"));
  lcd.setCursor(0,1);
  lcd.print(WLAN_SSID);
  
  // Initialise the module
  Serial.println(F("\nInitializing..."));
  if (!cc3000.begin()) {
    Serial.println(F("Couldn't begin()! Check your wiring?"));
    while(1);
  }
  
  Serial.print(F("\nAttempting to connect to ")); Serial.println(WLAN_SSID);
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    lcd.setCursor(0,1);
    lcd.print("Failed to initialize WiFi");
    while(1);
  }
   
  Serial.println(F("Connected!"));
  
  Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP()) {
    delay(100); // ToDo: Insert a DHCP timeout!
  }  

  // Display the IP address DNS, Gateway, etc.
  while (! displayConnectionDetails()) {
    delay(1000);
  }

  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  
  if(cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv)) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(WLAN_SSID);
    lcd.setCursor(0,1);
    lcd.print((uint8_t)(ipAddress >> 24));
    lcd.setCursor(3,1);
    lcd.print(".");
    lcd.setCursor(4,1);
    lcd.print((uint8_t)(ipAddress >> 16));
    lcd.setCursor(8,1);
    lcd.print(".");
    lcd.setCursor(9,1);
    lcd.print((uint8_t)(ipAddress >> 8));
    lcd.setCursor(12,1);
    lcd.print(".");
    lcd.setCursor(13,1);
    lcd.print((uint8_t)(ipAddress));
  } else {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("unable to get IP");
  }

  pinMode(GREENRELAY, OUTPUT);
  pinMode(ORANGERELAY, OUTPUT);
  pinMode(YELLOWRELAY, OUTPUT);
  pinMode(AUXRELAY, OUTPUT);
  digitalWrite(GREENRELAY, HIGH);
  digitalWrite(ORANGERELAY, HIGH);
  digitalWrite(YELLOWRELAY, HIGH);
  digitalWrite(AUXRELAY, HIGH);

  // Start listening for connections
  httpServer.begin();
  
  Serial.println(F("Listening for connections..."));

}

void loop(void) {
  bool done;
  
  // Try to get a client which is connected.
  Adafruit_CC3000_ClientRef client = httpServer.available();
  if (client) {
    Serial.println(F("Client connected."));
    // Process this request until it completes or times out.
    // Note that this is explicitly limited to handling one request at a time!

    // Clear the incoming data buffer and point to the beginning of it.
    bufindex = 0;
    memset(&buffer, 0, sizeof(buffer));
    
    // Clear action and path strings.
    memset(&action, 0, sizeof(action));
    memset(&path,   0, sizeof(path));
    memset(&params, 0, sizeof(params));
    memset(&paramkey, 0, sizeof(params));
    memset(&value, 0, sizeof(params));

    tokenizer = NULL;

    // Set a timeout for reading all the incoming data.
    unsigned long endtime = millis() + TIMEOUT_MS;
    
    // Read all the incoming data until it can be parsed or the timeout expires.
    bool parsed = false;
    while (!parsed && (millis() < endtime) && (bufindex < BUFFER_SIZE)) {
      if (client.available()) {
        buffer[bufindex++] = client.read();
      }
      parsed = parseRequest(buffer, bufindex, action, path, params);
    }

    // Handle the request if it was parsed.
    if (parsed) {
      Serial.println(F("Processing request"));
      Serial.print(F("Action: ")); Serial.println(action);
      Serial.print(F("Path: ")); Serial.println(path);
      // Check the action to see if it was a GET request.
      if (strcmp(action, "GET") == 0) {
        // Respond with the path that was accessed.
        // First send the success response code.
        client.fastrprintln(F("HTTP/1.1 200 OK"));
        // Then send a few headers to identify the type of data returned and that
        // the connection will not be held open.
        client.fastrprintln(F("Content-Type: text/plain"));
        client.fastrprintln(F("Connection: close"));
        client.fastrprintln(F("Server: Thermostat Remote Relay Controller"));
        // Send an empty line to signal start of body.
        client.fastrprintln(F(""));
//        // Now send the response data.
//        client.fastrprintln(F("Hello world!"));
//        client.fastrprint(F("You accessed path: ")); client.fastrprintln(path);
//        client.fastrprint(F("Params: ")); client.fastrprintln(params);

        tokenizer = strtok(params, "?&=");
        if (tokenizer != NULL) strncpy(paramkey, tokenizer, MAX_PATH);
        Serial.println(paramkey);
        tokenizer = strtok(NULL, "?&=");
        if (tokenizer != NULL) strncpy(value, tokenizer, MAX_PATH);
        Serial.println(value);

        if (strcmp(paramkey, "key") == 0) {
          Serial.println("checking authentication key");
          if (strcmp(value, CONNECTION_KEY) == 0) {
            Serial.println("authenticated");

            // Since we're authenticated, process the request

            if (strcmp(path, "/green=on") == 0) relay(GREENRELAY,1);
            if (strcmp(path, "/green=off") == 0) relay(GREENRELAY,0);
            if (strcmp(path, "/orange=on") == 0) relay(ORANGERELAY,1);
            if (strcmp(path, "/orange=off") == 0) relay(ORANGERELAY,0);
            if (strcmp(path, "/yellow=on") == 0) relay(YELLOWRELAY,1);
            if (strcmp(path, "/yellow=off") == 0) relay(YELLOWRELAY,0);
            if (strcmp(path, "/aux=on") == 0) relay(AUXRELAY,1);
            if (strcmp(path, "/aux=off") == 0) relay(AUXRELAY,0);
            if (strcmp(path, "/all=on") == 0) { relay(GREENRELAY,1); relay(ORANGERELAY, 1); relay(YELLOWRELAY,1); relay(AUXRELAY, 1); }
            if (strcmp(path, "/all=off") == 0) { relay(GREENRELAY,0); relay(ORANGERELAY, 0); relay(YELLOWRELAY,0); relay(AUXRELAY, 0); }

            if (strcmp(path, "/green=state") == 0) client.fastrprintln(relayState(GREENRELAY));
            if (strcmp(path, "/orange=state") == 0) client.fastrprintln(relayState(ORANGERELAY));
            if (strcmp(path, "/yellow=state") == 0) client.fastrprintln(relayState(YELLOWRELAY));
            if (strcmp(path, "/aux=state") == 0) client.fastrprintln(relayState(AUXRELAY));
            if (strcmp(path, "/all=state") == 0) {
              Serial.println("all state");
              client.fastrprint(relayState(GREENRELAY));
              client.fastrprint(",");
              client.fastrprint(relayState(ORANGERELAY));
              client.fastrprint(",");
              client.fastrprint(relayState(YELLOWRELAY));
              client.fastrprint(",");
              client.fastrprintln(relayState(AUXRELAY));
            }
          } else {
            Serial.println("Invalid key");
          }
        } else {
          Serial.println("no auth key provided");
        }
        
      } else {
        // Unsupported action, respond with an HTTP 405 method not allowed error.
        client.fastrprintln(F("HTTP/1.1 405 Method Not Allowed"));
        client.fastrprintln(F(""));
      }
    }

    // Wait a short period to make sure the response had time to send before
    // the connection is closed (the CC3000 sends data asyncronously).
    delay(100);

    // Close the connection when done.
    Serial.println(F("Client disconnected"));
    client.close();
  
    lcd.noBacklight();

  }
}

// Return true if the buffer contains an HTTP request.  Also returns the request
// path and action strings if the request was parsed.  This does not attempt to
// parse any HTTP headers because there really isn't enough memory to process
// them all.
// HTTP request looks like:
//  [method] [path] [version] \r\n
//  Header_key_1: Header_value_1 \r\n
//  ...
//  Header_key_n: Header_value_n \r\n
//  \r\n
bool parseRequest(uint8_t* buf, int bufSize, char* action, char* path, char *params) {
  // Check if the request ends with \r\n to signal end of first line.
  if (bufSize < 2)
    return false;
  if (buf[bufSize-2] == '\r' && buf[bufSize-1] == '\n') {
    parseFirstLine((char*)buf, action, path, params);
    return true;
  }
  return false;
}

// Parse the action and path from the first line of an HTTP request.
void parseFirstLine(char* line, char* action, char* path, char *params) {
  // Parse first word up to whitespace as action.
  char* lineaction = strtok(line, " ");
  if (lineaction != NULL)
    strncpy(action, lineaction, MAX_ACTION);
  // Parse second word up to whitespace as path.
  char* linepath = strtok(NULL, " ?");
  if (linepath != NULL)
    strncpy(path, linepath, MAX_PATH);

  char* lineparam = strtok(NULL, " ?");
  if (lineparam != NULL) 
    strncpy(params,  lineparam, MAX_PATH);
    
}

// Tries to read the IP address and other connection details
bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  
  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}

void relay(int port, int state) {
  Serial.print("Setting port ");Serial.print(port);Serial.print(" to ");Serial.println(state);
  digitalWrite(port, state == 1 ? LOW : HIGH);
}

char *relayState(int port) {
  Serial.print("port state is ");Serial.println(digitalRead(port));
  return (digitalRead(port) == LOW ? "ON" : "OFF"); 
}

