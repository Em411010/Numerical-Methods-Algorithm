#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define WINDOW_WIDTH 1500
#define WINDOW_HEIGHT 950
#define MAX_ITERATIONS 100
#define TOLERANCE 0.0001

// Iteration data structure
typedef struct {
    int n;
    double x_prev;
    double x_curr;
    double f_prev;
    double f_curr;
    double x_next;
    double error;
} IterationData;

// UI component structures
typedef struct {
    SDL_Rect rect;
    char label[80];
    char value[50];
    int active;
} InputBox;

typedef struct {
    SDL_Rect rect;
    char text[50];
    int hovered;
    int clicked;
} Button;

// Global iteration storage
IterationData iterations[MAX_ITERATIONS];
int iterationCount = 0;

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

// Render bold text
void renderTextBold(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    renderText(renderer, font, text, x, y, color);
    renderText(renderer, font, text, x + 1, y, color);
}

// Draw a styled panel
void drawPanel(SDL_Renderer* renderer, int x, int y, int w, int h, SDL_Color bg, SDL_Color border, int borderWidth) {
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(renderer, &r);
    
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    for (int i = 0; i < borderWidth; i++) {
        SDL_Rect br = {x + i, y + i, w - 2*i, h - 2*i};
        SDL_RenderDrawRect(renderer, &br);
    }
}

// Draw rounded corner effect
void drawRoundedPanel(SDL_Renderer* renderer, int x, int y, int w, int h, SDL_Color bg) {
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
    SDL_Rect r = {x + 2, y + 2, w - 4, h - 4};
    SDL_RenderFillRect(renderer, &r);
    
    // Corner pixels
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
    SDL_RenderDrawLine(renderer, x + 1, y + 2, x + 1, y + h - 3);
    SDL_RenderDrawLine(renderer, x + w - 2, y + 2, x + w - 2, y + h - 3);
    SDL_RenderDrawLine(renderer, x + 2, y + 1, x + w - 3, y + 1);
    SDL_RenderDrawLine(renderer, x + 2, y + h - 2, x + w - 3, y + h - 2);
}

// Render input box
void renderInputBox(SDL_Renderer* renderer, TTF_Font* font, InputBox* box) {
    // Box fill
    if (box->active) {
        SDL_SetRenderDrawColor(renderer, 255, 240, 240, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 250, 250, 255);
    }
    SDL_RenderFillRect(renderer, &box->rect);
    
    // Box border
    if (box->active) {
        SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255);
        SDL_RenderDrawRect(renderer, &box->rect);
        SDL_Rect inner = {box->rect.x + 1, box->rect.y + 1, box->rect.w - 2, box->rect.h - 2};
        SDL_RenderDrawRect(renderer, &inner);
    } else {
        SDL_SetRenderDrawColor(renderer, 180, 100, 100, 255);
        SDL_RenderDrawRect(renderer, &box->rect);
    }
    
    // Label
    SDL_Color labelColor = {120, 30, 30, 255};
    renderTextBold(renderer, font, box->label, box->rect.x, box->rect.y - 25, labelColor);
    
    // Value text
    SDL_Color textColor = {80, 20, 20, 255};
    if (strlen(box->value) > 0) {
        renderText(renderer, font, box->value, box->rect.x + 10, box->rect.y + 8, textColor);
    }
}

