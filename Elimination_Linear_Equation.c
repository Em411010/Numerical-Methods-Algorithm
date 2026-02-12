#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900

// UI component structures
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

// Render text with UTF-8 support
void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &rect);
    
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

// Render bold text (draw twice offset by 1px)
void renderTextBold(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    renderText(renderer, font, text, x, y, color);
    renderText(renderer, font, text, x + 1, y, color);
}

// Draw a panel background with border
void drawPanel(SDL_Renderer* renderer, int x, int y, int w, int h, 
               SDL_Color bg, SDL_Color border) {
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &r);
}

// Render input box with label directly to the left
void renderInputBox(SDL_Renderer* renderer, TTF_Font* font, InputBox* box) {
    // Label to the left
    SDL_Color labelColor = {120, 80, 0, 255};
    renderTextBold(renderer, font, box->label, box->rect.x - 35, box->rect.y + 6, labelColor);
    
    // Box fill
    if (box->active) {
        SDL_SetRenderDrawColor(renderer, 255, 245, 215, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 252, 240, 255);
    }
    SDL_RenderFillRect(renderer, &box->rect);
    
    // Box border
    if (box->active) {
        SDL_SetRenderDrawColor(renderer, 200, 140, 20, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 200, 170, 100, 255);
    }
    SDL_RenderDrawRect(renderer, &box->rect);
    
    // Value text
    SDL_Color textColor = {80, 50, 0, 255};
    if (strlen(box->value) > 0) {
        renderText(renderer, font, box->value, box->rect.x + 8, box->rect.y + 6, textColor);
    }
}

