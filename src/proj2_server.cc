// Copyright 2026 JC Austin
// For a majority of this I used python then converted it into C++

#include<iostream>
#include<vector>
#include<cstdint>
#include<mutex>
#include<semaphore.h>
#include<unistd.h>
#include<cstring>

// Libraries for Server
#include<client-server-communication/lib/domain_socket.h>
#include<client-server-communication/lib/file_reader.h>
#include<client-server-communication/lib/sha_solver.h>
#include<client-server-communication/lib/thread_log.h>
#include<client-server-communication/lib/timings.h>
#include<client-server-communication/include/proj2_server.h>


struct File {

    std::string f_name;
    uint32_t f_rows;
};

void ParseMessage(const std::string& msg, std::string* client_adder, std::vector<std::string>* file_paths, std::vector<uint32_t>* max_rows){

    const char* c_ptr = msg.data();


    std::size_t mark = 0;
    std::uint32_t path_len;

    std::memcpy(&path_len, c_ptr + mark, 4);
    mark += 4;
    std::cout << mark << std::endl;

    *client_adder = std::string(c_ptr + mark, path_len); // A pointer that takes a string and writes over the already existing data there

    std::cout << "Client Address: " << *client_adder << std::endl;
    mark += path_len;

    std::uint32_t file_no;
    std::memcpy(&file_no, c_ptr + mark, 4);
    mark += 4;

    for (std::uint32_t i = 0; i < file_no; ++i) {

        // Read path of length
        std::cout << mark << "\n";
        std::uint32_t f_path_len;
        std::memcpy(&f_path_len, c_ptr + mark, 4);
        std::cout << f_path_len << "\n";
        mark += 4;

        // Read path from msg
        file_paths -> push_back(std::string(c_ptr + mark, f_path_len));
        mark += f_path_len;
        std::cout << "Added path length to mark " << mark << "\n";


        std::uint32_t row_count;
        std::memcpy(&row_count, c_ptr + mark, 4);
        mark += 4;
        max_rows -> push_back(row_count);


    }

}


void* StartRoutine(void* arg);


void Proj2Server::Run() {


    this->Init();
    
    // Creates our worker threads
    // Creates the solver number of threads

    pthread_t thread_id;

    for (uint32_t i = 0; i < solvers_; ++i) {

        if(pthread_create(&thread_id, nullptr, StartRoutine, this)) {

            std::cerr << "Cannot create thread " << thread_id << std::endl;

        }

        else {

            pthread_detach(thread_id);

        }


    }

    for (;;) {

	std::cout << "Your server is alive.. Waiting on a client!" << std::endl;

        std::string dummy;

        std::string msg = RecvFrom(&dummy, 65000);

        // Busy waiting crit section
        // Shuts out other threads from resource
        queue_mutex_.lock();

            // Creates are we there yet message
            msg_queue_.push(msg);

            // Opens it back up for resources
        queue_mutex_.unlock();

        // Alerts or says we are here
        sem_post(&msg_queue_sem_);
    }

}



// Routine for my worker threads
void* StartRoutine(void* arg) {

    // Tells thread who the server is
    Proj2Server* server = static_cast<Proj2Server*>(arg);

    // While Loop to keep running
    while(true) {
    // Crit section for my threads
    // Need to solve deadlock now
        sem_wait(&(server -> msg_queue_sem_));

	std::string msg; // Creates empty string to fill up with data
	bool found_msg = false; // Determines whether message is empty or not

        server -> queue_mutex_.lock();

	// If we don't have a message, puts it in front of our queue
	if(!server -> msg_queue_.empty()) {

            msg = server -> msg_queue_.front(); // Changed so that it can fit FIFO for queue. Definitely had to look this up to understand
            server -> msg_queue_.pop();
	    found_msg = true;

	}
        server -> queue_mutex_.unlock();

	// If we have a message we move forward
	if(!found_msg) {

	    continue;

	}

        std::string client_adder;
        std::vector<std::string> file_paths; // Creates a list of file paths
        std::vector<uint32_t> max_rows;


        ParseMessage(msg, &client_adder, &file_paths, &max_rows);


        uint32_t k = 0; // k = max amount value inside of max rows

        for (uint32_t row_count : max_rows) {
            if (row_count > k) {
                k = row_count;
            }
        }

        uint32_t n = file_paths.size(); // n = number of files

        // points towards our getters in header file
        uint32_t max_solve = server -> getNumSolvers(); 
        uint32_t max_read = server -> getNumReaders();


        uint32_t checking_k = max_solve; // Chooses the min between amount of solvers wanted and the total amount of solvers
        uint32_t checking_n = std::min(n, max_read); // Chooses the min between the number of files and total amount of reader spots


        // Inner list contains amount of hashes within a file
        // Outer list contains the indexes of files
        std::vector<std::vector<proj2::ReaderHandle::HashType>> file_hashes_out(n); // List of a list that stores the hashes the different files

        proj2::SolverHandle solvers = proj2::ShaSolvers::Checkout(checking_k); // Checks out max amount(k) and our total number of solvers
        proj2::ReaderHandle readers = proj2::FileReaders::Checkout(checking_n, &solvers); // Checks out number of file and total number of reader slots

        readers.Process(file_paths, max_rows, &file_hashes_out);


        proj2::FileReaders::Checkin(std::move(readers));
        proj2::ShaSolvers::Checkin(std::move(solvers));



	size_t num_of_bytes = 0; // total number of bytes

	for (uint32_t i = 0; i < n; ++i) {

	    std::vector<proj2::ReaderHandle::HashType>& current_hashes = file_hashes_out[i]; // Creates a list of hashes within a current file and gets the index of file
	    num_of_bytes += current_hashes.size() * sizeof(proj2::ReaderHandle::HashType); // Multiplies the number of hashes by the bytes per hash and adds it to our total number of bytes

	}

	std::vector<uint8_t> main_packet(num_of_bytes); // Creates a list of bites based our total number of bytes we calculated

	size_t tracker = 0;

	for (uint32_t i = 0; i < n; ++i) {

	    std::vector<proj2::ReaderHandle::HashType>& current_hashes = file_hashes_out[i]; // Same list from previous for loop
	    size_t file_bytes = current_hashes.size() * sizeof(proj2::ReaderHandle::HashType); // Multiplies the number of hashes by the bytes per hash


	    std::memcpy(main_packet.data() + tracker, current_hashes.data(), file_bytes);

	    tracker += file_bytes;

	}


	// Switched my sendTo to reply within my Domain_socket.h file
        try {

	    proj2::UnixDomainStreamClient reply(client_adder);
	    reply.Init();
	    reply.Write(main_packet.data(), num_of_bytes);

	}
	
	// Failure statement that catches my exceptions and siaply error message
	catch (const std::exception& e){
	
	    	std::cerr << "Reply not found!" << std::endl;

	}
    }

    return nullptr;

}

int main(int argc, char* argv[]) {

    // Error message for incorrect amount/number of arguments
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <s_path> <readers> <solvers>" << std::endl;
        return 1;
    }

    // Used for long unassigned integers (big or small)
    // Had to look up what this statement is and what it does 

    uint32_t num_of_readers = std::stoul(argv[2]); // Used for large unsigned int readers
    uint32_t num_of_solvers = std::stoul(argv[3]); // Used for large unsigned int solvers

    // Used to initialize the libraries used in this project
    proj2::FileReaders::Init(num_of_readers);
    proj2::ShaSolvers::Init(num_of_solvers);

    Proj2Server server(argv[1], num_of_readers, num_of_solvers);

    server.Run();

    return 0;

}
