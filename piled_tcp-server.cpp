/*
Kleiner TCP-Server 
Diese Software, ursprüng 2015 geschrieben, stellt einen TCP-Server dar, welcher zur Ausführung auf embedded-systemen wie dem Raspberry-Pi gedacht ist.
-Die vorliegende Version wurde angepasst um LED's über die GPIO-Pins des Raspberry-Pi zu steuern. (D. Marx, 28.02.2018)

Dieser Server nutzt die c++_Threadbibliothek um Multithreading (pro client ein Thread) zu ermöglichen und die Bibliothek WiringPi (http://wiringpi.com/) zur Ansteuerung der GPIO-Pins des RaspberryPi.

Compile with:  
	old g++:
		g++ -std=c++0x ./piled_tcp-server.cpp -lpthread -lwiringPi -o dmarxtcp
	newer g++ 5.3.0:
		g++ -std=c++17 ./piled_tcp-server.cpp -lpthread -lwiringPi -o dmarxtcp

Autor: D. Marx
https://github.com/derco0n/PiLED-Server
*/

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>    //strlen
#include <signal.h> //STRG+C, STRG+Z, etc...
#include <string>
#include <cstring>
#include <fstream>
#include <sys/socket.h> 
#include <arpa/inet.h> //inet_addr
#include <unistd.h> //close() 
#include <sstream>
#include <pthread.h> //for threading , link with lpthread
#include <ctime>
#include <time.h>
#include <wiringPi.h> //using wiringpi to control raspberry-pi's gpio pins

//Konstanten und Variablen
int pinred=0;
int pingreen=0;
int pinblue=0;
const int maxclients=200; //Maximale gleichzeitige Clientverbindungen

bool shuttingdown=false; //Wenn true werden alle offenen Clientverbindungen getrennt
int socket_desc, new_socket, c, *new_sock;

int curclicnt=0; //Arrayindex der aktuellen Clientverbindungen
//int connections[maxclients]; //Array of sockets

const double myversion=0.25; //Diese Programmversion
int listenPort = 0; //Auf welchem Port soll gelauscht werden?
void *connection_handler(void *);
int connection_id;

char retchar;
char retstr[1024];
char serialAnswer[1024];
int y=0;

/*
struct hathrargs {
    int socket;
    int connid;
};
*/

void closeSocket(int fd) {      // *not* the Windows closesocket()
   if (fd >= 0) {
      //getSO_ERROR(fd); // first clear any errors, which can cause close to fail
      if (shutdown(fd, SHUT_RDWR) < 0) // secondly, terminate the 'reliable' delivery
         if (errno != ENOTCONN && errno != EINVAL) // SGI causes EINVAL
            fprintf(stderr,"shutdown");
      if (close(fd) < 0) // finally call close()
         fprintf(stderr,"close");
	  
   }
}

std::string itos(int n)
{
	std::stringstream ss;
	ss << n;
	std::string str = ss.str();
	return str;
}


bool charState (char c){
	//Wandelt den char in einen boolschen Wert
	//Wenn c '1' entspricht ist der Wert TRUE sonst FALSe
	if (isdigit(c)){
		int num=(int)c - 48;
		if (num==1){
		return true;
		}
	}
	return false;
}

std::string BoolToString(bool b){
	//Wandelt einen Bool in einen String
	std::string myreturn="true";
	if (b){
		return myreturn;
	}
	else {
		myreturn="false";
	}
	return myreturn;
}

