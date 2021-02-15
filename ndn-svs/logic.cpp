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

#include "logic.hpp"

#include <ndn-cxx/security/signing-helpers.hpp>
#include <clogger.h>
#include <ndn-cxx/security/verification-helpers.hpp>

namespace ndn {
namespace svs {

int Logic::s_instanceCounter = 0;

const ndn::Name Logic::DEFAULT_NAME;
const NodeID Logic::EMPTY_NODE_ID;
const std::string Logic::DEFAULT_SYNC_KEY;

Logic::Logic(ndn::Face& face,
             ndn::KeyChain& keyChain,
             const Name& syncPrefix,
             const UpdateCallback& onUpdate,
             const std::string& syncKey,
             const Name& signingId,
             const NodeID nid)
  : m_face(face)
  , m_syncPrefix(syncPrefix)
  , m_syncKey(syncKey)
  , m_signingId(signingId)
  , m_id(nid)
  , m_onUpdate(onUpdate)
  , m_rng(ndn::random::getRandomNumberEngine())
  , m_packetDist(10, 15)
  , m_retxDist(30000 * 0.9, 30000 * 1.1)
  , m_intrReplyDist(200 * 0.9, 200 * 1.1)
  , m_keyChain(keyChain)
  , m_keyChainMem("pib-memory:", "tpm-memory:")
  , m_scheduler(m_face.getIoService())
  , m_instanceId(s_instanceCounter++)
{
  m_vv.set(m_id, 0);

  // Use default identity if not specified
  if (m_signingId == Logic::DEFAULT_NAME)
    m_signingId = m_keyChain.getPib().getDefaultIdentity().getName();

  // Register sync interest filter
  m_syncRegisteredPrefix =
    m_face.setInterestFilter(syncPrefix,
                             bind(&Logic::onSyncInterest, this, _2),
                             bind(&Logic::retxSyncInterest, this, true, 0),
                             [] (const Name& prefix, const std::string& msg) {});

  // Setup interest signing
  setSyncKey(m_syncKey);
}

Logic::~Logic()
{
}

void
Logic::onSyncInterest(const Interest &interest)
{
  clogger::getLogger()->log("inbound sync interest", interest);
  if (m_syncKey != Logic::DEFAULT_SYNC_KEY &&
      !security::verifySignature(interest, m_keyChainMem.getTpm(),
                                 m_interestSigningInfo.getSignerName(),
                                 DigestAlgorithm::SHA256))
  {
    return;
  }

  const auto &n = interest.getName();

  // Get state vector
  std::shared_ptr<VersionVector> vvOther;
  try
  {
    vvOther = make_shared<VersionVector>(n.get(-2));
  }
  catch (ndn::tlv::Error&)
  {
    return;
  }

  // Merge state vector
  bool myVectorNew, otherVectorNew;
  std::tie(myVectorNew, otherVectorNew) = mergeStateVector(*vvOther);
  mergeAggregate(*vvOther);
  
  if (myVectorNew)
    {
    setAggregateIfNotRecording(*vvOther);
    int delay = m_intrReplyDist(m_rng);
    if (getCurrentTime() + delay * 1000 < m_nextSyncInterest)
    {
      retxSyncInterest(false, delay);
    }
  }  
  /*
  // If incoming state identical/newer to local vector, reset timer
  // If incoming state is older, send sync interest immediately
  if (!myVectorNew)
  {
    retxSyncInterest(false, 0);
  }
  else
  {
    // Check how much time is left on the timer,
    // reset to ~m_intrReplyDist if more than that.
    int delay = m_intrReplyDist(m_rng);
    if (getCurrentTime() + delay * 1000 < m_nextSyncInterest)
    {
      retxSyncInterest(false, delay);
    }
  }
  */
}

void
Logic::retxSyncInterest(const bool send, unsigned int delay)
{
  if (send) {
    bool myVectorNew, otherVectorNew;
    std::tie(myVectorNew, otherVectorNew) = mergeStateVector(m_aggregatevv);
    if (myVectorNew) {
      sendSyncInterest();
    }
    m_aggregatevv = VersionVector();
    recording = false;
  }

  if (delay == 0)
    delay = m_retxDist(m_rng);

  // Store the scheduled time
  m_nextSyncInterest = getCurrentTime() + 1000 * delay;

  m_retxEvent = m_scheduler.schedule(time::milliseconds(delay),
                                     [this] { retxSyncInterest(true, 0); });
}

void
Logic::sendSyncInterest()
{
  Name syncName(m_syncPrefix);

  {
    std::lock_guard<std::mutex> lock(m_vvMutex);
    syncName.append(Name::Component(m_vv.encode()));
  }

  Interest interest(syncName, time::milliseconds(1000));
  interest.setCanBePrefix(true);
  interest.setMustBeFresh(true);

  m_keyChainMem.sign(interest, m_interestSigningInfo);

  clogger::getLogger()->log("outbound sync interest", interest);
  m_face.expressInterest(interest, nullptr, nullptr, nullptr);
}

std::pair<bool, bool>
Logic::mergeStateVector(const VersionVector &vvOther)
{
  std::lock_guard<std::mutex> lock(m_vvMutex);

  bool myVectorNew = false,
       otherVectorNew = false;

  // New data found in vvOther
  std::vector<MissingDataInfo> v;

  // Check if other vector has newer state
  for (auto entry : vvOther)
  {
    NodeID nidOther = entry.first;
    SeqNo seqOther = entry.second;
    SeqNo seqCurrent = m_vv.get(nidOther);

    if (seqCurrent < seqOther)
    {
      for (unsigned int i=seqCurrent+1; i<=seqOther; i++) {
        std::ostringstream stream;
        stream << nidOther << ":" << seqOther;
        clogger::getLogger()->log("new data", stream.str());
      }
      otherVectorNew = true;

      SeqNo startSeq = m_vv.get(nidOther) + 1;
      v.push_back({nidOther, startSeq, seqOther});

      m_vv.set(nidOther, seqOther);
    }
  }

  // Callback if missing data found
  if (!v.empty())
  {
    m_onUpdate(v);
  }

  // Check if I have newer state
  for (auto entry : m_vv)
  {
    NodeID nid = entry.first;
    SeqNo seq = entry.second;
    SeqNo seqOther = vvOther.get(nid);

    if (seqOther < seq)
    {
      myVectorNew = true;
      break;
    }
  }

  return std::make_pair(myVectorNew, otherVectorNew);
}

void
Logic::reset(bool isOnInterest)
{
}

SeqNo
Logic::getSeqNo(const NodeID& nid) const
{
  std::lock_guard<std::mutex> lock(m_vvMutex);
  NodeID t_nid = (nid == EMPTY_NODE_ID) ? m_id : nid;
  return m_vv.get(t_nid);
}

void
Logic::updateSeqNo(const SeqNo& seq, const NodeID& nid)
{
  NodeID t_nid = (nid == EMPTY_NODE_ID) ? m_id : nid;

  SeqNo prev;
  {
    std::lock_guard<std::mutex> lock(m_vvMutex);
    prev = m_vv.get(t_nid);
    m_vv.set(t_nid, seq);
  }

  if (seq > prev)
    sendSyncInterest();
}

void
Logic::setSyncKey(const std::string key)
{
  m_syncKey = key;
  m_interestSigningInfo.setSigningHmacKey(m_syncKey);
  m_interestSigningInfo.setDigestAlgorithm(DigestAlgorithm::SHA256);
  m_interestSigningInfo.setSignedInterestFormat(security::SignedInterestFormat::V03);
}

std::string
Logic::getSyncKey()
{
  return m_syncKey;
}

std::set<NodeID>
Logic::getSessionNames() const
{
  std::lock_guard<std::mutex> lock(m_vvMutex);
  std::set<NodeID> sessionNames;
  for (const auto& nid : m_vv)
  {
    sessionNames.insert(nid.first);
  }
  return sessionNames;
}

long
Logic::getCurrentTime() const
{
  return std::chrono::duration_cast<std::chrono::microseconds>(
    m_steadyClock.now().time_since_epoch()).count();
}

void
Logic::mergeAggregate(const VersionVector &vvOther)
{
  std::lock_guard<std::mutex> lock(m_vvMutex);

  for (auto entry : vvOther)
  {
    NodeID nidOther = entry.first;
    SeqNo seqOther = entry.second;
    SeqNo seqCurrent = m_aggregatevv.get(nidOther);

    if (seqCurrent < seqOther)
    {
      m_aggregatevv.set(nidOther, seqOther);
    }
  }
}

void
Logic::setAggregateIfNotRecording(const VersionVector &vvOther) {
  std::lock_guard<std::mutex> lock(m_vvMutex);

  if (!recording) {
    recording = true;
    m_aggregatevv = vvOther;
  }
}


}  // namespace svs
}  // namespace ndn
