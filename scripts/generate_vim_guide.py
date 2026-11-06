"""Build the exact copy/paste Vim guide from the verified project sources."""

from pathlib import Path


PROJECT = Path(__file__).resolve().parent.parent
DESTINATION = PROJECT.parent / "FileByChunks_VIM_GUIDE.md"
ORDER = [
    "CMakeLists.txt",
    "tests/CMakeLists.txt",
    "tests/test_data.hpp",
    "tests/collector_tests.cpp",
    "tests/chunk_tests.cpp",
    "tests/boundary_tests.cpp",
    "tests/concurrency_tests.cpp",
    "tests/lifecycle_tests.cpp",
    "tests/snapshot_tests.cpp",
    "tests/exception_tests.cpp",
    "tests/thread_pool_tests.cpp",
    "tests/stress_tests.cpp",
    "tests/performance_tests.cpp",
    "include/file_collector/i_file_collector.hpp",
    "include/file_collector/file_collector.hpp",
    "src/fault_injector.hpp",
    "src/thread_pool.hpp",
    "src/file_state.hpp",
    "src/thread_pool.cpp",
    "src/file_state.cpp",
    "src/file_collector.cpp",
    ".gitignore",
    "scripts/package.sh",
    "scripts/generate_vim_guide.py",
    "README.md",
]


def output() -> str:
    lines = [
        "# FileByChunks — полные блоки файлов для Vim",
        "",
        "Актуальность: 06.11.2026. Порядок: иерархия → CMake → тесты → исходники.",
        "У каждого файла сначала команда `vim`, затем полный текст для вставки.",
        "",
        "```bash",
        "mkdir -p /Users/avs/safeboard/2026/11/cpp/FileByChunks",
        "cd /Users/avs/safeboard/2026/11/cpp/FileByChunks",
        "git init -b main",
        "git remote add origin git@github.com:multmine1/safeboard_2026_11_Cpp_chunks_-.git",
        "mkdir -p include/file_collector src tests scripts",
        "```",
        "",
    ]
    for filename in ORDER:
        data = (PROJECT / filename).read_text(encoding="utf-8")
        language = "cmake" if filename.endswith("CMakeLists.txt") else "bash" if filename.endswith(".sh") else "cpp" if filename.endswith((".cpp", ".hpp")) else "python" if filename.endswith(".py") else "text"
        lines.extend([
            f"## `{filename}`",
            "",
            "```bash",
            f"vim {filename}",
            "```",
            "",
            f"```{language}",
            data.rstrip("\n"),
            "```",
            "",
        ])
    lines.extend([
        "## Сборка, тесты и сдача",
        "",
        "```bash",
        "cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release",
        "cmake --build build-release --parallel",
        "ctest --test-dir build-release --output-on-failure",
        "cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DFILE_COLLECTOR_ASAN_UBSAN=ON",
        "cmake --build build-asan --parallel",
        "ctest --test-dir build-asan --output-on-failure",
        "cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DFILE_COLLECTOR_TSAN=ON",
        "cmake --build build-tsan --parallel",
        "ctest --test-dir build-tsan --output-on-failure",
        "bash scripts/package.sh",
        "printf 'Выполнено\\n' > ../Выполнено.txt",
        "```",
        "",
        "ZIP отправляется в Kaspersky Box, TXT — в Skillaz.",
        "",
    ])
    return "\n".join(lines)


if __name__ == "__main__":
    DESTINATION.write_text(output(), encoding="utf-8")
    print(DESTINATION)

