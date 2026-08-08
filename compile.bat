@echo off
echo Nettoyage...
del MEMU1.exe 2>nul
del SRC\*.o 2>nul
del SRC\*.obj 2>nul

echo Compilation en cours (sources dans SRC\)...
g++ -std=c++17 -O2 -o MEMU1.exe ^
    SRC\main.cpp ^
    SRC\ACIA6551.cpp ^
    SRC\VIA6522.cpp ^
    SRC\Bus.cpp ^
    SRC\AudioEngine.cpp ^
    -static-libgcc -static-libstdc++ -static -lwinmm

if %ERRORLEVEL% == 0 (
    echo ================================================
    echo  COMPILATION REUSSIE !
    echo ================================================
    echo.
    echo Structure du projet :
    echo   MEMU1.exe          - l'emulateur
    echo   memo1_rom.bin      - la ROM interne (obligatoire)
    echo   SRC\               - le code source C++
    echo   HDD\               - vos programmes .bas (SAVE/LOAD)
    echo.
    echo Usage :
    echo   MEMU1.exe                (sans cartouche)
    echo   MEMU1.exe ma_rom.bin     (avec cartouche)
    echo   Glisser-deposer un .bin sur MEMU1.exe
    echo.
    echo Dans l'emulateur :
    echo   Ctrl+S = sauvegarder le programme BASIC courant dans HDD\
    echo   Ctrl+O = charger un programme .bas depuis HDD\
    echo   Le buzzer (PB7 du VIA) est reproduit via la carte son
    echo.
    echo Lancement...
    MEMU1.exe %1
) else (
    echo ERREUR DE COMPILATION.
    pause
)
