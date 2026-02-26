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
} Button;

static double evalPoly(double x, double A, double B, double C, double D) {
    return A*x*x*x + B*x*x + C*x + D;
}

static double exactIntegral(double a, double b,
                            double A, double B, double C, double D) {
    double Fb = (A/4.0)*b*b*b*b + (B/3.0)*b*b*b + (C/2.0)*b*b + D*b;
    double Fa = (A/4.0)*a*a*a*a + (B/3.0)*a*a*a + (C/2.0)*a*a + D*a;
    return Fb - Fa;
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
    renderText(r, f, t, cx - textW(f, t) / 2, y, c);
}

static void renderCenterBold(SDL_Renderer* r, TTF_Font* f, const char* t,
                             int cx, int y, SDL_Color c) {
    renderBold(r, f, t, cx - textW(f, t) / 2, y, c);
}

static int renderWithSup(SDL_Renderer* r, TTF_Font* fN, TTF_Font* fSp,
                         const char* base, const char* sup,
                         int x, int y, SDL_Color c) {
    renderBold(r, fN, base, x, y, c);
    x += textW(fN, base);
    renderText(r, fSp, sup, x, y - 6, c);
    x += textW(fSp, sup) + 1;
    return x;
}

static void drawPanel(SDL_Renderer* r, int x, int y, int w, int h,
                      SDL_Color bg, SDL_Color border) {
    SDL_SetRenderDrawColor(r, bg.r, bg.g, bg.b, bg.a);
    SDL_Rect rc = {x, y, w, h}; SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(r, &rc);
}

static void renderInputBox(SDL_Renderer* rr, TTF_Font* f, InputBox* box) {
    SDL_Color lc = {130, 60, 90, 255};
    renderBold(rr, f, box->label, box->rect.x, box->rect.y - 20, lc);
    SDL_SetRenderDrawColor(rr, box->active ? 255 : 255,
                                box->active ? 228 : 245,
                                box->active ? 240 : 248, 255);
    SDL_RenderFillRect(rr, &box->rect);
    SDL_SetRenderDrawColor(rr, box->active ? 200 : 190,
                                box->active ? 80  : 140,
                                box->active ? 130 : 160, 255);
    SDL_RenderDrawRect(rr, &box->rect);
    if (box->active) {
        SDL_Rect inn = {box->rect.x+1, box->rect.y+1, box->rect.w-2, box->rect.h-2};
        SDL_RenderDrawRect(rr, &inn);
    }
    if (strlen(box->value) > 0)
        renderText(rr, f, box->value,
                   box->rect.x + 8, box->rect.y + 7,
                   (SDL_Color){80, 20, 60, 255});
}

static void renderButton(SDL_Renderer* rr, TTF_Font* f, Button* btn) {
    SDL_Color bg = btn->clicked  ? (SDL_Color){150,  40,  80, 255}
                 : btn->hovered  ? (SDL_Color){210, 100, 140, 255}
                                 : (SDL_Color){190,  70, 120, 255};
    SDL_SetRenderDrawColor(rr, 100, 30, 60, 255);
    SDL_Rect sh = {btn->rect.x+3, btn->rect.y+3, btn->rect.w, btn->rect.h};
    SDL_RenderFillRect(rr, &sh);
    SDL_SetRenderDrawColor(rr, bg.r, bg.g, bg.b, bg.a);
    SDL_RenderFillRect(rr, &btn->rect);
    SDL_SetRenderDrawColor(rr, 140, 50, 80, 255);
    SDL_RenderDrawRect(rr, &btn->rect);
    SDL_Surface* s = TTF_RenderText_Blended(f, btn->text, (SDL_Color){255,255,255,255});
    if (s) {
        SDL_Texture* tx = SDL_CreateTextureFromSurface(rr, s);
        SDL_Rect tr = {btn->rect.x+(btn->rect.w-s->w)/2,
                       btn->rect.y+(btn->rect.h-s->h)/2, s->w, s->h};
        SDL_RenderCopy(rr, tx, NULL, &tr);
        SDL_FreeSurface(s); SDL_DestroyTexture(tx);
    }
}

static void drawFlower(SDL_Renderer* rr, int cx, int cy, int r, SDL_Color petal, SDL_Color center) {
    int offsets[5][2] = {{0,-r},{(int)(r*0.95),(int)(r*0.31)},{(int)(r*0.59),(int)(-r*0.81)},
                         {(int)(-r*0.59),(int)(-r*0.81)},{(int)(-r*0.95),(int)(r*0.31)}};
    int pr = (int)(r * 0.7);
    for (int p = 0; p < 5; p++) {
        int px = cx + offsets[p][0];
        int py = cy + offsets[p][1];
        for (int di = -pr; di <= pr; di++)
            for (int dj = -pr; dj <= pr; dj++)
                if (di*di + dj*dj <= pr*pr) {
                    SDL_SetRenderDrawColor(rr, petal.r, petal.g, petal.b, petal.a);
                    SDL_RenderDrawPoint(rr, px+di, py+dj);
                }
    }
    int cr = (int)(r * 0.45);
    for (int di = -cr; di <= cr; di++)
        for (int dj = -cr; dj <= cr; dj++)
            if (di*di + dj*dj <= cr*cr) {
                SDL_SetRenderDrawColor(rr, center.r, center.g, center.b, center.a);
                SDL_RenderDrawPoint(rr, cx+di, cy+dj);
            }
}

static void drawSmallFlower(SDL_Renderer* rr, int cx, int cy, int r, SDL_Color petal, SDL_Color center) {
    int offsets[6][2];
    for (int i = 0; i < 6; i++) {
        double a = i * 3.14159265 / 3.0;
        offsets[i][0] = (int)(r * cos(a));
        offsets[i][1] = (int)(r * sin(a));
    }
    int pr = (int)(r * 0.55);
    for (int p = 0; p < 6; p++) {
        int px = cx + offsets[p][0];
        int py = cy + offsets[p][1];
        for (int di = -pr; di <= pr; di++)
            for (int dj = -pr; dj <= pr; dj++)
                if (di*di + dj*dj <= pr*pr) {
                    SDL_SetRenderDrawColor(rr, petal.r, petal.g, petal.b, petal.a);
                    SDL_RenderDrawPoint(rr, px+di, py+dj);
                }
    }
    int cr = (int)(r * 0.35);
    for (int di = -cr; di <= cr; di++)
        for (int dj = -cr; dj <= cr; dj++)
            if (di*di + dj*dj <= cr*cr) {
                SDL_SetRenderDrawColor(rr, center.r, center.g, center.b, center.a);
                SDL_RenderDrawPoint(rr, cx+di, cy+dj);
            }
}

