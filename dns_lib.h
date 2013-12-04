#ifndef _DNS_LIB_H
#define _DNS_LIB_H

struct dns_t {

  // header section
  uint16_t msg_id;
  uint16_t flags;
  uint16_t QR;
  uint16_t OPCODE;
  uint16_t AA;
  uint16_t RD;
  uint16_t RA;
  uint16_t RCODE;
  uint16_t QDCOUNT;
  uint16_t ANCOUNT;
  
  // query section
  char *QNAME;
  uint16_t QTYPE;
  uint16_t QCLASS;

  // answer section
  uint16_t NAME;
  uint16_t TYPE;
  uint16_t CLASS;
  uint32_t TTL;
  uint16_t RDLENGTH;
  uint32_t RDATA;

};


/**
 * Helper function, make header section
 *
 * @param dns The pointer to the buffer to save dns
 * @param msg_id Message ID
 * @param flags QR & OPCODE & AA & TC &RD &RA &RCODE
 * @param QDCOUNT 
 * @param ANCOUNT
 *
 * @return size of header section, which is always 12 bytes
 */
int make_head(char *dns, uint16_t msg_id, uint16_t flags, uint16_t QDCOUNT, uint16_t ANCOUNT, uint16_t NSCOUNT, uint16_t ARCOUNT);

/**
 * Helper function, return the number of entries in the question section
 *
 * @param node The query string
 *
 * @return number of entries of string
 */
uint16_t get_qdcount(const char *node);


/**
 * Helper function, make question section of the dns
 *
 * @param dns The allocated buffer to store question section
 * @param node The node to request
 *
 * @return the length of the question
 */
int make_question(char *dns, const char *node);

/**
 * Helper function, make answer section of the dns
 *
 * @param dns The allocated buffer to store answer section
 *
 * @return the length of the answer
 */
int make_answer(char *dns, uint16_t RLENGTH, uint32_t RDATA);


/**
 * Print the dns
 *
 * @param dns The struct dns_t which contains all fields of query
 *
 * @return 0
 */
int print_dns(struct dns_t *dns);

/**
 * Parse dns string-either query or reply, and fill all fields into struct dns_t
 *
 * @param dns The query/reply string
 *
 * @return the pointer to the filled struct dns_t
 */
struct dns_t *parse_dns(char *dns);

/**
 * Make dns reply packet
 *
 * @param ip The ip which should be contained in the reply
 * @param query The received query, whose question part should be contained in reply
 *
 * @return char * to the reply packet
 */
char *make_dns_reply(struct dns_t *query, uint32_t ip, int *reply_len);

/**
 * Helper function, make dns query packet based on the node param. It's allocated inside, and should be freed by caller
 *
 * @param node The hostname to resolve
 * @param query_len Return the length of the query. '\0' is everywhere inside the query, strlen does not help a lot.
 *
 * @return char * to the generated dns query string
 */
char *make_dns_query(const char *node, int *query_len);

/**
 * Helper function, parse reply from dns server.
 *
 * @param dns_reply The reply from dns server
 *
 * @return sockaddr based the parse of dns_reply
 */
struct sockaddr *parse_dns_reply(char *dns_reply);

/**
 * Recover node from QNAME
 */
char *recover_node(char *QNAME);

#endif
