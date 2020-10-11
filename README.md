# CoreMess usage

Per utilizzare il modulo CoreMess prima di tutto è necessario:
1. compilare i codici sorgente presenti nella cartella ```CoreMess/src/service``` utilizzando il Makefile;
2. montare il modulo aprendo un terminale nella cartella dei file appena compilati e digitando come superuser
    
    ```$ sudo insmod module_messy.ko```
    
Per scoprire il valore del ``` <major_number>``` associato al modulo digitare in un terminale ```dmesg```.
#### creazione di un nuovo file
Per creare un nuovo file è necessario digitare in un terminale come superuser:

```$ sudo mknod <file_name> c <major_number> <minor_number>```

dove:
- ```<file_name>``` è il path e il nome del file che si desidera creare;
- ``` <major_number>``` è il valore del major associato al modulo, restituito dopo l'inserimento del modulo;
- ``` <minor_number>``` è il valore del minor che si desidera utilizzare.

#### CoreMess users
All'interno della cartella ```CoreMess/src/users``` sono presenti due file che descrivono i comportamenti possibili per l'interazione con il device file.

**Reader**

Per avviare un reader è necessario:
1. aver creato precedentemente il file che si desidera utilizzare;
2. compilare il codice sorgente (es: ```gcc ./reader.c -o reader.o```)
3. lanciare il processo digitando in un terminale come superuser:
    ```$ sudo ./reader.o <file_name> <read_timer_millis>```
dove:
- ```<file_name>``` è il path e il nome del file che si desidera utilizzare;
- ``` <read_timer_millis>``` è il valore del read_timer in millisecondi da associare alla sessione. 
Se il valore inserito è:
   - maggiore di zero verranno effettuare delle **deferred read**;
   - uguale a zero verranno effettuare delle **non-blocking read**;

**Writer**

Per avviare un writer è necessario:
1. aver creato precedentemente il file che si desidera utilizzare;
2. compilare il codice sorgente (es: ```gcc ./writer.c -o writer.o```)
3. lanciare il processo digitando in un terminale come superuser:
    ```$ sudo ./writer.o <file_name> ``` 
    
dove: ```<file_name>``` è il path e il nome del file che si desidera utilizzare.

Durante l'esecuzione è possibile scegliere se degitare un messaggio oppure invocare una ioctl specificando la MACRO associata, come:
  1. SEND_TIMEOUT, per specificare il write_timer (espresso in millisecondi) e invocare delle **deferred write**. Di Default verranno effettuate delle **non-blocking write**;
  2. REVOKE, per eliminare tutti i messaggi del file aperto non materializzati;
  3. DELETE, per eliminare tutti i messaggi salvati e non ancora letti all'interno del file;
  4. FLUSH, per eliminare tutti i messaggi del file aperto non materializzati e sbloccare tutti i readers ancora in attesa;
  5. QUIT, per terminare il processo corrente.
  
#### CoreMess test
Per testare i possibili flussi di esecuzione è possibile eseguire i test di esempio presenti nella cartella ```CoreMess/src/users/test```:
1.```1_write_read_test.c```, esegue una non-blocking write e una non-blocking read;

2.```2_defwrite_read_test.c```, esegue una deferred write e una non-blocking read;

3.```3_defread_write_test.c```, esegue una non-blocking write e una deferred read;

4.```4_rekove_messages_test.c```, elimina i messaggi non materializzati nel file invocando l'ioctl con MACRO REVOKE_DELAYED_MESSAGES;

5.```5_flushing_defworks_test.c```, elimina tutti i deferred work (messaggi non ancora materializzati nel file e i deferred reader ancora in attesa) invocando l'ioctl con MACRO FLUSH_DEF_WORK;

6.```6_multi_readers_writers_test.c```, lancia un numero configurabile di sessioni parallele di deferred readers e deferred writers. Tutti i parametri configurabili sono definiti nel file ```test_conf.h```

Per eseguirli, è necessario compilare i file sorgente presenti nella cartella (vedere il commento all'interno dei file .c) e avviare i test digitando in un terminale come superuser:
```$ sudo ./run_test_{test_number}.o <major_number>```

dove ```{test_number}``` corrisponde all'indice del test presente nel nome del file sorgente e ```<major_number>``` il valore del major associato al modulo in fase di montaggio.
Il ``` <minor_number>``` dei test di default è 0. 
