#include <Arduino.h>
#include "UdawaLogger.h"
#include "UdawaSerialLogger.h"
#include "Udawa.h"

UdawaLogger *logger = UdawaLogger::getInstance(LogLevel::VERBOSE);
UdawaSerialLogger *serialLogger = UdawaSerialLogger::getInstance(SERIAL_BAUD_RATE);
Udawa udawa;

void setup() {
  logger->addLogger(serialLogger);
  logger->setLogLevel(LogLevel::VERBOSE);
  udawa.begin();
}

void loop() {
  udawa.run();
  //logger->debug(PSTR(__func__), PSTR("%d\n"), millis());
}