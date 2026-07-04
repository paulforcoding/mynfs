/*
 * Stub linux/nfs.h for non-Linux builds (syntax check only)
 */
#ifndef _LINUX_NFS_H
#define _LINUX_NFS_H

#define NFS_MAXPATHLEN  1024
#define NFS_MAXNAMLEN   255
#define NFS_FHSIZE      32
#define NFS2_FHSIZE     32
#define NFS3_FHSIZE     64
#define NFS4_FHSIZE     128


struct nfs2_fh {
	unsigned char	data[NFS2_FHSIZE];
};

struct nfs3_fh {
	unsigned short	size;
	unsigned char	data[NFS3_FHSIZE];
};
#endif /* _LINUX_NFS_H */
