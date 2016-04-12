#include "motor.h"

motor::motor (char pinClockwise, char pinCounterclockwise) 
{
  clockwisePin = pinClockwise;
  counterclockwisePin = pinCounterclockwise;
}


// TODO: Convert this into an array. 
void motor::moveMotor(char motorDirection) {
  
  char clockwiseState, counterclockwiseState;
  switch (motorDirection) {
    case CLOCKWISE:
      clockwiseState = HIGH;
      counterclockwiseState = LOW;
      break;
    case COUNTERCLOCKWISE:
      clockwiseState = LOW;
      counterclockwiseState = HIGH;
      break;
    case STOP:
      clockwiseState = LOW;
      counterclockwiseState = LOW;
      break;
  }
  digitalWrite(clockwisePin, clockwiseState);
  digitalWrite(counterclockwisePin, counterclockwiseState);

}

