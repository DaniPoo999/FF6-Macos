#include "ff6/io.hpp"
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

namespace ff6 {
namespace {
struct File {
    int descriptor;
    explicit File(int value) : descriptor(value) {}
    ~File() {
        if (descriptor >= 0)
            ::close(descriptor);
    }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
};
[[noreturn]] void io_error(const char* operation, const std::filesystem::path& path) {
    throw std::system_error(errno, std::generic_category(),
                            std::string(operation) + ": " + path.string());
}
} // namespace
Bytes read_file(const std::filesystem::path& path, std::size_t maximum) {
    // Nonblocking open lets us reject FIFOs without waiting for another process.
    // It has no effect on regular-file reads, which are the only accepted input.
    File file(::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK));
    if (file.descriptor < 0)
        io_error("Open", path);
    struct stat status{};
    if (::fstat(file.descriptor, &status) != 0)
        io_error("Stat", path);
    if (!S_ISREG(status.st_mode) || status.st_size < 0 ||
        static_cast<std::uint64_t>(status.st_size) > maximum)
        throw std::runtime_error("Input is not a bounded regular file: " + path.string());
    Bytes result(static_cast<std::size_t>(status.st_size));
    std::size_t position = 0;
    while (position < result.size()) {
        const auto count =
            ::read(file.descriptor, result.data() + position, result.size() - position);
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0)
            io_error("Read", path);
        if (count == 0)
            throw std::runtime_error("Input shrank while reading: " + path.string());
        position += static_cast<std::size_t>(count);
    }
    std::uint8_t extra{};
    ssize_t tail;
    do {
        tail = ::read(file.descriptor, &extra, 1);
    } while (tail < 0 && errno == EINTR);
    if (tail < 0)
        io_error("Read", path);
    if (tail != 0)
        throw std::runtime_error("Input grew while reading: " + path.string());
    return result;
}
void write_new_file(const std::filesystem::path& path, ByteView bytes) {
    auto pattern = path.string() + ".tmp.XXXXXX";
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    File file(::mkstemp(name.data()));
    if (file.descriptor < 0)
        io_error("Create temporary file", path);
    const std::filesystem::path temporary{name.data()};
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    } cleanup{temporary};
    std::size_t position = 0;
    while (position < bytes.size()) {
        const auto count =
            ::write(file.descriptor, bytes.data() + position, bytes.size() - position);
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0)
            io_error("Write", path);
        if (count == 0)
            throw std::runtime_error("Write made no progress");
        position += static_cast<std::size_t>(count);
    }
    if (::fsync(file.descriptor) != 0)
        io_error("Sync", path);
    // Same-directory hard-link publication is atomic and fails if path exists,
    // including a dangling symlink. No overwrite race between check and rename.
    if (::link(temporary.c_str(), path.c_str()) != 0)
        io_error("Publish (destination must not exist)", path);
}
} // namespace ff6
