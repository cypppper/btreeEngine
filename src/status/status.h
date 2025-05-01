#pragma once

#include <stdexcept>
#include <utility>
#include <exception>
#include <variant>
#include <string>
#include <memory>
#include <format>

namespace visitor{
template<class... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};
template<class... Ts> overload(Ts...) -> overload<Ts...>;


struct StatusOrVisitor {
    template<typename T>
    static bool Ok(std::variant<T, std::unique_ptr<std::exception>>& var) {
        auto ret = std::visit(visitor::overload{
            [](const T& t) { return true; },
            [](const std::unique_ptr<std::exception>& except) { return false; }
        }, var);

        return ret;
    }
};

}

class OutofSpaceException : public std::exception {
private:
    static const char* message;
public:
    OutofSpaceException() {}

    virtual const char* what() const noexcept override {
        return message;
    }
};

class KeyNotFoundException : public std::exception {
private:
    static const char* message;
public:
    KeyNotFoundException() {}

    virtual const char* what() const noexcept override {
        return message;
    }
};

class KeyDuplicateException : public std::exception {
private:
    static const char* message;
public:
    KeyDuplicateException() {}

    virtual const char* what() const noexcept override {
        return message;
    }
};

    
template<typename T>
class StatusBase{
    virtual bool Ok() = 0;
    virtual T unwrap() = 0;
};

template<typename T> 
class StatusOr: StatusBase<T> {
public:
    StatusOr(const T& _res): result(_res) {}
    StatusOr(std::unique_ptr<std::exception>&& _e): result(std::move(_e)) {}
    bool Ok() override {
        return visitor::StatusOrVisitor::Ok(this->result);
    }
    T unwrap() override {
        if (Ok()) {
            return std::get<T>(this->result);
        } else {
            throw std::runtime_error(std::string(std::format("Exception: {}", std::get<std::unique_ptr<std::exception>>(this->result)->what())));
        }
    }
    
private:
    std::variant<T, std::unique_ptr<std::exception>> result;
};

template <>
class StatusOr<void> {
public:
    StatusOr() {}
    StatusOr(std::unique_ptr<std::exception>&& _e): result(std::move(_e)) {}
    bool Ok() {return result.get() == nullptr;}
private:
    std::unique_ptr<std::exception> result;
};

template<typename T>
std::unique_ptr<std::exception> make_exception() {
    return std::make_unique<T>(T{});
}

using Status = StatusOr<void>;