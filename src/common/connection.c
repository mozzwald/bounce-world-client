#ifndef _CMOC_VERSION_
#include <conio.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#else
#include <cmoc.h>
#include <coco.h>
#include "conio_wrapper.h"
#endif
#include "app_errors.h"
#include "data.h"
#include "delay.h"
#ifdef __ATARI__
#include "netstream/netstream.h"
#else
#include "fujinet-network.h"
#endif
#include "hex_dump.h"
#include "press_key.h"
#include "shapes.h"
#include "world.h"

#ifdef __PMD85__
#include "conio_wrapper.h"
#include "itoa_wrapper.h"
// #elif defined __ADAM__
// #include "conio_wrapper.h"
#endif

// platform specific values will be supplied here:
#include "screen.h"

#ifndef __ATARI__
extern uint8_t fn_default_timeout;
#endif

// extern void debug();

#ifdef __ATARI__
#define NETSTREAM_FLAG_TCP    0x01
#define NETSTREAM_FLAG_TX_INT 0x00
#define NETSTREAM_FLAG_RX_EXT 0x08
#define NETSTREAM_FLAGS       (NETSTREAM_FLAG_TCP | NETSTREAM_FLAG_TX_INT | NETSTREAM_FLAG_RX_EXT)
#define NETSTREAM_BAUD        0x4b00U
#define NETSTREAM_HOST_MAX    61
#define NETSTREAM_COMMAND_EOL 0x0a
#define NETSTREAM_WAIT_IDLE_PAUSES  15
#define NETSTREAM_DRAIN_IDLE_PAUSES 8
#define NETSTREAM_FRAME_FIRST_BYTE_PAUSES 8

static char netstream_host[NETSTREAM_HOST_MAX + 1];
static uint8_t netstream_client_data_read = 0;

static uint16_t swap_port(uint16_t port)
{
	return (uint16_t)((port << 8) | (port >> 8));
}

static void parse_netstream_url(uint16_t *port_swapped)
{
	char *p = server_url;
	char *host_start;
	char *colon;
	uint16_t port = 9003;
	uint8_t len;

	if (p[0] == 'n' && p[1] == '1' && p[2] == ':') {
		p += 3;
	}
	if (strncmp(p, "tcp://", 6) == 0) {
		p += 6;
	}

	host_start = p;
	colon = strrchr(host_start, ':');
	if (colon != NULL) {
		port = (uint16_t)atoi(colon + 1);
		len = (uint8_t)(colon - host_start);
	} else {
		len = (uint8_t)strlen(host_start);
	}
	if (len > NETSTREAM_HOST_MAX) {
		len = NETSTREAM_HOST_MAX;
	}

	memcpy(netstream_host, host_start, len);
	netstream_host[len] = '\0';
	*port_swapped = swap_port(port);
}

static void netstream_drain_input(uint8_t idle_limit, uint8_t wait_when_empty)
{
	uint8_t idle = 0;

	while (idle < idle_limit) {
		if (ns_bytes_avail() != 0) {
			(void)ns_recv_byte();
			idle = 0;
		} else {
			if (!wait_when_empty) {
				return;
			}
			++idle;
			pause(1);
		}
	}
}

static int16_t netstream_read_raw_limit(uint8_t *buf, int16_t len, uint8_t first_byte_idle_limit)
{
	int16_t total = 0;
	int16_t b;
	uint8_t idle = 0;

	while (total < len) {
		if (ns_bytes_avail() == 0) {
			if (total != 0) {
				++idle;
				if (idle >= NETSTREAM_WAIT_IDLE_PAUSES) {
					netstream_drain_input(NETSTREAM_DRAIN_IDLE_PAUSES, 1);
					return -1;
				}
			} else if (first_byte_idle_limit != 0) {
				++idle;
				if (idle >= first_byte_idle_limit) {
					return 0;
				}
			}
			pause(1);
			continue;
		}

		b = ns_recv_byte();
		if (b < 0) {
			continue;
		}
		buf[total++] = (uint8_t)b;
		idle = 0;
	}

	return total;
}

static int16_t netstream_read_raw(uint8_t *buf, int16_t len)
{
	return netstream_read_raw_limit(buf, len, 0);
}

