#ifndef FOLIOS_CLANG_TIDY_DISTINCT_TYPEDEF_CHECK_H
#define FOLIOS_CLANG_TIDY_DISTINCT_TYPEDEF_CHECK_H

#include <clang-tidy/ClangTidyCheck.h>

namespace clang::tidy::folios {

class DistinctTypedefCheck : public ClangTidyCheck {
public:
    DistinctTypedefCheck(llvm::StringRef Name, ClangTidyContext *Context);

    void registerMatchers(ast_matchers::MatchFinder *Finder) override;
    void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
    void storeOptions(ClangTidyOptions::OptionMap &Opts) override;

private:
    bool StrictNocast;
    bool StrictBitwise;
    bool StrictRefs;
};

}  // namespace clang::tidy::folios

#endif  // FOLIOS_CLANG_TIDY_DISTINCT_TYPEDEF_CHECK_H
