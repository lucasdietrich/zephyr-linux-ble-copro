#include <zephyr/logging/log.h>
#include <zephyr/app_version.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/unistd.h>

#include <led.h>
#include <stream_client.h>

LOG_MODULE_REGISTER(stream_client, LOG_LEVEL_INF);

#define CHANNEL_CONTROL_ID 0x00000000

typedef enum {
	STREAM_UNINITIALIZED,
	STREAM_DISCONNECTED,
	STREAM_CONNECTED,
} scli_state_t;

typedef struct {
	char name[32u];		 // channel name
	uint32_t channel_id; // channel id
	struct k_msgq *tx_msgq;
#if defined(CONFIG_COPRO_STREAM_CHANNEL_RX)
	struct k_msgq *rx_msgq; // incoming data from server, NULL if not used
#endif
} chan_t;

typedef struct {
	int sock;
	scli_state_t state;
	stream_client_conn_cb_t conn_cb;
#if defined(CONFIG_COPRO_STREAM_CHANNEL_RX)
	/* +1 for the RX-disconnect signal slot */
	struct k_poll_event poll_events[CONFIG_COPRO_STREAM_CHANNELS_COUNT + 1];
	struct k_poll_signal rx_disconnect_signal; /* fired by RX thread on drop */
	struct k_sem rx_connected_sem;			   /* given by TX on connect, taken by RX */
#else
	struct k_poll_event poll_events[CONFIG_COPRO_STREAM_CHANNELS_COUNT];
#endif
	struct k_mutex conn_mutex; /* serialises disconnect() */
	size_t channels_count;
	chan_t channels[CONFIG_COPRO_STREAM_CHANNELS_COUNT];
} scli_t;

// Global stream client instance
static scli_t scli = {
	.state = STREAM_UNINITIALIZED,
	.sock  = -1,
};

int tx_thread(void *arg0, void *arg1, void *arg2);

K_THREAD_DEFINE(tx_stream_tid,
				2048u,
				tx_thread,
				NULL,
				NULL,
				NULL,
				K_PRIO_PREEMPT(10),
				0,
				SYS_FOREVER_MS);

#if defined(CONFIG_COPRO_STREAM_CHANNEL_RX)
int rx_thread(void *arg0, void *arg1, void *arg2);
K_THREAD_DEFINE(rx_stream_tid,
				2048u,
				rx_thread,
				NULL,
				NULL,
				NULL,
				K_PRIO_PREEMPT(10),
				0,
				SYS_FOREVER_MS);
#endif

int stream_client_channel_add(uint32_t channel_id,
							  const char *name,
							  struct k_msgq *tx_msgq,
							  struct k_msgq *rx_msgq)
{
	int i;

	if (scli.state != STREAM_UNINITIALIZED) {
		return -EALREADY;
	}

	if (channel_id == CHANNEL_CONTROL_ID) {
		/* Reserved channel id */
		return -EINVAL;
	}

	if (channel_id == 0 || tx_msgq == NULL || name == NULL || tx_msgq->msg_size == 0 ||
		tx_msgq->msg_size > CONFIG_COPRO_STREAM_CHANNEL_MSG_MAX_SIZE) {
		return -EINVAL;
	}

#if defined(CONFIG_COPRO_STREAM_CHANNEL_RX)
	if (rx_msgq != NULL &&
		(rx_msgq->msg_size == 0 ||
		 rx_msgq->msg_size > CONFIG_COPRO_STREAM_CHANNEL_MSG_MAX_SIZE)) {
		return -EINVAL;
	}
#endif

	for (i = 0; i < CONFIG_COPRO_STREAM_CHANNELS_COUNT; i++) {
		if (scli.channels[i].channel_id == 0 ||
			scli.channels[i].channel_id == channel_id) {
			strncpy(scli.channels[i].name, name, sizeof(scli.channels[i].name));
			scli.channels[i].channel_id = channel_id;
			scli.channels[i].tx_msgq	= tx_msgq;
#if defined(CONFIG_COPRO_STREAM_CHANNEL_RX)
			scli.channels[i].rx_msgq = rx_msgq;
#endif

			scli.channels_count++;

			return 0;
		}
	}

	return -ENOMEM;
}

int stream_client_start(void)
{
	if (scli.state != STREAM_UNINITIALIZED) {
		return -EALREADY;
	}

	k_mutex_init(&scli.conn_mutex);

	for (int i = 0; i < scli.channels_count; i++) {
		k_poll_event_init(&scli.poll_events[i],
						  K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
						  K_POLL_MODE_NOTIFY_ONLY,
						  scli.channels[i].tx_msgq);
	}

#if defined(CONFIG_COPRO_STREAM_CHANNEL_RX)
	k_sem_init(&scli.rx_connected_sem, 0, 1);
	k_poll_signal_init(&scli.rx_disconnect_signal);

	/* Last poll slot watches for disconnection signaled by the RX thread */
	k_poll_event_init(&scli.poll_events[scli.channels_count],
					  K_POLL_TYPE_SIGNAL,
					  K_POLL_MODE_NOTIFY_ONLY,
					  &scli.rx_disconnect_signal);

	k_thread_start(rx_stream_tid);
#endif

	k_thread_start(tx_stream_tid);

	scli.state = STREAM_DISCONNECTED;

	return 0;
}

