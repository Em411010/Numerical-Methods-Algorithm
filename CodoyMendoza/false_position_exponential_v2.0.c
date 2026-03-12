#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define MAX_ITER 100
#define TOLERANCE 0.0001
#define WINDOW_WIDTH 1400
#define WINDOW_HEIGHT 800

#define SCREEN_LANDING 0
#define SCREEN_LOADING 1
#define SCREEN_SOLVER  2

#define NUM_BEARS 6

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
    int isPanda;       /* 0 = pink bear, 1 = panda */
    int facingRight;
    int action;        /* 0 = walk, 1 = bounce */
} Bear;

/* â”€â”€â”€ Math â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

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

/* â”€â”€â”€ Primitive Helpers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

void drawFilledCircle(SDL_Renderer* r, int cx, int cy, int rad) {
    for (int dy = -rad; dy <= rad; dy++)
        for (int dx = -rad; dx <= rad; dx++)
            if (dx*dx + dy*dy <= rad*rad)
                SDL_RenderDrawPoint(r, cx+dx, cy+dy);
}

void drawFilledEllipse(SDL_Renderer* r, int cx, int cy, int rx, int ry) {
    for (int dy = -ry; dy <= ry; dy++)
        for (int dx = -rx; dx <= rx; dx++)
            if ((float)(dx*dx)/(rx*rx) + (float)(dy*dy)/(ry*ry) <= 1.0f)
                SDL_RenderDrawPoint(r, cx+dx, cy+dy);
}

/* â”€â”€â”€ Bear / Panda Drawing â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

void drawBear(SDL_Renderer* renderer, int cx, int cy, float scale, int isPanda, Uint32 ticks, float bobPhase) {
    float bob = sinf(ticks * 0.0025f + bobPhase) * 5.0f * scale;
    int by = cy + (int)bob;

    /* Colours */
    int br, bg, bb;           /* body */
    int er, eg, eb;           /* ear */
    int nr, ng, nb;           /* nose/inner */
    if (isPanda) {
        br = 255; bg = 240; bb = 245;   /* white */
        er = 30;  eg = 30;  eb = 30;    /* black ear patches */
        nr = 30;  ng = 30;  nb = 30;
    } else {
        br = 255; bg = 130; bb = 175;   /* flamingo pink body */
        er = 230; eg = 80;  eb = 140;   /* deeper pink ears */
        nr = 200; ng = 50;  nb = 110;
    }

    int bw = (int)(46 * scale);
    int bh = (int)(40 * scale);
    int headR = (int)(22 * scale);
    int earR  = (int)(10 * scale);

    /* â”€â”€ Legs â”€â”€ */
    int legW = (int)(11 * scale), legH = (int)(14 * scale);
    float legWiggle = sinf(ticks * 0.005f + bobPhase) * 3.5f * scale;
    /* left leg */
    SDL_SetRenderDrawColor(renderer, er, eg, eb, 255);
    int llx = cx - bw/3 + (int)legWiggle;
    drawFilledEllipse(renderer, llx, by + bh/2 + legH/2, legW/2, legH/2);
    /* right leg */
    int rlx = cx + bw/3 + (int)(-legWiggle);
    drawFilledEllipse(renderer, rlx, by + bh/2 + legH/2, legW/2, legH/2);

    /* â”€â”€ Body â”€â”€ */
    SDL_SetRenderDrawColor(renderer, br, bg, bb, 255);
    drawFilledEllipse(renderer, cx, by, bw/2, bh/2);

    /* Tummy highlight */
    if (!isPanda) {
        SDL_SetRenderDrawColor(renderer, 255, 200, 225, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    }
    drawFilledEllipse(renderer, cx, by + (int)(4*scale), (int)(12*scale), (int)(10*scale));

    /* â”€â”€ Arms â”€â”€ */
    float armSwing = sinf(ticks * 0.003f + bobPhase + 1.2f) * 6.0f * scale;
    int armW = (int)(9*scale), armH = (int)(16*scale);
    SDL_SetRenderDrawColor(renderer, er, eg, eb, 255);
    /* left arm */
    drawFilledEllipse(renderer, cx - bw/2 - armW/2 + (int)(2*scale),
                      by - bh/5 + (int)armSwing, armW/2, armH/2);
    /* right arm */
    drawFilledEllipse(renderer, cx + bw/2 + armW/2 - (int)(2*scale),
                      by - bh/5 + (int)(-armSwing), armW/2, armH/2);

    /* â”€â”€ Head â”€â”€ */
    int headY = by - bh/2 - headR + (int)(4*scale);
    /* Panda eye patches */
    if (isPanda) {
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        drawFilledEllipse(renderer, cx - (int)(9*scale), headY - (int)(3*scale),
                          (int)(7*scale), (int)(6*scale));
        drawFilledEllipse(renderer, cx + (int)(9*scale), headY - (int)(3*scale),
                          (int)(7*scale), (int)(6*scale));
    }
    /* Head */
    SDL_SetRenderDrawColor(renderer, br, bg, bb, 255);
    drawFilledCircle(renderer, cx, headY, headR);

    /* â”€â”€ Ears â”€â”€ */
    int earLX = cx - headR + (int)(4*scale);
    int earRX = cx + headR - (int)(4*scale);
    int earY  = headY - headR + (int)(2*scale);
    SDL_SetRenderDrawColor(renderer, er, eg, eb, 255);
    drawFilledCircle(renderer, earLX, earY, earR);
    drawFilledCircle(renderer, earRX, earY, earR);
    /* Inner ear */
    SDL_SetRenderDrawColor(renderer, 255, 180, 210, 255);
    drawFilledCircle(renderer, earLX, earY, (int)(earR * 0.55f));
    drawFilledCircle(renderer, earRX, earY, (int)(earR * 0.55f));
    /* Re-draw head on top of ears */
    SDL_SetRenderDrawColor(renderer, br, bg, bb, 255);
    drawFilledCircle(renderer, cx, headY, headR);

    /* â”€â”€ Face â”€â”€ */
    /* Eyes */
    int eyeY = headY - (int)(4*scale);
    int eyeSep = (int)(8*scale);
    SDL_SetRenderDrawColor(renderer, 30, 15, 20, 255);
    drawFilledCircle(renderer, cx - eyeSep, eyeY, (int)(4*scale));
    drawFilledCircle(renderer, cx + eyeSep, eyeY, (int)(4*scale));
    /* Eye shine */
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    drawFilledCircle(renderer, cx - eyeSep + (int)(2*scale), eyeY - (int)(1*scale), (int)(1.5f*scale));
    drawFilledCircle(renderer, cx + eyeSep + (int)(2*scale), eyeY - (int)(1*scale), (int)(1.5f*scale));
    /* Nose */
    SDL_SetRenderDrawColor(renderer, nr, ng, nb, 255);
    drawFilledEllipse(renderer, cx, headY + (int)(5*scale), (int)(4*scale), (int)(3*scale));
    /* Blush cheeks */
    SDL_SetRenderDrawColor(renderer, 255, 160, 195, 180);
    drawFilledEllipse(renderer, cx - (int)(12*scale), headY + (int)(5*scale), (int)(6*scale), (int)(4*scale));
    drawFilledEllipse(renderer, cx + (int)(12*scale), headY + (int)(5*scale), (int)(6*scale), (int)(4*scale));
    /* Smile */
    SDL_SetRenderDrawColor(renderer, nr, ng, nb, 255);
    for (int i = -(int)(5*scale); i <= (int)(5*scale); i++) {
        int smileY = headY + (int)(9*scale) + (i*i) / (int)(8*scale + 1);
        SDL_RenderDrawPoint(renderer, cx + i, smileY);
        SDL_RenderDrawPoint(renderer, cx + i, smileY + 1);
    }
}

