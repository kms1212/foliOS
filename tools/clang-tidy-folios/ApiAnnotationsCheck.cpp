#include "ApiAnnotationsCheck.h"

#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Type.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Regex.h>

#include <cstdint>
#include <string>

namespace clang::tidy::folios {
namespace {

struct ApiAnnotationSet {
    std::string direction;
    bool has_buf = false;

    bool hasAny() const
    {
        return !direction.empty() || has_buf;
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

std::string getDirectionName(llvm::StringRef Annotation)
{
    const std::string Normalized = normalizeAnnotation(Annotation);
    if (Normalized == "in" || Normalized == "st_in" || Normalized == "vl_in") {
        return "in";
    }
    if (Normalized == "out" || Normalized == "st_out" || Normalized == "vl_out") {
        return "out";
    }
    if (Normalized == "inout" || Normalized == "st_inout" || Normalized == "vl_inout") {
        return "inout";
    }
    if (Normalized == "out_optional" || Normalized == "st_out_optional" ||
        Normalized == "vl_out_optional") {
        return "out_optional";
    }
    return {};
}

bool isBufferAnnotation(llvm::StringRef Annotation)
{
    const std::string Normalized = normalizeAnnotation(Annotation);
    return Normalized == "buf" || Normalized == "st_buf" || Normalized == "vl_buf";
}

ApiAnnotationSet getApiAnnotations(const ParmVarDecl &Param)
{
    ApiAnnotationSet Result;
    for (const auto *Attr : Param.specific_attrs<AnnotateAttr>()) {
        const std::string Direction = getDirectionName(Attr->getAnnotation());
        if (!Direction.empty()) {
            Result.direction = Direction;
            continue;
        }
        if (isBufferAnnotation(Attr->getAnnotation())) {
            Result.has_buf = true;
        }
    }
    return Result;
}

bool hasApiAnnotation(const ParmVarDecl &Param, bool AllowBufOnly)
{
    const ApiAnnotationSet Annotations = getApiAnnotations(Param);
    if (!Annotations.direction.empty()) {
        return true;
    }
    return AllowBufOnly && Annotations.has_buf;
}

bool isAnnotatableParameter(const ParmVarDecl &Param)
{
    QualType Type = Param.getType();
    if (Type.isNull() || Type->isVoidType()) {
        return false;
    }
    return true;
}

bool matchesRegex(llvm::StringRef Text, llvm::StringRef Pattern)
{
    if (Pattern.empty()) {
        return false;
    }
    llvm::Regex Regex(Pattern);
    return Regex.match(Text);
}

std::string formatAnnotations(const ApiAnnotationSet &Annotations)
{
    if (!Annotations.hasAny()) {
        return "none";
    }

    std::string Result;
    if (!Annotations.direction.empty()) {
        Result += "__";
        Result += Annotations.direction;
    }
    if (Annotations.has_buf) {
        if (!Result.empty()) {
            Result += " ";
        }
        Result += "__buf";
    }
    return Result;
}

bool annotationsEqual(const ApiAnnotationSet &Left, const ApiAnnotationSet &Right)
{
    return Left.direction == Right.direction && Left.has_buf == Right.has_buf;
}

bool hasAnyParameterAnnotation(const FunctionDecl &Func)
{
    for (const ParmVarDecl *Param : Func.parameters()) {
        if (Param && getApiAnnotations(*Param).hasAny()) {
            return true;
        }
    }
    return false;
}

const FunctionDecl *getReferenceDeclaration(const FunctionDecl &Func)
{
    const FunctionDecl *Fallback = nullptr;
    for (const FunctionDecl *Prev = Func.getPreviousDecl(); Prev; Prev = Prev->getPreviousDecl()) {
        if (Prev->getNumParams() != Func.getNumParams()) {
            continue;
        }
        if (!Fallback) {
            Fallback = Prev;
        }
        if (hasAnyParameterAnnotation(*Prev)) {
            return Prev;
        }
    }
    return Fallback;
}

bool isPublicHeaderDeclaration(
    const FunctionDecl &Func,
    const SourceManager &SM,
    llvm::StringRef HeaderRegex,
    llvm::StringRef IgnoreHeaderRegex
)
{
    SourceLocation Loc = SM.getExpansionLoc(Func.getLocation());
    if (Loc.isInvalid()) {
        return false;
    }

    llvm::StringRef Filename = SM.getFilename(Loc);
    return matchesRegex(Filename, HeaderRegex) && !matchesRegex(Filename, IgnoreHeaderRegex);
}

const FunctionDecl *getPublicHeaderDeclaration(
    const FunctionDecl &Func,
    const SourceManager &SM,
    llvm::StringRef HeaderRegex,
    llvm::StringRef IgnoreHeaderRegex
)
{
    const FunctionDecl *Fallback = nullptr;
    for (const FunctionDecl *Redecl : Func.redecls()) {
        if (!Redecl || Redecl->getNumParams() != Func.getNumParams() ||
            !isPublicHeaderDeclaration(*Redecl, SM, HeaderRegex, IgnoreHeaderRegex)) {
            continue;
        }
        if (!Fallback) {
            Fallback = Redecl;
        }
        if (Redecl->getPreviousDecl() == nullptr) {
            return Redecl;
        }
    }
    return Fallback;
}

}  // namespace

ApiAnnotationsCheck::ApiAnnotationsCheck(llvm::StringRef Name, ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context),
      HeaderRegex(
          Options.get("HeaderRegex", ".*/(strata|vellum)/(.*/)?include/(strata|vellum)/.*\\.h$")
      ),
      IgnoreHeaderRegex(Options.get("IgnoreHeaderRegex", ".*/(internal|private)\\.h$")),
      SourceRegex(Options.get("SourceRegex", ".*/(strata|vellum)/.*\\.[ch]$")),
      AllowBufOnly(Options.get("AllowBufOnly", true)),
      CheckRedeclarationAnnotations(Options.get("CheckRedeclarationAnnotations", true))
{
}

void ApiAnnotationsCheck::storeOptions(ClangTidyOptions::OptionMap &Opts)
{
    Options.store(Opts, "HeaderRegex", HeaderRegex);
    Options.store(Opts, "IgnoreHeaderRegex", IgnoreHeaderRegex);
    Options.store(Opts, "SourceRegex", SourceRegex);
    Options.store(Opts, "AllowBufOnly", AllowBufOnly);
    Options.store(Opts, "CheckRedeclarationAnnotations", CheckRedeclarationAnnotations);
}

void ApiAnnotationsCheck::registerMatchers(ast_matchers::MatchFinder *Finder)
{
    using namespace ast_matchers;

    Finder->addMatcher(functionDecl(unless(isImplicit())).bind("function"), this);
}

bool ApiAnnotationsCheck::shouldDiagnosePublicHeaderParam(
    const SourceManager &SM,
    SourceLocation Loc,
    unsigned Index
)
{
    Loc = SM.getExpansionLoc(Loc);
    if (Loc.isInvalid()) {
        return true;
    }

    const llvm::StringRef Filename = SM.getFilename(Loc);
    const std::uint64_t Offset = SM.getFileOffset(Loc);
    std::string Key = Filename.str();
    Key += ':';
    Key += std::to_string(Offset);
    Key += ':';
    Key += std::to_string(Index);
    return DiagnosedPublicHeaderParams.insert(Key).second;
}

void ApiAnnotationsCheck::checkPublicHeaderAnnotations(
    const FunctionDecl &Func,
    const SourceManager &SM,
    const FunctionDecl *DiagnosticFunc
)
{
    for (unsigned Index = 0; Index < Func.getNumParams(); ++Index) {
        const ParmVarDecl *Param = Func.getParamDecl(Index);
        if (!Param || !isAnnotatableParameter(*Param) || hasApiAnnotation(*Param, AllowBufOnly) ||
            !shouldDiagnosePublicHeaderParam(SM, Param->getLocation(), Index)) {
            continue;
        }

        const ParmVarDecl *DiagnosticParam =
            DiagnosticFunc && DiagnosticFunc->getNumParams() == Func.getNumParams()
                ? DiagnosticFunc->getParamDecl(Index)
                : Param;
        if (!DiagnosticParam) {
            DiagnosticParam = Param;
        }
        if (DiagnosticParam != Param && hasApiAnnotation(*DiagnosticParam, AllowBufOnly)) {
            continue;
        }

        llvm::StringRef ParamName = DiagnosticParam->getName();
        if (ParamName.empty()) {
            diag(DiagnosticParam->getLocation(), "public API parameter is missing an API annotation");
        } else if (DiagnosticParam != Param) {
            diag(DiagnosticParam->getLocation(),
                 "public API declaration for parameter %0 is missing an API annotation")
                << ParamName;
        } else {
            diag(DiagnosticParam->getLocation(), "public API parameter %0 is missing an API annotation")
                << ParamName;
        }
    }
}

void ApiAnnotationsCheck::check(const ast_matchers::MatchFinder::MatchResult &Result)
{
    const auto *Func = Result.Nodes.getNodeAs<FunctionDecl>("function");
    if (!Func || !Func->getIdentifier()) {
        return;
    }

    const SourceManager &SM = *Result.SourceManager;
    SourceLocation Loc = SM.getExpansionLoc(Func->getLocation());
    if (Loc.isInvalid()) {
        return;
    }

    llvm::StringRef Filename = SM.getFilename(Loc);
    const bool IsPublicHeaderDecl =
        isPublicHeaderDeclaration(*Func, SM, HeaderRegex, IgnoreHeaderRegex);

    if (IsPublicHeaderDecl && Func->getPreviousDecl() == nullptr) {
        checkPublicHeaderAnnotations(*Func, SM);
    } else if (!IsPublicHeaderDecl && Func->isThisDeclarationADefinition()) {
        if (const FunctionDecl *HeaderDecl =
                getPublicHeaderDeclaration(*Func, SM, HeaderRegex, IgnoreHeaderRegex)) {
            checkPublicHeaderAnnotations(*HeaderDecl, SM, Func);
        }
    }

    if (CheckRedeclarationAnnotations && matchesRegex(Filename, SourceRegex)) {
        const FunctionDecl *Reference = getReferenceDeclaration(*Func);
        if (Reference) {
            for (unsigned Index = 0; Index < Func->getNumParams(); ++Index) {
                const ParmVarDecl *Param = Func->getParamDecl(Index);
                const ParmVarDecl *ReferenceParam = Reference->getParamDecl(Index);
                if (!Param || !ReferenceParam || !isAnnotatableParameter(*Param) ||
                    !isAnnotatableParameter(*ReferenceParam)) {
                    continue;
                }

                const ApiAnnotationSet CurrentAnnotations = getApiAnnotations(*Param);
                const ApiAnnotationSet ReferenceAnnotations = getApiAnnotations(*ReferenceParam);
                if (annotationsEqual(CurrentAnnotations, ReferenceAnnotations)) {
                    continue;
                }

                llvm::StringRef ParamName = Param->getName();
                if (ParamName.empty()) {
                    diag(Param->getLocation(), "API annotation differs from previous declaration "
                                               "(current: %0, previous: %1)")
                        << formatAnnotations(CurrentAnnotations)
                        << formatAnnotations(ReferenceAnnotations);
                } else {
                    diag(Param->getLocation(),
                         "API annotation for parameter %0 differs from previous declaration "
                         "(current: %1, previous: %2)")
                        << ParamName << formatAnnotations(CurrentAnnotations)
                        << formatAnnotations(ReferenceAnnotations);
                }
            }
        }
    }
}

}  // namespace clang::tidy::folios
