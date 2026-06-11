#ifndef _CMOC_VERSION_
#include <conio.h>
#include <stdint.h>
#include <stdbool.h>
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
#ifndef __ATARI__
#include "fujinet-network.h"
#endif
#include "hex_dump.h"
#include "press_key.h"
#include "shapes.h"
#include "world.h"
#ifdef __ATARI__
#include "netstream/netstream.h"
#endif

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

#ifdef __ATARI__

#define NETSTREAM_FLAG_TCP     0x01
#define NETSTREAM_FLAG_TX_INT  0x00
#define NETSTREAM_FLAG_RX_EXT  0x08
#define NETSTREAM_FLAGS        (NETSTREAM_FLAG_TCP | NETSTREAM_FLAG_TX_INT | NETSTREAM_FLAG_RX_EXT)
#define NETSTREAM_BAUD         0xe100U
#define NETSTREAM_HOST_MAX     61
#define NETSTREAM_IDLE_PAUSES  2
#define NETSTREAM_COMMAND_EOL  0x0a

static char netstream_host[NETSTREAM_HOST_MAX + 1];
static uint16_t netstream_port;

static uint16_t swap16(uint16_t value) {
	return (uint16_t)(((value << 8) & 0xff00) | ((value >> 8) & 0x00ff));
}

static bool starts_with_ci(const char *s, const char *prefix) {
	while (*prefix) {
		char a = *s++;
		char b = *prefix++;
		if (a >= 'A' && a <= 'Z') {
			a = (char)(a + ('a' - 'A'));
		}
		if (b >= 'A' && b <= 'Z') {
			b = (char)(b + ('a' - 'A'));
		}
		if (a != b) {
			return false;
		}
	}
	return true;
}

static void parse_netstream_url(void) {
	const char *p = server_url;
	const char *colon;
	uint8_t host_len;
	uint16_t port;

	memset(netstream_host, 0, sizeof(netstream_host));
	netstream_port = 0;

	if (starts_with_ci(p, "n1:")) {
		p += 3;
	}
	if (starts_with_ci(p, "tcp://")) {
		p += 6;
	}

	colon = strchr(p, ':');
	if (colon == NULL || colon == p) {
		err = 1;
		handle_err("bad netstream url");
	}

	host_len = (uint8_t)(colon - p);
	if (host_len > NETSTREAM_HOST_MAX) {
		err = 1;
		handle_err("netstream host too long");
	}

	memcpy(netstream_host, p, host_len);
	netstream_host[host_len] = '\0';

	port = (uint16_t)atoi(colon + 1);
	if (port == 0) {
		err = 1;
		handle_err("bad netstream port");
	}
	netstream_port = port;
}

void send_command() {
	if (ns_send_cmd_tmp((uint8_t)strlen((char *) cmd_tmp)) != 0) {
		err = 1;
		handle_err("send_command");
	}
}

// just send the cached client data command
void request_client_data() {
	if (ns_send_client_data_cmd(client_data_cmd_len) != 0) {
		err = 1;
		handle_err("request_client_data");
	}
}

void connect_service() {
	parse_netstream_url();
	err = ns_init_netstream(netstream_host, NETSTREAM_FLAGS, NETSTREAM_BAUD, swap16(netstream_port));
	handle_err("netstream init");
	ns_begin_stream();
}

void disconnect_service() {
	if (client_id != 0) {
		create_command("close");
		append_command(client_str);
		send_command();
	}
	ns_end_stream();
}

// read fully until we get len bytes
// Used for commands where we must receive all the data
int16_t read_response_wait(uint8_t *buf, int16_t len) {
	int16_t total = 0;
	while (total < len) {
		if (ns_bytes_avail() == 0) {
			continue;
		}
		buf[total] = (uint8_t)ns_recv_byte();
		++total;
	}
	return total;
}

// read at least min bytes. Forces something to be read, not skip if there's no bytes waiting
int16_t read_response_min(uint8_t *buf, int16_t min, int16_t max) {
	int16_t total = 0;
	uint8_t idle_pauses = 0;

	while (total < min) {
		if (ns_bytes_avail() == 0) {
			pause(3); // about 50ms
			continue;
		}
		buf[total] = (uint8_t)ns_recv_byte();
		++total;
	}
	while (total < max && idle_pauses < NETSTREAM_IDLE_PAUSES) {
		if (ns_bytes_avail() == 0) {
			++idle_pauses;
			pause(3);
			continue;
		}
		idle_pauses = 0;
		while (total < max && ns_bytes_avail() != 0) {
			buf[total] = (uint8_t)ns_recv_byte();
			++total;
		}
	}
	return total;
}

#else

void send_command() {
	// gotoxy(0, 0);
	// hd(cmd_tmp, 64);
	// cgetc();
	err = network_write(server_url, (uint8_t *) cmd_tmp, strlen((char *) cmd_tmp));
	handle_err("send_command"); 
}

// just send the cached client data command
void request_client_data() {
	err = network_write(server_url, (uint8_t *) client_data_cmd, client_data_cmd_len);
	handle_err("request_client_data");
}

void connect_service() {
	fn_default_timeout = 2;
	err = network_open(server_url, 0x0C, 0);
	fn_default_timeout = 15;
	handle_err("connect");
}

void disconnect_service() {
	create_command("close");
	append_command(client_str);
	send_command();
	network_close(server_url);
}

// read fully until we get len bytes
// Used for commands where we must receive all the data
int16_t read_response_wait(uint8_t *buf, int16_t len) {
	int16_t n;
	n = network_read(server_url, buf, len);
	if (n < 0) {
		err = -n;
		handle_err("read_response_wait");
	}
	return n;
}

// read at least min bytes. Forces something to be read, not skip if there's no bytes waiting
int16_t read_response_min(uint8_t *buf, int16_t min, int16_t max) {
	int16_t n = 0;
	int16_t total = 0;
	while (total < min) {
		n = network_read_nb(server_url, buf + total, max - total);
		if (n < 0) {
			err = -n;
			handle_err("read_response_min");
		}
		// if we got no data, pause slightly and try again. this compensates for network latency
		if (n == 0) {
			pause(3); // about 50ms
			continue;
		}

		total += n;
	}
	return total;
}

#endif


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
