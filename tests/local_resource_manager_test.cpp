#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "p2p-resource-sync/local_resource_manager.hpp"

class LocalResourceManagerTest : public ::testing::Test {
protected:
    std::string createTempFile(const std::string& path, const std::string& content = "test content") {
        auto temp_path = std::filesystem::temp_directory_path() / path;
        std::ofstream file(temp_path);
        file << content;
        file.close();
        return temp_path.string();
    }
    
    void removeTempFile(const std::string& path) {
        std::filesystem::remove(path);
    }
};


TEST_F(LocalResourceManagerTest, CanBeInstantiated) {
    EXPECT_NO_THROW({
        p2p::LocalResourceManager manager;
    });
}


TEST_F(LocalResourceManagerTest, CannotBeCopied) {
    p2p::LocalResourceManager manager1;
    
    EXPECT_FALSE(std::is_copy_constructible_v<p2p::LocalResourceManager>);
    EXPECT_FALSE(std::is_copy_assignable_v<p2p::LocalResourceManager>);
}


TEST_F(LocalResourceManagerTest, AddResourceInsertSuccessful) {
    p2p::LocalResourceManager manager1;
    std::string temp_path = createTempFile("some_path");

    EXPECT_TRUE(manager1.addResource(std::string("some_name"), temp_path));

    removeTempFile(temp_path);
}

TEST_F(LocalResourceManagerTest, AddResourceInsertThenUpdateSuccessful) {
    p2p::LocalResourceManager manager1;
    std::string temp_path = createTempFile("some_path");

    EXPECT_TRUE(manager1.addResource(std::string("some_name"), temp_path));

    EXPECT_FALSE(manager1.addResource(std::string("some_name"), temp_path));

    removeTempFile(temp_path);
}

TEST_F(LocalResourceManagerTest, AddResourceFileDoesNotExist) {
    p2p::LocalResourceManager manager1;
    EXPECT_THROW(
        manager1.addResource(std::string("some_name"), std::string("some_path")),
        p2p::ResourceNotFoundError
    );
}

TEST_F(LocalResourceManagerTest, RemoveResourceSuccessful) {
    p2p::LocalResourceManager manager1;
    std::string temp_path = createTempFile("some_path");

    EXPECT_TRUE(manager1.addResource(std::string("some_name"), temp_path));
    EXPECT_TRUE(manager1.removeResource(std::string("some_name")));

    EXPECT_FALSE(manager1.getResourcePath(std::string("some_name")));
    removeTempFile(temp_path);
}


TEST_F(LocalResourceManagerTest, RemoveResourceFileDoesNotExist) {
    p2p::LocalResourceManager manager1;

    EXPECT_FALSE(manager1.removeResource(std::string("some_name")));
}


TEST_F(LocalResourceManagerTest, GetResourcePathResourceDoesExist) {
    p2p::LocalResourceManager manager1;
    std::string temp_path1 = createTempFile("some_path1");

    manager1.addResource(std::string("some_name1"), temp_path1);

    ASSERT_EQ(manager1.getResourcePath(std::string("some_name1")), std::string(temp_path1));

    removeTempFile(temp_path1);
}


TEST_F(LocalResourceManagerTest, GetResourcePathResourceDoesNotExist) {
    p2p::LocalResourceManager manager1;

    ASSERT_EQ(manager1.getResourcePath(std::string("some_name")), std::nullopt);
}


TEST_F(LocalResourceManagerTest, GetResourcePathResourceDoesExistMultiplePathsAdded) {
    p2p::LocalResourceManager manager1;
    std::string temp_path1 = createTempFile("some_path1");
    std::string temp_path2 = createTempFile("some_path2");

    manager1.addResource(std::string("some_name1"), temp_path1);
    manager1.addResource(std::string("some_name2"), temp_path2);

    ASSERT_EQ(manager1.getResourcePath(std::string("some_name1")), std::string(temp_path1));

    removeTempFile(temp_path1);
    removeTempFile(temp_path2);
}


TEST_F(LocalResourceManagerTest, GetAllResourcesNoResources) {
    p2p::LocalResourceManager manager1;
    ASSERT_TRUE(manager1.getAllResources().empty());
}

TEST_F(LocalResourceManagerTest, GetAllResourcesOneResource) {
    p2p::LocalResourceManager manager1;
    std::string test_name = "sone_name";
    std::string temp_path = createTempFile("some_path");

    manager1.addResource(test_name, temp_path);
    std::map<std::string, p2p::ResourceInfo> resource_map = manager1.getAllResources();
    
    ASSERT_EQ(resource_map[test_name].name, test_name);
    ASSERT_EQ(resource_map[test_name].path, temp_path);
    ASSERT_EQ(resource_map[test_name].size, std::filesystem::file_size(temp_path));

    removeTempFile(temp_path);
}

TEST_F(LocalResourceManagerTest, GetAllResourcesMoreThanOneResource) {
    p2p::LocalResourceManager manager1;
    std::string test_name1 = "sone_name1";
    std::string test_name2 = "sone_name2";
    std::string temp_path1 = createTempFile("some_path1");
    std::string temp_path2 = createTempFile("some_path2");

    manager1.addResource(test_name1, temp_path1);
    manager1.addResource(test_name2, temp_path2);
    std::map<std::string, p2p::ResourceInfo> resource_map1 = manager1.getAllResources();
    std::map<std::string, p2p::ResourceInfo> resource_map2 = manager1.getAllResources();

    ASSERT_EQ(resource_map1[test_name1].name, test_name1);
    ASSERT_EQ(resource_map1[test_name1].path, temp_path1);
    ASSERT_EQ(resource_map1[test_name1].size, std::filesystem::file_size(temp_path1));
    ASSERT_EQ(resource_map2[test_name2].name, test_name2);
    ASSERT_EQ(resource_map2[test_name2].path, temp_path2);
    ASSERT_EQ(resource_map2[test_name2].size, std::filesystem::file_size(temp_path2));

    removeTempFile(temp_path1);
    removeTempFile(temp_path2);
}


TEST_F(LocalResourceManagerTest, GetResourceInfoResourceExist) {
    p2p::LocalResourceManager manager1;
    std::string test_name = "sone_name";
    std::string temp_path = createTempFile("some_path");

    manager1.addResource(test_name, temp_path);
    std::optional<p2p::ResourceInfo> resource_info = manager1.getResourceInfo(test_name);
    
    ASSERT_TRUE(resource_info.has_value());
    ASSERT_EQ(resource_info->name, test_name);
    ASSERT_EQ(resource_info->path, temp_path);
    ASSERT_EQ(resource_info->size, std::filesystem::file_size(temp_path));

    removeTempFile(temp_path);
}


TEST_F(LocalResourceManagerTest, GetResourceInfoResourceDoesNotExist) {
    p2p::LocalResourceManager manager1;
    std::string test_name = "sone_name";

    std::optional<p2p::ResourceInfo> resource_info = manager1.getResourceInfo(test_name);
    
    ASSERT_FALSE(resource_info.has_value());
}
