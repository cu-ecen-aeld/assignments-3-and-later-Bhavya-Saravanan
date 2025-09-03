// finder-app/writer.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

int main(int argc, char *argv[]) {
    // Open syslog with facility LOG_USER and tag "writer"
    openlog("writer", LOG_PID, LOG_USER);

    // Expect exactly two arguments: <file> <string>
    if (argc != 3) {
        syslog(LOG_ERR, "Usage: %s <file> <string>", argv[0]);
        closelog();
        return 1;
    }

    const char *filepath = argv[1];
    const char *text     = argv[2];

    // Required debug log
    syslog(LOG_DEBUG, "Writing %s to %s", text, filepath);

    // DO NOT create directories; assume caller created them
    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        // %m expands to strerror(errno)
        syslog(LOG_ERR, "Failed to open %s: %m", filepath);
        closelog();
        return 1;
    }

    if (fputs(text, fp) == EOF) {
        syslog(LOG_ERR, "Failed to write to %s: %m", filepath);
        fclose(fp);
        closelog();
        return 1;
    }

    if (fclose(fp) == EOF) {
        syslog(LOG_ERR, "Failed to close %s: %m", filepath);
        closelog();
        return 1;
    }

    closelog();
    return 0;  // success
}

