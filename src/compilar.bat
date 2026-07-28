@echo off
g++ src/main.cpp -I C:\raylib\w64devkit\include -L C:\raylib\w64devkit\lib -lraylib -lopengl32 -lgdi32 -lwinmm -o juego.exe
if %errorlevel%==0 (
    echo.
    echo ===== COMPILACION EXITOSA =====
) else (
    echo.
    echo ===== ERROR AL COMPILAR =====
)
pause