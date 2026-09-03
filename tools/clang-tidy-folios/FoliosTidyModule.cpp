#include "ApiAnnotationsCheck.h"
#include "ApiNullabilityCheck.h"
#include "DistinctTypedefCheck.h"
#include "NullizedParamCheck.h"
#include "StatusMustCheckCheck.h"

#include <clang-tidy/ClangTidyModule.h>

namespace clang::tidy::folios {
namespace {

class FoliosModule : public ClangTidyModule {
public:
    void addCheckFactories(ClangTidyCheckFactories &Factories) override
    {
        Factories.registerCheck<ApiAnnotationsCheck>("folios-api-annotations");
        Factories.registerCheck<ApiNullabilityCheck>("folios-api-nullability");
        Factories.registerCheck<DistinctTypedefCheck>("folios-distinct-typedefs");
        Factories.registerCheck<NullizedParamCheck>("folios-nullized-params");
        Factories.registerCheck<StatusMustCheckCheck>("folios-status-must-check");
    }
};

static ClangTidyModuleRegistry::Add<FoliosModule> X(
    "folios-module", "Adds foliOS-specific clang-tidy checks."
);

}  // namespace

volatile int FoliosModuleAnchorSource = 0;

}  // namespace clang::tidy::folios
