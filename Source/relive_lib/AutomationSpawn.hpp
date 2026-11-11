#pragma once

#include <string>
#include <vector>

class BaseMap;
class ResourceManager;

// What the automation command spawn can make: the effects and other objects that are made while the game runs,
// so a restore of each can be checked without playing until the game makes one
namespace AutomationSpawn
{
    bool IsKnown(const std::string& name);
    std::vector<std::string> Names();
    // Makes it by the controlled character; false if there is none
    bool Spawn(const std::string& name, ResourceManager& resMan, BaseMap& map);
}
