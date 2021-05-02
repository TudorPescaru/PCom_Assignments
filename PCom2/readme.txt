Copyright 2021 Pescaru Tudor-Mihai 321CA

Protocol de Nivel Aplicatie

Protocolul de nivel aplicatie definit si utilizat in implementarea temei se 
bazeaza pe o serie de mesaje standard ce sunt definite printr-un tip si un 
payload de mici dimensiuni ce va contine doar cateva informatii suplimentare 
dupa caz, in functie de tipul mesajului. Mesajele standard din cadrul 
protocolului sunt de 6 tipuri iar fiecare are un rol diferit in comunicarea 
dintre server si clienti. Primul tip de mesaj este mesajul de exit, acesta 
fiind trimis de server tuturor clientilor conectati, atunci cand serverul 
primeste comanda de exit. Un alt tip de mesaj este cel de confirmare, trimis in 
urma unei cereri de subscribe/unsubscribe de la clientii TCP. Clienti se vor 
folosii de mesaje de tip ID pentru a-si trimite ID-ul la server, de mesaje de 
tip SUB/UNSUB pentru a realiza operatiile de subscribe si unsubscribe. In cazul 
mesajului de tip ID, acesta contine ID-ul clientului in payload-ul sau. Pentru 
mesajele de SUB si UNSUB, payload-ul mesajului contine o structura cu numele 
topic-ului la care sa da subscribe/unsubscribe si o valoare de SF pentru 
operatia de subscribe. In cazul mesajelor de exit sau de confirmare, payload-ul 
mesajului este gol deoarece nu este necesara transmiterea altor informatii. 
Ultimul tip de mesaj, cel ce sta la baza comunicatiei dintre server si client 
este mesajul de tip incoming packet sau PKT asa cum este referentiat in cod. 
Acest mesaj are rolul de a transmite clientului ca urmeaza sa primeasca, 
imediat dupa, un packet ce provine de la clientii UDP. In cadrul payload-ului 
acestui mesaj se afla o structura de tip size care va contine dimensiunea 
totala a packet-ului ce urmeaza sa fie primit, cat si dimensiunea individuala a 
topic-ului si a continutului din packet deoarece acestea sunt singurele doua 
campuri cu lungimi variabile. Clientul se va folosii de dimensiunea transimsa 
pentru a mai face un receive prin care va primi packet-ul, in forma sa cat mai 
compresata. Apoi, se va folosii de celelalte dimensiuni primite cat si de 
dimensiunile standard ale celorlalte componente din packet pentru a-l 
reconstrui intr-o structura mai usor utilizabila. Aceasta implementare asigura 
o comunicare cat mai eficienta intre clientii TCP si server.

Debug mode

In cadrul codului serverului si al subscriber-ului se afla o variabila ce poate 
fi setata pe true sau pe false, aceasta reprezentand pornirea modului de debug 
in care se vor afisa mesaje de eroare la STDERR acolo unde va fi nevoie. Am 
lasat aceasta variablia pe true deoarece am considerat ca este importanta 
afisarea de mesaje de eroare.

Server

