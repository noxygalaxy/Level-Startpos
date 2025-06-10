#pragma once

#ifndef LEVELDLDELEGATE_HPP
#define LEVELDLDELEGATE_HPP

#include <Geode/Geode.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/LevelManagerDelegate.hpp>
#include <Geode/binding/LevelInfoLayer.hpp>
#include <Geode/binding/FLAlertLayer.hpp>

using namespace geode::prelude;

class LevelDLDelegate : public LevelManagerDelegate {
private:
    int id = 0;

public:
    static LevelDLDelegate* get() {
        static LevelDLDelegate instance;
        return &instance;
    }

    void loadLevelsFinished(CCArray* levels, const char* key, int) override;
    void loadLevelsFailed(const char* key, int) override;
};

#endif