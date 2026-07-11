#include <gtest/gtest.h>
#include "Config.h"
#include <filesystem>
#include <fstream>

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = std::filesystem::temp_directory_path() / "frame0_test";
        std::filesystem::create_directories(dir_);
        path_ = dir_ / "config.json";
    }
    void TearDown() override {
        std::filesystem::remove_all(dir_);
    }
    std::filesystem::path dir_;
    std::filesystem::path path_;
};

TEST_F(ConfigTest, DefaultConfigValues) {
    auto cfg = defaultConfig();
    EXPECT_EQ(cfg.triggerKeyType, TriggerKeyType::MouseLeft);
    EXPECT_EQ(cfg.triggerKeyCode, 0u);
    EXPECT_DOUBLE_EQ(cfg.displayLatencyMs, 0.0);
    EXPECT_DOUBLE_EQ(cfg.mouseLatencyMs, 0.0);
    EXPECT_EQ(cfg.pollingRate, 0);
    EXPECT_TRUE(cfg.fullscreen);
}

TEST_F(ConfigTest, SaveAndLoadRoundTrip) {
    Config original;
    original.triggerKeyType = TriggerKeyType::Keyboard;
    original.triggerKeyCode = 0x20;
    original.displayLatencyMs = 5.0;
    original.mouseLatencyMs = 2.0;
    original.pollingRate = 1000;
    original.fullscreen = false;

    saveConfig(original, path_);
    auto loaded = loadConfig(path_);

    EXPECT_EQ(loaded.triggerKeyType, TriggerKeyType::Keyboard);
    EXPECT_EQ(loaded.triggerKeyCode, 0x20u);
    EXPECT_DOUBLE_EQ(loaded.displayLatencyMs, 5.0);
    EXPECT_DOUBLE_EQ(loaded.mouseLatencyMs, 2.0);
    EXPECT_EQ(loaded.pollingRate, 1000);
    EXPECT_FALSE(loaded.fullscreen);
}

TEST_F(ConfigTest, LoadMissingFileReturnsDefault) {
    auto cfg = loadConfig(dir_ / "nonexistent.json");
    EXPECT_EQ(cfg.triggerKeyType, TriggerKeyType::MouseLeft);
}

TEST_F(ConfigTest, LoadInvalidJsonReturnsDefault) {
    std::ofstream(path_) << "{ invalid json }";
    auto cfg = loadConfig(path_);
    EXPECT_EQ(cfg.triggerKeyType, TriggerKeyType::MouseLeft);
}
