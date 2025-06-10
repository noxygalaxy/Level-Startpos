#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/loader/Event.hpp>
#include <Geode/utils/JsonValidation.hpp>
#include <matjson.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include "LevelDLDelegate.hpp"

using namespace geode::prelude;

struct StartposData {
    std::string levelID;
    std::string startposID;
};

class StartposManager {
private:
    static StartposManager* instance;
    std::vector<StartposData> startposLevels;
    EventListener<web::WebTask> webListener;
    bool isLoading = false;
    std::chrono::steady_clock::time_point lastFetch;
    
public:
    static StartposManager* getInstance() {
        if (!instance) {
            instance = new StartposManager();
        }
        return instance;
    }
    
    void fetchStartposData() {
        if (isLoading) return;
        
        // Check if we need to refresh based on settings
        auto now = std::chrono::steady_clock::now();
        auto autoRefresh = Mod::get()->getSettingValue<bool>("auto-refresh");
        auto refreshInterval = Mod::get()->getSettingValue<int64_t>("refresh-interval");
        
        if (autoRefresh && !startposLevels.empty()) {
            auto minutesSinceLastFetch = std::chrono::duration_cast<std::chrono::minutes>(now - lastFetch).count();
            if (minutesSinceLastFetch < refreshInterval) {
                return; // Don't refresh yet
            }
        }
        
        isLoading = true;
        lastFetch = now;
        
        webListener.bind([this](web::WebTask::Event* e) {
            if (web::WebResponse* res = e->getValue()) {
                this->handleWebResponse(res);
			} else if (web::WebProgress* progress = e->getProgress()) {
				log::info("Fetching startpos data... {}%", 
					static_cast<int>(progress->downloadProgress().value_or(0.0f) * 100));
            } else if (e->isCancelled()) {
                log::warn("Startpos data fetch was cancelled");
                isLoading = false;
            }
        });
        
        auto jsonUrl = Mod::get()->getSettingValue<std::string>("json-url");
        auto req = web::WebRequest();
        req.timeout(std::chrono::seconds(30));
        req.userAgent("Level Startpos Mod");
        
        webListener.setFilter(req.get(jsonUrl));
        
        log::info("Fetching startpos data from: {}", jsonUrl);
    }
    
	void handleWebResponse(web::WebResponse* res) {
		isLoading = false;
		
		if (!res->ok()) {
			log::error("Failed to fetch startpos data. Status code: {}", res->code());
			return;
		}
		
		auto jsonResult = res->json();
		if (!jsonResult) {
			log::error("Failed to parse JSON response: {}", jsonResult.unwrapErr());
			return;
		}
		
		auto json = jsonResult.unwrap();
		if (!json.isArray()) {
			log::error("JSON response is not an array");
			return;
		}
		
		startposLevels.clear();
		
		auto jsonArrayResult = json.asArray();
		if (!jsonArrayResult) {
			log::error("Failed to convert JSON to array: {}", jsonArrayResult.unwrapErr());
			return;
		}
		
		auto jsonArray = jsonArrayResult.unwrap();
		for (auto& item : jsonArray) {
			if (!item.isObject()) continue;
			
			// Use matjson::Value directly for objects
			auto levelResult = item.get("level");
			auto startposResult = item.get("startpos");
			
			if (!levelResult || !startposResult) continue; // Skip if fields are missing
			
			auto levelValue = levelResult.unwrap();
			auto startposValue = startposResult.unwrap();
			
			if (!levelValue.isString() || !startposValue.isString()) continue;
			
			auto levelIDResult = levelValue.asString();
			auto startposIDResult = startposValue.asString();
			
			if (!levelIDResult || !startposIDResult) continue; // Skip if string conversion fails
			
			StartposData data;
			data.levelID = levelIDResult.unwrap();
			data.startposID = startposIDResult.unwrap();
			startposLevels.push_back(data);
		}
		
		log::info("Successfully loaded {} startpos entries", startposLevels.size());
	}
    
    std::string getStartposForLevel(const std::string& levelID) {
        for (const auto& data : startposLevels) {
            if (data.levelID == levelID) {
                return data.startposID;
            }
        }
        return "";
    }
    
    bool hasStartposForLevel(const std::string& levelID) {
        return !getStartposForLevel(levelID).empty();
    }
};

StartposManager* StartposManager::instance = nullptr;

class $modify(StartposLevelInfoLayer, LevelInfoLayer) {
    struct Fields {
        CCMenuItemSpriteExtra* startposButton = nullptr;
    };
    
    bool init(GJGameLevel* level, bool p1) {
        if (!LevelInfoLayer::init(level, p1)) return false;
        
        // Fetch startpos data when first opening a level
        StartposManager::getInstance()->fetchStartposData();
        
        // Check if this level has a startpos version
        std::string levelID = std::to_string(level->m_levelID);
        if (StartposManager::getInstance()->hasStartposForLevel(levelID)) {
            addStartposButton(level);
        }
        
        return true;
    }
    
    void addStartposButton(GJGameLevel* level) {
        auto menu = getChildByID("left-side-menu");
        
        auto startposButton = CCMenuItemSpriteExtra::create(
            CircleButtonSprite::createWithSpriteFrameName(fmt::format("edit_eStartPosBtn_001.png").c_str()),
            this,
            menu_selector(StartposLevelInfoLayer::onStartposButton)
        );

        startposButton->setID("startpos-btn"_spr);
        menu->addChild(startposButton);
        menu->updateLayout();
        
        log::info("Added startpos button for level {}", level->m_levelID);
    }
    
    void onStartposButton(CCObject* sender) {
        auto level = m_level;
        if (!level) return;
        
        std::string levelID = std::to_string(level->m_levelID);
        std::string startposID = StartposManager::getInstance()->getStartposForLevel(levelID);
        
        if (startposID.empty()) {
            log::error("No startpos found for level {}", levelID);
            return;
        }
        
        log::info("Opening startpos level {} for original level {}", startposID, levelID);
        
        int startposLevelID = 0;
        try {
            startposLevelID = std::stoi(startposID);
        } catch (const std::exception& e) {
            log::error("Invalid startpos ID: {}", startposID);
            return;
        }
        
        this->openStartposLevel(startposLevelID);
    }
    
	void openStartposLevel(int levelID) {
		GameLevelManager::get()->m_levelManagerDelegate = LevelDLDelegate::get();
        GameLevelManager::sharedState()->getOnlineLevels(GJSearchObject::create(SearchType::Search, std::to_string(levelID)));
    }
};

class $modify(StartposMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        StartposManager::getInstance()->fetchStartposData();
        return true;
    }
};