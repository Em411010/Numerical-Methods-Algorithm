#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define MAX_ITER   100
#define TOLERANCE  0.001
#define WIN_W      1600
#define WIN_H      930

typedef struct { double xn, xn1, error; } IterationRow;
typedef struct { SDL_Rect rect; char label[50]; char value[50]; int active; } InputBox;
typedef struct { SDL_Rect rect; char text[50]; int hovered; int clicked; } Button;
typedef struct { SDL_Rect rect; char formula[120]; int selected; int hovered; } MethodOption;

static double gfunc(double x, double a, double b, int method) {
    switch(method) {
        case 1: if(a*x+b<=0) return NAN; return log(a*x+b);
        case 2: if(a==0) return NAN; return (exp(x)-b)/a;
        case 3: if(a==0||exp(x)-b<=0) return NAN; return log((exp(x)-b)/a);
        case 4: if(a==0) return NAN; return exp(x)/a - b/a;
        case 5: return x - 0.1*(exp(x)-a*x-b);
        default: return NAN;
    }
}
static double feval(double x, double a, double b) { return exp(x)-a*x-b; }

static void renderText(SDL_Renderer* r, TTF_Font* f, const char* t, int x, int y, SDL_Color c) {
    SDL_Surface* s = TTF_RenderUTF8_Blended(f,t,c); if(!s) return;
    SDL_Texture* tx = SDL_CreateTextureFromSurface(r,s);
    SDL_Rect rc = {x,y,s->w,s->h}; SDL_RenderCopy(r,tx,NULL,&rc);
    SDL_FreeSurface(s); SDL_DestroyTexture(tx);
}
static void renderBold(SDL_Renderer* r, TTF_Font* f, const char* t, int x, int y, SDL_Color c) {
    renderText(r,f,t,x,y,c); renderText(r,f,t,x+1,y,c);
}
static int textW(TTF_Font* f, const char* t) { int w=0; TTF_SizeUTF8(f,t,&w,NULL); return w; }
static void renderCenter(SDL_Renderer* r, TTF_Font* f, const char* t, int cx, int y, SDL_Color c) {
    renderText(r,f,t,cx-textW(f,t)/2,y,c);
}
static void renderCenterBold(SDL_Renderer* r, TTF_Font* f, const char* t, int cx, int y, SDL_Color c) {
    renderBold(r,f,t,cx-textW(f,t)/2,y,c);
}
static void drawPanel(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color bg, SDL_Color bdr) {
    SDL_SetRenderDrawColor(r,bg.r,bg.g,bg.b,bg.a);
    SDL_Rect rc={x,y,w,h}; SDL_RenderFillRect(r,&rc);
    SDL_SetRenderDrawColor(r,bdr.r,bdr.g,bdr.b,255);
    SDL_RenderDrawRect(r,&rc);
}
static void renderInputBox(SDL_Renderer* r, TTF_Font* fLbl, TTF_Font* fVal, InputBox* box) {
    SDL_Color bg  = box->active ? (SDL_Color){255,240,248,255} : (SDL_Color){255,248,252,255};
    SDL_Color bdr = box->active ? (SDL_Color){180,40,80,255}   : (SDL_Color){210,140,170,255};
    drawPanel(r, box->rect.x, box->rect.y, box->rect.w, box->rect.h, bg, bdr);
    renderBold(r, fLbl, box->label, box->rect.x, box->rect.y-18, (SDL_Color){120,30,60,255});
    if(strlen(box->value)>0)
        renderText(r, fVal, box->value, box->rect.x+8, box->rect.y+8, (SDL_Color){80,20,50,255});
}
static void renderButton(SDL_Renderer* r, TTF_Font* f, Button* btn) {
    SDL_Color bg = btn->clicked ? (SDL_Color){120,20,55,255} :
                   btn->hovered ? (SDL_Color){200,60,100,255} :
                                  (SDL_Color){160,40,80,255};
    SDL_Rect rc = btn->rect;
    SDL_SetRenderDrawColor(r,bg.r,bg.g,bg.b,255); SDL_RenderFillRect(r,&rc);
    SDL_SetRenderDrawColor(r,90,10,40,255); SDL_RenderDrawRect(r,&rc);
    SDL_Color white={255,255,255,255};
    SDL_Surface* s = TTF_RenderUTF8_Blended(f,btn->text,white); if(!s) return;
    int tx2=rc.x+(rc.w-s->w)/2, ty2=rc.y+(rc.h-s->h)/2;
    SDL_Texture* tx=SDL_CreateTextureFromSurface(r,s);
    SDL_Rect tr={tx2,ty2,s->w,s->h}; SDL_RenderCopy(r,tx,NULL,&tr);
    SDL_FreeSurface(s); SDL_DestroyTexture(tx);
}

static void drawRiver(SDL_Renderer* r, int baseY) {
    // Draw grass banks on left and right
    SDL_SetRenderDrawColor(r,95,140,75,255);
    SDL_Rect leftBank={0,baseY,WIN_W/3,120}; SDL_RenderFillRect(r,&leftBank);
    SDL_SetRenderDrawColor(r,95,140,75,255);
    SDL_Rect rightBank={2*WIN_W/3,baseY,WIN_W/3,120}; SDL_RenderFillRect(r,&rightBank);
    
    // Draw river with flowing water effect
    int riverX=WIN_W/3, riverW=WIN_W/3;
    for(int y=baseY;y<baseY+120;y++){
        float prog=(float)(y-baseY)/120.0f;
        float wave=sin(prog*3.14159*2.5)*25+sin(prog*1.8)*12;
        int offset=(int)wave;
        
        // Gradient water color - darker at top, lighter at bottom
        Uint8 r2=(Uint8)(30+prog*40);
        Uint8 g2=(Uint8)(100+prog*60);
        Uint8 b2=(Uint8)(180+prog*60);
        SDL_SetRenderDrawColor(r,r2,g2,b2,255);
        SDL_RenderDrawLine(r,riverX+offset,y,riverX+offset+riverW,y);
    }
    
    // Add water flow lines for shimmer effect
    SDL_SetRenderDrawColor(r,140,200,255,130);
    for(int i=0;i<12;i++){
        int y=baseY+(i*10);
        float off=sin((float)i*0.8)*18;
        int len=40+(i%2)*15;
        SDL_RenderDrawLine(r,riverX+30+(int)off,y,riverX+30+(int)off+len,y);
    }
}

