/*
 * mount.mynfs - Lightweight NFS mount helper for the mynfs kernel module
 *
 * Constructs binary mount data structures (nfs4_mount_data / nfs_mount_data)
 * and invokes mount(2). Based on nfs-utils mount.nfs logic.
 *
 * Filesystem type: "mynfs" (NFSv4) or "mynfs" with vers=3 (NFSv3)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#ifndef __user
#define __user
#endif
#include "nfs4_mount.h"
#include "nfs_mount.h"

#define NFS_PORT        2049
#define MNT_PORT        0       /* let kernel decide */
#define EX_SUCCESS      0
#define EX_FAIL         32
#define EX_USAGE        64

/* NFSv4 mount flags (mirror values from nfs_mount.h) */
#define MYNFS4_MOUNT_SOFT       0x0001
#define MYNFS4_MOUNT_INTR       0x0002
#define MYNFS4_MOUNT_NOCTO      0x0010
#define MYNFS4_MOUNT_NOAC       0x0020
#define MYNFS4_MOUNT_STRICTLOCK 0x1000
#define MYNFS4_MOUNT_UNSHARED   0x8000
#define MYNFS4_MOUNT_FLAGMASK   0xFFFF

/* NFSv3 mount flags — must match kernel's include/uapi/linux/nfs_mount.h */
#define MYNFS_MOUNT_SOFT        0x0001
#define MYNFS_MOUNT_INTR        0x0002
#define MYNFS_MOUNT_POSIX       0x0008
#define MYNFS_MOUNT_NOCTO       0x0010
#define MYNFS_MOUNT_NOAC        0x0020
#define MYNFS_MOUNT_TCP         0x0040
#define MYNFS_MOUNT_VER3        0x0080
#define MYNFS_MOUNT_KERBEROS    0x0100
#define MYNFS_MOUNT_NONLM       0x0200
#define MYNFS_MOUNT_BROKEN_SUID 0x0400
#define MYNFS_MOUNT_NOACL       0x0800
#define MYNFS_MOUNT_FLAGMASK    0xFFFF

static const char *progname = "mount.mynfs";
static int verbose = 0;
static int sloppy = 0;

