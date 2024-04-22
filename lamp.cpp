#include<bits/stdc++.h>
#include <ctime>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <mutex>
#include <condition_variable>
#include <iomanip>  
#include <thread> 
#include <chrono> 
using namespace std;

// Define Clock class for Lamport logical clocks
class Clock {
public:
    Clock() : time(0) {}

    void increment() {
        ++time;
    }

    void update(int receivedTime) {
        time = std::max(time, receivedTime) + 1;
    }

    int getTime() const {
        return time;
    }

private:
    int time;
};

// Define Message class
class Message {
public:
    enum class Tag { CRITICAL_ENTER, RESOURCE_RELEASE, REQUEST, ACK, RELEASE, DONE, FINISH };

    Message(Tag t, Clock c, int p) : tag(t), clock(c), pid(p) {}

    Tag tag;
    Clock clock;
    int pid;
};

// Server function to handle client connections
// void server(int port) {
//     // Create a socket for the server
//     int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
//     if (serverSocket == -1) {
//         perror("Error in socket creation");
//         exit(EXIT_FAILURE);
//     }

//     // Set up the server address structure
//     struct sockaddr_in serverAddr;
//     serverAddr.sin_family = AF_INET;
//     serverAddr.sin_port = htons(port);
//     serverAddr.sin_addr.s_addr = INADDR_ANY;

//     // Bind the socket to the server address
//     if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
//         perror("Error in binding");
//         exit(EXIT_FAILURE);
//     }

//     // Listen for incoming connections
//     if (listen(serverSocket, 5) == -1) {
//         perror("Error in listening");
//         exit(EXIT_FAILURE);
//     }

//     std::cout << "Server listening on port " << port << std::endl;

//     // Accept incoming connections and handle them
//     while (true) {
//         // Accept connection
//         int clientSocket = accept(serverSocket, NULL, NULL);
//         if (clientSocket == -1) {
//             perror("Error in accepting connection");
//             exit(EXIT_FAILURE);
//         }

//         std::cout << "Connection accepted from client" << std::endl;

//         // Receive message from client
//         Message msg(Message::Tag::REQUEST, Clock(), 0); // Initialize with default clock and PID
//         int bytesRead = recv(clientSocket, &msg, sizeof(Message), 0);
//         if (bytesRead == -1) {
//            perror("Error in receiving message");
//            exit(EXIT_FAILURE);
//         } else if (bytesRead == 0) {
//            std::cerr << "Client disconnected" << std::endl;
//            close(clientSocket);
//            continue;
//         }


//         // Process the received message
//         std::cout << "Received message from client: ";
//         if (msg.tag == Message::Tag::REQUEST) {
//             std::cout << "REQUEST";
//         } else if (msg.tag == Message::Tag::ACK) {
//             std::cout << "ACK";
//         }
//         std::cout << " from process " << msg.pid << " with Lamport timestamp " << msg.clock.getTime() << std::endl;

//         // Send acknowledgment back to client
//         Message ack(Message::Tag::ACK, msg.clock, 0); // Assuming server's PID is 0
//         int bytesSent = send(clientSocket, &ack, sizeof(Message), 0);
//         if (bytesSent == -1) {
//             perror("Error in sending acknowledgment");
//             exit(EXIT_FAILURE);
//         }

//         close(clientSocket);
//     }

//     // Close the server socket
//     close(serverSocket);
// }

// Define Process class
class Process {
public:
   Process(int id, const std::string& ip, int port) : pid(id), ip(ip), port(port) {
        createSocket();
    }

    int getPid() const {
        return pid;
    }

    void createSocket() {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("Error in socket creation");
            exit(EXIT_FAILURE);
        }
        fcntl(sockfd, F_SETFL, O_NONBLOCK);
    }

    void sendRequest(const std::string& destIP, int destPort, std::queue<Message>& msgQueue, Clock& clock) {
    // Increment the clock before sending the request
    clock.increment();

    // Create a message with Lamport timestamp
    Message request(Message::Tag::REQUEST, clock, pid);

    // Connect to destination process
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error in socket creation");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in destAddr;
    memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(destPort);
    inet_pton(AF_INET, destIP.c_str(), &destAddr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&destAddr, sizeof(destAddr)) < 0) {
        perror("Error in connecting to destination process");
        exit(EXIT_FAILURE);
    }

    // Send message
    if (send(sockfd, &request, sizeof(Message), 0) == -1) {
        perror("Error in sending message");
        exit(EXIT_FAILURE);
    }

    close(sockfd);
}

void sendAck(const Message& msg) {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("Error in socket creation");
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in destAddr;
        memset(&destAddr, 0, sizeof(destAddr));
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(msg.pid); // Use the process ID as the port number for acknowledgment
        inet_pton(AF_INET, "127.0.0.1", &destAddr.sin_addr); // Example IP address, change as needed

        if (connect(sockfd, (struct sockaddr *)&destAddr, sizeof(destAddr)) < 0) {
            perror("Error in connecting to destination process");
            exit(EXIT_FAILURE);
        }

        // Send acknowledgment message
        if (send(sockfd, &msg, sizeof(Message), 0) == -1) {
            perror("Error in sending acknowledgment message");
            exit(EXIT_FAILURE);
        }

        close(sockfd);
    }


