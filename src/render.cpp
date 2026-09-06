#include "render.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <sstream>

namespace scarlet {
namespace {
constexpr float Pi=3.14159265358979323846f;
constexpr SDL_Color Ink{13,15,32,255}, White{247,238,225,255}, Muted{142,158,178,255};
constexpr SDL_Color Coral{255,107,119,255}, Gold{249,203,135,255}, Mint{125,237,211,255};
constexpr SDL_Color Colors[]={{255,104,141,255},{114,235,255,255},{255,212,112,255},{193,149,255,255},{142,255,184,255}};
std::string number(int n,int width=8) { char b[40]; std::snprintf(b,sizeof(b),"%0*d",width,n); return b; }
const char* diffName(Difficulty d) {return d==Difficulty::Easy?"EASY":d==Difficulty::Normal?"NORMAL":"LUNATIC";}
SDL_Color alpha(SDL_Color c,int a) {c.a=static_cast<Uint8>(std::clamp(a,0,255));return c;}
}
Renderer::~Renderer(){SDL_DestroyTexture(font);SDL_DestroyTexture(displayFont);SDL_DestroyTexture(art);}
bool Renderer::init(const std::string& assets) {
    auto loadFont=[&](const char* name)->SDL_Texture*{
    auto* source=SDL_LoadBMP((assets+"/"+name).c_str());
    if(!source) return nullptr;
    auto* rgba=SDL_ConvertSurfaceFormat(source,SDL_PIXELFORMAT_RGBA32,0);
    SDL_FreeSurface(source);
    if(!rgba) return nullptr;
    SDL_LockSurface(rgba);
    for(int y=0;y<rgba->h;++y) for(int x=0;x<rgba->w;++x){
        auto* p=reinterpret_cast<Uint32*>(static_cast<Uint8*>(rgba->pixels)+y*rgba->pitch)+x;
        Uint8 red,green,blue,a;SDL_GetRGBA(*p,rgba->format,&red,&green,&blue,&a);
        *p=SDL_MapRGBA(rgba->format,255,255,255,red);
    }
    SDL_UnlockSurface(rgba);
    auto* texture=SDL_CreateTextureFromSurface(r,rgba);SDL_FreeSurface(rgba);
    if(texture)SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
    return texture;
    };
    font=loadFont("font.bmp");displayFont=loadFont("font-display.bmp");
    if(!font||!displayFont)return false;
    auto* image=SDL_LoadBMP((assets+"/title.bmp").c_str());
    if(image){art=SDL_CreateTextureFromSurface(r,image);SDL_FreeSurface(image);}
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    return art!=nullptr;
}
void Renderer::rect(float x,float y,float w,float h,SDL_Color c){SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);SDL_FRect a{x,y,w,h};SDL_RenderFillRectF(r,&a);}
void Renderer::line(float x,float y,float x2,float y2,SDL_Color c){SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);SDL_RenderDrawLineF(r,x,y,x2,y2);}
void Renderer::circle(float x,float y,float radius,SDL_Color c,bool fill){
    SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
    if(fill){for(int i=-int(radius);i<=int(radius);++i){float w=std::sqrt(std::max(0.f,radius*radius-float(i*i)));SDL_RenderDrawLineF(r,x-w,y+float(i),x+w,y+float(i));}}
    else {float px=x+radius,py=y;for(int i=1;i<=64;++i){float a=float(i)*2*Pi/64,nx=x+std::cos(a)*radius,ny=y+std::sin(a)*radius;line(px,py,nx,ny,c);px=nx;py=ny;}}
}
void Renderer::poly(std::initializer_list<SDL_FPoint> pts,SDL_Color c){
    std::vector<SDL_Vertex> v;for(auto p:pts)v.push_back({p,c,{0,0}});
    std::vector<int> indices;for(int i=1;i+1<int(v.size());++i){indices.push_back(0);indices.push_back(i);indices.push_back(i+1);}
    SDL_RenderGeometry(r,nullptr,v.data(),int(v.size()),indices.data(),int(indices.size()));
}
void Renderer::text(std::string s,float x,float y,float size,SDL_Color c){
    SDL_Texture* face=size>=35?displayFont:font;
    int scale=size>=35?4:1;
    SDL_SetTextureColorMod(face,c.r,c.g,c.b);SDL_SetTextureAlphaMod(face,c.a);
    const float advance=size*0.60f;float sx=x;
    for(unsigned char ch:s){if(ch=='\n'){y+=size*1.35f;x=sx;continue;} if(ch<32||ch>126)ch='?';int i=int(ch)-32;SDL_Rect src{(i%16)*32*scale,(i/16)*44*scale,32*scale,44*scale};SDL_FRect dst{x,y,size*32/44,size};SDL_RenderCopyF(r,face,&src,&dst);x+=advance;}
}
void Renderer::wrap(const std::string& s,float x,float y,float size,int cols,SDL_Color c){
    std::istringstream words(s);std::string word,row;
    while(words>>word){if(int(row.size()+word.size()+1)>cols&&!row.empty()){text(row,x,y,size,c);y+=size*1.35f;row.clear();}if(!row.empty())row+=' ';row+=word;}if(!row.empty())text(row,x,y,size,c);
}
void Renderer::sprite(int who,float x,float y,float sc,float t){
    // Original small character illustrations built from colored geometry.
    auto P=[&](float a,float b){return SDL_FPoint{x+a*sc,y+b*sc};};
    auto Q=[&](std::initializer_list<SDL_FPoint> p,SDL_Color c){std::vector<SDL_FPoint> v;for(auto a:p)v.push_back(P(a.x,a.y));std::vector<SDL_Vertex> z;for(auto a:v)z.push_back({a,c,{0,0}});std::vector<int> idx;for(int k=1;k+1<int(z.size());++k){idx.push_back(0);idx.push_back(k);idx.push_back(k+1);}SDL_RenderGeometry(r,nullptr,z.data(),int(z.size()),idx.data(),int(idx.size()));};
    auto C=[&](float a,float b,float rad,SDL_Color c){circle(x+a*sc,y+b*sc,rad*sc,c);};
    const SDL_Color skin{255,217,185,255}, red{204,44,72,255}, hair{41,30,52,255};
    float bob=std::sin(t*4)*2;
    y+=bob*sc;
    if(who==0){
        Q({{-15,-13},{-21,16},{-5,26},{10,22},{19,10},{13,-15}},hair);
        Q({{-8,1},{8,1},{19,26},{-19,26}},red);
        Q({{-19,23},{19,23},{23,29},{-23,29}},White);
        Q({{-8,2},{-15,0},{-28,16},{-13,19}},White);Q({{8,2},{15,0},{28,16},{13,19}},White);
        Q({{-7,1},{7,1},{4,12},{-4,12}},White);Q({{-3,9},{3,9},{0,18}},Gold);
        C(0,-10,10,skin);Q({{-11,-15},{-6,-21},{8,-20},{12,-11},{3,-14},{-1,-8},{-4,-14}},hair);
        Q({{-1,-21},{-18,-28},{-17,-15},{-1,-18}},red);Q({{1,-21},{18,-28},{17,-15},{1,-18}},red);
        C(-3,-8,1.2f,hair);C(4,-8,1.2f,hair);
        Q({{-12,29},{-4,29},{-5,35},{-13,35}},hair);Q({{4,29},{12,29},{13,35},{5,35}},hair);
        auto a=P(22,9),b=P(32,-24);line(a.x,a.y,b.x,b.y,Gold);
        Q({{31,-23},{40,-20},{34,-14},{41,-10},{31,-7},{32,-15},{27,-18}},White);
    } else if(who==1){
        Q({{-10,0},{-36,-19},{-31,4},{-14,11}},alpha(Mint,180));Q({{10,0},{36,-19},{31,4},{14,11}},alpha(Mint,180));
        Q({{-12,9},{-35,16},{-23,29},{-8,20}},alpha(Colors[1],180));Q({{12,9},{35,16},{23,29},{8,20}},alpha(Colors[1],180));
        Q({{-13,-14},{-18,7},{18,7},{12,-19}},Colors[1]);
        Q({{-8,0},{8,0},{20,28},{-20,28}},SDL_Color{47,92,192,255});Q({{-20,23},{20,23},{22,29},{-22,29}},White);
        Q({{-8,0},{-21,15},{-14,19},{0,6}},White);Q({{8,0},{21,15},{14,19},{0,6}},White);
        C(0,-10,10,skin);Q({{-11,-13},{-7,-22},{11,-19},{14,-5},{7,-12},{0,-8},{-4,-15}},SDL_Color{84,181,241,255});
        Q({{-1,-23},{-15,-31},{-14,-19},{-1,-20}},SDL_Color{30,63,151,255});Q({{1,-23},{15,-31},{14,-19},{1,-20}},SDL_Color{30,63,151,255});
        C(-3,-8,1,Ink);C(4,-8,1,Ink);Q({{-4,0},{4,0},{0,9}},red);
    }else if(who==2){
        Q({{-14,-15},{-18,11},{18,11},{13,-17}},Muted);
        Q({{-8,1},{8,1},{21,29},{-21,29}},SDL_Color{40,66,113,255});Q({{-7,5},{7,5},{13,28},{-13,28}},White);
        Q({{-10,1},{-24,14},{-18,18},{-5,6}},White);Q({{10,1},{24,14},{18,18},{5,6}},White);
        C(0,-10,10,skin);Q({{-12,-15},{-6,-22},{12,-19},{13,-3},{6,-12},{0,-8},{-3,-15},{-11,-5}},SDL_Color{182,198,212,255});
        Q({{-12,-21},{-9,-27},{-4,-25},{0,-29},{5,-25},{10,-27},{14,-20}},White);
        C(-3,-8,1,Ink);C(4,-8,1,Ink);Q({{-4,0},{4,0},{0,8}},Mint);
        Q({{24,8},{27,-10},{29,7},{27,15}},White);
    }else{
        for(int k=-1;k<=1;k+=2){float a=float(k);Q({{a*12,-12},{a*22,5},{a*20,24},{a*9,14}},Gold);}
        Q({{-11,0},{11,0},{25,31},{-25,31}},SDL_Color{122,62,171,255});Q({{-5,2},{5,2},{13,31},{-13,31}},White);
        Q({{-10,0},{-27,13},{-19,20},{0,8}},SDL_Color{182,129,223,255});Q({{10,0},{27,13},{19,20},{0,8}},SDL_Color{182,129,223,255});
        C(0,-10,11,skin);Q({{-12,-13},{-8,-23},{10,-20},{15,-6},{6,-13},{0,-7},{-4,-16},{-12,5}},Gold);
        Q({{-23,-21},{-9,-33},{12,-32},{23,-19}},White);Q({{-22,-21},{23,-19},{22,-15},{-21,-16}},SDL_Color{171,94,185,255});
        Q({{6,-26},{14,-31},{14,-21},{4,-21}},red);C(-3,-8,1,Ink);C(4,-8,1,Ink);
        Q({{23,11},{42,-3},{42,15}},White);
    }
}
void Renderer::title(const UI& ui){
    if(art){int w=0,h=0;SDL_QueryTexture(art,nullptr,nullptr,&w,&h);int scaledWidth=int(float(w)*900/float(h));SDL_Rect target{1200-scaledWidth,0,scaledWidth,900};SDL_RenderCopy(r,art,nullptr,&target);}
    else rect(0,0,1200,900,Ink);
    for(int x=0;x<830;++x)rect(float(x),0,1,900,{8,12,28,static_cast<Uint8>(std::clamp(238-x*205/830,0,255))});
    for(int y=680;y<900;++y)rect(0,float(y),1200,1,{8,12,28,static_cast<Uint8>((y-680)*195/220)});
    for(int k=0;k<28;++k){float t=ui.clock;float x=std::fmod(float(k*139)+t*(6+float(k%4)),1200.f),y=std::fmod(float(k*83)-t*14+9000,900.f);circle(x,y,1.4f,alpha(Gold,100));}
    line(64,66,100,66,Coral);text("A GENSOKYO / SAN FRANCISCO INCIDENT",113,53,18,Gold);
    text("TOUHOU",60,141,26,White);
    text("SCARLET",52,179,90,White);text("MESA",52,258,112,Coral);
    text("UNALIGNED IN SAN FRANCISCO",63,383,20,Gold);
    if(ui.page==0){
        text("A boundary error. A city of optimizers.",65,436,17,White);
        text("One shrine maiden with a shutdown button.",65,460,17,White);
        const char* options[]={"START INCIDENT","STAGE PRACTICE","FIELD MANUAL","QUIT TO REALITY"};
        for(int i=0;i<4;++i){float y=529+float(i)*48;if(i==ui.menu){rect(60,y-4,374,42,{25,24,47,225});rect(60,y-4,3,42,Coral);text(">",75,y+4,21,Coral);}text(options[i],102,y+3,23,i==ui.menu?White:Muted);text(number(i+1,2),377,y+6,15,i==ui.menu?Gold:Muted);}
        text("<  "+std::string(diffName(ui.difficulty))+"  >",64,745,19,Mint);
        text("LEFT / RIGHT: DIFFICULTY",64,777,13,Muted);
    }else if(ui.page==1){
        text("CHOOSE A PRACTICE STAGE",65,479,20,Gold);
        for(int i=0;i<3;++i){float y=528+float(i)*65;rect(60,y-4,450,56,i==ui.selectedStage?SDL_Color{32,32,55,240}:SDL_Color{15,19,36,175});text(number(i+1,2),75,y+10,23,i==ui.selectedStage?Coral:Muted);text(stageInfo(i).name,125,y,20,White);text(stageInfo(i).boss,126,y+27,15,Muted);}
        text("ENTER: FLY    ESC: BACK",65,753,16,Mint);
    }else{
        rect(60,474,557,331,{12,17,33,230});
        const char* keys[]={"ARROWS / WASD     Move","Z / SPACE        Shoot","SHIFT            Focus + show hitbox","X                Bomb: shutdown ritual","C                Toggle auto-fire","ESC              Pause / resume","M / F11          Mute / fullscreen"};
        for(int i=0;i<7;++i)text(keys[i],77,491+float(i)*29,17,i%2==0?White:Muted);
        text("GRAZE bullets. Collect P for power.",77,706,17,Gold);
        text("Only the tiny center dot can be hit.",77,734,17,Gold);
        text("ENTER / ESC: BACK",77,772,15,Mint);
    }
    text("FREE TOUHOU PROJECT FAN GAME  /  TEAM SHANGHAI ALICE",63,855,12,Muted);
    text(ui.muted?"M  SOUND OFF":"M  SOUND ON",1005,855,13,White);
}
void Renderer::scenery(const Game& g,float t){
    // 600 x 800 field at (40,50); layers stay dim for bullet contrast.
    for(int y=0;y<800;++y){float f=float(y)/800;SDL_Color c=g.stage==0?SDL_Color{Uint8(25+f*5),Uint8(28+f*14),Uint8(53+f*15),255}:g.stage==1?SDL_Color{Uint8(22+f*3),Uint8(21+f*13),Uint8(43+f*13),255}:SDL_Color{Uint8(31-f*12),Uint8(21+f*10),Uint8(54+f*5),255};rect(40,50+float(y),600,1,c);}
    circle(512,171,62,{190,164,142,22});circle(512,171,47,{247,213,151,25});
    for(int i=0;i<45;++i){float x=55+float((i*97)%570),y=65+std::fmod(float(i*71)+t*(6+float(i%3)),760.f);circle(x,y,i%4==0?1.5f:0.8f,{177,206,236,Uint8(30+i%4*12)});}
    if(g.stage==0){
        for(int j=0;j<2;++j){float y=std::fmod(t*22+float(j)*520,1040.f)-180;float x1=154,x2=515;
            rect(x1,y+50,13,310,{102,53,70,155});rect(x2,y+50,13,310,{102,53,70,155});
            for(int z=0;z<3;++z){rect(x1-12,y+64+float(z)*93,38,8,{138,73,82,135});rect(x2-12,y+64+float(z)*93,38,8,{138,73,82,135});}
            line(40,y+284,640,y+284,{137,79,95,140});line(40,y+294,640,y+294,{137,79,95,90});
            float lastX=40,lastY=y+190;for(int x=40;x<=640;x+=6){float xx=float(x),wave=std::sin((xx-160)/355*Pi);float yy=y+78+180*std::abs(wave);line(lastX,lastY,xx,yy,{157,89,103,130});if(x%24==16)line(xx,yy,xx,y+284,{157,89,103,70});lastX=xx;lastY=yy;}
        }
        for(int i=0;i<12;++i){float y=440+float(i)*36+std::fmod(t*10,36.f);line(40,y,640,y,{78,127,158,16});}
    }else if(g.stage==1){
        for(int i=0;i<18;++i){float x=48+float(i)*34;float h=float((i*73)%160)+90;float y=std::fmod(t*16+float(i%3)*265,940.f)-h;rect(x,y,27,h,{44,49,74,185});for(float yy=y+12;yy<y+h-6;yy+=18)for(int k=0;k<2;++k)rect(x+6+float(k)*11,yy,4,5,{223,168,104,42});}
        for(int i=0;i<12;++i){float y=50+std::fmod(t*36+float(i)*75,800.f);line(40,y,640,y,{113,113,176,17});}
        for(int i=0;i<9;++i)line(340,50,40+float(i)*75,850,{113,113,176,20});
    }else{
        for(int j=0;j<7;++j){float rad=std::fmod(float(j)*85+t*18,620.f);circle(340,300,rad,{158,110,218,Uint8(38*(1-rad/650))},false);}
        for(int i=0;i<12;++i){float a=float(i)*Pi/6+t*0.025f;line(340,300,340+std::cos(a)*800,300+std::sin(a)*800,{139,115,184,24});}
        for(int i=0;i<5;++i){float x=95+float(i)*118,y=80+std::fmod(t*25+float(i)*191,780.f);circle(x,y,35,{92,48,138,35});line(x-28,y,x+28,y,alpha(Colors[3],75));circle(x,y,5,alpha(Gold,65));}
    }
    for(int j=0;j<3;++j){float y=std::fmod(t*9+float(j)*300,960.f)-80;for(int k=0;k<38;++k)rect(40,y+float(k),600,1,{144,179,204,Uint8(6*std::sin(float(k)/38*Pi))});}
}
void Renderer::field(const Game& g,const UI& ui){
    SDL_Rect clip{40,50,600,800};SDL_RenderSetClipRect(r,&clip);scenery(g,ui.clock);
    if(g.bossActive){float bx=40+g.boss.x,by=50+g.boss.y;circle(bx,by,64,{130,118,186,30});circle(bx,by,53,alpha(Colors[g.stage==0?1:g.stage==1?2:3],80),false);
        for(int i=0;i<8;++i){float a=float(i)*Pi/4+ui.clock*.4f;float b=a+Pi*3/4;line(bx+std::cos(a)*57,by+std::sin(a)*57,bx+std::cos(b)*57,by+std::sin(b)*57,{205,186,242,55});}
        sprite(g.stage+1,bx,by,1.1f,ui.clock);
    }
    for(const auto& e:g.enemies){float x=e.p.x+40,y=e.p.y+50;circle(x,y,19,{132,89,155,24});poly({{x-7,y},{x-24,y-10},{x-18,y+8}},alpha(Colors[(e.kind+1)%5],170));poly({{x+7,y},{x+24,y-10},{x+18,y+8}},alpha(Colors[(e.kind+1)%5],170));poly({{x,y-13},{x+10,y+10},{x,y+16},{x-10,y+10}},Colors[(e.kind+1)%5]);circle(x,y-4,5,White);}
    for(const auto& s:g.shots){float x=s.p.x+40,y=s.p.y+50;rect(x-5,y-15,10,25,{255,128,155,30});rect(x-2,y-12,4,20,White);rect(x-1,y-9,2,16,Mint);}
    for(const auto& p:g.pickups){float x=p.p.x+40,y=p.p.y+50;SDL_Color c=p.kind==0?Coral:Mint;rect(x-9,y-9,18,18,alpha(Ink,210));line(x-9,y-9,x+9,y-9,c);line(x-9,y+9,x+9,y+9,c);text(p.kind==0?"P":"+",x-6,y-10,19,c);}
    for(const auto& b:g.bullets){float x=b.p.x+40,y=b.p.y+50;SDL_Color c=Colors[(b.color%5+5)%5];float rad=b.radius;
        if(b.shape==1||b.shape==3){float a=std::atan2(b.v.y,b.v.x),dx=std::cos(a),dy=std::sin(a);poly({{x+dx*rad*1.9f,y+dy*rad*1.9f},{x-dy*rad*.65f,y+dx*rad*.65f},{x-dx*rad*1.9f,y-dy*rad*1.9f},{x+dy*rad*.65f,y-dx*rad*.65f}},c);line(x-dx*rad,y-dy*rad,x+dx*rad,y+dy*rad,White);}
        else if(b.shape==2){poly({{x,y-rad*1.5f},{x+rad,y},{x,y+rad*1.5f},{x-rad,y}},c);circle(x,y,rad*.35f,White);}
        else{circle(x,y,rad+2,alpha(c,40));circle(x,y,rad,c);circle(x,y,std::max(1.f,rad-2),White);circle(x,y,std::max(1.f,rad-3),alpha(c,150));}
    }
    if(g.invulnerable<=0||int(ui.clock*16)%2==0)sprite(0,g.player.x+40,g.player.y+50,.76f,ui.clock);
    float px=g.player.x+40,py=g.player.y+50;
    for(int j=-1;j<=1;j+=2){float x=px+float(j)*(ui.focus?20:36),y=py+std::sin(ui.clock*5)*4;circle(x,y,7,alpha(Coral,80));circle(x,y,4,White);circle(x,y,2,Coral);}
    if(ui.focus){circle(px,py,22,alpha(White,90),false);circle(px,py,5,Ink);circle(px,py,3,White);circle(px,py,1,Coral);}
    for(const auto& p:g.particles){circle(p.p.x+40,p.p.y+50,2.2f,alpha(Colors[(p.color%5+5)%5],int(255*p.life/std::max(.01f,p.maxLife))));}
    if(g.bombTime>0){float progress=(1.6f-g.bombTime)/1.6f;circle(px,py,progress*760,alpha(Coral,150),false);circle(px,py,progress*590,alpha(Gold,180),false);rect(40,50,600,800,alpha(White,int(g.bombTime*16)));text("SHUTDOWN RITUAL",162,435,27,White);text("HUMAN STILL IN THE LOOP",161,474,19,Gold);}
    if(g.bossActive){rect(53,65,573,6,{16,17,32,230});rect(53,65,573*std::clamp(g.bossHp/g.bossMaxHp,0.f,1.f),6,Coral);text(stageInfo(g.stage).boss,54,80,15,White);std::string spell=stageInfo(g.stage).spells[std::clamp(g.phase,0,2)];text(spell,54,107,15,Gold);text(number(std::max(0,38-int(g.phaseTime)),2),582,80,23,White);}
    if(g.bannerTime>0&&g.mode==Mode::Playing){float y=300;rect(65,y,550,107,{16,18,36,220});
        text(g.bossActive?"SPELL CARD "+number(g.phase+1,2):"STAGE "+number(g.stage+1,2),90,y+12,15,Coral);
        if(g.bossActive)wrap(stageInfo(g.stage).spells[std::clamp(g.phase,0,2)],90,y+40,20,41,White);
        else{text(stageInfo(g.stage).name,90,y+38,25,White);text(stageInfo(g.stage).subtitle,90,y+77,14,Gold);}
    }
    SDL_RenderSetClipRect(r,nullptr);
    line(39,49,641,49,{102,91,121,255});line(39,851,641,851,{102,91,121,255});line(39,49,39,851,{102,91,121,255});line(641,49,641,851,{102,91,121,255});
    if(g.bossActive){float x=std::clamp(g.boss.x+40,55.f,626.f);poly({{x,851},{x-5,860},{x+5,860}},Coral);text("BOSS",x-15,862,11,Coral);}
}
void Renderer::sidebar(const Game& g,const UI& ui){
    text("TOUHOU / SCARLET MESA",688,48,17,Gold);text("UNALIGNED",683,81,40,White);text("IN SAN FRANCISCO",688,130,20,Coral);
    line(690,183,1150,183,{61,57,82,255});
    text("HIGH SCORE",690,207,14,Muted);text(number(std::max(g.score,ui.highScore)),934,200,25,Gold);
    text("SCORE",690,256,14,Muted);text(number(g.score),922,247,29,White);
    text("REIMU",690,314,14,Muted);for(int i=0;i<std::min(g.lives,9);++i){float x=832+float(i)*32;poly({{x,322},{x+7,315},{x+14,322},{x+7,333}},Coral);}
    text("BOMBS",690,360,14,Muted);for(int i=0;i<std::min(g.bombs,9);++i)circle(839+float(i)*32,371,5,Mint,true);
    text("POWER",690,406,14,Muted);for(int i=0;i<4;++i)rect(831+float(i)*49,414,38,5,i<g.power?Gold:SDL_Color{57,53,75,255});text(std::to_string(g.power)+" / 4",1057,403,19,Gold);
    text("GRAZE",690,453,14,Muted);text(number(g.graze,5),831,448,22,Mint);
    text("SPELL CAPTURES",690,498,14,Muted);text(number(g.captures,2)+" / 09",980,492,22,Gold);
    line(690,550,1150,550,{61,57,82,255});
    text("INCIDENT REPORT",690,573,14,Coral);text(stageInfo(g.stage).name,688,601,22,White);
    const char* jokes[]={"The fog is a distribution shift.","Time stopped. The evals kept running.","The boundary was the training set."};
    wrap(jokes[g.stage],690,641,16,43,Muted);
    text(std::string(diffName(g.difficulty))+(g.practice?" / PRACTICE":" / STORY"),690,699,16,Mint);
    text(ui.autoFire?"C  AUTO-FIRE ON":"C  AUTO-FIRE OFF",690,737,13,Muted);
    text("Z SHOOT   SHIFT FOCUS   X BOMB",690,763,14,White);
    text("ESC PAUSE   M SOUND   F11 FULLSCREEN",690,790,12,Muted);
    text("P(DOOM): MOSTLY BULLETS",690,838,13,Coral);
    text("SCARLET MESA  /  FREE PLAY",43,24,11,Muted);
}
void Renderer::modal(const Game& g,const UI& ui){
    if(g.mode==Mode::Dialogue){
        const auto& d=stageInfo(g.stage).dialogue[std::clamp(g.dialogueIndex,0,int(stageInfo(g.stage).dialogue.size())-1)];
        int who=d.speaker.find("Reimu")!=std::string::npos?0:g.stage+1;
        rect(55,587,570,245,{10,14,29,243});rect(55,587,3,245,Coral);sprite(who,117,682,1.8f,ui.clock);
        text(d.speaker,182,610,20,Gold);wrap(d.text,181,650,19,36,White);
        text("Z / ENTER: NEXT   CTRL: SKIP",181,789,12,Mint);
    }else if(g.mode==Mode::Paused||g.mode==Mode::GameOver||g.mode==Mode::Victory){
        rect(40,50,600,800,{8,11,26,205});rect(79,266,522,318,{18,22,41,248});line(79,266,601,266,Coral);
        if(g.mode==Mode::Paused){text("HUMAN IN THE LOOP",113,295,29,White);text("THE MODEL CAN WAIT.",129,345,18,Gold);text("ESC / ENTER    Resume",126,404,19,White);text("R              Restart run",126,448,19,Muted);text("Q              Main menu",126,492,19,Muted);}
        else if(g.mode==Mode::GameOver){text("ALIGNMENT FAILURE",113,295,29,Coral);text("THE LOSS FUNCTION WAS YOU.",108,345,18,Gold);text("ENTER    Continue (score resets)",105,405,17,White);text("R        Restart the incident",105,450,17,Muted);text("ESC      Main menu",105,494,17,Muted);}
        else {text(g.practice?"PRACTICE COMPLETE":"INCIDENT RESOLVED",103,291,29,Mint);text("THE BAY IS STILL IN ONE PIECE.",103,335,17,Gold);wrap(g.practice?"A successful local eval. Surely it generalizes.":"Yukari agrees to keep the gap offline. Reimu sends the accelerator an invoice for shrine alignment.",105,378,18,42,White);text("SCORE "+number(g.score)+"  /  CAPTURES "+number(g.captures,2),105,479,16,Gold);text("ENTER: RETURN TO GENSOKYO",105,535,16,Mint);}
    }
}
void Renderer::draw(const Game& g,const UI& ui){
    SDL_SetRenderDrawColor(r,Ink.r,Ink.g,Ink.b,255);SDL_RenderClear(r);
    if(g.mode==Mode::Title)title(ui);else{field(g,ui);sidebar(g,ui);modal(g,ui);}
}
bool Renderer::screenshot(const std::string& path){
    int w=0,h=0;SDL_GetRendererOutputSize(r,&w,&h);if(w<=0||h<=0)return false;
    auto* s=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);if(!s)return false;
    bool ok=SDL_RenderReadPixels(r,nullptr,s->format->format,s->pixels,s->pitch)==0&&SDL_SaveBMP(s,path.c_str())==0;SDL_FreeSurface(s);return ok;
}
}
