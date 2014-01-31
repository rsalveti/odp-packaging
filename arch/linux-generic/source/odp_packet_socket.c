/* Copyright (c) 2013, Linaro Limited
 * Copyright (c) 2013, Nokia Solutions and Networks
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/*
 * PACKET_MMAP code inspired by the linux seltest file psock_tpacket.c by:
 * Copyright 2013 Red Hat, Inc.
 * Author: Daniel Borkmann <dborkman@redhat.com>
 *         Chetan Loke <loke.chetan@gmail.com>
 */

#define _GNU_SOURCE
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <linux/if_packet.h>
#include <linux/filter.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <bits/wordsize.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <net/if.h>
#include <inttypes.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/syscall.h>

#include <odp_packet_socket.h>
#include <odp_packet_internal.h>
#include <odp_hints.h>

#include <helper/odp_eth.h>
#include <helper/odp_ip.h>
#include <helper/odp_packet_helper.h>

/** Eth buffer start offset from u32-aligned address to make sure the following
 * header (e.g. IP) starts at a 32-bit aligned address.
 */
#define ETHBUF_OFFSET (ODP_ALIGN_ROUNDUP(ODP_ETHHDR_LEN, sizeof(uint32_t)) \
				- ODP_ETHHDR_LEN)

/** Round up buffer address to get a properly aliged eth buffer, i.e. aligned
 * so that the next header always starts at a 32bit aligned address.
 */
#define ETHBUF_ALIGN(buf_ptr) ((uint8_t *)ODP_ALIGN_ROUNDUP_PTR((buf_ptr), \
				sizeof(uint32_t)) + ETHBUF_OFFSET)


static void ethaddr_copy(unsigned char mac_dst[], unsigned char mac_src[])
{
	memcpy(mac_dst, mac_src, ETH_ALEN);
}

static inline int ethaddrs_equal(unsigned char mac_a[], unsigned char mac_b[])
{
	return !memcmp(mac_a, mac_b, ETH_ALEN);
}

static int set_pkt_sock_fanout(pkt_sock_t * const pkt_sock, int sock_group_idx)
{
#if ODP_PACKET_SOCKET_FANOUT == 1
	/* Use FANOUT-mode for socket */
	int sockfd = pkt_sock->sockfd;
	int val;
	int err;
	uint16_t fanout_group;

	fanout_group = (uint16_t) (sock_group_idx & 0xffff);
	val = (PACKET_FANOUT_HASH << 16) | fanout_group;

	err = setsockopt(sockfd, SOL_PACKET, PACKET_FANOUT, &val, sizeof(val));
	if (err != 0) {
		perror("set_pkt_sock_fanout() - setsockopt(PACKET_FANOUT)");
		return -1;
	}
#else
	(void)pkt_sock;
	(void)sock_group_idx;
#endif

	return 0;
}

#if (ODP_PACKET_SOCKET_MODE == ODP_PACKET_SOCKET_BASIC) || \
	(ODP_PACKET_SOCKET_MODE == ODP_PACKET_SOCKET_MMSG)
/*
 * ODP_PACKET_SOCKET_BASIC:
 * ODP_PACKET_SOCKET_MMSG:
 */
