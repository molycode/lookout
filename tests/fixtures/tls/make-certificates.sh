#!/bin/bash
# Writes the certificates the HTTPS tests use: a test CA, a localhost certificate it signed, and a second CA that
# signed nothing, for the tests that must not trust the server. Valid for a century, so they never expire under CI.
set -eu
cd "$(dirname "${BASH_SOURCE[0]}")"
readonly DAYS=36500

openssl ecparam -name prime256v1 -genkey -noout -out ca-key.pem
openssl req -x509 -new -key ca-key.pem -sha256 -days "$DAYS" -subj "/CN=Lookout Test CA" -out ca.pem

openssl ecparam -name prime256v1 -genkey -noout -out server-key.pem
openssl req -new -key server-key.pem -subj "/CN=localhost" -out server.csr
printf 'subjectAltName=DNS:localhost\nbasicConstraints=CA:FALSE\nkeyUsage=digitalSignature\nextendedKeyUsage=serverAuth\n' > server.ext
openssl x509 -req -in server.csr -CA ca.pem -CAkey ca-key.pem -CAcreateserial -sha256 -days "$DAYS" -extfile server.ext -out server.pem

openssl ecparam -name prime256v1 -genkey -noout -out other-ca-key.pem
openssl req -x509 -new -key other-ca-key.pem -sha256 -days "$DAYS" -subj "/CN=Lookout Other CA" -out other-ca.pem

rm -f server.csr server.ext ca.srl ca-key.pem other-ca-key.pem
