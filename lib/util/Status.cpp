#include "Status.hpp"

namespace skv::util {

const char* Status::message() const noexcept {
    return message_.data();
}

Status Status::Ok() {
    return Status{Code::Ok, ""};
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
