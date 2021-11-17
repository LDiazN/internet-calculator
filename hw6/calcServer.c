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
#define MAX_SIMULT_SESSIONS 100 		// Max amount of simultaneous sessions

/// Server persistent data
struct Server
{
	struct Calc *calc; 			// calc object with stored persistent data
	uint32_t 	port;	    	// port to listen to 
	bool  		running;    	// If the server should stope
	int 		socket_fd;  	// File descriptor for the server socket
	pthread_t main_thread_id; 				// Main thread id for signals
	pthread_mutex_t calc_mutex; 			// Mutex required to ensure thread safety with calculator operations
	pthread_mutex_t thread_pool_mutex; 		// Mutex required to ensure thread safety with thread pool
	pthread_t threads[MAX_SIMULT_SESSIONS];	//	Pool of threads
	bool	  thread_destroy_queue[MAX_SIMULT_SESSIONS]; // queue of threads to be destroyed
};

/// Arguments to pass to the session thread when creating it
struct SessionArgs
{
	int peer_socket_fd;
	struct Server * server;
	size_t thread_index;
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
///		args = argument struct to pass when creating the thread
void *chat_with_client(void * args);

/// Summary:
/// 	issue a server shutdown
/// Parameters:
///		server = server to shutdown

void server_shutdown_start(struct Server * server);

/// Summary:
/// 	Clean the destroy queue, deleting threads marked for destroy
void server_clean_destroy_queue(struct Server *server);

/// Summary 
///		Create a thread in the internal structure of the server. Returns a status code depending if it was possible
///	Parameters
///		server : pointer to a server to use
///		peer_socket_fd : file descriptor for peer socket
void server_create_thread(struct Server * server, int peer_socket_fd);

/// Summary
///		Queue this thread to be destroyed
void server_destroy_thread(struct Server* server, size_t thread_index);

// Server object for our application
struct Server server;

// Logger object
Logger * logger = NULL;

// thread object buffer

/// Summary:
///		Use this signal handler to wake up the server in case it is halting in "accept"
///		when a shutdown is triggered
///	Paraemters:
///		Signal code, should be SIGALARM only 
void sigalarm_handler(int signum);

int main(int argc, char **argv) {

	// Initialize logging
	logger = logger_get();	

	// Set up interruption signal:
	signal(SIGINT, sigint_handler);

	// Set sig alarm handler
	struct sigaction sig_act;
	sigemptyset(&sig_act.sa_mask);
	sig_act.sa_flags = 0;
	sig_act.sa_handler = sigalarm_handler;
	sigaction(SIGALRM, &sig_act, NULL);

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

	while (server_running(&server))
	{
		// Listen for a connection 
		LOG_TRACE("Waiting for new connections\n");
		int peer_socket = accept(server.socket_fd, NULL, NULL); // will halt here waiting for requests
		if (peer_socket < 0 && errno == EINTR) // an alarm was set to trigger a shutdown
		{
			LOG_INFO("Shutdown signal received!\n")
			continue; 
		}
		else if (peer_socket < 0)
		{
			LOG_ERROR("Error ocurred accepting connections from peers, error code: %d\n", peer_socket);
			continue; 
		}

		LOG_TRACE("New connection established!\n");
		
		// Start an interactive session
		server_create_thread(&server, peer_socket);
	}

	// Shut down server object
	server_shutdown(&server);
	
	// Destroy logger
	logger_destroy(logger);

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
	server->main_thread_id = pthread_self();
	
	// Reset threads to 0
	memset(server->threads, 0, sizeof(server->threads));

	// init calc mutex
	if (pthread_mutex_init(&server->calc_mutex, NULL) != 0)
	{
		LOG_ERROR("Calc mutex initialization failed\n");
		exit(1);
	}

	// init thread pool mutex
	if (pthread_mutex_init(&server->thread_pool_mutex, NULL) != 0)
	{
		LOG_ERROR("Thread pool mutex initialization failed\n");
		exit(1);
	}

	char port_str[6];

	sprintf(port_str, "%u", server->port);

	server->socket_fd = open_listenfd(port_str);

	// Error checking 
	if (server->socket_fd == -1 || server->socket_fd == -1)
		LOG_ERROR("Could not get a socket using open_listenfd. Error code: %d\n", server->socket_fd);
}

/// Summary
/// 	Clean the destroy queue, deleting threads marked for destroy
void server_clean_destroy_queue(struct Server *server)
{
	LOG_INFO("Cleaning destroy queue\n");

	// iterate over the thread destroy queue
	for (size_t i = 0; i < MAX_SIMULT_SESSIONS; i ++ )
	{
		if (server->thread_destroy_queue[i])
		{
			// the thread should be over by now
			pthread_join(server->threads[i], NULL);

			server->thread_destroy_queue[i] = FALSE;
			server->threads[i] = 0;
		}
	}
}

/// Summary 
///		Create a thread in the internal structure of the server. Returns a status code depending if it was possible

///	Parameters
///		server : pointer to a server to use
///		args   : args for the new session if it is succesfully created
void server_create_thread(struct Server * server, int peer_socket_fd)
{
	// Lock thread pool, not no one can access the thread pool
	pthread_mutex_lock(&server->thread_pool_mutex);

	// Clean pending threads 
	server_clean_destroy_queue(server);

	// to create a thread, search for an index with no thread associated
	size_t next_thread_index = 0;
	for(; 
			next_thread_index < MAX_SIMULT_SESSIONS && 
			server->threads[next_thread_index]
		; 
			next_thread_index ++
		);

	// if no session was available return an error status code 
	if (next_thread_index == MAX_SIMULT_SESSIONS)
	{
		LOG_WARN("Could not create thread, no sessions available\n");
		Rio_writen(peer_socket_fd, "No available sessions right now, try again later :(", 52);
		return;
	}

	// Create thread
	struct SessionArgs * args = malloc(sizeof(struct SessionArgs));
	args->server = server;
	args->peer_socket_fd = peer_socket_fd;
	args->thread_index = next_thread_index;

	LOG_INFO("Starting new session with id: %lu\n", next_thread_index);
	Pthread_create(&server->threads[next_thread_index], NULL, chat_with_client, (void *) args);

	// Unlock thread pool, no some process can access the thread pool
	pthread_mutex_unlock(&server->thread_pool_mutex);
}

/// Summary
///		Queue this thread to be destroyed
void server_destroy_thread(struct Server* server, size_t thread_index)
{
	// Sanity check
	assert( thread_index < MAX_SIMULT_SESSIONS && "Thread index out of range");

	// Lock thread pool
	pthread_mutex_lock(&server->thread_pool_mutex);
	LOG_INFO("Marking thread %lu for destroy\n", thread_index);

	// mark it in the destroy queue
	server->thread_destroy_queue[thread_index] = FALSE;

	// Unlock thread pool
	pthread_mutex_unlock(&server->thread_pool_mutex);
}

// Shut down server
void server_shutdown(struct Server * server)
{
	// Clean sessions marked for destroy
	server_clean_destroy_queue(server);

	// Wait pending sessions
	for (size_t i=0; i < MAX_SIMULT_SESSIONS; i++)
	{
		pthread_t current_thread = server->threads[i];
		if (current_thread)
		{
			pthread_join(current_thread, NULL);
			server->threads[i] = 0;
		}
	}

	// Destroy server object
	LOG_INFO("Shutting down server...\n");
	calc_destroy(server->calc);
	server->calc = NULL;
	server->running = FALSE;
	Close(server->socket_fd); 

	// Destroy mutex
	pthread_mutex_destroy(&server->calc_mutex);

	LOG_INFO("Server shutdown succesful\n");
}

// Trigger a server shutdown
void server_shutdown_start(struct Server * server)
{
	server->running = FALSE;
	pthread_kill(server->main_thread_id, SIGALRM);
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

// To be called when a shutdown is issued and you want to wake up the server while it's halting on accept
void sigalarm_handler(int signum)
{
	assert(signum == SIGALRM && "This handler function only supports SIGALARMS functions");

	if (!server_running(&server))
		LOG_INFO("Shut down signal triggered\n");
}

void *chat_with_client(void *args) {
	struct SessionArgs * session_args = (struct SessionArgs *) args;
	rio_t in;
	char linebuf[LINEBUFF_SIZE];

	int infd; // where to read from
	int outfd;// where to write to

	infd = outfd = session_args->peer_socket_fd;
	struct Server * server = session_args->server;
	size_t thread_index = session_args->thread_index;

	/* wrap standard input (which is file descriptor 0) */
	rio_readinitb(&in, infd);

	/*
	 * Read lines of input, evaluate them as calculator expressions,
	 * and (if evaluation was successful) print the result of each
	 * expression.  Quit when "quit" command is received.
	 */
	bool done = FALSE;
	while (!done) {
		ssize_t n = Rio_readlineb(&in, linebuf, LINEBUFF_SIZE);
		LOG_TRACE("peer said %s\n", linebuf);
		if (n <= 0) {
			/* error or end of input */
			done = TRUE;
		} else if (strcmp(linebuf, "quit\n") == 0 || strcmp(linebuf, "quit\r\n") == 0) {
			/* quit command */
			done = TRUE;
		} else if (strcmp(linebuf, "shutdown\n") == 0 || strcmp(linebuf, "shutdown\r\n") == 0) {

			// Stop interactive session 
			done = TRUE;

			// Write a farewell message
			Rio_writen(outfd, "Shutting down server, have a nice day :)\n", 42);

			// Issue a server shutdown
			server_shutdown_start(server);

		} else {
			/* process input line */
			int result;
			if (calc_eval(server->calc, linebuf, &result) == FAILURE) {
				/* expression couldn't be evaluated */
				Rio_writen(outfd, "Error\n", 6);
			} else {
				/* output result */
				int len = snprintf(linebuf, LINEBUFF_SIZE, "%d\n", result);
				if (len < LINEBUFF_SIZE) {
					Rio_writen(outfd, linebuf, len);
				}
			}
		}
	}

	// Queue this thread to be destroyed
	server_destroy_thread(server, thread_index);

	// Free args for this session
	free(session_args);

	// Close connection with peer
	close(session_args->peer_socket_fd);
	return NULL;
}

/// Thread safe eval
int server_calc_eval(struct Server * server, const char *expr, int *result)
{
	pthread_mutex_lock(&server->calc_mutex);

	int res = calc_eval(server->calc, expr, result);

	pthread_mutex_unlock(&server->calc_mutex);

	return res;
}