/* â”€â”€â”€ Text Helpers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

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

/* â”€â”€â”€ Background â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

void drawBackground(SDL_Renderer* renderer, Bear bears[], Uint32 ticks) {
    /* Flamingo pink gradient: top deep rose â†’ bottom light blush */
    for (int y = 0; y < WINDOW_HEIGHT; y++) {
        float t = (float)y / WINDOW_HEIGHT;
        int r = (int)(215 + t * 35);   /* 215â†’250 */
        int g = (int)(60  + t * 120);  /* 60â†’180  */
        int b = (int)(100 + t * 100);  /* 100â†’200 */
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderDrawLine(renderer, 0, y, WINDOW_WIDTH, y);
    }
    /* Soft grid */
    SDL_SetRenderDrawColor(renderer, 240, 120, 165, 40);
    for (int x = 0; x < WINDOW_WIDTH; x += 60)
        SDL_RenderDrawLine(renderer, x, 0, x, WINDOW_HEIGHT);
    for (int y = 0; y < WINDOW_HEIGHT; y += 60)
        SDL_RenderDrawLine(renderer, 0, y, WINDOW_WIDTH, y);

    /* Ground strip */
    SDL_SetRenderDrawColor(renderer, 220, 85, 140, 255);
    SDL_Rect ground = {0, WINDOW_HEIGHT - 65, WINDOW_WIDTH, 65};
    SDL_RenderFillRect(renderer, &ground);
    SDL_SetRenderDrawColor(renderer, 255, 160, 200, 255);
    SDL_Rect gline = {0, WINDOW_HEIGHT - 65, WINDOW_WIDTH, 3};
    SDL_RenderFillRect(renderer, &gline);

    /* Floating hearts / sparkles */
    for (int i = 0; i < 22; i++) {
        float px = (float)((i * 173 + (int)(ticks * 0.015f)) % WINDOW_WIDTH);
        float py = (float)((i * 97  + (int)(ticks * 0.006f * (i % 3 + 1))) % (WINDOW_HEIGHT - 80));
        int v = 200 + (i * 17) % 55;
        SDL_SetRenderDrawColor(renderer, 255, v/2 + 80, v, 200);
        drawFilledCircle(renderer, (int)px, (int)py, 2 + i % 3);
    }

    /* Bears / pandas */
    for (int i = 0; i < NUM_BEARS; i++)
        drawBear(renderer, (int)bears[i].x, (int)bears[i].y,
                 bears[i].scale, bears[i].isPanda, ticks, bears[i].bobPhase);
}

