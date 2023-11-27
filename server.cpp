#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include "utils.h"
using namespace std;

int main()
{
    int listen_sockfd, send_sockfd;
    sockaddr_in server_addr, client_addr_from;
    socklen_t addr_size = sizeof(client_addr_from);
    int expected_seq_num = 0;
    int recv_len;
    packet buffer;
    bool file_received = false;

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0)
    {
        perror("Could not create listen socket");
        return 1;
    }

    // Configure the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the server address
    if (bind(listen_sockfd, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open the target file for writing (always write to output.txt)
    std::ofstream file("output.txt", std::ios::binary);

    // Loop to continuously receive segments until the entire file is reconstructed
    while (!file_received)
    {
        // Receive a packet from the proxy
        recv_len = recvfrom(listen_sockfd, &buffer, sizeof(buffer), 0,
                            reinterpret_cast<sockaddr *>(&client_addr_from), &addr_size);
        cout << recv_len << endl;

        // Perform error handling for the received packet
        if (recv_len < 0)
        {
            perror("Error receiving packet");
            break;
        }

        // Check if the received packet is the expected one
        if (buffer.seqnum == expected_seq_num)
        {
            cout << "Received packet with expected sequence number" << endl;
            // Write the payload to the file
            file.write(buffer.payload, buffer.length);

            // Print the received action
            printRecv(&buffer);

            // Send acknowledgment to the proxy
            build_packet(&buffer, 0, expected_seq_num, 0, 1, 0, ""); // ACK packet
            sendto(listen_sockfd, &buffer, sizeof(buffer), 0,
                   reinterpret_cast<sockaddr *>(&client_addr_from), sizeof(addr_size));
            cout << "Sent ACK" << endl;

            // Move to the next expected sequence number
            expected_seq_num = (expected_seq_num + 1) % MAX_SEQUENCE;
            cout << "Expected sequence number: " << expected_seq_num << endl;

            // Check if the last packet is received
            if (buffer.last)
                file_received = true;
        }
        else
        {
            // Print the received action for a duplicate packet
            printRecv(&buffer);

            // Resend acknowledgment for the previous packet
            build_packet(&buffer, 0, (expected_seq_num - 1 + MAX_SEQUENCE) % MAX_SEQUENCE, 0, 1, 0, ""); // ACK packet
            sendto(listen_sockfd, &buffer, sizeof(buffer), 0,
                   reinterpret_cast<sockaddr *>(&client_addr_from), sizeof(addr_size));
        }
    }

    // Close the file after receiving the entire file
    file.close();

    // Close sockets
    close(listen_sockfd);

    return 0;
}
