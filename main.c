#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

// =====================================================
// ADIM 2: Gradient (Renk Geçişi) Arkaplan Çizim Fonksiyonu
// Ekranı yukarıdan aşağıya gökyüzü mavisinden çimen yeşiline boyar
// =====================================================
void drawGradientBackground(SDL_Renderer* renderer) {
    // Üst renk: Gökyüzü Mavisi (R=135, G=206, B=235)
    SDL_Color topColor    = {135, 206, 235, 255};
    // Alt renk: Çimen Yeşili (R=34, G=139, B=34)
    SDL_Color bottomColor = {34,  139, 34,  255};

    // Ekranın en üstünden (y=0) en altına (y=WINDOW_HEIGHT) kadar
    // her satır için ayrı ayrı renk hesaplayıp yatay bir çizgi çiziyoruz
    for (int y = 0; y < WINDOW_HEIGHT; y++) {
        // ratio: 0.0 (en üst) → 1.0 (en alt) arası bir oran
        float ratio = (float)y / WINDOW_HEIGHT;

        // O satır için rengi interpolasyon (enterpolasyon) ile hesapla
        Uint8 r = (Uint8)(topColor.r + (bottomColor.r - topColor.r) * ratio);
        Uint8 g = (Uint8)(topColor.g + (bottomColor.g - topColor.g) * ratio);
        Uint8 b = (Uint8)(topColor.b + (bottomColor.b - topColor.b) * ratio);

        // Hesaplanan renkle o satırı boydan boya çiz
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderDrawLine(renderer, 0, y, WINDOW_WIDTH, y);
    }
}

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
        // Önce ekranı siyaha temizle
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // ADIM 2: Gradient arkaplanı çiz (siyah temizliğin hemen üstüne)
        drawGradientBackground(renderer);

        // Çizilen her şeyi ekrana yansıt (Göster)
        SDL_RenderPresent(renderer);
    }

    // 6. Temizlik (Hafıza sızıntısını önlemek için her şeyi yok ediyoruz)
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