// Render button with hover and click effects
void renderButton(SDL_Renderer* renderer, TTF_Font* font, Button* btn) {
    if (btn->clicked) {
        SDL_SetRenderDrawColor(renderer, 180, 120, 0, 255);
    } else if (btn->hovered) {
        SDL_SetRenderDrawColor(renderer, 220, 160, 40, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 200, 140, 20, 255);
    }
    SDL_RenderFillRect(renderer, &btn->rect);
    
    SDL_SetRenderDrawColor(renderer, 150, 100, 0, 255);
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

// Draw graph showing two lines and their intersection
void drawGraph(SDL_Renderer* renderer, TTF_Font* fontSmall, double a1, double b1, double c1, 
               double a2, double b2, double c2, double solX, double solY, int hasSolution) {
    int graphX = 1090;
    int graphY = 210;
    int graphW = 480;
    int graphH = 420;
    int scale = 40;
    int centerX = graphX + graphW / 2;
    int centerY = graphY + graphH / 2;
    
    // Background
    SDL_SetRenderDrawColor(renderer, 255, 252, 240, 255);
    SDL_Rect graphRect = {graphX, graphY, graphW, graphH};
    SDL_RenderFillRect(renderer, &graphRect);
    
    // Border
    SDL_SetRenderDrawColor(renderer, 200, 170, 100, 255);
    SDL_RenderDrawRect(renderer, &graphRect);
    
    // Grid
    SDL_SetRenderDrawColor(renderer, 242, 228, 200, 255);
    for (int i = graphX; i <= graphX + graphW; i += scale) {
        SDL_RenderDrawLine(renderer, i, graphY, i, graphY + graphH);
    }
    for (int i = graphY; i <= graphY + graphH; i += scale) {
        SDL_RenderDrawLine(renderer, graphX, i, graphX + graphW, i);
    }
    
    // Axes
    SDL_SetRenderDrawColor(renderer, 140, 100, 40, 255);
    SDL_RenderDrawLine(renderer, centerX, graphY, centerX, graphY + graphH);
    SDL_RenderDrawLine(renderer, graphX, centerY, graphX + graphW, centerY);
    
    // Axis labels
    renderText(renderer, fontSmall, "x", graphX + graphW - 15, centerY + 5, (SDL_Color){120, 80, 0, 255});
    renderText(renderer, fontSmall, "y", centerX + 5, graphY + 5, (SDL_Color){120, 80, 0, 255});
    
    // Tick marks
    SDL_SetRenderDrawColor(renderer, 140, 100, 40, 255);
    for (int i = graphX; i <= graphX + graphW; i += scale) {
        if (i != centerX) SDL_RenderDrawLine(renderer, i, centerY - 3, i, centerY + 3);
    }
    for (int i = graphY; i <= graphY + graphH; i += scale) {
        if (i != centerY) SDL_RenderDrawLine(renderer, centerX - 3, i, centerX + 3, i);
    }
    
    // Draw Line 1 (red)
    if (b1 != 0) {
        SDL_SetRenderDrawColor(renderer, 200, 70, 70, 255);
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
            SDL_SetRenderDrawColor(renderer, 200, 70, 70, 255);
            SDL_RenderDrawLine(renderer, px, graphY, px, graphY + graphH);
        }
    }
    
    // Draw Line 2 (blue)
    if (b2 != 0) {
        SDL_SetRenderDrawColor(renderer, 70, 70, 200, 255);
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
            SDL_SetRenderDrawColor(renderer, 70, 70, 200, 255);
            SDL_RenderDrawLine(renderer, px, graphY, px, graphY + graphH);
        }
    }
    
    // Intersection point
    if (hasSolution) {
        int px = centerX + (int)(solX * scale);
        int py = centerY - (int)(solY * scale);
        if (px >= graphX && px < graphX + graphW && py >= graphY && py < graphY + graphH) {
            // Outer glow
            SDL_SetRenderDrawColor(renderer, 255, 200, 100, 255);
            for (int i = -12; i <= 12; i++)
                for (int j = -12; j <= 12; j++)
                    if (i*i + j*j <= 144 && i*i + j*j > 64)
                        SDL_RenderDrawPoint(renderer, px + i, py + j);
            // Inner dot
            SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255);
            for (int i = -8; i <= 8; i++)
                for (int j = -8; j <= 8; j++)
                    if (i*i + j*j <= 64)
                        SDL_RenderDrawPoint(renderer, px + i, py + j);
        }
    }
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    
    SDL_Window* window = SDL_CreateWindow("Gaussian Elimination - 2 Variables",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    
    // Load fonts
    TTF_Font* font = TTF_OpenFont("font.ttf", 18);
    TTF_Font* fontSmall = TTF_OpenFont("font.ttf", 14);
    TTF_Font* fontMedium = TTF_OpenFont("font.ttf", 16);
    TTF_Font* fontLarge = TTF_OpenFont("font.ttf", 20);
    TTF_Font* fontTitle = TTF_OpenFont("font.ttf", 26);
    TTF_Font* fontStep = TTF_OpenFont("font.ttf", 15);
    
    if (!font || !fontSmall || !fontMedium || !fontLarge || !fontTitle || !fontStep) {
        printf("Error loading font: %s\n", TTF_GetError());
        return 1;
    }
    
    // Initialize input boxes (a1, b1, c1, a2, b2, c2)
    InputBox inputs[6];
    const char* labels[] = {"a1:", "b1:", "c1:", "a2:", "b2:", "c2:"};
    for (int i = 0; i < 6; i++) {
        inputs[i].rect = (SDL_Rect){0, 0, 100, 35}; // Positions set during render
        strcpy(inputs[i].label, labels[i]);
        strcpy(inputs[i].value, "");
        inputs[i].active = 0;
    }
    
    Button computeBtn = {{0, 0, 170, 48}, "COMPUTE", 0, 0};
    Button clearBtn = {{0, 0, 170, 48}, "CLEAR", 0, 0};
    
    // State variables
    char resultText[500] = "Enter coefficients for both equations";
    int hasSolution = 0;
    double solX = 0, solY = 0;
    double a1 = 0, b1 = 0, c1 = 0, a2 = 0, b2 = 0, c2 = 0;
    
    // Step data
    double s_multiplier = 0;
    double s_new_b2 = 0, s_new_c2 = 0;
    double s_verify1 = 0, s_verify2 = 0;
    int hasSteps = 0;
    int specialCase = 0;
    
    int activeInput = -1;
    int quit = 0;
    SDL_Event e;
    
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = 1;
            
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                int mx = e.button.x;
                int my = e.button.y;
                
                activeInput = -1;
                for (int i = 0; i < 6; i++) {
                    if (mx >= inputs[i].rect.x && mx <= inputs[i].rect.x + inputs[i].rect.w &&
                        my >= inputs[i].rect.y && my <= inputs[i].rect.y + inputs[i].rect.h) {
                        activeInput = i;
                    }
                    inputs[i].active = (i == activeInput);
                }
                
                if (mx >= computeBtn.rect.x && mx <= computeBtn.rect.x + computeBtn.rect.w &&
                    my >= computeBtn.rect.y && my <= computeBtn.rect.y + computeBtn.rect.h) {
                    computeBtn.clicked = 1;
                    
                    a1 = atof(inputs[0].value);
                    b1 = atof(inputs[1].value);
                    c1 = atof(inputs[2].value);
                    a2 = atof(inputs[3].value);
                    b2 = atof(inputs[4].value);
                    c2 = atof(inputs[5].value);
                    
                    hasSolution = 0;
                    hasSteps = 0;
                    specialCase = 0;
                    
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
                    hasSolution = 0;
                    hasSteps = 0;
                    specialCase = 0;
                    clearBtn.clicked = 1;
                }
            }
            
            if (e.type == SDL_MOUSEBUTTONUP) {
                computeBtn.clicked = 0;
                clearBtn.clicked = 0;
            }
            
            if (e.type == SDL_MOUSEMOTION) {
                int mx = e.motion.x;
                int my = e.motion.y;
                computeBtn.hovered = (mx >= computeBtn.rect.x && mx <= computeBtn.rect.x + computeBtn.rect.w &&
                                     my >= computeBtn.rect.y && my <= computeBtn.rect.y + computeBtn.rect.h);
                clearBtn.hovered = (mx >= clearBtn.rect.x && mx <= clearBtn.rect.x + clearBtn.rect.w &&
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
        
        // ==================== RENDER ====================
        SDL_SetRenderDrawColor(renderer, 255, 250, 235, 255);
        SDL_RenderClear(renderer);
        
        // ---- TOP BANNER ----
        SDL_SetRenderDrawColor(renderer, 200, 140, 20, 255);
        SDL_Rect banner = {0, 0, WINDOW_WIDTH, 70};
        SDL_RenderFillRect(renderer, &banner);
        // Subtle bottom shadow
        SDL_SetRenderDrawColor(renderer, 170, 110, 0, 255);
        SDL_RenderDrawLine(renderer, 0, 70, WINDOW_WIDTH, 70);
        
        SDL_Color white = {255, 255, 255, 255};
        SDL_Color cream = {255, 235, 200, 255};
        renderTextBold(renderer, fontTitle, "ELIMINATION METHOD", 30, 18, white);
        renderText(renderer, fontLarge, "System of Linear Equations (2 Variables)", 530, 23, cream);
        
        renderText(renderer, fontSmall, "MT211 - Numerical Method  |  Semestral Project", 1200, 10, cream);
        renderText(renderer, fontSmall, "BSCPE 22001  |  Francis John Rodela | Joshua Deolino", 1200, 32, cream);
        
        // ---- LEFT PANEL: Input ----
        SDL_Color panelBg = {255, 252, 242, 255};
        SDL_Color panelBorder = {220, 190, 130, 255};
        drawPanel(renderer, 15, 85, 490, 800, panelBg, panelBorder);
        
        SDL_Color sectionColor = {150, 100, 0, 255};
        SDL_Color darkText = {80, 50, 0, 255};
        
        renderTextBold(renderer, fontLarge, "INPUT COEFFICIENTS", 130, 100, sectionColor);
        
        // Equation format reference
        SDL_Color eqBg = {255, 248, 225, 255};
        SDL_Color eqBorder = {220, 190, 130, 255};
        drawPanel(renderer, 35, 135, 450, 65, eqBg, eqBorder);
        
        SDL_Color formulaColor = {120, 80, 0, 255};
        renderText(renderer, font, "Eq 1:  a1*x  +  b1*y  =  c1", 55, 143, formulaColor);
        renderText(renderer, font, "Eq 2:  a2*x  +  b2*y  =  c2", 55, 170, formulaColor);
        
        // ---- EQUATION 1 INPUT ROW ----
        renderTextBold(renderer, font, "EQUATION 1", 180, 215, sectionColor);
        
        SDL_SetRenderDrawColor(renderer, 255, 248, 230, 255);
        SDL_Rect eq1Bg = {35, 245, 450, 60};
        SDL_RenderFillRect(renderer, &eq1Bg);
        SDL_SetRenderDrawColor(renderer, 230, 210, 170, 255);
        SDL_RenderDrawRect(renderer, &eq1Bg);
        
        // Position input boxes for Equation 1 in a row: [a1] x + [b1] y = [c1]
        inputs[0].rect = (SDL_Rect){80, 257, 85, 35};
        inputs[1].rect = (SDL_Rect){225, 257, 85, 35};
        inputs[2].rect = (SDL_Rect){385, 257, 85, 35};
        
        
        // ---- EQUATION 2 INPUT ROW ----
        renderTextBold(renderer, font, "EQUATION 2", 180, 320, sectionColor);
        
        SDL_SetRenderDrawColor(renderer, 255, 248, 230, 255);
        SDL_Rect eq2Bg = {35, 350, 450, 60};
        SDL_RenderFillRect(renderer, &eq2Bg);
        SDL_SetRenderDrawColor(renderer, 230, 210, 170, 255);
        SDL_RenderDrawRect(renderer, &eq2Bg);
        
        // Position input boxes for Equation 2
        inputs[3].rect = (SDL_Rect){80, 362, 85, 35};
        inputs[4].rect = (SDL_Rect){225, 362, 85, 35};
        inputs[5].rect = (SDL_Rect){385, 362, 85, 35};
        

        // Render all input boxes
        for (int i = 0; i < 6; i++) {
            renderInputBox(renderer, font, &inputs[i]);
        }
        
        // Buttons
        computeBtn.rect = (SDL_Rect){80, 440, 170, 48};
        clearBtn.rect = (SDL_Rect){275, 440, 170, 48};
        renderButton(renderer, font, &computeBtn);
        renderButton(renderer, font, &clearBtn);
        
        // ---- STATUS ----
        renderTextBold(renderer, font, "STATUS", 215, 510, sectionColor);
        drawPanel(renderer, 35, 540, 450, 60, eqBg, eqBorder);
        
        if (strlen(resultText) > 0) {
            char resultCopy[500];
            strcpy(resultCopy, resultText);
            char* line = strtok(resultCopy, "\n");
            int ry = 547;
            while (line) {
                SDL_Color resultColor = hasSolution ? (SDL_Color){0, 128, 0, 255} : (SDL_Color){178, 34, 34, 255};
                renderText(renderer, fontMedium, line, 50, ry, resultColor);
                ry += 22;
                line = strtok(NULL, "\n");
            }
        }
        
        // ---- SOLUTION BOX ----
        if (hasSolution) {
            renderTextBold(renderer, font, "FINAL ANSWER", 190, 620, sectionColor);
            
            SDL_Color solBg = {235, 255, 225, 255};
            SDL_Color solBorder = {100, 180, 100, 255};
            drawPanel(renderer, 35, 650, 450, 100, solBg, solBorder);
            
            SDL_Color conclusionColor = {0, 80, 0, 255};
            char buffer[200];
            
            sprintf(buffer, "x = %.6f", solX);
            renderTextBold(renderer, fontLarge, buffer, 55, 665, conclusionColor);
            
            sprintf(buffer, "y = %.6f", solY);
            renderTextBold(renderer, fontLarge, buffer, 260, 665, conclusionColor);
            
            sprintf(buffer, "Point of Intersection: (%.4f, %.4f)", solX, solY);
            renderText(renderer, font, buffer, 55, 718, (SDL_Color){0, 100, 0, 255});
        }
        
        // ---- CENTER PANEL: Solution Steps ----
        drawPanel(renderer, 520, 85, 545, 800, panelBg, panelBorder);
        renderTextBold(renderer, fontLarge, "SOLUTION STEPS", 695, 100, sectionColor);
        
        if (hasSteps) {
            char buf[200];
            int sy = 135;
            
            // Step 0: Original System
            SDL_Color stepBg0 = {255, 245, 225, 255};
            drawPanel(renderer, 535, sy, 515, 80, stepBg0, (SDL_Color){220, 190, 130, 255});
            renderTextBold(renderer, fontMedium, "GIVEN: Original System", 550, sy + 5, sectionColor);
            SDL_SetRenderDrawColor(renderer, 220, 190, 130, 255);
            SDL_RenderDrawLine(renderer, 550, sy + 25, 1040, sy + 25);
            
            sprintf(buf, "Eq1:  %.2fx + %.2fy = %.2f", a1, b1, c1);
            renderText(renderer, font, buf, 560, sy + 30, (SDL_Color){200, 70, 70, 255});
            sprintf(buf, "Eq2:  %.2fx + %.2fy = %.2f", a2, b2, c2);
            renderText(renderer, font, buf, 560, sy + 55, (SDL_Color){70, 70, 200, 255});
            
            sy += 95;
            
            // Step 1: Forward Elimination
            SDL_Color stepBg1 = {255, 240, 215, 255};
            drawPanel(renderer, 535, sy, 515, 130, stepBg1, (SDL_Color){220, 180, 100, 255});
            renderTextBold(renderer, fontMedium, "STEP 1: Forward Elimination", 550, sy + 5, sectionColor);
            SDL_SetRenderDrawColor(renderer, 220, 180, 100, 255);
            SDL_RenderDrawLine(renderer, 550, sy + 25, 1040, sy + 25);
            
            sprintf(buf, "Find multiplier:  m = a2 / a1 = %.4f / %.4f", a2, a1);
            renderText(renderer, fontStep, buf, 560, sy + 32, darkText);
            sprintf(buf, "m = %.6f", s_multiplier);
            renderTextBold(renderer, font, buf, 560, sy + 55, (SDL_Color){180, 100, 0, 255});
            
            renderText(renderer, fontStep, "Eliminate x:  New Eq2 = Eq2 - (m * Eq1)", 560, sy + 80, darkText);
            sprintf(buf, "Result:  0x + (%.6f)y = %.6f", s_new_b2, s_new_c2);
            renderTextBold(renderer, fontStep, buf, 560, sy + 103, (SDL_Color){180, 100, 0, 255});
            
            sy += 145;
            
            if (specialCase == 1) {
                SDL_Color warnBg = {255, 255, 220, 255};
                drawPanel(renderer, 535, sy, 515, 60, warnBg, (SDL_Color){200, 180, 0, 255});
                renderTextBold(renderer, font, "All coefficients became 0", 560, sy + 8, (SDL_Color){150, 130, 0, 255});
                renderText(renderer, font, "Equations are dependent - infinite solutions", 560, sy + 33, (SDL_Color){150, 130, 0, 255});
            } else if (specialCase == 2) {
                SDL_Color errBg = {255, 230, 230, 255};
                drawPanel(renderer, 535, sy, 515, 60, errBg, (SDL_Color){200, 100, 100, 255});
                renderTextBold(renderer, font, "Coefficient of y = 0, but constant != 0", 560, sy + 8, (SDL_Color){178, 34, 34, 255});
                renderText(renderer, font, "Equations are inconsistent - no solution", 560, sy + 33, (SDL_Color){178, 34, 34, 255});
            } else if (hasSolution) {
                // Step 2: Solve for y
                SDL_Color stepBg2 = {230, 250, 220, 255};
                drawPanel(renderer, 535, sy, 515, 80, stepBg2, (SDL_Color){130, 180, 100, 255});
                renderTextBold(renderer, fontMedium, "STEP 2: Back Substitution - Solve for y", 550, sy + 5, (SDL_Color){0, 100, 0, 255});
                SDL_SetRenderDrawColor(renderer, 130, 180, 100, 255);
                SDL_RenderDrawLine(renderer, 550, sy + 25, 1040, sy + 25);
                
                sprintf(buf, "y = %.6f / %.6f", s_new_c2, s_new_b2);
                renderText(renderer, fontStep, buf, 560, sy + 32, darkText);
                sprintf(buf, "y = %.6f", solY);
                renderTextBold(renderer, font, buf, 560, sy + 55, (SDL_Color){0, 120, 0, 255});
                
                sy += 95;
                
                // Step 3: Solve for x
                SDL_Color stepBg3 = {220, 240, 255, 255};
                drawPanel(renderer, 535, sy, 515, 100, stepBg3, (SDL_Color){100, 150, 200, 255});
                renderTextBold(renderer, fontMedium, "STEP 3: Substitute y into Eq1 - Solve for x", 550, sy + 5, (SDL_Color){0, 60, 140, 255});
                SDL_SetRenderDrawColor(renderer, 100, 150, 200, 255);
                SDL_RenderDrawLine(renderer, 550, sy + 25, 1040, sy + 25);
                
                sprintf(buf, "%.2fx + %.2f(%.6f) = %.2f", a1, b1, solY, c1);
                renderText(renderer, fontStep, buf, 560, sy + 32, darkText);
                sprintf(buf, "%.2fx = %.6f", a1, c1 - b1 * solY);
                renderText(renderer, fontStep, buf, 560, sy + 55, darkText);
                sprintf(buf, "x = %.6f", solX);
                renderTextBold(renderer, font, buf, 560, sy + 75, (SDL_Color){0, 60, 140, 255});
                
                sy += 115;
                
                // Step 4: Verification
                SDL_Color stepBg4 = {245, 240, 255, 255};
                drawPanel(renderer, 535, sy, 515, 105, stepBg4, (SDL_Color){150, 130, 200, 255});
                renderTextBold(renderer, fontMedium, "VERIFICATION", 550, sy + 5, (SDL_Color){100, 60, 160, 255});
                SDL_SetRenderDrawColor(renderer, 150, 130, 200, 255);
                SDL_RenderDrawLine(renderer, 550, sy + 25, 1040, sy + 25);
                
                int check1 = fabs(s_verify1 - c1) < 0.01;
                sprintf(buf, "Eq1: %.2f(%.4f) + %.2f(%.4f) = %.4f", a1, solX, b1, solY, s_verify1);
                renderText(renderer, fontStep, buf, 560, sy + 32, darkText);
                sprintf(buf, "Expected: %.2f    %s", c1, check1 ? "PASS" : "FAIL");
                renderText(renderer, fontStep, buf, 560, sy + 52, check1 ? (SDL_Color){0, 128, 0, 255} : (SDL_Color){200, 0, 0, 255});
                
                int check2 = fabs(s_verify2 - c2) < 0.01;
                sprintf(buf, "Eq2: %.2f(%.4f) + %.2f(%.4f) = %.4f", a2, solX, b2, solY, s_verify2);
                renderText(renderer, fontStep, buf, 560, sy + 75, darkText);
                sprintf(buf, "Expected: %.2f    %s", c2, check2 ? "PASS" : "FAIL");
                renderText(renderer, fontStep, buf, 560, sy + 95, check2 ? (SDL_Color){0, 128, 0, 255} : (SDL_Color){200, 0, 0, 255});
            }
        } else {
            renderText(renderer, font, "Enter coefficients and press COMPUTE", 620, 420, (SDL_Color){180, 160, 120, 255});
            renderText(renderer, font, "to see the step-by-step solution here.", 615, 450, (SDL_Color){180, 160, 120, 255});
        }
        
        // ---- RIGHT PANEL: Graph ----
        drawPanel(renderer, 1080, 85, 505, 800, panelBg, panelBorder);
        renderTextBold(renderer, fontLarge, "GRAPH", 1290, 100, sectionColor);
        renderText(renderer, fontSmall, "Visual representation of the two lines", 1180, 125, (SDL_Color){150, 130, 90, 255});
        
        drawGraph(renderer, fontSmall, a1, b1, c1, a2, b2, c2, solX, solY, hasSolution);
        
        // Legend
        int legendY = 660;
        drawPanel(renderer, 1095, legendY, 475, 115, eqBg, panelBorder);
        renderTextBold(renderer, fontMedium, "LEGEND", 1290, legendY + 8, sectionColor);
        
        SDL_SetRenderDrawColor(renderer, 200, 70, 70, 255);
        SDL_Rect l1 = {1115, legendY + 42, 30, 4};
        SDL_RenderFillRect(renderer, &l1);
        renderText(renderer, fontMedium, "Equation 1", 1155, legendY + 35, (SDL_Color){200, 70, 70, 255});
        
        SDL_SetRenderDrawColor(renderer, 70, 70, 200, 255);
        SDL_Rect l2 = {1115, legendY + 68, 30, 4};
        SDL_RenderFillRect(renderer, &l2);
        renderText(renderer, fontMedium, "Equation 2", 1155, legendY + 61, (SDL_Color){70, 70, 200, 255});
        
        SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255);
        for (int i = -6; i <= 6; i++)
            for (int j = -6; j <= 6; j++)
                if (i*i + j*j <= 36)
                    SDL_RenderDrawPoint(renderer, 1130 + i, legendY + 95 + j);
        renderText(renderer, fontMedium, "Solution Point", 1155, legendY + 87, (SDL_Color){200, 120, 0, 255});
        
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    
    TTF_CloseFont(font);
    TTF_CloseFont(fontSmall);
    TTF_CloseFont(fontMedium);
    TTF_CloseFont(fontLarge);
    TTF_CloseFont(fontTitle);
    TTF_CloseFont(fontStep);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    
    return 0;
}
