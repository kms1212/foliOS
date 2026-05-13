#include "DistinctTypedefCheck.h"

#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Stmt.h>
#include <clang/AST/Type.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/ADT/StringRef.h>

#include <string>

namespace clang::tidy::folios {
namespace {

enum class DistinctKind {
    None,
    Bitwise,
    Nocast,
    RefStrong,
    RefWeak,
    RefBorrowed,
    RefInternal,
};

struct DistinctType {
    DistinctKind Kind = DistinctKind::None;
    std::string Name;

    bool isTagged() const { return Kind != DistinctKind::None; }
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

DistinctKind annotationKind(llvm::StringRef Annotation)
{
    const std::string Normalized = normalizeAnnotation(Annotation);
    if (Normalized == "bitwise" || Normalized == "st_bitwise" || Normalized == "vl_bitwise") {
        return DistinctKind::Bitwise;
    }
    if (Normalized == "nocast" || Normalized == "st_nocast" || Normalized == "vl_nocast") {
        return DistinctKind::Nocast;
    }
    if (Normalized == "ref_strong" || Normalized == "st_ref_strong" ||
        Normalized == "vl_ref_strong") {
        return DistinctKind::RefStrong;
    }
    if (Normalized == "ref_weak" || Normalized == "st_ref_weak" ||
        Normalized == "vl_ref_weak") {
        return DistinctKind::RefWeak;
    }
    if (Normalized == "ref_borrowed" || Normalized == "st_ref_borrowed" ||
        Normalized == "vl_ref_borrowed") {
        return DistinctKind::RefBorrowed;
    }
    if (Normalized == "ref_internal" || Normalized == "st_ref_internal" ||
        Normalized == "vl_ref_internal") {
        return DistinctKind::RefInternal;
    }
    return DistinctKind::None;
}

DistinctKind getTypedefKind(const TypedefNameDecl &Decl)
{
    for (const auto *Attr : Decl.specific_attrs<AnnotateAttr>()) {
        const DistinctKind Kind = annotationKind(Attr->getAnnotation());
        if (Kind != DistinctKind::None) {
            return Kind;
        }
    }
    return DistinctKind::None;
}

DistinctType getDistinctType(QualType Type)
{
    Type = Type.getUnqualifiedType();
    const auto *Typedef = Type->getAs<TypedefType>();
    if (!Typedef) {
        return {};
    }

    const TypedefNameDecl *Decl = Typedef->getDecl();
    if (!Decl) {
        return {};
    }

    const DistinctKind Kind = getTypedefKind(*Decl);
    if (Kind == DistinctKind::None) {
        return {};
    }

    return {Kind, Decl->getNameAsString()};
}

const Expr *ignoreTransparentExprs(const Expr *Expr)
{
    if (!Expr) {
        return nullptr;
    }

    for (;;) {
        Expr = Expr->IgnoreParens();
        const auto *Cast = dyn_cast<ImplicitCastExpr>(Expr);
        if (!Cast) {
            return Expr;
        }
        Expr = Cast->getSubExpr();
    }
}

bool isExplicitCast(const Expr *Expr)
{
    Expr = ignoreTransparentExprs(Expr);
    return Expr && isa<ExplicitCastExpr>(Expr);
}

bool isRefKind(DistinctKind Kind)
{
    return Kind == DistinctKind::RefStrong || Kind == DistinctKind::RefWeak ||
           Kind == DistinctKind::RefBorrowed || Kind == DistinctKind::RefInternal;
}

bool shouldCheckMismatch(
    const DistinctType &Target,
    const DistinctType &Source,
    bool StrictNocast,
    bool StrictBitwise,
    bool StrictRefs
)
{
    if (!Target.isTagged() && !Source.isTagged()) {
        return false;
    }
    if (Target.Kind == Source.Kind && Target.Name == Source.Name) {
        return false;
    }
    if (isRefKind(Target.Kind) || isRefKind(Source.Kind)) {
        return StrictRefs || (Target.isTagged() && Source.isTagged());
    }
    if (Target.Kind == DistinctKind::Bitwise || Source.Kind == DistinctKind::Bitwise) {
        return StrictBitwise || (Target.isTagged() && Source.isTagged());
    }
    if (Target.Kind == DistinctKind::Nocast || Source.Kind == DistinctKind::Nocast) {
        return StrictNocast || (Target.isTagged() && Source.isTagged());
    }
    return false;
}

llvm::StringRef kindName(DistinctKind Kind)
{
    switch (Kind) {
    case DistinctKind::Bitwise:
        return "__bitwise";
    case DistinctKind::Nocast:
        return "__nocast";
    case DistinctKind::RefStrong:
        return "__ref_strong";
    case DistinctKind::RefWeak:
        return "__ref_weak";
    case DistinctKind::RefBorrowed:
        return "__ref_borrowed";
    case DistinctKind::RefInternal:
        return "__ref_internal";
    case DistinctKind::None:
        return "plain";
    }
    return "plain";
}

std::string displayName(const DistinctType &Type)
{
    if (!Type.isTagged()) {
        return "plain type";
    }
    return Type.Name;
}

class DistinctTypedefVisitor : public RecursiveASTVisitor<DistinctTypedefVisitor> {
public:
    DistinctTypedefVisitor(
        DistinctTypedefCheck &Check,
        ASTContext &Ctx,
        bool StrictNocast,
        bool StrictBitwise,
        bool StrictRefs
    )
        : Check(Check), Ctx(Ctx), SM(Ctx.getSourceManager()), StrictNocast(StrictNocast),
          StrictBitwise(StrictBitwise), StrictRefs(StrictRefs)
    {
    }

