# My Little C Builder
Small stb-style single file header, auto build system for C projects.
Software is unfinished, use at your own risk

Example: 
``` C
// in 'build.c' located at the root of your workspace

#include "mlcb.h"
#include "stdio.h"

string_t engine_build_params()
{
    char vulkan_sdk_path[256] ;
    if(os_getenv("VK_SDK_PATH", vulkan_sdk_path))
    {
        printf("Found VK_SDK_PATH : %s\n", vulkan_sdk_path);
    }
    else
    {
        printf("[ERR] Cannot find VK_SDK_PATH environment variable.\n");
        printf("      Make sure vulkan is installed and the VK_SDK_PATH is set.\n");
        exit(-1);
    }
    string_t cmd = {0};
    string_append(&cmd, "-g -Wvarargs -Wall -Werror -Wextra ");
    string_append(&cmd, "-I");
    string_append(&cmd, vulkan_sdk_path);
    string_append(&cmd, "\\Include ");
    string_append(&cmd, "-L");
    string_append(&cmd, vulkan_sdk_path);
    string_append(&cmd, "\\Lib\\ ");
    string_append(&cmd, "-luser32 ");
    string_append(&cmd, " -lvulkan-1 ");
    return cmd;
}

int main(int argc, char** argv)
{
    bootstrap(argc, argv, __FILE__, argv[0]);
    project_t engine =
    {
        .type = project_type_exe,
        .name = "engine",
        .src = "./engine/src/",
        .bin_dir = "./bin/engine",
        .compiler = "clang",
        .build_params = engine_build_params()
    };
    
    if(!build_project(&engine))
    {
        printf("Error: building project '%s' failed!\n", engine.name);
    }
    else
    {
        printf("Project '%s' build successfully\n", engine.name);
    }
}
```
