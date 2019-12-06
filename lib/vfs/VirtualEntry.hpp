#pragma once

#include "vfs/IEntry.hpp"
#include "vfs/IVolume.hpp"
#include "util/Log.hpp"
#include "util/Status.hpp"
#include "util/ThreadPool.hpp"

namespace skv::vfs {

using ThreadPool = util::ThreadPool<>;

class VirtualEntry final : public vfs::IEntry {
public:
    using Entries = std::vector<std::shared_ptr<IEntry>>;
    using Volumes = std::vector<IVolumePtr>;

    VirtualEntry(Handle handle, Entries&& entries, Volumes&& volumes, ThreadPool& threadPool);
    ~VirtualEntry() noexcept override = default;

    VirtualEntry(const VirtualEntry&) = delete;
    VirtualEntry& operator=(const VirtualEntry&) = delete;


    Handle handle() const noexcept override;


    std::tuple<Status, bool> hasProperty(const std::string& prop) const noexcept override;

    Status setProperty(const std::string& prop, const Property& value) override;

    std::tuple<Status, Property> property(const std::string& prop) const override;

    Status removeProperty(const std::string& prop) override;

    std::tuple<Status, Properties> properties() const override;

    std::tuple<Status, std::set<std::string>> propertiesNames() const override;


    Status expireProperty(const std::string& prop, chrono::milliseconds ms) override;

    Status cancelPropertyExpiration(const std::string& prop) override;


    std::tuple<Status, std::set<std::string>> children() const override;

    Volumes& volumes() const noexcept;

    Entries& entries() const noexcept;

private:
    static constexpr const char* const TAG = "vfs::VirtualEntry";

    template <typename Iterator>
    void waitAllFutures(Iterator start, Iterator stop) const  {
        using namespace std::literals;

        while (!std::all_of(start, stop,
                            [](auto& f) { return (f.wait_for(0ms) == std::future_status::ready); }))
            threadPool_.get().throttle(); // helping thread pool to do his work
    }

    template <typename F, typename ... Args>
    auto forEachEntry(F&& func, Args&& ... args) const {
        using namespace std::literals;
        using result      = std::invoke_result_t<F, IEntry*, Args...>;
        using future      = std::future<result>;
        using future_list = std::vector<future>;
        using result_list = std::vector<result>;

        auto start = std::begin(entries_),
             stop  = std::end(entries_);

        if (start == stop)
            return std::make_tuple(Status::Ok(), result_list{});

        result_list results;
        future_list futures;

        auto it = start;

        std::advance(start, 1); // first call in current thread context

        while (start != stop) {
            auto& entry = (*start);

            try {
                futures.emplace_back(threadPool_.get().schedule(std::forward<F>(func), entry.get(), std::forward<Args>(args)...));
            }
            catch (...) {
                return std::make_tuple(Status::Fatal("Exception"), result_list{});
            }

            ++start;
        }

        try {
            results.emplace_back(std::invoke(std::forward<F>(func), (*it).get(), std::forward<Args>(args)...));
        }
        catch (...) {
            return std::make_tuple(Status::Fatal("Exception"), result_list{});
        }

        waitAllFutures(std::begin(futures), std::end(futures));

        for (auto& f : futures) {
            try {
                results.emplace_back(f.get());
            }
            catch (...) {
                return std::make_tuple(Status::Fatal("Exception"), result_list{});
            }
        }

        return std::make_tuple(Status::Ok(), results);
    }

    Handle handle_;
    mutable Entries entries_;
    mutable Volumes volumes_;
    mutable std::reference_wrapper<ThreadPool> threadPool_;
};

}