static void drawGraph(SDL_Renderer* rr, TTF_Font* fSmall,
                      double A, double B, double C, double D,
                      double a, double b, int n, int hasSol) {
    int GX = 1100, GY = 195, GW = 470, GH = 415;

    double xMin = a - 1.0, xMax = b + 1.0;
    double span = xMax - xMin;
    if (span < 2.0) { span = 2.0; xMin = (a+b)/2.0 - 1.0; xMax = (a+b)/2.0 + 1.0; }

    double yMinV = 1e18, yMaxV = -1e18;
    for (double xx = xMin; xx <= xMax; xx += span/200.0) {
        double yy = evalPoly(xx, A, B, C, D);
        if (yy < yMinV) yMinV = yy;
        if (yy > yMaxV) yMaxV = yy;
    }
    double ySpan = yMaxV - yMinV;
    if (ySpan < 1.0) ySpan = 2.0;
    double yMin = yMinV - ySpan * 0.15;
    double yMax = yMaxV + ySpan * 0.15;

    drawPanel(rr, GX, GY, GW, GH,
              (SDL_Color){255, 245, 250, 255}, (SDL_Color){210, 140, 170, 255});

    SDL_SetRenderDrawColor(rr, 250, 230, 240, 255);
    for (int i = 0; i <= 10; i++) {
        int px = GX + i*GW/10;
        int py = GY + i*GH/10;
        SDL_RenderDrawLine(rr, px, GY, px, GY+GH);
        SDL_RenderDrawLine(rr, GX, py, GX+GW, py);
    }

    SDL_SetRenderDrawColor(rr, 140, 60, 100, 255);
    int ox = GX + (int)((-xMin)/(xMax-xMin)*GW);
    int oy = GY + GH - (int)((-yMin)/(yMax-yMin)*GH);
    if (ox >= GX && ox <= GX+GW) SDL_RenderDrawLine(rr, ox, GY, ox, GY+GH);
    if (oy >= GY && oy <= GY+GH) SDL_RenderDrawLine(rr, GX, oy, GX+GW, oy);
    renderText(rr, fSmall, "x", GX+GW-12, oy+4, (SDL_Color){140,60,100,255});
    renderText(rr, fSmall, "f(x)", ox+4,  GY+4, (SDL_Color){140,60,100,255});

    if (hasSol) {
        SDL_SetRenderDrawBlendMode(rr, SDL_BLENDMODE_BLEND);
        for (int px = 0; px < GW; px++) {
            double xv = xMin + (double)px / GW * (xMax - xMin);
            if (xv < a || xv > b) continue;
            double yv = evalPoly(xv, A, B, C, D);
            int sy = GY + GH - (int)((yv - yMin)/(yMax-yMin)*GH);
            if (sy < GY) sy = GY;
            if (sy > GY+GH) sy = GY+GH;
            SDL_SetRenderDrawColor(rr, 220, 110, 160, 60);
            if (sy < oy) {
                SDL_RenderDrawLine(rr, GX+px, sy, GX+px, oy);
            } else {
                SDL_RenderDrawLine(rr, GX+px, oy, GX+px, sy);
            }
        }
        SDL_SetRenderDrawBlendMode(rr, SDL_BLENDMODE_NONE);
    }

    int prevPx = -1, prevPy = -1;
    for (int px = 0; px < GW; px++) {
        double xv = xMin + (double)px / GW * (xMax - xMin);
        double yv = evalPoly(xv, A, B, C, D);
        int sy = GY + GH - (int)((yv - yMin)/(yMax-yMin)*GH);
        if (sy >= GY && sy <= GY+GH) {
            SDL_SetRenderDrawColor(rr, 180, 50, 100, 255);
            if (prevPx >= 0 && abs(sy - prevPy) < GH/2) {
                SDL_RenderDrawLine(rr, GX+prevPx, prevPy, GX+px, sy);
                SDL_RenderDrawLine(rr, GX+prevPx, prevPy+1, GX+px, sy+1);
            }
            prevPx = px; prevPy = sy;
        }
    }

    if (hasSol) {
        double h = (b - a) / n;
        for (int i = 0; i <= n; i++) {
            double xi = a + i * h;
            double yi = evalPoly(xi, A, B, C, D);
            int npx = GX + (int)((xi - xMin)/(xMax-xMin)*GW);
            int npy = GY + GH - (int)((yi - yMin)/(yMax-yMin)*GH);
            if (npx>=GX && npx<=GX+GW && npy>=GY && npy<=GY+GH) {
                for (int di=-4;di<=4;di++) for (int dj=-4;dj<=4;dj++)
                    if (di*di+dj*dj<=16) {
                        SDL_SetRenderDrawColor(rr, 255, 140, 0, 255);
                        SDL_RenderDrawPoint(rr, npx+di, npy+dj);
                    }
            }
        }

        int apx = GX + (int)((a - xMin)/(xMax-xMin)*GW);
        int bpx = GX + (int)((b - xMin)/(xMax-xMin)*GW);
        SDL_SetRenderDrawColor(rr, 160, 80, 120, 180);
        for (int yy = GY; yy <= GY+GH; yy += 4)
            SDL_RenderDrawPoint(rr, apx, yy);
        for (int yy = GY; yy <= GY+GH; yy += 4)
            SDL_RenderDrawPoint(rr, bpx, yy);
        char lb[16];
        sprintf(lb, "a=%.1f", a);
        renderText(rr, fSmall, lb, apx-10, GY+GH+4, (SDL_Color){160,80,120,255});
        sprintf(lb, "b=%.1f", b);
        renderText(rr, fSmall, lb, bpx-10, GY+GH+4, (SDL_Color){160,80,120,255});
    }

    char rb[16];
    sprintf(rb,"%.1f",xMin); renderText(rr,fSmall,rb,GX+2,oy+5,(SDL_Color){140,60,100,180});
    sprintf(rb,"%.1f",xMax); renderText(rr,fSmall,rb,GX+GW-28,oy+5,(SDL_Color){140,60,100,180});
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window* window = SDL_CreateWindow(
        "Simpson's 1/3 Rule - 3rd Degree Polynomial",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(renderer, WIN_W, WIN_H);

    TTF_Font* fHuge  = TTF_OpenFont("font.ttf", 36);
    TTF_Font* fBig   = TTF_OpenFont("font.ttf", 28);
    TTF_Font* fTitle = TTF_OpenFont("font.ttf", 27);
    TTF_Font* fLarge = TTF_OpenFont("font.ttf", 20);
    TTF_Font* fMed   = TTF_OpenFont("font.ttf", 17);
    TTF_Font* fNorm  = TTF_OpenFont("font.ttf", 15);
    TTF_Font* fSmall = TTF_OpenFont("font.ttf", 13);
    TTF_Font* fSup   = TTF_OpenFont("font.ttf", 11);
    TTF_Font* fSub   = TTF_OpenFont("font.ttf", 11);

    if (!fHuge||!fBig||!fTitle||!fLarge||!fMed||!fNorm||!fSmall||!fSup||!fSub) {
        printf("Font error: %s\n", TTF_GetError()); return 1;
    }

    Screen screen = SCREEN_LANDING;
    Uint32 loadStart = 0;
    int    hoverSolver = 0, hoverAbout = 0;

    InputBox inputs[7];
    const char* labels[7]   = {"A","B","C","D","a (lower)","b (upper)","n (even)"};
    const char* defaults[7] = {"1","0","0","0","0","4","4"};
    for (int i = 0; i < 7; i++) {
        strcpy(inputs[i].label, labels[i]);
        strcpy(inputs[i].value, defaults[i]);
        inputs[i].active = 0;
        inputs[i].rect = (SDL_Rect){0,0,80,36};
    }

    Button btnCompute = {{55, 530, 175, 46}, "COMPUTE", 0, 0};
    Button btnClear   = {{255,530, 175, 46}, "CLEAR",   0, 0};
    Button btnBack    = {{30, 30, 160, 50},  "< BACK",  0, 0};
    Button btnBackSolver = {{1390, 88, 120, 36}, "< MENU", 0, 0};

    int    hasSol = 0, hasError = 0, activeInput = -1, quit = 0;
    double simpsonResult = 0, exactResult = 0;
    double sA=0, sB=0, sC=0, sD=0, sa=0, sb=0;
    int    sn=4;
    char   errorMsg[200] = "";

    double nodeX[201], nodeF[201];
    int    nodeCount = 0;
    double sumEndpoints = 0, sumOdd = 0, sumEven = 0;
    double h_val = 0;
    int    scrollY = 0;

    SDL_StartTextInput();
    SDL_Event ev;

    while (!quit) {
        SDL_Rect cardSolver = {300, 320, 400, 280};
        SDL_Rect cardAbout  = {900, 320, 400, 280};

        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) quit = 1;

            if (screen == SCREEN_LANDING) {
                if (ev.type == SDL_MOUSEBUTTONDOWN) {
                    int mx = ev.button.x, my = ev.button.y;
                    if (mx>=cardSolver.x && mx<cardSolver.x+cardSolver.w &&
                        my>=cardSolver.y && my<cardSolver.y+cardSolver.h) {
                        screen = SCREEN_LOADING;
                        loadStart = SDL_GetTicks();
                    }
                    if (mx>=cardAbout.x && mx<cardAbout.x+cardAbout.w &&
                        my>=cardAbout.y && my<cardAbout.y+cardAbout.h) {
                        screen = SCREEN_ABOUT;
                    }
                }
                if (ev.type == SDL_MOUSEMOTION) {
                    int mx = ev.motion.x, my = ev.motion.y;
                    hoverSolver = (mx>=cardSolver.x && mx<cardSolver.x+cardSolver.w &&
                                   my>=cardSolver.y && my<cardSolver.y+cardSolver.h);
                    hoverAbout  = (mx>=cardAbout.x  && mx<cardAbout.x+cardAbout.w &&
                                   my>=cardAbout.y  && my<cardAbout.y+cardAbout.h);
                }
            }

            else if (screen == SCREEN_ABOUT) {
                if (ev.type == SDL_MOUSEBUTTONDOWN) {
                    int mx = ev.button.x, my = ev.button.y;
                    if (mx>=btnBack.rect.x && mx<btnBack.rect.x+btnBack.rect.w &&
                        my>=btnBack.rect.y && my<btnBack.rect.y+btnBack.rect.h) {
                        screen = SCREEN_LANDING;
                    }
                }
                if (ev.type == SDL_MOUSEMOTION) {
                    int mx = ev.motion.x, my = ev.motion.y;
                    btnBack.hovered = (mx>=btnBack.rect.x && mx<btnBack.rect.x+btnBack.rect.w &&
                                       my>=btnBack.rect.y && my<btnBack.rect.y+btnBack.rect.h);
                }
            }

            else if (screen == SCREEN_SOLVER) {
                if (ev.type == SDL_MOUSEWHEEL) {
                    scrollY -= ev.wheel.y * 20;
                    if (scrollY < 0) scrollY = 0;
                }

                if (ev.type == SDL_MOUSEBUTTONDOWN) {
                    int mx = ev.button.x, my = ev.button.y;
                    activeInput = -1;
                    for (int i = 0; i < 7; i++) {
                        inputs[i].active = 0;
                        if (mx >= inputs[i].rect.x && mx < inputs[i].rect.x+inputs[i].rect.w &&
                            my >= inputs[i].rect.y && my < inputs[i].rect.y+inputs[i].rect.h) {
                            activeInput = i;
                            inputs[i].active = 1;
                        }
                    }

                    if (mx >= btnBackSolver.rect.x && mx < btnBackSolver.rect.x+btnBackSolver.rect.w &&
                        my >= btnBackSolver.rect.y && my < btnBackSolver.rect.y+btnBackSolver.rect.h) {
                        screen = SCREEN_LANDING;
                        hasSol = 0; hasError = 0;
                        scrollY = 0;
                    }

                    if (mx >= btnCompute.rect.x && mx < btnCompute.rect.x+btnCompute.rect.w &&
                        my >= btnCompute.rect.y && my < btnCompute.rect.y+btnCompute.rect.h) {
                        btnCompute.clicked = 1;
                        hasSol = 0; hasError = 0;
                        scrollY = 0;
                        strcpy(errorMsg, "");

                        sA = atof(inputs[0].value);
                        sB = atof(inputs[1].value);
                        sC = atof(inputs[2].value);
                        sD = atof(inputs[3].value);
                        sa = atof(inputs[4].value);
                        sb = atof(inputs[5].value);
                        sn = atoi(inputs[6].value);

                        if (sn <= 0) {
                            hasError = 1;
                            strcpy(errorMsg, "n must be a positive integer.");
                        } else if (sn % 2 != 0) {
                            hasError = 1;
                            strcpy(errorMsg, "n must be EVEN for Simpson's 1/3 Rule.");
                        } else if (fabs(sa - sb) < 1e-14) {
                            hasError = 1;
                            strcpy(errorMsg, "a and b cannot be equal.");
                        } else if (sa > sb) {
                            hasError = 1;
                            strcpy(errorMsg, "Lower bound a must be less than upper bound b.");
                        } else if (sn > 200) {
                            hasError = 1;
                            strcpy(errorMsg, "n too large (max 200).");
                        } else {
                            h_val = (sb - sa) / (double)sn;
                            nodeCount = sn + 1;
                            sumEndpoints = 0; sumOdd = 0; sumEven = 0;

                            for (int i = 0; i <= sn; i++) {
                                nodeX[i] = sa + i * h_val;
                                nodeF[i] = evalPoly(nodeX[i], sA, sB, sC, sD);
                            }

                            sumEndpoints = nodeF[0] + nodeF[sn];
                            for (int i = 1; i < sn; i++) {
                                if (i % 2 == 1) sumOdd += nodeF[i];
                                else             sumEven += nodeF[i];
                            }

                            simpsonResult = (h_val / 3.0) * (sumEndpoints + 4.0*sumOdd + 2.0*sumEven);
                            exactResult = exactIntegral(sa, sb, sA, sB, sC, sD);
                            hasSol = 1;
                        }
                    }

                    if (mx >= btnClear.rect.x && mx < btnClear.rect.x+btnClear.rect.w &&
                        my >= btnClear.rect.y && my < btnClear.rect.y+btnClear.rect.h) {
                        btnClear.clicked = 1;
                        for (int i = 0; i < 7; i++) strcpy(inputs[i].value, "");
                        hasSol = 0; hasError = 0; scrollY = 0;
                        strcpy(errorMsg, "");
                    }
                }

                if (ev.type == SDL_MOUSEBUTTONUP) {
                    btnCompute.clicked = 0;
                    btnClear.clicked   = 0;
                }

                if (ev.type == SDL_MOUSEMOTION) {
                    int mx = ev.motion.x, my = ev.motion.y;
#define HOVER(b) ((b).hovered = (mx>=(b).rect.x && mx<(b).rect.x+(b).rect.w && \
                                  my>=(b).rect.y && my<(b).rect.y+(b).rect.h))
                    HOVER(btnCompute); HOVER(btnClear); HOVER(btnBackSolver);
                }

                if (ev.type == SDL_TEXTINPUT && activeInput >= 0) {
                    char c = ev.text.text[0];
                    int isN = (activeInput == 6);
                    int valid = (c>='0'&&c<='9') || (!isN && c=='.') || (c=='-');
                    if (valid) {
                        int len = strlen(inputs[activeInput].value);
                        if (len < 18) {
                            inputs[activeInput].value[len]   = c;
                            inputs[activeInput].value[len+1] = '\0';
                        }
                    }
                }

                if (ev.type == SDL_KEYDOWN && activeInput >= 0) {
                    if (ev.key.keysym.sym == SDLK_BACKSPACE) {
                        int len = strlen(inputs[activeInput].value);
                        if (len > 0) inputs[activeInput].value[len-1] = '\0';
                    }
                    if (ev.key.keysym.sym == SDLK_TAB) {
                        inputs[activeInput].active = 0;
                        activeInput = (activeInput+1) % 7;
                        inputs[activeInput].active = 1;
                    }
                }
            }
        }

        if (screen == SCREEN_LOADING) {
            Uint32 elapsed = SDL_GetTicks() - loadStart;
            if (elapsed >= 5000) screen = SCREEN_SOLVER;
        }

        /* ---- LANDING PAGE ---- */
        if (screen == SCREEN_LANDING) {
            SDL_SetRenderDrawColor(renderer, 255, 245, 250, 255);
            SDL_RenderClear(renderer);

            drawFlower(renderer, 80,  80,  18, (SDL_Color){255,180,200,180}, (SDL_Color){255,220,100,255});
            drawFlower(renderer, 1520, 80,  18, (SDL_Color){240,160,200,180}, (SDL_Color){255,210,100,255});
            drawFlower(renderer, 160, 850, 15, (SDL_Color){255,170,210,180}, (SDL_Color){255,230,120,255});
            drawFlower(renderer, 1440, 850, 15, (SDL_Color){240,150,190,180}, (SDL_Color){255,220,110,255});
            drawSmallFlower(renderer, 250, 150, 10, (SDL_Color){230,160,200,150}, (SDL_Color){255,200,90,255});
            drawSmallFlower(renderer, 1350, 150, 10, (SDL_Color){230,160,200,150}, (SDL_Color){255,200,90,255});
            drawSmallFlower(renderer, 300, 800, 8, (SDL_Color){255,190,210,140}, (SDL_Color){255,215,100,255});
            drawSmallFlower(renderer, 1300, 800, 8, (SDL_Color){255,190,210,140}, (SDL_Color){255,215,100,255});
            drawSmallFlower(renderer, 750, 690, 9, (SDL_Color){245,170,200,120}, (SDL_Color){255,210,100,255});
            drawSmallFlower(renderer, 850, 700, 7, (SDL_Color){235,155,190,120}, (SDL_Color){255,220,110,255});

            int cx = WIN_W / 2;
            SDL_Color dark = {120, 30, 70, 255};
            SDL_Color soft = {170, 90, 130, 255};

            renderCenterBold(renderer, fHuge, "SIMPSON'S 1/3 RULE",
                             cx, 65, dark);
            renderCenterBold(renderer, fBig, "3rd Degree Polynomial Integration", cx, 115, dark);

            renderCenterText(renderer, fLarge,
                             "A beautiful numerical integration program",
                             cx, 180, soft);

            {
                SDL_Color bg = hoverSolver ? (SDL_Color){255,215,225,255}
                                           : (SDL_Color){255,200,215,255};
                SDL_SetRenderDrawColor(renderer, 180,120,140,100);
                SDL_Rect sh = {cardSolver.x+6, cardSolver.y+6,
                               cardSolver.w, cardSolver.h};
                SDL_RenderFillRect(renderer, &sh);
                SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
                SDL_RenderFillRect(renderer, &cardSolver);
                SDL_SetRenderDrawColor(renderer, 210,140,170,255);
                SDL_RenderDrawRect(renderer, &cardSolver);

                int ix = cardSolver.x + cardSolver.w/2 - 25;
                int iy = cardSolver.y + 70;
                SDL_Color gc[3][3] = {
                    {{255,130,160,255}, {255,170,100,255}, {180,130,255,255}},
                    {{255,100,140,255}, {255,200,120,255}, {200,150,255,255}},
                    {{255,160,180,255}, {255,180,130,255}, {220,170,255,255}}
                };
                for (int r = 0; r < 3; r++)
                    for (int c = 0; c < 3; c++) {
                        SDL_SetRenderDrawColor(renderer,
                            gc[r][c].r, gc[r][c].g, gc[r][c].b, 255);
                        SDL_Rect cell = {ix + c*18, iy + r*18, 15, 15};
                        SDL_RenderFillRect(renderer, &cell);
                    }

                drawSmallFlower(renderer, cardSolver.x + 40, cardSolver.y + 40, 8,
                    (SDL_Color){255,180,200,180}, (SDL_Color){255,220,100,255});
                drawSmallFlower(renderer, cardSolver.x + cardSolver.w - 40, cardSolver.y + cardSolver.h - 40, 8,
                    (SDL_Color){255,180,200,180}, (SDL_Color){255,220,100,255});

                renderCenterBold(renderer, fBig, "SOLVER",
                    cardSolver.x + cardSolver.w/2, cardSolver.y + 175,
                    (SDL_Color){120,30,70,255});
            }

            {
                SDL_Color bg = hoverAbout ? (SDL_Color){235,215,245,255}
                                          : (SDL_Color){225,200,238,255};
                SDL_SetRenderDrawColor(renderer, 160,130,180,100);
                SDL_Rect sh = {cardAbout.x+6, cardAbout.y+6,
                               cardAbout.w, cardAbout.h};
                SDL_RenderFillRect(renderer, &sh);
                SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
                SDL_RenderFillRect(renderer, &cardAbout);
                SDL_SetRenderDrawColor(renderer, 190,160,210,255);
                SDL_RenderDrawRect(renderer, &cardAbout);

                int ix = cardAbout.x + cardAbout.w/2 - 20;
                int iy = cardAbout.y + 75;
                SDL_SetRenderDrawColor(renderer, 200, 120, 170, 255);
                SDL_Rect ib = {ix, iy, 40, 40};
                SDL_RenderFillRect(renderer, &ib);
                SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
                SDL_RenderDrawPoint(renderer, ix, iy);
                SDL_RenderDrawPoint(renderer, ix+39, iy);
                SDL_RenderDrawPoint(renderer, ix, iy+39);
                SDL_RenderDrawPoint(renderer, ix+39, iy+39);
                int tw = textW(fLarge, "i");
                renderBold(renderer, fLarge, "i",
                           ix + 20 - tw/2, iy + 8,
                           (SDL_Color){255,255,255,255});

                drawSmallFlower(renderer, cardAbout.x + 40, cardAbout.y + 40, 8,
                    (SDL_Color){220,170,230,180}, (SDL_Color){255,220,100,255});
                drawSmallFlower(renderer, cardAbout.x + cardAbout.w - 40, cardAbout.y + cardAbout.h - 40, 8,
                    (SDL_Color){220,170,230,180}, (SDL_Color){255,220,100,255});

                renderCenterBold(renderer, fBig, "ABOUT US",
                    cardAbout.x + cardAbout.w/2, cardAbout.y + 155,
                    (SDL_Color){100,50,120,255});
                renderCenterBold(renderer, fBig, "(INFO)",
                    cardAbout.x + cardAbout.w/2, cardAbout.y + 190,
                    (SDL_Color){100,50,120,255});
            }

            drawFlower(renderer, 180, 500, 12, (SDL_Color){255,170,200,140}, (SDL_Color){255,220,100,255});
            drawFlower(renderer, 1420, 500, 12, (SDL_Color){240,160,210,140}, (SDL_Color){255,210,100,255});
        }

        /* ---- ABOUT US PAGE ---- */
        else if (screen == SCREEN_ABOUT) {
            SDL_SetRenderDrawColor(renderer, 255, 245, 250, 255);
            SDL_RenderClear(renderer);

            int cx = WIN_W / 2;
            SDL_Color dark   = {120, 30, 70, 255};
            SDL_Color pink   = {190, 70, 120, 255};
            SDL_Color accent = {160, 50, 100, 255};
            SDL_Color line   = {220, 160, 190, 255};

            renderButton(renderer, fNorm, &btnBack);
            renderCenterBold(renderer, fHuge, "ABOUT US", cx, 50, dark);

            drawPanel(renderer, 250, 110, 1100, 710,
                      (SDL_Color){255,248,252,255}, line);

            drawFlower(renderer, 290, 145, 14, (SDL_Color){255,180,200,160}, (SDL_Color){255,220,100,255});
            drawFlower(renderer, 1310, 145, 14, (SDL_Color){240,160,200,160}, (SDL_Color){255,210,100,255});
            drawFlower(renderer, 290, 780, 12, (SDL_Color){255,170,210,140}, (SDL_Color){255,220,110,255});
            drawFlower(renderer, 1310, 780, 12, (SDL_Color){240,155,195,140}, (SDL_Color){255,215,105,255});

            renderCenterBold(renderer, fBig, "SIMPSON'S 1/3 RULE", cx, 140, dark);
            renderCenterBold(renderer, fLarge,
                "3RD DEGREE POLYNOMIAL INTEGRATION", cx, 178, dark);

            SDL_SetRenderDrawColor(renderer, line.r, line.g, line.b, 255);
            SDL_RenderDrawLine(renderer, 350, 218, 1250, 218);

            renderCenterBold(renderer, fTitle,
                "NUMERICAL METHOD  |  MT 221", cx, 238, pink);
            renderCenterBold(renderer, fLarge, "A FINAL PROJECT", cx, 275, accent);
            renderCenterBold(renderer, fLarge, "BSCPE 22001", cx, 305, accent);

            SDL_SetRenderDrawColor(renderer, line.r, line.g, line.b, 255);
            SDL_RenderDrawLine(renderer, 350, 340, 1250, 340);

            renderCenterBold(renderer, fTitle, "PROGRAM CREATED BY:", cx, 360, dark);
            renderCenterBold(renderer, fBig, "LARGA, ANNA MARIE", cx, 405, pink);
            renderCenterBold(renderer, fBig, "VISTA, LEIMARY", cx, 445, pink);

            SDL_SetRenderDrawColor(renderer, line.r, line.g, line.b, 255);
            SDL_RenderDrawLine(renderer, 350, 490, 1250, 490);

            SDL_Color desc = {140, 60, 100, 255};
            renderCenterText(renderer, fNorm,
                "This program performs numerical integration using Simpson's 1/3 Rule",
                cx, 515, desc);
            renderCenterText(renderer, fNorm,
                "on a 3rd degree polynomial f(x) = Ax^3 + Bx^2 + Cx + D over",
                cx, 540, desc);
            renderCenterText(renderer, fNorm,
                "the interval [a, b] with n subintervals (n must be even).",
                cx, 565, desc);
            renderCenterText(renderer, fNorm,
                "The program displays step-by-step calculation, a node table,",
                cx, 590, desc);
            renderCenterText(renderer, fNorm,
                "a graph of the function with shaded area, and verification.",
                cx, 625, desc);

            renderCenterText(renderer, fSmall,
                "Built with SDL2 and C  |  2026",
                cx, 680, (SDL_Color){190,140,170,255});

            SDL_SetRenderDrawColor(renderer, 200, 100, 150, 255);
            SDL_Rect bar = {250, 790, 1100, 6};
            SDL_RenderFillRect(renderer, &bar);
        }

        /* ---- LOADING SCREEN ---- */
        else if (screen == SCREEN_LOADING) {
            SDL_SetRenderDrawColor(renderer, 255, 245, 250, 255);
            SDL_RenderClear(renderer);

            drawFlower(renderer, 120, 120, 20, (SDL_Color){255,180,210,140}, (SDL_Color){255,220,100,255});
            drawFlower(renderer, 1480, 120, 20, (SDL_Color){240,160,200,140}, (SDL_Color){255,210,100,255});
            drawFlower(renderer, 120, 780, 16, (SDL_Color){255,170,200,130}, (SDL_Color){255,225,110,255});
            drawFlower(renderer, 1480, 780, 16, (SDL_Color){240,155,190,130}, (SDL_Color){255,215,105,255});
            drawSmallFlower(renderer, 300, 200, 10, (SDL_Color){245,170,200,100}, (SDL_Color){255,210,100,255});
            drawSmallFlower(renderer, 1300, 200, 10, (SDL_Color){245,170,200,100}, (SDL_Color){255,210,100,255});
            drawSmallFlower(renderer, 400, 750, 8, (SDL_Color){245,170,200,100}, (SDL_Color){255,210,100,255});
            drawSmallFlower(renderer, 1200, 750, 8, (SDL_Color){245,170,200,100}, (SDL_Color){255,210,100,255});

            int cx = WIN_W / 2;
            Uint32 elapsed = SDL_GetTicks() - loadStart;
            float progress = (float)elapsed / 5000.0f;
            if (progress > 1.0f) progress = 1.0f;

            SDL_Color dark = {120, 30, 70, 255};

            renderCenterBold(renderer, fBig, "SIMPSON'S 1/3 RULE",    cx, 100, dark);
            renderCenterBold(renderer, fBig, "3RD DEGREE",            cx, 140, dark);
            renderCenterBold(renderer, fBig, "POLYNOMIAL INTEGRATION",cx, 180, dark);

            renderCenterBold(renderer, fBig, "NUMERICAL METHOD",      cx, 260, dark);
            renderCenterBold(renderer, fBig, "MT 221",                cx, 300, dark);
            renderCenterBold(renderer, fBig, "A FINAL PROJECT",       cx, 340, dark);

            renderCenterBold(renderer, fBig, "PROGRAM CREATED BY:",   cx, 420, dark);
            renderCenterBold(renderer, fBig, "LARGA, ANNA MARIE",    cx, 460, dark);
            renderCenterBold(renderer, fBig, "VISTA, LEIMARY",       cx, 500, dark);

            renderCenterText(renderer, fLarge, "Loading Program", cx, 580, dark);

            int barW = 420, barH = 42;
            int barX = cx - barW/2, barY = 620;

            SDL_SetRenderDrawColor(renderer, 140, 50, 90, 255);
            SDL_Rect outer = {barX - 6, barY - 6, barW + 12, barH + 12};
            SDL_RenderFillRect(renderer, &outer);

            SDL_SetRenderDrawColor(renderer, 80, 30, 55, 255);
            SDL_Rect inner = {barX, barY, barW, barH};
            SDL_RenderFillRect(renderer, &inner);

            int fillW = (int)(barW * progress);
            for (int px = 0; px < fillW; px++) {
                float t = (float)px / (float)barW;
                int rv = (int)(200 + t * 55);
                int gv = (int)(100 + t * 80);
                int bv = (int)(140 + t * 60);
                SDL_SetRenderDrawColor(renderer, rv, gv, bv, 255);
                SDL_RenderDrawLine(renderer, barX + px, barY,
                                             barX + px, barY + barH - 1);
            }

            char pct[8];
            sprintf(pct, "%d%%", (int)(progress * 100));
            renderCenterBold(renderer, fLarge, pct, cx, barY + 9,
                             (SDL_Color){255,255,255,255});
        }

        /* ---- SOLVER PAGE ---- */
        else if (screen == SCREEN_SOLVER) {
            SDL_SetRenderDrawColor(renderer, 255, 245, 250, 255);
            SDL_RenderClear(renderer);

            SDL_Color white   = {255,255,255,255};
            SDL_Color cream   = {255,220,235,255};
            SDL_Color secCol  = {140, 40, 85, 255};
            SDL_Color darkTxt = {100, 20, 60, 255};
            SDL_Color hintCol = {180,110,145, 255};
            SDL_Color panelBg = {255,248,252, 255};
            SDL_Color panBdr  = {220,160,190, 255};
            SDL_Color eqBg    = {255,240,248, 255};
            SDL_Color eqBdr   = {210,150,180, 255};

            for (int i = 0; i < 80; i++) {
                int v = (i<5)?(i*8):(i>75?((79-i)*10):0);
                SDL_SetRenderDrawColor(renderer, 180+v/3, 60+v/2, 110+v/2, 255);
                SDL_RenderDrawLine(renderer, 0, i, WIN_W, i);
            }
            renderBold(renderer, fTitle, "SIMPSON'S 1/3 RULE", 30, 14, white);
            renderText(renderer, fLarge, "3rd Degree Polynomial  |  Numerical Integration", 30, 46, cream);
            renderText(renderer, fSmall, "MT221 - Numerical Methods  |  Semestral Project",
                       1130, 10, cream);
            renderText(renderer, fNorm,  "BSCPE 22001", 1195, 32, white);
            renderText(renderer, fSmall, "Vista, Leimary  |  Larga, Anna Marie",
                       1130, 54, cream);

            drawSmallFlower(renderer, 950, 30, 7, (SDL_Color){255,200,220,180}, (SDL_Color){255,225,110,255});
            drawSmallFlower(renderer, 1010, 55, 6, (SDL_Color){255,190,215,160}, (SDL_Color){255,220,100,255});

            renderButton(renderer, fSmall, &btnBackSolver);

            drawPanel(renderer, 14, 88, 495, 835, panelBg, panBdr);
            renderBold(renderer, fLarge, "INPUT COEFFICIENTS", 85, 100, secCol);

            drawSmallFlower(renderer, 470, 100, 7, (SDL_Color){255,190,215,120}, (SDL_Color){255,220,100,255});

            drawPanel(renderer, 30, 126, 462, 58, eqBg, eqBdr);
            renderText(renderer, fSmall, "Function:", 44, 132, hintCol);
            {
                int tx = 44, ty = 148;
                SDL_Color ec = {180, 50, 100, 255};
                renderBold(renderer, fMed, "f(x) = ", tx, ty, ec);
                tx += textW(fMed, "f(x) = ");
                tx = renderWithSup(renderer, fMed, fSup, "Ax", "3", tx, ty, ec);
                renderText(renderer, fMed, " + ", tx, ty, ec); tx += textW(fMed, " + ");
                tx = renderWithSup(renderer, fMed, fSup, "Bx", "2", tx, ty, ec);
                renderText(renderer, fMed, " + Cx + D", tx, ty, ec);
            }

            drawPanel(renderer, 30, 195, 462, 90, eqBg, eqBdr);
            renderBold(renderer, fNorm, "POLYNOMIAL COEFFICIENTS", 140, 200, secCol);
            int boxW = 80, boxH = 36;
            for (int i = 0; i < 4; i++)
                inputs[i].rect = (SDL_Rect){40 + i*112, 240, boxW, boxH};
            for (int i = 0; i < 4; i++) renderInputBox(renderer, fSmall, &inputs[i]);

            drawPanel(renderer, 30, 300, 462, 90, eqBg, eqBdr);
            renderBold(renderer, fNorm, "INTEGRATION BOUNDS & INTERVALS", 110, 305, secCol);
            inputs[4].rect = (SDL_Rect){40,  345, 120, boxH};
            inputs[5].rect = (SDL_Rect){185, 345, 120, boxH};
            inputs[6].rect = (SDL_Rect){330, 345, 120, boxH};
            for (int i = 4; i < 7; i++) renderInputBox(renderer, fSmall, &inputs[i]);

            drawPanel(renderer, 30, 405, 462, 60, eqBg, eqBdr);
            renderBold(renderer, fNorm, "EQUATION PREVIEW", 158, 409, secCol);
            {
                double pA=atof(inputs[0].value), pB=atof(inputs[1].value);
                double pC=atof(inputs[2].value), pD=atof(inputs[3].value);
                char prev[120];
                sprintf(prev, "f(x) = %.2gx^3 + %.2gx^2 + %.2gx + %.2g", pA, pB, pC, pD);
                renderBold(renderer, fNorm, prev, 44, 435, (SDL_Color){180,50,100,255});
            }

            renderButton(renderer, fNorm, &btnCompute);
            renderButton(renderer, fNorm, &btnClear);

            if (hasError && strlen(errorMsg) > 0) {
                drawPanel(renderer, 30, 590, 462, 36,
                          (SDL_Color){255,230,235,255}, (SDL_Color){210,80,100,255});
                renderText(renderer, fSmall, errorMsg, 42, 598, (SDL_Color){180,30,50,255});
            }

            if (hasSol) {
                drawPanel(renderer, 30, 640, 462, 92,
                          (SDL_Color){255,230,240,255}, (SDL_Color){210,120,160,255});
                renderBold(renderer, fMed, "RESULT", 210, 646, (SDL_Color){140,30,75,255});
                char buf[120];
                sprintf(buf, "Approx: %.6f", simpsonResult);
                renderBold(renderer, fLarge, buf, 48, 672, (SDL_Color){180,50,100,255});
                sprintf(buf, "Exact:  %.6f   |  Error: %.2e",
                        exactResult, fabs(simpsonResult - exactResult));
                renderText(renderer, fSmall, buf, 48, 700, (SDL_Color){160,70,110,255});

                drawSmallFlower(renderer, 460, 650, 6, (SDL_Color){255,190,210,140}, (SDL_Color){255,220,100,255});
            }

            drawPanel(renderer, 520, 88, 555, 835, panelBg, panBdr);
            renderBold(renderer, fLarge, "SOLUTION STEPS", 688, 100, secCol);

            if (hasSol) {
                int sy = 125;
                char sbuf[256];

                drawPanel(renderer, 534, sy, 526, 55, (SDL_Color){255,240,248,255}, eqBdr);
                renderBold(renderer, fMed, "GIVEN", 548, sy+5, secCol);
                SDL_SetRenderDrawColor(renderer, eqBdr.r,eqBdr.g,eqBdr.b,255);
                SDL_RenderDrawLine(renderer, 548, sy+25, 1050, sy+25);
                sprintf(sbuf,"f(x) = %.2gx^3 + %.2gx^2 + %.2gx + %.2g   on [%.2g, %.2g],  n = %d",
                        sA, sB, sC, sD, sa, sb, sn);
                renderText(renderer,fSmall,sbuf,558,sy+30,darkTxt);
                sy += 60;

                drawPanel(renderer, 534, sy, 526, 45, (SDL_Color){255,242,250,255}, eqBdr);
                renderBold(renderer, fMed, "STEP 1: Compute h", 548, sy+5, secCol);
                SDL_SetRenderDrawColor(renderer, eqBdr.r,eqBdr.g,eqBdr.b,255);
                SDL_RenderDrawLine(renderer, 548, sy+22, 1050, sy+22);
                sprintf(sbuf,"h = (b - a) / n = (%.2g - %.2g) / %d = %.6f", sb, sa, sn, h_val);
                renderText(renderer,fNorm,sbuf,558,sy+25,darkTxt);
                sy += 50;

                int tableH = nodeCount * 18 + 30;
                int maxTableH = 320;
                int dispH = tableH < maxTableH ? tableH : maxTableH;

                drawPanel(renderer, 534, sy, 526, dispH, (SDL_Color){255,245,252,255}, eqBdr);
                renderBold(renderer, fMed, "STEP 2: Node Table", 548, sy+5, secCol);
                SDL_SetRenderDrawColor(renderer, eqBdr.r,eqBdr.g,eqBdr.b,255);
                SDL_RenderDrawLine(renderer, 548, sy+22, 1050, sy+22);

                renderBold(renderer,fSmall,"i",558,sy+25,secCol);
                renderBold(renderer,fSmall,"xi",608,sy+25,secCol);
                renderBold(renderer,fSmall,"f(xi)",718,sy+25,secCol);
                renderBold(renderer,fSmall,"Coeff",868,sy+25,secCol);
                renderBold(renderer,fSmall,"Contrib",968,sy+25,secCol);

                SDL_Rect clipRect = {534, sy+40, 526, dispH-45};
                SDL_RenderSetClipRect(renderer, &clipRect);

                for (int i = 0; i < nodeCount; i++) {
                    int ry = sy + 42 + i*18 - scrollY;
                    if (ry < sy+35 || ry > sy+dispH) continue;

                    if (i % 2 == 0) {
                        SDL_SetRenderDrawColor(renderer, 255, 240, 248, 255);
                    } else {
                        SDL_SetRenderDrawColor(renderer, 255, 248, 252, 255);
                    }
                    SDL_Rect rowBg = {535, ry-1, 524, 18};
                    SDL_RenderFillRect(renderer, &rowBg);

                    int coeff = 1;
                    if (i == 0 || i == sn) coeff = 1;
                    else if (i % 2 == 1) coeff = 4;
                    else coeff = 2;

                    sprintf(sbuf, "%d", i);
                    renderText(renderer, fSmall, sbuf, 558, ry, darkTxt);
                    sprintf(sbuf, "%.4f", nodeX[i]);
                    renderText(renderer, fSmall, sbuf, 600, ry, darkTxt);
                    sprintf(sbuf, "%.6f", nodeF[i]);
                    renderText(renderer, fSmall, sbuf, 700, ry, darkTxt);
                    sprintf(sbuf, "%d", coeff);
                    renderText(renderer, fSmall, sbuf, 880, ry,
                        coeff==4 ? (SDL_Color){200,60,100,255} :
                        coeff==2 ? (SDL_Color){180,100,50,255} :
                                   (SDL_Color){100,20,60,255});
                    sprintf(sbuf, "%.6f", coeff * nodeF[i]);
                    renderText(renderer, fSmall, sbuf, 940, ry, darkTxt);
                }

                SDL_RenderSetClipRect(renderer, NULL);

                if (tableH > maxTableH) {
                    int sbH = (int)((float)maxTableH / tableH * (dispH-45));
                    if (sbH < 20) sbH = 20;
                    int maxScroll = tableH - maxTableH + 45;
                    if (scrollY > maxScroll) scrollY = maxScroll;
                    int sbY = sy + 40 + (int)((float)scrollY / maxScroll * (dispH - 45 - sbH));
                    SDL_SetRenderDrawColor(renderer, 210, 150, 180, 200);
                    SDL_Rect scrollBar = {1054, sbY, 6, sbH};
                    SDL_RenderFillRect(renderer, &scrollBar);
                }

                sy += dispH + 5;

                drawPanel(renderer, 534, sy, 526, 75, (SDL_Color){255,240,248,255}, eqBdr);
                renderBold(renderer, fMed, "STEP 3: Group Sums", 548, sy+5, secCol);
                SDL_SetRenderDrawColor(renderer, eqBdr.r,eqBdr.g,eqBdr.b,255);
                SDL_RenderDrawLine(renderer, 548, sy+22, 1050, sy+22);
                sprintf(sbuf, "Endpoints (x1):  f(x0) + f(x%d) = %.6f", sn, sumEndpoints);
                renderText(renderer, fSmall, sbuf, 558, sy+26, darkTxt);
                sprintf(sbuf, "Odd nodes (x4):  Sum = %.6f", sumOdd);
                renderText(renderer, fSmall, sbuf, 558, sy+42, (SDL_Color){200,60,100,255});
                sprintf(sbuf, "Even nodes (x2): Sum = %.6f", sumEven);
                renderText(renderer, fSmall, sbuf, 558, sy+58, (SDL_Color){180,100,50,255});
                sy += 82;

                drawPanel(renderer, 534, sy, 526, 58, (SDL_Color){255,238,248,255}, eqBdr);
                renderBold(renderer, fMed, "STEP 4: Apply Formula", 548, sy+5, secCol);
                SDL_SetRenderDrawColor(renderer, eqBdr.r,eqBdr.g,eqBdr.b,255);
                SDL_RenderDrawLine(renderer, 548, sy+22, 1050, sy+22);
                sprintf(sbuf, "I = (h/3)[f(x0)+f(xn) + 4*SumOdd + 2*SumEven]");
                renderText(renderer, fSmall, sbuf, 558, sy+25, darkTxt);
                sprintf(sbuf, "I = (%.6f/3)[%.4f + 4(%.4f) + 2(%.4f)] = %.6f",
                        h_val, sumEndpoints, sumOdd, sumEven, simpsonResult);
                renderBold(renderer, fSmall, sbuf, 558, sy+40, (SDL_Color){180,50,100,255});
                sy += 65;

                drawPanel(renderer, 534, sy, 526, 40,
                          (SDL_Color){255,230,240,255}, (SDL_Color){210,120,160,255});
                renderBold(renderer, fMed, "ANSWER", 548, sy+5, (SDL_Color){140,30,75,255});
                SDL_SetRenderDrawColor(renderer, 210,120,160,255);
                SDL_RenderDrawLine(renderer, 548, sy+22, 1050, sy+22);
                sprintf(sbuf, "Integral ~ %.6f", simpsonResult);
                renderBold(renderer, fNorm, sbuf, 650, sy+24, (SDL_Color){180,40,90,255});
                sy += 48;

                drawPanel(renderer, 534, sy, 526, 68,
                          (SDL_Color){248,238,255,255}, (SDL_Color){175,140,210,255});
                renderBold(renderer, fMed, "VERIFICATION", 548, sy+5, (SDL_Color){100,50,150,255});
                SDL_SetRenderDrawColor(renderer, 175,140,210,255);
                SDL_RenderDrawLine(renderer, 548, sy+22, 1050, sy+22);
                sprintf(sbuf, "Exact (antiderivative):  %.6f", exactResult);
                renderText(renderer, fSmall, sbuf, 558, sy+26, darkTxt);
                double err = fabs(simpsonResult - exactResult);
                int pass = err < 0.05;
                sprintf(sbuf, "Absolute Error: %.2e     %s", err, pass ? "PASS" : "FAIL");
                renderText(renderer, fSmall, sbuf, 558, sy+44,
                    pass ? (SDL_Color){60,150,90,255} : (SDL_Color){200,30,50,255});
                renderText(renderer, fSmall,
                    "(Simpson's 1/3 is exact for polynomials up to degree 3)",
                    558, sy+56, hintCol);

            } else {
                renderText(renderer, fNorm, "Steps will appear here after pressing COMPUTE.",
                           590, 430, hintCol);
                drawSmallFlower(renderer, 780, 500, 10, (SDL_Color){255,190,215,100}, (SDL_Color){255,220,100,255});
            }

            drawPanel(renderer, 1085, 88, 500, 835, panelBg, panBdr);
            renderBold(renderer, fLarge, "GRAPH", 1295, 100, secCol);

            drawSmallFlower(renderer, 1110, 100, 6, (SDL_Color){255,190,210,120}, (SDL_Color){255,220,100,255});

            drawGraph(renderer, fSmall, sA, sB, sC, sD, sa, sb, sn, hasSol);

            int LY = 635;
            drawPanel(renderer,1098,LY,462,88,eqBg,eqBdr);
            renderBold(renderer,fNorm,"LEGEND",1295,LY+5,secCol);
            SDL_SetRenderDrawColor(renderer,180,50,100,255);
            SDL_Rect l1={1115,LY+30,28,4}; SDL_RenderFillRect(renderer,&l1);
            renderText(renderer,fNorm,"f(x) curve (pink)",1153,LY+23,(SDL_Color){180,50,100,255});
            SDL_SetRenderDrawColor(renderer,255,140,0,255);
            for (int di=-4;di<=4;di++) for (int dj=-4;dj<=4;dj++)
                if (di*di+dj*dj<=16)
                    SDL_RenderDrawPoint(renderer,1127+di,LY+55+dj);
            renderText(renderer,fNorm,"Node points (orange)",1153,LY+48,(SDL_Color){220,120,0,255});
            SDL_SetRenderDrawColor(renderer,220,110,160,60);
            SDL_Rect l3={1115,LY+72,28,12}; SDL_RenderFillRect(renderer,&l3);
            renderText(renderer,fNorm,"Shaded area",1153,LY+68,(SDL_Color){200,90,140,255});

            drawSmallFlower(renderer, 1540, 900, 7, (SDL_Color){255,185,210,120}, (SDL_Color){255,220,100,255});
            drawSmallFlower(renderer, 30, 900, 7, (SDL_Color){255,185,210,120}, (SDL_Color){255,220,100,255});
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    TTF_CloseFont(fHuge);  TTF_CloseFont(fBig);
    TTF_CloseFont(fTitle); TTF_CloseFont(fLarge); TTF_CloseFont(fMed);
    TTF_CloseFont(fNorm);  TTF_CloseFont(fSmall);
    TTF_CloseFont(fSup);   TTF_CloseFont(fSub);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
