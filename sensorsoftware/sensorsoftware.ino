#include "src/WeatherSensor/WeatherSensor.h"
WeatherSensor ws(17695222l);

/*
 * - BLink is sent with an argument, and blinks that many times quickly
 * - Request sends a 16 byte response to the user
 * - Pause stops any LED blinking
 * - Resume unpauses system
 */
enum message {
    BLINK   = '1',
    REQUEST = '2',
    PAUSE   = '3',
    RESUME  = '4',
};

bool isPaused = false;
bool hasSentBlink = false;
bool hasDeadline = false;
int fastBlinksLeft = 0;
unsigned long blinkDeadline;
int ledState = LOW;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT); 
  digitalWrite(LED_BUILTIN, LOW); 
  Serial.begin(9600);
}

unsigned long umillis() {return (unsigned long) millis();}

void flipLED() {
  ledState = !ledState;
  digitalWrite(LED_BUILTIN, ledState);
}

// write a 16 byte message back over serial
void handleRequest() {
  weatherData_t datum;
  ws.readNextHour(&datum);
  // note not null terminated
  Serial.write((char *)&datum, 16);
  if (!hasDeadline) {
    flipLED();
    blinkDeadline = umillis() + 500;
    hasDeadline = true;
  }
}

// parse an incomming message and act accordingly
void handleCommand() {
      switch (Serial.read()) {
        case BLINK:
          hasSentBlink = true;
          break;
        case REQUEST:
          handleRequest();
          break;
        case PAUSE:
          isPaused = true;
          break;
        case RESUME:
          isPaused = false;
          break;
    }
}

void handleBlinkState(){
  if (isPaused) {
    ledState = LOW;
    digitalWrite(LED_BUILTIN, ledState);
    hasDeadline = false;
    return;
  }
  if (hasDeadline && blinkDeadline <= umillis()) {
    flipLED();
    if (fastBlinksLeft) {
      fastBlinksLeft -= 1;
      blinkDeadline = umillis() + 200;
    } else {
      hasDeadline = false;
    }
  }
}

void loop() {
  handleBlinkState();
  
  // if there is nothing to read, restart loop
  if (Serial.available() == 0) {
    return;
  }
  
  if (hasSentBlink) {
    fastBlinksLeft = Serial.read() * 2 - 1;
    hasSentBlink = false;
    hasDeadline = true;
  } else {
    handleCommand();
  }
} 
