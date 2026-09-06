#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace scarlet {
namespace {
constexpr float Pi=3.14159265358979323846f;
constexpr float Tau=Pi*2;
constexpr float WaveDuration=22;
constexpr float SpellDuration=38;
constexpr float PlayerRadius=3;
constexpr int BulletLimit=2400;

const StageInfo Stages[]={
    {"FOG OVER THE GOLDEN GATE", "Stage 01 / An unusually intelligent fog", "Cirno", "Chief Ice Officer",
     {"Ice Sign \"Nine-Billion Parameter Snowflake\"", "Freeze Sign \"Stochastic Gradient Descent\"", "Genius Sign \"The Strongest Baseline\""},
     {{"Reimu", "Yukari said the incident was in San Francisco. My donation box said the flight was out of scope."},
      {"Reimu", "Now the Golden Gate is covered in magical ice. At least the bridge has a concrete model."},
      {"Cirno", "Behold! I trained a nine-billion-parameter snowflake. Nine is the biggest number!"},
      {"Reimu", "What happened to the other parameters?"},
      {"Cirno", "Frozen! My model cannot misbehave if it cannot move. Perfect alignment!"},
      {"Reimu", "That explains the frozen bay. Time for a little gradient ascent."}}},
    {"THE SOMA TIME ACCELERATOR", "Stage 02 / Move slowly and break causality", "Sakuya", "Head of Inference-Time Compute",
     {"Clock Sign \"Inference-Time Scaling\"", "Time Sign \"Pause the Training Run\"", "Knife Sign \"A Thousand Paper Cuts\""},
     {{"Reimu", "The trail leads to a SoMa accelerator. The pitch deck promised infinite runway. I see a loading dock."},
      {"Reimu", "Someone is serving tea between frames of reality. Only one maid bills by the stopped second."},
      {"Sakuya", "Welcome. Lady Remilia invested in a startup that predicts the future. It predicted more fundraising."},
      {"Reimu", "And you are stopping time to make the benchmark faster?"},
      {"Sakuya", "Our latency is zero. Our evaluation took three eternities. The tea is still warm."},
      {"Reimu", "Let's test your model on something it has never seen: a deadline."}}},
    {"THE BOUNDARY ABOVE THE BAY", "Stage 03 / Some gaps should stay open", "Yukari", "Founder, Boundary Models",
     {"Boundary Sign \"Distribution Shift\"", "Gap Sign \"Out-of-Distribution Express\"", "Last Word \"The Alignment Problem\""},
     {{"Reimu", "The fog parts above the bay. There is a gap in the sky, and a terms-of-service page in the gap."},
      {"Reimu", "Gensokyo's boundary is running on a free trial. Naturally, Yukari has the only admin account."},
      {"Yukari", "I trained a boundary model on every incident in Gensokyo. It concluded that incidents increase engagement."},
      {"Reimu", "Your objective function is broken."},
      {"Yukari", "Then suggest a better one, dear. Maximize donations? Minimize shrine-maiden effort?"},
      {"Reimu", "Keep Gensokyo weird. Keep the bay unfrozen. And stop optimizing my life."}}}
};
float distanceSquared(Vec a,Vec b) { const float x=a.x-b.x,y=a.y-b.y; return x*x+y*y; }
float angleTo(Vec a,Vec b) { return std::atan2(b.y-a.y,b.x-a.x); }
float speedScale(Difficulty d) { return d==Difficulty::Easy?0.76f:d==Difficulty::Lunatic?1.20f:1.0f; }
int density(Difficulty d,int easy,int normal,int hard) { return d==Difficulty::Easy?easy:d==Difficulty::Lunatic?hard:normal; }
bool finite(Vec p) { return std::isfinite(p.x)&&std::isfinite(p.y); }
float countdown(float value,float dt) { return std::max(0.0f,value-dt); }
template<class T,class Predicate> void discard(std::vector<T>& values,Predicate predicate) {
    values.erase(std::remove_if(values.begin(),values.end(),predicate),values.end());
}
}

const StageInfo& stageInfo(int stage) { return Stages[std::clamp(stage,0,2)]; }

float Game::random() {
    rng^=rng<<13; rng^=rng>>17; rng^=rng<<5;
    return static_cast<float>(rng&0x00ffffffu)/16777216.0f;
}

