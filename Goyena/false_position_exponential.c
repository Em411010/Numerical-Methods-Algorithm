#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define MAX_ITER 50
#define TOLERANCE 0.0001
#define WINDOW_WIDTH 1400
#define WINDOW_HEIGHT 800

#define SCREEN_LANDING 0
#define SCREEN_LOADING 1
#define SCREEN_SOLVER  2

#define NUM_ROBOTS 8

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif



typedef struct {
    int iteration;
    double x0, x1, x2;
    double fx0, fx1, fx2;
    double error;
} IterationRow;

typedef struct {
    SDL_Rect rect;
    char label[50];
    char value[50];
    int active;
} InputBox;

typedef struct {
    SDL_Rect rect;
    char text[50];
    int hovered;
    int clicked;
} Button;

typedef struct {
    float x, y;
    float speed;
    float scale;
    float bobPhase;
    int style;          /* 0, 1, 2 = different looks */
    int facingRight;
} Robot;

/* ─── Math ─────────────────────────────────────────────── */

double f(double x, double a, double b) {
    return exp(x) - a * x - b;
}

void formatEquation(char* buffer, int a, int b) {
    char part1[50], part2[50];
    if (a == 0)       strcpy(part1, "");
    else if (a == 1)  strcpy(part1, " - x");
    else if (a == -1) strcpy(part1, " + x");
    else if (a > 0)   sprintf(part1, " - %dx", a);
    else              sprintf(part1, " + %dx", -a);

    if (b == 0)       strcpy(part2, "");
    else if (b > 0)   sprintf(part2, " - %d", b);
    else              sprintf(part2, " + %d", -b);

    sprintf(buffer, "Equation: e^x%s%s = 0", part1, part2);
}

/* ─── Primitive helpers ────────────────────────────────── */

