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
#include <tuple>

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
    RefLocked,
};

enum class UnitRole {
    None,
    Count,
    Index,
};

struct DistinctType {
    DistinctKind Kind = DistinctKind::None;
    std::string Name;
    UnitRole UnitRole = UnitRole::None;
    std::string Unit;
    std::string Domain;
    std::string FlagsetDomain;

    bool isTagged() const
    {
        return Kind != DistinctKind::None;
    }

    bool hasUnit() const
    {
        return UnitRole != UnitRole::None && !Unit.empty();
    }

    bool isUnitCount() const
    {
        return UnitRole == UnitRole::Count && !Unit.empty();
    }

    bool isUnitIndex() const
    {
        return UnitRole == UnitRole::Index && !Unit.empty() && !Domain.empty();
    }

    bool isFlagset() const
    {
        return !FlagsetDomain.empty();
    }
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

DistinctKind annotationKind(llvm::StringRef Annotation)
{
    const std::string Normalized = unscopedAnnotation(Annotation);
    if (Normalized == "bitwise") {
        return DistinctKind::Bitwise;
    }
    if (Normalized == "nocast") {
        return DistinctKind::Nocast;
    }
    if (Normalized == "ref_strong") {
        return DistinctKind::RefStrong;
    }
    if (Normalized == "ref_weak") {
        return DistinctKind::RefWeak;
    }
    if (Normalized == "ref_borrowed") {
        return DistinctKind::RefBorrowed;
    }
    if (Normalized == "ref_internal") {
        return DistinctKind::RefInternal;
    }
    if (Normalized == "ref_locked") {
        return DistinctKind::RefLocked;
    }
    return DistinctKind::None;
}

void applyDomainAnnotation(DistinctType &Type, llvm::StringRef Annotation)
{
    const std::string Normalized = unscopedAnnotation(Annotation);
    llvm::StringRef Ref(Normalized);

    if (Ref.consume_front("unit_count=")) {
        Type.UnitRole = UnitRole::Count;
        Type.Unit = Ref.str();
        Type.Domain.clear();
        return;
    }
    if (Ref.consume_front("unit_index=")) {
        llvm::StringRef Unit;
        llvm::StringRef Domain;
        std::tie(Unit, Domain) = Ref.split(':');
        if (!Unit.empty() && !Domain.empty()) {
            Type.UnitRole = UnitRole::Index;
            Type.Unit = Unit.str();
            Type.Domain = Domain.str();
        }
        return;
    }
    if (Ref.consume_front("flagset=")) {
        Type.FlagsetDomain = Ref.str();
    }
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

    DistinctType Result;
    Result.Kind = getTypedefKind(*Decl);
    Result.Name = Decl->getNameAsString();
    for (const auto *Attr : Decl->specific_attrs<AnnotateAttr>()) {
        applyDomainAnnotation(Result, Attr->getAnnotation());
    }
    if (Result.Kind == DistinctKind::None && !Result.hasUnit() && !Result.isFlagset()) {
        return {};
    }

    return Result;
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

bool isPlainIntegerConstantToNocast(
    const DistinctType &Target, const DistinctType &Source, const Expr *SourceExpr, ASTContext &Ctx
)
{
    if (Target.Kind != DistinctKind::Nocast || Source.Kind != DistinctKind::None || !SourceExpr) {
        return false;
    }

    return SourceExpr->isIntegerConstantExpr(Ctx);
}

bool isIntegerConstantExpr(const Expr *Expr, ASTContext &Ctx)
{
    if (!Expr) {
        return false;
    }
    return Expr->IgnoreParenImpCasts()->isIntegerConstantExpr(Ctx);
}

DistinctType makeUnitCount(llvm::StringRef Unit)
{
    DistinctType Type;
    Type.Kind = DistinctKind::Nocast;
    Type.UnitRole = UnitRole::Count;
    Type.Unit = Unit.str();
    return Type;
}

bool isSameUnitCount(const DistinctType &Left, const DistinctType &Right)
{
    return Left.isUnitCount() && Right.isUnitCount() && Left.Unit == Right.Unit;
}

bool isSameUnitIndex(const DistinctType &Left, const DistinctType &Right)
{
    return Left.isUnitIndex() && Right.isUnitIndex() && Left.Unit == Right.Unit &&
        Left.Domain == Right.Domain;
}

bool isCountOperandForUnit(
    const DistinctType &Type, const Expr *Expr, ASTContext &Ctx, llvm::StringRef Unit
)
{
    if (Type.isUnitCount() && Type.Unit == Unit) {
        return true;
    }
    return isIntegerConstantExpr(Expr, Ctx);
}

bool isFlagsetOperandForDomain(
    const DistinctType &Type, const Expr *Expr, ASTContext &Ctx, llvm::StringRef Domain
)
{
    if (Type.isFlagset() && Type.FlagsetDomain == Domain) {
        return true;
    }
    return isIntegerConstantExpr(Expr, Ctx);
}

DistinctType getExpressionDistinctType(const Expr *Expr, ASTContext &Ctx);

DistinctType getUnaryExpressionDistinctType(const UnaryOperator *Op, ASTContext &Ctx)
{
    const DistinctType Sub = getExpressionDistinctType(Op->getSubExpr(), Ctx);
    switch (Op->getOpcode()) {
    case UO_Plus:
        return Sub;
    case UO_Not:
        if (Sub.isFlagset()) {
            return Sub;
        }
        return {};
    default:
        return {};
    }
}

DistinctType getBinaryExpressionDistinctType(const BinaryOperator *Op, ASTContext &Ctx)
{
    const Expr *LeftExpr = Op->getLHS();
    const Expr *RightExpr = Op->getRHS();
    const DistinctType Left = getExpressionDistinctType(LeftExpr, Ctx);
    const DistinctType Right = getExpressionDistinctType(RightExpr, Ctx);

    switch (Op->getOpcode()) {
    case BO_Add:
    case BO_AddAssign:
        if (Left.isUnitIndex() && isCountOperandForUnit(Right, RightExpr, Ctx, Left.Unit)) {
            return Left;
        }
        if (isSameUnitCount(Left, Right) ||
            (Left.isUnitCount() && isIntegerConstantExpr(RightExpr, Ctx))) {
            return Left;
        }
        if (Right.isUnitCount() && isIntegerConstantExpr(LeftExpr, Ctx)) {
            return Right;
        }
        return {};
    case BO_Sub:
    case BO_SubAssign:
        if (Left.isUnitIndex() && isCountOperandForUnit(Right, RightExpr, Ctx, Left.Unit)) {
            return Left;
        }
        if (Op->getOpcode() == BO_SubAssign) {
            if (isSameUnitCount(Left, Right) ||
                (Left.isUnitCount() && isIntegerConstantExpr(RightExpr, Ctx))) {
                return Left;
            }
            return {};
        }
        if (isSameUnitIndex(Left, Right)) {
            return makeUnitCount(Left.Unit);
        }
        if (isSameUnitCount(Left, Right) ||
            (Left.isUnitCount() && isIntegerConstantExpr(RightExpr, Ctx))) {
            return Left;
        }
        return {};
    case BO_Mul:
    case BO_MulAssign:
        if (Left.isUnitCount() && isIntegerConstantExpr(RightExpr, Ctx)) {
            return Left;
        }
        if (Op->getOpcode() == BO_MulAssign) {
            return {};
        }
        if (Right.isUnitCount() && isIntegerConstantExpr(LeftExpr, Ctx)) {
            return Right;
        }
        return {};
    case BO_Div:
    case BO_Rem:
    case BO_DivAssign:
    case BO_RemAssign:
        if (Left.isUnitCount() && isIntegerConstantExpr(RightExpr, Ctx)) {
            return Left;
        }
        return {};
    case BO_And:
    case BO_Or:
    case BO_Xor:
    case BO_AndAssign:
    case BO_OrAssign:
    case BO_XorAssign:
        if (Left.isFlagset() &&
            isFlagsetOperandForDomain(Right, RightExpr, Ctx, Left.FlagsetDomain)) {
            return Left;
        }
        if (Op->isCompoundAssignmentOp()) {
            return {};
        }
        if (Right.isFlagset() && isIntegerConstantExpr(LeftExpr, Ctx)) {
            return Right;
        }
        return {};
    default:
        return {};
    }
}

DistinctType getConditionalExpressionDistinctType(const ConditionalOperator *Op, ASTContext &Ctx)
{
    const Expr *TrueExpr = Op->getTrueExpr();
    const Expr *FalseExpr = Op->getFalseExpr();
    const DistinctType TrueType = getExpressionDistinctType(TrueExpr, Ctx);
    const DistinctType FalseType = getExpressionDistinctType(FalseExpr, Ctx);

    if (TrueType.isTagged() && !FalseType.isTagged() && isIntegerConstantExpr(FalseExpr, Ctx)) {
        return TrueType;
    }
    if (FalseType.isTagged() && !TrueType.isTagged() && isIntegerConstantExpr(TrueExpr, Ctx)) {
        return FalseType;
    }
    if (TrueType.Kind == FalseType.Kind && TrueType.Name == FalseType.Name && TrueType.Name != "") {
        return TrueType;
    }
    if (isSameUnitCount(TrueType, FalseType) || isSameUnitIndex(TrueType, FalseType)) {
        return TrueType;
    }
    if (TrueType.isFlagset() && FalseType.isFlagset() &&
        TrueType.FlagsetDomain == FalseType.FlagsetDomain) {
        return TrueType;
    }
    return {};
}

DistinctType getExpressionDistinctType(const Expr *Expr, ASTContext &Ctx)
{
    if (!Expr) {
        return {};
    }

    Expr = Expr->IgnoreParens();
    if (const auto *Cast = dyn_cast<ImplicitCastExpr>(Expr)) {
        const DistinctType CastType = getDistinctType(Cast->getType());
        if (CastType.isTagged()) {
            return CastType;
        }
        return getExpressionDistinctType(Cast->getSubExpr(), Ctx);
    }

    const DistinctType Type = getDistinctType(Expr->getType());
    if (Type.isTagged()) {
        return Type;
    }

    if (const auto *Binary = dyn_cast<BinaryOperator>(Expr)) {
        return getBinaryExpressionDistinctType(Binary, Ctx);
    }
    if (const auto *Unary = dyn_cast<UnaryOperator>(Expr)) {
        return getUnaryExpressionDistinctType(Unary, Ctx);
    }
    if (const auto *Conditional = dyn_cast<ConditionalOperator>(Expr)) {
        return getConditionalExpressionDistinctType(Conditional, Ctx);
    }
    return {};
}

bool isRefKind(DistinctKind Kind)
{
    return Kind == DistinctKind::RefStrong || Kind == DistinctKind::RefWeak ||
        Kind == DistinctKind::RefBorrowed || Kind == DistinctKind::RefInternal ||
        Kind == DistinctKind::RefLocked;
}

bool areDistinctTypesCompatible(const DistinctType &Target, const DistinctType &Source)
{
    if (Target.Kind == Source.Kind && Target.Name == Source.Name && Target.Name != "") {
        return true;
    }
    if (isSameUnitCount(Target, Source) || isSameUnitIndex(Target, Source)) {
        return true;
    }
    return Target.isFlagset() && Source.isFlagset() && Target.FlagsetDomain == Source.FlagsetDomain;
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
    if (areDistinctTypesCompatible(Target, Source)) {
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
    case DistinctKind::RefLocked:
        return "__ref_locked";
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
    if (Type.Name.empty() && Type.isUnitCount()) {
        return Type.Unit + " count expression";
    }
    if (Type.Name.empty() && Type.isUnitIndex()) {
        return Type.Unit + " index expression";
    }
    if (Type.Name.empty() && Type.isFlagset()) {
        return Type.FlagsetDomain + " flagset expression";
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
        checkConversion(
            Decl->getLocation(),
            Decl->getType(),
            Decl->getInit()->getType(),
            Decl->getInit(),
            "initialization"
        );
        return true;
    }

    bool VisitBinaryOperator(BinaryOperator *Op)
    {
        if (!Op || !Op->isAssignmentOp() || shouldSkip(Op->getOperatorLoc())) {
            return true;
        }
        const Expr *SourceExpr = Op->isCompoundAssignmentOp() ? Op : Op->getRHS();
        checkConversion(
            Op->getOperatorLoc(),
            Op->getLHS()->getType(),
            SourceExpr->getType(),
            SourceExpr,
            "assignment"
        );
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
        DistinctType Source = getExpressionDistinctType(SourceExpr, Ctx);
        if (!Source.isTagged()) {
            Source = getDistinctType(SourceType);
        }
        if (!shouldCheckMismatch(Target, Source, StrictNocast, StrictBitwise, StrictRefs)) {
            return;
        }
        if (isPlainIntegerConstantToNocast(Target, Source, SourceExpr, Ctx)) {
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
    : ClangTidyCheck(Name, Context), StrictNocast(Options.get("StrictNocast", false)),
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
