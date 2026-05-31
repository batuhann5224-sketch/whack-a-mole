#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>

#define WINDOW_WIDTH    800
#define WINDOW_HEIGHT   600
#define MOLE_TEX_COUNT  4
#define HOLE_COUNT      9   // 3x3 grid

// =====================================================
// ADIM 4: Köstebek Animasyon Durumları (State Machine)
// =====================================================
typedef enum { MOLE_HIDDEN, MOLE_RISING, MOLE_VISIBLE, MOLE_FALLING } MoleState;
typedef enum { ITEM_MOLE, ITEM_BOMB } ItemType;
typedef enum { STATE_MENU, STATE_PLAYING, STATE_INPUT_NAME, STATE_GAME_OVER } GameState;
typedef enum { DIFF_EASY, DIFF_MEDIUM, DIFF_HARD } Difficulty;

#define MAX_LEADERBOARD_ENTRIES 5
typedef struct {
    char name[16];
    int score;
} LeaderboardEntry;

// Liderlik tablosu yardimci fonksiyonlari
void loadLeaderboard(LeaderboardEntry* board, int* count) {
    *count = 0;
    FILE* f = fopen("highscore.txt", "r");
    if (!f) return;
    while (*count < MAX_LEADERBOARD_ENTRIES && fscanf(f, "%15s %d", board[*count].name, &board[*count].score) == 2) {
        (*count)++;
    }
    fclose(f);
}

void saveLeaderboard(LeaderboardEntry* board, int count) {
    FILE* f = fopen("highscore.txt", "w");
    if (!f) return;
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s %d\n", board[i].name, board[i].score);
    }
    fclose(f);
}

void addLeaderboardEntry(LeaderboardEntry* board, int* count, const char* name, int score) {
    if (*count < MAX_LEADERBOARD_ENTRIES) {
        snprintf(board[*count].name, sizeof(board[*count].name), "%s", name);
        board[*count].score = score;
        (*count)++;
    } else {
        if (score > board[MAX_LEADERBOARD_ENTRIES - 1].score) {
            snprintf(board[MAX_LEADERBOARD_ENTRIES - 1].name, sizeof(board[MAX_LEADERBOARD_ENTRIES - 1].name), "%s", name);
            board[MAX_LEADERBOARD_ENTRIES - 1].score = score;
        }
    }
    // Bubble sort descending
    for (int i = 0; i < *count - 1; i++) {
        for (int j = 0; j < *count - i - 1; j++) {
            if (board[j].score < board[j+1].score) {
                LeaderboardEntry temp = board[j];
                board[j] = board[j+1];
                board[j+1] = temp;
            }
        }
    }
}

