#include "StatusMustCheckCheck.h"

#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ParentMapContext.h>
#include <clang/AST/Stmt.h>
#include <clang/AST/Type.h>
#include <clang/Basic/SourceManager.h>

namespace clang::tidy::folios {
namespace {

bool isNamedTypedef(QualType Type, llvm::StringRef Name)
{
    Type = Type.getUnqualifiedType();
    const auto *Typedef = Type->getAs<TypedefType>();
    return Typedef && Typedef->getDecl() && Typedef->getDecl()->getName() == Name;
}

bool isVoidExplicitCast(const Stmt *Stmt)
{
    const auto *Cast = dyn_cast<ExplicitCastExpr>(Stmt);
    return Cast && Cast->getType()->isVoidType();
}

bool isTransparentParent(const Stmt *Stmt)
{
    return isa<ImplicitCastExpr>(Stmt) || isa<ParenExpr>(Stmt) || isa<ExprWithCleanups>(Stmt) ||
           isa<FullExpr>(Stmt);
}

const Stmt *getOnlyStmtParent(const Stmt *Stmt, ASTContext &Ctx)
{
    DynTypedNodeList Parents = Ctx.getParents(*Stmt);
    if (Parents.size() != 1) {
        return nullptr;
    }

    const DynTypedNode &Parent = Parents[0];
    if (const auto *ParentExpr = Parent.get<Expr>()) {
        return ParentExpr;
    }
    if (const auto *Compound = Parent.get<CompoundStmt>()) {
        return Compound;
    }
    if (const auto *Label = Parent.get<LabelStmt>()) {
        return Label;
    }
    if (const auto *Case = Parent.get<CaseStmt>()) {
        return Case;
    }
    if (const auto *Default = Parent.get<DefaultStmt>()) {
        return Default;
    }
    return nullptr;
}

bool isDiscardedCallResult(const CallExpr *Call, ASTContext &Ctx)
{
    const Stmt *Current = Call;

    for (;;) {
        const Stmt *Parent = getOnlyStmtParent(Current, Ctx);
        if (!Parent) {
            return false;
        }

        if (isVoidExplicitCast(Parent)) {
            return false;
        }

        if (isTransparentParent(Parent)) {
            Current = Parent;
            continue;
        }

        if (isa<CompoundStmt>(Parent) || isa<LabelStmt>(Parent) || isa<CaseStmt>(Parent) ||
            isa<DefaultStmt>(Parent)) {
            return true;
        }

        return false;
    }
}

}  // namespace

StatusMustCheckCheck::StatusMustCheckCheck(llvm::StringRef Name, ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context), StatusTypeName(Options.get("StatusTypeName", "StStatus"))
{
}

void StatusMustCheckCheck::storeOptions(ClangTidyOptions::OptionMap &Opts)
{
    Options.store(Opts, "StatusTypeName", StatusTypeName);
}

void StatusMustCheckCheck::registerMatchers(ast_matchers::MatchFinder *Finder)
{
    using namespace ast_matchers;

    Finder->addMatcher(callExpr(unless(isExpansionInSystemHeader())).bind("call"), this);
}

void StatusMustCheckCheck::check(const ast_matchers::MatchFinder::MatchResult &Result)
{
    const auto *Call = Result.Nodes.getNodeAs<CallExpr>("call");
    if (!Call || !isNamedTypedef(Call->getType(), StatusTypeName)) {
        return;
    }

    const SourceManager &SM = *Result.SourceManager;
    SourceLocation Loc = SM.getExpansionLoc(Call->getBeginLoc());
    if (Loc.isInvalid() || SM.isInSystemHeader(Loc)) {
        return;
    }

    if (!isDiscardedCallResult(Call, *Result.Context)) {
        return;
    }

    diag(Loc, "%0 return value is ignored; check it or cast the call to void explicitly")
        << StatusTypeName;
}

}  // namespace clang::tidy::folios
