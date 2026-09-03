#include "NullizedParamCheck.h"

#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Stmt.h>
#include <clang/AST/Type.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/ADT/StringRef.h>

#include <algorithm>
#include <string>

namespace clang::tidy::folios {
namespace {

enum class Direction {
    None,
    InOut,
};

enum class NullizedKind {
    None,
    Always,
    Success,
};

std::string normalizeAnnotation(llvm::StringRef Annotation)
{
    std::string Result;
    Result.reserve(Annotation.size());
    for (char Ch : Annotation) {
        if (Ch != '"') {
            Result.push_back(Ch);
        }
    }
    return Result;
}

std::string unscopedAnnotation(llvm::StringRef Annotation)
{
    std::string Normalized = normalizeAnnotation(Annotation);
    llvm::StringRef Ref(Normalized);
    if (Ref.starts_with("st_") || Ref.starts_with("vl_")) {
        Ref = Ref.drop_front(3);
    }
    return Ref.str();
}

Direction getDirection(const ParmVarDecl &Param)
{
    for (const auto *Attr : Param.specific_attrs<AnnotateAttr>()) {
        if (unscopedAnnotation(Attr->getAnnotation()) == "inout") {
            return Direction::InOut;
        }
    }
    return Direction::None;
}

NullizedKind getNullizedKind(const ParmVarDecl &Param)
{
    for (const auto *Attr : Param.specific_attrs<AnnotateAttr>()) {
        const std::string Annotation = unscopedAnnotation(Attr->getAnnotation());
        if (Annotation == "nullized") {
            return NullizedKind::Always;
        }
        if (Annotation == "success_nullized") {
            return NullizedKind::Success;
        }
    }
    return NullizedKind::None;
}

llvm::StringRef nullizedSpelling(NullizedKind Kind)
{
    switch (Kind) {
    case NullizedKind::Always:
        return "__nullized";
    case NullizedKind::Success:
        return "__success_nullized";
    case NullizedKind::None:
        break;
    }
    return "";
}

bool isPointerToPointer(QualType Type)
{
    Type = Type.getCanonicalType();
    const auto *Pointer = Type->getAs<PointerType>();
    if (!Pointer) {
        return false;
    }
    return Pointer->getPointeeType().getCanonicalType()->isPointerType();
}

bool isParamRef(const Expr *Expr, const ParmVarDecl *Param)
{
    if (!Expr || !Param) {
        return false;
    }

    Expr = Expr->IgnoreParenImpCasts();
    const auto *Ref = dyn_cast<DeclRefExpr>(Expr);
    return Ref && Ref->getDecl() == Param;
}

bool isZeroIndex(const Expr *Expr, ASTContext &Ctx)
{
    if (!Expr) {
        return false;
    }
    Expr = Expr->IgnoreParenImpCasts();
    return Expr->isIntegerConstantExpr(Ctx) && Expr->EvaluateKnownConstInt(Ctx).isZero();
}

bool isNullPointer(const Expr *Expr, ASTContext &Ctx)
{
    if (!Expr) {
        return false;
    }
    Expr = Expr->IgnoreParenImpCasts();
    return Expr->isNullPointerConstant(Ctx, Expr::NPC_ValueDependentIsNotNull) !=
        Expr::NPCK_NotNull;
}

bool isNullizedSlotWrite(const BinaryOperator *Op, const ParmVarDecl *Param, ASTContext &Ctx)
{
    if (!Op || !Op->isAssignmentOp() || !isNullPointer(Op->getRHS(), Ctx)) {
        return false;
    }

    const Expr *Lhs = Op->getLHS()->IgnoreParenImpCasts();
    if (const auto *Unary = dyn_cast<UnaryOperator>(Lhs);
        Unary && Unary->getOpcode() == UO_Deref && isParamRef(Unary->getSubExpr(), Param)) {
        return true;
    }

    if (const auto *Subscript = dyn_cast<ArraySubscriptExpr>(Lhs)) {
        return isParamRef(Subscript->getBase(), Param) && isZeroIndex(Subscript->getIdx(), Ctx);
    }

    return false;
}

class NullAssignmentVisitor : public RecursiveASTVisitor<NullAssignmentVisitor> {
public:
    NullAssignmentVisitor(const ParmVarDecl *Param, ASTContext &Ctx) : Param(Param), Ctx(Ctx) {}

