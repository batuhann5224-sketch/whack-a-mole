#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#define WINDOW_WIDTH    800
#define WINDOW_HEIGHT   600
#define MOLE_TEX_COUNT  4
#define HOLE_COUNT      9   // 3x3 grid

// =====================================================
// ADIM 4: Köstebek Animasyon Durumları (State Machine)
// =====================================================
typedef enum { MOLE_HIDDEN, MOLE_RISING, MOLE_VISIBLE, MOLE_FALLING } MoleState;
typedef enum { ITEM_MOLE, ITEM_BOMB } ItemType;

// =====================================================
// ADIM 5: Her deliğin kendi durumunu tutan yapı
// =====================================================
typedef struct {
    int       cx, cy;          // Deliğin merkez koordinatı
    int       displayW, displayH; // Delik görselinin ekran boyutu
    int       drawW, drawH;    // Köstebek çizim boyutu
    int       clipY;           // Köstebeğin toprak altına gizlendiği y sınırı
    float     targetY;         // Köstebek tam çıktığında durduğu y
    float     hideY;           // Köstebek gizlendiğinde bulunduğu y
    float     moleY;           // Anlık köstebek y pozisyonu
    float     speed;           // Köstebek hareket hızı (piksel/saniye)
    MoleState state;
    Uint32    stateTimer;
    Uint32    hiddenWait;      // Gizli kalma süresi (ms)
    Uint32    visibleWait;     // Görünür kalma süresi (ms)
    int       currentMole;     // Hangi köstebek texture'u (0-3)
    ItemType  itemType;        // Köstebek mi, bomba mı?
} Hole;

// Deliği verilen merkez koordinat ve boyutlarla başlat
void initHole(Hole* h, int cx, int cy, int displayW, int displayH, int drawW, int drawH) {
    h->cx        = cx;
    h->cy        = cy;
    h->displayW  = displayW;
    h->displayH  = displayH;
    h->drawW     = drawW;
    h->drawH     = drawH;
    h->clipY     = cy + 10; // Delik ağzı sınırı (orta nokta + küçük offset)
    h->targetY   = (float)(h->clipY - drawH);
    h->hideY     = (float)(h->clipY + 20);
    h->moleY     = h->hideY;
    h->speed     = 280.0f;
    h->state     = MOLE_HIDDEN;
    h->stateTimer = SDL_GetTicks();
    h->hiddenWait = 500 + rand() % 2000; // Delikleri farklı zamanlarda başlat
    h->visibleWait = 1000 + rand() % 800;
    h->currentMole = rand() % MOLE_TEX_COUNT;
    h->itemType  = ITEM_MOLE;
}

// =====================================================
// ADIM 2: Gradient Arkaplan Çizim Fonksiyonu
// =====================================================
void drawGradientBackground(SDL_Renderer* renderer) {
    SDL_Color top    = {135, 206, 235, 255}; // Gökyüzü mavisi
    SDL_Color bottom = {34,  139,  34, 255}; // Çimen yeşili
    for (int y = 0; y < WINDOW_HEIGHT; y++) {
        float r = (float)y / WINDOW_HEIGHT;
        SDL_SetRenderDrawColor(renderer,
            (Uint8)(top.r + (bottom.r - top.r) * r),
            (Uint8)(top.g + (bottom.g - top.g) * r),
            (Uint8)(top.b + (bottom.b - top.b) * r), 255);
        SDL_RenderDrawLine(renderer, 0, y, WINDOW_WIDTH, y);
    }
}