std::string timeStr(void) //Erzeugt einen String mit dem aktuellen Zeitpunkt
{
	std::string myReturn="";
	struct tm *current;
	time_t now;
	time(&now);
	current = localtime(&now);
	std::string day, mon, year, hour, min, sec;
	if (current->tm_mday < 10)
	{//Falls nötig führende Null hinzufügen
		day = "0" + itos(current->tm_mday);
	}
	else
	{
		day = itos(current->tm_mday);
	}
	if (current->tm_mon+1 < 10)
	{//Falls nötig führende Null hinzufügen
		mon = "0" + itos(current->tm_mon+1);
	}
	else
	{
		mon = itos(current->tm_mon+1);
	}
	year = itos(1900 + current->tm_year); //Jahr
	if (current->tm_hour < 10)
	{//Falls nötig führende Null hinzufügen
		hour = "0" + itos(current->tm_hour);
	}
	else
	{
		hour = itos(current->tm_hour);
	}	
	if (current->tm_min < 10)
	{//Falls nötig führende Null hinzufügen
		min = "0" + itos(current->tm_min);
	}
	else
	{
		min = itos(current->tm_min);
	}
	if (current->tm_sec < 10)
	{//Falls nötig führende Null hinzufügen
		sec = "0" + itos(current->tm_sec);
	}
	else
	{
		sec = itos(current->tm_sec);
	}
	myReturn = day + "." + mon + "." + year + " " += hour + ":" + min + ":" + sec;
	return myReturn;
}

int writelog(/*const char*/ std::string logstring)
{
	std::ofstream log;
	log.open("/var/log/dmarxtcp.log", std::ofstream::out | std::ofstream::app);
	if (!log)
	{
		//cout << "Couldn't open file." << endl;
		system("touch /var/log/dmarxtcp.log");
		//return 1;
	}
	log << timeStr() << " -> " << logstring;
	log.close();
	return 0;
}

void printHelp(){
	fprintf(stdout,"\n");
	fprintf(stdout,"Usage:\n");
	fprintf(stdout,"######\n");
	fprintf(stdout,"dmarxtcp PINRED PINGREEN PINBLUE LISTENPORT\n");
	fprintf(stdout,"\n\n");
	fprintf(stdout,"Pins should be given in numbers used in wiringpi\n");
	fprintf(stdout,"\n\n");
	fprintf(stdout,"Example: dmarxtcp 4 5 6 6666\n");	
}

void clssrvsock(){
	delay(1500);
	try {
		closeSocket(socket_desc);
		//close(socket_desc);
		fprintf(stdout,"Serversocket closed.\n");
		writelog("Serversocket closed.\n"); //Logeintrag machen
	}
	catch (...) {
		fprintf(stdout,"Error closing Serversocket.\n");
		writelog("Error closing Serversocket\n"); //Logeintrag machen
	}
}

void signal_handler (int sig)
{
	/*
	https://www.gnu.org/software/libc/manual/html_node/Termination-Signals.html
	Macro: int SIGTERM

    The SIGTERM signal is a generic signal used to cause program termination. 
	Unlike SIGKILL, this signal can be blocked, handled, and ignored. 
	It is the normal way to politely ask a program to terminate.
    The shell command kill generates SIGTERM by default. 

Macro: int SIGINT

    The SIGINT (“program interrupt”) signal is sent when the user types the INTR character (normally C-c).
	See Special Characters, for information about terminal driver support for C-c. 

Macro: int SIGQUIT

    The SIGQUIT signal is similar to SIGINT, except that it’s controlled by a different key—the QUIT character, 
	usually C-\—and produces a core dump when it terminates the process, just like a program error signal. 
	You can think of this as a program error condition “detected” by the user.

    See Program Error Signals, for information about core dumps. See Special Characters, for information about terminal driver support.
    Certain kinds of cleanups are best omitted in handling SIGQUIT. For example, if the program creates temporary files, it should handle the other termination requests by deleting the temporary files. But it is better for SIGQUIT not to delete them, so that the user can examine them in conjunction with the core dump. 
	*/
	
  if (sig==SIGINT || sig==SIGQUIT || sig==SIGTERM){
	fprintf(stdout,"\nAbbruchsignal (z.B. STRG+C) empfangen. Beende Server...\n");
	writelog("Abbruchsignal (z.B. STRG+C) empfangen. Beende Server...\n"); //Logeintrag machen
	shuttingdown=true; //Abbruchsignal setzen	  
	clssrvsock();	
	exit(-9);
  }  
}