void Game::start(Difficulty d,int practiceStage) {
    // Starting again resets clocks, the random sequence, effects, and input edges.
    *this=Game{};
    difficulty=d;
    practice=practiceStage>=0;
    lives=d==Difficulty::Easy?7:d==Difficulty::Lunatic?3:5;
    bombs=3;
    power=practice?3:1;
    enterStage(practice?std::clamp(practiceStage,0,2):0);
}

void Game::enterStage(int index) {
    stage=std::clamp(index,0,2); phase=-1; bossActive=false;
    stageTime=0; phaseTime=0; bossHp=0; bossMaxHp=1;
    dialogueIndex=0; mode=Mode::Dialogue; resumeMode=Mode::Dialogue;
    player={300,690}; boss={300,145};
    bullets.clear(); shots.clear(); enemies.clear(); pickups.clear(); particles.clear();
    shotClock=0; spawnClock=0.7f; patternClock=0.7f; ringClock=1.3f;
    invulnerable=2; bombTime=0; bannerTime=3; shake=0; phaseFailed=false; bombHeld=false;
}

void Game::advanceDialogue() {
    if(mode!=Mode::Dialogue) return;
    ++dialogueIndex;
    if(dialogueIndex==2 && stageTime<WaveDuration && !practice) {
        mode=Mode::Playing; resumeMode=Mode::Playing;
    } else if(dialogueIndex>=static_cast<int>(stageInfo(stage).dialogue.size())) {
        beginBoss();
    }
}

void Game::pause() {
    if(mode==Mode::Playing||mode==Mode::Dialogue) { resumeMode=mode; mode=Mode::Paused; }
    else if(mode==Mode::Paused) mode=resumeMode;
}

void Game::continueRun() {
    if(mode!=Mode::GameOver) return;
    ++continues; score=0;
    lives=difficulty==Difficulty::Easy?7:difficulty==Difficulty::Lunatic?3:5;
    bombs=3; power=std::max(power,2); invulnerable=4; bombTime=1;
    bullets.clear(); shots.clear(); enemies.clear();
    player={300,690}; mode=Mode::Playing; resumeMode=Mode::Playing;
    phaseFailed=true; bombHeld=false;
}

void Game::returnToTitle() {
    mode=Mode::Title; bossActive=false;
    bullets.clear(); shots.clear(); enemies.clear(); pickups.clear(); particles.clear();
    bombTime=0; shake=0;
}

void Game::emit(Vec p,float angle,float speed,int color,int shape,float radius,float turn) {
    if(bullets.size()>=BulletLimit || bombTime>0) return;
    Bullet b;
    b.p=p; b.v={std::cos(angle)*speed,std::sin(angle)*speed};
    b.color=color; b.shape=shape; b.radius=radius; b.turn=turn;
    bullets.push_back(b);
}

void Game::burst(Vec p,int n,int color) {
    for(int i=0;i<n && particles.size()<800;++i) {
        const float angle=random()*Tau, speed=25+random()*120, life=0.25f+random()*0.5f;
        particles.push_back({p,{std::cos(angle)*speed,std::sin(angle)*speed},life,life,color});
    }
}

void Game::damagePlayer() {
    if(mode!=Mode::Playing || invulnerable>0 || bombTime>0) return;
    --lives; ++sfxHit; phaseFailed=true; shake=0.6f;
    burst(player,48,2);
    // Mutating the bullet vector is safe because update calls this after collision scans.
    bullets.clear();
    power=std::max(1,power-1);
    bombs=std::max(bombs,2);
    invulnerable=3.0f;
    if(lives<=0) { lives=0; mode=Mode::GameOver; }
    else player={300,690};
}

void Game::bomb() {
    if(mode!=Mode::Playing || bombs<=0 || bombTime>0) return;
    --bombs; ++sfxBomb; phaseFailed=true;
    score+=static_cast<int>(bullets.size())*10;
    bullets.clear();
    for(auto& enemy:enemies) enemy.hp-=30;
    if(bossActive) bossHp-=320;
    bombTime=1.6f; invulnerable=std::max(invulnerable,2.1f); shake=0.65f;
    burst(player,72,3);
}

