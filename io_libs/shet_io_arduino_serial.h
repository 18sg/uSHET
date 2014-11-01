/**
 * An I/O interface wrapper for an Arduino serial stream.
 */

#ifndef SHET_IO_ARDUINO_SERIAL_H
#define SHET_IO_ARDUINO_SERIAL_H

#include <arduino.h>
#include <HardwareSerial.h>

#include "shet.h"

/**
 * A struct which maintains state for the wrapper.
 */
typedef struct {
	// The Arduino serial device to use. For example, Serial.
	HardwareSerial *serial;
	
	// Buffer for incoming lines from serial
	char buf[SHET_BUF_SIZE];
	char *buf_head;
} shet_io_arduino_serial_t;


/**
 * Initialise the state object.
 *
 * @param io Pointer to the shet_io_arduino_serial_t to initialise.
 * @param serial A pointer to an Arduino HardwareSerial object, e.g. "Serial".
 */
void shet_io_arduino_serial_init(shet_io_arduino_serial_t *io,
                                 HardwareSerial *serial);


/**
 * Transmit callback. Expects a pointer to shet_io_arduino_serial_t as the
 * user_data.
 */
void shet_io_arduino_serial_tx(const char *data, void *user_data);


/**
 * Receive data and pass it to uSHET for processing.
 *
 * @param io Pointer to the initialised shet_io_arduino_serial_t.
 * @param shet Pointer to the initialised shet_state_t.
 * @return Passes through the return value from shet_process_line. Also returns
 *         SHET_PROC_OK if no complete line has been received.
 */
shet_processing_error_t shet_io_arduino_serial_rx(shet_io_arduino_serial_t *io,
                                                  shet_state_t *shet);


#endif
