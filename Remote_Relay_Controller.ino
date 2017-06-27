#include <WiFi101.h>

#include <MemoryFree.h>

#include <LiquidCrystal_I2C.h>
#include <SPI.h>

#define WLAN_SSID       "<SSID>"   // cannot be longer than 32 characters!
#define WLAN_PASS       "<PASS>"

#define TIMEOUT_MS            500    // Amount of time in milliseconds to wait for

#define CONNECTION_KEY "KEIWK23223MNN029KNX929D"
#define GREENRELAY 14
#define ORANGERELAY 15
#define YELLOWRELAY 16
#define AUXRELAY 17

WiFiServer httpServer(80);
int status = WL_IDLE_STATUS;
bool firstRequest = true;

LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address

void setup(void) {
  Serial.begin(115200);

  lcd.begin(16,2);
  lcd.backlight(); // start with the backlight on

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(F("Initializing"));
  lcd.setCursor(0,1);
  lcd.print(WLAN_SSID);

  // Ensure the WiFi shield is operational
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println(F("No WiFi Shield"));
    while (true);
  }

  // Connect to the WiFi network
  while (status != WL_CONNECTED) {
    Serial.print(F("Attempting to connect to SSID "));
    Serial.println(F(WLAN_SSID));
    status = WiFi.begin(WLAN_SSID, WLAN_PASS);
    delay(10000);
  }
  
  httpServer.begin();
  IPAddress ip = WiFi.localIP();
  long rssi = WiFi.RSSI();

  Serial.println(WiFi.SSID());
  Serial.print(F("IP Address: "));
  Serial.println(ip);

  // print the received signal strength:
  Serial.print(F("signal strength (RSSI):"));
  Serial.print(rssi);
  Serial.println(F(" dBm"));
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(WiFi.SSID());
  lcd.setCursor(0,1);
  lcd.print(ip);

  // Setup the relay interface
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
  // Try to get a client which is connected.
  WiFiClient client = httpServer.available();
  if (client) {
    Serial.println(F("Client connected."));
    bool thisIsRequestLine = true; // First input line is the request

    String requestLine = "";
    String currentLine = "";
    unsigned long timeout = millis() + TIMEOUT_MS;
    
    while (client.connected()) {
      bool authenticated = false;
      bool supportedAction = false;
      String statusOutput = "";

      // Check to see if we've waited too long. If so, forget about this client and move to the next
      if (millis() >= timeout) {
        Serial.println();
        Serial.println(F("Connection timed out. Moving to next client."));
        break;
      }
      if (client.available()) {
        timeout = millis() + TIMEOUT_MS;
        char c = client.read();
        if (c == '\n') {
          if (thisIsRequestLine) {
            thisIsRequestLine = false;
            requestLine = currentLine; // Found end of the request line so save this for later parsing
            Serial.println(""); Serial.print(F("Request line final : ")); Serial.println(requestLine);
          }
          if (currentLine.length() == 0) {
            requestLine.trim(); // Remove white space at the end of the line
            while (requestLine.charAt(0) == ' ') requestLine.remove(0); // Remove spaces from the start of the line

            // Only handle GET requests
            if (requestLine.indexOf("GET") == 0) {
              // We have a GET request
              supportedAction = true;

              requestLine.remove(0, 4); // Remove "GET "
              if (requestLine.indexOf(" ") >= 0) {
                requestLine.remove(requestLine.indexOf(" "));
              }

              // Find the parameters
              int delimeterIndex = requestLine.indexOf('?');
              if (delimeterIndex > 0) {
                String parameters = requestLine.substring(delimeterIndex + 1);
                String key = "";
                String value = "";

                // work with the next key/value pair
                delimeterIndex = parameters.indexOf('&');
                do {
                  String pair = parameters.substring(0, delimeterIndex >= 0 ? delimeterIndex : 100);
                  parameters.remove(0, pair.length()+1); // We have the key/value pair so remove them from the parameter list for easier, consistent processing of the next pair

                  // Seperate the key and value from the pair
                  delimeterIndex = pair.indexOf('=');
                  key = pair.substring(0, delimeterIndex >= 0 ? delimeterIndex : 100);
                  value = pair.substring(delimeterIndex >= 0 ? delimeterIndex+1 : 0);

                  // If this is the authentication parameter, check to see if the credentials are correct
                  if (key == "key" && value == CONNECTION_KEY) {
                    authenticated = true;
                  }
                  
                  delimeterIndex = parameters.indexOf('&');
                } while (parameters.length() > 0);
              }
            } else {
              supportedAction = false;
            }

            // Do we have a valid request?
            if (authenticated && supportedAction) {
              // Parse the request and act on it

              requestLine.toLowerCase(); // Make it easier to compare

              Serial.print("request:"); Serial.println(requestLine);
              
              if (requestLine.indexOf("/green=on") >= 0) relay(GREENRELAY, 1);
              if (requestLine.indexOf("/green=off") >= 0) relay(GREENRELAY,0);
              if (requestLine.indexOf("/orange=on") >= 0) relay(ORANGERELAY, 1);
              if (requestLine.indexOf("/orange=off") >= 0) relay(ORANGERELAY,0);
              if (requestLine.indexOf("/yellow=on") >= 0) relay(YELLOWRELAY, 1);
              if (requestLine.indexOf("/yellow=off") >= 0) relay(YELLOWRELAY,0);
              if (requestLine.indexOf("/aux=on") >= 0) relay(AUXRELAY, 1);
              if (requestLine.indexOf("/aux=off") >= 0) relay(AUXRELAY,0);
              if (requestLine.startsWith("/all=on")) { relay(GREENRELAY,1); relay(ORANGERELAY, 1); relay(YELLOWRELAY,1); relay(AUXRELAY, 1); }
              if (requestLine.startsWith("/all=off")) { relay(GREENRELAY,0); relay(ORANGERELAY, 0); relay(YELLOWRELAY,0); relay(AUXRELAY, 0); }

              if (requestLine.startsWith("/green=state")) statusOutput = relayState(GREENRELAY) == LOW ? "ON" : "OFF";
              if (requestLine.startsWith("/orange=state")) statusOutput = relayState(ORANGERELAY) == LOW ? "ON" : "OFF";
              if (requestLine.startsWith("/yellow=state")) statusOutput = relayState(YELLOWRELAY) == LOW ? "ON" : "OFF";
              if (requestLine.startsWith("/aux=state")) statusOutput = relayState(AUXRELAY) == LOW ? "ON" : "OFF";
              if (requestLine.startsWith("/all=state")) {
                statusOutput = relayState(GREENRELAY) == LOW ? "ON" : "OFF";
                statusOutput += ',';
                statusOutput += relayState(ORANGERELAY) == LOW ? "ON" : "OFF";
                statusOutput += ',';
                statusOutput += relayState(YELLOWRELAY) == LOW ? "ON" : "OFF";
                statusOutput += ',';
                statusOutput += relayState(AUXRELAY) == LOW ? "ON" : "OFF";
              }
              Serial.print(F("Return body is ")); Serial.println(statusOutput);
              client.println(F("HTTP/1.1 200 OK"));
              
              // Turn the LCD backlight off when we have the first valid request
              if (firstRequest) {
                lcd.noBacklight();
                firstRequest = false;
              }
            } else {
              if (!supportedAction) {
                client.println(F("HTTP/1.1 405 Method Not Allowed"));
              } else {
                client.println(F("HTTP/1.1 200 AUTH"));
              }
            }
            client.println(F("Content-type: text/html"));
            client.println(F("Connection: close"));
            client.println();
            if (statusOutput.length() > 0) client.println(statusOutput);
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    client.stop();
    Serial.println(F("Client disconnected"));
  }
}

// Setting the state of a relay port
void relay(int port, int state) {
  Serial.print(F("Setting port "));Serial.print(port);Serial.print(F(" to "));Serial.println(state);
  digitalWrite(port, state == 1 ? LOW : HIGH);
}

// Get the state of a relay port
int relayState(int port) {
  int readValue = digitalRead(port);
  Serial.print(F("port state is "));Serial.println(readValue == LOW ? "ON" : "OFF");
  return readValue;
}

int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}


