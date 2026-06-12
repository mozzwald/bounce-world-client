; cc65/ca65 wrappers for the MADS-built NETStream handler jump table.
;
; The handler is assembled separately at NS_BASE and concatenated before the
; cc65 executable as an Atari DOS load segment.

		.export _ns_begin_stream
		.export _ns_end_stream
		.export _ns_get_version
		.export _ns_get_base
		.export _ns_send_byte
		.export _ns_send_cmd_tmp
		.export _ns_send_client_data_cmd
		.export _ns_recv_byte
		.export _ns_bytes_avail
		.export _ns_get_status
		.export _ns_get_video_std
		.export _ns_init_netstream
		.export _ns_get_final_flags
		.export _ns_get_final_audf3
		.export _ns_get_final_audf4

		.import _cmd_tmp
		.import _client_data_cmd
		.importzp ptr1

NS_BASE = $2800
NETSTREAM_COMMAND_EOL = $0a

_ns_begin_stream:
		jsr		NS_BASE+0
		rts

_ns_end_stream:
		jsr		NS_BASE+3
		rts

_ns_get_version:
		jsr		NS_BASE+6
		ldx		#0
		rts

_ns_get_base:
		jsr		NS_BASE+9
		rts

; Input: A = byte. Output: A=0 ok, A=1 full.
_ns_send_byte:
		jsr		NS_BASE+12
		rts

; Input: A = length of _cmd_tmp payload. Sends payload plus ASCII LF.
_ns_send_cmd_tmp:
		sta		send_len
		lda		#<_cmd_tmp
		sta		ptr1
		lda		#>_cmd_tmp
		sta		ptr1+1
		jmp		send_buffer_with_lf

; Input: A = length of _client_data_cmd payload. Sends payload plus ASCII LF.
_ns_send_client_data_cmd:
		sta		send_len
		lda		#<_client_data_cmd
		sta		ptr1
		lda		#>_client_data_cmd
		sta		ptr1+1
		jmp		send_buffer_with_lf

send_buffer_with_lf:
		ldy		#0
send_loop:
		cpy		send_len
		beq		send_lf
		lda		(ptr1),y
		jsr		send_a_retry
		iny
		bne		send_loop
send_lf:
		lda		#NETSTREAM_COMMAND_EOL
		jsr		send_a_retry
		lda		#0
		rts

send_a_retry:
		sta		send_byte
retry_send:
		lda		send_byte
		jsr		NS_BASE+12
		cmp		#0
		bne		retry_send
		rts

; Output: A/X = byte (0-255), or $FFFF if empty.
_ns_recv_byte:
		jsr		NS_BASE+15
		ldx		#0
		rts

_ns_bytes_avail:
		jsr		NS_BASE+18
		rts

_ns_get_status:
		jsr		NS_BASE+21
		ldx		#0
		rts

_ns_get_video_std:
		jsr		NS_BASE+24
		ldx		#0
		rts

_ns_init_netstream:
		jmp		NS_BASE+27

_ns_get_final_flags:
		jsr		NS_BASE+30
		ldx		#0
		rts

_ns_get_final_audf3:
		jsr		NS_BASE+33
		ldx		#0
		rts

_ns_get_final_audf4:
		jsr		NS_BASE+36
		ldx		#0
		rts

		.bss
send_len:
		.res	1
send_byte:
		.res	1
