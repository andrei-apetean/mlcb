#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMMAND_INTIAL_CAPACITY 256
#ifdef _WIN32 
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define PATH_SEPARATOR "\\"
#define realpath(N,R) _fullpath((R),(N), MAX_PATH)
#else
#define PATH_SEPARATOR "/"
#endif

typedef struct string_t
{
    size_t length;
    size_t capacity;
    char* value;
} string_t;

void string_append(string_t* str, const char* value);
//void string_insert(string_t* str, size_t index, char* value);
void string_destroy(string_t* str);
void string_reset(string_t* str);

typedef struct dirent_t
{
    char name[256];
    bool is_directory;
} dirent_t;

typedef enum project_type_t
{
    project_type_exe = 0,
    project_type_shared_library = 1
} project_type_t;

typedef struct project_t
{
    project_type_t type;
    char           name[MAX_PATH];
    char           src[MAX_PATH];
    char           compiler[MAX_PATH];
    char           bin_dir[MAX_PATH];
    string_t       build_params;

} project_t;

bool build_project(project_t* project);

void bootstrap(int32_t argc, char** argv, const char* src_file, const char* exe_file);
bool _is_bootstrap_required(const char* src_file, const char* exe_file);

bool os_getenv(char* name, char* out_value);
bool os_run_sync(string_t* cmd);
bool os_file_rename(const char* old_path, const char* new_path);
bool os_directory_create(const char* path);
int32_t os_directory_get_entries(const char* dir_path, dirent_t* out);

#ifdef MLCB_IMPLEMENTATION
void bootstrap(int32_t argc, char** argv, const char* src_file, const char* exe_file)
{
    if (_is_bootstrap_required(src_file, exe_file))
    {
        printf("Bootstrapping required, rebuilding program...\n");
        // move the exe to backup
        char exe_backup[256] = { 0 };
        snprintf(exe_backup, 256, "%s.backup", exe_file);

        if (!os_file_rename(exe_file, exe_backup))
        {
            exit(-1);
        }
        //compile 
        string_t build = { 0 };
        string_append(&build, "clang ");
        string_append(&build, src_file);
        string_append(&build, " -o ");
        string_append(&build, exe_file);
        printf("Executing %s\n", build.value);
        if(os_run_sync(&build))
        {
            string_reset(&build);
            DWORD delete_result = DeleteFile(exe_backup);
            if (delete_result != 0)
            {
                printf("Error trying to delete backup %s, %lu\n", exe_backup, GetLastError());
            }
            for (int32_t i = 0; i < argc; ++i)
            {
                string_append(&build, argv[i]);
            }
            printf("Executing %s\n", build.value);
            os_run_sync(&build);
            string_destroy(&build);
            exit(0);
        }
        else {
            os_file_rename(exe_backup, exe_file);
            string_destroy(&build);
            printf("Bootstrapping program failed\n");
            exit(-1);
        }
    }
}

