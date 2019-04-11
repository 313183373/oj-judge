#include <iostream>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

#include <unistd.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <string>
#include <fstream>
#include <sstream>
#include <regex>


using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

#define DOCKER

#ifdef DOCKER
#define MONGO_URI "mongodb://192.168.0.1:27017"    //use host.docker.internal to connect to the mongodb server on the host computer
#else
#define MONGO_URI "mongodb://localhost:27017"
#endif

#define COMPILE_ERROR_LOG "compile_error.log"
#define COMPILE_LOG "compile.log"
#define DATA_IN "datain.txt"
#define DATA_OUT "dataout.txt"
#define ERR_OUT "errout.txt"
#define CPP_CODE_FILE "main.cpp"
#define C_CODE_FILE "main.c"

#define COMPILE_TIME_LIMIT 5    //s
#define COMPILE_MEM_LIMIT 128   //MB

#define OUTPUT_FILE_SIZE_LIMIT 10   //MB

#define AC 0
#define TLE 1
#define MLE 2
#define WA 3
#define RE 4
#define CE 5
#define SYSTEM_ERROR 6
#define INVALID_PROBLEM_ID 7
#define OLE 8

#define MB 1024 * 1024
#define KB 1024

#if __WORDSIZE == 64
#define SYSTEM_CALL(reg) reg.orig_rax
#define ARG_1(reg) reg.rdi
#define ARG_2(reg) reg.rsi
#define ARG_3(reg) reg.rdx
#define RET(reg) reg.rax
#else
#define SYSTEM_CALL(reg) reg.orig_eax
#define ARG_1(reg) reg.ebx
#define ARG_2(reg) reg.ecx
#define ARG_3(reg) reg.edx
#define RET(reg) reg.eax
#endif

std::string readFileToString(std::string);

void setLimit(int limitType, int limit);

bool isFileEmpty(std::string name);

int extractIntFromString(std::string string);

bool getProcStatus(int pid, int procStatus[]);

long getPagesize();

long getFileSize(std::string);

bool compare(std::string, std::string);

void saveCodeToFile(std::string, std::string);

void saveToFile(std::string, std::string);

void updateByID(mongocxx::collection, std::string, std::string, std::string);

void updateStatusAndMessage(mongocxx::collection, std::string, std::string, std::string);

void updateStatusAndMemAndTimeAndMessage(mongocxx::collection collection, std::string id, std::string status,
                                         long mem, long time, std::string message);

const std::string RESULT[] = {"AC", "TLE", "MLE", "WA", "RE", "CE", "SYSTEM_ERROR", "INVALID_PROBLEM_ID", "OLE"};

std::string getUtf8ValueFromDocument(bsoncxx::document::view view, std::string key) {
    bsoncxx::document::element value = view[key];
    if (value.type() == bsoncxx::type::k_utf8) {
        return value.get_utf8().value.to_string();
    } else {
        std::cout << key << " is not utf8" << std::endl;
        return "";
    }
}

int getInt32ValueFromDocument(bsoncxx::document::view view, std::string key) {
    bsoncxx::document::element value = view[key];
    if (value.type() == bsoncxx::type::k_int32) {
        return value.get_int32().value;
    } else {
        std::cout << key << "is not int32" << std::endl;
        return -1;
    }
}

void getArrayFromDocument(bsoncxx::document::view view, std::string key) {
    bsoncxx::document::element array = view[key];
    if (array.type() == bsoncxx::type::k_array) {
        bsoncxx::array::view arr = array.get_array().value;
        for (bsoncxx::array::element item : arr) {
            if (item.type() == bsoncxx::type::k_document) {
                bsoncxx::document::view itemView = item.get_document().value;
                bsoncxx::document::element input = itemView["input"];
                bsoncxx::document::element output = itemView["output"];
                if (input && input.type() == bsoncxx::type::k_utf8) {
                    std::cout << "Input: " << input.get_utf8().value.to_string() << std::endl;
                }
                if (output && output.type() == bsoncxx::type::k_utf8) {
                    std::cout << "Output: " << output.get_utf8().value.to_string() << std::endl;
                }
            }
        }
    }
}

