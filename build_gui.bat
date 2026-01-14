@echo off
echo Building GUI Application...
gcc gui_app.c -I"C:\SDL2\x86_64-w64-mingw32\include\SDL2" -L"C:\SDL2\x86_64-w64-mingw32\lib" -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -o gui_app.exe
if %errorlevel% equ 0 (
    echo.
    echo [SUCCESS] gui_app.exe compiled successfully!
    echo Run with: gui_app.exe
) else (
    echo.
    echo [FAILED] Compilation error!
)
pause