int setup_pkt_sock(pkt_sock_t * const pkt_sock, char *netdev,
		   odp_buffer_pool_t pool)
{
	int sockfd;
	int err;
	unsigned int if_idx;
	struct ifreq ethreq;
	struct sockaddr_ll sa_ll;
	odp_packet_t pkt;
	uint8_t *pkt_buf;
	uint8_t *l2_hdr;

	if (pool == ODP_BUFFER_POOL_INVALID)
		return -1;
	pkt_sock->pool = pool;

	pkt = odp_packet_alloc(pool);
	if (!odp_packet_is_valid(pkt))
		return -1;

	pkt_buf = odp_packet_buf_addr(pkt);
	l2_hdr = ETHBUF_ALIGN(pkt_buf);
	/* Store eth buffer offset for pkt buffers from this pool */
	pkt_sock->l2_offset = (uintptr_t)l2_hdr - (uintptr_t)pkt_buf;
	/* pkt buffer size */
	pkt_sock->buf_size = odp_packet_buf_size(pkt);
	/* max frame len taking into account the l2-offset */
	pkt_sock->max_frame_len = pkt_sock->buf_size - pkt_sock->l2_offset;

	odp_packet_free(pkt);

	sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sockfd == -1) {
		perror("setup_pkt_sock() - socket()");
		return -1;
	}
	pkt_sock->sockfd = sockfd;

	/* get if index */
	memset(&ethreq, 0, sizeof(struct ifreq));
	strncpy(ethreq.ifr_name, netdev, IFNAMSIZ);
	err = ioctl(sockfd, SIOCGIFINDEX, &ethreq);
	if (err != 0) {
		perror("setup_pkt_sock() - ioctl(SIOCGIFINDEX)");
		return -1;
	}
	if_idx = ethreq.ifr_ifindex;

	/* get MAC address */
	memset(&ethreq, 0, sizeof(ethreq));
	strncpy(ethreq.ifr_name, netdev, IFNAMSIZ);
	err = ioctl(sockfd, SIOCGIFHWADDR, &ethreq);
	if (err != 0) {
		perror("setup_pkt_sock() - ioctl(SIOCGIFHWADDR)");
		return -1;
	}
	ethaddr_copy(pkt_sock->if_mac,
		     (unsigned char *)ethreq.ifr_ifru.ifru_hwaddr.sa_data);

	/* bind socket to if */
	memset(&sa_ll, 0, sizeof(sa_ll));
	sa_ll.sll_family = AF_PACKET;
	sa_ll.sll_ifindex = if_idx;
	sa_ll.sll_protocol = htons(ETH_P_ALL);
	if (bind(sockfd, (struct sockaddr *)&sa_ll, sizeof(sa_ll)) < 0) {
		perror("setup_pkt_sock() - bind(to IF)");
		return -1;
	}

	/* configure PACKET_FANOUT mode for socket (if mode enabled) */
	err = set_pkt_sock_fanout(pkt_sock, if_idx);
	if (err != 0)
		return -1;

	return sockfd;
}

/*
 * ODP_PACKET_SOCKET_BASIC:
 * ODP_PACKET_SOCKET_MMSG:
 */
int close_pkt_sock(pkt_sock_t * const pkt_sock)
{
	if (close(pkt_sock->sockfd) != 0) {
		perror("close_pkt_sock() - close(sockfd)");
		return -1;
	}

	return 0;
}
#endif

#if ODP_PACKET_SOCKET_MODE == ODP_PACKET_SOCKET_BASIC
/*
 * ODP_PACKET_SOCKET_BASIC:
 */
int recv_pkt_sock(pkt_sock_t *const pkt_sock,
		  odp_packet_t pkt_table[], unsigned len)
{
	ssize_t recv_bytes;
	unsigned i;
	struct sockaddr_ll sll;
	socklen_t addrlen = sizeof(sll);
	int const sockfd = pkt_sock->sockfd;
	odp_packet_t pkt = ODP_PACKET_INVALID;
	uint8_t *pkt_buf;
	uint8_t *l2_hdr;
	int nb_rx = 0;

	for (i = 0; i < len; i++) {
		if (odp_likely(pkt == ODP_PACKET_INVALID)) {
			pkt = odp_packet_alloc(pkt_sock->pool);
			if (odp_unlikely(pkt == ODP_PACKET_INVALID))
				break;
		}

		pkt_buf = odp_packet_buf_addr(pkt);
		l2_hdr = pkt_buf + pkt_sock->l2_offset;

		recv_bytes = recvfrom(sockfd, l2_hdr,
				      pkt_sock->max_frame_len, MSG_DONTWAIT,
				      (struct sockaddr *)&sll, &addrlen);
		/* no data or error: free recv buf and break out of loop */
		if (odp_unlikely(recv_bytes < 1))
			break;
		/* frame not explicitly for us, reuse pkt buf for next frame */
		if (odp_unlikely(sll.sll_pkttype != PACKET_HOST))
			continue;

		/* Parse and set packet header data */
		odp_packet_parse(pkt, recv_bytes, pkt_sock->l2_offset);

		pkt_table[nb_rx] = pkt;
		pkt = ODP_PACKET_INVALID;
		nb_rx++;
	} /* end for() */

	if (odp_unlikely(pkt != ODP_PACKET_INVALID))
		odp_packet_free(pkt);

	return nb_rx;
}

