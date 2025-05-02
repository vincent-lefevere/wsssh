# wsssh
Accès à distance à des commandes shell via une connexion websocket (remote secure shell over websocket).

## Compilation

Ce programme, écrit en langage C, utilise la bibliothèque libwebsockets (voir [ici](https://libwebsockets.org/)).
Avant de procéder à la compilation, il convient donc de l'installer sur son environnement.

### via une distribution Debian

Le script suivant (nommé "install.sh" et lancé en root) peut être utilisé pour installer les dépendances, compiler wsssh et le placer dans le répertoire **/usr/local/sbin**.

```bash
#!/bin/bash
apt-get update
apt-get install -y --no-install-recommends build-essential ca-certificates git pkg-config
apt-get install -y --no-install-recommends libxml2 libxml2-dev libwebsockets-dev libmariadb-dev
git clone https://github.com/vincent-lefevere/wsssh
(cd wsssh/src ; make install) 
```

### via un docker de compilation

Alors que docker est installé sur notre machine, on place, dans un répertoire de travail, le fichier "install.sh" et on y ajoute également le fichier Dockerfile dont le contenu est le suivant :

```dockerfile
FROM debian:12.10
COPY --chmod=755 install.sh /tmp/install.sh
RUN /tmp/install.sh
```

On lance ensuite les 5 commandes ci-dessous :

```bash
docker build -t make_wsssh:current .
docker container create --name tmp_wsssh make_wsssh:current
docker cp -q tmp_wsssh:/usr/local/sbin/wsssh - | tar -xf -
docker container rm tmp_wsssh
docker rmi make_wsssh:current
```

On récupère alors le fichier "wsssh" dans le répertoire à côté des fichiers "install.sh" et "Dockerfile".

## Utilisation

Ce programme nécessite un fichier de configuration écrit en XML semblable à l'exemple présent dans le répertoire [conf](../../tree/main/conf).

### Edition du fichier de configuration

Comme indiquer dans la DTD du fichier XML incluse dans le fichier exemple, la balise racine **\<conf\>** contient 7 attributs :
- port

    On indique un numéro de port du serveur web utilisé par l'application.
	
- uri

	On indique le début de l'URL de traitement des requêtes http traitées via le websocket.

- log

	On indique le chemin du fichier servant de buffer de stockage du résultat de la commande exécutée afin de pouvoir l'envoyé aux autres clients qui se connectent ou la réenvoyer.

- cert

	On indique le chemin du fichier contenant le certificat pour faire une connexion "wss:".
	Si le paramètre est vide, on utilisera une connexion "ws:" à la place d'une connexion "wss:".

- key

	On indique le chemin du fichier contenant la clef privé pour faire la connexion "wss:".

- max

	On indique le nombre de client ayant la possiblité de ce connecter simultannément au serveur pour recevoir une copie du résultat de la commande exécutée.

- format

	On indique l'une des deux valeurs "bin" ou "txt" selon que l'on souhaite utiliser le websocket pour envoyer les résultats des commandes exécutées comme un flux binaire ou un flux texte.

Dans la balise racine, on peut éventuellement gérer l'authentification des clients souhaitant se connecter en y plaçant une première balise **\<auth\>**. Cette balise contiendra l'attribut suivant:

- cookie

    Cet attribut indique le nom du cookie qui servira à transmettre le token d'authetification.

Selon que l'on inclut dans la balise **\<auth\>** la balise **\<file\>** ou bien la balise **\<mysql\>** on active l'une ou l'autre des méthodes de vérification du token (voir les 2 méthodes si on inclut les 2 balises quelque soit l'ordre).

-   La balise **\<file\>** doit contenir, comme chaîne de caractères, le chemin d'accès au fichier contenant les tokens authorisant l'accès au serveur et donc l'exécution de commandes.
-   La balise **\<mysql\>** doit contenir, comme chaîne de caractères, la requête SQL servant à vérifier si la valeur, du token reçu, figure dans la table des autorisations d'accès.

	Dans la requête SQL inscrite, "%s" sera remplacé par la valeur du token reçu.
	
	Les attributs de la balise **\<mysql\>** sont utilisés pour définir les paramètres d'accès au serveur de base de données MySQL. C'est à dire :

    -   host
    
        Cet attribut indique le nom (ou l'IP) du serveur MySQL.

    -   user
    
        Cet attribut indique le login à utiliser sur le serveur MySQL.

    -   pwd
    
        Cet attribut indique le mot de passe associé à ce login.
            
    -   db
    
        Cet attribut indique le nom de la base de données sur laquelle les requêtes doivent être effectuées.

Enfin dans la racine, on place plusieurs (une par commande que l'on prévoit) balises **\<cmd\>**. Chaque balise doit contenir, comme chaîne de caractères, la commande shell qui sera exécutée.
On utilise les attributs, de la balise **\<cmd\>**, pour spécifier différentes informations complémentaires :

-   name

    le nom qu'il faudra indiquer dans l'URL pour exécuter la commande.

-   uid

    L'uid de l'utilisateur Unix (0 pour root) qui exécutera la commande.

-   gid

    Le gid du groupe Unix utilisé pour exécuter la commande.

### Lancement du programme

Lors du lancement du programme, on doit obligatoirement préciser, en argument, le chemin d'accès du fichier de configuration XML.

```bash
/usr/local/sbin/wsssh /etc/wsssh/conf.xml
```
