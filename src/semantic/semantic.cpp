#include "semantic/semantic.hpp"
#include <utility>

namespace dlang {
static bool numeric(TypeKind kind) {
  return kind == TypeKind::Int || kind == TypeKind::Long || kind == TypeKind::Float ||
         kind == TypeKind::Double || kind == TypeKind::Char;
}
static std::string typeName(Type type) { return type.name.empty() ? "unknown" : type.name; }
static bool compatible(Type expected, Type actual) {
  if (expected == actual)
    return true;
  if (expected.kind == TypeKind::String && actual.kind == TypeKind::String)
    return true;
  return (expected.kind == TypeKind::Float || expected.kind == TypeKind::Double) &&
         (actual.kind == TypeKind::Float || actual.kind == TypeKind::Double);
}
void SemanticAnalyzer::error(const SourceLocation& location, std::string message) {
  diagnostics_.push_back({Severity::Error, location, std::move(message)});
}
Type SemanticAnalyzer::lookup(const std::string& name, Scope& scope) {
  for (auto* current = &scope; current; current = current->parent) {
    auto found = current->values.find(name);
    if (found != current->values.end())
      return found->second;
  }
  return {};
}
Type SemanticAnalyzer::memberType(const MemberExpr& member, Scope& scope,
                                  const SourceLocation& location) {
  Type object = expressionType(*member.object, scope);
  if (object.kind != TypeKind::Struct) {
    error(location, "member access requires a struct value");
    return {};
  }
  auto declaration = structs_.find(object.name);
  if (declaration == structs_.end()) {
    error(location, "unknown struct type '" + object.name + "'");
    return {};
  }
  for (const auto& field : declaration->second->members)
    if (field.name == member.member)
      return field.type;
  error(location, "struct '" + object.name + "' has no member '" + member.member + "'");
  return {};
}
Type SemanticAnalyzer::expressionType(const Expr& expression, Scope& scope) {
  return std::visit(
      [&](const auto& value) -> Type {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, Literal>) {
          if (value.kind == TokenKind::True || value.kind == TokenKind::False)
            return {TypeKind::Bool, "bool"};
          if (value.kind == TokenKind::CharLiteral)
            return {TypeKind::Char, "char"};
          if (value.kind == TokenKind::FloatLiteral)
            return {TypeKind::Double, "double"};
          if (value.kind == TokenKind::StringLiteral)
            return {TypeKind::String, ""};
          return {TypeKind::Int, "int"};
        }
        if constexpr (std::is_same_v<Value, NameExpr>) {
          Type type = lookup(value.name, scope);
          if (type.kind == TypeKind::Unknown)
            error(expression.location, "undefined identifier '" + value.name + "'");
          return type;
        }
        if constexpr (std::is_same_v<Value, MemberExpr>)
          return memberType(value, scope, expression.location);
        if constexpr (std::is_same_v<Value, UnaryExpr>) {
          Type type = expressionType(*value.operand, scope);
          if (value.op == TokenKind::Bang) {
            if (type.kind != TypeKind::Bool)
              error(expression.location, "operator ! requires bool");
            return {TypeKind::Bool, "bool"};
          }
          if (!numeric(type.kind))
            error(expression.location, "unary operator requires numeric operand");
          return type;
        }
        if constexpr (std::is_same_v<Value, BinaryExpr>) {
          Type left = expressionType(*value.left, scope),
               right = expressionType(*value.right, scope);
          if (value.op == TokenKind::Equal) {
            if (const auto* name = std::get_if<NameExpr>(&value.left->value)) {
              if (lookup(name->name, scope).kind == TypeKind::Unknown)
                error(value.left->location, "undefined identifier '" + name->name + "'");
            } else if (!std::holds_alternative<MemberExpr>(value.left->value))
              error(expression.location, "left side of assignment must be an identifier");
            if (!compatible(left, right) && left.kind != TypeKind::Unknown &&
                right.kind != TypeKind::Unknown)
              error(expression.location,
                    "cannot assign '" + typeName(right) + "' to '" + typeName(left) + "'");
            return left;
          }
          if (value.op == TokenKind::AmpAmp || value.op == TokenKind::PipePipe) {
            if (left.kind != TypeKind::Bool || right.kind != TypeKind::Bool)
              error(expression.location, "logical operators require bool operands");
            return {TypeKind::Bool, "bool"};
          }
          if (value.op == TokenKind::EqualEqual || value.op == TokenKind::BangEqual ||
              value.op == TokenKind::Less || value.op == TokenKind::LessEqual ||
              value.op == TokenKind::Greater || value.op == TokenKind::GreaterEqual) {
            if (!compatible(left, right))
              error(expression.location, "comparison operands must have matching types");
            return {TypeKind::Bool, "bool"};
          }
          if (!numeric(left.kind) || !numeric(right.kind))
            error(expression.location, "arithmetic operands must be numeric");
          return left;
        }
        if constexpr (std::is_same_v<Value, CallExpr>) {
          auto found = functions_.find(value.callee);
          if (found == functions_.end()) {
            error(expression.location, "undefined function '" + value.callee + "'");
            return {};
          }
          if (value.arguments.size() != found->second.parameters.size())
            error(expression.location, "wrong number of arguments to '" + value.callee + "'");
          else
            for (size_t i = 0; i < value.arguments.size(); ++i) {
              Type actual = expressionType(*value.arguments[i], scope);
              if (!compatible(found->second.parameters[i], actual))
                error(value.arguments[i]->location, "argument type does not match parameter");
            }
          return found->second.returnType;
        }
      },
      expression.value);
}
bool SemanticAnalyzer::statement(const Stmt& statementNode, Scope& scope, const Type& returnType,
                                 bool inLoop) {
  return std::visit(
      [&](const auto& value) -> bool {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, BlockStmt>) {
          Scope nested{{}, &scope};
          bool ok = true;
          for (const auto& child : value.statements)
            ok = statement(*child, nested, returnType, inLoop) && ok;
          return ok;
        }
        if constexpr (std::is_same_v<Value, VarDeclStmt>) {
          if (scope.values.contains(value.name))
            error(statementNode.location, "duplicate declaration '" + value.name + "'");
          if (value.type.kind == TypeKind::String && value.type.name.empty())
            value.type.name = "string";
          if (value.initializer) {
            Type actual = expressionType(*value.initializer, scope);
            if (!compatible(value.type, actual))
              error(statementNode.location,
                    "cannot initialize '" + value.name + "' with incompatible type");
          }
          if (value.type.kind == TypeKind::Struct && !structs_.contains(value.type.name))
            error(statementNode.location, "unknown struct type '" + value.type.name + "'");
          scope.values[value.name] = value.type;
          return true;
        }
        if constexpr (std::is_same_v<Value, ExprStmt>) {
          expressionType(*value.expression, scope);
          return true;
        }
        if constexpr (std::is_same_v<Value, ReturnStmt>) {
          Type actual = value.expression ? expressionType(*value.expression, scope)
                                         : Type{TypeKind::Void, "void"};
          if (!compatible(returnType, actual))
            error(statementNode.location, "return type does not match function return type");
          return true;
        }
        if constexpr (std::is_same_v<Value, IfStmt>) {
          Type condition = expressionType(*value.condition, scope);
          if (condition.kind != TypeKind::Bool)
            error(value.condition->location, "if condition must be bool");
          statement(*value.thenBranch, scope, returnType, inLoop);
          if (value.elseBranch)
            statement(*value.elseBranch, scope, returnType, inLoop);
          return true;
        }
        if constexpr (std::is_same_v<Value, WhileStmt>) {
          Type condition = expressionType(*value.condition, scope);
          if (condition.kind != TypeKind::Bool)
            error(value.condition->location, "while condition must be bool");
          statement(*value.body, scope, returnType, true);
          return true;
        }
        if constexpr (std::is_same_v<Value, ForStmt>) {
          Scope loopScope{{}, &scope};
          if (value.initialization)
            statement(*value.initialization, loopScope, returnType, inLoop);
          if (value.condition) {
            Type condition = expressionType(*value.condition, loopScope);
            if (condition.kind != TypeKind::Bool)
              error(value.condition->location, "for condition must be bool");
          }
          if (value.increment)
            expressionType(*value.increment, loopScope);
          statement(*value.body, loopScope, returnType, true);
          return true;
        }
        if constexpr (std::is_same_v<Value, BreakStmt> || std::is_same_v<Value, ContinueStmt>) {
          if (!inLoop)
            error(statementNode.location, "loop control statement outside a loop");
          return true;
        }
      },
      statementNode.value);
}
bool SemanticAnalyzer::analyze(const Module& module) {
  for (const auto& declaration : module.structs) {
    if (structs_.contains(declaration.name))
      error(declaration.location, "duplicate declaration '" + declaration.name + "'");
    structs_[declaration.name] = &declaration;
    std::unordered_map<std::string, bool> members;
    for (const auto& member : declaration.members) {
      if (members.contains(member.name))
        error(member.location, "duplicate struct member '" + member.name + "'");
      members[member.name] = true;
    }
  }
  for (const auto& function : module.functions) {
    if (functions_.contains(function.name))
      error(function.location, "duplicate declaration '" + function.name + "'");
    functions_[function.name] = {function.returnType, {}};
    for (const auto& parameter : function.parameters)
      functions_[function.name].parameters.push_back(parameter.type);
  }
  for (const auto& function : module.functions) {
    Scope scope;
    for (const auto& parameter : function.parameters) {
      if (scope.values.contains(parameter.name))
        error(parameter.location, "duplicate parameter '" + parameter.name + "'");
      scope.values[parameter.name] = parameter.type;
    }
    statement(*function.body, scope, function.returnType, false);
  }
  return diagnostics_.empty();
}
} // namespace dlang
