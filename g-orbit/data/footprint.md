---
### License and Intellectual Property
© 2026 Emanuele Ferraro & GeoReport International Technologies.
This work (G-Mesh system logic, calculations, and technical documentation) is licensed under the Creative Commons Attribution-ShareAlike 4.0 International License (http://creativecommons.org/licenses/by-sa/4.0/).

The associated source code is released under the GNU GPL v3 license.
---

The interception cone is the cone in which the G-ORBIT radio signal is active and functioning.

## VAR
Height: *500km*
Movement Speed: *27,000 km/h - 7.5 km/s*
Angle: *90° - 45° per side*
Apothem: *707,100 km*
Radius: *500 km*
Diameter: *1,000 km*
Area = 78,000,000 km²

How we arrived at this data:

Data we had available:
Height, Movement Speed, Angle.

We divided the cone into two equal parts (Right and Left) and established a distance of 500 meters from the satellite, thus creating a right-angled triangle. Then, knowing the height and base of that small triangle, we used the Pythagorean theorem to find its apothem (a).

Formula:
a = *√ 500² + 500² = 707.1 km*
We have defined that we can divide the triangle created initially, which bisects the cone, into 1000 equal parts.
500000
----------- = 500
1000

So, to find the true apothem, we multiplied the result obtained from the apothem of the small right-angled triangle by a thousand times, that is, the number of times we divided the right-angled triangle of the cone.

707,100 x 1000 = 707,100

To calculate the radius, and therefore the diameter, we used the Pythagorean theorem:
*√ a² + h² = Radius*
r = *√ 707,100² + 500,000² = 500km*

So:
r = 500km
d = 2r = 1000km

Calculating a diameter of 210km from east to west (Latina - Peschici) and taking into account the following data:
latency = 3s
velocity (v) = 27,000 km/h - 7.5 km/s
d = 1000 km

We deduced that the satellite signal is fully available for *∼37.44 seconds*
Procedure:

km / d
210 / 1000 = 4.8 (how many times those 210 km fit within the diameter of the cone)
The time it takes to cross those 210 km is equal to:
km / km/s
210 / 7.5 = *∼28*
4.8 x time
4.8 x 28 = *∼134.4 seconds*

A = *3.14 x 500 (km) x 500 (km)* = 78,000 km²

Accounting for a latency of about 5 seconds for each communication, we can make a total of about 26 different communications as the satellite passes.

**Window Duration:** $133.3 \text{ s}$
**Communication Slots:** 26 slots (@ 5s latency)
**Efficiency Note:** If we optimize the encryption software, we can get latency down to 2s, bringing the slots to over 60.

## IF YOU SEE ANY ERROR IN THER, PLEASE, REPORT IT TO US. LAB@G-MESH.ORG
