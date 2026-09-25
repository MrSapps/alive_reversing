#pragma once

#include <nlohmann/json_fwd.hpp>

// Light header for (de)serializing a whole AE save - the per object serializers in
// AESaveSerialization.hpp are only needed by AESaveSerialization.cpp and a couple of converters.

struct Quicksave;

void to_json(nlohmann::json& j, const Quicksave& p);
void from_json(const nlohmann::json& j, Quicksave& p);
