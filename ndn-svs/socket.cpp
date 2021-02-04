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

#include "socket.hpp"

#include <ndn-cxx/util/string-helper.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <clogger.h>

namespace ndn {
namespace svs {

const ndn::Name Socket::DEFAULT_NAME;
const std::shared_ptr<Validator> Socket::DEFAULT_VALIDATOR;

Socket::Socket(const Name& syncPrefix,
               const NodeID& id,
               ndn::Face& face,
               const UpdateCallback& updateCallback,
               const Name& signingId,
               std::shared_ptr<Validator> validator)
  : m_syncPrefix(Name(syncPrefix).append("s"))
  , m_dataPrefix(Name(syncPrefix).append("d"))
  , m_signingId(signingId)
  , m_id(escape(id))
  , m_face(face)
  , m_validator(validator)
  , m_onUpdate(updateCallback)
  , m_logic(face, m_keyChain, m_syncPrefix, updateCallback, m_signingId,
            m_validator, Logic::DEFAULT_ACK_FRESHNESS, m_id)
{
  m_registeredDataPrefix =
    m_face.setInterestFilter(m_dataPrefix,
                             bind(&Socket::onDataInterest, this, _2),
                             [] (const Name& prefix, const std::string& msg) {});
}

Socket::~Socket()
{
}

void
Socket::publishData(const uint8_t* buf, size_t len, const ndn::time::milliseconds& freshness)
{
  publishData(ndn::encoding::makeBinaryBlock(ndn::tlv::Content, buf, len), freshness);
}

void
Socket::publishData(const uint8_t* buf, size_t len, const ndn::time::milliseconds& freshness,
                    const uint64_t& seqNo)
{
  publishData(ndn::encoding::makeBinaryBlock(ndn::tlv::Content, buf, len), freshness, seqNo);
}

void
Socket::publishData(const Block& content, const ndn::time::milliseconds& freshness)
{
  shared_ptr<Data> data = make_shared<Data>();
  data->setContent(content);
  data->setFreshnessPeriod(freshness);

  SeqNo newSeq = m_logic.getSeqNo(m_id) + 1;
  Name dataName(m_dataPrefix);
  dataName.append(m_id).appendNumber(newSeq);
  data->setName(dataName);

  if (m_signingId.empty())
    m_keyChain.sign(*data);
  else
    m_keyChain.sign(*data, signingByIdentity(m_signingId));

  m_ims.insert(*data);
  m_logic.updateSeqNo(newSeq, m_id);
}

void
Socket::publishData(const Block& content, const ndn::time::milliseconds& freshness,
                    const uint64_t& seqNo)
{
  shared_ptr<Data> data = make_shared<Data>();
  data->setContent(content);
  data->setFreshnessPeriod(freshness);

  SeqNo newSeq = seqNo;
  Name dataName(m_dataPrefix);
  dataName.append(m_id).appendNumber(newSeq);
  data->setName(dataName);

  if (m_signingId.empty())
    m_keyChain.sign(*data);
  else
    m_keyChain.sign(*data, signingByIdentity(m_signingId));

  m_ims.insert(*data);
  m_logic.updateSeqNo(newSeq, m_id);
}

void Socket::onDataInterest(const Interest &interest) {
  // If have data, reply. Otherwise forward with probability (?)
  clogger::getLogger()->log("inbound data interest", interest);
  shared_ptr<const Data> data = m_ims.find(interest);
  if (data != nullptr)
  {
    m_face.put(*data);
  }
  else
  {
    // TODO
  }
}

void
Socket::fetchData(const NodeID& nid, const SeqNo& seqNo,
                  const DataValidatedCallback& onValidated,
                  int nRetries)
{
  Name interestName(m_dataPrefix);
  interestName.append(nid).appendNumber(seqNo);

  Interest interest(interestName);
  interest.setMustBeFresh(true);
  interest.setCanBePrefix(false);

  DataValidationErrorCallback onValidationFailed =
    bind(&Socket::onDataValidationFailed, this, _1, _2);
  TimeoutCallback onTimeout =
    [] (const Interest& interest) {};

  clogger::getLogger()->log("outbound data interest", interest);
  m_face.expressInterest(interest,
                         bind(&Socket::onData, this, _1, _2, onValidated, onValidationFailed),
                         bind(&Socket::onDataTimeout, this, _1, nRetries,
                              onValidated, onValidationFailed, onTimeout), // Nack
                         bind(&Socket::onDataTimeout, this, _1, nRetries,
                              onValidated, onValidationFailed, onTimeout));
}

void
Socket::fetchData(const NodeID& nid, const SeqNo& seqNo,
                  const DataValidatedCallback& onValidated,
                  const DataValidationErrorCallback& onValidationFailed,
                  const TimeoutCallback& onTimeout,
                  int nRetries)
{
  Name interestName(m_dataPrefix);
  interestName.append(nid).appendNumber(seqNo);

  Interest interest(interestName);
  interest.setMustBeFresh(true);
  interest.setCanBePrefix(false);

  clogger::getLogger()->log("outbound data interest", interest);
  m_face.expressInterest(interest,
                         bind(&Socket::onData, this, _1, _2, onValidated, onValidationFailed),
                         bind(&Socket::onDataTimeout, this, _1, nRetries,
                              onValidated, onValidationFailed, onTimeout), // Nack
                         bind(&Socket::onDataTimeout, this, _1, nRetries,
                              onValidated, onValidationFailed, onTimeout));
}

void
Socket::onData(const Interest& interest, const Data& data,
               const DataValidatedCallback& onValidated,
               const DataValidationErrorCallback& onFailed)
{
  clogger::getLogger()->log("inbound data packet", data);
  if (static_cast<bool>(m_validator))
    m_validator->validate(data, onValidated, onFailed);
  else
    onValidated(data);
}

void
Socket::onDataTimeout(const Interest& interest, int nRetries,
                      const DataValidatedCallback& dataCallback,
                      const DataValidationErrorCallback& failCallback,
                      const TimeoutCallback& timeoutCallback)
{
  if (nRetries <= 0)
    return timeoutCallback(interest);

  Interest newNonceInterest(interest);
  newNonceInterest.refreshNonce();

  clogger::getLogger()->log("outbound data timeout retry", interest);
  m_face.expressInterest(newNonceInterest,
                         bind(&Socket::onData, this, _1, _2, dataCallback, failCallback),
                         bind(&Socket::onDataTimeout, this, _1, nRetries - 1,
                              dataCallback, failCallback, timeoutCallback), // Nack
                         bind(&Socket::onDataTimeout, this, _1, nRetries - 1,
                              dataCallback, failCallback, timeoutCallback));
}

void
Socket::onDataValidationFailed(const Data& data,
                               const ValidationError& error)
{
}

}  // namespace svs
}  // namespace ndn
