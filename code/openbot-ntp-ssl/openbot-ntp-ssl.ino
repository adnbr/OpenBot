/*
 *  Leeds Hackspace OpenBot
 *  Tweets (@LHSOpenBot) when the space is open for non-keyholders.
 *  For more information ask Aidan, or check the wiki. Flash onto an ESP-12E/NodeMCU dev board.
 *
 *  Set up Wifi/API credentials in keys.h.
 *  Alter tweeted text in strings.h.
 *
 *  Thingspeak channel fields:
 *
 *  +-------+-------+---------+-----------+
 *  | #1    | #2    | #3      | #4        |
 *  +-------+-------+---------+-----------+
 *  | hours | until | message | localutc  |
 *  +-------+-------+---------+-----------+
 *
 *  http://www.leedshackspace.org.uk
 *  CC-BY-SA 2016
 *
 */

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <EEPROM.h>

#include "keys.h"
#include "strings.h"
#include "LocalNTP.h"
#include "motor.h"

#define PIN_LED_RED        D3
#define PIN_LED_GREEN      D4
#define PIN_BUTTON         D5
#define PIN_MOTOR_FWD      D1
#define PIN_MOTOR_REV      D2
#define KNOB_VARIANCE      3 // +/- ideal knob position.

#define EEPROM_ADDRESS     0
#define EEPROM_OK          255

//const char* ntpServerName = "time.nist.gov";
const char* ntpServerName = "uk.pool.ntp.org";
#define NTP_UPDATE_FREQ    21700

// United Kingdom (London, Belfast, Edinburgh)
TimeChangeRule SummerTime = {"BST", Last, Sun, Mar, 1, 60};          // British Summer Time
TimeChangeRule StandardTime = {"GMT", Last, Sun, Oct, 2, 0};         // Greenwich Mean Time

// Set the timezone to allow for automatic DST updates.
Timezone TZ(SummerTime, StandardTime);

time_t closingTime;
unsigned long lastChecked;
static WiFiClientSecure client;

bool spaceStateOpen = false;

LocalNTP ntp;
motor knob(PIN_MOTOR_FWD, PIN_MOTOR_REV);

void connectWifi() 
{
  // Turn on the WiFi and connect using the credentials in the keys file.
  Serial.println("Connecting to '" + String(ssid) + "'");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  // Wait for a connection to be made, then print out the IP address.
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.print("\nConnected to '" + String(ssid) + "' with IP: ");
  Serial.println(WiFi.localIP());
}

void disconnectWifi() 
{
  Serial.println("Disconnecting from Wifi");
  WiFi.disconnect();
}


time_t ntpUpdate()
{
  time_t updateTime = ntp.getNtpTime();
  if (updateTime != 0) {
    setTime(TZ.toLocal(updateTime));
    Serial.println("Current local time is: " + displayTime(hour(), minute()));
    return (updateTime);
  } else {
    return 0;
  }
}

String generateHttpRequest(int hours, time_t closing, String message, bool tweet = false) 
{
  // Write the closingTime out to a string, and submit it to ThingSpeak. If NTP failed, send a message
  // to the 
  String closingTimeString = "#ntpfailure";
  if (closingTime != 0) {
    closingTimeString = displayTime(hour(closing), minute(closing));
  } 
  
  // Construct the actual data to be transmitted - includes the dial number,
  // expected closing time and the message itself.
  String data = String("field1=") + hours +
                       "&field2=" + closingTimeString +
                       "&field3=" + message +
                       "&field4=" + now(); //Second since Jan 1st 1970

  // If tweet string is not empty.
  if (tweet) {
    data = data + "&twitter=" + api_twitter_feed + "&tweet=" + message;
  }

  // Construct the HTTP header to be sent, and append the data.
  return String("POST ") + api_uri + " HTTP/1.1\r\n" +
               "Host: " + api_host + "\r\n" +
               "THINGSPEAKAPIKEY: " + api_key + "\r\n" +
               "Content-Type: application/x-www-form-urlencoded\r\n" +
               "Content-Length:"  + data.length() + "\n\n" + data;

}