/*
 * ODP_PACKET_SOCKET_BASIC:
 */
int send_pkt_sock(pkt_sock_t * const pkt_sock,
		  odp_packet_t pkt_table[], unsigned len)
{
	odp_packet_t pkt;
	uint8_t *frame;
	size_t frame_len;
	unsigned i;
	unsigned flags;
	int sockfd;
	int nb_tx;
	int ret;

	sockfd = pkt_sock->sockfd;
	flags = MSG_DONTWAIT;
	i = 0;
	while (i < len) {
		pkt = pkt_table[i];

		frame = odp_packet_l2(pkt);
		frame_len = odp_packet_get_len(pkt);

		ret = send(sockfd, frame, frame_len, flags);
		if (odp_unlikely(ret == -1)) {
			if (odp_likely(errno == EAGAIN)) {
				flags = 0;	/* blocking for next rounds */
				continue;	/* resend buffer */
			} else {
				break;
			}
		}

		i++;
	}			/* end while */
	nb_tx = i;

	for (i = 0; i < len; i++)
		odp_packet_free(pkt_table[i]);

	return nb_tx;
}

#elif ODP_PACKET_SOCKET_MODE == ODP_PACKET_SOCKET_MMSG
/*
 * ODP_PACKET_SOCKET_MMSG:
 */
int recv_pkt_sock(pkt_sock_t * const pkt_sock,
		  odp_packet_t pkt_table[], unsigned len)
{
	const int sockfd = pkt_sock->sockfd;
	int msgvec_len;
	struct mmsghdr msgvec[ODP_PACKET_SOCKET_MAX_BURST_RX];
	struct iovec iovecs[ODP_PACKET_SOCKET_MAX_BURST_RX];
	uint8_t *pkt_buf;
	uint8_t *l2_hdr;
	int nb_rx = 0;
	int recv_msgs;
	int i;

	if (odp_unlikely(len > ODP_PACKET_SOCKET_MAX_BURST_RX))
		return -1;

	memset(msgvec, 0, sizeof(msgvec));

	for (i = 0; i < (int)len; i++) {
		pkt_table[i] = odp_packet_alloc(pkt_sock->pool);
		if (odp_unlikely(pkt_table[i] == ODP_PACKET_INVALID))
			break;

		pkt_buf = odp_packet_buf_addr(pkt_table[i]);
		l2_hdr = pkt_buf + pkt_sock->l2_offset;
		iovecs[i].iov_base = l2_hdr;
		iovecs[i].iov_len = pkt_sock->max_frame_len;
		msgvec[i].msg_hdr.msg_iov = &iovecs[i];
		msgvec[i].msg_hdr.msg_iovlen = 1;
	}
	msgvec_len = i; /* number of successfully allocated pkt buffers */

	recv_msgs = recvmmsg(sockfd, msgvec, msgvec_len, MSG_DONTWAIT, NULL);

	for (i = 0; i < recv_msgs; i++) {
		void *base = msgvec[i].msg_hdr.msg_iov->iov_base;
		struct ethhdr *eth_hdr = base;

		/* Don't receive packets sent by ourselves */
		if (odp_unlikely(ethaddrs_equal(pkt_sock->if_mac,
						eth_hdr->h_source))) {
			odp_packet_free(pkt_table[i]);
			continue;
		}

		/* Parse and set packet header data */
		odp_packet_parse(pkt_table[i], msgvec[i].msg_len,
				 pkt_sock->l2_offset);

		pkt_table[nb_rx] = pkt_table[i];
		nb_rx++;
	}

	/* Free unused pkt buffers */
	for (; i < msgvec_len; i++)
		odp_packet_free(pkt_table[i]);

	return nb_rx;
}

