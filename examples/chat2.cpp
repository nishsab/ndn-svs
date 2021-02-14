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

class Options
{
public:
  Options() : prefix("/ndn/svs") {}

public:
  std::string prefix;
  std::string m_id;
  int m_stateVectorLogIntervalInMilliseconds = 1000;
  int averageTimeBetweenPublishesInMilliseconds = 30000;
  int varianceInTimeBetweenPublishesInMilliseconds = 5000;
};

class Program
{
public:
  Program(const Options &options)
    : m_rng(ndn::random::getRandomNumberEngine()),
      m_sleepTime(options.averageTimeBetweenPublishesInMilliseconds - options.varianceInTimeBetweenPublishesInMilliseconds, options.averageTimeBetweenPublishesInMilliseconds + options.varianceInTimeBetweenPublishesInMilliseconds),
    m_options(options)
  {
    instanceName = ndn::Name(m_options.m_id).get(-1).toUri();
    clogger::getLogger()->startLogger("/opt/svs/logs/svs/" + instanceName + ".log", instanceName);
    clogger::getLogger()->logf("startup", "Starting logging for %s", instanceName.c_str());

    m_svs = std::make_shared<ndn::svs::Socket>(
      ndn::Name(m_options.prefix),
      instanceName,
      face,
      std::bind(&Program::onMissingData, this, _1),
      "dGhpcyBpcyBhIHNlY3JldCBtZXNzYWdl",
      ndn::Name(m_options.m_id));

    //std::cout << "SVS client stared:" << m_options.m_id << std::endl;
  }

  void
  run()
  {
    m_running = true;
    std::thread thread_svs([this] { face.processEvents(); });
    std::thread thread_log_state_vector(&Program::logStateVector, this);

    std::string init_msg = "User " + m_options.m_id + " has joined the groupchat";
    publishMsg(init_msg);

    std::string userInput = "";

    for (int i=0; i<24; i++) {
        std::ostringstream ss = std::ostringstream();
        ss << m_options.m_id << ": message " << i;
        std::string message = ss.str();
        //std::cout << "Publishing " << message << std::endl;

        publishMsg(message);
        int sleepTimeInMilliseconds = m_sleepTime(m_rng);
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

  int getRandomIntAroundCenter(int center, int interval) {
    int delta = rand() % (2*interval + 1);
    return center - interval + delta;
  }

  std::string instanceName;
  volatile bool m_running = false;
  ndn::random::RandomNumberEngine& m_rng;
  std::uniform_int_distribution<> m_sleepTime;



public:
  const Options m_options;
  ndn::Face face;
  std::shared_ptr<ndn::svs::Socket> m_svs;
};

int
main(int argc, char **argv)
{
  if (argc != 2) {
    std::cout << "Usage: client <prefix>" << std::endl;
    exit(1);
  }

  Options opt;
  opt.m_id = argv[1];

  Program program(opt);
  program.run();
  return 0;
}
