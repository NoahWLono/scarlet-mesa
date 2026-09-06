#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

using namespace scarlet;
namespace {
void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
void near(float a, float b, float tolerance, const std::string& message) {
    require(std::abs(a-b)<=tolerance, message);
}
void clearDialogue(Game& g) {
    for (int i=0; g.mode==Mode::Dialogue && i<12; ++i) g.advanceDialogue();
    require(g.mode==Mode::Playing, "dialogue must lead to active gameplay");
}
void tick(Game& g, float seconds, Input input={}) {
    const int steps=static_cast<int>(std::ceil(seconds/Step));
    for(int i=0; i<steps; ++i) g.update(input);
}
Game waveGame(Difficulty d=Difficulty::Normal) {
    Game g; g.start(d); clearDialogue(g); return g;
}
Bullet stationary(Vec p, float radius=4) {
    Bullet b; b.p=p; b.radius=radius; return b;
}
void startup_and_story() {
    Game g;
    require(g.mode==Mode::Title, "new game opens on title");
    g.start(Difficulty::Easy);
    require(g.mode==Mode::Dialogue, "starting must show stage introduction");
    require(g.lives==7 && g.bombs==3, "Easy starts with seven lives and three bombs");
    require(g.stage==0 && g.phase==-1 && !g.bossActive, "campaign starts before boss");
    require(g.player.y>600 && g.power==1, "new player begins at the bottom with base power");
    for(int s=0;s<3;++s) {
        const auto& info=stageInfo(s);
        require(!info.name.empty() && !info.subtitle.empty() && !info.boss.empty(), "stage metadata must exist");
        require(info.spells.size()==3 && info.dialogue.size()>=4, "every boss has three spells and a scene");
    }
    g.advanceDialogue(); require(g.mode==Mode::Dialogue, "intro includes two lines");
    g.advanceDialogue(); require(g.mode==Mode::Playing, "second intro line starts waves");
    require(g.stageTime==0, "dialogue does not spend stage time");
}
void movement_focus_bounds_and_pause() {
    auto g=waveGame();
    float x=g.player.x;
    tick(g,0.25f,{1,0}); float normal=g.player.x-x;
    g.player={300,690};
    tick(g,0.25f,{1,0,false,true}); float focused=g.player.x-300;
    require(normal>focused*2 && focused>15, "focus makes movement markedly slower");
    g.player={300,690}; tick(g,0.1f,{1,1});
    float diagonal=std::hypot(g.player.x-300,g.player.y-690);
    g.player={300,690}; tick(g,0.1f,{1,0});
    near(diagonal,g.player.x-300,0.05f,"diagonal speed must be normalized");
    tick(g,4,{-1,1});
    require(g.player.x>=8 && g.player.y<=FieldH-8, "player is confined to playfield");
    g.pause(); require(g.mode==Mode::Paused,"pause enters paused mode");
    auto p=g.player; auto time=g.time; auto n=g.bullets.size();
    tick(g,1,{1,-1,true});
    require(g.player.x==p.x && g.time==time && g.bullets.size()==n,"pause freezes gameplay");
    g.pause(); require(g.mode==Mode::Playing,"pause toggles back to gameplay");
    g.start(Difficulty::Normal); g.pause();
    require(g.mode==Mode::Paused,"dialogue can be paused");
    g.pause(); require(g.mode==Mode::Dialogue,"pause restores dialogue");
}
void hitbox_graze_and_death() {
    auto g=waveGame(); g.invulnerable=0;
    int lives=g.lives;
    g.bullets.push_back(stationary({g.player.x+12,g.player.y}));
    g.update({});
    require(g.lives==lives,"visible player sprite is larger than collision hitbox");
    require(g.graze==1 && g.bullets[0].grazed,"a nearby bullet grants graze");
    tick(g,0.3f);
    require(g.graze==1,"one bullet may only grant graze once");
    g.bullets.push_back(stationary({g.player.x+6,g.player.y}));
    g.update({});
    require(g.lives==lives-1 && g.invulnerable>1,"bullet radius plus 3px hitbox costs one life");
    require(g.bullets.empty(),"death clears surrounding threats");
    g.damagePlayer(); require(g.lives==lives-1,"respawn invulnerability prevents chain deaths");
    g.invulnerable=0; g.lives=1; g.damagePlayer();
    require(g.mode==Mode::GameOver && g.lives==0,"last life ends the run");
    require(g.score>0,"graze earned score before continue");
    g.continueRun();
    require(g.mode==Mode::Playing && g.lives==5 && g.continues==1,"continue restores the current run");
    require(g.invulnerable>0 && g.score==0 && g.bullets.empty(),"continue is safe and resets the run score");
}
void bombs_are_edge_triggered_and_damage_bosses() {
    auto g=waveGame(); g.beginBoss();
    g.bullets.push_back(stationary({200,300}));
    float hp=g.bossHp; int bombs=g.bombs;
    g.update({0,0,false,false,true});
    require(g.bombs==bombs-1 && g.bullets.empty(),"bomb consumes one stock and clears bullets");
    require(g.bossHp<hp && g.phaseFailed,"bomb damages boss and forfeits capture");
    require(g.bombTime>1 && g.invulnerable>1,"bomb creates a protected cancellation window");
    tick(g,2,{0,0,false,false,true});
    require(g.bombs==bombs-1,"holding bomb must not spend more stocks");
    g.update({}); g.update({0,0,false,false,true});
    require(g.bombs==bombs-2,"release and press permits a second bomb");
    g.bombs=0; g.bombTime=0; g.bomb();
    require(g.bombs==0 && g.bombTime==0,"empty bombs do nothing");
}
void shooting_power_and_pickups() {
    auto g=waveGame(); tick(g,0.12f,{0,0,true});
    require(!g.shots.empty() && g.sfxShot>0,"holding fire emits player shots");
    g.shots.clear();
    Enemy e; e.p={g.player.x,g.player.y-100}; e.hp=1; e.nextFire=10;
    g.enemies.push_back(e);
    tick(g,0.3f,{0,0,true});
    require(g.enemies.empty() && !g.pickups.empty(),"destroying an enemy drops collectibles");
    int p=g.power; g.pickups.push_back({g.player,0}); g.update({});
    require(g.power>p,"power pickup increases shot power");
    g.power=4; g.pickups.push_back({g.player,0}); g.update({});
    require(g.power==4,"power is capped at four levels");
    int score=g.score; g.pickups.push_back({g.player,1}); g.update({});
    require(g.score>score,"point pickup increases score");
    g.beginBoss(); auto hp=g.bossHp;
    g.shots.push_back({g.boss,{0,0},10}); g.update({});
    require(g.bossHp<hp,"player shots damage an active boss");
}
void wave_dialogue_and_phase_progression() {
    auto g=waveGame(); g.invulnerable=100;
    tick(g,8);
    require(!g.enemies.empty() && !g.bullets.empty(),"stage opening has attacking fairy waves");
    tick(g,15);
    require(g.mode==Mode::Dialogue && g.dialogueIndex==2,"wave section yields to pre-boss dialogue");
    require(g.bullets.empty() && g.enemies.empty(),"pre-boss scene clears wave hazards");
    clearDialogue(g);
    require(g.bossActive && g.phase==0 && g.bossHp>0,"pre-boss scene starts first spell");
    int captures=g.captures;
    g.bossHp=0; g.update({});
    require(g.phase==1 && g.captures==captures+1,"defeating an unfailed spell awards a capture");
    g.phaseFailed=true; g.bossHp=0; g.update({});
    require(g.phase==2 && g.captures==captures+1,"bombed or missed spell cannot capture");
    g.bossHp=0; g.update({});
    require(g.stage==1 && g.mode==Mode::Dialogue && !g.bossActive,"boss defeat starts next stage intro");
    require(g.bullets.empty() && g.shots.empty() && g.enemies.empty(),"stage transition drops all old combat entities");
}
void timeouts_are_not_captures_and_practice_ends() {
    Game g; g.start(Difficulty::Easy,2); clearDialogue(g);
    require(g.practice && g.stage==2,"practice selects requested stage");
    require(g.bossActive && g.phase==0 && g.stageTime==0,"practice begins boss after dialogue without waves");
    g.beginBoss(); g.invulnerable=1000;
    for(int p=0;p<3;++p) {
        require(g.phase==p,"practice visits each spell once");
        g.phaseTime=1000; g.update({});
    }
    require(g.mode==Mode::Victory && g.captures==0,"survival timeout ends practice without capture bonuses");
    g.returnToTitle();
    require(g.mode==Mode::Title && g.bullets.empty() && g.shots.empty() && g.enemies.empty(),"return to title releases combat state");
}
void deterministic_and_difficulty_sensitive() {
    auto a=waveGame(Difficulty::Lunatic), b=waveGame(Difficulty::Lunatic);
    a.beginBoss(); b.beginBoss(); a.invulnerable=b.invulnerable=1000;
    for(int i=0;i<1800;++i) {
        Input input{std::sin(i*0.03f),std::cos(i*0.02f),i%7==0,i%5==0,false};
        a.update(input); b.update(input);
    }
    require(a.score==b.score && a.bullets.size()==b.bullets.size() && a.bossHp==b.bossHp,"same input gives same simulation");
    for(size_t i=0;i<a.bullets.size();++i) {
        require(a.bullets[i].p.x==b.bullets[i].p.x && a.bullets[i].p.y==b.bullets[i].p.y,"every bullet replays deterministically");
    }
    auto easy=waveGame(Difficulty::Easy), hard=waveGame(Difficulty::Lunatic);
    easy.beginBoss(); hard.beginBoss(); easy.invulnerable=hard.invulnerable=100;
    tick(easy,8); tick(hard,8);
    require(hard.peakBullets>easy.peakBullets,"Lunatic creates denser patterns than Easy");
}
void input_sanitization_and_entity_cleanup() {
    auto g=waveGame(); float before=g.time;
    g.update({},0); g.update({},-1); g.update({},std::numeric_limits<float>::quiet_NaN());
    require(g.time==before,"invalid timesteps cannot corrupt state");
    g.update({std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()});
    require(g.invariant(),"invalid movement input remains finite");
    g.bullets.push_back(stationary({-5000,100}));
    g.shots.push_back({{200,-1000},{0,-500},1});
    g.pickups.push_back({{200,2000},1});
    g.particles.push_back({{200,200},{0,0},-1,1,0});
    g.update({});
    require(g.bullets.empty() && g.shots.empty() && g.pickups.empty() && g.particles.empty(),"expired and offscreen entities are removed");
}
void every_spell_is_capturable_with_real_player_shots() {
    float fastest=1000,slowest=0;
    for(auto difficulty:{Difficulty::Easy,Difficulty::Normal,Difficulty::Lunatic}) {
        for(int stage=0;stage<3;++stage) {
            Game g; g.start(difficulty,stage); clearDialogue(g); g.power=4;
            for(int phase=0;phase<3;++phase) {
                require(g.phase==phase && g.bossActive,"ideal pilot reaches expected spell");
                const int captures=g.captures;
                float elapsed=0,previousBossX=g.boss.x;
                // An invulnerable predictive pilot follows boss motion while shooting.
                // This tests real damage output, flight time and firing cadence; it
                // deliberately removes dodging skill as a constraint on capture.
                while(g.mode==Mode::Playing && g.phase==phase && elapsed<39) {
                    const float vx=(g.boss.x-previousBossX)/Step;
                    previousBossX=g.boss.x;
                    const float target=g.boss.x+std::clamp(vx,-90.0f,90.0f)*0.65f;
                    g.invulnerable=10;
                    g.update({std::clamp((target-g.player.x)/(118*Step),-1.0f,1.0f),0,true,true,false});
                    elapsed+=Step;
                }
                require(g.captures==captures+1,"real shots must capture difficulty "+std::to_string(static_cast<int>(difficulty))+" stage "+std::to_string(stage)+" phase "+std::to_string(phase)+" before timeout");
                require(elapsed<37.5f,"ideal capture has headroom before the 38-second timeout");
                fastest=std::min(fastest,elapsed);slowest=std::max(slowest,elapsed);
            }
            require(g.mode==Mode::Victory,"real player shots complete stage practice");
        }
    }
    std::cout << "  real-shot ideal capture duration range=" << fastest << ".." << slowest << "s\n";
}
void full_campaign_all_patterns_and_bounded_entities() {
    for(auto difficulty:{Difficulty::Easy,Difficulty::Normal,Difficulty::Lunatic}) {
        Game g; g.start(difficulty);
        std::set<int> visited;
        int maxBullet=0, maxEnemy=0, maxShot=0;
        // This exercises every real emission and timeout at the production fixed timestep.
        // Invulnerability isolates progression and numerical stability from pilot skill.
        for(int i=0;i<65000 && g.mode!=Mode::Victory;++i) {
            if(g.mode==Mode::Dialogue) { clearDialogue(g); continue; }
            g.invulnerable=10;
            if(g.bossActive) visited.insert(g.stage*3+g.phase);
            g.update({std::sin(i*0.002f),0,false,i%2==0,false});
            require(g.invariant(),"full campaign invariant failed at stage "+std::to_string(g.stage)+" phase "+std::to_string(g.phase));
            maxBullet=std::max(maxBullet,static_cast<int>(g.bullets.size()));
            maxEnemy=std::max(maxEnemy,static_cast<int>(g.enemies.size()));
            maxShot=std::max(maxShot,static_cast<int>(g.shots.size()));
        }
        require(g.mode==Mode::Victory,"campaign must terminate through real phase timeouts");
        require(visited.size()==9,"long run exercises all nine spell patterns");
        require(g.time>350 && g.time<450,"full survival campaign is about six to seven minutes");
        require(g.captures==0,"timeouts alone never earn captures");
        require(maxBullet>80 && maxBullet<3000 && maxEnemy<40 && maxShot<300,"entity counts remain bounded through a full campaign");
        require(g.bullets.empty() && g.enemies.empty() && g.shots.empty(),"victory releases active combat entities");
        std::cout << "  campaign difficulty=" << static_cast<int>(difficulty) << " duration=" << g.time << "s peak=" << maxBullet << '\n';
    }
}
}
int main() {
    int failures=0;
    const std::pair<const char*,std::function<void()>> cases[]={
        {"startup and story",startup_and_story},
        {"movement focus bounds and pause",movement_focus_bounds_and_pause},
        {"tiny hitbox, once-only graze, death, continue",hitbox_graze_and_death},
        {"bomb cancellation, damage and edge trigger",bombs_are_edge_triggered_and_damage_bosses},
        {"shooting, power and pickups",shooting_power_and_pickups},
        {"waves, boss dialogue and phase transitions",wave_dialogue_and_phase_progression},
        {"timeouts, practice and title cleanup",timeouts_are_not_captures_and_practice_ends},
        {"determinism and difficulty",deterministic_and_difficulty_sensitive},
        {"input sanitization and entity cleanup",input_sanitization_and_entity_cleanup},
        {"every spell is capturable with real shots",every_spell_is_capturable_with_real_player_shots},
        {"three complete simulated campaigns",full_campaign_all_patterns_and_bounded_entities},
    };
    for(const auto& test:cases) {
        try { test.second(); std::cout << "PASS " << test.first << '\n'; }
        catch(const std::exception& error) { ++failures; std::cerr << "FAIL " << test.first << ": " << error.what() << '\n'; }
    }
    std::cout << std::size(cases)-failures << '/' << std::size(cases) << " test groups passed\n";
    return failures==0?0:1;
}
