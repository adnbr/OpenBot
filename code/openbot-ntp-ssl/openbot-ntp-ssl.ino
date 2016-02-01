/*  Leeds Hackspace OpenBot
 *  Tweets (@LHSOpenBot) when the space is open for non-keyholders.
 *  For more information ask Aidan, or check the wiki. Flash onto an ESP-12E.
 *  
 *  Set up Wifi/API credentials in keys.h.
 *  Alter tweeted text in strings.h.
 *  
 *  http://www.leedshackspace.org.uk
 *  CC-BY-SA 2016
 */

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h> 
#include <WiFiudp.h>
#include <TimeLib.h>
#include <Timezone.h>    //https://github.com/JChristensen/Timezone
#include <EEPROM.h>

#include "keys.h"
#include "strings.h"

#define PIN_LED_RED        D0
#define PIN_LED_GREEN      D1
#define PIN_BUTTON         D2
#define PIN_MOTOR_UP       D3
#define PIN_MOTOR_DOWN     D4

#define LIGHT_RED          1
#define LIGHT_GREEN        2
#define LIGHT_OFF          0          

#define EEPROM_ADDRESS     0

#define UP                 1
#define DOWN               2
#define STOP               3
#define KNOB_VARIANCE      6 // +/- ideal knob position.

static WiFiClientSecure client;
WiFiUDP udp;

// Port to listen on for UDP packets
const unsigned short localUDPPort = 2390;
IPAddress timeServerIP; 
const char* ntpServerName = "1.uk.pool.ntp.org";

const int NTP_PACKET_SIZE = 48;
byte packetBuffer[ NTP_PACKET_SIZE]; 

//United Kingdom (London, Belfast, Edinburgh)
TimeChangeRule BST = {"BST", Last, Sun, Mar, 1, 60};        // British Summer Time
TimeChangeRule GMT = {"GMT", Last, Sun, Oct, 2, 0};         // Greenwich Mean Time
// Set the timezone to allow for automatic DST updates.
  Timezone UK(BST, GMT);

time_t localTime;
short closingHour, closingMinute;

bool turnKnob;
unsigned long lastKnobCheck;

time_t getNtpTime()
{
  while (udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      
      Serial.println("Receive NTP Response");

      // Read packet into buffer
      udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long secsSince1900;
      
      // Convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];

      // Calculate epoch and calculate the local time from this. 
      time_t utc = secsSince1900 - 2208988800UL;
      localTime = UK.toLocal(utc);
      
      return utc;
    }
  }

  Serial.println("No response from NTP server (" + String(ntpServerName) + ", " + timeServerIP + ")");
  return 0;
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // Set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;            // Stratum, or type of clock
  packetBuffer[2] = 6;            // Polling Interval
  packetBuffer[3] = 0xEC;         // Peer Clock Precision
  
  // 8 bytes of zero for Root Delay & Root Dispersion.
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  
  // All NTP fields have been given values, now
  // send a packet requesting a timestamp.                 
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void eepromWriteCheckValue(int value) {
  byte lowByte = ((value >> 0) & 0xFF);
  EEPROM.write(EEPROM_ADDRESS, lowByte);
}

unsigned int eepromReadCheckValue() {
  byte lowByte = EEPROM.read(EEPROM_ADDRESS);
  return lowByte;
}

String displayTime(short hour, short minute){
  String output;
  if (hour < 10) {
    output = "0";
  }
  output = output + hour + ":";
  if (minute < 10) {
    output = output + "0";
  }
  output = output + minute;
  return output;
}

void switchLEDS(char lightMode) {
  char red, green;
  switch (lightMode) {
    case LIGHT_OFF:
      red = LOW;
      green = LOW;
      break;
    case LIGHT_RED:
      red = HIGH;
      green = LOW;
      break;    
    case LIGHT_GREEN:
      red = LOW;
      green = HIGH;
      break;
  }
  digitalWrite(PIN_LED_RED, red);
  digitalWrite(PIN_LED_GREEN, green);
}


