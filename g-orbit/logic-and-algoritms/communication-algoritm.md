---
### License and Intellectual Property
© 2026 Emanuele Ferraro & GeoReport International Technologies.
This work (G-Mesh system logic, calculations, and technical documentation) is licensed under the Creative Commons Attribution-ShareAlike 4.0 International License (http://creativecommons.org/licenses/by-sa/4.0/).

The associated source code is released under the GNU General Public License (GPL) v3.
---
Legend:
Walkye-talkie=wt
node=nd
satellite=st
If a walkye-talkie needs to communicate with another wt, then

wt-->pqc-->node-->node-->wt-->pqc decript-->wt <-- the decryption and encryption take place in the wt

wt-->pqc-->st-->nd-->wt-->pqc decript-->wt

wt-->pqc-->st-->wt-->pqc decript-->

nd-->pqc-->wt-->pqc decript-->wt <-- if in the same area

nd-->pqc-->st-->nd-->pqc decript <-- if they are distant

nd-->pqc-->st-->nd-->wt-->pqc decript <-- always talks to the node if the wt is within range

each wt sends data to the closest satellite/node as soon as something changes
parameters:
connected to node: True/False + node ID
connected to satellite: True/False --> satellite ID (if we have more than one, it's always best to put it first)
pqc: True/False <-- if you want to activate/deactivate; if deactivated, everyone will hear, including passing nodes
Dest: {ID} <-- ID of the destination wt or nd, each update will be sent to the satellite and will automatically update all wt and nodes.

Rule number one is: everyone minds their own business. If a pqc communication passes through a node, it will never know. We focus on privacy. If someone in Iran who has a node needs to talk to us, no one must know it's coming from there, no one except the recipient.

++ onion routing (everyone only knows the previous and next steps)
++ privacy ID --> your ID is replaced by a temporary one that changes every 10 minutes. To avoid being recognized, it is communicated only to the satellite/node in an encrypted form that is unrecognizable to the human eye.

We need to mathematically find a way to compress the packets as much as possible to minimize latency while maintaining full PQC, so we should find a formula that allows us to do this.

We need to understand how everything recognizes everything, so I'll tell you.

Case 1: wt-->pqc-->node-->node-->wt-->pqc decript-->wt <-- the decript and the crypt occur in the wt.

If wt1 and wt2 are connected to different nodes, but are still connected to nodes, then the parameter linked to the node: True/False + node ID of both shows a different ID.

Case 2: wt-->pqc-->st-->nd-->wt-->pqc decript-->wt
If wt1 is not connected to a node (parameter linked to the node: True/False + node ID) but wt2 is, the node is the main source to which data is sent since they are ultra-fast and stable copper antennas, compared to the tiny ones of the wt.

Case 3: wt-->pqc-->st-->wt-->pqc decript
when both are not connected to a node (parameter connected to the node: True/False + node ID)

case 4: nd-->pqc-->wt-->pqc decript-->wt <-- if in the same area
This needs to be reviewed, because it's not very reliable; it can't tell if it has a nearby wt. Discarded, in this case we go to case 3.

Case 5: nd-->pqc-->st-->nd-->pqc decript <-- if they are far away
communication between nodes, even the nodes can communicate (do you think this is okay or would it clog up the network? They would be like advanced radio stations)

Case 6: nd-->pqc-->st-->nd-->wt-->pqc decript <-- always talks to the node if the wt is within range
node-wt communication, if the node wants to communicate with a wt, it can be reversed, the wt is not connected to any node (parameter connected to the node: True/False + node ID)

## IF YOU SEE ANY ERROR IN THERE, PLEASE, REPORT IT TO US DESK@G-MESH.ORG