void Game::beginBoss() {
    if(bossActive) return;
    bullets.clear(); shots.clear(); enemies.clear(); pickups.clear();
    bossActive=true; phase=-1; boss={300,145};
    mode=Mode::Playing; resumeMode=Mode::Playing;
    invulnerable=std::max(invulnerable,1.5f);
    nextPhase();
}

void Game::nextPhase() {
    if(!bossActive) return;
    if(phase>=0) {
        const bool captured=bossHp<=0 && !phaseFailed && phaseTime<SpellDuration;
        if(captured) { ++captures; score+=50000+stage*15000+phase*5000; }
        else score+=8000+stage*2000;
        score+=static_cast<int>(bullets.size())*10;
        ++sfxClear; burst(boss,56,phase%6);
    }
    bullets.clear(); shots.clear(); enemies.clear();
    ++phase;
    if(phase>=3) {
        phase=2; bossActive=false;
        if(stage>=2||practice) {
            mode=Mode::Victory; resumeMode=Mode::Victory; pickups.clear();
            score+=100000+std::max(0,lives)*10000;
        } else {
            bombs=std::min(5,bombs+1);
            power=std::min(4,power+1);
            if(lives<8) ++lives;
            enterStage(stage+1);
        }
        return;
    }
    phaseTime=0; phaseFailed=false;
    const float healthScale=difficulty==Difficulty::Easy?0.82f:difficulty==Difficulty::Lunatic?1.12f:1;
    bossHp=bossMaxHp=(1050+stage*150+phase*140)*healthScale;
    patternClock=0.95f; ringClock=1.75f;
    bannerTime=2.6f;
    invulnerable=std::max(invulnerable,0.8f);
    // Every spell begins with a clear screen and a short readable wind-up.
}

void Game::updateWaves(float dt) {
    spawnClock-=dt;
    if(spawnClock<=0 && stageTime<WaveDuration-3) {
        spawnClock+=1.7f-(difficulty==Difficulty::Lunatic?0.28f:0);
        const int wave=static_cast<int>(stageTime/1.7f);
        const int count=wave%3==0?3:2;
        for(int i=0;i<count && enemies.size()<40;++i) {
            Enemy enemy;
            const bool left=(wave+i)%2==0;
            enemy.p={left?65.0f+static_cast<float>(i)*40:FieldW-65.0f-static_cast<float>(i)*40,-25.0f-static_cast<float>(i)*40};
            enemy.v={left?20.0f:-20.0f,53.0f+static_cast<float>(stage)*6};
            enemy.hp=8+stage*3;
            enemy.kind=(wave+i+stage)%3;
            enemy.nextFire=1.0f+static_cast<float>(i)*0.3f;
            enemies.push_back(enemy);
        }
    }
    const float speed=speedScale(difficulty);
    for(auto& enemy:enemies) {
        enemy.age+=dt;
        enemy.p.x+=(enemy.v.x+std::sin(enemy.age*1.8f+enemy.kind)*24)*dt;
        enemy.p.y+=enemy.v.y*dt;
        enemy.nextFire-=dt;
        if(enemy.nextFire<=0 && enemy.p.y>25 && enemy.p.y<570) {
            enemy.nextFire+=difficulty==Difficulty::Easy?2.8f:2.15f;
            const float aim=angleTo(enemy.p,player);
            const int n=density(difficulty,3,5,7);
            if(enemy.kind==0) {
                // Aimed fans invite the player to move, then hold a new lane.
                for(int j=0;j<n;++j) emit(enemy.p,aim+(j-(n-1)*0.5f)*0.15f,(105+stage*12)*speed,stage,1,4);
            } else if(enemy.kind==1) {
                const int ring=n*2;
                for(int j=0;j<ring;++j) emit(enemy.p,Tau*j/ring+enemy.age*0.2f,90*speed,(stage+2)%6,0,4);
            } else {
                for(int j=0;j<n;++j) emit(enemy.p,aim+(j-(n-1)*0.5f)*0.1f,(95+j*8)*speed,(stage+1)%6,2,4);
            }
        }
    }
}

