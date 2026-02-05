#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define MAX_ITER 50
#define TOLERANCE 0.01
#define WINDOW_WIDTH 1400
#define WINDOW_HEIGHT 800

// Iteration data structure
typedef struct {
    double xn;
    double xn1;
    double error;
} IterationRow;

// UI Element structure
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

// Fixed point iteration methods
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

double f(double x, double a, double b, double c) {
    return a * x * x + b * x + c;
}

void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) return;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &rect);
    
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void renderInputBox(SDL_Renderer* renderer, TTF_Font* font, InputBox* box) {
    // Box background
    if (box->active) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    }
    SDL_RenderFillRect(renderer, &box->rect);
    
    // Box border
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawRect(renderer, &box->rect);
    
    // Label
    SDL_Color labelColor = {50, 50, 50, 255};
    renderText(renderer, font, box->label, box->rect.x - 80, box->rect.y + 5, labelColor);
    
    // Value
    SDL_Color textColor = {0, 0, 0, 255};
    if (strlen(box->value) > 0) {
        renderText(renderer, font, box->value, box->rect.x + 5, box->rect.y + 5, textColor);
    }
}

void renderButton(SDL_Renderer* renderer, TTF_Font* font, Button* btn) {
    // Button background
    if (btn->clicked) {
        SDL_SetRenderDrawColor(renderer, 60, 120, 60, 255);
    } else if (btn->hovered) {
        SDL_SetRenderDrawColor(renderer, 80, 160, 80, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 70, 140, 70, 255);
    }
    SDL_RenderFillRect(renderer, &btn->rect);
    
    // Button border
    SDL_SetRenderDrawColor(renderer, 40, 90, 40, 255);
    SDL_RenderDrawRect(renderer, &btn->rect);
    
    // Button text (centered)
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

void drawGraph(SDL_Renderer* renderer, double a, double b, double c, double root, int hasRoot) {
    int graphX = 930;
    int graphY = 150;
    int graphW = 400;
    int graphH = 300;
    int scale = 10;
    int centerX = graphX + graphW / 2;
    int centerY = graphY + graphH / 2;
    
    // Graph background
    SDL_SetRenderDrawColor(renderer, 30, 35, 45, 255);
    SDL_Rect graphRect = {graphX, graphY, graphW, graphH};
    SDL_RenderFillRect(renderer, &graphRect);
    
    // Grid lines
    SDL_SetRenderDrawColor(renderer, 50, 60, 70, 255);
    for (int i = graphX; i <= graphX + graphW; i += scale) {
        SDL_RenderDrawLine(renderer, i, graphY, i, graphY + graphH);
    }
    for (int i = graphY; i <= graphY + graphH; i += scale) {
        SDL_RenderDrawLine(renderer, graphX, i, graphX + graphW, i);
    }
    
    // Main axes
    SDL_SetRenderDrawColor(renderer, 200, 200, 210, 255);
    SDL_RenderDrawLine(renderer, centerX, graphY, centerX, graphY + graphH);
    SDL_RenderDrawLine(renderer, graphX, centerY, graphX + graphW, centerY);
    
    // Draw parabola
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
    
    // Draw root marker if exists
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
    
    TTF_Font* font = TTF_OpenFont("font.ttf", 18);
    TTF_Font* fontSmall = TTF_OpenFont("font.ttf", 14);
    TTF_Font* fontMedium = TTF_OpenFont("font.ttf", 16);
    TTF_Font* fontTitle = TTF_OpenFont("font.ttf", 24);
    
    if (!font || !fontSmall || !fontMedium || !fontTitle) {
        printf("Error loading font: %s\n", TTF_GetError());
        return 1;
    }
    
    // Input boxes
    InputBox inputs[5];
    const char* labels[] = {"a:", "b:", "c:", "x0:", "Method:"};
    for (int i = 0; i < 5; i++) {
        inputs[i].rect = (SDL_Rect){140, 220 + i * 60, 150, 35};
        strcpy(inputs[i].label, labels[i]);
        strcpy(inputs[i].value, "");
        inputs[i].active = 0;
    }
    strcpy(inputs[4].value, "1"); // Default method
    
    // Compute button
    Button computeBtn = {{140, 540, 150, 45}, "COMPUTE", 0, 0};
    
    // Clear button
    Button clearBtn = {{140, 600, 150, 45}, "CLEAR", 0, 0};
    
    // Result variables
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
    
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
            
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                int mx = e.button.x;
                int my = e.button.y;
                
                // Check input boxes
                activeInput = -1;
                for (int i = 0; i < 5; i++) {
                    if (mx >= inputs[i].rect.x && mx <= inputs[i].rect.x + inputs[i].rect.w &&
                        my >= inputs[i].rect.y && my <= inputs[i].rect.y + inputs[i].rect.h) {
                        activeInput = i;
                    }
                    inputs[i].active = (i == activeInput);
                }
                
                // Check compute button
                if (mx >= computeBtn.rect.x && mx <= computeBtn.rect.x + computeBtn.rect.w &&
                    my >= computeBtn.rect.y && my <= computeBtn.rect.y + computeBtn.rect.h) {
                    computeBtn.clicked = 1;
                    
                    // Parse inputs
                    coefA = atof(inputs[0].value);
                    coefB = atof(inputs[1].value);
                    coefC = atof(inputs[2].value);
                    double x0 = atof(inputs[3].value);
                    int method = atoi(inputs[4].value);
                    
                    if (method < 1 || method > 5) {
                        sprintf(resultText, "Error: Method must be 1-5");
                        hasValidRoot = 0;
                    } else {
                        // Run Fixed Point Iteration
                        double x_current = x0;
                        int iter = 0;
                        int diverged = 0;
                        totalIterations = 0;
                        
                        for (iter = 0; iter < MAX_ITER; iter++) {
                            double x_next = g(x_current, coefA, coefB, coefC, method);
                            double error = fabs(x_next - x_current);
                            
                            // Store iteration data
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
                
                // Check clear button
                if (mx >= clearBtn.rect.x && mx <= clearBtn.rect.x + clearBtn.rect.w &&
                    my >= clearBtn.rect.y && my <= clearBtn.rect.y + clearBtn.rect.h) {
                    for (int i = 0; i < 4; i++) {
                        strcpy(inputs[i].value, "");
                    }
                    strcpy(inputs[4].value, "1");
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
            }
            
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
            
            if (e.type == SDL_MOUSEWHEEL) {
                // Scroll the iteration table
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
        
        // Rendering
        SDL_SetRenderDrawColor(renderer, 240, 240, 245, 255);
        SDL_RenderClear(renderer);
        
        // Header Information
        SDL_Color headerColor = {20, 20, 60, 255};
        renderText(renderer, fontMedium, "MT211 - Numerical Method", 50, 15, headerColor);
        renderText(renderer, fontSmall, "Semestral Project", 50, 40, headerColor);
        
        SDL_Color submittedColor = {60, 60, 80, 255};
        renderText(renderer, fontSmall, "Submitted By:", 50, 70, submittedColor);
        renderText(renderer, fontSmall, "BSCPE 22001", 50, 90, submittedColor);
        renderText(renderer, fontSmall, "Emmanuel Jr Porsona", 50, 110, submittedColor);
        renderText(renderer, fontSmall, "Amit Jeed", 50, 130, submittedColor);
        
        // Title
        SDL_Color titleColor = {40, 40, 100, 255};
        renderText(renderer, fontTitle, "FIXED POINT ITERATION SOLVER", 550, 10, titleColor);
        
        // Subtitle
        SDL_Color subtitleColor = {80, 80, 80, 255};
        renderText(renderer, fontSmall, "Equation: ax^2 + bx + c = 0", 650, 50, subtitleColor);
        
        // Input section
        SDL_Color sectionColor = {60, 60, 60, 255};
        renderText(renderer, font, "INPUT", 120, 180, sectionColor);
        
        for (int i = 0; i < 5; i++) {
            renderInputBox(renderer, font, &inputs[i]);
        }
        
        // Method info
        renderText(renderer, fontSmall, "Methods: 1-5", 140, 500, subtitleColor);
        
        // Buttons
        renderButton(renderer, font, &computeBtn);
        renderButton(renderer, font, &clearBtn);
        
        // Status section
        renderText(renderer, font, "STATUS", 70, 670, sectionColor);
        
        if (strlen(resultText) > 0) {
            char resultCopy[500];
            strcpy(resultCopy, resultText);
            char* line = strtok(resultCopy, "\n");
            int y = 700;
            while (line) {
                SDL_Color resultColor = hasValidRoot ? (SDL_Color){20, 120, 20, 255} : (SDL_Color){180, 20, 20, 255};
                renderText(renderer, fontSmall, line, 80, y, resultColor);
                y += 20;
                line = strtok(NULL, "\n");
            }
        }
        
        // Iteration Table
        if (totalIterations > 0) {
            renderText(renderer, font, "ITERATION TABLE", 390, 110, sectionColor);
            
            // Table header
            SDL_SetRenderDrawColor(renderer, 60, 80, 100, 255);
            SDL_Rect tableHeader = {390, 145, 460, 30};
            SDL_RenderFillRect(renderer, &tableHeader);
            
            SDL_Color headerColor = {255, 255, 255, 255};
            renderText(renderer, fontSmall, "n", 410, 150, headerColor);
            renderText(renderer, fontSmall, "x_n", 470, 150, headerColor);
            renderText(renderer, fontSmall, "x_(n+1)", 600, 150, headerColor);
            renderText(renderer, fontSmall, "error", 760, 150, headerColor);
            
            // Calculate visible rows
            int maxVisibleRows = 13;
            int startRow = tableScrollOffset;
            int endRow = startRow + maxVisibleRows;
            if (endRow > totalIterations) endRow = totalIterations;
            
            // Table rows (scrollable)
            for (int i = startRow; i < endRow; i++) {
                int displayIndex = i - startRow;
                int y = 180 + displayIndex * 25;
                
                // Alternating row colors
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
            
            // Draw scrollbar if needed
            if (totalIterations > maxVisibleRows) {
                int scrollbarX = 895;
                int scrollbarY = 180;
                int scrollbarHeight = maxVisibleRows * 25;
                
                // Scrollbar track
                SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
                SDL_Rect scrollbarTrack = {scrollbarX, scrollbarY, 10, scrollbarHeight};
                SDL_RenderFillRect(renderer, &scrollbarTrack);
                
                // Scrollbar thumb
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
        
        // Conclusion section
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
            
            // Bold text effect by rendering multiple times with offset
            sprintf(buffer, "Equation: %.1lfx^2 + (%.1lf)x + (%.1lf) = 0", coefA, coefB, coefC);
            renderText(renderer, font, buffer, 440, conclusionY + 45, conclusionColor);
            renderText(renderer, font, buffer, 441, conclusionY + 45, conclusionColor);
            
            sprintf(buffer, "Approximate Root: x = %.6lf", finalRoot);
            renderText(renderer, font, buffer, 440, conclusionY + 70, conclusionColor);
            renderText(renderer, font, buffer, 441, conclusionY + 70, conclusionColor);
            
            sprintf(buffer, "Total Iterations: %d   |   Tolerance: %.2lf", totalIterations, TOLERANCE);
            renderText(renderer, font, buffer, 440, conclusionY + 95, conclusionColor);
            renderText(renderer, font, buffer, 441, conclusionY + 95, conclusionColor);
        }
        
        // Graph section
        renderText(renderer, font, "GRAPH", 950, 110, sectionColor);
        drawGraph(renderer, coefA, coefB, coefC, finalRoot, hasValidRoot);
        
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    
    TTF_CloseFont(font);
    TTF_CloseFont(fontSmall);
    TTF_CloseFont(fontMedium);
    TTF_CloseFont(fontTitle);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    
    return 0;
}