int main(int argc, char *argv[])
{	
//root is needed, because we are switching gpoi-pins
	setbuf(stdout, NULL); //disabled buffering for stdout, so that we can tee messages imidiately
	
	//Signalhandler abbonieren
	signal (SIGQUIT, signal_handler); 		
	signal (SIGINT, signal_handler);

	if (geteuid() != 0)
	{ //Not root
		fprintf(stdout,"\nRoot wird zur Ausführung benötigt. Bitte als root ausführen.\n Please run as root!");
		writelog("Program aborted due to failing root privileges.\n"); //Logeintrag machen
		return -2;
	}
	else
	{//root
		fprintf(stdout,"\nWilkommen bei dmarxtcp: Version %0.2f\n", myversion);
		/*
		//debug
		std::cout << argv[1];
		std::cout << argv[2];
		std::cout << argv[3];	
		//debug ende
		*/
		
		if (argc != 5){
			printHelp();
			return -2;
		}
		else {
			//argv[0] ist der Programmaufruf selbst
			pinred=atoi(argv[1]);
			pingreen=atoi(argv[2]);
			pinblue=atoi(argv[3]);
			listenPort = atoi(argv[4]); //Port
		}
		
		if (pinred<=0 || pingreen<=0 || pinblue<=0 || pinred>=30 || pingreen>=30 || pinblue>=30){
			//Ungültige Werte. Wert muss zwischen 1 und 29 liegen
			fprintf(stderr,"Invalid pinvalues given! Aborting\n");
			printHelp();
			return -3;
		}
		
		if (listenPort <=1024 || listenPort >65535){
			//Ungültiger Port. Alles bis 1024 ist reseviert. Maximalwert 65535
			fprintf(stderr,"Invalid listen port! Aborting\n");
			printHelp();
			return -3;
		}
				
		struct sockaddr_in server, client;
		std::string message;
		
		//GPIO-Pins initialisieren
		wiringPiSetup () ;
		pinMode (pinred, OUTPUT) ;
		pinMode (pingreen, OUTPUT) ;
		pinMode (pinblue, OUTPUT) ;
		
		//Pins testen
		fprintf(stdout,"Testing Pin for RED (%i):\n",pinred);
		digitalWrite (pinred, true);
		delay (200) ;
		fprintf(stdout,"Testing Pin for GREEN (%i):\n",pingreen);
		digitalWrite (pingreen, true);
		delay (200) ;
		fprintf(stdout,"Testing Pin for BLUE (%i):\n",pinblue);
		digitalWrite (pinblue, true);
		delay (1200) ;
		
		digitalWrite (pinred, false);	
		delay (200) ;
		digitalWrite (pingreen, false);	
		delay (200) ;
		digitalWrite (pinblue, false);

		writelog("Program started.\n"); //Logeintrag machen
		
		std::stringstream ss;
		ss << listenPort;		
		
		std::string msg="";
		msg += "Listening on Port: " + 
		ss.str() + 
		std::string("\n");//+listenPort;
		
		fprintf(stdout,msg.c_str());
		writelog(msg.c_str()); //Logeintrag machen		
		
		//Create socket
		socket_desc = socket(AF_INET, SOCK_STREAM, 0);
		if (socket_desc == -1)
		{
			fprintf(stderr,"Could not create socket\n");
			writelog("Could not create socket\n"); //Logeintrag machen
			return -1;
		}

		//Prepare the sockaddr_in structure
		server.sin_family = AF_INET;
		server.sin_addr.s_addr = INADDR_ANY;
		server.sin_port = htons(listenPort);

		//Bind
		if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
		{
			fprintf(stderr,"Socket bind failed. Is socket free?\n");
			writelog("Socket bind failed.\n"); //Logeintrag machen
			return -2;
		}
		fprintf(stdout,"Socket bind done.\n");
		writelog("Socket bind done.\n"); //Logeintrag machen
		//Listen for new connections
		listen(socket_desc, 3);

		//Accept and incoming connection
		fprintf(stdout,"Waiting for incoming connections...\n");
		writelog("Waiting for incoming connections.\n"); //Logeintrag machen
		c = sizeof(struct sockaddr_in);

		while (shuttingdown==false && (new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)))
		{//Server fährt gerade nicht herunter, die maximale Clientanzahl ist noch nicht erreicht und die eingehende Verbindung wurde akzeptiert
			
			pthread_t sniffer_thread; //neuen Thread deklarieren
			new_sock = (int*)malloc(1);
			*new_sock = new_socket;
				
			if (curclicnt<maxclients){	
				//Reply to the client
				//message = "Hello Client , I have received your connection.\n";
				//write(new_socket , message.c_str() ,  strlen(message.c_str()));				
				
				std::string clientip=inet_ntoa(client.sin_addr);
				int clientport=(int)ntohs(client.sin_port);
				std::string message ="New Connection (ID: "+std::to_string(curclicnt)+") from "+clientip+":"+std::to_string(clientport)+" accepted.\n";			
				
				fprintf(stdout,message.c_str());			
				writelog(message); //Logeintrag machen

				if (pthread_create(&sniffer_thread, NULL, connection_handler, (void*)new_sock/*socket*/) < 0)
				/*
				struct hathrargs args;
				args.socket=new_sock;
				args.connid=curclicnt;
				*/
				//if (pthread_create(&sniffer_thread, NULL, connection_handler, (void*)args) < 0)
				{
					fprintf(stderr,"could not create thread.\n");
					writelog("Error: could not create thread.\n"); //Logeintrag machen
					return 1;
				}		
							
				curclicnt+=1;
				
				fprintf(stdout,"Thread-Handler assigned. %d active clients.\n",curclicnt);
				writelog("Thread-Handler assigned.\n"); //Logeintrag machen
				
				//pthread_join( sniffer_thread , NULL); //Join the thread , so that we dont terminate before the thread
			}
			else {
				//Maxclients erreicht
				//Reply to the client
				message = "good bye\n";
				write(new_socket , message.c_str() ,  strlen(message.c_str()));
				fprintf(stderr, "Max. clients reached. - connection aborted.\n");	
				writelog("Max. clients reached. - connection aborted.\n"); //Logeintrag machen	
				//close(new_socket);
				closeSocket(new_socket);
			}
			/*
			//DEBUG
			std::string debug="Debug-Main: Shuttingdown="+BoolToString(shuttingdown)+"\n";
			fprintf(stdout, debug.c_str());
			//DEBUG Ende
			*/
			if (shuttingdown==true){
				delay (500);
				clssrvsock();
				break; //Schleife abbrechen.
			}
			
		}

		if (new_socket < 0)
		{
			fprintf(stderr,"accept failed.\n");
			writelog("Error: accept failed.\n"); //Logeintrag machen
			return 1;
		}		
		
		/*
		//Disable remaining LEDs
		digitalWrite (pinred, false);	
		delay (200) ;
		digitalWrite (pingreen, false);	
		delay (200) ;
		digitalWrite (pinblue, false);		
		*/
		
		
		return 0;
	} //<= comment in, if root is needed
}


