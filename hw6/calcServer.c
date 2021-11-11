/*
	Implementación del servidor
*/
#include <stdio.h>      /* for snprintf */
#include "csapp.h"
#include "calc.h"
#include "logger.h"
#include <assert.h>
#include <string.h>
#include <signal.h>

// Booleans
#define TRUE 1
#define FALSE 0
#define bool int

// max size to read from input
#define LINEBUFF_SIZE 1024

// Server constraints
#define PORT_MIN 1024
#define PORT_MAX 65535
#define SERVER_ADDR INADDR_LOOPBACK // Listen to localhost
// #define SERVER_ADDR INADDR_ANY   // Listen anything
#define MAX_CONNECTION_QUEUE_SIZE 100


/// Server persistent data
struct Server
{
	struct Calc *calc; 		// calc object with stored persistent data
	uint32_t 	port;	    // port to listen to 
	bool  		running;    // If the server should stope
	int 		socket_fd;  // File descriptor for the server socket
	struct sockaddr_in address; // Address object
	int address_len;		// Size in bytes of the address object
	int opt;				// ???
};

/// Summary:
/// 	Initialize server
///	Parameters:
///		posrt = port to interact with
void server_start(struct Server * server, size_t port);

/// Summary:
///		Destroy current server object
void server_shutdown(struct Server * server);

/// Summary:
///		Check if the provided server is running
/// Parameters:
///		server = Pointer to the server to check if it's running
/// Returns:
///		0 if not running, anything else otherwhise
bool server_running(struct Server * server);

/// Summary:
/// 	Handle interruption signal (SIGINT), so we can safely destroy the server on interruption
/// Parameters:
/// 	signum = Signal number to handle
void sigint_handler(int signum);

/// Summary:
///		Main function to interact with the client in an interactive http session using the given server state, 
///		reading input from the provided file descriptor, and writing output to the provided file descriptor
///	Parameters
///		server = current server state
///		infd   = file descriptor for the file where to read the input
///		outfd  = file descriptor for the file where to write the output
void chat_with_client(struct Server * server, int infd, int outfd);

// Server object for our application
struct Server server = {.calc=NULL, .port = 0, .running = 0, .socket_fd = 0};

// Logger object
Logger * logger = NULL;

int main(int argc, char **argv) {

	// Initialize logging
	logger = logger_get();	

	// Set up interruption signal:
	signal(SIGINT, sigint_handler);

	// Check arguments
	if (argc <2)
	{
		LOG_ERROR("Not enough arguments: No port provided.\n");
		return 1;
	}
	else if(argc > 2)
		LOG_WARN("Too many arguments: taking only first argument\n");

	// Read port argument 
	size_t port = 0;
	sscanf(argv[1], "%lu", &port);

	// Error checking
	if (port < PORT_MIN) // Check port lower bound
	{
		LOG_ERROR("Invalid port. The given port is reserved for admin privileges. Choose a port greater or equal to 1024\n")
		return 1;
	}
	else if (port > PORT_MAX) // Check port upper bound
	{
		LOG_ERROR("Invalid port. The given port number is greater than the max port number: %d\n", PORT_MAX);
	}

	// Start the server
	server_start(&server, port);


	// TEMPORARY
	char peer_msg_buffer[LINEBUFF_SIZE];	// to store read message
	struct sockaddr_in peer_addr;		// to store peer address
	memset(&peer_addr, 0, sizeof(peer_addr));	// set to zeros just in case
	int peer_addr_size = sizeof(peer_addr);		// Peer address size

	while (server_running(&server))
	{
		// Listen for a connection 
		LOG_TRACE("Waiting for new connections zzz....\n");
		int new_socket = Accept(server.socket_fd, (struct sockaddr *) &peer_addr, (socklen_t *) &peer_addr_size);
		LOG_TRACE("Not more waiting!\n");

		Read(new_socket, peer_msg_buffer, LINEBUFF_SIZE);
		LOG_WARN("%s\n", peer_msg_buffer);
	}

	// Shut down server object
	server_shutdown(&server);
	
	return 0;
}


// -- < Implementation > -----------------------------------------

// Tells if the server is running
int server_running(struct Server * server)
{
	return server->running;
}

// Initializes server 
void server_start(struct Server * server, size_t port)
{
	LOG_INFO("Starting server, listenning to port: %lu\n", port);
	server->calc = calc_create();
	server->port = port;
	server->running = TRUE;

	// Create socket:
	// 	- AF_INET to tell the socket to communicate using ipv4 communication domain
	//	- SOCK_STREAM to tell it to communicate using tcp connections
	//	- 0 for internet protocol (IP)
	server->socket_fd = Socket(AF_INET, SOCK_STREAM, 0);

	// TODO Capaz debería usar setsocketopt aquí en caso de que tenga problemas con lo de reuse addres y port
	if (setsockopt(server->socket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                                                  &server->opt, sizeof(server->opt)))
    {
        LOG_ERROR("Error setting up socket options");
        exit(EXIT_FAILURE);
    }

	server->address.sin_family = AF_INET;
	server->address.sin_addr.s_addr = INADDR_ANY;
	server->address.sin_port = htonl(server->port);
	server->address_len = sizeof(server->address);

	// Bind the socket to the corresponding direction and port (localhost and stored port)
	Bind(server->socket_fd, (struct sockaddr *) &server->address, sizeof(server->address));

	// Start to listen 
	Listen(server->socket_fd, MAX_CONNECTION_QUEUE_SIZE);
}

// Shut down server
void server_shutdown(struct Server * server)
{
	// Destroy server object
	LOG_INFO("Shutting down server...\n");
	calc_destroy(server->calc);
	server->calc = NULL;
	server->running = FALSE;

	LOG_INFO("Server shutdown succesful\n");

	// Destroy logger
	logger_destroy(logger);
}

// To be called when an interruption signal is called
void sigint_handler(int signum)
{
	// Sanity check
	assert(signum == SIGINT && "Invalid signal for sigint handler");

	LOG_INFO("Interruption Signal Received (SIGINT)\n");

	// Server shutdown
	server_shutdown(&server);

	// Call default signal on shutdown finished
	signal(SIGINT, SIG_DFL);
	raise(SIGINT);
}

void chat_with_client(struct Server *server, int infd, int outfd) {
	rio_t in;
	char linebuf[LINEBUFF_SIZE];

	/* wrap standard input (which is file descriptor 0) */
	rio_readinitb(&in, infd);

	/*
	 * Read lines of input, evaluate them as calculator expressions,
	 * and (if evaluation was successful) print the result of each
	 * expression.  Quit when "quit" command is received.
	 */
	bool done = FALSE;
	while (!done) {
		ssize_t n = rio_readlineb(&in, linebuf, LINEBUFF_SIZE);
		if (n <= 0) {
			/* error or end of input */
			done = TRUE;
		} else if (strcmp(linebuf, "quit\n") == 0 || strcmp(linebuf, "quit\r\n") == 0) {
			/* quit command */
			done = TRUE;
		} else if (strcmp(linebuf, "shutdown\n") == 0 || strcmp(linebuf, "shutdown\r\n") == 0) {
			done = TRUE;
			// Will stop the main loop
			server_shutdown(server);

		} else {
			/* process input line */
			int result;
			if (calc_eval(server->calc, linebuf, &result) == 0) {
				/* expression couldn't be evaluated */
				rio_writen(outfd, "Error\n", 6);
			} else {
				/* output result */
				int len = snprintf(linebuf, LINEBUFF_SIZE, "%d\n", result);
				if (len < LINEBUFF_SIZE) {
					rio_writen(outfd, linebuf, len);
				}
			}
		}
	}
}