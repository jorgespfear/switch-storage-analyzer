#include <switch.h>
#include <SDL.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr int W = 1280, H = 720;
constexpr size_t MAX_NODES = 100000;
struct Node {
    std::string name, path;
    uint64_t bytes = 0;
    bool directory = false;
    std::vector<std::unique_ptr<Node>> children;
};
struct Palette { SDL_Color bg, panel, panel2, text, muted, accent, cyan, green, orange, red, border; };
const Palette C = {
    {12,14,24,255},{22,26,42,255},{30,36,56,255},{239,242,250,255},
    {145,155,178,255},{151,110,255,255},{63,190,235,255},{65,205,150,255},
    {255,174,82,255},{244,92,112,255},{53,61,86,255}
};
SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
std::unique_ptr<Node> root;
Node* current = nullptr;
std::vector<Node*> visible;
int selected = 0, scroll = 0;
bool treemapMode = true;
int sortMode = 0;
std::string statusText = "Preparado";
size_t scanned = 0;
bool scanLimitHit = false;

void color(SDL_Color c) { SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a); }
void fill(int x,int y,int w,int h, SDL_Color c) {
    color(c); SDL_Rect r{x,y,w,h}; SDL_RenderFillRect(renderer,&r);
}
void line(int x1,int y1,int x2,int y2, SDL_Color c) {
    color(c); SDL_RenderDrawLine(renderer,x1,y1,x2,y2);
}
void rect(int x,int y,int w,int h, SDL_Color c) {
    color(c); SDL_Rect r{x,y,w,h}; SDL_RenderDrawRect(renderer,&r);
}
const uint8_t* glyph(char ch) {
    static const uint8_t blank[7]={0,0,0,0,0,0,0};
    // 5x7 bitmap font, uppercase ASCII.
    #define G(a,b,c,d,e,f,g) { static const uint8_t v[7]={a,b,c,d,e,f,g}; return v; }
    switch(ch) {
    case 'A': G(14,17,17,31,17,17,17); case 'B': G(30,17,17,30,17,17,30);
    case 'C': G(14,17,16,16,16,17,14); case 'D': G(30,17,17,17,17,17,30);
    case 'E': G(31,16,16,30,16,16,31); case 'F': G(31,16,16,30,16,16,16);
    case 'G': G(14,17,16,23,17,17,15); case 'H': G(17,17,17,31,17,17,17);
    case 'I': G(14,4,4,4,4,4,14); case 'J': G(7,2,2,2,18,18,12);
    case 'K': G(17,18,20,24,20,18,17); case 'L': G(16,16,16,16,16,16,31);
    case 'M': G(17,27,21,21,17,17,17); case 'N': G(17,25,21,19,17,17,17);
    case 'O': G(14,17,17,17,17,17,14); case 'P': G(30,17,17,30,16,16,16);
    case 'Q': G(14,17,17,17,21,18,13); case 'R': G(30,17,17,30,20,18,17);
    case 'S': G(15,16,16,14,1,1,30); case 'T': G(31,4,4,4,4,4,4);
    case 'U': G(17,17,17,17,17,17,14); case 'V': G(17,17,17,17,17,10,4);
    case 'W': G(17,17,17,21,21,21,10); case 'X': G(17,17,10,4,10,17,17);
    case 'Y': G(17,17,10,4,4,4,4); case 'Z': G(31,1,2,4,8,16,31);
    case '0': G(14,17,19,21,25,17,14); case '1': G(4,12,4,4,4,4,14);
    case '2': G(14,17,1,2,4,8,31); case '3': G(30,1,1,14,1,1,30);
    case '4': G(2,6,10,18,31,2,2); case '5': G(31,16,16,30,1,1,30);
    case '6': G(14,16,16,30,17,17,14); case '7': G(31,1,2,4,8,8,8);
    case '8': G(14,17,17,14,17,17,14); case '9': G(14,17,17,15,1,1,14);
    case ':': G(0,4,4,0,4,4,0); case '.': G(0,0,0,0,0,12,12);
    case ',': G(0,0,0,0,4,4,8); case '-': G(0,0,0,31,0,0,0);
    case '_': G(0,0,0,0,0,0,31); case '/': G(1,2,2,4,8,8,16);
    case '%': G(17,2,4,8,17,0,0); case '+': G(0,4,4,31,4,4,0);
    case '(': G(2,4,8,8,8,4,2); case ')': G(8,4,2,2,2,4,8);
    case '[': G(14,8,8,8,8,8,14); case ']': G(14,2,2,2,2,2,14);
    case '?': G(14,17,1,2,4,0,4); case '!': G(4,4,4,4,4,0,4);
    case ' ': return blank; case '>': G(16,8,4,2,4,8,16);
    case '<': G(1,2,4,8,4,2,1); case '=': G(0,31,0,31,0,0,0);
    case '*': G(0,21,14,31,14,21,0); case '#': G(10,31,10,10,31,10,0);
    case '|': G(4,4,4,4,4,4,4); case '\'': G(4,4,8,0,0,0,0);
    case '"': G(10,10,0,0,0,0,0); case '~': G(0,0,9,22,0,0,0);
    default: return blank;
    }
    #undef G
}
void text(int x,int y,const std::string& s, SDL_Color c, int scale=2, int maxWidth=1000) {
    color(c); int ox=x;
    for(unsigned char raw : s) {
        char ch = raw;
        if(ch >= 'a' && ch <= 'z') ch = char(ch - 32);
        if(ch=='\n') { y += 9*scale; x=ox; continue; }
        if(ch < 32 || ch > 126) ch='?';
        if(x-ox+6*scale > maxWidth) break;
        const uint8_t* bits=glyph(ch);
        for(int row=0;row<7;row++) for(int col=0;col<5;col++)
            if(bits[row] & (1 << (4-col))) {
                SDL_Rect p{x+col*scale,y+row*scale,scale,scale};
                SDL_RenderFillRect(renderer,&p);
            }
        x += 6*scale;
    }
}
std::string human(uint64_t n) {
    const char* units[]={"B","KB","MB","GB","TB"};
    double v=(double)n; int u=0;
    while(v>=1024.0 && u<4){v/=1024.0;u++;}
    char b[48]; std::snprintf(b,sizeof(b),u==0?"%.0f %s":"%.2f %s",v,units[u]);
    return b;
}
std::string trimName(const std::string& s, size_t n=26) {
    if(s.size()<=n) return s;
    return s.substr(0,n>3?n-3:0)+"...";
}
void scanProgress(int done);
std::unique_ptr<Node> scanDir(const std::string& path,const std::string& name,
                              size_t& count, SDL_Event* unused=nullptr) {
    (void)unused;
    auto node=std::make_unique<Node>(); node->name=name; node->path=path; node->directory=true;
    DIR* d=opendir(path.c_str());
    if(!d) return node;
    dirent* ent;
    while((ent=readdir(d))!=nullptr) {
        if(!std::strcmp(ent->d_name,".") || !std::strcmp(ent->d_name,"..")) continue;
        if(count>=MAX_NODES){scanLimitHit=true;break;}
        std::string childPath=path;
        if(!childPath.empty() && childPath.back()!='/') childPath+='/';
        childPath+=ent->d_name;
        struct stat st{};
        if(lstat(childPath.c_str(),&st)!=0) continue;
        if(S_ISLNK(st.st_mode)) continue; // Never follow links.
        count++;
        if((count % 512)==0) scanProgress((int)count);
        auto child=std::make_unique<Node>();
        child->name=ent->d_name; child->path=childPath;
        child->directory=S_ISDIR(st.st_mode);
        if(child->directory) {
            child=scanDir(childPath,ent->d_name,count);
        } else {
            child->bytes=st.st_size>0?(uint64_t)st.st_size:0;
        }
        node->bytes += child->bytes;
        node->children.push_back(std::move(child));
    }
    closedir(d);
    std::sort(node->children.begin(),node->children.end(),[](const auto& a,const auto& b){
        if(a->directory!=b->directory) return a->directory>b->directory;
        return a->bytes>b->bytes;
    });
    return node;
}
void scanProgress(int done) {
    SDL_Event e; while(SDL_PollEvent(&e)) if(e.type==SDL_QUIT) { /* finish scan safely */ }
    fill(0,0,W,H,C.bg);
    text(70,110,"SWITCH STORAGE ANALYZER",C.text,4);
    text(70,190,"SCANNING MICROSD...",C.accent,3);
    fill(70,260,1100,22,C.panel2);
    fill(70,260,(int)(1100.0*std::min(done,(int)MAX_NODES)/(double)MAX_NODES),22,C.accent);
    text(70,310,"ENTRIES VISITED: "+std::to_string(done),C.muted,2);
    text(70,360,"PLEASE WAIT - LARGE CARDS MAY TAKE A WHILE",C.muted,2);
    SDL_RenderPresent(renderer);
}
void refreshVisible() {
    visible.clear();
    if(!current) return;
    for(auto& n:current->children) visible.push_back(n.get());
    std::sort(visible.begin(),visible.end(),[](Node* a,Node* b){
        if(sortMode==1) return a->name<b->name;
        if(sortMode==2) return a->bytes<b->bytes;
        if(a->directory!=b->directory) return a->directory>b->directory;
        return a->bytes>b->bytes;
    });
    selected=std::max(0,std::min(selected,(int)visible.size()-1));
    if(selected<scroll) scroll=selected;
    if(selected>=scroll+12) scroll=selected-11;
}
void donut(int cx,int cy,int outer,int inner,const std::vector<Node*>& items,uint64_t total) {
    if(total==0) return;
    SDL_Color cols[]={C.accent,C.cyan,C.green,C.orange,C.red,{180,130,220,255}};
    double start=-1.57079632679;
    int shown=std::min((int)items.size(),6);
    uint64_t used=0; for(int i=0;i<shown;i++) used+=items[i]->bytes;
    if(used==0) return;
    for(int i=0;i<shown;i++) {
        double span=6.28318530718*(double)items[i]->bytes/(double)total;
        if(i==shown-1 && shown<=(int)items.size()) span=std::max(0.0,6.28318530718*(double)used/(double)total - (start+1.57079632679));
        double end=start+span;
        for(int r=inner;r<=outer;r+=2) {
            for(double a=start;a<end;a+=0.012) {
                int x1=cx+(int)(std::cos(a)*r), y1=cy+(int)(std::sin(a)*r);
                int x2=cx+(int)(std::cos(std::min(a+0.012,end))*r), y2=cy+(int)(std::sin(std::min(a+0.012,end))*r);
                color(cols[i%6]); SDL_RenderDrawLine(renderer,x1,y1,x2,y2);
            }
        }
        start=end;
    }
    text(cx-45,cy-10,human(used),C.text,2,130);
}
void drawTreemap(int x,int y,int w,int h,const std::vector<Node*>& items) {
    uint64_t total=0; for(auto* n:items) total+=n->bytes;
    if(total==0) { text(x+15,y+20,"NO DATA TO DISPLAY",C.muted,2); return; }
    int cur=x, gap=5, usable=w-gap*(int)items.size();
    SDL_Color cols[]={C.accent,C.cyan,C.green,C.orange,C.red,{180,130,220,255}};
    for(size_t i=0;i<items.size();i++) {
        int ww=(i+1==items.size())?x+w-cur:(int)((double)usable*items[i]->bytes/(double)total);
        if(ww<2) continue;
        fill(cur,y,ww,h,cols[i%6]); rect(cur,y,ww,h,C.bg);
        if(ww>78) {
            text(cur+8,y+10,trimName(items[i]->name,14),{255,255,255,255},1,ww-12);
            if(h>55) text(cur+8,y+27,human(items[i]->bytes),{255,255,255,255},1,ww-12);
        }
        cur+=ww+gap;
    }
}
void draw() {
    fill(0,0,W,H,C.bg);
    fill(0,0,W,78,C.panel);
    text(34,22,"SWITCH STORAGE ANALYZER",C.text,3);
    text(930,27,"ATMOSPHERE / SDMC",C.muted,2);
    // Capacity panel
    fill(28,98,1224,105,C.panel);
    struct statvfs vfs{}; uint64_t total=0, freeB=0;
    if(statvfs("sdmc:/",&vfs)==0){ total=(uint64_t)vfs.f_blocks*vfs.f_frsize; freeB=(uint64_t)vfs.f_bavail*vfs.f_frsize; }
    uint64_t used=total>freeB?total-freeB:(root?root->bytes:0);
    text(52,116,"MICROSD",C.muted,2);
    text(52,148,human(total),C.text,3);
    text(350,116,"USED",C.muted,2); text(350,148,human(used),C.accent,3);
    text(620,116,"FREE",C.muted,2); text(620,148,human(freeB),C.green,3);
    int pct=total?std::min(100,(int)(used*100/total)):0;
    fill(890,137,320,18,C.panel2); fill(890,137,320*pct/100,18,C.accent);
    text(890,164,std::to_string(pct)+"% USED",C.muted,1);
    // Main panels
    fill(28,222,430,420,C.panel);
    fill(476,222,776,420,C.panel);
    text(50,242,"FOLDERS / FILES",C.text,2);
    text(50,270,trimName(current?current->path:"sdmc:/",54),C.muted,1);
    int start=scroll;
    for(int i=start;i<(int)visible.size() && i<start+12;i++) {
        int yy=300+(i-start)*27;
        bool sel=i==selected;
        if(sel) fill(42,yy-4,400,25,C.panel2);
        Node* n=visible[i];
        text(54,yy,n->directory?"[D]":"[F]",n->directory?C.cyan:C.orange,1);
        text(90,yy,trimName(n->name,29),sel?C.text:C.muted,1,225);
        text(330,yy,human(n->bytes),sel?C.text:C.muted,1,100);
    }
    text(50,610,"A OPEN  B UP  X VIEW  Y SORT",C.muted,1);
    text(500,242,treemapMode?"TREEMAP - SIZE BY AREA":"DONUT - SPACE DISTRIBUTION",C.text,2);
    if(treemapMode) {
        std::vector<Node*> top;
        for(size_t i=0;i<visible.size() && i<8;i++) top.push_back(visible[i]);
        drawTreemap(500,290,720,260,top);
    } else {
        std::vector<Node*> top;
        for(size_t i=0;i<visible.size() && i<6;i++) top.push_back(visible[i]);
        uint64_t chartTotal=0; for(auto* n:top) chartTotal+=n->bytes;
        donut(760,430,130,78,top,chartTotal);
        SDL_Color cols[]={C.accent,C.cyan,C.green,C.orange,C.red,{180,130,220,255}};
        for(size_t i=0;i<top.size();i++) {
            int yy=300+(int)i*43;
            fill(960,yy,16,16,cols[i%6]);
            text(988,yy,trimName(top[i]->name,22),C.text,1,190);
            text(988,yy+17,human(top[i]->bytes),C.muted,1,150);
        }
    }
    text(500,580,"Y SORT: "+std::string(sortMode==0?"SIZE":sortMode==1?"NAME":"SMALLEST"),C.muted,1);
    text(500,608,"X SWITCH CHART  PLUS RESCAN  MINUS EXIT",C.muted,1);
    fill(28,660,1224,36,C.panel);
    text(48,672,statusText,C.muted,1,1160);
    SDL_RenderPresent(renderer);
}
void enterSelected() {
    if(selected<0 || selected>=(int)visible.size()) return;
    Node* n=visible[selected];
    if(!n->directory) { statusText="FILE: "+n->name+" - "+human(n->bytes); return; }
    current=n; selected=0; scroll=0; refreshVisible();
}
void goUp() {
    if(!current || current==root.get()) return;
    std::string p=current->path;
    size_t pos=p.find_last_of('/');
    if(pos==std::string::npos || pos<5) { current=root.get(); }
    else {
        p=p.substr(0,pos);
        std::function<Node*(Node*)> find=[&](Node* n)->Node* {
            if(n->path==p) return n;
            for(auto& c:n->children) if(c->directory) if(auto* r=find(c.get())) return r;
            return nullptr;
        };
        Node* parent=find(root.get()); current=parent?parent:root.get();
    }
    selected=0; scroll=0; refreshVisible();
}
}

