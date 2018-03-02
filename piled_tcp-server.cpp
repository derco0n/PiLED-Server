/*
Kleiner TCP-Server 
Diese Software stellt einen TCP-Server dar, welcher zur Ausführung auf embedded-systemen wie dem Raspberry-Pi gedacht ist.
-Die vorliegende Version wurde angepasst um LED's über die GPIO-Pins des Raspberry-Pi zu steuern. (D. Marx, 28.02.2018)

Dieser Server nutzt die Threadbibliothek um Multithreading (pro client ein Thread) zu ermöglichen und die Bibliothek WiringPi zur Ansteuerung der GPIO-Pins des RaspberryPi.
Compile: g++ ./dmarxtcp.cpp -lpthread -lwiringPi

Autor: D. Marx
*/

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>    //strlen
#include <signal.h> //STRG+C, STRG+Z, etc...
#include <string>
#include <cstring>
#include <iostream>
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

const double myversion=0.17; //Diese Programmversion
int listenPort = 0; //Auf welchem Port soll gelauscht werden?
void *connection_handler(void *);

int connection_id;

char retchar;
char retstr[1024];
char serialAnswer[1024];
int y=0;

std::string itos(int n)
{
	std::stringstream ss;
	ss << n;
	std::string str = ss.str();
	return str;
}

std::string timeStr(void)
{
	std::string myReturn;
	struct tm *current;
	time_t now;

	time(&now);
	current = localtime(&now);

	std::string day, mon, year, hour, min, sec;
	if (current->tm_mday < 10)
	{
		day = "0" + itos(current->tm_mday);
	}
	else
	{
		day = itos(current->tm_mday);
	}


	if (current->tm_mon+1 < 10)
	{
		mon = "0" + itos(current->tm_mon+1);
	}
	else
	{
		mon = itos(current->tm_mon+1);
	}

	
	
	year = itos(1900 + current->tm_year); //Jahr



	if (current->tm_hour < 10)
	{
		hour = "0" + itos(current->tm_hour);
	}
	else
	{
		hour = itos(current->tm_hour);
	}

	
	if (current->tm_min < 10)
	{
		min = "0" + itos(current->tm_min);
	}
	else
	{
		min = itos(current->tm_min);
	}


	if (current->tm_sec < 10)
	{
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
	printf("\n");
	printf("Usage:\n");
	printf("######\n");
	printf("dmarxtcp PINRED PINGREEN PINBLUE LISTENPORT\n");
	printf("\n\n");
	printf("Pins should be given in numbers used in wiringpi\n");
	printf("\n\n");
	printf("Example: dmarxtcp 4 5 6 6666\n");	
}




int pinred=0;
int pingreen=0;
int pinblue=0;

int socket_desc, new_socket, c, *new_sock;


void signal_handler (int sig)
{
  if (sig==SIGQUIT){
	  close(socket_desc);
	  exit;  
  }
}


int main(int argc, char *argv[])
{	
	signal (SIGQUIT, signal_handler);
	signal (SIGINT, signal_handler);

	
	struct sockaddr_in server, client;
	std::string message;

	printf("\nWilkommen bei dmarxtcp: Version %0.2f\n", myversion);
	
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
		printf("Invalid pinvalues given! Aborting\n");
		printHelp();
		return -3;
	}
	
	if (listenPort <=1024 || listenPort >65535){
		//Ungültiger Port. Alles bis 1024 ist reseviert. Maximalwert 65535
		printf("Invalid listen port! Aborting\n");
		printHelp();
		return -3;
	}
	
	//GPIO-Pins initialisieren
	wiringPiSetup () ;
	pinMode (pinred, OUTPUT) ;
	pinMode (pingreen, OUTPUT) ;
	pinMode (pinblue, OUTPUT) ;
	
	
	//Pins testen
	printf("Testing Pin for RED (%i):\n",pinred);
	digitalWrite (pinred, true);
	delay (200) ;
	printf("Testing Pin for GREEN (%i):\n",pingreen);
	digitalWrite (pingreen, true);
	delay (200) ;
	printf("Testing Pin for BLUE (%i):\n",pinblue);
	digitalWrite (pinblue, true);
	delay (1200) ;
	
	digitalWrite (pinred, false);	
	delay (200) ;
	digitalWrite (pingreen, false);	
	delay (200) ;
	digitalWrite (pinblue, false);
	
/*
//root is not needed, because we are just switching gpoi-pins
	if (geteuid() != 0)
	{ //Not root
		printf("\nRoot wird zur Steuerung von Diensten benötigt. Bitte als root ausführen.\n Please run as root!");
		writelog("Program aborted due to failing root privileges.\n"); //logeintrag machen
		return -2;
	}
	else
	{
*/
		
		
		//writelog(timeStr()); //zeitpunkt loggen
		writelog("Program started.\n"); //logeintrag machen
		
		std::stringstream ss;
		ss << listenPort;
		
		
		std::string msg="";
		msg += "Listening on Port: " + 
		ss.str() + 
		std::string("\n");//+listenPort;
		
		printf(msg.c_str());
		writelog(msg.c_str()); //logeintrag machen

		//Create socket
		socket_desc = socket(AF_INET, SOCK_STREAM, 0);
		if (socket_desc == -1)
		{
			printf("Could not create socket");
			writelog("Could not create socket\n"); //logeintrag machen
			return -1;
		}

		//Prepare the sockaddr_in structure
		server.sin_family = AF_INET;
		server.sin_addr.s_addr = INADDR_ANY;
		server.sin_port = htons(listenPort);

		//Bind
		if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
		{
			puts("Socket bind failed. Is socket free?\n");
			writelog("Socket bind failed.\n"); //logeintrag machen
			return -2;
		}
		puts("Socket bind done.\n");
		writelog("Socket bind done.\n"); //logeintrag machen
		//Listen for new connections
		listen(socket_desc, 3);

		//Accept and incoming connection
		puts("Waiting for incoming connections...\n");
		writelog("Waiting for incoming connections.\n"); //logeintrag machen
		c = sizeof(struct sockaddr_in);

		while ((new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)))
		{
			puts("Connection accepted\n");
			writelog("New Connection\n"); //logeintrag machen
			//Reply to the client
			//message = "Hello Client , I have received your connection.\n";
			//write(new_socket , message.c_str() ,  strlen(message.c_str()));

			pthread_t sniffer_thread;
			new_sock = (int*)malloc(1);
			*new_sock = new_socket;

			if (pthread_create(&sniffer_thread, NULL, connection_handler, (void*)new_sock) < 0)
			{
				perror("could not create thread");
				writelog("could not create thread\n"); //logeintrag machen
				return 1;
			}

			//Now join the thread , so that we dont terminate before the thread
			pthread_join( sniffer_thread , NULL); //Not needed. because we can terminate before the thread. doesnt matter here
			
			puts("Handler assigned");
			writelog("Handler assigned\n"); //logeintrag machen
		}

		if (new_socket < 0)
		{
			perror("accept failed");
			writelog("accept failed\n"); //logeintrag machen
			return 1;
		}
		
		
		//Disable remaining LEDs
		digitalWrite (pinred, false);	
		delay (200) ;
		digitalWrite (pingreen, false);	
		delay (200) ;
		digitalWrite (pinblue, false);
		
		
		
		
		return 0;
	//} //<= comment in, if root is needed
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


