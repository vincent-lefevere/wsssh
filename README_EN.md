# wsssh
Remote access to shell commands via a websocket connection.

## Compilation

This program, written in C, uses the libwebsockets library (see [here](https://libwebsockets.org/)).
Before compiling, you need to install it on your environment.

### via a Debian distribution

The following script (named ‘install.sh’ and run as root) can be used to install the dependencies, compile wsssh and place it in the **/usr/local/sbin** directory.

```bash
#!/bin/bash
apt-get update
apt-get install -y --no-install-recommends build-essential ca-certificates git pkg-config
apt-get install -y --no-install-recommends libxml2 libxml2-dev libwebsockets-dev libmariadb-dev
git clone https://github.com/vincent-lefevere/wsssh
(cd wsssh/src ; make install) 
```

### via a compilation docker

Once docker is installed on our machine, we place the "install.sh" file in a working directory and we also add the Dockerfile file, the contents of which are as follows:

```dockerfile
FROM debian:12.10
COPY --chmod=755 install.sh /tmp/install.sh
RUN /tmp/install.sh
```

Then run the 5 commands below:

```bash
docker build -t make_wsssh:current .
docker container create --name tmp_wsssh make_wsssh:current
docker cp -q tmp_wsssh:/usr/local/sbin/wsssh - | tar -xf -
docker container rm tmp_wsssh
docker rmi make_wsssh:current
```

The "wsssh" file is then retrieved from the directory next to the "install.sh" and "Dockerfile" files.

## Use

This program requires a configuration file written in XML similar to the example in the [conf](../../tree/main/conf)  directory.

### Editing the configuration file

As indicated in the XML file DTD included in the example file, the **<conf\>** root tag contains 7 attributes:
- port

    	A port number is given for the web server used by the application.

- uri

	We indicate the start of the URL for processing http requests processed via the websocket.

- log

	We indicate the path of the file used as a buffer to store the result of the command executed so that it can be sent to other clients who connect or resent.

- cert

	We indicate the path of the file containing the certificate to make a "wss:" connection.
	If the parameter is empty, a "ws:" connection will be used instead of a "wss:" connection.

- key

	We indicate the path of the file containing the private key to make the "wss:" connection.

- max

	The number of clients able to connect simultaneously to the server to receive a copy of the result of the command executed is indicated.

- format

	One of the two values "bin" or "txt" is specified, depending on whether the websocket is to be used to send the results of commands executed as a binary stream or a text stream.

In the root tag, you can manage the authentication of clients wishing to connect by placing an initial **<auth\>** tag. This tag will contain the following attribute:

- cookie

    This attribute indicates the name of the cookie used to transmit the authentication token.

Depending on whether you include the **\<auth\>** tag with the **\<file\>** tag or the **\<mysql\>** tag, you activate one or other of the token verification methods (or both methods if you include the 2 tags in any order).

-   The **<file\>** tag must contain, as a character string, the path to the file containing the tokens authorising access to the server and therefore the execution of commands.

-   The **\<mysql\>** tag must contain, as a character string, the SQL query used to check whether the value of the token received is in the access authorisation table.

	In the SQL query entered, "%s" will be replaced by the value of the token received.
	
	The attributes of the **<mysql\>** tag are used to define the access parameters to the MySQL database server. This means :

    -   host
    
        This attribute indicates the name (or IP) of the MySQL server.

    -   user
    
        This attribute indicates the login to be used on the MySQL server.

    -   pwd
    
        This attribute indicates the password associated with this login.
            
    -   db
    
        This attribute indicates the name of the database on which queries should be performed.

Finally, place several **<cmd\>** tags in the root (one for each order you plan to place). Each tag must contain, as a character string, the shell command that will be executed.
The attributes in the **<cmd\>** tag are used to specify various additional items of information:

-   name

    the name to be given in the URL to execute the command.

-   uid

    The uid of the Unix user (0 for root) who will execute the command.

-   gid

    The gid of the Unix group used to execute the command.

### Launch of the programme

When the program is launched, the path to the XML configuration file must be specified as an argument.

```bash
/usr/local/sbin/wsssh /etc/wsssh/conf.xml
```
