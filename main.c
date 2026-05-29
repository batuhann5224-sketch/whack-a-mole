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
typedef enum { STATE_PLAYING, STATE_GAME_OVER } GameState;

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

// =====================================================
// ADIM 6: Retro Vector Font Çizim Sistemi
// =====================================================
void drawStrokeChar(SDL_Renderer* r, char c, int x, int y, int w, int h) {
    switch (c) {
        case 'A':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x, y, x, y + h);
            SDL_RenderDrawLine(r, x + w, y, x + w, y + h);
            SDL_RenderDrawLine(r, x, y + h / 2, x + w, y + h / 2);
            break;
        case 'B':
            SDL_RenderDrawLine(r, x, y, x + w * 4 / 5, y);
            SDL_RenderDrawLine(r, x, y, x, y + h);
            SDL_RenderDrawLine(r, x, y + h / 2, x + w * 4 / 5, y + h / 2);
            SDL_RenderDrawLine(r, x, y + h, x + w * 4 / 5, y + h);
            SDL_RenderDrawLine(r, x + w * 4 / 5, y, x + w, y + h / 4);
            SDL_RenderDrawLine(r, x + w, y + h / 4, x + w * 4 / 5, y + h / 2);
            SDL_RenderDrawLine(r, x + w * 4 / 5, y + h / 2, x + w, y + h * 3 / 4);
            SDL_RenderDrawLine(r, x + w, y + h * 3 / 4, x + w * 4 / 5, y + h);
            break;
        case 'C':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x, y, x, y + h);
            SDL_RenderDrawLine(r, x, y + h, x + w, y + h);
            break;
        case 'D':
            SDL_RenderDrawLine(r, x, y, x, y + h);
            SDL_RenderDrawLine(r, x, y, x + w * 2 / 3, y);
            SDL_RenderDrawLine(r, x, y + h, x + w * 2 / 3, y + h);
            SDL_RenderDrawLine(r, x + w * 2 / 3, y, x + w, y + h / 3);
            SDL_RenderDrawLine(r, x + w, y + h / 3, x + w, y + h * 2 / 3);
            SDL_RenderDrawLine(r, x + w, y + h * 2 / 3, x + w * 2 / 3, y + h);
            break;
        case 'E':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x, y, x, y + h);
            SDL_RenderDrawLine(r, x, y + h, x + w, y + h);
            SDL_RenderDrawLine(r, x, y + h / 2, x + w * 2 / 3, y + h / 2);
            break;
        case 'G':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x, y, x, y + h);
            SDL_RenderDrawLine(r, x, y + h, x + w, y + h);
            SDL_RenderDrawLine(r, x + w, y + h / 2, x + w, y + h);
            SDL_RenderDrawLine(r, x + w / 2, y + h / 2, x + w, y + h / 2);
            break;
        case 'H':
            SDL_RenderDrawLine(r, x, y, x, y + h);
            SDL_RenderDrawLine(r, x + w, y, x + w, y + h);
            SDL_RenderDrawLine(r, x, y + h / 2, x + w, y + h / 2);
            break;
        case 'I':
            SDL_RenderDrawLine(r, x + w / 2, y, x + w / 2, y + h);
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x, y + h, x + w, y + h);
            break;
        case 'K':
            SDL_RenderDrawLine(r, x, y, x, y + h);
            SDL_RenderDrawLine(r, x + w, y, x, y + h / 2);
            SDL_RenderDrawLine(r, x, y + h / 2, x + w, y + h);
            break;
        case 'L':
            SDL_RenderDrawLine(r, x, y, x, y + h);
            SDL_RenderDrawLine(r, x, y + h, x + w, y + h);
            break;
        case 'M':
            SDL_RenderDrawLine(r, x, y, x, y + h);
            SDL_RenderDrawLine(r, x + w, y, x + w, y + h);
            SDL_RenderDrawLine(r, x, y, x + w / 2, y + h / 2);
            SDL_RenderDrawLine(r, x + w, y, x + w / 2, y + h / 2);
            break;
        case 'N':
            SDL_RenderDrawLine(r, x, y, x, y + h);
            SDL_RenderDrawLine(r, x, y, x + w, y + h);
            SDL_RenderDrawLine(r, x + w, y, x + w, y + h);
            break;
        case 'O':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x, y, x, y + h);
            SDL_RenderDrawLine(r, x + w, y, x + w, y + h);
            SDL_RenderDrawLine(r, x, y + h, x + w, y + h);
            break;
        case 'P':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x, y, x, y + h);
            SDL_RenderDrawLine(r, x + w, y, x + w, y + h / 2);
            SDL_RenderDrawLine(r, x, y + h / 2, x + w, y + h / 2);
            break;
        case 'R':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x, y, x, y + h);
            SDL_RenderDrawLine(r, x + w, y, x + w, y + h / 2);
            SDL_RenderDrawLine(r, x, y + h / 2, x + w, y + h / 2);
            SDL_RenderDrawLine(r, x, y + h / 2, x + w, y + h);
            break;
        case 'S':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x, y, x, y + h / 2);
            SDL_RenderDrawLine(r, x, y + h / 2, x + w, y + h / 2);
            SDL_RenderDrawLine(r, x + w, y + h / 2, x + w, y + h);
            SDL_RenderDrawLine(r, x, y + h, x + w, y + h);
            break;
        case 'T':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x + w / 2, y, x + w / 2, y + h);
            break;
        case 'U':
            SDL_RenderDrawLine(r, x, y, x, y + h);
            SDL_RenderDrawLine(r, x + w, y, x + w, y + h);
            SDL_RenderDrawLine(r, x, y + h, x + w, y + h);
            break;
        case 'V':
            SDL_RenderDrawLine(r, x, y, x + w / 2, y + h);
            SDL_RenderDrawLine(r, x + w, y, x + w / 2, y + h);
            break;
        case 'Y':
            SDL_RenderDrawLine(r, x, y, x + w / 2, y + h / 2);
            SDL_RenderDrawLine(r, x + w, y, x + w / 2, y + h / 2);
            SDL_RenderDrawLine(r, x + w / 2, y + h / 2, x + w / 2, y + h);
            break;
        case 'Z':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x + w, y, x, y + h);
            SDL_RenderDrawLine(r, x, y + h, x + w, y + h);
            break;
        case '1':
            SDL_RenderDrawLine(r, x + w / 2, y, x + w / 2, y + h);
            SDL_RenderDrawLine(r, x + w / 4, y + h / 4, x + w / 2, y);
            break;
        case '2':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x + w, y, x + w, y + h / 2);
            SDL_RenderDrawLine(r, x + w, y + h / 2, x, y + h / 2);
            SDL_RenderDrawLine(r, x, y + h / 2, x, y + h);
            SDL_RenderDrawLine(r, x, y + h, x + w, y + h);
            break;
        case '3':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x + w, y, x + w, y + h);
            SDL_RenderDrawLine(r, x, y + h / 2, x + w, y + h / 2);
            SDL_RenderDrawLine(r, x, y + h, x + w, y + h);
            break;
        case '4':
            SDL_RenderDrawLine(r, x, y, x, y + h / 2);
            SDL_RenderDrawLine(r, x, y + h / 2, x + w, y + h / 2);
            SDL_RenderDrawLine(r, x + w, y, x + w, y + h);
            break;
        case '5':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x, y, x, y + h / 2);
            SDL_RenderDrawLine(r, x, y + h / 2, x + w, y + h / 2);
            SDL_RenderDrawLine(r, x + w, y + h / 2, x + w, y + h);
            SDL_RenderDrawLine(r, x, y + h, x + w, y + h);
            break;
        case '6':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x, y, x, y + h);
            SDL_RenderDrawLine(r, x, y + h / 2, x + w, y + h / 2);
            SDL_RenderDrawLine(r, x + w, y + h / 2, x + w, y + h);
            SDL_RenderDrawLine(r, x, y + h, x + w, y + h);
            break;
        case '7':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x + w, y, x + w, y + h);
            break;
        case '8':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x, y, x, y + h);
            SDL_RenderDrawLine(r, x + w, y, x + w, y + h);
            SDL_RenderDrawLine(r, x, y + h / 2, x + w, y + h / 2);
            SDL_RenderDrawLine(r, x, y + h, x + w, y + h);
            break;
        case '9':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x, y, x, y + h / 2);
            SDL_RenderDrawLine(r, x + w, y, x + w, y + h);
            SDL_RenderDrawLine(r, x, y + h / 2, x + w, y + h / 2);
            SDL_RenderDrawLine(r, x, y + h, x + w, y + h);
            break;
        case '0':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x, y, x, y + h);
            SDL_RenderDrawLine(r, x + w, y, x + w, y + h);
            SDL_RenderDrawLine(r, x, y + h, x + w, y + h);
            SDL_RenderDrawLine(r, x, y, x + w, y + h);
            break;
        case ':':
            SDL_RenderDrawPoint(r, x + w / 2, y + h / 3);
            SDL_RenderDrawPoint(r, x + w / 2, y + 2 * h / 3);
            break;
        case '!':
            SDL_RenderDrawLine(r, x + w / 2, y, x + w / 2, y + h * 2 / 3);
            SDL_RenderDrawPoint(r, x + w / 2, y + h);
            break;
        case '-':
            SDL_RenderDrawLine(r, x, y + h / 2, x + w, y + h / 2);
            break;
    }
}

