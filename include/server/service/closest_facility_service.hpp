/*

Copyright (c) 2025, Project OSRM contributors
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef SERVER_SERVICE_CLOSEST_FACILITY_SERVICE_HPP
#define SERVER_SERVICE_CLOSEST_FACILITY_SERVICE_HPP

#include "server/service/base_service.hpp"

#include "engine/status.hpp"
#include "osrm/osrm.hpp"
#include "util/coordinate.hpp"

#include <string>

namespace osrm::server::service
{

class ClosestFacilityService final : public BaseService
{
  public:
    ClosestFacilityService(OSRM &routing_machine) : BaseService(routing_machine) {}

    engine::Status RunQuery(std::size_t prefix_length,
                           std::string &query,
                           osrm::engine::api::ResultT &result) final override;

    // New: Handle POST requests with JSON body for bulk operations
    engine::Status RunQueryJSON(const std::string &json_body,
                               osrm::engine::api::ResultT &result);

    unsigned GetVersion() final override { return 1; }
};

} // namespace osrm::server::service

#endif // SERVER_SERVICE_CLOSEST_FACILITY_SERVICE_HPP