int main(int argc,char** argv) {
    (void)argc; (void)argv;
    if(SDL_Init(SDL_INIT_VIDEO)!=0) {
        return 1;
    }
    window=SDL_CreateWindow("Switch Storage Analyzer",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W,H,SDL_WINDOW_SHOWN);
    if(!window){SDL_Quit();return 1;}
    renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!renderer) renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    if(!renderer){SDL_DestroyWindow(window);SDL_Quit();return 1;}
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    scanProgress(0);
    size_t count=0;
    root=scanDir("sdmc:/","microSD",count);
    scanned=count; current=root.get(); refreshVisible();
    statusText="SCAN COMPLETE: "+std::to_string(scanned)+" ENTRIES"+(scanLimitHit?" (LIMIT REACHED)":"");
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    bool running=true;
    while(running) {
        SDL_Event ev;
        while(SDL_PollEvent(&ev)) if(ev.type==SDL_QUIT) running=false;
        padUpdate(&pad);
        u64 down=padGetButtonsDown(&pad);
        if(down & HidNpadButton_Plus) { statusText="RESCANNING..."; count=0; scanLimitHit=false; root=scanDir("sdmc:/","microSD",count); scanned=count; current=root.get(); selected=scroll=0; refreshVisible(); statusText="SCAN COMPLETE: "+std::to_string(scanned)+" ENTRIES"; }
        if(down & HidNpadButton_Minus) running=false;
        if(down & HidNpadButton_Down) { if(selected+1<(int)visible.size()) selected++; refreshVisible(); }
        if(down & HidNpadButton_Up) { if(selected>0) selected--; refreshVisible(); }
        if(down & HidNpadButton_L) { scroll=std::max(0,scroll-8); selected=std::max(0,selected-8); }
        if(down & HidNpadButton_R) { selected=std::min((int)visible.size()-1,selected+8); refreshVisible(); }
        if(down & HidNpadButton_A) enterSelected();
        if(down & HidNpadButton_B) goUp();
        if(down & HidNpadButton_X) treemapMode=!treemapMode;
        if(down & HidNpadButton_Y) { sortMode=(sortMode+1)%3; refreshVisible(); }
        draw();
        SDL_Delay(16);
    }
    root.reset();
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); 
    return 0;
}
