---
### License and Intellectual Property
© 2026 Emanuele Ferraro & GeoReport International.
This work (G-Mesh system logic, calculations, and technical documentation) is licensed under the Creative Commons Attribution-ShareAlike 4.0 International License (http://creativecommons.org/licenses/by-sa/4.0/).

The associated source code is released under the GNU GPL v3 license.
---
The "Recognition" Problem (Routing Table)
For the system to know where the destination WT is located without clogging everything up, each device must send a "Heartbeat" (a tiny packet of a few bytes) every minute.

G-TALK says: "I am ID #777, I am locked to G-NODE #001, and I see the G-EYE #01 satellite."

This information is stored in a Routing Table synchronized between all nodes and the satellite. When you want to talk to ID #888, your G-TALK asks the network, "Where is 888?" The network responds, "It's on G-NODE #005." The packet goes straight there.