// =====================================================
// Delik + Köstebek Çizimi (Sandviç Tekniği)
// Önce deliğin üst yarısı → köstebek → deliğin alt yarısı
// Böylece köstebek sanki gerçekten delikten çıkıyor gibi görünür
// =====================================================
void drawHole(SDL_Renderer* renderer, Hole* h,
              SDL_Texture* holeTex, int holeTexW, int holeTexH,
              SDL_Texture** moleTex, SDL_Texture* bombTex) {
    int rx = h->cx - h->displayW / 2;
    int ry = h->cy - h->displayH / 2;

    // 1. Deliğin ÜST yarısı (köstebeğin arkasında kalır)
    if (holeTex && holeTexW > 0) {
        SDL_Rect srcTop = { 0, 0, holeTexW, holeTexH / 2 };
        SDL_Rect dstTop = { rx, ry, h->displayW, h->displayH / 2 };
        SDL_RenderCopy(renderer, holeTex, &srcTop, &dstTop);
    }

    // 2. Köstebek veya Bomba (clipY'nin üstüne çıkamaz)
    if (h->state != MOLE_HIDDEN) {
        SDL_Texture* tex = (h->itemType == ITEM_BOMB) ? bombTex : moleTex[h->currentMole];
        if (tex) {
            SDL_Rect clip = { 0, 0, WINDOW_WIDTH, h->clipY };
            SDL_RenderSetClipRect(renderer, &clip);
            SDL_Rect dst = { h->cx - h->drawW / 2, (int)h->moleY, h->drawW, h->drawH };
            SDL_RenderCopy(renderer, tex, NULL, &dst);
            SDL_RenderSetClipRect(renderer, NULL);
        }
    }

    // 3. Deliğin ALT yarısı (köstebeğin önünde kalır - delik kenarı kapatır)
    if (holeTex && holeTexW > 0) {
        SDL_Rect srcBot = { 0, holeTexH / 2, holeTexW, holeTexH / 2 };
        SDL_Rect dstBot = { rx, ry + h->displayH / 2, h->displayW, h->displayH / 2 };
        SDL_RenderCopy(renderer, holeTex, &srcBot, &dstBot);
    }
}

