/**
 * An I/O interface wrapper for an ESP8266 WiFi module connected to an Arduino
 * serial port.
 *
 * This library assumes complete control over the ESP8266 and is known to work
 * with version 0.9.2 of the AT firmware. Since this version of the firmware has
 * known issues with full-duplex communication, this implementation must connect
 * to SHET via the included proxy server `shet_io_arduino_esp8266_proxy.py`.
 * This proxy allows the device to poll for data from the SHET server ensuring
 * half-duplex communication.
 */

#ifndef SHET_IO_ARDUINO_ESP8266_H
#define SHET_IO_ARDUINO_ESP8266_H

// Timeout for responses from the ESP8266 module. Must be long enough for WiFi
// connection to occur.
#define SHET_IO_ARDUINO_ESP8266_TIMEOUT 5000ul

#include <arduino.h>
#include <Stream.h>

#include "shet.h"

/**
 * A struct which maintains state for the wrapper. Not for external use.
 */
typedef struct {
	// The Arduino serial device to use. For example, Serial.
	Stream *serial;
	
	// Has a TCP connection been established?
	bool connected;
	
	// The SSID to connect to
	const char *ssid;
	
	// The Passphrase for the wifi network
	const char *passphrase;
	
	// The SHET Server hostname
	const char *hostname;
	
	// The SHET Server port
	int port;
	
	// Buffer for data being passed to SHET
	char buf[SHET_BUF_SIZE];
	// Length of line currently in buffer
	size_t len;
	
	// Number of characters remaining from an IPD command from the module
	int ipd_count;
} shet_io_arduino_esp8266_t;


/**
 * Initialise the state object.
 *
 * @param io Pointer to the shet_io_arduino_esp8266_t to initialise.
 * @param serial A pointer to an Arduino Stream object, e.g. "Serial".
 * @param ssid The SSID of the WiFi network to connect to.
 * @param passphrase The passphrase for the WiFi network.
 * @param hostname The hostname of the SHET server.
 * @param port The port of the SHET server (typically 11235).
 */
void shet_io_arduino_esp8266_init(shet_io_arduino_esp8266_t *io,
                                  Stream *serial,
                                  const char *ssid,
                                  const char *passphrase,
                                  const char *hostname,
                                  int port);


/**
 * Transmit callback for uSHET. Expects a pointer to shet_io_arduino_esp8266_t
 * as the user_data.
 */
void shet_io_arduino_esp8266_tx(const char *data, void *user_data);


/**
 * Receive data and pass it to uSHET for processing. Note that this command may
 * block for some time while attempting to connect to WiFi and SHET.
 *
 * This command will repeatedly attempt to reconnect to the SHET server (calling
 * shet_reregister() as appropriate).
 *
 * @param io Pointer to the initialised shet_io_arduino_esp8266_t.
 * @param shet Pointer to the initialised shet_state_t.
 * @return Always returns SHET_PROC_OK.
 */
shet_processing_error_t shet_io_arduino_esp8266_rx(shet_io_arduino_esp8266_t *io,
                                                   shet_state_t *shet);


/**
 * Are we currently connected to the SHET server?
 */
bool shet_io_arduino_esp8266_connected(shet_io_arduino_esp8266_t *io);


#endif
