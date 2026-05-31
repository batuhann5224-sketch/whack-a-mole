# 🐹 Köstebek Vurmaca (Whack-a-Mole) Oyunu

C dili ve **SDL2** kütüphanesi kullanılarak geliştirilmiş, modern görsel ve işitsel detaylarla zenginleştirilmiş, retro tarzda bir arcade oyunudur. Projede harici yazı tipi dosyalarına bağımlılığı ortadan kaldırmak için özel bir **Retro Vektör Yazı Sistemi** geliştirilmiştir.

---

## 🚀 Öne Çıkan Özellikler

- **Dinamik Zorluk Seviyeleri:** Klavyeden Yön Tuşları veya `1, 2, 3` tuşları ile seçilebilen 3 farklı zorluk modu (Kolay, Orta, Zor). Oyun esnasında skor arttıkça köstebeklerin hızları ve ekranda kalma süreleri dinamik olarak zorlaşır.
- **Kayıt Sistemi (Leaderboard):** En yüksek 5 skoru `highscore.txt` dosyasında kalıcı olarak saklar ve hem ana menüde hem de oyun bitti ekranında gösterir.
- **Zaman Bonusu Sistemi:** Oyunda kalan süre 30 saniyedir. Saat simgeli özel köstebek vurulduğunda süreye **+3 saniye bonus** eklenir.
- **Özel Tokmak İmleci ve Swing Animasyonu:** Sistem imleci gizlenerek yerine özel bir tokmak imleci entegre edilmiştir. Sol tık yapıldığında tokmak vurma animasyonu gerçekleştirir.
- **Gelişmiş Ses Altyapısı (`SDL_mixer`):** Arka plan müziği, köstebek vurulma sesi, bomba patlama sesi ve oyun bitiş jenerik seslerini içerir.
- **Retro Vektör Yazı Çizim Sistemi:** Harici font dosyası yüklemeksizin, doğrudan çizgiler ve noktalarla ekran üzerine yazı yazabilen özel 2-pass horizontal shift (kalınlaştırılmış) yazı fonksiyonları.
- **Bomba Engeli:** Köstebekler haricinde deliklerden bomba da çıkabilir. Bombaya vurulduğu an oyun patlama efektiyle birlikte anında sona erer.

---

## 🎮 Oyun Kontrolleri

| Tuş | İşlev |
| :--- | :--- |
| **Yön Tuşları (Yukarı/Aşağı/Sol/Sağ)** | Ana Menüde zorluk dereceleri arasında gezinme. |
| **Enter / Space** | Seçili zorluğu başlatma. |
| **1, 2, 3** | Doğrudan ilgili zorlukta (Kolay, Orta, Zor) oyunu başlatma. |
| **R Tuşu** | Oyun Bittiğinde Ana Menüye geri dönme. |
| **F11** | Tam Ekran (Fullscreen) modunu açma / kapama. |
| **ESC** | Oyundan çıkış. |
| **Fare Sol Tık** | Delikten çıkan köstebeklere veya bombalara vurma (Tokmak vuruşu). |

---

## 📂 Proje Klasör Yapısı

```text
├── main.c              # Ana kaynak kodu (Tüm oyun döngüsü ve çizim mantığı)
├── Makefile            # MinGW-w64 ve Linux/macOS için derleme dosyası
├── CMakeLists.txt      # Modern CMake derleme yönergeleri
├── highscore.txt       # En yüksek skorların saklandığı veri dosyası
├── background.png      # Gemini logosundan arındırılmış retro arka plan görseli
├── bomb.png            # Deliklerden fırlayan bomba görseli
├── hole.png            # Köstebeklerin arkasında/önünde duran delik halkaları
├── hammer.png          # Özel tokmak imleci görseli
├── mole1.png           # Köstebek görseli 1
├── mole2.png           # Köstebek görseli 2
├── mole3.png           # Köstebek görseli 3
├── mole4.png           # Saatli köstebek (Zaman bonusu veren) görseli 4
├── muzik.mp3           # Arka planda döngüsel olarak çalan oyun müziği
├── vurma.wav           # Köstebeğe başarıyla vurulduğunda çalan ses efekti
├── patlama.wav         # Bombaya tıklandığında çalan patlama ses efekti
└── oyun_bitti.wav      # Oyun süresi bittiğinde veya patlama olduğunda çalan ses efekti
```

---

## 🛠️ Derleme ve Çalıştırma Yöntemleri

### Gerekli Kütüphaneler
Oyunu derlemek için sisteminizde **GCC (MinGW-w64)** derleyicisi ile birlikte aşağıdaki SDL2 kütüphanelerinin kurulu olması gerekir:
- `SDL2`
- `SDL2_image`
- `SDL2_mixer`

### 1. Manuel GCC Komutu ile Derleme (Tavsiye Edilen)
En hızlı ve doğrudan derleme yöntemi terminal üzerinden GCC komutunu çalıştırmaktır:

#### Windows (MSYS2 / MinGW-w64):
```bash
gcc main.c -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_mixer -o kostebek_vurmaca.exe
```

#### Linux / macOS:
```bash
gcc main.c -lSDL2 -lSDL2_image -lSDL2_mixer -o kostebek_vurmaca
```

---

### 2. Makefile ile Derleme
Eğer sisteminizde `make` aracı kuruluysa:
```bash
# Derlemek için:
make

# Temizlemek için:
make clean
```

---

### 3. CMake ile Derleme
CMake sistemini kullanarak derleme adımları:
```bash
mkdir build
cd build
cmake ..
cmake --build .
```
Derleme sonrasında `kostebek_vurmaca` çalıştırılabilir dosyası oluşturulacaktır.

---

## 📝 Lisans ve Proje Detayları
Bu proje, bilgisayar grafikleri ve oyun programlama temellerini öğrenmek amacıyla SDL2 kütüphanesi üzerine kurulmuş bir eğitim projesidir. Tasarımlar, sesler ve C kodlarının tamamı uyum içinde çalışacak şekilde optimize edilmiştir.
