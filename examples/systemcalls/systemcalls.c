#include "systemcalls.h"

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <syslog.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/

   openlog("systemcalls.c", LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER);

    if (cmd == NULL) {
        syslog(LOG_ERR, "do_system: command is NULL, nothing to run");
        closelog();
        return false;
    }
 
    int rc = system(cmd);
    if (rc == -1) {
        syslog(LOG_ERR, "do_system: system() failed to start command, errno=%d (%m)", errno);
        closelog();
        return false;
    }

   if (WIFEXITED(rc) && (WEXITSTATUS(rc) == 0))
    {
        syslog(LOG_DEBUG,"do_system: command '%s' ran successfully with exit=0", cmd);
        closelog();
        return true;
    }
    else
    {
        syslog(LOG_ERR,"do_system: command '%s' did not exit normally (status=0x%x)", cmd, rc);
        closelog();
        return false;
    }
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{


/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
 
    openlog("systemcalls.c", LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER);

    va_list args;
    va_start(args, count);

    char *command[count + 1];
    for (int i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
   
    fflush(NULL);

    pid_t pid = fork();
    if (pid < 0) {
        va_end(args);
        syslog(LOG_ERR, "do_exec: fork() failed, errno=%d (%m)", errno);
        closelog();
        return false;
    }

    if (pid == 0) {
        execv(command[0], command);
        syslog(LOG_ERR, "do_exec: execv('%s', ...) failed, errno=%d (%m)", command[0], errno);
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        va_end(args);
        syslog(LOG_ERR, "do_exec: waitpid() failed, errno=%d (%m)", errno);
        closelog();
        return false;
    }

    va_end(args);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        syslog(LOG_DEBUG, "do_exec: child '%s' exited normally with status=0", command[0]);
        closelog();
        return true;
    } else {
        syslog(LOG_ERR, "do_exec: child '%s' exited abnormally (status=0x%x)", command[0], status);
        closelog();
        return false;
    }
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/

bool do_exec_redirect(const char *outputfile, int count, ...)
{


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

openlog("systemcalls.c", LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER);

    if (outputfile == NULL || count < 1) {
        syslog(LOG_ERR, "do_exec_redirect: bad args\n");
        closelog();
        return false;
    }

    va_list args;
    va_start(args, count);

     char *command[count + 1];
    for (int i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    int fd = open(outputfile, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) {
        va_end(args);
        syslog(LOG_ERR, "do_exec_redirect: open('%s') failed with errno=%d\n", outputfile, errno);
        closelog();
        return false;
    }

    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0) {
        va_end(args);
        syslog(LOG_ERR, "do_exec_redirect: fork failed with errno=%d\n", errno);
        close(fd);
        closelog();
        return false;
    }

    if (pid == 0) {
        if (dup2(fd, STDOUT_FILENO) < 0) {
            syslog(LOG_ERR, "do_exec_redirect: dup2 failed with errno=%d\n", errno);
            close(fd);
            _exit(126);
        }
        close(fd);
        execv(command[0], command);
        syslog(LOG_ERR, "do_exec_redirect: execv failed with errno=%d\n", errno);
        _exit(127);
    }

    close(fd);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        va_end(args);
        syslog(LOG_ERR, "do_exec_redirect: waitpid() failed, errno=%d (%m)", errno);
        closelog();
        return false;
    }

    va_end(args);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        syslog(LOG_DEBUG, "do_exec_redirect: child '%s' exited normally with status=0", command[0]);
        closelog();
        return true;
    }else{
        syslog(LOG_ERR, "do_exec_redirect: child '%s' exited abnormally (status=0x%x)", command[0], status);
        closelog();
        return false;
    }
    
}
