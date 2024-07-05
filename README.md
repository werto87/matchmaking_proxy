# matchmaking_proxy
- create certificates and add them to your certificate store 'mkcert -install ; mkcert localhost'
- create dhparams openssl dhparam -out dhparams.pem 2048
- specify the path to the file plus the name when calling the executable (DEFAULT_PATH_TO_CHAIN_FILE
  DEFAULT_PATH_TO_PRIVATE_FILE DEFAULT_PATH_TO_DH_File) **or** rename "localhost-key.pem" to "privkey.pem" and "
  localhost.pem" to "fullchain.pem" and put them together with
  dhparams.pem in /etc/letsencrypt/live/test-name/
