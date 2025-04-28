#include <libwebsockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <pty.h>
#include <mariadb/mysql.h>

#define USLEEP_TIME 25000
#define BUFFER_SIZE 1024

static char *cmd_log_filename;

static struct cmd_conf {
    char *uri;
    char *cmd;
    char *login;
    char *sql;
} *action_tab;

static char *start_uri, *key, *cert;
static char *token=NULL, *token_file=NULL, *token_sql=NULL;
static char *mysqlhost=NULL, *mysqluser=NULL, *mysqlpwd=NULL, *mysqldb=NULL;
static MYSQL *conn;

static int server_port, max_clients, write_format=LWS_WRITE_BINARY;

static void read_conf(int argc, char ** argv) {
    xmlDocPtr doc;
    xmlNodePtr root, cmdNode, authNode;
    xmlChar *xmlValue;
    int nb, i;

    if (argc!=2) exit(1);
    doc = xmlReadFile(argv[1], NULL, 0);
    if (doc==NULL) exit(2);
    root = xmlDocGetRootElement(doc);
    for (nb=1, cmdNode = root->children; cmdNode != NULL; cmdNode = cmdNode->next)
        if (cmdNode->type == XML_ELEMENT_NODE && xmlStrcmp(cmdNode->name, (const xmlChar*)"cmd") == 0)
                nb++;
    xmlValue = xmlGetProp(root, (const xmlChar*)"port");
    if (xmlValue!=NULL) {
        server_port=atoi((const char*)xmlValue);
        xmlFree(xmlValue);
    } else exit(1);
    xmlValue = xmlGetProp(root, (const xmlChar*)"uri");
    if (xmlValue!=NULL) {
        start_uri=strdup((const char*)xmlValue);
        xmlFree(xmlValue);
    } else exit(1);
    xmlValue = xmlGetProp(root, (const xmlChar*)"log");
    if (xmlValue!=NULL) {
        cmd_log_filename=strdup((const char*)xmlValue);
        xmlFree(xmlValue);
    } else exit(1);
    xmlValue = xmlGetProp(root, (const xmlChar*)"max");
    if (xmlValue!=NULL) {
        max_clients=atoi((const char*)xmlValue);
        xmlFree(xmlValue);
    } else exit(1);
    xmlValue = xmlGetProp(root, (const xmlChar*)"format");
    if (xmlValue!=NULL) {
        if (strcmp("bin",(const char*)xmlValue)==0) write_format=LWS_WRITE_BINARY ;
        else write_format=LWS_WRITE_TEXT;
        xmlFree(xmlValue);
    } else exit(1);
    cert=NULL;
    xmlValue = xmlGetProp(root, (const xmlChar*)"cert");
    if (xmlValue!=NULL) { 
        if (((const char*)xmlValue)[0]!='\0') cert=strdup((const char*)xmlValue);
        xmlFree(xmlValue);
    }
    key=NULL;
    xmlValue = xmlGetProp(root, (const xmlChar*)"key");
    if (xmlValue!=NULL) {
        if (((const char*)xmlValue)[0]!='\0') key=strdup((const char*)xmlValue);
        xmlFree(xmlValue);
    }
    action_tab = (struct cmd_conf *) malloc(sizeof(struct cmd_conf)*nb);
    for (i=0, cmdNode = root->children; cmdNode != NULL; cmdNode = cmdNode->next) {
        if (cmdNode->type == XML_ELEMENT_NODE && xmlStrcmp(cmdNode->name, (const xmlChar*)"mysql") == 0) {
            xmlValue = xmlGetProp(cmdNode, (const xmlChar*)"host");
            if (xmlValue!=NULL) { 
                mysqlhost=strdup((const char*)xmlValue);
                xmlFree(xmlValue);
            } else exit(1);
            xmlValue = xmlGetProp(cmdNode, (const xmlChar*)"user");
            if (xmlValue!=NULL) { 
                mysqluser=strdup((const char*)xmlValue);
                xmlFree(xmlValue);
            } else exit(1);
            xmlValue = xmlGetProp(cmdNode, (const xmlChar*)"pwd");
            if (xmlValue!=NULL) { 
               mysqlpwd=strdup((const char*)xmlValue);
                xmlFree(xmlValue);
            } else exit(1);
            xmlValue = xmlGetProp(cmdNode, (const xmlChar*)"db");
            if (xmlValue!=NULL) { 
                mysqldb=strdup((const char*)xmlValue);
                xmlFree(xmlValue);
            } else exit(1);
        }        
        if (cmdNode->type == XML_ELEMENT_NODE && xmlStrcmp(cmdNode->name, (const xmlChar*)"auth") == 0) {
            xmlValue = xmlGetProp(cmdNode, (const xmlChar*)"cookie");
            if (xmlValue!=NULL) { 
                token=strdup((const char*)xmlValue);
                xmlFree(xmlValue);
            } else exit(1);
            for (authNode = cmdNode->children; authNode != NULL; authNode = authNode->next) {
                if (authNode->type == XML_ELEMENT_NODE && xmlStrcmp(authNode->name, (const xmlChar*)"file") == 0) {
                    xmlValue = xmlNodeGetContent(authNode);
                    if (xmlValue!=NULL) { 
                        token_file=strdup((const char*)xmlValue);
                        xmlFree(xmlValue);
                    } else exit(1);
                }
                if (authNode->type == XML_ELEMENT_NODE && xmlStrcmp(authNode->name, (const xmlChar*)"sql") == 0) {
                    xmlValue = xmlNodeGetContent(authNode);
                    if (xmlValue!=NULL) { 
                        token_sql=strdup((const char*)xmlValue);
                        xmlFree(xmlValue);
                    } else exit(1);
                }
            }
        }        
        if (cmdNode->type == XML_ELEMENT_NODE && xmlStrcmp(cmdNode->name, (const xmlChar*)"cmd") == 0) {
            xmlValue = xmlGetProp(cmdNode, (const xmlChar*)"name");
            if (xmlValue!=NULL) { 
                action_tab[i].uri=strdup((const char*)xmlValue);
                xmlFree(xmlValue);
            } else exit(1);
            xmlValue = xmlGetProp(cmdNode, (const xmlChar*)"login");
            if (xmlValue!=NULL) {
                action_tab[i].login=strdup((const char*)xmlValue);
                xmlFree(xmlValue);
            } else exit(1);
            xmlValue = xmlGetProp(cmdNode, (const xmlChar*)"sql");
            if (xmlValue!=NULL) {
                action_tab[i].sql=strdup((const char*)xmlValue);
                xmlFree(xmlValue);
            } else action_tab[i].sql=NULL;
            xmlValue = xmlNodeGetContent(cmdNode);
            if (xmlValue!=NULL) { 
                action_tab[i].cmd=strdup((const char*)xmlValue);
                xmlFree(xmlValue);
            } else exit(1);
            i++;
        }
    }
    action_tab[i].uri=NULL;
    action_tab[i].cmd=NULL;
    xmlFreeDoc(doc);
    xmlCleanupParser();
}

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static struct lws **clients;
static struct per_client_data {
    int length;
    char preambule_of_buffer[LWS_PRE];
    char buffer[BUFFER_SIZE];
    int fd_cmd_log;
    char preambule_of_buf_cmd_log[LWS_PRE];
    char buf_cmd_log[BUFFER_SIZE];
} *clients_data;