//int doStuff(char code[1024], int connid)
int doStuff(char code[1024])
{
	int status = -1;
	if (
	(code[0]=='1' || code[0]=='0') &&
	(code[1]=='1' || code[1]=='0') &&
	(code[2]=='1' || code[2]=='0') &&
	 code[3]=='\r' &&
	 code[4]=='\n'	 
	)
		{
		//Zielstatus ermitteln
		bool redstate=charState(code[0]); //Erstes Zeichen = Rotstatus
		bool greenstate=charState(code[1]); //Zweites Zeichen = Gruenstatus
		bool bluestate=charState(code[2]); //Drittes Zeichen = Blaustatus
		
		std::string msg="";
		//msg += "Received new LED settings from connection "+std::to_string(connid)+". Code: \"" + std::string(code).substr(0,3) + std::string("\"\n");
		msg += "Received new LED settings. Code: \"" + std::string(code).substr(0,3) + std::string("\"\n");
		
		fprintf(stdout,msg.c_str());
		writelog(msg.c_str());
		
		delay(30); //30ms warten um die das Schaltvermögen der Relais nicht auszureizen
		//LEDs setzen		
						
		digitalWrite (pinred, redstate);
		digitalWrite (pingreen, greenstate);
		digitalWrite (pinblue, bluestate);	
		
		status = 0; //exitcode
		}
		
	else if (
	code[0]=='e' &&
	code[1]=='x' &&
	code[2]=='i' &&
	code[3]=='t' &&
	code[4]=='\r' &&
	code[5]=='\n'
	) 
	{
		status=1; //exitcode
	}
	
	return status;
}


