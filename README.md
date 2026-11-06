# FileByChunks — FileCollector

**Дата спецификации:** 06.11.2026. **Язык:** C++20. **Сборка:** CMake ≥ 3.25.
**Тесты:** GoogleTest и CTest. **Целевая ОС:** macOS, также Linux.

Потокобезопасное восстановление файлов из пересекающихся, дублирующихся и
поступающих в произвольном порядке чанков. Библиотека работает только с RAM.
Публичный полиморфный контракт расположен в
`include/file_collector/i_file_collector.hpp`.

## Контракт

- `CollectFile(id, size)` регистрирует файл; повторный `id` запрещён.
- `OnNewChunk(id, pos, vector<uint8_t>&&)` асинхронно ставит чанк в ThreadPool.
  Проверка `id` и границ синхронная. Чанки после завершения файла игнорируются.
- `GetFile(id, check_complete)` ждёт только задачи этого файла и возвращает
  `shared_ptr<const vector<uint8_t>>`. Готовый файл возвращается без копии.
- `GetFile(id, pos, length, check_complete)` ждёт задачи и возвращает
  отдельный неизменяемый снимок диапазона.
- `TakeFile(id, check_complete)` ждёт задачи, возвращает изменяемый
  `shared_ptr<vector<uint8_t>>` и снимает регистрацию. Если общий готовый
  буфер уже находится у читателя, выполняется копирование ради неизменяемости
  ранее выданного `FileView`.
- `RemoveFile(id)` удаляет регистрацию; `ClearFile(id)` обнуляет данные,
  сохраняет регистрацию и начинает сборку заново.

Полнота проверяется по умолчанию. `check_complete=false` разрешает неполный
снимок с нулями в ещё не записанных позициях. Ошибки полноты —
`std::logic_error`, неверные `id` или границы — `std::out_of_range`,
асинхронные ошибки сохраняются до `ClearFile` и передаются через `GetFile`
или `TakeFile`. Параллельные операции жизненного цикла одного файла
сериализованы; обращения во время закрытия могут получить `logic_error`.

## Сборка на macOS

Из корня проекта:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
```

GoogleTest находится через `find_package(GTest CONFIG)` или загружается
`FetchContent` по фиксированному commit. Для работы без сети можно указать
`-DFILE_COLLECTOR_GTEST_SOURCE_DIR=/path/to/googletest` при конфигурации.

### Санитайзеры (две отдельные сборки)

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DFILE_COLLECTOR_ASAN_UBSAN=ON
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure

cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DFILE_COLLECTOR_TSAN=ON
cmake --build build-tsan --parallel
ctest --test-dir build-tsan --output-on-failure
```

На Apple Silicon/AppleClang совместимость и доступность санитайзеров зависят
от поставки компилятора; если TSan не поддерживается, используйте отдельную
поддерживаемую сборочную среду. ASan/UBSan и TSan не совмещаются.
В окружениях с `ptrace` LeakSanitizer может завершаться аварийно; в таком
случае отдельный диагностический прогон возможен с
`ASAN_OPTIONS=detect_leaks=0`, но он **не проверяет утечки памяти**.

### Профилирование

Тест `Performance.BaselineMiBPerSecondAndSubmissionLatency` выводит
throughput, p50/p95 **времени вызова постановки чанка** и пиковую RSS
процесса. Это не p50/p95 выполнения задач ThreadPool, не время удержания
mutex и не точный пик собственной памяти FileCollector.

```sh
./build-release/tests/file_collector_tests --gtest_filter=Performance.*
```

Для системного профилирования на macOS можно дополнительно применить
встроенный Xcode Instruments, например шаблон Time Profiler. Выводы о
производительности нужно делать по повторным прогонам на целевой машине.

## Организация

- `include/` — внешний интерфейс и класс.
- `src/` — реестр, состояние, две побайтовые битовые маски, ThreadPool.
- `tests/` — корректность, границы, снимки, жизненный цикл, многопоточность,
  инъекция отказов, нагрузка и метрики.
- `Stress.RandomizedOverlapSpectrumAcrossSizesAndSeeds` проверяет 8 размеров
  файлов × 4 фиксированных seed, случайную длину/позицию чанков, полные
  дубликаты и пересечения от 1 байта до 100%; одновременно работают
  8 producers. Случайная исходная разбивка гарантирует покрытие всего файла.
- `Stress.RandomizedSnapshotsStayImmutableDuringConcurrentIngestion`
  проверяет 32 снимка при параллельном приёме четырьмя producers и
  неизменяемость исходного пустого снимка после окончательной сборки.
- `src/fault_injector.hpp` — внутренние, необязательные точки отказа,
  передаваемые конструктору для детерминированных тестов.

В нормальном конструкторе число workers равно
`std::thread::hardware_concurrency()` (fallback — 1 при нуле).
При тестировании разрешена явная настройка числа работников через конструктор.

## Сдача

ZIP: `Владислав_Агибалов_С++.zip` — исходники, CMake, тесты, README.
В Skillaz отдельно подаётся `Выполнено.txt`, содержащий слово `Выполнено`.
Бинарные артефакты, сборочные директории и `.git` в ZIP не включать.

