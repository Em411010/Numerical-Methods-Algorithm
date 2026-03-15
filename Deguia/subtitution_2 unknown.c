/*
 * Substitution Method — Linear Equations with 2 Unknowns
 * MT221 - Numerical Methods | Semestral Project
 * BSCPE 22003
 * Allen Jay De Guia

 */

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WIN_W    1400
#define WIN_H    800
#define NUM_PETALS 48

/* ── Screen IDs ──────────────────────────────────────── */
#define SCREEN_LANDING  0
#define SCREEN_LOADING  1
#define SCREEN_SOLVER   2

/* ── Data Structures ─────────────────────────────────── */
typedef struct {
    SDL_Rect rect;
    char     label[64];
    char     value[32];
    int      active;
} InputBox;

typedef struct {
    SDL_Rect rect;
    char     text[40];
    int      hovered, clicked;
} Button;

typedef struct {
    float x, y;
    float vy, vx;
    float size;
    float wobble;       /* phase for side-sway */
    float wobbleSpeed;
    Uint8 alpha;
    int   type;         /* 0 = small circle  1 = 5-petal flower */
} Petal;

/* ── Globals (results & steps) ───────────────────────── */
static double g_a1=0,g_b1=0,g_c1=0, g_a2=0,g_b2=0,g_c2=0;
static double g_solX=0, g_solY=0;
static double g_det=0;
static double g_expr_coeff_y=0, g_expr_const=0; /* y coeff & const after simplification */
static double g_verify1=0, g_verify2=0;
static int    g_hasSolution=0, g_hasSteps=0, g_specialCase=0;
/* 0=none  1=det=0 no solution  2=det=0 infinite  3=a1=0 swapped */

/* ══════════════════════════════════════════════════════
   HELPER — TEXT & DRAWING
   ══════════════════════════════════════════════════════ */
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
    renderText(r, f, t, x,   y, c);
    renderText(r, f, t, x+1, y, c);
}

static int textW(TTF_Font* f, const char* t) {
    int w = 0; TTF_SizeUTF8(f, t, &w, NULL); return w;
}

static void renderCenter(SDL_Renderer* r, TTF_Font* f, const char* t,
                         int cx, int y, SDL_Color c) {
    renderText(r, f, t, cx - textW(f,t)/2, y, c);
}

static void renderCenterBold(SDL_Renderer* r, TTF_Font* f, const char* t,
                              int cx, int y, SDL_Color c) {
    renderBold(r, f, t, cx - textW(f,t)/2, y, c);
}

static void drawPanel(SDL_Renderer* r, int x, int y, int w, int h,
                      SDL_Color bg, SDL_Color bdr) {
    SDL_SetRenderDrawColor(r, bg.r, bg.g, bg.b, bg.a);
    SDL_Rect rc = {x, y, w, h}; SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, bdr.r, bdr.g, bdr.b, bdr.a);
    SDL_RenderDrawRect(r, &rc);
}

static void drawCircleFill(SDL_Renderer* r, int cx, int cy, int rad) {
    for (int dy = -rad; dy <= rad; dy++) {
        int dx = (int)sqrt((double)(rad*rad - dy*dy));
        SDL_RenderDrawLine(r, cx-dx, cy+dy, cx+dx, cy+dy);
    }
}

static void renderInputBox(SDL_Renderer* r, TTF_Font* fLbl, TTF_Font* fVal, InputBox* b) {
    SDL_Color lc = {185, 45, 85, 255};
    renderBold(r, fLbl, b->label, b->rect.x, b->rect.y - 18, lc);
    SDL_Color bgC = b->active ? (SDL_Color){255,232,240,255} : (SDL_Color){255,245,250,255};
    SDL_Color bdC = b->active ? (SDL_Color){215,80,115,255}  : (SDL_Color){210,150,175,255};
    SDL_SetRenderDrawColor(r, bgC.r, bgC.g, bgC.b, 255); SDL_RenderFillRect(r, &b->rect);
    SDL_SetRenderDrawColor(r, bdC.r, bdC.g, bdC.b, 255); SDL_RenderDrawRect(r, &b->rect);
    if (b->active) {
        SDL_Rect inn = {b->rect.x+1, b->rect.y+1, b->rect.w-2, b->rect.h-2};
        SDL_RenderDrawRect(r, &inn);
    }
    if (b->value[0])
        renderText(r, fVal, b->value, b->rect.x+7, b->rect.y+8, (SDL_Color){140,30,70,255});
}

static void renderButton(SDL_Renderer* r, TTF_Font* f, Button* btn) {
    SDL_Color bg = btn->clicked  ? (SDL_Color){160, 35, 70,255}
                 : btn->hovered  ? (SDL_Color){240,110,145,255}
                                 : (SDL_Color){210, 75,110,255};
    SDL_SetRenderDrawColor(r, 100, 20, 50, 255);
    SDL_Rect sh = {btn->rect.x+3, btn->rect.y+3, btn->rect.w, btn->rect.h};
    SDL_RenderFillRect(r, &sh);
    SDL_SetRenderDrawColor(r, bg.r, bg.g, bg.b, 255); SDL_RenderFillRect(r, &btn->rect);
    SDL_SetRenderDrawColor(r, 160, 50, 90, 255);       SDL_RenderDrawRect(r, &btn->rect);
    SDL_Surface* s = TTF_RenderUTF8_Blended(f, btn->text, (SDL_Color){255,255,255,255});
    if (s) {
        SDL_Texture* tx = SDL_CreateTextureFromSurface(r, s);
        SDL_Rect tr = {btn->rect.x+(btn->rect.w-s->w)/2,
                       btn->rect.y+(btn->rect.h-s->h)/2, s->w, s->h};
        SDL_RenderCopy(r, tx, NULL, &tr);
        SDL_FreeSurface(s); SDL_DestroyTexture(tx);
    }
}

/* ══════════════════════════════════════════════════════
   CHERRY BLOSSOM BACKGROUND
   ══════════════════════════════════════════════════════ */
static void initPetals(Petal* petals) {
    srand((unsigned)time(NULL));
    for (int i = 0; i < NUM_PETALS; i++) {
        petals[i].x          = (float)(rand() % WIN_W);
        petals[i].y          = (float)(rand() % WIN_H);
        petals[i].vy         = 0.4f + (rand() % 100) / 120.0f;
        petals[i].vx         = -0.3f + (rand() % 100) / 200.0f - 0.25f;
        petals[i].size       = 3.0f + (rand() % 8);
        petals[i].wobble     = (float)(rand() % 628) / 100.0f;
        petals[i].wobbleSpeed= 0.015f + (rand() % 20) / 500.0f;
        petals[i].alpha      = 160 + rand() % 80;
        petals[i].type       = rand() % 3;  /* 0,1 = circle  2 = flower */
    }
}

