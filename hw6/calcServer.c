/*
	Implementación del servidor
*/
#include <stdio.h>      /* for snprintf */
#include "csapp.h"
#include "calc.h"
#include "logger.h"
#include <assert.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

/// Server persistent data
struct Server
{
	struct Calc *calc; // calc object with stored persistent data
	size_t port;	   // port to listen to 
	int running;   // If the server should stope
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
int server_running(struct Server * server);

int main(int argc, char **argv) {
	// Initialize logging
	Logger * logger = logger_get();	

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

	if (port < 1024)
	{
		LOG_ERROR("Invalid port. The given port is reserved for admin privileges. Choose a port greater or equal to 1024\n")
		return 1;
	}

	// Server object for our application
	struct Server server = {.calc=NULL, .port = 0, .running = 0};

	// Start the server
	server_start(&server, port);

	while (!server_running(&server))
	{

	}

	// Shut down server object
	server_shutdown(&server);
	
	// Destroy logger
	logger_destroy(logger);
	return 0;
}


// -- < Implementation > -----------------------------------------

// Tells if the server is running
int server_running(struct Server *server)
{
	return server->running;
}

// Initializes server 
void server_start(struct Server * server, size_t port)
{
	LOG_INFO("Starting server, listenning to port: %lu", port);
	server->calc = calc_create();
	server->port = port;
	server->running = TRUE;
}

void server_shutdown(struct Server * server)
{
	LOG_INFO("Shutting down server...");
	calc_destroy(server->calc);
	server->calc = NULL;
}