String buildRequest(int hours, String closingTime, String tweet) {
  // Construct the actual data to be transmitted - includes the dial number, 
  // expected closing time and the message itself.
  String data = String("field1=") + hours + 
                       "&field2=" + closingTime + 
                       "&field3=" + tweet +
                       "&twitter=" + api_twitter_feed + 
                       "&tweet=" + tweet;

  // Construct the HTTP header to be send, and append the data.
  return String("POST ") + api_uri + " HTTP/1.1\r\n" +
               "Host: " + api_host + "\r\n" + 
               "THINGSPEAKAPIKEY: " + api_key + "\r\n" +
               "Content-Type: application/x-www-form-urlencoded\r\n" +
               "Content-Length:"  + data.length() + "\n\n" + data;
}

bool sendHTTPRequest(String httpRequest, int hours) {
  
    // Write the hours value to the eeprom to allow for recovery should the SSL handshake
    // crash the device (sometimes this happens thanks to low/fragmented memory.)
    eepromWriteCheckValue(hours);
    Serial.println("Writing check value to EEPROM");
    
    // Try and connect to the API host. Restart at 5x failures.
    int failCount = 0;
    while (!client.connect(api_host, api_port)) {
      failCount++;
      Serial.println("Connection to '"+ String(api_host) + ":" + String(api_port) + "' failed on attempt " + String(failCount) + ". Waiting 1 second to retry.");
      delay(1000);
      if (failCount > 5) {
        ESP.restart();
        return false;
      }
    }

    // Check the certificate hash.
    if (!client.verify(api_cert_hash, api_host)) {
      Serial.println("Certificate doesn't match.");
      return false;
    }

    // Send the HTTP Request to the server. 
    client.print(httpRequest);
    
    Serial.println("Sending HTTP Request:");
    Serial.println(httpRequest);
    Serial.print("\n\n\n\n\n\n");

    // Wait for max 8 seconds for reply, otherwise exit.
    unsigned long timeout = millis() + 8000;
    while (!client.available()) {
      if (timeout - millis() < 0) {
        Serial.println("Client Timeout.");
        client.stop();
        return false;
      }
    }
    
    Serial.println("Received from server:");
    while(client.available()){
      Serial.print(client.readStringUntil('\r'));
    }
    Serial.print("\n\n\n\n\n\n");

    // Close the connection, we're done here.
    Serial.println("Closing connection with '" + String(api_host) + "'");
    client.stop();

    // If we've got to here then the message has probably gone through, nothing
    // more we can do on our end.
    eepromWriteCheckValue(255);
    Serial.println("Clearing check value from EEPROM.");
    
    return true;
}

String createHTTPRequest(int hours) {

  String message;
  short randomNumber;
  
  // Generate the HTTP request to be sent to ThingSpeak.
  if (hours == 0) {
    
    // Closed has been selected on the dial. Tweet a "random" closed message.
    randomNumber = random (1, numberClosedStrings);
    message = closeSpace[randomNumber].leader + "(It's " + displayTime(hour(localTime), minute(localTime)) + ")";

    turnKnob = false;
    
    return buildRequest(hours, displayTime(hour(localTime), minute(localTime)), message);
    
  } else {
    
    // Space will be open for a period of time, tweet a "random" open message.
    randomNumber = random (1, numberOpenStrings);

    // Increment the timer, if the new value is over 24 hours (midnight) then subtract 24 to make
    // it into a valid time. If equal to 24 then still subtract 24 so midnight is represented as 00:00
    closingHour = hour(localTime) + hours;
    if (closingHour >= 24) {
      closingHour -= 24;
    }
    

    // Round the minutes down to the closest quarter-hour. No need for minute level precision.
    closingMinute = minute(localTime) - (minute(localTime) % 15);

    // If the hour is singular "1" then we need to use the singular text. Otherwise, add the 
    // number and append "hours".
    if (hours == 1) {
      message = message = openSpace[randomNumber].leader + openSpace[randomNumber].singular + openSpace[randomNumber].punctuation + " (Until ~" + displayTime(closingHour, closingMinute) + ")";
    } else {
      message = message = openSpace[randomNumber].leader + hours + " hours" + openSpace[randomNumber].punctuation + " (Until ~" + displayTime(closingHour, closingMinute) + ")";
    }

    turnKnob = true;
    
    return buildRequest(hours, displayTime(hour(localTime), minute(localTime)), message);
  }
}

