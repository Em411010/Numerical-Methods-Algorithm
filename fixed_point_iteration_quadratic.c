#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define MAX_ITER 50
#define TOLERANCE 0.01
#define WINDOW_WIDTH 1400
#define WINDOW_HEIGHT 800

// Data structure for storing iteration results
typedef struct {
    double xn;
    double xn1;
    double error;
} IterationRow;

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

typedef struct {
    SDL_Rect rect;
    char formula[80];
    int selected;
    int hovered;
} MethodOption;

// Transform f(x) = ax² + bx + c into g(x) for different methods
double g(double x, double a, double b, double c, int method) {
    switch(method) {
        case 1: return -(a * x * x + c) / b;
        case 2: 
            if (fabs(a * x + b) < 1e-10) return NAN;
            return -c / (a * x + b);
        case 3: 
            if (a == 0 || (-b * x - c) / a < 0) return NAN;
            return sqrt((-b * x - c) / a);
        case 4: 
            if (a == 0 || (-b * x - c) / a < 0) return NAN;
            return -sqrt((-b * x - c) / a);
        case 5: 
            if (b == 0) return NAN;
            return (x * x - c / a) / (-b / a);
        default: return NAN;
    }
}

// Original quadratic function f(x) = ax² + bx + c
double f(double x, double a, double b, double c) {
    return a * x * x + b * x + c;
}

// Format equation with proper signs and superscript notation
void formatEquation(char* buffer, int a, int b, int c) {
    char part1[50], part2[50], part3[50];
    
    if (a == 1) {
        strcpy(part1, "x²");
    } else if (a == -1) {
        strcpy(part1, "-x²");
    } else {
        sprintf(part1, "%dx²", a);
    }
    
    if (b == 0) {
        strcpy(part2, "");
    } else if (b == 1) {
        strcpy(part2, " + x");
    } else if (b == -1) {
        strcpy(part2, " - x");
    } else if (b > 0) {
        sprintf(part2, " + %dx", b);
    } else {
        sprintf(part2, " - %dx", -b);
    }
    
    if (c == 0) {
        strcpy(part3, "");
    } else if (c > 0) {
        sprintf(part3, " + %d", c);
    } else {
        sprintf(part3, " - %d", -c);
    }
    
    sprintf(buffer, "Equation: %s%s%s = 0", part1, part2, part3);
}

// Render text with UTF-8 support for Unicode characters
void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &rect);
    
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

// Render input box with label and value
void renderInputBox(SDL_Renderer* renderer, TTF_Font* font, InputBox* box) {
    if (box->active) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    }
    SDL_RenderFillRect(renderer, &box->rect);
    
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawRect(renderer, &box->rect);
    
    SDL_Color labelColor = {50, 50, 50, 255};
    renderText(renderer, font, box->label, box->rect.x - 80, box->rect.y + 5, labelColor);
    
    SDL_Color textColor = {0, 0, 0, 255};
    if (strlen(box->value) > 0) {
        renderText(renderer, font, box->value, box->rect.x + 5, box->rect.y + 5, textColor);
    }
}

