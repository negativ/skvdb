#include "LogDevice.hpp"

#include <atomic>
#include <iostream>
#include <shared_mutex>
#include <thread>

#include <cassert>

#include "os/File.hpp"

namespace {
    constexpr std::size_t N_READERS = 17;
}

namespace skv::ondisk {

using namespace skv;
using namespace skv::util;

struct LogDevice::Impl {
    std::string path_;
    OpenOption openOption_;
    std::atomic<block_count_type> blocks_;
    bool opened_;
    os::File::Handle writeHandle_;
    buffer_type fillbuffer;
    std::array<os::File::Handle, N_READERS> readHandles_;
    std::array<std::mutex, N_READERS> readMutexes_;
    std::shared_mutex lock_;
};

LogDevice::LogDevice():
    impl_{std::make_unique<Impl>()}
{

}

LogDevice::~LogDevice() noexcept {
    close();
}

Status LogDevice::open(std::string_view path, LogDevice::OpenOption options) {
    assert(options.BlockSize % 512 == 0);

    std::unique_lock lock(impl_->lock_);

    const auto exists = [path]() {
        return os::File::open(path, "rb") != nullptr;
    }();

    if (!options.CreateNewIfNotExist && !exists)
        return Status::IOError("File not exists.");

    impl_->path_ = path;
    impl_->openOption_ = options;
    impl_->blocks_.store(0);
    impl_->opened_ = false;
    impl_->writeHandle_.reset();
    impl_->fillbuffer.resize(options.BlockSize);

    if (!exists && !createNew())
        return Status::IOError(std::string{"Unable to create block device at: "} + std::string{path});

    auto file = os::File::open(impl_->path_, "r+");
    impl_->writeHandle_.swap(file);

    if (!impl_->writeHandle_)
        return Status::IOError("Unable top open device for writing");

    impl_->opened_ = true;
    os::File::seek(impl_->writeHandle_, 0, os::File::Seek::End);

    impl_->blocks_.store(block_count_type(os::File::tell(impl_->writeHandle_) / blockSize()));

    if (!initReaders()) {
        close();

        return Status::IOError("Unable top open device for reading");
    }

    return Status::Ok();
}

Status LogDevice::close() {
    std::unique_lock lock(impl_->lock_);

    if (!opened())
        return Status::Ok();

    impl_->opened_ = false;
    impl_->writeHandle_.reset();

    for (std::size_t i = 0; i < N_READERS; ++i) {
        std::unique_lock lock(impl_->readMutexes_[i]);

        impl_->readHandles_[i].reset();
    }

    impl_->path_.clear();
    impl_->blocks_.store(0);

    buffer_type tmp;
    impl_->fillbuffer.swap(tmp);

    return Status::Ok();
}

std::tuple<Status, LogDevice::buffer_type> LogDevice::read(block_index_type n, bytes_count_type cnt) {
    static constexpr std::hash<std::thread::id> hasher;

    auto totalBlocks = impl_->blocks_.load();
    auto readBlocks = (cnt / blockSize()) + (cnt % blockSize()? 1 : 0);

    if (!opened())
        return {Status::IOError("Device not opened"), {}};
    if ((n + readBlocks) > totalBlocks)
        return {Status::InvalidArgument(""), {}};

    const auto readerId = hasher(std::this_thread::get_id()) % N_READERS;

    std::unique_lock lock(impl_->readMutexes_[readerId]);

    auto& fhandle = impl_->readHandles_[readerId];

    if (!fhandle)
        return {Status::IOError("Device not opened"), {}};

    buffer_type data(cnt);

    os::File::seek(fhandle, n * blockSize(), os::File::Seek::Set);
    os::File::read(data.data(), sizeof(buffer_type::value_type), cnt, fhandle);

    return {Status::Ok(), data};
}

std::tuple<Status, LogDevice::block_index_type, LogDevice::block_count_type> LogDevice::append(const buffer_type &buffer) {
    if (buffer.empty())
        return {Status::InvalidArgument("Unable to write empty buffer"), 0, 0};
    else if (!opened())
        return {Status::IOError("Device not opened"), 0, 0};

    std::unique_lock lock(impl_->lock_);

    auto& fhandle = impl_->writeHandle_;
    const auto cpos = os::File::tell(fhandle);
    const auto bufferSize = buffer.size();

    // TODO: check that write was successful and make rollback if neccessary
    assert(os::File::write(buffer.data(), sizeof(buffer_type::value_type), bufferSize, fhandle) == bufferSize);

    if ((bufferSize % blockSize()) != 0) {
        auto n = blockSize() - (bufferSize % blockSize());

        // TODO: check that write was successful and make rollback if neccessary
        assert(os::File::write(impl_->fillbuffer.data(), sizeof(buffer_type::value_type), n, fhandle) == n);
    }

    os::File::flush(impl_->writeHandle_);

    const auto spos = os::File::tell(fhandle);
    const auto blocksAdded = block_count_type(spos - cpos) / blockSize();

    impl_->blocks_.fetch_add(blocksAdded);

    return {Status::Ok(), cpos / blockSize(), blocksAdded};
}

uint64_t LogDevice::sizeInBytes() const noexcept {
    return uint64_t(impl_->openOption_.BlockSize) * sizeInBlocks();
}

LogDevice::block_count_type LogDevice::sizeInBlocks() const noexcept {
    return impl_->blocks_.load();
}

uint32_t LogDevice::blockSize() const noexcept {
    return impl_->openOption_.BlockSize;
}

bool LogDevice::opened() const noexcept {
    return impl_->opened_;
}

bool LogDevice::createNew() {
    return static_cast<bool>(os::File::open(impl_->path_, "w"));
}

bool LogDevice::initReaders() {
    for (size_t i = 0; i < N_READERS; ++i) {
        auto file = os::File::open(impl_->path_, "rb");
        impl_->readHandles_[i].swap(file);

        if (!impl_->readHandles_[i])
            return false; // calling function will close all opened handles
    }

    return true;
}

}
