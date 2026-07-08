# ESP32_Attack_Platform

Target hardware: ESP32-WROOM-32 DevKit V1.

## Release status

- Dashboard, scanner WiFi, REST API.
- STA Manager.
- Auto refresh.
- Packet sniffer con capture promiscuous, metriche live e controllo da UI/API.
- Download dei pacchetti catturati in formato `.pcap`.
- Access Point aperto `ESP32 - Management` per accesso locale alla dashboard.
- Connettivita' Internet per `freeWiFi` tramite uplink STA e NAT IPv4.
- Packet sniffing del traffico in transito sull'AP `freeWiFi`.

## Future plan

- Passive/active handshake sniffing
- WPA/WPA2 handshake capture and parsing
- Deauthentication attacks using various methods

## AP freeWiFi

La board avvia un AP aperto chiamato `freeWiFi`.

- SSID: `freeWiFi`
- Password: nessuna
- IP dashboard: `192.168.4.1`

La dashboard mostra SSID, IP e numero di client collegati all'AP.

## NAT routing

Quando la STA viene connessa a una rete WiFi esterna e ottiene un indirizzo IP,
il firmware abilita NAPT sul SoftAP `freeWiFi`. I client collegati a `freeWiFi`
ricevono indirizzi nella rete `192.168.4.0/24` e usano la board come gateway.

Il DNS dell'uplink STA viene propagato al DHCP server del SoftAP. Se la STA si
disconnette, il NAT viene disabilitato e `freeWiFi` torna a essere una rete
locale per dashboard/API.

## Packet sniffing AP

Lo sniffer ha una modalita' dedicata `freeWiFi`: usa il canale WiFi corrente e
salva nel PCAP solo i frame dati 802.11 che hanno come BSSID il SoftAP della
board. I contatori separano traffico uplink, dai client verso l'AP, e downlink,
dall'AP verso i client.

La modalita' generica `All` resta disponibile per catturare management, control
e data frame sul canale selezionato manualmente.

## API principali

- `GET /api/status`: stato generale, scanner, capture, STA e AP `freeWiFi`.
- `GET /api/system`: versione, SSID/IP AP e numero client collegati.
- `GET /api/scan`: avvia una scansione WiFi.
- `GET /api/results`: restituisce i risultati della scansione.
- `GET /api/station`: stato della connessione STA.
- `POST /api/connect`: connette la STA a una rete WiFi.
- `POST /api/disconnect`: disconnette la STA.
- `GET /api/capture`: stato dello sniffer e contatori live, inclusi quelli AP.
- `POST /api/capture/start`: avvia lo sniffer. Body supportato: `{"mode":"ap"}` per `freeWiFi`, oppure `{"mode":"all","channel":6}`.
- `POST /api/capture/stop`: ferma lo sniffer.
- `GET /api/capture/download`: scarica la capture corrente come `wifi_capture.pcap`.

## Prerequisiti

- <a href='https://dl.espressif.com/dl/esp-idf/'>ESP-IDF v5.5.4</a>
- Python v3.11.2

## Build

La build ESP-IDF e' stata verificata con target `esp32` e genera:

- `build/wifi_analyzer.bin`
- `build/storage.bin`

Istruzioni per la build:

- 'idf.py build'
- 'idf.py flash'
- 'idf.py monitor'