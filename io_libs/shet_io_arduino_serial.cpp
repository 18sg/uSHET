#include "shet_io_arduino_serial.h"


void shet_io_arduino_serial_init(shet_io_arduino_serial_t *io,
                                 HardwareSerial *serial) {
	io->serial = serial;
	io->buf_head = &(io->buf[0]);
}

void shet_io_arduino_serial_tx(const char *data, void *user_data) {
	((shet_io_arduino_serial_t *)user_data)->serial->print(data);
}


shet_processing_error_t shet_io_arduino_serial_rx(shet_io_arduino_serial_t *io,
                                                  shet_state_t *shet) {
	while (io->serial->available()) {
		// Buffer is full, just wrap-around since uSHET will absorb the error.
		if ((io->buf_head - &(io->buf[0])) / sizeof(char) > SHET_BUF_SIZE) {
			io->buf_head = &(io->buf[0]);
		}
		
		// Read the next character
		*(io->buf_head++) = io->serial->read();
		
		// If we've reached a newline; pass the string to SHET.
		if (*(io->buf_head-1) == '\n') {
			char *line = &(io->buf[0]);
			size_t len = (io->buf_head - &(io->buf[0])) / sizeof(char);
			io->buf_head = &(io->buf[0]);
			return shet_process_line(shet, io->buf, len);
		}
	}
	
	return SHET_PROC_OK;
}
