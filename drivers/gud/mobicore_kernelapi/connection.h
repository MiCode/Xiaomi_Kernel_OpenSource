/** @addtogroup MCD_MCDIMPL_DAEMON_SRV
 * @{
 * @file
 *
 * Connection data.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2009 - 2012 -->
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef CONNECTION_H_
#define CONNECTION_H_

#include <linux/semaphore.h>

#include <stddef.h>
#include <stdbool.h>

#define MAX_PAYLOAD_SIZE 128

struct connection {
	struct sock *socket_descriptor; /**< Netlink socket */
	uint32_t sequence_magic; /**< Random? magic to match requests/answers */

	struct nlmsghdr *data_msg;
	uint32_t data_len; /**< How much connection data is left */
	void *data_start; /**< Start pointer of remaining data */
	struct sk_buff *skb;

	struct mutex data_lock; /**< Data protection lock */
	struct semaphore data_available_sem; /**< Data protection semaphore */

	pid_t self_pid; /**< PID address used for local connection */
	pid_t peer_pid; /**< Remote PID for connection */

	struct list_head list; /**< The list param for using the kernel lists*/
};

struct connection *connection_new(
	void
);

struct connection *connection_create(
	int		  socket_descriptor,
	pid_t		dest
);

void connection_cleanup(
	struct connection *conn
);

/**
  * Connect to destination.
  *
  * @param Destination pointer.
  * @return true on success.
  */
bool connection_connect(
	struct connection *conn,
	pid_t		dest
);


/**
  * Read bytes from the connection.
  *
  * @param buffer	Pointer to destination buffer.
  * @param len	   Number of bytes to read.
  * @return Number of bytes read.
  */
size_t connection_read_datablock(
	struct connection *conn,
	void		 *buffer,
	uint32_t	 len
);
/**
  * Read bytes from the connection.
  *
  * @param buffer	Pointer to destination buffer.
  * @param len	   Number of bytes to read.
  * @param timeout   Timeout in milliseconds
  * @return Number of bytes read.
  * @return -1 if select() failed (returned -1)
  * @return -2 if no data available, i.e. timeout
  */
size_t connection_read_data(
	struct connection *conn,
	void		 *buffer,
	uint32_t	 len,
	int32_t	  timeout
);

/**
  * Write bytes to the connection.
  *
  * @param buffer	Pointer to source buffer.
  * @param len		Number of bytes to read.
  * @return Number of bytes written.
  */
size_t connection_write_data(
	struct connection *conn,
	void		 *buffer,
	uint32_t	  len
);

/**
 * Write bytes to the connection.
 *
 * @param buffer	Pointer to source buffer.
 * @param len		Number of bytes to read.
 * @return Number of bytes written.
 */
int connection_process(
	struct connection *conn,
	struct sk_buff *skb
);

#endif /* CONNECTION_H_ */

/** @} */
