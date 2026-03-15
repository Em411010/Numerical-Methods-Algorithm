#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900

#define SCREEN_LANDING 0
#define SCREEN_LOADING 1
#define SCREEN_SOLVER  2

#define NUM_CLOUDS 7
#define NUM_BIRDS  9

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── Structs ──────────────────────────────────────────── */

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
} Cloud;

typedef struct {
    float x, y;
    float speed;
    float wingPhase;
    float wingSpeed;
} Bird;

/* ─── Utility Functions ────────────────────────────────── */

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

void drawFilledCircle(SDL_Renderer* renderer, int cx, int cy, int r) {
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)sqrt((double)(r * r - dy * dy));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

/* ─── Background Elements ──────────────────────────────── */

void drawSun(SDL_Renderer* renderer, int cx, int cy, int radius, Uint32 ticks) {
    /* Outer glow */
    SDL_SetRenderDrawColor(renderer, 255, 215, 155, 255);
    drawFilledCircle(renderer, cx, cy, radius + 20);
    /* Main body */
    SDL_SetRenderDrawColor(renderer, 255, 220, 90, 255);
    drawFilledCircle(renderer, cx, cy, radius);
    /* Bright center */
    SDL_SetRenderDrawColor(renderer, 255, 242, 165, 255);
    drawFilledCircle(renderer, cx, cy, radius * 2 / 3);
    /* Rotating rays */
    float rotAngle = ticks * 0.0004f;
    SDL_SetRenderDrawColor(renderer, 255, 225, 110, 255);
    for (int i = 0; i < 12; i++) {
        float angle = rotAngle + i * (2.0f * (float)M_PI / 12.0f);
        int x1 = cx + (int)((radius + 10) * cos(angle));
        int y1 = cy + (int)((radius + 10) * sin(angle));
        int x2 = cx + (int)((radius + 40) * cos(angle));
        int y2 = cy + (int)((radius + 40) * sin(angle));
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        SDL_RenderDrawLine(renderer, x1 + 1, y1, x2 + 1, y2);
        SDL_RenderDrawLine(renderer, x1, y1 + 1, x2, y2 + 1);
    }
}

void drawCloudShape(SDL_Renderer* renderer, int cx, int cy, float scale) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    drawFilledCircle(renderer, cx, cy, (int)(30 * scale));
    drawFilledCircle(renderer, cx - (int)(26 * scale), cy + (int)(7 * scale), (int)(23 * scale));
    drawFilledCircle(renderer, cx + (int)(30 * scale), cy + (int)(7 * scale), (int)(25 * scale));
    drawFilledCircle(renderer, cx - (int)(13 * scale), cy - (int)(13 * scale), (int)(21 * scale));
    drawFilledCircle(renderer, cx + (int)(15 * scale), cy - (int)(11 * scale), (int)(23 * scale));
    drawFilledCircle(renderer, cx + (int)(50 * scale), cy + (int)(11 * scale), (int)(17 * scale));
    drawFilledCircle(renderer, cx - (int)(44 * scale), cy + (int)(11 * scale), (int)(16 * scale));
    /* Pink highlight on top */
    SDL_SetRenderDrawColor(renderer, 255, 235, 243, 255);
    drawFilledCircle(renderer, cx + (int)(5 * scale), cy - (int)(9 * scale), (int)(15 * scale));
}

void drawBirdShape(SDL_Renderer* renderer, int cx, int cy, float wingAngle) {
    SDL_SetRenderDrawColor(renderer, 95, 55, 70, 255);
    int wLen = 15;
    float a = 0.4f + 0.6f * wingAngle;
    /* Left wing */
    int lx = cx - (int)(wLen * cos(a * 0.85));
    int ly = cy - (int)(wLen * sin(a));
    SDL_RenderDrawLine(renderer, cx, cy, lx, ly);
    SDL_RenderDrawLine(renderer, cx, cy - 1, lx, ly - 1);
    /* Right wing */
    int rx = cx + (int)(wLen * cos(a * 0.85));
    int ry = cy - (int)(wLen * sin(a));
    SDL_RenderDrawLine(renderer, cx, cy, rx, ry);
    SDL_RenderDrawLine(renderer, cx, cy - 1, rx, ry - 1);
}

void drawBackground(SDL_Renderer* renderer, Cloud clouds[], Bird birds[], Uint32 ticks) {
    /* Sky gradient: soft flamingo pink top → lighter rose bottom */
    for (int y = 0; y < WINDOW_HEIGHT; y++) {
        float t = (float)y / WINDOW_HEIGHT;
        int r = (int)(255 - t * 10);   /* 255 → 245 */
        int g = (int)(208 + t * 40);   /* 208 → 248 */
        int b = (int)(226 + t * 20);   /* 226 → 246 */
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderDrawLine(renderer, 0, y, WINDOW_WIDTH, y);
    }
    /* Sun */
    drawSun(renderer, 1390, 105, 55, ticks);
    /* Clouds */
    for (int i = 0; i < NUM_CLOUDS; i++)
        drawCloudShape(renderer, (int)clouds[i].x, (int)clouds[i].y, clouds[i].scale);
    /* Birds */
    for (int i = 0; i < NUM_BIRDS; i++) {
        float wing = (float)sin(ticks * birds[i].wingSpeed * 0.001 + birds[i].wingPhase);
        drawBirdShape(renderer, (int)birds[i].x, (int)birds[i].y, 0.5f + 0.5f * wing);
    }
}

