/*
  WeatherSensor - Library for bogus weather data
  Created by Joseph Espy, March 26th, 2020
  Released into the public domain.
*/

#ifndef WeatherSensor_h
#define WeatherSensor_h

struct {
  unsigned char temperature;
  unsigned char pressure;
  unsigned char humidity;
  // 0 for no data, 1 for no rain, 2 for rain
  unsigned char rained;
  // yyyymmddhhmm
  char dateTime[13];
} typedef weatherData_t;

class WeatherSensor {
public:
  WeatherSensor(long seed);
  int readNextHour(weatherData_t *datum);
private:
  void updateTime(char *dateTime);
  long nhours;
  int pressureState;
};

#endif
