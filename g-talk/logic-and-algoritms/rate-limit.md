---
### License and Intellectual Property
© 2026 Emanuele Ferraro & GeoReport International.
This work (G-Mesh system logic, calculations, and technical documentation) is licensed under the Creative Commons Attribution-ShareAlike 4.0 International License (http://creativecommons.org/licenses/by-sa/4.0/).

The associated source code is released under the GNU GPL v3 license.
---
If two people transmit simultaneously on the same frequency, the signals overlap and become noise (the "Collision Problem"). To avoid this, we implement a protocol called CSMA/CA (Carrier Sense Multiple Access with Collision Avoidance).

Listen Before Talk (LBT): G-TALK listens to the channel for a microsecond before sending. If it senses another node transmitting, it waits a random amount of time (milliseconds) and tries again.

Queue Buffer: If two messages arrive at the node almost simultaneously, the G-NODE queues them (FIFO - First In, First Out). It sends the first one, waits for it to complete, and then sends the second one.

++ Before transmitting, the G-TALK waits a random few seconds as soon as the message is sent so as not to clog up nodes/satellites.
