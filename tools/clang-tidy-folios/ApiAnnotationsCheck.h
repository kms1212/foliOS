#ifndef FOLIOS_CLANG_TIDY_API_ANNOTATIONS_CHECK_H
#define FOLIOS_CLANG_TIDY_API_ANNOTATIONS_CHECK_H

#include <clang/AST/Decl.h>
#include <clang/Basic/SourceLocation.h>
#include <clang-tidy/ClangTidyCheck.h>

#include <set>
#include <string>

namespace clang::tidy::folios {

class ApiAnnotationsCheck : public ClangTidyCheck {
public:
    ApiAnnotationsCheck(llvm::StringRef Name, ClangTidyContext *Context);

    void registerMatchers(ast_matchers::MatchFinder *Finder) override;
    void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
    void storeOptions(ClangTidyOptions::OptionMap &Opts) override;

private:
    void checkPublicHeaderAnnotations(
        const FunctionDecl &Func,
        const SourceManager &SM,
        const FunctionDecl *DiagnosticFunc = nullptr
    );
    bool shouldDiagnosePublicHeaderParam(const SourceManager &SM, SourceLocation Loc, unsigned Index);

    std::string HeaderRegex;
    std::string IgnoreHeaderRegex;
    std::string SourceRegex;
    bool AllowBufOnly;
    bool CheckRedeclarationAnnotations;
    std::set<std::string> DiagnosedPublicHeaderParams;
};

}  // namespace clang::tidy::folios

#endif  // FOLIOS_CLANG_TIDY_API_ANNOTATIONS_CHECK_H