void drawStrokeText(SDL_Renderer* r, const char* text, int x, int y, int charW, int charH, int spacing) {
    int curX = x;
    while (*text) {
        char c = *text;
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        drawStrokeChar(r, c, curX, y, charW, charH);
        curX += charW + spacing;
        text++;
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

    GameState gameState = STATE_PLAYING;
    int score = 0;
    SDL_SetWindowTitle(window, "Whack-a-Mole | Skor: 0 | Sure: 60");

    // Aynı anda sadece 1 delikten köstebek çıkar
    int  activeHole      = -1;          // Şu an aktif delik (-1 = yok)
    Uint32 nextSpawnTimer = SDL_GetTicks();
    Uint32 nextSpawnDelay = 800;        // İlk köstebek 0.8s sonra çıksın
    Uint32 gameStartTime  = SDL_GetTicks();
    Uint32 gameDuration   = 60000;      // 60 saniye (ms)
    int timeLeft          = 60;

    bool isRunning    = true;
    bool isFullscreen = false;
    SDL_Event event;
    float deltaT = 1.0f / 60.0f;

    // 4. Ana Oyun Döngüsü
    while (isRunning) {
        Uint32 now = SDL_GetTicks();

        // Zaman Yönetimi
        if (gameState == STATE_PLAYING) {
            int elapsed = (int)(now - gameStartTime);
            timeLeft = (int)((gameDuration - elapsed) / 1000);
            if (timeLeft <= 0) {
                timeLeft = 0;
                gameState = STATE_GAME_OVER;
                SDL_SetWindowTitle(window, "Whack-a-Mole | Oyun Bitti! Sure Doldu.");
            }
        }

        // =====================================================
        // ADIM 6: Tek Aktif Delik State Machine & Oyun Durumu
        // =====================================================
        if (gameState == STATE_PLAYING) {
            if (activeHole == -1) {
                // Hiç aktif delik yok — bekleme süresi doldu mu?
                if (now - nextSpawnTimer >= nextSpawnDelay) {
                    activeHole = rand() % HOLE_COUNT; // Rastgele bir delik seç
                    Hole* h = &holes[activeHole];
                    // Köstebek mi bomba mı? (%25 şans)
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
                } else if (event.key.keysym.sym == SDLK_r) {
                    // Oyun bittiğinde 'R' ile doğrudan yeniden başlatma
                    if (gameState == STATE_GAME_OVER) {
                        score = 0;
                        activeHole = -1;
                        for (int i = 0; i < HOLE_COUNT; i++) {
                            holes[i].state = MOLE_HIDDEN;
                            holes[i].moleY = holes[i].hideY;
                        }
                        gameStartTime = SDL_GetTicks();
                        timeLeft = 60;
                        gameState = STATE_PLAYING;
                        SDL_SetWindowTitle(window, "Whack-a-Mole | Skor: 0 | Sure: 60");
                    }
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int mx = event.button.x;
                int my = event.button.y;
                // Sadece oynarken ve aktif delik varken kontrol et
                if (gameState == STATE_PLAYING && activeHole != -1) {
                    Hole* h = &holes[activeHole];
                    if (h->state == MOLE_RISING || h->state == MOLE_VISIBLE) {
                        SDL_Rect r = { h->cx - h->drawW / 2, (int)h->moleY, h->drawW, h->drawH };
                        if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= h->clipY) {
                            char title[150];
                            if (h->itemType == ITEM_BOMB) {
                                // Bombaya vurulduğu an oyun biter kanka!
                                gameState = STATE_GAME_OVER;
                                SDL_SetWindowTitle(window, "Whack-a-Mole | 💥 Patladın! Oyun Bitti!");
                            } else {
                                score += 10;
                                sprintf(title, "Whack-a-Mole | ⭐ +10  |  Skor: %d  |  Sure: %d", score, timeLeft);
                                SDL_SetWindowTitle(window, title);
                                // Vurulunca hemen geri kaçsın
                                h->state      = MOLE_FALLING;
                                h->stateTimer = now;
                            }
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

        // HUD Çizimi (Ekran üstünde canlı Skor ve Süre)
        if (gameState == STATE_PLAYING) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // Beyaz
            char scoreStr[32];
            sprintf(scoreStr, "SKOR: %d", score);
            drawStrokeText(renderer, scoreStr, 20, 20, 10, 16, 4);

            char timeStr[32];
            sprintf(timeStr, "SURE: %d", timeLeft);
            drawStrokeText(renderer, timeStr, WINDOW_WIDTH - 150, 20, 10, 16, 4);
        }

        // Oyun Bitti Ekranı (Overlay)
        if (gameState == STATE_GAME_OVER) {
            // Yarı şeffaf siyah overlay
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200); // 200/255 şeffaflık
            SDL_Rect overlay = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
            SDL_RenderFillRect(renderer, &overlay);

            // "OYUN BITTI" (Ortada Kırmızı Büyük Yazı)
            SDL_SetRenderDrawColor(renderer, 220, 20, 60, 255); // Crimson Kırmızı
            drawStrokeText(renderer, "OYUN BITTI", 286, WINDOW_HEIGHT / 2 - 80, 20, 36, 6);

            // Nihai Skor (Altın Sarısı)
            SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255); // Gold
            char finalScoreStr[64];
            sprintf(finalScoreStr, "SKORUNUZ: %d", score);
            int charCount = strlen(finalScoreStr);
            int textW = charCount * 16;
            drawStrokeText(renderer, finalScoreStr, (WINDOW_WIDTH - textW) / 2, WINDOW_HEIGHT / 2 - 20, 12, 20, 4);

            // R ile Yeniden Başlatma Talimatı (Beyaz)
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // Beyaz
            const char* restartMsg = "YENIDEN BASLAMAK ICIN R TUSUNA BASIN";
            int restartCharCount = strlen(restartMsg);
            int restartW = restartCharCount * 11;
            drawStrokeText(renderer, restartMsg, (WINDOW_WIDTH - restartW) / 2, WINDOW_HEIGHT / 2 + 40, 8, 14, 3);
        }

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