void printUsage(struct rusage usage) {
    // std::cout << "user time: " << "sec: " << usage.ru_utime.tv_sec << " usec: " << usage.ru_utime.tv_usec << std::endl;     //usec是10^-6次方秒，微秒
    // std::cout << "system time: " << "sec: " << usage.ru_stime.tv_sec << " usec: " << usage.ru_stime.tv_usec << std::endl;
    // std::cout << "max resident set size: " <<  usage.ru_maxrss << std::endl;
    // std::cout << "integral shared text memory size: " << usage.ru_ixrss << std::endl; // not supported
    // std::cout << "integral unshared data size: " << usage.ru_idrss << std::endl; // not supported
    // std::cout << "integral unshared stack size: " << usage.ru_isrss << std::endl; // not supported 
    // std::cout << "swaps: " << usage.ru_nswap << std::endl; // not supported
    // std::cout << "block input operations: " << usage.ru_inblock << std::endl;
    // std::cout << "block output operations: " << usage.ru_oublock << std::endl;
    // std::cout << "messages sent: " << usage.ru_msgsnd << std::endl; // not supported
    // std::cout << "messages received: " << usage.ru_msgrcv << std::endl; // not supported
    // std::cout << "signals received: " << usage.ru_nsignals << std::endl; // not supported
    // std::cout << "voluntary context switches: " << usage.ru_nvcsw << std::endl;
    // std::cout << "involuntary context switches: " << usage.ru_nivcsw << std::endl;
    std::cout << "cpu time: " << (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000 +
                                 (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000 << " ms" << std::endl;
    std::cout << "max resident set size " << usage.ru_maxrss << " kb" << std::endl;
    std::cout << "physical memory used " << usage.ru_minflt * getPagesize() << " kb" << std::endl;
}

void setLimit(int limitType, int limit) {
    struct rlimit l;
    l.rlim_cur = l.rlim_max = limit;
    setrlimit(limitType, &l);
}

bool isFileEmpty(std::string name) {
    std::ifstream file(name);
    return file.peek() == std::ifstream::traits_type::eof();
}

int extractIntFromString(std::string string) {
    std::regex regex("\\d+");
    std::smatch result;
    std::regex_search(string, result, regex);
    // check the result has a item
    return std::stoi(result[0]);
}

bool getProcStatus(int pid, int procStatus[]) {
    char filePath[100];
    sprintf(filePath, "/proc/%d/status", pid);
    std::ifstream file(filePath);
    std::string vmPeak, vmHWM, vmRSS, vmData, vmStk, vmExe, vmLib;
    for (int i = 0; i < 17; i++) {
        std::getline(file, vmPeak);
    }
    for (int i = 0; i < 4; i++) {
        std::getline(file, vmHWM);
    }
    std::getline(file, vmRSS);
    for (int i = 0; i < 4; i++) {
        std::getline(file, vmData);
    }
    std::getline(file, vmStk);
    std::getline(file, vmExe);
    std::getline(file, vmLib);
    if (vmPeak.substr(0, 6) != "VmPeak") {
        std::cout << "VmPeak line error" << std::endl;
        return false;
    }
    if (vmHWM.substr(0, 5) != "VmHWM") {
        std::cout << "VmHWM line error" << std::endl;
        return false;
    }
    if (vmRSS.substr(0, 5) != "VmRSS") {
        std::cout << "VmRSS line error" << std::endl;
        return false;
    }
    if (vmData.substr(0, 6) != "VmData") {
        std::cout << "VmData line error" << std::endl;
        return false;
    }
    if (vmStk.substr(0, 5) != "VmStk") {
        std::cout << "VmStk line error" << std::endl;
        return false;
    }
    if (vmExe.substr(0, 5) != "VmExe") {
        std::cout << "VmExe line error" << std::endl;
        return false;
    }
    if (vmLib.substr(0, 5) != "VmLib") {
        std::cout << "VmLib line error" << std::endl;
        return false;
    }
    int VmPeak, VmHWM, VmRSS, VmData, VmStk, VmExe, VmLib;
    VmPeak = extractIntFromString(vmPeak);
    VmHWM = extractIntFromString(vmHWM);
    VmRSS = extractIntFromString(vmRSS);
    VmData = extractIntFromString(vmData);
    VmStk = extractIntFromString(vmStk);
    VmExe = extractIntFromString(vmExe);
    VmLib = extractIntFromString(vmLib);
    procStatus[0] = VmPeak;
    procStatus[1] = VmHWM;
    procStatus[2] = VmRSS;
    procStatus[3] = VmData;
    procStatus[4] = VmStk;
    procStatus[5] = VmExe;
    procStatus[6] = VmLib;
    return true;
}

int compile(std::string language, long &compileMemoryUsed, long &compileTimeUsed, int &result) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    } else if (pid == 0) {      //child
        if (language == "cpp") {
            struct rlimit compileTimeLimit, compileMemLimit;
            setLimit(RLIMIT_AS, COMPILE_MEM_LIMIT * MB);
            setLimit(RLIMIT_CPU, COMPILE_TIME_LIMIT);
            freopen(COMPILE_LOG, "w", stdout);
            freopen(COMPILE_ERROR_LOG, "w", stderr);
            const char *argv[] = {"g++", "main.cpp", "-o", "main", "-lm", "-w", "-O2", "-fmax-errors=3", "-std=c++14",
                                  "-static", NULL};
            alarm(0);
            alarm(COMPILE_TIME_LIMIT);
            execvp(argv[0], (char *const *) argv);
            // if execvp run succeed, the child process will quit, if not,the child process will continue to run below code
            return -1;      //never reach this line
        }
    } else {        //parent
        int childStatus;
        struct rusage usage;
        wait4(pid, &childStatus, 0, &usage);
        std::cout << "===============================================" << std::endl;
        // printUsage(usage);
        compileMemoryUsed = usage.ru_maxrss;
        compileTimeUsed = (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000 +
                          (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000;
        return childStatus;
    }
    return 0;
}

int run(std::string language, pid_t pid, int timeLimitValue, int memLimitValue, int &result) {
    if (language == "cpp") {
        freopen(DATA_IN, "r", stdin);
        freopen(DATA_OUT, "w", stdout);
        freopen(ERR_OUT, "w", stderr);
        ptrace(PTRACE_TRACEME, NULL, NULL, NULL);
        setLimit(RLIMIT_CPU, timeLimitValue / 1000 + 1); //seconds
        setLimit(RLIMIT_AS, memLimitValue * KB);    //the rlimit_as unit is byte
        setLimit(RLIMIT_FSIZE, OUTPUT_FILE_SIZE_LIMIT * MB);    //the rlimit_as unit is byte
        // maybe also need to add stack size limit and max process limit
        const char *argv[] = {"./main", "./main", nullptr};
        alarm(0);
        alarm(timeLimitValue / 1000 + 1);
        execve(argv[0], (char *const *) argv, NULL);
        return -1;
    }
    return 0;
}

void saveToFile(std::string filePath, std::string string) {
    std::ofstream dataout(filePath);
    if (dataout.is_open()) {
        dataout << string;
        dataout.close();
    }
}


std::string readFileToString(std::string file) {
    std::ifstream in(file);
    std::stringstream stringStream;
    stringStream << in.rdbuf();
    return stringStream.str();
}

void monitorChildProcess(std::string language, pid_t pid, int &result, long &memUsed, long &memUsed2, int &timeUsed,
                         long &physicalMemUsed,
                         int timeLimit, int memLimit, std::string &runtimeErrorMessage) {
    int status;
    rusage usage;
    bool isEnterSystemCall = true;
    long tempMemUsed = 0, tempMemUsed2 = 0, tempPhysicalMemUsed = 0;
    int tempTimeUsed = 0;
    struct user_regs_struct regs;
    wait4(pid, &status, WUNTRACED, &usage);   // the child process will stop when execvp
    if (WIFSTOPPED(status)) {
        std::cout << "child process get a stop signal: " << strsignal(WSTOPSIG(status)) << std::endl;
//        switch (WSTOPSIG(status)) {
//            case SIGTRAP:
//                std::cout << "child process get a stop signal: trap" << std::endl;
//                break;
//
//            default:    //never happen
//                std::cout << "child process got a stop signal: " << WSTOPSIG(status) << std::endl;
//                break;
//        }
    } else {
        std::cout << "child process didn't got any stop signal" << std::endl;
    }
    if (WIFEXITED(status)) {     //never happen
        std::cout << "child process exit before tracer" << std::endl;
        timeUsed = (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000 +
                   (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000;
        memUsed = usage.ru_maxrss;
        physicalMemUsed = usage.ru_minflt * getPagesize();   //KB
        return;
    }
    if (WIFSIGNALED(status)) {  //never happen
        std::cout << "child process get a end signal before tracer " << strsignal(WTERMSIG(status)) << std::endl;
        timeUsed = (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000 +
                   (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000;
        memUsed = usage.ru_maxrss;
        physicalMemUsed = usage.ru_minflt * getPagesize();   //KB
        return;
    }
    ptrace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_EXITKILL | PTRACE_O_TRACEEXIT);   // no PTRACE_O_TRACESYSGOOD
    ptrace(PTRACE_SYSCALL, pid, NULL, NULL);    // continue the child process
    while (true) {
        wait4(pid, &status, WUNTRACED, &usage);
        tempTimeUsed = (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000 +
                       (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000;
        tempMemUsed = usage.ru_maxrss;
        tempPhysicalMemUsed = usage.ru_minflt * getPagesize();
        timeUsed = timeUsed < tempTimeUsed ? tempTimeUsed : timeUsed;
        memUsed = memUsed < tempMemUsed ? tempMemUsed : memUsed;
        physicalMemUsed = physicalMemUsed < tempPhysicalMemUsed ? tempPhysicalMemUsed : physicalMemUsed;
        if (WIFEXITED(status)) {
            break;
        }
        if (WIFSIGNALED(status)) {
            std::cout << "child process get a term signal: " << strsignal(WTERMSIG(status)) << std::endl;
            switch (WTERMSIG(status)) {
                case SIGCHLD :
                case SIGALRM :
                    alarm(0);
                case SIGKILL :
                case SIGXCPU :
                    if (result == AC) {    // when RE, use kill signal to kill the child process, dont't think it is TLE
                        result = TLE;
                    }
                    break;
                case SIGXFSZ :
                    if (result == AC) {
                        result = OLE;
                    }
                    break;
                default :
                    result = RE;
            }
//            kill(pid, SIGKILL);
            break;
        }
        std::cout << "WEXITSTATUS: " << WEXITSTATUS(status) << " WTERMSIGNAL: " << WTERMSIG(status) << std::endl;
        if (WIFSTOPPED(status)) {
            std::cout << "child process get a stop signal: " << strsignal(WSTOPSIG(status)) << std::endl;
            switch (WSTOPSIG(status)) {
                case SIGALRM:
                    alarm(0);
                case SIGXCPU:
                    result = TLE;
                    kill(pid, SIGKILL);
                    break;
                case SIGFPE:
//                    std::cout << "child process get a float point error, mostly division by zero" << std::endl;
                    result = RE;
                    runtimeErrorMessage = "float point exception";
                    kill(pid, SIGKILL);
                    break;
                case SIGSEGV:
//                    std::cout << "child process accessing memory incorrectly." << std::endl;
                    result = RE;
                    runtimeErrorMessage = "segmentation fault";
                    kill(pid, SIGKILL);
                    break;
                case SIGXFSZ:
                    result = OLE;
                    kill(pid, SIGKILL);
                    break;
                default:
//                    std::cout << "child process got a stop signal: " << WSTOPSIG(status) << std::endl;
                    break;
            }
        }
        ptrace(PTRACE_GETREGS, pid, NULL, &regs);
        if (isEnterSystemCall) {
            std::cout << "child process enter a system call: " << SYSTEM_CALL(regs) << std::endl;
        } else {
            std::cout << "child process exit from a system call: " << SYSTEM_CALL(regs) << " return: " << RET(regs)
                      << std::endl;
        }
        isEnterSystemCall = !isEnterSystemCall;
        if (timeUsed > timeLimit) {
            kill(pid, SIGKILL);
        }
        if (memUsed > memLimit) {
            result = MLE;
            kill(pid, SIGKILL);
        }
        if (getFileSize(DATA_OUT) > OUTPUT_FILE_SIZE_LIMIT * MB) {
            result = OLE;
            kill(pid, SIGKILL);
        }
        int procStatus[7] = {0};
        if (getProcStatus(pid, procStatus)) {   // just get the information, not using it
            std::cout << "VmPeak: " << procStatus[0] << " VmHWM: " << procStatus[1] << " VmRSS: " << procStatus[2]
                      << " VmData: " << procStatus[3] << " VmStk: " << procStatus[4] << " VmExe: " << procStatus[5]
                      << " VmLib: " << procStatus[6] << std::endl;
            tempMemUsed2 = procStatus[0] - procStatus[4] - procStatus[5] - procStatus[6];
            memUsed2 = memUsed2 < tempMemUsed2 ? tempMemUsed2 : memUsed2;
        } else {
            std::cout << "get proc status failed" << std::endl;
        }
        std::cout << "memory: " << memUsed << " KB " << "physical memory: " << physicalMemUsed << " KB" << " memory 2: "
                  << memUsed2 << " time: " << timeUsed << std::endl;
        std::cout << "===============================================" << std::endl;
        ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
    }
}

long getPagesize() {
    return sysconf(_SC_PAGESIZE) >> 10;     //KB
}

// all the infomation in database is correct, don't need to check them
int main(int argc, char *argv[]) {
    std::string submitID = argv[1];
    std::string problemID = argv[2];
    std::cout << "submit id: " << submitID << std::endl;
    std::cout << "problem id: " << problemID << std::endl;
    mongocxx::instance instance{};      // This should be done only once
    mongocxx::client connection{
            mongocxx::uri{MONGO_URI}}; // connection mongo server runing on localhost on port 27017(default)
    mongocxx::collection problemCollection = connection["oj"]["problems"];
    mongocxx::collection submitCollection = connection["oj"]["submits"];
    bsoncxx::stdx::optional <bsoncxx::document::value> submit = submitCollection.find_one(
            document{} << "_id" << bsoncxx::oid(submitID) << finalize);
    std::cout << bsoncxx::to_json(*submit) << std::endl;
    bsoncxx::document::view submitView = (*submit).view();
    std::string language = getUtf8ValueFromDocument(submitView, "language");
    std::string code = getUtf8ValueFromDocument(submitView, "code");
    saveCodeToFile(language, code);
    bsoncxx::stdx::optional <bsoncxx::document::value> problem = problemCollection.find_one(
            document{} << "_id" << bsoncxx::oid(problemID) << finalize); //find problem using problem id
    std::cout << bsoncxx::to_json(*problem) << std::endl;
    bsoncxx::document::view problemView = (*problem).view();
    long compileMemoryUsed = 0, compileTimeUsed = 0;  //kb and ms
    std::string compileErrorMessage = "";
    std::string runtimeErrorMessage = "";
    int result = AC;
    updateByID(submitCollection, submitID, "status", "compiling");
    int status = compile(language, compileMemoryUsed, compileTimeUsed, result);
    if (!WIFEXITED(status) ||
        !isFileEmpty(COMPILE_ERROR_LOG)) {     //not exit normally or compile error file is not empty
        if (!WIFEXITED(status)) {
            if (WIFSIGNALED(status)) {
                switch (WTERMSIG(status)) {
                    case SIGALRM:
                        alarm(0);
                    case SIGXCPU:
                        std::cout << "compile time exceeded" << std::endl;
                        compileErrorMessage = "compile time exceeded";
                        break;
                    default:
                        std::cout << "unknown signal " << WTERMSIG(status) << " exit code is: " << WEXITSTATUS(status)
                                  << std::endl;
                        compileErrorMessage = "exit code is " + WEXITSTATUS(status);
                        break;
                }
            } else {
                // never reach here
                std::cout << "compile process exit not normally but not killed by signal" << std::endl;
            }
        } else {
            if (!isFileEmpty(COMPILE_ERROR_LOG)) {
                compileErrorMessage = readFileToString(COMPILE_ERROR_LOG);
                std::cout << compileErrorMessage << std::endl;
            }
        }
        updateStatusAndMessage(submitCollection, submitID, RESULT[CE], compileErrorMessage);
        result = CE;
        std::cout << "Compile Error" << std::endl;
        return 0;
    } else if (status == -1) {
        // if the execvp function in compile function run error, then return code is -1;
        compileErrorMessage = "compile fork error";
        updateStatusAndMessage(submitCollection, submitID, RESULT[SYSTEM_ERROR], compileErrorMessage);
        result = SYSTEM_ERROR;
        std::cout << "server error" << std::endl;
        return 0;
    } else { // compile success, start running and judging
        std::cout << "compile memory used: " << compileMemoryUsed << " kb. compile time used: " << compileTimeUsed
                  << " ms" << std::endl;
        std::cout << "compile success, and start judging" << std::endl
                  << "===============================================" << std::endl;
        updateByID(submitCollection, submitID, "status", "running");
        int timeLimitValue = getInt32ValueFromDocument(problemView, "timeLimit");
        int memLimitValue = getInt32ValueFromDocument(problemView, "memLimit");
        if (timeLimitValue == -1 || memLimitValue == -1) {
            updateStatusAndMessage(submitCollection, submitID, RESULT[SYSTEM_ERROR],
                                   "time limit or memory limit for problem" + problemID + "is not correct");
            return 0;
        }
        int timeUsed = 0;   //ms
        long memUsed = 0, physicalMemUsed = 0, memUsed2 = 0;  // kb
        bsoncxx::document::element testArray = problemView["test"];
        if (testArray.type() != bsoncxx::type::k_array) {
            std::cout << "test is not array, db error!" << std::endl;
            updateStatusAndMessage(submitCollection, submitID, RESULT[SYSTEM_ERROR],
                                   "test is not array, problem " + problemID);
            return 0;
        }
        bsoncxx::array::view testArrayView = testArray.get_array().value;
        std::string wrongAnswerString = "";
        for (bsoncxx::array::element test : testArrayView) {
            if (result != AC) {
                break;
            }
            if (test.type() != bsoncxx::type::k_document) {
                std::cout << "test element is not a object, db error!" << std::endl;
                updateStatusAndMessage(submitCollection, submitID, RESULT[SYSTEM_ERROR],
                                       "test element is not a object, " + problemID);
                return 0;
            }
            bsoncxx::document::view testView = test.get_document().value;
            std::string inputString = getUtf8ValueFromDocument(testView, "input");
            std::string expectedOutput = getUtf8ValueFromDocument(testView, "output");
            if (inputString.empty() || expectedOutput.empty()) {
                updateStatusAndMessage(submitCollection, submitID, RESULT[SYSTEM_ERROR],
                                       "input or expectedOutput is not string, problem " + problemID);
                std::cout << "input or expectedOutput is not string when reading from the db, problem id:" << std::endl;
                return 0;
            }
            std::cout << "test for input " << inputString << std::endl;
            saveToFile(DATA_IN, inputString);
            struct timeval beginTime, endTime;
            gettimeofday(&beginTime, NULL);
            pid_t pid = fork();
            if (pid == -1) {
                std::cout << "fork error" << std::endl;
                updateStatusAndMessage(submitCollection, submitID, RESULT[SYSTEM_ERROR], "run fork error");
                result = SYSTEM_ERROR;
                return 0;
            } else if (pid == 0) {// child
                run(language, pid, timeLimitValue, memLimitValue, result);
            } else {    // parent
                monitorChildProcess(language, pid, result, memUsed, memUsed2, timeUsed, physicalMemUsed,
                                    timeLimitValue, memLimitValue, runtimeErrorMessage);
                gettimeofday(&endTime, NULL);
                int realTime = endTime.tv_sec * 1000 + endTime.tv_usec / 1000 - beginTime.tv_sec * 1000 -
                               beginTime.tv_usec / 1000;
                std::cout << "real time: " << realTime << std::endl;
                std::cout << "memory: " << memUsed << " KB  " << "physical memory: " << physicalMemUsed
                          << " KB " << "memory 2: " << memUsed2 << " time: " << timeUsed << std::endl;
                switch (result) {
                    case RE: {
                        if (!isFileEmpty(ERR_OUT)) {
                            runtimeErrorMessage = readFileToString(ERR_OUT);
                            std::cout << runtimeErrorMessage << std::endl;
                        }
                        break;
                    }
                    case AC: {
                        std::string realOutput = readFileToString(DATA_OUT);
                        wrongAnswerString = "input: " + inputString + "expected: " + expectedOutput + "real: " + realOutput;
                        std::cout << wrongAnswerString << std::endl;
                        if (!compare(expectedOutput, realOutput)) {
                            result = WA;
                            std::cout << "WRONG ANSWER" << std::endl;
                            std::cout << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" << std::endl;
                        } else {
                            std::cout << "===============================================" << std::endl;
                        }
                        break;
                    }
                    case TLE: {
                        timeUsed = timeLimitValue;
                        break;
                    }
                    case MLE: {
                        memUsed = memLimitValue;
                    }
                    default:
                        break;
                }
            }
        }
        if (result == AC || result == TLE || result == MLE || result == OLE) {
            updateStatusAndMemAndTimeAndMessage(submitCollection, submitID, RESULT[result], memUsed, timeUsed, "");
        } else if (result == WA) {
            updateStatusAndMemAndTimeAndMessage(submitCollection, submitID, RESULT[WA], memUsed, timeUsed,
                                                wrongAnswerString);
        } else if (result == RE) {
            updateStatusAndMemAndTimeAndMessage(submitCollection, submitID, RESULT[RE], memUsed, timeUsed,
                                                runtimeErrorMessage);
        }
        std::cout << RESULT[result] << std::endl;
        return 0;
    }
}

long getFileSize(std::string filename) {
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}


bool compare(std::string expected, std::string real) {
    return expected == real;
}

void saveCodeToFile(std::string language, std::string code) {
    if (language == "cpp") {
        saveToFile(CPP_CODE_FILE, code);
    } else if (language == "c") {
        saveToFile(C_CODE_FILE, code);
    }
}

void updateByID(mongocxx::collection collection, std::string id, std::string key, std::string value) {
    collection.update_one(document{} << "_id" << bsoncxx::oid(id) << finalize,
                          document{} << "$set" << open_document <<
                                     key << value << close_document
                                     << finalize);
}

void updateStatusAndMessage(mongocxx::collection collection, std::string id, std::string status, std::string message) {
    collection.update_one(document{} << "_id" << bsoncxx::oid(id) << finalize,
                          document{} << "$set" << open_document <<
                                     "status" << status <<
                                     "message" << message << close_document << finalize);
}

void updateStatusAndMemAndTimeAndMessage(mongocxx::collection collection, std::string id, std::string status,
                                         long mem, long time, std::string message) {
    collection.update_one(document{} << "_id" << bsoncxx::oid(id) << finalize,
                            document{} << "$set" << open_document <<
                                        "status" << status <<
                                        "time" << time <<
                                        "memory" << mem <<
                                        "message" << message << close_document << finalize);

}