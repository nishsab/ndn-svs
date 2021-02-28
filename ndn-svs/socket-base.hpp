/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012-2021 University of California, Los Angeles
 *
 * This file is part of ndn-svs, synchronization library for distributed realtime
 * applications for NDN.
 *
 * ndn-svs library is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free Software
 * Foundation, in version 2.1 of the License.
 *
 * ndn-svs library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
 */

#ifndef NDN_SVS_SOCKET_BASE_HPP
#define NDN_SVS_SOCKET_BASE_HPP

#include "common.hpp"
#include "logic.hpp"
#include "store.hpp"
#include "security-options.hpp"

namespace ndn {
namespace svs {

/**
 * @brief A simple interface to interact with client code
 *
 * Though it is called Socket, it is not a real socket. It just trying to provide
 * a simplified interface for data publishing and fetching.
 *
 * This interface simplify data publishing.  Client can simply dump raw data
 * into this interface without handling the SVS specific details, such
 * as sequence number and session id.
 *
 * The socket will throw if signingId does not exist.
 *
 * This interface also simplifies data fetching.  Client only needs to provide a
 * data fetching strategy (through a updateCallback).
 *
 * @param syncPrefix The prefix of the sync group
 * @param dataPrefix The prefix to listen for data on
 * @param id ID for the node
 * @param face The face used to communication
 * @param updateCallback The callback function to handle state updates
 * @param securityOptions Signing and validation options for interests and data
 * @param dataStore Interface to store data packets
 */
class SocketBase : noncopyable
{
public:
  SocketBase(const Name& syncPrefix,
             const Name& dataPrefix,
             const NodeID& id,
             ndn::Face& face,
             const UpdateCallback& updateCallback,
             const SecurityOptions& securityOptions = SecurityOptions::DEFAULT,
             std::shared_ptr<DataStore> dataStore = DEFAULT_DATASTORE);

  virtual ~SocketBase() = default;

  using DataValidatedCallback = function<void(const Data&)>;

  using DataValidationErrorCallback = function<void(const Data&, const ValidationError& error)> ;

  /**
   * @brief Publish a data packet in the session and trigger synchronization updates
   *
   * This method will create a data packet with the supplied content.
   * The packet name is the local session + seqNo.
   * The seqNo is set by the application.
   *
   * @param buf Pointer to the bytes in content
   * @param len size of the bytes in content
   * @param freshness FreshnessPeriod of the data packet.
   * @param id NodeID to publish the data under
   */
  void
  publishData(const uint8_t* buf, size_t len, const ndn::time::milliseconds& freshness,
              const NodeID id = EMPTY_NODE_ID);

  /**
   * @brief Publish a data packet in the session and trigger synchronization updates
   *
   * This method will create a data packet with the supplied content.
   * The packet name is the local session + seqNo.
   * The seqNo is set by the application.
   *
   * @param content Block that will be set as the content of the data packet.
   * @param freshness FreshnessPeriod of the data packet.
   * @param id NodeID to publish the data under
   */
  void
  publishData(const Block& content, const ndn::time::milliseconds& freshness,
              const NodeID id = EMPTY_NODE_ID);

  /**
   * @brief Retrive a data packet with a particular seqNo from a session
   *
   * @param sessionName The name of the target session.
   * @param seq The seqNo of the data packet.
   * @param onValidated The callback when the retrieved packet has been validated.
   * @param nRetries The number of retries.
   */
  void
  fetchData(const NodeID& nid, const SeqNo& seq,
            const DataValidatedCallback& onValidated,
            int nRetries = 0);

  /**
   * @brief Retrive a data packet with a particular seqNo from a session
   *
   * @param sessionName The name of the target session.
   * @param seq The seqNo of the data packet.
   * @param onValidated The callback when the retrieved packet has been validated.
   * @param onValidationFailed The callback when the retrieved packet failed validation.
   * @param onTimeout The callback when data is not retrieved.
   * @param nRetries The number of retries.
   */
  void
  fetchData(const NodeID& nid, const SeqNo& seq,
            const DataValidatedCallback& onValidated,
            const DataValidationErrorCallback& onValidationFailed,
            const TimeoutCallback& onTimeout,
            int nRetries = 0);

  /**
   * @brief Return data name for a given packet
   *
   * The derived Socket class must provide implementation. Note that
   * the data name for the own node MUST be under the regtistered
   * data prefix for proper functionality, or the application must
   * independently produce data under the prefix.
   */
  virtual Name
  getDataName(const NodeID& nid, const SeqNo& seqNo) = 0;

  /*** @brief Get the underlying data store */
  DataStore&
  getDataStore()
  {
    return *m_dataStore;
  }

  /*** @brief Get the underlying SVS logic */
  Logic&
  getLogic()
  {
    return m_logic;
  }

public:
  static const NodeID EMPTY_NODE_ID;
  static const std::shared_ptr<DataStore> DEFAULT_DATASTORE;

private:
  void
  onDataInterest(const Interest &interest);

  void
  onData(const Interest& interest, const Data& data,
         const DataValidatedCallback& dataCallback,
         const DataValidationErrorCallback& failCallback);

  void
  onDataTimeout(const Interest& interest, int nRetries,
                const DataValidatedCallback& dataCallback,
                const DataValidationErrorCallback& failCallback,
                const TimeoutCallback& timeoutCallback);

  void
  onDataValidated(const Data& data,
                  const DataValidatedCallback& dataCallback);

  void
  onDataValidationFailed(const Data& data,
                         const ValidationError& error);

  /**
   * Determines whether a particular data packet is to be cached
   * Can be used to cache data packets from other nodes when
   * using multicast data interests.
   */
  virtual bool
  shouldCache(const Data& data)
  {
      return false;
  }

protected:
  const Name m_syncPrefix;
  const Name m_dataPrefix;
  const SecurityOptions m_securityOptions;
  const NodeID m_id;

private:
  Face& m_face;
  KeyChain m_keyChain;

  ndn::ScopedRegisteredPrefixHandle m_registeredDataPrefix;

  const UpdateCallback m_onUpdate;

  std::shared_ptr<DataStore> m_dataStore;

  Logic m_logic;
};

}  // namespace svs
}  // namespace ndn

#endif // NDN_SVS_SOCKET_BASE_HPP