private:
    int pid;
    std::string ip;
    int port;
    int sockfd;
};

class MonitorProcess {
public:
    MonitorProcess(const std::set<Process*>& processes, int algo, int turn, int proc, int req, int check)
        : processes(processes), algo(algo), turn(turn), proc(proc), req(req), check(check), numMessagesReceived(0) {}

    void receive(const Message& msg) {
        // Update the local clock based on the received message's timestamp
        clock.update(msg.clock.getTime());

        if (msg.tag == Message::Tag::ACK) {
            // Handle acknowledgment message
            std::cout << "Received ACK from process " << msg.pid << " at time " << clock.getTime() << std::endl;
            // Additional processing for ACK messages
        } else if (msg.tag == Message::Tag::REQUEST) {
            // Handle request message
            std::cout << "Received request from process " << msg.pid << " at time " << clock.getTime() << std::endl;
            // Additional processing for request messages
        }

        // Increase the count of received messages
        numMessagesReceived++;
    }

    void sendAck(Process* destProcess, Clock& clock) {
        clock.increment();
        Message ack(Message::Tag::ACK, clock, destProcess->getPid());
        destProcess->sendAck(ack);
    }
    void wait() {
        // Wait for messages from all other processes
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this]() { return numMessagesReceived == processes.size(); });
    }

    void run() {
        wait();
        if (checkSafety()) {
            std::cout << "The system follows safety" << std::endl;
        } else {
            std::cout << "The system violates safety" << std::endl;
        }
    }

private:
    std::set<Process*> processes;
    int algo;
    int turn;
    int proc;
    int req;
    int check;
    int numMessagesReceived;
    Clock clock;
    std::mutex mutex;
    std::condition_variable cv;

    bool checkSafety() {
        // Implement safety checking logic here
        return true; // Placeholder return value
    }
};


int main() {

    // Set the port number for the server
    int port = 5000;

    // Start the server
    //server(port);
    // Initialize parameters
    int maxProcs = 2;   // Maximum number of processes
    int maxRequests = 2;  // Maximum number of requests
    int num = 3;  // Number of runs
    int d = 1;  // Denominator for varying requests and processes
    int a = 1;  // Number of runs for performance measurement

    const int totalAlgo = 3;

    std::cout << "Starting correctness verification loop..." << std::endl;

    // Correctness verification loop
    for (int i = 0; i < num; ++i) {
        std::cout << "Run " << i+1 << " of correctness verification." << std::endl;

        int nProcs = rand() % maxProcs + 1;
        int nRequests = rand() % maxRequests + 1;

        for (int j = 0; j < totalAlgo; ++j) {
            std::cout << "Algorithm " << j+1 << std::endl;

            // Instantiate processes and monitor process
            std::set<Process*> processes;
            std::queue<Message> msgQueue; 
            for (int k = 0; k < nProcs; ++k) {
                processes.insert(new Process(k, "192.168.1.100", 5000 + k));  // Example IP and port numbers
            }
            MonitorProcess monitorProcess(processes, j, i + 1, nProcs, nRequests, 1);

            std::vector<std::thread> threads;
            for (auto process : processes) 
            {
                threads.emplace_back([&monitorProcess, process, &msgQueue]() 
                {
                    // Simulate process behavior (e.g., sending requests)
                    Clock clock;
                    while (true) 
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Simulate some processing time
                        process->sendRequest("192.168.1.100", 5000, msgQueue, clock); // Pass the message queue to sendRequest
                            
                    }
                });
            }

            // Join threads
            for (auto& thread : threads) {
                thread.join();
            }

            // Run monitor process
            monitorProcess.run();

            // Clean up
            for (Process* p : processes) {
                delete p;
            }
        }
    }

    std::cout << "Starting performance measurement loops..." << std::endl;

     // Performance measurement for varying requests
    for (int j = 0; j < totalAlgo; ++j) {
        std::cout << "Performance measurement for varying requests using Algorithm " << j+1 << std::endl;

        int nProcs = maxProcs;
        int nRequests = maxRequests / d;
        int initialReq = maxRequests / d;

        while (nRequests <= maxRequests) {
            double totalTime = 0;
            for (int i = 0; i < a; ++i) {
                // Perform performance measurement
            }
            std::cout << "Requests: " << std::setw(2) << nRequests << " Total Time: " << totalTime << std::endl;
            nRequests += initialReq;
        }
    }

    // Performance measurement for varying processes
    for (int j = 0; j < totalAlgo; ++j) {
        std::cout << "Performance measurement for varying processes using Algorithm " << j+1 << std::endl;

        int nRequests = maxRequests;
        int nProcs = maxProcs / d;
        int initialProcs = maxProcs / d;

        while (nProcs <= maxProcs) {
            double totalTime = 0;
            for (int i = 0; i < a; ++i) {
                // Perform performance measurement
            }
            std::cout << "Processes: " << std::setw(2) << nProcs << " Total Time: " << totalTime << std::endl;
            nProcs += initialProcs;
        }
    }


    return 0;
}