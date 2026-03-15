/*
 * Substitution Method - Non-Linear Equations with 2 Unknowns
 * MT221 - Numerical Methods | Semestral Project
 * BSCPE 22003
 * Soriano, Gilbert  |  Fulla, John Micheal
 *
 * Equations:
 *   Eq1:  A1*x^n1 + B1*y^n2 = C1
 *   Eq2:  A2*x^m1 + B2*y^m2 = C2
 *
 * Algorithm (Pure Substitution):
 *   1. From Eq1, isolate x:  x = [(C1 - B1*y^n2) / A1]^(1/n1)
 *   2. Substitute x-expression into Eq2 -> single equation in y
 *   3. Solve for y
 *   4. Back-substitute y into x-expression to find x
 *   5. Check solution in both original equations
 *
 * Screens: Landing -> Loading (5s) -> Solver
 *          Landing -> About Us -> Landing
 */

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

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

static double ipow(double base, int e) {
    if (e == 0) return 1.0;
    if (e <  0) return 1.0 / ipow(base, -e);
    int odd = (e % 2 != 0);
    double r = pow(fabs(base), (double)e);
    return (odd && base < 0) ? -r : r;
}

static double iroot(double v, int n) {
    if (n == 0) return NAN;
    int odd = (n % 2 != 0);
    if (v < 0 && !odd) return NAN;
    double r = pow(fabs(v), 1.0 / (double)n);
    return (v < 0) ? -r : r;
}

static double xFromY(double y, double A1, int n1, double B1, int n2, double C1) {
    if (fabs(A1) < 1e-14) return NAN;
    return iroot((C1 - B1 * ipow(y, n2)) / A1, n1);
}

static double fSubEq(double y,
                     double A1, int n1, double B1, int n2, double C1,
                     double A2, int m1, double B2, int m2, double C2) {
    double x = xFromY(y, A1, n1, B1, n2, C1);
    if (isnan(x)) return NAN;
    return A2 * ipow(x, m1) + B2 * ipow(y, m2) - C2;
}