void lightUpPin(int pinnumber, int period){
	//Setzt den Status eines pins fuer einen gewissen Zeitraum auf 1 und anschließend wieder auf null
	digitalWrite (pinnumber, true);
	delay(period);
	digitalWrite (pinnumber, false);
}



void *connection_handler(void *socket_desc)
//void *connection_handler(void *arguments)
{
	/*
	* This will handle connection for each client
	* */
	
	//struct myargs *args = arguments;
	
    //Get the socket descriptor
    int sock = *(int*)socket_desc;
	//int sock = args->socket;
	//int connid= args->connid;
    int read_size;
    std::string message;
	char client_message[1024];
	char reciev[1024];
    int sendStatus=0; 
	char result[100];		
	
	//connections[connid]=sock; //Dieses Socket dem Socketarray hinzufügen
	
    //Send some messages to the client
    message = "LED-Control ready. Please tell me what to do.\n";
    write(sock , message.c_str() , strlen(message.c_str()));
 
	while(shuttingdown==false && (read_size = recv(sock , client_message , 1024 , 0)) > 0 ) //Solange größer 0, besteht die Verbindung zum Client
    {
		/*
		//DEBUG
		std::string debug="Debug-Inner: Shuttingdown="+BoolToString(shuttingdown)+"\n";
		fprintf(stdout, debug.c_str());
		//DEBUG Ende
		*/
		
		//Print out the recieved message				
		//fprintf(stdout, client_message);		
		
		//int retval=doStuff(client_message, connid);
		int retval=doStuff(client_message);
		if (retval==0){ 
		message = "command ok\n";
		}
		else if (retval==1 || shuttingdown==true){
			//Verbindung soll beendet werden
			//Free the socket pointer
			message = "good bye\n";	
		}
		else {
			message = "command error\n";
		}
		
		//Answer
		write(sock, message.c_str(), strlen(message.c_str())); //Send message to the client
		
		if (retval==1 || shuttingdown==true){
			//Close connection 
			shutdown(sock, SHUT_RDWR);	
			fprintf(stdout,"connection closed by server.\n");
			writelog("connection closed by server.\n");	
			delay(100); //wait 100ms
			break; //Aborts the while...
		}
    }
     
    if(read_size == 0)//Client hat die Verbindung getrennt
    {
        fprintf(stdout,"connection closed by Client\n");
		writelog("connection closed by Client\n");		
        //fflush(stdout);
    }
    else if(read_size == -1) //Verbindung wurde unterbrochen.
    {
        fprintf(stderr,"connection aborted\n");
		writelog("connection aborted\n");
    }
         
    //Free the socket pointer
	
	close(sock);
	//closeSocket(sock);
    free(socket_desc);
    
	
	
	curclicnt-=1; //Clientverbindungen um eine reduzieren
	
	fprintf(stdout,"%d active clients.\n",curclicnt);
    return 0;
}