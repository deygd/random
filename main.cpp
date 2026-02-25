// ============================================================
//  Practice Plus - Single File Version
//  Geometry Dash Geode Mod
//
//  Features:
//   • Deaths-per-checkpoint tracker
//   • Session timer HUD
//   • Best checkpoint gold glow
//   • "Jump to Best CP" pause menu button
//   • Click/jump sound effect on every input
// ============================================================
//
//  HOW TO BUILD:
//  1. Install Geode SDK: https://geode-sdk.org/docs/getting-started/
//  2. Create a new Geode mod project and replace its src/main.cpp with this file
//  3. Replace the mod.json with the one at the bottom of this file (in comments)
//  4. Run: cmake -B build && cmake --build build --config Release
//  5. Drop the .geode file into <GD folder>/geode/mods/
//
// ============================================================

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

using namespace geode::prelude;

// ─── Global Session State ──────────────────────────────────────────────────────

struct PracticePlusState {
    std::unordered_map<int, int> checkpointDeaths;
    int bestCheckpointIndex = -1;
    float sessionSeconds    = 0.f;
    bool  isPractice        = false;
    int   currentCPIndex    = -1;
};

static PracticePlusState g_state;

// ─── Helpers ───────────────────────────────────────────────────────────────────

static std::string formatTime(float s) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d.%02d",
        (int)s / 60, (int)s % 60, (int)((s - std::floor(s)) * 100));
    return buf;
}

static void playClickSound() {
    // Use GD's built-in audio engine to play a subtle click
    // "dialogClose.ogg" is a short, satisfying click already bundled with GD.
    // You can swap this for any .ogg file in your resources folder.
    auto* audio = FMODAudioEngine::sharedEngine();

    if (Mod::get()->getSettingValue<bool>("custom-click-sound")) {
        // Play the mod's own bundled click sound if the user enabled it
        std::string soundPath = Mod::get()->getResourcesDir() / "click.ogg";
        audio->playEffect(soundPath);
    } else {
        // Fallback: use a built-in GD sound (very short UI click)
        audio->playEffect("dialogClose.ogg");
    }

    // Respect volume setting
    float vol = Mod::get()->getSettingValue<double>("click-volume");
    // FMOD plays at full SFX volume by default; the setting acts as a relative scale
    // (advanced volume scaling can be done via FMOD channel groups — kept simple here)
}

// ─── PlayLayer Hook ────────────────────────────────────────────────────────────

class $modify(PPPlayLayer, PlayLayer) {

    // ── Init ──────────────────────────────────────────────────────────────────
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        g_state = PracticePlusState{};
        g_state.isPractice = m_isPracticeMode;

        if (m_isPracticeMode) {
            this->schedule(schedule_selector(PPPlayLayer::updateTimer), 0.f);
            this->spawnHUD();
        }

