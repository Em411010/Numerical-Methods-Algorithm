#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define MAX_ITER 50
#define TOLERANCE 0.0001
#define WINDOW_WIDTH 1400
#define WINDOW_HEIGHT 800

// Data structure for storing iteration results
typedef struct {
    int iteration;
    double x0;
    double x1;
    double x2;
    double fx0;
    double fx1;
    double fx2;
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

// Original exponential function f(x) = e^x - ax - b
double f(double x, double a, double b) {
    return exp(x) - a * x - b;
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
        SDL_SetRenderDrawColor(renderer, 230, 240, 255, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 240, 248, 255, 255);
    }
    SDL_RenderFillRect(renderer, &box->rect);
    
    SDL_SetRenderDrawColor(renderer, 70, 130, 180, 255);
    SDL_RenderDrawRect(renderer, &box->rect);
    
    SDL_Color labelColor = {30, 60, 120, 255};
    renderText(renderer, font, box->label, box->rect.x - 80, box->rect.y + 5, labelColor);
    
    SDL_Color textColor = {0, 0, 139, 255};
    if (strlen(box->value) > 0) {
        renderText(renderer, font, box->value, box->rect.x + 5, box->rect.y + 5, textColor);
    }
}

// Render button with hover and click effects
void renderButton(SDL_Renderer* renderer, TTF_Font* font, Button* btn) {
    if (btn->clicked) {
        SDL_SetRenderDrawColor(renderer, 30, 90, 180, 255);
    } else if (btn->hovered) {
        SDL_SetRenderDrawColor(renderer, 65, 105, 225, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 70, 130, 180, 255);
    }
    SDL_RenderFillRect(renderer, &btn->rect);
    
    SDL_SetRenderDrawColor(renderer, 25, 80, 160, 255);
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

// Draw exponential curve with axes, grid, and root marker
void drawGraph(SDL_Renderer* renderer, double a, double b, double root, int hasRoot) {
    int graphX = 980;
    int graphY = 220;
    int graphW = 400;
    int graphH = 440;
    int scale = 50;
    int centerX = graphX + graphW / 2;
    int centerY = graphY + graphH / 2;
    
    SDL_SetRenderDrawColor(renderer, 240, 248, 255, 255);
    SDL_Rect graphRect = {graphX, graphY, graphW, graphH};
    SDL_RenderFillRect(renderer, &graphRect);
    
    SDL_SetRenderDrawColor(renderer, 200, 220, 240, 255);
    for (int i = graphX; i <= graphX + graphW; i += 50) {
        SDL_RenderDrawLine(renderer, i, graphY, i, graphY + graphH);
    }
    for (int i = graphY; i <= graphY + graphH; i += 50) {
        SDL_RenderDrawLine(renderer, graphX, i, graphX + graphW, i);
    }
    
    SDL_SetRenderDrawColor(renderer, 70, 130, 180, 255);
    SDL_RenderDrawLine(renderer, centerX, graphY, centerX, graphY + graphH);
    SDL_RenderDrawLine(renderer, graphX, centerY, graphX + graphW, centerY);
    
    SDL_SetRenderDrawColor(renderer, 65, 105, 225, 255);
    for (int px = graphX; px < graphX + graphW; px++) {
        double x = (px - centerX) / (double)scale;
        double y = f(x, a, b);
        int py = centerY - (int)(y * 20);
        
        if (py >= graphY && py < graphY + graphH && fabs(y) < 50) {
            SDL_RenderDrawPoint(renderer, px, py);
            SDL_RenderDrawPoint(renderer, px, py + 1);
        }
    }
    
    if (hasRoot) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
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
    
    SDL_Window* window = SDL_CreateWindow("False Position Method - Exponential",
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
    const char* labels[] = {"a:", "b:", "x0:", "x1:"};
    for (int i = 0; i < 4; i++) {
        inputs[i].rect = (SDL_Rect){140, 230 + i * 60, 150, 35};
        strcpy(inputs[i].label, labels[i]);
        strcpy(inputs[i].value, "");
        inputs[i].active = 0;
    }
    
    Button computeBtn = {{50, 490, 120, 40}, "COMPUTE", 0, 0};
    Button clearBtn = {{190, 490, 120, 40}, "CLEAR", 0, 0};
    
    // State variables
    char resultText[500] = "Enter coefficients and initial guesses (x0 and x1)";
    double finalRoot = 0;
    int hasValidRoot = 0;
    double coefA = 0, coefB = 0;
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
                
                if (mx >= computeBtn.rect.x && mx <= computeBtn.rect.x + computeBtn.rect.w &&
                    my >= computeBtn.rect.y && my <= computeBtn.rect.y + computeBtn.rect.h) {
                    computeBtn.clicked = 1;
                    
                    // Parse input values
                    coefA = atof(inputs[0].value);
                    coefB = atof(inputs[1].value);
                    double x0 = atof(inputs[2].value);
                    double x1 = atof(inputs[3].value);
                    
                    double fx0 = f(x0, coefA, coefB);
                    double fx1 = f(x1, coefA, coefB);
                    
                    // Check bracketing condition
                    if (fx0 * fx1 >= 0) {
                        sprintf(resultText, "ERROR: f(x0) and f(x1) must have opposite signs!\nf(%.2f)=%.4f, f(%.2f)=%.4f", 
                                x0, fx0, x1, fx1);
                        hasValidRoot = 0;
                        totalIterations = 0;
                    } else {
                        totalIterations = 0;
                        hasValidRoot = 0;
                        
                        // False Position Algorithm
                        for (int iter = 0; iter < MAX_ITER; iter++) {
                            double x2 = x1 - fx1 * (x1 - x0) / (fx1 - fx0);
                            double fx2 = f(x2, coefA, coefB);
                            double error = fabs(fx2);
                            
                            iterations[iter].iteration = iter + 1;
                            iterations[iter].x0 = x0;
                            iterations[iter].x1 = x1;
                            iterations[iter].x2 = x2;
                            iterations[iter].fx0 = fx0;
                            iterations[iter].fx1 = fx1;
                            iterations[iter].fx2 = fx2;
                            iterations[iter].error = error;
                            totalIterations++;
                            
                            if (error < TOLERANCE) {
                                hasValidRoot = 1;
                                finalRoot = x2;
                                sprintf(resultText, "SUCCESS!\nRoot: x = %.6f\nIterations: %d", x2, iter + 1);
                                break;
                            }
                            
                            // Update interval
                            if (fx0 * fx2 < 0) {
                                x1 = x2;
                                fx1 = fx2;
                            } else {
                                x0 = x2;
                                fx0 = fx2;
                            }
                        }
                        
                        if (!hasValidRoot) {
                            sprintf(resultText, "FAILED: Did not converge\nTry different initial guesses");
                        }
                    }
                }
                
                // Clear button: Reset all inputs and state
                if (mx >= clearBtn.rect.x && mx <= clearBtn.rect.x + clearBtn.rect.w &&
                    my >= clearBtn.rect.y && my <= clearBtn.rect.y + clearBtn.rect.h) {
                    for (int i = 0; i < 4; i++) {
                        strcpy(inputs[i].value, "");
                    }
                    strcpy(resultText, "Enter coefficients and initial guesses (x0 and x1)");
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
            }
            
            // Handle text input for active input box
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
                    
                    int maxVisibleRows = 10;
                    int maxScroll = totalIterations - maxVisibleRows;
                    if (maxScroll < 0) maxScroll = 0;
                    if (tableScrollOffset > maxScroll) tableScrollOffset = maxScroll;
                }
            }
        }
        
        // Clear screen
        SDL_SetRenderDrawColor(renderer, 230, 240, 250, 255);
        SDL_RenderClear(renderer);
        
        // Render header information (centered at top)
        SDL_Color headerColor = {0, 51, 153, 255};
        renderText(renderer, fontTitle, "MT211 - Numerical Method", 500, 15, headerColor);
        renderText(renderer, fontLarge, "Semestral Project", 560, 45, headerColor);
        
        SDL_Color submittedColor = {25, 80, 160, 255};
        renderText(renderer, fontLarge, "Submitted By:", 575, 75, submittedColor);
        renderText(renderer, fontLarge, "BSCPE 22005", 575, 100, submittedColor);
        renderText(renderer, fontLarge, "Khurt Goyena", 575, 125, submittedColor);
    
        
        SDL_Color titleColor = {0, 51, 153, 255};
        renderText(renderer, fontTitle, "FALSE POSITION METHOD", 20, 40, titleColor);
        
        SDL_Color subtitleColor = {25, 80, 160, 255};
        renderText(renderer, fontLarge, "Exponential Equation: eˣ - ax - b = 0", 30, 75, subtitleColor);
        
        // Render input section
        SDL_Color sectionColor = {0, 51, 153, 255};
        renderText(renderer, font, "INPUT:", 55, 180, sectionColor);
        
        for (int i = 0; i < 4; i++) {
            renderInputBox(renderer, font, &inputs[i]);
        }
        
        renderButton(renderer, font, &computeBtn);
        renderButton(renderer, font, &clearBtn);
        
        renderText(renderer, font, "STATUS", 70, 600, sectionColor);
        
        if (strlen(resultText) > 0) {
            char resultCopy[500];
            strcpy(resultCopy, resultText);
            char* line = strtok(resultCopy, "\n");
            int y = 625;
            while (line) {
                SDL_Color resultColor = hasValidRoot ? (SDL_Color){0, 128, 0, 255} : (SDL_Color){178, 34, 34, 255};
                renderText(renderer, fontSmall, line, 80, y, resultColor);
                y += 20;
                line = strtok(NULL, "\n");
            }
        }
        
        // Render iteration table
        if (totalIterations > 0) {
            renderText(renderer, font, "ITERATION TABLE", 575, 200, sectionColor);
            
            SDL_SetRenderDrawColor(renderer, 70, 130, 180, 255);
            SDL_Rect tableHeader = {350, 230, 600, 30};
            SDL_RenderFillRect(renderer, &tableHeader);
            
            SDL_Color headerColor2 = {255, 255, 255, 255};
            renderText(renderer, fontSmall, "n", 360, 235, headerColor2);
            renderText(renderer, fontSmall, "x0", 410, 235, headerColor2);
            renderText(renderer, fontSmall, "x1", 510, 235, headerColor2);
            renderText(renderer, fontSmall, "x2", 610, 235, headerColor2);
            renderText(renderer, fontSmall, "f(x2)", 710, 235, headerColor2);
            renderText(renderer, fontSmall, "Error", 830, 235, headerColor2);
            
            int maxVisibleRows = 10;
            int startRow = tableScrollOffset;
            int endRow = startRow + maxVisibleRows;
            if (endRow > totalIterations) endRow = totalIterations;
            
            for (int i = startRow; i < endRow; i++) {
                int displayIndex = i - startRow;
                int y = 265 + displayIndex * 25;
                
                if (i % 2 == 0) {
                    SDL_SetRenderDrawColor(renderer, 240, 248, 255, 255);
                } else {
                    SDL_SetRenderDrawColor(renderer, 230, 240, 250, 255);
                }
                SDL_Rect row = {350, y, 600, 25};
                SDL_RenderFillRect(renderer, &row);
                
                SDL_Color textColor = {20, 20, 60, 255};
                char buffer[50];
                
                sprintf(buffer, "%d", iterations[i].iteration);
                renderText(renderer, fontSmall, buffer, 360, y + 3, textColor);
                
                sprintf(buffer, "%.3lf", iterations[i].x0);
                renderText(renderer, fontSmall, buffer, 410, y + 3, textColor);
                
                sprintf(buffer, "%.3lf", iterations[i].x1);
                renderText(renderer, fontSmall, buffer, 510, y + 3, textColor);
                
                sprintf(buffer, "%.3lf", iterations[i].x2);
                renderText(renderer, fontSmall, buffer, 610, y + 3, textColor);
                
                sprintf(buffer, "%.3lf", iterations[i].fx2);
                renderText(renderer, fontSmall, buffer, 710, y + 3, textColor);
                
                sprintf(buffer, "%.3lf", iterations[i].error);
                renderText(renderer, fontSmall, buffer, 830, y + 3, textColor);
            }
            
            if (totalIterations > maxVisibleRows) {
                int scrollbarX = 960;
                int scrollbarY = 265;
                int scrollbarHeight = maxVisibleRows * 25;
                
                SDL_SetRenderDrawColor(renderer, 200, 220, 240, 255);
                SDL_Rect scrollbarTrack = {scrollbarX, scrollbarY, 10, scrollbarHeight};
                SDL_RenderFillRect(renderer, &scrollbarTrack);
                
                float thumbRatio = (float)maxVisibleRows / totalIterations;
                int thumbHeight = (int)(scrollbarHeight * thumbRatio);
                if (thumbHeight < 20) thumbHeight = 20;
                
                float scrollRatio = (float)tableScrollOffset / (totalIterations - maxVisibleRows);
                int thumbY = scrollbarY + (int)((scrollbarHeight - thumbHeight) * scrollRatio);
                
                SDL_SetRenderDrawColor(renderer, 70, 130, 180, 255);
                SDL_Rect scrollbarThumb = {scrollbarX, thumbY, 10, thumbHeight};
                SDL_RenderFillRect(renderer, &scrollbarThumb);
            }
        }
        
        // Render conclusion box with final results
        if (hasValidRoot) {
            int conclusionY = 550;
            renderText(renderer, font, "CONCLUSION", 350, conclusionY, sectionColor);
            
            SDL_SetRenderDrawColor(renderer, 240, 248, 255, 255);
            SDL_Rect conclusionBox = {350, conclusionY + 30, 600, 90};
            SDL_RenderFillRect(renderer, &conclusionBox);
            
            SDL_SetRenderDrawColor(renderer, 70, 130, 180, 255);
            SDL_RenderDrawRect(renderer, &conclusionBox);
            
            SDL_Color conclusionColor = {0, 51, 153, 255};
            char buffer[200];
            
            formatEquation(buffer, (int)coefA, (int)coefB);
            renderText(renderer, font, buffer, 360, conclusionY + 40, conclusionColor);
            renderText(renderer, font, buffer, 361 , conclusionY + 40, conclusionColor);
            
            sprintf(buffer, "Approximate Root: x = %.6lf", finalRoot);
            renderText(renderer, font, buffer, 360, conclusionY + 65, conclusionColor);
            renderText(renderer, font, buffer, 361, conclusionY + 65, conclusionColor);
            
            sprintf(buffer, "Total Iterations: %d   |   Tolerance: %.4lf", totalIterations, TOLERANCE);
            renderText(renderer, font, buffer, 360, conclusionY + 90, conclusionColor);
            renderText(renderer, font, buffer, 361, conclusionY + 90, conclusionColor);
        }
        
        renderText(renderer, font, "GRAPH", 970, 180, sectionColor);
        drawGraph(renderer, coefA, coefB, finalRoot, hasValidRoot);
        
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
