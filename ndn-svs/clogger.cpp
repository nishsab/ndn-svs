//
// Created by NIshant Sabharwal on 1/15/21.
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdarg>
#include "clogger.h"

clogger* clogger::m_logger = NULL;
std::ofstream clogger::m_logFile;
std::string clogger::instanceName;
bool debug = false;
const std::string DELIM = "\t";

void clogger::startLogger(std::string path, std::string instanceName) {
  if (m_logger != NULL) {
    std::cerr << "logger already started" << std::endl;
    throw -1;
  }
  std::cout << path << std::endl;
  m_logger = new clogger();
  m_logger->m_logFile.open(path);
  m_logger->instanceName = instanceName;
}

clogger * clogger::getLogger() {
  return m_logger;
}

void clogger::log(std::string logType, std::string message) {
  std::string logLine = getLogLine(instanceName, logType, message);
  if (debug) {
    std::cout << logLine << std::endl;
  }
  m_logFile << logLine << std::endl;
}

void clogger::logf(std::string logType, const char * format, ...)
{
  int nLength = 0;
  va_list args;

  va_start(args, format);
  nLength = std::vsnprintf(NULL, 0, format, args) + 1;
  va_end(args);

  char *charBuf = new char[nLength];
  va_start(args, format);
  std::vsprintf(charBuf, format, args);
  va_end(args);

  std::string logLine = getLogLine(instanceName, logType, charBuf);

  if (debug) {
    std::cout << logLine << std::endl;
  }
  m_logFile << logLine << std::endl;

  delete [] charBuf;
}

void clogger::log(std::string logType, ndn::Interest interest) {
  std::string interestName = interest.getName().toUri();
  int size = interest.wireEncode().size();
  logf(logType, "{\"name\": \"%s\", \"size\": %d}", interestName.c_str(), size);
}

void clogger::log(std::string logType, ndn::Data data) {
  std::string interestName = data.getName().toUri();
  int size = data.wireEncode().size();
  logf(logType, "{\"name\": \"%s\", \"size\": %d}", interestName.c_str(), size);
}

std::string clogger::getLogLine(std::string instanceName, std::string logType, std::string message) {
  char timestamp[64];
  struct tm time_buffer;
  time_t tm = time(NULL);
  int len = strftime(timestamp, sizeof(timestamp), "%m/%d %H:%M:%S", localtime_r(&tm, &time_buffer));
  timestamp[len] = 0;

  std::ostringstream stringStream;
  stringStream << timestamp << DELIM << instanceName << DELIM << logType << DELIM << message;
  return stringStream.str();
}