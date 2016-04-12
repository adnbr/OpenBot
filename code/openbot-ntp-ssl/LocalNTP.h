


#ifndef LocalNTP_h
#define LocalNTP_h

  #include <Arduino.h>
  #include <WiFiudp.h>
  #include <TimeLib.h>
  #include <Timezone.h>    

  // Port to listen on for UDP packets
  const int NTP_PACKET_SIZE = 48;

  class LocalNTP {
    public:
      LocalNTP();
      void setNtpIP(IPAddress serverIP);
      time_t getNtpTime();
    private:
      void sendNtpPacket();
      WiFiUDP udp;
      IPAddress timeServerIP;
      
      byte packetBuffer[NTP_PACKET_SIZE];
  };

#endif


