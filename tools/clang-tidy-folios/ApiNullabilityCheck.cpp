#include "ApiNullabilityCheck.h"

#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Stmt.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Lex/Lexer.h>
#include <llvm/ADT/StringRef.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace clang::tidy::folios {
namespace {

enum class Direction {
    None,
    In,
    Out,
    InOut,
    OutOptional,
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

Direction getDirection(const ParmVarDecl &Param)
{
    for (const auto *Attr : Param.specific_attrs<AnnotateAttr>()) {
        const std::string Annotation = normalizeAnnotation(Attr->getAnnotation());
        if (Annotation == "in" || Annotation == "st_in" || Annotation == "vl_in") {
            return Direction::In;
        }
        if (Annotation == "out" || Annotation == "st_out" || Annotation == "vl_out") {
            return Direction::Out;
        }
        if (Annotation == "inout" || Annotation == "st_inout" || Annotation == "vl_inout") {
            return Direction::InOut;
        }
        if (Annotation == "out_optional" || Annotation == "st_out_optional" ||
            Annotation == "vl_out_optional") {
            return Direction::OutOptional;
        }
    }
    return Direction::None;
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

bool isNullPointer(const Expr *Expr, ASTContext &Ctx)
{
    if (!Expr) {
        return false;
    }
    Expr = Expr->IgnoreParenImpCasts();
    return Expr->isNullPointerConstant(Ctx, Expr::NPC_ValueDependentIsNotNull) !=
        Expr::NPCK_NotNull;
}

bool isNullCheckExpr(const Expr *Expr, const ParmVarDecl *Param, ASTContext &Ctx)
{
    if (!Expr) {
        return false;
    }

    Expr = Expr->IgnoreParenImpCasts();
    if (isParamRef(Expr, Param)) {
        return true;
    }

    if (const auto *Unary = dyn_cast<UnaryOperator>(Expr);
        Unary && Unary->getOpcode() == UO_LNot && isParamRef(Unary->getSubExpr(), Param)) {
        return true;
    }

    const auto *Binary = dyn_cast<BinaryOperator>(Expr);
    if (!Binary) {
        return false;
    }

    if (Binary->getOpcode() == BO_LAnd || Binary->getOpcode() == BO_LOr) {
        return isNullCheckExpr(Binary->getLHS(), Param, Ctx) ||
            isNullCheckExpr(Binary->getRHS(), Param, Ctx);
    }

    if (!(Binary->getOpcode() == BO_EQ || Binary->getOpcode() == BO_NE)) {
        return false;
    }

    return (isParamRef(Binary->getLHS(), Param) && isNullPointer(Binary->getRHS(), Ctx)) ||
        (isParamRef(Binary->getRHS(), Param) && isNullPointer(Binary->getLHS(), Ctx));
}

class NullCheckVisitor : public RecursiveASTVisitor<NullCheckVisitor> {
public:
    NullCheckVisitor(const ParmVarDecl *Param, ASTContext &Ctx) : Param(Param), Ctx(Ctx) {}

    bool VisitIfStmt(IfStmt *Stmt)
    {
        if (isNullCheckExpr(Stmt->getCond(), Param, Ctx)) {
            NullChecks.push_back(Stmt->getIfLoc());
        }
        return true;
    }

    const std::vector<SourceLocation> &getNullChecks() const
    {
        return NullChecks;
    }

private:
    const ParmVarDecl *Param;
    ASTContext &Ctx;
    std::vector<SourceLocation> NullChecks;
};

class ParamDerefVisitor : public RecursiveASTVisitor<ParamDerefVisitor> {
public:
    explicit ParamDerefVisitor(const ParmVarDecl *Param) : Param(Param) {}

    bool VisitUnaryOperator(UnaryOperator *Expr)
    {
        if (Expr->getOpcode() == UO_Deref && isParamRef(Expr->getSubExpr(), Param)) {
            Derefs.push_back(Expr->getOperatorLoc());
        }
        return true;
    }

    bool VisitMemberExpr(MemberExpr *Expr)
    {
        if (Expr->isArrow() && isParamRef(Expr->getBase(), Param)) {
            Derefs.push_back(Expr->getOperatorLoc());
        }
        return true;
    }

    bool VisitArraySubscriptExpr(ArraySubscriptExpr *Expr)
    {
        if (isParamRef(Expr->getBase(), Param)) {
            Derefs.push_back(Expr->getExprLoc());
        }
        return true;
    }

    const std::vector<SourceLocation> &getDerefs() const
    {
        return Derefs;
    }

private:
    const ParmVarDecl *Param;
    std::vector<SourceLocation> Derefs;
};

std::string getStatementText(const Stmt *Stmt, const SourceManager &SM, const LangOptions &LangOpts)
{
    if (!Stmt) {
        return {};
    }

    CharSourceRange Range = CharSourceRange::getTokenRange(Stmt->getSourceRange());
    return Lexer::getSourceText(Range, SM, LangOpts).str();
}

bool statementLooksLikeAssertForParam(
    const Stmt *Stmt,
    llvm::StringRef ParamName,
    const SourceManager &SM,
    const LangOptions &LangOpts
)
{
    if (ParamName.empty()) {
        return false;
    }

    const std::string Text = getStatementText(Stmt, SM, LangOpts);
    const std::string Name = ParamName.str();
    if (Text.find("assert") == std::string::npos) {
        return false;
    }

    size_t Pos = Text.find(Name);
    while (Pos != std::string::npos) {
        const bool StartsAtIdentifierBoundary =
            Pos == 0 ||
            (!std::isalnum(static_cast<unsigned char>(Text[Pos - 1])) && Text[Pos - 1] != '_');
        const size_t End = Pos + Name.size();
        const bool EndsAtIdentifierBoundary =
            End >= Text.size() ||
            (!std::isalnum(static_cast<unsigned char>(Text[End])) && Text[End] != '_');
        if (StartsAtIdentifierBoundary && EndsAtIdentifierBoundary) {
            return true;
        }

        Pos = Text.find(Name, Pos + 1);
    }

    return false;
}

bool hasEntryAssert(
    const CompoundStmt *Body,
    llvm::StringRef ParamName,
    unsigned EntryStatementLimit,
    const SourceManager &SM,
    const LangOptions &LangOpts
)
{
    if (!Body) {
        return false;
    }

    unsigned Checked = 0;
    for (const Stmt *Stmt : Body->body()) {
        if (Checked++ >= EntryStatementLimit) {
            break;
        }
        if (statementLooksLikeAssertForParam(Stmt, ParamName, SM, LangOpts)) {
            return true;
        }
    }

    return false;
}

const CallExpr *getSingleReturnCall(const CompoundStmt *Body)
{
    if (!Body) {
        return nullptr;
    }

    const Stmt *OnlyStmt = nullptr;
    for (const Stmt *Stmt : Body->body()) {
        if (!Stmt) {
            continue;
        }
        if (OnlyStmt) {
            return nullptr;
        }
        OnlyStmt = Stmt;
    }

    const auto *Return = dyn_cast_or_null<ReturnStmt>(OnlyStmt);
    if (!Return || !Return->getRetValue()) {
        return nullptr;
    }

    return dyn_cast<CallExpr>(Return->getRetValue()->IgnoreParenImpCasts());
}

bool delegatesRequiredOutputParam(const CompoundStmt *Body, const ParmVarDecl *Param)
{
    const CallExpr *Call = getSingleReturnCall(Body);
    if (!Call || !Param) {
        return false;
    }

    const FunctionDecl *Callee = Call->getDirectCallee();
    if (!Callee) {
        return false;
    }

    const unsigned ArgCount = Call->getNumArgs();
    const unsigned ParamCount = Callee->getNumParams();
    const unsigned Count = std::min(ArgCount, ParamCount);

    for (unsigned Index = 0; Index < Count; ++Index) {
        if (!isParamRef(Call->getArg(Index), Param)) {
            continue;
        }

        const Direction CalleeDir = getDirection(*Callee->getParamDecl(Index));
        if (CalleeDir == Direction::Out || CalleeDir == Direction::InOut) {
            return true;
        }
    }

    return false;
}

bool isPointerLikeParameter(const ParmVarDecl &Param)
{
    QualType Type = Param.getType();
    if (Type.isNull()) {
        return false;
    }

    return Type.getCanonicalType()->isPointerType();
}

bool isFoliosPublicFunction(const FunctionDecl &Func)
{
    const IdentifierInfo *Identifier = Func.getIdentifier();
    if (!Identifier || !Func.isExternallyVisible()) {
        return false;
    }

    const llvm::StringRef Name = Identifier->getName();
    return Name.starts_with("St") || Name.starts_with("Vl");
}

}  // namespace

ApiNullabilityCheck::ApiNullabilityCheck(llvm::StringRef Name, ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context), EntryStatementLimit(Options.get("EntryStatementLimit", 8U))
{
}

void ApiNullabilityCheck::storeOptions(ClangTidyOptions::OptionMap &Opts)
{
    Options.store(Opts, "EntryStatementLimit", EntryStatementLimit);
}

void ApiNullabilityCheck::registerMatchers(ast_matchers::MatchFinder *Finder)
{
    using namespace ast_matchers;

    Finder->addMatcher(functionDecl(isDefinition(), unless(isImplicit())).bind("function"), this);
}

void ApiNullabilityCheck::check(const ast_matchers::MatchFinder::MatchResult &Result)
{
    const auto *Func = Result.Nodes.getNodeAs<FunctionDecl>("function");
    if (!Func || !Func->hasBody()) {
        return;
    }

    const auto *Body = dyn_cast<CompoundStmt>(Func->getBody());
    if (!Body) {
        return;
    }

    const SourceManager &SM = *Result.SourceManager;
    ASTContext &Ctx = *Result.Context;
    const LangOptions &LangOpts = Ctx.getLangOpts();

    for (const ParmVarDecl *Param : Func->parameters()) {
        if (!Param || Param->getName().empty()) {
            continue;
        }

        const Direction Dir = getDirection(*Param);
        if (Dir != Direction::In && Dir != Direction::Out && Dir != Direction::InOut &&
            Dir != Direction::OutOptional) {
            continue;
        }

        const llvm::StringRef ParamName = Param->getName();
        if (Dir == Direction::In) {
            if (!isFoliosPublicFunction(*Func)) {
                continue;
            }
            if (!isPointerLikeParameter(*Param)) {
                continue;
            }

            ParamDerefVisitor DerefVisitor(Param);
            DerefVisitor.TraverseStmt(Func->getBody());
            if (DerefVisitor.getDerefs().empty()) {
                continue;
            }

            NullCheckVisitor NullVisitor(Param, Ctx);
            NullVisitor.TraverseStmt(Func->getBody());
            if (!hasEntryAssert(Body, ParamName, EntryStatementLimit, SM, LangOpts) &&
                NullVisitor.getNullChecks().empty()) {
                diag(
                    Param->getLocation(),
                    "input pointer parameter %0 is dereferenced and should be asserted at "
                    "function entry or handled with an explicit NULL check"
                ) << ParamName;
            }
        } else if (Dir == Direction::Out || Dir == Direction::InOut) {
            if (!hasEntryAssert(Body, ParamName, EntryStatementLimit, SM, LangOpts) &&
                !delegatesRequiredOutputParam(Body, Param)) {
                diag(
                    Param->getLocation(),
                    "non-optional output parameter %0 should be asserted at function entry"
                ) << ParamName;
            }

            NullCheckVisitor Visitor(Param, Ctx);
            Visitor.TraverseStmt(Func->getBody());
            for (SourceLocation Loc : Visitor.getNullChecks()) {
                diag(
                    Loc,
                    "non-optional output parameter %0 is checked for NULL; assert it at "
                    "function entry, or mark it __out_optional only if the result may be "
                    "intentionally discarded without changing ownership or resource lifetime"
                ) << ParamName;
            }
        } else if (
            Dir == Direction::OutOptional &&
            hasEntryAssert(Body, ParamName, EntryStatementLimit, SM, LangOpts)
        ) {
            diag(
                Param->getLocation(),
                "optional output parameter %0 should be handled with a NULL check, not asserted"
            ) << ParamName;
        }
    }
}

}  // namespace clang::tidy::folios