/*
 * ODP_PACKET_SOCKET_MMSG:
 */
int send_pkt_sock(pkt_sock_t * const pkt_sock,
		  odp_packet_t pkt_table[], unsigned len)
{
	struct mmsghdr msgvec[ODP_PACKET_SOCKET_MAX_BURST_TX];
	struct iovec iovecs[ODP_PACKET_SOCKET_MAX_BURST_TX];
	int ret;
	int sockfd;
	unsigned i;
	unsigned sent_msgs = 0;
	unsigned flags;

	if (odp_unlikely(len > ODP_PACKET_SOCKET_MAX_BURST_TX))
		return -1;

	sockfd = pkt_sock->sockfd;
	memset(msgvec, 0, sizeof(msgvec));

	for (i = 0; i < len; i++) {
		uint8_t *const frame = odp_packet_l2(pkt_table[i]);
		const size_t frame_len = odp_packet_get_len(pkt_table[i]);
		iovecs[i].iov_base = frame;
		iovecs[i].iov_len = frame_len;
		msgvec[i].msg_hdr.msg_iov = &iovecs[i];
		msgvec[i].msg_hdr.msg_iovlen = 1;
	}

	flags = MSG_DONTWAIT;
	for (i = 0; i < len; i += sent_msgs) {
		ret = sendmmsg(sockfd, &msgvec[i], len - i, flags);
		sent_msgs = ret > 0 ? (unsigned)ret : 0;
		flags = 0;	/* blocking for next rounds */
	}

	for (i = 0; i < len; i++)
		odp_packet_free(pkt_table[i]);

	return len;
}

#elif ODP_PACKET_SOCKET_MODE == ODP_PACKET_SOCKET_MMAP
/*
 * ODP_PACKET_SOCKET_MMAP:
 */

#ifndef __aligned_tpacket
#define __aligned_tpacket  ODP_ALIGNED(TPACKET_ALIGNMENT)
#endif

#ifndef __align_tpacket
#define __align_tpacket(x) ODP_ALIGNED(TPACKET_ALIGN(x))
#endif

union frame_map {
	struct {
		struct tpacket2_hdr tp_h __aligned_tpacket;
		struct sockaddr_ll s_ll
		    __align_tpacket(sizeof(struct tpacket2_hdr));
	} *v2;

	void *raw;
};

static int pkt_socket(void)
{
	int ver = TPACKET_V2;

	int ret, sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sock == -1) {
		perror("pkt_socket() - socket(SOCK_RAW)");
		return -1;
	}

	ret = setsockopt(sock, SOL_PACKET, PACKET_VERSION, &ver, sizeof(ver));
	if (ret == -1) {
		perror("pkt_socket() - setsockopt(PACKET_VERSION)");
		return -1;
	}

	return sock;
}

static inline int rx_kernel_ready(struct tpacket2_hdr *hdr)
{
	return ((hdr->tp_status & TP_STATUS_USER) == TP_STATUS_USER);
}

static inline void rx_user_ready(struct tpacket2_hdr *hdr)
{
	hdr->tp_status = TP_STATUS_KERNEL;
	__sync_synchronize();
}

static inline int tx_kernel_ready(struct tpacket2_hdr *hdr)
{
	return !(hdr->tp_status & (TP_STATUS_SEND_REQUEST | TP_STATUS_SENDING));
}

static inline void tx_user_ready(struct tpacket2_hdr *hdr)
{
	hdr->tp_status = TP_STATUS_SEND_REQUEST;
	__sync_synchronize();
}

