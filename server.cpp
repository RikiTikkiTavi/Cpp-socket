/* The port number is passed as an argument */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <map>
#include <iostream>

using namespace std;

void error(const char *msg) {
    perror(msg);
    exit(1);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"

// argc -> Argument count = the number of strings pointed to by argv
// argv -> Argument vector

char *getAvailableUsers (map<string, int> *clientsMap){
    string availableUsers;
    for(pair <string, int> user : (*clientsMap)){
        string userName = (user.first);
        availableUsers += userName + ", ";
    }
    availableUsers = "[ " + availableUsers + "]";
    char *availableUsersChar = new char[availableUsers.length() + 1];
    strcpy(availableUsersChar, availableUsers.c_str());

    return availableUsersChar;
}

void handleClient(int newsockfd, map<string, int> *clientsMap, map<string, thread *> *clientThreadsMap,
                  const string myName) {
    char buffer[256];
    char confString[] = "[D_S]";
    char nameSetConfString[] = "[N_S]";

    // Send back available users
    // TODO: Transform clientsMap object to JSON
    char *availableUsersChar = getAvailableUsers(clientsMap);
    // Transform availableUsers into char array and send back
    send(newsockfd, availableUsersChar, strlen(availableUsersChar), 0);

    // Set sendTo and send back confirmation of success
    bzero(buffer, 256);
    int n = read(newsockfd, buffer, 255);
    if (n < 0) error("ERROR reading from socket");
    string name = string(buffer);
    char closedConnectionString[] = "[U2_C_C]";
    int sendToSockfd = (*clientsMap)[name];
    send(newsockfd, nameSetConfString, strlen(nameSetConfString), 0);

    // Receive messages and send them to sendTo
    while (true) {
        bzero(buffer, 256);
        int n = read(newsockfd, buffer, 255);
        if (n < 0) error("ERROR reading from socket");
        if (n == 0) {
            //Send message to user2 about closed connection
            send(sendToSockfd, closedConnectionString, strlen(closedConnectionString), 0);
            break;
        }
        if(strncmp(buffer, "[U2_C_C]", 7)!=0){
            char * availableUsersChar = getAvailableUsers(clientsMap);
            send(newsockfd, availableUsersChar, strlen(availableUsersChar), 0);
        }
        // Close connection if message is [C_C]
        try {
            send(sendToSockfd, buffer, strlen(buffer), 0);
        } catch (exception &e) {
            cout << "Error sending confString \n";
            cout << e.what();
            break;
        }

        try {
            send(newsockfd, confString, strlen(confString), 0);
        } catch (exception &e) {
            cout << "Error sending message from: " << myName << "to: " << name;
            cout << e.what();
            break;
        }
    }
    close(newsockfd);
    map<string, thread *> tempClientThreadsMap = *clientThreadsMap;
}
void prepareServer(int argc, char *argv[]) {
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[256];
    // sockaddr_in -> Structure describing an Internet socket address.
    struct sockaddr_in serv_addr{}, cli_addr{};
    int n;
    if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }
    // create a socket
    // socket(int domain, int type, int protocol)
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    // clear address structure
    bzero(&serv_addr, sizeof(serv_addr));

    /*
    string
    Си-строка для выполнения преобразования.
    endptr
    Ссылка на объект типа char*, значение которой содержит адрес следующего символа в строке string, сразу после предыдущего найденного числа. Если этот параметр не используется, он должен быть нулевым указателем.
    basis
    Основание системы исчисления.
    */
    portno = strtol(argv[1], nullptr, 10);

    /* setup the host_addr structure for use in bind call */
    // server byte order
    // specifies the address family, usually the constant AF_INET
    serv_addr.sin_family = AF_INET;

    // automatically be filled with current host's IP address
    // holds the IP address returned by inet_addr() to be used in the socket connection.
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // convert short integer value for port must be converted into network byte order
    serv_addr.sin_port = htons(portno);

    // bind(int fd, struct sockaddr *local_addr, socklen_t addr_length)
    // bind() passes file descriptor, the address structure,
    // and the length of the address structure
    // This bind() call will bind  the socket to the current IP address on port, portno
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) error("ERROR on binding");

    // This listen() call tells the socket to listen to the incoming connections.
    // The listen() function places all incoming connection into a backlog queue
    // until accept() call accepts the connection.
    // Here, we set the maximum size for the backlog queue to 5.
    listen(sockfd, 5);

    // The accept() call actually accepts an incoming connection
    clilen = sizeof(cli_addr);

    // This accept() function will write the connecting client's address info
    // into the address structure and the size of that structure is clilen.
    // The accept() returns a new socket file descriptor for the accepted connection.
    // So, the original socket file descriptor can continue to be used
    // for accepting new connections while the new socket file descriptor is used for
    // communicating with the connected client.

    // Server will accept 2 clients
    /*static int clients[2];
    for (int &client : clients) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) error("ERROR on accept");
        printf("server: got connection from %s port %d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
        client = newsockfd;
    }*/

    map<string, int> clientsMap;
    map<string, thread *> clientThreadsMap;

    while (true) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) error("ERROR on accept");
        printf("server: got connection from %s port %d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
        bzero(buffer, 256);
        int n = read(newsockfd, buffer, 255);
        if (n < 0) error("ERROR reading from socket");
        string name = string(buffer);
        clientsMap.insert(pair<string, int>(name, newsockfd));
        // Client Accepted
        send(newsockfd, "[C_A]", strlen("[C_A]"), 0);
        cout << "server: [C_A] Client name: " << name;
        //New thread for working with client
        clientThreadsMap[name] = new thread(handleClient, newsockfd, &clientsMap, &clientThreadsMap, name);
    }
};

int main(int argc, char *argv[]) {

    prepareServer(argc, argv);

    return 0;
}

#pragma clang diagnostic pop