Serverul va creea doua socket-uri, unul pe care v-a primii transmisiunile de la 
clientii UDP iau unul pe care va asculta pentru cereri de conexiuni de la 
clientii TCP. Serverul se va folosi de niste structuri sub forma de obiecte, 
in care vor fi stocate toate informatiile aferente unui client. Pentru fiecare 
client, serverul va stoca id-ul, fd-ul socket-ului curent, starea de conectare 
a clientului, un dictionar de perechi intre topic-urile la care este subscribed 
acesta si valoarea parametrului SF pentru fiecare topic si o coada in care se 
vor stoca pachetele primite cat timp clientul este deconectat. Aceasta coada va 
fi golita prin trimiterea tuturor pachetelor catre client in momentul in care 
acesta se reconecteaza. Serverul va folosi si o serie de structuri de date 
pentru a asigura o functionare eficienta. Se foloseste un dictionar ce leaga 
fiecare socket fd de id-ul clientului conectat pe socket-ul respectiv, un 
dictionar ce leaga fiecare id de obiectul ce contine datele clientului cu id-ul 
respectiv, un dictionar ce leaga numele fiecarui topic de o lista de clienti ce 
sunt subscribed la el si un set de referinte catre obictele ce contin informatii 
despre clienti pentru dezalocarea memoriei la finalul executiei programului. 
Ciclul de rulare al serverului incepe prin selectarea file descriptor-ilor de 
pe care s-a primit informatie.
In cazul in care s-a primit informatie de la STDIN, aceasta poate fi doar 
comanda exit, acest lucru fiind verificat pentru a evita citirea unei comenzi 
inexistente. In cazul unei alte comenzi se va afisa un mesaj de eroare. In 
cazul in care comanda de exit a fost introdusa, se va porni secventa de exit 
in care se trimite un mesaj de exit la toti clientii conectati si conexiunile 
lor se vor inchide, urmand mai apoi ca serverul in sine sa se inchida.
In cazul in care serverul a primit informatie de pe socket-ul UDP, aceasta 
informatie este un packet standard primit de la un client UDP. La acest packet 
se adauga informatia clientului UDP de la care s-a primit packet-ul (IP-ul si 
portul) iar mai apoi toata aceasta informatie va fi trimisa, in mod eficient, 
fara padding sau valori garbage, in urma trimiterii unui mesaj de incoming 
packet, astfel urmand metodologia descrisa de protocolul de nivel aplicatie, 
tuturor clientilor care sunt abonati topic-ului caruia ii apartine mesajul. In 
cazul in care un client este deconectat si are functionalitatea de SF pornita 
pe topic-ul respectiv, packet-ul va fi pastrat in coada specifica clientului 
respectiv, coada ce va fi golita la reconectarea clientului.
In cazul in care s-a primit informatie pe socket-ul TCP, aceasta este o cerere 
de conectare de la un client TCP. Cererea va fi acceptata initial si ID-ul 
clientului va fi primit mai apoi pentru a se verifica faptul ca un client cu 
acelasi ID nu este deja conectat. In cazul in care exista un client conectat in 
baza de date a server-ului cu acelasi ID, clientului nou conectat ii este 
trimis un mesaj de exit si conexiunea sa este inchisa. In cazul in care exista 
un client cu acelasi ID in baza de date dar acesta nu este conectat se va 
realiza procesul de reconectare. Daca ID-ul nu exista deja in baza de date se 
va creea un nou client cu acest ID si va fi adaugat in baza de date.
Pe socket-urile clientilor se pot primi doar mesaje de subscribe sau 
unsubscribe. Aceste mesaje sunt procesate fara verificari suplimentare deoarece 
acelea sunt facute client-side pentru a mai reduce din traficul pe retea. 
Procesul de subscribe/unsubscribe se face simplu, prin adaugarea/eliminarea 
topic-ului din dictionarul intern al clientului si prin adaugarea/eliminarea 
client-ului din setul de clienti subscribed corespunzatori topic-ului.
In cazul in care un client isi inchide conexiunea, starea sa din baza de date 
este modificata corespunzator iar socket-ul sau este eliminat din lista de 
socket-uri pe care se face ascultarea pentru trafic.
La finalul executiei se va dezaloca tot spatiul alocat stocarii clientilor in 
memorie. Memoria alocata stocarii packetelor in cozi este eliberata in momentul 
in care se face golirea cozii la reconectarea clientului sau la finalul 
executiei serverului pentru acei clienti care nu s-au mai conectat.

Subscriber

Subscriber-ul se va folosi de un singur socket TCP pentru a se conecta la 
server. De asemenea subscriber-ul va folosi un dictionar de topic-uri, in care 
vor fi stocate perechi intre topic-urile la care clientul este abonat si 
valoarea SF pentru fiecare, pentru a realiza verificari client-side pentru 
comenzile de subscribe sau unsubscribe. Imediat dupa conectarea la server, 
clientul va trimite un mesaj ce va contine ID-ul sau. Ciclul de rulare al 
clientului incepe prin selectarea file descriptor-ilor de pe care s-a primit 
informatie.
In cazul in care s-au primit informatii de la server, acestea vor fi un 
mesaj standard de exit sau de incoming packet. In cazul in care mesajul este 
unul de exit se va realiza secventa de exit si clientul se va opri. In cazul in 
care mesajul este unul de incoming packet, se vor folosi dimensiunile transmise 
prin intermediul payload-ului mesajului pentru a primii in continuare si 
packet-ul in sine. Acesta va fi reconstruit din buffer intr-o structura mai 
usor utilizabila si va fi afisat in urma unei conversii aplicate asupra 
continutului in functie de tipul de date transmis.
Clientul poate primi, de asemenea, informatii de la STDIN. Acestea vor fi cele 
3 comenzi, exit, subscribe sau unsubscribe. Se va realiza si o verificare 
asupra comenzii pentru a verifica ca aceasta este una valida. Atat in cazul 
comenzii de subscribe cat si in cazul celei de unsubscribe se va verifica daca 
topic-ul introdus respecta limita de lungime. In cazul comenzii de subscribe se 
va verifica si daca valoarea introdusa pentru SF este doar 0 sau 1. Atat pentru 
comanda de subscribe cat si pentru comanda de unsubscribe, orice alt input 
introdus in plus fata de cel necesar va fi eliminat pentru a nu afecta comenzi 
viitoare. Se va putea da subscribe pe un topic pe care deja s-a dat subscribe, 
doar in cazul in care valoarea pentru SF difera de cea introdusa precedent, caz 
in care serverul doar va modifica valoarea SF-ului. Se va putea da unsubscribe 
de la un topic doar daca s-a dat deja subscribe la acesta. In urma unei 
operatii de subscribe sau unsubscribe se va astepta un mesaj de confirmare de 
la server inainte de a afisa confirmarea utilizatorului.
La finalul rularii clientul isi va inchide conexiunea catre server.