static void renderText(SDL_Renderer* r, TTF_Font* f, const char* t,
                       int x, int y, SDL_Color c) {
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

static int renderWithSup(SDL_Renderer* r, TTF_Font* fNorm, TTF_Font* fSup,
                         const char* base, const char* sup,
                         int x, int y, SDL_Color c) {
    renderBold(r, fNorm, base, x, y, c);
    x += textW(fNorm, base);
    renderText(r, fSup, sup, x, y - 6, c);
    x += textW(fSup, sup) + 1;
    return x;
}

static int renderWithSub(SDL_Renderer* r, TTF_Font* fNorm, TTF_Font* fSb,
                         const char* base, const char* sub,
                         int x, int y, SDL_Color c) {
    renderBold(r, fNorm, base, x, y, c);
    x += textW(fNorm, base);
    renderText(r, fSb, sub, x, y + 7, c);
    x += textW(fSb, sub) + 1;
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
    SDL_Color lc = {140, 30, 70, 255};
    renderBold(rr, f, box->label, box->rect.x, box->rect.y - 20, lc);
    SDL_SetRenderDrawColor(rr, box->active ? 255 : 255,
                               box->active ? 230 : 240,
                               box->active ? 240 : 248, 255);
    SDL_RenderFillRect(rr, &box->rect);
    SDL_SetRenderDrawColor(rr, box->active ? 180 : 210,
                                box->active ? 40  : 140,
                                box->active ? 80  : 170, 255);
    SDL_RenderDrawRect(rr, &box->rect);
    if (box->active) {
        SDL_Rect inn = {box->rect.x+1, box->rect.y+1, box->rect.w-2, box->rect.h-2};
        SDL_RenderDrawRect(rr, &inn);
    }
    if (strlen(box->value) > 0)
        renderText(rr, f, box->value,
                   box->rect.x + 8, box->rect.y + 7,
                   (SDL_Color){100, 20, 50, 255});
}

static void renderButton(SDL_Renderer* rr, TTF_Font* f, Button* btn) {
    SDL_Color bg = btn->clicked  ? (SDL_Color){120, 20, 55, 255}
                 : btn->hovered  ? (SDL_Color){200, 60,100, 255}
                                 : (SDL_Color){165, 40, 80, 255};
    SDL_SetRenderDrawColor(rr, 90, 10, 40, 255);
    SDL_Rect sh = {btn->rect.x+3, btn->rect.y+3, btn->rect.w, btn->rect.h};
    SDL_RenderFillRect(rr, &sh);
    SDL_SetRenderDrawColor(rr, bg.r, bg.g, bg.b, bg.a);
    SDL_RenderFillRect(rr, &btn->rect);
    SDL_SetRenderDrawColor(rr, 90, 10, 40, 255);
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

static void drawFilledCircle(SDL_Renderer* r, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; dy++)
        for (int dx = -radius; dx <= radius; dx++)
            if (dx*dx + dy*dy <= radius*radius)
                SDL_RenderDrawPoint(r, cx+dx, cy+dy);
}

static void drawHelloKitty(SDL_Renderer* r, int cx, int cy, int size) {
    int s = size;
    /* ---- LEFT EAR (normal) ---- */
    SDL_SetRenderDrawColor(r, 255, 228, 240, 255);
    for (int dy = -s; dy <= 0; dy++)
        for (int dx = -(s/2); dx <= s/2; dx++)
            if (abs(dx)*3/2 + abs(dy) <= s*9/16)
                SDL_RenderDrawPoint(r, cx - s*3/4 + dx, cy - s*6/7 + dy);
    /* ---- RIGHT EAR (bigger = weird asymmetry) ---- */
    for (int dy = -(s + s/5); dy <= 0; dy++)
        for (int dx = -(s*55/100); dx <= s*55/100; dx++)
            if (abs(dx)*3/2 + abs(dy) <= s*10/16)
                SDL_RenderDrawPoint(r, cx + s*3/4 + dx, cy - s*6/7 + dy);
    /* ---- INNER EARS (hot pink) ---- */
    SDL_SetRenderDrawColor(r, 255, 105, 155, 255);
    for (int dy = -(s/2); dy <= 0; dy++)
        for (int dx = -(s/4); dx <= s/4; dx++)
            if (abs(dx)*2 + abs(dy) <= s*5/16)
                SDL_RenderDrawPoint(r, cx - s*3/4 + dx, cy - s*6/7 + dy);
    for (int dy = -(s*3/5); dy <= 0; dy++)
        for (int dx = -(s*3/10); dx <= s*3/10; dx++)
            if (abs(dx)*2 + abs(dy) <= s*6/16)
                SDL_RenderDrawPoint(r, cx + s*3/4 + dx, cy - s*6/7 + dy);
    /* ---- FACE ---- */
    SDL_SetRenderDrawColor(r, 255, 253, 255, 255);
    drawFilledCircle(r, cx, cy, s);
    /* ---- FACE OUTLINE (double thick ring) ---- */
    SDL_SetRenderDrawColor(r, 220, 150, 180, 255);
    for (int ofs = 0; ofs <= 2; ofs++) {
        for (int a = 0; a < 360; a++) {
            float rad = (float)a * 3.14159f / 180.0f;
            SDL_RenderDrawPoint(r, cx + (int)(cosf(rad)*(s + ofs)),
                                   cy + (int)(sinf(rad)*(s + ofs)));
        }
    }
    /* ---- BLUSH CHEEKS (huge soft blobs) ---- */
    SDL_SetRenderDrawColor(r, 255, 148, 185, 255);
    drawFilledCircle(r, cx - s/2, cy + s/4, s*3/10);
    drawFilledCircle(r, cx + s/2, cy + s/4, s*3/10);
    SDL_SetRenderDrawColor(r, 255, 205, 220, 255);
    drawFilledCircle(r, cx - s/2, cy + s/4, s*15/100);
    drawFilledCircle(r, cx + s/2, cy + s/4, s*15/100);
    /* tiny freckle dots on cheeks (3 per side) */
    SDL_SetRenderDrawColor(r, 228, 115, 150, 220);
    drawFilledCircle(r, cx - s*36/100, cy + s*38/100, s/20 + 1);
    drawFilledCircle(r, cx - s*52/100, cy + s*38/100, s/20 + 1);
    drawFilledCircle(r, cx - s*44/100, cy + s*45/100, s/20 + 1);
    drawFilledCircle(r, cx + s*36/100, cy + s*38/100, s/20 + 1);
    drawFilledCircle(r, cx + s*52/100, cy + s*38/100, s/20 + 1);
    drawFilledCircle(r, cx + s*44/100, cy + s*45/100, s/20 + 1);
    /* ---- LEFT EYE (normal size) ---- */
    SDL_SetRenderDrawColor(r, 15, 15, 25, 255);
    drawFilledCircle(r, cx - s/3, cy - s/9, s/6);
    /* ---- RIGHT EYE (slightly bigger = weird) ---- */
    drawFilledCircle(r, cx + s/3, cy - s/9, s*7/40);
    /* sparkly shine dots (2 per eye) */
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    drawFilledCircle(r, cx - s/3 + s/14, cy - s/9 - s/13, s/12);
    drawFilledCircle(r, cx - s/3 - s/13, cy - s/9 + s/16, s/22);
    drawFilledCircle(r, cx + s/3 + s/14, cy - s/9 - s/13, s/12);
    drawFilledCircle(r, cx + s/3 - s/13, cy - s/9 + s/16, s/22);
    /* ---- NOSE (golden heart shape, off-center) ---- */
    SDL_SetRenderDrawColor(r, 255, 195, 25, 255);
    drawFilledCircle(r, cx + s/8, cy + s/10, s/10);
    SDL_SetRenderDrawColor(r, 255, 228, 55, 255);
    drawFilledCircle(r, cx + s/8 - s/16, cy + s/10 - s/16, s/14);
    drawFilledCircle(r, cx + s/8 + s/16, cy + s/10 - s/16, s/14);
    /* ---- WEIRD SMILE (Hello Kitty normally has no mouth!) ---- */
    SDL_SetRenderDrawColor(r, 175, 45, 80, 240);
    for (int dx = -(s*15/100); dx <= s*15/100; dx++) {
        float t = (float)dx / (float)(s*16/100 + 1);
        int curvey = (int)((float)s * 0.07f * t * t);
        SDL_RenderDrawPoint(r, cx + dx + s/18, cy + s*27/100 + curvey);
        SDL_RenderDrawPoint(r, cx + dx + s/18, cy + s*28/100 + curvey);
        SDL_RenderDrawPoint(r, cx + dx + s/18, cy + s*29/100 + curvey);
    }
    /* ---- WHISKERS (long and dramatic, 6 total) ---- */
    SDL_SetRenderDrawColor(r, 75, 75, 105, 215);
    SDL_RenderDrawLine(r, cx - s/10, cy + s/10, cx - s - s*3/8, cy - s/9);
    SDL_RenderDrawLine(r, cx - s/10, cy + s/7,  cx - s - s*3/8, cy + s*2/9);
    SDL_RenderDrawLine(r, cx - s/10, cy + s/5,  cx - s - s*3/8, cy + s/2);
    SDL_RenderDrawLine(r, cx + s*2/9, cy + s/10, cx + s + s*3/8, cy - s/9);
    SDL_RenderDrawLine(r, cx + s*2/9, cy + s/7,  cx + s + s*3/8, cy + s*2/9);
    SDL_RenderDrawLine(r, cx + s*2/9, cy + s/5,  cx + s + s*3/8, cy + s/2);
    /* ---- BOW (oversized dramatic red bow, right-ear side) ---- */
    SDL_SetRenderDrawColor(r, 210, 20, 60, 255);
    for (int dy = -(s*40/100); dy <= s*40/100; dy++)
        for (int dx = -(s*45/100); dx <= 0; dx++)
            if (abs(dx)*6/5 + abs(dy) <= s*44/100 + 2)
                SDL_RenderDrawPoint(r, cx + s*72/100 + dx, cy - s*108/100 + dy);
    for (int dy = -(s*40/100); dy <= s*40/100; dy++)
        for (int dx = 0; dx <= s*45/100; dx++)
            if (abs(dx)*6/5 + abs(dy) <= s*44/100 + 2)
                SDL_RenderDrawPoint(r, cx + s*72/100 + dx + 3, cy - s*108/100 + dy);
    /* bow knot (bright pink) */
    SDL_SetRenderDrawColor(r, 255, 75, 125, 255);
    drawFilledCircle(r, cx + s*72/100, cy - s*108/100, s*15/100);
    /* bow highlight shine */
    SDL_SetRenderDrawColor(r, 255, 205, 225, 220);
    drawFilledCircle(r, cx + s*67/100, cy - s*117/100, s*6/100 + 1);
    /* sparkle dots near bow */
    SDL_SetRenderDrawColor(r, 255, 245, 250, 255);
    drawFilledCircle(r, cx + s*56/100, cy - s*132/100, s*4/100 + 1);
    drawFilledCircle(r, cx + s*94/100, cy - s*127/100, s*4/100 + 1);
    drawFilledCircle(r, cx + s*104/100, cy - s*107/100, s*35/1000 + 1);
}

static void drawGraph(SDL_Renderer* rr, TTF_Font* fSmall,
                      double A1, int n1, double B1, int n2, double C1,
                      double A2, int m1, double B2, int m2, double C2,
                      double solX, double solY, int hasSol) {
    int GX = 1100, GY = 195, GW = 470, GH = 415;
    double cx = hasSol ? solX : 0, cy = hasSol ? solY : 0;
    double span = 6.0;
    double xMin = cx-span, xMax = cx+span, yMin = cy-span, yMax = cy+span;

    drawPanel(rr, GX, GY, GW, GH,
              (SDL_Color){240,255,252,255}, (SDL_Color){100,190,180,255});

    SDL_SetRenderDrawColor(rr, 215, 245, 242, 255);
    for (int i = 0; i <= 10; i++) {
        int px = GX + i*GW/10;
        int py = GY + i*GH/10;
        SDL_RenderDrawLine(rr, px, GY, px, GY+GH);
        SDL_RenderDrawLine(rr, GX, py, GX+GW, py);
    }

    SDL_SetRenderDrawColor(rr, 0, 100, 100, 255);
    int ox = GX + (int)((-xMin)/(xMax-xMin)*GW);
    int oy = GY+GH - (int)((-yMin)/(yMax-yMin)*GH);
    if (ox >= GX && ox <= GX+GW) SDL_RenderDrawLine(rr, ox, GY, ox, GY+GH);
    if (oy >= GY && oy <= GY+GH) SDL_RenderDrawLine(rr, GX, oy, GX+GW, oy);
    renderText(rr, fSmall, "x", GX+GW-12, oy+4, (SDL_Color){0,80,80,255});
    renderText(rr, fSmall, "y", ox+4,     GY+4, (SDL_Color){0,80,80,255});

    for (int py = GY; py < GY+GH; py++) {
        double yv = yMax - (py-GY)/(double)GH*(yMax-yMin);
        for (int px = GX; px < GX+GW-1; px++) {
            double xv  = xMin + (px-GX  )/(double)GW*(xMax-xMin);
            double xv2 = xMin + (px-GX+1)/(double)GW*(xMax-xMin);
            double f1a = A1*ipow(xv, n1)+B1*ipow(yv,n2)-C1;
            double f1b = A1*ipow(xv2,n1)+B1*ipow(yv,n2)-C1;
            if (f1a*f1b <= 0) {
                SDL_SetRenderDrawColor(rr, 0, 160, 150, 255);
                SDL_RenderDrawPoint(rr, px, py);
                SDL_RenderDrawPoint(rr, px, py+1);
                SDL_RenderDrawPoint(rr, px, py-1);
            }
            double f2a = A2*ipow(xv, m1)+B2*ipow(yv,m2)-C2;
            double f2b = A2*ipow(xv2,m1)+B2*ipow(yv,m2)-C2;
            if (f2a*f2b <= 0) {
                SDL_SetRenderDrawColor(rr, 220, 120, 0, 255);
                SDL_RenderDrawPoint(rr, px, py);
                SDL_RenderDrawPoint(rr, px, py+1);
                SDL_RenderDrawPoint(rr, px, py-1);
            }
        }
    }

    if (hasSol) {
        int spx = GX+(int)((solX-xMin)/(xMax-xMin)*GW);
        int spy = GY+GH-(int)((solY-yMin)/(yMax-yMin)*GH);
        if (spx>=GX && spx<GX+GW && spy>=GY && spy<GY+GH) {
            SDL_SetRenderDrawColor(rr, 180,255,240,255);
            for (int di=-12;di<=12;di++) for (int dj=-12;dj<=12;dj++)
                if (di*di+dj*dj<=144 && di*di+dj*dj>49)
                    SDL_RenderDrawPoint(rr, spx+di, spy+dj);
            SDL_SetRenderDrawColor(rr, 220,50,50,255);
            for (int di=-7;di<=7;di++) for (int dj=-7;dj<=7;dj++)
                if (di*di+dj*dj<=49)
                    SDL_RenderDrawPoint(rr, spx+di, spy+dj);
            char lbl[32]; sprintf(lbl,"(%.3f, %.3f)", solX, solY);
            renderBold(rr, fSmall, lbl, spx+10, spy-20, (SDL_Color){150,20,20,255});
        }
    }

    char rb[16];
    sprintf(rb,"%.1f",xMin); renderText(rr,fSmall,rb,GX+2,oy+5,(SDL_Color){0,80,80,180});
    sprintf(rb,"%.1f",xMax); renderText(rr,fSmall,rb,GX+GW-28,oy+5,(SDL_Color){0,80,80,180});
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window* window = SDL_CreateWindow(
        "Substitution Method - Non-Linear Equations (2 Unknowns)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(renderer, WIN_W, WIN_H);

    TTF_Font* fHuge  = TTF_OpenFont("font.ttf", 38);
    TTF_Font* fBig   = TTF_OpenFont("font.ttf", 30);
    TTF_Font* fTitle = TTF_OpenFont("font.ttf", 29);
    TTF_Font* fLarge = TTF_OpenFont("font.ttf", 22);
    TTF_Font* fMed   = TTF_OpenFont("font.ttf", 19);
    TTF_Font* fNorm  = TTF_OpenFont("font.ttf", 17);
    TTF_Font* fSmall = TTF_OpenFont("font.ttf", 15);
    TTF_Font* fSup   = TTF_OpenFont("font.ttf", 13);
    TTF_Font* fSub   = TTF_OpenFont("font.ttf", 13);

    if (!fHuge||!fBig||!fTitle||!fLarge||!fMed||!fNorm||!fSmall||!fSup||!fSub) {
        printf("Font error: %s\n", TTF_GetError()); return 1;
    }

    Screen screen = SCREEN_LANDING;
    Uint32 loadStart = 0;
    int    hoverSolver = 0, hoverAbout = 0;

    InputBox inputs[10];
    const char* labels[10]   = {"A1","n1","B1","n2","C1","A2","m1","B2","m2","C2"};
    const char* defaults[10] = {"1","2","1","2","25","1","1","1","1","7"};
    for (int i = 0; i < 10; i++) {
        strcpy(inputs[i].label, labels[i]);
        strcpy(inputs[i].value, defaults[i]);
        inputs[i].active = 0;
        inputs[i].rect = (SDL_Rect){0,0,80,36};
    }

    Button btnCompute = {{55, 553, 175, 46}, "COMPUTE", 0, 0};
    Button btnClear   = {{255,553, 175, 46}, "CLEAR",   0, 0};
    Button btnBack    = {{30, 30, 160, 50},  "< BACK",  0, 0};
    Button btnConfirmYes = {{WIN_W/2-130, WIN_H/2+30, 110, 44}, "YES",    0, 0};
    Button btnConfirmNo  = {{WIN_W/2+20,  WIN_H/2+30, 110, 44}, "CANCEL", 0, 0};
    Button btnSolverBack = {{0, 0, 170, 44}, "< BACK", 0, 0};

    int    hasSol = 0, hasError = 0, activeInput = -1, quit = 0, showConfirm = 0;
    double solX = 0, solY = 0;
    double sA1=0,sB1=0,sC1=0, sA2=0,sB2=0,sC2=0;
    int    sn1=1,sn2=1,sm1=1,sm2=1;
    double step4_inner = 0;
    double step4_byN2  = 0;
    char   errorMsg[200] = "";

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
                if (ev.type == SDL_MOUSEBUTTONDOWN) {
                    int mx = ev.button.x, my = ev.button.y;

                    /* clear confirm modal takes priority */
                    if (showConfirm) {
                        if (mx>=btnConfirmYes.rect.x && mx<btnConfirmYes.rect.x+btnConfirmYes.rect.w &&
                            my>=btnConfirmYes.rect.y && my<btnConfirmYes.rect.y+btnConfirmYes.rect.h) {
                            showConfirm = 0;
                            for (int i = 0; i < 10; i++) strcpy(inputs[i].value, defaults[i]);
                            hasSol = 0; hasError = 0;
                            strcpy(errorMsg, "");
                        }
                        if (mx>=btnConfirmNo.rect.x && mx<btnConfirmNo.rect.x+btnConfirmNo.rect.w &&
                            my>=btnConfirmNo.rect.y && my<btnConfirmNo.rect.y+btnConfirmNo.rect.h) {
                            showConfirm = 0;
                        }
                        /* swallow all other clicks while modal is open */
                    } else {
                        /* SOLVER BACK button */
                        if (mx >= btnSolverBack.rect.x && mx < btnSolverBack.rect.x+btnSolverBack.rect.w &&
                            my >= btnSolverBack.rect.y && my < btnSolverBack.rect.y+btnSolverBack.rect.h) {
                            screen = SCREEN_LANDING;
                        }

                        /* normal input handling */
                        activeInput = -1;
                        for (int i = 0; i < 10; i++) {
                            inputs[i].active = 0;
                            if (mx >= inputs[i].rect.x && mx < inputs[i].rect.x+inputs[i].rect.w &&
                                my >= inputs[i].rect.y && my < inputs[i].rect.y+inputs[i].rect.h) {
                                activeInput = i;
                                inputs[i].active = 1;
                            }
                        }

                        if (mx >= btnCompute.rect.x && mx < btnCompute.rect.x+btnCompute.rect.w &&
                            my >= btnCompute.rect.y && my < btnCompute.rect.y+btnCompute.rect.h) {
                            btnCompute.clicked = 1;
                            hasSol = 0; hasError = 0;
                            strcpy(errorMsg, "");

                            sA1 = atof(inputs[0].value); sn1 = (int)round(atof(inputs[1].value));
                            sB1 = atof(inputs[2].value); sn2 = (int)round(atof(inputs[3].value));
                            sC1 = atof(inputs[4].value);
                            sA2 = atof(inputs[5].value); sm1 = (int)round(atof(inputs[6].value));
                            sB2 = atof(inputs[7].value); sm2 = (int)round(atof(inputs[8].value));
                            sC2 = atof(inputs[9].value);

                            if (sn1==0 || sn2==0 || sm1==0 || sm2==0) {
                                hasError = 1;
                                strcpy(errorMsg, "Exponents (n1, n2, m1, m2) cannot be zero.");
                            } else if (fabs(sA1) < 1e-14) {
                                hasError = 1;
                                strcpy(errorMsg, "A1 cannot be zero (x cannot be isolated from Eq1).");
                            } else if (fabs(sB2) < 1e-14) {
                                hasError = 1;
                                strcpy(errorMsg, "B2 cannot be zero.");
                            } else {
                                double yLo = NAN, yHi = NAN;
                                double prevY = -50.0;
                                double prevF = fSubEq(-50.0, sA1,sn1,sB1,sn2,sC1,
                                                              sA2,sm1,sB2,sm2,sC2);

                                for (double ys = -49.99; ys <= 50.0; ys += 0.01) {
                                    double fv = fSubEq(ys, sA1,sn1,sB1,sn2,sC1,
                                                            sA2,sm1,sB2,sm2,sC2);
                                    if (isnan(fv) || isnan(prevF)) {
                                        prevF = fv; prevY = ys; continue;
                                    }
                                    if (prevF * fv < 0) {
                                        yLo = prevY; yHi = ys; break;
                                    }
                                    if (fabs(fv) < 1e-10) {
                                        yLo = ys - 0.005; yHi = ys + 0.005; break;
                                    }
                                    prevF = fv; prevY = ys;
                                }

                            if (!isnan(yLo) && !isnan(yHi)) {
                                for (int b = 0; b < 100; b++) {
                                    double yMid = (yLo + yHi) / 2.0;
                                    double fMid = fSubEq(yMid, sA1,sn1,sB1,sn2,sC1,
                                                                sA2,sm1,sB2,sm2,sC2);
                                    if (isnan(fMid)) break;
                                    double fL = fSubEq(yLo, sA1,sn1,sB1,sn2,sC1,
                                                             sA2,sm1,sB2,sm2,sC2);
                                    if (isnan(fL)) break;
                                    if (fL * fMid < 0) yHi = yMid;
                                    else               yLo = yMid;
                                    if (fabs(yHi - yLo) < 1e-12) break;
                                }
                                solY = (yLo + yHi) / 2.0;
                                solX = xFromY(solY, sA1,sn1,sB1,sn2,sC1);

                                if (!isnan(solX) && !isnan(solY)) {
                                    step4_byN2  = sB1 * ipow(solY, sn2);
                                    step4_inner = (sC1 - step4_byN2) / sA1;
                                    hasSol = 1;
                                } else {
                                    hasError = 1;
                                    strcpy(errorMsg, "Domain error during back-substitution.");
                                }
                            } else {
                                hasError = 1;
                                strcpy(errorMsg, "No solution found in search range. Try different values.");
                            }
                        }
                        }

                        if (mx >= btnClear.rect.x && mx < btnClear.rect.x+btnClear.rect.w &&
                            my >= btnClear.rect.y && my < btnClear.rect.y+btnClear.rect.h) {
                            showConfirm = 1;
                        }
                    }
                }

                if (ev.type == SDL_MOUSEBUTTONUP) {
                    btnCompute.clicked = 0;
                    btnClear.clicked   = 0;
                    btnSolverBack.clicked = 0;
                }

                if (ev.type == SDL_MOUSEMOTION) {
                    int mx = ev.motion.x, my = ev.motion.y;
#define HOVER(b) ((b).hovered = (mx>=(b).rect.x && mx<(b).rect.x+(b).rect.w && \
                                  my>=(b).rect.y && my<(b).rect.y+(b).rect.h))
                    HOVER(btnCompute); HOVER(btnClear);
                    HOVER(btnSolverBack);
                    HOVER(btnConfirmYes); HOVER(btnConfirmNo);
                }

                if (ev.type == SDL_TEXTINPUT && activeInput >= 0 && !showConfirm) {
                    char c = ev.text.text[0];
                    int isExp = (activeInput==1||activeInput==3||activeInput==6||activeInput==8);
                    int valid = (c>='0'&&c<='9') || (!isExp && c=='.') || (c=='-');
                    if (valid) {
                        int len = strlen(inputs[activeInput].value);
                        if (len < 18) {
                            inputs[activeInput].value[len]   = c;
                            inputs[activeInput].value[len+1] = '\0';
                        }
                    }
                }

                if (ev.type == SDL_KEYDOWN && activeInput >= 0 && !showConfirm) {
                    if (ev.key.keysym.sym == SDLK_BACKSPACE) {
                        int len = strlen(inputs[activeInput].value);
                        if (len > 0) inputs[activeInput].value[len-1] = '\0';
                    }
                    if (ev.key.keysym.sym == SDLK_TAB) {
                        inputs[activeInput].active = 0;
                        activeInput = (activeInput+1) % 10;
                        inputs[activeInput].active = 1;
                    }
                }
            }
        }

        if (screen == SCREEN_LOADING) {
            Uint32 elapsed = SDL_GetTicks() - loadStart;
            if (elapsed >= 5000) screen = SCREEN_SOLVER;
        }

        if (screen == SCREEN_LANDING) {
            SDL_SetRenderDrawColor(renderer, 255, 220, 230, 255);
            SDL_RenderClear(renderer);

            int cx = WIN_W / 2;
            SDL_Color black = {30, 30, 30, 255};
            SDL_Color gray  = {120, 120, 120, 255};

            renderCenterBold(renderer, fHuge, "WELCOME TO SYSTEM OF EQUATIONS PROGRAM",
                             cx, 65, black);
            renderCenterBold(renderer, fHuge, "SOLVER", cx, 115, black);

            renderCenterText(renderer, fLarge,
                             "A solver program that makes you life easier",
                             cx, 180, gray);

            /* Hello Kitty - left side */
            drawHelloKitty(renderer, 140, 500, 65);
            /* Hello Kitty - right side */
            drawHelloKitty(renderer, WIN_W-140, 500, 65);

            {
                SDL_Color bg = hoverSolver ? (SDL_Color){185,185,185,255}
                                           : (SDL_Color){168,168,168,255};
                SDL_SetRenderDrawColor(renderer, 130,130,130,100);
                SDL_Rect sh = {cardSolver.x+6, cardSolver.y+6,
                               cardSolver.w, cardSolver.h};
                SDL_RenderFillRect(renderer, &sh);
                SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
                SDL_RenderFillRect(renderer, &cardSolver);
                SDL_SetRenderDrawColor(renderer, 150,150,150,255);
                SDL_RenderDrawRect(renderer, &cardSolver);

                int ix = cardSolver.x + cardSolver.w/2 - 25;
                int iy = cardSolver.y + 80;
                SDL_Color gc[3][3] = {
                    {{232,117,17,255}, {76,175,80,255}, {33,150,243,255}},
                    {{244,67,54,255},  {156,39,176,255},{0,188,212,255}},
                    {{255,193,7,255},  {121,85,72,255}, {233,30,99,255}}
                };
                for (int r = 0; r < 3; r++)
                    for (int c = 0; c < 3; c++) {
                        SDL_SetRenderDrawColor(renderer,
                            gc[r][c].r, gc[r][c].g, gc[r][c].b, 255);
                        SDL_Rect cell = {ix + c*18, iy + r*18, 15, 15};
                        SDL_RenderFillRect(renderer, &cell);
                    }

                renderCenterBold(renderer, fBig, "SOLVER",
                    cardSolver.x + cardSolver.w/2, cardSolver.y + 175,
                    (SDL_Color){50,50,50,255});
            }

            {
                SDL_Color bg = hoverAbout ? (SDL_Color){195,205,232,255}
                                          : (SDL_Color){178,190,218,255};
                SDL_SetRenderDrawColor(renderer, 140,150,180,100);
                SDL_Rect sh = {cardAbout.x+6, cardAbout.y+6,
                               cardAbout.w, cardAbout.h};
                SDL_RenderFillRect(renderer, &sh);
                SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
                SDL_RenderFillRect(renderer, &cardAbout);
                SDL_SetRenderDrawColor(renderer, 160,170,200,255);
                SDL_RenderDrawRect(renderer, &cardAbout);

                int ix = cardAbout.x + cardAbout.w/2 - 20;
                int iy = cardAbout.y + 75;
                SDL_SetRenderDrawColor(renderer, 33, 150, 243, 255);
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

                renderCenterBold(renderer, fBig, "ABOUT US",
                    cardAbout.x + cardAbout.w/2, cardAbout.y + 155,
                    (SDL_Color){40,40,65,255});
                renderCenterBold(renderer, fBig, "(INFO)",
                    cardAbout.x + cardAbout.w/2, cardAbout.y + 190,
                    (SDL_Color){40,40,65,255});
            }
        }

        else if (screen == SCREEN_ABOUT) {
            SDL_SetRenderDrawColor(renderer, 255, 220, 230, 255);
            SDL_RenderClear(renderer);

            int cx = WIN_W / 2;
            SDL_Color dark   = {0, 60, 60, 255};
            SDL_Color teal   = {0, 120, 112, 255};
            SDL_Color accent = {0, 100, 90, 255};
            SDL_Color line   = {120, 195, 188, 255};

            renderButton(renderer, fNorm, &btnBack);
            renderCenterBold(renderer, fHuge, "ABOUT US", cx, 50, dark);

            drawPanel(renderer, 250, 110, 1100, 710,
                      (SDL_Color){248,255,254,255}, line);

            renderCenterBold(renderer, fBig, "SYSTEM OF EQUATION", cx, 140, dark);
            renderCenterBold(renderer, fLarge,
                "NON LINEAR USING SUBSTITUTION METHOD", cx, 178, dark);

            SDL_SetRenderDrawColor(renderer, line.r, line.g, line.b, 255);
            SDL_RenderDrawLine(renderer, 350, 218, 1250, 218);

            renderCenterBold(renderer, fTitle,
                "NUMERICAL METHOD  |  MT 221", cx, 238, teal);
            renderCenterBold(renderer, fLarge, "A FINAL PROJECT", cx, 275, accent);
            renderCenterBold(renderer, fLarge, "BSCPE 22003", cx, 305, accent);

            SDL_SetRenderDrawColor(renderer, line.r, line.g, line.b, 255);
            SDL_RenderDrawLine(renderer, 350, 340, 1250, 340);

            renderCenterBold(renderer, fTitle, "PROGRAM CREATED BY:", cx, 360, dark);
            renderCenterBold(renderer, fBig, "FULLA, JOHN MICHEAL", cx, 405, teal);
            renderCenterBold(renderer, fBig, "SORIANO, GILBERT", cx, 445, teal);

            SDL_SetRenderDrawColor(renderer, line.r, line.g, line.b, 255);
            SDL_RenderDrawLine(renderer, 350, 490, 1250, 490);

            SDL_Color desc = {0, 80, 75, 255};
            renderCenterText(renderer, fNorm,
                "This program solves a system of non-linear equations with two unknowns",
                cx, 515, desc);
            renderCenterText(renderer, fNorm,
                "using the Substitution Method. The user inputs coefficients and exponents",
                cx, 540, desc);
            renderCenterText(renderer, fNorm,
                "for two equations of the form Ax^n + By^m = C, and the program isolates",
                cx, 565, desc);
            renderCenterText(renderer, fNorm,
                "one variable, substitutes into the other equation, and solves step by step.",
                cx, 590, desc);
            renderCenterText(renderer, fNorm,
                "The solution is verified by checking both original equations.",
                cx, 625, desc);

            renderCenterText(renderer, fSmall,
                "Built with SDL2 and C  |  2026",
                cx, 680, (SDL_Color){100,160,155,255});

            SDL_SetRenderDrawColor(renderer, 0, 120, 112, 255);
            SDL_Rect bar = {250, 790, 1100, 6};
            SDL_RenderFillRect(renderer, &bar);
        }

        else if (screen == SCREEN_LOADING) {
            SDL_SetRenderDrawColor(renderer, 255, 220, 230, 255);
            SDL_RenderClear(renderer);

            int cx = WIN_W / 2;
            Uint32 elapsed = SDL_GetTicks() - loadStart;
            float progress = (float)elapsed / 5000.0f;
            if (progress > 1.0f) progress = 1.0f;

            SDL_Color black = {30, 30, 30, 255};

            renderCenterBold(renderer, fBig, "SYSTEM OF EQUATION",    cx, 100, black);
            renderCenterBold(renderer, fBig, "NON LINEAR USING",      cx, 140, black);
            renderCenterBold(renderer, fBig, "SUBSTITUTION METHOD",   cx, 180, black);

            renderCenterBold(renderer, fBig, "NUMERICAL METHOD",      cx, 260, black);
            renderCenterBold(renderer, fBig, "MT 221",                cx, 300, black);
            renderCenterBold(renderer, fBig, "A FINAL PROJECT",       cx, 340, black);

            renderCenterBold(renderer, fBig, "PROGRAM CREATED BY:",   cx, 420, black);
            renderCenterBold(renderer, fBig, "FULLA, JOHN MICHEAL",   cx, 460, black);
            renderCenterBold(renderer, fBig, "SORIANO, GILBERT",      cx, 500, black);

            renderCenterText(renderer, fLarge, "Loading Program", cx, 580, black);

            /* Hello Kitty on loading screen - sides, no overlap with centered text */
            drawHelloKitty(renderer, 155, 400, 90);
            drawHelloKitty(renderer, WIN_W - 155, 400, 90);

            int barW = 420, barH = 42;
            int barX = cx - barW/2, barY = 620;

            SDL_SetRenderDrawColor(renderer, 35, 35, 35, 255);
            SDL_Rect outer = {barX - 6, barY - 6, barW + 12, barH + 12};
            SDL_RenderFillRect(renderer, &outer);

            SDL_SetRenderDrawColor(renderer, 55, 55, 55, 255);
            SDL_Rect inner = {barX, barY, barW, barH};
            SDL_RenderFillRect(renderer, &inner);

            int fillW = (int)(barW * progress);
            for (int px = 0; px < fillW; px++) {
                float t = (float)px / (float)barW;
                int rv = (int)(55  + t * 150);
                int gv = (int)(210 + t *  45);
                int bv = (int)(185 + t *  55);
                SDL_SetRenderDrawColor(renderer, rv, gv, bv, 255);
                SDL_RenderDrawLine(renderer, barX + px, barY,
                                             barX + px, barY + barH - 1);
            }

            char pct[8];
            sprintf(pct, "%d%%", (int)(progress * 100));
            renderCenterBold(renderer, fLarge, pct, cx, barY + 9,
                             (SDL_Color){255,255,255,255});
        }

        else if (screen == SCREEN_SOLVER) {
            SDL_SetRenderDrawColor(renderer, 255, 220, 230, 255);
            SDL_RenderClear(renderer);

            SDL_Color white   = {255,255,255,255};
            SDL_Color cream   = {255,220,235,255};
            SDL_Color secCol  = {160, 30, 70, 255};
            SDL_Color darkTxt = {100, 20, 50, 255};
            SDL_Color hintCol = {180, 100,130, 255};
            SDL_Color panelBg = {255, 240,248, 255};
            SDL_Color panBdr  = {220, 140,170, 255};
            SDL_Color eqBg    = {255, 230,240, 255};
            SDL_Color eqBdr   = {210, 150,175, 255};

            for (int i = 0; i < 80; i++) {
                int v = (i<5)?(i*8):(i>75?((79-i)*10):0);
                SDL_SetRenderDrawColor(renderer, 180+v/4, 30+v/4, 70+v/4, 255);
                SDL_RenderDrawLine(renderer, 0, i, WIN_W, i);
            }
            renderBold(renderer, fTitle, "SUBSTITUTION METHOD", 30, 14, white);
            renderText(renderer, fLarge, "Non-Linear Equations  |  2 Unknowns", 30, 46, cream);
            renderText(renderer, fSmall, "MT221 - Numerical Methods  |  Semestral Project",
                       1030, 10, cream);
            renderText(renderer, fNorm,  "BSCPE 22003", 1195, 32, white);
            renderText(renderer, fSmall, "Soriano, Gilbert  |  Fulla, John Micheal",
                       1115, 54, cream);
            btnSolverBack.rect = (SDL_Rect){WIN_W-200, 16, 170, 44};
            renderButton(renderer, fNorm, &btnSolverBack);

            drawPanel(renderer, 14, 88, 495, 835, panelBg, panBdr);
            renderBold(renderer, fLarge, "INPUT COEFFICIENTS", 85, 100, secCol);

            drawPanel(renderer, 30, 126, 462, 78, eqBg, eqBdr);
            renderText(renderer, fSmall, "General Form:", 44, 132, hintCol);
            {
                int tx = 44, ty = 151;
                SDL_Color ec = {0, 105, 100, 255};
                tx = renderWithSub(renderer, fMed, fSub, "A", "1", tx, ty, ec);
                tx = renderWithSup(renderer, fMed, fSup, "x", "n1", tx, ty, ec);
                renderText(renderer, fMed, " + ", tx, ty, ec); tx += textW(fMed," + ");
                tx = renderWithSub(renderer, fMed, fSub, "B", "1", tx, ty, ec);
                tx = renderWithSup(renderer, fMed, fSup, "y", "n2", tx, ty, ec);
                renderText(renderer, fMed, " = ", tx, ty, ec); tx += textW(fMed," = ");
                tx = renderWithSub(renderer, fMed, fSub, "C", "1", tx, ty, ec);
                renderText(renderer, fMed, "  ,  ", tx, ty, ec); tx += textW(fMed,"  ,  ");
                tx = renderWithSub(renderer, fMed, fSub, "A", "2", tx, ty, ec);
                tx = renderWithSup(renderer, fMed, fSup, "x", "m1", tx, ty, ec);
                renderText(renderer, fMed, " + ", tx, ty, ec); tx += textW(fMed," + ");
                tx = renderWithSub(renderer, fMed, fSub, "B", "2", tx, ty, ec);
                tx = renderWithSup(renderer, fMed, fSup, "y", "m2", tx, ty, ec);
                renderText(renderer, fMed, " = ", tx, ty, ec); tx += textW(fMed," = ");
                renderWithSub(renderer, fMed, fSub, "C", "2", tx, ty, ec);
            }

            drawPanel(renderer, 30, 215, 462, 108, eqBg, eqBdr);
            renderBold(renderer, fNorm, "EQUATION 1", 193, 220, secCol);
            int boxW = 60, boxH = 36;
            int bx[5] = {40, 122, 210, 290, 378};
            for (int i = 0; i < 5; i++)
                inputs[i].rect = (SDL_Rect){bx[i], 258, boxW, boxH};
            renderText(renderer, fSmall, "x^", 103, 268, darkTxt);
            renderText(renderer, fNorm,  "+",  188, 265, darkTxt);
            renderText(renderer, fSmall, "y^", 273, 268, darkTxt);
            renderText(renderer, fNorm,  "=",  356, 265, darkTxt);

            drawPanel(renderer, 30, 333, 462, 108, eqBg, eqBdr);
            renderBold(renderer, fNorm, "EQUATION 2", 193, 338, secCol);
            for (int i = 0; i < 5; i++)
                inputs[5+i].rect = (SDL_Rect){bx[i], 376, boxW, boxH};
            renderText(renderer, fSmall, "x^", 103, 386, darkTxt);
            renderText(renderer, fNorm,  "+",  188, 383, darkTxt);
            renderText(renderer, fSmall, "y^", 273, 386, darkTxt);
            renderText(renderer, fNorm,  "=",  356, 383, darkTxt);

            for (int i = 0; i < 10; i++) renderInputBox(renderer, fSmall, &inputs[i]);

            drawPanel(renderer, 30, 455, 462, 78, eqBg, eqBdr);
            renderBold(renderer, fNorm, "EQUATION PREVIEW", 158, 459, secCol);
            double pA1=atof(inputs[0].value); int pn1=strlen(inputs[1].value)?atoi(inputs[1].value):0;
            double pB1=atof(inputs[2].value); int pn2=strlen(inputs[3].value)?atoi(inputs[3].value):0;
            double pC1=atof(inputs[4].value);
            double pA2=atof(inputs[5].value); int pm1=strlen(inputs[6].value)?atoi(inputs[6].value):0;
            double pB2=atof(inputs[7].value); int pm2=strlen(inputs[8].value)?atoi(inputs[8].value):0;
            double pC2=atof(inputs[9].value);
            SDL_Color pvCol = {0, 110, 105, 255};
            {
                int tx = 40, ty = 476; char nb[20];
                sprintf(nb,"%.0f",pA1); renderBold(renderer,fNorm,nb,tx,ty,pvCol); tx+=textW(fNorm,nb);
                renderText(renderer,fNorm,"x",tx,ty,pvCol); tx+=textW(fNorm,"x");
                if (pn1!=1 && strlen(inputs[1].value)>0) {
                    char es[8]; sprintf(es,"%d",pn1);
                    renderText(renderer,fSup,es,tx,ty-6,pvCol); tx+=textW(fSup,es)+1;
                }
                renderText(renderer,fNorm," + ",tx,ty,pvCol); tx+=textW(fNorm," + ");
                sprintf(nb,"%.0f",pB1); renderBold(renderer,fNorm,nb,tx,ty,pvCol); tx+=textW(fNorm,nb);
                renderText(renderer,fNorm,"y",tx,ty,pvCol); tx+=textW(fNorm,"y");
                if (pn2!=1 && strlen(inputs[3].value)>0) {
                    char es[8]; sprintf(es,"%d",pn2);
                    renderText(renderer,fSup,es,tx,ty-6,pvCol); tx+=textW(fSup,es)+1;
                }
                char rest[20]; sprintf(rest," = %.0f",pC1);
                renderText(renderer,fNorm,rest,tx,ty,pvCol);

                tx = 40; ty = 494;
                sprintf(nb,"%.0f",pA2); renderBold(renderer,fNorm,nb,tx,ty,pvCol); tx+=textW(fNorm,nb);
                renderText(renderer,fNorm,"x",tx,ty,pvCol); tx+=textW(fNorm,"x");
                if (pm1!=1 && strlen(inputs[6].value)>0) {
                    char es[8]; sprintf(es,"%d",pm1);
                    renderText(renderer,fSup,es,tx,ty-6,pvCol); tx+=textW(fSup,es)+1;
                }
                renderText(renderer,fNorm," + ",tx,ty,pvCol); tx+=textW(fNorm," + ");
                sprintf(nb,"%.0f",pB2); renderBold(renderer,fNorm,nb,tx,ty,pvCol); tx+=textW(fNorm,nb);
                renderText(renderer,fNorm,"y",tx,ty,pvCol); tx+=textW(fNorm,"y");
                if (pm2!=1 && strlen(inputs[8].value)>0) {
                    char es[8]; sprintf(es,"%d",pm2);
                    renderText(renderer,fSup,es,tx,ty-6,pvCol); tx+=textW(fSup,es)+1;
                }
                sprintf(rest," = %.0f",pC2);
                renderText(renderer,fNorm,rest,tx,ty,pvCol);
            }

            renderButton(renderer, fNorm, &btnCompute);
            renderButton(renderer, fNorm, &btnClear);

            if (hasError && strlen(errorMsg) > 0) {
                drawPanel(renderer, 30, 616, 462, 36,
                          (SDL_Color){255,235,230,255}, (SDL_Color){200,100,90,255});
                renderText(renderer, fSmall, errorMsg, 42, 624, (SDL_Color){180,30,20,255});
            }

            if (hasSol) {
                drawPanel(renderer, 30, 668, 462, 92,
                          (SDL_Color){212,255,238,255}, (SDL_Color){80,180,140,255});
                renderBold(renderer, fMed, "FINAL ANSWER", 175, 674, (SDL_Color){0,100,58,255});
                char buf[80];
                sprintf(buf,"x = %.3f", solX);
                renderBold(renderer, fLarge, buf, 48, 698, (SDL_Color){0,120,78,255});
                sprintf(buf,"y = %.3f", solY);
                renderBold(renderer, fLarge, buf, 268, 698, (SDL_Color){0,120,78,255});
                double v1 = sA1*ipow(solX,sn1)+sB1*ipow(solY,sn2);
                double v2 = sA2*ipow(solX,sm1)+sB2*ipow(solY,sm2);
                sprintf(buf,"Check  Eq1: %.4f = %.0f  |  Eq2: %.4f = %.0f", v1,sC1, v2,sC2);
                renderText(renderer, fSmall, buf, 40, 738, (SDL_Color){0,100,68,255});
            }

            drawPanel(renderer, 520, 88, 555, 835, panelBg, panBdr);
            renderBold(renderer, fLarge, "SOLUTION STEPS", 688, 100, secCol);

            if (hasSol) {
                int sy = 125;
                char sbuf[200];

                drawPanel(renderer, 534, sy, 526, 75, (SDL_Color){225,248,246,255}, eqBdr);
                renderBold(renderer, fMed, "GIVEN: System of Equations", 548, sy+5, secCol);
                SDL_SetRenderDrawColor(renderer, eqBdr.r,eqBdr.g,eqBdr.b,255);
                SDL_RenderDrawLine(renderer, 548, sy+25, 1050, sy+25);
                sprintf(sbuf,"Eq1:  %.0fx^%d + %.0fy^%d = %.0f", sA1,sn1,sB1,sn2,sC1);
                renderText(renderer,fNorm,sbuf,558,sy+30,(SDL_Color){0,140,130,255});
                sprintf(sbuf,"Eq2:  %.0fx^%d + %.0fy^%d = %.0f", sA2,sm1,sB2,sm2,sC2);
                renderText(renderer,fNorm,sbuf,558,sy+50,(SDL_Color){200,110,0,255});
                sy += 85;

                drawPanel(renderer, 534, sy, 526, 90, (SDL_Color){230,252,248,255}, eqBdr);
                renderBold(renderer,fMed,"STEP 1: Isolate x from Equation 1",548,sy+5,secCol);
                SDL_SetRenderDrawColor(renderer,eqBdr.r,eqBdr.g,eqBdr.b,255);
                SDL_RenderDrawLine(renderer,548,sy+25,1050,sy+25);
                sprintf(sbuf,"%.0fx^%d = %.0f - %.0fy^%d", sA1,sn1,sC1,sB1,sn2);
                renderText(renderer,fNorm,sbuf,558,sy+30,darkTxt);
                sprintf(sbuf,"x^%d = (%.0f - %.0fy^%d) / %.0f", sn1,sC1,sB1,sn2,sA1);
                renderText(renderer,fNorm,sbuf,558,sy+50,darkTxt);
                sprintf(sbuf,"x = [(%.0f - %.0fy^%d) / %.0f]^(1/%d)",
                        sC1,sB1,sn2,sA1,sn1);
                renderBold(renderer,fSmall,sbuf,558,sy+70,(SDL_Color){0,120,112,255});
                sy += 100;

                drawPanel(renderer,534,sy,526,90,(SDL_Color){230,248,244,255},eqBdr);
                renderBold(renderer,fMed,"STEP 2: Substitute x into Equation 2",548,sy+5,secCol);
                SDL_SetRenderDrawColor(renderer,eqBdr.r,eqBdr.g,eqBdr.b,255);
                SDL_RenderDrawLine(renderer,548,sy+25,1050,sy+25);
                renderText(renderer,fNorm,"Replace x in Eq2 with the expression from Step 1:",558,sy+30,darkTxt);
                sprintf(sbuf,"%.0f * {[(%.0f - %.0fy^%d)/%.0f]^(1/%d)}^%d + %.0fy^%d = %.0f",
                        sA2,sC1,sB1,sn2,sA1,sn1,sm1, sB2,sm2,sC2);
                renderBold(renderer,fSmall,sbuf,558,sy+50,(SDL_Color){180,90,0,255});
                renderText(renderer,fSmall,"This gives a single equation in y only.",558,sy+70,hintCol);
                sy += 100;

                drawPanel(renderer,534,sy,526,70,(SDL_Color){228,242,255,255},
                          (SDL_Color){110,145,210,255});
                renderBold(renderer,fMed,"STEP 3: Solve for y",548,sy+5,
                           (SDL_Color){30,50,160,255});
                SDL_SetRenderDrawColor(renderer,110,145,210,255);
                SDL_RenderDrawLine(renderer,548,sy+25,1050,sy+25);
                renderText(renderer,fNorm,"Solving the substituted equation for y:",558,sy+30,darkTxt);
                sprintf(sbuf,"y = %.6f", solY);
                renderBold(renderer,fNorm,sbuf,558,sy+50,(SDL_Color){30,50,160,255});
                sy += 80;

                drawPanel(renderer,534,sy,526,110,(SDL_Color){230,252,248,255},eqBdr);
                renderBold(renderer,fMed,"STEP 4: Back-substitute y to find x",548,sy+5,secCol);
                SDL_SetRenderDrawColor(renderer,eqBdr.r,eqBdr.g,eqBdr.b,255);
                SDL_RenderDrawLine(renderer,548,sy+25,1050,sy+25);
                sprintf(sbuf,"x = [(%.0f - %.0f*(%.6f)^%d) / %.0f]^(1/%d)",
                        sC1,sB1,solY,sn2,sA1,sn1);
                renderText(renderer,fSmall,sbuf,558,sy+30,darkTxt);
                sprintf(sbuf,"x = [(%.0f - %.6f) / %.0f]^(1/%d)",
                        sC1, step4_byN2, sA1, sn1);
                renderText(renderer,fSmall,sbuf,558,sy+48,darkTxt);
                sprintf(sbuf,"x = [%.6f]^(1/%d)", step4_inner, sn1);
                renderText(renderer,fSmall,sbuf,558,sy+66,darkTxt);
                sprintf(sbuf,"x = %.6f", solX);
                renderBold(renderer,fNorm,sbuf,558,sy+86,(SDL_Color){0,120,112,255});
                sy += 120;

                drawPanel(renderer,534,sy,526,50,
                          (SDL_Color){215,255,238,255},(SDL_Color){88,185,142,255});
                renderBold(renderer,fMed,"SOLUTION",548,sy+5,(SDL_Color){0,100,58,255});
                SDL_SetRenderDrawColor(renderer,88,185,142,255);
                SDL_RenderDrawLine(renderer,548,sy+25,1050,sy+25);
                sprintf(sbuf,"x = %.3f       y = %.3f", solX, solY);
                renderBold(renderer,fMed,sbuf,600,sy+30,(SDL_Color){0,110,65,255});
                sy += 60;

                drawPanel(renderer,534,sy,526,100,
                          (SDL_Color){238,232,255,255},(SDL_Color){138,118,210,255});
                renderBold(renderer,fMed,"CHECKING",548,sy+5,(SDL_Color){80,48,170,255});
                SDL_SetRenderDrawColor(renderer,138,118,210,255);
                SDL_RenderDrawLine(renderer,548,sy+25,1050,sy+25);

                double chk1 = sA1*ipow(solX,sn1)+sB1*ipow(solY,sn2);
                double chk2 = sA2*ipow(solX,sm1)+sB2*ipow(solY,sm2);
                int ok1 = fabs(chk1 - sC1) < 0.05;
                int ok2 = fabs(chk2 - sC2) < 0.05;

                sprintf(sbuf,"Eq1:  %.0f*(%.3f)^%d + %.0f*(%.3f)^%d  =  %.4f",
                        sA1,solX,sn1, sB1,solY,sn2, chk1);
                renderText(renderer,fSmall,sbuf,558,sy+30,darkTxt);
                sprintf(sbuf,"Expected: %.0f     %s", sC1, ok1 ? "PASS" : "FAIL");
                renderText(renderer,fSmall,sbuf,558,sy+47,
                           ok1?(SDL_Color){0,128,60,255}:(SDL_Color){200,30,30,255});

                sprintf(sbuf,"Eq2:  %.0f*(%.3f)^%d + %.0f*(%.3f)^%d  =  %.4f",
                        sA2,solX,sm1, sB2,solY,sm2, chk2);
                renderText(renderer,fSmall,sbuf,558,sy+66,darkTxt);
                sprintf(sbuf,"Expected: %.0f     %s", sC2, ok2 ? "PASS" : "FAIL");
                renderText(renderer,fSmall,sbuf,558,sy+83,
                           ok2?(SDL_Color){0,128,60,255}:(SDL_Color){200,30,30,255});

            } else {
                renderText(renderer,fNorm,"Steps will appear here after pressing COMPUTE.",
                           590,430,hintCol);
            }

            drawPanel(renderer, 1085, 88, 500, 835, panelBg, panBdr);
            renderBold(renderer, fLarge, "GRAPH", 1295, 100, secCol);

            drawGraph(renderer,fSmall,
                      sA1,sn1,sB1,sn2,sC1, sA2,sm1,sB2,sm2,sC2,
                      solX,solY,hasSol);

            int LY = 635;
            drawPanel(renderer,1098,LY,462,88,eqBg,eqBdr);
            renderBold(renderer,fNorm,"LEGEND",1295,LY+5,secCol);
            SDL_SetRenderDrawColor(renderer,0,160,150,255);
            SDL_Rect l1={1115,LY+30,28,4}; SDL_RenderFillRect(renderer,&l1);
            renderText(renderer,fNorm,"Equation 1  (teal)",1153,LY+23,(SDL_Color){0,140,128,255});
            SDL_SetRenderDrawColor(renderer,220,120,0,255);
            SDL_Rect l2={1115,LY+55,28,4}; SDL_RenderFillRect(renderer,&l2);
            renderText(renderer,fNorm,"Equation 2  (orange)",1153,LY+48,(SDL_Color){180,100,0,255});
            SDL_SetRenderDrawColor(renderer,220,50,50,255);
            for (int di=-5;di<=5;di++) for (int dj=-5;dj<=5;dj++)
                if (di*di+dj*dj<=25)
                    SDL_RenderDrawPoint(renderer,1127+di,LY+76+dj);
            renderText(renderer,fNorm,"Solution Point",1153,LY+70,(SDL_Color){155,20,20,255});

            /* Hello Kitty mascot in solver - bottom right corner */
            drawHelloKitty(renderer, 1530, 855, 40);

            /* ---- CLEAR CONFIRMATION MODAL ---- */
            if (showConfirm) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 130);
                SDL_Rect overlay = {0, 0, WIN_W, WIN_H};
                SDL_RenderFillRect(renderer, &overlay);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

                int mx2 = WIN_W/2, my2 = WIN_H/2 - 30;
                drawPanel(renderer, mx2-240, my2-110, 480, 230,
                          (SDL_Color){255,240,248,255}, (SDL_Color){220,100,140,255});
                SDL_SetRenderDrawColor(renderer, 210, 80, 120, 255);
                SDL_Rect mhdr = {mx2-240, my2-110, 480, 40};
                SDL_RenderFillRect(renderer, &mhdr);
                renderCenterBold(renderer, fLarge, "Confirm Clear",
                                 mx2, my2-102, (SDL_Color){255,255,255,255});

                drawHelloKitty(renderer, mx2, my2-42, 22);

                renderCenterText(renderer, fNorm, "Are you sure you want to clear",
                                 mx2, my2+5, (SDL_Color){80,20,50,255});
                renderCenterText(renderer, fNorm, "all inputs and reset the equations?",
                                 mx2, my2+27, (SDL_Color){80,20,50,255});
                renderButton(renderer, fNorm, &btnConfirmYes);
                renderButton(renderer, fNorm, &btnConfirmNo);
            }

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
