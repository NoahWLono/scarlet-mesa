#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace scarlet {
constexpr float FieldW = 600, FieldH = 800;
constexpr float Step = 1.0f / 120.0f;
struct Vec { float x=0, y=0; };
enum class Mode { Title, Dialogue, Playing, Paused, GameOver, Victory };
enum class Difficulty { Easy, Normal, Lunatic };
struct Input { float x=0, y=0; bool fire=false, focus=false, bomb=false; };
struct Bullet { Vec p, v; float radius=5, age=0, turn=0; int color=0, shape=0; bool grazed=false; };
struct Shot { Vec p, v; float damage=1; };
struct Enemy { Vec p, v; float hp=8, age=0, nextFire=1; int kind=0; };
struct Pickup { Vec p; int kind=0; };
struct Particle { Vec p,v; float life=1, maxLife=1; int color=0; };
struct Dialogue { std::string speaker, text; };
struct StageInfo { std::string name, subtitle, boss, role; std::vector<std::string> spells; std::vector<Dialogue> dialogue; };
const StageInfo& stageInfo(int stage);
class Game {
public:
    Mode mode=Mode::Title, resumeMode=Mode::Playing;
    Difficulty difficulty=Difficulty::Normal;
    Vec player{300,690}, boss{300,145};
    std::vector<Bullet> bullets;
    std::vector<Shot> shots;
    std::vector<Enemy> enemies;
    std::vector<Pickup> pickups;
    std::vector<Particle> particles;
    int stage=0, phase=-1, lives=5, bombs=3, power=1, graze=0, captures=0, continues=0;
    int dialogueIndex=0, score=0, peakBullets=0;
    float time=0, stageTime=0, phaseTime=0, bossHp=0, bossMaxHp=1;
    float invulnerable=0, bombTime=0, shake=0, bannerTime=0;
    bool practice=false, phaseFailed=false, bossActive=false;
    int sfxShot=0, sfxHit=0, sfxBomb=0, sfxClear=0, sfxGraze=0;
    void start(Difficulty d, int practiceStage=-1);
    void advanceDialogue();
    void update(const Input& input, float dt=Step);
    void pause();
    void continueRun();
    void returnToTitle();
    void damagePlayer();
    void bomb();
    void beginBoss();
    void nextPhase();
    bool invariant() const;
private:
    uint32_t rng=0xC1A090u;
    float shotClock=0, spawnClock=0, patternClock=0, ringClock=0;
    bool bombHeld=false;
    float random();
    void enterStage(int index);
    void emit(Vec p, float angle, float speed, int color, int shape=0, float radius=5, float turn=0);
    void burst(Vec p, int n, int color);
    void updateWaves(float dt);
    void updateBoss(float dt);
};
}
