//
// Created by NIshant Sabharwal on 1/15/21.
//

#ifndef NDN_SVS_CLOGGER_H
#define NDN_SVS_CLOGGER_H


#include <string>
#include <ndn-cxx/face.hpp>

class clogger {
public:
    static clogger* getLogger();
    void startLogger(std::string path, std::string instanceName);
    void log(std::string logType, std::string message);
    void log(std::string logType, ndn::Interest interest);
    void log(std::string logType, ndn::Data data);
    void logf(std::string logType, const char * format, ...);

private:
    static clogger *m_logger;
    static std::ofstream m_logFile;
    static std::string instanceName;
    std::string getLogLine(std::string instanceName, std::string logType, std::string message);
};


#endif //NDN_SVS_CLOGGER_H