void Game::updateBoss(float dt) {
    const float t=phaseTime;
    const float speed=speedScale(difficulty);
    boss.x=300+std::sin(t*(stage==1?0.65f:0.37f)+phase*0.8f)*(stage==2?118.0f:92.0f);
    boss.y=138+std::sin(t*0.6f)*18;
    patternClock-=dt; ringClock-=dt;
    const float aim=angleTo(boss,player);
    if(stage==0 && phase==0) {
        // Six snowflake arms, with space between their spokes and rotating seams.
        if(patternClock<=0) {
            patternClock+=density(difficulty,100,78,61)*0.01f;
            const int branches=density(difficulty,2,3,4);
            const float rotation=t*0.14f+0.15f*std::sin(t*0.8f);
            for(int arm=0;arm<6;++arm) for(int j=0;j<branches;++j) {
                emit(boss,rotation+arm*Tau/6+(j-(branches-1)*0.5f)*0.075f,
                     (92+j*11)*speed,j%2==0?0:1,2,4.5f);
            }
        }
        if(ringClock<=0) {
            ringClock+=2.7f;
            for(int j=-1;j<=1;++j) emit(boss,aim+j*0.20f,150*speed,2,0,5.5f);
        }
    } else if(stage==0 && phase==1) {
        // Falling staggered ice columns leave a two-column moving corridor.
        if(patternClock<=0) {
            patternClock+=density(difficulty,110,84,65)*0.01f;
            const int columns=density(difficulty,9,12,16);
            const int gap=static_cast<int>(t/2.4f)%std::max(1,columns-3)+1;
            const float spacing=FieldW/static_cast<float>(columns);
            for(int j=0;j<columns;++j) {
                if(j==gap||j==gap+1) continue;
                emit({spacing*(j+0.5f),-15},Pi*0.5f+std::sin(t*0.6f)*0.075f,
                     (95+(j%3)*12)*speed,j%2,2,4.2f);
            }
        }
        if(ringClock<=0) {
            ringClock+=2.3f;
            const int n=density(difficulty,12,18,24);
            for(int j=0;j<n;++j) emit(boss,Tau*j/n+t*0.18f,83*speed,4,0,4);
        }
    } else if(stage==0 && phase==2) {
        // The "strongest" fairy draws nine petals; each has a broad escape seam.
        if(patternClock<=0) {
            patternClock+=density(difficulty,85,65,49)*0.01f;
            const int petals=9, layers=density(difficulty,1,2,3);
            for(int petal=0;petal<petals;++petal) for(int j=0;j<layers;++j) {
                const float angle=Tau*petal/petals+0.27f*std::sin(t*0.6f)+j*0.06f;
                emit(boss,angle,(105+j*18)*speed,petal%2==0?0:4,2,4.1f,0.10f*std::sin(t*0.4f));
            }
        }
        if(ringClock<=0) {
            ringClock+=2.0f;
            const int n=density(difficulty,3,5,7);
            for(int j=0;j<n;++j) emit(boss,aim+(j-(n-1)*0.5f)*0.13f,165*speed,2,1,4);
        }
    } else if(stage==1 && phase==0) {
        // Rotating clock hands: one long sweep and a slower counter-rotation.
        if(patternClock<=0) {
            patternClock+=density(difficulty,32,23,17)*0.01f;
            const int hands=density(difficulty,3,4,5);
            for(int j=0;j<hands;++j) {
                const float angle=t*0.8f+Tau*j/hands;
                emit(boss,angle,160*speed,1,3,3.5f);
                if(difficulty!=Difficulty::Easy) emit(boss,angle+0.06f,143*speed,0,3,3.5f);
            }
        }
        if(ringClock<=0) {
            ringClock+=1.6f;
            const int n=density(difficulty,12,18,24);
            for(int j=0;j<n;++j) emit(boss,-t*0.31f+Tau*j/n,90*speed,2,0,4.5f);
        }
    } else if(stage==1 && phase==1) {
        // Knives coast, stop for three quarters of a second, then resume.
        if(patternClock<=0) {
            patternClock+=2.9f;
            const int n=density(difficulty,20,28,36);
            for(int j=0;j<n;++j) {
                const float angle=Tau*j/n+t*0.13f;
                emit(boss,angle,134*speed,4,3,3.5f);
                if(difficulty!=Difficulty::Easy) emit(boss,angle+0.025f,184*speed,1,3,3.5f);
            }
        }
        if(ringClock<=0) {
            ringClock+=1.6f;
            for(int side:{-1,1}) {
                Vec source{300+side*255.0f,100};
                const int n=density(difficulty,3,5,7);
                const float sideAim=angleTo(source,player);
                for(int j=0;j<n;++j) emit(source,sideAim+(j-(n-1)*0.5f)*0.16f,98*speed,2,0,4.2f);
            }
        }
    } else if(stage==1 && phase==2) {
        // Alternating diagonal curtains and deliberate aimed volleys.
        if(patternClock<=0) {
            patternClock+=density(difficulty,130,98,74)*0.01f;
            const int n=density(difficulty,8,11,14);
            const bool left=static_cast<int>(t/3.2f)%2==0;
            const int gap=static_cast<int>(t*0.5f)%(n-2);
            for(int j=0;j<n;++j) {
                if(j==gap||j==gap+1) continue;
                emit({(j+0.5f)*FieldW/n,-20},Pi*0.5f+(left?0.22f:-0.22f),
                     (150+(j%2)*12)*speed,1,3,3.6f);
            }
        }
        if(ringClock<=0) {
            ringClock+=1.5f;
            const int n=density(difficulty,5,7,9);
            for(int j=0;j<n;++j) emit(boss,aim+(j-(n-1)*0.5f)*0.11f,185*speed,2,3,3.5f);
        }
    } else if(stage==2 && phase==0) {
        // Left and right boundaries open in alternation, crossing below the boss.
        if(patternClock<=0) {
            patternClock+=density(difficulty,73,54,41)*0.01f;
            const int n=density(difficulty,5,7,9);
            const bool left=static_cast<int>(t/2.8f)%2==0;
            Vec source{left?18.0f:FieldW-18,125+50*std::sin(t*0.55f)};
            const float center=left?0.90f:Pi-0.90f;
            for(int j=0;j<n;++j) emit(source,center+(j-(n-1)*0.5f)*0.085f,135*speed,3,1,4.2f,
                                      left?0.025f:-0.025f);
        }
        if(ringClock<=0) {
            ringClock+=1.7f;
            const int n=density(difficulty,16,22,30);
            for(int j=0;j<n;++j) emit(boss,Tau*j/n+t*0.2f,92*speed,2,0,4.3f);
        }
    } else if(stage==2 && phase==1) {
        // Express trains are short horizontal rows, descending in open zigzags.
        if(patternClock<=0) {
            patternClock+=density(difficulty,105,78,58)*0.01f;
            const int carriages=density(difficulty,5,7,9);
            const bool left=static_cast<int>(t/3.4f)%2==0;
            const float y=90+std::fmod(t*19,170.0f);
            for(int j=0;j<carriages;++j) {
                Vec source{left?10.0f:590.0f,y-j*11};
                emit(source,left?0.55f:Pi-0.55f,(175+j*3)*speed,j%2==0?3:2,1,4.7f);
            }
        }
        if(ringClock<=0) {
            ringClock+=1.55f;
            const int n=density(difficulty,9,13,17);
            for(int j=0;j<n;++j) {
                const float angle=Pi*0.2f+Pi*0.6f*j/(n-1)+0.14f*std::sin(t);
                emit(boss,angle,114*speed,4,2,4.3f);
            }
        }
    } else if(stage==2 && phase==2) {
        // A slow lotus layers purple spirals with gold rings and sparse aimed petals.
        if(patternClock<=0) {
            patternClock+=density(difficulty,59,43,32)*0.01f;
            const int arms=density(difficulty,6,8,10);
            const float turn=0.085f*std::sin(t*0.24f);
            for(int j=0;j<arms;++j) {
                emit(boss,Tau*j/arms+t*0.37f,128*speed,3,1,4,turn);
                if(difficulty!=Difficulty::Easy) emit(boss,Tau*j/arms-t*0.25f,102*speed,1,0,4,-turn);
            }
        }
        if(ringClock<=0) {
            ringClock+=2.6f;
            const int n=density(difficulty,18,26,34);
            for(int j=0;j<n;++j) {
                const float a=Tau*j/n+t*0.12f;
                // A deliberately missing wedge keeps the layered rings navigable.
                if(std::abs(std::remainder(a-aim,Tau))<0.24f) continue;
                emit(boss,a,90*speed,2,0,4.5f);
            }
            const int fan=density(difficulty,3,5,7);
            for(int j=0;j<fan;++j) emit(boss,aim+(j-(fan-1)*0.5f)*0.18f,178*speed,4,1,4);
        }
    }
}

