/**
 * Loco program runner core
        Last modified: 2016-11-03 17:50:34
        Filename: lorun/cext/run.c
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "run.h"
#include "proc.h"
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>
#include <fcntl.h>
#include "access.h"
#include "limit.h"

const char *last_run_err;
#define RAISE_RUN(err) {last_run_err = err;return -1;}

int traceLoop(struct Runobj *runobj, struct Result *rst, pid_t pid) {
    long memory;
    int status, incall = 0;
    struct rusage ru;
    struct user_regs_struct regs;
    
    rst->memory_used = get_proc_status(pid, "VmRSS:");

    rst->judge_result = AC;

    while (1)
    {
        if (wait4(pid, &status, 0, &ru) == -1)
            RAISE_RUN("wait4 [WSTOPPED] failure");

        //Get memory
        if (runobj->java)
            memory = get_page_fault_mem(ru, pid);
        else
            memory = get_proc_status(pid, "VmPeak:");
        if (memory > rst->memory_used)
            rst->memory_used = memory;

        //Check mempry
        if (rst->memory_used > runobj->memory_limit)
        {
            rst->judge_result = MLE;
            ptrace(PTRACE_KILL, pid, NULL, NULL);
            break;
        }

        //If exited 
        if (WIFEXITED(status))
            break;
    
        //Get exitcode
        int exitcode = WEXITSTATUS(status);
        /**
            exitcode == 5 waiting for next CPU allocation
            ruby using system to run, exit 17 ok
        **/
        if (runobj->java || exitcode == 0x05 || exitcode == 0)
            ;
        else {
            if (rst->judge_result == AC)
            {
                switch (exitcode)
                {
                    case SIGCHLD:
                    case SIGALRM:
                        rst->judge_result = MLE;
                    case SIGKILL:
                    case SIGXCPU:
                        rst->judge_result = TLE;
                        break;
                    case SIGXFSZ:
                        rst->judge_result = OLE;
                        break;
                    default:
                        rst->judge_result = RE;
                }
                rst->re_signum = WTERMSIG(status);
            }
            ptrace(PTRACE_KILL, pid, NULL, NULL);
            break;
        }

        /*  WIFSIGNALED: if the process is terminated by signal */
        if (WIFSIGNALED(status))
        {
            int sig = WTERMSIG(status);
            if (rst->judge_result == AC)
            {
                switch (sig)
                {
                    case SIGCHLD:
                    case SIGALRM:
                        rst->judge_result = MLE;
                    case SIGKILL:
                    case SIGXCPU:
                        rst->judge_result = TLE;
                        break;
                    case SIGXFSZ:
                        rst->judge_result = OLE;
                        break;
                    default:
                        rst->judge_result = RE;
                }
                rst->re_signum = WTERMSIG(status);
            }
            break;
        }

        if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1)
            RAISE_RUN("PTRACE_GETREGS failure");
        if (incall && runobj->trace && !runobj->java)
        {
            int ret = checkAccess(runobj, pid, &regs);
            if (ret != ACCESS_OK)
            {
                ptrace(PTRACE_KILL, pid, NULL, NULL);
                waitpid(pid, NULL, 0);

                rst->judge_result = RE;
                if (ret == ACCESS_CALL_ERR) {
                    rst->re_call = REG_SYS_CALL(&regs);
                } else {
                    rst->re_file = lastFileAccess();
                    rst->re_file_flag = REG_ARG_2(&regs);
                }
                return 0;
            }
            incall = 0;
        } else
            incall = 1;
        ptrace(PTRACE_SYSCALL, pid, NULL, NULL);

    }
	//Don't forget this !!!
	rst->time_used = ru.ru_utime.tv_sec * 1000
        + ru.ru_utime.tv_usec / 1000
        + ru.ru_stime.tv_sec * 1000
        + ru.ru_stime.tv_usec / 1000;
    return 0;
}

int runit(struct Runobj *runobj, struct Result *rst) {
    pid_t pid;
    int fd_err[2];

    if (pipe2(fd_err, O_NONBLOCK))
        RAISE1("run :pipe2(fd_err) failure");

    pid = fork();
    if (pid < 0) {
        close(fd_err[0]);
        close(fd_err[1]);
        RAISE1("run : vfork failure");
    }

    if (pid == 0) {
        close(fd_err[0]);

#define RAISE_EXIT(err) {\
            int r = write(fd_err[1],err,strlen(err));\
            _exit(r);\
        }

        if (runobj->fd_in != -1)
            if (dup2(runobj->fd_in, 0) == -1)
                RAISE_EXIT("dup2 stdin failure!")

        if (runobj->fd_out != -1)
            if (dup2(runobj->fd_out, 1) == -1)
                RAISE_EXIT("dup2 stdout failure")

        if (runobj->fd_err != -1)
            if (dup2(runobj->fd_err, 2) == -1)
                RAISE_EXIT("dup2 stderr failure")

        if (setResLimit(runobj) == -1)
            RAISE_EXIT(last_limit_err)

        if (runobj->runner != -1)
            if (setuid(runobj->runner))
                RAISE_EXIT("setuid failure")

        if (runobj->trace)
            if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1)
                RAISE_EXIT("TRACEME failure")
        alarm(5);
        execvp(runobj->args[0], (char * const *) runobj->args);

        RAISE_EXIT("execvp failure")
    } else {
        int r;
        char errbuffer[100] = { 0 };

        close(fd_err[1]);
        r = read(fd_err[0], errbuffer, 90);
        close(fd_err[0]);
        if (r > 0) {
            waitpid(pid, NULL, WNOHANG);
            RAISE(errbuffer);
            return -1;
        }

        //A hack to warning ...
        r = nice(19);

        r = traceLoop(runobj, rst, pid);

        if (r)
            RAISE1(last_run_err);
        return 0;
    }
}

