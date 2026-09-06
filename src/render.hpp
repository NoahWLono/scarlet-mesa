#pragma once
#include "game.hpp"
#include <SDL.h>
#include <string>

namespace scarlet {
struct UI {
    int menu=0, page=0, selectedStage=0, highScore=0;
    Difficulty difficulty=Difficulty::Normal;
    bool muted=false, autoFire=false, focus=false;
    float clock=0;
};
class Renderer {
public:
    explicit Renderer(SDL_Renderer* r):r(r){}
    ~Renderer();
    bool init(const std::string& assets);
    void draw(const Game& g, const UI& ui);
    bool screenshot(const std::string& path);
private:
    SDL_Renderer* r;
    SDL_Texture* font=nullptr;
    SDL_Texture* displayFont=nullptr;
    SDL_Texture* art=nullptr;
    void rect(float x,float y,float w,float h,SDL_Color c);
    void line(float x,float y,float x2,float y2,SDL_Color c);
    void circle(float x,float y,float radius,SDL_Color c,bool fill=true);
    void poly(std::initializer_list<SDL_FPoint> p,SDL_Color c);
    void text(std::string s,float x,float y,float size,SDL_Color c);
    void wrap(const std::string& s,float x,float y,float size,int cols,SDL_Color c);
    void sprite(int who,float x,float y,float scale,float t);
    void scenery(const Game& g,float t);
    void title(const UI& ui);
    void field(const Game& g,const UI& ui);
    void sidebar(const Game& g,const UI& ui);
    void modal(const Game& g,const UI& ui);
};
}
