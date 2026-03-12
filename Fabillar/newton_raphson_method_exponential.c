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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    int iteration;
    double xn;
    double fxn;
    double fpxn;
    double xn1;
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

// Original exponential function f(x) = e^x - ax - b
double f(double x, double a, double b) {
    return exp(x) - a * x - b;
}

// Derivative f'(x) = e^x - a
double fp(double x, double a) {
    return exp(x) - a;
}

// Format equation with proper notation
void formatEquation(char* buffer, int a, int b) {
    char part1[50], part2[50];
    
    if (a == 0) {
        strcpy(part1, "");
    } else if (a == 1) {
        strcpy(part1, " - x");
    } else if (a == -1) {
        strcpy(part1, " + x");
    } else if (a > 0) {
        sprintf(part1, " - %dx", a);
    } else {
        sprintf(part1, " + %dx", -a);
    }
    
    if (b == 0) {
        strcpy(part2, "");
    } else if (b > 0) {
        sprintf(part2, " - %d", b);
    } else {
        sprintf(part2, " + %d", -b);
    }
    
    sprintf(buffer, "Equation: eˣ%s%s = 0", part1, part2);
}

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

/* ─── Primitive Helpers ──────────────────────────────────── */

void drawFilledCircle(SDL_Renderer* r, int cx, int cy, int rad) {
    for (int dy = -rad; dy <= rad; dy++)
        for (int dx = -rad; dx <= rad; dx++)
            if (dx*dx + dy*dy <= rad*rad)
                SDL_RenderDrawPoint(r, cx+dx, cy+dy);
}

void drawFilledRoundedRect(SDL_Renderer* r, int x, int y, int w, int h, int rad) {
    if (rad < 1) rad = 1;
    SDL_Rect mid = {x, y + rad, w, h - 2*rad};
    SDL_RenderFillRect(r, &mid);
    SDL_Rect top = {x + rad, y, w - 2*rad, rad};
    SDL_RenderFillRect(r, &top);
    SDL_Rect bot = {x + rad, y + h - rad, w - 2*rad, rad};
    SDL_RenderFillRect(r, &bot);
    drawFilledCircle(r, x+rad,     y+rad,     rad);
    drawFilledCircle(r, x+w-rad-1, y+rad,     rad);
    drawFilledCircle(r, x+rad,     y+h-rad-1, rad);
    drawFilledCircle(r, x+w-rad-1, y+h-rad-1, rad);
}

/* ─── Background ─────────────────────────────────────────── */

void drawBackground(SDL_Renderer* renderer, Uint32 ticks) {
    for (int y = 0; y < WINDOW_HEIGHT; y++) {
        float t = (float)y / WINDOW_HEIGHT;
        int r = (int)(130 + t * 95);
        int g = (int)(25  + t * 85);
        int b = (int)(65  + t * 70);
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderDrawLine(renderer, 0, y, WINDOW_WIDTH, y);
    }
    SDL_SetRenderDrawColor(renderer, 200, 80, 120, 50);
    for (int x = 0; x < WINDOW_WIDTH; x += 60)
        SDL_RenderDrawLine(renderer, x, 0, x, WINDOW_HEIGHT);
    for (int y = 0; y < WINDOW_HEIGHT; y += 60)
        SDL_RenderDrawLine(renderer, 0, y, WINDOW_WIDTH, y);
    SDL_SetRenderDrawColor(renderer, 110, 28, 60, 255);
    SDL_Rect ground = {0, WINDOW_HEIGHT - 65, WINDOW_WIDTH, 65};
    SDL_RenderFillRect(renderer, &ground);
    SDL_SetRenderDrawColor(renderer, 240, 90, 145, 255);
    SDL_Rect gstripe = {0, WINDOW_HEIGHT - 65, WINDOW_WIDTH, 3};
    SDL_RenderFillRect(renderer, &gstripe);
    SDL_SetRenderDrawColor(renderer, 185, 60, 105, 255);
    for (int x = 20; x < WINDOW_WIDTH; x += 45)
        drawFilledCircle(renderer, x, WINDOW_HEIGHT - 40, 2);
    for (int i = 0; i < 30; i++) {
        float px = (float)((i * 137 + (int)(ticks * 0.02f)) % WINDOW_WIDTH);
        float py = (float)((i * 89  + (int)(ticks * 0.008f * (i % 3 + 1))) % (WINDOW_HEIGHT - 80));
        int v = 60 + (i * 23) % 70;
        SDL_SetRenderDrawColor(renderer, 220, v, v + 40, 255);
        drawFilledCircle(renderer, (int)px, (int)py, 1 + i % 2);
    }
}

