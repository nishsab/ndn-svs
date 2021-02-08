//
// Created by NIshant Sabharwal on 1/15/21.
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdarg>
#include <mutex> 
#include "clogger.h"

clogger* clogger::m_logger = NULL;
std::ofstream clogger::m_logFile;
std::string clogger::instanceName;
bool debug = false;
const std::string DELIM = "\t";
std::mutex lock;

bool writeLogs = false;
int counts[txn_max] = {0};
std::string names[txn_max] = {
        "inbound sync interest",
        "inbound sync ack",
        "sync nack",
        "outbound data timeout retry",
        "outbound data interest",
        "inbound data packet",
        "sync timeout",
        "outbound sync ack",
        "inbound data interest",
        "outbound sync interest"
};

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

void clogger::log(txn_type_e txnType, std::string message) {
  std::string logType = names[txnType];
  if (writeLogs) {
    std::string logLine = getLogLine(instanceName, logType, message);
    if (debug) {
      std::cout << logLine << std::endl;
    }
    lock.lock();
    counts[txnType] += 1;
    m_logFile << logLine << std::endl;
    lock.unlock();
  }
  else {
    lock.lock();
    counts[txnType] += 1;
    lock.unlock();
  }

}

void clogger::logf(txn_type_e txnType, const char * format, ...)
{
  std::string logType = names[txnType];
  if (writeLogs) {
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
    lock.lock();
    m_logFile << logLine << std::endl;
    lock.unlock();

    delete[] charBuf;
  }
  else {
    lock.lock();
    counts[txnType] += 1;
    lock.unlock();
  }
}

void clogger::log(txn_type_e txnType, ndn::Interest interest) {
  if (writeLogs) {
    std::string interestName = interest.getName().toUri();
    int size = interest.wireEncode().size();
    logf(txnType, "{\"name\": \"%s\", \"size\": %d}", interestName.c_str(), size);
  }
  else {
    lock.lock();
    counts[txnType] += 1;
    lock.unlock();
  }

}

void clogger::log(txn_type_e txnType, ndn::Data data) {
  if (writeLogs) {
    std::string interestName = data.getName().toUri();
    int size = data.wireEncode().size();
    logf(txnType, "{\"name\": \"%s\", \"size\": %d}", interestName.c_str(), size);
  }
  else {
    lock.lock();
    counts[txnType] += 1;
    lock.unlock();
  }
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

void clogger::stopLogger() {
  std::ostringstream stringStream;
  for (int txn_type = inbound_sync_interest; txn_type < txn_max; txn_type++){
    stringStream << names[txn_type] << ":" << counts[txn_type];
    if (txn_type < txn_max-1) {
      stringStream << ",";
    }
  }
  lock.lock();
  m_logFile << stringStream.str() << std::endl;
  lock.unlock();
}

void clogger::log_direct(std::string logType, std::string message) {
  std::string logLine = getLogLine(instanceName, logType, message);
  if (debug) {
    std::cout << logLine << std::endl;
  }
  lock.lock();
  m_logFile << logLine << std::endl;
  lock.unlock();
}
