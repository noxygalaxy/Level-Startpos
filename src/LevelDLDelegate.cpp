#include "LevelDLDelegate.hpp"

void LevelDLDelegate::loadLevelsFinished(CCArray* levels, const char* key, int) {
    if (levels->count() < 1) {
        loadLevelsFailed(key, 0);
        return;
    }
    GameLevelManager::get()->m_levelManagerDelegate = nullptr;
    log::info("Level downloaded, delegate complete");
    auto levelLayer = LevelInfoLayer::scene(static_cast<GJGameLevel*>(levels->objectAtIndex(0)), false);
    CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(0.5, levelLayer));
}

void LevelDLDelegate::loadLevelsFailed(const char* key, int) {
    GameLevelManager::get()->m_levelManagerDelegate = nullptr;
    log::info("Level failed to download, delegate complete");
    FLAlertLayer::create(
        "Download Failed",
        "Level download failed. Please try again later.",
        "Ok"
    )->show();
}