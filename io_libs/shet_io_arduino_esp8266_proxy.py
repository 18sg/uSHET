#!/usr/bin/env python

"""
A proxy server for SHET which allows a client to run in a polling mode. Intended
for use with the ESP8266 I/O library for uSHET.

This proxy is made neccessary by the current ESP8266 firmwares being
exceptionally unreliable when handling full-duplex communication, This proxy
forces the uSHET client to poll for incoming data thus ensuring that no
full-duplex communication occurs. It is hoped that one day this defficiency will
be eliminated, along with the need for this module.

SHET data recieved from a connected client is immediately forwarded to the SHET
server. If a (\r\n terminated) line containing "?" is received, the proxy will
respond with an empty line if no data is available or a colon (:) followed by a
line of SHET data recieved from the SHET server otherwise.

Adapted from http://stackoverflow.com/a/15645169/221061 and
http://www.mostthingsweb.com/2013/08/a-basic-man-in-the-middle-proxy-with-twisted/
"""

from twisted.internet import reactor, protocol
from twisted.protocols.basic import LineReceiver


class ServerProtocol(LineReceiver):
	
	SHET_SERVER = "127.0.0.1"
	SHET_PORT = 11235
	
	def __init__(self):
		# Buffer for data received from the client while not connected to the server
		self.tx_buffer = []
		
		# Buffer for lines received from the server and not yet sent to the client
		self.rx_buffer = []
		
		# The client Protocol connected to the real server
		self.client = None
	
	def connectionMade(self):
		# Create a factory to handle the comms from the server
		factory = protocol.ClientFactory()
		factory.protocol = ClientProtocol
		
		# Add a reference to the client connection
		factory.server = self
		
		# Connect to the server
		reactor.connectTCP(ServerProtocol.SHET_SERVER, ServerProtocol.SHET_PORT, factory)
	
	# Client => Proxy
	def lineReceived(self, line):
		if line == b"?":
			# "RX" request, respond with ":data" or nothing.
			if self.rx_buffer:
				self.sendLine(b":" + self.rx_buffer.pop(0))
			else:
				self.sendLine(b"")
		else:
			# TX data, forward to server (if connected)
			if self.client:
				self.client.sendLine(line)
			else:
				self.tx_buffer.append(line)
	
	def connectionLost(self, reason):
		if self.client:
			self.factory.server = None
			self.client.transport.loseConnection()


class ClientProtocol(LineReceiver):
	
	def connectionMade(self):
		# Add a reference to the server connection
		self.factory.server.client = self
		
		# Write out anything that built up in the tx_buffer
		for line in self.factory.server.tx_buffer:
			self.sendLine(line)
		self.factory.server.tx_buffer = None
	
	# Server => Proxy
	def lineReceived(self, line):
		self.factory.server.rx_buffer.append(line)
	
	def connectionLost(self, reason):
		if self.factory.server:
			self.factory.server.client = None
			self.factory.server.transport.loseConnection()



if __name__ == '__main__':
	import argparse
	parser = argparse.ArgumentParser(description=
		"A SHET proxy which eliminates any full-duplex communication "
		"for buggy ESP8266 firmwares.")
	
	parser.add_argument("--shet-server", dest="shet_server", type=str,
	                    default="127.0.0.1",
	                    help="SHET Server Address/IP")
	parser.add_argument("--shet-port", dest="shet_port", type=int,
	                    default=11235,
	                    help="SHET Server Port")
	parser.add_argument("--proxy-port", dest="proxy_port", type=int,
	                    default=11236,
	                    help="SHET Server Proxy Port")
	args = parser.parse_args()
	
	ServerProtocol.SHET_SERVER = args.shet_server
	ServerProtocol.SHET_PORT   = args.shet_port
	
	factory = protocol.ServerFactory()
	factory.protocol = ServerProtocol
	
	reactor.listenTCP(args.proxy_port, factory)
	reactor.run()
