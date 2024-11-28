#include <zephyr/kernel.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>

#include <xiaomi.h>

LOG_MODULE_REGISTER(stream_client, LOG_LEVEL_INF);

int thread(void *arg0, void *arg1, void *arg2);

typedef enum {
	STREAM_DISCONNECTED,
	STREAM_CONNECTED,
} scli_state_t;

typedef struct stream_client {
	int sock;
	scli_state_t state;
} scli_t;

K_THREAD_DEFINE(
	stream_tid, 2048u, thread, NULL, NULL, NULL, K_PRIO_PREEMPT(10), 0, SYS_FOREVER_MS);

int stream_client_init(void)
{
	k_thread_start(stream_tid);

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

static int channel_send_data(scli_t *s, uint32_t channel_id, void *data, size_t len)
{
	int ret;

	if (s->state != STREAM_CONNECTED) {
		return -ENOTCONN;
	}

	ret = send(s->sock, data, len, 0);
	if (ret < 0) {
		LOG_ERR("Failed to send data: %d", ret);
		return ret;
	}

	return 0;
}

int thread(void *arg0, void *arg1, void *arg2)
{
	int ret;
	scli_t scli;

	// initial state
	scli.state = STREAM_DISCONNECTED;

	for (;;) {
		switch (scli.state) {
		case STREAM_DISCONNECTED:
			if (try_connect(&scli)) {
				k_sleep(K_MSEC(CONFIG_COPRO_STREAM_TRY_CONNECT_INTERVAL));
			}
			break;
		case STREAM_CONNECTED:
			xiaomi_record_t xc;
			if (k_msgq_get(&xiaomi_msgq, (void *)&xc, K_SECONDS(1)) == 0) {
				// do something with the xiaomi record
				ret = channel_send_data(&scli, STREAM_CHANNEL_ID_XIAOMI, &xc, sizeof(xc));
				if (ret < 0) {
					LOG_ERR("Failed to send xiaomi record: %d", ret);
					disconnect(&scli);
				}
			}
			break;
		}
	}
}
