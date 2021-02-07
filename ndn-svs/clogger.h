//
// Created by NIshant Sabharwal on 1/15/21.
//

#ifndef NDN_SVS_CLOGGER_H
#define NDN_SVS_CLOGGER_H


#include <string>
#include <ndn-cxx/face.hpp>

typedef enum {
    inbound_sync_interest = 0,
    inbound_sync_ack,
    sync_nack,
    outbound_data_timeout_retry,
    outbound_data_interest,
    inbound_data_packet,
    sync_timeout,
    outbound_sync_ack,
    inbound_data_interest,
    outbound_sync_interest,
    state_vector,
    txn_max
} txn_type_e;

class clogger {
public:
    static clogger* getLogger();
    void startLogger(std::string path, std::string instanceName);
    void log(txn_type_e txnType, std::string message);
    void log(txn_type_e txnType, ndn::Interest interest);
    void log(txn_type_e txnType, ndn::Data data);
    void logf(txn_type_e txnType, const char * format, ...);
    void log_direct(std::string logType, std::string message);
    void stopLogger();

private:
    static clogger *m_logger;
    static std::ofstream m_logFile;
    static std::string instanceName;
    std::string getLogLine(std::string instanceName, std::string logType, std::string message);
};


#endif //NDN_SVS_CLOGGER_H