static void nfs_error(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s: ", progname);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

/* Parse "server:/path" into hostname and directory */
static int parse_devname(char *spec, char **hostname, char **dirname)
{
    char *p;

    /* Handle [ipv6]:/path */
    if (*spec == '[') {
        p = strchr(spec, ']');
        if (!p) {
            nfs_error("bad server specification: %s\n", spec);
            return -1;
        }
        *p = '\0';
        *hostname = spec + 1;
        if (*(p + 1) == ':')
            *dirname = p + 2;
        else
            *dirname = "/";
        return 0;
    }

    p = strchr(spec, ':');
    if (!p) {
        nfs_error("no server path given: %s\n", spec);
        return -1;
    }
    *p = '\0';
    *hostname = spec;
    *dirname = p + 1;
    if (!**dirname)
        *dirname = "/";
    return 0;
}

/* Resolve hostname and fill sockaddr_in */
static int fill_ipv4_sockaddr(const char *hostname, struct sockaddr_in *sap)
{
    struct addrinfo hints, *res;
    int ret;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo(hostname, NULL, &hints, &res);
    if (ret) {
        nfs_error("failed to resolve hostname %s: %s\n",
                  hostname, gai_strerror(ret));
        return -1;
    }

    memcpy(sap, res->ai_addr, sizeof(*sap));
    freeaddrinfo(res);
    return 0;
}

/* Get local IPv4 address for clientaddr */
static int get_my_ipv4addr(char *ip_addr, int len)
{
    int sock;
    struct sockaddr_in local;
    struct sockaddr_in remote;
    socklen_t slen;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        return -1;

    memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_port = htons(NFS_PORT);
    remote.sin_addr.s_addr = inet_addr("8.8.8.8"); /* dummy for routing */

    if (connect(sock, (struct sockaddr *)&remote, sizeof(remote)) < 0) {
        close(sock);
        return -1;
    }

    slen = sizeof(local);
    if (getsockname(sock, (struct sockaddr *)&local, &slen) < 0) {
        close(sock);
        return -1;
    }
    close(sock);

    strncpy(ip_addr, inet_ntoa(local.sin_addr), len - 1);
    ip_addr[len - 1] = '\0';
    return 0;
}

/*
 * NFSv4 mount - constructs nfs4_mount_data and calls mount(2)
 */
static int nfs4mount(const char *spec, const char *node, int flags,
                     char *extra_opts)
{
    static struct nfs4_mount_data data;
    static char hostdir[1024];
    static char ip_addr[16] = "127.0.0.1";
    static struct sockaddr_in server_addr;
    static int pseudoflavour = 0; /* AUTH_UNIX = 0 */

    char *hostname, *dirname;
    char *opt, *opteq;
    int val;
    int soft, intr, nocto, noac, unshared;
    int retry = 2;
    int bg = 0;
    time_t timeout;

    if (strlen(spec) >= sizeof(hostdir)) {
        nfs_error("excessively long host:dir argument\n");
        return EX_FAIL;
    }
    strcpy(hostdir, spec);
    if (parse_devname(hostdir, &hostname, &dirname))
        return EX_FAIL;

    if (fill_ipv4_sockaddr(hostname, &server_addr))
        return EX_FAIL;

    get_my_ipv4addr(ip_addr, sizeof(ip_addr));

    /* Defaults */
    memset(&data, 0, sizeof(data));
    data.retrans    = 3;
    data.acregmin   = 3;
    data.acregmax   = 60;
    data.acdirmin   = 30;
    data.acdirmax   = 60;
    data.proto      = IPPROTO_TCP;
    server_addr.sin_port = htons(NFS_PORT);

    soft = 0;
    intr = MYNFS4_MOUNT_INTR;
    nocto = 0;
    noac = 0;
    unshared = 0;

    /* Parse options */
    if (extra_opts && *extra_opts) {
        char *opts_copy = strdup(extra_opts);
        for (opt = strtok(opts_copy, ","); opt; opt = strtok(NULL, ",")) {
            if ((opteq = strchr(opt, '='))) {
                val = atoi(opteq + 1);
                *opteq = '\0';
                if (!strcmp(opt, "rsize"))
                    data.rsize = val;
                else if (!strcmp(opt, "wsize"))
                    data.wsize = val;
                else if (!strcmp(opt, "timeo"))
                    data.timeo = val;
                else if (!strcmp(opt, "retrans"))
                    data.retrans = val;
                else if (!strcmp(opt, "acregmin"))
                    data.acregmin = val;
                else if (!strcmp(opt, "acregmax"))
                    data.acregmax = val;
                else if (!strcmp(opt, "acdirmin"))
                    data.acdirmin = val;
                else if (!strcmp(opt, "acdirmax"))
                    data.acdirmax = val;
                else if (!strcmp(opt, "actimeo")) {
                    data.acregmin = val;
                    data.acregmax = val;
                    data.acdirmin = val;
                    data.acdirmax = val;
                }
                else if (!strcmp(opt, "retry"))
                    retry = val;
                else if (!strcmp(opt, "port"))
                    server_addr.sin_port = htons(val);
                else if (!strcmp(opt, "proto")) {
                    if (!strncmp(opteq+1, "tcp", 3))
                        data.proto = IPPROTO_TCP;
                    else if (!strncmp(opteq+1, "udp", 3))
                        data.proto = IPPROTO_UDP;
                }
                else if (!strcmp(opt, "clientaddr")) {
                    strncpy(ip_addr, opteq+1, sizeof(ip_addr)-1);
                    ip_addr[sizeof(ip_addr)-1] = '\0';
                }
                else if (!strcmp(opt, "sec")) {
                    /* AUTH_UNIX = 0 (default), skip for now */
                }
                /* addr= is added by us, ignore */
                else if (!strcmp(opt, "addr") || sloppy) {
                    /* ignore */
                }
                else if (!strcmp(opt, "vers") || !strcmp(opt, "nfsvers")) {
                    /* already determined fs type, ignore */
                }
                else if (!strcmp(opt, "fg") || !strcmp(opt, "bg")) {
                    /* handled separately */
                }
                else {
                    if (verbose)
                        fprintf(stderr, "unknown option: %s=%d\n", opt, val);
                }
            } else {
                val = 1;
                if (!strncmp(opt, "no", 2)) {
                    val = 0;
                    opt += 2;
                }
                if (!strcmp(opt, "bg"))
                    bg = val;
                else if (!strcmp(opt, "fg"))
                    bg = !val;
                else if (!strcmp(opt, "soft"))
                    soft = val;
                else if (!strcmp(opt, "hard"))
                    soft = !val;
                else if (!strcmp(opt, "intr"))
                    intr = val;
                else if (!strcmp(opt, "cto"))
                    nocto = !val;
                else if (!strcmp(opt, "ac"))
                    noac = !val;
                else if (!strcmp(opt, "sharecache"))
                    unshared = !val;
            }
        }
        free(opts_copy);
    }

    data.flags = (soft ? MYNFS4_MOUNT_SOFT : 0)
        | (intr ? MYNFS4_MOUNT_INTR : 0)
        | (nocto ? MYNFS4_MOUNT_NOCTO : 0)
        | (noac ? MYNFS4_MOUNT_NOAC : 0)
        | (unshared ? MYNFS4_MOUNT_UNSHARED : 0);

    data.auth_flavourlen = 0;
    data.auth_flavours = NULL;

    data.client_addr.data = ip_addr;
    data.client_addr.len = strlen(ip_addr);
    data.mnt_path.data = dirname;
    data.mnt_path.len = strlen(dirname);
    data.hostname.data = hostname;
    data.hostname.len = strlen(hostname);
    data.host_addr = (struct sockaddr *)&server_addr;
    data.host_addrlen = sizeof(server_addr);
    data.version = NFS4_MOUNT_VERSION;

    timeout = time(NULL) + 60 * retry;

    if (verbose) {
        fprintf(stderr, "mounting %s on %s (type mynfs4)\n", spec, node);
        fprintf(stderr, "  server=%s, port=%d, proto=%s\n",
                hostname, ntohs(server_addr.sin_port),
                data.proto == IPPROTO_TCP ? "tcp" : "udp");
    }

    for (;;) {
#ifdef __linux__
        int result = mount(spec, node, "mynfs4", flags, &data);
#else
        int result = 0; /* stub for non-Linux syntax check */
#endif
        if (result == 0)
            return EX_SUCCESS;

        if (errno == EBUSY)
            return EX_FAIL;

        if (bg) {
            if (time(NULL) > timeout) {
                nfs_error("giving up after %d retries\n", retry);
                return EX_FAIL;
            }
            fprintf(stderr, "  retrying (bg)...\n");
            sleep(1);
            continue;
        }

        nfs_error("mount(%s): %s\n", spec, strerror(errno));
        return EX_FAIL;
    }
}

/*
 * NFSv3 mount - constructs nfs_mount_data and calls mount(2)
 */
static int nfsmount(const char *spec, const char *node, int flags,
                    char *extra_opts)
{
    static struct nfs_mount_data data;
    static char hostdir[1024];
    static char ip_addr[16] = "127.0.0.1";
    static struct sockaddr_in server_addr, mnt_server_addr;
    static int pseudoflavour = 0;

    char *hostname, *dirname;
    char *opt, *opteq;
    int val;
    int soft, intr, nocto, noac, nonlm;
    int retry = 2;
    int bg = 0;
    time_t timeout;
    int use_tcp = 1;
    int mountport = MNT_PORT;

    if (strlen(spec) >= sizeof(hostdir)) {
        nfs_error("excessively long host:dir argument\n");
        return EX_FAIL;
    }
    strcpy(hostdir, spec);
    if (parse_devname(hostdir, &hostname, &dirname))
        return EX_FAIL;

    if (fill_ipv4_sockaddr(hostname, &server_addr))
        return EX_FAIL;

    get_my_ipv4addr(ip_addr, sizeof(ip_addr));

    /* Defaults */
    memset(&data, 0, sizeof(data));
    data.retrans    = 3;
    data.acregmin   = 3;
    data.acregmax   = 60;
    data.acdirmin   = 30;
    data.acdirmax   = 60;
    data.version    = 4;  /* nfs_mount_data version */
    server_addr.sin_port = htons(NFS_PORT);

    soft = 0;
    intr = MYNFS_MOUNT_INTR;
    nocto = 0;
    noac = 0;
    nonlm = 0;

    /* Parse options */
    if (extra_opts && *extra_opts) {
        char *opts_copy = strdup(extra_opts);
        for (opt = strtok(opts_copy, ","); opt; opt = strtok(NULL, ",")) {
            if ((opteq = strchr(opt, '='))) {
                val = atoi(opteq + 1);
                *opteq = '\0';
                if (!strcmp(opt, "rsize"))
                    data.rsize = val;
                else if (!strcmp(opt, "wsize"))
                    data.wsize = val;
                else if (!strcmp(opt, "timeo"))
                    data.timeo = val;
                else if (!strcmp(opt, "retrans"))
                    data.retrans = val;
                else if (!strcmp(opt, "acregmin"))
                    data.acregmin = val;
                else if (!strcmp(opt, "acregmax"))
                    data.acregmax = val;
                else if (!strcmp(opt, "acdirmin"))
                    data.acdirmin = val;
                else if (!strcmp(opt, "acdirmax"))
                    data.acdirmax = val;
                else if (!strcmp(opt, "actimeo")) {
                    data.acregmin = val;
                    data.acregmax = val;
                    data.acdirmin = val;
                    data.acdirmax = val;
                }
                else if (!strcmp(opt, "retry"))
                    retry = val;
                else if (!strcmp(opt, "port"))
                    server_addr.sin_port = htons(val);
                else if (!strcmp(opt, "mountport"))
                    mountport = val;
                else if (!strcmp(opt, "clientaddr")) {
                    strncpy(ip_addr, opteq+1, sizeof(ip_addr)-1);
                    ip_addr[sizeof(ip_addr)-1] = '\0';
                }
                else if (!strcmp(opt, "proto") || !strcmp(opt, "mountproto")) {
                    if (!strncmp(opteq+1, "tcp", 3))
                        use_tcp = 1;
                    else if (!strncmp(opteq+1, "udp", 3))
                        use_tcp = 0;
                }
                else if (!strcmp(opt, "addr") || !strcmp(opt, "mountaddr") ||
                         !strcmp(opt, "mounthost") || sloppy) {
                    /* ignore */
                }
                else if (!strcmp(opt, "vers") || !strcmp(opt, "nfsvers")) {
                    /* already determined fs type */
                }
                else if (!strcmp(opt, "sec")) {
                    /* skip */
                }
                else if (!strcmp(opt, "fg") || !strcmp(opt, "bg")) {
                    /* handled separately */
                }
                else {
                    if (verbose)
                        fprintf(stderr, "unknown option: %s=%d\n", opt, val);
                }
            } else {
                val = 1;
                if (!strncmp(opt, "no", 2)) {
                    val = 0;
                    opt += 2;
                }
                if (!strcmp(opt, "bg"))
                    bg = val;
                else if (!strcmp(opt, "fg"))
                    bg = !val;
                else if (!strcmp(opt, "soft"))
                    soft = val;
                else if (!strcmp(opt, "hard"))
                    soft = !val;
                else if (!strcmp(opt, "intr"))
                    intr = val;
                else if (!strcmp(opt, "cto"))
                    nocto = !val;
                else if (!strcmp(opt, "ac"))
                    noac = !val;
                else if (!strcmp(opt, "nolock"))
                    nonlm = 1;
                else if (!strcmp(opt, "lock"))
                    nonlm = 0;
                else if (!strcmp(opt, "tcp"))
                    use_tcp = 1;
                else if (!strcmp(opt, "udp"))
                    use_tcp = 0;
            }
        }
        free(opts_copy);
    }

    data.flags = (soft ? MYNFS_MOUNT_SOFT : 0)
        | (intr ? MYNFS_MOUNT_INTR : 0)
        | (nocto ? MYNFS_MOUNT_NOCTO : 0)
        | (noac ? MYNFS_MOUNT_NOAC : 0)
        | (nonlm ? MYNFS_MOUNT_NONLM : 0)
        | (use_tcp ? MYNFS_MOUNT_TCP : 0)
        | MYNFS_MOUNT_VER3
        | MYNFS_MOUNT_BROKEN_SUID;

    /* Server address */
    memcpy(&data.addr, &server_addr, sizeof(server_addr));

    /* Hostname */
    strncpy(data.hostname, hostname, sizeof(data.hostname) - 1);
    data.hostname[sizeof(data.hostname) - 1] = '\0';

    timeout = time(NULL) + 60 * retry;

    if (verbose) {
        fprintf(stderr, "mounting %s on %s (type mynfs, vers=3)\n", spec, node);
        fprintf(stderr, "  server=%s, port=%d, proto=%s\n",
                hostname, ntohs(server_addr.sin_port),
                use_tcp ? "tcp" : "udp");
    }

    for (;;) {
#ifdef __linux__
        int result = mount(spec, node, "mynfs", flags, &data);
#else
        int result = 0; /* stub for non-Linux syntax check */
#endif
        if (result == 0)
            return EX_SUCCESS;

        if (errno == EBUSY)
            return EX_FAIL;

        if (bg) {
            if (time(NULL) > timeout) {
                nfs_error("giving up after %d retries\n", retry);
                return EX_FAIL;
            }
            fprintf(stderr, "  retrying (bg)...\n");
            sleep(1);
            continue;
        }

        nfs_error("mount(%s): %s\n", spec, strerror(errno));
        return EX_FAIL;
    }
}

static void usage(void)
{
    fprintf(stderr, "Usage: %s [-fnsv] [-o options] device dir\n", progname);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -f            Fake mount (add to mtab only)\n");
    fprintf(stderr, "  -n            Don't update /etc/mtab\n");
    fprintf(stderr, "  -s            Sloppy: ignore unknown options\n");
    fprintf(stderr, "  -v            Verbose output\n");
    fprintf(stderr, "  -o options    Comma-separated mount options\n");
    exit(EX_USAGE);
}

int main(int argc, char *argv[])
{
    int c;
    int fake = 0;
    int nomtab __attribute__((unused)) = 0;
    int flags = 0;
    char *extra_opts = "";
    char *spec, *node;

    while ((c = getopt(argc, argv, "fno:svVh")) != -1) {
        switch (c) {
        case 'f':
            fake = 1;
            break;
        case 'n':
            nomtab = 1;
            break;
        case 'o':
            extra_opts = optarg;
            break;
        case 's':
            sloppy = 1;
            break;
        case 'v':
            verbose++;
            break;
        case 'V':
        case 'h':
            usage();
            break;
        default:
            usage();
        }
    }

    if (argc - optind != 2)
        usage();

    spec = argv[optind];
    node = argv[optind + 1];

    /* Parse mount flags from options */
    flags = 0;
    {
        char *opts_copy = extra_opts ? strdup(extra_opts) : NULL;
        if (opts_copy) {
            char *opt;
            for (opt = strtok(opts_copy, ","); opt; opt = strtok(NULL, ",")) {
                char *eq = strchr(opt, '=');
                if (eq) continue; /* skip key=value options */
                if (!strcmp(opt, "ro"))         flags |= MS_RDONLY;
                else if (!strcmp(opt, "rw"))    flags &= ~MS_RDONLY;
                else if (!strcmp(opt, "nosuid"))    flags |= MS_NOSUID;
                else if (!strcmp(opt, "suid"))      flags &= ~MS_NOSUID;
                else if (!strcmp(opt, "nodev"))     flags |= MS_NODEV;
                else if (!strcmp(opt, "dev"))       flags &= ~MS_NODEV;
                else if (!strcmp(opt, "noexec"))    flags |= MS_NOEXEC;
                else if (!strcmp(opt, "exec"))      flags &= ~MS_NOEXEC;
                else if (!strcmp(opt, "noatime"))   flags |= MS_NOATIME;
                else if (!strcmp(opt, "atime"))     flags &= ~MS_NOATIME;
                else if (!strcmp(opt, "nodiratime")) flags |= MS_NODIRATIME;
                else if (!strcmp(opt, "diratime"))  flags &= ~MS_NODIRATIME;
                else if (!strcmp(opt, "sync"))      flags |= MS_SYNCHRONOUS;
                else if (!strcmp(opt, "dirsync"))   flags |= MS_DIRSYNC;
                /* skip NFS-specific and other options */
            }
            free(opts_copy);
        }
    }
    if (fake)
        goto success;

    /* Detect NFS version from options */
    int nfs_version = 4; /* default to v4 */
    {
        char *opts_copy = extra_opts ? strdup(extra_opts) : NULL;
        if (opts_copy) {
            char *opt;
            for (opt = strtok(opts_copy, ","); opt; opt = strtok(NULL, ",")) {
                char *eq = strchr(opt, '=');
                if (eq) {
                    if (!strncmp(opt, "vers=", 5) || !strncmp(opt, "nfsvers=", 8)) {
                        char *val = eq + 1;
                        if (!strcmp(val, "3") || !strcmp(val, "2"))
                            nfs_version = 3;
                        else
                            nfs_version = 4;
                    }
                } else {
                    if (!strcmp(opt, "v3") || !strcmp(opt, "v2"))
                        nfs_version = 3;
                    else if (!strcmp(opt, "v4") || !strcmp(opt, "v4.0") ||
                             !strcmp(opt, "v4.1") || !strcmp(opt, "v4.2"))
                        nfs_version = 4;
                }
            }
            free(opts_copy);
        }
    }

    if (nfs_version == 4)
        return nfs4mount(spec, node, flags, extra_opts);
    else
        return nfsmount(spec, node, flags, extra_opts);

success:
    return EX_SUCCESS;
}