static void drawPetalShape(SDL_Renderer* r, int px, int py, int sz, int type, Uint8 alpha) {
    /* petal colors: soft rose to blush */
    int ci = sz % 3;
    Uint8 pr = ci==0?255:ci==1?255:250;
    Uint8 pg = ci==0?182:ci==1?200:215;
    Uint8 pb = ci==0?200:ci==1?215:225;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, pr, pg, pb, alpha);

    if (type == 2) {
        /* flower shape — 5 small circles arranged in a + with center */
        int rs = sz/2 < 2 ? 2 : sz/2;
        int offs = sz;
        int pts[6][2] = {{0,0},{offs,0},{-offs,0},{0,offs},{0,-offs},{offs/2,offs/2}};
        for (int p = 0; p < 5; p++) {
            int bx = px + pts[p][0], by = py + pts[p][1];
            for (int dy = -rs; dy <= rs; dy++) {
                int dx2 = (int)sqrt((double)(rs*rs - dy*dy));
                SDL_RenderDrawLine(r, bx-dx2, by+dy, bx+dx2, by+dy);
            }
        }
    } else {
        /* simple filled circle */
        for (int dy = -sz; dy <= sz; dy++) {
            int dx2 = (int)sqrt((double)(sz*sz - dy*dy));
            SDL_RenderDrawLine(r, px-dx2, py+dy, px+dx2, py+dy);
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void drawSakuraBranch(SDL_Renderer* r) {
    /* top-left branch cluster */
    SDL_SetRenderDrawColor(r, 139, 90, 70, 200);
    /* main arm */
    SDL_RenderDrawLine(r, 0, 60, 120, 120);
    SDL_RenderDrawLine(r, 1, 60, 121, 120);
    /* sub branches */
    SDL_RenderDrawLine(r, 80, 100, 50, 70);
    SDL_RenderDrawLine(r, 100, 112, 140, 80);
    SDL_RenderDrawLine(r, 60, 88, 30, 55);

    /* blossom clusters on branch */
    int bpts[][2] = {{50,70},{30,55},{140,80},{80,95},{120,118},{0,55},{160,72}};
    for (int i = 0; i < 7; i++) {
        drawPetalShape(r, bpts[i][0], bpts[i][1], 6, 2, 200);
        drawPetalShape(r, bpts[i][0]+8, bpts[i][1]-6, 4, 0, 180);
    }

    /* top-right branch cluster */
    SDL_SetRenderDrawColor(r, 139, 90, 70, 200);
    SDL_RenderDrawLine(r, WIN_W, 40, WIN_W-130, 100);
    SDL_RenderDrawLine(r, WIN_W-1, 40, WIN_W-131, 100);
    SDL_RenderDrawLine(r, WIN_W-70, 70, WIN_W-40, 45);
    SDL_RenderDrawLine(r, WIN_W-100, 88, WIN_W-150, 60);
    SDL_RenderDrawLine(r, WIN_W-120, 95, WIN_W-80, 50);

    int bpts2[][2] = {{WIN_W-40,45},{WIN_W-130,98},{WIN_W-150,60},{WIN_W-80,72},{WIN_W-10,38},{WIN_W-170,75}};
    for (int i = 0; i < 6; i++) {
        drawPetalShape(r, bpts2[i][0], bpts2[i][1], 6, 2, 200);
        drawPetalShape(r, bpts2[i][0]-7, bpts2[i][1]+5, 4, 0, 180);
    }
}

static void updateAndDrawPetals(SDL_Renderer* r, Petal* petals, float dt) {
    for (int i = 0; i < NUM_PETALS; i++) {
        petals[i].wobble += petals[i].wobbleSpeed;
        petals[i].x += petals[i].vx + (float)sin(petals[i].wobble) * 0.5f;
        petals[i].y += petals[i].vy;
        if (petals[i].y > WIN_H + 20) {
            petals[i].y = -20.0f;
            petals[i].x = (float)(rand() % WIN_W);
        }
        if (petals[i].x < -20) petals[i].x = WIN_W + 10.0f;
        if (petals[i].x > WIN_W + 20) petals[i].x = -10.0f;
        drawPetalShape(r, (int)petals[i].x, (int)petals[i].y,
                       (int)petals[i].size, petals[i].type, petals[i].alpha);
    }
}

static void drawBackground(SDL_Renderer* r) {
    /* Sky gradient — soft pink top to blush-white bottom */
    for (int row = 0; row < WIN_H; row++) {
        float t = (float)row / WIN_H;
        int rv = (int)(255 - t * 8);
        int gv = (int)(220 - t * 18);
        int bv = (int)(230 - t * 10);
        SDL_SetRenderDrawColor(r, rv, gv, bv, 255);
        SDL_RenderDrawLine(r, 0, row, WIN_W, row);
    }
    /* ground strip */
    SDL_SetRenderDrawColor(r, 230, 170, 190, 255);
    SDL_Rect g = {0, WIN_H-40, WIN_W, 40}; SDL_RenderFillRect(r, &g);
    /* subtle ground highlight */
    SDL_SetRenderDrawColor(r, 245, 195, 210, 255);
    SDL_Rect g2 = {0, WIN_H-40, WIN_W, 4}; SDL_RenderFillRect(r, &g2);
}

/* ══════════════════════════════════════════════════════
   GRAPH
   ══════════════════════════════════════════════════════ */
static void drawGraph(SDL_Renderer* r, TTF_Font* fSmall,
                      double a1, double b1, double c1,
                      double a2, double b2, double c2,
                      double solX, double solY, int hasSol,
                      int mouseX, int mouseY) {
    /* GY=256: starts just below the right-panel header bar (y=225+28=253) */
    int GX=703, GY=256, GW=694, GH=529;
    /* inset drawing area — leave room for axis labels on all sides */
    int DX=GX+54, DY=GY+18, DW=GW-74, DH=GH-90;

    drawPanel(r, GX, GY, GW, GH, (SDL_Color){255,245,248,255}, (SDL_Color){230,155,175,255});

    /* grid */
    double scale = 40.0;
    int unitX=DX+DW/2, unitY=DY+DH/2;

    /* ── Clip to data area FIRST so grid lines never escape outside the graph box ── */
    SDL_Rect clip = {DX, DY, DW, DH};
    SDL_RenderSetClipRect(r, &clip);

    /* Grid lines — now safely clipped to data area */
    SDL_SetRenderDrawColor(r, 245, 220, 230, 255);
    for (int gx = unitX % (int)scale; gx < DX+DW; gx += (int)scale)
        SDL_RenderDrawLine(r, gx, DY, gx, DY+DH);
    for (int gy = unitY % (int)scale; gy < DY+DH; gy += (int)scale)
        SDL_RenderDrawLine(r, DX, gy, DX+DW, gy);

    /* Helper: world -> screen */
    #define WX(wx) (unitX + (int)((wx)*scale))
    #define WY(wy) (unitY - (int)((wy)*scale))

    /* Axes */
    SDL_SetRenderDrawColor(r, 180, 80, 120, 200);
    SDL_RenderDrawLine(r, DX, unitY, DX+DW, unitY);
    SDL_RenderDrawLine(r, unitX, DY, unitX, DY+DH);

    /* Equation 1 line — deep sakura */
    /* a1*x + b1*y = c1 => if b1!=0: y=(c1-a1*x)/b1  else: x=c1/a1 */
    SDL_SetRenderDrawColor(r, 215, 60, 100, 255);
    if (fabs(b1) > 1e-10) {
        int prevX=-1, prevY2=-1;
        for (int px = DX; px <= DX+DW; px++) {
            double wx = (double)(px - unitX) / scale;
            double wy = (c1 - a1*wx) / b1;
            int py = WY(wy);
            if (py >= DY && py <= DY+DH) {
                if (prevX >= 0 && abs(py-prevY2) < DH) {
                    SDL_RenderDrawLine(r, prevX, prevY2, px, py);
                    SDL_RenderDrawLine(r, prevX, prevY2+1, px, py+1);
                }
                prevX=px; prevY2=py;
            } else { prevX=-1; }
        }
    } else if (fabs(a1) > 1e-10) {
        int px = WX(c1/a1);
        if (px >= DX && px <= DX+DW)
            SDL_RenderDrawLine(r, px, DY, px, DY+DH);
    }

    /* Equation 2 line — light rose */
    SDL_SetRenderDrawColor(r, 255, 130, 160, 255);
    if (fabs(b2) > 1e-10) {
        int prevX=-1, prevY2=-1;
        for (int px = DX; px <= DX+DW; px++) {
            double wx = (double)(px - unitX) / scale;
            double wy = (c2 - a2*wx) / b2;
            int py = WY(wy);
            if (py >= DY && py <= DY+DH) {
                if (prevX >= 0 && abs(py-prevY2) < DH) {
                    SDL_RenderDrawLine(r, prevX, prevY2, px, py);
                    SDL_RenderDrawLine(r, prevX, prevY2+1, px, py+1);
                }
                prevX=px; prevY2=py;
            } else { prevX=-1; }
        }
    } else if (fabs(a2) > 1e-10) {
        int px = WX(c2/a2);
        if (px >= DX && px <= DX+DW)
            SDL_RenderDrawLine(r, px, DY, px, DY+DH);
    }

    /* Solution point */
    if (hasSol) {
        int spx = WX(solX), spy = WY(solY);
        if (spx >= DX && spx <= DX+DW && spy >= DY && spy <= DY+DH) {
            /* outer glow rings */
            SDL_SetRenderDrawColor(r, 215, 60, 100, 80);
            for (int di=-10;di<=10;di++) for (int dj=-10;dj<=10;dj++)
                if (di*di+dj*dj<=100) SDL_RenderDrawPoint(r, spx+di, spy+dj);
            /* inner dot */
            SDL_SetRenderDrawColor(r, 215, 60, 100, 255);
            for (int di=-5;di<=5;di++) for (int dj=-5;dj<=5;dj++)
                if (di*di+dj*dj<=25) SDL_RenderDrawPoint(r, spx+di, spy+dj);
            /* white center */
            SDL_SetRenderDrawColor(r, 255, 230, 240, 255);
            for (int di=-2;di<=2;di++) for (int dj=-2;dj<=2;dj++)
                if (di*di+dj*dj<=4) SDL_RenderDrawPoint(r, spx+di, spy+dj);
        }
    }

    /* Hover crosshair */
    if (mouseX >= DX && mouseX <= DX+DW && mouseY >= DY && mouseY <= DY+DH) {
        SDL_SetRenderDrawColor(r, 150, 60, 100, 150);
        SDL_RenderDrawLine(r, mouseX, DY, mouseX, DY+DH);
        SDL_SetRenderDrawColor(r, 150, 60, 100, 100);
        SDL_RenderDrawLine(r, DX, mouseY, DX+DW, mouseY);
        double hx = (double)(mouseX - unitX) / scale;
        double hy = (double)(unitY - mouseY) / scale;
        char htip[64]; sprintf(htip, "(%.2f, %.2f)", hx, hy);
        int tw = textW(fSmall, htip);
        int tx = mouseX + 10, ty = mouseY - 18;
        if (tx+tw > DX+DW) tx = mouseX - tw - 6;
        if (ty < DY) ty = DY + 2;
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 255, 240, 245, 220);
        SDL_Rect tipbg = {tx-3, ty-2, tw+8, 18};
        SDL_RenderFillRect(r, &tipbg);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(r, 210, 100, 140, 255); SDL_RenderDrawRect(r, &tipbg);
        renderText(r, fSmall, htip, tx, ty, (SDL_Color){160, 40, 80, 255});
    }

    /* Widen clip to full graph panel for tick marks, labels, and axis names
       — keeps them strictly inside the graph box, never leaking into other panels */
    SDL_Rect panelClip = {GX, GY, GW, GH};
    SDL_RenderSetClipRect(r, &panelClip);

    #undef WX
    #undef WY

    /* Axis end labels */
    renderText(r, fSmall, "x", DX+DW-10, unitY+4, (SDL_Color){180,80,120,200});
    renderText(r, fSmall, "y", unitX+4,  DY+2,    (SDL_Color){180,80,120,200});

    /* Tick marks and labels */
    for (int tx2 = -5; tx2 <= 5; tx2++) {
        if (tx2 == 0) continue;
        int sx = unitX + tx2*(int)scale;
        int sy2 = unitY - tx2*(int)scale;
        if (sx > DX && sx < DX+DW) {
            SDL_SetRenderDrawColor(r, 180,80,120,150);
            SDL_RenderDrawLine(r, sx, unitY-3, sx, unitY+3);
            char lb[8]; sprintf(lb, "%d", tx2);
            renderText(r, fSmall, lb, sx-5, unitY+5, (SDL_Color){160,70,110,180});
        }
        if (sy2 > DY && sy2 < DY+DH) {
            SDL_SetRenderDrawColor(r, 180,80,120,150);
            SDL_RenderDrawLine(r, unitX-3, sy2, unitX+3, sy2);
            char lb[8]; sprintf(lb, "%d", tx2);
            renderText(r, fSmall, lb, unitX+5, sy2-7, (SDL_Color){160,70,110,180});
        }
    }

    /* Remove clip before legend */
    SDL_RenderSetClipRect(r, NULL);

    /* Legend */
    int LY = GY + GH - 68;
    SDL_SetRenderDrawColor(r, 215, 60, 100, 255);
    SDL_Rect l1 = {GX+12, LY, 24, 3}; SDL_RenderFillRect(r, &l1);
    renderText(r, fSmall, "Equation 1", GX+42, LY-5, (SDL_Color){215,60,100,255});

    SDL_SetRenderDrawColor(r, 255, 130, 160, 255);
    SDL_Rect l2 = {GX+12, LY+18, 24, 3}; SDL_RenderFillRect(r, &l2);
    renderText(r, fSmall, "Equation 2", GX+42, LY+13, (SDL_Color){255,130,160,255});

    if (hasSol) {
        SDL_SetRenderDrawColor(r, 215, 60, 100, 255);
        drawCircleFill(r, GX+22, LY+38, 5);
        char slabel[64]; sprintf(slabel, "Solution (%.3f, %.3f)", solX, solY);
        renderText(r, fSmall, slabel, GX+42, LY+31, (SDL_Color){185,45,85,255});
    }
}

/* ══════════════════════════════════════════════════════
   COMPUTATION
   ══════════════════════════════════════════════════════ */
static void compute(void) {
    double a1=g_a1, b1=g_b1, c1=g_c1;
    double a2=g_a2, b2=g_b2, c2=g_c2;
    g_hasSteps  = 1;
    g_hasSolution = 0;
    g_specialCase = 0;

    /* If a1 == 0, try to isolate x from Eq2 instead, by swapping */
    int swapped = 0;
    if (fabs(a1) < 1e-10 && fabs(a2) > 1e-10) {
        double ta=a1; a1=a2; a2=ta;
        double tb=b1; b1=b2; b2=tb;
        double tc=c1; c1=c2; c2=tc;
        swapped = 1;
        g_specialCase = 3;
    }

    if (fabs(a1) < 1e-10) {
        /* Both a1=0 and a2=0 — can't isolate x this way */
        g_specialCase = 4; /* no x term in either eq */
        return;
    }

    /* Step: After substituting x=(c1-b1*y)/a1 into Eq2:
       a2*(c1-b1*y)/a1 + b2*y = c2
       Multiply by a1:
       a2*c1 - a2*b1*y + a1*b2*y = a1*c2
       y*(a1*b2 - a2*b1) = a1*c2 - a2*c1                 */
    double det    = a1*b2 - a2*b1;
    double rhs_y  = a1*c2 - a2*c1;
    g_det = det;
    /* Store the y-coefficient and constant for display */
    g_expr_coeff_y = det;     /* coefficient of y after simplification */
    g_expr_const   = rhs_y;   /* right-hand side */

    if (fabs(det) < 1e-10) {
        /* Determinant zero: check consistency */
        if (fabs(rhs_y) < 1e-10)
            g_specialCase = 2; /* infinite solutions */
        else
            g_specialCase = 1; /* no solution */
        return;
    }

    double solY = rhs_y / det;
    double solX = (c1 - b1*solY) / a1;

    /* If swapped, the symbols are still x,y so solution is fine — equations still correct */
    g_solX = solX;
    g_solY = solY;
    g_hasSolution = 1;

    /* Verify using the ORIGINAL (unswapped) coefficients */
    g_verify1 = g_a1*solX + g_b1*solY;
    g_verify2 = g_a2*solX + g_b2*solY;

    /* Update global a1 etc. to the potentially-swapped values for step display */
    if (swapped) {
        g_a1 = a1; g_b1 = b1; g_c1 = c1;
        g_a2 = a2; g_b2 = b2; g_c2 = c2;
    }
}

/* ══════════════════════════════════════════════════════
   MAIN
   ══════════════════════════════════════════════════════ */
int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window*   window   = SDL_CreateWindow(
        "Substitution Method - Linear Equations (2 Unknowns)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(renderer, WIN_W, WIN_H);

    TTF_Font* fHuge  = TTF_OpenFont("font.ttf", 38);
    TTF_Font* fBig   = TTF_OpenFont("font.ttf", 28);
    TTF_Font* fTitle = TTF_OpenFont("font.ttf", 22);
    TTF_Font* fLarge = TTF_OpenFont("font.ttf", 18);
    TTF_Font* fMed   = TTF_OpenFont("font.ttf", 16);
    TTF_Font* fNorm  = TTF_OpenFont("font.ttf", 14);
    TTF_Font* fSmall = TTF_OpenFont("font.ttf", 12);

    if (!fHuge||!fBig||!fTitle||!fLarge||!fMed||!fNorm||!fSmall) {
        printf("Font error: %s\n", TTF_GetError()); return 1;
    }

    /* ── Inputs (6 coefficients) ── */
    InputBox inputs[6];
    const char* iLabels[] = {"a1","b1","c1","a2","b2","c2"};
    /* Default equation:  2x + 3y = 12  |  x - y = 1  →  x=3, y=2 */
    const char* iDefaults[] = {"2", "3", "12", "1", "-1", "1"};
    for (int i = 0; i < 6; i++) {
        strcpy(inputs[i].label, iLabels[i]);
        strcpy(inputs[i].value, iDefaults[i]);
        inputs[i].active   = 0;
        inputs[i].rect     = (SDL_Rect){0, 0, 88, 36};
    }

    Button btnCompute    = {{0,0,130,40}, "COMPUTE",     0, 0};
    Button btnClear      = {{0,0,110,36}, "CLEAR",       0, 0};
    Button btnStart      = {{WIN_W/2-110,620,220,52}, "START", 0, 0};
    Button btnBack       = {{WIN_W-110,10,96,36}, "< BACK",   0, 0};
    Button btnClearYes   = {{WIN_W/2-190,330,168,44}, "YES, CLEAR", 0, 0};
    Button btnClearNo    = {{WIN_W/2+22, 330,140,44}, "CANCEL",     0, 0};
    int    showClearConfirm = 0;

    Petal  petals[NUM_PETALS];
    initPetals(petals);

    int    screen       = SCREEN_LANDING;
    int    activeInput  = -1;
    int    quit         = 0;
    Uint32 loadStart    = 0;
    float  loadProgress = 0.0f;
    char   statusMsg[256] = "";
    int    mouseX = 0, mouseY = 0;

    SDL_StartTextInput();
    SDL_Event ev;

    while (!quit) {
        /* ── Events ── */
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) { quit = 1; break; }

            /* ── LANDING ── */
            if (screen == SCREEN_LANDING) {
                if (ev.type == SDL_MOUSEBUTTONDOWN) {
                    int mx=ev.button.x, my=ev.button.y;
                    if (mx>=btnStart.rect.x && mx<btnStart.rect.x+btnStart.rect.w &&
                        my>=btnStart.rect.y && my<btnStart.rect.y+btnStart.rect.h) {
                        screen = SCREEN_LOADING;
                        loadStart = SDL_GetTicks();
                        loadProgress = 0.0f;
                    }
                }
                if (ev.type == SDL_MOUSEMOTION) {
                    int mx=ev.motion.x, my=ev.motion.y;
                    btnStart.hovered = (mx>=btnStart.rect.x && mx<btnStart.rect.x+btnStart.rect.w &&
                                        my>=btnStart.rect.y && my<btnStart.rect.y+btnStart.rect.h);
                }
                continue;
            }

            /* ── LOADING — no input needed ── */
            if (screen == SCREEN_LOADING) continue;

            /* ── SOLVER ── */
            if (ev.type == SDL_MOUSEBUTTONDOWN) {
                int mx=ev.button.x, my=ev.button.y;

                /* If clear confirm modal is active, handle only modal buttons */
                if (showClearConfirm) {
                    if (mx>=btnClearYes.rect.x && mx<btnClearYes.rect.x+btnClearYes.rect.w &&
                        my>=btnClearYes.rect.y && my<btnClearYes.rect.y+btnClearYes.rect.h) {
                        showClearConfirm = 0;
                        for (int i=0;i<6;i++) strcpy(inputs[i].value, iDefaults[i]);
                        statusMsg[0]='\0';
                        g_hasSteps=0; g_hasSolution=0; g_specialCase=0;
                        activeInput=-1;
                    }
                    if (mx>=btnClearNo.rect.x && mx<btnClearNo.rect.x+btnClearNo.rect.w &&
                        my>=btnClearNo.rect.y && my<btnClearNo.rect.y+btnClearNo.rect.h) {
                        showClearConfirm = 0;
                    }
                    continue;
                }

                /* BACK */
                if (mx>=btnBack.rect.x && mx<btnBack.rect.x+btnBack.rect.w &&
                    my>=btnBack.rect.y && my<btnBack.rect.y+btnBack.rect.h) {
                    screen = SCREEN_LANDING;
                    activeInput = -1;
                    for (int i=0;i<6;i++) { inputs[i].active=0; strcpy(inputs[i].value, iDefaults[i]); }
                    statusMsg[0]='\0';
                    g_hasSteps=0; g_hasSolution=0; g_specialCase=0;
                    continue;
                }

                /* Input boxes */
                activeInput = -1;
                for (int i=0;i<6;i++) {
                    inputs[i].active=0;
                    if (mx>=inputs[i].rect.x && mx<inputs[i].rect.x+inputs[i].rect.w &&
                        my>=inputs[i].rect.y && my<inputs[i].rect.y+inputs[i].rect.h) {
                        activeInput=i; inputs[i].active=1;
                    }
                }

                /* COMPUTE */
                if (mx>=btnCompute.rect.x && mx<btnCompute.rect.x+btnCompute.rect.w &&
                    my>=btnCompute.rect.y && my<btnCompute.rect.y+btnCompute.rect.h) {
                    btnCompute.clicked=1;
                    g_a1=atof(inputs[0].value); g_b1=atof(inputs[1].value); g_c1=atof(inputs[2].value);
                    g_a2=atof(inputs[3].value); g_b2=atof(inputs[4].value); g_c2=atof(inputs[5].value);
                    compute();
                    if (g_specialCase==1)       strcpy(statusMsg,"No unique solution (parallel lines).");
                    else if (g_specialCase==2)  strcpy(statusMsg,"Infinite solutions (same line).");
                    else if (g_specialCase==3)  strcpy(statusMsg,"a1=0: swapped — isolated x from Eq2.");
                    else if (g_specialCase==4)  strcpy(statusMsg,"Error: both equations have no x term.");
                    else if (g_hasSolution)     sprintf(statusMsg,"Solution found:  x = %.4f,  y = %.4f", g_solX, g_solY);
                    else                        strcpy(statusMsg,"Could not solve. Check inputs.");
                }

                /* CLEAR — open confirmation modal */
                if (mx>=btnClear.rect.x && mx<btnClear.rect.x+btnClear.rect.w &&
                    my>=btnClear.rect.y && my<btnClear.rect.y+btnClear.rect.h) {
                    btnClear.clicked=1;
                    showClearConfirm = 1;
                }
            }

            if (ev.type == SDL_MOUSEBUTTONUP) {
                btnCompute.clicked=0; btnClear.clicked=0;
                btnStart.clicked=0;   btnBack.clicked=0;
                btnClearYes.clicked=0; btnClearNo.clicked=0;
            }

            if (ev.type == SDL_MOUSEMOTION) {
                mouseX = ev.motion.x; mouseY = ev.motion.y;
                int mx=mouseX, my=mouseY;
                if (screen == SCREEN_SOLVER) {
                    btnBack.hovered    = (mx>=btnBack.rect.x    && mx<btnBack.rect.x+btnBack.rect.w    && my>=btnBack.rect.y    && my<btnBack.rect.y+btnBack.rect.h);
                    btnCompute.hovered = (mx>=btnCompute.rect.x && mx<btnCompute.rect.x+btnCompute.rect.w && my>=btnCompute.rect.y && my<btnCompute.rect.y+btnCompute.rect.h);
                    btnClear.hovered   = (mx>=btnClear.rect.x   && mx<btnClear.rect.x+btnClear.rect.w   && my>=btnClear.rect.y   && my<btnClear.rect.y+btnClear.rect.h);
                    btnClearYes.hovered = (showClearConfirm && mx>=btnClearYes.rect.x && mx<btnClearYes.rect.x+btnClearYes.rect.w && my>=btnClearYes.rect.y && my<btnClearYes.rect.y+btnClearYes.rect.h);
                    btnClearNo.hovered  = (showClearConfirm && mx>=btnClearNo.rect.x  && mx<btnClearNo.rect.x+btnClearNo.rect.w  && my>=btnClearNo.rect.y  && my<btnClearNo.rect.y+btnClearNo.rect.h);
                }
            }

            if (screen == SCREEN_SOLVER && !showClearConfirm) {
                if (ev.type == SDL_TEXTINPUT && activeInput >= 0) {
                    char ch = ev.text.text[0];
                    if ((ch>='0'&&ch<='9')||ch=='.'||ch=='-') {
                        int len=strlen(inputs[activeInput].value);
                        if (len < 18) {
                            inputs[activeInput].value[len]=ch;
                            inputs[activeInput].value[len+1]='\0';
                        }
                    }
                }
                if (ev.type == SDL_KEYDOWN && activeInput >= 0) {
                    if (ev.key.keysym.sym == SDLK_BACKSPACE) {
                        int len=strlen(inputs[activeInput].value);
                        if (len > 0) inputs[activeInput].value[len-1]='\0';
                    }
                    if (ev.key.keysym.sym == SDLK_TAB) {
                        inputs[activeInput].active=0;
                        activeInput = (activeInput+1) % 6;
                        inputs[activeInput].active=1;
                    }
                    if (ev.key.keysym.sym == SDLK_RETURN) {
                        /* trigger compute on Enter */
                        g_a1=atof(inputs[0].value); g_b1=atof(inputs[1].value); g_c1=atof(inputs[2].value);
                        g_a2=atof(inputs[3].value); g_b2=atof(inputs[4].value); g_c2=atof(inputs[5].value);
                        compute();
                        if (g_specialCase==1)       strcpy(statusMsg,"No unique solution (parallel lines).");
                        else if (g_specialCase==2)  strcpy(statusMsg,"Infinite solutions (same line).");
                        else if (g_specialCase==3)  strcpy(statusMsg,"a1=0: swapped — isolated x from Eq2.");
                        else if (g_specialCase==4)  strcpy(statusMsg,"Error: both equations have no x term.");
                        else if (g_hasSolution)     sprintf(statusMsg,"Solution found:  x = %.4f,  y = %.4f", g_solX, g_solY);
                        else                        strcpy(statusMsg,"Could not solve. Check inputs.");
                    }
                }
            }
        }

        /* ── Update loading ── */
        if (screen == SCREEN_LOADING) {
            Uint32 elapsed = SDL_GetTicks() - loadStart;
            loadProgress = (float)elapsed / 5000.0f;
            if (loadProgress >= 1.0f) { loadProgress = 1.0f; screen = SCREEN_SOLVER; }
        }

        float dtime = (float)SDL_GetTicks() / 1000.0f;

        /* ── Draw Background (all screens) ── */
        drawBackground(renderer);
        drawSakuraBranch(renderer);
        updateAndDrawPetals(renderer, petals, dtime);

        /* ══════════════════════════════════════
           LANDING SCREEN
           ══════════════════════════════════════ */
        if (screen == SCREEN_LANDING) {
            SDL_Color white = {255,255,255,255};
            SDL_Color cream = {255,220,235,255};
            SDL_Color rose  = {185, 45, 85, 255};
            SDL_Color deep  = {140, 25, 60, 255};

            /* Card */
            int cw=640, ch=440, cx=WIN_W/2-cw/2, cy=WIN_H/2-ch/2;

            /* Card shadow */
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 180, 80, 120, 60);
            SDL_Rect csh = {cx+6, cy+6, cw, ch}; SDL_RenderFillRect(renderer, &csh);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

            drawPanel(renderer, cx, cy, cw, ch, (SDL_Color){255,245,250,248}, (SDL_Color){220,140,168,255});

            /* Card header */
            SDL_SetRenderDrawColor(renderer, 215, 80, 115, 255);
            SDL_Rect chdr = {cx, cy, cw, 54}; SDL_RenderFillRect(renderer, &chdr);
            /* accent stripe */
            SDL_SetRenderDrawColor(renderer, 255, 160, 185, 255);
            SDL_Rect cstr = {cx, cy+51, cw, 3}; SDL_RenderFillRect(renderer, &cstr);
            renderCenterBold(renderer, fBig, "MT221 - NUMERICAL METHOD", WIN_W/2, cy+11, white);

            /* Cherry blossom flower decorations on card corners */
            drawPetalShape(renderer, cx+28, cy+90, 9, 2, 220);
            drawPetalShape(renderer, cx+cw-28, cy+90, 9, 2, 220);
            drawPetalShape(renderer, cx+14, cy+90, 5, 0, 180);
            drawPetalShape(renderer, cx+cw-14, cy+90, 5, 0, 180);

            /* Main title */
            renderCenterBold(renderer, fHuge, "SUBSTITUTION METHOD", WIN_W/2, cy+66, deep);
            renderCenterBold(renderer, fTitle, "Linear Equations \xe2\x80\x94 2 Unknowns", WIN_W/2, cy+112, rose);
            renderCenter(renderer, fLarge, "a\xe2\x82\x81x + b\xe2\x82\x81y = c\xe2\x82\x81     |     a\xe2\x82\x82x + b\xe2\x82\x82y = c\xe2\x82\x82", WIN_W/2, cy+140, (SDL_Color){185,90,130,255});

            /* Divider */
            SDL_SetRenderDrawColor(renderer, 220, 155, 175, 255);
            SDL_RenderDrawLine(renderer, cx+60, cy+172, cx+cw-60, cy+172);

            /* Info labels */
            renderCenter(renderer, fMed, "Semestral Project", WIN_W/2, cy+183, (SDL_Color){190,110,150,255});
            renderCenterBold(renderer, fTitle, "BSCPE 22003", WIN_W/2, cy+204, rose);

            SDL_SetRenderDrawColor(renderer, 220, 155, 175, 255);
            SDL_RenderDrawLine(renderer, cx+100, cy+232, cx+cw-100, cy+232);

            renderCenter(renderer, fMed, "Submitted by:", WIN_W/2, cy+241, (SDL_Color){190,110,150,255});
            renderCenterBold(renderer, fLarge, "Allen Jay De Guia", WIN_W/2, cy+260, deep);

            SDL_SetRenderDrawColor(renderer, 220, 155, 175, 255);
            SDL_RenderDrawLine(renderer, cx+100, cy+290, cx+cw-100, cy+290);

            renderCenter(renderer, fMed, "Instructor:", WIN_W/2, cy+298, (SDL_Color){190,110,150,255});
            renderCenterBold(renderer, fNorm, "Engr. Edgar Broncano", WIN_W/2, cy+315, rose);

            /* Card footer */
            SDL_SetRenderDrawColor(renderer, 215, 80, 115, 255);
            SDL_Rect cftr = {cx, cy+ch-32, cw, 32}; SDL_RenderFillRect(renderer, &cftr);
            renderCenter(renderer, fSmall, "Bestlink College of the Philippines", WIN_W/2, cy+ch-22, cream);

            /* START button */
            btnStart.rect = (SDL_Rect){WIN_W/2-110, cy+ch+18, 220, 50};
            renderButton(renderer, fTitle, &btnStart);

            renderCenter(renderer, fSmall, "Click START to begin", WIN_W/2, WIN_H-28, (SDL_Color){185,100,140,200});

            SDL_RenderPresent(renderer);
            SDL_Delay(16);
            continue;
        }

        /* ══════════════════════════════════════
           LOADING SCREEN
           ══════════════════════════════════════ */
        if (screen == SCREEN_LOADING) {
            SDL_Color white = {255,255,255,255};
            SDL_Color cream = {255,220,235,255};
            float pulse = 0.5f + 0.5f*(float)sin(dtime*3.0);

            /* Title */
            SDL_Color titleCol = {(Uint8)(185+pulse*30), 45, 85, 255};
            renderCenterBold(renderer, fHuge, "SUBSTITUTION METHOD", WIN_W/2, 280, titleCol);
            renderCenter(renderer, fTitle, "Linear Equations \xe2\x80\x94 2 Unknowns", WIN_W/2, 330, (SDL_Color){215,80,115,255});

            /* Spinning sakura dots */
            float angle = dtime * 2.2f;
            int dotCx = WIN_W/2, dotCy = 390;
            for (int d = 0; d < 6; d++) {
                float a2 = angle + d * (float)(M_PI*2/6);
                int dx = dotCx + (int)(26*(float)cos(a2));
                int dy = dotCy + (int)(26*(float)sin(a2));
                int alph = (int)(100 + 155*(0.5f + 0.5f*(float)sin(a2 - angle)));
                drawPetalShape(renderer, dx, dy, 5, 2, (Uint8)alph);
            }

            /* Loading labels */
            const char* loadLabels[] = {"Initializing...","Loading modules...","Preparing solver...","Almost ready...","Launching!"};
            int lIdx = (int)(loadProgress * 4.99f);
            if (lIdx > 4) lIdx = 4;
            renderCenter(renderer, fLarge, loadLabels[lIdx], WIN_W/2, 430, (SDL_Color){185,45,85,255});

            /* Progress bar */
            int barW=500, barH=24;
            int barX=WIN_W/2-barW/2, barY=470;
            drawPanel(renderer, barX, barY, barW, barH, (SDL_Color){255,230,240,255}, (SDL_Color){220,140,168,255});
            int fillW = (int)(loadProgress * (barW-4));
            if (fillW > 0) {
                for (int px = 0; px < fillW; px++) {
                    float t = (float)px / (barW-4);
                    int rv = (int)(215 - t*40);
                    int gv = (int)(80  + t*40);
                    int bv = (int)(115 + t*30);
                    SDL_SetRenderDrawColor(renderer, rv, gv, bv, 255);
                    SDL_RenderDrawLine(renderer, barX+2+px, barY+2, barX+2+px, barY+barH-3);
                }
            }

            /* Percentage */
            char pctBuf[16]; sprintf(pctBuf, "%d%%", (int)(loadProgress*100));
            renderCenterBold(renderer, fNorm, pctBuf, WIN_W/2, barY+barH+8, (SDL_Color){185,45,85,255});

            renderCenter(renderer, fNorm, "BSCPE 22003  |  Allen Jay De Guia", WIN_W/2, 545, cream);

            SDL_RenderPresent(renderer);
            SDL_Delay(16);
            continue;
        }

        /* ══════════════════════════════════════
           SOLVER SCREEN
           Different layout — top input strip,
           bottom-half: steps (left) | graph (right)
           ══════════════════════════════════════ */

        SDL_Color white  = {255,255,255,255};
        SDL_Color cream  = {255,220,235,255};
        SDL_Color rose   = {185, 45, 85, 255};
        SDL_Color deep   = {140, 25, 60, 255};
        SDL_Color panBg  = {255,245,248,255};
        SDL_Color panBdr = {230,155,175,255};
        SDL_Color secCol = {185, 45, 85, 255};
        SDL_Color drkTxt = {120, 30, 65, 255};
        SDL_Color hntCol = {200,120,155,255};

        /* ── Top banner (70px) ── */
        for (int row=0; row<70; row++) {
            float t = (float)row/70.0f;
            int rv=(int)(215 - t*30);
            int gv=(int)(80  + t*20);
            int bv=(int)(115 + t*15);
            SDL_SetRenderDrawColor(renderer,rv,gv,bv,255);
            SDL_RenderDrawLine(renderer,0,row,WIN_W,row);
        }
        SDL_SetRenderDrawColor(renderer,255,155,185,255);
        SDL_Rect bstripe={0,66,WIN_W,4}; SDL_RenderFillRect(renderer,&bstripe);

        renderBold(renderer, fTitle, "SUBSTITUTION METHOD", 20, 12, white);
        renderText(renderer, fMed, "Linear Equations  \xe2\x80\x94  2 Unknowns  |  MT221", 20, 38, cream);
        renderText(renderer, fSmall, "BSCPE 22003", WIN_W-240, 12, cream);
        renderText(renderer, fSmall, "Allen Jay De Guia", WIN_W-240, 30, cream);
        btnBack.rect = (SDL_Rect){WIN_W-106, 8, 96, 36};
        renderButton(renderer, fSmall, &btnBack);

        /* ── Input Strip (y=75 to y=195, full width) ── */
        drawPanel(renderer, 0, 71, WIN_W, 150, (SDL_Color){255,238,244,255}, (SDL_Color){220,140,168,255});

        /* Input strip inner label row */
        SDL_SetRenderDrawColor(renderer, 215,80,115,255);
        SDL_Rect istbar = {0,71,WIN_W,26}; SDL_RenderFillRect(renderer, &istbar);
        renderBold(renderer, fMed, "\xe2\x96\xb6  ENTER COEFFICIENTS", 18, 76, white);
        renderText(renderer, fSmall, "Eq 1:  a\xe2\x82\x81x + b\xe2\x82\x81y = c\xe2\x82\x81    |    Eq 2:  a\xe2\x82\x82x + b\xe2\x82\x82y = c\xe2\x82\x82", 400, 79, cream);

        /* Equation 1 group */
        renderBold(renderer, fNorm, "Equation 1:", 15, 100, rose);
        inputs[0].rect = (SDL_Rect){15,  138, 88, 36};
        inputs[1].rect = (SDL_Rect){118, 138, 88, 36};
        inputs[2].rect = (SDL_Rect){221, 138, 88, 36};
        for (int i=0;i<3;i++) renderInputBox(renderer, fSmall, fNorm, &inputs[i]);

        /* Divider between eq1 and eq2 */
        SDL_SetRenderDrawColor(renderer, 210,140,170,200);
        SDL_RenderDrawLine(renderer, 324, 97, 324, 190);

        /* Equation 2 group */
        renderBold(renderer, fNorm, "Equation 2:", 338, 100, rose);
        inputs[3].rect = (SDL_Rect){338, 138, 88, 36};
        inputs[4].rect = (SDL_Rect){441, 138, 88, 36};
        inputs[5].rect = (SDL_Rect){544, 138, 88, 36};
        for (int i=3;i<6;i++) renderInputBox(renderer, fSmall, fNorm, &inputs[i]);

        /* Vertical divider before buttons */
        SDL_SetRenderDrawColor(renderer, 210,140,170,200);
        SDL_RenderDrawLine(renderer, 648, 97, 648, 190);

        /* Buttons */
        btnCompute.rect = (SDL_Rect){660, 103, 130, 40};
        btnClear.rect   = (SDL_Rect){660, 153, 110, 36};
        renderButton(renderer, fNorm, &btnCompute);
        renderButton(renderer, fSmall, &btnClear);

        /* Hint text */
        renderText(renderer, fSmall, "Tab: next field", 803, 106, hntCol);
        renderText(renderer, fSmall, "Enter: compute",  803, 124, hntCol);

        /* Vertical divider before status */
        SDL_SetRenderDrawColor(renderer, 210,140,170,200);
        SDL_RenderDrawLine(renderer, 930, 97, 930, 190);

        /* Status box */
        renderBold(renderer, fNorm, "STATUS:", 944, 100, rose);
        if (statusMsg[0]) {
            int isOk = g_hasSolution;
            SDL_Color stc = isOk ? (SDL_Color){30,140,70,255} : (SDL_Color){200,40,60,255};
            if (g_specialCase==3) stc=(SDL_Color){200,120,40,255};
            renderText(renderer, fSmall, statusMsg, 944, 118, stc);
            if (isOk) {
                char buf[64]; sprintf(buf,"x = %.6f", g_solX);
                renderBold(renderer, fNorm, buf, 944, 140, drkTxt);
                sprintf(buf,"y = %.6f", g_solY);
                renderBold(renderer, fNorm, buf, 944, 160, drkTxt);
            }
        } else {
            renderText(renderer, fSmall, "Enter coefficients and press COMPUTE", 944, 118, hntCol);
        }

        /* ── Bottom half starts at y=225 ── */
        int bY = 225;
        int halfW = WIN_W / 2;  /* 700 each side */

        /* ── LEFT PANEL: Solution Steps ── */
        drawPanel(renderer, 0, bY, halfW, WIN_H-bY, panBg, panBdr);

        SDL_SetRenderDrawColor(renderer, 215,80,115,255);
        SDL_Rect spHdr = {0, bY, halfW, 28}; SDL_RenderFillRect(renderer, &spHdr);
        renderBold(renderer, fMed, "\xe2\x96\xb6  SOLUTION STEPS", 18, bY+6, white);

        if (!g_hasSteps) {
            renderCenter(renderer, fNorm, "Enter coefficients and press COMPUTE", halfW/2, bY+200, hntCol);
            renderCenter(renderer, fNorm, "to see the step-by-step solution.", halfW/2, bY+222, hntCol);
            /* decorative petal */
            drawPetalShape(renderer, halfW/2, bY+275, 18, 2, 100);
        } else {
            int sy = bY + 36;
            int pw = halfW - 20;   /* panel inner width */
            int px2 = 10;

            SDL_Color eqBg1  = {255,238,244,255};
            SDL_Color eqBdr1 = {225,145,170,255};
            SDL_Color eqBg2  = {255,232,240,255};
            SDL_Color eqBdr2 = {215,130,160,255};
            SDL_Color stepHdr = {185,45,85,255};
            char buf[256];

            /* ─ GIVEN ─ */
            drawPanel(renderer, px2, sy, pw, 65, eqBg1, eqBdr1);
            SDL_SetRenderDrawColor(renderer,205,65,100,255);
            SDL_Rect ghdr={px2, sy, pw, 22}; SDL_RenderFillRect(renderer,&ghdr);
            renderBold(renderer, fNorm, "GIVEN:  Original System", px2+8, sy+4, white);
            SDL_SetRenderDrawColor(renderer, eqBdr1.r,eqBdr1.g,eqBdr1.b,255);
            SDL_RenderDrawLine(renderer, px2+4, sy+22, px2+pw-4, sy+22);
            sprintf(buf, "Eq1:  %.4gx  %+.4gy  =  %.4g", g_a1, g_b1, g_c1);
            renderText(renderer, fNorm, buf, px2+12, sy+25, (SDL_Color){215,60,100,255});
            sprintf(buf, "Eq2:  %.4gx  %+.4gy  =  %.4g", g_a2, g_b2, g_c2);
            renderText(renderer, fNorm, buf, px2+12, sy+44, (SDL_Color){255,120,150,255});
            sy += 73;

            /* ─ STEP 1: Isolate x from Eq1 ─ */
            drawPanel(renderer, px2, sy, pw, 60, eqBg2, eqBdr2);
            SDL_SetRenderDrawColor(renderer,205,65,100,255);
            SDL_Rect s1h={px2,sy,pw,22}; SDL_RenderFillRect(renderer,&s1h);
            renderBold(renderer, fNorm, "STEP 1:  Isolate x from Equation 1", px2+8, sy+4, white);
            sprintf(buf, "a\xe2\x82\x81x = c\xe2\x82\x81 \xe2\x80\x93 b\xe2\x82\x81y     \xe2\x87\x92     x = (%.4g  \xe2\x80\x93  %.4g\xc2\xb7y) / %.4g", g_c1, g_b1, g_a1);
            renderText(renderer, fNorm, buf, px2+10, sy+26, drkTxt);
            sprintf(buf, "x = (%.4g \xe2\x80\x93 %.4gy) / %.4g", g_c1, g_b1, g_a1);
            renderBold(renderer, fNorm, buf, px2+10, sy+42, (SDL_Color){185,45,85,255});
            sy += 68;

            /* ─ STEP 2: Substitute into Eq2 ─ */
            drawPanel(renderer, px2, sy, pw, 62, eqBg1, eqBdr1);
            SDL_SetRenderDrawColor(renderer,205,65,100,255);
            SDL_Rect s2h={px2,sy,pw,22}; SDL_RenderFillRect(renderer,&s2h);
            renderBold(renderer, fNorm, "STEP 2:  Substitute x into Equation 2", px2+8, sy+4, white);
            sprintf(buf, "%.4g \xc2\xb7 [(%.4g \xe2\x80\x93 %.4gy)/%.4g]  +  %.4gy  =  %.4g",
                    g_a2, g_c1, g_b1, g_a1, g_b2, g_c2);
            renderText(renderer, fSmall, buf, px2+10, sy+25, drkTxt);
            /* Multiply through */
            sprintf(buf, "(%.4g\xe2\x80\x93%.4gy)  +  %.4gy  =  %.4g",
                    g_a2*g_c1/g_a1, g_a2*g_b1/g_a1, g_b2, g_c2);
            renderText(renderer, fSmall, buf, px2+10, sy+43, drkTxt);
            sy += 70;

            /* ─ STEP 3: Simplify — collect y terms ─ */
            drawPanel(renderer, px2, sy, pw, 78, eqBg2, eqBdr2);
            SDL_SetRenderDrawColor(renderer,205,65,100,255);
            SDL_Rect s3h={px2,sy,pw,22}; SDL_RenderFillRect(renderer,&s3h);
            renderBold(renderer, fNorm, "STEP 3:  Simplify \xe2\x80\x94 Collect y terms", px2+8, sy+4, white);
            /* left side constant: a2*c1/a1, y coefficient: b2 - a2*b1/a1 = det/a1 */
            double left_const = g_a2*g_c1/g_a1;
            double y_coeff    = g_b2 - g_a2*g_b1/g_a1;  /* = det/a1 */
            sprintf(buf, "%.6f  +  (%.6f)y  =  %.4g", left_const, y_coeff, g_c2);
            renderText(renderer, fNorm, buf, px2+10, sy+26, drkTxt);
            sprintf(buf, "(%.6f)y  =  %.4g  \xe2\x80\x93  %.6f", y_coeff, g_c2, left_const);
            renderText(renderer, fNorm, buf, px2+10, sy+44, drkTxt);
            sprintf(buf, "(%.6f)y  =  %.6f", g_expr_coeff_y/g_a1, g_expr_const/g_a1);
            renderBold(renderer, fSmall, buf, px2+10, sy+61, (SDL_Color){185,45,85,255});
            sy += 86;

            /* ─ STEP 4: Solve for y ─ */
            drawPanel(renderer, px2, sy, pw, 54, eqBg1, eqBdr1);
            SDL_SetRenderDrawColor(renderer,205,65,100,255);
            SDL_Rect s4h={px2,sy,pw,22}; SDL_RenderFillRect(renderer,&s4h);
            renderBold(renderer, fNorm, "STEP 4:  Solve for y", px2+8, sy+4, white);
            if (g_specialCase == 1) {
                renderBold(renderer, fNorm, "No unique solution \xe2\x80\x94 parallel lines (det = 0)", px2+10, sy+28, (SDL_Color){200,40,60,255});
            } else if (g_specialCase == 2) {
                renderBold(renderer, fNorm, "Infinite solutions \xe2\x80\x94 same line (det = 0)", px2+10, sy+28, (SDL_Color){200,120,40,255});
            } else {
                sprintf(buf, "y  =  %.6f / %.6f  =  %.6f",
                        g_expr_const/g_a1, g_expr_coeff_y/g_a1, g_solY);
                renderBold(renderer, fNorm, buf, px2+10, sy+30, (SDL_Color){185,45,85,255});
            }
            sy += 62;

            /* ─ STEP 5: Solve for x (only if hasSolution) ─ */
            if (g_hasSolution) {
                drawPanel(renderer, px2, sy, pw, 54, eqBg2, eqBdr2);
                SDL_SetRenderDrawColor(renderer,205,65,100,255);
                SDL_Rect s5h={px2,sy,pw,22}; SDL_RenderFillRect(renderer,&s5h);
                renderBold(renderer, fNorm, "STEP 5:  Back-substitute y \xe2\x86\x92 find x", px2+8, sy+4, white);
                sprintf(buf, "x = (%.4g \xe2\x80\x93 %.4g\xc2\xb7%.6f) / %.4g  =  %.6f",
                        g_c1, g_b1, g_solY, g_a1, g_solX);
                renderBold(renderer, fNorm, buf, px2+10, sy+30, (SDL_Color){185,45,85,255});
                sy += 62;

                /* ─ VERIFICATION ─ */
                drawPanel(renderer, px2, sy, pw, 90, eqBg1, eqBdr1);
                SDL_SetRenderDrawColor(renderer,205,65,100,255);
                SDL_Rect svh={px2,sy,pw,22}; SDL_RenderFillRect(renderer,&svh);
                renderBold(renderer, fNorm, "VERIFICATION", px2+8, sy+4, white);
                int ck1 = fabs(g_verify1 - g_c1) < 0.01;
                int ck2 = fabs(g_verify2 - g_c2) < 0.01;
                sprintf(buf, "Eq1: %.4g(%.4f) + %.4g(%.4f) = %.6f   [expected %.4g]  %s",
                        g_a1, g_solX, g_b1, g_solY, g_verify1, g_c1, ck1?"PASS":"FAIL");
                renderText(renderer, fSmall, buf, px2+10, sy+26,
                           ck1?(SDL_Color){30,140,70,255}:(SDL_Color){200,40,60,255});
                sprintf(buf, "Eq2: %.4g(%.4f) + %.4g(%.4f) = %.6f   [expected %.4g]  %s",
                        g_a2, g_solX, g_b2, g_solY, g_verify2, g_c2, ck2?"PASS":"FAIL");
                renderText(renderer, fSmall, buf, px2+10, sy+46,
                           ck2?(SDL_Color){30,140,70,255}:(SDL_Color){200,40,60,255});

                /* Final answer box */
                SDL_SetRenderDrawColor(renderer,215,80,115,255);
                SDL_Rect fab={px2+10, sy+65, pw-20, 22}; SDL_RenderFillRect(renderer,&fab);
                sprintf(buf,"Solution: x = %.6f          y = %.6f", g_solX, g_solY);
                renderCenterBold(renderer, fNorm, buf, halfW/2, sy+68, white);
            }
        }

        /* ── RIGHT PANEL: Graph ── */
        int mouseXl = mouseX, mouseYl = mouseY;
        int _ww, _wh; SDL_GetWindowSize(window, &_ww, &_wh);
        float _sc = ((float)_ww/WIN_W < (float)_wh/WIN_H) ? (float)_ww/WIN_W : (float)_wh/WIN_H;
        int _xo = (int)((_ww - _sc*WIN_W)/2), _yo = (int)((_wh - _sc*WIN_H)/2);
        int lmx = (int)((mouseXl - _xo) / _sc);
        int lmy = (int)((mouseYl - _yo) / _sc);

        drawPanel(renderer, halfW, bY, halfW, WIN_H-bY, panBg, panBdr);
        SDL_SetRenderDrawColor(renderer, 215,80,115,255);
        SDL_Rect gpHdr = {halfW, bY, halfW, 28}; SDL_RenderFillRect(renderer, &gpHdr);
        renderBold(renderer, fMed, "\xe2\x96\xb6  GRAPH  \xe2\x80\x94  Visual Representation", halfW+18, bY+6, white);

        drawGraph(renderer, fSmall,
                  g_a1, g_b1, g_c1, g_a2, g_b2, g_c2,
                  g_solX, g_solY, g_hasSolution,
                  lmx, lmy);

        /* Footer */
        renderCenter(renderer, fSmall, "MT221 Numerical Method  |  BSCPE 22003  |  Allen Jay De Guia",
                     WIN_W/2, WIN_H-18, (SDL_Color){200,130,160,200});

        /* ── CLEAR CONFIRMATION MODAL ── */
        if (showClearConfirm) {
            /* Dim overlay */
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 80, 20, 40, 175);
            SDL_Rect ovr = {0, 0, WIN_W, WIN_H};
            SDL_RenderFillRect(renderer, &ovr);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

            /* Modal card */
            int mw = 520, mh = 256;
            int mx2 = WIN_W/2 - mw/2, my2 = WIN_H/2 - mh/2;

            /* Shadow */
            SDL_SetRenderDrawColor(renderer, 60, 15, 30, 255);
            SDL_Rect shad = {mx2+6, my2+6, mw, mh};
            SDL_RenderFillRect(renderer, &shad);

            drawPanel(renderer, mx2, my2, mw, mh,
                      (SDL_Color){255,243,249,255},
                      (SDL_Color){215,80,115,255});

            /* Header bar */
            SDL_SetRenderDrawColor(renderer, 215, 80, 115, 255);
            SDL_Rect mhdr = {mx2, my2, mw, 48};
            SDL_RenderFillRect(renderer, &mhdr);
            SDL_SetRenderDrawColor(renderer, 255, 155, 185, 255);
            SDL_Rect mline = {mx2, my2+45, mw, 3};
            SDL_RenderFillRect(renderer, &mline);

            renderCenterBold(renderer, fTitle, "CONFIRM CLEAR", WIN_W/2, my2+11, white);
            drawPetalShape(renderer, mx2+22,    my2+24, 7, 2, 210);
            drawPetalShape(renderer, mx2+mw-22, my2+24, 7, 2, 210);

            /* Message */
            renderCenter(renderer, fLarge, "Are you sure you want to clear this equation?", WIN_W/2, my2+68, (SDL_Color){130,20,55,255});
            renderCenter(renderer, fNorm,  "All input fields and results will be removed.",  WIN_W/2, my2+96, (SDL_Color){180,70,110,255});

            /* Separator */
            SDL_SetRenderDrawColor(renderer, 215, 140, 168, 255);
            SDL_RenderDrawLine(renderer, mx2+40, my2+124, mx2+mw-40, my2+124);

            /* Action buttons */
            btnClearYes.rect = (SDL_Rect){WIN_W/2 - 190, my2+140, 168, 44};
            btnClearNo.rect  = (SDL_Rect){WIN_W/2 + 22,  my2+140, 140, 44};
            renderButton(renderer, fNorm, &btnClearYes);
            renderButton(renderer, fNorm, &btnClearNo);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    TTF_CloseFont(fHuge); TTF_CloseFont(fBig);  TTF_CloseFont(fTitle);
    TTF_CloseFont(fLarge);TTF_CloseFont(fMed);  TTF_CloseFont(fNorm);
    TTF_CloseFont(fSmall);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
