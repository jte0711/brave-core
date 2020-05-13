/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/confirmations/internal/logging_util.h"

#include "base/strings/stringprintf.h"

namespace confirmations {

std::string ToString(
    const std::string& url,
    const int response_status_code,
    const std::string& response,
    const std::map<std::string, std::string>& headers) {
  std::string formatted_headers;
  for (const auto& header : headers) {
    formatted_headers += "    ";
    formatted_headers += header.first;
    formatted_headers += ": ";
    formatted_headers += header.second;
    formatted_headers += "\n";
  }

  return base::StringPrintf("URL Response:\n  URL: %s\n  Response "
      "Status Code: %d\n  Response: %s\n  Headers: %s\n", url.c_str(),
          response_status_code, response.c_str(), formatted_headers.c_str());
}

}  // namespace confirmations