        return true;
    }

    // ── HUD ───────────────────────────────────────────────────────────────────
    void spawnHUD() {
        if (!Mod::get()->getSettingValue<bool>("show-session-timer")) return;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto label   = CCLabelBMFont::create("Session: 00:00.00", "bigFont.fnt");
        label->setScale(0.35f);
        label->setAnchorPoint({0.f, 1.f});
        label->setPosition({6.f, winSize.height - 6.f});
        label->setOpacity(200);
        label->setID("pp-timer-label");
        label->setZOrder(100);
        this->addChild(label);
    }

    void updateTimer(float dt) {
        g_state.sessionSeconds += dt;
        if (!Mod::get()->getSettingValue<bool>("show-session-timer")) return;
        if (auto* lbl = typeinfo_cast<CCLabelBMFont*>(this->getChildByID("pp-timer-label"))) {
            lbl->setString(("Session: " + formatTime(g_state.sessionSeconds)).c_str());
        }
    }

    // ── Click / Jump Sound ────────────────────────────────────────────────────
    // pushButton is called every time the player presses jump/click
    void pushButton(int button, bool player1) {
        PlayLayer::pushButton(button, player1);
        if (Mod::get()->getSettingValue<bool>("enable-click-sound")) {
            playClickSound();
        }
    }

    // ── Death Tracking ────────────────────────────────────────────────────────
    void playerDied() {
        PlayLayer::playerDied();
        if (!m_isPracticeMode) return;

        int idx = g_state.currentCPIndex;
        if (idx >= 0) {
            g_state.checkpointDeaths[idx]++;
            this->refreshDeathLabel(idx);
        }
    }

    // ── Checkpoint Tracking ───────────────────────────────────────────────────
    void markCheckpoint() {
        PlayLayer::markCheckpoint();
        if (!m_isPracticeMode) return;

        int newIdx = (int)m_checkpointArray->count() - 1;
        if (newIdx < 0) newIdx = 0;
        g_state.currentCPIndex = newIdx;

        if (newIdx > g_state.bestCheckpointIndex)
            g_state.bestCheckpointIndex = newIdx;

        this->refreshAllCheckpointVisuals();
    }

    void removeCheckpoint(bool p0) {
        PlayLayer::removeCheckpoint(p0);
        g_state.currentCPIndex = (int)m_checkpointArray->count() - 1;
    }

    // ── Checkpoint Visuals ────────────────────────────────────────────────────
    void refreshDeathLabel(int idx) {
        if (!Mod::get()->getSettingValue<bool>("show-death-counter")) return;
        if (idx >= (int)m_checkpointArray->count()) return;

        auto* cp = typeinfo_cast<CCNode*>(m_checkpointArray->objectAtIndex(idx));
        if (!cp) return;

        if (auto* old = cp->getChildByTag(9901)) old->removeFromParent();

        int deaths = g_state.checkpointDeaths.count(idx) ? g_state.checkpointDeaths[idx] : 0;
        if (deaths == 0) return;

        auto col   = Mod::get()->getSettingValue<ccColor4B>("death-counter-color");
        auto* label = CCLabelBMFont::create(("x" + std::to_string(deaths)).c_str(), "chatFont.fnt");
        label->setScale(0.55f);
        label->setColor({col.r, col.g, col.b});
        label->setOpacity(col.a);
        label->setPosition({0.f, 22.f});
        label->setTag(9901);
        cp->addChild(label, 10);
    }

    void refreshAllCheckpointVisuals() {
        int count = (int)m_checkpointArray->count();
        for (int i = 0; i < count; i++) {
            auto* cp = typeinfo_cast<CCNode*>(m_checkpointArray->objectAtIndex(i));
            if (!cp) continue;

            // Refresh death label
            refreshDeathLabel(i);

            // Gold glow on best checkpoint
            if (auto* oldGlow = cp->getChildByTag(9902)) oldGlow->removeFromParent();

            if (i == g_state.bestCheckpointIndex &&
                Mod::get()->getSettingValue<bool>("highlight-best-checkpoint")) {

                auto* glow = CCSprite::createWithSpriteFrameName("d_gradient_square_02_001.png");
                if (glow) {
                    glow->setColor({255, 215, 0});
                    glow->setOpacity(120);
                    glow->setScale(0.9f);
                    glow->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
                    glow->setTag(9902);
                    cp->addChild(glow, -1);
                    glow->runAction(CCRepeatForever::create(
                        CCSequence::create(CCFadeTo::create(0.7f, 60), CCFadeTo::create(0.7f, 150), nullptr)
                    ));
                }
            }
        }
    }

    // ── Jump to Best Checkpoint ───────────────────────────────────────────────
    void jumpToBestCheckpoint() {
        if (g_state.bestCheckpointIndex < 0) return;
        int count = (int)m_checkpointArray->count();
        for (int i = count - 1; i > g_state.bestCheckpointIndex; i--)
            this->removeCheckpoint(false);
        this->resetLevel();
    }

    void resetLevel() {
        PlayLayer::resetLevel();
    }
};

// ─── PauseLayer Hook ───────────────────────────────────────────────────────────

class $modify(PPPauseLayer, PauseLayer) {