void Game::update(const Input& input,float dt) {
    if(mode!=Mode::Playing || !std::isfinite(dt) || dt<=0) return;
    dt=std::min(dt,1.0f/30.0f);
    time+=dt; stageTime+=dt;
    invulnerable=countdown(invulnerable,dt); bombTime=countdown(bombTime,dt);
    shake=countdown(shake,dt); bannerTime=countdown(bannerTime,dt);
    float mx=std::isfinite(input.x)?std::clamp(input.x,-1.0f,1.0f):0;
    float my=std::isfinite(input.y)?std::clamp(input.y,-1.0f,1.0f):0;
    const float magnitude=std::hypot(mx,my);
    if(magnitude>1) { mx/=magnitude; my/=magnitude; }
    const float movement=input.focus?118.0f:310.0f;
    player.x=std::clamp(player.x+mx*movement*dt,8.0f,FieldW-8);
    player.y=std::clamp(player.y+my*movement*dt,12.0f,FieldH-12);

    if(input.bomb&&!bombHeld) bomb();
    bombHeld=input.bomb;
    shotClock-=dt;
    if(input.fire && shotClock<=0) {
        shotClock=0.09f;
        ++sfxShot;
        if(shots.size()<300) {
            shots.push_back({{player.x,player.y-15},{0,-860},2.4f});
            for(int side:{-1,1}) {
                shots.push_back({{player.x+side*7.0f,player.y-7},{side*(input.focus?3.0f:15.0f),-825},0.65f});
                for(int level=1;level<std::clamp(power,1,4);++level) {
                    shots.push_back({{player.x+side*(10.0f+level*4),player.y-2},
                                     {side*(input.focus?5.0f+level*3:22.0f+level*21),-800},0.22f});
                }
            }
        }
    } else if(!input.fire) shotClock=std::max(shotClock,0.0f);

    if(bossActive) {
        phaseTime+=dt;
        if(bossHp<=0 || phaseTime>=SpellDuration) { nextPhase(); return; }
        updateBoss(dt);
    } else {
        if(stageTime>=WaveDuration) {
            bullets.clear(); shots.clear(); enemies.clear(); pickups.clear();
            dialogueIndex=2; mode=Mode::Dialogue; resumeMode=Mode::Dialogue;
            return;
        }
        updateWaves(dt);
    }

    for(auto& shot:shots) {
        shot.p.x+=shot.v.x*dt; shot.p.y+=shot.v.y*dt;
        if(bossActive && distanceSquared(shot.p,boss)<30*30) {
            bossHp-=shot.damage; shot.p.y=-1000; score+=5;
        } else if(!bossActive) {
            for(auto& enemy:enemies) {
                if(enemy.hp>0 && distanceSquared(shot.p,enemy.p)<20*20) {
                    enemy.hp-=shot.damage; shot.p.y=-1000; break;
                }
            }
        }
    }
    discard(shots,[](const Shot& shot){ return shot.p.y<-30||shot.p.y>FieldH+50||shot.p.x<-60||shot.p.x>FieldW+60; });
    for(const auto& enemy:enemies) if(enemy.hp<=0) {
        score+=1000; ++sfxHit; burst(enemy.p,16,enemy.kind);
        if(pickups.size()<500) {
            pickups.push_back({enemy.p,enemy.kind%2==0?0:1});
            pickups.push_back({{enemy.p.x+12,enemy.p.y},1});
        }
    }
    discard(enemies,[](const Enemy& enemy){ return enemy.hp<=0||enemy.age>18||enemy.p.y>FieldH+60||enemy.p.x<-120||enemy.p.x>FieldW+120; });

    bool hit=false;
    for(auto& bullet:bullets) {
        bullet.age+=dt;
        if(bullet.turn!=0) {
            const float a=bullet.turn*dt,c=std::cos(a),s=std::sin(a);
            bullet.v={bullet.v.x*c-bullet.v.y*s,bullet.v.x*s+bullet.v.y*c};
        }
        float movementFactor=1;
        if(bossActive && stage==1 && phase==1 && bullet.shape==3 && bullet.age>=1.05f && bullet.age<1.85f)
            movementFactor=0;
        bullet.p.x+=bullet.v.x*dt*movementFactor; bullet.p.y+=bullet.v.y*dt*movementFactor;
        const float distance=distanceSquared(bullet.p,player);
        const float hitRadius=PlayerRadius+bullet.radius;
        if(distance<hitRadius*hitRadius && invulnerable<=0 && bombTime<=0) hit=true;
        else if(!bullet.grazed && distance<(bullet.radius+24)*(bullet.radius+24) && invulnerable<=0 && bombTime<=0) {
            bullet.grazed=true; ++graze; score+=100; ++sfxGraze;
        }
    }
    discard(bullets,[](const Bullet& bullet){
        return bullet.age>20||bullet.p.x<-120||bullet.p.x>FieldW+120||bullet.p.y<-160||bullet.p.y>FieldH+120;
    });
    peakBullets=std::max(peakBullets,static_cast<int>(bullets.size()));
    if(hit) damagePlayer();

    for(auto& pickup:pickups) {
        const float distance=distanceSquared(pickup.p,player);
        const bool attract=player.y<250||distance<(input.focus?150*150:90*90)||bombTime>0;
        if(attract && distance>1) {
            const float inv=1/std::sqrt(distance);
            pickup.p.x+=(player.x-pickup.p.x)*inv*420*dt;
            pickup.p.y+=(player.y-pickup.p.y)*inv*420*dt;
        } else pickup.p.y+=65*dt;
        if(distanceSquared(pickup.p,player)<18*18) {
            if(pickup.kind==0) { power=std::min(4,power+1); score+=500; }
            else score+=2000;
            burst(pickup.p,5,2); pickup.p.y=FieldH+100;
        }
    }
    discard(pickups,[](const Pickup& pickup){return pickup.p.y>FieldH+60||pickup.p.x<-100||pickup.p.x>FieldW+100;});
    for(auto& particle:particles) {
        particle.life-=dt;
        particle.p.x+=particle.v.x*dt; particle.p.y+=particle.v.y*dt;
        particle.v.x*=std::max(0.0f,1-dt*2); particle.v.y*=std::max(0.0f,1-dt*2);
    }
    discard(particles,[](const Particle& particle){return particle.life<=0;});
    // Phase and stage transitions happen only after all entity iterations have ended.
    if(mode==Mode::Playing && bossActive && bossHp<=0) nextPhase();
}

