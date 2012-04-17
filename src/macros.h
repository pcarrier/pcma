#ifndef PCMA__MACROS_H
#define PCMA__MACROS_H

#define LOG_SERV(...) \
	fprintf(stderr, "[SERV]  " __VA_ARGS__);
#define LOG_DEBUG(...) \
	{if (log_level > 2) fprintf(stderr, "[DEBUG] " __VA_ARGS__);}
#define LOG_INFO(...) \
	{if (log_level > 1) fprintf(stderr, "[INFO]  " __VA_ARGS__);}
#define LOG_ERROR(...) \
	{if (log_level > 0) fprintf(stderr, "[ERROR] " __VA_ARGS__);}

#define MAIN_ERR_FAIL(str) {perror(str); goto err;}

#endif /* PCMA__MACROS_H */