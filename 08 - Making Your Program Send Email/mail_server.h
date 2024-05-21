struct mail_server_info {
  char server_hostname[256];
  int priority;
};

/*
 * Utilizes the existing `dig` command line tool for getting the mail serers
 * given a domain
 */
struct mail_server_info *get_mail_servers(char *domain, int *count);