bool os_run_sync(string_t* cmd)
{
#ifdef _WIN32
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (CreateProcessA(
        NULL,           // No module name (use command line)
        cmd->value,     // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        TRUE,           // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi))           // Pointer to PROCESS_INFORMATION structure 
    {
        if(WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_OBJECT_0)
        {
            return true;
        }
        return false;

    }
    printf("Failed to create process: %s\n", cmd->value);
    return false;
#endif // _WIN32
}

bool os_getenv(char* name, char* out_value)
{
    size_t required_size;
    errno_t err = getenv_s(&required_size, NULL, 0, name);
    if (required_size != 0) {
        getenv_s(&required_size, out_value, required_size, name);
        return true;
    }
    out_value = NULL;
    return false;
}


bool os_file_rename(const char* old_path, const char* new_path)
{
#ifdef _WIN32
    if (!MoveFileEx(old_path, new_path, MOVEFILE_REPLACE_EXISTING))
    {
        printf("[ERR] could not rename %s to %s: %lu\n", old_path, new_path, GetLastError());
        return false;
    }
    printf("Renaming %s -> %s...\n", old_path, new_path);
    return true;
#endif // _WIN32
}

bool os_directory_create(const char* path)
{
#ifdef _WIN32
    if (CreateDirectoryA(path, NULL))
    {
        // Directory created successfully
        return true;
    }
    else
    {
        DWORD error = GetLastError();
        if (error == ERROR_ALREADY_EXISTS)
        {
            // Directory already exists, which is fine
            return true;
        }
        else
        {
            // Try to create the parent directory
            char parentPath[MAX_PATH];
            strcpy_s(parentPath, sizeof(parentPath), path);
            size_t length = strlen(parentPath);
            // Find the last path separator ('/' or '\')
            for (int i = length - 1; i >= 0; --i)
            {
                if (parentPath[i] == '/' || parentPath[i] == '\\')
                {
                    parentPath[i] = '\0';
                    break;
                }
            };

            if (os_directory_create(parentPath))
            {
                // Parent directory created successfully, try creating the current directory again
                return CreateDirectoryA(path, NULL);
            }
            else
            {
                // Failed to create parent directory
                return false;
            }
        }
    }
#endif
}


int32_t os_directory_get_entries(const char* dir_path, dirent_t* out)
{
#ifdef _WIN32
    HANDLE h_find;
    WIN32_FIND_DATA find_file_data;
    if (out == NULL)
    {
        int32_t count = 0;
        char searchPath[MAX_PATH];
        snprintf(searchPath, MAX_PATH, "%s\\*", dir_path);

        h_find = FindFirstFile(searchPath, &find_file_data);
        if (h_find == INVALID_HANDLE_VALUE) {
            printf("Unable to find files in directory. %s\n", dir_path);
            return -1;
        }
        do {
            count++;
        } while (FindNextFile(h_find, &find_file_data) != 0);
        FindClose(h_find);
        return count;
    }
    else
    {
        char searchPath[MAX_PATH];
        snprintf(searchPath, MAX_PATH, "%s\\*", dir_path);

        h_find = FindFirstFile(searchPath, &find_file_data);
        if (h_find == INVALID_HANDLE_VALUE) {
            return -1;
        }

        int i = 0;
        do {
            dirent_t d = { 0 };
            d.is_directory = (find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            strcpy_s(d.name, sizeof(d.name), find_file_data.cFileName);
            out[i] = d;
            i++;
        } while (FindNextFile(h_find, &find_file_data) != 0);
        FindClose(h_find);
        return 0;
    }
#endif
    return -1;
}

bool _is_bootstrap_required(const char* src_file, const char* exe_file)
{
#ifdef _WIN32
    HANDLE src_file_handle;
    HANDLE exe_file_handle;
    FILETIME src_file_time;
    FILETIME exe_file_time;
    //get time of src
    src_file_handle = CreateFile(src_file, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
    if (src_file_handle == INVALID_HANDLE_VALUE)
    {
        printf("Input file is invalid %lu\n", GetLastError());
        exit(-1);
    }
    BOOL bSuccess = GetFileTime(src_file_handle, NULL, NULL, &src_file_time);
    CloseHandle(src_file_handle);
    if (!bSuccess)
    {
        printf("Could not retrieve time of %s: %lu\n", src_file, GetLastError());
        exit(-1);
    }

    exe_file_handle = CreateFile(exe_file, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
    if (exe_file_handle == INVALID_HANDLE_VALUE)
    {
        printf("Input file is invalid %lu\n", GetLastError());
        exit(-1);
    }
    bSuccess = GetFileTime(exe_file_handle, NULL, NULL, &exe_file_time);
    CloseHandle(exe_file_handle);
    if (!bSuccess)
    {
        printf("Could not retrieve time of %s: %lu\n", exe_file, GetLastError());
        exit(-1);
    }

    return (CompareFileTime(&src_file_time, &exe_file_time) == 1);
#endif // _WIN32
}

void string_append(string_t* str, const char* value)
{
    size_t size = strlen(value);
    size_t minimum_capacity = size + str->length;
    if (str->capacity == 0)
    {
        str->capacity = COMMAND_INTIAL_CAPACITY;
        while (str->capacity < minimum_capacity)
        {
            str->capacity *= 2;
        }
        str->value = malloc(sizeof(char) * str->capacity);
        memset(str->value, 0, str->capacity);
        memcpy(str->value, value, size * sizeof(char));
        str->length += size;
        return;
    }
    if (str->capacity < minimum_capacity)
    {
        while (str->capacity < minimum_capacity)
        {
            str->capacity *= 2;
        }
        char* old_value = str->value;
        str->value = malloc(sizeof(char) * str->capacity);
        memset(str->value, 0, str->capacity);
        assert(str->value && "string malloc failed");
        memcpy(str->value, old_value, str->length);
        free(old_value);
    }
    memset(str->value + str->length, 0, size);
    memcpy(str->value + str->length, value, size);
    assert(str->value && "string memcpy failed");
    str->length += size;
}

void string_destroy(string_t* str)
{
    if (str->value != NULL) {
        free(str->value);
    }
    str->length = 0;
    str->capacity = 0;
}
void string_reset(string_t* str)
{
    memset(str->value, 0, sizeof(char) * str->capacity);
    str->length = 0;
}

void _os_get_directory_filenames(const char* path, string_t* buff)
{
    int num_entries = os_directory_get_entries(path, NULL);
    if(num_entries < 0)
    {
        return;
    }
    dirent_t* entries = malloc(num_entries * sizeof(dirent_t));
    os_directory_get_entries(path, entries);
    for(int i = 0; i < num_entries; i++)
    {
        char full_path[MAX_PATH] = { 0 };
        snprintf(full_path, MAX_PATH, "%s"PATH_SEPARATOR"%s",path, entries[i].name);
        if(strcmp(entries[i].name, "..") == 0 || strcmp(entries[i].name, ".") == 0)
        {
            continue;
        }
        if(entries[i].is_directory)
        {
            _os_get_directory_filenames(full_path, buff);
        }
        else
        {
            size_t path_len = strlen(full_path);
            char* ext = full_path + path_len-2;
            if(strcmp(".c", ext) == 0)
            {
                string_append(buff, full_path);
                string_append(buff, " ");
            }
        }
    }
    free(entries);
}

bool build_project(project_t* project)
{
#ifdef _WIN32
    char* fmt = "%s -o %s"PATH_SEPARATOR"%s.exe ";
#else
    char* fmr = "%s -o %s ";
#endif
    //append -o flag
    char buff[1024] = {0};
    snprintf(buff, 1024, fmt, project->compiler, project->bin_dir, project->name);
    string_t build = {0};
    string_append(&build, buff);
    
    // get src files
    char src_full_path[MAX_PATH] = { 0 };
    realpath(project->src, src_full_path);
    _os_get_directory_filenames(src_full_path, &build);
 
    //create out dir
    if(!os_directory_create(project->bin_dir))
    {
        printf("Failed to create bin directory %s", project->bin_dir);
    }

    string_append(&build, project->build_params.value);
    printf("Executing: \n%s\n", build.value);
    //build
    return os_run_sync(&build);
}
#endif