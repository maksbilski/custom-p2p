#include <filesystem>
#include <p2p-resource-sync/local_resource_manager.hpp>
#include <p2p-resource-sync/constants.hpp>

namespace p2p {
  bool LocalResourceManager::addResource(const std::string &new_resource_name,
                                         const std::string &new_resource_path) {
    if (!std::filesystem::exists(new_resource_path)) {
      throw ResourceNotFoundError(new_resource_path);
    }

    if (new_resource_name.length() > constants::local_resource_manager::MAX_RESOURCE_NAME_LENGTH) {
      throw ResourceError("Resource name exceeds maximum length of " +
                         std::to_string(constants::local_resource_manager::MAX_RESOURCE_NAME_LENGTH));
    }

    if (new_resource_path.length() > constants::local_resource_manager::MAX_RESOURCE_PATH_LENGTH) {
      throw ResourceError("Resource path exceeds maximum length of " +
                         std::to_string(constants::local_resource_manager::MAX_RESOURCE_PATH_LENGTH));
    }

    auto file_size = std::filesystem::file_size(new_resource_path);
    if (file_size > constants::local_resource_manager::MAX_RESOURCE_SIZE) {
      throw ResourceError("Resource size exceeds maximum allowed size of " +
                         std::to_string(constants::local_resource_manager::MAX_RESOURCE_SIZE) + " bytes");
    }

    std::unique_lock lock(mutex_);

    if (resources_.size() >= constants::local_resource_manager::MAX_RESOURCES &&
        resources_.find(new_resource_name) == resources_.end()) {
      throw ResourceError("Maximum number of resources (" +
                         std::to_string(constants::local_resource_manager::MAX_RESOURCES) + ") reached");
        }

    ResourceInfo resource_info{
      .name = new_resource_name,
      .path = new_resource_path,
      .size = file_size,
      .lastModified = std::time(nullptr)
    };

    return resources_.insert_or_assign(new_resource_name, resource_info).second;
  };

std::optional<std::string>
LocalResourceManager::getResourcePath(const std::string &name) const {
  std::shared_lock lock(mutex_);

  if (auto it = resources_.find(name); it != resources_.end()) {
    return it->second.path;
  }
  return std::nullopt;
};

bool LocalResourceManager::removeResource(const std::string &name) {
  std::unique_lock lock(mutex_);

  auto it = resources_.find(name);
  if (it != resources_.end()) {
    return resources_.erase(name);
  }
  return false;
};

std::map<std::string, ResourceInfo>
LocalResourceManager::getAllResources() const {
  std::shared_lock lock(mutex_);

  return resources_;
};

std::optional<ResourceInfo>
LocalResourceManager::getResourceInfo(const std::string &name) const {
  std::shared_lock lock(mutex_);

  auto it = resources_.find(name);
  if (it != resources_.end()) {
    return it->second;
  }
  return std::nullopt;
};

std::ostream &operator<<(std::ostream &os,
                         const LocalResourceManager &manager) {
  os << "LocalResourceManager{\n";
  const auto resources = manager.getAllResources();
  for (const auto &[name, info] : resources) {
    os << "  " << name << ": path='" << info.path << "', size=" << info.size
       << ", lastModified=" << info.lastModified << "\n";
  }
  os << "}";
  return os;
}

} // namespace p2p
