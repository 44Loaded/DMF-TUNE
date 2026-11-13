# DMF TUNE v1.1

Автотюн VST3 от DMF LAB: автоматическая коррекция высоты тона по тональности и гамме.

## Что в версии 1.1

- Меньше ручек: SPEED, STABILIZE, RANGE, FORMANT, CHORUS, MIX.
- NEG SPEED: мгновенная коррекция (жёсткий тюн, без плавного перехода).
- STABILIZE: стабилизатор нот, держит ноту и не даёт ей дрожать на границе.
- CHORUS: стерео-хорус после тюна.
- HEATMAP: подсвечивает ноты, на которые реально попадает голос.
- Зелёный дым на дисплее: появляется на подстроенной высоте и уплывает вместе с голосом. Чем сильнее коррекция, тем сильнее он клубится.
- Пресеты: HARD SNAP, ROBOT, SOFT NATURE, SUBTLE FIX, WIDE GLIDE.

Пока не сделано: Groups (связка нескольких инстансов одной кнопкой). Это следующий шаг.

## Как получить .vst3 без установки компилятора

1. Создай на GitHub новый репозиторий (можно приватный).
2. Загрузи в него содержимое этой папки: файлы `CMakeLists.txt`, `README.md`, папки `Source`, `assets`, `tests`. Перетаскивай файлы и папки внутрь страницы репозитория.
3. Отдельно загрузи папку `.github` (она скрытая, но в Windows видна). Если GitHub её не принял, открой `.github/workflows/build.yml` в Блокноте, выдели всё, скопируй и создай файл через **Add file → Create new file** с именем `.github/workflows/build.yml`.
4. Открой вкладку **Actions**, дождись зелёной галочки (около 10 минут).
5. В конце запуска в разделе **Artifacts** скачай **DMF-TUNE-Windows-VST3**, распакуй и положи `DMF TUNE.vst3` в `C:\Program Files\Common Files\VST3\`.

## Локальная сборка

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target DMF_TUNE_VST3
```

Нужны CMake 3.22+ и компилятор C++17 (Visual Studio 2022 на Windows, Xcode на macOS).

## Тесты

```
g++ -O2 -std=c++17 -o build_test/test_engine tests/test_engine.cpp
python3 tests/run_tests.py build_test/test_engine

g++ -O2 -std=c++17 -o build_test/test_chorus tests/test_chorus.cpp
./build_test/test_chorus
```

Тесты движка нужны Python 3, numpy и scipy.
