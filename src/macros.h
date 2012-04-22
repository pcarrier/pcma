#ifndef PCMA__MACROS_H
#define PCMA__MACROS_H

#define LOG_SERV(...) \
	fprintf(stderr, "[SERVER] " __VA_ARGS__);

#define LOGGING_DEBUG (log_level > 2)
#define LOGGING_INFO (log_level > 1)
#define LOGGING_ERROR (log_level > 0)
#define LOG_DEBUG(...) \
	{if (LOGGING_DEBUG) fprintf(stderr, "[DEBUG] " __VA_ARGS__);}
#define LOG_INFO(...) \
	{if (LOGGING_INFO) fprintf(stderr, "[INFO] " __VA_ARGS__);}
#define LOG_ERROR(...) \
	{if (LOGGING_ERROR) fprintf(stderr, "[ERROR] " __VA_ARGS__);}

#define MAIN_ERR_FAIL(str) {perror(str); goto err;}

#endif /* PCMA__MACROS_H */
