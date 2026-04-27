---
### License and Intellectual Property
© 2026 Emanuele Ferraro & GeoReport International.
This work (G-Mesh system logic, calculations, and technical documentation) is licensed under the Creative Commons Attribution-ShareAlike 4.0 International License (http://creativecommons.org/licenses/by-sa/4.0/).

The associated source code is released under the GNU General Public License (GPL) v3.
---
Message that is 'translated', that's all, with variables:

(0 = false 1 = true)

0x02 (signals the start of the packet)

1: {ID} (sender)

2: {ID} (recipient)

1_3: True/False (is the sender a node?)

2_3: True/False (is the recipient a node?)

message = "binary code of the encrypted message"

0xs = "encrypted signature"

1xs = "public key hash"

0xx = 12345678

x = "random text"

sos = True/False

0x03 (signals the end of the packet)

We'll use enums for SOS/fast messages to save bytes. Here's the List:

xxr0 = Network/Connection Test

xxr = Heartbeat

xxr1 = How are you?

xxr2 = General SOS

xxr3 = I need medical attention

xxr4 = Serious Injury

xxr5 = Out of Supplies (Water/Food) or Technical Failure

xxr6 = All OK

xxr7 = I Found the Person or Object I'm Looking for

xxr8 = Confirmation of Receipt

xxr9 = Can Anyone Hear the Satellite?

xxr10 = Can Anyone Hear the Node?

xxr11 = Low Battery Warning

xxr12 = The weather is getting worse

xxr13 = I'm stopping/going back

xxr14 = OK

xxr15 = YES

xxr16 = NO

xxr17 = I don't know

xxr18 = There's a problem

xxr19 = Path blocked/landslide

xxr20 = Message received

xxr21 = I'm lost

xxr22 = There's no signal here

These will be available in the G-TALK interface
