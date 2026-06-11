#ifndef BWC_ATARI_NETSTREAM_H
#define BWC_ATARI_NETSTREAM_H

#include <stdint.h>

void __fastcall__ ns_begin_stream(void);
void __fastcall__ ns_end_stream(void);
uint8_t __fastcall__ ns_get_version(void);
uint16_t __fastcall__ ns_get_base(void);
uint8_t __fastcall__ ns_send_byte(uint8_t b);
uint8_t __fastcall__ ns_send_cmd_tmp(uint8_t len);
uint8_t __fastcall__ ns_send_client_data_cmd(uint8_t len);
int16_t __fastcall__ ns_recv_byte(void);
uint16_t __fastcall__ ns_bytes_avail(void);
uint8_t __fastcall__ ns_get_status(void);
uint8_t __fastcall__ ns_get_video_std(void);
uint8_t __fastcall__ ns_init_netstream(const char *host,
                                       uint8_t flags,
                                       uint16_t nominal_baud,
                                       uint16_t port_swapped);
uint8_t __fastcall__ ns_get_final_flags(void);
uint8_t __fastcall__ ns_get_final_audf3(void);
uint8_t __fastcall__ ns_get_final_audf4(void);

#endif
