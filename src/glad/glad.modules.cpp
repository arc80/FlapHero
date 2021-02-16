#include <ply-build-repo/Module.h>

// [ply module="glad"]
void module_glad(ModuleArgs* args) {
    args->addSourceFiles("src", false);
    args->addIncludeDir(Visibility::Public, "include");
}

// [ply extern="assimp" provider="vcpkg"]
ExternResult extern_assimp_vcpkg(ExternCommand cmd, ExternProviderArgs* args) {
    PackageProvider prov{
        PackageProvider::Vcpkg, "assimp", [&](StringView prefix) {
            args->dep->includeDirs.append(NativePath::join(prefix, "x64-windows/include"));
            args->dep->libs.append(NativePath::join(prefix, "x64-windows/lib/assimp-vc142-mt.lib"));
            args->dep->dlls.append(NativePath::join(prefix, "x64-windows/bin/assimp-vc142-mt.dll"));
            args->dep->dlls.append(NativePath::join(prefix, "x64-windows/bin/zlib1.dll"));
            args->dep->dlls.append(NativePath::join(prefix, "x64-windows/bin/Irrlicht.dll"));
            args->dep->dlls.append(NativePath::join(prefix, "x64-windows/bin/libpng16.dll"));
            args->dep->dlls.append(NativePath::join(prefix, "x64-windows/bin/jpeg62.dll"));
            args->dep->dlls.append(NativePath::join(prefix, "x64-windows/bin/bz2.dll"));
        }};
    return prov.handle(cmd, args);
}

// [ply extern="assimp" provider="homebrew"]
ExternResult extern_assimp_homebrew(ExternCommand cmd, ExternProviderArgs* args) {
    PackageProvider prov{PackageProvider::Homebrew, "assimp", [&](StringView prefix) {
                             args->dep->includeDirs.append(NativePath::join(prefix, "include"));
                             args->dep->libs.append(NativePath::join(prefix, "lib/libassimp.dylib"));
                         }};
    return prov.handle(cmd, args);
}

// [ply extern="assimp" provider="macports"]
ExternResult extern_assimp_macports(ExternCommand cmd, ExternProviderArgs* args) {
    PackageProvider prov{PackageProvider::MacPorts, "assimp", [&](StringView prefix) {
                             args->dep->includeDirs.append(NativePath::join(prefix, "include"));
                             args->dep->libs.append(NativePath::join(prefix, "lib/libassimp.dylib"));
                         }};
    return prov.handle(cmd, args);
}

// [ply extern="assimp" provider="apt"]
ExternResult extern_assimp_apt(ExternCommand cmd, ExternProviderArgs* args) {
    PackageProvider prov{PackageProvider::Apt, "libassimp-dev",
                         [&](StringView) { args->dep->libs.append("-lassimp"); }};
    return prov.handle(cmd, args);
}

// [ply extern="glfw" provider="homebrew"]
ExternResult extern_glfw_homebrew(ExternCommand cmd, ExternProviderArgs* args) {
    PackageProvider prov{PackageProvider::Homebrew, "glfw", [&](StringView prefix) {
                             args->dep->includeDirs.append(NativePath::join(prefix, "include"));
                             args->dep->libs.append(NativePath::join(prefix, "lib/libglfw.dylib"));
                         }};
    return prov.handle(cmd, args);
}

// [ply extern="glfw" provider="apt"]
ExternResult extern_glfw_apt(ExternCommand cmd, ExternProviderArgs* args) {
    PackageProvider prov{PackageProvider::Apt, "libglfw3-dev",
                         [&](StringView) { args->dep->libs.append("-lglfw"); }};
    return prov.handle(cmd, args);
}

// [ply extern="glfw" provider="prebuilt"]
ExternResult extern_glfw_prebuilt(ExternCommand cmd, ExternProviderArgs* args) {
    // Toolchain filters
    if (args->toolchain->get("targetPlatform")->text() != "windows") {
        return {ExternResult::UnsupportedToolchain, "Target platform must be 'windows'"};
    }
    StringView arch = args->toolchain->get("arch")->text();
    if (find<StringView>({"x86", "x64"}, arch) < 0) {
        return {ExternResult::UnsupportedToolchain, "Target arch must be 'x86' or 'x64'"};
    }
    // Build system must be Visual Studio (for now) because the GLFW archive contains separate libs
    // for each Visual Studio version. Presumably this is for ABI compatibility, but I'm not sure
    // how important it is since GLFW seems to expose a C-style interface (?).
    StringView buildSystem = args->toolchain->get("buildSystem")->text();
    if (!buildSystem.startsWith("Visual Studio")) {
        return {ExternResult::UnsupportedToolchain, "Build system must be Visual Studio"};
    }
    StringView libFolder;
    if (buildSystem == "Visual Studio 16 2019") {
        libFolder = "lib-vc2019";
    } else if (buildSystem == "Visual Studio 15 2017") {
        libFolder = "lib-vc2017";
    } else if (buildSystem == "Visual Studio 13 2015") {
        libFolder = "lib-vc2015";
    } else {
        return {ExternResult::UnsupportedToolchain,
                String::format("Unsupported build system '{}'", buildSystem)};
    }
    if (args->providerArgs) {
        return {ExternResult::BadArgs, ""};
    }

    StringView version = "3.3.2";
    String archiveName =
        String::format("glfw-{}.bin.{}", version, arch == "x64" ? "WIN64" : "WIN32");

    // Handle Command
    Tuple<ExternResult, ExternFolder*> er = args->findExistingExternFolder(arch);
    if (cmd == ExternCommand::Status) {
        return er.first;
    } else if (cmd == ExternCommand::Install) {
        if (er.first.code != ExternResult::SupportedButNotInstalled) {
            return er.first;
        }
        ExternFolder* externFolder = args->createExternFolder(arch);
        String archivePath = NativePath::join(externFolder->path, archiveName + ".zip");
        String url = String::format("https://github.com/glfw/glfw/releases/download/{}/{}.zip",
                                    version, archiveName);
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
        String installFolder = NativePath::join(er.second->path, archiveName);
        args->dep->includeDirs.append(NativePath::join(installFolder, "include"));
        StringView platformFolder = arch;
        args->dep->libs.append(NativePath::join(installFolder, libFolder, "glfw3dll.lib"));
        args->dep->dlls.append(NativePath::join(installFolder, libFolder, "glfw3.dll"));
        return {ExternResult::Instantiated, ""};
    }
    PLY_ASSERT(0);
    return {ExternResult::Unknown, ""};
}