void drawFilledCircle(SDL_Renderer* r, int cx, int cy, int rad) {
    for (int dy = -rad; dy <= rad; dy++) {
        int dx = (int)sqrt((double)(rad * rad - dy * dy));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

void drawFilledRoundedRect(SDL_Renderer* r, int x, int y, int w, int h, int rad) {
    SDL_Rect core = {x + rad, y, w - 2 * rad, h};
    SDL_RenderFillRect(r, &core);
    SDL_Rect left = {x, y + rad, rad, h - 2 * rad};
    SDL_RenderFillRect(r, &left);
    SDL_Rect right = {x + w - rad, y + rad, rad, h - 2 * rad};
    SDL_RenderFillRect(r, &right);
    drawFilledCircle(r, x + rad, y + rad, rad);
    drawFilledCircle(r, x + w - rad, y + rad, rad);
    drawFilledCircle(r, x + rad, y + h - rad, rad);
    drawFilledCircle(r, x + w - rad, y + h - rad, rad);
}

/* ─── Text rendering ──────────────────────────────────── */

void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &rect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void renderTextBold(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    renderText(renderer, font, text, x, y, color);
    renderText(renderer, font, text, x + 1, y, color);
}

void renderTextCentered(SDL_Renderer* renderer, TTF_Font* font, const char* text, int cx, int y, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect rect = {cx - surface->w / 2, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &rect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void renderTextCenteredBold(SDL_Renderer* renderer, TTF_Font* font, const char* text, int cx, int y, SDL_Color color) {
    renderTextCentered(renderer, font, text, cx, y, color);
    renderTextCentered(renderer, font, text, cx + 1, y, color);
}

void drawPanel(SDL_Renderer* renderer, int x, int y, int w, int h, SDL_Color bg, SDL_Color border) {
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &r);
}

/* ─── Robot Drawing ────────────────────────────────────── */

void drawRobot(SDL_Renderer* renderer, int cx, int cy, float scale, int style, Uint32 ticks, float bobPhase) {
    float bob = sinf(ticks * 0.002f + bobPhase) * 4.0f * scale;
    int by = cy + (int)bob;

    int bw = (int)(38 * scale);
    int bh = (int)(44 * scale);
    int hw = (int)(32 * scale);
    int hh = (int)(26 * scale);

    /* Body colors per style */
    int br, bg, bb;
    int hr, hg, hb;
    if (style == 0) { br = 90; bg = 155; bb = 220; hr = 70; hg = 130; hb = 195; }
    else if (style == 1) { br = 105; bg = 175; bb = 140; hr = 80; hg = 150; hb = 115; }
    else { br = 195; bg = 115; bb = 145; hr = 170; hg = 90; hb = 120; }

    /* Legs */
    int legW = (int)(8 * scale);
    int legH = (int)(16 * scale);
    float legWiggle = sinf(ticks * 0.005f + bobPhase) * 3.0f * scale;
    /* Left leg */
    SDL_SetRenderDrawColor(renderer, 80, 80, 100, 255);
    SDL_Rect lleg = {cx - bw / 3 + (int)legWiggle, by + bh / 2, legW, legH};
    SDL_RenderFillRect(renderer, &lleg);
    /* Left foot */
    SDL_SetRenderDrawColor(renderer, 60, 60, 80, 255);
    SDL_Rect lfoot = {cx - bw / 3 + (int)legWiggle - 2, by + bh / 2 + legH, legW + 4, (int)(5 * scale)};
    SDL_RenderFillRect(renderer, &lfoot);
    /* Right leg */
    SDL_SetRenderDrawColor(renderer, 80, 80, 100, 255);
    SDL_Rect rleg = {cx + bw / 3 - legW + (int)(-legWiggle), by + bh / 2, legW, legH};
    SDL_RenderFillRect(renderer, &rleg);
    /* Right foot */
    SDL_SetRenderDrawColor(renderer, 60, 60, 80, 255);
    SDL_Rect rfoot = {cx + bw / 3 - legW + (int)(-legWiggle) - 2, by + bh / 2 + legH, legW + 4, (int)(5 * scale)};
    SDL_RenderFillRect(renderer, &rfoot);

    /* Body */
    SDL_SetRenderDrawColor(renderer, br, bg, bb, 255);
    drawFilledRoundedRect(renderer, cx - bw / 2, by - bh / 2, bw, bh, (int)(6 * scale));
    /* Body panel detail */
    SDL_SetRenderDrawColor(renderer, br + 30, bg + 30, bb + 30, 255);
    SDL_Rect panel = {cx - bw / 4, by - bh / 4, bw / 2, bh / 3};
    SDL_RenderFillRect(renderer, &panel);
    /* Body center button */
    SDL_SetRenderDrawColor(renderer, 235, 75, 75, 255);
    drawFilledCircle(renderer, cx, by, (int)(4 * scale));
    /* Body indicator lights */
    int lightY = by - (int)(10 * scale);
    SDL_SetRenderDrawColor(renderer, 50, 255, 50, 255);
    drawFilledCircle(renderer, cx - (int)(8 * scale), lightY, (int)(2 * scale));
    SDL_SetRenderDrawColor(renderer, 255, 220, 50, 255);
    drawFilledCircle(renderer, cx + (int)(8 * scale), lightY, (int)(2 * scale));

    /* Arms */
    int armW = (int)(7 * scale);
    int armH = (int)(28 * scale);
    float armSwing = sinf(ticks * 0.003f + bobPhase + 1.0f) * 6.0f * scale;
    /* Left arm */
    SDL_SetRenderDrawColor(renderer, 80, 80, 100, 255);
    SDL_Rect larm = {cx - bw / 2 - armW - 1, by - bh / 4 + (int)armSwing, armW, armH};
    SDL_RenderFillRect(renderer, &larm);
    SDL_SetRenderDrawColor(renderer, 200, 200, 210, 255);
    drawFilledCircle(renderer, cx - bw / 2 - armW / 2 - 1, by - bh / 4 + armH + (int)armSwing, (int)(5 * scale));
    /* Right arm */
    SDL_SetRenderDrawColor(renderer, 80, 80, 100, 255);
    SDL_Rect rarm = {cx + bw / 2 + 1, by - bh / 4 + (int)(-armSwing), armW, armH};
    SDL_RenderFillRect(renderer, &rarm);
    SDL_SetRenderDrawColor(renderer, 200, 200, 210, 255);
    drawFilledCircle(renderer, cx + bw / 2 + armW / 2 + 1, by - bh / 4 + armH + (int)(-armSwing), (int)(5 * scale));

    /* Neck */
    SDL_SetRenderDrawColor(renderer, 120, 120, 140, 255);
    SDL_Rect neck = {cx - (int)(4 * scale), by - bh / 2 - (int)(6 * scale), (int)(8 * scale), (int)(7 * scale)};
    SDL_RenderFillRect(renderer, &neck);

    /* Head */
    int headY = by - bh / 2 - hh - (int)(4 * scale);
    SDL_SetRenderDrawColor(renderer, hr, hg, hb, 255);
    drawFilledRoundedRect(renderer, cx - hw / 2, headY, hw, hh, (int)(5 * scale));

    /* Antenna */
    SDL_SetRenderDrawColor(renderer, 120, 120, 140, 255);
    SDL_RenderDrawLine(renderer, cx, headY, cx, headY - (int)(14 * scale));
    SDL_RenderDrawLine(renderer, cx + 1, headY, cx + 1, headY - (int)(14 * scale));
    /* Antenna tip (blinking) */
    int blink = ((ticks / 500 + style) % 3 == 0) ? 255 : 120;
    SDL_SetRenderDrawColor(renderer, 255, blink, blink - 80 > 0 ? blink - 80 : 0, 255);
    drawFilledCircle(renderer, cx, headY - (int)(14 * scale), (int)(4 * scale));

    /* Eyes */
    int eyeY = headY + hh / 2 - (int)(2 * scale);
    int eyeSep = (int)(9 * scale);
    int eyeR = (int)(5 * scale);
    /* Eye sockets (dark bg) */
    SDL_SetRenderDrawColor(renderer, 30, 30, 45, 255);
    drawFilledCircle(renderer, cx - eyeSep, eyeY, eyeR + 1);
    drawFilledCircle(renderer, cx + eyeSep, eyeY, eyeR + 1);
    /* Eye glow */
    if (style == 0) SDL_SetRenderDrawColor(renderer, 80, 220, 255, 255);
    else if (style == 1) SDL_SetRenderDrawColor(renderer, 80, 255, 130, 255);
    else SDL_SetRenderDrawColor(renderer, 255, 140, 180, 255);
    drawFilledCircle(renderer, cx - eyeSep, eyeY, eyeR);
    drawFilledCircle(renderer, cx + eyeSep, eyeY, eyeR);
    /* Pupils */
    SDL_SetRenderDrawColor(renderer, 15, 15, 30, 255);
    drawFilledCircle(renderer, cx - eyeSep + 1, eyeY + 1, (int)(2 * scale));
    drawFilledCircle(renderer, cx + eyeSep + 1, eyeY + 1, (int)(2 * scale));

    /* Mouth */
    int mouthY = headY + hh - (int)(7 * scale);
    SDL_SetRenderDrawColor(renderer, 200, 200, 215, 255);
    SDL_Rect mouth = {cx - (int)(7 * scale), mouthY, (int)(14 * scale), (int)(3 * scale)};
    SDL_RenderFillRect(renderer, &mouth);
    /* Mouth grill lines */
    SDL_SetRenderDrawColor(renderer, hr - 15, hg - 15, hb - 15, 255);
    for (int i = 0; i < 4; i++) {
        int lx = cx - (int)(6 * scale) + i * (int)(4 * scale);
        SDL_RenderDrawLine(renderer, lx, mouthY, lx, mouthY + (int)(3 * scale));
    }

    /* Ear bolts */
    SDL_SetRenderDrawColor(renderer, 170, 170, 185, 255);
    drawFilledCircle(renderer, cx - hw / 2 - (int)(2 * scale), headY + hh / 2, (int)(3 * scale));
    drawFilledCircle(renderer, cx + hw / 2 + (int)(2 * scale), headY + hh / 2, (int)(3 * scale));
}

/* ─── Background with gradient + robots ────────────────── */

void drawBackground(SDL_Renderer* renderer, Robot robots[], Uint32 ticks) {
    /* Pink gradient sky: deep rose at top → soft blush at bottom */
    for (int y = 0; y < WINDOW_HEIGHT; y++) {
        float t = (float)y / WINDOW_HEIGHT;
        int r = (int)(130 + t * 95);   /* 130 → 225 */
        int g = (int)(25  + t * 85);   /* 25  → 110 */
        int b = (int)(65  + t * 70);   /* 65  → 135 */
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderDrawLine(renderer, 0, y, WINDOW_WIDTH, y);
    }
    /* Subtle pink grid overlay */
    SDL_SetRenderDrawColor(renderer, 200, 80, 120, 50);
    for (int x = 0; x < WINDOW_WIDTH; x += 60)
        SDL_RenderDrawLine(renderer, x, 0, x, WINDOW_HEIGHT);
    for (int y = 0; y < WINDOW_HEIGHT; y += 60)
        SDL_RenderDrawLine(renderer, 0, y, WINDOW_WIDTH, y);

    /* Ground platform */
    SDL_SetRenderDrawColor(renderer, 110, 28, 60, 255);
    SDL_Rect ground = {0, WINDOW_HEIGHT - 65, WINDOW_WIDTH, 65};
    SDL_RenderFillRect(renderer, &ground);
    /* Ground stripe */
    SDL_SetRenderDrawColor(renderer, 240, 90, 145, 255);
    SDL_Rect gstripe = {0, WINDOW_HEIGHT - 65, WINDOW_WIDTH, 3};
    SDL_RenderFillRect(renderer, &gstripe);
    /* Ground dots */
    SDL_SetRenderDrawColor(renderer, 185, 60, 105, 255);
    for (int x = 20; x < WINDOW_WIDTH; x += 45)
        drawFilledCircle(renderer, x, WINDOW_HEIGHT - 40, 2);

    /* Floating pink particles */
    for (int i = 0; i < 30; i++) {
        float px = (float)((i * 137 + (int)(ticks * 0.02f)) % WINDOW_WIDTH);
        float py = (float)((i * 89 + (int)(ticks * 0.008f * (i % 3 + 1))) % (WINDOW_HEIGHT - 80));
        int v = 60 + (i * 23) % 70;
        SDL_SetRenderDrawColor(renderer, 220, v, v + 40, 255);
        drawFilledCircle(renderer, (int)px, (int)py, 1 + i % 2);
    }

    /* Robots */
    for (int i = 0; i < NUM_ROBOTS; i++) {
        drawRobot(renderer, (int)robots[i].x, (int)robots[i].y, robots[i].scale,
                  robots[i].style, ticks, robots[i].bobPhase);
    }
}

/* ─── UI Components (Steel-blue tech theme) ────────────── */

void renderInputBox(SDL_Renderer* renderer, TTF_Font* font, InputBox* box) {
    SDL_Color labelColor = {255, 140, 180, 255};
    renderTextBold(renderer, font, box->label, box->rect.x - 80, box->rect.y + 5, labelColor);
    if (box->active)
        SDL_SetRenderDrawColor(renderer, 90, 20, 50, 255);
    else
        SDL_SetRenderDrawColor(renderer, 65, 12, 35, 255);
    SDL_RenderFillRect(renderer, &box->rect);
    if (box->active)
        SDL_SetRenderDrawColor(renderer, 255, 100, 150, 255);
    else
        SDL_SetRenderDrawColor(renderer, 180, 60, 100, 255);
    SDL_RenderDrawRect(renderer, &box->rect);
    SDL_Color textColor = {255, 200, 225, 255};
    if (strlen(box->value) > 0)
        renderText(renderer, font, box->value, box->rect.x + 5, box->rect.y + 5, textColor);
}

void renderButton(SDL_Renderer* renderer, TTF_Font* font, Button* btn) {
    /* Shadow */
    SDL_SetRenderDrawColor(renderer, 60, 10, 30, 255);
    SDL_Rect shadow = {btn->rect.x + 3, btn->rect.y + 3, btn->rect.w, btn->rect.h};
    SDL_RenderFillRect(renderer, &shadow);
    /* Face */
    if (btn->clicked)
        SDL_SetRenderDrawColor(renderer, 130, 35, 75, 255);
    else if (btn->hovered)
        SDL_SetRenderDrawColor(renderer, 240, 85, 140, 255);
    else
        SDL_SetRenderDrawColor(renderer, 195, 60, 105, 255);
    SDL_RenderFillRect(renderer, &btn->rect);
    /* Highlight top edge */
    SDL_SetRenderDrawColor(renderer, 255, 160, 195, 255);
    SDL_RenderDrawLine(renderer, btn->rect.x + 1, btn->rect.y, btn->rect.x + btn->rect.w - 2, btn->rect.y);
    /* Border */
    SDL_SetRenderDrawColor(renderer, 155, 45, 85, 255);
    SDL_RenderDrawRect(renderer, &btn->rect);
    /* Text */
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(font, btn->text, textColor);
    if (surface) {
        int textX = btn->rect.x + (btn->rect.w - surface->w) / 2;
        int textY = btn->rect.y + (btn->rect.h - surface->h) / 2;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_Rect textRect = {textX, textY, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, NULL, &textRect);
        SDL_FreeSurface(surface);
        SDL_DestroyTexture(texture);
    }
}

/* ─── Graph (Tech blue theme) ──────────────────────────── */

void drawGraph(SDL_Renderer* renderer, TTF_Font* fontSmall, double a, double b, double root, int hasRoot) {
    int graphX = 980, graphY = 220, graphW = 400, graphH = 440;
    int scale = 50;
    int centerX = graphX + graphW / 2;
    int centerY = graphY + graphH / 2;

    SDL_SetRenderDrawColor(renderer, 55, 12, 35, 255);
    SDL_Rect graphRect = {graphX, graphY, graphW, graphH};
    SDL_RenderFillRect(renderer, &graphRect);
    SDL_SetRenderDrawColor(renderer, 190, 65, 110, 255);
    SDL_RenderDrawRect(renderer, &graphRect);

    /* Grid */
    SDL_SetRenderDrawColor(renderer, 100, 28, 60, 255);
    for (int i = graphX; i <= graphX + graphW; i += 50)
        SDL_RenderDrawLine(renderer, i, graphY, i, graphY + graphH);
    for (int i = graphY; i <= graphY + graphH; i += 50)
        SDL_RenderDrawLine(renderer, graphX, i, graphX + graphW, i);

    /* Axes */
    SDL_SetRenderDrawColor(renderer, 220, 90, 145, 255);
    SDL_RenderDrawLine(renderer, centerX, graphY, centerX, graphY + graphH);
    SDL_RenderDrawLine(renderer, graphX, centerY, graphX + graphW, centerY);

    /* Axis labels */
    renderText(renderer, fontSmall, "x", graphX + graphW - 15, centerY + 5, (SDL_Color){255, 150, 190, 255});
    renderText(renderer, fontSmall, "y", centerX + 5, graphY + 5, (SDL_Color){255, 150, 190, 255});

    /* Tick marks */
    SDL_SetRenderDrawColor(renderer, 220, 90, 145, 255);
    for (int i = graphX; i <= graphX + graphW; i += 50)
        if (i != centerX) SDL_RenderDrawLine(renderer, i, centerY - 3, i, centerY + 3);
    for (int i = graphY; i <= graphY + graphH; i += 50)
        if (i != centerY) SDL_RenderDrawLine(renderer, centerX - 3, i, centerX + 3, i);

    /* Curve (hot pink glow) */
    SDL_SetRenderDrawColor(renderer, 255, 60, 160, 255);
    for (int px = graphX; px < graphX + graphW; px++) {
        double x = (px - centerX) / (double)scale;
        double y = f(x, a, b);
        int py = centerY - (int)(y * 20);
        if (py >= graphY && py < graphY + graphH && fabs(y) < 50) {
            SDL_RenderDrawPoint(renderer, px, py);
            SDL_RenderDrawPoint(renderer, px, py + 1);
        }
    }

    /* Root point */
    if (hasRoot) {
        int rx = centerX + (int)(root * scale);
        /* Outer glow */
        SDL_SetRenderDrawColor(renderer, 255, 200, 80, 255);
        for (int i = -12; i <= 12; i++)
            for (int j = -12; j <= 12; j++)
                if (i*i + j*j <= 144 && i*i + j*j > 64)
                    SDL_RenderDrawPoint(renderer, rx + i, centerY + j);
        /* Inner dot */
        SDL_SetRenderDrawColor(renderer, 255, 240, 60, 255);
        for (int i = -8; i <= 8; i++)
            for (int j = -8; j <= 8; j++)
                if (i*i + j*j <= 64)
                    SDL_RenderDrawPoint(renderer, rx + i, centerY + j);
    }
}

/* ═══════════════════════════════════════════════════════════
   MAIN
   ═══════════════════════════════════════════════════════════ */

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    srand((unsigned)time(NULL));

    SDL_Window* window = SDL_CreateWindow("False Position Method - Exponential",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    /* Fonts */
    TTF_Font* font       = TTF_OpenFont("font.ttf", 18);
    TTF_Font* fontSmall   = TTF_OpenFont("font.ttf", 14);
    TTF_Font* fontMedium  = TTF_OpenFont("font.ttf", 16);
    TTF_Font* fontLarge   = TTF_OpenFont("font.ttf", 20);
    TTF_Font* fontTitle   = TTF_OpenFont("font.ttf", 24);
    TTF_Font* fontHuge    = TTF_OpenFont("font.ttf", 44);
    TTF_Font* fontBig     = TTF_OpenFont("font.ttf", 32);
    TTF_Font* fontStep    = TTF_OpenFont("font.ttf", 15);
    if (!font || !fontSmall || !fontMedium || !fontLarge || !fontTitle || !fontHuge || !fontBig || !fontStep) {
        printf("Error loading font: %s\n", TTF_GetError());
        return 1;
    }

    /* Robots */
    Robot robots[NUM_ROBOTS];
    /* Place robots across the bottom area and some floating */
    float robotPositions[][2] = {
        {120, 690}, {350, 700}, {620, 680}, {880, 695},
        {1100, 685}, {1300, 700}, {200, 250}, {1200, 200}
    };
    for (int i = 0; i < NUM_ROBOTS; i++) {
        robots[i].x = robotPositions[i][0];
        robots[i].y = robotPositions[i][1];
        robots[i].speed = 0.15f + (float)(rand() % 100) / 350.0f;
        robots[i].scale = 0.7f + (float)(rand() % 60) / 100.0f;
        robots[i].bobPhase = (float)(rand() % 628) / 100.0f;
        robots[i].style = i % 3;
        robots[i].facingRight = (i % 2 == 0);
    }
    /* Make floating robots smaller */
    robots[6].scale = 0.55f;
    robots[7].scale = 0.50f;

    /* Screen state */
    int currentScreen = SCREEN_LANDING;
    float loadingProgress = 0.0f;
    int loadingDoneDelay = 0;

    /* Landing button */
    Button startBtn = {{0, 0, 240, 55}, "LAUNCH SOLVER", 0, 0};

    /* Solver inputs */
    InputBox inputs[4];
    const char* labels[] = {"a:", "b:", "x0:", "x1:"};
    for (int i = 0; i < 4; i++) {
        inputs[i].rect = (SDL_Rect){140, 230 + i * 60, 150, 35};
        strcpy(inputs[i].label, labels[i]);
        strcpy(inputs[i].value, "");
        inputs[i].active = 0;
    }
    Button computeBtn = {{50, 490, 130, 42}, "COMPUTE", 0, 0};
    Button clearBtn   = {{200, 490, 130, 42}, "CLEAR", 0, 0};

    /* Solver state */
    char resultText[500] = "Enter coefficients and initial guesses (x0 and x1)";
    double finalRoot = 0;
    int hasValidRoot = 0;
    double coefA = 0, coefB = 0;
    IterationRow iterations[MAX_ITER];
    int totalIterations = 0;
    int activeInput = -1;
    int tableScrollOffset = 0;

    int quit = 0;
    SDL_Event e;

    while (!quit) {
        Uint32 ticks = SDL_GetTicks();

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = 1;

            if (currentScreen == SCREEN_LANDING) {
                if (e.type == SDL_MOUSEBUTTONDOWN) {
                    int mx = e.button.x, my = e.button.y;
                    if (mx >= startBtn.rect.x && mx <= startBtn.rect.x + startBtn.rect.w &&
                        my >= startBtn.rect.y && my <= startBtn.rect.y + startBtn.rect.h) {
                        currentScreen = SCREEN_LOADING;
                        loadingProgress = 0.0f;
                        loadingDoneDelay = 0;
                    }
                }
                if (e.type == SDL_MOUSEMOTION) {
                    int mx = e.motion.x, my = e.motion.y;
                    startBtn.hovered = (mx >= startBtn.rect.x && mx <= startBtn.rect.x + startBtn.rect.w &&
                                        my >= startBtn.rect.y && my <= startBtn.rect.y + startBtn.rect.h);
                }
            } else if (currentScreen == SCREEN_SOLVER) {
                if (e.type == SDL_MOUSEBUTTONDOWN) {
                    int mx = e.button.x, my = e.button.y;
                    activeInput = -1;
                    for (int i = 0; i < 4; i++) {
                        if (mx >= inputs[i].rect.x && mx <= inputs[i].rect.x + inputs[i].rect.w &&
                            my >= inputs[i].rect.y && my <= inputs[i].rect.y + inputs[i].rect.h)
                            activeInput = i;
                        inputs[i].active = (i == activeInput);
                    }
                    if (mx >= computeBtn.rect.x && mx <= computeBtn.rect.x + computeBtn.rect.w &&
                        my >= computeBtn.rect.y && my <= computeBtn.rect.y + computeBtn.rect.h) {
                        computeBtn.clicked = 1;
                        coefA = atof(inputs[0].value);
                        coefB = atof(inputs[1].value);
                        double x0 = atof(inputs[2].value);
                        double x1 = atof(inputs[3].value);
                        double fx0 = f(x0, coefA, coefB);
                        double fx1 = f(x1, coefA, coefB);
                        if (fx0 * fx1 >= 0) {
                            sprintf(resultText, "ERROR: f(x0) and f(x1) must have opposite signs!\nf(%.2f)=%.4f, f(%.2f)=%.4f",
                                    x0, fx0, x1, fx1);
                            hasValidRoot = 0;
                            totalIterations = 0;
                        } else {
                            totalIterations = 0;
                            hasValidRoot = 0;
                            for (int iter = 0; iter < MAX_ITER; iter++) {
                                double x2 = x1 - fx1 * (x1 - x0) / (fx1 - fx0);
                                double fx2 = f(x2, coefA, coefB);
                                double error = fabs(fx2);
                                iterations[iter].iteration = iter + 1;
                                iterations[iter].x0 = x0; iterations[iter].x1 = x1; iterations[iter].x2 = x2;
                                iterations[iter].fx0 = fx0; iterations[iter].fx1 = fx1; iterations[iter].fx2 = fx2;
                                iterations[iter].error = error;
                                totalIterations++;
                                if (error < TOLERANCE) {
                                    hasValidRoot = 1;
                                    finalRoot = x2;
                                    sprintf(resultText, "SUCCESS!\nRoot: x = %.6f\nIterations: %d", x2, iter + 1);
                                    break;
                                }
                                if (fx0 * fx2 < 0) { x1 = x2; fx1 = fx2; }
                                else { x0 = x2; fx0 = fx2; }
                            }
                            if (!hasValidRoot)
                                sprintf(resultText, "FAILED: Did not converge\nTry different initial guesses");
                        }
                    }
                    if (mx >= clearBtn.rect.x && mx <= clearBtn.rect.x + clearBtn.rect.w &&
                        my >= clearBtn.rect.y && my <= clearBtn.rect.y + clearBtn.rect.h) {
                        for (int i = 0; i < 4; i++) strcpy(inputs[i].value, "");
                        strcpy(resultText, "Enter coefficients and initial guesses (x0 and x1)");
                        hasValidRoot = 0;
                        totalIterations = 0;
                        tableScrollOffset = 0;
                        clearBtn.clicked = 1;
                    }
                }
                if (e.type == SDL_MOUSEBUTTONUP) {
                    computeBtn.clicked = 0; clearBtn.clicked = 0;
                }
                if (e.type == SDL_MOUSEMOTION) {
                    int mx = e.motion.x, my = e.motion.y;
                    computeBtn.hovered = (mx >= computeBtn.rect.x && mx <= computeBtn.rect.x + computeBtn.rect.w &&
                                          my >= computeBtn.rect.y && my <= computeBtn.rect.y + computeBtn.rect.h);
                    clearBtn.hovered   = (mx >= clearBtn.rect.x && mx <= clearBtn.rect.x + clearBtn.rect.w &&
                                          my >= clearBtn.rect.y && my <= clearBtn.rect.y + clearBtn.rect.h);
                }
                if (e.type == SDL_TEXTINPUT && activeInput >= 0) {
                    char c = e.text.text[0];
                    if ((c >= '0' && c <= '9') || c == '.' || c == '-') {
                        int len = strlen(inputs[activeInput].value);
                        if (len < 19) {
                            inputs[activeInput].value[len] = c;
                            inputs[activeInput].value[len + 1] = '\0';
                        }
                    }
                }
                if (e.type == SDL_KEYDOWN && activeInput >= 0) {
                    if (e.key.keysym.sym == SDLK_BACKSPACE) {
                        int len = strlen(inputs[activeInput].value);
                        if (len > 0) inputs[activeInput].value[len - 1] = '\0';
                    }
                }
                if (e.type == SDL_MOUSEWHEEL) {
                    if (totalIterations > 0) {
                        tableScrollOffset -= e.wheel.y * 2;
                        if (tableScrollOffset < 0) tableScrollOffset = 0;
                        int maxScroll = totalIterations - 10;
                        if (maxScroll < 0) maxScroll = 0;
                        if (tableScrollOffset > maxScroll) tableScrollOffset = maxScroll;
                    }
                }
            }
        }

        /* ── Update robot animations ── */
        for (int i = 0; i < NUM_ROBOTS; i++) {
            if (robots[i].facingRight) {
                robots[i].x += robots[i].speed;
                if (robots[i].x > WINDOW_WIDTH + 80) robots[i].x = -80.0f;
            } else {
                robots[i].x -= robots[i].speed;
                if (robots[i].x < -80) robots[i].x = WINDOW_WIDTH + 80.0f;
            }
        }
        if (currentScreen == SCREEN_LOADING) {
            loadingProgress += 0.004f;
            if (loadingProgress >= 1.0f) {
                loadingProgress = 1.0f;
                loadingDoneDelay++;
                if (loadingDoneDelay > 50) currentScreen = SCREEN_SOLVER;
            }
        }

        /* ════════════════════ RENDER ════════════════════ */
        drawBackground(renderer, robots, ticks);

        /* ──────────────── LANDING SCREEN ──────────────── */
        if (currentScreen == SCREEN_LANDING) {
            int cw = 750, ch = 440;
            int cardX = (WINDOW_WIDTH - cw) / 2, cardY = (WINDOW_HEIGHT - ch) / 2 - 40;

            /* Card shadow */
            SDL_SetRenderDrawColor(renderer, 8, 15, 35, 180);
            SDL_Rect cardShadow = {cardX + 5, cardY + 5, cw, ch};
            SDL_RenderFillRect(renderer, &cardShadow);
            /* Card bg */
            SDL_SetRenderDrawColor(renderer, 18, 30, 62, 240);
            SDL_Rect card = {cardX, cardY, cw, ch};
            SDL_RenderFillRect(renderer, &card);
            /* Borders */
            SDL_SetRenderDrawColor(renderer, 55, 130, 235, 255);
            SDL_RenderDrawRect(renderer, &card);
            SDL_Rect inner = {cardX + 3, cardY + 3, cw - 6, ch - 6};
            SDL_SetRenderDrawColor(renderer, 35, 85, 170, 255);
            SDL_RenderDrawRect(renderer, &inner);
            /* Top accent bar */
            SDL_SetRenderDrawColor(renderer, 40, 120, 230, 255);
            SDL_Rect topBar = {cardX, cardY, cw, 8};
            SDL_RenderFillRect(renderer, &topBar);
            /* Bottom accent bar */
            SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
            SDL_Rect bottomBar = {cardX, cardY + ch - 4, cw, 4};
            SDL_RenderFillRect(renderer, &bottomBar);

            /* Corner decorations (small brackets) */
            SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
            /* Top-left */
            SDL_RenderDrawLine(renderer, cardX + 12, cardY + 15, cardX + 12, cardY + 35);
            SDL_RenderDrawLine(renderer, cardX + 12, cardY + 15, cardX + 32, cardY + 15);
            /* Top-right */
            SDL_RenderDrawLine(renderer, cardX + cw - 12, cardY + 15, cardX + cw - 12, cardY + 35);
            SDL_RenderDrawLine(renderer, cardX + cw - 12, cardY + 15, cardX + cw - 32, cardY + 15);
            /* Bottom-left */
            SDL_RenderDrawLine(renderer, cardX + 12, cardY + ch - 15, cardX + 12, cardY + ch - 35);
            SDL_RenderDrawLine(renderer, cardX + 12, cardY + ch - 15, cardX + 32, cardY + ch - 15);
            /* Bottom-right */
            SDL_RenderDrawLine(renderer, cardX + cw - 12, cardY + ch - 15, cardX + cw - 12, cardY + ch - 35);
            SDL_RenderDrawLine(renderer, cardX + cw - 12, cardY + ch - 15, cardX + cw - 32, cardY + ch - 15);

            /* Title */
            renderTextCenteredBold(renderer, fontHuge, "FALSE POSITION",
                                   WINDOW_WIDTH / 2, cardY + 45, (SDL_Color){80, 195, 255, 255});
            renderTextCenteredBold(renderer, fontBig, "METHOD",
                                   WINDOW_WIDTH / 2, cardY + 100, (SDL_Color){60, 170, 240, 255});

            /* Divider line */
            int lineY = cardY + 148;
            SDL_SetRenderDrawColor(renderer, 50, 130, 230, 255);
            SDL_RenderDrawLine(renderer, cardX + 100, lineY, cardX + cw - 100, lineY);
            SDL_RenderDrawLine(renderer, cardX + 100, lineY + 1, cardX + cw - 100, lineY + 1);
            /* Center diamond */
            int dmx = WINDOW_WIDTH / 2;
            SDL_SetRenderDrawColor(renderer, 0, 220, 255, 255);
            for (int d = 0; d < 6; d++) {
                SDL_RenderDrawPoint(renderer, dmx - d, lineY - (5 - d));
                SDL_RenderDrawPoint(renderer, dmx + d, lineY - (5 - d));
                SDL_RenderDrawPoint(renderer, dmx - d, lineY + (5 - d) + 2);
                SDL_RenderDrawPoint(renderer, dmx + d, lineY + (5 - d) + 2);
            }

            /* Subtitle */
            renderTextCenteredBold(renderer, fontLarge, "Exponential Equation: e^x - ax - b = 0",
                                   WINDOW_WIDTH / 2, cardY + 168, (SDL_Color){140, 200, 245, 255});

            /* Course info */
            SDL_Color infoColor = {120, 175, 230, 255};
            renderTextCentered(renderer, fontMedium, "MT211 - Numerical Method  |  Semestral Project",
                               WINDOW_WIDTH / 2, cardY + 215, infoColor);

            /* Author box */
            SDL_SetRenderDrawColor(renderer, 25, 45, 90, 255);
            SDL_Rect authBg = {cardX + 100, cardY + 250, cw - 200, 80};
            SDL_RenderFillRect(renderer, &authBg);
            SDL_SetRenderDrawColor(renderer, 55, 120, 210, 255);
            SDL_RenderDrawRect(renderer, &authBg);

            renderTextCenteredBold(renderer, fontMedium, "Submitted By",
                                   WINDOW_WIDTH / 2, cardY + 258, (SDL_Color){80, 180, 255, 255});
            renderTextCenteredBold(renderer, fontLarge, "Khurt Goyena",
                                   WINDOW_WIDTH / 2, cardY + 282, (SDL_Color){200, 230, 255, 255});
            renderTextCentered(renderer, fontMedium, "BSCPE 22005",
                               WINDOW_WIDTH / 2, cardY + 312, infoColor);

            /* Start button */
            startBtn.rect = (SDL_Rect){(WINDOW_WIDTH - 240) / 2, cardY + 365, 240, 55};
            renderButton(renderer, fontLarge, &startBtn);

        /* ──────────────── LOADING SCREEN ──────────────── */
        } else if (currentScreen == SCREEN_LOADING) {
            int cw2 = 580, ch2 = 250;
            int lx = (WINDOW_WIDTH - cw2) / 2, ly = (WINDOW_HEIGHT - ch2) / 2;

            /* Card */
            SDL_SetRenderDrawColor(renderer, 18, 30, 62, 245);
            SDL_Rect lcard = {lx, ly, cw2, ch2};
            SDL_RenderFillRect(renderer, &lcard);
            SDL_SetRenderDrawColor(renderer, 55, 130, 235, 255);
            SDL_RenderDrawRect(renderer, &lcard);
            /* Top accent */
            SDL_SetRenderDrawColor(renderer, 40, 120, 230, 255);
            SDL_Rect ltop = {lx, ly, cw2, 5};
            SDL_RenderFillRect(renderer, &ltop);

            /* Title */
            renderTextCenteredBold(renderer, fontBig, "Initializing...",
                                   WINDOW_WIDTH / 2, ly + 30, (SDL_Color){80, 200, 255, 255});
            renderTextCentered(renderer, fontMedium, "Preparing the False Position Method Solver",
                               WINDOW_WIDTH / 2, ly + 80, (SDL_Color){110, 175, 235, 255});

            /* Progress bar */
            int barW = 420, barH = 30;
            int barX = (WINDOW_WIDTH - barW) / 2, barY = ly + 120;
            SDL_SetRenderDrawColor(renderer, 25, 40, 78, 255);
            SDL_Rect barBg = {barX, barY, barW, barH};
            SDL_RenderFillRect(renderer, &barBg);
            SDL_SetRenderDrawColor(renderer, 55, 120, 210, 255);
            SDL_RenderDrawRect(renderer, &barBg);

            int fillW = (int)(loadingProgress * (barW - 4));
            if (fillW > 0) {
                for (int px = 0; px < fillW; px++) {
                    float t = (float)px / (barW - 4);
                    int cr = (int)(30 + t * 10);
                    int cg = (int)(100 + t * 155);
                    int cb = (int)(200 + t * 55);
                    if (cb > 255) cb = 255;
                    SDL_SetRenderDrawColor(renderer, cr, cg, cb, 255);
                    SDL_RenderDrawLine(renderer, barX + 2 + px, barY + 2, barX + 2 + px, barY + barH - 3);
                }
            }

            /* Percentage */
            char pctText[20];
            sprintf(pctText, "%d%%", (int)(loadingProgress * 100));
            renderTextCenteredBold(renderer, font, pctText,
                                   WINDOW_WIDTH / 2, barY + 5, (SDL_Color){200, 235, 255, 255});

            /* Animated dots */
            int numDots = ((ticks / 400) % 4);
            char dots[10] = "";
            for (int d = 0; d < numDots; d++) strcat(dots, ".");
            char loadMsg[60];
            sprintf(loadMsg, "Loading systems%s", dots);
            renderTextCentered(renderer, fontSmall, loadMsg,
                               WINDOW_WIDTH / 2, barY + 48, (SDL_Color){90, 160, 220, 255});

            /* Spinning indicator */
            float angle = ticks * 0.003f;
            int spinCX = WINDOW_WIDTH / 2, spinCY = barY + 82;
            for (int i = 0; i < 8; i++) {
                float a2 = angle + i * (float)M_PI / 4.0f;
                int sx = spinCX + (int)(12 * cosf(a2));
                int sy = spinCY + (int)(12 * sinf(a2));
                int brightness = 80 + (int)(175.0f * ((i + (int)(ticks / 100)) % 8) / 8.0f);
                if (brightness > 255) brightness = 255;
                SDL_SetRenderDrawColor(renderer, 30, brightness / 2, brightness, 255);
                drawFilledCircle(renderer, sx, sy, 2);
            }

        /* ──────────────── SOLVER SCREEN ───────────────── */
        } else if (currentScreen == SCREEN_SOLVER) {
            /* ── Top Banner ── */
            SDL_SetRenderDrawColor(renderer, 60, 12, 35, 245);
            SDL_Rect banner = {0, 0, WINDOW_WIDTH, 70};
            SDL_RenderFillRect(renderer, &banner);
            /* Bottom glow line */
            SDL_SetRenderDrawColor(renderer, 210, 70, 120, 255);
            SDL_RenderDrawLine(renderer, 0, 70, WINDOW_WIDTH, 70);
            SDL_SetRenderDrawColor(renderer, 255, 100, 160, 255);
            SDL_Rect stripe = {0, 68, WINDOW_WIDTH, 2};
            SDL_RenderFillRect(renderer, &stripe);

            SDL_Color white = {255, 255, 255, 255};
            SDL_Color lightPink = {255, 185, 215, 255};
            renderTextBold(renderer, fontTitle, "FALSE POSITION METHOD", 20, 5, (SDL_Color){255, 130, 180, 255});
            renderText(renderer, fontLarge, "Exponential Equation: e^x - ax - b = 0", 30, 38, lightPink);

            renderText(renderer, fontSmall, "MT211 - Numerical Method  |  Semestral Project", 1030, 10, lightPink);
            renderText(renderer, fontSmall, "BSCPE 22005  |  Khurt Goyena", 1030, 32, lightPink);

            /* ── Left Panel (semi-transparent dark panel) ── */
            SDL_SetRenderDrawColor(renderer, 60, 12, 38, 225);
            SDL_Rect leftPanel = {15, 85, 310, 700};
            SDL_RenderFillRect(renderer, &leftPanel);
            SDL_SetRenderDrawColor(renderer, 200, 70, 120, 255);
            SDL_RenderDrawRect(renderer, &leftPanel);
            /* Top glow */
            SDL_SetRenderDrawColor(renderer, 225, 80, 135, 255);
            SDL_Rect lpTop = {15, 85, 310, 3};
            SDL_RenderFillRect(renderer, &lpTop);

            SDL_Color sectionColor = {255, 130, 175, 255};
            SDL_Color darkText  = {245, 185, 215, 255};

            renderTextBold(renderer, fontLarge, "INPUT", 125, 98, sectionColor);

            /* Formula box */
            SDL_SetRenderDrawColor(renderer, 80, 18, 45, 255);
            SDL_Rect formulaBox = {30, 130, 280, 50};
            SDL_RenderFillRect(renderer, &formulaBox);
            SDL_SetRenderDrawColor(renderer, 200, 70, 120, 255);
            SDL_RenderDrawRect(renderer, &formulaBox);
            renderText(renderer, fontMedium, "f(x) = e^x - ax - b", 50, 138, (SDL_Color){255, 150, 190, 255});
            renderText(renderer, fontSmall, "Find root where f(x) = 0", 55, 160, (SDL_Color){220, 120, 160, 255});

            inputs[0].rect = (SDL_Rect){140, 200, 150, 35};
            inputs[1].rect = (SDL_Rect){140, 250, 150, 35};
            inputs[2].rect = (SDL_Rect){140, 310, 150, 35};
            inputs[3].rect = (SDL_Rect){140, 360, 150, 35};

            /* Divider between coefficients and guesses */
            SDL_SetRenderDrawColor(renderer, 190, 65, 110, 255);
            SDL_RenderDrawLine(renderer, 35, 295, 310, 295);
            renderText(renderer, fontSmall, "Initial Guesses", 95, 290, (SDL_Color){240, 120, 165, 255});

            for (int i = 0; i < 4; i++) renderInputBox(renderer, font, &inputs[i]);

            computeBtn.rect = (SDL_Rect){30, 420, 130, 42};
            clearBtn.rect   = (SDL_Rect){180, 420, 130, 42};
            renderButton(renderer, font, &computeBtn);
            renderButton(renderer, font, &clearBtn);

            /* Status */
            renderTextBold(renderer, font, "STATUS", 125, 485, sectionColor);
            SDL_SetRenderDrawColor(renderer, 80, 18, 45, 255);
            SDL_Rect statusBox = {30, 510, 280, 55};
            SDL_RenderFillRect(renderer, &statusBox);
            SDL_SetRenderDrawColor(renderer, 200, 70, 120, 255);
            SDL_RenderDrawRect(renderer, &statusBox);

            if (strlen(resultText) > 0) {
                char resultCopy[500];
                strcpy(resultCopy, resultText);
                char* line = strtok(resultCopy, "\n");
                int ry = 517;
                while (line) {
                    SDL_Color resultColor = hasValidRoot ? (SDL_Color){50, 255, 150, 255} : (SDL_Color){255, 100, 100, 255};
                    renderText(renderer, fontSmall, line, 40, ry, resultColor);
                    ry += 18;
                    line = strtok(NULL, "\n");
                }
            }

            /* Conclusion */
            if (hasValidRoot) {
                renderTextBold(renderer, font, "CONCLUSION", 100, 585, sectionColor);
                SDL_SetRenderDrawColor(renderer, 75, 15, 42, 255);
                SDL_Rect concBox = {30, 610, 280, 105};
                SDL_RenderFillRect(renderer, &concBox);
                SDL_SetRenderDrawColor(renderer, 220, 80, 130, 255);
                SDL_RenderDrawRect(renderer, &concBox);

                char buffer[200];
                SDL_Color concColor = {255, 160, 200, 255};
                formatEquation(buffer, (int)coefA, (int)coefB);
                renderTextBold(renderer, fontSmall, buffer, 40, 620, concColor);
                sprintf(buffer, "Root: x = %.6lf", finalRoot);
                renderTextBold(renderer, font, buffer, 40, 645, (SDL_Color){255, 210, 230, 255});
                sprintf(buffer, "Iterations: %d", totalIterations);
                renderText(renderer, fontSmall, buffer, 40, 675, darkText);
                sprintf(buffer, "Tolerance: %.4lf", TOLERANCE);
                renderText(renderer, fontSmall, buffer, 40, 695, darkText);
            }

            /* ── Center Panel: Iteration Table ── */
            SDL_SetRenderDrawColor(renderer, 60, 12, 38, 225);
            SDL_Rect centerPanel = {340, 85, 620, 700};
            SDL_RenderFillRect(renderer, &centerPanel);
            SDL_SetRenderDrawColor(renderer, 200, 70, 120, 255);
            SDL_RenderDrawRect(renderer, &centerPanel);
            SDL_SetRenderDrawColor(renderer, 225, 80, 135, 255);
            SDL_Rect cpTop = {340, 85, 620, 3};
            SDL_RenderFillRect(renderer, &cpTop);

            renderTextBold(renderer, fontLarge, "ITERATION TABLE", 555, 98, sectionColor);

            if (totalIterations > 0) {
                /* Table header */
                SDL_SetRenderDrawColor(renderer, 110, 28, 70, 255);
                SDL_Rect tableHeader = {355, 130, 590, 28};
                SDL_RenderFillRect(renderer, &tableHeader);
                SDL_SetRenderDrawColor(renderer, 210, 75, 125, 255);
                SDL_RenderDrawRect(renderer, &tableHeader);

                SDL_Color hdrColor = {255, 200, 225, 255};
                renderTextBold(renderer, fontSmall, "n",    365, 134, hdrColor);
                renderTextBold(renderer, fontSmall, "x0",   415, 134, hdrColor);
                renderTextBold(renderer, fontSmall, "x1",   520, 134, hdrColor);
                renderTextBold(renderer, fontSmall, "x2",   630, 134, hdrColor);
                renderTextBold(renderer, fontSmall, "f(x2)",740, 134, hdrColor);
                renderTextBold(renderer, fontSmall, "Error", 850, 134, hdrColor);

                int maxVisibleRows = 20;
                int startRow = tableScrollOffset;
                int endRow = startRow + maxVisibleRows;
                if (endRow > totalIterations) endRow = totalIterations;

                for (int i = startRow; i < endRow; i++) {
                    int di = i - startRow;
                    int y = 162 + di * 25;

                    if (i % 2 == 0)
                        SDL_SetRenderDrawColor(renderer, 60, 12, 38, 255);
                    else
                        SDL_SetRenderDrawColor(renderer, 75, 18, 48, 255);
                    SDL_Rect row = {355, y, 590, 25};
                    SDL_RenderFillRect(renderer, &row);
                    /* Subtle row border */
                    SDL_SetRenderDrawColor(renderer, 130, 40, 80, 255);
                    SDL_RenderDrawLine(renderer, 355, y + 24, 945, y + 24);

                    SDL_Color tc = {255, 190, 215, 255};
                    char buf[50];
                    sprintf(buf, "%d", iterations[i].iteration);
                    renderText(renderer, fontSmall, buf, 365, y + 4, tc);
                    sprintf(buf, "%.4lf", iterations[i].x0);
                    renderText(renderer, fontSmall, buf, 405, y + 4, tc);
                    sprintf(buf, "%.4lf", iterations[i].x1);
                    renderText(renderer, fontSmall, buf, 510, y + 4, tc);
                    sprintf(buf, "%.4lf", iterations[i].x2);
                    renderText(renderer, fontSmall, buf, 620, y + 4, tc);
                    sprintf(buf, "%.5lf", iterations[i].fx2);
                    renderText(renderer, fontSmall, buf, 725, y + 4, tc);
                    sprintf(buf, "%.5lf", iterations[i].error);
                    renderText(renderer, fontSmall, buf, 840, y + 4, tc);
                }

                /* Scrollbar */
                if (totalIterations > maxVisibleRows) {
                    int sbX = 950, sbY = 162, sbH = maxVisibleRows * 25;
                    SDL_SetRenderDrawColor(renderer, 85, 22, 50, 255);
                    SDL_Rect track = {sbX, sbY, 8, sbH};
                    SDL_RenderFillRect(renderer, &track);
                    float thumbRatio = (float)maxVisibleRows / totalIterations;
                    int thumbH = (int)(sbH * thumbRatio);
                    if (thumbH < 20) thumbH = 20;
                    float scrollRatio = (float)tableScrollOffset / (totalIterations - maxVisibleRows);
                    int thumbY = sbY + (int)((sbH - thumbH) * scrollRatio);
                    SDL_SetRenderDrawColor(renderer, 240, 90, 145, 255);
                    SDL_Rect thumb = {sbX, thumbY, 8, thumbH};
                    SDL_RenderFillRect(renderer, &thumb);
                }
            } else {
                renderText(renderer, font, "Enter coefficients and press COMPUTE", 440, 400, (SDL_Color){220, 100, 150, 255});
                renderText(renderer, font, "to see iteration results here.", 480, 428, (SDL_Color){220, 100, 150, 255});
            }

            /* ── Right Panel: Graph ── */
            SDL_SetRenderDrawColor(renderer, 60, 12, 38, 225);
            SDL_Rect rightPanel = {975, 85, 410, 700};
            SDL_RenderFillRect(renderer, &rightPanel);
            SDL_SetRenderDrawColor(renderer, 200, 70, 120, 255);
            SDL_RenderDrawRect(renderer, &rightPanel);
            SDL_SetRenderDrawColor(renderer, 225, 80, 135, 255);
            SDL_Rect rpTop = {975, 85, 410, 3};
            SDL_RenderFillRect(renderer, &rpTop);

            renderTextBold(renderer, fontLarge, "GRAPH", 1140, 98, sectionColor);
            renderText(renderer, fontSmall, "f(x) = e^x - ax - b", 1100, 125, (SDL_Color){230, 120, 165, 255});

            drawGraph(renderer, fontSmall, coefA, coefB, finalRoot, hasValidRoot);

            /* Legend */
            int legendY = 680;
            SDL_SetRenderDrawColor(renderer, 80, 18, 48, 255);
            SDL_Rect legendBox = {990, legendY, 380, 85};
            SDL_RenderFillRect(renderer, &legendBox);
            SDL_SetRenderDrawColor(renderer, 200, 70, 120, 255);
            SDL_RenderDrawRect(renderer, &legendBox);
            renderTextBold(renderer, fontMedium, "LEGEND", 1140, legendY + 6, sectionColor);

            /* Curve line sample */
            SDL_SetRenderDrawColor(renderer, 255, 60, 160, 255);
            SDL_Rect cl = {1010, legendY + 35, 25, 3};
            SDL_RenderFillRect(renderer, &cl);
            renderText(renderer, fontSmall, "f(x) curve", 1045, legendY + 30, (SDL_Color){255, 60, 160, 255});

            /* Root dot sample */
            SDL_SetRenderDrawColor(renderer, 255, 240, 60, 255);
            drawFilledCircle(renderer, 1022, legendY + 62, 5);
            renderText(renderer, fontSmall, "Approximate root", 1045, legendY + 55, (SDL_Color){255, 240, 60, 255});
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    TTF_CloseFont(font);
    TTF_CloseFont(fontSmall);
    TTF_CloseFont(fontMedium);
    TTF_CloseFont(fontLarge);
    TTF_CloseFont(fontTitle);
    TTF_CloseFont(fontHuge);
    TTF_CloseFont(fontBig);
    TTF_CloseFont(fontStep);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
