#include "statement.h"

#include <iostream>
#include <sstream>
#include <cassert>

using namespace std;

namespace ast {

using runtime::Closure;
using runtime::Context;
using runtime::ObjectHolder;
    
using runtime::Class;
using runtime::ClassInstance;
using runtime::Number;
using runtime::String;
using runtime::Bool;

namespace {
const string ADD_METHOD = "__add__"s;
const string INIT_METHOD = "__init__"s;
}  // namespace

ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
    closure[var_] = rv_->Execute(closure, context);
    return closure[var_];
}

Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
: var_(move(var))
, rv_(move(rv)) {
}

VariableValue::VariableValue(const std::string& var_name)
: dotted_ids_({var_name}) {
}

VariableValue::VariableValue(std::vector<std::string> dotted_ids)
: dotted_ids_(move(dotted_ids)) {
    assert(dotted_ids_.size() > 0);
}

ObjectHolder VariableValue::Execute(Closure& closure, Context& /*context*/) {
    ObjectHolder result = [this, closure]() {
        auto it = closure.find(dotted_ids_[0]);
        if (it == closure.end()) {
            throw std::runtime_error("Error in VariableValue::Execute: \""s + dotted_ids_[0] + "\" field was not found"s);
        }
        return it->second;
    }();
    for (size_t i = 1; i < dotted_ids_.size(); ++i) {
        if (auto cls_ins = result.TryAs<ClassInstance>()) {
            auto it = cls_ins->Fields().find(dotted_ids_[i]);
            if (it == cls_ins->Fields().end()) {
                throw std::runtime_error("Error in VariableValue::Execute: \""s + dotted_ids_[i] + "\" field was not found"s);
            }
            result = it->second;
        } else {
            assert(false);
        }
    }
    return result;
}
    
Print::Print(const std::string* name)
: name_(name) {
}
    
unique_ptr<Print> Print::Variable(const std::string& name) {
    return unique_ptr<Print>(new Print(&name));
}

Print::Print(unique_ptr<Statement> argument) {
    args_.push_back(move(argument));
}

Print::Print(vector<unique_ptr<Statement>> args)
: args_(move(args)) {
}

ObjectHolder Print::Execute(Closure& closure, Context& context) {
    if (name_) {
        auto it = closure.find(*name_);
        if (it == closure.end()) {
            throw std::runtime_error("Error in Print::Execute"s);
        }
        it->second->Print(context.GetOutputStream(), context);
    } else if (args_.size() > 0) {
        ObjectHolder obj_h = args_[0]->Execute(closure, context);
        if (obj_h) {
            obj_h->Print(context.GetOutputStream(), context);
        } else {
            context.GetOutputStream() << "None"sv;
        }
        for (size_t i = 1; i < args_.size(); ++i) {
            context.GetOutputStream() << ' ';
            obj_h = args_[i]->Execute(closure, context);
            if (obj_h) {
                obj_h->Print(context.GetOutputStream(), context);
            } else {
                context.GetOutputStream() << "None"sv;
            }
        }
    }
    context.GetOutputStream() << '\n';
    return ObjectHolder::None();
}

MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
                       std::vector<std::unique_ptr<Statement>> args)
: object_(move(object))
, method_(move(method))
, args_(move(args)) {
}

ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
    vector<ObjectHolder> actual_args;
    actual_args.reserve(args_.size());
    for (auto& arg : args_) {
        actual_args.push_back(arg->Execute(closure, context));
    }
    return object_->Execute(closure, context).TryAs<ClassInstance>()->Call(method_, actual_args, context);
}

ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
    ostringstream os;
    ObjectHolder obj_h = argument_->Execute(closure, context);
    if (obj_h) {
        obj_h->Print(os, context);
    } else {
        os << "None"sv;
    }
    return ObjectHolder::Own(String(os.str()));
}

ObjectHolder Add::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs = lhs_->Execute(closure, context);
    ObjectHolder rhs = rhs_->Execute(closure, context);
    
    if (!lhs || !rhs) {
    } else if (auto cls_ins = lhs.TryAs<ClassInstance>()) {
        if (cls_ins->HasMethod(ADD_METHOD, 1)) {
            return cls_ins->Call(ADD_METHOD, {rhs}, context);
        }
    } else if (auto number1 = lhs.TryAs<Number>()) {
        if (auto number2 = rhs.TryAs<Number>()) {
            return ObjectHolder::Own(Number(number1->GetValue() + number2->GetValue()));
        }
    } else if (auto str1 = lhs.TryAs<String>()) {
        if (auto str2 = rhs.TryAs<String>()) {
            return ObjectHolder::Own(String(str1->GetValue() + str2->GetValue()));
        }
    } else if (auto bool1 = lhs.TryAs<Bool>()) {
        if (auto bool2 = rhs.TryAs<Bool>()) {
            return ObjectHolder::Own(Bool(bool1->GetValue() + bool2->GetValue()));
        }
    }
    throw std::runtime_error("Error in Add::Execute"s);
}