bool stream_client_is_connected(void)
{
	return scli.state == STREAM_CONNECTED;
}

void stream_client_set_conn_cb(stream_client_conn_cb_t cb)
{
	scli.conn_cb = cb;
}

static int channel_send_data(scli_t *s, uint32_t channel_id, void *data, size_t len);
static void send_control_firmware_version(scli_t *s);

static int try_connect(scli_t *s)
{
	int ret, sock;
	struct sockaddr_in addr;

	__ASSERT_NO_MSG(s);

	ret = net_addr_pton(AF_INET, CONFIG_COPRO_STREAM_HOST, &addr.sin_addr);
	if (ret < 0) {
		return ret;
	}

	addr.sin_family = AF_INET;
	addr.sin_port	= htons(CONFIG_COPRO_STREAM_PORT);

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		LOG_ERR("Failed to create socket: %d", sock);
		return sock;
	}

	ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		LOG_ERR("Failed to connect: %d", ret);
		close(sock);
		return ret;
	}

	s->sock	 = sock;
	s->state = STREAM_CONNECTED;
	LED_ON();

	/* Discard messages that accumulated while disconnected – they are stale. */
	for (int i = 0; i < s->channels_count; i++) {
		k_msgq_purge(s->channels[i].tx_msgq);
	}

	LOG_INF("Connected to %s:%d", CONFIG_COPRO_STREAM_HOST, CONFIG_COPRO_STREAM_PORT);

	if (s->conn_cb) {
		s->conn_cb(true);
	}

	send_control_firmware_version(s);

#if defined(CONFIG_COPRO_STREAM_CHANNEL_RX)
	/* Reset stale disconnect signal, then wake the RX thread */
	k_poll_signal_reset(&s->rx_disconnect_signal);
	s->poll_events[s->channels_count].state = K_POLL_STATE_NOT_READY;
	k_sem_give(&s->rx_connected_sem);
#endif

	return 0;
}

static int disconnect(scli_t *s)
{
	__ASSERT_NO_MSG(s);

	bool notify = false;

	k_mutex_lock(&s->conn_mutex, K_FOREVER);
	if (s->sock >= 0) {
		close(s->sock);
		s->sock	 = -1;
		s->state = STREAM_DISCONNECTED;
		LED_OFF();
		LOG_INF("Disconnected");
		notify = true;
	}
	k_mutex_unlock(&s->conn_mutex);

	if (notify && s->conn_cb) {
		s->conn_cb(false);
	}

	return 0;
}

/* Channel data layout is as follows:
 *  - 4 bytes: channel id
 *  - 2 bytes: data length
 *  - N bytes: data
 */

#define CTRL_MSG_FIRMWARE_VERSION 0x01u

/**
 * @brief Send the firmware version on the control channel.
 *
 * Wire layout:
 *   byte 0: message type (CTRL_MSG_FIRMWARE_VERSION = 0x01)
 *   byte 1: major
 *   byte 2: minor
 *   byte 3: patch
 */
static void send_control_firmware_version(scli_t *s)
{
	uint8_t buf[4u] = {
		CTRL_MSG_FIRMWARE_VERSION,
		APP_VERSION_MAJOR,
		APP_VERSION_MINOR,
		APP_PATCHLEVEL,
	};

	int ret = channel_send_data(s, CHANNEL_CONTROL_ID, buf, sizeof(buf));
	if (ret < 0) {
		LOG_ERR("Failed to send firmware version: %d", ret);
	} else {
		LOG_INF("Firmware version %d.%d.%d sent on control channel",
				APP_VERSION_MAJOR,
				APP_VERSION_MINOR,
				APP_PATCHLEVEL);
	}
}

static int channel_send_data(scli_t *s, uint32_t channel_id, void *data, size_t len)
{
	int ret;

	if (s->state != STREAM_CONNECTED) {
		return -ENOTCONN;
	}

	// Prepare the header
	char buf_hdr[6u];

	sys_put_le32(channel_id, buf_hdr);
	sys_put_le16((uint16_t)len, &buf_hdr[4u]);

	// Write the header
	ret = send(s->sock, buf_hdr, sizeof(buf_hdr), 0);
	if (ret < 0) {
		LOG_ERR("Failed to send header: %d errno: %d", ret, errno);
		return ret;
	}

	// Write the data
	ret = send(s->sock, data, len, 0);
	if (ret < 0) {
		LOG_ERR("Failed to send data: %d errno: %d", ret, errno);
		return ret;
	}

	return 0;
}

