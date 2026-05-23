@echo off
echo Kod derleniyor...
gcc main.c -o whackamole.exe -lmingw32 -lSDL2main -lSDL2
if %ERRORLEVEL% EQU 0 (
    echo Derleme basarili! Oyun baslatiliyor...
    whackamole.exe
) else (
    echo Derleme sirasinda hata olustu!
)
pause
