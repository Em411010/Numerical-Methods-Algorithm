#include <SDL.h>
#include <stdio.h>
#include <math.h>

#define MAX_ITER 50
#define TOLERANCE 0.01

typedef struct {
    double xn;
    double xn1;
    double error;
} IterationRow;

// Fixed point iteration - different rearrangement methods
double g(double x, double a, double b, double c, int method) {
    switch(method) {
        case 1: // x = -(ax^2 + c)/b
            return -(a * x * x + c) / b;
        case 2: // x = -c/(ax + b)
            if (fabs(a * x + b) < 1e-10) return NAN;
            return -c / (a * x + b);
        case 3: // x = sqrt((-bx - c)/a) - positive root
            if (a == 0 || (-b * x - c) / a < 0) return NAN;
            return sqrt((-b * x - c) / a);
        case 4: // x = -sqrt((-bx - c)/a) - negative root
            if (a == 0 || (-b * x - c) / a < 0) return NAN;
            return -sqrt((-b * x - c) / a);
        case 5: // x = (x^2 - c/a)/(-b/a)
            if (b == 0) return NAN;
            return (x * x - c / a) / (-b / a);
        default:
            return NAN;
    }
}

double f(double x, double a, double b, double c) {
    return a * x * x + b * x + c;
}

int main(int argc, char* argv[]) {
    double a, b, c, x0;
    int method;
    IterationRow iterations[MAX_ITER];
    int totalIter = 0;
    double finalRoot;
    int retry = 1;
    int retryMode = 0;  // 0=new equation, 1=new method only, 2=new x0 only
    
    while (retry) {
        system("cls");
        
        printf("========================================\n");
        printf("  FIXED POINT ITERATION - ROOT FINDER  \n");
        printf("========================================\n\n");
        
        // Input coefficients 
        if (retryMode == 0 || retryMode == 3) {
            printf("Solve: ax^2 + bx + c = 0\n\n");
            printf("Enter coefficient a: ");
            scanf("%lf", &a);
            printf("Enter coefficient b: ");
            scanf("%lf", &b);
            printf("Enter coefficient c: ");
            scanf("%lf", &c);
        } else {
            printf("Using equation: %.1lfx^2 + (%.1lf)x + (%.1lf) = 0\n\n", a, b, c);
        }
        
        // Input method (
        if (retryMode == 0 || retryMode == 1 || retryMode == 3) {
            printf("\nSelect rearrangement method:\n");
            printf("  1. x = -(ax^2 + c)/b       [Requires b != 0]\n");
            printf("  2. x = -c/(ax + b)          [Requires ax+b != 0]\n");
            printf("  3. x = sqrt((-bx-c)/a)      [Positive root, a != 0]\n");
            printf("  4. x = -sqrt((-bx-c)/a)     [Negative root, a != 0]\n");
            printf("  5. x = (x^2 - c/a)/(-b/a)   [Requires b != 0]\n");
            printf("Choice (1-5): ");
                scanf("%d", &method);
            
            if (method < 1 || method > 5) {
                printf("\nInvalid method! Please choose 1-5.\n");
                printf("Press any key to try again...\n");
                getchar(); getchar();
                retryMode = 1; 
                continue;
            }
            
            
            if ((method == 1 || method == 5) && b == 0) {
                printf("\nError: Method %d requires b != 0!\n", method);
                printf("Press any key to try again...\n");
                getchar(); getchar();
                retryMode = 1;  
                continue;
            }
            if ((method == 3 || method == 4) && a == 0) {
                printf("\nError: Method %d requires a != 0!\n", method);
                printf("Press any key to try again...\n");
                getchar(); getchar();
                retryMode = 1;  
                continue;
            }
        } else {
            printf("Using method: Arrangement #%d\n", method);
        }
        
        // Input x0 (skip if only changing method)
        if (retryMode == 0 || retryMode == 2 || retryMode == 3) {
            printf("\nEnter initial guess x0: ");
            scanf("%lf", &x0);
        } else {
            printf("Using initial guess: x0 = %.2lf\n", x0);
        }
        
        // Reset retry mode for next iteration
        retryMode = 0;
        
        printf("\n========================================\n");
        printf("       COMPUTING...\n");
        printf("========================================\n\n");
    
        double x_current = x0;
        totalIter = 0;
        int diverged = 0;
        
        do {
            double x_next = g(x_current, a, b, c, method);
            iterations[totalIter].xn = x_current;
            iterations[totalIter].xn1 = x_next;
            iterations[totalIter].error = fabs(x_next - x_current);
            
            // Check for divergence
            if (isnan(x_next) || isinf(x_next) || fabs(x_next) > 1e10) {
                printf("WARNING: Method is diverging!\n");
                printf("Try a different initial guess or method.\n\n");
                diverged = 1;
                break;
            }
            
            x_current = x_next;
            totalIter++;
            
            if (totalIter >= MAX_ITER) {
                printf("Max iterations reached!\n");
                break;
            }
        } while (fabs(iterations[totalIter-1].error) > TOLERANCE);
        
        finalRoot = x_current;
        
        // Check if we actually found a root
        double verification = fabs(f(finalRoot, a, b, c));
        int validRoot = (verification < 0.1);  
        
        if (diverged || !validRoot) {
            printf("\n========================================\n");
            printf("       RESULT: FAILED TO CONVERGE\n");
            printf("========================================\n\n");
            printf("Equation: %.1lfx^2 + (%.1lf)x + (%.1lf) = 0\n", a, b, c);
            printf("Method %d used with x0 = %.2lf\n\n", method, x0);
            
            if (diverged) {
                printf("STATUS: DIVERGED\n");
                printf("The iteration exploded or became undefined.\n\n");
            } else {
                printf("STATUS: DID NOT CONVERGE\n");
                printf("The method oscillated without finding a root.\n");
                printf("Final value x = %.2lf gives f(x) = %.4lf\n", finalRoot, f(finalRoot, a, b, c));
                printf("(Should be close to 0 for a valid root)\n\n");
            }
            
            printf("What would you like to do?\n");
            printf("  1. Try a different arrangement method\n");
            printf("  2. Try a different initial guess (x0)\n");
            printf("  3. Change coefficients (a, b, c)\n");
            printf("  4. Exit program\n");
            printf("Choice: ");
            
            int choice;
            scanf("%d", &choice);
            getchar(); 
            
            if (choice == 4) {
                return 0;
            } else if (choice == 1) {
                retryMode = 1;  
                continue;
            } else if (choice == 2) {
                retryMode = 2;  
                continue;
            } else if (choice == 3) {
                retryMode = 3;  
                continue;
            } else {
                printf("Invalid choice. Exiting...\n");
                return 0;
            }
        }
        
        // ===== SUCCESS PATH: Display results and graph =====
        
        // ===== DISPLAY ITERATION TABLE =====
        printf("\n========================================\n");
        printf("       ITERATION TABLE\n");
        printf("========================================\n");
        printf(" n  |    x_n     |   x_n+1    |   error\n");
        printf("----|------------|------------|-----------\n");
        
        int displayRows = (totalIter > 15) ? 15 : totalIter;
        for (int i = 0; i < displayRows; i++) {
            printf("%-3d | %10.4lf | %10.4lf | %.6lf\n", 
                   i+1, iterations[i].xn, iterations[i].xn1, iterations[i].error);
        }
        
        if (totalIter > 15) {
            printf("... (%d more iterations)\n", totalIter - 15);
        }
        
        printf("\n========================================\n");
        printf("       CONCLUSION\n");
        printf("========================================\n\n");
        printf("Equation: %.1lfx^2 + (%.1lf)x + (%.1lf) = 0\n\n", a, b, c);
        printf("Results:\n");
        printf("  Approximate Root: x = %.2lf\n", finalRoot);
        printf("  Verification: f(%.2lf) = %.4lf ✓\n", finalRoot, f(finalRoot, a, b, c));
        printf("  Total Iterations: %d\n", totalIter);
        printf("  Tolerance: %.2lf\n", TOLERANCE);
        printf("  Method: Arrangement #%d\n\n", method);
        
        printf("Press any key to view the graph...\n");
        getchar(); getchar();
        
        // GRAPH RENDERING 
        SDL_Init(SDL_INIT_VIDEO);
        
        char title[100];
        sprintf(title, "Graph: %.1lfx^2 + %.1lfx + %.1lf = 0 | Root: x = %.2lf", a, b, c, finalRoot);
        
        SDL_Window* window = SDL_CreateWindow(title,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            900, 650, SDL_WINDOW_SHOWN);
        
        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        
        // Graph parameters
        int scale = 20;  
        int centerX = 450;
        int centerY = 325;
        
        SDL_SetRenderDrawColor(renderer, 20, 25, 35, 255);
        SDL_RenderClear(renderer);
        
        // Draw grid lines
        SDL_SetRenderDrawColor(renderer, 50, 60, 70, 255);
        for (int i = 50; i <= 850; i += scale) {
            SDL_RenderDrawLine(renderer, i, 50, i, 600);
        }
        for (int i = 50; i <= 600; i += scale) {
            SDL_RenderDrawLine(renderer, 50, i, 850, i);
        }
        
        // Draw main axes (brighter)
        SDL_SetRenderDrawColor(renderer, 220, 220, 240, 255);
        SDL_RenderDrawLine(renderer, centerX, 50, centerX, 600);     // Y-axis
        SDL_RenderDrawLine(renderer, 50, centerY, 850, centerY);     // X-axis
        
        // Draw tick marks on X-axis
        SDL_SetRenderDrawColor(renderer, 180, 180, 200, 255);
        for (int tick = -10; tick <= 10; tick++) {
            if (tick == 0) continue;  // Skip origin
            int px = centerX + tick * scale;
            if (px >= 50 && px <= 850) {
                SDL_RenderDrawLine(renderer, px, centerY - 5, px, centerY + 5);
            }
        }
        
        // Draw tick marks on Y-axis
        for (int tick = -6; tick <= 6; tick++) {
            if (tick == 0) continue;  // Skip origin
            int py = centerY - tick * scale;
            if (py >= 50 && py <= 600) {
                SDL_RenderDrawLine(renderer, centerX - 5, py, centerX + 5, py);
            }
        }
        
        // Draw the parabola
        SDL_SetRenderDrawColor(renderer, 100, 255, 100, 255);
        for (int px = 50; px < 850; px++) {
            double x = (px - centerX) / (double)scale;
            double y = f(x, a, b, c);
            int py = centerY - (int)(y * scale);
            
            if (py >= 50 && py < 600) {
                for (int thick = -1; thick <= 1; thick++) {
                    SDL_RenderDrawPoint(renderer, px, py + thick);
                }
            }
        }
        
        // Draw root marker
        SDL_SetRenderDrawColor(renderer, 255, 80, 80, 255);
        int root_x = centerX + (int)(finalRoot * scale);
        for (int i = -10; i <= 10; i++) {
            for (int j = -10; j <= 10; j++) {
                if (i*i + j*j <= 100) {
                    SDL_RenderDrawPoint(renderer, root_x + i, centerY + j);
                }
            }
        }
        
        SDL_RenderPresent(renderer);
        
        SDL_Event e;
        int quit = 0;
        while (!quit) {
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) {
                    quit = 1;
                }
            }
            SDL_Delay(16);
        }
        
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        
        // Ask if user wants to try another equation
        printf("\n========================================\n");
        printf("       SOLVE ANOTHER EQUATION?\n");
        printf("========================================\n\n");
        printf("  1. Yes (Enter new equation)\n");
        printf("  2. No (Exit program)\n");
        printf("\nChoice: ");
        
        int again;
        scanf("%d", &again);
        getchar();
        
        if (again != 1) {
            retry = 0;
        }
        
    }  
    
    return 0;
}  