bool Game::invariant() const {
    if(stage<0||stage>2||phase<-1||phase>2||lives<0||lives>99||bombs<0||bombs>99||power<1||power>4) return false;
    if(!finite(player)||!finite(boss)||player.x<8||player.x>FieldW-8||player.y<12||player.y>FieldH-12) return false;
    for(float value:{time,stageTime,phaseTime,bossHp,bossMaxHp,invulnerable,bombTime,shake,bannerTime,shotClock,spawnClock,patternClock,ringClock})
        if(!std::isfinite(value)) return false;
    if(bossMaxHp<=0||time<0||stageTime<0||phaseTime<0||invulnerable<0||bombTime<0) return false;
    if(bullets.size()>BulletLimit||shots.size()>320||enemies.size()>40||pickups.size()>502||particles.size()>800) return false;
    for(const auto& bullet:bullets) {
        if(!finite(bullet.p)||!finite(bullet.v)||!std::isfinite(bullet.age)||!std::isfinite(bullet.radius)||!std::isfinite(bullet.turn)) return false;
        if(bullet.radius<=0||bullet.age<0||bullet.age>21) return false;
        if(bullet.p.x<-121||bullet.p.x>FieldW+121||bullet.p.y<-161||bullet.p.y>FieldH+121) return false;
    }
    for(const auto& shot:shots) if(!finite(shot.p)||!finite(shot.v)||!std::isfinite(shot.damage)||shot.damage<=0) return false;
    for(const auto& enemy:enemies) if(!finite(enemy.p)||!finite(enemy.v)||!std::isfinite(enemy.hp)||!std::isfinite(enemy.age)||!std::isfinite(enemy.nextFire)) return false;
    for(const auto& pickup:pickups) if(!finite(pickup.p)||pickup.kind<0||pickup.kind>1) return false;
    for(const auto& particle:particles) if(!finite(particle.p)||!finite(particle.v)||!std::isfinite(particle.life)||!std::isfinite(particle.maxLife)||particle.maxLife<=0) return false;
    return true;
}
}