static inline unsigned pkt_mmap_v2_rx(int sock, struct ring *ring,
				      odp_packet_t pkt_table[], unsigned len,
				      odp_buffer_pool_t pool, size_t l2_offset,
				      unsigned char if_mac[])
{
	union frame_map ppd;
	unsigned frame_num, next_frame_num;
	uint8_t *pkt_buf;
	int pkt_len;
	struct ethhdr *eth_hdr;
	uint8_t *l2_hdr;
	unsigned i = 0;

	(void)sock;

	frame_num = ring->frame_num;

	while (i < len) {
		if (rx_kernel_ready(ring->rd[frame_num].iov_base)) {
			ppd.raw = ring->rd[frame_num].iov_base;

			next_frame_num = (frame_num + 1) % ring->rd_num;

			pkt_buf = (uint8_t *)ppd.raw + ppd.v2->tp_h.tp_mac;
			pkt_len = ppd.v2->tp_h.tp_snaplen;

			/* Don't receive packets sent by ourselves */
			eth_hdr = (struct ethhdr *)pkt_buf;
			if (odp_unlikely(ethaddrs_equal(if_mac,
							eth_hdr->h_source))) {
				rx_user_ready(ppd.raw); /* drop */
				continue;
			}

			pkt_table[i] = odp_packet_alloc(pool);
			if (odp_unlikely(pkt_table[i] == ODP_PACKET_INVALID))
				break;

			l2_hdr = odp_packet_buf_addr(pkt_table[i]) + l2_offset;
			memcpy(l2_hdr, pkt_buf, pkt_len);

			rx_user_ready(ppd.raw);

			/* Parse and set packet header data */
			odp_packet_parse(pkt_table[i], pkt_len, l2_offset);

			frame_num = next_frame_num;
			i++;
		} else {
			break;
		}
	}

	ring->frame_num = frame_num;

	return i;
}

static inline unsigned pkt_mmap_v2_tx(int sock, struct ring *ring,
				      odp_packet_t pkt_table[], unsigned len)
{
	union frame_map ppd;
	uint8_t *pkt_buf;
	size_t pkt_len;
	unsigned frame_num, next_frame_num;
	int ret;
	unsigned i = 0;

	frame_num = ring->frame_num;

	while (i < len) {
		if (tx_kernel_ready(ring->rd[frame_num].iov_base)) {
			ppd.raw = ring->rd[frame_num].iov_base;

			next_frame_num = (frame_num + 1) % ring->rd_num;

			pkt_buf = odp_packet_l2(pkt_table[i]);
			pkt_len = odp_packet_get_len(pkt_table[i]);

			ppd.v2->tp_h.tp_snaplen = pkt_len;
			ppd.v2->tp_h.tp_len = pkt_len;

			memcpy((uint8_t *)ppd.raw + TPACKET2_HDRLEN -
			       sizeof(struct sockaddr_ll), pkt_buf, pkt_len);

			tx_user_ready(ppd.raw);

			odp_packet_free(pkt_table[i]);
			frame_num = next_frame_num;
			i++;
		} else {
			break;
		}
	}

	ring->frame_num = frame_num;

	ret = sendto(sock, NULL, 0, MSG_DONTWAIT, NULL, 0);
	if (ret == -1) {
		if (errno != EAGAIN) {
			perror("pkt_mmap_v2_tx() - sendto(pkt mmap)");
			return -1;
		}
	}

	return i;
}

static void fill_ring(struct ring *ring, unsigned blocks)
{
	ring->req.tp_block_size = getpagesize() << 2;
	ring->req.tp_frame_size = TPACKET_ALIGNMENT << 7;
	ring->req.tp_block_nr = blocks;

	ring->req.tp_frame_nr = ring->req.tp_block_size /
	    ring->req.tp_frame_size * ring->req.tp_block_nr;

	ring->mm_len = ring->req.tp_block_size * ring->req.tp_block_nr;
	ring->rd_num = ring->req.tp_frame_nr;
	ring->flen = ring->req.tp_frame_size;
}

static int set_packet_loss_discard(int sock)
{
	int ret, discard = 1;

	ret = setsockopt(sock, SOL_PACKET, PACKET_LOSS, (void *)&discard,
			 sizeof(discard));
	if (ret == -1) {
		perror("set_packet_loss_discard() - setsockopt(PACKET_LOSS)");
		return -1;
	}

	return 0;
}

