/*
 * umount.mynfs - direct umount syscall wrapper for mynfs filesystem
 * Calls umount2() directly, bypassing libmount helper lookup.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <getopt.h>
#include <errno.h>

#define EX_SUCCESS 0
#define EX_FAIL    32
#define EX_USAGE   64

static void usage(void)
{
    fprintf(stderr, "Usage: umount.mynfs [-fl] device|dir\n");
    exit(EX_USAGE);
}

int main(int argc, char *argv[])
{
    int c;
    int flags = 0;

    while ((c = getopt(argc, argv, "flrvVh")) != -1) {
        switch (c) {
        case 'f': flags |= MNT_FORCE; break;
        case 'l': flags |= MNT_DETACH; break;
        case 'r': /* remount ro - not supported here */ break;
        case 'v': break;
        case 'V':
        case 'h':
        default:
            usage();
        }
    }

    if (argc - optind != 1)
        usage();

    if (umount2(argv[optind], flags) != 0) {
        fprintf(stderr, "umount.mynfs: %s: %s\n", argv[optind], strerror(errno));
        return EX_FAIL;
    }

    return EX_SUCCESS;
}