static void netstream_send_buffer_with_lf(const uint8_t *buf, uint16_t len)
{
	uint16_t i;

	netstream_drain_input(NETSTREAM_DRAIN_IDLE_PAUSES, 0);
	for (i = 0; i < len; ++i) {
		while (ns_send_byte(buf[i]) != 0) {
			pause(1);
		}
	}
	while (ns_send_byte(NETSTREAM_COMMAND_EOL) != 0) {
		pause(1);
	}
}
#endif

void create_command(char *cmd) {
	memset(cmd_tmp, 0, 64);
	strcpy((char *) cmd_tmp, cmd);
	// cmd_tmp[strlen(cmd)] = '\0';
}

void append_command(char *cmd) {
	strcat((char *) cmd_tmp, " ");
	strcat((char *) cmd_tmp, cmd);
	// cmd_tmp[strlen((char *) cmd_tmp)] = '\0';
}

void send_command() {
	uint16_t len = strlen((char *) cmd_tmp);

#ifdef __ATARI__
	netstream_send_buffer_with_lf((uint8_t *)cmd_tmp, len);
#else
	/* append LF (0x0A); avoid "\n" which cc65 maps to 0x9B on Atari */
	cmd_tmp[len] = 0x0A;
	err = network_write(server_url, (uint8_t *) cmd_tmp, len + 1);
	cmd_tmp[len] = '\0';
	handle_err("send_command");
#endif
}

// just send the cached client data command
void request_client_data() {
	uint16_t len = client_data_cmd_len;

#ifdef __ATARI__
	netstream_send_buffer_with_lf((uint8_t *)client_data_cmd, len);
	netstream_client_data_read = 1;
#else
	/* append LF (0x0A); avoid '\n' which cc65 maps to 0x9B on Atari */
	client_data_cmd[len] = 0x0A;
	client_data_cmd[len + 1] = '\0';
	err = network_write(server_url, (uint8_t *) client_data_cmd, len + 1);
	client_data_cmd[len] = '\0';
	handle_err("request_client_data");
#endif
}

void connect_service() {
#ifdef __ATARI__
	uint16_t port_swapped;

	parse_netstream_url(&port_swapped);
	err = ns_init_netstream(netstream_host, NETSTREAM_FLAGS, NETSTREAM_BAUD, port_swapped);
	handle_err("netstream init");
	ns_begin_stream();
	pause(60);
#else
	fn_default_timeout = 2;
	err = network_open(server_url, 0x0C, 0);
	fn_default_timeout = 15;
	handle_err("connect");
#endif
}

void disconnect_service() {
	create_command("close");
	append_command(client_str);
	send_command();
#ifdef __ATARI__
	ns_end_stream();
#else
	network_close(server_url);
#endif
}

