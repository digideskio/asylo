/*
 *
 * Copyright 2017 Asylo authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string>

#include "asylo/platform/arch/include/trusted/host_calls.h"
#include "asylo/platform/posix/sockets/socket_test.pb.h"
#include "asylo/test/util/enclave_test_application.h"

namespace asylo {

class AddrinfoTestEnclave : public EnclaveTestCase {
 public:
  AddrinfoTestEnclave() = default;

  Status Run(const EnclaveInput &input, EnclaveOutput *output) {
    if (!input.HasExtension(addrinfo_test_input)) {
      return Status(error::GoogleError::INVALID_ARGUMENT,
                    "addrinfo test input use addrinfo hints not found");
    }

    bool use_addrinfo_hints =
        input.GetExtension(addrinfo_test_input).use_addrinfo_hints();
    if (use_addrinfo_hints) {
      return AddrInfoTest_Hints();
    } else {
      return AddrInfoTest_NoHints();
    }
  }

 private:
  bool VerifyLocalHostAddrInfoCanonname(const struct addrinfo *info) {
    for (const struct addrinfo *i = info; i != nullptr; i = i->ai_next) {
      // Check addrinfo canonname contains string "localhost"
      if (!strstr(i->ai_canonname, "localhost")) return false;
    }
    return true;
  }

  bool VerifyLocalHostAddrInfoAddress(const struct addrinfo *info) {
    char addr_buf[INET_ADDRSTRLEN];
    for (const struct addrinfo *i = info; i != nullptr; i = i->ai_next) {
      memset(addr_buf, 0, sizeof(addr_buf));
      // This test only checks the addrinfos returned by getaddrinfo for IPv4
      // and IPv6. We ignore other address families.
      if (i->ai_family == AF_INET6) {
        struct in6_addr *sin_addr6 =
            &(reinterpret_cast<struct sockaddr_in6 *>(i->ai_addr)->sin6_addr);
        inet_ntop(i->ai_family, sin_addr6, addr_buf, sizeof(addr_buf));
        if (strcmp(addr_buf, "::1") != 0) return false;
      } else if (i->ai_family == AF_INET) {
        struct in_addr *sin_addr =
            &(reinterpret_cast<struct sockaddr_in *>(i->ai_addr)->sin_addr);
        inet_ntop(i->ai_family, sin_addr, addr_buf, sizeof(addr_buf));
        if (strcmp(addr_buf, "127.0.0.1") != 0) return false;
      }
    }
    return true;
  }

  bool GetAddrInfoForLocalHost(const struct addrinfo *hints,
                               struct addrinfo **res) {
    return getaddrinfo("localhost", nullptr, hints, res) == 0;
  }

  Status AddrInfoTest_NoHints() {
    struct addrinfo *info = nullptr;
    if (!GetAddrInfoForLocalHost(/*hints=*/nullptr, &info)) {
      return Status(error::GoogleError::INTERNAL,
                    "getaddrinfo() system call failed");
    }
    if (!VerifyLocalHostAddrInfoAddress(info)) {
      return Status(error::GoogleError::INTERNAL,
                    "getaddrinfo() returned incorrect address string");
    }
    freeaddrinfo(info);
    return Status::OkStatus();
  }

  Status AddrInfoTest_Hints() {
    struct addrinfo *info = nullptr;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET | AF_INET6;  // limit to IPv4 and IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;  // return canonical names in addrinfo
    if (!GetAddrInfoForLocalHost(&hints, &info)) {
      return Status(error::GoogleError::INTERNAL,
                    "getaddrinfo() system call failed");
    }
    if (!VerifyLocalHostAddrInfoAddress(info)) {
      return Status(error::GoogleError::INTERNAL,
                    "getaddrinfo() returned incorrect address string");
    }
    if (!VerifyLocalHostAddrInfoCanonname(info)) {
      return Status(error::GoogleError::INTERNAL,
                    "getaddrinfo() returned incorrect canonical name");
    }
    freeaddrinfo(info);
    return Status::OkStatus();
  }
};

TrustedApplication *BuildTrustedApplication() {
  return new AddrinfoTestEnclave;
}

}  // namespace asylo
