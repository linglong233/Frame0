#include <gtest/gtest.h>
#include "Storage.h"
#include <filesystem>
#include <fstream>

class StorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = std::filesystem::temp_directory_path() / "reaction_timer_storage_test";
        std::filesystem::create_directories(dir_);
        path_ = dir_ / "history.json";
    }
    void TearDown() override {
        std::filesystem::remove_all(dir_);
    }
    std::filesystem::path dir_;
    std::filesystem::path path_;
};

TEST_F(StorageTest, AppendAndLoadRoundTrip) {
    SessionResult r;
    r.timestamp = "2026-05-16T14:30:00Z";
    r.rounds = {215.3, 198.7, 210.1, 205.5, 202.8};
    r.median = 205.5;
    r.mean = 206.48;
    r.stddev = 6.12;
    r.refreshRate = 240;
    r.pollingRate = 1000;
    r.fullscreen = true;

    appendHistory(path_, r);
    auto loaded = loadHistory(path_);

    ASSERT_EQ(loaded.size(), 1u);
    EXPECT_EQ(loaded[0].timestamp, "2026-05-16T14:30:00Z");
    ASSERT_EQ(loaded[0].rounds.size(), 5u);
    EXPECT_NEAR(loaded[0].rounds[0], 215.3, 0.01);
    EXPECT_NEAR(loaded[0].median, 205.5, 0.01);
    EXPECT_NEAR(loaded[0].mean, 206.48, 0.01);
    EXPECT_NEAR(loaded[0].stddev, 6.12, 0.01);
    EXPECT_EQ(loaded[0].refreshRate, 240);
    EXPECT_EQ(loaded[0].pollingRate, 1000);
    EXPECT_TRUE(loaded[0].fullscreen);
}

TEST_F(StorageTest, AppendMultipleEntries) {
    SessionResult r1;
    r1.timestamp = "2026-05-16T14:30:00Z";
    r1.rounds = {200.0};
    r1.median = 200.0; r1.mean = 200.0; r1.stddev = 0.0;

    SessionResult r2;
    r2.timestamp = "2026-05-16T15:00:00Z";
    r2.rounds = {180.0};
    r2.median = 180.0; r2.mean = 180.0; r2.stddev = 0.0;

    appendHistory(path_, r1);
    appendHistory(path_, r2);

    auto loaded = loadHistory(path_);
    ASSERT_EQ(loaded.size(), 2u);
    EXPECT_EQ(loaded[0].timestamp, "2026-05-16T14:30:00Z");
    EXPECT_EQ(loaded[1].timestamp, "2026-05-16T15:00:00Z");
}

TEST_F(StorageTest, LoadMissingFileReturnsEmpty) {
    auto loaded = loadHistory(dir_ / "nonexistent.json");
    EXPECT_TRUE(loaded.empty());
}

TEST_F(StorageTest, LoadInvalidJsonReturnsEmpty) {
    std::ofstream(path_) << "not json";
    auto loaded = loadHistory(path_);
    EXPECT_TRUE(loaded.empty());
}
