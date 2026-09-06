#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "audio.hpp"
#include "game.hpp"
#include "render.hpp"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace fs=std::filesystem;
using namespace scarlet;
namespace {
std::string assetDirectory(){
    std::vector<fs::path> candidates;
    if(const char* p=SDL_getenv("SCARLET_ASSET_DIR"))candidates.emplace_back(p);
    char* base=SDL_GetBasePath();
    if(base){fs::path b(base);SDL_free(base);candidates.push_back(b/"assets");candidates.push_back(b/"../Resources/assets");candidates.push_back(b/"../assets");candidates.push_back(b/"../share/scarlet-mesa/assets");}
    candidates.emplace_back("assets");
    for(const auto& p:candidates)if(fs::is_regular_file(p/"font.bmp")&&fs::is_regular_file(p/"title.bmp"))return fs::absolute(p).lexically_normal().string();
    throw std::runtime_error("Cannot find game assets. Keep the assets folder beside the executable, or set SCARLET_ASSET_DIR.");
}
struct Application {
    SDL_Window* window=nullptr;
    SDL_Renderer* device=nullptr;
    std::unique_ptr<Renderer> renderer;
    Audio audio;
    Game game;
    UI ui;
    std::array<bool,SDL_NUM_SCANCODES> held{};
    std::array<int,5> soundCounters{};
    bool running=true,fullscreen=false,testing=false;
    std::string recordPath;
    int musicStage=-1, savedScore=0;
    ~Application(){audio.shutdown();renderer.reset();SDL_DestroyRenderer(device);SDL_DestroyWindow(window);SDL_Quit();}
    void init(bool smoke){
        testing=smoke;
        SDL_SetMainReady();
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"linear");
        if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_TIMER)!=0)throw std::runtime_error(SDL_GetError());
        int width=1200,height=900;
        SDL_Rect usable{};
        if(!smoke&&SDL_GetDisplayUsableBounds(0,&usable)==0){float scale=std::min({1.f,float(usable.w-40)/1200,float(usable.h-60)/900});scale=std::max(.6f,scale);width=int(1200*scale);height=int(900*scale);}
        window=SDL_CreateWindow("Touhou: Scarlet Mesa - Unaligned in San Francisco",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,width,height,SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
        if(!window)throw std::runtime_error(SDL_GetError());
        SDL_SetWindowMinimumSize(window,720,540);
        device=SDL_CreateRenderer(window,-1,smoke?0:SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
        if(!device)device=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
        if(!device)throw std::runtime_error(SDL_GetError());
        if(testing){SDL_RendererInfo info{};SDL_GetRendererInfo(device,&info);std::cout<<"SDL driver: "<<SDL_GetCurrentVideoDriver()<<"; renderer: "<<info.name<<'\n';}
        SDL_RenderSetLogicalSize(device,1200,900);
        renderer=std::make_unique<Renderer>(device);
        const std::string assets=assetDirectory();
        if(!renderer->init(assets))throw std::runtime_error("Could not load title or font: "+std::string(SDL_GetError()));
        if(!audio.init(assets))std::cerr<<"Audio unavailable; continuing silently.\n";
        if(!testing){char* p=SDL_GetPrefPath("ScarletMesa","ScarletMesa");if(p){recordPath=std::string(p)+"records.txt";SDL_free(p);std::ifstream in(recordPath);int score=0,muted=0;if(in>>score>>muted){ui.highScore=std::clamp(score,0,999999999);ui.muted=muted!=0;}}}
        savedScore=ui.highScore;
        audio.setMuted(ui.muted);
        audio.music(0);
    }
    void save(){
        if(testing||recordPath.empty())return;
        // A read-only game directory is fine; SDL supplies the user data directory.
        const std::string temp=recordPath+".tmp";
        std::ofstream out(temp,std::ios::trunc);if(!out)return;
        out<<ui.highScore<<' '<<int(ui.muted)<<'\n';out.close();
        if(out){std::error_code ec;fs::rename(temp,recordPath,ec);if(ec)std::cerr<<"Could not save records: "<<ec.message()<<'\n';}
        savedScore=ui.highScore;
    }
    void start(int practice=-1){game.start(ui.difficulty,practice);held.fill(false);musicStage=-1;soundCounters={};}
    void backToTitle(){game.returnToTitle();ui.page=0;ui.menu=0;held.fill(false);audio.music(0);musicStage=-1;save();}
    void key(SDL_Scancode k){
        if(k==SDL_SCANCODE_M){ui.muted=!ui.muted;audio.setMuted(ui.muted);save();return;}
        if(k==SDL_SCANCODE_F11){fullscreen=!fullscreen;SDL_SetWindowFullscreen(window,fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);return;}
        if(k==SDL_SCANCODE_C){ui.autoFire=!ui.autoFire;return;}
        bool enter=k==SDL_SCANCODE_RETURN||k==SDL_SCANCODE_KP_ENTER||k==SDL_SCANCODE_Z||k==SDL_SCANCODE_SPACE;
        bool up=k==SDL_SCANCODE_UP||k==SDL_SCANCODE_W,down=k==SDL_SCANCODE_DOWN||k==SDL_SCANCODE_S;
        bool left=k==SDL_SCANCODE_LEFT||k==SDL_SCANCODE_A,right=k==SDL_SCANCODE_RIGHT||k==SDL_SCANCODE_D;
        if(game.mode==Mode::Title){
            if(ui.page==2){if(enter||k==SDL_SCANCODE_ESCAPE)ui.page=0;return;}
            if(ui.page==1){if(up||left)ui.selectedStage=(ui.selectedStage+2)%3;if(down||right)ui.selectedStage=(ui.selectedStage+1)%3;if(k==SDL_SCANCODE_ESCAPE)ui.page=0;if(enter)start(ui.selectedStage);return;}
            if(up)ui.menu=(ui.menu+3)%4;
            if(down)ui.menu=(ui.menu+1)%4;
            if(left)ui.difficulty=Difficulty((int(ui.difficulty)+2)%3);
            if(right)ui.difficulty=Difficulty((int(ui.difficulty)+1)%3);
            if(enter){if(ui.menu==0)start();else if(ui.menu==1)ui.page=1;else if(ui.menu==2)ui.page=2;else running=false;}
        }else if(game.mode==Mode::Dialogue){
            if(enter)game.advanceDialogue();
            if(k==SDL_SCANCODE_ESCAPE)game.pause();
        }else if(game.mode==Mode::Playing){if(k==SDL_SCANCODE_ESCAPE){game.pause();held.fill(false);}}
        else if(game.mode==Mode::Paused){if(k==SDL_SCANCODE_ESCAPE||enter)game.pause();else if(k==SDL_SCANCODE_R)start(game.practice?game.stage:-1);else if(k==SDL_SCANCODE_Q)backToTitle();}
        else if(game.mode==Mode::GameOver){if(enter){game.continueRun();held.fill(false);}else if(k==SDL_SCANCODE_R)start(game.practice?game.stage:-1);else if(k==SDL_SCANCODE_ESCAPE)backToTitle();}
        else if(game.mode==Mode::Victory){if(enter||k==SDL_SCANCODE_ESCAPE)backToTitle();}
    }
    void events(){
        SDL_Event e;
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT)running=false;
            if(e.type==SDL_KEYDOWN&&!e.key.repeat){held[e.key.keysym.scancode]=true;key(e.key.keysym.scancode);}
            if(e.type==SDL_KEYUP)held[e.key.keysym.scancode]=false;
            if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_FOCUS_LOST){held.fill(false);if(game.mode==Mode::Playing||game.mode==Mode::Dialogue)game.pause();}
        }
    }
    void step(){
        ui.clock+=Step;
        if(game.mode==Mode::Dialogue&&(held[SDL_SCANCODE_LCTRL]||held[SDL_SCANCODE_RCTRL])){game.advanceDialogue();}
        auto down=[&](SDL_Scancode a,SDL_Scancode b){return held[a]||held[b];};
        Input input;
        input.x=float(down(SDL_SCANCODE_RIGHT,SDL_SCANCODE_D))-float(down(SDL_SCANCODE_LEFT,SDL_SCANCODE_A));
        input.y=float(down(SDL_SCANCODE_DOWN,SDL_SCANCODE_S))-float(down(SDL_SCANCODE_UP,SDL_SCANCODE_W));
        input.fire=down(SDL_SCANCODE_Z,SDL_SCANCODE_SPACE)||ui.autoFire;
        input.focus=down(SDL_SCANCODE_LSHIFT,SDL_SCANCODE_RSHIFT);input.bomb=held[SDL_SCANCODE_X];ui.focus=input.focus;
        game.update(input);
        ui.highScore=std::max(ui.highScore,game.score);
        if(game.mode!=Mode::Title&&musicStage!=game.stage){audio.music(game.stage);musicStage=game.stage;}
        std::array<int,5> now={game.sfxShot,game.sfxHit,game.sfxBomb,game.sfxClear,game.sfxGraze};
        for(int i=0;i<5;++i)if(now[size_t(i)]>soundCounters[size_t(i)])audio.effect(i);
        soundCounters=now;
        if((game.mode==Mode::Victory||game.mode==Mode::GameOver)&&ui.highScore>savedScore)save();
    }
    void draw(){renderer->draw(game,ui);SDL_RenderPresent(device);}
    void run(){
        const double frequency=double(SDL_GetPerformanceFrequency());
        Uint64 previous=SDL_GetPerformanceCounter();double accumulator=0;
        while(running){
            Uint64 now=SDL_GetPerformanceCounter();double elapsed=std::min(.1,double(now-previous)/frequency);previous=now;
            events();accumulator+=elapsed;
            while(accumulator>=double(Step)){step();accumulator-=double(Step);}
            draw();SDL_Delay(1);
        }
        save();
    }
};
void require(bool value,const char* message){if(!value)throw std::runtime_error(std::string("SMOKE FAIL: ")+message);}
int smoke(Application& app,const fs::path& out){
    fs::create_directories(out);
    auto push=[&](SDL_Scancode code,bool down){SDL_Event e{};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.type=e.type;e.key.state=down?SDL_PRESSED:SDL_RELEASED;e.key.keysym.scancode=code;e.key.keysym.sym=SDL_GetKeyFromScancode(code);require(SDL_PushEvent(&e)==1,"queue keyboard event");app.events();};
    auto tap=[&](SDL_Scancode code){push(code,true);push(code,false);};
    auto frames=[&](int count){for(int i=0;i<count;++i){app.step();require(app.game.invariant(),"simulation invariants");}};
    auto capture=[&](const char* name){app.renderer->draw(app.game,app.ui);require(app.renderer->screenshot((out/(std::string(name)+".bmp")).string()),"save rendered screenshot");SDL_RenderPresent(app.device);};
    capture("title");
    tap(SDL_SCANCODE_DOWN);tap(SDL_SCANCODE_DOWN);tap(SDL_SCANCODE_RETURN);require(app.ui.page==2,"manual navigation");capture("manual");tap(SDL_SCANCODE_ESCAPE);
    tap(SDL_SCANCODE_UP);tap(SDL_SCANCODE_UP);tap(SDL_SCANCODE_LEFT);require(app.ui.difficulty==Difficulty::Easy,"difficulty selection");tap(SDL_SCANCODE_RETURN);require(app.game.mode==Mode::Dialogue,"story starts with dialogue");capture("dialogue");
    push(SDL_SCANCODE_LCTRL,true);frames(20);push(SDL_SCANCODE_LCTRL,false);require(app.game.mode==Mode::Playing,"skip dialogue through input");
    float initialX=app.game.player.x;push(SDL_SCANCODE_RIGHT,true);push(SDL_SCANCODE_Z,true);frames(50);push(SDL_SCANCODE_RIGHT,false);require(app.game.player.x>initialX+50,"normal movement");require(!app.game.shots.empty(),"held fire produces shots");
    float normalDistance=app.game.player.x-initialX;initialX=app.game.player.x;push(SDL_SCANCODE_LSHIFT,true);push(SDL_SCANCODE_LEFT,true);frames(50);push(SDL_SCANCODE_LEFT,false);require(initialX-app.game.player.x<normalDistance*.7f,"focus slows movement");
    tap(SDL_SCANCODE_ESCAPE);require(app.game.mode==Mode::Paused,"pause input");float pauseTime=app.game.time;frames(40);require(app.game.time==pauseTime,"paused simulation does not advance");capture("paused");tap(SDL_SCANCODE_ESCAPE);require(app.game.mode==Mode::Playing,"resume input");
    int bombs=app.game.bombs;push(SDL_SCANCODE_X,true);frames(30);require(app.game.bombs==bombs-1,"held bomb spends once");push(SDL_SCANCODE_X,false);
    frames(240);capture("gameplay");tap(SDL_SCANCODE_C);require(app.ui.autoFire,"auto-fire input");tap(SDL_SCANCODE_M);require(app.ui.muted,"mute input");tap(SDL_SCANCODE_M);
    push(SDL_SCANCODE_Z,false);push(SDL_SCANCODE_LSHIFT,false);
    for(int stage=0;stage<3;++stage){
        if(app.game.mode==Mode::Playing||app.game.mode==Mode::Dialogue)tap(SDL_SCANCODE_ESCAPE);
        if(app.game.mode==Mode::Paused)tap(SDL_SCANCODE_Q);else if(app.game.mode==Mode::GameOver)tap(SDL_SCANCODE_ESCAPE);else if(app.game.mode==Mode::Victory)tap(SDL_SCANCODE_RETURN);
        require(app.game.mode==Mode::Title,"return to menu");tap(SDL_SCANCODE_DOWN);tap(SDL_SCANCODE_RETURN);require(app.ui.page==1,"practice menu");
        while(app.ui.selectedStage!=stage)tap(SDL_SCANCODE_DOWN);
        tap(SDL_SCANCODE_RETURN);push(SDL_SCANCODE_LCTRL,true);frames(20);push(SDL_SCANCODE_LCTRL,false);
        require(app.game.practice&&app.game.stage==stage,"practice starts selected stage");
        int budget=4000;
        while(!app.game.bossActive&&budget-->0){if(app.game.mode==Mode::Dialogue)tap(SDL_SCANCODE_RETURN);frames(1);}
        require(app.game.bossActive,"practice boss encounter starts");
        if(app.ui.autoFire)tap(SDL_SCANCODE_C);
        push(SDL_SCANCODE_LSHIFT,true);frames(720);capture(stage==0?"cirno":stage==1?"sakuya":"yukari");push(SDL_SCANCODE_LSHIFT,false);
        require(!app.game.bullets.empty(),"boss emits live bullet patterns");
    }
    std::cout<<"SMOKE PASS: menus, difficulty, dialogue, movement, focus, shooting, pause, bombs, audio controls, all three practice encounters, rendering.\n";
    return 0;
}
}
int main(int argc,char** argv){
    bool testing=false;fs::path output="smoke-artifacts";
    for(int i=1;i<argc;++i){std::string arg=argv[i];if(arg=="--smoke-test"){testing=true;if(i+1<argc&&argv[i+1][0]!='-')output=argv[++i];}
        else if(arg=="--version"){std::cout<<"Scarlet Mesa 1.0.0 (C++17 / SDL2)\n";return 0;}
        else if(arg=="--help"){std::cout<<"Scarlet Mesa: a free Touhou fan game\nUsage: scarlet-mesa [--smoke-test [output-dir]] [--version]\nArrows/WASD move; Z/Space shoot; Shift focus; X bomb; Esc pause.\n";return 0;}
        else {std::cerr<<"Unknown option: "<<arg<<'\n';return 2;}}
    try{Application app;app.init(testing);if(testing)return smoke(app,output);app.run();}
    catch(const std::exception& e){std::cerr<<"Scarlet Mesa: "<<e.what()<<'\n';if(!testing)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Scarlet Mesa",e.what(),nullptr);return 1;}
    return 0;
}
