#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600
#define MOLE_COUNT    4

// =====================================================
// ADIM 4: Köstebek Animasyon Durumları (State Machine)
// =====================================================
typedef enum {
    MOLE_HIDDEN,   // Köstebek toprağın altında, görünmez
    MOLE_RISING,   // Köstebek yukarı çıkıyor
    MOLE_VISIBLE,  // Köstebek tamamen çıkmış, bekliyor
    MOLE_FALLING   // Köstebek aşağı giriyor
} MoleState;

// Oval/BFS fonksiyonları kaldırıldı (resimler artık doğrudan şeffaf PNG)


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

    // Mantıksal çözünürlük sabitle: F11 tam ekranda otomatik ölçeklenir
    SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH, WINDOW_HEIGHT);

    // =====================================================
    // ADIM 3: 4 Köstebek Resmini Doğrudan Yükle (Şeffaf PNG)
    // =====================================================
    SDL_Texture* moleTextures[MOLE_COUNT];
    char moleFiles[MOLE_COUNT][20] = {
        "mole1.png",
        "mole2.png",
        "mole3.png",
        "mole4.png"
    };

    for (int i = 0; i < MOLE_COUNT; i++) {
        SDL_Surface* surface = IMG_Load(moleFiles[i]);
        if (surface == NULL) {
            printf("Resim yuklenemedi: %s | Hata: %s\n", moleFiles[i], IMG_GetError());
            moleTextures[i] = NULL;
            continue;
        }

        moleTextures[i] = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);

        if (moleTextures[i] == NULL) {
            printf("Texture olusturulamadi: %s | Hata: %s\n", moleFiles[i], SDL_GetError());
        } else {
            SDL_SetTextureBlendMode(moleTextures[i], SDL_BLENDMODE_BLEND);
        }
    }

    // =====================================================
    // Bomba Resmini Doğrudan Yükle
    // =====================================================
    SDL_Texture* bombTexture = NULL;
    SDL_Surface* bombSurface = IMG_Load("bomb.png");
    if (bombSurface != NULL) {
        bombTexture = SDL_CreateTextureFromSurface(renderer, bombSurface);
        SDL_FreeSurface(bombSurface);
        if (bombTexture != NULL) {
            SDL_SetTextureBlendMode(bombTexture, SDL_BLENDMODE_BLEND);
        }
    } else {
        printf("Bomba resmi yuklenemedi: bomb.png | Hata: %s\n", IMG_GetError());
    }

    // =====================================================
    // Delik Resmini Doğrudan Yükle
    // =====================================================
    SDL_Texture* holeTexture = NULL;
    SDL_Surface* holeSurface = IMG_Load("hole.png");
    if (holeSurface != NULL) {
        holeTexture = SDL_CreateTextureFromSurface(renderer, holeSurface);
        SDL_FreeSurface(holeSurface);
        if (holeTexture != NULL) {
            SDL_SetTextureBlendMode(holeTexture, SDL_BLENDMODE_BLEND);
        }
    } else {
        printf("Delik resmi yuklenemedi: hole.png | Hata: %s\n", IMG_GetError());
    }

    // Oyun ve Skor Değişkenleri
    typedef enum {
        ITEM_MOLE,
        ITEM_BOMB
    } ItemType;

    ItemType currentItemType = ITEM_MOLE;
    int score = 0;
    SDL_SetWindowTitle(window, "Whack-a-Mole | Skor: 0");

    // Başlangıçta rastgele bir köstebek seç
    int currentMole = rand() % MOLE_COUNT;

    // =====================================================
    // DELİK GÖRSELİ VE KÖSTEBEK KONUMLANDIRMA PARAMETRELERİ
    // =====================================================
    int holeX  = WINDOW_WIDTH / 2;   // Merkez X
    int holeY  = 490;                 // Deliğin merkez Y'si

    // Hole görseli boyutları (ekranda gösterilecek)
    int holeDisplayW = 420;
    int holeDisplayH = 280;

    // Hole görselinin sol üst köşesi
    int holeRectX = holeX - holeDisplayW / 2;
    int holeRectY = holeY - holeDisplayH / 2;

    // Görselin doğal boyutunu al (sandwich kaynak rect için)
    int holeTexNatW = 0, holeTexNatH = 0;
    if (holeTexture != NULL) {
        SDL_QueryTexture(holeTexture, NULL, NULL, &holeTexNatW, &holeTexNatH);
    }

    int drawW  = 200;                  // Köstebek genişliği
    int drawH  = 220;                  // Köstebek yüksekliği

    // Clip çizgisi: delik görselinin orta noktası (yani delik ağzı)
    int clipY  = holeRectY + holeDisplayH / 2 + 10;

    // Tam göründüğünde duracağı tepe nokta Y koordinatı
    float moleTargetY = (float)(clipY - drawH);
    // Gizlendiğinde tamamen deliğin altına ineceği nokta Y koordinatı
    float moleHideY   = (float)(clipY + 20);
    
    float moleY       = moleHideY;
    float moleSpeed   = 250.0f; // Piksel/saniye

    MoleState moleState  = MOLE_HIDDEN;
    Uint32    stateTimer = SDL_GetTicks(); // Durum başlangıç zamanı
    Uint32    hiddenWait = 1000;           // Gizli kalma süresi (ms)
    Uint32    visibleWait= 1500;           // Görünür kalma süresi (ms)

    bool isRunning = true;
    bool isFullscreen = false;
    SDL_Event event;

    // 4. Ana Oyun Döngüsü
    while (isRunning) {
        // =====================================================
        // ADIM 4: Animasyon Guncelleme (State Machine)
        // =====================================================
        Uint32 now    = SDL_GetTicks();
        float  deltaT = 1.0f / 60.0f; // ~60 FPS varsayımıyla delta zaman

        if (moleState == MOLE_HIDDEN) {
            // Gizli bekleme süresi doldu mu?
            if (now - stateTimer >= hiddenWait) {
                // %25 ihtimalle bomba, %75 ihtimalle rastgele köstebek çıksın
                if (bombTexture != NULL && rand() % 100 < 25) {
                    currentItemType = ITEM_BOMB;
                } else {
                    currentItemType = ITEM_MOLE;
                    currentMole = rand() % MOLE_COUNT;
                }
                
                moleState   = MOLE_RISING;
                stateTimer  = now;
            }
        }
        else if (moleState == MOLE_RISING) {
            // Köstebeği yukarı doğru hareket ettir
            moleY -= moleSpeed * deltaT;
            if (moleY <= moleTargetY) {
                moleY      = moleTargetY;
                moleState  = MOLE_VISIBLE;
                stateTimer = now;
            }
        }
        else if (moleState == MOLE_VISIBLE) {
            // Görünür bekleme süresi doldu mu?
            if (now - stateTimer >= visibleWait) {
                moleState  = MOLE_FALLING;
                stateTimer = now;
            }
        }
        else if (moleState == MOLE_FALLING) {
            // Köstebeği aşağı doğru hareket ettir
            moleY += moleSpeed * deltaT;
            if (moleY >= moleHideY) {
                moleY      = moleHideY;
                moleState  = MOLE_HIDDEN;
                stateTimer = now;
            }
        }

        // Olaylar (Klavye ve Fare Tıklamaları)
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
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    int mouseX = event.button.x;
                    int mouseY = event.button.y;
                    
                    // Köstebek veya bomba çıkarken ya da görünürken tıklanabilir
                    if (moleState == MOLE_RISING || moleState == MOLE_VISIBLE) {
                        SDL_Rect itemRect = {
                            holeX - drawW / 2,
                            (int)moleY,
                            drawW,
                            drawH
                        };
                        
                        // Nesnenin üzerine tıklandı mı?
                        // Dikeyde sadece clipY (deliğin üst çizgisi) hizasına kadar vurulabilir (toprak altındakilere vurulamaz)
                        if (mouseX >= itemRect.x && mouseX <= itemRect.x + itemRect.w &&
                            mouseY >= itemRect.y && mouseY <= clipY) {
                            
                            if (currentItemType == ITEM_BOMB) {
                                score -= 50;
                                if (score < 0) score = 0;
                                
                                char title[150];
                                sprintf(title, "Whack-a-Mole | 💥 Bomba! -50  |  Skor: %d", score);
                                SDL_SetWindowTitle(window, title);
                            } else {
                                score += 10;
                                
                                char title[150];
                                sprintf(title, "Whack-a-Mole | ⭐ +10  |  Skor: %d", score);
                                SDL_SetWindowTitle(window, title);
                            }
                            
                            // Vurulduğu anda hemen geri kaçsın
                            moleState = MOLE_FALLING;
                            stateTimer = now;
                        }
                    }
                }
            }
        }

        // 5. Çizim
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Gradient arkaplan
        drawGradientBackground(renderer);

        // =====================================================
        // DELİK VE KÖSTEBEK ÇİZİMİ (PNG Texture Sandviç Tekniği)
        // =====================================================

        // 1. Deliğin ÜST yarısı (köstebeğin ARKASINDA kalacak)
        if (holeTexture != NULL && holeTexNatW > 0 && holeTexNatH > 0) {
            SDL_Rect srcTop = { 0, 0, holeTexNatW, holeTexNatH / 2 };
            SDL_Rect dstTop = { holeRectX, holeRectY, holeDisplayW, holeDisplayH / 2 };
            SDL_RenderCopy(renderer, holeTexture, &srcTop, &dstTop);
        }

        // 2. Köstebek veya Bomba (Clip rect ile delik ağzının altına taşması engellenir)
        if (moleState != MOLE_HIDDEN) {
            SDL_Texture* activeTexture = (currentItemType == ITEM_BOMB) ? bombTexture : moleTextures[currentMole];
            if (activeTexture != NULL) {
                SDL_Rect clipRect = { 0, 0, WINDOW_WIDTH, clipY };
                SDL_RenderSetClipRect(renderer, &clipRect);

                SDL_Rect moleRect = {
                    holeX - drawW / 2,
                    (int)moleY,
                    drawW,
                    drawH
                };
                SDL_RenderCopy(renderer, activeTexture, NULL, &moleRect);
                SDL_RenderSetClipRect(renderer, NULL);
            }
        }

        // 3. Deliğin ALT yarısı (köstebeğin ÖNÜNDE kalacak - ön rim kapatsın)
        if (holeTexture != NULL && holeTexNatW > 0 && holeTexNatH > 0) {
            SDL_Rect srcBot = { 0, holeTexNatH / 2, holeTexNatW, holeTexNatH / 2 };
            SDL_Rect dstBot = { holeRectX, holeRectY + holeDisplayH / 2, holeDisplayW, holeDisplayH / 2 };
            SDL_RenderCopy(renderer, holeTexture, &srcBot, &dstBot);
        }

        SDL_RenderPresent(renderer);
    }

    // 6. Temizlik
    for (int i = 0; i < MOLE_COUNT; i++) {
        if (moleTextures[i] != NULL) {
            SDL_DestroyTexture(moleTextures[i]);
        }
    }
    if (bombTexture != NULL) {
        SDL_DestroyTexture(bombTexture);
    }
    if (holeTexture != NULL) {
        SDL_DestroyTexture(holeTexture);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}
