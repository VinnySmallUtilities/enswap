// https://en.cppreference.com/

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <csignal>

using namespace std;

typedef uint_fast64_t uInt64;


std::vector<string> options;
const int maxSwapFileCount = 24;
string SwapFileSize;
uInt64 delay = 0;

string SwapFileNameTemplate, sdelPath;


char version[] = "enswap.cpp 2025.06.19";
void PrintVersion()
{
    cout << version << endl;
}

void PrintHelp()
{
    cout << "command line:" << endl;
    cout << "enswap file_with_options" << endl << endl;
    cout << "lines in file:" << endl << "Swap file size (param for fallocate, e.g. 2G)" << endl;
    cout << "free memory in Mb" << endl;
    cout << "Swap file name template, e.g. /Arcs/swapfile" << endl;
    cout << "Path to sdel, e.g. /usr/local/sbin/sdel"      << endl;
}

string meminfo_FileName = "/proc/meminfo";

std::vector<string> captureStrs = { "MemTotal:", "MemAvailable:", "SwapTotal:", "SwapFree:" };
map<string, uInt64> Dict;


uInt64 freeMemoryShould     = 1048576;  // Необходимый размер свободной памяти в кб, 1 гигабайт - это 1048576
uInt64 baseIntervalForCheck = 500;
uInt64 intervalForCheck     = baseIntervalForCheck;


uInt64 ParseValue(string line)
{
    uInt64 result = 0;
    int    i      = 0;

    for (; line[i] != ':'; i++) {}
    for (; line[i] < '0'  || line[i] > '9';  i++) {}
    for (; line[i] >= '0' && line[i] <= '9'; i++)
        result = result * 10  + (int)(line[i] - '0');

    return result;
}

bool getMemInfo()
{
    try
    {
        ifstream file(meminfo_FileName);

        if (!file.is_open())
        {
            cerr << "Error with operation: read file " << meminfo_FileName << endl;
            return 1;
        }

        Dict.clear();
        string line;
        while (getline(file, line))
        {
            // cout << line << endl;
            for (const auto& templateStr : captureStrs)
            {
                if (line.starts_with(templateStr))
                {
                    Dict[templateStr] = ParseValue(line);
                }
            }
            
            if (Dict.size() >= captureStrs.size())
            {
                break;
            }
        }

        file.close();
        return true;
    }
    catch (const exception& e)
    {
        cerr << "Error with operation: read file " << meminfo_FileName << endl << e.what() << endl;
        
        return false;
    }
}

// const char* path = "/bin/echo";
// char* const argv[] = {"echo", "'Hello from child process!'", nullptr};
pid_t RunAndWait(const char * process, char* const args[])
{/*
    // Атрибуты для posix_spawn
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    
    // Установка атрибутов для поиска в PATH
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
*/
    // Создаём дочерний процесс с помощью posix_spawn
    pid_t pid = 0;
    // if (  posix_spawn(&pid, process, &actions, &attr, args, environ)  )
    if (  posix_spawnp(&pid, process, nullptr, nullptr, args, environ)  )
    {
        perror("Ошибка при создании процесса");
        return 0;
    }
    
    int status;
    waitpid(pid, &status, 0);
    
    return pid;
}

int isFileNotExists(string fileName)
{
    if (!std::filesystem::exists(fileName))
    {
        cout << "File not found: " << fileName << endl;
        return 1;
    }

    // https://www.opennet.ru/man.shtml?topic=types.h&category=3&russian=5
    struct stat statbuf;
    if (stat(fileName.c_str(), &statbuf) == 0)
    {
        // Если это не обычный файл
        if ((statbuf.st_mode & S_IFREG) == 0)
        {
            cout << "Path to the file is not a regular file: " << fileName << endl;
            return 2;
        }
    }
    else
    {
        cout << "File not found: " << fileName << endl;
        return 1;
    }
    
    return 0;
}

bool getLinesFromFileStream(string FileName, std::vector<string> &lines)
{
    try
    {
        std::fstream file(FileName, std::ios_base::in);

        if (!file.is_open())
        {
            cout << "file can not open: " << FileName << endl;
            return false;
        }

        if (!file.good())
        {
            cout << "file can not read: " << FileName << endl;
            return false;
        }

        string line;
        while (getline(file, line))
        {
            lines.push_back(line);
        }

        return true;
    }
    catch (const exception& e)
    {
        cerr << "Error with operation: read file " << FileName << endl << e.what() << endl;
        return false;
    }
}

bool ParseOptions(string optionsFileName)
{
    if (isFileNotExists(optionsFileName))
    {
        return false;
    }

    if ( ! getLinesFromFileStream(optionsFileName, options)  )
        return false;

    if (options.size() < 5)
    {
        cout << "The options file must contains 5 lines." << endl << endl;
        PrintHelp();
        return false;
    }

    try
    {
        SwapFileSize         = options.at(0);
        freeMemoryShould     = std::stoll(options.at(1)) * 1024;
        SwapFileNameTemplate = options.at(2);
        sdelPath             = options.at(3);
        delay                = std::stoll(options.at(4));
    }
    catch (const exception& e)
    {
        cerr << "Error at parsing the option file: " << meminfo_FileName << endl << e.what() << endl;
        
        return false;
    }
    
    return true;
}

