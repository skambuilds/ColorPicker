![Color Picker Logo](CPLogo/CPLogo.png)

## ColorPicker
IoT device project to extract the dominant color from a capture image

Linee guida per il riutilizzo

1. Predisposizione dei servizi Amazon:

Per predisporre il server FTP è necessario prima:
    a. Creare un bucket su s3
    b. Impostare un utente con il servizio IAM che permetta l'accesso al bucket di S3. È possibile anche creare una policy per l'accesso al bucket di S3 e a CloudWatch e quindi associarla all'utente creato.
Dall'utente si generano le chiavi d'accesso ai servizi amazon nella scheda delle credenziali, basate sulle policy impostate, che verranno utilizzate sull'istanza ec2 e sulla lambda.
Prima della creazione dell'istanza EC2 è necessario creare un gruppo di sicurezza dalla dashboard con le regole per aprire le porte per FTP e MQTT, solitamente vengono utilizzate 20-21 1024-1048 per FTP e 1883 e 8883 per MQTT.

A questo punto è possibile lanciare l'istanza EC2 ubuntu server.
La prima cosa da fare è installare il demone vsftpd, quindi si imposta il suo file di configurazione /etc/vsftpd.conf.
Nello specifico è necessario abilitare la modalità passiva del demone, in quanto il server si trova dietro un firewall:
pasv_enable=YES
pasv_min_port=1024
pasv_max_port=1048
pasv_address=ip.dell.istanza.ec2

È necessario abilitare l'utente locale in scrittura per poter scrivere i dati in entrata, inserendo il suo nome nel file:
/etc/vsftpd.chroot_list.
write_enable=YES
allow_writeable_chroot=YES
chroot_local_user=YES
chroot_list_enable=YES
chroot_list_file=/etc/vsftpd.chroot_list

Nei passi successivi si utilizza un utente di esempio “ftpuser”.
L'utente di esempio "ftpuser" viene creato con:
sudo adduser ftpuser
A cui segue l'inserimento della password quindi si può riavviare il server ftp con:
sudo systemctl restart vsftpd.service

A questo punto il server FTP è configurato per memorizzare i dati in arrivo nell'istanza EC2.

Montare bucket s3 in ec2.
Per montare il bucket su ec2 si è scelto di utilizzare s3fs, il quale è supportato da amazon.
Si installa normalmente con:
sudo apt-get update install s3fs
I comandi successivi devono essere eseguiti come utente ftpuser, quindi si esegue su ftpuser.
All'interno della home dell'utente si crea il file .passwd-s3fs, contenente le chiavi di accesso ai servizi amazon precedentemente generate nel servizio IAM.
echo KEY:SECRET:KEY > .passwd-s3fs
Ci si assicuri che sia in modalità rw solo per ftpuser, ovvero si esegue:
chmod 600 .passwd-s3fs
A questo punto è possibile montare il bucket utilizzando come mount point una cartella locale che deve essere vuota eseguendo il comando:
s3fs nomebucket /mountpoint -o passwd_file=/.passwd-s3fs

Questo permette a s3fs di montare il bucket con le credenziali di accesso di aws. 
Se il bucket è stato montato correttamente dovrebbe essere possibile vedere un file system s3fs nella lista generata dal comando: “df -h”
Affinchè sia possibile comunicare con il dispositivo via mqtt si installano i seguenti pacchetti:
sudo apt-get install mosquitto-clients mosquitto

2. Creazione della funzione lambda:

La funzione lambda utilizzata è uno script in python 3 che richiede l'uso delle librerie paho-mqtt e opencv-python.
Non è possibile importare tutte le librerie direttamente dall'ambiente della lambda in quanto è un ambiente serverless ristretto, ed è dunque necessario che queste vengano caricate esternamente.
Si procede quindi nel seguente modo:
Si crea una cartella sul proprio pc che si deve obbligatoriamente rinominare "python", quindi al suo interno si installano le librerie necessarie con:
python3 -m pip install paho-mqtt -t .
python3 -m pip install opencv-python -t .
Quindi si comprime la cartella "python" con un archivio zip.

Per creare la funzione lambda si procede dalla relativa pagina di servizio.
Nell'interfaccia di creazione si seleziona il runtime python >=3.6 e si seleziona il ruolo, che deve avere accesso sia a S3 che a CloudWatch affinchè gli eventi di S3 possano essere rilevati.
Dopodichè è possibile caricare le librerie precedentemente zippate dal menù layers (livelli), scegliendo il runtime python appropriato python >=3.6 e infine associarlo alla lambda corrente dalla schermata principale.
Successivamente è necessario impostare un trigger sul bucket di s3 che permetta di leggere gli eventi quindi su "aggiungi trigger" si sceglie il servizio S3 e il bucket, avendo cura di indicare ".jpg" come suffisso del file.
A questo punto si può concludere inserendo il codice della lambda disponibile in questo repository alla directory:
/blob/master/lambda_handler.py
Nel codice è necessario indicare quale sia l’indirizzo pubblico del broker mqtt.

3. Installazione dell’ambiente di sviluppo Arduino.
4. Download della libreria per l’utilizzo del nodeMCU Adafruit_ESP8266.
5. Download della libreria PubSubClient per l’utilizzo del servizio MQTT.
6. Collegamento dei componenti come precedentemente indicato.
7. Download del codice disponibile in questo repository.
    a. Installazione delle librerie presenti nella sotto-cartella libraries del progetto.
    b. Esecuzione dello script .ino disponibile nella sotto-cartella I2C_Scanner per l’individuazione dell’indirizzo del display.
    c. Aprire lo script disponibile nella sotto-cartella SNAP_SPIFFS_FTP e impostare tutti i parametri di connessione, wi-fi e FTP ed inserire infine l’indirizzo ricavato al punto precedente.
8. Collegare la scheda al computer e procedere al caricamento dello script.
9. Effettuare il caricamento dell’interfaccia web disponibile nella sottodirectory WebInterface, seguendo le prime due sezioni di questo tutorial:

https://aws.amazon.com/it/getting-started/projects/build-serverless-web-app-lambda-apigateway-s3-dynamodb-cognito/module-2/

10. Aggiornare il file ride.js nella sottocartella ​WebInterface/js ​con le informazioni riguardanti il bucket e il nome del file .json e procedere al suo caricamento sul bucket impostato al punto precedente.
11. Registrarsi tramite apposito form cliccando sul pulsante Sign Up nella homepage dell’interfaccia.
12. Effettuare degli scatti con il dispositivo.
13. Effettuare il login e visualizzare le foto scattate.
