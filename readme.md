Extremely simple example of client-server application, which perform evaluation of math expression.

### Workflow ?
1. TCP-client send request in json format 
2. TLS-client take those request and send it to TLS-server via encrypted channel.
3. TLS-server decrypt request and send it to TCP-server.
4. TCP-server parse the request, evaluate expression via RPN and send response.
5. all operation vise-versa

### Requirements
- OpenSSL/LibreSSL dev
- jansson

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
3. assymetric brackets  
4. not enough parameters  
