#ifndef FOLIOS_CLANG_TIDY_NULLIZED_PARAM_CHECK_H
#define FOLIOS_CLANG_TIDY_NULLIZED_PARAM_CHECK_H

#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang-tidy/ClangTidyCheck.h>

#include <set>
#include <string>

namespace clang::tidy::folios {

class NullizedParamCheck : public ClangTidyCheck {
public:
    NullizedParamCheck(llvm::StringRef Name, ClangTidyContext *Context);

    void registerMatchers(ast_matchers::MatchFinder *Finder) override;
    void check(const ast_matchers::MatchFinder::MatchResult &Result) override;

private:
    bool shouldDiagnose(const SourceManager &SM, SourceLocation Loc, llvm::StringRef Suffix);

    std::set<std::string> Diagnosed;
};

}  // namespace clang::tidy::folios

#endif  // FOLIOS_CLANG_TIDY_NULLIZED_PARAM_CHECK_H
