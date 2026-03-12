/*
 * Bisection Method - Quadratic Equation Root Finder
 * BSCPE 22002  |  MT 221 Numerical Methods
 * Christian Aldrey B. Doncillo  |  Dexter Gabutin
 */

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define WIN_W 1600
#define WIN_H 930

typedef enum { SCREEN_LANDING, SCREEN_ABOUT, SCREEN_LOADING, SCREEN_SOLVER } Screen;

typedef struct {
    SDL_Rect rect;
    char     label[64];
    char     value[32];
    int      active;
} InputBox;

typedef struct {
    SDL_Rect rect;
    char     text[32];
    int      hovered, clicked;
    int      isBlue;   /* 0 = red button, 1 = blue button */
} Button;

typedef struct {
    int    step;
    double a, b, c;
    double fa, fb, fc;
} BisectIter;

/* ------------------------------------------------------------------ helpers */
static double evalQuad(double x, double A, double B, double C) {
    return A*x*x + B*x + C;
}

static void renderText(SDL_Renderer* r, TTF_Font* f, const char* t,
                       int x, int y, SDL_Color c) {
    if (!t || t[0]=='\0') return;
    SDL_Surface* s = TTF_RenderUTF8_Blended(f, t, c);
    if (!s) return;
    SDL_Texture* tx = SDL_CreateTextureFromSurface(r, s);
    SDL_Rect rc = {x, y, s->w, s->h};
    SDL_RenderCopy(r, tx, NULL, &rc);
    SDL_FreeSurface(s); SDL_DestroyTexture(tx);
}

static void renderBold(SDL_Renderer* r, TTF_Font* f, const char* t,
                       int x, int y, SDL_Color c) {
    renderText(r, f, t, x,   y, c);
    renderText(r, f, t, x+1, y, c);
}

static int textW(TTF_Font* f, const char* t) {
    int w = 0; TTF_SizeText(f, t, &w, NULL); return w;
}

static void renderCenterText(SDL_Renderer* r, TTF_Font* f, const char* t,
                             int cx, int y, SDL_Color c) {
    renderText(r, f, t, cx - textW(f,t)/2, y, c);
}

static void renderCenterBold(SDL_Renderer* r, TTF_Font* f, const char* t,
                             int cx, int y, SDL_Color c) {
    renderBold(r, f, t, cx - textW(f,t)/2, y, c);
}

static void drawPanel(SDL_Renderer* r, int x, int y, int w, int h,
                      SDL_Color bg, SDL_Color border) {
    SDL_SetRenderDrawColor(r, bg.r, bg.g, bg.b, bg.a);
    SDL_Rect rc = {x, y, w, h};
    SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(r, &rc);
}

static void renderInputBox(SDL_Renderer* rr, TTF_Font* fLabel, TTF_Font* fVal,
                           InputBox* box) {
    SDL_Color lc = {180, 60, 120, 255};
    renderBold(rr, fLabel, box->label, box->rect.x, box->rect.y - 18, lc);

    /* background */
    SDL_SetRenderDrawColor(rr,
        box->active ? 255 : 255,
        box->active ? 240 : 248,
        box->active ? 248 : 220, 255);
    SDL_RenderFillRect(rr, &box->rect);

    /* border - hot pink when active, warm yellow-pink when not */
    SDL_SetRenderDrawColor(rr,
        box->active ? 220 : 210,
        box->active ? 60  : 160,
        box->active ? 130 : 80, 255);
    SDL_RenderDrawRect(rr, &box->rect);
    if (box->active) {
        SDL_Rect inn = {box->rect.x+1, box->rect.y+1,
                        box->rect.w-2, box->rect.h-2};
        SDL_RenderDrawRect(rr, &inn);
    }
    if (strlen(box->value) > 0)
        renderText(rr, fVal, box->value,
                   box->rect.x+8, box->rect.y+8,
                   (SDL_Color){100, 20, 60, 255});
}

static void renderButton(SDL_Renderer* rr, TTF_Font* f, Button* btn) {
    SDL_Color bg, shadow;
    if (btn->isBlue) {
        /* "blue" slot -> golden yellow */
        bg     = btn->clicked ? (SDL_Color){180,120,  0,255}
               : btn->hovered ? (SDL_Color){255,210, 30,255}
                              : (SDL_Color){230,175, 10,255};
        shadow = (SDL_Color){120, 80, 0, 255};
    } else {
        /* "red" slot -> hot pink / magenta */
        bg     = btn->clicked ? (SDL_Color){160, 20, 90,255}
               : btn->hovered ? (SDL_Color){255, 80,160,255}
                              : (SDL_Color){220, 45,120,255};
        shadow = (SDL_Color){ 90,  8, 50, 255};
    }
    SDL_SetRenderDrawColor(rr, shadow.r, shadow.g, shadow.b, 255);
    SDL_Rect sh = {btn->rect.x+3, btn->rect.y+3, btn->rect.w, btn->rect.h};
    SDL_RenderFillRect(rr, &sh);
    SDL_SetRenderDrawColor(rr, bg.r, bg.g, bg.b, 255);
    SDL_RenderFillRect(rr, &btn->rect);
    SDL_SetRenderDrawColor(rr, shadow.r+40, shadow.g+20, shadow.b+40, 255);
    SDL_RenderDrawRect(rr, &btn->rect);

    SDL_Surface* s = TTF_RenderText_Blended(f, btn->text,
                                             (SDL_Color){255,255,255,255});
    if (s) {
        SDL_Texture* tx = SDL_CreateTextureFromSurface(rr, s);
        SDL_Rect tr = {btn->rect.x + (btn->rect.w - s->w)/2,
                       btn->rect.y + (btn->rect.h - s->h)/2, s->w, s->h};
        SDL_RenderCopy(rr, tx, NULL, &tr);
        SDL_FreeSurface(s); SDL_DestroyTexture(tx);
    }
}

