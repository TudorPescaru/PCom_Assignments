#ifndef _REQUESTS_
#define _REQUESTS_

// computes and returns a GET request string (query_params, jwt token
// and cookies can be set to NULL if not needed)
char *compute_get_request(char *host, char *url, char *query_params,
							char **cookies, int cookies_count, char *jwt);

// computes and returns a DELTE request string (query_params, jwt token
// and cookies can be set to NULL if not needed)
char *compute_delete_request(char *host, char *url, char **cookies,
								int cookies_count, char *jwt);

// computes and returns a POST request string (cookies and jwt token can be NULL if not needed)
char *compute_post_request(char *host, char *url, char* content_type, char **body_data,
							int body_data_fields_count, char** cookies, int cookies_count, char *jwt);

#endif