int tx_thread(void *arg0, void *arg1, void *arg2)
{
	int ret;
	char buf[CONFIG_COPRO_STREAM_CHANNEL_MSG_MAX_SIZE];

	for (;;) {
		switch (scli.state) {
		case STREAM_DISCONNECTED:
			if (try_connect(&scli)) {
				k_sleep(K_MSEC(CONFIG_COPRO_STREAM_TRY_CONNECT_INTERVAL));
			}
			break;
		case STREAM_CONNECTED:
#if defined(CONFIG_COPRO_STREAM_CHANNEL_RX)
			/* Poll TX msgqs (slots 0..channels_count-1) plus the RX-disconnect
			 * signal (slot channels_count). */
			ret = k_poll(scli.poll_events, scli.channels_count + 1, K_FOREVER);
#else
			ret = k_poll(scli.poll_events, scli.channels_count, K_FOREVER);
#endif
			if (ret < 0 && ret != -EAGAIN) {
				LOG_ERR("Failed to poll: %d", ret);
				disconnect(&scli);
				break;
			}

#if defined(CONFIG_COPRO_STREAM_CHANNEL_RX)
			/* Check whether the RX thread signaled a disconnect */
			if (scli.poll_events[scli.channels_count].state == K_POLL_STATE_SIGNALED) {
				k_poll_signal_reset(&scli.rx_disconnect_signal);
				scli.poll_events[scli.channels_count].state = K_POLL_STATE_NOT_READY;
				/* State already set to DISCONNECTED by the RX thread */
				break;
			}
#endif

			for (int i = 0; i < scli.channels_count && scli.state == STREAM_CONNECTED;
				 i++) {
				if (scli.poll_events[i].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE) {
					chan_t *chan = &scli.channels[i];

					if (k_msgq_get(chan->tx_msgq, (void *)buf, K_NO_WAIT) == 0) {
						ret = channel_send_data(
							&scli, chan->channel_id, buf, chan->tx_msgq->msg_size);
						if (ret < 0) {
							LOG_ERR("[channel %s:%X] Failed to send data: %d",
									chan->name,
									chan->channel_id,
									ret);
							disconnect(&scli);
						}
					}
				}
			}
			break;
		case STREAM_UNINITIALIZED:
		default:
			LOG_ERR("Invalid state: %d", scli.state);
			return -EINVAL;
		}
	}
}

#if defined(CONFIG_COPRO_STREAM_CHANNEL_RX)
/* Receive exactly @len bytes from @sock, retrying on short reads.
 * Returns @len on success, or a negative errno on connection loss. */
static int recv_all(int sock, void *buf, size_t len)
{
	size_t received = 0;

	while (received < len) {
		int ret = recv(sock, (uint8_t *)buf + received, len - received, 0);

		if (ret == 0) {
			return -ECONNRESET;
		}
		if (ret < 0) {
			return -errno;
		}
		received += ret;
	}

	return (int)received;
}

int rx_thread(void *arg0, void *arg1, void *arg2)
{
	uint8_t hdr[6];
	uint8_t buf[CONFIG_COPRO_STREAM_CHANNEL_MSG_MAX_SIZE];

	for (;;) {
		/* Block until the TX thread establishes a connection */
		k_sem_take(&scli.rx_connected_sem, K_FOREVER);

		LOG_DBG("RX thread: connection active, starting receive loop");

		while (scli.state == STREAM_CONNECTED) {
			/* Read the 6-byte frame header: 4-byte channel id + 2-byte length */
			int ret = recv_all(scli.sock, hdr, sizeof(hdr));

			if (ret < 0) {
				LOG_INF("RX: connection lost reading header (%d)", ret);
				disconnect(&scli);
				k_poll_signal_raise(&scli.rx_disconnect_signal, 0);
				break;
			}

			uint32_t channel_id = sys_get_le32(hdr);
			uint16_t data_len	= sys_get_le16(&hdr[4]);

			if (data_len > sizeof(buf)) {
				LOG_ERR("RX: oversized frame (%u B) for channel 0x%08X – dropping",
						data_len,
						channel_id);
				disconnect(&scli);
				k_poll_signal_raise(&scli.rx_disconnect_signal, 0);
				break;
			}

			if (data_len > 0) {
				ret = recv_all(scli.sock, buf, data_len);
				if (ret < 0) {
					LOG_INF("RX: connection lost reading payload (%d)", ret);
					disconnect(&scli);
					k_poll_signal_raise(&scli.rx_disconnect_signal, 0);
					break;
				}
			}

			/* Dispatch to the matching channel's rx_msgq */
			bool dispatched = false;

			for (int i = 0; i < scli.channels_count; i++) {
				if (scli.channels[i].channel_id == channel_id) {
					if (scli.channels[i].rx_msgq != NULL) {
						ret = k_msgq_put(scli.channels[i].rx_msgq, buf, K_NO_WAIT);
						if (ret < 0) {
							LOG_WRN("RX [%s]: queue full, dropping message",
									scli.channels[i].name);
						}
					} else {
						LOG_WRN("RX [%s]: no RX queue configured, dropping message",
								scli.channels[i].name);
					}
					dispatched = true;
					break;
				}
			}

			if (!dispatched) {
				LOG_WRN("RX: no channel registered for id 0x%08X", channel_id);
			}
		}
	}
}
#endif /* CONFIG_COPRO_STREAM_CHANNEL_RX */