/* ================================================================== graph == */
static void drawGraph(SDL_Renderer* rr, TTF_Font* fSmall,
                      double A, double B, double C,
                      double a, double b, double root,
                      int hasSol) {
    int GX = 1100, GY = 207, GW = 470, GH = 390;

    double xMin, xMax;
    if (hasSol) {
        double span = b - a;
        xMin = a - span * 0.6;
        xMax = b + span * 0.6;
    } else {
        xMin = -6.0; xMax = 6.0;
    }
    if (xMax - xMin < 2.0) {
        double mid = (xMin+xMax)/2.0;
        xMin = mid-1.0; xMax = mid+1.0;
    }

    double yMinV = 1e18, yMaxV = -1e18;
    for (int i = 0; i <= 200; i++) {
        double xx = xMin + (double)i/200.0 * (xMax-xMin);
        double yy = evalQuad(xx, A, B, C);
        if (yy < yMinV) yMinV = yy;
        if (yy > yMaxV) yMaxV = yy;
    }
    double ySpan = yMaxV - yMinV;
    if (ySpan < 1.0) ySpan = 2.0;
    double yMin = yMinV - ySpan*0.18;
    double yMax = yMaxV + ySpan*0.18;

    /* panel */
    drawPanel(rr, GX, GY, GW, GH,
              (SDL_Color){245,248,255,255}, (SDL_Color){80,110,210,255});

    /* grid */
    SDL_SetRenderDrawColor(rr, 215, 222, 245, 255);
    for (int i = 0; i <= 10; i++) {
        SDL_RenderDrawLine(rr, GX + i*GW/10, GY, GX + i*GW/10, GY+GH);
        SDL_RenderDrawLine(rr, GX, GY + i*GH/10, GX+GW, GY + i*GH/10);
    }

    /* axes */
    int ox = GX + (int)((-xMin)/(xMax-xMin)*GW);
    int oy = GY + GH - (int)((-yMin)/(yMax-yMin)*GH);
    SDL_SetRenderDrawColor(rr, 60, 80, 170, 255);
    if (ox >= GX && ox <= GX+GW) SDL_RenderDrawLine(rr, ox, GY, ox, GY+GH);
    if (oy >= GY && oy <= GY+GH) SDL_RenderDrawLine(rr, GX, oy, GX+GW, oy);
    renderText(rr, fSmall, "x",    GX+GW-14, oy+4,  (SDL_Color){60,80,170,255});
    renderText(rr, fSmall, "f(x)", ox+4,     GY+4,  (SDL_Color){60,80,170,255});

    /* shaded interval [a,b] */
    if (hasSol) {
        SDL_SetRenderDrawBlendMode(rr, SDL_BLENDMODE_BLEND);
        int apx = GX + (int)((a-xMin)/(xMax-xMin)*GW);
        int bpx = GX + (int)((b-xMin)/(xMax-xMin)*GW);
        for (int px = apx; px <= bpx && px <= GX+GW; px++) {
            if (px < GX) continue;
            double xv = xMin + (double)(px-GX)/GW*(xMax-xMin);
            double yv = evalQuad(xv, A, B, C);
            int sy = GY + GH - (int)((yv-yMin)/(yMax-yMin)*GH);
            if (sy < GY) sy = GY;
            if (sy > GY+GH) sy = GY+GH;
            SDL_SetRenderDrawColor(rr, 200, 50, 50, 45);
            if (sy < oy) SDL_RenderDrawLine(rr, px, sy, px, oy);
            else         SDL_RenderDrawLine(rr, px, oy, px, sy);
        }
        SDL_SetRenderDrawBlendMode(rr, SDL_BLENDMODE_NONE);
    }

    /* curve */
    int prevPx = -1, prevPy = -1;
    for (int px = 0; px < GW; px++) {
        double xv = xMin + (double)px/GW*(xMax-xMin);
        double yv = evalQuad(xv, A, B, C);
        int sy = GY + GH - (int)((yv-yMin)/(yMax-yMin)*GH);
        if (sy >= GY && sy <= GY+GH) {
            SDL_SetRenderDrawColor(rr, 190, 20, 20, 255);
            if (prevPx >= 0 && abs(sy-prevPy) < GH/2) {
                SDL_RenderDrawLine(rr, GX+prevPx, prevPy,   GX+px, sy);
                SDL_RenderDrawLine(rr, GX+prevPx, prevPy+1, GX+px, sy+1);
            }
            prevPx = px; prevPy = sy;
        }
    }

    /* root & bound markers */
    if (hasSol) {
        /* a and b dashed verticals */
        int apx = GX + (int)((a-xMin)/(xMax-xMin)*GW);
        int bpx = GX + (int)((b-xMin)/(xMax-xMin)*GW);
        SDL_SetRenderDrawColor(rr, 180, 60, 60, 200);
        for (int yy = GY; yy <= GY+GH; yy += 4) {
            SDL_RenderDrawPoint(rr, apx, yy);
            SDL_RenderDrawPoint(rr, bpx, yy);
        }
        char la[20], lb[20];
        sprintf(la, "a=%.2f", a);
        sprintf(lb, "b=%.2f", b);
        renderText(rr, fSmall, la, apx-14, GY+GH+4, (SDL_Color){180,40,40,255});
        renderText(rr, fSmall, lb, bpx-14, GY+GH+4, (SDL_Color){180,40,40,255});

        /* root dot (blue) */
        double ry = evalQuad(root, A, B, C);
        int rpx = GX + (int)((root-xMin)/(xMax-xMin)*GW);
        int rpy = GY + GH - (int)((ry-yMin)/(yMax-yMin)*GH);
        if (rpx>=GX && rpx<=GX+GW) {
            /* dashed vertical at root */
            SDL_SetRenderDrawColor(rr, 20, 70, 210, 200);
            for (int yy = GY; yy <= GY+GH; yy += 4)
                SDL_RenderDrawPoint(rr, rpx, yy);

            if (rpy >= GY && rpy <= GY+GH) {
                for (int di=-5;di<=5;di++) for (int dj=-5;dj<=5;dj++)
                    if (di*di+dj*dj<=25) {
                        SDL_SetRenderDrawColor(rr, 20, 70, 215, 255);
                        SDL_RenderDrawPoint(rr, rpx+di, rpy+dj);
                    }
            }
            char lr[32];
            sprintf(lr, "root~%.4f", root);
            int lrw = textW(fSmall, lr);
            int lrx = rpx - lrw/2;
            if (lrx < GX) lrx = GX;
            if (lrx + lrw > GX+GW) lrx = GX+GW - lrw;
            renderText(rr, fSmall, lr, lrx, GY+GH+18, (SDL_Color){20,70,210,255});
        }
    }

    /* axis tick labels */
    char rb[20];
    sprintf(rb,"%.1f",xMin);
    renderText(rr,fSmall,rb,GX+2,  oy+5, (SDL_Color){80,80,180,180});
    sprintf(rb,"%.1f",xMax);
    renderText(rr,fSmall,rb,GX+GW-32,oy+5,(SDL_Color){80,80,180,180});
    sprintf(rb,"%.1f",yMax);
    renderText(rr,fSmall,rb,ox+4,  GY+2, (SDL_Color){80,80,180,180});
    sprintf(rb,"%.1f",yMin);
    renderText(rr,fSmall,rb,ox+4,  GY+GH-14,(SDL_Color){80,80,180,180});
}

