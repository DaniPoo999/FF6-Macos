#include "ff6/io.hpp"
#include "ff6/saved_game.hpp"
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <system_error>
#include <unistd.h>
namespace ff6 {
namespace {
[[noreturn]] void fail(const char* operation) {
    throw std::system_error(errno, std::generic_category(), operation);
}
struct File {
    int fd = -1;
    explicit File(int value) : fd(value) {
        if (fd < 0)
            fail("Open save file");
    }
    ~File() {
        if (fd >= 0)
            ::close(fd);
    }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
};
std::filesystem::path slot_path(const std::filesystem::path& folder, unsigned slot) {
    if (slot >= 3)
        throw std::out_of_range("Save slot must be 1–3");
    return folder / ("slot-" + std::to_string(slot + 1) + ".ff6save");
}
void publish(const std::filesystem::path& destination, ByteView bytes) {
    auto name = destination.string() + ".tmp-XXXXXX";
    File file(::mkstemp(name.data()));
    try {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto count = ::write(file.fd, bytes.data() + offset, bytes.size() - offset);
            if (count < 0 && errno == EINTR)
                continue;
            if (count < 0)
                fail("Write save file");
            if (count == 0)
                throw std::runtime_error("Save write made no progress");
            offset += static_cast<std::size_t>(count);
        }
        if (::fsync(file.fd) < 0)
            fail("Flush save file");
        if (::rename(name.c_str(), destination.c_str()) < 0)
            fail("Publish save file");
    } catch (...) {
        ::unlink(name.c_str());
        throw;
    }
}
} // namespace
SaveStore::SaveStore(std::filesystem::path folder) : folder_(std::move(folder)) {
    if (folder_.empty())
        throw std::invalid_argument("Save folder is empty");
}
std::array<SaveSlot, 3> SaveStore::read() const {
    std::array<SaveSlot, 3> result;
    for (unsigned i = 0; i < 3; ++i) {
        const auto path = slot_path(folder_, i);
        auto& slot = result[i];
        for (unsigned version = 0; version < 2; ++version) {
            const auto candidate =
                version ? std::filesystem::path(path.string() + ".previous") : path;
            try {
                if (!std::filesystem::exists(candidate))
                    continue;
                slot.game = decode_saved_game(read_file(candidate, 16384));
                slot.recovered = version != 0;
                break;
            } catch (const std::exception& e) {
                if (slot.error.empty())
                    slot.error = e.what();
            }
        }
    }
    return result;
}
void SaveStore::write(unsigned slot, const SavedGame& game) const {
    const auto path = slot_path(folder_, slot);
    const auto bytes = encode_saved_game(game);
    std::filesystem::create_directories(folder_);
    // Concurrent app instances serialize publication; readers always observe a
    // complete old or new file. An invalid current file never replaces a good backup.
    File lock(::open((folder_ / ".save-lock").c_str(), O_CREAT | O_RDWR, 0600));
    if (::flock(lock.fd, LOCK_EX) < 0)
        fail("Lock save folder");
    std::optional<Bytes> previous;
    if (std::filesystem::exists(path)) {
        auto candidate = read_file(path, 16384);
        try {
            decode_saved_game(candidate);
            previous = std::move(candidate);
        } catch (const std::invalid_argument&) {
        }
    }
    if (previous)
        publish(path.string() + ".previous", *previous);
    publish(path, bytes);
    File directory(::open(folder_.c_str(), O_RDONLY));
    if (::fsync(directory.fd) < 0 && errno != EINVAL)
        fail("Flush save folder");
}
} // namespace ff6