static int fd_cmd_socket, fd_cmd_log;
static int action_index;
static int pid;

static void sigchld_handler(int signo) {
    signal(SIGCHLD, sigchld_handler);
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

static void action(){
    MYSQL_RES *result;
    MYSQL_ROW row;
    int fd_cmd_in, fd_cmd_out;
    unsigned int num_fields,i;
    char field[8];

    if (action_tab[action_index].sql!=NULL) {
        if (mysqlhost==NULL) return; 
        if (mysql_query(conn, action_tab[action_index].sql)) {
            fprintf(stderr, "SELECT query failed: %s\n", mysql_error(conn));
            exit(1);
        }
        result = mysql_store_result(conn);
        if (result == NULL) exit(1);
        num_fields = mysql_num_fields(result);
        row = mysql_fetch_row(result);
        if (row == NULL) exit(1);
        if (num_fields>8) num_fields=8;
        for (i = 0; i < num_fields; i++) {
            sprintf(field,"field%d",i+1);
            setenv(field,(row[i]!=NULL)?row[i]:"",1);
            lwsl_notice("==> field%d=%s\n",i+1,row[i]);
        }
    }

    lwsl_notice("Executing command (%s)\n",action_tab[action_index].cmd);
    if (openpty(&fd_cmd_in, &fd_cmd_out, NULL, NULL, NULL) == -1) {
        perror("Openpty failed!\n");
        exit(1);
    }
    pid=fork();
    if (pid<0) {
        close(fd_cmd_in);
        close(fd_cmd_out);
        exit(1);
    } else if (pid==0) {
        close(fd_cmd_in);
        dup2(fd_cmd_out, 0);
        dup2(fd_cmd_out, 1);
        dup2(fd_cmd_out, 2);
        close(fd_cmd_out);
        execl("/usr/bin/su", "su","-c", action_tab[action_index].cmd, action_tab[action_index].login , NULL);
        exit(1);
    } else {
        close(fd_cmd_out);
        fd_cmd_socket=fd_cmd_in;
        unlink(cmd_log_filename);
        fd_cmd_log=open(cmd_log_filename,O_CREAT|O_WRONLY, 0644);
    } 
}

static char *get_header(struct lws *wsi, enum lws_token_indexes token) {
    int len=1+lws_hdr_total_length(wsi,token);
    char *header=malloc(len);
    if (header==NULL) {
        perror("Error malloc in get_header() !\n");
        exit(1);
    } 
    lws_hdr_copy(wsi,header,len,token);
    return(header);
}

static char *findcookie(char *cookie, char *token) {
    char *p=cookie;
    int l = strlen(token);
    while(*p!=0x00) {
        if ((p[l]=='=') && (memcmp(token,p,l)==0)) {
            cookie=p+l+1;
            while ((*p!=0x00)&&(*p!=';')) p++;
            *p=0x00;
            return(cookie); 
        } else {
            while ((*p!=0x00)&&(*p!=';')) p++;
            if (*p==';') p++;
            while (*p==' ') p++;            
        }
    }
    return NULL;
}

static int check_file(char *p) {
    FILE *f;
    int rc=1,l;
    char *buffer;

    l=strlen(p)+3;
    buffer=(char *) malloc(l);
    f=fopen(token_file,"r");
    if (f!=NULL) {
        while (!feof(f)) {
            fgets(buffer,l,f);
            if (strcmp(buffer,p)==0) {
                rc=0;
                break;
            }
        }
        fclose(f);
    }
    return(rc);
}

static int check_mysql(char *p) {
    char * req;
    MYSQL_RES *result;
    MYSQL_ROW row;

    lwsl_notice("starting check_mysql() !\n");
    req=(char *) malloc(strlen(p)+strlen(token_sql));
    sprintf(req,token_sql,p);
    if (mysql_query(conn, req)) {
        fprintf(stderr, "SELECT query failed: %s\n", mysql_error(conn));
        return (1);
    }
    result = mysql_store_result(conn);
    if (result == NULL) {
        fprintf(stderr, "mysql_store_result() failed\n");
        return (1);
    }
    row = mysql_fetch_row(result);
    if (row == NULL) return (1);
    return (0);
}

int callback_echo(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    char *uri, *cookie, *p;
    int rc = 0;

    switch (reason) {
        case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
            if (token!=NULL) {
                rc=1;
                cookie = get_header(wsi,WSI_TOKEN_HTTP_COOKIE);
                p = findcookie(cookie,token);
                if (token_file!=NULL) rc=check_file(p);
                if (rc!=0 && token_sql!=NULL) rc=check_mysql(p);
                free(cookie);
                if (rc==1) break;
            }
            uri = get_header(wsi,WSI_TOKEN_GET_URI);
            rc = memcmp(uri,start_uri,strlen(start_uri)-1);
            if (action_index!=-1)
                rc = strcmp(action_tab[action_index].uri,uri+strlen(start_uri)) &&
                    uri[strlen(start_uri)]!=0x00;
            free(uri);
            break;

        case LWS_CALLBACK_ESTABLISHED:
            rc=1;
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < max_clients; i++) if (clients[i] == NULL) {
                clients[i] = wsi;
                clients_data[i].length=0;
                uri = get_header(wsi,WSI_TOKEN_GET_URI);
                int new_action=0;
                for (int j=0; action_tab[j].uri!=NULL; j++)
                    if (strcmp(action_tab[j].uri,uri+strlen(start_uri))==0) {
                        if (action_index==-1) {
                            action_index=j;
                            new_action=1;
                        }
                        break;
                    }
                free(uri);
                if (new_action==0) clients_data[i].fd_cmd_log=open(cmd_log_filename,O_RDONLY);
                lws_callback_on_writable(wsi);
                rc = 0;
                break;
            }
            pthread_mutex_unlock(&mutex);
            break;

        case LWS_CALLBACK_RECEIVE:
            pthread_mutex_lock(&mutex);
            if (fd_cmd_socket!=-1 && len>0) {
                if (memchr((unsigned char *)in,0x03,len)!=NULL) kill(-1,SIGINT);
                rc=(write(fd_cmd_socket, (unsigned char *)in, len)<=0);
                for (int i = 0; i < max_clients; i++)
                    if (clients[i] != NULL)
                        lws_callback_on_writable(clients[i]);
            }
            pthread_mutex_unlock(&mutex);
            break;

        case LWS_CALLBACK_CLOSED:
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < max_clients; i++) if (clients[i] == wsi) {
                clients[i]=NULL;
                clients_data[i].length=0;
                if (clients_data[i].fd_cmd_log>=0) {
                    close(clients_data[i].fd_cmd_log);
                    clients_data[i].fd_cmd_log=-1;
                }
                break;
            }
            pthread_mutex_unlock(&mutex);
            break;

        case LWS_CALLBACK_SERVER_WRITEABLE:
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < max_clients; i++) if (clients[i] == wsi){
                if (clients_data[i].fd_cmd_log<0) {
                    if (clients_data[i].length>0) 
                        lws_write(wsi, clients_data[i].buffer, clients_data[i].length, write_format);
                    else if (clients_data[i].length<0)
                        rc=1;
                    clients_data[i].length=0;
                    lws_callback_on_writable(wsi);
                } else {
                    int l=read(clients_data[i].fd_cmd_log, &(clients_data[i].buf_cmd_log), BUFFER_SIZE);
                    if (l<=0) {
                        close(clients_data[i].fd_cmd_log);
                        clients_data[i].fd_cmd_log=-1;
                        if (action_index==-1) rc=1;
                    } else {
                        lws_write(wsi, clients_data[i].buf_cmd_log, l, write_format);
                    }
                    lws_callback_on_writable(wsi);
                }
            }
            pthread_mutex_unlock(&mutex);
            usleep(USLEEP_TIME);
            break;

        default:
            break;
    }
    return rc;
}