/* ─── UI Components (Flamingo Pink) ────────────────────── */

void renderInputBox(SDL_Renderer* renderer, TTF_Font* font, InputBox* box) {
    SDL_Color labelColor = {205, 50, 105, 255};
    renderTextBold(renderer, font, box->label, box->rect.x - 35, box->rect.y + 6, labelColor);
    if (box->active)
        SDL_SetRenderDrawColor(renderer, 255, 232, 243, 255);
    else
        SDL_SetRenderDrawColor(renderer, 255, 244, 250, 255);
    SDL_RenderFillRect(renderer, &box->rect);
    if (box->active)
        SDL_SetRenderDrawColor(renderer, 220, 55, 115, 255);
    else
        SDL_SetRenderDrawColor(renderer, 235, 150, 185, 255);
    SDL_RenderDrawRect(renderer, &box->rect);
    SDL_Color textColor = {145, 30, 72, 255};
    if (strlen(box->value) > 0)
        renderText(renderer, font, box->value, box->rect.x + 8, box->rect.y + 6, textColor);
}

void renderButton(SDL_Renderer* renderer, TTF_Font* font, Button* btn) {
    /* Shadow */
    SDL_SetRenderDrawColor(renderer, 155, 25, 68, 255);
    SDL_Rect shadow = {btn->rect.x + 3, btn->rect.y + 3, btn->rect.w, btn->rect.h};
    SDL_RenderFillRect(renderer, &shadow);
    /* Face */
    if (btn->clicked)
        SDL_SetRenderDrawColor(renderer, 175, 28, 78, 255);
    else if (btn->hovered)
        SDL_SetRenderDrawColor(renderer, 240, 82, 142, 255);
    else
        SDL_SetRenderDrawColor(renderer, 218, 52, 112, 255);
    SDL_RenderFillRect(renderer, &btn->rect);
    /* Border */
    SDL_SetRenderDrawColor(renderer, 168, 22, 68, 255);
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

/* ─── Graph (Flamingo Pink themed) ─────────────────────── */

void drawGraph(SDL_Renderer* renderer, TTF_Font* fontSmall, double a1, double b1, double c1,
               double a2, double b2, double c2, double solX, double solY, int hasSolution) {
    int graphX = 1090, graphY = 210, graphW = 480, graphH = 420;
    int scale = 40;
    int centerX = graphX + graphW / 2;
    int centerY = graphY + graphH / 2;

    /* Background */
    SDL_SetRenderDrawColor(renderer, 255, 242, 249, 255);
    SDL_Rect graphRect = {graphX, graphY, graphW, graphH};
    SDL_RenderFillRect(renderer, &graphRect);
    /* Border */
    SDL_SetRenderDrawColor(renderer, 225, 95, 145, 255);
    SDL_RenderDrawRect(renderer, &graphRect);
    /* Grid */
    SDL_SetRenderDrawColor(renderer, 252, 222, 238, 255);
    for (int i = graphX; i <= graphX + graphW; i += scale)
        SDL_RenderDrawLine(renderer, i, graphY, i, graphY + graphH);
    for (int i = graphY; i <= graphY + graphH; i += scale)
        SDL_RenderDrawLine(renderer, graphX, i, graphX + graphW, i);
    /* Axes */
    SDL_SetRenderDrawColor(renderer, 185, 48, 102, 255);
    SDL_RenderDrawLine(renderer, centerX, graphY, centerX, graphY + graphH);
    SDL_RenderDrawLine(renderer, graphX, centerY, graphX + graphW, centerY);
    /* Axis labels */
    renderText(renderer, fontSmall, "x", graphX + graphW - 15, centerY + 5, (SDL_Color){200, 50, 105, 255});
    renderText(renderer, fontSmall, "y", centerX + 5, graphY + 5, (SDL_Color){200, 50, 105, 255});
    /* Tick marks */
    SDL_SetRenderDrawColor(renderer, 185, 48, 102, 255);
    for (int i = graphX; i <= graphX + graphW; i += scale)
        if (i != centerX) SDL_RenderDrawLine(renderer, i, centerY - 3, i, centerY + 3);
    for (int i = graphY; i <= graphY + graphH; i += scale)
        if (i != centerY) SDL_RenderDrawLine(renderer, centerX - 3, i, centerX + 3, i);

    /* Line 1 (deep flamingo pink) */
    if (b1 != 0) {
        SDL_SetRenderDrawColor(renderer, 218, 45, 98, 255);
        for (int px = graphX; px < graphX + graphW; px++) {
            double gx = (px - centerX) / (double)scale;
            double gy = (c1 - a1 * gx) / b1;
            int py = centerY - (int)(gy * scale);
            if (py >= graphY && py < graphY + graphH) {
                SDL_RenderDrawPoint(renderer, px, py);
                SDL_RenderDrawPoint(renderer, px, py + 1);
                SDL_RenderDrawPoint(renderer, px, py - 1);
            }
        }
    } else if (a1 != 0) {
        double gx = c1 / a1;
        int px = centerX + (int)(gx * scale);
        if (px >= graphX && px < graphX + graphW) {
            SDL_SetRenderDrawColor(renderer, 218, 45, 98, 255);
            SDL_RenderDrawLine(renderer, px, graphY, px, graphY + graphH);
        }
    }
    /* Line 2 (coral-rose) */
    if (b2 != 0) {
        SDL_SetRenderDrawColor(renderer, 255, 125, 138, 255);
        for (int px = graphX; px < graphX + graphW; px++) {
            double gx = (px - centerX) / (double)scale;
            double gy = (c2 - a2 * gx) / b2;
            int py = centerY - (int)(gy * scale);
            if (py >= graphY && py < graphY + graphH) {
                SDL_RenderDrawPoint(renderer, px, py);
                SDL_RenderDrawPoint(renderer, px, py + 1);
                SDL_RenderDrawPoint(renderer, px, py - 1);
            }
        }
    } else if (a2 != 0) {
        double gx = c2 / a2;
        int px = centerX + (int)(gx * scale);
        if (px >= graphX && px < graphX + graphW) {
            SDL_SetRenderDrawColor(renderer, 255, 125, 138, 255);
            SDL_RenderDrawLine(renderer, px, graphY, px, graphY + graphH);
        }
    }
    /* Intersection point */
    if (hasSolution) {
        int px = centerX + (int)(solX * scale);
        int py = centerY - (int)(solY * scale);
        if (px >= graphX && px < graphX + graphW && py >= graphY && py < graphY + graphH) {
            SDL_SetRenderDrawColor(renderer, 255, 185, 205, 255);
            for (int i = -12; i <= 12; i++)
                for (int j = -12; j <= 12; j++)
                    if (i*i + j*j <= 144 && i*i + j*j > 64)
                        SDL_RenderDrawPoint(renderer, px + i, py + j);
            SDL_SetRenderDrawColor(renderer, 220, 38, 98, 255);
            for (int i = -8; i <= 8; i++)
                for (int j = -8; j <= 8; j++)
                    if (i*i + j*j <= 64)
                        SDL_RenderDrawPoint(renderer, px + i, py + j);
        }
    }
}

/* ═══════════════════════════════════════════════════════════
   MAIN
   ═══════════════════════════════════════════════════════════ */

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    srand((unsigned)time(NULL));

    SDL_Window* window = SDL_CreateWindow("Gaussian Elimination - 2 Variables",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH, WINDOW_HEIGHT);

    /* Fonts */
    TTF_Font* font      = TTF_OpenFont("font.ttf", 18);
    TTF_Font* fontSmall  = TTF_OpenFont("font.ttf", 14);
    TTF_Font* fontMedium = TTF_OpenFont("font.ttf", 16);
    TTF_Font* fontLarge  = TTF_OpenFont("font.ttf", 20);
    TTF_Font* fontTitle  = TTF_OpenFont("font.ttf", 26);
    TTF_Font* fontStep   = TTF_OpenFont("font.ttf", 15);
    TTF_Font* fontHuge   = TTF_OpenFont("font.ttf", 44);
    TTF_Font* fontBig    = TTF_OpenFont("font.ttf", 34);
    if (!font || !fontSmall || !fontMedium || !fontLarge || !fontTitle || !fontStep || !fontHuge || !fontBig) {
        printf("Error loading font: %s\n", TTF_GetError());
        return 1;
    }

    /* Clouds */
    Cloud clouds[NUM_CLOUDS];
    for (int i = 0; i < NUM_CLOUDS; i++) {
        clouds[i].x = (float)(rand() % WINDOW_WIDTH);
        clouds[i].y = 45.0f + (float)(rand() % 190);
        clouds[i].speed = 0.22f + (float)(rand() % 100) / 280.0f;
        clouds[i].scale = 0.55f + (float)(rand() % 100) / 140.0f;
    }

    /* Birds */
    Bird birds[NUM_BIRDS];
    for (int i = 0; i < NUM_BIRDS; i++) {
        birds[i].x = (float)(rand() % WINDOW_WIDTH);
        birds[i].y = 35.0f + (float)(rand() % 240);
        birds[i].speed = 0.35f + (float)(rand() % 100) / 120.0f;
        birds[i].wingPhase = (float)(rand() % 628) / 100.0f;
        birds[i].wingSpeed = 2.8f + (float)(rand() % 350) / 100.0f;
    }

    /* Screen state */
    int currentScreen = SCREEN_LANDING;
    float loadingProgress = 0.0f;
    int loadingDoneDelay = 0;

    /* Landing button */
    Button startBtn = {{0, 0, 220, 58}, "START", 0, 0};

    /* Solver inputs */
    InputBox inputs[6];
    const char* labels[] = {"a1:", "b1:", "c1:", "a2:", "b2:", "c2:"};
    for (int i = 0; i < 6; i++) {
        inputs[i].rect = (SDL_Rect){0, 0, 100, 35};
        strcpy(inputs[i].label, labels[i]);
        strcpy(inputs[i].value, "");
        inputs[i].active = 0;
    }
    Button computeBtn = {{0, 0, 170, 48}, "COMPUTE", 0, 0};
    Button clearBtn   = {{0, 0, 170, 48}, "CLEAR", 0, 0};

    /* Solver state */
    char resultText[500] = "Enter coefficients for both equations";
    int hasSolution = 0;
    double solX = 0, solY = 0;
    double a1 = 0, b1 = 0, c1 = 0, a2 = 0, b2 = 0, c2 = 0;
    double s_multiplier = 0, s_new_b2 = 0, s_new_c2 = 0;
    double s_verify1 = 0, s_verify2 = 0;
    int hasSteps = 0, specialCase = 0;
    int activeInput = -1;

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
                    for (int i = 0; i < 6; i++) {
                        if (mx >= inputs[i].rect.x && mx <= inputs[i].rect.x + inputs[i].rect.w &&
                            my >= inputs[i].rect.y && my <= inputs[i].rect.y + inputs[i].rect.h)
                            activeInput = i;
                        inputs[i].active = (i == activeInput);
                    }
                    if (mx >= computeBtn.rect.x && mx <= computeBtn.rect.x + computeBtn.rect.w &&
                        my >= computeBtn.rect.y && my <= computeBtn.rect.y + computeBtn.rect.h) {
                        computeBtn.clicked = 1;
                        a1 = atof(inputs[0].value); b1 = atof(inputs[1].value); c1 = atof(inputs[2].value);
                        a2 = atof(inputs[3].value); b2 = atof(inputs[4].value); c2 = atof(inputs[5].value);
                        hasSolution = 0; hasSteps = 0; specialCase = 0;
                        if (fabs(a1) < 1e-10) {
                            sprintf(resultText, "ERROR: a1 cannot be zero\nSwap equations or adjust values");
                        } else {
                            hasSteps = 1;
                            s_multiplier = a2 / a1;
                            s_new_b2 = b2 - s_multiplier * b1;
                            s_new_c2 = c2 - s_multiplier * c1;
                            if (fabs(s_new_b2) < 1e-10) {
                                if (fabs(s_new_c2) < 1e-10) {
                                    specialCase = 1;
                                    sprintf(resultText, "INFINITE SOLUTIONS\nEquations are dependent (same line)");
                                } else {
                                    specialCase = 2;
                                    sprintf(resultText, "NO SOLUTION\nEquations are inconsistent (parallel lines)");
                                }
                            } else {
                                solY = s_new_c2 / s_new_b2;
                                solX = (c1 - b1 * solY) / a1;
                                s_verify1 = a1 * solX + b1 * solY;
                                s_verify2 = a2 * solX + b2 * solY;
                                hasSolution = 1;
                                sprintf(resultText, "SUCCESS!\nSolution: x = %.6f, y = %.6f", solX, solY);
                            }
                        }
                    }
                    if (mx >= clearBtn.rect.x && mx <= clearBtn.rect.x + clearBtn.rect.w &&
                        my >= clearBtn.rect.y && my <= clearBtn.rect.y + clearBtn.rect.h) {
                        for (int i = 0; i < 6; i++) strcpy(inputs[i].value, "");
                        strcpy(resultText, "Enter coefficients for both equations");
                        hasSolution = 0; hasSteps = 0; specialCase = 0;
                        clearBtn.clicked = 1;
                    }
                }
                if (e.type == SDL_MOUSEBUTTONUP) {
                    computeBtn.clicked = 0;
                    clearBtn.clicked = 0;
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
            }
        }

        /* ── Update animations ── */
        for (int i = 0; i < NUM_CLOUDS; i++) {
            clouds[i].x -= clouds[i].speed;
            if (clouds[i].x < -140) {
                clouds[i].x = WINDOW_WIDTH + 80.0f + (float)(rand() % 120);
                clouds[i].y = 45.0f + (float)(rand() % 190);
            }
        }
        for (int i = 0; i < NUM_BIRDS; i++) {
            birds[i].x += birds[i].speed;
            if (birds[i].x > WINDOW_WIDTH + 60) {
                birds[i].x = -55.0f;
                birds[i].y = 35.0f + (float)(rand() % 240);
            }
        }
        if (currentScreen == SCREEN_LOADING) {
            loadingProgress += 0.005f;
            if (loadingProgress >= 1.0f) {
                loadingProgress = 1.0f;
                loadingDoneDelay++;
                if (loadingDoneDelay > 45) currentScreen = SCREEN_SOLVER;
            }
        }

        /* ════════════════════ RENDER ════════════════════ */
        drawBackground(renderer, clouds, birds, ticks);

        /* ──────────────── LANDING SCREEN ──────────────── */
        if (currentScreen == SCREEN_LANDING) {
            int cw = 780, ch = 510;
            int cardX = (WINDOW_WIDTH - cw) / 2, cardY = (WINDOW_HEIGHT - ch) / 2 - 15;

            /* Card shadow */
            SDL_SetRenderDrawColor(renderer, 175, 38, 85, 50);
            SDL_Rect cardShadow = {cardX + 6, cardY + 6, cw, ch};
            SDL_RenderFillRect(renderer, &cardShadow);
            /* Card bg */
            SDL_SetRenderDrawColor(renderer, 255, 246, 251, 255);
            SDL_Rect card = {cardX, cardY, cw, ch};
            SDL_RenderFillRect(renderer, &card);
            /* Double border */
            SDL_SetRenderDrawColor(renderer, 218, 52, 112, 255);
            SDL_RenderDrawRect(renderer, &card);
            SDL_Rect inner = {cardX + 4, cardY + 4, cw - 8, ch - 8};
            SDL_RenderDrawRect(renderer, &inner);
            /* Top accent bar */
            SDL_SetRenderDrawColor(renderer, 218, 52, 112, 255);
            SDL_Rect topBar = {cardX, cardY, cw, 10};
            SDL_RenderFillRect(renderer, &topBar);
            /* Bottom accent bar */
            SDL_SetRenderDrawColor(renderer, 255, 150, 180, 255);
            SDL_Rect bottomBar = {cardX, cardY + ch - 5, cw, 5};
            SDL_RenderFillRect(renderer, &bottomBar);

            /* Title */
            renderTextCenteredBold(renderer, fontHuge, "ELIMINATION METHOD",
                                   WINDOW_WIDTH / 2, cardY + 55, (SDL_Color){198, 35, 95, 255});

            /* Decorative line + diamond */
            int lineY = cardY + 115;
            SDL_SetRenderDrawColor(renderer, 240, 135, 175, 255);
            SDL_RenderDrawLine(renderer, cardX + 120, lineY, cardX + cw - 120, lineY);
            SDL_RenderDrawLine(renderer, cardX + 120, lineY + 1, cardX + cw - 120, lineY + 1);
            int dmx = WINDOW_WIDTH / 2;
            SDL_SetRenderDrawColor(renderer, 218, 52, 112, 255);
            for (int d = 0; d < 7; d++) {
                SDL_RenderDrawPoint(renderer, dmx - d, lineY - (6 - d));
                SDL_RenderDrawPoint(renderer, dmx + d, lineY - (6 - d));
                SDL_RenderDrawPoint(renderer, dmx - d, lineY + (6 - d) + 2);
                SDL_RenderDrawPoint(renderer, dmx + d, lineY + (6 - d) + 2);
            }

            /* Subtitle */
            renderTextCenteredBold(renderer, fontLarge, "System of Linear Equations (2 Variables)",
                                   WINDOW_WIDTH / 2, cardY + 138, (SDL_Color){185, 55, 115, 255});

            /* Course info */
            SDL_Color infoColor = {165, 78, 125, 255};
            renderTextCentered(renderer, fontMedium, "MT211 - Numerical Method",
                               WINDOW_WIDTH / 2, cardY + 195, infoColor);
            renderTextCentered(renderer, fontMedium, "Semestral Project",
                               WINDOW_WIDTH / 2, cardY + 220, infoColor);

            /* Authors box */
            SDL_SetRenderDrawColor(renderer, 248, 215, 230, 255);
            SDL_Rect authBg = {cardX + 90, cardY + 268, cw - 180, 95};
            SDL_RenderFillRect(renderer, &authBg);
            SDL_SetRenderDrawColor(renderer, 232, 128, 172, 255);
            SDL_RenderDrawRect(renderer, &authBg);

            renderTextCenteredBold(renderer, fontMedium, "Presented By",
                                   WINDOW_WIDTH / 2, cardY + 276, (SDL_Color){205, 55, 112, 255});
            renderTextCenteredBold(renderer, fontLarge, "Francis John Rodela  |  Joshua Deolino",
                                   WINDOW_WIDTH / 2, cardY + 302, (SDL_Color){155, 35, 82, 255});
            renderTextCentered(renderer, fontMedium, "BSCPE 22001",
                               WINDOW_WIDTH / 2, cardY + 338, infoColor);

            /* START button */
            startBtn.rect = (SDL_Rect){(WINDOW_WIDTH - 220) / 2, cardY + 405, 220, 58};
            renderButton(renderer, fontLarge, &startBtn);

        /* ──────────────── LOADING SCREEN ──────────────── */
        } else if (currentScreen == SCREEN_LOADING) {
            int cw2 = 620, ch2 = 280;
            int lx = (WINDOW_WIDTH - cw2) / 2, ly = (WINDOW_HEIGHT - ch2) / 2;

            /* Card */
            SDL_SetRenderDrawColor(renderer, 255, 246, 251, 245);
            SDL_Rect lcard = {lx, ly, cw2, ch2};
            SDL_RenderFillRect(renderer, &lcard);
            SDL_SetRenderDrawColor(renderer, 218, 52, 112, 255);
            SDL_RenderDrawRect(renderer, &lcard);
            /* Top accent */
            SDL_Rect ltop = {lx, ly, cw2, 6};
            SDL_RenderFillRect(renderer, &ltop);

            /* Title */
            renderTextCenteredBold(renderer, fontBig, "Loading...",
                                   WINDOW_WIDTH / 2, ly + 35, (SDL_Color){200, 38, 98, 255});
            renderTextCentered(renderer, fontMedium, "Preparing the Gaussian Elimination Solver",
                               WINDOW_WIDTH / 2, ly + 90, (SDL_Color){185, 68, 125, 255});

            /* Progress bar */
            int barW = 460, barH = 34;
            int barX = (WINDOW_WIDTH - barW) / 2, barY = ly + 135;
            SDL_SetRenderDrawColor(renderer, 248, 215, 230, 255);
            SDL_Rect barBg = {barX, barY, barW, barH};
            SDL_RenderFillRect(renderer, &barBg);
            SDL_SetRenderDrawColor(renderer, 222, 95, 145, 255);
            SDL_RenderDrawRect(renderer, &barBg);

            int fillW = (int)(loadingProgress * (barW - 4));
            if (fillW > 0) {
                for (int px = 0; px < fillW; px++) {
                    float t = (float)px / (barW - 4);
                    int cr = (int)(218 + t * 37);
                    int cg = (int)(52  + t * 78);
                    int cb = (int)(112 + t * 28);
                    if (cr > 255) cr = 255;
                    SDL_SetRenderDrawColor(renderer, cr, cg, cb, 255);
                    SDL_RenderDrawLine(renderer, barX + 2 + px, barY + 2, barX + 2 + px, barY + barH - 3);
                }
            }

            /* Percentage */
            char pctText[20];
            sprintf(pctText, "%d%%", (int)(loadingProgress * 100));
            renderTextCenteredBold(renderer, font, pctText,
                                   WINDOW_WIDTH / 2, barY + 6, (SDL_Color){142, 22, 68, 255});

            /* Animated dots */
            int numDots = ((ticks / 400) % 4);
            char dots[10] = "";
            for (int d = 0; d < numDots; d++) strcat(dots, ".");
            char loadMsg[60];
            sprintf(loadMsg, "Please wait%s", dots);
            renderTextCentered(renderer, fontSmall, loadMsg,
                               WINDOW_WIDTH / 2, barY + 58, (SDL_Color){185, 100, 145, 255});

        /* ──────────────── SOLVER SCREEN ───────────────── */
        } else if (currentScreen == SCREEN_SOLVER) {
            /* ── Top Banner ── */
            SDL_SetRenderDrawColor(renderer, 212, 48, 102, 255);
            SDL_Rect banner = {0, 0, WINDOW_WIDTH, 70};
            SDL_RenderFillRect(renderer, &banner);
            SDL_SetRenderDrawColor(renderer, 178, 28, 72, 255);
            SDL_RenderDrawLine(renderer, 0, 70, WINDOW_WIDTH, 70);
            /* Accent stripe */
            SDL_SetRenderDrawColor(renderer, 255, 155, 185, 255);
            SDL_Rect stripe = {0, 67, WINDOW_WIDTH, 3};
            SDL_RenderFillRect(renderer, &stripe);

            SDL_Color white = {255, 255, 255, 255};
            SDL_Color cream = {255, 228, 238, 255};
            renderTextBold(renderer, fontTitle, "ELIMINATION METHOD", 30, 18, white);
            renderText(renderer, fontLarge, "System of Linear Equations (2 Variables)", 530, 23, cream);
            renderText(renderer, fontSmall, "MT211 - Numerical Method  |  Semestral Project", 1200, 10, cream);
            renderText(renderer, fontSmall, "BSCPE 22001  |  Francis John Rodela | Joshua Deolino", 1200, 32, cream);

            /* ── Left Panel ── */
            SDL_Color panelBg = {255, 246, 251, 255};
            SDL_Color panelBorder = {232, 138, 178, 255};
            drawPanel(renderer, 15, 85, 490, 800, panelBg, panelBorder);

            SDL_Color sectionColor = {192, 38, 95, 255};
            SDL_Color darkText = {135, 28, 68, 255};

            renderTextBold(renderer, fontLarge, "INPUT COEFFICIENTS", 130, 100, sectionColor);

            SDL_Color eqBg = {255, 238, 248, 255};
            SDL_Color eqBorder = {232, 138, 178, 255};
            drawPanel(renderer, 35, 135, 450, 65, eqBg, eqBorder);

            SDL_Color formulaColor = {182, 48, 102, 255};
            renderText(renderer, font, "Eq 1:  a1*x  +  b1*y  =  c1", 55, 143, formulaColor);
            renderText(renderer, font, "Eq 2:  a2*x  +  b2*y  =  c2", 55, 170, formulaColor);

            /* Equation 1 inputs */
            renderTextBold(renderer, font, "EQUATION 1", 180, 215, sectionColor);
            SDL_SetRenderDrawColor(renderer, 255, 240, 248, 255);
            SDL_Rect eq1Bg = {35, 245, 450, 60};
            SDL_RenderFillRect(renderer, &eq1Bg);
            SDL_SetRenderDrawColor(renderer, 242, 182, 205, 255);
            SDL_RenderDrawRect(renderer, &eq1Bg);

            inputs[0].rect = (SDL_Rect){80, 257, 85, 35};
            inputs[1].rect = (SDL_Rect){225, 257, 85, 35};
            inputs[2].rect = (SDL_Rect){385, 257, 85, 35};

            /* Equation 2 inputs */
            renderTextBold(renderer, font, "EQUATION 2", 180, 320, sectionColor);
            SDL_SetRenderDrawColor(renderer, 255, 240, 248, 255);
            SDL_Rect eq2Bg = {35, 350, 450, 60};
            SDL_RenderFillRect(renderer, &eq2Bg);
            SDL_SetRenderDrawColor(renderer, 242, 182, 205, 255);
            SDL_RenderDrawRect(renderer, &eq2Bg);

            inputs[3].rect = (SDL_Rect){80, 362, 85, 35};
            inputs[4].rect = (SDL_Rect){225, 362, 85, 35};
            inputs[5].rect = (SDL_Rect){385, 362, 85, 35};

            for (int i = 0; i < 6; i++) renderInputBox(renderer, font, &inputs[i]);

            /* Buttons */
            computeBtn.rect = (SDL_Rect){80, 440, 170, 48};
            clearBtn.rect   = (SDL_Rect){275, 440, 170, 48};
            renderButton(renderer, font, &computeBtn);
            renderButton(renderer, font, &clearBtn);

            /* Status */
            renderTextBold(renderer, font, "STATUS", 215, 510, sectionColor);
            drawPanel(renderer, 35, 540, 450, 60, eqBg, eqBorder);
            if (strlen(resultText) > 0) {
                char resultCopy[500];
                strcpy(resultCopy, resultText);
                char* line = strtok(resultCopy, "\n");
                int ry = 547;
                while (line) {
                    SDL_Color resultColor = hasSolution ? (SDL_Color){0, 135, 68, 255} : (SDL_Color){200, 38, 78, 255};
                    renderText(renderer, fontMedium, line, 50, ry, resultColor);
                    ry += 22;
                    line = strtok(NULL, "\n");
                }
            }

            /* Solution box */
            if (hasSolution) {
                renderTextBold(renderer, font, "FINAL ANSWER", 190, 620, sectionColor);
                SDL_Color solBg = {255, 232, 242, 255};
                SDL_Color solBorder = {222, 78, 132, 255};
                drawPanel(renderer, 35, 650, 450, 100, solBg, solBorder);

                SDL_Color conclusionColor = {172, 18, 78, 255};
                char buffer[200];
                sprintf(buffer, "x = %.6f", solX);
                renderTextBold(renderer, fontLarge, buffer, 55, 665, conclusionColor);
                sprintf(buffer, "y = %.6f", solY);
                renderTextBold(renderer, fontLarge, buffer, 260, 665, conclusionColor);
                sprintf(buffer, "Point of Intersection: (%.4f, %.4f)", solX, solY);
                renderText(renderer, font, buffer, 55, 718, (SDL_Color){202, 48, 105, 255});
            }

            /* ── Center Panel: Steps ── */
            drawPanel(renderer, 520, 85, 545, 800, panelBg, panelBorder);
            renderTextBold(renderer, fontLarge, "SOLUTION STEPS", 695, 100, sectionColor);

            if (hasSteps) {
                char buf[200];
                int sy = 135;

                /* Step 0: Original System */
                drawPanel(renderer, 535, sy, 515, 80,
                          (SDL_Color){255, 238, 248, 255}, (SDL_Color){232, 138, 178, 255});
                renderTextBold(renderer, fontMedium, "GIVEN: Original System", 550, sy + 5, sectionColor);
                SDL_SetRenderDrawColor(renderer, 232, 138, 178, 255);
                SDL_RenderDrawLine(renderer, 550, sy + 25, 1040, sy + 25);
                sprintf(buf, "Eq1:  %.2fx + %.2fy = %.2f", a1, b1, c1);
                renderText(renderer, font, buf, 560, sy + 30, (SDL_Color){218, 45, 98, 255});
                sprintf(buf, "Eq2:  %.2fx + %.2fy = %.2f", a2, b2, c2);
                renderText(renderer, font, buf, 560, sy + 55, (SDL_Color){255, 125, 138, 255});

                sy += 95;

                /* Step 1: Forward Elimination */
                drawPanel(renderer, 535, sy, 515, 130,
                          (SDL_Color){255, 232, 242, 255}, (SDL_Color){225, 118, 162, 255});
                renderTextBold(renderer, fontMedium, "STEP 1: Forward Elimination", 550, sy + 5, sectionColor);
                SDL_SetRenderDrawColor(renderer, 225, 118, 162, 255);
                SDL_RenderDrawLine(renderer, 550, sy + 25, 1040, sy + 25);
                sprintf(buf, "Find multiplier:  m = a2 / a1 = %.4f / %.4f", a2, a1);
                renderText(renderer, fontStep, buf, 560, sy + 32, darkText);
                sprintf(buf, "m = %.6f", s_multiplier);
                renderTextBold(renderer, font, buf, 560, sy + 55, (SDL_Color){202, 38, 92, 255});
                renderText(renderer, fontStep, "Eliminate x:  New Eq2 = Eq2 - (m * Eq1)", 560, sy + 80, darkText);
                sprintf(buf, "Result:  0x + (%.6f)y = %.6f", s_new_b2, s_new_c2);
                renderTextBold(renderer, fontStep, buf, 560, sy + 103, (SDL_Color){202, 38, 92, 255});

                sy += 145;

                if (specialCase == 1) {
                    drawPanel(renderer, 535, sy, 515, 60,
                              (SDL_Color){255, 242, 232, 255}, (SDL_Color){222, 152, 102, 255});
                    renderTextBold(renderer, font, "All coefficients became 0", 560, sy + 8, (SDL_Color){202, 122, 62, 255});
                    renderText(renderer, font, "Equations are dependent - infinite solutions", 560, sy + 33, (SDL_Color){202, 122, 62, 255});
                } else if (specialCase == 2) {
                    drawPanel(renderer, 535, sy, 515, 60,
                              (SDL_Color){255, 228, 238, 255}, (SDL_Color){222, 68, 102, 255});
                    renderTextBold(renderer, font, "Coefficient of y = 0, but constant != 0", 560, sy + 8, (SDL_Color){202, 38, 72, 255});
                    renderText(renderer, font, "Equations are inconsistent - no solution", 560, sy + 33, (SDL_Color){202, 38, 72, 255});
                } else if (hasSolution) {
                    /* Step 2: Solve for y */
                    drawPanel(renderer, 535, sy, 515, 80,
                              (SDL_Color){252, 238, 248, 255}, (SDL_Color){205, 98, 152, 255});
                    renderTextBold(renderer, fontMedium, "STEP 2: Back Substitution - Solve for y", 550, sy + 5, (SDL_Color){182, 28, 92, 255});
                    SDL_SetRenderDrawColor(renderer, 205, 98, 152, 255);
                    SDL_RenderDrawLine(renderer, 550, sy + 25, 1040, sy + 25);
                    sprintf(buf, "y = %.6f / %.6f", s_new_c2, s_new_b2);
                    renderText(renderer, fontStep, buf, 560, sy + 32, darkText);
                    sprintf(buf, "y = %.6f", solY);
                    renderTextBold(renderer, font, buf, 560, sy + 55, (SDL_Color){182, 28, 92, 255});

                    sy += 95;

                    /* Step 3: Solve for x */
                    drawPanel(renderer, 535, sy, 515, 100,
                              (SDL_Color){248, 232, 248, 255}, (SDL_Color){192, 88, 152, 255});
                    renderTextBold(renderer, fontMedium, "STEP 3: Substitute y into Eq1 - Solve for x", 550, sy + 5, (SDL_Color){172, 22, 88, 255});
                    SDL_SetRenderDrawColor(renderer, 192, 88, 152, 255);
                    SDL_RenderDrawLine(renderer, 550, sy + 25, 1040, sy + 25);
                    sprintf(buf, "%.2fx + %.2f(%.6f) = %.2f", a1, b1, solY, c1);
                    renderText(renderer, fontStep, buf, 560, sy + 32, darkText);
                    sprintf(buf, "%.2fx = %.6f", a1, c1 - b1 * solY);
                    renderText(renderer, fontStep, buf, 560, sy + 55, darkText);
                    sprintf(buf, "x = %.6f", solX);
                    renderTextBold(renderer, font, buf, 560, sy + 75, (SDL_Color){172, 22, 88, 255});

                    sy += 115;

                    /* Step 4: Verification */
                    drawPanel(renderer, 535, sy, 515, 105,
                              (SDL_Color){252, 240, 250, 255}, (SDL_Color){205, 118, 172, 255});
                    renderTextBold(renderer, fontMedium, "VERIFICATION", 550, sy + 5, (SDL_Color){182, 38, 112, 255});
                    SDL_SetRenderDrawColor(renderer, 205, 118, 172, 255);
                    SDL_RenderDrawLine(renderer, 550, sy + 25, 1040, sy + 25);

                    int check1 = fabs(s_verify1 - c1) < 0.01;
                    sprintf(buf, "Eq1: %.2f(%.4f) + %.2f(%.4f) = %.4f", a1, solX, b1, solY, s_verify1);
                    renderText(renderer, fontStep, buf, 560, sy + 32, darkText);
                    sprintf(buf, "Expected: %.2f    %s", c1, check1 ? "PASS" : "FAIL");
                    renderText(renderer, fontStep, buf, 560, sy + 52,
                               check1 ? (SDL_Color){0, 142, 68, 255} : (SDL_Color){202, 28, 58, 255});

                    int check2 = fabs(s_verify2 - c2) < 0.01;
                    sprintf(buf, "Eq2: %.2f(%.4f) + %.2f(%.4f) = %.4f", a2, solX, b2, solY, s_verify2);
                    renderText(renderer, fontStep, buf, 560, sy + 75, darkText);
                    sprintf(buf, "Expected: %.2f    %s", c2, check2 ? "PASS" : "FAIL");
                    renderText(renderer, fontStep, buf, 560, sy + 95,
                               check2 ? (SDL_Color){0, 142, 68, 255} : (SDL_Color){202, 28, 58, 255});
                }
            } else {
                renderText(renderer, font, "Enter coefficients and press COMPUTE", 620, 420, (SDL_Color){205, 142, 172, 255});
                renderText(renderer, font, "to see the step-by-step solution here.", 615, 450, (SDL_Color){205, 142, 172, 255});
            }

            /* ── Right Panel: Graph ── */
            drawPanel(renderer, 1080, 85, 505, 800, panelBg, panelBorder);
            renderTextBold(renderer, fontLarge, "GRAPH", 1290, 100, sectionColor);
            renderText(renderer, fontSmall, "Visual representation of the two lines", 1180, 125, (SDL_Color){205, 128, 168, 255});

            drawGraph(renderer, fontSmall, a1, b1, c1, a2, b2, c2, solX, solY, hasSolution);

            /* Legend */
            int legendY = 660;
            drawPanel(renderer, 1095, legendY, 475, 115, eqBg, panelBorder);
            renderTextBold(renderer, fontMedium, "LEGEND", 1290, legendY + 8, sectionColor);

            SDL_SetRenderDrawColor(renderer, 218, 45, 98, 255);
            SDL_Rect l1 = {1115, legendY + 42, 30, 4};
            SDL_RenderFillRect(renderer, &l1);
            renderText(renderer, fontMedium, "Equation 1", 1155, legendY + 35, (SDL_Color){218, 45, 98, 255});

            SDL_SetRenderDrawColor(renderer, 255, 125, 138, 255);
            SDL_Rect l2 = {1115, legendY + 68, 30, 4};
            SDL_RenderFillRect(renderer, &l2);
            renderText(renderer, fontMedium, "Equation 2", 1155, legendY + 61, (SDL_Color){255, 125, 138, 255});

            SDL_SetRenderDrawColor(renderer, 220, 38, 98, 255);
            for (int i = -6; i <= 6; i++)
                for (int j = -6; j <= 6; j++)
                    if (i*i + j*j <= 36)
                        SDL_RenderDrawPoint(renderer, 1130 + i, legendY + 95 + j);
            renderText(renderer, fontMedium, "Solution Point", 1155, legendY + 87, (SDL_Color){202, 38, 102, 255});
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    TTF_CloseFont(font);
    TTF_CloseFont(fontSmall);
    TTF_CloseFont(fontMedium);
    TTF_CloseFont(fontLarge);
    TTF_CloseFont(fontTitle);
    TTF_CloseFont(fontStep);
    TTF_CloseFont(fontHuge);
    TTF_CloseFont(fontBig);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
