# CppHTTPServer
A Simple HTTP Server made with C++ that supports the standard ```get`` and ```put``` HTTP protocol commands as well as others. There are two programs in the repo cxi and cxid. cxi is the client while cxid is the server program.


## Getting Started

### Prerequisites

You will need C++11

```
git clone https://github.com/WillHord/CppHTTPServer.git
```

### Installing

Once downloaded run ```make``` to compile the program. Then start a server by using the command:

```./cxid [port]```

Then you should be able to connect to the server using the command:

```./cxi [server ip] [port]```

The cxi program supports the following commands:

```
exit         - Exit the program.  Equivalent to EOF.   
get filename - Copy remote file to local host.
help         - Print help summary.
ls           - List names of files on remote server.
put filename - Copy local file to remote host.
rm filename  - Remove file from remote server.
```


## Built With

* [C++](https://cplusplus.com/) - Application Language

## Authors

* **[Will Hord](https://github.com/WillHord)** 

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