void scan_cmd_pipe() {
    char buf_cmd_pipe[BUFFER_SIZE];
    int len;
    int i;

    if (fd_cmd_socket==-1) { 
        usleep(USLEEP_TIME);
        return;
    }
    pthread_mutex_lock(&mutex);
    for (i = 0; i < max_clients; i++)
        if (clients_data[i].length!=0) {
            pthread_mutex_unlock(&mutex);
            usleep(USLEEP_TIME);
            return;
        }
    pthread_mutex_unlock(&mutex);
    len=read(fd_cmd_socket,&buf_cmd_pipe,BUFFER_SIZE);
    if (len<=0) {
        close(fd_cmd_log);
        close(fd_cmd_socket);
        fd_cmd_log=-1;
        fd_cmd_socket=-1;
        lwsl_notice("Command finished !\n");
        action_index=-1;
        pthread_mutex_lock(&mutex);
        for (i = 0; i < max_clients; i++) if (clients[i] != NULL) {
            clients_data[i].length=-1;
            lws_callback_on_writable(clients[i]);
        }
        pthread_mutex_unlock(&mutex);
    } else {
        write(fd_cmd_log,&buf_cmd_pipe,len);
        pthread_mutex_lock(&mutex);
        for (i = 0; i < max_clients; i++) if (clients[i] != NULL) {
            memcpy(&clients_data[i].buffer, &buf_cmd_pipe, len);
            clients_data[i].length=len;
            lws_callback_on_writable(clients[i]);
        }
        pthread_mutex_unlock(&mutex);
    }
}

