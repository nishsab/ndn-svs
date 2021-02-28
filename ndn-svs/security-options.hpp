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

#ifndef NDN_SVS_SIGNING_OPTIONS_HPP
#define NDN_SVS_SIGNING_OPTIONS_HPP

#include "common.hpp"

namespace ndn {
namespace svs {

struct SecurityOptions
{
  /** Signing options for sync interests */
  security::SigningInfo interestSigningInfo;
  /** Signing options for data packets */
  security::SigningInfo dataSigningInfo;

  /** Validator to validate data and interests (unless using HMAC) */
  const std::shared_ptr<Validator> validator = DEFAULT_VALIDATOR;

  static const SecurityOptions DEFAULT;
  static const std::shared_ptr<Validator> DEFAULT_VALIDATOR;

  SecurityOptions()
  {
    // Set defaults
    interestSigningInfo.setSignedInterestFormat(security::SignedInterestFormat::V03);
  }
};

}  // namespace svs
}  // namespace ndn

#endif // NDN_SVS_SIGNING_OPTIONS_HPP