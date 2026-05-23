#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

int main(int argc, char* argv[]) {
    // 1. SDL'i başlat
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL baslatilamadi! Hata: %s\n", SDL_GetError());
        return 1;
    }

    // 2. Pencere oluştur
    SDL_Window* window = SDL_CreateWindow(
        "Whack-a-Mole",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (window == NULL) {
        printf("Pencere olusturulamadi! Hata: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // 3. Renderer (Ressam) oluştur
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (renderer == NULL) {
        printf("Renderer olusturulamadi! Hata: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool isRunning = true;
    bool isFullscreen = false;
    SDL_Event event;

    // 4. Ana Oyun Döngüsü
    while (isRunning) {
        // Event (Olay) Yakalama
        while (SDL_PollEvent(&event)) {
            // Sağ üstteki çarpıya basıldığında
            if (event.type == SDL_QUIT) {
                isRunning = false;
            }
            // Klavyeden bir tuşa basıldığında
            else if (event.type == SDL_KEYDOWN) {
                // F11 tuşuna basılırsa tam ekran geçişi yap
                if (event.key.keysym.sym == SDLK_F11) {
                    isFullscreen = !isFullscreen;
                    if (isFullscreen) {
                        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    } else {
                        SDL_SetWindowFullscreen(window, 0); // Pencere moduna dön
                    }
                }
                // ESC tuşuna basılırsa oyundan çık (ekstra kolaylık)
                else if (event.key.keysym.sym == SDLK_ESCAPE) {
                    isRunning = false;
                }
            }
        }

        // 5. Çizim İşlemleri (Render)
        // Arka plan rengini siyah olarak ayarla (R=0, G=0, B=0, Alpha=255)
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        
        // Ekranı belirlediğimiz renkle temizle
        SDL_RenderClear(renderer);

        // Arka planda çizilen her şeyi ekrana yansıt (Göster)
        SDL_RenderPresent(renderer);
    }

    // 6. Temizlik (Hafıza sızıntısını önlemek için her şeyi yok ediyoruz)
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
