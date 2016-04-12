#ifndef motor_h
#define motor_h

  #include <Arduino.h>
  
  #define CLOCKWISE                 0
  #define COUNTERCLOCKWISE          1
  #define STOP                      2
  
  class motor {
    public:
      motor(char pinClockwise, char pinCounterclockwise);
      void moveMotor(char motorDirection);
    private:
      char clockwisePin, counterclockwisePin;
  };

#endif


