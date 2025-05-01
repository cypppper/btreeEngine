#pragma once

#include <utility>
#include <exception>
#include <variant>
#include <string>
#include <memory>

namespace visitor{
template<class... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};
template<class... Ts> overload(Ts...) -> overload<Ts...>;


struct StatusOrVisitor {
    template<typename T>
    static bool Ok(std::variant<T, std::unique_ptr<std::exception>>& var) {
        std::visit(visitor::overload{
            [](T& t) { return true; },
            [](std::unique_ptr<std::exception> except) { return false; }
        }, var);
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

class DoSplitException : public std::exception {
    private:
        static const char* message;
    public:
        DoSplitException() {}
    
        virtual const char* what() const noexcept override {
            return message;
        }
    };
    
    

class StatusBase{
    virtual bool Ok() = 0;
};

template<typename T> 
class StatusOr: StatusBase {
public:
    StatusOr(T&& _res): result(std::forward(_res)) {}
    StatusOr(std::unique_ptr<std::exception>&& _e): result(std::move(_e)) {}
    bool Ok() override {
        return visitor::StatusOrVisitor::Ok(&this->result);
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