int SwapFilesCount = 0;
string getSwapFileName(int index)
{
    string sNumber = std::to_string(index);
    if (sNumber.size() == 1)
        sNumber = "0" + sNumber;
    
    return SwapFileNameTemplate + "-" + sNumber;
}

void fallocate(string NewSwapFileName)
{
    string exe = "fallocate";
    string p1  = "-l";
    char* const args[] = {(char* const) exe.c_str(), (char* const) p1.c_str(), (char* const) SwapFileSize.c_str(), (char* const) NewSwapFileName.c_str(), nullptr};

    RunAndWait(exe.c_str(), args);
}

void chmod(string NewSwapFileName, string mode)
{
    string exe = "chmod";
    char* const args[] = {(char* const) exe.c_str(), (char* const) mode.c_str(), (char* const) NewSwapFileName.c_str(), nullptr};

    RunAndWait(exe.c_str(), args);
}

void mkswap(string NewSwapFileName)
{
    string exe = "mkswap";
    char* const args[] = {(char* const) exe.c_str(), (char* const) NewSwapFileName.c_str(), nullptr};

    RunAndWait(exe.c_str(), args);
}

void swapon(string NewSwapFileName)
{
    string exe = "swapon";
    string p1  = "-v";
    char* const args[] = {(char* const) exe.c_str(), (char* const) p1.c_str(), (char* const) NewSwapFileName.c_str(), nullptr};

    RunAndWait(exe.c_str(), args);
}

bool isSwapFileCountLimitReached()
{
    return SwapFilesCount >= maxSwapFileCount;
}

void addSwaps()
{
    string FileName;
    do
    {
        if (  isSwapFileCountLimitReached()  )
        {
            delay = 5000;
            cout << "Can not create new swap file, restriction reached: " << "SwapFilesCount > maxSwapFileCount" << endl;
            return;
        }

        FileName = getSwapFileName(SwapFilesCount);
        SwapFilesCount++;
    }
    while (filesystem::exists(FileName));

    cout << "Try to create swap file " << FileName << endl;
    fallocate(FileName);
    
    if (!filesystem::exists(FileName))
    {
        cout << "Can not create new swap file " << FileName << endl;
        return;
    }
    
    chmod (FileName, "0600");
    mkswap(FileName);
    swapon(FileName);
    
    cout << "Swapon ended" << endl;
}


void deleteAllSwaps()
{
    string dash = "-";
    // echo = "/usr/bin/echo"; echo.c_str()
    for (int i = 0; i < maxSwapFileCount; i++)
    {
        string fileName = getSwapFileName(i);
        if (!filesystem::exists(fileName))
            continue;

        cout << "Try to delete file " + fileName << endl;
        
        char* const args[] = {(char * const) sdelPath.c_str(), (char * const) dash.c_str(), (char * const) fileName.c_str(), nullptr};

        RunAndWait(sdelPath.c_str(), args);
        
        
        if (!filesystem::exists(fileName))
            continue;
        
        cout << "ERROR: The swap file can not delete: " << fileName << endl;
    }
}


bool isNeedPrintHelp(int argc, char* argv[])
{
    argc--;
    if (argc == 0 || argc < 0 || argc > 1)
        return true;

    std::vector<string> versionParamTemplates = { "-v", "--version", "--help", "-h" };
    if (argc == 1)
    {
        for (const auto& templateStr : versionParamTemplates)
        {
            if (argv[1] == templateStr)
            {
                cout << templateStr << endl;
                return true;
            }
        }
    }
    
    return false;
}

int checkStartupParams(int argc, char* argv[])
{
    if (isNeedPrintHelp(argc, argv))
    {
        PrintVersion();
        PrintHelp();
        return 1;
    }

    if (!getMemInfo())
    {
        return 2;
    }

    if (!ParseOptions(argv[1]))
    {
        return 3;
    }
    
    return 0;
}

bool toExit = false;
// param - номер получаемого сигнала
void terminate(int param)
{
    toExit = true;
}

// Основной цикл работы программы
void ProcessMemory()
{
    while (!toExit)
    {
        getMemInfo();
        uInt64 totalFree = Dict[captureStrs[1]] + Dict[captureStrs[3]];

        if ( ! isSwapFileCountLimitReached()  )
        if (totalFree < freeMemoryShould)
        {
            cout << "Low memory: " << totalFree << " < " << freeMemoryShould << endl;
            addSwaps();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
}

void swapoff()
{
    // string swapoffPath = "/usr/sbin/swapoff";
    string swapoffPath = "swapoff";
    string swapoffp1   = "-a";
    char* const args[] = {(char * const) swapoffPath.c_str(), (char * const) swapoffp1.c_str(), nullptr};

    RunAndWait(swapoffPath.c_str(), args);
}

int main(int argc, char* argv[])
{
    int ch = checkStartupParams(argc, argv);
    if (ch != 0)
        return ch;

    deleteAllSwaps();
    signal(SIGTERM, terminate);
    signal(SIGINT,  terminate);
    
    // Основной цикл работы программы
    ProcessMemory();
    
    // Завершение работы программы
    swapoff();

    // Ждём, пока всё отвалится, иначе файлы, почему-то, не получается удалить.
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    deleteAllSwaps();

    return 0;
}