bool qualifiesForLeaderboard(LeaderboardEntry* board, int count, int score) {
    if (count < MAX_LEADERBOARD_ENTRIES) return true;
    return (score > board[MAX_LEADERBOARD_ENTRIES - 1].score);
}

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
        case 'F':
            SDL_RenderDrawLine(r, x, y, x + w, y);
            SDL_RenderDrawLine(r, x, y, x, y + h);
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
        case 'J':
            SDL_RenderDrawLine(r, x + w / 2, y, x + w, y);
            SDL_RenderDrawLine(r, x + w * 3 / 4, y, x + w * 3 / 4, y + h * 4 / 5);
            SDL_RenderDrawLine(r, x + w * 3 / 4, y + h * 4 / 5, x + w / 2, y + h);
            SDL_RenderDrawLine(r, x + w / 2, y + h, x + w / 4, y + h * 4 / 5);
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
        case 'Q':
            SDL_RenderDrawLine(r, x, y, x + w * 3 / 4, y);
            SDL_RenderDrawLine(r, x, y, x, y + h * 3 / 4);
            SDL_RenderDrawLine(r, x + w * 3 / 4, y, x + w * 3 / 4, y + h * 3 / 4);
            SDL_RenderDrawLine(r, x, y + h * 3 / 4, x + w * 3 / 4, y + h * 3 / 4);
            SDL_RenderDrawLine(r, x + w / 2, y + h / 2, x + w, y + h);
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
        case 'W':
            SDL_RenderDrawLine(r, x, y, x + w / 4, y + h);
            SDL_RenderDrawLine(r, x + w / 4, y + h, x + w / 2, y + h / 2);
            SDL_RenderDrawLine(r, x + w / 2, y + h / 2, x + w * 3 / 4, y + h);
            SDL_RenderDrawLine(r, x + w * 3 / 4, y + h, x + w, y);
            break;
        case 'X':
            SDL_RenderDrawLine(r, x, y, x + w, y + h);
            SDL_RenderDrawLine(r, x + w, y, x, y + h);
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
        case '\'':
            SDL_RenderDrawLine(r, x + w / 2, y, x + w / 2, y + h / 4);
            break;
        case '>':
            SDL_RenderDrawLine(r, x, y, x + w, y + h / 2);
            SDL_RenderDrawLine(r, x + w, y + h / 2, x, y + h);
            break;
        case '<':
            SDL_RenderDrawLine(r, x + w, y, x, y + h / 2);
            SDL_RenderDrawLine(r, x, y + h / 2, x + w, y + h);
            break;
    }
}