String displayTime(short hour, short minute) 
{
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

long generateClosingTime (int hours) 
{
  long closing = now();
  
  int adjustHours = (hours - 1) * 3600;
  int adjustMinutes = 3600 - (60 * (minute() - (minute() - (minute() % 15))));
  closing += adjustHours + adjustMinutes;

  return closing;
  
}

float generateKnobAnalogValue (time_t closing) 
{
  float floatAnalogValue = 0.0f;
  int analogValue = 0;
  if (closing != 0) {
    floatAnalogValue = (closing - now()) / 3600.0f;
    Serial.println (String(closing) + " - " + now() + " = " + (closing - now()));
    Serial.println(floatAnalogValue);
    analogValue = round(floatAnalogValue);
    Serial.println(analogValue);
    analogValue = analogValue * 128 + 72;
  } else {
    analogValue = 72;
  }
  return analogValue;
}

String generateMessage (int hours, time_t closing) {
  String message;
  int randomNumber;
  if (hours == 0) {
    // 0/closed has been selected on the dial. Tweet a "random" closed message.
    randomNumber = random (1, numberClosedStrings);
    message = messageCloseSpace[randomNumber].leader;

    // Check for closingTime having a value - an empty closing time indicates that
    // the time couldnot be retrieved. Append the current time if it's OK.
    if (closing != 0) {
      message = message  + " (It's " + displayTime(hour(), minute()) + ")";
    } else {
      message = message + " #ntpfailure";
    }
    
    return message;
        
  } else {
    
    // Space is open. Tweet a "random" open message.
    randomNumber = random (1, numberOpenStrings);
    // If the hour is singular "1" then we need to use the singular text. Otherwise, add the
    // number and append "hours".
    if (hours == 1) {
      message = messageOpenSpace[randomNumber].leader + messageOpenSpace[randomNumber].singular + messageOpenSpace[randomNumber].punctuation;
    } else {
      message = messageOpenSpace[randomNumber].leader + hours + " hours" + messageOpenSpace[randomNumber].punctuation;
    }

    // Check for empty closing time again, append the time if it's OK, ntpfailure if it's not.
    if (closing != 0) {
      message = message + " (Until ~" + displayTime(hour(closing), minute(closing)) + ")";
    } else {
      message = message + " #ntpfailure";
    }
    
    return message;
    
    
  } // End if hours = 0
  
}

bool moveKnob(int targetPos) {

  int currentPos = analogRead(A0);
  char motorDirection;

  while (!(abs(currentPos - targetPos) <= KNOB_VARIANCE)) {
    // Not there yet. Move that knob!
    if (currentPos > targetPos) {
      knob.moveMotor(COUNTERCLOCKWISE);
      Serial.println("CCW - " + String(currentPos) + "/" + String(targetPos));
    } else {
      knob.moveMotor(CLOCKWISE);
      Serial.println(" CW - " + String(currentPos) + "/" + String(targetPos));
    }
    currentPos = analogRead(A0);
  } 
  knob.moveMotor(STOP);
  
}

bool sendHttpRequest(String httpRequest, int hours) {


    // Try and connect to the API host. Restart at 5x failures.
    int failCount = 0;

    while (!client.connect(api_host, api_port)) {
      failCount++;
      Serial.println("Connection to '"+ String(api_host) + ":" + String(api_port) + "' failed on attempt " + String(failCount) + ". Waiting 1 second to retry.");
      delay(1000);
      if (failCount > 5) {
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
    Serial.print("\n\n");

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
    Serial.print("\n\n");

    // Close the connection, we're done here.
    Serial.println("Closing connection with '" + String(api_host) + "'");
    client.stop();



    return true;

}

bool transmitMessage(int hours) {

 // Write the hours value to the eeprom to allow for recovery should the SSL handshake
 // crash the device (sometimes this happens thanks to low/fragmented memory.)
 Serial.println("Writing check value to EEPROM");
 eepromWriteCheckValue(hours);

     
  if (ntpUpdate()) {
    // Generate the closing time.
    Serial.println("NTP is OK. Generating closing time.");
    closingTime = generateClosingTime(hours);
  } else {
    Serial.println("No NTP.");
    closingTime = 0;
  }

 // Construct a message using the hours data, then generate the HTTP request.
 String message = generateMessage(hours, closingTime);
 String request = generateHttpRequest(hours, closingTime, message, true);

 Serial.println(message);
 Serial.println(request);

 bool messageSent = sendHttpRequest(request, hours);

 // If we've got to here then the message has probably gone through, nothing
 // more we can do on our end.
 if (messageSent) {
  eepromWriteCheckValue(EEPROM_OK);
   Serial.println("Check value cleared from EEPROM.");
   return true;
 } else {
   return false;
 } 
}

void eepromWriteCheckValue(int value) {

  byte lowByte = ((value >> 0) & 0xFF);
  EEPROM.write(EEPROM_ADDRESS, lowByte);
  EEPROM.commit();

}

unsigned int eepromReadCheckValue() {

  byte lowByte = EEPROM.read(EEPROM_ADDRESS);
  return lowByte;

}

bool setSpaceStatus(int hours) {
    // If the hours was 0, disconnect the wifi. Space is closed or NTP failed so not needed anymore.
    if (hours == 0 || closingTime == 0) {
      disconnectWifi();
      spaceStateOpen = false;
    } else {
      spaceStateOpen = true;
    }
    return spaceStateOpen;
}


void setup() 
{

  pinMode(PIN_BUTTON, INPUT);
  pinMode(PIN_MOTOR_FWD, OUTPUT);
  pinMode(PIN_MOTOR_REV, OUTPUT);

  knob.moveMotor(STOP);
  
  EEPROM.begin(128);
  Serial.begin(115200);
  Serial.println("\n\nStarting OpenBot...");

  // Connect to the Wifi & get the time.
  connectWifi();
  
  // Refresh the NTP server details.
  // Get the IP address of the NTP servers.
  IPAddress ntpServerIP;
  Serial.println("Looking up NTP Server (" + String(ntpServerName) + ")");
  bool noIP = false;
  while (!noIP) {
    noIP = WiFi.hostByName(ntpServerName, ntpServerIP);
  }
  
  ntp.setNtpIP(ntpServerIP);

  unsigned int checkValue = eepromReadCheckValue();
  if (checkValue <= 8) {
    
    // The device has rebooted before completing the submission. Is there an internet error?
    Serial.println("Uncompleted submission detected. Resending value '" + String(checkValue) + "'.");
    if (ntpUpdate()) {
      // Generate the closing time.
      Serial.println("NTP is OK. Generating closing time.");
      closingTime = generateClosingTime(checkValue);
    } else {
      Serial.println("No NTP.");
      closingTime = 0;
    }
    
    transmitMessage(checkValue);
    setSpaceStatus(checkValue);
    
  } else {

    // We are just booting normally. Grab the time from NTP to prove the connection works.
    ntpUpdate();
  }
  
  // Disconnect from the wifi.
  disconnectWifi();
  
}

void loop() {

  // Is button pressed?
  if (digitalRead(PIN_BUTTON) == LOW) {
    
    // Yes, button was pressed
    // Read the ADC value, divide into 8 and round to get the number of hours indicated.
    short knobValue = analogRead(A0);
    short hours = floor(knobValue / 128);
    Serial.println("Raw knob value: " + String(knobValue) + "\tHours value: " + hours);

    // Connect to the wifi & update the time from NTP. 
    // This wifi connection will be maintained for hours specified by the knob.
    connectWifi();

    // Transmit the message to Thingspeak & Twitter
    int failCount = 0;
    while (!transmitMessage(hours)) {
      failCount++;
      Serial.println("Couldn't transmit. Disconnecting Wifi, waiting 1 second & trying again.");
      disconnectWifi();
      delay(1000);
      connectWifi();
      if (failCount > 5) {
        ESP.restart();
        // Superstition says that this is required sometimes to make the ESP reboot cleanly.
        delay(1000);
      }
    }

    // Move the knob.
    int requiredKnobValue = generateKnobAnalogValue(closingTime);

    Serial.println("Required Analog Knob Value: " + String(requiredKnobValue));
    
    moveKnob(requiredKnobValue);

    setSpaceStatus(hours);
      
  } else {
    // No, button wasn't pressed.
    if (spaceStateOpen && millis() - lastChecked > 15000) {
      // Move the knob as required.
      int requiredKnobValue = generateKnobAnalogValue(closingTime);
      moveKnob(requiredKnobValue);
      
      // If the time is 15 minutes before the anticipated closing time disconnect the wifi
      // and set the status to closed. This gives time for Marvin (the space controller)
      // to close the space on time - it takes 15 minutes for a wifi device's presence to decay.

      int preClosingMinute = minute(closingTime) - 15;

      if (preClosingMinute < 0) {
        preClosingMinute += 60;
      }
      
      if (round((closingTime - now()) / 3600.0f) == 0 && minute() == preClosingMinute) {
        disconnectWifi();
        spaceStateOpen = false;
        Serial.println("Space Status: Closed");
      }
      lastChecked = millis();
    } // End if space status open     
  }
}

