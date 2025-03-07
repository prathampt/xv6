#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

void test_lseek() {
    int fd;
    char buffer[20];
    int offset;

    printf(1, "\n======== TESTING LSEEK SYSTEM CALL ========\n");

    // Create and open a test file
    fd = open("testfile.txt", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(1, "Error: Could not open test file!\n");
        exit();
    }

    // Write data to the file
    write(fd, "Hello, xv6 LSEEK!", 18);

    // Test SEEK_SET (absolute positioning)
    offset = lseek(fd, 6, SEEK_SET);
    if (offset != 6) printf(1, "Error: SEEK_SET failed!\n");

    read(fd, buffer, 5);
    buffer[5] = '\0';
    printf(1, "SEEK_SET(6) and read 5 bytes: %s (Expected: xv6 )\n", buffer);

    // Test SEEK_CUR (relative positioning)
    offset = lseek(fd, -2, SEEK_CUR);
    if (offset != 9) printf(1, "Error: SEEK_CUR failed!\n");

    read(fd, buffer, 3);
    buffer[3] = '\0';
    printf(1, "SEEK_CUR(-2) and read 3 bytes: %s (Expected: 6 L)\n", buffer);

    // Test SEEK_END (end positioning)
    offset = lseek(fd, -4, SEEK_END);
    if (offset < 0) printf(1, "Error: SEEK_END failed!\n");

    read(fd, buffer, 5);
    buffer[5] = '\0';
    printf(1, "SEEK_END(-4) and read: %s (Expected: EK! ) offset: %d\n", buffer, offset);

    // Test invalid seeks
    if (lseek(fd, -100, SEEK_SET) != -1) printf(1, "Error: Negative SEEK_SET should fail!\n");
    if (lseek(fd, 100, SEEK_CUR) != -1) printf(1, "Error: Large SEEK_CUR should fail!\n");
    if (lseek(fd, 1000, SEEK_END) != -1) printf(1, "Error: SEEK beyond EOF should fail!\n");

    // Test lseek on invalid file descriptor
    if (lseek(100, 5, SEEK_SET) != -1) printf(1, "Error: Invalid FD should fail!\n");

    close(fd);
    unlink("testfile.txt");

    printf(1, "\n======== ALL TESTS COMPLETED ========\n");
}

int main() {
    test_lseek();
    exit();
}
