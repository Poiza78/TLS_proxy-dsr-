An extremely simple example of a client-server application, which performs evaluation of math expression.

**Server applications are multi-client.**

### Workflow ?
1. TCP-client send request in json format 
2. TLS-client takes those request and send it to TLS-server via encrypted channel.
3. TLS-server decrypts request and send it to TCP-server.
4. TCP-server parses the request, evaluate expression via RPN and send the response.
5. all operation vise-versa

### Requirements
- OpenSSL/LibreSSL dev
- jansson lib

### Example

```console
$ make
$ bin/start 10101

You can use: add( *,+,() ), set, calculate, exit

>add 2+x*3
>set x = 8
>calculate
2+x*3 = 26
>exit
```

### Error codes:
1. parsing error  
2. wrong expression  
3. asymmetric brackets  
4. not enough parameters  
