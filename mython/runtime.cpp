#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>
#include <utility>
#include <algorithm>
#include <iostream>

using namespace std;

namespace runtime {

ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
    : data_(std::move(data)) {
}

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
}

ObjectHolder ObjectHolder::None() {
    return ObjectHolder();
}

Object& ObjectHolder::operator*() const {
    AssertIsValid();
    return *Get();
}

Object* ObjectHolder::operator->() const {
    AssertIsValid();
    return Get();
}

Object* ObjectHolder::Get() const {
    return data_.get();
}

ObjectHolder::operator bool() const {
    return Get() != nullptr;
}

bool IsTrue(const ObjectHolder& object) {
    if (!object) {
        return false;
    }
    if (auto number = dynamic_cast<Number*>(object.Get())) {
        return number->GetValue();
    } else if (auto str = dynamic_cast<String*>(object.Get())) {
        return !str->GetValue().empty();
    } else if (auto boolean = dynamic_cast<Bool*>(object.Get())) {
        return boolean->GetValue();
    }
    return false;
}

void ClassInstance::Print(std::ostream& os, Context& context) {
    if (HasMethod("__str__"s, 0)) {
        Call("__str__"s, {}, context)->Print(os, context);
    } else {
        os << this;
    }
}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    auto met = cls_.GetMethod(method);
    return met && met->formal_params.size() == argument_count;
}

Closure& ClassInstance::Fields() {
    return closure_;
}

const Closure& ClassInstance::Fields() const {
    return closure_;
}

ClassInstance::ClassInstance(const Class& cls)
: cls_(cls) {
}

ObjectHolder ClassInstance::Call(const std::string& method,
                                 const std::vector<ObjectHolder>& actual_args,
                                 Context& context) {
    auto met = cls_.GetMethod(method);
    if (!met || met->formal_params.size() != actual_args.size()) {
        throw std::runtime_error("Error in ClassInstance::Call: \""s + method + "\" method in Call was not found"s);
    }
    Closure closure{{"self"s, ObjectHolder::Share(*this)}};
    for (size_t i = 0; i < actual_args.size(); ++i) {
        closure[met->formal_params[i]] = actual_args[i];
    }
    return met->body->Execute(closure, context);
}

Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
: name_(move(name))
, methods_(move(methods))
, parent_(parent) {
    sort(methods_.begin(), methods_.end(),
         [](const Method& lhs, const Method& rhs) {
             return lhs.name > rhs.name;
         }
    );
}

const Method* Class::GetMethod(const std::string& name) const {
    const Method* method = [this, name]() {
        const auto it = lower_bound(methods_.begin(), methods_.end(), name,
            [](const Method& lhs, const string& rhs) {
                return lhs.name > rhs;
            }
        );
        return it != methods_.end() && it->name == name ? &*it : nullptr;
    }();
    return method ? method : parent_ ? parent_->GetMethod(name) : nullptr;
}

const std::string& Class::GetName() const {
    return name_;
}

void Class::Print(ostream& os, Context& /*context*/) {
    os << "Class "sv << name_;
}

void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    os << (GetValue() ? "True"sv : "False"sv);
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (!lhs || !rhs) {
        if (!lhs && !rhs) {
            return true;
        }
    } else if (auto cls_ins = dynamic_cast<ClassInstance*>(lhs.Get())) {
        if (cls_ins->HasMethod("__eq__"s, 1)) {
            return IsTrue(cls_ins->Call("__eq__"s, {rhs}, context));
        }
    } else if (auto number1 = dynamic_cast<Number*>(lhs.Get())) {
        if (auto number2 = dynamic_cast<Number*>(rhs.Get())) {
            return number1->GetValue() == number2->GetValue();
        }
    } else if (auto str1 = dynamic_cast<String*>(lhs.Get())) {
        if (auto str2 = dynamic_cast<String*>(rhs.Get())) {
            return str1->GetValue() == str2->GetValue();
        }
    } else if (auto bool1 = dynamic_cast<Bool*>(lhs.Get())) {
        if (auto bool2 = dynamic_cast<Bool*>(rhs.Get())) {
            return bool1->GetValue() == bool2->GetValue();
        }
    }
    throw std::runtime_error("Cannot compare objects for equality"s);
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (!lhs || !rhs) {
    } else if (auto cls_ins = dynamic_cast<ClassInstance*>(lhs.Get())) {
        if (cls_ins->HasMethod("__lt__"s, 1)) {
            return IsTrue(cls_ins->Call("__lt__"s, {rhs}, context));
        }
    } else if (auto number1 = dynamic_cast<Number*>(lhs.Get())) {
        if (auto number2 = dynamic_cast<Number*>(rhs.Get())) {
            return number1->GetValue() < number2->GetValue();
        }
    } else if (auto str1 = dynamic_cast<String*>(lhs.Get())) {
        if (auto str2 = dynamic_cast<String*>(rhs.Get())) {
            return str1->GetValue() < str2->GetValue();
        }
    } else if (auto bool1 = dynamic_cast<Bool*>(lhs.Get())) {
        if (auto bool2 = dynamic_cast<Bool*>(rhs.Get())) {
            return bool1->GetValue() < bool2->GetValue();
        }
    }
    throw std::runtime_error("Cannot compare objects for less"s);
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Equal(lhs, rhs, context);
}

bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context) && !Equal(lhs, rhs, context);
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return Less(lhs, rhs, context) || Equal(lhs, rhs, context);
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context);
}

}  // namespace runtime