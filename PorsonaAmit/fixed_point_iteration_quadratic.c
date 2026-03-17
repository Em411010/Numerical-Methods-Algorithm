#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define MAX_ITER 100
#define TOLERANCE 0.001
#define WIN_W 1600
#define WIN_H 930

typedef struct { double xn, xn1, error; } IterationRow;

typedef struct {
    SDL_Rect rect;
    char label[50];
    char value[50];
    int active;
} InputBox;

typedef struct {
    SDL_Rect rect;
    char text[50];
    int hovered, clicked;
} Button;

typedef struct {
    SDL_Rect rect;
    char formula[80];
    int selected, hovered;
} MethodOption;

/* ---------------------------------------------------------------- math --- */
static double g(double x, double a, double b, double c, int method) {
    switch (method) {
        case 1: return -(a*x*x + c) / b;
        case 2:
            if (fabs(a*x + b) < 1e-10) return NAN;
            return -c / (a*x + b);
        case 3:
            if (a == 0 || (-b*x - c)/a < 0) return NAN;
            return sqrt((-b*x - c) / a);
        case 4:
            if (a == 0 || (-b*x - c)/a < 0) return NAN;
            return -sqrt((-b*x - c) / a);
        case 5:
            if (b == 0) return NAN;
            return (x*x - c/a) / (-b/a);
        default: return NAN;
    }
}

static double feval(double x, double a, double b, double c) {
    return a*x*x + b*x + c;
}

static void formatEquation(char* buf, int a, int b, int c) {
    char p1[50], p2[50], p3[50];
    if (a==1) strcpy(p1,"x\xC2\xB2");
    else if (a==-1) strcpy(p1,"-x\xC2\xB2");
    else sprintf(p1,"%dx\xC2\xB2",a);
    if (b==0) strcpy(p2,"");
    else if (b==1) strcpy(p2," + x");
    else if (b==-1) strcpy(p2," - x");
    else if (b>0) sprintf(p2," + %dx",b);
    else sprintf(p2," - %dx",-b);
    if (c==0) strcpy(p3,"");
    else if (c>0) sprintf(p3," + %d",c);
    else sprintf(p3," - %d",-c);
    sprintf(buf,"%s%s%s = 0",p1,p2,p3);
}

/* --------------------------------------------------------------- render --- */
static void renderText(SDL_Renderer* r, TTF_Font* f, const char* t,
                       int x, int y, SDL_Color c) {
    if (!t || !t[0]) return;
    SDL_Surface* s = TTF_RenderUTF8_Blended(f, t, c);
    if (!s) return;
    SDL_Texture* tx = SDL_CreateTextureFromSurface(r, s);
    SDL_Rect rc = {x, y, s->w, s->h};
    SDL_RenderCopy(r, tx, NULL, &rc);
    SDL_FreeSurface(s); SDL_DestroyTexture(tx);
}

static void renderBold(SDL_Renderer* r, TTF_Font* f, const char* t,
                       int x, int y, SDL_Color c) {
    renderText(r,f,t,x,y,c); renderText(r,f,t,x+1,y,c);
}

static int textW(TTF_Font* f, const char* t) {
    int w=0; TTF_SizeText(f,t,&w,NULL); return w;
}

static void renderCenter(SDL_Renderer* r, TTF_Font* f, const char* t,
                         int cx, int y, SDL_Color c) {
    renderText(r,f,t,cx-textW(f,t)/2,y,c);
}

static void renderCenterBold(SDL_Renderer* r, TTF_Font* f, const char* t,
                             int cx, int y, SDL_Color c) {
    renderBold(r,f,t,cx-textW(f,t)/2,y,c);
}

static void drawPanel(SDL_Renderer* r, int x, int y, int w, int h,
                      SDL_Color bg, SDL_Color bdr) {
    SDL_SetRenderDrawColor(r,bg.r,bg.g,bg.b,bg.a);
    SDL_Rect rc={x,y,w,h}; SDL_RenderFillRect(r,&rc);
    SDL_SetRenderDrawColor(r,bdr.r,bdr.g,bdr.b,bdr.a);
    SDL_RenderDrawRect(r,&rc);
}

static void renderInputBox(SDL_Renderer* rr, TTF_Font* fLbl, TTF_Font* fVal,
                           InputBox* box) {
    SDL_Color lc = {155,35,110,255};
    renderBold(rr,fLbl,box->label,box->rect.x,box->rect.y-19,lc);
    SDL_SetRenderDrawColor(rr,box->active?255:255,box->active?230:245,box->active?245:252,255);
    SDL_RenderFillRect(rr,&box->rect);
    SDL_SetRenderDrawColor(rr,box->active?220:200,box->active?80:140,box->active?160:180,255);
    SDL_RenderDrawRect(rr,&box->rect);
    if (box->active) {
        SDL_Rect inn={box->rect.x+1,box->rect.y+1,box->rect.w-2,box->rect.h-2};
        SDL_RenderDrawRect(rr,&inn);
    }
    if (strlen(box->value)>0)
        renderText(rr,fVal,box->value,box->rect.x+8,box->rect.y+8,(SDL_Color){110,20,80,255});
}

static void renderButton(SDL_Renderer* rr, TTF_Font* f, Button* btn) {
    SDL_Color bg = btn->clicked  ? (SDL_Color){165,35,115,255}
                 : btn->hovered  ? (SDL_Color){255,110,185,255}
                                 : (SDL_Color){225,65,155,255};
    SDL_SetRenderDrawColor(rr,130,20,90,255);
    SDL_Rect sh={btn->rect.x+3,btn->rect.y+3,btn->rect.w,btn->rect.h};
    SDL_RenderFillRect(rr,&sh);
    SDL_SetRenderDrawColor(rr,bg.r,bg.g,bg.b,255);
    SDL_RenderFillRect(rr,&btn->rect);
    SDL_SetRenderDrawColor(rr,175,45,130,255);
    SDL_RenderDrawRect(rr,&btn->rect);
    SDL_Surface* s = TTF_RenderText_Blended(f,btn->text,(SDL_Color){255,255,255,255});
    if (s) {
        SDL_Texture* tx = SDL_CreateTextureFromSurface(rr,s);
        SDL_Rect tr={btn->rect.x+(btn->rect.w-s->w)/2,
                     btn->rect.y+(btn->rect.h-s->h)/2,s->w,s->h};
        SDL_RenderCopy(rr,tx,NULL,&tr);
        SDL_FreeSurface(s); SDL_DestroyTexture(tx);
    }
}

/* ----------------------------------------------------------- mountains --- */
static void drawMountains(SDL_Renderer* rr, int baseY, int w) {
    /* back range — lighter pink */
    {
        int pts[][2] = {{0,baseY},{80,baseY-120},{200,baseY-210},{340,baseY-170},
            {480,baseY-260},{600,baseY-180},{720,baseY-230},{900,baseY-300},
            {1050,baseY-220},{1200,baseY-280},{1350,baseY-190},{1500,baseY-250},
            {w,baseY-140},{w,baseY},{0,baseY}};
        int n = sizeof(pts)/sizeof(pts[0]);
        for (int col = 0; col < w; col++) {
            int top = baseY;
            for (int i = 0; i < n-1; i++) {
                if (col >= pts[i][0] && col < pts[i+1][0]) {
                    float t = (float)(col - pts[i][0]) / (pts[i+1][0] - pts[i][0]);
                    int y = (int)(pts[i][1] + t * (pts[i+1][1] - pts[i][1]));
                    if (y < top) top = y;
                    break;
                }
            }
            if (top < baseY) {
                SDL_SetRenderDrawColor(rr,255,185,215,255);
                SDL_RenderDrawLine(rr,col,top,col,baseY);
            }
        }
    }
    /* front range — darker pink/mauve (uses vertical lines instead of per-pixel) */
    {
        int pts[][2] = {{0,baseY},{100,baseY-80},{250,baseY-155},{400,baseY-100},
            {550,baseY-190},{700,baseY-120},{850,baseY-170},{1000,baseY-130},
            {1150,baseY-200},{1300,baseY-110},{1450,baseY-160},{w,baseY-90},
            {w,baseY},{0,baseY}};
        int n = sizeof(pts)/sizeof(pts[0]);
        for (int col = 0; col < w; col++) {
            int top = baseY;
            for (int i = 0; i < n-1; i++) {
                if (col >= pts[i][0] && col < pts[i+1][0]) {
                    float t = (float)(col - pts[i][0]) / (pts[i+1][0] - pts[i][0]);
                    int y = (int)(pts[i][1] + t * (pts[i+1][1] - pts[i][1]));
                    if (y < top) top = y;
                    break;
                }
            }
            if (top < baseY) {
                /* draw a few gradient bands instead of per-pixel */
                int height = baseY - top;
                int bands = 5;
                for (int b = 0; b < bands; b++) {
                    int y1 = top + height * b / bands;
                    int y2 = top + height * (b + 1) / bands;
                    float t = (float)(b + 0.5f) / bands;
                    int rv = (int)(218 + t*18);
                    int gv = (int)(105 + t*35);
                    int bv = (int)(165 + t*30);
                    SDL_SetRenderDrawColor(rr,rv,gv,bv,255);
                    SDL_RenderDrawLine(rr,col,y1,col,y2);
                }
            }
        }
    }
 
    int peaks[][2] = {{200,baseY-210},{480,baseY-260},{900,baseY-300},{1200,baseY-280},{1500,baseY-250}};
    int np = 5;
    for (int p = 0; p < np; p++) {
        int cx = peaks[p][0], cy = peaks[p][1];
        for (int dy = 0; dy < 22; dy++)
            for (int dx = -18+dy; dx <= 18-dy; dx++) {
                int px=cx+dx, py=cy+dy;
                if (px>=0 && px<w) {
                    SDL_SetRenderDrawColor(rr,255,245,252,180);
                    SDL_RenderDrawPoint(rr,px,py);
                }
            }
    }
}