static void drawGraph(SDL_Renderer* rr, TTF_Font* fSmall,
                      double a, double b, double root, int hasRoot,
                      int mouseX, int mouseY) {
    int GX=1110, GY=210, GW=455, GH=380;
    double xMin=-4, xMax=4;
    if(hasRoot){ double margin=fabs(root)*0.8+2.5; xMin=root-margin; xMax=root+margin; }
    if(xMax-xMin<2.0){double m=(xMin+xMax)/2;xMin=m-1;xMax=m+1;}
    double yMinV=1e18, yMaxV=-1e18;
    for(int i=0;i<=200;i++){
        double xx=xMin+(double)i/200*(xMax-xMin);
        double yy=feval(xx,a,b);
        if(yy<yMinV) yMinV=yy; if(yy>yMaxV) yMaxV=yy;
    }
    double ySpan=yMaxV-yMinV; if(ySpan<1) ySpan=2;
    double yMin=yMinV-ySpan*0.18, yMax=yMaxV+ySpan*0.18;
    drawPanel(rr,GX,GY,GW,GH,(SDL_Color){255,245,250,255},(SDL_Color){210,140,170,255});
    SDL_SetRenderDrawColor(rr,248,228,238,255);
    for(int i=0;i<=10;i++){
        SDL_RenderDrawLine(rr,GX+i*GW/10,GY,GX+i*GW/10,GY+GH);
        SDL_RenderDrawLine(rr,GX,GY+i*GH/10,GX+GW,GY+i*GH/10);
    }
    int ox=GX+(int)((-xMin)/(xMax-xMin)*GW);
    int oy=GY+GH-(int)((-yMin)/(yMax-yMin)*GH);
    SDL_SetRenderDrawColor(rr,140,60,100,255);
    if(ox>=GX&&ox<=GX+GW) SDL_RenderDrawLine(rr,ox,GY,ox,GY+GH);
    if(oy>=GY&&oy<=GY+GH) SDL_RenderDrawLine(rr,GX,oy,GX+GW,oy);
    renderText(rr,fSmall,"x",GX+GW-13,oy+4,(SDL_Color){140,60,100,255});
    renderText(rr,fSmall,"y",ox+4,GY+4,(SDL_Color){140,60,100,255});
    int prevPx=-1, prevPy=-1;
    for(int px=0;px<GW;px++){
        double xv=xMin+(double)px/GW*(xMax-xMin);
        double yv=feval(xv,a,b);
        int sy2=GY+GH-(int)((yv-yMin)/(yMax-yMin)*GH);
        if(sy2>=GY&&sy2<=GY+GH){
            SDL_SetRenderDrawColor(rr,180,40,90,255);
            if(prevPx>=0&&abs(sy2-prevPy)<GH/2){
                SDL_RenderDrawLine(rr,GX+prevPx,prevPy,GX+px,sy2);
                SDL_RenderDrawLine(rr,GX+prevPx,prevPy+1,GX+px,sy2+1);
            }
            prevPx=px; prevPy=sy2;
        }
    }
    if(hasRoot){
        int rpx=GX+(int)((root-xMin)/(xMax-xMin)*GW);
        double rfy=feval(root,a,b);
        int rpy=GY+GH-(int)((rfy-yMin)/(yMax-yMin)*GH);
        if(rpx>=GX&&rpx<=GX+GW){
            SDL_SetRenderDrawColor(rr,180,50,100,200);
            for(int yy=GY;yy<=GY+GH;yy+=4) SDL_RenderDrawPoint(rr,rpx,yy);
            if(rpy>=GY&&rpy<=GY+GH)
                for(int di=-5;di<=5;di++) for(int dj=-5;dj<=5;dj++)
                    if(di*di+dj*dj<=25){SDL_SetRenderDrawColor(rr,220,50,90,255);SDL_RenderDrawPoint(rr,rpx+di,rpy+dj);}
            char lr[32]; sprintf(lr,"root ~ %.3f",root);
            int lw=textW(fSmall,lr), lx=rpx-lw/2;
            if(lx<GX) lx=GX; if(lx+lw>GX+GW) lx=GX+GW-lw;
            renderText(rr,fSmall,lr,lx,GY+GH+6,(SDL_Color){180,40,90,255});
        }
    }
    char rb[20];
    sprintf(rb,"%.1f",xMin); renderText(rr,fSmall,rb,GX+2,oy+5,(SDL_Color){160,90,130,180});
    sprintf(rb,"%.1f",xMax); renderText(rr,fSmall,rb,GX+GW-30,oy+5,(SDL_Color){160,90,130,180});
    if(mouseX>=GX&&mouseX<=GX+GW&&mouseY>=GY&&mouseY<=GY+GH){
        SDL_Rect clipG={GX,GY,GW,GH}; SDL_RenderSetClipRect(rr,&clipG);
        SDL_SetRenderDrawColor(rr,110,45,85,180);
        SDL_RenderDrawLine(rr,mouseX,GY,mouseX,GY+GH);
        SDL_SetRenderDrawColor(rr,110,45,85,110);
        SDL_RenderDrawLine(rr,GX,mouseY,GX+GW,mouseY);
        SDL_RenderSetClipRect(rr,NULL);
        double hx=xMin+(double)(mouseX-GX)/GW*(xMax-xMin);
        double hy=feval(hx,a,b);
        int hsy=GY+GH-(int)((hy-yMin)/(yMax-yMin)*GH);
        if(hsy>=GY&&hsy<=GY+GH)
            for(int di=-4;di<=4;di++) for(int dj=-4;dj<=4;dj++)
                if(di*di+dj*dj<=16){SDL_SetRenderDrawColor(rr,40,170,80,255);SDL_RenderDrawPoint(rr,mouseX+di,hsy+dj);}
        char htip[64]; sprintf(htip,"x = %.3f   y = %.3f",hx,hy);
        int tw2=textW(fSmall,htip)+10;
        int tx2=mouseX+12, ty2=mouseY-22;
        if(tx2+tw2>GX+GW) tx2=mouseX-tw2-6;
        if(ty2<GY) ty2=GY+4;
        SDL_SetRenderDrawBlendMode(rr,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(rr,255,245,250,230);
        SDL_Rect tipBg={tx2-2,ty2-2,tw2+4,20}; SDL_RenderFillRect(rr,&tipBg);
        SDL_SetRenderDrawBlendMode(rr,SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(rr,180,60,100,255); SDL_RenderDrawRect(rr,&tipBg);
        renderText(rr,fSmall,htip,tx2,ty2,(SDL_Color){110,20,60,255});
    }
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO); TTF_Init();
    SDL_Window* window = SDL_CreateWindow(
        "Fixed Point Iteration - Exponential Equation",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window,-1,
        SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(renderer,WIN_W,WIN_H);

    TTF_Font* fHuge  = TTF_OpenFont("font.ttf",36);
    TTF_Font* fBig   = TTF_OpenFont("font.ttf",28);
    TTF_Font* fTitle = TTF_OpenFont("font.ttf",24);
    TTF_Font* fLarge = TTF_OpenFont("font.ttf",20);
    TTF_Font* fMed   = TTF_OpenFont("font.ttf",17);
    TTF_Font* fNorm  = TTF_OpenFont("font.ttf",15);
    TTF_Font* fSmall = TTF_OpenFont("font.ttf",13);
    if(!fHuge||!fBig||!fTitle||!fLarge||!fMed||!fNorm||!fSmall){
        printf("Font error: %s\n",TTF_GetError()); return 1;
    }

    InputBox inputs[3];
    const char* lbls[]={"A","B","x0"};
    for(int i=0;i<3;i++){
        strcpy(inputs[i].label,lbls[i]); strcpy(inputs[i].value,"");
        inputs[i].active=0; inputs[i].rect=(SDL_Rect){0,0,120,36};
    }

    int selectedMethod=1;
    MethodOption methods[5];
    const char* fmls[]={
        "g(x) = ln(ax + b)",
        "g(x) = (e^x - b) / a",
        "g(x) = ln((e^x - b) / a)",
        "g(x) = e^x/a - b/a",
        "g(x) = x - 0.1(e^x - ax - b)"
    };
    for(int i=0;i<5;i++){
        strcpy(methods[i].formula,fmls[i]);
        methods[i].selected=(i==0); methods[i].hovered=0;
        methods[i].rect=(SDL_Rect){0,0,456,36};
    }

    Button btnCompute   ={{55, 600,180,46},"COMPUTE",0,0};
    Button btnClear     ={{260,600,180,46},"CLEAR",  0,0};
    Button btnStart     ={{WIN_W/2-120,600,240,56},"START",0,0};
    Button btnBack      ={{1490,15,90,50},"< BACK",0,0};
    Button btnConfirmYes={{WIN_W/2-130,WIN_H/2+40,110,44},"YES",0,0};
    Button btnConfirmNo ={{WIN_W/2+20, WIN_H/2+40,110,44},"CANCEL",0,0};

    char   resultText[500]="";
    double finalRoot=0, coefA=0, coefB=0;
    int    hasValidRoot=0, totalIterations=0;
    IterationRow iterations[MAX_ITER];
    int    activeInput=-1, quit=0, tableScrollOffset=0;
    int    screen=0, showConfirm=0;
    Uint32 loadStart=0;
    SDL_Event ev;

    while(!quit){
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_QUIT) quit=1;
            if(ev.type==SDL_MOUSEBUTTONDOWN){
                int mx=ev.button.x, my=ev.button.y;
                if(screen==0){
                    if(mx>=btnStart.rect.x&&mx<btnStart.rect.x+btnStart.rect.w&&
                       my>=btnStart.rect.y&&my<btnStart.rect.y+btnStart.rect.h){
                        screen=1; loadStart=SDL_GetTicks();
                    }
                } else if(screen==2){
                    if(mx>=btnBack.rect.x&&mx<btnBack.rect.x+btnBack.rect.w&&
                       my>=btnBack.rect.y&&my<btnBack.rect.y+btnBack.rect.h){
                        screen=0;
                        for(int i=0;i<3;i++) strcpy(inputs[i].value,"");
                        strcpy(resultText,""); hasValidRoot=0; totalIterations=0;
                        tableScrollOffset=0; showConfirm=0; continue;
                    }
                    if(showConfirm){
                        if(mx>=btnConfirmYes.rect.x&&mx<btnConfirmYes.rect.x+btnConfirmYes.rect.w&&
                           my>=btnConfirmYes.rect.y&&my<btnConfirmYes.rect.y+btnConfirmYes.rect.h){
                            showConfirm=0;
                            coefA=atof(inputs[0].value); coefB=atof(inputs[1].value);
                            double x0=atof(inputs[2].value); int method=selectedMethod;
                            double x_cur=x0; int div2=0; totalIterations=0;
                            for(int iter=0;iter<MAX_ITER;iter++){
                                double xn=gfunc(x_cur,coefA,coefB,method);
                                double err=fabs(xn-x_cur);
                                iterations[iter].xn=x_cur; iterations[iter].xn1=xn; iterations[iter].error=err;
                                totalIterations++;
                                if(isnan(xn)||isinf(xn)||fabs(xn)>1e10){div2=1;break;}
                                x_cur=xn;
                                if(err<TOLERANCE) break;
                            }
                            finalRoot=x_cur;
                            double verif=fabs(feval(finalRoot,coefA,coefB));
                            if(div2||verif>0.5){
                                sprintf(resultText,"FAILED: %s\nTry different method or x0",
                                        div2?"Diverged":"Did not converge");
                                hasValidRoot=0;
                            } else {
                                sprintf(resultText,"SUCCESS!\nRoot: x = %.3lf\nIterations: %d",
                                        finalRoot,totalIterations);
                                hasValidRoot=1;
                            }
                            tableScrollOffset=0;
                        }
                        if(mx>=btnConfirmNo.rect.x&&mx<btnConfirmNo.rect.x+btnConfirmNo.rect.w&&
                           my>=btnConfirmNo.rect.y&&my<btnConfirmNo.rect.y+btnConfirmNo.rect.h)
                            showConfirm=0;
                        continue;
                    }
                    activeInput=-1;
                    for(int i=0;i<3;i++){
                        inputs[i].active=(mx>=inputs[i].rect.x&&mx<inputs[i].rect.x+inputs[i].rect.w&&
                                          my>=inputs[i].rect.y&&my<inputs[i].rect.y+inputs[i].rect.h);
                        if(inputs[i].active) activeInput=i;
                    }
                    for(int i=0;i<5;i++){
                        if(mx>=methods[i].rect.x&&mx<methods[i].rect.x+methods[i].rect.w&&
                           my>=methods[i].rect.y&&my<methods[i].rect.y+methods[i].rect.h){
                            selectedMethod=i+1;
                            for(int j=0;j<5;j++) methods[j].selected=(j==i);
                        }
                    }
                    if(mx>=btnCompute.rect.x&&mx<btnCompute.rect.x+btnCompute.rect.w&&
                       my>=btnCompute.rect.y&&my<btnCompute.rect.y+btnCompute.rect.h)
                        showConfirm=1;
                    if(mx>=btnClear.rect.x&&mx<btnClear.rect.x+btnClear.rect.w&&
                       my>=btnClear.rect.y&&my<btnClear.rect.y+btnClear.rect.h){
                        for(int i=0;i<3;i++) strcpy(inputs[i].value,"");
                        selectedMethod=1; for(int i=0;i<5;i++) methods[i].selected=(i==0);
                        strcpy(resultText,""); hasValidRoot=0; totalIterations=0; tableScrollOffset=0;
                    }
                }
            }
            if(ev.type==SDL_MOUSEMOTION){
                int mx=ev.motion.x, my=ev.motion.y;
                if(screen==0){
                    btnStart.hovered=(mx>=btnStart.rect.x&&mx<btnStart.rect.x+btnStart.rect.w&&
                                      my>=btnStart.rect.y&&my<btnStart.rect.y+btnStart.rect.h);
                } else if(screen==2){
                    btnBack.hovered=(mx>=btnBack.rect.x&&mx<btnBack.rect.x+btnBack.rect.w&&
                                     my>=btnBack.rect.y&&my<btnBack.rect.y+btnBack.rect.h);
                    btnCompute.hovered=(mx>=btnCompute.rect.x&&mx<btnCompute.rect.x+btnCompute.rect.w&&
                                        my>=btnCompute.rect.y&&my<btnCompute.rect.y+btnCompute.rect.h);
                    btnClear.hovered=(mx>=btnClear.rect.x&&mx<btnClear.rect.x+btnClear.rect.w&&
                                      my>=btnClear.rect.y&&my<btnClear.rect.y+btnClear.rect.h);
                    for(int i=0;i<5;i++)
                        methods[i].hovered=(mx>=methods[i].rect.x&&mx<methods[i].rect.x+methods[i].rect.w&&
                                            my>=methods[i].rect.y&&my<methods[i].rect.y+methods[i].rect.h);
                    if(showConfirm){
                        btnConfirmYes.hovered=(mx>=btnConfirmYes.rect.x&&mx<btnConfirmYes.rect.x+btnConfirmYes.rect.w&&
                                               my>=btnConfirmYes.rect.y&&my<btnConfirmYes.rect.y+btnConfirmYes.rect.h);
                        btnConfirmNo.hovered=(mx>=btnConfirmNo.rect.x&&mx<btnConfirmNo.rect.x+btnConfirmNo.rect.w&&
                                              my>=btnConfirmNo.rect.y&&my<btnConfirmNo.rect.y+btnConfirmNo.rect.h);
                    }
                }
            }
            if(ev.type==SDL_TEXTINPUT&&screen==2&&activeInput>=0&&!showConfirm){
                char c=ev.text.text[0];
                if((c>='0'&&c<='9')||c=='.'||c=='-'){
                    int len=strlen(inputs[activeInput].value);
                    if(len<19){inputs[activeInput].value[len]=c;inputs[activeInput].value[len+1]='\0';}
                }
            }
            if(ev.type==SDL_KEYDOWN&&screen==2&&activeInput>=0&&!showConfirm){
                if(ev.key.keysym.sym==SDLK_BACKSPACE){
                    int len=strlen(inputs[activeInput].value);
                    if(len>0) inputs[activeInput].value[len-1]='\0';
                }
                if(ev.key.keysym.sym==SDLK_TAB){
                    activeInput=(activeInput+1)%3;
                    for(int i=0;i<3;i++) inputs[i].active=(i==activeInput);
                }
            }
            if(ev.type==SDL_MOUSEWHEEL&&screen==2&&!showConfirm){
                tableScrollOffset-=ev.wheel.y*2;
                if(tableScrollOffset<0) tableScrollOffset=0;
            }
        }

        if(screen==1){ Uint32 elapsed=SDL_GetTicks()-loadStart; if(elapsed>=5000) screen=2; }

        if(screen==2){
            inputs[0].rect=(SDL_Rect){40,     200,120,36};
            inputs[1].rect=(SDL_Rect){195,    200,120,36};
            inputs[2].rect=(SDL_Rect){350,    200,120,36};
            for(int i=0;i<5;i++) methods[i].rect=(SDL_Rect){28,305+i*48,456,36};
        }

        /* gradient background */
        for(int y=0;y<WIN_H;y++){
            SDL_SetRenderDrawColor(renderer,(Uint8)(250-y*30/WIN_H),(Uint8)(215-y*50/WIN_H),(Uint8)(225-y*25/WIN_H),255);
            SDL_RenderDrawLine(renderer,0,y,WIN_W,y);
        }

        SDL_Color white   ={255,255,255,255};
        SDL_Color darkTxt ={80, 20, 50, 255};
        SDL_Color hintCol ={160,100,130,200};
        SDL_Color cream   ={255,240,248,255};
        SDL_Color secCol  ={120,30, 60, 255};
        SDL_Color panelBg ={255,248,252,255};
        SDL_Color panBdr  ={210,140,170,255};
        SDL_Color eqBg    ={255,240,248,255};
        SDL_Color eqBdr   ={200,120,155,255};

        drawRiver(renderer, WIN_H-120);
        SDL_SetRenderDrawColor(renderer,175,100,130,255);
        SDL_Rect ground={0,WIN_H-120,WIN_W,120}; SDL_RenderFillRect(renderer,&ground);

        /* ============== FRONT PAGE ============== */
        if(screen==0){
            int cx=WIN_W/2, cy=WIN_H/2-60;
            drawPanel(renderer,cx-380,cy-235,760,460,(SDL_Color){255,245,252,255},(SDL_Color){200,100,140,255});
            SDL_SetRenderDrawColor(renderer,160,40,80,255);
            SDL_Rect hdr2={cx-380,cy-235,760,50}; SDL_RenderFillRect(renderer,&hdr2);
            renderCenterBold(renderer,fLarge,"MT211 \xe2\x80\x94 Numerical Method  |  Semestral Project",cx,cy-228,white);
            renderCenterBold(renderer,fHuge,"FIXED POINT ITERATION",cx,cy-172,(SDL_Color){140,30,70,255});
            renderCenterBold(renderer,fTitle,"Exponential Equation  |  e^x - ax - b = 0",cx,cy-126,(SDL_Color){130,50,85,255});
            SDL_SetRenderDrawColor(renderer,210,140,170,255);
            SDL_RenderDrawLine(renderer,cx-320,cy-98,cx+320,cy-98);
            renderCenter(renderer,fMed,"Submitted by:",cx,cy-80,(SDL_Color){140,60,90,255});
            renderCenterBold(renderer,fLarge,"Jovielyn B. Panes",cx,cy-58,(SDL_Color){160,40,85,255});
            renderCenterBold(renderer,fLarge,"Princess Ella M. Panes",cx,cy-32,(SDL_Color){160,40,85,255});
            renderCenter(renderer,fMed,"BSCPE 22001",cx,cy-4,(SDL_Color){140,60,90,220});
            SDL_SetRenderDrawColor(renderer,210,140,170,255);
            SDL_RenderDrawLine(renderer,cx-320,cy+20,cx+320,cy+20);
            renderCenter(renderer,fSmall,"Instructor:",cx,cy+36,(SDL_Color){140,80,110,255});
            renderCenterBold(renderer,fMed,"Engr. Edgar Broncano",cx,cy+54,(SDL_Color){120,30,60,255});
            SDL_SetRenderDrawColor(renderer,210,140,170,255);
            SDL_RenderDrawLine(renderer,cx-320,cy+78,cx+320,cy+78);
            renderCenter(renderer,fSmall,"Bestlink College of the Philippines",cx,cy+94,(SDL_Color){140,80,110,200});
            renderCenter(renderer,fSmall,"College of Engineering and Architecture",cx,cy+112,(SDL_Color){140,80,110,180});
            renderButton(renderer,fLarge,&btnStart);
        }

        /* ============== LOADING ============== */
        else if(screen==1){
            float dtime=(float)SDL_GetTicks()/1000.0f;
            Uint32 elapsed=SDL_GetTicks()-loadStart;
            float lp=(float)elapsed/5000.0f; if(lp>1.0f) lp=1.0f;
            float pulse=(float)(sin(dtime*3)*0.15+0.85);
            SDL_Color tc={(Uint8)(180*pulse),(Uint8)(40*pulse),(Uint8)(80*pulse),255};
            renderCenterBold(renderer,fHuge,"FIXED POINT ITERATION",WIN_W/2,WIN_H/2-180,tc);
            renderCenter(renderer,fTitle,"Exponential Equation Solver",WIN_W/2,WIN_H/2-135,(SDL_Color){140,60,90,255});
            int pbX=WIN_W/2-300, pbY=WIN_H/2+40, pbW=600, pbH=22;
            drawPanel(renderer,pbX-2,pbY-2,pbW+4,pbH+4,(SDL_Color){230,200,215,255},(SDL_Color){180,80,120,255});
            SDL_SetRenderDrawColor(renderer,160,40,80,255);
            SDL_Rect fillR={pbX,pbY,(int)(pbW*lp),pbH}; SDL_RenderFillRect(renderer,&fillR);
            char pct2[16]; sprintf(pct2,"%d%%",(int)(lp*100));
            renderCenter(renderer,fMed,pct2,WIN_W/2,pbY+pbH+8,(SDL_Color){140,40,75,255});
            const char* stages[]={"Initializing...","Loading solver...","Preparing graph...","Ready!"};
            int si=(int)(lp*3.99f); if(si>3) si=3;
            renderCenter(renderer,fSmall,stages[si],WIN_W/2,pbY+pbH+28,(SDL_Color){160,80,110,200});
            for(int d=0;d<8;d++){
                float angle2=(float)d/8*2*3.14159f+(dtime*3);
                int ddx=WIN_W/2+(int)(cos(angle2)*40), ddy=WIN_H/2+85+(int)(sin(angle2)*14);
                int rr2=4+(d%2)*2;
                for(int di=-rr2;di<=rr2;di++) for(int dj=-rr2;dj<=rr2;dj++)
                    if(di*di+dj*dj<=rr2*rr2){SDL_SetRenderDrawColor(renderer,160,40,80,200);SDL_RenderDrawPoint(renderer,ddx+di,ddy+dj);}
            }
            renderCenter(renderer,fSmall,"Bestlink College of the Philippines",WIN_W/2,WIN_H-80,(SDL_Color){160,80,110,180});
        }

        /* ============== SOLVER ============== */
        else if(screen==2){

            /* header */
            SDL_SetRenderDrawColor(renderer,100,20,50,255);
            SDL_Rect hbar={0,0,WIN_W,80}; SDL_RenderFillRect(renderer,&hbar);
            renderBold(renderer,fBig,"FIXED POINT ITERATION METHOD",20,10,(SDL_Color){255,230,240,255});
            renderText(renderer,fNorm,"Exponential Equation  |  e^x - ax - b = 0",22,44,(SDL_Color){220,170,195,255});
            renderCenter(renderer,fSmall,"MT211 - Numerical Method  |  Semestral Project",WIN_W/2,10,(SDL_Color){220,170,195,255});
            renderCenterBold(renderer,fNorm,"BSCPE 22001",WIN_W/2,28,(SDL_Color){255,220,235,255});
            renderCenter(renderer,fSmall,"Jovielyn B. Panes  |  Princess Ella M. Panes",WIN_W/2,46,(SDL_Color){220,170,195,255});
            renderButton(renderer,fSmall,&btnBack);

            /* === LEFT PANEL === */
            drawPanel(renderer,10,90,492,838,panelBg,panBdr);
            SDL_SetRenderDrawColor(renderer,160,40,80,255);
            SDL_Rect lpbar={10,90,492,30}; SDL_RenderFillRect(renderer,&lpbar);
            renderBold(renderer,fLarge,"INPUT",24,95,white);
            renderText(renderer,fSmall,"Coefficients  +  Initial Guess  +  g(x)",130,97,cream);
            drawPanel(renderer,28,120,456,40,eqBg,eqBdr);
            renderText(renderer,fSmall,"Equation to solve:",40,124,hintCol);
            renderBold(renderer,fMed,"f(x) = e^x - ax - b = 0",40,138,secCol);
            SDL_SetRenderDrawColor(renderer,160,40,80,255);
            SDL_Rect s1hdr={28,168,456,24}; SDL_RenderFillRect(renderer,&s1hdr);
            renderBold(renderer,fNorm,"STEP 1 \xe2\x80\x94 Enter coefficients & initial guess",36,171,white);
            {
                const char* hints[]={"coefficient a","coefficient b","initial guess"};
                for(int i=0;i<3;i++){
                    renderInputBox(renderer,fSmall,fNorm,&inputs[i]);
                    renderText(renderer,fSmall,hints[i],inputs[i].rect.x,inputs[i].rect.y+40,hintCol);
                }
            }
            SDL_SetRenderDrawColor(renderer,160,40,80,255);
            SDL_Rect s2hdr={28,254,456,24}; SDL_RenderFillRect(renderer,&s2hdr);
            renderBold(renderer,fNorm,"STEP 2 \xe2\x80\x94 Select g(x) transformation",36,257,white);
            renderText(renderer,fSmall,"Choose the form that fits your equation:",40,282,hintCol);
            for(int i=0;i<5;i++){
                SDL_Color mbg = methods[i].selected?(SDL_Color){255,210,228,255}:
                                methods[i].hovered ?(SDL_Color){255,232,242,255}:
                                                    (SDL_Color){255,248,252,255};
                SDL_Color mbdr= methods[i].selected?(SDL_Color){180,50,90,255}:
                                                    (SDL_Color){210,150,175,255};
                drawPanel(renderer,methods[i].rect.x,methods[i].rect.y,methods[i].rect.w,methods[i].rect.h,mbg,mbdr);
                if(methods[i].selected){
                    SDL_SetRenderDrawColor(renderer,180,50,90,255);
                    SDL_Rect dot={methods[i].rect.x+8,methods[i].rect.y+12,10,10};
                    SDL_RenderFillRect(renderer,&dot);
                }
                SDL_Color mc=methods[i].selected?(SDL_Color){140,30,70,255}:
                             methods[i].hovered ?(SDL_Color){120,40,75,255}:
                                                 (SDL_Color){140,80,110,255};
                renderText(renderer,fSmall,methods[i].formula,methods[i].rect.x+24,methods[i].rect.y+10,mc);
            }
            {
                double pA=atof(inputs[0].value), pB=atof(inputs[1].value);
                char prev[128]; sprintf(prev,"Preview: e^x - %.3gx - %.3g = 0",pA,pB);
                renderBold(renderer,fSmall,prev,40,570,(SDL_Color){160,40,85,255});
            }
            btnCompute.rect=(SDL_Rect){55,595,180,46};
            btnClear.rect=(SDL_Rect){260,595,180,46};
            renderButton(renderer,fNorm,&btnCompute);
            renderButton(renderer,fNorm,&btnClear);
            if(strlen(resultText)>0){
                int isOk=hasValidRoot;
                drawPanel(renderer,28,665,456,80,
                    isOk?(SDL_Color){255,235,240,255}:(SDL_Color){255,228,230,255},
                    isOk?(SDL_Color){180,100,140,255}:(SDL_Color){210,80,100,255});
                renderBold(renderer,fSmall,isOk?"STATUS: SUCCESS":"STATUS: FAILED",
                           40,669,isOk?(SDL_Color){60,130,80,255}:(SDL_Color){180,20,30,255});
                char resC[500]; strcpy(resC,resultText);
                char* lnp=strtok(resC,"\n"); int rry=685;
                while(lnp){renderText(renderer,fSmall,lnp,40,rry,isOk?(SDL_Color){80,40,60,255}:(SDL_Color){160,30,40,255});rry+=15;lnp=strtok(NULL,"\n");}
            }
            if(hasValidRoot){
                drawPanel(renderer,28,770,456,85,(SDL_Color){255,230,240,255},(SDL_Color){200,100,150,255});
                SDL_SetRenderDrawColor(renderer,160,40,80,255);
                SDL_Rect rbdr={28,770,456,24}; SDL_RenderFillRect(renderer,&rbdr);
                renderBold(renderer,fLarge,"RESULT",200,774,white);
                char buf[200];
                double pA=atof(inputs[0].value), pB=atof(inputs[1].value);
                sprintf(buf,"e^x - %.3gx - %.3g = 0",pA,pB);
                renderBold(renderer,fNorm,buf,44,798,(SDL_Color){140,30,70,255});
                sprintf(buf,"Root: x = %.3lf",finalRoot);
                renderBold(renderer,fNorm,buf,44,816,(SDL_Color){160,40,85,255});
                sprintf(buf,"Iterations: %d   |   Tolerance: %.3lf",totalIterations,TOLERANCE);
                renderText(renderer,fSmall,buf,44,832,hintCol);
            }

            /* === MIDDLE PANEL === */
            drawPanel(renderer,516,90,570,838,panelBg,panBdr);
            SDL_SetRenderDrawColor(renderer,160,40,80,255);
            SDL_Rect stbar={516,90,570,30}; SDL_RenderFillRect(renderer,&stbar);
            renderBold(renderer,fLarge,"ITERATION TABLE",570,95,white);
            if(totalIterations>0){
                int sy=128;
                drawPanel(renderer,530,sy,542,42,eqBg,eqBdr);
                renderBold(renderer,fMed,"GIVEN",544,sy+3,secCol);
                SDL_SetRenderDrawColor(renderer,eqBdr.r,eqBdr.g,eqBdr.b,255);
                SDL_RenderDrawLine(renderer,544,sy+20,1060,sy+20);
                {char sb[200];sprintf(sb,"f(x) = e^x - %.3gx - %.3g     x0 = %s     Tol = %.3f",coefA,coefB,inputs[2].value,TOLERANCE);renderText(renderer,fSmall,sb,544,sy+24,darkTxt);}
                sy+=48;
                int rowH=20, maxVis=20;
                int tableH=totalIterations*rowH+40;
                int maxTableH=maxVis*rowH+40;
                int dispH=tableH<maxTableH?tableH:maxTableH;
                drawPanel(renderer,530,sy,542,dispH,(SDL_Color){255,245,252,255},eqBdr);
                renderBold(renderer,fMed,"STEP-BY-STEP",544,sy+3,(SDL_Color){140,40,85,255});
                SDL_SetRenderDrawColor(renderer,eqBdr.r,eqBdr.g,eqBdr.b,255);
                SDL_RenderDrawLine(renderer,544,sy+20,1060,sy+20);
                SDL_SetRenderDrawColor(renderer,160,40,80,255);
                SDL_Rect hdrBg={531,sy+22,540,18}; SDL_RenderFillRect(renderer,&hdrBg);
                renderBold(renderer,fSmall,"n",544,sy+23,white);
                renderBold(renderer,fSmall,"xn",580,sy+23,white);
                renderBold(renderer,fSmall,"g(xn) = xn+1",700,sy+23,white);
                renderBold(renderer,fSmall,"|error|",900,sy+23,white);
                renderBold(renderer,fSmall,"Status",1000,sy+23,white);
                SDL_Rect clipR={530,sy+40,542,dispH-46}; SDL_RenderSetClipRect(renderer,&clipR);
                for(int i=0;i<totalIterations;i++){
                    int rryy=sy+42+i*rowH-tableScrollOffset*rowH;
                    if(rryy<sy+36||rryy>sy+dispH) continue;
                    SDL_SetRenderDrawColor(renderer,i%2==0?255:252,i%2==0?242:248,i%2==0?248:255,255);
                    SDL_Rect rowBg={531,rryy-1,540,rowH-1}; SDL_RenderFillRect(renderer,&rowBg);
                    if(i==totalIterations-1){SDL_SetRenderDrawColor(renderer,255,225,235,255);SDL_RenderFillRect(renderer,&rowBg);}
                    char sb[50];
                    sprintf(sb,"%d",i+1); renderText(renderer,fSmall,sb,544,rryy,darkTxt);
                    sprintf(sb,"%.6f",iterations[i].xn); renderText(renderer,fSmall,sb,570,rryy,darkTxt);
                    sprintf(sb,"%.6f",iterations[i].xn1); renderText(renderer,fSmall,sb,690,rryy,darkTxt);
                    sprintf(sb,"%.6f",iterations[i].error);
                    renderText(renderer,fSmall,sb,870,rryy,iterations[i].error<TOLERANCE?(SDL_Color){50,140,70,255}:(SDL_Color){180,50,80,255});
                    if(i==totalIterations-1&&hasValidRoot) renderBold(renderer,fSmall,"CONVERGED",988,rryy,(SDL_Color){50,140,70,255});
                    else if(i==totalIterations-1&&!hasValidRoot) renderBold(renderer,fSmall,"FAILED",1000,rryy,(SDL_Color){180,30,40,255});
                    else renderText(renderer,fSmall,"...",1012,rryy,hintCol);
                }
                SDL_RenderSetClipRect(renderer,NULL);
                if(totalIterations>maxVis){
                    int maxScr=totalIterations-maxVis; if(tableScrollOffset>maxScr) tableScrollOffset=maxScr;
                    int sbH=(int)((float)maxVis/totalIterations*(dispH-46)); if(sbH<20) sbH=20;
                    int sbY=sy+40+(int)((float)tableScrollOffset/maxScr*(dispH-46-sbH));
                    SDL_SetRenderDrawColor(renderer,200,120,160,200);
                    SDL_Rect scbr={1066,sbY,6,sbH}; SDL_RenderFillRect(renderer,&scbr);
                }
                sy+=dispH+6;
                int conv=hasValidRoot;
                drawPanel(renderer,530,sy,542,50,conv?(SDL_Color){245,255,248,255}:(SDL_Color){255,235,238,255},conv?(SDL_Color){80,180,100,255}:(SDL_Color){210,80,100,255});
                renderBold(renderer,fMed,conv?"CONVERGED \xe2\x80\x94 ROOT FOUND":"DID NOT CONVERGE",544,sy+5,conv?(SDL_Color){40,130,60,255}:(SDL_Color){180,30,40,255});
                SDL_SetRenderDrawColor(renderer,conv?80:210,conv?180:80,conv?100:100,255);
                SDL_RenderDrawLine(renderer,544,sy+23,1060,sy+23);
                {char sb[200];sprintf(sb,"x = %.3f     f(x) = %.3e     n = %d",finalRoot,feval(finalRoot,coefA,coefB),totalIterations);renderBold(renderer,fNorm,sb,544,sy+28,darkTxt);}
            } else {
                renderCenter(renderer,fNorm,"Enter coefficients and select g(x)",516+285,380,hintCol);
                renderCenter(renderer,fNorm,"then press COMPUTE to see iterations.",516+285,404,hintCol);
                SDL_SetRenderDrawColor(renderer,220,170,190,200);
                SDL_RenderDrawLine(renderer,560,462,1060,462);
                renderCenter(renderer,fSmall,"Example: a=2, b=1, x0=0",516+285,474,hintCol);
            }

            /* === RIGHT PANEL === */
            drawPanel(renderer,1098,90,488,838,panelBg,panBdr);
            SDL_SetRenderDrawColor(renderer,160,40,80,255);
            SDL_Rect gpbar={1098,90,488,30}; SDL_RenderFillRect(renderer,&gpbar);
            renderBold(renderer,fLarge,"GRAPH",1300,95,white);
            {
                int _pmx,_pmy; SDL_GetMouseState(&_pmx,&_pmy);
                int _ww,_wh; SDL_GetWindowSize(window,&_ww,&_wh);
                float _sc=((float)_ww/WIN_W<(float)_wh/WIN_H)?(float)_ww/WIN_W:(float)_wh/WIN_H;
                int _xo=(int)((_ww-_sc*WIN_W)/2), _yo=(int)((_wh-_sc*WIN_H)/2);
                drawGraph(renderer,fSmall,coefA,coefB,finalRoot,hasValidRoot,(int)((_pmx-_xo)/_sc),(int)((_pmy-_yo)/_sc));
            }
            int LY=620;
            drawPanel(renderer,1110,LY,462,90,eqBg,eqBdr);
            SDL_SetRenderDrawColor(renderer,160,40,80,255);
            SDL_Rect lhdr={1110,LY,462,22}; SDL_RenderFillRect(renderer,&lhdr);
            renderBold(renderer,fNorm,"LEGEND",1320,LY+3,white);
            SDL_SetRenderDrawColor(renderer,180,40,90,255); SDL_RenderDrawLine(renderer,1125,LY+32,1165,LY+32);
            renderText(renderer,fSmall,"f(x) curve",1175,LY+25,(SDL_Color){80,20,50,255});
            for(int di=-5;di<=5;di++) for(int dj=-5;dj<=5;dj++) if(di*di+dj*dj<=25){SDL_SetRenderDrawColor(renderer,220,50,90,255);SDL_RenderDrawPoint(renderer,1145+di,LY+52+dj);}
            renderText(renderer,fSmall,"Root marker",1175,LY+45,(SDL_Color){80,20,50,255});
            SDL_SetRenderDrawColor(renderer,140,60,100,255); SDL_RenderDrawLine(renderer,1125,LY+72,1165,LY+72);
            renderText(renderer,fSmall,"Axes",1175,LY+65,(SDL_Color){80,20,50,255});
            int aly=730;
            drawPanel(renderer,1110,aly,462,96,eqBg,eqBdr);
            SDL_SetRenderDrawColor(renderer,160,40,80,255);
            SDL_Rect ahdr={1110,aly,462,22}; SDL_RenderFillRect(renderer,&ahdr);
            renderBold(renderer,fNorm,"ALGORITHM",1300,aly+3,white);
            renderText(renderer,fSmall,"1. Start with initial guess x0",1122,aly+28,(SDL_Color){80,20,50,255});
            renderText(renderer,fSmall,"2. Compute xn+1 = g(xn)",1122,aly+44,(SDL_Color){80,20,50,255});
            renderText(renderer,fSmall,"3. Repeat until |xn+1 - xn| < tolerance",1122,aly+60,(SDL_Color){80,20,50,255});
            renderText(renderer,fSmall,"4. f(x) = e^x - ax - b",1122,aly+76,(SDL_Color){80,20,50,255});

            /* confirm modal */
            if(showConfirm){
                SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer,0,0,0,140);
                SDL_Rect overlay={0,0,WIN_W,WIN_H}; SDL_RenderFillRect(renderer,&overlay);
                SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
                int mx2=WIN_W/2, my2=WIN_H/2-30;
                drawPanel(renderer,mx2-230,my2-85,460,200,(SDL_Color){255,245,252,255},(SDL_Color){180,60,100,255});
                SDL_SetRenderDrawColor(renderer,160,40,80,255);
                SDL_Rect mhdr={mx2-230,my2-85,460,34}; SDL_RenderFillRect(renderer,&mhdr);
                renderCenterBold(renderer,fLarge,"Confirm Computation",mx2,my2-77,white);
                renderCenter(renderer,fSmall,"Compute with these values?",mx2,my2-38,darkTxt);
                char x0buf[80];
                sprintf(x0buf,"a = %s    b = %s    x0 = %s",inputs[0].value,inputs[1].value,inputs[2].value);
                renderCenter(renderer,fSmall,x0buf,mx2,my2-18,(SDL_Color){130,40,70,255});
                renderCenter(renderer,fSmall,methods[selectedMethod-1].formula,mx2,my2+4,darkTxt);
                renderButton(renderer,fNorm,&btnConfirmYes);
                renderButton(renderer,fNorm,&btnConfirmNo);
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    TTF_CloseFont(fHuge); TTF_CloseFont(fBig); TTF_CloseFont(fTitle);
    TTF_CloseFont(fLarge); TTF_CloseFont(fMed); TTF_CloseFont(fNorm); TTF_CloseFont(fSmall);
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window);
    TTF_Quit(); SDL_Quit(); return 0;
}