/* â”€â”€â”€ UI Components â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

void renderInputBox(SDL_Renderer* renderer, TTF_Font* font, InputBox* box) {
    SDL_Color labelColor = {200, 60, 110, 255};
    renderTextBold(renderer, font, box->label, box->rect.x - 80, box->rect.y + 5, labelColor);
    if (box->active)
        SDL_SetRenderDrawColor(renderer, 100, 15, 50, 255);
    else
        SDL_SetRenderDrawColor(renderer, 75, 10, 40, 255);
    SDL_RenderFillRect(renderer, &box->rect);
    if (box->active)
        SDL_SetRenderDrawColor(renderer, 255, 105, 160, 255);
    else
        SDL_SetRenderDrawColor(renderer, 210, 75, 130, 255);
    SDL_RenderDrawRect(renderer, &box->rect);
    SDL_Color textColor = {255, 205, 230, 255};
    if (strlen(box->value) > 0)
        renderText(renderer, font, box->value, box->rect.x + 5, box->rect.y + 5, textColor);
}

void renderButton(SDL_Renderer* renderer, TTF_Font* font, Button* btn) {
    /* Shadow */
    SDL_SetRenderDrawColor(renderer, 130, 20, 65, 255);
    SDL_Rect shadow = {btn->rect.x + 3, btn->rect.y + 3, btn->rect.w, btn->rect.h};
    SDL_RenderFillRect(renderer, &shadow);
    /* Face */
    if (btn->clicked)
        SDL_SetRenderDrawColor(renderer, 160, 40, 90, 255);
    else if (btn->hovered)
        SDL_SetRenderDrawColor(renderer, 255, 100, 160, 255);
    else
        SDL_SetRenderDrawColor(renderer, 220, 70, 130, 255);
    SDL_RenderFillRect(renderer, &btn->rect);
    /* Highlight */
    SDL_SetRenderDrawColor(renderer, 255, 170, 205, 255);
    SDL_RenderDrawLine(renderer, btn->rect.x+1, btn->rect.y,
                       btn->rect.x + btn->rect.w - 2, btn->rect.y);
    /* Border */
    SDL_SetRenderDrawColor(renderer, 170, 50, 95, 255);
    SDL_RenderDrawRect(renderer, &btn->rect);
    /* Text */
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(font, btn->text, textColor);
    if (surface) {
        int tx = btn->rect.x + (btn->rect.w - surface->w) / 2;
        int ty = btn->rect.y + (btn->rect.h - surface->h) / 2;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_Rect tr = {tx, ty, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, NULL, &tr);
        SDL_FreeSurface(surface);
        SDL_DestroyTexture(texture);
    }
}

/* â”€â”€â”€ Graph â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

void drawGraph(SDL_Renderer* renderer, TTF_Font* fontSmall, double a, double b, double root, int hasRoot) {
    int graphX=980, graphY=220, graphW=400, graphH=440, scale=50;
    int centerX = graphX + graphW/2, centerY = graphY + graphH/2;

    /* Background */
    SDL_SetRenderDrawColor(renderer, 70, 10, 40, 255);
    SDL_Rect gr = {graphX, graphY, graphW, graphH};
    SDL_RenderFillRect(renderer, &gr);
    SDL_SetRenderDrawColor(renderer, 220, 80, 140, 255);
    SDL_RenderDrawRect(renderer, &gr);

    /* Grid */
    SDL_SetRenderDrawColor(renderer, 120, 35, 70, 255);
    for (int i = graphX; i <= graphX+graphW; i += 50)
        SDL_RenderDrawLine(renderer, i, graphY, i, graphY+graphH);
    for (int i = graphY; i <= graphY+graphH; i += 50)
        SDL_RenderDrawLine(renderer, graphX, i, graphX+graphW, i);

    /* Axes */
    SDL_SetRenderDrawColor(renderer, 255, 120, 175, 255);
    SDL_RenderDrawLine(renderer, centerX, graphY, centerX, graphY+graphH);
    SDL_RenderDrawLine(renderer, graphX, centerY, graphX+graphW, centerY);
    renderText(renderer, fontSmall, "x", graphX+graphW-15, centerY+5, (SDL_Color){255,180,215,255});
    renderText(renderer, fontSmall, "y", centerX+5,        graphY+5,  (SDL_Color){255,180,215,255});

    /* Tick marks */
    SDL_SetRenderDrawColor(renderer, 255, 120, 175, 255);
    for (int i = graphX; i <= graphX+graphW; i += 50)
        if (i != centerX) SDL_RenderDrawLine(renderer, i, centerY-3, i, centerY+3);
    for (int i = graphY; i <= graphY+graphH; i += 50)
        if (i != centerY) SDL_RenderDrawLine(renderer, centerX-3, i, centerX+3, i);

    /* Curve (hot pink) */
    SDL_SetRenderDrawColor(renderer, 255, 70, 170, 255);
    for (int px = graphX; px < graphX+graphW; px++) {
        double x = (px - centerX) / (double)scale;
        double y = f(x, a, b);
        int py = centerY - (int)(y * 20);
        if (py >= graphY && py < graphY+graphH && fabs(y) < 50) {
            SDL_RenderDrawPoint(renderer, px, py);
            SDL_RenderDrawPoint(renderer, px, py+1);
        }
    }

    /* Root dot */
    if (hasRoot) {
        int rx = centerX + (int)(root * scale);
        SDL_SetRenderDrawColor(renderer, 255, 210, 90, 255);
        for (int i=-12; i<=12; i++) for (int j=-12; j<=12; j++)
            if (i*i+j*j<=144 && i*i+j*j>64) SDL_RenderDrawPoint(renderer, rx+i, centerY+j);
        SDL_SetRenderDrawColor(renderer, 255, 245, 70, 255);
        for (int i=-8; i<=8; i++) for (int j=-8; j<=8; j++)
            if (i*i+j*j<=64) SDL_RenderDrawPoint(renderer, rx+i, centerY+j);
    }
}

