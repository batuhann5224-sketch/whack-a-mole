#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600
#define MOLE_COUNT    3   // 3 farklı köstebek resmi

// =====================================================
// ADIM 2: Gradient Arkaplan Çizim Fonksiyonu
// =====================================================
void drawGradientBackground(SDL_Renderer* renderer) {
    SDL_Color topColor    = {135, 206, 235, 255}; // Gökyüzü Mavisi
    SDL_Color bottomColor = {34,  139, 34,  255}; // Çimen Yeşili

    for (int y = 0; y < WINDOW_HEIGHT; y++) {
        float ratio = (float)y / WINDOW_HEIGHT;
        Uint8 r = (Uint8)(topColor.r + (bottomColor.r - topColor.r) * ratio);
        Uint8 g = (Uint8)(topColor.g + (bottomColor.g - topColor.g) * ratio);
        Uint8 b = (Uint8)(topColor.b + (bottomColor.b - topColor.b) * ratio);
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderDrawLine(renderer, 0, y, WINDOW_WIDTH, y);
    }
}

int main(int argc, char* argv[]) {
    srand((unsigned int)time(NULL)); // Rastgelelik için tohumla

    // 1. SDL ve SDL_image başlat
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL baslatilamadi! Hata: %s\n", SDL_GetError());
        return 1;
    }

    // PNG desteği için SDL_image başlat
    if (IMG_Init(IMG_INIT_PNG) == 0) {
        printf("SDL_image baslatilamadi! Hata: %s\n", IMG_GetError());
        SDL_Quit();
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
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 3. Renderer oluştur
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (renderer == NULL) {
        printf("Renderer olusturulamadi! Hata: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // =====================================================
    // ADIM 3: 4 Köstebek Resmini Yükle
    // =====================================================
    SDL_Texture* moleTextures[MOLE_COUNT];
    char moleFiles[MOLE_COUNT][20] = {
        "mole2.png",
        "mole3.png",
        "mole4.png"
    };

    for (int i = 0; i < MOLE_COUNT; i++) {
        // Önce Surface olarak yükle (ColorKey uygulayabilmek için)
        SDL_Surface* surface = IMG_Load(moleFiles[i]);
        if (surface == NULL) {
            printf("Resim yuklenemedi: %s | Hata: %s\n", moleFiles[i], IMG_GetError());
            moleTextures[i] = NULL;
            continue;
        }

        // Beyaz rengi (255, 255, 255) şeffaf (transparan) yap
        // Böylece arkaplanı olan resimlerde beyaz bölgeler görünmez
        Uint32 whiteColor = SDL_MapRGB(surface->format, 255, 255, 255);
        SDL_SetColorKey(surface, SDL_TRUE, whiteColor);

        // Surface'i Texture'a dönüştür ve belleği temizle
        moleTextures[i] = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);

        if (moleTextures[i] == NULL) {
            printf("Texture olusturulamadi: %s | Hata: %s\n", moleFiles[i], SDL_GetError());
        }
    }

    // Başlangıçta rastgele bir köstebek seç
    int currentMole = rand() % MOLE_COUNT;

    // Köstebeğin ekrandaki maksimum boyutu (oran korunarak ölçeklenecek)
    int MAX_MOLE_SIZE = 150;

    bool isRunning = true;
    bool isFullscreen = false;
    SDL_Event event;

    // 4. Ana Oyun Döngüsü
    while (isRunning) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                isRunning = false;
            }
            else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_F11) {
                    isFullscreen = !isFullscreen;
                    SDL_SetWindowFullscreen(window,
                        isFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                }
                else if (event.key.keysym.sym == SDLK_ESCAPE) {
                    isRunning = false;
                }
                // SPACE tuşuna basınca farklı bir köstebek göster (test için)
                else if (event.key.keysym.sym == SDLK_SPACE) {
                    currentMole = rand() % MOLE_COUNT;
                }
            }
        }

        // 5. Çizim
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Gradient arkaplan
        drawGradientBackground(renderer);

        // ADIM 3: Köstebeği çiz (orijinal oranı koruyarak)
        if (moleTextures[currentMole] != NULL) {
            // Resmin orijinal genişlik ve yüksekliğini al
            int origW, origH;
            SDL_QueryTexture(moleTextures[currentMole], NULL, NULL, &origW, &origH);

            // En büyük kenarı MAX_MOLE_SIZE yapacak şekilde oranı koru
            int drawW, drawH;
            if (origW > origH) {
                drawW = MAX_MOLE_SIZE;
                drawH = (int)(origH * ((float)MAX_MOLE_SIZE / origW));
            } else {
                drawH = MAX_MOLE_SIZE;
                drawW = (int)(origW * ((float)MAX_MOLE_SIZE / origH));
            }

            // Ekranın ortasına hizala
            SDL_Rect moleRect = {
                (WINDOW_WIDTH  - drawW) / 2,
                (WINDOW_HEIGHT - drawH) / 2,
                drawW,
                drawH
            };

            SDL_RenderCopy(renderer, moleTextures[currentMole], NULL, &moleRect);
        }

        SDL_RenderPresent(renderer);
    }

    // 6. Temizlik
    for (int i = 0; i < MOLE_COUNT; i++) {
        if (moleTextures[i] != NULL) {
            SDL_DestroyTexture(moleTextures[i]);
        }
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}
