$TTL	30s

@	IN	SOA	example.com.	tewok.leodhas.example.com. (
					0	; serial
					3h		; refresh
					30s		; retry
					1m		; expire
					30s )		; minimum

@		IN  	NS 	leodhas.example.com.

		IN	MX 10	leodhas.example.com.


mull			IN	A	100.9.82.21
iona			IN	A	100.9.82.22
leodhas			IN	A	100.9.82.23
harris			IN	A	100.9.82.24
barra			IN	A	100.9.82.25
skye			IN	A	100.9.82.26
uist			IN	A	100.9.82.27
staffa			IN	A	100.9.82.28
arran			IN	A	100.9.82.29
soarplane		IN	A	100.9.82.99


