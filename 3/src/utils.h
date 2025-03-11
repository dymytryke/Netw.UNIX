#ifndef UTILS_H
#define UTILS_H

#include <string.h>

// Function to validate a filename (no path separators or invalid characters)
static inline int validate_filename(const char *filename) {
    // Check for null or empty filename
    if (!filename || !*filename) {
        return 0;
    }
    
    // Check for path separators
    if (strchr(filename, '/') != NULL) {
        return 0;
    }
    
    // Check for "." or ".." which could be used to navigate directory structure
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
        return 0;
    }

    // Basic check for valid filename characters
    for (const char *p = filename; *p; p++) {
        if (*p <= 31 || *p == 127) {  // Control characters
            return 0;
        }
        
        // This is a simplified check - actual filesystem restrictions may vary
        if (strchr("<>:\"|?*\\", *p) != NULL) {
            return 0;
        }
    }
    
    return 1;
}

#endif // UTILS_H