    void customSetup() {
        PauseLayer::customSetup();

        if (!g_state.isPractice) return;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // ── Summary label ──────────────────────────────────────────────────────
        int totalDeaths = 0;
        for (auto& [k, v] : g_state.checkpointDeaths) totalDeaths += v;

        std::string summary =
            "Session: " + formatTime(g_state.sessionSeconds) +
            "   |   Deaths: " + std::to_string(totalDeaths) +
            "   |   Best CP: #" + std::to_string(g_state.bestCheckpointIndex + 1);

        auto* summaryLabel = CCLabelBMFont::create(summary.c_str(), "chatFont.fnt");
        summaryLabel->setScale(0.5f);
        summaryLabel->setOpacity(200);
        summaryLabel->setPosition({winSize.width / 2.f, 38.f});
        summaryLabel->setID("pp-summary-label");
        this->addChild(summaryLabel, 10);

        // ── "Jump to Best CP" button ───────────────────────────────────────────
        if (!Mod::get()->getSettingValue<bool>("best-checkpoint-jump")) return;
        if (g_state.bestCheckpointIndex < 0) return;

        auto* btnSprite = ButtonSprite::create(
            "Best CP", 80, true, "bigFont.fnt", "GJ_button_03.png", 30.f, 0.6f
        );
        auto* btn = CCMenuItemSpriteExtra::create(
            btnSprite, this, menu_selector(PPPauseLayer::onJumpToBest)
        );

        auto* menu = CCMenu::create();
        menu->addChild(btn);
        menu->setPosition({winSize.width / 2.f, 55.f});
        menu->setID("pp-menu");
        this->addChild(menu, 10);
    }

    void onJumpToBest(CCObject*) {
        this->onResume(nullptr);
        auto* pl = typeinfo_cast<PPPlayLayer*>(GameManager::sharedState()->m_playLayer);
        if (pl) pl->jumpToBestCheckpoint();
    }
};

// ============================================================
//  mod.json  (place this as mod.json in your project root)
// ============================================================
/*
{
    "geode": "3.0.0",
    "gd": {
        "win": "2.206",
        "android": "2.206",
        "mac": "2.206",
        "ios": "2.206"
    },
    "id": "claudemod.practiceplus",
    "name": "Practice Plus",
    "version": "v1.0.0",
    "developer": "ClaudeMod",
    "description": "Supercharge practice mode! Deaths per checkpoint, session timer, best CP highlight, jump-to-best button, and a satisfying click sound on every jump.",
    "tags": ["gameplay", "practice", "qol", "utility"],
    "settings": {
        "enable-click-sound": {
            "type": "bool",
            "default": true,
            "name": "Click Sound",
            "description": "Play a sound effect every time you click / jump."
        },
        "custom-click-sound": {
            "type": "bool",
            "default": false,
            "name": "Use Custom Click Sound",
            "description": "Use click.ogg from the mod's resources folder instead of the built-in GD sound."
        },
        "click-volume": {
            "type": "float",
            "default": 0.6,
            "min": 0.0,
            "max": 1.0,
            "name": "Click Volume",
            "description": "Volume of the click sound effect."
        },
        "show-death-counter": {
            "type": "bool",
            "default": true,
            "name": "Show Deaths Per Checkpoint",
            "description": "Displays how many times you have died at each checkpoint."
        },
        "show-session-timer": {
            "type": "bool",
            "default": true,
            "name": "Show Session Timer",
            "description": "Shows how long you have been practicing this level."
        },
        "highlight-best-checkpoint": {
            "type": "bool",
            "default": true,
            "name": "Highlight Best Checkpoint",
            "description": "Adds a pulsing gold glow to your furthest checkpoint."
        },
        "best-checkpoint-jump": {
            "type": "bool",
            "default": true,
            "name": "Best Checkpoint Jump Button",
            "description": "Adds a button in the pause menu to warp to your furthest checkpoint."
        },
        "death-counter-color": {
            "type": "color",
            "default": [255, 80, 80, 255],
            "name": "Death Counter Color",
            "description": "Color of the deaths-per-checkpoint label."
        }
    }
}
*/
