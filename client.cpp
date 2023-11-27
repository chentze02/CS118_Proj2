#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include "utils.h"
using namespace std;

int main(int argc, char *argv[])
{
    int listen_sockfd, send_sockfd;
    sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    packet pkt;
    packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    unsigned short seq_num = 0;
    unsigned short ack_num = 0;
    char last = 0;
    char ack = 0;

    // read filename from command line argument
    if (argc != 2)
    {
        std::cout << "Usage: ./client <filename>\n";
        return 1;
    }
    char *filename = argv[1];

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0)
    {
        perror("Could not create listen socket");
        return 1;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0)
    {
        perror("Could not create send socket");
        return 1;
    }

    // Configure the server address structure to which we will send data
    memset(&server_addr_to, 0, sizeof(server_addr_to));
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Configure the client address structure
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, reinterpret_cast<sockaddr *>(&client_addr), sizeof(client_addr)) < 0)
    {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open file for reading
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // Set a fixed timeout value (in seconds)
    const int TIMEOUT_SEC = 2;

    // TODO: Read from file, and initiate reliable data transfer to the server
    while (!file.eof())
    {
        // Read a chunk of data from the file
        file.read(buffer, PAYLOAD_SIZE);
        std::cout << buffer << std::endl;
        unsigned int length = file.gcount(); // Get the actual length of the data read

        // Populate packet structure
        build_packet(&pkt, seq_num, ack_num, (file.eof() ? 1 : 0), ack, length, buffer);

        // Print the sending action
        std::cout << "I AM HERE" << std::endl;
        printSend(&pkt, 0); // Assuming 0 for not a resend
        std::cout << "I AM HERE 2" << std::endl;

        // Send the packet to the server through the proxy
        sendto(send_sockfd, &pkt, sizeof(pkt), 0,
               reinterpret_cast<sockaddr *>(&server_addr_to), sizeof(server_addr_to));

        // Record the time when the packet was sent
        auto send_time = std::chrono::steady_clock::now();

        // Set a flag to check for acknowledgment
        bool ack_received = false;

        // Continue trying until an acknowledgment is received or a timeout occurs
        while (!ack_received)
        {
            cout << "WAITING" << endl;
            // Wait for an acknowledgment from the server with a timeout
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - send_time).count();

            cout << "time elapsed" << elapsed_time << endl;
            cout << "timeout" << TIMEOUT_SEC << endl;
            if (elapsed_time >= TIMEOUT_SEC)
            {
                // Timeout occurred, retransmit the packet
                printSend(&pkt, 1); // Assuming 1 for a resend
                sendto(send_sockfd, &pkt, sizeof(pkt), 0,
                       reinterpret_cast<sockaddr *>(&server_addr_to), sizeof(server_addr_to));

                // Reset the timer
                send_time = std::chrono::steady_clock::now();
            }

            // Check for acknowledgment
            cout << "CHECK" << endl;
            recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0,
                     reinterpret_cast<sockaddr *>(&server_addr_from), &addr_size);

            cout << "CHECK 2" << endl;

            cout << ack_pkt.acknum << endl;
            cout << seq_num << endl;
            if (ack_pkt.acknum == seq_num)
            {
                // Acknowledgment received, exit the loop
                ack_received = true;
            }
            else
            {
                // Handle the case when an unexpected acknowledgment is received
                // This might happen due to retransmissions of the same packet
                printRecv(&ack_pkt); // Print the unexpected acknowledgment
            }
        }

        cout << "IM OUT" << endl;

        // Move to the next sequence number
        seq_num = (seq_num + 1) % MAX_SEQUENCE;

        // Clear the buffer for the next iteration
        memset(buffer, 0, PAYLOAD_SIZE);
    }

    file.close();
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