static int setup_ring(int sock, struct ring *ring, int type)
{
	int ret = 0;
	unsigned blocks = 256;

	ring->sock = sock;
	ring->type = type;
	ring->version = TPACKET_V2;

	if (type == PACKET_TX_RING) {
		ret = set_packet_loss_discard(sock);
		if (ret != 0)
			return -1;
	}

	fill_ring(ring, blocks);

	ret = setsockopt(sock, SOL_PACKET, type, &ring->req, sizeof(ring->req));
	if (ret == -1) {
		perror("setup_ring() - setsockopt(pkt mmap)");
		return -1;
	}

	ring->rd_len = ring->rd_num * sizeof(*ring->rd);
	ring->rd = malloc(ring->rd_len);
	if (ring->rd == NULL) {
		perror("setup_ring() - env_shared_malloc()");
		return -1;
	}

	return 0;
}

static int mmap_sock(pkt_sock_t *pkt_sock)
{
	int i;
	int sock = pkt_sock->sockfd;

	/* map rx + tx buffer to userspace : they are in this order */
	pkt_sock->mmap_len =
	    pkt_sock->rx_ring.req.tp_block_size *
	    pkt_sock->rx_ring.req.tp_block_nr +
	    pkt_sock->tx_ring.req.tp_block_size *
	    pkt_sock->tx_ring.req.tp_block_nr;

	pkt_sock->mmap_base =
	    mmap(NULL, pkt_sock->mmap_len, PROT_READ | PROT_WRITE,
		 MAP_SHARED | MAP_LOCKED | MAP_POPULATE, sock, 0);

	if (pkt_sock->mmap_base == MAP_FAILED) {
		perror("mmap_sock() - mmap rx&tx buffer failed");
		return -1;
	}

	pkt_sock->rx_ring.mm_space = pkt_sock->mmap_base;
	memset(pkt_sock->rx_ring.rd, 0, pkt_sock->rx_ring.rd_len);
	for (i = 0; i < pkt_sock->rx_ring.rd_num; ++i) {
		pkt_sock->rx_ring.rd[i].iov_base =
		    pkt_sock->rx_ring.mm_space + (i * pkt_sock->rx_ring.flen);
		pkt_sock->rx_ring.rd[i].iov_len = pkt_sock->rx_ring.flen;
	}

	pkt_sock->tx_ring.mm_space =
	    pkt_sock->mmap_base + pkt_sock->rx_ring.mm_len;
	memset(pkt_sock->tx_ring.rd, 0, pkt_sock->tx_ring.rd_len);
	for (i = 0; i < pkt_sock->tx_ring.rd_num; ++i) {
		pkt_sock->tx_ring.rd[i].iov_base =
		    pkt_sock->tx_ring.mm_space + (i * pkt_sock->tx_ring.flen);
		pkt_sock->tx_ring.rd[i].iov_len = pkt_sock->tx_ring.flen;
	}

	return 0;
}

static void unmap_sock(pkt_sock_t *pkt_sock)
{
	munmap(pkt_sock->mmap_base, pkt_sock->mmap_len);
	free(pkt_sock->rx_ring.rd);
	free(pkt_sock->tx_ring.rd);
}

static int bind_sock(pkt_sock_t *pkt_sock, char *netdev)
{
	int ret;

	pkt_sock->ll.sll_family = PF_PACKET;
	pkt_sock->ll.sll_protocol = htons(ETH_P_ALL);
	pkt_sock->ll.sll_ifindex = if_nametoindex(netdev);
	pkt_sock->ll.sll_hatype = 0;
	pkt_sock->ll.sll_pkttype = 0;
	pkt_sock->ll.sll_halen = 0;

	ret =
	    bind(pkt_sock->sockfd, (struct sockaddr *)&pkt_sock->ll,
		 sizeof(pkt_sock->ll));
	if (ret == -1) {
		perror("bind_sock() - bind(to IF)");
		return -1;
	}

	return 0;
}