static void drawGraph(SDL_Renderer* rr, TTF_Font* fSmall,
                      double a, double b, double c,
                      double root, int hasRoot,
                      int mouseX, int mouseY) {
    int GX=1100, GY=210, GW=465, GH=380;

    double xMin=-6, xMax=6;
    if (hasRoot) {
        double margin = fabs(root)*0.8 + 3.0;
        xMin = root - margin; xMax = root + margin;
    }
    if (xMax-xMin < 2.0) { double m=(xMin+xMax)/2; xMin=m-1; xMax=m+1; }

    double yMinV=1e18, yMaxV=-1e18;
    for (int i=0;i<=200;i++) {
        double xx=xMin+(double)i/200*(xMax-xMin);
        double yy=feval(xx,a,b,c);
        if (yy<yMinV) yMinV=yy; if (yy>yMaxV) yMaxV=yy;
    }
    double ySpan=yMaxV-yMinV; if (ySpan<1) ySpan=2;
    double yMin=yMinV-ySpan*0.18, yMax=yMaxV+ySpan*0.18;

    drawPanel(rr,GX,GY,GW,GH,(SDL_Color){255,242,250,255},(SDL_Color){215,130,185,255});


    SDL_SetRenderDrawColor(rr,248,225,242,255);
    for (int i=0;i<=10;i++) {
        SDL_RenderDrawLine(rr,GX+i*GW/10,GY,GX+i*GW/10,GY+GH);
        SDL_RenderDrawLine(rr,GX,GY+i*GH/10,GX+GW,GY+i*GH/10);
    }

    int ox=GX+(int)((-xMin)/(xMax-xMin)*GW);
    int oy=GY+GH-(int)((-yMin)/(yMax-yMin)*GH);
    SDL_SetRenderDrawColor(rr,165,50,125,255);
    if (ox>=GX&&ox<=GX+GW) SDL_RenderDrawLine(rr,ox,GY,ox,GY+GH);
    if (oy>=GY&&oy<=GY+GH) SDL_RenderDrawLine(rr,GX,oy,GX+GW,oy);
    renderText(rr,fSmall,"x",GX+GW-13,oy+4,(SDL_Color){165,50,125,255});
    renderText(rr,fSmall,"y",ox+4,GY+4,(SDL_Color){165,50,125,255});

 
    int prevPx=-1, prevPy=-1;
    for (int px=0;px<GW;px++) {
        double xv=xMin+(double)px/GW*(xMax-xMin);
        double yv=feval(xv,a,b,c);
        int sy=GY+GH-(int)((yv-yMin)/(yMax-yMin)*GH);
        if (sy>=GY && sy<=GY+GH) {
            SDL_SetRenderDrawColor(rr,205,50,145,255);
            if (prevPx>=0 && abs(sy-prevPy)<GH/2) {
                SDL_RenderDrawLine(rr,GX+prevPx,prevPy,GX+px,sy);
                SDL_RenderDrawLine(rr,GX+prevPx,prevPy+1,GX+px,sy+1);
            }
            prevPx=px; prevPy=sy;
        }
    }

    if (hasRoot) {
        int rpx=GX+(int)((root-xMin)/(xMax-xMin)*GW);
        double rfy=feval(root,a,b,c);
        int rpy=GY+GH-(int)((rfy-yMin)/(yMax-yMin)*GH);
        if (rpx>=GX && rpx<=GX+GW) {
            SDL_SetRenderDrawColor(rr,200,50,140,200);
            for (int yy=GY;yy<=GY+GH;yy+=4) SDL_RenderDrawPoint(rr,rpx,yy);
            if (rpy>=GY && rpy<=GY+GH) {
                for (int di=-5;di<=5;di++)
                    for (int dj=-5;dj<=5;dj++)
                        if (di*di+dj*dj<=25) {
                            SDL_SetRenderDrawColor(rr,235,55,160,255);
                            SDL_RenderDrawPoint(rr,rpx+di,rpy+dj);
                        }
            }
            char lr[32]; sprintf(lr,"root ~ %.3f",root);
            int lw=textW(fSmall,lr), lx=rpx-lw/2;
            if (lx<GX) lx=GX; if (lx+lw>GX+GW) lx=GX+GW-lw;
            renderText(rr,fSmall,lr,lx,GY+GH+6,(SDL_Color){205,50,145,255});
        }
    }

    char rb[20];
    sprintf(rb,"%.1f",xMin); renderText(rr,fSmall,rb,GX+2,oy+5,(SDL_Color){185,100,165,180});
    sprintf(rb,"%.1f",xMax); renderText(rr,fSmall,rb,GX+GW-30,oy+5,(SDL_Color){185,100,165,180});


    if (mouseX >= GX && mouseX <= GX+GW && mouseY >= GY && mouseY <= GY+GH) {
        SDL_Rect clipG = {GX, GY, GW, GH};
        SDL_RenderSetClipRect(rr, &clipG);
        SDL_SetRenderDrawColor(rr, 170,50,140, 180);
        SDL_RenderDrawLine(rr, mouseX, GY, mouseX, GY+GH);
        SDL_SetRenderDrawColor(rr, 170,50,140, 110);
        SDL_RenderDrawLine(rr, GX, mouseY, GX+GW, mouseY);
        SDL_RenderSetClipRect(rr, NULL);
        double hx = xMin + (double)(mouseX - GX) / GW * (xMax - xMin);
        double hy = feval(hx, a, b, c);
        int hsy = GY+GH-(int)((hy-yMin)/(yMax-yMin)*GH);
        if (hsy >= GY && hsy <= GY+GH) {
            for(int di=-4;di<=4;di++) for(int dj=-4;dj<=4;dj++)
                if(di*di+dj*dj<=16) {
                    SDL_SetRenderDrawColor(rr, 40, 170, 80, 255);
                    SDL_RenderDrawPoint(rr, mouseX+di, hsy+dj);
                }
        }
        char htip[64];
        sprintf(htip, "x = %.3f   y = %.3f", hx, hy);
        int tw = textW(fSmall, htip) + 10;
        int tx = mouseX + 12, ty = mouseY - 22;
        if (tx + tw > GX+GW) tx = mouseX - tw - 6;
        if (ty < GY) ty = GY + 4;
        SDL_SetRenderDrawBlendMode(rr, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(rr, 255, 242, 252, 230);
        SDL_Rect tipBg = {tx-2, ty-2, tw+4, 20};
        SDL_RenderFillRect(rr, &tipBg);
        SDL_SetRenderDrawBlendMode(rr, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(rr, 205, 70, 165, 255);
        SDL_RenderDrawRect(rr, &tipBg);
        renderText(rr, fSmall, htip, tx, ty, (SDL_Color){130, 20, 100, 255});
    }
}


int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window* window = SDL_CreateWindow(
        "Fixed Point Iteration - Quadratic Equation",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 760, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(renderer, WIN_W, WIN_H);

    TTF_Font* fHuge  = TTF_OpenFont("font.ttf", 40);
    TTF_Font* fBig   = TTF_OpenFont("font.ttf", 32);
    TTF_Font* fTitle = TTF_OpenFont("font.ttf", 27);
    TTF_Font* fLarge = TTF_OpenFont("font.ttf", 23);
    TTF_Font* fMed   = TTF_OpenFont("font.ttf", 20);
    TTF_Font* fNorm  = TTF_OpenFont("font.ttf", 18);
    TTF_Font* fSmall = TTF_OpenFont("font.ttf", 16);

    if (!fHuge||!fBig||!fTitle||!fLarge||!fMed||!fNorm||!fSmall) {
        printf("Font error: %s\n", TTF_GetError()); return 1;
    }

    /* Pre-render static background (gradient + mountains + ground) once for performance */
    SDL_Texture* bgTexture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, WIN_W, WIN_H);
    SDL_SetRenderTarget(renderer, bgTexture);
    for (int i=0;i<WIN_H;i++) {
        float t=(float)i/WIN_H;
        int rv=(int)(255 - t*20);
        int gv=(int)(215 - t*70);
        int bv=(int)(235 - t*65);
        SDL_SetRenderDrawColor(renderer,rv,gv,bv,255);
        SDL_RenderDrawLine(renderer,0,i,WIN_W,i);
    }
    drawMountains(renderer, WIN_H-60, WIN_W);
    SDL_SetRenderDrawColor(renderer,215,100,170,255);
    SDL_Rect bgGround = {0, WIN_H-60, WIN_W, 60};
    SDL_RenderFillRect(renderer, &bgGround);
    SDL_SetRenderTarget(renderer, NULL);

    InputBox inputs[4];
    const char* labels[] = {"A","B","C","x0"};
    for (int i=0;i<4;i++) {
        strcpy(inputs[i].label, labels[i]);
        strcpy(inputs[i].value, "");
        inputs[i].active = 0;
        inputs[i].rect = (SDL_Rect){0,0,100,36};
    }

    int selectedMethod = 1;
    MethodOption methods[5];
    const char* formulas[] = {
        "g(x) = -(ax\xC2\xB2 + c) / b",
        "g(x) = -c / (ax + b)",
        "g(x) = \xE2\x88\x9A((-bx - c) / a)",
        "g(x) = -\xE2\x88\x9A((-bx - c) / a)",
        "g(x) = (x\xC2\xB2 - c/a) / (-b/a)"
    };
    for (int i=0;i<5;i++) {
        methods[i].rect = (SDL_Rect){0,0,280,28};
        strcpy(methods[i].formula, formulas[i]);
        methods[i].selected = (i==0);
        methods[i].hovered = 0;
    }

    Button btnCompute     = {{55,  615, 180, 46}, "COMPUTE",      0, 0};
    Button btnClear       = {{260, 615, 180, 46}, "CLEAR",         0, 0};
    Button btnStart       = {{WIN_W/2-120, 620, 240, 56}, "START", 0, 0};
    Button btnBack        = {{1490, 15, 90, 50},  "< BACK",        0, 0};
    Button btnConfirmYes  = {{580, 545, 210, 46},  "YES, COMPUTE",  0, 0};
    Button btnConfirmNo   = {{820, 545, 160, 46},  "CANCEL",        0, 0};

    char   resultText[500] = "";
    double finalRoot = 0, coefA=0, coefB=0, coefC=0;
    int    hasValidRoot = 0;
    IterationRow iterations[MAX_ITER];
    int    totalIterations = 0;
    int    activeInput = -1, quit = 0, tableScrollOffset = 0;
    int    screen = 0;          /* 0=front, 1=loading, 2=solver */
    int    showConfirm = 0;     /* 1 = confirmation modal visible */
    Uint32 loadStartTime = 0;
    float  loadProgress = 0.0f;
    SDL_Event ev;

    SDL_StartTextInput();

    while (!quit) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) quit = 1;

            if (ev.type == SDL_MOUSEBUTTONDOWN) {
                /* Scale physical window coords to logical canvas coords */
                int _ww2,_wh2; SDL_GetWindowSize(window,&_ww2,&_wh2);
                float _sc2=((float)_ww2/WIN_W<(float)_wh2/WIN_H)?(float)_ww2/WIN_W:(float)_wh2/WIN_H;
                int _xo2=(int)((_ww2-_sc2*WIN_W)/2),_yo2=(int)((_wh2-_sc2*WIN_H)/2);
                int mx=(int)((ev.button.x-_xo2)/_sc2),my=(int)((ev.button.y-_yo2)/_sc2);

                /* --- Front page events --- */
                if (screen == 0) {
                    if (mx>=btnStart.rect.x && mx<btnStart.rect.x+btnStart.rect.w &&
                        my>=btnStart.rect.y && my<btnStart.rect.y+btnStart.rect.h) {
                        btnStart.clicked = 1;
                        screen = 1;
                        loadStartTime = SDL_GetTicks();
                        loadProgress = 0.0f;
                    }
                }

                if (screen != 2) continue;

                /* --- BACK button --- */
                if (mx>=btnBack.rect.x && mx<btnBack.rect.x+btnBack.rect.w &&
                    my>=btnBack.rect.y && my<btnBack.rect.y+btnBack.rect.h) {
                    screen = 0;
                    showConfirm = 0;
                    strcpy(resultText,"");
                    hasValidRoot = 0; totalIterations = 0; tableScrollOffset = 0;
                    for (int i=0;i<4;i++) strcpy(inputs[i].value,"");
                    selectedMethod=1; for(int j=0;j<5;j++) methods[j].selected=(j==0);
                    continue;
                }

                /* --- Confirm dialog buttons (block everything else when shown) --- */
                if (showConfirm) {
                    if (mx>=btnConfirmYes.rect.x && mx<btnConfirmYes.rect.x+btnConfirmYes.rect.w &&
                        my>=btnConfirmYes.rect.y && my<btnConfirmYes.rect.y+btnConfirmYes.rect.h) {
                        showConfirm = 0;
                        coefA = atof(inputs[0].value);
                        coefB = atof(inputs[1].value);
                        coefC = atof(inputs[2].value);
                        double x0c = atof(inputs[3].value);
                        int mc = selectedMethod;
                        double x_cur = x0c;
                        int diverged = 0;
                        totalIterations = 0;
                        for (int it=0; it<MAX_ITER; it++) {
                            double x_next = g(x_cur, coefA, coefB, coefC, mc);
                            double error = fabs(x_next - x_cur);
                            iterations[it].xn  = x_cur;
                            iterations[it].xn1 = x_next;
                            iterations[it].error = error;
                            totalIterations++;
                            if (isnan(x_next)||isinf(x_next)||fabs(x_next)>1e10) {diverged=1; break;}
                            x_cur = x_next;
                            if (error < TOLERANCE) break;
                        }
                        finalRoot = x_cur;
                        double verif = fabs(feval(finalRoot, coefA, coefB, coefC));
                        if (diverged || verif > 0.1) {
                            sprintf(resultText,"FAILED: %s\nTry different method or x0",
                                    diverged?"Diverged":"Did not converge");
                            hasValidRoot = 0;
                        } else {
                            sprintf(resultText,"SUCCESS!\nRoot: x = %.3lf\nIterations: %d",
                                    finalRoot, totalIterations);
                            hasValidRoot = 1;
                        }
                        tableScrollOffset = 0;
                    }
                    if (mx>=btnConfirmNo.rect.x && mx<btnConfirmNo.rect.x+btnConfirmNo.rect.w &&
                        my>=btnConfirmNo.rect.y && my<btnConfirmNo.rect.y+btnConfirmNo.rect.h) {
                        showConfirm = 0;
                    }
                    continue; /* modal is blocking */
                }

                /* --- Solver events below --- */
                activeInput = -1;
                for (int i=0;i<4;i++) {
                    inputs[i].active = 0;
                    if (mx>=inputs[i].rect.x && mx<inputs[i].rect.x+inputs[i].rect.w &&
                        my>=inputs[i].rect.y && my<inputs[i].rect.y+inputs[i].rect.h) {
                        activeInput = i; inputs[i].active = 1;
                    }
                }
                for (int i=0;i<5;i++) {
                    if (mx>=methods[i].rect.x && mx<methods[i].rect.x+methods[i].rect.w &&
                        my>=methods[i].rect.y && my<methods[i].rect.y+methods[i].rect.h) {
                        selectedMethod = i+1;
                        for (int j=0;j<5;j++) methods[j].selected = (j==i);
                    }
                }

                /* COMPUTE — open confirmation dialog */
                if (mx>=btnCompute.rect.x && mx<btnCompute.rect.x+btnCompute.rect.w &&
                    my>=btnCompute.rect.y && my<btnCompute.rect.y+btnCompute.rect.h) {
                    btnCompute.clicked = 1;
                    showConfirm = 1;
                }

                /* CLEAR */
                if (mx>=btnClear.rect.x && mx<btnClear.rect.x+btnClear.rect.w &&
                    my>=btnClear.rect.y && my<btnClear.rect.y+btnClear.rect.h) {
                    btnClear.clicked = 1;
                    for (int i=0;i<4;i++) strcpy(inputs[i].value,"");
                    selectedMethod = 1;
                    for (int i=0;i<5;i++) methods[i].selected = (i==0);
                    strcpy(resultText,"");
                    hasValidRoot = 0; totalIterations = 0; tableScrollOffset = 0;
                }
            }

            if (ev.type==SDL_MOUSEBUTTONUP) {
                btnCompute.clicked=0; btnClear.clicked=0; btnStart.clicked=0;
                btnBack.clicked=0; btnConfirmYes.clicked=0; btnConfirmNo.clicked=0;
            }

            if (ev.type==SDL_MOUSEMOTION) {
                /* Scale physical window coords to logical canvas coords */
                int _ww2,_wh2; SDL_GetWindowSize(window,&_ww2,&_wh2);
                float _sc2=((float)_ww2/WIN_W<(float)_wh2/WIN_H)?(float)_ww2/WIN_W:(float)_wh2/WIN_H;
                int _xo2=(int)((_ww2-_sc2*WIN_W)/2),_yo2=(int)((_wh2-_sc2*WIN_H)/2);
                int mx=(int)((ev.motion.x-_xo2)/_sc2),my=(int)((ev.motion.y-_yo2)/_sc2);
                btnStart.hovered   = (mx>=btnStart.rect.x&&mx<btnStart.rect.x+btnStart.rect.w&&
                                      my>=btnStart.rect.y&&my<btnStart.rect.y+btnStart.rect.h);
                if (screen != 2) continue;
                btnBack.hovered       = (mx>=btnBack.rect.x&&mx<btnBack.rect.x+btnBack.rect.w&&
                                         my>=btnBack.rect.y&&my<btnBack.rect.y+btnBack.rect.h);
                btnConfirmYes.hovered = (mx>=btnConfirmYes.rect.x&&mx<btnConfirmYes.rect.x+btnConfirmYes.rect.w&&
                                         my>=btnConfirmYes.rect.y&&my<btnConfirmYes.rect.y+btnConfirmYes.rect.h);
                btnConfirmNo.hovered  = (mx>=btnConfirmNo.rect.x&&mx<btnConfirmNo.rect.x+btnConfirmNo.rect.w&&
                                         my>=btnConfirmNo.rect.y&&my<btnConfirmNo.rect.y+btnConfirmNo.rect.h);
                btnCompute.hovered = (mx>=btnCompute.rect.x&&mx<btnCompute.rect.x+btnCompute.rect.w&&
                                      my>=btnCompute.rect.y&&my<btnCompute.rect.y+btnCompute.rect.h);
                btnClear.hovered   = (mx>=btnClear.rect.x&&mx<btnClear.rect.x+btnClear.rect.w&&
                                      my>=btnClear.rect.y&&my<btnClear.rect.y+btnClear.rect.h);
                for (int i=0;i<5;i++)
                    methods[i].hovered = (mx>=methods[i].rect.x&&mx<methods[i].rect.x+methods[i].rect.w&&
                                          my>=methods[i].rect.y&&my<methods[i].rect.y+methods[i].rect.h);
            }

            if (screen != 2) continue;
            if (ev.type==SDL_TEXTINPUT && activeInput>=0) {
                char ch = ev.text.text[0];
                if ((ch>='0'&&ch<='9')||ch=='.'||ch=='-') {
                    int len=strlen(inputs[activeInput].value);
                    if (len<18) { inputs[activeInput].value[len]=ch; inputs[activeInput].value[len+1]='\0'; }
                }
            }
            if (ev.type==SDL_KEYDOWN && activeInput>=0) {
                if (ev.key.keysym.sym==SDLK_BACKSPACE) {
                    int len=strlen(inputs[activeInput].value);
                    if (len>0) inputs[activeInput].value[len-1]='\0';
                }
                if (ev.key.keysym.sym==SDLK_TAB) {
                    inputs[activeInput].active=0;
                    activeInput=(activeInput+1)%4;
                    inputs[activeInput].active=1;
                }
            }
            if (ev.type==SDL_MOUSEWHEEL && totalIterations>0) {
                tableScrollOffset -= ev.wheel.y*2;
                if (tableScrollOffset<0) tableScrollOffset=0;
                int maxVis=14, maxScr=totalIterations-maxVis;
                if (maxScr<0) maxScr=0;
                if (tableScrollOffset>maxScr) tableScrollOffset=maxScr;
            }
        }

        /* ========================== RENDER ========================== */

        /* --- Update loading progress --- */
        if (screen == 1) {
            Uint32 elapsed = SDL_GetTicks() - loadStartTime;
            loadProgress = (float)elapsed / 5000.0f;
            if (loadProgress >= 1.0f) { loadProgress = 1.0f; screen = 2; }
        }

        /* Background (pre-rendered texture — gradient + mountains + ground) */
        SDL_RenderCopy(renderer, bgTexture, NULL, NULL);

        /* ===================== FRONT PAGE (screen 0) ===================== */
        if (screen == 0) {
            SDL_Color white = {255,255,255,255};
            SDL_Color cream = {255,215,240,255};

            /* decorative top stripe */
            for (int i=0;i<6;i++) {
                SDL_SetRenderDrawColor(renderer,195,45,130,255);
                SDL_RenderDrawLine(renderer,0,i,WIN_W,i);
            }

            /* central card */
            int cw=700, ch=420;
            int cx=WIN_W/2-cw/2, cy=180;
            /* card shadow */
            SDL_SetRenderDrawColor(renderer,185,90,150,60);
            SDL_Rect csh={cx+6,cy+6,cw,ch}; SDL_RenderFillRect(renderer,&csh);
            /* card bg */
            drawPanel(renderer,cx,cy,cw,ch,(SDL_Color){255,242,252,245},(SDL_Color){215,130,185,255});

            /* card header bar */
            SDL_SetRenderDrawColor(renderer,195,45,130,255);
            SDL_Rect chbar={cx,cy,cw,50}; SDL_RenderFillRect(renderer,&chbar);
            renderCenterBold(renderer,fBig,"MT211 - NUMERICAL METHOD",WIN_W/2,cy+8,white);

            /* title */
            renderCenterBold(renderer,fHuge,"FIXED POINT ITERATION",WIN_W/2,cy+72,(SDL_Color){175,30,110,255});
            renderCenterBold(renderer,fTitle,"Quadratic Equation",WIN_W/2,cy+118,(SDL_Color){210,60,150,255});
            renderCenter(renderer,fLarge,"f(x) = ax\xC2\xB2 + bx + c = 0",WIN_W/2,cy+152,(SDL_Color){190,55,130,255});

            /* divider */
            SDL_SetRenderDrawColor(renderer,230,145,195,255);
            SDL_RenderDrawLine(renderer,cx+60,cy+190,cx+cw-60,cy+190);

            /* section label */
            renderCenter(renderer,fMed,"Semestral Project",WIN_W/2,cy+202,(SDL_Color){200,100,165,255});

            /* course */
            renderCenterBold(renderer,fTitle,"BSCPE 22001",WIN_W/2,cy+232,(SDL_Color){195,45,130,255});

            /* divider */
            SDL_SetRenderDrawColor(renderer,230,145,195,255);
            SDL_RenderDrawLine(renderer,cx+120,cy+268,cx+cw-120,cy+268);

            /* authors label */
            renderCenter(renderer,fMed,"Submitted By:",WIN_W/2,cy+278,(SDL_Color){200,100,165,255});

            /* authors */
            renderCenterBold(renderer,fLarge,"Emmanuel Jr Porsona",WIN_W/2,cy+296,(SDL_Color){175,30,110,255});
            renderCenterBold(renderer,fLarge,"Amit Jeed",WIN_W/2,cy+318,(SDL_Color){175,30,110,255});

            /* instructor divider */
            SDL_SetRenderDrawColor(renderer,230,145,195,255);
            SDL_RenderDrawLine(renderer,cx+120,cy+342,cx+cw-120,cy+342);
            renderCenter(renderer,fSmall,"Instructor:",WIN_W/2,cy+350,(SDL_Color){200,100,165,255});
            renderCenterBold(renderer,fMed,"Engr. Edgar Broncano",WIN_W/2,cy+368,(SDL_Color){175,30,110,255});

            /* card bottom bar */
            SDL_SetRenderDrawColor(renderer,195,45,130,255);
            SDL_Rect cbbar={cx,cy+ch-30,cw,30}; SDL_RenderFillRect(renderer,&cbbar);
            renderCenter(renderer,fSmall,"Bestlink College of the Philippines",WIN_W/2,cy+ch-22,cream);

            /* Start button */
            renderButton(renderer,fLarge,&btnStart);

            /* footer hint */
            renderCenter(renderer,fSmall,"Click START to begin",WIN_W/2,WIN_H-85,(SDL_Color){195,100,170,200});

            SDL_RenderPresent(renderer);
            SDL_Delay(16);
            continue;
        }

        /* ================== LOADING SCREEN (screen 1) ================== */
        if (screen == 1) {
            SDL_Color white = {255,255,255,255};
            SDL_Color cream = {255,215,242,255};
            float dtime = (float)SDL_GetTicks() / 1000.0f;

            /* === DRONE (left side) === */
            {
                int dcx = 210, dcy = 210;
                float rotAngle = dtime * 8.0f;
                /* rotor arms */
                SDL_SetRenderDrawColor(renderer,215,130,190,200);
                SDL_RenderDrawLine(renderer,dcx-18,dcy,dcx-56,dcy-38);
                SDL_RenderDrawLine(renderer,dcx+18,dcy,dcx+56,dcy-38);
                SDL_RenderDrawLine(renderer,dcx-18,dcy,dcx-56,dcy+38);
                SDL_RenderDrawLine(renderer,dcx+18,dcy,dcx+56,dcy+38);
                /* body */
                SDL_SetRenderDrawColor(renderer,235,140,200,220);
                SDL_Rect dbody={dcx-18,dcy-11,36,22}; SDL_RenderFillRect(renderer,&dbody);
                SDL_SetRenderDrawColor(renderer,200,85,165,200);
                SDL_RenderDrawRect(renderer,&dbody);
                /* center dot */
                for(int di=-4;di<=4;di++) for(int dj=-4;dj<=4;dj++) if(di*di+dj*dj<=16) {
                    SDL_SetRenderDrawColor(renderer,185,50,145,220);
                    SDL_RenderDrawPoint(renderer,dcx+di,dcy+dj);
                }
                /* 4 spinning rotors */
                int rpos[4][2]={{dcx-56,dcy-38},{dcx+56,dcy-38},{dcx-56,dcy+38},{dcx+56,dcy+38}};
                for (int ri=0;ri<4;ri++) {
                    int rx=rpos[ri][0], ry=rpos[ri][1];
                    for (int ai=0;ai<24;ai++) {
                        float a2 = rotAngle + ai*(3.14159f*2/24);
                        int px2=(int)(rx+26*cos(a2)), py2=(int)(ry+7*sin(a2));
                        SDL_SetRenderDrawColor(renderer,235,160,210,180);
                        SDL_RenderDrawPoint(renderer,px2,py2);
                    }
                    /* rotor hub */
                    SDL_SetRenderDrawColor(renderer,200,85,165,200);
                    for(int di=-3;di<=3;di++) for(int dj=-3;dj<=3;dj++) if(di*di+dj*dj<=9)
                        SDL_RenderDrawPoint(renderer,rx+di,ry+dj);
                }
                /* downward signal lines */
                SDL_SetRenderDrawColor(renderer,235,160,215,120);
                for(int li=0;li<3;li++) {
                    int sep=(li+1)*12;
                    SDL_RenderDrawLine(renderer,dcx-sep,dcy+14,dcx-sep,dcy+28+li*6);
                    SDL_RenderDrawLine(renderer,dcx+sep,dcy+14,dcx+sep,dcy+28+li*6);
                }
                /* label */
                renderText(renderer,fSmall,"DRONE",dcx-20,dcy+72,(SDL_Color){200,100,175,180});
            }

            /* === DRONE 2 (top right) === */
            {
                int dcx=1390, dcy=180;
                float rotAngle = -dtime * 10.0f;
                SDL_SetRenderDrawColor(renderer,215,130,190,180);
                SDL_RenderDrawLine(renderer,dcx-14,dcy,dcx-44,dcy-30);
                SDL_RenderDrawLine(renderer,dcx+14,dcy,dcx+44,dcy-30);
                SDL_RenderDrawLine(renderer,dcx-14,dcy,dcx-44,dcy+30);
                SDL_RenderDrawLine(renderer,dcx+14,dcy,dcx+44,dcy+30);
                SDL_SetRenderDrawColor(renderer,235,140,200,200);
                SDL_Rect dbody2={dcx-14,dcy-9,28,18}; SDL_RenderFillRect(renderer,&dbody2);
                SDL_SetRenderDrawColor(renderer,200,85,165,190); SDL_RenderDrawRect(renderer,&dbody2);
                int rpos2[4][2]={{dcx-44,dcy-30},{dcx+44,dcy-30},{dcx-44,dcy+30},{dcx+44,dcy+30}};
                for(int ri=0;ri<4;ri++) {
                    int rx=rpos2[ri][0], ry=rpos2[ri][1];
                    for(int ai=0;ai<20;ai++) {
                        float a2=rotAngle+ai*(3.14159f*2/20);
                        SDL_SetRenderDrawColor(renderer,235,160,210,160);
                        SDL_RenderDrawPoint(renderer,(int)(rx+20*cos(a2)),(int)(ry+6*sin(a2)));
                    }
                }
                renderText(renderer,fSmall,"DRONE",dcx-18,dcy+50,(SDL_Color){200,100,175,160});
            }

            /* === ROBOT (right side) === */
            {
                int rx=1380, ry=270;
                SDL_Color rc={220,135,195,200};
                SDL_Color rd={185,50,150,220};
                /* antenna */
                SDL_SetRenderDrawColor(renderer,rd.r,rd.g,rd.b,rd.a);
                SDL_RenderDrawLine(renderer,rx+28,ry,rx+28,ry-28);
                for(int di=-5;di<=5;di++) for(int dj=-5;dj<=5;dj++) if(di*di+dj*dj<=25) {
                    SDL_SetRenderDrawColor(renderer,245,185,225,200);
                    SDL_RenderDrawPoint(renderer,rx+28+di,ry-28+dj);
                }
                /* head */
                SDL_SetRenderDrawColor(renderer,rc.r,rc.g,rc.b,rc.a);
                SDL_Rect rhead={rx,ry,56,44}; SDL_RenderFillRect(renderer,&rhead);
                SDL_SetRenderDrawColor(renderer,rd.r,rd.g,rd.b,rd.a); SDL_RenderDrawRect(renderer,&rhead);
                /* eyes */
                SDL_SetRenderDrawColor(renderer,245,210,240,240);
                SDL_Rect leye={rx+10,ry+14,14,12}; SDL_RenderFillRect(renderer,&leye);
                SDL_Rect reye={rx+32,ry+14,14,12}; SDL_RenderFillRect(renderer,&reye);
                /* pupils — animated blink */
                int blink=(int)(dtime*2)%4; /* blink every 2s */
                if(blink!=3) {
                    SDL_SetRenderDrawColor(renderer,185,50,145,255);
                    SDL_Rect lp={rx+14,ry+17,6,7}; SDL_RenderFillRect(renderer,&lp);
                    SDL_Rect rp={rx+36,ry+17,6,7}; SDL_RenderFillRect(renderer,&rp);
                }
                /* mouth */
                SDL_SetRenderDrawColor(renderer,rd.r,rd.g,rd.b,200);
                SDL_RenderDrawLine(renderer,rx+12,ry+36,rx+44,ry+36);
                SDL_RenderDrawLine(renderer,rx+12,ry+36,rx+12,ry+38);
                SDL_RenderDrawLine(renderer,rx+44,ry+36,rx+44,ry+38);
                /* neck */
                SDL_SetRenderDrawColor(renderer,rc.r,rc.g,rc.b,rc.a);
                SDL_Rect neck={rx+20,ry+44,16,10}; SDL_RenderFillRect(renderer,&neck);
                /* body */
                SDL_Rect rbody={rx-8,ry+54,72,80}; SDL_RenderFillRect(renderer,&rbody);
                SDL_SetRenderDrawColor(renderer,rd.r,rd.g,rd.b,rd.a); SDL_RenderDrawRect(renderer,&rbody);
                /* chest panel */
                SDL_SetRenderDrawColor(renderer,245,210,240,160);
                SDL_Rect chest={rx+4,ry+60,44,30}; SDL_RenderFillRect(renderer,&chest);
                /* chest LED strip */
                for(int led=0;led<5;led++) {
                    int ledOn=(int)(dtime*5+led)%5==0;
                    SDL_SetRenderDrawColor(renderer,ledOn?255:200,ledOn?120:90,ledOn?195:165,200);
                    SDL_Rect ledR={rx+8+led*8,ry+68,5,5}; SDL_RenderFillRect(renderer,&ledR);
                }
                /* arms */
                SDL_SetRenderDrawColor(renderer,rc.r,rc.g,rc.b,rc.a);
                float armSwing = (float)sin(dtime*2)*12;
                SDL_Rect larm={rx-28,ry+56,20,(int)(60+armSwing)}; SDL_RenderFillRect(renderer,&larm);
                SDL_Rect rarm={rx+64,ry+56,20,(int)(60-armSwing)}; SDL_RenderFillRect(renderer,&rarm);
                SDL_SetRenderDrawColor(renderer,rd.r,rd.g,rd.b,rd.a);
                SDL_RenderDrawRect(renderer,&larm); SDL_RenderDrawRect(renderer,&rarm);
                /* legs */
                SDL_SetRenderDrawColor(renderer,rc.r,rc.g,rc.b,rc.a);
                SDL_Rect lleg={rx+2,ry+136,26,50}; SDL_RenderFillRect(renderer,&lleg);
                SDL_Rect rleg={rx+28,ry+136,26,50}; SDL_RenderFillRect(renderer,&rleg);
                SDL_SetRenderDrawColor(renderer,rd.r,rd.g,rd.b,rd.a);
                SDL_RenderDrawRect(renderer,&lleg); SDL_RenderDrawRect(renderer,&rleg);
                /* feet */
                SDL_SetRenderDrawColor(renderer,rd.r,rd.g,rd.b,rd.a);
                SDL_Rect lfoot={rx-4,ry+186,34,16}; SDL_RenderFillRect(renderer,&lfoot);
                SDL_Rect rfoot={rx+26,ry+186,34,16}; SDL_RenderFillRect(renderer,&rfoot);
                /* label */
                renderText(renderer,fSmall,"ROBOT",rx+6,ry+210,(SDL_Color){180,100,140,180});
            }

            /* === Circuit trace decorations === */
            SDL_SetRenderDrawColor(renderer,220,145,200,100);
            /* left traces */
            SDL_RenderDrawLine(renderer,30,120,30,820); /* vertical rail */
            SDL_RenderDrawLine(renderer,30,180,120,180);
            SDL_RenderDrawLine(renderer,30,350,120,350);
            SDL_RenderDrawLine(renderer,30,500,80,500);
            /* right traces */
            SDL_RenderDrawLine(renderer,WIN_W-30,100,WIN_W-30,860);
            SDL_RenderDrawLine(renderer,WIN_W-30,240,WIN_W-130,240);
            SDL_RenderDrawLine(renderer,WIN_W-30,440,WIN_W-100,440);
            /* junction dots */
            int jpts[][2]={{30,180},{30,350},{30,500},{WIN_W-30,240},{WIN_W-30,440}};
            for(int ji=0;ji<5;ji++) {
                for(int di=-3;di<=3;di++) for(int dj=-3;dj<=3;dj++) if(di*di+dj*dj<=9) {
                    SDL_SetRenderDrawColor(renderer,215,115,185,160);
                    SDL_RenderDrawPoint(renderer,jpts[ji][0]+di,jpts[ji][1]+dj);
                }
            }

            /* === Dancing women silhouettes === */
            /* Efficient filled-circle: uses horizontal lines instead of per-pixel */
            #define FC(cx,cy,cr,R,G,B,A) do { \
                SDL_SetRenderDrawColor(renderer,R,G,B,A); \
                for(int _j=-(cr);_j<=(cr);_j++) { \
                    int _hw=(int)sqrt((double)((cr)*(cr)-_j*_j)); \
                    SDL_RenderDrawLine(renderer,(cx)-_hw,(cy)+_j,(cx)+_hw,(cy)+_j); \
                } \
            } while(0)
            /* Efficient thick line: draws parallel offset lines instead of per-pixel circles */
            #define THICKLINE(x1,y1,x2,y2,thk,R,G,B,A) do { \
                SDL_SetRenderDrawColor(renderer,R,G,B,A); \
                for(int _t=-(thk);_t<=(thk);_t++) { \
                    int _dx=(x2)-(x1), _dy=(y2)-(y1); \
                    double _len=sqrt((double)(_dx*_dx+_dy*_dy)); \
                    if(_len<1)_len=1; \
                    int _ox=(int)(-_dy*(double)_t/_len), _oy=(int)(_dx*(double)_t/_len); \
                    SDL_RenderDrawLine(renderer,(x1)+_ox,(y1)+_oy,(x2)+_ox,(y2)+_oy); \
                } \
            } while(0)

            /* Animation params — faster, more dramatic */
            float beat  = (float)sin(dtime * 4.2f);
            float beat2 = (float)sin(dtime * 4.2f + 1.5f);
            float beat3 = (float)sin(dtime * 8.4f);
            float hipRoll = (float)sin(dtime * 4.2f + 0.7f); /* hip roll offset */

            /* spotlight glow under each dancer */
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            for(int gs=0;gs<40;gs++) {
                SDL_SetRenderDrawColor(renderer,240,155,210,(Uint8)(18-gs/3));
                SDL_Rect gl1={350-60+gs,700,120-gs*2,20}; SDL_RenderFillRect(renderer,&gl1);
                SDL_Rect gl2={1240-60+gs,700,120-gs*2,20}; SDL_RenderFillRect(renderer,&gl2);
            }
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

            /* -------- WOMAN 1 (left, ballet/Latin pose) -------- */
            {
                int bx=325, by=705;
                int sway   = (int)(beat * 18);
                int hb     = (int)(fabs(beat3) * 9);   /* hip bounce */
                int hipOff = (int)(hipRoll * 14);       /* hip sway side */
                int sx = bx + sway;

                /* --- long flowing hair (drawn first, behind body) --- */
                SDL_SetRenderDrawColor(renderer,45,10,30,215);
                for(int hi=0;hi<22;hi++) {
                    float hw = (float)sin(dtime*5.0f + hi*0.35f)*10 + sway*0.4f;
                    int hx1 = sx - 16 + hi*2;
                    int hy1 = by - 335 - hb;
                    int hx2 = sx - 30 + hi*3 + (int)hw;
                    int hy2 = by - 295 - hb - hi*6;
                    SDL_RenderDrawLine(renderer, hx1, hy1, hx2, hy2);
                    SDL_RenderDrawLine(renderer, hx1+1, hy1, hx2+1, hy2);
                }

                /* --- legs (shapely, stiletto heels) --- */
                float legSw = (float)sin(dtime*4.2f)*30;
                /* left leg — upper thigh to knee */
                int lkx = sx - 10 + (int)(legSw*0.4f), lky = by - 105;
                int lfx = sx - 22 + (int)(legSw*0.9f), lfy = by - 4;
                THICKLINE(sx+hipOff/2-8, by-160-hb, lkx, lky, 7, 70,25,55,220);
                THICKLINE(lkx, lky, lfx, lfy, 5, 70,25,55,220);
                /* pointed toe */
                FC(lfx+4, lfy, 4, 70,25,55,220);
                /* right leg */
                int rkx = sx + 10 - (int)(legSw*0.4f), rky = by - 105;
                int rfx = sx + 14 - (int)(legSw*0.7f), rfy = by - 4;
                THICKLINE(sx+hipOff/2+8, by-160-hb, rkx, rky, 7, 70,25,55,220);
                THICKLINE(rkx, rky, rfx, rfy, 5, 70,25,55,220);
                FC(rfx-4, rfy, 4, 70,25,55,220);

                /* --- hips / pelvis (wide, curvy) --- */
                for(int hi2=0;hi2<38;hi2++) {
                    float curve = (float)sin((float)hi2/37*3.14159f)*18;
                    int hw2 = (int)(32 + curve + hipOff*0.4f);
                    SDL_SetRenderDrawColor(renderer,75,25,55,225);
                    SDL_RenderDrawLine(renderer, sx+hipOff-hw2, by-160-hb+hi2,
                                                sx+hipOff+hw2, by-160-hb+hi2);
                }
                /* --- lower back / buttocks detail --- */
                /* two rounded gluteal masses at base of hip block */
                FC(sx+hipOff-17, by-132-hb, 19, 58,16,46,220);
                FC(sx+hipOff+17, by-132-hb, 19, 58,16,46,220);
                /* gluteal cleft — dark central crease */
                SDL_SetRenderDrawColor(renderer,32,4,22,180);
                for(int gc=0;gc<22;gc++)
                    SDL_RenderDrawLine(renderer, sx+hipOff-1, by-148-hb+gc,
                                                sx+hipOff+1, by-148-hb+gc);
                /* lumbar dimples at lower back */
                FC(sx+hipOff-12, by-166-hb, 3, 48,8,32,145);
                FC(sx+hipOff+12, by-166-hb, 3, 48,8,32,145);
                /* lower-back inward curve highlight */
                SDL_SetRenderDrawColor(renderer,90,34,66,130);
                for(int lb=0;lb<10;lb++) {
                    float lbc=(float)sin((float)lb/9*3.14159f)*5;
                    SDL_RenderDrawLine(renderer, sx-(int)lbc-3, by-205-hb+lb,
                                                sx+(int)lbc+3, by-205-hb+lb);
                }

                /* --- waist (narrowed) --- */
                for(int wi=0;wi<28;wi++) {
                    float curve = (float)sin((float)wi/27*3.14159f)*8;
                    int ww = (int)(13 - curve);
                    SDL_SetRenderDrawColor(renderer,75,25,55,225);
                    SDL_RenderDrawLine(renderer, sx-ww, by-198-hb+wi, sx+ww, by-198-hb+wi);
                }

                /* --- bust / chest (fuller) --- */
                for(int bi2=0;bi2<50;bi2++) {
                    float curve = (float)sin((float)bi2/49*3.14159f)*14;
                    int bw = (int)(18 + curve);
                    SDL_SetRenderDrawColor(renderer,75,25,55,225);
                    SDL_RenderDrawLine(renderer, sx-bw, by-248-hb+bi2, sx+bw, by-248-hb+bi2);
                }

                /* --- arms (one raised overhead, one out) --- */
                float aUp = beat2;
                /* raised left arm */
                int la1x=sx-18, la1y=by-242-hb;
                int la2x=sx-55+(int)(aUp*22), la2y=by-295-hb+(int)(aUp*-28);
                int la3x=la2x-14+(int)(aUp*10), la3y=la2y-42;
                THICKLINE(la1x,la1y, la2x,la2y, 5, 70,25,55,210);
                THICKLINE(la2x,la2y, la3x,la3y, 4, 70,25,55,210);
                FC(la3x,la3y,5,70,25,55,210);
                /* right arm out/flowing */
                int ra1x=sx+18, ra1y=by-242-hb;
                int ra2x=sx+62-(int)(aUp*18), ra2y=by-215-hb+(int)(aUp*20);
                int ra3x=ra2x+18-(int)(aUp*8), ra3y=ra2y+14;
                THICKLINE(ra1x,ra1y, ra2x,ra2y, 5, 70,25,55,210);
                THICKLINE(ra2x,ra2y, ra3x,ra3y, 4, 70,25,55,210);
                FC(ra3x,ra3y,5,70,25,55,210);

                /* --- neck --- */
                THICKLINE(sx,by-248-hb, sx,by-268-hb, 4, 70,25,55,225);

                /* --- head (elegant, slightly tilted) --- */
                int htilt = (int)(beat*5);
                FC(sx+htilt, by-292-hb, 24, 65,20,50,235);

                /* --- flowing skirt hem (animated petals) --- */
                SDL_SetRenderDrawColor(renderer,90,30,65,180);
                for(int sk=0;sk<10;sk++) {
                    float phase=(float)sin(dtime*5+sk*0.628f)*12;
                    int skx = sx - 50 + sk*10;
                    SDL_RenderDrawLine(renderer, skx+(int)phase, by-158-hb,
                                                skx+5+(int)(phase*0.5f), by-4);
                }
            }

            /* -------- WOMAN 2 (right, salsa/hip-hop pose) -------- */
            {
                int bx=1260, by=705;
                int sway   = (int)(-beat * 20);
                int hb     = (int)(fabs(beat3) * 11);
                int hipOff = (int)(-hipRoll * 16);
                int sx = bx + sway;

                /* --- long wavy hair behind body --- */
                SDL_SetRenderDrawColor(renderer,40,8,28,215);
                for(int hi=0;hi<26;hi++) {
                    float hw=(float)sin(dtime*4.5f + hi*0.4f)*12 + sway*0.5f;
                    SDL_RenderDrawLine(renderer, sx+10-hi,    by-348-hb,
                                                sx+38-hi*2+(int)hw, by-298-hb-hi*7);
                    SDL_RenderDrawLine(renderer, sx+11-hi,    by-348-hb,
                                                sx+39-hi*2+(int)hw, by-298-hb-hi*7);
                }

                /* --- legs (high kick pose one leg, standing other) --- */
                float kick2val = (float)sin(dtime*4.2f+0.9f);
                float kick2abs = (float)fabs(kick2val);
                int   kickOut  = (int)(kick2val * 50);
                /* planted right leg */
                int rkx2 = sx+14-(int)(kick2val*10), rky2=by-110;
                int rfx2 = sx+10-(int)(kick2val*8),  rfy2=by-4;
                THICKLINE(sx+hipOff/2+10, by-168-hb, rkx2, rky2, 8, 65,22,52,222);
                THICKLINE(rkx2, rky2, rfx2, rfy2, 6, 65,22,52,222);
                FC(rfx2-5, rfy2, 5, 65,22,52,222);
                /* kicking left leg — swings out to the side, foot always below knee */
                int lkx2 = sx - 16 - kickOut/2;
                int lky2 = by - 112 + (int)(kick2abs*12); /* knee rises slightly at kick */
                int lfx2 = sx - 30 - kickOut;
                int lfy2 = lky2 + 58 + (int)(kick2abs*14); /* foot ALWAYS below knee */
                THICKLINE(sx+hipOff/2-10, by-168-hb, lkx2, lky2, 8, 65,22,52,222);
                THICKLINE(lkx2, lky2, lfx2, lfy2, 6, 65,22,52,222);
                FC(lfx2-5, lfy2, 5, 65,22,52,222);

                /* --- hips (wider, more pronounced) --- */
                for(int hi2=0;hi2<44;hi2++) {
                    float curve=(float)sin((float)hi2/43*3.14159f)*22;
                    int hw2=(int)(36+curve+hipOff*0.5f);
                    SDL_SetRenderDrawColor(renderer,65,22,52,228);
                    SDL_RenderDrawLine(renderer, sx+hipOff-hw2, by-168-hb+hi2,
                                                sx+hipOff+hw2, by-168-hb+hi2);
                }
                /* --- lower back / buttocks detail --- */
                /* two rounded gluteal masses */
                FC(sx+hipOff-19, by-138-hb, 21, 52,14,42,222);
                FC(sx+hipOff+19, by-138-hb, 21, 52,14,42,222);
                /* gluteal cleft */
                SDL_SetRenderDrawColor(renderer,28,2,18,185);
                for(int gc=0;gc<24;gc++)
                    SDL_RenderDrawLine(renderer, sx+hipOff-1, by-156-hb+gc,
                                                sx+hipOff+1, by-156-hb+gc);
                /* lumbar dimples */
                FC(sx+hipOff-13, by-174-hb, 4, 44,6,28,150);
                FC(sx+hipOff+13, by-174-hb, 4, 44,6,28,150);
                /* lower-back arch highlight */
                SDL_SetRenderDrawColor(renderer,80,28,58,125);
                for(int lb=0;lb<10;lb++) {
                    float lbc=(float)sin((float)lb/9*3.14159f)*6;
                    SDL_RenderDrawLine(renderer, sx-(int)lbc-3, by-215-hb+lb,
                                                sx+(int)lbc+3, by-215-hb+lb);
                }

                /* --- waist --- */
                for(int wi=0;wi<30;wi++) {
                    float curve=(float)sin((float)wi/29*3.14159f)*10;
                    int ww=(int)(11-curve);
                    SDL_SetRenderDrawColor(renderer,65,22,52,228);
                    SDL_RenderDrawLine(renderer, sx-ww, by-212-hb+wi, sx+ww, by-212-hb+wi);
                }

                /* --- bust --- */
                for(int bi2=0;bi2<54;bi2++) {
                    float curve=(float)sin((float)bi2/53*3.14159f)*16;
                    int bw=(int)(20+curve);
                    SDL_SetRenderDrawColor(renderer,65,22,52,228);
                    SDL_RenderDrawLine(renderer, sx-bw, by-266-hb+bi2, sx+bw, by-266-hb+bi2);
                }

                /* --- arms (both in dramatic dance positions) --- */
                float aUp2=-beat2;
                /* left arm — sweeping wide */
                int la1x=sx-20, la1y=by-258-hb;
                int la2x=sx-70+(int)(aUp2*24), la2y=by-220-hb+(int)(aUp2*-30);
                int la3x=la2x-20+(int)(aUp2*12), la3y=la2y-14+(int)(aUp2*-12);
                THICKLINE(la1x,la1y, la2x,la2y, 6, 65,22,52,212);
                THICKLINE(la2x,la2y, la3x,la3y, 5, 65,22,52,212);
                FC(la3x,la3y,6,65,22,52,212);
                /* right arm raised high */
                int ra1x=sx+20, ra1y=by-258-hb;
                int ra2x=sx+58-(int)(aUp2*20), ra2y=by-310-hb+(int)(aUp2*-24);
                int ra3x=ra2x+12+(int)(aUp2*8), ra3y=ra2y-46+(int)(aUp2*-10);
                THICKLINE(ra1x,ra1y, ra2x,ra2y, 6, 65,22,52,212);
                THICKLINE(ra2x,ra2y, ra3x,ra3y, 5, 65,22,52,212);
                FC(ra3x,ra3y,6,65,22,52,212);

                /* --- neck --- */
                THICKLINE(sx,by-266-hb, sx,by-288-hb, 5, 65,22,52,228);

                /* --- head (tilted opposite direction) --- */
                int htilt=-((int)(beat*6));
                FC(sx+htilt, by-314-hb, 26, 60,18,48,238);

                /* --- skirt hem flare --- */
                SDL_SetRenderDrawColor(renderer,85,28,62,175);
                for(int sk=0;sk<12;sk++) {
                    float phase=(float)sin(dtime*5.5f+sk*0.523f)*14;
                    int skx = sx-55+sk*10;
                    SDL_RenderDrawLine(renderer, skx+(int)phase, by-165-hb,
                                                skx+6+(int)(phase*0.6f), by-4);
                }
            }
            #undef FC
            #undef THICKLINE

            /* pulsing title */
            float pulse = 0.5f + 0.5f * (float)sin((double)SDL_GetTicks() / 400.0);
            int titleAlpha = (int)(180 + pulse * 75);
            SDL_Color titleCol = {(Uint8)(185+pulse*25), 45, 135, (Uint8)titleAlpha};
            renderCenterBold(renderer,fHuge,"FIXED POINT ITERATION",WIN_W/2,280,titleCol);
            renderCenter(renderer,fTitle,"Quadratic Equation  |  ax\xC2\xB2 + bx + c = 0",WIN_W/2,330,(SDL_Color){210,60,155,255});

            /* loading text */
            const char* loadLabels[] = {"Initializing...","Loading modules...","Preparing solver...","Almost ready...","Launching!"};
            int labelIdx = (int)(loadProgress * 4.99f);
            if (labelIdx > 4) labelIdx = 4;
            renderCenter(renderer,fLarge,loadLabels[labelIdx],WIN_W/2,420,(SDL_Color){195,45,130,255});

            /* progress bar track */
            int barW=500, barH=22;
            int barX=WIN_W/2-barW/2, barY=470;
            drawPanel(renderer,barX,barY,barW,barH,(SDL_Color){255,230,248,255},(SDL_Color){215,130,185,255});

            /* progress bar fill — gradient */
            int fillW = (int)(loadProgress * (barW - 4));
            if (fillW > 0) {
                for (int px=0; px<fillW; px++) {
                    float t = (float)px / (barW - 4);
                    int rv = (int)(210 - t*30);
                    int gv = (int)(55  + t*20);
                    int bv = (int)(155 + t*20);
                    SDL_SetRenderDrawColor(renderer,rv,gv,bv,255);
                    SDL_RenderDrawLine(renderer,barX+2+px,barY+2,barX+2+px,barY+barH-3);
                }
            }

            /* percentage */
            char pctBuf[16];
            sprintf(pctBuf,"%d%%",(int)(loadProgress*100));
            renderCenterBold(renderer,fNorm,pctBuf,WIN_W/2,barY+barH+8,(SDL_Color){195,45,130,255});

            /* authors at bottom */
            renderCenter(renderer,fNorm,"BSCPE 22001  |  Emmanuel Jr Porsona  |  Amit Jeed",WIN_W/2,560,cream);

            /* spinning dots */
            {
                float angle = (float)SDL_GetTicks() / 300.0f;
                int dotCx = WIN_W/2, dotCy = 380;
                for (int d=0; d<6; d++) {
                    float a2 = angle + d * 1.047f;
                    int dx = dotCx + (int)(20 * cos(a2));
                    int dy = dotCy + (int)(20 * sin(a2));
                    int alpha = (int)(100 + 155 * (0.5f + 0.5f * (float)sin(a2 - angle)));
                    for (int di=-3;di<=3;di++) for (int dj=-3;dj<=3;dj++)
                        if (di*di+dj*dj<=9) {
                            SDL_SetRenderDrawColor(renderer,215,50,155,(Uint8)alpha);
                            SDL_RenderDrawPoint(renderer,dx+di,dy+dj);
                        }
                }
            }

            SDL_RenderPresent(renderer);
            SDL_Delay(33); /* 30fps during loading screen to reduce CPU load */
            continue;
        }

        /* =================== SOLVER PAGE (screen 2) =================== */
        /* ——— Header bar ——— */
        SDL_Color white = {255,255,255,255};
        SDL_Color cream = {255,215,242,255};

        for (int i=0;i<80;i++) {
            float t=(float)i/80.0f;
            int rv=(int)(200 - t*30);
            int gv=(int)(45  + t*20);
            int bv=(int)(130 + t*20);
            SDL_SetRenderDrawColor(renderer,rv,gv,bv,255);
            SDL_RenderDrawLine(renderer,0,i,WIN_W,i);
        }

        renderBold(renderer,fTitle,"FIXED POINT ITERATION METHOD",30,10,white);
        renderText(renderer,fLarge,"Quadratic Equation  |  ax\xC2\xB2 + bx + c = 0",30,42,cream);
        renderText(renderer,fSmall,"MT211 - Numerical Method  |  Semestral Project",1080,10,cream);
        renderText(renderer,fNorm, "BSCPE 22001",1200,30,white);
        renderText(renderer,fSmall,"Emmanuel Jr Porsona  |  Amit Jeed",1100,50,cream);
        renderButton(renderer,fSmall,&btnBack);

        /* ===== LEFT PANEL: INPUT ===== */
        SDL_Color panelBg  = {255,242,252,240};
        SDL_Color panBdr   = {215,130,185,255};
        SDL_Color eqBg     = {255,238,250,240};
        SDL_Color eqBdr    = {210,130,180,255};
        SDL_Color secCol   = {185,45,120,255};
        SDL_Color darkTxt  = {120,25,85,255};
        SDL_Color hintCol  = {190,100,165,255};

        drawPanel(renderer,14,90,490,838,panelBg,panBdr);

        /* title bar */
        SDL_SetRenderDrawColor(renderer,195,45,130,255);
        SDL_Rect ptbar={14,90,490,30}; SDL_RenderFillRect(renderer,&ptbar);
        renderBold(renderer,fLarge,"INPUT",22,95,white);
        renderText(renderer,fNorm,"Coefficients  +  Initial Guess  +  g(x)",130,97,cream);

        /* equation display */
        drawPanel(renderer,28,130,462,45,eqBg,eqBdr);
        renderText(renderer,fSmall,"Equation to solve:",40,134,hintCol);
        renderBold(renderer,fMed,"f(x) = ax\xC2\xB2 + bx + c = 0",40,151,(SDL_Color){195,45,120,255});

        /* coefficient inputs */
        drawPanel(renderer,28,183,462,85,eqBg,eqBdr);
        SDL_SetRenderDrawColor(renderer,195,45,130,255);
        SDL_Rect chdr={28,183,462,22}; SDL_RenderFillRect(renderer,&chdr);
        renderBold(renderer,fNorm,"STEP 1 \xe2\x80\x94 Enter coefficients & initial guess",36,186,white);

        inputs[0].rect=(SDL_Rect){42,  228,100,36};
        inputs[1].rect=(SDL_Rect){160, 228,100,36};
        inputs[2].rect=(SDL_Rect){278, 228,100,36};
        inputs[3].rect=(SDL_Rect){396, 228,80,36};
        for (int i=0;i<4;i++) renderInputBox(renderer,fSmall,fNorm,&inputs[i]);

        /* method selection */
        drawPanel(renderer,28,278,462,200,eqBg,eqBdr);
        SDL_SetRenderDrawColor(renderer,195,45,130,255);
        SDL_Rect mhdr={28,278,462,22}; SDL_RenderFillRect(renderer,&mhdr);
        renderBold(renderer,fNorm,"STEP 2 \xe2\x80\x94 Select g(x) transformation",36,281,white);
        renderText(renderer,fSmall,"Choose the form that fits your equation:",42,306,hintCol);

        for (int i=0;i<5;i++) {
            methods[i].rect = (SDL_Rect){42, 326 + i*30, 430, 26};

            if (methods[i].selected) {
                SDL_SetRenderDrawColor(renderer,255,210,238,255);
            } else if (methods[i].hovered) {
                SDL_SetRenderDrawColor(renderer,255,232,248,255);
            } else {
                SDL_SetRenderDrawColor(renderer,255,245,253,255);
            }
            SDL_RenderFillRect(renderer,&methods[i].rect);

            SDL_SetRenderDrawColor(renderer,
                methods[i].selected?200:215,
                methods[i].selected?65:130,
                methods[i].selected?150:185,255);
            SDL_RenderDrawRect(renderer,&methods[i].rect);

            /* radio indicator */
            int circX = methods[i].rect.x + 10;
            int circY = methods[i].rect.y + 13;
            for (int di=-5;di<=5;di++) for (int dj=-5;dj<=5;dj++)
                if (di*di+dj*dj<=25) {
                    SDL_SetRenderDrawColor(renderer,215,130,185,255);
                    SDL_RenderDrawPoint(renderer,circX+di,circY+dj);
                }
            if (methods[i].selected) {
                for (int di=-3;di<=3;di++) for (int dj=-3;dj<=3;dj++)
                    if (di*di+dj*dj<=9) {
                        SDL_SetRenderDrawColor(renderer,210,50,155,255);
                        SDL_RenderDrawPoint(renderer,circX+di,circY+dj);
                    }
            }

            SDL_Color mc = methods[i].selected?(SDL_Color){175,30,120,255}:(SDL_Color){150,60,120,255};
            renderText(renderer,fSmall,methods[i].formula,methods[i].rect.x+22,methods[i].rect.y+5,mc);
        }

        /* equation preview */
        drawPanel(renderer,28,486,462,28,eqBg,eqBdr);
        {
            double pA=atof(inputs[0].value),pB=atof(inputs[1].value),pC=atof(inputs[2].value);
            char prev[128];
            sprintf(prev,"Preview:  f(x) = %.3gx\xC2\xB2 %+.3gx %+.3g",pA,pB,pC);
            renderBold(renderer,fSmall,prev,40,492,(SDL_Color){195,45,120,255});
        }

        /* algorithm reminder */
        drawPanel(renderer,28,522,462,76,eqBg,eqBdr);
        renderText(renderer,fSmall,"Algorithm:",40,526,hintCol);
        renderBold(renderer,fSmall,"1. Start with initial guess x0",40,542,(SDL_Color){185,45,120,255});
        renderText(renderer,fSmall,"2. Compute xn+1 = g(xn)",40,558,darkTxt);
        renderText(renderer,fSmall,"3. Repeat until |xn+1 - xn| < tolerance",40,574,darkTxt);

        /* buttons */
        renderButton(renderer,fNorm,&btnCompute);
        renderButton(renderer,fNorm,&btnClear);

        /* status / result */
        if (strlen(resultText)>0) {
            int isOk = hasValidRoot;
            drawPanel(renderer,28,675,462,80,
                isOk?(SDL_Color){255,235,250,255}:(SDL_Color){255,228,230,255},
                isOk?(SDL_Color){200,120,180,255}:(SDL_Color){210,80,100,255});
            renderBold(renderer,fSmall,isOk?"STATUS: SUCCESS":"STATUS: FAILED",
                       40,679,isOk?(SDL_Color){60,130,80,255}:(SDL_Color){180,20,30,255});
            char resultCopy[500]; strcpy(resultCopy,resultText);
            char* line = strtok(resultCopy,"\n");
            int ry=695;
            while (line) {
                renderText(renderer,fSmall,line,40,ry,
                    isOk?(SDL_Color){120,30,90,255}:(SDL_Color){160,30,40,255});
                ry+=15; line=strtok(NULL,"\n");
            }
        }

        /* conclusion panel */
        if (hasValidRoot) {
            drawPanel(renderer,28,740,462,92,(SDL_Color){255,228,250,255},(SDL_Color){210,115,180,255});
            SDL_SetRenderDrawColor(renderer,195,45,130,255);
            SDL_Rect cbdr={28,740,462,24}; SDL_RenderFillRect(renderer,&cbdr);
            renderBold(renderer,fLarge,"RESULT",200,744,white);

            char buf[200];
            formatEquation(buf,(int)coefA,(int)coefB,(int)coefC);
            renderBold(renderer,fNorm,buf,44,770,(SDL_Color){175,30,120,255});
            sprintf(buf,"Root: x \xe2\x89\x88 %.3lf", finalRoot);
            renderBold(renderer,fNorm,buf,44,790,(SDL_Color){195,50,140,255});
            sprintf(buf,"Iterations: %d   |   Tolerance: %.3lf",totalIterations,TOLERANCE);
            renderText(renderer,fSmall,buf,44,812,hintCol);
        }

        /* ===== MIDDLE PANEL: ITERATION TABLE ===== */
        drawPanel(renderer,516,90,570,838,panelBg,panBdr);

        SDL_SetRenderDrawColor(renderer,195,45,130,255);
        SDL_Rect stbar={516,90,570,30}; SDL_RenderFillRect(renderer,&stbar);
        renderBold(renderer,fLarge,"ITERATION TABLE",570,95,white);

        if (totalIterations > 0) {
            int sy=128;

            /* GIVEN */
            drawPanel(renderer,530,sy,542,42,eqBg,eqBdr);
            renderBold(renderer,fMed,"GIVEN",544,sy+3,secCol);
            SDL_SetRenderDrawColor(renderer,eqBdr.r,eqBdr.g,eqBdr.b,255);
            SDL_RenderDrawLine(renderer,544,sy+20,1060,sy+20);
            {
                char sb[200];
                sprintf(sb,"f(x) = %.3gx\xC2\xB2 %+.3gx %+.3g     x0 = %s     Tol = %.4f",
                        coefA,coefB,coefC,inputs[3].value,TOLERANCE);
                renderText(renderer,fSmall,sb,544,sy+24,darkTxt);
            }
            sy+=48;

            /* table */
            int rowH=20;
            int maxVis=20;
            int tableH=totalIterations*rowH+40;
            int maxTableH=maxVis*rowH+40;
            int dispH=tableH<maxTableH?tableH:maxTableH;

            drawPanel(renderer,530,sy,542,dispH,(SDL_Color){255,245,252,255},eqBdr);
            renderBold(renderer,fMed,"STEP-BY-STEP",544,sy+3,(SDL_Color){140,40,85,255});
            SDL_SetRenderDrawColor(renderer,eqBdr.r,eqBdr.g,eqBdr.b,255);
            SDL_RenderDrawLine(renderer,544,sy+20,1060,sy+20);

            /* column headers */
            SDL_SetRenderDrawColor(renderer,195,45,130,255);
            SDL_Rect hdrBg={531,sy+22,540,18}; SDL_RenderFillRect(renderer,&hdrBg);
            renderBold(renderer,fSmall,"n",   544,sy+23,white);
            renderBold(renderer,fSmall,"xn",580,sy+23,white);
            renderBold(renderer,fSmall,"g(xn) = xn+1",700,sy+23,white);
            renderBold(renderer,fSmall,"|error|",900,sy+23,white);
            renderBold(renderer,fSmall,"Status",1000,sy+23,white);

            SDL_Rect clipR={530,sy+40,542,dispH-46};
            SDL_RenderSetClipRect(renderer,&clipR);

            for (int i=0;i<totalIterations;i++) {
                int ry=sy+42+i*rowH-tableScrollOffset*rowH;
                if (ry<sy+36 || ry>sy+dispH) continue;

                SDL_SetRenderDrawColor(renderer,
                    i%2==0?255:252, i%2==0?242:248, i%2==0?248:255, 255);
                SDL_Rect rowBg={531,ry-1,540,rowH-1};
                SDL_RenderFillRect(renderer,&rowBg);

                if (i==totalIterations-1) {
                    SDL_SetRenderDrawColor(renderer,255,225,235,255);
                    SDL_RenderFillRect(renderer,&rowBg);
                }

                char sb[50];
                sprintf(sb,"%d",i+1);
                renderText(renderer,fSmall,sb,544,ry,darkTxt);
                sprintf(sb,"%.6f",iterations[i].xn);
                renderText(renderer,fSmall,sb,570,ry,darkTxt);
                sprintf(sb,"%.6f",iterations[i].xn1);
                renderText(renderer,fSmall,sb,690,ry,darkTxt);
                sprintf(sb,"%.6f",iterations[i].error);
                renderText(renderer,fSmall,sb,870,ry,
                    iterations[i].error<TOLERANCE?(SDL_Color){50,140,70,255}:(SDL_Color){180,50,80,255});

                if (i==totalIterations-1 && hasValidRoot) {
                    renderBold(renderer,fSmall,"CONVERGED",988,ry,(SDL_Color){50,140,70,255});
                } else if (i==totalIterations-1 && !hasValidRoot) {
                    renderBold(renderer,fSmall,"FAILED",1000,ry,(SDL_Color){180,30,40,255});
                } else {
                    renderText(renderer,fSmall,"...",1012,ry,hintCol);
                }
            }
            SDL_RenderSetClipRect(renderer,NULL);

            /* scrollbar */
            if (totalIterations>maxVis) {
                int maxScr=totalIterations-maxVis;
                if (tableScrollOffset>maxScr) tableScrollOffset=maxScr;
                int sbH=(int)((float)maxVis/totalIterations*(dispH-46));
                if (sbH<20) sbH=20;
                int sbY=sy+40+(int)((float)tableScrollOffset/maxScr*(dispH-46-sbH));
                SDL_SetRenderDrawColor(renderer,215,100,180,200);
                SDL_Rect scrollBar={1066,sbY,6,sbH};
                SDL_RenderFillRect(renderer,&scrollBar);
            }
            sy+=dispH+6;

            /* convergence box */
            int conv = hasValidRoot;
            drawPanel(renderer,530,sy,542,50,
                conv?(SDL_Color){245,255,248,255}:(SDL_Color){255,235,238,255},
                conv?(SDL_Color){80,180,100,255}:(SDL_Color){210,80,100,255});
            renderBold(renderer,fMed,
                conv?"CONVERGED \xe2\x80\x94 ROOT FOUND":"DID NOT CONVERGE",
                544,sy+5,
                conv?(SDL_Color){40,130,60,255}:(SDL_Color){180,30,40,255});
            SDL_SetRenderDrawColor(renderer,conv?80:210,conv?180:80,conv?100:100,255);
            SDL_RenderDrawLine(renderer,544,sy+23,1060,sy+23);
            {
                char sb[200];
                sprintf(sb,"x \xe2\x89\x88 %.3f     f(x) = %.3e     n = %d",
                        finalRoot,feval(finalRoot,coefA,coefB,coefC),totalIterations);
                renderBold(renderer,fNorm,sb,544,sy+28,darkTxt);
            }

        } else {
            int py=380;
            renderCenter(renderer,fNorm,"Enter coefficients and select g(x)",
                         516+570/2,py,hintCol);
            renderCenter(renderer,fNorm,"then press COMPUTE to see iterations.",
                         516+570/2,py+24,hintCol);
            SDL_SetRenderDrawColor(renderer,220,145,200,200);
            SDL_RenderDrawLine(renderer,560,py+58,1060,py+58);
            renderCenter(renderer,fSmall,"Example: a=1, b=-3, c=-4, x0=0",
                         516+570/2,py+70,hintCol);
        }

        /* ===== RIGHT PANEL: GRAPH ===== */
        drawPanel(renderer,1098,90,488,838,panelBg,panBdr);

        SDL_SetRenderDrawColor(renderer,195,45,130,255);
        SDL_Rect gpbar={1098,90,488,30}; SDL_RenderFillRect(renderer,&gpbar);
        renderBold(renderer,fLarge,"GRAPH",1300,95,white);

        { int _pmx,_pmy; SDL_GetMouseState(&_pmx,&_pmy);
          int _ww,_wh; SDL_GetWindowSize(window,&_ww,&_wh);
          float _sc=((float)_ww/WIN_W<(float)_wh/WIN_H)?(float)_ww/WIN_W:(float)_wh/WIN_H;
          int _xo=(int)((_ww-_sc*WIN_W)/2), _yo=(int)((_wh-_sc*WIN_H)/2);
          drawGraph(renderer,fSmall,coefA,coefB,coefC,finalRoot,hasValidRoot,
                    (int)((_pmx-_xo)/_sc),(int)((_pmy-_yo)/_sc)); }

        /* legend */
        int LY=620;
        drawPanel(renderer,1110,LY,462,90,eqBg,eqBdr);
        SDL_SetRenderDrawColor(renderer,195,45,130,255);
        SDL_Rect lhdr={1110,LY,462,22}; SDL_RenderFillRect(renderer,&lhdr);
        renderBold(renderer,fNorm,"LEGEND",1300,LY+4,white);

        SDL_SetRenderDrawColor(renderer,205,50,145,255);
        SDL_Rect l1={1125,LY+36,28,3}; SDL_RenderFillRect(renderer,&l1);
        renderText(renderer,fNorm,"f(x) curve",1163,LY+29,(SDL_Color){205,50,145,255});

        for (int di=-4;di<=4;di++) for (int dj=-4;dj<=4;dj++)
            if (di*di+dj*dj<=16) {
                SDL_SetRenderDrawColor(renderer,235,55,160,255);
                SDL_RenderDrawPoint(renderer,1137+di,LY+58+dj);
            }
        renderText(renderer,fNorm,"Root marker",1163,LY+51,(SDL_Color){235,55,160,255});

        SDL_SetRenderDrawColor(renderer,165,50,125,255);
        SDL_RenderDrawLine(renderer,1125,LY+75,1153,LY+75);
        renderText(renderer,fNorm,"Axes",1163,LY+68,(SDL_Color){165,50,125,255});

        /* footer text in ground area */
        renderCenter(renderer,fSmall,
            "MT211 Numerical Method  |  BSCPE 22001  |  Fixed Point Iteration",
            WIN_W/2, WIN_H-30, (SDL_Color){255,225,248,200});

        /* ============= CONFIRMATION MODAL OVERLAY ============= */
        if (showConfirm) {
            /* dim overlay */
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 60, 10, 40, 180);
            SDL_Rect overlay = {0, 0, WIN_W, WIN_H};
            SDL_RenderFillRect(renderer, &overlay);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

            /* card */
            int mw=620, mh=310;
            int mx2=WIN_W/2-mw/2, my2=WIN_H/2-mh/2;
            /* shadow */
            SDL_SetRenderDrawColor(renderer,40,10,30,255);
            SDL_Rect mshadow={mx2+6,my2+6,mw,mh}; SDL_RenderFillRect(renderer,&mshadow);
            drawPanel(renderer,mx2,my2,mw,mh,(SDL_Color){255,242,252,255},(SDL_Color){215,130,185,255});

            /* card header */
            SDL_SetRenderDrawColor(renderer,195,45,130,255);
            SDL_Rect mhbar={mx2,my2,mw,46}; SDL_RenderFillRect(renderer,&mhbar);
            renderCenterBold(renderer,fTitle,"CONFIRM COMPUTATION",WIN_W/2,my2+8,white);

            /* question */
            renderCenterBold(renderer,fLarge,"Are you sure you want to compute?",WIN_W/2,my2+64,(SDL_Color){175,30,120,255});

            /* divider */
            SDL_SetRenderDrawColor(renderer,225,140,195,255);
            SDL_RenderDrawLine(renderer,mx2+40,my2+96,mx2+mw-40,my2+96);

            /* equation display */
            {
                double pA=atof(inputs[0].value);
                double pB=atof(inputs[1].value);
                double pC=atof(inputs[2].value);
                char eqbuf[150];
                sprintf(eqbuf,"f(x)  =  %.4gx\xC2\xB2  %+.4gx  %+.4g  =  0", pA, pB, pC);
                renderCenter(renderer,fMed,eqbuf,WIN_W/2,my2+108,(SDL_Color){130,25,100,255});
            }

            /* selected g(x) */
            renderCenter(renderer,fSmall,"Selected transformation:",WIN_W/2,my2+140,(SDL_Color){190,100,165,255});
            renderCenterBold(renderer,fMed,methods[selectedMethod-1].formula,WIN_W/2,my2+162,(SDL_Color){195,45,130,255});

            /* initial guess */
            {
                char x0buf[60];
                sprintf(x0buf,"Initial guess:  x0 = %s", inputs[3].value);
                renderCenter(renderer,fSmall,x0buf,WIN_W/2,my2+196,(SDL_Color){175,75,145,255});
            }

            /* divider */
            SDL_SetRenderDrawColor(renderer,225,140,195,255);
            SDL_RenderDrawLine(renderer,mx2+40,my2+222,mx2+mw-40,my2+222);

            /* reposition confirm buttons to center on this card */
            btnConfirmYes.rect = (SDL_Rect){WIN_W/2-220, my2+240, 200, 46};
            btnConfirmNo.rect  = (SDL_Rect){WIN_W/2+20,  my2+240, 160, 46};
            renderButton(renderer,fNorm,&btnConfirmYes);
            renderButton(renderer,fNorm,&btnConfirmNo);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    TTF_CloseFont(fHuge); TTF_CloseFont(fBig); TTF_CloseFont(fTitle);
    TTF_CloseFont(fLarge); TTF_CloseFont(fMed);
    TTF_CloseFont(fNorm); TTF_CloseFont(fSmall);
    SDL_DestroyTexture(bgTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