void setup() {
  // Setup the pin directions
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_BUTTON, INPUT);

  // Turn off all LEDs to give an indication of reboot, then
  // wait for a bit. Useful for debugging!
  switchLEDS(LIGHT_OFF);

  delay(500);

  // Start the serial. start of message line break to clear away
  // from the garbage the ESP8266 dumps out the serial port at boot.
  Serial.begin(115200);
  Serial.println("\n\nStarting OpenBot..."); 

  // We are starting to do work now, red LED on.
  switchLEDS(LIGHT_RED);

  // Connect to the wifi in station mode.
  Serial.println("Connecting to '" + String(ssid) + "'");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  // Wait for a connection to be made, then print out the IP address.
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.print("\nConnected to '" + String(ssid) + "' with IP: ");  
  Serial.println(WiFi.localIP());

  // Look up the IP address of the NTP time server, then set up the
  // recurring time polling.
  Serial.println("Syncing with NTP server (" + String(ntpServerName) + ", " + timeServerIP + ")");
  WiFi.hostByName(ntpServerName, timeServerIP); 
  setSyncProvider(getNtpTime);

  // Wait for the time to be synced with NTP.
  while (timeStatus() == timeNotSet) {  
    Serial.print("."); 
    delay(250); 
  }

  Serial.println("Current time (UTC) from NTP: " + displayTime(hour(), minute()));
  Serial.println("Current local time (UK):     " + displayTime(hour(localTime), minute(localTime)));

  unsigned int checkValue = eepromReadCheckValue();
  if (checkValue != 255) {
    // The device has rebooted before completing the submission.
    Serial.println("Uncompleted submission detected. Resending value '" + String(checkValue) + "'.");
    
    randomSeed(hour() * second() + minute() / analogRead(A0));
    
    String httpRequest = createHTTPRequest(checkValue);
    sendHTTPRequest(httpRequest, checkValue);
  }
  
  // Work complete, green LED lit.
  switchLEDS(LIGHT_GREEN);
  
}

bool moveKnob() {
  if (minute() - closingMinute == 0) {
    short currentPos = analogRead(A0);

    // The knob can be divided into 8 quite nicely - 128 - and we want to move to the middle of the
    // next segment so + 64.
    short targetPos = ((hour() - closingHour) * 128) + 64;
    
    while (!abs(currentPos - targetPos) <= KNOB_VARIANCE) {
      currentPos = analogRead(A0);
      // Not there yet. Move that knob!
      char motorDirection = UP;
      if (currentPos > targetPos) {
        motorDirection = DOWN;
      }
      controlMotor(motorDirection);
      delay(25);
      controlMotor(STOP);
    }
    // if pot doesnt equal what we want, +/- a few
    // move the pot.
    // what we are pretty much aiming for is Hours Remaining * 128 + 64.
  }
}

void controlMotor(char direction) {
  char up, down;
  switch (direction) {
    case UP:
      up = HIGH;
      down = LOW;
      break;
    case DOWN:
      up = LOW;
      down = HIGH;
      break;    
    case STOP:
      up = LOW;
      down = LOW;
      break;
  }
  digitalWrite(PIN_MOTOR_UP, up);
  digitalWrite(PIN_MOTOR_DOWN, down);
}


void loop() {

  // Wait for a button press
  if (digitalRead(PIN_BUTTON) == LOW) {

    // Set the LEDs to red
    switchLEDS(LIGHT_RED);

    // Read the ADC value, divide into 8 and round to get the number of hours indicated.
    short knobValue = analogRead(A0);
    short hours = floor(knobValue / 128);
    
    // Seed the RNG. It's still rubbish, but perhaps this will make it a bit better.
    randomSeed(hour() * second() + minute() / knobValue);
        
    String httpRequest = createHTTPRequest(hours);
    sendHTTPRequest(httpRequest, hours);

    switchLEDS(LIGHT_GREEN);
  }

  if (millis() - lastKnobCheck > 30000 && turnKnob == true) {
    moveKnob();
  }

}
