#include "LocalNTP.h"

LocalNTP::LocalNTP () { /*  */ }

void LocalNTP::setNtpIP(IPAddress serverIP)
{
  timeServerIP = serverIP;
}

time_t LocalNTP::getNtpTime() {
  
  udp.begin(2390);
  
  while (udp.parsePacket() > 0) ; // discard any previously received packets

  Serial.println("Sending NTP Request");
  
  this->sendNtpPacket();

  uint32_t beginWait = millis();

  while (millis() - beginWait < 5000) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {

      Serial.println("Received NTP Response");

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

      return utc;
    }
  }

  Serial.println("LocalNTP: No response from NTP server (" + String(timeServerIP) + ")");
  return 0;

}

// send an NTP request to the time server at the given address
void LocalNTP::sendNtpPacket() {

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
  udp.beginPacket(timeServerIP, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