static uint16_t packet_size_from_header(const uint8_t *buf)
{
	return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static int16_t read_raw(uint8_t *buf, int16_t len)
{
#ifdef __ATARI__
	return netstream_read_raw(buf, len);
#else
	int16_t total = 0;
	int16_t n;

	while (total < len) {
		n = network_read(server_url, buf + total, len - total);
		if (n < 0) {
			err = -n;
			handle_err("read_raw");
			return total;
		}
		if (n == 0) {
			pause(3);
			continue;
		}
		total += n;
	}
	return total;
#endif
}

// read fully until we get payload_len bytes from a framed response
// Used for commands where we must receive all the data
int16_t read_response_wait(uint8_t *payload_buf, int16_t payload_len)
{
	uint8_t header[PACKET_HEADER_SIZE];
	uint16_t packet_total;
	int16_t n;

	n = read_raw(header, PACKET_HEADER_SIZE);
	if (n < PACKET_HEADER_SIZE) {
		return n;
	}

	packet_total = packet_size_from_header(header);
	if (packet_total != (uint16_t)(payload_len + PACKET_HEADER_SIZE)) {
#ifdef __ATARI__
		netstream_drain_input(NETSTREAM_DRAIN_IDLE_PAUSES, 1);
		return -1;
#else
		err = 1;
		handle_err("read_response_wait size");
		return -1;
#endif
	}

	n = read_raw(payload_buf, payload_len);
	if (n < 0) {
		return n;
	}
	return n;
}

// read a framed response into buf; payload begins at buf + PACKET_HEADER_SIZE
// (use app_payload when buf is app_data). Returns payload length, or -1 on error.
int16_t read_response_min(uint8_t *buf, int16_t min_payload, int16_t max_payload)
{
#ifdef __ATARI__
	int16_t n;
	uint16_t packet_total;
	int16_t max_total = (int16_t)(max_payload + PACKET_HEADER_SIZE);
	uint8_t first_byte_idle_limit = 0;

	if (netstream_client_data_read) {
		first_byte_idle_limit = NETSTREAM_FRAME_FIRST_BYTE_PAUSES;
		netstream_client_data_read = 0;
	}

	n = netstream_read_raw_limit(buf, PACKET_HEADER_SIZE, first_byte_idle_limit);
	if (n == 0) {
		return 0;
	}
	if (n < PACKET_HEADER_SIZE) {
		return -1;
	}

	packet_total = packet_size_from_header(buf);
	if (packet_total < PACKET_HEADER_SIZE ||
		packet_total < (uint16_t)(min_payload + PACKET_HEADER_SIZE) ||
		packet_total > (uint16_t)max_total) {
		netstream_drain_input(NETSTREAM_DRAIN_IDLE_PAUSES, 1);
		return -1;
	}

	n = netstream_read_raw(buf + PACKET_HEADER_SIZE,
		(int16_t)(packet_total - PACKET_HEADER_SIZE));
	if (n < (int16_t)(packet_total - PACKET_HEADER_SIZE)) {
		return -1;
	}

	return (int16_t)(packet_total - PACKET_HEADER_SIZE);
#else
	int16_t total = 0;
	int16_t n = 0;
	int16_t max_total = (int16_t)(max_payload + PACKET_HEADER_SIZE);
	int16_t need_total = (int16_t)(min_payload + PACKET_HEADER_SIZE);
	uint16_t packet_total = 0;
	uint8_t have_header = 0;

	while (total < need_total) {
		n = network_read_nb(server_url, buf + total, max_total - total);
		if (n < 0) {
			err = -n;
			handle_err("read_response_min");
			return -1;
		}
		if (n == 0) {
			pause(3);
			continue;
		}

		total += n;

		if (total >= PACKET_HEADER_SIZE) {
			if (!have_header) {
				packet_total = packet_size_from_header(buf);
				if (packet_total < PACKET_HEADER_SIZE ||
					packet_total > (uint16_t)max_total) {
					err = 1;
					handle_err("read_response_min bad size");
					return -1;
				}
				need_total = (int16_t)packet_total;
				have_header = 1;
			}
			if (total >= need_total) {
				break;
			}
		}
	}

	if (!have_header) {
		return 0;
	}

	if (total < need_total) {
		return (int16_t)(total - PACKET_HEADER_SIZE);
	}

	if (packet_total != packet_size_from_header(buf)) {
		err = 1;
		handle_err("read_response_min mismatch");
		return -1;
	}

	return (int16_t)(packet_total - PACKET_HEADER_SIZE);
#endif
}


void send_client_data() {
	char tmp[6]; // for the itoa string
	memset(tmp, 0, sizeof(tmp));

	// send x-add-client with "name,version,screenX,screenY", and get the client id back

	memset((char *) app_data, 0, 64);
	strcat((char *) app_data, name);
	strcat((char *) app_data, ",2,"); // version
	itoa(SCREEN_WIDTH, tmp, 10);
	strcat((char *) app_data, tmp);
	strcat((char *) app_data, ",");
	itoa(SCREEN_HEIGHT, tmp, 10);
	strcat((char *) app_data, tmp);

	create_command("x-add-client");
	append_command((char *) app_data);
	send_command();
	read_response_wait((uint8_t *) &client_id, 1);
	if (client_id == 0) {
		err = -1;
		handle_err("bad client id");
	}

	memset(client_str, 0, 8);
	itoa(client_id, client_str, 10);

	cputsxy(10, 19, "Client ID: ");
	cputsxy(21, 19, client_str);

	// create the cached client data command like "x-w <id>"
	create_client_data_command();

	press_key();

}