static int store_hw_addr(pkt_sock_t * const pkt_sock, char *netdev)
{
	struct ifreq ethreq;
	int ret;

	/* get MAC address */
	memset(&ethreq, 0, sizeof(ethreq));
	strncpy(ethreq.ifr_name, netdev, IFNAMSIZ);
	ret = ioctl(pkt_sock->sockfd, SIOCGIFHWADDR, &ethreq);
	if (ret != 0) {
		perror("store_hw_addr() - ioctl(SIOCGIFHWADDR)");
		return -1;
	}

	ethaddr_copy(pkt_sock->if_mac,
		     (unsigned char *)ethreq.ifr_ifru.ifru_hwaddr.sa_data);

	return 0;
}

/*
 * ODP_PACKET_SOCKET_MMAP:
 */
int setup_pkt_sock(pkt_sock_t * const pkt_sock, char *netdev,
		   odp_buffer_pool_t pool)
{
	odp_packet_t pkt;
	uint8_t *pkt_buf;
	uint8_t *l2_hdr;
	int if_idx;
	int ret = 0;

	memset(pkt_sock, 0, sizeof(*pkt_sock));

	if (pool == ODP_BUFFER_POOL_INVALID)
		return -1;

	pkt = odp_packet_alloc(pool);
	if (!odp_packet_is_valid(pkt))
		return -1;

	pkt_buf = odp_packet_buf_addr(pkt);
	l2_hdr = ETHBUF_ALIGN(pkt_buf);
	/* Store eth buffer offset for pkt buffers from this pool */
	pkt_sock->l2_offset = (uintptr_t)l2_hdr - (uintptr_t)pkt_buf;

	odp_packet_free(pkt);

	pkt_sock->pool = pool;
	pkt_sock->sockfd = pkt_socket();

	ret = bind_sock(pkt_sock, netdev);
	if (ret != 0)
		return -1;

	ret = setup_ring(pkt_sock->sockfd, &pkt_sock->tx_ring, PACKET_TX_RING);
	if (ret != 0)
		return -1;

	ret = setup_ring(pkt_sock->sockfd, &pkt_sock->rx_ring, PACKET_RX_RING);
	if (ret != 0)
		return -1;

	ret = mmap_sock(pkt_sock);
	if (ret != 0)
		return -1;

	ret = store_hw_addr(pkt_sock, netdev);
	if (ret != 0)
		return -1;

	if_idx = if_nametoindex(netdev);
	if (if_idx == 0) {
		perror("setup_pkt_sock(): if_nametoindex()");
		return -1;
	}

	ret = set_pkt_sock_fanout(pkt_sock, if_idx);
	if (ret != 0)
		return -1;

	return pkt_sock->sockfd;
}

/*
 * ODP_PACKET_SOCKET_MMAP:
 */
int close_pkt_sock(pkt_sock_t * const pkt_sock)
{
	unmap_sock(pkt_sock);
	if (close(pkt_sock->sockfd) != 0) {
		perror("close_pkt_sock() - close(sockfd)");
		return -1;
	}

	return 0;
}

/*
 * ODP_PACKET_SOCKET_MMAP:
 */
int recv_pkt_sock(pkt_sock_t * const pkt_sock,
		  odp_packet_t pkt_table[], unsigned len)
{
	return pkt_mmap_v2_rx(pkt_sock->rx_ring.sock, &pkt_sock->rx_ring,
			      pkt_table, len, pkt_sock->pool,
			      pkt_sock->l2_offset, pkt_sock->if_mac);
}

/*
 * ODP_PACKET_SOCKET_MMAP:
 */
int send_pkt_sock(pkt_sock_t * const pkt_sock,
		  odp_packet_t pkt_table[], unsigned len)
{
	return pkt_mmap_v2_tx(pkt_sock->tx_ring.sock, &pkt_sock->tx_ring,
			      pkt_table, len);
}

#else
#error "Unsupported ODP_PACKET_SOCKET_MODE!"
#endif
