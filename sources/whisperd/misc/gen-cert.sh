#!/bin/bash

#openssl req -newkey rsa:2048 -new -nodes -x509 -days 3650 -keyout key.pem -out cert.pem
cat key.pem > server.pem
cat cert.pem >> server.pem

#openssl rsa -in server.key -out nopassword.key
#cat nopassword.key > server.pem
#cat intermediate.crt >> server.pem