    bool VisitBinaryOperator(BinaryOperator *Op)
    {
        if (isNullizedSlotWrite(Op, Param, Ctx)) {
            Found = true;
        }
        return !Found;
    }

    bool found() const
    {
        return Found;
    }

private:
    const ParmVarDecl *Param;
    ASTContext &Ctx;
    bool Found = false;
};

const ParmVarDecl *getAnnotatedParamFromRedecls(const FunctionDecl &Func, unsigned Index)
{
    for (const FunctionDecl *Redecl : Func.redecls()) {
        if (!Redecl || Redecl->getNumParams() <= Index) {
            continue;
        }
        const ParmVarDecl *Param = Redecl->getParamDecl(Index);
        if (Param && getNullizedKind(*Param) != NullizedKind::None) {
            return Param;
        }
    }
    return nullptr;
}

}  // namespace

NullizedParamCheck::NullizedParamCheck(llvm::StringRef Name, ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context)
{
}

void NullizedParamCheck::registerMatchers(ast_matchers::MatchFinder *Finder)
{
    using namespace ast_matchers;

    Finder->addMatcher(functionDecl(unless(isImplicit())).bind("function"), this);
    Finder->addMatcher(callExpr(callee(functionDecl())).bind("call"), this);
}

bool NullizedParamCheck::shouldDiagnose(
    const SourceManager &SM, SourceLocation Loc, llvm::StringRef Suffix
)
{
    if (Loc.isInvalid()) {
        return true;
    }

    Loc = SM.getExpansionLoc(Loc);
    if (Loc.isInvalid()) {
        return true;
    }

    std::string Key = SM.getFilename(Loc).str();
    Key += ':';
    Key += std::to_string(SM.getFileOffset(Loc));
    Key += ':';
    Key += Suffix.str();
    return Diagnosed.insert(Key).second;
}

void NullizedParamCheck::check(const ast_matchers::MatchFinder::MatchResult &Result)
{
    ASTContext &Ctx = *Result.Context;
    const SourceManager &SM = *Result.SourceManager;

    if (const auto *Func = Result.Nodes.getNodeAs<FunctionDecl>("function")) {
        for (const ParmVarDecl *Param : Func->parameters()) {
            if (!Param) {
                continue;
            }

            const NullizedKind Kind = getNullizedKind(*Param);
            if (Kind == NullizedKind::None) {
                continue;
            }

            if (getDirection(*Param) != Direction::InOut &&
                shouldDiagnose(SM, Param->getLocation(), "dir")) {
                diag(
                    Param->getLocation(),
                    "%0 parameter %1 should also be marked __inout"
                ) << nullizedSpelling(Kind) << Param->getName();
            }

            if (!isPointerToPointer(Param->getType()) &&
                shouldDiagnose(SM, Param->getLocation(), "type")) {
                diag(
                    Param->getLocation(),
                    "%0 parameter %1 should be a pointer to a pointer slot"
                ) << nullizedSpelling(Kind) << Param->getName();
            }

            if (!Func->isThisDeclarationADefinition() || !Func->hasBody()) {
                continue;
            }

            NullAssignmentVisitor Visitor(Param, Ctx);
            Visitor.TraverseStmt(Func->getBody());
            if (!Visitor.found() && shouldDiagnose(SM, Param->getLocation(), "write")) {
                diag(
                    Param->getLocation(),
                    "%0 parameter %1 is never assigned NULL through the parameter slot"
                ) << nullizedSpelling(Kind) << Param->getName();
            }
        }
        return;
    }

    const auto *Call = Result.Nodes.getNodeAs<CallExpr>("call");
    if (!Call) {
        return;
    }

    const FunctionDecl *Callee = Call->getDirectCallee();
    if (!Callee) {
        return;
    }

    const unsigned Count = std::min(Call->getNumArgs(), Callee->getNumParams());
    for (unsigned Index = 0; Index < Count; ++Index) {
        const ParmVarDecl *Param = getAnnotatedParamFromRedecls(*Callee, Index);
        if (!Param) {
            continue;
        }

        const Expr *Arg = Call->getArg(Index);
        if (!Arg || !isNullPointer(Arg, Ctx)) {
            continue;
        }

        const NullizedKind Kind = getNullizedKind(*Param);
        diag(
            Arg->getExprLoc(),
            "NULL cannot satisfy %0 parameter %1 because the callee must clear the pointed-to slot"
        ) << nullizedSpelling(Kind) << Param->getName();
    }
}

}  // namespace clang::tidy::folios