int doStuff(char code[1024])
{
	int status = -1;
	
	//Diese Methode verarbeitet anfragen
	
	/*
	if (strncmp(code, "00001", 5) == 0) //Die ersten 5 Zeichen vergleichen => strncmp==0 bedeutet, dass die beiden zu vergleichenden strings identisch sind
	{
		//Beispiel:
		//CODE: 00001 -> restart TOR-service
		//printf("recieved code \"%s\": restarting TOR...", code);
		//writelog("Command: restart TOR\n"); //logeintrag machen
		//status = system("/etc/init.d/tor restart");
	}
	
	//else if (){}
	
	else
	{
		printf("recieved code \"%s\": No such command", code);

	}
	*/
	

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
		
		//
		
		std::string msg="";
		msg += "Received new LED states. Code: \"" + 
		std::string(code) + 
		std::string("\"\n");//+listenPort;
		
		printf(msg.c_str());
		writelog(msg.c_str());
		
		delay(30); //30ms warten um die das Schaltvermögen der Relais nicht aszureizen
		//LEDs setzen
		
		digitalWrite (pinred, redstate);
		digitalWrite (pingreen, greenstate);
		digitalWrite (pinblue, bluestate);
		
		//Alternativ:
		/*
		lightUpPin(pinred, 1000);
		lightUpPin(pingreen, 1000);
		lightUpPin(pinblue, 1000);
		*/
		//um die PINS für je eine Sekunde an und anschlißend wieder auszuschalten
		
		//Status OK
		status = 0;
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
		status=1; //exit
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
{
	/*
	* This will handle connection for each client
	* */


    //Get the socket descriptor
    int sock = *(int*)socket_desc;
    int read_size;
    std::string message;
	char client_message[1024];
	char reciev[1024];
    int sendStatus=0; 
	char result[100];
		
	
    //Send some messages to the client
    message = "LED-Control ready. Please tell me what to do.\n";
    write(sock , message.c_str() , strlen(message.c_str()));
	     
    //Receive a message from client
	//recv(sock , client_message , 1024 , 0);
    //strcpy(reciev, client_message);
		
	
	while( (read_size = recv(sock , client_message , 1024 , 0)) > 0 )
    {
		//Print out the recieved message
		puts(client_message);
				
		strcpy(reciev, client_message);
		
		//printf("String: %s\n",reciev); //Debug
		
		
		//usleep(500);
		int retval=doStuff(client_message);
		if (retval==0){ //aufruf der Funktion die wirklich einen Systemaufruf macht
		message = "command ok\n";
		}
		else if (retval==1){
			//Verbindung soll beendet werden
			//Free the socket pointer
			message = "good bye\n";			
			
		}
		else {
			message = "command error\n";
		}
		
		//Answer
		write(sock, message.c_str(), strlen(message.c_str()));
		
		if (retval==1){
			//Close connection 
			shutdown(sock, SHUT_RDWR);	
			delay(100); //wait 100ms
			break; //Aborts the while...
		}
    }
     
    if(read_size == 0)
    {
        puts("Client disconnected");
		
        fflush(stdout);
    }
    else if(read_size == -1)
    {
		
        perror("recv failed");
    }
         
    //Free the socket pointer
	close(sock);
    free(socket_desc);
     
    return 0;
}