// Render button with hover and click effects
void renderButton(SDL_Renderer* renderer, TTF_Font* font, Button* btn) {
    if (btn->clicked) {
        SDL_SetRenderDrawColor(renderer, 60, 120, 60, 255);
    } else if (btn->hovered) {
        SDL_SetRenderDrawColor(renderer, 80, 160, 80, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 70, 140, 70, 255);
    }
    SDL_RenderFillRect(renderer, &btn->rect);
    
    SDL_SetRenderDrawColor(renderer, 40, 90, 40, 255);
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

// Draw parabola graph with axes, grid, and root marker
void drawGraph(SDL_Renderer* renderer, double a, double b, double c, double root, int hasRoot) {
    int graphX = 930;
    int graphY = 150;
    int graphW = 400;
    int graphH = 300;
    int scale = 10;
    int centerX = graphX + graphW / 2;
    int centerY = graphY + graphH / 2;
    
    SDL_SetRenderDrawColor(renderer, 30, 35, 45, 255);
    SDL_Rect graphRect = {graphX, graphY, graphW, graphH};
    SDL_RenderFillRect(renderer, &graphRect);
    
    SDL_SetRenderDrawColor(renderer, 50, 60, 70, 255);
    for (int i = graphX; i <= graphX + graphW; i += scale) {
        SDL_RenderDrawLine(renderer, i, graphY, i, graphY + graphH);
    }
    for (int i = graphY; i <= graphY + graphH; i += scale) {
        SDL_RenderDrawLine(renderer, graphX, i, graphX + graphW, i);
    }
    
    SDL_SetRenderDrawColor(renderer, 200, 200, 210, 255);
    SDL_RenderDrawLine(renderer, centerX, graphY, centerX, graphY + graphH);
    SDL_RenderDrawLine(renderer, graphX, centerY, graphX + graphW, centerY);
    
    SDL_SetRenderDrawColor(renderer, 100, 255, 100, 255);
    for (int px = graphX; px < graphX + graphW; px++) {
        double x = (px - centerX) / (double)scale;
        double y = f(x, a, b, c);
        int py = centerY - (int)(y * scale);
        
        if (py >= graphY && py < graphY + graphH) {
            SDL_RenderDrawPoint(renderer, px, py);
            SDL_RenderDrawPoint(renderer, px, py + 1);
        }
    }
    
    if (hasRoot) {
        SDL_SetRenderDrawColor(renderer, 255, 80, 80, 255);
        int root_x = centerX + (int)(root * scale);
        for (int i = -8; i <= 8; i++) {
            for (int j = -8; j <= 8; j++) {
                if (i*i + j*j <= 64) {
                    SDL_RenderDrawPoint(renderer, root_x + i, centerY + j);
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    
    SDL_Window* window = SDL_CreateWindow("Fixed Point Iteration - GUI",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    
    // Load fonts
    TTF_Font* font = TTF_OpenFont("font.ttf", 18);
    TTF_Font* fontSmall = TTF_OpenFont("font.ttf", 14);
    TTF_Font* fontMedium = TTF_OpenFont("font.ttf", 16);
    TTF_Font* fontLarge = TTF_OpenFont("font.ttf", 20);
    TTF_Font* fontTitle = TTF_OpenFont("font.ttf", 24);
    
    if (!font || !fontSmall || !fontMedium || !fontLarge || !fontTitle) {
        printf("Error loading font: %s\n", TTF_GetError());
        return 1;
    }
    
    // Initialize input boxes
    InputBox inputs[4];
    const char* labels[] = {"a:", "b:", "c:", "x0:"};
    for (int i = 0; i < 4; i++) {
        inputs[i].rect = (SDL_Rect){140, 240 + i * 60, 150, 35};
        strcpy(inputs[i].label, labels[i]);
        strcpy(inputs[i].value, "");
        inputs[i].active = 0;
    }
    
    // Initialize method selection options
    int selectedMethod = 1;
    MethodOption methods[5];
    const char* formulas[] = {
        "g(x) = -(ax² + c) / b",
        "g(x) = -c / (ax + b)",
        "g(x) = √((-bx - c) / a)",
        "g(x) = -√((-bx - c) / a)",
        "g(x) = (x² - c/a) / (-b/a)"
    };
    for (int i = 0; i < 5; i++) {
        methods[i].rect = (SDL_Rect){50, 500 + i * 28, 260, 26};
        strcpy(methods[i].formula, formulas[i]);
        methods[i].selected = (i == 0);
        methods[i].hovered = 0;
    }
    
    Button computeBtn = {{50, 650, 120, 40}, "COMPUTE", 0, 0};
    Button clearBtn = {{190, 650, 120, 40}, "CLEAR", 0, 0};
    
    // State variables
    char resultText[500] = "";
    double finalRoot = 0;
    int hasValidRoot = 0;
    double coefA = 0, coefB = 0, coefC = 0;
    IterationRow iterations[MAX_ITER];
    int totalIterations = 0;
    
    int activeInput = -1;
    int quit = 0;
    int tableScrollOffset = 0;
    SDL_Event e;
    
    // Main event loop
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
            
            // Handle mouse clicks
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
                
                for (int i = 0; i < 5; i++) {
                    if (mx >= methods[i].rect.x && mx <= methods[i].rect.x + methods[i].rect.w &&
                        my >= methods[i].rect.y && my <= methods[i].rect.y + methods[i].rect.h) {
                        selectedMethod = i + 1;
                        for (int j = 0; j < 5; j++) {
                            methods[j].selected = (j == i);
                        }
                    }
                }
                
                if (mx >= computeBtn.rect.x && mx <= computeBtn.rect.x + computeBtn.rect.w &&
                    my >= computeBtn.rect.y && my <= computeBtn.rect.y + computeBtn.rect.h) {
                    computeBtn.clicked = 1;
                    
                    // Parse input values
                    coefA = atof(inputs[0].value);
                    coefB = atof(inputs[1].value);
                    coefC = atof(inputs[2].value);
                    double x0 = atof(inputs[3].value);
                    int method = selectedMethod;
                    
                    {
                        // Fixed Point Iteration Algorithm
                        double x_current = x0;
                        int iter = 0;
                        int diverged = 0;
                        totalIterations = 0;
                        
                        for (iter = 0; iter < MAX_ITER; iter++) {
                            double x_next = g(x_current, coefA, coefB, coefC, method);
                            double error = fabs(x_next - x_current);
                            
                            iterations[iter].xn = x_current;
                            iterations[iter].xn1 = x_next;
                            iterations[iter].error = error;
                            totalIterations++;
                            
                            if (isnan(x_next) || isinf(x_next) || fabs(x_next) > 1e10) {
                                diverged = 1;
                                break;
                            }
                            
                            x_current = x_next;
                            
                            if (error < TOLERANCE) {
                                break;
                            }
                        }
                        
                        // Validate result and format output
                        finalRoot = x_current;
                        double verification = fabs(f(finalRoot, coefA, coefB, coefC));
                        
                        if (diverged || verification > 0.1) {
                            sprintf(resultText, "FAILED: %s\nTry different method or x0",
                                    diverged ? "Diverged" : "Did not converge");
                            hasValidRoot = 0;
                        } else {
                            sprintf(resultText, "SUCCESS!\nRoot: x = %.4lf\nIterations: %d",
                                    finalRoot, totalIterations);
                            hasValidRoot = 1;
                        }
                    }
                }
                
                // Clear button: Reset all inputs and state
                if (mx >= clearBtn.rect.x && mx <= clearBtn.rect.x + clearBtn.rect.w &&
                    my >= clearBtn.rect.y && my <= clearBtn.rect.y + clearBtn.rect.h) {
                    for (int i = 0; i < 4; i++) {
                        strcpy(inputs[i].value, "");
                    }
                    selectedMethod = 1;
                    for (int i = 0; i < 5; i++) {
                        methods[i].selected = (i == 0);
                    }
                    strcpy(resultText, "");
                    hasValidRoot = 0;
                    totalIterations = 0;
                    tableScrollOffset = 0;
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
                
                for (int i = 0; i < 5; i++) {
                    methods[i].hovered = (mx >= methods[i].rect.x && mx <= methods[i].rect.x + methods[i].rect.w &&
                                         my >= methods[i].rect.y && my <= methods[i].rect.y + methods[i].rect.h);
                }
            }
            
            // Handle text input for active input box
            if (e.type == SDL_TEXTINPUT && activeInput >= 0) {
                // Allow numbers, decimal point, and minus sign
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
                    if (len > 0) {
                        inputs[activeInput].value[len - 1] = '\0';
                    }
                }
            }
            
            // Handle mouse wheel for table scrolling
            if (e.type == SDL_MOUSEWHEEL) {
                if (totalIterations > 0) {
                    tableScrollOffset -= e.wheel.y * 2;
                    if (tableScrollOffset < 0) tableScrollOffset = 0;
                    
                    int maxVisibleRows = 13; // Max rows that fit in the display area
                    int maxScroll = totalIterations - maxVisibleRows;
                    if (maxScroll < 0) maxScroll = 0;
                    if (tableScrollOffset > maxScroll) tableScrollOffset = maxScroll;
                }
            }
        }
        
        // Clear screen
        SDL_SetRenderDrawColor(renderer, 240, 240, 245, 255);
        SDL_RenderClear(renderer);
        
        // Render header information
        SDL_Color headerColor = {20, 20, 60, 255};
        renderText(renderer, fontTitle, "MT211 - Numerical Method", 50, 15, headerColor);
        renderText(renderer, fontLarge, "Semestral Project", 50, 45, headerColor);
        
        SDL_Color submittedColor = {60, 60, 80, 255};
        renderText(renderer, fontLarge, "Submitted By:", 50, 75, submittedColor);
        renderText(renderer, fontLarge, "BSCPE 22001", 50, 100, submittedColor);
        renderText(renderer, fontLarge, "Emmanuel Jr Porsona", 50, 125, submittedColor);
        renderText(renderer, fontLarge, "Amit Jeed", 50, 150, submittedColor);
        
        SDL_Color titleColor = {40, 40, 100, 255};
        renderText(renderer, fontTitle, "FIXED POINT ITERATION METHOD", 550, 10, titleColor);
        
        SDL_Color subtitleColor = {80, 80, 80, 255};
        renderText(renderer, fontLarge, "Quadratic Equation: ax² + bx + c = 0", 610, 50, subtitleColor);
        
        // Render input section
        SDL_Color sectionColor = {60, 60, 60, 255};
        renderText(renderer, font, "INPUT", 120, 200, sectionColor);
        
        for (int i = 0; i < 4; i++) {
            renderInputBox(renderer, font, &inputs[i]);
        }
        
        // Render method selection
        renderText(renderer, font, "SELECT g(x):", 50, 475, sectionColor);
        
        for (int i = 0; i < 5; i++) {
            if (methods[i].selected) {
                SDL_SetRenderDrawColor(renderer, 180, 220, 180, 255);
            } else if (methods[i].hovered) {
                SDL_SetRenderDrawColor(renderer, 230, 230, 250, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
            }
            SDL_RenderFillRect(renderer, &methods[i].rect);
            
            if (methods[i].selected) {
                SDL_SetRenderDrawColor(renderer, 60, 140, 60, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
            }
            SDL_RenderDrawRect(renderer, &methods[i].rect);
            
            SDL_Color methodColor = methods[i].selected ? (SDL_Color){20, 80, 20, 255} : (SDL_Color){60, 60, 60, 255};
            renderText(renderer, fontSmall, methods[i].formula, methods[i].rect.x + 5, methods[i].rect.y + 4, methodColor);
        }
        
        renderButton(renderer, font, &computeBtn);
        renderButton(renderer, font, &clearBtn);
        
        renderText(renderer, font, "STATUS", 70, 720, sectionColor);
        
        if (strlen(resultText) > 0) {
            char resultCopy[500];
            strcpy(resultCopy, resultText);
            char* line = strtok(resultCopy, "\n");
            int y = 745;
            while (line) {
                SDL_Color resultColor = hasValidRoot ? (SDL_Color){20, 120, 20, 255} : (SDL_Color){180, 20, 20, 255};
                renderText(renderer, fontSmall, line, 80, y, resultColor);
                y += 20;
                line = strtok(NULL, "\n");
            }
        }
        
        // Render iteration table
        if (totalIterations > 0) {
            renderText(renderer, font, "ITERATION TABLE", 390, 110, sectionColor);
            
            SDL_SetRenderDrawColor(renderer, 60, 80, 100, 255);
            SDL_Rect tableHeader = {390, 145, 460, 30};
            SDL_RenderFillRect(renderer, &tableHeader);
            
            SDL_Color headerColor = {255, 255, 255, 255};
            renderText(renderer, fontSmall, "n", 410, 150, headerColor);
            renderText(renderer, fontSmall, "x_n", 470, 150, headerColor);
            renderText(renderer, fontSmall, "x_(n+1)", 600, 150, headerColor);
            renderText(renderer, fontSmall, "error", 760, 150, headerColor);
            
            int maxVisibleRows = 13;
            int startRow = tableScrollOffset;
            int endRow = startRow + maxVisibleRows;
            if (endRow > totalIterations) endRow = totalIterations;
            
            for (int i = startRow; i < endRow; i++) {
                int displayIndex = i - startRow;
                int y = 180 + displayIndex * 25;
                
                if (i % 2 == 0) {
                    SDL_SetRenderDrawColor(renderer, 245, 245, 250, 255);
                } else {
                    SDL_SetRenderDrawColor(renderer, 235, 235, 245, 255);
                }
                SDL_Rect row = {390, y, 460, 25};
                SDL_RenderFillRect(renderer, &row);
                
                SDL_Color textColor = {20, 20, 20, 255};
                char buffer[50];
                
                sprintf(buffer, "%d", i + 1);
                renderText(renderer, fontSmall, buffer, 400, y + 3, textColor);
                
                sprintf(buffer, "%.4lf", iterations[i].xn);
                renderText(renderer, fontSmall, buffer, 470, y + 3, textColor);
                
                sprintf(buffer, "%.4lf", iterations[i].xn1);
                renderText(renderer, fontSmall, buffer, 600, y + 3, textColor);
                
                sprintf(buffer, "%.6lf", iterations[i].error);
                renderText(renderer, fontSmall, buffer, 740, y + 3, textColor);
            }
            
            if (totalIterations > maxVisibleRows) {
                int scrollbarX = 895;
                int scrollbarY = 180;
                int scrollbarHeight = maxVisibleRows * 25;
                
                SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
                SDL_Rect scrollbarTrack = {scrollbarX, scrollbarY, 10, scrollbarHeight};
                SDL_RenderFillRect(renderer, &scrollbarTrack);
                
                float thumbRatio = (float)maxVisibleRows / totalIterations;
                int thumbHeight = (int)(scrollbarHeight * thumbRatio);
                if (thumbHeight < 20) thumbHeight = 20;
                
                float scrollRatio = (float)tableScrollOffset / (totalIterations - maxVisibleRows);
                int thumbY = scrollbarY + (int)((scrollbarHeight - thumbHeight) * scrollRatio);
                
                SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
                SDL_Rect scrollbarThumb = {scrollbarX, thumbY, 10, thumbHeight};
                SDL_RenderFillRect(renderer, &scrollbarThumb);
            }
        }
        
        // Render conclusion box with final results
        if (hasValidRoot) {
            int conclusionY = 550;
            renderText(renderer, font, "CONCLUSION", 430, conclusionY, sectionColor);
            
            SDL_SetRenderDrawColor(renderer, 240, 255, 240, 255);
            SDL_Rect conclusionBox = {430, conclusionY + 35, 480, 110};
            SDL_RenderFillRect(renderer, &conclusionBox);
            
            SDL_SetRenderDrawColor(renderer, 100, 180, 100, 255);
            SDL_RenderDrawRect(renderer, &conclusionBox);
            
            SDL_Color conclusionColor = {10, 70, 10, 255};
            char buffer[200];
            
            formatEquation(buffer, (int)coefA, (int)coefB, (int)coefC);
            renderText(renderer, font, buffer, 440, conclusionY + 45, conclusionColor);
            renderText(renderer, font, buffer, 441, conclusionY + 45, conclusionColor);
            
            sprintf(buffer, "Approximate Root: x = %.6lf", finalRoot);
            renderText(renderer, font, buffer, 440, conclusionY + 70, conclusionColor);
            renderText(renderer, font, buffer, 441, conclusionY + 70, conclusionColor);
            
            sprintf(buffer, "Total Iterations: %d   |   Tolerance: %.2lf", totalIterations, TOLERANCE);
            renderText(renderer, font, buffer, 440, conclusionY + 95, conclusionColor);
            renderText(renderer, font, buffer, 441, conclusionY + 95, conclusionColor);
        }
        
        renderText(renderer, font, "GRAPH", 950, 110, sectionColor);
        drawGraph(renderer, coefA, coefB, coefC, finalRoot, hasValidRoot);
        
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    
    TTF_CloseFont(font);
    TTF_CloseFont(fontSmall);
    TTF_CloseFont(fontMedium);
    TTF_CloseFont(fontLarge);
    TTF_CloseFont(fontTitle);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    
    return 0;
}