void drawStrokeText(SDL_Renderer* r, const char* text, int x, int y, int charW, int charH, int spacing) {
    int curX = x;
    while (*text) {
        char c = *text;
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        // Orta karar kalinlik: Sadece 1 piksel saga kaydirarak ciziyoruz kanka
        drawStrokeChar(r, c, curX, y, charW, charH);
        drawStrokeChar(r, c, curX + 1, y, charW, charH);
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
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("SDL_mixer baslatilamadi! %s\n", Mix_GetError());
    }

    // 2. Pencere oluştur
    SDL_Window* window = SDL_CreateWindow(
        "Köstebek Vurmaca",
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
    SDL_Texture* hammerTex = loadTexture(renderer, "hammer.png");

    // =====================================================
    // ADIM 9: Temizlenmis arka plan gorseli yukle
    // =====================================================
    SDL_Texture* bgTex = loadTexture(renderer, "background.png");

    // Mix_Music ve Mix_Chunk ses dosyalarini yukle kanka
    Mix_Music* bgm = Mix_LoadMUS("muzik.mp3");
    Mix_Chunk* hitSound = Mix_LoadWAV("vurma.wav");
    Mix_Chunk* explodeSound = Mix_LoadWAV("patlama.wav");
    Mix_Chunk* gameOverSound = Mix_LoadWAV("oyun_bitti.wav");

    int holeTexW = 0, holeTexH = 0;
    if (holeTex) SDL_QueryTexture(holeTex, NULL, NULL, &holeTexW, &holeTexH);

    // =====================================================
    // ADIM 9: Koordinatlar arka plan resmindeki deliklere gore hizalandi
    // Gorsel 2816x1536 -> 800x600 mantiksal alana olceklendi
    // Ust sira: kucuk delikler (perspektif), alt sira: buyuk delikler
    // =====================================================
    // ADIM 9: Delikler toprak alani icinde kalacak sekilde hizalandi
    // Gorsel 2816x1536 -> 800x600 mantiksal alana olceklendi
    // Ust sira: kucuk delikler (perspektif), alt sira: buyuk delikler
    // =====================================================
    Hole holes[HOLE_COUNT];
    int colsRow[3][3] = {
        {270, 400, 530},   // Ust sira: en dar (y=340)
        {240, 400, 560},   // Orta sira (y=430)
        {210, 400, 590}    // Alt sira (y=520)
    };
    int rows[3] = {340, 430, 520};
    int displayWs[3] = {110, 140, 170};
    int displayHs[3] = {55,  70,  85};
    int drawWs[3]    = {65,  85,  105};
    int drawHs[3]    = {76,  98,  121};
    int holeIdx = 0;
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            initHole(&holes[holeIdx++], colsRow[row][col], rows[row],
                displayWs[row], displayHs[row],
                drawWs[row], drawHs[row]);
        }
    }

    GameState gameState = STATE_MENU;
    Difficulty selectedDifficulty = DIFF_MEDIUM;
    int score = 0;
    SDL_SetWindowTitle(window, "Köstebek Vurmaca | Ana Menü");

    LeaderboardEntry leaderboard[MAX_LEADERBOARD_ENTRIES];
    int leaderboardCount = 0;
    loadLeaderboard(leaderboard, &leaderboardCount);



    char inputName[16] = "";

    // Aynı anda sadece 1 delikten köstebek çıkar
    int  activeHole      = -1;          // Şu an aktif delik (-1 = yok)
    Uint32 nextSpawnTimer = SDL_GetTicks();
    Uint32 nextSpawnDelay = 800;        // İlk köstebek 0.8s sonra çıksın
    Uint32 gameStartTime  = SDL_GetTicks();
    Uint32 gameDuration   = 30000;      // 30 saniye (ms)
    int timeLeft          = 30;

    // Zorluk Taban Parametreleri
    float baseSpeed = 280.0f;
    Uint32 baseVisibleWait = 1000;
    int bombChance = 25;

    int mouseX = 0, mouseY = 0;
    Uint32 swingEndTime = 0;
    SDL_ShowCursor(SDL_DISABLE); // Sistem imlecini gizle kanka

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
                Mix_HaltMusic();
                if (gameOverSound) Mix_PlayChannel(-1, gameOverSound, 0);
                if (qualifiesForLeaderboard(leaderboard, leaderboardCount, score)) {
                    gameState = STATE_INPUT_NAME;
                    inputName[0] = '\0';
                    SDL_StartTextInput();
                    SDL_SetWindowTitle(window, "Köstebek Vurmaca | Yeni Rekor!");
                } else {
                    gameState = STATE_GAME_OVER;
                    SDL_SetWindowTitle(window, "Köstebek Vurmaca | Oyun Bitti! Süre Doldu.");
                }
            }
        }

        // =====================================================
        // ADIM 5 & 6 & 7: Tek Aktif Delik State Machine & Oyun Durumu & Dinamik Zorluk
        // =====================================================
        if (gameState == STATE_PLAYING) {
            if (activeHole == -1) {
                // Hiç aktif delik yok — bekleme süresi doldu mu?
                if (now - nextSpawnTimer >= nextSpawnDelay) {
                    activeHole = rand() % HOLE_COUNT; // Rastgele bir delik seç
                    Hole* h = &holes[activeHole];
                    // Köstebek mi bomba mı? (Zorluğa göre bomba gelme ihtimali)
                    if (bombTex && rand() % 100 < bombChance)
                        h->itemType = ITEM_BOMB;
                    else {
                        h->itemType    = ITEM_MOLE;
                        h->currentMole = rand() % MOLE_TEX_COUNT;
                    }

                    // Dinamik Zorluk: Skor arttıkça köstebekler daha hızlı hareket eder!
                    h->speed = baseSpeed + (score * 0.5f);

                    // Dinamik Zorluk: Skor arttıkça ekranda bekleme süresi azalır (min limit 350ms)
                    int dynamicVisibleWait = (int)baseVisibleWait - (score * 12);
                    if (dynamicVisibleWait < 350) dynamicVisibleWait = 350;

                    h->moleY     = h->hideY;
                    h->state     = MOLE_RISING;
                    h->stateTimer = now;
                    h->visibleWait = dynamicVisibleWait + rand() % 300; // Küçük rastgelelik ekle
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
                        // Seçilen zorluğa göre sonraki köstebeği bekleme süresi
                        int dynamicSpawnDelay = 500 + rand() % 1000;
                        if (selectedDifficulty == DIFF_HARD) dynamicSpawnDelay = 300 + rand() % 600;
                        else if (selectedDifficulty == DIFF_EASY) dynamicSpawnDelay = 800 + rand() % 1200;
                        nextSpawnDelay = dynamicSpawnDelay;
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
                } else if (gameState == STATE_MENU) {
                    // Ana menüde zorluk seçimi (Yön tuşları ve Enter desteği kanka!)
                    bool startTheGame = false;
                    if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_LEFT) {
                        if (selectedDifficulty == DIFF_MEDIUM) selectedDifficulty = DIFF_EASY;
                        else if (selectedDifficulty == DIFF_HARD) selectedDifficulty = DIFF_MEDIUM;
                    } else if (event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_RIGHT) {
                        if (selectedDifficulty == DIFF_EASY) selectedDifficulty = DIFF_MEDIUM;
                        else if (selectedDifficulty == DIFF_MEDIUM) selectedDifficulty = DIFF_HARD;
                    } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER || event.key.keysym.sym == SDLK_SPACE) {
                        startTheGame = true;
                    } else if (event.key.keysym.sym == SDLK_1 || event.key.keysym.sym == SDLK_KP_1) {
                        selectedDifficulty = DIFF_EASY;
                        startTheGame = true;
                    } else if (event.key.keysym.sym == SDLK_2 || event.key.keysym.sym == SDLK_KP_2) {
                        selectedDifficulty = DIFF_MEDIUM;
                        startTheGame = true;
                    } else if (event.key.keysym.sym == SDLK_3 || event.key.keysym.sym == SDLK_KP_3) {
                        selectedDifficulty = DIFF_HARD;
                        startTheGame = true;
                    }

                    if (startTheGame) {
                        // Seçili zorluğa göre parametreleri ata
                        if (selectedDifficulty == DIFF_EASY) {
                            baseSpeed = 200.0f;
                            baseVisibleWait = 1400;
                            bombChance = 10;
                            SDL_SetWindowTitle(window, "Köstebek Vurmaca | Zorluk: Kolay");
                        } else if (selectedDifficulty == DIFF_MEDIUM) {
                            baseSpeed = 280.0f;
                            baseVisibleWait = 1000;
                            bombChance = 25;
                            SDL_SetWindowTitle(window, "Köstebek Vurmaca | Zorluk: Orta");
                        } else if (selectedDifficulty == DIFF_HARD) {
                            baseSpeed = 380.0f;
                            baseVisibleWait = 700;
                            bombChance = 40;
                            SDL_SetWindowTitle(window, "Köstebek Vurmaca | Zorluk: Zor");
                        }
                        score = 0;
                        activeHole = -1;
                        for (int i = 0; i < HOLE_COUNT; i++) {
                            holes[i].state = MOLE_HIDDEN;
                            holes[i].moleY = holes[i].hideY;
                        }
                        gameStartTime = SDL_GetTicks();
                        timeLeft = 30;
                        gameState = STATE_PLAYING;
                        if (bgm) Mix_PlayMusic(bgm, -1);
                    }
                } else if (gameState == STATE_INPUT_NAME) {
                    if (event.key.keysym.sym == SDLK_BACKSPACE) {
                        int len = strlen(inputName);
                        if (len > 0) {
                            inputName[len - 1] = '\0';
                        }
                    } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
                        SDL_StopTextInput();
                        if (strlen(inputName) == 0) {
                            strcpy(inputName, "OYUNCU");
                        }
                        addLeaderboardEntry(leaderboard, &leaderboardCount, inputName, score);
                        saveLeaderboard(leaderboard, leaderboardCount);
                        gameState = STATE_GAME_OVER;
                        SDL_SetWindowTitle(window, "Köstebek Vurmaca | Skor Kaydedildi!");
                    }
                } else if (event.key.keysym.sym == SDLK_r) {
                    // Oyun bittiğinde 'R' ile ana menüye geri dön kanka!
                    if (gameState == STATE_GAME_OVER) {
                        score = 0;
                        activeHole = -1;
                        for (int i = 0; i < HOLE_COUNT; i++) {
                            holes[i].state = MOLE_HIDDEN;
                            holes[i].moleY = holes[i].hideY;
                        }
                        gameState = STATE_MENU;
                        SDL_SetWindowTitle(window, "Köstebek Vurmaca | Ana Menü");
                    }
                }
            } else if (event.type == SDL_TEXTINPUT) {
                if (gameState == STATE_INPUT_NAME) {
                    if (strlen(inputName) < 10) {
                        char c = event.text.text[0];
                        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
                        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
                            int len = strlen(inputName);
                            inputName[len] = c;
                            inputName[len + 1] = '\0';
                        }
                    }
                }
            } else if (event.type == SDL_MOUSEMOTION) {
                mouseX = event.motion.x;
                mouseY = event.motion.y;
            } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                mouseX = event.button.x;
                mouseY = event.button.y;
                swingEndTime = SDL_GetTicks() + 100;
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
                                Mix_HaltMusic();
                                if (explodeSound) Mix_PlayChannel(-1, explodeSound, 0);
                                if (gameOverSound) Mix_PlayChannel(-1, gameOverSound, 0);
                                if (qualifiesForLeaderboard(leaderboard, leaderboardCount, score)) {
                                    gameState = STATE_INPUT_NAME;
                                    inputName[0] = '\0';
                                    SDL_StartTextInput();
                                    SDL_SetWindowTitle(window, "Köstebek Vurmaca | Yeni Rekor!");
                                } else {
                                    gameState = STATE_GAME_OVER;
                                    SDL_SetWindowTitle(window, "Köstebek Vurmaca | 💥 Patladın! Oyun Bitti!");
                                }
                            } else {
                                if (hitSound) Mix_PlayChannel(-1, hitSound, 0);
                                int scoreGain = 10;
                                if (selectedDifficulty == DIFF_MEDIUM) scoreGain = 20;
                                else if (selectedDifficulty == DIFF_HARD) scoreGain = 30;
                                score += scoreGain;

                                bool timeBonus = false;
                                if (h->currentMole == 3) {
                                    timeBonus = true;
                                    gameStartTime += 3000;
                                    if (gameStartTime > now) gameStartTime = now;
                                }

                                // Kalan sureyi anlik guncelle ki baslikta dogru gozuksun
                                int elapsed = (int)(now - gameStartTime);
                                timeLeft = (int)((gameDuration - elapsed) / 1000);
                                if (timeLeft > 30) timeLeft = 30;

                                if (timeBonus) {
                                    sprintf(title, "Köstebek Vurmaca | ⭐ +%d & ⏱️ +3sn | Skor: %d | Süre: %d", scoreGain, score, timeLeft);
                                } else {
                                    sprintf(title, "Köstebek Vurmaca | ⭐ +%d | Skor: %d | Süre: %d", scoreGain, score, timeLeft);
                                }
                                SDL_SetWindowTitle(window, title);

                                // Vurulunca hemen geri kacsin
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
        // ADIM 9: Gradient yerine gercek arka plan gorseli ciz
        if (bgTex) {
            SDL_RenderCopy(renderer, bgTex, NULL, NULL);
        } else {
            drawGradientBackground(renderer);
        }

        // Delikleri arka satırdan öne doğru çiz (doğal perspektif için)
        for (int i = 0; i < HOLE_COUNT; i++)
            drawHole(renderer, &holes[i], holeTex, holeTexW, holeTexH, moleTex, bombTex);

        // Oynanış HUD (Skor, Süre, Zorluk)
        if (gameState == STATE_PLAYING) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // Beyaz
            char scoreStr[32];
            sprintf(scoreStr, "SKOR: %d", score);
            drawStrokeText(renderer, scoreStr, 20, 20, 10, 16, 4);

            char timeStr[32];
            sprintf(timeStr, "SURE: %d", timeLeft);
            drawStrokeText(renderer, timeStr, WINDOW_WIDTH - 150, 20, 10, 16, 4);

            // Mod Yazısı
            char diffStr[32];
            if (selectedDifficulty == DIFF_EASY) sprintf(diffStr, "KOLAY");
            else if (selectedDifficulty == DIFF_MEDIUM) sprintf(diffStr, "ORTA");
            else sprintf(diffStr, "ZOR");

            char fullDiffStr[64];
            sprintf(fullDiffStr, "MOD: %s", diffStr);
            int fullDiffW = strlen(fullDiffStr) * 11;
            drawStrokeText(renderer, fullDiffStr, (WINDOW_WIDTH - fullDiffW) / 2, 20, 8, 14, 3);
        }

        // ANA MENÜ EKRANI (Overlay)
        if (gameState == STATE_MENU) {
            // Yarı şeffaf siyah örtü
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
            SDL_Rect overlay = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
            SDL_RenderFillRect(renderer, &overlay);

            // Başlık "KOSTEBEK VURMACA" (Ortalanmış ve Yanıp Sönen Efekt)
            Uint32 ticks = SDL_GetTicks();
            if ((ticks / 400) % 2 == 0) {
                SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255); // Altın
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255); // Turuncu
            }
            int titleW = strlen("KOSTEBEK VURMACA") * 26 - 6;
            drawStrokeText(renderer, "KOSTEBEK VURMACA", (WINDOW_WIDTH - titleW) / 2, 120, 20, 36, 6);

            // Açıklama
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // Beyaz
            const char* desc = "YON TUSLARI VE ENTER ILE SECIM YAPIN";
            int descLen = strlen(desc);
            int descW = descLen * 11;
            drawStrokeText(renderer, desc, (WINDOW_WIDTH - descW) / 2, 220, 8, 14, 3);

            // Zorluklar
            // 1 - KOLAY
            const char* optEasy = (selectedDifficulty == DIFF_EASY) ? "> 1 - KOLAY <" : "  1 - KOLAY  ";
            if (selectedDifficulty == DIFF_EASY) SDL_SetRenderDrawColor(renderer, 50, 205, 50, 255);
            else SDL_SetRenderDrawColor(renderer, 130, 130, 130, 255);
            int easyW = strlen(optEasy) * 14 - 4;
            drawStrokeText(renderer, optEasy, (WINDOW_WIDTH - easyW) / 2, 280, 10, 18, 4);

            // 2 - ORTA
            const char* optMedium = (selectedDifficulty == DIFF_MEDIUM) ? "> 2 - ORTA <" : "  2 - ORTA  ";
            if (selectedDifficulty == DIFF_MEDIUM) SDL_SetRenderDrawColor(renderer, 255, 165, 0, 255);
            else SDL_SetRenderDrawColor(renderer, 130, 130, 130, 255);
            int medW = strlen(optMedium) * 14 - 4;
            drawStrokeText(renderer, optMedium, (WINDOW_WIDTH - medW) / 2, 330, 10, 18, 4);

            // 3 - ZOR
            const char* optHard = (selectedDifficulty == DIFF_HARD) ? "> 3 - ZOR <" : "  3 - ZOR  ";
            if (selectedDifficulty == DIFF_HARD) SDL_SetRenderDrawColor(renderer, 220, 20, 60, 255);
            else SDL_SetRenderDrawColor(renderer, 130, 130, 130, 255);
            int hardW = strlen(optHard) * 14 - 4;
            drawStrokeText(renderer, optHard, (WINDOW_WIDTH - hardW) / 2, 380, 10, 18, 4);

            // Liderlik Tablosu (En Yüksek Skorlar)
            SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255); // Altın
            const char* titleLeaderboard = "EN YUKSEK SKORLAR";
            int titleLeaderboardW = strlen(titleLeaderboard) * 8;
            drawStrokeText(renderer, titleLeaderboard, (WINDOW_WIDTH - titleLeaderboardW) / 2, 435, 6, 10, 2);

            for (int i = 0; i < 5; i++) {
                char entryStr[64];
                if (i < leaderboardCount) {
                    sprintf(entryStr, "%d. %-10s  %5d", i + 1, leaderboard[i].name, leaderboard[i].score);
                } else {
                    sprintf(entryStr, "%d. ----------  -----", i + 1);
                }
                int entryW = strlen(entryStr) * 8;
                drawStrokeText(renderer, entryStr, (WINDOW_WIDTH - entryW) / 2, 455 + i * 18, 6, 10, 2);
            }

            // Alt Bilgi
            SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255); // Gri
            const char* footer = "KEYIFLI OYUNLAR DILERIZ";
            int footerLen = strlen(footer);
            int footerW = footerLen * 8;
            drawStrokeText(renderer, footer, (WINDOW_WIDTH - footerW) / 2, WINDOW_HEIGHT - 60, 6, 10, 2);
        }

        // İsim Giriş Ekranı (Overlay)
        if (gameState == STATE_INPUT_NAME) {
            // Yarı şeffaf siyah overlay
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 220); // Daha koyu şeffaflık
            SDL_Rect overlay = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
            SDL_RenderFillRect(renderer, &overlay);

            // "YENI REKOR!" (Ortada altın sarısı ve yanıp sönen büyük yazı)
            Uint32 ticks = SDL_GetTicks();
            if ((ticks / 300) % 2 == 0) {
                SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255); // Altın
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // Beyaz
            }
            int rekorW = strlen("YENI REKOR") * 26 - 6;
            drawStrokeText(renderer, "YENI REKOR", (WINDOW_WIDTH - rekorW) / 2, 130, 20, 36, 6);

            // Skor bilgisi
            SDL_SetRenderDrawColor(renderer, 50, 205, 50, 255); // Yeşil
            char scoreStr[64];
            sprintf(scoreStr, "SKORUNUZ: %d", score);
            int scoreW = strlen(scoreStr) * 16;
            drawStrokeText(renderer, scoreStr, (WINDOW_WIDTH - scoreW) / 2, 210, 12, 20, 4);

            // Adınızı Girin
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // Beyaz
            const char* promptText = "ADINIZI GIRIN VE ENTER'A BASIN";
            int promptW = strlen(promptText) * 8;
            drawStrokeText(renderer, promptText, (WINDOW_WIDTH - promptW) / 2, 280, 6, 10, 2);

            // Yazılan İsim (Kutulu ve yanıp sönen imleç kanka)
            SDL_SetRenderDrawColor(renderer, 255, 165, 0, 255); // Turuncu çerçeve
            SDL_Rect box1 = { (WINDOW_WIDTH - 240) / 2, 320, 240, 40 };
            SDL_Rect box2 = { (WINDOW_WIDTH - 240) / 2 - 1, 320 - 1, 240 + 2, 40 + 2 };
            SDL_RenderDrawRect(renderer, &box1);
            SDL_RenderDrawRect(renderer, &box2);

            char displayName[32];
            if ((ticks / 400) % 2 == 0) {
                sprintf(displayName, "%s_", inputName);
            } else {
                sprintf(displayName, "%s ", inputName);
            }
            int displayW = strlen(displayName) * 16;
            drawStrokeText(renderer, displayName, (WINDOW_WIDTH - displayW) / 2, 330, 12, 20, 4);
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
            int obW = strlen("OYUN BITTI") * 26 - 6;
            drawStrokeText(renderer, "OYUN BITTI", (WINDOW_WIDTH - obW) / 2, WINDOW_HEIGHT / 2 - 190, 20, 36, 6);

            // Nihai Skor (Altın Sarısı)
            SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255); // Gold
            char finalScoreStr[64];
            sprintf(finalScoreStr, "SKORUNUZ: %d", score);
            int charCount = strlen(finalScoreStr);
            int textW = charCount * 16;
            drawStrokeText(renderer, finalScoreStr, (WINDOW_WIDTH - textW) / 2, WINDOW_HEIGHT / 2 - 130, 12, 20, 4);

            // En Yüksek Skorlar (Liderlik Tablosu)
            SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255); // Gold
            const char* goTitle = "EN YUKSEK SKORLAR";
            int goTitleW = strlen(goTitle) * 8;
            drawStrokeText(renderer, goTitle, (WINDOW_WIDTH - goTitleW) / 2, WINDOW_HEIGHT / 2 - 70, 6, 10, 2);

            for (int i = 0; i < 5; i++) {
                char entryStr[64];
                if (i < leaderboardCount) {
                    sprintf(entryStr, "%d. %-10s  %5d", i + 1, leaderboard[i].name, leaderboard[i].score);
                } else {
                    sprintf(entryStr, "%d. ----------  -----", i + 1);
                }
                int entryW = strlen(entryStr) * 8;
                drawStrokeText(renderer, entryStr, (WINDOW_WIDTH - entryW) / 2, WINDOW_HEIGHT / 2 - 50 + i * 18, 6, 10, 2);
            }

            // R ile Yeniden Başlatma Talimatı (Beyaz)
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // Beyaz
            const char* restartMsg = "ANA MENUYE DONMEK ICIN R TUSUNA BASIN";
            int restartCharCount = strlen(restartMsg);
            int restartW = restartCharCount * 11;
            drawStrokeText(renderer, restartMsg, (WINDOW_WIDTH - restartW) / 2, WINDOW_HEIGHT / 2 + 80, 8, 14, 3);
        }

        // 5. Ozel Tokmak Imleci Ciz (En Ustte)
        if (hammerTex) {
            int hW = 48; // Cizim genisligi
            int hH = 48; // Cizim yuksekligi
            
            // Hitting hotspot (Tokmagin vuran sol kafa merkezi)
            // 128x128 olceginde ~x=25, y=45 -> 48x48 olceginde ~x=9, y=17
            int offset_x = 9;
            int offset_y = 17;
            
            SDL_Rect dstRect = { mouseX - offset_x, mouseY - offset_y, hW, hH };
            double angle = 0.0;
            
            // Donme merkezi (sapin ucu: 128x128'de ~x=110, y=115 -> 48x48'te ~x=41, y=43)
            SDL_Point pivot = { 41, 43 };
            
            if (SDL_GetTicks() < swingEndTime) {
                angle = -35.0; // Tiklama esnasinda tokmak -35 derece sola/asagi doner (vurus animasyonu)
            }
            
            SDL_RenderCopyEx(renderer, hammerTex, NULL, &dstRect, angle, &pivot, SDL_FLIP_NONE);
        }

        SDL_RenderPresent(renderer);
    }

    // 6. Temizlik
    for (int i = 0; i < MOLE_TEX_COUNT; i++)
        if (moleTex[i]) SDL_DestroyTexture(moleTex[i]);
    if (bombTex) SDL_DestroyTexture(bombTex);
    if (holeTex) SDL_DestroyTexture(holeTex);
    if (hammerTex) SDL_DestroyTexture(hammerTex);
    if (bgTex)   SDL_DestroyTexture(bgTex);

    // Audio temizligi kanka
    if (bgm) Mix_FreeMusic(bgm);
    if (hitSound) Mix_FreeChunk(hitSound);
    if (explodeSound) Mix_FreeChunk(explodeSound);
    if (gameOverSound) Mix_FreeChunk(gameOverSound);
    Mix_CloseAudio();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
