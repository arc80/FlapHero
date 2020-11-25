#include <ply-build-repo/Module.h>

// [ply module="flapGame"]
void module_flapGame(ModuleArgs* args) {
    args->addSourceFiles("flapGame", false);
    args->addIncludeDir(Visibility::Public, ".");
    args->addIncludeDir(Visibility::Public,
                        NativePath::join(args->projInst->env->buildFolderPath, "codegen/flapGame"));
    args->addTarget(Visibility::Public, "runtime");
    args->addTarget(Visibility::Public, "math");
    args->addTarget(Visibility::Private, "image");
    if (args->projInst->env->toolchain->get("targetPlatform")->text() == "ios") {
        args->setPreprocessorDefinition(Visibility::Private, "GLES_SILENCE_DEPRECATION", "1");
    } else {
        args->addTarget(Visibility::Private, "glad");
    }
    args->addExtern(Visibility::Private, "assimp");
    args->addExtern(Visibility::Private, "soloud");
    if (args->projInst->env->isGenerating) {
        String configFile = String::format(
            R"(#pragma once
#define FLAPGAME_REPO_FOLDER "{}"
)",
            fmt::EscapedString{
                NativePath::join(PLY_WORKSPACE_FOLDER, "repos", args->targetInst->repo->repoName)});
        FileSystem::native()->makeDirsAndSaveTextIfDifferent(
            NativePath::join(args->projInst->env->buildFolderPath,
                             "codegen/flapGame/flapGame/Config.h"),
            configFile, TextFormat::platformPreference());
    }
}

// [ply extern="soloud" provider="source"]
ExternResult extern_soloud_source(ExternCommand cmd, ExternProviderArgs* args) {
    if (args->providerArgs) {
        return {ExternResult::BadArgs, ""};
    }

    StringView version = "20200207";

    // Handle Command
    Tuple<ExternResult, ExternFolder*> er = args->findExistingExternFolder({});
    if (cmd == ExternCommand::Status) {
        return er.first;
    } else if (cmd == ExternCommand::Install) {
        if (er.first.code != ExternResult::SupportedButNotInstalled) {
            return er.first;
        }
        ExternFolder* externFolder = args->createExternFolder({});
        String archivePath =
            NativePath::join(externFolder->path, String::format("soloud_{}_lite.zip", version));
        String url = String::format("http://sol.gfxile.net/soloud/soloud_{}_lite.zip", version);
        if (!downloadFile(archivePath, url)) {
            return {ExternResult::InstallFailed, String::format("Error downloading '{}'", url)};
        }
        if (!extractFile(archivePath)) {
            return {ExternResult::InstallFailed,
                    String::format("Error extracting '{}'", archivePath)};
        }
        FileSystem::native()->deleteFile(archivePath);
        externFolder->success = true;
        externFolder->save();
        return {ExternResult::Installed, ""};
    } else if (cmd == ExternCommand::Instantiate) {
        if (er.first.code != ExternResult::Installed) {
            return er.first;
        }
        String installFolder = NativePath::join(er.second->path, StringView{"soloud"} + version);
        args->dep->buildTarget = new BuildTarget{args->dep};
        args->dep->buildTarget->name = "soloud";
        args->dep->buildTarget->addIncludeDir(Visibility::Public,
                                              NativePath::join(installFolder, "include"));
        args->dep->buildTarget->addSourceFiles(NativePath::join(installFolder, "src/audiosource"));
        args->dep->buildTarget->addSourceFiles(NativePath::join(installFolder, "src/core"));
        args->dep->buildTarget->addSourceFiles(NativePath::join(installFolder, "src/filter"));
        if (args->toolchain->get("apple")->isValid()) {
            args->dep->buildTarget->addSourceFiles(
                NativePath::join(installFolder, "src/backend/coreaudio"));
            args->dep->buildTarget->setPreprocessorDefinition(Visibility::Private, "WITH_COREAUDIO",
                                                              "1");
            args->dep->buildTarget->dep->libs.append("-framework AudioToolbox");
        } else {
            args->dep->buildTarget->addSourceFiles(
                NativePath::join(installFolder, "src/backend/miniaudio"));
            args->dep->buildTarget->setPreprocessorDefinition(Visibility::Private, "WITH_MINIAUDIO",
                                                              "1");
        }
        return {ExternResult::Instantiated, ""};
    }
    PLY_ASSERT(0);
    return {ExternResult::Unknown, ""};
}
