#pragma once

#include <string>
#include <map>
#include <shared_mutex>
#include <optional>
#include <ctime>

namespace p2p {

/**
 * @brief Structure storing information about a local resource
 */
struct ResourceInfo {
    std::string name;
    std::string path;
    std::size_t size;
    std::time_t lastModified;
};

/**
 * @brief Class managing local node resources
 * 
 * Provides thread-safe access to information about locally available resources.
 * Implements the monitor pattern.
 */
class LocalResourceManager {
public:
    LocalResourceManager() = default;
    ~LocalResourceManager() = default;
    
    // Delete copy operations, allow moving
    LocalResourceManager(const LocalResourceManager&) = delete;
    LocalResourceManager& operator=(const LocalResourceManager&) = delete;
    LocalResourceManager(LocalResourceManager&&) = default;
    LocalResourceManager& operator=(LocalResourceManager&&) = default;

    /**
     * @brief Adds a new resource to the manager
     * @param name Resource name
     * @param path Path to the file
     * @return true if addition was successful, false otherwise
     */
    bool addResource(const std::string& name, const std::string& path);

    /**
     * @brief Removes a resource from the manager
     * @param name Name of the resource to remove
     * @return true if resource was removed, false if it didn't exist
     */
    bool removeResource(const std::string& name);

    /**
     * @brief Gets information about a specific resource
     * @param name Resource name
     * @return optional containing resource information or std::nullopt if it doesn't exist
     */
    std::optional<ResourceInfo> getResourceInfo(const std::string& name) const;

    /**
     * @brief Gets a list of all available resources
     * @return Map of name -> resource information
     */
    std::map<std::string, ResourceInfo> getAllResources() const;

    /**
     * @brief Checks if a resource exists
     * @param name Resource name
     * @return true if resource exists, false otherwise
     */
    bool hasResource(const std::string& name) const;

    /**
     * @brief Gets the file path for a given resource
     * @param name Resource name
     * @return optional containing the path or std::nullopt if resource doesn't exist
     */
    std::optional<std::string> getResourcePath(const std::string& name) const;

private:
    mutable std::shared_mutex mutex_;
    std::map<std::string, ResourceInfo> resources_;
};

}