/* â”€â”€â”€ MAIN â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window* window = SDL_CreateWindow("False Position Method - Exponential",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    /* Fonts */
    TTF_Font* font      = TTF_OpenFont("font.ttf", 18);
    TTF_Font* fontSmall = TTF_OpenFont("font.ttf", 14);
    TTF_Font* fontMedium= TTF_OpenFont("font.ttf", 16);
    TTF_Font* fontLarge = TTF_OpenFont("font.ttf", 20);
    TTF_Font* fontTitle = TTF_OpenFont("font.ttf", 24);
    TTF_Font* fontHuge  = TTF_OpenFont("font.ttf", 44);
    TTF_Font* fontBig   = TTF_OpenFont("font.ttf", 32);
    if (!font || !fontSmall || !fontMedium || !fontLarge || !fontTitle || !fontHuge || !fontBig) {
        printf("Error loading font: %s\n", TTF_GetError());
        return 1;
    }

    /* Bears â€” 4 pink bears + 2 pandas */
    Bear bears[NUM_BEARS] = {
        {100,  710, 0.25f, 0.85f, 0.0f, 0, 1, 0},
        {320,  720, 0.20f, 0.75f, 1.2f, 1, 1, 1},  /* panda */
        {610,  705, 0.22f, 0.80f, 2.4f, 0, 0, 0},
        {870,  715, 0.18f, 0.90f, 0.8f, 1, 0, 1},  /* panda */
        {1120, 700, 0.24f, 0.70f, 1.6f, 0, 1, 0},
        {1330, 710, 0.20f, 0.95f, 3.0f, 0, 0, 1},
    };

    /* Screen state */
    int currentScreen = SCREEN_LANDING;
    float loadingProgress = 0.0f;
    int loadingDoneDelay = 0;

    /* Landing button */
    Button startBtn = {{0, 0, 240, 55}, "LAUNCH SOLVER", 0, 0};

    /* Solver inputs (4: a, b, x0, x1) */
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
    char resultText[500] = "Enter values and initial guesses,\nthen press COMPUTE";
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
                            sprintf(resultText, "ERROR: f(x0) and f(x1) must\nhave opposite signs!\nf(%.2f)=%.4f, f(%.2f)=%.4f",
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
                                else               { x0 = x2; fx0 = fx2; }
                            }
                            if (!hasValidRoot)
                                sprintf(resultText, "FAILED: Did not converge\nTry different initial guesses");
                        }
                    }
                    if (mx >= clearBtn.rect.x && mx <= clearBtn.rect.x + clearBtn.rect.w &&
                        my >= clearBtn.rect.y && my <= clearBtn.rect.y + clearBtn.rect.h) {
                        for (int i = 0; i < 4; i++) strcpy(inputs[i].value, "");
                        strcpy(resultText, "Enter values and initial guesses,\nthen press COMPUTE");
                        hasValidRoot = 0; totalIterations = 0; tableScrollOffset = 0;
                        clearBtn.clicked = 1;
                    }
                }
                if (e.type == SDL_MOUSEBUTTONUP) { computeBtn.clicked = 0; clearBtn.clicked = 0; }
                if (e.type == SDL_MOUSEMOTION) {
                    int mx = e.motion.x, my = e.motion.y;
                    computeBtn.hovered = (mx >= computeBtn.rect.x && mx <= computeBtn.rect.x + computeBtn.rect.w &&
                                          my >= computeBtn.rect.y && my <= computeBtn.rect.y + computeBtn.rect.h);
                    clearBtn.hovered   = (mx >= clearBtn.rect.x   && mx <= clearBtn.rect.x + clearBtn.rect.w &&
                                          my >= clearBtn.rect.y   && my <= clearBtn.rect.y + clearBtn.rect.h);
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
                if (e.type == SDL_MOUSEWHEEL && totalIterations > 0) {
                    tableScrollOffset -= e.wheel.y * 2;
                    if (tableScrollOffset < 0) tableScrollOffset = 0;
                    int maxScroll = totalIterations - 15;
                    if (maxScroll < 0) maxScroll = 0;
                    if (tableScrollOffset > maxScroll) tableScrollOffset = maxScroll;
                }
            }
        }

        /* â”€â”€ Animate bears â”€â”€ */
        for (int i = 0; i < NUM_BEARS; i++) {
            if (bears[i].facingRight) {
                bears[i].x += bears[i].speed;
                if (bears[i].x > WINDOW_WIDTH + 80) bears[i].x = -80.0f;
            } else {
                bears[i].x -= bears[i].speed;
                if (bears[i].x < -80) bears[i].x = WINDOW_WIDTH + 80.0f;
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

        /* â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• RENDER â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• */
        drawBackground(renderer, bears, ticks);

        /* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ LANDING SCREEN â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
        if (currentScreen == SCREEN_LANDING) {
            int cw = 760, ch = 460;
            int cardX = (WINDOW_WIDTH - cw) / 2, cardY = (WINDOW_HEIGHT - ch) / 2 - 40;

            /* Card shadow */
            SDL_SetRenderDrawColor(renderer, 130, 15, 60, 180);
            SDL_Rect cardShadow = {cardX + 5, cardY + 5, cw, ch};
            SDL_RenderFillRect(renderer, &cardShadow);
            /* Card bg */
            SDL_SetRenderDrawColor(renderer, 80, 5, 35, 245);
            SDL_Rect card = {cardX, cardY, cw, ch};
            SDL_RenderFillRect(renderer, &card);
            /* Borders */
            SDL_SetRenderDrawColor(renderer, 255, 100, 165, 255);
            SDL_RenderDrawRect(renderer, &card);
            SDL_Rect inner = {cardX+3, cardY+3, cw-6, ch-6};
            SDL_SetRenderDrawColor(renderer, 210, 70, 130, 255);
            SDL_RenderDrawRect(renderer, &inner);
            /* Top accent */
            SDL_SetRenderDrawColor(renderer, 255, 100, 165, 255);
            SDL_Rect topBar = {cardX, cardY, cw, 8};
            SDL_RenderFillRect(renderer, &topBar);
            /* Bottom accent */
            SDL_SetRenderDrawColor(renderer, 255, 160, 200, 255);
            SDL_Rect botBar = {cardX, cardY+ch-4, cw, 4};
            SDL_RenderFillRect(renderer, &botBar);

            /* Corner brackets */
            SDL_SetRenderDrawColor(renderer, 255, 160, 200, 255);
            SDL_RenderDrawLine(renderer, cardX+12, cardY+15, cardX+12, cardY+35);
            SDL_RenderDrawLine(renderer, cardX+12, cardY+15, cardX+32, cardY+15);
            SDL_RenderDrawLine(renderer, cardX+cw-12, cardY+15, cardX+cw-12, cardY+35);
            SDL_RenderDrawLine(renderer, cardX+cw-12, cardY+15, cardX+cw-32, cardY+15);
            SDL_RenderDrawLine(renderer, cardX+12, cardY+ch-15, cardX+12, cardY+ch-35);
            SDL_RenderDrawLine(renderer, cardX+12, cardY+ch-15, cardX+32, cardY+ch-15);
            SDL_RenderDrawLine(renderer, cardX+cw-12, cardY+ch-15, cardX+cw-12, cardY+ch-35);
            SDL_RenderDrawLine(renderer, cardX+cw-12, cardY+ch-15, cardX+cw-32, cardY+ch-15);

            /* Heart deco top */
            SDL_SetRenderDrawColor(renderer, 255, 120, 175, 255);
            drawFilledCircle(renderer, cardX + 60,      cardY+50, 8);
            drawFilledCircle(renderer, cardX + cw - 60, cardY+50, 8);

            /* Titles */
            renderTextCenteredBold(renderer, fontHuge, "FALSE POSITION",
                                   WINDOW_WIDTH/2, cardY+45, (SDL_Color){255,160,205,255});
            renderTextCenteredBold(renderer, fontBig,  "METHOD",
                                   WINDOW_WIDTH/2, cardY+100, (SDL_Color){255,130,185,255});

            /* Divider */
            int lineY = cardY + 150;
            SDL_SetRenderDrawColor(renderer, 220, 80, 140, 255);
            SDL_RenderDrawLine(renderer, cardX+100, lineY, cardX+cw-100, lineY);
            SDL_RenderDrawLine(renderer, cardX+100, lineY+1, cardX+cw-100, lineY+1);
            /* Diamond */
            int dmx = WINDOW_WIDTH/2;
            SDL_SetRenderDrawColor(renderer, 255, 140, 190, 255);
            for (int d=0; d<6; d++) {
                SDL_RenderDrawPoint(renderer, dmx-d, lineY-(5-d));
                SDL_RenderDrawPoint(renderer, dmx+d, lineY-(5-d));
                SDL_RenderDrawPoint(renderer, dmx-d, lineY+(5-d)+2);
                SDL_RenderDrawPoint(renderer, dmx+d, lineY+(5-d)+2);
            }

            renderTextCenteredBold(renderer, fontLarge, "Exponential Equation: e^x - ax - b = 0",
                                   WINDOW_WIDTH/2, cardY+168, (SDL_Color){255,190,220,255});
            renderTextCentered(renderer, fontMedium, "MT211 - Numerical Method  |  Semestral Project",
                               WINDOW_WIDTH/2, cardY+215, (SDL_Color){240,150,190,255});

            /* Author box */
            SDL_SetRenderDrawColor(renderer, 100, 10, 45, 255);
            SDL_Rect authBg = {cardX+100, cardY+250, cw-200, 95};
            SDL_RenderFillRect(renderer, &authBg);
            SDL_SetRenderDrawColor(renderer, 235, 95, 155, 255);
            SDL_RenderDrawRect(renderer, &authBg);

            renderTextCenteredBold(renderer, fontMedium, "Submitted By",
                                   WINDOW_WIDTH/2, cardY+258, (SDL_Color){255,150,200,255});
            renderTextCenteredBold(renderer, fontLarge,  "Kerlstein Aleizon Codoy",
                                   WINDOW_WIDTH/2, cardY+280, (SDL_Color){255,220,235,255});
            renderTextCenteredBold(renderer, fontLarge,  "Maria Angela Mendoza",
                                   WINDOW_WIDTH/2, cardY+304, (SDL_Color){255,220,235,255});
            renderTextCentered(renderer, fontMedium, "BSCPE 22001",
                               WINDOW_WIDTH/2, cardY+330, (SDL_Color){240,150,190,255});

            /* Start button */
            startBtn.rect = (SDL_Rect){(WINDOW_WIDTH-240)/2, cardY+380, 240, 55};
            renderButton(renderer, fontLarge, &startBtn);

        /* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ LOADING SCREEN â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
        } else if (currentScreen == SCREEN_LOADING) {
            int cw2 = 580, ch2 = 260;
            int lx = (WINDOW_WIDTH-cw2)/2, ly = (WINDOW_HEIGHT-ch2)/2;

            SDL_SetRenderDrawColor(renderer, 80, 5, 35, 245);
            SDL_Rect lcard = {lx, ly, cw2, ch2};
            SDL_RenderFillRect(renderer, &lcard);
            SDL_SetRenderDrawColor(renderer, 255, 100, 165, 255);
            SDL_RenderDrawRect(renderer, &lcard);
            SDL_SetRenderDrawColor(renderer, 255, 100, 165, 255);
            SDL_Rect ltop = {lx, ly, cw2, 5};
            SDL_RenderFillRect(renderer, &ltop);

            renderTextCenteredBold(renderer, fontBig, "Loading...",
                                   WINDOW_WIDTH/2, ly+30, (SDL_Color){255,160,205,255});
            renderTextCentered(renderer, fontMedium, "Preparing the False Position Method Solver",
                               WINDOW_WIDTH/2, ly+80, (SDL_Color){255,195,225,255});

            /* Progress bar */
            int barW=420, barH=30;
            int barX=(WINDOW_WIDTH-barW)/2, barY=ly+118;
            SDL_SetRenderDrawColor(renderer, 100, 10, 45, 255);
            SDL_Rect barBg = {barX, barY, barW, barH};
            SDL_RenderFillRect(renderer, &barBg);
            SDL_SetRenderDrawColor(renderer, 235, 90, 150, 255);
            SDL_RenderDrawRect(renderer, &barBg);

            int fillW = (int)(loadingProgress * (barW-4));
            if (fillW > 0) {
                for (int px=0; px<fillW; px++) {
                    float t = (float)px / (barW-4);
                    int cr = 220;
                    int cg = (int)(60 + t * 100);
                    int cb = (int)(130 + t * 80);
                    SDL_SetRenderDrawColor(renderer, cr, cg, cb, 255);
                    SDL_RenderDrawLine(renderer, barX+2+px, barY+2, barX+2+px, barY+barH-3);
                }
            }

            char pctText[20];
            sprintf(pctText, "%d%%", (int)(loadingProgress * 100));
            renderTextCenteredBold(renderer, font, pctText,
                                   WINDOW_WIDTH/2, barY+5, (SDL_Color){255,230,240,255});

            int numDots = ((ticks / 400) % 4);
            char dots[10] = "";
            for (int d=0; d<numDots; d++) strcat(dots, ".");
            char loadMsg[60];
            sprintf(loadMsg, "Loading systems%s", dots);
            renderTextCentered(renderer, fontSmall, loadMsg,
                               WINDOW_WIDTH/2, barY+50, (SDL_Color){255,160,200,255});

            /* Spinning hearts */
            float angle = ticks * 0.003f;
            int spinCX = WINDOW_WIDTH/2, spinCY = barY+88;
            for (int i=0; i<8; i++) {
                float a2 = angle + i * (float)M_PI / 4.0f;
                int sx = spinCX + (int)(12 * cosf(a2));
                int sy = spinCY + (int)(12 * sinf(a2));
                int bright = 100 + (int)(155.0f * ((i + (int)(ticks/100)) % 8) / 8.0f);
                SDL_SetRenderDrawColor(renderer, 255, bright/2 + 60, bright, 255);
                drawFilledCircle(renderer, sx, sy, 2);
            }

        /* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ SOLVER SCREEN â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
        } else if (currentScreen == SCREEN_SOLVER) {
            /* â”€â”€ Top Banner â”€â”€ */
            SDL_SetRenderDrawColor(renderer, 80, 5, 35, 245);
            SDL_Rect banner = {0, 0, WINDOW_WIDTH, 70};
            SDL_RenderFillRect(renderer, &banner);
            SDL_SetRenderDrawColor(renderer, 235, 80, 145, 255);
            SDL_RenderDrawLine(renderer, 0, 70, WINDOW_WIDTH, 70);
            SDL_SetRenderDrawColor(renderer, 255, 130, 185, 255);
            SDL_Rect stripe = {0, 68, WINDOW_WIDTH, 2};
            SDL_RenderFillRect(renderer, &stripe);

            SDL_Color lightPink = {255, 200, 225, 255};
            renderTextCenteredBold(renderer, fontTitle, "FALSE POSITION METHOD",
                                   WINDOW_WIDTH/2, 5, (SDL_Color){255,150,200,255});
            renderTextCentered(renderer, fontLarge, "Exponential Equation: e^x - ax - b = 0",
                               WINDOW_WIDTH/2, 38, lightPink);

            /* Top-right names */
            renderText(renderer, fontSmall, "Submitted by:", 1040, 8, (SDL_Color){240,160,200,255});
            renderTextBold(renderer, fontSmall, "K.A. Codoy", 1040, 26, lightPink);
            renderTextBold(renderer, fontSmall, "M.A. Mendoza", 1040, 44, lightPink);

            SDL_Color sectionColor = {255, 150, 195, 255};
            SDL_Color darkText     = {255, 200, 225, 255};

            /* â”€â”€ Left Panel â”€â”€ */
            SDL_SetRenderDrawColor(renderer, 70, 8, 35, 225);
            SDL_Rect leftPanel = {15, 85, 310, 700};
            SDL_RenderFillRect(renderer, &leftPanel);
            SDL_SetRenderDrawColor(renderer, 220, 75, 135, 255);
            SDL_RenderDrawRect(renderer, &leftPanel);
            SDL_SetRenderDrawColor(renderer, 255, 110, 170, 255);
            SDL_Rect lpTop = {15, 85, 310, 3};
            SDL_RenderFillRect(renderer, &lpTop);

            renderTextBold(renderer, fontLarge, "INPUT", 130, 98, sectionColor);

            /* Formula box */
            SDL_SetRenderDrawColor(renderer, 95, 12, 48, 255);
            SDL_Rect formulaBox = {30, 130, 280, 50};
            SDL_RenderFillRect(renderer, &formulaBox);
            SDL_SetRenderDrawColor(renderer, 220, 75, 135, 255);
            SDL_RenderDrawRect(renderer, &formulaBox);
            renderText(renderer, fontMedium, "f(x) = e^x - ax - b", 50, 138, (SDL_Color){255,170,210,255});
            renderText(renderer, fontSmall,  "Find root where f(x) = 0", 58, 160, (SDL_Color){240,140,180,255});

            inputs[0].rect = (SDL_Rect){140, 198, 150, 35};
            inputs[1].rect = (SDL_Rect){140, 243, 150, 35};
            inputs[2].rect = (SDL_Rect){140, 310, 150, 35};
            inputs[3].rect = (SDL_Rect){140, 355, 150, 35};

            SDL_SetRenderDrawColor(renderer, 210, 70, 125, 255);
            SDL_RenderDrawLine(renderer, 35, 302, 310, 302);
            renderText(renderer, fontSmall, "Initial Guesses", 100, 285, (SDL_Color){255,150,195,255});

            for (int i = 0; i < 4; i++) renderInputBox(renderer, font, &inputs[i]);

            computeBtn.rect = (SDL_Rect){30, 408, 130, 42};
            clearBtn.rect   = (SDL_Rect){180, 408, 130, 42};
            renderButton(renderer, font, &computeBtn);
            renderButton(renderer, font, &clearBtn);

            /* Status */
            renderTextBold(renderer, font, "STATUS", 130, 464, sectionColor);
            SDL_SetRenderDrawColor(renderer, 95, 12, 48, 255);
            SDL_Rect statusBox = {30, 488, 280, 78};
            SDL_RenderFillRect(renderer, &statusBox);
            SDL_SetRenderDrawColor(renderer, 220, 75, 135, 255);
            SDL_RenderDrawRect(renderer, &statusBox);

            if (strlen(resultText) > 0) {
                char resultCopy[500];
                strcpy(resultCopy, resultText);
                char* line = strtok(resultCopy, "\n");
                int ry = 496;
                while (line) {
                    SDL_Color rc = hasValidRoot ? (SDL_Color){80,255,160,255} : (SDL_Color){255,110,110,255};
                    renderText(renderer, fontSmall, line, 40, ry, rc);
                    ry += 18;
                    line = strtok(NULL, "\n");
                }
            }

            /* â”€â”€ Center Panel: Iteration Table â”€â”€ */
            SDL_SetRenderDrawColor(renderer, 70, 8, 35, 225);
            SDL_Rect centerPanel = {340, 85, 620, 700};
            SDL_RenderFillRect(renderer, &centerPanel);
            SDL_SetRenderDrawColor(renderer, 220, 75, 135, 255);
            SDL_RenderDrawRect(renderer, &centerPanel);
            SDL_SetRenderDrawColor(renderer, 255, 110, 170, 255);
            SDL_Rect cpTop = {340, 85, 620, 3};
            SDL_RenderFillRect(renderer, &cpTop);

            renderTextBold(renderer, fontLarge, "ITERATION TABLE", 555, 98, sectionColor);

            if (totalIterations > 0) {
                SDL_SetRenderDrawColor(renderer, 130, 25, 75, 255);
                SDL_Rect tableHeader = {355, 130, 590, 28};
                SDL_RenderFillRect(renderer, &tableHeader);
                SDL_SetRenderDrawColor(renderer, 240, 100, 160, 255);
                SDL_RenderDrawRect(renderer, &tableHeader);

                SDL_Color hdrColor = {255, 210, 230, 255};
                renderTextBold(renderer, fontSmall, "n",     365, 134, hdrColor);
                renderTextBold(renderer, fontSmall, "x0",    405, 134, hdrColor);
                renderTextBold(renderer, fontSmall, "x1",    510, 134, hdrColor);
                renderTextBold(renderer, fontSmall, "x2",    620, 134, hdrColor);
                renderTextBold(renderer, fontSmall, "f(x2)", 730, 134, hdrColor);
                renderTextBold(renderer, fontSmall, "Error", 845, 134, hdrColor);

                int maxVisibleRows = 15;
                int startRow = tableScrollOffset;
                int endRow = startRow + maxVisibleRows;
                if (endRow > totalIterations) endRow = totalIterations;

                for (int i = startRow; i < endRow; i++) {
                    int di = i - startRow;
                    int y = 162 + di * 25;
                    if (i % 2 == 0)
                        SDL_SetRenderDrawColor(renderer, 80, 10, 42, 255);
                    else
                        SDL_SetRenderDrawColor(renderer, 95, 15, 52, 255);
                    SDL_Rect row = {355, y, 590, 25};
                    SDL_RenderFillRect(renderer, &row);
                    SDL_SetRenderDrawColor(renderer, 150, 45, 95, 255);
                    SDL_RenderDrawLine(renderer, 355, y+24, 945, y+24);

                    SDL_Color tc = {255, 200, 225, 255};
                    char buf[50];
                    sprintf(buf, "%d", iterations[i].iteration);
                    renderText(renderer, fontSmall, buf, 365, y+4, tc);
                    sprintf(buf, "%.4lf", iterations[i].x0);
                    renderText(renderer, fontSmall, buf, 400, y+4, tc);
                    sprintf(buf, "%.4lf", iterations[i].x1);
                    renderText(renderer, fontSmall, buf, 505, y+4, tc);
                    sprintf(buf, "%.4lf", iterations[i].x2);
                    renderText(renderer, fontSmall, buf, 615, y+4, tc);
                    sprintf(buf, "%.5lf", iterations[i].fx2);
                    renderText(renderer, fontSmall, buf, 718, y+4, tc);
                    sprintf(buf, "%.5lf", iterations[i].error);
                    renderText(renderer, fontSmall, buf, 838, y+4, tc);
                }

                /* Scrollbar */
                if (totalIterations > maxVisibleRows) {
                    int sbX=950, sbY=162, sbH=maxVisibleRows*25;
                    SDL_SetRenderDrawColor(renderer, 100, 18, 55, 255);
                    SDL_Rect track = {sbX, sbY, 8, sbH};
                    SDL_RenderFillRect(renderer, &track);
                    float thumbRatio = (float)maxVisibleRows / totalIterations;
                    int thumbH = (int)(sbH * thumbRatio);
                    if (thumbH < 20) thumbH = 20;
                    float scrollRatio = (float)tableScrollOffset / (totalIterations - maxVisibleRows);
                    int thumbY = sbY + (int)((sbH - thumbH) * scrollRatio);
                    SDL_SetRenderDrawColor(renderer, 255, 110, 170, 255);
                    SDL_Rect thumb = {sbX, thumbY, 8, thumbH};
                    SDL_RenderFillRect(renderer, &thumb);
                }
            } else {
                renderText(renderer, font, "Enter coefficients and press COMPUTE", 420, 400, (SDL_Color){235,120,170,255});
                renderText(renderer, font, "to see iteration results here.", 465, 428, (SDL_Color){235,120,170,255});
            }

            /* â”€â”€ Conclusion â”€â”€ */
            if (hasValidRoot) {
                int concY = 547;
                renderTextCenteredBold(renderer, font, "CONCLUSION", 650, concY, sectionColor);
                SDL_SetRenderDrawColor(renderer, 90, 12, 48, 255);
                SDL_Rect concBox = {355, concY+22, 590, 108};
                SDL_RenderFillRect(renderer, &concBox);
                SDL_SetRenderDrawColor(renderer, 240, 90, 150, 255);
                SDL_RenderDrawRect(renderer, &concBox);

                char concBuf[200];
                SDL_Color concColor = {255, 175, 215, 255};
                formatEquation(concBuf, (int)coefA, (int)coefB);
                renderTextBold(renderer, fontSmall, concBuf, 365, concY+30, concColor);
                sprintf(concBuf, "Root: x = %.6lf", finalRoot);
                renderTextBold(renderer, font, concBuf, 365, concY+52, (SDL_Color){255,225,240,255});
                sprintf(concBuf, "Iterations: %d  |  Tolerance: %.4lf", totalIterations, TOLERANCE);
                renderText(renderer, fontSmall, concBuf, 365, concY+80, darkText);
            }

            /* â”€â”€ Right Panel: Graph â”€â”€ */
            SDL_SetRenderDrawColor(renderer, 70, 8, 35, 225);
            SDL_Rect rightPanel = {975, 85, 410, 700};
            SDL_RenderFillRect(renderer, &rightPanel);
            SDL_SetRenderDrawColor(renderer, 220, 75, 135, 255);
            SDL_RenderDrawRect(renderer, &rightPanel);
            SDL_SetRenderDrawColor(renderer, 255, 110, 170, 255);
            SDL_Rect rpTop = {975, 85, 410, 3};
            SDL_RenderFillRect(renderer, &rpTop);

            renderTextBold(renderer, fontLarge, "GRAPH", 1140, 98, sectionColor);
            renderText(renderer, fontSmall, "f(x) = e^x - ax - b", 1100, 125, (SDL_Color){250,145,190,255});

            drawGraph(renderer, fontSmall, coefA, coefB, finalRoot, hasValidRoot);

            /* Legend */
            int legendY = 680;
            SDL_SetRenderDrawColor(renderer, 95, 12, 50, 255);
            SDL_Rect legendBox = {990, legendY, 380, 85};
            SDL_RenderFillRect(renderer, &legendBox);
            SDL_SetRenderDrawColor(renderer, 220, 75, 135, 255);
            SDL_RenderDrawRect(renderer, &legendBox);
            renderTextBold(renderer, fontMedium, "LEGEND", 1140, legendY+6, sectionColor);

            SDL_SetRenderDrawColor(renderer, 255, 70, 170, 255);
            SDL_Rect cl = {1010, legendY+35, 25, 3};
            SDL_RenderFillRect(renderer, &cl);
            renderText(renderer, fontSmall, "f(x) curve", 1045, legendY+30, (SDL_Color){255,70,170,255});

            SDL_SetRenderDrawColor(renderer, 255, 245, 70, 255);
            drawFilledCircle(renderer, 1022, legendY+62, 5);
            renderText(renderer, fontSmall, "Approximate root", 1045, legendY+55, (SDL_Color){255,245,70,255});
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
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
