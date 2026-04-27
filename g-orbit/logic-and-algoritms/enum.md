---
### Licenza e Proprietà Intellettuale
© 2026 Emanuele Ferraro & GeoReport International.
Quest'opera (logica di sistema, calcoli e documentazione tecnica di G-Mesh) è distribuita con Licenza [Creative Commons Attribuzione - Condividi allo stesso modo 4.0 Internazionale](http://creativecommons.org/licenses/by-sa/4.0/).

Il codice sorgente associato è invece rilasciato sotto licenza GNU GPL v3.
---
messaggio che viene 'tradotto', tutto qui, con delle variabili:

(0 = false 1 = true)

0x02 (segnala l'inizio del pacchetto)

1: {ID} (mittente)

2: {ID} (destinatario)

1_3: True/False (il mittente è un nodo?)

2_3: True/False (il destinatario è un nodo?)

message = "codice binario del messaggio criptografato"

0xs = "firma cittografata" (signature)

1xs = "hash della chiave pubblica" 

0xx = 12345678

x = "testo casuale"

sos = True/False

0x03 (segnala la chiusura del pacchetto)


utilizzeremo degli enum (enumerazioni) per dei messaggi di sos/messaggi veloci per risparmiare byte, ecco la lista:

xxr0 = test di rete/connessione

xxr = heartbeat

xxr1 = Come stai?

xxr2 = SOS generico

xxr3 = Ho bisogno di un medico

xxr4 = infortunio grave

xxr5 = Esaurimento scorte (acqua/cibo) o guasto tecnico

xxr6 = Tutto bene

xxr7 = Ho trovato la persona o l'oggetto cercato

xxr8 = Conferma di ricezione

xxr9 = Qualcuno sente il satellite?

xxr10 = Qualcuno sente il nodo?

xxr11 = Low Battery Warning

xxr12 = Il meteo sta peggiorando

xxr13 = Mi fermo/Torno indietro

xxr14 = OK

xxr15 = SI

xxr16 = NO

xxr17 = Non lo so

xxr18 = c'è un problema

xxr19 = Sentiero bloccato / frana

xxr20 = Messaggio ricevuto

xxr21 = Mi sono perso

xxr22 = Qui non c'è campo

Quest'ultimi saranno disponibili nell'interfaccia del G-TALK