static void *reader(void *arg) {
    unlink(cmd_log_filename);
    action_index=-1;
    fd_cmd_socket=-1;
    while(1) {
        if (action_index!=-1 && fd_cmd_socket==-1) action();
        scan_cmd_pipe();
    }
    return NULL;
}

static struct lws_protocols protocols[] = {
    { "default", callback_echo, 0, 0, },
    { NULL, NULL, 0, 0 } // Terminateur du tableau de protocoles
};

int main (int argc, char ** argv) {
    read_conf(argc, argv);

    if (mysqlhost!=NULL) {
       conn = mysql_init(NULL);
        if (conn == NULL) {
            perror("mysql_init() failed\n");
            return -1;
        }
        my_bool reconnect= 1; /* enable reconnect */
        mysql_optionsv(conn, MYSQL_OPT_RECONNECT, (void *)&reconnect);
        if (mysql_real_connect(conn, mysqlhost, mysqluser, mysqlpwd, mysqldb, 0, NULL, 0) == NULL) {
            perror("mysql_real_connect() failed\n");
            mysql_close(conn);
            return -1;
        }
    }
    clients=(struct lws **) malloc(max_clients*sizeof(struct lws *));
    clients_data=(struct per_client_data *) malloc(max_clients*sizeof(struct per_client_data));

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));

    info.port = server_port;
    info.iface = NULL;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.ssl_cert_filepath = cert;
    info.ssl_private_key_filepath = key;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    for (int i = 0; i < max_clients; i++) {
        clients[i]=NULL;
        clients_data[i].length=0;
    }

    struct lws_context *context = lws_create_context(&info);
    if (context == NULL) {
        perror("Erreur lors de la création du contexte du serveur WebSocket\n");
        return -1;
    }

    pthread_t thread_reader;
    if (pthread_create(&thread_reader, NULL, reader, NULL) != 0) {
        perror("Erreur lors de la création du thread de lecture\n");
        return -1; 
    }

    lwsl_notice("WebSocket Shell Server Started.\n");

    signal(SIGCHLD, sigchld_handler);
    signal(SIGPIPE, SIG_IGN);
    while (1) {
        lws_service(context, 1000);
    }
    lws_context_destroy(context);
    return 0;
}
