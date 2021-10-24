# C++ FILE/PROCESS SERVER

This server application returns the result of process execution requests and file queries using the HTTP protocol. It implements low level networking and operating system concepts such as tcp endpoints, tcp input/output streams, detached threads, and process handling using os system calls.

> **NOTE:** This project was completed as part of a computer systems class. All of the work for the program was completed in ```main.cpp```. The remaining files were provided by the professor as helpful starter code.

## OVERVIEW

### General Program Flow

- The server is initialized by running the compiled executable and passing in a port number as a command line argument.
- The client spins up a web browser and hits this endpoint, specifying exec.html as the desired file path in the URL.
- The server parses the HTTP request and serves this html file, which presents a "terminal" for sending a process to the machine.
- Upon clicking **run command**, the text input is sent to the endpoint.
- The server executes this command and returns the result along with a table of statistics about the process's life cycle such as user time, system time, memory usage, and cpu usage at one second intervals.
- A chart displaying the cpu and memory usage over time for the process is also returned to the client. 

> **NOTE:** An example output is shown in ```example_output.html```

### Alternative Usage

- Requests for any raw file in the server's subdirectories can be retrieved by specifying the file path in the URL.
- Clients can also send raw process execution requests to the server by including ```/cgi-bin/exec?cmd=``` followed by the process in the URL path.

## FILE STRUCTURE

```ChildProcess.cpp```

- Contains the implementation of the ChildProcess class. This includes methods for splitting a string of commands, forking a new process, waiting on an exit code, and redirecting the output of a child process using a pipe.

```ChildProcess.h```

- Definition of the ChildProcess class.

```HTMLFragments.h```

- Useful chunks of HTML that are commonly sent to clients upon a process execution request.

```HTTPFile.h```

- A class definition for sending file contents to an output stream.

```HTTPFile.cpp```

- Contains an implementation of the http::file class for parsing a file's content type and overloading the stream insertion operator.

```mystyle.css```

- Simple style sheet for touching up the HTML output.

```draw_chart.js```

- Javascript function that draws a chart of CPU and memory Usage for the process overt time.

```exec.html```

- A nice landing page that lets clients easily send process requests.

```example_output.html```

- Contains the sample output page for an example process execution request.

```main.cpp```

- This main file contains all of the program's core logic for serving clients. All requests for files and processes are handled here using the classes and functions described above.
