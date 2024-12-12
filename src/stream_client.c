#include <zephyr/kernel.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/byteorder.h>

#include <stream_client.h>

LOG_MODULE_REGISTER(stream_client, LOG_LEVEL_INF);

#define CHANNEL_CONTROL_ID 0x00000000

typedef enum {
	STREAM_UNINITIALIZED,
	STREAM_DISCONNECTED,
	STREAM_CONNECTED,
} scli_state_t;

typedef struct {
	char name[32];		 // channel name
	uint32_t channel_id; // channel id
	struct k_msgq *msgq;
} chan_t;

typedef struct {
	int sock;
	scli_state_t state;
	struct k_poll_event poll_events[CONFIG_COPRO_STREAM_CHANNELS_COUNT];
	size_t channels_count;
	chan_t channels[CONFIG_COPRO_STREAM_CHANNELS_COUNT];
} scli_t;

// Global stream client instance
static scli_t scli = {
	.state = STREAM_UNINITIALIZED,
	.sock  = -1,
};

int thread(void *arg0, void *arg1, void *arg2);

K_THREAD_DEFINE(
	stream_tid, 2048u, thread, NULL, NULL, NULL, K_PRIO_PREEMPT(10), 0, SYS_FOREVER_MS);

int stream_client_channel_add(uint32_t channel_id, const char *name, struct k_msgq *msgq)
{
	int i;

	if (scli.state != STREAM_UNINITIALIZED) {
		return -EALREADY;
	}

	if (channel_id == CHANNEL_CONTROL_ID) {
		/* Reserved channel id */
		return -EINVAL;
	}

	if (channel_id == 0 || msgq == NULL || name == NULL || msgq->msg_size == 0 ||
		msgq->msg_size > CONFIG_COPRO_STREAM_CHANNEL_MSG_MAX_SIZE) {
		return -EINVAL;
	}

	for (i = 0; i < CONFIG_COPRO_STREAM_CHANNELS_COUNT; i++) {
		if (scli.channels[i].channel_id == 0 ||
			scli.channels[i].channel_id == channel_id) {
			strncpy(scli.channels[i].name, name, sizeof(scli.channels[i].name));
			scli.channels[i].channel_id = channel_id;
			scli.channels[i].msgq		= msgq;

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

	for (int i = 0; i < scli.channels_count; i++) {
		k_poll_event_init(&scli.poll_events[i],
						  K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
						  K_POLL_MODE_NOTIFY_ONLY,
						  scli.channels[i].msgq);
	}

	k_thread_start(stream_tid);

	scli.state = STREAM_DISCONNECTED;

	return 0;
}

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

	LOG_INF("Connected to %s:%d", CONFIG_COPRO_STREAM_HOST, CONFIG_COPRO_STREAM_PORT);

	return 0;
}

static int disconnect(scli_t *s)
{
	__ASSERT_NO_MSG(s);

	if (s->sock >= 0) {
		close(s->sock);
		s->sock = -1;
	}

	s->state = STREAM_DISCONNECTED;

	return 0;
}

/* Channel data layout is as follows:
 *  - 4 bytes: channel id
 *  - 2 bytes: data length
 *  - N bytes: data
 */

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
		return ret;
	}

	// Write the data
	ret = send(s->sock, data, len, 0);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

int thread(void *arg0, void *arg1, void *arg2)
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
			ret = k_poll(scli.poll_events, scli.channels_count, K_FOREVER);
			if (ret < 0) {
				if (ret == -EAGAIN) {
					// Timeout, should not happen
					continue;
				} else {
					LOG_ERR("Failed to poll: %d", ret);
					disconnect(&scli);
				}
			}

			for (int i = 0; i < scli.channels_count; i++) {
				if (scli.poll_events[i].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE) {
					chan_t *chan = &scli.channels[i];

					if (k_msgq_get(chan->msgq, (void *)buf, K_NO_WAIT) == 0) {
						ret = channel_send_data(
							&scli, chan->channel_id, buf, chan->msgq->msg_size);
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