// Resim yükleme yardımcı fonksiyonu
SDL_Texture* loadTexture(SDL_Renderer* renderer, const char* path) {
    SDL_Surface* surf = IMG_Load(path);
    if (!surf) {
        printf("Resim yuklenemedi: %s | %s\n", path, IMG_GetError());
        return NULL;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (tex) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    return tex;
}

int main(int argc, char* argv[]) {
    srand((unsigned int)time(NULL));

    // 1. SDL ve SDL_image başlat
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL baslatilamadi! %s\n", SDL_GetError());
        return 1;
    }
    if (IMG_Init(IMG_INIT_PNG) == 0) {
        printf("SDL_image baslatilamadi! %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    // 2. Pencere oluştur
    SDL_Window* window = SDL_CreateWindow(
        "Whack-a-Mole",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        printf("Pencere olusturulamadi! %s\n", SDL_GetError());
        IMG_Quit(); SDL_Quit(); return 1;
    }

    // 3. Renderer oluştur
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        printf("Renderer olusturulamadi! %s\n", SDL_GetError());
        SDL_DestroyWindow(window); IMG_Quit(); SDL_Quit(); return 1;
    }
    SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH, WINDOW_HEIGHT);

    // Tekstürleri yükle
    SDL_Texture* moleTex[MOLE_TEX_COUNT];
    char moleFiles[MOLE_TEX_COUNT][20] = {"mole1.png", "mole2.png", "mole3.png", "mole4.png"};
    for (int i = 0; i < MOLE_TEX_COUNT; i++)
        moleTex[i] = loadTexture(renderer, moleFiles[i]);

    SDL_Texture* bombTex = loadTexture(renderer, "bomb.png");
    SDL_Texture* holeTex = loadTexture(renderer, "hole.png");

    int holeTexW = 0, holeTexH = 0;
    if (holeTex) SDL_QueryTexture(holeTex, NULL, NULL, &holeTexW, &holeTexH);

    // =====================================================
    // ADIM 5: 9 Deliği 3x3 Grid Olarak Başlat
    // Sütunlar: x = 155, 400, 645
    // Satırlar:  y = 350, 455, 540
    // =====================================================
    // =====================================================
    // 3x3 grid: Ekrana tam ortalanmış
    // Yatay: 150, 400, 650 → sağ/sol kenar 55px
    // Dikey: 180, 300, 420 → grid merkezi = 300 = ekran ortası
    // =====================================================
    Hole holes[HOLE_COUNT];
    int cols[3] = {150, 400, 650};
    int rows[3] = {180, 300, 420};
    int holeIdx = 0;
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            initHole(&holes[holeIdx++], cols[col], rows[row], 190, 100, 105, 118);
        }
    }

    int score = 0;
    SDL_SetWindowTitle(window, "Whack-a-Mole | Skor: 0");

    // Aynı anda sadece 1 delikten köstebek çıkar
    int  activeHole      = -1;          // Şu an aktif delik (-1 = yok)
    Uint32 nextSpawnTimer = SDL_GetTicks();
    Uint32 nextSpawnDelay = 800;        // İlk köstebek 0.8s sonra çıksın

    bool isRunning    = true;
    bool isFullscreen = false;
    SDL_Event event;
    float deltaT = 1.0f / 60.0f;

    // 4. Ana Oyun Döngüsü
    while (isRunning) {
        Uint32 now = SDL_GetTicks();

        // =====================================================
        // ADIM 5: Tek Aktif Delik State Machine
        // =====================================================
        if (activeHole == -1) {
            // Hiç aktif delik yok — bekleme süresi doldu mu?
            if (now - nextSpawnTimer >= nextSpawnDelay) {
                activeHole = rand() % HOLE_COUNT; // Rastgele bir delik seç
                Hole* h = &holes[activeHole];
                // Köstebek mi bomba mı?
                if (bombTex && rand() % 100 < 25)
                    h->itemType = ITEM_BOMB;
                else {
                    h->itemType    = ITEM_MOLE;
                    h->currentMole = rand() % MOLE_TEX_COUNT;
                }
                h->moleY     = h->hideY;
                h->state     = MOLE_RISING;
                h->stateTimer = now;
                h->visibleWait = 1000 + rand() % 800;
            }
        } else {
            // Aktif deliğin animasyonunu güncelle
            Hole* h = &holes[activeHole];
            if (h->state == MOLE_RISING) {
                h->moleY -= h->speed * deltaT;
                if (h->moleY <= h->targetY) {
                    h->moleY     = h->targetY;
                    h->state     = MOLE_VISIBLE;
                    h->stateTimer = now;
                }
            } else if (h->state == MOLE_VISIBLE) {
                if (now - h->stateTimer >= h->visibleWait) {
                    h->state     = MOLE_FALLING;
                    h->stateTimer = now;
                }
            } else if (h->state == MOLE_FALLING) {
                h->moleY += h->speed * deltaT;
                if (h->moleY >= h->hideY) {
                    h->moleY     = h->hideY;
                    h->state     = MOLE_HIDDEN;
                    activeHole   = -1;             // Delik boşaldı
                    nextSpawnTimer = now;
                    nextSpawnDelay = 500 + rand() % 1000; // Sonraki köstebek için bekle
                }
            }
        }

        // Olaylar (Klavye ve Fare)
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                isRunning = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_F11) {
                    isFullscreen = !isFullscreen;
                    SDL_SetWindowFullscreen(window,
                        isFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                    isRunning = false;
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int mx = event.button.x;
                int my = event.button.y;
                // Sadece aktif deliği kontrol et
                if (activeHole != -1) {
                    Hole* h = &holes[activeHole];
                    if (h->state == MOLE_RISING || h->state == MOLE_VISIBLE) {
                        SDL_Rect r = { h->cx - h->drawW / 2, (int)h->moleY, h->drawW, h->drawH };
                        if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= h->clipY) {
                            char title[150];
                            if (h->itemType == ITEM_BOMB) {
                                score -= 50;
                                if (score < 0) score = 0;
                                sprintf(title, "Whack-a-Mole | 💥 Bomba! -50  |  Skor: %d", score);
                            } else {
                                score += 10;
                                sprintf(title, "Whack-a-Mole | ⭐ +10  |  Skor: %d", score);
                            }
                            SDL_SetWindowTitle(window, title);
                            // Vurulunca hemen geri kaçsın
                            h->state      = MOLE_FALLING;
                            h->stateTimer = now;
                        }
                    }
                }
            }
        }

        // 5. Çizim
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        drawGradientBackground(renderer);

        // Delikleri arka satırdan öne doğru çiz (doğal perspektif için)
        for (int i = 0; i < HOLE_COUNT; i++)
            drawHole(renderer, &holes[i], holeTex, holeTexW, holeTexH, moleTex, bombTex);

        SDL_RenderPresent(renderer);
    }

    // 6. Temizlik
    for (int i = 0; i < MOLE_TEX_COUNT; i++)
        if (moleTex[i]) SDL_DestroyTexture(moleTex[i]);
    if (bombTex) SDL_DestroyTexture(bombTex);
    if (holeTex) SDL_DestroyTexture(holeTex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
