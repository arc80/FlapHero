#include <ply-build-repo/Module.h>

// [ply module="Triangle"]
void module_Triangle(ModuleArgs* args) {
    args->buildTarget->targetType = BuildTargetType::EXE;
    args->addSourceFiles(".", false);
    args->addTarget(Visibility::Private, "runtime");
    args->addTarget(Visibility::Private, "math");
    args->addTarget(Visibility::Private, "glad");
    args->addExtern(Visibility::Private, "glfw");
}
