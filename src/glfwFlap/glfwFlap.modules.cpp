#include <ply-build-repo/Module.h>

// [ply module="glfwFlap"]
void module_glfwFlap(ModuleArgs* args) {
    args->buildTarget->targetType = BuildTargetType::EXE;
    args->addSourceFiles(".", false);
    args->addIncludeDir(Visibility::Private, ".");
    args->addTarget(Visibility::Private, "flapGame");
    args->addExtern(Visibility::Private, "glfw");
    args->addTarget(Visibility::Private, "glad");
}