/* ─── UI Components ──────────────────────────────────────── */

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
    SDL_SetRenderDrawColor(renderer, 60, 10, 30, 255);
    SDL_Rect shadow = {btn->rect.x + 3, btn->rect.y + 3, btn->rect.w, btn->rect.h};
    SDL_RenderFillRect(renderer, &shadow);
    if (btn->clicked)
        SDL_SetRenderDrawColor(renderer, 130, 35, 75, 255);
    else if (btn->hovered)
        SDL_SetRenderDrawColor(renderer, 240, 85, 140, 255);
    else
        SDL_SetRenderDrawColor(renderer, 195, 60, 105, 255);
    SDL_RenderFillRect(renderer, &btn->rect);
    SDL_SetRenderDrawColor(renderer, 255, 160, 195, 255);
    SDL_RenderDrawLine(renderer, btn->rect.x+1, btn->rect.y, btn->rect.x+btn->rect.w-2, btn->rect.y);
    SDL_SetRenderDrawColor(renderer, 155, 45, 85, 255);
    SDL_RenderDrawRect(renderer, &btn->rect);
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

void drawGraph(SDL_Renderer* renderer, TTF_Font* fontSmall, double a, double b, double root, int hasRoot) {
    int graphX=980, graphY=220, graphW=400, graphH=440, scale=50;
    int centerX=graphX+graphW/2, centerY=graphY+graphH/2;
    SDL_SetRenderDrawColor(renderer, 55, 12, 35, 255);
    SDL_Rect graphRect={graphX,graphY,graphW,graphH}; SDL_RenderFillRect(renderer,&graphRect);
    SDL_SetRenderDrawColor(renderer, 190, 65, 110, 255); SDL_RenderDrawRect(renderer,&graphRect);
    SDL_SetRenderDrawColor(renderer, 100, 28, 60, 255);
    for(int i=graphX;i<=graphX+graphW;i+=50) SDL_RenderDrawLine(renderer,i,graphY,i,graphY+graphH);
    for(int i=graphY;i<=graphY+graphH;i+=50) SDL_RenderDrawLine(renderer,graphX,i,graphX+graphW,i);
    SDL_SetRenderDrawColor(renderer,220,90,145,255);
    SDL_RenderDrawLine(renderer,centerX,graphY,centerX,graphY+graphH);
    SDL_RenderDrawLine(renderer,graphX,centerY,graphX+graphW,centerY);
    renderText(renderer,fontSmall,"x",graphX+graphW-15,centerY+5,(SDL_Color){255,150,190,255});
    renderText(renderer,fontSmall,"y",centerX+5,graphY+5,(SDL_Color){255,150,190,255});
    SDL_SetRenderDrawColor(renderer,220,90,145,255);
    for(int i=graphX;i<=graphX+graphW;i+=50) if(i!=centerX) SDL_RenderDrawLine(renderer,i,centerY-3,i,centerY+3);
    for(int i=graphY;i<=graphY+graphH;i+=50) if(i!=centerY) SDL_RenderDrawLine(renderer,centerX-3,i,centerX+3,i);
    SDL_SetRenderDrawColor(renderer,255,60,160,255);
    for(int px=graphX;px<graphX+graphW;px++){
        double x=(px-centerX)/(double)scale;
        double y=f(x,a,b);
        int py=centerY-(int)(y*20);
        if(py>=graphY&&py<graphY+graphH&&fabs(y)<50){
            SDL_RenderDrawPoint(renderer,px,py);
            SDL_RenderDrawPoint(renderer,px,py+1);
        }
    }
    if(hasRoot){
        int rx=centerX+(int)(root*scale);
        SDL_SetRenderDrawColor(renderer,255,200,80,255);
        for(int i=-12;i<=12;i++) for(int j=-12;j<=12;j++) if(i*i+j*j<=144&&i*i+j*j>64) SDL_RenderDrawPoint(renderer,rx+i,centerY+j);
        SDL_SetRenderDrawColor(renderer,255,240,60,255);
        for(int i=-8;i<=8;i++) for(int j=-8;j<=8;j++) if(i*i+j*j<=64) SDL_RenderDrawPoint(renderer,rx+i,centerY+j);
    }
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window* window = SDL_CreateWindow("Newton-Raphson Method - Exponential",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    /* Fonts */
    TTF_Font* font       = TTF_OpenFont("font.ttf", 18);
    TTF_Font* fontSmall  = TTF_OpenFont("font.ttf", 14);
    TTF_Font* fontMedium = TTF_OpenFont("font.ttf", 16);
    TTF_Font* fontLarge  = TTF_OpenFont("font.ttf", 20);
    TTF_Font* fontTitle  = TTF_OpenFont("font.ttf", 24);
    TTF_Font* fontHuge   = TTF_OpenFont("font.ttf", 44);
    TTF_Font* fontBig    = TTF_OpenFont("font.ttf", 32);
    if (!font || !fontSmall || !fontMedium || !fontLarge || !fontTitle || !fontHuge || !fontBig) {
        printf("Error loading font: %s\n", TTF_GetError());
        return 1;
    }

    /* Screen state */
    int currentScreen = SCREEN_LANDING;
    float loadingProgress = 0.0f;
    int loadingDoneDelay = 0;

    /* Landing button */
    Button startBtn = {{0, 0, 240, 55}, "LAUNCH SOLVER", 0, 0};

    /* Solver inputs (3: a, b, x0) */
    InputBox inputs[3];
    const char* labels[] = {"a:", "b:", "x0:"};
    for (int i = 0; i < 3; i++) {
        inputs[i].rect = (SDL_Rect){140, 230 + i * 60, 150, 35};
        strcpy(inputs[i].label, labels[i]);
        strcpy(inputs[i].value, "");
        inputs[i].active = 0;
    }
    Button computeBtn = {{50, 430, 120, 40}, "COMPUTE", 0, 0};
    Button clearBtn   = {{190, 430, 120, 40}, "CLEAR", 0, 0};
    
    /* Solver state */
    char resultText[500] = "Enter coefficients and initial guess,\nthen press COMPUTE";
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
                    for (int i = 0; i < 3; i++) {
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
                        totalIterations = 0;
                        hasValidRoot = 0;
                        double xn = x0;
                        int diverged = 0;
                        for (int iter = 0; iter < MAX_ITER; iter++) {
                            double fxn = f(xn, coefA, coefB);
                            double fpxn = fp(xn, coefA);
                            if (fabs(fpxn) < 1e-12) { diverged = 1; break; }
                            double xn1 = xn - fxn / fpxn;
                            double error = fabs(xn1 - xn);
                            iterations[iter].iteration = iter + 1;
                            iterations[iter].xn  = xn;
                            iterations[iter].fxn = fxn;
                            iterations[iter].fpxn = fpxn;
                            iterations[iter].xn1 = xn1;
                            iterations[iter].error = error;
                            totalIterations++;
                            if (isnan(xn1) || isinf(xn1) || fabs(xn1) > 1e10) { diverged = 1; break; }
                            if (error < TOLERANCE) {
                                hasValidRoot = 1;
                                finalRoot = xn1;
                                sprintf(resultText, "SUCCESS!\nRoot: x = %.6f\nIterations: %d", xn1, iter + 1);
                                break;
                            }
                            xn = xn1;
                        }
                        if (diverged) {
                            sprintf(resultText, "FAILED: Diverged\n(f'(x) near zero or overflow)\nTry a different x0");
                            hasValidRoot = 0;
                        } else if (!hasValidRoot) {
                            sprintf(resultText, "FAILED: Did not converge\nwithin %d iterations\nTry a different x0", MAX_ITER);
                        }
                    }
                    if (mx >= clearBtn.rect.x && mx <= clearBtn.rect.x + clearBtn.rect.w &&
                        my >= clearBtn.rect.y && my <= clearBtn.rect.y + clearBtn.rect.h) {
                        for (int i = 0; i < 3; i++) strcpy(inputs[i].value, "");
                        strcpy(resultText, "Enter coefficients and initial guess,\nthen press COMPUTE");
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
                if (e.type == SDL_MOUSEWHEEL) {
                    if (totalIterations > 0) {
                        tableScrollOffset -= e.wheel.y * 2;
                        if (tableScrollOffset < 0) tableScrollOffset = 0;
                        int maxScroll = totalIterations - 15;
                        if (maxScroll < 0) maxScroll = 0;
                        if (tableScrollOffset > maxScroll) tableScrollOffset = maxScroll;
                    }
                }
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
        drawBackground(renderer, ticks);

        /* ──────────────── LANDING SCREEN ──────────────── */
        if (currentScreen == SCREEN_LANDING) {
            int cw = 750, ch = 440;
            int cardX = (WINDOW_WIDTH - cw) / 2, cardY = (WINDOW_HEIGHT - ch) / 2 - 40;

            /* Card shadow */
            SDL_SetRenderDrawColor(renderer, 40, 0, 20, 180);
            SDL_Rect cardShadow = {cardX + 5, cardY + 5, cw, ch};
            SDL_RenderFillRect(renderer, &cardShadow);
            /* Card bg */
            SDL_SetRenderDrawColor(renderer, 50, 8, 30, 240);
            SDL_Rect card = {cardX, cardY, cw, ch};
            SDL_RenderFillRect(renderer, &card);
            /* Borders */
            SDL_SetRenderDrawColor(renderer, 220, 70, 130, 255);
            SDL_RenderDrawRect(renderer, &card);
            SDL_Rect inner = {cardX + 3, cardY + 3, cw - 6, ch - 6};
            SDL_SetRenderDrawColor(renderer, 170, 45, 95, 255);
            SDL_RenderDrawRect(renderer, &inner);
            /* Top accent bar */
            SDL_SetRenderDrawColor(renderer, 210, 60, 120, 255);
            SDL_Rect topBar = {cardX, cardY, cw, 8};
            SDL_RenderFillRect(renderer, &topBar);
            /* Bottom accent bar */
            SDL_SetRenderDrawColor(renderer, 255, 100, 170, 255);
            SDL_Rect bottomBar = {cardX, cardY + ch - 4, cw, 4};
            SDL_RenderFillRect(renderer, &bottomBar);

            /* Corner decorations */
            SDL_SetRenderDrawColor(renderer, 255, 100, 170, 255);
            SDL_RenderDrawLine(renderer, cardX + 12, cardY + 15, cardX + 12, cardY + 35);
            SDL_RenderDrawLine(renderer, cardX + 12, cardY + 15, cardX + 32, cardY + 15);
            SDL_RenderDrawLine(renderer, cardX + cw - 12, cardY + 15, cardX + cw - 12, cardY + 35);
            SDL_RenderDrawLine(renderer, cardX + cw - 12, cardY + 15, cardX + cw - 32, cardY + 15);
            SDL_RenderDrawLine(renderer, cardX + 12, cardY + ch - 15, cardX + 12, cardY + ch - 35);
            SDL_RenderDrawLine(renderer, cardX + 12, cardY + ch - 15, cardX + 32, cardY + ch - 15);
            SDL_RenderDrawLine(renderer, cardX + cw - 12, cardY + ch - 15, cardX + cw - 12, cardY + ch - 35);
            SDL_RenderDrawLine(renderer, cardX + cw - 12, cardY + ch - 15, cardX + cw - 32, cardY + ch - 15);

            /* Title */
            renderTextCenteredBold(renderer, fontHuge, "NEWTON-RAPHSON",
                                   WINDOW_WIDTH / 2, cardY + 45, (SDL_Color){255, 130, 185, 255});
            renderTextCenteredBold(renderer, fontBig, "METHOD",
                                   WINDOW_WIDTH / 2, cardY + 100, (SDL_Color){240, 100, 160, 255});

            /* Divider line */
            int lineY = cardY + 148;
            SDL_SetRenderDrawColor(renderer, 210, 60, 120, 255);
            SDL_RenderDrawLine(renderer, cardX + 100, lineY, cardX + cw - 100, lineY);
            SDL_RenderDrawLine(renderer, cardX + 100, lineY + 1, cardX + cw - 100, lineY + 1);
            int dmx = WINDOW_WIDTH / 2;
            SDL_SetRenderDrawColor(renderer, 255, 110, 170, 255);
            for (int d = 0; d < 6; d++) {
                SDL_RenderDrawPoint(renderer, dmx - d, lineY - (5 - d));
                SDL_RenderDrawPoint(renderer, dmx + d, lineY - (5 - d));
                SDL_RenderDrawPoint(renderer, dmx - d, lineY + (5 - d) + 2);
                SDL_RenderDrawPoint(renderer, dmx + d, lineY + (5 - d) + 2);
            }

            /* Subtitle */
            renderTextCenteredBold(renderer, fontLarge, "Exponential Equation: e^x - ax - b = 0",
                                   WINDOW_WIDTH / 2, cardY + 168, (SDL_Color){255, 180, 215, 255});

            /* Course info */
            renderTextCentered(renderer, fontMedium, "MT211 - Numerical Method  |  Semestral Project",
                               WINDOW_WIDTH / 2, cardY + 215, (SDL_Color){230, 140, 185, 255});

            /* Author box */
            SDL_SetRenderDrawColor(renderer, 65, 10, 38, 255);
            SDL_Rect authBg = {cardX + 100, cardY + 250, cw - 200, 90};
            SDL_RenderFillRect(renderer, &authBg);
            SDL_SetRenderDrawColor(renderer, 210, 65, 120, 255);
            SDL_RenderDrawRect(renderer, &authBg);

            renderTextCenteredBold(renderer, fontMedium, "Submitted By",
                                   WINDOW_WIDTH / 2, cardY + 258, (SDL_Color){255, 130, 180, 255});
            renderTextCenteredBold(renderer, fontLarge, "Clarence P. Fabillar",
                                   WINDOW_WIDTH / 2, cardY + 280, (SDL_Color){255, 210, 230, 255});
            renderTextCenteredBold(renderer, fontLarge, "Maica Pearl Lancero",
                                   WINDOW_WIDTH / 2, cardY + 304, (SDL_Color){255, 210, 230, 255});
            renderTextCentered(renderer, fontMedium, "BSCPE 22001",
                               WINDOW_WIDTH / 2, cardY + 328, (SDL_Color){230, 140, 185, 255});

            /* Start button */
            startBtn.rect = (SDL_Rect){(WINDOW_WIDTH - 240) / 2, cardY + 365, 240, 55};
            renderButton(renderer, fontLarge, &startBtn);

        /* ──────────────── LOADING SCREEN ──────────────── */
        } else if (currentScreen == SCREEN_LOADING) {
            int cw2 = 580, ch2 = 250;
            int lx = (WINDOW_WIDTH - cw2) / 2, ly = (WINDOW_HEIGHT - ch2) / 2;

            SDL_SetRenderDrawColor(renderer, 50, 8, 30, 245);
            SDL_Rect lcard = {lx, ly, cw2, ch2};
            SDL_RenderFillRect(renderer, &lcard);
            SDL_SetRenderDrawColor(renderer, 220, 70, 130, 255);
            SDL_RenderDrawRect(renderer, &lcard);
            SDL_SetRenderDrawColor(renderer, 210, 60, 120, 255);
            SDL_Rect ltop = {lx, ly, cw2, 5};
            SDL_RenderFillRect(renderer, &ltop);

            renderTextCenteredBold(renderer, fontBig, "Initializing...",
                                   WINDOW_WIDTH / 2, ly + 30, (SDL_Color){255, 130, 185, 255});
            renderTextCentered(renderer, fontMedium, "Preparing the Newton-Raphson Method Solver",
                               WINDOW_WIDTH / 2, ly + 80, (SDL_Color){240, 160, 200, 255});

            /* Progress bar */
            int barW = 420, barH = 30;
            int barX = (WINDOW_WIDTH - barW) / 2, barY = ly + 120;
            SDL_SetRenderDrawColor(renderer, 65, 10, 38, 255);
            SDL_Rect barBg = {barX, barY, barW, barH};
            SDL_RenderFillRect(renderer, &barBg);
            SDL_SetRenderDrawColor(renderer, 210, 65, 120, 255);
            SDL_RenderDrawRect(renderer, &barBg);

            int fillW = (int)(loadingProgress * (barW - 4));
            if (fillW > 0) {
                for (int px = 0; px < fillW; px++) {
                    float t = (float)px / (barW - 4);
                    int cr = (int)(150 + t * 105);
                    int cg = (int)(20 + t * 50);
                    int cb = (int)(80 + t * 100);
                    if (cr > 255) cr = 255;
                    if (cb > 255) cb = 255;
                    SDL_SetRenderDrawColor(renderer, cr, cg, cb, 255);
                    SDL_RenderDrawLine(renderer, barX + 2 + px, barY + 2, barX + 2 + px, barY + barH - 3);
                }
            }

            char pctText[20];
            sprintf(pctText, "%d%%", (int)(loadingProgress * 100));
            renderTextCenteredBold(renderer, font, pctText,
                                   WINDOW_WIDTH / 2, barY + 5, (SDL_Color){255, 220, 235, 255});

            int numDots = ((ticks / 400) % 4);
            char dots[10] = "";
            for (int d = 0; d < numDots; d++) strcat(dots, ".");
            char loadMsg[60];
            sprintf(loadMsg, "Loading systems%s", dots);
            renderTextCentered(renderer, fontSmall, loadMsg,
                               WINDOW_WIDTH / 2, barY + 48, (SDL_Color){230, 130, 175, 255});

            float angle = ticks * 0.003f;
            int spinCX = WINDOW_WIDTH / 2, spinCY = barY + 82;
            for (int i = 0; i < 8; i++) {
                float a2 = angle + i * (float)M_PI / 4.0f;
                int sx = spinCX + (int)(12 * cosf(a2));
                int sy = spinCY + (int)(12 * sinf(a2));
                int brightness = 80 + (int)(175.0f * ((i + (int)(ticks / 100)) % 8) / 8.0f);
                if (brightness > 255) brightness = 255;
                SDL_SetRenderDrawColor(renderer, brightness, brightness / 4, brightness / 2, 255);
                drawFilledCircle(renderer, sx, sy, 2);
            }

        /* ──────────────── SOLVER SCREEN ───────────────── */
        } else if (currentScreen == SCREEN_SOLVER) {
            /* ── Top Banner ── */
            SDL_SetRenderDrawColor(renderer, 60, 12, 35, 245);
            SDL_Rect banner = {0, 0, WINDOW_WIDTH, 70};
            SDL_RenderFillRect(renderer, &banner);
            SDL_SetRenderDrawColor(renderer, 210, 70, 120, 255);
            SDL_RenderDrawLine(renderer, 0, 70, WINDOW_WIDTH, 70);
            SDL_SetRenderDrawColor(renderer, 255, 100, 160, 255);
            SDL_Rect stripe = {0, 68, WINDOW_WIDTH, 2};
            SDL_RenderFillRect(renderer, &stripe);

            SDL_Color lightPink = {255, 185, 215, 255};
            renderTextCenteredBold(renderer, fontTitle, "NEWTON-RAPHSON METHOD",
                                   WINDOW_WIDTH / 2, 5, (SDL_Color){255, 130, 180, 255});
            renderTextCentered(renderer, fontLarge, "Exponential Equation: e^x - ax - b = 0",
                               WINDOW_WIDTH / 2, 38, lightPink);

            /* Top-right: names */
            renderText(renderer, fontSmall, "Submitted by:", 1035, 8, (SDL_Color){220, 140, 175, 255});
            renderTextBold(renderer, fontSmall, "Clarence P. Fabillar", 1035, 26, lightPink);
            renderTextBold(renderer, fontSmall, "Maica Pearl Lancero",  1035, 44, lightPink);

            SDL_Color sectionColor = {255, 130, 175, 255};
            SDL_Color darkText     = {245, 185, 215, 255};

            /* ── Left Panel ── */
            SDL_SetRenderDrawColor(renderer, 60, 12, 38, 225);
            SDL_Rect leftPanel = {15, 85, 310, 700};
            SDL_RenderFillRect(renderer, &leftPanel);
            SDL_SetRenderDrawColor(renderer, 200, 70, 120, 255);
            SDL_RenderDrawRect(renderer, &leftPanel);
            SDL_SetRenderDrawColor(renderer, 225, 80, 135, 255);
            SDL_Rect lpTop = {15, 85, 310, 3};
            SDL_RenderFillRect(renderer, &lpTop);

            renderTextBold(renderer, fontLarge, "INPUT", 125, 98, sectionColor);

            /* Formula box */
            SDL_SetRenderDrawColor(renderer, 80, 18, 45, 255);
            SDL_Rect formulaBox = {30, 130, 280, 68};
            SDL_RenderFillRect(renderer, &formulaBox);
            SDL_SetRenderDrawColor(renderer, 200, 70, 120, 255);
            SDL_RenderDrawRect(renderer, &formulaBox);
            renderText(renderer, fontMedium, "f(x)  = e^x - ax - b",  42, 136, (SDL_Color){255, 150, 190, 255});
            renderText(renderer, fontMedium, "f'(x) = e^x - a",       42, 158, (SDL_Color){255, 150, 190, 255});
            renderText(renderer, fontSmall,  "x(n+1) = x(n) - f/f'",  50, 180, (SDL_Color){220, 120, 160, 255});

            /* Inputs: a, b — then divider, then x0 */
            inputs[0].rect = (SDL_Rect){140, 218, 150, 35};
            inputs[1].rect = (SDL_Rect){140, 263, 150, 35};
            inputs[2].rect = (SDL_Rect){140, 330, 150, 35};

            SDL_SetRenderDrawColor(renderer, 190, 65, 110, 255);
            SDL_RenderDrawLine(renderer, 35, 315, 310, 315);
            renderText(renderer, fontSmall, "Initial Guess", 108, 298, (SDL_Color){240, 120, 165, 255});

            for (int i = 0; i < 3; i++) renderInputBox(renderer, font, &inputs[i]);

            computeBtn.rect = (SDL_Rect){30, 385, 130, 42};
            clearBtn.rect   = (SDL_Rect){180, 385, 130, 42};
            renderButton(renderer, font, &computeBtn);
            renderButton(renderer, font, &clearBtn);

            /* Status */
            renderTextBold(renderer, font, "STATUS", 125, 440, sectionColor);
            SDL_SetRenderDrawColor(renderer, 80, 18, 45, 255);
            SDL_Rect statusBox = {30, 464, 280, 78};
            SDL_RenderFillRect(renderer, &statusBox);
            SDL_SetRenderDrawColor(renderer, 200, 70, 120, 255);
            SDL_RenderDrawRect(renderer, &statusBox);

            if (strlen(resultText) > 0) {
                char resultCopy[500];
                strcpy(resultCopy, resultText);
                char* line = strtok(resultCopy, "\n");
                int ry = 472;
                while (line) {
                    SDL_Color rc = hasValidRoot ? (SDL_Color){50, 255, 150, 255} : (SDL_Color){255, 100, 100, 255};
                    renderText(renderer, fontSmall, line, 40, ry, rc);
                    ry += 18;
                    line = strtok(NULL, "\n");
                }
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
                SDL_SetRenderDrawColor(renderer, 110, 28, 70, 255);
                SDL_Rect tableHeader = {355, 130, 590, 28};
                SDL_RenderFillRect(renderer, &tableHeader);
                SDL_SetRenderDrawColor(renderer, 210, 75, 125, 255);
                SDL_RenderDrawRect(renderer, &tableHeader);

                SDL_Color hdrColor = {255, 200, 225, 255};
                renderTextBold(renderer, fontSmall, "n",       365, 134, hdrColor);
                renderTextBold(renderer, fontSmall, "x_n",     405, 134, hdrColor);
                renderTextBold(renderer, fontSmall, "f(x_n)",  505, 134, hdrColor);
                renderTextBold(renderer, fontSmall, "f'(x_n)", 620, 134, hdrColor);
                renderTextBold(renderer, fontSmall, "x_(n+1)", 720, 134, hdrColor);
                renderTextBold(renderer, fontSmall, "Error",   840, 134, hdrColor);

                int maxVisibleRows = 15;
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
                    SDL_SetRenderDrawColor(renderer, 130, 40, 80, 255);
                    SDL_RenderDrawLine(renderer, 355, y + 24, 945, y + 24);

                    SDL_Color tc = {255, 190, 215, 255};
                    char buf[50];
                    sprintf(buf, "%d", iterations[i].iteration);
                    renderText(renderer, fontSmall, buf, 365, y + 4, tc);
                    sprintf(buf, "%.4lf", iterations[i].xn);
                    renderText(renderer, fontSmall, buf, 400, y + 4, tc);
                    sprintf(buf, "%.4lf", iterations[i].fxn);
                    renderText(renderer, fontSmall, buf, 500, y + 4, tc);
                    sprintf(buf, "%.4lf", iterations[i].fpxn);
                    renderText(renderer, fontSmall, buf, 615, y + 4, tc);
                    sprintf(buf, "%.4lf", iterations[i].xn1);
                    renderText(renderer, fontSmall, buf, 715, y + 4, tc);
                    sprintf(buf, "%.6lf", iterations[i].error);
                    renderText(renderer, fontSmall, buf, 833, y + 4, tc);
                }

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
                renderText(renderer, font, "Enter coefficients and press COMPUTE", 420, 400, (SDL_Color){220, 100, 150, 255});
                renderText(renderer, font, "to see iteration results here.", 460, 428, (SDL_Color){220, 100, 150, 255});
            }

            /* ── Conclusion (below iteration table) ── */
            if (hasValidRoot) {
                int concY = 547; /* 162 + 15*25 + 10 */
                renderTextCenteredBold(renderer, font, "CONCLUSION", 650, concY, sectionColor);
                SDL_SetRenderDrawColor(renderer, 75, 15, 42, 255);
                SDL_Rect concBox = {355, concY + 22, 590, 108};
                SDL_RenderFillRect(renderer, &concBox);
                SDL_SetRenderDrawColor(renderer, 220, 80, 130, 255);
                SDL_RenderDrawRect(renderer, &concBox);

                char concBuf[200];
                SDL_Color concColor = {255, 160, 200, 255};
                formatEquation(concBuf, (int)coefA, (int)coefB);
                renderTextBold(renderer, fontSmall, concBuf, 365, concY + 30, concColor);
                sprintf(concBuf, "Root: x = %.6lf", finalRoot);
                renderTextBold(renderer, font, concBuf, 365, concY + 52, (SDL_Color){255, 210, 230, 255});
                sprintf(concBuf, "Iterations: %d  |  Tolerance: %.4lf", totalIterations, TOLERANCE);
                renderText(renderer, fontSmall, concBuf, 365, concY + 80, darkText);
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

            SDL_SetRenderDrawColor(renderer, 255, 60, 160, 255);
            SDL_Rect cl = {1010, legendY + 35, 25, 3};
            SDL_RenderFillRect(renderer, &cl);
            renderText(renderer, fontSmall, "f(x) curve", 1045, legendY + 30, (SDL_Color){255, 60, 160, 255});

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
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