    bool VisitVarDecl(VarDecl *Decl)
    {
        if (!Decl || !Decl->hasInit() || shouldSkip(Decl->getLocation())) {
            return true;
        }
        checkConversion(Decl->getLocation(), Decl->getType(), Decl->getInit()->getType(), Decl->getInit(), "initialization");
        return true;
    }

    bool VisitBinaryOperator(BinaryOperator *Op)
    {
        if (!Op || !Op->isAssignmentOp() || shouldSkip(Op->getOperatorLoc())) {
            return true;
        }
        checkConversion(Op->getOperatorLoc(), Op->getLHS()->getType(), Op->getRHS()->getType(), Op->getRHS(), "assignment");
        return true;
    }

    bool VisitReturnStmt(ReturnStmt *Stmt)
    {
        if (!Stmt || !CurrentReturnType || shouldSkip(Stmt->getReturnLoc())) {
            return true;
        }
        const Expr *Ret = Stmt->getRetValue();
        if (!Ret) {
            return true;
        }
        checkConversion(Stmt->getReturnLoc(), *CurrentReturnType, Ret->getType(), Ret, "return");
        return true;
    }

    bool TraverseFunctionDecl(FunctionDecl *Decl)
    {
        const QualType *SavedReturnType = CurrentReturnType;
        QualType ReturnType;
        if (Decl) {
            ReturnType = Decl->getReturnType();
            CurrentReturnType = &ReturnType;
        }
        RecursiveASTVisitor<DistinctTypedefVisitor>::TraverseFunctionDecl(Decl);
        CurrentReturnType = SavedReturnType;
        return true;
    }

    bool VisitCallExpr(CallExpr *Call)
    {
        if (!Call || shouldSkip(Call->getBeginLoc())) {
            return true;
        }

        const FunctionDecl *Callee = Call->getDirectCallee();
        if (!Callee) {
            return true;
        }

        const unsigned ArgCount = std::min(Call->getNumArgs(), Callee->getNumParams());
        for (unsigned I = 0; I < ArgCount; ++I) {
            const ParmVarDecl *Param = Callee->getParamDecl(I);
            const Expr *Arg = Call->getArg(I);
            if (!Param || !Arg) {
                continue;
            }
            checkConversion(Arg->getBeginLoc(), Param->getType(), Arg->getType(), Arg, "argument");
        }
        return true;
    }

private:
    bool shouldSkip(SourceLocation Loc) const
    {
        if (Loc.isInvalid()) {
            return true;
        }
        Loc = SM.getExpansionLoc(Loc);
        return SM.isInSystemHeader(Loc);
    }

    void checkConversion(
        SourceLocation Loc,
        QualType TargetType,
        QualType SourceType,
        const Expr *SourceExpr,
        llvm::StringRef Context
    )
    {
        if (isExplicitCast(SourceExpr)) {
            return;
        }

        const DistinctType Target = getDistinctType(TargetType);
        const DistinctType Source = getDistinctType(SourceType);
        if (!shouldCheckMismatch(Target, Source, StrictNocast, StrictBitwise, StrictRefs)) {
            return;
        }

        Check.diag(Loc, "%0 converts %1 %2 to %3 %4 without an explicit cast")
            << Context << kindName(Source.Kind) << displayName(Source) << kindName(Target.Kind)
            << displayName(Target);
    }

    DistinctTypedefCheck &Check;
    ASTContext &Ctx;
    SourceManager &SM;
    bool StrictNocast;
    bool StrictBitwise;
    bool StrictRefs;
    const QualType *CurrentReturnType = nullptr;
};

}  // namespace

DistinctTypedefCheck::DistinctTypedefCheck(llvm::StringRef Name, ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context),
      StrictNocast(Options.get("StrictNocast", false)),
      StrictBitwise(Options.get("StrictBitwise", true)),
      StrictRefs(Options.get("StrictRefs", false))
{
}

void DistinctTypedefCheck::storeOptions(ClangTidyOptions::OptionMap &Opts)
{
    Options.store(Opts, "StrictNocast", StrictNocast);
    Options.store(Opts, "StrictBitwise", StrictBitwise);
    Options.store(Opts, "StrictRefs", StrictRefs);
}

void DistinctTypedefCheck::registerMatchers(ast_matchers::MatchFinder *Finder)
{
    using namespace ast_matchers;

    Finder->addMatcher(translationUnitDecl().bind("tu"), this);
}

void DistinctTypedefCheck::check(const ast_matchers::MatchFinder::MatchResult &Result)
{
    const auto *TU = Result.Nodes.getNodeAs<TranslationUnitDecl>("tu");
    if (!TU) {
        return;
    }

    DistinctTypedefVisitor Visitor(*this, *Result.Context, StrictNocast, StrictBitwise, StrictRefs);
    Visitor.TraverseDecl(const_cast<TranslationUnitDecl *>(TU));
}

}  // namespace clang::tidy::folios