ObjectHolder Sub::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs = lhs_->Execute(closure, context);
    ObjectHolder rhs = rhs_->Execute(closure, context);
    
    if (!lhs || !rhs) {
    } else if (auto number1 = lhs.TryAs<Number>()) {
        if (auto number2 = rhs.TryAs<Number>()) {
            return ObjectHolder::Own(Number(number1->GetValue() - number2->GetValue()));
        }
    }
    throw std::runtime_error("Error in Sub::Execute"s);
}

ObjectHolder Mult::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs = lhs_->Execute(closure, context);
    ObjectHolder rhs = rhs_->Execute(closure, context);
    
    if (!lhs || !rhs) {
    } else if (auto number1 = lhs.TryAs<Number>()) {
        if (auto number2 = rhs.TryAs<Number>()) {
            return ObjectHolder::Own(Number(number1->GetValue() * number2->GetValue()));
        }
    }
    throw std::runtime_error("Error in Mult::Execute"s);
}

ObjectHolder Div::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs = lhs_->Execute(closure, context);
    ObjectHolder rhs = rhs_->Execute(closure, context);
    
    if (!lhs || !rhs) {
    } else if (auto number1 = lhs.TryAs<Number>()) {
        if (auto number2 = rhs.TryAs<Number>()) {
            return ObjectHolder::Own(Number(number1->GetValue() / number2->GetValue()));
        }
    }
    throw std::runtime_error("Error in Dir::Execute"s);
}

ObjectHolder Compound::Execute(Closure& closure, Context& context) {
    for (size_t i = 0; i < args_.size(); ++i) {
        args_[i]->Execute(closure, context);
    }
    return ObjectHolder::None();
}

Return::Return(std::unique_ptr<Statement> statement)
: statement_(move(statement)) {
}
    
ObjectHolder Return::Execute(Closure& closure, Context& context) {
    throw ObjectHolder(statement_->Execute(closure, context));
}

ClassDefinition::ClassDefinition(ObjectHolder cls)
: cls_(move(cls)) {
}

ObjectHolder ClassDefinition::Execute(Closure& closure, Context& /*context*/) {
    closure[cls_.TryAs<Class>()->GetName()] = cls_;
    return ObjectHolder::None();
}

FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
                                 std::unique_ptr<Statement> rv)
: object_(move(object))
, field_name_(move(field_name))
, rv_(move(rv)) {
}

ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
    if (auto cls_ins = object_.Execute(closure, context).TryAs<ClassInstance>()) {
        cls_ins->Fields()[field_name_] = rv_->Execute(closure, context);
        return cls_ins->Fields()[field_name_];
    }
    throw std::runtime_error("FieldAssignment::Execute: Error in FieldAssignment::Execute"s);
}

IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
               std::unique_ptr<Statement> else_body)
: condition_(move(condition))
, if_body_(move(if_body))
, else_body_(move(else_body)) {
}

ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
    return runtime::IsTrue(condition_->Execute(closure, context))
         ? if_body_->Execute(closure, context)
         : else_body_
             ? else_body_->Execute(closure, context)
             : ObjectHolder::None();
}

ObjectHolder Or::Execute(Closure& closure, Context& context) {
    return runtime::IsTrue(lhs_->Execute(closure, context)) ?
           ObjectHolder::Own(Bool(true)) :
           ObjectHolder::Own(Bool(runtime::IsTrue(rhs_->Execute(closure, context))));
}

ObjectHolder And::Execute(Closure& closure, Context& context) {
    return !runtime::IsTrue(lhs_->Execute(closure, context)) ?
           ObjectHolder::Own(Bool(false)) :
           ObjectHolder::Own(Bool(runtime::IsTrue(rhs_->Execute(closure, context))));
}

ObjectHolder Not::Execute(Closure& closure, Context& context) {
    return ObjectHolder::Own(Bool(!runtime::IsTrue(argument_->Execute(closure, context))));
}

Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
    : BinaryOperation(std::move(lhs), std::move(rhs))
    , comparator_(std::move(cmp)) {
}

ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
    return ObjectHolder::Own(Bool(comparator_(lhs_->Execute(closure, context), rhs_->Execute(closure, context), context)));
}

NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args)
: cls_ins_(class_)
, args_(move(args)) {
}

NewInstance::NewInstance(const runtime::Class& class_)
: cls_ins_(class_) {
}

ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
    if (cls_ins_.HasMethod(INIT_METHOD, args_.size())) {
        std::vector<ObjectHolder> objs;
        objs.reserve(args_.size());
        for (auto& arg : args_) {
            objs.push_back(arg->Execute(closure, context));
        }
        cls_ins_.Call(INIT_METHOD, objs, context);
    }
    return ObjectHolder::Share(cls_ins_);
}

MethodBody::MethodBody(std::unique_ptr<Statement>&& body)
: body_(move(body)) {
}

ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
    try {
        body_->Execute(closure, context);
    } catch (ObjectHolder obj_h) {
        return obj_h;
    }
    return ObjectHolder::None();
}

}  // namespace ast