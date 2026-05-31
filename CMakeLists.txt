cmake_minimum_required(VERSION 3.12)
project(KostebekVurmaca C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Kaynak dosyalar
set(SOURCES main.c)

# Hedef executable
add_executable(kostebek_vurmaca ${SOURCES})

# SDL2 Kütüphanelerini Bul
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_mixer REQUIRED)

# Başlık dosyaları dizinlerini ekle
target_include_directories(kostebek_vurmaca PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
)

# Kütüphaneleri bağla
target_link_libraries(kostebek_vurmaca PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${SDL2_MIXER_LIBRARIES}
)
