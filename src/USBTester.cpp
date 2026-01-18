#include "USBTester.h"
#include "Logger.h"
#include <windows.h>
#include <vector>
#include <string>
#include <chrono>
#include <filesystem>


namespace fs = std::filesystem;

static std::string make_root(char driveLetter) {
    std::string root;
    root += driveLetter;
    root += ":\\";
    return root;
}

static std::string make_test_path(char driveLetter) {
    return make_root(driveLetter) + "usb_speed_test.bin";
}

static void* aligned_alloc_4096(size_t size) {
    return _aligned_malloc(size, 4096);
}
static void aligned_free_4096(void* p) {
    _aligned_free(p);
}

SpeedResult USBTester::TestSpeed(char driveLetter, size_t megabytes, bool /*verify*/) {
    SpeedResult r;
    r.testFilePath = make_test_path(driveLetter);

    const std::string root = make_root(driveLetter);
    if (!fs::exists(root) || !fs::is_directory(root)) {
        r.error = "Drive root not accessible: " + root;
        Logger::Error(r.error);
        return r;
    }

    // Use 8MB aligned chunks (must be multiple of sector size; 4096 alignment works well)
    const size_t chunkSize = 8ULL * 1024ULL * 1024ULL;
    const size_t totalBytes = megabytes * 1024ULL * 1024ULL;

    if (totalBytes == 0) {
        r.error = "Test size must be > 0 MB";
        Logger::Error(r.error);
        return r;
    }

    // totalBytes must be multiple of chunkSize for NO_BUFFERING simplicity
    size_t totalRounded = (totalBytes / chunkSize) * chunkSize;
    if (totalRounded < chunkSize) totalRounded = chunkSize;

    void* buf = aligned_alloc_4096(chunkSize);
    if (!buf) {
        r.error = "Failed to allocate aligned buffer";
        Logger::Error(r.error);
        return r;
    }

    // Fill buffer with a pattern
    unsigned char* b = (unsigned char*)buf;
    for (size_t i = 0; i < chunkSize; i++) b[i] = (unsigned char)((i * 131u + 7u) & 0xFF);

    Logger::Info("USB speed test (uncached) starting on " + root +
                 " size=" + std::to_string(megabytes) + "MB (rounded bytes=" + std::to_string(totalRounded) + ")");

    // -------------------
    // WRITE (write-through)
    // -------------------
    HANDLE hW = CreateFileA(
        r.testFilePath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,            // allow read by others
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        nullptr
    );

    if (hW == INVALID_HANDLE_VALUE) {
        r.error = "CreateFileA(write) failed";
        Logger::WinError(r.error + " path=" + r.testFilePath);
        aligned_free_4096(buf);
        return r;
    }

    auto w0 = std::chrono::steady_clock::now();
    size_t writtenTotal = 0;

    while (writtenTotal < totalRounded) {
        DWORD wrote = 0;
        BOOL ok = WriteFile(hW, buf, (DWORD)chunkSize, &wrote, nullptr);
        if (!ok || wrote != chunkSize) {
            r.error = "WriteFile failed (disk full / removed / permission)";
            Logger::WinError(r.error);
            CloseHandle(hW);
            aligned_free_4096(buf);
            return r;
        }
        writtenTotal += wrote;
    }

    FlushFileBuffers(hW);
    CloseHandle(hW);

    auto w1 = std::chrono::steady_clock::now();
    std::chrono::duration<double> wdt = w1 - w0;
    r.writeMBps = (writtenTotal / (1024.0 * 1024.0)) / (wdt.count() > 0 ? wdt.count() : 1.0);
    Logger::Info("Write done: " + std::to_string(r.writeMBps) + " MB/s");

    // -------------------
    // READ (no buffering)
    // -------------------
    HANDLE hR = CreateFileA(
        r.testFilePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr
    );

    if (hR == INVALID_HANDLE_VALUE) {
        r.error = "CreateFileA(read) failed";
        Logger::WinError(r.error + " path=" + r.testFilePath);
        aligned_free_4096(buf);
        return r;
    }

    // Ensure file pointer at start
    LARGE_INTEGER zero{};
    SetFilePointerEx(hR, zero, nullptr, FILE_BEGIN);

    auto r0 = std::chrono::steady_clock::now();
    size_t readTotal = 0;

    while (readTotal < writtenTotal) {
        DWORD got = 0;
        BOOL ok = ReadFile(hR, buf, (DWORD)chunkSize, &got, nullptr);
        if (!ok || got != chunkSize) {
            r.error = "ReadFile failed (device removed / invalid alignment / etc.)";
            Logger::WinError(r.error);
            CloseHandle(hR);
            aligned_free_4096(buf);
            return r;
        }
        readTotal += got;
    }

    CloseHandle(hR);

    auto r1t = std::chrono::steady_clock::now();
    std::chrono::duration<double> rdt = r1t - r0;
    r.readMBps = (readTotal / (1024.0 * 1024.0)) / (rdt.count() > 0 ? rdt.count() : 1.0);
    Logger::Info("Read done: " + std::to_string(r.readMBps) + " MB/s");

    aligned_free_4096(buf);

    // Cleanup test file
    try { fs::remove(r.testFilePath); }
    catch (...) { Logger::Warn("Could not remove test file: " + r.testFilePath); }

    r.ok = true;
    return r;
}
