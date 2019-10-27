 #include "Status.hpp"

namespace skv::util {

Status Status::Ok() noexcept(noexcept_copy_constructible) {
    return Status{Status::Code::Ok, ""};
}

Status Status::IOError(std::string message) noexcept(noexcept_copy_constructible) {
    return Status{Status::Code::IOError, std::move(message)};
}

Status Status::InvalidArgument(std::string message) noexcept(noexcept_copy_constructible) {
    return Status{Status::Code::InvalidArgument, std::move(message)};
}

Status Status::NotFound(std::string message) noexcept(noexcept_copy_constructible) {
    return Status{Status::Code::NotFound, std::move(message)};
}

Status Status::Fatal(std::string message) noexcept(noexcept_copy_constructible) {
    return Status{Status::Code::Fatal, std::move(message)};
}


Status::Status(Status::Code code, std::string message) noexcept(noexcept_move_constructible):
    code_{code},
    message_{std::move(message)}
{
    // Do nothing
}

std::string Status::message() const noexcept(std::is_nothrow_copy_constructible_v<decltype(message_)>) {
    return message_;
}

Status::Code Status::code() const noexcept {
    return code_;
}

bool Status::isOk() const noexcept {
    return code_ == Code::Ok;
}

bool Status::isIOError() const noexcept {
    return code_ == Code::IOError;
}

bool Status::isInvalidArgument() const noexcept {
    return code_ == Code::InvalidArgument;
}

bool Status::isNotFound() const noexcept {
    return code_ == Code::NotFound;
}

bool Status::isFatal() const noexcept {
    return code_ == Code::Fatal;
}

}
