# Whack-a-Mole (C & SDL2)

Programlama II dersi için C dili ve SDL2 kütüphanesi kullanılarak geliştirilen gelişmiş bir Whack-a-Mole (Köstebek Vurmaca) oyunu.

## Özellikler (Yapım Aşamasında)
- [x] Pencere Oluşturma ve Tam Ekran Desteği (F11)
- [ ] Arkaplan Çizimi
- [ ] Animasyonlu Köstebekler (3x3 Grid)
- [ ] Tıklama Algılama ve Puanlama
- [ ] Bomba Mekaniği (Tek Can Sistemi)
- [ ] Zorluk Seçimi (Kolay, Orta, Zor)
- [ ] Liderlik Tablosu (Leaderboard)

## Nasıl Derlenir ve Çalıştırılır?
Projeyi derlemek için sisteminizde GCC derleyicisi ve SDL2 kütüphanesi kurulu olmalıdır. 

Terminal üzerinden kaynak kodu (`main.c`) derlemek için şu komutu kullanabilirsiniz:
```bash
gcc main.c -lmingw32 -lSDL2main -lSDL2 -o whackamole
```
