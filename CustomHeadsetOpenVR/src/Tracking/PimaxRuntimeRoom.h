#pragma once
#include <nlohmann/json.hpp>
#include <cmath>
#include <cstdint>
#include <istream>
#include <optional>
#include <string>

namespace dreamair {
// PVR FloorLevel applies the inverse standing yaw to pose/linear velocity,
// while SDK 1.33.1 Lighthouse angular velocity stays in the original frame.
inline std::optional<double> ReadPimaxVelocityYaw(std::istream& input,uint64_t universe){
    auto room=nlohmann::json::parse(input,nullptr,false);
    if(room.is_discarded()||!room.is_object()||!room.contains("universes")||!room["universes"].is_array())return {};
    std::optional<double> result;
    for(const auto& entry:room["universes"]){
        if(!entry.is_object()||!entry.contains("universeID"))continue;
        const auto& id=entry["universeID"];
        bool matches=id.is_string()&&id.get<std::string>()==std::to_string(universe);
        if(id.is_number_unsigned())matches=id.get<uint64_t>()==universe;
        else if(id.is_number_integer())matches=id.get<int64_t>()>0&&static_cast<uint64_t>(id.get<int64_t>())==universe;
        if(!matches)continue;
        if(result||!entry.contains("standing")||!entry["standing"].is_object()
           ||!entry["standing"].contains("yaw")||!entry["standing"]["yaw"].is_number())return {};
        double yaw=entry["standing"]["yaw"].get<double>();
        if(!std::isfinite(yaw))return {};
        result=-yaw;
    }
    return result;
}
}
