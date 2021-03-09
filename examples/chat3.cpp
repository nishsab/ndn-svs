/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012-2020 University of California, Los Angeles
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

#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <ndn-svs/socket.hpp>
#include <ndn-cxx/util/random.hpp>
#include <clogger.h>
#include <ndn-svs/socket-shared.hpp>
#include <sys/time.h>
#include <socket-shared.hpp>

class Options
{
public:
  Options() : prefix("/ndn/svs") {}

public:
  std::string prefix;
  std::string m_id;
  int m_stateVectorLogIntervalInMilliseconds = 250;
  int m_start_delay;
  int m_duration;
};

class Program
{
public:
  Program(const Options &options)
    : m_rng(ndn::random::getRandomNumberEngine()),
      m_uniform_mean(50, 500),
      m_uniform_wait(0, 30),
      m_sleepTime(15),
      runTimeInMillseconds(15*60*1000),
    m_options(options)
  {
    instanceName = ndn::Name(m_options.m_id).get(-1).toUri();
    clogger::getLogger()->startLogger("/opt/svs/logs/svs/" + instanceName + ".log", instanceName);
    clogger::getLogger()->logf("startup", "Starting logging for %s", instanceName.c_str());
#if defined(OPTION1_ALL_CHUNKS)
    clogger::getLogger()->log("startup", "option1: all chunks");
#elif defined(OPTION2_JUST_LATEST)
    clogger::getLogger()->log("startup", "option2: just latest");
#elif defined(OPTION3_LATEST_PLUS_RANDOM)
    clogger::getLogger()->log("startup", "option3: latest plus random 1");
#elif defined(OPTION3_LATEST_PLUS_RANDOM3)
    clogger::getLogger()->log("startup", "option3: latest plus random 3");
#elif defined(OPTION4_RANDOM)
    clogger::getLogger()->log("startup", "option4: pure random");
#else
    clogger::getLogger()->log("startup", "option5: no chunks");
#endif

    ndn::svs::SecurityOptions securityOptions;
    securityOptions.interestSigningInfo.setSigningHmacKey("dGhpcyBpcyBhIHNlY3JldCBtZXNzYWdl");

    // Create socket with shared prefix
    auto svs = std::make_shared<ndn::svs::SocketShared>(
      ndn::Name(m_options.prefix),
      //ndn::Name(m_options.m_id).get(-1).toUri(),
      m_options.m_id,
      face,
      std::bind(&Program::onMissingData, this, _1),
      securityOptions);

    // Cache data from all nodes
    svs->setCacheAll(true);
    m_svs = svs;
    
    //std::cout << "SVS client stared:" << m_options.m_id << std::endl;
  }

  long currentTimeMills() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return tp.tv_sec * 1000 + tp.tv_usec / 1000;
  }

  void
  run()
  {
    m_running = true;
    long endTime = currentTimeMills() + runTimeInMillseconds;
    usleep(m_options.m_start_delay * 1000000);
    
    //usleep(m_uniform_wait(m_rng)*1000000);
    std::thread thread_svs([this] { face.processEvents(); });
    std::thread thread_log_state_vector(&Program::logStateVector, this);
    //usleep(m_uniform_wait(m_rng)*1000000);
    usleep(2000000);
    std::string init_msg = "User " + m_options.m_id + " has joined the groupchat";
    publishMsg(init_msg);

    long stopPublishingTime = currentTimeMills() + m_options.m_duration * 1000;

    int i=0;
    while (currentTimeMills() < endTime) {
        std::ostringstream ss = std::ostringstream();
        ss << m_options.m_id << ": message " << i;
        i++;
        std::string message = ss.str();
        //std::cout << "Publishing " << message << std::endl;

        if (currentTimeMills() < stopPublishingTime) {
          publishMsg(message);
          clogger::getLogger()->log("publish", message);
        }
        int sleepTimeInMilliseconds = m_sleepTime(m_rng) * 1000;
        long timeRemaining = endTime - currentTimeMills();
        if (timeRemaining < sleepTimeInMilliseconds) {
          sleepTimeInMilliseconds = timeRemaining;
        }
        usleep(sleepTimeInMilliseconds * 1000);
    }

    m_running = false;
    thread_log_state_vector.join();
    face.shutdown();
    thread_svs.detach();
  }

  void logStateVector() const {
    while (m_running) {
      std::string stateVector = m_svs->getLogic().getStateStr();
      clogger::getLogger()->log("state vector", stateVector);
      usleep(m_options.m_stateVectorLogIntervalInMilliseconds * 1000);
    }
  }

private:
  void
  onMissingData(const std::vector<ndn::svs::MissingDataInfo>& v)
  {
    for (size_t i = 0; i < v.size(); i++)
    {
      for (ndn::svs::SeqNo s = v[i].low; s <= v[i].high; ++s)
      {
        ndn::svs::NodeID nid = v[i].session;
        m_svs->fetchData(nid, s, [nid] (const ndn::Data& data)
          {
            size_t data_size = data.getContent().value_size();
            std::string content_str((char *)data.getContent().value(), data_size);
            content_str = nid + " : " + content_str;
            //std::cout << content_str << std::endl;
          });
      }
    }
  }

  void
  publishMsg(std::string msg)
  {
    m_svs->publishData(reinterpret_cast<const uint8_t*>(msg.c_str()),
                       msg.size(),
                       ndn::time::milliseconds(1000));
  }

  std::string instanceName;
  volatile bool m_running = false;
  ndn::random::RandomNumberEngine& m_rng;
  std::uniform_int_distribution<> m_uniform_mean;
  std::uniform_int_distribution<> m_uniform_wait;
  std::poisson_distribution<> m_sleepTime;
  long runTimeInMillseconds;



public:
  const Options m_options;
  ndn::Face face;
  std::shared_ptr<ndn::svs::SocketBase> m_svs;
};

int
main(int argc, char **argv)
{
  if (argc != 4) {
    std::cout << "Usage: client start duration" << std::endl;
    exit(1);
  }

  Options opt;
  opt.m_id = argv[1];
  opt.m_start_delay = atoi(argv[2]);
  opt.m_duration = atoi(argv[3]);
  
  Program program(opt);
  program.run();
  return 0;
}