/* ================================================================= main == */
int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window* window = SDL_CreateWindow(
        "Bisection Method - Quadratic Equation Root Finder",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(renderer, WIN_W, WIN_H);

    TTF_Font* fHuge  = TTF_OpenFont("font.ttf", 38);
    TTF_Font* fBig   = TTF_OpenFont("font.ttf", 28);
    TTF_Font* fTitle = TTF_OpenFont("font.ttf", 24);
    TTF_Font* fLarge = TTF_OpenFont("font.ttf", 20);
    TTF_Font* fMed   = TTF_OpenFont("font.ttf", 17);
    TTF_Font* fNorm  = TTF_OpenFont("font.ttf", 15);
    TTF_Font* fSmall = TTF_OpenFont("font.ttf", 13);

    if (!fHuge||!fBig||!fTitle||!fLarge||!fMed||!fNorm||!fSmall) {
        printf("Font error: %s\n", TTF_GetError()); return 1;
    }

    Screen screen   = SCREEN_LANDING;
    Uint32 loadStart = 0;
    int    hoverSolver = 0, hoverAbout = 0;

    /* inputs: A, B, C, a, b, tolerance, maxIter */
    InputBox inputs[7];
    const char* labels[7]   = {"A","B","C","a (lower)","b (upper)","Tolerance","Max Iter"};
    const char* defaults[7] = {"1","-3","-4","-2","0","0.0001","50"};
    for (int i = 0; i < 7; i++) {
        strcpy(inputs[i].label, labels[i]);
        strcpy(inputs[i].value, defaults[i]);
        inputs[i].active = 0;
        inputs[i].rect   = (SDL_Rect){0,0,90,36};
    }

    Button btnCompute    = {{55, 578, 175, 46}, "COMPUTE",  0, 0, 0};
    Button btnClear      = {{255,578, 175, 46}, "CLEAR",    0, 0, 1};
    Button btnBack       = {{30,  30, 160, 46}, "< BACK",   0, 0, 1};
    Button btnBackSolver = {{1390, 94, 120, 36},"< MENU",   0, 0, 1};

    int        hasSol = 0, hasError = 0, activeInput = -1, quit = 0;
    double     sA=1, sB=-3, sC=-4, sa=-2, sb=0, sTol=0.0001;
    int        sMaxIter=50;
    double     sRoot=0;
    int        sIterCount=0;
    char       errorMsg[200] = "";
    BisectIter iters[200];
    int        iterCount=0;
    int        scrollY=0;

    SDL_StartTextInput();
    SDL_Event ev;

    while (!quit) {
        SDL_Rect cardSolver = {280, 310, 420, 290};
        SDL_Rect cardAbout  = {900, 310, 420, 290};

        /* ======================= EVENT LOOP ======================= */
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) quit = 1;

            /* ---------- LANDING ---------- */
            if (screen == SCREEN_LANDING) {
                if (ev.type == SDL_MOUSEBUTTONDOWN) {
                    int mx = ev.button.x, my = ev.button.y;
                    if (mx>=cardSolver.x && mx<cardSolver.x+cardSolver.w &&
                        my>=cardSolver.y && my<cardSolver.y+cardSolver.h) {
                        screen = SCREEN_LOADING; loadStart = SDL_GetTicks();
                    }
                    if (mx>=cardAbout.x && mx<cardAbout.x+cardAbout.w &&
                        my>=cardAbout.y && my<cardAbout.y+cardAbout.h)
                        screen = SCREEN_ABOUT;
                }
                if (ev.type == SDL_MOUSEMOTION) {
                    int mx = ev.motion.x, my = ev.motion.y;
                    hoverSolver = (mx>=cardSolver.x && mx<cardSolver.x+cardSolver.w &&
                                   my>=cardSolver.y && my<cardSolver.y+cardSolver.h);
                    hoverAbout  = (mx>=cardAbout.x  && mx<cardAbout.x+cardAbout.w  &&
                                   my>=cardAbout.y  && my<cardAbout.y+cardAbout.h);
                }
            }

            /* ---------- ABOUT ---------- */
            else if (screen == SCREEN_ABOUT) {
                if (ev.type == SDL_MOUSEBUTTONDOWN) {
                    int mx=ev.button.x, my=ev.button.y;
                    if (mx>=btnBack.rect.x && mx<btnBack.rect.x+btnBack.rect.w &&
                        my>=btnBack.rect.y && my<btnBack.rect.y+btnBack.rect.h)
                        screen = SCREEN_LANDING;
                }
                if (ev.type == SDL_MOUSEMOTION) {
                    int mx=ev.motion.x, my=ev.motion.y;
                    btnBack.hovered = (mx>=btnBack.rect.x &&
                                       mx<btnBack.rect.x+btnBack.rect.w &&
                                       my>=btnBack.rect.y &&
                                       my<btnBack.rect.y+btnBack.rect.h);
                }
            }

            /* ---------- SOLVER ---------- */
            else if (screen == SCREEN_SOLVER) {
                if (ev.type == SDL_MOUSEWHEEL) {
                    scrollY -= ev.wheel.y * 20;
                    if (scrollY < 0) scrollY = 0;
                }

                if (ev.type == SDL_MOUSEBUTTONDOWN) {
                    int mx=ev.button.x, my=ev.button.y;
                    activeInput = -1;
                    for (int i=0;i<7;i++) {
                        inputs[i].active = 0;
                        if (mx>=inputs[i].rect.x && mx<inputs[i].rect.x+inputs[i].rect.w &&
                            my>=inputs[i].rect.y && my<inputs[i].rect.y+inputs[i].rect.h) {
                            activeInput = i; inputs[i].active = 1;
                        }
                    }

                    /* back button */
                    if (mx>=btnBackSolver.rect.x &&
                        mx<btnBackSolver.rect.x+btnBackSolver.rect.w &&
                        my>=btnBackSolver.rect.y &&
                        my<btnBackSolver.rect.y+btnBackSolver.rect.h) {
                        screen=SCREEN_LANDING; hasSol=0; hasError=0; scrollY=0;
                    }

                    /* COMPUTE */
                    if (mx>=btnCompute.rect.x &&
                        mx<btnCompute.rect.x+btnCompute.rect.w &&
                        my>=btnCompute.rect.y &&
                        my<btnCompute.rect.y+btnCompute.rect.h) {
                        btnCompute.clicked=1;
                        hasSol=0; hasError=0; scrollY=0;
                        strcpy(errorMsg,"");

                        sA       = atof(inputs[0].value);
                        sB       = atof(inputs[1].value);
                        sC       = atof(inputs[2].value);
                        sa       = atof(inputs[3].value);
                        sb       = atof(inputs[4].value);
                        sTol     = atof(inputs[5].value);
                        sMaxIter = atoi(inputs[6].value);

                        double fa = evalQuad(sa, sA, sB, sC);
                        double fb = evalQuad(sb, sA, sB, sC);

                        if (fabs(sa-sb) < 1e-14) {
                            hasError=1; strcpy(errorMsg,"a and b cannot be equal.");
                        } else if (sa >= sb) {
                            hasError=1; strcpy(errorMsg,"a must be strictly less than b.");
                        } else if (sTol <= 0.0) {
                            hasError=1; strcpy(errorMsg,"Tolerance must be a positive number.");
                        } else if (sMaxIter<1 || sMaxIter>200) {
                            hasError=1; strcpy(errorMsg,"Max iterations must be between 1 and 200.");
                        } else if (fa*fb > 0) {
                            hasError=1; strcpy(errorMsg,
                                "f(a) and f(b) must have OPPOSITE signs — no root guaranteed in [a,b].");
                        } else {
                            /* run bisection */
                            iterCount=0;
                            double ca=sa, cb=sb, cfa=fa, cfb=fb, c=ca, fc=cfa;
                            for (int k=0; k<sMaxIter && iterCount<200; k++) {
                                c  = (ca+cb)/2.0;
                                fc = evalQuad(c, sA, sB, sC);
                                iters[iterCount].step = k+1;
                                iters[iterCount].a    = ca;
                                iters[iterCount].b    = cb;
                                iters[iterCount].c    = c;
                                iters[iterCount].fa   = cfa;
                                iters[iterCount].fb   = cfb;
                                iters[iterCount].fc   = fc;
                                iterCount++;

                                if (fabs(fc)<sTol || (cb-ca)/2.0<sTol) break;

                                if (cfa*fc < 0) { cb=c; cfb=fc; }
                                else             { ca=c; cfa=fc; }
                            }
                            sRoot      = c;
                            sIterCount = iterCount;
                            hasSol     = 1;
                        }
                    }

                    /* CLEAR */
                    if (mx>=btnClear.rect.x && mx<btnClear.rect.x+btnClear.rect.w &&
                        my>=btnClear.rect.y && my<btnClear.rect.y+btnClear.rect.h) {
                        btnClear.clicked=1;
                        for (int i=0;i<7;i++) strcpy(inputs[i].value,"");
                        hasSol=0; hasError=0; scrollY=0; strcpy(errorMsg,"");
                    }
                }

                if (ev.type==SDL_MOUSEBUTTONUP) {
                    btnCompute.clicked=0; btnClear.clicked=0;
                }

                if (ev.type==SDL_MOUSEMOTION) {
                    int mx=ev.motion.x, my=ev.motion.y;
#define HOVER(b) ((b).hovered=(mx>=(b).rect.x&&mx<(b).rect.x+(b).rect.w&&\
                               my>=(b).rect.y&&my<(b).rect.y+(b).rect.h))
                    HOVER(btnCompute); HOVER(btnClear); HOVER(btnBackSolver);
                }

                if (ev.type==SDL_TEXTINPUT && activeInput>=0) {
                    char ch = ev.text.text[0];
                    int isInt = (activeInput==6);
                    /* allow digits, dot (not for maxIter), minus, 'e'/'E' for tolerance */
                    int valid = (ch>='0'&&ch<='9')
                             || (!isInt && (ch=='.' || ch=='e' || ch=='E'))
                             || (ch=='-');
                    if (valid) {
                        int len = strlen(inputs[activeInput].value);
                        if (len < 18) {
                            inputs[activeInput].value[len]   = ch;
                            inputs[activeInput].value[len+1] = '\0';
                        }
                    }
                }

                if (ev.type==SDL_KEYDOWN && activeInput>=0) {
                    if (ev.key.keysym.sym==SDLK_BACKSPACE) {
                        int len=strlen(inputs[activeInput].value);
                        if (len>0) inputs[activeInput].value[len-1]='\0';
                    }
                    if (ev.key.keysym.sym==SDLK_TAB) {
                        inputs[activeInput].active=0;
                        activeInput=(activeInput+1)%7;
                        inputs[activeInput].active=1;
                    }
                }
            }
        }

        /* loading timeout */
        if (screen==SCREEN_LOADING) {
            if (SDL_GetTicks()-loadStart >= 5000) screen=SCREEN_SOLVER;
        }

        /* ========================= RENDER =========================== */

        /* -------- LANDING -------- */
        if (screen==SCREEN_LANDING) {
            /* gradient background: soft pink -> warm cream yellow */
            for (int i=0;i<WIN_H;i++) {
                float t=(float)i/WIN_H;
                SDL_SetRenderDrawColor(renderer,
                    255,
                    (int)(200 + t*45),
                    (int)(220 - t*110),255);
                SDL_RenderDrawLine(renderer,0,i,WIN_W,i);
            }

            /* top banner: deep pink -> hot pink */
            for (int i=0;i<96;i++) {
                float t=(float)i/96.0f;
                int rv=(int)(200 + t*30), gv=(int)(20 + t*30), bv=(int)(100 + t*20);
                SDL_SetRenderDrawColor(renderer,rv,gv,bv,255);
                SDL_RenderDrawLine(renderer,0,i,WIN_W,i);
            }
            /* yellow stripe */
            SDL_SetRenderDrawColor(renderer,255,210,10,255);
            SDL_Rect stripe={0,96,WIN_W,8};
            SDL_RenderFillRect(renderer,&stripe);

            int cx=WIN_W/2;
            renderCenterBold(renderer,fHuge,"BISECTION METHOD",
                             cx,10,(SDL_Color){255,255,220,255});
            renderCenterBold(renderer,fBig,"Quadratic Equation Root Finder",
                             cx,57,(SDL_Color){255,245,180,255});

            /* Manji (\xE5\x8D\x90) decorative accents flanking the banner */
            SDL_Color manjiCol = {255,235,140,255};
            renderBold(renderer,fHuge,"\xE5\x8D\x90", 30,  12, manjiCol);
            renderBold(renderer,fHuge,"\xE5\x8D\x90", WIN_W - 80, 12, manjiCol);
            renderBold(renderer,fBig, "\xE5\x8D\x90", 55,  58, manjiCol);
            renderBold(renderer,fBig, "\xE5\x8D\x90", WIN_W - 65, 58, manjiCol);

            /* subtitle */
            renderCenterText(renderer,fLarge,
                "Find real roots of  f(x) = Ax\xC2\xB2 + Bx + C  using the Bisection Method",
                cx,120,(SDL_Color){160,30,90,255});

            /* ---- SOLVER CARD ---- */
            {
                SDL_Color bg = hoverSolver
                    ? (SDL_Color){255,230,240,255}
                    : (SDL_Color){255,245,250,255};

                SDL_SetRenderDrawColor(renderer,200,60,120,120);
                SDL_Rect sh={cardSolver.x+6,cardSolver.y+6,cardSolver.w,cardSolver.h};
                SDL_RenderFillRect(renderer,&sh);

                SDL_SetRenderDrawColor(renderer,bg.r,bg.g,bg.b,255);
                SDL_RenderFillRect(renderer,&cardSolver);
                /* hot pink left accent */
                SDL_SetRenderDrawColor(renderer,230,40,110,255);
                SDL_Rect lb2={cardSolver.x,cardSolver.y,6,cardSolver.h};
                SDL_RenderFillRect(renderer,&lb2);
                SDL_SetRenderDrawColor(renderer,230,130,180,255);
                SDL_RenderDrawRect(renderer,&cardSolver);

                /* calculator icon (yellow) */
                int ix=cardSolver.x+cardSolver.w/2-22, iy=cardSolver.y+55;
                SDL_SetRenderDrawColor(renderer,230,175,10,255);
                SDL_Rect ico={ix,iy,44,54};
                SDL_RenderFillRect(renderer,&ico);
                SDL_SetRenderDrawColor(renderer,255,235,200,255);
                SDL_Rect disp={ix+4,iy+5,36,12};
                SDL_RenderFillRect(renderer,&disp);
                SDL_SetRenderDrawColor(renderer,255,255,220,255);
                for (int row=0;row<3;row++)
                    for (int col=0;col<3;col++) {
                        SDL_Rect btn2={ix+4+col*13,iy+22+row*11,10,8};
                        SDL_RenderFillRect(renderer,&btn2);
                    }

                renderCenterBold(renderer,fBig,"SOLVER",
                    cardSolver.x+cardSolver.w/2, cardSolver.y+172,
                    (SDL_Color){180,20,80,255});
                renderCenterText(renderer,fNorm,"Solve bisection step-by-step",
                    cardSolver.x+cardSolver.w/2, cardSolver.y+210,
                    (SDL_Color){200,50,110,255});
                renderCenterText(renderer,fSmall,"Click to open",
                    cardSolver.x+cardSolver.w/2, cardSolver.y+235,
                    (SDL_Color){210,100,150,255});
            }

            /* ---- ABOUT CARD ---- */
            {
                SDL_Color bg = hoverAbout
                    ? (SDL_Color){255,248,200,255}
                    : (SDL_Color){255,252,220,255};

                SDL_SetRenderDrawColor(renderer,180,140,10,120);
                SDL_Rect sh={cardAbout.x+6,cardAbout.y+6,cardAbout.w,cardAbout.h};
                SDL_RenderFillRect(renderer,&sh);

                SDL_SetRenderDrawColor(renderer,bg.r,bg.g,bg.b,255);
                SDL_RenderFillRect(renderer,&cardAbout);
                /* yellow left accent */
                SDL_SetRenderDrawColor(renderer,230,175,10,255);
                SDL_Rect rb2={cardAbout.x,cardAbout.y,6,cardAbout.h};
                SDL_RenderFillRect(renderer,&rb2);
                SDL_SetRenderDrawColor(renderer,230,195,80,255);
                SDL_RenderDrawRect(renderer,&cardAbout);

                /* info icon (pink) */
                int ix=cardAbout.x+cardAbout.w/2-22, iy=cardAbout.y+55;
                SDL_SetRenderDrawColor(renderer,220,45,120,255);
                SDL_Rect ib={ix,iy,44,44};
                SDL_RenderFillRect(renderer,&ib);
                int tw=textW(fLarge,"i");
                renderBold(renderer,fLarge,"i",ix+22-tw/2,iy+8,
                           (SDL_Color){255,255,220,255});

                renderCenterBold(renderer,fBig,"ABOUT US",
                    cardAbout.x+cardAbout.w/2, cardAbout.y+172,
                    (SDL_Color){150,100,0,255});
                renderCenterText(renderer,fNorm,"Authors and program info",
                    cardAbout.x+cardAbout.w/2, cardAbout.y+210,
                    (SDL_Color){170,120,10,255});
                renderCenterText(renderer,fSmall,"Click to open",
                    cardAbout.x+cardAbout.w/2, cardAbout.y+235,
                    (SDL_Color){190,140,20,255});
            }

            /* footer */
            SDL_SetRenderDrawColor(renderer,220,40,110,255);
            SDL_Rect fb1={0,WIN_H-50,WIN_W,50}; SDL_RenderFillRect(renderer,&fb1);
            SDL_SetRenderDrawColor(renderer,255,210,10,255);
            SDL_Rect fb2={0,WIN_H-50,WIN_W,5}; SDL_RenderFillRect(renderer,&fb2);
            renderCenterText(renderer,fNorm,
                "BSCPE 22002  |  MT 221 Numerical Methods  |  Bisection Method",
                cx,WIN_H-34,(SDL_Color){255,248,210,255});
        }

        /* -------- ABOUT -------- */
        else if (screen==SCREEN_ABOUT) {
            /* pink-to-yellow gradient */
            for (int i=0;i<WIN_H;i++) {
                float t=(float)i/WIN_H;
                SDL_SetRenderDrawColor(renderer,
                    255,
                    (int)(190 + t*55),
                    (int)(215 - t*115), 255);
                SDL_RenderDrawLine(renderer,0,i,WIN_W,i);
            }
            int cx=WIN_W/2;

            renderButton(renderer,fNorm,&btnBack);
            renderCenterBold(renderer,fHuge,"ABOUT US",cx,50,
                             (SDL_Color){170,20,80,255});

            /* two-tone underline: pink + yellow */
            SDL_SetRenderDrawColor(renderer,225,40,110,255);
            SDL_Rect d1={350,96,420,4}; SDL_RenderFillRect(renderer,&d1);
            SDL_SetRenderDrawColor(renderer,240,190,10,255);
            SDL_Rect d2={770,96,430,4}; SDL_RenderFillRect(renderer,&d2);

            drawPanel(renderer,230,112,1140,750,
                      (SDL_Color){255,248,235,255},(SDL_Color){230,130,170,255});

            renderCenterBold(renderer,fBig,"BISECTION METHOD",cx,135,
                             (SDL_Color){180,20,80,255});
            renderCenterBold(renderer,fTitle,"QUADRATIC EQUATION ROOT FINDER",cx,173,
                             (SDL_Color){160,110,0,255});

            SDL_SetRenderDrawColor(renderer,230,160,100,255);
            SDL_RenderDrawLine(renderer,350,212,1250,212);

            renderCenterBold(renderer,fTitle,"NUMERICAL METHOD  |  MT 221",cx,230,
                             (SDL_Color){200,50,110,255});
            renderCenterBold(renderer,fLarge,"A FINAL PROJECT",cx,268,
                             (SDL_Color){160,110,0,255});
            renderCenterBold(renderer,fLarge,"BSCPE 22002",cx,298,
                             (SDL_Color){180,20,80,255});

            SDL_SetRenderDrawColor(renderer,235,175,80,255);
            SDL_RenderDrawLine(renderer,350,335,1250,335);

            renderCenterBold(renderer,fTitle,"PROGRAM CREATED BY:",cx,354,
                             (SDL_Color){180,20,80,255});
            renderCenterBold(renderer,fBig,"DONCILLO, CHRISTIAN ALDREY B.",cx,400,
                             (SDL_Color){150,95,0,255});
            renderCenterBold(renderer,fBig,"GABUTIN, DEXTER",cx,442,
                             (SDL_Color){150,95,0,255});

            SDL_SetRenderDrawColor(renderer,230,160,100,255);
            SDL_RenderDrawLine(renderer,350,485,1250,485);

            SDL_Color desc={170,60,100,255};
            renderCenterText(renderer,fNorm,
                "This program solves for the real root of f(x) = Ax\xC2\xB2 + Bx + C",cx,510,desc);
            renderCenterText(renderer,fNorm,
                "using the Bisection Method, a reliable bracketing root-finding technique.",cx,535,desc);
            renderCenterText(renderer,fNorm,
                "Requirements: supply an interval [a, b] such that f(a) and f(b) have",cx,560,desc);
            renderCenterText(renderer,fNorm,
                "opposite signs, a convergence tolerance, and a maximum iteration count.",cx,585,desc);
            renderCenterText(renderer,fNorm,
                "The algorithm halves the interval each step, converging to the root.",cx,615,desc);
            renderCenterText(renderer,fNorm,
                "Output: full iteration table, graph with shaded interval, and root value.",cx,640,desc);

            renderCenterText(renderer,fSmall,"Built with SDL2 and C  |  2026",
                             cx,698,(SDL_Color){190,120,60,255});

            /* bottom color bar: pink + yellow */
            SDL_SetRenderDrawColor(renderer,225,40,110,255);
            SDL_Rect bar1={230,840,570,6}; SDL_RenderFillRect(renderer,&bar1);
            SDL_SetRenderDrawColor(renderer,240,190,10,255);
            SDL_Rect bar2={800,840,570,6}; SDL_RenderFillRect(renderer,&bar2);
        }

        /* -------- LOADING -------- */
        else if (screen==SCREEN_LOADING) {
            /* deep pink -> warm golden gradient */
            for (int i=0;i<WIN_H;i++) {
                float t=(float)i/WIN_H;
                int rv=255;
                int gv=(int)(30  + t*160);
                int bv=(int)(120 - t*100);
                SDL_SetRenderDrawColor(renderer,rv,gv,bv,255);
                SDL_RenderDrawLine(renderer,0,i,WIN_W,i);
            }

            int cx=WIN_W/2;
            Uint32 elapsed=SDL_GetTicks()-loadStart;
            float  progress=(float)elapsed/5000.0f;
            if (progress>1.0f) progress=1.0f;

            renderCenterBold(renderer,fHuge,"BISECTION METHOD",         cx, 80,(SDL_Color){255,252,210,255});
            renderCenterBold(renderer,fBig, "QUADRATIC EQUATION",       cx,130,(SDL_Color){255,248,200,255});
            renderCenterBold(renderer,fBig, "ROOT FINDER",              cx,168,(SDL_Color){255,248,200,255});

            /* separator: yellow */
            SDL_SetRenderDrawColor(renderer,255,220,20,255);
            SDL_Rect sep={cx-220,214,440,4}; SDL_RenderFillRect(renderer,&sep);

            renderCenterBold(renderer,fBig, "NUMERICAL METHOD  MT 221", cx,228,(SDL_Color){255,235,180,255});
            renderCenterBold(renderer,fBig, "A FINAL PROJECT",          cx,270,(SDL_Color){255,235,180,255});
            renderCenterBold(renderer,fBig, "PROGRAM CREATED BY:",      cx,350,(SDL_Color){255,252,210,255});
            renderCenterBold(renderer,fBig, "DONCILLO, CHRISTIAN ALDREY B.",cx,392,
                             (SDL_Color){255,240,170,255});
            renderCenterBold(renderer,fBig, "GABUTIN, DEXTER",          cx,432,
                             (SDL_Color){255,240,170,255});

            renderCenterText(renderer,fLarge,"Loading, please wait...", cx,515,
                             (SDL_Color){255,230,160,255});

            /* progress bar */
            int barW=520, barH=44, barX=cx-barW/2, barY=555;

            /* glow: yellow outline */
            SDL_SetRenderDrawColor(renderer,255,220,20,220);
            SDL_Rect glow={barX-4,barY-4,barW+8,barH+8};
            SDL_RenderDrawRect(renderer,&glow);

            SDL_SetRenderDrawColor(renderer,100,10,40,255);
            SDL_Rect inner={barX,barY,barW,barH};
            SDL_RenderFillRect(renderer,&inner);

            int fillW=(int)(barW*progress);
            for (int px=0;px<fillW;px++) {
                float t=(float)px/barW;
                /* pink -> yellow sweep */
                int rv=255;
                int gv=(int)(40  + t*180);
                int bv=(int)(130 - t*120);
                SDL_SetRenderDrawColor(renderer,rv,gv,bv,255);
                SDL_RenderDrawLine(renderer,barX+px,barY,barX+px,barY+barH-1);
            }
            char pct[8];
            sprintf(pct,"%d%%",(int)(progress*100));
            renderCenterBold(renderer,fLarge,pct,cx,barY+11,(SDL_Color){255,255,230,255});

            renderCenterText(renderer,fNorm,"BSCPE 22002",cx,622,
                             (SDL_Color){255,230,160,255});

            /* Manji decorative accents on loading screen */
            SDL_Color manjiYellow = {255,230,80,255};
            renderBold(renderer,fHuge,"\xE5\x8D\x90", 30,  75, manjiYellow);
            renderBold(renderer,fHuge,"\xE5\x8D\x90", WIN_W - 80, 75, manjiYellow);
        }

        /* -------- SOLVER -------- */
        else if (screen==SCREEN_SOLVER) {
            /* soft pink base */
            SDL_SetRenderDrawColor(renderer,255,235,245,255);
            SDL_RenderClear(renderer);

            SDL_Color white   = {255,255,255,255};
            SDL_Color cream   = {255,245,200,255};
            SDL_Color blueCol = {200, 50, 110, 255};  /* repurposed as pink accent */
            SDL_Color redCol  = {160, 90,   0, 255};  /* deep golden for results */
            SDL_Color darkTxt = {100, 20,  60, 255};
            SDL_Color hintCol = {190,100, 140, 255};
            SDL_Color panelBg = {255,245, 250, 255};
            SDL_Color panBdr  = {230,140, 175, 255};
            SDL_Color eqBg    = {255,250, 230, 255};
            SDL_Color eqBdr   = {230,185,  80, 255};

            /* header gradient: deep pink -> hot pink */
            for (int i=0;i<84;i++) {
                float t=(float)i/84.0f;
                int rv=(int)(195+t*35), gv=(int)(20+t*30), bv=(int)(95+t*25);
                SDL_SetRenderDrawColor(renderer,rv,gv,bv,255);
                SDL_RenderDrawLine(renderer,0,i,WIN_W,i);
            }
            /* yellow strip */
            SDL_SetRenderDrawColor(renderer,255,215,10,255);
            SDL_Rect hstrip={0,84,WIN_W,6};
            SDL_RenderFillRect(renderer,&hstrip);

            renderBold(renderer,fTitle,"BISECTION METHOD",          30, 8, white);
            renderText(renderer,fLarge,"Quadratic Equation  |  Root Finding",30,44, cream);
            renderText(renderer,fSmall,"MT221 - Numerical Methods  |  Final Project",1090, 8, cream);
            renderText(renderer,fNorm, "BSCPE 22002",                         1200,28, white);
            renderText(renderer,fSmall,"Doncillo, Christian Aldrey B.  |  Gabutin, Dexter", 1090,48, cream);
            renderButton(renderer,fSmall,&btnBackSolver);

            /* ===== LEFT PANEL: INPUT ===== */
            drawPanel(renderer,14,100,495,823,panelBg,panBdr);

            /* panel title bar: hot pink */
            SDL_SetRenderDrawColor(renderer,220,45,120,255);
            SDL_Rect ptbar={14,100,495,32}; SDL_RenderFillRect(renderer,&ptbar);
            renderBold(renderer,fLarge,"INPUT",22,107,white);
            renderText(renderer,fNorm,"Coefficients + Interval",130,109,cream);

            /* function display */
            drawPanel(renderer,28,142,464,52,eqBg,eqBdr);
            renderText(renderer,fSmall,"Equation to solve:",40,146,hintCol);
            renderBold(renderer,fMed,"f(x) = Ax\xC2\xB2 + Bx + C = 0",40,163,(SDL_Color){160,90,0,255});

            /* algorithm reminder */
            drawPanel(renderer,28,202,464,68,eqBg,eqBdr);
            renderText(renderer,fSmall,"Method:",40,207,hintCol);
            renderBold(renderer,fNorm,"Bisection:  c = (a + b) / 2",40,223,blueCol);
            renderText(renderer,fSmall,"  If f(a)*f(c) < 0  ->  new b = c",40,241,hintCol);
            renderText(renderer,fSmall,"  Else               ->  new a = c",40,257,hintCol);

            /* coefficients */
            drawPanel(renderer,28,278,464,102,eqBg,eqBdr);
            /* colored header: golden yellow */
            SDL_SetRenderDrawColor(renderer,230,155,10,255);
            SDL_Rect chdr={28,278,464,22}; SDL_RenderFillRect(renderer,&chdr);
            renderBold(renderer,fNorm,"STEP 1 — Enter polynomial coefficients",38,282,white);
            renderText(renderer,fSmall,"for  f(x) = Ax\xC2\xB2 + Bx + C",195,300,hintCol);
            inputs[0].rect=(SDL_Rect){42, 330,90,36};
            inputs[1].rect=(SDL_Rect){172,330,90,36};
            inputs[2].rect=(SDL_Rect){302,330,90,36};
            for (int i=0;i<3;i++) renderInputBox(renderer,fSmall,fNorm,&inputs[i]);

            /* interval + settings */
            drawPanel(renderer,28,388,464,110,eqBg,eqBdr);
            SDL_SetRenderDrawColor(renderer,230,155,10,255);
            SDL_Rect ihdr={28,388,464,22}; SDL_RenderFillRect(renderer,&ihdr);
            renderBold(renderer,fNorm,"STEP 2 — Enter interval & tolerance",38,392,white);
            renderText(renderer,fSmall,"f(a) and f(b) must have OPPOSITE signs",44,418,
                       (SDL_Color){190,100,10,255});
            inputs[3].rect=(SDL_Rect){42, 450,85,36};
            inputs[4].rect=(SDL_Rect){155,450,85,36};
            inputs[5].rect=(SDL_Rect){268,450,110,36};
            inputs[6].rect=(SDL_Rect){406,450,70,36};
            for (int i=3;i<7;i++) renderInputBox(renderer,fSmall,fNorm,&inputs[i]);

            /* equation preview */
            drawPanel(renderer,28,508,464,28,eqBg,eqBdr);
            {
                double pA=atof(inputs[0].value),pB=atof(inputs[1].value),pC=atof(inputs[2].value);
                char prev[128];
                sprintf(prev,"Preview:  f(x) = %.3gx\xC2\xB2 %+.3gx %+.3g",pA,pB,pC);
                renderBold(renderer,fSmall,prev,40,514,(SDL_Color){160,90,0,255});
            }

            /* sign check hint */
            if (!hasSol && !hasError) {
                double pA=atof(inputs[0].value),pB=atof(inputs[1].value),pC=atof(inputs[2].value);
                double pa=atof(inputs[3].value),pb2=atof(inputs[4].value);
                if (strlen(inputs[3].value)>0 && strlen(inputs[4].value)>0) {
                    double fa2=evalQuad(pa,pA,pB,pC), fb2=evalQuad(pb2,pA,pB,pC);
                    char hint[80];
                    sprintf(hint,"f(a)=%.4g  f(b)=%.4g  -> %s",
                        fa2,fb2,(fa2*fb2<0)?"Opposite signs OK":"Same sign - change bounds!");
                    SDL_Color hc = (fa2*fb2<0)?(SDL_Color){18,140,50,255}:(SDL_Color){190,80,10,255};
                    drawPanel(renderer,28,544,464,26,eqBg,eqBdr);
                    renderText(renderer,fSmall,hint,40,549,hc);
                }
            }

            renderButton(renderer,fNorm,&btnCompute);
            renderButton(renderer,fNorm,&btnClear);

            /* error */
            if (hasError && strlen(errorMsg)>0) {
                drawPanel(renderer,28,638,464,50,
                    (SDL_Color){255,235,210,255},(SDL_Color){230,130,30,255});
                renderBold(renderer,fSmall,"ERROR:",40,642,(SDL_Color){180,80,10,255});
                renderText(renderer,fSmall,errorMsg,40,659,(SDL_Color){160,70,10,255});
            }

            /* result panel */
            if (hasSol) {
                drawPanel(renderer,28,698,464,108,
                    (SDL_Color){255,240,210,255},(SDL_Color){230,155,10,255});

                SDL_SetRenderDrawColor(renderer,220,45,120,255);
                SDL_Rect rbdr={28,698,464,28}; SDL_RenderFillRect(renderer,&rbdr);
                renderBold(renderer,fLarge,"RESULT",185,703,white);

                char buf[128];
                sprintf(buf,"Root  ~  %.8f",sRoot);
                renderBold(renderer,fLarge,buf,44,732,redCol);
                sprintf(buf,"f(root) = %.3e",evalQuad(sRoot,sA,sB,sC));
                renderText(renderer,fNorm,buf,44,758,hintCol);
                sprintf(buf,"Converged in %d iteration(s)",sIterCount);
                renderText(renderer,fSmall,buf,44,776,(SDL_Color){18,120,50,255});
                renderText(renderer,fSmall,"(~ means approximately equal to)",44,791,hintCol);
            }

            /* ===== MIDDLE PANEL: STEPS ===== */
            drawPanel(renderer,520,100,555,823,panelBg,panBdr);

            SDL_SetRenderDrawColor(renderer,220,45,120,255);
            SDL_Rect stbar={520,100,555,32}; SDL_RenderFillRect(renderer,&stbar);
            renderBold(renderer,fLarge,"ITERATION TABLE",570,107,white);

            if (hasSol) {
                int sy=140;
                char sbuf[256];

                /* GIVEN */
                drawPanel(renderer,534,sy,526,58,eqBg,eqBdr);
                renderBold(renderer,fMed,"GIVEN",548,sy+4,(SDL_Color){160,90,0,255});
                SDL_SetRenderDrawColor(renderer,eqBdr.r,eqBdr.g,eqBdr.b,255);
                SDL_RenderDrawLine(renderer,548,sy+22,1050,sy+22);
                sprintf(sbuf,"f(x) = %.3gx\xC2\xB2 %+.3gx %+.3g    on [%.4g, %.4g]",
                        sA,sB,sC,sa,sb);
                renderText(renderer,fSmall,sbuf,548,sy+26,darkTxt);
                sprintf(sbuf,"Tolerance = %.2e     Max Iterations = %d",sTol,sMaxIter);
                renderText(renderer,fSmall,sbuf,548,sy+41,darkTxt);
                sy+=62;

                /* ALGORITHM STEPS */
                drawPanel(renderer,534,sy,526,90,eqBg,eqBdr);
                renderBold(renderer,fMed,"ALGORITHM",548,sy+4,blueCol);
                SDL_SetRenderDrawColor(renderer,eqBdr.r,eqBdr.g,eqBdr.b,255);
                SDL_RenderDrawLine(renderer,548,sy+22,1050,sy+22);
                renderText(renderer,fSmall,"1. Verify: f(a) x f(b) < 0  (root guaranteed)",548,sy+26,darkTxt);
                renderText(renderer,fSmall,"2. Midpoint:  c = (a + b) / 2",548,sy+42,darkTxt);
                renderText(renderer,fSmall,"3. If f(a) x f(c) < 0:  b = c   (root in left half)",548,sy+58,darkTxt);
                renderText(renderer,fSmall,"   Else:                 a = c   (root in right half)",548,sy+74,darkTxt);
                sy+=94;

                /* TABLE */
                int rowH=18;
                int tableH=iterCount*rowH+50;
                int maxTableH=310;
                int dispH=tableH<maxTableH?tableH:maxTableH;

                drawPanel(renderer,534,sy,526,dispH,(SDL_Color){255,248,230,255},eqBdr);
                renderBold(renderer,fMed,"STEP-BY-STEP",548,sy+4,blueCol);
                SDL_SetRenderDrawColor(renderer,eqBdr.r,eqBdr.g,eqBdr.b,255);
                SDL_RenderDrawLine(renderer,548,sy+22,1050,sy+22);

                /* column header row */
                SDL_SetRenderDrawColor(renderer,220,45,120,255);
                SDL_Rect hdrBg={535,sy+24,524,17}; SDL_RenderFillRect(renderer,&hdrBg);
                renderBold(renderer,fSmall,"n",   546,sy+25,white);
                renderBold(renderer,fSmall,"a",   574,sy+25,white);
                renderBold(renderer,fSmall,"b",   656,sy+25,white);
                renderBold(renderer,fSmall,"c=(a+b)/2",728,sy+25,white);
                renderBold(renderer,fSmall,"f(c)",880,sy+25,white);
                renderBold(renderer,fSmall,"Next",976,sy+25,white);

                SDL_Rect clipR={534,sy+41,526,dispH-47};
                SDL_RenderSetClipRect(renderer,&clipR);

                for (int i=0;i<iterCount;i++) {
                    int ry=sy+43+i*rowH-scrollY;
                    if (ry<sy+36 || ry>sy+dispH) continue;

                    /* row bg: alternating pink tones */
                    SDL_SetRenderDrawColor(renderer,
                        255, i%2==0?238:248, i%2==0?225:238, 255);
                    SDL_Rect rowBg={535,ry-1,524,rowH-1};
                    SDL_RenderFillRect(renderer,&rowBg);

                    /* last row highlight */
                    if (i==iterCount-1) {
                        SDL_SetRenderDrawColor(renderer,255,235,170,255);
                        SDL_RenderFillRect(renderer,&rowBg);
                    }

                    sprintf(sbuf,"%d",iters[i].step);
                    renderText(renderer,fSmall,sbuf,546,ry,darkTxt);
                    sprintf(sbuf,"%.5f",iters[i].a);
                    renderText(renderer,fSmall,sbuf,566,ry,darkTxt);
                    sprintf(sbuf,"%.5f",iters[i].b);
                    renderText(renderer,fSmall,sbuf,648,ry,darkTxt);
                    sprintf(sbuf,"%.5f",iters[i].c);
                    renderBold(renderer,fSmall,sbuf,722,ry,blueCol);
                    sprintf(sbuf,"%.2e",iters[i].fc);
                    renderText(renderer,fSmall,sbuf,854,ry,
                        fabs(iters[i].fc)<sTol
                            ?(SDL_Color){18,140,35,255}
                            :(SDL_Color){190,90,10,255});

                    if (i<iterCount-1) {
                        int goLeft=(iters[i].fa*iters[i].fc<0);
                        renderText(renderer,fSmall,goLeft?"[a,c]":"[c,b]",970,ry,
                                   (SDL_Color){200,50,110,255});
                    } else {
                        renderBold(renderer,fSmall,"ROOT",970,ry,
                                   (SDL_Color){18,130,35,255});
                    }
                }
                SDL_RenderSetClipRect(renderer,NULL);

                /* scrollbar */
                if (tableH>maxTableH) {
                    int maxScroll=tableH-maxTableH+47;
                    if (scrollY>maxScroll) scrollY=maxScroll;
                    int sbH=(int)((float)maxTableH/tableH*(dispH-47));
                    if (sbH<20) sbH=20;
                    int sbY=sy+41+(int)((float)scrollY/maxScroll*(dispH-47-sbH));
                    SDL_SetRenderDrawColor(renderer,80,110,210,200);
                    SDL_Rect scrollBar={1054,sbY,6,sbH};
                    SDL_RenderFillRect(renderer,&scrollBar);
                }
                sy+=dispH+6;

                /* convergence status */
                double lastFC=iters[iterCount-1].fc;
                int conv=(fabs(lastFC)<sTol ||
                         (iters[iterCount-1].b-iters[iterCount-1].a)/2.0<sTol);

                drawPanel(renderer,534,sy,526,56,
                    conv?(SDL_Color){228,255,218,255}:(SDL_Color){255,240,210,255},
                    conv?(SDL_Color){50,160,70,255}:(SDL_Color){230,155,10,255});
                renderBold(renderer,fMed,
                    conv?"CONVERGED - ROOT FOUND":"MAX ITERATIONS REACHED",
                    548,sy+5,
                    conv?(SDL_Color){18,130,38,255}:(SDL_Color){180,100,10,255});
                SDL_SetRenderDrawColor(renderer,
                    conv?50:230, conv?160:155, conv?70:10, 255);
                SDL_RenderDrawLine(renderer,548,sy+23,1050,sy+23);
                sprintf(sbuf,"Root ~ %.8f     f(root) = %.3e     n = %d",
                        sRoot,evalQuad(sRoot,sA,sB,sC),iterCount);
                renderBold(renderer,fNorm,sbuf,548,sy+27,darkTxt);
                sy+=62;

            } else {
                /* placeholder */
                int py=420;
                renderCenterText(renderer,fNorm,
                    "Enter values and press  COMPUTE",
                    520+555/2,py,hintCol);
                renderCenterText(renderer,fNorm,
                    "to see the iteration table.",
                    520+555/2,py+24,hintCol);
                SDL_SetRenderDrawColor(renderer,180,190,230,200);
                SDL_RenderDrawLine(renderer,560,py+54,1050,py+54);
                renderCenterText(renderer,fNorm,
                    "TIP: f(a) and f(b) must have",
                    520+555/2,py+65,redCol);
                renderCenterText(renderer,fNorm,
                    "OPPOSITE signs.",
                    520+555/2,py+89,redCol);
                renderCenterText(renderer,fSmall,
                    "Example: A=1, B=-3, C=-4, a=-2, b=0",
                    520+555/2,py+120,hintCol);
                renderCenterText(renderer,fSmall,
                    "gives f(-2)=6 > 0  and  f(0)=-4 < 0",
                    520+555/2,py+138,hintCol);
            }

            /* ===== RIGHT PANEL: GRAPH ===== */
            drawPanel(renderer,1085,100,500,823,panelBg,panBdr);

            SDL_SetRenderDrawColor(renderer,220,45,120,255);
            SDL_Rect gpbar={1085,100,500,32}; SDL_RenderFillRect(renderer,&gpbar);
            renderBold(renderer,fLarge,"GRAPH",1285,107,white);

            drawGraph(renderer,fSmall,sA,sB,sC,sa,sb,sRoot,hasSol);

            /* legend */
            int LY=630;
            drawPanel(renderer,1098,LY,462,105,eqBg,eqBdr);
            SDL_SetRenderDrawColor(renderer,230,155,10,255);
            SDL_Rect lhdr={1098,LY,462,22}; SDL_RenderFillRect(renderer,&lhdr);
            renderBold(renderer,fNorm,"GRAPH LEGEND",1275,LY+5,white);

            SDL_SetRenderDrawColor(renderer,190,20,20,255);
            SDL_Rect l1={1115,LY+36,28,3}; SDL_RenderFillRect(renderer,&l1);
            renderText(renderer,fNorm,"f(x) curve (red)",1153,LY+29,(SDL_Color){180,20,20,255});

            for (int di=-4;di<=4;di++) for (int dj=-4;dj<=4;dj++)
                if (di*di+dj*dj<=16) {
                    SDL_SetRenderDrawColor(renderer,220,45,120,255);
                    SDL_RenderDrawPoint(renderer,1127+di,LY+58+dj);
                }
            renderText(renderer,fNorm,"Approximate root (pink dot)",1153,LY+51,(SDL_Color){200,40,110,255});

            SDL_SetRenderDrawColor(renderer,200,50,50,90);
            SDL_Rect l3={1113,LY+76,28,14}; SDL_RenderFillRect(renderer,&l3);
            renderText(renderer,fNorm,"Shaded interval [a, b]",1153,LY+74,(SDL_Color){175,35,35,255});
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    TTF_CloseFont(fHuge);  TTF_CloseFont(fBig);
    TTF_CloseFont(fTitle); TTF_CloseFont(fLarge); TTF_CloseFont(fMed);
    TTF_CloseFont(fNorm);  TTF_CloseFont(fSmall);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
