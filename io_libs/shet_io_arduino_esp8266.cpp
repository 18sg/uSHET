#include "Arduino.h"
#include "shet_io_arduino_esp8266.h"

/**
 * Ignore all data currently in the serial buffer.
 */
static bool
flush_buffer(Stream *stream) {
	while (stream->available()) {
		while (stream->available()) {
			stream->read();
		}
		
		// Delay slightly to ensure we catch any characters in a stream which is
		// currently being transmitted.
		delay(1);
	}
	
	return true;
}

/**
 * A thin wrapper around Stream.print which always returns true.
 */
static bool
send_string(Stream *stream, const char *str) {
	stream->print(str);
	return true;
}

/**
 * A thin wrapper around Stream.print which always returns true.
 */
static bool
send_int(Stream *stream, int num) {
	stream->print(num);
	return true;
}

/**
 * Ignore all incoming serial data until the supplied string is matched. Returns
 * true if the string is matched and 0 if a timeout occurs.
 */
static bool
ignore_until_string(Stream *stream, const char *str_) {
	const char *str = str_;
	unsigned long start_time = millis();
	
	while (*str != '\0' && millis() - start_time < SHET_IO_ARDUINO_ESP8266_TIMEOUT) {
		if (stream->available()) {
			char c = stream->read();
			if (*str == c)
				str++;
			else {
				str = str_;
			}
		}
	}
	
	return *str == '\0';
}


/**
 * Verify that the incoming serial stream exactly matches the supplied string.
 * Returns true if found and false if a timeout or mismatch occurs. If a
 * mismatch occurs, the first letter to mismatch will remain in the buffer.
 */
static bool
expect_string(Stream *stream, const char *str) {
	unsigned long start_time = millis();
	
	while (*str != '\0' && millis() - start_time < SHET_IO_ARDUINO_ESP8266_TIMEOUT) {
		if (stream->available()) {
			char cur = *str;
			char got = stream->peek();
			if (cur != got) {
				return false;
			} else {
				stream->read();
				str++;
			}
		}
	}
	
	return *str == '\0';
}


/**
 * Attempt to connect to WiFi and the SHET server. Returns true on success and
 * false otherwise. Warning this function blocks. Note that if the ESP8266 is
 * already connected to SHET, sending "AT\r\n" will cause the SHET server to
 * disconnect it.
 */
static bool
reconnect(shet_io_arduino_esp8266_t *io) {
	return // Flush everything
	       send_string(io->serial, "AT\r\n")
	    && flush_buffer(io->serial)
	       // Check the device is alive
	    && send_string(io->serial, "AT\r\n")
	    && ignore_until_string(io->serial, "\r\nOK\r\n")
	       // Select station mode (and hope/assume this works to save the effort
	       // of checking for the response given that echo may/maynot be enabled
	    && send_string(io->serial, "AT+CWMODE=1\r\n")
	    && flush_buffer(io->serial)
	       // Station mode selection requires reset if changed. Additionally,
	       // resetting will conveniently cancel any existing connections.
	    && send_string(io->serial, "AT+RST\r\n")
	    && ignore_until_string(io->serial, "\r\nOK\r\n")
	    && ignore_until_string(io->serial, "eady") // Ignores case of 'R'
	    && ignore_until_string(io->serial, "\r\n") // Ignores vendor message
	       // Turn off echo
	    && send_string(io->serial, "ATE0\r\n")
	    && ignore_until_string(io->serial, "\r\nOK\r\n")
	       // Check that worked...
	    && send_string(io->serial, "AT\r\n")
	    && expect_string(io->serial, "\r\nOK\r\n")
	       // Connect to WiFi
	    && send_string(io->serial, "AT+CWJAP=\"")
	    && send_string(io->serial, io->ssid)
	    && send_string(io->serial, "\",\"")
	    && send_string(io->serial, io->passphrase)
	    && send_string(io->serial, "\"\r\n")
	    && expect_string(io->serial, "\r\nOK\r\n")
	       // Single connection mode.
	    && send_string(io->serial, "AT+CIPMUX=0\r\n")
	    && expect_string(io->serial, "\r\nOK\r\n")
	       // Enable transparent connection mode.
	    && send_string(io->serial, "AT+CIPMODE=1\r\n")
	    && expect_string(io->serial, "\r\nOK\r\n")
	       // Connect to the SHET server
	    && send_string(io->serial, "AT+CIPSTART=\"TCP\",\"")
	    && send_string(io->serial, io->hostname)
	    && send_string(io->serial, "\",")
	    && send_int(io->serial, io->port)
	    && send_string(io->serial, "\r\n")
	    && expect_string(io->serial, "\r\nOK\r\n")
	    && expect_string(io->serial, "Linked\r\n")
	       // Initiate transparent mode (all data sent/received will now be
	       // sent/recieved to/from the SHET server.
	    && send_string(io->serial, "AT+CIPSEND\r\n")
	    && expect_string(io->serial, "\r\n>")
	       ;
}


void
shet_io_arduino_esp8266_init(shet_io_arduino_esp8266_t *io,
                             Stream *serial,
                             const char *ssid,
                             const char *passphrase,
                             const char *hostname,
                             int port) {
	io->serial = serial;
	io->len = 0;
	
	io->ssid       = ssid;
	io->passphrase = passphrase;
	io->hostname   = hostname;
	io->port       = port;
	
	io->connected = reconnect(io);
}

static void
receive_data(shet_io_arduino_esp8266_t *io) {
	io->len = 0;
	
	// Attempt to receive any available data
	io->serial->print("?\r\n");
	if (expect_string(io->serial, ":")) {
		// Read whole line with timeout
		char c = '\0';
		unsigned long start_time = millis();
		while (c != '\n' && millis() - start_time < SHET_IO_ARDUINO_ESP8266_TIMEOUT) {
			if (io->serial->available()) {
				c = io->serial->read();
				io->buf[io->len++] = c;
			}
		}
	} else if (expect_string(io->serial, "\r\n")) {
		// Nothing to receive!
	} else {
		// Fail!
		io->connected = false;
	}
}

void shet_io_arduino_esp8266_tx(const char *data, void *user_data) {
	shet_io_arduino_esp8266_t *io = (shet_io_arduino_esp8266_t *)user_data;
	
	// Don't bother trying to send if not connected.
	if (io->connected) {
		io->serial->print(data);
	}
}


shet_processing_error_t shet_io_arduino_esp8266_rx(shet_io_arduino_esp8266_t *io,
                                                   shet_state_t *shet) {
	// Get any new data from the network
	if (io->connected)
		receive_data(io);
	
	// If a complete line was found, feed it to SHET.
	if (io->len) {
		shet_processing_error_t e = shet_process_line(shet, io->buf, io->len);
		
		// Reconnect if SHET couldn't parse the line (e.g. if we got "Unlink" or
		// similar from the ESP8266).
		if (e != SHET_PROC_OK)
			io->connected = false;
	}
	
	// Reconnect if not connected
	if (!io->connected) {
		if (reconnect(io)) {
			io->connected = true;
			shet_reregister(shet);
		}
	}
	
	// XXX: Always just pretend everything went OK...
	return SHET_PROC_OK;
}


bool
shet_io_arduino_esp8266_connected(shet_io_arduino_esp8266_t *io) {
	return io->connected;
}
