Remote Relay Controller

An Arduino project that serves as a WiFi connected remote appliance that controls an HVAC system with relays.

The appliance has a simple API that accepts commands to turn on and off relays and get the current status of 
those relays. The API is protected via a key to protect against basic attempts at hacking. 

API:
The commands are determined by the requested path and the security key is passed as a GET parameter.

Example:
http://<URL>/green=on?key=12345 - turns the green relay on
http://<URL>/orange=off?key=12345 - turns the orange relay off
http://<URL>/all=on?key=12345 - turns all relays on
http://<URL>/all=off?key=12345 - turns all relays off
http://<URL>/yellow=state?key=12345 - returns the status of the yellow relay (ON or OFF)
http://<URL>/all=status?key=12345 - returns the status of all 4 of the relays ("OFF,ON,ON,OFF")

Commands:
/green=on
/green=off
/orange=on
/orange=off
/yellow=on
/yellow=off
/aux=on
/aux=off
/all=on
/all=off
/<relay>=state
/all=state

Configuration:
Update the following #define options:
CONNECTION_KEY - unique complex key signature that needs to match the REMOTE_RELAY_KEY setting in the thermostat-control module
WLAN_SSID - WiFi network SSID
WLAN_PASS - WiFi network password

Hardware:
The project is designed to run on an Uno or a Mega 2560. Both devices are more robust than needed for 
this simple controller. The WiFi shield is a CC3000 and an LCM1602 LCD display is used to show the status and 
IP address so the thermostat configuration can be set. The relay controller used is a 4-relay HL-54S.

Wiring Pin Chart:
Arduino
Gnd  - LCM1602 Gnd
Gnd  - Relay Gnd
+5v  - LCM1602 Vcc
+5v  - Relay Vcc
SCL  - LCM1602 SCL
SDA  - LCM1602 SDA
D14  - Relay In 1
D15  - Relay In 2
D16  - Relay In 3
D17  - Relay In 4

