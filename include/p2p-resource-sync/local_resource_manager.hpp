#pragma once

#include <string>
#include <map>
#include <stdexcept>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <ctime>

namespace p2p {

class ResourceError : public std::runtime_error {
public:
    explicit ResourceError(const std::string& message) : std::runtime_error(message) {}
};

class ResourceNotFoundError : public ResourceError {
public:
    explicit ResourceNotFoundError(const std::string& path) 
        : ResourceError("Resource file not found: " + path) {}
};

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
    
    // Delete copy operations
    LocalResourceManager(const LocalResourceManager&) = delete;
    LocalResourceManager& operator=(const LocalResourceManager&) = delete;

    /**
     * @brief Adds a new resource to the manager
     * @param name Resource name
     * @param path Path to the file
     * @return true if addition was successful, false otherwise
     */
    bool addResource(const std::string& name, const std::string& added_resource_path);

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
     * @brief Gets the file path for a given resource
     * @param name Resource name
     * @return optional containing the path or std::nullopt if resource doesn't exist
     */
    std::optional<std::string> getResourcePath(const std::string& name) const;

private:
    mutable std::shared_mutex mutex_;
    std::map<std::string, p2p::ResourceInfo> resources_;
};

/**
 * @brief Outputs the contents of LocalResourceManager to an output stream
 * @param os Output stream to write to
 * @param manager LocalResourceManager to output
 * @return Reference to the output stream
 */
std::ostream& operator<<(std::ostream& os, const LocalResourceManager& manager);

}
