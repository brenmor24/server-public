/** 
 * A custom web-server that can process HTTP GET requests and:
 *    1. Respond with contents of a given HTML file
 *    2. Run a specified program and return the output from the process.
 */

#include <sys/wait.h>
#include <boost/asio.hpp>
#include <string>
#include <thread>
#include <future>
#include "HTTPFile.h"
#include "ChildProcess.h"
#include "HTMLFragments.h"
#include "iostream"

// helpful namespaces
using namespace boost::asio;
using namespace boost::asio::ip;

// forward declaration for url_decode method defined below
std::string url_decode(std::string url);

// forward declaration for serveClient that is defined below
void serveClient(std::istream& is, std::ostream& os, bool);

// forward declaration for process info thread
void statInfo(int myPid, std::string& results, std::string& json);

/**
 * runs the program as a server that listens to incoming connections
 * 
 * @param port port number that the server should listen on
 */
void runServer(int port) {
    // setup a server socket to accept connections on the socket
    io_service service;
    // create end point
    tcp::endpoint myEndpoint(tcp::v4(), port);
    // create a socket that accepts connections
    tcp::acceptor server(service, myEndpoint);
    std::cout << "Server is listening on "
              << server.local_endpoint().port()
              << " & ready to process clients...\n";
    // process client connections one-by-one continuously
    using TcpStreamPtr = std::shared_ptr<tcp::iostream>;
    while (true) {
        // create shared object for maintaining socket connection
        TcpStreamPtr client = std::make_shared<tcp::iostream>();
        // wait for a client to connect    
        server.accept(*client->rdbuf());
        // process request from client on a separate background thread
        std::thread thr([client](){ serveClient(*client, *client, true); });
        thr.detach();
    }
}

/**
 * @param cmd the command to be executed
 *
 * @param os the output stream that the output should be written to
 * 
 * @param genChart if flag is true, graph data is also sent
 */
void sendCmdOutput(std::string& cmd, std::ostream& os, bool genChart) {
    // split the command into words for processing
    const StrVec args = ChildProcess::split(cmd);
    // create the child process
    ChildProcess cp;
    int myPid = cp.forkNexecIO(args);
    // get stats
    std::string statOutput = "";
    std::string json = "";
    std::thread statThread(statInfo, myPid, 
        std::ref(statOutput), std::ref(json));
    // get the outputs from the child process.
    std::istream& is = cp.getChildOutput();
    // send the output from child process to the client as chunked response
    os << http::DefaultHttpHeaders << "text/html\r\n\r\n";
    // send initial headers
    os << std::hex << htmlStart.size() << "\r\n" << htmlStart << "\r\n";
    // send each line as a spearate chunk
    for (std::string line; std::getline(is, line);) {
        line += "\n";  // add the newline that was not included by getline
        os << std::hex << line.size() << "\r\n" << line << "\r\n";
    }
    // wait for the process to finish
    int exitCode = cp.wait();
    const std::string line = "Exit code: " + std::to_string(exitCode) + "\n";
    os << std::hex << line.size() << "\r\n" << line << "\r\n";
    statThread.join();
    // send final chunk
    if (!genChart) { json = ""; }
    int chunkSize = htmlMid1.size() + statOutput.size() + htmlMid2.size()
        + htmlEnd.size() + json.size();
    os << std::hex << chunkSize << "\r\n" << htmlMid1 << statOutput 
        << htmlMid2 << json << htmlEnd << "\r\n";

    // send the trailing chunk
    os << "0\r\n\r\n";
}

void statInfo(int myPid, std::string& results, std::string& json) {
    int counter = 0;
    int exitCode = 0;
    float utime, stime;
    long vsize;
    std::string x;
    while (waitpid(myPid, &exitCode, WNOHANG) == 0) {
        // sleep for 1 second.
        sleep(1);
        // record current statistics about the process
        counter++;
        // read stats from file
        std::string path = "/proc/" + std::to_string(myPid) + "/stat";
        std::ifstream is(path);
        is >> x >> x >> x >> x >> x >> x >> x >> x >> x >> x >> x >> x >>
            x >> utime >> stime >> x >> x >> x >> x >> x >> x >> x >> vsize;
        // format output
        results += "       <tr><td>" + std::to_string(counter) + "</td><td>";
        int num1 = static_cast<int>(std::round(utime/sysconf(_SC_CLK_TCK)));
        results += std::to_string(num1) + "</td><td>";
        int num2 = static_cast<int>(std::round(stime/sysconf(_SC_CLK_TCK)));
        results += std::to_string(num2) + "</td><td>";
        results += std::to_string(vsize/1000000) + "</td></tr>\n";
        json += ",\n          [" + std::to_string(counter) + ", " + 
            std::to_string(num1 + num2) + ", " + 
            std::to_string(vsize/1000000) + "]";
    }
}

/**
 * process http request and return response to client
 * 
 * @param is input stream to read data from client
 *
 * @param os output stream to send data to client
 * 
 * @param genChart if flag is true, graph data is also sent
 */
void serveClient(std::istream& is, std::ostream& os, bool genChart) {
    // read headers from client and print them
    std::string line, path;
    // read the "GET" word and then the path
    is >> line >> path;
    // skip/ignore the HTTP request & headers
    while (std::getline(is, line) && (line != "\r")) {}
    // check and handle the request
    if (path.find("/cgi-bin/exec?cmd=") == 0) {
        // remove the "/cgi-bin/exec?cmd=" substring
        auto cmd = url_decode(path.substr(18));
        sendCmdOutput(cmd, os, genChart);
    } else if (!path.empty()) {
        // assume the client is asking for a file in this case
        path = "." + path;  // make path with-respect-to the pwd
        // use http::file to send response to client
        os << http::file(path);
    }
}

/** 
 * method for decoding HTML/URL strings.
 *
 * \param[in] str the string to be decoded
 *
 * \return the decoded string
 */
std::string url_decode(std::string str) {
    size_t pos = 0;
    while ((pos = str.find_first_of("%+", pos)) != std::string::npos) {
        switch (str.at(pos)) {
            case '+': str.replace(pos, 1, " ");
            break;
            case '%': {
                std::string hex = str.substr(pos + 1, 2);
                char ascii = std::stoi(hex, nullptr, 16);
                str.replace(pos, 3, 1, ascii);
            }
        }
        pos++;
    }
    return str;
}

/**
 * \param[in] argc the number of command-line arguments
 *
 * \param[in] argv the command line arguments. if its just an number,
 * it's assumed to be a port number. otherwise its a file
 */
int main(int argc, char *argv[]) {
    // check if first command line argument is port or file
    const std::string True = "true";
    std::string arg = (argc > 1 ? argv[1] : "0");
    const bool genChart = (argc > 2 ? (argv[2] == True) : false);
    // Check and use a given input data file for testing.
    if (arg.find_first_not_of("1234567890") == std::string::npos) {
        // only contains digits, so must be port number
        runServer(std::stoi(arg));
    } else {
        std::ifstream getReq(arg);
        if (!getReq.good()) {
            std::cerr << "Unable to open " << arg << ". Aborting.\n";
            return 2;
        }
        serveClient(getReq, std::cout, genChart);
    }
    return 0;
}
