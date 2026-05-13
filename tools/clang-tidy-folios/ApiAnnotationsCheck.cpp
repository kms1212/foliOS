#include "ApiAnnotationsCheck.h"

#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Type.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Regex.h>

#include <string>

namespace clang::tidy::folios {
namespace {

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

bool isDirectionAnnotation(llvm::StringRef Annotation)
{
    const std::string Normalized = normalizeAnnotation(Annotation);
    return Normalized == "in" || Normalized == "out" || Normalized == "inout" ||
           Normalized == "out_optional" || Normalized == "st_in" || Normalized == "st_out" ||
           Normalized == "st_inout" || Normalized == "st_out_optional" ||
           Normalized == "vl_in" || Normalized == "vl_out" || Normalized == "vl_inout" ||
           Normalized == "vl_out_optional";
}

bool isBufferAnnotation(llvm::StringRef Annotation)
{
    const std::string Normalized = normalizeAnnotation(Annotation);
    return Normalized == "buf" || Normalized == "st_buf" || Normalized == "vl_buf";
}

bool hasApiAnnotation(const ParmVarDecl &Param, bool AllowBufOnly)
{
    for (const auto *Attr : Param.specific_attrs<AnnotateAttr>()) {
        if (isDirectionAnnotation(Attr->getAnnotation())) {
            return true;
        }
        if (AllowBufOnly && isBufferAnnotation(Attr->getAnnotation())) {
            return true;
        }
    }
    return false;
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

}  // namespace

ApiAnnotationsCheck::ApiAnnotationsCheck(llvm::StringRef Name, ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context),
      HeaderRegex(Options.get("HeaderRegex", ".*/(strata|vellum)/(.*/)?include/.*\\.h$")),
      IgnoreHeaderRegex(Options.get("IgnoreHeaderRegex", ".*/(internal|private)\\.h$")),
      AllowBufOnly(Options.get("AllowBufOnly", true))
{
}

void ApiAnnotationsCheck::storeOptions(ClangTidyOptions::OptionMap &Opts)
{
    Options.store(Opts, "HeaderRegex", HeaderRegex);
    Options.store(Opts, "IgnoreHeaderRegex", IgnoreHeaderRegex);
    Options.store(Opts, "AllowBufOnly", AllowBufOnly);
}

void ApiAnnotationsCheck::registerMatchers(ast_matchers::MatchFinder *Finder)
{
    using namespace ast_matchers;

    Finder->addMatcher(functionDecl(unless(isImplicit())).bind("function"), this);
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
    if (!matchesRegex(Filename, HeaderRegex) || matchesRegex(Filename, IgnoreHeaderRegex)) {
        return;
    }

    if (Func->getPreviousDecl() != nullptr) {
        return;
    }

    for (const ParmVarDecl *Param : Func->parameters()) {
        if (!Param || !isAnnotatableParameter(*Param) || hasApiAnnotation(*Param, AllowBufOnly)) {
            continue;
        }

        llvm::StringRef ParamName = Param->getName();
        if (ParamName.empty()) {
            diag(Param->getLocation(), "public API parameter is missing an API annotation");
        } else {
            diag(Param->getLocation(), "public API parameter %0 is missing an API annotation") << ParamName;
        }
    }
}

}  // namespace clang::tidy::folios