// Render button
void renderButton(SDL_Renderer* renderer, TTF_Font* font, Button* btn) {
    SDL_Color bgColor, textColor = {255, 255, 255, 255};
    
    if (btn->clicked) {
        bgColor = (SDL_Color){150, 30, 30, 255};
    } else if (btn->hovered) {
        bgColor = (SDL_Color){220, 60, 60, 255};
    } else {
        bgColor = (SDL_Color){190, 50, 50, 255};
    }
    
    // Shadow effect
    SDL_SetRenderDrawColor(renderer, 100, 20, 20, 255);
    SDL_Rect shadow = {btn->rect.x + 3, btn->rect.y + 3, btn->rect.w, btn->rect.h};
    SDL_RenderFillRect(renderer, &shadow);
    
    // Button
    SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
    SDL_RenderFillRect(renderer, &btn->rect);
    
    SDL_SetRenderDrawColor(renderer, 120, 20, 20, 255);
    SDL_RenderDrawRect(renderer, &btn->rect);
    
    // Text
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

// Exponential function: e^x - ax - b
double function(double x, double a, double b) {
    return exp(x) - a * x - b;
}

// Draw graph with function and convergence visualization
void drawGraph(SDL_Renderer* renderer, TTF_Font* fontSmall, double a, double b, 
               double root, int hasRoot) {
    int graphX = 40;
    int graphY = 540;
    int graphW = 660;
    int graphH = 360;
    
    // Background
    SDL_SetRenderDrawColor(renderer, 255, 252, 248, 255);
    SDL_Rect graphRect = {graphX, graphY, graphW, graphH};
    SDL_RenderFillRect(renderer, &graphRect);
    
    // Border
    drawPanel(renderer, graphX, graphY, graphW, graphH, 
              (SDL_Color){255, 252, 248, 255}, (SDL_Color){180, 80, 80, 255}, 2);
    
    // Determine range
    double x_min = hasRoot ? root - 3 : -2;
    double x_max = hasRoot ? root + 3 : 4;
    double y_min = -5, y_max = 5;
    
    // Find y range
    if (hasRoot) {
        double testY = function(root, a, b);
        for (double tx = x_min; tx <= x_max; tx += 0.5) {
            double ty = function(tx, a, b);
            if (ty < y_min && ty > -100) y_min = ty;
            if (ty > y_max && ty < 100) y_max = ty;
        }
        y_min *= 1.2;
        y_max *= 1.2;
    }
    
    double scaleX = graphW / (x_max - x_min);
    double scaleY = graphH / (y_max - y_min);
    
    int originX = graphX + (int)((-x_min) * scaleX);
    int originY = graphY + graphH - (int)((-y_min) * scaleY);
    
    // Grid
    SDL_SetRenderDrawColor(renderer, 245, 230, 230, 255);
    for (int gx = graphX; gx < graphX + graphW; gx += 40) {
        SDL_RenderDrawLine(renderer, gx, graphY, gx, graphY + graphH);
    }
    for (int gy = graphY; gy < graphY + graphH; gy += 40) {
        SDL_RenderDrawLine(renderer, graphX, gy, graphX + graphW, gy);
    }
    
    // Axes
    SDL_SetRenderDrawColor(renderer, 100, 40, 40, 255);
    if (originY >= graphY && originY <= graphY + graphH)
        SDL_RenderDrawLine(renderer, graphX, originY, graphX + graphW, originY);
    if (originX >= graphX && originX <= graphX + graphW)
        SDL_RenderDrawLine(renderer, originX, graphY, originX, graphY + graphH);
    
    // Draw function curve
    SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255);
    for (int px = 0; px < graphW - 1; px++) {
        double x1 = x_min + (px / (double)graphW) * (x_max - x_min);
        double x2 = x_min + ((px + 1) / (double)graphW) * (x_max - x_min);
        double y1 = function(x1, a, b);
        double y2 = function(x2, a, b);
        
        if (fabs(y1) < 100 && fabs(y2) < 100) {
            int py1 = graphY + graphH - (int)((y1 - y_min) * scaleY);
            int py2 = graphY + graphH - (int)((y2 - y_min) * scaleY);
            
            if (py1 >= graphY && py1 <= graphY + graphH && py2 >= graphY && py2 <= graphY + graphH) {
                SDL_RenderDrawLine(renderer, graphX + px, py1, graphX + px + 1, py2);
            }
        }
    }
    
    // Draw secant lines for iterations
    if (iterationCount > 0 && hasRoot) {
        SDL_SetRenderDrawColor(renderer, 255, 150, 100, 150);
        for (int i = 0; i < iterationCount && i < 8; i++) {
            double x1 = iterations[i].x_prev;
            double x2 = iterations[i].x_curr;
            double y1 = iterations[i].f_prev;
            double y2 = iterations[i].f_curr;
            
            if (x1 >= x_min && x1 <= x_max && x2 >= x_min && x2 <= x_max) {
                int px1 = graphX + (int)((x1 - x_min) * scaleX);
                int py1 = graphY + graphH - (int)((y1 - y_min) * scaleY);
                int px2 = graphX + (int)((x2 - x_min) * scaleX);
                int py2 = graphY + graphH - (int)((y2 - y_min) * scaleY);
                
                if (py1 >= graphY && py1 <= graphY + graphH && py2 >= graphY && py2 <= graphY + graphH) {
                    SDL_RenderDrawLine(renderer, px1, py1, px2, py2);
                }
            }
        }
    }
    
    // Draw iteration points
    if (iterationCount > 0 && hasRoot) {
        for (int i = 0; i < iterationCount && i < 10; i++) {
            double x = iterations[i].x_curr;
            double y = iterations[i].f_curr;
            
            if (x >= x_min && x <= x_max && fabs(y) < 100) {
                int px = graphX + (int)((x - x_min) * scaleX);
                int py = graphY + graphH - (int)((y - y_min) * scaleY);
                
                if (px >= graphX && px < graphX + graphW && py >= graphY && py < graphY + graphH) {
                    // Gradient from orange to red
                    int r = 255 - (i * 15);
                    int g = 150 - (i * 12);
                    SDL_SetRenderDrawColor(renderer, r, g, 0, 255);
                    
                    for (int dx = -5; dx <= 5; dx++) {
                        for (int dy = -5; dy <= 5; dy++) {
                            if (dx*dx + dy*dy <= 25) {
                                SDL_RenderDrawPoint(renderer, px + dx, py + dy);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Root point
    if (hasRoot) {
        double rx = root;
        double ry = function(rx, a, b);
        
        if (rx >= x_min && rx <= x_max && fabs(ry) < 100) {
            int px = graphX + (int)((rx - x_min) * scaleX);
            int py = graphY + graphH - (int)((ry - y_min) * scaleY);
            
            if (px >= graphX && px < graphX + graphW && py >= graphY && py < graphY + graphH) {
                // Glow effect
                SDL_SetRenderDrawColor(renderer, 255, 200, 150, 255);
                for (int r = 10; r > 6; r--) {
                    for (int angle = 0; angle < 360; angle += 10) {
                        int dx = (int)(r * cos(angle * M_PI / 180));
                        int dy = (int)(r * sin(angle * M_PI / 180));
                        SDL_RenderDrawPoint(renderer, px + dx, py + dy);
                    }
                }
                
                // Center dot
                SDL_SetRenderDrawColor(renderer, 220, 20, 60, 255);
                for (int dx = -6; dx <= 6; dx++) {
                    for (int dy = -6; dy <= 6; dy++) {
                        if (dx*dx + dy*dy <= 36) {
                            SDL_RenderDrawPoint(renderer, px + dx, py + dy);
                        }
                    }
                }
            }
        }
    }
    
    // Labels
    renderText(renderer, fontSmall, "y", originX + 8, graphY + 5, (SDL_Color){100, 40, 40, 255});
    renderText(renderer, fontSmall, "x", graphX + graphW - 15, originY + 5, (SDL_Color){100, 40, 40, 255});
    renderTextBold(renderer, fontSmall, "f(x) = e^x - ax - b", graphX + 15, graphY + 15, (SDL_Color){200, 50, 50, 255});
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    
    SDL_Window* window = SDL_CreateWindow("Secant Method - Exponential Equations",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    
    // Load fonts
    TTF_Font* fontTitle = TTF_OpenFont("font.ttf", 28);
    TTF_Font* fontLarge = TTF_OpenFont("font.ttf", 20);
    TTF_Font* fontMedium = TTF_OpenFont("font.ttf", 18);
    TTF_Font* font = TTF_OpenFont("font.ttf", 16);
    TTF_Font* fontSmall = TTF_OpenFont("font.ttf", 14);
    TTF_Font* fontTiny = TTF_OpenFont("font.ttf", 13);
    
    if (!fontTitle || !fontLarge || !fontMedium || !font || !fontSmall || !fontTiny) {
        printf("Error loading font: %s\n", TTF_GetError());
        return 1;
    }
    
    // Initialize input boxes
    InputBox inputs[4];
    const char* labels[] = {
        "Coefficient 'a' (in e^x - ax - b = 0)",
        "Constant 'b' (in e^x - ax - b = 0)",
        "First Initial Guess (x0)",
        "Second Initial Guess (x1)"
    };
    
    for (int i = 0; i < 4; i++) {
        inputs[i].rect = (SDL_Rect){60 + (i % 2) * 290, 250 + (i / 2) * 90, 240, 40};
        strcpy(inputs[i].label, labels[i]);
        strcpy(inputs[i].value, "");
        inputs[i].active = 0;
    }
    
    Button computeBtn = {{230, 450, 180, 50}, "COMPUTE", 0, 0};
    Button clearBtn = {{440, 450, 180, 50}, "CLEAR", 0, 0};
    
    // State variables
    int activeInput = -1;
    int hasResult = 0;
    double root = 0;
    double a_val = 0, b_val = 0;
    char statusMsg[300] = "Ready to compute. Enter values and press COMPUTE.";
    int statusSuccess = 0;
    int scrollOffset = 0;
    
    int quit = 0;
    SDL_Event e;
    
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = 1;
            
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                int mx = e.button.x;
                int my = e.button.y;
                
                activeInput = -1;
                for (int i = 0; i < 4; i++) {
                    if (mx >= inputs[i].rect.x && mx <= inputs[i].rect.x + inputs[i].rect.w &&
                        my >= inputs[i].rect.y && my <= inputs[i].rect.y + inputs[i].rect.h) {
                        activeInput = i;
                    }
                    inputs[i].active = (i == activeInput);
                }
                
                // Compute button
                if (mx >= computeBtn.rect.x && mx <= computeBtn.rect.x + computeBtn.rect.w &&
                    my >= computeBtn.rect.y && my <= computeBtn.rect.y + computeBtn.rect.h) {
                    computeBtn.clicked = 1;
                    
                    a_val = atof(inputs[0].value);
                    b_val = atof(inputs[1].value);
                    double x0 = atof(inputs[2].value);
                    double x1 = atof(inputs[3].value);
                    
                    hasResult = 0;
                    iterationCount = 0;
                    statusSuccess = 0;
                    
                    // Validation
                    if (fabs(x1 - x0) < 1e-10) {
                        sprintf(statusMsg, "ERROR: x0 and x1 must be different!\nPlease choose two distinct initial guesses.");
                    } else {
                        double x_prev = x0;
                        double x_curr = x1;
                        double f_prev = function(x_prev, a_val, b_val);
                        double f_curr = function(x_curr, a_val, b_val);
                        int converged = 0;
                        
                        for (int iter = 0; iter < MAX_ITERATIONS; iter++) {
                            double denominator = f_curr - f_prev;
                            
                            if (fabs(denominator) < 1e-10) {
                                sprintf(statusMsg, "ERROR: Division by zero at iteration %d\nf(x%d) = f(x%d), cannot continue.", 
                                        iter + 1, iter, iter + 1);
                                break;
                            }
                            
                            double x_next = x_curr - f_curr * (x_curr - x_prev) / denominator;
                            double error = fabs(x_next - x_curr);
                            
                            // Store iteration
                            iterations[iterationCount].n = iter + 1;
                            iterations[iterationCount].x_prev = x_prev;
                            iterations[iterationCount].x_curr = x_curr;
                            iterations[iterationCount].f_prev = f_prev;
                            iterations[iterationCount].f_curr = f_curr;
                            iterations[iterationCount].x_next = x_next;
                            iterations[iterationCount].error = error;
                            iterationCount++;
                            
                            if (error < TOLERANCE || fabs(function(x_next, a_val, b_val)) < TOLERANCE) {
                                root = x_next;
                                hasResult = 1;
                                statusSuccess = 1;
                                sprintf(statusMsg, "SUCCESS! Converged in %d iterations.\nApproximate root: x = %.3f", 
                                        iterationCount, root);
                                converged = 1;
                                break;
                            }
                            
                            x_prev = x_curr;
                            x_curr = x_next;
                            f_prev = f_curr;
                            f_curr = function(x_curr, a_val, b_val);
                        }
                        
                        if (!converged && iterationCount > 0) {
                            sprintf(statusMsg, "Did not converge in %d iterations.\nTry different initial guesses.", MAX_ITERATIONS);
                        }
                    }
                }
                
                // Clear button
                if (mx >= clearBtn.rect.x && mx <= clearBtn.rect.x + clearBtn.rect.w &&
                    my >= clearBtn.rect.y && my <= clearBtn.rect.y + clearBtn.rect.h) {
                    clearBtn.clicked = 1;
                    for (int i = 0; i < 4; i++) strcpy(inputs[i].value, "");
                    strcpy(statusMsg, "Ready to compute. Enter values and press COMPUTE.");
                    hasResult = 0;
                    iterationCount = 0;
                    statusSuccess = 0;
                    scrollOffset = 0;
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
            
            if (e.type == SDL_MOUSEWHEEL) {
                scrollOffset -= e.wheel.y * 20;
                if (scrollOffset < 0) scrollOffset = 0;
                if (scrollOffset > iterationCount * 22) scrollOffset = iterationCount * 22;
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
        SDL_Color bgMain = {255, 248, 245, 255};
        SDL_SetRenderDrawColor(renderer, bgMain.r, bgMain.g, bgMain.b, bgMain.a);
        SDL_RenderClear(renderer);
        
        // ---- TOP BANNER ----
        SDL_SetRenderDrawColor(renderer, 190, 50, 50, 255);
        SDL_Rect banner = {0, 0, WINDOW_WIDTH, 80};
        SDL_RenderFillRect(renderer, &banner);
        
        // Gradient effect
        for (int i = 0; i < 5; i++) {
            SDL_SetRenderDrawColor(renderer, 220 - i * 10, 60 - i * 5, 60 - i * 5, 255);
            SDL_RenderDrawLine(renderer, 0, i, WINDOW_WIDTH, i);
        }
        for (int i = 0; i < 5; i++) {
            SDL_SetRenderDrawColor(renderer, 160 + i * 6, 40 + i * 2, 40 + i * 2, 255);
            SDL_RenderDrawLine(renderer, 0, 75 + i, WINDOW_WIDTH, 75 + i);
        }
        
        SDL_Color white = {255, 255, 255, 255};
        SDL_Color lightPink = {255, 220, 220, 255};
        
        renderTextBold(renderer, fontTitle, "SECANT METHOD", 50, 15, white);
        renderText(renderer, fontLarge, "for Exponential Equations", 50, 48, lightPink);
        
        renderText(renderer, fontSmall, "MT211 - Numerical Methods  |  Semestral Project", 950, 12, lightPink);
        renderText(renderer, font, "BSCPE 22001", 1050, 36, white);
        renderText(renderer, fontSmall, "Jayboy Acilo  |  Billy Jay Penalba", 1028, 60, lightPink);
        
        // ---- LEFT PANEL: Input & Graph ----
        drawPanel(renderer, 25, 95, 700, 830, (SDL_Color){255, 252, 250, 255}, 
                  (SDL_Color){180, 80, 80, 255}, 2);
        
        SDL_Color sectionColor = {140, 30, 30, 255};
        SDL_Color hintColor = {150, 90, 90, 255};
        
        // Method explanation box
        drawPanel(renderer, 40, 110, 670, 105, (SDL_Color){255, 245, 245, 255}, 
                  (SDL_Color){220, 120, 120, 255}, 1);
        
        renderTextBold(renderer, fontMedium, "HOW IT WORKS:", 55, 118, sectionColor);
        renderText(renderer, fontSmall, "The Secant Method finds roots using two initial points without", 55, 143, hintColor);
        renderText(renderer, fontSmall, "calculating derivatives. It draws secant lines between points to", 55, 163, hintColor);
        renderText(renderer, fontSmall, "converge to the root. Formula:", 55, 183, hintColor);
        renderTextBold(renderer, font, "x(n+1) = x(n) - f(x(n)) * [x(n) - x(n-1)] / [f(x(n)) - f(x(n-1))]", 255, 183, (SDL_Color){180, 40, 40, 255});
        
      
        
        for (int i = 0; i < 4; i++) {
            renderInputBox(renderer, font, &inputs[i]);
        }

        // Live equation preview using current input values
        double a_preview = NAN, b_preview = NAN;
        if (strlen(inputs[0].value) > 0) a_preview = atof(inputs[0].value);
        if (strlen(inputs[1].value) > 0) b_preview = atof(inputs[1].value);
        char eqPreview[200];
        if (!isnan(a_preview) && !isnan(b_preview)) {
            long a_int = lround(a_preview);
            long b_int = lround(b_preview);
            sprintf(eqPreview, "Equation: e^x - %ldx - %ld = 0", a_int, b_int);
        } else if (!isnan(a_preview)) {
            long a_int = lround(a_preview);
            sprintf(eqPreview, "Equation: e^x - %ldx - b = 0", a_int);
        } else if (!isnan(b_preview)) {
            long b_int = lround(b_preview);
            sprintf(eqPreview, "Equation: e^x - a x - %ld = 0", b_int);
        } else {
            strcpy(eqPreview, "Equation: e^x - a x - b = 0");
        }
        renderTextBold(renderer, fontMedium, eqPreview, 60, 380, (SDL_Color){140, 30, 30, 255});

        // Hint text
        renderText(renderer, fontTiny, "Note: x0 and x1 should be close to the expected root", 140, 410, hintColor);
        
        // Buttons
        renderButton(renderer, font, &computeBtn);
        renderButton(renderer, font, &clearBtn);
        
        // Status box
        drawPanel(renderer, 40, 515, 670, 10, (SDL_Color){255, 245, 240, 255}, 
                  (SDL_Color){200, 100, 100, 255}, 1);
        
        if (statusSuccess) {
            SDL_SetRenderDrawColor(renderer, 220, 255, 220, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 255, 240, 240, 255);
        }
        SDL_Rect statusBg = {40, 515, 670, 10};
        SDL_RenderFillRect(renderer, &statusBg);
        
        SDL_Color statusColor = statusSuccess ? (SDL_Color){0, 120, 0, 255} : (SDL_Color){180, 40, 40, 255};
        char statusCopy[300];
        strcpy(statusCopy, statusMsg);
        char* line = strtok(statusCopy, "\n");
        int sy = 518;
        while (line) {
            renderText(renderer, fontSmall, line, 55, sy, statusColor);
            sy += 20;
            line = strtok(NULL, "\n");
        }
        
        // Graph
        drawGraph(renderer, fontSmall, a_val, b_val, root, hasResult);
        
        // ---- RIGHT PANEL: Results ----
        drawPanel(renderer, 740, 95, 735, 830, (SDL_Color){255, 252, 250, 255}, 
                  (SDL_Color){180, 80, 80, 255}, 2);
        
        renderTextBold(renderer, fontLarge, "ITERATION TABLE", 1000, 110, sectionColor);
        
        if (iterationCount > 0) {
            renderText(renderer, fontSmall, "(Scroll with mouse wheel to see all iterations)", 900, 135, hintColor);
            
            // Table header
            int tableX = 755;
            int tableY = 165;
            
            drawPanel(renderer, tableX, tableY, 705, 30, (SDL_Color){200, 70, 70, 255}, 
                      (SDL_Color){150, 40, 40, 255}, 1);
            
            SDL_Color headerColor = {255, 255, 255, 255};
            renderTextBold(renderer, fontSmall, "n", tableX + 10, tableY + 8, headerColor);
            renderTextBold(renderer, fontSmall, "x(n-1)", tableX + 50, tableY + 8, headerColor);
            renderTextBold(renderer, fontSmall, "x(n)", tableX + 165, tableY + 8, headerColor);
            renderTextBold(renderer, fontSmall, "f(x(n-1))", tableX + 280, tableY + 8, headerColor);
            renderTextBold(renderer, fontSmall, "f(x(n))", tableX + 400, tableY + 8, headerColor);
            renderTextBold(renderer, fontSmall, "x(n+1)", tableX + 515, tableY + 8, headerColor);
            renderTextBold(renderer, fontSmall, "Error", tableX + 630, tableY + 8, headerColor);
            
            // Table rows
            int rowY = tableY + 35;
            int maxRows = 32;
            int startIdx = scrollOffset / 22;
            
            for (int i = startIdx; i < iterationCount && i < startIdx + maxRows; i++) {
                SDL_Color rowBg = (i % 2 == 0) ? (SDL_Color){255, 250, 250, 255} : (SDL_Color){255, 245, 245, 255};
                SDL_SetRenderDrawColor(renderer, rowBg.r, rowBg.g, rowBg.b, rowBg.a);
                SDL_Rect rowRect = {tableX, rowY, 705, 22};
                SDL_RenderFillRect(renderer, &rowRect);
                
                SDL_Color textColor = {80, 30, 30, 255};
                char buf[50];
                
                sprintf(buf, "%d", iterations[i].n);
                renderText(renderer, fontTiny, buf, tableX + 12, rowY + 5, textColor);
                
                sprintf(buf, "%.6f", iterations[i].x_prev);
                renderText(renderer, fontTiny, buf, tableX + 45, rowY + 5, textColor);
                
                sprintf(buf, "%.6f", iterations[i].x_curr);
                renderText(renderer, fontTiny, buf, tableX + 160, rowY + 5, textColor);
                
                sprintf(buf, "%.6f", iterations[i].f_prev);
                renderText(renderer, fontTiny, buf, tableX + 275, rowY + 5, textColor);
                
                sprintf(buf, "%.6f", iterations[i].f_curr);
                renderText(renderer, fontTiny, buf, tableX + 395, rowY + 5, textColor);
                
                sprintf(buf, "%.6f", iterations[i].x_next);
                renderText(renderer, fontTiny, buf, tableX + 510, rowY + 5, textColor);
                
                sprintf(buf, "%.8f", iterations[i].error);
                renderText(renderer, fontTiny, buf, tableX + 615, rowY + 5, textColor);
                
                rowY += 22;
            }
            
            // Final result box
            if (hasResult) {
                int resultY = 880;
                drawPanel(renderer, 755, resultY, 705, 35, (SDL_Color){230, 255, 230, 255}, 
                          (SDL_Color){100, 180, 100, 255}, 2);
                
                char resultText[200];
                sprintf(resultText, "FINAL ROOT:  x = %.3f     |     f(x) = %.2e     |     Iterations: %d", 
                        root, function(root, a_val, b_val), iterationCount);
                renderTextBold(renderer, font, resultText, 770, resultY + 9, (SDL_Color){0, 100, 0, 255});
            }
        } else {
            renderText(renderer, fontMedium, "No iterations yet.", 1020, 400, hintColor);
            renderText(renderer, font, "Enter values and press COMPUTE to see results.", 920, 440, hintColor);
        }
        
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    
    TTF_CloseFont(fontTitle);
    TTF_CloseFont(fontLarge);
    TTF_CloseFont(fontMedium);
    TTF_CloseFont(font);
    TTF_CloseFont(fontSmall);
    TTF_CloseFont(fontTiny);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    
    return 0;
}
