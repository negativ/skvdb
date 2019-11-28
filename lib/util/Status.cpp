#include "Status.hpp"

namespace skv::util {

Status Status::Ok() {
    return Status{Status::Code::Ok, ""};
}

Status Status::IOError(const char* message) {
    return Status{Status::Code::IOError, message};
}

Status Status::InvalidArgument(const char* message) {
    return Status{Status::Code::InvalidArgument, message};
}

Status Status::NotFound(const char* message) {
    return Status{Status::Code::NotFound, message};
}

Status Status::Fatal(const char* message) {
    return Status{Status::Code::Fatal, message};
}

Status Status::InvalidOperation(const char* message) {
    return Status{Status::Code::InvalidOp, message};
}


Status::Status(Status::Code code, const char* message) noexcept:
    code_{code},
    message_{message}
{
    // Do nothing
}

const char* Status::message() const noexcept {
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

bool Status::isInvalidOperation() const noexcept {
    return code_ == Code::InvalidOp;
}

}
