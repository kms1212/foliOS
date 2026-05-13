#ifndef FOLIOS_CLANG_TIDY_STATUS_MUST_CHECK_CHECK_H
#define FOLIOS_CLANG_TIDY_STATUS_MUST_CHECK_CHECK_H

#include <clang-tidy/ClangTidyCheck.h>

#include <string>

namespace clang::tidy::folios {

class StatusMustCheckCheck : public ClangTidyCheck {
public:
    StatusMustCheckCheck(llvm::StringRef Name, ClangTidyContext *Context);

    void registerMatchers(ast_matchers::MatchFinder *Finder) override;
    void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
    void storeOptions(ClangTidyOptions::OptionMap &Opts) override;

private:
    std::string StatusTypeName;
};

}  // namespace clang::tidy::folios

#endif  // FOLIOS_CLANG_TIDY_STATUS_MUST_CHECK_CHECK_H
